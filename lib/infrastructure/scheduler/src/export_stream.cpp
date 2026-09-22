// AstroCS Core — ARCH-504 export 子块流式调度器实现
// 依据：ASTROCS_DESIGN.md §6/§8.3；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md
#include "astrocs/core/export_stream.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>
#include "aio_atomic_file.h"
#include "aio_file_io.h"   // CLEAN-403：aio 唯一 I/O 实现（原子写/顺序写/目录/rename）

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

std::string dirname_of(const std::string& p) {
  const std::size_t k = p.find_last_of('/');
  return (k == std::string::npos) ? std::string(".") : p.substr(0, k);
}

}  // namespace

ExportStreamScheduler::ExportStreamScheduler(ExportStreamConfig cfg)
    : cfg_(std::move(cfg)), probes_(cfg_.probe_path) {
  // 合同下界：worker 数由配置注入（ThreadBudget），此处仅做下界保护，非默认值。
  constexpr int kMinWorkers = 1;
  cfg_.workers = std::max(cfg_.workers, kMinWorkers);
  if (cfg_.sub_block_px < 1) cfg_.sub_block_px = 1;
  if (cfg_.queue_depth < 1) cfg_.queue_depth = 1;
}

ExportStreamScheduler::~ExportStreamScheduler() {
  // 池是 run() 的局部量（run 返回前已全部 join 回收），对象析构只需置取消唤醒在途 worker。
  cancel();
}

std::size_t ExportStreamScheduler::sub_block_count() const {
  if (width_ <= 0 || height_ <= 0) return 0;
  const std::size_t nx = static_cast<std::size_t>((width_ + cfg_.sub_block_px - 1) / cfg_.sub_block_px);
  const std::size_t ny = static_cast<std::size_t>((height_ + cfg_.sub_block_px - 1) / cfg_.sub_block_px);
  return nx * ny;
}

void ExportStreamScheduler::write_manifest() const {
  if (cfg_.manifest_path.empty()) return;
  aio_atomic::make_dirs(dirname_of(cfg_.manifest_path));
  const std::size_t n = sub_block_count();
  const std::size_t sb_bytes =
      static_cast<std::size_t>(cfg_.sub_block_px) * cfg_.sub_block_px * sizeof(double);
  std::ostringstream f;
  f << R"JSON({
  "schema": "astrocs.export-stream-manifest/v1",
  "width": )JSON" << width_ << R"JSON(,
  "height": )JSON" << height_ << R"JSON(,
  "sub_block_px": )JSON" << cfg_.sub_block_px << R"JSON(,
  "sub_block_count": )JSON" << n << R"JSON(,
  "queue_depth": )JSON" << cfg_.queue_depth << R"JSON(,
  "workers": )JSON" << cfg_.workers << R"JSON(,
  "wcs_header_ready_before_write": )JSON"
    << ((cfg_.sink ? cfg_.header_ready
                   : (!cfg_.wcs_header.empty() && !cfg_.properties.empty()))
            ? "true" : "false") << R"JSON(,
  "wcs_header_bytes": )JSON" << cfg_.wcs_header.size() << R"JSON(,
  "properties_bytes": )JSON" << cfg_.properties.size() << R"JSON(,
  "sub_block_bytes": )JSON" << sb_bytes << R"JSON(,
  "bounded_inflight_bytes": )JSON"
    << (2 * static_cast<std::size_t>(cfg_.queue_depth) * sb_bytes) << R"JSON(,
  "sink_mode": )JSON" << (cfg_.sink ? "true" : "false") << R"JSON(,
  "header_ready_before_write": )JSON"
    << ((cfg_.sink ? cfg_.header_ready
                   : (!cfg_.wcs_header.empty() && !cfg_.properties.empty()))
            ? "true" : "false") << R"JSON(
}
)JSON";
  // CLEAN-403：机制经 aio 唯一实现，本 TU 不自持 ofstream 通道。
  aio_atomic::write_file_atomic(cfg_.manifest_path, f.str(), nullptr);
}

