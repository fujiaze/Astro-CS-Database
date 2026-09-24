// ACSD Core — ARCH-503 mosaic 天球窗口并行调度器
//
// 依据：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器）、§8.3（mosaic = 空间窗口
//       并行）、§5；ENGINEERING_SPEC.md §4.1；CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md。
//
// 核心机制（合同条款，测试逐条锁）：
//   ① 窗口划分：按 HiPS 层级把天区切成固定大小窗口（tile 组），**窗口大小是配置参数**
//      并写入 manifest（合同 §3 资源声明）；
//   ② 输入帧块按窗口路由：每个窗口只读它真正需要的 tile 切片，消除「为每个窗口读整帧」
//      的读放大（RELEASE-04 实测约 3.9 GB/帧）；
//   ③ 窗口内顺序固定：coverage → UPM → 排异 → SNR² 集成；窗口间无共享可变状态；
//   ④ 稠密 SNR **现场求值**：用到哪个像素算哪个（稀疏-稠密等价已证 3.1e-15），
//      不预计算稠密面、不驻留；
//   ④b 窗口峰值驻留 = **实测值**：修复前该值被写成
//      编译期常量 sizeof(double)*4 = 32 B，是写入 peak 的**唯一来源** ⇒ 相关判据恒真、
//      无证据资格。现行实现按窗口真实存活分配逐次记账（输出像素缓冲 + 路由指针向量的
//      capacity() 字节），峰值取循环内最大值 ⇒ 判据可红可绿。
//   ⑤ 归约顺序冻结（窗口 ID 升序、窗口内像素序升序）⇒ 1/N worker **逐位一致**；
//   ⑥ 探针按窗口记录耗时 / 输入量 / worker 均衡。
//
// 边界：不改排异档位与科学口径（SCI-502 已修 UPM）；最终并行度调优归 PERF-501。
#pragma once

