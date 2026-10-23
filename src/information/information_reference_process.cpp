// information.reference typed adapter: the estimable reference-flux variance.
//
// The design matrix h/Z, the covariance range split and the exact null-space
// are one global barrier; the plan reserves the densified design/C workspace
// and the rank certificate.  Run calls the one existing
// estimate_flux_information implementation through the science bridge.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/information.hpp>

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

constexpr const char* kOrigin = "information.reference";
constexpr const char* kProcess = "information.reference";
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

U64 action_domain(const Ref<LinearAction>& action) {
  if (!action || !action->spaces.domain) return 0;
  return action->spaces.domain->size;
}

Result<Done> validate_metadata(const Ref<EstimateFluxInformationInput>& input) {
  if (!input) return failure<Done>(Code::invalid_request, "the typed input is null",
                                   "the information is unknown");
  const InformationRequest& request = input->request;
  if (!request.h) {
    return failure<Done>(Code::invalid_request, "no flux template supplied",
                         "the information is unknown");
  }
  if (request.h->dtype != DType::f64) {
    return failure<Done>(Code::invalid_model,
                         "the flux template is not binary64",
                         "the information is unknown");
  }
  if (elements_of(request.h) == 0) {
    return failure<Done>(Code::invalid_request, "the flux template is empty",
                         "the information is unknown");
  }
  if (!request.covariance) {
    return failure<Done>(Code::invalid_request, "no covariance supplied",
                         "the information is unknown");
  }
  if (request.background) {
    const U64 domain = action_domain(request.background);
    if (domain == 0) {
      return failure<Done>(Code::invalid_model,
                           "the background action has no declared domain",
                           "the estimable space cannot be built");
    }
  }
  if (request.template_evaluation && !request.template_evaluation->values) {
    return failure<Done>(Code::invalid_model,
                         "the bound template evaluation has no values",
                         "the template column cannot be verified");
  }
  if (!request.template_evaluation && request.template_column != 0) {
    return failure<Done>(Code::invalid_request,
                         "template_column requires a bound template evaluation",
                         "the estimable space is ambiguous");
  }
  return model::ok_result(Done{});
}

Result<Done> validate_plan_options(const PlanOptions& options) {
  if (options.window == 0)
    return failure<Done>(Code::invalid_request, "the plan window is zero",
                         "the information solve cannot be scheduled");
  if (options.window > kMaxWindow)
    return failure<Done>(Code::resource_exhausted,
                         "the window exceeds the process maximum",
                         "the information solve would exceed its frontier");
  return model::ok_result(Done{});
}

Result<rt::InvocationPlan> build_plan(
    const Ref<EstimateFluxInformationInput>& input, PlanOptions options) {
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
  const InformationRequest& request = input->request;
  const U64 n = elements_of(request.h);
  const U64 background = request.background ? action_domain(request.background) : 0;
  const U64 columns = sat_add(1, background);
  const U64 block =
      std::max<U64>(1, std::min<U64>(options.block_elements == 0
                                         ? kDefaultBlockElements
                                         : options.block_elements,
                                     n));
  const U64 cohorts = (n + block - 1) / block;

  // Barrier: the dense h/Z design (n x columns), the covariance range basis
  // (n x n in the worst case) and the exact null-space certificate.
  const U64 design = sat_mul(n, sat_mul(columns, 8));
  const U64 factor = sat_mul(n, sat_mul(n, 8));
  const U64 persistent =
      sat_add(design, sat_add(factor, sat_add(64ull << 10, 1ull << 20)));
  const U64 progress_floor =
      sat_add(sat_mul(n, sat_mul(columns, 8)), kWorkspaceUpper);
  const U64 work_upper = sat_add(
      sat_mul(n, sat_add(sat_mul(columns, columns), sat_mul(n, 2))), 1024);

  rt::InvocationPlan plan;
  plan.epoch = 1;
  plan.options = options;
  plan.options.block_elements = block;
  plan.options.window =
      std::max<U32>(1, std::min<U32>(options.window, kMaxWindow));
  plan.elements = n;
  plan.block_elements = block;
  plan.cohorts = cohorts;
  plan.persistent = capacity(persistent);
  plan.progress_floor = capacity(progress_floor);
  plan.elastic = capacity(0);
  plan.control_bytes = kControlBytes;
  plan.cleanup_bytes = 8ull << 10;
  plan.cleanup_work_upper = 512;
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
  stage.output_upper = capacity(sat_mul(columns, 8 * 2));
  stage.control_bytes = kControlBytes;
  stage.work_upper = work_upper;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));

  model::Sha256 input_hash;
  input_hash.field("process", kProcess);
  input_hash.field("h", request.h->identity.content_hash);
  input_hash.field("background", request.background
                                     ? request.background->identity.content_hash
                                     : std::string("<absent>"));
  model::Sha256 request_hash;
  request_hash.update_u64(n);
  request_hash.update_u64(columns);
  request_hash.field_f64("rank_relative", request.solve.rank_relative);
  request_hash.field("estimator", request.reference.id);
  model::Sha256 algorithm_hash;
  algorithm_hash.field("algorithm", "estimate_flux_information/v1");
  plan.input_hash = model::content_id(input_hash.hex());
  plan.request_hash = model::content_id(request_hash.hex());
  plan.algorithm_hash = model::content_id(algorithm_hash.hex());
  return model::ok_result(std::move(plan));
}

}  // namespace

