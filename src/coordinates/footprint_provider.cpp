#include "coordinates/footprint_provider.hpp"

#include <algorithm>
#include <cmath>

#include "coordinates/tan_sip.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::coordinates {
namespace {

using Iv = numerics::Interval;

double exact_to_double(const ExactInterval& value) {
  return model::dyadic_to_double(value.lower);
}

}  // namespace

TanSipFootprintProvider::TanSipFootprintProvider(Ref<WcsModel> model) : model_(std::move(model)) {}

Result<EnclosedSkyRegion> TanSipFootprintProvider::forward_interval(Context& context,
                                                                    Box domain) const {
  using model::fail;
  using model::ok_result;
  if (!model_) {
    return fail<EnclosedSkyRegion>(Code::invalid_model, "null_wcs_model", "forward_interval");
  }
  if (context.cancellation.requested()) {
    return fail<EnclosedSkyRegion>(Code::cancelled, "cancelled", "forward_interval");
  }
  const WcsModel& model = *model_;
  const Iv u(domain.lower[0] + 1.0 - model.crpix_fits1[0],
             domain.upper[0] + 1.0 - model.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - model.crpix_fits1[1],
             domain.upper[1] + 1.0 - model.crpix_fits1[1]);
  Iv nx;
  Iv ny;
  Iv nz;
  forward_interval_iv(model, u, v, nx, ny, nz);
  if (!nx.is_finite() || !ny.is_finite() || !nz.is_finite()) {
    return fail<EnclosedSkyRegion>(Code::precision_exhausted, "non_finite_sky_enclosure",
                                   "forward_interval");
  }
  EnclosedSkyRegion region;
  region.xyz[0] = Interval(nx.lower, nx.upper);
  region.xyz[1] = Interval(ny.lower, ny.upper);
  region.xyz[2] = Interval(nz.lower, nz.upper);
  return ok_result(region);
}

Result<DerivativeResult> TanSipFootprintProvider::derivative_interval(Context& context,
                                                                      Box domain) const {
  using model::fail;
  using model::ok_result;
  if (!model_) {
    return fail<DerivativeResult>(Code::invalid_model, "null_wcs_model", "derivative_interval");
  }
  if (context.cancellation.requested()) {
    return fail<DerivativeResult>(Code::cancelled, "cancelled", "derivative_interval");
  }
  const WcsModel& model = *model_;
  const Iv u(domain.lower[0] + 1.0 - model.crpix_fits1[0],
             domain.upper[0] + 1.0 - model.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - model.crpix_fits1[1],
             domain.upper[1] + 1.0 - model.crpix_fits1[1]);
  Iv jacobian[3][2];
  jacobian_interval_iv(model, u, v, jacobian);
  DerivativeResult result;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 2; ++col) {
      const std::size_t index = static_cast<std::size_t>(row * 2 + col);
      result.enclosure[index] = Interval(jacobian[row][col].lower, jacobian[row][col].upper);
      result.jacobian[index] = jacobian[row][col].midpoint();
    }
  }
  return ok_result(result);
}

Result<EnclosedSkyRegion> TanSipFootprintProvider::boundary_interval(Context& context,
                                                                      const CurvedEdge& curve,
                                                                      Interval parameter) const {
  using model::fail;
  using model::ok_result;
  (void)parameter;
  if (!model_) {
    return fail<EnclosedSkyRegion>(Code::invalid_model, "null_wcs_model", "boundary_interval");
  }
  if (context.cancellation.requested()) {
    return fail<EnclosedSkyRegion>(Code::cancelled, "cancelled", "boundary_interval");
  }
  if (curve.bernstein.empty()) {
    return fail<EnclosedSkyRegion>(Code::invalid_request, "empty_boundary_curve", "boundary_interval");
  }
  const WcsModel& model = *model_;
  EnclosedSkyRegion region;
  region.xyz[0] = Interval(1.0, -1.0);
  region.xyz[1] = Interval(1.0, -1.0);
  region.xyz[2] = Interval(1.0, -1.0);
  bool any = false;
  for (const auto& control : curve.bernstein) {
    const double x = exact_to_double(control[0]);
    const double y = exact_to_double(control[1]);
    const Vec2 uv = coordinates::pixel_to_uv(model, x, y);
    Iv nx;
    Iv ny;
    Iv nz;
    forward_interval_iv(model, Iv(uv[0], uv[0]), Iv(uv[1], uv[1]), nx, ny, nz);
    if (!nx.is_finite() || !ny.is_finite() || !nz.is_finite()) {
      return fail<EnclosedSkyRegion>(Code::precision_exhausted, "non_finite_boundary_enclosure",
                                     "boundary_interval");
    }
    const Iv hull0 = numerics::interval_hull(Iv(region.xyz[0].lower, region.xyz[0].upper), nx);
    const Iv hull1 = numerics::interval_hull(Iv(region.xyz[1].lower, region.xyz[1].upper), ny);
    const Iv hull2 = numerics::interval_hull(Iv(region.xyz[2].lower, region.xyz[2].upper), nz);
    region.xyz[0] = Interval(hull0.lower, hull0.upper);
    region.xyz[1] = Interval(hull1.lower, hull1.upper);
    region.xyz[2] = Interval(hull2.lower, hull2.upper);
    any = true;
  }
  if (!any) {
    return fail<EnclosedSkyRegion>(Code::invalid_request, "empty_boundary_curve", "boundary_interval");
  }
  return ok_result(region);
}

Result<SphericalAreaInterval> TanSipFootprintProvider::measure(Context& context,
                                                                const DetectorFootprint& source,
                                                                const GeometryBudget& request) const {
  (void)context;
  (void)source;
  (void)request;
  // The certified area ledger is owned by the geometry module: this provider
  // publishes the model and the enclosures, not an independent area.  The
  // boundary is named, never silent.
  Result<SphericalAreaInterval> out =
      model::fail<SphericalAreaInterval>(Code::unsupported, "area_ledger_owned_by_geometry_module",
                                         "measure:source_footprint");
  model::add_issue(out.assessment, Code::unsupported, "area_ledger_owned_by_geometry_module",
                   "measure:source_footprint",
                   "the certified area of this footprint is published by acsd_geometry::measure and "
                   "acsd_geometry::compute_overlap, not by the provider",
                   "call the geometry module for the certified spherical area");
  return out;
}

}  // namespace acsd::coordinates

namespace acsd {

// The single public construction entry of a footprint provider (contracts.hpp).
// The provider identity is bound to the content hash of its WcsModel, so two
// different WCS models must never share one provider instance.
Result<Ref<FootprintProvider>> make_footprint_provider(Context& context, Ref<WcsModel> model) {
  using model::fail;
  using model::ok_result;
  if (!model) {
    return fail<Ref<FootprintProvider>>(Code::invalid_model, "null_wcs_model", "make_footprint_provider");
  }
  if (context.cancellation.requested()) {
    return fail<Ref<FootprintProvider>>(Code::cancelled, "cancelled", "make_footprint_provider");
  }
  auto provider = std::make_shared<coordinates::TanSipFootprintProvider>(model);
  return ok_result(std::static_pointer_cast<const FootprintProvider>(provider));
}

}  // namespace acsd
