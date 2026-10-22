// F01 native in-memory API driver: the real photometric path
// measure_science_sources -> fit_photometry -> apply_scale.
//
// The driver only moves fields: it builds the frozen fixture (the frames, the
// catalogue stars, the response), calls the public API and reports what came
// back.  It includes only <acsd/contracts.hpp> and the shared service
// implementations; it contains no science of its own.

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "validation_support.hpp"

namespace {

using acsd::ArrayRef;
using acsd::Code;
using acsd::Context;
using acsd::Id;
using acsd::Ref;
using acsd::Result;
using acsd::U32;
using acsd::U64;
using json = nlohmann::json;
using valsupport::code_name;
using valsupport::make_index;
using valsupport::NeverCancelled;
using valsupport::store_doubles;
using valsupport::unit_dimensionless;
using valsupport::unit_luminosity;
using valsupport::unit_variance_luminosity;
using valsupport::ValidationResources;
using valsupport::ValidationStorage;

constexpr int SIDE = 16;
constexpr int FRAMES = 2;

template <class T>
bool ok(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& val(const Result<T>& result) {
  return *result.value;
}

template <class T>
Result<T> failure(Code code) {
  Result<T> out;
  out.code = code;
  return out;
}

std::string reason_of(const Result<Ref<acsd::WcsModel>>& result) {
  if (!result.assessment.issues.empty()) return result.assessment.issues.front().reason;
  return "";
}

// The doubles of an array, read back through the public storage.
std::vector<double> read_doubles(acsd::Context& context, acsd::ArrayRef array) {
  std::vector<double> values;
  if (!array) return values;
  acsd::U64 count = 1;
  for (acsd::U64 extent : array->shape) count *= extent;
  acsd::Result<acsd::ArrayView> view =
      context.storage.read(context, array, acsd::BlockRequest{0, count});
  if (!ok(view)) return values;
  values.resize(count);
  const acsd::ArrayView& view_value = *view.value;
  if (!view_value.bytes.empty()) {
    std::memcpy(values.data(), view_value.bytes.data(),
                std::min(values.size() * sizeof(double), view_value.bytes.size()));
  }
  return values;
}

// The identity and the assessment of a fixture object.  The production
// helpers live in an internal header, which an independent driver must not
// include, so the public fields are filled here.
acsd::Identity make_identity(const std::string& object_id, const std::string& schema_id) {
  acsd::Identity identity;
  identity.object_id = object_id;
  identity.content_hash = "sha256:" + valsupport::hash_hex(object_id);
  identity.run_id = "validation";
  identity.revision = 0;
  identity.schema_id = schema_id;
  identity.schema_major = 2;
  identity.schema_minor = 0;
  return identity;
}

acsd::Assessment make_assessment() {
  acsd::Assessment assessment;
  assessment.complete = true;
  assessment.validity = acsd::Validity::certified;
  assessment.science = acsd::ScienceState::bounded;
  assessment.noise = acsd::NoiseState::known;
  assessment.execution = acsd::ExecutionState::complete;
  assessment.capability = acsd::Capability::supported;
  assessment.condition.kind = "fixed_geometry";
  assessment.condition.estimator = "fixture-F01";
  return assessment;
}

// The identity action of the noise model: a public interface with no science.
class IdentityAction final : public acsd::LinearAction {
 public:
  acsd::Result<acsd::ArrayRef> apply(acsd::Context&, acsd::ArrayRef values) const override {
    acsd::Result<acsd::ArrayRef> out;
    out.code = Code::ok;
    out.value = values;
    return out;
  }

  acsd::Result<acsd::ArrayRef> transpose_apply(acsd::Context& context,
                                                acsd::ArrayRef values) const override {
    return apply(context, values);
  }

  acsd::Result<std::vector<acsd::Id>> dependency_blocks(acsd::BlockRequest, bool) const override {
    acsd::Result<std::vector<acsd::Id>> out;
    out.code = Code::ok;
    out.value = std::vector<acsd::Id>();
    return out;
  }

  acsd::Result<acsd::ActionRecipe> serialize_recipe() const override {
    acsd::Result<acsd::ActionRecipe> out;
    out.code = Code::unsupported;
    return out;
  }

  acsd::Result<std::vector<acsd::ExactInterval>> row_enclosure(acsd::Context&, acsd::U64) const override {
    acsd::Result<std::vector<acsd::ExactInterval>> out;
    out.code = Code::unsupported;
    return out;
  }
};

// The tangent point of the fixture, at the origin of the detector.
std::shared_ptr<const acsd::WcsModel> make_fixture_wcs(acsd::Context& context,
                                                          const std::string& frame) {
  acsd::HeaderFields header;
  header.push_back({"CTYPE1", std::string("RA---TAN")});
  header.push_back({"CTYPE2", std::string("DEC--TAN")});
  header.push_back({"CRPIX1", 8.5});
  header.push_back({"CRPIX2", 8.5});
  header.push_back({"CRVAL1", 10.0});
  header.push_back({"CRVAL2", 20.0});
  header.push_back({"CD1_1", -1.0e-4});
  header.push_back({"CD1_2", 0.0});
  header.push_back({"CD2_1", 0.0});
  header.push_back({"CD2_2", 1.0e-4});
  header.push_back({"RADESYS", std::string("ICRS")});
  acsd::Box domain;
  domain.lower = {0.0, 0.0};
  domain.upper = {SIDE, SIDE};
  acsd::AstrometricContext astrometry;
  astrometry.epoch_julian_year = 2016.0;
  astrometry.observation_jd_tdb = 2457600.5;
  Result<Ref<acsd::WcsModel>> model =
      acsd::make_wcs(context, header, domain, astrometry, frame, valsupport::numeric_budget());
  if (!ok(model)) return nullptr;
  return val(model);
}

// The spectral basis of the fixture: three knots and the identity basis.
struct BasisData {
  Ref<acsd::SpectralBasis> basis;
  Ref<acsd::Response> response;
  Ref<acsd::ReferenceSED> reference;
};

BasisData make_basis(acsd::Context& context) {
  BasisData out;
  std::vector<double> knots{400.0, 550.0, 700.0};
  std::vector<double> coefficients{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
  std::vector<double> wavelength{400.0, 450.0, 500.0, 550.0, 600.0, 650.0, 700.0};
  std::vector<double> throughput{0.5, 0.6, 0.6, 0.55, 0.45, 0.3, 0.15};

  auto basis = std::make_shared<acsd::SpectralBasis>();
  Result<ArrayRef> knot_array =
      store_doubles(context, knots, {knots.size()}, unit_dimensionless(), "fixture:knots");
  Result<ArrayRef> coefficient_array = store_doubles(context, coefficients, {3, 3},
                                                     unit_dimensionless(), "fixture:basis");
  if (!ok(knot_array) || !ok(coefficient_array)) return out;
  basis->knots_nm = val(knot_array);
  basis->basis_coefficients = val(coefficient_array);
  basis->degree = 2;
  basis->coefficient_count = 3;
  basis->support_nm.lower = 400.0;
  basis->support_nm.upper = 700.0;
  basis->identity = make_identity("fixture-basis", "acsd.SpectralBasis");
  basis->assessment = make_assessment();
  out.basis = Ref<acsd::SpectralBasis>(basis);

  auto response = std::make_shared<acsd::Response>();
  Result<ArrayRef> grid =
      store_doubles(context, wavelength, {wavelength.size()}, unit_dimensionless(),
                    "fixture:response:wavelength");
  Result<ArrayRef> values =
      store_doubles(context, throughput, {throughput.size()}, unit_dimensionless(),
                    "fixture:response:throughput");
  if (!ok(grid) || !ok(values)) return out;
  response->wavelength_nm = val(grid);
  response->throughput = val(values);
  response->convention = acsd::Response::Convention::photon_throughput;
  response->support_nm.lower = 400.0;
  response->support_nm.upper = 700.0;
  response->provenance = "fixture";
  response->normalization = "photon_throughput";
  response->identity = make_identity("fixture-response", "acsd.Response");
  response->assessment = make_assessment();
  out.response = Ref<acsd::Response>(response);
  return out;
}

// The noise model of one frame: an independent Gaussian per pixel with the
// variance of the read noise plus the photon noise of the background.
Ref<acsd::NoiseModel> make_noise(acsd::Context& context, double variance) {
  auto root = std::make_shared<acsd::NoiseRoot>();
  root->coordinates = make_index("fixture:noise:coordinates", SIDE * SIDE, acsd::Index::Kind::native);
  root->unit = unit_luminosity();
  root->shared_id.component = "all";
  root->shared_id.entity = "independent-root";
  root->shared_id.external_namespace = "fixture";
  root->shared_id.model_revision = "1";

  auto identity_action = std::make_shared<IdentityAction>();
  identity_action->spaces.input = unit_luminosity();
  identity_action->spaces.output = unit_luminosity();
  identity_action->spaces.domain = make_index("fixture:noise:domain", SIDE * SIDE,
                                              acsd::Index::Kind::native);
  identity_action->spaces.codomain = make_index("fixture:noise:codomain", SIDE * SIDE,
                                                acsd::Index::Kind::native);
  identity_action->identity = make_identity("fixture-noise-action", "acsd.LinearAction");
  identity_action->assessment = make_assessment();

  auto model = std::make_shared<acsd::NoiseModel>();
  model->variance_unit = unit_variance_luminosity();
  model->condition.kind = "fixed_geometry";
  model->condition.estimator = "fixture-F01";
  model->condition.assumptions.push_back("independent standard normal latent");
  model->state = acsd::NoiseState::known;
  model->innovations.push_back(Ref<acsd::NoiseRoot>(root));
  model->influence.push_back(Ref<acsd::LinearAction>(identity_action));
  model->identity = make_identity("fixture-noise", "acsd.NoiseModel");
  model->assessment = make_assessment();
  (void)context;
  (void)variance;
  return Ref<acsd::NoiseModel>(model);
}

// The footprint set of the fixture detector.
Ref<acsd::FootprintSet> make_footprints(acsd::Context& context, Ref<acsd::WcsModel> wcs,
                                         const std::string& frame) {
  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->index = make_index("fixture:native", SIDE * SIDE, acsd::Index::Kind::native);
  footprints->width = SIDE;
  footprints->height = SIDE;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->offset = {0.0, 0.0};
  footprints->local_cell = {{0.0, 0.0}, {1.0, 1.0}};
  footprints->wcs = wcs;
  footprints->detector_frame = frame;
  footprints->identity = make_identity("fixture-footprints", "acsd.FootprintSet");
  footprints->assessment = make_assessment();
  (void)context;
  return Ref<acsd::FootprintSet>(footprints);
}

}  // namespace

int main(int argc, char** argv) {
  std::string output_path;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--output" && index + 1 < argc) output_path = argv[++index];
  }

  ValidationStorage storage;
  ValidationResources resources;
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};

