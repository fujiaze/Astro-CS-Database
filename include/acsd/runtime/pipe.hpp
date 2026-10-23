#pragma once
// Bounded strongly typed pipe with explicit broadcast.
//
// The bounded queue, ordered frontier and broadcast credit live in a single
// type-erased core (src/runtime/pipe.cpp).  The typed wrapper below is
// header-only, so any payload type with strong ownership can be used without a
// per-type explicit instantiation; the logical queue bytes are supplied by the
// wrapper (a Block<T> is charged its value bytes, not its handle size).
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/types.hpp>

#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace acsd::rt {

enum class EndKind { eof, failed, cancelled };
struct PipeEnd {
  EndKind kind = EndKind::eof;
  std::vector<Issue> issues;
};

template <class T>
using Delivery = std::variant<T, PipeEnd>;

template <class T>
class Pipe;

namespace detail {

class PipeCore;

struct ErasedDelivery {
  bool has_end = false;
  PipeEnd end;
  std::shared_ptr<const void> value;
};

// Logical queued bytes of one payload.  A block carries its value bytes; other
// payloads must be small owned DTOs and are charged their object size.
template <class T>
U64 pipe_payload_bytes(const T&) {
  return sizeof(T);
}
template <class T>
U64 pipe_payload_bytes(const Block<T>& block) {
  return static_cast<U64>(block.values.size()) * sizeof(T);
}

Result<std::shared_ptr<PipeCore>> make_pipe_core(InvocationContext,
                                                 Ref<InvocationPlan>, U32,
                                                 U32);
U32 pipe_region(const PipeCore&) noexcept;
U32 pipe_edge(const PipeCore&) noexcept;
Task<Done> pipe_send_erased(InvocationContext, std::shared_ptr<PipeCore>, Key,
                            std::shared_ptr<const void> value, U64 bytes);
Task<ErasedDelivery> pipe_receive_erased(InvocationContext,
                                         std::shared_ptr<PipeCore>);
Task<Done> pipe_close_erased(InvocationContext, std::shared_ptr<PipeCore>,
                             PipeEnd);
Task<Done> pipe_broadcast_erased(InvocationContext, Key,
                                 std::shared_ptr<const void> value, U64 bytes,
                                 std::vector<std::shared_ptr<PipeCore>>);

// Ordered pipes are producer-ordered from the minimum key of the first
// (epoch, cohort, stage) group (item 0).  A higher key that arrives first is
// parked without taking the reserved slot; it is delivered only after the
// minimum key.  When a stream is not contiguous from item 0 (a new cohort, or
// the executor declaring a frontier after a gap), the publication permit
// holder - not the pipe - must announce the next required key with
// pipe_expect(); otherwise the pipe cannot distinguish a gap from an
// out-of-order item and will wait (bounded backpressure, never reordering).
// The direct producer API is therefore for contiguous ordered streams; an
// executor that owns the plan window declares the frontier.
Result<Done> pipe_expect_core(PipeCore&, Key);
template <class T>
Result<Done> pipe_expect(const std::shared_ptr<Pipe<T>>& pipe, Key key);

}  // namespace detail

template <class T>
class Pipe {
 public:
  Pipe() noexcept = default;
  Pipe(Pipe&&) noexcept = default;
  Pipe& operator=(Pipe&&) noexcept = default;
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  ~Pipe() = default;

  Task<Done> send(InvocationContext ctx, Key key, T value) {
    Result<Done> failure;
    failure.code = Code::invalid_request;
    if (!impl_) return ready<Done>(ctx, std::move(failure));
    auto box = std::static_pointer_cast<const void>(
        std::make_shared<T>(std::move(value)));
    // Let overload resolution pick the Block<T> value-byte rule; an explicit
    // template argument would select the generic sizeof(T) overload.
    const U64 bytes =
        detail::pipe_payload_bytes(*std::static_pointer_cast<const T>(box));
    return detail::pipe_send_erased(std::move(ctx), impl_, key, std::move(box),
                                    bytes);
  }

