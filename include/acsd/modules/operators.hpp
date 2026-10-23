#pragma once
// Typed process module: operators.action.
//
// The seven operations own the single shared LinearAction implementation
// (CSR/diagonal/selection/composition, apply/transpose, recipe and enclosure).
// The typed registration is consumed by the generated assembly; the Input
// wrappers are generated from include/acsd/model.hpp.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

Result<rt::Done> register_operators_action(rt::RegistryBuilder& builder);


// Plan introspection for validation/process/*: it calls the same plan the
// Host uses and never reserves capacity or runs science.
Result<rt::InvocationPlan> plan_operators_action_make_action(
    rt::InvocationContext, Ref<MakeActionInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_apply(
    rt::InvocationContext, Ref<ApplyInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_transpose_apply(
    rt::InvocationContext, Ref<TransposeApplyInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_compose(
    rt::InvocationContext, Ref<ComposeInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_action_dependency_blocks(
    rt::InvocationContext, Ref<ActionDependencyBlocksInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_action_serialize_recipe(
    rt::InvocationContext, Ref<ActionSerializeRecipeInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_operators_action_action_row_enclosure(
    rt::InvocationContext, Ref<ActionRowEnclosureInput>, rt::PlanOptions);

}  // namespace acsd::modules
