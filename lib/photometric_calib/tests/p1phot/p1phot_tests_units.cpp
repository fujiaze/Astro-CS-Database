// P1-PHOT-TEST · units 组 (正常路径 + 解析期望 + F6 aperture 重锚)
//
// 覆盖 (TEST-PHOT-DESIGN-001 / SCI-PHOT-001 §11 / ALG-PHOT-001..002):
//   U1  F1 合成注入 k 场: location≈log10 k (rtol 1e-4 冻结门) + S=0 直
//       median (robust_iterations=0) + I1 f64 像素校正 bitwise (ALG-PHOT-001)
//   U2  I6 方向: k>1 → location>0/scale<1; k<1 → 反之 (r=log10(F_instr/F_syn))
//   U3  直通 f32 通道: 像素校正 f32 rtol 1e-7 (float 存储舍入), scale/sigma
//       与 f64 通道一致 (匹配/IRLS 全 double)
//   U4  F3 双向唯一配对: spatial_candidates/unique/rejected_ambiguous/
//       rejected_distance 分阶段 diag + per-star records (status/reject_reason)
//       + 诊断计数闭合恒等式 (I3 两层口径, 见组内注释)
//   U5  F5 退化 (直通): n_gaia=0 / n_psf=0 → rc=0, scale=1.0, 恒等校正
//       bitwise (README §6 退化显式登记语义)
//   U6  F4 XPSD 闭式 + oracle 端到端: 常数谱+常数 T 闭式 (Simpson 对 ≤3 次
//       多项式精确) rel 1e-12; 随机谱 oracle 复算 rtol 1e-9 (冻结容差);
//       mag 归一化通道 compute_f_syn 同口径
//   U7  TAN+SIP WCS: oracle 对拍 rel 1e-9 + round-trip 门
//   U8  SIP order 越界: WcsTransform 抛 std::invalid_argument; C API -4
//   U9  F6 aperture 重锚 (p1_wcs_phot_test 4 组): 已知通量/越界/空环/空孔径
//       显式失败 + Result::fail 通道
//   U10 F4 v2 全链: records.reference_flux = XPSD 积分 vs oracle rtol 1e-9
//       + dr3sp_id 跨调用确定性
#include "p1phot_test_main.hpp"
#include "p1phot_field_stub.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"
#include "p1phot_oracle.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "photometric_calib.h"      // API-PHOT-001 C ABI (被测公共合同头)
#include "photometer.h"             // F6 legacy 旧符号 (README §9, 只读引用)
#include "wcs_transform.h"          // 被测域内部符号 (冻结旧实现直调, 模板 §5)
#include "spectrum_integrator.h"    // 被测域内部符号 (U6 直调积分器)

