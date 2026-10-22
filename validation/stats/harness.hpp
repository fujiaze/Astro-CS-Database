#pragma once
// Independent validation harness for the statistics modules.
//
// The harness only uses the public contract of include/acsd/contracts.hpp; it
// never includes a header of src/ and never reuses a helper of the solver
// under test.  The storage, the resources and the frame provider are written
// here so that the validation is an independent caller of the public API.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd_validation {

using acsd::ArrayRef;
using acsd::ArrayView;
using acsd::BlockRequest;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::Id;
using acsd::Index;
using acsd::Unit;

// ---------------------------------------------------------------------------
// Result helpers (the contract owns the type, the harness only fills it)
// ---------------------------------------------------------------------------

template <class T>
acsd::Result<T> make_ok(T value) {
  acsd::Result<T> out;
  out.code = Code::ok;
  out.value = std::move(value);
  out.assessment.complete = true;
  out.assessment.execution = acsd::ExecutionState::complete;
  out.assessment.capability = acsd::Capability::supported;
  return out;
}

template <class T>
acsd::Result<T> make_fail(acsd::Code code, const std::string& reason) {
  acsd::Result<T> out;
  out.code = code;
  out.assessment.execution = acsd::ExecutionState::failed;
  acsd::Issue issue;
  issue.code = code;
  issue.reason = reason;
  out.assessment.issues.push_back(issue);
  return out;
}

template <class T>
bool is_ok(const acsd::Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& value_of(const acsd::Result<T>& result) {
  return *result.value;
}

// ---------------------------------------------------------------------------
// In memory storage
// ---------------------------------------------------------------------------

class MemoryStorage final : public acsd::Storage {
 public:
  acsd::Result<ArrayRef> copy(acsd::Context& context, acsd::DType dtype,
                              std::span<const std::byte> bytes, std::span<const acsd::U64> shape,
                              const acsd::Unit& unit, acsd::Id index) override {
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<acsd::Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->identity.object_id = index;
    array->identity.content_hash = index;
    array->identity.schema_id = "acsd.Array";
    array->assessment.complete = true;
    entries_[array.get()] = buffer;
    return make_ok(ArrayRef(array));
  }

  acsd::Result<ArrayView> read(acsd::Context& context, ArrayRef array,
                                acsd::BlockRequest range) const override {
    if (!array) return make_fail<ArrayView>(Code::invalid_request, "no array");
    auto found = entries_.find(array.get());
    if (found == entries_.end()) {
      return make_fail<ArrayView>(Code::invalid_request, "unknown array");
    }
    const std::size_t element = dtype_size(array->dtype);
    if (element == 0) {
      return make_fail<ArrayView>(Code::unsupported, "the dtype has no byte view");
    }
    const std::size_t begin = static_cast<std::size_t>(range.begin) * element;
    const std::size_t end = static_cast<std::size_t>(range.begin + range.count) * element;
    if (end > found->second->size()) {
      return make_fail<ArrayView>(Code::invalid_request, "the range is out of bounds");
    }
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(found->second->data() + begin, end - begin);
    view.lease.owner = found->second;
    return make_ok(view);
  }

  acsd::Result<ArrayRef> open(acsd::Context& context, std::string root, const acsd::Array& description,
                              std::span<const acsd::BlockDescriptor> blocks) override {
    (void)context;
    (void)root;
    (void)description;
    (void)blocks;
    return make_fail<ArrayRef>(Code::unsupported, "the harness does not open products");
  }

 private:
  static std::size_t dtype_size(acsd::DType dtype) {
    switch (dtype) {
      case acsd::DType::f64:
      case acsd::DType::u64:
        return 8;
      case acsd::DType::f32:
      case acsd::DType::u32:
        return 4;
      case acsd::DType::u16:
        return 2;
      case acsd::DType::u8:
        return 1;
      case acsd::DType::dyadic:
        return 0;
    }
    return 0;
  }

  std::map<const acsd::Array*, std::shared_ptr<std::vector<std::byte>>> entries_;
};

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

class CountingResources final : public acsd::Resources {
 public:
  explicit CountingResources(acsd::U64 bytes = 1ull << 30, acsd::U64 work = 1ull << 24)
      : bytes_(bytes), work_(work) {}

  acsd::Result<acsd::Reservation> reserve(acsd::U64 bytes, acsd::U32 threads) override {
    if (bytes > bytes_) return make_fail<acsd::Reservation>(Code::resource_exhausted, "no bytes");
    auto owner = std::make_shared<std::vector<std::byte>>(1);
    acsd::Reservation reservation;
    reservation.lease.owner = owner;
    reservation.bytes = bytes;
    reservation.threads = threads;
    return make_ok(reservation);
  }

  acsd::Result<acsd::U64> consume_work(acsd::U64 units) override {
    used_work_ += units;
    if (used_work_ > work_) {
      return make_fail<acsd::U64>(Code::resource_exhausted, "the work budget is exhausted");
    }
    return make_ok(used_work_);
  }

  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = used_work_;
    return usage;
  }

 private:
  acsd::U64 bytes_;
  acsd::U64 work_;
  acsd::U64 used_work_ = 0;
};

class NoCancellation final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// ---------------------------------------------------------------------------
// Array helpers
// ---------------------------------------------------------------------------

inline std::vector<acsd::U64> decode_u64(std::span<const std::byte> bytes) {
  std::vector<acsd::U64> out(bytes.size() / sizeof(acsd::U64));
  if (!out.empty()) {
    std::memcpy(out.data(), bytes.data(), out.size() * sizeof(acsd::U64));
  }
  return out;
}

inline std::vector<double> decode_f64(std::span<const std::byte> bytes) {
  std::vector<double> values(bytes.size() / sizeof(double));
  if (!values.empty()) {
    std::memcpy(values.data(), bytes.data(), values.size() * sizeof(double));
  }
  return values;
}

inline std::vector<std::byte> encode_f64(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  if (!values.empty()) {
    std::memcpy(bytes.data(), values.data(), bytes.size());
  }
  return bytes;
}

inline std::vector<std::byte> encode_u64(const std::vector<acsd::U64>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(acsd::U64));
  if (!values.empty()) {
    std::memcpy(bytes.data(), values.data(), bytes.size());
  }
  return bytes;
}

inline std::vector<std::byte> encode_u8(const std::vector<std::uint8_t>& values) {
  std::vector<std::byte> bytes(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    bytes[i] = static_cast<std::byte>(values[i]);
  }
  return bytes;
}

inline std::vector<std::byte> encode_u16(const std::vector<std::uint16_t>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(std::uint16_t));
  if (!values.empty()) {
    std::memcpy(bytes.data(), values.data(), bytes.size());
  }
  return bytes;
}

inline acsd::Result<ArrayRef> store_f64(acsd::Context& context, const std::vector<double>& values,
                                         const std::vector<acsd::U64>& shape, const acsd::Id& index,
                                         const acsd::Unit& unit) {
  const std::vector<std::byte> bytes = encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index);
}

inline acsd::Result<ArrayRef> store_u64(acsd::Context& context, const std::vector<acsd::U64>& values,
                                         const std::vector<acsd::U64>& shape, const acsd::Id& index) {
  const std::vector<std::byte> bytes = encode_u64(values);
  return context.storage.copy(context, DType::u64, bytes, shape, acsd::Unit{}, index);
}

// ---------------------------------------------------------------------------
// Frame provider used by the tests
// ---------------------------------------------------------------------------

struct TestFrame {
  acsd::Id frame;
  acsd::U32 order = 3;
  std::vector<acsd::HealpixLeaf> leaves;
  std::vector<double> signal;
  std::vector<std::uint16_t> dq;
  std::vector<std::uint8_t> state;
  acsd::Ref<acsd::CovarianceAction> noise;
  acsd::Ref<acsd::LinearAction> native_to_leaf;
};

class TestFramesProvider final : public acsd::FramesProvider {
 public:
  explicit TestFramesProvider(std::vector<TestFrame> frames) : frames_(std::move(frames)) {}

  acsd::Result<std::vector<acsd::Id>> list_frames(acsd::Context& context) const override {
    (void)context;
    std::vector<acsd::Id> ids;
    for (const TestFrame& frame : frames_) ids.push_back(frame.frame);
    std::sort(ids.begin(), ids.end());
    return make_ok(ids);
  }

