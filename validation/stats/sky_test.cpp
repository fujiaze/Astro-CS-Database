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


  // ---- the weighted sum gauge ----------------------------------------------
  // The gauge of the weighted sum fixes the sum of the components of a
  // connected component to zero, so the components carry the difference of the
  // frames to their weighted mean.
  {
    acsd::SkyRequest weighted = request;
    weighted.gauge.kind = acsd::Gauge::Kind::weighted_sum;
    weighted.gauge.reference.clear();
    acsd::Result<acsd::ArrayRef> gauge_weights = store_f64(
        context, std::vector<double>{1.0, 1.0}, {2}, "sky_test:gauge_weights", acsd::Unit{});
    weighted.gauge.weights = value_of(gauge_weights);

    acsd::Result<acsd::Ref<acsd::SkyFit>> weighted_fit =
        acsd::fit_relative_sky(context, value_of(controls), weighted);
    check(is_ok(weighted_fit), "the weighted sum gauge is fitted");
    if (is_ok(weighted_fit)) {
      acsd::Result<acsd::ArrayView> beta_view = context.storage.read(
          context, value_of(weighted_fit)->beta, acsd::BlockRequest{0, 2});
      check(is_ok(beta_view), "the components of the weighted sum gauge can be read");
      if (is_ok(beta_view)) {
        const std::vector<double> beta = decode_f64(value_of(beta_view).bytes);
        check_close(beta[0] + beta[1], 0.0, 1e-9,
                    "the weighted sum of the components vanishes");
        check_close(beta[0], -1.0, 1e-9,
                    "the first component of the weighted sum gauge is minus one");
        check_close(beta[1], 1.0, 1e-9,
                    "the second component of the weighted sum gauge is one");
      }
    }
  }

  // ---- several patches ------------------------------------------------------
  // The frames observe the leaves of two faces, so the control set carries two
  // patches and every frame contributes a control to each of them.
  {
    std::vector<acsd::HealpixLeaf> wide;
    for (acsd::U64 ipix = 0; ipix < 8; ++ipix) wide.push_back(acsd::HealpixLeaf{1, ipix});
    std::vector<TestFrame> many;
    for (int f = 0; f < 3; ++f) {
      TestFrame frame;
      frame.frame = f == 0 ? "frame_a" : (f == 1 ? "frame_b" : "frame_c");
      frame.order = 1;
      frame.leaves = wide;
      frame.signal = std::vector<double>(wide.size(), f == 0 ? 1.0 : (f == 1 ? 3.0 : 5.0));
      frame.dq = std::vector<std::uint16_t>(wide.size(), 0);
      frame.state = std::vector<std::uint8_t>(wide.size(), 1);
      frame.noise = value_of(frame_noise);
      many.push_back(frame);
    }
    TestFramesProvider many_provider(many);

    acsd::SkyRequest multi = request;
    multi.gauge.kind = acsd::Gauge::Kind::reference_frame;
    multi.gauge.reference = "frame_a";
    multi.groups.clear();
    acsd::Comparability multi_group;
    multi_group.frames = {"frame_a", "frame_b", "frame_c"};
    multi_group.response_proof = "validation_same_response";
    multi_group.spatial_kernel_proof = "validation_same_kernel";
    multi.groups.push_back(multi_group);
    // The control noise of the six controls of the two patches.
    std::vector<std::vector<double>> multi_covariance(6, std::vector<double>(6, 0.0));
    for (std::size_t i = 0; i < 6; ++i) multi_covariance[i][i] = 1.0;
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> multi_noise =
        make_covariance(context, multi_covariance, multi_covariance, "multi_control");
    multi.control_noise = value_of(multi_noise);
    acsd::Result<acsd::Ref<acsd::SkyControls>> multi_controls =
        acsd::build_sky_controls(context, many_provider, multi);
    if (!is_ok(multi_controls)) {
      std::printf("       the controls of two patches failed with code %d\n",
                  static_cast<int>(multi_controls.code));
      for (const acsd::Issue& issue : multi_controls.assessment.issues) {
        std::printf("       issue: %s\n", issue.reason.c_str());
      }
    }
    check(is_ok(multi_controls), "the controls of two patches are built");
    if (is_ok(multi_controls)) {
      const std::size_t control_count = value_of(multi_controls)->c->shape[0];
      check(control_count == 6, "every frame contributes a control to each of the two patches");
      acsd::Result<acsd::Ref<acsd::SkyFit>> multi_fit =
          acsd::fit_relative_sky(context, value_of(multi_controls), multi);
      if (!is_ok(multi_fit)) {
        std::printf("       the fit of two patches failed with code %d\n",
                    static_cast<int>(multi_fit.code));
        for (const acsd::Issue& issue : multi_fit.assessment.issues) {
          std::printf("       issue: %s\n", issue.reason.c_str());
        }
      }
      check(is_ok(multi_fit), "the sky of two patches is fitted");
      if (is_ok(multi_fit)) {
        acsd::Result<acsd::ArrayView> beta_view = context.storage.read(
            context, value_of(multi_fit)->beta, acsd::BlockRequest{0, 3});
        check(is_ok(beta_view), "the components of two patches can be read");
        if (is_ok(beta_view)) {
          const std::vector<double> beta = decode_f64(value_of(beta_view).bytes);
          check_close(beta[0], 0.0, 1e-9, "the reference frame of the first component vanishes");
          check_close(beta[1], 2.0, 1e-9, "the other frame of the first component carries the offset");
          check_close(beta[2], 4.0, 1e-9, "the third frame carries its own offset");
        }
      }
    }
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
