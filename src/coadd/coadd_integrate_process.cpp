// coadd.integrate typed adapter: consumes the already sealed Selection and
// WeightSolution through the unique combination core `coadd_with_inputs()`.
//
// The adapter only validates the frozen-input identities (metadata only), plans
// the conservative resource model, and runs the one existing combination body
// through the runtime science Context bridge.  It never calls select_samples()
// or solve_weights().
#include <acsd/modules/coadd.hpp>
#include <acsd/modules/generated/status.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "acsd/runtime/diagnostics.hpp"
#include "coadd/coadd.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {
namespace {

using rt::Done;
using rt::PlanBounds;
using rt::PlanOptions;
using rt::ResourceVector;
using rt::StageSpec;

constexpr const char* kOrigin = "coadd.integrate";
constexpr const char* kProcess = "coadd.integrate";

// The combination core is whole-frame (it reads every frozen frame leaf block),
// so the plan is serial and reflects that working set honestly.  The constants
// below are per-element conservative bounds:
//   frame view: signal 8 + DQ 2 + support 1 byte per (leaf, frame);
//   sealed coefficient matrix: 8 bytes per (leaf, frame);
//   combined/output signal: 8 bytes per leaf;
//   plus the fixed metadata/control and one canonical workspace budget.
constexpr U64 kFrameViewBytesPerPair = 11;
constexpr U64 kCoefficientBytesPerPair = 8;
constexpr U64 kWorkspaceUpper = 1ull << 20;
constexpr U64 kControlBytes = 8ull << 10;
constexpr U64 kMetadataBytes = 64ull << 10;

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

// The science input of the unique core.  `frames` is borrowed for the call.
CoaddInputs inputs_of(const CoaddInput& input) {
  CoaddInputs inputs;
  inputs.frames = input.frames.get();
  inputs.sky = input.sky;
  inputs.selection = input.selection;
  inputs.weights = input.weights;
  inputs.request = input.request;
  return inputs;
}

Result<Done> validate_metadata(const Ref<CoaddInput>& input) {
  if (!input) {
    return failure<Done>(Code::invalid_request, "the typed input is null",
                         "the combination is not available");
  }
  if (!input->frames) {
    return failure<Done>(Code::invalid_request, "field 'frames' is null",
                         "the combination is not available");
  }
  if (!input->selection) {
    return failure<Done>(Code::invalid_request, "field 'selection' is null",
                         "the combination is not available");
  }
  if (!input->weights) {
    return failure<Done>(Code::invalid_request, "field 'weights' is null",
                         "the combination is not available");
  }
  Result<Done> shared = ::acsd::validate_coadd_inputs(inputs_of(*input));
  if (shared.code != Code::ok) {
    Result<Done> out = failure<Done>(shared.code, "the sealed coadd inputs are invalid",
                                     "the combination is not available");
    out.assessment.issues.insert(out.assessment.issues.end(),
                                 shared.assessment.issues.begin(),
                                 shared.assessment.issues.end());
    return out;
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options, U64 leaves) {
  if (options.window != 1) {
    return failure<Done>(Code::invalid_request,
                         "the serial combination only accepts window 1",
                         "the combination cannot be scheduled");
  }
  if (options.block_elements != 0 && options.block_elements != leaves) {
    return failure<Done>(
        Code::invalid_request,
        "the combination core is whole-frame; the block must cover all leaves",
        "the combination cannot be scheduled");
  }
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
  const U64 leaves = input->selection->index->size;
  PlanBounds bounds;
  // The core reads the whole frozen frame set; a smaller block would only be a
  // plan fiction, so the legal range is the leaf count itself.
  bounds.min_block_elements = leaves == 0 ? 1 : leaves;
  bounds.max_block_elements = leaves == 0 ? 1 : leaves;
  bounds.max_window = 1;
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
  const U64 leaves = input->selection->index->size;
  if (leaves == 0) {
    return failure<rt::InvocationPlan>(Code::invalid_request,
                                       "the sealed selection has no leaf",
                                       "the combination cannot be planned");
  }
  Result<Done> options_ok = validate_plan_options(options, leaves);
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const CoaddRequest& request = input->request;
  const U64 frames = input->weights->coefficients->shape[1];
  const U64 pairs = sat_mul(leaves, frames);

  // F is the transient working set of the whole-frame core: the frame views
  // (signal/DQ/support), the coefficient matrix, the combined vector and the
  // fixed workspace.  These vectors are allocated outside the arena by the
  // library; the term is the conservative upper bound, not a measured peak.
  const U64 frame_views = sat_mul(pairs, kFrameViewBytesPerPair);
  const U64 coefficients = sat_mul(pairs, kCoefficientBytesPerPair);
  const U64 combined = sat_mul(leaves, 8);
  const U64 workspace = kWorkspaceUpper;
  const U64 progress_floor = sat_add(
      sat_add(frame_views, coefficients),
      sat_add(combined, sat_add(workspace, kControlBytes)));

  // R holds the final output signal array and the model/metadata refs.
  const U64 persistent = sat_add(sat_add(combined, kMetadataBytes), kControlBytes);
  const U64 work_upper = sat_add(sat_mul(pairs, 16), 1024);

  rt::InvocationPlan plan;
  plan.epoch = 0;
  plan.options = options;
  plan.options.block_elements = leaves;
  plan.options.window = 1;
  plan.elements = leaves;
  plan.block_elements = leaves;
  plan.cohorts = 1;
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
  region.cohorts = 1;
  region.persistent = capacity(persistent);
  region.progress_floor = capacity(progress_floor);
  StageSpec stage;
  stage.id = 0;
  stage.scratch = capacity(progress_floor);
  stage.output_upper = capacity(combined);
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  region.edges.clear();
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
  algorithm_hash.field("algorithm", "coadd_with_inputs/frozen-selection/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<CoaddResult> run_coadd_integrate(rt::InvocationContext context,
                                          Ref<CoaddInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  if (!input) {
    return rt::ready<CoaddResult>(
        context, failure<CoaddResult>(Code::invalid_request,
                                      "the typed input is null",
                                      "the combination is not available"));
  }
  U64 work_upper = 0;
  if (plan && !plan->regions.empty() && !plan->regions.front().stages.empty())
    work_upper = plan->regions.front().stages.front().work_upper;
  if (work_upper == 0 && input->selection && input->selection->index &&
      input->weights && input->weights->coefficients &&
      input->weights->coefficients->shape.size() == 2) {
    work_upper = sat_add(
        sat_mul(sat_mul(input->selection->index->size,
                        input->weights->coefficients->shape[1]),
                16),
        1024);
  }
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, 0};
  spec.running.cpu_tokens = 1;
  spec.work_upper = work_upper;
  spec.semantic = "coadd.integrate/coadd_with_inputs/v1";
  const rt::ArenaRef arena = context.persistent ? context.persistent : context.progress;
  return rt::submit_cpu<CoaddResult>(
      context, spec,
      [context, input, arena](rt::InvocationContext task) -> Result<CoaddResult> {
        Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
            rt::detail::make_science_context(context, arena);
        if (bridge.code != Code::ok || !bridge.value) {
          return failure<CoaddResult>(bridge.code, "the science context bridge cannot be created",
                                      "the combination is not available");
        }
        acsd::Context& science = (*bridge.value)->context();
        CoaddInputs inputs = inputs_of(*input);
        Result<CoaddResult> result = ::acsd::coadd_with_inputs(science, inputs);
        return result;
      });
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
