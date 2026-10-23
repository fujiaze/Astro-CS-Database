// C-CAL kernel/plan unit case of observation.calibrate.
//
// This is the in-memory half of the M4 acceptance: it exercises the unique
// per-sample kernel (formula, outward enclosure, data quality rules), the fixed
// plan (N/n/cohorts, stage/edge ids, R/F numbers and payload bounds) and the
// metadata-only validate/bounds.  It does not open the Host, a pool or a
// Storage; the full driver -> registry -> pool/Storage -> Native chain is the
// separate cal_host case once every runtime service is linked.
//
// The only non-public include is the module's own kernel header, which owns the
// declarations of the functions under test.
#include "../../../src/observation/calibration_kernel.hpp"

#include <acsd/contracts.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Array;
using acsd::ArrayRef;
using acsd::Calibration;
using acsd::Code;
using acsd::DType;
using acsd::FootprintSet;
using acsd::Index;
using acsd::RawObservation;
using acsd::Ref;
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

ArrayRef make_array(DType dtype, const std::vector<U64>& shape, const Unit& unit,
                    const std::string& index_id) {
  auto array = std::make_shared<Array>();
  array->dtype = dtype;
  array->shape = shape;
  array->unit = unit;
  array->index_id = index_id;
  array->identity.object_id = "acsd.Array:validation";
  array->identity.content_hash = "sha256:" + std::string(64, '0');
  array->identity.schema_id = "acsd.Array";
  array->identity.schema_major = 2;
  array->identity.schema_minor = 0;
  array->assessment.complete = true;
  return array;
}

Ref<Index> make_index(U64 size, const std::string& object_id) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::native;
  index->namespace_id = "detector0";
  index->identity.object_id = object_id;
  index->identity.content_hash = "sha256:" + std::string(64, '0');
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  return index;
}

Ref<FootprintSet> make_footprints(Ref<Index> index) {
  auto footprints = std::make_shared<FootprintSet>();
  footprints->index = index;
  footprints->width = 1;
  footprints->height = 1;
  footprints->identity.object_id = "acsd.FootprintSet:validation";
  footprints->identity.content_hash = "sha256:" + std::string(64, '0');
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->assessment.complete = true;
  return footprints;
}

// A metadata-only raw observation: validate/bounds/plan never read payload, so
// no Storage is needed and the absence of a block is part of the contract.
struct MetadataFrame {
  Ref<FootprintSet> footprints;
  Ref<RawObservation> raw;
  Calibration calibration;
};

MetadataFrame make_frame(U64 width, U64 height) {
  MetadataFrame frame;
  const U64 count = width * height;
  const std::vector<U64> shape{count};
  const Ref<Index> index = make_index(count, "acsd.Index:detector0");
  frame.footprints = make_footprints(index);
  auto raw = std::make_shared<RawObservation>();
  raw->adu = make_array(DType::f64, shape, adu_unit(), index->identity.object_id);
  raw->dq = make_array(DType::u16, shape, dq_unit(), index->identity.object_id);
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = frame.footprints;
  raw->frame = "frame0";
  raw->context.epoch_julian_year = 2016.0;
  raw->context.observation_jd_tdb = 2457500.5;
  raw->identity.object_id = "acsd.RawObservation:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, '0');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;
  frame.raw = raw;

  Calibration& calibration = frame.calibration;
  calibration.bias = make_array(DType::f64, shape, adu_unit(), index->identity.object_id);
  calibration.dark = make_array(DType::f64, shape, adu_unit(), index->identity.object_id);
  calibration.flat = make_array(DType::f64, shape, adu_unit(), index->identity.object_id);
  calibration.dark_contains_bias = true;
  calibration.exposure_seconds = 2.0;
  calibration.dark_exposure_seconds = 1.0;
  calibration.flat_normalization = 2.0;
  return frame;
}

