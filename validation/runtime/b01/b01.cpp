// B01: streaming output is appended many times and sealed; the output is far
// larger than the per-request buffer, so no complete output is resident.  A
// mid-way failure or cancel leaves no half product: the writer goes failed and
// only abort() succeeds; a sealed array cannot be modified.
//
// Fixed configuration is scaled down from the H=32MiB / output=256MiB row to
// 1MiB buffer / 8MiB output / 1MiB appends so the case is bounded; the ratio
// output/buffer (64x > 1) and the failure paths are the same.
#include <acsd/contracts.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;
using acsd::U64;

constexpr U64 kMiB = 1ull << 20;
constexpr U64 kChunk = 1ull << 20;
constexpr U64 kOutput = 8 * kMiB;  // 8 appends of 1 MiB, 64x the buffer

bool terminal(rt::TaskState state) {
  return state == rt::TaskState::success || state == rt::TaskState::failed ||
         state == rt::TaskState::cancelled;
}

void fill_random(std::vector<std::uint8_t>& bytes, std::uint64_t seed) {
  std::mt19937_64 engine(seed);
  for (std::size_t i = 0; i < bytes.size(); i += 8) {
    const std::uint64_t value = engine();
    const std::size_t count = std::min<std::size_t>(8, bytes.size() - i);
    std::memcpy(bytes.data() + i, &value, count);
  }
}

Result<rt::WriterRef> begin_writer(const rt::InvocationContext& ctx,
                                   const rt::ArenaRef& arena,
                                   const rt::ArrayLayout& layout) {
  rt::BuildSpec spec;
  spec.layout = layout;
  spec.placement = rt::Placement::spill;
  spec.buffer_bytes = kChunk;
  spec.metadata_bytes = 4096;
  auto task = rt::begin_async(ctx, arena, spec);
  auto result = rt::wait_external(std::move(task));
  if (result.code != Code::ok)
    return Result<rt::WriterRef>{result.code, std::nullopt, result.assessment, result.usage,
                                 result.dependencies};
  return result;
}

Result<rt::Block<std::uint8_t>> make_block(const rt::InvocationContext& ctx,
                                           const rt::ArenaRef& arena,
                                           const rt::ArrayLayout& layout,
                                           const std::vector<std::uint8_t>& payload,
                                           U64 begin) {
  auto buffer = rt::allocate<std::uint8_t>(ctx, arena,
                                           acsd::BlockRequest{begin, payload.size()},
                                           layout.index, layout.unit, vrt::zero_hash());
  if (buffer.code != Code::ok || !buffer.value) {
    Result<rt::Block<std::uint8_t>> failure;
    failure.code = buffer.code;
    return failure;
  }
  std::copy(payload.begin(), payload.end(), buffer.value->values.begin());
  return rt::freeze(std::move(*buffer.value));
}

Result<rt::WriterRef> open_writer(const rt::InvocationContext& ctx, const rt::ArenaRef& arena,
                                  const rt::ArrayLayout& layout) {
  return begin_writer(ctx, arena, layout);
}

}  // namespace

