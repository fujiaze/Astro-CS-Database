// The observation.regroup typed process adapter (M5).
//
// regroup_observation is the unique synchronous implementation: it analyses
// the public LinearAction G, checks the physical mode and writes the new native
// arrays through Storage.  This adapter registers that one implementation as a
// typed process; it repeats no regrouping formula and creates no pool, ledger
// or output path.
#include <acsd/modules/observation.hpp>

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/process.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

using rt::Done;

namespace {

constexpr const char* kProcess = "observation.regroup";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 64ull << 10;
constexpr U64 kCleanupWorkUpper = 32;
constexpr U64 kWorkspaceUpper = 1ull << 20;

U64 saturating_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b) return std::numeric_limits<U64>::max();
  return a * b;
}
U64 saturating_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b) return std::numeric_limits<U64>::max();
  return a + b;
}

template <class T>
Result<T> adapter_failure(Code code, std::string reason, std::string impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, std::move(reason), kProcess, std::move(impact));
  return out;
}

std::string hex_id(const std::string& text) { return "sha256:" + model::sha256_hex(text); }

std::string number_text(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

U64 element_count(const ArrayRef& array) {
  if (!array) return 0;
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) return 0;
    total = saturating_mul(total, extent);
  }
  return array->shape.empty() ? 0 : total;
}

std::string array_text(const ArrayRef& array) {
  if (!array) return "<absent>";
  std::string text = array->identity.content_hash + "|" + array->index_id + "|" +
                     array->unit.scale + "|" + std::to_string(array->unit.linear_power) + "|";
  for (U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

U64 input_samples(const RegroupObservationInput& input) {
  return input.observation && input.observation->native_index
             ? input.observation->native_index->size
             : 0;
}
U64 output_samples(const RegroupObservationInput& input) {
  return input.request.output_footprints && input.request.output_footprints->index
             ? input.request.output_footprints->index->size
             : 0;
}

Result<Done> require_input(Ref<RegroupObservationInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "the regrouping cannot be validated");
  if (!input->observation)
    return adapter_failure<Done>(Code::invalid_request, "no observation supplied",
                                 "the regrouping cannot be validated");
  if (!input->request.G)
    return adapter_failure<Done>(Code::invalid_request, "no regrouping matrix supplied",
                                 "the regrouping cannot be validated");
  if (!input->request.G->spaces.domain || !input->request.G->spaces.codomain)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the regrouping matrix has no declared domain",
                                 "the regrouping cannot be validated");
  if (!input->request.output_footprints || !input->request.output_footprints->index)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the regrouping has no output footprint set",
                                 "the regrouping cannot be validated");
  if (!input->observation->native_index)
    return adapter_failure<Done>(Code::invalid_model, "the observation has no native index",
                                 "the regrouping cannot be validated");
  if (!input->observation->integrated_values || !input->observation->dq)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the observation has no integrated values or DQ array",
                                 "the regrouping cannot be validated");
  if (input->observation->unit.scale.empty())
    return adapter_failure<Done>(Code::invalid_model, "the observation has no unit",
                                 "the regrouping cannot be validated");
  const U64 n = input_samples(*input);
  const U64 m = output_samples(*input);
  if (n == 0 || m == 0)
    return adapter_failure<Done>(Code::invalid_model, "the input or output index is empty",
                                 "the regrouping cannot be validated");
  if (input->request.G->spaces.domain->size != n)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the regrouping matrix does not act on the native samples",
                                 "the regrouping cannot be validated");
  if (input->request.G->spaces.codomain->size != m)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the regrouping matrix does not match the output samples",
                                 "the regrouping cannot be validated");
  if (element_count(input->observation->integrated_values) != n ||
      element_count(input->observation->dq) != n)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the native arrays do not match the native index",
                                 "the regrouping cannot be validated");
  if (input->observation->numerical_lower &&
      element_count(input->observation->numerical_lower) != n)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the numerical lower bound does not match the native index",
                                 "the regrouping cannot be validated");
  if (input->observation->numerical_upper &&
      element_count(input->observation->numerical_upper) != n)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the numerical upper bound does not match the native index",
                                 "the regrouping cannot be validated");
  if (input->request.mode == RegroupRequest::Mode::physical_mean &&
      input->request.mean_member_count == 0)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the mean regrouping has no member count",
                                 "the regrouping cannot be validated");
  const double determinant = input->request.old_from_new[0] * input->request.old_from_new[3] -
                             input->request.old_from_new[1] * input->request.old_from_new[2];
  if (!std::isfinite(determinant) || determinant == 0.0)
    return adapter_failure<Done>(Code::invalid_model, "the image transform is not invertible",
                                 "the regrouping cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_regroup(rt::InvocationContext, Ref<RegroupObservationInput> input) {
  return require_input(input);
}

Result<rt::PlanBounds> bounds_regroup(rt::InvocationContext, Ref<RegroupObservationInput> input) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the regroup input is invalid",
                                           "the regrouping cannot be planned");
  const U64 elements = std::max<U64>(output_samples(*input), 1);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  // The unique implementation writes whole output arrays through Storage; there
  // is no spill route that preserves the exact signed interval reduction yet.
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

U64 regroup_work(const RegroupObservationInput& input) {
  const U64 n = input_samples(input);
  const U64 m = output_samples(input);
  // n + 1 units are charged explicitly; the column-by-column action application
  // and the row reduction are bounded by one unit per (native sample, output
  // sample) pair.
  return std::max<U64>(saturating_add(saturating_add(n, 1), saturating_mul(n, m)),
                       1ull << 20);
}