  Task<Delivery<T>> receive(InvocationContext ctx) {
    Result<Delivery<T>> failure;
    failure.code = Code::invalid_request;
    if (!impl_) return ready<Delivery<T>>(ctx, std::move(failure));
    auto core = impl_;
    return then(ctx, detail::pipe_receive_erased(ctx, core),
                [ctx](detail::ErasedDelivery delivery) -> Task<Delivery<T>> {
                  Result<Delivery<T>> out;
                  out.code = Code::ok;
                  if (delivery.has_end) {
                    out.value = std::move(delivery.end);
                  } else if (delivery.value) {
                    out.value =
                        *std::static_pointer_cast<const T>(delivery.value);
                  } else {
                    out.code = Code::internal;
                    return ready<Delivery<T>>(ctx, std::move(out));
                  }
                  return ready<Delivery<T>>(ctx, std::move(out));
                });
  }

  Task<Done> close(InvocationContext ctx, PipeEnd end) {
    Result<Done> failure;
    failure.code = Code::invalid_request;
    if (!impl_) return ready<Done>(ctx, std::move(failure));
    return detail::pipe_close_erased(std::move(ctx), impl_, std::move(end));
  }

  U32 region_id() const noexcept {
    return impl_ ? detail::pipe_region(*impl_) : 0;
  }
  U32 edge_id() const noexcept {
    return impl_ ? detail::pipe_edge(*impl_) : 0;
  }

 private:
  explicit Pipe(std::shared_ptr<detail::PipeCore> impl) noexcept
      : impl_(std::move(impl)) {}
  std::shared_ptr<detail::PipeCore> impl_;
  template <class U>
  friend Result<std::shared_ptr<Pipe<U>>> make_pipe(InvocationContext,
                                                    Ref<InvocationPlan>, U32,
                                                    U32);
  template <class U>
  friend Task<Done> broadcast(InvocationContext, Key, U,
                              std::vector<std::shared_ptr<Pipe<U>>>);
  template <class U>
  friend Result<Done> detail::pipe_expect(const std::shared_ptr<Pipe<U>>&,
                                          Key);
};

namespace detail {
template <class T>
Result<Done> pipe_expect(const std::shared_ptr<Pipe<T>>& pipe, Key key) {
  if (!pipe || !pipe->impl_) {
    Result<Done> failure;
    failure.code = Code::invalid_request;
    return failure;
  }
  return pipe_expect_core(*pipe->impl_, key);
}
}  // namespace detail

template <class T>
Result<std::shared_ptr<Pipe<T>>> make_pipe(InvocationContext ctx,
                                           Ref<InvocationPlan> plan,
                                           U32 region_id, U32 edge_id) {
  Result<std::shared_ptr<Pipe<T>>> result;
  auto core = detail::make_pipe_core(ctx, std::move(plan), region_id, edge_id);
  if (core.code != Code::ok || !core.value) {
    result.code = core.code;
    return result;
  }
  result.code = Code::ok;
  result.value = std::shared_ptr<Pipe<T>>(new Pipe<T>(std::move(*core.value)));
  return result;
}

template <class T>
Task<Done> broadcast(InvocationContext ctx, Key key, T value,
                     std::vector<std::shared_ptr<Pipe<T>>> outputs) {
  Result<Done> failure;
  failure.code = Code::invalid_request;
  if (outputs.empty()) return ready<Done>(ctx, std::move(failure));
  std::vector<std::shared_ptr<detail::PipeCore>> cores;
  cores.reserve(outputs.size());
  for (auto& pipe : outputs) {
    if (!pipe || !pipe->impl_) return ready<Done>(ctx, std::move(failure));
    cores.push_back(pipe->impl_);
  }
  auto box = std::static_pointer_cast<const void>(
      std::make_shared<T>(std::move(value)));
  const U64 bytes =
      detail::pipe_payload_bytes(*std::static_pointer_cast<const T>(box));
  return detail::pipe_broadcast_erased(std::move(ctx), key, std::move(box),
                                       bytes, std::move(cores));
}

}  // namespace acsd::rt
