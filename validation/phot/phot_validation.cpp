// Independent validation of the response and photometry modules.
//
// The program includes only the public declaration contract
// (include/acsd/contracts.hpp).  It implements its own storage, resource and
// cancellation providers, so that it does not depend on any internal header of
// the implementation and computes its own analytic truth.
//
// Cases
//   R1  analytic polynomial spectrum: the basis integrals against the closed
//       form of a top hat response (photon and energy convention)
//   R2  sampled response with breakpoints: the segmented quadrature against the
//       exact piecewise linear integral, and a top hat edge on a knot
//   P1  shared latent across two frames: the scale recovery and the non zero
//       cross frame covariance of the frame scales
//   P2  a colour/position collinear training set: the rank deficiency must be
//       reported with a null space and no scale may be published
//   P3  unknown measurement variance: the scale survives, the missing term is
//       recorded and the solution stays conditional
//   F01 counts = 100 with the enclosures [100,100] and [99,101]: the scale
//       interval must differ by the numerical enclosure only
//   S1  apply_scale applies the spatial factor per native pixel and refuses a
//       repeated application
//
// The exit code is zero only when every case passes.
#include "acsd/contracts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace acsd;

// The public Result type has no boolean conversion; these helpers give the
// validation the same discipline as the implementation modules.
template <class T>
bool good(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& get(const Result<T>& result) {
  return *result.value;
}

constexpr double kPlanck = 6.62607015e-34;
constexpr double kLightSpeed = 299792458.0;
constexpr double kPhotonFactor = 1e-9 / (kPlanck * kLightSpeed);

// ---------------------------------------------------------------------------
// Minimal infrastructure
// ---------------------------------------------------------------------------

std::string fake_hash(const std::string& tag) {
  std::string hex;
  std::uint64_t state = 1469598103934665603ull;
  for (char c : tag) {
    state ^= static_cast<unsigned char>(c);
    state *= 1099511628211ull;
  }
  for (int i = 0; i < 4; ++i) {
    std::ostringstream stream;
    stream << std::hex << ((state >> (16 * i)) & 0xffffffffull);
    hex += stream.str();
  }
  while (hex.size() < 64) {
    hex += "0";
  }
  return "sha256:" + hex;
}

class NeverCancelled final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

class MemResources final : public Resources {
 public:
  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    Reservation reservation;
    reservation.bytes = bytes;
    reservation.threads = threads;
    reservation.lease.owner = std::shared_ptr<const void>(&owner_, [](const void*) {});
    Result<Reservation> out;
    out.code = Code::ok;
    out.value = reservation;
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }
  Result<U64> consume_work(U64 units) override {
    work_ += units;
    Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }
  Usage usage() const override {
    Usage usage;
    usage.work = work_;
    return usage;
  }

 private:
  int owner_ = 0;
  U64 work_ = 0;
};

class MemStorage final : public Storage {
 public:
  Result<ArrayRef> copy(Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    array->identity.revision = 1;
    array->identity.object_id = "array:" + std::to_string(counter_++);
    array->identity.content_hash = fake_hash(array->identity.object_id);
    array->identity.run_id = "validation";
    {
      std::lock_guard<std::mutex> lock(mutex_);
      buffers_[array->identity.object_id] = buffer;
    }
    Result<ArrayRef> out;
    out.code = Code::ok;
    out.value = std::shared_ptr<const Array>(array);
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }

  Result<ArrayView> read(Context& context, ArrayRef array, BlockRequest range) const override {
    if (!array) {
      Result<ArrayView> out;
      out.code = Code::invalid_request;
      return out;
    }
    std::shared_ptr<std::vector<std::byte>> buffer;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto it = buffers_.find(array->identity.object_id);
      if (it != buffers_.end()) {
        buffer = it->second;
      }
    }
    if (!buffer) {
      Result<ArrayView> out;
      out.code = Code::missing_dependency;
      return out;
    }
    const std::size_t item = dtype_size(array->dtype);
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(buffer->data() + offset, length);
    // The lease keeps the buffer alive for the lifetime of the view.
    auto alias = static_cast<const void*>(buffer.get());
    view.lease.owner = std::shared_ptr<const void>(buffer, alias);
    Result<ArrayView> out;
    out.code = Code::ok;
    out.value = view;
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }

  Result<ArrayRef> open(Context& context, std::string root, const Array& description,
                        std::span<const BlockDescriptor> blocks) override {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  static std::size_t dtype_size(DType dtype) {
    switch (dtype) {
      case DType::f64:
        return 8;
      case DType::f32:
        return 4;
      case DType::u64:
        return 8;
      case DType::u32:
        return 4;
      case DType::u16:
        return 2;
      case DType::u8:
        return 1;
      default:
        return 0;
    }
  }
  mutable std::mutex mutex_;
  std::map<std::string, std::shared_ptr<std::vector<std::byte>>> buffers_;
  U64 counter_ = 1;
};

