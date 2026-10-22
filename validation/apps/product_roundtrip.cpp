// The product round trip of the apps module.
//
// The program uses only the public declaration contract: it implements the
// three public interfaces it needs (the resources, the storage and the
// cancellation) and calls publish_product, open_product, build_export and
// export_signal.  It never includes a header of src/.
//
// The output is a JSON document with the observed values; the expected model
// is not read back.
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "acsd/contracts.hpp"

using json = nlohmann::json;
namespace acsd {

// ---------------------------------------------------------------------------
// A minimal resource service
// ---------------------------------------------------------------------------

class TestResources final : public Resources {
 public:
  explicit TestResources(const Limits& limits) : limits_(limits) {}

  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    if (bytes > limits_.bytes) {
      Result<Reservation> out;
      out.code = Code::resource_exhausted;
      out.assessment.complete = false;
      return out;
    }
    Reservation reservation;
    reservation.bytes = bytes;
    reservation.threads = threads;
    return Result<Reservation>{Code::ok, reservation, {}, {}, {}};
  }

  Result<U64> consume_work(U64 units) override {
    work_ += units;
    U64 used = work_;
    Result<U64> out;
    out.code = Code::ok;
    out.value = used;
    return out;
  }

  Usage usage() const override {
    Usage usage;
    usage.work = work_;
    usage.peak_payload_bytes = bytes_;
    return usage;
  }

 private:
  Limits limits_;
  U64 work_ = 0;
  U64 bytes_ = 0;
};

