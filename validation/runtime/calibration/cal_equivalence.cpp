// P1-4 equivalence: the synchronous calibrate_observation entry and the typed
// validate_calibration hook must return the same metadata verdict for the same
// RawObservation/Calibration.  The only allowed difference is the payload-level
// NaN/DQ conflict, which validate must not see (no payload I/O) and the CPU
// stage rejects after the block hash.
//
// The file brings its own in-memory science Storage/Resources/Cancellation; it
// includes only the public contract plus the test harness.
#include <acsd/contracts.hpp>
#include <acsd/modules/observation.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Array;
using acsd::ArrayRef;
using acsd::Calibration;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::FootprintSet;
using acsd::Index;
using acsd::RawObservation;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using acsd::Unit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

Unit adu_unit() {
  Unit unit;
  unit.scale = "ADU";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit dq_unit() {
  Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

template <class T>
std::vector<std::byte> to_bytes(const std::vector<T>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(T));
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

Result<ArrayRef> ok_array(const std::shared_ptr<Array>& array) {
  Result<ArrayRef> out;
  out.code = Code::ok;
  out.value = std::shared_ptr<const Array>(array);
  out.assessment.complete = true;
  return out;
}

class MemoryStorage final : public acsd::Storage {
 public:
  Result<ArrayRef> copy(Context&, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, acsd::Id index) override {
    Entry entry;
    entry.dtype = dtype;
    entry.shape.assign(shape.begin(), shape.end());
    entry.unit = unit;
    entry.index = std::move(index);
    entry.bytes.assign(bytes.begin(), bytes.end());
    const acsd::Id id = "acsd.Array:fixture:" + std::to_string(sequence_++);
    entries_[id] = std::move(entry);
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape = entries_[id].shape;
    array->unit = unit;
    array->index_id = entries_[id].index;
    array->identity.object_id = id;
    array->identity.content_hash = "sha256:" + std::string(64, 'a');
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    array->assessment.complete = true;
    return ok_array(array);
  }

  Result<acsd::ArrayView> read(Context&, ArrayRef array, acsd::BlockRequest range) const override {
    Result<acsd::ArrayView> out;
    const auto found = entries_.find(array->identity.object_id);
    if (found == entries_.end()) {
      out.code = Code::invalid_request;
      return out;
    }
    const Entry& entry = found->second;
    const std::size_t item = entry.dtype == DType::u16 ? 2 : 8;
    const std::size_t offset = static_cast<std::size_t>(range.begin) * item;
    const std::size_t length = static_cast<std::size_t>(range.count) * item;
    if (offset + length > entry.bytes.size()) {
      out.code = Code::invalid_request;
      return out;
    }
    acsd::ArrayView view;
    view.dtype = entry.dtype;
    view.shape = entry.shape;
    view.bytes = std::span<const std::byte>(entry.bytes.data() + offset, length);
    out.code = Code::ok;
    out.value = std::move(view);
    return out;
  }

  Result<ArrayRef> open(Context&, std::string, const Array&,
                        std::span<const acsd::BlockDescriptor>) override {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  struct Entry {
    DType dtype = DType::f64;
    std::vector<U64> shape;
    Unit unit;
    acsd::Id index;
    std::vector<std::byte> bytes;
  };
  U64 sequence_ = 0;
  std::map<acsd::Id, Entry> entries_;
};

class FixtureResources final : public acsd::Resources {
 public:
  Result<acsd::Reservation> reserve(U64 bytes, acsd::U32 threads) override {
    Result<acsd::Reservation> out;
    out.code = Code::ok;
    out.value = acsd::Reservation{acsd::Lease{}, bytes, threads};
    return out;
  }
  Result<U64> consume_work(U64 units) override {
    Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    return out;
  }
  acsd::Usage usage() const override { return {}; }
};

class FixtureCancellation final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

struct Fixture {
  MemoryStorage storage;
  FixtureResources resources;
  FixtureCancellation cancellation;
  Context context{resources, cancellation, storage};
};

Ref<Index> make_index(U64 size) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::native;
  index->namespace_id = "detector0";
  index->identity.object_id = "acsd.Index:detector0";
  index->identity.content_hash = "sha256:" + std::string(64, 'b');
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  return index;
}

Ref<FootprintSet> make_footprints(Ref<Index> index, U64 width, U64 height) {
  auto footprints = std::make_shared<FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->detector_frame = "frame0";
  footprints->identity.object_id = "acsd.FootprintSet:frame0";
  footprints->identity.content_hash = "sha256:" + std::string(64, 'c');
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->assessment.complete = true;
  return footprints;
}

ArrayRef store_f64(Context& context, const std::vector<double>& values,
                   const std::vector<U64>& shape, const Unit& unit, const acsd::Id& index) {
  const std::vector<std::byte> bytes = to_bytes(values);
  Result<ArrayRef> out = context.storage.copy(context, DType::f64, bytes, shape, unit, index);
  return out.value.value_or(nullptr);
}

ArrayRef store_u16(Context& context, const std::vector<std::uint16_t>& values,
                   const std::vector<U64>& shape, const Unit& unit, const acsd::Id& index) {
  const std::vector<std::byte> bytes = to_bytes(values);
  Result<ArrayRef> out = context.storage.copy(context, DType::u16, bytes, shape, unit, index);
  return out.value.value_or(nullptr);
}

struct Frame {
  Ref<RawObservation> raw;
  Calibration request;
};

Frame make_frame(Fixture& fixture, const std::vector<double>& raw_values,
                 const std::vector<std::uint16_t>& dq, const std::vector<double>& flat,
                 U64 width, U64 height) {
  Frame frame;
  const U64 count = width * height;
  const std::vector<U64> shape{count};
  const Ref<Index> index = make_index(count);
  auto raw = std::make_shared<RawObservation>();
  raw->adu = store_f64(fixture.context, raw_values, shape, adu_unit(), index->identity.object_id);
  raw->dq = store_u16(fixture.context, dq, shape, dq_unit(), index->identity.object_id);
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = make_footprints(index, width, height);
  raw->frame = "frame0";
  raw->context.epoch_julian_year = 2016.0;
  raw->context.observation_jd_tdb = 2457500.5;
  raw->identity.object_id = "acsd.RawObservation:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, 'd');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;
  frame.raw = raw;

  Calibration& request = frame.request;
  request.bias = store_f64(fixture.context, {2.0, 2.0, 2.0}, shape, adu_unit(),
                           index->identity.object_id);
  request.dark = store_f64(fixture.context, {4.0, 4.0, 4.0}, shape, adu_unit(),
                           index->identity.object_id);
  request.flat = store_f64(fixture.context, flat, shape, adu_unit(),
                           index->identity.object_id);
  request.dark_contains_bias = true;
  request.exposure_seconds = 2.0;
  request.dark_exposure_seconds = 1.0;
  request.flat_normalization = 2.0;
  return frame;
}

std::string code_name(Code code) { return std::string(vrt::code_name(code)); }

}  // namespace

int main() {
  vrt::Recorder recorder("cal_equivalence",
                         "validation/runtime/calibration/cal_equivalence.cpp");
  Fixture fixture;
  const std::vector<double> raw_values{10.0, 20.0, -2.0};
  const std::vector<std::uint16_t> dq{0, 0, 0};

  auto same_verdict = [&](const std::string& name, Frame frame, Code expected) {
    const Result<Ref<acsd::NativeObservation>> sync =
        acsd::calibrate_observation(fixture.context, frame.raw, frame.request);
    auto input = std::make_shared<acsd::modules::CalibrateInput>();
    input->raw = frame.raw;
    input->request = frame.request;
    const Result<rt::Done> typed =
        acsd::modules::validate_calibration(rt::InvocationContext{}, input);
    const Result<rt::PlanBounds> bounds =
        acsd::modules::bounds_calibration(rt::InvocationContext{}, input);
    const bool equal = sync.code == typed.code && typed.code == expected &&
                       bounds.code == typed.code;
    recorder.value("eq." + name + ".sync", code_name(sync.code));
    recorder.value("eq." + name + ".typed", code_name(typed.code));
    recorder.value("eq." + name + ".bounds", code_name(bounds.code));
    recorder.check("cal.eq." + name, equal, code_name(expected),
                   "sync=" + code_name(sync.code) + " typed=" + code_name(typed.code) +
                       " bounds=" + code_name(bounds.code),
                   "the synchronous entry point and the typed hooks share one metadata verdict");
  };

  // 1. The fixed valid C-CAL-1 frame.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    const Result<Ref<acsd::NativeObservation>> sync =
        acsd::calibrate_observation(fixture.context, frame.raw, frame.request);
    recorder.check("cal.eq.valid", sync.code == Code::ok, "ok", code_name(sync.code),
                   "the valid frame calibrates through the synchronous entry");
  }

  // 2. DQ array with an ADU unit.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    auto raw = std::make_shared<RawObservation>(*frame.raw);
    raw->dq = store_u16(fixture.context, dq, {3}, adu_unit(), "detector0");
    frame.raw = raw;
    same_verdict("dq_not_dimensionless", frame, Code::invalid_model);
  }
  // 3. Zero exposure.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    frame.request.exposure_seconds = 0.0;
    same_verdict("exposure_zero", frame, Code::invalid_model);
  }
  // 4. Absent flat without the explicit normalisation 1.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    frame.request.flat = nullptr;
    frame.request.flat_normalization = 0.0;
    same_verdict("absent_flat_zero_normalization", frame, Code::invalid_model);
  }
  // 5. Missing detector footprint.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    auto raw = std::make_shared<RawObservation>(*frame.raw);
    raw->footprints = nullptr;
    frame.raw = raw;
    same_verdict("missing_footprints", frame, Code::unavailable);
  }
  // 6. Raw array shape does not cover the frame.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    auto raw = std::make_shared<RawObservation>(*frame.raw);
    raw->adu = store_f64(fixture.context, {1.0, 2.0, 3.0, 4.0}, {4}, adu_unit(), "detector0");
    frame.raw = raw;
    same_verdict("shape_mismatch", frame, Code::invalid_model);
  }
  // 7. Flat with the wrong dtype.
  {
    Frame frame = make_frame(fixture, raw_values, dq, {2.0, 2.0, 2.0}, 3, 1);
    frame.request.flat =
        store_u16(fixture.context, {2, 2, 2}, {3}, adu_unit(), "detector0");
    same_verdict("flat_wrong_dtype", frame, Code::invalid_model);
  }

  // 8. The one allowed difference: a NaN payload with valid DQ is metadata-valid
  // (validate does not read payload) and the synchronous CPU stage still
  // rejects it as invalid_model.  This is not a second validation rule.
  {
    Frame frame = make_frame(fixture, {10.0, kNaN, -2.0}, dq, {2.0, 2.0, 2.0}, 3, 1);
    auto input = std::make_shared<acsd::modules::CalibrateInput>();
    input->raw = frame.raw;
    input->request = frame.request;
    const Result<rt::Done> typed =
        acsd::modules::validate_calibration(rt::InvocationContext{}, input);
    const Result<Ref<acsd::NativeObservation>> sync =
        acsd::calibrate_observation(fixture.context, frame.raw, frame.request);
    const bool expected = typed.code == Code::ok && sync.code == Code::invalid_model;
    recorder.value("eq.payload_conflict.typed", code_name(typed.code));
    recorder.value("eq.payload_conflict.sync", code_name(sync.code));
    recorder.check("cal.eq.payload_conflict_after_read", expected,
                   "typed=ok, sync=invalid_model",
                   "typed=" + code_name(typed.code) + " sync=" + code_name(sync.code),
                   "validate is metadata-only; the payload conflict is rejected by the CPU stage");
  }

  recorder.resource("checks", static_cast<long long>(recorder.attempts()));
  recorder.resource("successes", static_cast<long long>(recorder.successes()));
  return recorder.finish();
}
