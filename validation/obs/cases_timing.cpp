// The cost of the photometric path on a frame that is large enough to be
// interesting: 128 by 128 native samples with one catalogue source.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "obs_validation.hpp"

namespace acsd_validation {
namespace {

struct Fixture {
  MemoryStorage storage;
  CountingResources resources{100000000000ULL};
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

}  // namespace

int run_timing_cases() {
  std::printf("[timing]\n");
  Fixture fixture;
  const U64 side = 128;
  const U64 count = side * side;
  const double sigma = 1.6;
  const double source_x = 63.3;
  const double source_y = 64.7;

  std::shared_ptr<const acsd::Index> index = make_index(count, "detector0");
  std::shared_ptr<acsd::FootprintSet> footprints = make_footprints(index, side, side);

  std::vector<double> truth(count, 10.0);
  for (U64 row = 0; row < count; ++row) {
    const double x = static_cast<double>(row % side);
    const double y = static_cast<double>(row / side);
    truth[static_cast<std::size_t>(row)] +=
        1000.0 * std::exp(-0.5 * ((x + 0.5 - source_x) * (x + 0.5 - source_x) +
                                   (y + 0.5 - source_y) * (y + 0.5 - source_y)) /
                            (sigma * sigma)) /
        (2.0 * M_PI * sigma * sigma);
  }

  auto observation = std::make_shared<acsd::NativeObservation>();
  observation->integrated_values =
      make_f64_array(fixture.context, truth, adu_unit("detector0"), index->identity.object_id);
  observation->native_index = index;
  observation->footprints = footprints;
  observation->frame = "frame0";
  observation->unit = adu_unit("detector0");
  observation->context.epoch_julian_year = 2016.0;
  observation->identity.object_id = "native:frame0";
  observation->identity.content_hash = "sha256:" + std::string(64, '0');
  observation->identity.schema_id = "acsd.NativeObservation";
  observation->identity.schema_major = 2;
  observation->identity.schema_minor = 0;
  observation->assessment.complete = true;

  // The frame model of the fixture.
  auto wcs = std::make_shared<acsd::WcsModel>();
  wcs->crpix_fits1 = {1.0, 1.0};
  wcs->cd_rad_per_pixel = {1.0e-4, 0.0, 0.0, 1.0e-4};
  wcs->c = {0.9396926208, 0.0, 0.3420201433};
  wcs->e = {0.0, 1.0, 0.0};
  wcs->north = {0.0, 0.0, 1.0};
  wcs->sip_a.order = 0;
  wcs->sip_b.order = 0;
  wcs->domain.lower = {-0.5, -0.5};
  wcs->domain.upper = {static_cast<double>(side) - 0.5, static_cast<double>(side) - 0.5};
  wcs->detector_frame = "detector0";
  wcs->identity.object_id = "wcs:frame0";
  wcs->identity.content_hash = "sha256:" + std::string(64, '0');
  wcs->identity.schema_id = "acsd.WcsModel";
  wcs->identity.schema_major = 2;
  wcs->identity.schema_minor = 0;
  wcs->assessment.complete = true;

  acsd::CatalogueStar star;
  star.source_id = 1;
  star.ra_rad = 0.0;
  star.dec_rad = 0.0;
  star.epoch_year = 2016.0;
  star.brightness_mag = 10.0;
  std::vector<acsd::CatalogueStar> stars{star};

  acsd::PsfRequest request{};
  request.family = acsd::PsfRequest::Family::gaussian;
  request.spatial_degree = 0;
  request.initial_width_pixel = 2.0;
  request.window_radius_pixel = 5.0;
  request.budget.tolerance.absolute = 1.0e-10;
  request.budget.max_iterations = 60;

  // The source position is taken from the frame model, so the fixture uses the
  // inverse projection of the pixel.
  {
    acsd::NumericBudget budget{};
    budget.tolerance.absolute = 1.0e-10;
    budget.max_iterations = 50;
    budget.tolerance.absolute = 1.0e-10;
    budget.max_iterations = 50;
    Result<acsd::SkyICRS> sky =
        acsd::forward(fixture.context, wcs, acsd::PixelCenter0{source_x, source_y}, budget);
    if (sky.value.has_value()) {
      const acsd::SkyICRS& direction = *sky.value;
      star.ra_rad = std::atan2(direction.ny, direction.nx);
      star.dec_rad = std::asin(std::max(-1.0, std::min(1.0, direction.nz)));
    }
  }

  const auto start_fit = std::chrono::steady_clock::now();
  Result<acsd::Ref<acsd::EffectivePSF>> fitted =
      acsd::fit_psf(fixture.context, observation, wcs, stars, request);
  const auto end_fit = std::chrono::steady_clock::now();
  const double fit_seconds = std::chrono::duration<double>(end_fit - start_fit).count();
  std::printf("  fit_psf on %llux%llu samples: %.3f s\n", static_cast<unsigned long long>(side),
              static_cast<unsigned long long>(side), fit_seconds);
  check(fitted.code == acsd::Code::ok, "the PSF is fitted on the large frame");
  check(fit_seconds < 60.0, "the PSF fit stays below a minute on the large frame");
  if (!fitted.value.has_value()) {
    std::printf("  timing cases done\n");
    return 1;
  }

  const auto start_measure = std::chrono::steady_clock::now();
  Result<acsd::Ref<acsd::SourceMeasurements>> measured = acsd::measure_science_sources(
      fixture.context, observation, wcs, stars, *fitted.value, request);
  const auto end_measure = std::chrono::steady_clock::now();
  const double measure_seconds = std::chrono::duration<double>(end_measure - start_measure).count();
  std::printf("  measure_science_sources on %llux%llu samples: %.3f s\n",
              static_cast<unsigned long long>(side), static_cast<unsigned long long>(side),
              measure_seconds);
  check(measured.code == acsd::Code::ok, "the sources are measured on the large frame");
  check(measure_seconds < 60.0, "the measurement stays below a minute on the large frame");

  std::printf("  timing cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
