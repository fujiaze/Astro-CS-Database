#include "storage/local_storage.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#include <zstd.h>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/units.hpp"

namespace acsd::storage {
namespace {

bool checked_element_count(DType dtype, std::span<const std::byte> bytes,
                           std::span<const U64> shape, U64& count, std::string& error) {
  count = 1;
  for (U64 extent : shape) {
    if (extent == 0) { count = 0; return true; }
    if (count > (~0ull) / extent) {
      error = "array shape overflows the element count";
      return false;
    }
    count *= extent;
  }
  if (dtype == DType::dyadic) return true;
  const std::size_t item = model::dtype_size(dtype);
  if (item == 0) {
    error = "unknown dtype";
    return false;
  }
  if (bytes.size() != count * item) {
    error = "byte count does not match the declared shape";
    return false;
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
  if (range.begin > range.count || range.begin + range.count > total) {
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
  const U64 expected_bytes = expected_elements * item;
  buffer.reserve(expected_bytes);

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
    const std::filesystem::path path = base / block.path;
    std::vector<std::byte> raw;
    std::string error;
    if (!read_file(path, raw, error)) {
      return model::fail<ArrayRef>(Code::io_error, error, "storage.open",
                                   "the array cannot be opened");
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
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "cannot open " + path.string() + " for writing";
    return false;
  }
  if (!bytes.empty()) {
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }
  stream.flush();
  if (!stream) {
    error = "write failed for " + path.string();
    return false;
  }
  return true;
}

bool LocalStorage::read_file(const std::filesystem::path& path, std::vector<std::byte>& bytes,
                             std::string& error) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    error = "missing file " + path.string();
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot open " + path.string() + " for reading";
    return false;
  }
  bytes.clear();
  char buffer[65536];
  while (stream.good()) {
    stream.read(buffer, sizeof(buffer));
    const std::streamsize got = stream.gcount();
    if (got <= 0) break;
    for (std::streamsize i = 0; i < got; ++i)
      bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(buffer[i])));
  }
  return true;
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
  std::vector<std::byte> out(raw_bytes);
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