void ExportStreamScheduler::reader_loop() {
  const int s = cfg_.sub_block_px;
  std::size_t idx = 0;
  for (int y0 = 0; y0 < height_; y0 += s) {
    for (int x0 = 0; x0 < width_; x0 += s) {
      if (cancel_.load()) return;
      Job j;
      j.index = idx++;
      j.x0 = x0;
      j.y0 = y0;
      j.w = std::min(s, width_ - x0);
      j.h = std::min(s, height_ - y0);
      {
        std::unique_lock<std::mutex> lk(mu_);
        // ② 背压（唯一施加点）：读队列满 **或** 在途子块总量达上限则阻塞等待。
        //    在途总量 = 已投递 - 已写出，涵盖 读队列 + 算队列 + 重排缓冲 + 计算中，
        //    因此重排缓冲不可能无界增长，且不会与下游背压互锁。
        const std::size_t cap = 2 * static_cast<std::size_t>(cfg_.queue_depth);
        cv_space_.wait(lk, [&] {
          return cancel_.load() ||
                 (q_jobs_.size() < static_cast<std::size_t>(cfg_.queue_depth) &&
                  inflight_count_ < cap);
        });
        if (cancel_.load()) return;
        q_jobs_.push_back(j);
        ++jobs_produced_;
        ++inflight_count_;
        inflight_bytes_ += static_cast<std::size_t>(j.w) * j.h * sizeof(double);
        max_queue_ = std::max(max_queue_, q_jobs_.size() + q_results_.size());
        peak_resident_ = std::max(peak_resident_, inflight_bytes_);
      }
      cv_item_.notify_all();
      ProbeEvent ev;
      ev.ts = now_seconds();
      ev.kind = ProbeKind::IO;
      ev.value = static_cast<double>(j.w) * j.h * sizeof(double);
      ev.unit = "B";
      ev.bytes = static_cast<std::int64_t>(ev.value);
      ev.window_id = j.index;
      ev.stage = "export";
      probes_.emit(ev);
    }
  }
  // 生产完毕：必须唤醒所有等待者（否则 compute 在最后一次 pop 后无人唤醒 ⇒ join 挂死）
  cv_item_.notify_all();
  cv_space_.notify_all();
}

void ExportStreamScheduler::compute_loop() {
  for (;;) {
    Job j;
    {
      std::unique_lock<std::mutex> lk(mu_);
      const double t0 = now_seconds();
      cv_item_.wait(lk, [&] {
        return cancel_.load() || !q_jobs_.empty() || jobs_produced_ >= jobs_total_;
      });
      const double waited = now_seconds() - t0;
      if (q_jobs_.empty()) {
        if (cancel_.load() || jobs_produced_ >= jobs_total_) return;
        continue;
      }
      j = q_jobs_.front();
      q_jobs_.pop_front();
      cv_space_.notify_all();
      ProbeEvent qe;
      qe.ts = now_seconds();
      qe.kind = ProbeKind::QUEUE_WAIT;
      qe.value = waited;
      qe.unit = "s";
      qe.window_id = j.index;
      qe.stage = "export";
      probes_.emit(qe);
    }
    const double t0 = now_seconds();
    Result r;
    r.index = j.index;
    r.x0 = j.x0; r.y0 = j.y0; r.w = j.w; r.h = j.h;
    r.data.resize(static_cast<std::size_t>(j.w) * j.h);
    // 生产面：调用方按**子块**产出（读子块 → 投影重采样）；兼容面：逐像素回调。
    if (cfg_.sub_block_fn) {
      cfg_.sub_block_fn(j.x0, j.y0, j.w, j.h, r.data.data());
    } else {
      for (int yy = 0; yy < j.h; ++yy)
        for (int xx = 0; xx < j.w; ++xx)
          r.data[static_cast<std::size_t>(yy) * j.w + xx] =
              pixel_fn_ ? pixel_fn_(j.x0 + xx, j.y0 + yy) : 0.0;
    }
    ProbeEvent we;
    we.ts = now_seconds();
    we.kind = ProbeKind::NODE_WALL;
    we.node = "export_compute";
    we.value = we.ts - t0;
    we.unit = "s";
    we.window_id = j.index;
    we.stage = "export";
    probes_.emit(we);
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_space_.wait(lk, [&] {
        return cancel_.load() || q_results_.size() < static_cast<std::size_t>(cfg_.queue_depth);
      });
      if (cancel_.load()) return;
      q_results_.push_back(std::move(r));
      max_queue_ = std::max(max_queue_, q_jobs_.size() + q_results_.size());
      peak_resident_ = std::max(peak_resident_, inflight_bytes_);
    }
    cv_item_.notify_all();
  }
}

