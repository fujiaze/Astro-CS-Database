// Measurement cases: the numerical flux bounds of the science sources.
//
// The F01 counterexample: the same nominal counts with a narrow and with a
// wide native interval must produce a narrow and a wide flux enclosure.  The
// nominal flux is kept in both cases and the bounds say which of the two
// situations was measured.
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include "obs_validation.hpp"

namespace acsd_validation {
namespace {

struct Fixture {
  MemoryStorage storage;
  CountingResources resources{100000000};
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

// A simple tangent plane frame model without a distortion.
std::shared_ptr<acsd::WcsModel> make_wcs(Fixture& fixture, U64 width, U64 height) {
  const double scale = 1.0e-4;  // rad per pixel
  auto wcs = std::make_shared<acsd::WcsModel>();
  // The FITS reference pixel is the first pixel: CRPIX = (1,1) gives u = x.
  wcs->crpix_fits1 = {1.0, 1.0};
  wcs->cd_rad_per_pixel = {scale, 0.0, 0.0, scale};
  wcs->c = {1.0, 0.0, 0.0};
  wcs->e = {0.0, 1.0, 0.0};
  wcs->north = {0.0, 0.0, 1.0};
  wcs->sip_a.order = 0;
  wcs->sip_b.order = 0;
  wcs->domain.lower = {-0.5, -0.5};
  wcs->domain.upper = {static_cast<double>(width) - 0.5, static_cast<double>(height) - 0.5};
  wcs->detector_frame = "detector0";
  wcs->identity.object_id = "wcs:test";
  wcs->identity.content_hash = "sha256:" + std::string(64, '0');
  wcs->identity.schema_id = "acsd.WcsModel";
  wcs->identity.schema_major = 2;
  wcs->identity.schema_minor = 0;
  wcs->assessment.complete = true;
  return wcs;
}

// The catalogue star of a detector position, obtained from the frame model.
acsd::CatalogueStar make_star(Fixture& fixture, std::shared_ptr<acsd::WcsModel> wcs,
                               U64 source_id, double x, double y) {
  acsd::NumericBudget budget{};
    budget.tolerance.absolute = 1.0e-10;
    budget.max_iterations = 50;
  Result<acsd::SkyICRS> sky =
      acsd::forward(fixture.context, wcs, acsd::PixelCenter0{x, y}, budget);
  acsd::CatalogueStar star;
  star.source_id = source_id;
  if (sky.value.has_value()) {
    const acsd::SkyICRS& direction = *sky.value;
    star.ra_rad = std::atan2(direction.ny, direction.nx);
    star.dec_rad = std::asin(std::max(-1.0, std::min(1.0, direction.nz)));
  }
  star.epoch_year = 2016.0;
  star.pmra_cosdec_rad_year = 0.0;
  star.pmdec_rad_year = 0.0;
  star.parallax_rad = 0.0;
  star.brightness_mag = 10.0;
  return star;
}

}  // namespace

int run_measurement_cases() {
  std::printf("[measurement]\n");
  Fixture fixture;
  const U64 width = 12;
  const U64 height = 12;
  const U64 count = width * height;
  const double sigma = 1.6;
  const double source_x = 5.3;
  const double source_y = 6.7;

  std::shared_ptr<const acsd::Index> index = make_index(count, "detector0");
  std::shared_ptr<acsd::FootprintSet> footprints = make_footprints(index, width, height);
  std::shared_ptr<acsd::WcsModel> wcs = make_wcs(fixture, width, height);

  // The synthetic frame: a Gaussian source of total flux 100 on a background
  // of 10, built with the analytic pixel integral.
  std::vector<double> truth(count, 10.0);
  for (U64 row = 0; row < count; ++row) {
    const double x = static_cast<double>(row % width);
    const double y = static_cast<double>(row / width);
    truth[static_cast<std::size_t>(row)] +=
        100.0 * gaussian_pixel_integral(source_x, source_y, x, y, sigma);
  }

  auto raw = std::make_shared<acsd::RawObservation>();
  raw->adu = make_f64_array(fixture.context, truth, adu_unit("detector0"), index->identity.object_id);
  raw->dq = make_u16_array(fixture.context, std::vector<std::uint16_t>(count, 0),
                           index->identity.object_id);
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = footprints;
  raw->frame = "frame0";
  raw->context.epoch_julian_year = 2016.0;
  raw->identity.object_id = "raw:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, '0');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;

  acsd::Calibration calibration;
  calibration.exposure_seconds = 1.0;
  calibration.dark_exposure_seconds = 1.0;
  calibration.flat_normalization = 1.0;
  Result<acsd::Ref<acsd::NativeObservation>> calibrated =
      acsd::calibrate_observation(fixture.context, raw, calibration);
  check(calibrated.code == acsd::Code::ok, "the frame is calibrated");
  if (!calibrated.value.has_value()) return 1;
  const acsd::Ref<acsd::NativeObservation> observation = *calibrated.value;
  check(observation->noise->state == acsd::NoiseState::unknown,
        "the noise is unknown without an electron expectation");

  // The PSF used by the measurement: a Gaussian of the true width.
  auto psf = std::make_shared<acsd::EffectivePSF>();
  psf->family = acsd::PsfRequest::Family::gaussian;
  psf->parameters = make_f64_array(fixture.context, {std::log(sigma), std::log(sigma), 0.0},
                                    dimensionless(), "psf-parameters");
  psf->spatial_basis.kind = acsd::Basis::Kind::constant;
  psf->spatial_basis.degree = 0;
  psf->spatial_basis.center = {0.5 * static_cast<double>(width), 0.5 * static_cast<double>(height)};
  psf->spatial_basis.scale = {0.5 * static_cast<double>(width), 0.5 * static_cast<double>(height)};
  psf->achromatic = true;
  psf->detector_samples = footprints;
  psf->regroup_scale_project = nullptr;
  psf->frame = "frame0";
  psf->identity.object_id = "psf:test";
  psf->identity.content_hash = "sha256:" + std::string(64, '0');
  psf->identity.schema_id = "acsd.EffectivePSF";
  psf->identity.schema_major = 2;
  psf->identity.schema_minor = 0;
  psf->assessment.complete = true;

  std::vector<acsd::CatalogueStar> stars;
  stars.push_back(make_star(fixture, wcs, 42, source_x, source_y));
  {
    // The round trip of the frame model is checked before the measurement.
    acsd::NumericBudget budget{};
    budget.tolerance.absolute = 1.0e-10;
    budget.max_iterations = 50;
    Result<acsd::SkyICRS> sky = acsd::forward(fixture.context, wcs,
                                               acsd::PixelCenter0{source_x, source_y}, budget);
    check(sky.value.has_value(), "the frame model maps the source position");
    if (sky.value.has_value()) {
      Result<acsd::InverseResult> back =
          acsd::inverse(fixture.context, wcs, *sky.value, budget);
      check(back.value.has_value(), "the frame model inverts the source position");
      if (!back.value.has_value()) {
        for (const auto& issue : back.assessment.issues) {
          std::printf("  inverse issue: %s %s\n", issue.origin.c_str(), issue.reason.c_str());
        }
      }
      if (back.value.has_value()) {
        check_close(back.value->point.x, source_x, 1.0e-9, "the detector round trip is exact");
        check_close(back.value->point.y, source_y, 1.0e-9, "the detector round trip is exact");
      }
    }
  }

  acsd::PsfRequest request{};
  request.family = acsd::PsfRequest::Family::gaussian;
  request.spatial_degree = 0;
  request.initial_width_pixel = sigma;
  request.window_radius_pixel = 5.0;
  request.budget.tolerance.absolute = 1.0e-12;

  Result<acsd::Ref<acsd::SourceMeasurements>> measured =
      acsd::measure_science_sources(fixture.context, observation, wcs, stars, psf, request);
  check(measured.code == acsd::Code::ok, "the science sources are measured");
  if (!measured.value.has_value()) {
    for (const auto& issue : measured.assessment.issues) {
      std::printf("  issue: %s %s\n", issue.origin.c_str(), issue.reason.c_str());
    }
    return 1;
  }
  const acsd::Ref<acsd::SourceMeasurements> result = *measured.value;
  check(result->frame == observation->frame, "the measured frame is the frame of the observation");
  check(result->observation == observation, "the measurement keeps a strong reference to the native");
  check(result->stars.size() == 1, "one science source is measured");
  check(result->stars[0].gaia_source_id == 42, "the source identity is the catalogue identity");
  check_close(result->stars[0].position.x, source_x, 1.0e-6, "the source position is recovered");
  check_close(result->stars[0].position.y, source_y, 1.0e-6, "the source position is recovered");
  std::printf("  measured flux = %.10f (truth 100)\n", result->stars[0].integrated_flux);
  check(std::fabs(result->stars[0].integrated_flux - 100.0) < 1.0,
        "the measured flux is close to the truth");
  check(result->stars[0].variance.value.has_value() == false,
        "the variance is null when the noise is unknown");
  check(result->stars[0].variance.state == acsd::NoiseState::unknown,
        "the variance state is unknown when the noise is unknown");
  check(result->training_pixel_ids && result->training_pixel_ids->shape[0] > 0,
        "the training pixels keep the connection with the native roots");

  // The F01 counterexample: the same nominal counts with a narrow and with a
  // wide native interval.
  auto measure_with_interval = [&](double half_width, std::vector<double>& lower,
                                    std::vector<double>& upper) -> bool {
    auto narrow = std::make_shared<acsd::NativeObservation>(*observation);
    narrow->numerical_lower = make_f64_array(fixture.context, truth, adu_unit("detector0"),
                                              index->identity.object_id);
    narrow->numerical_upper = make_f64_array(fixture.context, truth, adu_unit("detector0"),
                                              index->identity.object_id);
    if (half_width > 0.0) {
      std::vector<double> low = truth;
      std::vector<double> high = truth;
      for (U64 row = 0; row < count; ++row) {
        low[static_cast<std::size_t>(row)] -= half_width;
        high[static_cast<std::size_t>(row)] += half_width;
      }
      narrow->numerical_lower =
          make_f64_array(fixture.context, low, adu_unit("detector0"), index->identity.object_id);
      narrow->numerical_upper =
          make_f64_array(fixture.context, high, adu_unit("detector0"), index->identity.object_id);
    }
    Result<acsd::Ref<acsd::SourceMeasurements>> m =
        acsd::measure_science_sources(fixture.context, narrow, wcs, stars, psf, request);
    if (!m.value.has_value()) return false;
    if (!(*m.value)->flux_numerical_lower || !(*m.value)->flux_numerical_upper) return false;
    acsd::ArrayView low_view = *fixture.storage.read(fixture.context, (*m.value)->flux_numerical_lower,
                                                      acsd::BlockRequest{0, 1})
                                    .value;
    acsd::ArrayView high_view = *fixture.storage.read(fixture.context, (*m.value)->flux_numerical_upper,
                                                       acsd::BlockRequest{0, 1})
                                     .value;
    lower = decode_f64(low_view);
    upper = decode_f64(high_view);
    return true;
  };

  std::vector<double> tight_lower;
  std::vector<double> tight_upper;
  std::vector<double> wide_lower;
  std::vector<double> wide_upper;
  const bool tight_ok = measure_with_interval(0.0, tight_lower, tight_upper);
  const bool wide_ok = measure_with_interval(1.0, wide_lower, wide_upper);
  check(tight_ok, "the narrow interval produces flux bounds");
  check(wide_ok, "the wide interval produces flux bounds");
  if (tight_ok && wide_ok) {
    const double tight_radius = 0.5 * (tight_upper[0] - tight_lower[0]);
    const double wide_radius = 0.5 * (wide_upper[0] - wide_lower[0]);
    check(wide_radius > 10.0 * tight_radius,
          "the wide native interval produces a much wider flux enclosure");
    check(tight_lower[0] <= 100.0 && tight_upper[0] >= 100.0,
          "the narrow enclosure contains the exact flux");
    check(wide_lower[0] <= 100.0 && wide_upper[0] >= 100.0,
          "the wide enclosure contains the exact flux");
    std::printf("  flux enclosure half width: narrow %.6g, wide %.6g\n", tight_radius, wide_radius);
  }

  std::printf("  measurement cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
