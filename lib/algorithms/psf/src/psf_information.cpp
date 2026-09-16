/* psf_information.cpp - A_NEA 与 effective PSF 实现 (IMPL-P1-PSFW-001)
 * 合同锚见 psf_information.h。纯 std + libm; 无 session 接线。 */
#include "astrocs/v6/psf_information.h"

#include <algorithm>
#include <cmath>

namespace astrocs {
namespace v6 {
namespace p1psfw {

const char* to_string(EffectivePsfNormalization n) {
    return n == EffectivePsfNormalization::integral ? "integral" : "peak";
}

PsfProfileStats psf_profile_stats(const double* p, std::size_t n, double norm_rtol) {
    PsfProfileStats s;
    if (p == nullptr || n == 0) {
        s.reject = (p == nullptr) ? "null_input" : "empty_support";
        return s;
    }
    s.n = n;
    double sum = 0.0, sum2 = 0.0;
    bool negative = false, nonfinite = false;
    for (std::size_t i = 0; i < n; ++i) {
        const double v = p[i];
        if (!std::isfinite(v)) {
            nonfinite = true;
            continue;
        }
        if (v < 0.0) negative = true;
        sum += v;
        sum2 += v * v;
    }
    if (nonfinite) {
        s.reject = "non_finite";
        return s;
    }
    if (negative) {
        s.reject = "negative_sample";
        return s;
    }
    if (sum <= 0.0) {
        s.reject = "empty_support";
        return s;
    }
    s.sum_p = sum;
    s.sum_p2 = sum2;
    s.norm_abs_error = std::fabs(sum - 1.0);
    if (s.norm_abs_error > norm_rtol) {
        s.reject = "not_normalized";
        return s;
    }
    s.a_nea = 1.0 / sum2;   /* [px^2] */
    s.ok = true;
    return s;
}

double psf_anea(const double* p, std::size_t n) {
    const PsfProfileStats s = psf_profile_stats(p, n);
    return s.ok ? s.a_nea : 0.0;
}

namespace {

/* 线性插值求解 profile 在 (峰/2) 处的半宽; 返回峰中心两侧半宽之和 (px)。 */
double fwhm_from_half(const std::vector<double>& y) {
    const std::size_t n = y.size();
    if (n < 2) return 0.0;
    std::size_t imax = 0;
    for (std::size_t i = 1; i < n; ++i)
        if (y[i] > y[imax]) imax = i;
    const double peak = y[imax];
    if (!(peak > 0.0)) return 0.0;
    const double half = peak / 2.0;

    double left = 0.0, right = 0.0;
    bool have_left = false, have_right = false;
    for (std::size_t i = imax; i-- > 0;) {
        if (y[i] <= half) {
            const double denom = y[i + 1] - y[i];
            const double t = (denom != 0.0) ? (half - y[i]) / denom : 0.0;
            left = static_cast<double>(imax) - (static_cast<double>(i) + t);
            have_left = true;
            break;
        }
    }
    for (std::size_t i = imax + 1; i < n; ++i) {
        if (y[i] <= half) {
            const double denom = y[i] - y[i - 1];
            const double t = (denom != 0.0) ? (half - y[i - 1]) / denom : 0.0;
            right = (static_cast<double>(i - 1) + t) - static_cast<double>(imax);
            have_right = true;
            break;
        }
    }
    if (!have_left && !have_right) return 0.0;
    if (!have_left) left = right;
    if (!have_right) right = left;
    return left + right;
}

}  /* namespace */

double measure_fwhm(const double* profile, std::size_t n) {
    if (profile == nullptr || n < 2) return 0.0;
    std::vector<double> y(profile, profile + n);
    return fwhm_from_half(y);
}

double measure_encircled_energy(const double* profile, std::size_t n, double radius_px) {
    if (profile == nullptr || n == 0) return 0.0;
    const double cx = static_cast<double>(n / 2);
    double total = 0.0, inside = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double v = profile[i];
        if (v <= 0.0) continue;
        total += v;
        if (std::fabs(static_cast<double>(i) - cx) <= radius_px) inside += v;
    }
    if (total <= 0.0) return 0.0;
    return inside / total;
}

EffectivePsf conventional_effective_psf(
    const std::vector<const double*>& profiles, std::size_t n,
    const std::vector<double>& a_k, const std::vector<double>& alpha_k,
    EffectivePsfNormalization normalization, const std::string& effective_psf_id) {
    EffectivePsf out;
    out.normalization = normalization;
    out.effective_psf_id = effective_psf_id;
    if (profiles.empty() || n == 0) {
        out.reject = "no_profiles";
        return out;
    }
    if (a_k.size() != profiles.size() || alpha_k.size() != profiles.size()) {
        out.reject = "coefficient_size_mismatch";
        return out;
    }
    if (effective_psf_id.empty()) {
        out.reject = "empty_effective_psf_id";
        return out;
    }
    /* Sum_k alpha_k a_k P_k / Sum_k alpha_k a_k P_k(0) */
    std::vector<double> num(n, 0.0);
    double denom_peak = 0.0;
    const std::size_t centre = n / 2;
    for (std::size_t k = 0; k < profiles.size(); ++k) {
        if (profiles[k] == nullptr) {
            out.reject = "null_profile";
            return out;
        }
        const PsfProfileStats st = psf_profile_stats(profiles[k], n);
        if (!st.ok) {
            out.reject = st.reject;
            return out;
        }
        const double w = alpha_k[k] * a_k[k];
        for (std::size_t i = 0; i < n; ++i) num[i] += w * profiles[k][i];
        denom_peak += w * profiles[k][centre];
    }
    if (!(denom_peak > 0.0) && normalization == EffectivePsfNormalization::peak) {
        out.reject = "zero_peak_denominator";
        return out;
    }
    double total = 0.0;
    for (std::size_t i = 0; i < n; ++i) total += num[i];
    if (!(total > 0.0)) {
        out.reject = "zero_integral";
        return out;
    }
    out.profile.resize(n);
    if (normalization == EffectivePsfNormalization::peak) {
        for (std::size_t i = 0; i < n; ++i) out.profile[i] = num[i] / denom_peak;
    } else {
        for (std::size_t i = 0; i < n; ++i) out.profile[i] = num[i] / total;
    }
    out.fwhm = fwhm_from_half(out.profile);
    out.ee_r1 = measure_encircled_energy(out.profile.data(), n, out.fwhm);
    out.ee_r2 = measure_encircled_energy(out.profile.data(), n, 2.0 * out.fwhm);
    out.only_fwhm_scalar = false;         /* 已产出完整算子脉冲响应 */
    out.normalization_declared = true;
    out.ok = true;
    return out;
}

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */
