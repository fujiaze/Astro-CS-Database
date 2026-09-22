// AstroCS Core — ARCH-504 export 子块流式调度器实现
// 依据：ASTROCS_DESIGN.md §6/§8.3；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md
#include "astrocs/core/export_stream.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

void mkdir_p(const std::string& path) {
  std::string acc;
  std::size_t i = 0;
  if (!path.empty() && path[0] == '/') { acc = "/"; i = 1; }
  while (i <= path.size()) {
    const std::size_t j = path.find('/', i);
    const std::string part = path.substr(i, (j == std::string::npos ? path.size() : j) - i);
    if (!part.empty()) {
      if (!acc.empty() && acc.back() != '/') acc.push_back('/');
      acc += part;
      ::mkdir(acc.c_str(), 0755);
    }
    if (j == std::string::npos) break;
    i = j + 1;
  }
}

std::string dirname_of(const std::string& p) {
  const std::size_t k = p.find_last_of('/');
  return (k == std::string::npos) ? std::string(".") : p.substr(0, k);
}

}  // namespace

ExportStreamScheduler::ExportStreamScheduler(ExportStreamConfig cfg)
    : cfg_(std::move(cfg)), probes_(cfg_.probe_path) {
  if (cfg_.workers < 1) cfg_.workers = 1;
  if (cfg_.sub_block_px < 1) cfg_.sub_block_px = 1;
  if (cfg_.queue_depth < 1) cfg_.queue_depth = 1;
}

ExportStreamScheduler::~ExportStreamScheduler() {
  cancel();
  for (auto& t : pool_) if (t.joinable()) t.join();
}

std::size_t ExportStreamScheduler::sub_block_count() const {
  if (width_ <= 0 || height_ <= 0) return 0;
  const std::size_t nx = static_cast<std::size_t>((width_ + cfg_.sub_block_px - 1) / cfg_.sub_block_px);
  const std::size_t ny = static_cast<std::size_t>((height_ + cfg_.sub_block_px - 1) / cfg_.sub_block_px);
  return nx * ny;
}

