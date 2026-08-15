// ============================================================================
// ipv_wcs.cpp - IPV WCS 输出模块实现 (V4.20)
//
// 包含两个函数:
//   1. build_wcs (旧版, 从 SimTransform 提取 WCS, 保留向后兼容)
//   2. extract_wcs_sip (新版, 从 TRANS 提取 WCS + SIP)
//
// V4.20 extract_wcs_sip 设计 (从 TRANS 线性项提取 CD 矩阵 +
// SIP 解析公式 + 网格反变换逆向 SIP):
//   - TRANS 方向: U(像素) → W(角秒), 线性项单位 = 角秒/像素
//   - CD 矩阵: trans 线性项 / 3600 (度/像素), 直接提取, 不用 M^-1
//   - CRVAL = 收敛后中心
//   - CRPIX = 图像中心 (1-based)
//   - ctype: trans.order==1 → "RA---TAN"/"DEC--TAN", 否则 "RA---TAN-SIP"/...
//   - SIP A/B: 解析公式 A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
//     (cd_inv = trans 线性项的逆, 单位 像素/角秒)
//   - SIP AP/BP: 网格反变换法 (NB_GRID=7)
//   - RMS: 残差 = apply_trans(U) - W (角秒)
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
#include <cstring>   // std::strncpy (V4.20 ctype)

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
//   U.x = s0 * (px - cx), U.y = -s0 * (py - cy)   (Y 轴向上)
//   相似变换对 W' 求解: W' = s·R(θ)·U + t
//   原始 W = flip(W'):
//     NONE:    W = W'
//     FLIP_X:  W.x = -W'.x, W.y = W'.y
//     FLIP_Y:  W.x = W'.x,  W.y = -W'.y
//     FLIP_XY: W.x = -W'.x, W.y = -W'.y
//   CD 矩阵 (度/像素):
//     scale = s·s0/3600
//     sign_x = (mode==FLIP_X || mode==FLIP_XY) ? -1 : +1
//     sign_y = (mode==FLIP_Y || mode==FLIP_XY) ? -1 : +1
//     CD1_1 =  sign_x · scale · cos
//     CD1_2 =  sign_x · scale · sin
//     CD2_1 =  sign_y · scale · sin
//     CD2_2 = -sign_y · scale · cos
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
    // V4.20: 零初始化以确保 ctype/AP/BP/ap_order 等新增字段有定义值
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
    // P11-006: 统一 CRPIX 为 width/2.0 + 0.5 (1-based FITS, P11-001 frozen value)
    result.crpix[0] = cx + 0.5;
    result.crpix[1] = cy + 0.5;

    result.sip = fit_sip(U, W_flipped, inliers, transform, s0,
                         img_width, img_height, 3, nullptr);
    // V4.20: fit_sip 仅初始化 A/B/order, 显式清零逆向 SIP 字段 (build_wcs 不输出 AP/BP)
    for (int i = 0; i < 36; ++i) {
        result.sip.AP[i] = 0.0;
        result.sip.BP[i] = 0.0;
    }
    result.sip.ap_order = 0;
    // V4.20: ctype 根据 fit_sip.order 设置 (fit_sip 成功时 order=3, 否则 0)
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
    // V4.19: best_mode 已移除, 用 trans_order=1 (线性 SimTransform)
    result.trans_order = 1;

    return result;
}

