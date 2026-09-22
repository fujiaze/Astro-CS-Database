// eng/tests/unit/v6_p3_proj/v6_p3_proj_probe.cpp — IMPL-P3-PROJ-001 独立 WCS Oracle
// 的被测面探针：输出被测实现（lib/algorithms/projection/p3_proj_v6.cpp）的
// pix2world / 逐像素 Ω / R-S 归一 CSV，供 eng/tests/unit/v6_p3_proj/
// p3_proj_wcs_oracle.py（astropy/WCSLIB 独立实现）对拍。
// 探针本身不做任何判定；判定在独立 oracle 侧。
#include "p3_proj_v6.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "p3_wcs.h"   // 只读合同值（p3_wcs_applicability）; 不做任何投影计算

using astrocs::phase3proj::v6::Descriptor;
using astrocs::phase3proj::v6::Plan;
using astrocs::phase3proj::v6::ProjectionId;
using astrocs::phase3proj::v6::ProjStatus;

namespace {

const char* id_code(ProjectionId id) {
    switch (id) {
        case ProjectionId::kTAN: return "TAN";
        case ProjectionId::kSIN: return "SIN";
        case ProjectionId::kCAR: return "CAR";
        default: return "AIT";
    }
}

ProjectionId parse_id(const char* s) {
    if (std::strcmp(s, "TAN") == 0) return ProjectionId::kTAN;
    if (std::strcmp(s, "SIN") == 0) return ProjectionId::kSIN;
    if (std::strcmp(s, "CAR") == 0) return ProjectionId::kCAR;
    return ProjectionId::kAIT;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 8) {
        std::fprintf(stderr,
                     "usage: %s PROJ RA0 DEC0 SCALE W H [grid_n] [parity] [pa]\n",
                     argv[0]);
        return 2;
    }
    const ProjectionId id = parse_id(argv[1]);
    const double ra0 = std::atof(argv[2]);
    const double dec0 = std::atof(argv[3]);
    const double scale = std::atof(argv[4]);
    const int w = std::atoi(argv[5]);
    const int h = std::atoi(argv[6]);
    const int gn = (argc > 7) ? std::atoi(argv[7]) : 5;
    const char* parity = (argc > 8) ? argv[8] : "east_left";
    const double pa = (argc > 9) ? std::atof(argv[9]) : 0.0;

    // 往返容差单一事实源（GATES §3 G-P1-WCS-BRIDGE / -GLOBAL; GATE-WCS-01 裁决 7）:
    // 判定在独立 oracle 侧, 本探针只如实打印生产注册表声明的合同值。
    {
        const astrocs::phase3::P3WcsApplicability* ap =
            astrocs::phase3::p3_wcs_applicability("TAN");
        if (ap == nullptr) {
            std::fprintf(stderr, "no TAN applicability declaration\n");
            return 4;
        }
        std::printf("CONTRACT tight=%.17g global=%.17g min_scale=%.17g c_env=%.17g\n",
                    ap->roundtrip_tol_px, ap->roundtrip_tol_global_px,
                    ap->min_scale_arcsec, ap->envelope_c_env);
    }

    Descriptor d;
    const ProjStatus mk =
        astrocs::phase3proj::v6::make(id, ra0, dec0, scale, w, h, parity, pa, &d);
    std::printf(
        "V6PROBE proj=%s status_make=%d crval1=%.17g crval2=%.17g scale=%.17g "
        "w=%d h=%d crpix1=%.17g crpix2=%.17g cd11=%.17g cd12=%.17g cd21=%.17g "
        "cd22=%.17g parity=%s pa=%.17g\n",
        id_code(id), (int)mk, ra0, dec0, scale, w, h, d.crpix_x, d.crpix_y,
        d.cd[0][0], d.cd[0][1], d.cd[1][0], d.cd[1][1], parity, pa);
    if (mk != ProjStatus::kOk) {
        std::printf("END\n");
        return 0;
    }

    // CRPIX↔CRVAL 定义性不变量（Paper I §2.1.1）：CRPIX 处 world 必须 == CRVAL
    {
        double ra_c = 0, dec_c = 0;
        const ProjStatus stc = astrocs::phase3proj::v6::pix2world(
            &d, d.crpix_x - 1.0, d.crpix_y - 1.0, &ra_c, &dec_c);
        std::printf("CRPIXW x=%.17g y=%.17g status=%d ra=%.17g dec=%.17g\n",
                    d.crpix_x - 1.0, d.crpix_y - 1.0, (int)stc, ra_c, dec_c);
    }

