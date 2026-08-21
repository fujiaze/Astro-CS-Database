/* Moffat4/PSF 拟合数值契约（B4-14，不改算法，仅文档锚点）：
 * SCI-PSF-001 / ALG-STAR-PSF-*：I=B+A/(1+Q)⁴；Q/dx·dy 定见 STAR_PSF_ALGORITHMS.md 伪代码与 docs/science/PSF.md；
 * 守卫：pivot 1e-30（数值奇异，45）、sx/sy≤0 哨兵 1e10（71-74）、Q<0 哨兵 1e10（89-91）、
 *  h=max(|x|·1e-6,1e-8)（114）、sx/sy 下界 0.3（169-170,326）、FWHM>rect 拒绝（333-341）、
 *  背景比 0.5（343-344）；对照表见 docs/science/PSF.md / docs/algorithms/STAR_PSF_ALGORITHMS.md。
 */
#include "dpsf_psf.h"
#include "dpsf_log.h"
#include "dpsf_image.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include <cstdlib>
#include <omp.h>

// Moffat4 (beta=4) FWHM 因子
// 模型使用 Q = 0.5 * r^2 / sigma^2 (注意 1/(2*sigma^2) 系数)
// 标准 Moffat: M = A/(1 + r^2/alpha^2)^beta, FWHM = 2*alpha*sqrt(2^(1/beta)-1)
// 由 Q = r^2/(2*sigma^2) = r^2/alpha^2 得 alpha = sqrt(2)*sigma
// 故 FWHM = 2*sqrt(2)*sigma*sqrt(2^(1/4)-1) = 0.87 * sqrt(2) * sigma ≈ 1.2303 * sigma
static const double MOFFAT4_FWHM_FACTOR = 1.230310;
static const int NPARAMS = 7;

struct SamplePixel {
    double dx;
    double dy;
    double val;
};

static bool gauss_solve(int n, const double* A, const double* b, double* x) {
    // CodeQL #1：int 乘法提升到 size_t 再分配，避免大矩阵尺寸溢出
    std::vector<double> aug((std::size_t)n * (std::size_t)(n + 1));
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

static void moffat4_residual(double* params, int m, void* userdata, double* fvec) {
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
        double ddx = samples[i].dx - x0;
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        if (Q < 0) {
            fvec[i] = 1e10;
            continue;
        }
        double model = B + A / std::pow(1.0 + Q, 4.0);
        fvec[i] = samples[i].val - model;
    }
}

static int lm_solve(int m, int n, double* x, void* userdata,
                    void (*residual_func)(double*, int, void*, double*),
                    double tol, int max_iter) {
    std::vector<double> fvec(m), fvec_new(m);
    std::vector<double> J((std::size_t)m * (std::size_t)n);
    std::vector<double> JtJ((std::size_t)n * (std::size_t)n), Jtf(n),
        delta(n), x_new(n);

    double lambda = 1e-3;

    residual_func(x, m, userdata, fvec.data());
    double cost = 0;
    for (int i = 0; i < m; i++) cost += fvec[i] * fvec[i];

    for (int iter = 0; iter < max_iter; iter++) {
        for (int j = 0; j < n; j++) {
            double h = std::max(std::abs(x[j]) * 1e-6, 1e-8);
            double xj_orig = x[j];
            x[j] = xj_orig + h;
            residual_func(x, m, userdata, fvec_new.data());
            x[j] = xj_orig;
            for (int i = 0; i < m; i++)
                J[i * n + j] = (fvec_new[i] - fvec[i]) / h;
        }

        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                double sum = 0;
                for (int k = 0; k < m; k++)
                    sum += J[k * n + i] * J[k * n + j];
                JtJ[i * n + j] = sum;
            }

        for (int i = 0; i < n; i++) {
            double sum = 0;
            for (int k = 0; k < m; k++)
                sum += J[k * n + i] * fvec[k];
            Jtf[i] = sum;
        }

        std::vector<double> A(JtJ);
        for (int i = 0; i < n; i++) A[i * n + i] += lambda;

        std::vector<double> rhs(n);
        for (int i = 0; i < n; i++) rhs[i] = -Jtf[i];

        if (!gauss_solve(n, A.data(), rhs.data(), delta.data())) {
            lambda *= 10.0;
            continue;
        }

        double norm_delta = 0, norm_x = 0;
        for (int i = 0; i < n; i++) {
            norm_delta += delta[i] * delta[i];
            norm_x += x[i] * x[i];
        }
        norm_delta = std::sqrt(norm_delta);
        norm_x = std::sqrt(norm_x);

        if (norm_delta < tol * (norm_x + 1e-30)) {
            dpsf_log(LOG_DEBUG, "DPSF", "LM converged at iter %d, cost=%.6f", iter, cost);
            return DPSF_FIT_OK;
        }

        for (int i = 0; i < n; i++) x_new[i] = x[i] + delta[i];
        residual_func(x_new.data(), m, userdata, fvec_new.data());
        double cost_new = 0;
        for (int i = 0; i < m; i++) cost_new += fvec_new[i] * fvec_new[i];

        if (cost_new < cost) {
            for (int i = 0; i < n; i++) x[i] = x_new[i];
            if (x[4] < 0.3) x[4] = 0.3;
            if (x[5] < 0.3) x[5] = 0.3;
            if (x[1] < 0.0) x[1] = 0.0;
            for (int i = 0; i < m; i++) fvec[i] = fvec_new[i];
            cost = cost_new;
            lambda *= 0.1;
        } else {
            lambda *= 10.0;
        }
    }

    dpsf_log(LOG_DEBUG, "DPSF", "LM hit iteration limit, cost=%.6f", cost);
    return DPSF_FIT_ITERATION_LIMIT;
}

