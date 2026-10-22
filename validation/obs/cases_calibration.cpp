// Calibration cases: the N1 formula, the data quality rules and the noise
// accounting of calibrate_observation.
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include "obs_validation.hpp"

namespace acsd_validation {
namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Fixture {
  MemoryStorage storage;
  CountingResources resources{1000000};
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

std::shared_ptr<acsd::RawObservation> make_raw(Fixture& fixture, const std::vector<double>& adu,
                                               const std::vector<std::uint16_t>& dq, U64 width,
                                               U64 height) {
  std::shared_ptr<const Index> index = make_index(static_cast<U64>(adu.size()), "detector0");
  auto footprints = make_footprints(index, width, height);
  auto raw = std::make_shared<acsd::RawObservation>();
  raw->adu = make_f64_array(fixture.context, adu, adu_unit("detector0"),
                             index->identity.object_id);
  raw->dq = make_u16_array(fixture.context, dq, index->identity.object_id);
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = footprints;
  raw->frame = "frame0";
  raw->context.epoch_julian_year = 2016.0;
  raw->context.observation_jd_tdb = 2457500.5;
  raw->identity.object_id = "raw:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, '0');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;
  return raw;
}

acsd::Calibration make_calibration(Fixture& fixture, const std::vector<double>& bias,
                                   const std::vector<double>& dark, const std::vector<double>& flat,
                                   bool dark_contains_bias) {
  acsd::Calibration calibration;
  calibration.bias = bias.empty() ? ArrayRef()
                                  : make_f64_array(fixture.context, bias, adu_unit("detector0"),
                                                   "detector0");
  calibration.dark = dark.empty() ? ArrayRef()
                                  : make_f64_array(fixture.context, dark, adu_unit("detector0"),
                                                   "detector0");
  calibration.flat = flat.empty() ? ArrayRef()
                                  : make_f64_array(fixture.context, flat, adu_unit("detector0"),
                                                   "detector0");
  calibration.dark_contains_bias = dark_contains_bias;
  calibration.exposure_seconds = 10.0;
  calibration.dark_exposure_seconds = 5.0;
  calibration.flat_normalization = 1.0;
  return calibration;
}

}  // namespace

int run_calibration_cases() {
  std::printf("[calibration]\n");
  Fixture fixture;
  const U64 width = 4;
  const U64 height = 4;
  const U64 count = width * height;

  std::vector<double> adu(count);
  std::vector<double> bias(count, 100.0);
  std::vector<double> dark(count, 20.0);
  std::vector<double> flat(count, 2.0);
  std::vector<std::uint16_t> dq(count, 0);
  for (U64 i = 0; i < count; ++i) {
    adu[i] = 1000.0 + static_cast<double>(i);
    flat[i] = 1.0 + 0.1 * static_cast<double>(i % 4);
  }
  flat[5] = 0.0;  // a non-positive flat must not be repaired numerically

  auto raw = make_raw(fixture, adu, dq, width, height);
  acsd::Calibration calibration =
      make_calibration(fixture, bias, dark, flat, /*dark_contains_bias=*/false);
  Result<acsd::Ref<acsd::NativeObservation>> calibrated =
      acsd::calibrate_observation(fixture.context, raw, calibration);
  check(calibrated.code == Code::ok && calibrated.value.has_value(), "the calibration succeeds");
  if (calibrated.code != Code::ok) {
    std::printf("  calibration code=%d", static_cast<int>(calibrated.code));
    for (const auto& issue : calibrated.assessment.issues) {
      std::printf(" | %s: %s", issue.origin.c_str(), issue.reason.c_str());
    }
    std::printf("\n");
  }
  if (!calibrated.value.has_value()) return 1;

  const acsd::Ref<acsd::NativeObservation> native = *calibrated.value;
  check(native->frame == "frame0", "the frame is preserved");
  check_close(native->context.epoch_julian_year, 2016.0, 0.0, "the astrometric context is preserved");
  check(native->physical_collection, "the calibration is a physical collection");
  check(native->native_parent == nullptr, "the calibration has no parent");

  acsd::ArrayView values = *fixture.storage.read(fixture.context, native->integrated_values,
                                          acsd::BlockRequest{0, count}).value;
  const std::vector<double> calibrated_values = decode_f64(values);
  acsd::ArrayView flags =
      *fixture.storage.read(fixture.context, native->dq, acsd::BlockRequest{0, count}).value;
  const std::vector<std::uint16_t> calibrated_flags = decode_u16(flags);
  acsd::ArrayView low = *fixture.storage.read(fixture.context, native->numerical_lower,
                                       acsd::BlockRequest{0, count}).value;
  acsd::ArrayView high = *fixture.storage.read(fixture.context, native->numerical_upper,
                                        acsd::BlockRequest{0, count}).value;
  const std::vector<double> lower = decode_f64(low);
  const std::vector<double> upper = decode_f64(high);

  // t = 10/5 = 2 and x = (d - b - 2*dark)/flat.
  for (U64 i = 0; i < count; ++i) {
    const double expected = (adu[i] - bias[i] - 2.0 * dark[i]) / flat[i];
    if (i == 5) {
      check(std::isnan(calibrated_values[i]), "a non-positive flat yields no value");
      check((calibrated_flags[i] & 1u) != 0u, "a non-positive flat sets the bad flag");
      continue;
    }
    check_close(calibrated_values[i], expected, 1e-12, "the calibrated value follows N1");
    check(lower[i] <= calibrated_values[i] && calibrated_values[i] <= upper[i],
          "the numerical bounds enclose the nominal value");
  }
  std::printf("  x[0] = %.17g (expected %.17g)\n", calibrated_values[0],
              (adu[0] - bias[0] - 2.0 * dark[0]) / flat[0]);

  // The dark map that carries the bias.
  acsd::Calibration biased = make_calibration(fixture, bias, dark, flat, /*dark_contains_bias=*/true);
  Result<acsd::Ref<acsd::NativeObservation>> calibrated_biased =
      acsd::calibrate_observation(fixture.context, raw, biased);
  check(calibrated_biased.code == Code::ok, "the biased dark calibration succeeds");
  if (calibrated_biased.value.has_value()) {
    acsd::ArrayView values_biased = *fixture.storage.read(fixture.context,
                                                    (*calibrated_biased.value)->integrated_values,
                                                    acsd::BlockRequest{0, count}).value;
    const std::vector<double> biased_values = decode_f64(values_biased);
    for (U64 i = 0; i < count; ++i) {
      if (i == 5) continue;
      const double expected = (adu[i] - bias[i] - 2.0 * (dark[i] - bias[i])) / flat[i];
      check_close(biased_values[i], expected, 1e-12, "the biased dark is removed before scaling");
    }
  }

  // A non-finite value that conflicts with the valid data quality bits.
  {
    std::vector<double> broken = adu;
    broken[2] = kNaN;
    std::vector<std::uint16_t> broken_flags(count, 0);
    auto raw_broken = make_raw(fixture, broken, broken_flags, width, height);
    Result<acsd::Ref<acsd::NativeObservation>> rejected =
        acsd::calibrate_observation(fixture.context, raw_broken, calibration);
    check(rejected.code == Code::invalid_model,
          "a non-finite value without a disqualifying flag is an invalid model");
  }

  // The same value with the missing flag is accepted and stays unusable.
  {
    std::vector<double> broken = adu;
    broken[2] = kNaN;
    std::vector<std::uint16_t> broken_flags(count, 0);
    broken_flags[2] = 8u;
    auto raw_broken = make_raw(fixture, broken, broken_flags, width, height);
    Result<acsd::Ref<acsd::NativeObservation>> accepted =
        acsd::calibrate_observation(fixture.context, raw_broken, calibration);
    check(accepted.code == Code::ok, "a missing sample is accepted");
    if (accepted.value.has_value()) {
      acsd::ArrayView values_missing = *fixture.storage.read(fixture.context,
                                                      (*accepted.value)->integrated_values,
                                                      acsd::BlockRequest{0, count}).value;
      check(std::isnan(decode_f64(values_missing)[2]), "a missing sample carries no value");
    }
  }

  // The noise accounting.
  {
    auto raw_noise = make_raw(fixture, adu, dq, width, height);
    Result<acsd::Ref<acsd::NativeObservation>> unknown =
        acsd::calibrate_observation(fixture.context, raw_noise, calibration);
    check(unknown.value.has_value() &&
              (*unknown.value)->noise->state == acsd::NoiseState::unknown,
          "without an electron expectation the complete variance is unknown");
    check(unknown.value.has_value() && !(*unknown.value)->noise->unknown_terms.empty(),
          "the unknown shot term is listed");
  }
  {
    auto raw_plugin = make_raw(fixture, adu, dq, width, height);
    acsd::Calibration plugin = calibration;
    plugin.gain_e_per_adu = 2.0;
    plugin.read_rms_e = 5.0;
    Result<acsd::Ref<acsd::NativeObservation>> approximate =
        acsd::calibrate_observation(fixture.context, raw_plugin, plugin);
    check(approximate.value.has_value() &&
              (*approximate.value)->noise->state == acsd::NoiseState::approximate,
          "the explicit Poisson plug-in is reported as approximate");
    check(approximate.value.has_value() &&
              (*approximate.value)->noise->innovations.size() == 2,
          "the plug-in keeps the shot and the readout root");
  }

  std::printf("  calibration cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
