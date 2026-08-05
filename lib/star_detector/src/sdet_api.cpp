/**
 * @file sdet_api.cpp
 * @brief Star Detector API实现
 *
 * 主要功能：
 * 1. Moffat4 PSF拟合（Levenberg-Marquardt优化）
 * 2. 星点检测流水线（正常星+饱和星）
 * 3. FWHM/圆度过滤
 * 4. 半阈值饱和星检测
 */

#include "../include/star_detector.h"
#include "sdet_detector.h"
#include "sdet_image.h"
#include "sdet_log.h"
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <vector>
#include <type_traits>
#include <cmath>
#include <set>
#include <unordered_map>
#include <omp.h>

//   替代 IPv 手写 LM, 解决 More 缩放/对角预处理缺失导致的饱和星收敛率低
#include <gsl/gsl_multifit_nlinear.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// V4.64: LM 拟合收敛参数
#define LM_XTOL 1e-3
#define LM_GTOL 1e-3
#define LM_FTOL 1e-3
#define LM_MAX_ITER_ANGLE 20
#define LM_MIN_HALF_RADIUS 1
#define LM_MIN_LARGE_SAMPLING 3

#define INV_4_LOG2 0.36067376022224075

#define TWO_SQRT_2_LOG2 2.3548200450309493

struct StarDetectorHandle_s {
    StarDetectorInternal internal;
};

namespace {

// V4.54: Gaussian FWHM = 2*sqrt(2*ln2)*σ = 2.3548*σ
static const double GAUSSIAN_FWHM_FACTOR = 2.3548200450309493;
static const int NPARAMS = 7;

#define SDET_FIT_OK              0
#define SDET_FIT_NO_CONVERGENCE  1
#define SDET_FIT_INVALID_PARAMS  2
#define SDET_FIT_ITERATION_LIMIT 3

struct SamplePixel {
    double dx;
    double dy;
    double val;
};

struct InternalFitResult {
    int status;
    double B, A, cx, cy, sx, sy, theta;
    double fwhm_x, fwhm_y;
    double mad;
};

// V4.64: PSF 拟合

//   IPv 用 samples 数组 (已排除饱和像素), 等价于 mask=true 子集
//   保留 NbRows/NbCols 的概念, 但 samples 已预过滤
struct PSFFitData {
    size_t n;           // 样本数 (mask=true 的像素数)
    const double* y;    // 像素值数组 (mask=true 的像素)
    size_t NbRows;      // 矩阵行数 (未使用, 兼容保留)
    size_t NbCols;      // 矩阵列数 (未使用, 兼容保留)
    const SamplePixel* samples;  // IPv 样本 (dx, dy 已含 +0.5 偏移)
    double rmse;        // 输出: RMSE
};

// V4.64: PSF 拟合
//   参数: x[0]=B, x[1]=A, x[2]=x0, x[3]=y0, x[4]=SX=2σ², x[5]=fr, x[6]=alpha
//   SX = fabs(x[4]), r = 0.5*(cos(x[5])+1) ∈ [0,1], SY = r²*SX
//   tmpx = ca*(j+0.5-x0) - sa*(i+0.5-y0), IPv samples.dx 已含 +0.5
//   f[k] = B + A*exp(-(tmpx²/SX + tmpy²/SY)) - y[k]
static int sdet_gaussian_f(const gsl_vector* x, void* params, gsl_vector* f) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    size_t n = d->n;
    const double* y = d->y;
    const SamplePixel* samples = d->samples;
    double B = gsl_vector_get(x, 0);
    double A = gsl_vector_get(x, 1);
    double x0 = gsl_vector_get(x, 2);
    double y0 = gsl_vector_get(x, 3);
    double SX = fabs(gsl_vector_get(x, 4));
    double r = 0.5 * (cos(gsl_vector_get(x, 5)) + 1.0);
    double SY = r * r * SX;
    double alpha = gsl_vector_get(x, 6);
    double ca = cos(alpha), sa = sin(alpha);
    double sumres = 0.0;
    for (size_t k = 0; k < n; k++) {
        // IPv: samples[k].dx = x_pixel + 0.5 - cx, 残差中用 ddx = samples[k].dx - x0
        //   等价于
        //   samples[k].dx = j+0.5-x0_local (x0_local=x0 相对候选中心)

        double raw_x = samples[k].dx;  // 已含 +0.5, 相对候选中心
        double raw_y = samples[k].dy;
        double tmpx = ca * (raw_x - x0) - sa * (raw_y - y0);
        double tmpy = sa * (raw_x - x0) + ca * (raw_y - y0);
        double tmpc = exp(-(tmpx * tmpx / SX + tmpy * tmpy / SY));
        double val = B + A * tmpc - y[k];
        gsl_vector_set(f, k, val);
        sumres += val * val;
    }
    d->rmse = sqrt(sumres / n);
    return GSL_SUCCESS;
}

// V4.64: PSF 拟合
//   雅可比 (对 B, A, x0, y0, SX, fr, alpha):
//   dB = 1, dA = tmpc
//   dx0 = 2*A*tmpc*(tmpx/SX*ca + tmpy/SY*sa)
//   dy0 = 2*A*tmpc*(-tmpx/SX*sa + tmpy/SY*ca)
//   dSX = tmpc*A*( (tmpx/SX)² + (tmpy/(SX*r))² )
//   dfr = -A*tmpc*sc*tmpy²/SY/r   (sc = sin(fr))
//   dalpha = 2*A*tmpc*tmpx*tmpy*(1/SX - 1/SY)
static int sdet_gaussian_df(const gsl_vector* x, void* params, gsl_matrix* J) {
    PSFFitData* d = static_cast<PSFFitData*>(params);
    size_t n = d->n;
    const SamplePixel* samples = d->samples;
    double A = gsl_vector_get(x, 1);
    double x0 = gsl_vector_get(x, 2);
    double y0 = gsl_vector_get(x, 3);
    double SX = fabs(gsl_vector_get(x, 4));
    double r = 0.5 * (cos(gsl_vector_get(x, 5)) + 1.0);
    double SY = r * r * SX;
    double alpha = gsl_vector_get(x, 6);
    double ca = cos(alpha), sa = sin(alpha);
    double sc = sin(gsl_vector_get(x, 5));
    for (size_t k = 0; k < n; k++) {
        double raw_x = samples[k].dx;
        double raw_y = samples[k].dy;
        double tmpx = ca * (raw_x - x0) - sa * (raw_y - y0);
        double tmpy = sa * (raw_x - x0) + ca * (raw_y - y0);
        double tmpc = exp(-(tmpx * tmpx / SX + tmpy * tmpy / SY));
        gsl_matrix_set(J, k, 0, 1.0);  // dB
        gsl_matrix_set(J, k, 1, tmpc);  // dA
        double tmpd = 2.0 * A * tmpc * (tmpx / SX * ca + tmpy / SY * sa);  // dx0
        gsl_matrix_set(J, k, 2, tmpd);
        tmpd = 2.0 * A * tmpc * (-tmpx / SX * sa + tmpy / SY * ca);  // dy0
        gsl_matrix_set(J, k, 3, tmpd);
        tmpd = tmpc * A * (tmpx * tmpx / (SX * SX) + tmpy * tmpy / (SX * SX * r * r));  // dSX
        gsl_matrix_set(J, k, 4, tmpd);
        tmpd = -A * tmpc * sc * tmpy * tmpy / SY / r;  // dfr
        gsl_matrix_set(J, k, 5, tmpd);
        tmpd = 2.0 * A * tmpc * tmpx * tmpy * (1.0 / SX - 1.0 / SY);  // dalpha
        gsl_matrix_set(J, k, 6, tmpd);
    }
    return GSL_SUCCESS;
}

// reject_star 验证错误码（参考
enum SfError {
    SF_OK = 0,
    SF_FWHM_NEG = 1,
    SF_FWHM_TOO_SMALL = 2,
    SF_ROUNDNESS_BELOW_CRIT = 3,
    SF_RMSE_TOO_LARGE = 4,
    SF_FWHM_TOO_LARGE = 5
};

// reject_star 验证：检查 FWHM、roundness、RMSE、FWHM 过大
// has_saturated=true 时豁免 RMSE 检查（饱和星保留用于配准）


//   IPv:   cand_sx/cand_sy = candidates[i].sx/sy = Sr/Sc (与候选阶段 se->sx/se->sy 同源)
// V4.17 修复: RMSE 系数从 mad*3.0 改为 mad*1.4826
// V4.17 修复: FWHM_TOO_SMALL 从 1.0 放宽到 0.5 (长焦 H-alpha 星点 FWHM 可达 ~1px)
// V4.27: 圆度, 1.0]
SfError reject_star(const InternalFitResult& fit, bool has_saturated,
                    double cand_sx, double cand_sy) {
    // FWHM 检查：必须为正
    if (fit.fwhm_x <= 0.0 || fit.fwhm_y <= 0.0) return SF_FWHM_NEG;
    // FWHM 过小检查（<0.5 像素, 放宽以支持长焦窄带）
    if (fit.fwhm_x <= 0.5 || fit.fwhm_y <= 0.5) return SF_FWHM_TOO_SMALL;
    // 圆度, 1.0] (star_finder.c:105-108)

    // IPv Moffat4 有 theta 旋转, x/y 方向可互换, 用 min/max 比值处理方向
    double fwhm_max = std::max(fit.fwhm_x, fit.fwhm_y);
    double fwhm_min = std::min(fit.fwhm_x, fit.fwhm_y);
    double roundness_ratio = fwhm_min / fwhm_max;  // 始终 <= 1.0
    if (roundness_ratio < 0.5) return SF_ROUNDNESS_BELOW_CRIT;
    // RMSE 检查（饱和星豁免）：mad*1.4826/A > 0.2 拒绝
    if (!has_saturated && fit.A > 0.0) {
        double rmse_ratio = fit.mad * 1.4826 / fit.A;
        if (rmse_ratio > 0.2) return SF_RMSE_TOO_LARGE;
    }
    // V4.51: FWHM 上限完全

    //   se->sx/sy = 候选阶段 Gaussian sigma 估计 (高斯平滑图上二阶导数零交叉点 Sr/Sc)
    //   _2_SQRT_2_LOG2 = 2.3548 (FWHM=2.3548*sigma), KERNEL_SIZE = 2.0
    // V4.51 修复: 直接用候选阶段 cand_sx/cand_sy (Sr/Sc), 不再用 fit.sx/fit.sy 转换
    //   原因: 候选阶段 se->sx/se->sy 是候选阶段估计, 与拟合结果 fit.sx/fit.sy 量纲和含义不同
    //   Moffat4 sx ≠ Gaussian sigma (fwhm=0.87*sx vs 2.3548*sigma), 转换会引入误差
    //   候选 Sr/Sc 已含高斯平滑核展宽, 直接代入公式即可
    const double _2_SQRT_2_LOG2 = 2.3548200450309493;  // 2*sqrt(2*ln2)
    const double KERNEL_SIZE = 2.0;
    double se_smax = std::max(cand_sx, cand_sy);
    // 保护: 候选 sx/sy 可能退化 (如饱和星 edge-walking 失败), 此时跳过 FWHM 上限检查
    if (se_smax > KERNEL_SIZE) {
        double fwhm_limit = se_smax * _2_SQRT_2_LOG2 * (1.0 + 0.5 * std::log(se_smax / KERNEL_SIZE));
        if (fit.fwhm_x > fwhm_limit || fit.fwhm_y > fwhm_limit) return SF_FWHM_TOO_LARGE;
    }
    return SF_OK;
}

// 统一星点数据结构（正常星+饱和星共用）
struct StarRecord {
    double cx, cy;
    float flux;          // 正常星=振幅A，饱和星=PSF拟合A或0(失败)
    int is_saturated;     // 0=正常星，1=饱和星
    // Moffat4拟合数据（正常星+饱和星均有，饱和星拟合失败时为0）
    float fwhm_x, fwhm_y;
    float sx, sy, theta;
    float background, amplitude;
    // 饱和星圆盘拟合数据
    float r;             // 等效半径，正常星=0
    // 新增字段：mag 和 has_saturated
    float mag;           // -2.5*log10(A)，拟合失败时为NaN
    int has_saturated;   // 1=该星检测到饱和平台，0=正常星
    float cand_R;        // V4.59-diag: 候选阶段 R (mag box 半径, star_finder.c:529)
};

struct LMWorkspace {
    std::vector<double> fvec, fvec_new, J, JtJ, Jtf, delta, x_new, rhs, A_aug;
    void resize(int m, int n) {
        fvec.resize(m);
        fvec_new.resize(m);
        J.resize(m * n);
        JtJ.resize(n * n);
        Jtf.resize(n);
        delta.resize(n);
        x_new.resize(n);
        rhs.resize(n);
        A_aug.resize(n * n);
    }
};

bool sdet_gauss_solve(int n, const double* A, const double* b, double* x) {
    std::vector<double> aug(n * (n + 1));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            aug[i * (n + 1) + j] = A[i * n + j];
        aug[i * (n + 1) + n] = b[i];
    }
    for (int col = 0; col < n; col++) {
        int max_row = col;
        double max_val = std::abs(aug[col * (n + 1) + col]);
        for (int row = col + 1; row < n; row++) {
            double v = std::abs(aug[row * (n + 1) + col]);
            if (v > max_val) {
                max_val = v;
                max_row = row;
            }
        }
        if (max_val < 1e-30) return false;
        if (max_row != col) {
            for (int j = col; j <= n; j++)
                std::swap(aug[col * (n + 1) + j], aug[max_row * (n + 1) + j]);
        }
        double pivot = aug[col * (n + 1) + col];
        for (int row = col + 1; row < n; row++) {
            double factor = aug[row * (n + 1) + col] / pivot;
            for (int j = col; j <= n; j++)
                aug[row * (n + 1) + j] -= factor * aug[col * (n + 1) + j];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        x[i] = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++)
            x[i] -= aug[i * (n + 1) + j] * x[j];
        x[i] /= aug[i * (n + 1) + i];
    }
    return true;
}

// V4.64: sdet_lm_fit — 完全复刻
//   用 GSL trust-region LM (gsl_multifit_nlinear) 替代 IPv 手写 LM
//   参数化: {B, A, x0, y0, SX=2σ², fr=acos(2r-1), alpha}

