// ACSD Core — ARCH-502 normalize 异步工作流调度器实现
// 依据：ASTROCS_DESIGN.md §8.3；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md
#include "astrocs/core/normalize_workflow.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <sstream>
#include <thread>
#include <vector>
#include "aio_atomic_file.h"   // aio 唯一 I/O：原子文本落盘（CLEAN-403）

namespace astrocs::core {
namespace {

double now_seconds() {
  using clock = std::chrono::steady_clock;
  static const clock::time_point t0 = clock::now();
  return std::chrono::duration<double>(clock::now() - t0).count();
}

// 确定性校验和：FNV-1a over (name, bytes)（与 worker 数、执行顺序无关）
std::uint64_t fnv1a(std::uint64_t h, const std::uint8_t* p, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<std::uint64_t>(p[i]);
    h *= 1099511628211ULL;
  }
  return h;
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
    else if (c == '\n') out += "\\n";
    else out.push_back(c);
  }
  return out;
}

}  // namespace

std::string prefetch_block_name(const std::string& key) {
  std::string n = "prefetch_";
  for (char c : key) {
    const bool okc = (c >= 97 && c <= 122) || (c >= 48 && c <= 57) || c == 95;
    n.push_back(okc ? c : 95);
  }
  return n;
}

const char* probe_kind_name(ProbeKind k) {
  switch (k) {
    case ProbeKind::NODE_WALL: return "node_wall";
    case ProbeKind::QUEUE_WAIT: return "queue_wait";
    case ProbeKind::BLOCK_BIRTH: return "block_birth";
    case ProbeKind::BLOCK_DEATH: return "block_death";
    case ProbeKind::RSS: return "rss";
    case ProbeKind::IO: return "io";
    case ProbeKind::WORKER_BUSY: return "worker_busy";
    case ProbeKind::CACHE_HIT: return "cache_hit";
  }
  return "?";
}

// ── ProbeSink ──────────────────────────────────────────────────────────────
ProbeSink::ProbeSink(std::string path) : path_(std::move(path)) {}
ProbeSink::~ProbeSink() { flush(); }

void ProbeSink::emit(const ProbeEvent& ev) {
  std::lock_guard<std::mutex> lk(mu_);
  events_.push_back(ev);
}

std::size_t ProbeSink::count(ProbeKind k) const {
  std::lock_guard<std::mutex> lk(mu_);
  std::size_t n = 0;
  for (const auto& e : events_) if (e.kind == k) ++n;
  return n;
}

std::size_t ProbeSink::count_prefetch_phase(int phase) const {
  std::lock_guard<std::mutex> lk(mu_);
  std::size_t n = 0;
  for (const auto& e : events_)
    if (e.kind == ProbeKind::CACHE_HIT && e.prefetch_phase == phase) ++n;
  return n;
}

double ProbeSink::sum_value(ProbeKind k) const {
  std::lock_guard<std::mutex> lk(mu_);
  double s = 0.0;
  for (const auto& e : events_) if (e.kind == k) s += e.value;
  return s;
}

bool ProbeSink::flush() {
  if (path_.empty()) return true;
  std::string blob;
  {
    std::lock_guard<std::mutex> lk(mu_);
    std::ostringstream f;
    for (const auto& e : events_) {
      f << "{\"ts\":" << e.ts << ",\"stage\":\"" << json_escape(e.stage) << "\",\"kind\":\""
        << probe_kind_name(e.kind) << "\"";
      if (!e.node.empty()) f << ",\"node\":\"" << json_escape(e.node) << "\"";
      if (!e.block.empty()) f << ",\"block\":\"" << json_escape(e.block) << "\"";
      f << ",\"value\":" << e.value << ",\"unit\":\"" << e.unit << "\"";
      if (e.bytes) f << ",\"bytes\":" << e.bytes;
      if (e.frame_id) f << ",\"frame_id\":" << e.frame_id;
      if (e.window_id) f << ",\"window_id\":" << e.window_id;
      if (e.worker >= 0) f << ",\"worker\":" << e.worker;
      f << "}\n";
    }
    blob = f.str();
  }
  // CLEAN-403：机制经 aio 唯一实现（aio_atomic::write_file_atomic），本 TU 不自持 ofstream 通道。
  return aio_atomic::write_file_atomic(path_, blob, nullptr) == 0;
}

// ── PrefetchCache ──────────────────────────────────────────────────────────
PrefetchCache::PrefetchCache(Loader l2) : l2_(std::move(l2)) {}

bool PrefetchCache::try_get(const std::string& key, std::string* value) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = l1_.find(key);
  if (it == l1_.end()) return false;
  if (value) *value = it->second;
  return true;
}

