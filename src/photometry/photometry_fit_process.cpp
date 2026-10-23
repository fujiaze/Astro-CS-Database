// photometry.fit typed adapter: one joint flux-domain Gaussian barrier.
//
// The adapter validates metadata, publishes the real joint-latent plan and runs
// the single existing fit_photometry implementation through the science
// Context bridge.  No photometry formula is repeated here.
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

constexpr const char* kOrigin = "photometry.fit";
constexpr const char* kProcess = "photometry.fit";
constexpr const char* kOperation = "fit_photometry";
constexpr U64 kDefaultBlockElements = 1024;
constexpr U32 kMaxWindow = 4;
constexpr U64 kControlBytes = 8ull << 10;
constexpr U64 kWorkspaceUpper = 1ull << 20;
constexpr U64 kCleanupBytes = 32ull << 10;
constexpr U64 kCleanupWork = 4096;

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

std::string array_key(const ArrayRef& array) {
  if (!array) return "<absent>";
  std::string text = array->identity.content_hash + "|" + array->index_id + "|" +
                     array->unit.scale + "|" +
                     std::to_string(array->unit.linear_power) + "|";
  for (const U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

std::string ref_key(const Ref<Response>& response) {
  return response ? response->identity.content_hash : std::string("<absent>");
}

U64 source_count(const Ref<FitPhotometryInput>& input) {
  if (!input || !input->measurements) return 0;
  U64 total = 0;
  for (const Ref<SourceMeasurements>& measurement : *input->measurements) {
    if (!measurement) continue;
    total = sat_add(total, measurement->stars.size());
  }
  return total;
}

Result<Done> validate_metadata(const Ref<FitPhotometryInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the photometry fit cannot run");
  if (!input->measurements)
    return failure<Done>(Code::invalid_request, "field 'measurements' is null",
                         "the photometry fit cannot run");
  if (!input->stars)
    return failure<Done>(Code::invalid_request, "field 'stars' is null",
                         "the photometry fit cannot run");
  if (!input->predictions)
    return failure<Done>(Code::invalid_request, "field 'predictions' is null",
                         "the photometry fit cannot run");
  const PhotometryRequest& request = input->request;
  const std::size_t k = request.frames.size();
  if (k == 0 || input->measurements->empty() || input->predictions->empty() ||
      input->stars->empty()) {
    return failure<Done>(Code::invalid_request,
                         "the joint fit needs at least one frame, measurement, star and prediction",
                         "the photometry fit cannot run");
  }
  if (input->measurements->size() != k || input->predictions->size() != k ||
      request.responses.size() != k) {
    return failure<Done>(
        Code::invalid_request,
        "frames, responses, measurements and predictions must have the same length",
        "the photometry fit cannot run");
  }
  if (request.application_id.empty()) {
    return failure<Done>(Code::invalid_request, "an application id is required",
                         "the photometry fit cannot produce a solution");
  }
  if (!request.likelihood.kind.empty() &&
      request.likelihood.kind != "joint_flux_gaussian") {
    return failure<Done>(Code::unsupported,
                         "only the joint flux-domain Gaussian likelihood is implemented",
                         "the photometry fit cannot run");
  }
  const std::string basis_hash =
      input->predictions->empty() || !(*input->predictions)[0] ||
              !(*input->predictions)[0]->basis
          ? std::string()
          : (*input->predictions)[0]->basis->identity.content_hash;
  if (basis_hash.empty()) {
    return failure<Done>(Code::invalid_model, "a prediction has no shared XP basis",
                         "the photometry fit cannot run");
  }
  for (std::size_t i = 0; i < k; ++i) {
    const Ref<SourceMeasurements>& measurement = (*input->measurements)[i];
    if (!measurement) {
      return failure<Done>(Code::invalid_request, "a measurement set is missing",
                           "the photometry fit cannot run");
    }
    if (measurement->frame != request.frames[i]) {
      return failure<Done>(
          Code::invalid_request,
          "the measurement frames do not follow the requested frame order",
          "the photometry fit cannot run");
    }
    if (!measurement->stars.empty() && !measurement->observation) {
      return failure<Done>(Code::invalid_model,
                           "a measured frame has no NativeObservation",
                           "the joint model cannot be assembled");
    }
    const Ref<SpectralPrediction>& prediction = (*input->predictions)[i];
    if (!prediction || !prediction->basis || !prediction->response) {
      return failure<Done>(Code::invalid_model,
                           "a spectral prediction is incomplete",
                           "the joint model cannot be assembled");
    }
    if (prediction->basis->identity.content_hash != basis_hash) {
      return failure<Done>(Code::invalid_request,
                           "the predictions do not share one XP basis",
                           "the photometry fit cannot run");
    }
    if (!request.responses[i] ||
        prediction->response->identity.content_hash !=
            request.responses[i]->identity.content_hash) {
      return failure<Done>(Code::invalid_request,
                           "the prediction and the requested response are different objects",
                           "the photometry fit cannot run");
    }
    if (!prediction->basis_integrals) {
      return failure<Done>(Code::invalid_model,
                           "a prediction has no basis integrals",
                           "the joint model cannot be assembled");
    }
  }
  for (const CatalogueStar& star : *input->stars) {
    if (!star.xp_basis || star.xp_basis->identity.content_hash != basis_hash) {
      return failure<Done>(Code::invalid_request,
                           "the catalogue stars do not use the XP basis of the predictions",
                           "the photometry fit cannot run");
    }
    if (star.source_id == 0) {
      return failure<Done>(Code::invalid_model, "a catalogue star has no source id",
                           "the source join cannot be built");
    }
  }
  if (source_count(input) == 0) {
    return failure<Done>(Code::invalid_request, "no measured source is supplied",
                         "the joint fit cannot be assembled");
  }
  if (request.max_outer_iterations == 0) {
    return failure<Done>(Code::invalid_request,
                         "max_outer_iterations must be at least 1",
                         "the joint fit has no bounded iteration plan");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the joint fit cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the joint fit would exceed its bounded frontier");
  return model::ok_result(Done{});
}

void set_hashes(rt::InvocationPlan& plan, const Ref<FitPhotometryInput>& input,
                U64 sources, U64 k, U64 terms) {
  const PhotometryRequest& request = input->request;
  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.update_u64(k);
  for (std::size_t i = 0; i < k; ++i) {
    const Ref<SourceMeasurements>& measurement = (*input->measurements)[i];
    input_hash.field("measurement",
                     measurement ? measurement->identity.content_hash
                                 : std::string("<null>"));
    input_hash.field("response", ref_key(request.responses[i]));
    input_hash.field("prediction",
                     (*input->predictions)[i]
                         ? (*input->predictions)[i]->identity.content_hash
                         : std::string("<null>"));
  }
  model::Sha256 request_hash;
  request_hash.field("application_id", request.application_id);
  request_hash.update_u64(request.frames.size());
  for (const Id& frame : request.frames) request_hash.field("frame", frame);
  request_hash.update_u64(request.max_outer_iterations);
  request_hash.field_f64("gradient_tolerance", request.gradient_tolerance);
  request_hash.field_f64("step_tolerance", request.step_tolerance);
  request_hash.update_u64(terms);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "joint_flux_gaussian/v1");
  algorithm_hash.update_u64(sources);
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
}

}  // namespace

Result<Done> validate_photometry_fit(rt::InvocationContext,
                                     Ref<FitPhotometryInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_photometry_fit(rt::InvocationContext,
                                         Ref<FitPhotometryInput> input) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  const U64 sources = source_count(input);
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements =
      std::max<U64>(1, std::min<U64>(kDefaultBlockElements, sources));
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_photometry_fit(rt::InvocationContext,
                                               Ref<FitPhotometryInput> input,
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
  const PhotometryRequest& request = input->request;
  const U64 k = request.frames.size();
  const U64 terms = basis_terms(request.basis);
  const U64 sources = source_count(input);
  const U64 block =
      std::max<U64>(1, std::min<U64>(options.block_elements == 0
                                         ? kDefaultBlockElements
                                         : options.block_elements,
                                     sources));
  const U64 cohorts = (sources + block - 1) / block;

  // Joint latent model: per-source local nuisance is eliminated, then the
  // global theta (K*P) Schur/QR is solved at the barrier.  Local terms are
  // (P + log-scale + Gaia latent) wide.
  const U64 local_terms = terms + 4;
  const U64 local_matrix = sat_mul(local_terms, local_terms);
  const U64 global_columns = sat_mul(k, terms);

  const U64 persistent =
      sat_add(sat_mul(global_columns, 8 * 8),          // theta + bounds
              sat_add(sat_mul(global_columns * global_columns, 8),
                      sat_mul(terms, 1ull << 20)));     // Schur + basis/control
  const U64 progress_floor =
      sat_add(sat_mul(block, sat_mul(local_matrix, 8)),
              sat_add(sat_mul(global_columns, sat_mul(local_terms, 8)),
                      sat_add(kWorkspaceUpper, kControlBytes)));
  const U64 work_upper = sat_add(
      sat_mul(sources, sat_add(sat_mul(local_matrix, 4),
                               sat_mul(local_terms, 16))),
      sat_add(sat_mul(request.max_outer_iterations,
                      sat_mul(global_columns, global_columns)),
              request.max_outer_iterations * 64));

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window = std::max<U32>(1, std::min<U32>(options.window,
                                                       kMaxWindow));
  plan.elements = sources;
  plan.block_elements = block;
  plan.cohorts = cohorts;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(sat_mul(global_columns, 64));
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWork;
  plan.adaptive = false;

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent = capacity(persistent);
  region.progress_floor = capacity(progress_floor);
  StageSpec stage;
  stage.id = 0;
  stage.predecessors = {};
  stage.scratch = capacity(progress_floor);
  stage.output_upper = capacity(sat_mul(global_columns, 8 * 2));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  set_hashes(plan, input, sources, k, terms);
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<PhotoSolution>> run_photometry_fit(rt::InvocationContext context,
                                                Ref<FitPhotometryInput> input,
                                                Ref<rt::InvocationPlan> plan) {
  if (!plan || !input) {
    return rt::ready<Ref<PhotoSolution>>(
        context, failure<Ref<PhotoSolution>>(Code::invalid_request,
                                             "no plan or input supplied",
                                             "the photometry fit cannot run"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->cleanup_work_upper + plan->elements + 1;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<PhotoSolution>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<PhotoSolution>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Ref<PhotoSolution>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the photometry fit cannot run");
            }
            acsd::Context& science = (*bridge.value)->context();
            if (!input->measurements || !input->predictions || !input->stars) {
              return failure<Ref<PhotoSolution>>(
                  Code::invalid_request, "the typed input is incomplete",
                  "the photometry fit cannot run");
            }
            std::vector<Ref<SourceMeasurements>> measurements(
                input->measurements->begin(), input->measurements->end());
            std::vector<Ref<SpectralPrediction>> predictions(
                input->predictions->begin(), input->predictions->end());
            std::vector<CatalogueStar> stars(input->stars->begin(),
                                             input->stars->end());
            return fit_photometry(science, measurements, stars, predictions,
                                  input->request);
          }),
      [context, start_ns, span_id](
          Ref<PhotoSolution> solution) -> rt::Task<Ref<PhotoSolution>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.photometry.fit";
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
        return rt::ready<Ref<PhotoSolution>>(
            context, model::ok_result(std::move(solution)));
      });
}

rt::Task<Ref<PhotoSolution>> finalize_photometry_fit(
    rt::InvocationContext context, Ref<FitPhotometryInput>,
    Ref<rt::InvocationPlan>, Result<Ref<PhotoSolution>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<PhotoSolution>>(
        context, failure<Ref<PhotoSolution>>(Code::internal,
                                             "the fit returned no solution",
                                             "no photometric solution can be published"));
  }
  return rt::ready<Ref<PhotoSolution>>(context, std::move(result));
}

Result<Done> register_photometry_fit(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "photometry.fit cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "photometry.fit is unavailable");
  }
  rt::Operation<FitPhotometryInput, Ref<PhotoSolution>> operation{
      kOperation,
      {"acsd.input.FitPhotometryInput", 1},
      {"acsd.PhotoSolution", 1},
      &validate_photometry_fit,
      &bounds_photometry_fit,
      &plan_photometry_fit,
      &run_photometry_fit,
      &finalize_photometry_fit};
  return builder.add<FitPhotometryInput, Ref<PhotoSolution>>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
