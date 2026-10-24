// The published product: writing, reading and reopening.
//
// A product directory holds the manifest, the canonical object graph, the
// immutable blocks and the signal:
//
//   manifest.json
//   frame_science.json | mosaic_science.json | export_science.json
//   objects/<hash>.json
//   blocks/<sha256>.bin
//   signal/Norder<k>/Dir<d>/Npix<p>.fits   (normalize, mosaic)
//   signal.fits                              (export)
//
// The manifest is written last: a reader that sees it sees a complete
// product.  The publication is atomic: the content is written into a staging
// directory on the same file system, verified, flushed and then renamed.
#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "storage/local_storage.hpp"
#include "workflows/fits_writer.hpp"
#include "workflows/product_internal.hpp"
#include "workflows/product_io.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;
using workflows::json;

std::string hex_of(const Id& content_id) {
  const std::string prefix = "sha256:";
  return content_id.rfind(prefix, 0) == 0 ? content_id.substr(prefix.size()) : content_id;
}

// ---------------------------------------------------------------------------
// The builder: it collects the objects and the blocks of the product
// ---------------------------------------------------------------------------

struct ProductBuilder {
  std::map<Id, workflows::json> objects;
  std::vector<BlockDescriptor> blocks;
  std::map<Id, Array> arrays;
  std::vector<std::string> capabilities;
  std::vector<std::string> omitted;
};

void store_object(ProductBuilder& builder, const Id& hash, const std::string& type,
                  const workflows::json& payload) {
  if (hash.empty()) return;
  if (builder.objects.count(hash) == 0) builder.capabilities.push_back(type);
  builder.objects[hash] = workflows::json{{"type", type}, {"data", payload}};
}

Result<std::string> write_file(const std::filesystem::path& path, const std::string& text) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return fail<std::string>(Code::io_error, "the file cannot be opened", "product.write",
                             "the product cannot be published");
  }
  stream << text;
  stream.flush();
  if (!stream) {
    return fail<std::string>(Code::io_error, "the file cannot be written", "product.write",
                             "the product cannot be published");
  }
  return ok_result(std::string());
}

Result<std::vector<std::byte>> array_bytes(Context& context, const ArrayRef& array) {
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return fail<std::vector<std::byte>>(view.code, "the array cannot be read", "product.write",
                                        "the product cannot be published");
  }
  return ok_result(std::vector<std::byte>(val(view).bytes.begin(), val(view).bytes.end()));
}

// Persists one array as a block and records its descriptor.
Result<Id> persist_array(Context& context, ProductBuilder& builder, const ArrayRef& array) {
  if (!array) return ok_result(Id());
  const Id hash = array->identity.object_id;
  if (builder.arrays.count(hash) != 0) return ok_result(hash);
  Result<std::vector<std::byte>> bytes = array_bytes(context, array);
  if (!ok(bytes)) {
    return fail<Id>(bytes.code, bytes.assessment.issues.front().reason, "product.write",
                    "the product cannot be published");
  }
  const std::string hex = model::sha256_hex(val(bytes));
  BlockDescriptor descriptor;
  descriptor.id = hash;
  descriptor.path = "blocks/" + hex + ".bin";
  descriptor.dtype = array->dtype;
  descriptor.shape = array->shape;
  descriptor.index_id = array->index_id;
  descriptor.unit = array->unit;
  descriptor.begin = 0;
  descriptor.count = 1;
  for (U64 extent : array->shape) descriptor.count *= extent;
  descriptor.codec = BlockDescriptor::Codec::raw;
  descriptor.raw_bytes = val(bytes).size();
  descriptor.stored_bytes = val(bytes).size();
  descriptor.raw_sha256 = model::content_id(hex);
  builder.blocks.push_back(descriptor);
  builder.arrays[hash] = *array;
  workflows::json payload = workflows::encode_array_descriptor(*array);
  payload["block_ids"] = json::array({hash});
  store_object(builder, hash, "acsd.Array", payload);
  return ok_result(hash);
}

// The action closure.  The public interface of a LinearAction exposes its
// recipe but not the child objects, so an action can only be reopened when
// its recipe names its dependencies by their content hash.
Result<std::string> collect_action(Context& context, ProductBuilder& builder,
                                   const LinearAction& action) {
  Result<ActionRecipe> recipe = action.serialize_recipe();
  if (!ok(recipe)) {
    return fail<std::string>(recipe.code, "the action cannot be described", "product.write",
                             "the product cannot be published");
  }
  std::vector<Id> children;
  for (const Id& name : val(recipe).children) {
    if (builder.objects.count(name) == 0) {
      return fail<std::string>(Code::unsupported,
                               "the action closure is not reopenable: the child " + name +
                                   " is not part of the product",
                               "product.write", "the product loses the action closure");
    }
    children.push_back(name);
  }
  std::vector<Id> arrays;
  for (const Id& name : val(recipe).arrays) {
    auto it = builder.arrays.find(name);
    if (it == builder.arrays.end()) {
      return fail<std::string>(Code::unsupported,
                               "the action closure is not reopenable: the array " + name +
                                   " is not part of the product",
                               "product.write", "the product loses the action closure");
    }
    Result<Id> stored = persist_array(context, builder,
                                      std::make_shared<const Array>(it->second));
    if (!ok(stored)) {
      return fail<std::string>(stored.code, "the action array cannot be stored", "product.write",
                               "the product cannot be published");
    }
    arrays.push_back(name);
  }
  workflows::json payload = workflows::encode_linear_action(action, children, arrays);
  store_object(builder, action.identity.object_id, "acsd.LinearAction", payload);
  return ok_result(std::string());
}

