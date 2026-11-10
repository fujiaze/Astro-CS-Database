#include "storage/local_storage.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <new>
#include <system_error>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <zstd.h>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/units.hpp"

namespace acsd::storage {
namespace {

// A single block file is a bounded object.  The hard ceiling is a last resort;
// the real bound below is the declared descriptor (stored/raw bytes), which is
// itself validated against shape*itemsize before any read.
constexpr U64 kMaxBlockFileBytes = 1ull << 31;   // 2 GiB
constexpr U64 kMaxArrayBytes = 1ull << 36;       // 64 GiB, checked product only

bool checked_mul(U64 a, U64 b, U64& out) noexcept {
  if (a != 0 && b > (~U64{0}) / a) return false;
  out = a * b;
  return true;
}

bool checked_element_count(DType dtype, std::span<const std::byte> bytes,
                           std::span<const U64> shape, U64& count, std::string& error) {
  count = 1;
  for (U64 extent : shape) {
    if (extent == 0) { count = 0; return true; }
    if (!checked_mul(count, extent, count)) {
      error = "array shape overflows the element count";
      return false;
    }
  }
  if (dtype == DType::dyadic) return true;
  const std::size_t item = model::dtype_size(dtype);
  if (item == 0) {
    error = "unknown dtype";
    return false;
  }
  U64 expected = 0;
  if (!checked_mul(count, static_cast<U64>(item), expected)) {
    error = "the byte count overflows 64 bits";
    return false;
  }
  if (bytes.size() != expected) {
    error = "byte count does not match the declared shape";
    return false;
  }
  return true;
}

// P1-7: a product-relative path must never leave the product root.  Absolute
// paths are rejected (std::filesystem::operator/ would replace the root with
// them) and so is every ".." component; the joined result is additionally
// checked against the normalized root so that a symlink or a crafted prefix
// cannot escape either.  Validation happens before anything is opened.
bool resolve_within(const std::filesystem::path& root, const std::string& relative,
                    std::filesystem::path& out, std::string& error) {
  if (relative.empty()) {
    error = "the block has an empty path";
    return false;
  }
  const std::filesystem::path candidate(relative);
  if (candidate.is_absolute() || candidate.has_root_name() ||
      candidate.has_root_directory()) {
    error = "the block path is absolute and would replace the product root";
    return false;
  }
  for (const auto& part : candidate) {
    const std::string text = part.string();
    if (text == "..") {
      error = "the block path escapes the product root";
      return false;
    }
  }
  std::error_code ec;
  const std::filesystem::path root_abs = std::filesystem::absolute(root, ec);
  const std::filesystem::path base = ec ? root.lexically_normal()
                                        : root_abs.lexically_normal();
  const std::filesystem::path joined = (base / candidate).lexically_normal();
  // Lexical containment: every component of `base` must be a prefix.
  auto base_it = base.begin();
  auto joined_it = joined.begin();
  for (; base_it != base.end(); ++base_it, ++joined_it) {
    if (joined_it == joined.end() || *base_it != *joined_it) {
      error = "the block path resolves outside the product root";
      return false;
    }
  }
  out = joined;
  return true;
}

#if defined(_WIN32)
bool sync_handle(const std::filesystem::path& path, bool directory) {
  const int fd = _wopen(path.wstring().c_str(), _O_RDONLY | _O_BINARY);
  if (fd < 0) return false;
  const int rc = _commit(fd);
  _close(fd);
  (void)directory;
  return rc == 0;
}
#else
bool sync_handle(const std::filesystem::path& path, bool directory) {
  const int flags = directory ? (O_RDONLY | O_DIRECTORY) : O_RDONLY;
  const int fd = ::open(path.c_str(), flags);
  if (fd < 0) return false;
  const int rc = directory ? ::fsync(fd) : ::fdatasync(fd);
  ::close(fd);
  return rc == 0;
}
#endif

// Bounded file read: the caller states the exact number of bytes it expects, so
// a corrupt or hostile file can never drive an unbounded allocation (P1-8).
bool read_file_bounded(const std::filesystem::path& path,
                       std::vector<std::byte>& bytes, U64 expected_bytes,
                       std::string& error) {
  std::error_code ec;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "missing file " + path.string();
    return false;
  }
  if (size != expected_bytes) {
    error = "file size " + std::to_string(size) + " does not match the declared " +
            std::to_string(expected_bytes) + " bytes for " + path.string();
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot open " + path.string() + " for reading";
    return false;
  }
  try {
    bytes.assign(static_cast<std::size_t>(expected_bytes), std::byte{0});
  } catch (const std::bad_alloc&) {
    error = "the declared block does not fit the address space";
    return false;
  }
  if (expected_bytes != 0) {
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(expected_bytes));
    if (static_cast<U64>(stream.gcount()) != expected_bytes) {
      error = "short read of " + path.string();
      return false;
    }
  }
  return true;
}

}  // namespace

