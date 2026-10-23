#pragma once
// Typed process module: uncertainty.covariance.
//
// The five operations own the one covariance implementation (shared root graph,
// factor chains, cross terms and the control transform).  Input wrappers are
// generated from include/acsd/model.hpp; this header declares the unique typed
// registration consumed by the generated assembly.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

Result<rt::Done> register_uncertainty_covariance(rt::RegistryBuilder& builder);


// Plan introspection for validation/process/*: it calls the same plan the
// Host uses and never reserves capacity or runs science.
Result<rt::InvocationPlan> plan_uncertainty_covariance_make_covariance(
    rt::InvocationContext, Ref<MakeCovarianceInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_uncertainty_covariance_covariance_apply(
    rt::InvocationContext, Ref<CovarianceApplyInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_uncertainty_covariance_quadratic_form(
    rt::InvocationContext, Ref<QuadraticFormInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_uncertainty_covariance_geometry_covariance_bound(
    rt::InvocationContext, Ref<GeometryCovarianceBoundInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_uncertainty_covariance_control_covariance(
    rt::InvocationContext, Ref<ControlCovarianceInput>, rt::PlanOptions);

}  // namespace acsd::modules