  json report;
  report["driver"] = "f01_native";
  report["route"] = "in_memory_public_api";

  const BasisData spectral = make_basis(context);
  if (!spectral.basis || !spectral.response) {
    std::cerr << "the spectral basis could not be built\n";
    return 2;
  }

  // The frozen fixture: two frames, two sources each.
  struct StarTruth {
    double x, y;
    double flux;
    double colour;
  };
  const std::vector<StarTruth> stars = {{5.5, 5.5, 1000.0, 1.0}, {10.25, 6.75, 640.0, 0.8}};
  const std::vector<double> frame_scales = {1.0, 0.5};  // the second frame is dimmer

  std::vector<Ref<acsd::NativeObservation>> observations;
  std::vector<Ref<acsd::WcsModel>> models;
  std::vector<Ref<acsd::FootprintSet>> footprints;
  for (int frame = 0; frame < FRAMES; ++frame) {
    const std::string frame_id = "frame-" + std::to_string(frame);
    Ref<acsd::WcsModel> wcs = make_fixture_wcs(context, frame_id);
    if (!wcs) {
      std::cerr << "the WCS could not be built\n";
      return 2;
    }
    models.push_back(wcs);
    Ref<acsd::FootprintSet> set = make_footprints(context, wcs, frame_id);
    footprints.push_back(set);

    std::vector<double> values(SIDE * SIDE, 10.0);  // the background
    for (const StarTruth& star : stars) {
      const double total = star.flux * frame_scales[frame];
      for (int row = 0; row < SIDE; ++row) {
        for (int column = 0; column < SIDE; ++column) {
          const double dx = column + 0.5 - star.x;
          const double dy = row + 0.5 - star.y;
          values[static_cast<std::size_t>(row * SIDE + column)] +=
              total * std::exp(-0.5 * (dx * dx + dy * dy) / (1.2 * 1.2)) /
              (2.0 * M_PI * 1.2 * 1.2);
        }
      }
    }
    auto observation = std::make_shared<acsd::NativeObservation>();
    Result<ArrayRef> array = store_doubles(context, values, {SIDE, SIDE}, unit_luminosity(),
                                            "fixture:" + frame_id + ":values");
    if (!ok(array)) {
      std::cerr << "the frame could not be stored\n";
      return 2;
    }
    observation->integrated_values = val(array);
    observation->native_index = make_index("fixture:native", SIDE * SIDE, acsd::Index::Kind::native);
    observation->footprints = set;
    observation->noise = make_noise(context, 100.0);
    observation->unit = unit_luminosity();
    observation->response = spectral.response;
    observation->frame = frame_id;
    observation->context.epoch_julian_year = 2016.0;
    observation->context.observation_jd_tdb = 2457600.5;
    observation->identity = make_identity("fixture-observation-" + frame_id, "acsd.NativeObservation");
    observation->assessment = make_assessment();
    observations.push_back(Ref<acsd::NativeObservation>(observation));
  }