  acsd::Result<acsd::Ref<acsd::FrameDescription>> describe_frame(acsd::Context& context,
                                                                  acsd::Id frame) const override {
    (void)context;
    for (const TestFrame& candidate : frames_) {
      if (candidate.frame != frame) continue;
      auto description = std::make_shared<acsd::FrameDescription>();
      auto index = std::make_shared<acsd::Index>();
      index->size = candidate.leaves.size();
      index->kind = Index::Kind::leaf;
      index->order = candidate.order;
      index->namespace_id = "test:" + frame;
      std::vector<acsd::U64> ids;
      for (const acsd::HealpixLeaf& leaf : candidate.leaves) {
        ids.push_back(leaf.order);
        ids.push_back(leaf.ipix);
      }
      const std::vector<acsd::U64> id_shape{static_cast<acsd::U64>(candidate.leaves.size()), 2};
      acsd::Result<ArrayRef> id_array =
          store_u64(context, ids, id_shape, "test_index:" + frame);
      if (!is_ok(id_array)) return make_fail<acsd::Ref<acsd::FrameDescription>>(
          Code::internal, "the leaf identities cannot be stored");
      index->ids = value_of(id_array);
      description->index = std::shared_ptr<const acsd::Index>(index);
      description->frame = frame;
      description->order = candidate.order;
      description->noise = candidate.noise;
      description->native_to_leaf = candidate.native_to_leaf;
      description->capabilities = {"signal", "dq", "support"};
      return make_ok(acsd::Ref<acsd::FrameDescription>(description));
    }
    return make_fail<acsd::Ref<acsd::FrameDescription>>(Code::invalid_request, "unknown frame");
  }

  acsd::Result<acsd::LeafBlock> read_leaf_block(acsd::Context& context,
                                                 const acsd::FrameReadRequest& request) const override {
    for (const TestFrame& candidate : frames_) {
      if (candidate.frame != request.frame) continue;
      acsd::LeafBlock block;
      acsd::Result<ArrayRef> signal =
          store_f64(context, candidate.signal, {candidate.signal.size()}, "test_signal:" + request.frame,
                    acsd::Unit{});
      if (!is_ok(signal)) {
        return make_fail<acsd::LeafBlock>(Code::internal, "the signal cannot be stored");
      }
      block.signal = value_of(signal);
      const std::vector<acsd::U64> dq_shape{static_cast<acsd::U64>(candidate.dq.size())};
      acsd::Result<ArrayRef> dq =
          context.storage.copy(context, DType::u16, encode_u16(candidate.dq), dq_shape,
                               acsd::Unit{}, "test_dq:" + request.frame);
      if (!is_ok(dq)) return make_fail<acsd::LeafBlock>(Code::internal, "the quality cannot be stored");
      block.dq = value_of(dq);
      return make_ok(block);
    }
    return make_fail<acsd::LeafBlock>(Code::invalid_request, "unknown frame");
  }

  acsd::Result<std::vector<ArrayRef>> read_native_dependencies(
      acsd::Context& context, std::span<const acsd::SourceIdentity> sources) const override {
    (void)context;
    (void)sources;
    return make_ok(std::vector<ArrayRef>{});
  }

  acsd::Result<acsd::Ref<acsd::LinearAction>> open_action(acsd::Context& context, acsd::Id action) const override {
    (void)context;
    (void)action;
    return make_fail<acsd::Ref<acsd::LinearAction>>(Code::unsupported, "no action in the harness");
  }

  acsd::Result<acsd::Ref<acsd::SupportMap>> query_support(acsd::Context& context, acsd::Id frame,
                                                           acsd::Ref<acsd::Index> index) const override {
    (void)index;
    for (const TestFrame& candidate : frames_) {
      if (candidate.frame != frame) continue;
      auto support = std::make_shared<acsd::SupportMap>();
      const std::vector<acsd::U64> state_shape{static_cast<acsd::U64>(candidate.state.size())};
      acsd::Result<ArrayRef> states =
          context.storage.copy(context, DType::u8, encode_u8(candidate.state), state_shape,
                               acsd::Unit{}, "test_support:" + frame);
      if (!is_ok(states)) {
        return make_fail<acsd::Ref<acsd::SupportMap>>(Code::internal, "the support cannot be stored");
      }
      support->states = value_of(states);
      support->index = index;
      return make_ok(acsd::Ref<acsd::SupportMap>(support));
    }
    return make_fail<acsd::Ref<acsd::SupportMap>>(Code::invalid_request, "unknown frame");
  }

  acsd::Result<acsd::Ref<acsd::RefinableGeometryProvider>> open_refinable_geometry(
      acsd::Context& context, acsd::Id snapshot) const override {
    (void)context;
    (void)snapshot;
    return make_fail<acsd::Ref<acsd::RefinableGeometryProvider>>(Code::unsupported,
                                                                 "no geometry in the harness");
  }

 private:
  std::vector<TestFrame> frames_;
};

// ---------------------------------------------------------------------------
// Small linear algebra helpers of the validation (independent of src/)
// ---------------------------------------------------------------------------

inline std::vector<double> mat_vec(const std::vector<std::vector<double>>& matrix,
                                   const std::vector<double>& vector) {
  std::vector<double> out(matrix.size(), 0.0);
  for (std::size_t i = 0; i < matrix.size(); ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < vector.size(); ++j) sum += matrix[i][j] * vector[j];
    out[i] = sum;
  }
  return out;
}

