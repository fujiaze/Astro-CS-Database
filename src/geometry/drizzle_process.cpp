// The geometry.drizzle typed process adapter: one operation that calls the one
// drizzle implementation (src/geometry/drizzle.cpp).  The plan keeps the same
// R for the overlap/support/normaliser quantities and gives a bounded F for the
// per-leaf merge spool; no second drizzle formula is added here.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "coordinates/healpix.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

namespace {

using rt::Done;

constexpr const char* kProcess = "geometry.drizzle";

template <class T>
Result<T> failure(Code code, std::string reason, std::string impact) {
  return model::fail<T>(code, std::move(reason), kProcess, std::move(impact));
}

U64 sat_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b)
    return std::numeric_limits<U64>::max();
  return a * b;
}

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) total = sat_mul(total, extent);
  return total;
}

U64 array_bytes(const ArrayRef& array) {
  if (!array) return 0;
  return sat_mul(element_count(array->shape),
                 static_cast<U64>(model::dtype_size(array->dtype)));
}

U64 overlaps_bytes(const Ref<Overlaps>& overlaps) {
  if (!overlaps) return 0;
  U64 bytes = 2048;
  bytes += sat_mul(static_cast<U64>(overlaps->contributions.size()),
                   sizeof(OverlapContribution));
  bytes += sat_mul(static_cast<U64>(overlaps->coarse_candidates.size()),
                   sizeof(UnresolvedDomain));
  if (overlaps->contribution_blocks) {
    bytes += array_bytes(overlaps->contribution_blocks->source_rows);
    bytes += array_bytes(overlaps->contribution_blocks->leaf_orders);
    bytes += array_bytes(overlaps->contribution_blocks->leaf_ipix);
  }
  return bytes;
}

U64 observation_bytes(const Ref<NativeObservation>& observation) {
  if (!observation) return 0;
  return array_bytes(observation->integrated_values) +
         array_bytes(observation->dq) +
         array_bytes(observation->numerical_lower) +
         array_bytes(observation->numerical_upper) + 2048;
}

U64 observation_elements(const Ref<NativeObservation>& observation) {
  return observation && observation->integrated_values
             ? element_count(observation->integrated_values->shape)
             : 0;
}

Id digest(std::initializer_list<std::string> parts) {
  std::string text;
  for (const std::string& part : parts) {
    text += part;
    text.push_back('|');
  }
  return "sha256:" + model::sha256_hex(text);
}

rt::PlanBounds bounds_of(U64 max_block, bool spill) {
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::max<U64>(1, max_block);
  bounds.max_window = 1;
  bounds.allow_spill = spill;
  return bounds;
}

rt::InvocationPlan plan_of(const rt::PlanOptions& options, U64 work,
                           U64 persistent, U64 progress, const Id& input_hash) {
  rt::InvocationPlan plan;
  plan.input_hash = input_hash;
  plan.request_hash = digest({"build_drizzle", input_hash});
  plan.options = options;
  plan.elements = std::max<U64>(1, work);
  plan.block_elements =
      options.block_elements > 0 ? options.block_elements : plan.elements;
  plan.block_elements = std::min(plan.block_elements, plan.elements);
  plan.cohorts = (plan.elements + plan.block_elements - 1) / plan.block_elements;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = 1024;
  plan.cleanup_work_upper = 1;
  plan.adaptive = false;
  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = std::max<U64>(1, plan.cohorts);
  region.persistent = plan.persistent;
  region.progress_floor = plan.progress_floor;
  rt::StageSpec stage;
  stage.id = 0;
  stage.work_upper = plan.elements;
  stage.control_bytes = plan.control_bytes;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  return plan;
}

