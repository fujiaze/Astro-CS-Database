// astrocs RSS/allocation report (MON-002, V7 04_CPU_RESOURCE_TASKS) —
// 长 run 定期采样 RSS/private/commit/allocator outstanding; 区分 cache/高水位/工作集;
// run 结束验证可解释回落; 保存原始曲线(alloc_samples.csv + alloc_report.json)。
//
// 消费口径: ProcSample(cli/monitor.h 同域) —— 不改外域 schema, 拒外域字段。
// 哨兵纪律(批次 P p2007 先例): 采样失败样本显式标记(sampled=0 哨兵行), 不参与
// 统计判定, n_sentinel 呈现; reclaim_frac 不可计算记 -1(未采样非合法值, 不是
// 0 回落证据)。判定用整条曲线(Theil-Sen 稳健斜率+首末/留存), 峰值只是呈现
// 字段之一 —— 禁止只看峰值(规格验收)。
// allocator outstanding 探针: Linux glibc mallinfo2(uordblks+hblkhd);
// 非 glibc 平台保持 0 且 allocator_probe_available=false(显式未采样, 不冒充)。
// private 探针: /proc/self/smaps_rollup Private_Clean+Private_Dirty; 不可得=0
// 且 private_probe_available=false。cache 口径: commit - RSS(VM 保留非驻留页,
// 与工作集/高水位三分类呈现; 注释即口径, 消费者按字段名取用)。
// 纯 CLI 侧内部工具; 判定集中本头文件, 阈值常量集中, 无硬编码散落。
#pragma once
#if !defined(_WIN32)
#include <malloc.h>   // mallinfo2: allocator outstanding 探针(glibc; 无则编译期确认)
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "monitor.h"

