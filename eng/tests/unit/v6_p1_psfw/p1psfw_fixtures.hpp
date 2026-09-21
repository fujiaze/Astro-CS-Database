/* p1psfw_fixtures.hpp - 合成 PSF / 帧 / 合法记录构造 (不内嵌生产算法) */
#ifndef P1PSFW_FIXTURES_HPP
#define P1PSFW_FIXTURES_HPP

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/psf_information.h"
#include "astrocs/v6/psfsw.h"

namespace astrocs {
namespace v6 {
namespace p1psfw {
namespace fixture {

inline std::vector<double> gaussian_psf(std::size_t n, double sigma) {
    std::vector<double> p(n, 0.0);
    const double cx = static_cast<double>(n - 1) / 2.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = static_cast<double>(i) - cx;
        p[i] = std::exp(-0.5 * x * x / (sigma * sigma));
        sum += p[i];
    }
    for (double& v : p) v /= sum;
    return p;
}

inline std::vector<double> gaussian_psf_2d(std::size_t n, double sigma) {
    std::vector<double> p(n * n, 0.0);
    const double c = static_cast<double>(n - 1) / 2.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            const double dx = static_cast<double>(i) - c;
            const double dy = static_cast<double>(j) - c;
            p[i * n + j] = std::exp(-0.5 * (dx * dx + dy * dy) / (sigma * sigma));
            sum += p[i * n + j];
        }
    for (double& v : p) v /= sum;
    return p;
}

inline double quantile_shim(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const double pos = p * static_cast<double>(v.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(pos));
    if (lo == hi) return v[lo];
    const double f = pos - static_cast<double>(lo);
    return v[lo] * (1.0 - f) + v[hi] * f;
}

inline ComponentMeasure component(const std::string& name, const std::string& unit,
                                  double value, const std::vector<double>& samples = {}) {
    ComponentMeasure c;
    c.name = name;
    c.measurement_id = "psfsw." + name;
    c.unit = unit;
    c.estimator = "test";
    c.estimator_version = "v1";
    c.value = value;
    if (samples.empty()) {
        c.p05 = c.p50 = c.p95 = value;
    } else {
        c.p05 = quantile_shim(samples, 0.05);
        c.p50 = quantile_shim(samples, 0.50);
        c.p95 = quantile_shim(samples, 0.95);
        c.samples = samples;
    }
    c.valid_area_fraction = 1.0;
    return c;
}

inline std::vector<ComponentMeasure> four_components(double s, double conc, double n, double b) {
    return {component("signal", "ADU", s),
            component("concentration", "ADU/px^2", conc),
            component("noise", "ADU", n),
            component("background", "ADU", b)};
}

/* 合法 psfsw 记录 (正向控制): 所有 G01..G25 通过 */
inline PsfswRecord good_record() {
    PsfswRecord r;
    r.weight_mode = "psfsw_robust";
    r.production_modes = production_weight_modes();
    r.weight_kind = "relative_dimensionless";
    r.weight_units = "1";
    r.group_normalized = true;
    r.normalization_scope = "group";
    r.normalization_median_target = kGroupMedianTarget;
    r.component_flux_unit = "ADU";
    r.common_star_set_id = "css-001";
    r.selection_function_id = "sel-001";
    r.independence_proof = "external_reference_catalog";
    r.n_common = 30;
    r.depth_gate_evaluated = true;
    r.depth_gate_ok = true;
    r.depth_max_rel_dev = 0.01;
    r.depth_max_abs_rho = 0.1;
    r.depth_k = 5;
    r.composite = CompositeParams();
    r.components = four_components(100.0, 4.0, 2.0, 200.0);
    r.w_psfsw = {0.5, 1.0, 2.0};   /* median = 1, all positive */
    r.valid = true;
    r.has_weight_value = false;
    r.reason = PsfswReason::none;
    r.covariance_method = "propagated_from_composite_coefficients";
    r.variance_from_weight = false;
    r.uses_relative_weight_as_ivar = false;
    r.combination_coefficient_ids = {"cc-0", "cc-1"};
    r.effective_psf_id = "effpsf-001";
    r.effective_psf_only_fwhm = false;
    r.effective_psf_normalization_declared = true;
    r.power_loss = 0.01;
    r.flux_bias = 0.001;
    r.weight_sources = {"psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"};
    r.produced_keys = {"psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background",
                       "psfsw.w_psfsw", "psfsw.covariance"};
    r.calibration_sample_id = "calib-001";
    r.acceptance_sample_id = "accept-001";
    return r;
}

}  /* namespace fixture */
}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif /* P1PSFW_FIXTURES_HPP */
