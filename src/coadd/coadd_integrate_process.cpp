// coadd.integrate typed adapter: consumes frozen Selection/WeightSolution.
//
// BLOCKED REFACTOR: the existing unique implementation
//     Result<CoaddResult> coadd(Context&, const FramesProvider&, Ref<SkyFit>,
//                               const CoaddRequest&)
// re-runs select_samples() and solve_weights() internally (src/coadd/coadd.cpp,
// coadd() lines ~1008-1020) and has no entry point that consumes the already
// sealed Selection/WeightSolution.  Running it here would silently recompute
// both stages and claim that the sealed inputs were consumed, which violates the
// process contract ("只消费已封存的Selection/WeightSolution，不调用
// select/weights").  The run hook therefore returns a named unsupported
// failure.  The minimal counterexample and the refactor are recorded in
// validation/process/coadd.integrate/README.md.
#include <acsd/modules/coadd.hpp>
#include <acsd/modules/generated/status.hpp>

#include <algorithm>
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

constexpr const char* kOrigin = "coadd.integrate";
constexpr const char* kProcess = "coadd.integrate";
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

U64 frame_count(const CoaddRequest& request) {
  U64 total = 0;
  for (const Comparability& group : request.groups)
    total = sat_add(total, group.frames.size());
  return total;
}

bool same_rejection(const RejectionPolicy& a, const RejectionPolicy& b) {
  return a.kind == b.kind &&
         a.common_mean_proof == b.common_mean_proof;
}

Result<Done> validate_metadata(const Ref<CoaddInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the combination is not available");
  if (!input->frames)
    return failure<Done>(Code::invalid_request, "field 'frames' is null",
                         "the combination is not available");
  if (!input->sky)
    return failure<Done>(Code::invalid_request, "field 'sky' is null",
                         "the combination is not available");
  if (!input->selection)
    return failure<Done>(Code::invalid_request, "field 'selection' is null",
                         "the combination is not available");
  if (!input->weights)
    return failure<Done>(Code::invalid_request, "field 'weights' is null",
                         "the combination is not available");
  const Selection& selection = *input->selection;
  const WeightSolution& weights = *input->weights;
  if (!selection.index || !selection.accepted || !selection.eligibility ||
      !selection.reasons) {
    return failure<Done>(Code::invalid_model,
                         "the sealed selection is incomplete",
                         "the combination is not available");
  }
  if (!weights.coefficients || !weights.samples) {
    return failure<Done>(Code::invalid_model,
                         "the sealed weight solution is incomplete",
                         "the combination is not available");
  }
  if (!weights.selection ||
      weights.selection->identity.content_hash !=
          selection.identity.content_hash) {
    return failure<Done>(
        Code::identity_conflict,
        "the weights were solved for a different Selection object",
        "the sealed inputs cannot be combined");
  }
  if (weights.selection && weights.selection->index &&
      selection.index &&
      weights.selection->index->identity.content_hash !=
          selection.index->identity.content_hash) {
    return failure<Done>(Code::identity_conflict,
                         "the weight and selection leaf indexes differ",
                         "the sealed inputs cannot be combined");
  }
  if (!same_rejection(selection.policy, input->request.rejection)) {
    return failure<Done>(
        Code::identity_conflict,
        "the sealed selection policy differs from the requested rejection policy",
        "the sealed inputs cannot be combined");
  }
  if (input->request.weights != WeightPolicy::automatic &&
      input->request.weights != weights.policy) {
    return failure<Done>(
        Code::identity_conflict,
        "the sealed weight policy differs from the requested weight policy",
        "the sealed inputs cannot be combined");
  }
  if (!selection.condition.estimator.empty() &&
      !input->request.target.id.empty() &&
      selection.condition.estimator != input->request.target.id) {
    return failure<Done>(
        Code::identity_conflict,
        "the sealed selection belongs to a different estimator",
        "the sealed inputs cannot be combined");
  }
  const U64 leaves = selection.index->size;
  const U64 frames = frame_count(input->request);
  const U64 coefficients = elements_of(weights.coefficients);
  if (frames > 0 && leaves > 0 &&
      coefficients != sat_mul(leaves, frames)) {
    return failure<Done>(Code::invalid_model,
                         "the weight matrix does not match leaves times frames",
                         "the sealed inputs cannot be combined");
  }
  for (const Comparability& group : input->request.groups) {
    if (group.frames.empty())
      return failure<Done>(Code::invalid_model,
                           "a comparability group has no frame",
                           "the combination is not available");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the combination cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the combination would exceed its frontier");
  return model::ok_result(Done{});
}

}  // namespace

