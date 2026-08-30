// CORE-006 统一 DAG 调度器实现: 有界 worker 池 + 依赖就绪 + 取消/失败传播
#include "astrocs/core/scheduler.h"

#include <algorithm>

namespace astrocs::core {

Scheduler::Scheduler(uint32_t available_cpu, uint32_t budget)
    : available_cpu_(available_cpu),
      budget_(std::max<uint32_t>(1, std::min(budget, std::max<uint32_t>(1, available_cpu)))) {}

Scheduler::~Scheduler() = default;

void Scheduler::add_node(NodeSpec spec) {
  nodes_[spec.node_id] = std::move(spec);
  status_[spec.node_id] = NodeStatus::PLANNED;
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

Result<void> Scheduler::run(RunContext& ctx, std::vector<NodeStatus>* statuses) {
  if (!built_) {
    auto b = build();
    if (b.failed()) return b;
  }
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
  std::atomic<bool> failed{false};
  std::string fail_node;
  std::string fail_msg;

  auto worker = [&]() {
    while (true) {
      std::string node_id;
      {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [&] {
          return failed.load() || cancel_.load() || !ready.empty() || active.load() == 0;
        });
        if (failed.load() || cancel_.load()) {
          // 取消未运行节点
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
        node_id = ready.front(); ready.pop();
        status[node_id] = NodeStatus::RUNNING;
        ++active;
      }
      // 执行
      {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end() && it->second.fn) {
          ctx.set_thread_budget(budget_);
          auto r = it->second.fn(node_id, ctx);
          if (r.failed() && !failed.load()) {
            std::lock_guard<std::mutex> lk(mtx);
            failed.store(true);
            fail_node = node_id;
            fail_msg = r.error().message();
          }
        }
        if ((ctx.cancelled() || cancel_.load()) && !failed.load()) {
          failed.store(true);
          fail_node = node_id;
          fail_msg = "cancelled";
        }
      }
      {
        std::lock_guard<std::mutex> lk(mtx);
        --active;
        status[node_id] = failed.load() ? NodeStatus::FAILED : NodeStatus::COMPLETED;
        for (const auto& nxt : rev[node_id]) {
          if (--remaining_deps[nxt] == 0 && !failed.load()) ready.push(nxt);
        }
        cv.notify_all();
      }
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(budget_);
  for (uint32_t i = 0; i < budget_; ++i) pool.emplace_back(worker);
  for (auto& t : pool) t.join();

  // 未运行节点标 CANCELLED/SKIPPED
  for (auto& [id, st] : status) {
    if (st == NodeStatus::PLANNED || st == NodeStatus::QUEUED) {
      st = failed.load() ? NodeStatus::SKIPPED : NodeStatus::CANCELLED;
    }
  }
  if (statuses) *statuses = std::vector<NodeStatus>(status.size());
  if (statuses) {
    size_t i = 0;
    for (const auto& [id, st] : status) (*statuses)[i++] = st;
  }
  if (failed.load()) {
    return Result<void>::fail(Error(ErrorDomain::BACKEND,
        "node " + fail_node + " failed: " + fail_msg));
  }
  return Result<void>::success();
}

}  // namespace astrocs::core
