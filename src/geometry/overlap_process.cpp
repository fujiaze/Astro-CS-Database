// The geometry.overlap typed process adapter: eleven operations that call the
// one existing implementation (src/geometry) through the runtime science
// context bridge.  No overlap, containment or refinement formula is repeated
// here; the adapter only validates the request shape, gives a conservative
// resource plan (provisional W_geometry where the process document has no
// closed-form bound), admits one CPU execution segment and checks the
// delivered immutable result.
//
// The resource model is derived from the actual input metadata:
//   * ring vertex counts of the detector footprint,
//   * WCS/SIP/header metadata of the provider model,
//   * the request GeometryBudget (max_work/max_bytes, 0 means module default),
//   * functionals stored in a GeometrySnapshot.
// Every number is a conservative upper bound for one bounded source; the
// adaptive candidate expansion is allowed to spill (SCHEDULER replan order) but
// the shared R/F vectors are never scaled by a ratio.
#include <acsd/modules/geometry.hpp>
#include <acsd/modules/generated/status.hpp>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "coordinates/healpix.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

namespace {

using rt::Done;
constexpr const char* kProcess = "geometry.overlap";

template <class T>
Result<T> failure(Code code, std::string reason, std::string impact) {
  return model::fail<T>(code, std::move(reason), kProcess, std::move(impact));
}

U64 sat_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b)
    return std::numeric_limits<U64>::max();
  return a * b;
}

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) total = sat_mul(total, extent);
  return total;
}

U64 array_bytes(const ArrayRef& array) {
  if (!array) return 0;
  const std::size_t item = model::dtype_size(array->dtype);
  return sat_mul(element_count(array->shape), static_cast<U64>(item));
}

U64 wcs_metadata_bytes(const Ref<WcsModel>& wcs) {
  if (!wcs) return 0;
  U64 bytes = 2048;
  bytes += sat_mul(static_cast<U64>(wcs->sip_a.terms.size()), sizeof(Term2));
  bytes += sat_mul(static_cast<U64>(wcs->sip_b.terms.size()), sizeof(Term2));
  bytes += sat_mul(static_cast<U64>(wcs->original_header.size()), 96);
  return bytes;
}

U64 footprint_vertices(const Ref<DetectorFootprint>& source) {
  if (!source) return 0;
  U64 vertices = 0;
  for (const Ring& ring : source->detector_rings)
    vertices += ring.vertices.size();
  return vertices;
}

U64 footprint_set_bytes(const Ref<FootprintSet>& set) {
  if (!set) return 0;
  U64 bytes = 2048;
  if (set->index) bytes += sat_mul(set->index->size, 64);
  if (set->exceptions) {
    bytes += array_bytes(set->exceptions->sample_rows);
    bytes += array_bytes(set->exceptions->ring_offsets);
    bytes += array_bytes(set->exceptions->vertex_offsets);
    bytes += array_bytes(set->exceptions->vertices);
    bytes += array_bytes(set->exceptions->hole_flags);
  }
  for (const Ref<DetectorFootprint>& footprint : set->inline_footprints) {
    const U64 vertices = footprint_vertices(footprint);
    bytes += sat_mul(vertices, sizeof(PixelEdge0) + 32);
  }
  bytes += array_bytes(set->sample_response_gain);
  return bytes;
}

U64 budget_functional_bytes(const GeometryBudget& budget) {
  U64 bytes = sat_mul(static_cast<U64>(budget.requested_functionals.size()),
                      sizeof(QuantityRequest));
  for (const QuantityRequest& request : budget.requested_functionals) {
    bytes += array_bytes(request.left);
    bytes += array_bytes(request.right);
  }
  return bytes;
}

U64 snapshot_bytes(const Ref<GeometrySnapshot>& snapshot) {
  if (!snapshot) return 0;
  U64 bytes = 4096;
  bytes += sat_mul(static_cast<U64>(snapshot->overlaps.size()), 4096);
  bytes += sat_mul(static_cast<U64>(snapshot->operators.size()), 2048);
  bytes += sat_mul(static_cast<U64>(snapshot->functionals.size()),
                   sizeof(QuantityRequest));
  for (const QuantityRequest& request : snapshot->functionals) {
    bytes += array_bytes(request.left);
    bytes += array_bytes(request.right);
  }
  return bytes;
}

U64 budget_work(const GeometryBudget& budget, U64 derived) {
  return budget.max_work > 0 ? budget.max_work : derived;
}

U64 candidate_bound(const GeometryBudget& budget) {
  // The process document caps the adaptive candidate expansion at 4096
  // candidate cells/nodes; a larger budget is still bounded by that cap.
  const U64 from_budget = budget.max_work > 0 ? budget.max_work : 4096;
  return std::clamp<U64>(from_budget, 1, 4096);
}

Id digest(std::initializer_list<std::string> parts) {
  std::string text;
  for (const std::string& part : parts) {
    text += part;
    text.push_back('|');
  }
  return "sha256:" + model::sha256_hex(text);
}

rt::PlanBounds bounds_of(U64 max_block, U64 work, bool spill) {
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::max<U64>(1, max_block);
  bounds.max_window = 1;
  bounds.allow_spill = spill;
  (void)work;
  return bounds;
}

rt::InvocationPlan plan_of(const char* operation, const rt::PlanOptions& options,
                           U64 work, U64 persistent, U64 progress,
                           const Id& input_hash) {
  rt::InvocationPlan plan;
  plan.input_hash = input_hash;
  plan.request_hash = digest({Id(operation), input_hash});
  plan.epoch = 0;
  plan.options = options;
  plan.elements = std::max<U64>(1, work);
  plan.block_elements =
      options.block_elements > 0 ? options.block_elements : plan.elements;
  plan.block_elements = std::min(plan.block_elements, plan.elements);
  plan.cohorts = (plan.elements + plan.block_elements - 1) / plan.block_elements;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
  plan.control_bytes = 1024;
  plan.cleanup_bytes = 0;
  plan.cleanup_work_upper = 1;
  plan.adaptive = false;
  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = std::max<U64>(1, plan.cohorts);
  region.persistent = plan.persistent;
  region.progress_floor = plan.progress_floor;
  rt::StageSpec stage;
  stage.id = 0;
  stage.work_upper = plan.elements;
  stage.control_bytes = plan.control_bytes;
  region.stages.push_back(std::move(stage));
  plan.regions.push_back(std::move(region));
  return plan;
}

