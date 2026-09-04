// AstroCS Core Contracts — CORE-005 RunContext 服务接口
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"
#include "astrocs/core/trace.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Windows 头 (windows.h 经 monitor.h 等引入) 定义 ERROR 为宏 (值 0),
// 与 LogLevel::ERROR 枚举值冲突 (C2143, 见 WIN-001 Fatduck MSVC 复现)。
// 宏在 windows.h 内的用途已展开完毕, undef 仅恢复本头枚举语义 (同 hiss_reader 先例)。
#ifdef ERROR
#undef ERROR
#endif

namespace astrocs::core {

// 日志级别 (CORE-008 统一日志)
enum class LogLevel : uint8_t { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

// 运行指标 (CORE-005/008)
struct Metrics {
  uint64_t wall_us = 0;
  uint64_t cpu_us = 0;
  uint64_t peak_rss_bytes = 0;
  uint64_t bytes_read = 0;
  uint64_t bytes_written = 0;
  uint32_t active_workers = 0;
  uint32_t completed_units = 0;
};

// 线程租约 (08 §6: Runtime 唯一线程来源)
// RT-001 冻结合同：acquire 返回 ThreadLease（RAII；析构自动归还）。
// RT-002 实现原子 reserve/release；本文件只冻结接口语义。
class ThreadLease {
 public:
  ThreadLease() = default;
  ThreadLease(const ThreadLease&) = delete;
  ThreadLease& operator=(const ThreadLease&) = delete;
  ThreadLease(ThreadLease&& other) noexcept
      : size_(other.size_), release_(std::move(other.release_)) {
    other.size_ = 0;
  }
  ThreadLease& operator=(ThreadLease&& other) noexcept {
    if (this != &other) {
      release();
      size_ = other.size_;
      release_ = std::move(other.release_);
      other.size_ = 0;
    }
    return *this;
  }
  ~ThreadLease() { release(); }

  uint32_t size() const noexcept { return size_; }
  bool acquired() const noexcept { return size_ > 0; }

  // 归还（幂等；析构/异常/取消路径自动调用）
  void release() noexcept {
    if (size_ > 0 && release_) {
      release_();
    }
    size_ = 0;
  }

 private:
  // RT-002 构造：acquire(min,max) 成功后注入 size 与归还回调
  explicit ThreadLease(uint32_t size, std::function<void()> release)  // NOLINT
      : size_(size), release_(std::move(release)) {}
  uint32_t size_ = 0;
  std::function<void()> release_;
  friend class ThreadBudget;
  friend class RunContext;

 public:
  // 仅大小语义的租约（RT-003 前兼容；无原子预留，析构不回调）
  static ThreadLease make(uint32_t size) {
    ThreadLease l;
    l.size_ = size;
    return l;
  }
};

// ── ThreadBudget: 全进程原子线程租约 (RT-002) ──
// 语义: acquire(min,max,policy) 原子预留 token；sum(active)<=budget 全局不超卖；
// 取消/异常 RAII 自动归还；Scheduler 自身 worker 与节点内部 work 共用同一预算。
enum class AcquirePolicy : uint8_t {
  BLOCK = 0,        // 阻塞等待满足 min（需并发归还方，否则死锁风险由调用方管理）
  NONBLOCK = 1,     // 立即判定；不足 min 返回空租约
  BEST_EFFORT = 2,  // 立即判定；不足 min 也返回 available（>0 时）
};

class ThreadBudget {
 public:
  explicit ThreadBudget(uint32_t budget) : budget_(budget) {}
  ThreadBudget(const ThreadBudget&) = delete;
  ThreadBudget& operator=(const ThreadBudget&) = delete;

  // acquire(min, max, policy):
  // - NONBLOCK: available<min → 空租约；
  // - BEST_EFFORT: available<min 且 available>0 → 取 available；
  // - BLOCK: 自旋/CV 等待直到 available>=min（调用方保证最终有归还方）。
  // 成功租约 size<=max 且 <=available；线程安全：可并发；阻塞：BLOCK 除外。
  ThreadLease acquire(uint32_t min, uint32_t max,
                      AcquirePolicy policy = AcquirePolicy::NONBLOCK) noexcept;

  uint32_t available() const noexcept { return available_.load(std::memory_order_relaxed); }
  uint32_t budget() const noexcept { return budget_; }

  // 工厂初始化（仅 create_thread_budget 调用一次；非并发阶段）
  void reset_available() noexcept {
    available_.store(budget_, std::memory_order_relaxed);
  }

 private:
  uint32_t budget_;
  std::atomic<uint32_t> available_{0};
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  // 构造已预留 size 的租约并注入归还回调（内部）
  ThreadLease _make_lease(uint32_t got) noexcept;
};

// 全局唯一预算工厂（RT-002 提供实现；owner=Runtime）
// budget: 有效 CPU 配额（CPU-002 探测结果，>0）。
// 失败返回 Error(RESOURCE)。
Result<std::shared_ptr<ThreadBudget>> create_thread_budget(uint32_t budget) noexcept;

// ── RT-006 线程本地观测（scheduler worker 执行节点期间的当前节点/provider） ──
// scheduler 的 worker 线程各自顺序执行节点 fn；节点归属按线程本地判定，
// 避免并发节点共享 RunContext 时的交叉归属。executor worker 任务线程亦同。
// 定义在 context.cpp（thread_local）；头内声明供 RunContext inline 转发。
void trace_set_current_node(const std::string& node_id);
const std::string& trace_current_node();
void trace_set_provider(const std::string& provider);
const std::string& trace_provider();

// RunContext: 模块访问服务的唯一通道 (CORE-005: 模块只能通过 context 访问服务)
// 禁止 singleton/global scheduler (G2 checklist)
// RT-003: 日志/metrics/artifact/checkpoint/cancel 全部线程安全；
// 不暴露裸容器引用；不返回可能因并发插入失效的 map 指针。
class RunContext {
 public:
  RunContext() = default;
  RunContext(const RunContext&) = delete;
  RunContext& operator=(const RunContext&) = delete;

