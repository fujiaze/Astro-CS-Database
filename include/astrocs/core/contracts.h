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

}  // namespace astrocs::core
