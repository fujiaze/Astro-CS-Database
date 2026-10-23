// Resource ledger: scope tree, arenas, capacity tickets and owned buffers.
//
// One ledger serves CPU tokens, I/O slots, host/pinned/device memory and spill.
// Every mutation checks all ancestors first and then commits the whole batch;
// a failure leaves no partial occupancy.  The physical allocator never runs
// while the ledger lock is held.
#include "internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <new>

namespace acsd::rt {
namespace {

using detail::ArenaImpl;
using detail::ClaimImpl;

struct ChainLocks {
  std::vector<ScopeRef> scopes;
  std::vector<std::unique_lock<std::mutex>> locks;

  bool fits(const ResourceVector& extra) const {
    for (std::size_t i = 0; i < scopes.size(); ++i) {
      const auto& scope = *scopes[i];
      const ResourceVector sum = scope.held + extra;
      if (!fits_within(sum, scope.limits.ceiling)) return false;
    }
    return true;
  }
  void add_held(const ResourceVector& v) {
    for (auto& scope : scopes) scope->held += v;
  }
  void sub_held(const ResourceVector& v) {
    for (auto& scope : scopes) scope->held = scope->held - v;
  }
  void add_claimed(const ResourceVector& v) {
    for (auto& scope : scopes) scope->claimed += v;
  }
  void sub_claimed(const ResourceVector& v) {
    for (auto& scope : scopes) scope->claimed = scope->claimed - v;
  }
};

ChainLocks lock_chain(const acsd::rt::ScopeRef& scope) {
  ChainLocks chain;
  chain.scopes = detail::scope_chain(scope);
  chain.locks.reserve(chain.scopes.size());
  for (auto& item : chain.scopes) chain.locks.emplace_back(item->mutex);
  return chain;
}

bool checked_mul(U64 a, U64 b, U64& out) {
  if (a != 0 && b > std::numeric_limits<U64>::max() / a) return false;
  out = a * b;
  return true;
}

}  // namespace

namespace detail {

std::vector<std::shared_ptr<ScopeState>> scope_chain(
    const acsd::rt::ScopeRef& scope) {
  std::vector<std::shared_ptr<ScopeState>> chain;
  for (auto current = scope; current; current = current->parent)
    chain.push_back(current);
  std::reverse(chain.begin(), chain.end());
  return chain;
}

struct ArenaAccess {
  static std::shared_ptr<ArenaImpl> impl(const Arena& arena) {
    return arena.impl_;
  }
};
struct ClaimAccess {
  static std::shared_ptr<ClaimImpl> impl(const Claim& claim) {
    return claim.impl_;
  }
};

}  // namespace detail

void ScopeState::add_child(const ScopeRef& child) {
  std::lock_guard<std::mutex> lock(mutex);
  children.erase(
      std::remove_if(children.begin(), children.end(),
                     [](const std::weak_ptr<ScopeState>& item) {
                       return item.expired();
                     }),
      children.end());
  children.push_back(child);
}

namespace detail {

Result<Done> ledger_reserve(const acsd::rt::ScopeRef& scope,
                            const ResourceVector& bytes) {
  if (!scope) {
    Result<Done> failure;
    failure.code = Code::invalid_request;
    return failure;
  }
  auto chain = lock_chain(scope);
  Result<Done> result;
  if (bytes.pinned_bytes > bytes.host_bytes) {
    result.code = Code::invalid_request;
    return result;
  }
  if (!chain.fits(bytes)) {
    result.code = Code::resource_exhausted;
    return result;
  }
  chain.add_held(bytes);
  result.code = Code::ok;
  result.value = Done{};
  return result;
}

void ledger_claim_committed(const acsd::rt::ScopeRef& scope,
                            const ResourceVector& bytes) {
  auto chain = lock_chain(scope);
  chain.add_claimed(bytes);
}

void ledger_commit(const acsd::rt::ScopeRef& scope,
                   const ResourceVector& bytes) {
  auto chain = lock_chain(scope);
  chain.sub_claimed(bytes);
  for (auto& item : chain.scopes) {
    item->allocated += bytes;
    if (!fits_within(item->allocated, item->peak_allocated))
      item->peak_allocated = item->allocated;
  }
}

void ledger_release_claimed(const acsd::rt::ScopeRef& scope,
                            const ResourceVector& bytes) {
  auto chain = lock_chain(scope);
  chain.sub_claimed(bytes);
}

void ledger_release_committed(const acsd::rt::ScopeRef& scope,
                              const ResourceVector& bytes) {
  auto chain = lock_chain(scope);
  for (auto& item : chain.scopes) item->allocated = item->allocated - bytes;
}

void ledger_return_held(const acsd::rt::ScopeRef& scope,
                        const ResourceVector& bytes) {
  auto chain = lock_chain(scope);
  chain.sub_held(bytes);
}

Result<Done> ledger_admit_work(const acsd::rt::ScopeRef& scope, U64 units) {
  Result<Done> result;
  if (!scope) {
    result.code = Code::invalid_request;
    return result;
  }
  auto chain = lock_chain(scope);
  for (auto& item : chain.scopes) {
    if (units == 0) continue;
    if (item->limits.max_work == 0) {
      result.code = Code::resource_exhausted;  // zero budget is a real zero
      return result;
    }
    if (item->work + item->work_reserved + units > item->limits.max_work) {
      result.code = Code::resource_exhausted;
      return result;
    }
  }
  if (units != 0)
    for (auto& item : chain.scopes) item->work_reserved += units;
  result.code = Code::ok;
  result.value = Done{};
  return result;
}

void ledger_consume_work(const acsd::rt::ScopeRef& scope, U64 units) {
  auto chain = lock_chain(scope);
  for (auto& item : chain.scopes) {
    if (units >= item->work_reserved)
      item->work_reserved = 0;
    else
      item->work_reserved -= units;
    item->work += units;
  }
  chain.scopes.back()->own_work += units;
}

void ledger_release_reserved_work(const acsd::rt::ScopeRef& scope,
                                  U64 units) {
  if (units == 0) return;
  auto chain = lock_chain(scope);
  for (auto& item : chain.scopes) {
    if (units >= item->work_reserved)
      item->work_reserved = 0;
    else
      item->work_reserved -= units;
  }
}

void ledger_account_bytes(const acsd::rt::ScopeRef& scope, U64 read,
                          U64 written, U64 spill) {
  if (!scope) return;
  auto chain = lock_chain(scope);
  for (auto& item : chain.scopes) {
    item->read_bytes += read;
    item->written_bytes += written;
    item->spill_written_bytes += spill;
  }
}

void services_notify_capacity(const acsd::rt::ScopeRef& scope) {
  if (!scope) return;
  if (auto owner = scope->owner.lock()) {
    // The executor resolves parked tasks; no lock of this translation unit is
    // held here.
    owner->notify_capacity();
  }
}

Allocation::~Allocation() {
  // Physical bytes always go before the capacity ticket is released.
  if (memory) destroy(memory, bytes, alignment);
  memory = nullptr;
  claim.reset();
}

ClaimImpl::~ClaimImpl() {
  if (released) return;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (released) return;
    released = true;
  }
  if (!arena) return;
  bool open = false;
  {
    std::lock_guard<std::mutex> lock(arena->mutex);
    open = arena->open;
    if (open) arena->available += bytes;
  }
  if (committed)
    ledger_release_committed(arena->scope, bytes);
  else
    ledger_release_claimed(arena->scope, bytes);
  if (!open) {
    ledger_return_held(arena->scope, bytes);
    services_notify_capacity(arena->scope);
  }
}

void Allocation::destroy(void* memory, std::size_t bytes,
                         std::size_t alignment) noexcept {
  (void)bytes;
#if defined(_WIN32)
  (void)alignment;
  ::operator delete(memory);
#else
  ::operator delete(memory, std::align_val_t(alignment));
#endif
}

// ---------------------------------------------------------------------------
// Execution capacity and scope status
// ---------------------------------------------------------------------------

Result<std::shared_ptr<detail::ClaimImpl>> claim_capacity(
    const ArenaRef& arena, const ResourceVector& bytes) {
  Result<std::shared_ptr<detail::ClaimImpl>> result;
  if (!arena) {
    result.code = Code::invalid_request;
    return result;
  }
  ResourceVector capacity;
  capacity.host_bytes = bytes.host_bytes;
  capacity.pinned_bytes = bytes.pinned_bytes;
  capacity.device_bytes = bytes.device_bytes;
  capacity.spill_bytes = bytes.spill_bytes;
  if (is_zero(capacity)) {
    result.code = Code::invalid_request;
    return result;
  }
  auto ticket = arena->claim(capacity);
  if (ticket.code != Code::ok || !ticket.value) {
    result.code = ticket.code;
    return result;
  }
  result.code = Code::ok;
  result.value = detail::ClaimAccess::impl(**ticket.value);
  return result;
}

Result<ScopeRef> make_child_scope(const ScopeRef& parent, Id local_id,
                                  ScopeLimits limits) {
  Result<ScopeRef> result;
  if (!parent || local_id.empty()) {
    result.code = Code::invalid_request;
    return result;
  }
  if (limits.ceiling.pinned_bytes > limits.ceiling.host_bytes ||
      !fits_within(limits.ceiling, parent->limits.ceiling)) {
    result.code = Code::invalid_request;
    return result;
  }
  auto scope = std::make_shared<ScopeState>();
  scope->local_id = std::move(local_id);
  scope->parent = parent;
  scope->limits = limits;
  scope->cancel = parent->cancel;
  scope->start = std::chrono::steady_clock::now();
  scope->owner = parent->owner;
  parent->add_child(scope);
  result.code = Code::ok;
  result.value = std::move(scope);
  return result;
}

void set_scope_status(const ScopeRef& scope, ScopeStatus status) {
  if (!scope) return;
  std::lock_guard<std::mutex> lock(scope->mutex);
  scope->status = status;
}

ScopeStatus scope_status(const ScopeRef& scope) {
  if (!scope) return ScopeStatus::closed;
  std::lock_guard<std::mutex> lock(scope->mutex);
  return scope->status;
}

void close_scope(const ScopeRef& scope) {
  if (!scope) return;
  set_scope_status(scope, ScopeStatus::closed);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Scope and arena
// ---------------------------------------------------------------------------

Result<ScopeRef> child_scope(InvocationContext parent, Id local_id,
                             ScopeLimits limits) {
  Result<ScopeRef> result;
  if (!parent.services || !parent.scope || local_id.empty()) {
    result.code = Code::invalid_request;
    return result;
  }
  if (limits.ceiling.pinned_bytes > limits.ceiling.host_bytes) {
    result.code = Code::invalid_request;
    return result;
  }
  if (!fits_within(limits.ceiling, parent.scope->limits.ceiling)) {
    // A child may only narrow the ceiling of its parent.
    result.code = Code::invalid_request;
    return result;
  }
  auto scope = detail::make_child_scope(parent.scope, std::move(local_id), limits);
  if (scope.code != Code::ok) {
    result.code = scope.code;
    return result;
  }
  (*scope.value)->cancel = parent.cancel ? parent.cancel : (*scope.value)->cancel;
  result.code = Code::ok;
  result.value = std::move(scope.value);
  return result;
}

namespace detail {

Result<ArenaRef> make_arena(const ScopeRef& scope, ResourceVector available) {
  Result<ArenaRef> result;
  if (!scope || is_zero(available)) {
    result.code = Code::invalid_request;
    return result;
  }
  auto impl = std::make_shared<ArenaImpl>();
  impl->scope = scope;
  impl->available = available;
  result.code = Code::ok;
  result.value = std::shared_ptr<Arena>(new Arena(std::move(impl)));
  return result;
}

}  // namespace detail

Result<ArenaRef> reserve(InvocationContext ctx, ResourceVector bytes) {
  Result<ArenaRef> result;
  if (!ctx.scope || !ctx.services) {
    result.code = Code::invalid_request;
    return result;
  }
  if (is_zero(bytes)) {
    result.code = Code::invalid_request;
    return result;
  }
  auto reserved = detail::ledger_reserve(ctx.scope, bytes);
  if (reserved.code != Code::ok) {
    result.code = reserved.code;
    return result;
  }
  return detail::make_arena(ctx.scope, bytes);
}

Arena::Arena(std::shared_ptr<detail::ArenaImpl> impl) noexcept
    : impl_(std::move(impl)) {}

Arena::~Arena() {
  if (!impl_) return;
  close();
}

ResourceVector Arena::available() const noexcept {
  if (!impl_) return {};
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->available;
}

Result<std::shared_ptr<Claim>> Arena::claim(ResourceVector bytes) {
  Result<std::shared_ptr<Claim>> result;
  if (!impl_) {
    result.code = Code::invalid_request;
    return result;
  }
  if (bytes.pinned_bytes > bytes.host_bytes) {
    result.code = Code::invalid_request;
    return result;
  }
  std::unique_lock<std::mutex> lock(impl_->mutex);
  if (!impl_->open) {
    result.code = Code::invalid_request;
    return result;
  }
  if (!fits_within(bytes, impl_->available)) {
    result.code = Code::resource_exhausted;
    return result;
  }
  auto ticket = std::make_shared<detail::ClaimImpl>();
  ticket->arena = impl_;
  ticket->bytes = bytes;
  impl_->available = impl_->available - bytes;
  lock.unlock();
  detail::ledger_claim_committed(impl_->scope, bytes);
  result.code = Code::ok;
  result.value = std::shared_ptr<Claim>(new Claim(std::move(ticket)));
  return result;
}

void Arena::close() noexcept {
  if (!impl_) return;
  ResourceVector unused;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->open) return;
    impl_->open = false;
    unused = impl_->available;
    impl_->available = {};
  }
  if (!is_zero(unused)) {
    detail::ledger_return_held(impl_->scope, unused);
    detail::services_notify_capacity(impl_->scope);
  }
}

