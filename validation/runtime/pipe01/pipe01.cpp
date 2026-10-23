// PIPE01: bounded pipe progress, the three terminal states, full-queue
// backpressure and cancellation wake-up.
//
// Fixed configuration (VERIFICATION.md PIPE01 row and FOUNDATION_API.md "Pipe
// 与广播"): a two-stage pipe with max_items=1 and a slot/bytes budget that only
// covers the minimum frontier; high-key-first readiness; slow consumer; full
// queue; cancel; EOF/failed/cancelled must stay distinct and keep the original
// Issue.  0 max_items/max_queued_bytes is a named rejection, never "unbounded".
#include <acsd/contracts.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "harness.hpp"

namespace {

std::atomic<int> g_abort{0};

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;

struct Item {
  rt::Key key;
  std::uint64_t value = 0;
};

using ItemPipe = rt::Pipe<Item>;

bool terminal(rt::TaskState state) {
  return state == rt::TaskState::success || state == rt::TaskState::failed ||
         state == rt::TaskState::cancelled;
}

bool wait_terminal(const auto& task, int milliseconds) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < deadline) {
    if (terminal(task.state())) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return terminal(task.state());
}

std::shared_ptr<rt::InvocationPlan> plan_with(std::vector<rt::EdgeSpec> edges) {
  return std::make_shared<rt::InvocationPlan>(vrt::make_plan(0, std::move(edges), {}, 64, 0));
}

}  // namespace

