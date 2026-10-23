// S-LIFE / LIFE01: a closed arena still bills surviving output leases; a
// cancelled in-flight write does not release its buffer early; no double free.
//
// Independent criteria (SCHEDULER.md "资源账本的原子操作" and the S-LIFE row):
//   * close(arena) returns only the not-yet-claimed capacity to the ancestor;
//     an already committed Block keeps its bytes charged until the last Lease
//     is released;
//   * the physical peak is history and does not shrink on release;
//   * request_cancel does not free a buffer whose real write still runs; after
//     the write task and abort drain, the bytes are released exactly once.
//
// The case is compiled with AddressSanitizer so that a use-after-free or a
// double release of the surviving Block is observable, not inferred.
#include <acsd/contracts.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;

std::uint64_t g_peak_seen = 0;

void fill_random(std::vector<std::uint8_t>& bytes) {
  std::mt19937_64 engine(20261023);
  for (std::size_t i = 0; i < bytes.size(); i += 8) {
    const std::uint64_t value = engine();
    const std::size_t count = std::min<std::size_t>(8, bytes.size() - i);
    std::memcpy(bytes.data() + i, &value, count);
  }
}

bool terminal(rt::TaskState state) {
  return state == rt::TaskState::success || state == rt::TaskState::failed ||
         state == rt::TaskState::cancelled;
}

}  // namespace

