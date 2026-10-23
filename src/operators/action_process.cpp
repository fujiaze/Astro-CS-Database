// The operators.action typed process adapter.  Every entry point calls the one
// shared LinearAction implementation (src/operators) through the runtime
// science-context bridge; the adapter owns validation, admission and the
// immutable output check only.
//
// The plan uses the real CSR/diagonal/selection payload of the concrete action
// when it is the local ActionImpl; for an action of another module it falls
// back to the declared domain/codomain sizes (dense worst case) and records
// that fact via a conservative bound.  No N x N covariance or dense matrix is
// materialised by this process.
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/operators.hpp>

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

constexpr const char* kProcess = "operators.action";
constexpr U64 kElementBytes = sizeof(double);

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

U64 action_domain(const Ref<LinearAction>& action) {
  if (!action) return 0;
  if (const auto* impl =
          dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (impl->domain_size() > 0) return impl->domain_size();
    const auto& children = impl->children();
    if (children.size() == 1) return action_domain(children.front());
  }
  return action->spaces.domain ? action->spaces.domain->size : 0;
}

U64 action_codomain(const Ref<LinearAction>& action) {
  if (!action) return 0;
  if (const auto* impl =
          dynamic_cast<const operators::ActionImpl*>(action.get())) {
    if (impl->codomain_size() > 0) return impl->codomain_size();
    const auto& children = impl->children();
    if (children.size() == 1) return action_codomain(children.front());
  }
  return action->spaces.codomain ? action->spaces.codomain->size : 0;
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
    U64 child_nnz = 0;
    for (const auto& child : impl->children()) child_nnz += action_nnz(child);
    if (child_nnz > 0) return child_nnz;
    return std::max(impl->domain_size(), impl->codomain_size());
  }
  // A foreign action does not expose its sparsity; use the dense worst case of
  // the declared spaces rather than inventing a smaller number.
  return std::max(action_domain(action), action_codomain(action));
}

U64 action_metadata_bytes(const Ref<LinearAction>& action) {
  if (!action) return 0;
  return 1024 + sat_mul(action_nnz(action), sizeof(U64) + kElementBytes);
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
  if (array->shape.empty())
    return failure<Done>(Code::invalid_model,
                         std::string("field '") + field + "' has an empty shape",
                         "the operation cannot run");
  if (element_count(array->shape) == 0)
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
        "the delivered operators.action result violates its immutable output "
        "contract",
        "the result is not published");
    failed.assessment.issues.insert(failed.assessment.issues.end(),
                                    partial.assessment.issues.begin(),
                                    partial.assessment.issues.end());
    return rt::ready<Out>(ctx, std::move(failed));
  }
  return rt::ready<Out>(ctx, std::move(partial));
}

const std::vector<Ref<LinearAction>>& children_of(const MakeActionInput& input) {
  static const std::vector<Ref<LinearAction>> empty;
  return input.children ? *input.children : empty;
}

const std::vector<ArrayRef>& arrays_of(const MakeActionInput& input) {
  static const std::vector<ArrayRef> empty;
  return input.arrays ? *input.arrays : empty;
}

U64 make_action_nnz(const MakeActionInput& input) {
  const auto& recipe = input.recipe;
  const auto& arrays = arrays_of(input);
  switch (recipe.kind) {
    case ActionRecipe::Kind::csr:
      if (arrays.size() >= 2 && arrays[1])
        return arrays[1]->shape.empty() ? 0 : arrays[1]->shape[0];
      break;
    case ActionRecipe::Kind::diagonal:
      if (!arrays.empty() && arrays[0]) return element_count(arrays[0]->shape);
      break;
    case ActionRecipe::Kind::selection:
      if (!arrays.empty() && arrays[0]) return element_count(arrays[0]->shape);
      break;
    case ActionRecipe::Kind::compose: {
      U64 nnz = 0;
      for (const auto& child : children_of(input)) nnz += action_nnz(child);
      if (nnz > 0) return nnz;
      break;
    }
    default:
      break;
  }
  const U64 domain = recipe.spaces.domain ? recipe.spaces.domain->size : 0;
  const U64 codomain = recipe.spaces.codomain ? recipe.spaces.codomain->size : 0;
  return std::max(domain, codomain);
}

