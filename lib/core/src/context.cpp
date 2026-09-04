// CORE-005 RunContext 实现（RT-003 线程安全）
// RT-006: 追加 trace 事件实现（TraceEvent 序列化/解析、TraceStore、replay）。
#include "astrocs/core/context.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <set>
#include <utility>

namespace astrocs::core {

using nlohmann::json;

// ── RT-006 线程本地观测（当前节点/provider） ──
namespace {
thread_local std::string tls_current_node;
thread_local std::string tls_provider;
}  // namespace

void trace_set_current_node(const std::string& node_id) { tls_current_node = node_id; }
const std::string& trace_current_node() { return tls_current_node; }
void trace_set_provider(const std::string& provider) { tls_provider = provider; }
const std::string& trace_provider() { return tls_provider; }

void RunContext::log(LogLevel level, const std::string& component,
                     const std::string& message) {
  const char* lv = level == LogLevel::DEBUG ? "DEBUG"
                  : level == LogLevel::INFO ? "INFO"
                  : level == LogLevel::WARN ? "WARN" : "ERROR";
  std::lock_guard<std::mutex> lock(mu_);
  log_entries_.push_back(std::string("[") + lv + "][" + component + "] " + message);
}

std::vector<std::string> RunContext::log_entries() const {
  std::lock_guard<std::mutex> lock(mu_);
  return log_entries_;
}

void RunContext::add_metric(const std::string& name, uint64_t value) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[name] = value;
}

void RunContext::record_tick(const Metrics& m) {
  std::lock_guard<std::mutex> lock(mu_);
  ticks_.push_back(m);
}

std::map<std::string, uint64_t> RunContext::metrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return metrics_;
}

std::vector<Metrics> RunContext::ticks() const {
  std::lock_guard<std::mutex> lock(mu_);
  return ticks_;
}

Result<void> RunContext::store_artifact(DataArtifactDescriptor desc) {
  std::string err;
  if (!desc.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "store_artifact: " + err));
  }
  std::lock_guard<std::mutex> lock(mu_);
  if (artifacts_.count(desc.id.id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "store_artifact: duplicate id " + desc.id.id));
  }
  artifacts_[desc.id.id] = std::move(desc);
  return Result<void>::success();
}

bool RunContext::get_artifact(const std::string& id,
                              DataArtifactDescriptor* out) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = artifacts_.find(id);
  if (it == artifacts_.end()) return false;
  if (out) *out = it->second;
  return true;
}

std::vector<std::string> RunContext::artifact_ids() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  out.reserve(artifacts_.size());
  for (const auto& [k, v] : artifacts_) out.push_back(k);
  return out;
}

void RunContext::mark_checkpoint(const std::string& node_id) {
  std::lock_guard<std::mutex> lock(mu_);
  checkpoints_.push_back(node_id);
  if (trace_store_) {
    TraceEvent e;
    e.type = TraceEventType::CHECKPOINT;
    const std::string& cur = trace_current_node();
    e.node_id = cur.empty() ? node_id : cur;
    e.run_id = run_id_;
    e.status = "CHECKPOINT";
    trace_store_->record(std::move(e));
  }
}

// ── RT-006: 真实运行 trace 事件记录（观测点填写；无汇则无操作） ──
void RunContext::record_trace(TraceEvent e) const {
  auto store = trace_store();
  if (!store) return;
  if (e.node_id.empty()) e.node_id = trace_current_node();
  if (e.run_id.empty()) e.run_id = run_id_;
  store->record(std::move(e));
}

std::vector<std::string> RunContext::checkpoints() const {
  std::lock_guard<std::mutex> lock(mu_);
  return checkpoints_;
}

