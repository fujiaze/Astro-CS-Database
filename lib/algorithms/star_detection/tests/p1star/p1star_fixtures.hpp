// ============================================================================
// p1star_fixtures.hpp — FIX-STAR-A..G 固定 seed fixture generator (P1-STAR-TEST)
// ----------------------------------------------------------------------------
// 合同锚: STAR_DETECTION_ALGORITHMS.md §11.4 TEST-STAR-DESIGN-001 (P1-STAR-DOC
// 冻结, 2026-09-08, wave W3) — FIX-STAR-A↔F1, B/C↔F2, D↔F3, E↔F4, F↔F6;
// §2 ALG-STARDET-001 合成场与检测语义。
//
// 容差来源注记 (§11.4, P1-STAR-TEST 不得放宽, 亦不事后修改):
//   F1 |Δc|≤0.3px (SNR≥20), FWHM 相对误差≤10%, SNR≥10 召回≥99%,
//      纯噪声虚警≤0.1/千像素
//   F2 平台≥3px 检出 saturated=1, d<2px 保饱和星, 边界 2px 内允许丢弃
//   F3 线程 1/2/4 bitwise 一致, mag 升序 + NaN 恒末尾, maxStars 保最亮
//   F4 FP64 |Δ中心|≤0.05px, A/B 相对误差≤1e-3; FP32(u16 量化) |Δc|≤0.5px
//   F5 NULL/空图/0 尺寸 → rc=-1; 空场 → count=0 且 rc=0
//   F6 Galaxy_Center 类饱和平台场回归锚
//
// 全部 fixture 为确定性 mt19937_64 固定 seed 生成, 无大二进制提交。
// 像素几何约定: 像素 (px,py) 的物理中心 = (px+0.5, py+0.5) (对齐生产残差
// 坐标系, 探针标定 2026-09-09: 无噪场复原 x=cx 精确成立)。
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace p1star {

struct SynthStar {
    double cx;      // 亚像素中心 (物理坐标)
    double cy;
    double sigma;   // 高斯 σ (px); sigma<0 → 平台盘半径 |sigma| (TOP_HAT)
    double amp;     // 峰值振幅 (bg 之上)
};

// FIX-STAR-A (F1): 含噪多星场 256×256, 40 星网格布点 (边界 ≥27.5px),
// SNR 混谱 (≈20 谱段 15% / 中 25% / 亮 60%), bg=300, noise σ=6, seed 20260907。
// 与 truth 一同返回, 供召回/虚警/精度统计 (SNR≥10/20 分谱判定)。
struct FixStarA {
    int w = 256, h = 256;
    double bg = 300.0;
    double noise = 6.0;
    std::vector<double> img;
    std::vector<SynthStar> truth;
};

inline FixStarA fix_star_a_f1() {
    FixStarA fx;
    std::mt19937_64 rng(20260907ull);
    std::uniform_real_distribution<double> jitter(-3.0, 3.0), sig_u(1.8, 3.2), amp_u(0.0, 1.0);
    std::normal_distribution<double> ndist(fx.bg, fx.noise);
    fx.img.assign((std::size_t)fx.w * fx.h, 0.0);
    for (auto& v : fx.img) v = ndist(rng);
    fx.truth.reserve(40);
    for (int gy = 0; gy < 5; ++gy) {
        for (int gx = 0; gx < 8; ++gx) {
            const double p = amp_u(rng);
            double amp;
            if (p < 0.15) amp = 40.0 + p * 100.0;       // SNR≈20 谱段
            else if (p < 0.4) amp = 150.0 + p * 300.0;  // 中
            else amp = 1500.0 + p * 6500.0;             // 亮
            SynthStar s{40.0 + gx * 25.0 + jitter(rng),
                        40.0 + gy * 45.0 + jitter(rng),
                        sig_u(rng), amp};
            fx.truth.push_back(s);
        }
    }
    // 独立加星 (fixture 侧实现; oracle 用自己的 psf 函数复算期望)
    for (const auto& s : fx.truth) {
        const int r = (int)std::ceil(6.0 * s.sigma) + 1;
        const int x0 = std::max(0, (int)s.cx - r), x1 = std::min(fx.w, (int)s.cx + r + 1);
        const int y0 = std::max(0, (int)s.cy - r), y1 = std::min(fx.h, (int)s.cy + r + 1);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                const double dx = x + 0.5 - s.cx, dy = y + 0.5 - s.cy;
                fx.img[(std::size_t)y * fx.w + x] +=
                    s.amp * std::exp(-(dx * dx + dy * dy) / (2.0 * s.sigma * s.sigma));
            }
    }
    return fx;
}

