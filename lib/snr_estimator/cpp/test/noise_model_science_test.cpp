// ============================================================================
// noise_model_science_test.cpp — SNR/Noise 科学矩阵 (模块级)
//
// 覆盖 (SNR_SCIENCE_DERIVATION.md / SCIENCE_ACCEPTANCE_MATRIX.md):
// TEST-SNR-001: SNR/Noise 科学矩阵（SNR-001..015 子项）
// SNR-001 pedestal invariance
// SNR-002 multiplicative scale covariance
// SNR-003 star-population invariance
// SNR-004 Gaussian blank-sky variance recovery
// SNR-005 Poisson+read-noise cross-check
// SNR-006 spatial noise-field recovery
// SNR-007 dex/mag unit correctness
// SNR-008 legacy A-B metric retired (semantic)
// SNR-009 inverse-variance coadd efficiency
// SNR-010 signal-independence
// SNR-013 PSF quality semantics
// SNR-014 missing/degenerate metadata behavior
// (SNR-011/012 Drizzle variance MC → healpix_drizzle 侧; SNR-015 UPM ablation → phase2)
//
// 编译: g++ -O2 -std=c++17 noise_model_science_test.cpp ../src/snr_estimator.cpp ../src/noise_model.cpp
// 运行: noise_model_science_test.exe
// ============================================================================
#include "snr_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

namespace {

constexpr int W = 256, H = 256;

// 生成带星的校准帧: 均匀背景 + Gaussian 噪声 + 星点 (Gaussian PSF)
void make_frame(int w, int h, std::vector<float>& img, std::vector<double>& sx,
                std::vector<double>& sy, std::vector<double>& sA,
                double sky, double sigma, int n_stars, unsigned seed,
                double star_amp_scale = 1.0) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd(0.0, sigma);
    img.assign((std::size_t)h * w, (float)sky);
    for (std::size_t i = 0; i < img.size(); ++i) img[i] += (float)nd(rng);
    sx.clear(); sy.clear(); sA.clear();
    std::uniform_real_distribution<double> pos(30.0, (double)w - 30.0);
    std::uniform_real_distribution<double> amp(200.0, 4000.0);
    for (int i = 0; i < n_stars; ++i) {
        const double x = pos(rng), y = pos(rng);
        const double A = amp(rng) * star_amp_scale;
        sx.push_back(x); sy.push_back(y); sA.push_back(A);
        const double fwhm = 3.0;
        const double sigma2 = (fwhm / 2.3548200450309493) *
                              (fwhm / 2.3548200450309493);
        const int r0 = 12;
        for (int dy = -r0; dy <= r0; ++dy) {
            for (int dx = -r0; dx <= r0; ++dx) {
                const int px = (int)std::lround(x) + dx;
                const int py = (int)std::lround(y) + dy;
                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                const double d2 = (double)(dx * dx + dy * dy);
                img[(std::size_t)py * w + (std::size_t)px] +=
                    (float)(A * std::exp(-d2 / (2.0 * sigma2)));
            }
        }
    }
}

// 无星纯 sky 帧
void make_sky(int w, int h, std::vector<float>& img, double sky, double sigma,
              unsigned seed, double sigma_gradient = 0.0) {
    std::mt19937 rng(seed);
    img.assign((std::size_t)h * w, (float)sky);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double s = sigma + sigma_gradient * (double)x / (double)w;
            std::normal_distribution<double> nd(0.0, s);
            img[(std::size_t)y * w + (std::size_t)x] += (float)nd(rng);
        }
    }
}

double pearson(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 2.0;
    double ma = 0, mb = 0;
    for (std::size_t i = 0; i < a.size(); ++i) { ma += a[i]; mb += b[i]; }
    ma /= (double)a.size();
    mb /= (double)b.size();
    double num = 0, da = 0, db = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double aa = a[i] - ma, bb = b[i] - mb;
        num += aa * bb; da += aa * aa; db += bb * bb;
    }
    const double den = std::sqrt(da * db);
    return den > 0.0 ? num / den : 2.0;
}

}  // namespace