namespace p1phot {
using fix::SplitMix64;  // fixture RNG (p1phot::fix)

namespace {

CheckState g_cs;

// 常数谱 v2 场装配: 星表 ra/dec 来自 StarField, 常数 uint8 谱 + 固定量化参数
stub::GaiaClient* make_const_client(const fix::StarField& f) {
    stub::FakeClientConfig cfg =
        bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
    return stub::create(cfg);
}

// 两点常数滤光片 [300,1050] T=0.5 (Akima n=2 退化 → 网格内精确常数)
void const_filter(std::vector<double>& wl, std::vector<double>& tr) {
    wl = {300.0, 1050.0};
    tr = {0.5, 0.5};
}

// 343 点光谱波长网格 [336,1020] step 2
std::vector<double> spec_grid() {
    std::vector<double> wl;
    for (int x = 336; x <= 1020; x += 2) wl.push_back((double)x);
    return wl;
}

}  // namespace

int test_units() {
    CheckState& cs = g_cs;
    fix::FrameGeom g;

    // ── U1/U2: F1 合成注入 k 场 (直通 f64) ──────────────────────────────
    const double ks[] = {0.5, 1.0, 2.0};
    for (double k : ks) {
        fix::StarField f = fix::fixture_f1(g, k, 12);
        const int w = g.width, h = g.height;
        std::vector<double> pixels((std::size_t)w * h);
        {
            SplitMix64 rng(0x5EED00000000BEEFULL);
            for (auto& v : pixels) v = rng.uniform(50.0, 5000.0);
        }
        std::vector<double> out((std::size_t)w * h, 0.0);
        int n_matched = -1;
        double scale = 0.0, sigma = -1.0;
        PhotometricDiag diag;
        std::memset(&diag, 0, sizeof(diag));
        const int rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(),
            nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, &diag);
        P1PHOT_CHECK_MSG(cs, rc == 0, "u1_scale_injection", "rc=%d k=%g", rc, k);

        // 冻结门 (SCI-PHOT-001 §11): location≈log10 k, rtol 1e-4
        const double location = -std::log10(scale);
        const double want_loc = std::log10(k);
        const bool loc_ok = (want_loc == 0.0)
                                ? (std::fabs(location) < 1e-12)
                                : std::fabs(location - want_loc) <= 1e-4 * std::fabs(want_loc);
        P1PHOT_CHECK_MSG(cs, loc_ok, "u1_scale_injection",
                         "k=%g location=%.12f want=%.12f", k, location, want_loc);
        const double want_scale = std::pow(10.0, -want_loc);
        P1PHOT_CHECK_MSG(cs, rel_close_d(scale, want_scale, 1e-9), "u1_scale_injection",
                         "k=%g scale=%.15g want=%.15g", k, scale, want_scale);

        // S=0 直 median (r≡log10 k): 不迭代 (SCI §11 S=0 门同型语义)
        P1PHOT_CHECK_MSG(cs, diag.robust_iterations == 0, "u1_s0_gate",
                         "k=%g iters=%d", k, diag.robust_iterations);
        P1PHOT_CHECK(cs, diag.fit_used == (int)f.psf_cx.size(), "u1_fit_used");
        P1PHOT_CHECK(cs, n_matched == (int)f.psf_cx.size(), "u1_n_matched");
        P1PHOT_CHECK(cs, sigma == 0.0, "u1_sigma_s0");

        // U2 (I6 方向)
        if (k > 1.0) {
            P1PHOT_CHECK(cs, location > 0.0 && scale < 1.0, "u2_direction");
        } else if (k < 1.0) {
            P1PHOT_CHECK(cs, location < 0.0 && scale > 1.0, "u2_direction");
        } else {
            P1PHOT_CHECK(cs, scale == 1.0, "u2_direction");
        }

        // I1 f64 像素校正 bitwise: out = in·scale (逐元素 IEEE 乘法确定);
        // NaN 像素单独断言 (NaN payload 传播不保证 bitwise)
        bool bitwise_ok = true, nan_ok = true;
        for (int i = 0; i < w * h; ++i) {
            if (std::isnan(pixels[(std::size_t)i])) {
                if (!std::isnan(out[(std::size_t)i])) nan_ok = false;
            } else if (!bits_eq_d(out[(std::size_t)i],
                                  pixels[(std::size_t)i] * scale)) {
                bitwise_ok = false;
            }
        }
        P1PHOT_CHECK(cs, bitwise_ok, "u1_identity_bitwise");
        P1PHOT_CHECK(cs, nan_ok, "u1_identity_bitwise");
    }

    // ── U3: 直通 f32 通道 ────────────────────────────────────────────────
    {
        const double k = 1.6;
        fix::StarField f = fix::fixture_f1(g, k, 12);
        const int w = g.width, h = g.height;
        std::vector<float> pixels32((std::size_t)w * h);
        std::vector<double> pixels64((std::size_t)w * h);
        {
            SplitMix64 rng(0x5EED00000000F32FULL);
            for (int i = 0; i < w * h; ++i) {
                const double v = rng.uniform(50.0, 5000.0);
                pixels32[(std::size_t)i] = (float)v;
                pixels64[(std::size_t)i] = (double)pixels32[(std::size_t)i];
            }
        }
        std::vector<float> out32((std::size_t)w * h, 0.0f);
        std::vector<double> out64((std::size_t)w * h, 0.0);
        int n32 = -1, n64 = -1;
        double s32 = 0, s64 = 0, sg32 = -1, sg64 = -1;
        PhotometricDiag d32, d64;
        std::memset(&d32, 0, sizeof(d32));
        std::memset(&d64, 0, sizeof(d64));
        const int rc32 = pc_calibrate_simple(
            pixels32.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out32.data(), &n32, &s32, &sg32, &d32);
        const int rc64 = pc_calibrate_simple_f64(
            pixels64.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out64.data(), &n64, &s64, &sg64, &d64);
        P1PHOT_CHECK(cs, rc32 == 0 && rc64 == 0, "u3_f32_channel");
        // 匹配/IRLS 全 double → scale/sigma 双通道一致
        P1PHOT_CHECK(cs, bits_eq_d(s32, s64), "u3_f32_channel");
        P1PHOT_CHECK(cs, bits_eq_d(sg32, sg64), "u3_f32_channel");
        P1PHOT_CHECK(cs, n32 == n64 && n32 == (int)f.psf_cx.size(), "u3_f32_channel");
        // 冻结门同 U1: location≈log10 1.6 (rtol 1e-4)
        const double loc32 = -std::log10(s32);
        P1PHOT_CHECK(cs, std::fabs(loc32 - std::log10(k)) <= 1e-4 * std::fabs(std::log10(k)),
                     "u3_f32_channel");
        // f32 像素校正 rtol 1e-7 (float 存储舍入界)
        bool f32_ok = true;
        for (int i = 0; i < w * h; ++i)
            if (!rel_close_f(out32[(std::size_t)i],
                             (double)pixels32[(std::size_t)i] * s32, 1e-7))
                f32_ok = false;
        P1PHOT_CHECK(cs, f32_ok, "u3_f32_rounding");
    }

