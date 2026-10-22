// The public covariance entry points declared in include/acsd/contracts.hpp.
#include <algorithm>
#include <cmath>

#include "acsd/contracts.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd {

Result<Ref<CovarianceAction>> make_covariance(Context& context, Ref<NoiseModel> model) {
  if (!model) {
    return model::fail<Ref<CovarianceAction>>(Code::invalid_request, "no noise model supplied",
                                              "make_covariance", "the covariance cannot be built");
  }
  const std::size_t root_count = model->innovations.size() + model->shared_factors.size();
  if (root_count != model->influence.size()) {
    return model::fail<Ref<CovarianceAction>>(
        Code::invalid_model, "the influence count does not match the root count",
        "make_covariance", "the covariance cannot be built");
  }
  for (const Ref<NoiseRoot>& root : model->innovations) {
    if (!root || !root->factor) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model, "a noise root is incomplete",
                                                "make_covariance",
                                                "the covariance cannot be built");
    }
  }
  for (const Ref<NoiseRoot>& root : model->shared_factors) {
    if (!root || !root->factor) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model, "a shared root is incomplete",
                                                "make_covariance",
                                                "the covariance cannot be built");
    }
  }
  for (const Ref<LinearAction>& influence : model->influence) {
    if (!influence) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model, "an influence is missing",
                                                "make_covariance",
                                                "the covariance cannot be built");
    }
  }
  // Shared roots with the same identity must carry the same definition.
  for (std::size_t i = 0; i < model->shared_factors.size(); ++i) {
    for (std::size_t j = i + 1; j < model->shared_factors.size(); ++j) {
      const SharedId& a = model->shared_factors[i]->shared_id;
      const SharedId& b = model->shared_factors[j]->shared_id;
      const bool same = a.external_namespace == b.external_namespace && a.entity == b.entity &&
                        a.model_revision == b.model_revision && a.component == b.component;
      if (same && model->shared_factors[i]->unit.multiplier != model->shared_factors[j]->unit.multiplier) {
        return model::fail<Ref<CovarianceAction>>(
            Code::identity_conflict, "a shared root is defined twice with different units",
            "make_covariance", "the covariance cannot be built");
      }
    }
  }

  auto covariance = std::make_shared<CovarianceAction>();
  covariance->model = model;
  DomainUnits spaces;
  if (!model->innovations.empty() && model->innovations.front()) {
    spaces.domain = model->innovations.front()->coordinates;
  } else if (!model->shared_factors.empty() && model->shared_factors.front()) {
    spaces.domain = model->shared_factors.front()->coordinates;
  }
  spaces.codomain = spaces.domain;
  spaces.input = model->variance_unit;
  spaces.output = model->variance_unit;
  covariance->input_output = spaces;
  covariance->identity = model::make_identity("acsd.CovarianceAction", 2, 0,
                                              model::hash_index(Index::Kind::latent, root_count, 0, {}),
                                              "acsd");
  covariance->assessment = model::make_assessment(Code::ok);
  covariance->assessment.complete = true;
  covariance->assessment.noise = model->state;
  if (!model->unknown_terms.empty()) {
    // A non-zero influence of an unknown term makes the complete variance
    // unknown; the known part is still available for diagnostics.
    bool material = false;
    for (const UnknownTerm& term : model->unknown_terms) {
      if (term.influence) material = true;
    }
    if (material) {
      model::add_issue(covariance->assessment, Code::unavailable,
                       "an unknown model term influences the covariance",
                       "make_covariance", "the complete variance is unknown");
      covariance->assessment.noise = NoiseState::unknown;
    }
  }
  (void)context;
  return model::ok_result(std::shared_ptr<const CovarianceAction>(covariance));
}

Result<ArrayRef> covariance_apply(Context& context, Ref<CovarianceAction> covariance,
                                  ArrayRef values) {
  if (!covariance) {
    return model::fail<ArrayRef>(Code::invalid_request, "no covariance supplied",
                                  "covariance_apply", "the covariance cannot be applied");
  }
  if (!covariance->model || !covariance->model->unknown_terms.empty()) {
    bool material = false;
    if (covariance->model) {
      for (const UnknownTerm& term : covariance->model->unknown_terms) {
        if (term.influence) material = true;
      }
    }
    if (material) {
      return model::fail<ArrayRef>(Code::unavailable,
                                   "an unknown model term influences the covariance",
                                   "covariance_apply", "the complete covariance is unknown");
    }
  }
  Result<std::vector<double>> data =
      operators::read_f64(context, values, 0, values->shape.empty() ? 0 : values->shape[0]);
  if (!ok(data)) {
    return model::fail<ArrayRef>(data.code, "the input cannot be read", "covariance_apply",
                                 "the covariance cannot be applied");
  }
  Result<std::vector<double>> mapped = uncertainty::apply_covariance(context, covariance, val(data));
  if (!ok(mapped)) {
    return model::fail<ArrayRef>(mapped.code, "the covariance cannot be applied",
                                 "covariance_apply", "the covariance cannot be applied");
  }
  const std::vector<U64> shape{static_cast<U64>(val(mapped).size())};
  return operators::write_f64(context, val(mapped), shape, covariance->model->variance_unit,
                              covariance->input_output.domain
                                  ? covariance->input_output.domain->identity.object_id
                                  : Id());
}