//   输入: image (原始像素), rect (box), cx/cy (候选中心), bkg0, sat_threshold
//   输出: fit_result (B, A, cx, cy, sx, sy, theta, fwhm_x/y, mad=rmse)
//   返回: SDET_FIT_OK / SDET_FIT_NO_CONVERGENCE / SDET_FIT_INVALID_PARAMS
template <typename T>
static int sdet_lm_fit(const T* image, int width,
                       int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                       double cx, double cy, double bkg0, double sat_threshold,
                       const SamplePixel* samples, int m,
                       bool has_saturated, InternalFitResult* result) {
    const size_t n = (size_t)m;
    const size_t p = 7;  // Gaussian: 7 参数

    if (n <= p) return SDET_FIT_INVALID_PARAMS;  //

    // 准备 y 数组 (像素值)
    std::vector<double> y(n);
    for (size_t k = 0; k < n; k++) y[k] = samples[k].val;

    PSFFitData pdata;
    pdata.n = n;
    pdata.y = y.data();
    pdata.samples = samples;
    pdata.NbRows = 0;
    pdata.NbCols = 0;
    pdata.rmse = 0.0;


    int NbRows = rect_y1 - rect_y0;
    int NbCols = rect_x1 - rect_x0;
    int yc = NbRows / 2;  // 矩阵中心 y
    int xc = NbCols / 2;  // 矩阵中心 x

    double max_val = (double)image[(int)cy * width + (int)cx] - bkg0;
    if (max_val <= 0.0) return SDET_FIT_INVALID_PARAMS;
    double halfA = max_val * 0.5;
    double A0 = max_val;  //

    int ii1 = yc + LM_MIN_HALF_RADIUS;
    int ii2 = yc - LM_MIN_HALF_RADIUS;
    int jj1 = xc + LM_MIN_HALF_RADIUS;
    int jj2 = xc - LM_MIN_HALF_RADIUS;

    while (ii1 < NbRows - 1 && (double)image[(rect_y0 + ii1 + 1) * width + (rect_x0 + xc)] - bkg0 > halfA) {
        ii1++;
    }
    while (ii2 > 0 && (double)image[(rect_y0 + ii2 - 1) * width + (rect_x0 + xc)] - bkg0 > halfA) {
        ii2--;
    }
    while (jj1 < NbCols - 1 && (double)image[(rect_y0 + yc) * width + (rect_x0 + jj1 + 1)] - bkg0 > halfA) {
        jj1++;
    }
    while (jj2 > 0 && (double)image[(rect_y0 + yc) * width + (rect_x0 + jj2 - 1)] - bkg0 > halfA) {
        jj2--;
    }

    double FWHMx = (double)(jj1 - jj2);
    double FWHMy = (double)(ii1 - ii2);

    //   samples.dx - x0 = (x + 0.5 - cx) - x0, 要等于 (j + 0.5 - x0_box)
    //   j = x - rect_x0, x0_box = (jj1+jj2+1)/2 (矩阵坐标)
    //   x0_ipv (相对候选中心) = x0_box - (xc + 0.5) = (jj1+jj2+1)/2 - NbCols/2 - 0.5
    //   简化: x0_ipv = (jj1+jj2)/2.0 - xc + 0.5  (但, 让我精确对齐)

    //   IPv samples.dx = x_pixel + 0.5 - cx, x_pixel = rect_x0 + j
    //   samples.dx = rect_x0 + j + 0.5 - cx = j + 0.5 - (cx - rect_x0) = j + 0.5 - xc_box

    //   要等价: samples.dx - x0_ipv = j + 0.5 - x0_box
    //   j + 0.5 - xc_box - x0_ipv = j + 0.5 - x0_box
    //   x0_ipv = x0_box - xc_box = (jj1+jj2+1)/2.0 - xc
    double x0_init = (double)(jj1 + jj2 + 1) / 2.0 - xc;
    double y0_init = (double)(ii1 + ii2 + 1) / 2.0 - yc;

    //   FWHM = max(FWHMx, FWHMy), roundness = min/max, angle = 0
    //   否则: 复杂路径 (惯性矩阵 SVD), IPv 暂不实现, 用简单路径兜底
    double FWHM = std::max(FWHMx, FWHMy);
    double FWHM_min = std::min(FWHMx, FWHMy);
    if (FWHM < 1.0) FWHM = 1.0;  // 保护

    double roundness = (FWHM > 0) ? FWHM_min / FWHM : 1.0;
    if (roundness > 1.0) roundness = 1.0;
    if (roundness < 0.01) roundness = 0.01;
    if (roundness >= 0.999) roundness = 0.9;  //

    double SX_init = FWHM * FWHM * INV_4_LOG2;  // S_from_FWHM(Gaussian) = FWHM² * INV_4_LOG2 = 2σ²
    double fr_init = acos(2.0 * roundness - 1.0);
    double alpha_init = 0.0;  //

    double x_init[7] = { bkg0, A0, x0_init, y0_init, SX_init, fr_init, alpha_init };
    gsl_vector_view x = gsl_vector_view_array(x_init, p);

    gsl_multifit_nlinear_parameters fdf_params = gsl_multifit_nlinear_default_parameters();
    fdf_params.trs = gsl_multifit_nlinear_trs_lm;

    gsl_multifit_nlinear_fdf fdf;
    fdf.f = &sdet_gaussian_f;
    fdf.df = &sdet_gaussian_df;
    fdf.fvv = NULL;
    fdf.n = n;
    fdf.p = p;
    fdf.params = &pdata;

    const gsl_multifit_nlinear_type* nlin_type = gsl_multifit_nlinear_trust;
    gsl_multifit_nlinear_workspace* work = gsl_multifit_nlinear_alloc(nlin_type, &fdf_params, n, p);
    if (!work) return SDET_FIT_INVALID_PARAMS;

    gsl_multifit_nlinear_init(&x.vector, &fdf, work);

    int max_iter = LM_MAX_ITER_ANGLE * (has_saturated ? 3 : 1);

    int info;
    int status = gsl_multifit_nlinear_driver(max_iter, LM_XTOL, LM_GTOL, LM_FTOL,
                                              NULL, NULL, &info, work);

    if (status != GSL_SUCCESS) {
        gsl_multifit_nlinear_free(work);
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    #define GSL_FIT(i) gsl_vector_get(work->x, i)
    double B = GSL_FIT(0);
    double A = GSL_FIT(1);
    double x0 = GSL_FIT(2);
    double y0 = GSL_FIT(3);
    double SX = fabs(GSL_FIT(4));
    double fr = GSL_FIT(5);
    double alpha = GSL_FIT(6);

    double sx = sqrt(SX * 0.5);  // Gaussian: σ = sqrt(SX/2)
    double r = 0.5 * (cos(fr) + 1.0);
    double sy = sx * r;
    double fwhm_x = sx * TWO_SQRT_2_LOG2;
    double fwhm_y = sy * TWO_SQRT_2_LOG2;

    double angle_deg = -alpha * 180.0 / M_PI;
    while (fabs(angle_deg) > 90.0) {
        if (angle_deg > 0.0) angle_deg -= 180.0;
        else angle_deg += 180.0;
    }
    double theta = angle_deg * M_PI / 180.0;

    if (fabs(angle_deg) > 10000.0) {
        gsl_multifit_nlinear_free(work);
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    result->status = SDET_FIT_OK;
    result->B = B;
    result->A = A;
    result->cx = x0;  // 相对候选中心
    result->cy = y0;
    result->sx = sx;
    result->sy = sy;
    result->theta = theta;
    result->fwhm_x = fwhm_x;
    result->fwhm_y = fwhm_y;
    result->mad = pdata.rmse;

    gsl_multifit_nlinear_free(work);
    return SDET_FIT_OK;
}


//   Gaussian: model = B + A * exp(-Q), Q = r²/(2σ²), fwhm = 2.3548*σ
//   Moffat4:  model = B + A / (1+Q)^4, fwhm = 0.87*σ
//   同一星点 Gaussian A ≈ 峰值, Moffat4 A 偏高 (翼宽, 需更高 A 拟合峰值)
// V4.61: 保留 (sx, sy, theta) 参数化, 因 IPv 简单 LM 无参数缩放
//   新参数化 (SX=2σ², fr, alpha) 在 IPv LM 下 JtJ 对角线量级差 4 个数量级
//   (SX 项 ~2.4e6 vs B 项 ~100), 导致 SX 方向正则化不足, 收敛率从 40% 降到 24%
//   GSL trust-region LM 有 More 缩放能处理, IPv 手写 LM 无此机制
//   保留 +0.5 像素中心偏移 (samples 构建)
void sdet_gaussian_residual(double* params, int m, void* userdata, double* fvec) {
    const SamplePixel* samples = static_cast<const SamplePixel*>(userdata);
    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    if (sx <= 0 || sy <= 0) {
        for (int i = 0; i < m; i++) fvec[i] = 1e10;
        return;
    }

    double cos_t = std::cos(theta), sin_t = std::sin(theta);
    double cos2 = cos_t * cos_t, sin2 = sin_t * sin_t;
    double sin2t = std::sin(2.0 * theta);
    double inv_sx2 = 1.0 / (2.0 * sx * sx);
    double inv_sy2 = 1.0 / (2.0 * sy * sy);
    double p1 = cos2 * inv_sx2 + sin2 * inv_sy2;
    double p2 = sin2t / (4.0 * sx * sx) - sin2t / (4.0 * sy * sy);
    double p3 = sin2 * inv_sx2 + cos2 * inv_sy2;

    for (int i = 0; i < m; i++) {
        double ddx = samples[i].dx - x0;  // V4.60: samples.dx 已含 +0.5 偏移
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        if (Q < 0) {
            fvec[i] = 1e10;
            continue;
        }
        double model = B + A * std::exp(-Q);
        fvec[i] = samples[i].val - model;
    }
}

void sdet_gaussian_residual_and_jacobian(double* params, int m, void* userdata, double* fvec, double* J) {
    const SamplePixel* samples = static_cast<const SamplePixel*>(userdata);
    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    if (sx <= 0 || sy <= 0) {
        for (int i = 0; i < m; i++) {
            fvec[i] = 1e10;
            if (J) {
                for (int j = 0; j < NPARAMS; j++) J[i * NPARAMS + j] = 0.0;
            }
        }
        return;
    }

    double cos_t = std::cos(theta), sin_t = std::sin(theta);
    double cos2 = cos_t * cos_t, sin2 = sin_t * sin_t;
    double sin2t = std::sin(2.0 * theta);
    double cos2t = std::cos(2.0 * theta);
    double inv_sx2 = 1.0 / (2.0 * sx * sx);
    double inv_sy2 = 1.0 / (2.0 * sy * sy);
    double p1 = cos2 * inv_sx2 + sin2 * inv_sy2;
    double p2 = sin2t / (4.0 * sx * sx) - sin2t / (4.0 * sy * sy);
    double p3 = sin2 * inv_sx2 + cos2 * inv_sy2;

    double inv_sx3 = 1.0 / (sx * sx * sx);
    double inv_sy3 = 1.0 / (sy * sy * sy);

    double dp1_dtheta = -sin2t * inv_sx2 + sin2t * inv_sy2;
    double dp2_dtheta = cos2t * inv_sx2 - cos2t * inv_sy2;
    double dp3_dtheta = sin2t * inv_sx2 - sin2t * inv_sy2;

    for (int i = 0; i < m; i++) {
        double ddx = samples[i].dx - x0;  // V4.60: samples.dx 已含 +0.5 偏移
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;

        if (Q < 0) {
            fvec[i] = 1e10;
            if (J) {
                for (int j = 0; j < NPARAMS; j++) J[i * NPARAMS + j] = 0.0;
            }
            continue;
        }

        // V4.54: Gaussian model = B + A * exp(-Q)
        double exp_Q = std::exp(-Q);
        double model = B + A * exp_Q;
        fvec[i] = samples[i].val - model;

        if (J) {
            double dx2 = ddx * ddx;
            double dxy = ddx * ddy;
            double dy2 = ddy * ddy;
            double p1_dx_p2_dy = p1 * ddx + p2 * ddy;
            double p2_dx_p3_dy = p2 * ddx + p3 * ddy;

            // V4.54: Gaussian 雅可比
            //   dmodel/dB = 1, dmodel/dA = exp(-Q)
            //   dmodel/dx0 = A * exp(-Q) * 2*(p1*dx+p2*dy)
            //   dmodel/dsx = A * exp(-Q) * (cos²*dx²+sin2θ*dx*dy+sin²*dy²) / sx³
            //   dmodel/dθ  = A * exp(-Q) * (-dQ/dθ)
            //   J = -dmodel/dparam (因为 fvec = val - model)
            J[i * NPARAMS + 0] = -1.0;
            J[i * NPARAMS + 1] = -exp_Q;
            J[i * NPARAMS + 2] = -2.0 * A * exp_Q * p1_dx_p2_dy;
            J[i * NPARAMS + 3] = -2.0 * A * exp_Q * p2_dx_p3_dy;
            J[i * NPARAMS + 4] = -A * exp_Q * (cos2 * dx2 + sin2t * dxy + sin2 * dy2) * inv_sx3;
            J[i * NPARAMS + 5] = -A * exp_Q * (sin2 * dx2 - sin2t * dxy + cos2 * dy2) * inv_sy3;
            double dQ_dtheta = dp1_dtheta * dx2 + 2.0 * dp2_dtheta * dxy + dp3_dtheta * dy2;
            J[i * NPARAMS + 6] = A * exp_Q * dQ_dtheta;
        }
    }
}

int sdet_lm_solve(int m, int n, double* x, void* userdata,
                  void (*rj_func)(double*, int, void*, double*, double*),
                  double tol, int max_iter, LMWorkspace* ws) {
    std::vector<double> fvec_local, fvec_new_local, J_local, JtJ_local, Jtf_local, delta_local, x_new_local, rhs_local, A_local;
    double* fvec_ptr;
    double* fvec_new_ptr;
    double* J_ptr;
    double* JtJ_ptr;
    double* Jtf_ptr;
    double* delta_ptr;
    double* x_new_ptr;
    double* rhs_ptr;
    double* A_ptr;

    if (ws) {
        ws->resize(m, n);
        fvec_ptr = ws->fvec.data();
        fvec_new_ptr = ws->fvec_new.data();
        J_ptr = ws->J.data();
        JtJ_ptr = ws->JtJ.data();
        Jtf_ptr = ws->Jtf.data();
        delta_ptr = ws->delta.data();
        x_new_ptr = ws->x_new.data();
        rhs_ptr = ws->rhs.data();
        A_ptr = ws->A_aug.data();
    } else {
        fvec_local.resize(m);
        fvec_new_local.resize(m);
        J_local.resize(m * n);
        JtJ_local.resize(n * n);
        Jtf_local.resize(n);
        delta_local.resize(n);
        x_new_local.resize(n);
        rhs_local.resize(n);
        A_local.resize(n * n);
        fvec_ptr = fvec_local.data();
        fvec_new_ptr = fvec_new_local.data();
        J_ptr = J_local.data();
        JtJ_ptr = JtJ_local.data();
        Jtf_ptr = Jtf_local.data();
        delta_ptr = delta_local.data();
        x_new_ptr = x_new_local.data();
        rhs_ptr = rhs_local.data();
        A_ptr = A_local.data();
    }

    double lambda = 1e-3;

    rj_func(x, m, userdata, fvec_ptr, J_ptr);
    double cost = 0;
    for (int i = 0; i < m; i++) cost += fvec_ptr[i] * fvec_ptr[i];

    // 提前失败检测：初始cost太大说明初始参数完全不匹配
    if (cost > 1e10 * m) {
        return SDET_FIT_NO_CONVERGENCE;
    }

    // V4.63: 移除自创 stall_count,

    for (int iter = 0; iter < max_iter; iter++) {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                double sum = 0;
                for (int k = 0; k < m; k++)
                    sum += J_ptr[k * n + i] * J_ptr[k * n + j];
                JtJ_ptr[i * n + j] = sum;
            }

        for (int i = 0; i < n; i++) {
            double sum = 0;
            for (int k = 0; k < m; k++)
                sum += J_ptr[k * n + i] * fvec_ptr[k];
            Jtf_ptr[i] = sum;
        }

        for (int i = 0; i < n * n; i++) A_ptr[i] = JtJ_ptr[i];
        for (int i = 0; i < n; i++) A_ptr[i * n + i] += lambda;

        for (int i = 0; i < n; i++) rhs_ptr[i] = -Jtf_ptr[i];

        if (!sdet_gauss_solve(n, A_ptr, rhs_ptr, delta_ptr)) {
            lambda *= 10.0;
            continue;
        }

        double norm_delta = 0, norm_x = 0;
        for (int i = 0; i < n; i++) {
            norm_delta += delta_ptr[i] * delta_ptr[i];
            norm_x += x[i] * x[i];
        }
        norm_delta = std::sqrt(norm_delta);
        norm_x = std::sqrt(norm_x);

        if (norm_delta < tol * (norm_x + 1e-30)) {
            return SDET_FIT_OK;
        }

        for (int i = 0; i < n; i++) x_new_ptr[i] = x[i] + delta_ptr[i];
        rj_func(x_new_ptr, m, userdata, fvec_new_ptr, nullptr);
        double cost_new = 0;
        for (int i = 0; i < m; i++) cost_new += fvec_new_ptr[i] * fvec_new_ptr[i];

        if (cost_new < cost) {
            for (int i = 0; i < n; i++) x[i] = x_new_ptr[i];
            // V4.61: 回退到 V4.57 参数化保护 (x[4]=sx, x[5]=sy, x[6]=theta)
            if (x[4] < 0.3) x[4] = 0.3;
            if (x[5] < 0.3) x[5] = 0.3;
            if (x[1] < 0.0) x[1] = 0.0;
            rj_func(x, m, userdata, fvec_ptr, J_ptr);
            cost = 0;
            for (int i = 0; i < m; i++) cost += fvec_ptr[i] * fvec_ptr[i];
            lambda *= 0.1;
            // V4.63: 移除自创 stall_count 提前终止,

        } else {
            lambda *= 10.0;
        }
    }

    return SDET_FIT_ITERATION_LIMIT;
}

// V4.61: 回退到 V4.57 (sx, sy, theta) 参数化, 保留 +0.5 偏移 (samples 已含)
double sdet_compute_trimmed_mad(const SamplePixel* samples, int m, const double* params) {
    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    double cos_t = std::cos(theta), sin_t = std::sin(theta);
    double cos2 = cos_t * cos_t, sin2 = sin_t * sin_t;
    double sin2t = std::sin(2.0 * theta);
    double inv_sx2 = 1.0 / (2.0 * sx * sx);
    double inv_sy2 = 1.0 / (2.0 * sy * sy);
    double p1 = cos2 * inv_sx2 + sin2 * inv_sy2;
    double p2 = sin2t / (4.0 * sx * sx) - sin2t / (4.0 * sy * sy);
    double p3 = sin2 * inv_sx2 + cos2 * inv_sy2;

    std::vector<double> abs_res(m);
    for (int i = 0; i < m; i++) {
        double ddx = samples[i].dx - x0;  // V4.60: samples.dx 已含 +0.5 偏移
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        // V4.54: Gaussian model
        double model = B + A * std::exp(-std::max(Q, 0.0));
        abs_res[i] = std::abs(samples[i].val - model);
    }

    std::sort(abs_res.begin(), abs_res.end());
    int lo = static_cast<int>(m * 0.1);
    int hi = static_cast<int>(m * 0.9);
    if (lo >= hi) return abs_res[m / 2];
    double sum = 0;
    for (int i = lo; i < hi; i++) sum += abs_res[i];
    return sum / (hi - lo);
}

// V4.27 阶段B: 背景噪声估计 (FnNoise1_ushort 算法)
// 算法: 行差分 -> sigma-clip(3次,5.0) -> stdev -> 中位数 -> *0.7071
// 注意: sdet_robust_mad 返回值已含 *1.4826 (即 sigma 估计), 直接用作 dsigma
template <typename T>
static T sdet_compute_bgnoise(const T* img, int width, int height) {
    if (width < 2 || height < 1) return 0.0f;
    std::vector<T> row_stdevs(height);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int y = 0; y < height; y++) {
        const T* row = img + y * width;
        std::vector<T> diffs(width - 1);
        for (int x = 1; x < width; x++) {
            diffs[x-1] = row[x] - row[x-1];
        }
        T dmed, dsigma;
        if constexpr (std::is_same_v<T, float>) {
            dmed = sdet_robust_median(diffs.data(), (int)diffs.size());
            dsigma = sdet_robust_mad(diffs.data(), (int)diffs.size());
        } else {
            dmed = sdet_robust_median_d(diffs.data(), (int)diffs.size());
            dsigma = sdet_robust_mad_d(diffs.data(), (int)diffs.size());
        }
        std::vector<T> clipped;
        for (int iter = 0; iter < 3; iter++) {
            clipped.clear();
            T lo = dmed - T(5.0) * dsigma;
            T hi = dmed + T(5.0) * dsigma;
            for (T v : diffs) {
                if (v >= lo && v <= hi) clipped.push_back(v);
            }
            if (clipped.size() < 2) break;
            if constexpr (std::is_same_v<T, float>) {
                dmed = sdet_robust_median(clipped.data(), (int)clipped.size());
                dsigma = sdet_robust_mad(clipped.data(), (int)clipped.size());
            } else {
                dmed = sdet_robust_median_d(clipped.data(), (int)clipped.size());
                dsigma = sdet_robust_mad_d(clipped.data(), (int)clipped.size());
            }
        }
        if (clipped.size() < 2) {
            row_stdevs[y] = T(0);
        } else {
            double sum = 0.0;
            for (T v : clipped) sum += (double)v;
            double mean = sum / clipped.size();
            double var = 0.0;
            for (T v : clipped) var += ((double)v - mean) * ((double)v - mean);
            row_stdevs[y] = (T)std::sqrt(var / clipped.size());
        }
    }
    T med_stdev;
    if constexpr (std::is_same_v<T, float>) {
        med_stdev = sdet_robust_median(row_stdevs.data(), height);
    } else {
        med_stdev = sdet_robust_median_d(row_stdevs.data(), height);
    }
    return med_stdev * T(0.70710678);  // 1/sqrt(2)
}

