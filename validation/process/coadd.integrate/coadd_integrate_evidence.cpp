// coadd.integrate evidence: the sealed Selection/WeightSolution are combined by
// the one core `coadd_with_inputs()` and the result is bit-identical to the
// historical `coadd()` for the same policy.
//
// Covered:
//   * 2 frames x 2 leaves, equal weights / dq_only rejection;
//   * old coadd() (select + weights + combination) vs coadd_with_inputs()
//     with the pre-sealed selection/weights: signal and identity bit-identical;
//   * provider call counters prove the integration path performs exactly one
//     list_frames/describe/read pass (no second selection or weight solve);
//   * negative cases: wrong selection identity -> identity_conflict, missing
//     weights -> invalid_request, wrong coefficient dimension -> invalid_request,
//     requested policy != sealed policy -> identity_conflict;
//   * process validate/bounds/plan and the real run hook through the runtime
//     science Context bridge, with the plan R/F numbers;
//   * the same counters on the process run (no select/weights).
//
// Only public contract types plus the module's own headers are used; the
// runtime bridge is the same implementation the production adapter uses.
#include <acsd/contracts.hpp>
#include <acsd/modules/coadd.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "coadd/coadd.hpp"
#include "harness.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "runtime/internal.hpp"

// The registration helper of the production module asks the generated assembly
// for the descriptor of coadd.integrate.  This evidence binary does not link the
// full generated assembly; the fixture supplies the same descriptor so the
// translation unit can be linked without dragging in every other process.
namespace acsd::generated {
Result<rt::ProcessDescriptor> descriptor_for(Id process_id) {
  rt::ProcessDescriptor descriptor{
      process_id,
      "coadd",
      "docs/engineering/rebuild/processes/coadd.integrate.md",
      1,
      {"model", "operators", "uncertainty", "numerics"},
      {{"mosaic.integration.signal", "span", 1},
       {"mosaic.integration.block", "span", 1}}};
  return model::ok_result(descriptor);
}
}  // namespace acsd::generated

