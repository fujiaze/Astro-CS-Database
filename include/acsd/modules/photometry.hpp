#pragma once
// Typed process registrations of the photometry module.
//
// photometry.fit  / fit_photometry   -> Ref<PhotoSolution>
// photometry.apply/ apply_scale      -> Ref<NativeObservation>
//
// The adapters below are the only typed boundary: they call the single existing
// photometry implementation (src/photometry) through the science Context
// bridge, own the real resource plan and never re-implement a photometry
// formula.  The process documents own the scientific contract; the plan honours
// the DEVELOPMENT.md row (joint latent / local elimination / global Schur).
// Break the generated inputs.hpp <-> observation.hpp include cycle (the
// observation header uses this generated type before inputs.hpp defines it).
namespace acsd::modules {
struct RegroupObservationInput;
}

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

#include <string>

namespace acsd::modules {

// photometry.fit: one global joint flux-domain Gaussian barrier.
Result<rt::Done> validate_photometry_fit(rt::InvocationContext,
                                         Ref<FitPhotometryInput>);
Result<rt::PlanBounds> bounds_photometry_fit(rt::InvocationContext,
                                             Ref<FitPhotometryInput>);
Result<rt::InvocationPlan> plan_photometry_fit(rt::InvocationContext,
                                               Ref<FitPhotometryInput>,
                                               rt::PlanOptions);
rt::Task<Ref<PhotoSolution>> run_photometry_fit(rt::InvocationContext,
                                                Ref<FitPhotometryInput>,
                                                Ref<rt::InvocationPlan>);
rt::Task<Ref<PhotoSolution>> finalize_photometry_fit(
    rt::InvocationContext, Ref<FitPhotometryInput>, Ref<rt::InvocationPlan>,
    Result<Ref<PhotoSolution>>);
Result<rt::Done> register_photometry_fit(rt::RegistryBuilder&);

// photometry.apply: per-native-sample alpha application (stream region).
Result<rt::Done> validate_photometry_apply(rt::InvocationContext,
                                           Ref<ApplyScaleInput>);
Result<rt::PlanBounds> bounds_photometry_apply(rt::InvocationContext,
                                               Ref<ApplyScaleInput>);
Result<rt::InvocationPlan> plan_photometry_apply(rt::InvocationContext,
                                                 Ref<ApplyScaleInput>,
                                                 rt::PlanOptions);
rt::Task<Ref<NativeObservation>> run_photometry_apply(
    rt::InvocationContext, Ref<ApplyScaleInput>, Ref<rt::InvocationPlan>);
rt::Task<Ref<NativeObservation>> finalize_photometry_apply(
    rt::InvocationContext, Ref<ApplyScaleInput>, Ref<rt::InvocationPlan>,
    Result<Ref<NativeObservation>>);
Result<rt::Done> register_photometry_apply(rt::RegistryBuilder&);

}  // namespace acsd::modules
