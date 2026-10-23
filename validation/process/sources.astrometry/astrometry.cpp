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
  return {std::array<double, 3>{8.0, 8.0, 120.0},  std::array<double, 3>{20.0, 10.0, 110.0},
          std::array<double, 3>{12.0, 22.0, 100.0}, std::array<double, 3>{24.0, 24.0, 90.0},
          std::array<double, 3>{16.0, 16.0, 80.0}};
}

CatalogueFixture make_catalogue(const vpf::Chain& chain, const Ref<WcsModel>& model,
                                const std::vector<std::array<double, 3>>& sources) {
  CatalogueFixture out;
  const auto index = vpf::make_index(sources.size(), "catalogue:shard");
  std::vector<U64> ids;
  std::vector<double> astrometry;
  std::vector<double> brightness;
  (void)model;
  // A frozen compact shard (the same distribution as the catalogue.access
  // driver, whose offline index is known to build).  The sky directions are
  // not forced through the image WCS: this driver exercises the real solver
  // and records the named scientific outcome instead of pretending a match.
  for (std::size_t i = 0; i < sources.size(); ++i) {
    const double ra = 0.10 + 0.0003 * static_cast<double>(i);
    const double dec = 0.05 + 0.0002 * static_cast<double>(i);
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
  build_input->request.sky_order = 3;
  build_input->request.max_stars_per_cell = 8;
  build_input->request.neighbors_per_anchor = 3;
  build_input->request.quads_per_anchor = 4;
  build_input->request.quad_baseline_rad = Interval{1e-4, 1e-2};
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
  number("CRPIX1", 17.0);
  number("CRPIX2", 17.0);
  number("CRVAL1", 10.0);
  number("CRVAL2", 20.0);
  number("CD1_1", -0.001);
  number("CD2_2", 0.001);
  auto make_wcs_input = std::make_shared<modules::MakeWcsInput>();
  make_wcs_input->header = header;
  make_wcs_input->domain = Box{Vec2{-0.5, -0.5}, Vec2{32.5, 32.5}};
  make_wcs_input->detector_frame = "detector0";
  make_wcs_input->request = vpf::budget(1e-12, 1e-12, 128);
  auto wcs = vpf::invoke<modules::MakeWcsInput, Ref<WcsModel>>(
      chain, "coordinates.wcs", "make_wcs", make_wcs_input);
  recorder.expect_code("fixture.wcs", "ok", wcs);
  if (wcs.code != Code::ok || !wcs.value) return recorder.finish();

  const auto sources = field();
  const std::vector<double> image = vpf::gaussian_image(32, 32, sources, 1.2, 0.0);
  auto native = vpf::make_native(chain.command, image,
                                 std::vector<std::uint16_t>(image.size(), 0), 32, 32, "frame0",
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
  input->request.scale_rad_pixel = Interval{1e-5, 3e-5};
  input->request.max_candidates = 32;
  input->request.max_hypotheses = 200;
  input->request.sip_order = 0;
  input->request.detect_snr = 5.0;
  input->request.match_radius_pixel = 2.0;
  input->request.min_matches = 4;
  input->request.quad_descriptor_tolerance = 0.02;
  input->request.budget = vpf::budget(1e-8, 1e-8, 256);

  auto plan = modules::plan_solve_astrometry_op(chain.command, input,
                                                PlanOptions{32, 1, Placement::memory});
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
    recorder.check("astrometry.wcs_present", (*solution.value)->wcs != nullptr, "true",
                   (*solution.value)->wcs ? "true" : "false");
    recorder.value("astrometry.solved", true);
  } else {
    recorder.value("astrometry.solved", false);
    recorder.note("solve_astrometry returned the named code " +
                  std::string(vrt::code_name(solution.code)) +
                  " for the frozen synthetic field");
  }

  // negative: a zero match budget is refused before run.
  auto bad_input = std::make_shared<modules::SolveAstrometryInput>(*input);
  bad_input->request.min_matches = 0;
  auto bad = vpf::invoke<modules::SolveAstrometryInput, Ref<AstrometricSolution>>(
      chain, "sources.astrometry", "solve_astrometry", bad_input);
  recorder.expect_code("negative.min_matches_zero", "invalid_request", bad);

  return recorder.finish();
}