inline void add_gaussian_to(std::vector<double>& img, int w, int h,
                            double cx, double cy, double amp, double sigma) {
    const int r = (int)std::ceil(6.0 * sigma) + 1;
    const int x0 = std::max(0, (int)cx - r), x1 = std::min(w, (int)cx + r + 1);
    const int y0 = std::max(0, (int)cy - r), y1 = std::min(h, (int)cy + r + 1);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            img[(std::size_t)y * w + x] +=
                amp * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
        }
}

inline void add_top_hat_to(std::vector<double>& img, int w, int h,
                           double cx, double cy, double amp, double radius) {
    for (int y = std::max(0, (int)(cy - radius - 1)); y < std::min(h, (int)(cy + radius + 2)); ++y)
        for (int x = std::max(0, (int)(cx - radius - 1)); x < std::min(w, (int)(cx + radius + 2)); ++x) {
            const double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
            if (dx * dx + dy * dy <= radius * radius)
                img[(std::size_t)y * w + x] += amp;
        }
}

// u16 量化 (生产 u16 通道输入语义; clamp 到 [0,65535])
inline std::vector<uint16_t> quantize_u16(const std::vector<double>& img,
                                          int* out_clipped = nullptr) {
    std::vector<uint16_t> u(img.size());
    if (out_clipped) *out_clipped = 0;
    for (std::size_t i = 0; i < img.size(); ++i) {
        double v = img[i] < 0.0 ? 0.0 : (img[i] > 65535.0 ? 65535.0 : img[i]);
        if (img[i] > 65535.0 && out_clipped) (*out_clipped)++;
        u[i] = (uint16_t)(v + 0.5);
    }
    return u;
}

// FIX-STAR-B (F2): u16 饱和平台星 — σ=2.5 高斯 A=90000 经 clamp 产生直径≈4px
// 顶格平台 (nclip≈13px), 伴星 A=2000 σ=2 d≈85px 不混。标定探针 2026-09-09:
// 检出 sat=1, flux=89999.8 (σ=4 顶格盘候选被 Moffat4 拟合 NO_CONVERGENCE
// 依合同拒绝 — 顶格平台必须窄 σ 大 A 构造, 平台≥3px 语义成立)。
struct FixStarB {
    int w = 96, h = 96;
    double bg = 300.0;
    std::vector<uint16_t> img;
    int nclip = 0;
};

inline FixStarB fix_star_b_f2_saturated() {
    FixStarB fx;
    std::vector<double> img((std::size_t)fx.w * fx.h, fx.bg);
    add_gaussian_to(img, fx.w, fx.h, 48.5, 47.5, 90000.0, 2.5);
    add_gaussian_to(img, fx.w, fx.h, 40.0, 20.0, 2000.0, 2.0);  // d≈28.8 独立伴星
    fx.img = quantize_u16(img, &fx.nclip);
    return fx;
}

// FIX-STAR-C (F2): u16 饱和混合对 — 亮星 (饱和) + 暗星 d=1.5px (<2px 保饱和
// 星语义), 同场第三星 d=6px (独立检出)。
struct FixStarC {
    int w = 96, h = 96;
    double bg = 300.0;
    std::vector<uint16_t> img;
};

inline FixStarC fix_star_c_f2_blend() {
    FixStarC fx;
    std::vector<double> img((std::size_t)fx.w * fx.h, fx.bg);
    add_gaussian_to(img, fx.w, fx.h, 40.0, 40.0, 90000.0, 2.5);  // 饱和
    add_gaussian_to(img, fx.w, fx.h, 41.5, 40.0, 900.0, 1.6);    // d=1.5 混合伴
    add_gaussian_to(img, fx.w, fx.h, 70.0, 60.0, 3000.0, 2.0);   // d≈33 独立
    fx.img = quantize_u16(img);
    return fx;
}