std::string PrefetchCache::get_or_load(const std::string& key) {
  {
    std::unique_lock<std::mutex> lk(mu_);
    auto it = l1_.find(key);
    if (it != l1_.end()) {
      ++l1_hits_;
      return it->second;
    }
    if (in_flight_[key]) {
      // single-flight：等同一 key 的加载者完成，再走 L1 命中
      cv_.wait(lk, [&] { return l1_.count(key) != 0 || !in_flight_[key]; });
      auto it2 = l1_.find(key);
      if (it2 != l1_.end()) { ++l1_hits_; return it2->second; }
      ++misses_;
      return std::string();
    }
    in_flight_[key] = true;
    ++misses_;
  }
  std::string value;
  bool from_l2 = false;
  if (l2_) {
    value = l2_(key);
    from_l2 = !value.empty();
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    ++loads_;
    if (from_l2) ++l2_hits_;
    l1_[key] = value;
    in_flight_[key] = false;
  }
  cv_.notify_all();
  return value;
}

void PrefetchCache::set_loader(Loader l2) {
  std::lock_guard<std::mutex> lk(mu_);
  l2_ = std::move(l2);
}

void PrefetchCache::put(const std::string& key, std::string value) {
  std::lock_guard<std::mutex> lk(mu_);
  l1_[key] = std::move(value);
}

std::size_t PrefetchCache::size() const {
  std::lock_guard<std::mutex> lk(mu_);
  return l1_.size();
}

// ── NormalizeWorkflowScheduler ─────────────────────────────────────────────
NormalizeWorkflowScheduler::NormalizeWorkflowScheduler(NormalizeWorkflowConfig cfg)
    : cfg_(std::move(cfg)), probes_(cfg_.probe_path) {
  // 合同下界：worker 数由配置注入（ThreadBudget），此处仅做下界保护，非默认值。
  constexpr int kMinWorkers = 1;
  cfg_.workers = std::max(cfg_.workers, kMinWorkers);
}

NormalizeWorkflowScheduler::~NormalizeWorkflowScheduler() {
  // 池是 run() 的局部量（run 返回前已全部 join 回收），对象析构只需置取消唤醒在途 worker。
  cancel();
}

void NormalizeWorkflowScheduler::add_frame(NormalizeFrame frame) {
  frames_.push_back(std::move(frame));
}

void NormalizeWorkflowScheduler::set_prefetch_loader(PrefetchCache::Loader loader) {
  cache_.set_loader(std::move(loader));
}

void NormalizeWorkflowScheduler::cancel() {
  {
    std::lock_guard<std::mutex> lk(mu_);
    cancel_.store(true);
  }
  cv_.notify_all();
  cv_done_.notify_all();
}

