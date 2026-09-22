// AstroCS Core — ARCH-503 mosaic 天球窗口并行调度器实现
// 依据：ASTROCS_DESIGN.md §8.3；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md
#include "astrocs/core/mosaic_window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>
#include "aio_atomic_file.h"   // CLEAN-403：aio 唯一 I/O 实现
#include <set>
#include <sstream>

namespace astrocs::core {
namespace {

std::string dirname_of(const std::string& p) {
  const std::size_t k = p.find_last_of('/');
  return (k == std::string::npos) ? std::string(".") : p.substr(0, k);
}

double now_seconds() {
  using clock = std::chrono::steady_clock;
  static const clock::time_point t0 = clock::now();
  return std::chrono::duration<double>(clock::now() - t0).count();
}

std::uint64_t fnv1a(std::uint64_t h, const void* p, std::size_t n) {
  const std::uint8_t* b = static_cast<const std::uint8_t*>(p);
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<std::uint64_t>(b[i]);
    h *= 1099511628211ULL;
  }
  return h;
}

// 窗口内像素的确定性合成：对覆盖该像素的各帧做 SNR² 加权（模拟 SNR² 集成），
// 帧按 frame_id 升序归约（顺序冻结）。SNR 由 tile 尺寸与像素位置确定性导出，
// 使本调度器的输出可复现且与窗口划分无关。
double integrate_pixel(const std::vector<const MosaicFrameInput*>& cover, std::uint64_t tile,
                       std::int64_t px) {
  double num = 0.0, den = 0.0;
  for (const MosaicFrameInput* f : cover) {
    const double tile_bytes = static_cast<double>(f->tile_bytes.at(tile));
    const double snr = 10.0 + std::fmod(tile_bytes, 97.0) + static_cast<double>(px % 13) * 0.5;
    const double value = 100.0 + std::fmod(tile_bytes + static_cast<double>(f->frame_id), 53.0);
    const double w = snr * snr;
    num += w * value;
    den += w;
  }
  return (den > 0.0) ? (num / den) : 0.0;
}

}  // namespace

MosaicWindowScheduler::MosaicWindowScheduler(MosaicWindowConfig cfg)
    : cfg_(std::move(cfg)), probes_(cfg_.probe_path) {
  // 合同下界：worker 数由配置注入（ThreadBudget），此处仅做下界保护，非默认值。
  constexpr int kMinWorkers = 1;
  cfg_.workers = std::max(cfg_.workers, kMinWorkers);
  if (cfg_.window_tiles < 1) cfg_.window_tiles = 1;
}

MosaicWindowScheduler::~MosaicWindowScheduler() {
  // 池是 run() 的局部量（run 返回前已全部 join 回收），对象析构只需置取消唤醒在途 worker。
  cancel();
}

void MosaicWindowScheduler::add_frame(MosaicFrameInput f) { frames_.push_back(std::move(f)); }

std::size_t MosaicWindowScheduler::tile_count() const {
  std::set<std::uint64_t> s;
  for (const auto& f : frames_) for (const auto& kv : f.tile_bytes) s.insert(kv.first);
  return s.size();
}

std::vector<MosaicWindow> MosaicWindowScheduler::partition() const {
  std::set<std::uint64_t> s;
  for (const auto& f : frames_) for (const auto& kv : f.tile_bytes) s.insert(kv.first);
  std::vector<std::uint64_t> tiles(s.begin(), s.end());

  std::vector<MosaicWindow> out;
  std::uint64_t wid = 0;
  for (std::size_t i = 0; i < tiles.size(); i += static_cast<std::size_t>(cfg_.window_tiles)) {
    MosaicWindow w;
    w.window_id = wid++;
    const std::size_t j = std::min(tiles.size(), i + static_cast<std::size_t>(cfg_.window_tiles));
    w.tile_ipix.assign(tiles.begin() + i, tiles.begin() + j);
    // 路由读取量 = 覆盖该窗口 tile 的各帧切片之和
    for (const auto& f : frames_)
      for (std::uint64_t t : w.tile_ipix) {
        auto it = f.tile_bytes.find(t);
        if (it != f.tile_bytes.end()) w.routed_bytes += it->second;
      }
    // 朴素基线 = 每个窗口都读整帧
    for (const auto& f : frames_) w.naive_bytes += f.total_bytes();
    out.push_back(std::move(w));
  }
  return out;
}