    // ── U4: F3 双向唯一配对 (v2 桩通道) ─────────────────────────────────
    {
        fix::StarField f = fix::fixture_f3(g);
        stub::GaiaClient* client = make_const_client(f);
        const int w = g.width, h = g.height;
        std::vector<double> pixels((std::size_t)w * h, 100.0);
        std::vector<double> out((std::size_t)w * h, 0.0);
        int n_matched = -1;
        double scale = 0.0, sigma = 0.0;
        PhotometricDiag diag;
        std::memset(&diag, 0, sizeof(diag));
        std::vector<PcMatchRecord> records(f.psf_cx.size());
        std::vector<double> fw, ft;
        const_filter(fw, ft);
        std::vector<double> swl = spec_grid();
        const int rc = pc_calibrate_simple_with_gaia_f64_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), f.psf_star_ids.data(), records.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, &diag);
        P1PHOT_CHECK(cs, rc == 0, "u4_match_phases");
        // 分阶段 diag (fixture_f3 设计: A/B→g0 A 唯一; C↔g1; F↔g2; E 超距;
        // psf[5] status=1 无效; B 歧义)
        P1PHOT_CHECK_MSG(cs, diag.psf_total == 6, "u4_match_phases", "psf_total=%d", diag.psf_total);
        P1PHOT_CHECK_MSG(cs, diag.psf_valid == 5, "u4_match_phases", "psf_valid=%d", diag.psf_valid);
        P1PHOT_CHECK_MSG(cs, diag.spatial_candidates == 4, "u4_match_phases",
                         "cand=%d", diag.spatial_candidates);
        P1PHOT_CHECK_MSG(cs, diag.unique_matches == 3, "u4_match_phases",
                         "uniq=%d", diag.unique_matches);
        P1PHOT_CHECK_MSG(cs, diag.rejected_ambiguous == 1, "u4_match_phases",
                         "amb=%d", diag.rejected_ambiguous);
        P1PHOT_CHECK_MSG(cs, diag.rejected_distance == 1, "u4_match_phases",
                         "dist=%d", diag.rejected_distance);
        P1PHOT_CHECK_MSG(cs, diag.fit_used == 3, "u4_match_phases", "fit=%d", diag.fit_used);
        // I3 诊断计数闭合 (两层口径, 与 ALG §13.1 实现锚一致):
        //   清洗层: fit_used + rejected_quality == unique_matches
        //   匹配层: unique_matches + rejected_ambiguous + rejected_distance == psf_valid
        P1PHOT_CHECK_MSG(cs, diag.fit_used + diag.rejected_quality == diag.unique_matches,
                         "u4_diag_closure", "fit=%d rq=%d uniq=%d",
                         diag.fit_used, diag.rejected_quality, diag.unique_matches);
        P1PHOT_CHECK_MSG(cs, diag.unique_matches + diag.rejected_ambiguous +
                                     diag.rejected_distance == diag.psf_valid,
                         "u4_diag_closure", "uniq=%d amb=%d dist=%d valid=%d",
                         diag.unique_matches, diag.rejected_ambiguous,
                         diag.rejected_distance, diag.psf_valid);
        // oracle 暴力配对对拍 (KD-tree 独立参照, ALG-PHOT-002)
        {
            oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                                  g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
            std::vector<double> gx(f.gaia_ra.size()), gy(f.gaia_ra.size());
            for (std::size_t i = 0; i < f.gaia_ra.size(); ++i)
                wcs.sky_to_pixel(f.gaia_ra[i], f.gaia_dec[i], gx[i], gy[i]);
            std::vector<double> px, py;
            std::vector<int> pidx;
            for (std::size_t i = 0; i < f.psf_cx.size(); ++i)
                if (f.psf_status[i] == 0) {
                    px.push_back(f.psf_cx[i]);
                    py.push_back(f.psf_cy[i]);
                    pidx.push_back((int)i);
                }
            const auto om = oracle::oracle_bruteforce_match(gx, gy, px, py, 2.0);
            P1PHOT_CHECK_MSG(cs, (int)om.size() == diag.unique_matches,
                             "u4_oracle_match", "oracle=%zu uniq=%d", om.size(),
                             diag.unique_matches);
            // 配对逐星一致: oracle (psf_row,gaia_idx) ↔ records status=1 星
            for (const auto& m : om) {
                const int row = pidx[(std::size_t)m.psf_idx];
                const PcMatchRecord& rec = records[(std::size_t)row];
                P1PHOT_CHECK_MSG(cs, rec.star_id == f.psf_star_ids[(std::size_t)row] &&
                                         rec.status == 1 && rec.reject_reason == 0,
                                 "u4_oracle_match", "row=%d status=%d", row, rec.status);
            }
        }
        // per-star records (I4 lineage): star_id 原样回传 + 显式三态
        P1PHOT_CHECK(cs, records[0].star_id == 2001 && records[0].status == 1 &&
                             records[0].reject_reason == 0, "u4_records");
        P1PHOT_CHECK(cs, records[1].star_id == 2002 && records[1].status == 0 &&
                             records[1].reject_reason == 4,   // 歧义 → no spatial match
                     "u4_records");
        P1PHOT_CHECK(cs, records[2].star_id == 2003 && records[2].status == 1,
                     "u4_records");
        P1PHOT_CHECK(cs, records[3].star_id == 2004 && records[3].status == 0 &&
                             records[3].reject_reason == 4,   // 超距
                     "u4_records");
        P1PHOT_CHECK(cs, records[4].star_id == 2005 && records[4].status == 1,
                     "u4_records");
        P1PHOT_CHECK(cs, records[5].star_id == 2006 && records[5].status == 3 &&
                             records[5].reject_reason == 6,   // psf-invalid
                     "u4_records");
        // 残差记录 (I6): status=1 行 residual=log10(F_instr/F_syn) 有限
        bool res_ok = true;
        for (const auto& rec : records)
            if (rec.status == 1 && !std::isfinite(rec.residual)) res_ok = false;
        P1PHOT_CHECK(cs, res_ok, "u4_records");
        stub::destroy(client);
    }

    // ── U5: F5 退化 (直通) ───────────────────────────────────────────────
    {
        const int w = g.width, h = g.height;
        std::vector<double> pixels((std::size_t)w * h);
        SplitMix64 rng(0x5EED00000000DE5AULL);
        for (auto& v : pixels) v = rng.uniform(1.0, 100.0);
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        std::vector<double> out((std::size_t)w * h, 0.0);
        int n_matched = -1;
        double scale = 0.0, sigma = -1.0;
        // n_gaia=0 → rc=0 退化恒等
        int rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            nullptr, nullptr, nullptr, nullptr, 0,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, nullptr);
        P1PHOT_CHECK(cs, rc == 0, "u5_degenerate");
        P1PHOT_CHECK(cs, scale == 1.0 && n_matched == 0 && sigma == 0.0, "u5_degenerate");
        bool ident = true;
        for (int i = 0; i < w * h; ++i)
            if (!bits_eq_d(out[(std::size_t)i], pixels[(std::size_t)i])) ident = false;
        P1PHOT_CHECK(cs, ident, "u5_identity_bitwise");
        // n_psf=0 → rc=0 退化恒等
        std::fill(out.begin(), out.end(), 0.0);
        rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, nullptr);
        P1PHOT_CHECK(cs, rc == 0, "u5_degenerate");
        P1PHOT_CHECK(cs, scale == 1.0 && n_matched == 0, "u5_degenerate");
        ident = true;
        for (int i = 0; i < w * h; ++i)
            if (!bits_eq_d(out[(std::size_t)i], pixels[(std::size_t)i])) ident = false;
        P1PHOT_CHECK(cs, ident, "u5_identity_bitwise");
    }

    // ── U6: F4 XPSD 闭式 + oracle 端到端 ────────────────────────────────
    {
        std::vector<double> swl = spec_grid();
        std::vector<double> fw, ft;
        const_filter(fw, ft);
        // 常数谱闭式: F_syn = f·T0·(λmax²−λmin²)/2, f=200·mul+min
        {
            const std::vector<uint8_t> spec(swl.size(), 200);
            const auto cache = photo_calib::prepare_filter_cache(
                fw.data(), ft.data(), (int)fw.size(),
                nullptr, nullptr, 0, swl.data(), (int)swl.size());
            P1PHOT_CHECK(cs, !cache.spectrum_wl.empty(), "u6_fsyn_closed");
            const double got = photo_calib::compute_f_syn_cached_xpsd(
                cache, spec.data(), (int)spec.size(), 1.0e-15f, 5.0e-18f);
            // 量化参数以 float 存储 → 闭式从 (double)(float) 值构造
            const double f0 = 200.0 * (double)5.0e-18f + (double)1.0e-15f;
            const double want = f0 * 0.5 * (1020.0 * 1020.0 - 336.0 * 336.0) / 2.0;
            P1PHOT_CHECK_MSG(cs, rel_close_d(got, want, 1e-12), "u6_fsyn_closed",
                             "got=%.15g want=%.15g", got, want);
        }
        // 随机谱 oracle 端到端 (F4 冻结容差 rtol 1e-9)
        {
            SplitMix64 rng(0x5EED00000000F0B4ULL);
            std::vector<double> spec_wl;
            std::vector<std::vector<uint8_t>> spectra;
            std::vector<float> fmin, fmul;
            std::vector<double> qw, qt;
            fix::fixture_f4_spectra(rng, spec_wl, spectra, fmin, fmul, fw, ft, qw, qt);
            // fixture_f4 也生成了滤光片/QE 曲线 → 覆盖 fw/ft (高斯+QE)
            const auto cache = photo_calib::prepare_filter_cache(
                fw.data(), ft.data(), (int)fw.size(),
                qw.data(), qt.data(), (int)qw.size(),
                spec_wl.data(), (int)spec_wl.size());
            for (std::size_t s = 0; s < spectra.size(); ++s) {
                const double got = photo_calib::compute_f_syn_cached_xpsd(
                    cache, spectra[s].data(), (int)spectra[s].size(),
                    fmin[s], fmul[s]);
                const double want = oracle::oracle_f_syn_xpsd(
                    spec_wl, spectra[s], fw, ft, qw, qt, fmin[s], fmul[s]);
                P1PHOT_CHECK_MSG(cs, rel_close_d(got, want, 1e-9), "u6_fsyn_oracle",
                                 "star=%zu got=%.12g want=%.12g", s, got, want);
            }
            // mag 归一化通道 (compute_f_syn) 同口径
            const double got_m = photo_calib::compute_f_syn(
                spectra[0].data(), (int)spectra[0].size(),
                spec_wl.data(), (int)spec_wl.size(),
                fw.data(), ft.data(), (int)fw.size(),
                qw.data(), qt.data(), (int)qw.size(), 15.5);
            const double want_m = oracle::oracle_f_syn_mag(
                spec_wl, spectra[0], fw, ft, qw, qt, 15.5);
            P1PHOT_CHECK(cs, rel_close_d(got_m, want_m, 1e-9), "u6_fsyn_oracle");
        }
    }

    // ── U7: TAN+SIP WCS oracle 对拍 + round-trip ────────────────────────
    {
        // 无 SIP
        pc::WcsTransform wcs(g.crval1, g.crval2, g.crpix1, g.crpix2,
                             g.cd11, g.cd12, g.cd21, g.cd22,
                             0, nullptr, nullptr, nullptr, nullptr);
        oracle::OracleWcs owcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                               g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
        bool cmp_ok = true, rt_ok = true;
        for (int iy = 0; iy < 5; ++iy)
            for (int ix = 0; ix < 5; ++ix) {
                const double x = 5.0 + 12.0 * ix, y = 5.0 + 12.0 * iy;
                double ra1, dec1, ra2, dec2;
                wcs.pixelToSky(x, y, ra1, dec1);
                owcs.pixel_to_sky(x, y, ra2, dec2);
                if (!rel_close_d(ra1, ra2, 1e-9) || !rel_close_d(dec1, dec2, 1e-9))
                    cmp_ok = false;
                double xb, yb;
                wcs.skyToPixel(ra1, dec1, xb, yb);
                if (std::fabs(xb - x) > 1e-6 || std::fabs(yb - y) > 1e-6) rt_ok = false;
            }
        P1PHOT_CHECK(cs, cmp_ok, "u7_wcs_oracle");
        P1PHOT_CHECK(cs, rt_ok, "u7_wcs_roundtrip");
        // SIP order=2 (前向 + 逆向 AP/BP 齐备)
        std::vector<double> A(36, 0.0), B(36, 0.0), AP(36, 0.0), BP(36, 0.0);
        // 系数量级对齐真实 SIP (Gaia/HSC A~1e-7): u²~2800 → 位移 ~5e-4 px,
        // 一阶逆补偿残差 ~1e-6 px — 门 1e-5 可测且不吞被测缺陷
        A[2] = 2.0e-7; A[13] = -1.5e-7; A[8] = 8.0e-8;    // [0*6+2],[2*6+1],[1*6+2]
        B[7] = 1.2e-7; B[12] = -0.9e-8;                    // [1*6+1],[2*6+0]
        AP[2] = -1.9e-7; AP[13] = 1.4e-7;
        BP[7] = -1.1e-7; BP[12] = 0.8e-8;
        pc::WcsTransform wsip(g.crval1, g.crval2, g.crpix1, g.crpix2,
                              g.cd11, g.cd12, g.cd21, g.cd22,
                              2, A.data(), B.data(), AP.data(), BP.data());
        oracle::OracleWcs osip{g.crval1, g.crval2, g.crpix1, g.crpix2,
                               g.cd11, g.cd12, g.cd21, g.cd22, 2, A, B, AP, BP};
        bool sip_cmp = true, sip_rt = true;
        for (int iy = 0; iy < 5; ++iy)
            for (int ix = 0; ix < 5; ++ix) {
                const double x = 5.0 + 12.0 * ix, y = 5.0 + 12.0 * iy;
                double ra1, dec1, ra2, dec2;
                wsip.pixelToSky(x, y, ra1, dec1);
                osip.pixel_to_sky(x, y, ra2, dec2);
                if (!rel_close_d(ra1, ra2, 1e-9) || !rel_close_d(dec1, dec2, 1e-9))
                    sip_cmp = false;
                double xb, yb;
                wsip.skyToPixel(ra1, dec1, xb, yb);
                // 门 1e-5 px: 被测逆 SIP 为直接式一阶补偿 (ALG §13.1
                // skyToPixel — AP/BP 直接加, 非精确逆), 残差 = 未补偿二阶
                // 项 ~|∇A·δSIP|·px ≪ 1e-5 (系数已对齐真实量级); 前向正确性
                // 由 oracle 对拍 rel 1e-9 严控
                if (std::fabs(xb - x) > 1e-5 || std::fabs(yb - y) > 1e-5) sip_rt = false;
            }
        P1PHOT_CHECK(cs, sip_cmp, "u7_wcs_oracle");
        P1PHOT_CHECK(cs, sip_rt, "u7_wcs_roundtrip");
    }

    // ── U8: SIP order 越界硬失败 ─────────────────────────────────────────
    {
        bool threw = false;
        try {
            pc::WcsTransform bad(g.crval1, g.crval2, g.crpix1, g.crpix2,
                                 g.cd11, g.cd12, g.cd21, g.cd22,
                                 6, nullptr, nullptr, nullptr, nullptr);
            (void)bad;
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        P1PHOT_CHECK(cs, threw, "u8_sip_order_guard");
        // C API: -4 (C 边界异常屏障)
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        const int w = g.width, h = g.height;
        std::vector<double> pixels((std::size_t)w * h, 100.0);
        std::vector<double> out((std::size_t)w * h, 0.0);
        int n_matched = -1;
        double scale = 0.0, sigma = 0.0;
        const int rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            6, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -4, "u8_sip_order_guard", "rc=%d", rc);
    }

    // ── U9: F6 aperture 重锚 (p1_wcs_phot_test 4 组对齐) ─────────────────
    {
        using astrocs::phase1::Photometer;
        // (1) 已知通量: oracle 独立逐像素求和对拍 + 常数背景精确
        {
            SplitMix64 rng(0x5EED00000000A90FULL);
            const auto img = fix::fixture_f6_image(48, 48, 100.0, 600.0, 2,
                                                   24.0, 24.0, rng);
            Photometer phot(4.0, 6.0, 10.0);
            const auto res = phot.measure(img.data(), 48, 48, 24.0, 24.0);
            P1PHOT_CHECK(cs, res.ok() && res.value().valid, "u9_aperture_flux");
            double oflux = 0.0, obg = 0.0;
            const bool o = oracle::oracle_aperture_flux(img.data(), 48, 48, 24.0, 24.0,
                                                        4.0, 6.0, 10.0, oflux, obg);
            P1PHOT_CHECK(cs, o, "u9_aperture_flux");
            P1PHOT_CHECK(cs, rel_close_d(res.value().flux, oflux, 1e-12), "u9_aperture_flux");
            P1PHOT_CHECK(cs, rel_close_d(res.value().background, obg, 1e-12),
                         "u9_aperture_flux");
            // snr 语义: flux_error=sqrt(max(sum,0)+n_in·σ_sky²) > 0 → snr>0
            P1PHOT_CHECK(cs, res.value().flux_error > 0.0 && res.value().snr > 0.0,
                         "u9_aperture_flux");
        }
        // (2) 越界显式失败
        {
            SplitMix64 rng(0x5EED00000000A911ULL);
            const auto img = fix::fixture_f6_image(48, 48, 100.0, 600.0, 2,
                                                   24.0, 24.0, rng);
            Photometer phot;
            const auto res = phot.measure(img.data(), 48, 48, -1.0, 24.0);
            P1PHOT_CHECK(cs, res.ok() && !res.value().valid, "u9_aperture_oob");
            P1PHOT_CHECK(cs, res.value().failure_reason == "center out of bounds",
                         "u9_aperture_oob");
        }
        // (3) 空天空环显式失败 (4×4 帧, annulus 6..10 无像素)
        {
            std::vector<float> img(16, 50.0f);
            Photometer phot(1.0, 6.0, 10.0);
            const auto res = phot.measure(img.data(), 4, 4, 1.5, 1.5);
            P1PHOT_CHECK(cs, res.ok() && !res.value().valid, "u9_aperture_nosky");
            P1PHOT_CHECK(cs, res.value().failure_reason == "no sky annulus pixels",
                         "u9_aperture_nosky");
        }
        // (4) 空孔径显式失败 (非整像素中心, aperture 内无整像素)
        {
            SplitMix64 rng(0x5EED00000000A912ULL);
            const auto img = fix::fixture_f6_image(48, 48, 100.0, 600.0, 2,
                                                   24.0, 24.0, rng);
            Photometer phot(0.5, 6.0, 10.0);
            const auto res = phot.measure(img.data(), 48, 48, 0.5, 0.5);
            P1PHOT_CHECK(cs, res.ok() && !res.value().valid, "u9_aperture_empty");
            P1PHOT_CHECK(cs, res.value().failure_reason == "aperture empty",
                         "u9_aperture_empty");
        }
        // (5) Result::fail 通道 (坏图像 → fail, 非 valid=false)
        {
            Photometer phot;
            const auto res = phot.measure(nullptr, 4, 4, 2.0, 2.0);
            P1PHOT_CHECK(cs, res.failed(), "u9_aperture_fail_channel");
        }
    }

    // ── U10: F4 v2 全链 reference_flux oracle 对拍 ──────────────────────
    {
        SplitMix64 rng(0x5EED00000000F0B5ULL);
        std::vector<double> spec_wl;
        std::vector<std::vector<uint8_t>> spectra;
        std::vector<float> fmin, fmul;
        std::vector<double> fw, ft, qw, qt;
        fix::fixture_f4_spectra(rng, spec_wl, spectra, fmin, fmul, fw, ft, qw, qt);
        // 桩星 ra/dec 由 oracle WCS 从目标像素反推; PSF 同像素;
        // psf_flux = oracle F_syn (k=1) → r≈0, status=1
        oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                              g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
        fix::StarField f;
        for (int i = 0; i < 4; ++i) {
            const double x = 12.0 + 13.0 * i, y = 14.0 + 11.0 * i;
            double ra, dec;
            wcs.pixel_to_sky(x, y, ra, dec);
            const double fsyn_oracle = oracle::oracle_f_syn_xpsd(
                spec_wl, spectra[(std::size_t)i], fw, ft, qw, qt,
                fmin[(std::size_t)i], fmul[(std::size_t)i]);
            f.gaia_ra.push_back(ra);
            f.gaia_dec.push_back(dec);
            f.gaia_mag.push_back(-2.5 * std::log10(fsyn_oracle > 0 ? fsyn_oracle : 1.0));
            f.gaia_fsyn.push_back(fsyn_oracle);
            f.psf_cx.push_back(x);
            f.psf_cy.push_back(y);
            f.psf_flux.push_back(fsyn_oracle);
            f.psf_status.push_back(0);
            f.psf_star_ids.push_back(3000 + i);
        }
        stub::FakeClientConfig cfg;
        cfg.wl_start = 336;
        cfg.wl_step = 2;
        cfg.wl_count = (int)spec_wl.size();
        for (int i = 0; i < 4; ++i) {
            stub::FakeStar s;
            s.ra = f.gaia_ra[(std::size_t)i];
            s.dec = f.gaia_dec[(std::size_t)i];
            s.magG = f.gaia_mag[(std::size_t)i];
            s.flux_min = fmin[(std::size_t)i];
            s.flux_mul = fmul[(std::size_t)i];
            s.spectrum = spectra[(std::size_t)i];
            cfg.stars.push_back(s);
        }
        stub::GaiaClient* client = stub::create(cfg);
        const int w = g.width, h = g.height;
        std::vector<double> pixels((std::size_t)w * h, 100.0);
        std::vector<double> out((std::size_t)w * h, 0.0);
        int n_matched = -1;
        double scale = 0.0, sigma = 0.0;
        PhotometricDiag diag;
        std::memset(&diag, 0, sizeof(diag));
        std::vector<PcMatchRecord> records(4);
        const int rc = pc_calibrate_simple_with_gaia_f64_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            qw.data(), qt.data(), (int)qw.size(),
            spec_wl.data(), (int)spec_wl.size(),
            pixels.data(), w, h,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            4, f.psf_star_ids.data(), records.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &n_matched, &scale, &sigma, &diag);
        P1PHOT_CHECK(cs, rc == 0, "u10_v2_fsyn");
        P1PHOT_CHECK(cs, diag.spectrum_rows_total == 4, "u10_v2_fsyn");
        P1PHOT_CHECK(cs, diag.valid_fsyn == 4, "u10_v2_fsyn");
        P1PHOT_CHECK(cs, n_matched == 4, "u10_v2_fsyn");
        for (int i = 0; i < 4; ++i) {
            const double fsyn_oracle = f.gaia_fsyn[(std::size_t)i];
            P1PHOT_CHECK_MSG(cs, records[(std::size_t)i].status == 1 &&
                                     rel_close_d(records[(std::size_t)i].reference_flux,
                                                 fsyn_oracle, 1e-9),
                             "u10_v2_fsyn", "i=%d ref=%.12g want=%.12g status=%d",
                             i, records[(std::size_t)i].reference_flux, fsyn_oracle,
                             records[(std::size_t)i].status);
        }
        // dr3sp_id 跨调用确定性 (同输入两次调用逐位一致)
        {
            std::vector<PcMatchRecord> records2(4);
            std::vector<double> out2((std::size_t)w * h, 0.0);
            int nm2 = -1;
            double sc2 = 0.0, sg2 = 0.0;
            PhotometricDiag diag2;
            std::memset(&diag2, 0, sizeof(diag2));
            pc_calibrate_simple_with_gaia_f64_v2(
                client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
                fw.data(), ft.data(), (int)fw.size(),
                qw.data(), qt.data(), (int)qw.size(),
                spec_wl.data(), (int)spec_wl.size(),
                pixels.data(), w, h,
                f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
                4, f.psf_star_ids.data(), records2.data(),
                g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
                0, nullptr, nullptr, nullptr, nullptr,
                out2.data(), &nm2, &sc2, &sg2, &diag2);
            bool det = true;
            for (int i = 0; i < 4; ++i)
                if (records[(std::size_t)i].dr3sp_id != records2[(std::size_t)i].dr3sp_id)
                    det = false;
            P1PHOT_CHECK(cs, det, "u10_dr3sp_determinism");
        }
        stub::destroy(client);
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