template <class In>
Result<Done> validate_common(rt::InvocationContext ctx, const Ref<In>& input,
                             const char* operation) {
  if (!input)
    return failure<Done>(Code::invalid_request, "the typed input is null",
                         "the operation cannot run");
  if (!ctx.ids.process.empty() && ctx.ids.process != kProcess)
    return failure<Done>(Code::invalid_request,
                         "the invocation process id does not match " +
                             std::string(kProcess),
                         "the operation cannot run");
  if (!ctx.ids.operation.empty() && ctx.ids.operation != operation)
    return failure<Done>(Code::invalid_request,
                         "the invocation operation id does not match " +
                             std::string(operation),
                         "the operation cannot run");
  return model::ok_result(Done{});
}

template <class In, class Extra>
Result<Done> validate_with(rt::InvocationContext ctx, const Ref<In>& input,
                           const char* operation, Extra extra) {
  const Result<Done> common = validate_common(ctx, input, operation);
  if (!ok(common)) return common;
  return extra(*input);
}

Result<Done> check_budget(const GeometryBudget& budget) {
  if (budget.arithmetic.max_bits < 8 || budget.arithmetic.max_bits > 65536)
    return failure<Done>(Code::invalid_request,
                         "arithmetic.max_bits must be within 8..65536",
                         "the exact enclosure cannot be produced");
  if (budget.arithmetic.initial_bits > budget.arithmetic.max_bits)
    return failure<Done>(Code::invalid_request,
                         "arithmetic.initial_bits exceeds max_bits",
                         "the exact enclosure cannot be produced");
  for (const LeafAreaLimit& limit : budget.per_leaf_absolute_area) {
    if (!std::isfinite(limit.absolute_sr) || limit.absolute_sr < 0.0)
      return failure<Done>(Code::invalid_request,
                           "per-leaf absolute area must be finite and non-negative",
                           "the support Boolean cannot be evaluated");
  }
  if (!std::isfinite(budget.area_absolute_sr) ||
      budget.area_absolute_sr < 0.0)
    return failure<Done>(Code::invalid_request,
                         "area_absolute_sr must be finite and non-negative",
                         "the support Boolean cannot be evaluated");
  return model::ok_result(Done{});
}

Result<Done> check_box(const Box& box) {
  if (!std::isfinite(box.lower[0]) || !std::isfinite(box.lower[1]) ||
      !std::isfinite(box.upper[0]) || !std::isfinite(box.upper[1]))
    return failure<Done>(Code::invalid_request,
                         "the detector box has non-finite endpoints",
                         "the enclosure cannot be computed");
  if (!(box.lower[0] < box.upper[0]) || !(box.lower[1] < box.upper[1]))
    return failure<Done>(Code::invalid_request,
                         "the detector box is empty or inverted",
                         "the enclosure cannot be computed");
  return model::ok_result(Done{});
}

Result<Done> check_pixfrac(double pixfrac) {
  if (!(pixfrac > 0.0) || pixfrac > 1.0)
    return failure<Done>(Code::invalid_request,
                         "pixfrac must be within (0,1]",
                         "the footprint cannot be contracted");
  return model::ok_result(Done{});
}

template <class Out, class Body>
rt::Task<Out> run_operation(rt::InvocationContext ctx,
                            Ref<rt::InvocationPlan> plan, const char* operation,
                            Body body) {
  if (!plan)
    return rt::ready<Out>(ctx, failure<Out>(Code::invalid_request,
                                            "no invocation plan supplied",
                                            "the operation cannot run"));
  U64 work = plan->elements;
  if (!plan->regions.empty() && !plan->regions.front().stages.empty())
    work = plan->regions.front().stages.front().work_upper;
  work = std::max<U64>(1, work);
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.cpu_tokens = 1;
  spec.work_upper = work;
  spec.semantic = Id(operation);
  return rt::submit_cpu<Out>(
      ctx, spec, [body = std::move(body)](rt::InvocationContext task) mutable
                     -> Result<Out> {
        Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
            rt::detail::make_science_context(task, task.progress);
        if (!ok(bridge) || !bridge.value)
          return failure<Out>(bridge.code == Code::ok ? Code::internal
                                                      : bridge.code,
                              "the runtime science-context bridge cannot be "
                              "created",
                              "the operation cannot run");
        return body((*bridge.value)->context());
      });
}

template <class Out, class Check>
rt::Task<Out> finalize_operation(rt::InvocationContext ctx,
                                 const char* operation, Result<Out> partial,
                                 Check check) {
  if (partial.code != Code::ok || !partial.value)
    return rt::ready<Out>(ctx, std::move(partial));
  const Result<Done> checked = check(*partial.value);
  if (!ok(checked)) {
    Result<Out> failed = failure<Out>(
        checked.code == Code::ok ? Code::internal : checked.code,
        "the delivered " + std::string(operation) +
            " result violates its immutable output contract",
        "the result is not published");
    failed.assessment.issues.insert(failed.assessment.issues.end(),
                                    partial.assessment.issues.begin(),
                                    partial.assessment.issues.end());
    return rt::ready<Out>(ctx, std::move(failed));
  }
  return rt::ready<Out>(ctx, std::move(partial));
}

U64 wcs_persistent(const Ref<WcsModel>& wcs) {
  return std::max<U64>(1024, wcs_metadata_bytes(wcs));
}

U64 source_persistent(const Ref<DetectorFootprint>& source) {
  U64 bytes = 1024;
  bytes += sat_mul(footprint_vertices(source), sizeof(PixelEdge0) + 32);
  return bytes;
}

// ---------------------------------------------------------------------------
// make_footprint_provider
// ---------------------------------------------------------------------------