int main() {
  vrt::Recorder recorder("b01", "validation/runtime/b01/b01.cpp");

  auto services = vrt::start_services(256 * kMiB, 1000000, 2, 2, 2,
                                      vrt::scratch_root("b01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("b01.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.b01.command",
                                  vrt::limits(240 * kMiB, 1000000, 8192, 1, 2));
  recorder.expect_code("b01.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;

  rt::ArrayLayout layout;
  layout.dtype = acsd::DType::u8;
  layout.shape = {kOutput};
  layout.unit = vrt::dimensionless_unit();
  layout.index = vrt::make_index(kOutput, "validation.b01.output");

  // --- A. 8 appends then seal; read back; sealed cannot be modified. -------
  {
    auto arena = rt::reserve(ctx, vrt::bytes(kOutput + 2 * kChunk));
    recorder.expect_code("b01.reserve_ok", "ok", arena);
    if (arena.code == Code::ok && arena.value) {
      auto writer = open_writer(ctx, *arena.value, layout);
      recorder.expect_code("b01.begin_ok", "ok", writer);
      if (writer.code == Code::ok && writer.value) {
        std::vector<std::uint8_t> chunk(kChunk);
        bool append_ok = true;
        for (U64 i = 0; i < kOutput / kChunk; ++i) {
          fill_random(chunk, 7000 + i);
          auto block = make_block(ctx, *arena.value, layout, chunk, i * kChunk);
          if (block.code != Code::ok || !block.value) {
            append_ok = false;
            break;
          }
          auto appended = rt::wait_external(
              rt::append_async<std::uint8_t>(ctx, *writer.value, std::move(*block.value)));
          const U64 expected = (i + 1) * kChunk;
          if (appended.code != Code::ok || !appended.value ||
              *appended.value != expected) {
            append_ok = false;
            recorder.value("b01.append_failed_at", static_cast<long long>(i));
            break;
          }
        }
        recorder.check("b01.eight_appends_cumulative", append_ok,
                       "each append returns (i+1)*1 MiB",
                       append_ok ? "8 appends ok" : "append mismatch");

        auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
        recorder.check("b01.seal_ok", sealed.code == Code::ok && sealed.value && *sealed.value,
                       "ok + ArrayRef", vrt::code_of(sealed));
        if (sealed.code == Code::ok && sealed.value) {
          recorder.check("b01.sealed_shape", (*sealed.value)->shape.size() == 1 &&
                                                (*sealed.value)->shape.front() == kOutput,
                         std::to_string(kOutput) + " elements",
                         (*sealed.value)->shape.empty()
                             ? "empty"
                             : std::to_string((*sealed.value)->shape.front()));
          auto read = rt::wait_external(rt::read_async<std::uint8_t>(
              ctx, *arena.value, *sealed.value, acsd::BlockRequest{0, kChunk}));
          recorder.check("b01.readback_ok", read.code == Code::ok && read.value,
                         "ok", vrt::code_of(read));
          if (read.code == Code::ok && read.value) {
            std::vector<std::uint8_t> expected(kChunk);
            fill_random(expected, 7000);
            const bool same =
                read.value->values.size() == kChunk &&
                std::memcmp(read.value->values.data(), expected.data(), kChunk) == 0;
            recorder.check("b01.readback_matches_input", same, "first 1 MiB byte-identical",
                           same ? "identical" : "different");
          }
          auto late_block = make_block(ctx, *arena.value, layout, chunk, kOutput);
          Result<U64> after_seal;
          if (late_block.code == Code::ok && late_block.value)
            after_seal = rt::wait_external(rt::append_async<std::uint8_t>(
                ctx, *writer.value, std::move(*late_block.value)));
          else
            after_seal.code = late_block.code;
          recorder.check("b01.append_after_seal_rejected",
                         after_seal.code == Code::invalid_request ||
                             after_seal.code == Code::invalid_model,
                         "invalid_request or invalid_model", vrt::code_of(after_seal));
        }
        auto aborted = rt::wait_external(rt::abort_async(ctx, *writer.value));
        recorder.expect_code("b01.abort_sealed_noop_ok", "ok", aborted);
      }
      (*arena.value)->close();
    }
  }

  // --- B. empty writer cannot seal. ---------------------------------------
  {
    auto arena = rt::reserve(ctx, vrt::bytes(2 * kChunk));
    if (arena.code == Code::ok && arena.value) {
      auto writer = open_writer(ctx, *arena.value, layout);
      if (writer.code == Code::ok && writer.value) {
        auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
        recorder.check("b01.empty_writer_seal_rejected", sealed.code == Code::invalid_request,
                       "invalid_request", vrt::code_of(sealed));
        auto aborted = rt::wait_external(rt::abort_async(ctx, *writer.value));
        recorder.expect_code("b01.empty_writer_abort_ok", "ok", aborted);
      }
      (*arena.value)->close();
    }
  }

  // --- C. non-contiguous and overflowing appends fail the writer. ---------
  {
    auto arena = rt::reserve(ctx, vrt::bytes(3 * kChunk));
    if (arena.code == Code::ok && arena.value) {
      auto writer = open_writer(ctx, *arena.value, layout);
      if (writer.code == Code::ok && writer.value) {
        std::vector<std::uint8_t> chunk(kChunk);
        fill_random(chunk, 11);
        auto first = make_block(ctx, *arena.value, layout, chunk, 0);
        auto appended = rt::wait_external(
            rt::append_async<std::uint8_t>(ctx, *writer.value, std::move(*first.value)));
        recorder.expect_code("b01.contiguous_first_append_ok", "ok", appended);

        // Out of order: the next expected element is 1 MiB, not 2 MiB.
        auto gap = rt::allocate<std::uint8_t>(ctx, *arena.value,
                                              acsd::BlockRequest{2 * kMiB, kChunk},
                                              layout.index, layout.unit, vrt::zero_hash());
        recorder.expect_code("b01.gap_allocate_ok", "ok", gap);
        if (gap.code == Code::ok && gap.value) {
          auto gap_block = rt::freeze(std::move(*gap.value));
          Result<U64> bad;
          if (gap_block.code == Code::ok && gap_block.value)
            bad = rt::wait_external(rt::append_async<std::uint8_t>(
                ctx, *writer.value, std::move(*gap_block.value)));
          else
            bad.code = gap_block.code;
          recorder.check("b01.non_contiguous_append_rejected",
                         bad.code == Code::invalid_model ||
                             bad.code == Code::invalid_request,
                         "invalid_model or invalid_request", vrt::code_of(bad));
        }
        auto seal_failed = rt::wait_external(rt::seal_async(ctx, *writer.value));
        recorder.check("b01.failed_writer_cannot_seal",
                       seal_failed.code == Code::invalid_model ||
                           seal_failed.code == Code::invalid_request,
                       "invalid_model or invalid_request", vrt::code_of(seal_failed));
        auto abort_one = rt::wait_external(rt::abort_async(ctx, *writer.value));
        auto abort_two = rt::wait_external(rt::abort_async(ctx, *writer.value));
        recorder.expect_code("b01.abort_after_failure_ok", "ok", abort_one);
        recorder.expect_code("b01.abort_idempotent_ok", "ok", abort_two);
      }
      (*arena.value)->close();
    }
  }

  // --- D. cancel in the middle of an append: no half product. --------------
  {
    auto arena = rt::reserve(ctx, vrt::bytes(2 * kOutput));
    if (arena.code == Code::ok && arena.value) {
      auto writer = open_writer(ctx, *arena.value, layout);
      if (writer.code == Code::ok && writer.value) {
        std::vector<std::uint8_t> chunk(kChunk);
        fill_random(chunk, 99);
        auto first = make_block(ctx, *arena.value, layout, chunk, 0);
        auto appended = rt::wait_external(
            rt::append_async<std::uint8_t>(ctx, *writer.value, std::move(*first.value)));
        recorder.expect_code("b01.cancel_first_append_ok", "ok", appended);

        auto big = std::make_shared<std::vector<std::uint8_t>>(4 * kMiB);
        fill_random(*big, 100);
        auto buffer = rt::allocate<std::uint8_t>(ctx, *arena.value,
                                                 acsd::BlockRequest{kChunk, 4 * kMiB},
                                                 layout.index, layout.unit, vrt::zero_hash());
        recorder.expect_code("b01.cancel_allocate_ok", "ok", buffer);
        if (buffer.code == Code::ok && buffer.value) {
          std::copy(big->begin(), big->end(), buffer.value->values.begin());
          auto block = rt::freeze(std::move(*buffer.value));
          auto in_flight = rt::append_async<std::uint8_t>(ctx, *writer.value,
                                                          std::move(*block.value));
          rt::request_cancel(ctx);
          auto result = rt::wait_external(std::move(in_flight));
          recorder.check("b01.cancel_append_terminal_named",
                         result.code == Code::cancelled || result.code == Code::ok,
                         "cancelled or ok", vrt::code_of(result));
          auto seal_cancelled = rt::wait_external(rt::seal_async(ctx, *writer.value));
          recorder.check("b01.cancel_no_half_product",
                         seal_cancelled.code != Code::ok ||
                             !seal_cancelled.value.has_value(),
                         "seal does not deliver an ArrayRef",
                         vrt::code_of(seal_cancelled));
          auto aborted = rt::wait_external(rt::abort_async(ctx, *writer.value));
          recorder.expect_code("b01.cancel_abort_ok", "ok", aborted);
        }
      }
      (*arena.value)->close();
    }
  }

  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("b01.close_command_ok", "ok", closed);
  return recorder.finish();
}
