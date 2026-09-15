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
// B13-R13-2 修复: 内存回压等待不得忙等自旋; 回压只压内存超限节点, 不队头阻塞
//   整队 (HoL: ready 中首个满足内存约束的节点即可执行; active==0 时放行队首推进)。
//
// P36 D1 调度重叠契约 (取代 P7-UTIL-003 的全局 heavy_gate 互斥):
// ---------------------------------------------------------------------------
// 缺陷 (P24 §4.1 实测): 旧实现用一把全局 heavy_mu 把所有"声明满额预算"的
//   cpu_heavy 节点串行化 —— Σ节点墙钟 == 整 run 墙钟 (零重叠), 38.8% 墙钟落在
//   平均只用 ~1.6 核的低并行度节点上。串行根因不是"资源真冲突", 而是判定只问
//   "是否声明满额"就整段互斥; 而**互不冲突**的判定应按声明需求与空闲预算比较。
//
// 三种模式 (ASTROCS_SCHED_OVERLAP, 每次 run 起始读取):
//   demand (默认): 需求准入 —— 节点 i 的预留 r_i = 声明需求 D_i (max_workers,
//     0 -> budget; 上限即租约上限), 当且仅当 free = budget - Σ在途预留 >= D_i
//     且内存约束满足时准入; 一次锁内对就绪集合逐个准入 ⇒ **需求之和 <= 预算的
//     节点真正重叠**。节点永不低于声明需求运行 —— 因为模块 OMP team 在节点起始
//     固定 (ScopedOmpWorkerInjection), 降级不可恢复, 故本模式零退化。
//   fair: max-min 公平均分 (P24 R1-1 的"朴素释放互斥") —— 仅作测量/阴性对照:
//     满额节点会在同伴在途时以 free/k 启动并把该宽度保持到节点结束。
//   legacy (=0/off): 旧 P7-UTIL-003 全局互斥 (一次仅一个节点在途) —— 回滚对照。
// 不变量 (三模式共同): Σ在途预留 <= budget_ (不超卖); r_i <= D_i; 无空租约
//   ⇒ 模块不会回退到未记账的 cap=1 线程。单进程仍只有一个 Scheduler 与一个
//   ThreadBudget 源 (§10.4); 无私有线程池; 无硬编码线程数 (D_i 全部来自模块
//   plan() 声明, P10-UTIL2-005 通道)。
#include "astrocs/core/scheduler.h"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <set>
#include <string>
#include <mutex>

namespace astrocs::core {

namespace {

// P36 D1: 重叠准入模式 (默认 demand)。
enum class SchedOverlapMode { kDemand = 0, kFair = 1, kLegacy = 2 };

SchedOverlapMode sched_overlap_mode() {
  const char* v = std::getenv("ASTROCS_SCHED_OVERLAP");
  if (!v || !v[0]) return SchedOverlapMode::kDemand;  // 默认需求准入
  const std::string s(v);
  if (s == "fair") return SchedOverlapMode::kFair;
  if (s == "0" || s == "off" || s == "false" || s == "legacy")
    return SchedOverlapMode::kLegacy;
  return SchedOverlapMode::kDemand;
}

}  // namespace

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
  // RT-007: run 边界 —— 每 run 起始清除调度器取消位。
  cancel_.store(false, std::memory_order_release);
  ctx.cancel_token().reset();
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
  std::deque<std::string> ready;
  // P36 D1: 已准入待执行队列 + 每节点预算预留账本 (Σ reserved <= budget_)。
  std::deque<std::string> admitted;
  std::map<std::string, uint32_t> reserved_of;
  for (const auto& [id, spec] : nodes_) {
    remaining_deps[id] = static_cast<int>(spec.deps.size());
    for (const auto& d : spec.deps) rev[d].push_back(id);
    if (spec.deps.empty()) ready.push_back(id);
  }
  std::atomic<uint32_t> active{0};
  std::atomic<uint32_t> reserved_total{0};   // Σ 在途预留槽 (不变量: <= budget_)
  std::atomic<uint64_t> mem_used{0};
  // P36 D1: 状态代际 —— 完成/新就绪时自增并 notify, 挂起的 worker 据此重跑准入
  // (替代"ready 非空"谓词, 避免预算/内存受限时的忙等自旋)。
  std::atomic<uint64_t> epoch{0};
  std::atomic<bool> cancelled_run{false};
  std::string fail_node;
  std::string fail_msg;
  ErrorDomain fail_domain = ErrorDomain::INTERNAL;  // RT-008: 保留节点原始错误域
  std::set<std::string> blocked;  // 失败节点的传递依赖（SKIPPED）

  const SchedOverlapMode mode = sched_overlap_mode();

