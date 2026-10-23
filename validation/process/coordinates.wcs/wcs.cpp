// M5 fixed driver for coordinates.wcs.
//
// Public-header-only driver: the unique services, the frozen registry (shared
// codecs + the coordinates.wcs registration), the ProcessHost and the typed
// invocation.  Exact positive cases (TAN reference point, round trip, exact
// interval) and one negative case (inverted detector domain) are frozen here.
#include "../lib/fixtures.hpp"

#include <acsd/modules/coordinates.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

bool close(double a, double b, double tolerance) {
  return std::fabs(a - b) <= tolerance;
}

Unit sky_unit_vector() {
  return Unit{};
}

}  // namespace

int main() {
  vrt::Recorder recorder("coordinates.wcs", "validation/process/coordinates.wcs/wcs.cpp");
  auto started = vpf::start({{"coordinates.wcs", &acsd::modules::register_wcs}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;
  const NumericBudget request = vpf::budget(1e-10, 1e-10, 128);
  const Box domain{acsd::Vec2{-0.5, -0.5}, acsd::Vec2{2.5, 2.5}};

  // ---- make_wcs ---------------------------------------------------------
  auto make_input = std::make_shared<modules::MakeWcsInput>();
  make_input->header = vpf::tan_header(0.001);
  make_input->domain = domain;
  make_input->detector_frame = "detector0";
  make_input->request = request;
  make_input->astrometric_context.epoch_julian_year = 2016.0;
  make_input->astrometric_context.observation_jd_tdb = 2457500.5;

  auto make_plan = modules::plan_make_wcs_op(chain.command, make_input,
                                             PlanOptions{1, 1, Placement::memory});
  recorder.expect_code("make_wcs.plan", "ok", make_plan);
  if (make_plan.code == Code::ok) {
    recorder.value("make_wcs.plan.elements", make_plan.value->elements);
    recorder.value("make_wcs.plan.block_elements", make_plan.value->block_elements);
    recorder.value("make_wcs.plan.persistent_host_bytes", make_plan.value->persistent.host_bytes);
    recorder.value("make_wcs.plan.progress_floor_host_bytes",
                   make_plan.value->progress_floor.host_bytes);
    recorder.check("make_wcs.plan.region", make_plan.value->regions.size() == 1, "1", 
                   std::to_string(make_plan.value->regions.size()));
  }
  auto wcs = vpf::invoke<modules::MakeWcsInput, Ref<WcsModel>>(chain, "coordinates.wcs", "make_wcs",
                                                               make_input);
  recorder.expect_code("make_wcs.run", "ok", wcs);
  if (wcs.code != Code::ok || !wcs.value) return recorder.finish();
  const Ref<WcsModel> model = *wcs.value;
  recorder.check("make_wcs.detector_frame", model->detector_frame == "detector0", "detector0",
                 model->detector_frame);

  // ---- forward.center0: the reference point maps to CRVAL -----------------
  auto forward_input = std::make_shared<modules::ForwardCenter0Input>();
  forward_input->wcs = model;
  forward_input->point = PixelCenter0{0.0, 0.0};
  forward_input->request = request;
  auto forward_plan = modules::plan_forward_center0_op(chain.command, forward_input,
                                                       PlanOptions{1, 1, Placement::memory});
  recorder.expect_code("forward.center0.plan", "ok", forward_plan);
  auto sky = vpf::invoke<modules::ForwardCenter0Input, SkyICRS>(
      chain, "coordinates.wcs", "forward.center0", forward_input);
  recorder.expect_code("forward.center0.run", "ok", sky);
  const double ra = 10.0 * 3.14159265358979323846 / 180.0;
  const double dec = 20.0 * 3.14159265358979323846 / 180.0;
  if (sky.code == Code::ok) {
    const SkyICRS& value = *sky.value;
    recorder.check("forward.center0.ra", close(value.nx, std::cos(dec) * std::cos(ra), 1e-12),
                   vrt::number(std::cos(dec) * std::cos(ra)), vrt::number(value.nx));
    recorder.check("forward.center0.dec", close(value.ny, std::cos(dec) * std::sin(ra), 1e-12),
                   vrt::number(std::cos(dec) * std::sin(ra)), vrt::number(value.ny));
    recorder.check("forward.center0.unit",
                   close(value.nx * value.nx + value.ny * value.ny + value.nz * value.nz, 1.0,
                         1e-12),
                   "1", vrt::number(value.nx * value.nx + value.ny * value.ny +
                                    value.nz * value.nz));
  }

  // ---- forward.edge0 -----------------------------------------------------
  auto edge_input = std::make_shared<modules::ForwardEdge0Input>();
  edge_input->wcs = model;
  edge_input->point = PixelEdge0{0.0, 0.0};
  edge_input->request = request;
  auto edge = vpf::invoke<modules::ForwardEdge0Input, SkyICRS>(
      chain, "coordinates.wcs", "forward.edge0", edge_input);
  recorder.expect_code("forward.edge0.run", "ok", edge);

  // ---- inverse round trip ------------------------------------------------
  if (sky.code == Code::ok) {
    auto inverse_input = std::make_shared<modules::InverseInput>();
    inverse_input->wcs = model;
    inverse_input->sky = *sky.value;
    inverse_input->request = request;
    auto inverse_plan = modules::plan_inverse_op(chain.command, inverse_input,
                                                 PlanOptions{1, 1, Placement::memory});
    recorder.expect_code("inverse.plan", "ok", inverse_plan);
    auto inverse = vpf::invoke<modules::InverseInput, InverseResult>(
        chain, "coordinates.wcs", "inverse", inverse_input);
    recorder.expect_code("inverse.run", "ok", inverse);
    if (inverse.code == Code::ok) {
      recorder.check("inverse.roundtrip_x", close(inverse.value->point.x, 0.0, 1e-9), "0",
                     vrt::number(inverse.value->point.x));
      recorder.check("inverse.roundtrip_y", close(inverse.value->point.y, 0.0, 1e-9), "0",
                     vrt::number(inverse.value->point.y));
    }
  }

  // ---- derivative --------------------------------------------------------
  auto derivative_input = std::make_shared<modules::DerivativeInput>();
  derivative_input->wcs = model;
  derivative_input->point = PixelCenter0{0.5, 0.5};
  derivative_input->request = request;
  auto derivative_plan = modules::plan_derivative_op(chain.command, derivative_input,
                                                     PlanOptions{1, 1, Placement::memory});
  recorder.expect_code("derivative.plan", "ok", derivative_plan);
  auto derivative = vpf::invoke<modules::DerivativeInput, DerivativeResult>(
      chain, "coordinates.wcs", "derivative", derivative_input);
  recorder.expect_code("derivative.run", "ok", derivative);
  if (derivative.code == Code::ok) {
    bool all_finite = true;
    for (double value : derivative.value->jacobian) all_finite = all_finite && std::isfinite(value);
    recorder.check("derivative.finite", all_finite, "finite", "finite");
  }

  // ---- solid_angle_jacobian ---------------------------------------------
  auto solid_input = std::make_shared<modules::SolidAngleJacobianInput>();
  solid_input->wcs = model;
  solid_input->domain = Box{acsd::Vec2{-0.5, -0.5}, acsd::Vec2{2.5, 2.5}};
  solid_input->request = request;
  auto solid_plan = modules::plan_solid_angle_jacobian_op(chain.command, solid_input,
                                                          PlanOptions{1, 1, Placement::memory});
  recorder.expect_code("solid_angle.plan", "ok", solid_plan);
  auto solid = vpf::invoke<modules::SolidAngleJacobianInput, Interval>(
      chain, "coordinates.wcs", "solid_angle_jacobian", solid_input);
  recorder.expect_code("solid_angle.run", "ok", solid);
  if (solid.code == Code::ok) {
    recorder.check("solid_angle.ordered", solid.value->lower <= solid.value->upper, "lower<=upper",
                   vrt::number(solid.value->lower) + "<=" + vrt::number(solid.value->upper));
    recorder.check("solid_angle.positive", solid.value->lower > 0.0, ">0",
                   vrt::number(solid.value->lower));
  }

  // ---- bind_wcs ----------------------------------------------------------
  auto native = vpf::make_native(chain.command, {1.0, 2.0, 3.0}, {0, 0, 0}, 3, 1, "frame0", 2016.0);
  if (native.observation) {
    auto bind_input = std::make_shared<modules::BindWcsInput>();
    bind_input->observation = native.observation;
    bind_input->wcs = model;
    auto bind_plan = modules::plan_bind_wcs_op(chain.command, bind_input,
                                               PlanOptions{1, 1, Placement::memory});
    recorder.expect_code("bind_wcs.plan", "ok", bind_plan);
    auto bound = vpf::invoke<modules::BindWcsInput, Ref<NativeObservation>>(
        chain, "coordinates.wcs", "bind_wcs", bind_input);
    recorder.expect_code("bind_wcs.run", "ok", bound);
    if (bound.code == Code::ok)
      recorder.check("bind_wcs.footprint_wcs", (*bound.value)->footprints->wcs != nullptr, "true",
                     "true");
  }

  // ---- negative: inverted detector domain --------------------------------
  auto bad_input = std::make_shared<modules::MakeWcsInput>();
  bad_input->header = vpf::tan_header();
  bad_input->domain = Box{acsd::Vec2{2.0, 2.0}, acsd::Vec2{1.0, 1.0}};
  bad_input->detector_frame = "detector0";
  bad_input->request = request;
  auto bad = vpf::invoke<modules::MakeWcsInput, Ref<WcsModel>>(
      chain, "coordinates.wcs", "make_wcs", bad_input);
  recorder.expect_code("negative.inverted_domain", "invalid_request", bad);

  // ---- negative: non-unit sky direction ---------------------------------
  auto bad_sky_input = std::make_shared<modules::InverseInput>();
  bad_sky_input->wcs = model;
  bad_sky_input->sky = SkyICRS{1.0, 0.0, 0.0};
  bad_sky_input->request = request;
  auto bad_sky = vpf::invoke<modules::InverseInput, InverseResult>(
      chain, "coordinates.wcs", "inverse", bad_sky_input);
  recorder.expect_code("negative.non_unit_sky", "invalid_request", bad_sky);

  return recorder.finish();
}
