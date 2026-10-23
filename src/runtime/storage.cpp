// Asynchronous array read / append / seal / abort on the unique I/O lanes.
//
// Canonical storage format (OBJECTS.md, "数组封存、规范块与科学内容身份"):
//   * fixed width dtypes are cut into canonical blocks of floor(2^20/itemsize)
//     elements from flatten offset 0; the last block may be short;
//   * dyadic arrays use 1024 complete elements per canonical block and the
//     canonical UTF-8 Dyadic JSON element encoding;
//   * a block's raw_sha256 covers the uncompressed canonical bytes and its id is
//     the canonical descriptor hash (no path, no codec);
//   * the array content hash covers the canonical metadata and the ordered raw
//     block hashes, so append granularity, host memory budget and raw/zstd
//     storage never change the scientific identity.
//
// CPU stages (encoding, compression, hashing, copying) run on the single CPU
// pool and consume the execution segment's work; every file read/write runs on
// the I/O lanes.  An in-flight read/write keeps its arena Lease alive through
// the completion dependency and never releases it early.
#include <acsd/runtime/storage.hpp>

#include "runtime/internal.hpp"

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/units.hpp"

#include <zstd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace acsd::rt {
namespace {

using detail::IoRequest;

// ---------------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------------

bool multiply_checked(U64 a, U64 b, U64& out) noexcept {
  if (b != 0 && a > std::numeric_limits<U64>::max() / b) return false;
  out = a * b;
  return true;
}

bool shape_elements(const std::vector<U64>& shape, U64& elements) noexcept {
  U64 count = 1;
  for (const U64 extent : shape) {
    if (extent == 0) {
      elements = 0;
      return true;
    }
    if (!multiply_checked(count, extent, count)) return false;
  }
  elements = count;
  return true;
}

bool unit_equal(const Unit& a, const Unit& b) noexcept {
  return a.scale == b.scale && a.sr_power == b.sr_power &&
         a.pixel_power == b.pixel_power && a.nm_power == b.nm_power &&
         a.multiplier == b.multiplier && a.linear_power == b.linear_power;
}

// ---------------------------------------------------------------------------
// Element <-> dtype mapping
// ---------------------------------------------------------------------------

template <class T>
struct DTypeOf {
  static constexpr bool known = false;
  static constexpr DType value = DType::f64;
};
template <>
struct DTypeOf<double> {
  static constexpr bool known = true;
  static constexpr DType value = DType::f64;
};
template <>
struct DTypeOf<float> {
  static constexpr bool known = true;
  static constexpr DType value = DType::f32;
};
template <>
struct DTypeOf<std::uint64_t> {
  static constexpr bool known = true;
  static constexpr DType value = DType::u64;
};
template <>
struct DTypeOf<std::uint32_t> {
  static constexpr bool known = true;
  static constexpr DType value = DType::u32;
};
template <>
struct DTypeOf<std::uint16_t> {
  static constexpr bool known = true;
  static constexpr DType value = DType::u16;
};
template <>
struct DTypeOf<std::uint8_t> {
  static constexpr bool known = true;
  static constexpr DType value = DType::u8;
};
template <>
struct DTypeOf<std::byte> {
  static constexpr bool known = true;
  static constexpr DType value = DType::u8;
};
template <>
struct DTypeOf<Dyadic> {
  static constexpr bool known = true;
  static constexpr DType value = DType::dyadic;
};

template <class T>
void append_le(std::vector<std::byte>& out, T value) {
  if constexpr (std::is_same_v<T, std::byte>) {
    out.push_back(value);
  } else if constexpr (sizeof(T) == 8) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    if constexpr (std::is_floating_point_v<T>) {
      // Canonical payload: -0.0 and +0.0 hash identically; every NaN maps to the
      // one quiet NaN of its sign.  The scientific value itself is untouched.
      const std::uint64_t sign = bits & (1ull << 63);
      if ((bits & 0x7fffffffffffffffull) == 0)
        bits = 0;
      else if (std::isnan(value))
        bits = sign | 0x7ff8000000000000ull;
    }
    model::append_u64_le(out, bits);
  } else if constexpr (sizeof(T) == 4) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    if constexpr (std::is_floating_point_v<T>) {
      const std::uint32_t sign = bits & 0x80000000u;
      if ((bits & 0x7fffffffu) == 0)
        bits = 0;
      else if (std::isnan(value))
        bits = sign | 0x7fc00000u;
    }
    model::append_u32_le(out, bits);
  } else if constexpr (sizeof(T) == 2) {
    std::uint16_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    out.push_back(static_cast<std::byte>(bits & 0xffu));
    out.push_back(static_cast<std::byte>((bits >> 8) & 0xffu));
  } else {
    static_assert(sizeof(T) == 1);
    std::uint8_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    out.push_back(static_cast<std::byte>(bits));
  }
}

// Canonical Dyadic element: zero is (0,0); a nonzero integer has all factors of
// two removed (odd integer) and exponent2 compensates.  The encoding is UTF-8
// JSON with keys in UTF-8 order and no whitespace.
bool normalize_dyadic(const Dyadic& in, std::string& integer,
                      std::int64_t& exponent) {
  std::string digits = in.signed_integer;
  if (digits.empty()) return false;
  bool negative = false;
  std::size_t first = 0;
  if (digits[0] == '+' || digits[0] == '-') {
    negative = digits[0] == '-';
    first = 1;
  }
  if (first >= digits.size()) return false;
  std::string magnitude;
  magnitude.reserve(digits.size());
  for (std::size_t i = first; i < digits.size(); ++i) {
    if (digits[i] < '0' || digits[i] > '9') return false;
    magnitude.push_back(digits[i]);
  }
  std::size_t lead = 0;
  while (lead + 1 < magnitude.size() && magnitude[lead] == '0') ++lead;
  magnitude = magnitude.substr(lead);
  if (magnitude == "0") {
    integer = "0";
    exponent = 0;
    return true;
  }
  std::int64_t result_exponent = in.exponent2;
  // Remove every factor of two by exact decimal long division.
  for (;;) {
    char last = magnitude.back();
    if ((last - '0') % 2 != 0) break;
    std::string quotient;
    quotient.reserve(magnitude.size());
    int carry = 0;
    for (const char digit : magnitude) {
      const int current = carry * 10 + (digit - '0');
      quotient.push_back(static_cast<char>('0' + current / 2));
      carry = current % 2;
    }
    std::size_t skip = 0;
    while (skip + 1 < quotient.size() && quotient[skip] == '0') ++skip;
    magnitude = quotient.substr(skip);
    result_exponent += 1;
  }
  std::string out;
  if (negative) out.push_back('-');
  out += magnitude;
  integer = std::move(out);
  exponent = result_exponent;
  return true;
}

bool encode_dyadic_value(const Dyadic& value, std::string& out) {
  std::string integer;
  std::int64_t exponent = 0;
  if (!normalize_dyadic(value, integer, exponent)) return false;
  out.clear();
  out.reserve(integer.size() + 40);
  // Key order: exponent2 < signed_integer (UTF-8 byte order).
  out += "{\"exponent2\":";
  out += std::to_string(exponent);
  out += ",\"signed_integer\":\"";
  out += integer;
  out += "\"}";
  return true;
}

template <class T>
void encode_values(std::span<const T> values, std::vector<std::byte>& out) {
  out.clear();
  out.reserve(values.size() * sizeof(T));
  for (const T& value : values) append_le(out, value);
}

// ---------------------------------------------------------------------------
// Canonical hashing
// ---------------------------------------------------------------------------

void hash_array_header(model::Sha256& hash, DType dtype,
                       const std::vector<U64>& shape, const Unit& unit,
                       const Id& index_id) {
  hash.field("kind", "acsd.runtime.array");
  hash.update_u64(static_cast<U64>(dtype));
  hash.update_u64(shape.size());
  for (const U64 extent : shape) hash.update_u64(extent);
  hash.field("unit.scale", unit.scale);
  hash.field_f64("unit.multiplier", unit.multiplier);
  hash.update_u64(static_cast<U64>(static_cast<std::int64_t>(unit.sr_power)));
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(unit.pixel_power)));
  hash.update_u64(static_cast<U64>(static_cast<std::int64_t>(unit.nm_power)));
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(unit.linear_power)));
  hash.field("index_id", index_id);
}

void hash_block(model::Sha256& hash, const BlockDescriptor& block) {
  hash.update_u64(block.begin);
  hash.update_u64(block.count);
  hash.update_u64(block.raw_bytes);
  hash.field("raw_sha256", block.raw_sha256);
}

