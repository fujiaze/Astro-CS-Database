#pragma once
// Typed process module: geometry.overlap.
//
// The process owns the curved-boundary overlap, the provider interval methods
// and the refinable geometry revision.  All eleven operations call the single
// existing implementation declared in <acsd/contracts.hpp>; this header only
// declares the unique typed registration consumed by the generated assembly
// (tools/integrated_processes.json).  Input wrappers are generated from the
// public declaration in include/acsd/modules/generated/inputs.hpp.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

// Registers all eleven geometry.overlap operations from the generated
// descriptor.  One process is begun once; every operation uses the same
// (process_id, operation_id) typed binding.
Result<rt::Done> register_geometry_overlap(rt::RegistryBuilder& builder);

// The one geometry.drizzle operation (same geometry module, separate process).
Result<rt::Done> register_geometry_drizzle(rt::RegistryBuilder& builder);


// Plan introspection for validation/process/*: it calls the same plan the
// Host uses and never reserves capacity or runs science.
Result<rt::InvocationPlan> plan_geometry_overlap_make_footprint_provider(
    rt::InvocationContext, Ref<MakeFootprintProviderInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_detector_footprint(
    rt::InvocationContext, Ref<DetectorFootprintInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_footprint(
    rt::InvocationContext, Ref<FootprintInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_measure(
    rt::InvocationContext, Ref<MeasureInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_compute_overlap(
    rt::InvocationContext, Ref<ComputeOverlapInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_refine_geometry(
    rt::InvocationContext, Ref<RefineGeometryInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_forward_interval(
    rt::InvocationContext, Ref<FootprintForwardIntervalInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_boundary_interval(
    rt::InvocationContext, Ref<FootprintBoundaryIntervalInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_derivative_interval(
    rt::InvocationContext, Ref<FootprintDerivativeIntervalInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_refinable_open(
    rt::InvocationContext, Ref<RefinableOpenInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_geometry_overlap_refinable_refine(
    rt::InvocationContext, Ref<RefinableRefineInput>, rt::PlanOptions);


// Plan introspection for validation/process/*: it calls the same plan the
// Host uses and never reserves capacity or runs science.
Result<rt::InvocationPlan> plan_geometry_drizzle_build_drizzle(
    rt::InvocationContext, Ref<BuildDrizzleInput>, rt::PlanOptions);

}  // namespace acsd::modules
