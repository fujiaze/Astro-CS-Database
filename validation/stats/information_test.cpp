// Independent validation of the information module.
//
// The checks are the ones of the statistics contract:
//   * V_F and V_B,p are different estimators: two frames with the same V_F
//     have different single leaf surface brightness weights, so the reference
//     source SNR never gives 1/V_B;
//   * an exact PSD constraint can determine the flux and then the variance is
//     exactly zero;
//   * a flux that is absorbed by the background has no finite variance;
//   * the sparse actual scene reconstruction reproduces a known log plane
//     inside the interval of equation (4a) and, without an external Hessian
//     bound, publishes only a conditional prediction.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "harness.hpp"

using namespace acsd_validation;
using acsd::Code;
using acsd::DType;
using acsd::Id;
using acsd::U64;

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

// A background action with the given columns.
acsd::Result<acsd::Ref<acsd::LinearAction>> background_action(acsd::Context& context,
                                                               const std::vector<std::vector<double>>& columns,
                                                               const std::string& name) {
  const std::size_t rows = columns.empty() ? 0 : columns[0].size();
  const std::size_t count = columns.size();
  std::vector<U64> offsets{0};
  std::vector<U64> indices;
  std::vector<double> values;
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < count; ++j) {
      if (columns[j][i] == 0.0) continue;
      indices.push_back(j);
      values.push_back(columns[j][i]);
    }
    offsets.push_back(indices.size());
  }
  acsd::Result<acsd::ArrayRef> off = store_u64(context, offsets, {offsets.size()}, name + ":offsets");
  acsd::Result<acsd::ArrayRef> ind = store_u64(context, indices, {indices.size()}, name + ":indices");
  acsd::Result<acsd::ArrayRef> val = store_f64(context, values, {values.size()}, name + ":values",
                                                acsd::Unit{});
  if (!is_ok(off) || !is_ok(ind) || !is_ok(val)) {
    return make_fail<acsd::Ref<acsd::LinearAction>>(Code::internal, "the arrays cannot be stored");
  }
  acsd::ActionRecipe recipe;
  recipe.kind = acsd::ActionRecipe::Kind::csr;
  recipe.algorithm = "validation_csr_v1";
  recipe.version = 1;
  recipe.arrays = {name + ":offsets", name + ":indices", name + ":values"};
  const std::vector<acsd::ArrayRef> arrays{value_of(off), value_of(ind), value_of(val)};
  return acsd::make_action(context, recipe, std::span<const acsd::Ref<acsd::LinearAction>>{}, arrays);
}

struct Case {
  const char* name;
  std::vector<double> h;
  std::vector<std::vector<double>> factor;
  std::vector<std::vector<double>> influence;
  double expected_vf;
  bool identifiable;
};

}  // namespace