  // P36 D1: 节点声明需求 —— max_workers (0 -> budget) 为可并行度上限; min_workers
  // 为不可降级下限。二者来自模块 plan() 声明 (P10-UTIL2-005 通道), 调度器不硬编码。
  auto demand_of = [&](const std::string& id) -> uint32_t {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return 1;
    const uint32_t m = std::max<uint32_t>(1u, it->second.min_workers);
    uint32_t d = it->second.max_workers;
    if (d == 0 || d > budget_) d = budget_;
    return std::max(d, m);
  };
  auto min_of = [&](const std::string& id) -> uint32_t {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? 1u : std::max<uint32_t>(1u, it->second.min_workers);
  };
  auto mem_of = [&](const std::string& id) -> uint64_t {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? 0u : it->second.estimated_memory_bytes;
  };
  auto mem_ok = [&](const std::string& id, uint64_t need) -> bool {
    // 与旧回压语义一致: active>0 时才严格判定; 无在途时放行 (无死锁推进)。
    if (memory_limit_bytes_ == 0 || active.load() == 0) return true;
    return mem_used.load() + need <= memory_limit_bytes_;
  };

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

  // P36 D1: 账本准入 (调用者持 mtx)。先传播失败下游 SKIPPED, 再按模式准入。
  auto admit_locked = [&]() {
    // (a) 失败上游的传递依赖 → SKIPPED (不占预算/内存), 并传播新就绪节点
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto it = ready.begin(); it != ready.end();) {
        if (blocked.count(*it)) {
          const std::string id = *it;
          status[id] = NodeStatus::SKIPPED;
          for (const auto& nxt : rev[id]) {
            if (--remaining_deps[nxt] == 0) ready.push_back(nxt);
          }
          it = ready.erase(it);
          changed = true;
        } else {
          ++it;
        }
      }
    }
    if (ready.empty()) return;

    auto admit_one = [&](std::deque<std::string>::iterator it, uint32_t r) {
      const std::string id = *it;
      reserved_of[id] = r;
      reserved_total.fetch_add(r, std::memory_order_relaxed);
      mem_used.fetch_add(mem_of(id), std::memory_order_relaxed);
      status[id] = NodeStatus::RUNNING;
      active.fetch_add(1, std::memory_order_relaxed);
      admitted.push_back(id);
      ready.erase(it);
      ++epoch;
    };

    if (mode == SchedOverlapMode::kLegacy) {
      // 旧全局互斥: 一次仅一个节点在途; 独占时拿声明需求 (<= budget)。
      if (active.load() > 0) return;
      for (auto it = ready.begin(); it != ready.end(); ++it) {
        const uint32_t d = demand_of(*it);
        if (d > budget_) continue;
        if (!mem_ok(*it, mem_of(*it))) continue;
        admit_one(it, d);
        return;
      }
      return;
    }

    const uint32_t free_budget =
        budget_ - reserved_total.load(std::memory_order_relaxed);
    if (free_budget == 0) return;

    if (mode == SchedOverlapMode::kDemand) {
      // 需求准入: 逐个按声明需求预留, 需求之和 <= 预算的节点真正重叠。
      for (auto it = ready.begin(); it != ready.end();) {
        const uint32_t d = demand_of(*it);
        if (d > budget_ - reserved_total.load(std::memory_order_relaxed)) {
          ++it;
          continue;
        }
        if (!mem_ok(*it, mem_of(*it))) { ++it; continue; }
        admit_one(it, d);
        it = ready.begin();  // 重新扫描 (预留/内存已变化, 队列可能仍有序)
      }
      return;
    }

    // kFair: max-min 公平均分 (测量/阴性对照; 会让满额节点以 free/k 启动)。
    std::vector<std::string> elig;
    std::map<std::string, uint32_t> alloc;
    for (auto it = ready.begin(); it != ready.end(); ++it) {
      if (!mem_ok(*it, mem_of(*it))) continue;
      elig.push_back(*it);
      alloc[*it] = 0u;
    }
    if (elig.empty()) return;
    uint32_t remaining = free_budget;
    std::vector<std::string> act = elig;
    while (!act.empty() && remaining > 0) {
      const uint32_t share = std::max<uint32_t>(
          1u, remaining / static_cast<uint32_t>(act.size()));
      std::vector<std::string> next;
      for (const auto& id : act) {
        const uint32_t d = demand_of(id);
        const uint32_t have = alloc[id];
        const uint32_t room = d > have ? d - have : 0u;
        const uint32_t add = std::min(share, std::min(room, remaining));
        alloc[id] = have + add;
        remaining -= add;
        if (alloc[id] < d) next.push_back(id);
      }
      if (next.size() == act.size() && share <= 1u) break;
      act.swap(next);
    }
    for (auto it = ready.begin(); it != ready.end();) {
      const std::string id = *it;
      const uint32_t r = alloc.count(id) ? alloc[id] : 0u;
      if (r < min_of(id) || !mem_ok(id, mem_of(id))) { ++it; continue; }
      admit_one(it, r);
      it = ready.begin();  // erase 后重启扫描 (alloc 固定, 剩余节点逐一准入)
    }
  };

  // P36 D1: 防御路径 —— active==0 且有就绪节点却仍未准入时强制准入队首至少
  // min_workers 槽, 保证无死锁推进。
  auto force_admit_front_locked = [&]() {
    if (ready.empty()) return;
    const std::string id = ready.front();
    ready.pop_front();
    const uint32_t free_budget =
        budget_ > reserved_total.load(std::memory_order_relaxed)
            ? budget_ - reserved_total.load(std::memory_order_relaxed)
            : 1u;
    const uint32_t r = std::max(min_of(id), std::min(free_budget, demand_of(id)));
    reserved_of[id] = r;
    reserved_total.fetch_add(r, std::memory_order_relaxed);
    mem_used.fetch_add(mem_of(id), std::memory_order_relaxed);
    status[id] = NodeStatus::RUNNING;
    active.fetch_add(1, std::memory_order_relaxed);
    admitted.push_back(id);
    ++epoch;
  };

  auto worker = [&]() {
    uint64_t seen_epoch = 0;
    while (true) {
      std::string node_id;
      uint32_t my_reserved = 0u;
      {
        std::unique_lock<std::mutex> lk(mtx);
        for (;;) {
          if (cancelled_run.load()) {
            // 取消: 已准入未执行 / 仍就绪的节点就地 CANCELLED 并释放预留
            while (!admitted.empty()) {
              const std::string id = admitted.front(); admitted.pop_front();
              auto ri = reserved_of.find(id);
              if (ri != reserved_of.end()) {
                reserved_total.fetch_sub(ri->second, std::memory_order_relaxed);
                mem_used.fetch_sub(mem_of(id), std::memory_order_relaxed);
                reserved_of.erase(ri);
                --active;
              }
              status[id] = NodeStatus::CANCELLED;
            }
            while (!ready.empty()) {
              status[ready.front()] = NodeStatus::CANCELLED;
              ready.pop_front();
            }
            return;
          }
          if (admitted.empty()) admit_locked();
          if (!admitted.empty()) break;
          if (ready.empty() && active.load() == 0) return;  // 全部完成
          if (active.load() == 0) {
            // 防御: 无在途且就绪却未准入 → 强制推进 (不死锁)
            force_admit_front_locked();
            continue;
          }
          cv.wait(lk, [&] {
            return cancelled_run.load() || !admitted.empty() ||
                   active.load() == 0 ||
                   epoch.load(std::memory_order_relaxed) != seen_epoch;
          });
          seen_epoch = epoch.load(std::memory_order_relaxed);
        }
        node_id = admitted.front();
        admitted.pop_front();
        my_reserved = reserved_of.count(node_id) ? reserved_of[node_id] : 1u;
        // P36 D1: 本节点可用份额 = 预留槽数 (经 thread_local hint 收缩
        // RunContext::acquire_lease 的 want, 不超卖)。
        set_dispatch_budget_hint(my_reserved);
      }
      // 执行
      bool node_ok = true;
      {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end() && it->second.fn) {
          // RT-006: 节点执行期间线程本地归属当前 node；RAII 复位。
          trace_set_current_node(node_id);
          struct NodeTlsGuard {
            ~NodeTlsGuard() { trace_set_current_node(""); }
          } tls_guard;
          // 首败者记录：同一锁内判定 + 写入。
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
              record_failure(node_id, r.error().message(), r.error().domain());
            }
          } catch (const std::exception& e) {
            node_ok = false;
            record_failure(node_id, std::string("node fn threw: ") + e.what(),
                           ErrorDomain::INTERNAL);
          } catch (...) {
            node_ok = false;
            record_failure(node_id, "node fn threw (non-std exception)",
                           ErrorDomain::INTERNAL);
          }
        }
        if ((ctx.cancelled() || cancel_.load()) && !cancelled_run.load()) {
          // RT-007: 取消观察 —— 结果弃用, run 进入 cancelled。
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
      set_dispatch_budget_hint(0);  // P36 D1: 节点结束复位（异常路径亦到达）
      {
        std::lock_guard<std::mutex> lk(mtx);
        --active;
        mem_used.fetch_sub(mem_of(node_id), std::memory_order_relaxed);
        auto ri = reserved_of.find(node_id);
        if (ri != reserved_of.end()) {
          reserved_total.fetch_sub(ri->second, std::memory_order_relaxed);
          reserved_of.erase(ri);
        }
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
            if (--remaining_deps[nxt] == 0) ready.push_back(nxt);
          }
        }
        ++epoch;  // 预算/内存释放 + 新就绪 → 唤醒挂起 worker 重跑准入
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