// V4.55: 新增 init_sx/init_sy 参数, 用候选阶段 Sr/Sc 作为初始 σ
// V4.58: 新增 bg_init 参数, 用全局中位数作为 B 初始值
template <typename T>
int sdet_moffat4_fit(const T* image, int width, int height,
                     double cx, double cy,
                     int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                     InternalFitResult* result, LMWorkspace* ws = nullptr,
                     double sat_threshold = 0.0, double init_sx = 0.0, double init_sy = 0.0,
                     double bg_init = 0.0) {
    std::memset(result, 0, sizeof(InternalFitResult));
    result->status = SDET_FIT_INVALID_PARAMS;

    int rw = rect_x1 - rect_x0;
    int rh = rect_y1 - rect_y0;

    if (rw * rh < 9) return SDET_FIT_INVALID_PARAMS;
    if (rect_x0 < 0 || rect_y0 < 0 || rect_x1 > width || rect_y1 > height)
        return SDET_FIT_INVALID_PARAMS;

    std::vector<SamplePixel> samples;
    samples.reserve(rw * rh);
    for (int y = rect_y0; y < rect_y1; y++) {
        for (int x = rect_x0; x < rect_x1; x++) {
            double val = static_cast<double>(image[y * width + x]);
            // 饱和 mask：sat_threshold > 0 时排除饱和像素（pixel >= sat_threshold）
            if (sat_threshold > 0.0 && val >= sat_threshold) continue;
            SamplePixel sp;


            //   IPv:   sp.dx = x+0.5-cx, 残差中 ddx = sp.dx - x0 = x+0.5-cx-x0
            //   等价于 j+0.5-x0 (j=x-cx+R, x0_box=x0_ipv+R, R=box半宽)
            sp.dx = static_cast<double>(x) + 0.5 - cx;
            sp.dy = static_cast<double>(y) + 0.5 - cy;
            sp.val = val;
            samples.push_back(sp);
        }
    }
    int m = static_cast<int>(samples.size());

    // 饱和 mask 拟合时，有效像素数 ≤ 8 视为失败
    if (sat_threshold > 0.0 && m <= 8) {
        sdet_log(SDET_LOG_DEBUG, "SDET", "Moffat4 mask fit: too few samples (%d) after sat mask", m);
        result->status = SDET_FIT_INVALID_PARAMS;
        return SDET_FIT_INVALID_PARAMS;
    }

    std::vector<double> vals(m);
    for (int i = 0; i < m; i++) vals[i] = samples[i].val;
    std::sort(vals.begin(), vals.end());

    double median_val = (m % 2 == 0)
        ? (vals[m / 2 - 1] + vals[m / 2]) / 2.0
        : vals[m / 2];

    std::vector<double> lower_half;
    lower_half.reserve(m / 2);
    for (int i = 0; i < m; i++) {
        if (vals[i] < median_val) lower_half.push_back(vals[i]);
    }
    if (lower_half.empty()) lower_half.push_back(median_val);

    int nh = static_cast<int>(lower_half.size());
    double med_lh = (nh % 2 == 0)
        ? (lower_half[nh / 2 - 1] + lower_half[nh / 2]) / 2.0
        : lower_half[nh / 2];

    std::vector<double> abs_dev_lh(nh);
    for (int i = 0; i < nh; i++) abs_dev_lh[i] = std::abs(lower_half[i] - med_lh);
    std::sort(abs_dev_lh.begin(), abs_dev_lh.end());
    double mad_lh = (nh % 2 == 0)
        ? (abs_dev_lh[nh / 2 - 1] + abs_dev_lh[nh / 2]) / 2.0
        : abs_dev_lh[nh / 2];

    double threshold = 2.0 * 1.4826 * mad_lh;
    std::vector<double> filtered;
    filtered.reserve(nh);
    for (int i = 0; i < nh; i++) {
        if (std::abs(lower_half[i] - med_lh) <= threshold)
            filtered.push_back(lower_half[i]);
    }
    if (filtered.empty()) filtered.push_back(med_lh);

    int nf = static_cast<int>(filtered.size());
    std::sort(filtered.begin(), filtered.end());
    double bkg0 = (nf % 2 == 0)
        ? (filtered[nf / 2 - 1] + filtered[nf / 2]) / 2.0
        : filtered[nf / 2];

    // V4.58 回退: B 初始值仍用局部 lower_half 中位数 (bkg0), 不用全局中位数
    //   原因: V4.58 试验用全局中位数导致 NGC4945 顺序 100%→21%, 偏差 1→4
    //   IPv 手写 LM 与 GSL trust-region LM 收敛行为不同, 全局中位数初始值在 IPv 中导致 B 收敛偏差
    // (void)bg_init;  // V4.58 参数保留但不使用, 避免签名变更

    double max_val = -1e30;
    for (int i = 0; i < m; i++)
        if (samples[i].val > max_val) max_val = samples[i].val;

    double A0 = max_val - bkg0;
    if (A0 <= 0) return SDET_FIT_INVALID_PARAMS;

    // V4.55: 用候选 Sr/Sc 作为初始 σ, 退化时用 0.15*rw
    //   Gaussian: FWHM=2.3548*σ, Sr/Sc 已是 σ 估计 (高斯平滑图零交叉点距离)
    // V4.66: 用 GSL trust-region LM + halfA 边界搜索初始化, 替代 IPv 手写 LM
    //   GSL LM 有 More 缩放/对角预处理, 能处理参数量级差异 (V4.60 失败根因)
    //   V4.64 用 init_sx*2.3548 转 FWHM 不对齐, V4.66 用 halfA 边界搜索

    // V4.66: has_saturated = (sat_threshold > 0.0 且有像素被排除)

    bool has_saturated = (sat_threshold > 0.0 && m < rw * rh);

    int gsl_status = sdet_lm_fit<T>(image, width,
                                    rect_x0, rect_y0, rect_x1, rect_y1,
                                    cx, cy, bkg0, sat_threshold,
                                    samples.data(), m, has_saturated, result);

    if (gsl_status != SDET_FIT_OK) {
        return gsl_status;
    }

    // V4.66: 保留 NaN/A 保护
    if (!std::isfinite(result->B) || !std::isfinite(result->A) ||
        !std::isfinite(result->cx) || !std::isfinite(result->cy) ||
        !std::isfinite(result->sx) || !std::isfinite(result->sy) ||
        !std::isfinite(result->theta) || result->A <= 0.0) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    // 转换为图像绝对坐标
    result->cx += cx;
    result->cy += cy;

    return SDET_FIT_OK;
}

// 半阈值饱和星检测
struct SaturatedCandidate {
    double cx, cy;
    float r;
    int pixel_count;
};

// edge-walking 几何中心定位（参考
// 沿饱和平台四方向走边缘，返回几何中心
// sat 为饱和阈值，pixel > sat 视为饱和平台内
// 接近图像边界（<2px）返回 false（丢弃该饱和星）
bool edge_walking_center(const float* fimg, int width, int height,
                         int xx, int yy, float sat,
                         double& center_x, double& center_y) {
    // 起点边界检查：距边界 <2px 直接丢弃
    if (xx < 2 || yy < 2 || xx >= width - 2 || yy >= height - 2)
        return false;

    // 向右走：找到最右侧 pixel > sat 的像素
    int xr = xx;
    for (int x = xx + 1; x < width - 1; x++) {
        if (fimg[yy * width + x] <= sat) break;
        xr = x;
    }
    if (xr >= width - 2) return false;

    // 向左走
    int xl = xx;
    for (int x = xx - 1; x >= 1; x--) {
        if (fimg[yy * width + x] <= sat) break;
        xl = x;
    }
    if (xl < 2) return false;

    // 向下走
    int yd = yy;
    for (int y = yy + 1; y < height - 1; y++) {
        if (fimg[y * width + xx] <= sat) break;
        yd = y;
    }
    if (yd >= height - 2) return false;

    // 向上走
    int yu = yy;
    for (int y = yy - 1; y >= 1; y--) {
        if (fimg[y * width + xx] <= sat) break;
        yu = y;
    }
    if (yu < 2) return false;

    // 几何中心 = (xr+xl)/2, (yd+yu)/2
    center_x = (double)(xr + xl) / 2.0;
    center_y = (double)(yd + yu) / 2.0;
    return true;
}

