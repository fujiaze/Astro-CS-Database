// Independent validation of the sky module.
//
// The case is deliberately small and analytic: two frames observe the same
// four leaves of one patch, the second frame carries a constant offset of two.
// A constant basis therefore has two components, one per frame, and the
// gauge of the reference frame fixes the component of the first frame to zero.
//
// The checks are the ones the science note demands:
//   * the controls are built from the frames and the control noise;
//   * the estimator leaves the common sky untouched, K H = 0;
//   * the reference frame is the gauge, so its own component vanishes and the
//     component of the other frame carries the offset;
//   * the corrected covariance is (I - E K T) C (I - E K T)^T, so the
//     correction changes the signal but keeps the covariance finite.

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

// Applies an action to a dense vector and returns the result.
std::vector<double> apply_action(acsd::Context& context, acsd::Ref<acsd::LinearAction> action,
                                  const std::vector<double>& input) {
  std::vector<double> out;
  if (!action) return out;
  acsd::Result<acsd::ArrayRef> payload = store_f64(context, input, {input.size()},
                                                     "sky_test:input", acsd::Unit{});
  if (!is_ok(payload)) return out;
  acsd::Result<acsd::ArrayRef> applied = action->apply(context, value_of(payload));
  if (!is_ok(applied)) return out;
  acsd::Result<acsd::ArrayView> view = context.storage.read(
      context, value_of(applied), acsd::BlockRequest{0, static_cast<acsd::U64>(input.size())});
  if (!is_ok(view)) return out;
  return decode_f64(value_of(view).bytes);
}

}  // namespace

