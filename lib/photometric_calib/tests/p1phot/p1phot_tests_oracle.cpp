// P1-PHOT-TEST · oracle 组 (期望值非被测函数生成)
//
// 覆盖 (模板验收: "无生产函数生成期望值"; SCI-PHOT-001 §11 冻结容差):
//   O1  F2 场全链: oracle 暴力配对 + 独立 IRLS 重建 r 集合 → location/scale
//       对拍 rtol 1e-9 (冻结 NumPy 参考口径的 C++ 对应物)
//   O2  Akima 独立式直拍: 随机曲线 rel 1e-9 + 边界 fill=0 + n=2 线性特例
//       + 常数数据精确常数
//   O3  Simpson 独立式直拍: λ² 被积闭式 (Simpson 对 ≤3 次多项式精确)
//       rel 1e-15 + 3/8 尾分支 (奇数区间)
//   O4  diag 阶段8 统计: r_median/r_p90/r_max oracle 重建对拍 (percentile
//       线性插值同定义)
//   O5  aperture oracle 对拍 (F6 噪声帧, 非常数盒): flux/background rel 1e-12
// 注: U4/U6/U7/U10 已覆盖 KD-tree 匹配/F_syn/WCS 的 oracle 对拍, 本组补
// IRLS 全链与独立数学内核的直接对拍。
#include "p1phot_test_main.hpp"
#include "p1phot_field_stub.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"
#include "p1phot_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "photometric_calib.h"
#include "spectrum_integrator.h"   // 被测域内部符号 (直拍对象)
#include "wcs_transform.h"
#include "photometer.h"            // F6 legacy 旧符号 (O5 对拍, 只读)

