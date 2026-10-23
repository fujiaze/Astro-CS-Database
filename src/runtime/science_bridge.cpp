// The science Context bridge: the existing scientific free functions take
// acsd::Context (Resources/Cancellation/Storage).  This adapter serves that
// interface from the single runtime so that a module can compose the one
// existing implementation inside an admitted CPU execution segment without a
// second pool, ledger or storage root.
#include "internal.hpp"

#include <cstring>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::rt {
namespace detail {
namespace {

// Releases the BlockStore reference exactly once, when the last Array handle
// dies; the entry (and its capacity Lease) is erased at that point.
struct BridgeArrayReaper {
  std::weak_ptr<Services> services;
  Id id;
  void operator()(const Array* array) const {
    delete array;
    if (auto owner = services.lock())
      if (owner->blocks) owner->blocks->release_ref(id);
  }
};

// Releases the physical bytes before the capacity ticket, so the ledger never
// reports a committed allocation that has already been freed.
struct ArrayOwner {
  std::shared_ptr<ClaimImpl> claim;
  std::shared_ptr<const std::vector<std::byte>> data;
  ~ArrayOwner() = default;
};

class BridgeStorage final : public Storage {
 public:
  BridgeStorage(std::shared_ptr<Services> services, InvocationContext ctx,
                ArenaRef arena)
      : services_(std::move(services)), ctx_(std::move(ctx)),
        arena_(std::move(arena)) {}

  Result<ArrayRef> copy(Context&, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit,
                        Id index) override {
    Result<ArrayRef> result;
    if (!services_ || !services_->blocks) {
      result.code = Code::invalid_request;
      return result;
    }
    if (bytes.size() == 0) {
      result.code = Code::invalid_request;
      return result;
    }
    auto data = std::make_shared<std::vector<std::byte>>(bytes.begin(),
                                                         bytes.end());
    auto owner = std::make_shared<ArrayOwner>();
    owner->data = data;
    if (arena_) {
      ResourceVector required;
      required.host_bytes = bytes.size();
      auto claim = claim_capacity(arena_, required);
      if (claim.code != Code::ok || !claim.value) {
        result.code = claim.code;
        return result;
      }
      owner->claim = *claim.value;
      {
        std::lock_guard<std::mutex> lock(owner->claim->mutex);
        owner->claim->committed = true;
      }
      ledger_commit(owner->claim->arena->scope, required);
    }
    std::vector<U64> shape_vec(shape.begin(), shape.end());
    const std::string hex =
        model::hash_payload(dtype, shape_vec, unit, *data);
    const model::ArrayDescription description =
        model::describe_array("acsd.Array", dtype, shape_vec, unit, index, *data,
                              "acsd");
    (void)hex;
    Lease lease;
    lease.owner = std::static_pointer_cast<const void>(owner);
    Array array;
    array.identity = description.identity;
    array.assessment = model::make_assessment(Code::ok);
    array.assessment.complete = true;
    array.dtype = dtype;
    array.shape = shape_vec;
    array.unit = unit;
    array.index_id = index;
    services_->blocks->register_array(array.identity.object_id, data, dtype,
                                      shape_vec, lease);
    services_->blocks->add_ref(array.identity.object_id);
    result.code = Code::ok;
    result.value = std::shared_ptr<const Array>(
        new Array(array), BridgeArrayReaper{services_, array.identity.object_id});
    return result;
  }

  Result<ArrayView> read(Context&, ArrayRef array, BlockRequest range) const override {
    Result<ArrayView> result;
    if (!array || !services_ || !services_->blocks) {
      result.code = Code::invalid_request;
      return result;
    }
    U64 total = 1;
    for (U64 extent : array->shape) {
      if (extent == 0) {
        total = 0;
        break;
      }
      if (total > (~U64{0}) / extent) {
        result.code = Code::invalid_request;
        return result;
      }
      total *= extent;
    }
    if (range.begin > total || range.count > total - range.begin) {
      result.code = Code::invalid_request;
      return result;
    }
    auto bytes = services_->blocks->find(array->identity.object_id);
    if (!bytes) {
      result.code = Code::missing_dependency;
      return result;
    }
    const std::size_t item = model::dtype_size(array->dtype);
    if (item == 0) {
      result.code = Code::unsupported;
      return result;
    }
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    if (offset + length > bytes->size()) {
      result.code = Code::corrupt_product;
      return result;
    }
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(bytes->data() + offset, length);
    view.lease = services_->blocks->lease_of(array->identity.object_id);
    result.code = Code::ok;
    result.value = std::move(view);
    return result;
  }

