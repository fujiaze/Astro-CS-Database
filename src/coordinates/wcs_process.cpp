// The coordinates.wcs typed process adapter (M5).
//
// Seven whitelisted operations are registered; each one calls the unique
// implementation in src/coordinates through the runtime science bridge inside
// an admitted CPU execution segment.  The adapter contains no coordinate
// formula, no interval rule and no second pool/ledger: validate reads metadata,
// plan gives the conservative resource model and run composes the one existing
// entry point.
#include <acsd/modules/coordinates.hpp>

#include <acsd/modules/generated/inputs.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/process.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

using rt::Done;

namespace {

constexpr const char* kProcess = "coordinates.wcs";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 32ull << 10;
constexpr U64 kCleanupWorkUpper = 32;
constexpr U32 kMaxWindow = 1;

// Declared conservative CPU working set of one coordinate step: the exact
// dyadic interval primitives (MPFR/GMP managed buffers), the injectivity
// certificate and the root-box iteration state.  The legacy synchronous
// implementation allocates its scratch with the standard allocator, so this is
// a *declared* plan bound, not a per-allocation ledger charge; the trial
// evidence records the conservative value explicitly.
constexpr U64 kIntervalWorkspace = 1ull << 20;
constexpr U64 kRootBoxWorkspace = 1ull << 20;

U64 saturating_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b) return std::numeric_limits<U64>::max();
  return a * b;
}

U64 saturating_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b) return std::numeric_limits<U64>::max();
  return a + b;
}

template <class T>
Result<T> adapter_failure(Code code, std::string reason, std::string impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, std::move(reason), kProcess, std::move(impact));
  return out;
}

std::string hex_id(const std::string& text) { return "sha256:" + model::sha256_hex(text); }