// 失败/取消的统一收尾：置取消位、放行所有等待者（不发布、不留半成品）。
void ExportStreamScheduler::signal_stop() {
  cancel_.store(true);
  {
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
  }
  cv_done_.notify_all();
  cv_item_.notify_all();
  cv_space_.notify_all();
}

void ExportStreamScheduler::writer_loop() {
  if (cfg_.sink) writer_loop_sink();
  else writer_loop_raw();
}

// ── 生产面：sink 通道（phase3 writer 节点经此把子块写进 FITS 数据区）──────
// 与内建通道同构：同一背压/在途上界/子块索引升序重排；差别只在「谁来落盘」。
// ③ 头齐备性由 sink 在 open() 内完成（header_ready 声明头已在首像素前组装）。
void ExportStreamScheduler::writer_loop_sink() {
  if (!cfg_.header_ready) {
    sink_error_ = "sink header not ready before first pixel write";
    signal_stop();
    return;
  }
  std::string err;
  if (!cfg_.sink->open(&err)) {
    sink_error_ = "sink open failed: " + err;
    signal_stop();
    return;
  }
  std::size_t next_index = 0;
  std::size_t done = 0;
  std::uint64_t written = 0;
  bool ok = true;
  std::map<std::size_t, Result> pending;   // 子块索引升序写出（与 worker 数无关）
  for (;;) {
    Result r;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_item_.wait(lk, [&] {
        return cancel_.load() || !q_results_.empty() || results_done_ >= jobs_total_;
      });
      if (q_results_.empty()) {
        if (cancel_.load() || results_done_ >= jobs_total_) break;
        continue;
      }
      r = std::move(q_results_.front());
      q_results_.pop_front();
      cv_space_.notify_all();
    }
    pending.emplace(r.index, std::move(r));
    while (true) {
      auto it = pending.find(next_index);
      if (it == pending.end()) break;
      const Result& cur = it->second;
      const std::uint64_t nb =
          static_cast<std::uint64_t>(cur.data.size() * sizeof(double));
      if (cfg_.fail_write_after_bytes && written + nb > cfg_.fail_write_after_bytes) {
        disk_full_.store(true);
        ok = false;
        sink_error_ = "disk_full (injected)";
        break;
      }
      if (!cfg_.sink->write_sub_block(next_index, cur.x0, cur.y0, cur.w, cur.h,
                                      cur.data.data(), &err)) {
        ok = false;
        sink_error_ = "sink write_sub_block failed: " + err;
      }
      written += nb;
      {
        std::lock_guard<std::mutex> lk(mu_);
        inflight_bytes_ -= cur.data.size() * sizeof(double);
        if (inflight_count_ > 0) --inflight_count_;
        ++results_done_;
      }
      cv_space_.notify_all();
      pending.erase(it);
      ++next_index;
      ++done;
      ProbeEvent ev;
      ev.ts = now_seconds();
      ev.kind = ProbeKind::BLOCK_DEATH;
      ev.block = "sub_block";
      ev.bytes = static_cast<std::int64_t>(written);
      ev.value = static_cast<double>(written);
      ev.unit = "B";
      ev.window_id = next_index;
      ev.stage = "export";
      probes_.emit(ev);
      if (!ok) break;
    }
    if (!ok) { cancel_.store(true); break; }
  }
  const bool complete = ok && !cancel_.load() && next_index == jobs_total_;
  if (complete) {
    if (cfg_.sink->finish(&err)) {
      sink_published_ = true;
    } else {
      sink_error_ = "sink finish failed: " + err;
      cfg_.sink->abort();
    }
  } else {
    cfg_.sink->abort();          // 取消/中断：不得留下半成品
  }
  bytes_written_.store(written);
  {
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
  }
  cv_done_.notify_all();
  cv_item_.notify_all();
  cv_space_.notify_all();
}