  Result<ArrayRef> open(Context&, std::string, const Array&,
                        std::span<const BlockDescriptor>) override {
    // Product directories are still served by the product layer; the bridge
    // never guesses a product root.
    Result<ArrayRef> result;
    result.code = Code::unsupported;
    return result;
  }

 private:
  std::shared_ptr<Services> services_;
  InvocationContext ctx_;
  ArenaRef arena_;
};

class BridgeResources final : public Resources {
 public:
  BridgeResources(InvocationContext ctx, ArenaRef arena)
      : ctx_(std::move(ctx)), arena_(std::move(arena)) {}

  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    Result<Reservation> result;
    if (!arena_ || bytes == 0) {
      result.code = Code::invalid_request;
      return result;
    }
    if (ctx_.services && threads > ctx_.services->cpu_threads()) {
      result.code = Code::resource_exhausted;
      return result;
    }
    ResourceVector required;
    required.host_bytes = bytes;
    auto claim = claim_capacity(arena_, required);
    if (claim.code != Code::ok || !claim.value) {
      result.code = claim.code;
      return result;
    }
    Reservation reservation;
    reservation.bytes = bytes;
    reservation.threads = threads;
    reservation.lease.owner =
        std::static_pointer_cast<const void>(*claim.value);
    result.code = Code::ok;
    result.value = std::move(reservation);
    return result;
  }

  Result<U64> consume_work(U64 units) override {
    Result<U64> result;
    auto consumed = rt::consume_work(ctx_, units);
    if (consumed.code != Code::ok) {
      result.code = consumed.code;
      return result;
    }
    result.code = Code::ok;
    result.value = units;
    return result;
  }

  Usage usage() const override {
    Usage usage;
    const ScopeSnapshot snap = rt::snapshot(ctx_.scope);
    usage.work = snap.work;
    usage.peak_payload_bytes = snap.peak_allocated.host_bytes;
    usage.read_bytes = snap.read_bytes;
    usage.written_bytes = snap.written_bytes;
    usage.spill_bytes = snap.spill_written_bytes;
    usage.elapsed_seconds =
        static_cast<double>(snap.elapsed_ns) / 1'000'000'000.0;
    return usage;
  }

 private:
  InvocationContext ctx_;
  ArenaRef arena_;
};

class BridgeCancellation final : public Cancellation {
 public:
  explicit BridgeCancellation(std::shared_ptr<CancelState> cancel)
      : cancel_(std::move(cancel)) {}
  bool requested() const noexcept override {
    return cancel_ && cancel_->is_requested();
  }

 private:
  std::shared_ptr<CancelState> cancel_;
};

}  // namespace

struct ScienceContext::Impl {
  InvocationContext ctx;
  ArenaRef arena;
  std::shared_ptr<BridgeStorage> storage;
  std::shared_ptr<BridgeResources> resources;
  std::shared_ptr<BridgeCancellation> cancellation;
  std::unique_ptr<acsd::Context> context;
};

ScienceContext::ScienceContext() = default;
ScienceContext::ScienceContext(std::shared_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
ScienceContext::~ScienceContext() = default;

acsd::Context& ScienceContext::context() {
  return *impl_->context;
}

Result<std::shared_ptr<ScienceContext>> make_science_context(
    InvocationContext ctx, ArenaRef arena) {
  Result<std::shared_ptr<ScienceContext>> result;
  if (!ctx.services || !ctx.scope) {
    result.code = Code::invalid_request;
    return result;
  }
  ArenaRef effective = arena ? std::move(arena) : ctx.progress;
  if (!effective) effective = ctx.persistent;
  if (!effective) {
    // A module may run a purely in-memory composition without a pre-reserved
    // arena only when it does not need any storage capacity; the bridge still
    // serves reads from the BlockStore.
    effective = nullptr;
  }
  auto impl = std::make_shared<ScienceContext::Impl>();
  impl->ctx = ctx;
  impl->arena = effective;
  impl->storage = std::make_shared<BridgeStorage>(ctx.services, ctx, effective);
  impl->resources = std::make_shared<BridgeResources>(ctx, effective);
  impl->cancellation = std::make_shared<BridgeCancellation>(ctx.cancel);
  impl->context = std::make_unique<acsd::Context>(
      acsd::Context{*impl->resources, *impl->cancellation, *impl->storage});
  result.code = Code::ok;
  result.value = std::shared_ptr<ScienceContext>(new ScienceContext(std::move(impl)));
  return result;
}

}  // namespace detail
}  // namespace acsd::rt
