// The mosaic workflow.
//
// The inputs are opened first: a corrupted input fails the whole mosaic, no
// frame is silently dropped.  The frames of the products are merged into one
// provider, the relative sky is fitted and the coaddition produces the
// signal HiPS of the mosaic.
#include <algorithm>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include <acsd/modules/catalogue.hpp>
#include <acsd/modules/coadd.hpp>
#include <acsd/modules/generated/status.hpp>
#include <acsd/modules/sky.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include "operators/linear_action.hpp"
#include "workflows/normalize_runtime.hpp"
#include <nlohmann/json.hpp>

namespace acsd {
namespace {

using model::fail;
using model::ok_result;
using rt::Done;

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

}  // namespace

Result<ProductLocation> mosaic(Context& context, const MosaicRequest& request) {
  if (request.inputs.empty()) {
    return fail<ProductLocation>(Code::invalid_request, "the mosaic has no input", "mosaic",
                                 "no product was published");
  }
  // The inputs are opened before anything is computed.
  std::vector<Ref<ProductProvider>> products;
  for (const ProductLocation& location : request.inputs) {
    // The cancellation is checked at the frame boundary: the mosaic opens
    // every input and then publishes one product.
    if (context.cancellation.requested()) {
      return fail<ProductLocation>(Code::cancelled, "the mosaic was cancelled", "mosaic",
                                   "no product was published");
    }
  Result<Ref<ProductProvider>> product = open_product(context, location);
    if (!ok(product)) {
      return fail<ProductLocation>(product.code,
                                   product.assessment.issues.empty()
                                       ? "an input product cannot be opened"
                                       : product.assessment.issues.front().reason,
                                   "mosaic", "no product was published");
    }
    products.push_back(val(product));
  }

  // The frames of the products: the same order, no repeated source.
  std::vector<Ref<FrameDescription>> descriptions;
  std::vector<ArrayRef> signals;
  std::vector<ArrayRef> dq;
  std::vector<Ref<SignalEstimate>> estimates;
  U32 order = 0;
  bool first = true;
  std::set<std::pair<U32, U64>> leaves;
  for (const Ref<ProductProvider>& product : products) {
    Result<std::vector<Id>> frames = product->list_frames(context);
    if (!ok(frames)) {
      return fail<ProductLocation>(frames.code, "the frames of an input cannot be listed", "mosaic",
                                   "no product was published");
    }
    Ref<ProductGraph> product_graph = product->graph();
    if (!product_graph || !product_graph->signal) {
      return fail<ProductLocation>(Code::invalid_model,
                                   "an input product carries no signal estimate", "mosaic",
                                   "no product was published");
    }
    // apply_sky is a per-signal correction.  A product that carries several
    // frames would need one estimate per frame; the current Normalize product
    // carries exactly one, so anything else is refused instead of guessing.
    if (val(frames).size() != 1) {
      return fail<ProductLocation>(Code::unsupported,
                                   "the mosaic expects one signal estimate per input product", "mosaic",
                                   "no product was published");
    }
    for (const Id& frame : val(frames)) {
      Result<Ref<FrameDescription>> description = product->describe_frame(context, frame);
      if (!ok(description)) {
        return fail<ProductLocation>(description.code, "a frame cannot be described", "mosaic",
                                     "no product was published");
      }
      if (val(description)->order == 0 && val(description)->index) {
        // The leaf order of the frame.
      }
      if (first) {
        order = val(description)->order;
        first = false;
      } else if (val(description)->order != order) {
        return fail<ProductLocation>(Code::invalid_model,
                                     "the inputs do not share the leaf order", "mosaic",
                                     "no product was published");
      }
      // The repeated leaves of two products are the same random variable: the
      // frames are kept, the covariance is merged later by the shared index.
      if (val(description)->index && val(description)->index->ids) {
        Result<ArrayView> view = context.storage.read(context, val(description)->index->ids,
                                                      BlockRequest{0, val(description)->index->size * 2});
        if (ok(view)) {
          const std::vector<U64> ids = model::decode_u64(val(view).bytes);
          for (std::size_t i = 0; i + 1 < ids.size(); i += 2) {
            leaves.insert({static_cast<U32>(ids[i]), ids[i + 1]});
          }
        }
      }
      Result<LeafBlock> block = product->read_leaf_block(context, FrameReadRequest{frame, {}, {}, {}});
      if (!ok(block)) {
        return fail<ProductLocation>(block.code, "a leaf block cannot be read", "mosaic",
                                     "no product was published");
      }
      descriptions.push_back(val(description));
      signals.push_back(val(block).signal);
      dq.push_back(val(block).dq);
      estimates.push_back(product_graph->signal);
    }
  }
  if (descriptions.empty()) {
    return fail<ProductLocation>(Code::invalid_model, "the inputs carry no frame", "mosaic",
                                 "no product was published");
  }

  // The relative sky and the coaddition, through the typed operations of one
  // frozen registry.  The frames provider is a storage service; its arrays are
  // served from the runtime BlockStore.
  Result<std::shared_ptr<workflows::WorkflowRuntime>> runtime =
      workflows::open_workflow_runtime("mosaic");
  if (!runtime.value) {
    return fail<ProductLocation>(runtime.code, "the workflow runtime cannot be created", "mosaic",
                                 "no product was published");
  }
  workflows::WorkflowRuntime& workflow_runtime = **runtime.value;
  Result<Done> ingested = workflows::ingest_frame_inputs(
      context, workflow_runtime, descriptions, signals, dq);
  if (!ingested.value) {
    return fail<ProductLocation>(ingested.code,
                                 "the frame arrays cannot be served to the runtime", "mosaic",
                                 "no product was published");
  }
  const char* trace_path = std::getenv("ACSD_MOSAIC_TRACE");
  std::ofstream trace;
  if (trace_path && *trace_path) trace.open(trace_path, std::ios::app);
  const auto record = [&](const char* step, const char* process, const char* operation,
                          const Result<rt::InvocationPlan>& plan, Code code) {
    if (!trace.is_open()) return;
    trace << "{\"step\":\"" << step << "\",\"process\":\"" << process
          << "\",\"operation\":\"" << operation << "\",\"code\":\"" << code_name(code)
          << "\",\"plan_code\":\"" << code_name(plan.code) << "\"";
    if (plan.value) {
      trace << ",\"r_bytes\":" << plan.value->persistent.host_bytes
            << ",\"f_bytes\":" << plan.value->progress_floor.host_bytes
            << ",\"work\":" << plan.value->elements;
    }
    trace << "}\n";
  };

  // The night-to-night multiplicative scale, measured on the star sample that
  // each Normalize product carries in photometry.json (the fitted fluxes of the
  // same Gaia sources), is APPLIED here: every corrected frame is divided by
  // its scale relative to frame 0 before the coadd.  Without it the equal-weight
  // coadd is an average of three frames whose effective scale varies with the
  // coverage (the second independent S1 review measured 1.1506/0.9416, a 15%
  // step at the K=1/K=2 boundaries).  The applied values and their p16..p84 are
  // named in the mosaic assessment; a frame without a usable sample keeps 1 and
  // the omission is named.
  std::vector<double> night_scale(estimates.size(), 1.0);
  std::vector<double> night_scale_p16(estimates.size(), 1.0);
  std::vector<double> night_scale_p84(estimates.size(), 1.0);
  std::vector<std::size_t> night_scale_stars(estimates.size(), 0);
  std::vector<std::string> scale_notes;
  // The scale is applied to the frames BEFORE the relative-sky fit: the sky
  // model assumes a common scene, and fitting it on the raw frames let the
  // multiplicative scale (1.226/0.963) be absorbed as an additive offset,
  // leaving a scene-proportional S(1/g-1) residual (independent S1 review
  // round 3: f1 67% / f2 34% of the median level).  Scaled first, the fit
  // sees the common scene and estimates the true airglow offsets.
  std::vector<ArrayRef> scaled_signals = signals;
  if (estimates.size() > 1) {
    std::vector<std::map<U64, double>> star_flux(estimates.size());
    bool have_reference = false;
    for (std::size_t f = 0; f < estimates.size(); ++f) {
      const std::string path = request.inputs[f].directory + "/photometry.json";
      std::ifstream stream(path, std::ios::binary);
      if (!stream) {
        scale_notes.push_back("frame " + std::to_string(f) +
                              " has no photometry.json; its scale is not measured");
        continue;
      }
      nlohmann::json doc;
      try {
        stream >> doc;
      } catch (const std::exception&) {
        scale_notes.push_back("frame " + std::to_string(f) +
                              " photometry.json is not valid JSON; its scale is not measured");
        continue;
      }
      for (const auto& star : doc.value("stars", nlohmann::json::array())) {
        const double flux = star.value("flux", 0.0);
        if (!(flux > 0.0) || !std::isfinite(flux)) continue;
        star_flux[f][star.value("id", static_cast<U64>(0))] = flux;
      }
      if (f == 0) have_reference = !star_flux[0].empty();
    }
    if (have_reference) {
      for (std::size_t f = 1; f < estimates.size(); ++f) {
        std::vector<double> ratios;
        for (const auto& entry : star_flux[0]) {
          const auto found = star_flux[f].find(entry.first);
          if (found == star_flux[f].end() || !(found->second > 0.0)) continue;
          ratios.push_back(found->second / entry.second);
        }
        if (ratios.size() < 3) {
          scale_notes.push_back("frame " + std::to_string(f) + "/frame 0 has only " +
                                std::to_string(ratios.size()) +
                                " common stars; its scale keeps 1");
          continue;
        }
        std::sort(ratios.begin(), ratios.end());
        night_scale[f] = ratios[ratios.size() / 2];
        night_scale_p16[f] = ratios[ratios.size() * 16 / 100];
        night_scale_p84[f] = ratios[ratios.size() * 84 / 100];
        night_scale_stars[f] = ratios.size();
        if (!(night_scale[f] > 0.0) || !std::isfinite(night_scale[f])) night_scale[f] = 1.0;
      }
      // Apply: divide the input signal of every measured frame by its scale.
      workflows::WorkflowStorage scale_storage(context.storage, workflow_runtime.services);
      Context scale_read_context{context.resources, context.cancellation, scale_storage};
      for (std::size_t f = 1; f < estimates.size(); ++f) {
        if (std::fabs(night_scale[f] - 1.0) <= 1e-12) continue;
        const U64 count = signals[f]->shape.empty() ? 0 : signals[f]->shape[0];
        Result<std::vector<double>> values =
            operators::read_f64(scale_read_context, signals[f], 0, count);
        if (!ok(values)) {
          return fail<ProductLocation>(values.code, "the corrected signal cannot be read for the "
                                                    "relative scale",
                                       "mosaic", "no product was published");
        }
        std::vector<double> scaled_values = val(values);
        for (double& value : scaled_values) {
          if (std::isfinite(value)) value /= night_scale[f];
        }
        Result<ArrayRef> scaled = operators::write_f64(
            context, scaled_values, signals[f]->shape, estimates[f]->unit,
            signals[f]->index_id);
        if (!ok(scaled)) {
          return fail<ProductLocation>(scaled.code, "the scaled signal cannot be stored",
                                       "mosaic", "no product was published");
        }
        scaled_signals[f] = val(scaled);
      }
    } else {
      scale_notes.push_back("frame 0 has no usable star sample; no relative scale is applied");
    }
  }

  // The scaled signals are the frames the sky fit and the coadd see.  The
  // estimates are copied because apply_sky is a per-signal operation and the
  // original product estimates stay untouched.
  std::vector<Ref<SignalEstimate>> scaled_estimates;
  for (std::size_t f = 0; f < estimates.size(); ++f) {
    auto copy = std::make_shared<SignalEstimate>(*estimates[f]);
    copy->signal = scaled_signals[f];
    scaled_estimates.push_back(copy);
    Result<Done> ingested_scaled =
        workflows::ingest_array(context, workflow_runtime, scaled_signals[f]);
    if (!ingested_scaled.value) {
      return fail<ProductLocation>(ingested_scaled.code,
                                   "a scaled signal cannot be served to the runtime", "mosaic",
                                   "no product was published");
    }
  }

  Result<Ref<FramesProvider>> frames = make_frames_provider(context, descriptions, scaled_signals, dq);
  if (!ok(frames)) {
    return fail<ProductLocation>(frames.code,
                                 frames.assessment.issues.empty()
                                     ? "the frames cannot be merged"
                                     : frames.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }

  // Without an explicit T/H the process document builds the default 32x32
  // face-chart patch inside a comparability group.  The workflow supplies such
  // a group only when the frames actually share the response and the spatial
  // kernel (their identities are the proofs); otherwise comparability is not
  // established and the mosaic stops with the named reason instead of
  // fabricating a group.
  SkyRequest sky_request = request.sky;
  if (sky_request.groups.empty() && !sky_request.T && !sky_request.H) {
    Comparability group;
    bool comparable = !descriptions.empty();
    Id response_proof;
    Id kernel_proof;
    for (const Ref<FrameDescription>& description : descriptions) {
      if (!description->response) {
        comparable = false;
        break;
      }
      const Id response = description->response->identity.object_id;
      const Id kernel = description->psf ? description->psf->identity.object_id : response;
      if (response.empty() || kernel.empty()) {
        comparable = false;
        break;
      }
      if (group.frames.empty()) {
        response_proof = response;
        kernel_proof = kernel;
      } else if (response != response_proof || kernel != kernel_proof) {
        comparable = false;
        break;
      }
      group.frames.push_back(description->frame);
    }
    if (!comparable || group.frames.empty()) {
      return fail<ProductLocation>(
          Code::invalid_request,
          "the default comparability group cannot be established: the frames do not "
          "share one response and one spatial kernel",
          "mosaic", "no product was published");
    }
    group.response_proof = response_proof;
    group.spatial_kernel_proof = kernel_proof;
    group.matching = nullptr;
    group.assumptions.kind = "same_response_and_kernel_identity";
    group.assumptions.fixed_objects = {response_proof, kernel_proof};
    sky_request.groups.push_back(std::move(group));
  }
  auto controls_input = std::make_shared<const modules::BuildSkyControlsInput>(
      modules::BuildSkyControlsInput{val(frames), sky_request});
  Result<rt::InvocationPlan> controls_plan = modules::plan_sky_controls(
      workflow_runtime.command, controls_input, workflow_runtime.options);
  Result<Ref<SkyControls>> controls = rt::wait_external(
      workflow_runtime.host->invoke<modules::BuildSkyControlsInput, Ref<SkyControls>>(
          workflow_runtime.command, "sky.controls", "build_sky_controls", controls_input));
  record("sky_controls", "sky.controls", "build_sky_controls", controls_plan, controls.code);
  if (!ok(controls)) {
    return fail<ProductLocation>(controls.code, controls.assessment.issues.empty()
                                                     ? "the sky controls cannot be built"
                                                     : controls.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }
  auto sky_input = std::make_shared<const modules::FitRelativeSkyInput>(
      modules::FitRelativeSkyInput{val(controls), sky_request});
  Result<rt::InvocationPlan> sky_plan = modules::plan_sky_fit(
      workflow_runtime.command, sky_input, workflow_runtime.options);
  Result<Ref<SkyFit>> sky = rt::wait_external(
      workflow_runtime.host->invoke<modules::FitRelativeSkyInput, Ref<SkyFit>>(
          workflow_runtime.command, "sky.fit", "fit_relative_sky", sky_input));
  record("sky_fit", "sky.fit", "fit_relative_sky", sky_plan, sky.code);
  if (!ok(sky)) {
    return fail<ProductLocation>(sky.code, sky.assessment.issues.empty()
                                                ? "the relative sky cannot be fitted"
                                                : sky.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }
  // The fitted relative sky is subtracted exactly once, here, before the
  // selection and the weight solve.  The coaddition only carries the frozen
  // fit; it must never apply it a second time.
  for (const Ref<SignalEstimate>& estimate : scaled_estimates) {
    Result<Done> ingested_estimate = workflows::ingest_signal(context, workflow_runtime, estimate);
    if (!ingested_estimate.value) {
      return fail<ProductLocation>(ingested_estimate.code,
                                   "a frame estimate cannot be served to the runtime", "mosaic",
                                   "no product was published");
    }
  }
  std::vector<Ref<FrameDescription>> corrected_descriptions;
  std::vector<ArrayRef> corrected_signals;
  std::vector<ArrayRef> unscaled_corrected;
  for (std::size_t i = 0; i < estimates.size(); ++i) {
    // Pass the frame identity: in a deep mosaic several frames observe the
    // same leaf, so the training-space match alone is ambiguous.
    auto apply_input = std::make_shared<const modules::ApplySkyInput>(
        modules::ApplySkyInput{scaled_estimates[i], val(sky), descriptions[i]->frame});
    Result<rt::InvocationPlan> apply_plan = modules::plan_sky_apply(
        workflow_runtime.command, apply_input, workflow_runtime.options);
    Result<Ref<SignalEstimate>> applied = rt::wait_external(
        workflow_runtime.host->invoke<modules::ApplySkyInput, Ref<SignalEstimate>>(
            workflow_runtime.command, "sky.fit", "apply_sky", apply_input));
    record("sky_apply", "sky.fit", "apply_sky", apply_plan, applied.code);
    if (!ok(applied)) {
      return fail<ProductLocation>(applied.code,
                                   applied.assessment.issues.empty()
                                       ? "the relative sky cannot be applied"
                                       : applied.assessment.issues.front().reason,
                                   "mosaic", "no product was published");
    }
    auto description = std::make_shared<FrameDescription>(*descriptions[i]);
    if (val(applied)->noise) description->noise = val(applied)->noise;
    if (val(applied)->effective_response) description->response = val(applied)->effective_response;
    corrected_descriptions.push_back(description);
    corrected_signals.push_back(val(applied)->signal);
    // The additive sky correction is the difference of the two arrays; keep
    // the pre-scale corrected array so the per-leaf offset can be stored.
    unscaled_corrected.push_back(val(applied)->signal);
  }
  Result<Done> corrected_ingest = workflows::ingest_frame_inputs(
      context, workflow_runtime, corrected_descriptions, corrected_signals, dq);
  if (!corrected_ingest.value) {
    return fail<ProductLocation>(corrected_ingest.code,
                                 "the corrected frame arrays cannot be served to the runtime",
                                 "mosaic", "no product was published");
  }
  Result<Ref<FramesProvider>> corrected_frames =
      make_frames_provider(context, corrected_descriptions, corrected_signals, dq);
  if (!ok(corrected_frames)) {
    return fail<ProductLocation>(corrected_frames.code,
                                 corrected_frames.assessment.issues.empty()
                                     ? "the corrected frames cannot be merged"
                                     : corrected_frames.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }
  frames = std::move(corrected_frames);
  descriptions = std::move(corrected_descriptions);
  // The comparability groups are the same scientific object for the sky and
  // the coaddition; the workflow supplies the group it verified above.
  CoaddRequest coadd_request = request.coaddition;
  coadd_request.groups = sky_request.groups;
  // The coadded signal keeps the frames' physical surface-brightness unit: the
  // per-frame declared scale embeds the frame identity, so the common unit is
  // the shared linear/sr/pixel/nm dimension with a neutral scale.  Without
  // this the unknown control covariance made the coadd fall back to
  // scale="unknown" and the export wrote a bogus ACSUNIT="unknown^sr-1^L1".
  {
    Unit common;
    bool first = true;
    bool agree = true;
    for (const Ref<SignalEstimate>& estimate : estimates) {
      if (!estimate) {
        agree = false;
        break;
      }
      const Unit& unit = estimate->unit;
      if (first) {
        common = unit;
        first = false;
      } else if (unit.linear_power != common.linear_power || unit.sr_power != common.sr_power ||
                 unit.pixel_power != common.pixel_power || unit.nm_power != common.nm_power ||
                 unit.multiplier != common.multiplier) {
        agree = false;
        break;
      }
    }
    if (agree && !first && coadd_request.target.unit.scale == "1" &&
        coadd_request.target.unit.sr_power == 0 && coadd_request.target.unit.pixel_power == 0 &&
        coadd_request.target.unit.nm_power == 0 && coadd_request.target.unit.linear_power == 0) {
      common.scale = "1";
      coadd_request.target.unit = common;
    }
  }
  auto select_input = std::make_shared<const modules::SelectSamplesInput>(
      modules::SelectSamplesInput{val(frames), coadd_request});
  Result<rt::InvocationPlan> select_plan = modules::plan_coadd_select(
      workflow_runtime.command, select_input, workflow_runtime.options);
  Result<Ref<Selection>> selection = rt::wait_external(
      workflow_runtime.host->invoke<modules::SelectSamplesInput, Ref<Selection>>(
          workflow_runtime.command, "coadd.select", "select_samples", select_input));
  record("coadd_select", "coadd.select", "select_samples", select_plan, selection.code);
  if (!ok(selection)) {
    return fail<ProductLocation>(selection.code, selection.assessment.issues.empty()
                                                       ? "the samples cannot be selected"
                                                       : selection.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }
  auto weights_input = std::make_shared<const modules::SolveWeightsInput>(
      modules::SolveWeightsInput{val(frames), val(selection), coadd_request});
  Result<rt::InvocationPlan> weights_plan = modules::plan_coadd_weights(
      workflow_runtime.command, weights_input, workflow_runtime.options);
  Result<Ref<WeightSolution>> weights = rt::wait_external(
      workflow_runtime.host->invoke<modules::SolveWeightsInput, Ref<WeightSolution>>(
          workflow_runtime.command, "coadd.weights", "solve_weights", weights_input));
  record("coadd_weights", "coadd.weights", "solve_weights", weights_plan, weights.code);
  if (!ok(weights)) {
    return fail<ProductLocation>(weights.code, weights.assessment.issues.empty()
                                                     ? "the weights cannot be solved"
                                                     : weights.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }
  auto coadd_input = std::make_shared<const modules::CoaddInput>(
      modules::CoaddInput{val(frames), val(sky), val(selection), val(weights),
                          coadd_request});
  Result<rt::InvocationPlan> coadd_plan = modules::plan_coadd_integrate(
      workflow_runtime.command, coadd_input, workflow_runtime.options);
  Result<CoaddResult> coadd_result = rt::wait_external(
      workflow_runtime.host->invoke<modules::CoaddInput, CoaddResult>(
          workflow_runtime.command, "coadd.integrate", "coadd", coadd_input));
  record("coadd_integrate", "coadd.integrate", "coadd", coadd_plan, coadd_result.code);
  if (!ok(coadd_result)) {
    return fail<ProductLocation>(coadd_result.code,
                                 coadd_result.assessment.issues.empty()
                                     ? "the coaddition failed"
                                     : coadd_result.assessment.issues.front().reason,
                                 "mosaic", "no product was published");
  }

  // The per-leaf additive sky correction and the applied scales are stored in
  // the product: with coadd_coefficients they make the combined signal exactly
  // reproducible from the product (the second review asked for the SkyFit and
  // WeightSolution in the closure; this is the numeric form the reader uses).
  Ref<ArrayRef> sky_offsets_holder;  // (unused; the refs are graph fields)
  (void)sky_offsets_holder;
  ArrayRef sky_offsets_array;
  ArrayRef relative_scales_array;
  ArrayRef sky_beta_ordered_array;
  // The measured frame-to-mosaic ratio in the 0.3-3x median level band: the coadd is
  // expressed in frame 0's star scale, but the residual spatial multiplicative
  // term (master-flat mismatch between nights) makes it non-common; the
  // numbers are named in the assessment.
  std::vector<double> faint_ratio_median(estimates.size(), 1.0);
  std::vector<double> faint_ratio_p16(estimates.size(), 1.0);
  std::vector<double> faint_ratio_p84(estimates.size(), 1.0);
  bool faint_ratio_measured = false;
  // Pairwise corrected-frame ratios (f vs frame 0) in the same level band.
  // The frame-vs-mosaic ratio is self-referential (the mosaic contains the
  // frame) and compressed; the pairwise ratio is the honest amplitude
  // (independent S1 review, round 5).
  std::vector<double> pair_ratio_median(estimates.size(), 1.0);
  std::vector<double> pair_ratio_p16(estimates.size(), 1.0);
  std::vector<double> pair_ratio_p84(estimates.size(), 1.0);
  bool pair_ratio_measured = false;
  // 8x8 spatial cells over the NESTED leaf order (Hilbert-contiguous segments
  // of the selection index): the min/max of the cell medians is the measured
  // spatial amplitude of the frame-to-frame multiplicative residual.
  std::vector<double> spatial_min(estimates.size(), 1.0);
  std::vector<double> spatial_max(estimates.size(), 1.0);
  bool spatial_measured = false;
  if (!estimates.empty() && coadd_result.value && val(coadd_result).selection &&
      val(coadd_result).selection->index) {
    const Ref<Index> sample_index = val(coadd_result).selection->index;
    workflows::WorkflowStorage read_storage(context.storage, workflow_runtime.services);
    Context read_context{context.resources, context.cancellation, read_storage};
    Result<ArrayView> sample_ids_view = read_storage.read(
        read_context, sample_index->ids, BlockRequest{0, sample_index->size * 2});
    if (ok(sample_ids_view)) {
      const std::vector<U64> sample_ids = model::decode_u64(val(sample_ids_view).bytes);
      const std::size_t samples = sample_ids.size() / 2;
      const std::size_t frames_count = estimates.size();
      // The coadd indexes its frames in the order it lists them (the frame ids
      // of the comparability group, sorted); the coefficients are in that
      // order, not in the input order.  The stored offsets and scales must use
      // the same order, otherwise the reopened combination mis-assigns frames
      // on the leaves where the frame sets differ (measured: 148 leaves).
      std::vector<std::size_t> position_of_input(frames_count);
      for (std::size_t i = 0; i < frames_count; ++i) position_of_input[i] = i;
      if (!sky_request.groups.empty() &&
          sky_request.groups.front().frames.size() == frames_count) {
        const std::vector<Id>& group_frames = sky_request.groups.front().frames;
        std::stable_sort(position_of_input.begin(), position_of_input.end(),
                         [&](std::size_t a, std::size_t b) {
                           return group_frames[a] < group_frames[b];
                         });
        std::vector<std::size_t> rank(frames_count);
        for (std::size_t pos = 0; pos < frames_count; ++pos) {
          rank[position_of_input[pos]] = pos;
        }
        position_of_input = rank;
      }
      std::vector<double> offsets(samples * frames_count, 0.0);
      for (std::size_t f = 0; f < frames_count; ++f) {
        if (!scaled_estimates[f] || !scaled_estimates[f]->signal || !unscaled_corrected[f] ||
            !descriptions[f] || !descriptions[f]->index) {
          continue;
        }
        Result<std::vector<double>> original =
            operators::read_f64(read_context, scaled_estimates[f]->signal, 0,
                                scaled_estimates[f]->signal->shape.empty()
                                    ? 0
                                    : scaled_estimates[f]->signal->shape[0]);
        Result<std::vector<double>> corrected =
            operators::read_f64(read_context, unscaled_corrected[f], 0,
                                unscaled_corrected[f]->shape.empty()
                                    ? 0
                                    : unscaled_corrected[f]->shape[0]);
        Result<ArrayView> frame_ids_view = read_storage.read(
            read_context, descriptions[f]->index->ids,
            BlockRequest{0, descriptions[f]->index->size * 2});
        if (!ok(original) || !ok(corrected) || !ok(frame_ids_view)) continue;
        const std::vector<U64> frame_ids = model::decode_u64(val(frame_ids_view).bytes);
        std::map<std::pair<U32, U64>, double> offset_of_leaf;
        const std::size_t rows = std::min({val(original).size(), val(corrected).size(),
                                           frame_ids.size() / 2});
        for (std::size_t r = 0; r < rows; ++r) {
          const double value = val(original)[r] - val(corrected)[r];
          if (!std::isfinite(value)) continue;
          offset_of_leaf[{static_cast<U32>(frame_ids[2 * r]),
                          frame_ids[2 * r + 1]}] = value;
        }
        for (std::size_t sel = 0; sel < samples; ++sel) {
          const auto found = offset_of_leaf.find(
              {static_cast<U32>(sample_ids[2 * sel]), sample_ids[2 * sel + 1]});
          if (found != offset_of_leaf.end()) {
            offsets[sel * frames_count + position_of_input[f]] = found->second;
          }
        }
      }
      Result<ArrayRef> stored_offsets = operators::write_f64(
          context, offsets, std::vector<U64>{static_cast<U64>(samples),
                                             static_cast<U64>(frames_count)},
          model::unit_dimensionless(), Id());
      if (ok(stored_offsets)) sky_offsets_array = val(stored_offsets);
      std::vector<double> scales(night_scale.begin(), night_scale.end());
      for (std::size_t f = 0; f < scales.size(); ++f) {
        scales[position_of_input[f]] = night_scale[f];
      }
      Result<ArrayRef> stored_scales = operators::write_f64(
          context, scales, std::vector<U64>{static_cast<U64>(scales.size())},
          model::unit_dimensionless(), Id());
      if (ok(stored_scales)) relative_scales_array = val(stored_scales);
      // Measure the frame-to-mosaic ratio in the 0.3-3x median level band.  A common
      // coadd would give 1.0 for every frame; the measured p16..p84 is the
      // named amplitude of the spatial multiplicative residual.
      Result<std::vector<double>> mosaic_values = operators::read_f64(
          read_context, val(coadd_result).estimate->signal, 0,
          val(coadd_result).estimate->signal->shape.empty()
              ? 0
              : val(coadd_result).estimate->signal->shape[0]);
      if (ok(mosaic_values) && !val(mosaic_values).empty()) {
        std::vector<double> finite_mosaic;
        for (double value : val(mosaic_values)) {
          if (std::isfinite(value)) finite_mosaic.push_back(value);
        }
        std::vector<std::vector<double>> ratios(frames_count);
        if (!finite_mosaic.empty()) {
          std::sort(finite_mosaic.begin(), finite_mosaic.end());
          const double median = finite_mosaic[finite_mosaic.size() / 2];
          const double lower = 0.3 * std::fabs(median);
          const double upper = 3.0 * std::fabs(median);
          for (std::size_t f = 0; f < frames_count; ++f) {
            if (!unscaled_corrected[f] || !descriptions[f] || !descriptions[f]->index) continue;
            Result<std::vector<double>> values = operators::read_f64(
                read_context, unscaled_corrected[f], 0,
                unscaled_corrected[f]->shape.empty() ? 0 : unscaled_corrected[f]->shape[0]);
            Result<ArrayView> ids_view = read_storage.read(
                read_context, descriptions[f]->index->ids,
                BlockRequest{0, descriptions[f]->index->size * 2});
            if (!ok(values) || !ok(ids_view)) continue;
            const std::vector<U64> ids = model::decode_u64(val(ids_view).bytes);
            std::map<std::pair<U32, U64>, double> corrected_of_leaf;
            const std::size_t rows = std::min(val(values).size(), ids.size() / 2);
            for (std::size_t r = 0; r < rows; ++r) {
              corrected_of_leaf[{static_cast<U32>(ids[2 * r]), ids[2 * r + 1]}] = val(values)[r];
            }
            for (std::size_t p = 0; p < samples && p < val(mosaic_values).size(); ++p) {
              const double m = val(mosaic_values)[p];
              if (!std::isfinite(m) || m < lower || m > upper) continue;
              const auto found = corrected_of_leaf.find(
                  {static_cast<U32>(sample_ids[2 * p]), sample_ids[2 * p + 1]});
              if (found == corrected_of_leaf.end() || !std::isfinite(found->second)) continue;
              ratios[f].push_back(found->second / m);
            }
          }
          for (std::size_t f = 0; f < frames_count; ++f) {
            if (ratios[f].size() < 100) continue;
            std::sort(ratios[f].begin(), ratios[f].end());
            faint_ratio_median[f] = ratios[f][ratios[f].size() / 2];
            faint_ratio_p16[f] = ratios[f][ratios[f].size() * 16 / 100];
            faint_ratio_p84[f] = ratios[f][ratios[f].size() * 84 / 100];
            faint_ratio_measured = true;
          }
          // Pairwise corrected-frame ratios over the same band.  Build the
          // per-frame corrected maps once.
          std::vector<std::map<std::pair<U32, U64>, double>> corrected_maps(frames_count);
          for (std::size_t f = 0; f < frames_count; ++f) {
            if (!unscaled_corrected[f] || !descriptions[f] || !descriptions[f]->index) continue;
            Result<std::vector<double>> values = operators::read_f64(
                read_context, unscaled_corrected[f], 0,
                unscaled_corrected[f]->shape.empty() ? 0 : unscaled_corrected[f]->shape[0]);
            Result<ArrayView> ids_view = read_storage.read(
                read_context, descriptions[f]->index->ids,
                BlockRequest{0, descriptions[f]->index->size * 2});
            if (!ok(values) || !ok(ids_view)) continue;
            const std::vector<U64> ids = model::decode_u64(val(ids_view).bytes);
            const std::size_t rows = std::min(val(values).size(), ids.size() / 2);
            for (std::size_t r = 0; r < rows; ++r) {
              corrected_maps[f][{static_cast<U32>(ids[2 * r]), ids[2 * r + 1]}] = val(values)[r];
            }
          }
          std::vector<std::vector<double>> pair_ratios(frames_count);
          std::vector<std::vector<std::vector<double>>> cell_ratios(frames_count);
          for (std::size_t f = 0; f < frames_count; ++f) cell_ratios[f].resize(64);
          for (std::size_t p = 0; p < samples && p < val(mosaic_values).size(); ++p) {
            const double m = val(mosaic_values)[p];
            if (!std::isfinite(m) || std::fabs(m) < lower || std::fabs(m) > upper) continue;
            const std::pair<U32, U64> leaf{static_cast<U32>(sample_ids[2 * p]),
                                           sample_ids[2 * p + 1]};
            const auto reference = corrected_maps[0].find(leaf);
            if (reference == corrected_maps[0].end() || !std::isfinite(reference->second) ||
                reference->second == 0.0) {
              continue;
            }
            const std::size_t cell = samples == 0 ? 0 : (p * 64) / samples;
            for (std::size_t f = 1; f < frames_count; ++f) {
              const auto found = corrected_maps[f].find(leaf);
              if (found == corrected_maps[f].end() || !std::isfinite(found->second)) continue;
              const double ratio = found->second / reference->second;
              pair_ratios[f].push_back(ratio);
              if (cell < cell_ratios[f].size()) cell_ratios[f][cell].push_back(ratio);
            }
          }
          for (std::size_t f = 1; f < frames_count; ++f) {
            if (pair_ratios[f].size() < 100) continue;
            std::sort(pair_ratios[f].begin(), pair_ratios[f].end());
            pair_ratio_median[f] = pair_ratios[f][pair_ratios[f].size() / 2];
            pair_ratio_p16[f] = pair_ratios[f][pair_ratios[f].size() * 16 / 100];
            pair_ratio_p84[f] = pair_ratios[f][pair_ratios[f].size() * 84 / 100];
            pair_ratio_measured = true;
            std::vector<double> cells;
            for (const auto& cell : cell_ratios[f]) {
              if (cell.size() < 30) continue;
              std::vector<double> sorted = cell;
              std::sort(sorted.begin(), sorted.end());
              cells.push_back(sorted[sorted.size() / 2]);
            }
            if (cells.size() >= 8) {
              spatial_min[f] = *std::min_element(cells.begin(), cells.end());
              spatial_max[f] = *std::max_element(cells.begin(), cells.end());
              spatial_measured = true;
            }
          }
        }
      }
      // The additive offsets are stored in the same coadd frame order as the
      // weights and the per-leaf offsets (the former beta array was in the
      // input order).
      if (ok(sky) && val(sky) && val(sky)->beta) {
        Result<std::vector<double>> beta = operators::read_f64(
            read_context, val(sky)->beta, 0,
            val(sky)->beta->shape.empty() ? 0 : val(sky)->beta->shape[0]);
        if (ok(beta) && val(beta).size() == frames_count) {
          std::vector<double> ordered(val(beta).size(), 0.0);
          for (std::size_t f = 0; f < frames_count; ++f) {
            ordered[position_of_input[f]] = val(beta)[f];
          }
          Result<ArrayRef> stored_beta = operators::write_f64(
              context, ordered, val(sky)->beta->shape, model::unit_dimensionless(), Id());
          if (ok(stored_beta)) sky_beta_ordered_array = val(stored_beta);
        }
      }
    }
  }
  // Seam / low-frequency diagnostic in the product itself: for adjacent
  // leaves with different frame coverage K the combined value steps by the
  // unremoved flat-screen residual.  Signed median with MAD and the dim
  // brightness band (independent S1 review, round 6, S1.5-2 form).
  std::ostringstream seam_note;
  bool seam_measured = false;
  if (coadd_result.value && val(coadd_result).estimate && val(coadd_result).estimate->signal &&
      ok(weights) && val(weights) && val(weights)->coefficients) {
    workflows::WorkflowStorage seam_storage(context.storage, workflow_runtime.services);
    Context seam_context{context.resources, context.cancellation, seam_storage};
    // read_f64's count is the number of *flattened* elements; using shape[0]
    // read only the first third of a [leaves,3] array and the diagnostic
    // silently covered 1/3 of the support (independent S1 review, round 7).
    U64 coef_count = 1;
    for (U64 extent : val(weights)->coefficients->shape) coef_count *= extent;
    Result<std::vector<double>> coef_values = operators::read_f64(
        seam_context, val(weights)->coefficients, 0, coef_count);
    if (ok(coef_values) && val(coef_values).size() != coef_count) {
      return fail<ProductLocation>(Code::invalid_model,
                                   "the coefficient array read is truncated", "mosaic",
                                   "no product was published");
    }
    Result<std::vector<double>> signal_values = operators::read_f64(
        seam_context, val(coadd_result).estimate->signal, 0,
        val(coadd_result).estimate->signal->shape.empty()
            ? 0
            : val(coadd_result).estimate->signal->shape[0]);
    Ref<Index> samples =
        coadd_result.value && val(coadd_result).selection ? val(coadd_result).selection->index
                                                          : Ref<Index>{};
    if (ok(coef_values) && ok(signal_values) && samples && samples->ids) {
      Result<ArrayView> ids_view = seam_storage.read(
          seam_context, samples->ids, BlockRequest{0, samples->size * 2});
      if (ok(ids_view)) {
        const std::vector<U64> ids = model::decode_u64(val(ids_view).bytes);
        const std::size_t count = std::min({ids.size() / 2, val(signal_values).size(),
                                            val(coef_values).size() / 3});
        std::vector<int> coverage(count, 0);
        std::vector<double> level(count, 0.0);
        std::vector<double> finite_levels;
        for (std::size_t i = 0; i < count; ++i) {
          int k = 0;
          for (int f = 0; f < 3; ++f) {
            if (val(coef_values)[i * 3 + f] != 0.0) ++k;
          }
          coverage[i] = k;
          level[i] = val(signal_values)[i];
          if (std::isfinite(level[i])) finite_levels.push_back(level[i]);
        }
        double median_level = 0.0;
        if (!finite_levels.empty()) {
          std::sort(finite_levels.begin(), finite_levels.end());
          median_level = finite_levels[finite_levels.size() / 2];
        }
        std::vector<double> interior, boundary13, boundary23, interior_dim;
        std::vector<double> boundary13_dim, boundary23_dim;
        for (std::size_t i = 0; i + 1 < count; ++i) {
          if (ids[2 * i] != ids[2 * i + 2]) continue;
          if (ids[2 * i + 1] + 1 != ids[2 * i + 3]) continue;
          const double a = level[i];
          const double b = level[i + 1];
          if (!std::isfinite(a) || !std::isfinite(b) || a + b == 0.0) continue;
          const double delta = (b - a) / (0.5 * (std::fabs(a) + std::fabs(b)));
          const int ka = coverage[i];
          const int kb = coverage[i + 1];
          if (ka == 3 && kb == 3) {
            interior.push_back(delta);
            const double mean = 0.5 * (std::fabs(a) + std::fabs(b));
            if (median_level > 0 && mean >= 0.9 * median_level && mean <= 1.3 * median_level) {
              interior_dim.push_back(delta);
            }
          } else if ((ka == 1 && kb == 3) || (ka == 3 && kb == 1)) {
            boundary13.push_back(delta);
            const double mean = 0.5 * (std::fabs(a) + std::fabs(b));
            if (median_level > 0 && mean >= 0.9 * median_level && mean <= 1.3 * median_level) {
              boundary13_dim.push_back(delta);
            }
          } else if ((ka == 2 && kb == 3) || (ka == 3 && kb == 2)) {
            boundary23.push_back(delta);
            const double mean = 0.5 * (std::fabs(a) + std::fabs(b));
            if (median_level > 0 && mean >= 0.9 * median_level && mean <= 1.3 * median_level) {
              boundary23_dim.push_back(delta);
            }
          }
        }
        auto stats = [](std::vector<double> values, double& median, double& mad, double& absolute) {
          median = 0.0;
          mad = 0.0;
          absolute = 0.0;
          if (values.empty()) return;
          std::sort(values.begin(), values.end());
          median = values[values.size() / 2];
          std::vector<double> deviations;
          deviations.reserve(values.size());
          for (double value : values) deviations.push_back(std::fabs(value - median));
          std::sort(deviations.begin(), deviations.end());
          mad = deviations[deviations.size() / 2];
          std::vector<double> magnitudes;
          magnitudes.reserve(values.size());
          for (double value : values) magnitudes.push_back(std::fabs(value));
          std::sort(magnitudes.begin(), magnitudes.end());
          absolute = magnitudes[magnitudes.size() / 2];
        };
        double i13 = 0, m13 = 0, a13 = 0, i23 = 0, m23 = 0, a23 = 0;
        double i33 = 0, m33 = 0, a33 = 0, idim = 0, mdim = 0, adim = 0;
        double i13d = 0, m13d = 0, a13d = 0, i23d = 0, m23d = 0, a23d = 0;
        stats(boundary13, i13, m13, a13);
        stats(boundary23, i23, m23, a23);
        stats(interior, i33, m33, a33);
        stats(interior_dim, idim, mdim, adim);
        stats(boundary13_dim, i13d, m13d, a13d);
        stats(boundary23_dim, i23d, m23d, a23d);
        // The residual low-frequency field: median |delta| per cell of an 8x8
        // split of the selection order (Hilbert-contiguous cells).
        std::vector<double> cell_medians;
        for (std::size_t cell = 0; cell < 8; ++cell) {
          std::vector<double> values;
          for (std::size_t i = 0; i + 1 < count; ++i) {
            if (ids[2 * i] != ids[2 * i + 2] || ids[2 * i + 1] + 1 != ids[2 * i + 3]) continue;
            if (coverage[i] != 3 || coverage[i + 1] != 3) continue;
            if (i * 8 / count != cell) continue;
            const double a = level[i];
            const double b = level[i + 1];
            if (!std::isfinite(a) || !std::isfinite(b) || a + b == 0.0) continue;
            values.push_back(std::fabs((b - a) / (0.5 * (std::fabs(a) + std::fabs(b)))));
          }
          if (values.size() < 30) continue;
          std::sort(values.begin(), values.end());
          cell_medians.push_back(values[values.size() / 2]);
        }
        double lf_min = 0.0, lf_max = 0.0;
        if (!cell_medians.empty()) {
          lf_min = *std::min_element(cell_medians.begin(), cell_medians.end());
          lf_max = *std::max_element(cell_medians.begin(), cell_medians.end());
        }
        seam_note.precision(4);
        seam_note << "seam_low_frequency: K=1/K=3 boundary signed median " << 100.0 * i13
                  << "% med|delta| " << 100.0 * a13 << "% MAD " << 100.0 * m13
                  << "% n=" << boundary13.size() << "; K=2/K=3 signed " << 100.0 * i23
                  << "% med|delta| " << 100.0 * a23 << "% MAD " << 100.0 * m23
                  << "% n=" << boundary23.size() << "; interior K=3/K=3 signed " << 100.0 * i33
                  << "% med|delta| " << 100.0 * a33 << "% MAD " << 100.0 * m33
                  << "% n=" << interior.size() << "; dim band (0.9-1.3x median): K1/K3 med|delta| "
                  << 100.0 * a13d << "% n=" << boundary13_dim.size() << ", K2/K3 "
                  << 100.0 * a23d << "% n=" << boundary23_dim.size() << ", interior "
                  << 100.0 * adim << "% n=" << interior_dim.size() << "; interior 8x8 cell "
                  << "median |delta| " << 100.0 * lf_min << ".." << 100.0 * lf_max
                  << "% (17-cell field); standardized residual not computable: the coadd noise "
                     "is unknown and named; the residual flat screen is not removed";
        seam_measured = true;
      }
    }
  }
  // The applied relative scale is named on a mutable copy of the coadd
  // estimate: the coadd core returns a const share and the assessment of a
  // published product is not rewritten in place.
  Ref<SignalEstimate> published_estimate = val(coadd_result).estimate;
  {
    bool applied = false;
    for (std::size_t f = 1; f < night_scale.size(); ++f) {
      if (std::fabs(night_scale[f] - 1.0) > 1e-12) applied = true;
    }
    if (applied && published_estimate) {
      auto copy = std::make_shared<SignalEstimate>(*published_estimate);
      std::ostringstream note;
      note.precision(6);
      note << "relative_scale_applied_from_star_photometry:";
      for (std::size_t f = 1; f < night_scale.size(); ++f) {
        note << " f" << f << "/f0=" << night_scale[f] << " [" << night_scale_p16[f] << ","
             << night_scale_p84[f] << "] over " << night_scale_stars[f] << " stars;";
      }
      note << " the coadd is expressed in frame 0's star scale; the p16..p84 spread is the "
              "named star-scale systematic";
      if (faint_ratio_measured) {
        note << "; measured frame-to-mosaic ratio in the 0.3-3x median level band (the "
                "frame-vs-mosaic ratio is self-referential and compressed):";
        for (std::size_t f = 0; f < faint_ratio_median.size(); ++f) {
          note << " f" << f << "=" << faint_ratio_median[f] << " [" << faint_ratio_p16[f] << ","
               << faint_ratio_p84[f] << "]";
        }
        if (pair_ratio_measured) {
          note << "; honest pairwise corrected-frame ratios in the same band:";
          for (std::size_t f = 1; f < pair_ratio_median.size(); ++f) {
            note << " f" << f << "/f0=" << pair_ratio_median[f] << " [" << pair_ratio_p16[f]
                 << "," << pair_ratio_p84[f] << "]";
          }
        }
        if (spatial_measured) {
          note << "; 8x8 cell medians over the NESTED (Hilbert-contiguous) leaf order:";
          for (std::size_t f = 1; f < spatial_min.size(); ++f) {
            note << " f" << f << "=" << spatial_min[f] << ".." << spatial_max[f] << ";";
          }
        }
        note << " (a common coadd would be 1.0); the residual is a spatially varying "
                "multiplicative term correlated with the rotated master flat, a calibration "
                "systematic that is NOT removed, so the coadd is NOT a common quantity at this "
                "amplitude";
      }
      if (seam_measured) note << "; " << seam_note.str();
      model::set_included(copy->assessment, note.str());
      model::set_omitted(copy->assessment,
                         "numerical_intervals_unavailable: the coaddition does not propagate "
                         "the frame signal intervals");
      model::set_omitted(copy->assessment,
                         "effective_response_unavailable: the coadded estimate carries the "
                         "common unit, not a spatial response");
      if (faint_ratio_measured) {
        model::add_issue(copy->assessment, Code::unsupported,
                         "the coadd is not a common quantity: the 0.3-3x median level band "
                         "ratio spans the measured p16..p84; a single common master flat cannot "
                         "capture the per-night sky-projected throughput/illumination (the "
                         "correlation with the orientation-dependent flat ratio is a hypothesis)",
                         "mosaic.common_quantity",
                         "the delivered mosaic is conditional on the flat/throughput residual; "
                         "it must not be used as a common surface-brightness product without "
                         "reducing the residual",
                         "build a per-night flat or model the multiplicative screen");
      } else {
        model::add_issue(copy->assessment, Code::unsupported,
                         "relative_scale_applied_from_star_photometry; the per-frame scale "
                         "uncertainty is the p16..p84 spread of the star ratios",
                         "mosaic.relative_scale",
                         "the coadd combines frames in one scale; the residual systematic is the "
                         "star-ratio spread",
                         "measure the scale on more stars if a tighter common quantity is needed");
      }
      published_estimate = copy;
    }
    if (!scale_notes.empty() && published_estimate) {
      auto copy = std::make_shared<SignalEstimate>(*published_estimate);
      for (const std::string& text : scale_notes) model::set_omitted(copy->assessment, text);
      published_estimate = copy;
    }
  }
  // The publication.
  auto graph = std::make_shared<ProductGraph>();
  graph->stage = ProductGraph::Stage::mosaic;
  graph->signal = published_estimate;
  graph->profile = request.output.profile.empty() ? "full" : request.output.profile;
  graph->assessment = published_estimate->assessment;
  graph->identity = model::make_identity("acsd.ProductGraph", 2, 0,
                                          model::sha256_hex("acsd.mosaic"), "acsd");
  // The mosaic must reopen and reproduce from the product alone: store the
  // additive offsets of the relative sky and the coadd weights, not only the
  // combined signal (second independent S1 review, blocking item 2).
  graph->sky_beta = sky_beta_ordered_array;
  if (!graph->sky_beta && ok(sky) && val(sky)) graph->sky_beta = val(sky)->beta;
  if (ok(weights) && val(weights)) graph->coadd_coefficients = val(weights)->coefficients;
  graph->sky_leaf_offsets = sky_offsets_array;
  graph->relative_scales = relative_scales_array;
  if (!sky_request.groups.empty()) {
    graph->coadd_frame_order = sky_request.groups.front().frames;
    std::sort(graph->coadd_frame_order.begin(), graph->coadd_frame_order.end());
  }
  for (const ProductLocation& location : request.inputs) graph->inputs.push_back(location.manifest_hash);
  // The coadded arrays were produced inside the Host and live in the runtime
  // BlockStore; publication reads them through the workflow storage view while
  // writing with the ordinary storage.
  workflows::WorkflowStorage publish_storage(context.storage, workflow_runtime.services);
  Context publish_context{context.resources, context.cancellation, publish_storage};
  return publish_product(publish_context, graph, request.output);
}

}  // namespace acsd
