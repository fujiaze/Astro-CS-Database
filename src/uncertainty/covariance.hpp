#pragma once
// Covariance assembly from a noise model.  One shared random variable is
// propagated once: the roots are the independent blocks and every influence
// chain is applied in the same pass, so the cross terms between outputs that
// share a root are preserved.
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::uncertainty {

// Concatenated root list: innovations first, then shared factors.
struct RootList {
  std::vector<Ref<NoiseRoot>> roots;
  std::vector<Ref<LinearAction>> influences;
  U64 total_coordinates = 0;
};

// Applies C to a vector over the output index.
Result<std::vector<double>> apply_covariance(Context& context, Ref<CovarianceAction> covariance,
                                              const std::vector<double>& values);

// Quadratic form t^T C t.
Result<double> quadratic_form_value(Context& context, Ref<CovarianceAction> covariance,
                                     const std::vector<double>& functional);

// Applies a linear action (and its transpose) to a plain vector.
Result<std::vector<double>> apply_linear_helper(Context& context, Ref<LinearAction> action,
                                                const std::vector<double>& values);
Result<std::vector<double>> apply_linear_transposed_helper(Context& context,
                                                            Ref<LinearAction> action,
                                                            const std::vector<double>& values);

// The absolute coefficient bound |C| applied to a vector, when the covariance
// exposes an absolute factor bound.
Result<std::vector<double>> apply_absolute_bound(Context& context,
                                                 Ref<CovarianceAction> covariance,
                                                 const std::vector<double>& values);

}  // namespace acsd::uncertainty
