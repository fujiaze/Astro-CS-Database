// information.scene typed adapter: actual-scene variance, the unique 2D
// Delaunay triangulation and the sparse query reconstruction.
//
// The three operations keep their separate barriers/streams: the triangulation
// is a global exact-predicate barrier (Bowyer-Watson, O(M log M) typical and
// O(M^2) worst case), the leaf variance and the query map are bounded streams.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/information.hpp>

#include <cmath>
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

constexpr const char* kProcess = "information.scene";
constexpr U64 kDefaultBlockElements = 65536;
constexpr U64 kTriangulationBlockElements = 4096;
constexpr U32 kMaxWindow = 2;
constexpr U64 kControlBytes = 8ull << 10;
constexpr U64 kWorkspaceUpper = 1ull << 20;
constexpr double kFirstRadiusLimitRad = 5.0 * 3.14159265358979323846 / 180.0;

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
Result<T> scene_failure(Code code, std::string reason, std::string impact) {
  return failure<T>(code, "information.scene", std::move(reason),
                    std::move(impact));
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

Result<Done> validate_plan_options(const PlanOptions& options,
                                   const char* operation) {
  if (options.window == 0)
    return scene_failure<Done>(Code::invalid_request,
                               std::string(operation) + ": the plan window is zero",
                               "the operation cannot be scheduled");
  if (options.window > kMaxWindow)
    return scene_failure<Done>(Code::resource_exhausted,
                               std::string(operation) +
                                   ": the window exceeds the process maximum",
                               "the operation would exceed its frontier");
  return model::ok_result(Done{});
}

void set_hashes(rt::InvocationPlan& plan, const char* operation,
                const std::string& input_key, const std::string& request_key) {
  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("operation", operation);
  input_hash.field("input", input_key);
  model::Sha256 request_hash;
  request_hash.field("process", kProcess);
  request_hash.field("request", request_key);
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", std::string(operation) + "/v1");
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
}

// ---------------------------------------------------------------------------
// evaluate_scene_variance
// ---------------------------------------------------------------------------

Result<Done> validate_evaluate(const Ref<EvaluateSceneVarianceInput>& input) {
  if (!input) return scene_failure<Done>(Code::invalid_request,
                                         "the typed input is null",
                                         "the scene variance is unknown");
  if (!input->frames)
    return scene_failure<Done>(Code::invalid_request, "field 'frames' is null",
                               "the scene variance is unknown");
  if (input->frame.empty())
    return scene_failure<Done>(Code::invalid_request, "field 'frame' is empty",
                               "the scene variance is unknown");
  if (!input->leaves)
    return scene_failure<Done>(Code::invalid_request, "field 'leaves' is null",
                               "the scene variance is unknown");
  return model::ok_result(Done{});
}

// ---------------------------------------------------------------------------
// build_supported_triangulation
// ---------------------------------------------------------------------------

Result<Done> validate_build(
    const Ref<BuildSupportedTriangulationInput>& input) {
  if (!input) return scene_failure<Done>(Code::invalid_request,
                                         "the typed input is null",
                                         "the triangulation cannot be built");
  if (!input->controls)
    return scene_failure<Done>(Code::invalid_request, "field 'controls' is null",
                               "the triangulation cannot be built");
  if (!input->support)
    return scene_failure<Done>(Code::invalid_request, "field 'support' is null",
                               "the triangulation cannot be built");
  if (!input->controls->positions || !input->controls->values)
    return scene_failure<Done>(Code::invalid_model,
                               "the precision controls have no positions or values",
                               "the triangulation cannot be built");
  if (input->controls->kind != PrecisionControls::Kind::actual_scene)
    return scene_failure<Done>(
        Code::invalid_request,
        "the controls are not an actual-scene variance model",
        "the triangulation is only defined for actual-scene controls");
  if (!std::isfinite(input->chart.maximum_radius_rad) ||
      !(input->chart.maximum_radius_rad > 0.0))
    return scene_failure<Done>(Code::invalid_request,
                               "the chart radius is not positive",
                               "the triangulation cannot be built");
  if (input->chart.maximum_radius_rad > kFirstRadiusLimitRad)
    return scene_failure<Done>(Code::unsupported,
                               "the chart radius exceeds the first-implementation bound",
                               "the triangulation is unavailable for this chart");
  if (elements_of(input->controls->positions) < 3)
    return scene_failure<Done>(Code::unavailable,
                               "fewer than three control points are supplied",
                               "no triangulation exists");
  return model::ok_result(Done{});
}

// ---------------------------------------------------------------------------
// reconstruct_scene_variance
// ---------------------------------------------------------------------------

Result<Done> validate_reconstruct(
    const Ref<ReconstructSceneVarianceInput>& input) {
  if (!input) return scene_failure<Done>(Code::invalid_request,
                                         "the typed input is null",
                                         "the scene variance is unknown");
  if (!input->controls)
    return scene_failure<Done>(Code::invalid_request, "field 'controls' is null",
                               "the scene variance is unknown");
  if (input->controls->kind != PrecisionControls::Kind::actual_scene)
    return scene_failure<Done>(
        Code::invalid_request,
        "the controls are not an actual-scene variance model",
        "the reference sensitivity cannot be used as a scene variance");
  if (!input->triangulation)
    return scene_failure<Done>(Code::invalid_request,
                               "field 'triangulation' is null",
                               "the scene variance is unknown");
  if (!input->queries)
    return scene_failure<Done>(Code::invalid_request, "field 'queries' is null",
                               "the scene variance is unknown");
  return model::ok_result(Done{});
}

}  // namespace

