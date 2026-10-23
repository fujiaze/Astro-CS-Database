// The sources.psf typed process adapter (M5).
//
// fit_psf, evaluate_psf and bind_observation_model call the unique
// implementations in src/sources and src/observation.  The adapter registers
// the typed operations; it repeats no PSF integration and creates no pool.
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

constexpr const char* kProcess = "sources.psf";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 64ull << 10;
constexpr U64 kCleanupWorkUpper = 32;
constexpr U64 kWorkspaceUpper = 2ull << 20;
constexpr U64 kMaxSourcePositions = 4096;

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

U64 window_samples(const PsfRequest& request) {
  double radius = request.window_radius_pixel;
  if (!(radius > 0.0)) radius = 8.0 * std::max(request.initial_width_pixel, 0.5);
  const double side = 2.0 * std::ceil(radius) + 1.0;
  return static_cast<U64>(std::max(1.0, side * side));
}

// Generic atomic plan used by every synchronous source entry point.
Result<rt::InvocationPlan> atomic_plan(rt::PlanOptions options, U64 elements, U64 persistent,
                                       U64 progress, U64 work_upper, const std::string& input_hash,
                                       const std::string& request_hash,
                                       const std::string& algorithm_hash) {
  if (elements == 0 || options.block_elements == 0 || options.block_elements > elements)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "the source step cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic source step uses one window",
                                               "the source step cannot be planned");
  if (persistent == std::numeric_limits<U64>::max() ||
      progress == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "the source step cannot be planned");
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
  plan.input_hash = input_hash;
  plan.request_hash = request_hash;
  plan.algorithm_hash = algorithm_hash;
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
  stage.work_upper = work_upper;
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

template <class Out>
rt::Task<Out> run_with_science(rt::InvocationContext context, Ref<rt::InvocationPlan> plan,
                               const char* semantic, U64 work_upper,
                               std::function<Result<Out>(acsd::Context&)> body) {
  if (!plan)
    return rt::ready<Out>(context, adapter_failure<Out>(Code::invalid_request,
                                                        "no invocation plan supplied",
                                                        "the source step cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = std::max<U64>(work_upper, 1);
  spec.semantic = semantic;
  return rt::submit_cpu<Out>(
      context, spec,
      [body = std::move(body)](rt::InvocationContext task) mutable -> Result<Out> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Out>(bridge.code, "the science context bridge cannot be created",
                                      "the source step cannot run");
        return body((*bridge.value)->context());
      });
}

template <class Out>
rt::Task<Out> passthrough_finalize(rt::InvocationContext context, Result<Out> prior) {
  if (prior.code == Code::ok) return rt::ready<Out>(context, std::move(prior));
  Result<Out> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Out>(context, std::move(kept));
}

// ---------------------------------------------------------------------------
// fit_psf
// ---------------------------------------------------------------------------

Result<Done> require_fit(Ref<FitPsfInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "fit_psf cannot be validated");
  if (!input->observation || !input->observation->footprints ||
      !input->observation->footprints->index)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the observation has no native footprint",
                                 "fit_psf cannot be validated");
  if (!input->wcs)
    return adapter_failure<Done>(Code::invalid_request, "no frame model supplied",
                                 "fit_psf cannot be validated");
  if (!input->stars || input->stars->empty())
    return adapter_failure<Done>(Code::invalid_request, "no catalogue star supplied",
                                 "fit_psf cannot be validated");
  if (input->request.spatial_degree > 2)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the first release supports a spatial degree of at most two",
                                 "fit_psf cannot be validated");
  if (!(input->request.initial_width_pixel > 0.0))
    return adapter_failure<Done>(Code::invalid_request, "the initial width must be positive",
                                 "fit_psf cannot be validated");
  if (!std::isfinite(input->request.window_radius_pixel) ||
      input->request.window_radius_pixel < 0.0)
    return adapter_failure<Done>(Code::invalid_request, "the window radius is invalid",
                                 "fit_psf cannot be validated");
  if (!std::isfinite(input->request.tail_absolute_flux) ||
      input->request.tail_absolute_flux < 0.0)
    return adapter_failure<Done>(Code::invalid_request, "the tail flux bound is invalid",
                                 "fit_psf cannot be validated");
  if (input->wcs->detector_frame != input->observation->footprints->detector_frame)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the frame model and the footprint set use different detectors",
                                 "fit_psf cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_fit(rt::InvocationContext, Ref<FitPsfInput> input) { return require_fit(input); }

