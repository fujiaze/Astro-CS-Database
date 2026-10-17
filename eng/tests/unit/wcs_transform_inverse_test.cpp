// Issue #92 A1-FIX —— WcsTransform 统一 SIP 逆解算子的解析 + 对抗性单元测试。
//
// 被测实现由驱动脚本 `run_wcs_transform_inverse_test.py` 以**当前树内**源码编译
// （不链接旧归档），真实六帧 WCS/samples 由脚本从 `实验/issue92-wcs-gaia-match/
// fixtures/*.json` 生成 `wcs_fixtures.inc`。判据全部打印实测读数。
//
// 覆盖:
//   · 真实六帧 WCS 各 64 个 samples 的 0-based 往返 + FITS 1-based 切点基准
//   · CRVAL 切点（真实 AP_0_0 != 0 的解析反例，修复前必 NaN）
//   · 无 SIP / 无 AP-BP / AP-BP 全零 / 真实 AP-BP 四条路径结果一致
//   · 公开 C ABI pc_project_sky_to_pixel 非 AP 调用回归
//   · 负例: 奇异 CD、TAN 背半球、非有限输入、非有限 AP 系数、病态 SIP 不收敛、SIP order>5
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "wcs_transform.h"
#include "photometric_calib.h"   // pc_project_sky_to_pixel C ABI 声明
#include "wcs_fixtures.inc"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

int g_checks = 0, g_fail = 0;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

void check(bool ok, const std::string& name, const std::string& detail = "") {
    ++g_checks;
    if (!ok) {
        ++g_fail;
        std::printf("[FAIL] %s%s%s\n", name.c_str(), detail.empty() ? "" : " : ",
                    detail.c_str());
    }
}

double ang_sep_deg(double ra1, double dec1, double ra2, double dec2) {
    // Haversine: 在近零角距下数值稳定 (acos 公式在 sep<~1e-6 deg 处被舍入地板
    // 卡在 ~8.5e-7 deg, 无法判别 1e-13 量级的一致性)。
    const double d2r = M_PI / 180.0;
    const double d1 = dec1 * d2r, d2 = dec2 * d2r;
    const double sdd = std::sin((d2 - d1) * 0.5);
    const double sdr = std::sin((ra2 - ra1) * d2r * 0.5);
    const double h = sdd * sdd + std::cos(d1) * std::cos(d2) * sdr * sdr;
    const double hc = std::max(0.0, std::min(1.0, h));
    return 2.0 * std::asin(std::sqrt(hc)) / d2r;
}

pc::WcsTransform make_real(const FixFrame& f) {
    return pc::WcsTransform(f.crval1, f.crval2, f.crpix1, f.crpix2,
                            f.cd11, f.cd12, f.cd21, f.cd22,
                            f.order, f.a, f.b, f.ap, f.bp);
}

}  // namespace

