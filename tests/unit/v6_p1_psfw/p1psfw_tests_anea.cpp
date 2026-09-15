/* p1psfw_tests_anea.cpp - A_NEA / PSF 归一 / effective PSF (正+负) */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <limits>
#include <string>

using namespace astrocs::v6::p1psfw;

P1PSFW_REGISTER(anea) {
    (void)mode;
    /* 正向: 归一高斯 PSF 的 A_NEA == 独立 Oracle (long double 显式 Sum P^2) */
    const std::vector<double> p = fixture::gaussian_psf(33, 2.0);
    const PsfProfileStats st = psf_profile_stats(p.data(), p.size());
    P1_CHECKF("anea_ok", st.ok);
    P1_CHECK_NEAR(st.sum_p, 1.0, 1e-12);
    P1_CHECK_NEAR(st.a_nea, static_cast<double>(oracle::o_anea(p.data(), p.size())), 1e-12);

    /* 窄 PSF 的 A_NEA 更小 (噪声等效面积随集中度下降) */
    const std::vector<double> narrow = fixture::gaussian_psf(33, 1.0);
    const std::vector<double> wide = fixture::gaussian_psf(33, 3.0);
    const double a_narrow = psf_anea(narrow.data(), narrow.size());
    const double a_wide = psf_anea(wide.data(), wide.size());
    P1_CHECK(a_narrow < a_wide);
    P1_CHECK(a_narrow > 0.0 && a_wide > 0.0);

    /* 2D 高斯: A_NEA 与 Oracle 一致 */
    const std::vector<double> p2 = fixture::gaussian_psf_2d(25, 3.0);
    P1_CHECK_NEAR(psf_anea(p2.data(), p2.size()),
                  static_cast<double>(oracle::o_anea(p2.data(), p2.size())), 1e-12);

    /* 负向: 未归一 -> REJECT not_normalized */
    std::vector<double> bad = p;
    for (double& v : bad) v *= 2.0;
    P1_CHECK(psf_profile_stats(bad.data(), bad.size()).reject != nullptr);
    P1_CHECK(std::string(psf_profile_stats(bad.data(), bad.size()).reject) == "not_normalized");
    /* 负向: 负样本 */
    std::vector<double> neg = p;
    neg[0] = -1.0;
    P1_CHECK(std::string(psf_profile_stats(neg.data(), neg.size()).reject) == "negative_sample");
    /* 负向: 空/零/非有限 */
    P1_CHECK(psf_profile_stats(nullptr, 0).reject != nullptr);
    std::vector<double> zero(p.size(), 0.0);
    P1_CHECK(psf_profile_stats(zero.data(), zero.size()).reject != nullptr);
    std::vector<double> inf = p;
    inf[1] = std::numeric_limits<double>::infinity();
    P1_CHECK(std::string(psf_profile_stats(inf.data(), inf.size()).reject) == "non_finite");

    /* ---------------- effective PSF ---------------- */
    const std::vector<double> q1 = fixture::gaussian_psf(65, 2.0);
    const std::vector<double> q2 = fixture::gaussian_psf(65, 3.0);
    const std::vector<const double*> profs = {q1.data(), q2.data()};
    const std::vector<double> a_k = {1.0, 1.0};
    const std::vector<double> alpha_k = {0.75, 0.25};
    const EffectivePsf eff = conventional_effective_psf(
        profs, q1.size(), a_k, alpha_k, EffectivePsfNormalization::peak, "eff-1");
    P1_CHECK(eff.ok);
    P1_CHECK(!eff.only_fwhm_scalar);            /* FZ-GATE-PSFSW-EPSF: 非仅 FWHM */
    P1_CHECK(eff.normalization_declared);
    /* peak 归一: 峰 = 1 */
    const std::size_t centre = q1.size() / 2;
    P1_CHECK_NEAR(eff.profile[centre], 1.0, 1e-12);
    /* 独立复算: P_eff = (sum alpha a P) / (sum alpha a P(centre)) (long double) */
    long double num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < q1.size(); ++i) {
        const long double w = static_cast<long double>(alpha_k[0]) * a_k[0] * q1[i] +
                              static_cast<long double>(alpha_k[1]) * a_k[1] * q2[i];
        num = w;
        const long double wc = static_cast<long double>(alpha_k[0]) * a_k[0] * q1[centre] +
                               static_cast<long double>(alpha_k[1]) * a_k[1] * q2[centre];
        den = wc;
        P1_CHECK_NEAR(eff.profile[i], static_cast<double>(num / den), 1e-12);
    }
    /* integral 归一: Sum = 1 */
    const EffectivePsf eff_i = conventional_effective_psf(
        profs, q1.size(), a_k, alpha_k, EffectivePsfNormalization::integral, "eff-2");
    P1_CHECK(eff_i.ok);
    double isum = 0.0;
    for (double v : eff_i.profile) isum += v;
    P1_CHECK_NEAR(isum, 1.0, 1e-12);
    /* V7: FWHM(P_eff) != median(FWHM_k) */
    const double f1 = measure_fwhm(q1.data(), q1.size());
    const double f2 = measure_fwhm(q2.data(), q2.size());
    const double fmed = 0.5 * (f1 + f2);
    P1_CHECK(std::fabs(eff.fwhm - fmed) > 1e-6);
    P1_CHECK(eff.fwhm > std::min(f1, f2) && eff.fwhm < std::max(f1, f2));
    P1_CHECK(eff.ee_r1 > 0.0 && eff.ee_r1 < 1.0);   /* 1*FWHM 内能量 < 1 (Gaussian ~0.76) */
    P1_CHECK(eff.ee_r2 > eff.ee_r1 && eff.ee_r2 <= 1.0 + 1e-12);

    /* 负向: 空 id / 尺寸不符 / 空 profiles -> REJECT */
    P1_CHECK(!conventional_effective_psf(profs, q1.size(), a_k, alpha_k,
               EffectivePsfNormalization::peak, "").ok);
    P1_CHECK(!conventional_effective_psf(profs, q1.size(), {1.0}, alpha_k,
               EffectivePsfNormalization::peak, "x").ok);
    P1_CHECK(!conventional_effective_psf({}, q1.size(), {}, {},
               EffectivePsfNormalization::peak, "x").ok);
}