Result<std::string> collect_noise(Context& context, ProductBuilder& builder,
                                  const NoiseModel& model) {
  for (const Ref<NoiseRoot>& root : model.innovations) {
    if (!root) continue;
    if (root->coordinates) {
      store_object(builder, root->coordinates->identity.object_id, "acsd.Index",
                   workflows::encode_index(*root->coordinates));
    }
    if (root->factor) {
      Result<std::string> factor = collect_action(context, builder, *root->factor);
      if (!ok(factor)) return factor;
    }
    store_object(builder, root->identity.object_id, "acsd.NoiseRoot",
                 workflows::encode_noise_root(*root));
  }
  for (const Ref<NoiseRoot>& root : model.shared_factors) {
    if (!root) continue;
    if (root->coordinates) {
      store_object(builder, root->coordinates->identity.object_id, "acsd.Index",
                   workflows::encode_index(*root->coordinates));
    }
    if (root->factor) {
      Result<std::string> factor = collect_action(context, builder, *root->factor);
      if (!ok(factor)) return factor;
    }
    store_object(builder, root->identity.object_id, "acsd.NoiseRoot",
                 workflows::encode_noise_root(*root));
  }
  for (const Ref<LinearAction>& influence : model.influence) {
    if (!influence) continue;
    Result<std::string> action = collect_action(context, builder, *influence);
    if (!ok(action)) return action;
  }
  store_object(builder, model.identity.object_id, "acsd.NoiseModel",
               workflows::encode_noise_model(model));
  return ok_result(std::string());
}

Result<std::string> collect_signal(Context& context, ProductBuilder& builder,
                                   const SignalEstimate& estimate) {
  Result<Id> signal = persist_array(context, builder, estimate.signal);
  if (!ok(signal)) {
    return fail<std::string>(signal.code, "the signal cannot be stored", "product.write",
                             "the product cannot be published");
  }
  for (const ArrayRef& array : {estimate.numerical_lower, estimate.numerical_upper}) {
    if (!array) continue;
    Result<Id> stored = persist_array(context, builder, array);
    if (!ok(stored)) {
      return fail<std::string>(stored.code, "the bound cannot be stored", "product.write",
                               "the product cannot be published");
    }
  }
  if (estimate.support) {
    const SupportMap& support = *estimate.support;
    if (support.index) {
      store_object(builder, support.index->identity.object_id, "acsd.Index",
                   workflows::encode_index(*support.index));
      // The identifiers of the leaf index are part of the closure of the
      // product: the export reads the leaves from them.
      if (support.index->ids) {
        Result<Id> stored = persist_array(context, builder, support.index->ids);
        if (!ok(stored)) {
          return fail<std::string>(stored.code, "the leaf identifiers cannot be stored",
                                   "product.write", "the product cannot be published");
        }
      }
    }
    for (const ArrayRef& array : {support.states, support.area_lower_sr, support.area_upper_sr,
                                  support.normalization_states, support.observational_states}) {
      if (!array) continue;
      Result<Id> stored = persist_array(context, builder, array);
      if (!ok(stored)) {
        return fail<std::string>(stored.code, "the support cannot be stored", "product.write",
                                 "the product cannot be published");
      }
    }
    store_object(builder, support.identity.object_id, "acsd.SupportMap",
                 workflows::encode_support_map(support));
  }
  if (estimate.effective_response) {
    store_object(builder, estimate.effective_response->identity.object_id, "acsd.SpatialResponse",
                 workflows::encode_spatial_response(*estimate.effective_response));
  }
  if (estimate.error) {
    store_object(builder, estimate.error->identity.object_id, "acsd.ErrorRecord",
                 workflows::encode_error_record(*estimate.error));
  }
  // The covariance closure may contain an action that the product cannot
  // reopen (the public interface exposes no child list).  That degrades the
  // noise to unknown: the signal and the estimate stay publishable and the
  // omission is named, instead of publishing an estimate reference whose
  // object was never written.
  SignalEstimate effective = estimate;
  if (estimate.noise) {
    bool noise_complete = true;
    if (estimate.noise->model) {
      Result<std::string> model = collect_noise(context, builder, *estimate.noise->model);
      if (!ok(model)) {
        noise_complete = false;
        builder.omitted.push_back(
            "the covariance model is not reopenable: " +
            (model.assessment.issues.empty() ? std::string("the closure is incomplete")
                                             : model.assessment.issues.front().reason));
      }
    }
    if (noise_complete && estimate.noise->influence) {
      Result<std::string> action = collect_action(context, builder, *estimate.noise->influence);
      if (!ok(action)) {
        noise_complete = false;
        builder.omitted.push_back(
            "the covariance influence is not reopenable: " +
            (action.assessment.issues.empty() ? std::string("the closure is incomplete")
                                              : action.assessment.issues.front().reason));
      }
    }
    if (noise_complete) {
      store_object(builder, estimate.noise->identity.object_id, "acsd.CovarianceAction",
                   workflows::encode_covariance_action(*estimate.noise));
    } else {
      effective.noise = nullptr;  // null means unknown, never zero variance
    }
  }
  store_object(builder, effective.identity.object_id, "acsd.SignalEstimate",
               workflows::encode_signal_estimate(effective));
  return ok_result(std::string());
}

