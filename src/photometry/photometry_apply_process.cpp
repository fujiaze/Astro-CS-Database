// photometry.apply typed adapter: one stream region over the native samples.
//
// The adapter validates the PhotoSolution/NativeObservation identity, publishes
// the per-sample resource plan and calls the single apply_scale implementation
// through the science Context bridge.  alpha(u)=exp(-h theta) is applied once;
// a repeated application_id is rejected before any block is produced.
#include <acsd/modules/photometry.hpp>
#include <acsd/modules/generated/status.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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

constexpr const char* kOrigin = "photometry.apply";
constexpr const char* kProcess = "photometry.apply";
constexpr const char* kOperation = "apply_scale";
constexpr U64 kDefaultBlockElements = 65536;
constexpr U32 kMaxWindow = 4;
constexpr U64 kControlBytes = 8ull << 10;
constexpr U64 kWorkspaceUpper = 1ull << 20;
constexpr U64 kCleanupBytes = 16ull << 10;
constexpr U64 kCleanupWork = 2048;

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
Result<T> failure(Code code, std::string reason, std::string impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, std::move(reason), kOrigin,
                   std::move(impact));
  return out;
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

Result<Done> validate_metadata(const Ref<ApplyScaleInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the scale cannot be applied");
  if (!input->solution)
    return failure<Done>(Code::invalid_request, "field 'solution' is null",
                         "the scale cannot be applied");
  if (!input->observation)
    return failure<Done>(Code::invalid_request, "field 'observation' is null",
                         "the scale cannot be applied");
  if (input->frame.empty())
    return failure<Done>(Code::invalid_request, "field 'frame' is empty",
                         "the scale cannot be applied");
  const PhotoSolution& solution = *input->solution;
  const NativeObservation& observation = *input->observation;
  if (solution.application_id.empty()) {
    return failure<Done>(Code::invalid_model,
                         "the photometric solution has no application id",
                         "the scale cannot be applied");
  }
  if (std::find(solution.frames.begin(), solution.frames.end(), input->frame) ==
      solution.frames.end()) {
    return failure<Done>(Code::invalid_request,
                         "the requested frame is not part of the photometric solution",
                         "the scale cannot be applied");
  }
  if (observation.frame != input->frame) {
    return failure<Done>(Code::invalid_request,
                         "the observation does not belong to the requested frame",
                         "the scale cannot be applied");
  }
  if (std::find(observation.scale_applications.begin(),
                observation.scale_applications.end(),
                solution.application_id) !=
      observation.scale_applications.end()) {
    return failure<Done>(Code::identity_conflict,
                         "the photometric scale was already applied to this observation",
                         "the scale cannot be applied twice");
  }
  if (!observation.footprints) {
    return failure<Done>(Code::invalid_model, "the observation has no footprints",
                         "the scale cannot be applied");
  }
  if (!observation.integrated_values || !observation.dq ||
      !observation.native_index) {
    return failure<Done>(Code::invalid_model,
                         "the observation has no values, quality or native index",
                         "the scale cannot be applied");
  }
  if (!solution.theta) {
    return failure<Done>(Code::invalid_model,
                         "the photometric solution has no parameter array",
                         "the scale cannot be applied");
  }
  const U64 terms = basis_terms(solution.spatial_basis);
  const U64 frames = solution.frames.size();
  const U64 theta_count = elements_of(solution.theta);
  if (theta_count < sat_mul(frames, terms)) {
    return failure<Done>(Code::invalid_model,
                         "the parameter array does not cover frames and basis terms",
                         "the scale cannot be applied");
  }
  const U64 samples = elements_of(observation.integrated_values);
  if (samples == 0) {
    return failure<Done>(Code::invalid_model, "the observation has no native samples",
                         "the scale cannot be applied");
  }
  if (observation.native_index->size != samples) {
    return failure<Done>(Code::invalid_model,
                         "the native index does not cover the observation samples",
                         "the scale cannot be applied");
  }
  if (elements_of(observation.dq) != samples) {
    return failure<Done>(Code::invalid_model,
                         "the data quality does not match the native samples",
                         "the scale cannot be applied");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the scale cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the scale would exceed its bounded frontier");
  return model::ok_result(Done{});
}

Result<rt::InvocationPlan> build_plan(const Ref<ApplyScaleInput>& input,
                                      PlanOptions options) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok = validate_plan_options(options);
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const PhotoSolution& solution = *input->solution;
  const NativeObservation& observation = *input->observation;
  const U64 elements = elements_of(observation.integrated_values);
  const U64 block =
      std::max<U64>(1, std::min<U64>(options.block_elements == 0
                                         ? kDefaultBlockElements
                                         : options.block_elements,
                                     elements));
  const U64 cohorts = (elements + block - 1) / block;
  const U64 terms = basis_terms(solution.spatial_basis);
  const U64 frames = solution.frames.size();

  // Persistent: the parameter box/alpha metadata and the solution dependency.
  const U64 persistent = sat_add(sat_mul(sat_mul(frames, terms), 8 * 2),
                                 sat_add(terms * 8, 64ull << 10));
  // Per cohort: input values + DQ + numerical bounds + output values/lower/
  // upper/DQ and the alpha scratch.
  const U64 per_sample = sat_add(8 * 3, 2 + 8 * 3);  // in 26 + out 26
  const U64 progress_floor =
      sat_add(sat_mul(block, per_sample), sat_add(kWorkspaceUpper, kControlBytes));
  const U64 work_upper = sat_add(sat_mul(elements, per_sample + 8),
                                 sat_mul(cohorts, 64));

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
  plan.elastic = capacity(sat_mul(block, per_sample));
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWork;
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
  stage.output_upper = capacity(sat_mul(block, 8 * 3 + 2));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("solution", solution.identity.content_hash);
  input_hash.field("observation", observation.identity.content_hash);
  input_hash.field("frame", input->frame);
  input_hash.update_u64(frames);
  model::Sha256 request_hash;
  request_hash.field("application_id", solution.application_id);
  request_hash.update_u64(elements);
  request_hash.update_u64(terms);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "apply_scale/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

}  // namespace