Id canonical_block_id(const BlockDescriptor& block) {
  model::Sha256 hash;
  hash.field("kind", "acsd.runtime.block");
  hash.update_u64(static_cast<U64>(block.dtype));
  hash.update_u64(block.shape.size());
  for (const U64 extent : block.shape) hash.update_u64(extent);
  hash.field("unit.scale", block.unit.scale);
  hash.field_f64("unit.multiplier", block.unit.multiplier);
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(block.unit.sr_power)));
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(block.unit.pixel_power)));
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(block.unit.nm_power)));
  hash.update_u64(
      static_cast<U64>(static_cast<std::int64_t>(block.unit.linear_power)));
  hash.field("index_id", block.index_id);
  hash.update_u64(block.begin);
  hash.update_u64(block.count);
  hash.update_u64(block.raw_bytes);
  hash.field("raw_sha256", block.raw_sha256);
  return model::content_id(hash.hex());
}

// ---------------------------------------------------------------------------
// Committed byte vector used by memory placement
// ---------------------------------------------------------------------------

struct ChargedVector {
  // Destruction order is reverse declaration order: the physical vector dies
  // before its capacity ticket is released.
  std::shared_ptr<detail::Allocation> allocation;
  std::shared_ptr<std::vector<std::byte>> bytes;
};

Result<std::shared_ptr<ChargedVector>> charge_vector(InvocationContext ctx,
                                                     ArenaRef arena,
                                                     U64 bytes) {
  Result<std::shared_ptr<ChargedVector>> result;
  if (!ctx.services || !arena || bytes == 0) {
    result.code = Code::invalid_request;
    return result;
  }
  ResourceVector capacity;
  capacity.host_bytes = bytes;
  auto ticket = detail::claim_capacity(arena, capacity);
  if (ticket.code != Code::ok || !ticket.value) {
    result.code = ticket.code;
    return result;
  }
  auto holder = std::make_shared<ChargedVector>();
  try {
    holder->bytes = std::make_shared<std::vector<std::byte>>(
        static_cast<std::size_t>(bytes));
  } catch (...) {
    result.code = Code::resource_exhausted;
    return result;
  }
  holder->allocation = std::make_shared<detail::Allocation>();
  holder->allocation->memory = nullptr;
  holder->allocation->bytes = 0;
  holder->allocation->claim = *ticket.value;
  {
    std::lock_guard<std::mutex> lock(holder->allocation->claim->mutex);
    holder->allocation->claim->committed = true;
  }
  detail::ledger_commit((*ticket.value)->arena->scope, capacity);
  result.code = Code::ok;
  result.value = std::move(holder);
  return result;
}

std::shared_ptr<const std::vector<std::byte>> alias_bytes(
    const std::shared_ptr<ChargedVector>& owner) {
  if (!owner || !owner->bytes) return nullptr;
  return std::shared_ptr<const std::vector<std::byte>>(owner,
                                                      owner->bytes.get());
}

// ---------------------------------------------------------------------------
// Writer state
// ---------------------------------------------------------------------------

void cleanup_writer(struct WriterState& writer) noexcept;

struct WriterState {
  std::mutex mutex;
  InvocationContext ctx;
  ArenaRef arena;
  ArrayLayout layout;
  Placement placement = Placement::memory;
  DType dtype = DType::f64;
  U64 itemsize = 0;
  U64 total_elements = 0;
  U64 canonical_elements = 0;
  U64 next_element = 0;
  U64 buffer_bytes = 0;
  U64 metadata_bytes = 0;
  bool use_zstd = false;
  std::atomic<std::uint64_t> stage_sequence{1};

  enum class Status { open, failed, sealed, aborted } status = Status::open;
  Code failure_code = Code::internal;
  std::string failure_reason;
  Id model_hash;
  bool model_hash_set = false;
  bool append_in_flight = false;
  bool seal_in_flight = false;
  bool abort_requested = false;
  std::vector<std::shared_ptr<detail::TaskNode>> abort_waiters;

  // A writer that dies before seal is equivalent to an abort: its private spool
  // and its charged bytes are reclaimed (OBJECTS.md).
  ~WriterState() {
    if (status != Status::sealed && status != Status::aborted) {
      status = Status::aborted;
      cleanup_writer(*this);
    }
  }

  // open canonical block
  bool block_open = false;
  U64 open_block_begin = 0;
  U64 block_elements = 0;
  U64 block_raw_bytes = 0;
  model::Sha256 block_hash;

  // memory placement
  std::shared_ptr<ChargedVector> memory;
  U64 memory_bytes = 0;

  // spill placement
  std::string directory;
  bool spill_open = false;
  bool spill_zstd = false;
  ZSTD_CStream* zstd_stream = nullptr;
  std::string spill_tmp_path;
  U64 spill_stored_bytes = 0;
  U64 spill_sequence = 0;

  std::vector<BlockDescriptor> blocks;
  std::vector<Id> block_ids;
  model::Sha256 array_hash;
  bool hash_seeded = false;
};

struct WriteSegment {
  std::string tmp_path;
  std::string final_path;
  std::shared_ptr<Buffer<std::byte>> payload;
  U64 payload_bytes = 0;
  bool finalize = false;
};

struct AppendBatch {
  std::vector<WriteSegment> segments;
};

struct SealPrep {
  bool needs_io = false;
  std::vector<WriteSegment> segments;
};

}  // namespace

class ArrayWriter {
 public:
  ArrayWriter() = default;
  explicit ArrayWriter(std::shared_ptr<WriterState> state)
      : state(std::move(state)) {}
  std::shared_ptr<WriterState> state;
};

