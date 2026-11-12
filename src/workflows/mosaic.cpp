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

  Result<Ref<FramesProvider>> frames = make_frames_provider(context, descriptions, signals, dq);
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
  for (const Ref<SignalEstimate>& estimate : estimates) {
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
        modules::ApplySkyInput{estimates[i], val(sky), descriptions[i]->frame});
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
      // Apply: divide the corrected signal of every measured frame by its scale.
      workflows::WorkflowStorage scale_storage(context.storage, workflow_runtime.services);
      Context scale_read_context{context.resources, context.cancellation, scale_storage};
      for (std::size_t f = 1; f < estimates.size(); ++f) {
        if (std::fabs(night_scale[f] - 1.0) <= 1e-12) continue;
        const U64 count = corrected_signals[f]->shape.empty() ? 0 : corrected_signals[f]->shape[0];
        Result<std::vector<double>> values =
            operators::read_f64(scale_read_context, corrected_signals[f], 0, count);
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
            context, scaled_values, corrected_signals[f]->shape, estimates[f]->unit,
            corrected_signals[f]->index_id);
        if (!ok(scaled)) {
          return fail<ProductLocation>(scaled.code, "the scaled signal cannot be stored",
                                       "mosaic", "no product was published");
        }
        corrected_signals[f] = val(scaled);
      }
    } else {
      scale_notes.push_back("frame 0 has no usable star sample; no relative scale is applied");
    }
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
      std::vector<double> offsets(samples * frames_count, 0.0);
      for (std::size_t f = 0; f < frames_count; ++f) {
        if (!estimates[f] || !estimates[f]->signal || !unscaled_corrected[f] ||
            !descriptions[f] || !descriptions[f]->index) {
          continue;
        }
        Result<std::vector<double>> original =
            operators::read_f64(read_context, estimates[f]->signal, 0,
                                estimates[f]->signal->shape.empty()
                                    ? 0
                                    : estimates[f]->signal->shape[0]);
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
          if (found != offset_of_leaf.end()) offsets[sel * frames_count + f] = found->second;
        }
      }
      Result<ArrayRef> stored_offsets = operators::write_f64(
          context, offsets, std::vector<U64>{static_cast<U64>(samples),
                                             static_cast<U64>(frames_count)},
          model::unit_dimensionless(), Id());
      if (ok(stored_offsets)) sky_offsets_array = val(stored_offsets);
      std::vector<double> scales(night_scale.begin(), night_scale.end());
      Result<ArrayRef> stored_scales = operators::write_f64(
          context, scales, std::vector<U64>{static_cast<U64>(scales.size())},
          model::unit_dimensionless(), Id());
      if (ok(stored_scales)) relative_scales_array = val(stored_scales);
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
      note << " coadd is common in frame 0's scale; the p16..p84 spread is the named systematic";
      model::set_included(copy->assessment, note.str());
      model::add_issue(copy->assessment, Code::unsupported,
                       "relative_scale_applied_from_star_photometry; the per-frame scale "
                       "uncertainty is the p16..p84 spread of the star ratios",
                       "mosaic.relative_scale",
                       "the coadd combines frames in one scale; the residual systematic is the "
                       "star-ratio spread",
                       "measure the scale on more stars if a tighter common quantity is needed");
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
  if (ok(sky) && val(sky)) graph->sky_beta = val(sky)->beta;
  if (ok(weights) && val(weights)) graph->coadd_coefficients = val(weights)->coefficients;
  graph->sky_leaf_offsets = sky_offsets_array;
  graph->relative_scales = relative_scales_array;
  for (const ProductLocation& location : request.inputs) graph->inputs.push_back(location.manifest_hash);
  // The coadded arrays were produced inside the Host and live in the runtime
  // BlockStore; publication reads them through the workflow storage view while
  // writing with the ordinary storage.
  workflows::WorkflowStorage publish_storage(context.storage, workflow_runtime.services);
  Context publish_context{context.resources, context.cancellation, publish_storage};
  return publish_product(publish_context, graph, request.output);
}

}  // namespace acsd
