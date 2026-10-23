// Bounded pipe core: one type-erased queue with an ordered frontier, explicit
// broadcast credit and three close states.  The typed wrapper in pipe.hpp owns
// the payload boxes; this file never touches a scientific algorithm.
#include "internal.hpp"

#include <algorithm>
#include <mutex>

namespace acsd::rt {
namespace detail {
namespace {

struct Item {
  Key key;
  std::shared_ptr<const void> value;
  U64 bytes = 0;
};

struct WaitSend {
  Item item;
  std::shared_ptr<TaskNode> node;
};

struct BroadcastGroup {
  Key key;
  Item item;
  std::vector<std::shared_ptr<PipeCore>> pipes;
  std::shared_ptr<TaskNode> node;
};

Assessment failure_assessment(Code code, std::string reason) {
  Assessment assessment;
  assessment.complete = false;
  assessment.validity = Validity::invalid;
  assessment.execution =
      code == Code::cancelled ? ExecutionState::cancelled : ExecutionState::failed;
  Issue issue;
  issue.code = code;
  issue.scope.frame = "pipe";
  issue.origin = "runtime.pipe";
  issue.reason = std::move(reason);
  issue.impact = "the bounded pipe cannot deliver the payload";
  issue.retry_condition = "the caller corrects the key order or capacity";
  assessment.issues.push_back(std::move(issue));
  return assessment;
}

std::mutex& broadcast_mutex() {
  static std::mutex mutex;
  return mutex;
}
std::vector<std::shared_ptr<BroadcastGroup>>& broadcast_groups() {
  static std::vector<std::shared_ptr<BroadcastGroup>> groups;
  return groups;
}

// One pipe binding per (plan instance, region, edge).  A weak plan reference
// lets a freed plan's address be reused without a false duplicate.
std::mutex& binding_mutex() {
  static std::mutex mutex;
  return mutex;
}
using BindingKey = std::tuple<const void*, U32, U32>;
std::map<BindingKey, std::weak_ptr<const InvocationPlan>>& bindings() {
  static std::map<BindingKey, std::weak_ptr<const InvocationPlan>> map;
  return map;
}

bool bind_once(const Ref<InvocationPlan>& plan, U32 region, U32 edge) {
  std::lock_guard<std::mutex> lock(binding_mutex());
  auto& map = bindings();
  const BindingKey key{plan.get(), region, edge};
  auto it = map.find(key);
  if (it != map.end()) {
    if (!it->second.expired()) return false;
    map.erase(it);
  }
  map.emplace(key, std::weak_ptr<const InvocationPlan>(plan));
  return true;
}

void unbind(const Ref<InvocationPlan>& plan, U32 region, U32 edge) {
  std::lock_guard<std::mutex> lock(binding_mutex());
  bindings().erase(BindingKey{plan.get(), region, edge});
}

Key next_key(const Key& key) {
  Key next = key;
  if (next.item != ~U64{0}) ++next.item;
  return next;
}

}  // namespace

class PipeCore {
 public:
  PipeCore(U32 region, U32 edge, PipeSpec spec)
      : region_id(region), edge_id(edge), spec(spec) {}

  U32 region_id = 0;
  U32 edge_id = 0;
  PipeSpec spec;

  std::mutex mutex;
  std::deque<Item> ready;
  U64 queued_bytes = 0;
  U32 queued_items = 0;
  std::deque<WaitSend> waiting;
  std::deque<std::shared_ptr<TaskNode>> receivers;
  std::vector<std::weak_ptr<BroadcastGroup>> waiting_broadcasts;

  Key expected{};
  bool have_expected = false;
  bool closed = false;
  bool end_delivered = false;
  PipeEnd end;

  bool has_credit(U64 bytes) const {
    if (spec.max_items == 0 || spec.max_queued_bytes == 0) return false;
    if (queued_items + 1 > spec.max_items) return false;
    return bytes <= spec.max_queued_bytes - queued_bytes;
  }
  U64 wait_cap() const {
    // Parked producers are plan-window bounded (SCHEDULER).  The pipe holds a
    // bounded reorder allowance of one delivery window even when max_items is
    // one: a high key may wait without taking the reserved slot.
    const U64 window = spec.max_items == 0 ? 1 : spec.max_items;
    return window * 2;
  }

