// astrocs 资源事件分层模块 (MON-002) — stage/resource/backend 事件 + summary/downsample/raw 分层
// 07 §1: resource-detail summary|timeseries; summary 强制存在; raw timeseries 留节点, 审核只放摘要+降采样。
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

// 降采样目标点数(审核摘要小型化: 曲线 ≤ 121 点, 禁止原始几十 MB 打包)。
constexpr std::size_t kDownsampleMax = 121;

// 降采样: 从 bucket 数量 N 的序列抽 ≤ kDownsampleMax 个代表(首尾保留, 均匀抽取)。
template <typename T>
inline std::vector<T> downsample_curve(const std::vector<T>& src, std::size_t max_points) {
    if (src.size() <= max_points) return src;
    std::vector<T> out;
    out.reserve(max_points);
    const std::size_t n = src.size();
    for (std::size_t i = 0; i < max_points; ++i) {
        const std::size_t idx = static_cast<std::size_t>(
            static_cast<double>(i) * static_cast<double>(n - 1) / static_cast<double>(max_points - 1));
        out.push_back(src[std::min(idx, n - 1)]);
    }
    return out;
}

// 单个时间点的资源样本(供 timeseries/downsample 曲线)。
struct ResourcePoint {
    double t_seconds = 0.0;         // 距起始的单调秒
    double equivalent_cores = 0.0;  // 该区间等价核数
    uint64_t rss_bytes = 0;
    uint64_t read_bytes = 0;
    uint64_t write_bytes = 0;
    uint64_t ctx_switches = 0;
    double cpu_pct = 0.0;           // cpu_seconds / 区间墙钟 × 100
};

// 分层报告: summary(必) + downsample 曲线 + raw 计数(原始样本目录路径, 不内嵌数据)。
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
    // downsample 曲线(summary 强制即有此; timeseries 详略受 detail 控制)
    std::vector<ResourcePoint> curve;
    std::string raw_dir;             // 原始 timeseries 留存路径(节点)
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

// 构建降采样曲线(等价核数/RSS/IO/ctxsw 逐点); max_points 受 kDownsampleMax 约束。
inline std::vector<ResourcePoint> build_curve(const std::vector<ProcSample>& samples,
                                              double interval_seconds,
                                              std::size_t max_points = kDownsampleMax) {
    std::vector<ResourcePoint> pts;
    pts.reserve(std::min(max_points, samples.size()));
    for (std::size_t i = 0; i < samples.size(); ++i) {
        ResourcePoint p;
        p.t_seconds = static_cast<double>(i) * interval_seconds;
        const ProcSample& s = samples[i];
        double eq_cores = 0.0;
        if (interval_seconds > 0) eq_cores = s.d_cpu_seconds / interval_seconds;
        p.equivalent_cores = eq_cores;
        p.rss_bytes = s.rss_bytes;
        p.read_bytes = s.d_read_bytes;
        p.write_bytes = s.d_write_bytes;
        p.ctx_switches = s.d_ctx_switches;
        p.cpu_pct = eq_cores * 100.0;
        pts.push_back(p);
    }
    return downsample_curve(pts, max_points);
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