Result<Variance> quadratic_form(Context& context, Ref<CovarianceAction> covariance,
                                ArrayRef functional) {
  if (!covariance) {
    return model::fail<Variance>(Code::invalid_request, "no covariance supplied", "quadratic_form",
                                 "the variance cannot be computed");
  }
  Result<std::vector<double>> data =
      operators::read_f64(context, functional, 0,
                          functional->shape.empty() ? 0 : functional->shape[0]);
  if (!ok(data)) {
    return model::fail<Variance>(data.code, "the functional cannot be read", "quadratic_form",
                                 "the variance cannot be computed");
  }
  bool unknown = false;
  std::vector<Id> unknown_ids;
  if (covariance->model) {
    for (const UnknownTerm& term : covariance->model->unknown_terms) {
      if (term.influence) {
        unknown = true;
        unknown_ids.push_back(term.id);
      }
    }
  }
  if (unknown) {
    Variance variance;
    variance.value = std::nullopt;
    variance.unit = covariance->model ? covariance->model->variance_unit : model::unit_variance("unknown");
    variance.state = NoiseState::unknown;
    variance.unknown_terms = unknown_ids;
    return model::ok_result(variance);
  }
  Result<double> value = uncertainty::quadratic_form_value(context, covariance, val(data));
  if (!ok(value)) {
    Variance variance;
    variance.value = std::nullopt;
    variance.unit = covariance->model ? covariance->model->variance_unit : model::unit_variance("unknown");
    variance.state = NoiseState::unknown;
    return model::fail<Variance>(value.code, value.assessment.issues.empty()
                                                  ? "the variance cannot be computed"
                                                  : value.assessment.issues.front().reason,
                                 "quadratic_form", "the variance cannot be computed");
  }
  Variance variance;
  variance.value = val(value);
  variance.unit = covariance->model ? covariance->model->variance_unit : model::unit_variance("unknown");
  variance.state = covariance->model ? covariance->model->state : NoiseState::known;
  if (val(value) < 0.0) {
    // A negative variance is only acceptable when the numerical interval
    // around it contains zero.
    const double magnitude = std::fabs(val(value));
    const double scale = 1e-12 * std::max(1.0, magnitude);
    if (magnitude > scale) {
      return model::fail<Variance>(Code::invalid_covariance,
                                   "the covariance produced a negative variance",
                                   "quadratic_form", "the variance cannot be computed");
    }
    variance.value = 0.0;
  }
  return model::ok_result(variance);
}

Result<BoundOrUnavailable> geometry_covariance_bound(Context& context,
                                                      Ref<CovarianceAction> covariance,
                                                      Ref<LinearAction> nominal,
                                                      Ref<LinearAction> absolute_error,
                                                      ArrayRef functional) {
  BoundOrUnavailable out;
  if (!covariance || !nominal || !absolute_error) {
    out.reason = "the covariance bound needs a covariance and both actions";
    return model::ok_result(out);
  }
  const bool has_absolute = covariance->absolute_factor_bound != nullptr;
  const bool has_norm = covariance->operator_norm_upper.has_value();
  if (!has_absolute && !has_norm) {
    out.reason =
        "the covariance exposes neither an absolute factor bound nor a spectral norm bound";
    return model::ok_result(out);
  }
  Result<std::vector<double>> t =
      operators::read_f64(context, functional, 0, functional->shape.empty() ? 0 : functional->shape[0]);
  if (!ok(t)) {
    out.reason = "the functional cannot be read";
    return model::ok_result(out);
  }
  // u0 = R0^T t
  Result<std::vector<double>> u0 = uncertainty::apply_linear_transposed_helper(context, nominal, val(t));
  if (!ok(u0)) {
    out.reason = "the nominal action cannot be applied";
    return model::ok_result(out);
  }
  // delta = E^T |t|
  std::vector<double> abs_t(val(t).size());
  for (std::size_t i = 0; i < val(t).size(); ++i) abs_t[i] = std::fabs((val(t))[i]);
  Result<std::vector<double>> delta =
      uncertainty::apply_linear_transposed_helper(context, absolute_error, abs_t);
  if (!ok(delta)) {
    out.reason = "the absolute error action cannot be applied";
    return model::ok_result(out);
  }
  if (val(delta).size() != val(u0).size()) {
    out.reason = "the nominal and error actions disagree on the codomain";
    return model::ok_result(out);
  }
  double bound = 0.0;
  if (has_absolute) {
    Result<std::vector<double>> abs_cu = uncertainty::apply_absolute_bound(context, covariance, val(u0));
    if (!ok(abs_cu)) {
      out.reason = "the absolute factor bound cannot be applied";
      return model::ok_result(out);
    }
    Result<std::vector<double>> abs_cd = uncertainty::apply_absolute_bound(context, covariance, val(delta));
    if (!ok(abs_cd)) {
      out.reason = "the absolute factor bound cannot be applied";
      return model::ok_result(out);
    }
    // 2 delta^T |C u0| + delta^T |C| delta
    double first = 0.0, second = 0.0;
    for (std::size_t i = 0; i < val(delta).size(); ++i) {
      first += (val(delta))[i] * std::fabs((val(abs_cu))[i]);
      second += (val(delta))[i] * (val(abs_cd))[i];
    }
    bound = 2.0 * first + second;
  } else {
    // Spectral norm route: |dV| <= 2 |||C|| ||u0|| ||delta|| + |||C|| ||delta||^2
    const double norm = *covariance->operator_norm_upper;
    double norm_u0 = 0.0, norm_delta = 0.0;
    for (double v : val(u0)) norm_u0 += v * v;
    for (double v : val(delta)) norm_delta += v * v;
    norm_u0 = std::sqrt(norm_u0);
    norm_delta = std::sqrt(norm_delta);
    bound = 2.0 * norm * norm_u0 * norm_delta + norm * norm_delta * norm_delta;
  }
  out.bound = model::exact_point(bound);
  return model::ok_result(out);
}

}  // namespace acsd
