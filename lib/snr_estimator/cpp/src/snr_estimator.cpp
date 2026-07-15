// snr_estimator.cpp - SNR 估算模块实现
// 乘法模型: SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))
//
// SNR_phot = 1.0 / (ln(10) × sigma_residual)  全帧常数
// SNR_psf(pixel) = IDW(PSF星位置, (A-B)/mad)  反距离加权插值
//   - A: psf[i*9+6] (振幅), B: psf[i*9+1] (局部背景), mad: psf[i*9+7] (残差MAD)
//   - IDW power=2.0, 搜索半径=FOV对角线像素
//   - 跳过 status!=0 或 A<=B 或 mad<=0 的星

#include "snr_estimator.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <omp.h>

namespace {

// 计算中位数 (会修改输入 vector 的顺序)
double medianValue(std::vector<double>& v) {
    if (v.empty()) return 0.0;
    size_t n = v.size();
    if (n == 1) return v[0];
    std::nth_element(v.begin(), v.begin() + n / 2, v.end());
    double med = v[n / 2];
    if (n % 2 == 0) {
        std::nth_element(v.begin(), v.begin() + n / 2 - 1, v.end());
        med = (v[n / 2 - 1] + med) * 0.5;
    }
    return med;
}

}  // namespace

extern "C" {

SNR_API int snr_estimate(const float* data, int h, int w,
                         const double* psf, int n_stars,
                         double sigma_residual,
                         float* out_snr) {
    // ---- nullptr 检查 (返回 3) ----
    if (data == nullptr || out_snr == nullptr || psf == nullptr) {
        fprintf(stderr, "[snr] error: null pointer (data=%p out_snr=%p psf=%p)\n",
                (const void*)data, (void*)out_snr, (const void*)psf);
        return 3;
    }

    int N = h * w;
    if (h <= 0 || w <= 0 || N <= 0) {
        fprintf(stderr, "[snr] error: invalid image size h=%d w=%d\n", h, w);
        return 3;
    }

    // ---- 退化路径: sigma_residual <= 0 (返回 2, 全填 1.0) ----
    if (sigma_residual <= 0.0) {
        fprintf(stderr, "[snr] degenerate: sigma_residual=%g <= 0, fill 1.0\n",
                sigma_residual);
        #pragma omp parallel for num_threads(16) schedule(static)
        for (int i = 0; i < N; ++i) {
            out_snr[i] = 1.0f;
        }
        return 2;
    }

    // ---- SNR_phot 全帧常数 ----
    const double LN10 = 2.302585092994045684017991454684;
    double snr_phot = 1.0 / (LN10 * sigma_residual);
    fprintf(stderr, "[snr] SNR_phot = 1/(ln(10)*sigma) = 1/(%.6f*%.6f) = %.6f\n",
            LN10, sigma_residual, snr_phot);

    // ---- 退化路径: n_stars <= 0 (返回 1, 全填 SNR_phot) ----
    if (n_stars <= 0) {
        fprintf(stderr, "[snr] degenerate: n_stars=%d <= 0, fill SNR_phot=%.6f\n",
                n_stars, snr_phot);
        #pragma omp parallel for num_threads(16) schedule(static)
        for (int i = 0; i < N; ++i) {
            out_snr[i] = (float)snr_phot;
        }
        return 1;
    }

    // ---- 收集有效 PSF 星 ----
    // 跳过 status!=0 或 A<=B 或 mad<=0
    std::vector<double> star_x, star_y, star_snr;
    star_x.reserve(n_stars);
    star_y.reserve(n_stars);
    star_snr.reserve(n_stars);
    int n_skip_status = 0;
    int n_skip_ab = 0;
    int n_skip_mad = 0;

    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        double status = row[0];
        double B = row[1];
        // double flux = row[2];   // 未使用
        double cx = row[3];
        double cy = row[4];
        // double fwhm = row[5];   // 未使用
        double A = row[6];
        double mad = row[7];
        // double ecc = row[8];    // 未使用

        if (status != 0.0) { ++n_skip_status; continue; }
        if (A <= B) { ++n_skip_ab; continue; }
        if (mad <= 0.0) { ++n_skip_mad; continue; }

        double s = (A - B) / mad;
        star_x.push_back(cx);
        star_y.push_back(cy);
        star_snr.push_back(s);
    }

    int n_valid = (int)star_x.size();
    int n_skipped = n_skip_status + n_skip_ab + n_skip_mad;
    fprintf(stderr, "[snr] PSF stars: total=%d valid=%d skipped=%d "
            "(status=%d A<=B=%d mad<=0=%d)\n",
            n_stars, n_valid, n_skipped, n_skip_status, n_skip_ab, n_skip_mad);

    // ---- 无有效星退化: 全填 SNR_phot (返回 1) ----
    if (n_valid <= 0) {
        fprintf(stderr, "[snr] no valid PSF stars, fill SNR_phot=%.6f\n", snr_phot);
        #pragma omp parallel for num_threads(16) schedule(static)
        for (int i = 0; i < N; ++i) {
            out_snr[i] = (float)snr_phot;
        }
        return 1;
    }

    // ---- median(SNR_psf) ----
    std::vector<double> snr_copy = star_snr;  // 拷贝, medianValue 会修改顺序
    double median_snr = medianValue(snr_copy);
    fprintf(stderr, "[snr] median(SNR_psf) = %.6f (n_valid=%d)\n", median_snr, n_valid);

    if (median_snr <= 0.0) {
        fprintf(stderr, "[snr] warning: median(SNR_psf)=%.6f <= 0, fallback fill SNR_phot\n",
                median_snr);
        #pragma omp parallel for num_threads(16) schedule(static)
        for (int i = 0; i < N; ++i) {
            out_snr[i] = (float)snr_phot;
        }
        return 1;
    }

    // ---- IDW 搜索半径 = FOV 对角线像素 ----
    double radius = std::sqrt((double)w * (double)w + (double)h * (double)h);
    double radius2 = radius * radius;
    fprintf(stderr, "[snr] IDW: power=2.0 radius=%.2f px (FOV diagonal), image=%dx%d\n",
            radius, w, h);

    // ---- 并行 IDW 插值 ----
    const double EPS2 = 1e-6;
    const double* px = star_x.data();
    const double* py = star_y.data();
    const double* ps = star_snr.data();

    #pragma omp parallel for num_threads(16) schedule(static)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double sum_w = 0.0;
            double sum_v = 0.0;
            bool has_neighbor = false;

            for (int i = 0; i < n_valid; ++i) {
                double dx = (double)x - px[i];
                double dy = (double)y - py[i];
                double dist2 = dx * dx + dy * dy;
                if (dist2 > radius2) continue;
                if (dist2 < EPS2) {
                    // 正好在星位置
                    sum_w = 1.0;
                    sum_v = ps[i];
                    has_neighbor = true;
                    break;
                }
                double wgt = 1.0 / dist2;  // power=2
                sum_w += wgt;
                sum_v += wgt * ps[i];
                has_neighbor = true;
            }

            double snr_psf;
            if (has_neighbor) {
                snr_psf = sum_v / sum_w;
            } else {
                snr_psf = median_snr;  // 边界兜底
            }

            double snr = snr_phot * (snr_psf / median_snr);
            out_snr[y * w + x] = (float)snr;
        }
    }

    fprintf(stderr, "[snr] done: SNR = SNR_phot(%.6f) * (SNR_psf/median(%.6f)), "
            "n_valid=%d, returned 0\n", snr_phot, median_snr, n_valid);
    return 0;
}

}  // extern "C"