// Solves a symmetric positive definite system with a plain Gaussian
// elimination; the validation never uses the solver under test.
inline std::vector<double> solve_spd(std::vector<std::vector<double>> matrix,
                                      std::vector<double> right) {
  const std::size_t n = matrix.size();
  for (std::size_t k = 0; k < n; ++k) {
    std::size_t pivot = k;
    for (std::size_t i = k + 1; i < n; ++i) {
      if (std::fabs(matrix[i][k]) > std::fabs(matrix[pivot][k])) pivot = i;
    }
    std::swap(matrix[k], matrix[pivot]);
    std::swap(right[k], right[pivot]);
    for (std::size_t i = k + 1; i < n; ++i) {
      const double factor = matrix[i][k] / matrix[k][k];
      if (factor == 0.0) continue;
      for (std::size_t j = k; j < n; ++j) matrix[i][j] -= factor * matrix[k][j];
      right[i] -= factor * right[k];
    }
  }
  std::vector<double> out(n, 0.0);
  for (std::size_t i = n; i-- > 0;) {
    double sum = right[i];
    for (std::size_t j = i + 1; j < n; ++j) sum -= matrix[i][j] * out[j];
    out[i] = sum / matrix[i][i];
  }
  return out;
}

// The exact binomial (Clopper-Pearson) interval of a proportion.
inline std::pair<double, double> clopper_pearson(std::size_t successes, std::size_t trials,
                                                  double alpha = 0.05) {
  // The normal approximation of the inverse beta function is replaced by a
  // bisection on the binomial tail, which is exact enough for the report.
  if (trials == 0) return {0.0, 1.0};
  auto binom_tail = [&](std::size_t k, double p) {
    // The probability of at least k successes.
    double total = 0.0;
    double log_p = std::log(std::max(p, 1e-300));
    double log_q = std::log(std::max(1.0 - p, 1e-300));
    for (std::size_t i = k; i <= trials; ++i) {
      const double log_probability =
          std::lgamma(static_cast<double>(trials) + 1.0) - std::lgamma(static_cast<double>(i) + 1.0) -
          std::lgamma(static_cast<double>(trials - i) + 1.0) + static_cast<double>(i) * log_p +
          static_cast<double>(trials - i) * log_q;
      total += std::exp(log_probability);
    }
    return total;
  };
  double lower = 0.0;
  double upper = 1.0;
  double low = 0.0;
  double high = 1.0;
  for (int step = 0; step < 200; ++step) {
    const double middle = 0.5 * (low + high);
    if (binom_tail(successes, middle) > alpha / 2.0) {
      low = middle;
    } else {
      high = middle;
    }
  }
  lower = low;
  low = 0.0;
  high = 1.0;
  for (int step = 0; step < 200; ++step) {
    const double middle = 0.5 * (low + high);
    if (binom_tail(successes + 1, middle) > alpha / 2.0) {
      high = middle;
    } else {
      low = middle;
    }
  }
  upper = high;
  return {lower, upper};
}

// ---------------------------------------------------------------------------
// Actions and covariances
// ---------------------------------------------------------------------------