static double compute_trimmed_mad(const SamplePixel* samples, int m, const double* params) {
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
        double ddx = samples[i].dx - x0;
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;
        double model = B + A / std::pow(1.0 + std::max(Q, 0.0), 4.0);
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

// 模板版本: 支持 float/double 输入 ( 双精度 ABI 改造)
// ImageT = float -> moffat4_fit (向后兼容)
// ImageT = double -> moffat4_fit_d (双精度, 不降级)
template<typename ImageT>
static int moffat4_fit_tmpl(const ImageT* image, int width, int height,
                            double cx, double cy,
                            int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                            DPSFFitResult* result) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::memset(result, 0, sizeof(DPSFFitResult));
    result->status = DPSF_FIT_INVALID_PARAMS;

    int rw = rect_x1 - rect_x0;
    int rh = rect_y1 - rect_y0;

    if (rw * rh < 9) {
        dpsf_log(LOG_WARN, "DPSF", "Rect area too small: %d", rw * rh);
        return DPSF_FIT_INVALID_PARAMS;
    }
    if (rect_x0 < 0 || rect_y0 < 0 || rect_x1 > width || rect_y1 > height) {
        dpsf_log(LOG_WARN, "DPSF", "Rect out of image bounds: [%d,%d]-[%d,%d] img=%dx%d",
               rect_x0, rect_y0, rect_x1, rect_y1, width, height);
        return DPSF_FIT_INVALID_PARAMS;
    }

    std::vector<SamplePixel> samples;
    samples.reserve((std::size_t)rw * (std::size_t)rh);
    for (int y = rect_y0; y < rect_y1; y++) {
        for (int x = rect_x0; x < rect_x1; x++) {
            SamplePixel sp;
            sp.dx = static_cast<double>(x) - cx;
            sp.dy = static_cast<double>(y) - cy;
            sp.val = static_cast<double>(image[y * width + x]);
            samples.push_back(sp);
        }
    }
    int m = static_cast<int>(samples.size());

    dpsf_log(LOG_DEBUG, "DPSF", "Sampled %d pixels from rect [%d,%d]-[%d,%d], center=(%.2f,%.2f)",
           m, rect_x0, rect_y0, rect_x1, rect_y1, cx, cy);

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

    double max_val = -1e30;
    for (int i = 0; i < m; i++)
        if (samples[i].val > max_val) max_val = samples[i].val;

    double A0 = max_val - bkg0;
    if (A0 <= 0) {
        dpsf_log(LOG_WARN, "DPSF", "Amplitude <= 0: A=%.2f max=%.2f bkg=%.2f", A0, max_val, bkg0);
        return DPSF_FIT_INVALID_PARAMS;
    }

    double sx0 = 0.15 * rw;
    double params[7] = { bkg0, A0, 0.0, 0.0, sx0, sx0, 0.0 };

    dpsf_log(LOG_INFO, "DPSF", "Initial params: B=%.2f A=%.2f x0=0 y0=0 sx=%.2f sy=%.2f theta=0",
           bkg0, A0, sx0, sx0);

    int lm_status = lm_solve(m, NPARAMS, params, static_cast<void*>(samples.data()),
                              moffat4_residual, 1e-8, 200);

    dpsf_log(LOG_INFO, "DPSF", "LM result: status=%d B=%.2f A=%.2f x0=%.4f y0=%.4f sx=%.4f sy=%.4f theta=%.4f",
           lm_status, params[0], params[1], params[2], params[3], params[4], params[5], params[6]);

    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    bool all_finite = std::isfinite(B) && std::isfinite(A) && std::isfinite(x0) &&
                      std::isfinite(y0) && std::isfinite(sx) && std::isfinite(sy) &&
                      std::isfinite(theta);
    if (!all_finite || A <= 0 || sx <= 0.3 || sy <= 0.3) {
        dpsf_log(LOG_WARN, "DPSF", "Invalid fit params: finite=%d A=%.2f sx=%.4f sy=%.4f",
               all_finite, A, sx, sy);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_FIT_NO_CONVERGENCE;
    }

    double fwhm_x = MOFFAT4_FWHM_FACTOR * sx;
    double fwhm_y = MOFFAT4_FWHM_FACTOR * sy;

    if (fwhm_x > rw || fwhm_y > rh) {
        dpsf_log(LOG_WARN, "DPSF", "FWHM exceeds rect: fwhm_x=%.2f fwhm_y=%.2f rect=%dx%d",
               fwhm_x, fwhm_y, rw, rh);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_FIT_NO_CONVERGENCE;
    }

    double bkg_range = std::max(bkg0, 0.01);
    if (std::abs(B - bkg0) / bkg_range > 0.5) {
        dpsf_log(LOG_WARN, "DPSF", "Background constraint violated: B=%.4f bkg0=%.4f ratio=%.4f",
               B, bkg0, std::abs(B - bkg0) / bkg_range);
        result->status = DPSF_FIT_NO_CONVERGENCE;
        return DPSF_FIT_NO_CONVERGENCE;
    }

    double thetas[4] = { theta, M_PI / 2.0 - theta, M_PI / 2.0 + theta, M_PI - theta };
    double best_mad = 1e30;
    double best_theta = theta;
    for (int t = 0; t < 4; t++) {
        double test_params[7] = { B, A, x0, y0, sx, sy, thetas[t] };
        double mad = compute_trimmed_mad(samples.data(), m, test_params);
        dpsf_log(LOG_DEBUG, "DPSF", "Theta disambig [%d]: theta=%.4f mad=%.4f", t, thetas[t], mad);
        if (mad < best_mad) {
            best_mad = mad;
            best_theta = thetas[t];
        }
    }
    theta = best_theta;

    double final_params[7] = { B, A, x0, y0, sx, sy, theta };
    double mad = compute_trimmed_mad(samples.data(), m, final_params);

    // Moffat4 (beta=4) 解析积分: flux = 2 * pi * A * sx * sy / (beta - 1) = 2 * pi * A * sx * sy / 3
    double flux = 2.0 * M_PI * A * sx * sy / 3.0;

    double sx_max = std::max(sx, sy), sx_min = std::min(sx, sy);
    double eccentricity = std::sqrt(1.0 - (sx_min / sx_max) * (sx_min / sx_max));

    // Phase1 Final Closure (PSF-001): 样本 dx = pixel_x - cx,
    // 拟合 x0 是相对传入中心 cx 的偏移, 正确还原为 cx + x0。
    // 原实现用 rect 中心近似 cx, 对奇数宽 rect 引入 ~0.5px 系统偏差。
    double img_cx = cx + x0;
    double img_cy = cy + y0;

    auto t1 = std::chrono::high_resolution_clock::now();
    dpsf_log(LOG_INFO, "DPSF", "Fit done: %.1f ms status=%d cx=%.2f cy=%.2f fwhm_x=%.2f fwhm_y=%.2f mad=%.4f ecc=%.4f",
           std::chrono::duration<double, std::milli>(t1 - t0).count(),
           lm_status, img_cx, img_cy, fwhm_x, fwhm_y, mad, eccentricity);

    result->status = lm_status;
    result->B = B;
    result->A = A;
    result->cx = img_cx;
    result->cy = img_cy;
    result->sx = sx;
    result->sy = sy;
    result->theta = theta;
    result->fwhm_x = fwhm_x;
    result->fwhm_y = fwhm_y;
    result->mad = mad;
    result->flux = flux;
    result->eccentricity = eccentricity;

    return result->status;
}