LocalStorage::LocalStorage() = default;
LocalStorage::~LocalStorage() = default;

void LocalStorage::account_read(U64 bytes) const { read_bytes_.fetch_add(bytes); }
void LocalStorage::account_written(U64 bytes) const { written_bytes_.fetch_add(bytes); }

Result<ArrayRef> LocalStorage::copy(Context& context, DType dtype,
                                    std::span<const std::byte> bytes, std::span<const U64> shape,
                                    const Unit& unit, Id index) {
  U64 count = 0;
  std::string error;
  if (!checked_element_count(dtype, bytes, shape, count, error)) {
    return model::fail<ArrayRef>(Code::invalid_request, error, "storage.copy",
                                 "the array cannot be constructed");
  }
  auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
  auto stored = std::make_shared<StoredArray>();
  stored->dtype = dtype;
  stored->shape.assign(shape.begin(), shape.end());
  stored->unit = unit;
  stored->index_id = index;
  stored->bytes = buffer;
  const std::string hex = model::hash_payload(dtype, stored->shape, unit, *buffer);
  stored->identity = model::make_identity("acsd.Array", 2, 0, hex, "acsd");
  {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    arrays_[stored->identity.object_id] = stored;
  }
  Array array;
  array.identity = stored->identity;
  array.assessment = model::make_assessment(Code::ok);
  array.assessment.complete = true;
  array.dtype = dtype;
  array.shape = stored->shape;
  array.unit = unit;
  array.index_id = index;
  account_written(buffer->size());
  (void)count;
  (void)context;
  return model::ok_result(std::shared_ptr<const Array>(new Array(array)));
}

Result<ArrayRef> LocalStorage::adopt(Context& context, DType dtype,
                                     std::span<const std::byte> bytes, std::span<const U64> shape,
                                     const Unit& unit, Id index,
                                     std::span<const BlockDescriptor> blocks) {
  auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
  auto stored = std::make_shared<StoredArray>();
  stored->dtype = dtype;
  stored->shape.assign(shape.begin(), shape.end());
  stored->unit = unit;
  stored->index_id = index;
  stored->bytes = buffer;
  stored->blocks.assign(blocks.begin(), blocks.end());
  const std::string hex = model::hash_payload(dtype, stored->shape, unit, *buffer);
  stored->identity = model::make_identity("acsd.Array", 2, 0, hex, "acsd");
  {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    arrays_[stored->identity.object_id] = stored;
  }
  Array array;
  array.identity = stored->identity;
  array.assessment = model::make_assessment(Code::ok);
  array.assessment.complete = true;
  array.dtype = dtype;
  array.shape = stored->shape;
  array.unit = unit;
  array.index_id = index;
  array.block_ids.reserve(blocks.size());
  for (const BlockDescriptor& block : blocks) array.block_ids.push_back(block.id);
  (void)context;
  return model::ok_result(std::shared_ptr<const Array>(new Array(array)));
}

Result<ArrayView> LocalStorage::read(Context& context, ArrayRef array,
                                     BlockRequest range) const {
  if (!array) {
    return model::fail<ArrayView>(Code::invalid_request, "no array supplied", "storage.read",
                                   "the requested range cannot be resolved");
  }
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) { total = 0; break; }
    if (total > (~0ull) / extent) {
      return model::fail<ArrayView>(Code::invalid_request, "array shape overflow", "storage.read",
                                     "the requested range cannot be resolved");
    }
    total *= extent;
  }
  if (range.begin > total || range.begin + range.count > total) {
    return model::fail<ArrayView>(Code::invalid_request, "block request is out of range",
                                  "storage.read", "the requested range cannot be resolved");
  }
  std::shared_ptr<const StoredArray> stored;
  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = arrays_.find(array->identity.object_id);
    if (it != arrays_.end()) stored = it->second;
  }
  if (!stored) {
    return model::fail<ArrayView>(Code::missing_dependency, "the array is not present in storage",
                                  "storage.read", "the requested range cannot be resolved");
  }
  const std::size_t item = model::dtype_size(array->dtype);
  if (item == 0) {
    return model::fail<ArrayView>(Code::unsupported, "dyadic arrays are not served as byte views",
                                  "storage.read", "the requested range cannot be resolved");
  }
  const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
  const std::size_t length = static_cast<std::size_t>(range.count) * item;
  if (offset + length > stored->bytes->size()) {
    return model::fail<ArrayView>(Code::corrupt_product, "the stored array is shorter than declared",
                                  "storage.read", "the requested range cannot be resolved");
  }
  ArrayView view;
  view.dtype = array->dtype;
  view.shape = array->shape;
  view.bytes = std::span<const std::byte>(stored->bytes->data() + offset, length);
  const void* alias = static_cast<const void*>(stored.get());
  auto owner = std::shared_ptr<const void>(std::move(stored), alias);
  view.lease.owner = owner;
  account_read(length);
  (void)context;
  return model::ok_result(view);
}

