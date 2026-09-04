// AstroCS Core Contracts — CORE-001 Result/Error/Cancel 语义 (API-001 §2.2)
#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace astrocs::core {

// 稳定 error domains (API-001 §2.2; 错误码不随实现漂移)
enum class ErrorDomain : uint8_t {
  CONFIG = 0,                // 配置/参数错误
  DATA = 1,                  // 数据/合同违例
  SCIENCE_PRECONDITION = 2,  // 科学前置条件失败
  IO = 3,                    // I/O 失败
  RESOURCE = 4,              // 资源/配额失败
  BACKEND = 5,               // 计算后端失败
  CANCELLED = 6,             // 用户取消
  INTERNAL = 7,              // 未分类内部错误
};

constexpr const char* error_domain_name(ErrorDomain d) noexcept {
  switch (d) {
    case ErrorDomain::CONFIG: return "CONFIG";
    case ErrorDomain::DATA: return "DATA";
    case ErrorDomain::SCIENCE_PRECONDITION: return "SCIENCE_PRECONDITION";
    case ErrorDomain::IO: return "IO";
    case ErrorDomain::RESOURCE: return "RESOURCE";
    case ErrorDomain::BACKEND: return "BACKEND";
    case ErrorDomain::CANCELLED: return "CANCELLED";
    case ErrorDomain::INTERNAL: return "INTERNAL";
  }
  return "INTERNAL";
}

// CLI 退出码映射 (cli/exit_codes.h 唯一源; 此处只做映射表, 不重定义数值)
enum class ExitCode : uint8_t {
  OK = 0, ARGS = 2, INPUT = 3, SCIENCE = 4, BACKEND = 5,
  COMPUTE = 6, IO = 7, INTEGRITY = 8, CANCELLED = 9, RESOURCE = 10, INTERNAL = 70,
};

// Error: 不可变错误对象, 支持 nested cause 与序列化
class Error {
 public:
  Error() = default;
  Error(ErrorDomain domain, std::string message)
      : domain_(domain), message_(std::move(message)) {}
  Error(const Error& other)
      : domain_(other.domain_), message_(other.message_) {
    if (other.cause_) cause_ = std::make_unique<Error>(*other.cause_);
  }
  Error& operator=(const Error& other) {
    if (this != &other) {
      domain_ = other.domain_;
      message_ = other.message_;
      cause_ = other.cause_ ? std::make_unique<Error>(*other.cause_) : nullptr;
    }
    return *this;
  }

  ErrorDomain domain() const noexcept { return domain_; }
  const std::string& message() const noexcept { return message_; }

  // nested cause (CORE-001: 支持 cause 链)
  void set_cause(const Error& cause) { cause_ = std::make_unique<Error>(cause); }
  const Error* cause() const noexcept { return cause_.get(); }
  bool has_cause() const noexcept { return static_cast<bool>(cause_); }

  // 稳定序列化 (human + machine 双表示)
  std::string to_string() const {
    std::string out = std::string(error_domain_name(domain_)) + ": " + message_;
    if (cause_) out += " [cause: " + cause_->to_string() + "]";
    return out;
  }
  std::vector<std::string> cause_chain() const {
    std::vector<std::string> chain{to_string()};
    for (const Error* c = cause(); c; c = c->cause()) chain.push_back(c->to_string());
    return chain;
  }

 private:
  ErrorDomain domain_{ErrorDomain::INTERNAL};
  std::string message_;
  std::unique_ptr<Error> cause_;
};

// Result<T>: value 或 Error, 禁止裸 exit/abort (API-001: 模块不得直接退出进程)
template <typename T>
class Result {
 public:
  Result(T value) : value_(std::move(value)) {}          // NOLINT
  Result(Error error) : error_(std::move(error)) {}      // NOLINT

  static Result ok(T v) { return Result(std::move(v)); }
  static Result fail(Error e) { return Result(std::move(e)); }

  bool ok() const noexcept { return value_.has_value(); }
  bool failed() const noexcept { return !value_.has_value(); }

  T& value() & {
    if (!value_) throw std::logic_error("Result::value() on error: " + error_->to_string());
    return *value_;
  }
  const T& value() const& {
    if (!value_) throw std::logic_error("Result::value() on error: " + error_->to_string());
    return *value_;
  }

  const Error& error() const& {
    if (value_) throw std::logic_error("Result::error() on ok");
    return *error_;
  }

  // value_or: 错误时返回默认 (仅对可默认构造/拷贝类型)
  template <typename U>
  T value_or(U&& fallback) const& {
    return value_ ? *value_ : T(std::forward<U>(fallback));
  }

 private:
  std::optional<T> value_;
  std::optional<Error> error_;
};

// 特化: void 结果
template <>
class Result<void> {
 public:
  Result() = default;
  Result(Error error) : error_(std::move(error)) {}  // NOLINT
  static Result success() { return Result(); }
  static Result fail(Error e) { return Result(std::move(e)); }
  bool ok() const noexcept { return !error_.has_value(); }
  bool failed() const noexcept { return error_.has_value(); }
  const Error& error() const& {
    if (!error_) throw std::logic_error("Result<void>::error() on ok");
    return *error_;
  }

