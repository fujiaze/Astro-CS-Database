// ============================================================================
// ipv_wcs.cpp - IPV WCS 输出模块实现
//
// 包含两个函数:
// 1. build_wcs (旧版, 从 SimTransform 提取 WCS, 保留向后兼容)
// 2. extract_wcs_sip (新版, 从 TRANS 提取 WCS + SIP)
//
// extract_wcs_sip 设计 (从 TRANS 线性项提取 CD 矩阵 +
// SIP 解析公式 + 网格反变换逆向 SIP):
// - TRANS 方向: U(像素) → W(角秒), 线性项单位 = 角秒/像素
// - CD 矩阵: trans 线性项 / 3600 (度/像素), 直接提取, 不用 M^-1
// - CRVAL = 收敛后中心
// - CRPIX = 图像中心 (1-based)
// - ctype: trans.order==1 → "RA---TAN"/"DEC--TAN", 否则 "RA---TAN-SIP"/...
// - SIP A/B: 解析公式 A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
// (cd_inv = trans 线性项的逆, 单位 像素/角秒)
// - SIP AP/BP: 网格反变换法 (NB_GRID=7)
// - RMS: 残差 = apply_trans(U) - W (角秒)
//
// 日期: 2026-07-05
// ============================================================================

#include "ipv_wcs.h"
#include "ipv_solver.h"   // extract_wcs_sip 声明
#include "ipv_itertrans.h" // Trans, apply_trans
#include "ipv_sip.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cstring>   // std::strncpy
#include <limits>    // std::numeric_limits (WCS-003 迭代反演)
#include <utility>   // std::pair (WCS-003 扩展拟合基表)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ipv {

// ---------------------------------------------------------------------------
// 内部辅助函数 (SIP 拟合用)
// ---------------------------------------------------------------------------


// 计算 x^i * y^j
static double eval_monomial(double x, double y, int i, int j) {
    double v = 1.0;
    for (int k = 0; k < i; k++) v *= x;
    for (int k = 0; k < j; k++) v *= y;
    return v;
}

// N×N 高斯消元法解线性方程组 (带部分主元选取)
static bool gauss_solve_wcs(std::vector<std::vector<double>>& A,
                            std::vector<double>& b) {
    int n = (int)b.size();
    for (int col = 0; col < n; col++) {
        int max_row = col;
        double max_val = std::abs(A[col][col]);
        for (int row = col + 1; row < n; row++) {
            if (std::abs(A[row][col]) > max_val) {
                max_val = std::abs(A[row][col]);
                max_row = row;
            }
        }
        if (max_val < 1e-15) {
            return false;
        }
        if (max_row != col) {
            std::swap(A[col], A[max_row]);
            std::swap(b[col], b[max_row]);
        }
        for (int row = col + 1; row < n; row++) {
            double factor = A[row][col] / A[col][col];
            A[row][col] = 0.0;
            for (int j = col + 1; j < n; j++) {
                A[row][j] -= factor * A[col][j];
            }
            b[row] -= factor * b[col];
        }
    }
    for (int row = n - 1; row >= 0; row--) {
        double sum = b[row];
        for (int j = row + 1; j < n; j++) {
            sum -= A[row][j] * b[j];
        }
        b[row] = sum / A[row][row];
    }
    return true;
}

