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
#include <cmath>
#include <set>
#include <unordered_map>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct StarDetectorHandle_s {
    StarDetectorInternal internal;
};

namespace {

static const double MOFFAT4_FWHM_FACTOR = 0.8700;
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

// 统一星点数据结构（正常星+饱和星共用）
struct StarRecord {
    double cx, cy;
    float flux;          // 正常星=振幅A，饱和星=-1
    int is_saturated;     // 0=正常星，1=饱和星
    // 正常星Moffat拟合数据
    float fwhm_x, fwhm_y;
    float sx, sy, theta;
    float background, amplitude;
    // 饱和星圆盘拟合数据
    float r;             // 等效半径，正常星=0
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

void sdet_moffat4_residual(double* params, int m, void* userdata, double* fvec) {
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

void sdet_moffat4_residual_and_jacobian(double* params, int m, void* userdata, double* fvec, double* J) {
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
        double ddx = samples[i].dx - x0;
        double ddy = samples[i].dy - y0;
        double Q = p1 * ddx * ddx + 2.0 * p2 * ddx * ddy + p3 * ddy * ddy;

        if (Q < 0) {
            fvec[i] = 1e10;
            if (J) {
                for (int j = 0; j < NPARAMS; j++) J[i * NPARAMS + j] = 0.0;
            }
            continue;
        }

        double Qp1 = 1.0 + Q;
        double inv_Qp4 = 1.0 / (Qp1 * Qp1 * Qp1 * Qp1);
        double inv_Qp5 = inv_Qp4 / Qp1;

        double model = B + A * inv_Qp4;
        fvec[i] = samples[i].val - model;

        if (J) {
            double dx2 = ddx * ddx;
            double dxy = ddx * ddy;
            double dy2 = ddy * ddy;
            double p1_dx_p2_dy = p1 * ddx + p2 * ddy;
            double p2_dx_p3_dy = p2 * ddx + p3 * ddy;

            J[i * NPARAMS + 0] = -1.0;
            J[i * NPARAMS + 1] = -inv_Qp4;
            J[i * NPARAMS + 2] = -8.0 * A * p1_dx_p2_dy * inv_Qp5;
            J[i * NPARAMS + 3] = -8.0 * A * p2_dx_p3_dy * inv_Qp5;
            J[i * NPARAMS + 4] = -4.0 * A * (cos2 * dx2 + sin2t * dxy + sin2 * dy2) * inv_sx3 * inv_Qp5;
            J[i * NPARAMS + 5] = -4.0 * A * (sin2 * dx2 - sin2t * dxy + cos2 * dy2) * inv_sy3 * inv_Qp5;
            double dQ_dtheta = dp1_dtheta * dx2 + 2.0 * dp2_dtheta * dxy + dp3_dtheta * dy2;
            J[i * NPARAMS + 6] = 4.0 * A * dQ_dtheta * inv_Qp5;
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

    double cost_prev = cost;
    int stall_count = 0;

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
            if (x[4] < 0.3) x[4] = 0.3;
            if (x[5] < 0.3) x[5] = 0.3;
            if (x[1] < 0.0) x[1] = 0.0;
            rj_func(x, m, userdata, fvec_ptr, J_ptr);
            cost = 0;
            for (int i = 0; i < m; i++) cost += fvec_ptr[i] * fvec_ptr[i];
            lambda *= 0.1;

            // 提前终止：cost下降<1%时累计stall计数
            if (cost_prev > 0 && (cost_prev - cost) / cost_prev < 0.01) {
                stall_count++;
                if (stall_count >= 20) {
                    return SDET_FIT_NO_CONVERGENCE;
                }
            } else {
                stall_count = 0;
            }
            cost_prev = cost;
        } else {
            lambda *= 10.0;
        }
    }

    return SDET_FIT_ITERATION_LIMIT;
}

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

int sdet_moffat4_fit(const float* image, int width, int height,
                     double cx, double cy,
                     int rect_x0, int rect_y0, int rect_x1, int rect_y1,
                     InternalFitResult* result, LMWorkspace* ws = nullptr) {
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
            SamplePixel sp;
            sp.dx = static_cast<double>(x) - cx;
            sp.dy = static_cast<double>(y) - cy;
            sp.val = static_cast<double>(image[y * width + x]);
            samples.push_back(sp);
        }
    }
    int m = static_cast<int>(samples.size());

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
    if (A0 <= 0) return SDET_FIT_INVALID_PARAMS;

