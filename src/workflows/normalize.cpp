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
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "workflows/product_internal.hpp"


namespace acsd {
namespace {

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

}  // namespace

Result<NormalizeResult> normalize(Context& context, const CatalogueProvider& catalogue,
                                   const NormalizeRequest& request) {
  NormalizeResult result;
#if !ACSD_HAVE_OBSERVATION_BINDING
  // The observation module does not bind the world coordinate solution and the
  // observation model yet.  The capability is declared as unsupported instead
  // of being a link error: the marker is set by the build from the sources of
  // the module.
  return fail<NormalizeResult>(
      Code::unsupported,
      "the observation module does not provide the binding of the world coordinate solution and "
      "of the observation model",
      "normalize", "no frame was processed");
#endif
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

  // Phase one: the observation model of every frame.
  std::vector<Ref<NativeObservation>> observations;
  std::vector<Ref<SourceMeasurements>> measurements;
  std::vector<FrameOutcome> outcomes(request.frames.size());
  std::vector<Ref<Response>> responses;
  std::vector<std::vector<U64>> source_ids_per_frame;

  for (std::size_t i = 0; i < request.frames.size(); ++i) {
    const NormalizeFrame& frame = request.frames[i];
    FrameOutcome& outcome = outcomes[i];
    outcome.frame = frame.frame;
    if (context.cancellation.requested()) {
      outcome.cancelled = true;
      outcome.code = Code::cancelled;
      outcome.reason = "the run was cancelled";
      continue;
    }
    // The calibration.
    Result<Ref<NativeObservation>> calibrated =
        calibrate_observation(context, frame.raw, frame.calibration);
    if (!ok(calibrated)) {
      outcome.failed = true;
      outcome.code = calibrated.code;
      outcome.reason = calibrated.assessment.issues.empty()
                           ? "the observation cannot be calibrated"
                           : calibrated.assessment.issues.front().reason;
      outcome.assessment = calibrated.assessment;
      continue;
    }
    Ref<NativeObservation> observation = val(calibrated);
    // The optional physical regroup.
    if (frame.regroup.has_value()) {
      Result<Ref<NativeObservation>> regrouped =
          regroup_observation(context, observation, *frame.regroup);
      if (!ok(regrouped)) {
        outcome.failed = true;
        outcome.code = regrouped.code;
        outcome.reason = regrouped.assessment.issues.empty()
                             ? "the observation cannot be regrouped"
                             : regrouped.assessment.issues.front().reason;
        outcome.assessment = regrouped.assessment;
        continue;
      }
      observation = val(regrouped);
    }
    // The world coordinate solution: the supplied header or the blind solve.
    Ref<WcsModel> wcs = frame.supplied_wcs;
    Ref<AstrometricSolution> solution;
    if (!wcs) {
      Result<Ref<AstrometricSolution>> solved =
          solve_astrometry(context, observation, catalogue, request.astrometry);
      if (!ok(solved)) {
        outcome.unsolved = true;
        outcome.code = solved.code;
        outcome.reason = solved.assessment.issues.empty()
                             ? "the frame has no world coordinate solution"
                             : solved.assessment.issues.front().reason;
        outcome.assessment = solved.assessment;
        continue;
      }
      solution = val(solved);
      wcs = val(solved)->wcs;
    }
#if ACSD_HAVE_OBSERVATION_BINDING
    Result<Ref<NativeObservation>> bound = bind_wcs(context, observation, wcs, solution);
    if (!ok(bound)) {
      outcome.failed = true;
      outcome.code = bound.code;
      outcome.reason = bound.assessment.issues.empty()
                           ? "the world coordinate solution cannot be bound"
                           : bound.assessment.issues.front().reason;
      outcome.assessment = bound.assessment;
      continue;
    }
    observation = val(bound);
#else
    outcome.failed = true;
    outcome.code = Code::unsupported;
    outcome.reason =
        "the observation module does not bind the world coordinate solution to the observation";
    result.frames.emplace_back(outcome.frame, outcome.assessment);
    continue;
#endif
    // The catalogue stars of the field: the candidates of the region are
    // taken from the catalogue and used for the point spread function and
    // for the source measurement.
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
    if (ok(page)) stars = val(page).stars;
    // The PSF and the measurement use the same star set.
    Result<Ref<EffectivePSF>> psf =
        fit_psf(context, observation, wcs, std::span<const CatalogueStar>(stars), request.psf);
    if (!ok(psf)) {
      outcome.failed = true;
      outcome.code = psf.code;
      outcome.reason = psf.assessment.issues.empty()
                           ? "the point spread function cannot be fitted"
                           : psf.assessment.issues.front().reason;
      outcome.assessment = psf.assessment;
      continue;
    }
    Result<Ref<SourceMeasurements>> measured =
        measure_science_sources(context, observation, wcs, std::span<const CatalogueStar>(stars),
                                 val(psf), request.psf);
    if (!ok(measured)) {
      outcome.failed = true;
      outcome.code = measured.code;
      outcome.reason = measured.assessment.issues.empty()
                           ? "the science sources cannot be measured"
                           : measured.assessment.issues.front().reason;
      outcome.assessment = measured.assessment;
      continue;
    }
#if ACSD_HAVE_OBSERVATION_BINDING
    // The point spread function and the response are bound to the
    // observation: the stochastic roots and the influence of the model are
    // kept.
    Result<Ref<NativeObservation>> modelled =
        bind_observation_model(context, observation, val(psf), frame.response);
    if (!ok(modelled)) {
      outcome.failed = true;
      outcome.code = modelled.code;
      outcome.reason = modelled.assessment.issues.empty()
                           ? "the observation model cannot be bound"
                           : modelled.assessment.issues.front().reason;
      outcome.assessment = modelled.assessment;
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    observation = val(modelled);
#endif
    observations.push_back(observation);
    measurements.push_back(val(measured));
    if (frame.response) responses.push_back(frame.response);
    std::vector<U64> ids;
    for (const StarFlux& star : val(measured)->stars) ids.push_back(star.gaia_source_id);
    source_ids_per_frame.push_back(ids);
  }

  // Phase two: the joint photometry over the frames that produced a
  // measurement.
  std::vector<Ref<PhotoSolution>> solutions(measurements.size());
  if (!measurements.empty() && !request.photometry.frames.empty()) {
    std::vector<U64> all_ids;
    for (const std::vector<U64>& ids : source_ids_per_frame) {
      for (U64 id : ids) all_ids.push_back(id);
    }
    std::sort(all_ids.begin(), all_ids.end());
    all_ids.erase(std::unique(all_ids.begin(), all_ids.end()), all_ids.end());
    Result<std::vector<CatalogueStar>> stars = catalogue.lookup(context, all_ids);
    if (!ok(stars)) {
      return fail<NormalizeResult>(stars.code, "the catalogue cannot be read", "normalize",
                                    "no frame was published");
    }
    std::vector<Ref<SpectralPrediction>> predictions;
    const SpectralBasis* basis = val(stars).empty() ? nullptr : val(stars).front().xp_basis.get();
    for (const Ref<Response>& response : responses) {
      if (!basis) break;
      NumericBudget budget;
      budget.tolerance.absolute = 1e-12;
      budget.tolerance.absolute_floor = 1e-12;
      budget.tolerance.unit = model::unit_dimensionless();
      budget.max_iterations = 200;
      budget.arithmetic = Arithmetic{Arithmetic::Mode::outward, 64, 256, true};
      Result<Ref<SpectralPrediction>> prediction =
          integrate_response(context, response, std::shared_ptr<const SpectralBasis>(basis), budget);
      if (!ok(prediction)) {
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
      predictions.push_back(val(prediction));
    }
    Result<Ref<PhotoSolution>> solution =
        fit_photometry(context, measurements, val(stars), predictions, request.photometry);
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
      for (std::size_t i = 0; i < measurements.size(); ++i) solutions[i] = val(solution);
    }
  }

  // Phase three: the geometry, the information and the publication.
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
    // The scale of the frame.
    if (solution) {
      Result<Ref<NativeObservation>> scaled =
          apply_scale(context, solution, observation, request.frames[i].frame);
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
      observation = val(scaled);
    } else if (!request.allow_relative_uncalibrated) {
      outcome.failed = true;
      outcome.code = Code::unavailable;
      outcome.reason = "the frame has no photometric scale";
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    // The geometry: every source of the frame against the leaves of the
    // drizzle order.  The pieces of one leaf are merged before the operator
    // is built.
    if (!observation->footprints || !observation->footprints->wcs) {
      outcome.failed = true;
      outcome.code = Code::missing_dependency;
      outcome.reason = "the frame has no footprint";
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    NumericBudget budget;
    budget.tolerance.absolute = 1e-12;
    budget.tolerance.absolute_floor = 1e-12;
    budget.tolerance.unit = model::unit_dimensionless();
    budget.max_iterations = 200;
    budget.arithmetic = request.drizzle.budget.arithmetic;
    Result<Ref<FootprintProvider>> provider =
        make_footprint_provider(context, observation->footprints->wcs);
    if (!ok(provider)) {
      outcome.failed = true;
      outcome.code = provider.code;
      outcome.reason = "the frame has no footprint provider";
      outcome.assessment = provider.assessment;
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    Ref<Overlaps> merged;
    std::map<std::pair<U32, U64>, double> leaf_areas;
    std::vector<HealpixLeaf> leaves;
    U64 source_count = observation->native_index ? observation->native_index->size : 0;
    for (U64 row = 0; row < source_count; ++row) {
      Result<Ref<DetectorFootprint>> footprint =
          detector_footprint(context, observation->footprints, row);
      if (!ok(footprint)) {
        outcome.failed = true;
        outcome.code = footprint.code;
        outcome.reason = footprint.assessment.issues.empty()
                             ? "the detector footprint cannot be read"
                             : footprint.assessment.issues.front().reason;
        outcome.assessment = footprint.assessment;
        break;
      }
      Result<Ref<Overlaps>> overlaps = compute_overlap(context, *val(provider), val(footprint),
                                                        request.drizzle.order, request.drizzle.pixfrac,
                                                        request.drizzle.budget);
      if (!ok(overlaps)) {
        outcome.failed = true;
        outcome.code = overlaps.code;
        outcome.reason = overlaps.assessment.issues.empty()
                             ? "the overlap cannot be computed"
                             : overlaps.assessment.issues.front().reason;
        outcome.assessment = overlaps.assessment;
        break;
      }
      merged = val(overlaps);
      for (const OverlapContribution& contribution : val(overlaps)->contributions) {
        const auto key = std::make_pair(contribution.leaf.order, contribution.leaf.ipix);
        auto it = leaf_areas.find(key);
        if (it == leaf_areas.end()) {
          leaf_areas[key] = contribution.overlap_area.lower_sr;
          leaves.push_back(contribution.leaf);
        } else {
          it->second += contribution.overlap_area.lower_sr;
        }
      }
    }
    if (outcome.failed) {
      result.frames.emplace_back(outcome.frame, outcome.assessment);
      continue;
    }
    Result<DrizzleResult> drizzle = build_drizzle(context, merged, observation, request.drizzle);
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
    // The information of the frame.
    if (!request.information.empty()) {
      InformationRequest information;
      information.reference = request.information.front().reference;
      information.solve = request.information.front().solve;
      Result<Information> info = estimate_flux_information(context, information);
      if (!ok(info)) {
        model::add_issue(outcome.assessment, info.code,
                         info.assessment.issues.empty()
                             ? "the information cannot be estimated"
                             : info.assessment.issues.front().reason,
                         "normalize", "the frame loses its information estimate");
      }
    }
    // The publication of the frame.
    auto graph = std::make_shared<ProductGraph>();
    graph->stage = ProductGraph::Stage::normalize;
    graph->signal = val(drizzle).estimate;
    graph->profile = request.outputs[i].profile.empty() ? "full" : request.outputs[i].profile;
    graph->assessment = val(drizzle).estimate->assessment;
    graph->identity = model::make_identity("acsd.ProductGraph", 2, 0,
                                            model::sha256_hex("acsd.normalize:" + request.frames[i].frame), "acsd");
    Result<ProductLocation> published = publish_product(context, graph, request.outputs[i]);
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
    result.published.push_back(val(published));
    result.frames.emplace_back(outcome.frame, outcome.assessment);
  }

  result.published.clear();
  return ok_result(result);
}

}  // namespace acsd
