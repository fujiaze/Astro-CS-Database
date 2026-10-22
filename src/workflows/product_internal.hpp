#pragma once
// Internal helpers shared by the workflow modules.
#include <filesystem>

#include "acsd/contracts.hpp"

namespace acsd::workflows {

// Publishes a product graph.  When `image` is given the export product also
// carries the two dimensional signal image with its target WCS.
Result<ProductLocation> publish_graph(Context& context, Ref<ProductGraph> graph,
                                      const PublishRequest& request, const ExportResult* image);

}  // namespace acsd::workflows
