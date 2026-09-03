// AstroCS Core Contracts — RT-004 唯一共享执行器（CPU heavy + 有界 I/O）
//
// 冻结语义（tasks/03_RUNTIME_DATA_IO_TASKS.md RT-004 + 约束 D.1-D.4 +
// 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §2 线程模型）:
//   - 全进程唯一共享 CPU heavy executor 与唯一有界 I/O executor（每 Phase Runtime
//     一个）；模块/节点不得自建私有永久线程池（std::thread 私建池 → 静态扫描拒绝）。
//   - CPU heavy 任务并发度受 ThreadBudget lease 约束：worker 领取任务前经
//     ThreadBudget::acquire 预留（min=1），执行结束 RAII 归还 → Σ(active) ≤ budget
//     全局不超卖。并发任务不超预算。
//   - 有界 I/O executor：短 I/O（读/写/校验/原子提交）有界并发；队列有界；任务可
//     显式标记 io_write/metadata 串行类（short_io_serial 标记允许，但不得把 heavy
//     kernel 放进串行 I/O）。
//   - worker 不空转：队列空 → worker 阻塞在 CV；队列有工作 → 立即被唤醒取任务
//     （不接受忙等/轮询空转）。
//   - 取消能唤醒等待：cancel() 置位后 notify_all，阻塞中的任务/worker 立即退出。
//   - 执行上下文：任务签名携带 RunContext&（经 ctx.budget()/acquire_lease 取 lease），
//     不携带/不依赖任何全局调度器单例。
//
// 所有权/线程：Executors 为进程内单实例（per Runtime）；enqueue 线程安全；
// 所有任务在 executor worker 线程执行，调用者不得在任务内再 enqueue 同一 executor
// 的阻塞等待（嵌套并行禁止，见约束 D/THREAD_BUDGET_ARCH §3）。
// 本头为 RT-004 冻结合同；实现见 lib/core/src/executor.cpp，测试见
// tests/runtime/test_rt004_executor.py（照 RT-003 harness 模式）。
#pragma once

#ifndef ASTROCS_CORE_EXECUTOR_H
#define ASTROCS_CORE_EXECUTOR_H

#include "astrocs/core/context.h"
#include "astrocs/core/contracts.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace astrocs::core {

// 任务函数：返回 void；结果/错误经任务内 ctx.log + 输出参数或回调上报。
// 任务内可经 ctx.acquire_lease(ctx.budget()->budget()) 原子预留 worker 预算。
using ExecutorTask = std::function<void(RunContext&)>;

// I/O 任务类别（约束 D.2: 短 I/O 串行需显式标记）
enum class IoTaskClass : uint8_t {
  IO_READ = 0,       // 短读（元数据/校验/读回）
  IO_WRITE = 1,      // 原子写/发布（可串行标记）
  METADATA = 2,      // 初始化/清理/日志（串行标记）
  SHORT_SERIAL = 3,  // 显式标记的串行短任务（禁止 heavy kernel 进入）
};

// ── 共享 CPU heavy executor ──
// 唯一共享 executor：worker 数 = ThreadBudget 预算上限（可用资源）；
// 每个任务执行前经 ThreadBudget::acquire(1, max, NONBLOCK) 预留；
// 预算耗尽 → 任务在队列等待（不超卖）；取消 → 唤醒全部等待。
class CpuHeavyExecutor {
 public:
  explicit CpuHeavyExecutor(std::shared_ptr<ThreadBudget> budget);
  ~CpuHeavyExecutor();

  CpuHeavyExecutor(const CpuHeavyExecutor&) = delete;
  CpuHeavyExecutor& operator=(const CpuHeavyExecutor&) = delete;

  // 提交 CPU heavy 任务（并发度受预算约束；worker 不空转）。
  // 任务不得阻塞等待同一 executor 的其他任务（嵌套并行禁止）。
  void enqueue(ExecutorTask task);

  // 置位取消并唤醒所有等待 worker（幂等）。
  void cancel() noexcept;

  // 阻塞直到全部已提交任务完成（取消后尽快返回）。可重复调用。
  void wait_all();

  // worker 数（= 预算上限）；诊断/测试用。
  uint32_t worker_count() const noexcept { return worker_count_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint32_t worker_count_ = 0;

  void worker_loop();          // RT-004: worker 主循环（取任务→acquire lease→执行）
  void io_worker_loop();       // RT-004: I/O worker 主循环（有界并发）
};

// ── 有界 I/O executor ──
// 有界并发：队列容量有界（bounded），任务类别可标记串行短 I/O；
// 短 I/O 允许串行执行但必须带 IoTaskClass（heavy kernel 入 I/O 队列 → 拒绝语义由
// 上层检查，本 executor 只执行已分类任务）。
class IoExecutor {
 public:
  explicit IoExecutor(uint32_t max_concurrency, uint32_t queue_capacity);
  ~IoExecutor();

  IoExecutor(const IoExecutor&) = delete;
  IoExecutor& operator=(const IoExecutor&) = delete;

  // 提交有界 I/O 任务（带类别标记）。队列满 → 返回 false（调用方重试/串行降级）。
  bool enqueue(IoTaskClass cls, ExecutorTask task);

  // 置位取消并唤醒所有等待（幂等）。
  void cancel() noexcept;

  // 阻塞直到全部已提交任务完成。
  void wait_all();

  uint32_t max_concurrency() const noexcept { return max_concurrency_; }
  uint32_t queue_capacity() const noexcept { return queue_capacity_; }
  uint32_t queued() const noexcept;  // 当前排队任务数（诊断）

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint32_t max_concurrency_ = 1;
  uint32_t queue_capacity_ = 8;

  void io_worker_loop();       // RT-004: I/O worker 主循环（有界并发）
};

// 工厂（owner=调用者独占销毁）：
// - create_cpu_heavy_executor(budget): budget 必须非空且 >0。
// - create_io_executor(max_concurrency, queue_capacity): 两者均 >0。
Result<std::unique_ptr<CpuHeavyExecutor>> create_cpu_heavy_executor(
    std::shared_ptr<ThreadBudget> budget) noexcept;
Result<std::unique_ptr<IoExecutor>> create_io_executor(
    uint32_t max_concurrency, uint32_t queue_capacity) noexcept;

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_EXECUTOR_H
