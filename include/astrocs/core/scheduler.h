// AstroCS Core Contracts — CORE-006 统一 DAG 调度器 + 线程租约
// RT-006: NodePlan 先估计 work/memory 再取 lease；内存回压；状态输出带 node ID。
#pragma once

#include "astrocs/core/context.h"
#include "astrocs/core/contracts.h"
#include "astrocs/core/pipeline.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace astrocs::core {

// 节点执行入口: (node_id, RunContext) -> Result
using NodeFn = std::function<Result<void>(const std::string&, RunContext&)>;

enum class NodeStatus : uint8_t {
  PLANNED = 0, QUEUED = 1, RUNNING = 2, COMPLETED = 3, FAILED = 4, CANCELLED = 5, SKIPPED = 6,
};

// Scheduler: 唯一拥有全局执行顺序、线程预算与内存回压 (ARCH-001 §1)
// 禁止第二套全局调度器; 模块只投递 work, 不建私有 pool
// RT-003: Scheduler 创建并持有唯一 ThreadBudget；run() 注入 RunContext，
// 模块经 ctx.acquire_lease/ctx.budget() 原子预留（禁止 ThreadLease::make 伪授权）。
class Scheduler {
 public:
  struct NodeSpec {
    std::string node_id;
    std::vector<std::string> deps;  // 依赖的 node_id
    NodeFn fn;
    std::string resource_class;  // metadata|io|cpu_light|cpu_heavy
    uint64_t estimated_memory_bytes = 0;  // NodePlan 估计（内存回压用）
    uint32_t min_workers = 1;             // 该节点所需最小 worker（lease 下限）
    uint32_t max_workers = 1;             // 该节点所需最大 worker（lease 上限）
  };

  // memory_limit_bytes: 0 = 不限制内存回压
  Scheduler(uint32_t available_cpu, uint32_t budget,
            uint64_t memory_limit_bytes = 0);
  ~Scheduler();

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  void add_node(NodeSpec spec);
  Result<void> build();  // 验证 DAG 无环

  // 运行整个 DAG; statuses 输出带 node ID 的 (node_id, status) 快照（RT-006）
  Result<void> run(RunContext& ctx,
                   std::vector<std::pair<std::string, NodeStatus>>* statuses = nullptr);

  uint32_t max_concurrency() const { return budget_; }
  uint64_t memory_limit() const { return memory_limit_bytes_; }
  void cancel() { cancel_.store(true, std::memory_order_release); }

  // RT-003: Scheduler 持有的唯一 ThreadBudget（run 间复用；重复 run 不泄漏）。
  std::shared_ptr<ThreadBudget> thread_budget() const noexcept { return budget_obj_; }

  // RT-006: 注入运行 trace 汇（run 内所有节点事件写入；nullptr 解除）。
  // run() 开始时把 store 与 run_id 注入 ctx（模块/节点经 ctx.record_trace 观测）。
  void set_run_observation(std::shared_ptr<TraceStore> store, std::string run_id) {
    obs_store_ = std::move(store);
    obs_run_id_ = std::move(run_id);
  }
  const std::string& observation_run_id() const noexcept { return obs_run_id_; }

 private:
  uint32_t budget_;
  uint64_t memory_limit_bytes_;
  std::shared_ptr<ThreadBudget> budget_obj_;  // RT-003: 唯一全局预算（run 间复用）
  std::map<std::string, NodeSpec> nodes_;
  std::map<std::string, NodeStatus> status_;
  std::atomic<bool> cancel_{false};
  bool built_ = false;
  std::shared_ptr<TraceStore> obs_store_;  // RT-006: run 观测汇（可选）
  std::string obs_run_id_;                 // RT-006: run 观测 id（可选）
};

}  // namespace astrocs::core