// ---------------------------------------------------------------------------
// The reader
// ---------------------------------------------------------------------------

class ProductReader {
 public:
  ProductReader(Context& context, std::filesystem::path root, ProductManifest manifest,
                std::map<Id, workflows::json> objects)
      : context_(context), root_(std::move(root)), manifest_(std::move(manifest)),
        objects_(std::move(objects)) {}

  Result<workflows::json> object(const Id& hash) {
    auto it = objects_.find(hash);
    if (it == objects_.end()) {
      return fail<workflows::json>(Code::missing_dependency, "the object " + hash + " is missing",
                                   "product.read", "the product cannot be reopened");
    }
    const json& entry = it->second;
    if (!entry.is_object()) {
      return fail<workflows::json>(Code::corrupt_product, "the object " + hash + " is malformed",
                                   "product.read", "the product cannot be reopened");
    }
    // The objects carry their type and their payload; the sidecar of the root
    // object may carry the payload alone.
    if (entry.contains("data")) return ok_result(entry.at("data"));
    return ok_result(entry);
  }

  Result<ArrayRef> array(const Id& hash) {
    auto cached = arrays_.find(hash);
    if (cached != arrays_.end()) return ok_result(cached->second);
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<ArrayRef>(payload.code, payload.assessment.issues.front().reason, "product.read",
                            "the product cannot be reopened");
    }
    Array description = workflows::decode_array_descriptor(val(payload));
    std::vector<BlockDescriptor> descriptors;
    for (const Id& block : description.block_ids) {
      bool found = false;
      for (const BlockDescriptor& candidate : manifest_.blocks) {
        if (candidate.id == block) {
          descriptors.push_back(candidate);
          found = true;
          break;
        }
      }
      if (!found) {
        return fail<ArrayRef>(Code::missing_dependency, "the block " + block + " is missing",
                              "product.read", "the product cannot be reopened");
      }
    }
    // A block that is not on the disk is an incomplete closure, not a disk
    // failure: the caller distinguishes the two.
    for (const BlockDescriptor& block : descriptors) {
      const std::filesystem::path path = root_ / block.path;
      std::error_code missing;
      if (!std::filesystem::exists(path, missing)) {
        return fail<ArrayRef>(Code::missing_dependency,
                              "the block " + block.path + " is missing from the product",
                              "product.read", "the product cannot be reopened");
      }
    }
    Result<ArrayRef> opened =
        context_.storage.open(context_, root_.string(), description, descriptors);
    if (!ok(opened)) {
      return fail<ArrayRef>(opened.code, opened.assessment.issues.front().reason, "product.read",
                            "the product cannot be reopened");
    }
    arrays_[hash] = val(opened);
    return ok_result(val(opened));
  }

  Result<Ref<Index>> index(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<Index>>(payload.code, payload.assessment.issues.front().reason, "product.read",
                             "the product cannot be reopened");
    }
    Index index = workflows::decode_index(val(payload));
    // The identifiers of a leaf index are reopened: the export reads the
    // (order, ipix) pairs of the leaves from them.
    const Id ids = workflows::decode_ref(val(payload).contains("ids")
                                             ? val(payload).at("ids")
                                             : json(nullptr));
    if (!ids.empty()) {
      Result<ArrayRef> array = this->array(ids);
      if (!ok(array)) {
        return fail<Ref<Index>>(array.code, "the index identifiers cannot be reopened",
                                "product.read", "the product cannot be reopened");
      }
      index.ids = val(array);
    }
    auto result = std::make_shared<Index>(index);
    return ok_result(std::shared_ptr<const Index>(result));
  }

  Result<Ref<SupportMap>> support(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<SupportMap>>(payload.code, payload.assessment.issues.front().reason,
                                   "product.read", "the product cannot be reopened");
    }
    SupportMap support = workflows::decode_support_map(val(payload));
    // The leaf index of the support is reopened: the export resolves the
    // leaves of the product through it.
    const Id index = workflows::decode_ref(val(payload).contains("index")
                                               ? val(payload).at("index")
                                               : json(nullptr));
    if (!index.empty()) {
      Result<Ref<Index>> resolved = this->index(index);
      if (!ok(resolved)) {
        return fail<Ref<SupportMap>>(resolved.code, "the support index cannot be reopened",
                                     "product.read", "the product cannot be reopened");
      }
      support.index = val(resolved);
    }
    struct Link {
      Id hash;
      ArrayRef* target;
    };
    const Link links[5] = {{support.states ? support.states->identity.object_id : Id(), &support.states},
                            {support.area_lower_sr ? support.area_lower_sr->identity.object_id : Id(),
                             &support.area_lower_sr},
                            {support.area_upper_sr ? support.area_upper_sr->identity.object_id : Id(),
                             &support.area_upper_sr},
                            {support.normalization_states
                                 ? support.normalization_states->identity.object_id
                                 : Id(),
                             &support.normalization_states},
                            {support.observational_states
                                 ? support.observational_states->identity.object_id
                                 : Id(),
                             &support.observational_states}};
    for (const Link& link : links) {
      if (link.hash.empty()) continue;
      Result<ArrayRef> array = this->array(link.hash);
      if (!ok(array)) {
        return fail<Ref<SupportMap>>(array.code, "the support array cannot be reopened",
                                     "product.read", "the product cannot be reopened");
      }
      *link.target = val(array);
    }
    auto result = std::make_shared<SupportMap>(support);
    return ok_result(std::shared_ptr<const SupportMap>(result));
  }

  Result<Ref<ErrorRecord>> error(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<ErrorRecord>>(payload.code, payload.assessment.issues.front().reason,
                                    "product.read", "the product cannot be reopened");
    }
    auto result = std::make_shared<ErrorRecord>(workflows::decode_error_record(val(payload)));
    return ok_result(std::shared_ptr<const ErrorRecord>(result));
  }

  Result<Ref<SpatialResponse>> response(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<SpatialResponse>>(payload.code, payload.assessment.issues.front().reason,
                                        "product.read", "the product cannot be reopened");
    }
    auto result = std::make_shared<SpatialResponse>(
        workflows::decode_spatial_response(val(payload)));
    return ok_result(std::shared_ptr<const SpatialResponse>(result));
  }

  Result<Ref<SignalEstimate>> estimate(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<SignalEstimate>>(payload.code, payload.assessment.issues.front().reason,
                                       "product.read", "the product cannot be reopened");
    }
    SignalEstimate estimate = workflows::decode_signal_estimate(val(payload));
    // The array of the signal is reopened.
    const Id signal = workflows::decode_ref(val(payload).contains("signal")
                                                ? val(payload).at("signal")
                                                : json(nullptr));
    if (!signal.empty()) {
      Result<ArrayRef> array = this->array(signal);
      if (!ok(array)) {
        return fail<Ref<SignalEstimate>>(array.code, "the signal cannot be reopened", "product.read",
                                        "the product cannot be reopened");
      }
      estimate.signal = val(array);
    }
    // The support map of the estimate is reopened as well: the export needs
    // the support of every leaf.
    const auto ref = [&](const char* key) {
      return workflows::decode_ref(val(payload).contains(key) ? val(payload).at(key)
                                                              : json(nullptr));
    };
    const Id support_ref = ref("support");
    if (!support_ref.empty()) {
      Result<Ref<SupportMap>> resolved = this->support(support_ref);
      if (!ok(resolved)) {
        return fail<Ref<SignalEstimate>>(
            resolved.code,
            resolved.assessment.issues.empty()
                ? "the support cannot be reopened"
                : "the support cannot be reopened: " + resolved.assessment.issues.front().reason,
            "product.read", "the product cannot be reopened");
      }
      estimate.support = val(resolved);
    }
    // The published estimate carries the target bounds, the error record and
    // the effective response as object references.  They are reopened here;
    // a silent drop used to leave the mosaic without its spatial response.
    for (const char* key : {"numerical_lower", "numerical_upper"}) {
      const Id id = ref(key);
      if (id.empty()) continue;
      Result<ArrayRef> array = this->array(id);
      if (!ok(array)) {
        return fail<Ref<SignalEstimate>>(array.code, "the bound cannot be reopened",
                                         "product.read", "the product cannot be reopened");
      }
      if (std::string(key) == "numerical_lower")
        estimate.numerical_lower = val(array);
      else
        estimate.numerical_upper = val(array);
    }
    const Id response_ref = ref("effective_response");
    if (!response_ref.empty()) {
      Result<Ref<SpatialResponse>> resolved = this->response(response_ref);
      if (!ok(resolved)) {
        return fail<Ref<SignalEstimate>>(resolved.code, "the response cannot be reopened",
                                         "product.read", "the product cannot be reopened");
      }
      estimate.effective_response = val(resolved);
    }
    const Id error_ref = ref("error");
    if (!error_ref.empty()) {
      Result<Ref<ErrorRecord>> resolved = this->error(error_ref);
      if (!ok(resolved)) {
        return fail<Ref<SignalEstimate>>(resolved.code, "the error record cannot be reopened",
                                         "product.read", "the product cannot be reopened");
      }
      estimate.error = val(resolved);
    }
    auto result = std::make_shared<SignalEstimate>(estimate);
    return ok_result(std::shared_ptr<const SignalEstimate>(result));
  }

  Result<Ref<ProductGraph>> graph(const Id& hash) {
    Result<workflows::json> payload = object(hash);
    if (!ok(payload)) {
      return fail<Ref<ProductGraph>>(payload.code, payload.assessment.issues.front().reason,
                                     "product.read", "the product cannot be reopened");
    }
    ProductGraph graph = workflows::decode_product_graph(val(payload));
    const Id signal = workflows::decode_ref(val(payload).contains("signal")
                                                ? val(payload).at("signal")
                                                : json(nullptr));
    if (!signal.empty()) {
      Result<Ref<SignalEstimate>> resolved = this->estimate(signal);
      if (!ok(resolved)) {
        return fail<Ref<ProductGraph>>(
            resolved.code,
            resolved.assessment.issues.empty()
                ? "the signal cannot be reopened"
                : "the signal cannot be reopened: " + resolved.assessment.issues.front().reason,
            "product.read", "the product cannot be reopened");
      }
      graph.signal = val(resolved);
    }
    auto result = std::make_shared<ProductGraph>(graph);
    return ok_result(std::shared_ptr<const ProductGraph>(result));
  }

 private:
  Context& context_;
  std::filesystem::path root_;
  ProductManifest manifest_;
  std::map<Id, workflows::json> objects_;
  std::map<Id, ArrayRef> arrays_;
};