namespace {

std::atomic<std::uint64_t> g_writer_sequence{1};

std::string scratch_directory(InvocationContext ctx, std::uint64_t sequence) {
  std::string root;
  if (ctx.services) root = ctx.services->config().scratch_root;
  if (root.empty()) {
    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    root = ec ? std::string("/tmp") : temp.string();
    root += "/acsd-runtime";
  }
  return root + "/writer-" + std::to_string(sequence);
}

void release_zstd(WriterState& writer) noexcept {
  if (writer.zstd_stream != nullptr) {
    ZSTD_freeCStream(writer.zstd_stream);
    writer.zstd_stream = nullptr;
  }
}

void cleanup_writer(WriterState& writer) noexcept {
  release_zstd(writer);
  if (!writer.directory.empty()) {
    std::error_code ec;
    std::filesystem::remove_all(writer.directory, ec);
  }
  writer.memory.reset();
  writer.blocks.clear();
  writer.block_ids.clear();
}

void fail_writer_locked(WriterState& writer, Code code, std::string reason) {
  if (writer.status == WriterState::Status::failed) return;
  writer.status = WriterState::Status::failed;
  writer.failure_code = code;
  writer.failure_reason = std::move(reason);
  release_zstd(writer);
}

// The single abort rendezvous: any operation that ends while abort_requested
// is set calls this under the writer mutex, performs cleanup outside the lock
// and completes the abort waiters.  A writer is never both sealed and aborted.
std::vector<std::shared_ptr<detail::TaskNode>> mark_aborted_locked(
    WriterState& writer) {
  writer.status = WriterState::Status::aborted;
  writer.seal_in_flight = false;
  writer.append_in_flight = false;
  release_zstd(writer);
  std::vector<std::shared_ptr<detail::TaskNode>> waiters;
  waiters.swap(writer.abort_waiters);
  return waiters;
}

TaskSpec stage_spec(std::uint64_t sequence, std::string semantic, U64 work) {
  TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = work;
  spec.semantic = std::move(semantic) + "#" + std::to_string(sequence);
  return spec;
}

U64 work_units(U64 bytes) noexcept { return 1 + bytes / 4096; }

// Cleanup nodes are control work: the runtime exempts the "service.*" namespace
// from the scientific cancellation check, so aborting a cancelled command can
// still reclaim its private spool (C-CAL-CLEAN).
TaskSpec cleanup_spec(U64 work) {
  TaskSpec spec;
  spec.key = Key{};
  spec.running.cpu_tokens = 1;
  spec.work_upper = work;
  spec.semantic = "service.cleanup";
  return spec;
}

Result<Done> consume(InvocationContext task, U64 units) {
  if (units == 0) return model::ok_result(Done{});
  return consume_work(task, units);
}

// ---------------------------------------------------------------------------
// Canonical block feeding (CPU stage)
// ---------------------------------------------------------------------------

Result<Done> spill_start_block(WriterState& writer, std::string& error) {
  writer.spill_zstd = writer.use_zstd;
  writer.spill_tmp_path = writer.directory + "/tmp-" +
                          std::to_string(writer.spill_sequence) + ".bin";
  writer.spill_sequence += 1;
  writer.spill_stored_bytes = 0;
  if (writer.spill_zstd) {
    writer.zstd_stream = ZSTD_createCStream();
    if (writer.zstd_stream == nullptr) {
      error = "zstd stream allocation failed";
      return model::fail<Done>(Code::resource_exhausted, error,
                               "runtime.storage.append");
    }
    const std::size_t rc = ZSTD_initCStream(writer.zstd_stream, 3);
    if (ZSTD_isError(rc)) {
      release_zstd(writer);
      error = "zstd stream init failed";
      return model::fail<Done>(Code::internal, error,
                               "runtime.storage.append");
    }
  }
  writer.spill_open = true;
  writer.block_open = true;
  return model::ok_result(Done{});
}

void start_block(WriterState& writer, U64 block_begin) {
  writer.block_open = true;
  writer.open_block_begin = block_begin;
  writer.block_elements = 0;
  writer.block_raw_bytes = 0;
  writer.block_hash = model::Sha256{};
}

Result<Done> compress_piece(WriterState& writer,
                            std::span<const std::byte> raw,
                            std::vector<std::byte>& out, bool finish,
                            std::string& error) {
  out.clear();
  if (!writer.spill_zstd) {
    out.assign(raw.begin(), raw.end());
    return model::ok_result(Done{});
  }
  out.resize(raw.size() + ZSTD_CStreamOutSize() + 256);
  ZSTD_inBuffer input{raw.data(), raw.size(), 0};
  std::size_t cursor = 0;
  for (;;) {
    if (cursor == out.size()) out.resize(out.size() * 2 + 64);
    ZSTD_outBuffer output{out.data() + cursor, out.size() - cursor, 0};
    const std::size_t remaining = ZSTD_compressStream2(
        writer.zstd_stream, &output, &input,
        finish ? ZSTD_e_end : ZSTD_e_continue);
    if (ZSTD_isError(remaining)) {
      error = "zstd compression failed";
      return model::fail<Done>(Code::internal, error,
                               "runtime.storage.append");
    }
    cursor += output.pos;
    if (finish) {
      if (remaining == 0) break;
    } else {
      if (input.pos == input.size) break;
    }
  }
  out.resize(cursor);
  return model::ok_result(Done{});
}

Result<Done> finish_block(WriterState& writer, std::vector<WriteSegment>& out,
                          std::string& error) {
  BlockDescriptor descriptor;
  descriptor.dtype = writer.dtype;
  descriptor.shape = writer.layout.shape;
  descriptor.index_id = writer.layout.index
                            ? writer.layout.index->identity.content_hash
                            : Id{};
  descriptor.unit = writer.layout.unit;
  descriptor.begin = writer.open_block_begin;
  descriptor.count = writer.block_elements;
  descriptor.raw_bytes = writer.block_raw_bytes;
  descriptor.raw_sha256 = model::content_id(writer.block_hash.hex());
  if (writer.placement == Placement::memory) {
    descriptor.codec = BlockDescriptor::Codec::raw;
    descriptor.stored_bytes = descriptor.raw_bytes;
    descriptor.id = canonical_block_id(descriptor);
  } else {
    descriptor.codec = writer.spill_zstd ? BlockDescriptor::Codec::zstd
                                         : BlockDescriptor::Codec::raw;
    // Flush the last compressed bytes of the canonical block.
    std::vector<std::byte> tail;
    auto flushed = compress_piece(writer, {}, tail, true, error);
    if (flushed.code != Code::ok) return flushed;
    if (!tail.empty()) {
      auto owner = std::make_shared<std::vector<std::byte>>(std::move(tail));
      auto payload = std::make_shared<Buffer<std::byte>>();
      payload->range = BlockRequest{0, owner->size()};
      payload->values = std::span<std::byte>(owner->data(), owner->size());
      payload->lease.owner = owner;
      WriteSegment segment;
      segment.tmp_path = writer.spill_tmp_path;
      segment.payload = std::move(payload);
      segment.payload_bytes = owner->size();
      writer.spill_stored_bytes += owner->size();
      out.push_back(std::move(segment));
    }
    descriptor.stored_bytes = writer.spill_stored_bytes;
    descriptor.id = canonical_block_id(descriptor);
    descriptor.path = writer.directory + "/" + descriptor.id.substr(7) + ".bin";
    WriteSegment finalize;
    finalize.tmp_path = writer.spill_tmp_path;
    finalize.final_path = descriptor.path;
    finalize.finalize = true;
    out.push_back(std::move(finalize));
    release_zstd(writer);
    writer.spill_open = false;
  }
  hash_block(writer.array_hash, descriptor);
  writer.block_ids.push_back(descriptor.id);
  writer.blocks.push_back(std::move(descriptor));
  writer.block_open = false;
  writer.block_elements = 0;
  writer.block_raw_bytes = 0;
  writer.block_hash = model::Sha256{};
  return model::ok_result(Done{});
}

// Feeds `elements` complete elements of canonical raw bytes into the current
// canonical block.  `block_begin` is the flatten begin of the whole block when
// this call opens it.
Result<Done> feed_piece(InvocationContext task, const std::shared_ptr<WriterState>& state,
                        std::span<const std::byte> piece, U64 elements,
                        U64 block_begin, AppendBatch& batch) {
  WriterState& writer = *state;
  if (elements == 0) return model::ok_result(Done{});
  if (!writer.block_open) {
    if (writer.placement == Placement::memory) {
      start_block(writer, block_begin);
    } else {
      std::string error;
      auto started = spill_start_block(writer, error);
      if (started.code != Code::ok) {
        fail_writer_locked(writer, started.code, error);
        return started;
      }
      writer.open_block_begin = block_begin;
      writer.block_elements = 0;
      writer.block_raw_bytes = 0;
      writer.block_hash = model::Sha256{};
    }
  }
  writer.block_hash.update(piece);
  writer.block_raw_bytes += piece.size();
  writer.block_elements += elements;
  if (writer.placement == Placement::memory) {
    const U64 memory_offset = writer.open_block_begin * writer.itemsize +
                              (writer.block_raw_bytes - piece.size());
    if (memory_offset + piece.size() > writer.memory_bytes) {
      fail_writer_locked(writer, Code::internal,
                         "memory placement offset overflow");
      return model::fail<Done>(Code::internal, "memory placement overflow",
                               "runtime.storage.append");
    }
    std::memcpy(writer.memory->bytes->data() + memory_offset, piece.data(),
                piece.size());
  } else {
    std::vector<std::byte> compressed;
    std::string error;
    auto done = compress_piece(writer, piece, compressed, false, error);
    if (done.code != Code::ok) {
      fail_writer_locked(writer, done.code, error);
      return done;
    }
    if (!compressed.empty()) {
      auto owner =
          std::make_shared<std::vector<std::byte>>(std::move(compressed));
      auto payload = std::make_shared<Buffer<std::byte>>();
      payload->range = BlockRequest{0, owner->size()};
      payload->values = std::span<std::byte>(owner->data(), owner->size());
      payload->lease.owner = owner;
      WriteSegment segment;
      segment.tmp_path = writer.spill_tmp_path;
      segment.payload = std::move(payload);
      segment.payload_bytes = owner->size();
      writer.spill_stored_bytes += owner->size();
      batch.segments.push_back(std::move(segment));
    }
  }
  if (writer.block_elements == writer.canonical_elements) {
    std::string error;
    auto finished = finish_block(writer, batch.segments, error);
    if (finished.code != Code::ok) return finished;
  }
  (void)task;
  return model::ok_result(Done{});
}

Result<Done> feed_dyadic_piece(InvocationContext task,
                               const std::shared_ptr<WriterState>& state,
                               std::span<const std::byte> piece,
                               U64 block_begin, AppendBatch& batch) {
  return feed_piece(task, state, piece, 1, block_begin, batch);
}

// ---------------------------------------------------------------------------
// Append CPU/IO stages
// ---------------------------------------------------------------------------

std::shared_ptr<Buffer<std::byte>> buffer_from_vector(
    std::vector<std::byte> bytes) {
  auto owner = std::make_shared<std::vector<std::byte>>(std::move(bytes));
  auto payload = std::make_shared<Buffer<std::byte>>();
  payload->range = BlockRequest{0, owner->size()};
  payload->values = std::span<std::byte>(owner->data(), owner->size());
  payload->lease.owner = owner;
  return payload;
}

template <class T>
Result<AppendBatch> encode_append(InvocationContext task,
                                  std::shared_ptr<WriterState> state,
                                  Block<T> block) {
  AppendBatch batch;
  const U64 count = block.values.size();
  std::vector<std::byte> encoded;
  std::vector<U64> element_ends;  // dyadic only: end offset of each element
  if constexpr (std::is_same_v<T, Dyadic>) {
    encoded.reserve(count * 48);
    element_ends.reserve(count);
    for (const Dyadic& value : block.values) {
      std::string text;
      if (!encode_dyadic_value(value, text)) {
        std::lock_guard<std::mutex> lock(state->mutex);
        fail_writer_locked(*state, Code::invalid_model,
                           "non-canonical dyadic value");
        return model::fail<AppendBatch>(Code::invalid_model,
                                        "non-canonical dyadic value",
                                        "runtime.storage.append");
      }
      encoded.insert(encoded.end(),
                     reinterpret_cast<const std::byte*>(text.data()),
                     reinterpret_cast<const std::byte*>(text.data()) +
                         text.size());
      element_ends.push_back(encoded.size());
    }
  } else {
    encode_values(block.values, encoded);
  }
  auto accounted = consume(task, work_units(encoded.size()));
  if (accounted.code != Code::ok) {
    std::lock_guard<std::mutex> lock(state->mutex);
    fail_writer_locked(*state, accounted.code, "work budget exhausted");
    return model::fail<AppendBatch>(accounted.code, "work budget exhausted",
                                    "runtime.storage.append");
  }
  U64 consumed = 0;
  U64 offset = 0;
  while (consumed < count) {
    U64 take = 0;
    U64 piece_offset = offset;
    U64 piece_bytes = 0;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (state->status != WriterState::Status::open) {
        return model::fail<AppendBatch>(Code::invalid_request,
                                        "writer is not open",
                                        "runtime.storage.append");
      }
      const U64 remaining =
          state->canonical_elements - state->block_elements;
      take = std::min<U64>(remaining, count - consumed);
      if (take == 0) {
        return model::fail<AppendBatch>(Code::internal,
                                        "canonical block accounting failed",
                                        "runtime.storage.append");
      }
      if constexpr (std::is_same_v<T, Dyadic>) {
        take = 1;
        piece_bytes = element_ends[consumed] - offset;
      } else {
        piece_bytes = take * sizeof(T);
      }
    }
    const U64 block_begin = state->next_element + consumed;
    if constexpr (std::is_same_v<T, Dyadic>) {
      auto fed = feed_dyadic_piece(
          task, state,
          std::span<const std::byte>(encoded.data() + piece_offset,
                                     piece_bytes),
          block_begin, batch);
      if (fed.code != Code::ok) {
        return model::fail<AppendBatch>(fed.code, "canonical block feed failed",
                                        "runtime.storage.append");
      }
    } else {
      auto fed = feed_piece(
          task, state,
          std::span<const std::byte>(encoded.data() + piece_offset,
                                     piece_bytes),
          take, block_begin, batch);
      if (fed.code != Code::ok) {
        return model::fail<AppendBatch>(fed.code, "canonical block feed failed",
                                        "runtime.storage.append");
      }
    }
    consumed += take;
    offset += piece_bytes;
  }
  return model::ok_result(std::move(batch));
}

