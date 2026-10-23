// The uncertainty.covariance typed process adapter.  The five operations call
// the one covariance implementation (src/uncertainty) through the runtime
// science-context bridge.  The plan estimates the shared root graph and the
// touched factor chains from the actual NoiseModel/CovarianceAction metadata;
// an N x N covariance is never materialised.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/uncertainty.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

namespace {

using rt::Done;

constexpr const char* kProcess = "uncertainty.covariance";

template <class T>
Result<T> failure(Code code, std::string reason, std::string impact) {
  return model::fail<T>(code, std::move(reason), kProcess, std::move(impact));
}

U64 sat_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b)
    return std::numeric_limits<U64>::max();
  return a * b;
}

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) total = sat_mul(total, extent);
  return total;
}

U64 array_bytes(const ArrayRef& array) {
  if (!array) return 0;
  return sat_mul(element_count(array->shape),
                 static_cast<U64>(model::dtype_size(array->dtype)));
}

U64 action_nnz(const Ref<LinearAction>& action) {
  if (!action) return 0;
  if (const auto* impl =
          dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (!impl->csr().indices.empty())
      return static_cast<U64>(impl->csr().indices.size());
    if (!impl->diagonal().empty())
      return static_cast<U64>(impl->diagonal().size());
    if (!impl->selection_indices().empty())
      return static_cast<U64>(impl->selection_indices().size());
    U64 child = 0;
    for (const auto& value : impl->children()) child += action_nnz(value);
    if (child > 0) return child;
    return std::max(impl->domain_size(), impl->codomain_size());
  }
  const U64 domain = action->spaces.domain ? action->spaces.domain->size : 0;
  const U64 codomain = action->spaces.codomain ? action->spaces.codomain->size : 0;
  return std::max(domain, codomain);
}

U64 action_bytes(const Ref<LinearAction>& action) {
  if (!action) return 0;
  return 1024 + sat_mul(action_nnz(action), sizeof(U64) + sizeof(double));
}

U64 noise_bytes(const Ref<NoiseModel>& model) {
  if (!model) return 0;
  U64 bytes = 2048;
  bytes += sat_mul(static_cast<U64>(model->innovations.size()), 256);
  bytes += sat_mul(static_cast<U64>(model->shared_factors.size()), 256);
  bytes += sat_mul(static_cast<U64>(model->influence.size()), 256);
  for (const auto& root : model->innovations) bytes += action_bytes(root->factor);
  for (const auto& root : model->shared_factors)
    bytes += action_bytes(root->factor);
  for (const auto& influence : model->influence)
    bytes += action_bytes(influence);
  for (const UnknownTerm& term : model->unknown_terms)
    bytes += action_bytes(term.influence) + 128;
  return bytes;
}

U64 covariance_bytes(const Ref<CovarianceAction>& covariance) {
  if (!covariance) return 0;
  U64 bytes = 2048 + noise_bytes(covariance->model) +
              action_bytes(covariance->influence);
  if (covariance->absolute_factor_bound)
    bytes += action_bytes(*covariance->absolute_factor_bound);
  return bytes;
}

U64 covariance_nnz(const Ref<CovarianceAction>& covariance) {
  if (!covariance) return 0;
  U64 nnz = action_nnz(covariance->influence);
  if (covariance->model) {
    for (const auto& root : covariance->model->innovations)
      nnz += action_nnz(root->factor);
    for (const auto& root : covariance->model->shared_factors)
      nnz += action_nnz(root->factor);
    for (const auto& influence : covariance->model->influence)
      nnz += action_nnz(influence);
  }
  return std::max<U64>(1, nnz);
}

Id digest(std::initializer_list<std::string> parts) {
  std::string text;
  for (const std::string& part : parts) {
    text += part;
    text.push_back('|');
  }
  return "sha256:" + model::sha256_hex(text);
}

rt::PlanBounds bounds_of(U64 max_block, bool spill) {
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::max<U64>(1, max_block);
  bounds.max_window = 1;
  bounds.allow_spill = spill;
  return bounds;
}

