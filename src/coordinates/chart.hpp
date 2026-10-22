#pragma once
// Continuous equal area HEALPix face chart used by the production overlap
// kernel.
//
// The chart is the one of SPHERICAL_DRIZZLE section 2: every face f carries a
// unit square (s, t) and the map to the sphere is
//   r = J_f - s - t, d = s - t,
//   r < 1      : z = 1 - r^2/3,  phi = (pi/4)(L_f + d/r)
//   1 <= r <= 3: z = (2-r)*2/3,  phi = (pi/4)(L_f + d)
//   r > 3      : z = -1 + (4-r)^2/3, phi = (pi/4)(L_f + d/(4-r))
// with dOmega = (pi/3) ds dt.  The leaves of order N are the dyadic cells
// [i/N,(i+1)/N] x [j/N,(j+1)/N] of that chart, which is the same partition as
// the NESTED numbering used by the coordinates module.
//
// The inverse chart (direction -> face, s, t) is implemented here because the
// geometry kernel needs it on the continuous domain; the coordinates module
// owns the forward chart (coordinates::healpix_face_point).
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/interval.hpp"

namespace acsd::coordinates {

using Iv = numerics::Interval;

struct ChartPoint {
  U32 face = 0;
  double s = 0.0;
  double t = 0.0;
};

// Inverse chart: direction -> (face, s, t).  Returns false when the direction
// is not a unit vector or when no face chart contains it.
bool chart_inverse(const SkyICRS& direction, ChartPoint& out);

// Partial derivatives of (s, t) with respect to (z, phi) for the face that
// contains the given direction.  Returns false when the chart is singular
// (the apex of a polar face).
bool chart_partials(double z, double phi, double& ds_dz, double& ds_dphi, double& dt_dz,
                    double& dt_dphi);

// Interval enclosure of the partial derivatives over a box of (z, phi).
bool chart_partials_interval(const Iv& z, const Iv& phi, U32 face, Iv& ds_dz, Iv& ds_dphi, Iv& dt_dz,
                             Iv& dt_dphi);

// The chart parameters of a face.
int chart_j(U32 face);
int chart_l(U32 face);

// The longitude of a direction in the branch that belongs to the face: the
// chart coordinates of a face are defined with respect to the longitude of
// that face, so every use of the chart must use this value and not the
// absolute longitude.
double chart_face_longitude(double phi, U32 face);

// Interval enclosure of the chart coordinates over an interval of (z, phi)
// for the given face.
bool chart_interval(U32 face, const Iv& z, const Iv& phi, Iv& s, Iv& t);

// Leaf of order `order` that contains the chart cell (face, ix, iy).
HealpixLeaf chart_leaf(U32 order, U32 face, U64 ix, U64 iy);

// Leaf that contains the given direction.
bool chart_leaf_for_direction(U32 order, const SkyICRS& direction, HealpixLeaf& leaf);

}  // namespace acsd::coordinates