std::string number_text(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

std::string header_text(const HeaderFields& header) {
  std::string text;
  for (const HeaderField& field : header) {
    text += field.key;
    text += '=';
    if (std::holds_alternative<std::string>(field.value)) {
      text += std::get<std::string>(field.value);
    } else if (std::holds_alternative<double>(field.value)) {
      text += number_text(std::get<double>(field.value));
    } else {
      text += std::to_string(std::get<std::int64_t>(field.value));
    }
    text += ';';
  }
  return text;
}

std::string budget_text(const NumericBudget& budget) {
  std::string text = number_text(budget.tolerance.absolute) + "|" +
                     number_text(budget.tolerance.relative) + "|" +
                     number_text(budget.tolerance.absolute_floor) + "|" +
                     budget.tolerance.unit.scale + "|" + std::to_string(budget.max_iterations) + "|" +
                     std::to_string(budget.arithmetic.initial_bits) + "|" +
                     std::to_string(budget.arithmetic.max_bits) + "|" +
                     std::to_string(budget.arithmetic.allow_target_fallback);
  return text;
}

std::string box_text(const Box& box) {
  return number_text(box.lower[0]) + "," + number_text(box.lower[1]) + "," +
         number_text(box.upper[0]) + "," + number_text(box.upper[1]);
}

bool finite_point(const PixelCenter0& point) {
  return std::isfinite(point.x) && std::isfinite(point.y);
}
bool finite_point(const PixelEdge0& point) {
  return std::isfinite(point.x) && std::isfinite(point.y);
}
bool finite_sky(const SkyICRS& sky) {
  return std::isfinite(sky.nx) && std::isfinite(sky.ny) && std::isfinite(sky.nz);
}
bool finite_box(const Box& box) {
  return std::isfinite(box.lower[0]) && std::isfinite(box.lower[1]) &&
         std::isfinite(box.upper[0]) && std::isfinite(box.upper[1]) &&
         box.lower[0] <= box.upper[0] && box.lower[1] <= box.upper[1];
}

U64 total_work(const rt::InvocationPlan& plan) {
  U64 total = 0;
  for (const rt::RegionSpec& region : plan.regions) {
    const U64 cohorts = region.kind == rt::RegionKind::stream ? std::max<U64>(region.cohorts, 1) : 1;
    U64 region_work = 0;
    for (const rt::StageSpec& stage : region.stages)
      region_work = saturating_add(region_work, stage.work_upper);
    total = saturating_add(total, saturating_mul(region_work, cohorts));
  }
  return std::max<U64>(total, 1);
}

rt::PlanBounds scalar_bounds() {
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = 1;
  bounds.max_window = kMaxWindow;
  // The coordinate entries are whole-domain certificates; splitting the box or
  // spilling an interval certificate would change the scientific object.
  bounds.allow_spill = false;
  return bounds;
}

template <class In>
Result<In> require_input(Ref<In> input) {
  if (!input) return adapter_failure<In>(Code::invalid_request, "the typed input is null",
                                         "the operation cannot be validated");
  return model::ok_result(*input);
}

// --- plan construction -----------------------------------------------------

Result<rt::InvocationPlan> barrier_plan(rt::PlanOptions options, U64 persistent_bytes,
                                        U64 progress_bytes, U64 work_upper,
                                        const std::string& input_hash,
                                        const std::string& request_hash,
                                        const std::string& algorithm_hash) {
  if (options.block_elements != 1) {
    return adapter_failure<rt::InvocationPlan>(
        Code::invalid_request, "the block size must be 1 for a whole-domain coordinate step",
        "the coordinates.wcs plan is not applicable");
  }
  if (options.window != 1) {
    return adapter_failure<rt::InvocationPlan>(
        Code::invalid_request, "the window must be 1 for a barrier coordinate step",
        "the coordinates.wcs plan is not applicable");
  }
  if (persistent_bytes == std::numeric_limits<U64>::max() ||
      progress_bytes == std::numeric_limits<U64>::max()) {
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "the coordinates.wcs plan is not applicable");
  }
  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = 1;
  plan.block_elements = 1;
  plan.cohorts = 1;
  plan.adaptive = false;
  plan.persistent.host_bytes = saturating_add(persistent_bytes, kPlanControlBytes);
  plan.progress_floor.host_bytes = progress_bytes;
  plan.elastic = {};
  plan.control_bytes = saturating_add(kPlanControlBytes, kCohortControlBytes);
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = input_hash;
  plan.request_hash = request_hash;
  plan.algorithm_hash = algorithm_hash;

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::barrier;
  region.cohorts = 1;
  region.persistent.host_bytes = plan.persistent.host_bytes;
  region.progress_floor.host_bytes = plan.progress_floor.host_bytes;

  rt::StageSpec stage;
  stage.id = 0;
  stage.scratch.host_bytes = progress_bytes;
  stage.control_bytes = kStageControlBytes;
  stage.work_upper = work_upper;
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

// --- run bridge ------------------------------------------------------------

template <class Out>
rt::Task<Out> run_with_science(rt::InvocationContext context, Ref<rt::InvocationPlan> plan,
                               const char* semantic,
                               std::function<Result<Out>(acsd::Context&)> body) {
  if (!plan) {
    return rt::ready<Out>(context, adapter_failure<Out>(Code::invalid_request,
                                                        "no invocation plan supplied",
                                                        "the operation cannot run"));
  }
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.pinned_bytes = plan->progress_floor.pinned_bytes;
  spec.running.device_bytes = plan->progress_floor.device_bytes;
  spec.running.spill_bytes = plan->progress_floor.spill_bytes;
  spec.running.cpu_tokens = 1;
  spec.work_upper = total_work(*plan);
  spec.semantic = semantic;
  return rt::submit_cpu<Out>(
      context, spec,
      [body = std::move(body)](rt::InvocationContext task) mutable -> Result<Out> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge)) {
          return adapter_failure<Out>(bridge.code, "the science context bridge cannot be created",
                                      "the operation cannot run");
        }
        return body((*bridge.value)->context());
      });
}

template <class Out>
rt::Task<Out> passthrough_finalize(rt::InvocationContext context, Result<Out> prior) {
  if (prior.code == Code::ok) return rt::ready<Out>(context, std::move(prior));
  Result<Out> kept;
  kept.code = prior.code;
  kept.assessment = prior.assessment;
  kept.usage = prior.usage;
  kept.dependencies = prior.dependencies;
  return rt::ready<Out>(context, std::move(kept));
}

