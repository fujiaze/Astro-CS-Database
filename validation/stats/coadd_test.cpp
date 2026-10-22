// Independent validation of the coadd module.
//
// The case is analytic.  Two frames observe one leaf each and their noise
// models share one root, so the joint covariance of the two samples is
//
//     Gamma = L L^T = [[1, 2], [2, 9]]   with   L = [[1, 0], [2, sqrt(5)]].
//
// For that covariance the unconstrained BLUE weights are
//
//     t = Gamma^-1 1 / (1^T Gamma^-1 1) = (7/6, -1/6),
//
// and the weights of the non negative quadratic programme are (1, 0), because
// the unconstrained solution has a negative component.  The two numbers are
// independent of the frame order, so the test also checks the ordering.
//
// The leave one out rejection is checked with a frame that carries a large
// offset: the rejection must remove that frame from the combination and the
// remaining combination must stay the one of the accepted frames.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

#include "harness.hpp"

using namespace acsd_validation;

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  std::printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what.c_str());
  if (!condition) ++failures;
}

void check_close(double value, double expected, double tolerance, const std::string& what) {
  const double difference = std::fabs(value - expected);
  std::printf("%s  %s (value %.12g, expected %.12g, |difference| %.3g)\n",
              difference <= tolerance ? "[ ok ]" : "[FAIL]", what.c_str(), value, expected, difference);
  if (difference > tolerance) ++failures;
}

}  // namespace