  // Moves the smallest parked contiguous key into the delivery queue.  Called
  // with the pipe mutex held; completions are appended to `completed`.
  void promote(std::vector<std::shared_ptr<TaskNode>>& completed) {
    bool progress = true;
    while (progress) {
      progress = false;
      for (auto it = waiting.begin(); it != waiting.end(); ++it) {
        const bool admissible = !have_expected ||
                                it->item.key == expected ||
                                spec.order == PipeOrder::unordered;
        if (!admissible) continue;
        if (!has_credit(it->item.bytes)) continue;
        if (spec.order == PipeOrder::ordered) {
          expected = next_key(it->item.key);
          have_expected = true;
        }
        ready.push_back(std::move(it->item));
        ++queued_items;
        queued_bytes += ready.back().bytes;
        completed.push_back(std::move(it->node));
        waiting.erase(it);
        progress = true;
        break;
      }
    }
  }

  // Pops deliverable items into waiting receivers.  Called with the mutex held;
  // the receiver nodes are completed after the caller releases the lock.
  void dispatch(std::vector<std::shared_ptr<TaskNode>>& receivers_out,
                std::vector<ErasedDelivery>& deliveries,
                std::vector<std::shared_ptr<TaskNode>>& end_nodes) {
    while (!receivers.empty() && !ready.empty()) {
      Item item = std::move(ready.front());
      ready.pop_front();
      --queued_items;
      queued_bytes -= item.bytes;
      ErasedDelivery delivery;
      delivery.value = std::move(item.value);
      receivers_out.push_back(std::move(receivers.front()));
      receivers.pop_front();
      deliveries.push_back(std::move(delivery));
    }
    if (closed && ready.empty() && !end_delivered) {
      end_delivered = true;
      while (!receivers.empty()) {
        end_nodes.push_back(std::move(receivers.front()));
        receivers.pop_front();
      }
    }
  }
};

namespace {

void complete_node(const std::shared_ptr<TaskNode>& node, Code code,
                   std::shared_ptr<void> value, const char* reason = "") {
  if (!node || node->terminal.load(std::memory_order_acquire)) return;
  Assessment assessment;
  if (code != Code::ok) assessment = failure_assessment(code, reason);
  node_complete(node, code, std::move(assessment), Usage{}, {}, std::move(value));
}

void complete_delivery(const std::shared_ptr<TaskNode>& node,
                       ErasedDelivery delivery) {
  complete_node(node, Code::ok,
                box_value<ErasedDelivery>(std::move(delivery)));
}

void release_credit_and_dispatch(PipeCore& pipe) {
  std::vector<std::shared_ptr<TaskNode>> receiver_nodes;
  std::vector<ErasedDelivery> deliveries;
  std::vector<std::shared_ptr<TaskNode>> end_nodes;
  std::vector<std::shared_ptr<TaskNode>> completed_sends;
  {
    std::lock_guard<std::mutex> lock(pipe.mutex);
    pipe.promote(completed_sends);
    pipe.dispatch(receiver_nodes, deliveries, end_nodes);
  }
  for (auto& node : completed_sends)
    complete_node(node, Code::ok, box_value<Done>(Done{}));
  for (std::size_t i = 0; i < receiver_nodes.size(); ++i)
    complete_delivery(receiver_nodes[i], std::move(deliveries[i]));
  for (auto& node : end_nodes) {
    ErasedDelivery end;
    end.has_end = true;
    end.end = pipe.end;
    complete_delivery(node, std::move(end));
  }
}

// Attempts to satisfy every parked broadcast group.  Called without holding a
// pipe or ledger lock.
void try_fulfill_broadcasts() {
  std::vector<std::shared_ptr<BroadcastGroup>> done;
  {
    std::lock_guard<std::mutex> global(broadcast_mutex());
    auto& groups = broadcast_groups();
    for (auto& group : groups) {
      if (!group || !group->node) continue;
      if (group->node->terminal.load(std::memory_order_acquire)) {
        done.push_back(group);
        continue;
      }
      bool closed_branch = false;
      bool credit = true;
      for (auto& pipe : group->pipes) {
        std::lock_guard<std::mutex> lock(pipe->mutex);
        closed_branch = closed_branch || pipe->closed;
        if (!pipe->closed && !pipe->has_credit(group->item.bytes))
          credit = false;
      }
      if (closed_branch) {
        complete_node(group->node, Code::invalid_request, nullptr,
                      "a broadcast branch was closed while the group waited");
        done.push_back(group);
        continue;
      }
      if (!credit) continue;
      for (auto& pipe : group->pipes) {
        std::lock_guard<std::mutex> lock(pipe->mutex);
        Item item = group->item;
        pipe->ready.push_back(std::move(item));
        ++pipe->queued_items;
        pipe->queued_bytes += group->item.bytes;
        if (pipe->spec.order == PipeOrder::ordered) {
          pipe->have_expected = true;
          pipe->expected = next_key(group->key);
        }
      }
      complete_node(group->node, Code::ok, box_value<Done>(Done{}));
      done.push_back(group);
    }
    groups.erase(std::remove_if(groups.begin(), groups.end(),
                                [&done](const std::shared_ptr<BroadcastGroup>& g) {
                                  return std::find(done.begin(), done.end(), g) !=
                                         done.end();
                                }),
                 groups.end());
  }
  for (auto& group : done) {
    if (!group) continue;
    for (auto& pipe : group->pipes) {
      if (pipe) release_credit_and_dispatch(*pipe);
    }
    group->pipes.clear();
  }
}

void cancel_waiters(PipeCore& pipe) {
  std::vector<std::shared_ptr<TaskNode>> senders;
  std::vector<std::shared_ptr<TaskNode>> receivers;
  {
    std::lock_guard<std::mutex> lock(pipe.mutex);
    for (auto& waiter : pipe.waiting) senders.push_back(std::move(waiter.node));
    pipe.waiting.clear();
    for (auto& receiver : pipe.receivers) receivers.push_back(std::move(receiver));
    pipe.receivers.clear();
  }
  for (auto& node : senders)
    complete_node(node, Code::cancelled, nullptr, "the command was cancelled");
  for (auto& node : receivers) {
    ErasedDelivery end;
    end.has_end = true;
    end.end.kind = EndKind::cancelled;
    Issue issue;
    issue.code = Code::cancelled;
    issue.origin = "runtime.pipe";
    issue.reason = "the command was cancelled while waiting for a payload";
    end.end.issues.push_back(std::move(issue));
    complete_delivery(node, std::move(end));
  }
}

}  // namespace

Result<std::shared_ptr<PipeCore>> make_pipe_core(InvocationContext ctx,
                                                 Ref<InvocationPlan> plan,
                                                 U32 region_id,
                                                 U32 edge_id) {
  Result<std::shared_ptr<PipeCore>> result;
  if (!ctx.services || !plan) {
    result.code = Code::invalid_request;
    return result;
  }
  if (ctx.region && *ctx.region != region_id) {
    result.code = Code::invalid_request;
    return result;
  }
  const RegionSpec* region = nullptr;
  for (const auto& candidate : plan->regions)
    if (candidate.id == region_id) region = &candidate;
  if (!region) {
    result.code = Code::invalid_request;
    return result;
  }
  const EdgeSpec* edge = nullptr;
  for (const auto& candidate : region->edges)
    if (candidate.id == edge_id) edge = &candidate;
  if (!edge) {
    result.code = Code::invalid_request;
    return result;
  }
  if (edge->window.max_items == 0 || edge->window.max_queued_bytes == 0) {
    result.code = Code::invalid_request;
    return result;
  }
  if (!bind_once(plan, region_id, edge_id)) {
    result.code = Code::invalid_request;
    return result;
  }
  auto core = std::make_shared<PipeCore>(region_id, edge_id, edge->window);
  if (ctx.cancel) {
    std::weak_ptr<PipeCore> weak = core;
    ctx.cancel->add_watcher([weak]() {
      if (auto pipe = weak.lock()) cancel_waiters(*pipe);
    });
  }
  result.code = Code::ok;
  result.value = std::move(core);
  return result;
}

Result<Done> pipe_expect_core(PipeCore& pipe, Key key) {
  Result<Done> result;
  std::vector<std::shared_ptr<TaskNode>> completed_sends;
  std::vector<std::shared_ptr<TaskNode>> receiver_nodes;
  std::vector<ErasedDelivery> deliveries;
  std::vector<std::shared_ptr<TaskNode>> end_nodes;
  {
    std::lock_guard<std::mutex> lock(pipe.mutex);
    if (pipe.closed) {
      result.code = Code::invalid_request;
      return result;
    }
    if (pipe.spec.order == PipeOrder::unordered) {
      result.code = Code::invalid_request;
      return result;
    }
    if (!pipe.have_expected || pipe.expected < key) {
      pipe.expected = key;
      pipe.have_expected = true;
    }
    pipe.promote(completed_sends);
    pipe.dispatch(receiver_nodes, deliveries, end_nodes);
  }
  for (auto& node : completed_sends)
    complete_node(node, Code::ok, box_value<Done>(Done{}));
  for (std::size_t i = 0; i < receiver_nodes.size(); ++i)
    complete_delivery(receiver_nodes[i], std::move(deliveries[i]));
  for (auto& node : end_nodes) {
    ErasedDelivery end;
    end.has_end = true;
    end.end = pipe.end;
    complete_delivery(node, std::move(end));
  }
  result.code = Code::ok;
  result.value = Done{};
  return result;
}

U32 pipe_region(const PipeCore& pipe) noexcept { return pipe.region_id; }
U32 pipe_edge(const PipeCore& pipe) noexcept { return pipe.edge_id; }

Task<Done> pipe_send_erased(InvocationContext ctx, std::shared_ptr<PipeCore> pipe,
                            Key key, std::shared_ptr<const void> value,
                            U64 bytes) {
  auto node = make_node(ctx, TaskSpec{});
  if (!pipe) {
    complete_node(node, Code::invalid_request, nullptr, "no pipe");
    return wrap_task<Done>(std::move(node));
  }
  std::vector<std::shared_ptr<TaskNode>> completed_sends;
  std::vector<std::shared_ptr<TaskNode>> receiver_nodes;
  std::vector<ErasedDelivery> deliveries;
  std::vector<std::shared_ptr<TaskNode>> end_nodes;
  Code failure = Code::ok;
  const char* reason = "";
  {
    std::lock_guard<std::mutex> lock(pipe->mutex);
    if (pipe->closed) {
      failure = Code::invalid_request;
      reason = "the pipe is already closed";
    } else if (bytes > pipe->spec.max_queued_bytes) {
      // A payload larger than the declared pipe byte window can never obtain
      // credit; reject it by name instead of parking the producer forever.
      failure = Code::resource_exhausted;
      reason =
          "the payload exceeds the declared max_queued_bytes window of the pipe";
    } else if (pipe->spec.order == PipeOrder::ordered &&
               pipe->have_expected && key < pipe->expected) {
      failure = Code::invalid_request;
      reason = "the ordered key is older than the required frontier";
    } else {
      if (pipe->spec.order == PipeOrder::ordered && !pipe->have_expected) {
        pipe->have_expected = true;
        pipe->expected = Key{key.epoch, key.cohort, key.stage, 0};
      }
      const bool admissible =
          pipe->spec.order == PipeOrder::unordered ||
          key == pipe->expected;
      if (admissible && pipe->has_credit(bytes)) {
        if (pipe->spec.order == PipeOrder::ordered) {
          pipe->have_expected = true;
          pipe->expected = next_key(key);
        }
        pipe->ready.push_back(Item{key, std::move(value), bytes});
        ++pipe->queued_items;
        pipe->queued_bytes += bytes;
        completed_sends.push_back(node);
        pipe->dispatch(receiver_nodes, deliveries, end_nodes);
      } else if (pipe->waiting.size() < pipe->wait_cap()) {
        pipe->waiting.push_back(WaitSend{Item{key, std::move(value), bytes}, node});
        return wrap_task<Done>(std::move(node));
      } else {
        failure = Code::resource_exhausted;
        reason = "the bounded pipe reorder window is full";
      }
    }
  }
  if (failure != Code::ok) {
    complete_node(node, failure, nullptr, reason);
    return wrap_task<Done>(std::move(node));
  }
  for (auto& item : completed_sends)
    complete_node(item, Code::ok, box_value<Done>(Done{}));
  for (std::size_t i = 0; i < receiver_nodes.size(); ++i)
    complete_delivery(receiver_nodes[i], std::move(deliveries[i]));
  for (auto& item : end_nodes) {
    ErasedDelivery end;
    end.has_end = true;
    end.end = pipe->end;
    complete_delivery(item, std::move(end));
  }
  return wrap_task<Done>(std::move(node));
}

Task<ErasedDelivery> pipe_receive_erased(InvocationContext ctx,
                                         std::shared_ptr<PipeCore> pipe) {
  auto node = make_node(ctx, TaskSpec{});
  if (!pipe) {
    complete_node(node, Code::invalid_request, nullptr, "no pipe");
    return wrap_task<ErasedDelivery>(std::move(node));
  }
  std::vector<std::shared_ptr<TaskNode>> completed_sends;
  std::vector<std::shared_ptr<TaskNode>> receiver_nodes;
  std::vector<ErasedDelivery> deliveries;
  std::vector<std::shared_ptr<TaskNode>> end_nodes;
  {
    std::lock_guard<std::mutex> lock(pipe->mutex);
    if (!pipe->ready.empty()) {
      Item item = std::move(pipe->ready.front());
      pipe->ready.pop_front();
      --pipe->queued_items;
      pipe->queued_bytes -= item.bytes;
      pipe->promote(completed_sends);
      pipe->dispatch(receiver_nodes, deliveries, end_nodes);
      ErasedDelivery delivery;
      delivery.value = std::move(item.value);
      complete_delivery(node, std::move(delivery));
    } else if (pipe->closed) {
      pipe->end_delivered = true;
      ErasedDelivery end;
      end.has_end = true;
      end.end = pipe->end;
      complete_delivery(node, std::move(end));
    } else {
      pipe->receivers.push_back(node);
      return wrap_task<ErasedDelivery>(std::move(node));
    }
  }
  for (auto& item : completed_sends)
    complete_node(item, Code::ok, box_value<Done>(Done{}));
  for (std::size_t i = 0; i < receiver_nodes.size(); ++i)
    complete_delivery(receiver_nodes[i], std::move(deliveries[i]));
  for (auto& item : end_nodes) {
    ErasedDelivery end;
    end.has_end = true;
    end.end = pipe->end;
    complete_delivery(item, std::move(end));
  }
  try_fulfill_broadcasts();
  return wrap_task<ErasedDelivery>(std::move(node));
}

Task<Done> pipe_close_erased(InvocationContext ctx,
                             std::shared_ptr<PipeCore> pipe, PipeEnd end) {
  auto node = make_node(ctx, TaskSpec{});
  if (!pipe) {
    complete_node(node, Code::invalid_request, nullptr, "no pipe");
    return wrap_task<Done>(std::move(node));
  }
  std::vector<std::shared_ptr<TaskNode>> senders;
  std::vector<std::shared_ptr<TaskNode>> receiver_nodes;
  std::vector<ErasedDelivery> deliveries;
  std::vector<std::shared_ptr<TaskNode>> end_nodes;
  bool conflict = false;
  {
    std::lock_guard<std::mutex> lock(pipe->mutex);
    if (pipe->closed) {
      const bool same = pipe->end.kind == end.kind &&
                        pipe->end.issues.size() == end.issues.size();
      if (!same) conflict = true;
    } else {
      pipe->closed = true;
      pipe->end = std::move(end);
      const bool gap =
          pipe->spec.order == PipeOrder::ordered && !pipe->waiting.empty() &&
          pipe->end.kind == EndKind::eof;
      if (gap) {
        pipe->end.kind = EndKind::failed;
        Issue issue;
        issue.code = Code::missing_dependency;
        issue.scope.frame = "pipe";
        issue.origin = "runtime.pipe";
        issue.reason =
            "the ordered pipe closed before the required key was delivered";
        issue.impact = "the downstream ordered stream is incomplete";
        issue.retry_condition = "the producer delivers the missing key";
        pipe->end.issues.push_back(std::move(issue));
      }
      for (auto& waiter : pipe->waiting) senders.push_back(std::move(waiter.node));
      pipe->waiting.clear();
      pipe->dispatch(receiver_nodes, deliveries, end_nodes);
    }
  }
  if (conflict) {
    complete_node(node, Code::invalid_request, nullptr,
                  "the pipe was already closed with a different terminal state");
    return wrap_task<Done>(std::move(node));
  }
  for (auto& sender : senders)
    complete_node(sender, Code::cancelled, nullptr,
                  "the pipe was closed before the key could be delivered");
  for (std::size_t i = 0; i < receiver_nodes.size(); ++i)
    complete_delivery(receiver_nodes[i], std::move(deliveries[i]));
  for (auto& receiver : end_nodes) {
    ErasedDelivery terminal;
    terminal.has_end = true;
    terminal.end = pipe->end;
    complete_delivery(receiver, std::move(terminal));
  }
  complete_node(node, Code::ok, box_value<Done>(Done{}));
  try_fulfill_broadcasts();
  return wrap_task<Done>(std::move(node));
}

Task<Done> pipe_broadcast_erased(
    InvocationContext ctx, Key key, std::shared_ptr<const void> value,
    U64 bytes, std::vector<std::shared_ptr<PipeCore>> pipes) {
  auto node = make_node(ctx, TaskSpec{});
  if (pipes.empty() || !value) {
    complete_node(node, Code::invalid_request, nullptr, "no broadcast outputs");
    return wrap_task<Done>(std::move(node));
  }
  std::sort(pipes.begin(), pipes.end(),
            [](const std::shared_ptr<PipeCore>& a,
               const std::shared_ptr<PipeCore>& b) {
              return a->edge_id < b->edge_id;
            });
  auto group = std::make_shared<BroadcastGroup>();
  group->key = key;
  group->item = Item{key, std::move(value), bytes};
  group->pipes = std::move(pipes);
  group->node = node;
  bool parked = false;
  {
    std::lock_guard<std::mutex> global(broadcast_mutex());
    bool ok = true;
    for (auto& pipe : group->pipes) {
      std::lock_guard<std::mutex> lock(pipe->mutex);
      if (pipe->closed || !pipe->has_credit(bytes)) {
        ok = false;
        break;
      }
    }
    if (ok) {
      for (auto& pipe : group->pipes) {
        std::lock_guard<std::mutex> lock(pipe->mutex);
        Item item = group->item;
        pipe->ready.push_back(std::move(item));
        ++pipe->queued_items;
        pipe->queued_bytes += bytes;
        if (pipe->spec.order == PipeOrder::ordered) {
          pipe->have_expected = true;
          pipe->expected = next_key(key);
        }
      }
    } else {
      bool closed_branch = false;
      for (auto& pipe : group->pipes) {
        std::lock_guard<std::mutex> lock(pipe->mutex);
        closed_branch = closed_branch || pipe->closed;
      }
      if (closed_branch) {
        complete_node(node, Code::invalid_request, nullptr,
                      "a broadcast branch is already closed");
        return wrap_task<Done>(std::move(node));
      }
      broadcast_groups().push_back(group);
      parked = true;
    }
  }
  if (parked) {
    // Bounded: the group waits for every required branch to free credit; one
    // slow branch backpressures the whole scientific broadcast.
    return wrap_task<Done>(std::move(node));
  }
  complete_node(node, Code::ok, box_value<Done>(Done{}));
  for (auto& pipe : group->pipes) release_credit_and_dispatch(*pipe);
  return wrap_task<Done>(std::move(node));
}

}  // namespace detail
}  // namespace acsd::rt
