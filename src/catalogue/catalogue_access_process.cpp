// The catalogue.access typed process adapter (M5).
//
// Five whitelisted operations: the offline index build, the provider factory
// and the three provider methods.  Each one calls the unique implementation in
// src/catalogue; the provider methods are invoked through the typed Ref, never
// downcast.  The legacy entry points are whole-input calls, so the plan is one
// atomic barrier segment sized from the actual array metadata.
#include <acsd/modules/catalogue.hpp>

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

constexpr const char* kProcess = "catalogue.access";
constexpr U64 kPlanControlBytes = 16ull << 10;
constexpr U64 kStageControlBytes = 1ull << 10;
constexpr U64 kCohortControlBytes = 4ull << 10;
constexpr U64 kCleanupBytes = 64ull << 10;
constexpr U64 kCleanupWorkUpper = 32;
constexpr U64 kWorkspaceUpper = 2ull << 20;
constexpr U32 kMaxPageSize = 4096;

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

U64 element_count(const ArrayRef& array) {
  if (!array) return 0;
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) return 0;
    total = saturating_mul(total, extent);
  }
  return array->shape.empty() ? 0 : total;
}

std::string array_text(const ArrayRef& array) {
  if (!array) return "<absent>";
  std::string text = array->identity.content_hash + "|" + array->index_id + "|" +
                     array->unit.scale + "|" + std::to_string(static_cast<int>(array->dtype)) + "|";
  for (U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

// --- shared atomic plan ----------------------------------------------------

Result<rt::InvocationPlan> atomic_plan(rt::PlanOptions options, U64 elements, U64 persistent,
                                       U64 progress, U64 work_upper, const std::string& input_hash,
                                       const std::string& request_hash,
                                       const std::string& algorithm_hash) {
  if (elements == 0 || options.block_elements == 0 || options.block_elements > elements)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the block size is outside the legal range",
                                               "the catalogue step cannot be planned");
  if (options.window != 1)
    return adapter_failure<rt::InvocationPlan>(Code::invalid_request,
                                               "the atomic catalogue step uses one window",
                                               "the catalogue step cannot be planned");
  if (persistent == std::numeric_limits<U64>::max() ||
      progress == std::numeric_limits<U64>::max())
    return adapter_failure<rt::InvocationPlan>(Code::resource_exhausted,
                                               "the resource model overflows",
                                               "the catalogue step cannot be planned");
  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = elements;
  plan.block_elements = options.block_elements;
  plan.cohorts = 1;
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent;
  plan.progress_floor.host_bytes = progress;
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
  region.persistent.host_bytes = persistent;
  region.progress_floor.host_bytes = progress;

  rt::StageSpec stage;
  stage.id = 0;
  stage.scratch.host_bytes = progress;
  stage.control_bytes = kStageControlBytes;
  stage.work_upper = work_upper;
  region.stages = {stage};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

template <class Out>
rt::Task<Out> run_with_science(rt::InvocationContext context, Ref<rt::InvocationPlan> plan,
                               const char* semantic, U64 work_upper,
                               std::function<Result<Out>(acsd::Context&)> body) {
  if (!plan)
    return rt::ready<Out>(context, adapter_failure<Out>(Code::invalid_request,
                                                        "no invocation plan supplied",
                                                        "the catalogue step cannot run"));
  rt::TaskSpec spec;
  spec.key = rt::Key{plan->epoch, 0, 0, 0};
  spec.running.host_bytes = std::max<U64>(plan->progress_floor.host_bytes, 1);
  spec.running.cpu_tokens = 1;
  spec.work_upper = std::max<U64>(work_upper, 1);
  spec.semantic = semantic;
  return rt::submit_cpu<Out>(
      context, spec,
      [body = std::move(body)](rt::InvocationContext task) mutable -> Result<Out> {
        auto bridge = rt::detail::make_science_context(
            task, task.persistent ? task.persistent : task.progress);
        if (!ok(bridge))
          return adapter_failure<Out>(bridge.code, "the science context bridge cannot be created",
                                      "the catalogue step cannot run");
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

std::string columns_text(const CatalogueColumns& columns) {
  return array_text(columns.source_ids) + "|" + array_text(columns.astrometry) + "|" +
         array_text(columns.brightness_mag) + "|" + array_text(columns.xp_coefficients);
}

std::string build_request_text(const CatalogueBuildRequest& request) {
  return std::to_string(request.sky_order) + "|" + std::to_string(request.max_stars_per_cell) +
         "|" + std::to_string(request.neighbors_per_anchor) + "|" +
         std::to_string(request.quads_per_anchor) + "|" +
         number_text(request.quad_baseline_rad.lower) + "," +
         number_text(request.quad_baseline_rad.upper) + "|" + request.release + "|" +
         request.calibration;
}

// ---------------------------------------------------------------------------
// build_catalogue_index
// ---------------------------------------------------------------------------

Result<Done> require_build(Ref<BuildCatalogueIndexInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "build_catalogue_index cannot be validated");
  if (!input->request.columns)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue columns supplied",
                                 "build_catalogue_index cannot be validated");
  const CatalogueColumns& columns = *input->request.columns;
  if (!columns.source_ids || !columns.astrometry || !columns.brightness_mag)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the catalogue columns lack a mandatory array",
                                 "build_catalogue_index cannot be validated");
  if (columns.source_ids->dtype != DType::u64)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the source identifier array must be u64",
                                 "build_catalogue_index cannot be validated");
  if (columns.astrometry->dtype != DType::f64 || columns.brightness_mag->dtype != DType::f64)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the astrometry and brightness arrays must be f64",
                                 "build_catalogue_index cannot be validated");
  const U64 count = element_count(columns.source_ids);
  if (count == 0)
    return adapter_failure<Done>(Code::invalid_request, "the catalogue is empty",
                                 "build_catalogue_index cannot be validated");
  if (columns.astrometry->shape.size() != 2 || columns.astrometry->shape[0] != count ||
      columns.astrometry->shape[1] != 8)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the astrometry array must have the shape [N,8]",
                                 "build_catalogue_index cannot be validated");
  if (element_count(columns.brightness_mag) != count)
    return adapter_failure<Done>(Code::invalid_model,
                                 "the brightness array does not match the source count",
                                 "build_catalogue_index cannot be validated");
  const CatalogueBuildRequest& request = input->request;
  if (request.sky_order > 29)
    return adapter_failure<Done>(Code::invalid_request, "the sky order is out of range",
                                 "build_catalogue_index cannot be validated");
  if (request.max_stars_per_cell == 0 || request.neighbors_per_anchor < 3 ||
      request.quads_per_anchor == 0)
    return adapter_failure<Done>(Code::invalid_request,
                                 "the shard and anchor budgets must be positive",
                                 "build_catalogue_index cannot be validated");
  if (!std::isfinite(request.quad_baseline_rad.lower) ||
      !std::isfinite(request.quad_baseline_rad.upper) ||
      request.quad_baseline_rad.lower < 0.0 ||
      request.quad_baseline_rad.upper < request.quad_baseline_rad.lower)
    return adapter_failure<Done>(Code::invalid_request, "the baseline range is invalid",
                                 "build_catalogue_index cannot be validated");
  if (request.release.empty() || request.calibration.empty())
    return adapter_failure<Done>(Code::invalid_request,
                                 "the catalogue release or calibration id is empty",
                                 "build_catalogue_index cannot be validated");
  return model::ok_result(Done{});
}

