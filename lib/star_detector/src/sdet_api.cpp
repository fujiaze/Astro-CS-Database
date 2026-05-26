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

int sdet_lm_solve(int m, int n, double* x, void* userdata,
                  void (*residual_func)(double*, int, void*, double*),
                  double tol, int max_iter) {
    std::vector<double> fvec(m), fvec_new(m);
    std::vector<double> J(m * n);
    std::vector<double> JtJ(n * n), Jtf(n), delta(n), x_new(n);

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

        if (!sdet_gauss_solve(n, A.data(), rhs.data(), delta.data())) {
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
            return SDET_FIT_OK;
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
                     InternalFitResult* result) {
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
                                   sdet_moffat4_residual, 1e-8, 200);

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

}

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
        defaults.fitRadius = 8;
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
    for (size_t i = 0; i < n; i++) {
        fimg[i] = static_cast<float>(image[i]);
    }
    sdet_log(SDET_LOG_INFO, "SDET", "uint16 -> float32 conversion done (%zu pixels)", n);

    handle->internal.width = width;
    handle->internal.height = height;

    std::vector<float> map(n);
    sdet_get_structure_map(&handle->internal, fimg.data(), width, height, map.data());

    float *raw_detail = handle->internal.raw_detail;
    handle->internal.raw_detail = nullptr;

    float *raw_med_filtered = nullptr;
    if (params.medianFilterDetail && raw_detail) {
        raw_med_filtered = new float[n];
        sdet_median_filter_3x3(raw_detail, raw_med_filtered, width, height);
    }

    std::vector<float> med_filtered(n);
    float *work_map = map.data();
    if (params.medianFilterDetail) {
        sdet_median_filter_3x3(map.data(), med_filtered.data(), width, height);
        work_map = med_filtered.data();
    }

    std::vector<float> maxima(n, 0.0f);
    sdet_local_maxima_map(fimg.data(), maxima.data(), width, height, 3, 1e30f);

    float map_threshold;
    const float *threshold_map = nullptr;

    {
        const float *raw_for_thresh = raw_med_filtered ? raw_med_filtered : raw_detail;
        if (raw_for_thresh) {
            float bg_med, bg_mad;
            sdet_iterative_sigma_clip(raw_for_thresh, (int)n, params.iterativeClipSigma,
                                      params.iterativeMaxRounds, &bg_med, &bg_mad);
            map_threshold = bg_med + params.iterativeClipSigma * bg_mad;
            if (map_threshold <= bg_med) map_threshold = bg_med + 1e-6f;
            threshold_map = raw_for_thresh;

            sdet_log(SDET_LOG_INFO, "SDET", "Iterative sigma-clip on raw detail: bg_med=%.6f bg_mad=%.6f threshold=%.6f",
                     bg_med, bg_mad, map_threshold);
        } else {
            float med = sdet_robust_median(work_map, (int)n);
            float mad = sdet_robust_mad(work_map, (int)n);
            map_threshold = med + params.iterativeClipSigma * mad;
            if (map_threshold <= med) map_threshold = med + 1e-6f;
            threshold_map = work_map;

            sdet_log(SDET_LOG_INFO, "SDET", "Map stats: med=%.6f mad=%.6f threshold=%.6f",
                     med, mad, map_threshold);
        }
    }

    float img_med = sdet_robust_median(fimg.data(), (int)n);
    float img_mad = sdet_robust_mad(fimg.data(), (int)n);
    float img_threshold = img_med + 5.0f * img_mad;
    if (img_threshold <= img_med) img_threshold = img_med + 1e-4f;

    struct Candidate { int x, y; float img_val; float map_val; };
    std::vector<Candidate> candidates;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (maxima[idx] > 0.0f && threshold_map[idx] >= map_threshold && fimg[idx] >= img_threshold) {
                candidates.push_back({x, y, fimg[idx], threshold_map[idx]});
            }
        }
    }

    if (params.maxStars > 0 && (int)candidates.size() > params.maxStars * 2) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &a, const Candidate &b) { return a.img_val > b.img_val; });
        candidates.resize(params.maxStars * 2);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Candidates: %d, img_t=%.6f, map_t=%.6f",
             (int)candidates.size(), img_threshold, map_threshold);

    delete[] raw_detail;
    delete[] raw_med_filtered;

    if (candidates.empty()) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        return 0;
    }

    int half_win = 8;
    int cc_count = (int)candidates.size();
    std::vector<DetectedStarInternal> stars(cc_count);

    for (int i = 0; i < cc_count; i++) {
        int cx = candidates[i].x;
        int cy = candidates[i].y;

        int x0 = std::max(0, cx - half_win);
        int y0 = std::max(0, cy - half_win);
        int x1 = std::min(width, cx + half_win + 1);
        int y1 = std::min(height, cy + half_win + 1);

        ConnectedComponent cc;
        cc.x0 = x0; cc.y0 = y0; cc.x1 = x1; cc.y1 = y1;
        cc.count = 0;

        float bkg = 0, sigma = 0;
        {
            std::vector<float> vals;
            for (int py = y0; py < y1; py++) {
                for (int px = x0; px < x1; px++) {
                    vals.push_back(fimg[py * width + px]);
                }
            }
            bkg = sdet_robust_median(vals.data(), (int)vals.size());
            sigma = sdet_robust_mad(vals.data(), (int)vals.size());
        }

        float star_threshold = bkg + 1.5f * sigma;
        for (int py = y0; py < y1; py++) {
            for (int px = x0; px < x1; px++) {
                if (fimg[py * width + px] > star_threshold) {
                    cc.px.push_back(px);
                    cc.py.push_back(py);
                    cc.count++;
                }
            }
        }

        if (cc.count > 0) {
            int min_x = x1, max_x = x0, min_y = y1, max_y = y0;
            for (int j = 0; j < cc.count; j++) {
                if (cc.px[j] < min_x) min_x = cc.px[j];
                if (cc.px[j] > max_x) max_x = cc.px[j];
                if (cc.py[j] < min_y) min_y = cc.py[j];
                if (cc.py[j] > max_y) max_y = cc.py[j];
            }
            cc.x0 = min_x; cc.y0 = min_y; cc.x1 = max_x + 1; cc.y1 = max_y + 1;
        }

        sdet_get_star_parameters(fimg.data(), width, height, &cc, &stars[i]);
    }

    int filtered_count = sdet_apply_filters(stars.data(), cc_count, width, height);

    int auto_min = sdet_compute_auto_min_structure_size(stars.data(), filtered_count);
    int final_count = sdet_deduplicate_stars(stars.data(), filtered_count, auto_min);

    std::sort(stars.data(), stars.data() + final_count, [](const DetectedStarInternal &a, const DetectedStarInternal &b) {
        return a.flux > b.flux;
    });

    if (params.maxStars > 0 && final_count > params.maxStars) {
        final_count = params.maxStars;
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Detection phase: %d stars after dedup (sorted by flux)", final_count);

    if (final_count == 0) {
        *out_x = nullptr;
        *out_y = nullptr;
        *out_count = 0;
        return 0;
    }

    auto t_fit_start = std::chrono::high_resolution_clock::now();

    std::vector<InternalFitResult> fit_results(final_count);
    int fit_radius = params.fitRadius;

    int fit_ok_count = 0;
    #pragma omp parallel for schedule(dynamic) num_threads(16) reduction(+:fit_ok_count)
    for (int i = 0; i < final_count; i++) {
        int rx0 = std::max(0, (int)stars[i].cx - fit_radius);
        int ry0 = std::max(0, (int)stars[i].cy - fit_radius);
        int rx1 = std::min(width, (int)stars[i].cx + fit_radius + 1);
        int ry1 = std::min(height, (int)stars[i].cy + fit_radius + 1);

        sdet_moffat4_fit(fimg.data(), width, height,
                         stars[i].cx, stars[i].cy,
                         rx0, ry0, rx1, ry1,
                         &fit_results[i]);
        if (fit_results[i].status == SDET_FIT_OK) {
            fit_ok_count++;
        }
    }

    auto t_fit_end = std::chrono::high_resolution_clock::now();
    sdet_log(SDET_LOG_INFO, "SDET", "Moffat4 fit: %d/%d OK (%.1f ms)",
             fit_ok_count, final_count,
             std::chrono::duration<double, std::milli>(t_fit_end - t_fit_start).count());

    std::vector<float> fwhm_values;
    fwhm_values.reserve(final_count);
    for (int i = 0; i < final_count; i++) {
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

    int f_fit = 0, f_fwhm = 0, f_round = 0;
    std::vector<int> keep(final_count, 0);
    for (int i = 0; i < final_count; i++) {
        if (fit_results[i].status != SDET_FIT_OK) {
            f_fit++;
            continue;
        }

        float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
        if (fwhm_mad_val > 0.0f) {
            float fwhm_lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
            float fwhm_hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
            if (avg_fwhm < fwhm_lo || avg_fwhm > fwhm_hi) {
                f_fwhm++;
                continue;
            }
        }

        float axis_ratio = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                                    std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
        if (axis_ratio > params.maxAxisRatio) {
            f_round++;
            continue;
        }

        keep[i] = 1;
    }

    int result_count = 0;
    for (int i = 0; i < final_count; i++) {
        if (keep[i]) result_count++;
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Post-fit filters: %d/%d passed (fit_fail=%d fwhm=%d roundness=%d)",
             result_count, final_count, f_fit, f_fwhm, f_round);

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

    int j = 0;
    for (int i = 0; i < final_count; i++) {
        if (keep[i]) {
            x_coords[j] = fit_results[i].cx;
            y_coords[j] = fit_results[i].cy;
            j++;
        }
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
                                    float **out_detail, float **out_smap, float **out_binary)
{
    if (!handle || !image || !out_x || !out_y || !out_count) return -1;

    const SDetParams &params = handle->internal.params;
    size_t n = (size_t)width * height;

    std::vector<float> fimg(n);
    for (size_t i = 0; i < n; i++) {
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
    float *smap_out = (float *)malloc(n * sizeof(float));
    float *binary_out = (float *)calloc(n, sizeof(float));

    if (raw_detail) {
        std::memcpy(detail_out, raw_detail, n * sizeof(float));
    } else {
        std::memset(detail_out, 0, n * sizeof(float));
    }
    std::memcpy(smap_out, map.data(), n * sizeof(float));

    float *raw_med_filtered = nullptr;
    if (params.medianFilterDetail && raw_detail) {
        raw_med_filtered = new float[n];
        sdet_median_filter_3x3(raw_detail, raw_med_filtered, width, height);
    }

    std::vector<float> med_filtered(n);
    float *work_map = map.data();
    if (params.medianFilterDetail) {
        sdet_median_filter_3x3(map.data(), med_filtered.data(), width, height);
        work_map = med_filtered.data();
    }

    std::vector<float> maxima(n, 0.0f);
    sdet_local_maxima_map(fimg.data(), maxima.data(), width, height, 3, 1e30f);

    float map_threshold;
    const float *threshold_map = nullptr;
    {
        const float *raw_for_thresh = raw_med_filtered ? raw_med_filtered : raw_detail;
        if (raw_for_thresh) {
            float bg_med, bg_mad;
            sdet_iterative_sigma_clip(raw_for_thresh, (int)n, params.iterativeClipSigma,
                                      params.iterativeMaxRounds, &bg_med, &bg_mad);
            map_threshold = bg_med + params.iterativeClipSigma * bg_mad;
            if (map_threshold <= bg_med) map_threshold = bg_med + 1e-6f;
            threshold_map = raw_for_thresh;
            sdet_log(SDET_LOG_INFO, "SDET", "Debug: bg_med=%.6f bg_mad=%.6f threshold=%.6f",
                     bg_med, bg_mad, map_threshold);
        } else {
            float med = sdet_robust_median(work_map, (int)n);
            float mad = sdet_robust_mad(work_map, (int)n);
            map_threshold = med + params.iterativeClipSigma * mad;
            if (map_threshold <= med) map_threshold = med + 1e-6f;
            threshold_map = work_map;
        }
    }

    float img_med = sdet_robust_median(fimg.data(), (int)n);
    float img_mad = sdet_robust_mad(fimg.data(), (int)n);
    float img_threshold = img_med + 5.0f * img_mad;
    if (img_threshold <= img_med) img_threshold = img_med + 1e-4f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (maxima[idx] > 0.0f && threshold_map[idx] >= map_threshold && fimg[idx] >= img_threshold) {
                binary_out[idx] = 1.0f;
            }
        }
    }

    *out_detail = detail_out;
    *out_smap = smap_out;
    *out_binary = binary_out;

    struct Candidate { int x, y; float img_val; float map_val; };
    std::vector<Candidate> candidates;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            if (maxima[idx] > 0.0f && threshold_map[idx] >= map_threshold && fimg[idx] >= img_threshold) {
                candidates.push_back({x, y, fimg[idx], threshold_map[idx]});
            }
        }
    }

    if (params.maxStars > 0 && (int)candidates.size() > params.maxStars * 2) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate &a, const Candidate &b) { return a.img_val > b.img_val; });
        candidates.resize(params.maxStars * 2);
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug candidates: %d, img_t=%.6f, map_t=%.6f",
             (int)candidates.size(), img_threshold, map_threshold);

    delete[] raw_detail;
    delete[] raw_med_filtered;

    if (candidates.empty()) {
        *out_x = nullptr; *out_y = nullptr; *out_count = 0;
        return 0;
    }

    int half_win = 8;
    int cc_count = (int)candidates.size();
    std::vector<DetectedStarInternal> stars(cc_count);

    for (int i = 0; i < cc_count; i++) {
        int cx = candidates[i].x;
        int cy = candidates[i].y;
        int x0 = std::max(0, cx - half_win);
        int y0 = std::max(0, cy - half_win);
        int x1 = std::min(width, cx + half_win + 1);
        int y1 = std::min(height, cy + half_win + 1);
        ConnectedComponent cc;
        cc.x0 = x0; cc.y0 = y0; cc.x1 = x1; cc.y1 = y1;
        cc.count = 0;
        float bkg = 0, sigma = 0;
        {
            std::vector<float> vals;
            for (int py = y0; py < y1; py++)
                for (int px = x0; px < x1; px++)
                    vals.push_back(fimg[py * width + px]);
            bkg = sdet_robust_median(vals.data(), (int)vals.size());
            sigma = sdet_robust_mad(vals.data(), (int)vals.size());
        }
        float star_threshold = bkg + 1.5f * sigma;
        for (int py = y0; py < y1; py++)
            for (int px = x0; px < x1; px++)
                if (fimg[py * width + px] > star_threshold) {
                    cc.px.push_back(px); cc.py.push_back(py); cc.count++;
                }
        if (cc.count > 0) {
            int min_x = x1, max_x = x0, min_y = y1, max_y = y0;
            for (int j = 0; j < cc.count; j++) {
                if (cc.px[j] < min_x) min_x = cc.px[j];
                if (cc.px[j] > max_x) max_x = cc.px[j];
                if (cc.py[j] < min_y) min_y = cc.py[j];
                if (cc.py[j] > max_y) max_y = cc.py[j];
            }
            cc.x0 = min_x; cc.y0 = min_y; cc.x1 = max_x + 1; cc.y1 = max_y + 1;
        }
        sdet_get_star_parameters(fimg.data(), width, height, &cc, &stars[i]);
    }

    int filtered_count = sdet_apply_filters(stars.data(), cc_count, width, height);
    int auto_min = sdet_compute_auto_min_structure_size(stars.data(), filtered_count);
    int final_count = sdet_deduplicate_stars(stars.data(), filtered_count, auto_min);

    std::sort(stars.data(), stars.data() + final_count, [](const DetectedStarInternal &a, const DetectedStarInternal &b) {
        return a.flux > b.flux;
    });

    if (params.maxStars > 0 && final_count > params.maxStars) {
        final_count = params.maxStars;
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug detection phase: %d stars after dedup (sorted by flux)", final_count);

    if (final_count == 0) {
        *out_x = nullptr; *out_y = nullptr; *out_count = 0;
        return 0;
    }

    std::vector<InternalFitResult> fit_results(final_count);
    int fit_radius = params.fitRadius;
    #pragma omp parallel for schedule(dynamic) num_threads(16)
    for (int i = 0; i < final_count; i++) {
        int rx0 = std::max(0, (int)stars[i].cx - fit_radius);
        int ry0 = std::max(0, (int)stars[i].cy - fit_radius);
        int rx1 = std::min(width, (int)stars[i].cx + fit_radius + 1);
        int ry1 = std::min(height, (int)stars[i].cy + fit_radius + 1);
        sdet_moffat4_fit(fimg.data(), width, height, stars[i].cx, stars[i].cy,
                         rx0, ry0, rx1, ry1, &fit_results[i]);
    }

    std::vector<float> fwhm_values;
    for (int i = 0; i < final_count; i++)
        if (fit_results[i].status == SDET_FIT_OK)
            fwhm_values.push_back((float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0));
    float fwhm_med = 0, fwhm_mad_val = 0;
    if (!fwhm_values.empty()) {
        fwhm_med = sdet_robust_median(fwhm_values.data(), (int)fwhm_values.size());
        fwhm_mad_val = sdet_robust_mad(fwhm_values.data(), (int)fwhm_values.size());
    }

    sdet_log(SDET_LOG_INFO, "SDET", "Debug FWHM: med=%.4f mad=%.4f (%d fitted)",
             fwhm_med, fwhm_mad_val, (int)fwhm_values.size());

    int result_count = 0;
    std::vector<int> keep(final_count, 0);
    for (int i = 0; i < final_count; i++) {
        if (fit_results[i].status != SDET_FIT_OK) continue;
        float avg_fwhm = (float)((fit_results[i].fwhm_x + fit_results[i].fwhm_y) / 2.0);
        if (fwhm_mad_val > 0) {
            float lo = fwhm_med - params.fwhmClipSigma * fwhm_mad_val;
            float hi = fwhm_med + params.fwhmClipSigma * fwhm_mad_val;
            if (avg_fwhm < lo || avg_fwhm > hi) continue;
        }
        float ar = (float)(std::max(fit_results[i].sx, fit_results[i].sy) /
                            std::max(std::min(fit_results[i].sx, fit_results[i].sy), 0.001));
        if (ar > params.maxAxisRatio) continue;
        keep[i] = 1;
        result_count++;
    }

    if (result_count == 0) {
        *out_x = nullptr; *out_y = nullptr; *out_count = 0;
        return 0;
    }

    double *x_coords = (double *)malloc(result_count * sizeof(double));
    double *y_coords = (double *)malloc(result_count * sizeof(double));
    int j = 0;
    for (int i = 0; i < final_count; i++) {
        if (keep[i]) {
            x_coords[j] = fit_results[i].cx;
            y_coords[j] = fit_results[i].cy;
            j++;
        }
    }
    *out_x = x_coords;
    *out_y = y_coords;
    *out_count = result_count;

    sdet_log(SDET_LOG_INFO, "SDET", "sdet_detect_debug done: %d stars", result_count);
    return 0;
}

SDET_EXPORT void sdet_free_debug_maps(float *maps)
{
    free(maps);
}
