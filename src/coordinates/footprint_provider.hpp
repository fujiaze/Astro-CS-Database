#pragma once
// The canonical TAN + SIP footprint provider.
//
// The provider owns the numerical geometry capability of one WcsModel: the
// enclosure of a detector box on the sky, the enclosure of the Jacobian over
// that box and the enclosure of a boundary curve.  The production area and
// overlap ledger belongs to the geometry module (acsd_geometry), which is the
// only module allowed to publish certified spherical areas.
#include <memory>

#include "acsd/contracts.hpp"

namespace acsd::coordinates {

class TanSipFootprintProvider final : public FootprintProvider {
 public:
  explicit TanSipFootprintProvider(Ref<WcsModel> model);

  Ref<WcsModel> model() const override { return model_; }
  Result<EnclosedSkyRegion> forward_interval(Context& context, Box domain) const override;
  Result<DerivativeResult> derivative_interval(Context& context, Box domain) const override;
  Result<EnclosedSkyRegion> boundary_interval(Context& context, const CurvedEdge& curve,
                                              Interval parameter) const override;
  Result<SphericalAreaInterval> measure(Context& context, const DetectorFootprint& source,
                                        const GeometryBudget& request) const override;

 private:
  Ref<WcsModel> model_;
};

// Builds the canonical provider for one WcsModel.
Result<Ref<FootprintProvider>> make_footprint_provider(Context& context, Ref<WcsModel> model);

}  // namespace acsd::coordinates
