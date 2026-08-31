// CORE-005 RunContext 实现
#include "astrocs/core/context.h"

namespace astrocs::core {

void RunContext::log(LogLevel level, const std::string& component,
                     const std::string& message) {
  const char* lv = level == LogLevel::DEBUG ? "DEBUG"
                  : level == LogLevel::INFO ? "INFO"
                  : level == LogLevel::WARN ? "WARN" : "ERROR";
  log_entries_.push_back(std::string("[") + lv + "][" + component + "] " + message);
}

Result<void> RunContext::store_artifact(DataArtifactDescriptor desc) {
  std::string err;
  if (!desc.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "store_artifact: " + err));
  }
  if (artifacts_.count(desc.id.id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "store_artifact: duplicate id " + desc.id.id));
  }
  artifacts_[desc.id.id] = std::move(desc);
  return Result<void>::success();
}

const DataArtifactDescriptor* RunContext::get_artifact(const std::string& id) const {
  auto it = artifacts_.find(id);
  return it == artifacts_.end() ? nullptr : &it->second;
}

std::vector<std::string> RunContext::artifact_ids() const {
  std::vector<std::string> out;
  out.reserve(artifacts_.size());
  for (const auto& [k, v] : artifacts_) out.push_back(k);
  return out;
}

// ── RT-001/RT-002: ThreadBudget 原子租约 ──
ThreadLease ThreadBudget::acquire(uint32_t min, uint32_t max,
                                  AcquirePolicy policy) noexcept {
  if (min == 0) min = 1;
  if (max == 0) max = budget_;

  auto try_take = [&]() -> ThreadLease {
    uint32_t cur = available_.load(std::memory_order_relaxed);
    for (;;) {
      if (cur < min) {
        if (policy == AcquirePolicy::BEST_EFFORT && cur > 0) {
          const uint32_t take = (max < cur) ? max : cur;
          if (available_.compare_exchange_weak(cur, cur - take,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
            return _make_lease(take);
          }
          continue;
        }
        return ThreadLease();  // 空租约（不满足最小值）
      }
      const uint32_t take = (max < cur) ? max : cur;
      if (available_.compare_exchange_weak(cur, cur - take,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
        return _make_lease(take);
      }
    }
  };

  if (policy != AcquirePolicy::BLOCK) {
    return try_take();
  }
  std::unique_lock<std::mutex> lock(cv_mutex_);
  for (;;) {
    ThreadLease lease = try_take();
    if (lease.acquired()) return lease;
    cv_.wait(lock, [&] { return available_.load(std::memory_order_relaxed) >= min; });
  }
}

ThreadLease ThreadBudget::_make_lease(uint32_t got) noexcept {
  return ThreadLease(got, [this, got]() noexcept {
    available_.fetch_add(got, std::memory_order_acq_rel);
    cv_.notify_all();
  });
}

Result<std::shared_ptr<ThreadBudget>> create_thread_budget(uint32_t budget) noexcept {
  if (budget == 0) {
    return Result<std::shared_ptr<ThreadBudget>>::fail(
        Error(ErrorDomain::RESOURCE, "create_thread_budget: budget must be > 0"));
  }
  auto b = std::make_shared<ThreadBudget>(budget);
  b->reset_available();
  return Result<std::shared_ptr<ThreadBudget>>::ok(std::move(b));
}

}  // namespace astrocs::core