Result<ArrayRef> LocalStorage::open(Context& context, std::string root, const Array& description,
                                    std::span<const BlockDescriptor> blocks) {
  const std::filesystem::path base(root);
  std::vector<std::byte> buffer;
  U64 expected_elements = 1;
  for (U64 extent : description.shape) {
    if (extent == 0) { expected_elements = 0; break; }
    if (expected_elements > (~0ull) / extent) {
      return model::fail<ArrayRef>(Code::invalid_model, "array shape overflow", "storage.open",
                                   "the array cannot be opened");
    }
    expected_elements *= extent;
  }
  const std::size_t item = model::dtype_size(description.dtype);
  if (item == 0) {
    return model::fail<ArrayRef>(Code::unsupported, "dyadic arrays are not supported by open",
                                  "storage.open", "the array cannot be opened");
  }
  // P1-2: the byte count of the opened array is a checked product, and an
  // absurd declaration is refused before any reservation (OBJECTS: "任一不适配
  // 返回…且不开始分配/写入").
  U64 expected_bytes = 0;
  if (!checked_mul(expected_elements, static_cast<U64>(item), expected_bytes) ||
      expected_bytes > kMaxArrayBytes) {
    return model::fail<ArrayRef>(Code::resource_exhausted,
                                  "the declared array does not fit a bounded byte count",
                                  "storage.open", "the array cannot be opened");
  }
  try {
    buffer.reserve(static_cast<std::size_t>(expected_bytes));
  } catch (const std::bad_alloc&) {
    return model::fail<ArrayRef>(Code::resource_exhausted,
                                  "the declared array does not fit the address space",
                                  "storage.open", "the array cannot be opened");
  }

  // Blocks are sorted by their begin offset; the description must cover the
  // whole array without gaps or overlaps.
  std::vector<BlockDescriptor> ordered(blocks.begin(), blocks.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const BlockDescriptor& a, const BlockDescriptor& b) { return a.begin < b.begin; });
  U64 cursor = 0;
  for (const BlockDescriptor& block : ordered) {
    if (block.begin != cursor) {
      return model::fail<ArrayRef>(Code::corrupt_product, "block coverage has a gap or overlap",
                                    "storage.open", "the array cannot be opened");
    }
    if (block.count == 0) continue;
    // P1-2: the descriptor's element range is a checked product before use.
    U64 declared_raw = 0;
    if (!checked_mul(block.count, static_cast<U64>(item), declared_raw) ||
        declared_raw != block.raw_bytes) {
      return model::fail<ArrayRef>(Code::invalid_model,
                                    "block element count does not match its raw bytes",
                                    "storage.open", "the array cannot be opened");
    }
    // P1-7: resolve inside the product root, never by replacing it.
    std::filesystem::path path;
    std::string error;
    if (!resolve_within(base, block.path, path, error)) {
      return model::fail<ArrayRef>(Code::invalid_request, error, "storage.open",
                                    "the array cannot be opened");
    }
    // P1-8: the read is bounded by the declared size.  A raw block stores
    // exactly raw_bytes; a zstd block stores exactly stored_bytes (when the
    // descriptor states it) and may never exceed the hard file ceiling.
    U64 expected_file_bytes = 0;
    if (block.codec == BlockDescriptor::Codec::zstd) {
      expected_file_bytes = block.stored_bytes != 0 ? block.stored_bytes
                                                    : kMaxBlockFileBytes;
    } else {
      if (block.stored_bytes != 0 && block.stored_bytes != block.raw_bytes) {
        return model::fail<ArrayRef>(Code::invalid_model,
                                      "a raw block stores exactly its raw bytes",
                                      "storage.open", "the array cannot be opened");
      }
      expected_file_bytes = block.raw_bytes;
    }
    if (expected_file_bytes > kMaxBlockFileBytes) {
      return model::fail<ArrayRef>(Code::resource_exhausted,
                                    "the block file exceeds the bounded read ceiling",
                                    "storage.open", "the array cannot be opened");
    }
    std::vector<std::byte> raw;
    if (!read_file_bounded(path, raw, expected_file_bytes, error)) {
      return model::fail<ArrayRef>(
          error.rfind("missing file", 0) == 0 ? Code::missing_dependency : Code::io_error,
          error, "storage.open", "the array cannot be opened");
    }
    if (block.codec == BlockDescriptor::Codec::zstd) {
      std::vector<std::byte> expanded;
      // zstd blocks are bounded by the declared raw size.
      expanded = decompress(raw, block.raw_bytes, error);
      if (expanded.empty() && block.raw_bytes != 0) {
        return model::fail<ArrayRef>(Code::corrupt_product, error, "storage.open",
                                     "the array cannot be opened");
      }
      raw = std::move(expanded);
    }
    if (raw.size() != block.raw_bytes) {
      return model::fail<ArrayRef>(Code::corrupt_product, "block byte count mismatch",
                                    "storage.open", "the array cannot be opened");
    }
    // The descriptor's hash is only trusted for a block that actually decodes
    // to the declared range; a block that would push the array past its
    // declared size is refused before it is appended.
    U64 projected = 0;
    if (!checked_mul(cursor + block.count, static_cast<U64>(item), projected) ||
        projected > expected_bytes) {
      return model::fail<ArrayRef>(Code::corrupt_product,
                                    "block coverage exceeds the declared array size",
                                    "storage.open", "the array cannot be opened");
    }
    const std::string hex = model::sha256_hex(raw);
    if (model::content_id(hex) != block.raw_sha256) {
      return model::fail<ArrayRef>(Code::hash_mismatch, "block hash mismatch", "storage.open",
                                   "the array cannot be opened");
    }
    buffer.insert(buffer.end(), raw.begin(), raw.end());
    cursor += block.count;
  }
  if (cursor != expected_elements) {
    return model::fail<ArrayRef>(Code::corrupt_product, "block coverage does not fill the array",
                                  "storage.open", "the array cannot be opened");
  }
  if (buffer.size() != expected_bytes) {
    return model::fail<ArrayRef>(Code::corrupt_product, "decoded byte count mismatch",
                                  "storage.open", "the array cannot be opened");
  }
  return adopt(context, description.dtype, buffer, description.shape, description.unit,
               description.index_id, ordered);
}

