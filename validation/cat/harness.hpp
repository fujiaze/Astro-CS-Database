// Independent validation harness for the catalogue and the blind solver.
//
// The harness implements the public Storage and Resources interfaces of the
// declaration contract only.  It never includes an internal header of the
// production tree: the validation calls the public API exactly like an
// external driver would.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::validation {

// The result helpers are re-implemented here: the validation never includes an
// internal header of the production tree, it only uses the public fields of the
// declared Result type.
template <class T>
bool ok(const acsd::Result<T>& result) {
  return result.code == acsd::Code::ok && result.value.has_value();
}

template <class T>
const T& val(const acsd::Result<T>& result) {
  return *result.value;
}

template <class T>
acsd::Result<T> ok_result(T value) {
  acsd::Result<T> result;
  result.code = acsd::Code::ok;
  result.value = std::move(value);
  result.assessment.complete = true;
  result.assessment.execution = acsd::ExecutionState::complete;
  result.assessment.capability = acsd::Capability::supported;
  return result;
}

template <class T>
acsd::Result<T> fail(acsd::Code code, const std::string& reason, const std::string& origin,
                      const std::string& impact) {
  acsd::Result<T> result;
  result.code = code;
  result.assessment.complete = false;
  result.assessment.execution = acsd::ExecutionState::failed;
  result.assessment.capability = code == acsd::Code::unsupported ? acsd::Capability::unsupported
                                 : code == acsd::Code::unavailable ? acsd::Capability::unavailable
                                                                 : acsd::Capability::supported;
  acsd::Issue issue;
  issue.code = code;
  issue.reason = reason;
  issue.origin = origin;
  issue.impact = impact;
  result.assessment.issues.push_back(issue);
  return result;
}

inline acsd::Assessment make_assessment_ok() {
  acsd::Assessment assessment;
  assessment.complete = true;
  assessment.execution = acsd::ExecutionState::complete;
  assessment.capability = acsd::Capability::supported;
  return assessment;
}

// A cancellation source that is never triggered.
class NoCancellation final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// A minimal resource service with an explicit byte and work budget.
class SimpleResources final : public Resources {
 public:
  SimpleResources(U64 bytes, U64 work) : bytes_(bytes), work_(work) {}

  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    if (bytes > bytes_ - in_use_.load()) {
      return fail<Reservation>(Code::resource_exhausted, "the byte budget is exhausted",
                                      "validation.resources", "nothing can be reserved");
    }
    in_use_.fetch_add(bytes);
    auto owner = std::shared_ptr<const void>(nullptr, [this, bytes](const void*) {
      in_use_.fetch_sub(bytes);
    });
    Reservation reservation;
    reservation.lease.owner = owner;
    reservation.bytes = bytes;
    reservation.threads = threads;
    return ok_result(reservation);
  }

  Result<U64> consume_work(U64 units) override {
    const U64 used = used_.fetch_add(units);
    if (used + units > work_) {
      return fail<U64>(Code::resource_exhausted, "the work budget is exhausted",
                               "validation.resources", "no work can be consumed");
    }
    return ok_result(used);
  }

  Usage usage() const override {
    Usage usage;
    usage.work = used_.load();
    usage.peak_payload_bytes = in_use_.load();
    return usage;
  }

  U64 work_limit() const { return work_; }
  void set_work_limit(U64 work) { work_ = work; }

 private:
  U64 bytes_ = 0;
  U64 work_ = 0;
  std::atomic<U64> in_use_{0};
  std::atomic<U64> used_{0};
};

// An in memory immutable array store.
class MemoryStorage final : public Storage {
 public:
  struct Entry {
    Identity identity;
    DType dtype = DType::f64;
    std::vector<U64> shape;
    Unit unit;
    Id index_id;
    std::vector<std::byte> bytes;
  };

  Result<ArrayRef> copy(Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    (void)context;
    U64 count = 1;
    for (U64 extent : shape) count *= extent;
    const std::size_t item = dtype == DType::f64 ? 8 : (dtype == DType::u64 ? 8 : 1);
    if (bytes.size() != count * item) {
      return fail<ArrayRef>(Code::invalid_request, "the byte count does not match the shape",
                                    "validation.storage", "the array cannot be created");
    }
    auto entry = std::make_shared<Entry>();
    entry->dtype = dtype;
    entry->shape.assign(shape.begin(), shape.end());
    entry->unit = unit;
    entry->index_id = index;
    entry->bytes.assign(bytes.begin(), bytes.end());
    entry->identity = make_identity("acsd.Array", 2, 0, hash_bytes(dtype, entry->shape, unit,
                                                                     entry->bytes),
                                     "validation");
    {
      std::lock_guard<std::mutex> lock(mutex_);
      arrays_[entry->identity.object_id] = entry;
    }
    Array array;
    array.identity = entry->identity;
    array.assessment = make_assessment_ok();
    array.assessment.complete = true;
    array.dtype = dtype;
    array.shape = entry->shape;
    array.unit = unit;
    array.index_id = index;
    return ok_result(std::shared_ptr<const Array>(std::make_shared<Array>(array)));
  }