// ---------------------------------------------------------------------------
// build_wcs: 旧版, 从相似变换提取标准 WCS (保留向后兼容)
//
// 关键推导 (U → W' → W):
// U.x = s0 * (px - cx), U.y = -s0 * (py - cy) (Y 轴向上)
// 相似变换对 W' 求解: W' = s·R(θ)·U + t
// 原始 W = flip(W'):
// NONE: W = W'
// FLIP_X: W.x = -W'.x, W.y = W'.y
// FLIP_Y: W.x = W'.x, W.y = -W'.y
// FLIP_XY: W.x = -W'.x, W.y = -W'.y
// CD 矩阵 (度/像素):
// scale = s·s0/3600
// sign_x = (mode==FLIP_X || mode==FLIP_XY) ? -1: +1
// sign_y = (mode==FLIP_Y || mode==FLIP_XY) ? -1: +1
// CD1_1 = sign_x · scale · cos
// CD1_2 = sign_x · scale · sin
// CD2_1 = sign_y · scale · sin
// CD2_2 = -sign_y · scale · cos
// ---------------------------------------------------------------------------
WcsFitResult build_wcs(
    const SimTransform& transform,
    double s0,
    int img_width,
    int img_height,
    double ra0,
    double dec0,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W_flipped,
    const std::vector<MatchPair>& inliers,
    int flip_mode
)
{
    // 零初始化以确保 ctype/AP/BP/ap_order 等新增字段有定义值
    WcsFitResult result{};

    const double s     = transform.s;
    const double theta = transform.theta;
    const double tx    = transform.tx;
    const double ty    = transform.ty;

    const double scale = s * s0 / 3600.0;
    const double cos_t = std::cos(theta);
    const double sin_t = std::sin(theta);

    double sign_x = +1.0, sign_y = +1.0;
    switch (flip_mode) {
        case 0: /* NONE     */ sign_x = +1.0; sign_y = +1.0; break;
        case 1: /* FLIP_X   */ sign_x = -1.0; sign_y = +1.0; break;
        case 2: /* FLIP_Y   */ sign_x = +1.0; sign_y = -1.0; break;
        case 3: /* FLIP_XY  */ sign_x = -1.0; sign_y = -1.0; break;
        default:              sign_x = +1.0; sign_y = +1.0; break;
    }

    const double cd1_1 =  sign_x * scale * cos_t;
    const double cd1_2 =  sign_x * scale * sin_t;
    const double cd2_1 =  sign_y * scale * sin_t;
    const double cd2_2 = -sign_y * scale * cos_t;

    result.crval[0] = ra0;
    result.crval[1] = dec0;

    const double cx = img_width  / 2.0;
    const double cy = img_height / 2.0;
    // 统一 CRPIX 为 width/2.0 + 0.5 (1-based FITS, frozen value)
    result.crpix[0] = cx + 0.5;
    result.crpix[1] = cy + 0.5;

    result.sip = fit_sip(U, W_flipped, inliers, transform, s0,
                         img_width, img_height, 3, nullptr);
    // fit_sip 仅初始化 A/B/order, 显式清零逆向 SIP 字段 (build_wcs 不输出 AP/BP)
    for (int i = 0; i < 36; ++i) {
        result.sip.AP[i] = 0.0;
        result.sip.BP[i] = 0.0;
    }
    result.sip.ap_order = 0;
    // WCS-003: 扩展逆向布局同步清零 (fit_sip 不初始化尾部新字段)
    for (int i = 0; i < 100; ++i) {
        result.sip.APx[i] = 0.0;
        result.sip.BPx[i] = 0.0;
    }
    result.sip.apx_order = 0;
    // ctype 根据 fit_sip.order 设置 (fit_sip 成功时 order=3, 否则 0)
    if (result.sip.order >= 2) {
        std::strncpy(result.ctype[0], "RA---TAN-SIP", 16);
        std::strncpy(result.ctype[1], "DEC--TAN-SIP", 16);
    } else {
        std::strncpy(result.ctype[0], "RA---TAN", 16);
        std::strncpy(result.ctype[1], "DEC--TAN", 16);
    }

    // RMS 计算
    double sum_r2 = 0.0;
    int    n_valid = 0;
    for (const MatchPair& mp : inliers) {
        if (mp.u < 0 || mp.u >= (int)U.size()) continue;
        if (mp.w < 0 || mp.w >= (int)W_flipped.size()) continue;

        const StarPoint& u = U[mp.u];
        const StarPoint& w = W_flipped[mp.w];

        const double x_pred = s * (cos_t * u.x - sin_t * u.y) + tx;
        const double y_pred = s * (sin_t * u.x + cos_t * u.y) + ty;

        const double dx = w.x - x_pred;
        const double dy = w.y - y_pred;
        sum_r2 += dx * dx + dy * dy;
        ++n_valid;
    }

    if (n_valid > 0) {
        const double mean_r2 = sum_r2 / static_cast<double>(n_valid);
        result.rms_px     = std::sqrt(mean_r2);
        result.rms_arcsec = result.rms_px * s0;
    } else {
        result.rms_arcsec = 0.0;
        result.rms_px     = 0.0;
    }

    result.cd.cd11 = cd1_1;
    result.cd.cd12 = cd1_2;
    result.cd.cd21 = cd2_1;
    result.cd.cd22 = cd2_2;
    result.n_pairs = (int)inliers.size();
    result.success = true;
    // best_mode 已移除, 用 trans_order=1 (线性 SimTransform)
    result.trans_order = 1;

    return result;
}

// ===========================================================================
// extract_wcs_sip: 从 TRANS 提取 WCS + SIP ( 重写)
//
// 修正: TRANS 方向从 W->U (像素->像素) 改为 U->W (像素->角秒)。
//
// 步骤:
// 1. CD 矩阵: trans 线性项 / 3600 (度/像素), 直接提取
// 2. CRVAL = 收敛后中心 (ra0, dec0)
// 3. CRPIX = 图像中心 (1-based FITS 约定)
// 4. ctype: trans.order==1 → "RA---TAN"/"DEC--TAN", 否则加 -SIP 后缀
// 5. SIP A/B: 解析公式 A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
// cd_inv = trans 线性项的逆 (像素/角秒)
// 6. SIP AP/BP: 网格反变换法 (NB_GRID=7, 拟合 UV→uv 多项式 = revtrans)
// 7. RMS: 残差 = apply_trans(U) - W (角秒), rms_px = rms_arcsec / s0
// ===========================================================================
void extract_wcs_sip(
    const Trans& trans,
    double ra0, double dec0,
    int img_width, int img_height,
    double s0,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& matched,
    WcsFitResult* result,
    Logger* logger)
{
    // 检查 result 指针
    if (result == nullptr) {
        return;
    }

    // 零初始化 (含 ctype, AP/BP, ap_order)
    *result = WcsFitResult{};

    if (logger) {
        logger->infof("  [诊断] extract_wcs_sip 入口: trans.order=%d, trans.valid=%d, "
                      "ra0=%.6f, dec0=%.6f, img=%dx%d, s0=%.4f, "
                      "U.size=%zu, W.size=%zu, matched.size=%zu",
                      trans.order, (int)trans.valid, ra0, dec0,
                      img_width, img_height, s0,
                      U.size(), W.size(), matched.size());
    }

    // ------------------------------------------------------------------
    // 1. CD 矩阵: 从 TRANS 线性项提取
    // TRANS: U(像素)->W(角秒), 线性项单位 = 角秒/像素
    // CD = 线性项 / 3600 (度/像素)
    // ------------------------------------------------------------------
    result->cd.cd11 = trans.x10 / 3600.0;
    result->cd.cd12 = trans.x01 / 3600.0;
    result->cd.cd21 = trans.y10 / 3600.0;
    result->cd.cd22 = trans.y01 / 3600.0;

    // ------------------------------------------------------------------
    // 2. CRVAL = 收敛后中心
    // ------------------------------------------------------------------
    result->crval[0] = ra0;
    result->crval[1] = dec0;

    // ------------------------------------------------------------------
    // 3. CRPIX = 图像中心 (1-based FITS 约定)
    // ------------------------------------------------------------------
    result->crpix[0] = img_width  / 2.0 + 0.5;
    result->crpix[1] = img_height / 2.0 + 0.5;

    // ------------------------------------------------------------------
    // 4. ctype 设置 ( 新增)
    // trans.order == 1: 纯线性, "RA---TAN" / "DEC--TAN"
    // trans.order >= 2: 含 SIP, "RA---TAN-SIP" / "DEC--TAN-SIP"
    // ------------------------------------------------------------------
    if (trans.order <= 1) {
        std::strncpy(result->ctype[0], "RA---TAN", 16);
        std::strncpy(result->ctype[1], "DEC--TAN", 16);
    } else {
        std::strncpy(result->ctype[0], "RA---TAN-SIP", 16);
        std::strncpy(result->ctype[1], "DEC--TAN-SIP", 16);
    }

    // ------------------------------------------------------------------
    // 5. SIP 系数 (order >= 2 时)
    // ------------------------------------------------------------------
    // 初始化 SIP (全 0, order=0, ap_order=0, apx_order=0)
    for (int i = 0; i < 36; ++i) {
        result->sip.A[i]  = 0.0;
        result->sip.B[i]  = 0.0;
        result->sip.AP[i] = 0.0;
        result->sip.BP[i] = 0.0;
    }
    for (int i = 0; i < 100; ++i) {
        result->sip.APx[i] = 0.0;
        result->sip.BPx[i] = 0.0;
    }
    result->sip.order     = 0;
    result->sip.ap_order  = 0;
    result->sip.apx_order = 0;

    if (trans.order >= 2) {
        // P1-5: 清零 trans.x00/y00
        // 平移完全由 CRVAL 吸收，避免 SIP AP/BP 常数项与 CRVAL 形成双重平移
        // trans->x00 = 0.; trans->y00 = 0.;
        // 注意: trans 是 const 引用，不能直接修改，创建副本用于 SIP 计算
        double saved_x00 = trans.x00;
        double saved_y00 = trans.y00;
        if (logger) {
            logger->infof("  [V4.22] 清零 trans.x00/y00 (saved: x00=%.4f, y00=%.4f)", saved_x00, saved_y00);
        }
        Trans trans_for_sip = trans;
        trans_for_sip.x00 = 0.0;
        trans_for_sip.y00 = 0.0;

        // 5.1 计算 trans 线性项的逆 (cd_inv, 像素/角秒)
        // cd_inv = inv(trans 线性项)
        // 注意: 用 trans 线性项的逆, 不是 result->cd 的逆 (差 3600 倍)
        // cd_inv[0][0] = invdet * cd[1][1], cd = trans 线性项
        const double det_lin = trans_for_sip.x10 * trans_for_sip.y01 - trans_for_sip.x01 * trans_for_sip.y10;
        if (std::abs(det_lin) < 1e-15) {
            if (logger) logger->warn("extract_wcs_sip: TRANS 线性项奇异 (det≈0), 跳过 SIP");
        } else {
            const double inv_det_lin = 1.0 / det_lin;
            const double cd_inv_00 =  inv_det_lin * trans_for_sip.y01;   // 像素/角秒
            const double cd_inv_01 = -inv_det_lin * trans_for_sip.x01;
            const double cd_inv_10 = -inv_det_lin * trans_for_sip.y10;
            const double cd_inv_11 =  inv_det_lin * trans_for_sip.x10;

            // 5.2 SIP A/B: 解析公式
            // A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
            // 单位: (像素/角秒) * (角秒/像素^(i+j)) = 1/像素^(i+j-1) (SIP 标准)
            // 索引: A[i*6+j], i=x幂, j=y幂
            result->sip.order = trans_for_sip.order;

            // 二阶项 (i+j=2): x20, x11, x02
            result->sip.A[12] = cd_inv_00 * trans_for_sip.x20 + cd_inv_01 * trans_for_sip.y20;  // A_20, idx=2*6+0=12
            result->sip.B[12] = cd_inv_10 * trans_for_sip.x20 + cd_inv_11 * trans_for_sip.y20;
            result->sip.A[7]  = cd_inv_00 * trans_for_sip.x11 + cd_inv_01 * trans_for_sip.y11;  // A_11, idx=1*6+1=7
            result->sip.B[7]  = cd_inv_10 * trans_for_sip.x11 + cd_inv_11 * trans_for_sip.y11;
            result->sip.A[2]  = cd_inv_00 * trans_for_sip.x02 + cd_inv_01 * trans_for_sip.y02;  // A_02, idx=0*6+2=2
            result->sip.B[2]  = cd_inv_10 * trans_for_sip.x02 + cd_inv_11 * trans_for_sip.y02;

            if (trans_for_sip.order >= 3) {
                // 三阶项 (i+j=3): x30, x21, x12, x03
                result->sip.A[18] = cd_inv_00 * trans_for_sip.x30 + cd_inv_01 * trans_for_sip.y30;  // A_30, idx=3*6+0=18
                result->sip.B[18] = cd_inv_10 * trans_for_sip.x30 + cd_inv_11 * trans_for_sip.y30;
                result->sip.A[13] = cd_inv_00 * trans_for_sip.x21 + cd_inv_01 * trans_for_sip.y21;  // A_21, idx=2*6+1=13
                result->sip.B[13] = cd_inv_10 * trans_for_sip.x21 + cd_inv_11 * trans_for_sip.y21;
                result->sip.A[8]  = cd_inv_00 * trans_for_sip.x12 + cd_inv_01 * trans_for_sip.y12;  // A_12, idx=1*6+2=8
                result->sip.B[8]  = cd_inv_10 * trans_for_sip.x12 + cd_inv_11 * trans_for_sip.y12;
                result->sip.A[3]  = cd_inv_00 * trans_for_sip.x03 + cd_inv_01 * trans_for_sip.y03;  // A_03, idx=0*6+3=3
                result->sip.B[3]  = cd_inv_10 * trans_for_sip.x03 + cd_inv_11 * trans_for_sip.y03;
            }

            if (logger) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "extract_wcs_sip: SIP A/B 解析公式成功, order=%d",
                    trans_for_sip.order);
                logger->info(buf);
            }

            // 5.3 SIP AP/BP: 网格反变换法 (WCS-002/DISP-WCS-008 整改:
            // 网格加密 7×7→41×41、拟合阶 trans.order→AP 布局上限 5、
            // 各向归一化正规方程)
            // 流程:
            // a. 生成像素网格 (u, v), 相对于图像中心
            // b. 对网格应用 trans_for_sip -> IWC (角秒, 已清零 x00/y00)
            // c. UV = cd_inv · IWC (无畸变像素, 线性部分=恒等)
            // d. 拟合 UV -> (u, v) 的多项式 = revtrans (阶 5, 归一化坐标)
            // e. AP/BP = revtrans 系数回转像素域, AP_10 -= 1, BP_01 -= 1
            //
            // 冻结消费语义: 一步 u = u0 + AP(u0, v0) (wcs_transform.cpp
            // skyToPixel 有 AP/BP 直加路径)。本拟合为该逆映射多项式在
            // SIPCoeffs 布局 (i*6+j, i+j <= ap_order) 内的最小二乘逼近。
            // 可达性 (F2 冻结 fixture 实测, 探针 run/tmp_wcs002): 边缘畸变
            // ~117px 时 5 阶最优 center90 roundtrip ~7.6 px, 为逆映射最近
            // 奇点决定的逼近极限 (order 6..15 → 5.3..0.26 px, 收敛率~0.73);
            // 冻结 1e-4 px 在该 fixture 下数学不可达, 需负责人裁决 (finding)。
            // 确定性: 固定网格顺序 + 顺序归约 (§5c 禁并行重结合), 高斯消元单线程。
            const int NB_GRID = 41;       // 每轴网格点数 (整改: 7 -> 41)
            const int AP_FIT_ORDER = 5;   // 逆向拟合阶 (SIPCoeffs i*6+j 布局上限)

            // 逆向 SIP 基函数: (i, j), i+j <= AP_FIT_ORDER (含常数和线性)
            // revtrans 是完整多项式 (含常数和线性), AP/BP 提取所有项
            std::vector<std::pair<int,int>> inv_basis;
            for (int deg = 0; deg <= AP_FIT_ORDER; ++deg) {
                for (int i = deg; i >= 0; --i) {
                    int j = deg - i;
                    inv_basis.push_back({i, j});
                }
            }
            int n_inv_coef = (int)inv_basis.size();

            // 网格范围 (相对于图像中心, 像素)
            double u_range = img_width  / 2.0;
            double v_range = img_height / 2.0;

            // 归一化半径 (各向, 正规方程条件数控制)
            const double R_u = (u_range > 0.0) ? u_range : 1.0;
            const double R_v = (v_range > 0.0) ? v_range : 1.0;

            // 构建正规方程: M_inv * capx = bx (u), M_inv * capy = by (v)
            std::vector<std::vector<double>> M_inv(n_inv_coef,
                                                   std::vector<double>(n_inv_coef, 0.0));
            std::vector<double> bx(n_inv_coef, 0.0), by(n_inv_coef, 0.0);
            int n_points = 0;

            for (int gi = 0; gi < NB_GRID; ++gi) {
                for (int gj = 0; gj < NB_GRID; ++gj) {
                    // a. 原始像素网格 (相对于图像中心)
                    double u = -u_range + 2.0 * u_range * gi / (NB_GRID - 1);
                    double v = -v_range + 2.0 * v_range * gj / (NB_GRID - 1);

                    // b. 应用 trans_for_sip → IWC (角秒)
                    // P1-5: trans_for_sip 已清零 x00/y00, IWC 不含平移
                    double wx, wy;
                    apply_trans(trans_for_sip, u, v, &wx, &wy);

                    // c. UV = cd_inv · IWC (无畸变像素)
                    // transUV 用 cd_inv 作线性项, atApplyTrans 后 xygrid = UV
                    // 因 trans_for_sip.x00/y00=0, IWC 无平移,
                    // UV 也无平移, revtrans 常数项 AP_00/BP_00 → 0
                    double uv_x = cd_inv_00 * wx + cd_inv_01 * wy;
                    double uv_y = cd_inv_10 * wx + cd_inv_11 * wy;

                    // 计算基函数值 (归一化 UV 上)
                    std::vector<double> bv(n_inv_coef);
                    for (int k = 0; k < n_inv_coef; ++k) {
                        bv[k] = eval_monomial(uv_x / R_u, uv_y / R_v,
                                              inv_basis[k].first,
                                              inv_basis[k].second);
                    }

                    // 累加正规方程 (拟合 UV → (u, v), 归一化目标)
                    for (int p = 0; p < n_inv_coef; ++p) {
                        for (int q = 0; q < n_inv_coef; ++q) {
                            M_inv[p][q] += bv[p] * bv[q];
                        }
                        bx[p] += bv[p] * (u / R_u);
                        by[p] += bv[p] * (v / R_v);
                    }
                    ++n_points;
                }
            }

            // d. 求解 AP/BP 系数 (最小二乘, 归一化坐标)
            std::vector<double> capx = bx;
            std::vector<std::vector<double>> M_x = M_inv;
            std::vector<double> capy = by;
            std::vector<std::vector<double>> M_y = M_inv;

            bool ap_ok = false;
            if (gauss_solve_wcs(M_x, capx) && gauss_solve_wcs(M_y, capy)) {
                ap_ok = true;
            }

            if (ap_ok) {
                // e. 填充 AP/BP 系数 (归一化系数回转像素域:
                //    u = Σ c_ij·(UV_x/R_u)^i·(UV_y/R_v)^j·R_u
                //      = Σ [c_ij·R_u^{1-i}·R_v^{-j}]·UV_x^i·UV_y^j)
                for (int k = 0; k < n_inv_coef; ++k) {
                    int i = inv_basis[k].first;
                    int j = inv_basis[k].second;
                    int idx = i * 6 + j;
                    if (idx < 36) {
                        result->sip.AP[idx] =
                            capx[k] * std::pow(R_u, 1.0 - i) * std::pow(R_v, -j);
                        result->sip.BP[idx] =
                            capy[k] * std::pow(R_v, 1.0 - j) * std::pow(R_u, -i);
                    }
                }
                // 约定: 逆向 SIP 线性项减 1
                // AP[1][0] (idx=6) = revtrans.x10 - 1
                // BP[0][1] (idx=1) = revtrans.y01 - 1
                result->sip.AP[6] -= 1.0;  // AP_10
                result->sip.BP[1] -= 1.0;  // BP_01

                result->sip.ap_order = AP_FIT_ORDER;

                if (logger) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                        "extract_wcs_sip: SIP AP/BP 网格反变换成功, ap_order=%d, "
                        "n_inv_coef=%d, n_grid=%dx%d",
                        AP_FIT_ORDER, n_inv_coef, NB_GRID, NB_GRID);
                    logger->info(buf);
                }
            } else {
                if (logger) logger->warn("extract_wcs_sip: SIP AP/BP 拟合失败 (奇异矩阵), 仅输出前向 A/B");
            }

            // --------------------------------------------------------------
            // 5.4 扩展逆向拟合 (WCS-003 布局扩展, owner 裁决 1 选 B):
            // 布局 36 (6x6, i*6+j) -> 100 (10x10, i*10+j), 网格 41x41 ->
            // 81x81, 拟合阶 5 -> 7。APx/BPx 为逆映射多项式的高容量表达,
            // 作 wcs_sky_to_pixel_iterative 一步初值; F2 冻结门 <1e-4 px
            // 由迭代式反演达成, 不受一步逼近极限 (逆映射最近奇点) 约束。
            // 与 5.3 段 36 项兼容层 (41 网格阶 5, C ABI/消费方契约面)
            // 并行输出, 5.3 段代码零改动。
            // 确定性: 同 5.3 固定网格顺序 + 顺序归约 (§5c 禁并行重结合),
            // 高斯消元单线程。
            // --------------------------------------------------------------
            {
                const int NB_GRID_X   = 81;  // 每轴网格点数 (5.3 段 41 -> 81)
                const int APX_ORDER   = 7;   // 扩展拟合阶 (布局容量上限 9 内)
                const int APX_STRIDE  = 10;  // 布局步长 i*10+j

                std::vector<std::pair<int,int>> xb;
                for (int deg = 0; deg <= APX_ORDER; ++deg) {
                    for (int i = deg; i >= 0; --i) {
                        xb.push_back({i, deg - i});
                    }
                }
                const int nc = (int)xb.size();  // (APX_ORDER+1)(APX_ORDER+2)/2

                std::vector<std::vector<double>> MX(nc, std::vector<double>(nc, 0.0));
                std::vector<double> cx2(nc, 0.0), cy2(nc, 0.0);
                for (int gi = 0; gi < NB_GRID_X; ++gi) {
                    for (int gj = 0; gj < NB_GRID_X; ++gj) {
                        const double u = -u_range + 2.0 * u_range * gi / (NB_GRID_X - 1);
                        const double v = -v_range + 2.0 * v_range * gj / (NB_GRID_X - 1);
                        double wx, wy;
                        apply_trans(trans_for_sip, u, v, &wx, &wy);
                        const double uv_x = cd_inv_00 * wx + cd_inv_01 * wy;
                        const double uv_y = cd_inv_10 * wx + cd_inv_11 * wy;
                        std::vector<double> bv(nc);
                        for (int k = 0; k < nc; ++k) {
                            bv[k] = eval_monomial(uv_x / R_u, uv_y / R_v,
                                                  xb[k].first, xb[k].second);
                        }
                        for (int p = 0; p < nc; ++p) {
                            for (int q = 0; q < nc; ++q) {
                                MX[p][q] += bv[p] * bv[q];
                            }
                            cx2[p] += bv[p] * (u / R_u);
                            cy2[p] += bv[p] * (v / R_v);
                        }
                    }
                }
                std::vector<std::vector<double>> MXx = MX, MXy = MX;
                if (gauss_solve_wcs(MXx, cx2) && gauss_solve_wcs(MXy, cy2)) {
                    // 归一化系数回转像素域 (同 5.3 段回转公式, 布局 i*10+j)
                    for (int k = 0; k < nc; ++k) {
                        const int i = xb[k].first;
                        const int j = xb[k].second;
                        const int idx = i * APX_STRIDE + j;
                        if (idx >= 0 && idx < 100) {
                            result->sip.APx[idx] =
                                cx2[k] * std::pow(R_u, 1.0 - i) * std::pow(R_v, -j);
                            result->sip.BPx[idx] =
                                cy2[k] * std::pow(R_v, 1.0 - j) * std::pow(R_u, -i);
                        }
                    }
                    // 约定: 逆向 SIP 线性项减 1 (同 5.3 段, 布局 i*10+j)
                    result->sip.APx[10] -= 1.0;  // APx_10 (i=1, j=0)
                    result->sip.BPx[1]  -= 1.0;  // BPx_01 (i=0, j=1)
                    result->sip.apx_order = APX_ORDER;
                    if (logger) {
                        char buf[256];
                        std::snprintf(buf, sizeof(buf),
                            "extract_wcs_sip: 扩展逆向 APx/BPx 拟合成功, "
                            "apx_order=%d, n_coef=%d, n_grid=%dx%d",
                            APX_ORDER, nc, NB_GRID_X, NB_GRID_X);
                        logger->info(buf);
                    }
                } else {
                    if (logger) logger->warn("extract_wcs_sip: 扩展逆向 APx/BPx 拟合失败 (奇异矩阵), apx_order=0");
                }
            }
        }
    } else {
        if (logger) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "extract_wcs_sip: TRANS order=%d < 2, 不输出 SIP", trans.order);
            logger->info(buf);
        }
    }

    // ------------------------------------------------------------------
    // 6. RMS 计算 (用最终匹配对)
    // TRANS: U(像素)→W(角秒), 残差 = apply_trans(U) - W (角秒)
    // ------------------------------------------------------------------
    double sum_r2 = 0.0;
    int n_valid = 0;
    for (const MatchPair& mp : matched) {
        if (mp.u < 0 || mp.u >= (int)U.size()) continue;
        if (mp.w < 0 || mp.w >= (int)W.size()) continue;

        const StarPoint& u = U[mp.u];
        const StarPoint& w = W[mp.w];

        // apply_trans(U) → W_pred (角秒)
        double wx_pred, wy_pred;
        apply_trans(trans, u.x, u.y, &wx_pred, &wy_pred);

        double dx = wx_pred - w.x;
        double dy = wy_pred - w.y;
        sum_r2 += dx * dx + dy * dy;
        ++n_valid;
    }

    if (n_valid > 0) {
        // 残差单位 = 角秒 (因为 W 是角秒, trans: U→W)
        result->rms_arcsec = std::sqrt(sum_r2 / n_valid);
        result->rms_px     = result->rms_arcsec / s0;
    } else {
        result->rms_px     = 0.0;
        result->rms_arcsec = 0.0;
    }

    // ------------------------------------------------------------------
    // 7. 填充统计字段
    // ------------------------------------------------------------------
    result->n_pairs      = n_valid;
    result->trans_order  = trans.order;
    result->success      = true;

    // ------------------------------------------------------------------
    // 8.: 转换为标准 FITS WCS (Y-down)
    // solver 内部用 Y-up 约定 (U.y = -(det_y - cy), 见 ipv_select.cpp:687),
    // 但 FITS/WCS 国际标准 Y 向下 (数据行号递增 = Y 增大)。
    // 需在输出边界做 Y-up → Y-down 转换, 使 IpvWcsResult 直接为标准 WCS。
    //
    // 推导 (U = (p_x, -p_y), CD_FITS = M·diag(1,-1)):
    // CD: cd12, cd22 取反 (Y 相关列)
    // SIP A (x输出): A' = A·(-1)^j (仅输入 y 翻转)
    // SIP B (y输出): B' = -B·(-1)^j (输入+输出 y 翻转)
    // SIP AP/BP: 同 A/B 规则
    // CRVAL/CRPIX 不变 (中心点对称)。
    // validate_wcs 不受影响: 中心点翻转后仍是中心; det=|cd11*cd22-cd12*cd21| 不变。
    // ------------------------------------------------------------------
    result->cd.cd12 = -result->cd.cd12;
    result->cd.cd22 = -result->cd.cd22;

    // 前向 SIP A/B (仅当 sip_order >= 2 时有非零系数)
    {
        int so = result->sip.order;
        for (int i = 0; i <= so; ++i) {
            for (int j = 0; j <= so - i; ++j) {
                int idx = i * 6 + j;
                if (idx >= 36) break;
                double sign_in = (j & 1) ? -1.0 : 1.0;   // (-1)^j
                result->sip.A[idx]  *= sign_in;          // 仅输入 y 翻转
                result->sip.B[idx]  *= -sign_in;         // 输入+输出 y 翻转
            }
        }
    }
    // 逆向 SIP AP/BP
    {
        int apo = result->sip.ap_order;
        for (int i = 0; i <= apo; ++i) {
            for (int j = 0; j <= apo - i; ++j) {
                int idx = i * 6 + j;
                if (idx >= 36) break;
                double sign_in = (j & 1) ? -1.0 : 1.0;
                result->sip.AP[idx] *= sign_in;
                result->sip.BP[idx] *= -sign_in;
            }
        }
    }
    // 扩展逆向 SIP APx/BPx (WCS-003 布局 i*10+j, 同 A/B 规则)
    {
        int axo = result->sip.apx_order;
        for (int i = 0; i <= axo; ++i) {
            for (int j = 0; j <= axo - i; ++j) {
                int idx = i * 10 + j;
                if (idx >= 100) break;
                double sign_in = (j & 1) ? -1.0 : 1.0;
                result->sip.APx[idx] *= sign_in;
                result->sip.BPx[idx] *= -sign_in;
            }
        }
    }

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "extract_wcs_sip: CD=[%.6e %.6e; %.6e %.6e], CRVAL=(%.6f, %.6f), "
            "CRPIX=(%.1f, %.1f), ctype=[%s, %s], n_pairs=%d, rms_px=%.4f, "
            "rms_arcsec=%.4f, trans_order=%d, sip_order=%d, ap_order=%d",
            result->cd.cd11, result->cd.cd12,
            result->cd.cd21, result->cd.cd22,
            result->crval[0], result->crval[1],
            result->crpix[0], result->crpix[1],
            result->ctype[0], result->ctype[1],
            n_valid, result->rms_px, result->rms_arcsec,
            result->trans_order, result->sip.order, result->sip.ap_order);
        logger->info(buf);
    }
}

// ===========================================================================
// wcs_sky_to_pixel_iterative: 迭代式反演 (WCS-003 owner 裁决 1 选 B)
// ...
// ===========================================================================

// TAN gnomonic 正投影 (度进出): (ra,dec) -> (ξ,η)
// 标准公式 (Calabretta & Greisen 2002), 与消费方 tanWorldToIntermediate 同式。
static bool tan_project_iter(double ra, double dec,
                             double ra0, double dec0,
                             double* xi, double* eta) {
    if (!std::isfinite(ra) || !std::isfinite(dec) ||
        !std::isfinite(ra0) || !std::isfinite(dec0)) {
        return false;
    }
    const double ra_rad   = ra  * (M_PI / 180.0);
    const double dec_rad  = dec * (M_PI / 180.0);
    const double ra0_rad  = ra0 * (M_PI / 180.0);
    const double dec0_rad = dec0 * (M_PI / 180.0);
    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);
    const double sdec  = std::sin(dec_rad);
    const double cdec  = std::cos(dec_rad);
    const double dra   = ra_rad - ra0_rad;
    const double cdra  = std::cos(dra);
    const double sdra  = std::sin(dra);
    const double cosc  = sdec0 * sdec + cdec0 * cdec * cdra;
    if (std::fabs(cosc) < 1e-12) {
        return false;  // 投影背面/发散
    }
    const double xi_rad  = cdec * sdra / cosc;
    const double eta_rad = (cdec0 * sdec - sdec0 * cdec * cdra) / cosc;
    *xi  = xi_rad * (180.0 / M_PI);
    *eta = eta_rad * (180.0 / M_PI);
    return std::isfinite(*xi) && std::isfinite(*eta);
}