FrameOutcome NormalizeWorkflowScheduler::run_frame(const NormalizeFrame& f, int worker_index) {
  FrameOutcome out;
  out.frame_id = f.frame_id;
  const double t_start = now_seconds();

  BlockFrame frame;
  std::uint64_t h = 1469598103934665603ULL;   // FNV offset basis

  // 预取键：空闲时已可能命中 L1；此处取或加载（结果以块形式入帧）。
  // 块的 consumers 必须来自帧内 DAG 的声明（consumes），否则引用计数为 0、无人能消费它，
  // 帧结束会留下无人消费的块（bytes_leaked > 0）——这正是本合同要抓的良构性问题。
  std::map<std::string, std::vector<std::string>> declared_consumers;
  for (const NormalizeNode& n : f.nodes)
    for (const std::string& blk : n.consumes) declared_consumers[blk].push_back(n.id);
  for (const std::string& key : f.prefetch_keys) {
    if (cancel_.load()) break;
    const std::string v = cache_.get_or_load(key);
    BlockMeta meta;
    meta.name = prefetch_block_name(key);
        meta.dtype = BlockDtype::U8;
    meta.unit.clear();
    meta.optional = true;
    meta.producer = "prefetch";
    meta.lifecycle = BlockLifecycle::SHORT;
    {
      auto it = declared_consumers.find(meta.name);
      if (it != declared_consumers.end()) meta.consumers = it->second;
    }
    Block* b = frame.create(meta, v.size());
    if (b) {
      std::uint8_t* p = b->raw();
      for (std::size_t i = 0; i < v.size(); ++i) p[i] = static_cast<std::uint8_t>(v[i]);
      b->set_provenance("prefetch_key", key);
    }
  }

  // 帧内 DAG 流水：节点按声明顺序执行（依赖已由 BlockDagValidator 在构建期校验）
  for (const NormalizeNode& n : f.nodes) {
    if (cancel_.load()) {
      out.error = "cancelled";
      break;
    }
    const double t0 = now_seconds();
    const std::size_t before = frame.bytes_alive();
    bool ok = false;
    try {
      ok = n.run ? n.run(frame) : false;
    } catch (...) {
      ok = false;
    }
    const double t1 = now_seconds();

    ProbeEvent ev;
    ev.ts = t1;
    ev.kind = ProbeKind::NODE_WALL;
    ev.node = n.id;
    ev.value = t1 - t0;
    ev.unit = "s";
    ev.frame_id = f.frame_id;
    ev.worker = worker_index;
    probes_.emit(ev);

    const std::size_t after = frame.bytes_alive();
    if (after > before) {
      ProbeEvent be;
      be.ts = t1;
      be.kind = ProbeKind::BLOCK_BIRTH;
      be.block = n.id;
      be.bytes = static_cast<std::int64_t>(after - before);
      be.value = static_cast<double>(after - before);
      be.unit = "B";
      be.frame_id = f.frame_id;
      probes_.emit(be);
    }
    // 生命周期由调度器掌管：节点声明的消费在此统一执行
    for (const std::string& blk : n.consumes) {
      const std::size_t b0 = frame.bytes_alive();
      frame.consume(blk, n.id);
      const std::size_t b1 = frame.bytes_alive();
      if (b1 < b0) {
        ProbeEvent de;
        de.ts = now_seconds();
        de.kind = ProbeKind::BLOCK_DEATH;
        de.block = blk;
        de.bytes = static_cast<std::int64_t>(b0 - b1);
        de.value = static_cast<double>(b0 - b1);
        de.unit = "B";
        de.frame_id = f.frame_id;
        probes_.emit(de);
      }
    }

    const std::size_t cur = frame.bytes_alive();
    std::size_t prev = peak_inflight_.load();
    while (cur > prev && !peak_inflight_.compare_exchange_weak(prev, cur)) {}
    if (cur > out.peak_bytes) out.peak_bytes = cur;
    if (cfg_.memory_limit_bytes > 0 && cur > cfg_.memory_limit_bytes) {
      out.ok = false;
      out.error = "memory_limit_exceeded";
      break;
    }
    if (!ok) {
      out.ok = false;
      if (out.error.empty()) out.error = "node_failed:" + n.id;
      break;
    }
  }

  // 确定性校验和：按块名升序（与创建/销毁顺序无关）
  {
    std::vector<std::string> names;
    for (const char* nmc : {"raw", "calibrated", "photometric", "snr"}) {
      const std::string nm(nmc);
      const Block* b = frame.find(nm);
      if (b) names.push_back(nm);
    }
    for (const std::string& nm : names) {
      const Block* b = frame.find(nm);
      h = fnv1a(h, reinterpret_cast<const std::uint8_t*>(nm.data()), nm.size());
      if (b) h = fnv1a(h, b->raw(), b->bytes());
    }
  }

  out.bytes_leaked = frame.bytes_alive();   // 销毁前仍存活量（良构帧为 0）
  frame.destroy_all();                      // 阶段结束不得残留块（强制回收）
  out.wall_seconds = now_seconds() - t_start;
  if (out.error.empty()) out.ok = true;
  else if (out.error == "cancelled") out.ok = false;
  out.checksum = h;
  return out;
}

void NormalizeWorkflowScheduler::emit_prefetch_probe(std::size_t idx, int phase, int worker) {
  const NormalizeFrame& f = frames_[idx];
  std::size_t hits = 0;
  for (const std::string& key : f.prefetch_keys) {
    if (cancel_.load()) break;
    const bool was = cache_.try_get(key, nullptr);
    cache_.get_or_load(key);
    if (was) ++hits;
  }
  ProbeEvent ce;
  ce.ts = now_seconds();
  ce.kind = ProbeKind::CACHE_HIT;
  ce.value = f.prefetch_keys.empty()
                 ? 0.0
                 : static_cast<double>(hits) / static_cast<double>(f.prefetch_keys.size());
  ce.unit = "1";
  ce.frame_id = f.frame_id;
  ce.worker = worker;
  ce.block = "prefetch";
  ce.prefetch_phase = phase;
  probes_.emit(ce);
}

void NormalizeWorkflowScheduler::prefetch_loop(int thread_index) {
  for (;;) {
    if (cancel_.load()) return;
    const std::size_t k = prefetch_cursor_.fetch_add(1);
    if (k >= frames_.size()) return;
    {
      std::lock_guard<std::mutex> lk(mu_);
      if (completed_ >= frames_.size()) return;
    }
    if (k >= probe_claimed_.size() || !probe_claimed_[k]) continue;
    if (probe_claimed_[k]->exchange(1) != 0) continue;   // 已被 worker 抢先
    emit_prefetch_probe(k, 1, -1 - thread_index);
  }
}

