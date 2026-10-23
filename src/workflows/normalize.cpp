// The normalize workflow.
//
// The causal chain of one frame is fixed:
//
//   calibrate -> optional physical regroup -> solve or supplied WCS ->
//   bind_wcs -> fit_psf / measure -> bind_observation_model ->
//   integrate_response / fit_photometry -> apply_scale ->
//   geometry / build_drizzle -> information -> publish
//
// The photometry is joint over all frames: the measurements of every frame
// enter one fit, so that a shared Gaia source keeps one latent and one
// influence chain.  A frame that fails does not cancel the other frames: the
// result carries the partial outcome and the full denominator, and a failed
// frame publishes no product.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <fstream>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include <acsd/modules/catalogue.hpp>
#include <acsd/modules/coordinates.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/geometry.hpp>
#include <acsd/modules/information.hpp>
#include <acsd/modules/observation.hpp>
#include <acsd/modules/photometry.hpp>
#include <acsd/modules/response.hpp>
#include <acsd/modules/sources.hpp>
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include "workflows/normalize_runtime.hpp"
#include "workflows/product_internal.hpp"


namespace acsd {

using rt::Done;
using rt::InvocationContext;
using namespace modules;
using workflows::NormalizeRuntime;
using workflows::WorkflowStorage;
using workflows::ingest_array;
using workflows::ingest_basis;
using workflows::ingest_calibration;
using workflows::ingest_covariance;
using workflows::ingest_footprints;
using workflows::ingest_index;
using workflows::ingest_information;
using workflows::ingest_raw;
using workflows::ingest_response;
using workflows::ingest_spectrum;
using workflows::ingest_star;

namespace {

template <class T>
const T& value_of(const Result<T>& result) {
  return *result.value;
}

template <class T>
Id identity_of(const T& value) {
  if constexpr (requires { value->identity.object_id; })
    return value ? value->identity.object_id : Id{};
  else
    return Id{};
}

inline const char* code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "unknown";
}


// The footprint of the whole detector: the rectangular collection domain of
// the frame in the detector frame.  The geometry module intersects it with
// the leaves of the requested order.

// The footprint of one block of the detector.
//
// The source rows of the geometry are the blocks of the detector: the
// foundation form a_jp = Omega(D_j cap H_p) needs one area per source row and
// per leaf, so the detector is partitioned into blocks of `block` by `block`
// pixels.  The block is the rectangular collection domain of one source row.
Result<Ref<DetectorFootprint>> block_footprint(Context& context, Ref<FootprintSet> set, U64 block,
                                                U64 block_size, U64 source_row) {
  using model::fail;
  using model::ok_result;
  if (!set) {
    return fail<Ref<DetectorFootprint>>(Code::invalid_model, "the frame has no footprint set",
                                        "normalize", "the geometry cannot be computed");
  }
  const U64 blocks_per_row = (set->width + block_size - 1) / block_size;
  const U64 first_x = (block % blocks_per_row) * block_size;
  const U64 first_y = (block / blocks_per_row) * block_size;
  const U64 last_x = std::min(first_x + block_size, set->width);
  const U64 last_y = std::min(first_y + block_size, set->height);
  DetectorFootprint footprint;
  footprint.wcs = set->wcs;
  footprint.pixel_response = set->pixel_response;
  footprint.source = model::source_at(*set->index, source_row);
  footprint.source.sample = block;
  footprint.identity = model::make_identity("acsd.DetectorFootprint", 2, 0,
                                            model::sha256_hex("acsd.normalize.block:" +
                                                              std::to_string(block)),
                                            "acsd");
  footprint.assessment = model::make_assessment(Code::ok);
  footprint.assessment.complete = true;
  const double x0 = static_cast<double>(first_x) - 0.5;
  const double y0 = static_cast<double>(first_y) - 0.5;
  const double x1 = static_cast<double>(last_x) - 0.5;
  const double y1 = static_cast<double>(last_y) - 0.5;
  footprint.contraction_center = PixelCenter0{0.5 * (x0 + x1), 0.5 * (y0 + y1)};
  Ring ring;
  ring.hole = false;
  const double corners[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
  for (const auto& corner : corners) {
    PixelEdge0 edge;
    edge.x = corner[0];
    edge.y = corner[1];
    ring.vertices.push_back(edge);
  }
  footprint.detector_rings.push_back(ring);
  return ok_result(std::shared_ptr<const DetectorFootprint>(
      std::make_shared<DetectorFootprint>(footprint)));
}

Result<Ref<DetectorFootprint>> frame_footprint(Context& context, Ref<FootprintSet> set) {
  using model::fail;
  using model::ok_result;
  if (!set) {
    return fail<Ref<DetectorFootprint>>(Code::invalid_model, "the frame has no footprint set",
                                        "normalize", "the geometry cannot be computed");
  }
  DetectorFootprint footprint;
  footprint.wcs = set->wcs;
  footprint.pixel_response = set->pixel_response;
  if (set->index && set->index->size > 0) {
    footprint.source = model::source_at(*set->index, 0);
  }
  footprint.identity = model::make_identity("acsd.DetectorFootprint", 2, 0,
                                            model::sha256_hex("acsd.normalize.frame"), "acsd");
  footprint.assessment = model::make_assessment(Code::ok);
  footprint.assessment.complete = true;
  const double width = static_cast<double>(set->width);
  const double height = static_cast<double>(set->height);
  footprint.contraction_center = PixelCenter0{0.5 * (width - 1.0), 0.5 * (height - 1.0)};
  Ring ring;
  ring.hole = false;
  const double corners[4][2] = {{-0.5, -0.5},
                                {width - 0.5, -0.5},
                                {width - 0.5, height - 0.5},
                                {-0.5, height - 0.5}};
  for (const auto& corner : corners) {
    PixelEdge0 edge;
    edge.x = corner[0];
    edge.y = corner[1];
    ring.vertices.push_back(edge);
  }
  footprint.detector_rings.push_back(ring);
  return ok_result(std::shared_ptr<const DetectorFootprint>(
      std::make_shared<DetectorFootprint>(footprint)));
}



using model::fail;
using model::ok_result;


struct FrameOutcome {
  Id frame;
  bool published = false;
  bool unsolved = false;
  bool failed = false;
  bool cancelled = false;
  Code code = Code::ok;
  std::string reason;
  Assessment assessment;
};

// The leaf index of a frame: the leaves of the drizzle order that carry a
// positive support.
Result<Ref<Index>> build_leaf_index(Context& context, const DrizzleResult& drizzle,
                                     const std::vector<HealpixLeaf>& leaves) {
  const std::vector<std::byte> bytes = model::encode_u64([&]() {
    std::vector<U64> ids;
    ids.reserve(leaves.size() * 2);
    for (const HealpixLeaf& leaf : leaves) {
      ids.push_back(leaf.order);
      ids.push_back(leaf.ipix);
    }
    return ids;
  }());
  const std::vector<U64> shape{static_cast<U64>(leaves.size() * 2)};
  Result<ArrayRef> ids = context.storage.copy(context, DType::u64, bytes, shape,
                                              model::unit_dimensionless(), "acsd.leaf.ids");
  if (!ok(ids)) {
    return fail<Ref<Index>>(ids.code, "the leaf identifiers cannot be stored", "normalize",
                            "the frame cannot be published");
  }
  Index index = model::make_leaf_index(leaves);
  index.ids = val(ids);
  auto result = std::make_shared<Index>(index);
  return ok_result(std::shared_ptr<const Index>(result));
}

// The support of the frame: one state per leaf.
Result<Ref<SupportMap>> build_support(Context& context, const std::vector<HealpixLeaf>& leaves,
                                      const std::vector<double>& areas) {
  const std::vector<std::byte> states_bytes = model::encode_u8([&]() {
    std::vector<std::uint8_t> states;
    states.reserve(leaves.size());
    for (std::size_t i = 0; i < leaves.size(); ++i) {
      states.push_back(areas[i] > 0.0 ? 1 : 0);
    }
    return states;
  }());
  const std::vector<std::byte> lower_bytes = model::encode_f64(areas);
  const std::vector<std::byte> upper_bytes = model::encode_f64(areas);
  const std::vector<U64> shape{static_cast<U64>(leaves.size())};
  Result<ArrayRef> states = context.storage.copy(context, DType::u8, states_bytes, shape,
                                                 model::unit_dimensionless(), "acsd.support.states");
  if (!ok(states)) {
    return fail<Ref<SupportMap>>(states.code, "the support states cannot be stored", "normalize",
                                 "the frame cannot be published");
  }
  Result<ArrayRef> lower = context.storage.copy(context, DType::f64, lower_bytes, shape,
                                                 model::unit_sr(), "acsd.support.area");
  Result<ArrayRef> upper = context.storage.copy(context, DType::f64, upper_bytes, shape,
                                                 model::unit_sr(), "acsd.support.area");
  if (!ok(lower) || !ok(upper)) {
    return fail<Ref<SupportMap>>(Code::io_error, "the support areas cannot be stored", "normalize",
                                 "the frame cannot be published");
  }
  Result<Ref<Index>> index = build_leaf_index(context, DrizzleResult(), leaves);
  if (!ok(index)) {
    return fail<Ref<SupportMap>>(index.code, "the leaf index cannot be built", "normalize",
                                 "the frame cannot be published");
  }
  auto support = std::make_shared<SupportMap>();
  support->kind = SupportMap::Kind::primitives;
  support->index = val(index);
  support->states = val(states);
  support->area_lower_sr = val(lower);
  support->area_upper_sr = val(upper);
  support->identity = model::make_identity("acsd.SupportMap", 2, 0,
                                           model::sha256_hex("acsd.normalize.support"), "acsd");
  support->assessment = model::make_assessment(Code::ok);
  support->assessment.complete = true;
  return ok_result(std::shared_ptr<const SupportMap>(support));
}


// A stage that ended with a named termination (the work or the precision
// budget exhausted, the cancellation, an unresolved domain) does not give a
// result.  The code and the reason of the termination are taken from the
// assessment of the stage.
bool stage_terminated(const Assessment& assessment, Code& code, std::string& reason) {
  for (const Issue& issue : assessment.issues) {
    switch (issue.code) {
      case Code::resource_exhausted:
      case Code::precision_exhausted:
      case Code::cancelled:
      case Code::nonconverged:
      case Code::geometry_solve_unresolved:
      case Code::selection_unresolved:
      case Code::support_unresolved:
      case Code::no_support:
      case Code::not_identifiable:
      case Code::unsolved:
      case Code::hash_mismatch:
      case Code::corrupt_product:
      case Code::missing_dependency:
        code = issue.code;
        reason = issue.reason;
        return true;
      default:
        break;
    }
  }
  return false;
}

}  // namespace

Result<NormalizeResult> normalize(Context& context, const CatalogueProvider& catalogue,
                                   const NormalizeRequest& request) {
  NormalizeResult result;
  if (request.frames.size() != request.outputs.size()) {
    return fail<NormalizeResult>(Code::invalid_request,
                                  "the frames and the outputs must have the same length",
                                  "normalize", "no frame was processed");
  }
  std::set<Id> identifiers;
  for (const NormalizeFrame& frame : request.frames) {
    if (!identifiers.insert(frame.frame).second) {
      return fail<NormalizeResult>(Code::identity_conflict,
                                   "the frame " + frame.frame + " is declared twice", "normalize",
                                   "no frame was processed");
    }
    if (!frame.raw) {
      return fail<NormalizeResult>(Code::invalid_request, "a frame has no observation", "normalize",
                                   "no frame was processed");
    }
  }

  // One frozen registry and one command scope for the whole workflow.  The
  // host owns the only pool, ledger and BlockStore; the science bridge of each
  // invocation serves the one existing implementation.
  rt::PoolConfig pool;
  pool.cpu_threads = 2;
  pool.read_slots = 1;
  pool.write_slots = 1;
  pool.process.ceiling.host_bytes = 1ull << 30;
  pool.process.ceiling.cpu_tokens = 2;
  pool.process.max_work = 4000000000ull;
  Result<std::shared_ptr<rt::Services>> services = rt::make_services(pool);
  if (!ok(services)) {
    return fail<NormalizeResult>(services.code == Code::ok ? Code::internal : services.code,
                                 "the workflow runtime cannot be created", "normalize",
                                 "no frame was processed");
  }
  auto runtime = std::make_shared<NormalizeRuntime>();
  runtime->services = value_of(services);

  rt::RegistryBuilder builder;
  Result<Done> registered = generated::register_builtin_types(builder);
  if (!ok(registered)) {
    return fail<NormalizeResult>(registered.code, "the runtime type codecs cannot be registered",
                                 "normalize", "no frame was processed");
  }
  registered = generated::register_processes(builder);
  if (!ok(registered)) {
    return fail<NormalizeResult>(registered.code, "the runtime processes cannot be registered",
                                 "normalize", "no frame was processed");
  }
  Result<std::shared_ptr<const rt::Registry>> frozen = builder.freeze();
  if (!ok(frozen)) {
    return fail<NormalizeResult>(frozen.code, "the runtime registry cannot be frozen", "normalize",
                                 "no frame was processed");
  }
  runtime->registry = value_of(frozen);

  rt::ScopeLimits limits;
  limits.ceiling.host_bytes = 1ull << 30;
  limits.ceiling.cpu_tokens = 2;
  limits.max_work = 4000000000ull;
  Result<InvocationContext> command =
      rt::open_command(runtime->services, "normalize", limits, 1);
  if (!ok(command)) {
    return fail<NormalizeResult>(command.code, "the normalize command scope cannot be opened",
                                 "normalize", "no frame was processed");
  }
  runtime->command = value_of(command);
  Result<std::shared_ptr<rt::ProcessHost>> host =
      rt::make_host(runtime->services, runtime->registry);
  if (!ok(host)) {
    return fail<NormalizeResult>(host.code, "the process host cannot be created", "normalize",
                                 "no frame was processed");
  }
  runtime->host = value_of(host);

  const char* trace_path = std::getenv("ACSD_NORMALIZE_TRACE");
  std::ofstream trace;
  if (trace_path && *trace_path) trace.open(trace_path, std::ios::app);
  const auto record = [&](const std::string& frame, const char* step, const char* process,
                          const char* operation, const Result<rt::InvocationPlan>& plan, Code code,
                          const Id& identity) {
    if (!trace.is_open()) return;
    trace << "{\"frame\":\"" << frame << "\",\"step\":\"" << step << "\",\"process\":\""
          << process << "\",\"operation\":\"" << operation << "\",\"code\":\""
          << code_name(code) << "\"";
    if (plan.value) {
      trace << ",\"r_bytes\":" << plan.value->persistent.host_bytes
            << ",\"f_bytes\":" << plan.value->progress_floor.host_bytes
            << ",\"work\":" << plan.value->elements;
    }
    trace << ",\"plan_code\":\"" << code_name(plan.code) << "\"";
    if (!plan.assessment.issues.empty())
      trace << ",\"plan_reason\":\"" << plan.assessment.issues.front().reason << "\"";
    trace << ",\"identity\":\"" << identity << "\"}\n";
  };

  // Ingests the CLI-owned immutable arrays into the runtime BlockStore.  The
  // bytes keep their identity; no science formula is copied.
  const auto ingest_all = [&](const NormalizeFrame& frame) -> Result<Done> {
    Result<Done> step = ingest_raw(context, *runtime, frame.raw);
    if (!step.value) return step;
    step = ingest_calibration(context, *runtime, frame.calibration);
    if (!step.value) return step;
    step = ingest_response(context, *runtime, frame.response);
    if (!step.value) return step;
    if (frame.supplied_wcs && frame.supplied_wcs->injectivity.preconditioner.size() != 0) {
      // WcsModel has no storage arrays; the preconditioner is a plain matrix.
    }
    return model::ok_result(Done{});
  };

  std::vector<Ref<NativeObservation>> observations;
  std::vector<Ref<SourceMeasurements>> measurements;
  std::vector<FrameOutcome> outcomes(request.frames.size());
  std::vector<Ref<Response>> responses;
  std::vector<Id> photometry_frames;
  std::vector<std::vector<U64>> source_ids_per_frame;

  for (std::size_t i = 0; i < request.frames.size(); ++i) {
    const NormalizeFrame& frame = request.frames[i];
    FrameOutcome& outcome = outcomes[i];
    outcome.frame = frame.frame;
    outcome.assessment = model::make_assessment(Code::ok);
    outcome.assessment.complete = true;
    if (context.cancellation.requested()) {
      outcome.cancelled = true;
      outcome.code = Code::cancelled;
      outcome.reason = "the run was cancelled";
      continue;
    }
    Result<Done> ingested = ingest_all(frame);
    if (!ingested.value) {
      outcome.failed = true;
      outcome.code = ingested.code;
      outcome.reason = ingested.assessment.issues.empty()
                           ? "the input arrays cannot be served to the runtime"
                           : ingested.assessment.issues.front().reason;
      outcome.assessment = ingested.assessment;
      continue;
    }

    // calibrate_observation
    auto calibrate_input =
        std::make_shared<const CalibrateInput>(CalibrateInput{frame.raw, frame.calibration});
    Result<rt::InvocationPlan> plan = plan_calibration(runtime->command, calibrate_input,
                                                       runtime->options);
    Result<Ref<NativeObservation>> calibrated =
        rt::wait_external(runtime->host->invoke<CalibrateInput, Ref<NativeObservation>>(
            runtime->command, "observation.calibrate", "calibrate_observation", calibrate_input));
    record(frame.frame, "calibrate", "observation.calibrate", "calibrate_observation", plan,
           calibrated.code, identity_of(calibrated.value));
    if (!ok(calibrated)) {
      outcome.failed = true;
      outcome.code = calibrated.code;
      outcome.reason = calibrated.assessment.issues.empty()
                           ? "the observation cannot be calibrated"
                           : calibrated.assessment.issues.front().reason;
      outcome.assessment = calibrated.assessment;
      continue;
    }
    Ref<NativeObservation> observation = value_of(calibrated);

    // optional regroup_observation
    if (frame.regroup.has_value()) {
      auto regroup_input = std::make_shared<const RegroupObservationInput>(
          RegroupObservationInput{observation, *frame.regroup});
      Result<rt::InvocationPlan> regroup_plan =
          plan_regroup_op(runtime->command, regroup_input, runtime->options);
      Result<Ref<NativeObservation>> regrouped =
          rt::wait_external(runtime->host->invoke<RegroupObservationInput, Ref<NativeObservation>>(
              runtime->command, "observation.regroup", "regroup_observation", regroup_input));
      record(frame.frame, "regroup", "observation.regroup", "regroup_observation", regroup_plan,
             regrouped.code, identity_of(regrouped.value));
      if (!ok(regrouped)) {
        outcome.failed = true;
        outcome.code = regrouped.code;
        outcome.reason = regrouped.assessment.issues.empty()
                             ? "the observation cannot be regrouped"
                             : regrouped.assessment.issues.front().reason;
        outcome.assessment = regrouped.assessment;
        continue;
      }
      observation = value_of(regrouped);
    }

    // solve_astrometry or supplied WCS
    Ref<WcsModel> wcs = frame.supplied_wcs;
    Ref<AstrometricSolution> solution;
    if (!wcs) {
      Ref<CatalogueProvider> catalogue_ref =
          std::shared_ptr<const CatalogueProvider>(&catalogue, [](const CatalogueProvider*) {});
      auto solve_input = std::make_shared<const SolveAstrometryInput>(
          SolveAstrometryInput{observation, catalogue_ref, request.astrometry});
      Result<rt::InvocationPlan> solve_plan =
          plan_solve_astrometry_op(runtime->command, solve_input, runtime->options);
      Result<Ref<AstrometricSolution>> solved =
          rt::wait_external(runtime->host->invoke<SolveAstrometryInput, Ref<AstrometricSolution>>(
              runtime->command, "sources.astrometry", "solve_astrometry", solve_input));
      record(frame.frame, "solve", "sources.astrometry", "solve_astrometry", solve_plan,
             solved.code, identity_of(solved.value));
      if (!ok(solved)) {
        outcome.unsolved = true;
        outcome.code = solved.code;
        outcome.reason = solved.assessment.issues.empty()
                             ? "the frame has no world coordinate solution"
                             : solved.assessment.issues.front().reason;
        outcome.assessment = solved.assessment;
        continue;
      }
      solution = value_of(solved);
      wcs = solution->wcs;
    } else {
      // A supplied WCS is an immutable input model; no process computes it.
      record(frame.frame, "supplied_wcs", "-", "-", Result<rt::InvocationPlan>{}, Code::ok,
             wcs->identity.object_id);
    }

    // bind_wcs
    auto bind_input =
        std::make_shared<const BindWcsInput>(BindWcsInput{observation, wcs, solution});
    Result<rt::InvocationPlan> bind_plan =
        plan_bind_wcs_op(runtime->command, bind_input, runtime->options);
    Result<Ref<NativeObservation>> bound =
        rt::wait_external(runtime->host->invoke<BindWcsInput, Ref<NativeObservation>>(
            runtime->command, "coordinates.wcs", "bind_wcs", bind_input));
    record(frame.frame, "bind_wcs", "coordinates.wcs", "bind_wcs", bind_plan, bound.code,
           identity_of(bound.value));
    if (!ok(bound)) {
      outcome.failed = true;
      outcome.code = bound.code;
      outcome.reason = bound.assessment.issues.empty()
                           ? "the world coordinate solution cannot be bound"
                           : bound.assessment.issues.front().reason;
      outcome.assessment = bound.assessment;
      continue;
    }
    observation = value_of(bound);

    // The catalogue stars of the field.  query() is a provider method of the
    // storage service (WORKFLOWS: provider methods are not per-call processes);
    // the returned stars are ingested for the typed steps that consume them.
    CatalogueQuery query;
    query.region.center = SkyICRS{wcs->c[0], wcs->c[1], wcs->c[2]};
    const double scale = request.astrometry.scale_rad_pixel.upper > 0.0
                             ? request.astrometry.scale_rad_pixel.upper
                             : 1e-6;
    U64 width = 0;
    U64 height = 0;
    if (observation->footprints) {
      width = observation->footprints->width;
      height = observation->footprints->height;
    }
    const double radius =
        0.5 * std::sqrt(static_cast<double>(width) * static_cast<double>(width) +
                         static_cast<double>(height) * static_cast<double>(height)) *
        scale;
    query.region.radius_rad = radius > 0.0 ? radius : 1e-3;
    query.epoch_julian_year = observation->context.epoch_julian_year;
    query.page_size = 64;
    query.cursor = "";
    Result<CataloguePage> page = catalogue.query(context, query);
    std::vector<CatalogueStar> stars;
    if (ok(page)) stars = value_of(page).stars;
    Result<Done> star_ingest = model::ok_result(Done{});
    for (const CatalogueStar& star : stars) {
      star_ingest = ingest_star(context, *runtime, star);
      if (!star_ingest.value) break;
    }
    if (!star_ingest.value) {
      outcome.failed = true;
      outcome.code = star_ingest.code;
      outcome.reason = "the catalogue stars cannot be served to the runtime";
      outcome.assessment = star_ingest.assessment;
      continue;
    }
    auto star_ref = std::make_shared<const std::vector<CatalogueStar>>(std::move(stars));

    // fit_psf
    auto psf_input = std::make_shared<const FitPsfInput>(
        FitPsfInput{observation, wcs, star_ref, request.psf});
    Result<rt::InvocationPlan> psf_plan =
        plan_fit_psf_op(runtime->command, psf_input, runtime->options);
    Result<Ref<EffectivePSF>> psf =
        rt::wait_external(runtime->host->invoke<FitPsfInput, Ref<EffectivePSF>>(
            runtime->command, "sources.psf", "fit_psf", psf_input));
    record(frame.frame, "fit_psf", "sources.psf", "fit_psf", psf_plan, psf.code,
           identity_of(psf.value));
    if (!ok(psf)) {
      outcome.failed = true;
      outcome.code = psf.code;
      outcome.reason = psf.assessment.issues.empty()
                           ? "the point spread function cannot be fitted"
                           : psf.assessment.issues.front().reason;
      outcome.assessment = psf.assessment;
      continue;
    }

    // measure_science_sources
    auto measure_input = std::make_shared<const MeasureScienceSourcesInput>(
        MeasureScienceSourcesInput{observation, wcs, star_ref, value_of(psf), request.psf});
    Result<rt::InvocationPlan> measure_plan =
        plan_measure_science_sources_op(runtime->command, measure_input, runtime->options);
    Result<Ref<SourceMeasurements>> measured =
        rt::wait_external(runtime->host->invoke<MeasureScienceSourcesInput,
                                                Ref<SourceMeasurements>>(
            runtime->command, "sources.measure", "measure_science_sources", measure_input));
    record(frame.frame, "measure", "sources.measure", "measure_science_sources", measure_plan,
           measured.code, identity_of(measured.value));
    if (!ok(measured)) {
      outcome.failed = true;
      outcome.code = measured.code;
      outcome.reason = measured.assessment.issues.empty()
                           ? "the science sources cannot be measured"
                           : measured.assessment.issues.front().reason;
      outcome.assessment = measured.assessment;
      continue;
    }

    // bind_observation_model
    if (frame.response) {
      Result<Done> response_ingest = ingest_response(context, *runtime, frame.response);
      if (!response_ingest.value) {
        outcome.failed = true;
        outcome.code = response_ingest.code;
        outcome.reason = "the response arrays cannot be served to the runtime";
        outcome.assessment = response_ingest.assessment;
        continue;
      }
    }
    auto model_input = std::make_shared<const BindObservationModelInput>(
        BindObservationModelInput{observation, value_of(psf), frame.response});
    Result<rt::InvocationPlan> model_plan =
        plan_bind_observation_model_op(runtime->command, model_input, runtime->options);
    Result<Ref<NativeObservation>> modelled =
        rt::wait_external(runtime->host->invoke<BindObservationModelInput,
                                                Ref<NativeObservation>>(
            runtime->command, "sources.psf", "bind_observation_model", model_input));
    record(frame.frame, "bind_observation_model", "sources.psf", "bind_observation_model",
           model_plan, modelled.code, identity_of(modelled.value));
    if (!ok(modelled)) {
      outcome.failed = true;
      outcome.code = modelled.code;
      outcome.reason = modelled.assessment.issues.empty()
                           ? "the observation model cannot be bound"
                           : modelled.assessment.issues.front().reason;
      outcome.assessment = modelled.assessment;
      continue;
    }
    observation = value_of(modelled);

    observations.push_back(observation);
    measurements.push_back(value_of(measured));
    if (frame.response) {
      responses.push_back(frame.response);
      photometry_frames.push_back(frame.frame);
    }
    std::vector<U64> ids;
    for (const StarFlux& star : value_of(measured)->stars) ids.push_back(star.gaia_source_id);
    source_ids_per_frame.push_back(ids);
  }

  // Phase two: the joint photometry over the frames that produced a
  // measurement.  Every response is integrated once per catalogue basis.
  std::vector<Ref<PhotoSolution>> solutions(measurements.size());
  if (!measurements.empty() && !request.photometry.frames.empty()) {
    std::vector<U64> all_ids;
    for (const std::vector<U64>& ids : source_ids_per_frame) {
      for (U64 id : ids) all_ids.push_back(id);
    }
    std::sort(all_ids.begin(), all_ids.end());
    all_ids.erase(std::unique(all_ids.begin(), all_ids.end()), all_ids.end());
    // catalogue.lookup is a provider method of the storage service (WORKFLOWS:
    // provider methods are not per-call process nodes).  Keeping it on the
    // workflow Context is also what makes the returned stars carry their XP
    // coefficient arrays; a Host-invoked lookup would have to re-read the
    // provider's private column arrays through the science bridge.
    Result<std::vector<CatalogueStar>> stars_result = catalogue.lookup(context, all_ids);
    record("all", "catalogue_lookup", "catalogue.access", "catalogue.lookup",
           Result<rt::InvocationPlan>{}, stars_result.code, Id{});
    if (!ok(stars_result)) {
      return fail<NormalizeResult>(stars_result.code, "the catalogue cannot be read", "normalize",
                                    "no frame was published");
    }
    std::vector<CatalogueStar> stars = value_of(stars_result);
    std::size_t xp_present = 0;
    std::size_t xp_block = 0;
    for (const CatalogueStar& star : stars) {
      if (star.xp_coefficients) ++xp_present;
      Result<Done> star_ingest = ingest_star(context, *runtime, star);
      if (!star_ingest.value) {
        return fail<NormalizeResult>(star_ingest.code, "the catalogue stars cannot be served",
                                      "normalize", "no frame was published");
      }
      if (star.xp_coefficients &&
          runtime->services->blocks->find(star.xp_coefficients->identity.object_id))
        ++xp_block;
    }
    if (trace.is_open())
      trace << "{\"step\":\"catalogue_lookup\",\"code\":\"ok\",\"xp_present\":" << xp_present
            << ",\"xp_blocks\":" << xp_block << ",\"stars\":" << stars.size() << "}\n";
    const Ref<SpectralBasis> basis = stars.empty() ? nullptr : stars.front().xp_basis;
    std::vector<Ref<SpectralPrediction>> predictions;
    for (const Ref<Response>& response : responses) {
      if (!basis) break;
      Result<Done> response_ingest = ingest_response(context, *runtime, response);
      if (!response_ingest.value) {
        return fail<NormalizeResult>(response_ingest.code, "the response arrays cannot be served",
                                      "normalize", "no frame was published");
      }
      Result<Done> basis_ingest = ingest_basis(context, *runtime, basis);
      if (!basis_ingest.value) {
        return fail<NormalizeResult>(basis_ingest.code, "the basis arrays cannot be served",
                                      "normalize", "no frame was published");
      }
      NumericBudget budget;
      budget.tolerance.absolute = 1e-12;
      budget.tolerance.absolute_floor = 1e-12;
      budget.tolerance.unit = model::unit_dimensionless();
      budget.max_iterations = 200;
      budget.arithmetic = Arithmetic{Arithmetic::Mode::outward, 64, 256, true};
      auto integrate_input = std::make_shared<const IntegrateResponseInput>(
          IntegrateResponseInput{response, basis, budget});
      Result<rt::InvocationPlan> integrate_plan =
          plan_integrate_response_op(runtime->command, integrate_input, runtime->options);
      Result<Ref<SpectralPrediction>> prediction =
          rt::wait_external(runtime->host->invoke<IntegrateResponseInput,
                                                  Ref<SpectralPrediction>>(
              runtime->command, "response.integrate", "integrate_response", integrate_input));
      record("all", "integrate_response", "response.integrate", "integrate_response",
             integrate_plan, prediction.code, identity_of(prediction.value));
      if (!prediction.value) {
        for (FrameOutcome& outcome : outcomes) {
          if (outcome.failed || outcome.unsolved || outcome.cancelled) continue;
          outcome.failed = true;
          outcome.code = prediction.code;
          outcome.reason = prediction.assessment.issues.empty()
                               ? "the response cannot be integrated"
                               : prediction.assessment.issues.front().reason;
          outcome.assessment = prediction.assessment;
        }
        break;
      }
      predictions.push_back(value_of(prediction));
    }
    if (responses.size() != measurements.size()) {
      // A measured frame without a response cannot enter the joint fit; the
      // fit request stays consistent instead of reporting a length conflict.
      for (FrameOutcome& outcome : outcomes) {
        if (outcome.failed || outcome.unsolved || outcome.cancelled) continue;
        outcome.failed = true;
        outcome.code = Code::unavailable;
        outcome.reason = "a measured frame has no response for the joint photometry";
      }
    } else {
    PhotometryRequest fit_request = request.photometry;
    fit_request.frames = photometry_frames;
    fit_request.responses = responses;
    auto measurements_ref =
        std::make_shared<const std::vector<Ref<SourceMeasurements>>>(measurements);
    auto stars_ref = std::make_shared<const std::vector<CatalogueStar>>(stars);
    auto predictions_ref =
        std::make_shared<const std::vector<Ref<SpectralPrediction>>>(predictions);
    for (const Ref<Response>& response : request.photometry.responses) {
      Result<Done> response_ingest = ingest_response(context, *runtime, response);
      if (!response_ingest.value) {
        return fail<NormalizeResult>(response_ingest.code, "the photometry response cannot be served",
                                      "normalize", "no frame was published");
      }
    }
    if (request.photometry.reference) {
      Result<Done> reference_ingest =
          ingest_spectrum(context, *runtime, request.photometry.reference->spectrum);
      if (!reference_ingest.value) {
        return fail<NormalizeResult>(reference_ingest.code,
                                      "the reference spectrum cannot be served", "normalize",
                                      "no frame was published");
      }
      Result<Done> target_ingest = ingest_response(
          context, *runtime, request.photometry.reference->target_response);
      if (!target_ingest.value) {
        return fail<NormalizeResult>(target_ingest.code,
                                      "the reference response cannot be served", "normalize",
                                      "no frame was published");
      }
    }
    if (request.photometry.basis.weighted_mean) {
      Result<Done> basis_ingest = ingest_array(
          context, *runtime, request.photometry.basis.weighted_mean);
      if (!basis_ingest.value) {
        return fail<NormalizeResult>(basis_ingest.code,
                                      "the photometry basis cannot be served", "normalize",
                                      "no frame was published");
      }
    }
    auto fit_input = std::make_shared<const FitPhotometryInput>(
        FitPhotometryInput{measurements_ref, stars_ref, predictions_ref, fit_request});
    Result<rt::InvocationPlan> fit_plan =
        plan_photometry_fit(runtime->command, fit_input, runtime->options);
    Result<Ref<PhotoSolution>> solution =
        rt::wait_external(runtime->host->invoke<FitPhotometryInput, Ref<PhotoSolution>>(
            runtime->command, "photometry.fit", "fit_photometry", fit_input));
    record("all", "fit_photometry", "photometry.fit", "fit_photometry", fit_plan, solution.code,
           identity_of(solution.value));
    if (!ok(solution)) {
      for (FrameOutcome& outcome : outcomes) {
        if (outcome.failed || outcome.unsolved || outcome.cancelled) continue;
        outcome.failed = true;
        outcome.code = solution.code;
        outcome.reason = solution.assessment.issues.empty()
                             ? "the joint photometry failed"
                             : solution.assessment.issues.front().reason;
        outcome.assessment = solution.assessment;
      }
    } else {
      for (std::size_t i = 0; i < measurements.size(); ++i) solutions[i] = value_of(solution);
    }
    }
  }

  // Phase three: geometry, information and publication.
  std::size_t measured_index = 0;
  for (std::size_t i = 0; i < request.frames.size(); ++i) {
    FrameOutcome& outcome = outcomes[i];
    if (outcome.failed || outcome.unsolved || outcome.cancelled) {
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    Ref<NativeObservation> observation = observations[measured_index];
    Ref<PhotoSolution> solution = solutions[measured_index];
    ++measured_index;
    if (solution) {
      auto scale_input = std::make_shared<const ApplyScaleInput>(
          ApplyScaleInput{solution, observation, request.frames[i].frame});
      Result<rt::InvocationPlan> scale_plan =
          plan_photometry_apply(runtime->command, scale_input, runtime->options);
      Result<Ref<NativeObservation>> scaled =
          rt::wait_external(runtime->host->invoke<ApplyScaleInput, Ref<NativeObservation>>(
              runtime->command, "photometry.apply", "apply_scale", scale_input));
      record(request.frames[i].frame, "apply_scale", "photometry.apply", "apply_scale", scale_plan,
             scaled.code, identity_of(scaled.value));
      if (!ok(scaled)) {
        outcome.failed = true;
        outcome.code = scaled.code;
        outcome.reason = scaled.assessment.issues.empty()
                             ? "the photometric scale cannot be applied"
                             : scaled.assessment.issues.front().reason;
        outcome.assessment = scaled.assessment;
        result.frames.emplace_back(outcome.frame, outcome.assessment);
        continue;
      }
      observation = value_of(scaled);
    } else if (!request.allow_relative_uncalibrated) {
      outcome.failed = true;
      outcome.code = Code::unavailable;
      outcome.reason = "the frame has no photometric scale";
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }

    if (!observation->footprints || !observation->footprints->wcs) {
      outcome.failed = true;
      outcome.code = Code::missing_dependency;
      outcome.reason = "the frame has no footprint";
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }

    // make_footprint_provider
    auto provider_input = std::make_shared<const MakeFootprintProviderInput>(
        MakeFootprintProviderInput{observation->footprints->wcs});
    Result<rt::InvocationPlan> provider_plan = plan_geometry_overlap_make_footprint_provider(
        runtime->command, provider_input, runtime->options);
    Result<Ref<FootprintProvider>> provider =
        rt::wait_external(runtime->host->invoke<MakeFootprintProviderInput,
                                                Ref<FootprintProvider>>(
            runtime->command, "geometry.overlap", "make_footprint_provider", provider_input));
    record(request.frames[i].frame, "make_footprint_provider", "geometry.overlap",
           "make_footprint_provider", provider_plan, provider.code,
           identity_of(provider.value));
    if (!ok(provider)) {
      outcome.failed = true;
      outcome.code = provider.code;
      outcome.reason = "the frame has no footprint provider";
      outcome.assessment = provider.assessment;
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }

    U64 block_size = 1;
    if (const char* override_size = std::getenv("ACSD_DRIZZLE_BLOCK")) {
      const long parsed = std::atol(override_size);
      if (parsed > 0 && parsed <= 1024) block_size = static_cast<U64>(parsed);
    }
    std::vector<OverlapContribution> contributions;
    U64 source_rows = 0;
    bool geometry_failed = false;
    if (observation->footprints->width > 0 && observation->footprints->height > 0) {
      const U64 width = observation->footprints->width;
      const U64 height = observation->footprints->height;
      const U64 blocks_per_row = (width + block_size - 1) / block_size;
      const U64 block_count = blocks_per_row * ((height + block_size - 1) / block_size);
      Result<Done> footprint_ingest = ingest_footprints(
          context, *runtime, observation->footprints);
      if (!footprint_ingest.value) {
        outcome.failed = true;
        outcome.code = footprint_ingest.code;
        outcome.reason = "the footprint arrays cannot be served to the runtime";
        outcome.assessment = footprint_ingest.assessment;
        result.frames.emplace_back(outcome.frame, outcome.assessment);
        geometry_failed = true;
      }
      for (U64 block = 0; !geometry_failed && block < block_count; ++block) {
        if (context.cancellation.requested()) {
          outcome.failed = true;
          outcome.code = Code::cancelled;
          outcome.reason = "the export was cancelled";
          geometry_failed = true;
          break;
        }
        Result<Ref<DetectorFootprint>> footprint =
            block_footprint(context, observation->footprints, block, block_size, block);
        if (!footprint.value) {
          outcome.failed = true;
          outcome.code = footprint.code;
          outcome.reason = footprint.assessment.issues.empty()
                               ? "the detector footprint cannot be built"
                               : footprint.assessment.issues.front().reason;
          outcome.assessment = footprint.assessment;
          geometry_failed = true;
          break;
        }
        auto overlap_input = std::make_shared<const ComputeOverlapInput>(
            ComputeOverlapInput{value_of(provider), value_of(footprint),
                                request.drizzle.order, request.drizzle.pixfrac,
                                request.drizzle.budget});
        Result<rt::InvocationPlan> overlap_plan = plan_geometry_overlap_compute_overlap(
            runtime->command, overlap_input, runtime->options);
        Result<Ref<Overlaps>> overlaps =
            rt::wait_external(runtime->host->invoke<ComputeOverlapInput, Ref<Overlaps>>(
                runtime->command, "geometry.overlap", "compute_overlap", overlap_input));
        record(request.frames[i].frame, "compute_overlap", "geometry.overlap", "compute_overlap",
               overlap_plan, overlaps.code, identity_of(overlaps.value));
        if (ok(overlaps) &&
            value_of(overlaps)->status.termination != Termination::tolerance_met) {
          const Termination termination = value_of(overlaps)->status.termination;
          outcome.failed = true;
          outcome.code = termination == Termination::work      ? Code::resource_exhausted
                          : termination == Termination::precision ? Code::precision_exhausted
                                                                 : Code::unsupported;
          outcome.reason = "the geometry did not reach the requested tolerance";
          result.frames.emplace_back(outcome.frame, outcome.assessment);
          geometry_failed = true;
          break;
        }
        if (!ok(overlaps)) {
          outcome.failed = true;
          outcome.code = overlaps.code;
          outcome.reason = overlaps.assessment.issues.empty()
                               ? "the overlap cannot be computed"
                               : overlaps.assessment.issues.front().reason;
          outcome.assessment = overlaps.assessment;
          geometry_failed = true;
          break;
        }
        for (const OverlapContribution& contribution : value_of(overlaps)->contributions)
          contributions.push_back(contribution);
        ++source_rows;
      }
    }
    if (geometry_failed) {
      if (result.frames.empty() || result.frames.back().first != outcome.frame)
        result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    auto merged = std::make_shared<Overlaps>();
    merged->contributions = std::move(contributions);
    merged->identity = model::make_identity("acsd.Overlaps", 2, 0,
                                            model::sha256_hex("acsd.normalize.overlaps"), "acsd");
    merged->assessment = model::make_assessment(Code::ok);
    merged->assessment.complete = true;
    merged->assessment.included.push_back(
        block_size == 1
            ? "the source rows are the pixels of the detector: " + std::to_string(source_rows) +
                  " source rows for " + std::to_string(source_rows) + " pixels"
            : "the source rows are the blocks of the detector: " + std::to_string(source_rows) +
                  " blocks of " + std::to_string(block_size) + " by " + std::to_string(block_size) +
                  " pixels");

    auto drizzle_input = std::make_shared<const BuildDrizzleInput>(
        BuildDrizzleInput{merged, observation, request.drizzle});
    Result<rt::InvocationPlan> drizzle_plan = plan_geometry_drizzle_build_drizzle(
        runtime->command, drizzle_input, runtime->options);
    Result<DrizzleResult> drizzle =
        rt::wait_external(runtime->host->invoke<BuildDrizzleInput, DrizzleResult>(
            runtime->command, "geometry.drizzle", "build_drizzle", drizzle_input));
    record(request.frames[i].frame, "build_drizzle", "geometry.drizzle", "build_drizzle",
           drizzle_plan, drizzle.code, identity_of(drizzle.value));
    if (!ok(drizzle)) {
      outcome.failed = true;
      outcome.code = drizzle.code;
      outcome.reason = drizzle.assessment.issues.empty()
                           ? "the drizzle operator cannot be built"
                           : drizzle.assessment.issues.front().reason;
      outcome.assessment = drizzle.assessment;
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    {
      Code termination = Code::ok;
      std::string reason;
      if (stage_terminated(value_of(drizzle).op ? value_of(drizzle).op->assessment
                                                : value_of(drizzle).estimate->assessment,
                           termination, reason)) {
        outcome.failed = true;
        outcome.code = termination;
        outcome.reason = reason;
        result.frames.emplace_back(outcome.frame, outcome.assessment);
        continue;
      }
    }

    // information.reference (a named unavailable estimate is an issue on the
    // frame, not a fabricated variance).
    if (!request.information.empty()) {
      InformationRequest information;
      information.reference = request.information.front().reference;
      information.solve = request.information.front().solve;
      Result<Done> information_ingest = ingest_information(context, *runtime, information);
      if (!information_ingest.value) {
        model::add_issue(outcome.assessment, information_ingest.code,
                         "the information request cannot be served to the runtime", "normalize",
                         "the frame loses its information estimate");
      } else {
        auto information_input = std::make_shared<const EstimateFluxInformationInput>(
            EstimateFluxInformationInput{information});
        Result<rt::InvocationPlan> information_plan = plan_information_reference(
            runtime->command, information_input, runtime->options);
        Result<Information> info =
            rt::wait_external(runtime->host->invoke<EstimateFluxInformationInput, Information>(
                runtime->command, "information.reference", "estimate_flux_information",
                information_input));
        record(request.frames[i].frame, "information", "information.reference",
               "estimate_flux_information", information_plan, info.code, Id{});
        if (!ok(info)) {
          model::add_issue(outcome.assessment, info.code,
                           info.assessment.issues.empty()
                               ? "the information cannot be estimated"
                               : info.assessment.issues.front().reason,
                           "normalize", "the frame loses its information estimate");
        }
      }
    }

    auto graph = std::make_shared<ProductGraph>();
    graph->stage = ProductGraph::Stage::normalize;
    graph->signal = value_of(drizzle).estimate;
    graph->profile = request.outputs[i].profile.empty() ? "full" : request.outputs[i].profile;
    graph->assessment = value_of(drizzle).estimate->assessment;
    graph->identity = model::make_identity(
        "acsd.ProductGraph", 2, 0,
        model::sha256_hex("acsd.normalize:" + request.frames[i].frame), "acsd");
    WorkflowStorage storage(context.storage, runtime->services);
    Context publish_context{context.resources, context.cancellation, storage};
    Result<ProductLocation> published = publish_product(publish_context, graph, request.outputs[i]);
    if (!ok(published)) {
      outcome.failed = true;
      outcome.code = published.code;
      outcome.reason = published.assessment.issues.empty()
                           ? "the frame cannot be published"
                           : published.assessment.issues.front().reason;
      outcome.assessment = published.assessment;
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    outcome.published = true;
    outcome.assessment = value_of(drizzle).estimate->assessment;
    outcome.assessment.complete = true;
    result.published.push_back(value_of(published));
    result.frames.emplace_back(outcome.frame, outcome.assessment);
  }

  result.published.clear();
  return ok_result(result);
}

}  // namespace acsd
