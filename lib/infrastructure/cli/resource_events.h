// astrocs 资源事件模块 (MON-002) — stage/resource/backend 事件 + summary + raw 指针。
// GATE-FIX-RES(R-4 D-14): 「summary|timeseries 分层档」已退役 —— 该档旗标
// --resource-detail 不在命令树白名单（真 CLI rc=2 「unknown flag」）, 且 timeseries
// 档只写恒空 curve_points, 唯一曲线构造器 build_curve() 生产零调用 ⇒ 删除, 避免
// 下游据 «curve_points 存在» 推断「有曲线」而读到空数组（静默错误信号）。
// **资源时序曲线的唯一载体 = 磁盘工件 resource_timeseries.csv**（summary 事件内嵌
// raw_dir/raw_n 指针, 不内嵌几十 MB 数据）。
// 07 §2 必采指标; stage 标注 compute|memory|io|mixed; 无标注 >5s 区间 = P1(MON-003 gating 用)。
// ABI 冻结(v1)不改公共 ABI; 本模块纯 CLI 侧事件组装。
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "monitor.h"

namespace astrocs {

// stage 标注(MON-002): 每个 stage 声明资源类别, 无标注的可由 MON-003 判 P1。
enum class StageKind { Compute, Memory, Io, Mixed, Unknown };

inline const char* stage_kind_name(StageKind k) {
    switch (k) {
    case StageKind::Compute: return "compute";
    case StageKind::Memory:  return "memory";
    case StageKind::Io:      return "io";
    case StageKind::Mixed:   return "mixed";
    default:                 return "unknown";
    }
}

// 摘要报告: summary(必) + raw 计数(原始时序目录路径, 不内嵌数据)。
struct ResourceReport {
    struct SummaryPayload {
        double avg_equivalent_cores = 0.0;
        double peak_equivalent_cores = 0.0;
        uint64_t peak_rss_bytes = 0;
        int64_t rss_slope_bytes_per_s = 0;
        uint64_t total_read_bytes = 0;
        uint64_t total_write_bytes = 0;
        uint64_t total_ctx_switches = 0;
        uint32_t max_threads = 0;
        double wall_seconds = 0.0;
        double sample_overhead_ms = 0.0;
        uint64_t n_samples = 0;
    } summary;
    std::string raw_dir;             // 原始时序留存路径(节点)
    // 曲线不再内嵌事件: 唯一载体 = raw_dir/resource_timeseries.csv
    // (GATE-FIX-RES / R-4 D-14/D-15)。
    std::size_t raw_n = 0;           // 原始样本数(入库, 但不内嵌数据)
};

// 从 ProcessMonitor 构建摘要(供 emit resource summary 事件)。
inline ResourceReport::SummaryPayload summarize(const ProcessMonitor::Summary& s) {
    ResourceReport::SummaryPayload p;
    p.avg_equivalent_cores = s.avg_equivalent_cores;
    p.peak_equivalent_cores = s.peak_equivalent_cores;
    p.peak_rss_bytes = s.peak_rss_bytes;
    p.rss_slope_bytes_per_s = s.rss_slope_bytes_per_s;
    p.total_read_bytes = s.total_read_bytes;
    p.total_write_bytes = s.total_write_bytes;
    p.total_ctx_switches = s.total_ctx_switches;
    p.max_threads = s.max_threads;
    p.wall_seconds = s.wall_seconds;
    p.sample_overhead_ms = s.sample_overhead_ms;
    p.n_samples = s.n_samples;
    return p;
}

// 判定一个 stage 资源类别(供事件标注; mixed 拆份见 MON-003)。
inline StageKind classify_stage(const char* ann) {
    if (ann == nullptr) return StageKind::Unknown;
    const std::string a(ann);
    if (a == "compute") return StageKind::Compute;
    if (a == "memory")  return StageKind::Memory;
    if (a == "io")      return StageKind::Io;
    if (a == "mixed")   return StageKind::Mixed;
    return StageKind::Unknown;
}

// 无标注(Unknown)且 wall>阈值 → 该 stage 为 P1(未标注, MON-003 判失败用)。
inline bool is_unannotated_priority(const char* ann, double wall_seconds,
                                    double threshold_seconds = 5.0) {
    return classify_stage(ann) == StageKind::Unknown && wall_seconds > threshold_seconds;
}

}  // namespace astrocs