    double sx0 = 0.15 * rw;
    double params[7] = { bkg0, A0, 0.0, 0.0, sx0, sx0, 0.0 };

    int lm_status = sdet_lm_solve(m, NPARAMS, params, static_cast<void*>(samples.data()),
                                   sdet_moffat4_residual_and_jacobian, 1e-8, 100, ws);

    double B = params[0], A = params[1], x0 = params[2], y0 = params[3];
    double sx = params[4], sy = params[5], theta = params[6];

    bool all_finite = std::isfinite(B) && std::isfinite(A) && std::isfinite(x0) &&
                      std::isfinite(y0) && std::isfinite(sx) && std::isfinite(sy) &&
                      std::isfinite(theta);
    if (!all_finite || A <= 0 || sx <= 0.3 || sy <= 0.3) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    double fwhm_x = MOFFAT4_FWHM_FACTOR * sx;
    double fwhm_y = MOFFAT4_FWHM_FACTOR * sy;

    if (fwhm_x > rw || fwhm_y > rh) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    double bkg_range = std::max(bkg0, 0.01);
    if (std::abs(B - bkg0) / bkg_range > 0.5) {
        result->status = SDET_FIT_NO_CONVERGENCE;
        return SDET_FIT_NO_CONVERGENCE;
    }

    double thetas[4] = { theta, M_PI / 2.0 - theta, M_PI / 2.0 + theta, M_PI - theta };
    double best_mad = 1e30;
    double best_theta = theta;
    for (int t = 0; t < 4; t++) {
        double test_params[7] = { B, A, x0, y0, sx, sy, thetas[t] };
        double mad = sdet_compute_trimmed_mad(samples.data(), m, test_params);
        if (mad < best_mad) {
            best_mad = mad;
            best_theta = thetas[t];
        }
    }
    theta = best_theta;

    double final_params[7] = { B, A, x0, y0, sx, sy, theta };
    double mad = sdet_compute_trimmed_mad(samples.data(), m, final_params);

    double img_cx = x0 + cx;
    double img_cy = y0 + cy;

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

    return result->status;
}

// 半阈值饱和星检测
struct SaturatedCandidate {
    double cx, cy;
    float r;
    int pixel_count;
};

void sdet_detect_saturated_stars(const float* fimg, int width, int height,
                                  std::vector<SaturatedCandidate>& sat_stars) {
    auto t0 = std::chrono::high_resolution_clock::now();

    size_t n = (size_t)width * height;

    // 计算半阈值 = (max + min) / 2
    float img_min = 1e30f, img_max = -1e30f;
    #pragma omp parallel for reduction(min:img_min) reduction(max:img_max) schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        if (fimg[i] < img_min) img_min = fimg[i];
        if (fimg[i] > img_max) img_max = fimg[i];
    }

    float half_threshold = (img_max + img_min) * 0.5f;
    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star half-threshold: %.1f (min=%.1f max=%.1f)",
             half_threshold, img_min, img_max);

    // 二值化
    std::vector<float> binary(n, 0.0f);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        if (fimg[i] > half_threshold) binary[i] = 1.0f;
    }

    // 连通域分析
    ConnectedComponent* components = nullptr;
    int comp_count = 0;
    sdet_find_connected_components(binary.data(), width, height, &components, &comp_count);

    sdet_log(SDET_LOG_INFO, "SDET", "Saturated star connected components: %d", comp_count);

    // 对每个连通域计算加权重心+等效半径
    for (int i = 0; i < comp_count; i++) {
        if (components[i].count <= 4) continue;
        // 最低2x2包围盒
        int bw = components[i].x1 - components[i].x0 + 1;
        int bh = components[i].y1 - components[i].y0 + 1;
        if (bw < 2 || bh < 2) continue;
        // 长宽比>3丢弃
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

        // 等效半径 r = sqrt(pixel_count / π)
        float r = std::sqrt((float)components[i].count / (float)M_PI);

        sat_stars.push_back({cx, cy, r, components[i].count});
    }
    sdet_free_connected_components(components, comp_count);

    auto t1 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "SDET", "Saturated stars: %d (%.1f ms)",
             (int)sat_stars.size(), std::chrono::duration<double, std::milli>(t1 - t0).count());
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
    return EXTRA_UNKNOWN;
}

