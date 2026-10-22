// The public response entry point declared in include/acsd/contracts.hpp.
//
//   Result<Ref<SpectralPrediction>> integrate_response(Context&, Ref<Response>,
//                                                      Ref<SpectralBasis>,
//                                                      const NumericBudget&)
//
// Scientific contract: docs/science/foundation/GAIA_RESPONSE.md §1-§2 and
// docs/engineering/rebuild/SYSTEM.md N3.
//
//   q_ks = INT f_s R_k lambda_nm 1e-9/(h c) dlambda_nm      [s^-1 m^-2]
//   v_kl = INT b_l R_k lambda_nm 1e-9/(h c) dlambda_nm      (photon convention)
//   v_kl = INT b_l R_k dlambda_nm                          (energy convention)
//
// The integration is segmented at every filter/QE breakpoint, at both top-hat
// edges and at every knot of the spectral representation, so that no panel
// ever spans a step of the response.  Each panel uses the adaptive
// Gauss-Kronrod rule of src/numerics/quadrature.hpp with its error estimate.
//
// Two conventions that the declaration contract does not fix explicitly are
// documented here because the code has to pick one:
//   * an unset support interval is the all zero interval {0,0}; such an
//     interval is ignored and the natural domain of the payload is used;
//   * a response whose provenance contains the token "zero_tail" is known to
//     vanish outside its sampled wavelength range; every other response has an
//     unknown tail there.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/quadrature.hpp"
#include "response/spectral.hpp"

namespace acsd {
namespace {

using response::BasisData;
using response::ResponseData;

constexpr const char* kOrigin = "integrate_response";

// The order of the Gauss panel of the shared quadrature kernel.  It is exact
// for the polynomial integrand of one segment (the spectral basis times the
// piecewise linear response times the linear photon weight), so the reported
// error is the floating point level of the evaluation.
constexpr int kPanelOrder = 32;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array, const std::string& what) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " array is missing",
                                             kOrigin, "the basis integrals cannot be computed");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model,
                                             "the " + what + " array is not binary64", kOrigin,
                                             "the basis integrals cannot be computed");
  }
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) {
      total = 0;
      break;
    }
    if (total > (~0ull) / extent) {
      return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " shape overflows",
                                               kOrigin, "the basis integrals cannot be computed");
    }
    total *= extent;
  }
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, total});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the " + what + " array cannot be read",
                                             kOrigin, "the basis integrals cannot be computed");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

