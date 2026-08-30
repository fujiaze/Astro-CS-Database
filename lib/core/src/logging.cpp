// CORE-008 统一日志 + 指标实现
#include "astrocs/core/logging.h"

#include <cstdio>
#include <ctime>

namespace astrocs::core {

namespace {
std::string utc_now() {
  char buf[32];
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}
std::string level_str(LogLevel l) {
  switch (l) {
    case LogLevel::DEBUG: return "debug";
    case LogLevel::INFO: return "info";
    case LogLevel::WARN: return "warn";
    case LogLevel::ERROR: return "error";
  }
  return "info";
}
}  // namespace

std::string LogEvent::to_jsonl() const {
  char buf[512];
  std::snprintf(buf, sizeof(buf), "{\"ts\":\"%s\",\"level\":\"%s\",\"component\":\"%s\","
      "\"event\":\"%s\",\"message\":\"%s\",\"seq\":%llu",
      ts_utc.c_str(), level_str(level).c_str(), component.c_str(), event.c_str(),
      message.c_str(), (unsigned long long)seq);
  std::string out = buf;
  if (!node_id.empty()) out += ",\"node_id\":\"" + node_id + "\"";
  if (!run_id.empty()) out += ",\"run_id\":\"" + run_id + "\"";
  if (progress >= 0.0) {
    char pbuf[32];
    std::snprintf(pbuf, sizeof(pbuf), ",\"progress\":%.3f", progress);
    out += pbuf;
  }
  if (wall_us) out += ",\"wall_us\":" + std::to_string(wall_us);
  out += "}";
  return out;
}

Logger::Logger(LogLevel min_level) : min_level_(min_level) {}

void Logger::log(LogLevel level, const std::string& component, const std::string& event,
                 const std::string& message, const std::string& node_id,
                 const std::string& run_id, double progress, uint64_t wall_us) {
  if (static_cast<uint8_t>(level) < static_cast<uint8_t>(min_level_)) return;
  LogEvent e;
  e.level = level;
  e.ts_utc = utc_now();
  e.component = component;
  e.event = event;
  e.message = message;
  e.seq = ++seq_;
  e.node_id = node_id;
  e.run_id = run_id;
  e.progress = progress;
  e.wall_us = wall_us;
  for (auto& sink : sinks_) sink->emit(e);
  ++emitted_;
}

void MetricsAggregator::record(const std::string& name, uint64_t value) {
  std::lock_guard<std::mutex> lk(mtx_);
  auto& a = agg_[name];
  ++a.count_;
  a.sum_ += value;
  if (value > a.max_) a.max_ = value;
}

uint64_t MetricsAggregator::count(const std::string& name) const {
  std::lock_guard<std::mutex> lk(mtx_);
  auto it = agg_.find(name);
  return it == agg_.end() ? 0 : it->second.count_;
}

uint64_t MetricsAggregator::sum(const std::string& name) const {
  std::lock_guard<std::mutex> lk(mtx_);
  auto it = agg_.find(name);
  return it == agg_.end() ? 0 : it->second.sum_;
}

uint64_t MetricsAggregator::max(const std::string& name) const {
  std::lock_guard<std::mutex> lk(mtx_);
  auto it = agg_.find(name);
  return it == agg_.end() ? 0 : it->second.max_;
}

std::vector<std::string> MetricsAggregator::names() const {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<std::string> out;
  for (const auto& [k, v] : agg_) out.push_back(k);
  return out;
}

std::string MetricsAggregator::export_jsonl() const {
  std::lock_guard<std::mutex> lk(mtx_);
  std::string out;
  for (const auto& [k, a] : agg_) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "{\"metric\":\"%s\",\"count\":%llu,\"sum\":%llu,\"max\":%llu}\n",
        k.c_str(), (unsigned long long)a.count_, (unsigned long long)a.sum_,
        (unsigned long long)a.max_);
    out += buf;
  }
  return out;
}

}  // namespace astrocs::core
