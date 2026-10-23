#pragma once
// Typed source module functions and the sources.astrometry / sources.psf /
// sources.measure process bindings.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

// sources.astrometry: solve_astrometry.
Result<rt::Done> register_astrometry(rt::RegistryBuilder& builder);

// sources.psf: fit_psf, evaluate_psf, bind_observation_model.
Result<rt::Done> register_psf(rt::RegistryBuilder& builder);

// sources.measure: measure_science_sources.
Result<rt::Done> register_measure(rt::RegistryBuilder& builder);
// Plan introspection for the sources process drivers.
Result<rt::InvocationPlan> plan_solve_astrometry_op(rt::InvocationContext, Ref<SolveAstrometryInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_fit_psf_op(rt::InvocationContext, Ref<FitPsfInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_evaluate_psf_op(rt::InvocationContext, Ref<EvaluatePsfInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_bind_observation_model_op(rt::InvocationContext, Ref<BindObservationModelInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_measure_science_sources_op(rt::InvocationContext, Ref<MeasureScienceSourcesInput>, rt::PlanOptions);
}  // namespace acsd::modules
