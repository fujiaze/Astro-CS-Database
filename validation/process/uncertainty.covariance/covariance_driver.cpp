// M5 typed driver for uncertainty.covariance.
//
// Positive: C = diag(4,9), C*[1,2] = [4,18]; quadratic form with [1,0] = 4.
// Negative: an unknown model makes covariance_apply fail with unavailable.
#include "../lib/fixtures.hpp"

#include <acsd/modules/operators.hpp>
#include <acsd/modules/uncertainty.hpp>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

#include <cmath>
#include <memory>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::model;
using namespace acsd::rt;

Result<Ref<LinearAction>> invoke_action(const vpf::Chain& chain, ActionRecipe recipe,
                                        std::vector<Ref<LinearAction>> children,
                                        std::vector<ArrayRef> arrays) {
  std::vector<Id> children_ids;
  for (const auto& child : children) children_ids.push_back(child->identity.object_id);
  std::vector<Id> array_ids;
  for (const auto& array : arrays) array_ids.push_back(array->identity.object_id);
  recipe.children = children_ids;
  recipe.arrays = array_ids;
  auto input = std::make_shared<modules::MakeActionInput>();
  input->recipe = std::move(recipe);
  input->children = std::make_shared<const std::vector<Ref<LinearAction>>>(std::move(children));
  input->arrays = std::make_shared<const std::vector<ArrayRef>>(std::move(arrays));
  return vpf::invoke<modules::MakeActionInput, Ref<LinearAction>>(
      chain, "operators.action", "make_action", input);
}

Result<Ref<CovarianceAction>> build_covariance(const vpf::Chain& chain, Ref<Index> index,
                                               bool known) {
  auto coefficients = vpf::make_array<double>(
      chain.command, DType::f64, std::vector<double>{2.0, 3.0}, std::vector<U64>{2}, index,
      vpf::unit_of("ADU"));
  if (coefficients.code != Code::ok) {
    return model::fail<Ref<CovarianceAction>>(coefficients.code, "coefficients", "driver", "no");
  }
  ActionRecipe factor_recipe;
  factor_recipe.kind = ActionRecipe::Kind::diagonal;
  factor_recipe.algorithm = "validation.diagonal";
  factor_recipe.version = 1;
  factor_recipe.spaces.domain = index;
  factor_recipe.spaces.codomain = index;
  factor_recipe.spaces.input = vpf::unit_of("ADU");
  factor_recipe.spaces.output = vpf::unit_of("ADU");
  Result<Ref<LinearAction>> factor =
      invoke_action(chain, factor_recipe, {}, {coefficients.value->array});
  if (factor.code != Code::ok) {
    return model::fail<Ref<CovarianceAction>>(factor.code, "factor", "driver", "no");
  }
  ActionRecipe identity_recipe;
  identity_recipe.kind = ActionRecipe::Kind::identity;
  identity_recipe.algorithm = "validation.identity";
  identity_recipe.version = 1;
  identity_recipe.spaces.domain = index;
  identity_recipe.spaces.codomain = index;
  identity_recipe.spaces.input = vpf::unit_of("ADU");
  identity_recipe.spaces.output = vpf::unit_of("ADU");
  Result<Ref<LinearAction>> influence = invoke_action(chain, identity_recipe, {}, {});
  if (influence.code != Code::ok) {
    return model::fail<Ref<CovarianceAction>>(influence.code, "influence", "driver", "no");
  }
  auto root = std::make_shared<NoiseRoot>();
  root->coordinates = index;
  root->factor = *factor.value;
  root->unit = vpf::unit_of("ADU");
  auto model = std::make_shared<NoiseModel>();
  model->innovations = {root};
  model->influence = {*influence.value};
  model->variance_unit = unit_variance("adu");
  model->state = known ? NoiseState::known : NoiseState::unknown;
  model->assessment = make_assessment(Code::ok);
  model->assessment.complete = known;
  model->assessment.noise = model->state;
  model->identity = make_identity(
      "acsd.NoiseModel", 2, 0,
      sha256_hex(std::string("driver:cov:") + (known ? "known" : "unknown")), "driver");
  auto input = std::make_shared<modules::MakeCovarianceInput>();
  input->model = Ref<NoiseModel>(model);
  return vpf::invoke<modules::MakeCovarianceInput, Ref<CovarianceAction>>(
      chain, "uncertainty.covariance", "make_covariance", input);
}

}  // namespace

int main() {
  vrt::Recorder recorder("uncertainty.covariance",
                         "validation/process/uncertainty.covariance/covariance_driver.cpp");
  auto started = vpf::start({{"operators.action", &acsd::modules::register_operators_action},
                             {"uncertainty.covariance",
                              &acsd::modules::register_uncertainty_covariance}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;
  const Ref<Index> index = vpf::make_index(2, "cov:index");

  auto x = vpf::make_array<double>(chain.command, DType::f64, std::vector<double>{1.0, 2.0},
                                   std::vector<U64>{2}, index, vpf::unit_of("ADU"));
  auto c = vpf::make_array<double>(chain.command, DType::f64, std::vector<double>{1.0, 0.0},
                                   std::vector<U64>{2}, index, vpf::unit_of("ADU"));
  recorder.expect_code("fixture.x", "ok", x);
  recorder.expect_code("fixture.c", "ok", c);
  if (x.code != Code::ok || c.code != Code::ok) return recorder.finish();

  for (bool known : {true, false}) {
    Result<Ref<CovarianceAction>> covariance = build_covariance(chain, index, known);
    recorder.expect_code(known ? "fixture.cov_known" : "fixture.cov_unknown", "ok", covariance);
    if (covariance.code != Code::ok) return recorder.finish();
    auto apply_input = std::make_shared<modules::CovarianceApplyInput>();
    apply_input->covariance = *covariance.value;
    apply_input->values = x.value->array;
    auto applied = vpf::invoke<modules::CovarianceApplyInput, ArrayRef>(
        chain, "uncertainty.covariance", "covariance_apply", apply_input);
    if (known) {
      recorder.expect_code("positive.apply_code", "ok", applied);
      if (applied.code == Code::ok) {
        const std::vector<double> values =
            vpf::read_f64(chain.command, *applied.value, 2);
        const bool expected = values.size() == 2 &&
                              std::fabs(values[0] - 4.0) <= 1e-12 &&
                              std::fabs(values[1] - 18.0) <= 1e-12;
        recorder.check("positive.C_times_x", expected, "[4,18]",
                       values.size() == 2 ? vrt::number(values[0]) + "," + vrt::number(values[1])
                                          : "read_failed");
      }
      auto qf_input = std::make_shared<modules::QuadraticFormInput>();
      qf_input->covariance = *covariance.value;
      qf_input->functional = c.value->array;
      auto qf = vpf::invoke<modules::QuadraticFormInput, Variance>(
          chain, "uncertainty.covariance", "quadratic_form", qf_input);
      recorder.expect_code("positive.qf_code", "ok", qf);
      if (qf.code == Code::ok) {
        const bool four = qf.value->value.has_value() &&
                          std::fabs(*qf.value->value - 4.0) <= 1e-12;
        recorder.check("positive.quadratic_form_is_4", four, "4",
                       qf.value->value ? vrt::number(*qf.value->value) : "null");
      }
    } else {
      recorder.check("negative.apply_unavailable", applied.code == Code::unavailable,
                     "unavailable", vrt::code_name(applied.code));
    }
  }
  return recorder.finish();
}