U64 fit_work(const FitPsfInput& input) {
  const U64 stars = input.stars ? input.stars->size() : 0;
  const U64 samples = window_samples(input.request);
  const U64 template_work = saturating_mul(std::max<U64>(stars, 1), std::max<U64>(samples, 1));
  // Every template evaluation charges samples*sources; the bounded iteration
  // count is the multiplier of that charge.
  // Declared conservative bound: the template evaluation charge times the
  // bounded iteration count, with a floor that covers the finite-difference
  // and SVD probe passes of the first implementation.
  const U64 bounded =
      saturating_mul(template_work,
                     saturating_add(static_cast<U64>(input.request.budget.max_iterations), 2));
  return std::max<U64>(bounded, 8ull << 20);
}

Result<rt::PlanBounds> bounds_fit(rt::InvocationContext, Ref<FitPsfInput> input) {
  Result<Done> validation = require_fit(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the PSF fit input is invalid",
                                           "fit_psf cannot be planned");
  const U64 elements = std::max<U64>(input->stars->size(), 1);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_fit(rt::InvocationContext, Ref<FitPsfInput> input,
                                    rt::PlanOptions options) {
  Result<Done> validation = require_fit(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the PSF fit input is invalid",
                                               "fit_psf cannot be planned");
  const U64 stars = input->stars->size();
  const U64 samples = window_samples(input->request);
  const U64 native = input->observation->native_index
                         ? input->observation->native_index->size
                         : element_count(input->observation->integrated_values);
  const U64 window_payload = saturating_mul(16, saturating_mul(stars, samples));
  const U64 progress = saturating_add(
      saturating_add(saturating_mul(26, native), window_payload),
      saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes, kStageControlBytes)));
  const U64 persistent = saturating_add(saturating_mul(64, stars), 256ull << 10);
  return atomic_plan(options, std::max<U64>(stars, 1), persistent, progress, fit_work(*input),
                     hex_id("native|" + input->observation->identity.content_hash + "|stars|" +
                            std::to_string(stars) + "|wcs|" + input->wcs->identity.content_hash +
                            "|request|" + psf_text(input->request)),
                     hex_id("stars|" + std::to_string(stars)),
                     hex_id("sources.psf|fit_psf|pixel_integral_v1|1"));
}

rt::Task<Ref<EffectivePSF>> run_fit(rt::InvocationContext context, Ref<FitPsfInput> input,
                                    Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<EffectivePSF>>(context,
                                        adapter_failure<Ref<EffectivePSF>>(
                                            Code::invalid_request, "the typed input is null",
                                            "fit_psf cannot run"));
  return run_with_science<Ref<EffectivePSF>>(
      context, plan, "sources.psf.fit_psf", fit_work(*input),
      [input](acsd::Context& science) -> Result<Ref<EffectivePSF>> {
        return acsd::fit_psf(science, input->observation, input->wcs,
                             std::span<const CatalogueStar>(input->stars->data(),
                                                            input->stars->size()),
                             input->request);
      });
}

rt::Task<Ref<EffectivePSF>> finalize_fit(rt::InvocationContext context, Ref<FitPsfInput>,
                                         Ref<rt::InvocationPlan>, Result<Ref<EffectivePSF>> prior) {
  return passthrough_finalize<Ref<EffectivePSF>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// evaluate_psf
// ---------------------------------------------------------------------------

Result<Done> require_evaluate(Ref<EvaluatePsfInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "evaluate_psf cannot be validated");
  if (!input->psf)
    return adapter_failure<Done>(Code::invalid_request, "no PSF supplied",
                                 "evaluate_psf cannot be validated");
  if (!input->psf->detector_samples || !input->psf->detector_samples->index)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the PSF has no native sample footprint",
                                 "evaluate_psf cannot be validated");
  if (input->request.source_positions.empty())
    return adapter_failure<Done>(Code::invalid_request, "no source position requested",
                                 "evaluate_psf cannot be validated");
  if (input->request.source_positions.size() > kMaxSourcePositions)
    return adapter_failure<Done>(Code::invalid_request,
                                 "too many source positions in one request",
                                 "evaluate_psf cannot be validated");
  for (const PixelCenter0& point : input->request.source_positions)
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
      return adapter_failure<Done>(Code::invalid_request, "a source position is not finite",
                                   "evaluate_psf cannot be validated");
  if (!std::isfinite(input->request.budget.tolerance.absolute) ||
      input->request.budget.arithmetic.initial_bits == 0)
    return adapter_failure<Done>(Code::invalid_request, "the numeric budget is invalid",
                                 "evaluate_psf cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_evaluate(rt::InvocationContext, Ref<EvaluatePsfInput> input) {
  return require_evaluate(input);
}

U64 evaluate_elements(const EvaluatePsfInput& input) {
  return std::max<U64>(input.request.source_positions.size(), 1);
}

U64 evaluate_work(const EvaluatePsfInput& input) {
  const U64 positions = evaluate_elements(input);
  const U64 rows = std::max<U64>(input.request.output_rows.size(), 1);
  // The template evaluation charges every native sample per source; keep a
  // conservative floor for the bounded numeric probes around it.
  return std::max<U64>(saturating_mul(positions, rows), 1ull << 20);
}

Result<rt::PlanBounds> bounds_evaluate(rt::InvocationContext, Ref<EvaluatePsfInput> input) {
  Result<Done> validation = require_evaluate(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the PSF evaluation input is invalid",
                                           "evaluate_psf cannot be planned");
  const U64 elements = evaluate_elements(*input);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, kMaxSourcePositions);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_evaluate(rt::InvocationContext, Ref<EvaluatePsfInput> input,
                                         rt::PlanOptions options) {
  Result<Done> validation = require_evaluate(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code,
                                               "the PSF evaluation input is invalid",
                                               "evaluate_psf cannot be planned");
  const U64 positions = evaluate_elements(*input);
  const U64 rows = std::max<U64>(input->request.output_rows.size(), 1);
  const U64 native = input->psf->detector_samples->index->size;
  const U64 output = saturating_mul(24, saturating_mul(positions, rows));
  const U64 progress = saturating_add(
      saturating_add(saturating_mul(26, native), output),
      saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes, kStageControlBytes)));
  const U64 persistent = saturating_add(output, 256ull << 10);
  return atomic_plan(options, positions, persistent, progress, evaluate_work(*input),
                     hex_id("psf|" + input->psf->identity.content_hash + "|positions|" +
                            std::to_string(positions) + "|rows|" + std::to_string(rows)),
                     hex_id("budget|" + budget_text(input->request.budget)),
                     hex_id("sources.psf|evaluate_psf|pixel_integral_v1|1"));
}

rt::Task<Ref<PsfEvaluation>> run_evaluate(rt::InvocationContext context,
                                          Ref<EvaluatePsfInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<PsfEvaluation>>(context,
                                         adapter_failure<Ref<PsfEvaluation>>(
                                             Code::invalid_request, "the typed input is null",
                                             "evaluate_psf cannot run"));
  return run_with_science<Ref<PsfEvaluation>>(
      context, plan, "sources.psf.evaluate_psf", evaluate_work(*input),
      [input](acsd::Context& science) -> Result<Ref<PsfEvaluation>> {
        return acsd::evaluate_psf(science, input->psf, input->request);
      });
}

