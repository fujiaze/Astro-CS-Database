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
#include <cstdlib>
#include <cstring>
#include <omp.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// ============================================================================
// snr_estimate_f64: FP64 版本 (R10 双精度 ABI)
//
// 与 snr_estimate 逻辑完全一致 (data 参数实际未参与 SNR 计算, 仅作 nullptr/尺寸校验),
// 此处为 ABI 一致性提供 double* 入口, 便于 FP64 管线统一调用 _f64 后缀接口.
// 输出 out_snr 仍为 float32 (HISS SNR 子块格式已冻结, 见 02_FROZEN §17).
// ============================================================================
SNR_API int snr_estimate_f64(const double* data, int h, int w,
                             const double* psf, int n_stars,
                             double sigma_residual,
                             float* out_snr) {
    // ---- nullptr 检查 (返回 3) ----
    if (data == nullptr || out_snr == nullptr || psf == nullptr) {
        fprintf(stderr, "[snr_f64] error: null pointer (data=%p out_snr=%p psf=%p)\n",
                (const void*)data, (void*)out_snr, (const void*)psf);
        return 3;
    }

    int N = h * w;
    if (h <= 0 || w <= 0 || N <= 0) {
        fprintf(stderr, "[snr_f64] error: invalid image size h=%d w=%d\n", h, w);
        return 3;
    }

    // ---- 退化路径: sigma_residual <= 0 (返回 2, 全填 1.0) ----
    if (sigma_residual <= 0.0) {
        fprintf(stderr, "[snr_f64] degenerate: sigma_residual=%g <= 0, fill 1.0\n",
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
    fprintf(stderr, "[snr_f64] SNR_phot = 1/(ln(10)*sigma) = 1/(%.6f*%.6f) = %.6f\n",
            LN10, sigma_residual, snr_phot);

    // ---- 退化路径: n_stars <= 0 (返回 1, 全填 SNR_phot) ----
    if (n_stars <= 0) {
        fprintf(stderr, "[snr_f64] degenerate: n_stars=%d <= 0, fill SNR_phot=%.6f\n",
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
        double cx = row[3];
        double cy = row[4];
        double A = row[6];
        double mad = row[7];

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
    fprintf(stderr, "[snr_f64] PSF stars: total=%d valid=%d skipped=%d "
            "(status=%d A<=B=%d mad<=0=%d)\n",
            n_stars, n_valid, n_skipped, n_skip_status, n_skip_ab, n_skip_mad);

    // ---- 无有效星退化: 全填 SNR_phot (返回 1) ----
    if (n_valid <= 0) {
        fprintf(stderr, "[snr_f64] no valid PSF stars, fill SNR_phot=%.6f\n", snr_phot);
        #pragma omp parallel for num_threads(16) schedule(static)
        for (int i = 0; i < N; ++i) {
            out_snr[i] = (float)snr_phot;
        }
        return 1;
    }

    // ---- median(SNR_psf) ----
    std::vector<double> snr_copy = star_snr;
    double median_snr = medianValue(snr_copy);
    fprintf(stderr, "[snr_f64] median(SNR_psf) = %.6f (n_valid=%d)\n", median_snr, n_valid);

    if (median_snr <= 0.0) {
        fprintf(stderr, "[snr_f64] warning: median(SNR_psf)=%.6f <= 0, fallback fill SNR_phot\n",
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
    fprintf(stderr, "[snr_f64] IDW: power=2.0 radius=%.2f px (FOV diagonal), image=%dx%d\n",
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
                    sum_w = 1.0;
                    sum_v = ps[i];
                    has_neighbor = true;
                    break;
                }
                double wgt = 1.0 / dist2;
                sum_w += wgt;
                sum_v += wgt * ps[i];
                has_neighbor = true;
            }

            double snr_psf;
            if (has_neighbor) {
                snr_psf = sum_v / sum_w;
            } else {
                snr_psf = median_snr;
            }

            double snr = snr_phot * (snr_psf / median_snr);
            out_snr[y * w + x] = (float)snr;
        }
    }

    fprintf(stderr, "[snr_f64] done: SNR = SNR_phot(%.6f) * (SNR_psf/median(%.6f)), "
            "n_valid=%d, returned 0\n", snr_phot, median_snr, n_valid);
    return 0;
}

// ============================================================================
// SIP 前向多项式求值 (复用 healpix_drizzle/wcs_sip.cpp 算法)
//
// 系数按 coeffs[i*6+j] 存储, 对应 dx^i * dy^j
// 下三角: i+j <= order
// order<=0 时返回 0 (无 SIP 修正)
//
// 公式: result = Σ_{i+j<=order} coeffs[i*6+j] * dx^i * dy^j
// ============================================================================
static double snrEvalSip(const double* coeffs, double dx, double dy, int order) {
    if (order <= 0) return 0.0;
    double result = 0.0;
    // dx^i 缓存
    double px[SNR_SIP_MAX_ORDER + 1];
    double py[SNR_SIP_MAX_ORDER + 1];
    px[0] = 1.0;
    py[0] = 1.0;
    for (int k = 1; k <= order; ++k) {
        px[k] = px[k - 1] * dx;
        py[k] = py[k - 1] * dy;
    }
    for (int i = 0; i <= order; ++i) {
        for (int j = 0; j <= order - i; ++j) {
            result += coeffs[i * 6 + j] * px[i] * py[j];
        }
    }
    return result;
}

// ============================================================================
// 像素坐标 → 球面坐标 (TAN 投影 + 前向 SIP A/B)
// 复用 healpix_drizzle/wcs_sip.cpp 的 pixelToSky 算法, 保证 SNR 控制点
// 与 drizzle 阶段查询点使用同一坐标系 (P03-004 WCS+SIP 一致性)
//
// 步骤:
//   1. 归一化像素坐标: dx = x - (crpix1-1), dy = y - (crpix2-1) (CRPIX 1-based)
//   2. 前向 SIP 修正 (A/B): dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)
//   3. CD 矩阵: xi = cd[0]*dx' + cd[1]*dy', eta = cd[2]*dx' + cd[3]*dy'
//   4. TAN 反投影: (xi, eta) → (ra, dec)
// ============================================================================
static void pixelToSkySimple(double x, double y,
                              const SnrWcsParams* wcs,
                              double& ra, double& dec) {
    const double D2R = 0.017453292519943295769;
    const double R2D = 57.295779513082320877;

    // 1. 归一化像素坐标 (CRPIX 1-based, 输入 0-based)
    double dx = x - (wcs->crpix1 - 1.0);
    double dy = y - (wcs->crpix2 - 1.0);

    // 2. 前向 SIP 修正 (A/B), 若 a_order>0
    //    FITS 标准: A/B 是前向多项式, U = dx + A(dx,dy), V = dy + B(dx,dy)
    if (wcs->sip.a_order > 0 || wcs->sip.b_order > 0) {
        double f = snrEvalSip(wcs->sip.a, dx, dy, wcs->sip.a_order);
        double g = snrEvalSip(wcs->sip.b, dx, dy, wcs->sip.b_order);
        dx += f;
        dy += g;
    }

    // 3. CD 矩阵: 像素 → 中间世界坐标 (度)
    double xi  = wcs->cd[0] * dx + wcs->cd[1] * dy;
    double eta = wcs->cd[2] * dx + wcs->cd[3] * dy;

    // 4. TAN 反投影: 中间坐标 → 天球
    const double ra0_deg  = wcs->crval1;
    const double dec0_deg = wcs->crval2;

    const double xi_rad  = xi  * D2R;
    const double eta_rad = eta * D2R;
    const double rho = std::sqrt(xi_rad * xi_rad + eta_rad * eta_rad);

    if (rho < 1e-12) {
        ra  = ra0_deg;
        dec = dec0_deg;
        return;
    }

    const double dec0_rad = dec0_deg * D2R;
    const double sdec0 = std::sin(dec0_rad);
    const double cdec0 = std::cos(dec0_rad);

    const double c    = std::atan(rho);
    const double sinc = std::sin(c);
    const double cosc = std::cos(c);

    // dec = asin(cos(c)*sin(dec0) + eta*sin(c)*cos(dec0)/rho)
    double sin_dec = cosc * sdec0 + eta_rad * sinc * cdec0 / rho;
    if (sin_dec >  1.0) sin_dec =  1.0;
    if (sin_dec < -1.0) sin_dec = -1.0;
    const double dec_rad = std::asin(sin_dec);

    // ra = ra0 + atan2(xi*sin(c), rho*cos(dec0)*cos(c) - eta*sin(dec0)*sin(c))
    const double dra = std::atan2(xi_rad * sinc,
                                  rho * cdec0 * cosc - eta_rad * sdec0 * sinc);
    double ra_rad = ra0_deg * D2R + dra;

    // 归一化 RA 到 [0, 360)
    while (ra_rad < 0.0)         ra_rad += 2.0 * M_PI;
    while (ra_rad >= 2.0 * M_PI) ra_rad -= 2.0 * M_PI;

    ra  = ra_rad * R2D;
    dec = dec_rad * R2D;
}

// ============================================================================
// snr_extract_model - 从 PSF 块提取稀疏 SNR 控制点模型
// ============================================================================
SNR_API int snr_extract_model(const double* psf, int n_stars,
                               double sigma_residual,
                               const SnrWcsParams* wcs,
                               SnrModel* out_model) {
    // nullptr 检查
    if (!psf || !wcs || !out_model) {
        fprintf(stderr, "[snr_model] error: null pointer\n");
        return 3;
    }

    // 初始化输出
    out_model->n_points = 0;
    out_model->points = nullptr;
    out_model->snr_phot = 0.0;
    out_model->median_snr = 0.0;
    out_model->idw_power = 2.0;

    // 退化: sigma_residual <= 0
    if (sigma_residual <= 0.0) {
        fprintf(stderr, "[snr_model] degenerate: sigma_residual=%g <= 0\n", sigma_residual);
        return 2;
    }

    // SNR_phot 全局标量
    const double LN10 = 2.302585092994045684;
    double snr_phot = 1.0 / (LN10 * sigma_residual);
    out_model->snr_phot = snr_phot;
    fprintf(stderr, "[snr_model] SNR_phot = %.6f\n", snr_phot);

    // 退化: n_stars <= 0
    if (n_stars <= 0) {
        fprintf(stderr, "[snr_model] degenerate: n_stars=%d <= 0\n", n_stars);
        return 1;
    }

    // 收集有效 PSF 星: status==0, A>B, mad>0
    std::vector<double> star_x, star_y, star_snr;
    star_x.reserve(n_stars);
    star_y.reserve(n_stars);
    star_snr.reserve(n_stars);
    int n_skip_status = 0, n_skip_ab = 0, n_skip_mad = 0;

    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        double status = row[0];
        double B = row[1];
        double cx = row[3];
        double cy = row[4];
        double A = row[6];
        double mad = row[7];

        if (status != 0.0) { ++n_skip_status; continue; }
        if (A <= B) { ++n_skip_ab; continue; }
        if (mad <= 0.0) { ++n_skip_mad; continue; }

        double s = (A - B) / mad;
        star_x.push_back(cx);
        star_y.push_back(cy);
        star_snr.push_back(s);
    }

    int n_valid = (int)star_x.size();
    fprintf(stderr, "[snr_model] PSF stars: total=%d valid=%d skipped=%d "
            "(status=%d A<=B=%d mad<=0=%d)\n",
            n_stars, n_valid, n_skip_status + n_skip_ab + n_skip_mad,
            n_skip_status, n_skip_ab, n_skip_mad);

    if (n_valid <= 0) {
        fprintf(stderr, "[snr_model] no valid PSF stars\n");
        return 1;
    }

    // median(SNR_psf)
    std::vector<double> snr_copy = star_snr;
    double median_snr = medianValue(snr_copy);
    out_model->median_snr = median_snr;
    fprintf(stderr, "[snr_model] median(SNR_psf) = %.6f\n", median_snr);

    if (median_snr <= 0.0) {
        fprintf(stderr, "[snr_model] warning: median_snr <= 0\n");
        return 1;
    }

    // WCS 像素→球面转换, 构造控制点
    // P03-004: 使用完整 WCS+SIP (前向 A/B), 与 drizzle 阶段坐标系一致
    out_model->points = new SnrControlPoint[n_valid];
    out_model->n_points = (uint32_t)n_valid;

    int sip_a_order = wcs->sip.a_order;
    int sip_b_order = wcs->sip.b_order;
    fprintf(stderr, "[snr_model] WCS+SIP: a_order=%d b_order=%d "
                    "(前向 SIP %s, 与 drizzle WcsSip.pixelToSky 一致)\n",
            sip_a_order, sip_b_order,
            (sip_a_order > 0 || sip_b_order > 0) ? "启用" : "禁用 (仅 CD+TAN)");

    for (int i = 0; i < n_valid; ++i) {
        double ra, dec;
        pixelToSkySimple(star_x[i], star_y[i], wcs, ra, dec);
        out_model->points[i].ra = ra;
        out_model->points[i].dec = dec;
        out_model->points[i].snr_psf = (float)star_snr[i];
    }

    // P03-004: 输出前 3 个控制点坐标用于调试验证
    int show_n = (n_valid < 3) ? n_valid : 3;
    for (int i = 0; i < show_n; ++i) {
        fprintf(stderr, "[snr_model] ctrl_point[%d]: ra=%.6f dec=%.6f "
                        "snr_psf=%.4f (src px x=%.2f y=%.2f)\n",
                i, out_model->points[i].ra, out_model->points[i].dec,
                out_model->points[i].snr_psf, star_x[i], star_y[i]);
    }

    fprintf(stderr, "[snr_model] done: n_points=%u, snr_phot=%.6f, median=%.6f, power=2.0\n",
            out_model->n_points, out_model->snr_phot, out_model->median_snr);
    return 0;
}

// ============================================================================
// snr_free_model - 释放 SnrModel 内部资源
// ============================================================================
SNR_API void snr_free_model(SnrModel* model) {
    if (!model) return;
    if (model->points) {
        delete[] model->points;
        model->points = nullptr;
    }
    model->n_points = 0;
}

// ============================================================================
// snr_extract_model_v2 - 版本化 SNR 模型 (value_dtype=0 f32 / 1 f64)
// ============================================================================
SNR_API int snr_extract_model_v2(const double* psf, int n_stars,
                                  double sigma_residual,
                                  const SnrWcsParams* wcs,
                                  int value_dtype,
                                  SnrModelV2* out_model) {
    if (!psf || !wcs || !out_model) {
        fprintf(stderr, "[snr_model_v2] error: null pointer\n");
        return 3;
    }
    if (value_dtype != 0 && value_dtype != 1) {
        fprintf(stderr, "[snr_model_v2] error: value_dtype=%d (0=f32, 1=f64)\n", value_dtype);
        return 3;
    }
    std::memset(out_model, 0, sizeof(SnrModelV2));
    out_model->value_dtype = (uint8_t)value_dtype;
    out_model->idw_power = 2.0;

    if (sigma_residual <= 0.0) {
        fprintf(stderr, "[snr_model_v2] degenerate: sigma_residual=%g <= 0\n", sigma_residual);
        return 2;
    }
    const double LN10 = 2.302585092994045684;
    double snr_phot = 1.0 / (LN10 * sigma_residual);
    out_model->snr_phot = snr_phot;

    if (n_stars <= 0) {
        fprintf(stderr, "[snr_model_v2] degenerate: n_stars=%d <= 0\n", n_stars);
        return 1;
    }

    // 收集有效 PSF 星 (snr_psf 全程 double 计算)
    std::vector<double> star_x, star_y, star_snr;
    star_x.reserve(n_stars); star_y.reserve(n_stars); star_snr.reserve(n_stars);
    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        double status = row[0], B = row[1], cx = row[3], cy = row[4];
        double A = row[6], mad = row[7];
        if (status != 0.0) continue;
        if (A <= B) continue;
        if (mad <= 0.0) continue;
        star_x.push_back(cx); star_y.push_back(cy);
        star_snr.push_back((A - B) / mad);
    }
    int n_valid = (int)star_x.size();
    if (n_valid <= 0) {
        fprintf(stderr, "[snr_model_v2] no valid PSF stars\n");
        return 1;
    }
    std::vector<double> snr_copy = star_snr;
    double median_snr = medianValue(snr_copy);
    out_model->median_snr = median_snr;
    if (median_snr <= 0.0) {
        fprintf(stderr, "[snr_model_v2] warning: median_snr <= 0\n");
        return 1;
    }

    out_model->n_points = (uint32_t)n_valid;
    if (value_dtype == 1) {
        auto* pts = (SnrControlPointF64*)std::malloc(
            (size_t)n_valid * sizeof(SnrControlPointF64));
        if (!pts) return 3;
        for (int i = 0; i < n_valid; ++i) {
            double ra, dec;
            pixelToSkySimple(star_x[i], star_y[i], wcs, ra, dec);
            pts[i].ra = ra; pts[i].dec = dec; pts[i].snr_psf = star_snr[i];
        }
        out_model->points = pts;
    } else {
        auto* pts = (SnrControlPoint*)std::malloc(
            (size_t)n_valid * sizeof(SnrControlPoint));
        if (!pts) return 3;
        for (int i = 0; i < n_valid; ++i) {
            double ra, dec;
            pixelToSkySimple(star_x[i], star_y[i], wcs, ra, dec);
            pts[i].ra = ra; pts[i].dec = dec;
            pts[i].snr_psf = (float)star_snr[i];
        }
        out_model->points = pts;
    }
    fprintf(stderr, "[snr_model_v2] done: n_points=%u dtype=%d snr_phot=%.6f median=%.6f\n",
            out_model->n_points, value_dtype, out_model->snr_phot, out_model->median_snr);
    return 0;
}

SNR_API void snr_free_model_v2(SnrModelV2* model) {
    if (!model) return;
    std::free(model->points);
    model->points = nullptr;
    model->n_points = 0;
}

// ============================================================================
// snr_extract_model_v3 - 版本化 SNR 模型, 携带 star_id/quality_flags/status
// ============================================================================
SNR_API int snr_extract_model_v3(const double* psf, int n_stars,
                                  double sigma_residual,
                                  const SnrWcsParams* wcs,
                                  int value_dtype,
                                  const int64_t* star_ids,
                                  const uint32_t* quality_flags,
                                  const uint32_t* photometric_status,
                                  SnrModelV3* out_model) {
    if (!psf || !wcs || !out_model) {
        fprintf(stderr, "[snr_model_v3] error: null pointer\n");
        return 3;
    }
    if (value_dtype != 0 && value_dtype != 1) {
        fprintf(stderr, "[snr_model_v3] error: value_dtype=%d (0=f32, 1=f64)\n", value_dtype);
        return 3;
    }
    std::memset(out_model, 0, sizeof(SnrModelV3));
    out_model->value_dtype = (uint8_t)value_dtype;
    out_model->idw_power = 2.0;

    if (sigma_residual <= 0.0) {
        fprintf(stderr, "[snr_model_v3] degenerate: sigma_residual=%g <= 0\n", sigma_residual);
        return 2;
    }
    const double LN10 = 2.302585092994045684;
    double snr_phot = 1.0 / (LN10 * sigma_residual);
    out_model->snr_phot = snr_phot;

    if (n_stars <= 0) {
        fprintf(stderr, "[snr_model_v3] degenerate: n_stars=%d <= 0\n", n_stars);
        return 1;
    }

    // 收集有效 PSF 星 (snr_psf 全程 double 计算; ID/状态按原行对齐)
    struct ValidRow {
        double x, y, snr;
        int64_t star_id;
        uint32_t qf;
        uint32_t ps;
    };
    std::vector<ValidRow> valid;
    valid.reserve((size_t)n_stars);
    for (int i = 0; i < n_stars; ++i) {
        const double* row = psf + i * 9;
        double status = row[0], B = row[1], cx = row[3], cy = row[4];
        double A = row[6], mad = row[7];
        if (status != 0.0) continue;
        if (A <= B) continue;
        if (mad <= 0.0) continue;
        ValidRow vr;
        vr.x = cx; vr.y = cy;
        vr.snr = (A - B) / mad;
        vr.star_id = star_ids ? star_ids[i] : 0;
        vr.qf = quality_flags ? quality_flags[i] : 0u;
        vr.ps = photometric_status ? photometric_status[i] : 0u;
        valid.push_back(vr);
    }
    int n_valid = (int)valid.size();
    if (n_valid <= 0) {
        fprintf(stderr, "[snr_model_v3] no valid PSF stars\n");
        return 1;
    }
    std::vector<double> snr_copy;
    snr_copy.reserve((size_t)n_valid);
    for (const auto& v : valid) snr_copy.push_back(v.snr);
    double median_snr = medianValue(snr_copy);
    out_model->median_snr = median_snr;
    if (median_snr <= 0.0) {
        fprintf(stderr, "[snr_model_v3] warning: median_snr <= 0\n");
        return 1;
    }

    out_model->n_points = (uint32_t)n_valid;
    if (value_dtype == 1) {
        auto* pts = (SnrControlPointF64V3*)std::malloc(
            (size_t)n_valid * sizeof(SnrControlPointF64V3));
        if (!pts) return 3;
        for (int i = 0; i < n_valid; ++i) {
            double ra, dec;
            pixelToSkySimple(valid[(size_t)i].x, valid[(size_t)i].y, wcs, ra, dec);
            pts[i].ra = ra; pts[i].dec = dec; pts[i].snr_psf = valid[(size_t)i].snr;
            pts[i].star_id = valid[(size_t)i].star_id;
            pts[i].quality_flags = valid[(size_t)i].qf;
            pts[i].photometric_status = valid[(size_t)i].ps;
        }
        out_model->points = pts;
    } else {
        auto* pts = (SnrControlPointV3*)std::malloc(
            (size_t)n_valid * sizeof(SnrControlPointV3));
        if (!pts) return 3;
        for (int i = 0; i < n_valid; ++i) {
            double ra, dec;
            pixelToSkySimple(valid[(size_t)i].x, valid[(size_t)i].y, wcs, ra, dec);
            pts[i].ra = ra; pts[i].dec = dec; pts[i].snr_psf = (float)valid[(size_t)i].snr;
            pts[i].star_id = valid[(size_t)i].star_id;
            pts[i].quality_flags = valid[(size_t)i].qf;
            pts[i].photometric_status = valid[(size_t)i].ps;
        }
        out_model->points = pts;
    }
    fprintf(stderr, "[snr_model_v3] done: n_points=%u dtype=%d snr_phot=%.6f median=%.6f\n",
            out_model->n_points, value_dtype, out_model->snr_phot, out_model->median_snr);
    return 0;
}

SNR_API void snr_free_model_v3(SnrModelV3* model) {
    if (!model) return;
    std::free(model->points);
    model->points = nullptr;
    model->n_points = 0;
}

}  // extern "C"
