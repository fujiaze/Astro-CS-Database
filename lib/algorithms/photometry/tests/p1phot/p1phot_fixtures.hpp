// P1-PHOT-TEST · 固定 seed fixture (F1-F6, TEST-PHOT-DESIGN-001 冻结设计)
//
// 模板 <prefix>-TEST: "fixture 由固定 seed+参数生成, 不提交大二进制"。
// RNG = splitmix64 (固定 seed, 无全局状态, 逐用例可重放); 期望值一律由
// p1phot_oracle.hpp 独立实现生成, fixture 只负责装配输入场。
//
//   FIX-PHOT-A (F1): 合成注入已知乘性偏移 k — F_instr=k·F_syn 精确场
//                    (SCI-PHOT-001 §11: location≈log10 k, rtol 1e-4);
//   FIX-PHOT-B (F2): 20% 星等离群注入 (Δlocation<0.1 dex 冻结门);
//   FIX-PHOT-C (F3): 双向唯一配对构造帧 (含歧义对/超距对);
//   FIX-PHOT-D (F4): XPSD uint8 光谱解码 + 1.0nm 积分 (rtol 1e-9);
//   FIX-PHOT-E (F5): 退化输入矩阵 (逐用例参数化, 负面组消费);
//   FIX-PHOT-F (F6): aperture 已知通量帧 (对齐 p1_wcs_phot_test 4 组)。
//
// Gaia 表 ra/dec 由 oracle WCS 正变换从目标像素反推 (fixture 装配允许用
// oracle 数学; 期望值仍不出自被测函数)。
#ifndef P1PHOT_FIXTURES_HPP
#define P1PHOT_FIXTURES_HPP

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "p1phot_oracle.hpp"