namespace p1phot {
using fix::SplitMix64;  // fixture RNG (p1phot::fix)

namespace {

CheckState g_cs;

}  // namespace

int test_oracle() {
    CheckState& cs = g_cs;
    fix::FrameGeom g;
    const int w = g.width, h = g.height;

    // ── O2: Akima 独立式直拍 ─────────────────────────────────────────────
    {
        SplitMix64 rng(0x5EED00000000A71AULL);
        // 随机严格递增节点 + 密插值网格
        for (int trial = 0; trial < 3; ++trial) {
            const int n = 8 + trial * 5;
            std::vector<double> xs, ys;
            double x = 300.0;
            for (int i = 0; i < n; ++i) {
                x += 10.0 + rng.uniform(0.0, 20.0);
                xs.push_back(x);
                ys.push_back(rng.uniform(-5.0, 5.0));
            }
            std::vector<double> dst;
            for (double d = xs.front(); d <= xs.back() + 0.5; d += 1.0)
                dst.push_back(d);
            const auto got = photo_calib::akima_interpolate(xs, ys, dst, 0.0);
            const auto want = oracle::oracle_akima(xs, ys, dst, 0.0);
            bool ok = (got.size() == want.size());
            if (ok)
                for (std::size_t i = 0; i < got.size(); ++i)
                    if (!rel_close_d(got[i], want[i], 1e-9)) ok = false;
            P1PHOT_CHECK_MSG(cs, ok, "o2_akima_reference", "trial=%d", trial);
            // 范围外 fill=0
            bool fill_ok = true;
            for (std::size_t i = 0; i < dst.size(); ++i)
                if (dst[i] < xs.front() || dst[i] > xs.back())
                    if (got[i] != 0.0) fill_ok = false;
            P1PHOT_CHECK(cs, fill_ok, "o2_akima_reference");
        }
        // n=2 两点 → 线性精确
        {
            std::vector<double> xs = {300.0, 1000.0}, ys = {1.0, 3.0};
            std::vector<double> dst = {300.0, 500.0, 800.0, 1000.0};
            const auto got = photo_calib::akima_interpolate(xs, ys, dst, 0.0);
            bool ok = rel_close_d(got[1], 1.0 + (2.0) * (200.0 / 700.0), 1e-12) &&
                      rel_close_d(got[2], 1.0 + (2.0) * (500.0 / 700.0), 1e-12);
            P1PHOT_CHECK(cs, ok, "o2_akima_reference");
        }
        // 常数数据 → 精确常数 (w1=w2=0 分支)
        {
            std::vector<double> xs = {0.0, 1.0, 2.0, 3.0, 4.0}, ys(5, 7.5);
            std::vector<double> dst = {0.5, 1.7, 3.3};
            const auto got = photo_calib::akima_interpolate(xs, ys, dst, 0.0);
            bool ok = true;
            for (double v : got)
                if (!rel_close_d(v, 7.5, 1e-15)) ok = false;
            P1PHOT_CHECK(cs, ok, "o2_akima_reference");
        }
    }

    // ── O3: Simpson 独立式直拍 ───────────────────────────────────────────
    {
        // 偶数区间 (纯 1/3): λ² 精确闭式
        {
            std::vector<double> x, y;
            for (int i = 0; i <= 684; ++i) {  // 684 区间 (偶)
                const double wl = 336.0 + (double)i;
                x.push_back(wl);
                y.push_back(wl * wl);
            }
            const double got = photo_calib::simpson_integrate(x, y);
            const double want = (1020.0 * 1020.0 * 1020.0 - 336.0 * 336.0 * 336.0) / 3.0;
            P1PHOT_CHECK_MSG(cs, rel_close_d(got, want, 1e-15), "o3_simpson_reference",
                             "got=%.15g want=%.15g", got, want);
            const double o = oracle::oracle_simpson(x, y);
            P1PHOT_CHECK(cs, rel_close_d(got, o, 1e-15), "o3_simpson_reference");
        }
        // 奇数区间 (尾 3/8): λ³ 精确闭式 (1/3 与 3/8 对 3 次均精确)
        {
            std::vector<double> x, y;
            for (int i = 0; i <= 685; ++i) {  // 685 区间 (奇)
                const double wl = 336.0 + (double)i;
                x.push_back(wl);
                y.push_back(wl * wl * wl);
            }
            const double got = photo_calib::simpson_integrate(x, y);
            // x 终点 = 336+685 = 1021 (685 区间) → 闭式同区间
            const double want = (1021.0 * 1021.0 * 1021.0 * 1021.0 -
                                 336.0 * 336.0 * 336.0 * 336.0) / 4.0;
            P1PHOT_CHECK_MSG(cs, rel_close_d(got, want, 1e-15), "o3_simpson_reference",
                             "got=%.15g want=%.15g", got, want);
        }
        // 随机曲线独立式对拍
        {
            SplitMix64 rng(0x5EED0000000051B3ULL);
            std::vector<double> x, y;
            for (int i = 0; i <= 128; ++i) {
                x.push_back(336.0 + (double)i);
                y.push_back(rng.uniform(-1.0, 1.0));
            }
            const double got = photo_calib::simpson_integrate(x, y);
            const double want = oracle::oracle_simpson(x, y);
            P1PHOT_CHECK(cs, rel_close_d(got, want, 1e-12), "o3_simpson_reference");
        }
    }

    // ── O1: F2 场全链 oracle IRLS (独立配对+独立统计) ─────────────────────
    {
        const double k = 1.0;
        fix::StarField f = fix::fixture_f2(g, k, 20, 4);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        const double fsyn = fix::v2_fsyn_const();  // 常数谱 XPSD 闭式 (float 语义)
        // oracle 侧: 常数谱 → F_syn 全星同值 (oracle 独立积分复算)
        std::vector<double> swl;
        for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);
        std::vector<uint8_t> spec(swl.size(), 200);
        std::vector<double> fw = {300.0, 1050.0}, ft = {0.5, 0.5};
        const double fsyn_o = oracle::oracle_f_syn_xpsd(swl, spec, fw, ft, {}, {},
                                                        1.0e-15f, 5.0e-18f);
        P1PHOT_CHECK(cs, rel_close_d(fsyn_o, fsyn, 1e-12), "o1_irls_reference");
        // oracle 暴力配对 (像素域) → r 集合 → 独立 IRLS
        oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                              g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
        std::vector<double> gx(f.gaia_ra.size()), gy(f.gaia_ra.size());
        for (std::size_t i = 0; i < f.gaia_ra.size(); ++i)
            wcs.sky_to_pixel(f.gaia_ra[i], f.gaia_dec[i], gx[i], gy[i]);
        std::vector<double> px, py;
        std::vector<int> prow;
        for (std::size_t i = 0; i < f.psf_cx.size(); ++i)
            if (f.psf_status[i] == 0) {
                px.push_back(f.psf_cx[i]);
                py.push_back(f.psf_cy[i]);
                prow.push_back((int)i);
            }
        const auto om = oracle::oracle_bruteforce_match(gx, gy, px, py, 2.0);
        P1PHOT_CHECK(cs, om.size() == f.psf_cx.size(), "o1_irls_reference");
        std::vector<double> r_all;
        for (const auto& m : om) {
            const int row = prow[(std::size_t)m.psf_idx];
            r_all.push_back(std::log10(f.psf_flux[(std::size_t)row] / fsyn_o));
        }
        double oloc = 0, osig = 0;
        int oiters = 0;
        std::vector<char> mask;
        const bool ok = oracle::oracle_irls_tukey(r_all, oloc, osig, oiters, &mask);
        P1PHOT_CHECK(cs, ok, "o1_irls_reference");
        // 被测 v2 全链
        std::vector<double> pixels((std::size_t)w * h, 100.0);
        std::vector<double> out((std::size_t)w * h, 0.0);
        int nm = -1;
        double sc = 0, sg = 0;
        PhotometricDiag dg;
        std::memset(&dg, 0, sizeof(dg));
        std::vector<PcMatchRecord> rec(f.psf_cx.size());
        const int rc = pc_calibrate_simple_with_gaia_f64_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), f.psf_star_ids.data(), rec.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &dg);
        P1PHOT_CHECK_MSG(cs, rc == 0, "o1_irls_reference", "rc=%d", rc);
        // 冻结容差 rtol 1e-9 (SCI §11 NumPy 参考口径)
        P1PHOT_CHECK_MSG(cs, rel_close_d(-std::log10(sc), oloc, 1e-9), "o1_irls_reference",
                         "loc=%.12g oracle=%.12g", -std::log10(sc), oloc);
        P1PHOT_CHECK_MSG(cs, rel_close_d(sc, std::pow(10.0, -oloc), 1e-9), "o1_irls_reference",
                         "scale=%.12g oracle=%.12g", sc, std::pow(10.0, -oloc));
        P1PHOT_CHECK_MSG(cs, rel_close_d(sg, osig, 1e-9), "o1_irls_reference",
                         "sigma=%.12g oracle=%.12g", sg, osig);
        P1PHOT_CHECK_MSG(cs, dg.robust_iterations == oiters, "o1_irls_reference",
                         "impl=%d oracle=%d", dg.robust_iterations, oiters);
        // 冻结鲁棒门: Δlocation<0.1 dex
        P1PHOT_CHECK(cs, std::fabs(oloc) < 0.1, "o1_irls_reference");
        // 逐星 reason 对拍: oracle mask=1 ↔ status=1; mask=0 ↔ status=2/reason=2
        {
            bool mask_ok = true;
            for (std::size_t m = 0; m < om.size(); ++m) {
                const int row = prow[(std::size_t)om[m].psf_idx];
                const PcMatchRecord& r = rec[(std::size_t)row];
                if (mask[m] && !(r.status == 1 && r.reject_reason == 0)) mask_ok = false;
                if (!mask[m] && !(r.status == 2 && r.reject_reason == 2)) mask_ok = false;
            }
            P1PHOT_CHECK(cs, mask_ok, "o1_irls_reference");
        }
        stub::destroy(client);
    }

    // ── O4: diag 阶段8 统计 oracle 重建 ──────────────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.4, 12);
        // 扰动场 (S>0) 使 r_inliers 非常数 → percentile 语义可验
        oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                              g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
        std::vector<std::pair<double, double>> pix;
        for (int row = 0; row < 6; ++row)
            for (int col = 0; col < 4; ++col)
                pix.emplace_back(10.0 + 12.0 * col + 1.0 * row, 10.0 + 9.0 * row);
        SplitMix64 rng(0x5EED000000000003ULL);
        fix::StarField fp;
        fix::build_matched_field(g, wcs, pix, 1.4, 1000.0, rng, 0.02, fp);
        std::vector<double> pixels((std::size_t)w * h, 100.0);
        stub::FakeClientConfig cfg = bridge::make_config(fp, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        const double fsyn_c = fix::v2_fsyn_const();  // 常数谱 XPSD 闭式
        std::vector<double> swl;
        for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);
        std::vector<double> fw = {300.0, 1050.0}, ft = {0.5, 0.5};
        std::vector<double> out((std::size_t)w * h, 0.0);
        int nm = -1;
        double sc = 0, sg = 0;
        PhotometricDiag dg;
        std::memset(&dg, 0, sizeof(dg));
        const int rc = pc_calibrate_simple_with_gaia_f64_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0, swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            fp.psf_cx.data(), fp.psf_cy.data(), fp.psf_flux.data(), fp.psf_status.data(),
            (int)fp.psf_cx.size(), nullptr, nullptr,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &dg);
        P1PHOT_CHECK(cs, rc == 0, "o4_diag_stats");
        // oracle: inliers 由 status=1 records 回连 (r 集合), percentile 复算
        std::vector<double> r_in;
        for (int i = 0; i < (int)fp.psf_cx.size(); ++i) {
            // v2 无 records (nullptr) → 用 location 重建: inlier=全部正常星
            // (±0.02 扰动全 |u|<cS) — r 集合=全部 12 星
            r_in.push_back(std::log10(fp.psf_flux[(std::size_t)i] / fsyn_c));
        }
        // location 对拍 → r_inliers = r - location 距离中值等
        const double loc = -std::log10(sc);
        std::vector<double> dev;
        for (double r : r_in) dev.push_back(std::fabs(r - loc));
        std::sort(dev.begin(), dev.end());
        // r_median/r_p90 重建需 inliers 顺序 — 被测 percentile 于 r_inliers
        // (Tukey 权重>0)。±0.02 扰动 + cS≈0.139 → 全 inlier → r_inliers=r_all
        auto pct = [&](std::vector<double> v, double p) {
            std::sort(v.begin(), v.end());
            const std::size_t n = v.size();
            if (n == 1) return v[0];
            const double idx = p * (double)(n - 1);
            const std::size_t lo = (std::size_t)std::floor(idx);
            const std::size_t hiq = (lo + 1 < n) ? lo + 1 : n - 1;
            const double fr = idx - (double)lo;
            return v[lo] * (1.0 - fr) + v[hiq] * fr;
        };
        P1PHOT_CHECK(cs, rel_close_d(dg.r_median, pct(r_in, 0.5), 1e-9), "o4_diag_stats");
        P1PHOT_CHECK(cs, rel_close_d(dg.r_p90, pct(r_in, 0.9), 1e-9), "o4_diag_stats");
        P1PHOT_CHECK(cs, rel_close_d(dg.r_max, *std::max_element(r_in.begin(), r_in.end()),
                                     1e-9), "o4_diag_stats");
        (void)dev;
        stub::destroy(client);
    }

    // ── O5: aperture oracle (噪声帧) ─────────────────────────────────────
    {
        using astrocs::phase1::Photometer;
        SplitMix64 rng(0x5EED00000000A90EULL);
        const auto img = fix::fixture_f6_image(48, 48, 100.0, 600.0, 2, 24.0, 24.0, rng);
        Photometer phot(4.0, 6.0, 10.0);
        const auto res = phot.measure(img.data(), 48, 48, 24.0, 24.0);
        P1PHOT_CHECK(cs, res.ok() && res.value().valid, "o5_aperture_oracle");
        double oflux = 0.0, obg = 0.0;
        const bool o = oracle::oracle_aperture_flux(img.data(), 48, 48, 24.0, 24.0,
                                                    4.0, 6.0, 10.0, oflux, obg);
        P1PHOT_CHECK(cs, o, "o5_aperture_oracle");
        P1PHOT_CHECK_MSG(cs, rel_close_d(res.value().flux, oflux, 1e-12), "o5_aperture_oracle",
                         "flux=%.12g oracle=%.12g", res.value().flux, oflux);
        P1PHOT_CHECK(cs, rel_close_d(res.value().background, obg, 1e-12),
                     "o5_aperture_oracle");
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