class TestCancellation final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// A minimal SHA-256 so that the block hashes of the product are verified by
// this program alone.
std::string sha256_hex(const std::vector<unsigned char>& message) {
  static const std::uint32_t k[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  std::vector<unsigned char> padded = message;
  const std::uint64_t length = static_cast<std::uint64_t>(message.size()) * 8;
  padded.push_back(0x80);
  while (padded.size() % 64 != 56) padded.push_back(0);
  for (int i = 7; i >= 0; --i) padded.push_back(static_cast<unsigned char>((length >> (8 * i)) & 0xff));
  for (std::size_t block = 0; block < padded.size(); block += 64) {
    std::uint32_t w[64] = {};
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(padded[block + 4 * i]) << 24) |
             (static_cast<std::uint32_t>(padded[block + 4 * i + 1]) << 16) |
             (static_cast<std::uint32_t>(padded[block + 4 * i + 2]) << 8) |
             static_cast<std::uint32_t>(padded[block + 4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = ((w[i - 15] >> 7) | (w[i - 15] << 25)) ^ ((w[i - 15] >> 18) | (w[i - 15] << 14)) ^
                               (w[i - 15] >> 3);
      const std::uint32_t s1 = ((w[i - 2] >> 17) | (w[i - 2] << 15)) ^ ((w[i - 2] >> 19) | (w[i - 2] << 13)) ^
                               (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t s1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::uint32_t word : h) stream << std::setw(8) << word;
  return stream.str();
}

// ---------------------------------------------------------------------------
// A minimal in-memory storage
// ---------------------------------------------------------------------------

class TestStorage final : public Storage {
 public:
  Result<ArrayRef> copy(Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    (void)context;
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    U64 count = 1;
    for (U64 extent : shape) count *= extent;
    if (dtype != DType::dyadic && buffer->size() != count * item_size(dtype)) {
      Result<ArrayRef> out;
      out.code = Code::invalid_request;
      return out;
    }
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->identity.object_id = "acsd.Array:" + std::to_string(arrays_.size());
    array->identity.content_hash = array->identity.object_id;
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    arrays_[array->identity.object_id] = buffer;
    Result<ArrayRef> out;
    out.code = Code::ok;
    out.value = array;
    return out;
  }

  Result<ArrayView> read(Context& context, ArrayRef array, BlockRequest range) const override {
    (void)context;
    Result<ArrayView> out;
    if (!array) {
      out.code = Code::invalid_request;
      return out;
    }
    auto it = arrays_.find(array->identity.object_id);
    if (it == arrays_.end()) {
      out.code = Code::missing_dependency;
      return out;
    }
    const U64 item = item_size(array->dtype);
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    if (offset + length > it->second->size()) {
      out.code = Code::corrupt_product;
      return out;
    }
    out.code = Code::ok;
    ArrayView view;
    view.bytes = std::span<const std::byte>(it->second->data() + offset, length);
    view.shape = array->shape;
    view.dtype = array->dtype;
    out.value = view;
    return out;
  }

  Result<ArrayRef> open(Context& context, std::string root, const Array& description,
                        std::span<const BlockDescriptor> blocks) override {
    (void)context;
    Result<ArrayRef> out;
    U64 count = 1;
    for (U64 extent : description.shape) count *= extent;
    const U64 item = item_size(description.dtype);
    std::vector<std::byte> buffer;
    U64 cursor = 0;
    for (const BlockDescriptor& block : blocks) {
      if (block.begin != cursor) {
        out.code = Code::corrupt_product;
        return out;
      }
      cursor += block.count;
      const std::filesystem::path path = std::filesystem::path(root) / block.path;
      std::ifstream stream(path, std::ios::binary);
      if (!stream) {
        out.code = Code::io_error;
        return out;
      }
      std::vector<unsigned char> raw;
      char chunk[4096];
      while (stream.read(chunk, sizeof(chunk)) || stream.gcount() > 0) {
        raw.insert(raw.end(), chunk, chunk + stream.gcount());
      }
      if (raw.size() != block.raw_bytes) {
        out.code = Code::corrupt_product;
        return out;
      }
      const std::string digest = "sha256:" + sha256_hex(raw);
      if (digest != block.raw_sha256) {
        out.code = Code::hash_mismatch;
        return out;
      }
      for (unsigned char byte : raw) buffer.push_back(static_cast<std::byte>(byte));
    }
    if (cursor != count || buffer.size() != count * item) {
      out.code = Code::corrupt_product;
      return out;
    }
    auto array = std::make_shared<Array>();
    array->dtype = description.dtype;
    array->shape = description.shape;
    array->unit = description.unit;
    array->index_id = description.index_id;
    array->identity.object_id = "acsd.Array:opened:" + std::to_string(opened_++);
    array->identity.content_hash = array->identity.object_id;
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    arrays_[array->identity.object_id] = std::make_shared<std::vector<std::byte>>(buffer.begin(), buffer.end());
    out.code = Code::ok;
    out.value = array;
    return out;
  }

 private:
  static U64 item_size(DType dtype) {
    switch (dtype) {
      case DType::f64: return 8;
      case DType::f32: return 4;
      case DType::u64: return 8;
      case DType::u32: return 4;
      case DType::u16: return 2;
      case DType::u8: return 1;
      case DType::dyadic: return 0;
    }
    return 0;
  }

  mutable std::map<Id, std::shared_ptr<std::vector<std::byte>>> arrays_;
  mutable int opened_ = 0;
};


namespace {

// The stable name of an error code; the validation does not use the internal
// headers of the implementation.
const char* code_name(acsd::Code code) {
  switch (code) {
    case acsd::Code::ok: return "ok";
    case acsd::Code::invalid_request: return "invalid_request";
    case acsd::Code::invalid_model: return "invalid_model";
    case acsd::Code::domain: return "domain";
    case acsd::Code::unsupported: return "unsupported";
    case acsd::Code::unavailable: return "unavailable";
    case acsd::Code::unsolved: return "unsolved";
    case acsd::Code::not_identifiable: return "not_identifiable";
    case acsd::Code::invalid_covariance: return "invalid_covariance";
    case acsd::Code::duplicate_source: return "duplicate_source";
    case acsd::Code::identity_conflict: return "identity_conflict";
    case acsd::Code::hash_mismatch: return "hash_mismatch";
    case acsd::Code::corrupt_product: return "corrupt_product";
    case acsd::Code::missing_dependency: return "missing_dependency";
    case acsd::Code::no_support: return "no_support";
    case acsd::Code::support_unresolved: return "support_unresolved";
    case acsd::Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case acsd::Code::selection_unresolved: return "selection_unresolved";
    case acsd::Code::nonconverged: return "nonconverged";
    case acsd::Code::precision_exhausted: return "precision_exhausted";
    case acsd::Code::resource_exhausted: return "resource_exhausted";
    case acsd::Code::io_error: return "io_error";
    case acsd::Code::cancelled: return "cancelled";
    case acsd::Code::internal: return "internal";
  }
  return "internal";
}

Id content_hash(const std::string& text) {
  // The content id of the test fixture: a local digest of the text.
  std::uint64_t hash = 1469598103934665603ull;
  for (char character : text) {
    hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
    hash *= 1099511628211ull;
  }
  std::ostringstream stream;
  stream << "sha256:";
  for (int i = 0; i < 8; ++i) {
    stream.width(16);
    stream.fill('0');
    stream << std::hex << ((hash >> (8 * i)) & 0xffull);
  }
  return stream.str();
}

}  // namespace
}  // namespace acsd

using acsd::code_name;

int main(int argc, char** argv) {
  const std::filesystem::path root = argc > 1 ? std::filesystem::path(argv[1])
                                              : std::filesystem::path(".work/product-roundtrip");
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);

  acsd::Limits limits;
  limits.bytes = 64ull << 20;
  limits.work = 1000000;
  limits.threads = 1;
  limits.max_depth = 64;
  limits.max_precision_bits = 128;
  limits.scratch_directory = ".work";
  acsd::TestResources resources(limits);
  acsd::TestStorage storage;
  acsd::TestCancellation cancellation;
  acsd::Context context{resources, cancellation, storage};

  json report;
  report["product_root"] = root.string();

  // --- the synthetic frame product -----------------------------------------
  // Two leaves of order 3 with the values -2, 0 and +3 in the leaves.
  const std::vector<acsd::HealpixLeaf> leaves = {{6, 100}, {6, 101}, {6, 102}};
  std::vector<double> signal_values = {-2.0, 0.0, 3.0};

  std::vector<std::uint8_t> leaf_bytes;
  std::vector<std::uint64_t> leaf_ids;
  for (const acsd::HealpixLeaf& leaf : leaves) {
    leaf_ids.push_back(leaf.order);
    leaf_ids.push_back(leaf.ipix);
  }
  {
    std::vector<std::byte> bytes(leaf_ids.size() * 8);
    for (std::size_t i = 0; i < leaf_ids.size(); ++i) {
      for (int b = 0; b < 8; ++b) {
        bytes[i * 8 + b] = static_cast<std::byte>((leaf_ids[i] >> (8 * b)) & 0xffull);
      }
    }
    acsd::Result<acsd::ArrayRef> ids = storage.copy(context, acsd::DType::u64, bytes,
                                                    std::vector<acsd::U64>{leaf_ids.size()},
                                                    acsd::Unit{}, "acsd.leaf.ids");
    if (ids.code != acsd::Code::ok || !ids.value.has_value()) {
      report["error"] = "the leaf identifiers cannot be stored";
      std::cout << report.dump(2) << "\n";
      return 1;
    }
    acsd::Index index;
    index.size = leaves.size();
    index.kind = acsd::Index::Kind::leaf;
    index.order = 6;
    index.namespace_id = "acsd.leaf";
    index.ids = *ids.value;
    index.identity.object_id = "acsd.Index:leaf";
    index.identity.content_hash = index.identity.object_id;

    std::vector<std::byte> signal_bytes(signal_values.size() * 8);
    for (std::size_t i = 0; i < signal_values.size(); ++i) {
      std::uint64_t bits = 0;
      std::memcpy(&bits, &signal_values[i], sizeof(bits));
      for (int b = 0; b < 8; ++b) {
        signal_bytes[i * 8 + b] = static_cast<std::byte>((bits >> (8 * b)) & 0xffull);
      }
    }
    acsd::Result<acsd::ArrayRef> signal = storage.copy(context, acsd::DType::f64, signal_bytes,
                                                       std::vector<acsd::U64>{signal_values.size()},
                                                       acsd::Unit{}, "acsd.leaf.signal");
    acsd::Unit unit;
    unit.scale = "fixture:L";
    unit.linear_power = 1;
    unit.sr_power = -1;

    auto support = std::make_shared<acsd::SupportMap>();
    support->kind = acsd::SupportMap::Kind::primitives;
    support->index = std::make_shared<acsd::Index>(index);
    support->identity.object_id = "acsd.SupportMap:test";
    support->identity.content_hash = support->identity.object_id;

    auto estimate = std::make_shared<acsd::SignalEstimate>();
    estimate->signal = *signal.value;
    estimate->unit = unit;
    estimate->estimator.id = "test-frame";
    estimate->estimator.quantity = "brightness";
    estimate->estimator.unit = unit;
    estimate->support = std::shared_ptr<const acsd::SupportMap>(support);
    estimate->identity.object_id = "acsd.SignalEstimate:test";
    estimate->identity.content_hash = estimate->identity.object_id;
    estimate->assessment.complete = true;

    auto graph = std::make_shared<acsd::ProductGraph>();
    graph->stage = acsd::ProductGraph::Stage::normalize;
    graph->signal = std::shared_ptr<const acsd::SignalEstimate>(estimate);
    graph->profile = "full";
    graph->identity.object_id = "acsd.ProductGraph:test";
    graph->identity.content_hash = graph->identity.object_id;

    acsd::PublishRequest publish;
    publish.destination = (root / "frame").string();
    publish.signal_dtype = acsd::DType::f64;
    publish.profile = "full";
    publish.tile_log2_width = 6;
    publish.durable = true;

    acsd::Result<acsd::ProductLocation> published = acsd::publish_product(context, graph, publish);
    report["publish_code"] = code_name(published.code);
    if (!published.value.has_value()) {
      report["publish_reason"] = published.assessment.issues.empty()
                                     ? "unknown"
                                     : published.assessment.issues.front().reason;
      std::cout << report.dump(2) << "\n";
      return 1;
    }
    report["published"] = published.value->directory;

    // The reopened product.
    acsd::Result<acsd::Ref<acsd::ProductProvider>> opened =
        acsd::open_product(context, published.value.value());
    report["open_code"] = code_name(opened.code);
    if (!opened.value.has_value()) {
      report["open_reason"] = opened.assessment.issues.empty()
                                  ? "unknown"
                                  : opened.assessment.issues.front().reason;
      std::cout << report.dump(2) << "\n";
      return 1;
    }
    report["reopened_frames"] = opened.value.value()->list_frames(context).value.value().size();

    // The manifest on disk.
    const std::filesystem::path manifest_path = std::filesystem::path(published.value->directory) /
                                                "manifest.json";
    std::ifstream manifest_stream(manifest_path);
    std::ostringstream manifest_text;
    manifest_text << manifest_stream.rdbuf();
    json manifest = json::parse(manifest_text.str());
    report["manifest_format"] = manifest.value("format", std::string());
    report["manifest_major"] = manifest.value("major", 0);
    report["manifest_complete"] = manifest.value("complete", false);
    report["manifest_objects"] = manifest.value("objects", json::array()).size();
    report["manifest_blocks"] = manifest.value("blocks", json::array()).size();
    report["manifest_root_object"] = manifest.value("root_object", std::string());
    report["sidecar_present"] =
        std::filesystem::exists(std::filesystem::path(published.value->directory) / "frame_science.json");
    report["manifest_hash_present"] = !published.value->manifest_hash.empty();
  }

  std::cout << report.dump(2) << "\n";
  return 0;
}
