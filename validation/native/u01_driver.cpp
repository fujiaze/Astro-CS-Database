// U01 native in-memory API driver.
//
// Independence rules honoured by this file:
//   * it includes ONLY the public declaration contract <acsd/contracts.hpp>;
//   * it does not include any production internal header and does not
//     re-implement any science algorithm;
//   * it only moves fields and arrays into the public API and reports what the
//     API returned;
//   * the truth values compared against this output are produced by an
//     independent Python oracle (exact rational arithmetic) and never read
//     back from this program.
//
// The driver implements the three public service interfaces (Storage,
// Resources, Cancellation) itself, which the API contract explicitly allows
// for an independent in-process caller.

#include <acsd/contracts.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using acsd::Array;
using acsd::ArrayRef;
using acsd::ArrayView;
using acsd::BlockRequest;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::DomainUnits;
using acsd::Id;
using acsd::Identity;
using acsd::Index;
using acsd::LinearAction;
using acsd::Ref;
using acsd::Result;
using acsd::Unit;
using acsd::U32;
using acsd::U64;

// ---------------------------------------------------------------------------
// tiny helpers (the production Result helpers live in src/ and must not be
// included from an independent caller)
// ---------------------------------------------------------------------------

template <class T>
bool ok(const Result<T>& r) {
  return r.code == Code::ok;
}

template <class T>
const T& val(const Result<T>& r) {
  return *r.value;
}

template <class T>
Result<T> failure(Code code, const std::string& reason) {
  Result<T> r;
  r.code = code;
  return r;
}

template <class T>
std::string reason_of(const Result<T>& r) {
  if (!r.assessment.issues.empty()) return r.assessment.issues.front().reason;
  return "no issue recorded";
}

// ---------------------------------------------------------------------------
// SHA-256 (public domain style implementation) for stable content identities
// ---------------------------------------------------------------------------

class Sha256 {
 public:
  Sha256() { reset(); }

  void reset() {
    state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    length_ = 0;
    buffer_used_ = 0;
  }

  void update(const std::uint8_t* data, std::size_t size) {
    length_ += static_cast<std::uint64_t>(size) * 8u;
    while (size > 0) {
      const std::size_t take = std::min(size, sizeof(buffer_) - buffer_used_);
      std::memcpy(buffer_.data() + buffer_used_, data, take);
      buffer_used_ += take;
      data += take;
      size -= take;
      if (buffer_used_ == sizeof(buffer_)) {
        transform(buffer_.data());
        buffer_used_ = 0;
      }
    }
  }

  std::string hex() {
    std::uint8_t pad[72] = {0x80};
    std::size_t pad_len = (buffer_used_ < 56) ? (56 - buffer_used_) : (120 - buffer_used_);
    const std::uint64_t bit_length = length_;
    update(pad, pad_len);
    std::uint8_t tail[8];
    for (int i = 0; i < 8; ++i) tail[i] = static_cast<std::uint8_t>((bit_length >> (56 - 8 * i)) & 0xffu);
    length_ -= 64;  // the length bytes are not part of the message length
    update(tail, 8);
    std::ostringstream out;
    for (std::uint32_t word : state_) {
      char chunk[9];
      std::snprintf(chunk, sizeof(chunk), "%08x", word);
      out << chunk;
    }
    return out.str();
  }

 private:
  static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