// ---------------------------------------------------------------------------
// Public hooks: evaluate_scene_variance
// ---------------------------------------------------------------------------

Result<Done> validate_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput> input) {
  return validate_evaluate(input);
}

Result<PlanBounds> bounds_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput> input) {
  Result<Done> validated = validate_evaluate(input);
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

Result<rt::InvocationPlan> plan_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput> input,
    PlanOptions options) {
  Result<Done> validated = validate_evaluate(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok =
      validate_plan_options(options, "evaluate_scene_variance");
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const U64 leaves = input->leaves ? input->leaves->size() : 0;
  const U64 block = std::max<U64>(
      1, std::min<U64>(options.block_elements == 0 ? kDefaultBlockElements
                                                   : options.block_elements,
                       leaves == 0 ? 1 : leaves));
  const U64 cohorts = leaves == 0 ? 0 : (leaves + block - 1) / block;
  const U64 persistent = 1ull << 20;  // frame/noise metadata and dependencies
  const U64 per_leaf = 8 * 3 + 1;     // value/interval/state
  const U64 progress_floor = sat_add(sat_mul(block, per_leaf), kWorkspaceUpper);
  const U64 work_upper = sat_add(sat_mul(leaves, 64), 64);

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
  set_hashes(plan, "evaluate_scene_variance", input->frame,
             std::to_string(leaves));
  return model::ok_result(std::move(plan));
}

rt::Task<std::vector<Variance>> run_information_scene_evaluate(
    rt::InvocationContext context, Ref<EvaluateSceneVarianceInput> input,
    Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->frames || !input->leaves) {
    return rt::ready<std::vector<Variance>>(
        context, scene_failure<std::vector<Variance>>(
                     Code::invalid_request, "no plan, provider or leaves supplied",
                     "the scene variance is unknown"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 4 + 1;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<std::vector<Variance>>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<std::vector<Variance>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return scene_failure<std::vector<Variance>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the scene variance is unknown");
            }
            acsd::Context& science = (*bridge.value)->context();
            return evaluate_scene_variance(science, *input->frames, input->frame,
                                           *input->leaves);
          }),
      [context, start_ns, span_id](std::vector<Variance> variances)
          -> rt::Task<std::vector<Variance>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.information.scene_variance";
        record.kind = rt::PerfKind::counter;
        record.count = variances.size();
        record.start_steady_ns = start_ns;
        record.span_id = span_id;
        record.tags.component = kProcess;
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<std::vector<Variance>>(
            context, model::ok_result(std::move(variances)));
      });
}

rt::Task<std::vector<Variance>> finalize_information_scene_evaluate(
    rt::InvocationContext context, Ref<EvaluateSceneVarianceInput>,
    Ref<rt::InvocationPlan>, Result<std::vector<Variance>> result) {
  return rt::ready<std::vector<Variance>>(context, std::move(result));
}

// ---------------------------------------------------------------------------
// Public hooks: build_supported_triangulation
// ---------------------------------------------------------------------------

Result<Done> validate_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput> input) {
  return validate_build(input);
}

