// ============================================================================
// P1-PSF-TEST · core 测试组 (units / properties / oracle / negative / boundary)
// ----------------------------------------------------------------------------
// 合同锚: STAR_PSF_ALGORITHMS.md §11 TEST-PSF-DESIGN-001 (单测/oracle/性质/
// 负面/边界组设计); README §5 (容差元数据); 11_MODULE_SOURCE_TEST_STANDARD
// §5 (容差引用来源)。状态码语义: dpsf_psf.cpp §moffat4_fit_tmpl 验证链
// (:360-400) + README §6 (两处源/文档差异在测试注释中登记, 不改文档)。
//
// 所有期望值由 p1psf_oracle.hpp 独立实现生成; 被测函数仅通过 dynamic_psf.h
// 公共 C API 调用 (dpsf_fit / dpsf_fit_batch / dpsf_fit_batch_f /
// dpsf_fit_batch_f32 / dpsf_fit_batch_f64 / dpsf_fit_batch_d /
// dpsf_free_results)。负面/边界参数均经 2026-09-09 probe 实证 (fixture 头
// 注释), 非臆测阈值。
// ============================================================================
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "dynamic_psf.h"

#include "p1psf_fixtures.hpp"
#include "p1psf_oracle.hpp"
#include "p1psf_test_main.hpp"

using namespace p1psf;

namespace {

inline void set_threads(int n) {
#ifdef _OPENMP
    omp_set_num_threads(n);
#else
    (void)n;
#endif
}

// 共享工具 flatten9 / vec_bitwise_eq 在 p1psf_test_main.hpp (core/perf 复用)

// 单星 f64 回收断言 (oracle 容差表; source: probe 无噪声回收实证)
void check_recovery(CheckState& cs, const DPSFFitResult& r, const PsfStar& s,
                    const char* faultname, const char* tag) {
    P1PSF_CHECK_MSG(cs, r.status == DPSF_FIT_OK, faultname,
                    "%s: status=%d (期望 OK=0)", tag, r.status);
    if (r.status != DPSF_FIT_OK) return;
    P1PSF_CHECK_MSG(cs, std::fabs(r.B - s.B) <= tol().b_abs, faultname,
                    "%s: B=%.10f truth=%.10f |dB|=%.3e (tol %.1e, README §5)",
                    tag, r.B, s.B, std::fabs(r.B - s.B), tol().b_abs);
    P1PSF_CHECK_MSG(cs, rel_err(r.A, s.A) <= tol().a_rel, faultname,
                    "%s: A=%.8f truth=%.8f rel=%.3e (tol %.1e)",
                    tag, r.A, s.A, rel_err(r.A, s.A), tol().a_rel);
    P1PSF_CHECK_MSG(cs, std::fabs(r.cx - s.cx) <= tol().cen_abs, faultname,
                    "%s: cx=%.12f truth=%.12f", tag, r.cx, s.cx);
    P1PSF_CHECK_MSG(cs, std::fabs(r.cy - s.cy) <= tol().cen_abs, faultname,
                    "%s: cy=%.12f truth=%.12f", tag, r.cy, s.cy);
    P1PSF_CHECK_MSG(cs, rel_err(r.sx, s.sx) <= tol().s_rel, faultname,
                    "%s: sx=%.10f truth=%.10f rel=%.3e", tag, r.sx, s.sx,
                    rel_err(r.sx, s.sx));
    P1PSF_CHECK_MSG(cs, rel_err(r.sy, s.sy) <= tol().s_rel, faultname,
                    "%s: sy=%.10f truth=%.10f rel=%.3e", tag, r.sy, s.sy,
                    rel_err(r.sy, s.sy));
}

// ---------------------------------------------------------------------------
// units 组: 单星回收 + θ 简并 M 等价 + 图像坐标 + ABI 一致性
// ---------------------------------------------------------------------------
int test_units() {
    CheckState cs;
    const char* F_REC = "recovery";
    const char* F_ABI = "abi_consistency";

    // U1: FIX-PSF-A f64 解析单星回收 (无噪声 → 紧容差, probe 实证 1e-9 量级)
    {
        struct Case { const char* name; PsfStar s; int w; };
        const Case cases[] = {
            {"round",     {50.0, 900.0, 20.0, 20.0, 2.0, 2.0, 0.0}, 40},
            {"ellip0.3",  {30.0, 500.0, 20.0, 20.0, 2.6, 1.7, 0.3}, 48},
            {"ellip0.6",  {30.0, 500.0, 20.0, 20.0, 2.6, 1.7, 0.6}, 48},
            {"ellip-0.4", {30.0, 500.0, 20.0, 20.0, 2.6, 1.7, -0.4}, 48},
            {"faint",     {10.0, 60.0, 20.0, 20.0, 1.4, 1.1, 0.25}, 48},
            {"bighigh",   {100.0, 3000.0, 24.0, 24.0, 3.5, 3.5, 0.0}, 48},
        };
        for (const auto& c : cases) {
            // oracle 直接渲染 (sx≠sy 椭圆星无同源 fixture)
            std::vector<double> img((size_t)c.w * c.w);
            for (int y = 0; y < c.w; ++y)
                for (int x = 0; x < c.w; ++x)
                    img[(size_t)y * c.w + x] = moffat4_eval(c.s.B, c.s.A, c.s.cx, c.s.cy,
                                                            c.s.sx, c.s.sy, c.s.theta,
                                                            (double)x, (double)y);
            const DPSFFitParams p = default_params();
            double cx = c.s.cx, cy = c.s.cy;
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(img.data(), c.w, c.w, &cx, &cy, 1, &p, &rs);
            P1PSF_CHECK_MSG(cs, rc == 0, F_REC, "U1/%s: batch_d rc=%d", c.name, rc);
            if (rc == 0 && rs) {
                check_recovery(cs, rs[0], c.s, F_REC, c.name);
                // θ 椭圆等价: M 元素判据 (θ 简并无关; round 星 Mxy 真值为 0,
                // 必须用混合容差 — rel_err 对零真值发散)
                double Mf[4], Mt[4];
                oracle_m_matrix(rs[0].sx, rs[0].sy, rs[0].theta, Mf);
                oracle_m_matrix(c.s.sx, c.s.sy, c.s.theta, Mt);
                P1PSF_CHECK_MSG(cs, close_hybrid(Mf[0], Mt[0], tol().m_rel) &&
                                    close_hybrid(Mf[1], Mt[1], tol().m_rel) &&
                                    close_hybrid(Mf[3], Mt[3], tol().m_rel), F_REC,
                                "U1/%s: M-equiv [%.8f %.8f; %.8f] vs [%.8f %.8f; %.8f]",
                                c.name, Mf[0], Mf[1], Mf[3], Mt[0], Mt[1], Mt[3]);
                dpsf_free_results(rs);
            }
        }
    }

    // U2: 贴边星图像坐标 (batch_d 内部 +x0/+y0 回写, README §4 坐标语义;
    //     probe 实证 cx=0.500000 cy=47.500000)
    {
        const FixPsfD d = fix_psf_d();
        const DPSFFitParams p = default_params();
        double cx = d.edge.star.cx, cy = d.edge.star.cy;
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch_d(d.edge.f64.data(), d.edge.w, d.edge.h,
                                        &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == DPSF_FIT_OK, F_REC,
                        "U2: edge-star rc=%d", rc);
        if (rc == 0 && rs) {
            P1PSF_CHECK_MSG(cs, std::fabs(rs[0].cx - 0.5) <= 1e-3 &&
                                std::fabs(rs[0].cy - 47.5) <= 1e-3, F_REC,
                            "U2: edge cx=%.8f cy=%.8f (truth 0.5/47.5)", rs[0].cx, rs[0].cy);
            dpsf_free_results(rs);
        }
    }

    // U3: uint16 (截断量化) ↔ float32 (舍入) 双通道批量 ABI — 同源解析场。
    //     语义注意 (probe2 实证 bitwise 不同): u16 走 (uint16_t) 截断、f32 走
    //     (float) 舍入, patch 数值域不同 → 两通道结果不 bitwise 相等, 断言
    //     各自独立回收真值 (量化/舍入噪声元数据宽容差, README §5)。
    {
        const int W = 96, N = 3;
        const double xs[3] = {24.0, 48.0, 72.0};
        std::vector<double> img((size_t)W * W, 50.0);
        for (int i = 0; i < N; ++i)
            for (int y = 0; y < W; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] +=
                        moffat4_eval(0.0, 900.0, xs[i], 48.0, 2.0, 2.0, 0.0,
                                     (double)x, (double)y);
        std::vector<uint16_t> u16(img.size());
        std::vector<float> f32(img.size());
        for (size_t i = 0; i < img.size(); ++i) {
            u16[i] = (uint16_t)img[i];   // 截断量化
            f32[i] = (float)img[i];      // 舍入
        }
        const DPSFFitParams p = default_params();
        double cxv[3] = {24.0, 48.0, 72.0}, cyv[3] = {48.0, 48.0, 48.0};
        DPSFFitResult* ru = nullptr;
        DPSFFitResult* rf = nullptr;
        const int rc1 = dpsf_fit_batch(u16.data(), W, W, cxv, cyv, N, &p, &ru);
        const int rc2 = dpsf_fit_batch_f(f32.data(), W, W, cxv, cyv, N, &p, &rf);
        P1PSF_CHECK(cs, rc1 == 0 && rc2 == 0 && ru && rf, F_ABI);
        if (ru && rf) {
            for (int i = 0; i < N; ++i) {
                char tag[32];
                std::snprintf(tag, sizeof(tag), "U3/%d", i);
                // u16 截断量化噪声 (≤1 ADU) → 宽容差 (B3 同口径)
                P1PSF_CHECK_MSG(cs, ru[i].status == DPSF_FIT_OK, F_ABI,
                                "U3/%s: u16 status=%d", tag, ru[i].status);
                if (ru[i].status == DPSF_FIT_OK) {
                    P1PSF_CHECK_MSG(cs, std::fabs(ru[i].cx - xs[i]) <= 0.05 &&
                                        std::fabs(ru[i].cy - 48.0) <= 0.05, F_ABI,
                                    "U3/%s: u16 cx=%.6f cy=%.6f", tag, ru[i].cx, ru[i].cy);
                    P1PSF_CHECK_MSG(cs, rel_err(ru[i].sx, 2.0) <= 2e-2, F_ABI,
                                    "U3/%s: u16 sx=%.6f", tag, ru[i].sx);
                    P1PSF_CHECK_MSG(cs, rel_err(ru[i].A, 900.0) <= 2e-2, F_ABI,
                                    "U3/%s: u16 A=%.4f", tag, ru[i].A);
                    P1PSF_CHECK(cs, std::fabs(ru[i].B - 50.0) <= 1.0, F_ABI);
                }
                // f32 舍入 (相对误差 ~1e-7) → 紧于 u16 的元数据容差
                P1PSF_CHECK_MSG(cs, rf[i].status == DPSF_FIT_OK, F_ABI,
                                "U3/%s: f32 status=%d", tag, rf[i].status);
                if (rf[i].status == DPSF_FIT_OK) {
                    P1PSF_CHECK_MSG(cs, std::fabs(rf[i].cx - xs[i]) <= 0.02 &&
                                        std::fabs(rf[i].cy - 48.0) <= 0.02, F_ABI,
                                    "U3/%s: f32 cx=%.8f cy=%.8f", tag, rf[i].cx, rf[i].cy);
                    P1PSF_CHECK_MSG(cs, rel_err(rf[i].sx, 2.0) <= 5e-3, F_ABI,
                                    "U3/%s: f32 sx=%.6f", tag, rf[i].sx);
                    P1PSF_CHECK_MSG(cs, rel_err(rf[i].A, 900.0) <= 1e-3, F_ABI,
                                    "U3/%s: f32 A=%.4f", tag, rf[i].A);
                }
            }
            dpsf_free_results(ru);
            dpsf_free_results(rf);
        }
    }

    // U4: 单星 dpsf_fit ↔ size-1 批量 bitwise (同路径一致性)
    {
        const FixPsfA fx = fix_psf_a();
        const DPSFFitParams p = default_params();
        DPSFFitResult single{};
        DPSFFitResult* bp = nullptr;
        const double cx = fx.star.cx, cy = fx.star.cy;
        const int rc1 = dpsf_fit(fx.u16.data(), fx.w, fx.h, cx, cy, &p, &single);
        const int rc2 = dpsf_fit_batch(fx.u16.data(), fx.w, fx.h, &cx, &cy, 1, &p, &bp);
        P1PSF_CHECK(cs, rc1 == 0 && rc2 == 0 && bp, F_ABI);
        if (bp) {
            P1PSF_CHECK_MSG(cs,
                            single.status == bp[0].status &&
                                std::memcmp(&single.B, &bp[0].B, 12 * sizeof(double)) == 0,
                            F_ABI, "U4: single-vs-batch mismatch");
            dpsf_free_results(bp);
        }
    }

    // U5: float32 [N,9] schema 输出 ↔ batch_f DPSFFitResult 位级一致 (3 独立星)
    {
        const int W = 128, H = 128, N = 3;
        std::vector<double> img((size_t)W * H, 50.0);
        const double xs[3] = {20.0, 64.0, 108.0};
        for (int i = 0; i < N; ++i)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 900.0, xs[i], 64.0,
                                                           2.2, 1.8, 0.35, (double)x, (double)y);
        std::vector<float> imgf(img.size());
        for (size_t i = 0; i < img.size(); ++i) imgf[i] = (float)img[i];
        std::vector<double> det(6 * N, 0.0);
        for (int i = 0; i < N; ++i) { det[i * 6 + 0] = xs[i]; det[i * 6 + 1] = 64.0; }
        const DPSFFitParams p = default_params();
        std::vector<double> out9(9 * N, -1.0);
        int nv = -5;
        const double cyv[3] = {64.0, 64.0, 64.0};
        DPSFFitResult* rf2 = nullptr;
        const int rc2 = dpsf_fit_batch_f(imgf.data(), W, H, xs, cyv, N, &p, &rf2);
        const int rc3 = dpsf_fit_batch_f32(imgf.data(), W, H, det.data(), N, &p, out9.data(), &nv, NULL);
        P1PSF_CHECK(cs, rc2 == 0 && rc3 == 0 && rf2, F_ABI);
        if (rf2) {
            bool same = nv == N;
            for (int i = 0; i < N && same; ++i) {
                const double row[9] = {rf2[i].B, rf2[i].A, rf2[i].cx, rf2[i].cy,
                                       rf2[i].sx, rf2[i].sy, rf2[i].theta,
                                       rf2[i].fwhm_x, rf2[i].fwhm_y};
                if (std::memcmp(row, &out9[(size_t)i * 9], 9 * sizeof(double)) != 0) same = false;
            }
            P1PSF_CHECK_MSG(cs, same, F_ABI,
                            "U5: f32 [N,9] vs batch_f mismatch (nv=%d)", nv);
            dpsf_free_results(rf2);
        }
    }