Result<rt::InvocationPlan> plan_regroup(rt::InvocationContext, Ref<RegroupObservationInput> input,
                                        rt::PlanOptions options) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the regroup input is invalid",
                                               "the regrouping cannot be planned");
  const U64 n = input_samples(*input);
  const U64 m = output_samples(*input);
  if (options.block_elements == 0 || options.block_elements > std::max<U64>(m, 1))
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "the regrouping cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic regrouping uses one window",
                                               "the regrouping cannot be planned");
  const U64 input_payload = saturating_mul(26, n);         // value 8 + bounds 16 + DQ 2
  const U64 output_payload = saturating_mul(26, m);
  const U64 column_scratch = saturating_mul(8, saturating_add(n, m));
  const U64 progress = saturating_add(
      saturating_add(input_payload, output_payload),
      saturating_add(column_scratch,
                     saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes,
                                                                    kStageControlBytes))));
  const U64 persistent =
      saturating_add(saturating_add(output_payload, 64ull << 10), kPlanControlBytes);
  if (progress == std::numeric_limits<U64>::max() ||
      persistent == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "the regrouping cannot be planned");

  const RegroupRequest& request = input->request;
  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = m;
  plan.block_elements = options.block_elements;
  plan.cohorts = 1;  // the unique implementation regroups all output samples atomically
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = saturating_add(kPlanControlBytes, kCohortControlBytes);
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = hex_id("native|" + input->observation->identity.content_hash + "|values|" +
                           array_text(input->observation->integrated_values) + "|dq|" +
                           array_text(input->observation->dq));
  plan.request_hash = hex_id(
      "G|" + request.G->identity.content_hash + "|footprints|" +
      (request.output_footprints ? request.output_footprints->identity.content_hash : "-") +
      "|old_from_new|" + number_text(request.old_from_new[0]) + "," +
      number_text(request.old_from_new[1]) + "," + number_text(request.old_from_new[2]) + "," +
      number_text(request.old_from_new[3]) + "|offset|" + number_text(request.offset[0]) + "," +
      number_text(request.offset[1]) + "|mode|" + std::to_string(static_cast<int>(request.mode)) +
      "|members|" + std::to_string(request.mean_member_count) + "|input_is_mean|" +
      std::to_string(request.input_is_mean));
  plan.algorithm_hash = hex_id("observation.regroup|exact_linear_regrouping_v1|1");

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent.host_bytes = persistent;
  region.progress_floor.host_bytes = progress;

  rt::StageSpec stage;
  stage.id = 0;
  stage.scratch.host_bytes = progress;
  stage.output_upper.host_bytes = output_payload;
  stage.control_bytes = kStageControlBytes;
  stage.work_upper = regroup_work(*input);
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

rt::Task<Ref<NativeObservation>> run_regroup(rt::InvocationContext context,
                                             Ref<RegroupObservationInput> input,
                                             Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<NativeObservation>>(
        context, adapter_failure<Ref<NativeObservation>>(Code::invalid_request,
                                                         "the typed input is null",
                                                         "the regrouping cannot run"));
  if (!plan)
    return rt::ready<Ref<NativeObservation>>(
        context, adapter_failure<Ref<NativeObservation>>(Code::invalid_request,
                                                         "no invocation plan supplied",
                                                         "the regrouping cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = regroup_work(*input);
  spec.semantic = "observation.regroup.regroup_observation";
  return rt::submit_cpu<Ref<NativeObservation>>(
      context, spec,
      [input](rt::InvocationContext task) -> Result<Ref<NativeObservation>> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Ref<NativeObservation>>(
              bridge.code, "the science context bridge cannot be created",
              "the regrouping cannot run");
        return acsd::regroup_observation((*bridge.value)->context(), input->observation,
                                         input->request);
      });
}

rt::Task<Ref<NativeObservation>> finalize_regroup(rt::InvocationContext context,
                                                  Ref<RegroupObservationInput>,
                                                  Ref<rt::InvocationPlan>,
                                                  Result<Ref<NativeObservation>> prior) {
  if (prior.code == Code::ok) return rt::ready<Ref<NativeObservation>>(context, std::move(prior));
  Result<Ref<NativeObservation>> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Ref<NativeObservation>>(context, std::move(kept));
}

}  // namespace

Result<Done> register_regroup(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor = acsd::generated::descriptor_for("observation.regroup");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the observation.regroup descriptor is unavailable",
                                 "observation.regroup cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the observation.regroup process cannot be started",
                                 "observation.regroup cannot be registered");
  const rt::Operation<RegroupObservationInput, Ref<NativeObservation>> operation{
      "regroup_observation",
      rt::TypeId{"acsd.input.RegroupObservationInput", 1},
      rt::TypeId{"acsd.NativeObservation", 1},
      &validate_regroup,
      &bounds_regroup,
      &plan_regroup,
      &run_regroup,
      &finalize_regroup};
  return builder.add<RegroupObservationInput, Ref<NativeObservation>>(*process.value, operation);
}

// Plan introspection for validation/process/observation.regroup.
Result<rt::InvocationPlan> plan_regroup_op(rt::InvocationContext ctx,
                                           Ref<RegroupObservationInput> input,
                                           rt::PlanOptions options) {
  return plan_regroup(ctx, input, options);
}

}  // namespace acsd::modules
