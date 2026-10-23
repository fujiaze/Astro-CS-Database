#pragma once
// Typed response module functions and the response.integrate process binding.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

// response.integrate: integrate_response.
Result<rt::Done> register_integrate_response(rt::RegistryBuilder& builder);
// Plan introspection for validation/process/response.integrate.
Result<rt::InvocationPlan> plan_integrate_response_op(rt::InvocationContext, Ref<IntegrateResponseInput>, rt::PlanOptions);
}  // namespace acsd::modules
