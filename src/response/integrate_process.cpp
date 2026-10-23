// The response.integrate typed process adapter (M5).
//
// integrate_response is the unique synchronous implementation of the response
// module; this adapter registers it as a typed process and gives it an honest
// resource model.  The legacy entry point performs the whole integration in one
// call, so the plan describes one atomic barrier segment sized from the actual
// array metadata (no fabricated per-cohort pipeline, no second pool).
#include <acsd/modules/response.hpp>

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/process.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

using rt::Done;

namespace {

constexpr const char* kProcess = "response.integrate";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 16ull << 10;
constexpr U64 kCleanupWorkUpper = 16;
constexpr U64 kWorkspaceUpper = 1ull << 20;

U64 saturating_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b) return std::numeric_limits<U64>::max();
  return a * b;
}
U64 saturating_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b) return std::numeric_limits<U64>::max();
  return a + b;
}

template <class T>
Result<T> adapter_failure(Code code, std::string reason, std::string impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, std::move(reason), kProcess, std::move(impact));
  return out;
}

std::string hex_id(const std::string& text) { return "sha256:" + model::sha256_hex(text); }

std::string number_text(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

U64 element_count(const ArrayRef& array) {
  if (!array) return 0;
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) return 0;
    total = saturating_mul(total, extent);
  }
  return array->shape.empty() ? 0 : total;
}

U64 itemsize(DType dtype) {
  switch (dtype) {
    case DType::f64: return 8;
    case DType::f32: return 4;
    case DType::u64: return 8;
    case DType::u32: return 4;
    case DType::u16: return 2;
    case DType::u8: return 1;
    case DType::dyadic: return 0;
  }
  return 0;
}

std::string array_text(const ArrayRef& array) {
  if (!array) return "<absent>";
  std::string text = array->identity.content_hash + "|" + array->index_id + "|" +
                     array->unit.scale + "|" + std::to_string(array->unit.linear_power) + "|" +
                     std::to_string(static_cast<int>(array->dtype)) + "|";
  for (U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

std::string budget_text(const NumericBudget& budget) {
  return number_text(budget.tolerance.absolute) + "|" +
         number_text(budget.tolerance.relative) + "|" +
         number_text(budget.tolerance.absolute_floor) + "|" + budget.tolerance.unit.scale + "|" +
         std::to_string(budget.max_iterations) + "|" +
         std::to_string(budget.arithmetic.initial_bits) + "|" +
         std::to_string(budget.arithmetic.max_bits);
}

Result<Done> require_input(Ref<IntegrateResponseInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "the response integral cannot be validated");
  if (!input->response)
    return adapter_failure<Done>(Code::invalid_request, "no response supplied",
                                 "the response integral cannot be validated");
  if (!input->basis)
    return adapter_failure<Done>(Code::invalid_request, "no spectral basis supplied",
                                 "the response integral cannot be validated");
  if (!input->response->wavelength_nm || !input->response->throughput)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the response has no wavelength or throughput array",
                                 "the response integral cannot be validated");
  if (!input->basis->knots_nm || !input->basis->basis_coefficients)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the basis has no knots or coefficient array",
                                 "the response integral cannot be validated");
  if (input->basis->coefficient_count == 0)
    return adapter_failure<Done>(Code::invalid_model, "the basis has no coefficients",
                                 "the response integral cannot be validated");
  if (!(input->response->support_nm.lower <= input->response->support_nm.upper))
    return adapter_failure<Done>(Code::invalid_model, "the response support is not an interval",
                                 "the response integral cannot be validated");
  if (!(input->basis->support_nm.lower <= input->basis->support_nm.upper))
    return adapter_failure<Done>(Code::invalid_model, "the basis support is not an interval",
                                 "the response integral cannot be validated");
  if (input->basis->knots_nm->index_id != input->basis->basis_coefficients->index_id)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the basis arrays do not share an index",
                                 "the response integral cannot be validated");
  if (!std::isfinite(input->request.tolerance.absolute) ||
      !std::isfinite(input->request.tolerance.relative) ||
      !std::isfinite(input->request.tolerance.absolute_floor))
    return adapter_failure<Done>(Code::invalid_request, "the numeric tolerance is not finite",
                                 "the response integral cannot be validated");
  if (input->request.arithmetic.initial_bits == 0 ||
      input->request.arithmetic.max_bits < input->request.arithmetic.initial_bits)
    return adapter_failure<Done>(Code::invalid_request, "the arithmetic bit budget is not ordered",
                                 "the response integral cannot be validated");
  return model::ok_result(Done{});
}

U64 response_nodes(const IntegrateResponseInput& input) {
  return element_count(input.response->wavelength_nm);
}
U64 basis_knots(const IntegrateResponseInput& input) {
  return element_count(input.basis->knots_nm);
}
U64 basis_coefficients(const IntegrateResponseInput& input) {
  return element_count(input.basis->basis_coefficients);
}

U64 integrate_work(const IntegrateResponseInput& input) {
  // One work unit per response node and per basis knot, plus one per
  // coefficient column: the integration visits the fixed panel order.
  return saturating_add(saturating_add(response_nodes(input), basis_knots(input)),
                        basis_coefficients(input));
}

Result<Done> validate_integrate(rt::InvocationContext, Ref<IntegrateResponseInput> input) {
  return require_input(input);
}

