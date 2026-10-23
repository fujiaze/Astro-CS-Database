// sky.controls typed adapter: common control patches and the frozen global
// connectivity.
//
// Patches are assembled in a bounded stream; the component connectivity and the
// T/H/A/E basis freeze is a second barrier region.  The plan uses the
// request-declared frame groups only (plan must not read a provider).
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/sky.hpp>

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

constexpr const char* kOrigin = "sky.controls";
constexpr const char* kProcess = "sky.controls";
constexpr U64 kPatchBlockElements = 1024;
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

U64 frame_count(const SkyRequest& request) {
  U64 total = 0;
  for (const Comparability& group : request.groups)
    total = sat_add(total, group.frames.size());
  return total;
}

Result<Done> validate_metadata(const Ref<BuildSkyControlsInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the sky controls cannot be built");
  if (!input->frames)
    return failure<Done>(Code::invalid_request, "field 'frames' is null",
                         "the sky controls cannot be built");
  const SkyRequest& request = input->request;
  const bool explicit_t_h = request.T && request.H;
  if (request.groups.empty() && !explicit_t_h) {
    return failure<Done>(
        Code::invalid_request,
        "no comparability group and no explicit T/H basis are supplied",
        "the sky controls cannot be built");
  }
  for (const Comparability& group : request.groups) {
    if (group.frames.empty())
      return failure<Done>(Code::invalid_model,
                           "a comparability group has no frame",
                           "the common support cannot be built");
    if (group.response_proof.empty() || group.spatial_kernel_proof.empty())
      return failure<Done>(
          Code::invalid_request,
          "a comparability group has no response or spatial-kernel proof",
          "different passbands must not be treated as comparable by default");
  }
  if (request.basis.kind == Basis::Kind::polynomial &&
      request.basis.degree > 16) {
    return failure<Done>(Code::resource_exhausted,
                         "the sky basis degree exceeds the bounded model",
                         "the sky controls cannot be built");
  }
  if (request.gauge.kind == Gauge::Kind::weighted_sum &&
      !request.gauge.weights) {
    return failure<Done>(Code::invalid_request,
                         "a weighted-sum gauge requires explicit weights",
                         "the sky gauge cannot be defined");
  }
  if (!std::isfinite(request.basis.scale[0]) ||
      !std::isfinite(request.basis.scale[1]) || request.basis.scale[0] <= 0 ||
      request.basis.scale[1] <= 0) {
    return failure<Done>(Code::invalid_model,
                         "the basis scale must be finite and positive",
                         "the sky basis cannot be evaluated");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the sky controls cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the sky controls would exceed their frontier");
  return model::ok_result(Done{});
}

}  // namespace

