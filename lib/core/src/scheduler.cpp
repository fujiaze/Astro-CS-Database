// CORE-006 / RT-006 统一 DAG 调度器实现:
// 有界 worker 池 + 依赖就绪 + 取消/失败传播 + 内存回压/预留 + 带 node ID 状态输出。
#include "astrocs/core/scheduler.h"

#include <algorithm>
#include <set>
#include <mutex>

namespace astrocs::core {

Scheduler::Scheduler(uint32_t available_cpu, uint32_t budget,
                     uint64_t memory_limit_bytes)
    : available_cpu_(available_cpu),
      budget_(std::max<uint32_t>(1, std::min(budget, std::max<uint32_t>(1, available_cpu)))),
      memory_limit_bytes_(memory_limit_bytes) {}

Scheduler::~Scheduler() = default;

void Scheduler::add_node(NodeSpec spec) {
  const std::string id = spec.node_id;  // 先拷贝，避免 move 后使用空 id
  nodes_[id] = std::move(spec);
  status_[id] = NodeStatus::PLANNED;
}

Result<void> Scheduler::build() {
  if (nodes_.empty()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "scheduler: no nodes"));
  }
  // 环检测
  std::map<std::string, int> indegree;
  std::map<std::string, std::vector<std::string>> rev;
  for (const auto& [id, spec] : nodes_) {
    indegree[id] = 0;
    for (const auto& d : spec.deps) {
      if (!nodes_.count(d)) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "scheduler: node " + id + " unknown dep " + d));
      }
      rev[d].push_back(id);
      indegree[id]++;
    }
  }
  std::queue<std::string> ready;
  for (const auto& [id, deg] : indegree) if (deg == 0) ready.push(id);
  size_t visited = 0;
  while (!ready.empty()) {
    auto id = ready.front(); ready.pop();
    ++visited;
    for (const auto& nxt : rev[id]) {
      if (--indegree[nxt] == 0) ready.push(nxt);
    }
  }
  if (visited != nodes_.size()) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "scheduler: dependency cycle"));
  }
  built_ = true;
  return Result<void>::success();
}

Result<void> Scheduler::run(
    RunContext& ctx, std::vector<std::pair<std::string, NodeStatus>>* statuses) {
  if (!built_) {
    auto b = build();
    if (b.failed()) return b;
  }
  // RT-003: worker 启动前单线程设置 budget
  ctx.set_thread_budget(budget_);
  std::mutex mtx;
  std::condition_variable cv;
  std::map<std::string, NodeStatus> status = status_;
  std::map<std::string, int> remaining_deps;
  std::map<std::string, std::vector<std::string>> rev;
  std::queue<std::string> ready;
  for (const auto& [id, spec] : nodes_) {
    remaining_deps[id] = static_cast<int>(spec.deps.size());
    for (const auto& d : spec.deps) rev[d].push_back(id);
    if (spec.deps.empty()) ready.push(id);
  }
  std::atomic<uint32_t> active{0};
  std::atomic<bool> cancelled_run{false};
  std::atomic<uint64_t> mem_used{0};
  std::string fail_node;
  std::string fail_msg;
  std::set<std::string> blocked;  // 失败节点的传递依赖（SKIPPED）

  auto compute_blocked = [&](const std::string& root) {
    std::set<std::string> out;
    std::vector<std::string> stack{root};
    while (!stack.empty()) {
      auto id = stack.back(); stack.pop_back();
      for (const auto& nxt : rev[id]) {
        if (!out.count(nxt)) { out.insert(nxt); stack.push_back(nxt); }
      }
    }
    blocked = std::move(out);
  };

  auto worker = [&]() {
    while (true) {
      std::string node_id;
      uint64_t node_mem = 0;
      {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] {
          return cancelled_run.load() || !ready.empty() || active.load() == 0;
        });
        if (cancelled_run.load()) {
          while (!ready.empty()) {
            auto id = ready.front(); ready.pop();
            status[id] = NodeStatus::CANCELLED;
          }
          return;
        }
        if (ready.empty()) {
          if (active.load() == 0) return;  // 全部完成
          continue;
        }
        // 内存回压: 若启用限制且 ready 队首节点超限，等待
        if (memory_limit_bytes_ > 0) {
          const std::string candidate = ready.front();
          auto cit = nodes_.find(candidate);
          uint64_t need = cit != nodes_.end() ? cit->second.estimated_memory_bytes : 0;
          if (mem_used.load() + need > memory_limit_bytes_) {
            if (active.load() > 0) { cv.notify_all(); continue; }
          }
        }
        node_id = ready.front(); ready.pop();
        auto nit = nodes_.find(node_id);
        if (nit != nodes_.end()) node_mem = nit->second.estimated_memory_bytes;
        if (blocked.count(node_id)) {
          // 失败节点的传递依赖 → SKIPPED（不执行），继续推进依赖计数
          status[node_id] = NodeStatus::SKIPPED;
          for (const auto& nxt : rev[node_id]) {
            if (--remaining_deps[nxt] == 0) ready.push(nxt);
          }
          cv.notify_all();
          continue;
        }
        mem_used.fetch_add(node_mem);
        status[node_id] = NodeStatus::RUNNING;
        ++active;
      }
      // 执行
      bool node_ok = true;
      {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end() && it->second.fn) {
          auto r = it->second.fn(node_id, ctx);
          node_ok = r.ok();
          if (r.failed() && fail_node.empty()) {
            std::lock_guard<std::mutex> lk(mtx);
            fail_node = node_id;
            fail_msg = r.error().message();
            compute_blocked(node_id);
          }
        }
        if ((ctx.cancelled() || cancel_.load()) && !cancelled_run.load()) {
          node_ok = false;
          std::lock_guard<std::mutex> lk(mtx);
          cancelled_run.store(true);
          fail_node = node_id;
          fail_msg = "cancelled";
        }
      }
      {
        std::lock_guard<std::mutex> lk(mtx);
        --active;
        mem_used.fetch_sub(node_mem);
        status[node_id] = node_ok ? NodeStatus::COMPLETED : NodeStatus::FAILED;
        if (fail_node.empty() && !cancelled_run.load()) {
          for (const auto& nxt : rev[node_id]) {
            if (--remaining_deps[nxt] == 0) ready.push(nxt);
          }
        }
        cv.notify_all();
      }
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(budget_);
  for (uint32_t i = 0; i < budget_; ++i) pool.emplace_back(worker);
  for (auto& t : pool) t.join();

  // 未运行节点: 取消 → CANCELLED；失败且未标 → SKIPPED（若其依赖链在失败下游）
  for (auto& [id, st] : status) {
    if (st == NodeStatus::PLANNED || st == NodeStatus::QUEUED) {
      st = cancelled_run.load() ? NodeStatus::CANCELLED : NodeStatus::SKIPPED;
    }
  }
  if (statuses) {
    statuses->clear();
    statuses->reserve(status.size());
    for (const auto& [id, st] : status) statuses->emplace_back(id, st);
  }
  if (cancelled_run.load()) {
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "node " + fail_node + " cancelled: " + fail_msg));
  }
  if (!fail_node.empty()) {
    return Result<void>::fail(Error(ErrorDomain::BACKEND,
        "node " + fail_node + " failed: " + fail_msg));
  }
  return Result<void>::success();
}

}  // namespace astrocs::core
