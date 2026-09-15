/* psfsw.cpp - PSFSW 四分量/共同星集/复合/validity-depth/记录门实现 (IMPL-P1-PSFW-001)
 * 合同锚见 psfsw.h。纯 std + libm; 不接线 session。 */
#include "astrocs/v6/psfsw.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace astrocs {
namespace v6 {
namespace p1psfw {

const char* to_string(PsfswReason r) {
    switch (r) {
        case PsfswReason::none: return "none";
        case PsfswReason::no_common_star_set: return "no_common_star_set";
        case PsfswReason::background_nonpositive_undefined_transform:
            return "background_nonpositive_undefined_transform";
        case PsfswReason::insufficient_valid_stars: return "insufficient_valid_stars";
        case PsfswReason::selection_bias_gate_failed: return "selection_bias_gate_failed";
        case PsfswReason::spatial_nonuniformity_gate_failed:
            return "spatial_nonuniformity_gate_failed";
    }
    return "unknown";
}

bool is_whitelisted_reason(PsfswReason r) {
    return r == PsfswReason::no_common_star_set ||
           r == PsfswReason::background_nonpositive_undefined_transform ||
           r == PsfswReason::insufficient_valid_stars ||
           r == PsfswReason::selection_bias_gate_failed ||
           r == PsfswReason::spatial_nonuniformity_gate_failed;
}

bool parse_reason(const std::string& s, PsfswReason* out) {
    static const PsfswReason all[] = {
        PsfswReason::no_common_star_set,
        PsfswReason::background_nonpositive_undefined_transform,
        PsfswReason::insufficient_valid_stars,
        PsfswReason::selection_bias_gate_failed,
        PsfswReason::spatial_nonuniformity_gate_failed,
    };
    for (PsfswReason r : all) {
        if (s == to_string(r)) { if (out) *out = r; return true; }
    }
    return false;
}

const char* to_string(NCommonTier t) {
    switch (t) {
        case NCommonTier::hard_fail: return "hard_fail";
        case NCommonTier::low: return "low";
        case NCommonTier::standard: return "standard";
        case NCommonTier::preferred: return "preferred";
    }
    return "unknown";
}

NCommonTier n_common_tier(int n_common) {
    if (n_common < kNCommonMin) return NCommonTier::hard_fail;
    if (n_common < kNCommonRobust) return NCommonTier::low;
    if (n_common < kNCommonPreferred) return NCommonTier::standard;
    return NCommonTier::preferred;
}

/* ------------------------------------------------------------------------- */
/* 稳健统计                                                                    */
/* ------------------------------------------------------------------------- */
double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

double robust_scale_mad(const double* x, std::size_t n, double* out_median) {
    if (x == nullptr || n == 0) { if (out_median) *out_median = 0.0; return 0.0; }
    std::vector<double> v(x, x + n);
    const double med = median_of(v);
    if (out_median) *out_median = med;
    std::vector<double> dev(n);
    for (std::size_t i = 0; i < n; ++i) dev[i] = std::fabs(x[i] - med);
    return kMadToSigma * median_of(dev);
}

double quantile_of(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    if (p <= 0.0) return v.front();
    if (p >= 1.0) return v.back();
    const double pos = p * static_cast<double>(v.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
    if (lo == hi) return v[lo];
    const double f = pos - static_cast<double>(lo);
    return v[lo] * (1.0 - f) + v[hi] * f;
}

namespace {
std::vector<double> rank_with_ties(const std::vector<double>& x) {
    const std::size_t n = x.size();
    std::vector<std::size_t> idx(n);
    for (std::size_t i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        if (x[a] != x[b]) return x[a] < x[b];
        return a < b;
    });
    std::vector<double> r(n, 0.0);
    std::size_t i = 0;
    while (i < n) {
        std::size_t j = i;
        while (j + 1 < n && x[idx[j + 1]] == x[idx[i]]) ++j;
        const double avg = 0.5 * (static_cast<double>(i + 1) + static_cast<double>(j + 1));
        for (std::size_t k = i; k <= j; ++k) r[idx[k]] = avg;
        i = j + 1;
    }
    return r;
}
}  /* namespace */

double spearman_rho(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return 0.0;
    const std::vector<double> rx = rank_with_ties(x);
    const std::vector<double> ry = rank_with_ties(y);
    const std::size_t n = rx.size();
    double mx = 0.0, my = 0.0;
    for (std::size_t i = 0; i < n; ++i) { mx += rx[i]; my += ry[i]; }
    mx /= static_cast<double>(n);
    my /= static_cast<double>(n);
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double dx = rx[i] - mx, dy = ry[i] - my;
        sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
    }
    if (sxx <= 0.0 || syy <= 0.0) return 0.0;
    return sxy / std::sqrt(sxx * syy);
}

bool ExclusionFlags::any() const {
    return saturated || blended || trailed || moving || psf_mismatch || edge_truncated;
}

/* ------------------------------------------------------------------------- */
/* 四分量                                                                      */
/* ------------------------------------------------------------------------- */
namespace {

void fill_summary(ComponentMeasure& c, const std::string& name, const std::string& mid,
                  const std::string& unit, const std::string& est,
                  const std::string& est_ver, double value,
                  const std::vector<double>& samples) {
    c.name = name;
    c.measurement_id = mid;
    c.unit = unit;
    c.estimator = est;
    c.estimator_version = est_ver;
    c.value = value;
    if (samples.empty()) {
        c.p05 = c.p50 = c.p95 = value;
        c.valid_area_fraction = 1.0;
        c.spatial_spread_rel = 0.0;
        c.linear_trend_max_rel = 0.0;
        c.samples.clear();
        return;
    }
    c.p05 = quantile_of(samples, 0.05);
    c.p50 = quantile_of(samples, 0.50);
    c.p95 = quantile_of(samples, 0.95);
    std::size_t finite = 0;
    for (double v : samples) if (std::isfinite(v)) ++finite;
    c.valid_area_fraction = static_cast<double>(finite) / static_cast<double>(samples.size());
    c.spatial_spread_rel = (c.p50 > 0.0) ? (c.p95 - c.p05) / c.p50 : 0.0;
    /* 线性趋势: max |拟合值| / p50 */
    double trend = 0.0;
    if (samples.size() >= 2) {
        const double n = static_cast<double>(samples.size());
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const double xi = static_cast<double>(i);
            sx += xi; sy += samples[i]; sxx += xi * xi; sxy += xi * samples[i];
        }
        const double denom = n * sxx - sx * sx;
        if (std::fabs(denom) > 0.0) {
            const double slope = (n * sxy - sx * sy) / denom;
            const double intercept = (sy - slope * sx) / n;
            const double mean_fit = sy / n;   /* LS 拟合值的均值 == 样本均值 */
            for (std::size_t i = 0; i < samples.size(); ++i) {
                const double fit = slope * static_cast<double>(i) + intercept;
                trend = std::max(trend, std::fabs(fit - mean_fit));
            }
        }
        if (c.p50 > 0.0) trend /= c.p50;
    }
    c.linear_trend_max_rel = trend;
    c.samples = samples;
}

}  /* namespace */