// 前向 SIP 求值 (Y-down 标准域, 36 布局 i*6+j, i+j<=order)
static void sip_fwd_ab(const WcsFitResult& w, double u, double v,
                       double* fu, double* fv) {
    double ax = 0.0, by = 0.0;
    const int so = w.sip.order;
    if (so >= 2) {
        for (int i = 0; i <= so; ++i) {
            for (int j = 0; j <= so - i; ++j) {
                const int idx = i * 6 + j;
                if (idx >= 36) break;
                const double uv = std::pow(u, i) * std::pow(v, j);
                ax += w.sip.A[idx] * uv;
                by += w.sip.B[idx] * uv;
            }
        }
    }
    *fu = ax;
    *fv = by;
}

// 迭代式反演入口 (原 namespace ipv 内, 41 行起)

WcsIterativeResult wcs_sky_to_pixel_iterative(
    const WcsFitResult& wcs,
    double ra_deg,
    double dec_deg,
    double tol_px,
    int max_iter)
{
    WcsIterativeResult out{};
    out.converged = false;
    out.x = std::numeric_limits<double>::quiet_NaN();
    out.y = std::numeric_limits<double>::quiet_NaN();
    out.iterations = 0;
    out.reject_code = 0;

    if (max_iter <= 0 || !(tol_px > 0.0) ||
        !std::isfinite(tol_px)) {
        out.reject_code = 2;
        return out;
    }

    // 1. TAN 正投影 (背面/非有限确定性拒绝)
    double xi, eta;
    if (!tan_project_iter(ra_deg, dec_deg,
                          wcs.crval[0], wcs.crval[1], &xi, &eta)) {
        out.reject_code = 1;
        return out;
    }

    // 2. UV = CD⁻¹·(ξ,η) (CRPIX 相对)
    const double det = wcs.cd.cd11 * wcs.cd.cd22 - wcs.cd.cd12 * wcs.cd.cd21;
    if (!std::isfinite(det) || std::fabs(det) < 1e-15) {
        out.reject_code = 3;  // CD 奇异 (构造病态)
        return out;
    }
    const double i00 =  wcs.cd.cd22 / det;
    const double i01 = -wcs.cd.cd12 / det;
    const double i10 = -wcs.cd.cd21 / det;
    const double i11 =  wcs.cd.cd11 / det;
    const double uvx = i00 * xi + i01 * eta;
    const double uvy = i10 * xi + i11 * eta;
    double u = uvx, v = uvy;

    // 3. 初值: APx (扩展逆向, 一步) -> AP (兼容层一步) -> UV
    {
        const int axo = wcs.sip.apx_order;
        if (axo > 0) {
            double ax = 0.0, by = 0.0;
            for (int i = 0; i <= axo; ++i) {
                for (int j = 0; j <= axo - i; ++j) {
                    const int idx = i * 10 + j;
                    if (idx >= 100) break;
                    const double uv = std::pow(u, i) * std::pow(v, j);
                    ax += wcs.sip.APx[idx] * uv;
                    by += wcs.sip.BPx[idx] * uv;
                }
            }
            u += ax;
            v += by;
        } else if (wcs.sip.ap_order > 0) {
            double ax = 0.0, by = 0.0;
            for (int i = 0; i <= wcs.sip.ap_order; ++i) {
                for (int j = 0; j <= wcs.sip.ap_order - i; ++j) {
                    const int idx = i * 6 + j;
                    if (idx >= 36) break;
                    const double uv = std::pow(u, i) * std::pow(v, j);
                    ax += wcs.sip.AP[idx] * uv;
                    by += wcs.sip.BP[idx] * uv;
                }
            }
            u += ax;
            v += by;
        }
    }
    if (!std::isfinite(u) || !std::isfinite(v)) {
        out.reject_code = 2;
        return out;
    }

    // 4. 牛顿迭代: 解 F(u) = u + A(u) = UV
    //    J = I + ∂A/∂u; |F|∞<tol 收敛; |det J|<1e-15 → 奇点拒绝。
    //    混合阻尼 (畸变场 |∇A|→1 强非线性域牛顿步可越收敛盆):
    //    残差增大时步长减半 (确定性回退, 至多 20 次阻尼/步)。
    const double guard = 1e6;  // 像素域发散护栏 (确定性)
    int iters = 0;
    double f_prev = std::numeric_limits<double>::infinity();
    for (int k = 1; k <= max_iter; ++k) {
        double fu, fv;
        sip_fwd_ab(wcs, u, v, &fu, &fv);
        const double fx = u + fu - uvx;  // F(u) - UV
        const double fy = v + fv - uvy;
        iters = k;
        const double f_now = std::max(std::fabs(fx), std::fabs(fy));
        if (f_now < tol_px) {
            out.converged = true;
            out.reject_code = 0;
            out.iterations = iters;
            // 生产自洽输出约定: u = x − crpix (与 oracle_wcs_forward /
            // oracle_wcs_reverse 及 WcsFitResult FITS 语义同一口径)。
            out.x = u + wcs.crpix[0];
            out.y = v + wcs.crpix[1];
            return out;
        }
        // ∂A/∂u (解析, 与数值微分对拍一致):
        //   ∂/∂u = Σ_{i>=1} i·A_ij·u^(i-1)·v^j
        //   ∂/∂v = Σ_{j>=1} j·A_ij·u^i·v^(j-1)  (含 i=0 项, A_02/B_02 等)
        double d11 = 0.0, d12 = 0.0, d21 = 0.0, d22 = 0.0;
        const int so = wcs.sip.order;
        if (so >= 2) {
            for (int i = 1; i <= so; ++i) {
                for (int j = 0; j <= so - i; ++j) {
                    const int idx = i * 6 + j;
                    if (idx >= 36) break;
                    const double pu = std::pow(u, i - 1) * std::pow(v, j);
                    d11 += wcs.sip.A[idx] * ((double)i * pu);
                    d21 += wcs.sip.B[idx] * ((double)i * pu);
                }
            }
            for (int i = 0; i <= so; ++i) {
                for (int j = 1; j <= so - i; ++j) {
                    const int idx = i * 6 + j;
                    if (idx >= 36) break;
                    const double pv = std::pow(u, i) * std::pow(v, j - 1);
                    d12 += wcs.sip.A[idx] * ((double)j * pv);
                    d22 += wcs.sip.B[idx] * ((double)j * pv);
                }
            }
        }
        const double j11v = 1.0 + d11, j12v = d12, j21v = d21, j22v = 1.0 + d22;
        const double jdet = j11v * j22v - j12v * j21v;
        if (!std::isfinite(jdet) || std::fabs(jdet) < 1e-15) {
            out.reject_code = 3;
            out.iterations = iters;
            return out;
        }
        // 阻尼: 残差未降则回退步长 (确定性, 至多 20 次折半)
        double lambda = 1.0;
        double nu = 0.0, nv = 0.0;
        for (int d = 0; d < 20; ++d) {
            const double su = lambda * ( j22v * fx - j12v * fy) / jdet;
            const double sv = lambda * (-j21v * fx + j11v * fy) / jdet;
            nu = u - su;
            nv = v - sv;
            if (!std::isfinite(nu) || !std::isfinite(nv) ||
                std::fabs(su) > guard || std::fabs(sv) > guard) {
                out.reject_code = 2;
                out.iterations = iters;
                return out;
            }
            double fu2, fv2;
            sip_fwd_ab(wcs, nu, nv, &fu2, &fv2);
            const double f2 = std::max(std::fabs(nu + fu2 - uvx),
                                       std::fabs(nv + fv2 - uvy));
            if (f2 < f_now || f2 <= f_prev) break;
            lambda *= 0.5;
        }
        f_prev = f_now;
        u = nu;
        v = nv;
        if (!std::isfinite(u) || !std::isfinite(v)) {
            out.reject_code = 2;
            out.iterations = iters;
            return out;
        }
    }
    out.reject_code = 2;  // max_iter 用尽未收敛
    out.iterations = iters;
    return out;
}

} // namespace ipv
