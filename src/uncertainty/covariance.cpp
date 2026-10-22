#include "uncertainty/covariance.hpp"

#include <algorithm>
#include <cmath>

#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd::uncertainty {
namespace {

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    if (total > (~0ull) / extent) return 0;
    total *= extent;
  }
  return total;
}

Result<std::vector<double>> apply_linear(Context& context, Ref<LinearAction> action,
                                         const std::vector<double>& values) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> array =
      operators::write_f64(context, values, shape, model::unit_dimensionless(), Id());
  if (!ok(array)) {
    return model::fail<std::vector<double>>(array.code, "the vector cannot be stored",
                                            "uncertainty.apply", "the action cannot be applied");
  }
  Result<ArrayRef> mapped = action->apply(context, val(array));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the action cannot be applied",
                                            "uncertainty.apply", "the action cannot be applied");
  }
  // The action may change the dimension: read back the number of elements the
  // action actually produced, not the length of the input vector.
  return operators::read_f64(context, val(mapped), 0, element_count(val(mapped)->shape));
}

Result<std::vector<double>> apply_linear_transposed(Context& context, Ref<LinearAction> action,
                                                     const std::vector<double>& values) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> array =
      operators::write_f64(context, values, shape, model::unit_dimensionless(), Id());
  if (!ok(array)) {
    return model::fail<std::vector<double>>(array.code, "the vector cannot be stored",
                                            "uncertainty.apply", "the action cannot be applied");
  }
  Result<ArrayRef> mapped = action->transpose_apply(context, val(array));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the action cannot be applied",
                                            "uncertainty.apply", "the action cannot be applied");
  }
  // The action may change the dimension: read back the number of elements the
  // action actually produced, not the length of the input vector.
  return operators::read_f64(context, val(mapped), 0, element_count(val(mapped)->shape));
}

}  // namespace

Result<std::vector<double>> apply_linear_helper(Context& context, Ref<LinearAction> action,
                                                const std::vector<double>& values) {
  return apply_linear(context, action, values);
}

Result<std::vector<double>> apply_linear_transposed_helper(Context& context,
                                                            Ref<LinearAction> action,
                                                            const std::vector<double>& values) {
  return apply_linear_transposed(context, action, values);
}

Result<std::vector<double>> apply_covariance(Context& context, Ref<CovarianceAction> covariance,
                                              const std::vector<double>& values) {
  if (!covariance || !covariance->model) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no covariance supplied",
                                            "covariance_apply", "the covariance cannot be applied");
  }
  const NoiseModel& model_ref = *covariance->model;
  std::vector<Ref<NoiseRoot>> roots;
  roots.insert(roots.end(), model_ref.innovations.begin(), model_ref.innovations.end());
  roots.insert(roots.end(), model_ref.shared_factors.begin(), model_ref.shared_factors.end());
  if (roots.size() != model_ref.influence.size()) {
    return model::fail<std::vector<double>>(Code::invalid_model,
                                            "the influence count does not match the root count",
                                            "covariance_apply", "the covariance cannot be applied");
  }
  // The action's own influence maps the covariance space onto the space of
  // the action: the published covariance is I C I^T.  A functional on the
  // codomain is therefore pushed back with I^T before the model covariance is
  // applied, and the result is pushed forward with I afterwards.
  std::vector<double> input = values;
  if (covariance->influence) {
    Result<std::vector<double>> pushed = apply_linear_transposed(context, covariance->influence, values);
    if (!ok(pushed)) {
      return model::fail<std::vector<double>>(pushed.code, "the influence cannot be applied",
                                              "covariance_apply", "the covariance cannot be applied");
    }
    input = val(pushed);
  }
  std::vector<double> accumulator(input.size(), 0.0);
  for (std::size_t k = 0; k < roots.size(); ++k) {
    const Ref<NoiseRoot>& root = roots[k];
    const Ref<LinearAction>& influence = model_ref.influence[k];
    if (!root || !root->factor || !influence) {
      return model::fail<std::vector<double>>(Code::invalid_model, "a root is incomplete",
                                              "covariance_apply", "the covariance cannot be applied");
    }
    // L_k^T v  ->  F_k^T (L_k^T v)  ->  F_k (...)  ->  L_k (...)
    Result<std::vector<double>> projected =
        apply_linear_transposed(context, influence, input);
    if (!ok(projected)) return projected;
    Result<std::vector<double>> latent = apply_linear_transposed(context, root->factor, val(projected));
    if (!ok(latent)) return latent;
    Result<std::vector<double>> back = apply_linear(context, root->factor, val(latent));
    if (!ok(back)) return back;
    Result<std::vector<double>> contribution = apply_linear(context, influence, val(back));
    if (!ok(contribution)) return contribution;
    if (val(contribution).size() != accumulator.size()) {
      return model::fail<std::vector<double>>(Code::invalid_model,
                                              "the influence changes the output size",
                                              "covariance_apply", "the covariance cannot be applied");
    }
    for (std::size_t i = 0; i < accumulator.size(); ++i) accumulator[i] += (val(contribution))[i];
  }
  if (covariance->influence) {
    Result<std::vector<double>> forwarded = apply_linear(context, covariance->influence, accumulator);
    if (!ok(forwarded)) {
      return model::fail<std::vector<double>>(forwarded.code, "the influence cannot be applied",
                                              "covariance_apply", "the covariance cannot be applied");
    }
    return model::ok_result(val(forwarded));
  }
  return model::ok_result(std::move(accumulator));
}

Result<double> quadratic_form_value(Context& context, Ref<CovarianceAction> covariance,
                                     const std::vector<double>& functional) {
  Result<std::vector<double>> mapped = apply_covariance(context, covariance, functional);
  if (!ok(mapped)) {
    return model::fail<double>(mapped.code, "the covariance cannot be applied",
                                "quadratic_form", "the variance cannot be computed");
  }
  double sum = 0.0;
  for (std::size_t i = 0; i < functional.size() && i < val(mapped).size(); ++i)
    sum += functional[i] * (val(mapped))[i];
  return model::ok_result(sum);
}

Result<std::vector<double>> apply_absolute_bound(Context& context, Ref<CovarianceAction> covariance,
                                                 const std::vector<double>& values) {
  if (!covariance || !covariance->absolute_factor_bound) {
    return model::fail<std::vector<double>>(Code::unsupported,
                                            "the covariance has no absolute factor bound",
                                            "covariance.absolute", "the bound cannot be computed");
  }
  Result<ArrayRef> array =
      operators::write_f64(context, values, std::span<const U64>{},
                           model::unit_dimensionless(), Id());
  if (!ok(array)) {
    return model::fail<std::vector<double>>(array.code, "the vector cannot be stored",
                                            "covariance.absolute", "the bound cannot be computed");
  }
  Result<ArrayRef> mapped = (*covariance->absolute_factor_bound)->apply(context, val(array));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the absolute bound cannot be applied",
                                            "covariance.absolute", "the bound cannot be computed");
  }
  // The action may change the dimension: read back the number of elements the
  // action actually produced, not the length of the input vector.
  return operators::read_f64(context, val(mapped), 0, element_count(val(mapped)->shape));
}

}  // namespace acsd::uncertainty
