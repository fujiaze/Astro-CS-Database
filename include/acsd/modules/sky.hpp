#pragma once
// Typed process registrations of the sky module.
//
// sky.controls / build_sky_controls -> SkyControls
// sky.fit      / fit_relative_sky   -> SkyFit
//              / apply_sky          -> SignalEstimate
//
// The global control assembly and the joint [H, A G] solve with gauge/nullspace
// are explicit barrier regions; apply_sky is a bounded stream.  The adapters
// call the single existing implementation in src/sky.
// Break the generated inputs.hpp <-> observation.hpp include cycle (the
// observation header uses this generated type before inputs.hpp defines it).
namespace acsd::modules {
struct RegroupObservationInput;
}

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

#include <string>

namespace acsd::modules {

Result<rt::Done> validate_sky_controls(rt::InvocationContext,
                                       Ref<BuildSkyControlsInput>);
Result<rt::PlanBounds> bounds_sky_controls(rt::InvocationContext,
                                           Ref<BuildSkyControlsInput>);
Result<rt::InvocationPlan> plan_sky_controls(rt::InvocationContext,
                                             Ref<BuildSkyControlsInput>,
                                             rt::PlanOptions);
rt::Task<Ref<SkyControls>> run_sky_controls(rt::InvocationContext,
                                            Ref<BuildSkyControlsInput>,
                                            Ref<rt::InvocationPlan>);
rt::Task<Ref<SkyControls>> finalize_sky_controls(
    rt::InvocationContext, Ref<BuildSkyControlsInput>, Ref<rt::InvocationPlan>,
    Result<Ref<SkyControls>>);
Result<rt::Done> register_sky_controls(rt::RegistryBuilder&);

Result<rt::Done> validate_sky_fit(rt::InvocationContext,
                                  Ref<FitRelativeSkyInput>);
Result<rt::PlanBounds> bounds_sky_fit(rt::InvocationContext,
                                      Ref<FitRelativeSkyInput>);
Result<rt::InvocationPlan> plan_sky_fit(rt::InvocationContext,
                                        Ref<FitRelativeSkyInput>,
                                        rt::PlanOptions);
rt::Task<Ref<SkyFit>> run_sky_fit(rt::InvocationContext,
                                  Ref<FitRelativeSkyInput>,
                                  Ref<rt::InvocationPlan>);
rt::Task<Ref<SkyFit>> finalize_sky_fit(rt::InvocationContext,
                                       Ref<FitRelativeSkyInput>,
                                       Ref<rt::InvocationPlan>,
                                       Result<Ref<SkyFit>>);

Result<rt::Done> validate_sky_apply(rt::InvocationContext,
                                    Ref<ApplySkyInput>);
Result<rt::PlanBounds> bounds_sky_apply(rt::InvocationContext,
                                        Ref<ApplySkyInput>);
Result<rt::InvocationPlan> plan_sky_apply(rt::InvocationContext,
                                          Ref<ApplySkyInput>,
                                          rt::PlanOptions);
rt::Task<Ref<SignalEstimate>> run_sky_apply(rt::InvocationContext,
                                            Ref<ApplySkyInput>,
                                            Ref<rt::InvocationPlan>);
rt::Task<Ref<SignalEstimate>> finalize_sky_apply(
    rt::InvocationContext, Ref<ApplySkyInput>, Ref<rt::InvocationPlan>,
    Result<Ref<SignalEstimate>>);
Result<rt::Done> register_sky_fit(rt::RegistryBuilder&);

}  // namespace acsd::modules