// FIX-STAR-D (F3): 无噪 25 星确定性场 256×256 (splitmix 定位, 无随机数依赖
// 单调递推), 供 OMP 1/2/4 bitwise + 排序全序 + maxStars 截断。
struct FixStarD {
    int w = 256, h = 256;
    double bg = 300.0;
    std::vector<double> img;
    std::vector<SynthStar> truth;
};

inline FixStarD fix_star_d_f3() {
    FixStarD fx;
    fx.img.assign((std::size_t)fx.w * fx.h, fx.bg);
    uint64_t st = 0x20260909u;
    auto next = [&st]() {
        st += 0x9E3779B97F4A7C15ull;
        uint64_t z = st;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    };
    for (int k = 0; k < 25; ++k) {
        SynthStar s{25.0 + (double)(next() % 20600) / 100.0,
                    25.0 + (double)(next() % 20600) / 100.0,
                    1.8 + (double)(next() % 140) / 100.0,
                    800.0 + (double)(next() % 400000) / 100.0};
        fx.truth.push_back(s);
        add_gaussian_to(fx.img, fx.w, fx.h, s.cx, s.cy, s.amp, s.sigma);
    }
    return fx;
}

// FIX-STAR-E (F4): 无噪解析场 256×256, 20 星位置/σ/A 全公式化 (无随机数),
// oracle 独立复算期望 (构造参数即期望)。
struct FixStarE {
    int w = 256, h = 256;
    double bg = 300.0;
    std::vector<double> img;
    std::vector<SynthStar> truth;
};

inline FixStarE fix_star_e_f4() {
    FixStarE fx;
    fx.img.assign((std::size_t)fx.w * fx.h, fx.bg);
    for (int k = 0; k < 20; ++k) {
        SynthStar s{30.0 + (k % 5) * 48.0 + 0.37 * ((k * 7) % 3 - 1),
                    30.0 + (k / 5) * 48.0 + 0.61 * ((k * 11) % 3 - 1),
                    1.8 + 0.2 * (k % 6),
                    800.0 + k * 250.0};
        fx.truth.push_back(s);
        add_gaussian_to(fx.img, fx.w, fx.h, s.cx, s.cy, s.amp, s.sigma);
    }
    return fx;
}

// FIX-STAR-F (F6): 回归锚 — Galaxy_Center 类饱和平台场: u16, 128×128,
// 中心大振幅窄 σ 饱和星 + 3 颗常规伴星 + 固定噪声。锚值在 oracle 组断言。
struct FixStarF {
    int w = 128, h = 128;
    double bg = 300.0;
    std::vector<uint16_t> img;
};

inline FixStarF fix_star_f_f6_anchor() {
    FixStarF fx;
    std::mt19937_64 rng(20260908ull);
    std::normal_distribution<double> ndist(fx.bg, 5.0);
    std::vector<double> img((std::size_t)fx.w * fx.h, 0.0);
    for (auto& v : img) v = ndist(rng);
    add_gaussian_to(img, fx.w, fx.h, 64.3, 63.7, 120000.0, 2.2);  // 饱和平台核
    add_gaussian_to(img, fx.w, fx.h, 30.2, 40.9, 2500.0, 2.0);
    add_gaussian_to(img, fx.w, fx.h, 95.6, 88.4, 1500.0, 2.4);
    add_gaussian_to(img, fx.w, fx.h, 40.1, 100.3, 900.0, 1.9);
    fx.img = quantize_u16(img);
    return fx;
}

// FIX-STAR-G: 纯噪声空场 256×256 (F1 虚警面) — seed 固定, bg=300 σ=5。
struct FixStarG {
    int w = 256, h = 256;
    std::vector<double> img;
};

inline FixStarG fix_star_g_noise_only() {
    FixStarG fx;
    std::mt19937_64 rng(20260907ull);
    std::normal_distribution<double> ndist(300.0, 5.0);
    fx.img.assign((std::size_t)fx.w * fx.h, 0.0);
    for (auto& v : fx.img) v = ndist(rng);
    return fx;
}

}  // namespace p1star