// 饱和星检测：阈值 70% 动态范围 + 连通域 + edge-walking 中心
// out_sat_threshold 输出饱和阈值，供后续 PSF mask 拟合使用
// out_img_median 输出全局中位数背景，供饱和星 mag 计算使用 (V4.41)
void sdet_detect_saturated_stars(const float* fimg, int width, int height,
                                  std::vector<SaturatedCandidate>& sat_stars,
                                  float& out_sat_threshold,
                                  float& out_img_median) {
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = (size_t)width * height;

    // 计算动态范围 + median (V4.32:, bg 用 median 而非 img_min)
    float img_min = 1e30f, img_max = -1e30f;
    #pragma omp parallel for reduction(min:img_min) reduction(max:img_max) schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        if (fimg[i] < img_min) img_min = fimg[i];
        if (fimg[i] > img_max) img_max = fimg[i];
    }
    // V4.32: 计算 median 作为背景估计
    std::vector<float> img_copy(fimg, fimg + n);
    std::nth_element(img_copy.begin(), img_copy.begin() + n / 2, img_copy.end());
    float img_median = img_copy[n / 2];
    out_img_median = img_median;  // V4.41: 输出供饱和星 mag 计算

    // 饱和阈值：median + dynrange * 0.7

    float bg = img_median;  // V4.32:, 用 median 替代 img_min
    float dynrange = img_max - bg;
    float minsatlevel = dynrange * 0.7f;
    float sat_threshold = bg + minsatlevel;
    float satrange = dynrange * 0.1f;  // 平台判定范围（保留）

    out_sat_threshold = sat_threshold;

    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star threshold: %.1f (bg=%.1f dynrange=%.1f minsatlevel=%.1f satrange=%.1f)",
             sat_threshold, bg, dynrange, minsatlevel, satrange);

    // 二值化：pixel > sat_threshold
    std::vector<float> binary(n, 0.0f);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        if (fimg[i] > sat_threshold) binary[i] = 1.0f;
    }

    // 连通域分析
    ConnectedComponent* components = nullptr;
    int comp_count = 0;
    sdet_find_connected_components(binary.data(), width, height, &components, &comp_count);

    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star connected components: %d", comp_count);

    // 对每个连通域计算 edge-walking 几何中心
    // V4.35: 放宽过滤条件 (count<=4→<=1, bw/bh<2→<1, ar>2→>3)
    // 原过滤过严导致 Galaxy_Center 425连通域→28饱和星,
    // V4.46: 添加, 过滤小饱和块 (对齐 star_finder.c:271-274,332-333,346)

    //   IPv: meanhigh = 连通域内像素平均值(等价于 3x3 邻域对小连通域)
    int ew_fail_count = 0;
    int meanhigh_filtered = 0;
    for (int i = 0; i < comp_count; i++) {
        if (components[i].count <= 1) continue;
        int bw = components[i].x1 - components[i].x0 + 1;
        int bh = components[i].y1 - components[i].y0 + 1;
        if (bw < 1 || bh < 1) continue;
        float ar = (float)std::max(bw, bh) / std::max(std::min(bw, bh), 1);
        if (ar > 3.0f) continue;

        // 加权重心作为 edge-walking 起点
        double sum_wx = 0, sum_wy = 0, sum_w = 0;
        for (int j = 0; j < components[i].count; j++) {
            float val = fimg[components[i].py[j] * width + components[i].px[j]];
            sum_wx += components[i].px[j] * val;
            sum_wy += components[i].py[j] * val;
            sum_w += val;
        }

        // V4.46:
        //   需要更精确的 dynrange 计算或改为在 peaker 候选阶段检查
        // float meanhigh = ...; if (meanhigh - bg < minsatlevel) { meanhigh_filtered++; continue; }

        int start_x = (sum_w > 0) ? (int)(sum_wx / sum_w + 0.5) : (components[i].x0 + components[i].x1) / 2;
        int start_y = (sum_w > 0) ? (int)(sum_wy / sum_w + 0.5) : (components[i].y0 + components[i].y1) / 2;

        // edge-walking 几何中心
        double ew_cx, ew_cy;
        if (!edge_walking_center(fimg, width, height, start_x, start_y, sat_threshold, ew_cx, ew_cy)) {
            ew_fail_count++;
            continue;  // 接近边界，丢弃
        }

        // 等效半径 r = sqrt(pixel_count / π)
        float r = std::sqrt((float)components[i].count / (float)M_PI);

        sat_stars.push_back({ew_cx, ew_cy, r, components[i].count});
    }
    sdet_free_connected_components(components, comp_count);

    auto t1 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "SDET", "Saturated stars: %d (meanhigh filtered: %d, edge-walking failed: %d, %.1f ms)",
             (int)sat_stars.size(), meanhigh_filtered, ew_fail_count, std::chrono::duration<double, std::milli>(t1 - t0).count());
}

// 可选输出参数名解析
enum ExtraField {
    EXTRA_FWHM_X = 0,
    EXTRA_FWHM_Y,
    EXTRA_SX,
    EXTRA_SY,
    EXTRA_THETA,
    EXTRA_BACKGROUND,
    EXTRA_AMPLITUDE,
    EXTRA_R,
    EXTRA_CAND_R,
    EXTRA_UNKNOWN
};

ExtraField parse_extra_name(const char* name) {
    if (strcmp(name, "fwhm_x") == 0) return EXTRA_FWHM_X;
    if (strcmp(name, "fwhm_y") == 0) return EXTRA_FWHM_Y;
    if (strcmp(name, "sx") == 0) return EXTRA_SX;
    if (strcmp(name, "sy") == 0) return EXTRA_SY;
    if (strcmp(name, "theta") == 0) return EXTRA_THETA;
    if (strcmp(name, "background") == 0) return EXTRA_BACKGROUND;
    if (strcmp(name, "amplitude") == 0) return EXTRA_AMPLITUDE;
    if (strcmp(name, "r") == 0) return EXTRA_R;
    if (strcmp(name, "cand_R") == 0) return EXTRA_CAND_R;
    return EXTRA_UNKNOWN;
}

float get_extra_field(const StarRecord& star, ExtraField field) {
    // 饱和星现在也有 PSF 拟合数据，不再填 -1.0
    // 拟合失败时 fwhm_x 等为 0.0
    switch (field) {
        case EXTRA_FWHM_X:     return star.fwhm_x;
        case EXTRA_FWHM_Y:     return star.fwhm_y;
        case EXTRA_SX:         return star.sx;
        case EXTRA_SY:         return star.sy;
        case EXTRA_THETA:      return star.theta;
        case EXTRA_BACKGROUND: return star.background;
        case EXTRA_AMPLITUDE:  return star.amplitude;
        case EXTRA_R:          return star.r;
        case EXTRA_CAND_R:     return star.cand_R;
        default:               return 0.0f;
    }
}

// 去重：饱和星与正常星重叠时丢弃饱和星
void sdet_dedup_stars(std::vector<StarRecord>& stars) {
    // 分离正常星和饱和星
    std::vector<int> normal_idx, sat_idx;
    for (int i = 0; i < (int)stars.size(); i++) {
        if (stars[i].is_saturated) sat_idx.push_back(i);
        else normal_idx.push_back(i);
    }

    // 构建正常星网格
    const int grid_sz = 2;
    struct GK { int gx, gy; bool operator==(const GK& o) const { return gx == o.gx && gy == o.gy; } };
    struct GKH { size_t operator()(const GK& k) const { return (size_t)k.gx * 1000003ULL + (size_t)k.gy; } };
    std::unordered_map<GK, std::vector<int>, GKH> normal_grid;
    for (int idx : normal_idx) {
        int gx = (int)stars[idx].cx / grid_sz;
        int gy = (int)stars[idx].cy / grid_sz;
        normal_grid[{gx, gy}].push_back(idx);
    }

    // V4.35: 饱和星与正常星去重改为丢弃正常星（保留饱和星,

    // V4.52 修复: normal_deleted_by_sat 大小改为 stars.size(), 因为 V4.52 后 stars 布局混合
    //   (阶段8 同时添加正常星和饱和星, normal_idx 中的 stars 索引不再连续 [0, normal_count))
    std::vector<uint8_t> normal_deleted_by_sat(stars.size(), 0);
    for (int si = 0; si < (int)sat_idx.size(); si++) {
        int i = sat_idx[si];
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                auto it = normal_grid.find({gx + dx, gy + dy});
                if (it == normal_grid.end()) continue;
                for (int ni : it->second) {
                    double ddx = stars[i].cx - stars[ni].cx;
                    double ddy = stars[i].cy - stars[ni].cy;
                    if (ddx * ddx + ddy * ddy < 4.0) {
                        normal_deleted_by_sat[ni] = 1;
                    }
                }
            }
        }
    }

    // 饱和星之间去重：保留r较大的
    std::vector<uint8_t> sat_deleted(sat_idx.size(), 0);
    std::unordered_map<GK, std::vector<int>, GKH> sat_grid;
    for (int si = 0; si < (int)sat_idx.size(); si++) {
        if (sat_deleted[si]) continue;
        int i = sat_idx[si];
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        sat_grid[{gx, gy}].push_back(si);
    }
    for (int si = 0; si < (int)sat_idx.size(); si++) {
        if (sat_deleted[si]) continue;
        int i = sat_idx[si];
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                auto it = sat_grid.find({gx + dx, gy + dy});
                if (it == sat_grid.end()) continue;
                for (int sj : it->second) {
                    if (sj == si || sat_deleted[sj]) continue;
                    int j = sat_idx[sj];
                    double ddx = stars[i].cx - stars[j].cx;
                    double ddy = stars[i].cy - stars[j].cy;
                    if (ddx * ddx + ddy * ddy < 4.0) {
                        // 保留r较大的 (V4.52: r=0.0f 时稳定, 后续按 flux 排序)
                        if (stars[i].r >= stars[j].r) sat_deleted[sj] = 1;
                        else { sat_deleted[si] = 1; goto next_sat; }
                    }
                }
            }
        }
        next_sat:;
    }

    // 正常星之间去重（V4.35: 合并饱和星去重标记）
    // V4.52 修复: normal_deleted 以 stars 索引为下标, 大小 = stars.size()
    //   normal_grid 存储的是 stars 索引 (不是 normal_idx 位置索引)
    std::vector<uint8_t> normal_deleted = normal_deleted_by_sat;
    for (int ni = 0; ni < (int)normal_idx.size(); ni++) {
        int i = normal_idx[ni];  // stars 索引
        if (normal_deleted[i]) continue;
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                auto it = normal_grid.find({gx + dx, gy + dy});
                if (it == normal_grid.end()) continue;
                for (int nj : it->second) {  // nj 是 stars 索引
                    if (nj == i || normal_deleted[nj]) continue;
                    int j = nj;  // V4.52: nj 已经是 stars 索引
                    double ddx = stars[i].cx - stars[j].cx;
                    double ddy = stars[i].cy - stars[j].cy;
                    if (ddx * ddx + ddy * ddy <= 1.0) normal_deleted[nj] = 1;
                }
            }
        }
    }

    // 合并结果
    std::vector<StarRecord> result;
    for (int si = 0; si < (int)sat_idx.size(); si++) {
        if (!sat_deleted[si]) result.push_back(stars[sat_idx[si]]);
    }
    for (int ni = 0; ni < (int)normal_idx.size(); ni++) {
        if (!normal_deleted[normal_idx[ni]]) result.push_back(stars[normal_idx[ni]]);
    }

    stars = std::move(result);
}

// V4.53: 排序

//   mag = -2.5*log10(box_sum), box_sum 越大 mag 越小 (越亮)
//   NaN mag (box_sum<=0) 排到最后
//   注: plate solver (ipv_select) 自己按 mag 重排, 此处排序仅供 API 输出一致性
void sdet_sort_stars(std::vector<StarRecord>& stars) {
    std::stable_sort(stars.begin(), stars.end(), [](const StarRecord& a, const StarRecord& b) {
        bool a_nan = std::isnan(a.mag);
        bool b_nan = std::isnan(b.mag);
        if (a_nan && b_nan) return false;
        if (a_nan) return false;  // NaN 排最后
        if (b_nan) return true;
        return a.mag < b.mag;  // mag 升序 (越小越亮)
    });
}

} // anonymous namespace

SDET_EXPORT StarDetectorHandle sdet_create(const SDetParams *params)
{
    StarDetectorHandle_s *sd = (StarDetectorHandle_s *)malloc(sizeof(StarDetectorHandle_s));
    if (!sd) return nullptr;

    if (params) {
        sd->internal.params = *params;
    } else {
        SDetParams defaults;
        defaults.structureLayers = 5;
        defaults.hotPixelFilterRadius = 1;
        defaults.iterativeClipSigma = 9.0f;
        defaults.iterativeMaxRounds = 5;
        defaults.medianFilterDetail = 1;
        defaults.maxStars = 2000;
        defaults.fitRadius = 6;
        defaults.fwhmClipSigma = 3.0f;
        defaults.maxAxisRatio = 2.0f;
        sd->internal.params = defaults;
    }

    sd->internal.width = 0;
    sd->internal.height = 0;
    sd->internal.raw_detail = nullptr;

    sdet_log(SDET_LOG_INFO, "SDET", "StarDetector created (fitRadius=%d, fwhmClipSigma=%.1f, maxAxisRatio=%.1f)",
             sd->internal.params.fitRadius, sd->internal.params.fwhmClipSigma, sd->internal.params.maxAxisRatio);
    return sd;
}

SDET_EXPORT void sdet_destroy(StarDetectorHandle handle)
{
    if (!handle) return;
    delete[] handle->internal.raw_detail;
    sdet_log(SDET_LOG_INFO, "SDET", "StarDetector destroyed");
    free(handle);
}

