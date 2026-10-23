#pragma once
// Internal helpers of the coadd module.
//
// The module exchanges data with the other science modules only through the
// public contract, so the helpers below are private to src/coadd.  They
// implement the statistical estimators of STATISTICS_SKY_COADD section 5: the
// BLUE solution with its exact PSD treatment, the non negative quadratic
// program, the conservative single rejection leave one out test and the
// cross frame covariance assembly.
#include <cstddef>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"

namespace acsd {

// The inputs of the one combination implementation.  The frames provider is a
// borrowed reference for the duration of the call; the frozen sky fit, the
// sealed Selection and the sealed WeightSolution are strong references.
struct CoaddInputs {
  const FramesProvider* frames = nullptr;
  Ref<SkyFit> sky;
  Ref<Selection> selection;
  Ref<WeightSolution> weights;
  CoaddRequest request;
};

// Metadata-only validation of the sealed inputs.  It reads no payload, so the
// typed validate/plan hooks can use it; the payload-level leaf order and the
// coefficient matrix are checked by coadd_with_inputs after the read.
Result<rt::Done> validate_coadd_inputs(const CoaddInputs& inputs);

// The unique combination core: it consumes the sealed Selection/WeightSolution
// and never calls select_samples()/solve_weights().  coadd() delegates here
// after producing its selection and weights with the same functions, and the
// coadd.integrate run hook delegates here with the already sealed objects.
Result<CoaddResult> coadd_with_inputs(Context& context, const CoaddInputs& inputs);

}  // namespace acsd

namespace acsd::coadd_internal {


// The inverse of the standard normal cumulative distribution function.
double normal_quantile(double probability);

// ---------------------------------------------------------------------------
// Weight estimators
// ---------------------------------------------------------------------------

enum class BlueOutcome { kSolved, kZeroVariance, kUnresolved };

struct BlueResult {
  BlueOutcome outcome = BlueOutcome::kUnresolved;
  std::vector<double> weights;
  double variance = 0.0;       // t^T Gamma t
  double denominator = 0.0;    // 1^T q
  std::string reason;
};

// The BLUE solution t = Gamma^{-1} 1 / (1^T Gamma^{-1} 1).
//
// For a positive semi definite Gamma the null space is an exact constraint:
// when a null vector exists whose sum is not zero, a zero variance normalised
// solution exists and is used; otherwise the solution is taken in the range of
// Gamma.  No epsilon floor is added and the pseudo inverse never pretends to
// be a finite error.
BlueResult solve_blue(const numerics::Matrix& gamma, double rank_relative);

enum class QpOutcome { kSolved, kUnresolved, kNonConverged };

struct QpResult {
  QpOutcome outcome = QpOutcome::kUnresolved;
  std::vector<double> weights;
  double variance = 0.0;
  std::string reason;
};

// The non negative quadratic program min t^T Gamma t with 1^T t = 1 and
// t >= 0.  The active set method starts from the equal weight feasible
// solution, walks along the segment to a candidate with a negative weight,
// breaks the ties by the frame identity and uses the Bland rule.
QpResult solve_nonnegative_qp(Context& context, const numerics::Matrix& gamma,
                               const std::vector<std::string>& names,
                               std::size_t max_iterations);

// The covariance matrix of the accepted samples of one leaf.
numerics::Matrix sub_matrix(const numerics::Matrix& gamma, const std::vector<std::size_t>& rows);

}  // namespace acsd::coadd_internal