// ---------------------------------------------------------------------------
// The product provider that reads a published product
// ---------------------------------------------------------------------------

class DiskProduct final : public ProductProvider {
 public:
  DiskProduct(Ref<ProductGraph> graph, ProductManifest manifest, std::filesystem::path root,
              std::map<Id, BlockDescriptor> blocks)
      : graph_(std::move(graph)),
        manifest_(std::move(manifest)),
        root_(std::move(root)),
        blocks_(std::move(blocks)) {}

  Ref<ProductGraph> graph() const override { return graph_; }

  Result<ArrayView> read_array(Context& context, ArrayRef array, BlockRequest range) const override {
    if (!array) {
      return fail<ArrayView>(Code::invalid_request, "no array supplied", "product.read_array",
                             "the array cannot be read");
    }
    std::vector<BlockDescriptor> descriptors;
    for (const Id& block : array->block_ids) {
      auto it = blocks_.find(block);
      if (it == blocks_.end()) {
        return fail<ArrayView>(Code::missing_dependency, "the block " + block + " is missing",
                               "product.read_array", "the array cannot be read");
      }
      descriptors.push_back(it->second);
    }
    if (descriptors.empty()) {
      return fail<ArrayView>(Code::missing_dependency, "the array has no block", "product.read_array",
                             "the array cannot be read");
    }
    Result<ArrayRef> opened = context.storage.open(context, root_.string(), *array, descriptors);
    if (!ok(opened)) {
      return fail<ArrayView>(opened.code, "the array cannot be opened", "product.read_array",
                             "the array cannot be read");
    }
    return context.storage.read(context, val(opened), range);
  }

