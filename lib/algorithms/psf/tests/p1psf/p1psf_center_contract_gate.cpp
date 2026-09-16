// ============================================================================
// p1psf_center_contract_gate.cpp — G-P1-CENTER-CONTRACT-1: psf 块像素中心口径门
// ----------------------------------------------------------------------------
// 任务: W4-A1（像素中心契约口径; OPEN_ITEMS A1 / 问题扫描 M1a-C-003）。
//
// 缺陷背景（探针实测, run/PROJECT-GOVERNANCE-01/W4-A1/logs/30_oracle.txt）:
//   * psf 块 cx/cy = dpsf 原始输出 = **index-is-center（0-based 数组下标即中心）**
//     —— 与 star_coord_contract.h 的统一契约、DATA-P1-PSF §15.2/§15.3 一致;
//   * 内部 0-based 与 FITS 1-based（CRPIX 1-based, Paper I §2.1.1）之间只允许
//     **一次** +1 桥接（SCI-WCS-001 §3a/§5a: 内部 0-based, xp = x + 1）;
//   * 消费链实测:
//       psf→photometry  pc::WcsTransform::pixelToSky  x-(CRPIX-1)  ⇒ 差 0 px ✓
//       psf→snr         snr pixelToSkySimple          x-(CRPIX-1)  ⇒ 差 0 px ✓
//       psf→导出        WcsTan（1-based 契约）若直喂 0-based 下标 ⇒ 恒差 1 px ✗
//     （后者即 p1_op_wcs 修复前形态; 修复见 module_adapters.cpp kP1FitsPixelOrigin）
//
// 本门链接 **真实生产源**（不 mock, 不重写公式）:
//   * lib/algorithms/photometry/cpp/src/wcs_transform.cpp      (photometry 消费端)
//   * lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp(+snr_science.cpp) (SNR 消费端)
//   * lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp     (导出 1-based 契约)
//   * lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h
//
// 判据（docs/algorithms/GATES_AND_TOLERANCES.md 待登记为 G-P1-CENTER-CONTRACT-1）:
//   [C1] 契约恒等式: sdet→统一 -0.5 / dpsf→统一 恒等 / 统一→ipv +0.5
//   [C2] psf→photometry: 16 个 psf 块点经生产 pixelToSky 与**独立**单位向量
//        gnomonic 参考解（FITS 1-based 入参 = x+1）一致 |Δ| <= 1e-9 deg
//   [C3] psf→snr: snr_extract_model_v3 控制点 ra/dec 同上判据
//   [C4] 导出链: WcsTan(1-based 契约) 经**单次** +1 桥接后与参考解一致 <= 1e-9 deg
//   [C5] 原点鉴别力（防恒真）: 同一 WcsTan **未桥接**喂法与桥接喂法的角距
//        必须 >= 0.9 px（实测 1.414 px = x/y 各 1px）
//
// 负例 (ENGINEERING_SPEC §8 可执行负例面): argv[1] == "negative-injection" 时
//   [C4] 用**修复前形态**（0-based 直喂 1-based 契约）计算, 必须被 [C4] 判据
//   检出（>= 0.9 px）且本进程打印 "NEGATIVE INJECTION DETECTED" 并 rc=0;
//   若注入未被检出（rc=1）说明本门失效。
//
// 注册: lib/algorithms/psf/tests/p1psf/CMakeLists.txt
//   ctest -R p1psf_center_contract_gate       (正例, 必 PASS)
//   ctest -R p1psf_center_contract_gate_neg   (负例注入, 必被检出)
//   ctest -R p1psf_center_contract_astropy    (astropy 第三方交叉)
//
// --export <path>: 导出各链 (ra,dec) 供 astropy 第三方判定 (fail-closed)。
// ============================================================================
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "snr_estimator.h"
#include "star_coord_contract.h"
#include "wcs_tan.h"
#include "wcs_transform.h"

namespace coord = astrocs::p1::coord;

