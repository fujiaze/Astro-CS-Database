// CORE-005 RunContext 实现（RT-003 线程安全）
#include "astrocs/core/context.h"

namespace astrocs::core {

void RunContext::log(LogLevel level, const std::string& component,
                     const std::string& message) {
  const char* lv = level == LogLevel::DEBUG ? "DEBUG"
                  : level == LogLevel::INFO ? "INFO"
                  : level == LogLevel::WARN ? "WARN" : "ERROR";
  std::lock_guard<std::mutex> lock(mu_);
  log_entries_.push_back(std::string("[") + lv + "][" + component + "] " + message);
}

std::vector<std::string> RunContext::log_entries() const {
  std::lock_guard<std::mutex> lock(mu_);
  return log_entries_;
}

void RunContext::add_metric(const std::string& name, uint64_t value) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[name] = value;
}

void RunContext::record_tick(const Metrics& m) {
  std::lock_guard<std::mutex> lock(mu_);
  ticks_.push_back(m);
}

std::map<std::string, uint64_t> RunContext::metrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return metrics_;
}

std::vector<Metrics> RunContext::ticks() const {
  std::lock_guard<std::mutex> lock(mu_);
  return ticks_;
}

Result<void> RunContext::store_artifact(DataArtifactDescriptor desc) {
  std::string err;
  if (!desc.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "store_artifact: " + err));
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (artifacts_.count(desc.id.id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "store_artifact: duplicate id " + desc.id.id));
  }
  artifacts_[desc.id.id] = std::move(desc);
  return Result<void>::success();
}

bool RunContext::get_artifact(const std::string& id,
                              DataArtifactDescriptor* out) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = artifacts_.find(id);
  if (it == artifacts_.end()) return false;
  if (out) *out = it->second;
  return true;
}

std::vector<std::string> RunContext::artifact_ids() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  out.reserve(artifacts_.size());
  for (const auto& [k, v] : artifacts_) out.push_back(k);
  return out;
}

void RunContext::mark_checkpoint(const std::string& node_id) {
  std::lock_guard<std::mutex> lock(mu_);
  checkpoints_.push_back(node_id);
}

std::vector<std::string> RunContext::checkpoints() const {
  std::lock_guard<std::mutex> lock(mu_);
  return checkpoints_;
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
