// lib/infrastructure/observability/probes/src/probe.cpp
// RELEASE-02 性能探针框架实现 (仅 ASTROCS_PROBES=ON 时参与编译)。
//
// 低开销策略:
//   - 记录路径: 线程本地 std::string 行缓冲 + 线程本地 count/gauge 聚合, 无锁;
//   - 每 N 行或 T 毫秒 flush 一次 (N/T 可由环境变量调优), flush 时才取互斥锁写文件;
//   - 线程退出自动 flush; 进程 atexit 兜底 flush 主线程;
//   - 未设置 ASTROCS_PROBE_LOG 时 enabled()==false, 所有入口立即返回。
//
// 本文件只观测, 不改变任何科学数值或控制流。
#include "astrocs/probe.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 探针 sink 的追加写
// 经 aio 唯一实现 (aio_atomic::append_open/append_write/append_flush), 本 TU 不
// 自持 FILE* 通道。
#include "aio_atomic_file.h"

namespace astrocs {
namespace probe {
namespace {

using Clock = std::chrono::steady_clock;
using Millis = std::chrono::milliseconds;

// ── 运行期配置 (进程内只读一次) ─────────────────────────────────────────────
struct RuntimeConfig {
  const char* path = nullptr;
  std::size_t flush_lines = 1024;
  long long flush_ms = 1000;
};

const RuntimeConfig& config() noexcept {
  static const RuntimeConfig cfg = [] {
    RuntimeConfig c;
    const char* p = std::getenv("ASTROCS_PROBE_LOG");
    c.path = (p != nullptr && p[0] != '\0') ? p : nullptr;
    if (const char* n = std::getenv("ASTROCS_PROBE_FLUSH_LINES")) {
      const long v = std::strtol(n, nullptr, 10);
      if (v > 0) c.flush_lines = static_cast<std::size_t>(v);
    }
    if (const char* m = std::getenv("ASTROCS_PROBE_FLUSH_MS")) {
      const long v = std::strtol(m, nullptr, 10);
      if (v > 0) c.flush_ms = static_cast<long long>(v);
    }
    return c;
  }();
  return cfg;
}

// ── 文件 sink (故意泄漏: 线程退出期 TLS 析构仍需其存活) ─────────────────────
struct Sink {
  std::mutex mu;
  aio_atomic::AppendSink* file = nullptr;
  bool tried = false;
};

Sink& sink() noexcept {
  static Sink* s = new Sink();
  return *s;
}

void sink_write(const char* data, std::size_t n) noexcept {
  if (n == 0) return;
  Sink& s = sink();
  std::lock_guard<std::mutex> lock(s.mu);
  if (!s.tried) {
    s.tried = true;
    const char* p = config().path;
    if (p != nullptr) s.file = aio_atomic::append_open(p, nullptr);
  }
  if (s.file == nullptr) return;
  (void)aio_atomic::append_write(s.file, data, n);
  (void)aio_atomic::append_flush(s.file);
}

// ── 文本工具 ────────────────────────────────────────────────────────────────
void append_escaped_n(std::string& out, const char* s, std::size_t n) {
  out.push_back('"');
  for (std::size_t i = 0; i < n; ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
}

void append_escaped(std::string& out, const char* s) {
  if (s == nullptr) {
    out += "null";
    return;
  }
  append_escaped_n(out, s, std::strlen(s));
}

void append_ts(std::string& out) {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const std::time_t t = system_clock::to_time_t(now);
  const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[80];
  std::snprintf(buf, sizeof(buf), "\"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ\"",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec,
                static_cast<int>(ms < 0 ? ms + 1000 : ms));
  out += buf;
}

void append_double(std::string& out, double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  out += buf;
}

std::atomic<unsigned long long> g_next_tid{1};

// ── 线程本地缓冲/聚合 ───────────────────────────────────────────────────────
struct GaugeAgg {
  double last = 0.0;
  double min = 0.0;
  double max = 0.0;
  long long n = 0;
};

struct Tls {
  std::string buf;
  std::size_t lines = 0;
  Clock::time_point last_flush = Clock::now();
  std::unordered_map<std::string, long long> counts;
  std::unordered_map<std::string, GaugeAgg> gauges;
  unsigned long long tid = 0;

  ~Tls() { flush_self(); }

  void append_aggregates() {
    for (const auto& kv : counts) {
      const std::size_t sep = kv.first.find('\x1f');
      const std::string scope = kv.first.substr(0, sep);
      const std::string name =
          (sep == std::string::npos) ? std::string() : kv.first.substr(sep + 1);
      buf += "{\"ts_utc\":";
      append_ts(buf);
      buf += ",\"thread_id\":";
      buf += std::to_string(tid);
      buf += ",\"scope\":";
      append_escaped(buf, scope.c_str());
      buf += ",\"name\":";
      append_escaped(buf, name.c_str());
      buf += ",\"wall_us\":0,\"count\":";
      buf += std::to_string(kv.second);
      buf += "}\n";
    }
    for (const auto& kv : gauges) {
      const std::size_t sep = kv.first.find('\x1f');
      const std::string scope = kv.first.substr(0, sep);
      const std::string name =
          (sep == std::string::npos) ? std::string() : kv.first.substr(sep + 1);
      const GaugeAgg& a = kv.second;
      buf += "{\"ts_utc\":";
      append_ts(buf);
      buf += ",\"thread_id\":";
      buf += std::to_string(tid);
      buf += ",\"scope\":";
      append_escaped(buf, scope.c_str());
      buf += ",\"name\":";
      append_escaped(buf, name.c_str());
      buf += ",\"wall_us\":0,\"last\":";
      append_double(buf, a.last);
      buf += ",\"min\":";
      append_double(buf, a.min);
      buf += ",\"max\":";
      append_double(buf, a.max);
      buf += ",\"n\":";
      buf += std::to_string(a.n);
      buf += "}\n";
    }
  }

  void flush_self() noexcept {
    if (!buf.empty() || !counts.empty() || !gauges.empty()) {
      append_aggregates();
      if (!buf.empty()) sink_write(buf.data(), buf.size());
      buf.clear();
    }
    counts.clear();
    gauges.clear();
    lines = 0;
    last_flush = Clock::now();
  }

  void note_line() {
    ++lines;
    const RuntimeConfig& c = config();
    if (lines >= c.flush_lines ||
        std::chrono::duration_cast<Millis>(Clock::now() - last_flush).count() >=
            c.flush_ms) {
      flush_self();
    }
  }
};

Tls& tls();

// atexit 兜底: 主线程 thread_local 析构时序在不同实现下与 atexit 交错,
// 这里再显式 flush 一次; sink 与 config 均为泄漏/平凡单例, 此处安全。
void flush_at_exit() { tls().flush_self(); }

Tls& tls() {
  static thread_local Tls t;
  static const bool atexit_registered = [] {
    std::atexit(&flush_at_exit);
    return true;
  }();
  (void)atexit_registered;
  if (t.tid == 0) t.tid = g_next_tid.fetch_add(1, std::memory_order_relaxed);
  return t;
}

std::string make_key(const char* scope, const char* name) {
  std::string key;
  const char* s = (scope != nullptr) ? scope : "";
  const char* n = (name != nullptr) ? name : "";
  key.reserve(std::strlen(s) + std::strlen(n) + 1);
  key += s;
  key.push_back('\x1f');
  key += n;
  return key;
}

}  // namespace

// ── 公共接口 ────────────────────────────────────────────────────────────────
const char* log_path() noexcept { return config().path; }

bool enabled() noexcept { return config().path != nullptr; }

ScopeTimer::ScopeTimer(const char* scope, const char* name) noexcept
    : scope_(scope != nullptr ? scope : ""), name_(name != nullptr ? name : "") {
  active_ = enabled();
  if (active_) start_ = Clock::now();
}

ScopeTimer::ScopeTimer(const char* qualified_name) noexcept {
  const char* q = (qualified_name != nullptr) ? qualified_name : "";
  const char* dot = std::strchr(q, '.');
  if (dot != nullptr && dot != q) {
    scope_ = q;
    scope_len_ = static_cast<std::size_t>(dot - q);
    name_ = dot + 1;
  } else {
    scope_ = q;
    name_ = q;
  }
  active_ = enabled();
  if (active_) start_ = Clock::now();
}

ScopeTimer::~ScopeTimer() { stop(); }

void ScopeTimer::stop() noexcept {
  if (!active_) return;
  active_ = false;
  const long long wall_us =
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start_)
          .count();
  Tls& t = tls();
  std::string& b = t.buf;
  b += "{\"ts_utc\":";
  append_ts(b);
  b += ",\"thread_id\":";
  b += std::to_string(t.tid);
  b += ",\"scope\":";
  if (scope_len_ != 0) {
    append_escaped_n(b, scope_, scope_len_);
  } else {
    append_escaped(b, scope_);
  }
  b += ",\"name\":";
  append_escaped(b, name_);
  b += ",\"wall_us\":";
  b += std::to_string(wall_us);
  for (std::size_t i = 0; i < n_tags_; ++i) {
    const Tag& tg = tags_[i];
    if (tg.key == nullptr) continue;
    b += ',';
    append_escaped(b, tg.key);
    b += ':';
    switch (tg.kind) {
      case TagKind::kInt: b += std::to_string(tg.i); break;
      case TagKind::kUint: b += std::to_string(tg.u); break;
      case TagKind::kDouble: append_double(b, tg.d); break;
      case TagKind::kString: append_escaped(b, tg.s.c_str()); break;
    }
  }
  if (tags_dropped_) b += ",\"_probe_tags_dropped\":true";
  b += "}\n";
  t.note_line();
}

ScopeTimer& ScopeTimer::tag(const char* key, const char* value) noexcept {
  if (!active_ || n_tags_ >= kMaxTags) {
    if (n_tags_ >= kMaxTags) tags_dropped_ = true;
    return *this;
  }
  tags_[n_tags_].key = key;
  tags_[n_tags_].kind = TagKind::kString;
  tags_[n_tags_].s = (value != nullptr) ? value : "";
  ++n_tags_;
  return *this;
}

void count_add(const char* scope, const char* name, long long delta) noexcept {
  if (!enabled()) return;
  Tls& t = tls();
  t.counts[make_key(scope, name)] += delta;
}

void gauge_set(const char* scope, const char* name, double value) noexcept {
  if (!enabled()) return;
  Tls& t = tls();
  const std::string key = make_key(scope, name);
  auto it = t.gauges.find(key);
  if (it == t.gauges.end()) {
    GaugeAgg a;
    a.last = value;
    a.min = value;
    a.max = value;
    a.n = 1;
    t.gauges.emplace(key, a);
  } else {
    GaugeAgg& a = it->second;
    a.last = value;
    if (value < a.min) a.min = value;
    if (value > a.max) a.max = value;
    ++a.n;
  }
}

void flush() noexcept { tls().flush_self(); }

}  // namespace probe
}  // namespace astrocs
