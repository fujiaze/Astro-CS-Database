// The sources.measure typed process adapter (M5).
//
// measure_science_sources is the unique synchronous implementation.  The
// adapter registers it and sizes the atomic resource model from the actual
// star/window/array metadata; it repeats no measurement formula.
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

constexpr const char* kProcess = "sources.measure";
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

std::string budget_text(const NumericBudget& budget) {
  return number_text(budget.tolerance.absolute) + "|" +
         number_text(budget.tolerance.relative) + "|" +
         number_text(budget.tolerance.absolute_floor) + "|" + budget.tolerance.unit.scale + "|" +
         std::to_string(budget.max_iterations) + "|" +
         std::to_string(budget.arithmetic.initial_bits) + "|" +
         std::to_string(budget.arithmetic.max_bits);
}

std::string psf_text(const PsfRequest& request) {
  return std::to_string(static_cast<int>(request.family)) + "|" +
         std::to_string(request.spatial_degree) + "|" +
         number_text(request.initial_width_pixel) + "|" +
         number_text(request.tail_absolute_flux) + "|" +
         number_text(request.window_radius_pixel) + "|" + budget_text(request.budget);
}

Result<Done> require_input(Ref<MeasureScienceSourcesInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "measure_science_sources cannot be validated");
  if (!input->observation || !input->observation->native_index)
    return adapter_failure<Done>(Code::invalid_request, "no observation supplied",
                                 "measure_science_sources cannot be validated");
  if (!input->wcs)
    return adapter_failure<Done>(Code::invalid_request, "no frame model supplied",
                                 "measure_science_sources cannot be validated");
  if (!input->psf)
    return adapter_failure<Done>(Code::invalid_request, "no PSF supplied",
                                 "measure_science_sources cannot be validated");
  if (!input->stars || input->stars->empty())
    return adapter_failure<Done>(Code::invalid_request, "no catalogue star supplied",
                                 "measure_science_sources cannot be validated");
  if (!input->observation->footprints || !input->observation->footprints->index)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the observation has no native footprint",
                                 "measure_science_sources cannot be validated");
  if (!input->observation->integrated_values || !input->observation->dq)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the observation has no values or DQ array",
                                 "measure_science_sources cannot be validated");
  if (input->observation->native_index->size !=
      element_count(input->observation->integrated_values))
    return adapter_failure<Done>(Code::invalid_model,
                                 "the native arrays do not match the native index",
                                 "measure_science_sources cannot be validated");
  if (input->request.spatial_degree > 2)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the first release supports a spatial degree of at most two",
                                 "measure_science_sources cannot be validated");
  if (!(input->request.initial_width_pixel > 0.0))
    return adapter_failure<Done>(Code::invalid_request, "the initial width must be positive",
                                 "measure_science_sources cannot be validated");
  if (!std::isfinite(input->request.window_radius_pixel) ||
      input->request.window_radius_pixel < 0.0)
    return adapter_failure<Done>(Code::invalid_request, "the window radius is invalid",
                                 "measure_science_sources cannot be validated");
  if (input->wcs->detector_frame != input->observation->footprints->detector_frame)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the frame model and the footprint set use different detectors",
                                 "measure_science_sources cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_measure(rt::InvocationContext, Ref<MeasureScienceSourcesInput> input) {
  return require_input(input);
}

U64 window_samples(const PsfRequest& request) {
  double radius = request.window_radius_pixel;
  if (!(radius > 0.0)) radius = 8.0 * std::max(request.initial_width_pixel, 0.5);
  const double side = 2.0 * std::ceil(radius) + 1.0;
  return static_cast<U64>(std::max(1.0, side * side));
}

U64 measure_elements(const MeasureScienceSourcesInput& input) {
  return std::max<U64>(input.stars ? input.stars->size() : 0, 1);
}

U64 measure_work(const MeasureScienceSourcesInput& input) {
  const U64 stars = measure_elements(input);
  const U64 samples = window_samples(input.request);
  const U64 bounded =
      saturating_mul(saturating_mul(stars, samples),
                     saturating_add(static_cast<U64>(input.request.budget.max_iterations), 2));
  return std::max<U64>(bounded, 1ull << 20);
}