int main() {
  vrt::Recorder recorder("pipe01", "validation/runtime/pipe01/pipe01.cpp");

  auto services = vrt::start_services(64ull << 20, 100, 2, 2, 2,
                                      vrt::scratch_root("pipe01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("pipe.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto open = [&](const std::string& id) {
    return rt::open_command(*services.value, id, vrt::limits(32ull << 20, 100, 4096, 1, 2));
  };

  // --- A. 0 capacity / unknown edge / unknown region are named rejections. --
  {
    auto command = open("validation.pipe.reject");
    recorder.expect_code("pipe.open_reject_command_ok", "ok", command);
    if (command.code == Code::ok) {
      auto zero_items = rt::make_pipe<Item>(*command.value,
                                            plan_with({vrt::make_edge(10, 0, 8)}), 0, 10);
      recorder.check("pipe.zero_items_rejected", zero_items.code == Code::invalid_request,
                     "invalid_request", vrt::code_of(zero_items));
      auto zero_bytes = rt::make_pipe<Item>(*command.value,
                                            plan_with({vrt::make_edge(10, 1, 0)}), 0, 10);
      recorder.check("pipe.zero_bytes_rejected", zero_bytes.code == Code::invalid_request,
                     "invalid_request", vrt::code_of(zero_bytes));
      auto unknown_edge = rt::make_pipe<Item>(*command.value,
                                              plan_with({vrt::make_edge(10, 1, 64)}), 0, 99);
      recorder.check("pipe.unknown_edge_rejected",
                     unknown_edge.code == Code::invalid_request, "invalid_request",
                     vrt::code_of(unknown_edge));
      auto unknown_region = rt::make_pipe<Item>(*command.value,
                                                plan_with({vrt::make_edge(10, 1, 64)}), 7, 10);
      recorder.check("pipe.unknown_region_rejected",
                     unknown_region.code == Code::invalid_request, "invalid_request",
                     vrt::code_of(unknown_region));
      auto closed = rt::wait_external(rt::close_command(*command.value));
      recorder.expect_code("pipe.close_reject_command_ok", "ok", closed);
    }
  }

  // --- B. EOF / failed / cancelled stay distinct; send after close is
  //        invalid_request. ------------------------------------------------
  {
    auto command = open("validation.pipe.terminal.eof");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      recorder.expect_code("pipe.eof_make_pipe_ok", "ok", pipe);
      if (pipe.code == Code::ok && pipe.value) {
        auto closed = rt::wait_external((*pipe.value)->close(
            ctx, rt::PipeEnd{rt::EndKind::eof, {}}));
        recorder.expect_code("pipe.eof_close_ok", "ok", closed);
        auto receive = rt::wait_external((*pipe.value)->receive(ctx));
        const auto* end = receive.value ? std::get_if<rt::PipeEnd>(&*receive.value) : nullptr;
        recorder.check("pipe.eof_distinct", receive.code == Code::ok && end != nullptr &&
                                                end->kind == rt::EndKind::eof,
                       "PipeEnd{eof}", "end=" +
                           std::string(end != nullptr ? std::to_string(static_cast<int>(end->kind))
                                                      : "none"));
        auto send = rt::wait_external((*pipe.value)->send(ctx, rt::Key{0, 0, 0, 1},
                                                          Item{}));
        recorder.check("pipe.send_after_close_invalid_request",
                       send.code == Code::invalid_request, "invalid_request",
                       vrt::code_of(send));
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("pipe.eof_close_command_ok", "ok", closed);
    }
  }

  {
    auto command = open("validation.pipe.terminal.failed");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      if (pipe.code == Code::ok && pipe.value) {
        acsd::Issue issue;
        issue.code = Code::io_error;
        issue.origin = "validation.pipe";
        issue.reason = "validation.pipe.failed.reason";
        auto closed = rt::wait_external((*pipe.value)->close(
            ctx, rt::PipeEnd{rt::EndKind::failed, {issue}}));
        recorder.expect_code("pipe.failed_close_ok", "ok", closed);
        auto receive = rt::wait_external((*pipe.value)->receive(ctx));
        const auto* end = receive.value ? std::get_if<rt::PipeEnd>(&*receive.value) : nullptr;
        recorder.check("pipe.failed_distinct", receive.code == Code::ok && end != nullptr &&
                                                   end->kind == rt::EndKind::failed,
                       "PipeEnd{failed}", end != nullptr
                                              ? std::to_string(static_cast<int>(end->kind))
                                              : "none");
        recorder.check("pipe.failed_keeps_issue",
                       end != nullptr && !end->issues.empty() &&
                           end->issues.front().reason == "validation.pipe.failed.reason",
                       "original Issue reason preserved",
                       end != nullptr && !end->issues.empty() ? end->issues.front().reason
                                                              : "no issue");
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("pipe.failed_close_command_ok", "ok", closed);
    }
  }

  {
    auto command = open("validation.pipe.terminal.cancelled");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      if (pipe.code == Code::ok && pipe.value) {
        auto closed = rt::wait_external((*pipe.value)->close(
            ctx, rt::PipeEnd{rt::EndKind::cancelled, {}}));
        recorder.expect_code("pipe.cancelled_close_ok", "ok", closed);
        auto receive = rt::wait_external((*pipe.value)->receive(ctx));
        const auto* end = receive.value ? std::get_if<rt::PipeEnd>(&*receive.value) : nullptr;
        recorder.check("pipe.cancelled_distinct",
                       receive.code == Code::ok && end != nullptr &&
                           end->kind == rt::EndKind::cancelled,
                       "PipeEnd{cancelled}",
                       end != nullptr ? std::to_string(static_cast<int>(end->kind)) : "none");
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("pipe.cancelled_close_command_ok", "ok", closed);
    }
  }

  // --- C. full queue: max_items=1 backpressures the second send until the
  //        consumer takes the first item. ---------------------------------
  {
    auto command = open("validation.pipe.full");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      recorder.expect_code("pipe.full_make_pipe_ok", "ok", pipe);
      if (pipe.code == Code::ok && pipe.value) {
        auto first_send = (*pipe.value)->send(ctx, rt::Key{0, 0, 0, 0},
                                              Item{{0, 0, 0, 0}, 0});
        auto second_send = (*pipe.value)->send(ctx, rt::Key{0, 0, 0, 1},
                                               Item{{0, 0, 0, 1}, 1});
        const bool second_done_early = wait_terminal(second_send, 25);
        recorder.check("pipe.full_second_send_backpressured", !second_done_early,
                       "send task still waiting with a full queue",
                       std::string("state=") +
                           std::to_string(static_cast<int>(second_send.state())));
        recorder.value("pipe.first_send_terminal_before_consume",
                       terminal(first_send.state()));
        if (first_send.state() == rt::TaskState::failed ||
            second_send.state() == rt::TaskState::failed) {
          recorder.check("pipe.full_send_named_state", false,
                         "a queued send waits for credit; a failed send is a named defect",
                         "first state=" + std::to_string(static_cast<int>(first_send.state())) +
                             " second state=" +
                             std::to_string(static_cast<int>(second_send.state())),
                         "the first/second send failed instead of waiting; later checks skipped");
          return recorder.finish();
        }
        auto received = rt::wait_external((*pipe.value)->receive(ctx));
        const auto* item = received.value ? std::get_if<Item>(&*received.value) : nullptr;
        recorder.check("pipe.full_first_received", received.code == Code::ok &&
                                                       item != nullptr && item->value == 0,
                       "item 0", item != nullptr ? std::to_string(item->value) : "none");
        auto first_result = rt::wait_external(std::move(first_send));
        auto second_result = rt::wait_external(std::move(second_send));
        recorder.check("pipe.full_first_send_ok", first_result.code == Code::ok,
                       "ok", vrt::code_of(first_result));
        recorder.check("pipe.full_second_send_after_consume_ok",
                       second_result.code == Code::ok, "ok", vrt::code_of(second_result));
        auto second_received = rt::wait_external((*pipe.value)->receive(ctx));
        const auto* second_item =
            second_received.value ? std::get_if<Item>(&*second_received.value) : nullptr;
        recorder.check("pipe.full_second_received",
                       second_received.code == Code::ok && second_item != nullptr &&
                           second_item->value == 1,
                       "item 1", second_item != nullptr ? std::to_string(second_item->value)
                                                        : "none");
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("pipe.full_close_command_ok", "ok", closed);
    }
  }

  // --- D. window=2 arrivals out of order are still reduced by key. ---------
  {
    auto command = open("validation.pipe.order");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 2, 128)}), 0, 10);
      recorder.expect_code("pipe.order_make_pipe_ok", "ok", pipe);
      if (pipe.code == Code::ok && pipe.value) {
        auto send1 = (*pipe.value)->send(ctx, rt::Key{0, 0, 0, 1}, Item{{0, 0, 0, 1}, 1});
        auto send0 = (*pipe.value)->send(ctx, rt::Key{0, 0, 0, 0}, Item{{0, 0, 0, 0}, 0});
        auto first = (*pipe.value)->receive(ctx);
        if (!wait_terminal(first, 3000)) {
          recorder.check("pipe.order_progresses", false,
                         "receive terminal within 3 s",
                         "state=" + std::to_string(static_cast<int>(first.state())),
                         "ordered window=2 did not deliver after both keys were sent");
          return recorder.finish();
        }
        auto first_result = rt::wait_external(std::move(first));
        const auto* a = first_result.value ? std::get_if<Item>(&*first_result.value) : nullptr;
        auto second = (*pipe.value)->receive(ctx);
        if (!wait_terminal(second, 3000)) {
          recorder.check("pipe.order_progresses", false,
                         "second receive terminal within 3 s",
                         "state=" + std::to_string(static_cast<int>(second.state())));
          return recorder.finish();
        }
        auto second_result = rt::wait_external(std::move(second));
        const auto* b = second_result.value ? std::get_if<Item>(&*second_result.value) : nullptr;
        auto s1 = rt::wait_external(std::move(send1));
        auto s0 = rt::wait_external(std::move(send0));
        recorder.expect_code("pipe.order_send_high_first_ok", "ok", s1);
        recorder.expect_code("pipe.order_send_low_ok", "ok", s0);
        recorder.check("pipe.order_reduces_by_key",
                       a != nullptr && b != nullptr && a->key.item == 0 && b->key.item == 1,
                       "items 0 then 1",
                       std::string(a != nullptr ? std::to_string(a->key.item) : "?") + " then " +
                           std::string(b != nullptr ? std::to_string(b->key.item) : "?"));
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("pipe.order_close_command_ok", "ok", closed);
    }
  }

  // --- E. cancel wakes a blocked receive and a blocked send (last: a broken
  //        wake-up must not hang the earlier evidence). ---------------------
  {
    auto command = open("validation.pipe.cancel.receive");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      if (pipe.code == Code::ok && pipe.value) {
        auto receive = (*pipe.value)->receive(ctx);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        rt::request_cancel(ctx);
        const bool woke = wait_terminal(receive, 2000);
        recorder.check("pipe.cancel_wakes_receive", woke, "receive terminal within 2 s",
                       std::string("state=") +
                           std::to_string(static_cast<int>(receive.state())));
        if (woke) {
          auto result = rt::wait_external(std::move(receive));
          const auto* end = result.value ? std::get_if<rt::PipeEnd>(&*result.value) : nullptr;
          recorder.check("pipe.cancel_receive_terminal_named",
                         result.code == Code::cancelled ||
                             (end != nullptr && end->kind == rt::EndKind::cancelled),
                         "cancelled", vrt::code_of(result));
        } else {
          g_abort.store(1);
          recorder.note("receive was not woken by request_cancel; the close check is "
                        "skipped so the case can still emit evidence");
        }
      }
      if (g_abort.load() == 0) {
        auto closed = rt::wait_external(rt::close_command(ctx));
        recorder.expect_code("pipe.cancel_receive_close_command_ok", "ok", closed);
      }
    }
  }

  {
    auto command = open("validation.pipe.cancel.send");
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      auto pipe = rt::make_pipe<Item>(ctx, plan_with({vrt::make_edge(10, 1, 64)}), 0, 10);
      if (pipe.code == Code::ok && pipe.value) {
        auto fill = rt::wait_external((*pipe.value)->send(
            ctx, rt::Key{0, 0, 0, 0}, Item{{0, 0, 0, 0}, 0}));
        recorder.expect_code("pipe.cancel_send_fill_ok", "ok", fill);
        auto blocked = (*pipe.value)->send(ctx, rt::Key{0, 0, 0, 1},
                                           Item{{0, 0, 0, 1}, 1});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        rt::request_cancel(ctx);
        const bool woke = wait_terminal(blocked, 2000);
        recorder.check("pipe.cancel_wakes_send", woke, "send terminal within 2 s",
                       std::string("state=") +
                           std::to_string(static_cast<int>(blocked.state())));
        if (woke) {
          auto result = rt::wait_external(std::move(blocked));
          recorder.check("pipe.cancel_send_terminal_named", result.code == Code::cancelled,
                         "cancelled", vrt::code_of(result));
        } else {
          g_abort.store(1);
          recorder.note("blocked send was not woken by request_cancel; the close check "
                        "is skipped so the case can still emit evidence");
        }
      }
      if (g_abort.load() == 0) {
        auto closed = rt::wait_external(rt::close_command(ctx));
        recorder.expect_code("pipe.cancel_send_close_command_ok", "ok", closed);
      }
    }
  }

  return recorder.finish();
}