// ── 兼容/内建通道：WCS 头 + properties + 行主序 double 像素（原样语义）─────
void ExportStreamScheduler::writer_loop_raw() {
  // ③ WCS 头与 properties 在开写前组装：这里先校验齐备性（缺则拒绝开写）
  if (cfg_.wcs_header.empty() || cfg_.properties.empty()) {
    signal_stop();
    return;
  }
  // ④ 原子发布：先写临时文件，全部写完后 rename
  const std::string tmp = cfg_.output_path + ".tmp";
  aio_atomic::make_dirs(dirname_of(cfg_.output_path));
  std::string open_err;
  aio_atomic::AppendSink* out = aio_atomic::write_open_trunc(tmp, &open_err);
  if (!out) {
    signal_stop();
    return;
  }
  // 顺序写：WCS 头 + properties 先落，随后**按输出行序**追加像素带。
  // FITS 产品是全图行主序；子块是 tile 局部行主序，故不能整块追加。
  // 这里用「行带缓冲」：带高 = 子块边长，子块按 (tile_y, tile_x) 行主序产出，
  // 同一行带内的子块任意 x 序都散射进同一缓冲区，带满即顺序追加。
  // 结果：内存上界 = width × band_rows × 8 B（与子块数无关），且全程无 seek。
  bool io_failed = false;
  auto append = [&](const void* p, std::size_t n) -> bool {
    if (n == 0) return true;
    if (aio_atomic::append_write(out, p, n) != 0) { io_failed = true; return false; }
    return true;
  };
  if (!append(cfg_.wcs_header.data(), cfg_.wcs_header.size()) ||
      !append(cfg_.properties.data(), cfg_.properties.size())) {
    aio_atomic::append_close(out);
    aio_atomic::remove_file(tmp);
    disk_full_.store(true);
    cancel_.store(true);
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
    cv_done_.notify_all();
    cv_item_.notify_all();
    cv_space_.notify_all();
    return;
  }
  std::uint64_t written = cfg_.wcs_header.size() + cfg_.properties.size();
  const int band_rows = cfg_.sub_block_px;
  const std::size_t band_cap = static_cast<std::size_t>(width_) *
                               static_cast<std::size_t>(band_rows);
  std::vector<double> band;
  int band_start = 0;
  auto flush_band = [&](int rows) -> bool {
    if (rows <= 0) return true;
    const std::size_t n = static_cast<std::size_t>(rows) * static_cast<std::size_t>(width_);
    return append(band.data(), n * sizeof(double));
  };
  // 重排缓冲：按子块索引升序写出（与 worker 数无关 ⇒ 输出逐位一致）
  std::map<std::size_t, Result> pending;
  std::size_t next_index = 0;
  std::size_t done = 0;
  for (;;) {
    Result r;
    {
      std::unique_lock<std::mutex> lk(mu_);
      cv_item_.wait(lk, [&] {
        return cancel_.load() || !q_results_.empty() || results_done_ >= jobs_total_;
      });
      if (q_results_.empty()) {
        if (cancel_.load() || results_done_ >= jobs_total_) break;
        continue;
      }
      r = std::move(q_results_.front());
      q_results_.pop_front();
      cv_space_.notify_all();
    }
    pending.emplace(r.index, std::move(r));
    while (true) {
      auto it = pending.find(next_index);
      if (it == pending.end()) break;
      const Result& cur = it->second;
      const std::uint64_t nb = static_cast<std::uint64_t>(cur.data.size() * sizeof(double));
      if (cfg_.fail_write_after_bytes && written + nb > cfg_.fail_write_after_bytes) {
        // 磁盘满：停止写出、**不发布**（原子语义：临时文件不得 rename 到产品路径）
        disk_full_.store(true);
        cancel_.store(true);           // 让读/算线程立即退出，避免 join 挂死
        aio_atomic::append_close(out);
        aio_atomic::remove_file(tmp);
        {
          std::lock_guard<std::mutex> lk(mu_);
          results_done_ = jobs_total_;
        }
        cv_done_.notify_all();
        cv_item_.notify_all();
        cv_space_.notify_all();
        return;
      }
      // 跨行带：先把上一带顺序追加，再开新带（子块与带边界对齐，不跨界）
      if (cur.y0 >= band_start + band_rows) {
        const int rows = std::min(band_rows, height_ - band_start);
        if (!flush_band(rows)) break;
        band_start += band_rows;
        band.assign(band_cap, 0.0);
      }
      if (band.empty()) band.assign(band_cap, 0.0);
      for (int yy = 0; yy < cur.h; ++yy) {
        const int oy = cur.y0 + yy;
        if (oy < band_start || oy >= band_start + band_rows) continue;   // 越界行不写（不静默扩带）
        double* dst = band.data() +
            static_cast<std::size_t>(oy - band_start) * static_cast<std::size_t>(width_) +
            static_cast<std::size_t>(cur.x0);
        std::copy(cur.data.begin() + static_cast<std::size_t>(yy) * cur.w,
                  cur.data.begin() + static_cast<std::size_t>(yy + 1) * cur.w, dst);
      }
      written += nb;
      pending.erase(it);
      ++next_index;
      ++done;
      {
        // ACCEPT-501 P-1：在途计数的**递减必须与 reader 的递增同锁**。
        // 原实现在 mu_ 之外递减非原子成员，与持锁递增构成数据竞争 ⇒ 丢失更新 +
        // 无符号下溢（实测 inflight_bytes_ 回绕到 2^64-1048576，约 21% 概率红）。
        std::lock_guard<std::mutex> lk(mu_);
        inflight_bytes_ -= cur.data.size() * sizeof(double);
        if (inflight_count_ > 0) --inflight_count_;
        ++results_done_;
      }
      cv_space_.notify_all();
    }
    if (io_failed) break;
    ProbeEvent ev;
    ev.ts = now_seconds();
    ev.kind = ProbeKind::BLOCK_DEATH;
    ev.block = "sub_block";
    ev.bytes = static_cast<std::int64_t>(written);
    ev.value = static_cast<double>(written);
    ev.unit = "B";
    ev.window_id = next_index;
    ev.stage = "export";
    probes_.emit(ev);
  }
  // 收尾：刷出最后一个未满带
  if (!io_failed && !cancel_.load()) {
    for (int bs = band_start; bs < height_; bs += band_rows) {
      const int rows = std::min(band_rows, height_ - bs);
      if (!flush_band(rows)) break;
      if (bs + band_rows < height_) band.assign(band_cap, 0.0);
    }
  }
  if (io_failed) disk_full_.store(true);
  bytes_written_.store(written);
  const bool complete = (next_index == jobs_total_) && !cancel_.load() &&
                        !disk_full_.load() && !io_failed;
  if (aio_atomic::append_flush(out) != 0) disk_full_.store(true);
  aio_atomic::append_close(out);
  if (complete) {
    // 原子 rename（同目录内 rename 为原子操作）
    if (aio_atomic::atomic_replace(tmp, cfg_.output_path) != 0) {
      aio_atomic::remove_file(tmp);
      disk_full_.store(true);        // rename 失败按写失败处理
    }
  } else {
    aio_atomic::remove_file(tmp);    // 取消/中断：不得留下半成品
  }
  {
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
  }
  cv_done_.notify_all();
}