    // 逐像素 Ω 全网格统计 + 交叉（盈余 vs 微分）
    std::vector<double> omega(static_cast<size_t>(w) * h, 0.0);
    const ProjStatus gst = astrocs::phase3proj::v6::solid_angle_grid(&d, omega.data(), nullptr);
    double omin = 0, omax = 0;
    int nok = 0;
    for (double v : omega) {
        if (!std::isfinite(v)) continue;
        if (nok == 0) { omin = omax = v; }
        omin = std::fmin(omin, v);
        omax = std::fmax(omax, v);
        ++nok;
    }
    double diff_rel_max = 0.0;
    int ndiff = 0;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            double a = 0, b = 0;
            if (astrocs::phase3proj::v6::pixel_solid_angle(&d, i, j, &a) != ProjStatus::kOk)
                continue;
            if (astrocs::phase3proj::v6::pixel_solid_angle_differential(&d, i, j, &b) !=
                ProjStatus::kOk)
                continue;
            const double rel = std::fabs(a - b) / a;
            diff_rel_max = std::fmax(diff_rel_max, rel);
            ++ndiff;
        }
    }
    std::printf("OMEGA_STAT status_grid=%d n_ok=%d n_diff=%d min=%.17g max=%.17g "
                "ratio=%.17g diff_rel_max=%.17g\n",
                (int)gst, nok, ndiff, omin, omax, (omin > 0 ? omax / omin : 0.0),
                diff_rel_max);

    // 采样网格：ra/dec + omega（供 astropy 对拍）
    for (int jj = 0; jj < gn; ++jj) {
        const double y = (h - 1) * (gn == 1 ? 0.0 : double(jj) / (gn - 1));
        for (int ii = 0; ii < gn; ++ii) {
            const double x = (w - 1) * (gn == 1 ? 0.0 : double(ii) / (gn - 1));
            double ra = 0, dec = 0, a = 0, b = 0;
            const ProjStatus st = astrocs::phase3proj::v6::pix2world(&d, x, y, &ra, &dec);
            const ProjStatus sa = astrocs::phase3proj::v6::pixel_solid_angle(&d, x, y, &a);
            const ProjStatus sb = astrocs::phase3proj::v6::pixel_solid_angle_differential(&d, x, y, &b);
            std::printf("ROW x=%.17g y=%.17g status=%d ra=%.17g dec=%.17g "
                        "omega_excess=%.17g status_omega=%d omega_diff=%.17g "
                        "status_diff=%d\n",
                        x, y, (int)st, ra, dec, a, (int)sa, b, (int)sb);
        }
    }

    // 往返检查样本（world2pix∘pix2world）
    for (int jj = 0; jj < gn; ++jj) {
        const double y = (h - 1) * (gn == 1 ? 0.0 : double(jj) / (gn - 1));
        for (int ii = 0; ii < gn; ++ii) {
            const double x = (w - 1) * (gn == 1 ? 0.0 : double(ii) / (gn - 1));
            double ra = 0, dec = 0, xb = 0, yb = 0;
            const ProjStatus st = astrocs::phase3proj::v6::pix2world(&d, x, y, &ra, &dec);
            if (st != ProjStatus::kOk) continue;
            const ProjStatus st2 = astrocs::phase3proj::v6::world2pix(&d, ra, dec, &xb, &yb);
            std::printf("RT x=%.17g y=%.17g status=%d xb=%.17g yb=%.17g err=%.17g\n",
                        x, y, (int)st2, xb, yb,
                        std::hypot(xb - x, yb - y));
        }
    }

    // 计划摘要
    Plan plan;
    const ProjStatus pst = astrocs::phase3proj::v6::plan(id, ra0, dec0, scale, w, h,
                                                         parity, pa, &plan);
    std::printf("PLAN status=%d domain_valid=%d singularity_free=%d wrap=%d "
                "dec_min=%.17g dec_max=%.17g pole_margin=%.17g fov_x=%.17g "
                "min_margin=%.17g omega_ratio=%.17g\n",
                (int)pst, (int)plan.domain_valid, (int)plan.singularity_free,
                (int)plan.crosses_ra_wrap, plan.dec_min_deg, plan.dec_max_deg,
                plan.pole_margin_deg, plan.fov_x_deg, plan.min_domain_margin,
                plan.omega_max_min_ratio);

    // R/S 归一二元语义构造性检查（1D 重叠 A_ij=|Ω_j∩Ω'_i|）
    {
        const int m = 3, n = 5;
        const double om_out[3] = {2.0, 2.5, 1.5};
        const double om_in[5] = {1.0, 0.5, 1.5, 1.5, 1.5};
        double a[15], r[15], s[15], s2[15], rs[3], cs[5];
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < n; ++j) a[i * n + j] = 0.0;
        // 行 i 的 A 行和 = Ω'_i；列 j 的 A 列和 = Ω_j（构造性 partition）
        a[0] = 1.0;  a[1] = 0.4;  a[2] = 0.6;  a[3] = 0.0; a[4] = 0.0;
        a[5] = 0.0;  a[6] = 0.1;  a[7] = 0.9;  a[8] = 1.5; a[9] = 0.0;
        a[10] = 0.0; a[11] = 0.0; a[12] = 0.0;  a[13] = 0.0; a[14] = 1.5;
        const ProjStatus nr = astrocs::phase3proj::v6::row_normalise(a, m, n, om_out, r);
        const ProjStatus nc = astrocs::phase3proj::v6::col_normalise(a, m, n, om_in, s);
        const ProjStatus n2 = astrocs::phase3proj::v6::row_to_col(r, m, n, om_out, om_in, s2);
        astrocs::phase3proj::v6::matrix_row_sums(r, m, n, rs);
        astrocs::phase3proj::v6::matrix_col_sums(s, m, n, cs);
        std::printf("NORM m=%d n=%d status_row=%d status_col=%d status_rtc=%d\n", m, n,
                    (int)nr, (int)nc, (int)n2);
        for (int i = 0; i < m; ++i) std::printf("NORM_ROWSUM i=%d v=%.17g\n", i, rs[i]);
        for (int j = 0; j < n; ++j) std::printf("NORM_COLSUM j=%d v=%.17g\n", j, cs[j]);
        for (int i = 0; i < m; ++i)
            for (int j = 0; j < n; ++j)
                std::printf("NORM_S i=%d j=%d v=%.17g s2=%.17g\n", i, j,
                            s[i * n + j], s2[i * n + j]);
    }

    std::printf("END\n");
    return 0;
}
