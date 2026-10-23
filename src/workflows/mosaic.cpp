// The mosaic workflow.
//
// The inputs are opened first: a corrupted input fails the whole mosaic, no
// frame is silently dropped.  The frames of the products are merged into one
// provider, the relative sky is fitted and the coaddition produces the
// signal HiPS of the mosaic.
#include <algorithm>
#include <map>
#include <cstdlib>
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
  U32 order = 0;
  bool first = true;
  std::set<std::pair<U32, U64>> leaves;
  for (const Ref<ProductProvider>& product : products) {
    Result<std::vector<Id>> frames = product->list_frames(context);
    if (!ok(frames)) {
      return fail<ProductLocation>(frames.code, "the frames of an input cannot be listed", "mosaic",
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
  auto controls_input = std::make_shared<const modules::BuildSkyControlsInput>(
      modules::BuildSkyControlsInput{val(frames), request.sky});
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
      modules::FitRelativeSkyInput{val(controls), request.sky});
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
  auto select_input = std::make_shared<const modules::SelectSamplesInput>(
      modules::SelectSamplesInput{val(frames), request.coaddition});
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
      modules::SolveWeightsInput{val(frames), val(selection), request.coaddition});
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
                          request.coaddition});
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

  // The publication.
  auto graph = std::make_shared<ProductGraph>();
  graph->stage = ProductGraph::Stage::mosaic;
  graph->signal = val(coadd_result).estimate;
  graph->profile = request.output.profile.empty() ? "full" : request.output.profile;
  graph->assessment = val(coadd_result).estimate->assessment;
  graph->identity = model::make_identity("acsd.ProductGraph", 2, 0,
                                          model::sha256_hex("acsd.mosaic"), "acsd");
  for (const ProductLocation& location : request.inputs) graph->inputs.push_back(location.manifest_hash);
  return publish_product(context, graph, request.output);
}

}  // namespace acsd