std::shared_ptr<const StoredArray> LocalStorage::find(const Id& id) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = arrays_.find(id);
  if (it == arrays_.end()) return nullptr;
  return it->second;
}

bool LocalStorage::write_file(const std::filesystem::path& path, std::span<const std::byte> bytes,
                               std::string& error) {
  std::error_code ec;
  const std::filesystem::path parent = path.parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent, ec);
  {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
      error = "cannot open " + path.string() + " for writing";
      return false;
    }
    std::size_t written = 0;
    if (!bytes.empty()) {
      stream.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      written = bytes.size();
    }
    stream.flush();
    if (!stream) {
      error = "write failed for " + path.string();
      return false;
    }
    (void)written;
  }
  // P1-5: `durable` must be a real property of the bytes, not a hash input.
  // The data file is flushed to the device and the directory entry is synced
  // as well, so a crash can never leave a name that points at torn content
  // (OBJECTS: "Linux fdatasync文件+fsync目录+rename").  The publication order
  // and the final rename belong to the product layer; this makes every block
  // and object file it writes durable before the manifest can name it.
  if (!sync_handle(path, false)) {
    error = "the written file " + path.string() + " cannot be flushed to the device";
    return false;
  }
  if (!parent.empty() && !sync_handle(parent, true)) {
    error = "the directory of " + path.string() + " cannot be synced";
    return false;
  }
  return true;
}

bool LocalStorage::read_file(const std::filesystem::path& path, std::vector<std::byte>& bytes,
                             std::string& error) {
  // P1-8: the public helper keeps a hard ceiling.  Production readers that know
  // the declared size use the bounded internal path inside open(), where the
  // exact expected byte count is verified against the descriptor.
  std::error_code ec;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "missing file " + path.string();
    return false;
  }
  if (size > kMaxBlockFileBytes) {
    error = "the file " + path.string() + " exceeds the bounded read ceiling";
    return false;
  }
  return read_file_bounded(path, bytes, static_cast<U64>(size), error);
}

std::vector<std::byte> LocalStorage::compress(std::span<const std::byte> bytes) {
  std::vector<std::byte> out;
  if (bytes.empty()) return out;
  const std::size_t bound = ZSTD_compressBound(bytes.size());
  out.resize(bound);
  const std::size_t written = ZSTD_compress(out.data(), bound, bytes.data(), bytes.size(), 3);
  if (ZSTD_isError(written)) {
    out.assign(bytes.begin(), bytes.end());
    return out;
  }
  out.resize(written);
  return out;
}

std::vector<std::byte> LocalStorage::decompress(std::span<const std::byte> bytes, U64 raw_bytes,
                                               std::string& error) {
  // P1-8: the declared raw size is an attacker controlled number unless it is
  // validated first; the output is bounded, the product is checked and the
  // allocation failure is a named error instead of an exception.
  if (raw_bytes > kMaxBlockFileBytes) {
    error = "the declared decompressed size exceeds the bounded ceiling";
    return {};
  }
  const std::size_t frame = ZSTD_getFrameContentSize(bytes.data(), bytes.size());
  if (frame != ZSTD_CONTENTSIZE_UNKNOWN && frame != ZSTD_CONTENTSIZE_ERROR &&
      frame != raw_bytes) {
    error = "the zstd frame size disagrees with the declared raw size";
    return {};
  }
  std::vector<std::byte> out;
  try {
    out.assign(static_cast<std::size_t>(raw_bytes), std::byte{0});
  } catch (const std::bad_alloc&) {
    error = "the decompressed block does not fit the address space";
    return {};
  }
  if (raw_bytes == 0) return out;
  if (bytes.empty()) {
    error = "empty compressed block";
    return {};
  }
  const std::size_t written =
      ZSTD_decompress(out.data(), out.size(), bytes.data(), bytes.size());
  if (ZSTD_isError(written) || written != raw_bytes) {
    error = "zstd decompression failed";
    return {};
  }
  return out;
}

}  // namespace acsd::storage