// ── RT-001/RT-002: ThreadBudget 原子租约 ──
ThreadLease ThreadBudget::acquire(uint32_t min, uint32_t max,
                                  AcquirePolicy policy) noexcept {
  if (min == 0) min = 1;
  if (max == 0) max = budget_;

  auto try_take = [&]() -> ThreadLease {
    uint32_t cur = available_.load(std::memory_order_relaxed);
    for (;;) {
      if (cur < min) {
        if (policy == AcquirePolicy::BEST_EFFORT && cur > 0) {
          const uint32_t take = (max < cur) ? max : cur;
          if (available_.compare_exchange_weak(cur, cur - take,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
            return _make_lease(take);
          }
          continue;
        }
        return ThreadLease();  // 空租约（不满足最小值）
      }
      const uint32_t take = (max < cur) ? max : cur;
      if (available_.compare_exchange_weak(cur, cur - take,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
        return _make_lease(take);
      }
    }
  };

  if (policy != AcquirePolicy::BLOCK) {
    return try_take();
  }
  std::unique_lock<std::mutex> lock(cv_mutex_);
  for (;;) {
    ThreadLease lease = try_take();
    if (lease.acquired()) return lease;
    cv_.wait(lock, [&] { return available_.load(std::memory_order_relaxed) >= min; });
  }
}

ThreadLease ThreadBudget::_make_lease(uint32_t got) noexcept {
  return ThreadLease(got, [this, got]() noexcept {
    available_.fetch_add(got, std::memory_order_acq_rel);
    cv_.notify_all();
  });
}

Result<std::shared_ptr<ThreadBudget>> create_thread_budget(uint32_t budget) noexcept {
  if (budget == 0) {
    return Result<std::shared_ptr<ThreadBudget>>::fail(
        Error(ErrorDomain::RESOURCE, "create_thread_budget: budget must be > 0"));
  }
  auto b = std::make_shared<ThreadBudget>(budget);
  b->reset_available();
  return Result<std::shared_ptr<ThreadBudget>>::ok(std::move(b));
}

// ── RT-003: RunContext::acquire_lease 接真实 ThreadBudget 原子预留 ──
// 消灭伪授权：不再退回 ThreadLease::make。lease RAII 析构自动归还；
// 取消/异常路径经 ThreadLease 析构统一回收。
ThreadLease RunContext::acquire_lease(uint32_t requested) const {
  auto b = budget();
  if (!b) {
    // 未注入预算（非调度/测试构造上下文）→ 空租约，不伪造授权
    return ThreadLease();
  }
  if (requested == 0) requested = 1;
  const uint32_t cap = b->budget();
  const uint32_t want = requested < cap ? requested : cap;
  return b->acquire(1u, want, AcquirePolicy::NONBLOCK);
}

namespace {

std::string utc_now_ms() {
  // RFC3339 UTC（毫秒精度）：2026-09-03T13:56:51.123Z
  char buf[40];
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);   // MSVC: gmtime_r 不存在 (WIN-001)
#else
  gmtime_r(&t, &tm);
#endif
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count() %
                  1000;
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  char out[48];
  std::snprintf(out, sizeof(out), "%s.%03lldZ", buf, static_cast<long long>(ms));
  return out;
}

std::string jstr(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return std::string();
  return it->get<std::string>();
}

double jdbl(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number()) return 0.0;
  return it->get<double>();
}

uint64_t ju64(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number_unsigned()) return 0;
  return it->get<uint64_t>();
}

}  // namespace

// ── TraceEvent 序列化 ──
std::string TraceEvent::to_jsonl() const {
  json j;
  j["schema"] = "astrocs.trace-event/v1";
  j["type"] = trace_event_type_name(type);
  j["ts_utc"] = ts_utc;
  j["run_id"] = run_id;
  j["node_id"] = node_id;
  j["seq"] = seq;
  if (!module_id.empty()) j["module_id"] = module_id;
  if (!module_version.empty()) j["module_version"] = module_version;
  if (!dll_name.empty()) j["dll_name"] = dll_name;
  if (!dll_sha256.empty()) j["dll_sha256"] = dll_sha256;
  if (!build_id.empty()) j["build_id"] = build_id;
  if (!entry.empty()) j["entry"] = entry;
  if (call_count) j["call_count"] = call_count;
  if (workers) j["workers"] = workers;
  if (granted_workers) j["granted_workers"] = granted_workers;
  if (!provider.empty()) j["provider"] = provider;
  if (!kernel_id.empty()) j["kernel_id"] = kernel_id;
  if (!status.empty()) j["status"] = status;
  if (!error.empty()) j["error"] = error;
  if (!error_domain.empty()) j["error_domain"] = error_domain;
  if (!artifact_id.empty()) j["artifact_id"] = artifact_id;
  if (!artifact_sha256.empty()) j["artifact_sha256"] = artifact_sha256;
  if (artifact_size) j["artifact_size"] = artifact_size;
  if (cpu_ms > 0.0) j["cpu_ms"] = cpu_ms;
  if (wall_ms > 0.0) j["wall_ms"] = wall_ms;
  return j.dump();
}

