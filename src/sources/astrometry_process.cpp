// The sources.astrometry typed process adapter (M5).
//
// solve_astrometry is the unique synchronous implementation.  The adapter
// registers it as a typed process and sizes the atomic resource model from the
// actual detection/array metadata; it contains no matching, WCS-fit or
// detection formula.
#include <acsd/modules/sources.hpp>

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

constexpr const char* kProcess = "sources.astrometry";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 64ull << 10;
constexpr U64 kCleanupWorkUpper = 32;
constexpr U64 kWorkspaceUpper = 2ull << 20;

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
                     array->unit.scale + "|";
  for (U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

std::string solver_text(const SolverRequest& request) {
  std::string text = std::to_string(request.regions.size()) + "|" +
                     number_text(request.scale_rad_pixel.lower) + "," +
                     number_text(request.scale_rad_pixel.upper) + "|" +
                     std::to_string(request.max_candidates) + "|" +
                     std::to_string(request.max_hypotheses) + "|" +
                     std::to_string(request.sip_order) + "|" +
                     number_text(request.detect_snr) + "|" +
                     number_text(request.match_radius_pixel) + "|" +
                     std::to_string(request.min_matches) + "|" +
                     number_text(request.quad_descriptor_tolerance) + "|" +
                     std::to_string(request.budget.max_iterations) + "|" +
                     number_text(request.budget.tolerance.absolute) + "|" +
                     number_text(request.budget.tolerance.relative);
  for (const SkySearchRegion& region : request.regions) {
    text += "|" + number_text(region.center.nx) + "," + number_text(region.center.ny) + "," +
            number_text(region.center.nz) + "," + number_text(region.radius_rad);
  }
  return text;
}

U64 discovery_elements(const SolveAstrometryInput& input) {
  if (!input.observation) return 0;
  if (input.observation->native_index) return input.observation->native_index->size;
  return element_count(input.observation->integrated_values);
}

Result<Done> require_input(Ref<SolveAstrometryInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "solve_astrometry cannot be validated");
  if (!input->observation)
    return adapter_failure<Done>(Code::invalid_request, "no observation supplied",
                                 "solve_astrometry cannot be validated");
  if (!input->catalogue)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue provider supplied",
                                 "solve_astrometry cannot be validated");
  if (!input->observation->integrated_values || !input->observation->dq ||
      !input->observation->native_index || !input->observation->footprints)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the observation has no native data, index or footprint",
                                 "solve_astrometry cannot be validated");
  if (input->observation->native_index->size !=
      element_count(input->observation->integrated_values))
    return adapter_failure<Done>(Code::invalid_model,
                                 "the native arrays do not match the native index",
                                 "solve_astrometry cannot be validated");
  const SolverRequest& request = input->request;
  if (request.min_matches == 0 || request.max_candidates == 0)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the match and the candidate budgets must be positive",
                                 "solve_astrometry cannot be validated");
  if (!(request.detect_snr > 0.0) || !(request.match_radius_pixel > 0.0))
    return adapter_failure<Done>(Code::invalid_request,
                                 "the detection and the match thresholds must be positive",
                                 "solve_astrometry cannot be validated");
  if (!(request.scale_rad_pixel.lower > 0.0) || !(request.scale_rad_pixel.upper > 0.0) ||
      request.scale_rad_pixel.upper < request.scale_rad_pixel.lower)
    return adapter_failure<Done>(Code::invalid_request, "the scale interval is invalid",
                                 "solve_astrometry cannot be validated");
  if (!std::isfinite(request.quad_descriptor_tolerance) ||
      request.quad_descriptor_tolerance < 0.0)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the quad descriptor tolerance is invalid",
                                 "solve_astrometry cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_astrometry(rt::InvocationContext, Ref<SolveAstrometryInput> input) {
  return require_input(input);
}

U64 astrometry_work(const SolveAstrometryInput& input) {
  const U64 detections = std::max<U64>(discovery_elements(input), 1);
  const U64 candidates = std::max<U64>(input.request.max_candidates, 1);
  const U64 hypotheses = std::max<U64>(input.request.max_hypotheses, 1);
  return std::max<U64>(saturating_add(saturating_add(detections, candidates), hypotheses),
                       8ull << 20);
}