rt::Task<Ref<PsfEvaluation>> finalize_evaluate(rt::InvocationContext context,
                                               Ref<EvaluatePsfInput>, Ref<rt::InvocationPlan>,
                                               Result<Ref<PsfEvaluation>> prior) {
  return passthrough_finalize<Ref<PsfEvaluation>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// bind_observation_model
// ---------------------------------------------------------------------------

Result<Done> validate_bind_model(rt::InvocationContext,
                                 Ref<BindObservationModelInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "bind_observation_model cannot be validated");
  if (!input->observation)
    return adapter_failure<Done>(Code::invalid_request, "no observation supplied",
                                 "bind_observation_model cannot be validated");
  if (!input->kernel)
    return adapter_failure<Done>(Code::invalid_request, "no PSF kernel supplied",
                                 "bind_observation_model cannot be validated");
  if (!input->response)
    return adapter_failure<Done>(Code::invalid_request, "no response supplied",
                                 "bind_observation_model cannot be validated");
  if (!input->observation->footprints)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the observation has no detector footprint",
                                 "bind_observation_model cannot be validated");
  if (input->kernel->frame != input->observation->frame)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the PSF kernel belongs to another frame",
                                 "bind_observation_model cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_bind_model(rt::InvocationContext,
                                         Ref<BindObservationModelInput> input) {
  Result<Done> validation = validate_bind_model({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code,
                                           "the observation model input is invalid",
                                           "bind_observation_model cannot be planned");
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = 1;
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_bind_model(rt::InvocationContext,
                                           Ref<BindObservationModelInput> input,
                                           rt::PlanOptions options) {
  Result<Done> validation = validate_bind_model({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code,
                                               "the observation model input is invalid",
                                               "bind_observation_model cannot be planned");
  return atomic_plan(options, 1, 128ull << 10, 1ull << 20, 1,
                     hex_id("observation|" + input->observation->identity.content_hash +
                            "|kernel|" + input->kernel->identity.content_hash + "|response|" +
                            input->response->identity.content_hash),
                     hex_id("request|bind_observation_model"),
                     hex_id("sources.psf|bind_observation_model|1"));
}

rt::Task<Ref<NativeObservation>> run_bind_model(rt::InvocationContext context,
                                                Ref<BindObservationModelInput> input,
                                                Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<NativeObservation>>(
        context, adapter_failure<Ref<NativeObservation>>(Code::invalid_request,
                                                         "the typed input is null",
                                                         "bind_observation_model cannot run"));
  return run_with_science<Ref<NativeObservation>>(
      context, plan, "sources.psf.bind_observation_model", 1,
      [input](acsd::Context& science) -> Result<Ref<NativeObservation>> {
        return acsd::bind_observation_model(science, input->observation, input->kernel,
                                            input->response);
      });
}

rt::Task<Ref<NativeObservation>> finalize_bind_model(rt::InvocationContext context,
                                                     Ref<BindObservationModelInput>,
                                                     Ref<rt::InvocationPlan>,
                                                     Result<Ref<NativeObservation>> prior) {
  return passthrough_finalize<Ref<NativeObservation>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------

template <class In, class Out>
Result<Done> add_operation(rt::RegistryBuilder& builder,
                           const std::shared_ptr<rt::ProcessRegistration>& process,
                           const char* operation_id, const char* input_schema,
                           const char* output_schema,
                           Result<Done> (*validate)(rt::InvocationContext, Ref<In>),
                           Result<rt::PlanBounds> (*bounds)(rt::InvocationContext, Ref<In>),
                           Result<rt::InvocationPlan> (*plan)(rt::InvocationContext, Ref<In>,
                                                              rt::PlanOptions),
                           rt::Task<Out> (*run)(rt::InvocationContext, Ref<In>,
                                                Ref<rt::InvocationPlan>),
                           rt::Task<Out> (*finalize)(rt::InvocationContext, Ref<In>,
                                                     Ref<rt::InvocationPlan>, Result<Out>)) {
  const rt::Operation<In, Out> operation{operation_id, rt::TypeId{input_schema, 1},
                                         rt::TypeId{output_schema, 1}, validate, bounds, plan, run,
                                         finalize};
  return builder.add<In, Out>(process, operation);
}

}  // namespace

Result<Done> register_psf(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor = acsd::generated::descriptor_for("sources.psf");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the sources.psf descriptor is unavailable",
                                 "sources.psf cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the sources.psf process cannot be started",
                                 "sources.psf cannot be registered");
  Result<Done> step;
  step = add_operation<FitPsfInput, Ref<EffectivePSF>>(
      builder, *process.value, "fit_psf", "acsd.input.FitPsfInput", "acsd.EffectivePSF",
      &validate_fit, &bounds_fit, &plan_fit, &run_fit, &finalize_fit);
  if (!ok(step)) return step;
  step = add_operation<EvaluatePsfInput, Ref<PsfEvaluation>>(
      builder, *process.value, "evaluate_psf", "acsd.input.EvaluatePsfInput",
      "acsd.PsfEvaluation", &validate_evaluate, &bounds_evaluate, &plan_evaluate, &run_evaluate,
      &finalize_evaluate);
  if (!ok(step)) return step;
  return add_operation<BindObservationModelInput, Ref<NativeObservation>>(
      builder, *process.value, "bind_observation_model",
      "acsd.input.BindObservationModelInput", "acsd.NativeObservation", &validate_bind_model,
      &bounds_bind_model, &plan_bind_model, &run_bind_model, &finalize_bind_model);
}

// Plan introspection for validation/process/sources.psf.
Result<rt::InvocationPlan> plan_fit_psf_op(rt::InvocationContext ctx, Ref<FitPsfInput> input,
                                           rt::PlanOptions options) {
  return plan_fit(ctx, input, options);
}
Result<rt::InvocationPlan> plan_evaluate_psf_op(rt::InvocationContext ctx,
                                                Ref<EvaluatePsfInput> input,
                                                rt::PlanOptions options) {
  return plan_evaluate(ctx, input, options);
}
Result<rt::InvocationPlan> plan_bind_observation_model_op(
    rt::InvocationContext ctx, Ref<BindObservationModelInput> input, rt::PlanOptions options) {
  return plan_bind_model(ctx, input, options);
}

}  // namespace acsd::modules
