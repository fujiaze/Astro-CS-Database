// The in-memory frames provider.
//
// The provider serves the frame descriptions, the signals and the data
// quality of a set of normalized frames without a product directory.  The
// frame identifiers are unique and the description, the signal and the data
// quality of one frame share the same index.
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

class MemoryFramesProvider final : public FramesProvider {
 public:
  MemoryFramesProvider(std::vector<Ref<FrameDescription>> descriptions,
                       std::vector<ArrayRef> signals, std::vector<ArrayRef> dq)
      : descriptions_(std::move(descriptions)), signals_(std::move(signals)), dq_(std::move(dq)) {}

  Result<std::vector<Id>> list_frames(Context& context) const override {
    (void)context;
    std::vector<Id> frames;
    for (const Ref<FrameDescription>& description : descriptions_) frames.push_back(description->frame);
    std::sort(frames.begin(), frames.end());
    return ok_result(frames);
  }

  Result<Ref<FrameDescription>> describe_frame(Context& context, Id frame) const override {
    (void)context;
    for (const Ref<FrameDescription>& description : descriptions_) {
      if (description->frame == frame) return ok_result(description);
    }
    return fail<Ref<FrameDescription>>(Code::missing_dependency, "the frame " + frame + " is unknown",
                                       "frames.describe_frame", "the frame cannot be described");
  }

  Result<LeafBlock> read_leaf_block(Context& context, const FrameReadRequest& request) const override {
    (void)context;
    for (std::size_t i = 0; i < descriptions_.size(); ++i) {
      if (descriptions_[i]->frame != request.frame) continue;
      LeafBlock block;
      block.description = descriptions_[i];
      block.signal = signals_[i];
      block.dq = dq_[i];
      block.index = descriptions_[i]->index;
      return ok_result(block);
    }
    return fail<LeafBlock>(Code::missing_dependency, "the frame " + request.frame + " is unknown",
                           "frames.read_leaf_block", "the leaf block cannot be read");
  }

  Result<std::vector<ArrayRef>> read_native_dependencies(Context& context,
                                                          std::span<const SourceIdentity>) const override {
    (void)context;
    std::vector<ArrayRef> arrays;
    for (const ArrayRef& signal : signals_) arrays.push_back(signal);
    return ok_result(arrays);
  }

  Result<Ref<LinearAction>> open_action(Context& context, Id) const override {
    (void)context;
    return fail<Ref<LinearAction>>(Code::unsupported, "the provider carries no reopenable action",
                                   "frames.open_action", "the action cannot be opened");
  }

  Result<Ref<SupportMap>> query_support(Context& context, Id frame, Ref<Index>) const override {
    (void)context;
    for (const Ref<FrameDescription>& description : descriptions_) {
      if (description->frame == frame && description->support) return ok_result(description->support);
    }
    return fail<Ref<SupportMap>>(Code::missing_dependency, "the frame has no support map",
                                 "frames.query_support", "the support cannot be queried");
  }

  Result<Ref<RefinableGeometryProvider>> open_refinable_geometry(Context& context, Id) const override {
    (void)context;
    return fail<Ref<RefinableGeometryProvider>>(
        Code::unsupported, "the provider carries no refinable geometry closure",
        "frames.open_refinable_geometry", "the geometry cannot be reopened");
  }

 private:
  std::vector<Ref<FrameDescription>> descriptions_;
  std::vector<ArrayRef> signals_;
  std::vector<ArrayRef> dq_;
};

}  // namespace

Result<Ref<FramesProvider>> make_frames_provider(Context& context,
                                                 std::span<const Ref<FrameDescription>> descriptions,
                                                 std::span<const ArrayRef> signals,
                                                 std::span<const ArrayRef> dq) {
  (void)context;
  if (descriptions.size() != signals.size() || descriptions.size() != dq.size()) {
    return fail<Ref<FramesProvider>>(Code::invalid_request,
                                     "the descriptions, the signals and the data quality must "
                                     "have the same length",
                                     "make_frames_provider", "the provider cannot be built");
  }
  std::set<Id> frames;
  for (std::size_t i = 0; i < descriptions.size(); ++i) {
    if (!descriptions[i]) {
      return fail<Ref<FramesProvider>>(Code::invalid_request, "a frame description is missing",
                                       "make_frames_provider", "the provider cannot be built");
    }
    if (!signals[i]) {
      return fail<Ref<FramesProvider>>(Code::invalid_request, "a frame signal is missing",
                                       "make_frames_provider", "the provider cannot be built");
    }
    if (descriptions[i]->frame.empty()) {
      return fail<Ref<FramesProvider>>(Code::invalid_request, "a frame has no identifier",
                                       "make_frames_provider", "the provider cannot be built");
    }
    if (!frames.insert(descriptions[i]->frame).second) {
      return fail<Ref<FramesProvider>>(Code::identity_conflict,
                                       "the frame " + descriptions[i]->frame + " is declared twice",
                                       "make_frames_provider", "the provider cannot be built");
    }
    // An array's index_id may be written either as the Index object_id or as
    // its content hash (producers use the content hash when the Index is
    // serialized).  Both name the same index; a different id still fails.
    const Id& expected_index = descriptions[i]->index
                                   ? descriptions[i]->index->identity.object_id
                                   : Id{};
    const Id& expected_content = descriptions[i]->index
                                     ? descriptions[i]->index->identity.content_hash
                                     : Id{};
    if (descriptions[i]->index && !signals[i]->index_id.empty() &&
        signals[i]->index_id != expected_index && signals[i]->index_id != expected_content) {
      return fail<Ref<FramesProvider>>(Code::identity_conflict,
                                       "the signal of the frame " + descriptions[i]->frame +
                                           " does not share its index",
                                       "make_frames_provider", "the provider cannot be built");
    }
  }
  auto provider = std::make_shared<MemoryFramesProvider>(
      std::vector<Ref<FrameDescription>>(descriptions.begin(), descriptions.end()),
      std::vector<ArrayRef>(signals.begin(), signals.end()),
      std::vector<ArrayRef>(dq.begin(), dq.end()));
  return ok_result(std::shared_ptr<const FramesProvider>(provider));
}

}  // namespace acsd