namespace {

// WCS fixture: CRPIX 1-based, CD deg/px (与 p1phot/p1wcs fixture 同族)
constexpr double kCrpix1 = 16.0, kCrpix2 = 20.0;
constexpr double kCrval1 = 10.0, kCrval2 = 20.0;
constexpr double kCd11 = -1.0e-4, kCd12 = 0.0, kCd21 = 0.0, kCd22 = 1.0e-4;
constexpr double kPxScaleDeg = 1.0e-4;        // sqrt|det CD| = 1e-4 deg/px
constexpr double kAngTolDeg = 1.0e-9;         // 一致判据 (两路径均 FP64 同式)
constexpr double kMinOriginSepPx = 0.9;       // 未桥接必须 >= 0.9 px 被检出
constexpr int    kNumPoints = 16;

int g_checks = 0;
int g_failures = 0;
bool g_negative_injection = false;
std::vector<std::string> g_export_lines;

void check(bool ok, const std::string& msg) {
    ++g_checks;
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", msg.c_str());
    if (!ok) ++g_failures;
}

// 独立参考解: 单位向量 gnomonic (与 WcsTan::pix2sky / pc::WcsTransform 不同源推导),
// 入参为 **FITS 1-based** 像素 (Paper I §2.1.1)。
void ref_gnomonic_1based(double px1, double py1, double* ra, double* dec) {
    constexpr double d2r = 0.017453292519943295769;
    constexpr double r2d = 57.295779513082320877;
    const double u = px1 - kCrpix1;
    const double v = py1 - kCrpix2;
    const double xi = (kCd11 * u + kCd12 * v) * d2r;
    const double eta = (kCd21 * u + kCd22 * v) * d2r;
    const double a0 = kCrval1 * d2r, d0 = kCrval2 * d2r;
    const double ca = std::cos(a0), sa = std::sin(a0);
    const double c0 = std::cos(d0), s0 = std::sin(d0);
    const double ex = -sa, ey = ca;
    const double nx = -s0 * ca, ny = -s0 * sa, nz = c0;
    double qx = c0 * ca + xi * ex + eta * nx;
    double qy = c0 * sa + xi * ey + eta * ny;
    double qz = s0 + eta * nz;
    const double n = std::sqrt(qx * qx + qy * qy + qz * qz);
    qx /= n; qy /= n; qz /= n;
    *dec = std::asin(qz) * r2d;
    *ra = std::atan2(qy, qx) * r2d;
}

double sep_deg(double ra1, double dec1, double ra2, double dec2) {
    constexpr double d2r = 0.017453292519943295769;
    const double d1 = dec1 * d2r, d2 = dec2 * d2r;
    double dra = (ra2 - ra1) * d2r;
    while (dra > M_PI) dra -= 2.0 * M_PI;
    while (dra < -M_PI) dra += 2.0 * M_PI;
    const double sh = std::sin((d2 - d1) / 2.0), sn = std::sin(dra / 2.0);
    const double s = sh * sh + std::cos(d1) * std::cos(d2) * sn * sn;
    return 2.0 * std::asin(std::sqrt(s > 1.0 ? 1.0 : s)) / d2r;
}

void export_rec(const char* key, double ra, double dec) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "{\"key\":\"%s\",\"ra\":%.17g,\"dec\":%.17g}", key, ra, dec);
    g_export_lines.push_back(buf);
}

