// Binding cases: the PSF shape fit and the immutable revisions of the
// observation.
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

std::shared_ptr<acsd::WcsModel> make_wcs(Fixture& fixture, U64 width, U64 height) {
  const double scale = 1.0e-4;
  auto wcs = std::make_shared<acsd::WcsModel>();
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

acsd::CatalogueStar make_star(Fixture& fixture, std::shared_ptr<acsd::WcsModel> wcs, U64 source_id,
                               double x, double y) {
  acsd::NumericBudget budget{};
    budget.tolerance.absolute = 1.0e-10;
    budget.max_iterations = 50;
  Result<acsd::SkyICRS> sky = acsd::forward(fixture.context, wcs, acsd::PixelCenter0{x, y}, budget);
  acsd::CatalogueStar star;
  star.source_id = source_id;
  if (sky.value.has_value()) {
    const acsd::SkyICRS& direction = *sky.value;
    star.ra_rad = std::atan2(direction.ny, direction.nx);
    star.dec_rad = std::asin(std::max(-1.0, std::min(1.0, direction.nz)));
  }
  star.epoch_year = 2016.0;
  star.brightness_mag = 10.0;
  return star;
}

std::shared_ptr<acsd::Response> make_response(Fixture& fixture) {
  auto response = std::make_shared<acsd::Response>();
  response->convention = acsd::Response::Convention::photon_throughput;
  response->wavelength_nm =
      make_f64_array(fixture.context, {500.0, 600.0, 700.0}, dimensionless(), "response-wavelength");
  response->throughput =
      make_f64_array(fixture.context, {0.1, 0.2, 0.1}, dimensionless(), "response-throughput");
  response->support_nm.lower = 500.0;
  response->support_nm.upper = 700.0;
  response->provenance = "test";
  response->normalization = "relative";
  response->identity.object_id = "response:test";
  response->identity.content_hash = "sha256:" + std::string(64, '0');
  response->identity.schema_id = "acsd.Response";
  response->identity.schema_major = 2;
  response->identity.schema_minor = 0;
  response->assessment.complete = true;
  return response;
}

}  // namespace