void complete_abort_waiters(std::vector<std::shared_ptr<detail::TaskNode>> waiters) {
  for (auto& node : waiters) {
    detail::node_complete(node, Code::ok, Assessment{}, Usage{}, {},
                          detail::box_value<Done>(Done{}));
  }
}

// Finalises one append: publishes the new element offset or records failure.
// Returns the completed cumulative element count on success.
Result<U64> finish_append(const std::shared_ptr<WriterState>& state,
                          Result<Done> io_result, U64 appended,
                          U64 accounted_bytes) {
  std::vector<std::shared_ptr<detail::TaskNode>> waiters;
  bool cleanup = false;
  Code code = io_result.code;
  std::string reason;
  U64 published = 0;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (code == Code::ok) {
      state->next_element += appended;
      if (appended != 0 && state->ctx.scope) {
        const U64 spill =
            state->placement == Placement::spill ? accounted_bytes : 0;
        detail::ledger_account_bytes(state->ctx.scope, 0, accounted_bytes,
                                     spill);
      }
    } else {
      if (state->status == WriterState::Status::open) {
        fail_writer_locked(*state, code == Code::ok ? Code::internal : code,
                           "append I/O failed");
      }
      code = state->failure_code;
      reason = state->failure_reason;
    }
    state->append_in_flight = false;
    published = state->next_element;
    if (state->abort_requested) {
      waiters = mark_aborted_locked(*state);
      cleanup = true;
    }
  }
  if (cleanup) {
    cleanup_writer(*state);
    complete_abort_waiters(std::move(waiters));
  }
  if (code != Code::ok) {
    Result<U64> failure;
    failure.code = code;
    return failure;
  }
  Result<U64> result;
  result.code = Code::ok;
  result.value = published;
  return result;
}

template <class X>
Task<X> storage_failure(InvocationContext ctx, Code code) {
  Result<X> failure;
  failure.code = code;
  failure.assessment = model::make_assessment(code);
  return ready<X>(ctx, std::move(failure));
}

// I/O stage body: writes every payload of the batch and renames completed
// canonical blocks into their content-addressed private paths.
Task<Done> submit_write_batch(InvocationContext ctx,
                              std::shared_ptr<Services> services,
                              std::shared_ptr<std::vector<WriteSegment>> segments) {
  auto node = detail::make_node(ctx, TaskSpec{});
  if (!services) {
    detail::node_complete(node, Code::invalid_request, Assessment{}, Usage{},
                          {}, detail::box_value<Done>(Done{}));
    return wrap_task<Done>(std::move(node));
  }
  services->submit_io(IoRequest{
      true, [node, segments = std::move(segments)]() {
        Code code = Code::ok;
        for (const WriteSegment& segment : *segments) {
          if (code != Code::ok) break;
          std::error_code ec;
          if (segment.payload && segment.payload_bytes > 0) {
            std::ofstream stream(segment.tmp_path,
                                 std::ios::binary | std::ios::app);
            if (!stream) {
              code = Code::io_error;
              break;
            }
            stream.write(
                reinterpret_cast<const char*>(segment.payload->values.data()),
                static_cast<std::streamsize>(segment.payload_bytes));
            stream.flush();
            if (!stream) code = Code::io_error;
          }
          if (code == Code::ok && segment.finalize) {
            std::error_code exists_ec;
            const bool final_exists =
                std::filesystem::exists(segment.final_path, exists_ec);
            if (final_exists) {
              std::filesystem::remove(segment.tmp_path, ec);
              if (!std::filesystem::exists(segment.tmp_path, exists_ec)) {
                // The tmp file may never have been created for an empty block.
                ec.clear();
              }
            } else {
              std::filesystem::rename(segment.tmp_path, segment.final_path,
                                      ec);
              if (ec) code = Code::io_error;
            }
          }
        }
        detail::node_complete(node, code, Assessment{}, Usage{}, {},
                              detail::box_value<Done>(Done{}));
      }});
  return wrap_task<Done>(std::move(node));
}

// ---------------------------------------------------------------------------
// Read helpers
// ---------------------------------------------------------------------------

using StoredBuffer = std::shared_ptr<Buffer<std::byte>>;

template <class T>
struct ReadState {
  InvocationContext ctx;
  std::shared_ptr<Services> services;
  ArenaRef arena;
  ArrayRef array;
  BlockRequest range{};
  U64 itemsize = 0;
  U64 total_elements = 0;
  bool done = false;
  Buffer<T> target;
  std::size_t next_block = 0;
  std::vector<BlockDescriptor> blocks;
};

template <class T>
Result<Done> ingest_block(InvocationContext task,
                          std::shared_ptr<ReadState<T>> state,
                          const BlockDescriptor& descriptor,
                          StoredBuffer stored);

