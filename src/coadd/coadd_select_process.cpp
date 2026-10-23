// coadd.select typed adapter: eligibility and the single-round LOO rejection.
//
// The selection is a bounded per-leaf stream after the sky is frozen; the
// provider is the already-frozen read-only view supplied by the caller.  This
// adapter never fits a sky and never solves weights.
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

constexpr const char* kOrigin = "coadd.select";
constexpr const char* kProcess = "coadd.select";
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

Result<Done> validate_metadata(const Ref<SelectSamplesInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the selection is not available");
  if (!input->frames)
    return failure<Done>(Code::invalid_request, "field 'frames' is null",
                         "the selection is not available");
  const CoaddRequest& request = input->request;
  if (request.groups.empty())
    return failure<Done>(Code::invalid_request,
                         "no comparability group is declared",
                         "the selection is not available");
  for (const Comparability& group : request.groups) {
    if (group.frames.empty())
      return failure<Done>(Code::invalid_model,
                           "a comparability group has no frame",
                           "the selection is not available");
  }
  if (request.rejection.kind == RejectionPolicy::Kind::single_max_loo) {
    if (request.rejection.common_mean_proof.empty() &&
        !request.rejection.external_fixed_covariance) {
      return failure<Done>(
          Code::invalid_request,
          "single_max_loo requires a common-mean proof or an external fixed covariance",
          "no sample may be rejected without the stated common-mean hypothesis");
    }
    if (!(request.rejection.alpha > 0.0 && request.rejection.alpha < 1.0)) {
      return failure<Done>(Code::invalid_request,
                           "the LOO alpha must lie strictly between 0 and 1",
                           "the rejection threshold cannot be defined");
    }
  }
  if (request.weights == WeightPolicy::blue && request.mixed_observation) {
    return failure<Done>(Code::invalid_request,
                         "a mixed observation has no common mean for BLUE",
                         "the selection is not available");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the selection cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the selection would exceed its frontier");
  return model::ok_result(Done{});
}

}  // namespace

Result<Done> validate_coadd_select(rt::InvocationContext,
                                   Ref<SelectSamplesInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_coadd_select(rt::InvocationContext,
                                       Ref<SelectSamplesInput> input) {
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

Result<rt::InvocationPlan> plan_coadd_select(rt::InvocationContext,
                                             Ref<SelectSamplesInput> input,
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
  const U64 persistent = sat_add(sat_mul(frames, 64ull << 10), 1ull << 20);
  const U64 per_leaf = sat_mul(frames, 8 * 3);  // eligibility/accepted/reasons
  const U64 progress_floor = sat_add(sat_mul(block, per_leaf), kWorkspaceUpper);
  const U64 work_upper = sat_add(sat_mul(leaves_proxy, sat_mul(frames, 16)),
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
  stage.output_upper = capacity(sat_mul(block, per_leaf));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 request_hash;
  request_hash.field("process", kProcess);
  request_hash.update_u64(frames);
  request_hash.update_u64(request.groups.size());
  request_hash.update_u64(static_cast<U64>(request.rejection.kind));
  request_hash.field_f64("alpha", request.rejection.alpha);
  request_hash.field("common_mean_proof", request.rejection.common_mean_proof);
  request_hash.update_u64(static_cast<U64>(request.weights));
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "select_samples/v1");
  plan.input_hash = model::content_id(request_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<Selection>> run_coadd_select(rt::InvocationContext context,
                                          Ref<SelectSamplesInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->frames) {
    return rt::ready<Ref<Selection>>(
        context, failure<Ref<Selection>>(Code::invalid_request,
                                         "no plan, provider or request supplied",
                                         "the selection is not available"));
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
      rt::submit_cpu<Ref<Selection>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<Selection>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Ref<Selection>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the selection is not available");
            }
            acsd::Context& science = (*bridge.value)->context();
            return select_samples(science, *input->frames, input->request);
          }),
      [context, start_ns, span_id](Ref<Selection> selection)
          -> rt::Task<Ref<Selection>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "mosaic.selection.samples";
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
        return rt::ready<Ref<Selection>>(
            context, model::ok_result(std::move(selection)));
      });
}

rt::Task<Ref<Selection>> finalize_coadd_select(
    rt::InvocationContext context, Ref<SelectSamplesInput>,
    Ref<rt::InvocationPlan>, Result<Ref<Selection>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<Selection>>(
        context, failure<Ref<Selection>>(Code::internal, "no selection was produced",
                                         "the selection cannot be published"));
  }
  return rt::ready<Ref<Selection>>(context, std::move(result));
}

Result<Done> register_coadd_select(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "coadd.select cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "coadd.select is unavailable");
  }
  rt::Operation<SelectSamplesInput, Ref<Selection>> operation{
      "select_samples",
      {"acsd.input.SelectSamplesInput", 1},
      {"acsd.Selection", 1},
      &validate_coadd_select,
      &bounds_coadd_select,
      &plan_coadd_select,
      &run_coadd_select,
      &finalize_coadd_select};
  return builder.add<SelectSamplesInput, Ref<Selection>>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
