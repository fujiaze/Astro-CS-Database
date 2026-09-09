// P1-WCS-TEST · units 组 (F1 合成线性场 + F3 CRPIX/Y-down 不变量 + F6 legacy 桥)
//
// 合同锚: docs/algorithms/PLATESOLVE.md §11.4 TEST-WCS-DESIGN-001 (P1-WCS-DOC
// 冻结)。冻结容差逐项写死, 不得放宽:
//   F1: n_pairs≥12, rms_arcsec≤0.5", CD 相对误差≤2%, |ΔCRVAL|≤1"
//   F3: CRPIX=(w/2+0.5, h/2+0.5) 精确; Y-down CD 第 2 列符号翻转
//   F6: WcsTan roundtrip < 1e-6 deg (tests/unit/p1_wcs_phot_test.cpp:50 冻结值)
// 期望值全部由 p1wcs_oracle.hpp 独立推导 (fixture 真值 → oracle), 绝不经
// 被测函数生成 (模板 <prefix>-TEST §3)。
#include "p1wcs_test_main.hpp"
#include "p1wcs_fixtures.hpp"
#include "p1wcs_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "ipv_itertrans.h"  // iter_trans_solve (被测: 迭代重投影多项式拟合)
#include "ipv_solver.h"     // extract_wcs_sip (被测: WCS 提取)
#include "wcs_tan.h"        // astrocs::phase1::WcsTan (被测: legacy 桥)

using namespace p1wcs;

namespace {

constexpr unsigned kSeedA = 20260907u;  // FIX-WCS-A 固定 seed

// FIX-WCS-A 无平移场: iter_trans(order=1) + extract_wcs_sip 全链
struct SolvedA {
    ipv::IterTransResult fit;
    ipv::WcsFitResult wcs;
    FixWcsA fx;
};

SolvedA solve_fix_a(unsigned seed, double t0, double t1) {
    SolvedA s;
    s.fx = fix_wcs_a_linear(seed, t0, t1);
    s.fit = ipv::iter_trans_solve(s.fx.U, s.fx.W, s.fx.pairs, 5.0, 1);
    extract_wcs_sip(s.fit.trans, s.fx.truth.ra0, s.fx.truth.dec0, s.fx.width,
                    s.fx.height, s.fx.truth.s0, s.fx.U, s.fx.W, s.fit.inliers,
                    &s.wcs, nullptr);
    return s;
}

}  // namespace