void NormalizeWorkflowScheduler::worker_loop(int worker_index) {
  for (;;) {
    std::size_t idx = 0;
    bool need_prefetch = false;
    double waited = 0.0;
    {
      std::unique_lock<std::mutex> lk(mu_);
      const double t0 = now_seconds();
      cv_.wait(lk, [&] { return cancel_.load() || !pending_.empty() || completed_ >= frames_.size(); });
      waited = now_seconds() - t0;
      if (cancel_.load() && pending_.empty()) return;
      if (pending_.empty()) {
        if (completed_ >= frames_.size()) return;
        // 空闲 worker：为尚未开始的帧预取（I/O 与其它 worker 的计算重叠）
        need_prefetch = true;
      } else {
        idx = pending_.front();
        pending_.pop_front();
      }
    }
    if (need_prefetch) {
      // 空闲窗口：交给专用预取线程推进（此处只让出时间片，避免忙等）
      std::this_thread::yield();
      continue;
    }
    busy_workers_.fetch_add(1);
    ProbeEvent qe;
    qe.ts = now_seconds();
    qe.kind = ProbeKind::QUEUE_WAIT;
    qe.value = waited;
    qe.unit = "s";
    qe.worker = worker_index;
    qe.frame_id = frames_[idx].frame_id;
    probes_.emit(qe);

    // 兜底预取：仅当该帧未被空闲期预取覆盖时才加载并计数（保证每 (帧,键) 恰好一条 cache_hit 探针）
    // 兜底：与预取线程竞争同一"探针归属位"——谁先谁发唯一一条 cache_hit（phase 2 = 关键路径）
    if (idx < probe_claimed_.size() && probe_claimed_[idx] &&
        probe_claimed_[idx]->exchange(1) == 0) {
      emit_prefetch_probe(idx, 2, worker_index);
    } else {
      // 已由预取线程认领：仍要保证键在 L1（命中即返回，不重复加载）
      for (const std::string& key : frames_[idx].prefetch_keys) {
        if (cancel_.load()) break;
        cache_.get_or_load(key);
      }
    }

    FrameOutcome out = run_frame(frames_[idx], worker_index);
    busy_workers_.fetch_sub(1);
    {
      std::lock_guard<std::mutex> lk(mu_);
      outcomes_[idx] = std::move(out);
      ++completed_;
    }
    cv_done_.notify_all();
    cv_.notify_all();
  }
}

std::vector<FrameOutcome> NormalizeWorkflowScheduler::run() {
  outcomes_.assign(frames_.size(), FrameOutcome{});
  for (std::size_t i = 0; i < frames_.size(); ++i) outcomes_[i].frame_id = frames_[i].frame_id;
  {
    std::lock_guard<std::mutex> lk(mu_);
    pending_.clear();
    for (std::size_t i = 0; i < frames_.size(); ++i) pending_.push_back(i);
    completed_ = 0;
    prefetch_cursor_.store(0);
    prefetch_done_.store(0);
    probe_claimed_.clear();
    for (std::size_t i = 0; i < frames_.size(); ++i)
      probe_claimed_.push_back(std::unique_ptr<std::atomic<int>>(new std::atomic<int>(0)));
  }
  // 本次 run 的**有界 run 作用域池**（RT-004：调度器不持有永久池成员）：
  // 帧 worker 数 = cfg_.workers、预取线程数 = cfg_.prefetch_threads（均由配置/预算注入），
  // run 返回前全部 join 回收。
  const int n = std::max(1, cfg_.workers);
  std::vector<std::thread> pool;
  pool.reserve(static_cast<std::size_t>(n));
  std::vector<std::thread> prefetch_pool;
  if (cfg_.prefetch_enabled) {
    const int np = cfg_.prefetch_threads;
    prefetch_pool.reserve(static_cast<std::size_t>(np));
    for (int i = 0; i < np; ++i) prefetch_pool.emplace_back([this, i] { prefetch_loop(i); });
  }
  for (int i = 0; i < n; ++i) pool.emplace_back([this, i] { worker_loop(i); });
  {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [&] { return completed_ >= frames_.size() || cancel_.load(); });
  }
  for (auto& t : pool) if (t.joinable()) t.join();   // run 返回前全部回收（无 detach）
  pool.clear();
  for (auto& t : prefetch_pool) if (t.joinable()) t.join();
  prefetch_pool.clear();

  // 归约顺序冻结：按 frame_id 升序输出（与 worker 数无关）
  std::vector<FrameOutcome> out;
  out.reserve(outcomes_.size());
  for (const auto& o : outcomes_) out.push_back(o);
  std::stable_sort(out.begin(), out.end(),
                   [](const FrameOutcome& a, const FrameOutcome& b) { return a.frame_id < b.frame_id; });
  probes_.flush();
  return out;
}

}  // namespace astrocs::core