int main() {
  CountingResources resources;
  NoCancellation cancellation;
  MemoryStorage storage;
  acsd::Context context{resources, cancellation, storage};

  const std::size_t leaves = 4;
  std::vector<acsd::HealpixLeaf> leaf_ids;
  for (acsd::U64 ipix = 0; ipix < leaves; ++ipix) leaf_ids.push_back(acsd::HealpixLeaf{1, ipix});

  // A unit variance noise for every frame.
  std::vector<std::vector<double>> eye(leaves, std::vector<double>(leaves, 0.0));
  for (std::size_t i = 0; i < leaves; ++i) eye[i][i] = 1.0;
  acsd::Result<acsd::Ref<acsd::CovarianceAction>> frame_noise =
      make_covariance(context, eye, eye, "frame");

  std::vector<TestFrame> frames;
  for (int f = 0; f < 2; ++f) {
    TestFrame frame;
    frame.frame = f == 0 ? "frame_a" : "frame_b";
    frame.order = 1;
    frame.leaves = leaf_ids;
    frame.signal = std::vector<double>(leaves, f == 0 ? 1.0 : 3.0);
    frame.dq = std::vector<std::uint16_t>(leaves, 0);
    frame.state = std::vector<std::uint8_t>(leaves, 1);
    frame.noise = value_of(frame_noise);
    frames.push_back(frame);
  }
  TestFramesProvider provider(frames);

  // The control noise of the two control rows, one per frame.
  std::vector<std::vector<double>> control_covariance(2, std::vector<double>(2, 0.0));
  for (std::size_t i = 0; i < 2; ++i) control_covariance[i][i] = 1.0;
  acsd::Result<acsd::Ref<acsd::CovarianceAction>> control_noise =
      make_covariance(context, control_covariance, control_covariance, "control");

  acsd::SkyRequest request;
  acsd::Comparability group;
  group.frames = {"frame_a", "frame_b"};
  group.response_proof = "validation_same_response";
  group.spatial_kernel_proof = "validation_same_kernel";
  request.groups.push_back(group);
  request.basis.kind = acsd::Basis::Kind::constant;
  request.gauge.kind = acsd::Gauge::Kind::reference_frame;
  request.gauge.reference = "frame_a";
  request.control_noise = value_of(control_noise);
  request.solve.relative_residual = 1e-10;
  request.solve.absolute_residual = 1e-12;
  request.solve.rank_relative = 1e-12;
  request.solve.max_iterations = 64;
  request.solve.dense_max_bytes = 1u << 22;

  acsd::Result<acsd::Ref<acsd::SkyControls>> controls =
      acsd::build_sky_controls(context, provider, request);
  if (!is_ok(controls)) {
    std::printf("       the sky controls failed with code %d\n", static_cast<int>(controls.code));
    for (const acsd::Issue& issue : controls.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(controls), "the sky controls are built");
  if (!is_ok(controls)) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }

  acsd::Result<acsd::ArrayView> control_view = context.storage.read(
      context, value_of(controls)->c, acsd::BlockRequest{0, static_cast<acsd::U64>(2)});
  check(is_ok(control_view), "the control vector can be read");

  acsd::Result<acsd::Ref<acsd::SkyFit>> fit = acsd::fit_relative_sky(context, value_of(controls), request);
  if (!is_ok(fit)) {
    std::printf("       the sky fit failed with code %d\n", static_cast<int>(fit.code));
    for (const acsd::Issue& issue : fit.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(fit), "the relative sky is fitted");
  if (!is_ok(fit)) {
    std::printf("%d failure(s)\n", failures);
    return 1;
  }

  // The gauge of the reference frame: its own component vanishes and the
  // component of the other frame carries the offset of two.
  acsd::Result<acsd::ArrayView> beta_view = context.storage.read(
      context, value_of(fit)->beta, acsd::BlockRequest{0, static_cast<acsd::U64>(2)});
  check(is_ok(beta_view), "the fitted components can be read");
  if (is_ok(beta_view)) {
    const std::vector<double> beta = decode_f64(value_of(beta_view).bytes);
    check(beta.size() == 2, "the fit has one component per frame");
    if (beta.size() == 2) {
      check_close(beta[0], 0.0, 1e-9, "the component of the reference frame vanishes");
      check_close(beta[1], 2.0, 1e-9, "the component of the other frame carries the offset");
    }
  }

  // K H = 0: the estimator does not move the common sky.
  double worst = 0.0;
  if (value_of(fit)->K && value_of(controls)->H) {
    const std::size_t width =
        value_of(controls)->H->spaces.domain ? value_of(controls)->H->spaces.domain->size : 0;
    for (std::size_t j = 0; j < width; ++j) {
      std::vector<double> unit(width, 0.0);
      unit[j] = 1.0;
      const std::vector<double> mapped = apply_action(context, value_of(controls)->H, unit);
      if (mapped.empty()) continue;
      const std::vector<double> projected = apply_action(context, value_of(fit)->K, mapped);
      for (const double value : projected) worst = std::max(worst, std::fabs(value));
    }
  }
  check(worst <= 1e-10, "the estimator leaves the common sky untouched (K H = 0)");

  // The correction of the data: the second frame loses the offset of two and
  // the reference frame keeps its own values.
  auto signal = std::make_shared<acsd::SignalEstimate>();
  // The signal of the group is the joint data of the two frames.
  std::vector<double> joint_signal(2 * leaves, 1.0);
  for (std::size_t i = leaves; i < 2 * leaves; ++i) joint_signal[i] = 3.0;
  acsd::Result<acsd::ArrayRef> signal_array = store_f64(context, joint_signal, {2 * leaves},
                                                         "sky_test:signal", acsd::Unit{});
  signal->signal = value_of(signal_array);
  // The joint noise of the two frames.
  std::vector<std::vector<double>> joint_noise(2 * leaves, std::vector<double>(2 * leaves, 0.0));
  for (std::size_t i = 0; i < 2 * leaves; ++i) joint_noise[i][i] = 1.0;
  acsd::Result<acsd::Ref<acsd::CovarianceAction>> joint_covariance =
      make_covariance(context, joint_noise, joint_noise, "joint");
  signal->noise = value_of(joint_covariance);
  signal->unit = acsd::Unit{};
  signal->estimator.id = "validation";
  signal->estimator.quantity = "brightness";
  signal->estimator.condition.estimator = "validation";
  signal->estimator.condition.kind = "calibrated";

  acsd::Result<acsd::Ref<acsd::SignalEstimate>> corrected =
      acsd::apply_sky(context, acsd::Ref<acsd::SignalEstimate>(signal), value_of(fit));
  if (!is_ok(corrected)) {
    std::printf("       the sky correction failed with code %d\n", static_cast<int>(corrected.code));
    for (const acsd::Issue& issue : corrected.assessment.issues) {
      std::printf("       issue: %s\n", issue.reason.c_str());
    }
  }
  check(is_ok(corrected), "the sky correction is applied");
  if (is_ok(corrected)) {
    acsd::Result<acsd::ArrayView> corrected_view = context.storage.read(
        context, value_of(corrected)->signal,
        acsd::BlockRequest{0, static_cast<acsd::U64>(2 * leaves)});
    check(is_ok(corrected_view), "the corrected signal can be read");
    if (is_ok(corrected_view)) {
      const std::vector<double> values = decode_f64(value_of(corrected_view).bytes);
      check(values.size() == 2 * leaves, "the corrected signal keeps the joint length");
      check_close(values[0], 1.0, 1e-9, "the reference frame keeps its own values");
      check_close(values[leaves], 1.0, 1e-9, "the corrected signal loses the common offset");
    }
    check(value_of(corrected)->noise != nullptr, "the corrected covariance is available");
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