  Result<std::vector<Id>> list_frames(Context& context) const override {
    (void)context;
    std::vector<Id> frames;
    if (graph_ && graph_->signal) {
      // The product identity is unique per published frame; the estimator id
      // names the method and is shared by every product of the same stage.
      frames.push_back(graph_->identity.object_id.empty() ? graph_->signal->estimator.id
                                                          : graph_->identity.object_id);
    }
    return ok_result(frames);
  }

  Result<Ref<FrameDescription>> describe_frame(Context& context, Id frame) const override {
    (void)context;
    if (!graph_ || !graph_->signal) {
      return fail<Ref<FrameDescription>>(Code::missing_dependency, "the product has no signal",
                                         "product.describe_frame", "the frame cannot be described");
    }
    auto description = std::make_shared<FrameDescription>();
    description->frame = frame;
    description->order =
        graph_->signal->support && graph_->signal->support->index
            ? graph_->signal->support->index->order
            : 0;
    description->index = graph_->signal->support ? graph_->signal->support->index : nullptr;
    description->estimator = graph_->signal->estimator;
    description->support = graph_->signal->support;
    description->response = graph_->signal->effective_response;
    description->noise = graph_->signal->noise;
    description->capabilities = manifest_.capabilities;
    description->identity = graph_->signal->identity;
    description->assessment = graph_->signal->assessment;
    return ok_result(std::shared_ptr<const FrameDescription>(description));
  }

  Result<LeafBlock> read_leaf_block(Context& context, const FrameReadRequest& request) const override {
    (void)request;
    (void)context;
    if (!graph_ || !graph_->signal) {
      return fail<LeafBlock>(Code::missing_dependency, "the product has no signal",
                             "product.read_leaf_block", "the leaf block cannot be read");
    }
    LeafBlock block;
    block.signal = graph_->signal->signal;
    block.index = graph_->signal->support ? graph_->signal->support->index : nullptr;
    return ok_result(block);
  }

  Result<std::vector<ArrayRef>> read_native_dependencies(Context& context,
                                                          std::span<const SourceIdentity>) const override {
    (void)context;
    std::vector<ArrayRef> arrays;
    if (graph_ && graph_->signal && graph_->signal->signal) arrays.push_back(graph_->signal->signal);
    return ok_result(arrays);
  }

  Result<Ref<LinearAction>> open_action(Context& context, Id) const override {
    (void)context;
    return fail<Ref<LinearAction>>(Code::unsupported, "the product carries no reopenable action",
                                   "product.open_action", "the action cannot be opened");
  }

  Result<Ref<SupportMap>> query_support(Context& context, Id, Ref<Index>) const override {
    (void)context;
    if (!graph_ || !graph_->signal || !graph_->signal->support) {
      return fail<Ref<SupportMap>>(Code::missing_dependency, "the product has no support map",
                                    "product.query_support", "the support cannot be queried");
    }
    return ok_result(graph_->signal->support);
  }

  Result<Ref<RefinableGeometryProvider>> open_refinable_geometry(Context& context,
                                                                 Id) const override {
    (void)context;
    return fail<Ref<RefinableGeometryProvider>>(
        Code::unsupported, "the published product carries no refinable geometry closure",
        "product.open_refinable_geometry", "the geometry cannot be reopened");
  }

