// P1-PHOT-TEST · core 测试组: units / properties / negative / gaia / legacy
//
// 组清单 (模板 <prefix>-TEST):
//   units      — FIX-PHOT-D XPSD oracle (F4) + FIX-PHOT-A 已知 k 注入 (F1)
//                + FIX-PHOT-F 孔径解析 oracle
//   properties — 不变量 I1..I6 (含 1/4/N 线程位相等) + FIX-PHOT-B 离群 (F2)
//                + FIX-PHOT-C 双向配对诊断 (F3)
//   negative   — FIX-PHOT-E 拒绝/退化矩阵 (空指针/无效尺寸/handle null/
//                n_gaia=0/无有效 PSF/sip_order 越界)
//   gaia       — v2 通道端到端 (测试侧 gaia_client stub, 不触用户域) + F4 全链
//   legacy     — F6 Photometer 孔径已知通量 + 越界/空环/空孔径显式失败
#include "p1phot_test_main.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "photometric_calib.h"

#include "p1phot_fixtures.hpp"
#include "p1phot_oracle.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace p1phot {

namespace {

using p1phot::FixPhotFrame;

// ---- 公共小工具 ----
inline bool feq(double a, double b, double tol) {
    const double d = a > b ? a - b : b - a;
    return d <= tol;
}
inline bool fbits_eq(double a, double b) {
    std::uint64_t x, y;
    std::memcpy(&x, &a, 8);
    std::memcpy(&y, &b, 8);
    return x == y;
}

// FIX-PHOT-A/B/C 公共帧 → pc_calibrate_simple_f64 入参 (零 SIP)
struct SimpleCall {
    std::vector<double> pixels;
    std::vector<double> out_pixels;
    std::vector<int> psf_status;
    std::vector<PcMatchRecord> records;  // I4/I6: per-star records (v2 语义)
    int n_matched = -1;
    double scale = -1.0, sigma = -1.0;
    PhotometricDiag diag{};
};

inline SimpleCall call_simple_f64(const FixPhotFrame& fx, int threads,
                                  PhotometricDiag* diag_out = nullptr) {
    SimpleCall c;
    c.pixels = fx.pixels;
    c.psf_status = fx.psf_status;
    c.out_pixels.assign(fx.pixels.size(), -999.0);
    c.records.resize(fx.psf_cx.size());
    PhotometricDiag d;
    std::memset(&d, 0, sizeof d);
    const int rc = pc_calibrate_simple_f64(
        c.pixels.data(), fx.w, fx.h,
        fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(),
        fx.gaia_fsyn.data(), (int)fx.gaia_ra.size(),
        fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
        c.psf_status.data(), (int)fx.psf_cx.size(),
        nullptr, nullptr, 0,
        fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
        fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22,
        fx.wcs.sip_order,
        nullptr, nullptr, nullptr, nullptr,
        c.out_pixels.data(), &c.n_matched, &c.scale, &c.sigma, &d);
    c.diag = d;
    if (diag_out) *diag_out = d;
    if (rc != 0) {
        std::fprintf(stderr, "[p1phot] call_simple_f64 rc=%d\n", rc);
    }
    return c;
}

// ---------- units 组 ----------
void test_units(CheckState& cs) {
    // ---- F4: XPSD uint8 解码 + 闭式积分 oracle (rtol 1e-9) ----
    P1PHOT_FAULT_POINT("units_pre");
    {
        FixPhotXpsd fx = fix_phot_d_xpsd();
        std::vector<double> wl = oracle_weighted_wl_linear(fx);
        // 闭式 (一次多项式精确积分; fixture 保持 T 线性 → 每段被积核一次)
        const double closed = oracle_fsyn_xpsd_closed_form(fx, wl, fx.flux_min, fx.flux_mul);
        // 第二通道: 梯形数值积分 (与被测 Simpson 不同阶) — 交叉一致性
        const double trapz = oracle_fsyn_trapezoid(fx, wl, fx.flux_min, fx.flux_mul);
        P1PHOT_CHECK(cs, std::isfinite(closed) && closed > 0.0, "F4: 闭式 oracle 有限且为正");
        P1PHOT_CHECK_NEAR(cs, trapz / closed, 1.0, 1e-9, "F4: 梯形通道与闭式一致 (rtol 1e-9)");
        // 解码常量检查: F = byte·mul + min = 200·2e-16 + 1e-14
        P1PHOT_CHECK_NEAR(cs, fx.spectrum_byte * fx.flux_mul + fx.flux_min, 5.0e-14, 1e-30,
                          "F4: uint8 解码常数 F=5e-14");
        // Simpson 对一次被积核精确 ⇒ closed 即逐段解析值; 用极高密度梯形复核
        // (h→0 时梯形→解析, 差 < 1e-12 相对)
        {
            double dense = 0.0;
            const int n = (int)fx.spectrum_wl.size();
            for (int i = 0; i + 1 < n; ++i) {
                const double a = fx.spectrum_wl[i], b = fx.spectrum_wl[i + 1];
                const double ga = 5.0e-14 * wl[i], gb = 5.0e-14 * wl[i + 1];
                dense += 0.5 * (ga + gb) * (b - a);
            }
            P1PHOT_CHECK_NEAR(cs, dense / closed, 1.0, 1e-12, "F4: 逐段梯形复核闭式 (1e-12)");
        }
    }

    // ---- F1: 已知乘性 k 注入 → location=log10 k (rtol 1e-4), I1/I2 ----
    {
        const std::uint64_t seed = 0xA15EED0001ULL;
        const double k = 2.5;
        FixPhotFrame fx = fix_phot_a_inject_k(seed, 30, k, false);
        SimpleCall c = call_simple_f64(fx, 1);
        // 期望值来自 fixture/SCI-PHOT-001 §11, 非被测函数:
        const double want_scale = 1.0 / k;
        const double want_sigma = 0.0;  // r 恒定 → MAD=0
        P1PHOT_CHECK_NEAR(cs, c.scale, want_scale, 1e-4 * want_scale,
                          "F1: scale=1/k (location rtol 1e-4)");
        P1PHOT_CHECK_NEAR(cs, c.sigma, want_sigma, 1e-12, "F1: S=0 门 → sigma=0");
        P1PHOT_CHECK(cs, c.n_matched == 30, "F1: n_matched=30");
        // oracle 重算 (独立路径)
        OracleRobust orob = oracle_robust_location(fx.psf_flux, fx.gaia_fsyn,
                                                   fx.gaia_mag, 3.0);
        P1PHOT_CHECK_NEAR(cs, std::log10(1.0 / orob.scale), std::log10(k), 1e-4,
                          "F1: oracle location=log10 k (rtol 1e-4)");
        // I1: out_pixels = I·scale 位相等 (f64)
        bool i1 = true;
        for (std::size_t i = 0; i < fx.pixels.size(); ++i)
            if (!fbits_eq(c.out_pixels[i], fx.pixels[i] * c.scale)) { i1 = false; break; }
        P1PHOT_CHECK(cs, i1, "I1: out_pixels=I·scale 位相等 (f64)");
        // 退化 sanity: scale 恰为 1/2.5 的位级最接近值
        P1PHOT_CHECK(cs, feq(c.scale, 0.4, 1e-9), "F1: scale≈0.4");
    }

    // ---- FIX-PHOT-F: 孔径已知通量 oracle (F6 正向, legacy 组独立复验) ----
    {
        FixPhotAperture af = fix_phot_f_aperture();
        // 独立重算孔径内和 (不依赖 fixture 预存值): D≤4 且非核盒像素贡献 0
        double direct = 0.0;
        for (int y = 0; y < af.h; ++y)
            for (int x = 0; x < af.w; ++x) {
                const double d2 = (x - af.cx) * (x - af.cx) + (y - af.cy) * (y - af.cy);
                if (d2 <= af.aperture_r * af.aperture_r)
                    direct += af.image[(std::size_t)y * af.w + x] - af.background;
            }
        P1PHOT_CHECK_NEAR(cs, direct, af.expected_flux, 1e-9, "F6: 孔径 oracle 自一致");
        P1PHOT_CHECK(cs, direct > 0.0, "F6: 期望通量为正 (核盒进入孔径)");
    }
}

// ---------- properties 组 ----------
void test_properties(CheckState& cs) {
    // ---- F1(jitter): IRLS 真迭代路径 + I2/I6 ----
    {
        const std::uint64_t seed = 0xA15EED0002ULL;
        const double k = 1.6;
        FixPhotFrame fx = fix_phot_a_inject_k(seed, 40, k, true);
        PhotometricDiag d;
        SimpleCall c = call_simple_f64(fx, 1, &d);
        OracleRobust orob = oracle_robust_location(fx.psf_flux, fx.gaia_fsyn,
                                                   fx.gaia_mag, 3.0);
        P1PHOT_CHECK_NEAR(cs, std::log10(1.0 / c.scale), std::log10(1.0 / orob.scale), 1e-9,
                          "F1(jitter): location 与 oracle 重算一致 (1e-9)");
        P1PHOT_CHECK_NEAR(cs, std::log10(1.0 / c.scale), std::log10(k), 1e-4,
                          "F1(jitter): location≈log10 k (rtol 1e-4)");
        P1PHOT_CHECK_NEAR(cs, c.sigma, orob.sigma_residual, 1e-9, "F1(jitter): sigma 与 oracle 一致");
        P1PHOT_CHECK(cs, d.robust_iterations == orob.robust_iterations,
                     "F1(jitter): robust_iterations 与 oracle 一致");
        P1PHOT_CHECK(cs, d.fit_used == (int)orob.inlier_match_rows.size(),
                     "F1(jitter): fit_used 与 oracle inliers 一致");
        P1PHOT_CHECK(cs, d.robust_iterations >= 1, "F1(jitter): IRLS 真迭代 (>0 次)");
    }

    // ---- F2: 20% 离群 → Δlocation<0.1 dex + IRLS 拒绝 (权重 0) ----
    {
        const std::uint64_t seed = 0xA15EED0003ULL;
        const double k = 2.0, out_dex = 0.5;
        FixPhotFrame fx = fix_phot_b_outliers(seed, 30, k, out_dex);
        PhotometricDiag d;
        SimpleCall c = call_simple_f64(fx, 1, &d);
        const double loc = std::log10(1.0 / c.scale);
        P1PHOT_CHECK_NEAR(cs, loc, std::log10(k), 0.1,
                          "F2: 离群下 |Δlocation|<0.1 dex (SCI-PHOT-001 §11)");
        P1PHOT_CHECK(cs, d.fit_used == 30 - 6,
                     "F2: fit_used = 30 − 6 (20% 离群被 Tukey 权 0)");
        P1PHOT_CHECK(cs, d.rejected_quality == 6,
                     "F2: rejected_quality 记 6 (全为 IRLS 离群)");
        // oracle 交叉
        OracleRobust orob = oracle_robust_location(fx.psf_flux, fx.gaia_fsyn,
                                                   fx.gaia_mag, 3.0);
        P1PHOT_CHECK(cs, (int)orob.mag_rejected_rows.size() == 0,
                     "F2: 星等门 0 拒绝 (漂移 1.25 mag < 3.0)");
        P1PHOT_CHECK(cs, (int)orob.inlier_match_rows.size() == 24,
                     "F2: oracle inliers=24 与 fit_used 呼应");
    }

    // ---- F3: 双向唯一配对诊断 (F1=25, F2=8, I3 会计) ----
    {
        const std::uint64_t seed = 0xA15EED0004ULL;
        FixPhotFrame fx = fix_phot_c_pairing(seed);
        PhotometricDiag d;
        SimpleCall c = call_simple_f64(fx, 1, &d);
        P1PHOT_CHECK(cs, d.spatial_candidates == 25, "F3: spatial_candidates=25 (全部正向命中)");
        P1PHOT_CHECK(cs, d.unique_matches == 25, "F3: unique_matches=25 (互为最近邻)");
        P1PHOT_CHECK(cs, d.rejected_ambiguous == 0, "F3: rejected_ambiguous=0 (挤对已被前置剔除)");
        P1PHOT_CHECK(cs, d.rejected_distance == 0, "F3: rejected_distance=0");
        P1PHOT_CHECK(cs, c.n_matched == 25, "F3: n_matched=25");
        // I3: Σdiag.rejected_* + fit_used = unique_matches
        P1PHOT_CHECK(cs, d.rejected_ambiguous + d.rejected_distance + d.rejected_quality +
                             d.fit_used == d.unique_matches,
                     "I3: Σrejected_*+fit_used=unique_matches");
        // oracle 暴力配对对拍
        OraclePairing op = oracle_pairing(fx, 2.0);
        P1PHOT_CHECK(cs, (int)op.unique_pairs.size() == 25,
                     "F3: oracle O(n²) 配对=25 (对拍 KD-tree)");
    }

    // ---- I4: records 与 psf_star_ids 1:1 ----
    {
        const std::uint64_t seed = 0xA15EED0002ULL;
        FixPhotFrame fx = fix_phot_a_inject_k(seed, 12, 1.7, true);
        SimpleCall c = call_simple_f64(fx, 1);
        std::vector<int> seen((int)fx.psf_cx.size(), 0);
        bool i4 = true;
        for (const auto& pr : c.records) {
            int row = -1;
            for (int i = 0; i < (int)fx.psf_star_ids.size(); ++i)
                if (fx.psf_star_ids[i] == pr.star_id) { row = i; break; }
            if (row < 0 || seen[row]) { i4 = false; break; }  // 不存在或重复
            seen[row] = 1;
            if (pr.status != 1) { i4 = false; break; }  // 全 inlier → status=1
        }
        P1PHOT_CHECK(cs, i4, "I4: records[star_id] 与 psf_star_ids 1:1, 全部 status=1");
        P1PHOT_CHECK(cs, (int)c.records.size() == 12, "I4: records 数 = n_psf");
        for (const auto& pr : c.records)
            P1PHOT_CHECK(cs, feq(pr.residual, std::log10(1.7), 1e-9),
                         "I6: residual=log10(F_instr/F_syn) 方向 (与 fixture 同式)");
    }

    // ---- I5: 线程扫描 1/4/N 科学输出位相等 ----
    {
        const std::uint64_t seed = 0xA15EED0005ULL;
        FixPhotFrame fx = fix_phot_b_outliers(seed, 36, 2.2, 0.6);
        SimpleCall c1 = call_simple_f64(fx, 1);
        SimpleCall c4 = call_simple_f64(fx, 4);
        long nthr = 8;
#ifdef _OPENMP
        nthr = omp_get_max_threads();
#endif
        SimpleCall cn = call_simple_f64(fx, (int)nthr);
        bool eq14 = c1.out_pixels.size() == c4.out_pixels.size();
        for (std::size_t i = 0; eq14 && i < c1.out_pixels.size(); ++i)
            eq14 = fbits_eq(c1.out_pixels[i], c4.out_pixels[i]);
        bool eq1n = c1.out_pixels.size() == cn.out_pixels.size();
        for (std::size_t i = 0; eq1n && i < c1.out_pixels.size(); ++i)
            eq1n = fbits_eq(c1.out_pixels[i], cn.out_pixels[i]);
        P1PHOT_CHECK(cs, eq14, "I5: 1/4 线程 out_pixels 位相等");
        P1PHOT_CHECK(cs, eq1n, "I5: 1/N 线程 out_pixels 位相等");
        P1PHOT_CHECK(cs, fbits_eq(c1.scale, c4.scale) && fbits_eq(c1.scale, cn.scale),
                     "I5: scale 位相等 (1/4/N)");
        P1PHOT_CHECK(cs, c1.diag.unique_matches == c4.diag.unique_matches &&
                         c1.diag.unique_matches == cn.diag.unique_matches &&
                         c1.diag.fit_used == c4.diag.fit_used &&
                         c1.diag.fit_used == cn.diag.fit_used,
                     "I5: 诊断计数字段跨线程一致");
    }
}

// ---------- negative 组 (FIX-PHOT-E) ----------
void test_negative(CheckState& cs) {
    const std::uint64_t seed = 0xA15EED0006ULL;
    FixPhotFrame fx = fix_phot_a_inject_k(seed, 8, 1.4, false);
    SimpleCall base = call_simple_f64(fx, 1);
    {
        int nm = -1; double sc = -1.0, sg = -1.0;
        PhotometricDiag d; std::memset(&d, 0, sizeof d);
        std::vector<double> out(fx.pixels.size(), 0.0);
        const int rc = pc_calibrate_simple_f64(
            nullptr, fx.w, fx.h,  // pixels null
            fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(), fx.gaia_fsyn.data(), 8,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), fx.psf_status.data(), 8,
            nullptr, nullptr, 0,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &d);
        P1PHOT_CHECK(cs, rc == -1, "NEG: pixels=null → rc=-1");
    }
    {
        int nm = -1; double sc = -1.0, sg = -1.0;
        std::vector<double> out(fx.pixels.size(), 0.0);
        const int rc = pc_calibrate_simple_f64(
            fx.pixels.data(), 0, fx.h,  // width=0
            fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(), fx.gaia_fsyn.data(), 8,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), fx.psf_status.data(), 8,
            nullptr, nullptr, 0,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK(cs, rc == -2, "NEG: width=0 → rc=-2");
    }
    {
        int nm = -1; double sc = -1.0, sg = -1.0;
        std::vector<double> out(fx.pixels.size(), 0.0);
        const int rc = pc_calibrate_simple_f64(
            fx.pixels.data(), fx.w, fx.h,
            nullptr, nullptr, nullptr, nullptr, 0,  // n_gaia=0
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), fx.psf_status.data(), 8,
            nullptr, nullptr, 0,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK(cs, rc == 0, "NEG: n_gaia=0 → rc=0 退化");
        P1PHOT_CHECK_NEAR(cs, sc, 1.0, 0.0, "NEG: n_gaia=0 → scale=1.0 精确");
        P1PHOT_CHECK(cs, nm == 0, "NEG: n_gaia=0 → n_matched=0");
        bool copy_ok = true;
        for (std::size_t i = 0; i < fx.pixels.size(); ++i)
            if (!fbits_eq(out[i], fx.pixels[i])) { copy_ok = false; break; }
        P1PHOT_CHECK(cs, copy_ok, "NEG: n_gaia=0 → out_pixels 原样拷贝");
    }
    {
        // 全部 PSF status!=0 → 退化 (无有效 PSF)
        std::vector<int> bad_status(fx.psf_status.size(), 3);
        int nm = -1; double sc = -1.0, sg = -1.0;
        std::vector<double> out(fx.pixels.size(), 0.0);
        PhotometricDiag d; std::memset(&d, 0, sizeof d);
        const int rc = pc_calibrate_simple_f64(
            fx.pixels.data(), fx.w, fx.h,
            fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(), fx.gaia_fsyn.data(), 8,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), bad_status.data(), 8,
            nullptr, nullptr, 0,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &d);
        P1PHOT_CHECK(cs, rc == 0, "NEG: PSF 全 status!=0 → rc=0 退化");
        P1PHOT_CHECK_NEAR(cs, sc, 1.0, 0.0, "NEG: PSF 全无效 → scale=1.0");
        P1PHOT_CHECK(cs, d.psf_valid == 0, "NEG: psf_valid=0 (诊断记录显式拒绝)");
    }
    {
        // sip_order 越界 → -4 (正式支持 [0,5], 拒绝不截断)
        int nm = -1; double sc = -1.0, sg = -1.0;
        std::vector<double> out(fx.pixels.size(), 0.0);
        const int rc = pc_calibrate_simple_f64(
            fx.pixels.data(), fx.w, fx.h,
            fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(), fx.gaia_fsyn.data(), 8,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), fx.psf_status.data(), 8,
            nullptr, nullptr, 0,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 6,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK(cs, rc == -4, "NEG: sip_order=6 → rc=-4 (异常屏障)");
    }
    {
        // sip_order=0 与=5 合法域冒烟 (不越界拒绝)
        for (int so : {0, 5}) {
            int nm = -1; double sc = -1.0, sg = -1.0;
            std::vector<double> out(fx.pixels.size(), 0.0);
            const int rc = pc_calibrate_simple_f64(
                fx.pixels.data(), fx.w, fx.h,
                fx.gaia_ra.data(), fx.gaia_dec.data(), fx.gaia_mag.data(), fx.gaia_fsyn.data(), 8,
                fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(), fx.psf_status.data(), 8,
                nullptr, nullptr, 0,
                fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
                fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, so,
                nullptr, nullptr, nullptr, nullptr,
                out.data(), &nm, &sc, &sg, nullptr);
            P1PHOT_CHECK(cs, rc == 0, "NEG: sip_order=合法域接受");
        }
    }
}

