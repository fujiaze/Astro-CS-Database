// M5 fixed driver for sources.astrometry.
//
// A five-star synthetic field is frozen.  The exact sky directions of the
// detected sources are produced through the typed coordinates.wcs forward
// operation from their pixel centres, the catalogue shard is built by the typed
// catalogue.access process, and solve_astrometry consumes the typed provider.
// The scientific outcome (solved or a named unsolved result) is recorded as is;
// an internal failure is never accepted.  Negative: min_matches=0 is refused
// before run.
#include "../lib/fixtures.hpp"

#include <acsd/modules/catalogue.hpp>
#include <acsd/modules/coordinates.hpp>
#include <acsd/modules/sources.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

SkyICRS to_sky(double ra, double dec) { return vpf::direction(ra, dec); }

struct CatalogueFixture {
  Ref<CatalogueProvider> provider;
  std::vector<U64> ids;
  Code build_code = Code::internal;
  Code provider_code = Code::internal;
  std::string build_issue;
};

std::vector<std::array<double, 3>> field() {
  // Twenty-six non-degenerate sources over the 100x100 detector: the solver
  // holds out one source group of five, and every negative test needs the
  // training set to stay above the eight fit parameters.
  return {std::array<double, 3>{10.0, 12.0, 140.0}, std::array<double, 3>{24.0, 9.0, 120.0},
          std::array<double, 3>{41.0, 14.0, 110.0}, std::array<double, 3>{58.0, 8.0, 100.0},
          std::array<double, 3>{76.0, 15.0, 130.0}, std::array<double, 3>{90.0, 11.0, 90.0},
          std::array<double, 3>{8.0, 30.0, 115.0},  std::array<double, 3>{22.0, 33.0, 95.0},
          std::array<double, 3>{38.0, 28.0, 125.0}, std::array<double, 3>{55.0, 36.0, 105.0},
          std::array<double, 3>{72.0, 31.0, 85.0},  std::array<double, 3>{88.0, 34.0, 120.0},
          std::array<double, 3>{14.0, 52.0, 100.0}, std::array<double, 3>{30.0, 49.0, 130.0},
          std::array<double, 3>{47.0, 55.0, 90.0},  std::array<double, 3>{64.0, 50.0, 115.0},
          std::array<double, 3>{81.0, 57.0, 110.0}, std::array<double, 3>{93.0, 48.0, 95.0},
          std::array<double, 3>{20.0, 72.0, 120.0}, std::array<double, 3>{36.0, 80.0, 105.0},
          std::array<double, 3>{53.0, 74.0, 135.0}, std::array<double, 3>{69.0, 84.0, 95.0},
          std::array<double, 3>{86.0, 77.0, 115.0}, std::array<double, 3>{12.0, 90.0, 90.0},
          std::array<double, 3>{45.0, 92.0, 110.0}, std::array<double, 3>{74.0, 94.0, 100.0}};
}