// Copies a binary64 vector into storage as an immutable array.
Result<ArrayRef> copy_f64(Context& context, const std::vector<double>& values,
                          const std::vector<U64>& shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

bool interval_is_set(const Interval& value) {
  return std::isfinite(value.lower) && std::isfinite(value.upper) && value.lower <= value.upper &&
         !(value.lower == 0.0 && value.upper == 0.0);
}

// ---------------------------------------------------------------------------
// Bounded cache of the response/basis integrals.
//
// The key carries the response and basis content hashes, the integration
// tolerances and the arithmetic identity, so that only an identical request
// can ever reuse a stored result (EXECUTION: a cache may only reuse a proof
// that carries the input hash, domain, algorithm and condition).
// ---------------------------------------------------------------------------

struct CacheKey {
  std::string response_hash;
  std::string basis_hash;
  int convention = 0;
  double absolute_tolerance = 0.0;
  double relative_tolerance = 0.0;
  U32 initial_bits = 0;
  U32 max_bits = 0;

  bool operator==(const CacheKey& other) const {
    return response_hash == other.response_hash && basis_hash == other.basis_hash &&
           convention == other.convention &&
           absolute_tolerance == other.absolute_tolerance &&
           relative_tolerance == other.relative_tolerance && initial_bits == other.initial_bits &&
           max_bits == other.max_bits;
  }
};

struct CacheKeyHash {
  std::size_t operator()(const CacheKey& key) const {
    std::size_t seed = std::hash<std::string>{}(key.response_hash);
    seed ^= std::hash<std::string>{}(key.basis_hash) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::size_t>(key.convention) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    const auto mix = [&seed](double value) {
      std::uint64_t bits = 0;
      static_assert(sizeof(bits) == sizeof(value));
      __builtin_memcpy(&bits, &value, sizeof(bits));
      seed ^= static_cast<std::size_t>(bits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };
    mix(key.absolute_tolerance);
    mix(key.relative_tolerance);
    seed ^= static_cast<std::size_t>(key.initial_bits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= static_cast<std::size_t>(key.max_bits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};

struct CacheEntry {
  std::vector<double> integrals;
  std::vector<double> errors;
  Interval support{0.0, 0.0};
  bool truncated = false;
  bool converged = true;
  int panels = 0;
};

class IntegralCache {
 public:
  static IntegralCache& instance() {
    static IntegralCache cache;
    return cache;
  }

  bool find(const CacheKey& key, CacheEntry& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
      return false;
    }
    out = it->second;
    return true;
  }

  void store(const CacheKey& key, const CacheEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.size() >= kMaxEntries) {
      entries_.clear();
    }
    entries_[key] = entry;
  }

 private:
  static constexpr std::size_t kMaxEntries = 16;
  mutable std::mutex mutex_;
  std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> entries_;
};

// ---------------------------------------------------------------------------
// Payload validation
// ---------------------------------------------------------------------------

Result<BasisData> load_basis(Context& context, Ref<SpectralBasis> basis) {
  Result<std::vector<double>> knots = read_f64_array(context, basis->knots_nm, "knots");
  if (!ok(knots)) {
    return model::fail<BasisData>(knots.code, "the basis knots cannot be read", kOrigin,
                                   "the basis integrals cannot be computed");
  }
  BasisData data;
  data.knots = val(knots);
  if (data.knots.size() < 2) {
    return model::fail<BasisData>(Code::invalid_model, "the basis needs at least one segment",
                                   kOrigin, "the basis integrals cannot be computed");
  }
  for (std::size_t i = 1; i < data.knots.size(); ++i) {
    if (!(data.knots[i] > data.knots[i - 1])) {
      return model::fail<BasisData>(Code::invalid_model, "the basis knots are not strictly increasing",
                                     kOrigin, "the basis integrals cannot be computed");
    }
  }
  data.segments = data.knots.size() - 1;
  data.functions = basis->coefficient_count;
  data.degree = basis->degree;

  Result<std::vector<double>> coefficients =
      read_f64_array(context, basis->basis_coefficients, "basis coefficients");
  if (!ok(coefficients)) {
    return model::fail<BasisData>(coefficients.code, "the basis coefficients cannot be read", kOrigin,
                                   "the basis integrals cannot be computed");
  }
  data.coefficients = val(coefficients);
  const std::size_t expected = data.functions * data.segments * (data.degree + 1);
  if (data.functions == 0 || data.coefficients.size() != expected) {
    return model::fail<BasisData>(
        Code::invalid_model,
        "the basis coefficient payload does not match coefficient_count x segments x (degree+1)",
        kOrigin, "the basis integrals cannot be computed");
  }
  for (double value : data.coefficients) {
    if (!std::isfinite(value)) {
      return model::fail<BasisData>(Code::invalid_model, "the basis coefficients are not finite",
                                     kOrigin, "the basis integrals cannot be computed");
    }
  }
  return model::ok_result(std::move(data));
}

Result<ResponseData> load_response(Context& context, Ref<Response> response) {
  ResponseData data;
  data.top_hat = response->top_hat_center_width_nm.has_value();
  if (data.top_hat) {
    const Vec2 top_hat = *response->top_hat_center_width_nm;
    data.top_hat_center_nm = top_hat[0];
    data.top_hat_width_nm = top_hat[1];
    if (!std::isfinite(top_hat[0]) || !std::isfinite(top_hat[1]) || !(top_hat[1] > 0.0)) {
      return model::fail<ResponseData>(Code::invalid_model,
                                        "the top hat needs a finite center and a positive width",
                                        kOrigin, "the basis integrals cannot be computed");
    }
    data.band_lower_nm = top_hat[0] - 0.5 * top_hat[1];
    data.band_upper_nm = top_hat[0] + 0.5 * top_hat[1];
    data.known_lower_nm = data.band_lower_nm;
    data.known_upper_nm = data.band_upper_nm;
  } else {
    Result<std::vector<double>> wavelength = read_f64_array(context, response->wavelength_nm, "wavelength");
    if (!ok(wavelength)) {
      return model::fail<ResponseData>(wavelength.code, "the response wavelengths cannot be read",
                                        kOrigin, "the basis integrals cannot be computed");
    }
    Result<std::vector<double>> throughput = read_f64_array(context, response->throughput, "throughput");
    if (!ok(throughput)) {
      return model::fail<ResponseData>(throughput.code, "the response throughput cannot be read",
                                        kOrigin, "the basis integrals cannot be computed");
    }
    data.wavelength_nm = val(wavelength);
    data.throughput = val(throughput);
    if (data.wavelength_nm.empty() || data.wavelength_nm.size() != data.throughput.size()) {
      return model::fail<ResponseData>(Code::invalid_model,
                                        "the response wavelength and throughput arrays disagree",
                                        kOrigin, "the basis integrals cannot be computed");
    }
    for (std::size_t i = 0; i < data.wavelength_nm.size(); ++i) {
      if (!std::isfinite(data.wavelength_nm[i]) || !std::isfinite(data.throughput[i])) {
        return model::fail<ResponseData>(Code::invalid_model, "the response payload is not finite",
                                          kOrigin, "the basis integrals cannot be computed");
      }
      if (data.throughput[i] < 0.0) {
        return model::fail<ResponseData>(Code::invalid_model, "the response throughput is negative",
                                          kOrigin, "the basis integrals cannot be computed");
      }
      if (i > 0 && !(data.wavelength_nm[i] > data.wavelength_nm[i - 1])) {
        return model::fail<ResponseData>(Code::invalid_model,
                                          "the response wavelengths are not strictly increasing",
                                          kOrigin, "the basis integrals cannot be computed");
      }
    }
    data.known_lower_nm = data.wavelength_nm.front();
    data.known_upper_nm = data.wavelength_nm.back();
    data.band_lower_nm = data.known_lower_nm;
    data.band_upper_nm = data.known_upper_nm;
  }

  data.zero_tail = response::provenance_declares_zero_tail(response->provenance);
  if (interval_is_set(response->support_nm)) {
    data.known_lower_nm = std::max(data.known_lower_nm, response->support_nm.lower);
    data.known_upper_nm = std::min(data.known_upper_nm, response->support_nm.upper);
  }
  return model::ok_result(std::move(data));
}

// ---------------------------------------------------------------------------
// Integration
// ---------------------------------------------------------------------------

struct IntegrationPlan {
  double lower_nm = 0.0;
  double upper_nm = 0.0;
  std::vector<double> breakpoints;  // strictly increasing, inside [lower, upper]
  bool truncated = false;           // the response is unknown outside the integrated range
};

IntegrationPlan plan_integration(const BasisData& basis, const ResponseData& response,
                                 const std::vector<double>& extra_breakpoints) {
  IntegrationPlan plan;
  const double basis_lower = basis.knots.front();
  const double basis_upper = basis.knots.back();

  // The response is known to vanish outside its band when it is a top hat or
  // when the provenance declares a zero tail; otherwise the tail is unknown.
  const bool zero_outside = response.top_hat || response.zero_tail;
  plan.truncated = false;
  double lower = basis_lower;
  double upper = basis_upper;
  if (!zero_outside) {
    lower = std::max(basis_lower, response.known_lower_nm);
    upper = std::min(basis_upper, response.known_upper_nm);
    if (lower > basis_lower || upper < basis_upper) {
      plan.truncated = true;
    }
  } else {
    // Outside the band the integrand is exactly zero; the integration range
    // is nevertheless clipped to the band so that no panel spans the step.
    lower = std::max(basis_lower, response.band_lower_nm);
    upper = std::min(basis_upper, response.band_upper_nm);
  }
  plan.lower_nm = lower;
  plan.upper_nm = upper;

  std::vector<double> points;
  points.reserve(basis.knots.size() + response.wavelength_nm.size() + extra_breakpoints.size() + 4);
  points.push_back(lower);
  points.push_back(upper);
  for (double knot : basis.knots) {
    points.push_back(knot);
  }
  for (double wavelength : response.wavelength_nm) {
    points.push_back(wavelength);
  }
  for (double extra : extra_breakpoints) {
    points.push_back(extra);
  }
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  for (double point : points) {
    if (point > lower && point < upper) {
      plan.breakpoints.push_back(point);
    }
  }
  return plan;
}

}  // namespace

Result<Ref<SpectralPrediction>> integrate_response(Context& context, Ref<Response> response,
                                                   Ref<SpectralBasis> basis,
                                                   const NumericBudget& request) {
  if (!response || !basis) {
    return model::fail<Ref<SpectralPrediction>>(Code::invalid_request,
                                                "a response and a spectral basis are required",
                                                kOrigin, "the basis integrals cannot be computed");
  }
  if (context.cancellation.requested()) {
    return model::fail<Ref<SpectralPrediction>>(Code::cancelled, "the call was cancelled", kOrigin,
                                                "the basis integrals cannot be computed");
  }

  Result<BasisData> basis_result = load_basis(context, basis);
  if (!ok(basis_result)) {
    return model::fail<Ref<SpectralPrediction>>(basis_result.code, "the spectral basis is invalid",
                                                kOrigin, "the basis integrals cannot be computed");
  }
  Result<ResponseData> response_result = load_response(context, response);
  if (!ok(response_result)) {
    return model::fail<Ref<SpectralPrediction>>(response_result.code, "the response is invalid", kOrigin,
                                                "the basis integrals cannot be computed");
  }
  const BasisData basis_data = val(basis_result);
  const ResponseData response_data = val(response_result);

  const double relative_tolerance =
      request.tolerance.relative > 0.0 ? request.tolerance.relative : 1e-12;
  const double absolute_tolerance = request.tolerance.absolute > 0.0 ? request.tolerance.absolute : 0.0;

  CacheKey key;
  key.response_hash = response->identity.content_hash;
  key.basis_hash = basis->identity.content_hash;
  key.convention = static_cast<int>(response->convention);
  key.absolute_tolerance = absolute_tolerance;
  key.relative_tolerance = relative_tolerance;
  key.initial_bits = request.arithmetic.initial_bits;
  key.max_bits = request.arithmetic.max_bits;

  CacheEntry entry;
  bool from_cache = IntegralCache::instance().find(key, entry);

  IntegrationPlan plan = plan_integration(basis_data, response_data, response->breakpoints_nm);
  if (!from_cache) {
    entry.integrals.assign(basis_data.functions, 0.0);
    entry.errors.assign(basis_data.functions, 0.0);
    entry.support = Interval{plan.lower_nm, plan.upper_nm};
    entry.truncated = plan.truncated;
    entry.converged = true;
    entry.panels = 0;

    std::vector<double> absolute_sums(basis_data.functions, 0.0);
    std::vector<std::size_t> term_counts(basis_data.functions, 0);

    const bool photon = response->convention == Response::Convention::photon_throughput;
    const int max_panels =
        request.max_iterations > 0 ? static_cast<int>(std::min<std::uint32_t>(request.max_iterations, 400u))
                                   : 200;

    if (plan.upper_nm > plan.lower_nm) {
      std::vector<double> limits;
      limits.push_back(plan.lower_nm);
      limits.insert(limits.end(), plan.breakpoints.begin(), plan.breakpoints.end());
      limits.push_back(plan.upper_nm);
      for (std::size_t segment = 0; segment + 1 < limits.size(); ++segment) {
        const double a = limits[segment];
        const double b = limits[segment + 1];
        if (!(b > a)) {
          continue;
        }
        for (std::size_t function = 0; function < basis_data.functions; ++function) {
          const auto integrand = [&](double lambda) -> double {
            const double basis_part = response::basis_value(basis_data, function, lambda);
            if (basis_part == 0.0) {
              return 0.0;
            }
            const double response_part = response::response_value(response_data, lambda);
            if (response_part == 0.0) {
              return 0.0;
            }
            const double weight = photon ? response::photon_weight(lambda) : 1.0;
            return basis_part * response_part * weight;
          };
          // The shared base kernel: a high order Gauss panel with the
          // difference of the half order rule as its error estimate.
          numerics::QuadratureResult panel = numerics::adaptive_integrate_gauss(
              integrand, a, b, kPanelOrder, absolute_tolerance, relative_tolerance, max_panels);
          entry.integrals[function] += panel.value;
          entry.errors[function] += panel.absolute_error;
          absolute_sums[function] += std::fabs(panel.value);
          ++term_counts[function];
          entry.panels += panel.panels;
          if (!panel.converged) {
            entry.converged = false;
          }
          if (context.cancellation.requested()) {
            return model::fail<Ref<SpectralPrediction>>(Code::cancelled, "the call was cancelled",
                                                        kOrigin, "the basis integrals are incomplete");
          }
        }
        const U64 consumed = context.resources.consume_work(1).code == Code::ok ? 1u : 0u;
        (void)consumed;
      }
    }

    // Floating point summation bound: |fl(sum) - sum| <= gamma_n * sum |x_i| with
    // gamma_n = n u / (1 - n u).  The Gauss-Kronrod difference only bounds the
    // truncation of the rule, not the rounding of the accumulation, so both are
    // reported.
    const double unit_roundoff = std::ldexp(1.0, -53);
    for (std::size_t function = 0; function < basis_data.functions; ++function) {
      const double n = static_cast<double>(term_counts[function]);
      if (n > 0.0) {
        const double gamma = n * unit_roundoff / (1.0 - n * unit_roundoff);
        entry.errors[function] += gamma * absolute_sums[function];
      }
    }
    IntegralCache::instance().store(key, entry);
  }

  // ---------------------------------------------------------------------
  // Output
  // ---------------------------------------------------------------------
  const std::string scale_name =
      response->convention == Response::Convention::photon_throughput
          ? "photon_rate_m2:response/" + response->identity.content_hash
          : "watt_m2:response/" + response->identity.content_hash;
  Unit integral_unit;
  integral_unit.scale = scale_name;
  integral_unit.sr_power = 0;
  integral_unit.pixel_power = 0;
  integral_unit.nm_power =
      response->convention == Response::Convention::photon_throughput ? 0 : -1;
  integral_unit.multiplier = 1.0;
  integral_unit.linear_power = 1;

  Index parameter_index =
      model::make_parameter_index({basis->identity.content_hash, response->identity.content_hash});
  const Id index_id = parameter_index.identity.object_id;

  Result<ArrayRef> integrals =
      copy_f64(context, entry.integrals, {static_cast<U64>(entry.integrals.size())}, integral_unit,
               index_id);
  if (!ok(integrals)) {
    return model::fail<Ref<SpectralPrediction>>(integrals.code, "the basis integrals cannot be stored",
                                                kOrigin, "the prediction cannot be published");
  }
  Result<ArrayRef> errors =
      copy_f64(context, entry.errors, {static_cast<U64>(entry.errors.size())}, integral_unit, index_id);
  if (!ok(errors)) {
    return model::fail<Ref<SpectralPrediction>>(errors.code, "the integration errors cannot be stored",
                                                kOrigin, "the prediction cannot be published");
  }

  model::Sha256 hasher;
  hasher.field("response", response->identity.content_hash);
  hasher.field("basis", basis->identity.content_hash);
  hasher.field("support_lower", entry.support.lower);
  hasher.field_f64("support_upper", entry.support.upper);
  hasher.field("truncated", entry.truncated ? 1 : 0);
  for (double value : entry.integrals) {
    hasher.update_f64(value);
  }
  for (double value : entry.errors) {
    hasher.update_f64(value);
  }

  auto prediction = std::make_shared<SpectralPrediction>();
  prediction->basis_integrals = val(integrals);
  prediction->absolute_errors = val(errors);
  prediction->basis = basis;
  prediction->response = response;
  prediction->support_nm = entry.support;
  if (response->shape_noise) {
    for (const Ref<NoiseRoot>& root : response->shape_noise->shared_factors) {
      if (root) {
        prediction->shared_spectral_error.push_back(root->shared_id);
      }
    }
  }
  prediction->identity = model::make_identity("acsd.SpectralPrediction", 2, 0,
                                              model::content_id(hasher.hex()), "acsd");
  prediction->assessment = model::make_assessment(Code::ok);
  prediction->assessment.complete = true;
  prediction->assessment.execution = ExecutionState::complete;
  // The Gauss-Kronrod panel difference is an approximation of the quadrature
  // error, not an interval certificate (SYSTEM N3).
  prediction->assessment.validity = Validity::approximate;
  prediction->assessment.science = ScienceState::bounded;
  prediction->assessment.condition = model::make_condition("basis_integrals", "response_integration");
  model::set_included(prediction->assessment, "segmented_gauss_kronrod_panels");
  model::set_included(prediction->assessment, "floating_point_summation_bound");
  model::set_omitted(prediction->assessment, "xp_resolution_model_bias");
  model::set_omitted(prediction->assessment, "response_shape_model_error");

  if (entry.truncated) {
    // The response is not known outside the integrated range: the returned
    // integrals are a lower bound of the full integral and no complete q may
    // be claimed.
    prediction->assessment.complete = false;
    prediction->assessment.science = ScienceState::conditional;
    model::add_issue(prediction->assessment, Code::unavailable,
                     "the response is unknown outside the integrated spectral range, so the "
                     "basis integrals do not describe the complete photon rate",
                     kOrigin,
                     "the photon rate is conditional on a non negative spectral upper bound that is "
                     "not available",
                     "supply a response whose provenance declares a zero tail or a spectral upper "
                     "bound");
  }
  if (!entry.converged) {
    prediction->assessment.complete = false;
    prediction->assessment.science = ScienceState::unconverged;
    model::add_issue(prediction->assessment, Code::nonconverged,
                     "the quadrature budget was exhausted before the panel error met the requested "
                     "tolerance",
                     kOrigin, "the reported absolute errors are the achieved panel estimates",
                     "increase the budget or the tolerance");
  }
  return model::ok_result(std::shared_ptr<const SpectralPrediction>(prediction));
}

}  // namespace acsd