Result<PlanBounds> bounds_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput> input) {
  Result<Done> validated = validate_build(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  PlanBounds bounds;
  bounds.min_block_elements = 3;
  bounds.max_block_elements = kTriangulationBlockElements;
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput> input,
    PlanOptions options) {
  Result<Done> validated = validate_build(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok =
      validate_plan_options(options, "build_supported_triangulation");
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const U64 points = elements_of(input->controls->positions) / 2;
  const U64 block = std::max<U64>(
      3, std::min<U64>(options.block_elements == 0
                           ? kTriangulationBlockElements
                           : options.block_elements,
                       points == 0 ? 3 : points));
  const U64 persistent = sat_add(1ull << 20, sat_mul(points, 2 * 8 * 4));
  // Bowyer-Watson keeps all vertices, the super triangle and the local cavity;
  // the exact predicates need the GMP filter workspace.
  const U64 progress_floor =
      sat_add(sat_mul(points, sat_mul(2, 8 * 4)),
              sat_add(sat_mul(points, sat_mul(3, 8)), kWorkspaceUpper));
  const U64 work_upper =
      sat_add(sat_mul(points, sat_mul(points, 16)), sat_mul(points, 128));

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window = 1;
  plan.elements = points;
  plan.block_elements = block;
  plan.cohorts = 1;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 8ull << 10;
  plan.cleanup_work_upper = 256;
  plan.adaptive = false;

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent = capacity(persistent);
  region.progress_floor = capacity(progress_floor);
  StageSpec stage;
  stage.id = 0;
  stage.scratch = capacity(progress_floor);
  stage.output_upper = capacity(sat_mul(points, 8 * 5));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  set_hashes(plan, "build_supported_triangulation",
             input->controls->identity.content_hash,
             std::to_string(points) + "|" +
                 std::to_string(input->chart.maximum_radius_rad));
  return model::ok_result(std::move(plan));
}

rt::Task<Ref<SupportedTriangulation>> run_information_scene_build(
    rt::InvocationContext context, Ref<BuildSupportedTriangulationInput> input,
    Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->controls || !input->support) {
    return rt::ready<Ref<SupportedTriangulation>>(
        context, scene_failure<Ref<SupportedTriangulation>>(
                     Code::invalid_request, "no plan, controls or support supplied",
                     "the triangulation cannot be built"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * plan->elements / 4 + 1024;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Ref<SupportedTriangulation>>(
          context, spec,
          [input](rt::InvocationContext task)
              -> Result<Ref<SupportedTriangulation>> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return scene_failure<Ref<SupportedTriangulation>>(
                  bridge.code, "the science context bridge cannot be created",
                  "the triangulation cannot be built");
            }
            acsd::Context& science = (*bridge.value)->context();
            return build_supported_triangulation(
                science, input->controls, input->chart, input->support);
          }),
      [context, start_ns,
       span_id](Ref<SupportedTriangulation> mesh)
          -> rt::Task<Ref<SupportedTriangulation>> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.information.scene_variance";
        record.kind = rt::PerfKind::span;
        record.start_steady_ns = start_ns;
        record.wall_ns = rt::steady_ns() - start_ns;
        record.span_id = span_id;
        record.tags.component = "build_supported_triangulation";
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<Ref<SupportedTriangulation>>(
            context, model::ok_result(std::move(mesh)));
      });
}

rt::Task<Ref<SupportedTriangulation>> finalize_information_scene_build(
    rt::InvocationContext context, Ref<BuildSupportedTriangulationInput>,
    Ref<rt::InvocationPlan>, Result<Ref<SupportedTriangulation>> result) {
  return rt::ready<Ref<SupportedTriangulation>>(context, std::move(result));
}

// ---------------------------------------------------------------------------
// Public hooks: reconstruct_scene_variance
// ---------------------------------------------------------------------------

Result<Done> validate_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput> input) {
  return validate_reconstruct(input);
}

