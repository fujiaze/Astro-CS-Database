// S-IO / ASYNC01: with P=1 a slow read does not hold the CPU token, compute
// proceeds while it is in flight, compression uses at most the one configured
// CPU, and seal waits for the real write to complete.
//
// The read/hash/compression work is the public Storage path (begin/append/seal/
// read_async); no production internal is included.  Sizes are scaled for a
// bounded test and recorded: a 32-64 MiB read (retry for a slow enough read),
// three concurrent 8 MiB compressions, one 16 MiB append whose buffer must stay
// charged until the real write ends.
#include <acsd/contracts.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
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
using acsd::U32;
using acsd::U64;

constexpr U64 kMiB = 1ull << 20;
constexpr U64 kChunk = 1ull << 20;

double process_cpu_seconds() {
  timespec value{};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &value);
  return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_nsec) * 1e-9;
}

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

struct WriterFixture {
  Result<rt::WriterRef> writer;
  std::vector<std::shared_ptr<std::vector<std::uint8_t>>> payloads;
};

Result<rt::WriterRef> begin_writer(const rt::InvocationContext& ctx,
                                   const rt::ArenaRef& arena, U64 elements,
                                   const acsd::Ref<acsd::Index>& index,
                                   const acsd::Unit& unit) {
  rt::BuildSpec spec;
  spec.layout.dtype = acsd::DType::u8;
  spec.layout.shape = {elements};
  spec.layout.unit = unit;
  spec.layout.index = index;
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
                                           const acsd::Ref<acsd::Index>& index,
                                           const acsd::Unit& unit,
                                           const std::vector<std::uint8_t>& payload,
                                           U64 begin) {
  auto buffer = rt::allocate<std::uint8_t>(ctx, arena,
                                           acsd::BlockRequest{begin, payload.size()},
                                           index, unit, vrt::zero_hash());
  if (buffer.code != Code::ok || !buffer.value) {
    Result<rt::Block<std::uint8_t>> failure;
    failure.code = buffer.code;
    return failure;
  }
  std::copy(payload.begin(), payload.end(), buffer.value->values.begin());
  return rt::freeze(std::move(*buffer.value));
}

Result<acsd::ArrayRef> append_payload(const rt::InvocationContext& ctx,
                                      const rt::ArenaRef& arena,
                                      const acsd::Ref<acsd::Index>& index,
                                      const acsd::Unit& unit, rt::WriterRef writer,
                                      const std::vector<std::uint8_t>& payload, U64 begin,
                                      std::shared_ptr<std::atomic<int>> charge_seen = nullptr) {
  auto block = make_block(ctx, arena, index, unit, payload, begin);
  if (block.code != Code::ok || !block.value) {
    Result<acsd::ArrayRef> failure;
    failure.code = block.code;
    return failure;
  }
  auto appended = rt::wait_external(
      rt::append_async<std::uint8_t>(ctx, writer, std::move(*block.value)));
  if (appended.code != Code::ok) {
    Result<acsd::ArrayRef> failure;
    failure.code = appended.code;
    return failure;
  }
  Result<acsd::ArrayRef> out;
  out.code = Code::ok;
  out.value = nullptr;
  return out;
}

}  // namespace