// ---------------------------------------------------------------------------
// make_wcs
// ---------------------------------------------------------------------------

Result<Done> validate_make_wcs(rt::InvocationContext, Ref<MakeWcsInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "make_wcs cannot be validated");
  if (input->header.empty())
    return adapter_failure<Done>(Code::invalid_request, "the FITS header is empty",
                                 "make_wcs cannot be validated");
  if (!finite_box(input->domain))
    return adapter_failure<Done>(Code::invalid_request,
                                 "the detector domain is not a finite ordered rectangle",
                                 "make_wcs cannot be validated");
  if (input->detector_frame.empty())
    return adapter_failure<Done>(Code::invalid_request, "the detector frame id is empty",
                                 "make_wcs cannot be validated");
  if (input->request.max_iterations == 0)
    return adapter_failure<Done>(Code::invalid_request, "the numeric budget has no iterations",
                                 "make_wcs cannot be validated");
  if (!std::isfinite(input->request.tolerance.absolute) ||
      !std::isfinite(input->request.tolerance.relative) ||
      !std::isfinite(input->request.tolerance.absolute_floor))
    return adapter_failure<Done>(Code::invalid_request, "the numeric tolerance is not finite",
                                 "make_wcs cannot be validated");
  if (input->request.arithmetic.initial_bits == 0 ||
      input->request.arithmetic.max_bits < input->request.arithmetic.initial_bits)
    return adapter_failure<Done>(Code::invalid_request, "the arithmetic bit budget is not ordered",
                                 "make_wcs cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_make_wcs(rt::InvocationContext, Ref<MakeWcsInput> input) {
  Result<Done> validation = validate_make_wcs({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the world coordinate input is invalid",
                                           "make_wcs cannot be planned");
  rt::PlanBounds bounds = scalar_bounds();
  bounds.allow_spill = true;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_make_wcs(rt::InvocationContext, Ref<MakeWcsInput> input,
                                         rt::PlanOptions options) {
  Result<Done> validation = validate_make_wcs({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code,
                                               "the world coordinate input is invalid",
                                               "make_wcs cannot be planned");
  const std::string header = header_text(input->header);
  const U64 header_bytes = saturating_add(static_cast<U64>(header.size()), 4096);
  const U64 persistent = saturating_add(header_bytes, 128ull << 10);  // model + certificate
  const U64 progress = saturating_add(kIntervalWorkspace, header_bytes);
  return barrier_plan(options, persistent, progress, 8192,
                      hex_id("header|" + header + "|" + box_text(input->domain) + "|" +
                             input->detector_frame + "|" +
                             number_text(input->astrometric_context.epoch_julian_year)),
                      hex_id("budget|" + budget_text(input->request)),
                      hex_id("coordinates.wcs|make_wcs|tan_sip_injectivity_v1|1"));
}

rt::Task<Ref<WcsModel>> run_make_wcs(rt::InvocationContext context, Ref<MakeWcsInput> input,
                                     Ref<rt::InvocationPlan> plan) {
  if (!input) {
    return rt::ready<Ref<WcsModel>>(context, adapter_failure<Ref<WcsModel>>(
                                                 Code::invalid_request, "the typed input is null",
                                                 "make_wcs cannot run"));
  }
  return run_with_science<Ref<WcsModel>>(
      context, plan, "coordinates.wcs.make_wcs",
      [input](acsd::Context& science) -> Result<Ref<WcsModel>> {
        return acsd::make_wcs(science, input->header, input->domain, input->astrometric_context,
                              input->detector_frame, input->request);
      });
}

rt::Task<Ref<WcsModel>> finalize_make_wcs(rt::InvocationContext context, Ref<MakeWcsInput>,
                                          Ref<rt::InvocationPlan>, Result<Ref<WcsModel>> prior) {
  return passthrough_finalize<Ref<WcsModel>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// forward.center0 / forward.edge0 / inverse / derivative / solid_angle_jacobian
// ---------------------------------------------------------------------------

template <class In>
Result<Done> validate_wcs_use(const Ref<In>& input, const char* what) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           std::string(what) + " cannot be validated");
  if (!input->wcs)
    return adapter_failure<Done>(Code::invalid_request, "no frame model supplied",
                                 std::string(what) + " cannot be validated");
  if (!std::isfinite(input->request.tolerance.absolute) ||
      !std::isfinite(input->request.tolerance.relative) ||
      !std::isfinite(input->request.tolerance.absolute_floor))
    return adapter_failure<Done>(Code::invalid_request, "the numeric tolerance is not finite",
                                 std::string(what) + " cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_forward_center0(rt::InvocationContext, Ref<ForwardCenter0Input> input) {
  Result<Done> base = validate_wcs_use(input, "forward.center0");
  if (!ok(base)) return base;
  if (!finite_point(input->point))
    return adapter_failure<Done>(Code::invalid_request, "the pixel centre is not finite",
                                 "forward.center0 cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_forward_edge0(rt::InvocationContext, Ref<ForwardEdge0Input> input) {
  Result<Done> base = validate_wcs_use(input, "forward.edge0");
  if (!ok(base)) return base;
  if (!finite_point(input->point))
    return adapter_failure<Done>(Code::invalid_request, "the pixel edge is not finite",
                                 "forward.edge0 cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_inverse(rt::InvocationContext, Ref<InverseInput> input) {
  Result<Done> base = validate_wcs_use(input, "inverse");
  if (!ok(base)) return base;
  if (!finite_sky(input->sky))
    return adapter_failure<Done>(Code::invalid_request, "the sky direction is not finite",
                                 "inverse cannot be validated");
  const double norm = input->sky.nx * input->sky.nx + input->sky.ny * input->sky.ny +
                      input->sky.nz * input->sky.nz;
  if (std::abs(norm - 1.0) > 1e-6)
    return adapter_failure<Done>(Code::invalid_request, "the sky direction is not a unit vector",
                                 "inverse cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_derivative(rt::InvocationContext, Ref<DerivativeInput> input) {
  Result<Done> base = validate_wcs_use(input, "derivative");
  if (!ok(base)) return base;
  if (!finite_point(input->point))
    return adapter_failure<Done>(Code::invalid_request, "the pixel centre is not finite",
                                 "derivative cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_solid_angle(rt::InvocationContext, Ref<SolidAngleJacobianInput> input) {
  Result<Done> base = validate_wcs_use(input, "solid_angle_jacobian");
  if (!ok(base)) return base;
  if (!finite_box(input->domain))
    return adapter_failure<Done>(Code::invalid_request,
                                 "the integration domain is not a finite ordered rectangle",
                                 "solid_angle_jacobian cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_scalar(rt::InvocationContext, Ref<MakeWcsInput>) {
  return model::ok_result(scalar_bounds());
}

template <class In>
Result<rt::PlanBounds> scalar_bounds_for(Ref<In> input, const char* what) {
  Result<Done> validation = validate_wcs_use(input, what);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the coordinate input is invalid",
                                           std::string(what) + " cannot be planned");
  return model::ok_result(scalar_bounds());
}

Result<rt::PlanBounds> bounds_forward_center0(rt::InvocationContext, Ref<ForwardCenter0Input> in) {
  return scalar_bounds_for(in, "forward.center0");
}
Result<rt::PlanBounds> bounds_forward_edge0(rt::InvocationContext, Ref<ForwardEdge0Input> in) {
  return scalar_bounds_for(in, "forward.edge0");
}
Result<rt::PlanBounds> bounds_inverse(rt::InvocationContext, Ref<InverseInput> in) {
  return scalar_bounds_for(in, "inverse");
}
Result<rt::PlanBounds> bounds_derivative(rt::InvocationContext, Ref<DerivativeInput> in) {
  return scalar_bounds_for(in, "derivative");
}
Result<rt::PlanBounds> bounds_solid_angle(rt::InvocationContext, Ref<SolidAngleJacobianInput> in) {
  return scalar_bounds_for(in, "solid_angle_jacobian");
}

constexpr U64 kForwardWork = 2048;       // polynomial + interval normalisation
constexpr U64 kInverseWork = 65536;      // bounded root-box iteration (provisional)
constexpr U64 kDerivativeWork = 4096;    // interval Jacobian
constexpr U64 kSolidAngleWork = 65536;   // exact integration panels (provisional)

Result<rt::InvocationPlan> plan_scalar(rt::PlanOptions options, const std::string& wcs_hash,
                                       const std::string& request_hash,
                                       const std::string& algorithm, U64 work,
                                       U64 persistent, U64 progress) {
  return barrier_plan(options, persistent, progress, work,
                      hex_id("wcs|" + wcs_hash), hex_id("budget|" + request_hash),
                      hex_id(algorithm));
}

Result<rt::InvocationPlan> plan_forward_center0(rt::InvocationContext, Ref<ForwardCenter0Input> input,
                                                rt::PlanOptions options) {
  const std::string wcs = input && input->wcs ? input->wcs->identity.content_hash : "-";
  const std::string point = input ? finite_point(input->point)
                                        ? number_text(input->point.x) + "," + number_text(input->point.y)
                                        : "invalid"
                                  : "-";
  return plan_scalar(options, wcs + "|" + point,
                     input ? budget_text(input->request) : "-",
                     "coordinates.wcs|forward_center0|tan_sip_v1|1", kForwardWork, 8ull << 10,
                     kIntervalWorkspace);
}

Result<rt::InvocationPlan> plan_forward_edge0(rt::InvocationContext, Ref<ForwardEdge0Input> input,
                                              rt::PlanOptions options) {
  const std::string wcs = input && input->wcs ? input->wcs->identity.content_hash : "-";
  const std::string point = input ? finite_point(input->point)
                                        ? number_text(input->point.x) + "," + number_text(input->point.y)
                                        : "invalid"
                                  : "-";
  return plan_scalar(options, wcs + "|edge|" + point,
                     input ? budget_text(input->request) : "-",
                     "coordinates.wcs|forward_edge0|tan_sip_v1|1", kForwardWork, 8ull << 10,
                     kIntervalWorkspace);
}

Result<rt::InvocationPlan> plan_inverse(rt::InvocationContext, Ref<InverseInput> input,
                                        rt::PlanOptions options) {
  const std::string wcs = input && input->wcs ? input->wcs->identity.content_hash : "-";
  const std::string sky = input ? number_text(input->sky.nx) + "," + number_text(input->sky.ny) +
                                      "," + number_text(input->sky.nz)
                                : "-";
  return plan_scalar(options, wcs + "|" + sky, input ? budget_text(input->request) : "-",
                     "coordinates.wcs|inverse|tan_sip_rootbox_v1|1", kInverseWork, 16ull << 10,
                     kRootBoxWorkspace);
}

Result<rt::InvocationPlan> plan_derivative(rt::InvocationContext, Ref<DerivativeInput> input,
                                           rt::PlanOptions options) {
  const std::string wcs = input && input->wcs ? input->wcs->identity.content_hash : "-";
  const std::string point = input ? number_text(input->point.x) + "," + number_text(input->point.y)
                                  : "-";
  return plan_scalar(options, wcs + "|" + point, input ? budget_text(input->request) : "-",
                     "coordinates.wcs|derivative|interval_jacobian_v1|1", kDerivativeWork,
                     16ull << 10, kIntervalWorkspace);
}

Result<rt::InvocationPlan> plan_solid_angle(rt::InvocationContext, Ref<SolidAngleJacobianInput> input,
                                            rt::PlanOptions options) {
  const std::string wcs = input && input->wcs ? input->wcs->identity.content_hash : "-";
  const std::string domain = input ? box_text(input->domain) : "-";
  return plan_scalar(options, wcs + "|" + domain, input ? budget_text(input->request) : "-",
                     "coordinates.wcs|solid_angle_jacobian|exact_panels_v1|1", kSolidAngleWork,
                     16ull << 10, kIntervalWorkspace);
}

template <class In, class Out, class Body>
rt::Task<Out> run_scalar(rt::InvocationContext context, Ref<In> input, Ref<rt::InvocationPlan> plan,
                         const char* semantic, Body body) {
  if (!input) {
    return rt::ready<Out>(context, adapter_failure<Out>(Code::invalid_request,
                                                        "the typed input is null",
                                                        "the operation cannot run"));
  }
  return run_with_science<Out>(context, plan, semantic,
                               [input, body = std::move(body)](acsd::Context& science) mutable
                                   -> Result<Out> { return body(science, *input); });
}

rt::Task<SkyICRS> run_forward_center0(rt::InvocationContext context, Ref<ForwardCenter0Input> input,
                                      Ref<rt::InvocationPlan> plan) {
  return run_scalar<ForwardCenter0Input, SkyICRS>(
      context, input, plan, "coordinates.wcs.forward.center0",
      [](acsd::Context& science, const ForwardCenter0Input& in) {
        return acsd::forward(science, in.wcs, in.point, in.request);
      });
}

rt::Task<SkyICRS> run_forward_edge0(rt::InvocationContext context, Ref<ForwardEdge0Input> input,
                                    Ref<rt::InvocationPlan> plan) {
  return run_scalar<ForwardEdge0Input, SkyICRS>(
      context, input, plan, "coordinates.wcs.forward.edge0",
      [](acsd::Context& science, const ForwardEdge0Input& in) {
        return acsd::forward(science, in.wcs, in.point, in.request);
      });
}

rt::Task<InverseResult> run_inverse(rt::InvocationContext context, Ref<InverseInput> input,
                                    Ref<rt::InvocationPlan> plan) {
  return run_scalar<InverseInput, InverseResult>(
      context, input, plan, "coordinates.wcs.inverse",
      [](acsd::Context& science, const InverseInput& in) {
        return acsd::inverse(science, in.wcs, in.sky, in.request);
      });
}

rt::Task<DerivativeResult> run_derivative(rt::InvocationContext context, Ref<DerivativeInput> input,
                                          Ref<rt::InvocationPlan> plan) {
  return run_scalar<DerivativeInput, DerivativeResult>(
      context, input, plan, "coordinates.wcs.derivative",
      [](acsd::Context& science, const DerivativeInput& in) {
        return acsd::derivative(science, in.wcs, in.point, in.request);
      });
}

rt::Task<Interval> run_solid_angle(rt::InvocationContext context, Ref<SolidAngleJacobianInput> input,
                                   Ref<rt::InvocationPlan> plan) {
  return run_scalar<SolidAngleJacobianInput, Interval>(
      context, input, plan, "coordinates.wcs.solid_angle_jacobian",
      [](acsd::Context& science, const SolidAngleJacobianInput& in) {
        return acsd::solid_angle_jacobian(science, in.wcs, in.domain, in.request);
      });
}

// ---------------------------------------------------------------------------
// bind_wcs
// ---------------------------------------------------------------------------

Result<Done> validate_bind_wcs(rt::InvocationContext, Ref<BindWcsInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "bind_wcs cannot be validated");
  if (!input->observation)
    return adapter_failure<Done>(Code::invalid_request, "no observation supplied",
                                 "bind_wcs cannot be validated");
  if (!input->wcs)
    return adapter_failure<Done>(Code::invalid_request, "no frame model supplied",
                                 "bind_wcs cannot be validated");
  if (!input->observation->native_index)
    return adapter_failure<Done>(Code::invalid_model, "the observation has no native index",
                                 "bind_wcs cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_bind_wcs(rt::InvocationContext, Ref<BindWcsInput> input) {
  Result<Done> validation = validate_bind_wcs({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the bind input is invalid",
                                           "bind_wcs cannot be planned");
  return model::ok_result(scalar_bounds());
}

Result<rt::InvocationPlan> plan_bind_wcs(rt::InvocationContext, Ref<BindWcsInput> input,
                                         rt::PlanOptions options) {
  Result<Done> validation = validate_bind_wcs({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the bind input is invalid",
                                               "bind_wcs cannot be planned");
  const std::string observation = input->observation->identity.content_hash;
  const std::string wcs = input->wcs->identity.content_hash;
  const std::string solution = input->solution ? input->solution->identity.content_hash : "-";
  return barrier_plan(options, 64ull << 10, 1ull << 20, 1024,
                      hex_id("observation|" + observation + "|wcs|" + wcs + "|solution|" + solution),
                      hex_id("request|bind_wcs"), hex_id("observation|bind_wcs|1"));
}

rt::Task<Ref<NativeObservation>> run_bind_wcs(rt::InvocationContext context, Ref<BindWcsInput> input,
                                              Ref<rt::InvocationPlan> plan) {
  if (!input) {
    return rt::ready<Ref<NativeObservation>>(context,
                                             adapter_failure<Ref<NativeObservation>>(
                                                 Code::invalid_request, "the typed input is null",
                                                 "bind_wcs cannot run"));
  }
  return run_with_science<Ref<NativeObservation>>(
      context, plan, "coordinates.wcs.bind_wcs",
      [input](acsd::Context& science) -> Result<Ref<NativeObservation>> {
        return acsd::bind_wcs(science, input->observation, input->wcs, input->solution);
      });
}

rt::Task<Ref<NativeObservation>> finalize_bind_wcs(rt::InvocationContext context,
                                                   Ref<BindWcsInput>,
                                                   Ref<rt::InvocationPlan>,
                                                   Result<Ref<NativeObservation>> prior) {
  return passthrough_finalize<Ref<NativeObservation>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------

template <class In, class Out>
Result<Done> add_operation(rt::RegistryBuilder& builder,
                           const std::shared_ptr<rt::ProcessRegistration>& process,
                           const char* operation_id, const char* input_schema,
                           const char* output_schema,
                           Result<Done> (*validate)(rt::InvocationContext, Ref<In>),
                           Result<rt::PlanBounds> (*bounds)(rt::InvocationContext, Ref<In>),
                           Result<rt::InvocationPlan> (*plan)(rt::InvocationContext, Ref<In>,
                                                              rt::PlanOptions),
                           rt::Task<Out> (*run)(rt::InvocationContext, Ref<In>,
                                                Ref<rt::InvocationPlan>),
                           rt::Task<Out> (*finalize)(rt::InvocationContext, Ref<In>,
                                                     Ref<rt::InvocationPlan>, Result<Out>)) {
  const rt::Operation<In, Out> operation{
      operation_id,
      rt::TypeId{input_schema, 1},
      rt::TypeId{output_schema, 1},
      validate,
      bounds,
      plan,
      run,
      finalize};
  return builder.add<In, Out>(process, operation);
}

}  // namespace

Result<Done> register_wcs(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor = acsd::generated::descriptor_for("coordinates.wcs");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the coordinates.wcs descriptor is unavailable",
                                 "coordinates.wcs cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the coordinates.wcs process cannot be started",
                                 "coordinates.wcs cannot be registered");

  Result<Done> step;
  step = add_operation<MakeWcsInput, Ref<WcsModel>>(
      builder, *process.value, "make_wcs", "acsd.input.MakeWcsInput", "acsd.WcsModel",
      &validate_make_wcs, &bounds_make_wcs, &plan_make_wcs, &run_make_wcs, &finalize_make_wcs);
  if (!ok(step)) return step;
  step = add_operation<ForwardCenter0Input, SkyICRS>(
      builder, *process.value, "forward.center0", "acsd.input.ForwardCenter0Input", "acsd.SkyICRS",
      &validate_forward_center0, &bounds_forward_center0, &plan_forward_center0,
      &run_forward_center0,
      [](rt::InvocationContext context, Ref<ForwardCenter0Input>, Ref<rt::InvocationPlan>,
         Result<SkyICRS> prior) { return passthrough_finalize<SkyICRS>(context, std::move(prior)); });
  if (!ok(step)) return step;
  step = add_operation<ForwardEdge0Input, SkyICRS>(
      builder, *process.value, "forward.edge0", "acsd.input.ForwardEdge0Input", "acsd.SkyICRS",
      &validate_forward_edge0, &bounds_forward_edge0, &plan_forward_edge0, &run_forward_edge0,
      [](rt::InvocationContext context, Ref<ForwardEdge0Input>, Ref<rt::InvocationPlan>,
         Result<SkyICRS> prior) { return passthrough_finalize<SkyICRS>(context, std::move(prior)); });
  if (!ok(step)) return step;
  step = add_operation<InverseInput, InverseResult>(
      builder, *process.value, "inverse", "acsd.input.InverseInput", "acsd.InverseResult",
      &validate_inverse, &bounds_inverse, &plan_inverse, &run_inverse,
      [](rt::InvocationContext context, Ref<InverseInput>, Ref<rt::InvocationPlan>,
         Result<InverseResult> prior) {
        return passthrough_finalize<InverseResult>(context, std::move(prior));
      });
  if (!ok(step)) return step;
  step = add_operation<DerivativeInput, DerivativeResult>(
      builder, *process.value, "derivative", "acsd.input.DerivativeInput", "acsd.DerivativeResult",
      &validate_derivative, &bounds_derivative, &plan_derivative, &run_derivative,
      [](rt::InvocationContext context, Ref<DerivativeInput>, Ref<rt::InvocationPlan>,
         Result<DerivativeResult> prior) {
        return passthrough_finalize<DerivativeResult>(context, std::move(prior));
      });
  if (!ok(step)) return step;
  step = add_operation<SolidAngleJacobianInput, Interval>(
      builder, *process.value, "solid_angle_jacobian", "acsd.input.SolidAngleJacobianInput",
      "acsd.Interval", &validate_solid_angle, &bounds_solid_angle, &plan_solid_angle,
      &run_solid_angle,
      [](rt::InvocationContext context, Ref<SolidAngleJacobianInput>, Ref<rt::InvocationPlan>,
         Result<Interval> prior) {
        return passthrough_finalize<Interval>(context, std::move(prior));
      });
  if (!ok(step)) return step;
  return add_operation<BindWcsInput, Ref<NativeObservation>>(
      builder, *process.value, "bind_wcs", "acsd.input.BindWcsInput", "acsd.NativeObservation",
      &validate_bind_wcs, &bounds_bind_wcs, &plan_bind_wcs, &run_bind_wcs, &finalize_bind_wcs);
}

// Plan introspection for validation/process/coordinates.wcs.
Result<rt::InvocationPlan> plan_make_wcs_op(rt::InvocationContext ctx, Ref<MakeWcsInput> input,
                                            rt::PlanOptions options) {
  return plan_make_wcs(ctx, input, options);
}
Result<rt::InvocationPlan> plan_forward_center0_op(rt::InvocationContext ctx,
                                                   Ref<ForwardCenter0Input> input,
                                                   rt::PlanOptions options) {
  return plan_forward_center0(ctx, input, options);
}
Result<rt::InvocationPlan> plan_forward_edge0_op(rt::InvocationContext ctx,
                                                 Ref<ForwardEdge0Input> input,
                                                 rt::PlanOptions options) {
  return plan_forward_edge0(ctx, input, options);
}
Result<rt::InvocationPlan> plan_inverse_op(rt::InvocationContext ctx, Ref<InverseInput> input,
                                           rt::PlanOptions options) {
  return plan_inverse(ctx, input, options);
}
Result<rt::InvocationPlan> plan_derivative_op(rt::InvocationContext ctx, Ref<DerivativeInput> input,
                                              rt::PlanOptions options) {
  return plan_derivative(ctx, input, options);
}
Result<rt::InvocationPlan> plan_solid_angle_jacobian_op(
    rt::InvocationContext ctx, Ref<SolidAngleJacobianInput> input, rt::PlanOptions options) {
  return plan_solid_angle(ctx, input, options);
}
Result<rt::InvocationPlan> plan_bind_wcs_op(rt::InvocationContext ctx, Ref<BindWcsInput> input,
                                            rt::PlanOptions options) {
  return plan_bind_wcs(ctx, input, options);
}

}  // namespace acsd::modules
