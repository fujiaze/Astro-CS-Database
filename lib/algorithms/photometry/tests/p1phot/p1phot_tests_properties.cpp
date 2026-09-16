// P1-PHOT-TEST · properties 组 (不变量 I1-I6 + 1-N worker 确定性)
//
// 覆盖 (TEST-PHOT-DESIGN-001 §13.4 不变量 / README §7 并发与确定性):
//   P1  I5 线程扫描: omp threads 1/4/max 下直通 f64 + v2 全输出 bitwise
//       (DOC §7: 逐星/逐元素独立 ⇒ 输出 bitwise 与线程数无关)
//   P2  F2 鲁棒门: 20% 离群 Δlocation<0.1 dex (冻结门) + 离群星权重 0
//       (records reject_reason=2) + fit_used=80% (SCI-PHOT-001 §11)
//   P3  IRLS 真迭代语义: S>0 场 robust_iterations>=1 且与 oracle 独立复算一致
//   P4  I2: sigma_residual == MAD(r_inliers)/0.6745 oracle 独立复算 (rtol 1e-9)
//       + records 残差与逐星 r 一致 (I6 方向)
//   P5  v2/_f64_v2 双通道: 像素 f32 可精确表示场景下 scale/sigma/records
//       bitwise, out_pixels f32 存储舍入 rtol 1e-7
//   P6  函数级确定性: 同输入重复调用两次全输出 bitwise (1-N worker 之外
//       的重放确定性)
#include "p1phot_test_main.hpp"
#include "p1phot_field_stub.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"
#include "p1phot_oracle.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "photometric_calib.h"

namespace p1phot {
using fix::SplitMix64;  // fixture RNG (p1phot::fix)

namespace {

CheckState g_cs;

// v2 f64 调用装配 (F1 场 + 常数谱 + 两点常数滤光片; 输出经结构体取回)
struct V2Result {
    int rc = -99;
    std::vector<double> out;
    int n_matched = -1;
    double scale = 0.0, sigma = 0.0;
    PhotometricDiag diag;
    std::vector<PcMatchRecord> records;
};

V2Result run_v2(void* client, const fix::StarField& f, const fix::FrameGeom& g,
                const std::vector<double>& pixels, bool with_records) {
    V2Result r;
    r.out.assign((std::size_t)g.width * g.height, 0.0);
    if (with_records) r.records.resize(f.psf_cx.size());
    std::memset(&r.diag, 0, sizeof(r.diag));
    std::vector<double> fw = {300.0, 1050.0};
    std::vector<double> ft = {0.5, 0.5};
    std::vector<double> swl;
    for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);
    r.rc = pc_calibrate_simple_with_gaia_f64_v2(
        client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
        fw.data(), ft.data(), (int)fw.size(),
        nullptr, nullptr, 0,
        swl.data(), (int)swl.size(),
        pixels.data(), g.width, g.height,
        f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
        (int)f.psf_cx.size(),
        with_records ? f.psf_star_ids.data() : nullptr,
        with_records ? r.records.data() : nullptr,
        g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
        0, nullptr, nullptr, nullptr, nullptr,
        r.out.data(), &r.n_matched, &r.scale, &r.sigma, &r.diag);
    return r;
}

void fill_random_pixels(std::vector<double>& px, std::uint64_t seed) {
    SplitMix64 rng(seed);
    for (auto& v : px) v = rng.uniform(10.0, 2000.0);
}

bool diag_bits_eq(const PhotometricDiag& a, const PhotometricDiag& b) {
    return a.spectrum_rows_total == b.spectrum_rows_total &&
           a.valid_fsyn == b.valid_fsyn &&
           a.gaia_projected_in_frame == b.gaia_projected_in_frame &&
           a.psf_total == b.psf_total && a.psf_valid == b.psf_valid &&
           a.spatial_candidates == b.spatial_candidates &&
           a.unique_matches == b.unique_matches &&
           a.rejected_ambiguous == b.rejected_ambiguous &&
           a.rejected_distance == b.rejected_distance &&
           a.rejected_quality == b.rejected_quality &&
           a.fit_used == b.fit_used && a.robust_iterations == b.robust_iterations &&
           bits_eq_d(a.scale_factor, b.scale_factor) &&
           bits_eq_d(a.sigma_residual, b.sigma_residual) &&
           bits_eq_d(a.r_median, b.r_median) && bits_eq_d(a.r_p90, b.r_p90) &&
           bits_eq_d(a.r_max, b.r_max) &&
           bits_eq_d(a.match_distance_median, b.match_distance_median) &&
           bits_eq_d(a.match_distance_p90, b.match_distance_p90) &&
           bits_eq_d(a.match_distance_max, b.match_distance_max);
}

}  // namespace