template <class T>
Result<Done> prepare_read(InvocationContext task,
                          std::shared_ptr<ReadState<T>> state) {
  using D = DTypeOf<T>;
  if constexpr (!D::known) {
    return model::fail<Done>(Code::unsupported,
                             "the requested element type has no storage dtype",
                             "runtime.storage.read");
  } else {
    if (!state->services || !state->array || !state->arena) {
      return model::fail<Done>(Code::invalid_request, "missing read arguments",
                               "runtime.storage.read");
    }
    if (state->array->dtype != D::value) {
      return model::fail<Done>(Code::invalid_model,
                               "requested element type does not match array dtype",
                               "runtime.storage.read");
    }
    U64 total = 0;
    if (!shape_elements(state->array->shape, total)) {
      return model::fail<Done>(Code::invalid_model, "array shape overflow",
                               "runtime.storage.read");
    }
    state->total_elements = total;
    const U64 itemsize = model::dtype_size(state->array->dtype);
    if (itemsize == 0) {
      return model::fail<Done>(Code::unsupported,
                               "dyadic arrays are not served as typed byte reads",
                               "runtime.storage.read");
    }
    state->itemsize = itemsize;
    if (state->range.begin > total ||
        state->range.count > total - state->range.begin) {
      return model::fail<Done>(Code::invalid_request,
                               "block request is out of range",
                               "runtime.storage.read");
    }
    if (state->range.count == 0) {
      state->done = true;
      return model::ok_result(Done{});
    }
    auto buffer =
        allocate<T>(task, state->arena, state->range, nullptr,
                    state->array->unit, state->array->identity.content_hash);
    if (buffer.code != Code::ok || !buffer.value) {
      return model::fail<Done>(buffer.code, "read buffer allocation failed",
                               "runtime.storage.read");
    }
    state->target = std::move(*buffer.value);
    // Cache hit: copy the requested bytes into the freshly charged buffer.
    auto cached =
        state->services->blocks->find(state->array->identity.object_id);
    if (cached) {
      const U64 expected = total * itemsize;
      if (cached->size() < expected) {
        return model::fail<Done>(Code::corrupt_product,
                                 "registered array bytes are shorter than shape",
                                 "runtime.storage.read");
      }
      const U64 byte_begin = state->range.begin * itemsize;
      const U64 byte_count = state->range.count * itemsize;
      std::memcpy(state->target.values.data(), cached->data() + byte_begin,
                  static_cast<std::size_t>(byte_count));
      // A cache hit reads no bytes from the I/O lane; only the copy work is
      // charged (usage compute), not read_bytes.
      auto accounted = consume(task, work_units(byte_count));
      if (accounted.code != Code::ok) return accounted;
      state->done = true;
      return model::ok_result(Done{});
    }
    // Disk-backed blocks: resolve every descriptor and verify its closure.
    if (state->array->block_ids.empty()) {
      return model::fail<Done>(
          Code::missing_dependency,
          "the array has no block descriptors and is not cached",
          "runtime.storage.read");
    }
    std::vector<BlockDescriptor> known;
    known.reserve(state->array->block_ids.size());
    for (const Id& id : state->array->block_ids) {
      auto block = state->services->blocks->find_block(id);
      if (!block) {
        return model::fail<Done>(Code::missing_dependency,
                                 "unknown block descriptor " + id,
                                 "runtime.storage.read");
      }
      if (block->dtype != state->array->dtype ||
          block->shape != state->array->shape ||
          block->index_id != state->array->index_id ||
          !unit_equal(block->unit, state->array->unit)) {
        return model::fail<Done>(Code::invalid_model,
                                 "block descriptor disagrees with the array",
                                 "runtime.storage.read");
      }
      if (block->begin > total || block->count > total - block->begin) {
        return model::fail<Done>(Code::invalid_model,
                                 "block descriptor range is out of bounds",
                                 "runtime.storage.read");
      }
      if (block->raw_bytes != block->count * itemsize) {
        return model::fail<Done>(
            Code::invalid_model,
            "block byte count does not match its elements",
            "runtime.storage.read");
      }
      known.push_back(*block);
    }
    std::sort(known.begin(), known.end(),
              [](const BlockDescriptor& a, const BlockDescriptor& b) {
                return a.begin < b.begin;
              });
    const U64 required_begin = state->range.begin;
    const U64 required_end = state->range.begin + state->range.count;
    U64 cursor = required_begin;
    for (const BlockDescriptor& block : known) {
      if (block.count == 0) continue;
      const U64 block_end = block.begin + block.count;
      if (block_end <= required_begin) continue;
      if (block.begin >= required_end) break;
      if (block.begin > cursor) {
        return model::fail<Done>(
            Code::missing_dependency,
            "block descriptors leave a gap in the request",
            "runtime.storage.read");
      }
      state->blocks.push_back(block);
      cursor = std::max(cursor, block_end);
      if (cursor >= required_end) break;
    }
    if (cursor < required_end) {
      return model::fail<Done>(Code::missing_dependency,
                               "block descriptors do not cover the request",
                               "runtime.storage.read");
    }
    for (const BlockDescriptor& block : state->blocks) {
      if (block.path.empty()) {
        return model::fail<Done>(Code::missing_dependency,
                                 "block has no private path and is not cached",
                                 "runtime.storage.read");
      }
    }
    return model::ok_result(Done{});
  }
}

Task<StoredBuffer> submit_file_read(InvocationContext ctx,
                                    std::shared_ptr<Services> services,
                                    StoredBuffer buffer, std::string path,
                                    U64 expected_bytes) {
  auto node = detail::make_node(ctx, TaskSpec{});
  services->submit_io(IoRequest{
      false, [node, buffer = std::move(buffer), path = std::move(path),
              expected_bytes]() {
        Code code = Code::ok;
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
          code = Code::io_error;
        } else {
          stream.read(reinterpret_cast<char*>(buffer->values.data()),
                      static_cast<std::streamsize>(expected_bytes));
          if (static_cast<U64>(stream.gcount()) != expected_bytes)
            code = Code::io_error;
        }
        if (code != Code::ok) {
          detail::node_complete(node, code, Assessment{}, Usage{}, {}, nullptr);
          return;
        }
        detail::node_complete(node, Code::ok, Assessment{}, Usage{}, {},
                              detail::box_value<StoredBuffer>(buffer));
      }});
  return wrap_task<StoredBuffer>(std::move(node));
}

template <class T>
Result<Done> ingest_block(InvocationContext task,
                          std::shared_ptr<ReadState<T>> state,
                          const BlockDescriptor& descriptor,
                          StoredBuffer stored) {
  if (!stored) {
    return model::fail<Done>(Code::internal, "missing read staging buffer",
                             "runtime.storage.read");
  }
  std::shared_ptr<Buffer<std::byte>> raw = stored;
  if (descriptor.codec == BlockDescriptor::Codec::zstd) {
    auto expanded =
        detail::allocate_bytes(task, state->arena, descriptor.raw_bytes);
    if (expanded.code != Code::ok || !expanded.value) {
      return model::fail<Done>(expanded.code,
                               "decompression buffer allocation failed",
                               "runtime.storage.read");
    }
    raw = std::make_shared<Buffer<std::byte>>(std::move(*expanded.value));
    const std::size_t written =
        ZSTD_decompress(raw->values.data(), raw->values.size(),
                        stored->values.data(), stored->values.size());
    if (ZSTD_isError(written) || written != descriptor.raw_bytes) {
      return model::fail<Done>(Code::hash_mismatch,
                               "zstd block does not decode to raw_bytes",
                               "runtime.storage.read");
    }
  }
  if (raw->values.size() != descriptor.raw_bytes) {
    return model::fail<Done>(Code::corrupt_product,
                             "decoded block length mismatch",
                             "runtime.storage.read");
  }
  const std::string digest = model::sha256_hex(raw->values);
  if (model::content_id(digest) != descriptor.raw_sha256) {
    return model::fail<Done>(Code::hash_mismatch,
                             "block raw hash mismatch for " + descriptor.id,
                             "runtime.storage.read");
  }
  const U64 required_end = state->range.begin + state->range.count;
  const U64 overlap_begin = std::max(descriptor.begin, state->range.begin);
  const U64 overlap_end =
      std::min(descriptor.begin + descriptor.count, required_end);
  if (overlap_end > overlap_begin) {
    const U64 source_bytes =
        (overlap_begin - descriptor.begin) * state->itemsize;
    const U64 target_offset =
        (overlap_begin - state->range.begin) * sizeof(T);
    const U64 length = (overlap_end - overlap_begin) * state->itemsize;
    std::memcpy(reinterpret_cast<std::byte*>(state->target.values.data()) +
                    target_offset,
                raw->values.data() + source_bytes,
                static_cast<std::size_t>(length));
    detail::ledger_account_bytes(state->ctx.scope, stored->values.size(), 0,
                                 0);
    auto accounted = consume(task, work_units(length) + 1);
    if (accounted.code != Code::ok) return accounted;
  }
  state->next_block += 1;
  return model::ok_result(Done{});
}

template <class T>
Task<Block<T>> continue_read(std::shared_ptr<ReadState<T>> state) {
  if (state->done || state->next_block >= state->blocks.size()) {
    Block<T> block;
    block.range = state->range;
    block.unit = state->array->unit;
    block.model_hash = state->array->identity.content_hash;
    if (state->range.count > 0) {
      auto frozen = freeze<T>(std::move(state->target));
      if (frozen.code != Code::ok || !frozen.value) {
        Result<Block<T>> failure;
        failure.code = frozen.code;
        return ready<Block<T>>(state->ctx, std::move(failure));
      }
      return ready<Block<T>>(state->ctx, std::move(frozen));
    }
    return ready<Block<T>>(state->ctx, model::ok_result(std::move(block)));
  }
  const BlockDescriptor descriptor = state->blocks[state->next_block];
  const U64 stored = descriptor.codec == BlockDescriptor::Codec::zstd
                         ? descriptor.stored_bytes
                         : descriptor.raw_bytes;
  const InvocationContext ctx = state->ctx;
  auto allocate_task = submit_cpu<StoredBuffer>(
      ctx, stage_spec(0, "storage.read.stage", work_units(stored) + 64),
      [state, stored](InvocationContext task) -> Result<StoredBuffer> {
        auto buffer = detail::allocate_bytes(task, state->arena, stored);
        if (buffer.code != Code::ok || !buffer.value) {
          return model::fail<StoredBuffer>(buffer.code,
                                           "read staging allocation failed",
                                           "runtime.storage.read");
        }
        return model::ok_result(
            std::make_shared<Buffer<std::byte>>(std::move(*buffer.value)));
      });
  return then<StoredBuffer>(
      ctx, std::move(allocate_task),
      [state, descriptor, stored](StoredBuffer buffer) -> Task<Block<T>> {
        if (!buffer) {
          Result<Block<T>> failure;
          failure.code = Code::internal;
          return ready<Block<T>>(state->ctx, std::move(failure));
        }
        auto io = submit_file_read(state->ctx, state->services, buffer,
                                   descriptor.path, stored);
        return then<StoredBuffer>(
            state->ctx, std::move(io),
            [state, descriptor](StoredBuffer staged) -> Task<Block<T>> {
              auto ingest_task = submit_cpu<Done>(
                  state->ctx,
                  stage_spec(0, "storage.read.verify",
                             work_units(descriptor.raw_bytes) + 64),
                  [state, descriptor, staged](
                      InvocationContext task) -> Result<Done> {
                    return ingest_block<T>(task, state, descriptor, staged);
                  });
              return then<Done>(
                  state->ctx, std::move(ingest_task),
                  [state](Done) -> Task<Block<T>> {
                    return continue_read(state);
                  });
            });
      });
}

}  // namespace

// ---------------------------------------------------------------------------
// Public asynchronous storage entry points

// ---------------------------------------------------------------------------

Task<WriterRef> begin_async(InvocationContext ctx, ArenaRef arena,
                            BuildSpec spec) {
  auto make_failure = [ctx](Code code, std::string reason) {
    Result<WriterRef> failure;
    failure.code = code;
    failure.assessment = model::make_assessment(code);
    model::add_issue(failure.assessment, code, std::move(reason),
                     "runtime.storage.begin");
    return ready<WriterRef>(ctx, std::move(failure));
  };
  if (!ctx.services || !arena) {
    return make_failure(Code::invalid_request, "missing begin arguments");
  }
  const U64 sequence = g_writer_sequence.fetch_add(1) + 1;
  const U64 work = 16;
  return submit_cpu<WriterRef>(
      ctx, stage_spec(sequence, "storage.begin", work),
      [ctx, arena, spec, sequence](InvocationContext task) -> Result<WriterRef> {
        const DType dtype = spec.layout.dtype;
        U64 total = 0;
        if (!shape_elements(spec.layout.shape, total)) {
          return model::fail<WriterRef>(Code::invalid_request,
                                        "array shape overflows",
                                        "runtime.storage.begin");
        }
        U64 itemsize = 0;
        if (dtype == DType::dyadic) {
          if (spec.placement != Placement::spill) {
            return model::fail<WriterRef>(
                Code::unsupported,
                "dyadic begin requires spill placement (unknown raw size)",
                "runtime.storage.begin");
          }
        } else {
          itemsize = model::dtype_size(dtype);
          if (itemsize == 0) {
            return model::fail<WriterRef>(Code::unsupported,
                                          "unknown dtype",
                                          "runtime.storage.begin");
          }
        }
        if (spec.buffer_bytes == 0) {
          return model::fail<WriterRef>(Code::invalid_request,
                                        "buffer_bytes is zero",
                                        "runtime.storage.begin");
        }
        auto writer = std::make_shared<WriterState>();
        writer->ctx = ctx;
        writer->arena = arena;
        writer->layout = spec.layout;
        writer->placement = spec.placement;
        writer->dtype = dtype;
        writer->itemsize = itemsize;
        writer->total_elements = total;
        writer->buffer_bytes = spec.buffer_bytes;
        writer->metadata_bytes = spec.metadata_bytes;
        writer->use_zstd = spec.buffer_bytes >= 256u * 1024u && itemsize > 0;
        if (itemsize > 0) {
          writer->canonical_elements =
              std::max<U64>(1, (1u << 20) / itemsize);
        } else {
          writer->canonical_elements = 1024;
        }
        if (writer->placement == Placement::memory) {
          const U64 total_bytes = total * itemsize;
          if (total_bytes > 0) {
            auto charged = charge_vector(task, arena, total_bytes);
            if (charged.code != Code::ok || !charged.value) {
              return model::fail<WriterRef>(charged.code,
                                            "output capacity is insufficient",
                                            "runtime.storage.begin");
            }
            writer->memory = std::move(*charged.value);
            writer->memory_bytes = total_bytes;
          }
        } else {
          writer->directory = scratch_directory(ctx, sequence);
          std::error_code ec;
          std::filesystem::create_directories(writer->directory, ec);
          if (ec) {
            return model::fail<WriterRef>(
                Code::io_error,
                "cannot create the private spool directory",
                "runtime.storage.begin");
          }
        }
        hash_array_header(writer->array_hash, dtype, spec.layout.shape,
                          spec.layout.unit,
                          spec.layout.index
                              ? spec.layout.index->identity.content_hash
                              : Id{});
        writer->hash_seeded = true;
        auto accounted = consume(task, work);
        if (accounted.code != Code::ok) {
          cleanup_writer(*writer);
          return model::fail<WriterRef>(accounted.code,
                                        "work budget exhausted",
                                        "runtime.storage.begin");
        }
        return model::ok_result(
            std::make_shared<ArrayWriter>(std::move(writer)));
      });
}