Result<Done> validate_build_drizzle(rt::InvocationContext ctx,
                                    Ref<BuildDrizzleInput> input) {
  if (!input)
    return failure<Done>(Code::invalid_request, "the typed input is null",
                         "the drizzle cannot be built");
  if (!ctx.ids.process.empty() && ctx.ids.process != kProcess)
    return failure<Done>(Code::invalid_request,
                         "the invocation process id does not match " +
                             std::string(kProcess),
                         "the drizzle cannot be built");
  if (!ctx.ids.operation.empty() && ctx.ids.operation != "build_drizzle")
    return failure<Done>(Code::invalid_request,
                         "the invocation operation id does not match "
                         "build_drizzle",
                         "the drizzle cannot be built");
  if (!input->overlaps)
    return failure<Done>(Code::invalid_request,
                         "field 'overlaps' is a required object reference",
                         "the drizzle cannot be built");
  if (!input->observation)
    return failure<Done>(Code::invalid_request,
                         "field 'observation' is a required object reference",
                         "the drizzle cannot be built");
  if (!input->observation->integrated_values)
    return failure<Done>(Code::invalid_request,
                         "the native observation has no integrated values",
                         "the drizzle cannot be built");
  if (input->request.order > coordinates::kMaxOrder)
    return failure<Done>(Code::invalid_request,
                         "order exceeds the supported HEALPix order",
                         "the drizzle cannot be built");
  if (!(input->request.pixfrac > 0.0) || input->request.pixfrac > 1.0)
    return failure<Done>(Code::invalid_request,
                         "pixfrac must be within (0,1]",
                         "the drizzle cannot be built");
  if (input->request.budget.arithmetic.max_bits < 8 ||
      input->request.budget.arithmetic.max_bits > 65536)
    return failure<Done>(Code::invalid_request,
                         "arithmetic.max_bits must be within 8..65536",
                         "the drizzle cannot be built");
  if (!(input->request.budget.area_absolute_sr >= 0.0) ||
      !std::isfinite(input->request.budget.area_absolute_sr))
    return failure<Done>(Code::invalid_request,
                         "area_absolute_sr must be finite and non-negative",
                         "the drizzle cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_build_drizzle(rt::InvocationContext,
                                            Ref<BuildDrizzleInput> input) {
  const U64 elements =
      std::max<U64>(1, static_cast<U64>(input->overlaps->contributions.size()));
  return model::ok_result(bounds_of(elements, true));
}

Result<rt::InvocationPlan> plan_build_drizzle(rt::InvocationContext,
                                              Ref<BuildDrizzleInput> input,
                                              rt::PlanOptions options) {
  const U64 contributions =
      static_cast<U64>(input->overlaps->contributions.size());
  const U64 sources = observation_elements(input->observation);
  const U64 work = std::max<U64>(
      input->request.budget.max_work > 0 ? input->request.budget.max_work
                                         : 1,
      std::max<U64>(contributions, sources));
  // R: the overlap/support/recipe and observation metadata that stay alive for
  // every leaf merge (the same R is shared by all quantities).
  const U64 r = overlaps_bytes(input->overlaps) +
                observation_bytes(input->observation) + 4096;
  // F: the per-leaf merge spool (signal, bounds, support and normaliser).
  const U64 f = sat_mul(contributions, 256) +
                sat_mul(sources, sizeof(double) * 2) +
                sat_mul(contributions, 8 * 6 + 3) + 64 * 1024;
  return model::ok_result(plan_of(
      options, work, r, f,
      digest({"build_drizzle",
              input->overlaps->identity.object_id,
              input->observation->identity.object_id,
              std::to_string(input->request.order),
              std::to_string(input->request.pixfrac),
              std::to_string(contributions)})));
}

rt::Task<DrizzleResult> run_build_drizzle(rt::InvocationContext ctx,
                                          Ref<BuildDrizzleInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  if (!plan)
    return rt::ready<DrizzleResult>(
        ctx, failure<DrizzleResult>(Code::invalid_request,
                                    "no invocation plan supplied",
                                    "the drizzle cannot run"));
  U64 work = plan->elements;
  if (!plan->regions.empty() && !plan->regions.front().stages.empty())
    work = plan->regions.front().stages.front().work_upper;
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.cpu_tokens = 1;
  spec.work_upper = std::max<U64>(1, work);
  spec.semantic = Id("build_drizzle");
  return rt::submit_cpu<DrizzleResult>(
      ctx, spec,
      [input](rt::InvocationContext task) -> Result<DrizzleResult> {
        Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
            rt::detail::make_science_context(task, task.progress);
        if (!ok(bridge) || !bridge.value)
          return failure<DrizzleResult>(
              bridge.code == Code::ok ? Code::internal : bridge.code,
              "the runtime science-context bridge cannot be created",
              "the drizzle cannot run");
        return build_drizzle((*bridge.value)->context(), input->overlaps,
                             input->observation, input->request);
      });
}

rt::Task<DrizzleResult> finalize_build_drizzle(
    rt::InvocationContext ctx, Ref<BuildDrizzleInput>, Ref<rt::InvocationPlan>,
    Result<DrizzleResult> partial) {
  if (partial.code != Code::ok || !partial.value)
    return rt::ready<DrizzleResult>(ctx, std::move(partial));
  const DrizzleResult& result = *partial.value;
  Result<Done> checked = model::ok_result(Done{});
  if (!result.op)
    checked = failure<Done>(Code::invalid_model,
                            "the drizzle operator is null", "not publishable");
  else if (!result.estimate)
    checked = failure<Done>(Code::invalid_model,
                            "the drizzle estimate is null", "not publishable");
  else if (!result.estimate->signal)
    checked = failure<Done>(Code::invalid_model,
                            "the drizzle estimate carries no signal array",
                            "not publishable");
  else if (!result.op->bounds)
    checked = failure<Done>(Code::invalid_model,
                            "the drizzle operator carries no overlap bounds",
                            "not publishable");
  else if (result.estimate->numerical_lower &&
           result.estimate->numerical_upper &&
           element_count(result.estimate->numerical_lower->shape) !=
               element_count(result.estimate->signal->shape))
    checked = failure<Done>(Code::invalid_model,
                            "the drizzle lower bound is not co-indexed with the "
                            "signal",
                            "not publishable");
  if (!ok(checked)) {
    Result<DrizzleResult> failed =
        failure<DrizzleResult>(checked.code, "the drizzle result violates its "
                               "immutable output contract",
                               "the result is not published");
    return rt::ready<DrizzleResult>(ctx, std::move(failed));
  }
  return rt::ready<DrizzleResult>(ctx, std::move(partial));
}

}  // namespace

// Public plan introspection for the independent process tests.
Result<rt::InvocationPlan> plan_geometry_drizzle_build_drizzle(
    rt::InvocationContext ctx, Ref<BuildDrizzleInput> input,
    rt::PlanOptions options) {
  return plan_build_drizzle(ctx, std::move(input), options);
}

Result<rt::Done> register_geometry_drizzle(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for(kProcess);
  if (!ok(descriptor))
    return failure<Done>(
        descriptor.code,
        "the generated descriptor for geometry.drizzle is unavailable",
        "the process cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process =
      builder.begin(*descriptor.value);
  if (!ok(process))
    return failure<Done>(process.code,
                         "the geometry.drizzle process cannot be begun",
                         "the process cannot be registered");
  return builder.add<BuildDrizzleInput, DrizzleResult>(
      *process.value,
      rt::Operation<BuildDrizzleInput, DrizzleResult>{
          "build_drizzle", {"acsd.input.BuildDrizzleInput", 1},
          {"acsd.DrizzleResult", 1}, &validate_build_drizzle,
          &bounds_build_drizzle, &plan_build_drizzle, &run_build_drizzle,
          &finalize_build_drizzle});
}

}  // namespace acsd::modules