  Result<ArrayView> read(Context& context, ArrayRef array, BlockRequest range) const override {
    (void)context;
    if (!array) {
      return fail<ArrayView>(Code::invalid_request, "no array supplied", "validation.storage",
                                    "the range cannot be read");
    }
    std::shared_ptr<const Entry> entry;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto found = arrays_.find(array->identity.object_id);
      if (found == arrays_.end()) {
        return fail<ArrayView>(Code::missing_dependency, "the array is unknown",
                                      "validation.storage", "the range cannot be read");
      }
      entry = found->second;
    }
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    if (range.begin > range.count || range.begin + range.count > total) {
      return fail<ArrayView>(Code::invalid_request, "the range is out of bounds",
                                    "validation.storage", "the range cannot be read");
    }
    const std::size_t item = array->dtype == DType::f64 ? 8 : (array->dtype == DType::u64 ? 8 : 1);
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(entry->bytes.data() + offset, length);
    view.lease.owner = std::shared_ptr<const void>(entry, entry.get());
    return ok_result(view);
  }

  Result<ArrayRef> open(Context& context, std::string root, const Array& description,
                        std::span<const BlockDescriptor> blocks) override {
    (void)context;
    (void)root;
    (void)description;
    (void)blocks;
    return fail<ArrayRef>(Code::unsupported, "the validation store has no disk backend",
                                  "validation.storage", "the array cannot be opened");
  }

 private:
  static std::string hash_bytes(DType dtype, const std::vector<U64>& shape, const Unit& unit,
                                const std::vector<std::byte>& bytes) {
    std::ostringstream stream;
    stream << "validation:" << static_cast<int>(dtype) << ':' << shape.size();
    for (U64 extent : shape) stream << ':' << extent;
    stream << ':' << unit.scale << ':' << unit.multiplier;
    std::uint64_t hash = 1469598103934665603ull;
    for (std::byte value : bytes) {
      hash ^= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(value));
      hash *= 1099511628211ull;
    }
    stream << ':' << hash;
    return stream.str();
  }

  static Identity make_identity(const std::string& schema, U32 major, U32 minor,
                                const std::string& content, const std::string& run) {
    Identity identity;
    identity.object_id = schema + ":" + content;
    identity.content_hash = content;
    identity.run_id = run;
    identity.revision = 1;
    identity.schema_id = schema;
    identity.schema_major = major;
    identity.schema_minor = minor;
    return identity;
  }

  mutable std::mutex mutex_;
  std::map<Id, std::shared_ptr<const Entry>> arrays_;
};

// A very small JSON writer, enough for the validation evidence files.
class Json {
 public:
  Json& key(const std::string& name) {
    separate();
    stream_ << '"' << name << "\":";
    pending_value_ = true;
    return *this;
  }
  Json& value(double number) {
    separate();
    stream_ << std::setprecision(17) << number;
    return *this;
  }
  Json& value(long long number) {
    separate();
    stream_ << number;
    return *this;
  }
  Json& value(unsigned long long number) {
    separate();
    stream_ << number;
    return *this;
  }
  Json& value(unsigned long number) {
    separate();
    stream_ << number;
    return *this;
  }
  Json& value(bool flag) {
    separate();
    stream_ << (flag ? "true" : "false");
    return *this;
  }
  Json& value(const std::string& text) {
    separate();
    stream_ << '"' << text << '"';
    return *this;
  }
  Json& value(const char* text) { return value(std::string(text)); }
  Json& begin_object() {
    separate();
    stream_ << '{';
    stack_.push_back(false);
    return *this;
  }
  Json& end_object() {
    stream_ << '}';
    stack_.pop_back();
    return *this;
  }
  Json& begin_array() {
    separate();
    stream_ << '[';
    stack_.push_back(false);
    return *this;
  }
  Json& end_array() {
    stream_ << ']';
    stack_.pop_back();
    return *this;
  }
  std::string str() const { return stream_.str(); }

 private:
  void separate() {
    if (pending_value_) {
      pending_value_ = false;
      return;
    }
    if (!stack_.empty()) {
      if (stack_.back()) stream_ << ',';
      stack_.back() = true;
    }
  }
  std::ostringstream stream_;
  std::vector<bool> stack_;
  bool pending_value_ = false;
};

}  // namespace acsd::validation