 private:
  Ref<ProductGraph> graph_;
  ProductManifest manifest_;
  std::filesystem::path root_;
  std::map<Id, BlockDescriptor> blocks_;
};

std::string sidecar_name(ProductGraph::Stage stage) {
  switch (stage) {
    case ProductGraph::Stage::normalize: return "frame_science.json";
    case ProductGraph::Stage::mosaic: return "mosaic_science.json";
    case ProductGraph::Stage::export_image: return "export_science.json";
  }
  return "frame_science.json";
}

}  // namespace

// ---------------------------------------------------------------------------
// publish_product
// ---------------------------------------------------------------------------

Result<ProductLocation> workflows::publish_graph(Context& context, Ref<ProductGraph> graph,
                                                 const PublishRequest& request,
                                                 const ExportResult* image) {
  if (!graph) {
    return fail<ProductLocation>(Code::invalid_request, "no product graph", "publish_product",
                                  "the product cannot be published");
  }
  if (!graph->signal || !graph->signal->signal) {
    return fail<ProductLocation>(Code::invalid_model, "the product graph has no signal",
                                 "publish_product", "the product cannot be published");
  }
  if (request.destination.empty()) {
    return fail<ProductLocation>(Code::invalid_request, "the destination is empty", "publish_product",
                                  "the product cannot be published");
  }
  const std::filesystem::path destination(request.destination);
  std::error_code ec;
  if (std::filesystem::exists(destination, ec)) {
    return fail<ProductLocation>(Code::invalid_request,
                                 "the destination already exists: " + request.destination,
                                 "publish_product", "the product cannot be published");
  }

  ProductBuilder builder;
  Result<std::string> collected = collect_signal(context, builder, *graph->signal);
  if (!ok(collected)) {
    // The signal is published without the closure that could not be reopened.
    builder.omitted.push_back(collected.assessment.issues.empty()
                                   ? "the closure is incomplete"
                                   : collected.assessment.issues.front().reason);
    builder.objects.clear();
    builder.blocks.clear();
    builder.arrays.clear();
    builder.capabilities.clear();
    Result<std::string> retry = collect_signal(context, builder, *graph->signal);
    (void)retry;
  }

  // The staging directory lives on the same file system as the destination.
  const std::filesystem::path staging = destination.string() + ".staging";
  std::filesystem::remove_all(staging, ec);
  std::filesystem::create_directories(staging, ec);
  if (ec) {
    return fail<ProductLocation>(Code::io_error, "the staging directory cannot be created",
                                 "publish_product", "the product cannot be published");
  }

  // The blocks.
  for (const BlockDescriptor& descriptor : builder.blocks) {
    auto it = builder.arrays.find(descriptor.id);
    if (it == builder.arrays.end()) continue;
    Result<std::vector<std::byte>> bytes = array_bytes(context, std::make_shared<const Array>(it->second));
    if (!ok(bytes)) {
      return fail<ProductLocation>(bytes.code, "the block cannot be read", "publish_product",
                                   "the product cannot be published");
    }
    Result<std::string> written = write_file(staging / descriptor.path,
                                              std::string(reinterpret_cast<const char*>(val(bytes).data()),
                                                          val(bytes).size()));
    if (!ok(written)) return fail<ProductLocation>(written.code, written.assessment.issues.front().reason,
                                                   "publish_product", "the product cannot be published");
  }

  // The objects.
  for (const auto& entry : builder.objects) {
    Result<std::string> written =
        write_file(staging / "objects" / (hex_of(entry.first) + ".json"), workflows::canonical_text(entry.second));
    if (!ok(written)) return fail<ProductLocation>(written.code, written.assessment.issues.front().reason,
                                                   "publish_product", "the product cannot be published");
  }

  // The sidecar: the root object of the graph.
  const workflows::json root_payload = workflows::encode_product_graph(*graph);
  const Id root_hash = workflows::canonical_id(root_payload);
  const workflows::json root_object = workflows::json{{"type", "acsd.ProductGraph"},
                                                       {"data", root_payload}};
  Result<std::string> sidecar = write_file(staging / sidecar_name(graph->stage),
                                            workflows::canonical_text(root_object));
  if (!ok(sidecar)) return fail<ProductLocation>(sidecar.code, sidecar.assessment.issues.front().reason,
                                                 "publish_product", "the product cannot be published");
  write_file(staging / "objects" / (hex_of(root_hash) + ".json"),
             workflows::canonical_text(root_object));

  // The signal: the HiPS tiles for the frame products, the image for the
  // export product.
  if (graph->stage == ProductGraph::Stage::export_image) {
    if (!image) {
      return fail<ProductLocation>(Code::invalid_request,
                                   "the export product needs the projected image",
                                   "publish_product", "the product cannot be published");
    }
    Result<std::string> written = workflows::write_export_image(context, staging, *image);
    if (!ok(written)) {
      return fail<ProductLocation>(written.code, written.assessment.issues.front().reason,
                                   "publish_product", "the product cannot be published");
    }
  } else {
    const SupportMap* support = graph->signal->support.get();
    if (!support || !support->index) {
      return fail<ProductLocation>(Code::missing_dependency, "the product has no support map",
                                   "publish_product", "the product cannot be published");
    }
    const U32 order = support->index->order;
    const U32 tile_log2_width = request.tile_log2_width == 0 ? 9 : request.tile_log2_width;
    if (tile_log2_width < 6 || tile_log2_width > 9) {
      return fail<ProductLocation>(Code::invalid_request, "the tile width is outside the allowed range",
                                   "publish_product", "the product cannot be published");
    }
    U64 count = 1;
    for (U64 extent : graph->signal->signal->shape) count *= extent;
    Result<std::vector<double>> values = operators::read_f64(context, graph->signal->signal, 0, count);
    if (!ok(values)) {
      return fail<ProductLocation>(values.code, "the signal cannot be read", "publish_product",
                                   "the product cannot be published");
    }
    std::vector<HealpixLeaf> leaves;
    if (support->index->ids) {
      Result<ArrayView> view = context.storage.read(context, support->index->ids,
                                                    BlockRequest{0, support->index->size * 2});
      if (ok(view)) {
        const std::vector<U64> ids = model::decode_u64(val(view).bytes);
        for (std::size_t i = 0; i + 1 < ids.size(); i += 2) {
          HealpixLeaf leaf;
          leaf.order = static_cast<U32>(ids[i]);
          leaf.ipix = ids[i + 1];
          leaves.push_back(leaf);
        }
      }
    }
    Result<std::string> tiles = workflows::write_hips_order(staging / "signal", order, tile_log2_width,
                                                             leaves, val(values),
                                                             graph->signal->unit.scale);
    if (!ok(tiles)) {
      return fail<ProductLocation>(tiles.code, tiles.assessment.issues.front().reason,
                                   "publish_product", "the product cannot be published");
    }
    Result<std::string> properties = workflows::write_hips_properties(staging, order, tile_log2_width,
                                                                       graph->signal->unit.scale);
    if (!ok(properties)) {
      return fail<ProductLocation>(properties.code, properties.assessment.issues.front().reason,
                                   "publish_product", "the product cannot be published");
    }
  }

  // The manifest: the staging manifest first, the complete manifest last.
  ProductManifest manifest;
  manifest.format = "acsd.product";
  manifest.major = 2;
  manifest.minor = 0;
  manifest.stage = graph->stage;
  manifest.product_id = "acsd.product:" + hex_of(root_hash);
  manifest.revision = 1;
  manifest.root_object = root_hash;
  manifest.profile = request.profile.empty() ? "full" : request.profile;
  manifest.capabilities = builder.capabilities;
  for (const std::string& omitted : builder.omitted) manifest.capabilities.push_back("omitted:" + omitted);
  manifest.inputs = graph->inputs;
  for (const auto& entry : builder.objects) {
    ObjectEntry object;
    object.hash = entry.first;
    object.path = "objects/" + hex_of(entry.first) + ".json";
    object.type = entry.second.at("type").get<std::string>();
    object.major = 2;
    object.minor = 0;
    manifest.objects.push_back(object);
  }
  ObjectEntry root_entry;
  root_entry.hash = root_hash;
  root_entry.path = sidecar_name(graph->stage);
  root_entry.type = "acsd.ProductGraph";
  root_entry.major = 2;
  root_entry.minor = 0;
  manifest.objects.push_back(root_entry);
  manifest.blocks = builder.blocks;
  manifest.configuration_hash = workflows::canonical_id(workflows::json{
      {"destination", request.destination},
      {"profile", request.profile},
      {"tile_log2_width", request.tile_log2_width},
      {"signal_dtype", static_cast<int>(request.signal_dtype)},
      {"durable", request.durable}});
  AlgorithmIdentity algorithm;
  algorithm.name = "acsd.publish_product_v1";
  algorithm.version = "1";
  algorithm.source_hash = model::content_id(model::sha256_hex("acsd.publish_product_v1"));
  manifest.algorithm_set.push_back(algorithm);
  manifest.arithmetic.mode = Arithmetic::Mode::outward;
  manifest.arithmetic.initial_bits = 64;
  manifest.arithmetic.max_bits = 256;
  manifest.arithmetic.allow_target_fallback = true;
  manifest.assessment = graph->assessment;
  manifest.assessment.execution = ExecutionState::complete;
  manifest.assessment.complete = true;

  manifest.complete = false;
  Result<std::string> staging_manifest =
      write_file(staging / "manifest.staging.json", workflows::canonical_text(workflows::encode_manifest(manifest)));
  if (!ok(staging_manifest)) {
    return fail<ProductLocation>(staging_manifest.code, staging_manifest.assessment.issues.front().reason,
                                 "publish_product", "the product cannot be published");
  }

  // The verification of the closure before the manifest becomes complete.
  for (const ObjectEntry& object : manifest.objects) {
    if (!std::filesystem::exists(staging / object.path, ec)) {
      return fail<ProductLocation>(Code::corrupt_product, "the object " + object.hash + " is missing",
                                   "publish_product", "the product cannot be published");
    }
  }
  for (const BlockDescriptor& descriptor : manifest.blocks) {
    const std::filesystem::path path = staging / descriptor.path;
    if (!std::filesystem::exists(path, ec)) {
      return fail<ProductLocation>(Code::corrupt_product, "the block is missing", "publish_product",
                                   "the product cannot be published");
    }
  }

  manifest.complete = true;
  Result<std::string> complete_manifest =
      write_file(staging / "manifest.json", workflows::canonical_text(workflows::encode_manifest(manifest)));
  if (!ok(complete_manifest)) {
    return fail<ProductLocation>(complete_manifest.code, complete_manifest.assessment.issues.front().reason,
                                 "publish_product", "the product cannot be published");
  }

  // The atomic publication.
  std::filesystem::rename(staging, destination, ec);
  if (ec) {
    return fail<ProductLocation>(Code::io_error, "the product cannot be published: " + ec.message(),
                                 "publish_product", "the product cannot be published");
  }

  ProductLocation location;
  location.directory = destination.string();
  location.manifest_hash = workflows::canonical_id(workflows::encode_manifest(manifest));
  return ok_result(location);
}

