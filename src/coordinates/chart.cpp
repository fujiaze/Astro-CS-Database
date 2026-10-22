#include "coordinates/chart.hpp"

#include <algorithm>
#include <cmath>

#include "coordinates/healpix.hpp"

namespace acsd::coordinates {
namespace {

constexpr double kPi = 3.14159265358979323846;

// J_f and L_f of the equal area face chart (SPHERICAL_DRIZZLE section 2).
constexpr int kJ[12] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
constexpr int kL[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};

// Brings the longitude into the branch that belongs to the face with offset L.
double face_longitude(double phi, int l) {
  const double centre = static_cast<double>(l) * kPi / 4.0;
  const double two_pi = 2.0 * kPi;
  double shifted = phi;
  while (shifted - centre > kPi / 4.0) shifted -= two_pi;
  while (centre - shifted > kPi / 4.0) shifted += two_pi;
  return shifted;
}

struct FaceChart {
  int face = 0;
  int j = 0;
  int l = 0;
  bool cap_north = false;
  bool cap_south = false;
};

// The candidate faces for a given z: the north polar faces for z > 2/3, the
// equatorial faces plus one of the polar halves inside the band, and the south
// polar faces for z < -2/3.
std::vector<FaceChart> candidate_charts(double z) {
  std::vector<FaceChart> charts;
  if (z > 2.0 / 3.0) {
    for (int f = 0; f < 4; ++f) charts.push_back({f, kJ[f], kL[f], true, false});
    return charts;
  }
  if (z < -2.0 / 3.0) {
    for (int f = 8; f < 12; ++f) charts.push_back({f, kJ[f], kL[f], false, true});
    return charts;
  }
  // The equatorial band is shared by the equatorial faces and by the band part
  // of the polar faces (the zebra structure of the standard HEALPix charts).
  for (int f = 4; f < 8; ++f) charts.push_back({f, kJ[f], kL[f], false, false});
  if (z >= 0.0) {
    for (int f = 0; f < 4; ++f) charts.push_back({f, kJ[f], kL[f], false, false});
  } else {
    for (int f = 8; f < 12; ++f) charts.push_back({f, kJ[f], kL[f], false, false});
  }
  return charts;
}

bool chart_from_face(const FaceChart& chart, double z, double phi, double& s, double& t) {
  double r = 0.0;
  double d = 0.0;
  if (chart.cap_north) {
    r = std::sqrt(3.0 * (1.0 - z));
    if (!(r > 0.0)) return false;
    d = r * (4.0 * phi / kPi - static_cast<double>(chart.l));
  } else if (chart.cap_south) {
    const double w = std::sqrt(3.0 * (1.0 + z));
    if (!(w > 0.0)) return false;
    r = 4.0 - w;
    d = (4.0 * phi / kPi - static_cast<double>(chart.l)) * w;
  } else {
    r = 2.0 - 1.5 * z;
    d = 4.0 * phi / kPi - static_cast<double>(chart.l);
  }
  s = (static_cast<double>(chart.j) - r + d) / 2.0;
  t = (static_cast<double>(chart.j) - r - d) / 2.0;
  return true;
}

}  // namespace

bool chart_inverse(const SkyICRS& direction, ChartPoint& out) {
  const double norm = std::sqrt(direction.nx * direction.nx + direction.ny * direction.ny +
                                direction.nz * direction.nz);
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const double z = direction.nz / norm;
  const double phi = std::atan2(direction.ny / norm, direction.nx / norm);
  for (const FaceChart& chart : candidate_charts(z)) {
    const double phi_face = face_longitude(phi, chart.l);
    double s = 0.0;
    double t = 0.0;
    if (!chart_from_face(chart, z, phi_face, s, t)) continue;
    if (s < 0.0 || s > 1.0 || t < 0.0 || t > 1.0) continue;
    out.face = static_cast<U32>(chart.face);
    out.s = s;
    out.t = t;
    return true;
  }
  return false;
}

bool chart_partials(double z, double phi, double& ds_dz, double& ds_dphi, double& dt_dz,
                    double& dt_dphi) {
  const double two_thirds = 2.0 / 3.0;
  if (z > two_thirds) {
    const double r = std::sqrt(3.0 * (1.0 - z));
    if (!(r > 0.0)) return false;
    const double dr_dz = -3.0 / (2.0 * r);
    const double dd_dz = (4.0 * phi / kPi) * dr_dz;
    const double dd_dphi = 4.0 * r / kPi;
    ds_dz = (-dr_dz + dd_dz) / 2.0;
    ds_dphi = dd_dphi / 2.0;
    dt_dz = (-dr_dz - dd_dz) / 2.0;
    dt_dphi = -dd_dphi / 2.0;
    return true;
  }
  if (z < -two_thirds) {
    const double w = std::sqrt(3.0 * (1.0 + z));
    if (!(w > 0.0)) return false;
    const double dw_dz = 3.0 / (2.0 * w);
    const double dr_dz = -dw_dz;
    const double dd_dz = (4.0 * phi / kPi) * dw_dz;
    const double dd_dphi = 4.0 * w / kPi;
    ds_dz = (-dr_dz + dd_dz) / 2.0;
    ds_dphi = dd_dphi / 2.0;
    dt_dz = (-dr_dz - dd_dz) / 2.0;
    dt_dphi = -dd_dphi / 2.0;
    return true;
  }
  // The equatorial band.
  ds_dz = 0.75;
  ds_dphi = 2.0 / kPi;
  dt_dz = 0.75;
  dt_dphi = -2.0 / kPi;
  return true;
}

bool chart_partials_interval(const Iv& z, const Iv& phi, U32 face, Iv& ds_dz, Iv& ds_dphi, Iv& dt_dz,
                             Iv& dt_dphi) {
  (void)face;
  const double two_thirds = 2.0 / 3.0;
  const bool north = z.lower > two_thirds;
  const bool south = z.upper < -two_thirds;
  if (!north && !south) {
    // The band: the partials are constant.
    ds_dz = Iv(0.75, 0.75);
    ds_dphi = Iv(2.0 / kPi, 2.0 / kPi);
    dt_dz = Iv(0.75, 0.75);
    dt_dphi = Iv(-2.0 / kPi, -2.0 / kPi);
    return true;
  }
  if (north) {
    const Iv r = numerics::interval_sqrt(numerics::interval_mul(Iv(3.0, 3.0),
                                                               numerics::interval_sub(Iv(1.0, 1.0), z)));
    if (r.lower <= 0.0) return false;
    const Iv dr_dz = numerics::interval_div(Iv(-3.0, -3.0), numerics::interval_mul(Iv(2.0, 2.0), r));
    const Iv four_phi = numerics::interval_scale(phi, 4.0 / kPi);
    const Iv dd_dz = numerics::interval_mul(four_phi, dr_dz);
    const Iv dd_dphi = numerics::interval_scale(r, 4.0 / kPi);
    ds_dz = numerics::interval_div(numerics::interval_add(numerics::interval_neg(dr_dz), dd_dz), Iv(2.0, 2.0));
    ds_dphi = numerics::interval_div(dd_dphi, Iv(2.0, 2.0));
    dt_dz = numerics::interval_div(numerics::interval_add(numerics::interval_neg(dr_dz),
                                                          numerics::interval_neg(dd_dz)), Iv(2.0, 2.0));
    dt_dphi = numerics::interval_div(numerics::interval_neg(dd_dphi), Iv(2.0, 2.0));
    return true;
  }
  const Iv w = numerics::interval_sqrt(numerics::interval_mul(Iv(3.0, 3.0),
                                                             numerics::interval_add(Iv(1.0, 1.0), z)));
  if (w.lower <= 0.0) return false;
  const Iv dw_dz = numerics::interval_div(Iv(3.0, 3.0), numerics::interval_mul(Iv(2.0, 2.0), w));
  const Iv dr_dz = numerics::interval_neg(dw_dz);
  const Iv four_phi = numerics::interval_scale(phi, 4.0 / kPi);
  const Iv dd_dz = numerics::interval_mul(four_phi, dw_dz);
  const Iv dd_dphi = numerics::interval_scale(w, 4.0 / kPi);
  ds_dz = numerics::interval_div(numerics::interval_add(numerics::interval_neg(dr_dz), dd_dz), Iv(2.0, 2.0));
  ds_dphi = numerics::interval_div(dd_dphi, Iv(2.0, 2.0));
  dt_dz = numerics::interval_div(numerics::interval_add(numerics::interval_neg(dr_dz),
                                                        numerics::interval_neg(dd_dz)), Iv(2.0, 2.0));
  dt_dphi = numerics::interval_div(numerics::interval_neg(dd_dphi), Iv(2.0, 2.0));
  return true;
}

int chart_j(U32 face) { return kJ[face % 12]; }

double chart_face_longitude(double phi, U32 face) { return face_longitude(phi, kL[face % 12]); }
int chart_l(U32 face) { return kL[face % 12]; }

bool chart_interval(U32 face, const Iv& z, const Iv& phi, Iv& s, Iv& t) {
  const int j = chart_j(face);
  const int l = chart_l(face);
  const double two_thirds = 2.0 / 3.0;
  const Iv four_phi = numerics::interval_scale(phi, 4.0 / kPi);
  const Iv l_const(static_cast<double>(l), static_cast<double>(l));
  if (z.lower > two_thirds) {
    const Iv r = numerics::interval_sqrt(numerics::interval_mul(
        Iv(3.0, 3.0), numerics::interval_sub(Iv(1.0, 1.0), z)));
    if (r.lower <= 0.0) return false;
    const Iv d = numerics::interval_mul(r, numerics::interval_sub(four_phi, l_const));
    s = numerics::interval_div(numerics::interval_add(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
    t = numerics::interval_div(numerics::interval_sub(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
    return true;
  }
  if (z.upper < -two_thirds) {
    const Iv w = numerics::interval_sqrt(numerics::interval_mul(
        Iv(3.0, 3.0), numerics::interval_add(Iv(1.0, 1.0), z)));
    if (w.lower <= 0.0) return false;
    const Iv r = numerics::interval_sub(Iv(4.0, 4.0), w);
    const Iv d = numerics::interval_mul(numerics::interval_sub(four_phi, l_const), w);
    s = numerics::interval_div(numerics::interval_add(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
    t = numerics::interval_div(numerics::interval_sub(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
    return true;
  }
  const Iv r = numerics::interval_sub(Iv(2.0, 2.0), numerics::interval_scale(z, 1.5));
  const Iv d = numerics::interval_sub(four_phi, l_const);
  s = numerics::interval_div(numerics::interval_add(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
  t = numerics::interval_div(numerics::interval_sub(numerics::interval_sub(Iv(j, j), r), d), Iv(2.0, 2.0));
  return true;
}

HealpixLeaf chart_leaf(U32 order, U32 face, U64 ix, U64 iy) {
  const U64 nside = nside_for(order);
  const U64 cx = std::min<U64>(nside - 1, ix);
  const U64 cy = std::min<U64>(nside - 1, iy);
  HealpixLeaf leaf;
  leaf.order = order;
  leaf.ipix = static_cast<U64>(face) * nside * nside + healpix_face_index(order, cx, cy);
  return leaf;
}

bool chart_leaf_for_direction(U32 order, const SkyICRS& direction, HealpixLeaf& leaf) {
  ChartPoint point;
  if (!chart_inverse(direction, point)) return false;
  const U64 nside = nside_for(order);
  U64 ix = static_cast<U64>(std::floor(point.s * static_cast<double>(nside)));
  U64 iy = static_cast<U64>(std::floor(point.t * static_cast<double>(nside)));
  if (ix >= nside) ix = nside - 1;
  if (iy >= nside) iy = nside - 1;
  leaf = chart_leaf(order, point.face, ix, iy);
  return true;
}

}  // namespace acsd::coordinates
