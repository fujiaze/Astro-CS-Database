// AstroCS Core Contracts — CORE-005 RunContext 服务接口
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

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

// RunContext: 模块访问服务的唯一通道 (CORE-005: 模块只能通过 context 访问服务)
// 禁止 singleton/global scheduler (G2 checklist)
class RunContext {
 public:
  RunContext() = default;
  RunContext(const RunContext&) = delete;
  RunContext& operator=(const RunContext&) = delete;

  // ── 日志 ──
  void log(LogLevel level, const std::string& component, const std::string& message);
  const std::vector<std::string>& log_entries() const { return log_entries_; }

  // ── 指标 ──
  void add_metric(const std::string& name, uint64_t value) { metrics_[name] = value; }
  void record_tick(const Metrics& m) { ticks_.push_back(m); }
  const std::map<std::string, uint64_t>& metrics() const { return metrics_; }
  const std::vector<Metrics>& ticks() const { return ticks_; }

  // ── 预算/线程租约 (Runtime 唯一授权) ──
  void set_thread_budget(uint32_t budget) { thread_budget_ = budget; }
  uint32_t thread_budget() const noexcept { return thread_budget_; }
  // RT-003 起经 ThreadBudget 原子预留；此处为预算上限语义（不超卖请求值）。
  ThreadLease acquire_lease(uint32_t requested) const {
    const uint32_t n = requested <= thread_budget_ ? requested : thread_budget_;
    return ThreadLease::make(n);
  }

  // ── artifact 存取 (模块经此读写; 禁止直接路径猜测) ──
  Result<void> store_artifact(DataArtifactDescriptor desc);
  const DataArtifactDescriptor* get_artifact(const std::string& id) const;
  std::vector<std::string> artifact_ids() const;

  // ── 取消 ──
  CancellationToken& cancel_token() noexcept { return cancel_; }
  bool cancelled() const noexcept { return cancel_.cancelled(); }

  // ── checkpoint (CORE-007 边界; 此处仅接口) ──
  void mark_checkpoint(const std::string& node_id) { checkpoints_.push_back(node_id); }
  const std::vector<std::string>& checkpoints() const { return checkpoints_; }

 private:
  std::vector<std::string> log_entries_;
  std::map<std::string, uint64_t> metrics_;
  std::vector<Metrics> ticks_;
  uint32_t thread_budget_ = 1;
  std::map<std::string, DataArtifactDescriptor> artifacts_;
  CancellationToken cancel_;
  std::vector<std::string> checkpoints_;
};

}  // namespace astrocs::core