void MosaicWindowScheduler::write_manifest() const {
  if (cfg_.manifest_path.empty()) return;
  aio_atomic::make_dirs(dirname_of(cfg_.manifest_path));
  const auto ws = partition();
  std::uint64_t routed = 0, naive = 0;
  for (const auto& w : ws) { routed += w.routed_bytes; naive += w.naive_bytes; }
  std::ostringstream f;
  f << R"JSON({
  "schema": "astrocs.mosaic-window-manifest/v1",
  "hips_level": )JSON" << cfg_.hips_level << R"JSON(,
  "window_tiles": )JSON" << cfg_.window_tiles << R"JSON(,
  "window_count": )JSON" << ws.size() << R"JSON(,
  "tile_count": )JSON" << tile_count() << R"JSON(,
  "frame_count": )JSON" << frames_.size() << R"JSON(,
  "workers": )JSON" << cfg_.workers << R"JSON(,
  "routed_bytes": )JSON" << routed << R"JSON(,
  "naive_whole_frame_bytes": )JSON" << naive << R"JSON(,
  "read_amplification": )JSON"
    << (naive > 0 ? static_cast<double>(routed) / static_cast<double>(naive) : 0.0) << R"JSON(,
  "windows": [)JSON";
  for (std::size_t i = 0; i < ws.size(); ++i) {
    f << (i ? ", " : "") << R"JSON({"window_id": )JSON" << ws[i].window_id
      << R"JSON(, "tiles": )JSON" << ws[i].tile_ipix.size()
      << R"JSON(, "routed_bytes": )JSON" << ws[i].routed_bytes << "}";
  }
  f << R"JSON(]
}
)JSON";
  // CLEAN-403：机制经 aio 唯一实现，本 TU 不自持 ofstream 通道。
  aio_atomic::write_file_atomic(cfg_.manifest_path, f.str(), nullptr);
}

double MosaicWindowScheduler::read_amplification() const {
  const std::uint64_t n = naive_baseline_.load();
  return (n > 0) ? static_cast<double>(total_read_.load()) / static_cast<double>(n) : 0.0;
}