// psf 块取样点 = index-is-center (0-based 数组下标即中心)
void psf_block_points(std::vector<std::pair<double, double>>* pts) {
    for (int i = 0; i < kNumPoints; ++i) {
        pts->emplace_back(10.0 + 1.5 * i, 12.0 + 0.75 * i);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string export_path;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "negative-injection") == 0) {
            g_negative_injection = true;
        } else if (std::strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            export_path = argv[++i];
        }
    }
    std::printf("G-P1-CENTER-CONTRACT-1 psf 块像素中心口径门 (negative_injection=%d)\n",
                g_negative_injection ? 1 : 0);
    std::printf("fixture: CRPIX=(%.1f,%.1f) 1-based CRVAL=(%.1f,%.1f) CD=diag(%.1e,%.1e)\n",
                kCrpix1, kCrpix2, kCrval1, kCrval2, kCd11, kCd22);
    {
        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "{\"fixture\":{\"crpix1\":%.17g,\"crpix2\":%.17g,\"crval1\":%.17g,"
            "\"crval2\":%.17g,\"cd11\":%.17g,\"cd12\":%.17g,\"cd21\":%.17g,"
            "\"cd22\":%.17g,\"n_points\":%d}}",
            kCrpix1, kCrpix2, kCrval1, kCrval2, kCd11, kCd12, kCd21, kCd22, kNumPoints);
        g_export_lines.push_back(buf);
    }

    std::vector<std::pair<double, double>> pts;
    psf_block_points(&pts);

    // ---- [C1] star_coord_contract.h 契约恒等式 ----
    {
        const bool ok_sdet = std::fabs(coord::star_measurement_from_sdet(182.9472) - 182.4472) < 1e-12;
        const bool ok_dpsf = std::fabs(coord::star_measurement_from_dpsf(182.4472) - 182.4472) < 1e-12;
        const bool ok_ipv  = std::fabs(coord::ipv_detection_from_star_measurement(182.4472) - 182.9472) < 1e-12;
        check(ok_sdet && ok_dpsf && ok_ipv,
              "C1 契约恒等式 (sdet -0.5 / dpsf 恒等 / ipv +0.5)");
    }

    // ---- [C2] psf→photometry: 生产 pc::WcsTransform (0-based 输入, crpix 1-based) ----
    {
        pc::WcsTransform wcs(kCrval1, kCrval2, kCrpix1, kCrpix2,
                             kCd11, kCd12, kCd21, kCd22, 0,
                             nullptr, nullptr, nullptr, nullptr);
        double worst = 0.0;
        for (size_t i = 0; i < pts.size(); ++i) {
            const double x = pts[i].first, y = pts[i].second;  // index-is-center
            double ra = 0.0, dec = 0.0, rar = 0.0, decr = 0.0;
            wcs.pixelToSky(x, y, ra, dec);
            ref_gnomonic_1based(x + 1.0, y + 1.0, &rar, &decr);  // 单次桥接 = FITS 1-based
            const double s = sep_deg(ra, dec, rar, decr);
            if (s > worst) worst = s;
            if (i == 0) {
                char key[64];
                std::snprintf(key, sizeof(key), "photometry_pixelToSky_pt0");
                export_rec(key, ra, dec);
            }
        }
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "C2 psf→photometry vs 独立参考解(xp=x+1): worst=%.3e deg (tol %.0e)",
                      worst, kAngTolDeg);
        check(worst <= kAngTolDeg, buf);
    }

    // ---- [C3] psf→snr: 生产 snr_extract_model_v3 控制点 ----
    {
        double psf[9] = {0.0, 100.0, 5000.0, pts[0].first, pts[0].second,
                         3.0, 900.0, 12.0, 0.1};
        SnrWcsParams w{};
        w.crval1 = kCrval1; w.crval2 = kCrval2;
        w.crpix1 = kCrpix1; w.crpix2 = kCrpix2;
        w.cd[0] = kCd11; w.cd[1] = kCd12; w.cd[2] = kCd21; w.cd[3] = kCd22;
        w.sip.a_order = 0; w.sip.b_order = 0;
        int64_t sid = 1; uint32_t qf = 1u, ps = 0u;
        SnrModelV3 m{};
        const int rc = snr_extract_model_v3(psf, 1, 0.1, &w, 1, &sid, &qf, &ps, &m);
        bool ok = (rc == 0 && m.n_points == 1);
        double worst = 1e30;
        if (ok) {
            const auto* p = static_cast<const SnrControlPointF64V3*>(m.points);
            double rar = 0.0, decr = 0.0;
            ref_gnomonic_1based(pts[0].first + 1.0, pts[0].second + 1.0, &rar, &decr);
            worst = sep_deg(p[0].ra, p[0].dec, rar, decr);
            ok = (worst <= kAngTolDeg);
            export_rec("snr_control_point_pt0", p[0].ra, p[0].dec);
        }
        snr_free_model_v3(&m);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "C3 psf→snr 控制点 vs 独立参考解: rc=%d worst=%.3e deg (tol %.0e)",
                      rc, worst, kAngTolDeg);
        check(ok, buf);
    }

    // ---- [C4]/[C5] 导出链: WcsTan(1-based 契约) 必须经单次 +1 桥接 ----
    {
        astrocs::phase1::WcsTan wcs;
        wcs.crpix1 = kCrpix1; wcs.crpix2 = kCrpix2;
        wcs.crval1 = kCrval1; wcs.crval2 = kCrval2;
        wcs.cd11 = kCd11; wcs.cd12 = kCd12; wcs.cd21 = kCd21; wcs.cd22 = kCd22;
        double worst_bridged = 0.0;
        double min_origin_sep_px = 1e30;
        for (const auto& [x, y] : pts) {
            double rab = 0.0, decb = 0.0, rau = 0.0, decu = 0.0;
            double rar = 0.0, decr = 0.0;
            if (g_negative_injection) {
                // 负例注入 = 修复前形态: 0-based 下标直喂 1-based 契约
                wcs.pix2sky(x, y, &rab, &decb);
            } else {
                wcs.pix2sky(x + 1.0, y + 1.0, &rab, &decb);
            }
            wcs.pix2sky(x, y, &rau, &decu);           // 未桥接读法 (永远算, 供鉴别力判据)
            ref_gnomonic_1based(x + 1.0, y + 1.0, &rar, &decr);
            const double sb = sep_deg(rab, decb, rar, decr);
            if (sb > worst_bridged) worst_bridged = sb;
            const double sep_px = sep_deg(rab, decb, rau, decu) / kPxScaleDeg;
            if (sep_px < min_origin_sep_px) min_origin_sep_px = sep_px;
        }
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "C4 导出链 WcsTan 单次 +1 桥接 vs 参考解: worst=%.3e deg (tol %.0e)",
                          worst_bridged, kAngTolDeg);
            check(worst_bridged <= kAngTolDeg, buf);
        }
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "C5 原点鉴别力 (未桥接 vs 桥接): min_sep=%.6f px (need >= %.1f px)",
                          min_origin_sep_px, kMinOriginSepPx);
            check(min_origin_sep_px >= kMinOriginSepPx, buf);
        }
        // 导出首个点 (单次桥接读法) 供 astropy 第三方判定
        {
            double ra = 0.0, dec = 0.0;
            wcs.pix2sky(pts[0].first + 1.0, pts[0].second + 1.0, &ra, &dec);
            export_rec("wcs_tan_bridged_pt0", ra, dec);
            double rau = 0.0, decu = 0.0;
            wcs.pix2sky(pts[0].first, pts[0].second, &rau, &decu);
            export_rec("wcs_tan_unbridged_pt0", rau, decu);
        }
    }

    if (!export_path.empty()) {
        FILE* f = std::fopen(export_path.c_str(), "wb");
        if (f == nullptr) {
            std::printf("  [FAIL] export 无法写入: %s (fail-closed)\n", export_path.c_str());
            ++g_failures;
            ++g_checks;
        } else {
            for (const auto& l : g_export_lines) std::fprintf(f, "%s\n", l.c_str());
            std::fclose(f);
            std::printf("  [INFO] export -> %s (%zu 行)\n", export_path.c_str(),
                        g_export_lines.size());
        }
    }

    std::printf("checks=%d failures=%d\n", g_checks, g_failures);
    if (g_negative_injection) {
        if (g_failures > 0) {
            std::printf("NEGATIVE INJECTION DETECTED (failures=%d)\n", g_failures);
            return 0;   // 注入被检出 = 本负例测试通过
        }
        std::printf("NEGATIVE INJECTION NOT DETECTED — 门已失效\n");
        return 1;
    }
    return (g_failures == 0) ? 0 : 1;
}