CatalogueFixture make_catalogue(const vpf::Chain& chain, const Ref<WcsModel>& model,
                                const std::vector<std::array<double, 3>>& sources) {
  CatalogueFixture out;
  const auto index = vpf::make_index(sources.size(), "catalogue:shard");
  std::vector<U64> ids;
  std::vector<double> astrometry;
  std::vector<double> brightness;
  // The catalogue is same-source with the image: every star direction is the
  // exact sky position of the injected pixel centre under the same TAN frame
  // (obtained through the typed forward operation).  The solver therefore has
  // a genuine matched catalogue, not a decoy shard.
  for (std::size_t i = 0; i < sources.size(); ++i) {
    auto forward_input = std::make_shared<modules::ForwardCenter0Input>();
    forward_input->wcs = model;
    forward_input->point = PixelCenter0{sources[i][0], sources[i][1]};
    forward_input->request = vpf::budget(1e-12, 1e-12, 128);
    auto sky = vpf::invoke<modules::ForwardCenter0Input, SkyICRS>(
        chain, "coordinates.wcs", "forward.center0", forward_input);
    if (sky.code != Code::ok) return out;
    const double ra = std::atan2(sky.value->ny, sky.value->nx);
    const double dec = std::asin(sky.value->nz);
    const U64 id = 1000 + i;
    ids.push_back(id);
    astrometry.insert(astrometry.end(), {ra, dec, 2016.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    brightness.push_back(10.0 + static_cast<double>(i));
  }
  auto id_array = vpf::make_array<U64>(chain.command, DType::u64, ids,
                                       {static_cast<U64>(ids.size())}, index,
                                       vpf::dimensionless());
  auto astrometry_array = vpf::make_array<double>(
      chain.command, DType::f64, astrometry, {static_cast<U64>(ids.size()), 8}, index,
      vpf::unit_of("rad", 1));
  auto brightness_array = vpf::make_array<double>(
      chain.command, DType::f64, brightness, {static_cast<U64>(ids.size())}, index,
      vpf::dimensionless());
  if (id_array.code != Code::ok || astrometry_array.code != Code::ok ||
      brightness_array.code != Code::ok)
    return out;
  auto columns = std::make_shared<CatalogueColumns>();
  columns->source_ids = id_array.value->array;
  columns->astrometry = astrometry_array.value->array;
  columns->brightness_mag = brightness_array.value->array;
  columns->identity = vpf::identity_of("columns:astrometry", "acsd.CatalogueColumns");
  columns->assessment.complete = true;

  auto build_input = std::make_shared<modules::BuildCatalogueIndexInput>();
  build_input->request.columns = columns;
  build_input->request.sky_order = 6;
  build_input->request.max_stars_per_cell = 16;
  build_input->request.neighbors_per_anchor = 4;
  build_input->request.quads_per_anchor = 8;
  build_input->request.quad_baseline_rad = Interval{1e-6, 1e-1};
  build_input->request.release = "validation-release";
  build_input->request.calibration = "validation-calibration";
  auto built = vpf::invoke<modules::BuildCatalogueIndexInput, Ref<CatalogueIndex>>(
      chain, "catalogue.access", "build_catalogue_index", build_input);
  out.build_code = built.code;
  out.build_issue = vrt::issue_text(built.assessment);
  if (built.code != Code::ok || !built.value) return out;
  auto provider_input = std::make_shared<modules::MakeCatalogueProviderInput>();
  provider_input->index = *built.value;
  auto provider = vpf::invoke<modules::MakeCatalogueProviderInput, Ref<CatalogueProvider>>(
      chain, "catalogue.access", "make_catalogue_provider", provider_input);
  out.provider_code = provider.code;
  if (provider.code != Code::ok || !provider.value) return out;
  out.provider = *provider.value;
  out.ids = ids;
  return out;
}

}  // namespace

int main() {
  vrt::Recorder recorder("sources.astrometry",
                         "validation/process/sources.astrometry/astrometry.cpp");
  auto started = vpf::start({{"coordinates.wcs", &acsd::modules::register_wcs},
                             {"catalogue.access", &acsd::modules::register_catalogue_access},
                             {"sources.astrometry", &acsd::modules::register_astrometry}},
                            1ull << 30, 64000000);
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  HeaderFields header;
  auto text = [&header](const char* key, const std::string& value) {
    HeaderField field;
    field.key = key;
    field.value = value;
    header.push_back(std::move(field));
  };
  auto number = [&header](const char* key, double value) {
    HeaderField field;
    field.key = key;
    field.value = value;
    header.push_back(std::move(field));
  };
  text("CTYPE1", "RA---TAN");
  text("CTYPE2", "DEC--TAN");
  text("RADESYS", "ICRS");
  number("CRPIX1", 51.0);
  number("CRPIX2", 51.0);
  number("CRVAL1", 10.0);
  number("CRVAL2", 20.0);
  number("CD1_1", -0.0005);
  number("CD2_2", 0.0005);
  auto make_wcs_input = std::make_shared<modules::MakeWcsInput>();
  make_wcs_input->header = header;
  make_wcs_input->domain = Box{Vec2{-0.5, -0.5}, Vec2{100.5, 100.5}};
  make_wcs_input->detector_frame = "detector0";
  make_wcs_input->request = vpf::budget(1e-12, 1e-12, 128);
  auto wcs = vpf::invoke<modules::MakeWcsInput, Ref<WcsModel>>(
      chain, "coordinates.wcs", "make_wcs", make_wcs_input);
  recorder.expect_code("fixture.wcs", "ok", wcs);
  if (wcs.code != Code::ok || !wcs.value) return recorder.finish();

  const auto sources = field();
  const std::vector<double> image = vpf::gaussian_image(100, 100, sources, 1.2, 0.0);
  auto native = vpf::make_native(chain.command, image,
                                 std::vector<std::uint16_t>(image.size(), 0), 100, 100, "frame0",
                                 2016.0);
  if (!native.observation) return recorder.finish();

  const CatalogueFixture catalogue = make_catalogue(chain, *wcs.value, sources);
  recorder.check("fixture.catalogue_build", catalogue.build_code == Code::ok, "ok",
                 vrt::code_name(catalogue.build_code), catalogue.build_issue);
  recorder.check("fixture.catalogue_provider", catalogue.provider != nullptr, "true",
                 catalogue.provider ? "true" : "false");
  if (!catalogue.provider) return recorder.finish();

  auto input = std::make_shared<modules::SolveAstrometryInput>();
  input->observation = native.observation;
  input->catalogue = catalogue.provider;
  input->request.scale_rad_pixel = Interval{7.0e-6, 1.2e-5};
  input->request.max_candidates = 40;
  input->request.max_hypotheses = 200;
  input->request.sip_order = 0;
  input->request.detect_snr = 3.0;
  input->request.match_radius_pixel = 1.0;
  input->request.min_matches = 9;
  input->request.quad_descriptor_tolerance = 0.02;
  input->request.budget = vpf::budget(1e-8, 1e-8, 256);

  auto plan = modules::plan_solve_astrometry_op(chain.command, input,
                                                PlanOptions{40, 1, Placement::memory});
  recorder.expect_code("astrometry.plan", "ok", plan);
  if (plan.code == Code::ok) {
    recorder.value("astrometry.plan.elements", plan.value->elements);
    recorder.value("astrometry.plan.persistent_host_bytes", plan.value->persistent.host_bytes);
    recorder.value("astrometry.plan.progress_floor_host_bytes",
                   plan.value->progress_floor.host_bytes);
  }
  auto solution = vpf::invoke<modules::SolveAstrometryInput, Ref<AstrometricSolution>>(
      chain, "sources.astrometry", "solve_astrometry", input);
  recorder.check("astrometry.named_result", solution.code != Code::internal,
                 "a named scientific code", vrt::code_name(solution.code));
  if (solution.code == Code::ok && solution.value) {
    const Ref<WcsModel> solved = (*solution.value)->wcs;
    recorder.check("astrometry.wcs_present", solved != nullptr, "true",
                   solved ? "true" : "false");
    recorder.value("astrometry.solved", true);
    if (solved) {
      const Ref<WcsModel>& injected = *wcs.value;
      const double crpix_error = std::max(std::fabs(solved->crpix_fits1[0] - 51.0),
                                          std::fabs(solved->crpix_fits1[1] - 51.0));
      recorder.value("astrometry.crpix_error_fits_pixel", crpix_error);
      recorder.check("astrometry.crpix_close", crpix_error <= 1e-3, "<=1e-3 fits pixel",
                     vrt::number(crpix_error));
      double cd_error = 0.0;
      for (std::size_t i = 0; i < 4; ++i)
        cd_error = std::max(cd_error, std::fabs(solved->cd_rad_per_pixel[i] -
                                                injected->cd_rad_per_pixel[i]));
      recorder.value("astrometry.cd_error_rad_per_pixel", cd_error);
      for (std::size_t i = 0; i < 4; ++i) {
        recorder.value("astrometry.cd_solved_" + std::to_string(i), solved->cd_rad_per_pixel[i]);
        recorder.value("astrometry.cd_injected_" + std::to_string(i), injected->cd_rad_per_pixel[i]);
      }
      recorder.value("astrometry.crpix_solved_0", solved->crpix_fits1[0]);
      recorder.value("astrometry.crpix_solved_1", solved->crpix_fits1[1]);
      const std::vector<double> parameters = vpf::read_f64(chain.command, (*solution.value)->parameters, 8);
      for (std::size_t i = 0; i < parameters.size(); ++i)
        recorder.value("astrometry.parameter_" + std::to_string(i), parameters[i]);
      recorder.check("astrometry.cd_close", cd_error <= 1e-12, "<=5e-7 rad/pixel",
                     vrt::number(cd_error));
      // Sky agreement at the field centre through both frame models.
      auto forward = [&](const Ref<WcsModel>& model, PixelCenter0 point) {
        auto forward_input = std::make_shared<modules::ForwardCenter0Input>();
        forward_input->wcs = model;
        forward_input->point = point;
        forward_input->request = vpf::budget();
        return vpf::invoke<modules::ForwardCenter0Input, SkyICRS>(
            chain, "coordinates.wcs", "forward.center0", forward_input);
      };
      for (const PixelCenter0 point : {PixelCenter0{10.0, 12.0}, PixelCenter0{90.0, 11.0}}) {
        auto injected_sky = forward(injected, point);
        auto solved_sky = forward(solved, point);
        if (injected_sky.code == Code::ok && solved_sky.code == Code::ok) {
          const double dot = injected_sky.value->nx * solved_sky.value->nx +
                             injected_sky.value->ny * solved_sky.value->ny +
                             injected_sky.value->nz * solved_sky.value->nz;
          const std::string suffix = std::to_string(static_cast<int>(point.x));
          recorder.value("astrometry.sky_dot_" + suffix, dot);
          recorder.check("astrometry.sky_matches_injection_" + suffix, dot >= 1.0 - 1e-14,
                         "dot>=1-1e-14", vrt::number(dot));
        }
      }
    }
  } else {
    recorder.value("astrometry.solved", false);
    recorder.value("astrometry.issue", vrt::issue_text(solution.assessment));
    recorder.note("solve_astrometry returned the named code " +
                  std::string(vrt::code_name(solution.code)) +
                  " for the frozen synthetic field");
  }

  // negative: a blank (source-free) frame with the same catalogue must report
  // unsolved and publish no WCS; a solver that invents a solution fails here.
  {
    const std::vector<double> blank(100 * 100, 100.0);
    auto noise_native = vpf::make_native(
        chain.command, blank, std::vector<std::uint16_t>(blank.size(), 0), 100, 100, "noise0",
        2016.0);
    if (noise_native.observation) {
      auto noise_input = std::make_shared<modules::SolveAstrometryInput>(*input);
      noise_input->observation = noise_native.observation;
      auto noise = vpf::invoke<modules::SolveAstrometryInput, Ref<AstrometricSolution>>(
          chain, "sources.astrometry", "solve_astrometry", noise_input);
      recorder.check("negative.no_fabricated_solution",
                     noise.code == Code::unsolved &&
                         !(noise.code == Code::ok && noise.value && (*noise.value)->wcs),
                     "unsolved and no WCS", vrt::code_name(noise.code));
    }
  }

  // negative: a zero match budget is refused before run.
  auto bad_input = std::make_shared<modules::SolveAstrometryInput>(*input);
  bad_input->request.min_matches = 0;
  auto bad = vpf::invoke<modules::SolveAstrometryInput, Ref<AstrometricSolution>>(
      chain, "sources.astrometry", "solve_astrometry", bad_input);
  recorder.expect_code("negative.min_matches_zero", "invalid_request", bad);

  return recorder.finish();
}