  // ── 日志（线程安全 sink；返回快照，不暴露内部引用） ──
  void log(LogLevel level, const std::string& component, const std::string& message);
  // 返回日志快照（拷贝）；线程安全
  std::vector<std::string> log_entries() const;

  // ── 指标（线程安全） ──
  void add_metric(const std::string& name, uint64_t value);
  void record_tick(const Metrics& m);
  // 返回 metrics 快照（拷贝）；线程安全
  std::map<std::string, uint64_t> metrics() const;
  // 返回 ticks 快照（拷贝）；线程安全
  std::vector<Metrics> ticks() const;

  // ── 预算/线程租约 (Runtime 唯一授权) ──
  void set_thread_budget(uint32_t budget) { thread_budget_ = budget; }
  uint32_t thread_budget() const noexcept { return thread_budget_; }

  // RT-003: Scheduler/Runtime 注入真实 ThreadBudget（唯一全局预算对象）。
  // 注入后 acquire_lease 经原子预留；未注入时（仅测试/非调度上下文）返回空租约
  // （拒绝伪授权——绝不退回 ThreadLease::make）。
  void set_budget(std::shared_ptr<ThreadBudget> budget) {
    budget_ = std::move(budget);
    if (budget_) thread_budget_ = budget_->budget();
  }
  std::shared_ptr<ThreadBudget> budget() const noexcept { return budget_; }

  // RT-003: 经注入的 ThreadBudget 原子预留 requested（cap 到预算上限）。
  // 预算耗尽 → 空租约（NONBLOCK）；租约 RAII 析构自动归还。
  // 无注入预算 → 空租约（不伪造授权）。
  ThreadLease acquire_lease(uint32_t requested) const;

  // ── artifact 存取 (模块经此读写; 禁止直接路径猜测) ──
  // 线程安全；duplicate id 写 → 确定性失败（唯一 producer）
  Result<void> store_artifact(DataArtifactDescriptor desc);
  // 返回描述快照（拷贝）；id 不存在 → ok=false。不返回内部指针。
  bool get_artifact(const std::string& id, DataArtifactDescriptor* out) const;
  // 返回已存 id 快照（拷贝）；线程安全
  std::vector<std::string> artifact_ids() const;

  // ── 取消（原子） ──
  CancellationToken& cancel_token() noexcept { return cancel_; }
  bool cancelled() const noexcept { return cancel_.cancelled(); }

  // ── checkpoint (顺序可追溯；线程安全) ──
  void mark_checkpoint(const std::string& node_id);
  // 返回 checkpoint 序列快照（拷贝）；线程安全
  std::vector<std::string> checkpoints() const;

  // ── RT-006 运行 trace（真实观测汇；线程安全） ──
  // 注入 TraceStore（Scheduler/Runtime 每 run 注入）；未注入时 record_trace
  // 为无操作（不伪造观测）。store() 返回空指针即表示无观测汇。
  void set_trace_store(std::shared_ptr<TraceStore> store) {
    trace_store_ = std::move(store);
  }
  std::shared_ptr<TraceStore> trace_store() const noexcept { return trace_store_; }
  void set_run_id(const std::string& run_id) { run_id_ = run_id; }
  const std::string& run_id() const noexcept { return run_id_; }
  // 记录一条真实 trace 事件（executor/module/provider 在运行点填写；
  // node_id 自动回退线程本地 trace_current_node()）。
  void record_trace(TraceEvent e) const;
  // 当前执行节点（线程本地；scheduler worker 在 node fn 前设置）
  void set_current_node(const std::string& node_id) { trace_set_current_node(node_id); }
  const std::string& current_node() const noexcept { return trace_current_node(); }
  // 当前 provider（线程本地；模块/provider 后端在真实选择点置位，node end 观测）
  void set_provider(const std::string& provider) { trace_set_provider(provider); }
  const std::string& provider() const noexcept { return trace_provider(); }

 private:
  mutable std::mutex mu_;
  std::vector<std::string> log_entries_;
  std::map<std::string, uint64_t> metrics_;
  std::vector<Metrics> ticks_;
  uint32_t thread_budget_ = 1;
  std::shared_ptr<ThreadBudget> budget_;  // RT-003: Scheduler 注入的唯一预算对象
  std::map<std::string, DataArtifactDescriptor> artifacts_;
  CancellationToken cancel_;
  std::vector<std::string> checkpoints_;
  std::shared_ptr<TraceStore> trace_store_;  // RT-006: 运行 trace 汇（每 run）
  std::string run_id_;                       // RT-006: 本次运行 ID
};

}  // namespace astrocs::core