// Builds a noise model with one root whose factor and influence are given.
acsd::Result<acsd::Ref<acsd::CovarianceAction>> make_covariance(
    acsd::Context& context, const std::vector<std::vector<double>>& factor,
    const std::vector<std::vector<double>>& influence, const std::string& index,
    const std::string& shared = std::string(), bool shared_root = false) {
  const std::size_t rows = influence.size();
  const std::size_t columns = influence.empty() ? 0 : influence[0].size();
  std::vector<acsd::U64> offsets{0};
  std::vector<acsd::U64> indices;
  std::vector<double> values;
  for (std::size_t i = 0; i < rows; ++i) {
    for (std::size_t j = 0; j < columns; ++j) {
      if (influence[i][j] == 0.0) continue;
      indices.push_back(j);
      values.push_back(influence[i][j]);
    }
    offsets.push_back(indices.size());
  }
  auto build_action = [&](const std::vector<std::vector<double>>& matrix,
                          const std::string& name) -> acsd::Result<acsd::Ref<acsd::LinearAction>> {
    const std::size_t m = matrix.size();
    const std::size_t n = m == 0 ? 0 : matrix[0].size();
    std::vector<acsd::U64> off{0};
    std::vector<acsd::U64> ind;
    std::vector<double> val;
    for (std::size_t i = 0; i < m; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        if (matrix[i][j] == 0.0) continue;
        ind.push_back(j);
        val.push_back(matrix[i][j]);
      }
      off.push_back(ind.size());
    }
    acsd::Result<acsd::ArrayRef> offsets = store_u64(context, off, {off.size()}, name + ":offsets");
    acsd::Result<acsd::ArrayRef> columns = store_u64(context, ind, {ind.size()}, name + ":indices");
    acsd::Result<acsd::ArrayRef> entries = store_f64(context, val, {val.size()}, name + ":values",
                                                       acsd::Unit{});
    if (!is_ok(offsets) || !is_ok(columns) || !is_ok(entries)) {
      return make_fail<acsd::Ref<acsd::LinearAction>>(Code::internal, "the arrays cannot be stored");
    }
    acsd::ActionRecipe recipe;
    recipe.kind = acsd::ActionRecipe::Kind::csr;
    recipe.algorithm = "validation_csr_v1";
    recipe.version = 1;
    recipe.arrays = {name + ":offsets", name + ":indices", name + ":values"};
    const std::vector<acsd::ArrayRef> arrays{value_of(offsets), value_of(columns), value_of(entries)};
    return acsd::make_action(context, recipe, std::span<const acsd::Ref<acsd::LinearAction>>{}, arrays);
  };

  acsd::Result<acsd::Ref<acsd::LinearAction>> factor_action = build_action(factor, index + ":factor");
  acsd::Result<acsd::Ref<acsd::LinearAction>> influence_result = build_action(influence, index + ":influence");
  if (!is_ok(factor_action) || !is_ok(influence_result)) {
    return make_fail<acsd::Ref<acsd::CovarianceAction>>(Code::internal, "the actions cannot be built");
  }
  auto root = std::make_shared<acsd::NoiseRoot>();
  root->shared_id = acsd::SharedId{"validation", shared.empty() ? index : shared, "1", "root"};
  root->unit = acsd::Unit{};
  root->factor = value_of(factor_action);
  auto coordinates = std::make_shared<acsd::Index>();
  coordinates->size = columns;
  coordinates->kind = acsd::Index::Kind::latent;
  root->coordinates = std::shared_ptr<const acsd::Index>(coordinates);

  auto noise = std::make_shared<acsd::NoiseModel>();
  noise->variance_unit = acsd::Unit{};
  if (shared_root) {
    noise->shared_factors.push_back(std::shared_ptr<const acsd::NoiseRoot>(root));
  } else {
    noise->innovations.push_back(std::shared_ptr<const acsd::NoiseRoot>(root));
  }
  noise->influence.push_back(value_of(influence_result));
  noise->state = acsd::NoiseState::known;
  return acsd::make_covariance(context,
                               acsd::Ref<acsd::NoiseModel>(std::shared_ptr<const acsd::NoiseModel>(noise)));
}

// The identity action of a given size.
acsd::Result<acsd::Ref<acsd::LinearAction>> identity_action(acsd::Context& context, std::size_t size,
                                                             const std::string& name) {
  std::vector<std::vector<double>> matrix(size, std::vector<double>(size, 0.0));
  for (std::size_t i = 0; i < size; ++i) matrix[i][i] = 1.0;
  std::vector<acsd::U64> offsets{0};
  std::vector<acsd::U64> indices;
  std::vector<double> values;
  for (std::size_t i = 0; i < size; ++i) {
    indices.push_back(i);
    values.push_back(1.0);
    offsets.push_back(indices.size());
  }
  acsd::Result<acsd::ArrayRef> off = store_u64(context, offsets, {offsets.size()}, name + ":offsets");
  acsd::Result<acsd::ArrayRef> ind = store_u64(context, indices, {indices.size()}, name + ":indices");
  acsd::Result<acsd::ArrayRef> val = store_f64(context, values, {values.size()}, name + ":values",
                                                acsd::Unit{});
  if (!is_ok(off) || !is_ok(ind) || !is_ok(val)) {
    return make_fail<acsd::Ref<acsd::LinearAction>>(Code::internal, "the arrays cannot be stored");
  }
  acsd::ActionRecipe recipe;
  recipe.kind = acsd::ActionRecipe::Kind::csr;
  recipe.algorithm = "validation_csr_v1";
  recipe.version = 1;
  recipe.arrays = {name + ":offsets", name + ":indices", name + ":values"};
  const std::vector<acsd::ArrayRef> arrays{value_of(off), value_of(ind), value_of(val)};
  return acsd::make_action(context, recipe, std::span<const acsd::Ref<acsd::LinearAction>>{}, arrays);
}

}  // namespace acsd_validation
