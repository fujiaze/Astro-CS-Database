// The shared support of the independent native drivers.
//
// The drivers include only the public declaration contract; this header
// implements the three public service interfaces (Storage, Resources and
// Cancellation) that the caller of the public API has to provide.  It
// contains no science: no projection, no integration, no solve.
//
// The rules of the independence red line are honoured here:
//   * only <acsd/contracts.hpp> is included;
//   * the dyadic dtype has a variable length encoding, so it has no fixed
//     element size (the same rule as the production storage).

#ifndef ACSD_VALIDATION_SUPPORT_HPP
#define ACSD_VALIDATION_SUPPORT_HPP

#include <acsd/contracts.hpp>

#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace valsupport {

namespace acsd_ns = acsd;

inline std::size_t item_size(acsd::DType dtype) {
  switch (dtype) {
    case acsd::DType::f64: return 8;
    case acsd::DType::f32: return 4;
    case acsd::DType::u64: return 8;
    case acsd::DType::u32: return 4;
    case acsd::DType::u16: return 2;
    case acsd::DType::u8: return 1;
    default: return 0;  // dyadic: a variable length encoding
  }
}

inline std::string first_issue(const acsd::Assessment& assessment) {
  if (assessment.issues.empty()) return "";
  const acsd::Issue& issue = assessment.issues.front();
  if (!issue.reason.empty()) return issue.reason;
  return issue.origin;
}