struct Fixture {
  MemStorage storage;
  MemResources resources;
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

// ---------------------------------------------------------------------------
// Small builders
// ---------------------------------------------------------------------------

std::vector<std::byte> encode_f64(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  for (std::size_t i = 0; i < values.size(); ++i) {
    double value = values[i];
    std::memcpy(bytes.data() + i * sizeof(double), &value, sizeof(double));
  }
  return bytes;
}

std::vector<std::byte> encode_u64(const std::vector<U64>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(U64));
  for (std::size_t i = 0; i < values.size(); ++i) {
    U64 value = values[i];
    std::memcpy(bytes.data() + i * sizeof(U64), &value, sizeof(U64));
  }
  return bytes;
}

Result<ArrayRef> make_f64(Fixture& fixture, const std::vector<double>& values,
                          const std::vector<U64>& shape, const Unit& unit, const Id& index) {
  const std::vector<std::byte> bytes = encode_f64(values);
  return fixture.context.storage.copy(fixture.context, DType::f64, bytes, shape, unit, index);
}

Result<ArrayRef> make_u64(Fixture& fixture, const std::vector<U64>& values,
                          const std::vector<U64>& shape, const Unit& unit, const Id& index) {
  const std::vector<std::byte> bytes = encode_u64(values);
  return fixture.context.storage.copy(fixture.context, DType::u64, bytes, shape, unit, index);
}

Index make_index(U64 size, Index::Kind kind, const std::string& tag) {
  Index index;
  index.size = size;
  index.kind = kind;
  index.namespace_id = "validation:" + tag;
  index.identity.schema_id = "acsd.Index";
  index.identity.schema_major = 2;
  index.identity.schema_minor = 0;
  index.identity.revision = 1;
  index.identity.object_id = "index:" + tag;
  index.identity.content_hash = fake_hash(tag);
  index.identity.run_id = "validation";
  return index;
}

Result<Ref<LinearAction>> make_identity_action(Fixture& fixture, const Index& index) {
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::identity;
  recipe.algorithm = "canonical_identity_v1";
  recipe.version = 1;
  recipe.spaces.domain = std::make_shared<const Index>(index);
  recipe.spaces.codomain = std::make_shared<const Index>(index);
  recipe.spaces.input = Unit{"1", 0, 0, 0, 1.0, 0};
  recipe.spaces.output = Unit{"1", 0, 0, 0, 1.0, 0};
  const std::vector<ArrayRef> arrays;
  const std::vector<Ref<LinearAction>> children;
  return make_action(fixture.context, recipe, children, arrays);
}

