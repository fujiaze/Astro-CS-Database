// S-ORDER: with max_items=1 the minimum required key keeps the reserved slot.
// A higher key that is ready first must not take that slot, and after the low
// key arrives the ordered pipe returns key0 then key1, with no unbounded
// reordering.
//
// Fixed configuration (SCHEDULER.md S-ORDER row): key0 "slow", key1 ready
// first, slot=1, H only the floor.  The observable is the delivery order and
// the fact that the high-key send cannot finish before the low key exists.
#include <acsd/contracts.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;

struct Item {
  rt::Key key;
  std::uint64_t value = 0;
};

using ItemPipe = rt::Pipe<Item>;

std::atomic<int> g_abort{0};

bool terminal(rt::TaskState state) {
  return state == rt::TaskState::success || state == rt::TaskState::failed ||
         state == rt::TaskState::cancelled;
}

}  // namespace

int main() {
  vrt::Recorder recorder("order01", "validation/runtime/order01/order01.cpp");

  auto services = vrt::start_services(64ull << 20, 100, 2, 2, 2,
                                      vrt::scratch_root("order01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("order.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.order.command",
                                  vrt::limits(64ull << 20, 100, 4096, 1, 2));
  recorder.expect_code("order.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;
  const rt::ScopeSnapshot baseline = rt::snapshot(ctx.scope);

  auto plan = std::make_shared<rt::InvocationPlan>(
      vrt::make_plan(0, {vrt::make_edge(10, 1, 64)}, {}, 64, 0));

  auto body = [&](rt::InvocationContext region_ctx) -> rt::Task<std::int64_t> {
    Result<std::int64_t> failure;
    failure.code = Code::invalid_request;
    auto pipe = rt::make_pipe<Item>(region_ctx, plan, 0, 10);
    recorder.expect_code("order.make_pipe_ok", "ok", pipe);
    if (pipe.code != Code::ok || !pipe.value) return rt::ready(region_ctx, failure);

    const rt::Key key0{0, 0, 0, 0};
    const rt::Key key1{0, 0, 0, 1};

    // key1 is "ready" first and is submitted first.
    auto high = (*pipe.value)->send(region_ctx, key1, Item{key1, 1});
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const bool high_done_early = terminal(high.state());
    recorder.check("order.high_key_does_not_take_reserved_slot", !high_done_early,
                   "high-key send still waiting for the minimum key",
                   std::string("state=") + std::to_string(static_cast<int>(high.state())));
    const rt::ScopeSnapshot pending = rt::snapshot(ctx.scope);
    recorder.value("order.allocated_while_high_pending",
                   static_cast<long long>(pending.allocated.host_bytes -
                                          baseline.allocated.host_bytes));

    // key0 arrives.  It may preempt; a correct ordered pipe delivers it first.
    auto low = (*pipe.value)->send(region_ctx, key0, Item{key0, 0});
    auto low_result = rt::wait_external(std::move(low));
    recorder.expect_code("order.low_key_send_ok", "ok", low_result);
    if (low_result.code != Code::ok) {
      // The minimum key must be able to enter the reserved slot.  Do not block
      // on the still-waiting high-key send: record the evidence and stop.
      recorder.note("minimum-key send rejected while a higher-key send waited; "
                    "the missing later checks document the blocked progression");
      g_abort.store(1);
      return rt::ready(region_ctx, failure);
    }

    auto first = rt::wait_external((*pipe.value)->receive(region_ctx));
    const auto* first_item = first.value ? std::get_if<Item>(&*first.value) : nullptr;
    recorder.check("order.first_receive_is_low_key",
                   first.code == Code::ok && first_item != nullptr &&
                       first_item->key == key0,
                   "key0", first_item != nullptr
                                ? "key item=" + std::to_string(first_item->key.item)
                                : vrt::code_of(first));

    auto high_result = rt::wait_external(std::move(high));
    recorder.expect_code("order.high_key_send_ok_after_credit", "ok", high_result);
    auto second = rt::wait_external((*pipe.value)->receive(region_ctx));
    const auto* second_item = second.value ? std::get_if<Item>(&*second.value) : nullptr;
    recorder.check("order.second_receive_is_high_key",
                   second.code == Code::ok && second_item != nullptr &&
                       second_item->key == key1,
                   "key1", second_item != nullptr
                                ? "key item=" + std::to_string(second_item->key.item)
                                : vrt::code_of(second));
    recorder.check("order.no_unbounded_reorder",
                   first_item != nullptr && second_item != nullptr &&
                       first_item->key < second_item->key,
                   "delivery keys strictly increasing", "ok");

    auto closed = rt::wait_external((*pipe.value)->close(
        region_ctx, rt::PipeEnd{rt::EndKind::eof, {}}));
    recorder.expect_code("order.close_pipe_ok", "ok", closed);
    Result<std::int64_t> ok;
    ok.code = Code::ok;
    ok.value = 1;
    return rt::ready(region_ctx, std::move(ok));
  };

  auto task = rt::run_region<std::int64_t>(ctx, plan, 0, body);
  auto result = rt::wait_external(std::move(task));
  recorder.check("order.region_body_ok", result.code == Code::ok, "ok",
                 vrt::code_of(result));

  if (g_abort.load() == 0) {
    auto closed = rt::wait_external(rt::close_command(ctx));
    recorder.expect_code("order.close_command_ok", "ok", closed);
  }
  return recorder.finish();
}
