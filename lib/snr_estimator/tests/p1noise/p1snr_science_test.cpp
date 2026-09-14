// p1snr_science_test.cpp - P5-SNR 逐源 SNR 回归锁 (TEST-P5-SNR)
//
// 被测面: lib/snr_estimator/cpp/src/snr_science.cpp (Horne 1986 / CCD 方程 /
//         5-sigma 深度 / 零点标准误) 与 lib/snr_estimator/cpp/src/snr_estimator.cpp
//         的生产控制点路径 (snr_extract_model_v3)。
// Oracle: 本文件内置**独立**参考实现 (长双精度、逐像素暴力累加、不同循环结构),
//         并在注释中锚定 NumPy oracle (run/perf-fix/P5-snr/harness/snr_oracle.py)
//         给出的数值; 二者不共享代码路径。
//
// 依据: run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md §C.3.1。
// 用法: p1snr_science_test [units|oracle|negative|production|determinism|all]
#include "snr_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_total = 0;

void check(bool ok, const char* what) {
    ++g_total;
    if (!ok) { ++g_fail; std::printf("FAIL %s\n", what); }
}

void checkClose(double got, double exp, double rtol, const char* what) {
    ++g_total;
    const double a = std::fabs(got - exp);
    const double tol = 1e-12 + rtol * std::fabs(exp);
    if (!(a <= tol)) {
        ++g_fail;
        std::printf("FAIL %s: got=%.17g exp=%.17g abs=%.3g tol=%.3g\n", what, got, exp, a, tol);
    }
}

constexpr double kPi = 3.14159265358979323846;
constexpr double kFwhmFactor = 1.230310;
constexpr double kTrimToSigma = 0.7316727929211932;

int refHalf(double fwhm) {
    int h = (int)std::ceil(12.0 * fwhm);
    if (h < 30) h = 30;
    if (h > 256) h = 256;
    return h;
}

// 独立参考实现 (暴力逐像素, long double)
struct RefResult {
    double snr_optimal, sigma_f_optimal, snr_aperture, sigma_f_aperture;
    double enclosed_fraction, aperture_correction, n_pix, sum_p2, snr_peak;
    double flux5, m5;
};

RefResult refCompute(double flux, double fwhm, double sky, double gain,
                     double rn, double r_ap, double n_sky, double zp) {
    RefResult o{};
    const long double sigma = (long double)fwhm / (long double)kFwhmFactor;
    const int half = refHalf(fwhm);
    const long double alpha2 = 2.0L * sigma * sigma;
    long double sum = 0.0L, sum2 = 0.0L, center = 0.0L;
    for (int j = -half; j <= half; ++j) {
        for (int i = -half; i <= half; ++i) {
            const long double r2 = (long double)i * i + (long double)j * j;
            const long double t = 1.0L + r2 / alpha2;
            const long double v = 1.0L / (t * t * t * t);
            sum += v; sum2 += v * v;
            if (i == 0 && j == 0) center = v;
        }
    }
    const long double Pc = center / sum;
    long double var_f = 0.0L;
    long double sum_p2 = 0.0L;
    if (gain > 0.0) {
        long double rn2 = (rn > 0.0) ? (long double)((rn / gain) * (rn / gain)) : 0.0L;
        for (int j = -half; j <= half; ++j) {
            for (int i = -half; i <= half; ++i) {
                const long double r2 = (long double)i * i + (long double)j * j;
                const long double t = 1.0L + r2 / alpha2;
                const long double v = 1.0L / (t * t * t * t);
                const long double P = v / sum;
                const long double si = (long double)flux * P;
                long double var_i = (long double)(sky * sky) + rn2;
                if (si > 0.0L) var_i += si / (long double)gain;
                var_f += (P * P) / var_i;
                sum_p2 += P * P;
            }
        }
        var_f = 1.0L / var_f;
    } else {
        var_f = (long double)(sky * sky) * (sum * sum) / sum2;
        sum_p2 = sum2 / (sum * sum);
    }
    o.sigma_f_optimal = (double)std::sqrt(var_f);
    o.snr_optimal = flux / o.sigma_f_optimal;
    o.sum_p2 = (double)sum_p2;
    o.snr_peak = flux * (double)Pc / sky;
    o.flux5 = 5.0 * o.sigma_f_optimal;

    const double r = (r_ap > 0.0) ? r_ap : 1.5 * fwhm;
    const long double u = 1.0L + (long double)(r * r) / alpha2;
    const long double f_in = 1.0L - 1.0L / (u * u * u);
    const long double n_pix = (long double)kPi * r * r;
    const long double nsky = (n_sky > 0.0) ? (long double)n_sky : n_pix;
    const long double s_ap = (long double)flux * f_in;
    long double var_ap = n_pix * (long double)(sky * sky) * (1.0L + n_pix / nsky);
    if (gain > 0.0 && s_ap > 0.0L) var_ap += s_ap / (long double)gain;
    o.enclosed_fraction = (double)f_in;
    o.aperture_correction = 1.0 / (double)f_in;
    o.n_pix = (double)n_pix;
    o.snr_aperture = (double)(s_ap / std::sqrt(var_ap));
    o.sigma_f_aperture = (double)(std::sqrt(var_ap) / f_in);
    o.m5 = (zp != 0.0 && o.flux5 > 0.0) ? (zp - 2.5 * std::log10(o.flux5)) : std::nan("");
    return o;
}

