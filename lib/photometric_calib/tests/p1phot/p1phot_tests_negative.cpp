// P1-PHOT-TEST · negative 组 (§13.4 负面矩阵 + 错误语义 + NaN/Inf)
//
// 覆盖 (TEST-PHOT-DESIGN-001 负面矩阵逐行 / README §6 错误语义):
//   N1  直通通道空指针/尺寸无效: pixels/out null → -1; width/height<=0 → -2
//   N2  v2 通道参数域: pixels null/width<=0 → -1; handle null → -2;
//       filter/spectrum null 或 count<=0 → -1
//   N3  锥形搜索失败注入 (桩 fail_mode=1) → -3 (负面矩阵 "锥搜失败(rc=-3)")
//   N4  锥搜 0 星 → rc=0 退化 scale=1.0 + records status=0/reason=5
//   N5  PSF 全 status≠0 (v2) → rc=0 退化 + records status=3/reason=6
//   N6  NaN/Inf flux 场: gaia_fsyn=NaN / psf_flux<=0/NaN/Inf → 该星
//       invalid-flux (reason=3) 不崩溃; 全无效 → fit_used=0/scale=1.0
//   N7  像素 NaN/Inf 直通: 逐元素传播 (NaN·s=NaN), rc=0
//   N8  SIP order 越界 f32 通道 → -4 (异常屏障)
//   N9  v2 出参向后兼容: out_records=nullptr/psf_star_ids=nullptr → rc=0
#include "p1phot_test_main.hpp"
#include "p1phot_field_stub.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"
#include "p1phot_oracle.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "photometric_calib.h"

namespace p1phot {
namespace {

CheckState g_cs;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

}  // namespace

int test_negative() {
    CheckState& cs = g_cs;
    fix::FrameGeom g;
    const int w = g.width, h = g.height;
    std::vector<double> pixels((std::size_t)w * h, 100.0);
    std::vector<double> out((std::size_t)w * h, 0.0);
    std::vector<float> pixels32((std::size_t)w * h, 100.0f);
    std::vector<float> out32((std::size_t)w * h, 0.0f);
    int nm = -1;
    double sc = 0.0, sg = 0.0;
    std::vector<double> fw = {300.0, 1050.0}, ft = {0.5, 0.5};
    std::vector<double> swl;
    for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);