// float 版本 (向后兼容, 原有签名)
int moffat4_fit(const float* image, int width, int height,
                double cx, double cy,
                int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                DPSFFitResult* result) {
    return moffat4_fit_tmpl<float>(image, width, height, cx, cy,
                                    rect_x0, rect_y0, rect_x1, rect_y1, result);
}

// double 版本 (双精度 ABI, 新增)
// FP64 模式下采样像素值直接为 double, 不降级到 float32 (精度关键路径)
int moffat4_fit_d(const double* image, int width, int height,
                  double cx, double cy,
                  int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                  DPSFFitResult* result) {
    return moffat4_fit_tmpl<double>(image, width, height, cx, cy,
                                     rect_x0, rect_y0, rect_x1, rect_y1, result);
}

DPSF_EXPORT int dpsf_fit(const uint16_t *image, int width, int height,
                          double cx, double cy,
                          const DPSFFitParams *params,
                          DPSFFitResult *result) {
    if (!image || !params || !result || width <= 0 || height <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit: invalid arguments");
        return DPSF_FIT_INVALID_PARAMS;
    }

    int fitRadius = params->fitRadius;

    int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
    int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
    int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
    int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

    int rw = x1 - x0;
    int rh = y1 - y0;
    if (rw <= 0 || rh <= 0) {
        dpsf_log(LOG_WARN, "DPSF", "dpsf_fit: empty rect for cx=%.2f cy=%.2f fitRadius=%d", cx, cy, fitRadius);
        std::memset(result, 0, sizeof(DPSFFitResult));
        result->status = DPSF_FIT_INVALID_PARAMS;
        return DPSF_FIT_INVALID_PARAMS;
    }

    std::vector<float> float_patch((size_t)rw * rh);
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float_patch[(y - y0) * rw + (x - x0)] =
                static_cast<float>(image[y * width + x]);
        }
    }

    double local_cx = cx - x0;
    double local_cy = cy - y0;

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit: cx=%.2f cy=%.2f rect=[%d,%d]-[%d,%d] local_cx=%.2f local_cy=%.2f fitRadius=%d",
           cx, cy, x0, y0, x1, y1, local_cx, local_cy, fitRadius);

    int ret = moffat4_fit(float_patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, result);

    if (ret == DPSF_FIT_OK || ret == DPSF_FIT_ITERATION_LIMIT) {
        result->cx += x0;
        result->cy += y0;
    }

    return ret;
}