namespace p1phot {
namespace fix {

// splitmix64 (固定 seed; 逐序列可重放, 无全局状态)
struct SplitMix64 {
    std::uint64_t state;
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // [0,1) 均匀
    double uniform() {
        return (double)(next() >> 11) * (1.0 / 9007199254740992.0);
    }
    // [lo,hi) 均匀
    double uniform(double lo, double hi) { return lo + (hi - lo) * uniform(); }
    // ±amp 均匀对称
    double sym(double amp) { return uniform(-amp, amp); }
};

// 共用帧/WCS 几何 (fixture 常数; 任意可行值, 非被测域契约值)
struct FrameGeom {
    int width = 64;
    int height = 64;
    double crval1 = 180.0;
    double crval2 = 45.0;
    double crpix1 = 32.5;   // 1-based
    double crpix2 = 32.5;
    double cd11 = -2.8e-4;  // ≈1.008 "/px, 北向上
    double cd12 = 0.0;
    double cd21 = 0.0;
    double cd22 = 2.8e-4;
};

// Gaia 参考星 + 对应 PSF 拟合星 (F1/F2/F3 共用装配)
struct StarField {
    std::vector<double> gaia_ra, gaia_dec, gaia_mag, gaia_fsyn;
    std::vector<double> psf_cx, psf_cy, psf_flux;
    std::vector<int> psf_status;
    std::vector<std::int64_t> psf_star_ids;
};

// v2 通道常数谱 XPSD 闭式 F_syn (桩常数谱 200 + min=1e-15f + mul=5e-18f +
// 两点常数滤光片 T=0.5 + 无 QE; Simpson 对线性被积函数精确):
//   F_syn = f0·T0·(λmax²−λmin²)/2, f0=200·(double)mul_f+(double)min_f
// 注意量化参数以 float 存储 → 闭式必须从 (double)(float) 值构造。
inline double v2_fsyn_const() {
    const double f0 = 200.0 * (double)5.0e-18f + (double)1.0e-15f;
    return f0 * 0.5 * (1020.0 * 1020.0 - 336.0 * 336.0) / 2.0;
}

// 由目标像素经 oracle WCS 正变换生成 Gaia 表 (ra/dec), 并在同像素放置 PSF 星。
// fsyn_ref: F_syn 参考语义值 —
//   0 (默认): psf_flux=fsyn_base·k·10^ε, gaia_mag=−2.5log10(fsyn 星值)
//     (直通通道语义: gaia_fsyn 即参考流量, 自洽);
//   >0: psf_flux=fsyn_ref·k·10^ε, gaia_mag=−2.5log10(fsyn_ref)
//     (v2 通道语义: F_syn 由桩光谱积分决定 → 调用方传 v2_fsyn_const())。
inline void build_matched_field(const FrameGeom& g, const oracle::OracleWcs& wcs,
                                const std::vector<std::pair<double, double>>& pixels,
                                double k, double fsyn_base, SplitMix64& rng,
                                double perturb_amp, StarField& out,
                                double fsyn_ref = 0.0) {
    out.gaia_ra.clear(); out.gaia_dec.clear(); out.gaia_mag.clear(); out.gaia_fsyn.clear();
    out.psf_cx.clear(); out.psf_cy.clear(); out.psf_flux.clear();
    out.psf_status.clear(); out.psf_star_ids.clear();
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const double x = pixels[i].first, y = pixels[i].second;
        double ra, dec;
        wcs.pixel_to_sky(x, y, ra, dec);   // oracle 正变换 (fixture 装配)
        const double fsyn = fsyn_base + 100.0 * rng.uniform();  // 正值互异
        const double eps = (perturb_amp > 0.0) ? rng.sym(perturb_amp) : 0.0;
        const double ref = (fsyn_ref > 0.0) ? fsyn_ref : fsyn;
        out.gaia_ra.push_back(ra);
        out.gaia_dec.push_back(dec);
        out.gaia_mag.push_back(-2.5 * std::log10(ref));
        out.gaia_fsyn.push_back(fsyn);
        out.psf_cx.push_back(x);
        out.psf_cy.push_back(y);
        out.psf_flux.push_back(ref * k * std::pow(10.0, eps));
        out.psf_status.push_back(0);
        out.psf_star_ids.push_back(1000 + (std::int64_t)i);
    }
}

// FIX-PHOT-A: F1 精确 k 场 (n_star 颗, 无扰动; 位置=互异格点, 全帧内)
inline StarField fixture_f1(const FrameGeom& g, double k, int n_star) {
    oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                          g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
    // 固定格点 (避开边缘 8px, 间隔≥3px): 确定性像素表
    std::vector<std::pair<double, double>> pixels;
    for (int row = 0; row < 6 && (int)pixels.size() < n_star; ++row)
        for (int col = 0; col < 4 && (int)pixels.size() < n_star; ++col)
            pixels.emplace_back(10.0 + 12.0 * col + 1.0 * row, 10.0 + 9.0 * row);
    SplitMix64 rng2(0x5EED000000000001ULL);
    StarField f;
    build_matched_field(g, wcs, pixels, k, 1000.0, rng2, 0.0, f);
    return f;
}

// FIX-PHOT-B: F2 = 扰动 v2 场 (ε=±0.02 dex, S>0) + 20% IRLS 级离群
// (+0.8 dex; mag 偏移 2.0<3.0 不触发星等预过滤, 走 IRLS-outlier 通道 —
// reason=2 可断言; 扰动基底保证 S>0, 否则 S=0 门直出 median 吞掉离群)。
// F_syn 参考语义 = v2_fsyn_const() (F2 冻结设计 "F_instr=k·F_syn 注入场"
// 在 v2 通道下 F_syn 由桩常数谱积分决定)。
inline StarField fixture_f2(const FrameGeom& g, double k, int n_star, int n_outlier) {
    oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                          g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
    std::vector<std::pair<double, double>> pixels;
    for (int row = 0; row < 6 && (int)pixels.size() < n_star; ++row)
        for (int col = 0; col < 4 && (int)pixels.size() < n_star; ++col)
            pixels.emplace_back(10.0 + 12.0 * col + 1.0 * row, 10.0 + 9.0 * row);
    SplitMix64 rng(0x5EED000000000002ULL);
    StarField f;
    build_matched_field(g, wcs, pixels, k, 1000.0, rng, 0.02, f, v2_fsyn_const());
    // 离群星 = 前 n_outlier 颗 (确定性); r 偏移 +0.8 dex = flux ×10^0.8
    for (int i = 0; i < n_outlier && i < (int)f.psf_flux.size(); ++i)
        f.psf_flux[(std::size_t)i] *= std::pow(10.0, 0.8);
    return f;
}

// FIX-PHOT-C: F3 双向唯一配对构造帧 (ALG-PHOT-002 歧义/超距构造)
//   g0 = A/B 中间偏 A (双向→A, B 歧义拒绝); g1↔C 精确互近邻 (唯一对);
//   PSF E 远离一切 Gaia (rejected_distance); Gaia g2 无 PSF 在半径内。
inline StarField fixture_f3(const FrameGeom& g) {
    oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                          g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
    StarField f;
    struct Spec { double px, py, fsyn; };
    // Gaia g0 投影在 (30.4, 30.0); g1 在 (45.0, 45.0); g2 在 (55.0, 20.0)
    const Spec gs[] = {{30.4, 30.0, 1200.0}, {45.0, 45.0, 1500.0}, {55.0, 20.0, 900.0}};
    for (const Spec& s : gs) {
        double ra, dec;
        wcs.pixel_to_sky(s.px, s.py, ra, dec);
        f.gaia_ra.push_back(ra);
        f.gaia_dec.push_back(dec);
        f.gaia_mag.push_back(-2.5 * std::log10(s.fsyn));
        f.gaia_fsyn.push_back(s.fsyn);
    }
    struct PSpec { double px, py, flux; int status; std::int64_t id; };
    // A(30.0,30.0) d(g0)=0.4 → 双向唯一; B(31.5,30.0) d(g0)=1.1<2 → 正向命中
    // g0 但 g0 反向最近是 A → B 歧义; C(45.05,45.0)↔g1 互近邻; E(15.0,50.0)
    // 距最近 Gaia >2 → rejected_distance; F(55.0,20.05)↔g2 互近邻
    const PSpec ps[] = {{30.0, 30.0, 1200.0 * 1.0, 0, 2001},
                        {31.5, 30.0, 900.0, 0, 2002},
                        {45.05, 45.0, 1500.0, 0, 2003},
                        {15.0, 50.0, 700.0, 0, 2004},
                        {55.0, 20.05, 900.0, 0, 2005},
                        {20.0, 20.0, 600.0, 1, 2006}};  // status=1: psf-invalid
    for (const PSpec& s : ps) {
        f.psf_cx.push_back(s.px);
        f.psf_cy.push_back(s.py);
        f.psf_flux.push_back(s.flux);
        f.psf_status.push_back(s.status);
        f.psf_star_ids.push_back(s.id);
    }
    return f;
}

// FIX-PHOT-D: F4 XPSD 光谱装配 (343 点 [336,1020]nm step 2; 固定 seed uint8;
// 滤光片/QE 曲线严格递增)
inline void fixture_f4_spectra(SplitMix64& rng,
                               std::vector<double>& spec_wl,
                               std::vector<std::vector<uint8_t>>& spectra,
                               std::vector<float>& flux_min,
                               std::vector<float>& flux_mul,
                               std::vector<double>& filter_wl,
                               std::vector<double>& filter_trans,
                               std::vector<double>& qe_wl,
                               std::vector<double>& qe_trans) {
    spec_wl.clear();
    for (int wl = 336; wl <= 1020; wl += 2) spec_wl.push_back((double)wl);
    const int n = (int)spec_wl.size();
    spectra.clear(); flux_min.clear(); flux_mul.clear();
    for (int s = 0; s < 4; ++s) {
        std::vector<uint8_t> spec(n);
        for (int i = 0; i < n; ++i) spec[(std::size_t)i] = (uint8_t)(rng.next() & 0xFF);
        spectra.push_back(spec);
        flux_min.push_back((float)(1.0e-15 + 1.0e-16 * rng.uniform()));
        flux_mul.push_back((float)(5.0e-18 + 1.0e-18 * rng.uniform()));
    }
    // 滤光片: 高斯形 21 点 [400,960] (T 峰 0.9)
    filter_wl.clear(); filter_trans.clear();
    for (int i = 0; i < 21; ++i) {
        const double wl = 400.0 + 28.0 * i;
        filter_wl.push_back(wl);
        filter_trans.push_back(0.9 * std::exp(-0.5 * std::pow((wl - 678.0) / 60.0, 2)));
    }
    // QE: 缓变 17 点 [350,1010]
    qe_wl.clear(); qe_trans.clear();
    for (int i = 0; i < 17; ++i) {
        const double wl = 350.0 + 41.25 * i;
        qe_wl.push_back(wl);
        qe_trans.push_back(0.6 + 0.25 * std::exp(-0.5 * std::pow((wl - 550.0) / 180.0, 2)));
    }
}

// FIX-PHOT-E: F5 退化矩阵基底 (正常小场, 负面组在其上逐参数破坏)
inline StarField fixture_f5_base(const FrameGeom& g) {
    return fixture_f1(g, 1.25, 8);
}

// FIX-PHOT-F: F6 aperture 帧 — 常数背景 + 中心方盒亮源 (flux 可由 oracle
// 独立逐像素求和精确复算; 另含 ±对称噪声产生非零 σ_sky)
inline std::vector<float> fixture_f6_image(int w, int h, double bg,
                                           double box_val, int box_half,
                                           double cx, double cy,
                                           SplitMix64& rng) {
    std::vector<float> img((std::size_t)w * h, (float)bg);
    for (std::size_t i = 0; i < img.size(); ++i) img[i] = (float)(bg + rng.sym(2.0));
    const int cx0 = (int)std::floor(cx), cy0 = (int)std::floor(cy);
    for (int dy = -box_half; dy <= box_half; ++dy)
        for (int dx = -box_half; dx <= box_half; ++dx) {
            const int x = cx0 + dx, y = cy0 + dy;
            if (x >= 0 && x < w && y >= 0 && y < h)
                img[(std::size_t)y * w + x] = (float)box_val;
        }
    return img;
}

}  // namespace fix
}  // namespace p1phot

#endif  // P1PHOT_FIXTURES_HPP
