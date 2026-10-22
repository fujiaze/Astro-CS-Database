// The export workflow: open a product, build the target operator and publish
// the projected image.
#include <memory>

#include "acsd/contracts.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "workflows/product_internal.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

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
  Result<Ref<ProductProvider>> product = open_product(context, request.input);
  if (!ok(product)) {
    return fail<ProductLocation>(product.code, product.assessment.issues.empty()
                                                    ? "the input product cannot be opened"
                                                    : product.assessment.issues.front().reason,
                                  "export_image", "the export cannot be produced");
  }
  Result<ExportResult> image = build_export(context, *val(product), request.projection);
  if (!ok(image)) {
    return fail<ProductLocation>(image.code, image.assessment.issues.empty()
                                                  ? "the export cannot be built"
                                                  : image.assessment.issues.front().reason,
                                  "export_image", "the export cannot be produced");
  }
  return export_signal(context, val(image), request.output);
}

}  // namespace acsd