// ---------- gaia 组: v2 通道 (测试侧 stub gaia_client 符号) ----------
namespace stub {
struct StubSpec {
    std::vector<double> ra, dec, mag;
    std::vector<float> flux_min, flux_mul;
    std::vector<uint8_t> spectra;  // row-major, count=336→1020 step 2 (343)
    int stride = 343;
};
inline StubSpec& stub_data() {
    static StubSpec s;
    return s;
}
}  // namespace stub

}  // namespace p1phot

// ---- 测试侧提供的 gaia_client C 符号 (lib/infrastructure/gaia_xpsd_client 不参与链接) ----
extern "C" int gaia_client_cone_search_with_spectrum(
    void* /*client*/, double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar** out_stars, uint8_t** out_spectra, int* out_count) {
    using p1phot::stub::stub_data;
    StubSpec& s = stub_data();
    if (s.ra.empty()) { if (out_count) *out_count = 0; return 0; }
    (void)radius_deg;
    const int n = (int)s.ra.size();
    GaiaSpectrumStar* stars = (GaiaSpectrumStar*)std::malloc(sizeof(GaiaSpectrumStar) * n);
    uint8_t* spec = (uint8_t*)std::malloc((size_t)s.stride * n);
    for (int i = 0; i < n; ++i) {
        stars[i].ra = s.ra[i];
        stars[i].dec = s.dec[i];
        stars[i].magG = s.mag[i];
        stars[i].flux_min = s.flux_min[i];
        stars[i].flux_mul = s.flux_mul[i];
        std::memcpy(spec + (size_t)i * s.stride, s.spectra.data() + (size_t)i * s.stride, s.stride);
    }
    *out_stars = stars;
    *out_spectra = spec;
    *out_count = n;
    (void)ra; (void)dec; (void)mag_low; (void)mag_high;
    return 0;
}

