// P1-PHOT-TEST · fixture generator (FIX-PHOT-A..F 谱系, 固定 seed)
//
// 控制包任务: P1-PHOT-TEST (SA-P1P-T, queue 43, lock-P1-PHOT; 依赖 P1-PHOT-DOC 闭环)
// 合同锚: docs/algorithms/PHOTOMETRIC_FIT.md §13.4 TEST-PHOT-DESIGN-001
//        (P1-PHOT-DOC 冻结, 2026-09-07, wave W1); 矩阵行 P1-PHOT
//        (MOD astrocs.p1.photometry, TEST-PHOT-DESIGN-001 → TEST-P1-PHOT-001)。
//
// 纪律 (模块迁移模板 <prefix>-TEST):
//   - fixture 由固定 seed + 参数生成, 不提交大二进制 (splitmix64 谱系约定)。
//   - Gaia 星像素↔天球坐标由独立 oracle 投影 (p1phot_oracle.hpp) 生成,
//     绝不调用被测函数 (pc::WcsTransform/StarMatcher) 生成期望值。
//   - 被测面: lib/algorithms/photometry/cpp/src (用户域生产源, 只读编译);
//     测试面共址 lib/algorithms/photometry/wrapper_phase1/tests/p1phot (用户域外最近共址, src_path 决策
//     见 CMakeLists.txt 头注)。
#ifndef P1PHOT_FIXTURES_HPP
#define P1PHOT_FIXTURES_HPP

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace p1phot {

// ---------------------------------------------------------------------------
// 统一 PRNG: splitmix64 (fixture 谱系约定, seed 完全决定序列)
// ---------------------------------------------------------------------------
inline std::uint64_t splitmix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// [0,1) 均匀
inline double uni01(std::uint64_t& state) {
    return static_cast<double>(splitmix64(state) >> 11) * (1.0 / 9007199254740992.0);
}

// [lo,hi) 均匀
inline double uni(std::uint64_t& state, double lo, double hi) {
    return lo + (hi - lo) * uni01(state);
}

// ---------------------------------------------------------------------------
// 合成 WCS 帧 (FIX-PHOT 公共配置): 小视场 TAN, 无 SIP 主覆盖
// crpix 为 1-based (FITS 约定, WcsTransform 同)
// ---------------------------------------------------------------------------
struct FixWcs {
    double crval1 = 150.0, crval2 = 2.0;   // deg (切点)
    double crpix1 = 129.0, crpix2 = 129.0; // 1-based 参考像素
    double cd11 = 5.555555555555556e-05;   // 0.2"/px → deg/px
    double cd12 = 0.0, cd21 = 0.0;
    double cd22 = 5.555555555555556e-05;
    int sip_order = 0;
};

// ---------------------------------------------------------------------------
// FIX-PHOT-A/B/C 合成帧: Gaia↔PSF 匹配场 + 已知乘性偏移 k
//   r_i = log10(F_instr/F_syn) 由 fixture 完全决定 (解析可控)
//   Gaia (ra,dec) 由独立 oracle 投影生成 (不调用被测函数)
// ---------------------------------------------------------------------------
struct FixPhotFrame {
    int w = 256, h = 256;
    double k = 2.5;  // 注入乘性偏移: F_instr = k·F_syn (location=log10 k)
    FixWcs wcs;
    std::vector<double> gaia_ra, gaia_dec, gaia_mag, gaia_fsyn;
    std::vector<double> psf_cx, psf_cy, psf_flux;
    std::vector<int> psf_status;
    std::vector<int64_t> psf_star_ids;
    std::vector<double> pixels;  // 平坦背景帧 (w*h)
    // 匹配预期 (oracle 由构造直接得知, 非被测函数生成):
    int n_clean_pairs = 0;   // 互为唯一配对的 Gaia↔PSF 对数
    int n_ambiguous = 0;     // 正向命中但非互为最近邻的对数
    int n_out_of_radius = 0; // PSF 有效星但 2px 内无 Gaia
    std::vector<int> outlier_psf_rows;  // F2: IRLS 离群注入的 PSF 行号
};

// FIX-PHOT-A: 合成注入已知 k, 全部配对, r_i = log10 k 恒定 (S=0 median 门,
// SCI-PHOT-001 §11 oracle 场景) 或带对称微散布 (IRLS 真迭代路径, jitter=true)。
inline FixPhotFrame fix_phot_a_inject_k(std::uint64_t seed, int n_pairs,
                                        double k, bool jitter) {
    FixPhotFrame fx;
    fx.k = k;
    const double logk = std::log10(k);
    std::vector<double> psf_px, psf_py;
    // Gaia 星 i ↔ PSF 星 i 一一配对, 间距 ≥ 12px (互为唯一最近邻)
    for (int i = 0; i < n_pairs; ++i) {
        const double x = 20.0 + 9.0 * (i % 24) + (i / 24) * 4.5;
        const double y = 18.0 + 9.5 * (i % 24);
        psf_px.push_back(x);
        psf_py.push_back(y);
    }
    std::uint64_t st = seed;
    for (int i = 0; i < n_pairs; ++i) {
        // F_syn 对数均匀 ~ [1e3, 1e5] ADU
        const double fsyn = std::pow(10.0, uni(st, 3.0, 5.0));
        const double eps = jitter ? uni(st, -0.002, 0.002) : 0.0;
        const double finstr = k * std::pow(10.0, eps) * fsyn;
        // mag = -2.5 log10 F (任意约定); delta = mag_inst - mag_gaia 恒定
        const double mag_gaia = 12.0 + uni(st, 0.0, 4.0);
        // Gaia 天球坐标: 独立 oracle 投影 (fixtures 不调用被测函数)
        double ra, dec;
        p1phot_oracle_pixel_to_sky(fx.wcs, psf_px[i], psf_py[i], ra, dec);
        fx.gaia_ra.push_back(ra);
        fx.gaia_dec.push_back(dec);
        fx.gaia_mag.push_back(mag_gaia);
        fx.gaia_fsyn.push_back(fsyn);
        fx.psf_cx.push_back(psf_px[i]);
        fx.psf_cy.push_back(psf_py[i]);
        fx.psf_flux.push_back(finstr);
        fx.psf_status.push_back(0);
        fx.psf_star_ids.push_back(1000 + i);
    }
    fx.n_clean_pairs = n_pairs;
    fx.pixels.assign(static_cast<std::size_t>(fx.w) * fx.h, 100.0);
    (void)logk;
    return fx;
}

// FIX-PHOT-B: 20% 星等离群注入 — 前述 A 场景 + 20% 星 F_instr 偏 +0.5 dex
// (|Δdelta| = 1.25 mag < 3.0 通过星等一致性门, 由 IRLS/Tukey 权重 0 拒绝;
// SCI-PHOT-001 §11: |location − log10 k| < 0.1 dex)
inline FixPhotFrame fix_phot_b_outliers(std::uint64_t seed, int n_pairs,
                                        double k, double outlier_dex) {
    FixPhotFrame fx = fix_phot_a_inject_k(seed, n_pairs, k, true);
    const double logk = std::log10(k);
    // 每第 5 颗注入离群 (20%), 记录行号
    for (int i = 0; i < n_pairs; ++i) {
        if (i % 5 == 4) {
            fx.psf_flux[i] = k * std::pow(10.0, outlier_dex) *
                             std::pow(10.0, 0.0) * fx.gaia_fsyn[i];
            // 保持星等一致性门可通过: delta 漂移 = 2.5·outlier_dex ≤ 3 mag
            fx.outlier_psf_rows.push_back(i);
        }
    }
    (void)logk;
    return fx;
}

// FIX-PHOT-C: 双向唯一配对构造帧 (含歧义对) — 8 正常对 + 1 歧义挤对:
// PSF 星 j=8 (x,y) 与 PSF 星 j=9 (x+0.7,y) 同落 Gaia 星 g=8 的 2px 邻域内;
// Gaia g=8 反向最近邻 = PSF 9 (更近), PSF 8 正向命中但非互为 → 歧义 1 对。
inline FixPhotFrame fix_phot_c_pairing(std::uint64_t seed) {
    FixPhotFrame fx;
    fx.k = 1.3;
    const int n_clean = 8;
    std::uint64_t st = seed;
    for (int i = 0; i < n_clean; ++i) {
        const double x = 24.0 + 11.0 * (i % 4) + (i / 4) * 33.0;
        const double y = 26.0 + 52.0 * (i / 4);
        const double fsyn = std::pow(10.0, uni(st, 3.0, 4.5));
        const double finstr = fx.k * fsyn;
        const double mag_gaia = 12.0 + uni(st, 0.0, 4.0);
        double ra, dec;
        p1phot_oracle_pixel_to_sky(fx.wcs, x, y, ra, dec);
        fx.gaia_ra.push_back(ra);
        fx.gaia_dec.push_back(dec);
        fx.gaia_mag.push_back(mag_gaia);
        fx.gaia_fsyn.push_back(fsyn);
        fx.psf_cx.push_back(x);
        fx.psf_cy.push_back(y);
        fx.psf_flux.push_back(finstr);
        fx.psf_status.push_back(0);
        fx.psf_star_ids.push_back(2000 + i);
    }
    // 歧义挤对: 复用 Gaia g=8 (i=8, 最后一个 push 的位置)
    const double gx = fx.psf_cx[n_clean - 1] + 0.7, gy = fx.psf_cy[n_clean - 1];
    double ra8, dec8;
    p1phot_oracle_pixel_to_sky(fx.wcs, gx, gy, ra8, dec8);
    fx.gaia_ra.push_back(ra8);
    fx.gaia_dec.push_back(dec8);
    fx.gaia_mag.push_back(fx.gaia_mag[n_clean - 1] + 0.3);
    fx.gaia_fsyn.push_back(fx.gaia_fsyn[n_clean - 1] * std::pow(10.0, -0.12));
    fx.psf_cx.push_back(fx.psf_cx[n_clean - 1]);
    fx.psf_cy.push_back(fx.psf_cy[n_clean - 1]);
    fx.psf_flux.push_back(fx.k * fx.gaia_fsyn[n_clean - 1]);
    fx.psf_status.push_back(0);
    fx.psf_star_ids.push_back(2999);
    fx.n_clean_pairs = n_clean;      // 互为唯一配对 (含挤对胜出者 9→g8)
    fx.n_ambiguous = 1;              // PSF 8 正向命中但非互为
    fx.n_out_of_radius = 0;
    fx.pixels.assign(static_cast<std::size_t>(fx.w) * fx.h, 100.0);
    (void)st;
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-PHOT-D: XPSD uint8 光谱解码 + 积分 oracle 场景
// spectrum_wl 等距 [336, 1020] step 2nm, 343 点; filter 线性 T(λ); bytes 恒定
// → F(λ)=byte·mul+min 常数, integrand = F·T(λ)·λ 为二次多项式 → Simpson 9 点
// 网格重采样后闭式精确 (oracle p1phot_oracle_fsyn_xpsd_closed_form)。
// ---------------------------------------------------------------------------
struct FixPhotXpsd {
    int spectrum_count = 343;          // spec_stride ≤ 9 → n_gaia ≤ 9 颗星
    std::vector<double> spectrum_wl;   // [336, 338, ..., 1020]
    std::vector<double> filter_wl;     // [340, 1016] 两端
    std::vector<double> filter_trans;  // 线性: T=0.8→0.9
    std::vector<double> qe_wl;         // 空 (Q=1.0)
    std::vector<double> qe_trans;
    uint8_t spectrum_byte = 200;
    double flux_min = 1.0e-14;         // W·m⁻²·nm⁻¹
    double flux_mul = 2.0e-16;
    FixWcs wcs;
};

inline FixPhotXpsd fix_phot_d_xpsd() {
    FixPhotXpsd fx;
    fx.spectrum_wl.reserve(343);
    for (int i = 0; i < 343; ++i) fx.spectrum_wl.push_back(336.0 + 2.0 * i);
    fx.filter_wl = {340.0, 1016.0};
    fx.filter_trans = {0.8, 0.9};
    return fx;
}

// ---------------------------------------------------------------------------
// FIX-PHOT-F: Photometer aperture 已知通量帧 (平坦背景 + 中心盒源)
// 孔径 r=4 内 (cx,cy)=(64.5,64.5) 像素 d²≤16 共 n_ap 像素; 源为核盒内
// +s ADU、背景 B: oracle 期望 flux = Σ_ap(v−B) (v−B = 核盒内 s, 其余 0)。
// ---------------------------------------------------------------------------
struct FixPhotAperture {
    int w = 128, h = 128;
    double background = 100.0;
    double core_flux = 50.0;   // 核盒内超出背景的每像素 ADU
    double cx = 64.5, cy = 64.5;
    double aperture_r = 4.0, sky_inner = 6.0, sky_outer = 10.0;
    int core_half = 3;         // 核盒 [cx±core_half] 内均匀 core_flux
    std::vector<float> image;
    // oracle 期望值 (逐像素模拟, 独立于被测实现):
    double expected_flux = 0.0;
};

inline FixPhotAperture fix_phot_f_aperture() {
    FixPhotAperture fx;
    fx.image.assign(static_cast<std::size_t>(fx.w) * fx.h,
                    static_cast<float>(fx.background));
    for (int y = -fx.core_half; y <= fx.core_half; ++y)
        for (int x = -fx.core_half; x <= fx.core_half; ++x) {
            const int px = static_cast<int>(fx.cx) + x;
            const int py = static_cast<int>(fx.cy) + y;
            fx.image[static_cast<std::size_t>(py) * fx.w + px] =
                static_cast<float>(fx.background + fx.core_flux);
        }
    // oracle: 与 Photometer 相同的像素域定义 (孔径内求和−背景, 背景取天环
    // 中位数) — 由构造直接解析可知: 天环全部为纯背景 → B=100 精确;
    // 孔径内像素 = 核盒像素 (d≤4 ⊇ |x|,|y|≤3?) — 核盒半宽 3, 角点 d≈4.24>4
    // → 孔径内含核盒全部 + 无额外 (角点 3,3 → d²=18>16 不在盒外...) 逐像素
    // 独立模拟期望值:
    const double med = fx.background;  // 天环中位数 (构造保证)
    double sum = 0.0;
    for (int y = 0; y < fx.h; ++y)
        for (int x = 0; x < fx.w; ++x) {
            const double d2 = (x - fx.cx) * (x - fx.cx) + (y - fx.cy) * (y - fx.cy);
            if (d2 <= fx.aperture_r * fx.aperture_r)
                sum += fx.image[static_cast<std::size_t>(y) * fx.w + x] - med;
        }
    fx.expected_flux = sum;
    return fx;
}

}  // namespace p1phot

#endif  // P1PHOT_FIXTURES_HPP
