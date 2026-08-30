// AstroCS Core Contracts — CORE-006 统一 DAG 调度器 + 线程租约
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

// Scheduler: 唯一拥有全局执行顺序与线程预算 (ARCH-001 §1)
// 禁止第二套全局调度器; 模块只投递 work, 不建私有 pool
class Scheduler {
 public:
  // graph: 由 PipelineIR 构建的 node -> deps
  struct NodeSpec {
    std::string node_id;
    std::vector<std::string> deps;  // 依赖的 node_id
    NodeFn fn;
    std::string resource_class;  // metadata|io|cpu_light|cpu_heavy
  };

  explicit Scheduler(uint32_t available_cpu, uint32_t budget);
  ~Scheduler();

  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;

  void add_node(NodeSpec spec);
  Result<void> build();  // 验证 DAG 无环

  // 运行整个 DAG; active_workers 记录并发; cancel 经 context token
  Result<void> run(RunContext& ctx, std::vector<NodeStatus>* statuses = nullptr);

  uint32_t max_concurrency() const { return budget_; }
  void cancel() { cancel_.store(true, std::memory_order_release); }

 private:
  uint32_t available_cpu_;
  uint32_t budget_;
  std::map<std::string, NodeSpec> nodes_;
  std::map<std::string, NodeStatus> status_;
  std::atomic<bool> cancel_{false};
  bool built_ = false;
};

}  // namespace astrocs::core
