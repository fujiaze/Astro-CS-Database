#pragma once
// The target-domain containment fallback (SPHERICAL_DRIZZLE section 3).
//
// The primary kernel of the geometry module is the affine-strip cut-cell.  The
// foundation asks for a second, independent algorithm: the equal-area
// containment integral A.  For every candidate target leaf cell of the
// chart the algorithm proves whether the whole cell lies inside or outside the
// source region.  A cell that cannot be decided keeps its full cell mass as
// the error bound; it is never estimated by centre sampling.
#include <array>
#include <vector>

#include "acsd/contracts.hpp"
#include "geometry/kernel.hpp"

namespace acsd::geometry {

// The decision of the containment test for one target cell.
enum class CellDecision { inside, outside, undecided };

struct ContainmentLeaf {
  HealpixLeaf leaf;
  CellDecision decision = CellDecision::undecided;
  // The certified enclosure of the overlap mass carried by this cell.
  Dyadic lower;
  Dyadic upper;
};

struct ContainmentOutcome {
  std::vector<ContainmentLeaf> leaves;
  // The mass of the cells that could not be decided, kept as an error bound.
  Dyadic undecided_mass;
  // True when the chart enclosure of the source region could be proven, so
  // that the per leaf intervals are a valid enclosure of the overlap mass.
  bool complete = false;
  U64 work = 0;
  // The adaptive refinement of the source boundary: the number of segments
  // the boundary was split into and the number of cells that were decided.
  U64 boundary_segments = 0;
  U64 cells_decided = 0;
  U64 cells_undecided = 0;
};

// The equal-area containment integral A over the detector rectangle.
//
// The source region is the image of the detector rectangle under the WCS.  The
// algorithm proves the containment of every candidate target cell:
//  * the separation test proves that the boundary curve of the source region
//    does not meet the cell, using the chart enclosure of the boundary
//    segments;
//  * the membership test decides the sign of the region over the cell from the
//    preimage of the cell centre.
// A cell that fails either proof is undecided and keeps its whole mass.
ContainmentOutcome target_containment(Context& context, const WcsModel& wcs, Ref<WcsModel> model_ref,
                                      const Box& detector_rect, U32 order,
                                      const GeometryBudget& budget);

// The rigorous dyadic enclosure of pi, used for the exact cell area of the
// equal-area chart (the cell area is pi/(3 nside^2) steradian).
struct DyadicInterval {
  Dyadic lower;
  Dyadic upper;
};
DyadicInterval pi_enclosure();

}  // namespace acsd::geometry
