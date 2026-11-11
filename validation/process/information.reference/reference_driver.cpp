// M5 typed driver for information.reference.
//
// Positive: C = diag(4,9) (factor diag(2,3), identity influence), h = [1,0],
// so V_F = 1/(h^T C^-1 h) = 4.
// Negative: the same model with NoiseState::unknown must publish V_F nullopt.
#include "../lib/fixtures.hpp"

#include <acsd/modules/information.hpp>
#include <acsd/modules/operators.hpp>
#include <acsd/modules/uncertainty.hpp>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

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
  Result<Ref<LinearAction>> factor = invoke_action(
      chain, factor_recipe, {}, {coefficients.value->array});
  if (factor.code != Code::ok) {
    return model::fail<Ref<CovarianceAction>>(factor.code, "factor", "driver", "no");
  }
  ActionRecipe influence_recipe;
  influence_recipe.kind = ActionRecipe::Kind::identity;
  influence_recipe.algorithm = "validation.identity";
  influence_recipe.version = 1;
  influence_recipe.spaces.domain = index;
  influence_recipe.spaces.codomain = index;
  influence_recipe.spaces.input = vpf::unit_of("ADU");
  influence_recipe.spaces.output = vpf::unit_of("ADU");
  Result<Ref<LinearAction>> influence = invoke_action(chain, influence_recipe, {}, {});
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
  model->variance_unit = model::unit_variance("adu");
  model->state = known ? NoiseState::known : NoiseState::unknown;
  model->assessment = model::make_assessment(Code::ok);
  model->assessment.complete = known;
  model->assessment.noise = model->state;
  model->identity = model::make_identity(
      "acsd.NoiseModel", 2, 0,
      model::sha256_hex(std::string("driver:info:") + (known ? "known" : "unknown")), "driver");
  auto input = std::make_shared<modules::MakeCovarianceInput>();
  input->model = Ref<NoiseModel>(model);
  return vpf::invoke<modules::MakeCovarianceInput, Ref<CovarianceAction>>(
      chain, "uncertainty.covariance", "make_covariance", input);
}

}  // namespace

int main() {
  vrt::Recorder recorder("information.reference",
                         "validation/process/information.reference/reference_driver.cpp");
  auto started = vpf::start(
      {{"operators.action", &acsd::modules::register_operators_action},
       {"uncertainty.covariance", &acsd::modules::register_uncertainty_covariance},
       {"information.reference", &acsd::modules::register_information_reference}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  const Ref<Index> index = vpf::make_index(2, "info:index");
  auto h = vpf::make_array<double>(chain.command, DType::f64, std::vector<double>{1.0, 0.0},
                                   std::vector<U64>{2}, index, model::unit_dimensionless());
  recorder.expect_code("fixture.h", "ok", h);
  if (h.code != Code::ok) return recorder.finish();

  for (bool known : {true, false}) {
    Result<Ref<CovarianceAction>> covariance = build_covariance(chain, index, known);
    recorder.expect_code(known ? "fixture.cov_known" : "fixture.cov_unknown", "ok", covariance);
    if (covariance.code != Code::ok) return recorder.finish();
    auto input = std::make_shared<modules::EstimateFluxInformationInput>();
    input->request.h = h.value->array;
    input->request.covariance = *covariance.value;
    input->request.reference = Estimator{"flux"};
    input->request.solve.rank_relative = 1e-12;
    auto information = vpf::invoke<modules::EstimateFluxInformationInput, Information>(
        chain, "information.reference", "estimate_flux_information", input);
    recorder.expect_code(known ? "positive.code" : "negative.code", "ok", information);
    if (information.code != Code::ok) continue;
    if (known) {
      const bool value_is_4 =
          information.value->flux_variance.value.has_value() &&
          std::fabs(*information.value->flux_variance.value - 4.0) <= 1e-12;
      recorder.check("positive.V_F_is_4", value_is_4, "4",
                     information.value->flux_variance.value
                         ? vrt::number(*information.value->flux_variance.value)
                         : "null");
      recorder.check("positive.state_known",
                     information.value->flux_variance.state == NoiseState::known, "known",
                     vrt::code_name(Code::ok));
    } else {
      recorder.check("negative.V_F_is_null",
                     !information.value->flux_variance.value.has_value(), "nullopt",
                     information.value->flux_variance.value ? "number" : "nullopt");
      recorder.check("negative.state_unknown",
                     information.value->flux_variance.state == NoiseState::unknown, "unknown",
                     vrt::code_name(Code::ok));
    }
  }
  return recorder.finish();
}
