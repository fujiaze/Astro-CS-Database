// AstroCS Core Contracts — CORE-008 统一日志 + 指标 (JSONL)
#pragma once

#include "astrocs/core/context.h"
#include "astrocs/core/contracts.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs::core {

// 结构化日志条目 (JSONL; CORE-008: 模块只写结构化事件, 格式由 Runtime 决定)
struct LogEvent {
  LogLevel level = LogLevel::INFO;
  std::string ts_utc;       // ISO8601 UTC
  std::string component;    // module/node id
  std::string event;        // 事件名: start|progress|end|warn|error|metric
  std::string message;
  uint64_t seq = 0;
  // 可选结构化字段
  std::string node_id;
  std::string run_id;
  double progress = -1.0;   // [0,1] 或 -1 表示无
  uint64_t wall_us = 0;

  std::string to_jsonl() const;
};

// 日志接收器接口 (可注入: console/file/JSONL)
class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void emit(const LogEvent& e) = 0;
};

// 统一 Logger: 时间戳 + 序号 + 级别过滤 (CORE-008)
class Logger {
 public:
  explicit Logger(LogLevel min_level = LogLevel::INFO);

  void attach(std::shared_ptr<LogSink> sink) { sinks_.push_back(std::move(sink)); }

  void log(LogLevel level, const std::string& component, const std::string& event,
           const std::string& message,
           const std::string& node_id = "", const std::string& run_id = "",
           double progress = -1.0, uint64_t wall_us = 0);
  void info(const std::string& component, const std::string& msg) {
    log(LogLevel::INFO, component, "info", msg);
  }
  void warn(const std::string& component, const std::string& msg) {
    log(LogLevel::WARN, component, "warn", msg);
  }
  void error(const std::string& component, const std::string& msg) {
    log(LogLevel::ERROR, component, "error", msg);
  }

  LogLevel min_level() const { return min_level_; }
  size_t emitted() const { return emitted_; }

 private:
  LogLevel min_level_;
  std::vector<std::shared_ptr<LogSink>> sinks_;
  uint64_t seq_ = 0;
  std::atomic<uint64_t> emitted_{0};
};

// 指标聚合: 名称 -> 计数/和/最大/最小/最后 (CORE-008)
class MetricsAggregator {
 public:
  void record(const std::string& name, uint64_t value);
  uint64_t count(const std::string& name) const;
  uint64_t sum(const std::string& name) const;
  uint64_t max(const std::string& name) const;
  std::vector<std::string> names() const;

  // JSONL 导出 (机器可读)
  std::string export_jsonl() const;

 private:
  struct Agg {
    uint64_t count_ = 0, sum_ = 0, max_ = 0;
  };
  std::map<std::string, Agg> agg_;
  mutable std::mutex mtx_;
};

// 内存 sink (测试/捕获用)
class InMemorySink : public LogSink {
 public:
  void emit(const LogEvent& e) override { events_.push_back(e); }
  const std::vector<LogEvent>& events() const { return events_; }
  size_t size() const { return events_.size(); }

 private:
  std::vector<LogEvent> events_;
};

}  // namespace astrocs::core