namespace astrocs {

// ---- 阈值常量(集中声明; 诊断/报告引用同源) ----
// 增长判定: 稳健斜率(Unbounded)对齐 resource_gate.h GateConfig
// memory_growth_limit_mb_per_s 默认 32 MB/s; 超预警线(未达失败线)记 Growing。
inline constexpr double kAllocGrowthUnboundedMbPerS = 32.0;
inline constexpr double kAllocGrowthWarnMbPerS = 2.0;
// 回落判定仅长 run(active window>=10s 规格口径); 短 run 显式 ShortRunSkipped。
inline constexpr double kAllocMinReclaimWallSeconds = 10.0;
inline constexpr double kAllocMinReclaimFrac = 0.5;              // (peak-last)/peak 下限
inline constexpr uint64_t kAllocReclaimResidualTolBytes = 32ull * 1024 * 1024;
inline constexpr double kAllocReclaimWindowSeconds = 2.0;        // 峰值落入结尾窗=活跃中
inline constexpr std::size_t kAllocMinCurveSamples = 4;          // 稳健斜率最少有效样本
// Theil-Sen O(n²) 内存护栏: 长曲线等距降采样到该点数内再算稳健斜率(口径呈现)。
inline constexpr std::size_t kAllocSlopeMaxPoints = 200;

// 增长判定(整条曲线; 峰值不参与判定)。
enum class AllocGrowthVerdict {
    InsufficientSamples,  // 有效样本 < kAllocMinCurveSamples(含全哨兵) → 不判
    Stable,               // 稳健斜率 <= 预警线(负/近零 = 可解释回落方向)
    Growing,              // 预警线 < 斜率 < 失败线(早期预警, 不阻塞)
    Unbounded,            // 斜率 >= 失败线 → gate MemoryGrowth(RESOURCE 10)
};

// 回落判定(run 结束; 长 run 才判, 短 run 显式跳过非静默)。
enum class AllocReclaimVerdict {
    InsufficientSamples,   // 有效样本不足/峰值 0 → 不判, reclaim_frac=-1
    ShortRunSkipped,       // wall < kAllocMinReclaimWallSeconds(显式, 非 silent)
    Reclaimed,             // (peak-last)/peak >= kAllocMinReclaimFrac
    PeakInWindow,          // 峰值在结尾 kAllocReclaimWindowSeconds 内(活跃中, 不判负)
    UnexplainedResidual,   // 回落不足且残留超容差 → gate AllocReclaimMissing
};

inline const char* alloc_growth_verdict_name(AllocGrowthVerdict v) {
    switch (v) {
    case AllocGrowthVerdict::InsufficientSamples: return "insufficient_samples";
    case AllocGrowthVerdict::Stable:              return "stable";
    case AllocGrowthVerdict::Growing:             return "growing";
    case AllocGrowthVerdict::Unbounded:           return "unbounded";
    }
    return "unknown";
}

inline const char* alloc_reclaim_verdict_name(AllocReclaimVerdict v) {
    switch (v) {
    case AllocReclaimVerdict::InsufficientSamples:  return "insufficient_samples";
    case AllocReclaimVerdict::ShortRunSkipped:      return "short_run_skipped";
    case AllocReclaimVerdict::Reclaimed:            return "reclaimed";
    case AllocReclaimVerdict::PeakInWindow:         return "peak_in_window";
    case AllocReclaimVerdict::UnexplainedResidual:  return "unexplained_residual";
    }
    return "unknown";
}

// 单行曲线样本(sampled=0 为采样失败哨兵行: 入曲线留证, 不入统计)。
struct AllocSamplePoint {
    double t_seconds = 0.0;
    uint64_t rss_bytes = 0;             // 工作集(working set)
    uint64_t pss_bytes = 0;
    uint64_t commit_bytes = 0;          // VM commit(VmSize / Win PagefileUsage)
    uint64_t private_bytes = 0;         // smaps_rollup Private(可得时)
    uint64_t alloc_outstanding_bytes = 0;  // mallinfo2(可得时)
    uint64_t cache_bytes = 0;           // commit - RSS(保留非驻留, cache 口径)
    uint8_t sampled = 1;                // 0=采样失败哨兵(非法值, 不入统计)
};

// 报告(astrocs.memory-report/v1; 原始曲线在 alloc_samples.csv, 本结构为摘要)。
struct AllocReport {
    static constexpr const char* kSchema = "astrocs.memory-report/v1";
    std::size_t n_samples = 0;         // 有效样本(哨兵除外)
    std::size_t n_sentinel = 0;        // 采样失败哨兵行数(显式呈现)
    std::size_t n_curve = 0;           // 曲线总行数(有效+哨兵)
    double wall_seconds = 0.0;
    // 三分类呈现: 高水位/工作集/cache(判定不看单点峰值)
    uint64_t peak_rss_bytes = 0;       // 高水位
    uint64_t last_rss_bytes = 0;       // 收尾工作集
    uint64_t peak_commit_bytes = 0;
    uint64_t last_commit_bytes = 0;
    uint64_t peak_cache_bytes = 0;
    // allocator outstanding
    uint64_t peak_alloc_outstanding_bytes = 0;
    uint64_t last_alloc_outstanding_bytes = 0;
    bool allocator_probe_available = false;
    bool private_probe_available = false;
    // growth(整条曲线 Theil-Sen; 降采样口径见 slope_points)
    double rss_slope_bytes_per_s = 0.0;
    double rss_growth_mb_per_s = 0.0;
    std::size_t slope_points = 0;      // 参与斜率的有效点数(降采样后)
    AllocGrowthVerdict growth_verdict = AllocGrowthVerdict::InsufficientSamples;
    // reclaim(run 结束可解释回落)
    double reclaim_frac = -1.0;        // -1=未采样/不可计算(哨兵纪律, 非 0 回落)
    uint64_t retained_bytes = 0;       // peak - last(>0 即残留)
    AllocReclaimVerdict reclaim_verdict = AllocReclaimVerdict::InsufficientSamples;
    double alloc_sample_overhead_ms = 0.0;  // 每 tick 探针开销均值(实测呈现)
};

// glibc allocator outstanding 探针(非 glibc=0 且不可用显式呈现, 不冒充)。
inline uint64_t read_alloc_outstanding() {
#if defined(__GLIBC__)
    struct mallinfo2 m = mallinfo2();
    return static_cast<uint64_t>(m.uordblks) + static_cast<uint64_t>(m.hblkhd);
#else
    return 0;
#endif
}
inline bool alloc_probe_available() {
#if defined(__GLIBC__)
    return true;
#else
    return false;
#endif
}

// private 探针: /proc/self/smaps_rollup Private_Clean+Private_Dirty(Linux);
// 失败/非 Linux = 0 且不可用显式呈现。
inline uint64_t read_private_bytes(bool* available) {
    uint64_t priv = 0;
    bool ok = false;
#if !defined(_WIN32)
    std::FILE* f = std::fopen("/proc/self/smaps_rollup", "r");
    if (f) {
        char line[512];
        while (std::fgets(line, sizeof(line), f)) {
            if (std::strncmp(line, "Private_Clean:", 14) == 0 ||
                std::strncmp(line, "Private_Dirty:", 14) == 0) {
                unsigned long long v = 0;
                if (std::sscanf(line + 14, "%llu", &v) == 1) { priv += v * 1024ull; ok = true; }
            }
        }
        std::fclose(f);
    }
#endif
    if (available != nullptr) *available = ok;
    return ok ? priv : 0;
}

// (t, y) 平面 Theil-Sen 稳健斜率: 所有点对斜率的中位数(抗单点噪声;
// n² 内存受 kAllocSlopeMaxPoints 护栏, 调用方先等距降采样)。
inline double theil_sen_slope_xy(const std::vector<double>& t, const std::vector<double>& y) {
    const std::size_t n = std::min(t.size(), y.size());
    if (n < 2) return 0.0;
    std::vector<double> slopes;
    slopes.reserve(n * (n - 1) / 2);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j) {
            const double dt = t[j] - t[i];
            if (dt > 0.0) slopes.push_back((y[j] - y[i]) / dt);
        }
    if (slopes.empty()) return 0.0;
    const std::size_t mid = slopes.size() / 2;
    std::nth_element(slopes.begin(), slopes.begin() + static_cast<long>(mid), slopes.end());
    return slopes[mid];
}