SDET_EXPORT int sdet_detect(StarDetectorHandle handle,
                             const uint16_t *image, int width, int height,
                             double **out_x, double **out_y, int *out_count)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect start: %dx%d", width, height);

    if (!handle || !image || !out_x || !out_y || !out_count) return -1;

    const SDetParams &params = handle->internal.params;
    size_t n = (size_t)width * height;

    std::vector<float> fimg(n);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }

    handle->internal.width = width;
    handle->internal.height = height;

    std::vector<float> map(n);
    sdet_get_structure_map(&handle->internal, fimg.data(), width, height, map.data());

    float *raw_detail = handle->internal.raw_detail;
    handle->internal.raw_detail = nullptr;

    std::vector<float> binary(n, 0.0f);
    if (raw_detail) {
        for (size_t i = 0; i < n; i++) {
            if (raw_detail[i] > 0.0f) binary[i] = 1.0f;
        }
        delete[] raw_detail;
    }

    ConnectedComponent *components = nullptr;
    int comp_count = 0;
    sdet_find_connected_components(binary.data(), width, height, &components, &comp_count);

    struct Candidate { double cx, cy; int pixel_count; double brightness; };
    std::vector<Candidate> candidates;
    for (int i = 0; i < comp_count; i++) {
        if (components[i].count <= 4) continue;
        int bw = components[i].x1 - components[i].x0 + 1;
        int bh = components[i].y1 - components[i].y0 + 1;
        if (bw < 2 || bh < 2) continue;
        float ar = (float)std::max(bw, bh) / std::max(std::min(bw, bh), 1);
        if (ar > 2.0f) continue;
        double sum_wx = 0, sum_wy = 0, sum_w = 0;
        for (int j = 0; j < components[i].count; j++) {
            float val = fimg[components[i].py[j] * width + components[i].px[j]];
            sum_wx += components[i].px[j] * val;
            sum_wy += components[i].py[j] * val;
            sum_w += val;
        }
        double cx = (sum_w > 0) ? sum_wx / sum_w : (components[i].x0 + components[i].x1) / 2.0;
        double cy = (sum_w > 0) ? sum_wy / sum_w : (components[i].y0 + components[i].y1) / 2.0;
        candidates.push_back({cx, cy, components[i].count, sum_w});
    }
    sdet_free_connected_components(components, comp_count);

    sdet_log(SDET_LOG_INFO, "SDET", "Candidates: %d (from %d connected components, filtered single-pixel)",
             (int)candidates.size(), comp_count);

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        return 0;
    }

    // 自适应fitRadius：基于连通域像素数中位数估算FWHM
    std::vector<int> pixel_counts;
    for (const auto& c : candidates) pixel_counts.push_back(c.pixel_count);
    int med_pixel_count = 0;
    if (!pixel_counts.empty()) {
        std::sort(pixel_counts.begin(), pixel_counts.end());
        int mid = pixel_counts.size() / 2;
        med_pixel_count = pixel_counts[mid];
    }
    
    // FWHM估算：连通域像素数 -> 等效半径 -> FWHM (Moffat4因子0.87)
    float fwhm_est = sqrt((float)med_pixel_count / 3.14159265f) * 0.87f;
    int auto_fit_radius = (int)(3.0f * fwhm_est);
    auto_fit_radius = std::max(6, std::min(20, auto_fit_radius));
    
    int actual_fit_radius = params.fitRadius;
    if (params.fitRadius <= 0) { // fitRadius=0表示自动模式
        actual_fit_radius = auto_fit_radius;
    }
    
    sdet_log(SDET_LOG_INFO, "SDET", "Auto fitRadius: med_pixels=%d fwhm_est=%.2f auto_radius=%d actual=%d",
             med_pixel_count, fwhm_est, auto_fit_radius, actual_fit_radius);

    if (params.maxStars > 0 && (int)candidates.size() > params.maxStars * 2) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &a, const Candidate &b) { return a.brightness > b.brightness; });
        candidates.resize(params.maxStars * 2);
    }

    int cc_count = (int)candidates.size();
    std::vector<InternalFitResult> fit_results(cc_count);
    int fit_ok_count = 0;

    #pragma omp parallel
    {
        LMWorkspace ws;
        #pragma omp for schedule(dynamic) reduction(+:fit_ok_count)
        for (int i = 0; i < cc_count; i++) {
            int rx0 = std::max(0, (int)candidates[i].cx - actual_fit_radius);
            int ry0 = std::max(0, (int)candidates[i].cy - actual_fit_radius);
            int rx1 = std::min(width, (int)candidates[i].cx + actual_fit_radius + 1);
            int ry1 = std::min(height, (int)candidates[i].cy + actual_fit_radius + 1);

            sdet_moffat4_fit(fimg.data(), width, height,
                             candidates[i].cx, candidates[i].cy,
                             rx0, ry0, rx1, ry1, &fit_results[i], &ws);
            if (fit_results[i].status == SDET_FIT_OK) fit_ok_count++;
        }
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Moffat4 fit: %d/%d OK", fit_ok_count, cc_count);

    // 拟合统计：按像素数分段统计成功率
    {
        struct SizeBin { int lo, hi; int total, ok, fail_invalid, fail_noconv, fail_iter; };
        SizeBin bins[] = {
            {5, 9, 0, 0, 0, 0, 0},
            {10, 19, 0, 0, 0, 0, 0},
            {20, 49, 0, 0, 0, 0, 0},
            {50, 99, 0, 0, 0, 0, 0},
            {100, 299, 0, 0, 0, 0, 0},
            {300, 999, 0, 0, 0, 0, 0},
            {1000, 99999, 0, 0, 0, 0, 0},
        };
        int n_bins = 7;
        for (int i = 0; i < cc_count; i++) {
            int px = candidates[i].pixel_count;
            for (int b = 0; b < n_bins; b++) {
                if (px >= bins[b].lo && px < bins[b].hi) {
                    bins[b].total++;
                    if (fit_results[i].status == SDET_FIT_OK) bins[b].ok++;
                    else if (fit_results[i].status == SDET_FIT_INVALID_PARAMS) bins[b].fail_invalid++;
                    else if (fit_results[i].status == SDET_FIT_NO_CONVERGENCE) bins[b].fail_noconv++;
                    else if (fit_results[i].status == SDET_FIT_ITERATION_LIMIT) bins[b].fail_iter++;
                    break;
                }
            }
        }
        sdet_log(SDET_LOG_INFO, "SDET", "=== Fit statistics by pixel count ===");
        for (int b = 0; b < n_bins; b++) {
            if (bins[b].total == 0) continue;
            float rate = (float)bins[b].ok / bins[b].total * 100.0f;
            sdet_log(SDET_LOG_INFO, "SDET", "  px[%d-%d]: total=%d ok=%d(%.1f%%) invalid=%d noconv=%d iterlimit=%d",
                     bins[b].lo, bins[b].hi, bins[b].total, bins[b].ok, rate,
                     bins[b].fail_invalid, bins[b].fail_noconv, bins[b].fail_iter);
        }
    }

    std::vector<float> fwhm_values;
    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status == SDET_FIT_OK) {
            float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
            fwhm_values.push_back(avg_fwhm);
        }
    }

    float fwhm_med = 0.0f, fwhm_mad_val = 0.0f;
    if (!fwhm_values.empty()) {
        fwhm_med = sdet_robust_median(fwhm_values.data(), (int)fwhm_values.size());
        fwhm_mad_val = sdet_robust_mad(fwhm_values.data(), (int)fwhm_values.size());
    }

    sdet_log(SDET_LOG_INFO, "SDET", "FWHM stats: med=%.4f mad=%.4f (from %d fitted stars)",
             fwhm_med, fwhm_mad_val, (int)fwhm_values.size());

    struct StarInfo { double cx, cy; float amp; };
    std::vector<StarInfo> stars;
    int f_fit = 0, f_fwhm = 0, f_round = 0;

    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status != SDET_FIT_OK) { f_fit++; continue; }
        float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
        if (fwhm_mad_val > 0.0f) {
            float fwhm_lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
            if (avg_fwhm < fwhm_lo) { f_fwhm++; continue; }
        }
        float axis_ratio = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                                    std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
        if (axis_ratio > params.maxAxisRatio) { f_round++; continue; }
        stars.push_back({fit_results[i].cx, fit_results[i].cy, (float)fit_results[i].A});
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Post-fit filters: %d/%d passed (fit_fail=%d fwhm=%d roundness=%d)",
             (int)stars.size(), cc_count, f_fit, f_fwhm, f_round);

    if (stars.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        return 0;
    }

    std::sort(stars.begin(), stars.end(), [](const StarInfo &a, const StarInfo &b) { return a.amp > b.amp; });

    {
        const int grid_sz = 2;
        struct GK { int gx, gy; bool operator==(const GK& o) const { return gx == o.gx && gy == o.gy; } };
        struct GKH { size_t operator()(const GK& k) const { return (size_t)k.gx * 1000003ULL + (size_t)k.gy; } };
        std::unordered_map<GK, std::vector<int>, GKH> grid;
        for (int i = 0; i < (int)stars.size(); i++) {
            int gx = (int)stars[i].cx / grid_sz;
            int gy = (int)stars[i].cy / grid_sz;
            grid[{gx, gy}].push_back(i);
        }
        std::vector<uint8_t> deleted(stars.size(), 0);
        for (int i = 0; i < (int)stars.size(); i++) {
            if (deleted[i]) continue;
            int gx = (int)stars[i].cx / grid_sz;
            int gy = (int)stars[i].cy / grid_sz;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    auto it = grid.find({gx + dx, gy + dy});
                    if (it == grid.end()) continue;
                    for (int j : it->second) {
                        if (j == i || deleted[j]) continue;
                        double ddx = stars[i].cx - stars[j].cx;
                        double ddy = stars[i].cy - stars[j].cy;
                        if (ddx * ddx + ddy * ddy <= 1.0) deleted[j] = 1;
                    }
                }
            }
        }
        int j = 0;
        for (int i = 0; i < (int)stars.size(); i++) {
            if (!deleted[i]) {
                if (j != i) stars[j] = stars[i];
                j++;
            }
        }
        stars.resize(j);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "After dedup: %d stars", (int)stars.size());

    if (params.maxStars > 0 && (int)stars.size() > params.maxStars) {
        stars.resize(params.maxStars);
    }

    int result_count = (int)stars.size();

    if (result_count == 0) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    if (!x_coords || !y_coords) {
        free(x_coords);
        free(y_coords);
        return -1;
    }

    for (int i = 0; i < result_count; i++) {
        x_coords[i] = stars[i].cx;
        y_coords[i] = stars[i].cy;
    }

    *out_x = x_coords;
    *out_y = y_coords;
    *out_count = result_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect done: %d stars, %.3f s", result_count, elapsed);
    return 0;
}

SDET_EXPORT void sdet_free_coords(double *coords)
{
    free(coords);
}

SDET_EXPORT int sdet_detect_debug(StarDetectorHandle handle,
                                   const uint16_t *image, int width, int height,
                                   double **out_x, double **out_y, int *out_count,
                                   float **out_mag, int **out_has_saturated,
                                   float **out_detail, float **out_smap, float **out_binary,
                                   const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || !out_x || !out_y || !out_count) return -1;

    const SDetParams &params = handle->internal.params;
    size_t n = (size_t)width * height;

    std::vector<float> fimg(n);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_debug start: %dx%d", width, height);

    handle->internal.width = width;
    handle->internal.height = height;

    std::vector<float> map(n);
    sdet_get_structure_map(&handle->internal, fimg.data(), width, height, map.data());

    float *raw_detail = handle->internal.raw_detail;
    handle->internal.raw_detail = nullptr;

    float *detail_out = (float *)malloc(n * sizeof(float));
    if (raw_detail) {
        std::memcpy(detail_out, raw_detail, n * sizeof(float));
    } else {
        std::memset(detail_out, 0, n * sizeof(float));
    }

    std::vector<float> binary(n, 0.0f);
    if (raw_detail) {
        for (size_t i = 0; i < n; i++) {
            if (raw_detail[i] > 0.0f) binary[i] = 1.0f;
        }
        delete[] raw_detail;
    }

    float *smap_out = (float *)malloc(n * sizeof(float));
    std::memcpy(smap_out, binary.data(), n * sizeof(float));
    float *binary_out = (float *)malloc(n * sizeof(float));
    std::memcpy(binary_out, binary.data(), n * sizeof(float));

    *out_detail = detail_out;
    *out_smap = smap_out;
    *out_binary = binary_out;

    // 正常星检测：细节层>0二值化→连通域→Moffat4拟合
    ConnectedComponent *components = nullptr;
    int comp_count = 0;
    sdet_find_connected_components(binary.data(), width, height, &components, &comp_count);

    struct Candidate { double cx, cy; int pixel_count; double brightness; };
    std::vector<Candidate> candidates;
    for (int i = 0; i < comp_count; i++) {
        if (components[i].count <= 2) continue;
        int bw = components[i].x1 - components[i].x0 + 1;
        int bh = components[i].y1 - components[i].y0 + 1;
        if (bw < 2 || bh < 2) continue;
        float ar = (float)std::max(bw, bh) / std::max(std::min(bw, bh), 1);
        if (ar > 3.0f) continue;
        double sum_wx = 0, sum_wy = 0, sum_w = 0;
        for (int j = 0; j < components[i].count; j++) {
            float val = fimg[components[i].py[j] * width + components[i].px[j]];
            sum_wx += components[i].px[j] * val;
            sum_wy += components[i].py[j] * val;
            sum_w += val;
        }
        double cx = (sum_w > 0) ? sum_wx / sum_w : (components[i].x0 + components[i].x1) / 2.0;
        double cy = (sum_w > 0) ? sum_wy / sum_w : (components[i].y0 + components[i].y1) / 2.0;
        candidates.push_back({cx, cy, components[i].count, sum_w});
    }
    sdet_free_connected_components(components, comp_count);

    sdet_log(SDET_LOG_INFO, "SDET", "Debug candidates: %d (from %d connected components, filtered ≤4px+2x2+ar≤3)",
             (int)candidates.size(), comp_count);

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        if (out_mag) *out_mag = nullptr;
        if (out_has_saturated) *out_has_saturated = nullptr;
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    // 自适应fitRadius：基于连通域像素数中位数估算FWHM
    std::vector<int> pixel_counts;
    for (const auto& c : candidates) pixel_counts.push_back(c.pixel_count);
    int med_pixel_count = 0;
    if (!pixel_counts.empty()) {
        std::sort(pixel_counts.begin(), pixel_counts.end());
        int mid = pixel_counts.size() / 2;
        med_pixel_count = pixel_counts[mid];
    }

    float fwhm_est = sqrt((float)med_pixel_count / 3.14159265f) * 0.87f;
    int auto_fit_radius = (int)(3.0f * fwhm_est);
    auto_fit_radius = std::max(6, std::min(20, auto_fit_radius));

    int actual_fit_radius = params.fitRadius;
    if (params.fitRadius <= 0) actual_fit_radius = auto_fit_radius;

    sdet_log(SDET_LOG_INFO, "SDET", "Auto fitRadius: med_pixels=%d fwhm_est=%.2f auto_radius=%d actual=%d",
             med_pixel_count, fwhm_est, auto_fit_radius, actual_fit_radius);

    if (params.maxStars > 0 && (int)candidates.size() > params.maxStars * 2) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &a, const Candidate &b) { return a.brightness > b.brightness; });
        candidates.resize(params.maxStars * 2);
    }

    int cc_count = (int)candidates.size();
    std::vector<InternalFitResult> fit_results(cc_count);
    int fit_ok_count = 0;

    #pragma omp parallel
    {
        LMWorkspace ws;
        #pragma omp for schedule(dynamic) reduction(+:fit_ok_count)
        for (int i = 0; i < cc_count; i++) {
            int rx0 = std::max(0, (int)candidates[i].cx - actual_fit_radius);
            int ry0 = std::max(0, (int)candidates[i].cy - actual_fit_radius);
            int rx1 = std::min(width, (int)candidates[i].cx + actual_fit_radius + 1);
            int ry1 = std::min(height, (int)candidates[i].cy + actual_fit_radius + 1);
            sdet_moffat4_fit(fimg.data(), width, height,
                             candidates[i].cx, candidates[i].cy,
                             rx0, ry0, rx1, ry1, &fit_results[i], &ws);
            if (fit_results[i].status == SDET_FIT_OK) fit_ok_count++;
        }
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug Moffat4 fit: %d/%d OK", fit_ok_count, cc_count);

    std::vector<float> fwhm_values;
    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status == SDET_FIT_OK) {
            float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
            fwhm_values.push_back(avg_fwhm);
        }
    }

    float fwhm_med = 0.0f, fwhm_mad_val = 0.0f;
    if (!fwhm_values.empty()) {
        fwhm_med = sdet_robust_median(fwhm_values.data(), (int)fwhm_values.size());
        fwhm_mad_val = sdet_robust_mad(fwhm_values.data(), (int)fwhm_values.size());
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug FWHM: med=%.4f mad=%.4f (%d fitted)",
             fwhm_med, fwhm_mad_val, (int)fwhm_values.size());

    // 构建正常星StarRecord列表（含 reject_star 验证）
    std::vector<StarRecord> stars;
    int f_fit = 0, f_fwhm = 0, f_round = 0, f_reject = 0;

    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status != SDET_FIT_OK) { f_fit++; continue; }
        float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
        if (fwhm_mad_val > 0.0f) {
            float lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
            float hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
            if (avg_fwhm < lo || avg_fwhm > hi) { f_fwhm++; continue; }
        }
        float ar = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                            std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
        if (ar > params.maxAxisRatio) { f_round++; continue; }
        // reject_star 验证（非饱和星）— legacy 路径无候选 sx/sy, 用 fit.sx/fit.sy 近似
        SfError sf_err = reject_star(fit_results[i], false,
                                     fit_results[i].sx, fit_results[i].sy);
        if (sf_err != SF_OK) { f_reject++; continue; }
        StarRecord rec;
        rec.cx = fit_results[i].cx;
        rec.cy = fit_results[i].cy;
        rec.flux = (float)fit_results[i].A;
        rec.is_saturated = 0;
        rec.fwhm_x = (float)fit_results[i].fwhm_x;
        rec.fwhm_y = (float)fit_results[i].fwhm_y;
        rec.sx = (float)fit_results[i].sx;
        rec.sy = (float)fit_results[i].sy;
        rec.theta = (float)fit_results[i].theta;
        rec.background = (float)fit_results[i].B;
        rec.amplitude = (float)fit_results[i].A;
        rec.r = 0.0f;
        rec.cand_R = 0.0f;
        rec.mag = (fit_results[i].A > 0.0) ? -2.5f * log10f((float)fit_results[i].A) : NAN;
        rec.has_saturated = 0;
        stars.push_back(rec);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug normal stars: %d (fit_fail=%d fwhm=%d roundness=%d reject=%d)",
             (int)stars.size(), f_fit, f_fwhm, f_round, f_reject);

    // 饱和星检测（阈值 70% + edge-walking 中心）
    std::vector<SaturatedCandidate> sat_candidates;
    float sat_threshold = 0.0f;
    float img_median = 0.0f;  // V4.41: 接收全局中位数背景
    sdet_detect_saturated_stars(fimg.data(), width, height, sat_candidates, sat_threshold, img_median);

    // 饱和星 PSF mask 拟合
    int sat_fit_ok = 0, sat_fit_fail = 0;
    for (const auto& sc : sat_candidates) {
        StarRecord rec;
        rec.cx = sc.cx;
        rec.cy = sc.cy;
        rec.is_saturated = 1;
        rec.has_saturated = 1;
        rec.r = sc.r;

        // PSF mask 拟合：传入 sat_threshold 排除饱和像素
        int rx0 = std::max(0, (int)sc.cx - actual_fit_radius);
        int ry0 = std::max(0, (int)sc.cy - actual_fit_radius);
        int rx1 = std::min(width, (int)sc.cx + actual_fit_radius + 1);
        int ry1 = std::min(height, (int)sc.cy + actual_fit_radius + 1);

        InternalFitResult sat_fit;
        sdet_moffat4_fit(fimg.data(), width, height, sc.cx, sc.cy,
                         rx0, ry0, rx1, ry1, &sat_fit, nullptr, (double)sat_threshold);

        if (sat_fit.status == SDET_FIT_OK) {
            // PSF 拟合成功
            rec.flux = (float)sat_fit.A;
            rec.fwhm_x = (float)sat_fit.fwhm_x;
            rec.fwhm_y = (float)sat_fit.fwhm_y;
            rec.sx = (float)sat_fit.sx;
            rec.sy = (float)sat_fit.sy;
            rec.theta = (float)sat_fit.theta;
            rec.background = (float)sat_fit.B;
            rec.amplitude = (float)sat_fit.A;
            rec.mag = (sat_fit.A > 0.0) ? -2.5f * log10f((float)sat_fit.A) : NAN;
            sat_fit_ok++;
        } else {
            // 拟合失败，退回 edge-walking 中心，mag=NaN
            rec.flux = 0.0f;
            rec.fwhm_x = 0.0f;
            rec.fwhm_y = 0.0f;
            rec.sx = 0.0f;
            rec.sy = 0.0f;
            rec.theta = 0.0f;
            rec.background = 0.0f;
            rec.amplitude = 0.0f;
            rec.mag = NAN;
            sat_fit_fail++;
        }
        stars.push_back(rec);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug saturated stars: %d (PSF fit ok=%d fail=%d)",
             (int)sat_candidates.size(), sat_fit_ok, sat_fit_fail);

    // 去重+排序
    sdet_dedup_stars(stars);
    sdet_sort_stars(stars);

    sdet_log(SDET_LOG_INFO, "SDET", "Debug after dedup+sort: %d stars", (int)stars.size());

    if (params.maxStars > 0 && (int)stars.size() > params.maxStars) {
        stars.resize(params.maxStars);
    }

    int result_count = (int)stars.size();

    if (result_count == 0) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        if (out_mag) *out_mag = nullptr;
        if (out_has_saturated) *out_has_saturated = nullptr;
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    float *mag_arr = out_mag ? (float *)malloc(result_count * sizeof(float)) : nullptr;
    int *has_sat_arr = out_has_saturated ? (int *)malloc(result_count * sizeof(int)) : nullptr;
    for (int i = 0; i < result_count; i++) {
        x_coords[i] = stars[i].cx;
        y_coords[i] = stars[i].cy;
        if (mag_arr) mag_arr[i] = stars[i].mag;
        if (has_sat_arr) has_sat_arr[i] = stars[i].has_saturated;
    }

    *out_x = x_coords;
    *out_y = y_coords;
    if (out_mag) *out_mag = mag_arr;
    if (out_has_saturated) *out_has_saturated = has_sat_arr;
    *out_count = result_count;

    // 可选输出参数
    if (out_extras && extra_count > 0) {
        *out_extras = (float **)malloc(extra_count * sizeof(float *));
        for (int e = 0; e < extra_count; e++) {
            (*out_extras)[e] = (float *)malloc(result_count * sizeof(float));
            ExtraField field = parse_extra_name(extra_names[e]);
            for (int i = 0; i < result_count; i++) {
                (*out_extras)[e][i] = get_extra_field(stars[i], field);
            }
        }
    }

    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_debug done: %d stars", result_count);
    return 0;
}