// ---------------------------------------------------------------------------
// Claim
// ---------------------------------------------------------------------------

Claim::Claim(std::shared_ptr<detail::ClaimImpl> impl) noexcept
    : impl_(std::move(impl)) {}

Claim::Claim(Claim&& other) noexcept : impl_(std::move(other.impl_)) {}

Claim& Claim::operator=(Claim&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

Claim::~Claim() = default;

ResourceVector Claim::bytes() const noexcept {
  return impl_ ? impl_->bytes : ResourceVector{};
}

// ---------------------------------------------------------------------------
// Consume work and snapshots
// ---------------------------------------------------------------------------

Result<Done> consume_work(InvocationContext ctx, U64 units) {
  Result<Done> result;
  if (!ctx.scope || !ctx.cancel) {
    result.code = Code::invalid_request;
    return result;
  }
  if (ctx.cancel->is_requested()) {
    result.code = Code::cancelled;
    return result;
  }
  if (ctx.scope->work_upper_enforced &&
      ctx.scope->own_work + units > ctx.scope->work_upper) {
    result.code = Code::resource_exhausted;
    return result;
  }
  detail::ledger_consume_work(ctx.scope, units);
  result.code = Code::ok;
  result.value = Done{};
  return result;
}

ScopeSnapshot snapshot(const ScopeRef& scope) noexcept {
  ScopeSnapshot out;
  if (!scope) return out;
  std::lock_guard<std::mutex> lock(scope->mutex);
  out.held = scope->held;
  out.claimed = scope->claimed;
  out.allocated = scope->allocated;
  out.peak_allocated = scope->peak_allocated;
  out.work = scope->work;
  out.read_bytes = scope->read_bytes;
  out.written_bytes = scope->written_bytes;
  out.spill_written_bytes = scope->spill_written_bytes;
  out.cleanup_work = scope->cleanup_work;
  out.elapsed_ns = static_cast<U64>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now() - scope->start)
          .count());
  out.cpu_ns = std::nullopt;
  return out;
}