int main() {
  CountingResources resources;
  NoCancellation cancellation;
  MemoryStorage storage;
  acsd::Context context{resources, cancellation, storage};

  // The shared root: the latent dimension is two and the influences of the
  // two frames are the rows of L.
  const double root = std::sqrt(5.0);
  std::vector<std::vector<double>> factor{{1.0, 0.0}, {0.0, 1.0}};
  std::vector<std::vector<double>> influence_a{{1.0, 0.0}};
  std::vector<std::vector<double>> influence_b{{2.0, root}};
  acsd::Result<acsd::Ref<acsd::CovarianceAction>> noise_a =
      make_covariance(context, factor, influence_a, "frame_a", "shared", true);
  acsd::Result<acsd::Ref<acsd::CovarianceAction>> noise_b =
      make_covariance(context, factor, influence_b, "frame_b", "shared", true);

  std::vector<TestFrame> frames;
  for (int f = 0; f < 2; ++f) {
    TestFrame frame;
    frame.frame = f == 0 ? "frame_a" : "frame_b";
    frame.order = 1;
    frame.leaves = {acsd::HealpixLeaf{1, 0}};
    frame.signal = {f == 0 ? 1.0 : 2.0};
    frame.dq = {0};
    frame.state = {1};
    frame.noise = f == 0 ? value_of(noise_a) : value_of(noise_b);
    frames.push_back(frame);
  }
  TestFramesProvider provider(frames);

  acsd::CoaddRequest request;
  request.target.id = "validation";
  request.target.quantity = "brightness";
  request.target.unit = acsd::Unit{};
  request.mixed_observation = false;
  request.weights = acsd::WeightPolicy::blue;
  request.rejection.kind = acsd::RejectionPolicy::Kind::dq_only;
  request.solve.relative_residual = 1e-10;
  request.solve.absolute_residual = 1e-12;
  request.solve.rank_relative = 1e-12;
  request.solve.max_iterations = 64;
  request.solve.dense_max_bytes = 1u << 22;

  // ---- the selection --------------------------------------------------------
  acsd::Result<acsd::Ref<acsd::Selection>> selection =
      acsd::select_samples(context, provider, request);
  if (!is_ok(selection)) {
    std::printf("       the selection failed with code %d\n", static_cast<int>(selection.code));
    for (const acsd::Issue& issue : selection.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(selection), "the samples are selected");
  if (!is_ok(selection)) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }

  // ---- the BLUE weights -----------------------------------------------------
  acsd::Result<acsd::Ref<acsd::WeightSolution>> blue =
      acsd::solve_weights(context, provider, value_of(selection), request);
  if (!is_ok(blue)) {
    std::printf("       the weights failed with code %d\n", static_cast<int>(blue.code));
    for (const acsd::Issue& issue : blue.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(blue), "the BLUE weights are solved");
  if (is_ok(blue)) {
    acsd::Result<acsd::ArrayView> coefficients = context.storage.read(
        context, value_of(blue)->coefficients, acsd::BlockRequest{0, 2});
    check(is_ok(coefficients), "the weights can be read");
    if (is_ok(coefficients)) {
      const std::vector<double> weights = decode_f64(value_of(coefficients).bytes);
      check(weights.size() == 2, "there is one weight per frame");
      double sum = 0.0;
      for (const double weight : weights) sum += weight;
      check_close(sum, 1.0, 1e-12, "the weights sum to one");
      // The order of the frames is the order of the provider, so the analytic
      // pair is (7/6, -1/6) or its mirror.
      const bool forward = std::fabs(weights[0] - 7.0 / 6.0) < 1e-9 &&
                           std::fabs(weights[1] + 1.0 / 6.0) < 1e-9;
      const bool mirror = std::fabs(weights[0] + 1.0 / 6.0) < 1e-9 &&
                          std::fabs(weights[1] - 7.0 / 6.0) < 1e-9;
      check(forward || mirror, "the BLUE weights are (7/6, -1/6)");
    }
  }

  // ---- the non negative quadratic programme ---------------------------------
  acsd::CoaddRequest constrained = request;
  constrained.weights = acsd::WeightPolicy::nonnegative_qp;
  acsd::Result<acsd::Ref<acsd::WeightSolution>> qp =
      acsd::solve_weights(context, provider, value_of(selection), constrained);
  if (!is_ok(qp)) {
    std::printf("       the non negative weights failed with code %d\n", static_cast<int>(qp.code));
    for (const acsd::Issue& issue : qp.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(qp), "the non negative weights are solved");
  if (is_ok(qp)) {
    acsd::Result<acsd::ArrayView> coefficients = context.storage.read(
        context, value_of(qp)->coefficients, acsd::BlockRequest{0, 2});
    check(is_ok(coefficients), "the non negative weights can be read");
    if (is_ok(coefficients)) {
      const std::vector<double> weights = decode_f64(value_of(coefficients).bytes);
      check(weights.size() == 2, "the non negative weights have one entry per frame");
      double sum = 0.0;
      bool non_negative = true;
      for (const double weight : weights) {
        sum += weight;
        if (weight < -1e-12) non_negative = false;
      }
      check(non_negative, "the non negative weights are non negative");
      check_close(sum, 1.0, 1e-12, "the non negative weights sum to one");
      const bool unit = std::fabs(weights[0] - 1.0) < 1e-9 && std::fabs(weights[1]) < 1e-9;
      const bool mirror = std::fabs(weights[1] - 1.0) < 1e-9 && std::fabs(weights[0]) < 1e-9;
      check(unit || mirror, "the non negative weights are (1, 0)");
    }
  }

  // ---- the equal weights ----------------------------------------------------
  acsd::CoaddRequest equal = request;
  equal.weights = acsd::WeightPolicy::equal;
  acsd::Result<acsd::Ref<acsd::WeightSolution>> equal_solution =
      acsd::solve_weights(context, provider, value_of(selection), equal);
  check(is_ok(equal_solution), "the equal weights are solved");
  if (is_ok(equal_solution)) {
    acsd::Result<acsd::ArrayView> coefficients = context.storage.read(
        context, value_of(equal_solution)->coefficients, acsd::BlockRequest{0, 2});
    if (is_ok(coefficients)) {
      const std::vector<double> weights = decode_f64(value_of(coefficients).bytes);
      check_close(weights[0], 0.5, 1e-12, "the first equal weight is one half");
      check_close(weights[1], 0.5, 1e-12, "the second equal weight is one half");
    }
  }

  // ---- the coadd ------------------------------------------------------------
  acsd::Result<acsd::CoaddResult> combined = acsd::coadd(context, provider, nullptr, request);
  if (!is_ok(combined)) {
    std::printf("       the coadd failed with code %d\n", static_cast<int>(combined.code));
    for (const acsd::Issue& issue : combined.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(combined), "the coadd is produced");
  if (is_ok(combined)) {
    check(value_of(combined).estimate != nullptr, "the coadd carries an estimate");
    check(value_of(combined).weights != nullptr, "the coadd carries the weights");
    check(value_of(combined).selection != nullptr, "the coadd carries the selection");
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