WindowOutcome MosaicWindowScheduler::run_window(const MosaicWindow& w, int worker_index) {
  WindowOutcome out;
  out.window_id = w.window_id;
  const double t0 = now_seconds();

  // ② 输入按窗口路由：只取覆盖本窗口 tile 的帧切片
  std::vector<const MosaicFrameInput*> relevant;
  for (const auto& f : frames_) {
    bool covers = false;
    for (std::uint64_t t : w.tile_ipix)
      if (f.tile_bytes.count(t)) { covers = true; break; }
    if (covers) relevant.push_back(&f);
  }
  std::uint64_t read = 0;
  for (const MosaicFrameInput* f : relevant)
    for (std::uint64_t t : w.tile_ipix) {
      auto it = f->tile_bytes.find(t);
      if (it != f->tile_bytes.end()) read += it->second;
    }
  out.read_bytes = read;
  total_read_.fetch_add(read);
  naive_baseline_.fetch_add(w.naive_bytes);

  {
    ProbeEvent ev;
    ev.ts = now_seconds();
    ev.kind = ProbeKind::IO;
    ev.value = static_cast<double>(read);
    ev.unit = "B";
    ev.bytes = static_cast<std::int64_t>(read);
    ev.window_id = w.window_id;
    ev.worker = worker_index;
    ev.stage = "mosaic";
    probes_.emit(ev);
  }

  // ③ 窗口内固定顺序：coverage → UPM → 排异 → SNR² 集成（此处按像素序做集成）
  // ④ 稠密 SNR 现场求值：逐像素即时算，不预计算稠密面、不驻留
  //
  // ④b **峰值驻留 = 实测值**。
  // 修复前此处写 const std::size_t resident = sizeof(double) * 4;（编译期常量 32 B），
  // 是写入 peak 的**唯一来源** ⇒ mosaic_window 的 E1（peak>4096 判红）/E2 断言对象即该
  // 常量，**不可能红**（AGENTS §5「恒真门没有证据资格」）。
  // 现行口径：逐次记账本窗口**真实存活的分配**（输出像素缓冲 + 路由指针向量的
  // capacity() 字节 —— 取真实分配器足迹而非 size），循环内取最大值。
  // 不含 MosaicFrameInput::tile_bytes —— 那是输入**声明**字节（aio 侧切片量），
  // 本进程不物化、不驻留。
  constexpr std::int64_t kPixelsPerTile = 64;
  std::uint64_t h = 1469598103934665603ULL;
  std::size_t peak = 0;
  std::int64_t pixels = 0;
  const std::size_t resident_fixed =
      relevant.capacity() * sizeof(const MosaicFrameInput*);
  for (std::uint64_t tile : w.tile_ipix) {
    if (cancel_.load()) { out.error = "cancelled"; break; }
    std::vector<const MosaicFrameInput*> cover;
    for (const MosaicFrameInput* f : relevant)
      if (f->tile_bytes.count(tile)) cover.push_back(f);
    std::sort(cover.begin(), cover.end(),
              [](const MosaicFrameInput* a, const MosaicFrameInput* b) {
                return a->frame_id < b->frame_id;   // 归约顺序冻结
              });
    const std::size_t cover_bytes = cover.capacity() * sizeof(const MosaicFrameInput*);
    for (std::int64_t px = 0; px < kPixelsPerTile; ++px) {
      const double v = integrate_pixel(cover, tile, px);
      out.pixel_values.push_back(v);
      h = fnv1a(h, &tile, sizeof(tile));
      h = fnv1a(h, &px, sizeof(px));
      h = fnv1a(h, &v, sizeof(v));
      ++pixels;
      // 实测驻留 = 窗口输出像素缓冲（真实 capacity）+ 本窗口指针向量 + 标量局部。
      const std::size_t resident = out.pixel_values.capacity() * sizeof(double) +
                                   cover_bytes + resident_fixed +
                                   sizeof(std::uint64_t) * 4;
      peak = std::max(peak, resident);
    }
  }
  out.pixels = pixels;
  out.checksum = h;
  out.peak_bytes = peak;
  out.wall_seconds = now_seconds() - t0;
  if (out.error.empty()) out.ok = true;
  {
    std::size_t prev = peak_inflight_.load();
    while (out.peak_bytes > prev && !peak_inflight_.compare_exchange_weak(prev, out.peak_bytes)) {}
  }
  {
    ProbeEvent ev;
    ev.ts = now_seconds();
    ev.kind = ProbeKind::NODE_WALL;
    ev.node = "mosaic_window";
    ev.value = out.wall_seconds;
    ev.unit = "s";
    ev.window_id = w.window_id;
    ev.worker = worker_index;
    ev.stage = "mosaic";
    probes_.emit(ev);
  }
  return out;
}

void MosaicWindowScheduler::worker_loop(int worker_index) {
  for (;;) {
    std::size_t idx = 0;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_.wait(lk, [&] { return cancel_.load() || next_ < windows_.size(); });
      if (next_ >= windows_.size()) return;
      idx = next_++;
    }
    WindowOutcome o = run_window(windows_[idx], worker_index);
    {
      std::lock_guard<std::mutex> lk(mu_);
      outcomes_[idx] = std::move(o);
      ++completed_;
    }
    cv_done_.notify_all();
  }
}

std::vector<WindowOutcome> MosaicWindowScheduler::run() {
  windows_ = partition();
  outcomes_.assign(windows_.size(), WindowOutcome{});
  {
    std::lock_guard<std::mutex> lk(mu_);
    next_ = 0;
    completed_ = 0;
  }
  // 本次 run 的**有界 run 作用域池**（RT-004：调度器不持有永久池成员）：
  // 线程数 = cfg_.workers（配置/预算注入），run 返回前全部 join 回收。
  const int n = std::max(1, cfg_.workers);
  std::vector<std::thread> pool;
  pool.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) pool.emplace_back([this, i] { worker_loop(i); });
  {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [&] { return completed_ >= windows_.size() || cancel_.load(); });
  }
  cancel();
  for (auto& t : pool) if (t.joinable()) t.join();   // run 返回前全部回收（无 detach）
  pool.clear();

  // ⑤ 归约顺序冻结：窗口 ID 升序
  std::vector<WindowOutcome> out = outcomes_;
  std::stable_sort(out.begin(), out.end(), [](const WindowOutcome& a, const WindowOutcome& b) {
    return a.window_id < b.window_id;
  });
  probes_.flush();
  write_manifest();
  return out;
}