Result<Done> validate_make_footprint_provider(
    rt::InvocationContext ctx, Ref<MakeFootprintProviderInput> input) {
  return validate_with(ctx, input, "make_footprint_provider",
                       [](const MakeFootprintProviderInput& value) {
                         if (!value.wcs)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'wcs' is a required object reference",
                               "the footprint provider cannot be built");
                         return model::ok_result(Done{});
                       });
}

Result<rt::PlanBounds> bounds_make_footprint_provider(
    rt::InvocationContext, Ref<MakeFootprintProviderInput>) {
  return model::ok_result(bounds_of(64, 8, false));
}

Result<rt::InvocationPlan> plan_make_footprint_provider(
    rt::InvocationContext, Ref<MakeFootprintProviderInput> input,
    rt::PlanOptions options) {
  const U64 work = 8;
  const U64 r = std::max<U64>(1024, wcs_metadata_bytes(input->wcs));
  const U64 f = 64 * 1024;
  return model::ok_result(plan_of(
      "make_footprint_provider", options, work, r, f,
      digest({"make_footprint_provider",
              input->wcs ? input->wcs->identity.object_id : Id{}})));
}

rt::Task<Ref<FootprintProvider>> run_make_footprint_provider(
    rt::InvocationContext ctx, Ref<MakeFootprintProviderInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<FootprintProvider>>(
      ctx, plan, "make_footprint_provider",
      [input](acsd::Context& science) -> Result<Ref<FootprintProvider>> {
        return make_footprint_provider(science, input->wcs);
      });
}

