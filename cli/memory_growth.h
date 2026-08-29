// astrocs 内存增长/泄漏分析 (MON-004) — 07 §5: 20 次循环、预热剔除、稳健斜率、峰值/retained 趋势、OOM 预警
// §6 诊断: 泄漏/增长/接近 OOM 等类别; 稳定 cache 不得误判。
// 纯逻辑(无副作用): 输入 = 每轮 RSS(或 retained bytes)序 + 每轮 时间/吞吐;
// 判定 = 泄漏(L8) / 稳定(M0) / 峰谷(噪声) / 接近 OOM 预警。禁止硬编码。
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {

// 内存增长诊断类别(07 §6 诊断分类的子集: 泄漏/增长/震荡/稳定/OOM 预警)。
enum class MemDiag {
    Stable,          // 无无界增长
    Leak,            // retained bytes 持续无界增加(稳健斜率越过阈值)
    Growing,         // 趋势为正但未到漏级别(早期即预警)
    Oscillating,     // 峰谷震荡(每轮释放, 非泄漏)
    OomPreWarning,   // 峰值接近系统内存/上限(07 §5 接近 OOM)
    Unknown,
};

inline const char* mem_diag_name(MemDiag d) {
    switch (d) {
    case MemDiag::Stable:      return "stable";
    case MemDiag::Leak:        return "leak";
    case MemDiag::Growing:     return "growing";
    case MemDiag::Oscillating: return "oscillating";
    case MemDiag::OomPreWarning: return "oom_pre_warning";
    default:                   return "unknown";
    }
}

struct MemAnalysisConfig {
    std::size_t warmup = 3;                 // 丢弃前 warmup 轮(预热剔除)
    double leak_slope_bytes_per_iter = 0.0; // 泄漏判定阈值(每轮字节增量)
    double oom_frac = 0.85;                 // 峰值 / 上限比例 → OOM 预警
    uint64_t mem_limit_bytes = 0;           // 可用内存上限(0=未知, 不判 OOM)
};

struct MemAnalysisResult {
    MemDiag diag = MemDiag::Unknown;
    double robust_slope_bytes_per_iter = 0.0;  // Theil-Sen 稳健斜率
    uint64_t peak_bytes = 0;
    uint64_t retained_growth_bytes = 0;     // 末轮 - 预热后首轮
    double relative_growth_pct = 0.0;       // retained_growth / 首轮 × 100
    std::size_t n_analyzed = 0;             // 剔除预热后分析轮数
    std::string detail;                     // §6 诊断说明
};

// Theil-Sen 斜率: 所有点对斜率的中位数(抗单点噪声, 比最小二乘稳健)。
inline double theil_sen_slope(const std::vector<double>& y) {
    const std::size_t n = y.size();
    if (n < 2) return 0.0;
    std::vector<double> slopes;
    slopes.reserve(n * (n - 1) / 2);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 1; j < n; ++j)
            slopes.push_back((y[j] - y[i]) / static_cast<double>(j - i));
    const std::size_t mid = slopes.size() / 2;
    std::nth_element(slopes.begin(), slopes.begin() + mid, slopes.end());
    return slopes[mid];
}

// 峰值检测: 最大 retained; 相对首轮增长(%)。
inline uint64_t max_of(const std::vector<uint64_t>& v) {
    return v.empty() ? 0 : *std::max_element(v.begin(), v.end());
}

// 分析内存增长。输入 rss 每轮(已按序); 剔除 warmup 后 Theil-Sen 判斜率。
inline MemAnalysisResult analyze_memory_growth(const std::vector<uint64_t>& rss,
                                               const MemAnalysisConfig& cfg) {
    MemAnalysisResult r;
    if (rss.size() < 2) return r;
    // warmup 剔除
    const std::size_t start = std::min(cfg.warmup, rss.size() - 1);
    std::vector<uint64_t> used(rss.begin() + static_cast<long>(start), rss.end());
    std::vector<double> yd(used.size());
    for (std::size_t i = 0; i < used.size(); ++i) yd[i] = static_cast<double>(used[i]);
    r.n_analyzed = used.size();
    r.robust_slope_bytes_per_iter = theil_sen_slope(yd);
    r.peak_bytes = max_of(rss);
    const uint64_t first = used.front();
    const uint64_t last = used.back();
    r.retained_growth_bytes = (last > first) ? last - first : 0;
    r.relative_growth_pct = first ? static_cast<double>(r.retained_growth_bytes) / static_cast<double>(first) * 100.0
                                  : 0.0;
    // 判定顺序。先用峰谷振幅区分"震荡"与"平稳"，再判泄漏/增长。
    // 振幅 = (max-min)/min over used(剔除预热后); 大振幅+近零净斜率 → 振荡(非泄漏)。
    const uint64_t umin = *std::min_element(used.begin(), used.end());
    const uint64_t umax = *std::max_element(used.begin(), used.end());
    const double amplitude_ratio = umin ? static_cast<double>(umax - umin) / static_cast<double>(umin) : 0.0;
    const double slope_abs = std::fabs(r.robust_slope_bytes_per_iter);
    if (cfg.mem_limit_bytes > 0 &&
        static_cast<double>(r.peak_bytes) >= cfg.oom_frac * static_cast<double>(cfg.mem_limit_bytes)) {
        r.diag = MemDiag::OomPreWarning;
        r.detail = "peak retained " + std::to_string(r.peak_bytes) + " >= " +
                   std::to_string(static_cast<int>(cfg.oom_frac * 100)) +
                   "% of mem limit → 接近 OOM";
        return r;
    }
    // 近零净斜率: 区分震荡(高振幅+往返释放) 与 平稳(低振幅)。
    if (slope_abs < (cfg.leak_slope_bytes_per_iter > 0 ? cfg.leak_slope_bytes_per_iter * 0.5 : 0.0)) {
        if (amplitude_ratio >= 0.3) {   // 峰谷明显 → 振荡(每轮释放, 非泄漏)
            r.diag = MemDiag::Oscillating;
            r.detail = "波动幅度 " + std::to_string(static_cast<int>(amplitude_ratio * 100)) +
                       "%, 净斜率近 0 → 峰谷震荡(非泄漏)";
        } else {
            r.diag = MemDiag::Stable;
            r.detail = "retained 稳定(斜率近 0): +" + std::to_string(r.retained_growth_bytes) +
                       " B / " + std::to_string(r.n_analyzed) + " 轮";
        }
        return r;
    }
    if (r.robust_slope_bytes_per_iter >= cfg.leak_slope_bytes_per_iter &&
        cfg.leak_slope_bytes_per_iter > 0) {
        r.diag = MemDiag::Leak;
        r.detail = "Theil-Sen slope " + std::to_string(r.robust_slope_bytes_per_iter) +
                   " >= leak threshold " + std::to_string(cfg.leak_slope_bytes_per_iter) +
                   " → retained bytes 无界增长";
        return r;
    }
    if (r.robust_slope_bytes_per_iter > 0) {
        r.diag = MemDiag::Growing;
        r.detail = "slope " + std::to_string(r.robust_slope_bytes_per_iter) +
                   " > 0, retained +" + std::to_string(r.relative_growth_pct) + "% → 增长预警";
        return r;
    }
    r.diag = MemDiag::Stable;
    r.detail = "retained 稳定(净斜率<=0): +" + std::to_string(r.retained_growth_bytes) +
               " B / " + std::to_string(r.n_analyzed) + " 轮";
    return r;
}

}  // namespace astrocs