// 等距降采样(首尾保留)到 max_points 内(索引序列, 与 resource_events 同口径)。
inline std::vector<std::size_t> downsample_indices(std::size_t n, std::size_t max_points) {
    if (n <= max_points) {
        std::vector<std::size_t> idx(n);
        for (std::size_t i = 0; i < n; ++i) idx[i] = i;
        return idx;
    }
    std::vector<std::size_t> idx;
    idx.reserve(max_points);
    for (std::size_t i = 0; i < max_points; ++i)
        idx.push_back(static_cast<std::size_t>(
            static_cast<double>(i) * static_cast<double>(n - 1) /
            static_cast<double>(max_points - 1)));
    return idx;
}

// 记录器: 由既有采样线程驱动(无新增线程); tick 开销实测呈现。
class AllocationRecorder {
public:
    // 一次采样(t 可注入: 单测构造合成曲线; 生产经 tick 用真实单调钟)。
    // ProcSample.rss==0 视为采样失败(进程 RSS 恒 >0)→ 哨兵行:
    // 入曲线(sampled=0)留证, 不入统计; 哨兵纪律同批次 P p2007(-1/未采样非合法值)。
    void observe(double t_seconds, const ProcSample& s) {
        const auto a = SteadyClock::now();
        AllocSamplePoint p;
        p.t_seconds = t_seconds;
        p.rss_bytes = s.rss_bytes;
        p.pss_bytes = s.pss_bytes;
        p.commit_bytes = s.vms_bytes;
        p.private_bytes = read_private_bytes(&priv_ok_);
        p.alloc_outstanding_bytes = read_alloc_outstanding();
        p.sampled = s.rss_bytes > 0 ? uint8_t{1} : uint8_t{0};
        p.cache_bytes = p.commit_bytes > p.rss_bytes ? p.commit_bytes - p.rss_bytes : 0;
        const auto b = SteadyClock::now();
        {
            std::lock_guard<std::mutex> lk(mu_);
            overhead_ns_ += static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
            if (p.sampled == 0) ++n_sentinel_;
            pts_.push_back(p);
        }
    }

    // 生产入口: 既有采样线程驱动(无新增线程); 探针开销实测呈现。
    void tick(const ProcSample& s) {
        observe(std::chrono::duration<double>(SteadyClock::now() - t0_).count(), s);
    }

    // 结束统计(曲线定型; 之后只读)。反复调用幂等(重算同源)。
    void finalize() {
        std::lock_guard<std::mutex> lk(mu_);
        rep_ = build_report_locked();
        finalized_ = true;
    }

    const AllocReport& report() const {
        std::lock_guard<std::mutex> lk(mu_);
        if (!finalized_) rep_ = build_report_locked();
        finalized_ = true;
        return rep_;
    }