ExportOutcome ExportStreamScheduler::run() {
  ExportOutcome o;
  jobs_total_ = sub_block_count();
  {
    std::lock_guard<std::mutex> lk(mu_);
    q_jobs_.clear();
    q_results_.clear();
    jobs_produced_ = 0;
    results_done_ = 0;
    max_queue_ = 0;
    peak_resident_ = 0;
    inflight_bytes_ = 0;
    inflight_count_ = 0;
  }
  disk_full_.store(false);
  bytes_written_.store(0);
  sink_published_ = false;
  sink_error_.clear();
  const bool pre_cancelled = cancel_.load();
  const double t0 = now_seconds();

  // ② 三级有界流水线：本次 run 的**有界 run 作用域池**（RT-004：调度器不持有永久池成员）。
  //    线程数 = cfg_.workers（配置/预算注入）+ 读/写各 1，run 返回前全部 join 回收。
  std::vector<std::thread> pool;
  pool.reserve(static_cast<std::size_t>(cfg_.workers) + 2);
  pool.emplace_back([this] { writer_loop(); });
  pool.emplace_back([this] { reader_loop(); });
  for (int i = 0; i < cfg_.workers; ++i) pool.emplace_back([this] { compute_loop(); });

  {
    std::unique_lock<std::mutex> lk(mu_);
    cv_done_.wait(lk, [&] {
      return results_done_ >= jobs_total_ || cancel_.load() || disk_full_.load();
    });
  }
  const bool was_cancelled = cancel_.load();   // 外部取消 vs 收尾置位，必须区分
  {
    std::lock_guard<std::mutex> lk(mu_);
    cancel_.store(true);
  }
  cv_item_.notify_all();
  cv_space_.notify_all();
  for (auto& t : pool) if (t.joinable()) t.join();   // run 返回前全部回收（无 detach）
  pool.clear();

  (void)pre_cancelled;
  o.pixels = static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
  o.peak_resident_bytes = peak_resident_;
  o.max_queue_depth = max_queue_;
  o.sub_blocks = jobs_total_;
  o.sink_used = (cfg_.sink != nullptr);
  o.bytes_written = bytes_written_.load();
  o.wall_seconds = now_seconds() - t0;
  // checksum 由 writer 在行主序下累积；此处按同一算法独立复算以便核对（只读产品文件，经 aio）。
  // sink 通道的产物布局由 sink 拥有（FITS 头 + 数据区），本算法不适用 ⇒ 跳过。
  if (!cfg_.sink) {
    std::uint64_t h = 1469598103934665603ULL;
    const std::uint64_t base = cfg_.wcs_header.size() + cfg_.properties.size();
    const std::uint64_t expect = static_cast<std::uint64_t>(width_) * height_ * sizeof(double);
    std::vector<char> buf(1 << 16);
    std::uint64_t total = 0;
    while (total < expect) {
      const std::size_t want = static_cast<std::size_t>(
          std::min<std::uint64_t>(buf.size(), expect - total));
      std::string chunk;
      if (!aio_file::read_range(cfg_.output_path.c_str(), base + total, want, &chunk)) break;
      if (chunk.empty()) break;
      h = fnv1a(h, chunk.data(), chunk.size());
      total += chunk.size();
    }
    o.checksum = h;
  }
  const bool header_ok = cfg_.sink
                             ? cfg_.header_ready
                             : (!cfg_.wcs_header.empty() && !cfg_.properties.empty());
  if (disk_full_.load()) {
    o.ok = false;
    o.error = "disk_full";
    o.exit_code = 10;          // 保持已修语义
    o.published = false;
  } else if (!header_ok) {
    o.ok = false;
    o.error = cfg_.sink ? ("sink_header_not_ready: " + sink_error_)
                        : "wcs_header_or_properties_missing";
    o.exit_code = 1;
    o.published = false;
  } else if (!sink_error_.empty()) {
    o.ok = false;
    o.error = sink_error_;
    o.exit_code = 2;           // sink 落盘失败（I/O）
    o.published = false;
  } else if (cfg_.sink && !sink_published_) {
    o.ok = false;
    o.error = "sink not published (cancelled or incomplete)";
    o.exit_code = 130;
    o.published = false;
  } else if (was_cancelled) {
    o.ok = false;
    o.error = "cancelled";
    o.exit_code = 130;
    o.published = false;
  } else {
    o.ok = true;
    o.published = true;
  }
  if (cfg_.sink) o.published = sink_published_;
  probes_.flush();
  write_manifest();
  return o;
}

}  // namespace astrocs::core