PsfswFrameComponents extract_psfsw_components(const FrameComponentInput& in) {
    PsfswFrameComponents out;
    if (!(in.a_nea > 0.0) || !std::isfinite(in.a_nea)) {
        out.reject = "non_positive_a_nea";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    const std::size_t n = in.fhat.size();
    if (n < static_cast<std::size_t>(kNCommonMin)) {
        out.reject = "too_few_stars";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    if (!in.fhat_valid.empty() && in.fhat_valid.size() != n) {
        out.reject = "validity_size_mismatch";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    std::vector<double> valid;
    valid.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const bool ok = in.fhat_valid.empty() ? true : (in.fhat_valid[i] != 0);
        if (!ok) continue;
        if (!std::isfinite(in.fhat[i])) continue;
        valid.push_back(in.fhat[i]);
    }
    if (valid.size() < static_cast<std::size_t>(kNCommonMin)) {
        out.reject = "too_few_valid_stars";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    double s = 0.0;
    for (double v : valid) s += v;
    const double conc = (s / static_cast<double>(valid.size())) / in.a_nea;
    const double nscale = robust_scale_mad(valid.data(), valid.size());
    const double a_ref = (in.a_ref > 0.0) ? in.a_ref : in.a_nea;
    const double b = in.background_robust_mean * a_ref;

    out.s = s; out.conc = conc; out.n = nscale; out.b = b;
    const std::string& u = in.component_flux_unit;
    fill_summary(out.signal, "signal", "psfsw.signal", u, "psfsw.sum_psf_flux",
                 in.estimator_version, s, in.signal_samples);
    fill_summary(out.concentration, "concentration", "psfsw.concentration", u + "/px^2",
                 "psfsw.mean_flux_over_anea", in.estimator_version, conc,
                 in.concentration_samples);
    fill_summary(out.noise, "noise", "psfsw.noise", u, "psfsw.robust_scale_mad",
                 in.estimator_version, nscale, in.noise_samples);
    fill_summary(out.background, "background", "psfsw.background", u,
                 "psfsw.robust_mean_background", in.estimator_version, b,
                 in.background_samples);
    out.ok = true;
    return out;
}

/* ------------------------------------------------------------------------- */
/* 复合与组内归一                                                              */
/* ------------------------------------------------------------------------- */
bool composite_exponents_valid(const CompositeParams& p) {
    if (!(p.alpha >= 0.0 && p.beta >= 0.0 && p.gamma >= 0.0 && p.delta >= 0.0)) return false;
    const bool num_ok = (p.alpha > 0.0) || (p.beta > 0.0);
    const bool den_ok = (p.gamma > 0.0) || (p.delta > 0.0);
    return num_ok && den_ok;
}

std::vector<double> group_normalize_by_median(const std::vector<double>& wt, double* out_median) {
    const double med = median_of(wt);
    if (out_median) *out_median = med;
    std::vector<double> w(wt.size(), 0.0);
    if (!(med > 0.0)) return w;
    for (std::size_t i = 0; i < wt.size(); ++i) w[i] = wt[i] / med;
    return w;
}

namespace {

CompositeResult composite_impl(const std::vector<ComponentValues>& frames,
                               const CompositeParams& params, bool apply_normalization) {
    CompositeResult res;
    if (frames.empty()) {
        res.reject = "no_frames";
        res.reason = PsfswReason::insufficient_valid_stars;
        return res;
    }
    /* (1) fail-closed (幂运算前, 顺序固定) */
    for (const auto& f : frames) {
        if (!(f.b > 0.0) || !std::isfinite(f.b)) {
            res.reject = "background_nonpositive";
            res.reason = PsfswReason::background_nonpositive_undefined_transform;
            return res;
        }
    }
    for (const auto& f : frames) {
        if (!(f.s > 0.0) || !(f.conc > 0.0) || !(f.n > 0.0)) {
            res.reject = "degenerate_component";
            res.reason = PsfswReason::insufficient_valid_stars;
            return res;
        }
    }
    if (!(params.floor > 0.0)) {
        res.reject = "missing_component_floor";
        res.reason = PsfswReason::insufficient_valid_stars;
        return res;
    }
    /* (2) 分量下限 (仅在 fail-closed 通过后) */
    res.wt.resize(frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const double s = std::max(frames[i].s, params.floor);
        const double c = std::max(frames[i].conc, params.floor);
        const double nn = std::max(frames[i].n, params.floor);
        const double bb = std::max(frames[i].b, params.floor);
        const double num = params.c_norm * std::pow(s, params.alpha) * std::pow(c, params.beta);
        const double den = std::pow(nn, params.gamma) * std::pow(bb, params.delta);
        res.wt[i] = num / den;
    }
    /* (3)+(4) 组内中值归一 */
    if (apply_normalization) {
        res.w_psfsw = group_normalize_by_median(res.wt, &res.median_wt);
    } else {
        res.w_psfsw = res.wt;   /* 负向 mutation: 去掉组内归一 (测试用) */
        res.median_wt = median_of(res.wt);
    }
    res.median_w_psfsw = median_of(res.w_psfsw);
    res.ok = true;
    return res;
}

}  /* namespace */

CompositeResult compute_psfsw_weights(const std::vector<ComponentValues>& frames,
                                      const CompositeParams& params) {
    return composite_impl(frames, params, true);
}

double cnorm_invariance_deviation(const std::vector<ComponentValues>& frames, double c_norm) {
    CompositeParams p;
    p.c_norm = c_norm;
    const CompositeResult a = composite_impl(frames, p, true);
    p.c_norm = kCompositeCNorm;
    const CompositeResult b = composite_impl(frames, p, true);
    if (!a.ok || !b.ok || a.w_psfsw.size() != b.w_psfsw.size()) return 1e308;
    double dev = 0.0;
    for (std::size_t i = 0; i < a.w_psfsw.size(); ++i)
        dev = std::max(dev, std::fabs(a.w_psfsw[i] - b.w_psfsw[i]));
    return dev;
}

/* ------------------------------------------------------------------------- */
/* 共同星集                                                                    */
/* ------------------------------------------------------------------------- */
CommonStarValidation validate_common_star_set(const CommonStarSet& set, int n_frames) {
    CommonStarValidation out;
    if (n_frames <= 0) {
        out.reject = "invalid_frame_count";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    const SelectionFunction& sf = set.selection;
    if (set.common_star_set_id.empty() || sf.selection_function_id.empty() ||
        sf.reference_catalog_id.empty() || sf.reference_catalog_version_hash.empty() ||
        sf.epoch_pm_handling.empty() || sf.applied_at.empty() ||
        !(sf.mag_max > sf.mag_min) || !(sf.detection_threshold_sigma > 0.0) ||
        !(sf.matching_radius_arcsec > 0.0)) {
        out.reject = "incomplete_common_star_set";
        out.reason = PsfswReason::no_common_star_set;
        return out;
    }
    if (sf.independence_proof != "external_reference_catalog" &&
        sf.independence_proof != "reference_stack_single_threshold") {
        out.reject = "non_independent_common_star_set";
        out.reason = PsfswReason::selection_bias_gate_failed;
        return out;
    }
    std::vector<int> per_frame(static_cast<std::size_t>(n_frames), 0);
    int n_common = 0;
    for (const auto& m : set.members) {
        if (!m.valid_in_frame.empty() &&
            m.valid_in_frame.size() != static_cast<std::size_t>(n_frames)) {
            out.reject = "member_frame_size_mismatch";
            out.reason = PsfswReason::insufficient_valid_stars;
            return out;
        }
    }
    for (const auto& m : set.members) {
        const bool group_valid = !m.flags.any();
        if (group_valid) {
            ++n_common;
            out.valid_star_ids.push_back(m.star_id);
        }
        for (int f = 0; f < n_frames; ++f) {
            const bool frame_ok = m.valid_in_frame.empty() ? true : (m.valid_in_frame[f] != 0);
            if (group_valid && frame_ok) per_frame[f] += 1;
        }
    }
    out.n_common = n_common;
    out.per_frame_valid = per_frame;
    if (n_common < kNCommonMin || set.members.empty()) {
        out.reject = "n_common_below_min";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    for (int f = 0; f < n_frames; ++f) {
        if (per_frame[f] < kNCommonMin) {
            out.reject = "frame_valid_below_min";
            out.reason = PsfswReason::insufficient_valid_stars;
            return out;
        }
    }
    out.ok = true;
    return out;
}

/* ------------------------------------------------------------------------- */
/* depth 门                                                                    */
/* ------------------------------------------------------------------------- */
DepthGateResult depth_stability_gate(const std::vector<DepthScanPoint>& scans) {
    DepthGateResult out;
    out.k = static_cast<int>(scans.size());
    if (scans.empty()) {
        out.reject = "no_scan_points";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    if (out.k < kDepthMinScanPoints) {
        out.reject = "k_below_min";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    int min_n = scans[0].n_common, max_n = scans[0].n_common;
    for (const auto& s : scans) {
        if (s.n_common < kNCommonMin) {
            out.reject = "scan_point_n_common_below_min";
            out.reason = PsfswReason::insufficient_valid_stars;
            return out;
        }
        min_n = std::min(min_n, s.n_common);
        max_n = std::max(max_n, s.n_common);
    }
    double min_mag = scans[0].mag_limit, max_mag = scans[0].mag_limit;
    for (const auto& s : scans) {
        min_mag = std::min(min_mag, s.mag_limit);
        max_mag = std::max(max_mag, s.mag_limit);
    }
    out.span_mag = max_mag - min_mag;
    out.n_common_span_ok = (max_n >= 2 * min_n) && min_n > 0;
    if (out.span_mag < kDepthMinSpanMag && !out.n_common_span_ok) {
        out.reject = "depth_span_below_min";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    /* 长度一致性 */
    const std::size_t n_frames = scans[0].w_psfsw.size();
    if (n_frames == 0) {
        out.reject = "empty_weight_vector";
        out.reason = PsfswReason::insufficient_valid_stars;
        return out;
    }
    for (const auto& s : scans) {
        if (s.w_psfsw.size() != n_frames) {
            out.reject = "weight_vector_size_mismatch";
            out.reason = PsfswReason::insufficient_valid_stars;
            return out;
        }
    }
    double worst_dev = 0.0, worst_rho = 0.0;
    for (std::size_t f = 0; f < n_frames; ++f) {
        std::vector<double> series(scans.size(), 0.0), idx(scans.size(), 0.0);
        for (std::size_t t = 0; t < scans.size(); ++t) {
            series[t] = scans[t].w_psfsw[f];
            idx[t] = static_cast<double>(t);
        }
        const double med = median_of(series);
        if (!(med > 0.0)) {
            out.reject = "non_positive_median";
            out.reason = PsfswReason::insufficient_valid_stars;
            return out;
        }
        for (double v : series)
            worst_dev = std::max(worst_dev, std::fabs(v - med) / med);
        worst_rho = std::max(worst_rho, std::fabs(spearman_rho(idx, series)));
    }
    out.max_rel_dev = worst_dev;
    out.max_abs_rho = worst_rho;
    if (worst_dev > kDepthMaxRelDev) {
        out.reject = "depth_instability";
        out.reason = PsfswReason::selection_bias_gate_failed;
        return out;
    }
    if (worst_rho > kDepthMaxAbsRho) {
        out.reject = "depth_monotonic_drift";
        out.reason = PsfswReason::selection_bias_gate_failed;
        return out;
    }
    out.ok = true;
    return out;
}

/* ------------------------------------------------------------------------- */
/* 空间非均匀 / 标量降级门                                                     */
/* ------------------------------------------------------------------------- */
NonuniformityResult spatial_nonuniformity_gate(
    const std::vector<ComponentMeasure>& components, NCommonTier tier) {
    NonuniformityResult out;
    out.spread_limit = (tier == NCommonTier::low) ? kNonuniformityMaxLow : kNonuniformityMax;
    for (const auto& c : components) {
        double spread = c.spatial_spread_rel;
        if (!c.samples.empty()) {
            const double p05 = quantile_of(c.samples, 0.05);
            const double p95 = quantile_of(c.samples, 0.95);
            const double p50 = quantile_of(c.samples, 0.50);
            spread = (p50 > 0.0) ? (p95 - p05) / p50 : 0.0;
        } else {
            spread = (c.p50 > 0.0) ? (c.p95 - c.p05) / c.p50 : 0.0;
        }
        double trend = c.linear_trend_max_rel;
        if (c.samples.size() >= 2) {
            double fit_dev = 0.0;
            const double n = static_cast<double>(c.samples.size());
            double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
            for (std::size_t i = 0; i < c.samples.size(); ++i) {
                const double xi = static_cast<double>(i);
                sx += xi; sy += c.samples[i]; sxx += xi * xi; sxy += xi * c.samples[i];
            }
            const double denom = n * sxx - sx * sx;
            if (std::fabs(denom) > 0.0) {
                const double slope = (n * sxy - sx * sy) / denom;
                const double intercept = (sy - slope * sx) / n;
                const double mean_fit = sy / n;
                for (std::size_t i = 0; i < c.samples.size(); ++i) {
                    const double fit = slope * static_cast<double>(i) + intercept;
                    fit_dev = std::max(fit_dev, std::fabs(fit - mean_fit));
                }
            }
            const double p50 = quantile_of(c.samples, 0.50);
            trend = (p50 > 0.0) ? fit_dev / p50 : 0.0;
        }
        if (spread > out.spread_limit || trend > kSpatialTrendMax) {
            out.failing_component = &c;
            out.spread = spread;
            out.trend = trend;
            out.reject = (spread > out.spread_limit) ? "spatial_spread_exceeds_limit"
                                                     : "spatial_trend_exceeds_limit";
            out.reason = PsfswReason::spatial_nonuniformity_gate_failed;
            return out;
        }
        out.spread = std::max(out.spread, spread);
        out.trend = std::max(out.trend, trend);
    }
    out.ok = true;
    return out;
}

ScalarDegradationResult scalar_degradation_gate(double power_loss, double flux_bias) {
    ScalarDegradationResult out;
    if (!(power_loss <= kPowerLossMax) || !(flux_bias <= kFluxBiasMax)) {
        out.reject = (power_loss > kPowerLossMax) ? "power_loss_exceeds_limit"
                                                  : "flux_bias_exceeds_limit";
        out.reason = PsfswReason::spatial_nonuniformity_gate_failed;
        return out;
    }
    out.ok = true;
    return out;
}

WeightRangeGuard weight_dynamic_range_guard(const std::vector<double>& w_psfsw) {
    WeightRangeGuard g;
    if (w_psfsw.empty()) return g;
    double wmin = w_psfsw[0], wmax = w_psfsw[0], sum = 0.0, sum2 = 0.0;
    for (double w : w_psfsw) {
        wmin = std::min(wmin, w);
        wmax = std::max(wmax, w);
        sum += w;
        sum2 += w * w;
    }
    if (wmin > 0.0) g.range = wmax / wmin;
    if (sum > 0.0) g.concentration = sum2 / (sum * sum);
    g.exceeded = g.range > kWeightRangeMax;
    return g;
}

/* ------------------------------------------------------------------------- */
/* conventional coadd / covariance                                             */
/* ------------------------------------------------------------------------- */
CoaddResult conventional_coadd(const std::vector<std::vector<double>>& d,
                               const std::vector<std::vector<char>>& validity,
                               const std::vector<double>& w_psfsw) {
    CoaddResult out;
    const std::size_t k = d.size();
    if (k == 0 || validity.size() != k || w_psfsw.size() != k) {
        out.reject = "size_mismatch";
        return out;
    }
    const std::size_t npix = d[0].size();
    for (const auto& row : d)
        if (row.size() != npix) { out.reject = "ragged_input"; return out; }
    for (const auto& row : validity)
        if (row.size() != npix) { out.reject = "ragged_validity"; return out; }
    out.alpha.assign(k, std::vector<double>(npix, 0.0));
    out.i_out.assign(npix, 0.0);
    out.defined.assign(npix, 0);
    for (std::size_t p = 0; p < npix; ++p) {
        double denom = 0.0;
        for (std::size_t f = 0; f < k; ++f)
            if (validity[f][p] != 0) denom += w_psfsw[f];
        if (!(denom > 0.0)) continue;
        double acc = 0.0;
        for (std::size_t f = 0; f < k; ++f) {
            if (validity[f][p] == 0) continue;
            out.alpha[f][p] = w_psfsw[f] / denom;
            acc += out.alpha[f][p] * d[f][p];
        }
        out.i_out[p] = acc;
        out.defined[p] = 1;
    }
    out.ok = true;
    return out;
}

CovariancePropagation propagate_covariance(
    const std::vector<std::vector<double>>& alpha,
    const std::vector<double>& c_in) {
    CovariancePropagation out;
    const std::size_t k = alpha.size();
    if (k == 0) { out.reject = "no_frames"; return out; }
    if (c_in.size() != k * k) { out.reject = "covariance_size_mismatch"; return out; }
    const std::size_t npix = alpha[0].size();
    for (const auto& row : alpha)
        if (row.size() != npix) { out.reject = "ragged_alpha"; return out; }
    out.var_out.assign(npix, 0.0);
    for (std::size_t p = 0; p < npix; ++p) {
        double v = 0.0;
        for (std::size_t a = 0; a < k; ++a) {
            if (alpha[a][p] == 0.0) continue;
            for (std::size_t b = 0; b < k; ++b)
                v += alpha[a][p] * alpha[b][p] * c_in[a * k + b];
        }
        out.var_out[p] = v;
    }
    out.ok = true;
    return out;
}

/* ------------------------------------------------------------------------- */
/* 记录门                                                                      */
/* ------------------------------------------------------------------------- */
const std::vector<std::string>& forbidden_psfsw_product_keys() {
    static const std::vector<std::string> keys = {
        "ivar", "inverse_variance", "variance", "var", "sigma", "sigma2",
        "fisher", "fisher_information", "information", "w_info", "w_psf",
        /* 扩展守卫 (docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md §3 登记) */
        "snr", "snr2", "support", "coverage"};
    return keys;
}

const std::vector<std::string>& forbidden_weight_source_aliases() {
    static const std::vector<std::string> aliases = {
        "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
        "support", "support_area", "coverage", "coverage_area",
        "fwhm", "psf_fwhm", "median_fwhm", "source_fwhm",
        "residual", "psf_residual", "psf_fit_residual", "fit_residual",
        "psfsw_robust_weight", "psfsw"};
    return aliases;
}

const std::vector<std::string>& production_weight_modes() {
    static const std::vector<std::string> modes = {
        "point_information", "surface_gls", "psfsw_robust"};
    return modes;
}

bool is_production_weight_mode(const std::string& mode) {
    for (const auto& m : production_weight_modes())
        if (m == mode) return true;
    return false;
}

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string last_segment(const std::string& path) {
    const std::size_t pos = path.find_last_of('.');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool contains_any_token(const std::string& s, const std::vector<std::string>& toks) {
    const std::string ls = lower(s);
    for (const auto& t : toks)
        if (ls.find(lower(t)) != std::string::npos) return true;
    return false;
}

/* 诊断别名匹配: 精确等值 (非子串)。冻结别名里的 "psfsw" 不得误伤合法的
 * weight.sources=["psfsw.signal", ...] (02_WEIGHT_MODE_VOCABULARY §5)。 */
bool equals_any_token(const std::string& s, const std::vector<std::string>& toks) {
    const std::string ls = lower(s);
    for (const auto& t : toks)
        if (ls == lower(t)) return true;
    return false;
}

bool is_legacy_integer_mode(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

void add_finding(RecordValidation& v, const char* gate, const std::string& detail) {
    v.accept = false;
    v.findings.push_back(GateFinding{gate, detail});
}

/* G18/G19 通过 n_common 得到档位 */
NCommonTier tier_from_record(const PsfswRecord& rec) {
    return n_common_tier(rec.n_common);
}

}  /* namespace */

RecordValidation validate_psfsw_record(const PsfswRecord& rec) {
    RecordValidation v;

    /* G01 模式在生产列表且为 psfsw_robust */
    if (!is_production_weight_mode(rec.weight_mode)) {
        add_finding(v, "PSFSW-G01", "weight_mode not in production set: " + rec.weight_mode);
    } else if (rec.weight_mode != "psfsw_robust") {
        add_finding(v, "PSFSW-G01", "psfsw record must declare weight_mode=psfsw_robust");
    }

    /* G02 无量纲 + 禁 flux^-2/ivar 词 */
    const std::vector<std::string> bad_unit_tokens = {"flux^-2", "flux-2", "ivar",
                                                      "inverse_variance", "fisher"};
    const bool kind_ok = (rec.weight_kind == "relative_dimensionless" ||
                          rec.weight_kind == "psfsw_robust_weight");
    const bool units_ok = (rec.weight_units == "1" ||
                           rec.weight_units == "dimensionless_relative");
    if (!kind_ok || !units_ok ||
        contains_any_token(rec.weight_kind, bad_unit_tokens) ||
        contains_any_token(rec.weight_units, bad_unit_tokens)) {
        add_finding(v, "PSFSW-G02", "weight kind/units not relative_dimensionless / 1");
    }

    /* G03 组内归一 */
    if (!rec.group_normalized || rec.normalization_scope != "group" ||
        std::fabs(rec.normalization_median_target - kGroupMedianTarget) > kCompositeMedianRtol) {
        add_finding(v, "PSFSW-G03", "group_normalized/scope/median_target violates group normalization");
    }

    /* G04 四分量互异 */
    if (rec.components.size() != 4) {
        add_finding(v, "PSFSW-G04", "psfsw requires exactly 4 components");
    } else {
        std::set<std::string> ids, names;
        for (const auto& c : rec.components) {
            if (c.measurement_id.empty()) add_finding(v, "PSFSW-G04", "empty measurement_id");
            ids.insert(c.measurement_id);
            names.insert(c.name);
        }
        if (ids.size() != 4) add_finding(v, "PSFSW-G04", "measurement_id not distinct (collapse)");
        if (names.size() != 4) add_finding(v, "PSFSW-G04", "component names not distinct");
    }

    /* G05 禁止键 (任意层) */
    for (const auto& key : rec.produced_keys) {
        const std::string seg = lower(last_segment(key));
        for (const auto& bad : forbidden_psfsw_product_keys()) {
            if (seg == bad) {
                add_finding(v, "PSFSW-G05", "forbidden psfsw product key: " + key);
                break;
            }
        }
    }

    /* G06 covariance 来源 */
    if (rec.covariance_method != "propagated_from_composite_coefficients" ||
        rec.variance_from_weight || rec.uses_relative_weight_as_ivar ||
        rec.combination_coefficient_ids.empty()) {
        add_finding(v, "PSFSW-G06", "covariance not propagated from composite coefficients");
    }
    if (!rec.variance_from.empty() && contains_any_token(rec.variance_from, bad_unit_tokens)) {
        add_finding(v, "PSFSW-G06", "variance_from uses forbidden token: " + rec.variance_from);
    } else if (equals_any_token(rec.variance_from, forbidden_weight_source_aliases())) {
        add_finding(v, "PSFSW-G06", "variance_from is a diagnostic/relative-weight alias: " + rec.variance_from);
    }

    /* G07 effective PSF */
    if (rec.effective_psf_id.empty() || rec.effective_psf_only_fwhm ||
        !rec.effective_psf_normalization_declared) {
        add_finding(v, "PSFSW-G07", "effective PSF missing / fwhm-only / normalization undeclared");
    }

    /* G08 共同星集 + selection function */
    if (rec.common_star_set_id.empty() || rec.selection_function_id.empty()) {
        add_finding(v, "PSFSW-G08", "common_star_set_id / selection_function_id missing");
    }

    /* G09 独立性证明 */
    if (rec.independence_proof != "external_reference_catalog" &&
        rec.independence_proof != "reference_stack_single_threshold") {
        add_finding(v, "PSFSW-G09", "independence_proof not allowed: " + rec.independence_proof);
    }

    /* G10 n_common 下限 */
    if (rec.n_common < kNCommonMin) {
        add_finding(v, "PSFSW-G10", "n_common below hard minimum");
    }

    /* G11 深度稳定性门 (须已评估且过阈) */
    if (!rec.depth_gate_evaluated) {
        add_finding(v, "PSFSW-G11", "depth stability gate not evaluated");
    } else if (!rec.depth_gate_ok || rec.depth_k < kDepthMinScanPoints ||
               rec.depth_max_rel_dev > kDepthMaxRelDev ||
               rec.depth_max_abs_rho > kDepthMaxAbsRho) {
        add_finding(v, "PSFSW-G11", "depth stability gate failed");
    }

    /* G12/G13 validity */
    if (!rec.valid) {
        if (rec.has_weight_value) {
            add_finding(v, "PSFSW-G12", "valid=false but weight_value present");
        }
        if (!is_whitelisted_reason(rec.reason)) {
            add_finding(v, "PSFSW-G12", "valid=false reason not in whitelist");
        }
        if (equals_any_token(rec.weight_value_source, forbidden_weight_source_aliases())) {
            add_finding(v, "PSFSW-G12", "median SNR / diagnostic fallback in weight_value_source");
        }
    } else if (rec.reason != PsfswReason::none) {
        add_finding(v, "PSFSW-G13", "valid=true but failure reason present");
    }

    /* G14 版本化字段 */
    if (rec.composite.version != kCompositeVersion ||
        !composite_exponents_valid(rec.composite) ||
        !(rec.composite.floor > 0.0) ||
        rec.calibration_sample_id.empty() || rec.acceptance_sample_id.empty()) {
        add_finding(v, "PSFSW-G14", "composite version / exponents / floor / sample ids incomplete");
    }

    /* G15 组内 median=1 且全正 */
    if (rec.w_psfsw.empty()) {
        add_finding(v, "PSFSW-G15", "empty W_psfsw");
    } else {
        for (double w : rec.w_psfsw)
            if (!(w > 0.0)) { add_finding(v, "PSFSW-G15", "non-positive W_psfsw"); break; }
        const double med = median_of(rec.w_psfsw);
        if (std::fabs(med - kGroupMedianTarget) > kCompositeMedianRtol)
            add_finding(v, "PSFSW-G15", "group median(W_psfsw) != 1");
    }

    /* G17 分位与有效覆盖 */
    for (const auto& c : rec.components) {
        if (!(c.p05 <= c.p50 && c.p50 <= c.p95)) {
            add_finding(v, "PSFSW-G17", "p05<=p50<=p95 violated for " + c.name);
        }
        if (!(c.valid_area_fraction >= 0.0 && c.valid_area_fraction <= 1.0)) {
            add_finding(v, "PSFSW-G17", "valid_area_fraction out of [0,1] for " + c.name);
        }
    }

    /* G18 空间非均匀 / 趋势 (LOW 档收紧) */
    {
        const NonuniformityResult nu = spatial_nonuniformity_gate(rec.components, tier_from_record(rec));
        if (!nu.ok) add_finding(v, "PSFSW-G18", nu.reject != nullptr ? nu.reject : "nonuniformity");
    }

    /* G19 标量降级门 */
    {
        const ScalarDegradationResult sd = scalar_degradation_gate(rec.power_loss, rec.flux_bias);
        if (!sd.ok) add_finding(v, "PSFSW-G19", sd.reject != nullptr ? sd.reject : "degradation");
    }

    /* G20 生产模式列表不含 psf_snr_power */
    for (const auto& m : rec.production_modes) {
        if (m == "psf_snr_power" || m == "auto" || m == "support_x_snr2")
            add_finding(v, "PSFSW-G20", "deferred/forbidden mode in production list: " + m);
    }

    /* G21 legacy 整数模式 (尤其 0) */
    if (is_legacy_integer_mode(rec.weight_mode))
        add_finding(v, "PSFSW-G21", "legacy integer weight_mode: " + rec.weight_mode);
    for (const auto& m : rec.production_modes)
        if (is_legacy_integer_mode(m))
            add_finding(v, "PSFSW-G21", "legacy integer in production list: " + m);

    /* G22 诊断别名不得进权重来源/方差来源 */
    if (equals_any_token(rec.weight_value_source, forbidden_weight_source_aliases()))
        add_finding(v, "PSFSW-G22", "diagnostic alias in weight_value_source");
    for (const auto& s : rec.weight_sources)
        if (equals_any_token(s, forbidden_weight_source_aliases()))
            add_finding(v, "PSFSW-G22", "diagnostic alias in weight.sources: " + s);
    if (equals_any_token(rec.variance_from, forbidden_weight_source_aliases()))
        add_finding(v, "PSFSW-G22", "diagnostic alias in covariance.variance_from");

    /* G23 分量单位一致性: S/N/B 同单位; Conc 单位 = flux_unit/px^2 */
    if (rec.component_flux_unit.empty()) {
        add_finding(v, "PSFSW-G23", "component_flux_unit undeclared");
    } else if (rec.components.size() == 4) {
        for (const auto& c : rec.components) {
            if (c.name == "concentration") {
                if (c.unit != rec.component_flux_unit + "/px^2")
                    add_finding(v, "PSFSW-G23", "concentration unit != component_flux_unit/px^2");
            } else if (c.unit != rec.component_flux_unit) {
                add_finding(v, "PSFSW-G23", "component unit inconsistent for " + c.name);
            }
        }
    }

    /* G24 标定样本 != 验收样本 */
    if (!rec.calibration_sample_id.empty() &&
        rec.calibration_sample_id == rec.acceptance_sample_id) {
        add_finding(v, "PSFSW-G24", "calibration_sample_id == acceptance_sample_id");
    }

    /* G25 基线声明 */
    if (!rec.baseline_claim.empty()) {
        const bool claim_ok = (rec.baseline_claim == "better_than" ||
                               rec.baseline_claim == "non_inferior_to");
        const bool evidence_ok = (rec.baseline_bootstrap_resamples >= 200 &&
                                  rec.baseline_confidence >= 0.95);
        const std::vector<std::string> banned = {"fisher_optimal", "equals_ivar",
                                                 "equivalent_to_w_info", "looks_better"};
        if (!claim_ok || !evidence_ok || contains_any_token(rec.baseline_claim, banned)) {
            add_finding(v, "PSFSW-G25", "baseline claim not better_than/non_inferior_to with CI");
        }
    }

    return v;
}

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */
