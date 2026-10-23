// STORAGE01: LocalStorage row-read regression (commit 04af17cb7).
//
// A [7,3] array has 21 flattened elements.  `read(BlockRequest{6,3})` must be
// accepted and return elements 6..8 byte-for-byte: the range is valid because
// begin + count = 9 <= 21 even though begin (6) is greater than count (3).  The
// old bug compared begin with count and rejected the read.  Out-of-range
// requests must still be a named rejection, never a silent truncation.
//
// This case uses only the public runtime Storage path (begin/append/seal and
// read_async); it is independent of the science code that hit the bug.
#include <acsd/contracts.hpp>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;
using acsd::U64;

constexpr U64 kRows = 7;
constexpr U64 kCols = 3;
constexpr U64 kTotal = kRows * kCols;  // 21

template <class T>
Result<acsd::ArrayRef> seal_array(const rt::InvocationContext& ctx, acsd::DType dtype,
                                  const acsd::Ref<acsd::Index>& index,
                                  const acsd::Unit& unit,
                                  const std::vector<T>& values) {
  Result<acsd::ArrayRef> failure;
  failure.code = Code::internal;
  const U64 bytes = static_cast<U64>(values.size() * sizeof(T));
  auto arena = rt::reserve(ctx, vrt::bytes(bytes + (2ull << 20)));
  if (arena.code != Code::ok || !arena.value) return failure;
  rt::BuildSpec spec;
  spec.layout.dtype = dtype;
  spec.layout.shape = {kRows, kCols};
  spec.layout.unit = unit;
  spec.layout.index = index;
  spec.placement = rt::Placement::memory;
  spec.buffer_bytes = bytes;
  spec.metadata_bytes = 4096;
  auto writer = rt::wait_external(rt::begin_async(ctx, *arena.value, spec));
  if (writer.code != Code::ok || !writer.value) return failure;
  auto buffer = rt::allocate<T>(ctx, *arena.value, acsd::BlockRequest{0, kTotal}, index,
                                unit, vrt::zero_hash());
  if (buffer.code != Code::ok || !buffer.value) return failure;
  std::memcpy(buffer.value->values.data(), values.data(), bytes);
  auto block = rt::freeze(std::move(*buffer.value));
  if (block.code != Code::ok || !block.value) return failure;
  auto appended = rt::wait_external(
      rt::append_async<T>(ctx, *writer.value, std::move(*block.value)));
  if (appended.code != Code::ok) return failure;
  auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
  if (sealed.code != Code::ok || !sealed.value) return failure;
  Result<acsd::ArrayRef> out;
  out.code = Code::ok;
  out.value = *sealed.value;
  return out;
}

template <class T>
Result<rt::Block<T>> read_range(const rt::InvocationContext& ctx, acsd::ArrayRef array,
                                U64 begin, U64 count) {
  auto arena = rt::reserve(ctx, vrt::bytes((kTotal * sizeof(T)) * 2 + (2ull << 20)));
  if (arena.code != Code::ok) {
    Result<rt::Block<T>> failure;
    failure.code = arena.code;
    return failure;
  }
  auto task = rt::read_async<T>(ctx, *arena.value, array, acsd::BlockRequest{begin, count});
  auto result = rt::wait_external(std::move(task));
  (*arena.value)->close();
  return result;
}

}  // namespace

