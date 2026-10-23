// sky.fit typed adapter: joint [H, A G] relative sky solve and its application.
//
// fit_relative_sky is a global barrier (gauge/nullspace, PSD/KKT joint solve,
// nullspace certificate); apply_sky is a bounded stream over the signal leaves.
// Both call the single src/sky implementation through the science bridge.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/sky.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

#include "acsd/runtime/diagnostics.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {
namespace {

using rt::Done;
using rt::Key;
using rt::PlanBounds;
using rt::PlanOptions;
using rt::ResourceVector;
using rt::StageSpec;

constexpr const char* kProcess = "sky.fit";
constexpr U64 kDefaultBlockElements = 65536;
constexpr U32 kMaxWindow = 2;
constexpr U64 kControlBytes = 8ull << 10;
constexpr U64 kWorkspaceUpper = 1ull << 20;

U64 sat_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b)
    return std::numeric_limits<U64>::max();
  return a * b;
}
U64 sat_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b)
    return std::numeric_limits<U64>::max();
  return a + b;
}

template <class T>
Result<T> failure(Code code, std::string origin, std::string reason,
                  std::string impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, std::move(reason), std::move(origin),
                   std::move(impact));
  return out;
}

template <class T>
Result<T> sky_failure(Code code, std::string reason, std::string impact) {
  return failure<T>(code, "sky.fit", std::move(reason), std::move(impact));
}

ResourceVector capacity(U64 bytes) {
  ResourceVector v;
  v.host_bytes = bytes;
  return v;
}

U64 elements_of(const ArrayRef& array) {
  if (!array) return 0;
  U64 total = 1;
  for (const U64 extent : array->shape) {
    if (extent == 0) return 0;
    total = sat_mul(total, extent);
    if (total == std::numeric_limits<U64>::max()) return total;
  }
  return total;
}

U64 action_domain(const Ref<LinearAction>& action) {
  if (!action || !action->spaces.domain) return 0;
  return action->spaces.domain->size;
}
U64 action_codomain(const Ref<LinearAction>& action) {
  if (!action || !action->spaces.codomain) return 0;
  return action->spaces.codomain->size;
}

U64 basis_terms(const Basis& basis) {
  switch (basis.kind) {
    case Basis::Kind::constant: return 1;
    case Basis::Kind::plane: return 3;
    case Basis::Kind::polynomial: {
      const U64 order = static_cast<U64>(basis.degree) + 1;
      return order * (order + 1) / 2;
    }
  }
  return 1;
}

bool usable_basis(const Basis& basis) {
  return std::isfinite(basis.scale[0]) && std::isfinite(basis.scale[1]) &&
         basis.scale[0] > 0 && basis.scale[1] > 0 &&
         std::isfinite(basis.center[0]) && std::isfinite(basis.center[1]);
}

U64 frame_count(const SkyRequest& request) {
  U64 total = 0;
  for (const Comparability& group : request.groups)
    total = sat_add(total, group.frames.size());
  return total;
}

Result<Done> validate_plan_options(const PlanOptions& options,
                                   const char* operation) {
  if (options.window == 0)
    return sky_failure<Done>(Code::invalid_request,
                             std::string(operation) + ": the plan window is zero",
                             "the sky operation cannot be scheduled");
  if (options.window > kMaxWindow)
    return sky_failure<Done>(Code::resource_exhausted,
                             std::string(operation) +
                                 ": the window exceeds the process maximum",
                             "the sky operation would exceed its frontier");
  return model::ok_result(Done{});
}

// ---------------------------------------------------------------------------
// fit_relative_sky
// ---------------------------------------------------------------------------

Result<Done> validate_fit(const Ref<FitRelativeSkyInput>& input) {
  if (!input) return sky_failure<Done>(Code::invalid_request,
                                       "the typed input is null",
                                       "the sky cannot be fitted");
  if (!input->controls)
    return sky_failure<Done>(Code::invalid_request, "field 'controls' is null",
                             "the sky cannot be fitted");
  const SkyControls& controls = *input->controls;
  if (!controls.c || !controls.component_ids)
    return sky_failure<Done>(Code::invalid_model,
                             "the sky controls have no control values or components",
                             "the sky cannot be fitted");
  if (!controls.T || !controls.H)
    return sky_failure<Done>(Code::invalid_model,
                             "the sky controls have no T/H operators",
                             "the sky cannot be fitted");
  const SkyRequest& request = input->request;
  if (!usable_basis(request.basis))
    return sky_failure<Done>(Code::invalid_model,
                             "the sky basis has a non-finite centre or scale",
                             "the sky cannot be fitted");
  const U64 frames = frame_count(request);
  if (frames == 0)
    return sky_failure<Done>(Code::invalid_request, "no frame is declared",
                             "the sky cannot be fitted");
  if (action_codomain(controls.T) != elements_of(controls.c) ||
      action_domain(controls.T) == 0) {
    return sky_failure<Done>(Code::invalid_model,
                             "the control operator has an inconsistent shape",
                             "the sky cannot be fitted");
  }
  if (action_codomain(controls.H) == 0 || action_domain(controls.H) == 0) {
    return sky_failure<Done>(Code::invalid_model,
                             "the common sky operator has no declared shape",
                             "the sky cannot be fitted");
  }
  if (request.gauge.kind == Gauge::Kind::weighted_sum &&
      !request.gauge.weights) {
    return sky_failure<Done>(Code::invalid_request,
                             "a weighted-sum gauge requires explicit weights",
                             "the sky gauge cannot be defined");
  }
  if (!controls.noise) {
    return sky_failure<Done>(Code::invalid_model,
                             "the sky controls have no control noise",
                             "the joint sky system cannot be weighted");
  }
  return model::ok_result(Done{});
}

// ---------------------------------------------------------------------------
// apply_sky
// ---------------------------------------------------------------------------

Result<Done> validate_apply(const Ref<ApplySkyInput>& input) {
  if (!input) return sky_failure<Done>(Code::invalid_request,
                                       "the typed input is null",
                                       "the sky cannot be applied");
  if (!input->signal || !input->fit)
    return sky_failure<Done>(Code::invalid_request,
                             "a signal and a sky fit are required",
                             "the sky cannot be applied");
  if (!input->signal->signal)
    return sky_failure<Done>(Code::invalid_model,
                             "the signal estimate has no signal array",
                             "the sky cannot be applied");
  if (elements_of(input->signal->signal) == 0)
    return sky_failure<Done>(Code::invalid_model,
                             "the signal estimate is empty",
                             "the sky cannot be applied");
  if (!input->fit->beta)
    return sky_failure<Done>(Code::invalid_model,
                             "the sky fit has no parameter array",
                             "the sky cannot be applied");
  return model::ok_result(Done{});
}

}  // namespace

Result<Done> validate_sky_fit(rt::InvocationContext,
                              Ref<FitRelativeSkyInput> input) {
  return validate_fit(input);
}