int main() {
  vrt::Recorder recorder("life01", "validation/runtime/life01/life01.cpp");
  recorder.command("validation/runtime/build.sh builds this case with -fsanitize=address");

  auto services = vrt::start_services(768ull << 20, 100000, 1, 2, 2,
                                      vrt::scratch_root("life01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("life.services", false, "ok", vrt::code_of(services),
                   "unique service pool could not be created");
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.life.command",
                                  vrt::limits(512ull << 20, 100000, 1ull << 20, 1, 2));
  recorder.expect_code("life.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;
  const rt::ScopeSnapshot baseline = rt::snapshot(ctx.scope);
  recorder.resource("life.baseline_held", baseline.held.host_bytes);
  recorder.resource("life.baseline_allocated", baseline.allocated.host_bytes);

  // -------------------------------------------------------------------------
  // A. close(arena) with a surviving Block.
  // -------------------------------------------------------------------------
  auto arena = rt::reserve(ctx, vrt::bytes(4096));
  recorder.expect_code("life.reserve_arena_ok", "ok", arena);
  const auto index = vrt::make_index(4096, "validation.life.block");
  const acsd::Unit unit = vrt::dimensionless_unit();
  if (arena.code == Code::ok && arena.value) {
    auto buffer = rt::allocate<std::uint8_t>(ctx, *arena.value, acsd::BlockRequest{0, 4096},
                                             index, unit, vrt::zero_hash());
    recorder.expect_code("life.allocate_block_ok", "ok", buffer);
    if (buffer.code == Code::ok && buffer.value) {
      auto block = rt::freeze(std::move(*buffer.value));
      recorder.expect_code("life.freeze_block_ok", "ok", block);
      const rt::ScopeSnapshot before_close = rt::snapshot(ctx.scope);
      recorder.value("life.allocated_before_close",
                     static_cast<long long>(before_close.allocated.host_bytes));
      recorder.value("life.held_before_close",
                     static_cast<long long>(before_close.held.host_bytes));

      (*arena.value)->close();
      const rt::ScopeSnapshot after_close = rt::snapshot(ctx.scope);
      recorder.value("life.allocated_after_close",
                     static_cast<long long>(after_close.allocated.host_bytes));
      recorder.value("life.held_after_close",
                     static_cast<long long>(after_close.held.host_bytes));
      recorder.resource("arena.close_allocated", after_close.allocated.host_bytes);
      recorder.resource("arena.close_held", after_close.held.host_bytes);
      const acsd::U64 allocated_delta =
          after_close.allocated.host_bytes - baseline.allocated.host_bytes;
      const acsd::U64 held_delta = after_close.held.host_bytes - baseline.held.host_bytes;
      recorder.check("life.surviving_lease_still_billed", allocated_delta == 4096,
                     "allocated delta == 4096 after arena close",
                     std::to_string(allocated_delta));
      recorder.check("life.close_returns_unclaimed_only", held_delta == allocated_delta,
                     "held delta == allocated delta (nothing unclaimed remains)",
                     "held=" + std::to_string(held_delta) +
                         " allocated=" + std::to_string(allocated_delta));
      recorder.check("life.peak_is_history",
                     after_close.peak_allocated.host_bytes >=
                         baseline.peak_allocated.host_bytes + 4096,
                     "peak_allocated grows by >= 4096 while the Block lives",
                     std::to_string(after_close.peak_allocated.host_bytes -
                                    baseline.peak_allocated.host_bytes));

      if (block.code == Code::ok && block.value) {
        // The Block is released here; its bytes must come back exactly once.
        block.value.reset();
        const rt::ScopeSnapshot after_release = rt::snapshot(ctx.scope);
        recorder.value("life.allocated_after_lease_release",
                       static_cast<long long>(after_release.allocated.host_bytes));
        recorder.check("life.release_after_close_once",
                       after_release.allocated.host_bytes == baseline.allocated.host_bytes,
                       "allocated returns to baseline",
                       std::to_string(after_release.allocated.host_bytes - baseline.allocated.host_bytes),
                       "a closed arena must return a released committed claim to the ancestor");
        recorder.check("life.peak_history_preserved",
                       after_release.peak_allocated.host_bytes >=
                           baseline.peak_allocated.host_bytes + 4096,
                       "peak_allocated keeps its history after release",
                       std::to_string(after_release.peak_allocated.host_bytes -
                                      baseline.peak_allocated.host_bytes));
      }
    }
  }

  // -------------------------------------------------------------------------
  // B. Cancel an in-flight 16 MiB spill write; the buffer must stay charged
  //    until the real write and the abort have drained.
  // -------------------------------------------------------------------------
  constexpr acsd::U64 kPayload = 16ull << 20;  // 16 MiB > 1 MiB buffer
  auto write_arena = rt::reserve(ctx, vrt::bytes(kPayload + (2ull << 20)));
  recorder.expect_code("life.reserve_write_arena_ok", "ok", write_arena);
  if (write_arena.code == Code::ok && write_arena.value) {
    rt::BuildSpec spec;
    spec.layout.dtype = acsd::DType::u8;
    spec.layout.shape = {kPayload};
    spec.layout.unit = unit;
    spec.layout.index = index;
    spec.placement = rt::Placement::spill;
    spec.buffer_bytes = 1ull << 20;
    spec.metadata_bytes = 4096;
    auto writer = rt::wait_external(rt::begin_async(ctx, *write_arena.value, spec));
    recorder.check("life.begin_writer_ok",
                   writer.code == Code::ok && writer.value && *writer.value,
                   "ok + writer", vrt::code_of(writer));
    if (writer.code == Code::ok && writer.value) {
      auto payload = std::make_shared<std::vector<std::uint8_t>>(kPayload);
      fill_random(*payload);
      auto buffer = rt::allocate<std::uint8_t>(ctx, *write_arena.value,
                                               acsd::BlockRequest{0, kPayload}, index, unit,
                                               vrt::zero_hash());
      recorder.expect_code("life.allocate_write_buffer_ok", "ok", buffer);
      if (buffer.code == Code::ok && buffer.value) {
        std::copy(payload->begin(), payload->end(), buffer.value->values.begin());
        auto block = rt::freeze(std::move(*buffer.value));
        recorder.expect_code("life.freeze_write_block_ok", "ok", block);
        if (block.code == Code::ok && block.value) {
          auto append = rt::append_async<std::uint8_t>(ctx, *writer.value,
                                                       std::move(*block.value));
          const rt::ScopeSnapshot at_cancel = rt::snapshot(ctx.scope);
          rt::request_cancel(ctx);
          const bool in_flight_before = !terminal(append.state());
          const rt::ScopeSnapshot after_cancel = rt::snapshot(ctx.scope);
          recorder.value("life.append_state_at_cancel",
                         std::to_string(static_cast<int>(append.state())));
          recorder.resource("life.allocated_at_cancel",
                            after_cancel.allocated.host_bytes);
          recorder.check("life.cancel_write_in_flight",
                         in_flight_before, "append task not terminal when cancel is requested",
                         std::string("state=") + std::to_string(static_cast<int>(append.state())),
                         "if write already finished the release check is vacuous");
          if (in_flight_before) {
            recorder.check("life.cancel_write_not_released_early",
                           after_cancel.allocated.host_bytes - baseline.allocated.host_bytes >=
                               kPayload,
                           "allocated delta >= 16 MiB while the write is in flight",
                           std::to_string(after_cancel.allocated.host_bytes -
                                          baseline.allocated.host_bytes),
                           "payload is moved into the append task; only the writer may hold it");
          }
          auto appended = rt::wait_external(std::move(append));
          recorder.check("life.cancel_append_terminal_named",
                         appended.code == Code::ok || appended.code == Code::cancelled,
                         "ok or cancelled (never a fabricated success fault)",
                         vrt::code_of(appended));
          auto aborted = rt::wait_external(rt::abort_async(ctx, *writer.value));
          recorder.expect_code("life.abort_after_cancel_ok", "ok", aborted);
          const rt::ScopeSnapshot after_drain = rt::snapshot(ctx.scope);
          recorder.value("life.allocated_after_drain",
                         static_cast<long long>(after_drain.allocated.host_bytes));
          recorder.check("life.cancel_write_released_after_real_completion",
                         after_drain.allocated.host_bytes - baseline.allocated.host_bytes <
                             kPayload,
                         "allocated delta < 16 MiB after append+abort",
                         std::to_string(after_drain.allocated.host_bytes -
                                        baseline.allocated.host_bytes));
          g_peak_seen = after_cancel.peak_allocated.host_bytes;
          recorder.resource("life.peak_allocated", g_peak_seen);
          recorder.value("life.usage_work", static_cast<long long>(after_drain.work));
          recorder.value("life.usage_written_bytes",
                         static_cast<long long>(after_drain.written_bytes));
          recorder.value("life.usage_cleanup_work",
                         static_cast<long long>(after_drain.cleanup_work));
        }
      }
    }
  }

  if (write_arena.code == Code::ok && write_arena.value) (*write_arena.value)->close();
  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("life.close_command_ok", "ok", closed);
  const rt::ScopeSnapshot final_snapshot = rt::snapshot(ctx.scope);
  recorder.resource("life.final_allocated", final_snapshot.allocated.host_bytes);
  recorder.check("life.no_double_billing_after_close",
                 final_snapshot.allocated.host_bytes < kPayload,
                 "final allocated < 16 MiB", std::to_string(final_snapshot.allocated.host_bytes));

  return recorder.finish();
}