    // 产物: alloc_samples.csv(原始曲线, 含哨兵行与 sampled 列) + alloc_report.json。
    bool write_all(const std::string& out_dir) const {
        const AllocReport r = report();
        // ---- alloc_samples.csv ----
        {
            std::FILE* f = std::fopen((out_dir + "/alloc_samples.csv").c_str(), "w");
            if (!f) return false;
            std::fprintf(f, "t_seconds,sampled,rss_bytes,pss_bytes,commit_bytes,"
                            "private_bytes,alloc_outstanding_bytes,cache_bytes\n");
            for (const auto& p : snapshot_locked()) {
                std::fprintf(f, "%.3f,%u,%llu,%llu,%llu,%llu,%llu,%llu\n",
                             p.t_seconds, static_cast<unsigned>(p.sampled),
                             (unsigned long long)p.rss_bytes,
                             (unsigned long long)p.pss_bytes,
                             (unsigned long long)p.commit_bytes,
                             (unsigned long long)p.private_bytes,
                             (unsigned long long)p.alloc_outstanding_bytes,
                             (unsigned long long)p.cache_bytes);
            }
            std::fclose(f);
        }
        // ---- alloc_report.json ----
        {
            std::FILE* f = std::fopen((out_dir + "/alloc_report.json").c_str(), "w");
            if (!f) return false;
            std::fprintf(f,
                "{\"schema\":\"astrocs.memory-report/v1\",\"n_samples\":%zu,\"n_sentinel\":%zu,"
                "\"n_curve\":%zu,\"wall_seconds\":%.3f,"
                "\"peak_rss_bytes\":%llu,\"last_rss_bytes\":%llu,"
                "\"peak_commit_bytes\":%llu,\"last_commit_bytes\":%llu,"
                "\"peak_cache_bytes\":%llu,"
                "\"peak_alloc_outstanding_bytes\":%llu,\"last_alloc_outstanding_bytes\":%llu,"
                "\"allocator_probe_available\":%s,\"private_probe_available\":%s,"
                "\"rss_slope_bytes_per_s\":%.3f,\"rss_growth_mb_per_s\":%.3f,"
                "\"slope_points\":%zu,\"growth_verdict\":\"%s\","
                "\"reclaim_frac\":%.3f,\"retained_bytes\":%llu,\"reclaim_verdict\":\"%s\","
                "\"thresholds\":{\"growth_unbounded_mb_per_s\":%.1f,\"growth_warn_mb_per_s\":%.1f,"
                "\"min_reclaim_frac\":%.2f,\"residual_tol_bytes\":%llu},"
                "\"alloc_sample_overhead_ms\":%.3f}\n",
                r.n_samples, r.n_sentinel, r.n_curve, r.wall_seconds,
                (unsigned long long)r.peak_rss_bytes, (unsigned long long)r.last_rss_bytes,
                (unsigned long long)r.peak_commit_bytes, (unsigned long long)r.last_commit_bytes,
                (unsigned long long)r.peak_cache_bytes,
                (unsigned long long)r.peak_alloc_outstanding_bytes,
                (unsigned long long)r.last_alloc_outstanding_bytes,
                r.allocator_probe_available ? "true" : "false",
                r.private_probe_available ? "true" : "false",
                r.rss_slope_bytes_per_s, r.rss_growth_mb_per_s, r.slope_points,
                alloc_growth_verdict_name(r.growth_verdict),
                r.reclaim_frac, (unsigned long long)r.retained_bytes,
                alloc_reclaim_verdict_name(r.reclaim_verdict),
                kAllocGrowthUnboundedMbPerS, kAllocGrowthWarnMbPerS,
                kAllocMinReclaimFrac, (unsigned long long)kAllocReclaimResidualTolBytes,
                r.alloc_sample_overhead_ms);
            std::fclose(f);
        }
        return true;
    }

private:
    std::vector<AllocSamplePoint> snapshot_locked() const {
        std::vector<AllocSamplePoint> out;
        for (const auto& p : pts_) out.push_back(p);
        return out;
    }