  void transform(const std::uint8_t* block) {
    static const std::uint32_t k[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24) |
             (static_cast<std::uint32_t>(block[4 * i + 1]) << 16) |
             (static_cast<std::uint32_t>(block[4 * i + 2]) << 8) |
             static_cast<std::uint32_t>(block[4 * i + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
      const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const std::uint32_t ch = (e & f) ^ ((~e) & g);
      const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
      const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t t2 = s0 + maj;
      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
  }

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t buffer_used_ = 0;
  std::uint64_t length_ = 0;
};

std::string sha256_hex(const std::string& data) {
  Sha256 hash;
  hash.update(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
  return hash.hex();
}

std::string content_id(const std::string& payload) {
  return "sha256:" + sha256_hex(payload);
}

// ---------------------------------------------------------------------------
// public service interfaces implemented by this independent caller
// ---------------------------------------------------------------------------

class ValidationStorage final : public acsd::Storage {
 public:
  struct Entry {
    std::shared_ptr<std::vector<std::byte>> bytes;
    std::vector<U64> shape;
    Unit unit;
    DType dtype = DType::f64;
  };

  // A lenient mode is used only to expose the payload size that the public
  // encoder produces; the strict mode mirrors the production storage rule
  // (bytes.size() must equal count * item size).
  void set_lenient(bool lenient) { lenient_ = lenient; }
  std::vector<std::string> observed_payload_ratios;

  Result<ArrayRef> copy(acsd::Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    (void)context;
    U64 count = 1;
    for (U64 extent : shape) {
      if (extent == 0) return failure<ArrayRef>(Code::invalid_request, "zero sized array");
      count *= extent;
    }
    const std::size_t item = item_size(dtype);
    if (bytes.size() != static_cast<std::size_t>(count) * item) {
      std::ostringstream ratio;
      ratio << "dtype=" << static_cast<int>(dtype) << " bytes=" << bytes.size()
            << " expected=" << (static_cast<std::size_t>(count) * item) << " elements=" << count
            << " item_size=" << item;
      observed_payload_ratios.push_back(ratio.str());
      if (!lenient_) {
        return failure<ArrayRef>(Code::invalid_request, "byte count does not match the shape");
      }
    }
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->identity = make_identity("acsd.Array", index, dtype_name(dtype), count, unit, *buffer);
    Entry entry;
    entry.bytes = buffer;
    entry.shape = array->shape;
    entry.unit = unit;
    entry.dtype = dtype;
    entries_[array->identity.content_hash] = entry;
    Result<ArrayRef> out;
    out.code = Code::ok;
    out.value = ArrayRef(array);
    return out;
  }

  Result<ArrayView> read(acsd::Context& context, ArrayRef array, BlockRequest range) const override {
    (void)context;
    if (!array) return failure<ArrayView>(Code::invalid_request, "no array supplied");
    auto it = entries_.find(array->identity.content_hash);
    if (it == entries_.end()) {
      return failure<ArrayView>(Code::invalid_request, "unknown array");
    }
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    if (range.begin + range.count > total) {
      return failure<ArrayView>(Code::invalid_request, "the range is out of bounds");
    }
    const std::size_t item = item_size(array->dtype);
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(it->second.bytes->data() + offset, length);
    view.lease.owner = it->second.bytes;
    Result<ArrayView> out;
    out.code = Code::ok;
    out.value = view;
    return out;
  }

  Result<ArrayRef> open(acsd::Context& context, std::string root, const Array& description,
                         std::span<const acsd::BlockDescriptor> blocks) override {
    (void)context; (void)root; (void)description; (void)blocks;
    return failure<ArrayRef>(Code::unsupported, "the validation storage does not open products");
  }

 private:
  static std::size_t item_size(DType dtype) {
    switch (dtype) {
      case DType::f64: return 8;
      case DType::f32: return 4;
      case DType::u64: return 8;
      case DType::u32: return 4;
      case DType::u16: return 2;
      case DType::u8: return 1;
      default: return 0;
    }
  }

  static const char* dtype_name(DType dtype) {
    switch (dtype) {
      case DType::f64: return "f64";
      case DType::f32: return "f32";
      case DType::u64: return "u64";
      case DType::u32: return "u32";
      case DType::u16: return "u16";
      case DType::u8: return "u8";
      default: return "dyadic";
    }
  }

  static Identity make_identity(const char* schema, const Id& object_id, const char* dtype,
                                U64 count, const Unit& unit,
                                const std::vector<std::byte>& payload_bytes) {
    std::ostringstream payload;
    payload << schema << '|' << object_id << '|' << dtype << '|' << count << '|' << unit.scale
            << '|' << unit.linear_power << '|' << unit.sr_power << '|' << unit.pixel_power << '|'
            << unit.nm_power << '|' << unit.multiplier << '|';
    Sha256 hash;
    hash.update(reinterpret_cast<const std::uint8_t*>(payload_bytes.data()), payload_bytes.size());
    payload << hash.hex();
    Identity identity;
    identity.object_id = object_id;
    identity.content_hash = content_id(payload.str());
    identity.run_id = "validation-u01";
    identity.revision = 0;
    identity.schema_id = schema;
    identity.schema_major = 2;
    identity.schema_minor = 0;
    return identity;
  }

  std::map<std::string, Entry> entries_;
  bool lenient_ = false;
};

class ValidationResources final : public acsd::Resources {
 public:
  acsd::Result<acsd::Reservation> reserve(U64 bytes, U32 threads) override {
    if (bytes > budget_bytes_ || threads > threads_) {
      acsd::Result<acsd::Reservation> out;
      out.code = Code::resource_exhausted;
      return out;
    }
    auto lease = std::make_shared<std::vector<std::byte>>(1);
    acsd::Reservation reservation;
    reservation.lease.owner = lease;
    reservation.bytes = bytes;
    reservation.threads = threads;
    acsd::Result<acsd::Reservation> out;
    out.code = Code::ok;
    out.value = reservation;
    return out;
  }

  acsd::Result<U64> consume_work(U64 units) override {
    if (work_ + units > work_budget_) {
      acsd::Result<U64> out;
      out.code = Code::resource_exhausted;
      return out;
    }
    work_ += units;
    acsd::Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    return out;
  }

  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = work_;
    usage.peak_payload_bytes = payload_;
    return usage;
  }

  void set_budget(U64 bytes, U64 work, U32 threads) {
    budget_bytes_ = bytes;
    work_budget_ = work;
    threads_ = threads;
  }

 private:
  U64 budget_bytes_ = 1ull << 30;
  U64 work_budget_ = 1ull << 30;
  U32 threads_ = 8;
  U64 work_ = 0;
  U64 payload_ = 0;
};

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// ---------------------------------------------------------------------------
// fixture construction
// ---------------------------------------------------------------------------

Unit unit_signal() {
  Unit unit;
  unit.scale = "fixture:L";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit unit_brightness() {
  Unit unit = unit_signal();
  unit.sr_power = -1;
  return unit;
}

Unit unit_variance_brightness() {
  Unit unit = unit_signal();
  unit.sr_power = -1;
  unit.linear_power = 2;
  return unit;
}

Unit unit_variance_signal() {
  Unit unit = unit_signal();
  unit.linear_power = 2;
  return unit;
}

Unit unit_dimensionless() {
  Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

std::shared_ptr<Index> make_index(const std::string& namespace_id, U64 size,
                                   Index::Kind kind) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = kind;
  index->order = 0;
  index->namespace_id = namespace_id;
  index->identity.object_id = namespace_id;
  index->identity.content_hash = content_id("index|" + namespace_id + "|" + std::to_string(size));
  index->identity.run_id = "validation-u01";
  index->identity.revision = 0;
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  index->assessment.execution = acsd::ExecutionState::complete;
  index->assessment.capability = acsd::Capability::supported;
  index->assessment.validity = acsd::Validity::certified;
  return index;
}

Result<ArrayRef> copy_f64(acsd::Context& context, const std::vector<double>& values,
                          const Unit& unit, const Id& index_id) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  std::memcpy(bytes.data(), values.data(), bytes.size());
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

Result<ArrayRef> copy_u64(acsd::Context& context, const std::vector<U64>& values,
                          const Id& index_id) {
  std::vector<std::byte> bytes(values.size() * sizeof(U64));
  std::memcpy(bytes.data(), values.data(), bytes.size());
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, DType::u64, bytes, shape, unit_dimensionless(), index_id);
}

Result<Ref<LinearAction>> make_csr(acsd::Context& context, const std::vector<U64>& offsets,
                                   const std::vector<U64>& indices, const std::vector<double>& values,
                                   const Ref<Index>& domain, const Ref<Index>& codomain,
                                   const Unit& input_unit, const Unit& output_unit) {
  Result<ArrayRef> offsets_array = copy_u64(context, offsets, domain->identity.object_id + ":csrrow");
  if (!ok(offsets_array)) return failure<Ref<LinearAction>>(offsets_array.code, "offsets copy failed");
  Result<ArrayRef> indices_array = copy_u64(context, indices, domain->identity.object_id + ":csrnnz");
  if (!ok(indices_array)) return failure<Ref<LinearAction>>(indices_array.code, "indices copy failed");
  Result<ArrayRef> values_array = copy_f64(context, values, output_unit, domain->identity.object_id + ":csrnnz");
  if (!ok(values_array)) return failure<Ref<LinearAction>>(values_array.code, "values copy failed");

  acsd::ActionRecipe recipe;
  recipe.kind = acsd::ActionRecipe::Kind::csr;
  recipe.algorithm = "csr_v1";
  recipe.version = 1;
  recipe.spaces.domain = domain;
  recipe.spaces.codomain = codomain;
  recipe.spaces.input = input_unit;
  recipe.spaces.output = output_unit;
  const std::vector<Ref<LinearAction>> children;
  const std::vector<ArrayRef> arrays{val(offsets_array), val(indices_array), val(values_array)};
  recipe.arrays = {val(offsets_array)->identity.object_id, val(indices_array)->identity.object_id,
                   val(values_array)->identity.object_id};
  return acsd::make_action(context, recipe, children, arrays);
}

Result<Ref<LinearAction>> make_identity_action(acsd::Context& context, const Ref<Index>& index,
                                               const Unit& unit) {
  acsd::ActionRecipe recipe;
  recipe.kind = acsd::ActionRecipe::Kind::identity;
  recipe.algorithm = "identity_v1";
  recipe.version = 1;
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  const std::vector<Ref<LinearAction>> children;
  const std::vector<ArrayRef> arrays;
  return acsd::make_action(context, recipe, children, arrays);
}

std::vector<double> read_all(acsd::Context& context, ArrayRef array) {
  std::vector<double> out;
  if (!array) return out;
  const U64 count = array->shape.empty() ? 0 : array->shape[0];
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) return out;
  const auto* raw = reinterpret_cast<const double*>(val(view).bytes.data());
  out.assign(raw, raw + count);
  return out;
}

// ---------------------------------------------------------------------------
// JSON emission
// ---------------------------------------------------------------------------

std::string number(double value) {
  if (value == static_cast<double>(static_cast<long long>(value)) &&
      std::abs(value) < 1e15) {
    std::ostringstream out;
    out.precision(17);
    out << value;
    return out.str();
  }
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

std::string quote(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else {
      out += c;
    }
  }
  out += '"';
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  std::string output_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
  }