int main() {
    // ── A. 真实六帧: 0-based 往返 + 前向一致性 + 切点基准 ─────────────────
    int real_gate_pass = 0, real_total = 0;
    double worst_pix_err = 0.0, worst_sky_err_deg = 0.0;
    for (int fi = 0; fi < kNFrames; ++fi) {
        const FixFrame& f = kFrames[fi];
        const pc::WcsTransform w = make_real(f);
        const pc::WcsTransform w_noap(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                      f.cd11, f.cd12, f.cd21, f.cd22,
                                      f.order, f.a, f.b, nullptr, nullptr);
        for (int i = 0; i < f.n; ++i) {
            ++real_total;
            double x = kNaN, y = kNaN;
            w.skyToPixel(f.sra[i], f.sdec[i], x, y);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            ++real_gate_pass;
            const double e = std::hypot(x - f.sx[i], y - f.sy[i]);
            if (e > worst_pix_err) worst_pix_err = e;
            // 无 AP/BP 路径必须给出同一解 (统一算子)
            double xn = kNaN, yn = kNaN;
            w_noap.skyToPixel(f.sra[i], f.sdec[i], xn, yn);
            check(std::isfinite(xn) && std::hypot(xn - x, yn - y) <= 1e-6,
                  "real_frame_noap_matches_apbp",
                  f.name + std::string(" sample=") + std::to_string(i));
            // 前向一致性: pixelToSky(sample.x,sample.y) == sample.ra/dec
            double ra = 0, dec = 0;
            w.pixelToSky(f.sx[i], f.sy[i], ra, dec);
            const double se = ang_sep_deg(ra, dec, f.sra[i], f.sdec[i]);
            if (se > worst_sky_err_deg) worst_sky_err_deg = se;
        }
        // 切点: pixelToSky(crpix-1) == crval；skyToPixel(crval) == crpix-1
        double ra0 = 0, dec0 = 0;
        w.pixelToSky(f.crpix1 - 1.0, f.crpix2 - 1.0, ra0, dec0);
        check(ang_sep_deg(ra0, dec0, f.crval1, f.crval2) <= 1e-9,
              "fits_center_pixel_to_sky", f.name);
        double xc = kNaN, yc = kNaN;
        w.skyToPixel(f.crval1, f.crval2, xc, yc);
        check(std::isfinite(xc) && std::hypot(xc - (f.crpix1 - 1.0), yc - (f.crpix2 - 1.0)) <= 1e-6,
              "fits_center_sky_to_pixel", f.name);
    }
    check(real_gate_pass == real_total, "real_frames_all_pass_gate",
          std::to_string(real_gate_pass) + "/" + std::to_string(real_total));
    check(worst_pix_err <= 1e-6, "real_frames_pixel_roundtrip",
          "worst=" + std::to_string(worst_pix_err) + " px");
    check(worst_sky_err_deg <= 1e-9, "real_frames_forward_consistency",
          "worst=" + std::to_string(worst_sky_err_deg) + " deg");
    std::printf("== real frames: gate_pass=%d/%d worst_pix_err=%.3e px "
                "worst_forward_err=%.3e deg\n", real_gate_pass, real_total,
                worst_pix_err, worst_sky_err_deg);

    // ── B. 真实 AP 解析反例: CRVAL 切点 (修复前 AP_0_0 使残差越门 -> NaN) ──
    {
        const FixFrame& f = kFrames[0];
        const pc::WcsTransform w = make_real(f);
        check(std::fabs(f.ap[0]) > 1e-7, "fixture_has_nonzero_ap0",
              "ap0=" + std::to_string(f.ap[0]));
        double x = kNaN, y = kNaN;
        w.skyToPixel(f.crval1, f.crval2, x, y);
        check(std::isfinite(x) && std::isfinite(y), "tangent_point_finite_with_real_ap");
    }

    // ── C. AP/BP 全零 (present but zero) 与真实 AP/BP 结果一致 ────────────
    {
        const FixFrame& f = kFrames[0];
        double apz[36] = {0}, bpz[36] = {0};
        const pc::WcsTransform wz(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                  f.cd11, f.cd12, f.cd21, f.cd22,
                                  f.order, f.a, f.b, apz, bpz);
        const pc::WcsTransform wr = make_real(f);
        const pc::WcsTransform wn(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                  f.cd11, f.cd12, f.cd21, f.cd22,
                                  f.order, f.a, f.b, nullptr, nullptr);
        double max_dz = 0.0, max_dn = 0.0;
        for (int i = 0; i < f.n; ++i) {
            double xz = 0, yz = 0, xr = 0, yr = 0, xn = 0, yn = 0;
            wz.skyToPixel(f.sra[i], f.sdec[i], xz, yz);
            wr.skyToPixel(f.sra[i], f.sdec[i], xr, yr);
            wn.skyToPixel(f.sra[i], f.sdec[i], xn, yn);
            max_dz = std::max(max_dz, std::hypot(xz - xr, yz - yr));
            max_dn = std::max(max_dn, std::hypot(xn - xr, yn - yr));
        }
        check(max_dz <= 1e-6, "apbp_zero_matches_real", "max=" + std::to_string(max_dz));
        check(max_dn <= 1e-6, "apbp_null_matches_real", "max=" + std::to_string(max_dn));
    }

    // ── D. 无 SIP: 纯线性 CD 逆 ───────────────────────────────────────────
    {
        const FixFrame& f = kFrames[0];
        const pc::WcsTransform wl(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                  f.cd11, f.cd12, f.cd21, f.cd22,
                                  0, nullptr, nullptr, nullptr, nullptr);
        // 用真实样本的 (x,y) 前向 -> 再逆 -> 应回到原像素
        double worst = 0.0;
        for (int i = 0; i < f.n; ++i) {
            double ra = 0, dec = 0, x = 0, y = 0;
            wl.pixelToSky(f.sx[i], f.sy[i], ra, dec);
            wl.skyToPixel(ra, dec, x, y);
            worst = std::max(worst, std::hypot(x - f.sx[i], y - f.sy[i]));
        }
        check(worst <= 1e-9, "linear_roundtrip", "worst=" + std::to_string(worst));
    }

    // ── E. 负例 ───────────────────────────────────────────────────────────
    {
        // E1 奇异 CD (det=0): 不同天区不得返回同一合法像素 (#61)
        const double cd_sing[4] = {1.0 / 3600.0, 0.0, 0.0, 0.0};
        const pc::WcsTransform ws(10.0, 20.0, 100.0, 100.0,
                                  cd_sing[0], cd_sing[1], cd_sing[2], cd_sing[3],
                                  0, nullptr, nullptr, nullptr, nullptr);
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        ws.skyToPixel(10.0, 20.0, x1, y1);
        ws.skyToPixel(10.5, 20.5, x2, y2);
        check(!std::isfinite(x1) && !std::isfinite(y1) && !std::isfinite(x2) && !std::isfinite(y2),
              "singular_cd_rejected");

        // E2 TAN 背半球: 对跖点必须无效 (#57)
        const FixFrame& f = kFrames[0];
        const pc::WcsTransform w = make_real(f);
        double xb = 0, yb = 0;
        w.skyToPixel(f.crval1 + 180.0, -f.crval2, xb, yb);
        check(!std::isfinite(xb) && !std::isfinite(yb), "tan_back_hemisphere_rejected");

        // E3 非有限输入
        double xn = 0, yn = 0;
        w.skyToPixel(kNaN, f.crval2, xn, yn);
        check(!std::isfinite(xn) && !std::isfinite(yn), "nonfinite_ra_rejected");
        w.skyToPixel(f.crval1, std::numeric_limits<double>::infinity(), xn, yn);
        check(!std::isfinite(xn) && !std::isfinite(yn), "nonfinite_dec_rejected");

        // E4 非有限 AP 系数 -> 受保护初值 t, 仍由精化+门裁决 => 有限且与无 AP 一致
        {
            double ap_bad[36] = {0}, bp_bad[36] = {0};
            ap_bad[0] = kNaN;
            bp_bad[0] = std::numeric_limits<double>::infinity();
            const pc::WcsTransform wb(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                      f.cd11, f.cd12, f.cd21, f.cd22,
                                      f.order, f.a, f.b, ap_bad, bp_bad);
            const pc::WcsTransform wn(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                      f.cd11, f.cd12, f.cd21, f.cd22,
                                      f.order, f.a, f.b, nullptr, nullptr);
            double max_d = 0.0;
            for (int i = 0; i < f.n; ++i) {
                double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                wb.skyToPixel(f.sra[i], f.sdec[i], x1, y1);
                wn.skyToPixel(f.sra[i], f.sdec[i], x2, y2);
                check(std::isfinite(x1) && std::isfinite(y1), "nonfinite_ap_protected_finite");
                max_d = std::max(max_d, std::hypot(x1 - x2, y1 - y2));
            }
            check(max_d <= 1e-6, "nonfinite_ap_falls_back_to_linear_seed",
                  "max=" + std::to_string(max_d));
        }

        // E5 非有限前向 A 系数 -> 精化不可有限 => 必须 NaN, 不得伪有效
        {
            double a_bad[36] = {0};
            a_bad[6] = kNaN;  // A_1_0
            const pc::WcsTransform wbad(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                        f.cd11, f.cd12, f.cd21, f.cd22,
                                        f.order, a_bad, f.b, f.ap, f.bp);
            double x1 = 0, y1 = 0;
            wbad.skyToPixel(f.sra[0], f.sdec[0], x1, y1);
            check(!std::isfinite(x1) && !std::isfinite(y1), "nonfinite_forward_sip_rejected");
        }

        // E6 病态 SIP: A(u,v)=2u, B(u,v)=2v => F=(3u,3v), 不动点迭代发散, 门必须拒绝
        {
            double a[36] = {0}, b[36] = {0}, ap[36] = {0}, bp[36] = {0};
            a[6] = 2.0;   // A_1_0
            b[1] = 2.0;   // B_0_1
            ap[6] = 2.0;  // AP_1_0
            bp[1] = 2.0;  // BP_0_1
            const double cd[4] = {1.0, 0.0, 0.0, 1.0};
            const pc::WcsTransform wp(0.0, 0.0, 1.0, 1.0,
                                      cd[0], cd[1], cd[2], cd[3],
                                      1, a, b, ap, bp);
            double x1 = 0, y1 = 0;
            wp.skyToPixel(0.001, 0.001, x1, y1);
            check(!std::isfinite(x1) && !std::isfinite(y1),
                  "pathological_sip_nonconvergent_rejected");
        }

        // E7 SIP order > 5: 构造层硬失败 (禁静默截断)
        {
            bool threw = false;
            try {
                pc::WcsTransform w6(f.crval1, f.crval2, f.crpix1, f.crpix2,
                                    f.cd11, f.cd12, f.cd21, f.cd22,
                                    6, f.a, f.b, f.ap, f.bp);
                (void)w6;
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            check(threw, "sip_order_above_5_rejected");
        }
    }

    // ── F. 公开 C ABI 回归 (pc_project_sky_to_pixel, 非 AP 调用面) ────────
    {
        const FixFrame& f = kFrames[0];
        std::vector<double> ra(f.n), dec(f.n), ox(f.n), oy(f.n);
        for (int i = 0; i < f.n; ++i) { ra[i] = f.sra[i]; dec[i] = f.sdec[i]; }
        const int rc = pc_project_sky_to_pixel(
            f.crval1, f.crval2, f.crpix1, f.crpix2,
            f.cd11, f.cd12, f.cd21, f.cd22,
            f.order, f.a, f.b, ra.data(), dec.data(), f.n, ox.data(), oy.data());
        double worst = 0.0;
        bool all_finite = true;
        for (int i = 0; i < f.n; ++i) {
            if (!std::isfinite(ox[i]) || !std::isfinite(oy[i])) { all_finite = false; continue; }
            worst = std::max(worst, std::hypot(ox[i] - f.sx[i], oy[i] - f.sy[i]));
        }
        check(rc == 0, "cabi_rc_zero", "rc=" + std::to_string(rc));
        check(all_finite, "cabi_all_finite");
        check(worst <= 1e-6, "cabi_matches_samples", "worst=" + std::to_string(worst));

        // 奇异 CD -> NaN；order 越界 -> -2
        const double cd_sing[4] = {1.0 / 3600.0, 0.0, 0.0, 0.0};
        const int rc2 = pc_project_sky_to_pixel(
            10.0, 20.0, 100.0, 100.0, cd_sing[0], cd_sing[1], cd_sing[2], cd_sing[3],
            0, nullptr, nullptr, ra.data(), dec.data(), 1, ox.data(), oy.data());
        check(rc2 == 0 && !std::isfinite(ox[0]) && !std::isfinite(oy[0]),
              "cabi_singular_cd_nan");
        const int rc3 = pc_project_sky_to_pixel(
            f.crval1, f.crval2, f.crpix1, f.crpix2, f.cd11, f.cd12, f.cd21, f.cd22,
            6, f.a, f.b, ra.data(), dec.data(), 1, ox.data(), oy.data());
        check(rc3 == -2, "cabi_order_above_5_rc_minus2", "rc=" + std::to_string(rc3));
    }

    std::printf("== checks: %d/%d PASS\n", g_checks - g_fail, g_checks);
    return g_fail == 0 ? 0 : 1;
}