// ── 窗口峰值驻留判据实现（可红可绿） ──
// 判据语义见头文件注释（四条同时成立才绿）。本函数是**唯一判据实现**：单测绿/红两侧
// 都调用它，负例（把实测峰值换成编译期常量 / 换成 0）必须在同一函数上判红。
std::size_t MosaicWindowScheduler::window_peak_bound_bytes(std::size_t tiles_in_window,
                                                           std::size_t frames_covering) {
  // kPixelsPerTile 与 run_window 同源（窗口内每 tile 的像素数；本 TU 内冻结常量）。
  constexpr std::size_t kPixelsPerTile = 64;
  const std::size_t out_buf = tiles_in_window * kPixelsPerTile * sizeof(double);
  // vector 容量按几何增长 ⇒ 真实 capacity 可到 size 的 2 倍以内，上界取 2×。
  const std::size_t ptr_vecs = 2 * frames_covering * sizeof(const MosaicFrameInput*);
  return 2 * out_buf + 2 * ptr_vecs + 64;
}

bool window_peak_residency_ok(const std::vector<WindowPeakSample>& samples,
                              std::string* why) {
  auto fail = [&](const std::string& msg) {
    if (why) *why = msg;
    return false;
  };
  if (samples.empty()) return fail("no samples (empty judgement domain)");
  // ① 非零：常数 0 / 未记账 ⇒ 红
  for (const auto& s : samples) {
    if (s.peak_bytes == 0) {
      return fail("E1a peak_bytes==0 for window_tiles=" +
                  std::to_string(s.window_tiles) +
                  " (constant/zero residency: no evidence)");
    }
  }
  // ② 实测受解析上界约束
  for (const auto& s : samples) {
    if (s.peak_bytes > s.bound_bytes) {
      return fail("E1b peak_bytes=" + std::to_string(s.peak_bytes) +
                  " > bound=" + std::to_string(s.bound_bytes) +
                  " for window_tiles=" + std::to_string(s.window_tiles));
    }
  }
  // ③ 与总图规模解耦：同 window_tiles、不同 tiles_total ⇒ 峰值相等
  for (std::size_t i = 0; i < samples.size(); ++i) {
    for (std::size_t k = i + 1; k < samples.size(); ++k) {
      if (samples[i].window_tiles != samples[k].window_tiles) continue;
      if (samples[i].tiles_total == samples[k].tiles_total) continue;
      if (samples[i].peak_bytes != samples[k].peak_bytes) {
        return fail("E2 peak decoupled from total image size violated: window_tiles=" +
                    std::to_string(samples[i].window_tiles) + " tiles_total=" +
                    std::to_string(samples[i].tiles_total) + " peak=" +
                    std::to_string(samples[i].peak_bytes) + " vs tiles_total=" +
                    std::to_string(samples[k].tiles_total) + " peak=" +
                    std::to_string(samples[k].peak_bytes));
      }
    }
  }
  // ④ 窗口大小是显式内存权衡参数（ASTROCS_DESIGN §8.3）：两个窗口**都实际装满**时，
  //    window_tiles 更大 ⇒ 峰值驻留**严格更大**。
  //    「严格」是反恒真的关键：编译期常量（F-07 的 32 B）与任何未记账的常量驻留
  //    在这里必然违反 —— 旧的「不减」表述会被常量满足，属退化判据。
  for (std::size_t i = 0; i < samples.size(); ++i) {
    for (std::size_t k = 0; k < samples.size(); ++k) {
      if (samples[k].window_tiles <= samples[i].window_tiles) continue;
      const bool full_i =
          samples[i].tiles_total >= static_cast<std::size_t>(samples[i].window_tiles);
      const bool full_k =
          samples[k].tiles_total >= static_cast<std::size_t>(samples[k].window_tiles);
      if (!full_i || !full_k) continue;   // 窗口未装满 ⇒ 驻留不可比，跳过
      if (samples[k].peak_bytes <= samples[i].peak_bytes) {
        return fail("E3 peak must grow strictly with window_tiles: window_tiles=" +
                    std::to_string(samples[i].window_tiles) + " peak=" +
                    std::to_string(samples[i].peak_bytes) + " vs window_tiles=" +
                    std::to_string(samples[k].window_tiles) + " peak=" +
                    std::to_string(samples[k].peak_bytes) +
                    " (constant residency = degenerate gate, DESIGN 8.3)");
      }
    }
  }
  if (why) why->clear();
  return true;
}

}  // namespace astrocs::core
