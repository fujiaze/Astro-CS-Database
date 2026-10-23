#pragma once
// Typed process registrations of the coadd module.
//
// coadd.select    / select_samples -> Selection
// coadd.weights   / solve_weights  -> WeightSolution
// coadd.integrate / coadd          -> CoaddResult
//
// select/weights call the existing implementations.  integrate consumes the
// already sealed Selection/WeightSolution through the unique core
// acsd::coadd_with_inputs(); it never calls select_samples/solve_weights.  The
// compatibility coadd() entry still runs select+weights once and then delegates
// to that same core.  The evidence and the negative cases are recorded in
// validation/process/coadd.integrate/.
// Break the generated inputs.hpp <-> observation.hpp include cycle (the
// observation header uses this generated type before inputs.hpp defines it).
namespace acsd::modules {
struct RegroupObservationInput;
}

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

#include <string>

namespace acsd::modules {

Result<rt::Done> validate_coadd_select(rt::InvocationContext,
                                       Ref<SelectSamplesInput>);
Result<rt::PlanBounds> bounds_coadd_select(rt::InvocationContext,
                                           Ref<SelectSamplesInput>);
Result<rt::InvocationPlan> plan_coadd_select(rt::InvocationContext,
                                             Ref<SelectSamplesInput>,
                                             rt::PlanOptions);
rt::Task<Ref<Selection>> run_coadd_select(rt::InvocationContext,
                                          Ref<SelectSamplesInput>,
                                          Ref<rt::InvocationPlan>);
rt::Task<Ref<Selection>> finalize_coadd_select(
    rt::InvocationContext, Ref<SelectSamplesInput>, Ref<rt::InvocationPlan>,
    Result<Ref<Selection>>);
Result<rt::Done> register_coadd_select(rt::RegistryBuilder&);

Result<rt::Done> validate_coadd_weights(rt::InvocationContext,
                                        Ref<SolveWeightsInput>);
Result<rt::PlanBounds> bounds_coadd_weights(rt::InvocationContext,
                                            Ref<SolveWeightsInput>);
Result<rt::InvocationPlan> plan_coadd_weights(rt::InvocationContext,
                                              Ref<SolveWeightsInput>,
                                              rt::PlanOptions);
rt::Task<Ref<WeightSolution>> run_coadd_weights(rt::InvocationContext,
                                                Ref<SolveWeightsInput>,
                                                Ref<rt::InvocationPlan>);
rt::Task<Ref<WeightSolution>> finalize_coadd_weights(
    rt::InvocationContext, Ref<SolveWeightsInput>, Ref<rt::InvocationPlan>,
    Result<Ref<WeightSolution>>);
Result<rt::Done> register_coadd_weights(rt::RegistryBuilder&);

Result<rt::Done> validate_coadd_integrate(rt::InvocationContext,
                                          Ref<CoaddInput>);
Result<rt::PlanBounds> bounds_coadd_integrate(rt::InvocationContext,
                                              Ref<CoaddInput>);
Result<rt::InvocationPlan> plan_coadd_integrate(rt::InvocationContext,
                                                Ref<CoaddInput>,
                                                rt::PlanOptions);
rt::Task<CoaddResult> run_coadd_integrate(rt::InvocationContext,
                                          Ref<CoaddInput>,
                                          Ref<rt::InvocationPlan>);
rt::Task<CoaddResult> finalize_coadd_integrate(
    rt::InvocationContext, Ref<CoaddInput>, Ref<rt::InvocationPlan>,
    Result<CoaddResult>);
Result<rt::Done> register_coadd_integrate(rt::RegistryBuilder&);

}  // namespace acsd::modules
