// P1-DRZ-TEST · FIX-DRZ-A..F 合成 fixture generator
//
// 合同锚: docs/algorithms/DRIZZLE_GEOMETRY.md §9 TEST-DRZ-DESIGN-001
// (P1-DRZ-DOC 冻结, 2026-09-07, wave W1); SCI=SCI-DRZ-001
// (docs/science/DRIZZLE.md FROZEN T105 2026-08-23, 集合 014/015/016)。
//
// 规则 (模板 <prefix>-TEST §2, MODULE_MIGRATION_TEMPLATE.md):
//   - 全 fixture 由固定 seed + 参数确定生成, 零随机硬件依赖, 不提交大二进制。
//   - PRNG 沿用本仓 splitmix64 统一谱系 (与 p1cal/p1cos fixture 一致)。
//   - 常量场按 SCI-003 冻结语义构造: 常数**面亮度** B0 → x_j = B0·A_pixel_j
//     (每像素通量随真实球面像素面积变化), 绝不使用"每像素常量 ADU"
//     冒充常量天空 (DRIZZLE.md §5/§7 禁止混淆)。
//   - fixture 值只进被测函数的输入侧; 期望值一律由 p1drz_oracle.hpp
//     独立推导, 绝不经过被测函数 (模板 §3)。
//
// 域上下文: 以 HEAD 09ee5363 (P1-DRZ-NONFINITE) 为基线, 与既有
// drizzle_nonfinite_test (负面合同 18 断言) 互补不重复: 本文件 FIX-DRZ-E
// 只做 NaN/Inf 传播**正面**用例 (污染 leaf 集合精确断言), 合同所有权在
// drizzle_nonfinite_test。
#ifndef P1DRZ_FIXTURES_HPP
#define P1DRZ_FIXTURES_HPP

#include "fits_reader.h"
#include "p1drz_geom.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace p1drz {

// TAN 像素真实球面面积 (sr): 测试侧独立几何 (Van Oosterom 四角立体角)。
// 注意 TAN 度量为 dΩ=dξdη/μ³, **没有**等矩形 cos(dec) 因子 — 早期版本
// 误用 scale²·cos(dec) 模型导致 S_p 量级偏差 3e4 (μ³ 与 cos 因子在
// 20° dec 恰好反号, 已由 dbg_units 实证)。
inline double geom_pixel_area(const drizzle::WcsParams& wcs, double x, double y) {
    return geom_quad_area(geom_drop_polygon(wcs, x, y, 1.0));
}

// 统一 PRNG: splitmix64 (fixture 谱系约定, seed 完全决定序列)
inline std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline double uniform01(std::uint64_t& state) {
    return static_cast<double>(splitmix64(state) >> 11) * (1.0 / 9007199254740992.0);
}

inline double normal01(std::uint64_t& state) {
    // Box-Muller; u1 ∈ (0,1] 防止 log(0)
    double u1 = uniform01(state);
    while (u1 <= 0.0) u1 = uniform01(state);
    double u2 = uniform01(state);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.28318530717958647 * u2);
}

// ---------------------------------------------------------------------------
// 公共 WCS: TAN, dec0=20°, 正方形 CD (无 SIP), 与既有 drizzle 科学测试
// (variance_propagation_test / drizzle_nonfinite_test) 同一 fixture 家族。
// scale_arcsec/px, width/height, 可选 SIP (FIX-DRZ-F)。
// ---------------------------------------------------------------------------
inline void setup_wcs(drizzle::FitsImage& im, int width, int height,
                      double scale_arcsec = 300.0, bool with_sip = false) {
    im.width = width;
    im.height = height;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)width * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)height * 0.5 + 0.5;
    const double deg_per_px = scale_arcsec / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
    if (with_sip) {
        // FIX-DRZ-F: 3 阶前向 SIP 畸变 (A/B 下三角, 系数单位 = 像素)。
        // 15° 宽场 patch 的边缘畸变量级 (px): 视场半径 7.5°, 三阶项 ~1e-3 px
        // 系数 × 像素偏移^3 (偏移 ~±8 px) → 边缘 ~1 px 量级畸变。
        im.wcs.sip.order = 3;
        im.wcs.sip.a[0 * 6 + 2] = 2.0e-3;    // A02
        im.wcs.sip.a[1 * 6 + 2] = 1.0e-3;    // A12
        im.wcs.sip.a[2 * 6 + 0] = -1.5e-3;   // A20
        im.wcs.sip.a[3 * 6 + 0] = 0.5e-3;    // A30
        im.wcs.sip.b[0 * 6 + 3] = -2.0e-3;   // B03
        im.wcs.sip.b[1 * 6 + 1] = 0.8e-3;    // B11
        im.wcs.sip.b[2 * 6 + 1] = 0.6e-3;    // B21
        im.wcs.sip.b[3 * 6 + 0] = 1.5e-3;    // B30
        std::strncpy(im.wcs.ctype1, "RA---TAN-SIP", sizeof(im.wcs.ctype1) - 1);
        std::strncpy(im.wcs.ctype2, "DEC--TAN-SIP", sizeof(im.wcs.ctype2) - 1);
    }
}