Result<rt::PlanBounds> bounds_integrate(rt::InvocationContext, Ref<IntegrateResponseInput> input) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the response input is invalid",
                                           "the response integral cannot be planned");
  const U64 elements = std::max<U64>(basis_knots(*input), 1);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_integrate(rt::InvocationContext, Ref<IntegrateResponseInput> input,
                                          rt::PlanOptions options) {
  Result<Done> validation = require_input(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the response input is invalid",
                                               "the response integral cannot be planned");
  const U64 elements = std::max<U64>(basis_knots(*input), 1);
  if (options.block_elements == 0 || options.block_elements > elements)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "the response integral cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic integral uses one window",
                                               "the response integral cannot be planned");
  const U64 nodes = response_nodes(*input);
  const U64 coefficients = basis_coefficients(*input);
  const U64 knots = basis_knots(*input);
  const U64 input_payload = saturating_mul(16, saturating_add(nodes, knots));
  const U64 output_payload = saturating_mul(16, coefficients);
  const U64 progress = saturating_add(
      saturating_add(input_payload, output_payload),
      saturating_add(kWorkspaceUpper, saturating_add(kCohortControlBytes, kStageControlBytes)));
  const U64 persistent =
      saturating_add(saturating_add(output_payload, 32ull << 10), kPlanControlBytes);
  if (progress == std::numeric_limits<U64>::max() ||
      persistent == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "the response integral cannot be planned");

  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = elements;
  plan.block_elements = options.block_elements;
  plan.cohorts = 1;  // the unique implementation integrates atomically
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = saturating_add(kPlanControlBytes, kCohortControlBytes);
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = hex_id("response|" + array_text(input->response->wavelength_nm) + "|" +
                           array_text(input->response->throughput) + "|support|" +
                           number_text(input->response->support_nm.lower) + "," +
                           number_text(input->response->support_nm.upper));
  plan.request_hash = hex_id("basis|" + array_text(input->basis->knots_nm) + "|" +
                             array_text(input->basis->basis_coefficients) + "|degree|" +
                             std::to_string(input->basis->degree) + "|count|" +
                             std::to_string(input->basis->coefficient_count) + "|budget|" +
                             budget_text(input->request));
  plan.algorithm_hash = hex_id("response.integrate|piecewise_polynomial_panels_v1|1");

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent.host_bytes = persistent;
  region.progress_floor.host_bytes = progress;

  rt::StageSpec stage;
  stage.id = 0;
  stage.scratch.host_bytes = progress;
  stage.output_upper.host_bytes = output_payload;
  stage.control_bytes = kStageControlBytes;
  stage.work_upper = integrate_work(*input);
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

rt::Task<Ref<SpectralPrediction>> run_integrate(rt::InvocationContext context,
                                                Ref<IntegrateResponseInput> input,
                                                Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<SpectralPrediction>>(
        context, adapter_failure<Ref<SpectralPrediction>>(Code::invalid_request,
                                                          "the typed input is null",
                                                          "the response integral cannot run"));
  if (!plan)
    return rt::ready<Ref<SpectralPrediction>>(
        context, adapter_failure<Ref<SpectralPrediction>>(Code::invalid_request,
                                                          "no invocation plan supplied",
                                                          "the response integral cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = integrate_work(*input);
  spec.semantic = "response.integrate.integrate_response";
  return rt::submit_cpu<Ref<SpectralPrediction>>(
      context, spec,
      [input](rt::InvocationContext task) -> Result<Ref<SpectralPrediction>> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Ref<SpectralPrediction>>(
              bridge.code, "the science context bridge cannot be created",
              "the response integral cannot run");
        return acsd::integrate_response((*bridge.value)->context(), input->response, input->basis,
                                        input->request);
      });
}

rt::Task<Ref<SpectralPrediction>> finalize_integrate(rt::InvocationContext context,
                                                     Ref<IntegrateResponseInput>,
                                                     Ref<rt::InvocationPlan>,
                                                     Result<Ref<SpectralPrediction>> prior) {
  if (prior.code == Code::ok) return rt::ready<Ref<SpectralPrediction>>(context, std::move(prior));
  Result<Ref<SpectralPrediction>> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Ref<SpectralPrediction>>(context, std::move(kept));
}

}  // namespace

Result<Done> register_integrate_response(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for("response.integrate");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code,
                                 "the response.integrate descriptor is unavailable",
                                 "response.integrate cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code,
                                 "the response.integrate process cannot be started",
                                 "response.integrate cannot be registered");
  const rt::Operation<IntegrateResponseInput, Ref<SpectralPrediction>> operation{
      "integrate_response",
      rt::TypeId{"acsd.input.IntegrateResponseInput", 1},
      rt::TypeId{"acsd.SpectralPrediction", 1},
      &validate_integrate,
      &bounds_integrate,
      &plan_integrate,
      &run_integrate,
      &finalize_integrate};
  return builder.add<IntegrateResponseInput, Ref<SpectralPrediction>>(*process.value, operation);
}

// Plan introspection for validation/process/response.integrate.
Result<rt::InvocationPlan> plan_integrate_response_op(
    rt::InvocationContext ctx, Ref<IntegrateResponseInput> input, rt::PlanOptions options) {
  return plan_integrate(ctx, input, options);
}

}  // namespace acsd::modules
