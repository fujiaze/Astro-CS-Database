// M5 fixed driver for sources.measure.
//
// The effective PSF is an explicit frozen fixture (the first-release fitter is
// reported separately in sources.psf with its named nonconverged result), and
// one Gaussian source is measured on a 9x9 native frame.  Negative: an
// observation whose native index is empty is refused before run.
#include "../lib/fixtures.hpp"

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

Ref<EffectivePSF> make_kernel(const rt::InvocationContext& ctx, Ref<Index> index,
                              Ref<FootprintSet> footprints) {
  auto parameters = vpf::make_array<double>(ctx, DType::f64, {0.1823, 0.1823, 4.6052}, {3},
                                            index, vpf::dimensionless());
  if (parameters.code != Code::ok) return {};
  auto kernel = std::make_shared<EffectivePSF>();
  kernel->family = PsfRequest::Family::gaussian;
  kernel->parameters = parameters.value->array;
  kernel->spatial_domain = Box{Vec2{-0.5, -0.5}, Vec2{8.5, 8.5}};
  kernel->wavelength_nm = Interval{450.0, 650.0};
  kernel->tail_mass_upper = 0.0;
  kernel->spatial_basis.kind = Basis::Kind::constant;
  kernel->spatial_basis.degree = 0;
  kernel->achromatic = true;
  kernel->detector_samples = std::move(footprints);
  kernel->frame = "frame0";
  kernel->identity = vpf::identity_of("psf:validation", "acsd.EffectivePSF");
  kernel->assessment.complete = true;
  return kernel;
}

}  // namespace

int main() {
  vrt::Recorder recorder("sources.measure", "validation/process/sources.measure/measure.cpp");
  auto started = vpf::start({{"coordinates.wcs", &acsd::modules::register_wcs},
                             {"sources.measure", &acsd::modules::register_measure}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  // The same TAN frame as the PSF driver; the source sits at pixel (4,4).
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
  number("CRPIX1", 5.0);
  number("CRPIX2", 5.0);
  number("CRVAL1", 10.0);
  number("CRVAL2", 20.0);
  number("CD1_1", -0.001);
  number("CD2_2", 0.001);
  auto make_wcs_input = std::make_shared<modules::MakeWcsInput>();
  make_wcs_input->header = header;
  make_wcs_input->domain = Box{Vec2{-0.5, -0.5}, Vec2{8.5, 8.5}};
  make_wcs_input->detector_frame = "detector0";
  make_wcs_input->request = vpf::budget(1e-12, 1e-12, 128);
  auto wcs = vpf::invoke<modules::MakeWcsInput, Ref<WcsModel>>(
      chain, "coordinates.wcs", "make_wcs", make_wcs_input);
  recorder.expect_code("fixture.wcs", "ok", wcs);
  if (wcs.code != Code::ok || !wcs.value) return recorder.finish();

  const std::vector<double> image =
      vpf::gaussian_image(9, 9, {std::array<double, 3>{4.0, 4.0, 100.0}}, 1.2, 0.0);
  auto native = vpf::make_native(chain.command, image,
                                 std::vector<std::uint16_t>(image.size(), 0), 9, 9, "frame0",
                                 2016.0);
  if (!native.observation) return recorder.finish();
  const Ref<EffectivePSF> kernel = make_kernel(chain.command, native.index, native.footprints);
  recorder.check("fixture.kernel", kernel != nullptr, "true", kernel ? "true" : "false");
  if (!kernel) return recorder.finish();
  const std::vector<CatalogueStar> stars{
      vpf::make_star(7, 10.0 * kDegToRad, 20.0 * kDegToRad, 2016.0)};

  auto input = std::make_shared<modules::MeasureScienceSourcesInput>();
  input->observation = native.observation;
  input->wcs = *wcs.value;
  input->stars = std::make_shared<const std::vector<CatalogueStar>>(stars);
  input->psf = kernel;
  input->request.family = PsfRequest::Family::gaussian;
  input->request.spatial_degree = 0;
  input->request.initial_width_pixel = 1.2;
  input->request.tail_absolute_flux = 0.0;
  input->request.window_radius_pixel = 3.0;
  input->request.budget = vpf::budget(1e-8, 1e-8, 256);

  auto plan = modules::plan_measure_science_sources_op(
      chain.command, input, PlanOptions{1, 1, Placement::memory});
  recorder.expect_code("measure.plan", "ok", plan);
  if (plan.code == Code::ok) {
    recorder.value("measure.plan.elements", plan.value->elements);
    recorder.value("measure.plan.persistent_host_bytes", plan.value->persistent.host_bytes);
    recorder.value("measure.plan.progress_floor_host_bytes", plan.value->progress_floor.host_bytes);
  }
  auto measured = vpf::invoke<modules::MeasureScienceSourcesInput, Ref<SourceMeasurements>>(
      chain, "sources.measure", "measure_science_sources", input);
  recorder.check("measure.named_result", measured.code != Code::internal,
                 "a named scientific code", vrt::code_name(measured.code));
  recorder.expect_code("measure.run", "ok", measured);
  if (measured.code == Code::ok && measured.value) {
    recorder.value("measure.stars", static_cast<U64>((*measured.value)->stars.size()));
    bool finite = !(*measured.value)->stars.empty();
    for (const StarFlux& star : (*measured.value)->stars)
      finite = finite && std::isfinite(star.integrated_flux);
    recorder.check("measure.finite_flux", finite, "finite",
                   std::to_string((*measured.value)->stars.size()) + " stars");
  }

  // negative: an empty native index must be refused before run.
  auto bad_observation = std::make_shared<NativeObservation>(*native.observation);
  bad_observation->native_index = vpf::make_index(0, "empty");
  auto bad_input = std::make_shared<modules::MeasureScienceSourcesInput>(*input);
  bad_input->observation = bad_observation;
  auto bad = vpf::invoke<modules::MeasureScienceSourcesInput, Ref<SourceMeasurements>>(
      chain, "sources.measure", "measure_science_sources", bad_input);
  recorder.expect_code("negative.empty_index", "invalid_model", bad);

  return recorder.finish();
}