// 前向声明 (float32 拟合核心, 定义在 dpsf_fit_batch 之后)
static int fit_batch_float_image(const float *float_image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results);

DPSF_EXPORT int dpsf_fit_batch(const uint16_t *image, int width, int height,
                                const double *cx_array, const double *cy_array, int count,
                                const DPSFFitParams *params,
                                DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();
    (void)t0;  /* 非 DPSF_PROFILE 构建时避免 unused 告警 */

    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch: invalid arguments");
        return -1;
    }

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch: %d points, %dx%d image, fitRadius=%d",
           count, width, height, params->fitRadius);

    size_t n_pixels = (size_t)width * height;
    std::vector<float> float_image(n_pixels);
    for (size_t i = 0; i < n_pixels; i++) {
        float_image[i] = static_cast<float>(image[i]);
    }

    return fit_batch_float_image(float_image.data(), width, height,
                                 cx_array, cy_array, count, params, out_results);
}

// ============================================================================
// fit_batch_float_image - float32 图像批量 PSF 拟合核心
// 输出 DPSFFitResult* (status/B/flux/cx/cy/fwhm/A/mad/eccentricity), 与 dpsf_fit_batch 一致
// ============================================================================
static int fit_batch_float_image(const float *float_image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    if (!float_image || !cx_array || !cy_array || !params || !out_results || count <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: invalid arguments");
        return -1;
    }
    DPSFFitResult *results = (DPSFFitResult *)malloc(count * sizeof(DPSFFitResult));
    if (!results) {
        dpsf_log(LOG_ERROR, "DPSF", "fit_batch_float_image: failed to allocate results");
        return -1;
    }

    int fitRadius = params->fitRadius;
    int success_count = 0;

#pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < count; i++) {
        double cx = cx_array[i];
        double cy = cy_array[i];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        if (rw <= 0 || rh <= 0) {
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        std::vector<float> patch((size_t)rw * rh);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                patch[(y - y0) * rw + (x - x0)] = float_image[y * width + x];
            }
        }

        double local_cx = cx - x0;
        double local_cy = cy - y0;

        moffat4_fit(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &results[i]);

        if (results[i].status == DPSF_FIT_OK || results[i].status == DPSF_FIT_ITERATION_LIMIT) {
            results[i].cx += x0;
            results[i].cy += y0;
        }

        if (results[i].status == DPSF_FIT_OK) {
            success_count++;
        }
    }

    *out_results = results;

    dpsf_log(LOG_INFO, "DPSF", "fit_batch_float_image done: %d/%d success",
           success_count, count);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f - float32 图像直通拟合 (, PREC-105: 无 uint16 有损转换)
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f(const float *image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();
    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_f: invalid arguments");
        return -1;
    }
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_f: %d points, %dx%d image, fitRadius=%d (float32)",
             count, width, height, params->fitRadius);
    int ret = fit_batch_float_image(image, width, height,
                                    cx_array, cy_array, count, params, out_results);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_f done: %.3f s", elapsed);
    return ret;
}