 private:
  std::optional<Error> error_;
};

// CancellationToken: 取消传播 (API-001 §4; 模块不自行轮询全局状态)
class CancellationToken {
 public:
  void cancel() noexcept { cancelled_.store(true, std::memory_order_release); }
  bool cancelled() const noexcept {
    return cancelled_.load(std::memory_order_acquire);
  }
  void reset() noexcept { cancelled_.store(false, std::memory_order_release); }

 private:
  std::atomic<bool> cancelled_{false};
};

// ── RT-006 运行 trace 事件（真实观测；禁止 config 值冒充观测） ──
// trace 事件由 executor/scheduler/provider/module/monitor 在真实运行点填写：
//   event_type ∈ {module_call, provider_enter, provider_leave, worker_task,
//                 node_start, node_end, artifact_publish, checkpoint, error}
// 观测字段（observer_filled）与实际值同列；配置/预计值必须放 planned/config 前缀
// 字段或单独字段，绝不覆盖观测值（14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §4）。
enum class TraceEventType : uint8_t {
  MODULE_CALL = 0,      // 节点执行真实 module（含 DLL entry）调用
  PROVIDER_ENTER = 1,   // provider/kernel 进入（provider 观测）
  PROVIDER_LEAVE = 2,   // provider/kernel 离开（含 cpu/wall 计时）
  WORKER_TASK = 3,      // executor worker 领取/完成真实任务
  NODE_START = 4,       // scheduler 节点开始（node fn 入口）
  NODE_END = 5,         // scheduler 节点结束（状态/耗时）
  ARTIFACT_PUBLISH = 6, // ArtifactStore 原子发布（真实 hash/size）
  CHECKPOINT = 7,       // 节点 checkpoint（顺序可追溯）
  ERROR = 8,            // 真实错误（错误码/域/节点）
};

constexpr const char* trace_event_type_name(TraceEventType t) noexcept {
  switch (t) {
    case TraceEventType::MODULE_CALL: return "module_call";
    case TraceEventType::PROVIDER_ENTER: return "provider_enter";
    case TraceEventType::PROVIDER_LEAVE: return "provider_leave";
    case TraceEventType::WORKER_TASK: return "worker_task";
    case TraceEventType::NODE_START: return "node_start";
    case TraceEventType::NODE_END: return "node_end";
    case TraceEventType::ARTIFACT_PUBLISH: return "artifact_publish";
    case TraceEventType::CHECKPOINT: return "checkpoint";
    case TraceEventType::ERROR: return "error";
  }
  return "unknown";
}

// 单条运行 trace 记录（RT-006 冻结字段；JSONL 一事件一行，可重放到图）
struct TraceEvent {
  TraceEventType type = TraceEventType::MODULE_CALL;
  std::string ts_utc;         // RFC3339 UTC（观测时刻）
  std::string run_id;         // 本次运行 ID
  std::string node_id;        // 所属节点
  std::string module_id;      // 真实 module ID（MODULE_CALL）
  std::string module_version; // module 版本
  std::string dll_name;       // 真实 DLL 名（缺失/未知时为空）
  std::string dll_sha256;     // DLL 内容 hash（真实观测）
  std::string build_id;       // build id / 源码 commit（真实）
  std::string entry;          // 真实导出入口（如 astrocs_phase2_*_v1）
  uint64_t call_count = 0;    // 累计调用计数（该 node/module）
  uint32_t workers = 0;       // 实际 workers（租约数；node_end/worker_task）
  uint32_t granted_workers = 0; // 授予租约上限（观测）
  std::string provider;       // provider ID（baseline/avx2/avx512/...）
  std::string kernel_id;      // provider kernel ID
  std::string status;         // 终态字符串（COMPLETED/FAILED/CANCELLED/SKIPPED...）
  std::string error;          // 错误消息（ERROR 事件）
  std::string error_domain;   // 错误域（ERROR 事件）
  std::string artifact_id;    // 发布/读取 artifact id
  std::string artifact_sha256;// artifact 内容 hash
  uint64_t artifact_size = 0; // artifact 字节数
  double cpu_ms = 0.0;        // CPU 时间观测（ms）
  double wall_ms = 0.0;       // wall 时间观测（ms）
  uint64_t seq = 0;           // 全局递增序号（顺序可追溯）

  // 稳定 JSONL（转义正确；nlohmann dump 单行）
  std::string to_jsonl() const;
  // 从 JSONL 行解析（供重放；非法行 → false）
  static bool from_jsonl(const std::string& line, TraceEvent* out);
  // 与另一事件按语义键去重（同 run/node/module/entry 连续调用 → 重复检测用）
  bool same_call_site(const TraceEvent& o) const noexcept;
};

}  // namespace astrocs::core
