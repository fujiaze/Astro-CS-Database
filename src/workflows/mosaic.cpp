// The mosaic workflow.
//
// The inputs are opened first: a corrupted input fails the whole mosaic, no
// frame is silently dropped.  The frames of the products are merged into one
// provider, the relative sky is fitted and the coaddition produces the
// signal HiPS of the mosaic.
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

}  // namespace

Result<ProductLocation> mosaic(Context& context, const MosaicRequest& request) {
  if (request.inputs.empty()) {
    return fail<ProductLocation>(Code::invalid_request, "the mosaic has no input", "mosaic",
                                 "no product was published");
  }
  // The inputs are opened before anything is computed.
  std::vector<Ref<ProductProvider>> products;
  for (const ProductLocation& location : request.inputs) {
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

  // The provider of the mosaic.
  Result<Ref<FramesProvider>> frames = make_frames_provider(context, descriptions, signals, dq);
  if (!ok(frames)) {
    return fail<ProductLocation>(frames.code, "the frames cannot be merged", "mosaic",
                                 "no product was published");
  }

  // The relative sky and the coaddition.
  Result<Ref<SkyControls>> controls = build_sky_controls(context, *val(frames), request.sky);
  if (!ok(controls)) {
    return fail<ProductLocation>(controls.code, "the sky controls cannot be built", "mosaic",
                                 "no product was published");
  }
  Result<Ref<SkyFit>> sky = fit_relative_sky(context, val(controls), request.sky);
  if (!ok(sky)) {
    return fail<ProductLocation>(sky.code, "the relative sky cannot be fitted", "mosaic",
                                 "no product was published");
  }
  Result<CoaddResult> coadd_result = coadd(context, *val(frames), val(sky), request.coaddition);
  if (!ok(coadd_result)) {
    return fail<ProductLocation>(coadd_result.code, "the coaddition failed", "mosaic",
                                 "no product was published");
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