DPSF_EXPORT void dpsf_free_results(DPSFFitResult *results) {
    free(results);
}

// ============================================================================
// dpsf_fit_batch_d (双精度 ABI, 新增)
//
// 与 dpsf_fit_batch (uint16) 逻辑一致, 仅 image 数据类型从 uint16 改为 double。
// FP64 模式下直接在 double 图像上裁剪局部 patch 送入 moffat4_fit_d (double 拟合),
// 不创建整张 uint16/float 图像, 不降级 (精度关键路径)。
// 返回完整 DPSFFitResult 结构体 (含 status/flux/mad/eccentricity 等全部字段),
// 供 orchestrator 写出与 FP32 路径一致的 psf 块布局。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_d(const double *image, int width, int height,
                                 const double *cx_array, const double *cy_array, int count,
                                 const DPSFFitParams *params,
                                 DPSFFitResult **out_results) {
    auto t0 = std::chrono::high_resolution_clock::now();

    if (!image || !cx_array || !cy_array || !params || !out_results || count <= 0) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: invalid arguments");
        return -1;
    }

    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_d: %d points, %dx%d image (FP64), fitRadius=%d",
           count, width, height, params->fitRadius);

    DPSFFitResult *results = (DPSFFitResult *)malloc(count * sizeof(DPSFFitResult));
    if (!results) {
        dpsf_log(LOG_ERROR, "DPSF", "dpsf_fit_batch_d: failed to allocate results");
        return -1;
    }

    int fitRadius = params->fitRadius;
    int success_count = 0;

#pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < count; i++) {
        double cx = cx_array[i];
        double cy = cy_array[i];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width, static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        if (rw <= 0 || rh <= 0) {
            std::memset(&results[i], 0, sizeof(DPSFFitResult));
            results[i].status = DPSF_FIT_INVALID_PARAMS;
            continue;
        }

        // FP64 路径: 直接从 double 图像裁剪 patch (不降级到 float32)
        std::vector<double> patch((size_t)rw * rh);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
            }
        }

        double local_cx = cx - x0;
        double local_cy = cy - y0;

        moffat4_fit_d(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &results[i]);

        if (results[i].status == DPSF_FIT_OK || results[i].status == DPSF_FIT_ITERATION_LIMIT) {
            results[i].cx += x0;
            results[i].cy += y0;
        }

        if (results[i].status == DPSF_FIT_OK) {
            success_count++;
        }
    }

    *out_results = results;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF", "dpsf_fit_batch_d done: %d/%d success, %.3f s (FP64)",
           success_count, count, elapsed);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f32 (, v1.1)