Result<Ref<LinearAction>> make_diagonal_action(Fixture& fixture, const std::vector<double>& factors,
                                               const Index& index) {
  Result<ArrayRef> values =
      make_f64(fixture, factors, {static_cast<U64>(factors.size())}, Unit{"1", 0, 0, 0, 1.0, 0},
               index.identity.object_id);
  if (!good(values)) {
    Result<Ref<LinearAction>> out;
    out.code = values.code;
    return out;
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "canonical_diagonal_v1";
  recipe.version = 1;
  recipe.arrays = {"diagonal"};
  recipe.spaces.domain = std::make_shared<const Index>(index);
  recipe.spaces.codomain = std::make_shared<const Index>(index);
  recipe.spaces.input = Unit{"1", 0, 0, 0, 1.0, 0};
  recipe.spaces.output = Unit{"1", 0, 0, 0, 1.0, 0};
  const std::vector<ArrayRef> arrays{get(values)};
  const std::vector<Ref<LinearAction>> children;
  return make_action(fixture.context, recipe, children, arrays);
}

// Reads an f64 array back from storage.
Result<std::vector<double>> read_f64(Fixture& fixture, ArrayRef array) {
  if (!array) {
    Result<std::vector<double>> out;
    out.code = Code::invalid_request;
    return out;
  }
  U64 total = 1;
  for (U64 extent : array->shape) {
    total *= extent;
  }
  Result<ArrayView> view = fixture.context.storage.read(fixture.context, array, BlockRequest{0, total});
  if (!good(view)) {
    Result<std::vector<double>> out;
    out.code = view.code;
    return out;
  }
  std::vector<double> values(total);
  for (U64 i = 0; i < total; ++i) {
    double value = 0.0;
    std::memcpy(&value, get(view).bytes.data() + i * sizeof(double), sizeof(double));
    values[i] = value;
  }
  Result<std::vector<double>> out;
  out.code = Code::ok;
  out.value = values;
  return out;
}

// A covariance action for a diagonal set of variances.
Result<Ref<CovarianceAction>> make_diagonal_covariance(Fixture& fixture,
                                                        const std::vector<double>& variances,
                                                        const Index& index) {
  std::vector<double> roots;
  roots.reserve(variances.size());
  for (double variance : variances) {
    roots.push_back(std::sqrt(variance));
  }
  Result<Ref<LinearAction>> factor = make_diagonal_action(fixture, roots, index);
  if (!good(factor)) {
    Result<Ref<CovarianceAction>> out;
    out.code = factor.code;
    return out;
  }
  Result<Ref<LinearAction>> influence = make_identity_action(fixture, index);
  if (!good(influence)) {
    Result<Ref<CovarianceAction>> out;
    out.code = influence.code;
    return out;
  }
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = SharedId{"validation", index.identity.object_id, "1", "diagonal"};
  root->coordinates = std::make_shared<const Index>(index);
  root->unit = Unit{"validation", 0, 0, 0, 1.0, 2};
  root->factor = get(factor);
  auto model = std::make_shared<NoiseModel>();
  model->variance_unit = Unit{"validation", 0, 0, 0, 1.0, 2};
  model->condition = Condition{"validation", "diagonal"};
  model->shared_factors.push_back(root);
  model->influence.push_back(get(influence));
  model->state = NoiseState::known;
  Result<Ref<CovarianceAction>> covariance = make_covariance(fixture.context, model);
  if (!good(covariance)) {
    Result<Ref<CovarianceAction>> out;
    out.code = covariance.code;
    return out;
  }
  return covariance;
}

// ---------------------------------------------------------------------------
// Report helpers
// ---------------------------------------------------------------------------

struct Report {
  std::vector<std::pair<std::string, bool>> cases;
  std::ostringstream detail;

  void record(const std::string& name, bool passed, const std::string& information) {
    cases.emplace_back(name, passed);
    detail << "case " << name << ": " << (passed ? "PASS" : "FAIL") << " | " << information << "\n";
  }
};

double relative_difference(double a, double b) {
  const double scale = std::max(std::fabs(a), std::fabs(b));
  if (scale == 0.0) {
    return 0.0;
  }
  return std::fabs(a - b) / scale;
}

// ---------------------------------------------------------------------------
// Object builders for the photometry cases
// ---------------------------------------------------------------------------

// A tiny helper that mirrors the public ok_result contract without using an
// internal header.
template <class T>
Result<T> model_ok(T value) {
  Result<T> out;
  out.code = Code::ok;
  out.value = std::move(value);
  out.assessment.complete = true;
  out.assessment.execution = ExecutionState::complete;
  return out;
}

struct SpectralSetup {
  Ref<SpectralBasis> basis;
  Ref<Response> response;
  std::vector<double> integrals;  // the analytic truth of the single basis function
};

Result<SpectralSetup> build_top_hat_setup(Fixture& fixture, double center, double width,
                                           double knot_low, double knot_high, bool photon) {
  SpectralSetup setup;
  auto basis = std::make_shared<SpectralBasis>();
  basis->identity.schema_id = "acsd.SpectralBasis";
  basis->identity.schema_major = 2;
  basis->identity.schema_minor = 0;
  basis->identity.revision = 1;
  basis->identity.object_id = "basis:top_hat";
  basis->identity.content_hash = fake_hash("basis:top_hat");
  basis->identity.run_id = "validation";
  basis->degree = 0;
  basis->coefficient_count = 1;
  basis->support_nm = Interval{knot_low, knot_high};
  Result<ArrayRef> knots = make_f64(fixture, {knot_low, knot_high}, {2}, Unit{"nm", 0, 0, 1, 1.0, 0},
                                    "basis:top_hat");
  if (!good(knots)) {
    Result<SpectralSetup> out;
    out.code = knots.code;
    return out;
  }
  Result<ArrayRef> coefficients =
      make_f64(fixture, {1.0}, {1, 1, 1}, Unit{"1", 0, 0, 0, 1.0, 0}, "basis:top_hat");
  if (!good(coefficients)) {
    Result<SpectralSetup> out;
    out.code = coefficients.code;
    return out;
  }
  basis->knots_nm = get(knots);
  basis->basis_coefficients = get(coefficients);

  auto response = std::make_shared<Response>();
  response->identity.schema_id = "acsd.Response";
  response->identity.schema_major = 2;
  response->identity.schema_minor = 0;
  response->identity.revision = 1;
  response->identity.object_id = "response:top_hat";
  response->identity.content_hash = fake_hash("response:top_hat");
  response->identity.run_id = "validation";
  response->convention = photon ? Response::Convention::photon_throughput
                                : Response::Convention::includes_photon_conversion;
  response->top_hat_center_width_nm = Vec2{center, width};
  response->support_nm = Interval{center - 0.5 * width, center + 0.5 * width};
  response->provenance = "synthetic top hat";
  response->normalization = "unit peak";
  setup.basis = basis;
  setup.response = response;

  const double low = center - 0.5 * width;
  const double high = center + 0.5 * width;
  if (photon) {
    setup.integrals.push_back(kPhotonFactor * (high * high - low * low) * 0.5);
  } else {
    setup.integrals.push_back(width);
  }
  return model_ok(std::move(setup));
}

// A bit level self check of the local little endian encoders: the encoding
// must be the exact inverse of the decoding and must use the exact element
// size, otherwise the storage contract is broken.
bool encoder_self_check() {
  const std::vector<double> values{0.0, 1.0, -1.0, 0.5, 1e-300, 1e300, 3.14159265358979};
  const std::vector<std::byte> bytes = encode_f64(values);
  if (bytes.size() != values.size() * sizeof(double)) {
    std::cout << "encoder self check: the f64 buffer size is " << bytes.size() << "\n";
    return false;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    double decoded = 0.0;
    std::memcpy(&decoded, bytes.data() + i * sizeof(double), sizeof(double));
    std::uint64_t original_bits = 0;
    std::uint64_t decoded_bits = 0;
    const double original = values[i];
    std::memcpy(&original_bits, &original, sizeof(original_bits));
    std::memcpy(&decoded_bits, &decoded, sizeof(decoded_bits));
    if (original_bits != decoded_bits) {
      std::cout << "encoder self check: element " << i << " does not round trip\n";
      return false;
    }
  }
  const std::vector<U64> indices{0, 1, 4096, 1ull << 40};
  const std::vector<std::byte> index_bytes = encode_u64(indices);
  if (index_bytes.size() != indices.size() * sizeof(U64)) {
    std::cout << "encoder self check: the u64 buffer size is " << index_bytes.size() << "\n";
    return false;
  }
  return true;
}

#include "phot_cases.inc"

}  // namespace

int main() {
  Report report;
  if (!encoder_self_check()) {
    report.record("encoder_self_check", false, "the local little endian encoding is broken");
  } else {
    report.record("encoder_self_check", true, "the local little endian encoding round trips");
  }
  Fixture fixture;
  run_all_cases(fixture, report);
  std::cout << report.detail.str();
  bool passed = true;
  for (const auto& entry : report.cases) {
    passed = passed && entry.second;
  }
  std::cout << (passed ? "ALL CASES PASSED" : "SOME CASES FAILED") << "\n";
  return passed ? 0 : 1;
}