// ---------------------------------------------------------------------------
// Buffers and immutable blocks
// ---------------------------------------------------------------------------

template <class T>
Result<Buffer<T>> allocate(InvocationContext ctx, ArenaRef arena,
                           BlockRequest range, Ref<Index> index, Unit unit,
                           Id model_hash) {
  Result<Buffer<T>> result;
  if (!arena) {
    result.code = Code::invalid_request;
    return result;
  }
  if (range.count == 0) {
    result.code = Code::invalid_request;
    return result;
  }
  U64 bytes = 0;
  if (!checked_mul(range.count, sizeof(T), bytes) ||
      bytes > std::numeric_limits<std::size_t>::max()) {
    result.code = Code::resource_exhausted;
    return result;
  }
  ResourceVector required;
  required.host_bytes = bytes;
  auto ticket = arena->claim(required);
  if (ticket.code != Code::ok || !ticket.value) {
    result.code = ticket.code;
    return result;
  }
  constexpr std::size_t alignment = 64;
  void* memory = nullptr;
  try {
    memory = ::operator new(static_cast<std::size_t>(bytes),
                            std::align_val_t(alignment));
  } catch (...) {
    result.code = Code::resource_exhausted;
    return result;
  }
  auto owner = std::make_shared<detail::Allocation>();
  owner->memory = memory;
  owner->bytes = static_cast<std::size_t>(bytes);
  owner->alignment = alignment;
  owner->claim = detail::ClaimAccess::impl(**ticket.value);
  {
    std::lock_guard<std::mutex> lock(owner->claim->mutex);
    owner->claim->committed = true;
  }
  detail::ledger_commit(detail::ArenaAccess::impl(*arena)->scope, required);

  Buffer<T> buffer;
  buffer.range = range;
  buffer.index = std::move(index);
  buffer.unit = std::move(unit);
  buffer.model_hash = std::move(model_hash);
  buffer.values = std::span<T>(static_cast<T*>(memory), range.count);
  buffer.lease.owner = std::static_pointer_cast<const void>(owner);
  result.code = Code::ok;
  result.value = std::move(buffer);
  return result;
}