//
// float32 PSF 批量拟合, 消费 star_det v1 (FLOAT64 [N,6])。
// 不做 0-65535 clip, 不创建整张 uint16 图像, 不调用 sdet_detect_ex。
// 内部直接在 float32 图像上裁剪局部 patch 送入 moffat4_fit (float/double 拟合)。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f32(
    const float *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid
) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // ---- 参数校验 ----
    if (!image || !detections || !out_psf_params || !out_n_valid ||
        width <= 0 || height <= 0 || n_detections <= 0) {
        dpsf_log(LOG_ERROR, "DPSF",
                 "dpsf_fit_batch_f32: invalid arguments (image=%p detections=%p out=%p n_valid=%p w=%d h=%d n=%d)",
                 image, detections, out_psf_params, out_n_valid, width, height, n_detections);
        return -1;
    }

    // ---- 默认参数 ----
    DPSFFitParams default_params;
    default_params.fitRadius = 8;
    default_params.maxIter = 200;
    default_params.tolerance = 1e-8;
    const DPSFFitParams *p = params ? params : &default_params;
    int fitRadius = p->fitRadius;

    // ---- 记录消费的 schema / count (§8 要求) ----
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f32: consume schema=%s count=%d img=%dx%d fitRadius=%d",
             DPSF_STAR_DET_SCHEMA_V1, n_detections, width, height, fitRadius);

    // ---- 初始化输出: 全部置 NaN, n_valid=0 ----
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < n_detections * 9; i++) {
        out_psf_params[i] = nan_val;
    }
    *out_n_valid = 0;

    int success_count = 0;

    // ---- OpenMP 并行批量拟合 ----
    #pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < n_detections; i++) {
        // star_det v1: [0]=x_px, [1]=y_px, [2]=flux, [3]=mag,
        // [4]=saturated, [5]=has_saturated
        const double *row = detections + (size_t)i * 6;
        double cx = row[0];
        double cy = row[1];

        // 计算 local rect (与 dpsf_fit_batch 保持一致)
        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width,  static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        double *out_row = out_psf_params + (size_t)i * 9;

        if (rw <= 0 || rh <= 0) {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f32: star %d empty rect cx=%.2f cy=%.2f", i, cx, cy);
            // out_row 已为 NaN
            continue;
        }

        // 直接从 float32 图像裁剪 patch (不创建整张 uint16 图像, 不 clip)
        std::vector<float> patch((size_t)rw * rh);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
            }
        }

        double local_cx = cx - x0;
        double local_cy = cy - y0;

        DPSFFitResult result;
        moffat4_fit(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &result);

        if (result.status == DPSF_FIT_OK || result.status == DPSF_FIT_ITERATION_LIMIT) {
            // 把局部坐标转回图像坐标
            result.cx += x0;
            result.cy += y0;
        }

        if (result.status == DPSF_FIT_OK) {
            // 填充 9 字段输出: B, A, cx, cy, sx, sy, theta, fwhm_x, fwhm_y
            out_row[0] = result.B;
            out_row[1] = result.A;
            out_row[2] = result.cx;
            out_row[3] = result.cy;
            out_row[4] = result.sx;
            out_row[5] = result.sy;
            out_row[6] = result.theta;
            out_row[7] = result.fwhm_x;
            out_row[8] = result.fwhm_y;
            success_count++;
        } else {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f32: star %d fit failed status=%d cx=%.2f cy=%.2f",
                     i, result.status, cx, cy);
            // out_row 已为 NaN
        }
    }

    *out_n_valid = success_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f32 done: %d/%d valid, schema=%s, %.3f s",
             success_count, n_detections, DPSF_STAR_DET_SCHEMA_V1, elapsed);

    return 0;
}