inline drizzle::DrizzleConfig make_cfg(int nside, double pixfrac, int threads,
                                       bool fp64) {
    drizzle::DrizzleConfig c;
    c.nside = nside;
    c.nested = true;
    c.pixfrac = pixfrac;
    c.threads = threads;                     // 0=auto; 科学测试显式 1/2/4
    c.apply_photometry = true;               // 元数据标记 (B5: 不再应用)
    c.photometry_applied_upstream = true;
    c.tile_depth = 9;                        // HiPS 512x512 叶 tile
    c.precision_mode = fp64 ? 1 : 0;
    return c;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-A 常量面亮度场: B(Ω)=B0, x_j = B0·A_pixel_j。
// A_pixel_j 由测试侧独立几何逐像素精确给值 (TAN 度量, geom_pixel_area),
// 与生产 drop=pixel (pixfrac=1) 口径一致 → S_p=B0 恒等式残差只剩
// Van Oosterom vs 生产 Eriksson 的算法差 (~1e-6)。
// 与"每像素常量 ADU"对照件 fix_drz_a_const_adu 一起构成 SCI-003 语义对。
// ---------------------------------------------------------------------------
inline drizzle::FitsImage fix_drz_a_const_sb(int w, int h, double b0,
                                             double scale_arcsec = 300.0,
                                             bool with_sip = false) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, scale_arcsec, with_sip);
    im.pixels.assign((std::size_t)w * h, 0.0f);
    im.pixels_f64.assign((std::size_t)w * h, 0.0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double v = b0 * geom_pixel_area(im.wcs, (double)x, (double)y);
            im.pixels[(std::size_t)y * w + x] = (float)v;
            im.pixels_f64[(std::size_t)y * w + x] = v;
        }
    return im;
}

// SCI-003 对照件: 每像素常量 ADU C (非面亮度语义) — 期望 S_p = C/A_drop ≠ C。
inline drizzle::FitsImage fix_drz_a_const_adu(int w, int h, double c,
                                              double scale_arcsec = 300.0) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, scale_arcsec, false);
    im.pixels.assign((std::size_t)w * h, (float)c);
    im.pixels_f64.assign((std::size_t)w * h, c);
    return im;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-B 点源高斯 (总通量守恒素材): 背景 0 + amp·Gaussian(中心, sigma)。
// 全部像素 ≥0, 视场内完全覆盖 → Σ_p F_p ≈ Σ_j x_j (FP64 <1e-6 冻结门)。
// ---------------------------------------------------------------------------
inline drizzle::FitsImage fix_drz_b_gaussian(int w, int h, double amp,
                                             double sigma_px, std::uint64_t seed,
                                             double noise_sigma = 0.0) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, 300.0, false);
    im.pixels.assign((std::size_t)w * h, 0.0f);
    im.pixels_f64.assign((std::size_t)w * h, 0.0);
    const double cx = w * 0.5 - 0.5, cy = h * 0.5 - 0.5;
    std::uint64_t st = seed;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double dx = x - cx, dy = y - cy;
            double v = amp * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma_px * sigma_px));
            if (noise_sigma > 0.0) v += noise_sigma * normal01(st);
            im.pixels[(std::size_t)y * w + x] = (float)v;
            im.pixels_f64[(std::size_t)y * w + x] = v;
        }
    return im;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-C 线性梯度场 (面亮度语义): B(Ω) 随像素位置线性变化。
