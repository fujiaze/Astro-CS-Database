// M5 fixed driver for observation.regroup.
//
// Positive: the regrouping matrix is an identity LinearAction produced by the
// typed operators.action process (Host invoke make_action), then consumed by
// observation.regroup through the typed Host boundary.  With unit coefficients
// the regrouped values must equal the parent values exactly.
// Negative: missing G, an unknown operation, and a singular image transform.
#include "../lib/fixtures.hpp"

#include <acsd/modules/observation.hpp>
#include <acsd/modules/operators.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

Result<Ref<LinearAction>> make_identity(const vpf::Chain& chain, Ref<Index> domain,
                                        Ref<Index> codomain) {
  auto input = std::make_shared<modules::MakeActionInput>();
  input->recipe.kind = ActionRecipe::Kind::identity;
  input->recipe.algorithm = "validation.identity";
  input->recipe.version = 1;
  input->recipe.spaces.domain = std::move(domain);
  input->recipe.spaces.codomain = std::move(codomain);
  input->recipe.spaces.input = vpf::unit_of("ADU");
  input->recipe.spaces.output = vpf::unit_of("ADU");
  input->children = std::make_shared<const std::vector<Ref<LinearAction>>>();
  input->arrays = std::make_shared<const std::vector<ArrayRef>>();
  return vpf::invoke<modules::MakeActionInput, Ref<LinearAction>>(
      chain, "operators.action", "make_action", input);
}

}  // namespace

int main() {
  vrt::Recorder recorder("observation.regroup",
                         "validation/process/observation.regroup/regroup.cpp");
  auto started = vpf::start({{"operators.action", &acsd::modules::register_operators_action},
                             {"observation.regroup", &acsd::modules::register_regroup}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  auto native = vpf::make_native(chain.command, {1.0, 2.0, 4.0}, {0, 0, 0}, 3, 1, "frame0", 2016.0);
  if (!native.observation) return recorder.finish();
  const auto output_index = vpf::make_index(3, "regrouped:frame0");
  const auto output_footprints = vpf::make_footprints(output_index, 3, 1, "detector0");

  // ---- positive: identity regrouping through the typed action process ----
  auto action = make_identity(chain, native.index, output_index);
  recorder.expect_code("fixture.identity_action", "ok", action);
  if (action.code != Code::ok || !action.value) return recorder.finish();
  recorder.check("fixture.action_domain",
                 (*action.value)->spaces.domain && (*action.value)->spaces.domain->size == 3,
                 "3", (*action.value)->spaces.domain
                          ? std::to_string((*action.value)->spaces.domain->size)
                          : "absent");

  auto input = std::make_shared<modules::RegroupObservationInput>();
  input->observation = native.observation;
  input->request.G = *action.value;
  input->request.output_footprints = output_footprints;
  input->request.old_from_new = Mat2{1.0, 0.0, 0.0, 1.0};
  input->request.offset = Vec2{0.0, 0.0};
  input->request.input_is_mean = false;
  input->request.mean_member_count = 0;
  input->request.mode = RegroupRequest::Mode::physical_sum;

  auto plan = modules::plan_regroup_op(chain.command, input, PlanOptions{3, 1, Placement::memory});
  recorder.expect_code("regroup.plan", "ok", plan);
  if (plan.code == Code::ok) {
    recorder.value("regroup.plan.elements", plan.value->elements);
    recorder.value("regroup.plan.persistent_host_bytes", plan.value->persistent.host_bytes);
    recorder.value("regroup.plan.progress_floor_host_bytes",
                   plan.value->progress_floor.host_bytes);
    recorder.value("regroup.plan.work_upper", plan.value->regions.front().stages.front().work_upper);
  }
  auto regrouped = vpf::invoke<modules::RegroupObservationInput, Ref<NativeObservation>>(
      chain, "observation.regroup", "regroup_observation", input);
  recorder.expect_code("regroup.run", "ok", regrouped);
  if (regrouped.code == Code::ok && regrouped.value) {
    const NativeObservation& result = **regrouped.value;
    const std::vector<double> values =
        vpf::read_f64(chain.command, result.integrated_values, 3);
    recorder.check("regroup.exact_values",
                   values.size() == 3 && values[0] == 1.0 && values[1] == 2.0 && values[2] == 4.0,
                   "[1,2,4]",
                   values.size() == 3 ? vrt::number(values[0]) + "," + vrt::number(values[1]) +
                                            "," + vrt::number(values[2])
                                      : std::to_string(values.size()));
    recorder.check("regroup.index", result.native_index && result.native_index->size == 3, "3",
                   std::to_string(result.native_index ? result.native_index->size : 0));
    recorder.check("regroup.parent_preserved", result.native_parent == native.observation, "true",
                   result.native_parent ? "true" : "false");
    recorder.check("regroup.frame_preserved", result.frame == "frame0", "frame0", result.frame);
    const std::vector<double> dq = vpf::read_u16(chain.command, result.dq, 3).empty()
                                       ? std::vector<double>{}
                                       : std::vector<double>{};
    (void)dq;
  }

  // ---- negative: no regrouping matrix ------------------------------------
  auto missing = std::make_shared<modules::RegroupObservationInput>();
  missing->observation = native.observation;
  missing->request.output_footprints = output_footprints;
  missing->request.old_from_new = Mat2{1.0, 0.0, 0.0, 1.0};
  missing->request.mode = RegroupRequest::Mode::physical_sum;
  auto missing_plan =
      modules::plan_regroup_op(chain.command, missing, PlanOptions{3, 1, Placement::memory});
  recorder.expect_code("negative.missing_G_plan", "invalid_request", missing_plan);
  auto missing_run = vpf::invoke<modules::RegroupObservationInput, Ref<NativeObservation>>(
      chain, "observation.regroup", "regroup_observation", missing);
  recorder.expect_code("negative.missing_G", "invalid_request", missing_run);

  // ---- negative: unknown operation of a registered process --------------
  auto unknown = vpf::invoke<modules::RegroupObservationInput, Ref<NativeObservation>>(
      chain, "observation.regroup", "not_an_operation", missing);
  recorder.expect_code("negative.unknown_operation", "unsupported", unknown);

  // ---- negative: a singular image transform -----------------------------
  auto singular_input = std::make_shared<modules::RegroupObservationInput>(*input);
  singular_input->request.old_from_new = Mat2{1.0, 2.0, 2.0, 4.0};
  auto singular = vpf::invoke<modules::RegroupObservationInput, Ref<NativeObservation>>(
      chain, "observation.regroup", "regroup_observation", singular_input);
  recorder.expect_code("negative.singular_transform", "invalid_model", singular);

  return recorder.finish();
}