Result<rt::PlanBounds> bounds_astrometry(rt::InvocationContext, Ref<SolveAstrometryInput> input) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the astrometry input is invalid",
                                           "solve_astrometry cannot be planned");
  const U64 elements = std::max<U64>(discovery_elements(*input), 1);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_astrometry(rt::InvocationContext, Ref<SolveAstrometryInput> input,
                                           rt::PlanOptions options) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the astrometry input is invalid",
                                               "solve_astrometry cannot be planned");
  const U64 elements = std::max<U64>(discovery_elements(*input), 1);
  if (options.block_elements == 0 || options.block_elements > elements)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "solve_astrometry cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic solve uses one window",
                                               "solve_astrometry cannot be planned");
  const U64 native = discovery_elements(*input);
  const U64 candidates = std::min<U64>(native, std::max<U64>(input->request.max_candidates, 1));
  const U64 quads = saturating_mul(candidates, 16);
  const U64 input_payload = saturating_mul(26, native);
  const U64 solve_scratch = saturating_add(saturating_mul(128, quads),
                                           saturating_mul(512, candidates));
  const U64 progress = saturating_add(
      saturating_add(input_payload, solve_scratch),
      saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes, kStageControlBytes)));
  const U64 persistent = saturating_add(saturating_mul(64, candidates), 512ull << 10);
  const U64 work = astrometry_work(*input);
  if (progress == std::numeric_limits<U64>::max() ||
      persistent == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "solve_astrometry cannot be planned");

  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = elements;
  plan.block_elements = options.block_elements;
  plan.cohorts = 1;
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = saturating_add(kPlanControlBytes, kCohortControlBytes);
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = hex_id("native|" + input->observation->identity.content_hash + "|values|" +
                           array_text(input->observation->integrated_values) + "|dq|" +
                           array_text(input->observation->dq) + "|index|" +
                           std::to_string(input->observation->native_index->size));
  plan.request_hash = hex_id("catalogue|" + input->catalogue->dataset_hash() + "|solver|" +
                             solver_text(input->request));
  plan.algorithm_hash = hex_id("sources.astrometry|quad_match_wcs_fit_v1|1");

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent.host_bytes = persistent;
  region.progress_floor.host_bytes = progress;
  rt::StageSpec stage;
  stage.id = 0;
  stage.scratch.host_bytes = progress;
  stage.control_bytes = kStageControlBytes;
  stage.work_upper = work;
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

rt::Task<Ref<AstrometricSolution>> run_astrometry(rt::InvocationContext context,
                                                  Ref<SolveAstrometryInput> input,
                                                  Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<AstrometricSolution>>(
        context, adapter_failure<Ref<AstrometricSolution>>(Code::invalid_request,
                                                           "the typed input is null",
                                                           "solve_astrometry cannot run"));
  if (!plan)
    return rt::ready<Ref<AstrometricSolution>>(
        context, adapter_failure<Ref<AstrometricSolution>>(Code::invalid_request,
                                                           "no invocation plan supplied",
                                                           "solve_astrometry cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = astrometry_work(*input);
  spec.semantic = "sources.astrometry.solve_astrometry";
  return rt::submit_cpu<Ref<AstrometricSolution>>(
      context, spec,
      [input](rt::InvocationContext task) -> Result<Ref<AstrometricSolution>> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Ref<AstrometricSolution>>(
              bridge.code, "the science context bridge cannot be created",
              "solve_astrometry cannot run");
        return acsd::solve_astrometry((*bridge.value)->context(), input->observation,
                                      *input->catalogue, input->request);
      });
}

rt::Task<Ref<AstrometricSolution>> finalize_astrometry(rt::InvocationContext context,
                                                       Ref<SolveAstrometryInput>,
                                                       Ref<rt::InvocationPlan>,
                                                       Result<Ref<AstrometricSolution>> prior) {
  if (prior.code == Code::ok)
    return rt::ready<Ref<AstrometricSolution>>(context, std::move(prior));
  Result<Ref<AstrometricSolution>> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Ref<AstrometricSolution>>(context, std::move(kept));
}

}  // namespace

Result<Done> register_astrometry(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for("sources.astrometry");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the sources.astrometry descriptor is unavailable",
                                 "sources.astrometry cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the sources.astrometry process cannot be started",
                                 "sources.astrometry cannot be registered");
  const rt::Operation<SolveAstrometryInput, Ref<AstrometricSolution>> operation{
      "solve_astrometry",
      rt::TypeId{"acsd.input.SolveAstrometryInput", 1},
      rt::TypeId{"acsd.AstrometricSolution", 1},
      &validate_astrometry,
      &bounds_astrometry,
      &plan_astrometry,
      &run_astrometry,
      &finalize_astrometry};
  return builder.add<SolveAstrometryInput, Ref<AstrometricSolution>>(*process.value, operation);
}

// Plan introspection for validation/process/sources.astrometry.
Result<rt::InvocationPlan> plan_solve_astrometry_op(rt::InvocationContext ctx,
                                                    Ref<SolveAstrometryInput> input,
                                                    rt::PlanOptions options) {
  return plan_astrometry(ctx, input, options);
}

}  // namespace acsd::modules