// ===========================================================================
// extract_wcs_sip: 从 TRANS 提取 WCS + SIP (V4.20 重写)
//
// V4.20 修正: TRANS 方向从 W->U (像素->像素) 改为 U->W (像素->角秒)。
//
// 步骤:
//   1. CD 矩阵: trans 线性项 / 3600 (度/像素), 直接提取
//   2. CRVAL = 收敛后中心 (ra0, dec0)
//   3. CRPIX = 图像中心 (1-based FITS 约定)
//   4. ctype: trans.order==1 → "RA---TAN"/"DEC--TAN", 否则加 -SIP 后缀
//   5. SIP A/B: 解析公式 A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
//      cd_inv = trans 线性项的逆 (像素/角秒)
//   6. SIP AP/BP: 网格反变换法 (NB_GRID=7, 拟合 UV→uv 多项式 = revtrans)
//   7. RMS: 残差 = apply_trans(U) - W (角秒), rms_px = rms_arcsec / s0
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
    //    TRANS: U(像素)->W(角秒), 线性项单位 = 角秒/像素
    //    CD = 线性项 / 3600 (度/像素)
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
    // 4. ctype 设置 (V4.20 新增)
    //    trans.order == 1: 纯线性, "RA---TAN" / "DEC--TAN"
    //    trans.order >= 2: 含 SIP, "RA---TAN-SIP" / "DEC--TAN-SIP"
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
    // 初始化 SIP (全 0, order=0, ap_order=0)
    for (int i = 0; i < 36; ++i) {
        result->sip.A[i]  = 0.0;
        result->sip.B[i]  = 0.0;
        result->sip.AP[i] = 0.0;
        result->sip.BP[i] = 0.0;
    }
    result->sip.order    = 0;
    result->sip.ap_order = 0;

    if (trans.order >= 2) {
        // V4.22 P1-5: 清零 trans.x00/y00
        // 平移完全由 CRVAL 吸收，避免 SIP AP/BP 常数项与 CRVAL 形成双重平移
        //   trans->x00 = 0.; trans->y00 = 0.;
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
        //     cd_inv = inv(trans 线性项)
        //     注意: 用 trans 线性项的逆, 不是 result->cd 的逆 (差 3600 倍)
        //     cd_inv[0][0] = invdet * cd[1][1], cd = trans 线性项
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
            //     A[i][j] = cd_inv · (trans.x_ij, trans.y_ij)
            //     单位: (像素/角秒) * (角秒/像素^(i+j)) = 1/像素^(i+j-1) (SIP 标准)
            //     索引: A[i*6+j], i=x幂, j=y幂
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

            // 5.3 SIP AP/BP: 网格反变换法
            //     流程:
            //       a. 生成像素网格 (u, v), 相对于图像中心
            //       b. 对网格应用 trans_for_sip -> IWC (角秒, 已清零 x00/y00)
            //       c. UV = cd_inv · IWC (畸变像素)
            //       d. 拟合 UV -> (u, v) 的多项式 = revtrans
            //       e. AP/BP = revtrans 系数, AP_10 -= 1, BP_01 -= 1
            const int NB_GRID = 7;  // 网格点数
            const int order = trans_for_sip.order;

            // 逆向 SIP 基函数: (i, j) for i+j >= 0 (含常数和线性)
            // revtrans 是完整多项式 (含常数和线性), AP/BP 提取所有项
            std::vector<std::pair<int,int>> inv_basis;
            for (int deg = 0; deg <= order; ++deg) {
                for (int i = deg; i >= 0; --i) {
                    int j = deg - i;
                    inv_basis.push_back({i, j});
                }
            }
            int n_inv_coef = (int)inv_basis.size();

            // 网格范围 (相对于图像中心, 像素)
            double u_range = img_width  / 2.0;
            double v_range = img_height / 2.0;

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
                    //    V4.22 P1-5: trans_for_sip 已清零 x00/y00, IWC 不含平移
                    double wx, wy;
                    apply_trans(trans_for_sip, u, v, &wx, &wy);

                    // c. UV = cd_inv · IWC (畸变像素)
                    //    transUV 用 cd_inv 作线性项, atApplyTrans 后 xygrid = UV
                    //    V4.22: 因 trans_for_sip.x00/y00=0, IWC 无平移,
                    //    UV 也无平移, revtrans 常数项 AP_00/BP_00 → 0
                    double uv_x = cd_inv_00 * wx + cd_inv_01 * wy;
                    double uv_y = cd_inv_10 * wx + cd_inv_11 * wy;

                    // 计算基函数值 (在 UV 上)
                    std::vector<double> bv(n_inv_coef);
                    for (int k = 0; k < n_inv_coef; ++k) {
                        bv[k] = eval_monomial(uv_x, uv_y,
                                              inv_basis[k].first,
                                              inv_basis[k].second);
                    }

                    // 累加正规方程 (拟合 UV → (u, v))
                    for (int p = 0; p < n_inv_coef; ++p) {
                        for (int q = 0; q < n_inv_coef; ++q) {
                            M_inv[p][q] += bv[p] * bv[q];
                        }
                        bx[p] += bv[p] * u;
                        by[p] += bv[p] * v;
                    }
                    ++n_points;
                }
            }

            // d. 求解 AP/BP 系数 (最小二乘)
            std::vector<double> capx = bx;
            std::vector<std::vector<double>> M_x = M_inv;
            std::vector<double> capy = by;
            std::vector<std::vector<double>> M_y = M_inv;

            bool ap_ok = false;
            if (gauss_solve_wcs(M_x, capx) && gauss_solve_wcs(M_y, capy)) {
                ap_ok = true;
            }

            if (ap_ok) {
                // e. 填充 AP/BP 系数
                //    AP[i*6+j] 对应 x^i * y^j
                //    约定: AP[1][0] = revtrans.x10 - 1, BP[0][1] = revtrans.y01 - 1
                for (int k = 0; k < n_inv_coef; ++k) {
                    int i = inv_basis[k].first;
                    int j = inv_basis[k].second;
                    int idx = i * 6 + j;
                    if (idx < 36) {
                        result->sip.AP[idx] = capx[k];
                        result->sip.BP[idx] = capy[k];
                    }
                }
                // 约定: 逆向 SIP 线性项减 1
                // AP[1][0] (idx=6) = revtrans.x10 - 1
                // BP[0][1] (idx=1) = revtrans.y01 - 1
                result->sip.AP[6] -= 1.0;  // AP_10
                result->sip.BP[1] -= 1.0;  // BP_01

                result->sip.ap_order = order;

                if (logger) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf),
                        "extract_wcs_sip: SIP AP/BP 网格反变换成功, ap_order=%d, "
                        "n_inv_coef=%d, n_grid=%d",
                        order, n_inv_coef, n_points);
                    logger->info(buf);
                }
            } else {
                if (logger) logger->warn("extract_wcs_sip: SIP AP/BP 拟合失败 (奇异矩阵), 仅输出前向 A/B");
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
    //    V4.20: TRANS: U(像素)→W(角秒), 残差 = apply_trans(U) - W (角秒)
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
    // 8. V4.29: 转换为标准 FITS WCS (Y-down)
    //    solver 内部用 Y-up 约定 (U.y = -(det_y - cy), 见 ipv_select.cpp:687),
    //    但 FITS/WCS 国际标准 Y 向下 (数据行号递增 = Y 增大)。
    //    需在输出边界做 Y-up → Y-down 转换, 使 IpvWcsResult 直接为标准 WCS。
    //
    //    推导 (U = (p_x, -p_y), CD_FITS = M·diag(1,-1)):
    //      CD: cd12, cd22 取反 (Y 相关列)
    //      SIP A (x输出): A' = A·(-1)^j            (仅输入 y 翻转)
    //      SIP B (y输出): B' = -B·(-1)^j           (输入+输出 y 翻转)
    //      SIP AP/BP:     同 A/B 规则
    //    CRVAL/CRPIX 不变 (中心点对称)。
    //    validate_wcs 不受影响: 中心点翻转后仍是中心; det=|cd11*cd22-cd12*cd21| 不变。
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

} // namespace ipv