inline std::string hash_hex(const std::string& payload) {
  std::size_t hash = 1469598103934665603ull;
  for (unsigned char character : payload) {
    hash ^= character;
    hash *= 1099511628211ull;
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

// A memory backed storage: it copies the payload of the caller and serves it
// back by content hash.  Nothing is interpreted.
class ValidationStorage final : public acsd::Storage {
 public:
  struct Entry {
    std::shared_ptr<std::vector<std::byte>> bytes;
    std::vector<acsd::U64> shape;
    acsd::Unit unit;
  };

  acsd::Result<acsd::ArrayRef> copy(acsd::Context& context, acsd::DType dtype,
                                     std::span<const std::byte> bytes,
                                     std::span<const acsd::U64> shape, const acsd::Unit& unit,
                                     acsd::Id index) override {
    (void)context;
    acsd::U64 count = 1;
    for (acsd::U64 extent : shape) count *= extent;
    if (dtype != acsd::DType::dyadic &&
        bytes.size() != static_cast<std::size_t>(count) * item_size(dtype)) {
      std::cerr << "[validation storage] rejected " << index << ": bytes=" << bytes.size()
                << " shape=";
      for (acsd::U64 extent : shape) std::cerr << extent << ",";
      std::cerr << " expected=" << (static_cast<std::size_t>(count) * item_size(dtype)) << "\n";
      acsd::Result<acsd::ArrayRef> out;
      out.code = acsd::Code::invalid_request;
      return out;
    }
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<acsd::Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    acsd::Identity identity;
    identity.object_id = index;
    identity.content_hash = "sha256:" + hash_hex(std::string(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    identity.run_id = "validation";
    identity.revision = 0;
    identity.schema_id = "acsd.Array";
    identity.schema_major = 2;
    identity.schema_minor = 0;
    array->identity = identity;
    Entry entry;
    entry.bytes = buffer;
    entry.shape = array->shape;
    entry.unit = unit;
    entries_[identity.content_hash] = entry;
    acsd::Result<acsd::ArrayRef> out;
    out.code = acsd::Code::ok;
    out.value = acsd::ArrayRef(array);
    return out;
  }

  acsd::Result<acsd::ArrayView> read(acsd::Context& context, acsd::ArrayRef array,
                                      acsd::BlockRequest range) const override {
    (void)context;
    if (!array) {
      acsd::Result<acsd::ArrayView> out;
      out.code = acsd::Code::invalid_request;
      return out;
    }
    auto it = entries_.find(array->identity.content_hash);
    if (it == entries_.end()) {
      acsd::Result<acsd::ArrayView> out;
      out.code = acsd::Code::invalid_request;
      return out;
    }
    acsd::Result<acsd::ArrayView> out;
    out.code = acsd::Code::ok;
    if (array->dtype == acsd::DType::dyadic) {
      acsd::ArrayView view;
      view.dtype = array->dtype;
      view.shape = array->shape;
      view.bytes = std::span<const std::byte>(it->second.bytes->data(),
                                              it->second.bytes->size());
      view.lease.owner = it->second.bytes;
      out.value = view;
      return out;
    }
    acsd::U64 total = 1;
    for (acsd::U64 extent : array->shape) total *= extent;
    if (range.begin + range.count > total) {
      out.code = acsd::Code::invalid_request;
      return out;
    }
    const std::size_t item = item_size(array->dtype);
    acsd::ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(
        it->second.bytes->data() + static_cast<std::size_t>(range.begin) * item,
        static_cast<std::size_t>(range.count) * item);
    view.lease.owner = it->second.bytes;
    out.value = view;
    return out;
  }

  acsd::Result<acsd::ArrayRef> open(acsd::Context& context, std::string root,
                                     const acsd::Array& description,
                                     std::span<const acsd::BlockDescriptor> blocks) override {
    (void)context; (void)root; (void)description; (void)blocks;
    acsd::Result<acsd::ArrayRef> out;
    out.code = acsd::Code::unsupported;
    return out;
  }

 private:
  std::map<std::string, Entry> entries_;
};

// A resource ledger that counts the work and refuses to overbook.
class ValidationResources final : public acsd::Resources {
 public:
  explicit ValidationResources(acsd::U64 bytes = 1ull << 30, acsd::U32 threads = 8)
      : budget_bytes_(bytes), threads_(threads) {}

  acsd::Result<acsd::Reservation> reserve(acsd::U64 bytes, acsd::U32 threads) override {
    if (bytes > budget_bytes_ || threads > threads_) {
      acsd::Result<acsd::Reservation> out;
      out.code = acsd::Code::resource_exhausted;
      return out;
    }
    acsd::Reservation reservation;
    reservation.lease.owner = std::make_shared<std::vector<std::byte>>(1);
    reservation.bytes = bytes;
    reservation.threads = threads;
    acsd::Result<acsd::Reservation> out;
    out.code = acsd::Code::ok;
    out.value = reservation;
    return out;
  }

  acsd::Result<acsd::U64> consume_work(acsd::U64 units) override {
    work_ += units;
    acsd::Result<acsd::U64> out;
    out.code = acsd::Code::ok;
    out.value = units;
    return out;
  }

  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = work_;
    return usage;
  }

  acsd::U64 work() const { return work_; }

 private:
  acsd::U64 budget_bytes_;
  acsd::U32 threads_;
  acsd::U64 work_ = 0;
};

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

inline const char* code_name(int code) {
  static const char* names[] = {
      "ok", "invalid_request", "invalid_model", "domain", "unsupported", "unavailable",
      "unsolved", "not_identifiable", "invalid_covariance", "duplicate_source",
      "identity_conflict", "hash_mismatch", "corrupt_product", "missing_dependency",
      "no_support", "support_unresolved", "geometry_solve_unresolved", "selection_unresolved",
      "nonconverged", "precision_exhausted", "resource_exhausted", "io_error", "cancelled",
      "internal"};
  if (code < 0 || code >= static_cast<int>(sizeof(names) / sizeof(names[0]))) return "unknown";
  return names[code];
}

inline acsd::Identity make_identity(const std::string& object_id,
                                    const std::string& schema_id) {
  acsd::Identity identity;
  identity.object_id = object_id;
  identity.content_hash = "sha256:" + hash_hex(object_id);
  identity.run_id = "validation";
  identity.revision = 0;
  identity.schema_id = schema_id;
  identity.schema_major = 2;
  identity.schema_minor = 0;
  return identity;
}

inline acsd::Assessment make_assessment(const std::string& estimator = "fixture") {
  acsd::Assessment assessment;
  assessment.complete = true;
  assessment.validity = acsd::Validity::certified;
  assessment.science = acsd::ScienceState::bounded;
  assessment.noise = acsd::NoiseState::known;
  assessment.execution = acsd::ExecutionState::complete;
  assessment.capability = acsd::Capability::supported;
  assessment.condition.kind = "fixed_geometry";
  assessment.condition.estimator = estimator;
  return assessment;
}

inline acsd::Unit unit_luminosity() {
  acsd::Unit unit;
  unit.scale = "fixture:L";
  unit.linear_power = 1;
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  return unit;
}

inline acsd::Unit unit_variance_luminosity() {
  acsd::Unit unit;
  unit.scale = "fixture:L";
  unit.linear_power = 2;
  unit.sr_power = -2;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  return unit;
}

inline acsd::Unit unit_variance(const std::string& scale) {
  acsd::Unit unit;
  unit.scale = scale;
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 2;
  return unit;
}

inline acsd::Unit unit_dimensionless() {
  acsd::Unit unit;
  unit.scale = "1";
  unit.linear_power = 0;
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  return unit;
}

inline acsd::NumericBudget numeric_budget(double absolute = 1e-12) {
  acsd::NumericBudget budget;
  budget.tolerance.absolute = absolute;
  budget.tolerance.relative = 0.0;
  budget.tolerance.absolute_floor = 0.0;
  budget.tolerance.unit = unit_dimensionless();
  budget.max_iterations = 128;
  budget.arithmetic.mode = acsd::Arithmetic::Mode::outward;
  budget.arithmetic.initial_bits = 128;
  budget.arithmetic.max_bits = 256;
  budget.arithmetic.allow_target_fallback = false;
  return budget;
}

// An index of a given size and kind, built by the driver.
inline acsd::Ref<acsd::Index> make_index(const acsd::Id& identifier, acsd::U64 size,
                                          acsd::Index::Kind kind) {
  auto index = std::make_shared<acsd::Index>();
  index->size = size;
  index->kind = kind;
  index->namespace_id = identifier;
  index->identity.object_id = identifier;
  index->identity.content_hash = "sha256:" + hash_hex(identifier);
  index->identity.run_id = "validation";
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  return acsd::Ref<acsd::Index>(index);
}

// A copy of a double vector into the storage, as an array of the given index.
inline acsd::Result<acsd::ArrayRef> store_doubles(acsd::Context& context,
                                                   const std::vector<double>& values,
                                                   std::vector<acsd::U64> shape,
                                                   const acsd::Unit& unit,
                                                   const acsd::Id& index) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  if (!values.empty()) {
    std::memcpy(bytes.data(), values.data(), bytes.size());
  }
  return context.storage.copy(context, acsd::DType::f64, bytes, shape, unit, index);
}

}  // namespace valsupport

#endif  // ACSD_VALIDATION_SUPPORT_HPP