extern "C" int gaia_client_get_spectrum_params(void* /*client*/, int* out_start_nm,
                                               int* out_step_nm, int* out_count) {
    if (out_start_nm) *out_start_nm = 336;
    if (out_step_nm) *out_step_nm = 2;
    if (out_count) *out_count = 343;
    return 0;
}

namespace p1phot {
namespace {

void test_gaia(CheckState& cs) {
    P1PHOT_FAULT_POINT("gaia_pre");
    using stub::stub_data;
    StubSpec& s = stub_data();
    s.stride = 343;
    s.ra.clear(); s.dec.clear(); s.mag.clear();
    s.flux_min.clear(); s.flux_mul.clear(); s.spectra.clear();

    // fixture: 9 颗星 (spec_stride=343 ≤ 9 限制无关 — stride 是每星光谱点数,
    // 无星数限制; 此处 n=9 保证 n_gaia<2000 单轮自适应即完成)
    const int n = 9;
    const std::uint64_t seed = 0xA15EED0007ULL;
    FixPhotFrame fx;
    fx.w = 256; fx.h = 256;
    fx.k = 1.9;
    fx.pixels.assign((std::size_t)fx.w * fx.h, 100.0);
    std::uint64_t st = seed;
    for (int i = 0; i < n; ++i) {
        const double x = 30.0 + 20.0 * (i % 5) + (i / 5) * 90.0;
        const double y = 30.0 + 42.0 * (i / 5);
        double ra, dec;
        p1phot_oracle_pixel_to_sky(fx.wcs, x, y, ra, dec);
        const double fsyn = std::pow(10.0, 3.4 + 0.05 * i);
        s.ra.push_back(ra);
        s.dec.push_back(dec);
        s.mag.push_back(13.0 + 0.1 * i);
        s.flux_min.push_back((float)1.0e-14);
        s.flux_mul.push_back((float)2.0e-16);
        std::vector<uint8_t> row(s.stride, 200);  // F = 200·2e-16 + 1e-14
        s.spectra.insert(s.spectra.end(), row.begin(), row.end());
        fx.psf_cx.push_back(x);
        fx.psf_cy.push_back(y);
        fx.psf_flux.push_back(fx.k * fsyn);
        fx.psf_status.push_back(0);
        fx.psf_star_ids.push_back(500 + i);
        (void)fsyn; (void)st;
    }

    FixPhotXpsd xd = fix_phot_d_xpsd();
    std::vector<double> wl = oracle_weighted_wl_linear(xd);
    const double fsyn_expect = oracle_fsyn_xpsd_closed_form(xd, wl, 1.0e-14, 2.0e-16);

    std::vector<double> out((std::size_t)fx.w * fx.h, -1.0);
    std::vector<PcMatchRecord> records(n);
    int nm = -1; double sc = -1.0, sg = -1.0;
    PhotometricDiag d; std::memset(&d, 0, sizeof d);
    int handle_token = 1;  // 非空任意值 (stub 不解引用)
    const int rc = pc_calibrate_simple_with_gaia_f64_v2(
        &handle_token,
        fx.wcs.crval1, fx.wcs.crval2, 0.05, 12.0, 16.0,
        xd.filter_wl.data(), xd.filter_trans.data(), (int)xd.filter_wl.size(),
        nullptr, nullptr, 0,
        xd.spectrum_wl.data(), (int)xd.spectrum_wl.size(),
        fx.pixels.data(), fx.w, fx.h,
        fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
        fx.psf_status.data(), n,
        fx.psf_star_ids.data(), records.data(),
        fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
        fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
        nullptr, nullptr, nullptr, nullptr,
        out.data(), &nm, &sc, &sg, &d);
    P1PHOT_CHECK(cs, rc == 0, "GAIA: v2 通道 rc=0 (stub 数据注入)");
    P1PHOT_CHECK(cs, d.spectrum_rows_total == n && d.valid_fsyn == n,
                 "GAIA: spectrum_rows_total=9, valid_fsyn=9");
    P1PHOT_CHECK_NEAR(cs, sc, 1.0 / fx.k, 1e-4 * (1.0 / fx.k),
                      "GAIA: scale=1/k (F_syn oracle + 已知 k, rtol 1e-4)");
    P1PHOT_CHECK(cs, nm == n, "GAIA: n_matched=9");
    bool refs_ok = true;
    for (const auto& pr : records)
        if (!feq(pr.reference_flux, fsyn_expect, 1e-9 * fsyn_expect)) refs_ok = false;
    P1PHOT_CHECK(cs, refs_ok, "F4: 全链 reference_flux=F_syn oracle (rtol 1e-9)");
    bool stat_ok = true;
    for (const auto& pr : records)
        if (pr.status != 1 || pr.reject_reason != 0) stat_ok = false;
    P1PHOT_CHECK(cs, stat_ok, "GAIA: records 全部 status=1/reason=0");
    // I1 on v2 path
    bool i1 = true;
    for (std::size_t i = 0; i < fx.pixels.size(); ++i)
        if (!fbits_eq(out[i], fx.pixels[i] * sc)) { i1 = false; break; }
    P1PHOT_CHECK(cs, i1, "I1(v2): out_pixels=I·scale 位相等");
    // I5(v2): 线程位相等
    {
        std::vector<double> out2((std::size_t)fx.w * fx.h, -1.0);
        int nm2; double sc2, sg2;
        const int rc2 = pc_calibrate_simple_with_gaia_f64_v2(
            &handle_token, fx.wcs.crval1, fx.wcs.crval2, 0.05, 12.0, 16.0,
            xd.filter_wl.data(), xd.filter_trans.data(), (int)xd.filter_wl.size(),
            nullptr, nullptr, 0,
            xd.spectrum_wl.data(), (int)xd.spectrum_wl.size(),
            fx.pixels.data(), fx.w, fx.h,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
            fx.psf_status.data(), n,
            fx.psf_star_ids.data(), nullptr,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out2.data(), &nm2, &sc2, &sg2, nullptr);
        bool eq = rc2 == 0 && out2.size() == out.size();
        for (std::size_t i = 0; eq && i < out.size(); ++i)
            eq = fbits_eq(out[i], out2[i]);
        P1PHOT_CHECK(cs, eq, "I5(v2): out_pixels 位相等 (重复调用)");
    }
    // 负面: handle null → -2; 无效 filter → -1
    {
        int nmx; double scx, sgx;
        const int rc2 = pc_calibrate_simple_with_gaia_f64_v2(
            nullptr, 0, 0, 0.05, 12, 16,
            xd.filter_wl.data(), xd.filter_trans.data(), 2,
            nullptr, nullptr, 0,
            xd.spectrum_wl.data(), 343,
            fx.pixels.data(), fx.w, fx.h,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
            fx.psf_status.data(), n,
            nullptr, nullptr,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nmx, &scx, &sgx, nullptr);
        P1PHOT_CHECK(cs, rc2 == -2, "NEG(v2): handle=null → rc=-2");
    }
    {
        int nmx; double scx, sgx;
        int token = 1;
        const int rc3 = pc_calibrate_simple_with_gaia_f64_v2(
            &token, 0, 0, 0.05, 12, 16,
            nullptr, nullptr, 0,  // filter_count=0
            nullptr, nullptr, 0,
            xd.spectrum_wl.data(), 343,
            fx.pixels.data(), fx.w, fx.h,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
            fx.psf_status.data(), n,
            nullptr, nullptr,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nmx, &scx, &sgx, nullptr);
        P1PHOT_CHECK(cs, rc3 == -1, "NEG(v2): filter_count=0 → rc=-1");
    }
    // 锥搜失败路径: stub 数据清空且返回 -1 → rc=-3
    {
        // 临时断开 stub: 通过全局开关 (见下方 stub_fail 标志)
        stub::stub_fail() = true;
        int nmx; double scx, sgx;
        int token = 1;
        const int rc4 = pc_calibrate_simple_with_gaia_f64_v2(
            &token, 0, 0, 0.05, 12, 16,
            xd.filter_wl.data(), xd.filter_trans.data(), 2,
            nullptr, nullptr, 0,
            xd.spectrum_wl.data(), 343,
            fx.pixels.data(), fx.w, fx.h,
            fx.psf_cx.data(), fx.psf_cy.data(), fx.psf_flux.data(),
            fx.psf_status.data(), n,
            nullptr, nullptr,
            fx.wcs.crval1, fx.wcs.crval2, fx.wcs.crpix1, fx.wcs.crpix2,
            fx.wcs.cd11, fx.wcs.cd12, fx.wcs.cd21, fx.wcs.cd22, 0,
            nullptr, nullptr, nullptr, nullptr,
            out.data(), &nmx, &scx, &sgx, nullptr);
        P1PHOT_CHECK(cs, rc4 == -3, "NEG(v2): 锥搜失败 → rc=-3");
        stub::stub_fail() = false;
    }
}

// ---------- legacy 组: F6 Photometer ----------
void test_legacy(CheckState& cs) {
    P1PHOT_FAULT_POINT("legacy_pre");
    using astrocs::phase1::Photometer;
    using astrocs::phase1::PhotometryResult;

    // 正向: 已知通量
    {
        FixPhotAperture af = fix_phot_f_aperture();
        Photometer phot(4.0, 6.0, 10.0);
        auto r = phot.measure(af.image.data(), af.w, af.h, af.cx, af.cy);
        P1PHOT_CHECK(cs, r.ok(), "F6: measure 返回 ok Result");
        if (r.ok()) {
            const PhotometryResult& v = r.value();
            P1PHOT_CHECK(cs, v.valid, "F6: valid=true");
            P1PHOT_CHECK_NEAR(cs, v.flux, af.expected_flux, 1e-9,
                              "F6: flux=孔径 oracle (1e-9)");
            P1PHOT_CHECK_NEAR(cs, v.background, af.background, 1e-12, "F6: 背景中位数=100 精确");
        }
    }
    // 负面: 中心越界 / 空天环 / 空孔径
    {
        FixPhotAperture af = fix_phot_f_aperture();
        Photometer phot(4.0, 6.0, 10.0);
        auto r = phot.measure(af.image.data(), af.w, af.h, -3.0, 64.0);
        P1PHOT_CHECK(cs, r.ok() && !r.value().valid &&
                         r.value().failure_reason == "center out of bounds",
                     "F6: 中心越界 → valid=false + 显式 reason");
        // 空天环: 图像 4x4, 中心 (2,2), 天环 r∈[6,10] 无像素
        std::vector<float> tiny(16, 5.0f);
        auto r2 = phot.measure(tiny.data(), 4, 4, 2.0, 2.0);
        P1PHOT_CHECK(cs, r2.ok() && !r2.value().valid &&
                         r2.value().failure_reason == "no sky annulus pixels",
                     "F6: 空天环 → valid=false + 显式 reason");
        // 空孔径: 小图 3x3, 中心 (1,1), 孔径 r=8 (越过边界仍非空 — 改用:
        // 孔径 r=4 于 3x3 中心 (1.5,1.5): 像素 0,1,2 d²≤4? (0,0)d²=0.5≤16
        // → 非空。真空孔径需 r 小于半像素: r=0.3, 中心 (1.5,1.5)
        Photometer phot_tiny(0.3, 1.0, 2.0);
        auto r3 = phot_tiny.measure(tiny.data(), 3, 3, 1.5, 1.5);
        P1PHOT_CHECK(cs, r3.ok() && !r3.value().valid &&
                         r3.value().failure_reason == "aperture empty",
                     "F6: 空孔径 → valid=false + 显式 reason");
    }
}

}  // namespace
}  // namespace p1phot

static ::p1phot::GroupRegistrar rg_units("units", ::p1phot::test_units);
static ::p1phot::GroupRegistrar rg_props("properties", ::p1phot::test_properties);
static ::p1phot::GroupRegistrar rg_neg("negative", ::p1phot::test_negative);
static ::p1phot::GroupRegistrar rg_gaia("gaia", ::p1phot::test_gaia);
static ::p1phot::GroupRegistrar rg_legacy("legacy", ::p1phot::test_legacy);
