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
//   ⑦ **生产接线（P3-STREAM-01）**：phase3 writer 节点经 `ExportSink` 把每个
//      子块写进 FITS 数据区（cfitsio `fits_write_subset`）；resample2 / verify
//      节点同样按子块读写 —— 生产 export 全程无整幅平面驻留
//      （ASTROCS_DESIGN §8.3 export 行）。
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
#include <vector>

namespace astrocs::core {

// 每像素取值函数（由调用方注入；本调度器只做流式编排，不做投影数学）
using ExportPixelFn = std::function<double(int x, int y)>;

// 子块产出函数（生产面；与 ExportPixelFn 二选一，二者皆空 ⇒ 子块填 0）。
// 调用方按子块矩形填充 data（w×h 个 double，行主序）—— 生产 export 用它把
// 「读子块 → 投影重采样」与「写子块」分离到调度器的算/写两级。
using ExportSubBlockFn = std::function<void(int x0, int y0, int w, int h, double* data)>;

// 子块汇（生产面）：调度器按**子块索引升序**把每个子块交给 sink 落盘，
// 顺序与 worker 数无关 ⇒ 输出逐位确定。sink 自行负责原子发布语义
// （open 建临时对象 → write_sub_block 追加 → finish 走 tmp→fsync→rename，
// abort 不留半成品）；调度器只做编排、背压与在途字节上界。
// 生产 FITS 面（astrocs.phase3.writer）经此接口把子块写进 FITS 数据区，
// 不再需要整幅平面驻留。
class ExportSink {
 public:
  virtual ~ExportSink() = default;
  // 建临时对象并完成**开写前的头组装**；失败返回 false（err 写原因）。
  virtual bool open(std::string* err) = 0;
  // 写一个子块（index = 行主序子块序号，单调递增；data = w×h 行主序）。
  virtual bool write_sub_block(std::size_t index, int x0, int y0, int w, int h,
                               const double* data, std::string* err) = 0;
  // 收尾并**原子发布**（全部子块已写出且校验通过才发布）。失败返回 false。
  virtual bool finish(std::string* err) = 0;
  // 取消/失败：关闭临时对象并删除，**不得**发布半成品。幂等。
  virtual void abort() = 0;
};

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
  // ── 生产面（sink 模式）────────────────────────────────────────────────
  // sink 非空 ⇒ 由 sink 落盘（wcs_header/properties 的齐备性由 sink 承接，
  // 调度器改为要求 header_ready == true）；sink 为空 ⇒ 内建原样通道（兼容面）。
  ExportSink* sink = nullptr;
  bool header_ready = false;         // 头已在首个像素写出前组装完成（合同③）
  ExportSubBlockFn sub_block_fn;     // 子块产出（sink 模式的生产算级）
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
  std::size_t peak_resident_bytes = 0;   // 在途子块缓冲峰值（调度器记账面）
  std::size_t max_queue_depth = 0;       // 三级队列水位峰值
  std::size_t sub_blocks = 0;            // 子块总数（= 提交的算级单元数）
  bool sink_used = false;                // 是否走 sink（生产 FITS）面
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
  void writer_loop_raw();    // 内建通道（WCS 头 + properties + 行主序 double 像素）
  void writer_loop_sink();   // 生产通道（ExportSink：子块写进 FITS 数据区）
  void signal_stop();        // 失败/取消统一收尾（不发布、不留半成品）

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
  // sink 通道（生产面）的落盘结果：published 只在 finish() 成功（原子 rename
  // 完成）后置位；error 供 run() 归因（不改 exit_code 语义）。
  bool sink_published_ = false;
  std::string sink_error_;

  // 线程池形态（RT-004 架构约束，不得回退）：本调度器**不持有**线程池成员。
  // 每次 run() 在实现文件内建立 run 作用域的**有界**池（worker 数 = cfg_.workers，
  // 配置注入），run 返回前全部 join 回收；无 detach、无常驻线程。
  // 头文件不得出现线程容器成员（生命周期不可控的永久池）。
};

}  // namespace astrocs::core