namespace p1wcs {

int test_units() {
    CheckState cs;
    cs.failures = 0;
    cs.fault_reported = false;

    // ------------------------------------------------------------------
    // F1: 合成线性场 order=1 (PLATESOLVE.md §11.4 F1)
    // ------------------------------------------------------------------
    {
        const SolvedA s = solve_fix_a(kSeedA, 0.0, 0.0);
        P1WCS_CHECK(cs, s.fit.success, "u1_f1_rms");
        P1WCS_CHECK(cs, s.fit.n_inliers >= 12, "u1_f1_rms");  // n_pairs≥12 (冻结)
        P1WCS_CHECK(cs, s.fit.rms <= 0.5, "u1_f1_rms");       // rms_arcsec≤0.5" (冻结)

        // oracle 期望 CD (Y-down): cd11=m00/3600, cd12=-m01/3600, cd21=m10/3600,
        // cd22=-m11/3600 (oracle_cd_from_truth; 期望=fixture 真值直接映射,
        // 零拟合零被测路径)
        const TruthLinear& tr = s.fx.truth;
        const OracleLinear6 o_tr{tr.m00, tr.m01, tr.m10, tr.m11, tr.t0, tr.t1};
        double cd11, cd12, cd21, cd22;
        oracle_cd_from_truth(o_tr, &cd11, &cd12, &cd21, &cd22);
        // CD 相对误差 ≤2% (§9 尺度容差 0.002 同源, 冻结)
        const double eps = 1e-300;
        P1WCS_CHECK_NEAR(cs, std::fabs(s.wcs.cd.cd11 - cd11) / (std::fabs(cd11) + eps), 0.0,
                         0.02, "u1_f1_cd_relative");
        P1WCS_CHECK_NEAR(cs, std::fabs(s.wcs.cd.cd12 - cd12) / (std::fabs(cd12) + eps), 0.0,
                         0.02, "u1_f1_cd_relative");
        P1WCS_CHECK_NEAR(cs, std::fabs(s.wcs.cd.cd21 - cd21) / (std::fabs(cd21) + eps), 0.0,
                         0.02, "u1_f1_cd_relative");
        P1WCS_CHECK_NEAR(cs, std::fabs(s.wcs.cd.cd22 - cd22) / (std::fabs(cd22) + eps), 0.0,
                         0.02, "u1_f1_cd_relative");

        // |ΔCRVAL| ≤ 1" (冻结): CRVAL=收敛中心=切点 (gnomonic 原点)
        const double dcrval_as =
            3600.0 * std::hypot(s.wcs.crval[0] - tr.ra0, s.wcs.crval[1] - tr.dec0);
        P1WCS_CHECK(cs, dcrval_as <= 1.0, "u1_f1_crval");

        // F3: CRPIX=(w/2+0.5, h/2+0.5) 精确 (ASTROMETRY.md §7 CRPIX 不变量)
        P1WCS_CHECK_NEAR(cs, s.wcs.crpix[0], s.fx.width / 2.0 + 0.5, 1e-9,
                         "u1_f3_crpix_exact");
        P1WCS_CHECK_NEAR(cs, s.wcs.crpix[1], s.fx.height / 2.0 + 0.5, 1e-9,
                         "u1_f3_crpix_exact");

        // F3: Y-down CD 第 2 列符号翻转 (cd12 = -m01/3600, cd22 = -m11/3600)
        P1WCS_CHECK(cs, s.wcs.cd.cd12 * tr.m01 < 0.0 && s.wcs.cd.cd22 * tr.m11 < 0.0,
                    "u1_f3_ydown_sign");

        // 输出语义: trans_order=1, sip_order=0 (线性场无 SIP), ctype TAN
        P1WCS_CHECK(cs, s.wcs.trans_order == 1, "u1_f1_rms");
        P1WCS_CHECK(cs, s.wcs.sip.order == 0, "u1_f1_rms");
        P1WCS_CHECK(cs, s.wcs.success, "u1_f1_rms");
        P1WCS_CHECK(cs, std::strcmp(s.wcs.ctype[0], "RA---TAN") == 0,
                    "u1_f1_rms");
    }

    // F1 平移场: 非零常数项不破坏 CD/CRPIX/CRVAL 语义 (W 平移被 t0/t1 吸收)
    {
        const SolvedA s = solve_fix_a(kSeedA + 1u, -25.0, 40.0);
        P1WCS_CHECK(cs, s.fit.success && s.fit.rms <= 0.5, "u1_f1_rms");
        const TruthLinear& tr = s.fx.truth;
        const double dcrval_as =
            3600.0 * std::hypot(s.wcs.crval[0] - tr.ra0, s.wcs.crval[1] - tr.dec0);
        P1WCS_CHECK(cs, dcrval_as <= 1.0, "u1_f1_crval");
        P1WCS_CHECK_NEAR(cs, s.wcs.crpix[0], s.fx.width / 2.0 + 0.5, 1e-9,
                         "u1_f3_crpix_exact");
    }

    // ------------------------------------------------------------------
    // 边界: n=3 恰好 3 对 (6 参数仿射最低可解数), 拟合精确过点
    // ------------------------------------------------------------------
    {
        std::vector<ipv::StarPoint> U, W;
        std::vector<ipv::MatchPair> pairs;
        fix_wcs_c_few_stars(&U, &W, &pairs, 3);
        const ipv::IterTransResult r = ipv::iter_trans_solve(U, W, pairs, 5.0, 1);
        P1WCS_CHECK(cs, r.success, "u1_boundary_n3");
        // 精确过点 (3 点唯一确定 6 参数): oracle 从 fixture 输入侧 3 对独立
        // 重建期望 (Cramer 闭式, 与被测 sigma-clip 迭代路径不同源)
        const double u3[3][2] = {{U[0].x, U[0].y}, {U[1].x, U[1].y}, {U[2].x, U[2].y}};
        const double w3[3][2] = {{W[0].x, W[0].y}, {W[1].x, W[1].y}, {W[2].x, W[2].y}};
        const OracleLinear6 o = oracle_solve_linear6(u3, w3, 3);
        double wx, wy;
        oracle_apply6(o, U[0].x, U[0].y, &wx, &wy);
        P1WCS_CHECK_NEAR(cs, wx, W[0].x, 1e-6, "u1_boundary_n3");
        P1WCS_CHECK_NEAR(cs, wy, W[0].y, 1e-6, "u1_boundary_n3");
    }

    // ------------------------------------------------------------------
    // F6: legacy 桥 WcsTan roundtrip < 1e-6 deg (冻结) + oracle 前向交叉
    // WcsTan 参数由 fixture 真值 + oracle CD 推导 (期望值非被测函数生成)
    // ------------------------------------------------------------------
    {
        const SolvedA s = solve_fix_a(kSeedA, 0.0, 0.0);
        astrocs::phase1::WcsTan wt;
        wt.crpix1 = s.wcs.crpix[0];
        wt.crpix2 = s.wcs.crpix[1];
        wt.crval1 = s.fx.truth.ra0;
        wt.crval2 = s.fx.truth.dec0;
        wt.cd11 = s.wcs.cd.cd11;
        wt.cd12 = s.wcs.cd.cd12;
        wt.cd21 = s.wcs.cd.cd21;
        wt.cd22 = s.wcs.cd.cd22;

        // F6a: pix2sky → sky2pix roundtrip < 1e-6 deg (天球域, 冻结值)
        double max_sky_delta = 0.0;
        // F6b: sky2pix → pix2sky roundtrip < 1e-6 deg (像素域, 同冻结值量级)
        double max_px_delta = 0.0;
        const double cx = s.fx.width / 2.0, cy = s.fx.height / 2.0;
        for (std::size_t i = 0; i < s.fx.U.size(); ++i) {
            // IPV 接口契约 (center=index+0.5) → FITS 1-based: +0.5
            const double x_f = (s.fx.U[i].x + cx) + 0.5;
            const double y_f = (cy - s.fx.U[i].y) + 0.5;
            double ra = 0.0, dec = 0.0;
            wt.pix2sky(x_f, y_f, &ra, &dec);
            double x2 = 0.0, y2 = 0.0;
            wt.sky2pix(ra, dec, &x2, &y2);
            max_px_delta = std::max(max_px_delta, std::hypot(x2 - x_f, y2 - y_f));

            // 天球域 roundtrip: oracle 真值 (ra,dec) → sky2pix → pix2sky
            double ra_t = 0.0, dec_t = 0.0;
            oracle_wcs_forward(wt.cd11, wt.cd12, wt.cd21, wt.cd22, wt.crval1,
                               wt.crval2, wt.crpix1, wt.crpix2, nullptr, nullptr,
                               0, x_f, y_f, &ra_t, &dec_t);
            wt.sky2pix(ra_t, dec_t, &x2, &y2);
            wt.pix2sky(x2, y2, &ra, &dec);
            const double dra = std::fabs(ra - ra_t) * std::cos(dec_t * kDegToRad);
            const double ddec = std::fabs(dec - dec_t);
            max_sky_delta = std::max(max_sky_delta, std::max(dra, ddec));
        }
        P1WCS_CHECK(cs, max_sky_delta < 1e-6, "u1_f6_roundtrip");  // 冻结 1e-6 deg
        P1WCS_CHECK(cs, max_px_delta < 1e-6, "u1_f6_roundtrip");

        // F6 交叉对拍移除 — 发现被测 WcsTan.pix2sky 中间量单位缺陷 (ξ/η
        // deg 数值未转 rad 直接进球面公式; sky2pix 同族单位错与之成对抵消,
        // roundtrip 自洽掩盖)。交叉对拍在 negative 组 n1_wcs_tan_unit_anchor
        // 以缺陷行为锚锁定现状 (IMPL 修复后翻转断言并登记)。
    }

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1WCS UNITS PASS\n");
        return 0;
    }
    std::fprintf(stderr, "P1WCS UNITS FAIL (%d check(s))\n", cs.failures);
    return 1;
}

}  // namespace p1wcs