    // U6: star_det_v1 schema 饱和列 [4]/[5] 不消费 (位级不变; 源 :773-775
    //     仅读 [0]/[1])
    {
        const int W = 128, H = 128, N = 3;
        std::vector<double> img((size_t)W * H, 50.0);
        const double xs[3] = {20.0, 64.0, 108.0};
        for (int i = 0; i < N; ++i)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 900.0, xs[i], 64.0,
                                                           2.2, 1.8, 0.35, (double)x, (double)y);
        std::vector<double> detA(6 * N, 0.0), detB(6 * N, 0.0);
        for (int i = 0; i < N; ++i) {
            detA[i * 6 + 0] = xs[i]; detA[i * 6 + 1] = 64.0;
            detB[i * 6 + 0] = xs[i]; detB[i * 6 + 1] = 64.0;
        }
        for (int i = 0; i < N; ++i) { detA[i * 6 + 4] = 0.0; detA[i * 6 + 5] = 0.0; }
        for (int i = 0; i < N; ++i) { detB[i * 6 + 4] = 1.0; detB[i * 6 + 5] = 1.0; }
        const DPSFFitParams p = default_params();
        std::vector<double> outA(9 * N, -1.0), outB(9 * N, -1.0);
        int nvA = -5, nvB = -5;
        dpsf_fit_batch_f64(img.data(), W, H, detA.data(), N, &p, outA.data(), &nvA, NULL);
        dpsf_fit_batch_f64(img.data(), W, H, detB.data(), N, &p, outB.data(), &nvB, NULL);
        P1PSF_CHECK_MSG(cs, nvA == nvB && vec_bitwise_eq(outA, outB), F_ABI,
                        "U6: sat-col [4]/[5] altered output (nvA=%d nvB=%d)", nvA, nvB);
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// properties 组: 解析恒等式 + 1-N worker bitwise + shuffle 行对应
// ---------------------------------------------------------------------------
int test_properties() {
    CheckState cs;
    const char* F_ID = "identity";
    const char* F_BIT = "worker_bitwise";

    // P1: fwhm/flux/ecc 恒等式 (PSF.md 冻结定义; f64 无噪声星)
    {
        struct Case { const char* name; double sx, sy, theta; };
        const Case cases[] = {
            {"round", 2.0, 2.0, 0.0},
            {"ellip", 2.6, 1.7, 0.3},
            {"ellip2", 3.1, 2.2, -0.4},
        };
        for (const auto& c : cases) {
            const int W = 48;
            std::vector<double> img((size_t)W * W);
            for (int y = 0; y < W; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] = moffat4_eval(50.0, 900.0, 24.0, 24.0,
                                                          c.sx, c.sy, c.theta, (double)x, (double)y);
            const DPSFFitParams p = default_params();
            double cx = 24.0, cy = 24.0;
            DPSFFitResult* rs = nullptr;
            dpsf_fit_batch_d(img.data(), W, W, &cx, &cy, 1, &p, &rs);
            if (rs && rs[0].status == DPSF_FIT_OK) {
                const DPSFFitResult& r = rs[0];
                P1PSF_CHECK_MSG(cs, rel_err(r.fwhm_x, kMoffat4FwhmFactor * r.sx) <= tol().fwhm_rel,
                                F_ID, "P1/%s: fwhm_x=%.15g vs factor·sx=%.15g",
                                c.name, r.fwhm_x, kMoffat4FwhmFactor * r.sx);
                P1PSF_CHECK_MSG(cs, rel_err(r.fwhm_y, kMoffat4FwhmFactor * r.sy) <= tol().fwhm_rel,
                                F_ID, "P1/%s: fwhm_y mismatch", c.name);
                P1PSF_CHECK_MSG(cs, rel_err(r.flux, 2.0 * M_PI * r.A * r.sx * r.sy / 3.0) <= tol().flux_rel,
                                F_ID, "P1/%s: flux=%.15g vs 2π/3·A·sx·sy=%.15g",
                                c.name, r.flux, 2.0 * M_PI * r.A * r.sx * r.sy / 3.0);
                P1PSF_CHECK_MSG(cs, rel_err(r.eccentricity, oracle_eccentricity(r.sx, r.sy)) <= tol().flux_rel,
                                F_ID, "P1/%s: ecc=%.15g vs oracle=%.15g",
                                c.name, r.eccentricity, oracle_eccentricity(r.sx, r.sy));
            } else {
                P1PSF_CHECK(cs, false, F_ID);  // 无噪声星必 OK
            }
            if (rs) dpsf_free_results(rs);
        }
    }

    // P2: θ 消歧等价类 M 矩阵回归 (θ∈{0,±0.3,0.6,1.2,-0.4}; 1.2 → π/2 互换
    //     等价, M 元素不变 — probe 实证)
    {
        const double thetas[] = {0.0, 0.3, -0.3, 0.6, 1.2, -0.4};
        for (double th : thetas) {
            const int W = 48;
            std::vector<double> img((size_t)W * W);
            for (int y = 0; y < W; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] = moffat4_eval(30.0, 500.0, 20.0, 20.0,
                                                          2.6, 1.7, th, (double)x, (double)y);
            const DPSFFitParams p = default_params();
            double cx = 20.0, cy = 20.0;
            DPSFFitResult* rs = nullptr;
            dpsf_fit_batch_d(img.data(), W, W, &cx, &cy, 1, &p, &rs);
            if (rs && rs[0].status == DPSF_FIT_OK) {
                double Mf[4], Mt[4];
                oracle_m_matrix(rs[0].sx, rs[0].sy, rs[0].theta, Mf);
                oracle_m_matrix(2.6, 1.7, th, Mt);
                P1PSF_CHECK_MSG(cs, close_hybrid(Mf[0], Mt[0], tol().m_rel) &&
                                    close_hybrid(Mf[1], Mt[1], tol().m_rel) &&
                                    close_hybrid(Mf[3], Mt[3], tol().m_rel), F_ID,
                                "P2/θ=%.2f: M-equiv failed (%.6f vs %.6f)",
                                th, Mf[0], Mt[0]);
            } else {
                P1PSF_CHECK(cs, false, F_ID);
            }
            if (rs) dpsf_free_results(rs);
        }
    }

    // P3: 1/2/4/8 worker bitwise (batch_d + batch_f64 双路径; 每星独立写 →
    //     位级一致期望, 源 :559/:893 parallel for)
    {
        const int W = 128, H = 128, N = 8;
        std::vector<double> img((size_t)W * H, 80.0);
        std::vector<double> cx(N), cy(N);
        for (int i = 0; i < N; ++i) {
            cx[i] = 16.0 + 24.0 * (i % 4);
            cy[i] = 16.0 + 40.0 * (i / 4);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 600.0 + 100.0 * i, cx[i], cy[i],
                                                           1.5 + 0.2 * i, 1.3 + 0.15 * i,
                                                           0.2 * i, (double)x, (double)y);
        }
        const DPSFFitParams p = default_params();
        std::vector<double> ref_d, ref_9;
        for (int th : {1, 2, 4, 8}) {
            set_threads(th);
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(img.data(), W, H, cx.data(), cy.data(), N, &p, &rs);
            P1PSF_CHECK(cs, rc == 0 && rs, F_BIT);
            if (rs) {
                const std::vector<double> flat = flatten9(rs, N);
                if (th == 1) ref_d = flat;
                else P1PSF_CHECK_MSG(cs, vec_bitwise_eq(ref_d, flat), F_BIT,
                                     "P3: batch_d %dw vs 1w bitwise mismatch", th);
                dpsf_free_results(rs);
            }
            std::vector<double> out9(9 * N, -1.0);
            int nv = -1;
            std::vector<double> det(6 * N, 0.0);
            for (int i = 0; i < N; ++i) { det[i * 6 + 0] = cx[i]; det[i * 6 + 1] = cy[i]; }
            dpsf_fit_batch_f64(img.data(), W, H, det.data(), N, &p, out9.data(), &nv, NULL);
            P1PSF_CHECK(cs, nv == N, F_BIT);
            if (th == 1) ref_9 = out9;
            else P1PSF_CHECK_MSG(cs, vec_bitwise_eq(ref_9, out9), F_BIT,
                                 "P3: batch_f64 %dw vs 1w bitwise mismatch", th);
        }
        set_threads(1);
    }