    // ── N1: 直通通道空指针/尺寸 ──────────────────────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        int rc = pc_calibrate_simple_f64(
            nullptr, w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -1, "n1_null_args", "pixels=null rc=%d", rc);
        rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            nullptr, &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -1, "n1_null_args", "out=null rc=%d", rc);
        rc = pc_calibrate_simple_f64(
            pixels.data(), 0, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -2, "n1_null_args", "width=0 rc=%d", rc);
        rc = pc_calibrate_simple_f64(
            pixels.data(), w, -1,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -2, "n1_null_args", "height=-1 rc=%d", rc);
        // f32 通道同型
        rc = pc_calibrate_simple(
            pixels32.data(), 0, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out32.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK(cs, rc == -2, "n1_null_args");
    }

    // ── N2: v2 通道参数域 ────────────────────────────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        std::vector<PcMatchRecord> rec(4);
        auto call = [&](void* handle, const double* px, int width,
                        const double* fwl, int fcnt, const double* swl_ptr,
                        int scnt) {
            return pc_calibrate_simple_with_gaia_f64_v2(
                handle, g.crval1, g.crval2, 0.05, 10.0, 16.0,
                fwl, (fwl ? ft.data() : nullptr), fcnt,
                nullptr, nullptr, 0,
                swl_ptr, scnt,
                px, width, h,
                f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
                (int)f.psf_cx.size(), f.psf_star_ids.data(), rec.data(),
                g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
                0, nullptr, nullptr, nullptr, nullptr,
                out.data(), &nm, &sc, &sg, nullptr);
        };
        int rc = call(client, nullptr, w, fw.data(), (int)fw.size(), swl.data(),
                      (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "pixels=null rc=%d", rc);
        rc = call(client, pixels.data(), 0, fw.data(), (int)fw.size(), swl.data(),
                  (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "width=0 rc=%d", rc);
        rc = call(nullptr, pixels.data(), w, fw.data(), (int)fw.size(), swl.data(),
                  (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -2, "n2_v2_args", "handle=null rc=%d", rc);
        rc = call(client, pixels.data(), w, nullptr, (int)fw.size(), swl.data(),
                  (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "filter_wl=null rc=%d", rc);
        rc = call(client, pixels.data(), w, fw.data(), 0, swl.data(), (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "filter_count=0 rc=%d", rc);
        rc = call(client, pixels.data(), w, fw.data(), (int)fw.size(), nullptr,
                  (int)swl.size());
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "spectrum_wl=null rc=%d", rc);
        rc = call(client, pixels.data(), w, fw.data(), (int)fw.size(), swl.data(), 0);
        P1PHOT_CHECK_MSG(cs, rc == -1, "n2_v2_args", "spectrum_count=0 rc=%d", rc);
        stub::destroy(client);
    }

    // ── N3: 锥形搜索失败注入 → -3 ────────────────────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        stub::set_fail_mode(client, 1);
        std::vector<PcMatchRecord> rec(4);
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
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -3, "n3_cone_fail", "rc=%d", rc);
        stub::destroy(client);
    }

    // ── N4: 锥搜 0 星 → rc=0 退化 + records reason=5 ─────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        cfg.stars.clear();  // 0 星
        stub::GaiaClient* client = stub::create(cfg);
        std::vector<PcMatchRecord> rec(4);
        for (auto& r : rec) r.reject_reason = -1;
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
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == 0, "n4_no_stars", "rc=%d", rc);
        P1PHOT_CHECK(cs, sc == 1.0 && nm == 0 && sg == 0.0, "n4_no_stars");
        bool rec_ok = true;
        for (const auto& r : rec)
            if (r.status != 0 || r.reject_reason != 5) rec_ok = false;
        P1PHOT_CHECK(cs, rec_ok, "n4_no_stars");
        stub::destroy(client);
    }

    // ── N5: PSF 全 status≠0 → rc=0 退化 + records status=3/reason=6 ──────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        for (auto& s : f.psf_status) s = 2;  // 全无效
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        std::vector<PcMatchRecord> rec(4);
        for (auto& r : rec) r.reject_reason = -1;
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
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == 0, "n5_psf_all_invalid", "rc=%d", rc);
        P1PHOT_CHECK(cs, sc == 1.0 && nm == 0, "n5_psf_all_invalid");
        bool rec_ok = true;
        for (const auto& r : rec)
            if (r.status != 3 || r.reject_reason != 6) rec_ok = false;
        P1PHOT_CHECK(cs, rec_ok, "n5_psf_all_invalid");
        stub::destroy(client);
    }

    // ── N6: NaN/Inf flux 场 ──────────────────────────────────────────────
    {
        // 直通通道: gaia_fsyn NaN → 该星 r 非有限 → invalid 拒绝 (diag 层),
        // 其余 7 星正常定标 (S=0 直 median, k=1 → scale=1.0)
        fix::StarField f = fix::fixture_f1(g, 1.0, 8);
        f.gaia_fsyn[2] = kNaN;
        std::vector<double> o((std::size_t)w * h, 0.0);
        PhotometricDiag dgn;
        std::memset(&dgn, 0, sizeof(dgn));
        const int rc = pc_calibrate_simple_f64(
            pixels.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            o.data(), &nm, &sc, &sg, &dgn);
        P1PHOT_CHECK_MSG(cs, rc == 0, "n6_nan_flux", "rc=%d", rc);
        P1PHOT_CHECK_MSG(cs, dgn.fit_used == 7 && dgn.rejected_quality >= 1,
                         "n6_nan_flux", "fit=%d rq=%d", dgn.fit_used,
                         dgn.rejected_quality);
        P1PHOT_CHECK(cs, sc == 1.0, "n6_nan_flux");
        // v2 通道: psf_flux NaN/-1/Inf → reason=3; (全无效段在下方)
        fix::StarField f2 = fix::fixture_f1(g, 1.0, 6);
        f2.psf_flux[0] = kNaN;
        f2.psf_flux[1] = -1.0;
        f2.psf_flux[2] = kInf;
        std::vector<PcMatchRecord> rec2(6);
        stub::FakeClientConfig cfg2 = bridge::make_config(f2, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client2 = stub::create(cfg2);
        PhotometricDiag dg;
        std::memset(&dg, 0, sizeof(dg));
        const int rc2 = pc_calibrate_simple_with_gaia_f64_v2(
            client2, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            f2.psf_cx.data(), f2.psf_cy.data(), f2.psf_flux.data(), f2.psf_status.data(),
            (int)f2.psf_cx.size(), f2.psf_star_ids.data(), rec2.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &dg);
        P1PHOT_CHECK_MSG(cs, rc2 == 0, "n6_nan_flux", "rc=%d", rc2);
        P1PHOT_CHECK(cs, rec2[0].reject_reason == 3 && rec2[1].reject_reason == 3 &&
                             rec2[2].reject_reason == 3, "n6_nan_flux");
        stub::destroy(client2);
        // 全部无效 → 无有效 r → scale=1.0 (cleanAndScale 空回退)
        fix::StarField f3 = fix::fixture_f1(g, 1.0, 4);
        for (auto& v : f3.psf_flux) v = kNaN;
        std::vector<PcMatchRecord> rec3(4);
        stub::FakeClientConfig cfg3 = bridge::make_config(f3, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client3 = stub::create(cfg3);
        PhotometricDiag dg3;
        std::memset(&dg3, 0, sizeof(dg3));
        const int rc3 = pc_calibrate_simple_with_gaia_f64_v2(
            client3, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            f3.psf_cx.data(), f3.psf_cy.data(), f3.psf_flux.data(), f3.psf_status.data(),
            (int)f3.psf_cx.size(), f3.psf_star_ids.data(), rec3.data(),
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, &dg3);
        P1PHOT_CHECK_MSG(cs, rc3 == 0, "n6_all_invalid", "rc=%d", rc3);
        P1PHOT_CHECK_MSG(cs, sc == 1.0 && dg3.fit_used == 0 && dg3.rejected_quality == 4,
                         "n6_all_invalid", "scale=%.6g fit=%d rq=%d", sc, dg3.fit_used,
                         dg3.rejected_quality);
        stub::destroy(client3);
    }

    // ── N7: 像素 NaN/Inf 直通 (逐元素传播, rc=0) ──────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        std::vector<double> px((std::size_t)w * h, 50.0);
        px[0] = kNaN;
        px[1] = kInf;
        px[2] = -std::numeric_limits<double>::infinity();
        std::vector<double> o((std::size_t)w * h, 0.0);
        const int rc = pc_calibrate_simple_f64(
            px.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            o.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK(cs, rc == 0, "n7_pixel_nan");
        P1PHOT_CHECK(cs, std::isnan(o[0]) && std::isinf(o[1]) && o[2] < 0,
                     "n7_pixel_nan");
    }

    // ── N8: SIP order 越界 f32 通道 → -4 ─────────────────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        const int rc = pc_calibrate_simple(
            pixels32.data(), w, h,
            f.gaia_ra.data(), f.gaia_dec.data(), f.gaia_mag.data(), f.gaia_fsyn.data(),
            (int)f.gaia_ra.size(),
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr, 0,
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            -1, nullptr, nullptr, nullptr, nullptr,
            out32.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == -4, "n8_sip_guard_f32", "rc=%d", rc);
    }

    // ── N9: v2 出参向后兼容 (records/star_ids nullptr) ────────────────────
    {
        fix::StarField f = fix::fixture_f1(g, 1.0, 4);
        stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
        stub::GaiaClient* client = stub::create(cfg);
        const int rc = pc_calibrate_simple_with_gaia_f64_v2(
            client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
            fw.data(), ft.data(), (int)fw.size(),
            nullptr, nullptr, 0,
            swl.data(), (int)swl.size(),
            pixels.data(), w, h,
            f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
            (int)f.psf_cx.size(), nullptr, nullptr,  // 兼容: 无 records
            g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
            0, nullptr, nullptr, nullptr, nullptr,
            out.data(), &nm, &sc, &sg, nullptr);
        P1PHOT_CHECK_MSG(cs, rc == 0, "n9_records_compat", "rc=%d", rc);
        P1PHOT_CHECK(cs, nm == 4, "n9_records_compat");
        stub::destroy(client);
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
