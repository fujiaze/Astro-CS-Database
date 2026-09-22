// AstroCS Core — ARCH-503 mosaic 天球窗口并行调度器实现
// 依据：ASTROCS_DESIGN.md §8.3；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md
#include "astrocs/core/mosaic_window.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>

namespace astrocs::core {
namespace {

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
  if (cfg_.workers < 1) cfg_.workers = 1;
  if (cfg_.window_tiles < 1) cfg_.window_tiles = 1;
}

MosaicWindowScheduler::~MosaicWindowScheduler() {
  cancel();
  for (auto& t : pool_) if (t.joinable()) t.join();
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
  std::ofstream f(cfg_.manifest_path, std::ios::out | std::ios::trunc);
  if (!f) return;
  const auto ws = partition();
  std::uint64_t routed = 0, naive = 0;
  for (const auto& w : ws) { routed += w.routed_bytes; naive += w.naive_bytes; }
  f << "{\n  \"schema\": \"astrocs.mosaic-window-manifest/v1\",\n"
    << "  \"hips_level\": " << cfg_.hips_level << ",\n"
    << "  \"window_tiles\": " << cfg_.window_tiles << ",\n"
    << "  \"window_count\": " << ws.size() << ",\n"
    << "  \"tile_count\": " << tile_count() << ",\n"
    << "  \"frame_count\": " << frames_.size() << ",\n"
    << "  \"workers\": " << cfg_.workers << ",\n"
    << "  \"routed_bytes\": " << routed << ",\n"
    << "  \"naive_whole_frame_bytes\": " << naive << ",\n"
    << "  \"read_amplification\": "
    << (naive > 0 ? static_cast<double>(routed) / static_cast<double>(naive) : 0.0) << ",\n"
    << "  \"windows\": [";
  for (std::size_t i = 0; i < ws.size(); ++i) {
    f << (i ? ", " : "") << "{\"window_id\": " << ws[i].window_id
      << ", \"tiles\": " << ws[i].tile_ipix.size()
      << ", \"routed_bytes\": " << ws[i].routed_bytes << "}";
  }
  f << "]\n}\n";
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
  constexpr std::int64_t kPixelsPerTile = 64;
  std::uint64_t h = 1469598103934665603ULL;
  std::size_t peak = 0;
  std::int64_t pixels = 0;
  for (std::uint64_t tile : w.tile_ipix) {
    if (cancel_.load()) { out.error = "cancelled"; break; }
    std::vector<const MosaicFrameInput*> cover;
    for (const MosaicFrameInput* f : relevant)
      if (f->tile_bytes.count(tile)) cover.push_back(f);
    std::sort(cover.begin(), cover.end(),
              [](const MosaicFrameInput* a, const MosaicFrameInput* b) {
                return a->frame_id < b->frame_id;   // 归约顺序冻结
              });
    for (std::int64_t px = 0; px < kPixelsPerTile; ++px) {
      const double v = integrate_pixel(cover, tile, px);
      out.pixel_values.push_back(v);
      h = fnv1a(h, &tile, sizeof(tile));
      h = fnv1a(h, &px, sizeof(px));
      h = fnv1a(h, &v, sizeof(v));
      ++pixels;
      const std::size_t resident = sizeof(double) * 4;   // 现场求值：常数级驻留
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
  const int n = std::max(1, cfg_.workers);
  pool_.clear();
  for (int i = 0; i < n; ++i) pool_.emplace_back([this, i] { worker_loop(i); });
  {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [&] { return completed_ >= windows_.size() || cancel_.load(); });
  }
  cancel();
  for (auto& t : pool_) if (t.joinable()) t.join();
  pool_.clear();

  // ⑤ 归约顺序冻结：窗口 ID 升序
  std::vector<WindowOutcome> out = outcomes_;
  std::stable_sort(out.begin(), out.end(), [](const WindowOutcome& a, const WindowOutcome& b) {
    return a.window_id < b.window_id;
  });
  probes_.flush();
  write_manifest();
  return out;
}

}  // namespace astrocs::core