int test_properties() {
    CheckState& cs = g_cs;
    fix::FrameGeom g;
    const int w = g.width, h = g.height;

    // ── P1: I5 线程扫描 (1/4/max) — 直通 f64 + v2 双通道 bitwise ─────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.25, 12);
        std::vector<double> pixels((std::size_t)w * h);
        fill_random_pixels(pixels, 0x5EED00000000515AULL);

        // 直通 f64 基准 (threads=1)
        auto run_direct = [&](std::vector<double>& out, int& nm, double& sc,
                              double& sg, PhotometricDiag& dg) {
            std::memset(&dg, 0, sizeof(dg));
            out.assign((std::size_t)w * h, 0.0);
            return pc_calibrate_simple_f64(
                pixels.data(), w, h,
                f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
                (int)f.gaia_ra.size(),
                f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
                (int)f.psf_cx.size(), nullptr, nullptr, 0,
                g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
                0, nullptr, nullptr, nullptr, nullptr,
                out.data(), &nm, &sc, &sg, &dg);
        };
        std::vector<double> base_out;
        int base_nm = -1;
        double base_sc = 0, base_sg = 0;
        PhotometricDiag base_dg;
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        int rc = run_direct(base_out, base_nm, base_sc, base_sg, base_dg);
        P1PHOT_CHECK(cs, rc == 0, "p1_thread_bitwise");

        const int thread_counts[] = {4,
#ifdef _OPENMP
                                     omp_get_max_threads()
#else
                                     1
#endif
        };
        for (int nt : thread_counts) {
#ifdef _OPENMP
            omp_set_num_threads(nt);
#endif
            std::vector<double> out;
            int nm = -1;
            double sc = 0, sg = 0;
            PhotometricDiag dg;
            rc = run_direct(out, nm, sc, sg, dg);
            P1PHOT_CHECK_MSG(cs, rc == 0, "p1_thread_bitwise", "nt=%d rc=%d", nt, rc);
            bool pix_ok = true;
            for (int i = 0; i < w * h; ++i)
                if (!bits_eq_d(out[(std::size_t)i], base_out[(std::size_t)i]))
                    pix_ok = false;
            P1PHOT_CHECK_MSG(cs, pix_ok, "p1_thread_bitwise", "nt=%d pixels", nt);
            P1PHOT_CHECK_MSG(cs, nm == base_nm && bits_eq_d(sc, base_sc) &&
                                     bits_eq_d(sg, base_sg) && diag_bits_eq(dg, base_dg),
                             "p1_thread_bitwise", "nt=%d scalars", nt);
        }
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif

        // v2 通道 (桩) 线程扫描: records/diag bitwise
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        V2Result base_v2 = run_v2(client, f, g, pixels, true);
        P1PHOT_CHECK(cs, base_v2.rc == 0, "p1_thread_bitwise");
        for (int nt : thread_counts) {
#ifdef _OPENMP
            omp_set_num_threads(nt);
#endif
            V2Result v = run_v2(client, f, g, pixels, true);
            P1PHOT_CHECK_MSG(cs, v.rc == 0, "p1_thread_bitwise", "v2 nt=%d rc=%d", nt, v.rc);
            bool pix_ok = true;
            for (int i = 0; i < w * h; ++i)
                if (!bits_eq_d(v.out[(std::size_t)i], base_v2.out[(std::size_t)i]))
                    pix_ok = false;
            bool rec_ok = (v.records.size() == base_v2.records.size());
            if (rec_ok)
                for (std::size_t i = 0; i < v.records.size(); ++i) {
                    const PcMatchRecord& a = v.records[i];
                    const PcMatchRecord& b = base_v2.records[i];
                    if (a.star_id != b.star_id || a.dr3sp_id != b.dr3sp_id ||
                        a.status != b.status || a.reject_reason != b.reject_reason ||
                        !bits_eq_d(a.reference_flux, b.reference_flux) ||
                        !bits_eq_d(a.residual, b.residual))
                        rec_ok = false;
                }
            P1PHOT_CHECK_MSG(cs, pix_ok, "p1_thread_bitwise", "v2 nt=%d pixels", nt);
            P1PHOT_CHECK_MSG(cs, bits_eq_d(v.scale, base_v2.scale) &&
                                     bits_eq_d(v.sigma, base_v2.sigma) &&
                                     diag_bits_eq(v.diag, base_v2.diag) && rec_ok,
                             "p1_thread_bitwise", "v2 nt=%d scalars", nt);
        }
        stub::destroy(client);
#ifdef _OPENMP
        omp_set_num_threads(omp_get_max_threads());
#endif
    }

    // ── P2/P3/P4: F2 鲁棒门 + IRLS 迭代 + I2 oracle 复算 ─────────────────
    {
        const double k = 1.0;
        fix::StarField f = fix::fixture_f2(g, k, 20, 4);  // 20% 离群 (4/20)
        std::vector<double> pixels((std::size_t)w * h);
        fill_random_pixels(pixels, 0x5EED00000000F2A0ULL);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        V2Result v = run_v2(client, f, g, pixels, true);
        P1PHOT_CHECK_MSG(cs, v.rc == 0, "p2_robust_gate", "rc=%d", v.rc);
        // 冻结门: Δlocation < 0.1 dex (离群不拉偏)
        const double location = -std::log10(v.scale);
        P1PHOT_CHECK_MSG(cs, std::fabs(location) < 0.1, "p2_robust_gate",
                         "location=%.6f", location);
        // fit_used = inlier 数 (80%); 离群星 records reason=2 (IRLS-outlier,
        // 权重 0 — SCI §11 "离群权重为 0")
        P1PHOT_CHECK_MSG(cs, v.diag.fit_used == 16, "p2_robust_gate",
                         "fit=%d", v.diag.fit_used);
        int n_irls_outlier = 0;
        for (int i = 0; i < 4; ++i) {
            const PcMatchRecord& rec = v.records[(std::size_t)i];
            if (rec.status == 2 && rec.reject_reason == 2) ++n_irls_outlier;
        }
        P1PHOT_CHECK_MSG(cs, n_irls_outlier == 4, "p2_robust_gate",
                         "irls_outlier=%d", n_irls_outlier);
        // P3: S>0 真迭代 (离群场 S>0)
        P1PHOT_CHECK_MSG(cs, v.diag.robust_iterations >= 1, "p3_irls_iters",
                         "iters=%d", v.diag.robust_iterations);
        // P4 (I2): sigma_residual == MAD(r_inliers)/0.6745 oracle 复算。
        // r 集合由输入独立重建 — 复刻被测同一估计过程: 全部配对样本
        // (含离群) 进 IRLS (被测收敛门停机于过渡值, inlier 子集重算会得
        // 不同不动点, 属过程差异); F_syn=常数谱 XPSD 闭式
        {
            const double fsyn = fix::v2_fsyn_const();  // 常数谱 XPSD 闭式
            std::vector<double> r_in;
            for (const auto& rec : v.records)
                if (rec.status == 1 || (rec.status == 2 && rec.reject_reason == 2)) {
                    // F_instr 从 fixture 侧按 star_id 回连 (装配已知)
                    const std::size_t row = (std::size_t)(rec.star_id - 1000);
                    r_in.push_back(std::log10(f.psf_flux[row] / fsyn));
                }
            double oloc = 0, osig = 0;
            int oiters = 0;
            std::vector<char> mask;
            const bool ok = oracle::oracle_irls_tukey(r_in, oloc, osig, oiters, &mask);
            P1PHOT_CHECK(cs, ok, "p4_sigma_oracle");
            P1PHOT_CHECK_MSG(cs, rel_close_d(v.sigma, osig, 1e-9), "p4_sigma_oracle",
                             "sigma=%.12g oracle=%.12g", v.sigma, osig);
            P1PHOT_CHECK_MSG(cs, rel_close_d(location, oloc, 1e-9), "p4_sigma_oracle",
                             "loc=%.12g oracle=%.12g", location, oloc);
            P1PHOT_CHECK_MSG(cs, v.diag.robust_iterations == oiters, "p3_irls_iters",
                             "impl=%d oracle=%d", v.diag.robust_iterations, oiters);
            // records 残差一致性 (I6): status=1 行 residual == log10(F_instr/F_syn)
            bool res_ok = true;
            for (const auto& rec : v.records)
                if (rec.status == 1) {
                    const std::size_t row = (std::size_t)(rec.star_id - 1000);
                    if (!rel_close_d(rec.residual, std::log10(f.psf_flux[row] / fsyn), 1e-12))
                        res_ok = false;
                }
            P1PHOT_CHECK(cs, res_ok, "p4_records_residual");
        }
        stub::destroy(client);
    }

    // ── P5: v2/_f64_v2 双通道 (f32 像素可精确表示) ────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.25, 8);
        std::vector<double> pixels64((std::size_t)w * h, 128.0);  // f32 精确
        std::vector<float> pixels32((std::size_t)w * h, 128.0f);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        V2Result v64 = run_v2(client, f, g, pixels64, true);
        // f32 通道
        std::vector<float> out32((std::size_t)w * h, 0.0f);
        int nm32 = -1;
        double sc32 = 0, sg32 = 0;
        PhotometricDiag dg32;
        std::memset(&dg32, 0, sizeof(dg32));
        std::vector<PcMatchRecord> rec32(f.psf_cx.size());
        std::vector<double> fw = {300.0, 1050.0};
        std::vector<double> ft = {0.5, 0.5};
        std::vector<double> swl;
        for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);
        const int rc32 = pc_calibrate_simple_with_gaia_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels32.data(), w, h,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), f.psf_star_ids.data(), rec32.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out32.data(), &nm32, &sc32, &sg32, &dg32);
        P1PHOT_CHECK(cs, v64.rc == 0 && rc32 == 0, "p5_dual_precision");
        // scale/sigma/records bitwise (像素 dtype 不影响匹配/IRLS)
        P1PHOT_CHECK(cs, bits_eq_d(sc32, v64.scale) && bits_eq_d(sg32, v64.sigma),
                     "p5_dual_precision");
        bool rec_ok = true;
        for (std::size_t i = 0; i < rec32.size(); ++i)
            if (rec32[i].star_id != v64.records[i].star_id ||
                rec32[i].status != v64.records[i].status ||
                rec32[i].reject_reason != v64.records[i].reject_reason ||
                !bits_eq_d(rec32[i].reference_flux, v64.records[i].reference_flux) ||
                !bits_eq_d(rec32[i].residual, v64.records[i].residual))
                rec_ok = false;
        P1PHOT_CHECK(cs, rec_ok, "p5_dual_precision");
        // out_pixels: f32 存储舍入 rtol 1e-7
        bool pix_ok = true;
        for (int i = 0; i < w * h; ++i)
            if (!rel_close_f(out32[(std::size_t)i], v64.out[(std::size_t)i], 1e-7))
                pix_ok = false;
        P1PHOT_CHECK(cs, pix_ok, "p5_dual_precision");
        stub::destroy(client);
    }

    // ── P6: 函数级重放确定性 (同输入两次调用 bitwise) ─────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 0.8, 10);
        std::vector<double> pixels((std::size_t)w * h);
        fill_random_pixels(pixels, 0x5EED000000009E9AULL);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        V2Result a = run_v2(client, f, g, pixels, true);
        V2Result b = run_v2(client, f, g, pixels, true);
        P1PHOT_CHECK(cs, a.rc == 0 && b.rc == 0, "p6_replay_bitwise");
        bool pix_ok = true;
        for (int i = 0; i < w * h; ++i)
            if (!bits_eq_d(a.out[(std::size_t)i], b.out[(std::size_t)i])) pix_ok = false;
        P1PHOT_CHECK(cs, pix_ok, "p6_replay_bitwise");
        P1PHOT_CHECK(cs, bits_eq_d(a.scale, b.scale) && bits_eq_d(a.sigma, b.sigma) &&
                             diag_bits_eq(a.diag, b.diag),
                     "p6_replay_bitwise");
        stub::destroy(client);
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