Result<Done> validate_coadd_integrate(rt::InvocationContext,
                                      Ref<CoaddInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_coadd_integrate(rt::InvocationContext,
                                          Ref<CoaddInput> input) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = kDefaultBlockElements;
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_coadd_integrate(rt::InvocationContext,
                                                Ref<CoaddInput> input,
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
  const CoaddRequest& request = input->request;
  const U64 frames = std::max<U64>(1, frame_count(request));
  const U64 leaves = input->selection->index ? input->selection->index->size : 0;
  const U64 block = std::max<U64>(
      1, std::min<U64>(options.block_elements == 0 ? kDefaultBlockElements
                                                   : options.block_elements,
                       leaves == 0 ? 1 : leaves));
  const U64 cohorts = leaves == 0 ? 0 : (leaves + block - 1) / block;
  // The reduction reads the sealed accepted mask, the sealed coefficients and
  // the frozen sky-corrected samples; the K^2 Gamma is not rebuilt here.
  const U64 persistent =
      sat_add(sat_mul(leaves, sat_mul(frames, 8)),
              sat_add(sat_mul(leaves, 8 * 3), 1ull << 20));
  const U64 per_leaf = sat_mul(frames, 8 * 4);
  const U64 progress_floor = sat_add(sat_mul(block, per_leaf), kWorkspaceUpper);
  const U64 work_upper =
      sat_add(sat_mul(leaves, sat_mul(frames, 16)), 1024);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = leaves;
  plan.block_elements = block;
  plan.cohorts = cohorts;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 8ull << 10;
  plan.cleanup_work_upper = 128;
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
  stage.output_upper = capacity(sat_mul(block, 8 * 3));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("selection", input->selection->identity.content_hash);
  input_hash.field("weights", input->weights->identity.content_hash);
  input_hash.field("sky", input->sky->identity.content_hash);
  model::Sha256 request_hash;
  request_hash.update_u64(leaves);
  request_hash.update_u64(frames);
  request_hash.update_u64(static_cast<U64>(request.weights));
  request_hash.update_u64(request.mixed_observation ? 1 : 0);
  request_hash.field("target", request.target.id);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "coadd_integrate/frozen-selection/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<CoaddResult> run_coadd_integrate(rt::InvocationContext context,
                                          Ref<CoaddInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  (void)input;
  (void)plan;
  return rt::ready<CoaddResult>(
      context,
      failure<CoaddResult>(
          Code::unsupported,
          "coadd.integrate is blocked: the unique coadd() implementation still "
          "calls select_samples() and solve_weights() internally and has no "
          "entry point that consumes the sealed Selection/WeightSolution",
          "the combination is not available; no signal is fabricated and no "
          "silent recomputation is claimed"));
}

rt::Task<CoaddResult> finalize_coadd_integrate(
    rt::InvocationContext context, Ref<CoaddInput>, Ref<rt::InvocationPlan>,
    Result<CoaddResult> result) {
  return rt::ready<CoaddResult>(context, std::move(result));
}

Result<Done> register_coadd_integrate(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "coadd.integrate cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "coadd.integrate is unavailable");
  }
  rt::Operation<CoaddInput, CoaddResult> operation{
      "coadd",
      {"acsd.input.CoaddInput", 1},
      {"acsd.CoaddResult", 1},
      &validate_coadd_integrate,
      &bounds_coadd_integrate,
      &plan_coadd_integrate,
      &run_coadd_integrate,
      &finalize_coadd_integrate};
  return builder.add<CoaddInput, CoaddResult>(*registration.value,
                                              std::move(operation));
}

}  // namespace acsd::modules