bool TraceEvent::from_jsonl(const std::string& line, TraceEvent* out) {
  if (!out) return false;
  json j;
  try {
    j = json::parse(line);
  } catch (const std::exception&) {
    return false;
  }
  if (!j.is_object()) return false;
  const std::string ty = jstr(j, "type");
  if (ty.empty()) return false;
  // type 名 → 枚举（unknown → false：非法事件类型拒绝）
  TraceEventType t = TraceEventType::MODULE_CALL;
  bool known = false;
  for (int i = 0; i <= static_cast<int>(TraceEventType::ERROR); ++i) {
    if (ty == trace_event_type_name(static_cast<TraceEventType>(i))) {
      t = static_cast<TraceEventType>(i);
      known = true;
      break;
    }
  }
  if (!known) return false;
  TraceEvent e;
  e.type = t;
  e.ts_utc = jstr(j, "ts_utc");
  e.run_id = jstr(j, "run_id");
  e.node_id = jstr(j, "node_id");
  e.module_id = jstr(j, "module_id");
  e.module_version = jstr(j, "module_version");
  e.dll_name = jstr(j, "dll_name");
  e.dll_sha256 = jstr(j, "dll_sha256");
  e.build_id = jstr(j, "build_id");
  e.entry = jstr(j, "entry");
  e.call_count = ju64(j, "call_count");
  e.workers = static_cast<uint32_t>(ju64(j, "workers"));
  e.granted_workers = static_cast<uint32_t>(ju64(j, "granted_workers"));
  e.provider = jstr(j, "provider");
  e.kernel_id = jstr(j, "kernel_id");
  e.status = jstr(j, "status");
  e.error = jstr(j, "error");
  e.error_domain = jstr(j, "error_domain");
  e.artifact_id = jstr(j, "artifact_id");
  e.artifact_sha256 = jstr(j, "artifact_sha256");
  e.artifact_size = ju64(j, "artifact_size");
  e.cpu_ms = jdbl(j, "cpu_ms");
  e.wall_ms = jdbl(j, "wall_ms");
  e.seq = ju64(j, "seq");
  *out = std::move(e);
  return true;
}

bool TraceEvent::same_call_site(const TraceEvent& o) const noexcept {
  // 同一次调用位点：run + node + module + entry（隐藏 session/重复检测判定基础）
  return type == TraceEventType::MODULE_CALL && o.type == TraceEventType::MODULE_CALL &&
         run_id == o.run_id && node_id == o.node_id && module_id == o.module_id &&
         entry == o.entry;
}

// ── TraceStore ──
uint64_t TraceStore::record(TraceEvent e) {
  std::lock_guard<std::mutex> lock(mu_);
  if (e.ts_utc.empty()) e.ts_utc = utc_now_ms();
  e.seq = ++seq_;
  events_.push_back(std::move(e));
  return events_.back().seq;
}

std::vector<TraceEvent> TraceStore::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_;  // push_back 顺序 = seq 升序（单锁串行）
}

std::string TraceStore::export_jsonl() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::string out;
  out.reserve(events_.size() * 256u);
  for (const auto& e : events_) {
    out += e.to_jsonl();
    out += '\n';
  }
  return out;
}

void TraceStore::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  events_.clear();
  seq_ = 0;
}

std::vector<std::string> TraceStore::detect_repeated_calls() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> violations;
  // (a) 隐藏 session 扇出：同一 entry 出现在 ≥2 个不同 node 的 MODULE_CALL。
  std::map<std::string, std::set<std::string>> entry_nodes;   // entry -> nodes
  std::map<std::string, uint64_t> node_calls;                 // node -> module_call 计数
  for (const auto& e : events_) {
    if (e.type != TraceEventType::MODULE_CALL) continue;
    if (!e.entry.empty()) entry_nodes[e.entry].insert(e.node_id);
    node_calls[e.node_id]++;
  }
  for (const auto& [entry, nodes] : entry_nodes) {
    if (nodes.size() >= 2) {
      std::string nl;
      for (const auto& n : nodes) {
        if (!nl.empty()) nl += ",";
        nl += n;
      }
      violations.push_back("hidden-session-fanout: entry '" + entry +
                           "' observed at " + std::to_string(nodes.size()) +
                           " nodes [" + nl + "]");
    }
  }
  // (b) 重复调用：同一 node 的 MODULE_CALL 计数 > 1（7 个 P2 节点各一次契约）。
  for (const auto& [node, cnt] : node_calls) {
    if (cnt > 1) {
      violations.push_back("repeated-call: node '" + node + "' module_call count=" +
                           std::to_string(cnt));
    }
  }
  return violations;
}