SDET_EXPORT void sdet_free_debug_maps(float *maps)
{
    free(maps);
}

// R11 (PREC-108 第二步): sdet_detect_impl - 星点检测核心 (模板双实例)
//   T=float  : 与旧 sdet_detect_ex 行为逐位一致 (uint16→float 由入口包装完成)
//   T=double : FP64 模式 (输入 double 校准图, 全程 double 检测, 不降级 float32)
template <typename T>
static int sdet_detect_impl(StarDetectorHandle handle,
                            const T *image, int width, int height,
                            double **out_x, double **out_y, float **out_flux, int **out_saturated,
                            float **out_mag, int **out_has_saturated,
                            int *out_count,
                            const char **extra_names, int extra_count, float ***out_extras)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    auto t_last = t0;
    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_impl start: %dx%d", width, height);

    if (!handle || !image || !out_x || !out_y || !out_flux || !out_saturated || !out_count) return -1;

    const SDetParams &params = handle->internal.params;
    size_t n = (size_t)width * height;

    handle->internal.width = width;
    handle->internal.height = height;

    // 阶段2: 原图高斯平滑 (T 版本: float=YvV, double=YvV_d)
    // 使用 Young-van Vliet 递归 IIR 高斯滤波
    std::vector<T> smooth(n);
    if constexpr (std::is_same_v<T, float>) {
        sdet_gaussian_blur_yvv(image, smooth.data(), width, height, 2.0);
    } else {
        sdet_gaussian_blur_yvv_d(image, smooth.data(), width, height, 2.0);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[2] Gaussian YvV smooth (sigma=2.0): %.1f ms",
             std::chrono::duration<double, std::milli>(t2 - t_last).count());
    t_last = t2;

    // 阶段3: 全局阈值
    // star_finder.c:80,201: threshold = stat->median + sigma*5.0*stat->bgnoise (sigma=1.0)
    T bgnoise = sdet_compute_bgnoise<T>(image, width, height);
    T img_median;
    if constexpr (std::is_same_v<T, float>) {
        img_median = sdet_robust_median(image, (int)n);
    } else {
        img_median = sdet_robust_median_d(image, (int)n);
    }
    T threshold = img_median + T(5.0) * bgnoise;
    sdet_log(SDET_LOG_INFO, "SDET", "peaker: median=%.4f bgnoise=%.4f threshold=%.4f (median+5*bgnoise)",
             img_median, bgnoise, threshold);
    auto t3 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[3] Threshold: %.1f ms",
             std::chrono::duration<double, std::milli>(t3 - t_last).count());
    t_last = t3;

    // 阶段4: peaker 完整候选检测 (star_finder.c:278-544)
    //   (1) 11x11 (r=5) 局部极大值 + 平局决胜 (line 288-310)
    //   (2) 3x3 邻域亮度验证 meanhigh/count (line 312-333)
    //   (3) 饱和星 edge-walking 精确定位中心 (line 355-409)
    //   (4) 二阶导数零交叉 Sr/Sc + 振幅 Ar/Ac (line 411-492)
    //   (5) R=max(ceil(3.717*Sr),ceil(3.717*Sc),r) (line 495-510)
    //   (6) 对称性质量检查 dA/dSr/dSc (line 513-518)
    //   (7) candidate_is_duplicate 去重 (line 521)
    struct Candidate {
        double cx, cy;
        int pixel_count;
        double brightness;
        double mag_est;       // meanhigh (star_finder.c:528)
        float sx;             // Sr row width (star_finder.c:530)
        float sy;             // Sc column width (star_finder.c:531)
        int R;                // box radius for PSF fitting (star_finder.c:529)
        float sat;            // saturation level (star_finder.c:532)
        bool has_saturated;
    };
    std::vector<Candidate> candidates;

    const double SQRT_EXP1 = 1.6487212707;              // sqrt(e), 振幅估计 (line 47)
    const double SAT_THRESHOLD_FRAC = 0.7;               // 动态范围比例 (line 50)
    const double SAT_DETECTION_RANGE_FRAC = 0.1;         // 饱和平台变化 (line 51)
    const double DENSITY_THRESHOLD = 0.001;              // PSF能量密度阈值 (line 49)
    const double s_factor = std::sqrt(-2.0 * std::log(DENSITY_THRESHOLD)); // =3.7172 (line 275)
    const int MAX_BOX_RADIUS = 200;                      // (line 52)
    const double MAX_RADIUS_RATIO_DUP = 0.2;             // (line 53)

    const int r = 5;  // 搜索半径 (sf->radius)
    const int boxsize = (2 * r + 1) * (2 * r + 1);
    const double bg = (double)img_median;
    double maxi = 0.0;
    for (size_t i = 0; i < n; i++) {
        if (image[i] > maxi) maxi = image[i];
    }
    const float norm = 65535.0f;  // uint16 归一化值
    const double dynrange = std::min(maxi, (double)norm) - bg;
    const double minsatlevel = dynrange * SAT_THRESHOLD_FRAC;
    const double satrange = dynrange * SAT_DETECTION_RANGE_FRAC;
    const double locthreshold = 5.0 * (double)bgnoise;  // sf->sigma=1.0

    sdet_log(SDET_LOG_INFO, "SDET", "peaker: bg=%.1f maxi=%.1f norm=%.1f dynrange=%.1f "
             "minsatlevel=%.1f satrange=%.1f s_factor=%.4f locthreshold=%.4f",
             bg, maxi, norm, dynrange, minsatlevel, satrange, s_factor, locthreshold);

    // V4.51-debug: 饱和判断阶段统计
    int dbg_pass_threshold = 0;    // pixel > threshold
    int dbg_pass_localmax = 0;     // 11x11 局部极大值
    int dbg_pass_count3 = 0;       // 3x3 邻域 count >= 3
    int dbg_pass_boundary = 0;     // 边界检查
    int dbg_sat_meanhigh = 0;      // meanhigh - bg >= minsatlevel
    int dbg_sat_platform = 0;      // pixel0 - minhigh <= satrange
    int dbg_sat_both = 0;          // 两个条件都满足 (饱和)
    int dbg_nonsat = 0;            // 非饱和

    for (int y = r; y < height - r; y++) {
        for (int x = r; x < width - r; x++) {
            T pixel = smooth[(size_t)y * width + x];
            if (pixel <= threshold) continue;
            dbg_pass_threshold++;

            // (1) 11x11 局部极大值 + 平局决胜 (star_finder.c:288-310)
            bool bingo = true;
            int count = 0;
            for (int yy = y - r; yy <= y + r && bingo; yy++) {
                for (int xx = x - r; xx <= x + r; xx++) {
                    if (xx == x && yy == y) continue;
                    T neighbor = smooth[(size_t)yy * width + xx];
                    if (neighbor > pixel) { bingo = false; break; }
                    else if (neighbor == pixel) {
                        // 平局决胜: 取左上半平面为唯一极大值 (star_finder.c:296-300)
                        if ((xx <= x && yy <= y) || (xx > x && yy < y)) {
                            bingo = false; break;
                        }
                    }
                    count++;
                }
            }
            if (count < boxsize - 1) continue;
            dbg_pass_localmax++;
            x += r;  // 跳 r 像素 (star_finder.c:310)

            // (2) 3x3 邻域亮度验证 (star_finder.c:312-333)
            int xx = x - r;  // 原始 x
            int yy = y;
            T pixel0 = image[(size_t)yy * width + xx];  // 原图中心像素
            double meanhigh = 0.0, minhigh = 1e30;
            count = 0;
            for (int ny = y - 1; ny <= y + 1; ny++) {
                for (int nx = xx - 1; nx <= xx + 1; nx++) {
                    if (nx == xx && ny == y) continue;  // 中心像素
                    T neighbor = image[(size_t)ny * width + nx];  // 原图
                    if (neighbor >= threshold) {
                        if (neighbor < minhigh) minhigh = neighbor;
                        meanhigh += neighbor;
                        count++;
                    }
                }
            }
            // mono 模式 count<3 拒绝 (star_finder.c:329)
            if (count == 0 || count < 3) continue;
            dbg_pass_count3++;
            meanhigh /= (double)count;

            // 边界检查 (star_finder.c:344): 确保 xx±2, yy±2 在图像内
            if (xx - 2 < 1 || xx + 2 > width - 1 || yy - 2 < 1 || yy + 2 > height - 1) continue;
            dbg_pass_boundary++;

            // (3) 饱和星 edge-walking 精确定位中心 (star_finder.c:346-409)
            bool has_saturated = false;
            T sat = T(0);
            T r0 = T(0), c0 = T(0);

            // V4.51-debug: 统计饱和判断中间变量
            bool cond_meanhigh = (meanhigh - bg >= minsatlevel);
            bool cond_platform = (pixel0 - minhigh <= satrange);
            if (cond_meanhigh) dbg_sat_meanhigh++;
            if (cond_platform) dbg_sat_platform++;

            if (meanhigh - bg < minsatlevel || pixel0 - minhigh > satrange) {
                // 非饱和: 简单一阶导数 (star_finder.c:347-354)
                dbg_nonsat++;
                T d1rl = pixel - smooth[(size_t)yy * width + xx - 1];
                T d1rr = smooth[(size_t)yy * width + xx + 1] - pixel;
                T d1cu = pixel - smooth[(size_t)(yy - 1) * width + xx];
                T d1cd = smooth[(size_t)(yy + 1) * width + xx] - pixel;
                T denom_r = d1rr - d1rl;
                T denom_c = d1cd - d1cu;
                r0 = (std::abs(denom_r) < T(1e-20)) ? T(-0.5) : T(-0.5) - d1rl / denom_r;
                c0 = (std::abs(denom_c) < T(1e-20)) ? T(-0.5) : T(-0.5) - d1cu / denom_c;
            } else {
                // 饱和: edge-walking (star_finder.c:355-409)
                has_saturated = true;
                dbg_sat_both++;
                sat = std::min(pixel0, T(norm)) - T(satrange);
                int i = 0, j = 0;
                int xr = 0, xl = 0, yu = 0, yd = 0;

                // 向右找到边缘 (star_finder.c:361)
                while ((xx + i < width - 1) && smooth[(size_t)yy * width + xx + i] > sat) i++;
                // SW or S (star_finder.c:364-369)
                while ((xx + i < width - 1) && (yy + j < height - 1)
                    && (smooth[(size_t)(yy + j + 1) * width + xx + i + 1] > sat
                    ||  smooth[(size_t)(yy + j + 1) * width + xx + i    ] > sat)) {
                    if (smooth[(size_t)(yy + j + 1) * width + xx + i + 1] > sat) i++;
                    j++;
                }
                xr = i;
                // SE or E (star_finder.c:372-377)
                while ((xx + i > 1) && (yy + j < height - 1)
                    && (smooth[(size_t)(yy + j + 1) * width + xx + i - 1] > sat
                    ||  smooth[(size_t)(yy + j    ) * width + xx + i - 1] > sat)) {
                    if (smooth[(size_t)(yy + j + 1) * width + xx + i - 1] > sat) j++;
                    i--;
                }
                yd = j;
                // NE or N (star_finder.c:380-385)
                while ((xx + i > 1) && (yy + j > 1)
                    && (smooth[(size_t)(yy + j - 1) * width + xx + i - 1] > sat
                    ||  smooth[(size_t)(yy + j - 1) * width + xx + i    ] > sat)) {
                    if (smooth[(size_t)(yy + j - 1) * width + xx + i - 1] > sat) i--;
                    j--;
                }
                xl = i;
                // NW or W (star_finder.c:388-393)
                while ((xx + i < width - 1) && (yy + j > 1)
                    && (smooth[(size_t)(yy + j - 1) * width + xx + i + 1] > sat
                    ||  smooth[(size_t)(yy + j    ) * width + xx + i + 1] > sat)) {
                    if (smooth[(size_t)(yy + j - 1) * width + xx + i + 1] > sat) j--;
                    i++;
                }
                yu = j;
                // SW or S again to close the loop (star_finder.c:397-402)
                while ((xx + i < width - 1) && (yy + j < height - 1)
                    && (smooth[(size_t)(yy + j + 1) * width + xx + i + 1] > sat
                    ||  smooth[(size_t)(yy + j + 1) * width + xx + i    ] > sat)) {
                    if (smooth[(size_t)(yy + j + 1) * width + xx + i + 1] > sat) i++;
                    j++;
                }
                if (i > xr) xr = i;
                xx += (xr + xl) / 2;
                yy += (yu + yd) / 2;
                r0 = -0.5f;
                c0 = -0.5f;
                x += xr;
            }

            // (4) 二阶导数零交叉估计 (star_finder.c:411-492)
            T srr = T(0), srl = T(0), scd = T(0), scu = T(0);
            T Arr = T(0), Arl = T(0), Acd = T(0), Acu = T(0);

            // Row-wise moving right (star_finder.c:416-431)
            {
                int i = 0;
                if (has_saturated) while (xx + i < width && smooth[(size_t)yy * width + xx + i] > sat) i++;
                if (xx + i >= width - 2) continue;  // largely saturated close to border
                T d2rr  = smooth[(size_t)yy * width + xx + i + 1] + smooth[(size_t)yy * width + xx + i - 1] - T(2) * smooth[(size_t)yy * width + xx + i    ];
                T d2rrr = smooth[(size_t)yy * width + xx + i + 2] + smooth[(size_t)yy * width + xx + i    ] - T(2) * smooth[(size_t)yy * width + xx + i + 1];
                while ((d2rrr < 0) && ((xx + i + 2) < width - 1)) {
                    i++;
                    d2rr = d2rrr;
                    d2rrr = smooth[(size_t)yy * width + xx + i + 2] + smooth[(size_t)yy * width + xx + i] - 2 * smooth[(size_t)yy * width + xx + i + 1];
                }
                T denom = d2rrr - d2rr;
                srr = (std::abs(denom) < T(1e-20)) ? T(i) - r0 : T(i) - d2rr / denom - r0;
                T d1rr = smooth[(size_t)yy * width + xx + i] - smooth[(size_t)yy * width + xx + i - 1];
                Arr = -d1rr * srr * T(SQRT_EXP1);
            }
            // Row-wise moving left (star_finder.c:434-449)
            {
                int i = 0;
                if (has_saturated) while (xx - i > 0 && smooth[(size_t)yy * width + xx - i] > sat) i++;
                if (xx - i <= 2) continue;
                T d2rl  = smooth[(size_t)yy * width + xx - i - 1] + smooth[(size_t)yy * width + xx - i + 1] - T(2) * smooth[(size_t)yy * width + xx - i    ];
                T d2rll = smooth[(size_t)yy * width + xx - i - 2] + smooth[(size_t)yy * width + xx - i    ] - T(2) * smooth[(size_t)yy * width + xx - i - 1];
                while ((d2rll < 0) && ((xx - i - 2) > 1)) {
                    i++;
                    d2rl = d2rll;
                    d2rll = smooth[(size_t)yy * width + xx - i - 2] + smooth[(size_t)yy * width + xx - i] - 2 * smooth[(size_t)yy * width + xx - i - 1];
                }
                T denom = d2rll - d2rl;
                srl = (std::abs(denom) < T(1e-20)) ? T(-i) - r0 : -((T)i - d2rl / denom) - r0;
                T d1rl = smooth[(size_t)yy * width + xx - i] - smooth[(size_t)yy * width + xx - i + 1];
                Arl = d1rl * srl * T(SQRT_EXP1);
            }
            // Column-wise moving down (star_finder.c:452-467)
            {
                int i = 0;
                if (has_saturated) while (yy + i < height && smooth[(size_t)(yy + i) * width + xx] > sat) i++;
                if (yy + i >= height - 2) continue;
                T d2cd  = smooth[(size_t)(yy + i + 1) * width + xx] + smooth[(size_t)(yy + i - 1) * width + xx] - T(2) * smooth[(size_t)(yy + i    ) * width + xx];
                T d2cdd = smooth[(size_t)(yy + i + 2) * width + xx] + smooth[(size_t)(yy + i    ) * width + xx] - T(2) * smooth[(size_t)(yy + i + 1) * width + xx];
                while ((d2cdd < 0) && ((yy + i + 2) < height - 1)) {
                    i++;
                    d2cd = d2cdd;
                    d2cdd = smooth[(size_t)(yy + i + 2) * width + xx] + smooth[(size_t)(yy + i) * width + xx] - 2 * smooth[(size_t)(yy + i + 1) * width + xx];
                }
                T denom = d2cdd - d2cd;
                scd = (std::abs(denom) < T(1e-20)) ? T(i) - c0 : T(i) - d2cd / denom - c0;
                T d1cd = smooth[(size_t)(yy + i) * width + xx] - smooth[(size_t)(yy + i - 1) * width + xx];
                Acd = -d1cd * scd * T(SQRT_EXP1);
            }
            // Column-wise moving up (star_finder.c:470-485)
            {
                int i = 0;
                if (has_saturated) while (yy - i > 0 && smooth[(size_t)(yy - i) * width + xx] > sat) i++;
                if (yy - i <= 2) continue;
                T d2cu  = smooth[(size_t)(yy - i - 1) * width + xx] + smooth[(size_t)(yy - i + 1) * width + xx] - T(2) * smooth[(size_t)(yy - i    ) * width + xx];
                T d2cuu = smooth[(size_t)(yy - i - 2) * width + xx] + smooth[(size_t)(yy - i    ) * width + xx] - T(2) * smooth[(size_t)(yy - i - 1) * width + xx];
                while ((d2cuu < 0) && ((yy - i - 2) > 1)) {
                    i++;
                    d2cu = d2cuu;
                    d2cuu = smooth[(size_t)(yy - i - 2) * width + xx] + smooth[(size_t)(yy - i) * width + xx] - 2 * smooth[(size_t)(yy - i - 1) * width + xx];
                }
                T denom = d2cuu - d2cu;
                scu = (std::abs(denom) < T(1e-20)) ? T(-i) - c0 : -((T)i - d2cu / denom) - c0;
                T d1cu = smooth[(size_t)(yy - i) * width + xx] - smooth[(size_t)(yy - i + 1) * width + xx];
                Acu = d1cu * scu * T(SQRT_EXP1);
            }

            // Smoothed PSF estimators (star_finder.c:488-492)
            T Sr = T(0.5) * (-srl + srr);
            T Ar = T(0.5) * (Arl + Arr);
            T Sc = T(0.5) * (-scu + scd);
            T Ac = T(0.5) * (Acu + Acd);

            // (5) R computation (star_finder.c:495-510)
            int Rr = (int)std::ceil(s_factor * (double)Sr);
            int Rc = (int)std::ceil(s_factor * (double)Sc);
            int Rm = std::max(Rr, Rc);
            Rm = std::min(Rm, MAX_BOX_RADIUS);
            int R = std::max(Rm, r);
            // avoid enlarging outside frame (star_finder.c:502-510)
            if (xx - R < 0) R = xx;
            if (xx + R >= width) R = width - xx - 1;
            if (yy - R < 0) R = yy;
            if (yy + R >= height) R = height - yy - 1;
            if (R < 1) R = 1;

            // (6) 对称性质量检查 (star_finder.c:513-518)
            T minArAc = std::min(std::abs(Ar), std::abs(Ac));
            T maxArAc = std::max(std::abs(Ar), std::abs(Ac));
            T dA = (minArAc > T(1e-20)) ? maxArAc / minArAc : T(1e30);
            T msrl = std::max(-srl, srr);
            T lsrl = std::min(-srl, srr);
            T dSr = (lsrl > T(1e-20)) ? msrl / lsrl : T(1e30);
            T mscu = std::max(-scu, scd);
            T lscu = std::min(-scu, scd);
            T dSc = (lscu > T(1e-20)) ? mscu / lscu : T(1e30);
            // relax_checks=false (default): dA>2 || dSr>2 || dSc>2 || max(Ar,Ac)<locthreshold
            if (dA > T(2) || dSr > T(2) || dSc > T(2) || maxArAc < T(locthreshold))
                continue;

            // (7) candidate_is_duplicate (star_finder.c:521, 149-160)
            {
                int matchradius = (int)((double)R * MAX_RADIUS_RATIO_DUP);
                matchradius = std::max(matchradius, 1);
                bool is_dup = false;
                for (int k = (int)candidates.size() - 1; k >= 0; k--) {
                    if (std::abs(xx - (int)candidates[k].cx) + std::abs(yy - (int)candidates[k].cy) <= matchradius) {
                        is_dup = true; break;
                    }
                    if (yy - (int)candidates[k].cy > R) break;  // y too far, no dup possible
                }
                if (is_dup) continue;
            }

            // pixel_count = 11x11 窗口超阈值像素数 (供统计)
            int pc = 0;
            for (int ny = y - r; ny <= y + r; ny++) {
                for (int nx = xx - r; nx <= xx + r; nx++) {
                    if (smooth[(size_t)ny * width + nx] > threshold) pc++;
                }
            }

            double brightness = (double)pixel0;  // 原图中心像素 (edge-walking 前的原始中心)
            candidates.push_back({(double)xx, (double)yy, pc, brightness,
                                  meanhigh, Sr, Sc, R,
                                  has_saturated ? sat : norm, has_saturated});
        }
    }
    auto t4 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[4] peaker: %.1f ms (%d candidates, full peaker 7-step)",
             std::chrono::duration<double, std::milli>(t4 - t_last).count(), (int)candidates.size());
    t_last = t4;

    {
        int sat_cnt = 0;
        for (const auto& c : candidates) if (c.has_saturated) sat_cnt++;
        sdet_log(SDET_LOG_INFO, "SDET", "Candidates: %d",
                 (int)candidates.size(), threshold, sat_cnt);
        // V4.51-debug: 饱和判断阶段统计
        sdet_log(SDET_LOG_INFO, "SDET", "Peaker stages: pass_threshold=%d pass_localmax=%d pass_count3=%d pass_boundary=%d | "
                 "cond_meanhigh=%d cond_platform=%d sat_both=%d nonsat=%d",
                 dbg_pass_threshold, dbg_pass_localmax, dbg_pass_count3, dbg_pass_boundary,
                 dbg_sat_meanhigh, dbg_sat_platform, dbg_sat_both, dbg_nonsat);
    }

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_flux = nullptr;
        *out_saturated = nullptr;
        if (out_mag) *out_mag = nullptr;
        if (out_has_saturated) *out_has_saturated = nullptr;
        *out_count = 0;
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    // V4.34 回退: per-candidate 饱和判定在连通域架构下失效, 恢复 V4.33 独立饱和检测 (阶段9)

    // 自适应fitRadius：基于连通域像素数中位数估算FWHM
    std::vector<int> pixel_counts;
    for (const auto& c : candidates) pixel_counts.push_back(c.pixel_count);
    int med_pixel_count = 0;
    if (!pixel_counts.empty()) {
        std::sort(pixel_counts.begin(), pixel_counts.end());
        int mid = pixel_counts.size() / 2;
        med_pixel_count = pixel_counts[mid];
    }

    float fwhm_est = sqrt((float)med_pixel_count / 3.14159265f) * 0.87f;
    int auto_fit_radius = (int)(3.0f * fwhm_est);
    auto_fit_radius = std::max(6, std::min(20, auto_fit_radius));

    int actual_fit_radius = params.fitRadius;
    if (params.fitRadius <= 0) actual_fit_radius = auto_fit_radius;

    sdet_log(SDET_LOG_INFO, "SDET", "Auto fitRadius: med_pixels=%d fwhm_est=%.2f auto_radius=%d actual=%d",
             med_pixel_count, fwhm_est, auto_fit_radius, actual_fit_radius);

    // V4.63: Task 6 候选排序 —

    //   star_cmp_by_mag_est: mag_est 降序 (meanhigh 越大越亮, star_finder.c:136-144)
    //   V4.61 之前用 brightness(pixel0) 排序, 饱和星 pixel0=65535 全相同被任意截断
    //   mag_est=meanhigh 是局部背景上方均值估计, 能区分饱和星亮度
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.mag_est > b.mag_est; });
    sdet_log(SDET_LOG_INFO, "SDET", "Task6 candidates sorted by mag_est (desc): maxstars=%d candidates=%d",
             params.maxStars, (int)candidates.size());

    // 阶段6: Moffat4拟合 (使用 per-candidate R 作为拟合窗口,
    int cc_count = (int)candidates.size();
    std::vector<InternalFitResult> fit_results(cc_count);
    int fit_ok_count = 0;

    #pragma omp parallel
    {
        LMWorkspace ws;
        #pragma omp for schedule(dynamic) reduction(+:fit_ok_count)
        for (int i = 0; i < cc_count; i++) {
            int fit_r = candidates[i].R;  // per-candidate box radius
            int rx0 = std::max(0, (int)candidates[i].cx - fit_r);
            int ry0 = std::max(0, (int)candidates[i].cy - fit_r);
            int rx1 = std::min(width, (int)candidates[i].cx + fit_r + 1);
            int ry1 = std::min(height, (int)candidates[i].cy + fit_r + 1);


            //   非饱和候选 sat=norm=65535, mask 排除 pixel>=65535 的真正饱和像素 (含 65535 平台)
            //   之前 IPv 非饱和候选 sat_mask=0 不过滤, 导致含饱和像素的星 A 被拉高 (Galaxy_Center +79)
            double sat_mask = (double)candidates[i].sat;

            sdet_moffat4_fit<T>(image, width, height,
                                candidates[i].cx, candidates[i].cy,
                                rx0, ry0, rx1, ry1, &fit_results[i], &ws, sat_mask,
                                (double)candidates[i].sx, (double)candidates[i].sy,
                                (double)img_median);
            if (fit_results[i].status == SDET_FIT_OK) fit_ok_count++;
        }
    }
    auto t6 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[6] Moffat4 fit: %.1f ms (%d/%d OK)", 
             std::chrono::duration<double, std::milli>(t6 - t_last).count(), fit_ok_count, cc_count);
    t_last = t6;

    sdet_log(SDET_LOG_INFO, "SDET", "Moffat4 fit: %d/%d OK", fit_ok_count, cc_count);

    // 拟合统计：按像素数分段统计成功率
    {
        struct SizeBin { int lo, hi; int total, ok, fail_invalid, fail_noconv, fail_iter; };
        SizeBin bins[] = {
            {5, 9, 0, 0, 0, 0, 0},
            {10, 19, 0, 0, 0, 0, 0},
            {20, 49, 0, 0, 0, 0, 0},
            {50, 99, 0, 0, 0, 0, 0},
            {100, 299, 0, 0, 0, 0, 0},
            {300, 999, 0, 0, 0, 0, 0},
            {1000, 99999, 0, 0, 0, 0, 0},
        };
        int n_bins = 7;
        for (int i = 0; i < cc_count; i++) {
            int px = candidates[i].pixel_count;
            for (int b = 0; b < n_bins; b++) {
                if (px >= bins[b].lo && px < bins[b].hi) {
                    bins[b].total++;
                    if (fit_results[i].status == SDET_FIT_OK) bins[b].ok++;
                    else if (fit_results[i].status == SDET_FIT_INVALID_PARAMS) bins[b].fail_invalid++;
                    else if (fit_results[i].status == SDET_FIT_NO_CONVERGENCE) bins[b].fail_noconv++;
                    else if (fit_results[i].status == SDET_FIT_ITERATION_LIMIT) bins[b].fail_iter++;
                    break;
                }
            }
        }
        sdet_log(SDET_LOG_INFO, "SDET", "=== Fit statistics by pixel count ===");
        for (int b = 0; b < n_bins; b++) {
            if (bins[b].total == 0) continue;
            float rate = (float)bins[b].ok / bins[b].total * 100.0f;
            sdet_log(SDET_LOG_INFO, "SDET", "  px[%d-%d]: total=%d ok=%d(%.1f%%) invalid=%d noconv=%d iterlimit=%d",
                     bins[b].lo, bins[b].hi, bins[b].total, bins[b].ok, rate,
                     bins[b].fail_invalid, bins[b].fail_noconv, bins[b].fail_iter);
        }
    }

    // 阶段7: FWHM统计+过滤
    std::vector<float> fwhm_values;
    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status == SDET_FIT_OK) {
            float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
            fwhm_values.push_back(avg_fwhm);
        }
    }

    float fwhm_med = 0.0f, fwhm_mad_val = 0.0f;
    if (!fwhm_values.empty()) {
        fwhm_med = sdet_robust_median(fwhm_values.data(), (int)fwhm_values.size());
        fwhm_mad_val = sdet_robust_mad(fwhm_values.data(), (int)fwhm_values.size());
    }
    auto t7 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[7] FWHM stats: %.1f ms (med=%.4f mad=%.4f)", 
             std::chrono::duration<double, std::milli>(t7 - t_last).count(), fwhm_med, fwhm_mad_val);
    t_last = t7;

    sdet_log(SDET_LOG_INFO, "SDET", "FWHM stats: med=%.4f mad=%.4f (from %d fitted stars)",
             fwhm_med, fwhm_mad_val, (int)fwhm_values.size());

    // 阶段8: 构建正常星StarRecord列表（含 reject_star 验证）
    std::vector<StarRecord> stars;
    int f_fit = 0, f_fwhm = 0, f_round = 0, f_reject = 0;

    for (int i = 0; i < cc_count; i++) {


        //   GSL LM status != GSL_SUCCESS 时 error=PSF_ERR_DIVERGED, minimize_candidates 丢弃
        //   IPv: fit_results[i].status != SDET_FIT_OK 等价于 DIVERGED, 丢弃
        if (fit_results[i].status != SDET_FIT_OK) { f_fit++; continue; }
        // V4.49: 移除全局 FWHM clip (fwhm_med±3*mad), 完全依赖 reject_star 自适应 FWHM 上限

        //   全局 clip 会误杀宽星 (如 NGC6302 FWHM=7-33 的星),
        float axis_ratio = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                                    std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
        if (axis_ratio > params.maxAxisRatio) { f_round++; continue; }
        // V4.51: reject_star 传入候选阶段 sx/sy (Sr/Sc,

        SfError sf_err = reject_star(fit_results[i], candidates[i].has_saturated,
                                     (double)candidates[i].sx, (double)candidates[i].sy);
        if (sf_err != SF_OK) { f_reject++; continue; }
        StarRecord rec;
        rec.cx = fit_results[i].cx;
        rec.cy = fit_results[i].cy;
        rec.flux = (float)fit_results[i].A;


        //   候选阶段 has_saturated (meanhigh检查) 仅用于 sat 字段选择, 最终被 A>dynrange 覆盖
        //   这使饱和星直接从 peaker 候选中识别, 无需独立饱和星检测路径
        rec.is_saturated = (fit_results[i].A > dynrange) ? 1 : 0;
        rec.fwhm_x = (float)fit_results[i].fwhm_x;
        rec.fwhm_y = (float)fit_results[i].fwhm_y;
        rec.sx = (float)fit_results[i].sx;
        rec.sy = (float)fit_results[i].sy;
        rec.theta = (float)fit_results[i].theta;
        rec.background = (float)fit_results[i].B;
        rec.amplitude = (float)fit_results[i].A;
        rec.r = 0.0f;
        rec.cand_R = (float)candidates[i].R;  // V4.59-diag: 候选阶段 R (mag box 半径)
        // V4.50: mag, star_finder.c:638-655)

        //        mag = -2.5*log10(Σ_box(pixel - B)), B=PSF拟合背景 FIT(0) (PSF.c:724,653)
        //        box=(2R+1)², R=max(ceil(s_factor*Sr),ceil(s_factor*Sc),r) (star_finder.c:495-499)
        //        B 初始值=全局中位数 (compute_threshold, star_finder.c:82), 经LM拟合优化
        // V4.50 修复: mag_radius 改用 candidates[i].R (与拟合 box 一致), 不再用 1.58*FWHM 重算
        //   原因: 1.58*FWHM 可能 > R (宽星情况), 导致 mag box > 拟合 box, 累加更多星云像素
        //   NGC6302 rk=1: R=17, 1.58*FWHM=23, mag box 偏大导致 mag 偏亮 0.8 mag
        {
            int mag_radius = candidates[i].R;  // V4.50: 用候选 R
            mag_radius = std::max(mag_radius, 5);   // 下限 r=5
            mag_radius = std::min(mag_radius, 200);  // 上限 MAX_BOX_RADIUS
            // V4.57: mag box 中心用候选中心

            //   之前 IPv 用 fit_results[i].cx (拟合中心), 偏离候选中心 0.1-0.5px
            //   导致 box 包含像素不同 → box_sum 微小差异 → mag 排序偏移 (NGC6302 rank 差 +3~+5)
            int mcx = (int)candidates[i].cx;
            int mcy = (int)candidates[i].cy;
            int mx0 = std::max(0, mcx - mag_radius);
            int my0 = std::max(0, mcy - mag_radius);
            int mx1 = std::min(width, mcx + mag_radius + 1);
            int my1 = std::min(height, mcy + mag_radius + 1);
            float local_B = (float)fit_results[i].B;  // V4.44: 用拟合B)
            double box_sum = 0.0;
            for (int yy = my0; yy < my1; yy++) {
                for (int xx = mx0; xx < mx1; xx++) {
                    box_sum += (double)image[yy * width + xx] - local_B;
                }
            }
            rec.mag = (box_sum > 0.0) ? -2.5f * log10f((float)box_sum) : NAN;
        }
        // V4.52: has_saturated

        //   候选阶段 has_saturated 仅用于 sat 字段值选择, 不影响最终饱和标志
        rec.has_saturated = rec.is_saturated;
        stars.push_back(rec);
    }
    auto t8 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[8] Build normal stars: %.1f ms (%d stars, fit_fail=%d fwhm=%d round=%d reject=%d)",
             std::chrono::duration<double, std::milli>(t8 - t_last).count(), (int)stars.size(), f_fit, f_fwhm, f_round, f_reject);
    t_last = t8;

    sdet_log(SDET_LOG_INFO, "SDET", "Normal stars: %d (fit_fail=%d fwhm=%d roundness=%d reject=%d)",
             (int)stars.size(), f_fit, f_fwhm, f_round, f_reject);

    // 阶段9: [V4.52 移除] 独立饱和星检测路径

    //   IPv V4.52 流程: 阶段6/7 PSF 拟合所有候选 → 阶段8 用 A>dynrange 设置 is_saturated
    //   原阶段9的独立阈值二值化+edge-walking 与参考实现不一致, 导致 Galaxy_Center 多检7颗小饱和星
    //   饱和星现在直接从 peaker 候选中识别, 无需独立路径
    auto t9 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[9] Saturated star detection skipped (V4.52: use A>dynrange from peaker): %.1f ms",
             std::chrono::duration<double, std::milli>(t9 - t_last).count());
    t_last = t9;

    // 阶段10: 去重+排序
    sdet_dedup_stars(stars);
    auto t10a = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[10a] Dedup: %.1f ms", 
             std::chrono::duration<double, std::milli>(t10a - t_last).count());
    
    sdet_sort_stars(stars);
    auto t10b = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[10b] Sort: %.1f ms", 
             std::chrono::duration<double, std::milli>(t10b - t10a).count());
    t_last = t10b;

    int sat_count = 0, normal_count = 0;
    for (const auto& s : stars) {
        if (s.is_saturated) sat_count++;
        else normal_count++;
    }
    sdet_log(SDET_LOG_INFO, "SDET", "After dedup+sort: %d stars (saturated=%d normal=%d)",
             (int)stars.size(), sat_count, normal_count);

    if (params.maxStars > 0 && (int)stars.size() > params.maxStars) {
        stars.resize(params.maxStars);
    }

    int result_count = (int)stars.size();

    if (result_count == 0) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_flux = nullptr;
        *out_saturated = nullptr;
        if (out_mag) *out_mag = nullptr;
        if (out_has_saturated) *out_has_saturated = nullptr;
        *out_count = 0;
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    float *flux_arr = (float *)malloc(result_count * sizeof(float));
    int *sat_arr = (int *)malloc(result_count * sizeof(int));
    // 条件分配：out_mag/out_has_saturated 为 NULL 时不分配，避免泄漏（与 sdet_detect_debug 一致）
    float *mag_arr = out_mag ? (float *)malloc(result_count * sizeof(float)) : nullptr;
    int *has_sat_arr = out_has_saturated ? (int *)malloc(result_count * sizeof(int)) : nullptr;

    if (!x_coords || !y_coords || !flux_arr || !sat_arr ||
        (out_mag && !mag_arr) || (out_has_saturated && !has_sat_arr)) {
        free(x_coords); free(y_coords); free(flux_arr); free(sat_arr);
        free(mag_arr); free(has_sat_arr);
        return -1;
    }

    for (int i = 0; i < result_count; i++) {
        x_coords[i] = stars[i].cx;
        y_coords[i] = stars[i].cy;
        flux_arr[i] = stars[i].flux;
        // V4.33: saturated 标志用 is_saturated (阶段9 独立饱和检测)
        sat_arr[i] = stars[i].is_saturated;
        if (mag_arr) mag_arr[i] = stars[i].mag;
        if (has_sat_arr) has_sat_arr[i] = stars[i].has_saturated;
    }

    *out_x = x_coords;
    *out_y = y_coords;
    *out_flux = flux_arr;
    *out_saturated = sat_arr;
    if (out_mag) *out_mag = mag_arr;
    if (out_has_saturated) *out_has_saturated = has_sat_arr;
    *out_count = result_count;

    // 可选输出参数
    if (out_extras && extra_count > 0) {
        *out_extras = (float **)malloc(extra_count * sizeof(float *));
        for (int e = 0; e < extra_count; e++) {
            (*out_extras)[e] = (float *)malloc(result_count * sizeof(float));
            ExtraField field = parse_extra_name(extra_names[e]);
            for (int i = 0; i < result_count; i++) {
                (*out_extras)[e][i] = get_extra_field(stars[i], field);
            }
        }
    }

    auto t_final = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t_final - t0).count();
    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_ex done: %d stars (sat=%d normal=%d), %.3f s",
             result_count, sat_count, normal_count, elapsed);
    return 0;
}