    AllocReport build_report_locked() const {
        AllocReport r;
        r.n_curve = pts_.size();
        std::vector<const AllocSamplePoint*> valid;
        for (const auto& p : pts_) if (p.sampled == 1) valid.push_back(&p);
        r.n_samples = valid.size();
        r.n_sentinel = n_sentinel_;
        r.wall_seconds = pts_.empty() ? 0.0 : pts_.back().t_seconds;
        r.allocator_probe_available = alloc_probe_available();
        r.private_probe_available = priv_ok_;
        if (valid.empty()) {
            // 全哨兵/零样本: 判定双 InsufficientSamples, reclaim_frac=-1(未采样非 0 回落)。
            r.growth_verdict = AllocGrowthVerdict::InsufficientSamples;
            r.reclaim_verdict = AllocReclaimVerdict::InsufficientSamples;
            r.reclaim_frac = -1.0;
            if (overhead_ns_ > 0 && !pts_.empty())
                r.alloc_sample_overhead_ms =
                    static_cast<double>(overhead_ns_) / static_cast<double>(pts_.size()) / 1e6;
            return r;
        }
        // 三分类呈现(峰值=高水位; last=收尾工作集; cache=commit-RSS 峰值)。
        for (const auto* p : valid) {
            if (p->rss_bytes > r.peak_rss_bytes) r.peak_rss_bytes = p->rss_bytes;
            if (p->commit_bytes > r.peak_commit_bytes) r.peak_commit_bytes = p->commit_bytes;
            if (p->cache_bytes > r.peak_cache_bytes) r.peak_cache_bytes = p->cache_bytes;
            if (p->alloc_outstanding_bytes > r.peak_alloc_outstanding_bytes)
                r.peak_alloc_outstanding_bytes = p->alloc_outstanding_bytes;
        }
        r.last_rss_bytes = valid.back()->rss_bytes;
        r.last_commit_bytes = valid.back()->commit_bytes;
        r.last_alloc_outstanding_bytes = valid.back()->alloc_outstanding_bytes;
        // ---- growth: 整条曲线 Theil-Sen(t, rss), O(n²) 护栏内等距降采样 ----
        if (valid.size() >= kAllocMinCurveSamples) {
            const auto idx = downsample_indices(valid.size(), kAllocSlopeMaxPoints);
            std::vector<double> t, y;
            t.reserve(idx.size()); y.reserve(idx.size());
            for (std::size_t i : idx) {
                t.push_back(valid[i]->t_seconds);
                y.push_back(static_cast<double>(valid[i]->rss_bytes));
            }
            r.slope_points = idx.size();
            r.rss_slope_bytes_per_s = theil_sen_slope_xy(t, y);
            r.rss_growth_mb_per_s = r.rss_slope_bytes_per_s / (1024.0 * 1024.0);
            if (r.rss_growth_mb_per_s >= kAllocGrowthUnboundedMbPerS)
                r.growth_verdict = AllocGrowthVerdict::Unbounded;
            else if (r.rss_growth_mb_per_s > kAllocGrowthWarnMbPerS)
                r.growth_verdict = AllocGrowthVerdict::Growing;
            else
                r.growth_verdict = AllocGrowthVerdict::Stable;
        } else {
            r.growth_verdict = AllocGrowthVerdict::InsufficientSamples;
            r.slope_points = 0;
        }
        // ---- reclaim: run 结束可解释回落(仅长 run; 判定不看峰值, 用留存) ----
        if (r.wall_seconds < kAllocMinReclaimWallSeconds) {
            r.reclaim_verdict = AllocReclaimVerdict::ShortRunSkipped;
        } else if (r.peak_rss_bytes == 0) {
            r.reclaim_verdict = AllocReclaimVerdict::InsufficientSamples;
            r.reclaim_frac = -1.0;
        } else {
            r.retained_bytes = r.peak_rss_bytes > r.last_rss_bytes
                                   ? r.peak_rss_bytes - r.last_rss_bytes : 0;
            r.reclaim_frac = static_cast<double>(r.retained_bytes) /
                             static_cast<double>(r.peak_rss_bytes);
            // 峰值落在结尾窗内=仍在活跃(工作集未到回收点), 不判回落失败。
            bool peak_in_window = false;
            for (const auto* p : valid)
                if (p->rss_bytes == r.peak_rss_bytes &&
                    r.wall_seconds - p->t_seconds <= kAllocReclaimWindowSeconds) {
                    peak_in_window = true;
                    break;
                }
            if (peak_in_window) {
                r.reclaim_verdict = AllocReclaimVerdict::PeakInWindow;
            } else if (r.reclaim_frac >= kAllocMinReclaimFrac ||
                       r.retained_bytes <= kAllocReclaimResidualTolBytes) {
                r.reclaim_verdict = AllocReclaimVerdict::Reclaimed;
            } else {
                r.reclaim_verdict = AllocReclaimVerdict::UnexplainedResidual;
            }
        }
        if (overhead_ns_ > 0 && !pts_.empty())
            r.alloc_sample_overhead_ms =
                static_cast<double>(overhead_ns_) / static_cast<double>(pts_.size()) / 1e6;
        return r;
    }

