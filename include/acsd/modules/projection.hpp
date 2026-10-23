#pragma once
// Typed process module: projection.export.
//
// The single build_export operation owns the target projection of a product's
// supported signal.  The typed registration is consumed by the generated
// assembly; input wrappers come from the generated declaration table.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

Result<rt::Done> register_projection_export(rt::RegistryBuilder& builder);


// Plan introspection for validation/process/*: it calls the same plan the
// Host uses and never reserves capacity or runs science.
Result<rt::InvocationPlan> plan_projection_export_build_export(
    rt::InvocationContext, Ref<BuildExportInput>, rt::PlanOptions);

}  // namespace acsd::modules