Result<PlanBounds> bounds_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput> input) {
  Result<Done> validated = validate_reconstruct(input);
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

Result<rt::InvocationPlan> plan_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput> input,
    PlanOptions options) {
  Result<Done> validated = validate_reconstruct(input);
  if (validated.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  Result<Done> options_ok =
      validate_plan_options(options, "reconstruct_scene_variance");
  if (options_ok.code != Code::ok) {
    Result<rt::InvocationPlan> out;
    out.code = options_ok.code;
    out.assessment = options_ok.assessment;
    return out;
  }
  const U64 queries = input->queries ? input->queries->size() : 0;
  const U64 block = std::max<U64>(
      1, std::min<U64>(options.block_elements == 0 ? kDefaultBlockElements
                                                   : options.block_elements,
                       queries == 0 ? 1 : queries));
  const U64 cohorts = queries == 0 ? 0 : (queries + block - 1) / block;
  const U64 persistent = 1ull << 20;
  const U64 per_query = 8 * 3 + 1;
  const U64 progress_floor = sat_add(sat_mul(block, per_query), kWorkspaceUpper);
  const U64 work_upper = sat_add(sat_mul(queries, 64), 64);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = queries;
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
  stage.output_upper = capacity(sat_mul(block, per_query));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  set_hashes(plan, "reconstruct_scene_variance",
             input->controls->identity.content_hash,
             input->triangulation->identity.content_hash + "|" +
                 std::to_string(queries));
  return model::ok_result(std::move(plan));
}

rt::Task<VariancePrediction> run_information_scene_reconstruct(
    rt::InvocationContext context, Ref<ReconstructSceneVarianceInput> input,
    Ref<rt::InvocationPlan> plan) {
  if (!plan || !input || !input->controls || !input->triangulation ||
      !input->queries) {
    return rt::ready<VariancePrediction>(
        context, scene_failure<VariancePrediction>(
                     Code::invalid_request,
                     "no plan, controls, triangulation or queries supplied",
                     "the scene variance is unknown"));
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
      rt::submit_cpu<VariancePrediction>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<VariancePrediction> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return scene_failure<VariancePrediction>(
                  bridge.code, "the science context bridge cannot be created",
                  "the scene variance is unknown");
            }
            acsd::Context& science = (*bridge.value)->context();
            return reconstruct_scene_variance(science, input->controls,
                                              input->triangulation,
                                              *input->queries);
          }),
      [context, start_ns, span_id](VariancePrediction prediction)
          -> rt::Task<VariancePrediction> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.information.scene_variance";
        record.kind = rt::PerfKind::span;
        record.start_steady_ns = start_ns;
        record.wall_ns = rt::steady_ns() - start_ns;
        record.span_id = span_id;
        record.tags.component = "reconstruct_scene_variance";
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<VariancePrediction>(
            context, model::ok_result(std::move(prediction)));
      });
}

rt::Task<VariancePrediction> finalize_information_scene_reconstruct(
    rt::InvocationContext context, Ref<ReconstructSceneVarianceInput>,
    Ref<rt::InvocationPlan>, Result<VariancePrediction> result) {
  return rt::ready<VariancePrediction>(context, std::move(result));
}

Result<Done> register_information_scene(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return scene_failure<Done>(descriptor.code,
                               "the generated descriptor is unavailable",
                               "information.scene cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return scene_failure<Done>(registration.code,
                               "the process cannot be registered",
                               "information.scene is unavailable");
  }
  rt::Operation<EvaluateSceneVarianceInput, std::vector<Variance>>
      evaluate{"evaluate_scene_variance",
               {"acsd.input.EvaluateSceneVarianceInput", 1},
               {"acsd.vector.Variance", 1},
               &validate_information_scene_evaluate,
               &bounds_information_scene_evaluate,
               &plan_information_scene_evaluate,
               &run_information_scene_evaluate,
               &finalize_information_scene_evaluate};
  auto added = builder.add<EvaluateSceneVarianceInput, std::vector<Variance>>(
      *registration.value, std::move(evaluate));
  if (added.code != Code::ok) return added;

  rt::Operation<BuildSupportedTriangulationInput, Ref<SupportedTriangulation>>
      build{"build_supported_triangulation",
            {"acsd.input.BuildSupportedTriangulationInput", 1},
            {"acsd.SupportedTriangulation", 1},
            &validate_information_scene_build,
            &bounds_information_scene_build,
            &plan_information_scene_build,
            &run_information_scene_build,
            &finalize_information_scene_build};
  added = builder.add<BuildSupportedTriangulationInput,
                      Ref<SupportedTriangulation>>(*registration.value,
                                                   std::move(build));
  if (added.code != Code::ok) return added;

  rt::Operation<ReconstructSceneVarianceInput, VariancePrediction> reconstruct{
      "reconstruct_scene_variance",
      {"acsd.input.ReconstructSceneVarianceInput", 1},
      {"acsd.VariancePrediction", 1},
      &validate_information_scene_reconstruct,
      &bounds_information_scene_reconstruct,
      &plan_information_scene_reconstruct,
      &run_information_scene_reconstruct,
      &finalize_information_scene_reconstruct};
  return builder.add<ReconstructSceneVarianceInput, VariancePrediction>(
      *registration.value, std::move(reconstruct));
}

}  // namespace acsd::modules