void compareAll(const char* tag, const SnrSourceResult& r, const RefResult& e, double rtol) {
    char b[128];
    std::snprintf(b, sizeof(b), "%s.snr_optimal", tag);        checkClose(r.snr_optimal, e.snr_optimal, rtol, b);
    std::snprintf(b, sizeof(b), "%s.sigma_f_optimal_adu", tag);checkClose(r.sigma_f_optimal_adu, e.sigma_f_optimal, rtol, b);
    std::snprintf(b, sizeof(b), "%s.snr_aperture", tag);       checkClose(r.snr_aperture, e.snr_aperture, rtol, b);
    std::snprintf(b, sizeof(b), "%s.sigma_f_aperture_adu", tag); checkClose(r.sigma_f_aperture_adu, e.sigma_f_aperture, rtol, b);
    std::snprintf(b, sizeof(b), "%s.enclosed_fraction", tag);  checkClose(r.enclosed_fraction, e.enclosed_fraction, rtol, b);
    std::snprintf(b, sizeof(b), "%s.aperture_correction", tag);checkClose(r.aperture_correction, e.aperture_correction, rtol, b);
    std::snprintf(b, sizeof(b), "%s.n_pix", tag);              checkClose(r.n_pix, e.n_pix, rtol, b);
    std::snprintf(b, sizeof(b), "%s.sum_p2", tag);             checkClose(r.sum_p2, e.sum_p2, rtol, b);
    std::snprintf(b, sizeof(b), "%s.snr_peak", tag);           checkClose(r.snr_peak, e.snr_peak, rtol, b);
    std::snprintf(b, sizeof(b), "%s.flux5_adu", tag);          checkClose(r.flux5_adu, e.flux5, rtol, b);
    std::snprintf(b, sizeof(b), "%s.m5_mag", tag);             checkClose(r.m5_mag, e.m5, rtol, b);
}

SnrSourceParams makeParams(double flux, double fwhm, double sky,
                           double gain = 0.0, double rn = 0.0,
                           double r_ap = 0.0, double n_sky = 0.0, double zp = 0.0) {
    SnrSourceParams p;
    std::memset(&p, 0, sizeof(p));
    p.flux_adu = flux; p.fwhm_px = fwhm; p.sigma_sky_adu = sky;
    p.gain_e_per_adu = gain; p.read_noise_e = rn;
    p.aperture_radius_px = r_ap; p.n_sky = n_sky; p.zero_point_mag = zp;
    return p;
}

// ---- 5 真实源 (P2 t4_a p1_sources.json, R 波段, 2026-09-14 只读取值) ----
const double kRealSky = 260.8590110604006;
const double kRealFlux[5] = {2984.19140625, 11900.705322265625, 11581.213134765625,
                             30877.013671875, 11209.939697265625};
const double kRealFwhm[5] = {1.4072320071088888, 2.9622233810299274, 3.0339454080028325,
                             3.4166259417890283, 3.001042479230288};