Result<Done> validate_sky_controls(rt::InvocationContext,
                                   Ref<BuildSkyControlsInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_sky_controls(rt::InvocationContext,
                                       Ref<BuildSkyControlsInput> input) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = kPatchBlockElements;
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_sky_controls(rt::InvocationContext,
                                             Ref<BuildSkyControlsInput> input,
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
  const SkyRequest& request = input->request;
  const U64 groups = request.groups.size();
  const U64 frames = std::max<U64>(1, frame_count(request));
  const U64 terms = basis_terms(request.basis);
  const U64 block = std::max<U64>(
      1, std::min<U64>(options.block_elements == 0 ? kPatchBlockElements
                                                   : options.block_elements,
                       frames));
  const U64 cohorts = (frames + block - 1) / block;

  // T/H/A/E are one row per frame per patch; the control covariance and the
  // common support are persistent for the whole component solve.
  const U64 persistent =
      sat_add(sat_mul(frames, sat_mul(terms, 8 * 4)),
              sat_add(sat_mul(frames, sat_mul(frames, 8)),
                      sat_add(groups * 64, 1ull << 20)));
  const U64 patch_workspace =
      sat_add(sat_mul(block, sat_mul(terms + 1, 8 * 3)), kWorkspaceUpper);
  const U64 progress_floor = patch_workspace;
  const U64 work_upper =
      sat_add(sat_mul(frames, sat_mul(terms + 1, 64)), 1024);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = frames;
  plan.block_elements = block;
  plan.cohorts = cohorts;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 8ull << 10;
  plan.cleanup_work_upper = 256;
  plan.adaptive = false;

  rt::RegionSpec patches;
  patches.id = 0;
  patches.kind = rt::RegionKind::stream;
  patches.cohorts = cohorts;
  patches.persistent = capacity(persistent);
  patches.progress_floor = capacity(progress_floor);
  StageSpec assembly;
  assembly.id = 0;
  assembly.scratch = capacity(progress_floor);
  assembly.output_upper = capacity(sat_mul(block, sat_mul(terms, 8 * 3)));
  assembly.control_bytes = kControlBytes;
  assembly.work_upper = work_upper;
  patches.stages.push_back(std::move(assembly));
  plan.regions.push_back(std::move(patches));

  rt::RegionSpec freeze;
  freeze.id = 1;
  freeze.kind = rt::RegionKind::barrier;
  freeze.cohorts = 1;
  freeze.persistent = capacity(persistent);
  freeze.progress_floor = capacity(sat_add(sat_mul(frames, sat_mul(terms, 8)),
                                           kWorkspaceUpper));
  StageSpec connectivity;
  connectivity.id = 0;
  connectivity.scratch = freeze.progress_floor;
  connectivity.output_upper = capacity(sat_mul(frames, 8 * 3));
  connectivity.control_bytes = kControlBytes;
  connectivity.work_upper = sat_add(sat_mul(frames, 64), 256);
  freeze.stages.push_back(std::move(connectivity));
  plan.regions.push_back(std::move(freeze));

  rt::RegionEdge edge;
  edge.from = 0;
  edge.to = 1;
  edge.transfer = rt::RegionTransfer::materialized;
  edge.accumulator_bytes = freeze.progress_floor.host_bytes;
  edge.expected_contributions = cohorts;
  plan.region_edges.push_back(std::move(edge));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("frames", std::to_string(reinterpret_cast<std::uintptr_t>(
                                  input->frames.get())));
  model::Sha256 request_hash;
  request_hash.update_u64(groups);
  request_hash.update_u64(frames);
  request_hash.update_u64(terms);
  for (const Comparability& group : request.groups)
    for (const Id& frame : group.frames) request_hash.field("frame", frame);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "build_sky_controls/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<SkyControls>> run_sky_controls(rt::InvocationContext context,
                                            Ref<BuildSkyControlsInput> input,
                                            Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->frames) {
    return rt::ready<Ref<SkyControls>>(
        context, failure<Ref<SkyControls>>(Code::invalid_request,
                                           "no plan, provider or request supplied",
                                           "the sky controls cannot be built"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 8 + 1;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<SkyControls>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Ref<SkyControls>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Ref<SkyControls>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the sky controls cannot be built");
            }
            acsd::Context& science = (*bridge.value)->context();
            return build_sky_controls(science, *input->frames, input->request);
          }),
      [context, start_ns, span_id](Ref<SkyControls> controls)
          -> rt::Task<Ref<SkyControls>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "mosaic.sky.controls";
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
        return rt::ready<Ref<SkyControls>>(
            context, model::ok_result(std::move(controls)));
      });
}

rt::Task<Ref<SkyControls>> finalize_sky_controls(
    rt::InvocationContext context, Ref<BuildSkyControlsInput>,
    Ref<rt::InvocationPlan>, Result<Ref<SkyControls>> result) {
  if (result.code == Code::ok && result.value && !*result.value) {
    return rt::ready<Ref<SkyControls>>(
        context, failure<Ref<SkyControls>>(Code::internal,
                                           "no sky controls were produced",
                                           "the controls cannot be published"));
  }
  return rt::ready<Ref<SkyControls>>(context, std::move(result));
}

Result<Done> register_sky_controls(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "sky.controls cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "sky.controls is unavailable");
  }
  rt::Operation<BuildSkyControlsInput, Ref<SkyControls>> operation{
      "build_sky_controls",
      {"acsd.input.BuildSkyControlsInput", 1},
      {"acsd.SkyControls", 1},
      &validate_sky_controls,
      &bounds_sky_controls,
      &plan_sky_controls,
      &run_sky_controls,
      &finalize_sky_controls};
  return builder.add<BuildSkyControlsInput, Ref<SkyControls>>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