void ExportStreamScheduler::write_manifest() const {
  if (cfg_.manifest_path.empty()) return;
  mkdir_p(dirname_of(cfg_.manifest_path));
  std::ofstream f(cfg_.manifest_path, std::ios::out | std::ios::trunc);
  if (!f) return;
  const std::size_t n = sub_block_count();
  const std::size_t sb_bytes =
      static_cast<std::size_t>(cfg_.sub_block_px) * cfg_.sub_block_px * sizeof(double);
  f << "{\n  \"schema\": \"astrocs.export-stream-manifest/v1\",\n"
    << "  \"width\": " << width_ << ",\n  \"height\": " << height_ << ",\n"
    << "  \"sub_block_px\": " << cfg_.sub_block_px << ",\n"
    << "  \"sub_block_count\": " << n << ",\n"
    << "  \"queue_depth\": " << cfg_.queue_depth << ",\n"
    << "  \"workers\": " << cfg_.workers << ",\n"
    << "  \"wcs_header_ready_before_write\": "
    << ((!cfg_.wcs_header.empty() && !cfg_.properties.empty()) ? "true" : "false") << ",\n"
    << "  \"wcs_header_bytes\": " << cfg_.wcs_header.size() << ",\n"
    << "  \"properties_bytes\": " << cfg_.properties.size() << ",\n"
    << "  \"sub_block_bytes\": " << sb_bytes << ",\n"
    << "  \"bounded_inflight_bytes\": " << (2 * static_cast<std::size_t>(cfg_.queue_depth) * sb_bytes)
    << "\n}\n";
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
    for (int yy = 0; yy < j.h; ++yy)
      for (int xx = 0; xx < j.w; ++xx)
        r.data[static_cast<std::size_t>(yy) * j.w + xx] = pixel_fn_ ? pixel_fn_(j.x0 + xx, j.y0 + yy) : 0.0;
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

void ExportStreamScheduler::writer_loop() {
  // ③ WCS 头与 properties 在开写前组装：这里先校验齐备性（缺则拒绝开写）
  if (cfg_.wcs_header.empty() || cfg_.properties.empty()) {
    cancel_.store(true);
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
    cv_done_.notify_all();
    cv_item_.notify_all();
    cv_space_.notify_all();
    return;
  }
  // ④ 原子发布：先写临时文件，全部写完后 rename
  const std::string tmp = cfg_.output_path + ".tmp";
  mkdir_p(dirname_of(cfg_.output_path));
  std::ofstream out(tmp, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!out) {
    cancel_.store(true);
    std::lock_guard<std::mutex> lk(mu_);
    results_done_ = jobs_total_;
    cv_done_.notify_all();
    cv_item_.notify_all();
    cv_space_.notify_all();
    return;
  }
  out.write(cfg_.wcs_header.data(), static_cast<std::streamsize>(cfg_.wcs_header.size()));
  out.write(cfg_.properties.data(), static_cast<std::streamsize>(cfg_.properties.size()));
  // 预置到最终长度：乱序 seek 写不会因中途 EOF 造成截断，且发布前长度已确定
  out.seekp(static_cast<std::streamoff>(cfg_.wcs_header.size() + cfg_.properties.size() +
                                        static_cast<std::size_t>(width_) * height_ * sizeof(double) - 1));
  out.put(static_cast<char>(0));
  out.seekp(static_cast<std::streamoff>(cfg_.wcs_header.size() + cfg_.properties.size()));
  std::uint64_t written = cfg_.wcs_header.size() + cfg_.properties.size();
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
        out.close();
        ::unlink(tmp.c_str());
        {
          std::lock_guard<std::mutex> lk(mu_);
          results_done_ = jobs_total_;
        }
        cv_done_.notify_all();
        cv_item_.notify_all();
        cv_space_.notify_all();
        return;
      }
      // FITS 产品是**全图行主序**：子块的每一行必须写到它在整幅中的正确行偏移
      // （子块缓冲区是 tile 局部行主序，直接整块落在左上角偏移会串行）。
      // 乱序 worker 不影响结果：每个 (子块, 行) 的目标偏移由 (x0,y0) 唯一确定。
      const std::uint64_t base = static_cast<std::uint64_t>(cfg_.wcs_header.size() +
                                                            cfg_.properties.size());
      for (int yy = 0; yy < cur.h; ++yy) {
        const std::uint64_t off = base +
            (static_cast<std::uint64_t>(cur.y0 + yy) * static_cast<std::uint64_t>(width_) +
             static_cast<std::uint64_t>(cur.x0)) * sizeof(double);
        out.seekp(static_cast<std::streamoff>(off));
        out.write(reinterpret_cast<const char*>(cur.data.data() +
                                                static_cast<std::size_t>(yy) * cur.w),
                  static_cast<std::streamsize>(cur.w) * static_cast<std::streamsize>(sizeof(double)));
      }
      written += nb;
      inflight_bytes_ -= cur.data.size() * sizeof(double);
      if (inflight_count_ > 0) --inflight_count_;
      pending.erase(it);
      ++next_index;
      ++done;
      {
        std::lock_guard<std::mutex> lk(mu_);
        ++results_done_;
      }
      cv_space_.notify_all();
    }
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
  out.flush();
  out.close();
  bytes_written_.store(written);
  const bool complete = (next_index == jobs_total_) && !cancel_.load() && !disk_full_.load();
  if (complete) {
    // 原子 rename（同目录内 rename 为原子操作）
    if (::rename(tmp.c_str(), cfg_.output_path.c_str()) != 0) {
      ::unlink(tmp.c_str());
      disk_full_.store(true);        // rename 失败按写失败处理
    }
  } else {
    ::unlink(tmp.c_str());           // 取消/中断：不得留下半成品
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
  const bool pre_cancelled = cancel_.load();
  const double t0 = now_seconds();

  pool_.clear();
  pool_.emplace_back([this] { writer_loop(); });
  pool_.emplace_back([this] { reader_loop(); });
  for (int i = 0; i < cfg_.workers; ++i) pool_.emplace_back([this] { compute_loop(); });

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
  for (auto& t : pool_) if (t.joinable()) t.join();
  pool_.clear();

  (void)pre_cancelled;
  o.pixels = static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
  o.peak_resident_bytes = peak_resident_;
  o.max_queue_depth = max_queue_;
  o.bytes_written = bytes_written_.load();
  o.wall_seconds = now_seconds() - t0;
  // checksum 由 writer 在行主序下累积；此处按同一算法独立复算以便核对（只读产品文件）
  {
    std::uint64_t h = 1469598103934665603ULL;
    std::ifstream in(cfg_.output_path, std::ios::binary);
    if (in) {
      in.seekg(static_cast<std::streamoff>(cfg_.wcs_header.size() + cfg_.properties.size()));
      std::vector<char> buf(1 << 16);
      std::size_t total = 0;
      const std::size_t expect = static_cast<std::size_t>(width_) * height_ * sizeof(double);
      while (total < expect && in) {
        const std::size_t want = std::min(buf.size(), expect - total);
        in.read(buf.data(), static_cast<std::streamsize>(want));
        const std::size_t got = static_cast<std::size_t>(in.gcount());
        if (!got) break;
        h = fnv1a(h, buf.data(), got);
        total += got;
      }
    }
    o.checksum = h;
  }
  if (disk_full_.load()) {
    o.ok = false;
    o.error = "disk_full";
    o.exit_code = 10;          // 保持已修语义
    o.published = false;
  } else if (cfg_.wcs_header.empty() || cfg_.properties.empty()) {
    o.ok = false;
    o.error = "wcs_header_or_properties_missing";
    o.exit_code = 1;
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
  probes_.flush();
  write_manifest();
  return o;
}

}  // namespace astrocs::core