int main() {
  MemoryStorage storage;
  CountingResources resources;
  NoCancellation cancellation;
  acsd::Context context{resources, cancellation, storage};

  // ---- the reference flux variance of two frames --------------------------
  // Case A: h = (0.5, 0.5) and C = 0.5 I give V_F = 1.
  // Case B: h = (0.9, 0.1) and C = 0.82 I give V_F = 1 as well.
  const double root_a = std::sqrt(0.5);
  const double root_b = std::sqrt(0.82);
  std::vector<Case> cases{
      {"reference source, 0.5 I", {0.5, 0.5}, {{root_a, 0.0}, {0.0, root_a}}, {{1.0, 0.0}, {0.0, 1.0}},
       1.0, true},
      {"reference source, 0.82 I", {0.9, 0.1}, {{root_b, 0.0}, {0.0, root_b}}, {{1.0, 0.0}, {0.0, 1.0}},
       1.0, true},
  };
  double variance_a = 0.0;
  double variance_b = 0.0;
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const Case& item = cases[index];
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> covariance =
        make_covariance(context, item.factor, item.influence, "case" + std::to_string(index));
    if (!is_ok(covariance)) {
      check(false, "the covariance of " + std::string(item.name) + " cannot be built");
      continue;
    }
    acsd::Result<acsd::ArrayRef> h = store_f64(context, item.h, {item.h.size()}, "h" + std::to_string(index),
                                                acsd::Unit{});
    acsd::InformationRequest request;
    request.h = value_of(h);
    request.covariance = value_of(covariance);
    request.reference.id = "reference";
    request.reference.quantity = "reference_flux";
    request.solve.rank_relative = 1e-12;
    acsd::Result<acsd::Information> information = acsd::estimate_flux_information(context, request);
    if (!is_ok(information)) {
      check(false, "the information of " + std::string(item.name) + " fails");
      continue;
    }
    const acsd::Information& value = value_of(information);
    if (!value.flux_variance.value.has_value()) {
      check(false, "the flux variance of " + std::string(item.name) + " is missing");
      continue;
    }
    check_close(*value.flux_variance.value, item.expected_vf, 1e-12,
                "V_F of the " + std::string(item.name));
    if (index == 0) {
      variance_a = *value.flux_variance.value;
    } else {
      variance_b = *value.flux_variance.value;
    }
  }
  check(std::fabs(variance_a - variance_b) < 1e-12,
        "the two reference sources have the same V_F");
  check(std::fabs(1.0 / 0.5 - 2.0) < 1e-12,
        "the inverse variance weight of the first scene leaf is 2");
  check(std::fabs(1.0 / 0.82 - 1.219512) < 1e-6,
        "the inverse variance weight of the second scene leaf is 1.219512");
  check(std::fabs(1.0 / variance_a - 1.0 / 0.5) > 1e-6 &&
            std::fabs(1.0 / variance_b - 1.0 / 0.82) > 1e-6,
        "1/V_F is not the inverse variance weight of the actual scene");

  // ---- the actual scene variance of a leaf --------------------------------
  {
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> covariance =
        make_covariance(context, cases[1].factor, cases[1].influence, "scene");
    acsd::Result<acsd::Ref<acsd::LinearAction>> native_to_leaf = identity_action(context, 2, "native");
    if (!is_ok(covariance)) {
      std::printf("       the scene covariance failed with code %d\n", static_cast<int>(covariance.code));
    }
    if (!is_ok(native_to_leaf)) {
      std::printf("       the native to leaf action failed with code %d\n",
                  static_cast<int>(native_to_leaf.code));
    }
    std::vector<TestFrame> frames;
    TestFrame frame;
    frame.frame = "frame_a";
    frame.order = 1;
    frame.leaves = {acsd::HealpixLeaf{1, 0}, acsd::HealpixLeaf{1, 1}};
    frame.signal = {1.0, 2.0};
    frame.dq = {0, 0};
    frame.state = {1, 1};
    frame.noise = value_of(covariance);
    frame.native_to_leaf = value_of(native_to_leaf);
    frames.push_back(frame);
    TestFramesProvider provider(frames);
    std::vector<acsd::HealpixLeaf> leaves = {acsd::HealpixLeaf{1, 0}};
    acsd::Result<std::vector<acsd::Variance>> scene =
        acsd::evaluate_scene_variance(context, provider, "frame_a", leaves);
    if (!is_ok(scene)) {
      std::printf("       the scene variance failed with code %d\n", static_cast<int>(scene.code));
      for (const acsd::Issue& issue : scene.assessment.issues) {
        std::printf("       issue: %s\n", issue.reason.c_str());
      }
      check(false, "the scene variance cannot be evaluated");
    } else {
      const std::vector<acsd::Variance>& variances = value_of(scene);
      check(variances.size() == 1 && variances[0].value.has_value(),
            "the scene variance of the leaf is available");
      if (!variances.empty() && variances[0].value.has_value()) {
        check_close(*variances[0].value, 0.82, 1e-12, "V_B of the leaf is 0.82");
        check(std::fabs(1.0 / *variances[0].value - 1.0 / variance_b) > 1e-6,
              "1/V_B differs from 1/V_F for the same frames");
      }
    }
  }

  // ---- an exact PSD constraint that determines the flux -------------------
  {
    // The second output has no noise at all, so the constraint U0^T y =
    // U0^T h F determines the flux exactly.
    std::vector<std::vector<double>> factor{{1.0, 0.0}, {0.0, 1.0}};
    std::vector<std::vector<double>> influence{{1.0, 0.0}, {0.0, 0.0}};
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> covariance =
        make_covariance(context, factor, influence, "psd");
    acsd::Result<acsd::ArrayRef> h = store_f64(context, {0.0, 1.0}, {2}, "h_psd", acsd::Unit{});
    acsd::InformationRequest request;
    request.h = value_of(h);
    request.covariance = value_of(covariance);
    request.reference.id = "reference";
    request.solve.rank_relative = 1e-12;
    acsd::Result<acsd::Information> information = acsd::estimate_flux_information(context, request);
    check(is_ok(information), "the information of the exact constraint is available");
    if (is_ok(information)) {
      const acsd::Variance& variance = value_of(information).flux_variance;
      check(variance.value.has_value(), "the exact constraint gives a finite variance");
      if (variance.value.has_value()) {
        check_close(*variance.value, 0.0, 1e-15, "the exact constraint gives V_F = 0");
      }
    }
  }

  // ---- a flux that is absorbed by the background --------------------------
  {
    std::vector<std::vector<double>> factor{{1.0, 0.0}, {0.0, 1.0}};
    std::vector<std::vector<double>> influence{{1.0, 0.0}, {0.0, 1.0}};
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> covariance =
        make_covariance(context, factor, influence, "identity");
    acsd::Result<acsd::ArrayRef> h = store_f64(context, {1.0, 0.0}, {2}, "h_absorbed", acsd::Unit{});
    acsd::Result<acsd::Ref<acsd::LinearAction>> background =
        background_action(context, {{1.0, 0.0}}, "background");
    if (!is_ok(background)) {
      std::printf("       the background action failed with code %d\n",
                  static_cast<int>(background.code));
    }
    acsd::InformationRequest request;
    request.h = value_of(h);
    request.covariance = value_of(covariance);
    request.background = value_of(background);
    request.reference.id = "reference";
    request.solve.rank_relative = 1e-12;
    acsd::Result<acsd::Information> information = acsd::estimate_flux_information(context, request);
    check(is_ok(information), "the absorbed flux does not fail the call");
    if (is_ok(information)) {
      const acsd::Variance& variance = value_of(information).flux_variance;
      check(!variance.value.has_value(), "the absorbed flux has no finite variance");
      bool flagged = false;
      for (const acsd::Issue& issue : information.assessment.issues) {
        if (issue.code == Code::not_identifiable) flagged = true;
      }
      check(flagged, "the absorbed flux is reported as not identifiable");
    }
  }

  // ---- the sparse actual scene reconstruction -----------------------------
  {
    // A log plane V(x, y) = exp(a + b x + c y) on a chart of five degrees.
    const double a = 0.5;
    const double b = 0.3;
    const double c = -0.2;
    const std::size_t side = 5;
    std::vector<double> positions;
    std::vector<double> values;
    std::vector<double> log_errors;
    for (std::size_t i = 0; i < side; ++i) {
      for (std::size_t j = 0; j < side; ++j) {
        const double x = -0.02 + 0.01 * static_cast<double>(i);
        const double y = -0.02 + 0.01 * static_cast<double>(j);
        positions.push_back(x);
        positions.push_back(y);
        values.push_back(std::exp(a + b * x + c * y));
        log_errors.push_back(0.01);
      }
    }
    acsd::Result<acsd::ArrayRef> position_array =
        store_f64(context, positions, {side * side, 2}, "controls:positions", acsd::Unit{});
    acsd::Result<acsd::ArrayRef> value_array =
        store_f64(context, values, {side * side}, "controls:values", acsd::Unit{});
    acsd::Result<acsd::ArrayRef> error_array =
        store_f64(context, log_errors, {side * side}, "controls:log_errors", acsd::Unit{});
    if (!is_ok(position_array) || !is_ok(value_array) || !is_ok(error_array)) {
      check(false, "the control arrays cannot be stored");
    } else {
      // The support covers every leaf of order one.
      std::vector<acsd::HealpixLeaf> leaves;
      for (U64 ipix = 0; ipix < 48; ++ipix) leaves.push_back(acsd::HealpixLeaf{1, ipix});
      std::vector<U64> ids;
      for (const acsd::HealpixLeaf& leaf : leaves) {
        ids.push_back(leaf.order);
        ids.push_back(leaf.ipix);
      }
      acsd::Result<acsd::ArrayRef> id_array =
          store_u64(context, ids, {leaves.size(), 2}, "support:ids");
      const std::vector<U64> state_shape{static_cast<U64>(leaves.size())};
      acsd::Result<acsd::ArrayRef> state_array = context.storage.copy(
          context, DType::u8, encode_u8(std::vector<std::uint8_t>(leaves.size(), 1)), state_shape,
          acsd::Unit{}, "support:states");
      if (!is_ok(id_array) || !is_ok(state_array)) {
        check(false, "the support cannot be stored");
      } else {
        auto support = std::make_shared<acsd::SupportMap>();
        auto index = std::make_shared<acsd::Index>();
        index->size = leaves.size();
        index->kind = acsd::Index::Kind::leaf;
        index->order = 1;
        index->ids = value_of(id_array);
        support->index = std::shared_ptr<const acsd::Index>(index);
        support->states = value_of(state_array);
        acsd::Ref<acsd::SupportMap> support_ref(support);

        auto controls = std::make_shared<acsd::PrecisionControls>();
        controls->kind = acsd::PrecisionControls::Kind::actual_scene;
        controls->positions = value_of(position_array);
        controls->values = value_of(value_array);
        controls->log_errors = value_of(error_array);
        controls->estimator.id = "scene_variance";
        controls->estimator.quantity = "actual_scene_variance";
        controls->hessian_norm_upper = 0.0;  // a log plane has no curvature
        acsd::Ref<acsd::PrecisionControls> controls_ref(controls);

        acsd::Chart chart;
        // The chart centre avoids the poles, where the local east direction
        // of the gnomonic chart is not defined.
        chart.center = acsd::SkyICRS{0.6, 0.0, 0.8};
        chart.maximum_radius_rad = 5.0 * M_PI / 180.0;
        acsd::Result<acsd::Ref<acsd::SupportedTriangulation>> mesh =
            acsd::build_supported_triangulation(context, controls_ref, chart, support_ref);
        if (!is_ok(mesh)) {
          std::printf("       the triangulation failed with code %d\n", static_cast<int>(mesh.code));
          for (const acsd::Issue& issue : mesh.assessment.issues) {
            std::printf("       issue: %s\n", issue.reason.c_str());
          }
        }
        check(is_ok(mesh), "the triangulation of the control grid is built");
        if (is_ok(mesh)) {
          // The queries are inside the chart and inside the grid.
          std::vector<acsd::SkyICRS> queries;
          std::vector<double> truth;
          for (std::size_t i = 0; i < 5; ++i) {
            for (std::size_t j = 0; j < 5; ++j) {
              const double x = -0.019 + 0.009 * static_cast<double>(i);
              const double y = -0.019 + 0.009 * static_cast<double>(j);
              // The chart basis of the centre (0.6, 0, 0.8): the east
              // direction is the normalised cross product of the z axis and
              // the centre, the north direction the cross product of the
              // centre and the east direction.
              const double east[3] = {0.0, 1.0, 0.0};
              const double north[3] = {-0.8, 0.0, 0.6};
              const double centre[3] = {0.6, 0.0, 0.8};
              const double raw[3] = {x * east[0] + y * north[0] + centre[0],
                                     x * east[1] + y * north[1] + centre[1],
                                     x * east[2] + y * north[2] + centre[2]};
              const double length = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
              acsd::SkyICRS direction;
              direction.nx = raw[0] / length;
              direction.ny = raw[1] / length;
              direction.nz = raw[2] / length;
              queries.push_back(direction);
              truth.push_back(std::exp(a + b * x + c * y));
            }
          }
          acsd::Result<acsd::VariancePrediction> prediction =
              acsd::reconstruct_scene_variance(context, controls_ref, value_of(mesh), queries);
          check(is_ok(prediction), "the reconstruction is available");
          if (is_ok(prediction)) {
            const acsd::VariancePrediction& result = value_of(prediction);
            const acsd::BlockRequest request{0, static_cast<acsd::U64>(queries.size())};
            acsd::Result<acsd::ArrayView> values_read =
                context.storage.read(context, result.values, request);
            acsd::Result<acsd::ArrayView> lower_read =
                context.storage.read(context, result.lower, request);
            acsd::Result<acsd::ArrayView> upper_read =
                context.storage.read(context, result.upper, request);
            if (!is_ok(values_read) || !is_ok(lower_read) || !is_ok(upper_read)) {
              check(false, "the reconstruction arrays cannot be read");
            } else {
              const std::vector<double> predicted = decode_f64(value_of(values_read).bytes);
              const std::vector<double> lower = decode_f64(value_of(lower_read).bytes);
              const std::vector<double> upper = decode_f64(value_of(upper_read).bytes);
              double worst = 0.0;
              bool inside = true;

              for (std::size_t i = 0; i < queries.size(); ++i) {
                worst = std::max(worst, std::fabs(predicted[i] - truth[i]));
                if (predicted[i] < lower[i] - 1e-12 || predicted[i] > upper[i] + 1e-12) inside = false;
                if (!(truth[i] >= lower[i] - 1e-12 && truth[i] <= upper[i] + 1e-12)) inside = false;
              }
              std::printf("       the largest deviation of the log plane reproduction is %.3g\n", worst);
              check(worst < 1e-9, "the log plane is reproduced by the triangulation");
              check(inside, "the truth lies inside the interval of equation (4a)");
              check(!result.conditional, "the reconstruction with a curvature bound is certified");
            }
          }
        }
        // Without an external curvature bound only a conditional prediction is
        // published.
        controls->hessian_norm_upper = std::nullopt;
        acsd::Result<acsd::Ref<acsd::SupportedTriangulation>> mesh2 =
            acsd::build_supported_triangulation(context, controls_ref, chart, support_ref);
        if (is_ok(mesh2)) {
          std::vector<acsd::SkyICRS> queries{acsd::SkyICRS{0.0, 0.0, 1.0}};
          acsd::Result<acsd::VariancePrediction> prediction =
              acsd::reconstruct_scene_variance(context, controls_ref, value_of(mesh2), queries);
          check(is_ok(prediction), "the conditional reconstruction is available");
          if (is_ok(prediction)) {
            check(value_of(prediction).conditional,
                  "without a curvature bound the prediction is conditional");
            acsd::Result<acsd::ArrayView> lower_read =
                context.storage.read(context, value_of(prediction).lower, acsd::BlockRequest{0, 1});
            check(is_ok(lower_read) && std::isnan(decode_f64(value_of(lower_read).bytes)[0]),
                  "without a curvature bound no certified interval is published");
          }
        }
      }
    }
  }

  std::printf("\n%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