int run_bind_cases() {
  std::printf("[bind]\n");
  Fixture fixture;
  // The frame has to be large enough to contain the wings of the profile:
  // the model is the positive density of the whole plane, so a truncated
  // frame would bias the fitted width.
  const U64 width = 32;
  const U64 height = 32;
  const U64 count = width * height;
  const double sigma = 1.6;
  const double source_x = 15.3;
  const double source_y = 14.7;

  std::shared_ptr<const acsd::Index> index = make_index(count, "detector0");
  std::shared_ptr<acsd::FootprintSet> footprints = make_footprints(index, width, height);
  std::shared_ptr<acsd::WcsModel> wcs = make_wcs(fixture, width, height);

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

  std::vector<acsd::CatalogueStar> stars;
  stars.push_back(make_star(fixture, wcs, 42, source_x, source_y));

  // ---------------------------------------------------------------------
  // The PSF shape fit.
  // ---------------------------------------------------------------------
  acsd::PsfRequest request{};
  request.family = acsd::PsfRequest::Family::gaussian;
  request.spatial_degree = 0;
  request.initial_width_pixel = 2.0;
  request.window_radius_pixel = 5.0;
  request.budget.tolerance.absolute = 1.0e-10;
  request.budget.max_iterations = 60;

  Result<acsd::Ref<acsd::EffectivePSF>> fitted =
      acsd::fit_psf(fixture.context, observation, wcs, stars, request);
  check(fitted.code == acsd::Code::ok, "the PSF shape is fitted");
  if (fitted.code != acsd::Code::ok) {
    for (const auto& issue : fitted.assessment.issues) {
      std::printf("  fit issue: %s %s\n", issue.origin.c_str(), issue.reason.c_str());
    }
  }
  if (fitted.value.has_value()) {
    const acsd::Ref<acsd::EffectivePSF> psf = *fitted.value;
    acsd::ArrayView parameters = *fixture.storage.read(fixture.context, psf->parameters,
                                                       acsd::BlockRequest{0, 3})
                                     .value;
    const std::vector<double> values = decode_f64(parameters);
    const double fitted_sigma = std::exp(values[0]);
    std::printf("  fitted sigma = %.6f (truth %.6f)\n", fitted_sigma, sigma);
    // Regression assertion: the central difference of the shape derivative has
    // to be averaged.  When the two one sided differences are summed instead,
    // the derivative is twice too large, the Gauss-Newton step overshoots and
    // the fitted width drifts away from the truth without any error message.
    check(std::fabs(fitted_sigma - sigma) < 0.05,
          "the fitted width matches the truth (regression: a central difference that is summed "
          "instead of averaged would double the shape derivative)");
    check(std::fabs(values[2]) < 1.0e-6,
          "the fitted correlation is zero (regression: the shape derivative has to be built at "
          "the current position of the source, otherwise the nominal template and the perturbed "
          "template describe different models)");
    check(psf->achromatic, "the fitted kernel is achromatic");
    check(psf->parameter_lower && psf->parameter_upper,
          "the fitted kernel carries a numerical parameter box");
    check(psf->tail_mass_upper >= 0.0 && psf->tail_mass_upper < 1.0,
          "the tail bound of the fitted window is finite");
    check(psf->detector_samples == footprints, "the kernel is locked to the native samples");
  }

  // A spatial degree of two with a single source does not determine the
  // spatial terms: the fit must say so instead of reducing the degree.
  {
    acsd::PsfRequest spatial = request;
    spatial.spatial_degree = 2;
    Result<acsd::Ref<acsd::EffectivePSF>> degenerate =
        acsd::fit_psf(fixture.context, observation, wcs, stars, spatial);
    check(degenerate.code == acsd::Code::not_identifiable,
          "a spatial degree of two with one source is not identifiable");
  }

  // ---------------------------------------------------------------------
  // bind_wcs.
  // ---------------------------------------------------------------------
  {
    Result<acsd::Ref<acsd::NativeObservation>> bound =
        acsd::bind_wcs(fixture.context, observation, wcs, nullptr);
    check(bound.code == acsd::Code::ok, "the frame model is bound");
    if (bound.value.has_value()) {
      const acsd::Ref<acsd::NativeObservation> result = *bound.value;
      check(result->identity.revision == observation->identity.revision + 1,
            "the binding produces a new revision");
      check(result->identity.object_id == observation->identity.object_id,
            "the binding keeps the logical identity");
      check(result->identity.content_hash != observation->identity.content_hash,
            "the binding changes the content hash");
      check(result->integrated_values == observation->integrated_values,
            "the binding shares the native arrays");
      check(result->noise == observation->noise, "the binding keeps the random roots");
      check(result->frame == observation->frame, "the binding keeps the frame");
      check(result->context.epoch_julian_year == observation->context.epoch_julian_year,
            "the binding keeps the astrometric context");
      check(result->footprints->wcs == wcs, "the footprint set carries the frame model");
    }
    // A frame model of another detector is refused.
    auto foreign = make_wcs(fixture, width, height);
    foreign->detector_frame = "detector1";
    foreign->identity.content_hash = "sha256:" + std::string(63, '1') + "2";
    Result<acsd::Ref<acsd::NativeObservation>> refused =
        acsd::bind_wcs(fixture.context, observation, foreign, nullptr);
    check(refused.code == acsd::Code::invalid_model,
          "a frame model of another detector is an invalid model");
    // A solution of another frame model is an identity conflict.
    auto solution = std::make_shared<acsd::AstrometricSolution>();
    solution->wcs = foreign;
    solution->identity.object_id = "solution:test";
    solution->identity.content_hash = "sha256:" + std::string(64, '0');
    solution->identity.schema_id = "acsd.AstrometricSolution";
    solution->identity.schema_major = 2;
    solution->identity.schema_minor = 0;
    solution->assessment.complete = true;
    Result<acsd::Ref<acsd::NativeObservation>> conflict =
        acsd::bind_wcs(fixture.context, observation, wcs, solution);
    check(conflict.code == acsd::Code::identity_conflict,
          "a solution of another frame model is an identity conflict");
  }

  // ---------------------------------------------------------------------
  // bind_observation_model.
  // ---------------------------------------------------------------------
  {
    auto kernel = std::make_shared<acsd::EffectivePSF>();
    kernel->family = acsd::PsfRequest::Family::gaussian;
    kernel->parameters = make_f64_array(fixture.context, {std::log(sigma), std::log(sigma), 0.0},
                                        dimensionless(), "psf-parameters");
    kernel->spatial_basis.degree = 0;
    kernel->achromatic = true;
    kernel->detector_samples = footprints;
    kernel->frame = "frame0";
    kernel->identity.object_id = "psf:bind";
    kernel->identity.content_hash = "sha256:" + std::string(64, '0');
    kernel->identity.schema_id = "acsd.EffectivePSF";
    kernel->identity.schema_major = 2;
    kernel->identity.schema_minor = 0;
    kernel->assessment.complete = true;
    auto response = make_response(fixture);

    Result<acsd::Ref<acsd::NativeObservation>> bound =
        acsd::bind_observation_model(fixture.context, observation, kernel, response);
    check(bound.code == acsd::Code::ok, "the model is bound");
    if (bound.value.has_value()) {
      const acsd::Ref<acsd::NativeObservation> result = *bound.value;
      check(result->identity.revision == observation->identity.revision + 1,
            "the model binding produces a new revision");
      check(result->kernel == kernel, "the model binding keeps the kernel");
      check(result->response == response, "the model binding keeps the response");
      check(result->noise == observation->noise, "the model binding keeps the random roots");
      check(result->integrated_values == observation->integrated_values,
            "the model binding shares the native arrays");
    }
    // A missing response is refused.
    Result<acsd::Ref<acsd::NativeObservation>> missing =
        acsd::bind_observation_model(fixture.context, observation, kernel, nullptr);
    check(missing.code == acsd::Code::invalid_request, "a missing response is an invalid request");
    // A response without wavelength samples is unavailable.
    auto empty = make_response(fixture);
    empty->wavelength_nm = nullptr;
    Result<acsd::Ref<acsd::NativeObservation>> unusable =
        acsd::bind_observation_model(fixture.context, observation, kernel, empty);
    check(unusable.code == acsd::Code::unavailable,
          "a response without wavelength samples is unavailable");
    // A kernel of another frame is an identity conflict.
    auto foreign = std::make_shared<acsd::EffectivePSF>(*kernel);
    foreign->frame = "frame1";
    Result<acsd::Ref<acsd::NativeObservation>> conflict =
        acsd::bind_observation_model(fixture.context, observation, foreign, response);
    check(conflict.code == acsd::Code::identity_conflict,
          "a kernel of another frame is an identity conflict");
  }

  std::printf("  bind cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