Result<PlanBounds> bounds_sky_fit(rt::InvocationContext,
                                 Ref<FitRelativeSkyInput> input) {
  Result<Done> validated = validate_fit(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = kDefaultBlockElements;
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_sky_fit(rt::InvocationContext,
                                        Ref<FitRelativeSkyInput> input,
                                        PlanOptions options) {
  Result<Done> validated = validate_fit(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok = validate_plan_options(options, "fit_relative_sky");
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const SkyRequest& request = input->request;
  const SkyControls& controls = *input->controls;
  const U64 frames = frame_count(request);
  const U64 terms = basis_terms(request.basis);
  const U64 parameters = sat_mul(frames, terms);
  const U64 controls_count = elements_of(controls.c);
  const U64 t_domain = action_domain(controls.T);
  const U64 h_rows = action_codomain(controls.H);
  const U64 h_domain = action_domain(controls.H);

  // Persistent: beta, K/T/E/P metadata, parameter covariance and the rank/
  // nullspace certificate.
  const U64 persistent =
      sat_add(sat_mul(parameters, 8 * 2),
              sat_add(sat_mul(parameters, 256),
                      sat_add(sat_mul(parameters, parameters), 1ull << 20)));
  // Barrier working set: dense T (M x t), H (h x cols), the joint normal
  // matrix (P+rows) and the augmented KKT system (2 x P).  This is the global
  // [H, A G] solve, not a per-leaf estimate.
  const U64 dense_t = sat_mul(controls_count, sat_mul(t_domain, 8));
  const U64 dense_h = sat_mul(h_rows, sat_mul(h_domain, 8));
  const U64 joint = sat_mul(sat_add(parameters, controls_count),
                            sat_mul(sat_add(parameters, controls_count), 8));
  const U64 progress_floor =
      sat_add(sat_mul(dense_t, 2),
              sat_add(sat_mul(dense_h, 2),
                      sat_add(joint, sat_add(kWorkspaceUpper, kControlBytes))));
  const U64 work_upper =
      sat_add(sat_mul(sat_mul(parameters + controls_count,
                              parameters + controls_count),
                      4),
              sat_mul(request.solve.max_iterations, sat_mul(parameters, 8)));

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements =
      std::max<U64>(1, options.block_elements == 0 ? parameters
                                                   : std::min<U64>(
                                                         parameters,
                                                         options.block_elements));
  plan.options.window = 1;
  plan.elements = controls_count;
  plan.block_elements = plan.options.block_elements;
  plan.cohorts = 1;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 16ull << 10;
  plan.cleanup_work_upper = 512;
  plan.adaptive = false;

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent = capacity(persistent);
  region.progress_floor = capacity(progress_floor);
  StageSpec solve;
  solve.id = 0;
  solve.scratch = capacity(progress_floor);
  solve.output_upper = capacity(sat_mul(parameters, 8 * 2));
  solve.control_bytes = kControlBytes;
  solve.work_upper = work_upper;
  region.stages.push_back(std::move(solve));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("controls", controls.identity.content_hash);
  model::Sha256 request_hash;
  request_hash.update_u64(frames);
  request_hash.update_u64(terms);
  request_hash.update_u64(controls_count);
  request_hash.field("gauge", request.gauge.reference);
  request_hash.update_u64(static_cast<U64>(request.gauge.kind));
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "fit_relative_sky/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<SkyFit>> run_sky_fit(rt::InvocationContext context,
                                  Ref<FitRelativeSkyInput> input,
                                  Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->controls) {
    return rt::ready<Ref<SkyFit>>(
        context, sky_failure<Ref<SkyFit>>(Code::invalid_request,
                                          "no plan, controls or request supplied",
                                          "the sky cannot be fitted"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 8 + 1024;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<SkyFit>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<SkyFit>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return sky_failure<Ref<SkyFit>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the sky cannot be fitted");
            }
            acsd::Context& science = (*bridge.value)->context();
            return fit_relative_sky(science, input->controls, input->request);
          }),
      [context, start_ns, span_id](Ref<SkyFit> fit) -> rt::Task<Ref<SkyFit>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "mosaic.sky.fit";
        record.kind = rt::PerfKind::span;
        record.start_steady_ns = start_ns;
        record.wall_ns = rt::steady_ns() - start_ns;
        record.span_id = span_id;
        record.tags.component = kProcess;
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<Ref<SkyFit>>(context, model::ok_result(std::move(fit)));
      });
}

rt::Task<Ref<SkyFit>> finalize_sky_fit(rt::InvocationContext context,
                                       Ref<FitRelativeSkyInput>,
                                       Ref<rt::InvocationPlan>,
                                       Result<Ref<SkyFit>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<SkyFit>>(
        context, sky_failure<Ref<SkyFit>>(Code::internal, "no sky fit was produced",
                                          "the sky fit cannot be published"));
  }
  return rt::ready<Ref<SkyFit>>(context, std::move(result));
}

// ---------------------------------------------------------------------------
// apply_sky
// ---------------------------------------------------------------------------

Result<Done> validate_sky_apply(rt::InvocationContext, Ref<ApplySkyInput> input) {
  return validate_apply(input);
}