  // The catalogue stars of the fixture.
  std::vector<acsd::CatalogueStar> catalogue;
  {
    std::vector<double> xp_coefficients{1.0, 0.0, 0.0};
    Result<ArrayRef> xp = store_doubles(context, xp_coefficients, {1, 3}, unit_dimensionless(),
                                        "fixture:xp");
    for (std::size_t index = 0; index < stars.size(); ++index) {
      acsd::CatalogueStar star;
      star.source_id = 1000 + index;
      // the pixel of the star, projected with the fixture WCS (the truth of
      // the astrometry is not the object of this case)
      const double u = stars[index].x + 1.0 - 8.5;
      const double v = stars[index].y + 1.0 - 8.5;
      const double ra = 10.0 + u * (-1.0e-4) * M_PI / 180.0;
      const double dec = 20.0 + v * (1.0e-4) * M_PI / 180.0;
      star.ra_rad = ra * M_PI / 180.0;
      star.dec_rad = dec * M_PI / 180.0;
      star.epoch_year = 2016.0;
      star.brightness_mag = -2.5 * std::log10(stars[index].colour);
      star.xp_basis = spectral.basis;
      if (ok(xp)) star.xp_coefficients = val(xp);
      catalogue.push_back(star);
    }
  }

  // ---- the PSF and the source measurements --------------------------------
  std::vector<Ref<acsd::EffectivePSF>> psfs;
  std::vector<Ref<acsd::SourceMeasurements>> measurements;
  for (int frame = 0; frame < FRAMES; ++frame) {
    acsd::PsfRequest psf_request;
    psf_request.family = acsd::PsfRequest::Family::gaussian;
    psf_request.spatial_degree = 0;
    psf_request.initial_width_pixel = 1.2;
    psf_request.tail_absolute_flux = 1e-4;
    psf_request.window_radius_pixel = 5.0;
    psf_request.budget = valsupport::numeric_budget();

    Result<Ref<acsd::EffectivePSF>> psf =
        acsd::fit_psf(context, observations[frame], models[frame], catalogue, psf_request);
    json psf_item;
    psf_item["code"] = code_name(static_cast<int>(psf.code));
    if (!ok(psf)) {
      psf_item["reason"] = psf.assessment.issues.empty()
                               ? std::string("no issue was reported")
                               : psf.assessment.issues.front().reason;
      report["frames"].push_back(psf_item);
      const std::string early = report.dump(1);
      if (!output_path.empty()) {
        std::ofstream out(output_path);
        out << early << "\n";
      }
      std::cout << early;
      return 0;
    }
    psfs.push_back(val(psf));

    Result<Ref<acsd::SourceMeasurements>> measured = acsd::measure_science_sources(
        context, observations[frame], models[frame], catalogue, val(psf), psf_request);
    json measured_item;
    measured_item["code"] = code_name(static_cast<int>(measured.code));
    if (ok(measured)) {
      json fluxes = json::array();
      for (const acsd::StarFlux& flux : val(measured)->stars) {
        fluxes.push_back({{"source_id", flux.gaia_source_id},
                          {"position", {flux.position.x, flux.position.y}},
                          {"integrated_flux", flux.integrated_flux},
                          {"variance", flux.variance.value ? *flux.variance.value : -1.0}});
      }
      measured_item["stars"] = fluxes;
      measurements.push_back(val(measured));
    } else {
      measured_item["reason"] = measured.assessment.issues.empty()
                                    ? std::string("no issue was reported")
                                    : measured.assessment.issues.front().reason;
    }
    report["frames"].push_back(measured_item);
  }