template Result<Buffer<double>> allocate<double>(InvocationContext, ArenaRef,
                                                 BlockRequest, Ref<Index>, Unit,
                                                 Id);
template Result<Buffer<float>> allocate<float>(InvocationContext, ArenaRef,
                                               BlockRequest, Ref<Index>, Unit,
                                               Id);
template Result<Buffer<std::uint16_t>> allocate<std::uint16_t>(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id);
template Result<Buffer<std::uint32_t>> allocate<std::uint32_t>(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id);
template Result<Buffer<std::uint64_t>> allocate<std::uint64_t>(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id);

template <class T>
Result<Block<T>> freeze(Buffer<T>&& buffer) {
  Result<Block<T>> result;
  Block<T> block;
  block.range = buffer.range;
  block.index = std::move(buffer.index);
  block.unit = std::move(buffer.unit);
  block.model_hash = std::move(buffer.model_hash);
  block.values = std::span<const T>(buffer.values.data(), buffer.values.size());
  block.lease = std::move(buffer.lease);
  buffer.values = {};
  result.code = Code::ok;
  result.value = std::move(block);
  return result;
}

template Result<Block<double>> freeze<double>(Buffer<double>&&);
template Result<Block<float>> freeze<float>(Buffer<float>&&);
template Result<Block<std::uint16_t>> freeze<std::uint16_t>(
    Buffer<std::uint16_t>&&);
