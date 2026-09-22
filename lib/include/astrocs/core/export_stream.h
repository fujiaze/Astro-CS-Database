// AstroCS Core — ARCH-504 export 子块流式调度器
//
// 依据：ASTROCS_DESIGN.md §6（投影导出）、§8.3（export = 子块流式）；ENGINEERING_SPEC.md
//       §4.1；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md。
//
// 核心机制（合同条款，测试逐条锁）：
//   ① 输出平面按子块划分，HiPS 读取按子块范围路由（只读必要切片）；
//   ② **三级有界流水线**（读 / 算 / 写）+ 背压：任一级队列满则上游阻塞，不无界增长；
//   ③ WCS 头与 properties 在**开写前**组装完成（不边写边补头）；
//   ④ 原子发布：临时目录 → 完整写出 → 原子 rename；磁盘满 ⇒ exit 10（保持已修语义），
//      **不得**留下半成品；
//   ⑤ RSS 与**子块大小**成正比、与**总图大小无关**；
//   ⑥ 探针：每子块耗时、I/O 等待、队列水位。
//
// 边界：投影数学（TAN 冻结）不改；SIN 缺陷未解决前不启用。
#pragma once

#include "astrocs/core/normalize_workflow.h"   // 复用 ProbeSink / ProbeEvent

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace astrocs::core {

// 每像素取值函数（由调用方注入；本调度器只做流式编排，不做投影数学）
using ExportPixelFn = std::function<double(int x, int y)>;

struct ExportStreamConfig {
  int workers = 1;                   // 计算并发（配置注入）
  int sub_block_px = 256;            // 子块边长（配置参数，写 manifest）
  int queue_depth = 4;               // 三级队列深度（背压上限）
  std::uint64_t memory_limit_bytes = 0;
  std::string probe_path;            // 探针 JSONL
  std::string manifest_path;         // manifest（子块大小/队列深度/WCS 头状态）
  std::string output_path;           // 最终产品路径（原子发布目标）
  std::string wcs_header;            // 开写前组装好的 WCS 头（不边写边补）
  std::string properties;            // 开写前组装好的 properties
  // 测试注入：写到第 N 字节后模拟磁盘满（0 = 不注入）
  std::uint64_t fail_write_after_bytes = 0;
};

struct ExportOutcome {
  bool ok = false;
  std::string error;
  int exit_code = 0;                 // 0 = 正常；10 = 磁盘满（已修语义）
  std::uint64_t checksum = 0;        // 按行主序（子块索引序 + 块内行主序）累积
  std::uint64_t pixels = 0;
  std::uint64_t bytes_written = 0;
  std::size_t peak_resident_bytes = 0;   // 在途子块缓冲峰值
  std::size_t max_queue_depth = 0;       // 三级队列水位峰值
  bool published = false;                // 是否完成原子发布
  double wall_seconds = 0.0;
};

class ExportStreamScheduler {
 public:
  explicit ExportStreamScheduler(ExportStreamConfig cfg);
  ~ExportStreamScheduler();
  ExportStreamScheduler(const ExportStreamScheduler&) = delete;
  ExportStreamScheduler& operator=(const ExportStreamScheduler&) = delete;

  void set_image(int width, int height) { width_ = width; height_ = height; }
  void set_pixel_fn(ExportPixelFn fn) { pixel_fn_ = std::move(fn); }

  ExportOutcome run();
  void cancel() { cancel_.store(true); cv_space_.notify_all(); cv_item_.notify_all(); }

  ProbeSink& probes() { return probes_; }
  std::size_t sub_block_count() const;
  void write_manifest() const;

 private:
  struct Job { std::size_t index; int x0, y0, w, h; };
  struct Result {
    std::size_t index;
    int x0, y0, w, h;
    std::vector<double> data;
  };

  void reader_loop();
  void compute_loop();
  void writer_loop();

  ExportStreamConfig cfg_;
  ProbeSink probes_;
  int width_ = 0, height_ = 0;
  ExportPixelFn pixel_fn_;

  std::deque<Job> q_jobs_;       // 读 → 算（有界）
  std::deque<Result> q_results_; // 算 → 写（有界）
  std::mutex mu_;
  std::condition_variable cv_space_, cv_item_, cv_done_;
  std::atomic<bool> cancel_{false};
  std::size_t jobs_total_ = 0;
  std::size_t jobs_produced_ = 0;
  std::size_t results_done_ = 0;
  std::size_t max_queue_ = 0;
  std::size_t peak_resident_ = 0;
  // 在途子块字节 / 在途子块数：**读写都在 mu_ 之下**（reader 持锁递增，writer 持锁递减；
  // ACCEPT-501 P-1 修掉此前 writer 侧无锁递减造成的竞争与无符号下溢）。背压唯一施加点
  // 是 reader 的 inflight_count_ < 2*queue_depth 判据。
  std::size_t inflight_bytes_ = 0;
  std::size_t inflight_count_ = 0;
  std::atomic<std::uint64_t> bytes_written_{0};
  std::atomic<bool> disk_full_{false};

  std::vector<std::thread> pool_;
};

}  // namespace astrocs::core