  // ---- the joint photometry ------------------------------------------------
  if (measurements.size() == static_cast<std::size_t>(FRAMES)) {
    Result<Ref<acsd::SpectralPrediction>> prediction =
        acsd::integrate_response(context, spectral.response, spectral.basis,
                                 valsupport::numeric_budget());
    json photometry_item;
    photometry_item["prediction_code"] = code_name(static_cast<int>(prediction.code));
    if (ok(prediction)) {
      acsd::PhotometryRequest request;
      request.responses.push_back(spectral.response);
      request.basis.kind = acsd::Basis::Kind::constant;
      request.basis.degree = 0;
      request.basis.center = {0.0, 0.0};
      request.basis.scale = {1.0, 1.0};
      request.basis.normalization_domain = {{0.0, 0.0}, {1.0, 1.0}};
      request.application_id = "f01-scale";
      request.solve.relative_residual = 1e-10;
      request.solve.absolute_residual = 1e-12;
      request.solve.rank_relative = 1e-10;
      request.solve.max_iterations = 100;
      request.solve.dense_max_bytes = 16777216;
      request.solve.huber = false;
      request.gradient_tolerance = 1e-10;
      request.step_tolerance = 1e-12;
      request.max_outer_iterations = 20;
      request.likelihood.kind = "joint_flux_gaussian";
      request.likelihood.estimator = "fixture-F01";
      request.likelihood.assumptions.clear();
      request.likelihood.fixed_objects.clear();
      for (int frame = 0; frame < FRAMES; ++frame) request.frames.push_back("frame-" + std::to_string(frame));

      std::vector<Ref<acsd::SpectralPrediction>> predictions{val(prediction)};
      std::vector<Ref<acsd::SourceMeasurements>> inputs;
      for (const Ref<acsd::SourceMeasurements>& measurement : measurements) inputs.push_back(measurement);

      Result<Ref<acsd::PhotoSolution>> solution =
          acsd::fit_photometry(context, inputs, catalogue, predictions, request);
      photometry_item["code"] = code_name(static_cast<int>(solution.code));
      if (ok(solution)) {
        json theta = json::array();
        for (double value : read_doubles(context, val(solution)->theta)) theta.push_back(value);
        photometry_item["theta"] = theta;
        photometry_item["rank"] = {{"columns", val(solution)->rank.columns},
                                   {"rank", val(solution)->rank.rank},
                                   {"certified", val(solution)->rank.certified}};
        photometry_item["has_covariance"] = static_cast<bool>(val(solution)->covariance);

        // the applied scale: the observation of the first frame, scaled
        Result<Ref<acsd::NativeObservation>> scaled =
            acsd::apply_scale(context, val(solution), observations[0], "frame-0");
        photometry_item["apply_scale_code"] = code_name(static_cast<int>(scaled.code));
        if (ok(scaled)) {
          photometry_item["scale_applications"] = val(scaled)->scale_applications.size();
        }
      } else {
        photometry_item["reason"] = solution.assessment.issues.empty()
                                        ? std::string("no issue was reported")
                                        : solution.assessment.issues.front().reason;
      }
    }
    report["photometry"] = photometry_item;
  }

  const std::string text = report.dump(1);
  if (!output_path.empty()) {
    std::ofstream out(output_path);
    out << text << "\n";
  }
  std::cout << text;
  return 0;
}
