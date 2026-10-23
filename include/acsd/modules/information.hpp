#pragma once
// Typed process registrations of the information module.
//
// information.reference / estimate_flux_information -> Information
// information.scene    / evaluate_scene_variance,
//                        build_supported_triangulation,
//                        reconstruct_scene_variance  -> Variance list /
//                                                       SupportedTriangulation /
//                                                       VariancePrediction
//
// The adapters call the single existing implementation (src/information) via
// the science Context bridge; the plan carries the h/Z/C design, the estimable
// space and the exact-predicate triangulation workspace as an explicit
// barrier, never as a per-leaf guess.
// Break the generated inputs.hpp <-> observation.hpp include cycle (the
// observation header uses this generated type before inputs.hpp defines it).
namespace acsd::modules {
struct RegroupObservationInput;
}

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

#include <string>

namespace acsd::modules {

Result<rt::Done> validate_information_reference(rt::InvocationContext,
                                                Ref<EstimateFluxInformationInput>);
Result<rt::PlanBounds> bounds_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput>);
Result<rt::InvocationPlan> plan_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput>, rt::PlanOptions);
rt::Task<Information> run_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput>,
    Ref<rt::InvocationPlan>);
rt::Task<Information> finalize_information_reference(
    rt::InvocationContext, Ref<EstimateFluxInformationInput>,
    Ref<rt::InvocationPlan>, Result<Information>);
Result<rt::Done> register_information_reference(rt::RegistryBuilder&);

Result<rt::Done> validate_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput>);
Result<rt::PlanBounds> bounds_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput>);
Result<rt::InvocationPlan> plan_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput>, rt::PlanOptions);
rt::Task<std::vector<Variance>> run_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput>,
    Ref<rt::InvocationPlan>);
rt::Task<std::vector<Variance>> finalize_information_scene_evaluate(
    rt::InvocationContext, Ref<EvaluateSceneVarianceInput>,
    Ref<rt::InvocationPlan>, Result<std::vector<Variance>>);

Result<rt::Done> validate_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput>);
Result<rt::PlanBounds> bounds_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput>);
Result<rt::InvocationPlan> plan_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput>,
    rt::PlanOptions);
rt::Task<Ref<SupportedTriangulation>> run_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput>,
    Ref<rt::InvocationPlan>);
rt::Task<Ref<SupportedTriangulation>> finalize_information_scene_build(
    rt::InvocationContext, Ref<BuildSupportedTriangulationInput>,
    Ref<rt::InvocationPlan>, Result<Ref<SupportedTriangulation>>);

Result<rt::Done> validate_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput>);
Result<rt::PlanBounds> bounds_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput>);
Result<rt::InvocationPlan> plan_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput>,
    rt::PlanOptions);
rt::Task<VariancePrediction> run_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput>,
    Ref<rt::InvocationPlan>);
rt::Task<VariancePrediction> finalize_information_scene_reconstruct(
    rt::InvocationContext, Ref<ReconstructSceneVarianceInput>,
    Ref<rt::InvocationPlan>, Result<VariancePrediction>);

Result<rt::Done> register_information_scene(rt::RegistryBuilder&);

}  // namespace acsd::modules