Result<PlanBounds> bounds_sky_apply(rt::InvocationContext,
                                    Ref<ApplySkyInput> input) {
  Result<Done> validated = validate_apply(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  const U64 elements = elements_of(input->signal->signal);
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements =
      std::max<U64>(1, std::min<U64>(kDefaultBlockElements, elements));
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_sky_apply(rt::InvocationContext,
                                          Ref<ApplySkyInput> input,
                                          PlanOptions options) {
  Result<Done> validated = validate_apply(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok = validate_plan_options(options, "apply_sky");
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const U64 elements = elements_of(input->signal->signal);
  const U64 block = std::max<U64>(
      1, std::min<U64>(options.block_elements == 0 ? kDefaultBlockElements
                                                   : options.block_elements,
                       elements));
  const U64 cohorts = (elements + block - 1) / block;
  const U64 persistent = 1ull << 20;
  const U64 per_leaf = 8 * 3 + 1;
  const U64 progress_floor = sat_add(sat_mul(block, per_leaf), kWorkspaceUpper);
  const U64 work_upper = sat_add(sat_mul(elements, 32), 64);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = elements;
  plan.block_elements = block;
  plan.cohorts = cohorts;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 4ull << 10;
  plan.cleanup_work_upper = 64;
  plan.adaptive = false;

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = cohorts;
  region.persistent = capacity(persistent);
  region.progress_floor = capacity(progress_floor);
  StageSpec stage;
  stage.id = 0;
  stage.scratch = capacity(progress_floor);
  stage.output_upper = capacity(sat_mul(block, per_leaf));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("signal", input->signal->identity.content_hash);
  input_hash.field("fit", input->fit->identity.content_hash);
  model::Sha256 request_hash;
  request_hash.update_u64(elements);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "apply_sky/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<SignalEstimate>> run_sky_apply(rt::InvocationContext context,
                                            Ref<ApplySkyInput> input,
                                            Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->signal || !input->fit) {
    return rt::ready<Ref<SignalEstimate>>(
        context, sky_failure<Ref<SignalEstimate>>(
                     Code::invalid_request, "no plan, signal or fit supplied",
                     "the sky cannot be applied"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 4 + 64;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<SignalEstimate>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<SignalEstimate>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return sky_failure<Ref<SignalEstimate>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the sky cannot be applied");
            }
            acsd::Context& science = (*bridge.value)->context();
            return apply_sky(science, input->signal, input->fit);
          }),
      [context, start_ns, span_id](Ref<SignalEstimate> corrected)
          -> rt::Task<Ref<SignalEstimate>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "mosaic.sky.fit_component";
        record.kind = rt::PerfKind::span;
        record.start_steady_ns = start_ns;
        record.wall_ns = rt::steady_ns() - start_ns;
        record.span_id = span_id;
        record.tags.component = "apply_sky";
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<Ref<SignalEstimate>>(
            context, model::ok_result(std::move(corrected)));
      });
}

rt::Task<Ref<SignalEstimate>> finalize_sky_apply(
    rt::InvocationContext context, Ref<ApplySkyInput>, Ref<rt::InvocationPlan>,
    Result<Ref<SignalEstimate>> result) {
  return rt::ready<Ref<SignalEstimate>>(context, std::move(result));
}

Result<Done> register_sky_fit(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return sky_failure<Done>(descriptor.code,
                             "the generated descriptor is unavailable",
                             "sky.fit cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return sky_failure<Done>(registration.code, "the process cannot be registered",
                             "sky.fit is unavailable");
  }
  rt::Operation<FitRelativeSkyInput, Ref<SkyFit>> fit{
      "fit_relative_sky",
      {"acsd.input.FitRelativeSkyInput", 1},
      {"acsd.SkyFit", 1},
      &validate_sky_fit,
      &bounds_sky_fit,
      &plan_sky_fit,
      &run_sky_fit,
      &finalize_sky_fit};
  auto added = builder.add<FitRelativeSkyInput, Ref<SkyFit>>(
      *registration.value, std::move(fit));
  if (added.code != Code::ok) return added;

  rt::Operation<ApplySkyInput, Ref<SignalEstimate>> apply{
      "apply_sky",
      {"acsd.input.ApplySkyInput", 1},
      {"acsd.SignalEstimate", 1},
      &validate_sky_apply,
      &bounds_sky_apply,
      &plan_sky_apply,
      &run_sky_apply,
      &finalize_sky_apply};
  return builder.add<ApplySkyInput, Ref<SignalEstimate>>(
      *registration.value, std::move(apply));
}

}  // namespace acsd::modules