rt::InvocationPlan plan_of(const char* operation, const rt::PlanOptions& options,
                           U64 work, U64 persistent, U64 progress,
                           const Id& input_hash) {
  rt::InvocationPlan plan;
  plan.input_hash = input_hash;
  plan.request_hash = digest({Id(operation), input_hash});
  plan.options = options;
  plan.elements = std::max<U64>(1, work);
  plan.block_elements =
      options.block_elements > 0 ? options.block_elements : plan.elements;
  plan.block_elements = std::min(plan.block_elements, plan.elements);
  plan.cohorts = (plan.elements + plan.block_elements - 1) / plan.block_elements;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = 1024;
  plan.cleanup_work_upper = 1;
  plan.adaptive = false;
  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = std::max<U64>(1, plan.cohorts);
  region.persistent = plan.persistent;
  region.progress_floor = plan.progress_floor;
  rt::StageSpec stage;
  stage.id = 0;
  stage.work_upper = plan.elements;
  stage.control_bytes = plan.control_bytes;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  return plan;
}

template <class In>
Result<Done> validate_common(rt::InvocationContext ctx, const Ref<In>& input,
                             const char* operation) {
  if (!input)
    return failure<Done>(Code::invalid_request, "the typed input is null",
                         "the operation cannot run");
  if (!ctx.ids.process.empty() && ctx.ids.process != kProcess)
    return failure<Done>(Code::invalid_request,
                         "the invocation process id does not match " +
                             std::string(kProcess),
                         "the operation cannot run");
  if (!ctx.ids.operation.empty() && ctx.ids.operation != operation)
    return failure<Done>(Code::invalid_request,
                         "the invocation operation id does not match " +
                             std::string(operation),
                         "the operation cannot run");
  return model::ok_result(Done{});
}

Result<Done> check_array(const ArrayRef& array, const char* field) {
  if (!array)
    return failure<Done>(Code::invalid_request,
                         std::string("field '") + field +
                             "' is a required array reference",
                         "the operation cannot run");
  if (array->shape.empty() || element_count(array->shape) == 0)
    return failure<Done>(Code::invalid_model,
                         std::string("field '") + field + "' has no element",
                         "the operation cannot run");
  if (array->unit.scale.empty())
    return failure<Done>(Code::invalid_model,
                         std::string("field '") + field + "' has no unit",
                         "the operation cannot run");
  return model::ok_result(Done{});
}


template <class Out, class Body>
rt::Task<Out> run_operation(rt::InvocationContext ctx,
                            Ref<rt::InvocationPlan> plan, const char* operation,
                            Body body) {
  if (!plan)
    return rt::ready<Out>(ctx, failure<Out>(Code::invalid_request,
                                            "no invocation plan supplied",
                                            "the operation cannot run"));
  U64 work = plan->elements;
  if (!plan->regions.empty() && !plan->regions.front().stages.empty())
    work = plan->regions.front().stages.front().work_upper;
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.cpu_tokens = 1;
  spec.work_upper = std::max<U64>(1, work);
  spec.semantic = Id(operation);
  return rt::submit_cpu<Out>(
      ctx, spec, [body = std::move(body)](rt::InvocationContext task) mutable
                     -> Result<Out> {
        Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
            rt::detail::make_science_context(task, task.progress);
        if (!ok(bridge) || !bridge.value)
          return failure<Out>(bridge.code == Code::ok ? Code::internal
                                                      : bridge.code,
                              "the runtime science-context bridge cannot be "
                              "created",
                              "the operation cannot run");
        return body((*bridge.value)->context());
      });
}

template <class Out, class Check>
rt::Task<Out> finalize_operation(rt::InvocationContext ctx, Result<Out> partial,
                                 Check check) {
  if (partial.code != Code::ok || !partial.value)
    return rt::ready<Out>(ctx, std::move(partial));
  const Result<Done> checked = check(*partial.value);
  if (!ok(checked)) {
    Result<Out> failed = failure<Out>(
        checked.code == Code::ok ? Code::internal : checked.code,
        "the delivered uncertainty.covariance result violates its immutable "
        "output contract",
        "the result is not published");
    failed.assessment.issues.insert(failed.assessment.issues.end(),
                                    partial.assessment.issues.begin(),
                                    partial.assessment.issues.end());
    return rt::ready<Out>(ctx, std::move(failed));
  }
  return rt::ready<Out>(ctx, std::move(partial));
}

Result<Done> check_covariance(const Ref<CovarianceAction>& covariance) {
  if (!covariance)
    return failure<Done>(Code::invalid_model, "the covariance is null",
                         "not publishable");
  if (!covariance->model)
    return failure<Done>(Code::invalid_model,
                         "the covariance carries no noise model",
                         "not publishable");
  if (covariance->input_output.input.scale.empty())
    return failure<Done>(Code::invalid_model,
                         "the covariance has no input/output unit",
                         "not publishable");
  return model::ok_result(Done{});
}

