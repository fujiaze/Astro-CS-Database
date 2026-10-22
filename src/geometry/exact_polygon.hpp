#pragma once
// Exact rational convex polygon utilities.
//
// The geometry kernel of SPHERICAL_DRIZZLE section 6 clips the detector block
// by half spaces whose constants come from the actual binary64 affine model.
// Because every one of those constants is a binary64 number, they are exact
// dyadic rationals and the whole clipping can be carried out in exact
// arithmetic (model::Rational).  The resulting areas and first moments are
// therefore exact statements about the binary64 model, not rounded doubles.
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/exact.hpp"

namespace acsd::geometry {

using model::Rational;

struct RationalPoint {
  Rational x;
  Rational y;
};

// Convex polygon, vertices in counter clockwise order.
using RationalPolygon = std::vector<RationalPoint>;

// Clips the polygon by the closed half space a*x + b*y <= c.  Returns the
// clipped polygon (possibly empty).  All arithmetic is exact.
RationalPolygon clip_half_space(const RationalPolygon& polygon, const Rational& a, const Rational& b,
                                const Rational& c);

// Signed area (shoelace).  Positive for a counter clockwise polygon.
Rational polygon_signed_area(const RationalPolygon& polygon);

// First moments: integral of x and of y over the polygon.
Rational polygon_moment_x(const RationalPolygon& polygon);
Rational polygon_moment_y(const RationalPolygon& polygon);


}  // namespace acsd::geometry