namespace {

namespace rt = acsd::rt;
using acsd::Array;
using acsd::ArrayRef;
using acsd::CoaddRequest;
using acsd::CoaddResult;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::FrameDescription;
using acsd::FrameReadRequest;
using acsd::FramesProvider;
using acsd::HealpixLeaf;
using acsd::Id;
using acsd::Index;
using acsd::LeafBlock;
using acsd::RawObservation;
using acsd::Ref;
using acsd::Result;
using acsd::Selection;
using acsd::SkyFit;
using acsd::SupportMap;
using acsd::U32;
using acsd::U64;
using acsd::Unit;
using acsd::WeightPolicy;
using acsd::WeightSolution;

Unit signal_unit() {
  Unit unit;
  unit.scale = "coadd";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit dq_unit() {
  Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

template <class T>
std::vector<std::byte> to_bytes(const std::vector<T>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(T));
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

struct StoredArray {
  DType dtype = DType::f64;
  std::vector<U64> shape;
  Unit unit;
  Id index;
  std::vector<std::byte> bytes;
};

class FixtureStorage final : public acsd::Storage {
 public:
  Result<ArrayRef> copy(Context&, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    StoredArray stored;
    stored.dtype = dtype;
    stored.shape.assign(shape.begin(), shape.end());
    stored.unit = unit;
    stored.index = std::move(index);
    stored.bytes.assign(bytes.begin(), bytes.end());
    const Id id = "acsd.Array:coaddfx:" + std::to_string(sequence_++);
    entries_[id] = std::move(stored);
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape = entries_[id].shape;
    array->unit = unit;
    array->index_id = entries_[id].index;
    array->identity.object_id = id;
    array->identity.content_hash = "sha256:" + std::string(64, 'a');
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    array->assessment.complete = true;
    Result<ArrayRef> out;
    out.code = Code::ok;
    out.value = std::shared_ptr<const Array>(array);
    return out;
  }

  Result<acsd::ArrayView> read(Context&, ArrayRef array, acsd::BlockRequest range) const override {
    Result<acsd::ArrayView> out;
    const auto found = entries_.find(array->identity.object_id);
    if (found == entries_.end()) {
      out.code = Code::invalid_request;
      return out;
    }
    const StoredArray& stored = found->second;
    const std::size_t item = stored.dtype == DType::u16 ? 2
                              : stored.dtype == DType::u8 ? 1
                              : stored.dtype == DType::u64 ? 8
                                                           : 8;
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    if (offset + length > stored.bytes.size()) {
      out.code = Code::invalid_request;
      return out;
    }
    acsd::ArrayView view;
    view.dtype = stored.dtype;
    view.shape = stored.shape;
    view.bytes = std::span<const std::byte>(stored.bytes.data() + offset, length);
    out.code = Code::ok;
    out.value = std::move(view);
    return out;
  }

  Result<ArrayRef> open(Context&, std::string, const Array&,
                        std::span<const acsd::BlockDescriptor>) override {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }

  ArrayRef store_f64(Context& context, const std::vector<double>& values,
                     const std::vector<U64>& shape, const Id& index) {
    Result<ArrayRef> stored =
        context.storage.copy(context, DType::f64, to_bytes(values), shape, signal_unit(), index);
    return stored.value.value_or(nullptr);
  }
  ArrayRef store_u16(Context& context, const std::vector<std::uint16_t>& values,
                     const std::vector<U64>& shape, const Id& index) {
    Result<ArrayRef> stored =
        context.storage.copy(context, DType::u16, to_bytes(values), shape, dq_unit(), index);
    return stored.value.value_or(nullptr);
  }
  ArrayRef store_u8(Context& context, const std::vector<std::uint8_t>& values,
                    const std::vector<U64>& shape, const Id& index) {
    Result<ArrayRef> stored =
        context.storage.copy(context, DType::u8, to_bytes(values), shape, dq_unit(), index);
    return stored.value.value_or(nullptr);
  }
  ArrayRef store_u64(Context& context, const std::vector<U64>& values,
                     const std::vector<U64>& shape, const Id& index) {
    Result<ArrayRef> stored =
        context.storage.copy(context, DType::u64, to_bytes(values), shape, dq_unit(), index);
    return stored.value.value_or(nullptr);
  }

  // Registers every stored array in the runtime block store so the bridge can
  // read the already sealed Selection/WeightSolution arrays.
  void register_into(const std::shared_ptr<rt::Services>& services) const {
    if (!services || !services->blocks) return;
    for (const auto& [id, stored] : entries_) {
      auto bytes = std::make_shared<const std::vector<std::byte>>(stored.bytes);
      services->blocks->register_array(id, std::move(bytes), stored.dtype, stored.shape,
                                       acsd::Lease{});
    }
  }

 private:
  U64 sequence_ = 0;
  std::map<Id, StoredArray> entries_;
};

class FixtureResources final : public acsd::Resources {
 public:
  Result<acsd::Reservation> reserve(U64 bytes, acsd::U32 threads) override {
    Result<acsd::Reservation> out;
    out.code = Code::ok;
    out.value = acsd::Reservation{acsd::Lease{}, bytes, threads};
    return out;
  }
  Result<U64> consume_work(U64 units) override {
    Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    return out;
  }
  acsd::Usage usage() const override { return {}; }
};

class FixtureCancellation final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

struct Counters {
  std::atomic<int> list{0};
  std::atomic<int> describe{0};
  std::atomic<int> read{0};
  std::atomic<int> support{0};
  void reset() {
    list = 0;
    describe = 0;
    read = 0;
    support = 0;
  }
};

struct TestFrame {
  Id frame;
  U32 order = 3;
  std::vector<HealpixLeaf> leaves;
  std::vector<double> signal;
  std::vector<std::uint16_t> dq;
  std::vector<std::uint8_t> state;
};

class FixtureFrames final : public FramesProvider {
 public:
  FixtureFrames(FixtureStorage* storage, std::vector<TestFrame> frames)
      : storage_(storage), frames_(std::move(frames)) {}

  Result<std::vector<Id>> list_frames(Context&) const override {
    counters_->list.fetch_add(1);
    std::vector<Id> ids;
    for (const TestFrame& frame : frames_) ids.push_back(frame.frame);
    std::sort(ids.begin(), ids.end());
    Result<std::vector<Id>> out;
    out.code = Code::ok;
    out.value = std::move(ids);
    return out;
  }

  Result<Ref<FrameDescription>> describe_frame(Context& context, Id frame) const override {
    counters_->describe.fetch_add(1);
    for (const TestFrame& candidate : frames_) {
      if (candidate.frame != frame) continue;
      auto index = std::make_shared<Index>();
      index->size = candidate.leaves.size();
      index->kind = Index::Kind::leaf;
      index->order = candidate.order;
      index->namespace_id = "fixture:" + frame;
      std::vector<U64> ids;
      for (const HealpixLeaf& leaf : candidate.leaves) {
        ids.push_back(leaf.order);
        ids.push_back(leaf.ipix);
      }
      index->ids = storage_->store_u64(context, ids, {candidate.leaves.size(), 2},
                                       "fixture_index:" + frame);
      auto description = std::make_shared<FrameDescription>();
      description->frame = frame;
      description->order = candidate.order;
      description->index = std::shared_ptr<const Index>(index);
      description->capabilities = {"signal", "dq"};
      Result<Ref<FrameDescription>> out;
      out.code = Code::ok;
      out.value = std::shared_ptr<const FrameDescription>(description);
      return out;
    }
    Result<Ref<FrameDescription>> out;
    out.code = Code::invalid_request;
    return out;
  }

  Result<LeafBlock> read_leaf_block(Context& context,
                                    const FrameReadRequest& request) const override {
    counters_->read.fetch_add(1);
    for (const TestFrame& candidate : frames_) {
      if (candidate.frame != request.frame) continue;
      LeafBlock block;
      block.signal = storage_->store_f64(context, candidate.signal,
                                        {candidate.signal.size()}, "fixture_signal:" + request.frame);
      block.dq = storage_->store_u16(context, candidate.dq, {candidate.dq.size()},
                                     "fixture_dq:" + request.frame);
      Result<LeafBlock> out;
      out.code = Code::ok;
      out.value = std::move(block);
      return out;
    }
    Result<LeafBlock> out;
    out.code = Code::invalid_request;
    return out;
  }

  Result<std::vector<ArrayRef>> read_native_dependencies(
      Context&, std::span<const acsd::SourceIdentity>) const override {
    Result<std::vector<ArrayRef>> out;
    out.code = Code::ok;
    out.value = {};
    return out;
  }

  Result<Ref<acsd::LinearAction>> open_action(Context&, Id) const override {
    Result<Ref<acsd::LinearAction>> out;
    out.code = Code::unsupported;
    return out;
  }

  Result<Ref<SupportMap>> query_support(Context&, Id, Ref<Index>) const override {
    counters_->support.fetch_add(1);
    Result<Ref<SupportMap>> out;
    out.code = Code::unavailable;
    return out;
  }

  Result<Ref<acsd::RefinableGeometryProvider>> open_refinable_geometry(
      Context&, Id) const override {
    Result<Ref<acsd::RefinableGeometryProvider>> out;
    out.code = Code::unsupported;
    return out;
  }

  Counters* counters() const { return counters_; }
  void attach(Counters* counters) { counters_ = counters; }

 private:
  FixtureStorage* storage_ = nullptr;
  std::vector<TestFrame> frames_;
  Counters* counters_ = nullptr;
};

Ref<SkyFit> make_sky() {
  auto sky = std::make_shared<SkyFit>();
  sky->basis.kind = acsd::Basis::Kind::constant;
  sky->basis.degree = 0;
  sky->identity.object_id = "acsd.SkyFit:fixture";
  sky->identity.content_hash = "sha256:" + std::string(64, 'b');
  sky->identity.schema_id = "acsd.SkyFit";
  sky->identity.schema_major = 2;
  sky->identity.schema_minor = 0;
  sky->assessment.complete = true;
  return sky;
}

CoaddRequest make_request() {
  CoaddRequest request;
  request.target.id = "fixture_target";
  request.target.quantity = "flux";
  request.target.unit = signal_unit();
  request.target.condition.estimator = "fixture_target";
  request.target.condition.kind = "relative_sky";
  request.mixed_observation = false;
  request.weights = WeightPolicy::equal;
  request.rejection.kind = acsd::RejectionPolicy::Kind::dq_only;
  request.rejection.alpha = 0.0;
  return request;
}

std::vector<double> read_f64(Context& context, ArrayRef array) {
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<acsd::ArrayView> view =
      context.storage.read(context, array, acsd::BlockRequest{0, count});
  if (view.code != Code::ok || !view.value) return {};
  std::vector<double> values(count);
  if (count > 0) std::memcpy(values.data(), view.value->bytes.data(), count * sizeof(double));
  return values;
}

std::string code_name(Code code) { return std::string(vrt::code_name(code)); }

bool same_bits(const std::vector<double>& a, const std::vector<double>& b) {
  return a.size() == b.size() && !a.empty() &&
         std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}

struct Fixture {
  FixtureStorage storage;
  FixtureResources resources;
  FixtureCancellation cancellation;
  Context context{resources, cancellation, storage};
};

}  // namespace

int main() {
  vrt::Recorder recorder("coadd_integrate",
                         "validation/process/coadd.integrate/coadd_integrate_evidence.cpp");

  Fixture fixture;
  std::vector<TestFrame> frames;
  {
    TestFrame a;
    a.frame = "frame0";
    a.leaves = {HealpixLeaf{3, 10}, HealpixLeaf{3, 11}};
    a.signal = {10.0, 20.0};
    a.dq = {0, 0};
    a.state = {1, 1};
    frames.push_back(a);
    TestFrame b;
    b.frame = "frame1";
    b.leaves = {HealpixLeaf{3, 10}, HealpixLeaf{3, 11}};
    b.signal = {30.0, 45.0};
    b.dq = {0, 0};
    b.state = {1, 1};
    frames.push_back(b);
  }
  Counters counters;
  auto provider = std::make_shared<FixtureFrames>(&fixture.storage, frames);
  provider->attach(&counters);
  const Ref<FramesProvider> frames_ref = provider;
  const Ref<SkyFit> sky = make_sky();
  const CoaddRequest request = make_request();

  // The historical entry point: selection + weights + combination.
  counters.reset();
  Result<CoaddResult> old_result = acsd::coadd(fixture.context, *provider, sky, request);
  const int old_list = counters.list.load();
  const int old_describe = counters.describe.load();
  const int old_read = counters.read.load();
  recorder.value("coadd.old.list_frames_calls", static_cast<U64>(old_list));
  recorder.value("coadd.old.describe_calls", static_cast<U64>(old_describe));
  recorder.value("coadd.old.read_leaf_calls", static_cast<U64>(old_read));
  recorder.check("cal.coadd_old_ok", old_result.code == Code::ok && old_result.value, "ok",
                 code_name(old_result.code), "the historical coadd() entry still works");

  // Seal the two stages explicitly (the only place they are solved).
  Result<Ref<Selection>> selection =
      acsd::select_samples(fixture.context, *provider, request);
  Result<Ref<WeightSolution>> weights =
      selection.value ? acsd::solve_weights(fixture.context, *provider, *selection.value, request)
                      : Result<Ref<WeightSolution>>{Code::internal, std::nullopt, {}, {}, {}};
  recorder.check("coadd.select_ok", selection.code == Code::ok && selection.value, "ok",
                 code_name(selection.code), "the selection is sealed once");
  recorder.check("coadd.weights_ok", weights.code == Code::ok && weights.value, "ok",
                 code_name(weights.code), "the weights are solved once");

  acsd::CoaddInputs inputs;
  inputs.frames = provider.get();
  inputs.sky = sky;
  inputs.selection = selection.value ? *selection.value : nullptr;
  inputs.weights = weights.value ? *weights.value : nullptr;
  inputs.request = request;

  // The unique core with the sealed inputs.
  counters.reset();
  Result<CoaddResult> new_result = acsd::coadd_with_inputs(fixture.context, inputs);
  const int new_list = counters.list.load();
  const int new_describe = counters.describe.load();
  const int new_read = counters.read.load();
  recorder.value("coadd.new.list_frames_calls", static_cast<U64>(new_list));
  recorder.value("coadd.new.describe_calls", static_cast<U64>(new_describe));
  recorder.value("coadd.new.read_leaf_calls", static_cast<U64>(new_read));
  recorder.check("coadd.core_ok", new_result.code == Code::ok && new_result.value, "ok",
                 code_name(new_result.code), "the sealed core returns a result");
  recorder.check("coadd.no_second_selection",
                 new_list == 1 && new_describe == 2 && new_read == 2,
                 "1 list_frames, 2 describe, 2 read_leaf_block",
                 "list=" + std::to_string(new_list) + " describe=" +
                     std::to_string(new_describe) + " read=" + std::to_string(new_read),
                 "select_samples/solve_weights would each add a full list+read pass");
  recorder.check("coadd.no_second_weights", old_list >= 2 && new_list == 1,
                 "the old path lists at least twice, the sealed core once",
                 "old=" + std::to_string(old_list) + " new=" + std::to_string(new_list),
                 "counters show the frozen inputs are consumed, not recomputed");
  bool identical = false;
  if (old_result.value && new_result.value) {
    std::vector<double> old_values = read_f64(fixture.context, (*old_result.value).estimate->signal);
    std::vector<double> new_values = read_f64(fixture.context, (*new_result.value).estimate->signal);
    identical = same_bits(old_values, new_values) &&
                (*old_result.value).estimate->identity.content_hash ==
                    (*new_result.value).estimate->identity.content_hash;
    recorder.value("coadd.estimate.identity",
                   (*new_result.value).estimate->identity.content_hash);
    recorder.value("coadd.signal0", new_values.empty() ? 0.0 : new_values[0]);
    recorder.value("coadd.signal1", new_values.size() < 2 ? 0.0 : new_values[1]);
  }
  recorder.check("coadd.bit_identical", identical,
                 "the sealed core equals the historical coadd() signal and identity",
                 identical ? "bit identical" : "different",
                 "equal policy / dq_only rejection in both paths");

  // Negative cases.
  {
    auto other = std::make_shared<Selection>(*inputs.selection);
    other->identity.content_hash = "sha256:" + std::string(64, 'c');
    acsd::CoaddInputs bad = inputs;
    bad.selection = other;
    Result<rt::Done> validated = acsd::validate_coadd_inputs(bad);
    Result<CoaddResult> combined = acsd::coadd_with_inputs(fixture.context, bad);
    recorder.check("coadd.negative_selection_identity",
                   validated.code == Code::identity_conflict &&
                       combined.code == Code::identity_conflict,
                   "identity_conflict",
                   code_name(validated.code) + "/" + code_name(combined.code),
                   "the weights solve for a different Selection identity");
    acsd::modules::CoaddInput typed;
    typed.frames = frames_ref;
    typed.sky = sky;
    typed.selection = other;
    typed.weights = inputs.weights;
    typed.request = request;
  }
  {
    acsd::CoaddInputs bad = inputs;
    bad.weights = nullptr;
    Result<rt::Done> validated = acsd::validate_coadd_inputs(bad);
    recorder.check("coadd.negative_missing_weights", validated.code == Code::invalid_request,
                   "invalid_request", code_name(validated.code),
                   "a missing WeightSolution is invalid_request");
  }
  {
    std::vector<double> coefficients(2 * 3, 1.0);  // L=2, but 3 frames declared
    ArrayRef wrong = fixture.storage.store_f64(
        fixture.context, coefficients, {2, 3}, "coadd_weights_bad");
    auto bad_weights = std::make_shared<WeightSolution>(*inputs.weights);
    bad_weights->coefficients = wrong;
    acsd::CoaddInputs bad = inputs;
    bad.weights = bad_weights;
    Result<rt::Done> validated = acsd::validate_coadd_inputs(bad);
    recorder.check("coadd.negative_dimension", validated.code == Code::invalid_request,
                   "invalid_request", code_name(validated.code),
                   "the coefficient matrix dimension does not match leaves x frames");
  }
  {
    acsd::CoaddInputs bad = inputs;
    bad.request.weights = WeightPolicy::blue;  // the sealed solution is equal
    Result<rt::Done> validated = acsd::validate_coadd_inputs(bad);
    recorder.check("coadd.negative_policy", validated.code == Code::identity_conflict,
                   "identity_conflict", code_name(validated.code),
                   "the requested weight policy differs from the sealed policy");
    acsd::CoaddInputs rejection = inputs;
    rejection.request.rejection.kind = acsd::RejectionPolicy::Kind::single_max_loo;
    Result<rt::Done> rejection_validated = acsd::validate_coadd_inputs(rejection);
    recorder.check("coadd.negative_rejection", rejection_validated.code == Code::identity_conflict,
                   "identity_conflict", code_name(rejection_validated.code),
                   "the sealed selection rejection policy differs from the request");
  }

  // The typed process: validate/bounds/plan.
  auto typed = std::make_shared<acsd::modules::CoaddInput>();
  typed->frames = frames_ref;
  typed->sky = sky;
  typed->selection = inputs.selection;
  typed->weights = inputs.weights;
  typed->request = request;
  Result<rt::Done> validated = acsd::modules::validate_coadd_integrate(rt::InvocationContext{}, typed);
  Result<rt::PlanBounds> bounds =
      acsd::modules::bounds_coadd_integrate(rt::InvocationContext{}, typed);
  rt::PlanOptions options;
  options.block_elements = 2;
  options.window = 1;
  options.placement = rt::Placement::memory;
  Result<rt::InvocationPlan> plan =
      acsd::modules::plan_coadd_integrate(rt::InvocationContext{}, typed, options);
  recorder.check("coadd.process_validate", validated.code == Code::ok, "ok",
                 code_name(validated.code), "the sealed typed input validates");
  bool plan_ok = plan.code == Code::ok && plan.value;
  recorder.check("coadd.process_bounds",
                 bounds.code == Code::ok && bounds.value && bounds.value->min_block_elements == 2 &&
                     bounds.value->max_block_elements == 2 &&
                     bounds.value->max_window == 1 && !bounds.value->allow_spill,
                 "min=max=2, window=1, no spill",
                 bounds.value ? std::to_string(bounds.value->min_block_elements) : "-",
                 "the whole-frame core fixes the legal block");
  if (plan_ok) {
    recorder.value("coadd.plan.R", plan.value->persistent.host_bytes);
    recorder.value("coadd.plan.F", plan.value->progress_floor.host_bytes);
    recorder.value("coadd.plan.work", plan.value->regions[0].stages[0].work_upper);
    recorder.value("coadd.plan.edges", static_cast<U64>(plan.value->regions[0].edges.size()));
    recorder.check("coadd.plan_numbers",
                   plan.value->persistent.host_bytes == 73744 &&
                       plan.value->progress_floor.host_bytes == 1056860 &&
                       plan.value->regions[0].stages[0].work_upper == 1088 &&
                       plan.value->regions[0].edges.empty() && plan.value->cohorts == 1,
                   "R=73744, F=1056860, work=1088, no edge",
                   "R=" + std::to_string(plan.value->persistent.host_bytes) +
                       " F=" + std::to_string(plan.value->progress_floor.host_bytes),
                   "the conservative whole-frame plan");
  } else {
    recorder.check("coadd.plan_numbers", false, "ok", code_name(plan.code), "plan");
  }

  // The same negative inputs through the typed process validate hook: the
  // adapter must preserve the shared codes (identity_conflict / invalid_request).
  {
    auto other = std::make_shared<Selection>(*inputs.selection);
    other->identity.content_hash = "sha256:" + std::string(64, 'd');
    auto bad = std::make_shared<acsd::modules::CoaddInput>(*typed);
    bad->selection = other;
    Result<rt::Done> validated =
        acsd::modules::validate_coadd_integrate(rt::InvocationContext{}, bad);
    recorder.check("coadd.process_negative_selection", validated.code == Code::identity_conflict,
                   "identity_conflict", code_name(validated.code),
                   "the typed validate keeps the identity conflict");
    auto no_weights = std::make_shared<acsd::modules::CoaddInput>(*typed);
    no_weights->weights = nullptr;
    validated = acsd::modules::validate_coadd_integrate(rt::InvocationContext{}, no_weights);
    recorder.check("coadd.process_negative_missing_weights",
                   validated.code == Code::invalid_request, "invalid_request",
                   code_name(validated.code), "a missing WeightSolution is invalid_request");
    std::vector<double> coefficients(2 * 3, 1.0);
    ArrayRef wrong = fixture.storage.store_f64(fixture.context, coefficients, {2, 3},
                                               "coadd_weights_bad_typed");
    auto bad_weights = std::make_shared<WeightSolution>(*inputs.weights);
    bad_weights->coefficients = wrong;
    auto dimension = std::make_shared<acsd::modules::CoaddInput>(*typed);
    dimension->weights = bad_weights;
    validated = acsd::modules::validate_coadd_integrate(rt::InvocationContext{}, dimension);
    recorder.check("coadd.process_negative_dimension",
                   validated.code == Code::invalid_request, "invalid_request",
                   code_name(validated.code), "a wrong coefficient dimension is invalid_request");
  }

  // The real process run through the runtime science Context bridge.
  {
    rt::PoolConfig config;
    config.process = vrt::limits(64ull << 20, 1000000, 1ull << 20);
    config.cpu_threads = 2;
    config.read_slots = 1;
    config.write_slots = 1;
    config.scratch_root = vrt::scratch_root("coadd_integrate");
    Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
    if (services.code != Code::ok || !services.value) {
      recorder.check("coadd.process_run", false, "services", code_name(services.code),
                     "the runtime services cannot be created");
    } else {
      Result<rt::InvocationContext> command =
          rt::open_command(*services.value, "coadd.integrate.evidence",
                           vrt::limits(64ull << 20, 1000000, 1ull << 20));
      fixture.storage.register_into(*services.value);
      Result<rt::ArenaRef> arena =
          command.value ? rt::reserve(*command.value, vrt::bytes(16ull << 20))
                        : Result<rt::ArenaRef>{Code::internal, std::nullopt, {}, {}, {}};
      if (command.code != Code::ok || !command.value || arena.code != Code::ok ||
          !arena.value) {
        recorder.check("coadd.process_run", false, "context", code_name(command.code),
                       "the command/arena cannot be opened");
      } else {
        rt::InvocationContext context = *command.value;
        context.persistent = *arena.value;
        context.progress = *arena.value;
        counters.reset();
        const acsd::Ref<rt::InvocationPlan> plan_ref =
            plan.value ? std::make_shared<rt::InvocationPlan>(*plan.value) : nullptr;
        Result<CoaddResult> run = rt::wait_external(
            acsd::modules::run_coadd_integrate(context, typed, plan_ref));
        std::vector<double> values;
        if (run.code == Code::ok && run.value) {
          Result<rt::Block<double>> block = rt::wait_external(rt::read_async<double>(
              context, *arena.value, run.value->estimate->signal, acsd::BlockRequest{0, 2}));
          if (block.code == Code::ok && block.value)
            values.assign(block.value->values.begin(), block.value->values.end());
        }
        const int run_list = counters.list.load();
        const int run_read = counters.read.load();
        recorder.value("coadd.run.list_frames_calls", static_cast<U64>(run_list));
        recorder.value("coadd.run.read_leaf_calls", static_cast<U64>(run_read));
        bool run_ok = run.code == Code::ok && run.value && values.size() == 2 &&
                      values[0] == 20.0 && values[1] == 32.5 &&
                      run_list == 1 && run_read == 2;
        recorder.check("coadd.process_run", run_ok,
                       "[16,32.5], one list/read pass through the real run hook",
                       code_name(run.code), "run_coadd_integrate consumes the sealed inputs");
        if (!values.empty()) {
          recorder.value("coadd.run.signal0", values[0]);
          recorder.value("coadd.run.signal1", values[1]);
        }
      }
      if (command.code == Code::ok && command.value)
        (void)rt::wait_external(rt::close_command(*command.value));
      if (services.value) (*services.value)->shutdown();
    }
  }

  recorder.resource("checks", static_cast<long long>(recorder.attempts()));
  recorder.resource("successes", static_cast<long long>(recorder.successes()));
  return recorder.finish();
}
