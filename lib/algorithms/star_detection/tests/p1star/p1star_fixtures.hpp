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
// SNR 混谱 (低 6.7–9.2 占 15% / 中 32.5–45 占 25% / 亮 683–1333 占 60%),
// bg=300, noise σ=6, seed 20260907。注: 低谱段低于 SCI 冻结的 SNR≥10 召回域。
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
            if (p < 0.15) amp = 40.0 + p * 100.0;       // amp 40–55 → SNR 6.7–9.2
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

// ============================================================================
// FIX-STAR-H (F1-TB) — 过渡带真星召回场
// ----------------------------------------------------------------------------
// 合同锚: docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11.4 F1「判据式 + 判据
// 非退化要求」; docs/science/STAR_DETECTION.md §1 completeness 条。
//
// 存在理由: FIX-STAR-A 的真星 SNR_peak 只落在 [6.7,9.2) ∪ [32.5,45) ∪
// [683,1333), 区间 [10,32.5) 为空档 ⇒ 旧的平坦门「SNR≥10 召回≥99%」恒绿,
// 从未行使其声明域。本场按 §11.4 冻结的六个 σ_psf 档 × SNR 阶梯铺满
// [10, 32.5) 全宽, 峰值对齐像素中心 (冻结判据表的声明域)。
//
// 逐星分区 (逐档实测 99% 召回阈表 F1TB_THR99, 见下; σ_smooth = 2.0 px):
//   POS   : SNR_peak ≥ thr99(σ)       判据声明在域内 ⇒ 必须召回 (≥99%)
//   TB    : 10 ≤ SNR_peak < thr99(σ)  过渡带负例 ⇒ 必须存在且必须判红
//   BELOW : SNR_peak < 10             声明域之外 (本场不布点)
// ============================================================================

inline constexpr double F1TB_SIGMA_SMOOTH = 2.0;   // sdet_api.cpp:1772 实参
inline constexpr double F1TB_SNR_FLOOR = 10.0;     // 旧冻结声明的召回域下限

// §11.4 F1 判据表: σ_psf 六档 + 逐档实测阈 (生产 sdet_detect_ex_f64, 默认参数
// sdet_create(nullptr), 256² 单星居中, 峰值对齐像素中心, **300 次/档**)。
//   THR50 : 检出概率 0.5 的信噪比 (50% 过渡点)
//   THR99 : 使实测召回达到 **300/300** 的最小信噪比格点。
//           n=300 的依据: 零失败下 Clopper-Pearson 95% 下界 = 0.05^(1/300) = 0.99006
//           ≥ 0.99; 最小样本量 ln(0.05)/ln(0.99) = 298.07。n=64 零失败只给 0.954 下界,
//           不足以确立 ≥99% ⇒ 旧 64 次/档标定系统性偏乐观 (σ=1.5/2.5 各低 1 个 SNR 单位,
//           σ=1.0 低 8 个)。
inline constexpr double F1TB_SIGMA_BANDS[6] = {1.0, 1.27, 1.5, 2.0, 2.5, 3.0};
inline constexpr double F1TB_THR50[6] = {31.1, 21.5, 17.7, 14.1, 9.3, 8.3};
inline constexpr double F1TB_THR99[6] = {54.0, 24.0, 20.0, 16.0, 11.0, 10.0};
inline constexpr double F1TB_KAPPA50 = 6.21;       // 50% 点单参数拟合 (趋势模型)
inline constexpr double F1TB_KAPPA99 = 7.15;       // 99% 阈单参数拟合, σ>=1.27 五档
                                                   // (六档含 σ=1.0 时 7.75 ± 1.56, 残差 28.2%)

// 档值本身取该档实测值; 档间 (严格位于两档之间) 取相邻两档较严者 (较大阈);
// σ_psf 超出 [1.0, 3.0] px 取端点档值 (声明域外, 见 §11.4 适用域)
inline double f1tb_thr50(double sigma) {
    if (sigma <= F1TB_SIGMA_BANDS[0]) return F1TB_THR50[0];
    for (int b = 0; b + 1 < 6; ++b) {
        if (std::fabs(sigma - F1TB_SIGMA_BANDS[b + 1]) < 1e-12) return F1TB_THR50[b + 1];
        if (sigma < F1TB_SIGMA_BANDS[b + 1])
            return std::max(F1TB_THR50[b], F1TB_THR50[b + 1]);
    }
    return F1TB_THR50[5];
}
inline double f1tb_thr99(double sigma) {
    if (sigma <= F1TB_SIGMA_BANDS[0]) return F1TB_THR99[0];
    for (int b = 0; b + 1 < 6; ++b) {
        if (std::fabs(sigma - F1TB_SIGMA_BANDS[b + 1]) < 1e-12) return F1TB_THR99[b + 1];
        if (sigma < F1TB_SIGMA_BANDS[b + 1])
            return std::max(F1TB_THR99[b], F1TB_THR99[b + 1]);
    }
    return F1TB_THR99[5];
}
// 趋势模型 (不作验收判据; §11.4 判据表说明其残差)
inline double f1tb_thr_model(double sigma) {
    return F1TB_KAPPA99 * (1.0 + F1TB_SIGMA_SMOOTH * F1TB_SIGMA_SMOOTH / (sigma * sigma));
}

enum class F1TbZone { Pos, Transition, Below };

inline F1TbZone f1tb_zone(double sigma, double snr_peak) {
    if (snr_peak >= f1tb_thr99(sigma)) return F1TbZone::Pos;
    if (snr_peak >= F1TB_SNR_FLOOR) return F1TbZone::Transition;
    return F1TbZone::Below;
}

struct FixStarH {
    int w = 512, h = 384;
    double bg = 300.0;
    double noise = 6.0;
    std::vector<double> img;
    std::vector<SynthStar> truth;
};

// SNR 阶梯 20 级: 前 14 级铺满原空档 [10, 32.5) 全宽 (上端 32.4 < 32.5 严格落在
// 空档内), 后 6 级 (46/50/55/65/75/85) 使 σ_psf = 1.0 px 档也有域内真星 ——
// 该档 99% 阈 54.0 高于空档上端, 不补则该档只提供负例、域内召回不被行使。
inline constexpr double F1TB_SNR_LADDER[20] = {10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0,
                                               17.5, 19.0, 21.0, 23.0, 26.0, 29.0, 32.4,
                                               46.0, 50.0, 55.0, 65.0, 75.0, 85.0};

// σ 档 = §11.4 冻结判据表的六档; 峰值对齐像素中心。seed 可换, 供多 seed
// 翻转率研究; 冻结夹具用默认 seed。
inline FixStarH fix_star_h_f1_transition_band(unsigned long long seed = 20260923ull) {
    FixStarH fx;
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> ndist(fx.bg, fx.noise);
    fx.img.assign((std::size_t)fx.w * fx.h, 0.0);
    for (auto& v : fx.img) v = ndist(rng);
    fx.truth.reserve(6 * 20);
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 20; ++c) {
            // +0.5 使星心落在像素几何中心 (峰值对齐像素中心, 冻结表声明域)
            SynthStar s{26.5 + 24.0 * c, 30.5 + 60.0 * r, F1TB_SIGMA_BANDS[r],
                        F1TB_SNR_LADDER[c] * fx.noise};
            fx.truth.push_back(s);
        }
    }
    for (const auto& s : fx.truth)
        add_gaussian_to(fx.img, fx.w, fx.h, s.cx, s.cy, s.amp, s.sigma);
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