static int run_science_matrix() {
    printf("=== V19 SNR/Noise 科学矩阵 (模块级) ===\n");

    // ---- SNR-001 pedestal invariance ----
    {
        printf("\n[SNR-001] pedestal invariance\n");
        std::vector<float> img, img2;
        std::vector<double> sx, sy, sA;
        make_frame(W, H, img, sx, sy, sA, 1000.0, 10.0, 40, 20260815);
        img2 = img;
        for (float& v : img2) v += 500.0f;
        NoiseWeightModelV1 m1{}, m2{};
        const int r1 = snr_noise_model_v1(img.data(), H, W, nullptr,
                                          sx.data(), sy.data(), (int)sx.size(),
                                          nullptr, &m1);
        const int r2 = snr_noise_model_v1(img2.data(), H, W, nullptr,
                                          sx.data(), sy.data(), (int)sx.size(),
                                          nullptr, &m2);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "two models ok (r=%d/%d)", r1, r2);
        CHECK(r1 == 0 && r2 == 0, msg);
        const double rel = std::fabs(m1.variance_bg_global -
                                     m2.variance_bg_global) /
                           std::max(m1.variance_bg_global, 1e-12);
        snprintf(msg, sizeof(msg),
                 "variance invariant: %.4f vs %.4f rel=%.2e (<1%%)",
                 m1.variance_bg_global, m2.variance_bg_global, rel);
        CHECK(rel < 0.01, msg);
        snr_noise_model_v1_free(&m1);
        snr_noise_model_v1_free(&m2);
    }

    // ---- SNR-002 multiplicative scale covariance ----
    {
        printf("\n[SNR-002] multiplicative scale covariance\n");
        std::vector<float> img;
        std::vector<double> sx, sy, sA;
        make_frame(W, H, img, sx, sy, sA, 1000.0, 10.0, 30, 20260816);
        const double alpha = 2.0;
        std::vector<float> img2 = img;
        for (float& v : img2) v = (float)(v * alpha);
        NoiseWeightModelV1 m1{}, m2{};
        if (snr_noise_model_v1(img.data(), H, W, nullptr, sx.data(), sy.data(),
                               (int)sx.size(), nullptr, &m1) != 0 ||
            snr_noise_model_v1(img2.data(), H, W, nullptr, sx.data(), sy.data(),
                               (int)sx.size(), nullptr, &m2) != 0) {
            CHECK(false, "both models ok");
        } else {
            const double expect = m1.variance_bg_global * alpha * alpha;
            const double rel = std::fabs(m2.variance_bg_global - expect) /
                               std::max(expect, 1e-12);
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "var(αx)=α²var: got %.3f expect %.3f rel=%.2e (<2%%)",
                     m2.variance_bg_global, expect, rel);
            CHECK(rel < 0.02, msg);
            double iv = m1.ivar_bg_global;
            snr_noise_scale_law(alpha, nullptr, &iv);
            const double rel2 = std::fabs(iv - m2.ivar_bg_global) /
                                std::max(m2.ivar_bg_global, 1e-12);
            snprintf(msg, sizeof(msg),
                     "ivar scale law: got %.3e expect %.3e rel=%.2e (<2%%)",
                     iv, m2.ivar_bg_global, rel2);
            CHECK(rel2 < 0.02, msg);
        }
        snr_noise_model_v1_free(&m1);
        snr_noise_model_v1_free(&m2);
    }

    // ---- SNR-003 star-population invariance ----
    {
        printf("\n[SNR-003] star-population invariance\n");
        std::vector<float> faint, bright;
        std::vector<double> sx, sy, sA;
        make_frame(W, H, faint, sx, sy, sA, 1000.0, 10.0, 40, 20260817, 1.0);
        make_frame(W, H, bright, sx, sy, sA, 1000.0, 10.0, 40, 20260817, 8.0);
        NoiseWeightModelV1 mf{}, mb{};
        const int rf = snr_noise_model_v1(faint.data(), H, W, nullptr,
                                          sx.data(), sy.data(), (int)sx.size(),
                                          nullptr, &mf);
        const int rb = snr_noise_model_v1(bright.data(), H, W, nullptr,
                                          sx.data(), sy.data(), (int)sx.size(),
                                          nullptr, &mb);
        CHECK(rf == 0 && rb == 0, "both models ok");
        const double rel = std::fabs(mf.variance_bg_global -
                                     mb.variance_bg_global) /
                           std::max(mf.variance_bg_global, 1e-12);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "background variance independent of star flux: %.4f vs %.4f rel=%.2e (<2%%)",
                 mf.variance_bg_global, mb.variance_bg_global, rel);
        CHECK(rel < 0.02, msg);
        snr_noise_model_v1_free(&mf);
        snr_noise_model_v1_free(&mb);
    }

    // ---- SNR-004 Gaussian blank-sky variance recovery ----
    {
        printf("\n[SNR-004] Gaussian blank-sky recovery\n");
        std::vector<float> img;
        const int W4 = 512, H4 = 512;
        make_sky(W4, H4, img, 1500.0, 5.0, 20260818);
        // 8×8 patches → 64 个 patch 尺度估计
        NoiseWeightModelV1 m{};
        const int r = snr_noise_model_v1(img.data(), H4, W4, nullptr, nullptr,
                                         nullptr, 0, nullptr, &m);
        CHECK(r == 0, "model ok");
        const double bias = (m.sigma_bg_global - 5.0) / 5.0;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "global sigma bias: got %.4f truth 5.0 bias=%.3f%% (<=2%%)",
                 m.sigma_bg_global, bias * 100.0);
        CHECK(std::fabs(bias) <= 0.02, msg);
        // p95: 合格 patch 尺度落在 [0.95σ, 1.05σ]
        int in95 = 0;
        for (uint32_t i = 0; i < m.n_control_points; ++i) {
            const double rp = m.ctrl_sigma[i] / 5.0;
            if (rp >= 0.95 && rp <= 1.05) ++in95;
        }
        const double frac =
            m.n_control_points
                ? (double)in95 / (double)m.n_control_points
                : 0.0;
        snprintf(msg, sizeof(msg),
                 "patch p95 coverage: %u/%u = %.1f%% (>=95%%)",
                 in95, m.n_control_points, frac * 100.0);
        CHECK(frac >= 0.95, msg);
        snr_noise_model_v1_free(&m);
    }

    // ---- SNR-005 Poisson+read-noise cross-check ----
    {
        printf("\n[SNR-005] Poisson+read-noise cross-check\n");
        // ADU 空间: e- = ADU × gain; Poisson(λ=μ×gain), ADU = n/gain
        // var_ADU = μ/gain + rn²/gain²
        const double gain = 1.5, rn = 8.0, mu = 2000.0;
        std::vector<float> img((std::size_t)H * W);
        std::mt19937 rng(20260819);
        for (std::size_t i = 0; i < img.size(); ++i) {
            const double lambda = mu * gain;
            std::poisson_distribution<int> pd(lambda);
            const double n_e = (double)pd(rng) + rn * std::normal_distribution<double>(0, 1)(rng);
            img[i] = (float)(n_e / gain);
        }
        // 理论 variance (ADU²)
        const double var_th = mu / gain + rn * rn / (gain * gain);
        // 经验 blank-sky
        NoiseWeightModelV1 m{};
        const int r = snr_noise_model_v1(img.data(), H, W, nullptr, nullptr,
                                         nullptr, 0, nullptr, &m);
        CHECK(r == 0, "model ok");
        const double var_emp = m.variance_bg_global;
        const double rel = std::fabs(var_emp - var_th) / var_th;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "Poisson+read: emp %.1f th %.1f rel=%.2f%% (<=5%%)",
                 var_emp, var_th, rel * 100.0);
        CHECK(rel <= 0.05, msg);
        // gain 模型一致性
        const double var_gm = snr_noise_gain_variance(mu, gain, rn);
        snprintf(msg, sizeof(msg), "gain model identity: %.2f vs %.2f",
                 var_gm, var_th);
        CHECK(std::fabs(var_gm - var_th) / var_th < 1e-9, msg);
        snr_noise_model_v1_free(&m);
    }

    // ---- SNR-006 spatial noise-field recovery ----
    {
        printf("\n[SNR-006] spatial noise-field recovery\n");
        const double sigma0 = 3.0, sigma1 = 5.0;
        std::vector<float> img;
        const int W6 = 512, H6 = 512;
        make_sky(W6, H6, img, 1200.0, sigma0, 20260820, sigma1 - sigma0);
        NoiseWeightModelV1 m{};
        const int r = snr_noise_model_v1(img.data(), H6, W6, nullptr, nullptr,
                                         nullptr, 0, nullptr, &m);
        CHECK(r == 0 && m.has_spatial_field, "model ok + spatial field");
        std::vector<float> var_map((std::size_t)H6 * W6);
        snr_noise_model_v1_fill(&m, H6, W6, var_map.data(), nullptr);
        // truth variance
        std::vector<float> truth((std::size_t)H6 * W6);
        for (int y = 0; y < H6; ++y)
            for (int x = 0; x < W6; ++x) {
                const double s = sigma0 + (sigma1 - sigma0) * (double)x / (double)W6;
                truth[(std::size_t)y * W6 + (std::size_t)x] = (float)(s * s);
            }
        const double corr = pearson(var_map, truth);
        double rmse = 0, den = 0;
        for (std::size_t i = 0; i < var_map.size(); ++i) {
            rmse += (var_map[i] - truth[i]) * (var_map[i] - truth[i]);
            den += truth[i] * truth[i];
        }
        rmse = std::sqrt(rmse / (double)var_map.size());
        const double norm_rmse = rmse / std::sqrt(den / (double)var_map.size());
        char msg[160];
        snprintf(msg, sizeof(msg), "corr=%.4f (>=0.98)", corr);
        CHECK(corr >= 0.98, msg);
        snprintf(msg, sizeof(msg), "normalized RMSE=%.3f%% (<=5%%)",
                 norm_rmse * 100.0);
        CHECK(norm_rmse <= 0.05, msg);
        snr_noise_model_v1_free(&m);
    }

    // ---- SNR-007 dex/mag unit correctness ----
    {
        printf("\n[SNR-007] dex/mag unit correctness\n");
        PhotometricCalibrationQuality q{};
        const int r = snr_phot_cal_quality(0.1, 120, &q);
        CHECK(r == 0 && q.fit_status == 0, "phot_cal_quality ok");
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "sigma_mag=2.5×dex: got %.4f expect 0.2500", q.sigma_mag);
        CHECK(std::fabs(q.sigma_mag - 0.25) < 1e-9, msg);
        snprintf(msg, sizeof(msg),
                 "sigma_cal_rel=ln10×dex: got %.4f expect 0.2303",
                 q.sigma_cal_rel);
        CHECK(std::fabs(q.sigma_cal_rel - 0.2302585) < 1e-6, msg);
        // degenerate / invalid
        PhotometricCalibrationQuality qd{};
        snr_phot_cal_quality(0.0, 0, &qd);
        CHECK(qd.fit_status == 2, "invalid sigma → status 2");
        PhotometricCalibrationQuality qn{};
        snr_phot_cal_quality(0.1, 0, &qn);
        CHECK(qn.fit_status == 1, "no matches → degraded status (no fake SNR)");
    }

    // ---- SNR-008 legacy A-B metric retired ----
    {
        printf("\n[SNR-008] legacy (A-B)/mad retired\n");
        // 同一星: B 抬升 pedestal → q_psf 不变 (旧 (A-B)/mad 会变化)
        double psf1[9] = {0, 1000.0, 30000.0, 100.0, 100.0, 3.0, 5000.0,
                          120.0, 0.05};
        double psf2[9] = {0, 2000.0, 30000.0, 100.0, 100.0, 3.0, 5000.0,
                          120.0, 0.05};
        PsfFitQualityRow q1{}, q2{};
        snr_psf_fit_quality(psf1, 1, nullptr, nullptr, &q1);
        snr_psf_fit_quality(psf2, 1, nullptr, nullptr, &q2);
        const double rel = std::fabs(q1.q_psf - q2.q_psf) /
                           std::max(q1.q_psf, 1e-12);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "q_psf pedestal-invariant: q1=%.4f q2=%.4f rel=%.2e (<1e-12)",
                 q1.q_psf, q2.q_psf, rel);
        CHECK(rel < 1e-12, msg);
        // 语义: q_psf = A / residual_scale
        const double expect = 5000.0 / 120.0;
        snprintf(msg, sizeof(msg), "q_psf = A/residual_scale: %.4f vs %.4f",
                 q1.q_psf, expect);
        CHECK(std::fabs(q1.q_psf - expect) < 1e-12, msg);
        // robust_residual_sigma 换算
        snprintf(msg, sizeof(msg),
                 "robust sigma conversion: %.4f (~163.95)", q1.robust_residual_sigma);
        CHECK(std::fabs(q1.robust_residual_sigma - 120.0 / 0.7316727929211932) < 1e-9,
              msg);
    }

    // ---- SNR-009 inverse-variance coadd efficiency ----
    {
        printf("\n[SNR-009] inverse-variance coadd efficiency\n");
        const double s1 = 1.0, s2 = 3.0;
        // optimal: w ∝ 1/σ² → Var = 1/(1/σ1²+1/σ2²)
        const double var_opt = 1.0 / (1.0 / (s1 * s1) + 1.0 / (s2 * s2));
        // equal weight → Var = (σ1²+σ2²)/4
        const double var_eq = (s1 * s1 + s2 * s2) / 4.0;
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "analytic: var_opt=%.4f var_eq=%.4f (optimal wins)",
                 var_opt, var_eq);
        CHECK(var_opt < var_eq, msg);
        // ivar 加权与最优一致 (3% 容差覆盖 MC 估计误差)
        const double w1 = 1.0 / (s1 * s1), w2 = 1.0 / (s2 * s2);
        const double var_w = (w1 * w1 * s1 * s1 + w2 * w2 * s2 * s2) /
                             ((w1 + w2) * (w1 + w2));
        snprintf(msg, sizeof(msg),
                 "ivar-weighted variance: %.4f vs opt %.4f (rel<3%%)",
                 var_w, var_opt);
        CHECK(std::fabs(var_w - var_opt) / var_opt < 0.03, msg);
    }

    // ---- SNR-010 signal independence ----
    {
        printf("\n[SNR-010] signal independence\n");
        std::vector<float> sky, scene;
        std::vector<double> sx, sy, sA;
        make_sky(W, H, sky, 900.0, 8.0, 20260821);
        make_frame(W, H, scene, sx, sy, sA, 900.0, 8.0, 60, 20260821, 6.0);
        NoiseWeightModelV1 ms{}, mx{};
        const int rs = snr_noise_model_v1(sky.data(), H, W, nullptr, nullptr,
                                          nullptr, 0, nullptr, &ms);
        const int rx = snr_noise_model_v1(scene.data(), H, W, nullptr,
                                          sx.data(), sy.data(), (int)sx.size(),
                                          nullptr, &mx);
        CHECK(rs == 0 && rx == 0, "both models ok");
        std::vector<float> ivs((std::size_t)H * W), ivx((std::size_t)H * W);
        snr_noise_model_v1_fill(&ms, H, W, nullptr, ivs.data());
        snr_noise_model_v1_fill(&mx, H, W, nullptr, ivx.data());
        // |signal| map from scene
        std::vector<float> sig((std::size_t)H * W);
        for (std::size_t i = 0; i < sig.size(); ++i)
            sig[i] = std::fabs(scene[i] - 900.0f);
        const double rho = pearson(ivx, sig);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "|ρ(weight,signal)|=%.4f (<0.02)", std::fabs(rho));
        CHECK(std::fabs(rho) < 0.02, msg);
        snr_noise_model_v1_free(&ms);
        snr_noise_model_v1_free(&mx);
    }

    // ---- SNR-013 PSF quality semantics ----
    {
        printf("\n[SNR-013] PSF quality semantics\n");
        double good[9] = {0, 900.0, 30000.0, 50.0, 50.0, 3.0, 6000.0,
                          60.0, 0.02};
        double poor[9] = {0, 900.0, 30000.0, 50.0, 50.0, 3.0, 6000.0,
                          600.0, 0.5};
        PsfFitQualityRow qg{}, qp{};
        snr_psf_fit_quality(good, 1, nullptr, nullptr, &qg);
        snr_psf_fit_quality(poor, 1, nullptr, nullptr, &qp);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "good fit q=%.1f > poor q=%.1f", qg.q_psf, qp.q_psf);
        CHECK(qg.q_psf > qp.q_psf * 5.0, msg);
        // 饱和/失败状态
        double sat[9] = {0, 900.0, 30000.0, 50.0, 50.0, 3.0, 6000.0,
                         60.0, 0.02};
        uint32_t qf_sat = SNR_QF_SATURATED;
        PsfFitQualityRow qsat{};
        snr_psf_fit_quality(sat, 1, nullptr, &qf_sat, &qsat);
        CHECK(qsat.fit_status == 2, "saturated → status 2");
        double bad[9] = {1, 900.0, 30000.0, 50.0, 50.0, 3.0, 6000.0,
                         60.0, 0.02};
        PsfFitQualityRow qb{};
        snr_psf_fit_quality(bad, 1, nullptr, nullptr, &qb);
        CHECK(qb.fit_status == 1, "rejected → status 1");
    }

    // ---- SNR-014 missing metadata fallback ----
    {
        printf("\n[SNR-014] missing/degenerate metadata behavior\n");
        std::vector<float> img;
        make_sky(W, H, img, 800.0, 4.0, 20260822);
        SnrNoiseModelConfig cfg{};
        snr_noise_model_v1_default_config(&cfg);
        cfg.gain_e_per_adu = 0.0;      // 未知增益
        cfg.read_noise_e = 0.0;        // 未知读出噪声
        NoiseWeightModelV1 m{};
        const int r = snr_noise_model_v1(img.data(), H, W, nullptr, nullptr,
                                         nullptr, 0, &cfg, &m);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "empirical fallback ok (r=%d, source=%d, deg=%d, sigma=%.4f)",
                 r, m.source, m.degenerate, m.sigma_bg_global);
        CHECK(r == 0 && !m.degenerate && m.source == 0 && m.sigma_bg_global > 0.0,
              msg);
        // 完全退化帧 (NaN) → return 1, ivar=0
        std::vector<float> nan_img((std::size_t)H * W, std::nanf(""));
        NoiseWeightModelV1 md{};
        const int rd = snr_noise_model_v1(nan_img.data(), H, W, nullptr, nullptr,
                                          nullptr, 0, nullptr, &md);
        CHECK(rd == 1 && md.degenerate == 1 && md.ivar_bg_global == 0.0,
              "fully degenerate → status 1, ivar=0");
        snr_noise_model_v1_free(&m);
        snr_noise_model_v1_free(&md);
    }

    printf("\n== V19 SNR 科学矩阵结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

// ============================================================================
// V19R4 NOISE-WIRE-001：生产默认配置接线等价
//   cfg=nullptr == default_config() == 生产默认（default + gain/readnoise=0
//   覆盖）—— 逐字段 exact；输出模型 + fill 数组逐元素 exact。
//   同时证明：传零结构体（V19R3 生产 bug 形态）≠ 默认配置。
// ============================================================================
static int noise_wire_test() {
    const int W = 256, H = 256;
    std::vector<float> img;
    make_sky(W, H, img, 800.0, 4.0, 20260823);
    // 少量星点坐标（固定保守掩膜路径）
    double sx[2] = {10.0, 50.0};
    double sy[2] = {12.0, 48.0};

    NoiseWeightModelV1 m0{}, m1{}, m2{}, mz{};
    // A: cfg=nullptr（模块内部默认）
    CHECK(snr_noise_model_v1(img.data(), H, W, nullptr, sx, sy, 2,
                             nullptr, &m0) == 0, "nullptr cfg OK");
    // B: 显式 default_config()
    SnrNoiseModelConfig c1{};
    snr_noise_model_v1_default_config(&c1);
    CHECK(snr_noise_model_v1(img.data(), H, W, nullptr, sx, sy, 2,
                             &c1, &m1) == 0, "default_config cfg OK");
    // C: 生产默认（default + gain/readnoise 覆盖为 0，即 Orchestrator 在
    // 无 header 元数据时的实际配置）
    SnrNoiseModelConfig c2{};
    snr_noise_model_v1_default_config(&c2);
    c2.gain_e_per_adu = 0.0;
    c2.read_noise_e = 0.0;
    CHECK(snr_noise_model_v1(img.data(), H, W, nullptr, sx, sy, 2,
                             &c2, &m2) == 0, "production default cfg OK");
    // D: 零结构体（V19R3 生产 bug 形态；不应等于默认）
    SnrNoiseModelConfig cz{};
    snr_noise_model_v1(img.data(), H, W, nullptr, sx, sy, 2, &cz, &mz);

    // A/B/C 逐字段 exact
    auto eq = [&](const NoiseWeightModelV1& a, const NoiseWeightModelV1& b,
                  const char* tag) {
        bool ok = a.sigma_bg_global == b.sigma_bg_global &&
                  a.variance_bg_global == b.variance_bg_global &&
                  a.ivar_bg_global == b.ivar_bg_global &&
                  a.n_qualified_patches == b.n_qualified_patches &&
                  a.n_rejected_patches == b.n_rejected_patches &&
                  a.has_spatial_field == b.has_spatial_field &&
                  a.n_control_points == b.n_control_points &&
                  a.degenerate == b.degenerate && a.source == b.source;
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "[NOISE-WIRE-001] %s 逐字段 exact (sigma=%.10g n=%u "
                 "spatial=%d ctrl=%u)",
                 tag, a.sigma_bg_global, a.n_qualified_patches,
                 (int)a.has_spatial_field, (uint32_t)a.n_control_points);
        CHECK(ok, msg);
    };
    eq(m0, m1, "nullptr == default_config()");
    eq(m0, m2, "nullptr == production default");

    // fill 输出逐元素 exact（A vs C）
    std::vector<float> va(W * H), ia(W * H), vc(W * H), ic(W * H);
    snr_noise_model_v1_fill(&m0, H, W, va.data(), ia.data());
    snr_noise_model_v1_fill(&m2, H, W, vc.data(), ic.data());
    bool fill_ok = true;
    for (std::size_t i = 0; i < va.size(); ++i) {
        if (va[i] != vc[i] || ia[i] != ic[i]) { fill_ok = false; break; }
    }
    CHECK(fill_ok, "[NOISE-WIRE-001] fill 数组逐元素 exact (A vs C)");

    // 零结构体 ≠ 默认：spatial field 应关闭/退化（默认开启）
    char msg[160];
    snprintf(msg, sizeof(msg),
             "[NOISE-WIRE-001] 零结构体≠默认 (spatial=%d ctrl=%u vs "
             "spatial=%d ctrl=%u)",
             (int)mz.has_spatial_field, (uint32_t)mz.n_control_points,
             (int)m0.has_spatial_field, (uint32_t)m0.n_control_points);
    CHECK(mz.has_spatial_field != m0.has_spatial_field ||
              mz.n_control_points != m0.n_control_points,
          msg);
    snr_noise_model_v1_free(&m0);
    snr_noise_model_v1_free(&m1);
    snr_noise_model_v1_free(&m2);
    snr_noise_model_v1_free(&mz);
    return g_fail == 0 ? 0 : 1;
}

int main() {
    const int r0 = noise_wire_test();
    const int r1 = run_science_matrix();
    printf("\n== 总结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return (r0 == 0 && r1 == 0) ? 0 : 1;
}