rt::Task<Ref<FootprintProvider>> finalize_make_footprint_provider(
    rt::InvocationContext ctx, Ref<MakeFootprintProviderInput>,
    Ref<rt::InvocationPlan>, Result<Ref<FootprintProvider>> partial) {
  return finalize_operation(
      ctx, "make_footprint_provider", std::move(partial),
      [](const Ref<FootprintProvider>& provider) -> Result<Done> {
        if (!provider)
          return failure<Done>(Code::invalid_model,
                               "the provider is null", "not publishable");
        if (!provider->model())
          return failure<Done>(Code::invalid_model,
                               "the provider has no WCS model",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// detector_footprint
// ---------------------------------------------------------------------------

Result<Done> validate_detector_footprint(rt::InvocationContext ctx,
                                         Ref<DetectorFootprintInput> input) {
  return validate_with(ctx, input, "detector_footprint",
                       [](const DetectorFootprintInput& value) {
                         if (!value.footprints)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'footprints' is a required object reference",
                               "the detector footprint cannot be built");
                         return model::ok_result(Done{});
                       });
}

Result<rt::PlanBounds> bounds_detector_footprint(
    rt::InvocationContext, Ref<DetectorFootprintInput> input) {
  const U64 vertices =
      input && input->footprints && input->footprints->exceptions &&
              input->footprints->exceptions->vertices
          ? std::max<U64>(
                1, element_count(input->footprints->exceptions->vertices->shape))
          : 16;
  return model::ok_result(bounds_of(vertices, vertices, false));
}

Result<rt::InvocationPlan> plan_detector_footprint(
    rt::InvocationContext, Ref<DetectorFootprintInput> input,
    rt::PlanOptions options) {
  const U64 vertices =
      input->footprints && input->footprints->exceptions &&
              input->footprints->exceptions->vertices
          ? std::max<U64>(
                1, element_count(input->footprints->exceptions->vertices->shape))
          : 16;
  const U64 work = vertices;
  const U64 r = footprint_set_bytes(input->footprints);
  const U64 f = sat_mul(vertices, 128) + 64 * 1024;
  return model::ok_result(plan_of(
      "detector_footprint", options, work, r, f,
      digest({"detector_footprint", std::to_string(input->source_row),
              input->footprints ? input->footprints->identity.object_id : Id{}})));
}

rt::Task<Ref<DetectorFootprint>> run_detector_footprint(
    rt::InvocationContext ctx, Ref<DetectorFootprintInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<DetectorFootprint>>(
      ctx, plan, "detector_footprint",
      [input](acsd::Context& science) -> Result<Ref<DetectorFootprint>> {
        return detector_footprint(science, input->footprints, input->source_row);
      });
}

rt::Task<Ref<DetectorFootprint>> finalize_detector_footprint(
    rt::InvocationContext ctx, Ref<DetectorFootprintInput>,
    Ref<rt::InvocationPlan>, Result<Ref<DetectorFootprint>> partial) {
  return finalize_operation(
      ctx, "detector_footprint", std::move(partial),
      [](const Ref<DetectorFootprint>& footprint) -> Result<Done> {
        if (!footprint)
          return failure<Done>(Code::invalid_model,
                               "the detector footprint is null",
                               "not publishable");
        if (footprint->detector_rings.empty())
          return failure<Done>(Code::invalid_model,
                               "the detector footprint has no ring",
                               "not publishable");
        for (const Ring& ring : footprint->detector_rings)
          if (ring.vertices.size() < 2)
            return failure<Done>(Code::invalid_model,
                                 "a detector ring has fewer than two vertices",
                                 "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// footprint
// ---------------------------------------------------------------------------

Result<Done> validate_footprint(rt::InvocationContext ctx,
                                Ref<FootprintInput> input) {
  return validate_with(ctx, input, "footprint",
                       [](const FootprintInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the curved boundary cannot be built");
                         if (!value.source)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'source' is a required object reference",
                               "the curved boundary cannot be built");
                         Result<Done> pixfrac = check_pixfrac(value.pixfrac);
                         if (!ok(pixfrac)) return pixfrac;
                         return check_budget(value.request);
                       });
}

Result<rt::PlanBounds> bounds_footprint(rt::InvocationContext,
                                        Ref<FootprintInput> input) {
  const U64 vertices = std::max<U64>(1, footprint_vertices(input->source));
  return model::ok_result(bounds_of(vertices, vertices, false));
}

Result<rt::InvocationPlan> plan_footprint(rt::InvocationContext,
                                          Ref<FootprintInput> input,
                                          rt::PlanOptions options) {
  const U64 vertices = std::max<U64>(1, footprint_vertices(input->source));
  const U64 work = std::max<U64>(vertices, 1);
  const U64 r = wcs_persistent(input->provider->model()) +
                source_persistent(input->source);
  const U64 f = sat_mul(vertices, 192) + 64 * 1024;
  return model::ok_result(plan_of(
      "footprint", options, work, r, f,
      digest({"footprint", input->source->identity.object_id,
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{},
              std::to_string(input->pixfrac)})));
}

rt::Task<Ref<CurvedBoundary>> run_footprint(
    rt::InvocationContext ctx, Ref<FootprintInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<CurvedBoundary>>(
      ctx, plan, "footprint",
      [input](acsd::Context& science) -> Result<Ref<CurvedBoundary>> {
        return footprint(science, *input->provider, input->source, input->pixfrac,
                         input->request);
      });
}

rt::Task<Ref<CurvedBoundary>> finalize_footprint(
    rt::InvocationContext ctx, Ref<FootprintInput>, Ref<rt::InvocationPlan>,
    Result<Ref<CurvedBoundary>> partial) {
  return finalize_operation(
      ctx, "footprint", std::move(partial),
      [](const Ref<CurvedBoundary>& boundary) -> Result<Done> {
        if (!boundary)
          return failure<Done>(Code::invalid_model,
                               "the curved boundary is null", "not publishable");
        if (boundary->rings.empty() || boundary->rings.front().empty())
          return failure<Done>(Code::invalid_model,
                               "the curved boundary has no edge",
                               "not publishable");
        for (const auto& ring : boundary->rings)
          for (const CurvedEdge& edge : ring) {
            if (edge.bernstein.size() < 2)
              return failure<Done>(Code::invalid_model,
                                   "a curved edge has fewer than two Bernstein "
                                   "control intervals",
                                   "not publishable");
            if (!(edge.parameter.lower <= edge.parameter.upper))
              return failure<Done>(Code::invalid_model,
                                   "a curved edge has an empty parameter domain",
                                   "not publishable");
            for (const auto& control : edge.bernstein)
              if (!(model::dyadic_to_double(control[0].lower) <=
                    model::dyadic_to_double(control[0].upper)) ||
                  !(model::dyadic_to_double(control[1].lower) <=
                    model::dyadic_to_double(control[1].upper)))
                return failure<Done>(
                    Code::invalid_model,
                    "a Bernstein control interval is inverted",
                    "not publishable");
          }
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// measure
// ---------------------------------------------------------------------------

Result<Done> validate_measure(rt::InvocationContext ctx,
                              Ref<MeasureInput> input) {
  return validate_with(ctx, input, "measure",
                       [](const MeasureInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the native area cannot be measured");
                         if (!value.source)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'source' is a required object reference",
                               "the native area cannot be measured");
                         return check_budget(value.request);
                       });
}

Result<rt::PlanBounds> bounds_measure(rt::InvocationContext,
                                      Ref<MeasureInput> input) {
  const U64 vertices = std::max<U64>(1, footprint_vertices(input->source));
  return model::ok_result(bounds_of(vertices, vertices, false));
}

Result<rt::InvocationPlan> plan_measure(rt::InvocationContext,
                                        Ref<MeasureInput> input,
                                        rt::PlanOptions options) {
  const U64 vertices = std::max<U64>(1, footprint_vertices(input->source));
  const U64 work = vertices;
  const U64 r = wcs_persistent(input->provider->model()) +
                source_persistent(input->source);
  const U64 f = sat_mul(vertices, 128) + 64 * 1024;
  return model::ok_result(plan_of(
      "measure", options, work, r, f,
      digest({"measure", input->source->identity.object_id,
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{}})));
}

rt::Task<SphericalAreaInterval> run_measure(
    rt::InvocationContext ctx, Ref<MeasureInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<SphericalAreaInterval>(
      ctx, plan, "measure",
      [input](acsd::Context& science) -> Result<SphericalAreaInterval> {
        return measure(science, *input->provider, input->source, input->request);
      });
}

rt::Task<SphericalAreaInterval> finalize_measure(
    rt::InvocationContext ctx, Ref<MeasureInput>, Ref<rt::InvocationPlan>,
    Result<SphericalAreaInterval> partial) {
  return finalize_operation(
      ctx, "measure", std::move(partial),
      [](const SphericalAreaInterval& area) -> Result<Done> {
        if (!std::isfinite(area.lower_sr) || !std::isfinite(area.upper_sr))
          return failure<Done>(Code::invalid_model,
                               "the spherical area has non-finite endpoints",
                               "not publishable");
        if (!(area.lower_sr <= area.upper_sr) || area.lower_sr < 0.0)
          return failure<Done>(Code::invalid_model,
                               "the spherical area interval is inverted or "
                               "negative",
                               "not publishable");
        if (!(model::dyadic_to_double(area.exact.lower) <=
              model::dyadic_to_double(area.exact.upper)))
          return failure<Done>(Code::invalid_model,
                               "the exact dyadic area interval is inverted",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// compute_overlap
// ---------------------------------------------------------------------------

Result<Done> validate_compute_overlap(rt::InvocationContext ctx,
                                      Ref<ComputeOverlapInput> input) {
  return validate_with(ctx, input, "compute_overlap",
                       [](const ComputeOverlapInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the overlap cannot be computed");
                         if (!value.source)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'source' is a required object reference",
                               "the overlap cannot be computed");
                         if (value.order > coordinates::kMaxOrder)
                           return failure<Done>(
                               Code::invalid_request,
                               "order exceeds the supported HEALPix order",
                               "the overlap cannot be computed");
                         Result<Done> pixfrac = check_pixfrac(value.pixfrac);
                         if (!ok(pixfrac)) return pixfrac;
                         return check_budget(value.request);
                       });
}

Result<rt::PlanBounds> bounds_compute_overlap(rt::InvocationContext,
                                              Ref<ComputeOverlapInput> input) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  return model::ok_result(bounds_of(candidates, candidates, true));
}

Result<rt::InvocationPlan> plan_compute_overlap(
    rt::InvocationContext, Ref<ComputeOverlapInput> input,
    rt::PlanOptions options) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  const U64 vertices = std::max<U64>(1, footprint_vertices(input->source));
  const U64 work = budget_work(input->request, candidates);
  // R: the recipe, shared ledger, primitive table and the immutable output
  // contributions (a candidate may touch at most `vertices` primitive cells).
  const U64 r = wcs_persistent(input->provider->model()) +
                source_persistent(input->source) +
                sat_mul(candidates, 512) +
                budget_functional_bytes(input->request);
  // F: the adaptive candidate/vertex/sort workspace of one bounded cohort.
  const U64 f = sat_mul(candidates, sat_mul(vertices, 64) + 512) + 64 * 1024;
  return model::ok_result(plan_of(
      "compute_overlap", options, work, r, f,
      digest({"compute_overlap", input->source->identity.object_id,
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{},
              std::to_string(input->order), std::to_string(input->pixfrac)})));
}

rt::Task<Ref<Overlaps>> run_compute_overlap(
    rt::InvocationContext ctx, Ref<ComputeOverlapInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<Overlaps>>(
      ctx, plan, "compute_overlap",
      [input](acsd::Context& science) -> Result<Ref<Overlaps>> {
        return compute_overlap(science, *input->provider, input->source,
                               input->order, input->pixfrac, input->request);
      });
}

rt::Task<Ref<Overlaps>> finalize_compute_overlap(
    rt::InvocationContext ctx, Ref<ComputeOverlapInput>,
    Ref<rt::InvocationPlan>, Result<Ref<Overlaps>> partial) {
  return finalize_operation(
      ctx, "compute_overlap", std::move(partial),
      [](const Ref<Overlaps>& overlaps) -> Result<Done> {
        if (!overlaps)
          return failure<Done>(Code::invalid_model, "the overlap is null",
                               "not publishable");
        if (!overlaps->recipe)
          return failure<Done>(Code::invalid_model,
                               "the overlap carries no geometry recipe",
                               "not publishable");
        for (const OverlapContribution& contribution :
             overlaps->contributions) {
          const SphericalAreaInterval& area = contribution.overlap_area;
          if (!std::isfinite(area.lower_sr) || !std::isfinite(area.upper_sr) ||
              !(area.lower_sr <= area.upper_sr))
            return failure<Done>(Code::invalid_model,
                                 "a contribution has an inverted area interval",
                                 "not publishable");
        }
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// refine_geometry / refinable.refine
// ---------------------------------------------------------------------------

Result<Done> validate_refine_geometry(rt::InvocationContext ctx,
                                      Ref<RefineGeometryInput> input) {
  return validate_with(ctx, input, "refine_geometry",
                       [](const RefineGeometryInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the geometry revision cannot be built");
                         if (!value.base)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'base' is a required object reference",
                               "the geometry revision cannot be built");
                         return check_budget(value.request);
                       });
}

Result<rt::PlanBounds> bounds_refine_geometry(rt::InvocationContext,
                                              Ref<RefineGeometryInput> input) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  return model::ok_result(bounds_of(candidates, candidates, true));
}

Result<rt::InvocationPlan> plan_refine_geometry(
    rt::InvocationContext, Ref<RefineGeometryInput> input,
    rt::PlanOptions options) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  const U64 work = budget_work(input->request, candidates);
  const U64 r = snapshot_bytes(input->base) + 4096 +
                budget_functional_bytes(input->request);
  const U64 f = sat_mul(candidates, 512) + 64 * 1024;
  return model::ok_result(plan_of(
      "refine_geometry", options, work, r, f,
      digest({"refine_geometry",
              input->base->identity.object_id})));
}

rt::Task<Ref<GeometryRevision>> run_refine_geometry(
    rt::InvocationContext ctx, Ref<RefineGeometryInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<GeometryRevision>>(
      ctx, plan, "refine_geometry",
      [input](acsd::Context& science) -> Result<Ref<GeometryRevision>> {
        return refine_geometry(science, *input->provider, input->base,
                               input->request);
      });
}

Result<Done> check_revision(const Ref<GeometryRevision>& revision) {
  if (!revision)
    return failure<Done>(Code::invalid_model, "the geometry revision is null",
                         "not publishable");
  if (!revision->base || !revision->next)
    return failure<Done>(Code::invalid_model,
                         "the geometry revision lacks its base or next snapshot",
                         "not publishable");
  return model::ok_result(Done{});
}

rt::Task<Ref<GeometryRevision>> finalize_refine_geometry(
    rt::InvocationContext ctx, Ref<RefineGeometryInput>, Ref<rt::InvocationPlan>,
    Result<Ref<GeometryRevision>> partial) {
  return finalize_operation(ctx, "refine_geometry", std::move(partial),
                            [](const Ref<GeometryRevision>& revision) {
                              return check_revision(revision);
                            });
}

// ---------------------------------------------------------------------------
// provider interval methods
// ---------------------------------------------------------------------------

Result<Done> validate_forward_interval(
    rt::InvocationContext ctx, Ref<FootprintForwardIntervalInput> input) {
  return validate_with(ctx, input, "footprint.forward_interval",
                       [](const FootprintForwardIntervalInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the sky enclosure cannot be computed");
                         return check_box(value.box);
                       });
}

Result<rt::PlanBounds> bounds_forward_interval(
    rt::InvocationContext, Ref<FootprintForwardIntervalInput>) {
  return model::ok_result(bounds_of(1, 1, false));
}

Result<rt::InvocationPlan> plan_forward_interval(
    rt::InvocationContext, Ref<FootprintForwardIntervalInput> input,
    rt::PlanOptions options) {
  const U64 r = wcs_persistent(input->provider->model());
  return model::ok_result(plan_of(
      "footprint.forward_interval", options, 1, r, 64 * 1024,
      digest({"footprint.forward_interval",
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{},
              std::to_string(input->box.lower[0]),
              std::to_string(input->box.upper[0])})));
}

rt::Task<EnclosedSkyRegion> run_forward_interval(
    rt::InvocationContext ctx, Ref<FootprintForwardIntervalInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<EnclosedSkyRegion>(
      ctx, plan, "footprint.forward_interval",
      [input](acsd::Context& science) -> Result<EnclosedSkyRegion> {
        return input->provider->forward_interval(science, input->box);
      });
}

Result<Done> check_region(const EnclosedSkyRegion& region) {
  for (const Interval& component : region.xyz) {
    if (!std::isfinite(component.lower) || !std::isfinite(component.upper))
      return failure<Done>(Code::invalid_model,
                           "the enclosed sky region has a non-finite component",
                           "not publishable");
    if (!(component.lower <= component.upper))
      return failure<Done>(Code::invalid_model,
                           "the enclosed sky region interval is inverted",
                           "not publishable");
  }
  return model::ok_result(Done{});
}

rt::Task<EnclosedSkyRegion> finalize_forward_interval(
    rt::InvocationContext ctx, Ref<FootprintForwardIntervalInput>,
    Ref<rt::InvocationPlan>, Result<EnclosedSkyRegion> partial) {
  return finalize_operation(ctx, "footprint.forward_interval",
                            std::move(partial),
                            [](const EnclosedSkyRegion& region) {
                              return check_region(region);
                            });
}

Result<Done> validate_boundary_interval(
    rt::InvocationContext ctx, Ref<FootprintBoundaryIntervalInput> input) {
  return validate_with(
      ctx, input, "footprint.boundary_interval",
      [](const FootprintBoundaryIntervalInput& value) {
        if (!value.provider)
          return failure<Done>(
              Code::invalid_request,
              "field 'provider' is a required object reference",
              "the boundary enclosure cannot be computed");
        if (value.edge.bernstein.empty())
          return failure<Done>(Code::invalid_request,
                               "the curved edge has no Bernstein control point",
                               "the boundary enclosure cannot be computed");
        if (!std::isfinite(value.parameter.lower) ||
            !std::isfinite(value.parameter.upper) ||
            !(value.parameter.lower <= value.parameter.upper))
          return failure<Done>(Code::invalid_request,
                               "the parameter interval is invalid",
                               "the boundary enclosure cannot be computed");
        return model::ok_result(Done{});
      });
}

Result<rt::PlanBounds> bounds_boundary_interval(
    rt::InvocationContext, Ref<FootprintBoundaryIntervalInput>) {
  return model::ok_result(bounds_of(1, 1, false));
}

Result<rt::InvocationPlan> plan_boundary_interval(
    rt::InvocationContext, Ref<FootprintBoundaryIntervalInput> input,
    rt::PlanOptions options) {
  const U64 r = wcs_persistent(input->provider->model());
  return model::ok_result(plan_of(
      "footprint.boundary_interval", options, 1, r, 64 * 1024,
      digest({"footprint.boundary_interval",
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{},
              std::to_string(input->parameter.lower),
              std::to_string(input->parameter.upper)})));
}

rt::Task<EnclosedSkyRegion> run_boundary_interval(
    rt::InvocationContext ctx, Ref<FootprintBoundaryIntervalInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<EnclosedSkyRegion>(
      ctx, plan, "footprint.boundary_interval",
      [input](acsd::Context& science) -> Result<EnclosedSkyRegion> {
        return input->provider->boundary_interval(science, input->edge,
                                                  input->parameter);
      });
}

rt::Task<EnclosedSkyRegion> finalize_boundary_interval(
    rt::InvocationContext ctx, Ref<FootprintBoundaryIntervalInput>,
    Ref<rt::InvocationPlan>, Result<EnclosedSkyRegion> partial) {
  return finalize_operation(ctx, "footprint.boundary_interval",
                            std::move(partial),
                            [](const EnclosedSkyRegion& region) {
                              return check_region(region);
                            });
}

Result<Done> validate_derivative_interval(
    rt::InvocationContext ctx, Ref<FootprintDerivativeIntervalInput> input) {
  return validate_with(ctx, input, "footprint.derivative_interval",
                       [](const FootprintDerivativeIntervalInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the derivative enclosure cannot be computed");
                         return check_box(value.box);
                       });
}

Result<rt::PlanBounds> bounds_derivative_interval(
    rt::InvocationContext, Ref<FootprintDerivativeIntervalInput>) {
  return model::ok_result(bounds_of(1, 1, false));
}

Result<rt::InvocationPlan> plan_derivative_interval(
    rt::InvocationContext, Ref<FootprintDerivativeIntervalInput> input,
    rt::PlanOptions options) {
  const U64 r = wcs_persistent(input->provider->model());
  return model::ok_result(plan_of(
      "footprint.derivative_interval", options, 1, r, 64 * 1024,
      digest({"footprint.derivative_interval",
              input->provider->model()
                  ? input->provider->model()->identity.object_id
                  : Id{},
              std::to_string(input->box.lower[0]),
              std::to_string(input->box.upper[0])})));
}

rt::Task<DerivativeResult> run_derivative_interval(
    rt::InvocationContext ctx, Ref<FootprintDerivativeIntervalInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<DerivativeResult>(
      ctx, plan, "footprint.derivative_interval",
      [input](acsd::Context& science) -> Result<DerivativeResult> {
        return input->provider->derivative_interval(science, input->box);
      });
}

rt::Task<DerivativeResult> finalize_derivative_interval(
    rt::InvocationContext ctx, Ref<FootprintDerivativeIntervalInput>,
    Ref<rt::InvocationPlan>, Result<DerivativeResult> partial) {
  return finalize_operation(
      ctx, "footprint.derivative_interval", std::move(partial),
      [](const DerivativeResult& result) -> Result<Done> {
        for (std::size_t i = 0; i < 6; ++i) {
          const Interval& enclosure = result.enclosure[i];
          const double value = result.jacobian[i];
          if (!std::isfinite(value) || !std::isfinite(enclosure.lower) ||
              !std::isfinite(enclosure.upper))
            return failure<Done>(Code::invalid_model,
                                 "the derivative enclosure is non-finite",
                                 "not publishable");
          if (!(enclosure.lower <= value && value <= enclosure.upper))
            return failure<Done>(Code::invalid_model,
                                 "the derivative is outside its enclosure",
                                 "not publishable");
        }
        return model::ok_result(Done{});
      });
}

// ---------------------------------------------------------------------------
// refinable.open / refinable.refine
// ---------------------------------------------------------------------------

Result<Done> validate_refinable_open(rt::InvocationContext ctx,
                                     Ref<RefinableOpenInput> input) {
  return validate_with(ctx, input, "refinable.open",
                       [](const RefinableOpenInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the geometry snapshot cannot be opened");
                         if (value.snapshot.empty())
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'snapshot' must not be empty",
                               "the geometry snapshot cannot be opened");
                         return model::ok_result(Done{});
                       });
}

Result<rt::PlanBounds> bounds_refinable_open(
    rt::InvocationContext, Ref<RefinableOpenInput>) {
  return model::ok_result(bounds_of(64, 64, false));
}

Result<rt::InvocationPlan> plan_refinable_open(
    rt::InvocationContext, Ref<RefinableOpenInput> input,
    rt::PlanOptions options) {
  return model::ok_result(
      plan_of("refinable.open", options, 64, 4096, 64 * 1024,
              digest({"refinable.open", input->snapshot})));
}

rt::Task<Ref<GeometrySnapshot>> run_refinable_open(
    rt::InvocationContext ctx, Ref<RefinableOpenInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<GeometrySnapshot>>(
      ctx, plan, "refinable.open",
      [input](acsd::Context& science) -> Result<Ref<GeometrySnapshot>> {
        return input->provider->open(science, input->snapshot);
      });
}

rt::Task<Ref<GeometrySnapshot>> finalize_refinable_open(
    rt::InvocationContext ctx, Ref<RefinableOpenInput>, Ref<rt::InvocationPlan>,
    Result<Ref<GeometrySnapshot>> partial) {
  return finalize_operation(
      ctx, "refinable.open", std::move(partial),
      [](const Ref<GeometrySnapshot>& snapshot) -> Result<Done> {
        if (!snapshot)
          return failure<Done>(Code::invalid_model, "the geometry snapshot is null",
                               "not publishable");
        return model::ok_result(Done{});
      });
}

Result<Done> validate_refinable_refine(rt::InvocationContext ctx,
                                       Ref<RefinableRefineInput> input) {
  return validate_with(ctx, input, "refinable.refine",
                       [](const RefinableRefineInput& value) {
                         if (!value.provider)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'provider' is a required object reference",
                               "the geometry revision cannot be built");
                         if (!value.base)
                           return failure<Done>(
                               Code::invalid_request,
                               "field 'base' is a required object reference",
                               "the geometry revision cannot be built");
                         return check_budget(value.request);
                       });
}

Result<rt::PlanBounds> bounds_refinable_refine(
    rt::InvocationContext, Ref<RefinableRefineInput> input) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  return model::ok_result(bounds_of(candidates, candidates, true));
}

Result<rt::InvocationPlan> plan_refinable_refine(
    rt::InvocationContext, Ref<RefinableRefineInput> input,
    rt::PlanOptions options) {
  const U64 candidates = std::max<U64>(1, candidate_bound(input->request));
  const U64 work = budget_work(input->request, candidates);
  const U64 r = snapshot_bytes(input->base) + 4096 +
                budget_functional_bytes(input->request);
  const U64 f = sat_mul(candidates, 512) + 64 * 1024;
  return model::ok_result(plan_of(
      "refinable.refine", options, work, r, f,
      digest({"refinable.refine", input->base->identity.object_id})));
}

rt::Task<Ref<GeometryRevision>> run_refinable_refine(
    rt::InvocationContext ctx, Ref<RefinableRefineInput> input,
    Ref<rt::InvocationPlan> plan) {
  return run_operation<Ref<GeometryRevision>>(
      ctx, plan, "refinable.refine",
      [input](acsd::Context& science) -> Result<Ref<GeometryRevision>> {
        return input->provider->refine(science, input->base, input->request);
      });
}

rt::Task<Ref<GeometryRevision>> finalize_refinable_refine(
    rt::InvocationContext ctx, Ref<RefinableRefineInput>,
    Ref<rt::InvocationPlan>, Result<Ref<GeometryRevision>> partial) {
  return finalize_operation(ctx, "refinable.refine", std::move(partial),
                            [](const Ref<GeometryRevision>& revision) {
                              return check_revision(revision);
                            });
}

}  // namespace

// Public plan introspection for the independent process tests.
Result<rt::InvocationPlan> plan_geometry_overlap_make_footprint_provider(
    rt::InvocationContext ctx, Ref<MakeFootprintProviderInput> input,
    rt::PlanOptions options) {
  return plan_make_footprint_provider(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_detector_footprint(
    rt::InvocationContext ctx, Ref<DetectorFootprintInput> input,
    rt::PlanOptions options) {
  return plan_detector_footprint(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_footprint(
    rt::InvocationContext ctx, Ref<FootprintInput> input,
    rt::PlanOptions options) {
  return plan_footprint(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_measure(
    rt::InvocationContext ctx, Ref<MeasureInput> input,
    rt::PlanOptions options) {
  return plan_measure(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_compute_overlap(
    rt::InvocationContext ctx, Ref<ComputeOverlapInput> input,
    rt::PlanOptions options) {
  return plan_compute_overlap(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_refine_geometry(
    rt::InvocationContext ctx, Ref<RefineGeometryInput> input,
    rt::PlanOptions options) {
  return plan_refine_geometry(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_forward_interval(
    rt::InvocationContext ctx, Ref<FootprintForwardIntervalInput> input,
    rt::PlanOptions options) {
  return plan_forward_interval(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_boundary_interval(
    rt::InvocationContext ctx, Ref<FootprintBoundaryIntervalInput> input,
    rt::PlanOptions options) {
  return plan_boundary_interval(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_footprint_derivative_interval(
    rt::InvocationContext ctx, Ref<FootprintDerivativeIntervalInput> input,
    rt::PlanOptions options) {
  return plan_derivative_interval(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_refinable_open(
    rt::InvocationContext ctx, Ref<RefinableOpenInput> input,
    rt::PlanOptions options) {
  return plan_refinable_open(ctx, std::move(input), options);
}
Result<rt::InvocationPlan> plan_geometry_overlap_refinable_refine(
    rt::InvocationContext ctx, Ref<RefinableRefineInput> input,
    rt::PlanOptions options) {
  return plan_refinable_refine(ctx, std::move(input), options);
}

Result<rt::Done> register_geometry_overlap(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for(kProcess);
  if (!ok(descriptor))
    return failure<Done>(descriptor.code,
                         "the generated descriptor for geometry.overlap is "
                         "unavailable",
                         "the process cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process =
      builder.begin(*descriptor.value);
  if (!ok(process))
    return failure<Done>(process.code,
                         "the geometry.overlap process cannot be begun",
                         "the process cannot be registered");

  Result<Done> step = builder.add<MakeFootprintProviderInput, Ref<FootprintProvider>>(
      *process.value,
      rt::Operation<MakeFootprintProviderInput, Ref<FootprintProvider>>{
          "make_footprint_provider", {"acsd.input.MakeFootprintProviderInput", 1},
          {"acsd.FootprintProvider", 1}, &validate_make_footprint_provider,
          &bounds_make_footprint_provider, &plan_make_footprint_provider,
          &run_make_footprint_provider, &finalize_make_footprint_provider});
  if (!ok(step)) return step;

  step = builder.add<DetectorFootprintInput, Ref<DetectorFootprint>>(
      *process.value,
      rt::Operation<DetectorFootprintInput, Ref<DetectorFootprint>>{
          "detector_footprint", {"acsd.input.DetectorFootprintInput", 1},
          {"acsd.DetectorFootprint", 1}, &validate_detector_footprint,
          &bounds_detector_footprint, &plan_detector_footprint,
          &run_detector_footprint, &finalize_detector_footprint});
  if (!ok(step)) return step;

  step = builder.add<FootprintInput, Ref<CurvedBoundary>>(
      *process.value,
      rt::Operation<FootprintInput, Ref<CurvedBoundary>>{
          "footprint", {"acsd.input.FootprintInput", 1},
          {"acsd.CurvedBoundary", 1}, &validate_footprint, &bounds_footprint,
          &plan_footprint, &run_footprint, &finalize_footprint});
  if (!ok(step)) return step;

  step = builder.add<MeasureInput, SphericalAreaInterval>(
      *process.value,
      rt::Operation<MeasureInput, SphericalAreaInterval>{
          "measure", {"acsd.input.MeasureInput", 1},
          {"acsd.SphericalAreaInterval", 1}, &validate_measure, &bounds_measure,
          &plan_measure, &run_measure, &finalize_measure});
  if (!ok(step)) return step;

  step = builder.add<ComputeOverlapInput, Ref<Overlaps>>(
      *process.value,
      rt::Operation<ComputeOverlapInput, Ref<Overlaps>>{
          "compute_overlap", {"acsd.input.ComputeOverlapInput", 1},
          {"acsd.Overlaps", 1}, &validate_compute_overlap,
          &bounds_compute_overlap, &plan_compute_overlap, &run_compute_overlap,
          &finalize_compute_overlap});
  if (!ok(step)) return step;

  step = builder.add<RefineGeometryInput, Ref<GeometryRevision>>(
      *process.value,
      rt::Operation<RefineGeometryInput, Ref<GeometryRevision>>{
          "refine_geometry", {"acsd.input.RefineGeometryInput", 1},
          {"acsd.GeometryRevision", 1}, &validate_refine_geometry,
          &bounds_refine_geometry, &plan_refine_geometry, &run_refine_geometry,
          &finalize_refine_geometry});
  if (!ok(step)) return step;

  step = builder.add<FootprintForwardIntervalInput, EnclosedSkyRegion>(
      *process.value,
      rt::Operation<FootprintForwardIntervalInput, EnclosedSkyRegion>{
          "footprint.forward_interval",
          {"acsd.input.FootprintForwardIntervalInput", 1},
          {"acsd.EnclosedSkyRegion", 1}, &validate_forward_interval,
          &bounds_forward_interval, &plan_forward_interval,
          &run_forward_interval, &finalize_forward_interval});
  if (!ok(step)) return step;

  step = builder.add<FootprintBoundaryIntervalInput, EnclosedSkyRegion>(
      *process.value,
      rt::Operation<FootprintBoundaryIntervalInput, EnclosedSkyRegion>{
          "footprint.boundary_interval",
          {"acsd.input.FootprintBoundaryIntervalInput", 1},
          {"acsd.EnclosedSkyRegion", 1}, &validate_boundary_interval,
          &bounds_boundary_interval, &plan_boundary_interval,
          &run_boundary_interval, &finalize_boundary_interval});
  if (!ok(step)) return step;

  step = builder.add<FootprintDerivativeIntervalInput, DerivativeResult>(
      *process.value,
      rt::Operation<FootprintDerivativeIntervalInput, DerivativeResult>{
          "footprint.derivative_interval",
          {"acsd.input.FootprintDerivativeIntervalInput", 1},
          {"acsd.DerivativeResult", 1}, &validate_derivative_interval,
          &bounds_derivative_interval, &plan_derivative_interval,
          &run_derivative_interval, &finalize_derivative_interval});
  if (!ok(step)) return step;

  step = builder.add<RefinableOpenInput, Ref<GeometrySnapshot>>(
      *process.value,
      rt::Operation<RefinableOpenInput, Ref<GeometrySnapshot>>{
          "refinable.open", {"acsd.input.RefinableOpenInput", 1},
          {"acsd.GeometrySnapshot", 1}, &validate_refinable_open,
          &bounds_refinable_open, &plan_refinable_open, &run_refinable_open,
          &finalize_refinable_open});
  if (!ok(step)) return step;

  step = builder.add<RefinableRefineInput, Ref<GeometryRevision>>(
      *process.value,
      rt::Operation<RefinableRefineInput, Ref<GeometryRevision>>{
          "refinable.refine", {"acsd.input.RefinableRefineInput", 1},
          {"acsd.GeometryRevision", 1}, &validate_refinable_refine,
          &bounds_refinable_refine, &plan_refinable_refine,
          &run_refinable_refine, &finalize_refinable_refine});
  return step;
}

}  // namespace acsd::modules