Result<Done> validate_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput> input) {
  return validate_metadata(input);
}

Result<PlanBounds> bounds_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput> input) {
  Result<Done> validated = validate_metadata(input);
  if (validated.code != Code::ok) {
    Result<PlanBounds> out;
    out.code = validated.code;
    out.assessment = validated.assessment;
    return out;
  }
  const U64 n = elements_of(input->request.h);
  PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements =
      std::max<U64>(1, std::min<U64>(kDefaultBlockElements, n));
  bounds.max_window = kMaxWindow;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput> input,
    PlanOptions options) {
  return build_plan(input, options);
}

rt::Task<Information> run_information_reference(
    rt::InvocationContext context, Ref<EstimateFluxInformationInput> input,
    Ref<rt::InvocationPlan> plan) {
  if (!plan || !input) {
    return rt::ready<Information>(
        context, failure<Information>(Code::invalid_request,
                                      "no plan or input supplied",
                                      "the information is unknown"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = plan->elements * 16 + 1;
  spec.semantic = "service.science";
  return rt::then(
      context,
      rt::submit_cpu<Information>(
          context, spec,
          [input](rt::InvocationContext task) -> Result<Information> {
            const rt::ArenaRef arena =
                task.progress ? task.progress : task.persistent;
            auto bridge = rt::detail::make_science_context(task, arena);
            if (bridge.code != Code::ok || !bridge.value) {
              return failure<Information>(
                  bridge.code, "the science context bridge cannot be created",
                  "the information is unknown");
            }
            acsd::Context& science = (*bridge.value)->context();
            return estimate_flux_information(science, input->request);
          }),
      [context, start_ns, span_id](
          Information information) -> rt::Task<Information> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.information.reference_flux";
        record.kind = rt::PerfKind::counter;
        record.count = information.rank.rank;
        record.start_steady_ns = start_ns;
        record.span_id = span_id;
        record.tags.component = kProcess;
        ACSD_DEV_PERF(context, record);
#else
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<Information>(context,
                                      model::ok_result(std::move(information)));
      });
}

rt::Task<Information> finalize_information_reference(
    rt::InvocationContext context, Ref<EstimateFluxInformationInput>,
    Ref<rt::InvocationPlan>, Result<Information> result) {
  return rt::ready<Information>(context, std::move(result));
}

Result<Done> register_information_reference(rt::RegistryBuilder& builder) {
  auto descriptor = acsd::generated::descriptor_for(kProcess);
  if (descriptor.code != Code::ok || !descriptor.value) {
    return failure<Done>(descriptor.code,
                         "the generated descriptor is unavailable",
                         "information.reference cannot be registered");
  }
  auto registration = builder.begin(*descriptor.value);
  if (registration.code != Code::ok || !registration.value) {
    return failure<Done>(registration.code, "the process cannot be registered",
                         "information.reference is unavailable");
  }
  rt::Operation<EstimateFluxInformationInput, Information> operation{
      "estimate_flux_information",
      {"acsd.input.EstimateFluxInformationInput", 1},
      {"acsd.Information", 1},
      &validate_information_reference,
      &bounds_information_reference,
      &plan_information_reference,
      &run_information_reference,
      &finalize_information_reference};
  return builder.add<EstimateFluxInformationInput, Information>(
      *registration.value, std::move(operation));
}

}  // namespace acsd::modules