float get_extra_field(const StarRecord& star, ExtraField field) {
    switch (field) {
        case EXTRA_FWHM_X:     return star.is_saturated ? -1.0f : star.fwhm_x;
        case EXTRA_FWHM_Y:     return star.is_saturated ? -1.0f : star.fwhm_y;
        case EXTRA_SX:         return star.is_saturated ? -1.0f : star.sx;
        case EXTRA_SY:         return star.is_saturated ? -1.0f : star.sy;
        case EXTRA_THETA:      return star.is_saturated ? -1.0f : star.theta;
        case EXTRA_BACKGROUND: return star.is_saturated ? -1.0f : star.background;
        case EXTRA_AMPLITUDE:  return star.is_saturated ? -1.0f : star.amplitude;
        case EXTRA_R:          return star.is_saturated ? star.r : 0.0f;
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

    // 饱和星与正常星去重：距离<2px时丢弃饱和星
    std::vector<uint8_t> sat_deleted(sat_idx.size(), 0);
    for (int si = 0; si < (int)sat_idx.size(); si++) {
        int i = sat_idx[si];
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        bool too_close = false;
        for (int dy = -1; dy <= 1 && !too_close; dy++) {
            for (int dx = -1; dx <= 1 && !too_close; dx++) {
                auto it = normal_grid.find({gx + dx, gy + dy});
                if (it == normal_grid.end()) continue;
                for (int ni : it->second) {
                    double ddx = stars[i].cx - stars[ni].cx;
                    double ddy = stars[i].cy - stars[ni].cy;
                    if (ddx * ddx + ddy * ddy < 4.0) {
                        too_close = true;
                        break;
                    }
                }
            }
        }
        if (too_close) sat_deleted[si] = 1;
    }

    // 饱和星之间去重：保留r较大的
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
                        // 保留r较大的
                        if (stars[i].r >= stars[j].r) sat_deleted[sj] = 1;
                        else { sat_deleted[si] = 1; goto next_sat; }
                    }
                }
            }
        }
        next_sat:;
    }

    // 正常星之间去重
    std::vector<uint8_t> normal_deleted(normal_idx.size(), 0);
    for (int ni = 0; ni < (int)normal_idx.size(); ni++) {
        if (normal_deleted[ni]) continue;
        int i = normal_idx[ni];
        int gx = (int)stars[i].cx / grid_sz;
        int gy = (int)stars[i].cy / grid_sz;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                auto it = normal_grid.find({gx + dx, gy + dy});
                if (it == normal_grid.end()) continue;
                for (int nj : it->second) {
                    if (nj == ni || normal_deleted[nj]) continue;
                    int j = normal_idx[nj];
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
        if (!normal_deleted[ni]) result.push_back(stars[normal_idx[ni]]);
    }

    stars = std::move(result);
}

