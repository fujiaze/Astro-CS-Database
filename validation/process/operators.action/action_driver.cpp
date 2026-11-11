// M5 typed driver for operators.action.
//
// Positive: identity 2x2 applies [1,2] -> [1,2].
// Negative: a CSR with out-of-range column and a compose whose middle
// dimension disagrees must both be refused with invalid_model.
#include "../lib/fixtures.hpp"

#include <acsd/modules/operators.hpp>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

#include <memory>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::model;
using namespace acsd::rt;

Result<Ref<LinearAction>> make_action(const vpf::Chain& chain, ActionRecipe recipe,
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

ActionRecipe identity_recipe(Ref<Index> index) {
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::identity;
  recipe.algorithm = "validation.identity";
  recipe.version = 1;
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = vpf::unit_of("ADU");
  recipe.spaces.output = vpf::unit_of("ADU");
  return recipe;
}

}  // namespace

int main() {
  vrt::Recorder recorder("operators.action",
                         "validation/process/operators.action/action_driver.cpp");
  auto started = vpf::start({{"operators.action", &acsd::modules::register_operators_action}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;
  const Ref<Index> index = vpf::make_index(2, "action:index");
  const Ref<Index> one = vpf::make_index(1, "action:one");

  Result<Ref<LinearAction>> identity = make_action(chain, identity_recipe(index), {}, {});
  recorder.expect_code("positive.identity", "ok", identity);
  auto x = vpf::make_array<double>(chain.command, DType::f64, std::vector<double>{1.0, 2.0},
                                   std::vector<U64>{2}, index, vpf::unit_of("ADU"));
  recorder.expect_code("fixture.x", "ok", x);
  if (identity.code != Code::ok || x.code != Code::ok) return recorder.finish();
  auto apply_input = std::make_shared<modules::ApplyInput>();
  apply_input->action = *identity.value;
  apply_input->values = x.value->array;
  auto applied = vpf::invoke<modules::ApplyInput, ArrayRef>(chain, "operators.action", "apply",
                                                           apply_input);
  recorder.expect_code("positive.apply", "ok", applied);
  if (applied.code == Code::ok) {
    const std::vector<double> values = vpf::read_f64(chain.command, *applied.value, 2);
    recorder.check("positive.identity_values",
                   values.size() == 2 && values[0] == 1.0 && values[1] == 2.0, "[1,2]",
                   values.size() == 2 ? vrt::number(values[0]) + "," + vrt::number(values[1])
                                      : "read_failed");
  }

  // Negative: a CSR column outside the declared column count.
  auto coefficients = vpf::make_array<U64>(chain.command, DType::u64,
                                           std::vector<U64>{0, 1, 2, 1, 1},
                                           std::vector<U64>{5}, index, vpf::unit_of("ADU"));
  recorder.expect_code("fixture.csr", "ok", coefficients);
  if (coefficients.code != Code::ok) return recorder.finish();
  auto bad = make_action(chain, ActionRecipe{}, {}, {});
  // Build a deliberately malformed CSR through the typed builder: offsets
  // declare two rows but the column index is 5 with cols=2.
  {
    auto malformed = std::make_shared<modules::MakeActionInput>();
    malformed->recipe.kind = ActionRecipe::Kind::csr;
    malformed->recipe.algorithm = "validation.bad_csr";
    malformed->recipe.version = 1;
    malformed->recipe.spaces.domain = index;
    malformed->recipe.spaces.codomain = index;
    malformed->recipe.spaces.input = vpf::unit_of("ADU");
    malformed->recipe.spaces.output = vpf::unit_of("ADU");
    malformed->children =
        std::make_shared<const std::vector<Ref<LinearAction>>>(std::vector<Ref<LinearAction>>{});
    malformed->arrays = std::make_shared<const std::vector<ArrayRef>>(
        std::vector<ArrayRef>{coefficients.value->array});
    auto refused = vpf::invoke<modules::MakeActionInput, Ref<LinearAction>>(
        chain, "operators.action", "make_action", malformed);
    recorder.check("negative.bad_csr_refused",
                   refused.code == Code::invalid_model || refused.code == Code::invalid_request,
                   "invalid_model or invalid_request", vrt::code_name(refused.code));
  }

  // Negative: compose 1x1 outer with 2x2 inner (middle dimension disagrees).
  ActionRecipe outer_recipe = identity_recipe(one);
  Result<Ref<LinearAction>> outer = make_action(chain, outer_recipe, {}, {});
  recorder.expect_code("fixture.outer", "ok", outer);
  if (outer.code != Code::ok) return recorder.finish();
  ActionRecipe compose_recipe;
  compose_recipe.kind = ActionRecipe::Kind::compose;
  compose_recipe.algorithm = "validation.compose";
  compose_recipe.version = 1;
  compose_recipe.spaces.domain = index;
  compose_recipe.spaces.codomain = one;
  compose_recipe.spaces.input = vpf::unit_of("ADU");
  compose_recipe.spaces.output = vpf::unit_of("ADU");
  auto composed = make_action(chain, compose_recipe, {*outer.value, *identity.value}, {});
  recorder.check("negative.compose_shape_refused", composed.code == Code::invalid_model,
                 "invalid_model", vrt::code_name(composed.code));
  return recorder.finish();
}