// ============================================================================
// sdet_detect_ex - FP32 入口 (uint16 原始图像, 兼容旧 ABI)
//   内部 uint16→float32 转换后调用 sdet_detect_impl<float> (行为与旧版逐位一致)
// ============================================================================
SDET_EXPORT int sdet_detect_ex(StarDetectorHandle handle,
                               const uint16_t *image, int width, int height,
                               double **out_x, double **out_y, float **out_flux, int **out_saturated,
                               float **out_mag, int **out_has_saturated,
                               int *out_count,
                               const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || width <= 0 || height <= 0) return -1;
    size_t n = (size_t)width * height;
    std::vector<float> fimg(n);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }
    return sdet_detect_impl<float>(handle, fimg.data(), width, height,
                                   out_x, out_y, out_flux, out_saturated,
                                   out_mag, out_has_saturated, out_count,
                                   extra_names, extra_count, out_extras);
}

// ============================================================================
// sdet_detect_ex_f64 - FP64 入口 (double 校准图像)
//   R11 (PREC-108 第二步): 全程 double 检测, 不降级 float32
//   输出接口与 sdet_detect_ex 一致 (out_flux/mag float, 下游协议兼容)
// ============================================================================
SDET_EXPORT int sdet_detect_ex_f64(StarDetectorHandle handle,
                                   const double *image, int width, int height,
                                   double **out_x, double **out_y, float **out_flux, int **out_saturated,
                                   float **out_mag, int **out_has_saturated,
                                   int *out_count,
                                   const char **extra_names, int extra_count, float ***out_extras)
{
    if (!handle || !image || width <= 0 || height <= 0) return -1;
    return sdet_detect_impl<double>(handle, image, width, height,
                                    out_x, out_y, out_flux, out_saturated,
                                    out_mag, out_has_saturated, out_count,
                                    extra_names, extra_count, out_extras);
}

SDET_EXPORT void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated,
                                       float *mag, int *has_saturated,
                                       float **extras, int extra_count)
{
    free(x);
    free(y);
    free(flux);
    free(saturated);
    free(mag);
    free(has_saturated);
    if (extras && extra_count > 0) {
        for (int i = 0; i < extra_count; i++) {
            free(extras[i]);
        }
        free(extras);
    }
}