#include "astrocs/core/normalize_workflow.h"   // 复用 ProbeSink / ProbeEvent

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs::core {

// 帧的 tile 覆盖（aio 切片粒度：tile_ipix → 该 tile 的字节数）
struct MosaicFrameInput {
  std::uint64_t frame_id = 0;
  std::map<std::uint64_t, std::uint64_t> tile_bytes;
  std::uint64_t total_bytes() const {
    std::uint64_t n = 0;
    for (const auto& kv : tile_bytes) n += kv.second;
    return n;
  }
};

struct MosaicWindowConfig {
  int workers = 1;                    // 窗口并发度（配置注入，禁止硬编码）
  int hips_level = 0;                 // 窗口划分所用的 HiPS 层级
  int window_tiles = 4;               // 每个窗口包含的 tile 数（窗口大小）
  std::uint64_t memory_limit_bytes = 0;
  std::string probe_path;             // 探针 JSONL
  std::string manifest_path;          // 窗口划分 manifest（窗口大小必须落盘）
};

struct MosaicWindow {
  std::uint64_t window_id = 0;
  std::vector<std::uint64_t> tile_ipix;   // 升序
  std::uint64_t routed_bytes = 0;         // 按窗口路由后的输入量
  std::uint64_t naive_bytes = 0;          // 朴素基线：为每个窗口读整帧
};

struct WindowOutcome {
  std::uint64_t window_id = 0;
  bool ok = false;
  std::string error;
  std::uint64_t checksum = 0;        // 窗口内像素级确定性校验和
  std::uint64_t read_bytes = 0;      // 实际读取字节
  std::size_t peak_bytes = 0;        // 窗口内峰值驻留
  std::int64_t pixels = 0;
  // 按 (tile 升序, 像素升序) 的像素值序列——用于与单窗口参考实现做逐位等价比对
  // （窗口划分不改变每个 (tile,px) 的取值，故拼接后必须逐位相同）。
  std::vector<double> pixel_values;
  double wall_seconds = 0.0;
};

// ── 窗口峰值驻留判据（可红可绿） ──
// 修复前该维度的判据断言的是编译期常量（恒真 ⇒ AGENTS §5「恒真门没有证据资格」）。
// 现行判据作用在**实测样本**上，四条同时成立才绿：
//   ① 每个样本 peak_bytes > 0                          —— 防"常数 0/常量冒充驻留"
//   ② 每个样本 peak_bytes <= bound_bytes（解析上界）    —— 实测驻留受上界约束
//   ③ 同 window_tiles、不同 tiles_total ⇒ peak 相等     —— 驻留与**总图规模**解耦
//   ④ 两个窗口都装满时，window_tiles 更大 ⇒ peak **严格更大** —— 窗口大小是显式内存
//      权衡参数（§8.3）。「严格」是反恒真的关键：编译期常量与任何未记账的常量驻留在
//      这里必然违反（「不减」会被常量满足 ⇒ 退化判据）。
// 返回 true = 绿；why 非空 = 失败原因（机器可读，逐条给出违反项）。
struct WindowPeakSample {
  int window_tiles = 0;          // 窗口大小（tile/窗口）
  std::size_t tiles_total = 0;   // 全图 tile 数
  std::size_t peak_bytes = 0;    // 实测窗口峰值驻留
  std::size_t bound_bytes = 0;   // 解析上界（window_peak_bound_bytes）
};
bool window_peak_residency_ok(const std::vector<WindowPeakSample>& samples,
                              std::string* why);

class MosaicWindowScheduler {
 public:
  explicit MosaicWindowScheduler(MosaicWindowConfig cfg);
  ~MosaicWindowScheduler();
  MosaicWindowScheduler(const MosaicWindowScheduler&) = delete;
  MosaicWindowScheduler& operator=(const MosaicWindowScheduler&) = delete;

  void add_frame(MosaicFrameInput f);

  // 窗口划分（确定性：tile 升序、按 window_tiles 切块）
  std::vector<MosaicWindow> partition() const;
  std::size_t tile_count() const;
  void write_manifest() const;

  std::vector<WindowOutcome> run();
  void cancel() { cancel_.store(true); cv_.notify_all(); }

  ProbeSink& probes() { return probes_; }
  std::uint64_t total_read_bytes() const { return total_read_.load(); }
  std::uint64_t naive_baseline_bytes() const { return naive_baseline_.load(); }
  std::size_t peak_inflight_bytes() const { return peak_inflight_.load(); }
  // 读放大 = 实际读取 / 朴素基线（为每个窗口读整帧）
  double read_amplification() const;

  // 单窗口驻留的**解析上界**（与 run_window 的实测口径同源；判据②用，不参与运行调度）。
  //   = 2 × tiles_in_window × kPixelsPerTile × sizeof(double)   （输出像素缓冲；容量增长因子 2 余量）
  //   + 2 × frames_covering × sizeof(void*)                     （relevant/cover 指针向量）
  //   + 64                                                      （标量/局部余量）
  static std::size_t window_peak_bound_bytes(std::size_t tiles_in_window,
                                             std::size_t frames_covering);

 private:
  void worker_loop(int worker_index);
  WindowOutcome run_window(const MosaicWindow& w, int worker_index);

  MosaicWindowConfig cfg_;
  ProbeSink probes_;
  std::vector<MosaicFrameInput> frames_;
  std::vector<std::uint64_t> tiles_;        // 全体 tile 升序
  std::vector<MosaicWindow> windows_;
  std::vector<WindowOutcome> outcomes_;

  // 线程池形态（RT-004 架构约束，不得回退）：本调度器**不持有**线程池成员。
  // 每次 run() 在实现文件内建立 run 作用域的**有界**池（worker 数 = cfg_.workers，
  // 配置注入），run 返回前全部 join 回收；无 detach、无常驻线程。
  std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable cv_done_;
  std::size_t next_ = 0;
  std::size_t completed_ = 0;
  std::atomic<bool> cancel_{false};
  std::atomic<std::uint64_t> total_read_{0};
  std::atomic<std::uint64_t> naive_baseline_{0};
  std::atomic<std::size_t> peak_inflight_{0};
};

}  // namespace astrocs::core