// ============================================================================
// dpsf_fit_batch_f64 (双精度 ABI, 新增)
//
// double PSF 批量拟合, 消费 star_det v1 (FLOAT64 [N,6])。
// 与 dpsf_fit_batch_f32 逻辑一致, 仅 image 数据类型从 float 改为 double。
// 内部调用 moffat4_fit_d, 采样像素值直接为 double (不降级到 float32)。
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f64(
    const double *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid
) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // ---- 参数校验 ----
    if (!image || !detections || !out_psf_params || !out_n_valid ||
        width <= 0 || height <= 0 || n_detections <= 0) {
        dpsf_log(LOG_ERROR, "DPSF",
                 "dpsf_fit_batch_f64: invalid arguments (image=%p detections=%p out=%p n_valid=%p w=%d h=%d n=%d)",
                 image, detections, out_psf_params, out_n_valid, width, height, n_detections);
        return -1;
    }

    // ---- 默认参数 ----
    DPSFFitParams default_params;
    default_params.fitRadius = 8;
    default_params.maxIter = 200;
    default_params.tolerance = 1e-8;
    const DPSFFitParams *p = params ? params : &default_params;
    int fitRadius = p->fitRadius;

    // ---- 记录消费的 schema / count ----
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f64: consume schema=%s count=%d img=%dx%d fitRadius=%d (FP64)",
             DPSF_STAR_DET_SCHEMA_V1, n_detections, width, height, fitRadius);

    // ---- 初始化输出: 全部置 NaN, n_valid=0 ----
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < n_detections * 9; i++) {
        out_psf_params[i] = nan_val;
    }
    *out_n_valid = 0;

    int success_count = 0;

    // ---- OpenMP 并行批量拟合 (FP64: 直接从 double 图像裁剪 patch) ----
    #pragma omp parallel for schedule(dynamic) reduction(+:success_count)
    for (int i = 0; i < n_detections; i++) {
        const double *row = detections + (size_t)i * 6;
        double cx = row[0];
        double cy = row[1];

        int x0 = std::max(0, static_cast<int>(cx) - fitRadius);
        int y0 = std::max(0, static_cast<int>(cy) - fitRadius);
        int x1 = std::min(width,  static_cast<int>(cx) + fitRadius + 1);
        int y1 = std::min(height, static_cast<int>(cy) + fitRadius + 1);

        int rw = x1 - x0;
        int rh = y1 - y0;

        double *out_row = out_psf_params + (size_t)i * 9;

        if (rw <= 0 || rh <= 0) {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f64: star %d empty rect cx=%.2f cy=%.2f", i, cx, cy);
            continue;
        }

        // FP64 路径: 直接从 double 图像裁剪 patch (不降级到 float32)
        std::vector<double> patch((size_t)rw * rh);
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                patch[(y - y0) * rw + (x - x0)] = image[(size_t)y * width + x];
            }
        }

        double local_cx = cx - x0;
        double local_cy = cy - y0;

        DPSFFitResult result;
        moffat4_fit_d(patch.data(), rw, rh, local_cx, local_cy, 0, 0, rw, rh, &result);

        if (result.status == DPSF_FIT_OK || result.status == DPSF_FIT_ITERATION_LIMIT) {
            result.cx += x0;
            result.cy += y0;
        }

        if (result.status == DPSF_FIT_OK) {
            out_row[0] = result.B;
            out_row[1] = result.A;
            out_row[2] = result.cx;
            out_row[3] = result.cy;
            out_row[4] = result.sx;
            out_row[5] = result.sy;
            out_row[6] = result.theta;
            out_row[7] = result.fwhm_x;
            out_row[8] = result.fwhm_y;
            success_count++;
        } else {
            dpsf_log(LOG_DEBUG, "DPSF",
                     "dpsf_fit_batch_f64: star %d fit failed status=%d cx=%.2f cy=%.2f",
                     i, result.status, cx, cy);
        }
    }

    *out_n_valid = success_count;

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    dpsf_log(LOG_INFO, "DPSF",
             "dpsf_fit_batch_f64 done: %d/%d valid, schema=%s, %.3f s (FP64)",
             success_count, n_detections, DPSF_STAR_DET_SCHEMA_V1, elapsed);

    return 0;
}