template Result<Block<std::uint32_t>> freeze<std::uint32_t>(
    Buffer<std::uint32_t>&&);
template Result<Block<std::uint64_t>> freeze<std::uint64_t>(
    Buffer<std::uint64_t>&&);
template Result<Buffer<std::uint8_t>> allocate<std::uint8_t>(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id);
template Result<Block<std::uint8_t>> freeze<std::uint8_t>(Buffer<std::uint8_t>&&);
template Result<Buffer<std::byte>> allocate<std::byte>(
    InvocationContext, ArenaRef, BlockRequest, Ref<Index>, Unit, Id);
template Result<Block<std::byte>> freeze<std::byte>(Buffer<std::byte>&&);

namespace detail {

Result<Buffer<std::byte>> allocate_bytes(InvocationContext ctx, ArenaRef arena,
                                         U64 bytes) {
  if (bytes == 0) {
    Result<Buffer<std::byte>> result;
    result.code = Code::invalid_request;
    return result;
  }
  return rt::allocate<std::byte>(ctx, std::move(arena),
                                 BlockRequest{U64{0}, bytes}, nullptr, Unit{},
                                 Id{});
}

Result<Block<std::byte>> freeze_bytes(Buffer<std::byte>&& buffer) {
  return rt::freeze<std::byte>(std::move(buffer));
}

}  // namespace detail

}  // namespace acsd::rt
