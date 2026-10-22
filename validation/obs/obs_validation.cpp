#include "obs_validation.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace acsd_validation {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
  ++g_checks;
  if (!condition) {
    ++g_failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

void check_close(double value, double expected, double tolerance, const std::string& what) {
  ++g_checks;
  const double difference = std::fabs(value - expected);
  if (!(difference <= tolerance)) {
    ++g_failures;
    std::printf("  FAIL: %s (value %.17g, expected %.17g, |diff| %.3g > %.3g)\n", what.c_str(), value,
                expected, difference, tolerance);
  }
}

// ---------------------------------------------------------------------------
// In-memory storage.
// ---------------------------------------------------------------------------

Result<ArrayRef> MemoryStorage::copy(acsd::Context& context, DType dtype,
                                      std::span<const std::byte> bytes,
                                      std::span<const U64> shape, const Unit& unit, Id index) {
  auto stored = std::make_shared<StoredBytes>();
  stored->dtype = dtype;
  stored->shape.assign(shape.begin(), shape.end());
  stored->unit = unit;
  stored->bytes = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
  auto array = std::make_shared<acsd::Array>();
  array->dtype = dtype;
  array->shape = stored->shape;
  array->unit = unit;
  array->index_id = index;
  array->identity.object_id = "array:" + index + ":" + std::to_string(counter_++);
  array->identity.content_hash = "sha256:" + std::string(64, '0');
  array->identity.schema_id = "acsd.Array";
  array->identity.schema_major = 2;
  array->identity.schema_minor = 0;
  array->assessment.execution = acsd::ExecutionState::complete;
  array->assessment.complete = true;
  entries_[array->identity.object_id] = stored;
  Result<ArrayRef> out;
  out.code = Code::ok;
  out.value = std::shared_ptr<const acsd::Array>(array);
  out.assessment = array->assessment;
  return out;
}

Result<acsd::ArrayView> MemoryStorage::read(acsd::Context& context, ArrayRef array,
                                            acsd::BlockRequest range) const {
  auto it = entries_.find(array->identity.object_id);
  Result<acsd::ArrayView> out;
  if (it == entries_.end()) {
    out.code = Code::invalid_request;
    return out;
  }
  const StoredBytes& stored = *it->second;
  U64 total = 1;
  for (U64 extent : stored.shape) total *= extent;
  const std::size_t item = stored.dtype == DType::u16 ? 2 : 8;
  if (range.begin + range.count > total) {
    out.code = Code::invalid_request;
    return out;
  }
  acsd::ArrayView view;
  view.dtype = stored.dtype;
  view.shape = stored.shape;
  view.bytes = std::span<const std::byte>(
      stored.bytes->data() + static_cast<std::size_t>(range.begin) * item,
      static_cast<std::size_t>(range.count) * item);
  view.lease.owner = stored.bytes;
  out.code = Code::ok;
  out.value = view;
  return out;
}

Result<ArrayRef> MemoryStorage::open(acsd::Context& context, std::string root,
                                     const acsd::Array& description,
                                     std::span<const acsd::BlockDescriptor> blocks) {
  Result<ArrayRef> out;
  out.code = Code::unsupported;
  return out;
}

// ---------------------------------------------------------------------------
// Resources.
// ---------------------------------------------------------------------------

Result<acsd::Reservation> CountingResources::reserve(U64 bytes, U32 threads) {
  acsd::Reservation reservation;
  reservation.bytes = bytes;
  reservation.threads = threads;
  Result<acsd::Reservation> out;
  out.code = Code::ok;
  out.value = reservation;
  return out;
}

Result<U64> CountingResources::consume_work(U64 units) {
  Result<U64> out;
  if (work_used_ + units > work_budget_) {
    out.code = Code::resource_exhausted;
    return out;
  }
  work_used_ += units;
  out.code = Code::ok;
  out.value = units;
  return out;
}

acsd::Usage CountingResources::usage() const {
  acsd::Usage usage;
  usage.work = work_used_;
  return usage;
}

// ---------------------------------------------------------------------------
// Byte helpers.
// ---------------------------------------------------------------------------

std::vector<std::byte> to_bytes_f64(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> to_bytes_u16(const std::vector<std::uint16_t>& values) {
  std::vector<std::byte> bytes(values.size() * 2);
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> to_bytes_u64(const std::vector<U64>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<double> decode_f64(const acsd::ArrayView& view) {
  std::vector<double> values(view.bytes.size() / 8);
  if (!values.empty()) std::memcpy(values.data(), view.bytes.data(), view.bytes.size());
  return values;
}

std::vector<std::uint16_t> decode_u16(const acsd::ArrayView& view) {
  std::vector<std::uint16_t> values(view.bytes.size() / 2);
  if (!values.empty()) std::memcpy(values.data(), view.bytes.data(), view.bytes.size());
  return values;
}

std::vector<U64> decode_u64(const acsd::ArrayView& view) {
  std::vector<U64> values(view.bytes.size() / 8);
  if (!values.empty()) std::memcpy(values.data(), view.bytes.data(), view.bytes.size());
  return values;
}

// ---------------------------------------------------------------------------
// Object builders.
// ---------------------------------------------------------------------------

Unit adu_unit(const std::string& identity) {
  acsd::Unit unit;
  unit.scale = identity;
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit dimensionless() {
  acsd::Unit unit;
  unit.scale = "1";
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

std::shared_ptr<const Index> make_index(U64 size, const std::string& namespace_id) {
  auto owned = std::make_shared<Index>();
  Index& index = *owned;
  index.size = size;
  index.kind = Index::Kind::native;
  index.namespace_id = namespace_id;
  index.identity.object_id = "index:" + namespace_id;
  index.identity.content_hash = "sha256:" + std::string(64, '0');
  index.identity.schema_id = "acsd.Index";
  index.identity.schema_major = 2;
  index.identity.schema_minor = 0;
  index.assessment.complete = true;
  index.assessment.execution = acsd::ExecutionState::complete;
  return owned;
}

std::shared_ptr<acsd::FootprintSet> make_footprints(std::shared_ptr<const Index> index, U64 width,
                                                     U64 height) {
  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->offset = {0.0, 0.0};
  footprints->local_cell.lower = {-0.5, -0.5};
  footprints->local_cell.upper = {0.5, 0.5};
  footprints->detector_frame = "detector0";
  footprints->identity.object_id = "footprints:detector0";
  footprints->identity.content_hash = "sha256:" + std::string(64, '0');
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->assessment.complete = true;
  footprints->assessment.execution = acsd::ExecutionState::complete;
  return footprints;
}

ArrayRef make_f64_array(Context& context, const std::vector<double>& values, const Unit& unit,
                        const Id& index_id) {
  const std::vector<std::byte> bytes = to_bytes_f64(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> stored = context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
  return stored.value.value();
}

ArrayRef make_u16_array(Context& context, const std::vector<std::uint16_t>& values,
                        const Id& index_id) {
  const std::vector<std::byte> bytes = to_bytes_u16(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> stored =
      context.storage.copy(context, DType::u16, bytes, shape, dimensionless(), index_id);
  return stored.value.value();
}

ArrayRef make_u64_array(Context& context, const std::vector<U64>& values, const Id& index_id) {
  const std::vector<std::byte> bytes = to_bytes_u64(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> stored =
      context.storage.copy(context, DType::u64, bytes, shape, dimensionless(), index_id);
  return stored.value.value();
}

Result<acsd::Ref<acsd::LinearAction>> make_csr(Context& context, U64 rows, U64 cols,
                                                const std::vector<U64>& offsets,
                                                const std::vector<U64>& indices,
                                                const std::vector<double>& values,
                                                std::shared_ptr<const Index> domain,
                                                std::shared_ptr<const Index> codomain,
                                                const Unit& unit) {
  const Id domain_id = domain->identity.object_id;
  const std::vector<ArrayRef> arrays{make_u64_array(context, offsets, domain_id),
                                     make_u64_array(context, indices, domain_id),
                                     make_f64_array(context, values, unit, domain_id)};
  acsd::ActionRecipe recipe;
  recipe.kind = acsd::ActionRecipe::Kind::csr;
  recipe.algorithm = "csr_v1";
  recipe.version = 1;
  recipe.arrays.push_back(arrays[0]->identity.object_id);
  recipe.arrays.push_back(arrays[1]->identity.object_id);
  recipe.arrays.push_back(arrays[2]->identity.object_id);
  recipe.spaces.domain = domain;
  recipe.spaces.codomain = codomain;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  return acsd::make_action(context, recipe, std::span<const acsd::Ref<acsd::LinearAction>>{},
                                              std::span<const ArrayRef>{arrays.data(), arrays.size()});
}

}  // namespace acsd_validation

int main() {
  int status = 0;
  status |= acsd_validation::run_calibration_cases();
  status |= acsd_validation::run_regroup_cases();
  status |= acsd_validation::run_psf_cases();
  status |= acsd_validation::run_measurement_cases();
  std::printf("\n%d checks, %d failures\n", acsd_validation::g_checks, acsd_validation::g_failures);
  return acsd_validation::g_failures == 0 ? 0 : 1;
}