Ref<acsd::modules::CalibrateInput> make_input(const MetadataFrame& frame) {
  auto input = std::make_shared<acsd::modules::CalibrateInput>();
  input->raw = frame.raw;
  input->request = frame.calibration;
  return input;
}

acsd::observation::CalibrationSampleParameters parameters() {
  acsd::observation::CalibrationSampleParameters parameters;
  parameters.has_bias = true;
  parameters.has_dark = true;
  parameters.has_flat = true;
  parameters.dark_contains_bias = true;
  parameters.exposure_ratio = 2.0;
  parameters.flat_normalization = 2.0;
  return parameters;
}

struct Sample {
  double raw = 0.0;
  double bias = 0.0;
  double dark = 0.0;
  double flat = 0.0;
  std::uint16_t dq = 0;
};

acsd::Result<acsd::observation::CalibratedSample> run_sample(
    const Sample& sample, const acsd::observation::CalibrationSampleParameters& policy) {
  acsd::observation::CalibrationSampleInputs inputs;
  inputs.raw = sample.raw;
  inputs.bias = sample.bias;
  inputs.dark = sample.dark;
  inputs.flat = sample.flat;
  inputs.dq = sample.dq;
  inputs.parameters = policy;
  return acsd::observation::calibrate_sample(inputs);
}

std::string code_name(Code code) { return std::string(vrt::code_name(code)); }

}  // namespace