// 排序：饱和星按r降序在前，正常星按flux降序在后
void sdet_sort_stars(std::vector<StarRecord>& stars) {
    std::stable_sort(stars.begin(), stars.end(), [](const StarRecord& a, const StarRecord& b) {
        if (a.is_saturated != b.is_saturated) return a.is_saturated > b.is_saturated; // 饱和星在前
        if (a.is_saturated) return a.r > b.r; // 饱和星按r降序
        return a.flux > b.flux; // 正常星按flux降序
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
        defaults.maxStars = 0;
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
            float fwhm_hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
            if (avg_fwhm < fwhm_lo || avg_fwhm > fwhm_hi) { f_fwhm++; continue; }
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

    sdet_log(SDET_LOG_INFO, "SDET", "Debug candidates: %d (from %d connected components, filtered ≤4px+2x2+ar≤3)",
             (int)candidates.size(), comp_count);

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
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

    // 构建正常星StarRecord列表
    std::vector<StarRecord> stars;
    int f_fit = 0, f_fwhm = 0, f_round = 0;

    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status == SDET_FIT_OK) {
            float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
            if (fwhm_mad_val > 0.0f) {
                float lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
                float hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
                if (avg_fwhm < lo || avg_fwhm > hi) { f_fwhm++; continue; }
            }
            float ar = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                                std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
            if (ar > params.maxAxisRatio) { f_round++; continue; }
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
            stars.push_back(rec);
        } else {
            f_fit++;
        }
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug normal stars: %d (fit_fail=%d fwhm=%d roundness=%d)",
             (int)stars.size(), f_fit, f_fwhm, f_round);

    // 半阈值饱和星检测
    std::vector<SaturatedCandidate> sat_candidates;
    sdet_detect_saturated_stars(fimg.data(), width, height, sat_candidates);

    for (const auto& sc : sat_candidates) {
        StarRecord rec;
        rec.cx = sc.cx;
        rec.cy = sc.cy;
        rec.flux = -1.0f;
        rec.is_saturated = 1;
        rec.fwhm_x = 0.0f;
        rec.fwhm_y = 0.0f;
        rec.sx = 0.0f;
        rec.sy = 0.0f;
        rec.theta = 0.0f;
        rec.background = -1.0f;
        rec.amplitude = -1.0f;
        rec.r = sc.r;
        stars.push_back(rec);
    }

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
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    for (int i = 0; i < result_count; i++) {
        x_coords[i] = stars[i].cx;
        y_coords[i] = stars[i].cy;
    }

    *out_x = x_coords;
    *out_y = y_coords;
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

SDET_EXPORT int sdet_detect_ex(StarDetectorHandle handle,
                                const uint16_t *image, int width, int height,
                                double **out_x, double **out_y, float **out_flux, int **out_saturated, int *out_count,
                                const char **extra_names, int extra_count, float ***out_extras)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    auto t_last = t0;
    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_ex start: %dx%d", width, height);

    if (!handle || !image || !out_x || !out_y || !out_flux || !out_saturated || !out_count) return -1;

    const SDetParams &params = handle->internal.params;
    size_t n = (size_t)width * height;

    // 阶段1: uint16→float32转换
    std::vector<float> fimg(n);
    #pragma omp parallel for schedule(static) num_threads(16)
    for (int i = 0; i < (int)n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[1] uint16→float: %.1f ms", std::chrono::duration<double, std::milli>(t1 - t_last).count());
    t_last = t1;

    handle->internal.width = width;
    handle->internal.height = height;

    // 阶段2: 动态背景分离→细节层
    std::vector<float> map(n);
    sdet_get_structure_map(&handle->internal, fimg.data(), width, height, map.data());
    auto t2 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[2] Dynamic background: %.1f ms", std::chrono::duration<double, std::milli>(t2 - t_last).count());
    t_last = t2;

    float *raw_detail = handle->internal.raw_detail;
    handle->internal.raw_detail = nullptr;

    // 阶段3: 细节层>0二值化
    std::vector<float> binary(n, 0.0f);
    if (raw_detail) {
        #pragma omp parallel for schedule(static) num_threads(16)
        for (int i = 0; i < (int)n; i++) {
            if (raw_detail[i] > 0.0f) binary[i] = 1.0f;
        }
        delete[] raw_detail;
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[3] Binary (>0): %.1f ms", std::chrono::duration<double, std::milli>(t3 - t_last).count());
    t_last = t3;

    // 阶段4: 连通域分析
    ConnectedComponent *components = nullptr;
    int comp_count = 0;
    sdet_find_connected_components(binary.data(), width, height, &components, &comp_count);
    auto t4 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[4] Connected components: %.1f ms (%d components)", 
             std::chrono::duration<double, std::milli>(t4 - t_last).count(), comp_count);
    t_last = t4;

    // 阶段5: 候选提取+加权重心
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
    auto t5 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[5] Candidates extraction: %.1f ms (%d candidates, filtered ≤4px+2x2+ar≤3)", 
             std::chrono::duration<double, std::milli>(t5 - t_last).count(), (int)candidates.size());
    t_last = t5;

    sdet_log(SDET_LOG_INFO, "SDET", "Candidates: %d (from %d connected components, filtered single-pixel)",
             (int)candidates.size(), comp_count);

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_flux = nullptr;
        *out_saturated = nullptr;
        *out_count = 0;
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

    // 阶段6: Moffat4拟合
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

    // 阶段8: 构建正常星StarRecord列表
    std::vector<StarRecord> stars;
    int f_fit = 0, f_fwhm = 0, f_round = 0;

    for (int i = 0; i < cc_count; i++) {
        if (fit_results[i].status == SDET_FIT_OK) {
            float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
            if (fwhm_mad_val > 0.0f) {
                float fwhm_lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
                float fwhm_hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
                if (avg_fwhm < fwhm_lo || avg_fwhm > fwhm_hi) { f_fwhm++; continue; }
            }
            float axis_ratio = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                                        std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
            if (axis_ratio > params.maxAxisRatio) { f_round++; continue; }
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
            stars.push_back(rec);
        } else {
            f_fit++;
        }
    }
    auto t8 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[8] Build normal stars: %.1f ms (%d stars, fit_fail=%d fwhm=%d round=%d)", 
             std::chrono::duration<double, std::milli>(t8 - t_last).count(), (int)stars.size(), f_fit, f_fwhm, f_round);
    t_last = t8;

    sdet_log(SDET_LOG_INFO, "SDET", "Normal stars: %d (fit_fail=%d fwhm=%d roundness=%d)",
             (int)stars.size(), f_fit, f_fwhm, f_round);

    // 阶段9: 半阈值饱和星检测
    std::vector<SaturatedCandidate> sat_candidates;
    sdet_detect_saturated_stars(fimg.data(), width, height, sat_candidates);
    auto t9 = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_DEBUG, "SDET", "[9] Saturated stars: %.1f ms (%d candidates)", 
             std::chrono::duration<double, std::milli>(t9 - t_last).count(), (int)sat_candidates.size());
    t_last = t9;

    for (const auto& sc : sat_candidates) {
        StarRecord rec;
        rec.cx = sc.cx;
        rec.cy = sc.cy;
        rec.flux = -1.0f;
        rec.is_saturated = 1;
        rec.fwhm_x = 0.0f;
        rec.fwhm_y = 0.0f;
        rec.sx = 0.0f;
        rec.sy = 0.0f;
        rec.theta = 0.0f;
        rec.background = -1.0f;
        rec.amplitude = -1.0f;
        rec.r = sc.r;
        stars.push_back(rec);
    }

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
        *out_count = 0;
        if (out_extras && extra_count > 0) *out_extras = nullptr;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    float *flux_arr = (float *)malloc(result_count * sizeof(float));
    int *sat_arr = (int *)malloc(result_count * sizeof(int));

    if (!x_coords || !y_coords || !flux_arr || !sat_arr) {
        free(x_coords); free(y_coords); free(flux_arr); free(sat_arr);
        return -1;
    }

    for (int i = 0; i < result_count; i++) {
        x_coords[i] = stars[i].cx;
        y_coords[i] = stars[i].cy;
        flux_arr[i] = stars[i].flux;
        sat_arr[i] = stars[i].is_saturated;
    }

    *out_x = x_coords;
    *out_y = y_coords;
    *out_flux = flux_arr;
    *out_saturated = sat_arr;
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

SDET_EXPORT void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated,
                                       float **extras, int extra_count)
{
    free(x);
    free(y);
    free(flux);
    free(saturated);
    if (extras && extra_count > 0) {
        for (int i = 0; i < extra_count; i++) {
            free(extras[i]);
        }
        free(extras);
    }
}
