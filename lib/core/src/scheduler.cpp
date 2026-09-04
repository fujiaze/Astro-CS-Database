// CORE-006 / RT-006/RT-007 统一 DAG 调度器实现:
// 有界 worker 池 + 依赖就绪 + 取消/失败传播 + 内存回压/预留 + 带 node ID 状态输出。
// RT-007 语义固化:
//   - 统一取消 token: Scheduler::cancel() 同时置位本调度器 cancel_ 与当前运行
//     RunContext 的 CancellationToken; 节点 fn 内经 ctx.cancelled() 在
//     chunk/block 边界检查。cancel 后空闲 worker 立即退出（cv 唤醒, 不空转等新任务）。
//   - 节点异常安全: 节点 fn 抛 C++ 异常不杀死 worker、不泄漏 → catch 后按节点失败
//     传播。C++ 异常无显式 Result 错误域 → 归 INTERNAL（显式 Result 失败保留原始
//     域 IO/DATA/...，见 RT-008）。
//   - 取消/失败传播: FAILED 根 + 依赖 SKIPPED + 独立节点完成; 取消置位后所有
//     PLANNED/QUEUED 节点标 CANCELLED。
#include "astrocs/core/scheduler.h"

#include <algorithm>
#include <set>
#include <mutex>

namespace astrocs::core {

Scheduler::Scheduler(uint32_t available_cpu, uint32_t budget,
                     uint64_t memory_limit_bytes)
    : budget_(std::max<uint32_t>(1, std::min(budget, std::max<uint32_t>(1, available_cpu)))),
      memory_limit_bytes_(memory_limit_bytes) {
  // RT-003: 创建唯一 ThreadBudget（run 间复用；budget 由可用资源/上限计算注入）。
  auto b = create_thread_budget(budget_);
  if (b.ok()) budget_obj_ = b.value();
}

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
  // RT-003: worker 启动前单线程注入真实 ThreadBudget（run 间复用；重复 run 不泄漏）
  ctx.set_thread_budget(budget_);
  if (budget_obj_) ctx.set_budget(budget_obj_);
  // RT-006: 注入运行 trace 汇（run 内模块/节点经 ctx.record_trace 观测）
  if (obs_store_) {
    ctx.set_trace_store(obs_store_);
    ctx.set_run_id(obs_run_id_);
  }
  // RT-007: run 边界 —— 每 run 起始清除调度器取消位。取消语义只作用于「当前正在
  // 执行的 run」：run 外（调度器空闲）调 cancel() 只置位 cancel_，随后 run() 起始
  // 清除它，避免一次历史取消让后续全新 run 无条件失败（陈旧取消不跨 run 泄漏）。
  // run 中 cancel()（外部线程）置位 cancel_ + 唤醒当前 run 的 ctx token → 本 run
  // 结束；该次取消已被消费，不会残留到下一 run。
  cancel_.store(false, std::memory_order_release);
  ctx.cancel_token().reset();
  // 登记本次 run 的活动 token：cancel() 能桥到 ctx（Scheduler 与 RunContext 统一）。
  {
    std::lock_guard<std::mutex> lock(run_mu_);
    active_token_ = &ctx.cancel_token();
  }
  struct TokenGuard {
    Scheduler* s;
    ~TokenGuard() {
      std::lock_guard<std::mutex> lock(s->run_mu_);
      s->active_token_ = nullptr;
    }
  } token_guard{this};
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
  ErrorDomain fail_domain = ErrorDomain::INTERNAL;  // RT-008: 保留节点原始错误域
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
          // RT-006: 节点执行期间线程本地归属当前 node（观测事件按节点归属；
          // 并发节点在各自 worker 线程，无交叉）。RAII 复位：异常路径也清理 TLS。
          trace_set_current_node(node_id);
          struct NodeTlsGuard {
            ~NodeTlsGuard() { trace_set_current_node(""); }
          } tls_guard;
          // 首败者记录：同一锁内判定 + 写入（并发节点同时失败只保留第一失败根，
          // 避免裸读 fail_node 的数据竞争；SKIPPED 闭包在同锁 compute_blocked）。
          auto record_failure = [&](const std::string& nid, std::string msg,
                                    ErrorDomain dom) {
            std::lock_guard<std::mutex> lk(mtx);
            if (fail_node.empty()) {
              fail_node = nid;
              fail_msg = std::move(msg);
              fail_domain = dom;
              compute_blocked(nid);
            }
          };
          try {
            auto r = it->second.fn(node_id, ctx);
            node_ok = r.ok();
            if (r.failed()) {
              // RT-008: 保留节点原始错误域(IO/DATA/...)
              record_failure(node_id, r.error().message(), r.error().domain());
            }
          } catch (const std::exception& e) {
            // RT-007: 节点 fn 抛 C++ 异常不得杀死 worker/不得泄漏 → 统一按节点
            // 失败传播。C++ 异常无 Result 域 → 归 INTERNAL（非显式域错误）。
            node_ok = false;
            record_failure(node_id, std::string("node fn threw: ") + e.what(),
                           ErrorDomain::INTERNAL);
          } catch (...) {
            // 非 std 异常（如裸 throw）：同样按 INTERNAL 节点失败传播，不逃逸 worker。
            node_ok = false;
            record_failure(node_id, "node fn threw (non-std exception)",
                           ErrorDomain::INTERNAL);
          }
        }
        if ((ctx.cancelled() || cancel_.load()) && !cancelled_run.load()) {
          // RT-007: 取消观察 —— 本节点在取消置位后返回 → 该节点结果弃用，且本 run
          // 进入 cancelled（首次观察到取消的节点领取 CANCELLED 失败根；若此前已有
          // 真实失败根，保留原根/原域，取消只负责收尾分类）。
          node_ok = false;
          std::lock_guard<std::mutex> lk(mtx);
          cancelled_run.store(true);
          if (fail_node.empty()) {
            fail_node = node_id;
            fail_msg = "cancelled";
            fail_domain = ErrorDomain::CANCELLED;
          }
        }
      }
      {
        std::lock_guard<std::mutex> lk(mtx);
        --active;
        mem_used.fetch_sub(node_mem);
        // RT-007: 取消已 latch 的 run 内, 在途节点结果一律弃用（不得标 COMPLETED,
        // 无 false-success）。真实失败根（先于取消记录, 非取消 root）仍标 FAILED;
        // 其余（含取消观察节点自身）标 CANCELLED。
        NodeStatus st;
        if (!cancelled_run.load()) {
          st = node_ok ? NodeStatus::COMPLETED : NodeStatus::FAILED;
        } else {
          const bool real_root =
              !node_ok && fail_node == node_id &&
              fail_domain != ErrorDomain::CANCELLED;
          st = real_root ? NodeStatus::FAILED : NodeStatus::CANCELLED;
        }
        status[node_id] = st;
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
  // RT-007 收尾分类:
  //   - 真实失败根先于取消记录（非 CANCELLED 域）→ 返回原域失败（取消不掩盖
  //     已发生的真实失败; 无 false-success/无错误域丢失）;
  //   - 取消为决定根（cancel root / 仅 CANCELLED 域）→ 返回 CANCELLED;
  //   - 无失败 → 成功。
  if (cancelled_run.load() &&
      (fail_node.empty() || fail_domain == ErrorDomain::CANCELLED)) {
    return Result<void>::fail(Error(ErrorDomain::CANCELLED,
        "node " + fail_node + " cancelled: " + fail_msg));
  }
  if (!fail_node.empty()) {
    return Result<void>::fail(Error(fail_domain,
        "node " + fail_node + " failed: " + fail_msg));
  }
  return Result<void>::success();
}

}  // namespace astrocs::core