int main() {
  vrt::Recorder recorder("cal_kernel",
                         "validation/runtime/calibration/cal_kernel.cpp");
  const acsd::observation::CalibrationSampleParameters policy = parameters();

  // -------------------------------------------------------------------------
  // C-CAL-1: x = (raw - b - beta*(D-b))/f, negative values are not clipped.
  // -------------------------------------------------------------------------
  {
    const Sample samples[3] = {{10.0, 2.0, 4.0, 2.0, 0}, {20.0, 2.0, 4.0, 2.0, 0},
                               {-2.0, 2.0, 4.0, 2.0, 0}};
    const double expected[3] = {4.0, 14.0, -8.0};
    bool values_ok = true;
    bool enclosure_ok = true;
    bool dq_ok = true;
    for (int i = 0; i < 3; ++i) {
      auto result = run_sample(samples[i], policy);
      values_ok = values_ok && result.code == Code::ok && result.value &&
                  result.value->value == expected[i];
      enclosure_ok = enclosure_ok && result.value &&
                     result.value->lower <= expected[i] && expected[i] <= result.value->upper;
      dq_ok = dq_ok && result.value && result.value->dq == 0;
      if (result.value) {
        recorder.value("c_cal_1.x" + std::to_string(i), result.value->value);
        recorder.value("c_cal_1.lower" + std::to_string(i), result.value->lower);
        recorder.value("c_cal_1.upper" + std::to_string(i), result.value->upper);
      }
    }
    recorder.check("cal.c_cal_1_x", values_ok, "[4,14,-8]",
                   values_ok ? "[4,14,-8]" : "different values",
                   "x = (raw - b - beta*(D-b))/f with beta=2, f=2/2");
    recorder.check("cal.c_cal_1_enclosure", enclosure_ok, "lower <= exact <= upper",
                   enclosure_ok ? "contained" : "not contained",
                   "the same outward expression encloses its nominal value");
    recorder.check("cal.c_cal_1_dq", dq_ok, "0", dq_ok ? "0" : "different flags",
                   "a legal sample keeps its data quality flags");
  }

  // -------------------------------------------------------------------------
  // C-CAL-2: a dark that does not contain the bias is used directly.
  // -------------------------------------------------------------------------
  {
    auto dark_free = policy;
    dark_free.dark_contains_bias = false;
    const Sample samples[3] = {{10.0, 2.0, 4.0, 2.0, 0}, {20.0, 2.0, 4.0, 2.0, 0},
                               {-2.0, 2.0, 4.0, 2.0, 0}};
    const double expected[3] = {0.0, 10.0, -12.0};
    bool values_ok = true;
    for (int i = 0; i < 3; ++i) {
      auto result = run_sample(samples[i], dark_free);
      values_ok = values_ok && result.code == Code::ok && result.value &&
                  result.value->value == expected[i];
      if (result.value) recorder.value("c_cal_2.x" + std::to_string(i), result.value->value);
    }
    recorder.check("cal.c_cal_2_x", values_ok, "[0,10,-12]",
                   values_ok ? "[0,10,-12]" : "different values",
                   "the dark without bias is scaled directly, not through D-b");
  }

  // -------------------------------------------------------------------------
  // C-CAL-DQ: flat <= 0 and a NaN/DQ conflict.
  // -------------------------------------------------------------------------
  {
    const Sample sample{10.0, 2.0, 4.0, 0.0, 0};
    auto result = run_sample(sample, policy);
    const bool ok = result.code == Code::ok && result.value &&
                    std::isnan(result.value->value) && !result.value->flat_usable &&
                    (result.value->dq & 1u) != 0u;
    recorder.check("cal.c_cal_dq_flat_zero", ok, "NaN value, bad flag, flat unusable",
                   ok ? "NaN+bad" : "different result",
                   "flat<=0 is not repaired numerically");
    auto missing = run_sample(Sample{10.0, 2.0, 4.0, 0.0, 8u}, policy);
    const bool keeps = missing.code == Code::ok && missing.value &&
                       std::isnan(missing.value->value) && missing.value->dq == 8u;
    recorder.check("cal.c_cal_dq_flat_zero_keeps_existing_dq", keeps,
                   "missing stays missing (no extra bad bit)",
                   keeps ? "8" : "different flags",
                   "an already disqualifying sample is not rewritten");
  }
  {
    auto valid = run_sample(Sample{10.0, 2.0, 4.0, 2.0, 0u}, policy);
    recorder.check("cal.c_cal_dq_nan_metadata_ok", valid.code == Code::ok, "ok",
                   code_name(valid.code), "the kernel of a legal sample is ok");
    auto conflict = run_sample(Sample{kNaN, 2.0, 4.0, 2.0, 0u}, policy);
    recorder.check("cal.c_cal_dq_nan_valid_dq_rejected", conflict.code == Code::invalid_model,
                   "invalid_model", code_name(conflict.code),
                   "a NaN raw value with valid DQ conflicts with the flags");
    auto missing = run_sample(Sample{kNaN, 2.0, 4.0, 2.0, 8u}, policy);
    const bool accepted = missing.code == Code::ok && missing.value &&
                          std::isnan(missing.value->value) && missing.value->dq == 8u &&
                          std::isnan(missing.value->lower) && std::isnan(missing.value->upper);
    recorder.check("cal.c_cal_dq_nan_missing_accepted", accepted,
                   "ok, NaN value, unchanged missing flag", accepted ? "ok" : "different result",
                   "a NaN raw value with a disqualifying flag is accepted without a value");
  }
  {
    // A finite expression that overflows is not repaired either.
    auto overflow = run_sample(Sample{1.0e308, 0.0, 0.0, 1.0e-308, 0u}, policy);
    const bool ok = overflow.code == Code::ok && overflow.value &&
                    std::isnan(overflow.value->value) && (overflow.value->dq & 1u) != 0u;
    recorder.check("cal.c_cal_dq_overflow_not_repaired", ok, "NaN value, bad flag",
                   ok ? "NaN+bad" : "different result",
                   "an overflow is not clipped and dares not be reported as a width of 0");
  }

  // -------------------------------------------------------------------------
  // Fixed plan numbers: N=3 (one cohort) and N=200000 (4 cohorts).
  // The constants are the frozen plan of CALIBRATION_WALKTHROUGH.md:
  // S=1 MiB storage workspace, 4 KiB cohort control, 16 KiB final metadata,
  // 16 KiB plan control.
  // -------------------------------------------------------------------------
  const U64 kStorageWorkspace = 1ull << 20;
  const U64 kCohortControl = 4ull << 10;
  const U64 kFinalMetadata = 16ull << 10;
  const U64 kPlanControl = 16ull << 10;
  auto plan_of = [&](MetadataFrame& frame, U64 block) {
    rt::PlanOptions options;
    options.block_elements = block;
    options.window = 1;
    options.placement = rt::Placement::memory;
    return acsd::modules::plan_calibration(rt::InvocationContext{}, make_input(frame), options);
  };
  {
    MetadataFrame frame = make_frame(3, 1);
    auto plan = plan_of(frame, 3);
    const bool ok = plan.code == Code::ok && plan.value;
    recorder.check("cal.plan_small_ok", ok, "ok", code_name(plan.code),
                   "the fixed small frame plans");
    if (ok) {
      const rt::InvocationPlan& value = *plan.value;
      recorder.value("plan.small.elements", value.elements);
      recorder.value("plan.small.block", value.block_elements);
      recorder.value("plan.small.cohorts", value.cohorts);
      recorder.value("plan.small.f", value.progress_floor.host_bytes);
      recorder.value("plan.small.r", value.persistent.host_bytes);
      const U64 expected_floor = 60 * 3 + kStorageWorkspace + kCohortControl;
      const U64 expected_persistent = 26 * 3 + 16 * 3 + kFinalMetadata + kPlanControl;
      recorder.check("cal.plan_small_structure",
                     value.regions.size() == 1 && value.regions[0].id == 0 &&
                         value.regions[0].kind == rt::RegionKind::stream &&
                         value.regions[0].cohorts == 1 && value.regions[0].stages.size() == 3 &&
                         value.regions[0].edges.size() == 2 && !value.adaptive &&
                         value.regions[0].stages[1].work_upper == 3,
                     "region0 stream, cohorts1, stages3, edges2, adaptive=false",
                     "structure differs", "fixed plan of the walkthrough");
      recorder.check("cal.plan_small_floor", value.progress_floor.host_bytes == expected_floor,
                     std::to_string(expected_floor),
                     std::to_string(value.progress_floor.host_bytes), "F = 60n + S + control");
      recorder.check("cal.plan_small_persistent",
                     value.persistent.host_bytes == expected_persistent,
                     std::to_string(expected_persistent),
                     std::to_string(value.persistent.host_bytes),
                     "R = 26N + 16N (unknown noise placeholder) + final metadata + control");
      recorder.check("cal.plan_small_edges",
                     value.regions[0].edges[0].from == 0 && value.regions[0].edges[0].to == 1 &&
                         value.regions[0].edges[1].from == 1 && value.regions[0].edges[1].to == 2 &&
                         value.regions[0].edges[0].id == 0 && value.regions[0].edges[1].id == 1,
                     "edges 0->1, 1->2", "different edges", "fixed stage graph");
    }
  }
  {
    MetadataFrame frame = make_frame(400, 500);  // N = 200000
    auto plan = plan_of(frame, 65536);
    const bool ok = plan.code == Code::ok && plan.value;
    if (ok) {
      const rt::InvocationPlan& value = *plan.value;
      recorder.value("plan.large.elements", value.elements);
      recorder.value("plan.large.block", value.block_elements);
      recorder.value("plan.large.cohorts", value.cohorts);
      recorder.value("plan.large.f", value.progress_floor.host_bytes);
      recorder.value("plan.large.r", value.persistent.host_bytes);
      recorder.value("plan.large.stage1_work", value.regions[0].stages[1].work_upper);
      const U64 expected_floor = 60 * 65536 + kStorageWorkspace + kCohortControl;
      const U64 expected_persistent = 26 * 200000 + 16 * 200000 + kFinalMetadata + kPlanControl;
      recorder.check("cal.plan_large_cohorts", value.cohorts == 4, "4",
                     std::to_string(value.cohorts), "ceil(200000/65536)");
      recorder.check("cal.plan_large_block", value.block_elements == 65536, "65536",
                     std::to_string(value.block_elements), "the initial block elements");
      recorder.check("cal.plan_large_floor", value.progress_floor.host_bytes == expected_floor,
                     std::to_string(expected_floor),
                     std::to_string(value.progress_floor.host_bytes), "F = 60n + S + control");
      recorder.check("cal.plan_large_persistent",
                     value.persistent.host_bytes == expected_persistent,
                     std::to_string(expected_persistent),
                     std::to_string(value.persistent.host_bytes),
                     "R includes the full output and the final model arrays");
      recorder.check("cal.plan_large_work", value.regions[0].stages[1].work_upper == 65536,
                     "65536", std::to_string(value.regions[0].stages[1].work_upper),
                     "one work unit per native sample of the cohort");
    } else {
      recorder.check("cal.plan_large_ok", false, "ok", code_name(plan.code),
                     "the large frame must plan");
    }
  }
  {
    MetadataFrame frame = make_frame(100, 100);
    auto bounds = acsd::modules::bounds_calibration(rt::InvocationContext{}, make_input(frame));
    const bool ok = bounds.code == Code::ok && bounds.value;
    recorder.check("cal.bounds_ok", ok, "ok", code_name(bounds.code),
                   "bounds of a 10000 sample frame");
    if (ok) {
      recorder.check("cal.bounds_range",
                     bounds.value->min_block_elements == 1 &&
                         bounds.value->max_block_elements == 10000 &&
                         bounds.value->max_window >= 2 && bounds.value->allow_spill,
                     "min=1, max=N, window>=2, spill allowed",
                     "different bounds", "PlanBounds contract");
    }
    rt::PlanOptions bad;
    bad.block_elements = 10001;
    auto rejected = acsd::modules::plan_calibration(rt::InvocationContext{}, make_input(frame), bad);
    recorder.check("cal.plan_block_out_of_range", rejected.code == Code::invalid_request,
                   "invalid_request", code_name(rejected.code), "block > N is rejected");
    bad.block_elements = 0;
    rejected = acsd::modules::plan_calibration(rt::InvocationContext{}, make_input(frame), bad);
    recorder.check("cal.plan_block_zero", rejected.code == Code::invalid_request,
                   "invalid_request", code_name(rejected.code), "block = 0 is rejected");
    bad.block_elements = 16;
    bad.window = 0;
    rejected = acsd::modules::plan_calibration(rt::InvocationContext{}, make_input(frame), bad);
    recorder.check("cal.plan_window_zero", rejected.code == Code::invalid_request,
                   "invalid_request", code_name(rejected.code), "window = 0 is rejected");
  }

  // -------------------------------------------------------------------------
  // Metadata-only validate: positive + every documented negative.
  // No payload is read; the NaN/DQ conflict is a kernel check, not a validate
  // check.
  // -------------------------------------------------------------------------
  {
    MetadataFrame frame = make_frame(3, 1);
    auto valid = acsd::modules::validate_calibration(rt::InvocationContext{}, make_input(frame));
    recorder.check("cal.validate_ok", valid.code == Code::ok, "ok", code_name(valid.code),
                   "the metadata of C-CAL-1 is valid");

    MetadataFrame no_flat = make_frame(3, 1);
    no_flat.calibration.flat = nullptr;
    no_flat.calibration.flat_normalization = 1.0;
    auto absent_flat = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                           make_input(no_flat));
    recorder.check("cal.validate_absent_flat", absent_flat.code == Code::ok, "ok",
                   code_name(absent_flat.code), "an absent flat is the explicit normalisation 1");

    no_flat.calibration.flat_normalization = 0.0;
    auto absent_zero = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                           make_input(no_flat));
    recorder.check("cal.validate_absent_flat_zero_rejected",
                   absent_zero.code == Code::invalid_model, "invalid_model",
                   code_name(absent_zero.code), "absent flat with normalisation 0 is rejected");

    MetadataFrame bad_exposure = make_frame(3, 1);
    bad_exposure.calibration.exposure_seconds = 0.0;
    auto exposure = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                        make_input(bad_exposure));
    recorder.check("cal.validate_exposure", exposure.code == Code::invalid_model,
                   "invalid_model", code_name(exposure.code), "exposure must be positive");

    MetadataFrame bad_dark = make_frame(3, 1);
    bad_dark.calibration.dark_exposure_seconds = 0.0;
    auto dark = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                    make_input(bad_dark));
    recorder.check("cal.validate_dark_exposure", dark.code == Code::invalid_model,
                   "invalid_model", code_name(dark.code),
                   "dark exposure must be positive when a dark is used");

    MetadataFrame bad_flat = make_frame(3, 1);
    bad_flat.calibration.flat_normalization = 0.0;
    auto flat = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                    make_input(bad_flat));
    recorder.check("cal.validate_flat_normalization", flat.code == Code::invalid_model,
                   "invalid_model", code_name(flat.code),
                   "flat normalisation must be positive when a flat is used");

    MetadataFrame bad_shape = make_frame(3, 1);
    const std::vector<U64> wrong_shape{4};
    {
      auto raw_copy = std::make_shared<RawObservation>(*bad_shape.raw);
      raw_copy->adu = make_array(DType::f64, wrong_shape, adu_unit(),
                                 raw_copy->source_index->identity.object_id);
      bad_shape.raw = raw_copy;
    }
    auto shape = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                     make_input(bad_shape));
    recorder.check("cal.validate_shape", shape.code == Code::invalid_model, "invalid_model",
                   code_name(shape.code), "the raw array shape must cover the frame");

    MetadataFrame bad_dtype = make_frame(3, 1);
    {
      auto raw_copy = std::make_shared<RawObservation>(*bad_dtype.raw);
      raw_copy->adu = make_array(DType::u16, {3}, adu_unit(),
                                 raw_copy->source_index->identity.object_id);
      bad_dtype.raw = raw_copy;
    }
    auto dtype = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                     make_input(bad_dtype));
    recorder.check("cal.validate_dtype", dtype.code == Code::invalid_model, "invalid_model",
                   code_name(dtype.code), "the raw data array must be f64");

    MetadataFrame bad_dq = make_frame(3, 1);
    {
      auto raw_copy = std::make_shared<RawObservation>(*bad_dq.raw);
      raw_copy->dq = make_array(DType::u16, {3}, adu_unit(),
                                raw_copy->source_index->identity.object_id);
      bad_dq.raw = raw_copy;
    }
    auto dq = acsd::modules::validate_calibration(rt::InvocationContext{}, make_input(bad_dq));
    recorder.check("cal.validate_dq_unit", dq.code == Code::invalid_model, "invalid_model",
                   code_name(dq.code), "the data quality array must be dimensionless");

    MetadataFrame no_footprints = make_frame(3, 1);
    {
      auto raw_copy = std::make_shared<RawObservation>(*no_footprints.raw);
      raw_copy->footprints = nullptr;
      no_footprints.raw = raw_copy;
    }
    auto footprints = acsd::modules::validate_calibration(rt::InvocationContext{},
                                                          make_input(no_footprints));
    recorder.check("cal.validate_footprints", footprints.code == Code::unavailable,
                   "unavailable", code_name(footprints.code),
                   "the footprint capability is missing");


    auto missing_input = acsd::modules::validate_calibration(rt::InvocationContext{}, nullptr);
    recorder.check("cal.validate_missing_input", missing_input.code == Code::invalid_request,
                   "invalid_request", code_name(missing_input.code),
                   "a null input is invalid_request");
  }

  recorder.resource("checks", static_cast<long long>(recorder.attempts()));
  recorder.resource("successes", static_cast<long long>(recorder.successes()));
  return recorder.finish();
}