template <class T>
Task<U64> append_async(InvocationContext ctx, WriterRef writer, Block<T> block) {
  using D = DTypeOf<T>;
  auto failure_task = [ctx](Code code, std::string reason) {
    Result<U64> failure;
    failure.code = code;
    failure.assessment = model::make_assessment(code);
    model::add_issue(failure.assessment, code, std::move(reason),
                     "runtime.storage.append");
    return ready<U64>(ctx, std::move(failure));
  };
  if (!writer || !writer->state || !ctx.services) {
    return failure_task(Code::invalid_request, "missing writer");
  }
  auto state = writer->state;
  const U64 count = block.values.size();
  if constexpr (!D::known) {
    return failure_task(Code::unsupported, "unsupported append element type");
  } else {
    if (count == 0) {
      return failure_task(Code::invalid_request, "append of zero elements");
    }
    if (!block.index) {
      return failure_task(Code::invalid_model, "block has no index");
    }
    if (block.values.size() != block.range.count) {
      return failure_task(Code::invalid_model,
                          "block values length does not match its range");
    }
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (state->status != WriterState::Status::open) {
        return failure_task(Code::invalid_request,
                            "writer is not open for append");
      }
      if (state->append_in_flight) {
        return failure_task(Code::invalid_request,
                            "another append is already in flight");
      }
      if (state->dtype != D::value) {
        return failure_task(Code::invalid_model,
                            "block dtype does not match the writer");
      }
      if (state->itemsize != 0 && sizeof(T) != state->itemsize) {
        return failure_task(Code::invalid_model,
                            "element size does not match the writer dtype");
      }
      if (!unit_equal(block.unit, state->layout.unit)) {
        return failure_task(Code::invalid_model,
                            "block unit does not match the writer");
      }
      const Id writer_index =
          state->layout.index ? state->layout.index->identity.content_hash
                              : Id{};
      if (!block.index || block.index->identity.content_hash != writer_index) {
        return failure_task(Code::invalid_model,
                            "block index does not match the writer");
      }
      if (!state->model_hash_set) {
        state->model_hash = block.model_hash;
        state->model_hash_set = true;
      } else if (block.model_hash != state->model_hash) {
        return failure_task(Code::invalid_model,
                            "block model hash differs from the writer model");
      }
      if (block.range.begin != state->next_element) {
        return failure_task(Code::invalid_request,
                            "append range is not contiguous");
      }
      if (state->next_element > state->total_elements ||
          block.range.count >
              state->total_elements - state->next_element) {
        return failure_task(Code::invalid_request,
                            "append exceeds the declared element count");
      }
      if (!block.lease.owner) {
        return failure_task(Code::invalid_model,
                            "append does not hold a data lease");
      }
      state->append_in_flight = true;
    }
    const U64 encoded_work = count * std::max<U64>(state->itemsize, 1);
    const std::uint64_t sequence = state->stage_sequence.fetch_add(1);
    auto encode_task = submit_cpu<AppendBatch>(
        ctx, stage_spec(sequence, "storage.append.encode",
                        work_units(encoded_work) + 64),
        [state, block](InvocationContext task) -> Result<AppendBatch> {
          return encode_append<T>(task, state, block);
        });
    return settle<AppendBatch>(
        ctx, std::move(encode_task),
        [ctx, state, count](Result<AppendBatch> batch) -> Task<U64> {
          if (batch.code != Code::ok || !batch.value) {
            auto finished = finish_append(state, Result<Done>{batch.code}, 0, 0);
            Result<U64> failure;
            failure.code = batch.code;
            return ready<U64>(ctx, std::move(failure));
          }
          if (state->placement == Placement::memory) {
            Result<Done> ok;
            ok.code = Code::ok;
            ok.value = Done{};
            auto finished = finish_append(
                state, std::move(ok), count,
                count * std::max<U64>(state->itemsize, 1));
            if (finished.code != Code::ok) {
              Result<U64> failure;
              failure.code = finished.code;
              return ready<U64>(ctx, std::move(failure));
            }
            return ready<U64>(ctx, std::move(finished));
          }
          U64 stored_bytes = 0;
          for (const WriteSegment& segment : batch.value->segments)
            stored_bytes += segment.payload_bytes;
          auto segments =
              std::make_shared<std::vector<WriteSegment>>(
                  std::move(*batch.value).segments);
          auto io_task =
              submit_write_batch(ctx, ctx.services, std::move(segments));
          return settle<Done>(
              ctx, std::move(io_task),
              [ctx, state, count, stored_bytes](Result<Done> io) -> Task<U64> {
                auto finished = finish_append(state, std::move(io), count,
                                              stored_bytes);
                if (finished.code != Code::ok) {
                  Result<U64> failure;
                  failure.code = finished.code;
                  return ready<U64>(ctx, std::move(failure));
                }
                return ready<U64>(ctx, std::move(finished));
              });
        });
  }
}

Task<ArrayRef> seal_async(InvocationContext ctx, WriterRef writer) {
  auto failure_task = [ctx](Code code, std::string reason) {
    Result<ArrayRef> failure;
    failure.code = code;
    failure.assessment = model::make_assessment(code);
    model::add_issue(failure.assessment, code, std::move(reason),
                     "runtime.storage.seal");
    return ready<ArrayRef>(ctx, std::move(failure));
  };
  if (!writer || !writer->state || !ctx.services) {
    return failure_task(Code::invalid_request, "missing writer");
  }
  auto state = writer->state;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->status == WriterState::Status::sealed) {
      return failure_task(Code::invalid_request, "writer is already sealed");
    }
    if (state->status == WriterState::Status::aborted) {
      return failure_task(Code::invalid_request, "writer was aborted");
    }
    if (state->status == WriterState::Status::failed) {
      return failure_task(state->failure_code, state->failure_reason);
    }
    if (state->append_in_flight) {
      return failure_task(Code::invalid_request,
                          "seal while an append is in flight");
    }
    if (state->seal_in_flight) {
      return failure_task(Code::invalid_request,
                          "another seal is already in flight");
    }
    if (state->next_element != state->total_elements) {
      return failure_task(Code::invalid_request,
                          "seal requires exactly the declared element count");
    }
    // Freeze the writer: seal becomes the only in-flight terminal operation;
    // abort from now on only requests the abort and is completed by this seal.
    state->seal_in_flight = true;
  }
  const std::uint64_t sequence = state->stage_sequence.fetch_add(1);
  auto prepare = submit_cpu<SealPrep>(
      ctx, stage_spec(sequence, "storage.seal.prepare", 64),
      [state](InvocationContext task) -> Result<SealPrep> {
        SealPrep prep;
        std::vector<std::shared_ptr<detail::TaskNode>> waiters;
        bool cleanup = false;
        {
          std::lock_guard<std::mutex> lock(state->mutex);
          if (state->block_open && state->block_elements > 0) {
            std::string error;
            auto finished =
                finish_block(*state, prep.segments, error);
            if (finished.code != Code::ok) {
              fail_writer_locked(*state, finished.code, error);
              return model::fail<SealPrep>(finished.code, error,
                                           "runtime.storage.seal");
            }
          }
          prep.needs_io = state->placement == Placement::spill &&
                          !prep.segments.empty();
        }
        auto accounted = consume(task, 16);
        if (accounted.code != Code::ok) {
          std::lock_guard<std::mutex> lock(state->mutex);
          fail_writer_locked(*state, accounted.code, "work budget exhausted");
          return model::fail<SealPrep>(accounted.code, "work budget exhausted",
                                       "runtime.storage.seal");
        }
        return model::ok_result(std::move(prep));
      });
  return settle<SealPrep>(
      ctx, std::move(prepare),
      [ctx, state](Result<SealPrep> prep) -> Task<ArrayRef> {
        if (prep.code != Code::ok || !prep.value) {
          std::vector<std::shared_ptr<detail::TaskNode>> waiters;
          bool cleanup = false;
          {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->seal_in_flight = false;
            if (state->abort_requested) {
              waiters = mark_aborted_locked(*state);
              cleanup = true;
            }
          }
          if (cleanup) {
            cleanup_writer(*state);
            complete_abort_waiters(std::move(waiters));
          }
          Result<ArrayRef> failure;
          failure.code = cleanup ? Code::cancelled : prep.code;
          return ready<ArrayRef>(ctx, std::move(failure));
        }
        auto finalize = [ctx, state](Result<Done> io) -> Task<ArrayRef> {
          std::vector<std::shared_ptr<detail::TaskNode>> waiters;
          bool cleanup = false;
          bool aborted = false;
          Code failure_code = Code::ok;
          ArrayRef sealed_array;
          {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->seal_in_flight = false;
            if (state->abort_requested) {
              waiters = mark_aborted_locked(*state);
              cleanup = true;
              aborted = true;
            } else if (state->status == WriterState::Status::failed) {
              failure_code = state->failure_code;
            } else if (state->status != WriterState::Status::open) {
              failure_code = Code::invalid_request;
            } else if (io.code != Code::ok) {
              fail_writer_locked(*state, io.code, "seal write failed");
              failure_code = io.code;
            } else {
              const std::string hex = state->array_hash.hex();
              auto identity =
                  model::make_identity("acsd.Array", 2, 0, hex, "acsd");
              if (state->placement == Placement::memory) {
                if (auto alias = alias_bytes(state->memory)) {
                  Lease lease;
                  if (state->memory && state->memory->allocation)
                    lease.owner = std::static_pointer_cast<const void>(
                        state->memory->allocation);
                  ctx.services->blocks->register_array(
                      identity.object_id, alias, state->dtype,
                      state->layout.shape, std::move(lease));
                }
              }
              ctx.services->blocks->register_blocks(identity.object_id,
                                                    state->blocks);
              // Exactly one reference per sealed ArrayRef; the custom deleter
              // releases it so the entry (and its Lease) is never pinned.
              ctx.services->blocks->add_ref(identity.object_id);
              release_zstd(*state);
              state->status = WriterState::Status::sealed;
              Array array;
              array.identity = identity;
              array.assessment = model::make_assessment(Code::ok);
              array.assessment.complete = true;
              array.assessment.execution = ExecutionState::complete;
              array.dtype = state->dtype;
              array.shape = state->layout.shape;
              array.unit = state->layout.unit;
              array.index_id = state->layout.index
                                   ? state->layout.index->identity.content_hash
                                   : Id{};
              array.block_ids = state->block_ids;
              struct ArrayReaper {
                std::weak_ptr<Services> services;
                Id id;
                void operator()(const Array* value) const {
                  delete value;
                  if (auto owner = services.lock())
                    if (owner->blocks) owner->blocks->release_ref(id);
                }
              };
              sealed_array = ArrayRef(
                  new Array(std::move(array)),
                  ArrayReaper{ctx.services, identity.object_id});
            }
          }
          if (cleanup) {
            cleanup_writer(*state);
            complete_abort_waiters(std::move(waiters));
            return storage_failure<ArrayRef>(ctx, Code::cancelled);
          }
          if (failure_code != Code::ok)
            return storage_failure<ArrayRef>(ctx, failure_code);
          (void)aborted;
          return ready<ArrayRef>(ctx, model::ok_result(std::move(sealed_array)));
        };
        if (!prep.value->needs_io) {
          Result<Done> ok;
          ok.code = Code::ok;
          ok.value = Done{};
          return finalize(std::move(ok));
        }
        auto segments = std::make_shared<std::vector<WriteSegment>>(
            std::move(prep.value->segments));
        auto io_task = submit_write_batch(ctx, ctx.services, std::move(segments));
        return settle<Done>(ctx, std::move(io_task),
                            [finalize](Result<Done> io) mutable -> Task<ArrayRef> {
                              return finalize(std::move(io));
                            });
      });
}