// ── JSONL 可重放摘要 ──
TraceReplayResult trace_replay_from_jsonl(const std::string& jsonl) {
  TraceReplayResult r;
  if (jsonl.empty()) return r;  // ok=true, 0 行（合法空）
  std::map<std::string, TraceNodeReplay> by_node;
  std::map<std::string, std::set<std::string>> node_artifacts;
  std::string cur;
  cur.reserve(512);
  for (char ch : jsonl) {
    if (ch == '\n') {
      if (!cur.empty() && cur.back() == '\r') cur.pop_back();
      TraceEvent e;
      if (TraceEvent::from_jsonl(cur, &e)) {
        r.parsed_lines++;
        auto& node = by_node[e.node_id];
        if (node.node_id.empty()) {
          node.node_id = e.node_id;
          node.first_seq = e.seq;
        }
        switch (e.type) {
          case TraceEventType::MODULE_CALL:
            if (node.module_id.empty()) node.module_id = e.module_id;
            if (node.entry.empty()) node.entry = e.entry;
            node.call_count++;
            break;
          case TraceEventType::NODE_END:
            node.status = e.status;
            if (e.wall_ms > 0.0) node.wall_ms = e.wall_ms;
            break;
          case TraceEventType::NODE_START:
            if (node.status.empty()) node.status = "RUNNING";
            break;
          default:
            break;
        }
        if (!e.provider.empty()) node.provider = e.provider;
        if (!e.artifact_id.empty()) node_artifacts[e.node_id].insert(e.artifact_id);
        if (e.type == TraceEventType::ERROR && node.status.empty()) {
          node.status = "ERROR";
        }
      } else {
        r.skipped_lines++;
      }
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  // 末行无换行
  if (!cur.empty()) {
    TraceEvent e;
    if (TraceEvent::from_jsonl(cur, &e)) {
      r.parsed_lines++;
      auto& node = by_node[e.node_id];
      if (node.node_id.empty()) { node.node_id = e.node_id; node.first_seq = e.seq; }
      switch (e.type) {
        case TraceEventType::MODULE_CALL:
          if (node.module_id.empty()) node.module_id = e.module_id;
          if (node.entry.empty()) node.entry = e.entry;
          node.call_count++;
          break;
        case TraceEventType::NODE_END:
          node.status = e.status;
          if (e.wall_ms > 0.0) node.wall_ms = e.wall_ms;
          break;
        case TraceEventType::NODE_START:
          if (node.status.empty()) node.status = "RUNNING";
          break;
        default: break;
      }
      if (!e.provider.empty()) node.provider = e.provider;
      if (!e.artifact_id.empty()) node_artifacts[e.node_id].insert(e.artifact_id);
      if (e.type == TraceEventType::ERROR && node.status.empty()) node.status = "ERROR";
    } else {
      r.skipped_lines++;
    }
  }
  for (auto& [nid, node] : by_node) {
    (void)nid;
    for (const auto& a : node_artifacts[node.node_id]) node.artifact_ids.push_back(a);
  }
  for (auto& [nid, node] : by_node) r.nodes.push_back(std::move(node));
  std::sort(r.nodes.begin(), r.nodes.end(),
            [](const TraceNodeReplay& a, const TraceNodeReplay& b) {
              return a.node_id < b.node_id;
            });
  r.ok = r.parsed_lines > 0 || jsonl.empty();
  return r;
}

std::string trace_replay_nodes_json(const std::string& jsonl) {
  TraceReplayResult r = trace_replay_from_jsonl(jsonl);
  json j;
  j["replay_schema"] = "astrocs.trace-replay/v1";
  j["parsed_lines"] = r.parsed_lines;
  j["skipped_lines"] = r.skipped_lines;
  j["nodes"] = json::array();
  for (const auto& n : r.nodes) {
    json nj;
    nj["node_id"] = n.node_id;
    nj["status"] = n.status;
    if (!n.module_id.empty()) nj["module_id"] = n.module_id;
    if (!n.entry.empty()) nj["entry"] = n.entry;
    nj["call_count"] = n.call_count;
    if (!n.provider.empty()) nj["provider"] = n.provider;
    nj["artifact_ids"] = n.artifact_ids;
    if (n.wall_ms > 0.0) nj["wall_ms"] = n.wall_ms;
    j["nodes"].push_back(std::move(nj));
  }
  return j.dump();
}

Result<std::shared_ptr<TraceStore>> create_trace_store() noexcept {
  return Result<std::shared_ptr<TraceStore>>::ok(std::make_shared<TraceStore>());
}


}  // namespace astrocs::core