Result<ProductLocation> publish_product(Context& context, Ref<ProductGraph> graph,
                                        const PublishRequest& request) {
  return workflows::publish_graph(context, graph, request, nullptr);
}

// ---------------------------------------------------------------------------
// open_product
// ---------------------------------------------------------------------------

Result<Ref<ProductProvider>> open_product(Context& context, const ProductLocation& location) {
  if (location.directory.empty()) {
    return fail<Ref<ProductProvider>>(Code::invalid_request, "the product has no directory",
                                      "open_product", "the product cannot be opened");
  }
  const std::filesystem::path root(location.directory);
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    return fail<Ref<ProductProvider>>(Code::missing_dependency, "the product directory is missing",
                                      "open_product", "the product cannot be opened");
  }
  const std::filesystem::path manifest_path = root / "manifest.json";
  if (!std::filesystem::exists(manifest_path, ec)) {
    return fail<Ref<ProductProvider>>(Code::corrupt_product, "the product has no manifest",
                                      "open_product", "the product cannot be opened");
  }
  std::ifstream stream(manifest_path, std::ios::binary);
  if (!stream) {
    return fail<Ref<ProductProvider>>(Code::io_error, "the manifest cannot be read", "open_product",
                                      "the product cannot be opened");
  }
  std::ostringstream text;
  text << stream.rdbuf();
  json manifest_json;
  try {
    manifest_json = json::parse(text.str());
  } catch (const std::exception& failure) {
    return fail<Ref<ProductProvider>>(Code::corrupt_product,
                                      std::string("the manifest is not valid JSON: ") + failure.what(),
                                      "open_product", "the product cannot be opened");
  }
  ProductManifest manifest = workflows::decode_manifest(manifest_json);
  if (manifest.format != "acsd.product") {
    return fail<Ref<ProductProvider>>(Code::corrupt_product, "the product format is unknown",
                                      "open_product", "the product cannot be opened");
  }
  if (manifest.major != 2) {
    return fail<Ref<ProductProvider>>(Code::unsupported, "the product major version is not supported",
                                      "open_product", "the product cannot be opened");
  }
  if (!manifest.complete) {
    return fail<Ref<ProductProvider>>(Code::corrupt_product, "the product is not complete",
                                      "open_product", "the product cannot be opened");
  }
  if (!location.manifest_hash.empty()) {
    const Id actual = workflows::canonical_id(manifest_json);
    if (actual != location.manifest_hash) {
      return fail<Ref<ProductProvider>>(Code::hash_mismatch, "the manifest hash does not match",
                                        "open_product", "the product cannot be opened");
    }
  }

  // The object graph.
  std::map<Id, workflows::json> objects;
  for (const ObjectEntry& entry : manifest.objects) {
    const std::filesystem::path path = root / entry.path;
    if (!std::filesystem::exists(path, ec)) {
      return fail<Ref<ProductProvider>>(Code::missing_dependency,
                                        "the object " + entry.hash + " is missing", "open_product",
                                        "the product cannot be opened");
    }
    std::ifstream object_stream(path, std::ios::binary);
    if (!object_stream) {
      return fail<Ref<ProductProvider>>(Code::io_error, "the object cannot be read", "open_product",
                                        "the product cannot be opened");
    }
    std::ostringstream object_text;
    object_text << object_stream.rdbuf();
    json parsed;
    try {
      parsed = json::parse(object_text.str());
    } catch (const std::exception& failure) {
      return fail<Ref<ProductProvider>>(Code::corrupt_product,
                                        std::string("the object is not valid JSON: ") + failure.what(),
                                        "open_product", "the product cannot be opened");
    }
    objects[entry.hash] = parsed;
  }

  ProductReader reader(context, root, manifest, objects);
  Result<Ref<ProductGraph>> graph = reader.graph(manifest.root_object);
  if (!ok(graph)) {
    return fail<Ref<ProductProvider>>(graph.code, graph.assessment.issues.front().reason, "open_product",
                                      "the product cannot be opened");
  }
  std::map<Id, BlockDescriptor> blocks;
  for (const BlockDescriptor& descriptor : manifest.blocks) blocks[descriptor.id] = descriptor;
  auto product = std::make_shared<DiskProduct>(val(graph), manifest, root, blocks);
  return ok_result(std::shared_ptr<const ProductProvider>(product));
}

}  // namespace acsd
