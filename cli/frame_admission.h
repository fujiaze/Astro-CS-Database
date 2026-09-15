// cli/frame_admission.h — P36 D5: 帧级并发内存准入契约 (显式、可测、可审计)
// ---------------------------------------------------------------------------
// 依据: P24-sched-research REPORT §2.3 / §2.6 (K_mem 公式 + 实测峰值表) 与负责人
// 裁决 D5 —— "并发准入 = 内存 >= 2 x 单帧峰值 + reserve"。本头文件是该判据的
// **唯一实现点**, 纯函数、无副作用、可单测; 调用方 (commands.cpp 的
// run_with_resource_gate) 只负责把实测输入喂进来并把判定结果如实落盘。
//
// 契约 (冻结式声明):
//   R_reserve(MemTotal, K_active) = max(1 GiB, 5% * MemTotal) + 0.25 GiB * K_active
//   K_mem    = max(1, floor((MemTotal - R_reserve) / peak_p95))
//   放行并发: K_mem >= 2  且  MemAvailable >= 2 * peak_p95 + R_reserve
//            且  cpu_budget >= 2 * b_min
//   K_final  = min(K_mem, cpu_budget / b_min)
// 其中 peak_p95 = 同 (telescope, filter, nside) 历史单帧峰值 RSS 的 p95; 无历史时
// 调用方必须显式标注来源 (explicit/estimate/measured) —— 禁止把估计值冒充实测。
//
// 本机 (MemTotal 15.62 GiB) 生产分辨率 (T4 auto 单帧峰值 8.46 GB, P21 实测):
//   R_reserve = max(1 GiB, 0.78 GiB) + 0.25 GiB = 1.25 GiB
//   K_mem = floor((15.62 - 1.25) / 8.46) = 1  ⇒ memory_insufficient (不并发)
// nside=512 (单帧峰值 ~1.5 GB): K_mem ~ 8 ⇒ 内存侧不限制, 但帧并发编排不在 P36
// 范围内 (reason = frame_concurrency_not_wired), 本批只落地帧内节点重叠 (D1)。
// ---------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>

namespace astrocs {

inline constexpr uint64_t kGiB = 1024ull * 1024ull * 1024ull;
inline constexpr uint64_t kMiB = 1024ull * 1024ull;

// 峰值来源: 禁止把估计/缺省冒充实测 (与 §10.5 哨兵纪律同源)。
enum class FramePeakSource {
  Explicit = 0,          // 调用方显式给出 (--sched-frame-peak-bytes / 历史 p95)
  Estimate = 1,          // 由像素数下界估计 (仅供无历史时兜底, 明确标注)
  MeasuredThisRun = 2,   // 本次 run 实测进程峰值 RSS
};

inline const char* frame_peak_source_name(FramePeakSource s) {
  switch (s) {
    case FramePeakSource::Explicit: return "explicit";
    case FramePeakSource::Estimate: return "estimate";
    case FramePeakSource::MeasuredThisRun: return "measured_this_run";
  }
  return "unknown";
}

// D5 参数: 全部由调用方派生/实测注入, 无硬编码内存/核数常量。
struct FrameAdmissionParams {
  uint64_t mem_total_bytes = 0;        // /proc/meminfo MemTotal
  uint64_t mem_available_bytes = 0;    // 决策时点 MemAvailable
  uint64_t single_frame_peak_bytes = 0;  // 单帧峰值 RSS (p95 优先)
  FramePeakSource peak_source = FramePeakSource::Explicit;
  uint32_t cpu_budget = 1;             // 线程预算 B (affinity 派生)
  uint32_t min_workers_per_frame = 2;  // 每帧最小可用 worker b_min
  uint32_t active_frames = 1;          // 当前在途帧数 (reserve 的第二项)
};

struct FrameAdmissionDecision {
  uint64_t reserve_bytes = 0;
  uint32_t k_mem = 1;      // 内存允许的最大并发帧数
  uint32_t k_cpu = 1;      // 预算允许的最大并发帧数
  uint32_t k_final = 1;    // = min(k_mem, k_cpu)
  bool concurrent = false; // k_final >= 2
  const char* reason = "memory_insufficient";
};

// R_reserve: 固定 reserve (max(1 GiB, 5% MemTotal)) + 每在途帧 0.25 GiB 余量。
inline uint64_t frame_admission_reserve(uint64_t mem_total_bytes,
                                        uint32_t active_frames) {
  const uint64_t pct5 = mem_total_bytes / 20ull;
  const uint64_t fixed = pct5 > kGiB ? pct5 : kGiB;
  const uint64_t per_frame = static_cast<uint64_t>(active_frames) * (kGiB / 4ull);
  return fixed + per_frame;
}

// K_mem = max(1, floor((MemTotal - R_reserve) / peak)); peak=0 -> 1 (不可判定时
// 按最保守的单帧处理, 不放大并发)。
inline uint32_t frame_admission_k_mem(uint64_t mem_total_bytes,
                                      uint64_t peak_bytes,
                                      uint64_t reserve_bytes) {
  if (peak_bytes == 0) return 1;
  if (mem_total_bytes <= reserve_bytes) return 1;
  const uint64_t k = (mem_total_bytes - reserve_bytes) / peak_bytes;
  if (k < 1) return 1;
  if (k > 0xffffffffull) return 0xffffffffu;
  return static_cast<uint32_t>(k);
}

// 纯判定: 不修改任何全局状态; reason 取值:
//   "memory_insufficient"           K_mem < 2 (内存先到)
//   "cpu_budget_single"             cpu_budget < 2 * b_min
//   "available_below_two_frames"    K_mem>=2 但 MemAvailable < 2*peak+reserve
//   "frame_concurrency_not_wired"   内存/核均允许, 但本批未接帧并发编排
//   "admitted"                      允许并发 (K_final>=2)
inline FrameAdmissionDecision frame_admission_decide(
    const FrameAdmissionParams& p) {
  FrameAdmissionDecision d;
  d.reserve_bytes = frame_admission_reserve(p.mem_total_bytes, p.active_frames);
  d.k_mem = frame_admission_k_mem(p.mem_total_bytes, p.single_frame_peak_bytes,
                                  d.reserve_bytes);
  const uint32_t bmin = p.min_workers_per_frame > 0 ? p.min_workers_per_frame : 1u;
  d.k_cpu = p.cpu_budget / bmin;
  if (d.k_cpu < 1) d.k_cpu = 1;
  d.k_final = d.k_mem < d.k_cpu ? d.k_mem : d.k_cpu;
  if (d.k_mem < 2) {
    d.reason = "memory_insufficient";
    d.concurrent = false;
    return d;
  }
  if (d.k_cpu < 2) {
    d.reason = "cpu_budget_single";
    d.concurrent = false;
    return d;
  }
  // 放行第 2 帧的实时余量判据: 必须 >= 2*peak + reserve。
  const uint64_t need = 2ull * p.single_frame_peak_bytes + d.reserve_bytes;
  if (p.mem_available_bytes > 0 && p.mem_available_bytes < need) {
    d.reason = "available_below_two_frames";
    d.concurrent = false;
    return d;
  }
  // 内存与核均允许: P36 本批只落地帧内重叠 (D1), 帧间并发编排属后续批次。
  d.reason = "frame_concurrency_not_wired";
  d.concurrent = false;
  return d;
}

}  // namespace astrocs