// NumPy oracle 锚值 (snr_oracle.py, 独立实现)
const double kOracleSnrOptReal0 = 4.7863380411985785;
const double kOracleSnrOptCcd   = 109.32793489787282;
const double kOracleSnrOptSky   = 3.7103826382393557;
const double kOracleSumP2Fwhm25 = 0.049598592587386636;
const double kOracleZpSE        = 0.00443002398413372;

}  // namespace

int main(int argc, char** argv) {
    const std::string grp = (argc > 1) ? argv[1] : "all";
    const bool all = (grp == "all");

    if (all || grp == "units") {
        // Moffat4 离散轮廓: 生产 vs 独立参考
        const double fw[3] = {1.5, 2.5, 4.0};
        const double expP2[3] = {0.14911446001389192, kOracleSumP2Fwhm25, 0.019358611004407937};
        for (int i = 0; i < 3; ++i) {
            double sp2 = 0, pc = 0;
            check(snr_moffat4_profile_f64(fw[i], 0.0, 0, &sp2, &pc) == 0, "profile rc");
            RefResult e = refCompute(1.0, fw[i], 1.0, 0, 0, 0, 0, 0);
            checkClose(sp2, e.sum_p2, 1e-12, "profile sum_p2 vs ref");
            checkClose(sp2, expP2[i], 1e-12, "profile sum_p2 vs numpy oracle");
        }
        // 零点标准误 1.253*sigma/sqrt(N)
        checkClose(snr_calib_zero_point_standard_error(0.05, 200), kOracleZpSE, 1e-12, "zp_se n200");
        check(snr_calib_zero_point_standard_error(0.05, 0) == 0.0, "zp_se n0");
        check(snr_calib_zero_point_standard_error(0.0, 100) == 0.0, "zp_se sigma0");
        // 零点 SE 必须比逐星散度小 sqrt(N) 倍量级 (要求 3: 旧实现未除 sqrtN)
        const double scatter_rel = std::log(10.0) * 0.05;  // ln10*sigma -> 相对通量散度
        check(snr_calib_zero_point_standard_error(0.05, 200) < scatter_rel / 10.0,
              "zp_se << per-star scatter (sqrt(N) correction)");
    }

    if (all || grp == "oracle") {
        // 2 合成场景 + 5 真实源: 生产 API vs 独立参考 vs NumPy 锚
        SnrSourceResult r; SnrSourceParams p;

        p = makeParams(1.0e4, 2.5, 10.0, 2.0, 5.0, 1.5 * 2.5, 0.0, 25.0);
        check(snr_source_snr_f64(&p, &r) == 0 && r.status == 0, "syn ccd rc");
        compareAll("syn.ccd_gain", r, refCompute(1.0e4, 2.5, 10.0, 2.0, 5.0, 1.5 * 2.5, 0.0, 25.0), 1e-12);
        checkClose(r.snr_optimal, kOracleSnrOptCcd, 1e-12, "syn ccd vs numpy oracle");

        p = makeParams(1.0e3, 3.0, 50.0, 0.0, 0.0, 3.0 * 3.0, 0.0, 22.0);
        check(snr_source_snr_f64(&p, &r) == 0 && r.status == 0, "syn sky rc");
        compareAll("syn.sky_limited", r, refCompute(1.0e3, 3.0, 50.0, 0.0, 0.0, 3.0 * 3.0, 0.0, 22.0), 1e-12);
        checkClose(r.snr_optimal, kOracleSnrOptSky, 1e-12, "syn sky vs numpy oracle");

        for (int i = 0; i < 5; ++i) {
            char tag[32];
            std::snprintf(tag, sizeof(tag), "real.s%d", i);
            p = makeParams(kRealFlux[i], kRealFwhm[i], kRealSky, 0, 0, 0, 0, 20.0);
            check(snr_source_snr_f64(&p, &r) == 0 && r.status == 0, "real rc");
            compareAll(tag, r, refCompute(kRealFlux[i], kRealFwhm[i], kRealSky, 0, 0, 0, 0, 20.0), 1e-12);
        }
        p = makeParams(kRealFlux[0], kRealFwhm[0], kRealSky, 0, 0, 0, 0, 20.0);
        snr_source_snr_f64(&p, &r);
        checkClose(r.snr_optimal, kOracleSnrOptReal0, 1e-12, "real.s0 vs numpy oracle");

        // 5-sigma 深度公式
        double f5 = 0, m5 = 0;
        check(snr_frame_depth_f64(&r, 20.0, &f5, &m5) == 0, "frame_depth rc");
        checkClose(f5, 5.0 * r.sigma_f_optimal_adu, 1e-15, "frame_depth F5");
        checkClose(m5, 20.0 - 2.5 * std::log10(f5), 1e-15, "frame_depth m5");
        double m5b = 0;
        snr_frame_depth_f64(&r, 0.0, nullptr, &m5b);
        check(std::isnan(m5b), "frame_depth m5 NaN without ZP");
    }

    if (all || grp == "negative") {
        SnrSourceResult r;
        SnrSourceParams p;
        check(snr_source_snr_f64(nullptr, &r) == 3, "null params rc=3");
        check(snr_source_snr_f64(&p, nullptr) == 3, "null out rc=3");
        p = makeParams(0.0, 2.5, 10.0);   check(snr_source_snr_f64(&p, &r) == 0 && r.status == 1, "flux<=0 degenerate");
        p = makeParams(1e4, 0.0, 10.0);   check(snr_source_snr_f64(&p, &r) == 0 && r.status == 1, "fwhm/sigma<=0 degenerate");
        p = makeParams(1e4, 2.5, 0.0);    check(snr_source_snr_f64(&p, &r) == 0 && r.status == 1, "sigma_sky<=0 degenerate");
        p = makeParams(std::nan(""), 2.5, 10.0); check(snr_source_snr_f64(&p, &r) == 0 && r.status == 1, "NaN flux degenerate");
        // 量纲/单调性: SNR 与 flux 线性 (天空受限)
        SnrSourceResult a, b;
        p = makeParams(1e4, 2.5, 10.0); snr_source_snr_f64(&p, &a);
        p = makeParams(2e4, 2.5, 10.0); snr_source_snr_f64(&p, &b);
        checkClose(b.snr_optimal / a.snr_optimal, 2.0, 1e-12, "sky-limited SNR linear in flux");
    }

    if (all || grp == "production") {
        // 生产路径: snr_extract_model_v3 控制点必须是 Horne SNR, 不再是 (A-B)/residual_scale
        const int n = 5;
        std::vector<double> psf((size_t)n * 9, 0.0);
        std::vector<int64_t> ids((size_t)n);
        std::vector<uint32_t> qf((size_t)n, 0u), pstat((size_t)n, 0u);
        double old_ratio[5];
        for (int i = 0; i < n; ++i) {
            double* row = psf.data() + (size_t)i * 9;
            const double sigma = kRealFwhm[i] / kFwhmFactor;
            const double A = kRealFlux[i] * 3.0 / (2.0 * kPi * sigma * sigma);
            row[0] = 0.0; row[1] = 100.0; row[2] = kRealFlux[i];
            row[3] = 100.0 + i; row[4] = 100.0 + i; row[5] = kRealFwhm[i];
            row[6] = A; row[7] = kRealSky * kTrimToSigma; row[8] = 0.0;
            ids[(size_t)i] = 1000 + i;
            old_ratio[i] = (row[6] - row[1]) / row[7];
        }
        SnrWcsParams wcs; std::memset(&wcs, 0, sizeof(wcs));
        wcs.crval1 = 10.0; wcs.crval2 = 20.0; wcs.crpix1 = 1.0; wcs.crpix2 = 1.0;
        wcs.cd[0] = 1.0 / 3600.0; wcs.cd[3] = 1.0 / 3600.0;
        SnrModelV3 m; std::memset(&m, 0, sizeof(m));
        const int rc = snr_extract_model_v3(psf.data(), n, 0.05, &wcs, 1,
                                            ids.data(), qf.data(), pstat.data(), &m);
        check(rc == 0 && m.n_points == (uint32_t)n, "extract_v3 rc/n_points");
        if (m.points && m.value_dtype == 1) {
            auto* pts = (SnrControlPointF64V3*)m.points;
            for (uint32_t i = 0; i < m.n_points; ++i) {
                RefResult e = refCompute(kRealFlux[i], kRealFwhm[i], kRealSky, 0, 0, 0, 0, 0);
                char b[64];
                std::snprintf(b, sizeof(b), "extract_v3 point%u == Horne SNR", i);
                checkClose(pts[i].snr_psf, e.snr_optimal, 1e-12, b);
                // 退休断言: 控制点不得等于旧 (A-B)/residual_scale
                std::snprintf(b, sizeof(b), "extract_v3 point%u retired (A-B)/mad removed", i);
                check(std::fabs(pts[i].snr_psf - old_ratio[i]) > 1e-6, b);
            }
            // frame-level: snr_phot == median_snr == median(SNR_F); F5 = 5*med_sky/sqrt(sum_p2(med_fwhm))
            std::vector<double> snrs;
            for (uint32_t i = 0; i < m.n_points; ++i) snrs.push_back(pts[i].snr_psf);
            std::vector<double> c = snrs;
            std::sort(c.begin(), c.end());
            const double med = c[c.size() / 2];
            checkClose(m.snr_phot, med, 1e-12, "extract_v3 snr_phot == median(SNR_F)");
            checkClose(m.median_snr, med, 1e-12, "extract_v3 median_snr == median(SNR_F)");
            checkClose(m.median_source_snr, med, 1e-12, "extract_v3 median_source_snr");
            std::vector<double> fw(kRealFwhm, kRealFwhm + 5);
            std::sort(fw.begin(), fw.end());
            const double med_fwhm = fw[2];
            double sp2 = 0, pc = 0;
            snr_moffat4_profile_f64(med_fwhm, 0.0, 0, &sp2, &pc);
            checkClose(m.frame_depth_flux5_adu, 5.0 * kRealSky / std::sqrt(sp2), 1e-12,
                       "extract_v3 frame_depth_flux5_adu");
            check(std::isnan(m.frame_depth_m5_mag), "extract_v3 m5 NaN without ZP");
        } else {
            check(false, "extract_v3 f64 points present");
        }
        if (m.points) std::free(m.points);
    }

    if (all || grp == "determinism") {
        SnrSourceParams p = makeParams(1.0e4, 2.5, 10.0, 2.0, 5.0, 1.5 * 2.5, 0.0, 25.0);
        SnrSourceResult a, b;
        snr_source_snr_f64(&p, &a);
        snr_source_snr_f64(&p, &b);
        check(std::memcmp(&a, &b, sizeof(a)) == 0, "determinism bitwise (source)");
        std::vector<double> psf(9, 0.0);
        psf[0] = 0.0; psf[1] = 100.0; psf[2] = 1e4; psf[3] = 10.0; psf[4] = 10.0;
        psf[5] = 2.5; psf[6] = 1e4; psf[7] = 500.0; psf[8] = 0.0;
        SnrWcsParams wcs; std::memset(&wcs, 0, sizeof(wcs));
        wcs.crval1 = 10.0; wcs.crval2 = 20.0; wcs.crpix1 = 1.0; wcs.crpix2 = 1.0;
        wcs.cd[0] = 1.0 / 3600.0; wcs.cd[3] = 1.0 / 3600.0;
        SnrModelV3 m1, m2;
        std::memset(&m1, 0, sizeof(m1)); std::memset(&m2, 0, sizeof(m2));
        snr_extract_model_v3(psf.data(), 1, 0.05, &wcs, 1, nullptr, nullptr, nullptr, &m1);
        snr_extract_model_v3(psf.data(), 1, 0.05, &wcs, 1, nullptr, nullptr, nullptr, &m2);
        check(m1.n_points == m2.n_points && m1.snr_phot == m2.snr_phot &&
              m1.frame_depth_flux5_adu == m2.frame_depth_flux5_adu, "determinism bitwise (extract_v3)");
        if (m1.points) std::free(m1.points);
        if (m2.points) std::free(m2.points);
    }

    std::printf("P1SNR-SCIENCE [%s]: %d/%d checks passed, %d failed\n",
                grp.c_str(), g_total - g_fail, g_total, g_fail);
    return g_fail == 0 ? 0 : 1;
}