Result<Done> validate_build(rt::InvocationContext, Ref<BuildCatalogueIndexInput> input) {
  return require_build(input);
}

U64 build_elements(const BuildCatalogueIndexInput& input) {
  return std::max<U64>(element_count(input.request.columns->source_ids), 1);
}

U64 build_quads(const BuildCatalogueIndexInput& input) {
  const U64 count = element_count(input.request.columns->source_ids);
  const U64 cells = saturating_mul(12, U64{1} << std::min<U32>(2 * input.request.sky_order, 62));
  const U64 anchors = std::min<U64>(count, cells);
  // Two mirror descriptors are kept per accepted quad (catalogue.access.md).
  return saturating_mul(saturating_mul(anchors, input.request.quads_per_anchor), 2);
}

U64 build_work(const BuildCatalogueIndexInput& input) {
  const U64 count = element_count(input.request.columns->source_ids);
  const U64 quads = build_quads(input);
  return saturating_add(saturating_add(count, saturating_mul(quads, 2)),
                        saturating_mul(count, input.request.max_stars_per_cell));
}

Result<rt::PlanBounds> bounds_build(rt::InvocationContext, Ref<BuildCatalogueIndexInput> input) {
  Result<Done> validation = require_build(input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the catalogue build input is invalid",
                                           "build_catalogue_index cannot be planned");
  const U64 elements = build_elements(*input);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(elements, 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_build(rt::InvocationContext, Ref<BuildCatalogueIndexInput> input,
                                      rt::PlanOptions options) {
  Result<Done> validation = require_build(input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code,
                                               "the catalogue build input is invalid",
                                               "build_catalogue_index cannot be planned");
  const U64 count = element_count(input->request.columns->source_ids);
  const U64 quads = build_quads(*input);
  // [Nq,4] u64 ids + [Nq,4] f64 descriptors + [Nq] f64 baselines + [Nq,3]
  // f64 centres + the KD arrays (left/right u64, axis u8, split f64,
  // bounds [Nq,4,2] f64), all with their metadata.
  const U64 quad_bytes = saturating_add(
      saturating_add(saturating_mul(32, quads), saturating_mul(32, quads)),
      saturating_add(saturating_mul(8, quads),
                     saturating_add(saturating_mul(24, quads),
                                    saturating_add(saturating_mul(16, quads),
                                                   saturating_add(saturating_mul(quads, 1),
                                                                  saturating_add(
                                                                      saturating_mul(8, quads),
                                                                      saturating_mul(64, quads)))))));
  const U64 transient = saturating_add(
      saturating_mul(64, count),
      saturating_add(saturating_mul(64, quads),
                     saturating_add(kWorkspaceUpper, kCohortControlBytes)));
  const U64 work = build_work(*input);
  return atomic_plan(options, build_elements(*input),
                     saturating_add(quad_bytes, 256ull << 10), transient, work,
                     hex_id("columns|" + columns_text(*input->request.columns)),
                     hex_id("build|" + build_request_text(input->request)),
                     hex_id("catalogue.access|build_catalogue_index|quad_kd_v1|1"));
}

rt::Task<Ref<CatalogueIndex>> run_build(rt::InvocationContext context,
                                        Ref<BuildCatalogueIndexInput> input,
                                        Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<CatalogueIndex>>(
        context, adapter_failure<Ref<CatalogueIndex>>(Code::invalid_request,
                                                      "the typed input is null",
                                                      "build_catalogue_index cannot run"));
  return run_with_science<Ref<CatalogueIndex>>(
      context, plan, "catalogue.access.build_catalogue_index", build_work(*input),
      [input](acsd::Context& science) -> Result<Ref<CatalogueIndex>> {
        return acsd::build_catalogue_index(science, input->request);
      });
}

rt::Task<Ref<CatalogueIndex>> finalize_build(rt::InvocationContext context,
                                             Ref<BuildCatalogueIndexInput>, Ref<rt::InvocationPlan>,
                                             Result<Ref<CatalogueIndex>> prior) {
  return passthrough_finalize<Ref<CatalogueIndex>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// make_catalogue_provider
// ---------------------------------------------------------------------------

Result<Done> validate_provider(rt::InvocationContext, Ref<MakeCatalogueProviderInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "make_catalogue_provider cannot be validated");
  if (!input->index)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue index supplied",
                                 "make_catalogue_provider cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_provider(rt::InvocationContext, Ref<MakeCatalogueProviderInput> input) {
  Result<Done> validation = validate_provider({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the provider input is invalid",
                                           "make_catalogue_provider cannot be planned");
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = 1;
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_provider(rt::InvocationContext, Ref<MakeCatalogueProviderInput> input,
                                         rt::PlanOptions options) {
  Result<Done> validation = validate_provider({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the provider input is invalid",
                                               "make_catalogue_provider cannot be planned");
  return atomic_plan(options, 1, 64ull << 10, 1ull << 20, 1,
                     hex_id("index|" + input->index->identity.content_hash),
                     hex_id("request|make_catalogue_provider"),
                     hex_id("catalogue.access|make_catalogue_provider|1"));
}

rt::Task<Ref<CatalogueProvider>> run_provider(rt::InvocationContext context,
                                              Ref<MakeCatalogueProviderInput> input,
                                              Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<Ref<CatalogueProvider>>(
        context, adapter_failure<Ref<CatalogueProvider>>(Code::invalid_request,
                                                         "the typed input is null",
                                                         "make_catalogue_provider cannot run"));
  return run_with_science<Ref<CatalogueProvider>>(
      context, plan, "catalogue.access.make_catalogue_provider", 1,
      [input](acsd::Context& science) -> Result<Ref<CatalogueProvider>> {
        return acsd::make_catalogue_provider(science, input->index);
      });
}

rt::Task<Ref<CatalogueProvider>> finalize_provider(rt::InvocationContext context,
                                                   Ref<MakeCatalogueProviderInput>,
                                                   Ref<rt::InvocationPlan>,
                                                   Result<Ref<CatalogueProvider>> prior) {
  return passthrough_finalize<Ref<CatalogueProvider>>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// catalogue.query
// ---------------------------------------------------------------------------

Result<Done> validate_query(rt::InvocationContext, Ref<CatalogueQueryInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "catalogue.query cannot be validated");
  if (!input->provider)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue provider supplied",
                                 "catalogue.query cannot be validated");
  if (input->request.page_size == 0 || input->request.page_size > kMaxPageSize)
    return adapter_failure<Done>(Code::invalid_request, "the page size is outside 1..4096",
                                 "catalogue.query cannot be validated");
  if (!std::isfinite(input->request.epoch_julian_year))
    return adapter_failure<Done>(Code::invalid_request, "the query epoch is not finite",
                                 "catalogue.query cannot be validated");
  if (!std::isfinite(input->request.region.radius_rad) || input->request.region.radius_rad <= 0.0)
    return adapter_failure<Done>(Code::invalid_request, "the search radius is not positive",
                                 "catalogue.query cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_query(rt::InvocationContext, Ref<CatalogueQueryInput> input) {
  Result<Done> validation = validate_query({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the catalogue query is invalid",
                                           "catalogue.query cannot be planned");
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(input->request.page_size, kMaxPageSize);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

U64 page_plan_work(U64 page_size) { return std::max<U64>(page_size, 1); }

Result<rt::InvocationPlan> plan_query(rt::InvocationContext, Ref<CatalogueQueryInput> input,
                                      rt::PlanOptions options) {
  Result<Done> validation = validate_query({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the catalogue query is invalid",
                                               "catalogue.query cannot be planned");
  const U64 page = input->request.page_size;
  const U64 progress = saturating_add(saturating_mul(256, page),
                                      saturating_add(kWorkspaceUpper, kCohortControlBytes));
  const U64 persistent = saturating_add(saturating_mul(256, page), 256ull << 10);
  return atomic_plan(options, page, persistent, progress, page_plan_work(page),
                     hex_id("query|" + number_text(input->request.region.center.nx) + "," +
                            number_text(input->request.region.center.ny) + "," +
                            number_text(input->request.region.center.nz) + "|radius|" +
                            number_text(input->request.region.radius_rad) + "|epoch|" +
                            number_text(input->request.epoch_julian_year) + "|page|" +
                            std::to_string(page) + "|cursor|" + input->request.cursor),
                     hex_id("provider|" + input->provider->dataset_hash()),
                     hex_id("catalogue.access|catalogue.query|page_v1|1"));
}

rt::Task<CataloguePage> run_query(rt::InvocationContext context, Ref<CatalogueQueryInput> input,
                                  Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<CataloguePage>(context, adapter_failure<CataloguePage>(
                                                 Code::invalid_request, "the typed input is null",
                                                 "catalogue.query cannot run"));
  return run_with_science<CataloguePage>(
      context, plan, "catalogue.access.catalogue.query", page_plan_work(input->request.page_size),
      [input](acsd::Context& science) -> Result<CataloguePage> {
        return input->provider->query(science, input->request);
      });
}

rt::Task<CataloguePage> finalize_query(rt::InvocationContext context, Ref<CatalogueQueryInput>,
                                       Ref<rt::InvocationPlan>, Result<CataloguePage> prior) {
  return passthrough_finalize<CataloguePage>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// catalogue.query_quads
// ---------------------------------------------------------------------------

Result<Done> validate_quads(rt::InvocationContext, Ref<CatalogueQueryQuadsInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "catalogue.query_quads cannot be validated");
  if (!input->provider)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue provider supplied",
                                 "catalogue.query_quads cannot be validated");
  if (input->request.page_size == 0 || input->request.page_size > kMaxPageSize)
    return adapter_failure<Done>(Code::invalid_request, "the page size is outside 1..4096",
                                 "catalogue.query_quads cannot be validated");
  if (!(input->request.baseline_rad.lower >= 0.0) ||
      !(input->request.baseline_rad.upper >= input->request.baseline_rad.lower) ||
      !std::isfinite(input->request.baseline_rad.upper))
    return adapter_failure<Done>(Code::invalid_request, "the baseline range is invalid",
                                 "catalogue.query_quads cannot be validated");
  for (const Interval& component : input->request.descriptor)
    if (!std::isfinite(component.lower) || !std::isfinite(component.upper) ||
        component.upper < component.lower)
      return adapter_failure<Done>(Code::invalid_request,
                                   "a quad descriptor interval is invalid",
                                   "catalogue.query_quads cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_quads(rt::InvocationContext, Ref<CatalogueQueryQuadsInput> input) {
  Result<Done> validation = validate_quads({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the quad query is invalid",
                                           "catalogue.query_quads cannot be planned");
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(input->request.page_size, kMaxPageSize);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_quads(rt::InvocationContext, Ref<CatalogueQueryQuadsInput> input,
                                      rt::PlanOptions options) {
  Result<Done> validation = validate_quads({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the quad query is invalid",
                                               "catalogue.query_quads cannot be planned");
  const U64 page = input->request.page_size;
  const U64 progress = saturating_add(saturating_mul(160, page),
                                      saturating_add(kWorkspaceUpper, kCohortControlBytes));
  const U64 persistent = saturating_add(saturating_mul(160, page), 256ull << 10);
  return atomic_plan(options, page, persistent, progress, page_plan_work(page),
                     hex_id("descriptor|" + number_text(input->request.descriptor[0].lower) + "," +
                            number_text(input->request.descriptor[0].upper) + "|baseline|" +
                            number_text(input->request.baseline_rad.lower) + "," +
                            number_text(input->request.baseline_rad.upper) + "|page|" +
                            std::to_string(page) + "|cursor|" + input->request.cursor),
                     hex_id("provider|" + input->provider->dataset_hash()),
                     hex_id("catalogue.access|catalogue.query_quads|quad_search_v1|1"));
}

rt::Task<QuadPage> run_quads(rt::InvocationContext context, Ref<CatalogueQueryQuadsInput> input,
                             Ref<rt::InvocationPlan> plan) {
  if (!input)
    return rt::ready<QuadPage>(context, adapter_failure<QuadPage>(
                                             Code::invalid_request, "the typed input is null",
                                             "catalogue.query_quads cannot run"));
  return run_with_science<QuadPage>(
      context, plan, "catalogue.access.catalogue.query_quads", page_plan_work(input->request.page_size),
      [input](acsd::Context& science) -> Result<QuadPage> {
        return input->provider->query_quads(science, input->request);
      });
}

rt::Task<QuadPage> finalize_quads(rt::InvocationContext context, Ref<CatalogueQueryQuadsInput>,
                                  Ref<rt::InvocationPlan>, Result<QuadPage> prior) {
  return passthrough_finalize<QuadPage>(context, std::move(prior));
}

// ---------------------------------------------------------------------------
// catalogue.lookup
// ---------------------------------------------------------------------------

U64 lookup_elements(const CatalogueLookupInput& input) {
  return input.source_ids ? input.source_ids->size() : 0;
}

Result<Done> validate_lookup(rt::InvocationContext, Ref<CatalogueLookupInput> input) {
  if (!input) return adapter_failure<Done>(Code::invalid_request, "the typed input is null",
                                           "catalogue.lookup cannot be validated");
  if (!input->provider)
    return adapter_failure<Done>(Code::invalid_request, "no catalogue provider supplied",
                                 "catalogue.lookup cannot be validated");
  if (!input->source_ids || input->source_ids->empty())
    return adapter_failure<Done>(Code::invalid_request, "no source identifiers supplied",
                                 "catalogue.lookup cannot be validated");
  return model::ok_result(Done{});
}

Result<rt::PlanBounds> bounds_lookup(rt::InvocationContext, Ref<CatalogueLookupInput> input) {
  Result<Done> validation = validate_lookup({}, input);
  if (!ok(validation))
    return adapter_failure<rt::PlanBounds>(validation.code, "the lookup is invalid",
                                           "catalogue.lookup cannot be planned");
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min<U64>(lookup_elements(*input), 65536);
  bounds.max_window = 1;
  bounds.allow_spill = false;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_lookup(rt::InvocationContext, Ref<CatalogueLookupInput> input,
                                       rt::PlanOptions options) {
  Result<Done> validation = validate_lookup({}, input);
  if (!ok(validation))
    return adapter_failure<rt::InvocationPlan>(validation.code, "the lookup is invalid",
                                               "catalogue.lookup cannot be planned");
  const U64 count = lookup_elements(*input);
  const U64 progress = saturating_add(saturating_mul(256, count),
                                      saturating_add(kWorkspaceUpper, kCohortControlBytes));
  const U64 persistent = saturating_add(saturating_mul(256, count), 128ull << 10);
  return atomic_plan(options, count, persistent, progress, count,
                     hex_id("lookup|" + std::to_string(count) + "|" +
                            std::to_string(input->source_ids->front()) + "|" +
                            std::to_string(input->source_ids->back())),
                     hex_id("provider|" + input->provider->dataset_hash()),
                     hex_id("catalogue.access|catalogue.lookup|id_lookup_v1|1"));
}

rt::Task<std::vector<CatalogueStar>> run_lookup(rt::InvocationContext context,
                                                Ref<CatalogueLookupInput> input,
                                                Ref<rt::InvocationPlan> plan) {
  using Stars = std::vector<CatalogueStar>;
  if (!input)
    return rt::ready<Stars>(context, adapter_failure<Stars>(Code::invalid_request,
                                                            "the typed input is null",
                                                            "catalogue.lookup cannot run"));
  const U64 count = lookup_elements(*input);
  return run_with_science<Stars>(
      context, plan, "catalogue.access.catalogue.lookup", count,
      [input](acsd::Context& science) -> Result<Stars> {
        return input->provider->lookup(
            science, std::span<const U64>(input->source_ids->data(), input->source_ids->size()));
      });
}

rt::Task<std::vector<CatalogueStar>> finalize_lookup(rt::InvocationContext context,
                                                     Ref<CatalogueLookupInput>,
                                                     Ref<rt::InvocationPlan>,
                                                     Result<std::vector<CatalogueStar>> prior) {
  return passthrough_finalize<std::vector<CatalogueStar>>(context, std::move(prior));
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
  const rt::Operation<In, Out> operation{operation_id, rt::TypeId{input_schema, 1},
                                         rt::TypeId{output_schema, 1}, validate, bounds, plan, run,
                                         finalize};
  return builder.add<In, Out>(process, operation);
}

}  // namespace

Result<Done> register_catalogue_access(rt::RegistryBuilder& builder) {
  Result<rt::ProcessDescriptor> descriptor =
      acsd::generated::descriptor_for("catalogue.access");
  if (!ok(descriptor))
    return adapter_failure<Done>(descriptor.code, "the catalogue.access descriptor is unavailable",
                                 "catalogue.access cannot be registered");
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(*descriptor.value);
  if (!ok(process))
    return adapter_failure<Done>(process.code, "the catalogue.access process cannot be started",
                                 "catalogue.access cannot be registered");
  Result<Done> step;
  step = add_operation<BuildCatalogueIndexInput, Ref<CatalogueIndex>>(
      builder, *process.value, "build_catalogue_index", "acsd.input.BuildCatalogueIndexInput",
      "acsd.CatalogueIndex", &validate_build, &bounds_build, &plan_build, &run_build,
      &finalize_build);
  if (!ok(step)) return step;
  step = add_operation<MakeCatalogueProviderInput, Ref<CatalogueProvider>>(
      builder, *process.value, "make_catalogue_provider",
      "acsd.input.MakeCatalogueProviderInput", "acsd.CatalogueProvider", &validate_provider,
      &bounds_provider, &plan_provider, &run_provider, &finalize_provider);
  if (!ok(step)) return step;
  step = add_operation<CatalogueQueryInput, CataloguePage>(
      builder, *process.value, "catalogue.query", "acsd.input.CatalogueQueryInput",
      "acsd.CataloguePage", &validate_query, &bounds_query, &plan_query, &run_query,
      &finalize_query);
  if (!ok(step)) return step;
  step = add_operation<CatalogueQueryQuadsInput, QuadPage>(
      builder, *process.value, "catalogue.query_quads", "acsd.input.CatalogueQueryQuadsInput",
      "acsd.QuadPage", &validate_quads, &bounds_quads, &plan_quads, &run_quads, &finalize_quads);
  if (!ok(step)) return step;
  return add_operation<CatalogueLookupInput, std::vector<CatalogueStar>>(
      builder, *process.value, "catalogue.lookup", "acsd.input.CatalogueLookupInput",
      "acsd.vector.CatalogueStar", &validate_lookup, &bounds_lookup, &plan_lookup, &run_lookup,
      &finalize_lookup);
}

// Plan introspection for validation/process/catalogue.access.
Result<rt::InvocationPlan> plan_build_catalogue_index_op(
    rt::InvocationContext ctx, Ref<BuildCatalogueIndexInput> input, rt::PlanOptions options) {
  return plan_build(ctx, input, options);
}
Result<rt::InvocationPlan> plan_make_catalogue_provider_op(
    rt::InvocationContext ctx, Ref<MakeCatalogueProviderInput> input, rt::PlanOptions options) {
  return plan_provider(ctx, input, options);
}
Result<rt::InvocationPlan> plan_catalogue_query_op(rt::InvocationContext ctx,
                                                   Ref<CatalogueQueryInput> input,
                                                   rt::PlanOptions options) {
  return plan_query(ctx, input, options);
}
Result<rt::InvocationPlan> plan_catalogue_query_quads_op(
    rt::InvocationContext ctx, Ref<CatalogueQueryQuadsInput> input, rt::PlanOptions options) {
  return plan_quads(ctx, input, options);
}
Result<rt::InvocationPlan> plan_catalogue_lookup_op(rt::InvocationContext ctx,
                                                    Ref<CatalogueLookupInput> input,
                                                    rt::PlanOptions options) {
  return plan_lookup(ctx, input, options);
}

}  // namespace acsd::modules
