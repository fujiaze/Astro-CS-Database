// ============================================================================
// p1psf_prodpath_check.cpp — B4-5 (M3b-F-01) 共址回归锁
//
// 被验条目: M3b-F-01 "质心验收门按「非生产初始化配置」定义判据，生产路径质心无门"。
// 要求 (不改 SCI/不改门): 门按**生产路径**定义 —— sdet 坐标作初值, 不使用
//   gate2 脚本里那套 "+0.5 offset 匹配真值" 的口径; converged-init 版只允许
//   作为"拟合器本征精度旁证门"。
//
// 本锁构型 (findings 明确要求 p1psf 增加"非整数真值中心 + sdet 系初值"构型):
//   T1 生产路径初值: detection = (cx_truth + 0.5, cy_truth + 0.5)。
//      +0.5 的来源: star_detector 输出为像素中心坐标 (array index + 0.5,
//      FITS 约定), 而本 fixture 的解析真值在整数采样格点上 —— 这正是
//      gate2 生产路径要面对的坐标语义差 (端到端口径由 owner 裁决 M3b-C-01,
//      本锁不单方面定义; 仅按要求以初值构型覆盖)。
//   T2 本征精度对照: detection = 真值 (收敛初始化)。
//   断言口径: SCI-P1-STAR-001 §1 冻结绝对验收项 |Δc| ≤ 0.3 px (SNR≥20)。
//   本锁把该绝对门同时用于 T1 (生产路径) 与 T2 (本征对照), 从而把"生产路径
//   质心偏差"从 findings 里的 BLOCKER 文本变成可机跑断言。
//
// 回归锁语义 (未修复必红 / 已修复必绿): 修复前仓库无任何以 sdet 系初值 +
//   非整数真值中心为构型的质心门 (本锁新增);
//   T1 断言在 0.5px 级偏差下必红, 修复端到端坐标契约 (owner 裁决) 后方可绿。
//   T2 断言为对照: 若 T2 也红说明拟合器本身退化, 与本条无关, 直接暴露。
//   → 结论由证据给出, 本文件不预置"已绿"假设。
//
// ctest 目标名: p1psf_prodpath_centroid
// 日期: 2026-09-14 (RQS B4-phase1)
// ============================================================================

#include "dynamic_psf.h"

#include "p1psf_fixtures.hpp"
#include "p1psf_oracle.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { std::printf("  [PASS] %s\n", msg); }                 \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }        \
    } while (0)

// SCI-P1-STAR-001 §1 冻结绝对质心验收项 (SNR≥20 域)
constexpr double kSciCentroidAbsPx = 0.3;

// 合成 Moffat4 场 (复用 p1psf 测试侧独立 oracle 采样器, 不调用被测函数)
std::vector<float> make_field(int w, int h, double B, double A,
                              double cx, double cy, double sx) {
    std::vector<float> img((size_t)w * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img[(size_t)y * w + x] = (float)p1psf::moffat4_eval(
                B, A, cx, cy, sx, sx, 0.0, (double)x, (double)y);
    return img;
}

// 单星批量拟合: detection 为 [N,6] 行 (x,y,flux,...), 返回 (status, cx, cy)
struct FitOut { int status; double cx; double cy; bool ok; };

FitOut fit_one(const std::vector<float>& img, int w, int h,
               double det_x, double det_y) {
    double det[6] = {det_x, det_y, 0.0, 0.0, 0.0, 0.0};
    DPSFFitParams p;
    p.fitRadius = 8;
    p.maxIter = 200;
    p.tolerance = 1e-8;
    double out9[9] = {0};
    int nv = 0, status = -1;
    const int rc = dpsf_fit_batch_f32(img.data(), w, h, det, 1, &p, out9, &nv,
                                      &status);
    FitOut r{status, out9[2], out9[3], false};
    if (rc == 0 && nv == 1 && status == DPSF_FIT_OK) r.ok = true;
    return r;
}

}  // namespace

int main() {
    std::printf("[B4-5] 生产路径质心门 (sdet 系初值 + 非整数真值中心) 回归锁\n");

    const int W = 64, H = 64;
    const double B = 100.0;
    const double SIGMA = 8.0;         // 背景噪声 (本测试场无噪声, σ 仅用于标定 A)
    const double SNR = 20.0;          // SCI-P1-STAR-001 §1 验收域下限
    const double A = SNR * SIGMA;     // A/σ = 20 (SNR 定义在 SCI 未冻结, 见 REPORT)
    const double SX = 1.8;
    // 非整数真值中心 (findings 要求构型)
    const double CX = 31.37, CY = 27.61;

    std::vector<float> img = make_field(W, H, B, A, CX, CY, SX);

    // ── T1: 生产路径初值 (sdet 系 = 像素中心坐标, truth + 0.5) ──────────
    {
        FitOut r = fit_one(img, W, H, CX + 0.5, CY + 0.5);
        CHECK(r.ok, "t1_status: 生产路径初值拟合 status=OK");
        if (r.ok) {
            const double dcx = std::fabs(r.cx - CX);
            const double dcy = std::fabs(r.cy - CY);
            const double dc = std::hypot(dcx, dcy);
            std::printf("    [info] T1 fit=(%.6f,%.6f) truth=(%.6f,%.6f) "
                        "|dc|=%.6f px\n", r.cx, r.cy, CX, CY, dc);
            CHECK(dc <= kSciCentroidAbsPx,
                  "t1_sci_gate: 生产路径 |Δc| <= 0.3 px (SCI-P1-STAR-001 §1)");
        }
    }

    // ── T2: 本征精度对照 (收敛初始化 = 真值) ────────────────────────────
    {
        FitOut r = fit_one(img, W, H, CX, CY);
        CHECK(r.ok, "t2_status: 收敛初始化拟合 status=OK");
        if (r.ok) {
            const double dc = std::hypot(r.cx - CX, r.cy - CY);
            std::printf("    [info] T2 fit=(%.6f,%.6f) truth=(%.6f,%.6f) "
                        "|dc|=%.6f px\n", r.cx, r.cy, CX, CY, dc);
            CHECK(dc <= 1e-2,
                  "t2_intrinsic: 收敛初始化本征精度 <= 0.01 px (旁证门)");
        }
    }

    // ── T3: 判别力对照 — 轻度偏移初值必须拉回真值 ─────────────────────
    // 若拟合器只是"停在初值附近", T3 会以 ~0.25px 偏差失败; 若真收敛,
    // T3 回到真值 ⇒ 证明 T1/T2 的通过不是初值巧合。
    {
        FitOut r = fit_one(img, W, H, CX + 0.25, CY - 0.25);
        CHECK(r.ok, "t3_status: 偏移初值拟合 status=OK");
        if (r.ok) {
            const double dc = std::hypot(r.cx - CX, r.cy - CY);
            std::printf("    [info] T3 fit=(%.6f,%.6f) truth=(%.6f,%.6f) "
                        "|dc|=%.6f px\n", r.cx, r.cy, CX, CY, dc);
            CHECK(dc <= 1e-2,
                  "t3_pullback: 偏移 (0.25px) 初值仍收敛到真值 (拟合器非停驻)");
        }
    }

    if (g_failures == 0) {
        std::printf("B4-5 PRODPATH CENTROID PASS\n");
        return 0;
    }
    std::printf("B4-5 PRODPATH CENTROID FAIL (%d check(s))\n", g_failures);
    return 1;
}
