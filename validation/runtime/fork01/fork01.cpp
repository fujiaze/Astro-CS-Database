// S-FORK: one read-only 8-byte Block is broadcast to two branches with slot=1
// each.  There is exactly one physical 8-byte allocation, two logical credits,
// and a slow branch still receives the value (no silent sample loss).
//
// The two pipes are bound to the two edges of a real public InvocationPlan via
// make_pipe; broadcast() must acquire both credits atomically and share the
// same Lease.
#include <acsd/contracts.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;

using ByteBlock = rt::Block<std::uint8_t>;
using BytePipe = rt::Pipe<ByteBlock>;

}  // namespace

int main() {
  vrt::Recorder recorder("fork01", "validation/runtime/fork01/fork01.cpp");

  auto services = vrt::start_services(64ull << 20, 100, 2, 2, 2,
                                      vrt::scratch_root("fork01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("fork.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.fork.command",
                                  vrt::limits(64ull << 20, 100, 4096, 1, 2));
  recorder.expect_code("fork.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;
  const rt::ScopeSnapshot baseline = rt::snapshot(ctx.scope);

  auto plan = std::make_shared<rt::InvocationPlan>(
      vrt::make_plan(0, {vrt::make_edge(10, 1, 8), vrt::make_edge(11, 1, 8)}, {}, 64, 0));

  auto body = [&](rt::InvocationContext region_ctx) -> rt::Task<std::int64_t> {
    Result<std::int64_t> failure;
    failure.code = Code::invalid_request;

    auto pipe_a = rt::make_pipe<ByteBlock>(region_ctx, plan, 0, 10);
    auto pipe_b = rt::make_pipe<ByteBlock>(region_ctx, plan, 0, 11);
    recorder.expect_code("fork.pipe_a_ok", "ok", pipe_a);
    recorder.expect_code("fork.pipe_b_ok", "ok", pipe_b);
    if (pipe_a.code != Code::ok || pipe_b.code != Code::ok) return rt::ready(region_ctx, failure);

    recorder.value("fork.pipe_a_ids",
                   std::to_string((*pipe_a.value)->region_id()) + "/" +
                       std::to_string((*pipe_a.value)->edge_id()));
    recorder.value("fork.pipe_b_edge", static_cast<long long>((*pipe_b.value)->edge_id()));

    auto arena = rt::reserve(region_ctx, vrt::bytes(64));
    recorder.expect_code("fork.reserve_arena_ok", "ok", arena);
    if (arena.code != Code::ok || !arena.value) return rt::ready(region_ctx, failure);
    const auto index = vrt::make_index(8, "validation.fork.block");
    const acsd::Unit unit = vrt::dimensionless_unit();
    auto buffer = rt::allocate<std::uint8_t>(region_ctx, *arena.value, acsd::BlockRequest{0, 8},
                                             index, unit, vrt::zero_hash());
    recorder.expect_code("fork.allocate_8_ok", "ok", buffer);
    if (buffer.code != Code::ok || !buffer.value) return rt::ready(region_ctx, failure);
    for (std::size_t i = 0; i < buffer.value->values.size(); ++i)
      buffer.value->values[i] = static_cast<std::uint8_t>(i + 1);
    auto frozen = rt::freeze(std::move(*buffer.value));
    recorder.expect_code("fork.freeze_ok", "ok", frozen);
    if (frozen.code != Code::ok || !frozen.value) return rt::ready(region_ctx, failure);
    const ByteBlock reference = *frozen.value;

    // Submit the broadcast first, then consume.  The slow branch is received
    // after a delay, so a broadcast that needs both consumers to run still
    // exposes the slow-branch behaviour instead of deadlocking the test.
    // Both consumers are posted before the broadcast so a rendezvous-style
    // implementation still exposes the slow-branch ordering below.
    auto receive_a = (*pipe_a.value)->receive(region_ctx);
    auto receive_b = (*pipe_b.value)->receive(region_ctx);
    auto broadcast_task = rt::broadcast<ByteBlock>(region_ctx, rt::Key{0, 0, 0, 0},
                                                   std::move(*frozen.value),
                                                   {*pipe_a.value, *pipe_b.value});
    // Branch A consumes immediately; branch B is deliberately slow.
    bool delivered = false;
    const auto delivery_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < delivery_deadline) {
      const rt::TaskState state = receive_a.state();
      if (state == rt::TaskState::success || state == rt::TaskState::failed ||
          state == rt::TaskState::cancelled) {
        delivered = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    recorder.check("fork.receives_wake_within_3s", delivered,
                   "branch A receive terminal within 3 s",
                   std::string("state=") + std::to_string(static_cast<int>(receive_a.state())),
                   "a broadcast that never credits its branches is a named failure");
    if (!delivered) {
      recorder.note("broadcast did not deliver within 3 s; later checks skipped to keep "
                    "the case bounded. Minimal repro: /tmp/vrt-repro/pipe_block.cpp");
      return rt::ready(region_ctx, failure);
    }
    auto first = rt::wait_external(std::move(receive_a));
    const auto* first_block = std::get_if<ByteBlock>(&first.value.value());
    recorder.check("fork.branch_a_receives", first.code == Code::ok &&
                                                 first.value && first_block != nullptr,
                   "ok + Block", vrt::code_of(first));

    const rt::ScopeSnapshot after_broadcast = rt::snapshot(ctx.scope);
    const acsd::U64 allocated_delta =
        after_broadcast.allocated.host_bytes - baseline.allocated.host_bytes;
    recorder.value("fork.allocated_delta", static_cast<long long>(allocated_delta));
    recorder.value("fork.peak_delta", static_cast<long long>(
                                         after_broadcast.peak_allocated.host_bytes -
                                         baseline.peak_allocated.host_bytes));
    recorder.check("fork.one_physical_8b", allocated_delta >= 8 && allocated_delta < 16,
                   "8 <= allocated delta < 16 (one physical 8-byte block)",
                   std::to_string(allocated_delta),
                   "broadcast shares the read-only T/Lease; only the queue credits are per-edge");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    auto second = rt::wait_external(std::move(receive_b));
    const auto* second_block = std::get_if<ByteBlock>(&second.value.value());
    recorder.check("fork.slow_branch_not_lost", second.code == Code::ok &&
                                                    second.value && second_block != nullptr,
                   "ok + Block after 30 ms delay", vrt::code_of(second));
    auto broadcast_result = rt::wait_external(std::move(broadcast_task));
    recorder.expect_code("fork.broadcast_ok", "ok", broadcast_result);

    if (first_block != nullptr && second_block != nullptr) {
      bool same_values = first_block->values.size() == 8 && second_block->values.size() == 8;
      for (std::size_t i = 0; same_values && i < 8; ++i)
        same_values = first_block->values[i] == second_block->values[i];
      recorder.check("fork.both_branches_same_bytes", same_values,
                     "1..8 on both branches", same_values ? "identical" : "different");
      recorder.check("fork.both_branches_same_lease",
                     first_block->lease.owner.get() == second_block->lease.owner.get() &&
                         first_block->lease.owner.get() == reference.lease.owner.get(),
                     "all three Lease owners identical",
                     "a/b/reference owners " +
                         std::to_string(reinterpret_cast<std::uintptr_t>(
                             first_block->lease.owner.get())) +
                         "/" +
                         std::to_string(reinterpret_cast<std::uintptr_t>(
                             second_block->lease.owner.get())) +
                         "/" +
                         std::to_string(reinterpret_cast<std::uintptr_t>(
                             reference.lease.owner.get())));
      recorder.check("fork.join_not_early",
                     first_block->range.begin == 0 && second_block->range.begin == 0,
                     "both deliveries keep the full block range", "ok");
    }

    auto closed_a = rt::wait_external((*pipe_a.value)->close(
        region_ctx, rt::PipeEnd{rt::EndKind::eof, {}}));
    auto closed_b = rt::wait_external((*pipe_b.value)->close(
        region_ctx, rt::PipeEnd{rt::EndKind::eof, {}}));
    recorder.expect_code("fork.close_a_ok", "ok", closed_a);
    recorder.expect_code("fork.close_b_ok", "ok", closed_b);
    (*arena.value)->close();

    Result<std::int64_t> ok;
    ok.code = Code::ok;
    ok.value = 1;
    return rt::ready(region_ctx, std::move(ok));
  };

  auto region_task = rt::run_region<std::int64_t>(ctx, plan, 0, body);
  auto region_result = rt::wait_external(std::move(region_task));
  recorder.check("fork.region_body_ok", region_result.code == Code::ok, "ok",
                 vrt::code_of(region_result));

  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("fork.close_command_ok", "ok", closed);
  return recorder.finish();
}
