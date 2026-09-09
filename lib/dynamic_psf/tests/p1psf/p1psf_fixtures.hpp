// ============================================================================
// P1-PSF-TEST · 固定 seed fixture generator (FIX-PSF-A..F)
// ----------------------------------------------------------------------------
// 控制包任务: P1-PSF-TEST (queue 45, lock-P1-PSF; 依赖 P1-PSF-DOC 闭环)。
// 合同锚: docs/algorithms/STAR_PSF_ALGORITHMS.md §11 TEST-PSF-DESIGN-001;
// lib/dynamic_psf/README.md (P1-PSF-DOC 冻结合同, 2026-09-08)。
//
// splitmix64 固定 seed 谱系 (对齐 p1cal/p1cos/p1drz/p1hips): 所有伪随机量
// 由显式 seed 派生, 任何平台/优化级别逐 bit 可复现。噪声 field 用
// Box-Muller 从 splitmix64 uniform 流构造 Gaussian。
//
// fixture 语义: Moffat4 解析星 (FIX-PSF-A) + Gaussian 噪声场 (FIX-PSF-B) +
// NaN 污染场 (FIX-PSF-C) + 边界星 (FIX-PSF-D) + 拟合必败星 (FIX-PSF-E) +
// 性能负载 (FIX-PSF-PERF)。所有科学真值由 moffat4 采样器 (p1psf_oracle.hpp,
// 测试侧独立实现) 生成——真值先于被测函数存在, 期望值不来自被测函数。
// ============================================================================
#ifndef P1PSF_FIXTURES_HPP
#define P1PSF_FIXTURES_HPP

#include <cmath>
#include <cstdint>
#include <vector>

#include "p1psf_oracle.hpp"

