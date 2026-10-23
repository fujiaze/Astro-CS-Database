#pragma once
// Bounded strongly typed pipe with explicit broadcast.
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/types.hpp>

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
class Pipe {
 public:
  Pipe() noexcept = default;
  Pipe(Pipe&&) noexcept = default;
  Pipe& operator=(Pipe&&) noexcept = default;
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  ~Pipe() = default;

  Task<Done> send(InvocationContext, Key, T);
  Task<Delivery<T>> receive(InvocationContext);
  Task<Done> close(InvocationContext, PipeEnd);
  U32 region_id() const noexcept;
  U32 edge_id() const noexcept;

 private:
  struct Impl;
  explicit Pipe(std::shared_ptr<Impl> impl) noexcept;
  std::shared_ptr<Impl> impl_;
  template <class U>
  friend Result<std::shared_ptr<Pipe<U>>> make_pipe(InvocationContext,
                                                    Ref<InvocationPlan>, U32,
                                                    U32);
  template <class U>
  friend Task<Done> broadcast(InvocationContext, Key, U,
                              std::vector<std::shared_ptr<Pipe<U>>>);
};

template <class T>
Result<std::shared_ptr<Pipe<T>>> make_pipe(InvocationContext,
                                           Ref<InvocationPlan>, U32 region_id,
                                           U32 edge_id);

template <class T>
Task<Done> broadcast(InvocationContext, Key, T,
                     std::vector<std::shared_ptr<Pipe<T>>> outputs);

}  // namespace acsd::rt