Result<Done> check_array_output(const ArrayRef& output, const char* field) {
  if (!output)
    return failure<Done>(Code::invalid_model,
                         std::string(field) + " is null", "not publishable");
  if (output->shape.empty() || element_count(output->shape) == 0)
    return failure<Done>(Code::invalid_model,
                         std::string(field) + " has no element",
                         "not publishable");
  if (output->unit.scale.empty())
    return failure<Done>(Code::invalid_model,
                         std::string(field) + " has no unit", "not publishable");
  return model::ok_result(Done{});
}

// ---------------------------------------------------------------------------
// make_covariance
// ---------------------------------------------------------------------------

Result<Done> validate_make_covariance(rt::InvocationContext ctx,
                                      Ref<MakeCovarianceInput> input) {
  const Result<Done> common = validate_common(ctx, input, "make_covariance");
  if (!ok(common)) return common;
  if (!input->model)
    return failure<Done>(Code::invalid_request,
                         "field 'model' is a required object reference",
                         "the covariance cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_make_covariance(rt::InvocationContext,
                                              Ref<MakeCovarianceInput> input) {
  const U64 nnz = noise_bytes(input->model) / 16 + 1;
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_make_covariance(
    rt::InvocationContext, Ref<MakeCovarianceInput> input,
    rt::PlanOptions options) {
  const U64 roots = input->model
                        ? static_cast<U64>(input->model->innovations.size() +
                                           input->model->shared_factors.size())
                        : 0;
  const U64 work = std::max<U64>(1, roots);
  const U64 r = std::max<U64>(2048, noise_bytes(input->model));
  const U64 f = 64 * 1024;
  return model::ok_result(plan_of(
      "make_covariance", options, work, r, f,
      digest({"make_covariance",
              input->model ? input->model->identity.object_id : Id{},
              std::to_string(roots)})));
}

rt::Task<Ref<CovarianceAction>> run_make_covariance(
    rt::InvocationContext ctx, Ref<MakeCovarianceInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<CovarianceAction>>(
      ctx, plan, "make_covariance",
      [input](acsd::Context& science) -> Result<Ref<CovarianceAction>> {
        return make_covariance(science, input->model);
      });
}

rt::Task<Ref<CovarianceAction>> finalize_make_covariance(
    rt::InvocationContext ctx, Ref<MakeCovarianceInput>,
    Ref<rt::InvocationPlan>, Result<Ref<CovarianceAction>> partial) {
  return finalize_operation(ctx, std::move(partial),
                            [](const Ref<CovarianceAction>& covariance) {
                              return check_covariance(covariance);
                            });
}

// ---------------------------------------------------------------------------
// covariance_apply
// ---------------------------------------------------------------------------

Result<Done> validate_covariance_apply(rt::InvocationContext ctx,
                                       Ref<CovarianceApplyInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "covariance_apply");
  if (!ok(common)) return common;
  if (!input->covariance)
    return failure<Done>(Code::invalid_request,
                         "field 'covariance' is a required object reference",
                         "the covariance cannot be applied");
  return check_array(input->values, "values");
}

Result<rt::PlanBounds> bounds_covariance_apply(
    rt::InvocationContext, Ref<CovarianceApplyInput> input) {
  return model::ok_result(
      bounds_of(covariance_nnz(input->covariance), false));
}

Result<rt::InvocationPlan> plan_covariance_apply(
    rt::InvocationContext, Ref<CovarianceApplyInput> input,
    rt::PlanOptions options) {
  const U64 nnz = covariance_nnz(input->covariance);
  const U64 elements = std::max<U64>(1, element_count(input->values->shape));
  const U64 work = std::max<U64>(nnz, elements);
  const U64 r =
      covariance_bytes(input->covariance) + array_bytes(input->values);
  const U64 f = sat_mul(elements, sizeof(double) * 2) +
                sat_mul(nnz, sizeof(double) * 4) + 64 * 1024;
  return model::ok_result(plan_of(
      "covariance_apply", options, work, r, f,
      digest({"covariance_apply",
              input->covariance->identity.object_id,
              input->values->identity.object_id, std::to_string(nnz)})));
}

rt::Task<ArrayRef> run_covariance_apply(
    rt::InvocationContext ctx, Ref<CovarianceApplyInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<ArrayRef>(
      ctx, plan, "covariance_apply",
      [input](acsd::Context& science) -> Result<ArrayRef> {
        return covariance_apply(science, input->covariance, input->values);
      });
}

rt::Task<ArrayRef> finalize_covariance_apply(
    rt::InvocationContext ctx, Ref<CovarianceApplyInput>,
    Ref<rt::InvocationPlan>, Result<ArrayRef> partial) {
  return finalize_operation(ctx, std::move(partial),
                            [](const ArrayRef& output) {
                              return check_array_output(output,
                                                        "the applied array");
                            });
}

// ---------------------------------------------------------------------------
// quadratic_form
// ---------------------------------------------------------------------------

Result<Done> validate_quadratic_form(rt::InvocationContext ctx,
                                     Ref<QuadraticFormInput> input) {
  const Result<Done> common = validate_common(ctx, input, "quadratic_form");
  if (!ok(common)) return common;
  if (!input->covariance)
    return failure<Done>(Code::invalid_request,
                         "field 'covariance' is a required object reference",
                         "the variance cannot be computed");
  return check_array(input->functional, "functional");
}

Result<rt::PlanBounds> bounds_quadratic_form(rt::InvocationContext,
                                             Ref<QuadraticFormInput> input) {
  return model::ok_result(
      bounds_of(covariance_nnz(input->covariance), false));
}

Result<rt::InvocationPlan> plan_quadratic_form(
    rt::InvocationContext, Ref<QuadraticFormInput> input,
    rt::PlanOptions options) {
  const U64 nnz = covariance_nnz(input->covariance);
  const U64 elements = std::max<U64>(1, element_count(input->functional->shape));
  const U64 work = std::max<U64>(nnz, elements);
  const U64 r =
      covariance_bytes(input->covariance) + array_bytes(input->functional);
  const U64 f = sat_mul(elements, sizeof(double) * 2) +
                sat_mul(nnz, sizeof(double) * 4) + 64 * 1024;
  return model::ok_result(plan_of(
      "quadratic_form", options, work, r, f,
      digest({"quadratic_form", input->covariance->identity.object_id,
              input->functional->identity.object_id,
              std::to_string(nnz)})));
}

rt::Task<Variance> run_quadratic_form(rt::InvocationContext ctx,
                                      Ref<QuadraticFormInput> input,
                                      Ref<rt::InvocationPlan> plan) {
  return run_operation<Variance>(
      ctx, plan, "quadratic_form",
      [input](acsd::Context& science) -> Result<Variance> {
        return quadratic_form(science, input->covariance, input->functional);
      });
}

rt::Task<Variance> finalize_quadratic_form(
    rt::InvocationContext ctx, Ref<QuadraticFormInput>, Ref<rt::InvocationPlan>,
    Result<Variance> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const Variance& variance) {
        if (variance.unit.scale.empty())
          return failure<Done>(Code::invalid_model,
                               "the variance has no unit", "not publishable");
        if (variance.value) {
          if (!std::isfinite(*variance.value) || *variance.value < 0.0)
            return failure<Done>(Code::invalid_model,
                                 "the variance value is negative or non-finite",
                                 "not publishable");
          if (variance.numeric_enclosure &&
              !(variance.numeric_enclosure->lower <= *variance.value &&
                *variance.value <= variance.numeric_enclosure->upper))
            return failure<Done>(Code::invalid_model,
                                 "the variance is outside its numeric enclosure",
                                 "not publishable");
        } else if (variance.state == NoiseState::known) {
          // Unknown must never be reported as a zero or known variance.
          return failure<Done>(Code::invalid_model,
                               "a variance without a value claims a known state",
                               "not publishable");
        }
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// geometry_covariance_bound
// ---------------------------------------------------------------------------

Result<Done> validate_geometry_covariance_bound(
    rt::InvocationContext ctx, Ref<GeometryCovarianceBoundInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "geometry_covariance_bound");
  if (!ok(common)) return common;
  if (!input->covariance)
    return failure<Done>(Code::invalid_request,
                         "field 'covariance' is a required object reference",
                         "the covariance bound cannot be computed");
  if (!input->nominal)
    return failure<Done>(Code::invalid_request,
                         "field 'nominal' is a required object reference",
                         "the covariance bound cannot be computed");
  if (!input->absolute_error)
    return failure<Done>(Code::invalid_request,
                         "field 'absolute_error' is a required object reference",
                         "the covariance bound cannot be computed");
  return check_array(input->functional, "functional");
}

Result<rt::PlanBounds> bounds_geometry_covariance_bound(
    rt::InvocationContext, Ref<GeometryCovarianceBoundInput> input) {
  const U64 nnz = covariance_nnz(input->covariance) +
                  action_nnz(input->nominal) +
                  action_nnz(input->absolute_error);
  return model::ok_result(bounds_of(std::max<U64>(1, nnz), false));
}

Result<rt::InvocationPlan> plan_geometry_covariance_bound(
    rt::InvocationContext, Ref<GeometryCovarianceBoundInput> input,
    rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(
      1, covariance_nnz(input->covariance) + action_nnz(input->nominal) +
             action_nnz(input->absolute_error));
  const U64 elements = std::max<U64>(1, element_count(input->functional->shape));
  const U64 work = std::max<U64>(nnz, elements);
  const U64 r = covariance_bytes(input->covariance) +
                action_bytes(input->nominal) +
                action_bytes(input->absolute_error) +
                array_bytes(input->functional);
  const U64 f = sat_mul(elements, sizeof(double) * 3) +
                sat_mul(nnz, sizeof(double) * 4) + 64 * 1024;
  return model::ok_result(plan_of(
      "geometry_covariance_bound", options, work, r, f,
      digest({"geometry_covariance_bound",
              input->covariance->identity.object_id,
              input->nominal->identity.object_id,
              input->absolute_error->identity.object_id,
              input->functional->identity.object_id})));
}

rt::Task<BoundOrUnavailable> run_geometry_covariance_bound(
    rt::InvocationContext ctx, Ref<GeometryCovarianceBoundInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<BoundOrUnavailable>(
      ctx, plan, "geometry_covariance_bound",
      [input](acsd::Context& science) -> Result<BoundOrUnavailable> {
        return geometry_covariance_bound(science, input->covariance,
                                         input->nominal, input->absolute_error,
                                         input->functional);
      });
}

rt::Task<BoundOrUnavailable> finalize_geometry_covariance_bound(
    rt::InvocationContext ctx, Ref<GeometryCovarianceBoundInput>,
    Ref<rt::InvocationPlan>, Result<BoundOrUnavailable> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const BoundOrUnavailable& result) {
        if (!result.bound) {
          if (result.reason.empty())
            return failure<Done>(Code::invalid_model,
                                 "an unavailable bound has no reason",
                                 "not publishable");
          return model::ok_result(Done{});
        }
        if (!(model::dyadic_to_double(result.bound->lower) <=
              model::dyadic_to_double(result.bound->upper)))
          return failure<Done>(Code::invalid_model,
                               "the covariance bound interval is inverted",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// control_covariance
// ---------------------------------------------------------------------------

Result<Done> validate_control_covariance(rt::InvocationContext ctx,
                                         Ref<ControlCovarianceInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "control_covariance");
  if (!ok(common)) return common;
  if (!input->covariance)
    return failure<Done>(Code::invalid_request,
                         "field 'covariance' is a required object reference",
                         "the control covariance cannot be built");
  if (!input->T)
    return failure<Done>(Code::invalid_request,
                         "field 'T' is a required object reference",
                         "the control covariance cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_control_covariance(
    rt::InvocationContext, Ref<ControlCovarianceInput> input) {
  const U64 nnz =
      covariance_nnz(input->covariance) + action_nnz(input->T);
  return model::ok_result(bounds_of(std::max<U64>(1, nnz), false));
}

Result<rt::InvocationPlan> plan_control_covariance(
    rt::InvocationContext, Ref<ControlCovarianceInput> input,
    rt::PlanOptions options) {
  const U64 nnz =
      std::max<U64>(1, covariance_nnz(input->covariance) + action_nnz(input->T));
  const U64 r =
      covariance_bytes(input->covariance) + action_bytes(input->T) + 2048;
  const U64 f = 64 * 1024;
  return model::ok_result(plan_of(
      "control_covariance", options, nnz, r, f,
      digest({"control_covariance", input->covariance->identity.object_id,
              input->T->identity.object_id, std::to_string(nnz)})));
}

rt::Task<Ref<CovarianceAction>> run_control_covariance(
    rt::InvocationContext ctx, Ref<ControlCovarianceInput>,
    Ref<rt::InvocationPlan>) {
  // The process document whitelists control_covariance (T C T^T) but
  // src/uncertainty has no implementation of the public free function yet.
  // The operation is registered and validated, and the run hook reports the
  // missing implementation by name instead of fabricating a covariance.
  return rt::ready<Ref<CovarianceAction>>(
      ctx, failure<Ref<CovarianceAction>>(
               Code::unsupported,
               "control_covariance has no implementation in src/uncertainty; the "
               "typed adapter will not rebuild a second covariance formula",
               "the control-frame covariance is unavailable until the unique "
               "implementation lands"));
}

rt::Task<Ref<CovarianceAction>> finalize_control_covariance(
    rt::InvocationContext ctx, Ref<ControlCovarianceInput>,
    Ref<rt::InvocationPlan>, Result<Ref<CovarianceAction>> partial) {
  return finalize_operation(ctx, std::move(partial),
                            [](const Ref<CovarianceAction>& covariance) {
                              return check_covariance(covariance);
                            });
}

}  // namespace

// Public plan introspection for the independent process tests.
Result<rt::InvocationPlan> plan_uncertainty_covariance_make_covariance(
    rt::InvocationContext ctx, Ref<MakeCovarianceInput> input,
    rt::PlanOptions options) {
  return plan_make_covariance(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_uncertainty_covariance_covariance_apply(
    rt::InvocationContext ctx, Ref<CovarianceApplyInput> input,
    rt::PlanOptions options) {
  return plan_covariance_apply(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_uncertainty_covariance_quadratic_form(
    rt::InvocationContext ctx, Ref<QuadraticFormInput> input,
    rt::PlanOptions options) {
  return plan_quadratic_form(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_uncertainty_covariance_geometry_covariance_bound(
    rt::InvocationContext ctx, Ref<GeometryCovarianceBoundInput> input,
    rt::PlanOptions options) {
  return plan_geometry_covariance_bound(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_uncertainty_covariance_control_covariance(
    rt::InvocationContext ctx, Ref<ControlCovarianceInput> input,
    rt::PlanOptions options) {
  return plan_control_covariance(ctx, std::move(input), options);
}

Result<rt::Done> register_uncertainty_covariance(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for(kProcess);
  if (!ok(descriptor))
    return failure<Done>(
        descriptor.code,
        "the generated descriptor for uncertainty.covariance is unavailable",
        "the process cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process =
      builder.begin(*descriptor.value);
  if (!ok(process))
    return failure<Done>(process.code,
                         "the uncertainty.covariance process cannot be begun",
                         "the process cannot be registered");

  Result<Done> step = builder.add<MakeCovarianceInput, Ref<CovarianceAction>>(
      *process.value,
      rt::Operation<MakeCovarianceInput, Ref<CovarianceAction>>{
          "make_covariance", {"acsd.input.MakeCovarianceInput", 1},
          {"acsd.CovarianceAction", 1}, &validate_make_covariance,
          &bounds_make_covariance, &plan_make_covariance, &run_make_covariance,
          &finalize_make_covariance});
  if (!ok(step)) return step;

  step = builder.add<CovarianceApplyInput, ArrayRef>(
      *process.value,
      rt::Operation<CovarianceApplyInput, ArrayRef>{
          "covariance_apply", {"acsd.input.CovarianceApplyInput", 1},
          {"acsd.Array", 1}, &validate_covariance_apply,
          &bounds_covariance_apply, &plan_covariance_apply,
          &run_covariance_apply, &finalize_covariance_apply});
  if (!ok(step)) return step;

  step = builder.add<QuadraticFormInput, Variance>(
      *process.value,
      rt::Operation<QuadraticFormInput, Variance>{
          "quadratic_form", {"acsd.input.QuadraticFormInput", 1},
          {"acsd.Variance", 1}, &validate_quadratic_form,
          &bounds_quadratic_form, &plan_quadratic_form, &run_quadratic_form,
          &finalize_quadratic_form});
  if (!ok(step)) return step;

  step = builder.add<GeometryCovarianceBoundInput, BoundOrUnavailable>(
      *process.value,
      rt::Operation<GeometryCovarianceBoundInput, BoundOrUnavailable>{
          "geometry_covariance_bound",
          {"acsd.input.GeometryCovarianceBoundInput", 1},
          {"acsd.BoundOrUnavailable", 1}, &validate_geometry_covariance_bound,
          &bounds_geometry_covariance_bound, &plan_geometry_covariance_bound,
          &run_geometry_covariance_bound, &finalize_geometry_covariance_bound});
  if (!ok(step)) return step;

  step = builder.add<ControlCovarianceInput, Ref<CovarianceAction>>(
      *process.value,
      rt::Operation<ControlCovarianceInput, Ref<CovarianceAction>>{
          "control_covariance", {"acsd.input.ControlCovarianceInput", 1},
          {"acsd.CovarianceAction", 1}, &validate_control_covariance,
          &bounds_control_covariance, &plan_control_covariance,
          &run_control_covariance, &finalize_control_covariance});
  return step;
}

}  // namespace acsd::modules
