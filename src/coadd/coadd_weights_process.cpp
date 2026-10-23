// coadd.weights typed adapter: BLUE / equal / non-negative QP weights from the
// frozen selection and the joint Gamma.
//
// The K^2 Gamma, the PSD constraint and the active-set/QP workspace are the
// per-leaf working set; the provider is the already-frozen read-only view.
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

constexpr const char* kOrigin = "coadd.weights";
constexpr const char* kProcess = "coadd.weights";
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

U64 frame_count(const CoaddRequest& request) {
  U64 total = 0;
  for (const Comparability& group : request.groups)
    total = sat_add(total, group.frames.size());
  return total;
}

Result<Done> validate_metadata(const Ref<SolveWeightsInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the weights cannot be solved");
  if (!input->frames)
    return failure<Done>(Code::invalid_request, "field 'frames' is null",
                         "the weights cannot be solved");
  if (!input->selection)
    return failure<Done>(Code::invalid_request, "field 'selection' is null",
                         "the weights cannot be solved");
  if (!input->selection->index || !input->selection->accepted)
    return failure<Done>(Code::invalid_model,
                         "the selection has no leaf index or accepted mask",
                         "the weights cannot be solved");
  if (input->request.groups.empty())
    return failure<Done>(Code::invalid_request,
                         "no comparability group is declared",
                         "the weights cannot be solved");
  for (const Comparability& group : input->request.groups) {
    if (group.frames.empty())
      return failure<Done>(Code::invalid_model,
                           "a comparability group has no frame",
                           "the weights cannot be solved");
  }
  if (input->request.mixed_observation &&
      input->request.weights == WeightPolicy::blue) {
    return failure<Done>(Code::invalid_request,
                         "a mixed observation has no common mean, BLUE is not defined",
                         "the weights cannot be solved");
  }
  if (input->selection->policy.kind == RejectionPolicy::Kind::single_max_loo &&
      input->selection->policy.common_mean_proof.empty() &&
      !input->selection->policy.external_fixed_covariance) {
    return failure<Done>(Code::invalid_model,
                         "the selection claims LOO without a common-mean proof",
                         "the weights cannot be solved");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the weights cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the weights would exceed their frontier");
  return model::ok_result(Done{});
}

}  // namespace

Result<Done> validate_coadd_weights(rt::InvocationContext,
                                    Ref<SolveWeightsInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_coadd_weights(rt::InvocationContext,
                                        Ref<SolveWeightsInput> input) {
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

Result<rt::InvocationPlan> plan_coadd_weights(rt::InvocationContext,
                                              Ref<SolveWeightsInput> input,
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
  const U64 leaves_proxy = sat_mul(request.groups.size(), kDefaultBlockElements);
  const U64 block =
      std::max<U64>(1, std::min<U64>(options.block_elements == 0
                                         ? kDefaultBlockElements
                                         : options.block_elements,
                                     leaves_proxy));
  const U64 cohorts = (leaves_proxy + block - 1) / block;
  const U64 gamma = sat_mul(frames, frames);
  const U64 persistent = sat_add(sat_mul(frames, 64ull << 10), 1ull << 20);
  const U64 per_leaf = sat_add(sat_mul(gamma, 8 * 2), sat_mul(frames, 8 * 2));
  const U64 progress_floor = sat_add(sat_mul(block, per_leaf), kWorkspaceUpper);
  const U64 work_upper =
      sat_add(sat_mul(leaves_proxy, sat_add(sat_mul(gamma, 16),
                                            sat_mul(request.solve.max_iterations,
                                                    sat_mul(frames, 8)))),
              1024);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = leaves_proxy;
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
  stage.output_upper = capacity(sat_mul(block, sat_mul(frames, 8)));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("selection",
                   input->selection->identity.content_hash);
  model::Sha256 request_hash;
  request_hash.update_u64(frames);
  request_hash.update_u64(static_cast<U64>(request.weights));
  request_hash.update_u64(request.mixed_observation ? 1 : 0);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "solve_weights/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<WeightSolution>> run_coadd_weights(rt::InvocationContext context,
                                                Ref<SolveWeightsInput> input,
                                                Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->frames || !input->selection) {
    return rt::ready<Ref<WeightSolution>>(
        context, failure<Ref<WeightSolution>>(
                     Code::invalid_request, "no plan, provider or selection supplied",
                     "the weights cannot be solved"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 8 + 128;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<WeightSolution>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<WeightSolution>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Ref<WeightSolution>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the weights cannot be solved");
            }
            acsd::Context& science = (*bridge.value)->context();
            return solve_weights(science, *input->frames, input->selection,
                                 input->request);
          }),
      [context, start_ns, span_id](Ref<WeightSolution> weights)
          -> rt::Task<Ref<WeightSolution>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "mosaic.weight.coefficients";
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
        return rt::ready<Ref<WeightSolution>>(
            context, model::ok_result(std::move(weights)));
      });
}

rt::Task<Ref<WeightSolution>> finalize_coadd_weights(
    rt::InvocationContext context, Ref<SolveWeightsInput>,
    Ref<rt::InvocationPlan>, Result<Ref<WeightSolution>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<WeightSolution>>(
        context, failure<Ref<WeightSolution>>(Code::internal,
                                              "no weight solution was produced",
                                              "the weights cannot be published"));
  }
  return rt::ready<Ref<WeightSolution>>(context, std::move(result));
}

Result<Done> register_coadd_weights(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "coadd.weights cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "coadd.weights is unavailable");
  }
  rt::Operation<SolveWeightsInput, Ref<WeightSolution>> operation{
      "solve_weights",
      {"acsd.input.SolveWeightsInput", 1},
      {"acsd.WeightSolution", 1},
      &validate_coadd_weights,
      &bounds_coadd_weights,
      &plan_coadd_weights,
      &run_coadd_weights,
      &finalize_coadd_weights};
  return builder.add<SolveWeightsInput, Ref<WeightSolution>>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
