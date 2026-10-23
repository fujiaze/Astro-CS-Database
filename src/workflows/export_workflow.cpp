// The export workflow: open a product, invoke the typed projection.export
// operation inside the workflow runtime, then publish the projected image.
//
// The workflow is orchestration only: the one existing build_export
// implementation runs through the ProcessHost, the target WCS and the product
// arrays are served from the runtime BlockStore, and the product writer stays
// the shared storage service.
#include <cstdlib>
#include <fstream>
#include <memory>

#include "acsd/modules/generated/status.hpp"
#include "acsd/modules/projection.hpp"
#include "acsd/runtime/process.hpp"
#include "acsd/runtime/task.hpp"

#include "acsd/contracts.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "workflows/normalize_runtime.hpp"
#include "workflows/product_internal.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;
using rt::Done;

template <class T>
const T& value_of(const Result<T>& result) {
  return *result.value;
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

}  // namespace

Result<ProductLocation> export_signal(Context& context, const ExportResult& image,
                                      const PublishRequest& request) {
  if (!image.estimate) {
    return fail<ProductLocation>(Code::invalid_request, "the export has no estimate",
                                  "export_signal", "the product cannot be published");
  }
  auto graph = std::make_shared<ProductGraph>();
  graph->stage = ProductGraph::Stage::export_image;
  graph->signal = image.estimate;
  graph->profile = request.profile.empty() ? "full" : request.profile;
  graph->assessment = image.estimate->assessment;
  graph->identity = model::make_identity("acsd.ProductGraph", 2, 0,
                                          model::sha256_hex("acsd.export_image"), "acsd");
  graph->assessment.complete = true;
  return workflows::publish_graph(context, graph, request, &image);
}

Result<ProductLocation> export_image(Context& context, const ExportWorkflowRequest& request) {
  // The cancellation is checked before the product is opened and before it
  // is published: a cancelled export leaves no product behind.
  if (context.cancellation.requested()) {
    return fail<ProductLocation>(Code::cancelled, "the export was cancelled", "export_image",
                                 "no product was published");
  }
  Result<Ref<ProductProvider>> product = open_product(context, request.input);
  if (!ok(product)) {
    return fail<ProductLocation>(product.code, product.assessment.issues.empty()
                                                    ? "the input product cannot be opened"
                                                    : product.assessment.issues.front().reason,
                                  "export_image", "the export cannot be produced");
  }

  Result<std::shared_ptr<workflows::WorkflowRuntime>> runtime =
      workflows::open_workflow_runtime("export_image");
  if (!runtime.value) {
    return fail<ProductLocation>(runtime.code, "the workflow runtime cannot be created",
                                 "export_image", "the export cannot be produced");
  }
  workflows::WorkflowRuntime& workflow_runtime = **runtime.value;
  Result<Done> ingested = workflows::ingest_product_graph(
      context, workflow_runtime, value_of(product)->graph());
  if (!ingested.value) {
    return fail<ProductLocation>(ingested.code,
                                 "the product arrays cannot be served to the runtime",
                                 "export_image", "the export cannot be produced");
  }

  auto input = std::make_shared<const modules::BuildExportInput>(
      modules::BuildExportInput{value_of(product), request.projection});
  Result<rt::InvocationPlan> plan = modules::plan_projection_export_build_export(
      workflow_runtime.command, input, workflow_runtime.options);
  Result<ExportResult> image =
      rt::wait_external(workflow_runtime.host->invoke<modules::BuildExportInput,
                                                      ExportResult>(
          workflow_runtime.command, "projection.export", "build_export", input));

  if (const char* trace_path = std::getenv("ACSD_EXPORT_TRACE"); trace_path && *trace_path) {
    std::ofstream trace(trace_path, std::ios::app);
    trace << "{\"step\":\"build_export\",\"process\":\"projection.export\",\"code\":\""
          << code_name(image.code) << "\",\"plan_code\":\"" << code_name(plan.code) << "\"";
    if (plan.value) {
      trace << ",\"r_bytes\":" << plan.value->persistent.host_bytes
            << ",\"f_bytes\":" << plan.value->progress_floor.host_bytes
            << ",\"work\":" << plan.value->elements;
    }
    if (ok(image) && image.value->estimate && image.value->estimate->signal)
      trace << ",\"signal_identity\":\""
            << image.value->estimate->signal->identity.object_id << "\"";
    trace << "}\n";
  }

  if (!ok(image)) {
    return fail<ProductLocation>(image.code, image.assessment.issues.empty()
                                                  ? "the export cannot be built"
                                                  : image.assessment.issues.front().reason,
                                  "export_image", "the export cannot be produced");
  }
  workflows::WorkflowStorage storage(context.storage, workflow_runtime.services);
  Context publish_context{context.resources, context.cancellation, storage};
  return export_signal(publish_context, value_of(image), request.output);
}

}  // namespace acsd