namespace p1psf {

// ---------------------------------------------------------------------------
// splitmix64 (固定 seed 谱系) + Box-Muller Gaussian
// ---------------------------------------------------------------------------
inline uint64_t splitmix64(uint64_t& state) {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

inline double splitmix_uniform(uint64_t& state) {
    return static_cast<double>(splitmix64(state) >> 11) / 9007199254740992.0;  // [0,1)
}

inline double splitmix_gauss(uint64_t& state) {
    // Box-Muller: 独立 uniform 对 → 独立 Gaussian 对
    const double u1 = splitmix_uniform(state);
    const double u2 = splitmix_uniform(state);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

inline constexpr double kFwhmFactor = 1.230310;  // MOFFAT4_FWHM_FACTOR (PSF.md)

// 默认拟合参数 (合同冻结默认: fitRadius=8; LM tol=1e-8 / maxIter=200 为源内
// 硬编码, DPSFFitParams.maxIter/tolerance 实际无效 — DISP-PSF-003, 测试面
// 仅以 fitRadius 传参, 其余字段按默认值透传)
inline DPSFFitParams default_params(int fit_radius = 8) {
    DPSFFitParams p;
    p.fitRadius = fit_radius;
    p.maxIter = 200;
    p.tolerance = 1e-8;
    return p;
}

// 通用星模板 (truth)
struct PsfStar {
    double B, A, cx, cy, sx, sy, theta;
};

// ---------------------------------------------------------------------------
// FIX-PSF-A: 解析单星场 (round θ=0), f64/f32/u16 三份同源数据
// ---------------------------------------------------------------------------
struct FixPsfA {
    int w, h;
    std::vector<double> f64;
    std::vector<float> f32;
    std::vector<uint16_t> u16;
    PsfStar star;
};

inline FixPsfA fix_psf_a(double B = 50.0, double A = 900.0, double cx = 20.0,
                         double cy = 20.0, double sigma = 2.0, int w = 40, int h = 40) {
    FixPsfA fx;
    fx.w = w; fx.h = h;
    fx.star = {B, A, cx, cy, sigma, sigma, 0.0};
    fx.f64.resize((size_t)w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            fx.f64[(size_t)y * w + x] =
                moffat4_eval(B, A, cx, cy, sigma, sigma, 0.0, (double)x, (double)y);
    fx.f32.resize(fx.f64.size());
    for (size_t i = 0; i < fx.f64.size(); ++i) fx.f32[i] = (float)fx.f64[i];
    fx.u16.resize(fx.f64.size());
    for (size_t i = 0; i < fx.f64.size(); ++i) fx.u16[i] = (uint16_t)fx.f64[i];
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-PSF-B: Gaussian 噪声场 + 0..N 星 (splitmix64 seed)
// ---------------------------------------------------------------------------
struct FixPsfB {
    int w, h;
    std::vector<double> f64;
    std::vector<PsfStar> stars;
};

inline FixPsfB fix_psf_b(uint64_t seed, int w, int h, double bkg_mean,
                         double bkg_sigma, const std::vector<PsfStar>& stars) {
    FixPsfB fx;
    fx.w = w; fx.h = h;
    fx.stars = stars;
    fx.f64.assign((size_t)w * h, 0.0);
    uint64_t st = seed;
    for (size_t i = 0; i < fx.f64.size(); ++i) fx.f64[i] = bkg_mean + bkg_sigma * splitmix_gauss(st);
    for (const auto& s : stars) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                fx.f64[(size_t)y * w + x] +=
                    moffat4_eval(0.0, s.A, s.cx, s.cy, s.sx, s.sy, s.theta, (double)x, (double)y);
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-PSF-C: NaN 污染场 (splitmix64 seed 选污染像素; 全 NaN patch 由调用方
// 以整 patch 清 NaN 构造)
// ---------------------------------------------------------------------------
struct FixPsfC {
    int w, h;
    std::vector<double> f64;
    std::vector<int> nan_idx;   // 被污染像素 (行主序 flat index)
    PsfStar star;               // 单星 truth
};

inline FixPsfC fix_psf_c(uint64_t seed, int w, int h, const PsfStar& star, int n_nan) {
    FixPsfC fx;
    fx.w = w; fx.h = h;
    fx.star = star;
    fx.f64.assign((size_t)w * h, 0.0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            fx.f64[(size_t)y * w + x] =
                moffat4_eval(star.B, star.A, star.cx, star.cy, star.sx, star.sy, star.theta, (double)x, (double)y);
    uint64_t st = seed;
    for (int k = 0; k < n_nan; ++k) {
        const int idx = (int)(splitmix_uniform(st) * (double)(w * h));
        fx.f64[(size_t)idx] = std::nan("1");
        fx.nan_idx.push_back(idx);
    }
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-PSF-D: 边界星集合 (贴边 clamp / 大 sigma 背景约束 / 锐星 / 大 FWHM)
// 全部为解析场 — 无随机成分, 边界行为确定性
// ---------------------------------------------------------------------------
struct FixPsfD {
    // 贴边星 (0.5, 47.5), 图 48x48
    FixPsfA edge;
    // 大 sigma 星 (B=100, A=5000, sx=sy=5.5) — 背景约束链触发 NO_CONVERGENCE
    // 样本 (probe 实证 st=1; 拟合窗内恒星占优 → bkg0 抬升 → 背景约束违反)
    FixPsfA wide;
    // 锐星 (sigma=0.45) — ITERATION_LIMIT 样本 (probe 实证 st=3, 回填 sx)
    FixPsfA sharp;
    // 中等大星 (sigma=5.0) — 背景约束违反 → NO_CONVERGENCE 样本 (probe 实证)
    FixPsfA wide_fail;
};

inline FixPsfD fix_psf_d() {
    FixPsfD d;
    d.edge = fix_psf_a(100.0, 2000.0, 0.5, 47.5, 2.0, 48, 48);
    d.wide = fix_psf_a(100.0, 5000.0, 48.0, 48.0, 5.5, 96, 96);
    d.sharp = fix_psf_a(100.0, 2000.0, 48.0, 48.0, 0.45, 96, 96);
    d.wide_fail = fix_psf_a(100.0, 5000.0, 48.0, 48.0, 5.0, 96, 96);
    return d;
}

// ---------------------------------------------------------------------------
// FIX-PSF-E: 拟合必败星集合 (负面矩阵的解析构造面; 行为均经 probe 实证)
//   constant_field : B 恒定场 → A0≤0 → INVALID_PARAMS (码 2, memset 0)
//   all_nan_patch  : patch 内全 NaN → 样本过滤后空 → INVALID_PARAMS (码 2)
//   nan_center     : 仅中心像素 NaN → A0 估计 ≤0 → INVALID_PARAMS (码 2;
//                    实证非 NO_CONVERGENCE — 中心 NaN 使峰值估计失效)
//   noise_only     : 纯噪声场 (无星) → A0 小 → LM 不收敛 → NO_CONVERGENCE
//                    (码 1, memset 0) — 在 FIX-PSF-B 零星场中构造
// ---------------------------------------------------------------------------
struct FixPsfE {
    std::vector<double> constant_field;
    std::vector<double> all_nan_patch;
    std::vector<double> nan_center;
    int w = 32, h = 32;
};

inline FixPsfE fix_psf_e(double B = 120.0) {
    FixPsfE e;
    e.w = 32; e.h = 32;
    e.constant_field.assign((size_t)e.w * e.h, B);
    // 17x17 全 NaN patch (fitRadius=8 的完整拟合窗)
    e.all_nan_patch.assign((size_t)e.w * e.h, B);
    for (int y = 8; y < 25; ++y)
        for (int x = 8; x < 25; ++x)
            e.all_nan_patch[(size_t)y * e.w + x] = std::nan("1");
    // 仅中心像素 NaN
    e.nan_center.assign((size_t)e.w * e.h, B);
    e.nan_center[(size_t)16 * e.w + 16] = std::nan("1");
    return e;
}

// ---------------------------------------------------------------------------
// FIX-PSF-PERF: 性能负载 (128x128, 5x5=25 星网格, splitmix64 seed 噪声)
// ---------------------------------------------------------------------------
struct FixPsfPerf {
    int w, h, n;
    std::vector<double> f64;
    std::vector<double> cx, cy;
};

inline FixPsfPerf fix_psf_perf(uint64_t seed = 20260909ull, int grid = 5, int w = 128, int h = 128) {
    FixPsfPerf fx;
    fx.w = w; fx.h = h;
    fx.n = grid * grid;
    std::vector<PsfStar> stars;
    for (int gy = 0; gy < grid; ++gy) {
        for (int gx = 0; gx < grid; ++gx) {
            const double cx = 16.0 + (w - 32.0) * gx / (grid - 1.0);
            const double cy = 16.0 + (h - 32.0) * gy / (grid - 1.0);
            stars.push_back({100.0, 600.0 + 40.0 * (gx + gy), cx, cy,
                             1.5 + 0.1 * gx, 1.3 + 0.1 * gy, 0.05 * (gx + 2.0 * gy)});
        }
    }
    const FixPsfB b = fix_psf_b(seed, w, h, 80.0, 3.0, stars);
    fx.f64 = std::move(b.f64);
    fx.cx.resize(fx.n);
    fx.cy.resize(fx.n);
    for (int i = 0; i < fx.n; ++i) { fx.cx[i] = stars[i].cx; fx.cy[i] = stars[i].cy; }
    return fx;
}

}  // namespace p1psf

#endif  // P1PSF_FIXTURES_HPP