// ---------------------------------------------------------------------------
// make_action
// ---------------------------------------------------------------------------

Result<Done> validate_make_action(rt::InvocationContext ctx,
                                  Ref<MakeActionInput> input) {
  const Result<Done> common = validate_common(ctx, input, "make_action");
  if (!ok(common)) return common;
  if (input->recipe.version == 0)
    return failure<Done>(Code::invalid_request,
                         "the recipe has version 0",
                         "the action cannot be built");
  if (input->recipe.algorithm.empty())
    return failure<Done>(Code::invalid_request,
                         "the recipe has no algorithm identity",
                         "the action cannot be built");
  if (input->recipe.children.size() != children_of(*input).size())
    return failure<Done>(Code::invalid_request,
                         "the recipe children do not match the supplied action "
                         "dependencies",
                         "the action cannot be built");
  if (input->recipe.arrays.size() != arrays_of(*input).size())
    return failure<Done>(Code::invalid_request,
                         "the recipe arrays do not match the supplied array "
                         "dependencies",
                         "the action cannot be built");
  for (const ArrayRef& array : arrays_of(*input)) {
    const Result<Done> checked = check_array(array, "arrays");
    if (!ok(checked)) return checked;
  }
  if (input->recipe.kind == ActionRecipe::Kind::solve &&
      !input->recipe.solve)
    return failure<Done>(Code::invalid_request,
                         "a solve recipe needs a solve specification",
                         "the action cannot be built");
  if (input->recipe.kind != ActionRecipe::Kind::solve && input->recipe.solve)
    return failure<Done>(Code::invalid_request,
                         "only a solve recipe may carry a solve specification",
                         "the action cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_make_action(rt::InvocationContext,
                                          Ref<MakeActionInput> input) {
  const U64 nnz = std::max<U64>(1, make_action_nnz(*input));
  return model::ok_result(bounds_of(nnz, true));
}

Result<rt::InvocationPlan> plan_make_action(rt::InvocationContext,
                                            Ref<MakeActionInput> input,
                                            rt::PlanOptions options) {
  const auto& arrays = arrays_of(*input);
  U64 input_bytes = 0;
  U64 elements = 0;
  for (const ArrayRef& array : arrays) {
    input_bytes += array_bytes(array);
    elements += element_count(array->shape);
  }
  U64 children_bytes = 0;
  for (const auto& child : children_of(*input))
    children_bytes += action_metadata_bytes(child);
  const U64 nnz = std::max<U64>(1, make_action_nnz(*input));
  const U64 work = std::max<U64>(elements, nnz);
  const U64 r = input_bytes + children_bytes + 4096;
  const U64 f = sat_mul(nnz, sizeof(double) * 2) + 64 * 1024;
  return model::ok_result(plan_of(
      "make_action", options, work, r, f,
      digest({"make_action", input->recipe.algorithm,
              std::to_string(input->recipe.version),
              std::to_string(input->recipe.children.size())})));
}

rt::Task<Ref<LinearAction>> run_make_action(rt::InvocationContext ctx,
                                            Ref<MakeActionInput> input,
                                            Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<LinearAction>>(
      ctx, plan, "make_action",
      [input](acsd::Context& science) -> Result<Ref<LinearAction>> {
        const auto& children = children_of(*input);
        const auto& arrays = arrays_of(*input);
        return make_action(science, input->recipe, children, arrays);
      });
}

rt::Task<Ref<LinearAction>> finalize_make_action(
    rt::InvocationContext ctx, Ref<MakeActionInput>, Ref<rt::InvocationPlan>,
    Result<Ref<LinearAction>> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const Ref<LinearAction>& action) {
        if (!action)
          return failure<Done>(Code::invalid_model, "the linear action is null",
                               "not publishable");
        if (action_domain(action) == 0 && action_codomain(action) == 0)
          return failure<Done>(Code::invalid_model,
                               "the linear action declares no domain/codomain",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// apply / transpose_apply
// ---------------------------------------------------------------------------

bool apply_input_matches(const Ref<LinearAction>& action, const ArrayRef& values,
                         bool transpose) {
  if (!action || !values) return false;
  const U64 expected = transpose ? action_codomain(action)
                                 : action_domain(action);
  if (expected == 0) return true;
  return element_count(values->shape) == expected;
}

Result<Done> check_apply_shape(const ApplyInput& input, bool transpose) {
  if (!apply_input_matches(input.action, input.values, transpose))
    return failure<Done>(Code::invalid_model,
                         "the applied vector length does not match the action "
                         "space",
                         "the action cannot be applied");
  return model::ok_result(Done{});
}

Result<Done> check_transpose_shape(const TransposeApplyInput& input) {
  if (!apply_input_matches(input.action, input.values, true))
    return failure<Done>(Code::invalid_model,
                         "the transposed vector length does not match the "
                         "action codomain",
                         "the action cannot be transposed");
  return model::ok_result(Done{});
}



Result<Done> validate_apply(rt::InvocationContext ctx, Ref<ApplyInput> input) {
  const Result<Done> common = validate_common(ctx, input, "apply");
  if (!ok(common)) return common;
  if (!input->action)
    return failure<Done>(Code::invalid_request,
                         "field 'action' is a required object reference",
                         "the action cannot be applied");
  const Result<Done> array = check_array(input->values, "values");
  if (!ok(array)) return array;
  return check_apply_shape(*input, false);
}

Result<Done> validate_transpose_apply(rt::InvocationContext ctx,
                                      Ref<TransposeApplyInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "transpose_apply");
  if (!ok(common)) return common;
  if (!input->action)
    return failure<Done>(Code::invalid_request,
                         "field 'action' is a required object reference",
                         "the action cannot be transposed");
  const Result<Done> array = check_array(input->values, "values");
  if (!ok(array)) return array;
  return check_transpose_shape(*input);
}

Result<rt::PlanBounds> bounds_apply(rt::InvocationContext,
                                    Ref<ApplyInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::PlanBounds> bounds_transpose_apply(rt::InvocationContext,
                                              Ref<TransposeApplyInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_apply(rt::InvocationContext,
                                      Ref<ApplyInput> input,
                                      rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  const U64 output_elements =
      std::max<U64>(1, action_codomain(input->action));
  const U64 work = std::max<U64>(nnz, element_count(input->values->shape));
  const U64 r = array_bytes(input->values) + action_metadata_bytes(input->action);
  const U64 f = sat_mul(output_elements, kElementBytes) +
                sat_mul(nnz, sizeof(double) * 4) + 64 * 1024;
  return model::ok_result(plan_of(
      "apply", options, work, r, f,
      digest({"apply", input->action->identity.object_id,
              input->values->identity.object_id, std::to_string(nnz)})));
}

Result<rt::InvocationPlan> plan_transpose_apply(
    rt::InvocationContext, Ref<TransposeApplyInput> input,
    rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  const U64 output_elements = std::max<U64>(1, action_domain(input->action));
  const U64 work = std::max<U64>(nnz, element_count(input->values->shape));
  const U64 r = array_bytes(input->values) + action_metadata_bytes(input->action);
  const U64 f = sat_mul(output_elements, kElementBytes) +
                sat_mul(nnz, sizeof(double) * 4) + 64 * 1024;
  return model::ok_result(plan_of(
      "transpose_apply", options, work, r, f,
      digest({"transpose_apply", input->action->identity.object_id,
              input->values->identity.object_id, std::to_string(nnz)})));
}

rt::Task<ArrayRef> run_apply(rt::InvocationContext ctx, Ref<ApplyInput> input,
                             Ref<rt::InvocationPlan> plan) {
  return run_operation<ArrayRef>(
      ctx, plan, "apply",
      [input](acsd::Context& science) -> Result<ArrayRef> {
        return apply(science, input->action, input->values);
      });
}

rt::Task<ArrayRef> run_transpose_apply(rt::InvocationContext ctx,
                                       Ref<TransposeApplyInput> input,
                                       Ref<rt::InvocationPlan> plan) {
  return run_operation<ArrayRef>(
      ctx, plan, "transpose_apply",
      [input](acsd::Context& science) -> Result<ArrayRef> {
        return transpose_apply(science, input->action, input->values);
      });
}

Result<Done> check_action_output(const ArrayRef& output) {
  if (!output)
    return failure<Done>(Code::invalid_model, "the applied array is null",
                         "not publishable");
  if (output->shape.empty() || element_count(output->shape) == 0)
    return failure<Done>(Code::invalid_model,
                         "the applied array has no element", "not publishable");
  if (output->unit.scale.empty())
    return failure<Done>(Code::invalid_model,
                         "the applied array has no unit", "not publishable");
  return model::ok_result(Done{});
}

rt::Task<ArrayRef> finalize_apply(rt::InvocationContext ctx, Ref<ApplyInput>,
                                  Ref<rt::InvocationPlan>,
                                  Result<ArrayRef> partial) {
  return finalize_operation(ctx, std::move(partial),
                            [](const ArrayRef& output) {
                              return check_action_output(output);
                            });
}

rt::Task<ArrayRef> finalize_transpose_apply(
    rt::InvocationContext ctx, Ref<TransposeApplyInput>,
    Ref<rt::InvocationPlan>, Result<ArrayRef> partial) {
  return finalize_operation(ctx, std::move(partial),
                            [](const ArrayRef& output) {
                              return check_action_output(output);
                            });
}

// ---------------------------------------------------------------------------
// compose
// ---------------------------------------------------------------------------

Result<Done> validate_compose(rt::InvocationContext ctx,
                              Ref<ComposeInput> input) {
  const Result<Done> common = validate_common(ctx, input, "compose");
  if (!ok(common)) return common;
  if (!input->outer)
    return failure<Done>(Code::invalid_request,
                         "field 'outer' is a required object reference",
                         "the composition cannot be built");
  if (!input->inner)
    return failure<Done>(Code::invalid_request,
                         "field 'inner' is a required object reference",
                         "the composition cannot be built");
  const U64 outer_domain = action_domain(input->outer);
  const U64 inner_codomain = action_codomain(input->inner);
  if (outer_domain > 0 && inner_codomain > 0 && outer_domain != inner_codomain)
    return failure<Done>(Code::invalid_model,
                         "the inner codomain does not match the outer domain",
                         "the composition cannot be built");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_compose(rt::InvocationContext,
                                      Ref<ComposeInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->outer) +
                                        action_nnz(input->inner));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_compose(rt::InvocationContext,
                                        Ref<ComposeInput> input,
                                        rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->outer) +
                                        action_nnz(input->inner));
  const U64 work = nnz;
  const U64 r = action_metadata_bytes(input->outer) +
                action_metadata_bytes(input->inner);
  const U64 f = 64 * 1024;
  return model::ok_result(plan_of(
      "compose", options, work, r, f,
      digest({"compose", input->outer->identity.object_id,
              input->inner->identity.object_id})));
}

rt::Task<Ref<LinearAction>> run_compose(rt::InvocationContext ctx,
                                        Ref<ComposeInput> input,
                                        Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<LinearAction>>(
      ctx, plan, "compose",
      [input](acsd::Context& science) -> Result<Ref<LinearAction>> {
        return compose(science, input->outer, input->inner);
      });
}

rt::Task<Ref<LinearAction>> finalize_compose(
    rt::InvocationContext ctx, Ref<ComposeInput>, Ref<rt::InvocationPlan>,
    Result<Ref<LinearAction>> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const Ref<LinearAction>& action) {
        if (!action)
          return failure<Done>(Code::invalid_model,
                               "the composed action is null", "not publishable");
        if (action_domain(action) == 0 && action_codomain(action) == 0)
          return failure<Done>(Code::invalid_model,
                               "the composed action declares no domain/codomain",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// action.dependency_blocks
// ---------------------------------------------------------------------------

Result<Done> validate_dependency_blocks(
    rt::InvocationContext ctx, Ref<ActionDependencyBlocksInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "action.dependency_blocks");
  if (!ok(common)) return common;
  if (!input->action)
    return failure<Done>(Code::invalid_request,
                         "field 'action' is a required object reference",
                         "the dependency blocks cannot be listed");
  const U64 size = input->transpose ? action_codomain(input->action)
                                    : action_domain(input->action);
  if (size > 0) {
    if (input->range.begin > size ||
        input->range.count > size - input->range.begin)
      return failure<Done>(Code::invalid_request,
                           "the requested block range exceeds the action space",
                           "the dependency blocks cannot be listed");
  }
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_dependency_blocks(
    rt::InvocationContext, Ref<ActionDependencyBlocksInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_dependency_blocks(
    rt::InvocationContext, Ref<ActionDependencyBlocksInput> input,
    rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  const U64 work = std::max<U64>(1, input->range.count);
  const U64 r = action_metadata_bytes(input->action) + 1024;
  const U64 f = sat_mul(nnz, sizeof(Id)) + 64 * 1024;
  return model::ok_result(plan_of(
      "action.dependency_blocks", options, work, r, f,
      digest({"action.dependency_blocks", input->action->identity.object_id,
              std::to_string(input->range.begin),
              std::to_string(input->range.count),
              input->transpose ? "1" : "0"})));
}

rt::Task<std::vector<Id>> run_dependency_blocks(
    rt::InvocationContext ctx, Ref<ActionDependencyBlocksInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<std::vector<Id>>(
      ctx, plan, "action.dependency_blocks",
      [input](acsd::Context&) -> Result<std::vector<Id>> {
        return input->action->dependency_blocks(input->range, input->transpose);
      });
}

rt::Task<std::vector<Id>> finalize_dependency_blocks(
    rt::InvocationContext ctx, Ref<ActionDependencyBlocksInput>,
    Ref<rt::InvocationPlan>, Result<std::vector<Id>> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const std::vector<Id>& blocks) {
        for (const Id& block : blocks)
          if (block.empty())
            return failure<Done>(Code::invalid_model,
                                 "a dependency block has an empty id",
                                 "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// action.serialize_recipe
// ---------------------------------------------------------------------------

Result<Done> validate_serialize_recipe(
    rt::InvocationContext ctx, Ref<ActionSerializeRecipeInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "action.serialize_recipe");
  if (!ok(common)) return common;
  if (!input->action)
    return failure<Done>(Code::invalid_request,
                         "field 'action' is a required object reference",
                         "the recipe cannot be serialized");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_serialize_recipe(
    rt::InvocationContext, Ref<ActionSerializeRecipeInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_serialize_recipe(
    rt::InvocationContext, Ref<ActionSerializeRecipeInput> input,
    rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(plan_of(
      "action.serialize_recipe", options, std::max<U64>(1, nnz),
      action_metadata_bytes(input->action), 64 * 1024,
      digest({"action.serialize_recipe", input->action->identity.object_id})));
}

rt::Task<ActionRecipe> run_serialize_recipe(
    rt::InvocationContext ctx, Ref<ActionSerializeRecipeInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<ActionRecipe>(
      ctx, plan, "action.serialize_recipe",
      [input](acsd::Context&) -> Result<ActionRecipe> {
        return input->action->serialize_recipe();
      });
}

rt::Task<ActionRecipe> finalize_serialize_recipe(
    rt::InvocationContext ctx, Ref<ActionSerializeRecipeInput>,
    Ref<rt::InvocationPlan>, Result<ActionRecipe> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const ActionRecipe& recipe) {
        if (recipe.algorithm.empty())
          return failure<Done>(Code::invalid_model,
                               "the serialized recipe has no algorithm identity",
                               "not publishable");
        if (recipe.version == 0)
          return failure<Done>(Code::invalid_model,
                               "the serialized recipe has version 0",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// action.row_enclosure
// ---------------------------------------------------------------------------

Result<Done> validate_row_enclosure(rt::InvocationContext ctx,
                                    Ref<ActionRowEnclosureInput> input) {
  const Result<Done> common =
      validate_common(ctx, input, "action.row_enclosure");
  if (!ok(common)) return common;
  if (!input->action)
    return failure<Done>(Code::invalid_request,
                         "field 'action' is a required object reference",
                         "the row enclosure cannot be computed");
  const U64 codomain = action_codomain(input->action);
  if (codomain > 0 && input->row >= codomain)
    return failure<Done>(Code::invalid_request,
                         "the requested row exceeds the action codomain",
                         "the row enclosure cannot be computed");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_row_enclosure(
    rt::InvocationContext, Ref<ActionRowEnclosureInput> input) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  return model::ok_result(bounds_of(nnz, false));
}

Result<rt::InvocationPlan> plan_row_enclosure(
    rt::InvocationContext, Ref<ActionRowEnclosureInput> input,
    rt::PlanOptions options) {
  const U64 nnz = std::max<U64>(1, action_nnz(input->action));
  const U64 r = action_metadata_bytes(input->action);
  const U64 f = sat_mul(nnz, sizeof(ExactInterval) * 2) + 64 * 1024;
  return model::ok_result(plan_of(
      "action.row_enclosure", options, nnz, r, f,
      digest({"action.row_enclosure", input->action->identity.object_id,
              std::to_string(input->row)})));
}

rt::Task<std::vector<ExactInterval>> run_row_enclosure(
    rt::InvocationContext ctx, Ref<ActionRowEnclosureInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<std::vector<ExactInterval>>(
      ctx, plan, "action.row_enclosure",
      [input](acsd::Context& science) -> Result<std::vector<ExactInterval>> {
        return input->action->row_enclosure(science, input->row);
      });
}

rt::Task<std::vector<ExactInterval>> finalize_row_enclosure(
    rt::InvocationContext ctx, Ref<ActionRowEnclosureInput>,
    Ref<rt::InvocationPlan>, Result<std::vector<ExactInterval>> partial) {
  return finalize_operation(
      ctx, std::move(partial), [](const std::vector<ExactInterval>& row) {
        for (const ExactInterval& interval : row)
          if (!(model::dyadic_to_double(interval.lower) <=
                model::dyadic_to_double(interval.upper)))
            return failure<Done>(Code::invalid_model,
                                 "a row enclosure interval is inverted",
                                 "not publishable");
        return model::ok_result(Done{});
      });
}

}  // namespace

// Public plan introspection for the independent process tests.
Result<rt::InvocationPlan> plan_operators_action_make_action(
    rt::InvocationContext ctx, Ref<MakeActionInput> input,
    rt::PlanOptions options) {
  return plan_make_action(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_apply(
    rt::InvocationContext ctx, Ref<ApplyInput> input,
    rt::PlanOptions options) {
  return plan_apply(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_transpose_apply(
    rt::InvocationContext ctx, Ref<TransposeApplyInput> input,
    rt::PlanOptions options) {
  return plan_transpose_apply(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_compose(
    rt::InvocationContext ctx, Ref<ComposeInput> input,
    rt::PlanOptions options) {
  return plan_compose(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_action_dependency_blocks(
    rt::InvocationContext ctx, Ref<ActionDependencyBlocksInput> input,
    rt::PlanOptions options) {
  return plan_dependency_blocks(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_action_serialize_recipe(
    rt::InvocationContext ctx, Ref<ActionSerializeRecipeInput> input,
    rt::PlanOptions options) {
  return plan_serialize_recipe(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_operators_action_action_row_enclosure(
    rt::InvocationContext ctx, Ref<ActionRowEnclosureInput> input,
    rt::PlanOptions options) {
  return plan_row_enclosure(ctx, std::move(input), options);
}

Result<rt::Done> register_operators_action(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for(kProcess);
  if (!ok(descriptor))
    return failure<Done>(
        descriptor.code,
        "the generated descriptor for operators.action is unavailable",
        "the process cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process =
      builder.begin(*descriptor.value);
  if (!ok(process))
    return failure<Done>(process.code,
                         "the operators.action process cannot be begun",
                         "the process cannot be registered");

  Result<Done> step = builder.add<MakeActionInput, Ref<LinearAction>>(
      *process.value,
      rt::Operation<MakeActionInput, Ref<LinearAction>>{
          "make_action", {"acsd.input.MakeActionInput", 1},
          {"acsd.LinearAction", 1}, &validate_make_action, &bounds_make_action,
          &plan_make_action, &run_make_action, &finalize_make_action});
  if (!ok(step)) return step;

  step = builder.add<ApplyInput, ArrayRef>(
      *process.value,
      rt::Operation<ApplyInput, ArrayRef>{
          "apply", {"acsd.input.ApplyInput", 1}, {"acsd.Array", 1},
          &validate_apply, &bounds_apply, &plan_apply, &run_apply,
          &finalize_apply});
  if (!ok(step)) return step;

  step = builder.add<TransposeApplyInput, ArrayRef>(
      *process.value,
      rt::Operation<TransposeApplyInput, ArrayRef>{
          "transpose_apply", {"acsd.input.TransposeApplyInput", 1},
          {"acsd.Array", 1}, &validate_transpose_apply,
          &bounds_transpose_apply, &plan_transpose_apply, &run_transpose_apply,
          &finalize_transpose_apply});
  if (!ok(step)) return step;

  step = builder.add<ComposeInput, Ref<LinearAction>>(
      *process.value,
      rt::Operation<ComposeInput, Ref<LinearAction>>{
          "compose", {"acsd.input.ComposeInput", 1}, {"acsd.LinearAction", 1},
          &validate_compose, &bounds_compose, &plan_compose, &run_compose,
          &finalize_compose});
  if (!ok(step)) return step;

  step = builder.add<ActionDependencyBlocksInput, std::vector<Id>>(
      *process.value,
      rt::Operation<ActionDependencyBlocksInput, std::vector<Id>>{
          "action.dependency_blocks",
          {"acsd.input.ActionDependencyBlocksInput", 1},
          {"acsd.vector.Id", 1}, &validate_dependency_blocks,
          &bounds_dependency_blocks, &plan_dependency_blocks,
          &run_dependency_blocks, &finalize_dependency_blocks});
  if (!ok(step)) return step;

  step = builder.add<ActionSerializeRecipeInput, ActionRecipe>(
      *process.value,
      rt::Operation<ActionSerializeRecipeInput, ActionRecipe>{
          "action.serialize_recipe",
          {"acsd.input.ActionSerializeRecipeInput", 1},
          {"acsd.ActionRecipe", 1}, &validate_serialize_recipe,
          &bounds_serialize_recipe, &plan_serialize_recipe,
          &run_serialize_recipe, &finalize_serialize_recipe});
  if (!ok(step)) return step;

  step = builder.add<ActionRowEnclosureInput, std::vector<ExactInterval>>(
      *process.value,
      rt::Operation<ActionRowEnclosureInput, std::vector<ExactInterval>>{
          "action.row_enclosure",
          {"acsd.input.ActionRowEnclosureInput", 1},
          {"acsd.vector.ExactInterval", 1}, &validate_row_enclosure,
          &bounds_row_enclosure, &plan_row_enclosure, &run_row_enclosure,
          &finalize_row_enclosure});
  return step;
}

}  // namespace acsd::modules