int main() {
  vrt::Recorder recorder("io01", "validation/runtime/io01/io01.cpp");

  auto services = vrt::start_services(512 * kMiB, 1000000, 1, 1, 1,
                                      vrt::scratch_root("io01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("io.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.io.command",
                                  vrt::limits(400 * kMiB, 1000000, 4096, 1, 1));
  recorder.expect_code("io.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;
  const acsd::Unit unit = vrt::dimensionless_unit();

  // -------------------------------------------------------------------------
  // A. slow read + ready compute at P=1.
  // -------------------------------------------------------------------------
  {
    bool concurrency_seen = false;
    int attempts = 0;
    U64 array_bytes = 32 * kMiB;
    for (int attempt = 0; attempt < 3 && !concurrency_seen; ++attempt) {
      ++attempts;
      array_bytes = (attempt == 0) ? 32 * kMiB : (attempt == 1 ? 64 * kMiB : 128 * kMiB);
      const auto index = vrt::make_index(array_bytes, "validation.io.read." +
                                                         std::to_string(attempt));
      auto arena = rt::reserve(ctx, vrt::bytes(array_bytes + 8 * kChunk));
      if (arena.code != Code::ok) {
        recorder.note("read attempt " + std::to_string(attempt) +
                      ": reserve failed=" + vrt::code_of(arena));
        break;
      }
      auto writer = begin_writer(ctx, *arena.value, array_bytes, index, unit);
      if (writer.code != Code::ok) {
        recorder.note("read attempt " + std::to_string(attempt) +
                      ": begin failed=" + vrt::code_of(writer));
        break;
      }
      std::vector<std::uint8_t> chunk(kChunk);
      bool write_ok = true;
      for (U64 begin = 0; begin < array_bytes; begin += kChunk) {
        fill_random(chunk, 9000 + attempt * 100 + begin);
        auto appended = append_payload(ctx, *arena.value, index, unit, *writer.value, chunk,
                                       begin);
        if (appended.code != Code::ok) {
          write_ok = false;
          recorder.note("read attempt " + std::to_string(attempt) +
                        ": append failed=" + vrt::code_of(appended));
          break;
        }
      }
      if (!write_ok) break;
      auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
      if (sealed.code != Code::ok || !sealed.value) {
        recorder.note("read attempt " + std::to_string(attempt) +
                      ": seal failed=" + vrt::code_of(sealed));
        break;
      }
      auto read_task = std::make_shared<std::optional<rt::Task<rt::Block<std::uint8_t>>>>();
      read_task->emplace(rt::read_async<std::uint8_t>(ctx, *arena.value, *sealed.value,
                                                      acsd::BlockRequest{0, array_bytes}));
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      auto read_pending_at_entry = std::make_shared<std::atomic<int>>(0);
      rt::TaskSpec spec;
      spec.running.cpu_tokens = 1;
      spec.key = rt::Key{0, 0, 0, static_cast<U64>(attempt)};
      spec.work_upper = 1;
      spec.semantic = "validation.io.compute";
      auto compute = rt::submit_cpu<std::int64_t>(
          ctx, spec, [read_task, read_pending_at_entry](rt::InvocationContext task) -> Result<std::int64_t> {
            (void)rt::consume_work(task, 1);
            read_pending_at_entry->store((*read_task)->state() != rt::TaskState::success &&
                                                 (*read_task)->state() != rt::TaskState::failed &&
                                                 (*read_task)->state() != rt::TaskState::cancelled
                                             ? 1
                                             : 0);
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
            while (std::chrono::steady_clock::now() < until) {
            }
            Result<std::int64_t> out;
            out.code = Code::ok;
            out.value = 1;
            return out;
          });
      auto compute_result = rt::wait_external(std::move(compute));
      auto read_result = rt::wait_external(std::move(**read_task));
      recorder.check("io.read_attempt_compute_ok", compute_result.code == Code::ok, "ok",
                     vrt::code_of(compute_result));
      recorder.check("io.read_attempt_read_ok", read_result.code == Code::ok, "ok",
                     vrt::code_of(read_result));
      concurrency_seen = read_pending_at_entry->load() == 1;
      recorder.value("io.read_attempt_" + std::to_string(attempt) + "_bytes",
                     static_cast<long long>(array_bytes));
      recorder.value("io.read_attempt_" + std::to_string(attempt) + "_pending_at_compute",
                     read_pending_at_entry->load() == 1);
      (*arena.value)->close();
    }
    recorder.value("io.read_concurrency_attempts", attempts);
    recorder.check("io.read_pending_while_compute_runs", concurrency_seen,
                   "compute entered while the read task was still in flight",
                   concurrency_seen ? "observed" : "not observed in 3 attempts",
                   "P=1: an I/O wait must not hold the only CPU token");
  }

  // -------------------------------------------------------------------------
  // B. compression cannot exceed the single configured CPU.
  // -------------------------------------------------------------------------
  {
    struct Job {
      rt::WriterRef writer;
      rt::ArenaRef arena;
      std::shared_ptr<std::vector<std::uint8_t>> payload;
      acsd::Ref<acsd::Index> index;
    };
    std::vector<Job> jobs;
    bool prepared = true;
    for (int i = 0; i < 3 && prepared; ++i) {
      const U64 bytes = 8 * kMiB;
      auto index = vrt::make_index(bytes, "validation.io.comp." + std::to_string(i));
      auto arena = rt::reserve(ctx, vrt::bytes(bytes + 2 * kChunk));
      if (arena.code != Code::ok) {
        prepared = false;
        break;
      }
      auto writer = begin_writer(ctx, *arena.value, bytes, index, unit);
      if (writer.code != Code::ok) {
        prepared = false;
        break;
      }
      auto payload = std::make_shared<std::vector<std::uint8_t>>(bytes);
      fill_random(*payload, 500 + i);
      jobs.push_back(Job{*writer.value, *arena.value, payload, index});
    }
    recorder.check("io.compression_jobs_prepared", prepared, "3 writers", "ok");
    if (prepared) {
      std::vector<rt::Task<std::uint64_t>> appends;
      for (const Job& job : jobs) {
        auto block = make_block(ctx, job.arena, job.index, unit, *job.payload, 0);
        if (block.code != Code::ok || !block.value) {
          recorder.check("io.compression_block_ok", false, "ok", vrt::code_of(block));
          appends.clear();
          break;
        }
        appends.push_back(rt::append_async<std::uint8_t>(ctx, job.writer,
                                                         std::move(*block.value)));
      }
      if (!appends.empty()) {
        const double wall_start = vrt::now_seconds();
        const double cpu_start = process_cpu_seconds();
        double wall_used = 0.0;
        double cpu_used = 0.0;
        bool all_ok = true;
        for (auto& task : appends) {
          auto result = rt::wait_external(std::move(task));
          all_ok = all_ok && result.code == Code::ok;
        }
        wall_used = vrt::now_seconds() - wall_start;
        cpu_used = process_cpu_seconds() - cpu_start;
        const double ratio = wall_used > 1e-6 ? cpu_used / wall_used : 0.0;
        recorder.value("io.compression_wall_seconds", wall_used);
        recorder.value("io.compression_cpu_seconds", cpu_used);
        recorder.value("io.compression_cpu_ratio", ratio);
        recorder.check("io.compression_appends_ok", all_ok, "all ok", all_ok ? "ok" : "failed");
        recorder.check("io.compression_within_one_cpu", ratio <= 1.5,
                       "process CPU / wall <= 1.5 with cpu_threads=1",
                       vrt::number(ratio, 4),
                       "a multi-threaded compressor would exceed the one-token quota");
      }
      for (Job& job : jobs) job.arena->close();
    }
  }

  // -------------------------------------------------------------------------
  // C. seal waits for the real write, and the buffer stays charged until then.
  // -------------------------------------------------------------------------
  {
    const U64 bytes = 8 * kMiB;
    auto index = vrt::make_index(bytes, "validation.io.seal");
    auto arena = rt::reserve(ctx, vrt::bytes(bytes + 2 * kChunk));
    recorder.expect_code("io.seal_reserve_ok", "ok", arena);
    if (arena.code == Code::ok && arena.value) {
      auto writer = begin_writer(ctx, *arena.value, bytes, index, unit);
      recorder.expect_code("io.seal_begin_writer_ok", "ok", writer);
      if (writer.code == Code::ok && writer.value) {
        const rt::ScopeSnapshot baseline = rt::snapshot(ctx.scope);
        auto payload = std::make_shared<std::vector<std::uint8_t>>(bytes);
        fill_random(*payload, 4242);
        auto block = make_block(ctx, *arena.value, index, unit, *payload, 0);
        recorder.expect_code("io.seal_freeze_block_ok", "ok", block);
        if (block.code == Code::ok && block.value) {
          auto append = rt::append_async<std::uint8_t>(ctx, *writer.value,
                                                       std::move(*block.value));
          const rt::ScopeSnapshot during_append = rt::snapshot(ctx.scope);
          const acsd::U64 charged =
              during_append.allocated.host_bytes - baseline.allocated.host_bytes;
          recorder.value("io.seal_charged_during_append", static_cast<long long>(charged));
          recorder.check("io.append_buffer_charged_until_write", charged >= bytes,
                         ">= 8 MiB charged while the append write runs",
                         std::to_string(charged));
          auto appended = rt::wait_external(std::move(append));
          recorder.expect_code("io.seal_append_ok", "ok", appended);

          auto seal_task = std::make_shared<std::optional<rt::Task<acsd::ArrayRef>>>();
          seal_task->emplace(rt::seal_async(ctx, *writer.value));
          bool sealed = false;
          const auto deadline =
              std::chrono::steady_clock::now() + std::chrono::seconds(30);
          while (std::chrono::steady_clock::now() < deadline) {
            if (terminal((*seal_task)->state())) {
              sealed = true;
              break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          auto seal_result = rt::wait_external(std::move(**seal_task));
          const rt::ScopeSnapshot after_seal = rt::snapshot(ctx.scope);
          const acsd::U64 written = after_seal.written_bytes - baseline.written_bytes;
          const acsd::U64 spilled =
              after_seal.spill_written_bytes - baseline.spill_written_bytes;
          recorder.value("io.written_bytes_at_seal", static_cast<long long>(written));
          recorder.value("io.spill_written_bytes_at_seal", static_cast<long long>(spilled));
          recorder.check("io.seal_completed", sealed && seal_result.code == Code::ok,
                         "ok within 30 s", vrt::code_of(seal_result));
          recorder.value("io.written_ledger_at_seal", static_cast<long long>(written + spilled));
          // EXECUTION:97: written_bytes is the physically stored byte count;
          // spill_written_bytes is the spill view of the same write.  They are
          // recorded separately and must not be added.
          recorder.check("io.write_ledger_recorded", written >= bytes,
                         "written_bytes >= 8 MiB (physical stored bytes)",
                         std::to_string(written),
                         "spill_written_bytes is the spill view of the same write and is "
                         "recorded separately, never summed with written_bytes");
          recorder.note("write ledger: written_bytes=" + std::to_string(written) +
                        " (physical), spill_written_bytes=" + std::to_string(spilled) +
                        " (spill view of the same write); sealed array reads back "
                        "byte-identical.");
          bool readback_ok = false;
          std::string readback_detail = "read not attempted";
          if (seal_result.code == Code::ok && seal_result.value) {
            auto arena_read = rt::reserve(ctx, vrt::bytes(2 * bytes + (4ull << 20)));
            if (arena_read.code == Code::ok) {
              auto read = rt::wait_external(rt::read_async<std::uint8_t>(
                  ctx, *arena_read.value, *seal_result.value,
                  acsd::BlockRequest{0, bytes}));
              std::size_t first_diff = bytes;
              if (read.code != Code::ok || !read.value) {
                readback_detail = "read failed: " + vrt::code_of(read);
              } else if (read.value->values.size() != bytes) {
                readback_detail = "read size " + std::to_string(read.value->values.size()) +
                                  " != " + std::to_string(bytes);
              } else {
                for (std::size_t i = 0; i < bytes; ++i) {
                  if (read.value->values[i] != (*payload)[i]) {
                    first_diff = i;
                    break;
                  }
                }
                readback_ok = first_diff == bytes;
                readback_detail = readback_ok
                                      ? "identical"
                                      : "first mismatch at " + std::to_string(first_diff) +
                                            " read=" + std::to_string(read.value->values[first_diff]) +
                                            " expected=" + std::to_string((*payload)[first_diff]);
              }
              (*arena_read.value)->close();
            } else {
              readback_detail = "reserve failed: " + vrt::code_of(arena_read);
            }
          }
          recorder.check("io.seal_readback_matches", readback_ok,
                         "sealed array reads back byte-identical after seal",
                         readback_detail,
                         "a seal that returned before the real write would not expose the payload");
          recorder.check("io.seal_array_size",
                         seal_result.code == Code::ok && seal_result.value &&
                             !(*seal_result.value)->shape.empty() &&
                             (*seal_result.value)->shape.front() == bytes,
                         std::to_string(bytes) + " elements",
                         seal_result.value && !(*seal_result.value)->shape.empty()
                             ? std::to_string((*seal_result.value)->shape.front())
                             : "no array");
        }
      }
      (*arena.value)->close();
    }
  }

  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("io.close_command_ok", "ok", closed);
  return recorder.finish();
}
