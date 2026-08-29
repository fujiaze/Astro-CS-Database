// astrocs 资源利用率门禁 (MON-003) — 07 §3/§4/§5 分类+公式+first-10s 快速失败+exit 10+诊断分类
// ABI 冻结(v1)不改公共 API; 本模块纯 CLI 侧 gate 判定。硬编码禁令: 线程数/cpus 由调用方注入。
//
// 分类(07 §3): compute | memory | io | mixed(不得用 mixed 掩盖低利用率)。
// compute 门禁: 有线程利用(selected_workers/max_active_threads) + avg_equivalent_cores 达标
//                + 非"CPU/io/mem 皆低"。
// memory 门禁: 允许 CPU 未满, 但须达 pre-frozen 带宽比例(由 BENCH-003 写入)。
// io 门禁: 允许低 CPU, 但须有 bytes/ops/await 证据。
// mixed 门禁: 必须拆出 compute/io 子区间。
//
// first-10s 快速失败(07 §4): 首 10s 若"低 CPU + 非 I/O + 非内存带宽饱和" → gate 返回 FAIL。
// 门禁失败 → exit 10(RESOURCE)。诊断分类: 给出具体 failure_kind。
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {

// 资源类别(MON-002 StageKind 同义; MON-003 用于门禁分类与公式选择)。
enum class ResKind { Compute, Memory, Io, Mixed, Unknown };

inline const char* res_kind_name(ResKind k) {
    switch (k) {
    case ResKind::Compute: return "compute";
    case ResKind::Memory:  return "memory";
    case ResKind::Io:      return "io";
    case ResKind::Mixed:   return "mixed";
    default:               return "unknown";
    }
}

// 诊断分类(门禁失败的具体原因; 单值 e 枚举, 记录到事件)。
enum class GateDiag {
    Ok,
    SingleThreaded,          // selected_workers < 2 (available>=2 时)
    LowAvgCores,             // avg_equivalent_cores < 0.80*min(selected_workers,available)
    UnannotatedPriority,     // 无 stage 标注且 wall>5s(P1, 07 §1)
    ComputeIoMemAllLow,      // CPU/io/mem 带宽皆低(禁止"单线程算法正常"解释)
    MemoryBandwidthLow,      // memory 未达 pre-frozen 带宽比例
    IoMissingEvidence,       // io 缺 bytes/ops/await 证据
    MixedUnsplit,            // mixed 未拆出 compute/io 子区间
    FastFailFirst10s,        // 首 10s 低 CPU+非 IO+非内存带宽饱和 → 协作取消(07 §4)
    GlobalLockDegradation,   // N-worker 相对 1-worker 无正向加速(全局锁退化)
};

inline const char* gate_diag_name(GateDiag d) {
    switch (d) {
    case GateDiag::Ok:                      return "ok";
    case GateDiag::SingleThreaded:          return "single_threaded";
    case GateDiag::LowAvgCores:             return "low_avg_cores";
    case GateDiag::UnannotatedPriority:     return "unannotated_priority";
    case GateDiag::ComputeIoMemAllLow:      return "compute_io_mem_all_low";
    case GateDiag::MemoryBandwidthLow:      return "memory_bandwidth_low";
    case GateDiag::IoMissingEvidence:       return "io_missing_evidence";
    case GateDiag::MixedUnsplit:            return "mixed_unsplit";
    case GateDiag::FastFailFirst10s:        return "fast_fail_first_10s";
    case GateDiag::GlobalLockDegradation:   return "global_lock_degradation";
    default:                                return "unknown";
    }
}

struct GateConfig {
    ResKind kind = ResKind::Unknown;
    uint32_t available_cpus = 0;
    uint32_t selected_workers = 0;
    uint32_t max_active_threads = 0;
    double avg_equivalent_cores = 0.0;
    double wall_seconds = 0.0;
    bool has_stage_annotation = false;
    // memory 门禁: 预冻结吞吐比例(BENCH-003 写入, 不外推/不事后改)。
    double achieved_memory_bandwidth_frac = -1.0;   // -1=未测量
    double required_memory_bandwidth_frac = 0.0;    // pre-frozen threshold
    // io 门禁: 证据
    uint64_t io_bytes = 0;      // read+write bytes
    uint64_t io_ops = 0;        // read+write ops
    double io_await_ms = 0.0;   // 可得时
    bool io_is_short_serial = false;  // 短于5s 或 <5% 时长的串行 IO(非阻塞)
    // mixed 子区间拆份: 已拆出? 或仍单一 mixed
    bool mixed_has_compute_subrange = false;
    bool mixed_has_io_subrange = false;
    // N-worker vs 1-worker 合成加速(07 §3 全局锁校验)
    double one_worker_ns = -1.0;
    double n_worker_ns = -1.0;
    // first-10s 快照(07 §4 快速失败)
    bool first10s_low_cpu = false;
    bool first10s_non_io = false;                 // 非 IO 密集
    bool first10s_mem_not_saturated = false;
    double cpu_percent = 0.0;       // 可用于诊断的 CPU%
    double iowait_percent = 0.0;    // 可得时
    double mem_bandwidth_percent = -1.0;  // -1=未测
};

// 核心公式: compute 且 wall>=5s 时, avg_equivalent_cores 下限 = 0.80 * min(selected_workers, available_cpus)。
inline double compute_cores_threshold(const GateConfig& g) {
    if (g.kind != ResKind::Compute) return 0.0;
    const uint32_t m = std::min(g.selected_workers, g.available_cpus);
    return (m >= 1 ? 0.80 * static_cast<double>(m) : 0.0);
}

// 逐项门禁判定; 返回 GateDiag(Ok 表示通过)。顺序: 使首错可诊断。
inline GateDiag evaluate_gate(const GateConfig& g) {
    // mixed 必须拆份(07 §3): 未拆 start+emit 就 FAIL
    if (g.kind == ResKind::Mixed) {
        if (!g.mixed_has_compute_subrange || !g.mixed_has_io_subrange)
            return GateDiag::MixedUnsplit;
        // 拆份后按 compute 子区间继续判; 此处视为通过(compute 子区判据由调用方另行)
        return GateDiag::Ok;
    }
    // 无标注 >5s 区间 → P1(07 §1)
    if (!g.has_stage_annotation && g.wall_seconds > 5.0)
        return GateDiag::UnannotatedPriority;

    if (g.kind == ResKind::Compute) {
        if (g.wall_seconds < 5.0) return GateDiag::Ok;  // 短任务豁免
        if (g.available_cpus >= 2 && g.selected_workers < 2)
            return GateDiag::SingleThreaded;
        if (g.available_cpus >= 2 && g.max_active_threads < 2)
            return GateDiag::SingleThreaded;
        const double thr = compute_cores_threshold(g);
        if (thr > 0 && g.avg_equivalent_cores < thr)
            return GateDiag::LowAvgCores;
        // CPU/io/mem 皆低 → 禁止"单线程算法正常"解释(07 §3)
        if (g.cpu_percent < 20.0 && g.iowait_percent < 5.0 &&
            (g.mem_bandwidth_percent < 0.0 || g.mem_bandwidth_percent < 15.0))
            return GateDiag::ComputeIoMemAllLow;
        // N-worker 相对 1-worker 正向加速(全局锁退化)→ FAIL
        if (g.one_worker_ns > 0 && g.n_worker_ns > 0 && g.n_worker_ns > g.one_worker_ns)
            return GateDiag::GlobalLockDegradation;
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Memory) {
        if (g.achieved_memory_bandwidth_frac < 0.0)
            return GateDiag::MemoryBandwidthLow;   // 未测量=FAIL(须证明)
        if (g.achieved_memory_bandwidth_frac < g.required_memory_bandwidth_frac)
            return GateDiag::MemoryBandwidthLow;
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Io) {
        // 短于5s 或 <5% 时长的串行 IO 不作为发布阻塞(07 §3)
        if (g.io_is_short_serial) return GateDiag::Ok;
        if (g.io_bytes == 0 && g.io_ops == 0 && g.io_await_ms <= 0.0)
            return GateDiag::IoMissingEvidence;    // 允许低 CPU, 但须证据
        return GateDiag::Ok;
    }
    if (g.kind == ResKind::Unknown) {
        // 未知类别: 若未标注且>5s 则 P1; 否则不阻塞(留给上层确认)
        if (!g.has_stage_annotation && g.wall_seconds > 5.0)
            return GateDiag::UnannotatedPriority;
        return GateDiag::Ok;
    }
    return GateDiag::Ok;
}

// first-10s 快速失败(07 §4): 首 10s 低 CPU + 非 IO + 非内存饱和 → 协作取消, exit 10。
inline bool fast_fail_first10s(const GateConfig& g) {
    return g.first10s_low_cpu && g.first10s_non_io && g.first10s_mem_not_saturated;
}

// 诊断字符串: 失败判定给出可操作说明。
inline std::string diag_message(GateDiag d, const GateConfig& g) {
    switch (d) {
    case GateDiag::SingleThreaded:
        return "compute 门禁: available_cpus>=2 但 selected_workers/max_active_threads<2";
    case GateDiag::LowAvgCores: {
        const double thr = compute_cores_threshold(g);
        return "compute 门禁: avg_equivalent_cores " + std::to_string(g.avg_equivalent_cores) +
               " < 0.80*min(workers,cpus)=" + std::to_string(thr);
    }
    case GateDiag::UnannotatedPriority:
        return "stage 未标注且 wall>5s → P1(07 §1)";
    case GateDiag::ComputeIoMemAllLow:
        return "CPU/io/mem 带宽皆低: 禁止'单线程算法正常'解释(07 §3)";
    case GateDiag::MemoryBandwidthLow:
        return "memory 门禁: 带宽比例未达 pre-frozen 阈值(须 BENCH-003 写入)";
    case GateDiag::IoMissingEvidence:
        return "io 门禁: 缺 bytes/ops/await 证据(允许低 CPU 但须证据)";
    case GateDiag::MixedUnsplit:
        return "mixed 门禁: 未拆出 compute/io 子区间(不得用 mixed 掩盖低利用率)";
    case GateDiag::FastFailFirst10s:
        return "first-10s 快速失败: 低 CPU+非 IO+非内存带宽饱和(07 §4)";
    case GateDiag::GlobalLockDegradation:
        return "compute 门禁: N-worker 相对 1-worker 无正向加速(全局锁退化)";
    default: return "ok";
    }
}

}  // namespace astrocs