int main() {
  vrt::Recorder recorder("storage01", "validation/runtime/storage01/storage01.cpp");

  auto services = vrt::start_services(64ull << 20, 1000000, 1, 2, 2,
                                      vrt::scratch_root("storage01"));
  recorder.expect_code("storage.services_ok", "ok", services);
  if (services.code != Code::ok || !services.value) return recorder.finish();
  auto command = rt::open_command(*services.value, "validation.storage.command",
                                  vrt::limits(64ull << 20, 1000000, 1ull << 20, 1, 2));
  recorder.expect_code("storage.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;

  const auto index = vrt::make_index(kTotal, "validation.storage.index");
  std::vector<std::uint64_t> u64_values(kTotal);
  std::vector<double> f64_values(kTotal);
  for (U64 i = 0; i < kTotal; ++i) {
    u64_values[i] = 7000 + i;
    f64_values[i] = 1000.0 + 0.25 * static_cast<double>(i);
  }
  const acsd::Unit u64_unit = vrt::dimensionless_unit();
  const acsd::Unit f64_unit = vrt::adu_unit();

  auto u64_array = seal_array<std::uint64_t>(ctx, acsd::DType::u64, index, u64_unit,
                                             u64_values);
  auto f64_array = seal_array<double>(ctx, acsd::DType::f64, index, f64_unit, f64_values);
  recorder.expect_code("storage.u64_seal_ok", "ok", u64_array);
  recorder.expect_code("storage.f64_seal_ok", "ok", f64_array);
  if (u64_array.code != Code::ok || f64_array.code != Code::ok) return recorder.finish();

  // The regression: begin=6 > count=3, but begin+count=9 <= 21.
  {
    auto read = read_range<std::uint64_t>(ctx, *u64_array.value, 6, 3);
    recorder.check("storage.u64_row_read_ok", read.code == Code::ok && read.value,
                   "ok + 3 elements", vrt::code_of(read));
    bool exact = false;
    if (read.code == Code::ok && read.value && read.value->values.size() == 3) {
      exact = true;
      for (std::size_t i = 0; i < 3; ++i)
        exact = exact && read.value->values[i] == u64_values[6 + i];
    }
    recorder.check("storage.u64_row_bytes_exact", exact,
                   "elements 6..8 byte-identical", exact ? "identical" : "mismatch",
                   "a valid begin>count range must not be rejected or truncated");
  }
  {
    auto read = read_range<double>(ctx, *f64_array.value, 6, 3);
    recorder.check("storage.f64_row_read_ok", read.code == Code::ok && read.value,
                   "ok + 3 elements", vrt::code_of(read));
    bool exact = false;
    if (read.code == Code::ok && read.value && read.value->values.size() == 3) {
      exact = true;
      for (std::size_t i = 0; i < 3; ++i)
        exact = exact && read.value->values[i] == f64_values[6 + i];
    }
    recorder.check("storage.f64_row_bytes_exact", exact,
                   "elements 6..8 byte-identical", exact ? "identical" : "mismatch");
  }

  // Full array and a cross-row valid range remain readable.
  {
    auto full = read_range<std::uint64_t>(ctx, *u64_array.value, 0, kTotal);
    recorder.check("storage.full_read_ok",
                   full.code == Code::ok && full.value &&
                       full.value->values.size() == kTotal,
                   "21 elements", vrt::code_of(full));
    auto crossing = read_range<std::uint64_t>(ctx, *u64_array.value, 2, 10);
    bool exact = false;
    if (crossing.code == Code::ok && crossing.value && crossing.value->values.size() == 10) {
      exact = true;
      for (std::size_t i = 0; i < 10; ++i)
        exact = exact && crossing.value->values[i] == u64_values[2 + i];
    }
    recorder.check("storage.cross_row_read_ok", exact,
                   "begin=2 count=10 reads 10 elements", exact ? "identical" : "mismatch");
  }

  // Out-of-range requests stay named rejections.
  {
    auto past_end = read_range<std::uint64_t>(ctx, *u64_array.value, 6, 20);
    recorder.check("storage.oob_begin_plus_count_rejected",
                   past_end.code == Code::invalid_request, "invalid_request",
                   vrt::code_of(past_end));
    auto begin_past_end = read_range<std::uint64_t>(ctx, *u64_array.value, 22, 1);
    recorder.check("storage.oob_begin_rejected",
                   begin_past_end.code == Code::invalid_request, "invalid_request",
                   vrt::code_of(begin_past_end));
    auto full_past_end = read_range<double>(ctx, *f64_array.value, 0, 22);
    recorder.check("storage.oob_full_rejected",
                   full_past_end.code == Code::invalid_request, "invalid_request",
                   vrt::code_of(full_past_end));
    auto overflow = read_range<std::uint64_t>(ctx, *u64_array.value,
                                              std::numeric_limits<std::uint64_t>::max() - 1, 3);
    recorder.check("storage.oob_overflow_rejected",
                   overflow.code == Code::invalid_request, "invalid_request",
                   vrt::code_of(overflow),
                   "begin+count overflow must be rejected, not wrapped");
  }

  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("storage.close_command_ok", "ok", closed);
  return recorder.finish();
}