    SteadyClock::time_point t0_{SteadyClock::now()};
    mutable std::mutex mu_;
    std::vector<AllocSamplePoint> pts_;
    mutable AllocReport rep_{};
    mutable bool finalized_ = false;
    uint64_t overhead_ns_ = 0;
    std::size_t n_sentinel_ = 0;
    bool priv_ok_ = false;
};

// 报告自校验(供 reverify/审计复算; "原始 samples 必须可重算 summary"):
// 重读 alloc_samples.csv 重算 n/peak/last/slope, 与 alloc_report.json 比对;
// 行数不符/字段不符 → false(格式篡改拒绝)。无 nlohmann 依赖(轻量提取)。
inline bool validate_alloc_report(const std::string& dir) {
    // ---- 读 CSV ----
    std::FILE* f = std::fopen((dir + "/alloc_samples.csv").c_str(), "r");
    if (!f) return false;
    char line[512];
    if (!std::fgets(line, sizeof(line), f)) { std::fclose(f); return false; }
    std::vector<double> t; std::vector<double> y; std::size_t sentinel = 0;
    while (std::fgets(line, sizeof(line), f)) {
        double ts = 0.0; unsigned sampled = 0; unsigned long long rss = 0;
        if (std::sscanf(line, "%lf,%u,%llu", &ts, &sampled, &rss) != 3) { std::fclose(f); return false; }
        if (sampled > 1) { std::fclose(f); return false; }   // sampled 仅接受 0/1(格式篡改拒绝)
        t.push_back(ts);
        if (sampled == 1) y.push_back(static_cast<double>(rss));
        else ++sentinel;
    }
    std::fclose(f);
    // ---- 读 JSON 关键字段 ----
    f = std::fopen((dir + "/alloc_report.json").c_str(), "r");
    if (!f) return false;
    std::string js;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), f)) js += buf;
    std::fclose(f);
    auto extract_u64 = [&js](const char* key, unsigned long long* out) {
        const std::string pat = std::string("\"") + key + "\":";
        const auto pos = js.find(pat);
        if (pos == std::string::npos) return false;
        *out = std::strtoull(js.c_str() + pos + pat.size(), nullptr, 10);
        return true;
    };
    auto extract_f64 = [&js](const char* key, double* out) {
        const std::string pat = std::string("\"") + key + "\":";
        const auto pos = js.find(pat);
        if (pos == std::string::npos) return false;
        *out = std::strtod(js.c_str() + pos + pat.size(), nullptr);
        return true;
    };
    unsigned long long n_samples = 0, n_sentinel = 0, n_curve = 0;
    unsigned long long peak = 0, last = 0; double slope = 0.0, frac = 0.0;
    if (!extract_u64("n_samples", &n_samples) || !extract_u64("n_sentinel", &n_sentinel) ||
        !extract_u64("n_curve", &n_curve) || !extract_u64("peak_rss_bytes", &peak) ||
        !extract_u64("last_rss_bytes", &last) ||
        !extract_f64("rss_slope_bytes_per_s", &slope) || !extract_f64("reclaim_frac", &frac))
        return false;
    if (n_curve != t.size() || n_samples != y.size() || n_sentinel != sentinel) return false;
    if (js.find("astrocs.memory-report/v1") == std::string::npos) return false;
    if (y.empty()) {
        if (peak != 0 || last != 0) return false;
        if (frac >= 0.0) return false;   // 未采样必须 -1 哨兵, 不得冒充 0 回落
        return true;
    }
    unsigned long long rpeak = 0, rlast = 0;
    for (double v : y) {
        const auto u = static_cast<unsigned long long>(v);
        if (u > rpeak) rpeak = u;
    }
    rlast = static_cast<unsigned long long>(y.back());
    if (rpeak != peak || rlast != last) return false;
    // slope 重算(同降采样口径)与 JSON 比对(0.5% 相对容差, %.3f 打印误差内)
    if (y.size() >= kAllocMinCurveSamples) {
        const auto idx = downsample_indices(y.size(), kAllocSlopeMaxPoints);
        std::vector<double> ts, ys;
        for (std::size_t i : idx) { ts.push_back(t[i]); ys.push_back(y[i]); }
        const double rs = theil_sen_slope_xy(ts, ys);
        const double denom = std::max(1.0, std::fabs(rs));
        if (std::fabs(rs - slope) / denom > 0.005) return false;
    } else if (slope != 0.0) {
        return false;
    }
    return true;
}

}  // namespace astrocs