Task<Done> abort_async(InvocationContext ctx, WriterRef writer) {
  auto failure_task = [ctx](Code code) {
    Result<Done> failure;
    failure.code = code;
    failure.assessment = model::make_assessment(code);
    return ready<Done>(ctx, std::move(failure));
  };
  if (!writer || !writer->state || !ctx.services) {
    return failure_task(Code::invalid_request);
  }
  auto state = writer->state;
  auto out = detail::make_node(ctx, TaskSpec{});
  bool deferred = false;
  bool idle = false;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->status == WriterState::Status::sealed ||
        state->status == WriterState::Status::aborted) {
      detail::node_complete(out, Code::ok, Assessment{}, Usage{}, {},
                            detail::box_value<Done>(Done{}));
      return wrap_task<Done>(std::move(out));
    }
    if (state->append_in_flight || state->seal_in_flight) {
      state->abort_requested = true;
      state->abort_waiters.push_back(out);
      deferred = true;
    } else {
      state->status = WriterState::Status::aborted;
      idle = true;
    }
  }
  if (deferred) return wrap_task<Done>(std::move(out));
  // Run the cleanup in the uncancelled begin context when possible: the private
  // spool belongs to that scope, and a cancelled caller must not strand it.
  const InvocationContext cleanup_ctx = state->ctx.services ? state->ctx : ctx;
  auto cleanup_task = submit_cpu<Done>(
      cleanup_ctx, cleanup_spec(8),
      [state](InvocationContext) -> Result<Done> {
        cleanup_writer(*state);
        return model::ok_result(Done{});
      });
  auto cleanup_node = detail::TaskAccess::release<Done>(std::move(cleanup_task));
  detail::node_set_continuation(cleanup_node, [cleanup_node, out]() {
    detail::node_complete_from(out, cleanup_node);
  });
  (void)idle;
  return wrap_task<Done>(std::move(out));
}

template <class T>
Task<Block<T>> read_async(InvocationContext ctx, ArenaRef arena,
                          ArrayRef array, BlockRequest range) {
  auto state = std::make_shared<ReadState<T>>();
  state->ctx = ctx;
  state->services = ctx.services;
  state->arena = std::move(arena);
  state->array = std::move(array);
  state->range = range;
  auto prepare = submit_cpu<Done>(
      ctx,
      stage_spec(0, "storage.read.prepare",
                 64 + (range.count > 0 ? work_units(range.count * 8) : 0)),
      [state](InvocationContext task) -> Result<Done> {
        return prepare_read<T>(task, state);
      });
  return then<Done>(ctx, std::move(prepare),
                    [state](Done) -> Task<Block<T>> {
                      return continue_read(state);
                    });
}

// Explicit instantiations for the fixed-width payload types.
template Task<Block<double>> read_async<double>(InvocationContext, ArenaRef,
                                                ArrayRef, BlockRequest);
template Task<Block<float>> read_async<float>(InvocationContext, ArenaRef,
                                              ArrayRef, BlockRequest);
template Task<Block<std::uint64_t>> read_async<std::uint64_t>(
    InvocationContext, ArenaRef, ArrayRef, BlockRequest);
template Task<Block<std::uint32_t>> read_async<std::uint32_t>(
    InvocationContext, ArenaRef, ArrayRef, BlockRequest);
template Task<Block<std::uint16_t>> read_async<std::uint16_t>(
    InvocationContext, ArenaRef, ArrayRef, BlockRequest);
template Task<Block<std::uint8_t>> read_async<std::uint8_t>(
    InvocationContext, ArenaRef, ArrayRef, BlockRequest);
template Task<Block<std::byte>> read_async<std::byte>(InvocationContext,
                                                      ArenaRef, ArrayRef,
                                                      BlockRequest);

template Task<U64> append_async<double>(InvocationContext, WriterRef,
                                        Block<double>);
template Task<U64> append_async<float>(InvocationContext, WriterRef,
                                       Block<float>);
template Task<U64> append_async<std::uint64_t>(InvocationContext, WriterRef,
                                               Block<std::uint64_t>);
template Task<U64> append_async<std::uint32_t>(InvocationContext, WriterRef,
                                               Block<std::uint32_t>);
template Task<U64> append_async<std::uint16_t>(InvocationContext, WriterRef,
                                               Block<std::uint16_t>);
template Task<U64> append_async<std::uint8_t>(InvocationContext, WriterRef,
                                              Block<std::uint8_t>);
template Task<U64> append_async<std::byte>(InvocationContext, WriterRef,
                                           Block<std::byte>);
template Task<U64> append_async<Dyadic>(InvocationContext, WriterRef,
                                        Block<Dyadic>);

// ---------------------------------------------------------------------------
// Block store (declared in internal.hpp, defined here as the single owner of
// the runtime block/array cache)
// ---------------------------------------------------------------------------

namespace detail {

void BlockStore::register_array(
    const Id& id, std::shared_ptr<const std::vector<std::byte>> bytes,
    DType dtype, std::vector<U64> shape, Lease lease) {
  if (!bytes) return;
  std::lock_guard<std::mutex> lock(mutex_);
  auto& entry = entries_[id];
  entry.bytes = std::move(bytes);
  entry.dtype = dtype;
  entry.shape = std::move(shape);
  // The entry keeps the capacity Lease alive as long as the stored bytes do.
  entry.lease = std::move(lease);
}

void BlockStore::add_ref(const Id& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_[id].refs += 1;
}

bool BlockStore::release_ref(const Id& id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = entries_.find(id);
  if (it == entries_.end()) return true;
  if (it->second.refs > 1) {
    it->second.refs -= 1;
    return false;
  }
  entries_.erase(it);
  return true;
}

void BlockStore::register_blocks(const Id& array_id,
                                 const std::vector<BlockDescriptor>& blocks) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& entry = entries_[array_id];
  entry.blocks = blocks;
  for (const BlockDescriptor& block : blocks) block_index_[block.id] = block;
}

std::shared_ptr<const std::vector<std::byte>> BlockStore::find(
    const Id& id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = entries_.find(id);
  if (it == entries_.end()) return nullptr;
  return it->second.bytes;
}

std::optional<BlockDescriptor> BlockStore::find_block(
    const Id& block_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = block_index_.find(block_id);
  if (it == block_index_.end()) return std::nullopt;
  return it->second;
}

void BlockStore::retain(const Id& id, Lease lease) {
  if (!lease.owner) return;
  std::lock_guard<std::mutex> lock(mutex_);
  entries_[id].lease = std::move(lease);
}

Lease BlockStore::lease_of(const Id& id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = entries_.find(id);
  if (it == entries_.end()) return Lease{};
  return it->second.lease;
}

}  // namespace detail

}  // namespace acsd::rt