// 期望由 oracle 逐 leaf 独立重建 (w_jp 全重算)。
// ---------------------------------------------------------------------------
inline drizzle::FitsImage fix_drz_c_gradient(int w, int h, double b0,
                                             double gx, double gy,
                                             double scale_arcsec = 300.0) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, scale_arcsec, false);
    im.pixels.assign((std::size_t)w * h, 0.0f);
    im.pixels_f64.assign((std::size_t)w * h, 0.0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            // 像素中心即面亮度采样点 (小梯度: 像素内变化 ≪ 梯度尺度);
            // x_j = B(x,y)·A_pixel_j (面亮度语义, 同 FIX-DRZ-A)
            const double b = b0 + gx * (double)x + gy * (double)y;
            const double v = b * geom_pixel_area(im.wcs, (double)x, (double)y);
            im.pixels[(std::size_t)y * w + x] = (float)v;
            im.pixels_f64[(std::size_t)y * w + x] = v;
        }
    return im;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-D 脉冲单像素: 除 (px,py) 外全 0, 该像素 = amp (总通量语义)。
// 期望 (oracle 独立推导): 完全覆盖的 touched leaf 上
//   S_p = F_p/D_p = amp·w_jp/ΣA_jp = amp/A_drop,pixel (面亮度! ≠ amp),
// 与 SCI-003 常量 ADU 语义对一致 — 检验 D_p 归一化语义。
// ---------------------------------------------------------------------------
inline drizzle::FitsImage fix_drz_d_impulse(int w, int h, int px, int py,
                                            double amp) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, 300.0, false);
    im.pixels.assign((std::size_t)w * h, 0.0f);
    im.pixels_f64.assign((std::size_t)w * h, 0.0);
    im.pixels[(std::size_t)py * w + px] = (float)amp;
    im.pixels_f64[(std::size_t)py * w + px] = amp;
    return im;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-E NaN/Inf 注入面 (传播正面用例, 与 drizzle_nonfinite_test 互补):
// 常量面亮度场 + 指定索引 {NaN, +Inf, -Inf}。
// 冻结承诺 DRIZZLE.md:96: 值 NaN/Inf 经 F_p=Σx_j·w_jp 直接传播, drizzle
// 层不掩膜 (09ee5363 已删 4 处 isfinite→continue 静默掩膜)。
// 返回注入索引列表供 oracle 断言污染 leaf 集合。
// ---------------------------------------------------------------------------
struct FixDrzE {
    drizzle::FitsImage im;
    int nan_idx = -1, pinf_idx = -1, ninf_idx = -1;
};

inline FixDrzE fix_drz_e_nonfinite(int w, int h, double b0,
                                   double scale_arcsec = 300.0) {
    FixDrzE fx;
    fx.im = fix_drz_a_const_sb(w, h, b0, scale_arcsec, false);
    fx.nan_idx = (h / 2) * w + (w / 2);          // 中心 NaN
    fx.pinf_idx = (h / 2) * w + (w / 2 + 1);     // 中心右 +Inf
    fx.ninf_idx = (h / 2 + 1) * w + (w / 2);     // 中心下 -Inf
    const float fnan = std::numeric_limits<float>::quiet_NaN();
    const float finf = std::numeric_limits<float>::infinity();
    fx.im.pixels[(std::size_t)fx.nan_idx] = fnan;
    fx.im.pixels_f64[(std::size_t)fx.nan_idx] = std::numeric_limits<double>::quiet_NaN();
    fx.im.pixels[(std::size_t)fx.pinf_idx] = finf;
    fx.im.pixels_f64[(std::size_t)fx.pinf_idx] = std::numeric_limits<double>::infinity();
    fx.im.pixels[(std::size_t)fx.ninf_idx] = -finf;
    fx.im.pixels_f64[(std::size_t)fx.ninf_idx] = -std::numeric_limits<double>::infinity();
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-DRZ-F SIP 畸变边缘 patch (setup_wcs with_sip=true 载入 3 阶系数):
// 确定性 1/2/4 线程 bitwise + FP64 恒等素材; 场取常量面亮度。
// ---------------------------------------------------------------------------
inline drizzle::FitsImage fix_drz_f_sip_patch(int w, int h, double b0) {
    return fix_drz_a_const_sb(w, h, b0, 300.0, true);
}

// FIX-DRZ 参数域负面素材 (值域无关, 仅形状)
inline drizzle::FitsImage fix_drz_f_buffer(int w, int h, float fill) {
    drizzle::FitsImage im;
    setup_wcs(im, w, h, 300.0, false);
    im.pixels.assign((std::size_t)w * h, fill);
    im.pixels_f64.assign((std::size_t)w * h, (double)fill);
    return im;
}

}  // namespace p1drz

#endif  // P1DRZ_FIXTURES_HPP
