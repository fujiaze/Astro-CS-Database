// Self test of the uncertainty base: the published covariance of a
// CovarianceAction is I C I^T, where I is the action's own influence.
//
// The action's influence used to be ignored: the model covariance was applied
// directly, so a covariance that is pushed through a linear map came out
// unchanged.  The U01 fixture hides this because its influence is the
// identity, so the test below uses a non-identity influence.
#include <array>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"
#include "uncertainty/covariance.hpp"

namespace {

int failures = 0;

void check(const char* name, bool condition, const char* detail = "") {
  if (condition) {
    std::printf("ok     %s\n", name);
  } else {
    std::printf("FAIL   %s %s\n", name, detail);
    ++failures;
  }
}

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// A diagonal action with the given values.
acsd::Result<acsd::Ref<acsd::LinearAction>> diagonal(
    acsd::Context& context, acsd::storage::LocalStorage& storage, const std::vector<double>& values,
    const acsd::Id& name) {
  using namespace acsd;
  auto bytes = model::encode_f64(values);
  auto copied = storage.copy(context, DType::f64, bytes,
                              std::span<const U64>(std::array<U64, 1>{values.size()}),
                              model::unit_dimensionless(), name);
  if (!ok(copied)) {
    return model::fail<Ref<LinearAction>>(copied.code, "the array cannot be stored", "selftest",
                                         "the action cannot be built");
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "selftest_diagonal_v1";
  recipe.version = 1;
  recipe.arrays = {name};
  std::vector<ArrayRef> arrays{val(copied)};
  return make_action(context, recipe, std::span<const Ref<LinearAction>>{}, arrays);
}

}  // namespace

int main() {
  using namespace acsd;
  NeverCancelled cancellation;
  Limits limits;
  limits.bytes = 1u << 20;
  limits.work = 1000000;
  limits.threads = 1;
  resources::CpuResources resources(limits);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  // The model: one innovation with the identity factor and the identity
  // influence, so the model covariance is the scalar 1.
  auto identity = diagonal(context, storage, {1.0}, "identity");
  if (!ok(identity)) {
    std::printf("FAIL   the identity action cannot be built\n");
    return 1;
  }
  NoiseRoot root;
  root.shared_id.external_namespace = "selftest";
  root.shared_id.entity = "root";
  root.shared_id.model_revision = "1";
  root.shared_id.component = "all";
  root.unit = model::unit_dimensionless();
  root.factor = val(identity);

  NoiseModel noise;
  noise.variance_unit = model::unit_dimensionless();
  noise.innovations.push_back(std::make_shared<const NoiseRoot>(root));
  noise.influence.push_back(val(identity));
  noise.state = NoiseState::known;

  auto covariance = make_covariance(context, std::make_shared<const NoiseModel>(noise));
  if (!ok(covariance)) {
    std::printf("FAIL   the covariance cannot be built\n");
    return 1;
  }

  // The action's own influence multiplies the covariance by four.
  auto scale = diagonal(context, storage, {2.0}, "scale");
  if (!ok(scale)) {
    std::printf("FAIL   the scaling action cannot be built\n");
    return 1;
  }
  CovarianceAction action = *val(covariance);
  action.influence = val(scale);
  auto with_influence = std::make_shared<const CovarianceAction>(action);

  // The functional is the scalar 1 on the single output coordinate.
  std::vector<double> functional{1.0};
  auto bytes = model::encode_f64(functional);
  auto stored = storage.copy(context, DType::f64, bytes,
                             std::span<const U64>(std::array<U64, 1>{1}),
                             model::unit_dimensionless(), "functional");
  if (!ok(stored)) {
    std::printf("FAIL   the functional cannot be stored\n");
    return 1;
  }
  auto variance = quadratic_form(context, with_influence, val(stored));
  check("quadratic_form applies the influence on both sides", ok(variance));
  if (ok(variance)) {
    const double value = val(variance).value.value_or(
        std::numeric_limits<double>::quiet_NaN());
    std::printf("       variance = %.17g (expected 4)\n", value);
    check("the variance is 4", value == 4.0, "the influence was ignored");
  }

  // Two roots with the same shared identity are the same random variable:
  // their influences must be merged, so the cross terms appear once.  With
  // the scalar factor and the scalar influence of one, a single root gives
  // the variance 1 and two roots of the same identity give 4 (not 2).
  {
    auto build = [&](int root_count) -> Result<Variance> {
      NoiseModel shared;
      shared.variance_unit = model::unit_dimensionless();
      shared.state = NoiseState::known;
      for (int k = 0; k < root_count; ++k) {
        NoiseRoot root;
        root.shared_id.external_namespace = "selftest";
        root.shared_id.entity = "shared";
        root.shared_id.model_revision = "1";
        root.shared_id.component = "all";
        root.unit = model::unit_dimensionless();
        root.factor = val(identity);
        shared.shared_factors.push_back(std::make_shared<const NoiseRoot>(root));
        shared.influence.push_back(val(identity));
      }
      auto action = make_covariance(context, std::make_shared<const NoiseModel>(shared));
      if (!ok(action)) {
        return model::fail<Variance>(action.code, "the covariance cannot be built", "selftest",
                                     "the variance cannot be computed");
      }
      return quadratic_form(context, val(action), val(stored));
    };
    const auto single = build(1);
    check("one shared root gives the variance 1",
          ok(single) && val(single).value.value_or(-1.0) == 1.0);
    const auto twice = build(2);
    check("two roots of the same identity are merged",
          ok(twice) && val(twice).value.value_or(-1.0) == 4.0);
  }

  std::printf("%s: %d failure(s)\n",
              failures == 0 ? "covariance self test PASS" : "covariance self test FAIL", failures);
  return failures == 0 ? 0 : 1;
}