  ValidationStorage storage;
  ValidationResources resources;
  NeverCancelled cancellation;
  resources.set_budget(1ull << 30, 1ull << 30, 8);
  acsd::Context context{resources, cancellation, storage};

  std::ostringstream json;
  json << "{\n";
  json << "  \"driver\": \"u01_native\",\n";
  json << "  \"route\": \"in_memory_public_api\",\n";
  json << "  \"results\": [\n";

  auto emit = [&](const std::string& name, const std::string& status, const std::string& detail,
                  const std::vector<std::pair<std::string, double>>& values,
                  const std::vector<std::pair<std::string, std::string>>& texts) {
    json << "    {\"name\": " << quote(name) << ", \"status\": " << quote(status)
         << ", \"detail\": " << quote(detail) << ", \"values\": {";
    bool first = true;
    for (const auto& [key, value] : values) {
      if (!first) json << ", ";
      first = false;
      json << quote(key) << ": " << number(value);
    }
    json << "}, \"text\": {";
    first = true;
    for (const auto& [key, value] : texts) {
      if (!first) json << ", ";
      first = false;
      json << quote(key) << ": " << quote(value);
    }
    json << "}}";
  };

  // ---------------- fixture ----------------------------------------------
  // F: 3 x 24 factor whose Gram matrix is the fixture covariance
  //    C = [[4,1,0],[1,9,2],[0,2,16]] (L^2).
  // R: 2 x 3 drizzle row operator, entries in sr^-1.
  const std::vector<U64> f_offsets{0, 4, 13, 26};
  const std::vector<U64> f_indices{0, 2, 3, 4, 0, 1, 5, 6, 7, 8, 9, 10, 11,
                                   1, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
  std::vector<double> f_values(26, 1.0);
  f_values[13] = 2.0;
  const std::vector<U64> r_offsets{0, 2, 4};
  const std::vector<U64> r_indices{0, 1, 1, 2};
  const std::vector<double> r_values{0.5, 0.5, 0.25, 0.75};
  // The false-independence counter-example: same diagonal, no cross terms.
  const std::vector<U64> d_offsets{0, 1, 2, 3};
  const std::vector<U64> d_indices{0, 1, 2};
  const std::vector<double> d_values{2.0, 3.0, 4.0};

  auto index_native = make_index("fixture:native", 3, Index::Kind::native);
  auto index_latent = make_index("fixture:latent24", 24, Index::Kind::latent);
  auto index_output = make_index("fixture:output2", 2, Index::Kind::parameter);

  Result<Ref<LinearAction>> factor = make_csr(context, f_offsets, f_indices, f_values, index_latent,
                                               index_native, unit_dimensionless(), unit_signal());
  Result<Ref<LinearAction>> rows = make_csr(context, r_offsets, r_indices, r_values, index_native,
                                            index_output, unit_signal(), unit_brightness());
  Result<Ref<LinearAction>> identity_output = make_identity_action(context, index_output,
                                                                   unit_brightness());
  Result<Ref<LinearAction>> identity_native = make_identity_action(context, index_native,
                                                                   unit_signal());
  Result<Ref<LinearAction>> diagonal_factor =
      make_csr(context, d_offsets, d_indices, d_values, index_latent, index_native,
               unit_dimensionless(), unit_signal());

  if (!ok(factor) || !ok(rows) || !ok(identity_output) || !ok(identity_native) ||
      !ok(diagonal_factor)) {
    std::ostringstream detail;
    detail << "an action could not be built: factor=" << static_cast<int>(factor.code)
           << " rows=" << static_cast<int>(rows.code)
           << " identity_output=" << static_cast<int>(identity_output.code)
           << " identity_native=" << static_cast<int>(identity_native.code)
           << " diagonal_factor=" << static_cast<int>(diagonal_factor.code);
    json << "    {\"name\": \"construction\", \"status\": \"error\", \"detail\": "
         << quote(detail.str()) << ", \"values\": {}, \"text\": {}}\n";
    json << "  ]\n}\n";
    if (!output_path.empty()) {
      std::ofstream out(output_path);
      out << json.str();
    }
    std::cout << json.str();
    return 1;
  }

  auto make_noise_model = [&](Ref<LinearAction> root_factor, Ref<LinearAction> influence,
                              const Unit& variance_unit) -> Ref<acsd::NoiseModel> {
    auto noise = std::make_shared<acsd::NoiseModel>();
    noise->variance_unit = variance_unit;
    noise->condition.estimator = "fixture-U01";
    noise->condition.kind = "fixed_geometry";
    noise->condition.assumptions.push_back("independent standard normal latent");
    noise->state = acsd::NoiseState::known;
    auto root = std::make_shared<acsd::NoiseRoot>();
    root->shared_id.external_namespace = "fixture";
    root->shared_id.entity = "independent-root";
    root->shared_id.model_revision = "1";
    root->shared_id.component = "all";
    root->coordinates = index_native;
    root->unit = unit_signal();
    root->factor = root_factor;
    noise->innovations.push_back(Ref<acsd::NoiseRoot>(root));
    noise->influence.push_back(influence);
    noise->identity.object_id = "fixture:noise";
    noise->identity.content_hash = content_id("noise|" + variance_unit.scale + "|" +
                                              std::to_string(variance_unit.linear_power) + "|" +
                                              std::to_string(variance_unit.sr_power));
    noise->identity.run_id = "validation-u01";
    noise->identity.schema_id = "acsd.NoiseModel";
    noise->identity.schema_major = 2;
    noise->identity.schema_minor = 0;
    noise->assessment.complete = true;
    noise->assessment.execution = acsd::ExecutionState::complete;
    noise->assessment.noise = acsd::NoiseState::known;
    return Ref<acsd::NoiseModel>(noise);
  };

  Ref<acsd::NoiseModel> noise_leaf = make_noise_model(val(factor), val(rows),
                                                       unit_variance_brightness());
  Ref<acsd::NoiseModel> noise_native = make_noise_model(val(factor), val(identity_native),
                                                        unit_variance_signal());
  Ref<acsd::NoiseModel> noise_diagonal = make_noise_model(val(diagonal_factor), val(identity_native),
                                                           unit_variance_signal());

  Result<Ref<acsd::CovarianceAction>> cov_leaf = acsd::make_covariance(context, noise_leaf);
  Result<Ref<acsd::CovarianceAction>> cov_native = acsd::make_covariance(context, noise_native);
  Result<Ref<acsd::CovarianceAction>> cov_diagonal = acsd::make_covariance(context, noise_diagonal);

  std::vector<std::pair<std::string, double>> no_values;
  std::vector<std::pair<std::string, std::string>> no_texts;
  int encoder_probe_code = -1;
  std::string encoder_probe_decoded;
  std::vector<std::string> encoder_probe_payload;
  if (!ok(cov_leaf) || !ok(cov_native) || !ok(cov_diagonal)) {
    emit("covariance_construction", "error", "make_covariance failed", no_values, no_texts);
  } else {
    // The covariance action is used on the leaf domain.  The declared
    // CovarianceAction.influence is the *additional* output action and starts
    // as the identity; it is not what defines the leaf covariance.  The leaf
    // covariance R C R^T is produced by the noise model influence, which is R.

    const std::vector<double> t{1.0, -0.5};
    const std::vector<double> e1{1.0, 0.0};
    const std::vector<double> e2{0.0, 1.0};
    const std::vector<double> x{1.0, 1.0, 1.0};

    Result<ArrayRef> t_array = copy_f64(context, t, unit_dimensionless(),
                                        index_output->identity.object_id);
    Result<ArrayRef> e1_array = copy_f64(context, e1, unit_dimensionless(),
                                         index_output->identity.object_id);
    Result<ArrayRef> e2_array = copy_f64(context, e2, unit_dimensionless(),
                                         index_output->identity.object_id);
    Result<ArrayRef> x_array = copy_f64(context, x, unit_signal(),
                                        index_native->identity.object_id);
    if (!ok(t_array) || !ok(e1_array) || !ok(e2_array) || !ok(x_array)) {
      emit("array_construction", "error", "an input array could not be stored", no_values, no_texts);
      json << "  ]\n}\n";
      if (!output_path.empty()) {
        std::ofstream out(output_path);
        out << json.str();
      }
      std::cout << json.str();
      return 1;
    }

    // ------------------------------------------------------------------
    // Encoder payload probe.
    //
    // The public array writers must emit exactly count * item bytes for the
    // declared shape.  A storage that enforces that rule is the production
    // rule (src/storage/local_storage.cpp rejects a mismatch with
    // invalid_request).  A tolerant storage is used only to expose what the
    // public encoder actually produced.
    // ------------------------------------------------------------------
    {
      ValidationStorage tolerant_storage;
      tolerant_storage.set_lenient(true);
      acsd::Context tolerant_context{resources, cancellation, tolerant_storage};
      Result<ArrayRef> tolerant_input =
          copy_f64(tolerant_context, x, unit_signal(), index_native->identity.object_id);
      Result<ArrayRef> tolerant_applied =
          acsd::apply(tolerant_context, val(rows), val(tolerant_input));
      std::vector<double> tolerant_values;
      if (ok(tolerant_applied)) tolerant_values = read_all(tolerant_context, val(tolerant_applied));
      std::ostringstream decoded;
      for (std::size_t i = 0; i < tolerant_values.size(); ++i) {
        if (i) decoded << " ";
        decoded << tolerant_values[i];
      }
      encoder_probe_code = static_cast<int>(tolerant_applied.code);
      encoder_probe_decoded = decoded.str();
      encoder_probe_payload.clear();
      for (const std::string& note : tolerant_storage.observed_payload_ratios) {
        encoder_probe_payload.push_back(note);
      }
    }

    Result<ArrayRef> applied = acsd::apply(context, val(rows), val(x_array));
    std::vector<double> applied_values = ok(applied) ? read_all(context, val(applied))
                                                     : std::vector<double>{};

    Result<ArrayRef> transposed = acsd::transpose_apply(context, val(rows), val(t_array));
    std::vector<double> u = ok(transposed) ? read_all(context, val(transposed))
                                            : std::vector<double>{};

    Result<acsd::Variance> leaf_quadratic =
        acsd::quadratic_form(context, val(cov_leaf), val(t_array));
    Result<acsd::Variance> native_quadratic = failure<acsd::Variance>(Code::internal, "not run");
    Result<acsd::Variance> diagonal_quadratic = failure<acsd::Variance>(Code::internal, "not run");
    if (ok(transposed)) {
      native_quadratic = acsd::quadratic_form(context, val(cov_native), val(transposed));
      diagonal_quadratic = acsd::quadratic_form(context, val(cov_diagonal), val(transposed));
    }

    Result<ArrayRef> column1 = acsd::covariance_apply(context, val(cov_leaf), val(e1_array));
    Result<ArrayRef> column2 = acsd::covariance_apply(context, val(cov_leaf), val(e2_array));

    std::vector<double> column1_values = ok(column1) ? read_all(context, val(column1))
                                                     : std::vector<double>{};
    std::vector<double> column2_values = ok(column2) ? read_all(context, val(column2))
                                                     : std::vector<double>{};

    auto append = [&](std::vector<double> src, std::vector<double>& dst) {
      for (double value : src) dst.push_back(value);
    };
    std::vector<double> columns;
    append(column1_values, columns);
    append(column2_values, columns);

    std::vector<std::pair<std::string, double>> values{
        {"apply_R_ones", applied_values.size() > 0 ? applied_values[0] : 0.0},
        {"apply_R_ones_second", applied_values.size() > 1 ? applied_values[1] : 0.0},
        {"transpose_R_t_0", u.size() > 0 ? u[0] : 0.0},
        {"transpose_R_t_1", u.size() > 1 ? u[1] : 0.0},
        {"transpose_R_t_2", u.size() > 2 ? u[2] : 0.0},
        {"leaf_quadratic_form", leaf_quadratic.value && leaf_quadratic.value->value
                                    ? *leaf_quadratic.value->value
                                    : 0.0},
        {"native_quadratic_form", native_quadratic.value && native_quadratic.value->value
                                      ? *native_quadratic.value->value
                                      : 0.0},
        {"false_independence_quadratic_form",
         diagonal_quadratic.value && diagonal_quadratic.value->value
             ? *diagonal_quadratic.value->value
             : 0.0},
        {"covariance_column_1_0", columns.size() > 0 ? columns[0] : 0.0},
        {"covariance_column_1_1", columns.size() > 1 ? columns[1] : 0.0},
        {"covariance_column_2_0", columns.size() > 2 ? columns[2] : 0.0},
        {"covariance_column_2_1", columns.size() > 3 ? columns[3] : 0.0},
    };
    std::vector<std::pair<std::string, std::string>> texts{
        {"leaf_quadratic_form_code", std::to_string(static_cast<int>(leaf_quadratic.code))},
        {"apply_code", std::to_string(static_cast<int>(applied.code))},
        {"transpose_code", std::to_string(static_cast<int>(transposed.code))},
        {"native_quadratic_form_code", std::to_string(static_cast<int>(native_quadratic.code))},
        {"leaf_noise_state",
         leaf_quadratic.value ? std::to_string(static_cast<int>(leaf_quadratic.value->state)) : "-1"},
        {"false_independence_code",
         std::to_string(static_cast<int>(diagonal_quadratic.code))},
        {"apply_reason", reason_of(applied)},
        {"transpose_reason", reason_of(transposed)},
        {"leaf_quadratic_reason", reason_of(leaf_quadratic)},
        {"covariance_column_1_reason", reason_of(column1)},
        {"encoder_probe_code", std::to_string(encoder_probe_code)},
        {"encoder_probe_decoded", encoder_probe_decoded},
        {"encoder_probe_payload",
         encoder_probe_payload.empty() ? std::string("none")
                                       : encoder_probe_payload.front()},
    };
    emit("u01_matrix_and_aperture", "ok",
         "public API: storage.copy / make_action / make_covariance / apply / transpose_apply / "
         "covariance_apply / quadratic_form",
         values, texts);
  }

  json << "  ]\n}\n";
  const std::string text = json.str();
  if (!output_path.empty()) {
    std::ofstream out(output_path);
    out << text;
  }
  std::cout << text;
  return 0;
}
