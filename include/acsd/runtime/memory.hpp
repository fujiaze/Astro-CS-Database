#pragma once
// Resource scopes, capacity tickets, owned buffers and immutable blocks.
//
// The capacity rules (reserve -> claim -> commit -> release, close semantics,
// peak accounting) are fixed by FOUNDATION_API.md and SCHEDULER.md.  A bare
// span never crosses an asynchronous boundary: a Block carries its Lease.
#include <acsd/runtime/types.hpp>

#include <span>
#include <utility>

namespace acsd::rt {

namespace detail {
struct ArenaImpl;
struct ClaimImpl;
struct ArenaAccess;
struct ClaimAccess;
Result<ArenaRef> make_arena(const ScopeRef&, ResourceVector);
}  // namespace detail

template <class T>
struct Buffer;
template <class T>
struct Block;

Result<ScopeRef> child_scope(InvocationContext parent, Id local_id,
                             ScopeLimits limits);
Result<ArenaRef> reserve(InvocationContext, ResourceVector);
Result<Done> consume_work(InvocationContext, U64 units);
ScopeSnapshot snapshot(const ScopeRef&) noexcept;

// A capacity ticket.  claimed -> committed -> released, or claimed -> released.
class Claim {
 public:
  Claim() noexcept = default;
  Claim(Claim&&) noexcept;
  Claim& operator=(Claim&&) noexcept;
  Claim(const Claim&) = delete;
  Claim& operator=(const Claim&) = delete;
  ~Claim();
  ResourceVector bytes() const noexcept;

 private:
  explicit Claim(std::shared_ptr<detail::ClaimImpl> impl) noexcept;
  std::shared_ptr<detail::ClaimImpl> impl_;
  friend class Arena;
  friend struct detail::ClaimAccess;
  template <class T>
  friend Result<Buffer<T>> allocate(InvocationContext, ArenaRef, BlockRequest,
                                    Ref<Index>, Unit, Id);
};

class Arena {
 public:
  Arena() noexcept = default;
  Arena(Arena&&) noexcept = default;
  Arena& operator=(Arena&&) noexcept = default;
  Arena(const Arena&) = delete;
  Arena& operator=(const Arena&) = delete;
  ~Arena();

  ResourceVector available() const noexcept;
  Result<std::shared_ptr<Claim>> claim(ResourceVector);
  void close() noexcept;

 private:
  explicit Arena(std::shared_ptr<detail::ArenaImpl> impl) noexcept;
  std::shared_ptr<detail::ArenaImpl> impl_;
  friend Result<ArenaRef> reserve(InvocationContext, ResourceVector);
  friend class Claim;
  friend struct detail::ArenaAccess;
  friend Result<ArenaRef> detail::make_arena(const ScopeRef&, ResourceVector);
};

template <class T>
struct Block {
  BlockRequest range{};
  Ref<Index> index;
  Unit unit;
  Id model_hash;
  std::span<const T> values;
  Lease lease;
};

template <class T>
struct Buffer {
  Buffer() = default;
  Buffer(Buffer&&) noexcept = default;
  Buffer& operator=(Buffer&&) noexcept = default;
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  BlockRequest range{};
  Ref<Index> index;
  Unit unit;
  Id model_hash;
  std::span<T> values;
  Lease lease;
};

template <class T>
Result<Buffer<T>> allocate(InvocationContext, ArenaRef, BlockRequest,
                           Ref<Index>, Unit, Id model_hash);
template <class T>
Result<Block<T>> freeze(Buffer<T>&&);

struct ArrayLayout {
  DType dtype = DType::f64;
  std::vector<U64> shape;
  Unit unit;
  Ref<Index> index;
};

struct BuildSpec {
  ArrayLayout layout;
  Placement placement = Placement::memory;
  U64 buffer_bytes = 0, metadata_bytes = 0;
};

}  // namespace acsd::rt
