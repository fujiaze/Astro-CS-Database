// AstroCS Core Contracts — CORE-005 RunContext 服务接口
#pragma once

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"

#include <cstdint>
#include <map>
#include <memory>
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
class ThreadLease {
 public:
  explicit ThreadLease(uint32_t size) : size_(size) {}
  uint32_t size() const noexcept { return size_; }
  bool acquired() const noexcept { return size_ > 0; }

 private:
  uint32_t size_ = 0;
};

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
  ThreadLease acquire_lease(uint32_t requested) const {
    return ThreadLease(requested <= thread_budget_ ? requested : thread_budget_);
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