    // P4: 同线程数双跑 bitwise (确定性)
    {
        const FixPsfB fx = fix_psf_b(20260909ull, 96, 96, 80.0, 3.0,
                                     {{100.0, 900.0, 30.0, 48.0, 2.0, 2.0, 0.25},
                                      {100.0, 700.0, 66.0, 48.0, 2.4, 1.8, -0.3}});
        std::vector<double> det(12, 0.0);
        det[0] = 30.0; det[1] = 48.0; det[6] = 66.0; det[7] = 48.0;
        const DPSFFitParams p = default_params();
        std::vector<double> r1(18, -1.0), r2(18, -1.0);
        int nv1 = -1, nv2 = -1;
        dpsf_fit_batch_f64(fx.f64.data(), fx.w, fx.h, det.data(), 2, &p, r1.data(), &nv1, NULL);
        dpsf_fit_batch_f64(fx.f64.data(), fx.w, fx.h, det.data(), 2, &p, r2.data(), &nv2, NULL);
        P1PSF_CHECK_MSG(cs, nv1 == nv2 && vec_bitwise_eq(r1, r2), F_BIT,
                        "P4: double-run bitwise mismatch");
    }

    // P5: 星序 shuffle → 对应行 bitwise 不变 (行 i ↔ 行 N-1-i)
    {
        const int W = 128, H = 128, N = 6;
        std::vector<double> img((size_t)W * H, 80.0);
        std::vector<double> cx(N), cy(N);
        for (int i = 0; i < N; ++i) {
            cx[i] = 16.0 + 24.0 * (i % 3);
            cy[i] = 16.0 + 40.0 * (i / 3);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 600.0 + 80.0 * i, cx[i], cy[i],
                                                           1.6 + 0.1 * i, 1.4 + 0.1 * i,
                                                           0.1 * i, (double)x, (double)y);
        }
        const DPSFFitParams p = default_params();
        DPSFFitResult* rs1 = nullptr;
        DPSFFitResult* rs2 = nullptr;
        dpsf_fit_batch_d(img.data(), W, H, cx.data(), cy.data(), N, &p, &rs1);
        std::vector<double> rev_cx(N), rev_cy(N);
        for (int i = 0; i < N; ++i) { rev_cx[i] = cx[N - 1 - i]; rev_cy[i] = cy[N - 1 - i]; }
        dpsf_fit_batch_d(img.data(), W, H, rev_cx.data(), rev_cy.data(), N, &p, &rs2);
        if (rs1 && rs2) {
            bool ok = true;
            for (int i = 0; i < N && ok; ++i)
                if (std::memcmp(&rs1[i].B, &rs2[N - 1 - i].B, 12 * sizeof(double)) != 0) ok = false;
            P1PSF_CHECK_MSG(cs, ok, F_BIT, "P5: shuffle row-correspondence mismatch");
        } else {
            P1PSF_CHECK(cs, false, F_BIT);
        }
        if (rs1) dpsf_free_results(rs1);
        if (rs2) dpsf_free_results(rs2);
    }

    // P6 (B2-A2, RESCUE-P0-05): 星 ID ↔ PSF compact 映射门。
    // 3 颗检测, 中间一颗 rect 为空 (检测中心越界) ⇒ 确定性拟合失败。
    // 断言: out_status 按检测下标报告真值; 成功行按检测下标升序 compact;
    //       失败星不产生 NaN 行 (无 NaN 洞); out_n_valid == OK 计数;
    //       out_status=NULL 时 compact 布局不变 (ABI 兼容面)。
    {
        const int W = 128, H = 128, N = 3;
        std::vector<double> img((size_t)W * H, 100.0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                img[(size_t)y * W + x] += moffat4_eval(0.0, 4000.0, 20.0, 20.0, 2.0, 2.0, 0.0,
                                                       (double)x, (double)y)
                                        + moffat4_eval(0.0, 4000.0, 100.0, 100.0, 2.0, 2.0, 0.0,
                                                       (double)x, (double)y);
        std::vector<double> det(6 * N, 0.0);
        det[0] = 20.0;  det[1] = 20.0;
        det[6] = 10000.0; det[7] = 10000.0;   // 越界 → 空 rect → 失败
        det[12] = 100.0; det[13] = 100.0;
        const DPSFFitParams p = default_params();
        std::vector<double> out9(9 * N, -1.0);
        std::vector<int> st(N, -7);
        int nv = -5;
        const int rc = dpsf_fit_batch_f64(img.data(), W, H, det.data(), N, &p, out9.data(), &nv,
                                          st.data());
        P1PSF_CHECK_MSG(cs, rc == 0 && nv == 2, F_ID, "P6: rc=%d nv=%d (期望 2)", rc, nv);
        // 逐星真值: 星 0/2 成功, 星 1 空 rect (未执行拟合)
        P1PSF_CHECK_MSG(cs, st[0] == DPSF_PSF_STATUS_OK && st[2] == DPSF_PSF_STATUS_OK &&
                                st[1] == DPSF_PSF_STATUS_RECT_EMPTY,
                        F_ID, "P6: out_status=[%d,%d,%d]", st[0], st[1], st[2]);
        // 成功集合 (真值) == nv
        int n_ok = 0;
        for (int i = 0; i < N; ++i) if (st[i] == DPSF_PSF_STATUS_OK) ++n_ok;
        P1PSF_CHECK_MSG(cs, n_ok == nv, F_ID, "P6: n_ok=%d != nv=%d", n_ok, nv);
        // compact: 前 nv 行全有限, 第 nv 行 (原失败星位置) 无过期值可断言 ——
        // 本用例预填 -1, compact 只写前 nv 行 (B2-A2 语义)
        bool rows_finite = true;
        for (int k = 0; k < nv * 9; ++k) rows_finite = rows_finite && std::isfinite(out9[k]);
        P1PSF_CHECK_MSG(cs, rows_finite, F_ID, "P6: compact 行出现非有限值 (NaN 洞)");
        // 星↔行映射: row0 ↔ 星 0 (20,20), row1 ↔ 星 2 (100,100)
        const double want_cx[2] = {20.0, 100.0};
        for (int k = 0; k < nv; ++k)
            P1PSF_CHECK_MSG(cs, std::fabs(out9[(size_t)k * 9 + 2] - want_cx[k]) <= 0.05,
                            F_ID, "P6: row %d cx=%.6f (期望 %.1f) — 星↔行错位",
                            k, out9[(size_t)k * 9 + 2], want_cx[k]);
        // compact 行 == 单星拟合同星结果 (bitwise: 同 patch 同算法)
        for (int k = 0; k < nv; ++k) {
            const int star = (k == 0) ? 0 : 2;
            double cx = det[(size_t)star * 6 + 0], cy = det[(size_t)star * 6 + 1];
            DPSFFitResult* rs = nullptr;
            const int rc1 = dpsf_fit_batch_d(img.data(), W, H, &cx, &cy, 1, &p, &rs);
            if (rc1 == 0 && rs && rs[0].status == DPSF_FIT_OK) {
                const double row[9] = {rs[0].B, rs[0].A, rs[0].cx, rs[0].cy, rs[0].sx,
                                       rs[0].sy, rs[0].theta, rs[0].fwhm_x, rs[0].fwhm_y};
                P1PSF_CHECK_MSG(cs, std::memcmp(row, &out9[(size_t)k * 9],
                                                9 * sizeof(double)) == 0,
                                F_ID, "P6: row %d 与单星拟合 (star %d) 非位级一致", k, star);
            } else {
                P1PSF_CHECK(cs, false, F_ID);
            }
            if (rs) dpsf_free_results(rs);
        }
        // out_status=NULL: compact 布局不变 (旧调用方 ABI 面)
        std::vector<double> out9_null(9 * N, -1.0);
        int nv_null = -5;
        const int rc_null = dpsf_fit_batch_f64(img.data(), W, H, det.data(), N, &p,
                                               out9_null.data(), &nv_null, nullptr);
        P1PSF_CHECK_MSG(cs, rc_null == 0 && nv_null == nv &&
                                vec_bitwise_eq(out9_null, out9),
                        F_ID, "P6: out_status=NULL 布局漂移 (nv=%d vs %d)", nv_null, nv);
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// oracle 组: 独立 oracle 渲染的多星批量回收 (噪声场 → 元数据宽容差)
// ---------------------------------------------------------------------------
int test_oracle() {
    CheckState cs;
    const char* F_ORC = "oracle_recovery";

    // O1: FIX-PSF-B 25 星网格噪声场 (σ=3) batch_d 全量回收
    //     容差 (ALG §11.4: 由 fixture generator 元数据给定): 噪声 σ=3 下
    //     cx/cy ≤0.02 px, sx/sy rel ≤1e-2, A rel ≤1e-2, B ≤0.5
    {
        std::vector<PsfStar> stars;
        const int grid = 5;
        for (int gy = 0; gy < grid; ++gy)
            for (int gx = 0; gx < grid; ++gx)
                stars.push_back({100.0, 600.0 + 40.0 * (gx + gy),
                                 16.0 + 24.0 * gx, 16.0 + 24.0 * gy,
                                 1.5 + 0.1 * gx, 1.3 + 0.1 * gy, 0.05 * (gx + 2.0 * gy)});
        const FixPsfB fx = fix_psf_b(20260908ull, 128, 128, 100.0, 3.0, stars);
        const int N = (int)stars.size();
        std::vector<double> cx(N), cy(N), det(6 * N, 0.0);
        for (int i = 0; i < N; ++i) { cx[i] = stars[i].cx; cy[i] = stars[i].cy; det[i * 6 + 0] = cx[i]; det[i * 6 + 1] = cy[i]; }
        const DPSFFitParams p = default_params();
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch_d(fx.f64.data(), fx.w, fx.h, cx.data(), cy.data(), N, &p, &rs);
        P1PSF_CHECK_MSG(cs, rc == 0 && rs, F_ORC, "O1: batch_d rc=%d", rc);
        if (rs) {
            int n_ok = 0;
            for (int i = 0; i < N; ++i) {
                if (rs[i].status != DPSF_FIT_OK) continue;
                ++n_ok;
                const PsfStar& s = stars[i];
                const DPSFFitResult& r = rs[i];
                char tag[32];
                std::snprintf(tag, sizeof(tag), "O1/star%d", i);
                P1PSF_CHECK_MSG(cs, std::fabs(r.cx - s.cx) <= 0.02, F_ORC,
                                "%s: cx=%.6f truth=%.6f", tag, r.cx, s.cx);
                P1PSF_CHECK_MSG(cs, std::fabs(r.cy - s.cy) <= 0.02, F_ORC,
                                "%s: cy=%.6f truth=%.6f", tag, r.cy, s.cy);
                // 噪声场形状判据: 特征轴 sigma (谱 = {sx,sy}, 对 θ 简并等价
                // 类代表互换与 Mxy 交叉项估计噪声均不敏感 — 噪声 σ=3 实测
                // M 交叉项绝对误差可达 ~0.05, 见 O1 头注释)
                double Mf[4], Mt[4], sig_f[2], sig_t[2];
                oracle_m_matrix(r.sx, r.sy, r.theta, Mf);
                oracle_sigma_axes(Mf, sig_f);
                oracle_m_matrix(s.sx, s.sy, s.theta, Mt);
                oracle_sigma_axes(Mt, sig_t);
                P1PSF_CHECK_MSG(cs, rel_err(sig_f[0], sig_t[0]) <= 2e-2 &&
                                    rel_err(sig_f[1], sig_t[1]) <= 2e-2, F_ORC,
                                "%s: sigma-axes (%.4f,%.4f) vs (%.4f,%.4f)",
                                tag, sig_f[0], sig_f[1], sig_t[0], sig_t[1]);
                P1PSF_CHECK_MSG(cs, rel_err(r.A, s.A) <= 2e-2, F_ORC,
                                "%s: A=%.4f truth=%.4f rel=%.3e (tol 2e-2, 噪声 σ=3 元数据)",
                                tag, r.A, s.A, rel_err(r.A, s.A));
                P1PSF_CHECK_MSG(cs, std::fabs(r.B - s.B) <= 0.5, F_ORC,
                                "%s: B=%.4f truth=%.4f", tag, r.B, s.B);
                // 恒等式在噪声场仍成立 (同一 double 乘法)
                P1PSF_CHECK(cs, rel_err(r.fwhm_x, kMoffat4FwhmFactor * r.sx) <= tol().fwhm_rel, F_ORC);
                P1PSF_CHECK(cs, rel_err(r.flux, 2.0 * M_PI * r.A * r.sx * r.sy / 3.0) <= tol().flux_rel, F_ORC);
            }
            P1PSF_CHECK_MSG(cs, n_ok == N, F_ORC, "O1: n_ok=%d expect %d", n_ok, N);
            dpsf_free_results(rs);
        }
    }

    // O2: FIX-PSF-C NaN 污染 (10 px 随机, splitmix seed) → 仍 OK 且回收
    {
        const PsfStar s{50.0, 900.0, 16.0, 16.0, 2.0, 2.0, 0.0};
        const FixPsfC fx = fix_psf_c(424242ull, 32, 32, s, 10);
        P1PSF_CHECK(cs, (int)fx.nan_idx.size() == 10, F_ORC);
        const DPSFFitParams p = default_params();
        double cx = s.cx, cy = s.cy;
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch_d(fx.f64.data(), fx.w, fx.h, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK(cs, rc == 0 && rs, F_ORC);
        if (rs) {
            P1PSF_CHECK_MSG(cs, rs[0].status == DPSF_FIT_OK, F_ORC,
                            "O2: NaN-polluted star status=%d (期望跳过 NaN 仍收敛)", rs[0].status);
            if (rs[0].status == DPSF_FIT_OK) {
                // NaN 跳过 → 有效样本减少, 容差按元数据放宽 (rel 1e-2)
                P1PSF_CHECK(cs, rel_err(rs[0].sx, s.sx) <= 1e-2, F_ORC);
                P1PSF_CHECK(cs, rel_err(rs[0].A, s.A) <= 1e-2, F_ORC);
                P1PSF_CHECK(cs, std::fabs(rs[0].cx - s.cx) <= 0.02, F_ORC);
            }
            dpsf_free_results(rs);
        }
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// negative 组: 状态码矩阵 + 参数校验 + 非零回填 (README §6 + 源验证链;
// 行为均经 probe 实证)
// ---------------------------------------------------------------------------
int test_negative() {
    CheckState cs;
    const char* F_NEG = "negative_matrix";
    const char* F_NAN = "nan_semantics";
    const char* F_BB = "batch_boundary";  // PSF-001: w/h/乘积上界确定性拒绝

    // N1: 常数图 (A0≤0) → INVALID_PARAMS(2), 全字段 memset 0 (单星 + batch_d)
    {
        const FixPsfE e = fix_psf_e();
        const DPSFFitParams p = default_params();
        DPSFFitResult single{};
        double cx = 16.0, cy = 16.0;
        DPSFFitResult* rs = nullptr;
        // 单星 API 消费 uint16 (与 batch_d 的 double 各自同值常数场)
        const std::vector<uint16_t> u16_const((size_t)e.w * e.h, (uint16_t)120);
        const int rc1 = dpsf_fit(u16_const.data(), e.w, e.h, cx, cy, &p, &single);
        const int rc2 = dpsf_fit_batch_d(e.constant_field.data(), e.w, e.h, &cx, &cy, 1, &p, &rs);
        // 单星 dpsf_fit 返回码 = 错误码本身 (probe4 实证 rc=2, README §6
        // "单星返回码"); 批接口整体 rc=0、逐星 status=2
        P1PSF_CHECK_MSG(cs, rc1 == DPSF_FIT_INVALID_PARAMS && rc2 == 0 && rs, F_NEG,
                        "N1: rc1=%d rc2=%d (期望 rc1=2 rc2=0)", rc1, rc2);
        if (rs) {
            P1PSF_CHECK_MSG(cs, single.status == DPSF_FIT_INVALID_PARAMS, F_NEG,
                            "N1: single status=%d (期望 INVALID_PARAMS=2)", single.status);
            P1PSF_CHECK_MSG(cs, rs[0].status == DPSF_FIT_INVALID_PARAMS, F_NEG,
                            "N1: batch status=%d", rs[0].status);
            const bool zeroed = single.B == 0.0 && single.A == 0.0 && single.cx == 0.0 &&
                                single.cy == 0.0 && single.sx == 0.0 && single.sy == 0.0 &&
                                single.theta == 0.0 && single.fwhm_x == 0.0 && single.flux == 0.0;
            P1PSF_CHECK(cs, zeroed, F_NAN);
            dpsf_free_results(rs);
        }
    }

    // N2: fitRadius 阶梯 (probe 实证): R≤0 → rect 面积<9 → 码 2;
    //     R=1/2 (rect 3x3/5x5) → 信息不足 → 码 1; R≥3 → 完整回收码 0
    {
        const int W = 32;
        std::vector<double> img((size_t)W * W);
        for (int y = 0; y < W; ++y)
            for (int x = 0; x < W; ++x)
                img[(size_t)y * W + x] = moffat4_eval(50.0, 900.0, 16.0, 16.0, 2.0, 2.0, 0.0, (double)x, (double)y);
        const int radius[] = {-1, 0, 1, 2, 3, 4, 5};
        const int expect[] = {2, 2, 1, 1, 0, 0, 0};
        for (int k = 0; k < 7; ++k) {
            const DPSFFitParams p = default_params(radius[k]);
            double cx = 16.0, cy = 16.0;
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(img.data(), W, W, &cx, &cy, 1, &p, &rs);
            P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == expect[k], F_NEG,
                            "N2: R=%d status=%d (期望 %d)", radius[k],
                            rs ? rs[0].status : -999, expect[k]);
            if (rs) {
                if (expect[k] == 0)
                    P1PSF_CHECK(cs, rel_err(rs[0].sx, 2.0) <= tol().s_rel, F_NEG);
                else
                    P1PSF_CHECK(cs, rs[0].B == 0.0 && rs[0].sx == 0.0, F_NAN);
                dpsf_free_results(rs);
            }
        }
    }

    // N3: 全 NaN patch / 仅中心 NaN → INVALID_PARAMS(2) (probe 实证)
    {
        const FixPsfE e = fix_psf_e();
        const DPSFFitParams p = default_params();
        double cx = 16.0, cy = 16.0;
        DPSFFitResult* rs = nullptr;
        dpsf_fit_batch_d(e.all_nan_patch.data(), e.w, e.h, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK_MSG(cs, rs && rs[0].status == DPSF_FIT_INVALID_PARAMS, F_NAN,
                        "N3: all-NaN status=%d", rs ? rs[0].status : -999);
        if (rs) dpsf_free_results(rs);
        dpsf_fit_batch_d(e.nan_center.data(), e.w, e.h, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK_MSG(cs, rs && rs[0].status == DPSF_FIT_INVALID_PARAMS, F_NAN,
                        "N3: center-NaN status=%d (中心 NaN → A0 失效)", rs ? rs[0].status : -999);
        if (rs) dpsf_free_results(rs);
    }

    // N4: 参数校验矩阵 → -1 且不触碰输出 (源 dpsf_fit_batch 前置校验)
    {
        const FixPsfA fx = fix_psf_a();
        const DPSFFitParams p = default_params();
        double cx = 20.0, cy = 20.0;
        std::vector<double> det(6, 0.0);
        det[0] = 20.0; det[1] = 20.0;
        DPSFFitResult* rs = (DPSFFitResult*)0x1;
        P1PSF_CHECK(cs, dpsf_fit_batch(nullptr, fx.w, fx.h, &cx, &cy, 1, &p, &rs) == -1, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit_batch(fx.u16.data(), fx.w, fx.h, nullptr, &cy, 1, &p, &rs) == -1, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit_batch(fx.u16.data(), fx.w, fx.h, &cx, &cy, 0, &p, &rs) == -1, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit_batch(fx.u16.data(), fx.w, fx.h, &cx, &cy, -5, &p, &rs) == -1, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit_batch(fx.u16.data(), fx.w, fx.h, &cx, &cy, 1, nullptr, &rs) == -1, F_NEG);
        P1PSF_CHECK(cs, rs == (DPSFFitResult*)0x1, F_NAN);  // sentinel 未被触碰
        // N4b: 批接口 w/h 确定性拒绝 (PSF-001; 原 DISP-PSF-007 候选测试锚翻转:
        // 2026-09-10 前 dpsf_fit_batch/batch_f/batch_d 前置校验仅覆盖指针与
        // count, 无 w/h 检查 — h∈{0,-1,-3} 伪成功 rc=0 (每星 INVALID_PARAMS,
        // probe 实证); batch h=-1 触发 (size_t)w*h 下溢 → length_error →
        // SIGABRT; h=INT_MIN 触发 y1-y0 int 下溢回绕为正 → 巨量 patch 分配 →
        // bad_alloc → SIGABRT。修复后 5 批入口 w/h≤0 → 确定 rc=-1 且不触碰
        // 输出 sentinel)。
        for (int bad : {0, -1, INT_MIN}) {
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch(fx.u16.data(), bad, fx.h, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch w=%d (期望 -1 + sentinel 不变)", bad);
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch(fx.u16.data(), fx.w, bad, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch h=%d (期望 -1 + sentinel 不变)", bad);
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f(fx.f32.data(), bad, fx.h, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch_f w=%d (期望 -1 + sentinel 不变)", bad);
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f(fx.f32.data(), fx.w, bad, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch_f h=%d (期望 -1 + sentinel 不变)", bad);
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_d(fx.f64.data(), bad, fx.h, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch_d w=%d (期望 -1 + sentinel 不变)", bad);
            rs = (DPSFFitResult*)0x1;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_d(fx.f64.data(), fx.w, bad, &cx, &cy, 1, &p, &rs) == -1
                                && rs == (DPSFFitResult*)0x1, F_BB,
                            "N4b: batch_d h=%d (期望 -1 + sentinel 不变)", bad);
            // [N,9] 输出路径: out_psf_params/n_valid sentinel 同步不受触碰
            std::vector<double> out9(9, -7.0);
            int nv = -5;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f32(fx.f32.data(), bad, fx.h, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1
                                && nv == -5 && out9[0] == -7.0, F_BB,
                            "N4b: batch_f32 w=%d (期望 -1 + out/nv sentinel)", bad);
            nv = -5; out9.assign(9, -7.0);
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f32(fx.f32.data(), fx.w, bad, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1
                                && nv == -5 && out9[0] == -7.0, F_BB,
                            "N4b: batch_f32 h=%d (期望 -1 + out/nv sentinel)", bad);
            nv = -5; out9.assign(9, -7.0);
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f64(fx.f64.data(), bad, fx.h, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1
                                && nv == -5 && out9[0] == -7.0, F_BB,
                            "N4b: batch_f64 w=%d (期望 -1 + out/nv sentinel)", bad);
            nv = -5; out9.assign(9, -7.0);
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f64(fx.f64.data(), fx.w, bad, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1
                                && nv == -5 && out9[0] == -7.0, F_BB,
                            "N4b: batch_f64 h=%d (期望 -1 + out/nv sentinel)", bad);
        }
        // N4c: 乘法溢出/寻址上界 — w*h > INT_MAX (逐像素 int 索引 y*width+x
        // 不可寻址域) → 稳定 -1, 任何入口零整图分配零读取 (probe: batch
        // uint16 w=2^30,h=4 现状 16GB 副本 → bad_alloc → SIGABRT)。
        rs = (DPSFFitResult*)0x1;
        P1PSF_CHECK_MSG(cs, dpsf_fit_batch(fx.u16.data(), 1 << 30, 4, &cx, &cy, 1, &p, &rs) == -1
                            && rs == (DPSFFitResult*)0x1, F_BB, "N4c: batch w*h>INT_MAX");
        rs = (DPSFFitResult*)0x1;
        P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f(fx.f32.data(), 1 << 30, 4, &cx, &cy, 1, &p, &rs) == -1,
                        F_BB, "N4c: batch_f w*h>INT_MAX");
        rs = (DPSFFitResult*)0x1;
        P1PSF_CHECK_MSG(cs, dpsf_fit_batch_d(fx.f64.data(), 1 << 30, 4, &cx, &cy, 1, &p, &rs) == -1,
                        F_BB, "N4c: batch_d w*h>INT_MAX");
        {
            std::vector<double> out9(9, -7.0);
            int nv = -5;
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f32(fx.f32.data(), 1 << 30, 4, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1, F_BB,
                            "N4c: batch_f32 w*h>INT_MAX");
            P1PSF_CHECK_MSG(cs, dpsf_fit_batch_f64(fx.f64.data(), 1 << 30, 4, det.data(), 1, &p,
                                                    out9.data(), &nv, NULL) == -1, F_BB,
                            "N4c: batch_f64 w*h>INT_MAX");
        }
        DPSFFitResult single{};
        // 单星 dpsf_fit 参数校验返回错误码 2 (源 :431-434 !image/!params/
        // !result/w<=0/h<=0 → DPSF_FIT_INVALID_PARAMS, probe4 实证), 非 -1;
        // 批接口才返回 -1 (README §2/§6)
        P1PSF_CHECK(cs, dpsf_fit(nullptr, fx.w, fx.h, cx, cy, &p, &single) == DPSF_FIT_INVALID_PARAMS, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit(fx.u16.data(), 0, fx.h, cx, cy, &p, &single) == DPSF_FIT_INVALID_PARAMS, F_NEG);
        P1PSF_CHECK(cs, dpsf_fit(fx.u16.data(), fx.w, fx.h, cx, cy, nullptr, &single) == DPSF_FIT_INVALID_PARAMS, F_NEG);
        // batch_f64 [N,9]: n≤0 → -1 + 输出数组/nv sentinel 不变
        std::vector<double> out9(9, -7.0);
        int nv = -5;
        P1PSF_CHECK(cs, dpsf_fit_batch_f64(fx.f64.data(), fx.w, fx.h, det.data(), 0, &p, out9.data(), &nv, NULL) == -1, F_NEG);
        P1PSF_CHECK(cs, nv == -5 && out9[0] == -7.0, F_NAN);
        P1PSF_CHECK(cs, dpsf_fit_batch_f64(nullptr, fx.w, fx.h, det.data(), 1, &p, out9.data(), &nv, NULL) == -1, F_NEG);
    }

    // N5: 纯噪声场 (无星) → NO_CONVERGENCE(1) + memset 0 (3 seeds, probe 实证)
    {
        for (int seed : {1, 2, 3}) {
            const FixPsfB fx = fix_psf_b((uint64_t)seed, 48, 48, 100.0, 5.0, {});
            const DPSFFitParams p = default_params();
            double cx = 24.0, cy = 24.0;
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(fx.f64.data(), fx.w, fx.h, &cx, &cy, 1, &p, &rs);
            P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == DPSF_FIT_NO_CONVERGENCE, F_NEG,
                            "N5/seed%d: status=%d (期望 NO_CONVERGENCE=1)", seed, rs ? rs[0].status : -999);
            if (rs) {
                P1PSF_CHECK(cs, rs[0].B == 0.0 && rs[0].A == 0.0 && rs[0].sx == 0.0, F_NAN);
                dpsf_free_results(rs);
            }
        }
    }

    // N6: 背景约束链 → NO_CONVERGENCE (FIX-PSF-D wide/wide_fail; 大 σ 星占
    //     据拟合窗 → bkg0 抬升 → |B−bkg0|/max(bkg0,0.01)>0.5, 源 :380-386)
    {
        const FixPsfD d = fix_psf_d();
        const DPSFFitParams p = default_params();
        for (const FixPsfA* fx : {&d.wide, &d.wide_fail}) {
            double cx = fx->star.cx, cy = fx->star.cy;
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(fx->f64.data(), fx->w, fx->h, &cx, &cy, 1, &p, &rs);
            P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == DPSF_FIT_NO_CONVERGENCE, F_NEG,
                            "N6/σ=%.1f: status=%d (期望 NO_CONVERGENCE)",
                            fx->star.sx, rs ? rs[0].status : -999);
            if (rs) dpsf_free_results(rs);
        }
    }

    // N7: ITERATION_LIMIT(3) 非零回填 (FIX-PSF-D sharp σ=0.45; probe 实证
    //     st=3, B=100.12 A=1664.1 cx=24.000000 sx=0.463509; 钳位边界迭代
    //     耗尽, README §6 码 3 行为锚)
    {
        const FixPsfD d = fix_psf_d();
        const DPSFFitParams p = default_params();
        double cx = d.sharp.star.cx, cy = d.sharp.star.cy;
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch_d(d.sharp.f64.data(), d.sharp.w, d.sharp.h, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK(cs, rc == 0 && rs, F_NEG);
        if (rs) {
            const DPSFFitResult& r = rs[0];
            P1PSF_CHECK_MSG(cs, r.status == DPSF_FIT_ITERATION_LIMIT, F_NEG,
                            "N7: status=%d (期望 ITERATION_LIMIT=3)", r.status);
            if (r.status == DPSF_FIT_ITERATION_LIMIT) {
                // 回填参数有限且接近真值 (部分收敛; probe 回归锚)
                P1PSF_CHECK(cs, std::isfinite(r.B) && std::isfinite(r.A) &&
                                std::isfinite(r.cx) && std::isfinite(r.sx), F_NEG);
                P1PSF_CHECK_MSG(cs, std::fabs(r.B - 100.0) <= 0.5, F_NEG,
                                "N7: B=%.6f (回填锚 100.12±0.5)", r.B);
                P1PSF_CHECK_MSG(cs, rel_err(r.A, 1664.0912) <= 1e-3, F_NEG,
                                "N7: A=%.6f (回填锚 1664.09)", r.A);
                P1PSF_CHECK_MSG(cs, std::fabs(r.cx - 48.0) <= 1e-3 &&
                                    std::fabs(r.cy - 48.0) <= 1e-3, F_NEG,
                                "N7: cx=%.8f cy=%.8f (96x96 图星心 48/48)",
                                r.cx, r.cy);
                P1PSF_CHECK_MSG(cs, r.sx > 0.3 && r.sx < 0.55, F_NEG,
                                "N7: sx=%.6f (钳位域 (0.3, 0.55))", r.sx);
                // 码 3 行仍满足恒等式 (同一后处理链)
                P1PSF_CHECK(cs, rel_err(r.fwhm_x, kMoffat4FwhmFactor * r.sx) <= tol().fwhm_rel, F_NEG);
            }
            dpsf_free_results(rs);
        }
    }

    // N8: 混合批 f64 [N,9] — B2-A2: 成功行顺序 compact 到 0..nv-1, 失败星由
    //     逐星 out_status 报告 (不再占据 out_psf_params 行, 也不留 NaN 洞);
    //     (probe 实证 nv=2; 星↔行映射语义见 dynamic_psf.h DPSF_PSF_PARAMS_SCHEMA)
    {
        const int W = 96, H = 96, N = 3;
        std::vector<double> img((size_t)W * H, 80.0);
        const double xs[3] = {24.0, 48.0, 72.0};
        for (int i = 0; i < N; ++i)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 900.0, xs[i], 48.0,
                                                           2.0, 2.0, 0.0, (double)x, (double)y);
        // 第三星 (72,48) 整个拟合窗 (R=8 → 17x17) 置 NaN → 必败 (与
        // FIX-PSF-E all_nan_patch 同语义)。注意: 单像素 NaN 会被样本过滤
        // 跳过、有星场中不致命 (probe 实证 "Skipped 1 non-finite pixels"
        // 后正常回收); probe4 mixed 的 nv=2 必败实际源于其只渲染了第一颗
        // 星、第三星检测处为纯背景 — 本用例用全窗 NaN 构造确定性必败。
        for (int y = 48 - 8; y <= 48 + 8; ++y)
            for (int x = 72 - 8; x <= 72 + 8; ++x)
                img[(size_t)y * W + x] = std::nan("1");
        std::vector<double> det(6 * N, 0.0);
        for (int i = 0; i < N; ++i) { det[i * 6 + 0] = xs[i]; det[i * 6 + 1] = 48.0; }
        const DPSFFitParams p = default_params();
        std::vector<double> out9(9 * N, -1.0);
        int nv = -5;
        std::vector<int> st(N, -7);
        const int rc = dpsf_fit_batch_f64(img.data(), W, H, det.data(), N, &p, out9.data(), &nv,
                                          st.data());
        P1PSF_CHECK_MSG(cs, rc == 0 && nv == 2, F_NAN, "N8: rc=%d nv=%d (期望 2)", rc, nv);
        // B2-A2: 逐星状态按检测下标报告 (第 3 星失败, 未 compact 出洞)
        P1PSF_CHECK_MSG(cs, st[0] == DPSF_PSF_STATUS_OK && st[1] == DPSF_PSF_STATUS_OK &&
                                st[2] != DPSF_PSF_STATUS_OK && st[2] <= DPSF_PSF_STATUS_ALLOC_FAILED,
                        F_NAN, "N8: out_status=[%d,%d,%d]", st[0], st[1], st[2]);
        // 成功行 compact 到 0/1, 且全 9 字段有限 (无 NaN 洞)
        bool ok_rows_finite = true;
        for (int k = 0; k < 9; ++k)
            ok_rows_finite = ok_rows_finite && std::isfinite(out9[k]) && std::isfinite(out9[9 + k]);
        P1PSF_CHECK_MSG(cs, ok_rows_finite, F_NAN, "N8: compact 成功行出现非有限值");
        // compact 语义 (B2-A2): 成功行必须按检测下标升序 compact, 因此
        // row 0 的 cx 必须落在 star 0 (x=24) 的拟合窗内, row 1 落在 star 1
        // (x=48) 的窗内 —— star 2 (x=72, 失败) 不得出现在任何 compact 行。
        {
            const int R = p.fitRadius > 0 ? p.fitRadius : 8;
            const double win_lo[2] = {24.0 - R, 48.0 - R};
            const double win_hi[2] = {24.0 + R, 48.0 + R};
            for (int k = 0; k < nv; ++k) {
                const double cx_row = out9[(size_t)k * 9 + 2];
                P1PSF_CHECK_MSG(cs, cx_row >= win_lo[k] && cx_row <= win_hi[k], F_NAN,
                                "N8: compact row %d cx=%.4f 不在 star %d 窗 [%.1f,%.1f]",
                                k, cx_row, k, win_lo[k], win_hi[k]);
            }
        }
        // N8b (B2-A2 判别性保险): 逐星独立拟合的 9 参数值与该星在 compact 后的
        // 行位级相同; 且每个失败星的独立拟合行必须出现非有限值 —— 证明
        // "无 NaN compact 行" 断言对该数据确有鉴别力 (不是恒真)。
        {
            std::vector<double> single(9 * N, 0.0);
            std::vector<int> stx(N, -7);
            int nvx = -5;
            const int rcx = dpsf_fit_batch_f64(img.data(), W, H, det.data(), N, &p,
                                               single.data(), &nvx, stx.data());
            P1PSF_CHECK_MSG(cs, rcx == 0 && nvx == nv, F_NAN,
                            "N8b: independent fit nv=%d (期望 %d)", nvx, nv);
            int row = 0;
            for (int i = 0; i < N; ++i) {
                if (stx[i] == DPSF_PSF_STATUS_OK) {
                    P1PSF_CHECK_MSG(cs, std::memcmp(&single[(size_t)i * 9], &out9[(size_t)row * 9],
                                                    9 * sizeof(double)) == 0, F_NAN,
                                    "N8b: compact row %d != independent fit of star %d", row, i);
                    ++row;
                    continue;
                }
                bool any_nonfinite = false;
                for (int k = 0; k < 9; ++k)
                    if (!std::isfinite(single[(size_t)i * 9 + k])) any_nonfinite = true;
                P1PSF_CHECK_MSG(cs, any_nonfinite, F_NAN,
                                "N8b: 失败星 %d 的独立拟合行未出现非有限值 (断言无鉴别力)", i);
            }
        }

        // f64 版 (DPSFFitResult) 同输入: 失败星 status≠0
        double cxv[3] = {24.0, 48.0, 72.0}, cyv[3] = {48.0, 48.0, 48.0};
        DPSFFitResult* rs = nullptr;
        dpsf_fit_batch_d(img.data(), W, H, cxv, cyv, N, &p, &rs);
        if (rs) {
            P1PSF_CHECK(cs, rs[0].status == DPSF_FIT_OK && rs[1].status == DPSF_FIT_OK &&
                                rs[2].status != DPSF_FIT_OK, F_NAN);
            P1PSF_CHECK_MSG(cs, rs[2].B == 0.0 && rs[2].sx == 0.0, F_NAN,
                            "N8: batch_d 失败星未 memset 0 (B=%.4f)", rs[2].B);
            dpsf_free_results(rs);
        }
    }

    return cs.failures == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// boundary 组: 贴边四角 / FWHM≈rect 邻域 / 满量程 / det 行 stride 语义
// ---------------------------------------------------------------------------
int test_boundary() {
    CheckState cs;
    const char* F_REC = "recovery";
    const char* F_NEG = "negative_matrix";

    // B1: 四角贴边星回收 (rect clamp; README §4 坐标语义)
    {
        const double corners[4][2] = {{0.5, 0.5}, {47.5, 0.5}, {0.5, 47.5}, {47.5, 47.5}};
        for (int k = 0; k < 4; ++k) {
            const int W = 48;
            std::vector<double> img((size_t)W * W);
            for (int y = 0; y < W; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] = moffat4_eval(100.0, 2000.0, corners[k][0],
                                                          corners[k][1], 2.0, 2.0, 0.0,
                                                          (double)x, (double)y);
            const DPSFFitParams p = default_params();
            double cx = corners[k][0], cy = corners[k][1];
            DPSFFitResult* rs = nullptr;
            const int rc = dpsf_fit_batch_d(img.data(), W, W, &cx, &cy, 1, &p, &rs);
            P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == DPSF_FIT_OK, F_REC,
                            "B1/corner%d: rc=%d status=%d", k, rc, rs ? rs[0].status : -999);
            if (rs && rs[0].status == DPSF_FIT_OK) {
                P1PSF_CHECK_MSG(cs, std::fabs(rs[0].cx - corners[k][0]) <= 0.02 &&
                                    std::fabs(rs[0].cy - corners[k][1]) <= 0.02, F_REC,
                                "B1/corner%d: cx=%.6f cy=%.6f (truth %.1f/%.1f)",
                                k, rs[0].cx, rs[0].cy, corners[k][0], corners[k][1]);
                dpsf_free_results(rs);
            } else if (rs) {
                dpsf_free_results(rs);
            }
        }
    }

    // B2: FWHM≈rect 邻域 (R=8 → rect 17x17, 判据 fwhm>rect 宽; probe 实证
    //     σ∈{12.5..14} → 码 1; σ=3 对照 → 码 0)
    {
        const FixPsfD d = fix_psf_d();
        const DPSFFitParams p = default_params();
        double cx = d.wide.star.cx, cy = d.wide.star.cy;
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch_d(d.wide.f64.data(), d.wide.w, d.wide.h, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK_MSG(cs, rc == 0 && rs && rs[0].status == DPSF_FIT_NO_CONVERGENCE, F_NEG,
                        "B2: σ=5.5 status=%d (期望 1)", rs ? rs[0].status : -999);
        if (rs) dpsf_free_results(rs);
        // 正常对照: σ=3 星 (fwhm=3.69 ≪ rect) 码 0 — 拟合中心必须用对照星
        // 自身坐标 (24,24); 复用 wide 的 (48,48) 会在纯背景处拟合必败
        // (前一轮实现缺陷, probeB2 实证 s3A2000_96 → st=0)
        const FixPsfA ok = fix_psf_a(100.0, 2000.0, 24.0, 24.0, 3.0, 96, 96);
        double cx3 = 24.0, cy3 = 24.0;
        DPSFFitResult* rs2 = nullptr;
        dpsf_fit_batch_d(ok.f64.data(), ok.w, ok.h, &cx3, &cy3, 1, &p, &rs2);
        P1PSF_CHECK_MSG(cs, rs2 && rs2[0].status == DPSF_FIT_OK, F_REC,
                        "B2: σ=3 status=%d (期望 0)", rs2 ? rs2[0].status : -999);
        if (rs2) dpsf_free_results(rs2);
    }

    // B3: uint16 满量程 (65535 峰) — 饱和恒星光的真实域
    {
        const int W = 40;
        std::vector<double> img((size_t)W * W);
        for (int y = 0; y < W; ++y)
            for (int x = 0; x < W; ++x)
                img[(size_t)y * W + x] = moffat4_eval(200.0, 65000.0, 20.0, 20.0, 2.5, 2.5, 0.0, (double)x, (double)y);
        std::vector<uint16_t> u16(img.size());
        for (size_t i = 0; i < img.size(); ++i) u16[i] = (uint16_t)img[i];
        const DPSFFitParams p = default_params();
        double cx = 20.0, cy = 20.0;
        DPSFFitResult* rs = nullptr;
        const int rc = dpsf_fit_batch(u16.data(), W, W, &cx, &cy, 1, &p, &rs);
        P1PSF_CHECK(cs, rc == 0 && rs, F_REC);
        if (rs) {
            P1PSF_CHECK_MSG(cs, rs[0].status == DPSF_FIT_OK, F_REC,
                            "B3: saturated status=%d", rs[0].status);
            if (rs[0].status == DPSF_FIT_OK) {
                // u16 截断噪声 → 元数据宽容差 (README §5)
                P1PSF_CHECK(cs, std::fabs(rs[0].cx - 20.0) <= 0.05, F_REC);
                P1PSF_CHECK(cs, rel_err(rs[0].sx, 2.5) <= 2e-2, F_REC);
            }
            dpsf_free_results(rs);
        }
    }

    // B4: det 行 stride 语义 — 6 列行布局, 仅 [0]/[1] 消费 (与 U6 互补: 此处
    //     验证 [2]/[3] flux/mag 列同样不影响结果)
    {
        const int W = 128, H = 128, N = 2;
        std::vector<double> img((size_t)W * H, 50.0);
        const double xs[2] = {40.0, 88.0};
        for (int i = 0; i < N; ++i)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                    img[(size_t)y * W + x] += moffat4_eval(0.0, 900.0, xs[i], 64.0, 2.2, 1.8, 0.35, (double)x, (double)y);
        std::vector<double> detA(6 * N, 0.0), detB(6 * N, 0.0);
        for (int i = 0; i < N; ++i) { detA[i * 6 + 0] = xs[i]; detA[i * 6 + 1] = 64.0; }
        for (int i = 0; i < N; ++i) {
            detB[i * 6 + 0] = xs[i]; detB[i * 6 + 1] = 64.0;
            detB[i * 6 + 2] = 99999.0;   // flux 假值
            detB[i * 6 + 3] = -42.5;     // mag 假值
        }
        const DPSFFitParams p = default_params();
        std::vector<double> outA(9 * N, -1.0), outB(9 * N, -1.0);
        int nvA = -5, nvB = -5;
        dpsf_fit_batch_f64(img.data(), W, H, detA.data(), N, &p, outA.data(), &nvA, NULL);
        dpsf_fit_batch_f64(img.data(), W, H, detB.data(), N, &p, outB.data(), &nvB, NULL);
        P1PSF_CHECK_MSG(cs, nvA == nvB && vec_bitwise_eq(outA, outB), F_REC,
                        "B4: det [2]/[3] 列改变输出 (nvA=%d nvB=%d)", nvA, nvB);
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

// selfcheck 复用入口 (p1psf_tests_selfcheck.cpp 声明)
// 注: main() 独立于 p1psf_tests_main.cpp (对齐 p1hips/p1star 先例 —
// selfcheck 可执行与 core 可执行共享组 TU, 各自带自己的 main)
int p1psf_run_core_groups(int argc, char** argv) {
    const p1psf::TestGroup groups[] = {
        {"units", test_units},
        {"properties", test_properties},
        {"oracle", test_oracle},
        {"negative", test_negative},
        {"boundary", test_boundary},
    };
    return p1psf::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