Result<rt::PlanBounds> bounds_measure(rt::InvocationContext, Ref<MeasureScienceSourcesInput> input) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the measurement input is invalid",
                                           "measure_science_sources cannot be planned");
  const U64 elements = measure_elements(*input);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_measure(rt::InvocationContext, Ref<MeasureScienceSourcesInput> input,
                                        rt::PlanOptions options) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the measurement input is invalid",
                                               "measure_science_sources cannot be planned");
  if (options.block_elements == 0 || options.block_elements > measure_elements(*input))
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "measure_science_sources cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic measurement uses one window",
                                               "measure_science_sources cannot be planned");
  const U64 stars = measure_elements(*input);
  const U64 native = input->observation->native_index->size;
  const U64 samples = window_samples(input->request);
  const U64 design = saturating_mul(16, saturating_mul(stars, samples));
  const U64 progress = saturating_add(
      saturating_add(saturating_mul(26, native), design),
      saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes, kStageControlBytes)));
  const U64 persistent = saturating_add(saturating_mul(96, stars), 256ull << 10);
  const U64 work = measure_work(*input);
  if (progress == std::numeric_limits<U64>::max() ||
      persistent == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "measure_science_sources cannot be planned");

  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = stars;
  plan.block_elements = options.block_elements;
  plan.cohorts = 1;
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = saturating_add(kPlanControlBytes, kCohortControlBytes);
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = hex_id("native|" + input->observation->identity.content_hash + "|stars|" +
                           std::to_string(stars) + "|wcs|" + input->wcs->identity.content_hash +
                           "|psf|" + input->psf->identity.content_hash);
  plan.request_hash = hex_id("request|" + psf_text(input->request));
  plan.algorithm_hash = hex_id("sources.measure|measure_science_sources|joint_window_v1|1");
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

rt::Task<Ref<SourceMeasurements>> run_measure(rt::InvocationContext context,
                                              Ref<MeasureScienceSourcesInput> input,
                                              Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<SourceMeasurements>>(
        context, adapter_failure<Ref<SourceMeasurements>>(Code::invalid_request,
                                                          "the typed input is null",
                                                          "measure_science_sources cannot run"));
  if (!plan)
    return rt::ready<Ref<SourceMeasurements>>(
        context, adapter_failure<Ref<SourceMeasurements>>(Code::invalid_request,
                                                          "no invocation plan supplied",
                                                          "measure_science_sources cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = measure_work(*input);
  spec.semantic = "sources.measure.measure_science_sources";
  return rt::submit_cpu<Ref<SourceMeasurements>>(
      context, spec,
      [input](rt::InvocationContext task) -> Result<Ref<SourceMeasurements>> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Ref<SourceMeasurements>>(
              bridge.code, "the science context bridge cannot be created",
              "measure_science_sources cannot run");
        return acsd::measure_science_sources(
            (*bridge.value)->context(), input->observation, input->wcs,
            std::span<const CatalogueStar>(input->stars->data(), input->stars->size()),
            input->psf, input->request);
      });
}

rt::Task<Ref<SourceMeasurements>> finalize_measure(rt::InvocationContext context,
                                                   Ref<MeasureScienceSourcesInput>,
                                                   Ref<rt::InvocationPlan>,
                                                   Result<Ref<SourceMeasurements>> prior) {
  if (prior.code == Code::ok) return rt::ready<Ref<SourceMeasurements>>(context, std::move(prior));
  Result<Ref<SourceMeasurements>> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Ref<SourceMeasurements>>(context, std::move(kept));
}

}  // namespace

Result<Done> register_measure(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor = acsd::generated::descriptor_for("sources.measure");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the sources.measure descriptor is unavailable",
                                 "sources.measure cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the sources.measure process cannot be started",
                                 "sources.measure cannot be registered");
  const rt::Operation<MeasureScienceSourcesInput, Ref<SourceMeasurements>> operation{
      "measure_science_sources",
      rt::TypeId{"acsd.input.MeasureScienceSourcesInput", 1},
      rt::TypeId{"acsd.SourceMeasurements", 1},
      &validate_measure,
      &bounds_measure,
      &plan_measure,
      &run_measure,
      &finalize_measure};
  return builder.add<MeasureScienceSourcesInput, Ref<SourceMeasurements>>(*process.value,
                                                                         operation);
}

// Plan introspection for validation/process/sources.measure.
Result<rt::InvocationPlan> plan_measure_science_sources_op(
    rt::InvocationContext ctx, Ref<MeasureScienceSourcesInput> input, rt::PlanOptions options) {
  return plan_measure(ctx, input, options);
}

}  // namespace acsd::modules