Result<Done> validate_photometry_apply(rt::InvocationContext,
                                       Ref<ApplyScaleInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_photometry_apply(rt::InvocationContext,
                                           Ref<ApplyScaleInput> input) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  const U64 elements = elements_of(input->observation->integrated_values);
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements =
      std::max<U64>(1, std::min<U64>(kDefaultBlockElements, elements));
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_photometry_apply(rt::InvocationContext,
                                                 Ref<ApplyScaleInput> input,
                                                 PlanOptions options) {
  return build_plan(input, options);
}

rt::Task<Ref<NativeObservation>> run_photometry_apply(
    rt::InvocationContext context, Ref<ApplyScaleInput> input,
    Ref<rt::InvocationPlan> plan) {
  if (!plan || !input) {
    return rt::ready<Ref<NativeObservation>>(
        context, failure<Ref<NativeObservation>>(
                     Code::invalid_request, "no plan or input supplied",
                     "the scale cannot be applied"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 8 + plan->cohorts + 1;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<NativeObservation>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<NativeObservation>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Ref<NativeObservation>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the scale cannot be applied");
            }
            acsd::Context& science = (*bridge.value)->context();
            return apply_scale(science, input->solution, input->observation,
                               input->frame);
          }),
      [context, start_ns, span_id](
          Ref<NativeObservation> scaled) -> rt::Task<Ref<NativeObservation>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "photometry.apply.signal";
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
        return rt::ready<Ref<NativeObservation>>(
            context, model::ok_result(std::move(scaled)));
      });
}

rt::Task<Ref<NativeObservation>> finalize_photometry_apply(
    rt::InvocationContext context, Ref<ApplyScaleInput>, Ref<rt::InvocationPlan>,
    Result<Ref<NativeObservation>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<NativeObservation>>(
        context, failure<Ref<NativeObservation>>(
                     Code::internal, "the scale returned no observation",
                     "the scaled observation cannot be published"));
  }
  return rt::ready<Ref<NativeObservation>>(context, std::move(result));
}

Result<Done> register_photometry_apply(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "photometry.apply cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "photometry.apply is unavailable");
  }
  rt::Operation<ApplyScaleInput, Ref<NativeObservation>> operation{
      kOperation,
      {"acsd.input.ApplyScaleInput", 1},
      {"acsd.NativeObservation", 1},
      &validate_photometry_apply,
      &bounds_photometry_apply,
      &plan_photometry_apply,
      &run_photometry_apply,
      &finalize_photometry_apply};
  return builder.add<ApplyScaleInput, Ref<NativeObservation>>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
