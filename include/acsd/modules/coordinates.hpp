#pragma once
// Typed coordinate module functions and the coordinates.wcs process binding.
//
// The unique scientific implementation lives in src/coordinates; this header
// only declares the process registration.  The typed Input structs of the
// whitelisted operations are owned by
// <acsd/modules/generated/inputs.hpp>.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

// wcs: make_wcs, forward.center0, forward.edge0, inverse, derivative,
// solid_angle_jacobian, bind_wcs.
Result<rt::Done> register_wcs(rt::RegistryBuilder& builder);
// Plan introspection for validation/process/coordinates.wcs: the same hook the
// registry installs, exposed so the fixed driver prints the real R/F numbers.
Result<rt::InvocationPlan> plan_make_wcs_op(rt::InvocationContext, Ref<MakeWcsInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_forward_center0_op(rt::InvocationContext, Ref<ForwardCenter0Input>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_forward_edge0_op(rt::InvocationContext, Ref<ForwardEdge0Input>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_inverse_op(rt::InvocationContext, Ref<InverseInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_derivative_op(rt::InvocationContext, Ref<DerivativeInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_solid_angle_jacobian_op(rt::InvocationContext, Ref<SolidAngleJacobianInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_bind_wcs_op(rt::InvocationContext, Ref<BindWcsInput>, rt::PlanOptions);
}  // namespace acsd::modules
