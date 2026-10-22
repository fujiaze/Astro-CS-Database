// Sky geometry and epoch propagation for the offline Gaia catalogue.
//
// The epoch propagation is implemented here from the scientific formulas
// (docs/science/foundation/OBSERVATION_COORDINATES.md §2) because ERFA is not
// available on this machine.  The implementation is checked against astropy in
// validation/cat.
#include <algorithm>
#include <cmath>
#include <limits>

#include "catalogue/catalogue_internal.hpp"

namespace acsd::catalogue {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kAuMetres = 1.495978707e11;
constexpr double kSecondsPerJulianYear = 31557600.0;  // 365.25 d

// One astronomical unit expressed in au per year for a speed in m/s.
constexpr double kAuPerYearPerMetrePerSecond = kSecondsPerJulianYear / kAuMetres;

Vec3 cross3(const Vec3& a, const Vec3& b) {
  return Vec3{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot3(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

double norm3(const Vec3& a) { return std::sqrt(dot3(a, a)); }

Vec3 normalize3(const Vec3& a) {
  const double n = norm3(a);
  if (!(n > 0.0)) return Vec3{0.0, 0.0, 0.0};
  return Vec3{a[0] / n, a[1] / n, a[2] / n};
}

}  // namespace

Vec3 unit_from_ra_dec(double ra_rad, double dec_rad) {
  const double cd = std::cos(dec_rad);
  return Vec3{cd * std::cos(ra_rad), cd * std::sin(ra_rad), std::sin(dec_rad)};
}

void ra_dec_from_unit(const Vec3& unit, double& ra_rad, double& dec_rad) {
  const Vec3 n = normalize3(unit);
  dec_rad = std::asin(std::clamp(n[2], -1.0, 1.0));
  ra_rad = std::atan2(n[1], n[0]);
  if (ra_rad < 0.0) ra_rad += 2.0 * kPi;
}

double angular_separation(const Vec3& a, const Vec3& b) {
  const Vec3 na = normalize3(a);
  const Vec3 nb = normalize3(b);
  const double s = norm3(cross3(na, nb));
  const double c = std::clamp(dot3(na, nb), -1.0, 1.0);
  return std::atan2(s, c);
}

void local_basis(const Vec3& centre, Vec3& east, Vec3& north) {
  const Vec3 c = normalize3(centre);
  double ra = 0.0;
  double dec = 0.0;
  ra_dec_from_unit(c, ra, dec);
  east = Vec3{-std::sin(ra), std::cos(ra), 0.0};
  north = Vec3{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
}

Vec2 tangent_project(const Vec3& centre, const Vec3& east, const Vec3& north, const Vec3& target) {
  const double dc = dot3(centre, target);
  if (std::abs(dc) < 1e-12) return Vec2{0.0, 0.0};
  return Vec2{dot3(east, target) / dc, dot3(north, target) / dc};
}

Vec3 tangent_unproject(const Vec3& centre, const Vec3& east, const Vec3& north, const Vec2& xy) {
  const Vec3 raw{centre[0] + xy[0] * east[0] + xy[1] * north[0],
                 centre[1] + xy[0] * east[1] + xy[1] * north[1],
                 centre[2] + xy[0] * east[2] + xy[1] * north[2]};
  return normalize3(raw);
}

PropagatedPosition propagate_star(const StarRow& star, double target_epoch_year) {
  PropagatedPosition out;
  const double dt = target_epoch_year - star.epoch_year;

  const Vec3 n0 = unit_from_ra_dec(star.ra_rad, star.dec_rad);
  Vec3 east{};
  Vec3 north{};
  local_basis(n0, east, north);

  // Tangential proper motion (rad/yr) with the pmRA*cosDec convention.
  const Vec3 mu{star.pmra_cosdec_rad_year * east[0] + star.pmdec_rad_year * north[0],
                star.pmra_cosdec_rad_year * east[1] + star.pmdec_rad_year * north[1],
                star.pmra_cosdec_rad_year * east[2] + star.pmdec_rad_year * north[2]};
  const double mu_norm = norm3(mu);

  const bool have_distance = star.parallax_rad > 0.0;
  Vec3 direction = n0;
  double parallax = star.parallax_rad;

  if (have_distance) {
    const double distance = 1.0 / star.parallax_rad;  // au
    // Linear space motion: r(t) = r0 + v dt.  The tangential part uses the
    // proper motion scaled by the distance, the radial part the known RV.
    Vec3 velocity = Vec3{distance * mu[0], distance * mu[1], distance * mu[2]};
    if (star.rv_present) {
      const double radial_au_per_year = star.rv_m_s * kAuPerYearPerMetrePerSecond;
      velocity = Vec3{velocity[0] + radial_au_per_year * n0[0], velocity[1] + radial_au_per_year * n0[1],
                      velocity[2] + radial_au_per_year * n0[2]};
    }
    const Vec3 r1{star.parallax_rad * 0.0 + distance * n0[0] + velocity[0] * dt,
                  distance * n0[1] + velocity[1] * dt, distance * n0[2] + velocity[2] * dt};
    const double r1_norm = norm3(r1);
    if (r1_norm > 0.0) {
      direction = normalize3(r1);
      parallax = 1.0 / r1_norm;
    }
  } else if (mu_norm > 0.0) {
    // Unknown distance: the exact great circle motion, which is the infinite
    // distance limit of the expression above.
    const Vec3 tangent = normalize3(Vec3{mu[0] - dot3(mu, n0) * n0[0], mu[1] - dot3(mu, n0) * n0[1],
                                          mu[2] - dot3(mu, n0) * n0[2]});
    const double theta = mu_norm * dt;
    const double c = std::cos(theta);
    const double s = std::sin(theta);
    direction = normalize3(Vec3{n0[0] * c + tangent[0] * s, n0[1] * c + tangent[1] * s,
                                n0[2] * c + tangent[2] * s});
  }

  out.unit = direction;
  out.parallax_rad = parallax;
  ra_dec_from_unit(direction, out.ra_rad, out.dec_rad);
  return out;
}

bool topocentric_direction(const PropagatedPosition& propagated, const ObserverState& observer,
                           Vec3& out_unit, double& parallax_rad) {
  out_unit = propagated.unit;
  parallax_rad = propagated.parallax_rad;
  if (!observer.barycentric_available) return false;
  if (!(propagated.parallax_rad > 0.0)) return false;

  const double distance = 1.0 / propagated.parallax_rad;  // au
  const Vec3 star{distance * propagated.unit[0], distance * propagated.unit[1],
                  distance * propagated.unit[2]};
  const Vec3 relative{star[0] - observer.barycentric_au[0], star[1] - observer.barycentric_au[1],
                      star[2] - observer.barycentric_au[2]};
  const double length = norm3(relative);
  if (!(length > 0.0)) return false;
  out_unit = Vec3{relative[0] / length, relative[1] / length, relative[2] / length};
  parallax_rad = 1.0 / length;
  return true;
}

double observer_omission_bound_rad(const StarRow& star) {
  if (!(star.parallax_rad > 0.0)) return 0.0;
  // The observer barycentric distance is at most about 1.01 au; the angular
  // error of an uncorrected barycentric direction is bounded by the parallax.
  const double bound = std::asin(std::min(1.0, 1.01 * star.parallax_rad));
  return std::isfinite(bound) ? bound : 0.0;
}

namespace {

// Lexicographic comparison of two four vectors.
bool less4(const std::array<double, 4>& a, const std::array<double, 4>& b) {
  for (std::size_t i = 0; i < 4; ++i) {
    if (a[i] < b[i]) return true;
    if (a[i] > b[i]) return false;
  }
  return false;
}

// The four variants generated by the A/B swap and the C/D swap, expressed on
// the base four vector (x_C, y_C, x_D, y_D).
struct Variant {
  std::array<double, 4> value;
  std::array<int, 4> roles;  // role -> index of the input point
};

std::array<Variant, 4> enumerate_variants(const std::array<double, 4>& base,
                                           const std::array<int, 4>& base_roles) {
  std::array<Variant, 4> out{};
  // 0: identity, 1: C/D swap, 2: A/B swap, 3: A/B swap and C/D swap.
  const std::array<double, 4> identity{base};
  const std::array<double, 4> swap_cd{base[2], base[3], base[0], base[1]};
  const std::array<double, 4> swap_ab{1.0 - base[0], -base[1], 1.0 - base[2], -base[3]};
  const std::array<double, 4> swap_both{1.0 - base[2], -base[3], 1.0 - base[0], -base[1]};

  out[0].value = identity;
  out[0].roles = base_roles;
  out[1].value = swap_cd;
  out[1].roles = std::array<int, 4>{base_roles[0], base_roles[1], base_roles[3], base_roles[2]};
  out[2].value = swap_ab;
  out[2].roles = std::array<int, 4>{base_roles[1], base_roles[0], base_roles[2], base_roles[3]};
  out[3].value = swap_both;
  out[3].roles = std::array<int, 4>{base_roles[1], base_roles[0], base_roles[3], base_roles[2]};
  return out;
}

}  // namespace

QuadDescriptor describe_quad(const std::array<Vec2, 4>& points, const std::array<U64, 4>& ids) {
  QuadDescriptor out;

  // Longest baseline; the length tie is broken by the source identifier.
  int best_a = 0;
  int best_b = 1;
  double best_length = -1.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const double dx = points[j][0] - points[i][0];
      const double dy = points[j][1] - points[i][1];
      const double length = std::sqrt(dx * dx + dy * dy);
      bool better = false;
      if (length > best_length) {
        better = true;
      } else if (length == best_length && best_length >= 0.0) {
        const U64 low = std::min(ids[i], ids[j]);
        const U64 high = std::max(ids[i], ids[j]);
        const U64 best_low = std::min(ids[best_a], ids[best_b]);
        const U64 best_high = std::max(ids[best_a], ids[best_b]);
        better = (low < best_low) || (low == best_low && high < best_high);
      }
      if (better) {
        best_length = length;
        best_a = i;
        best_b = j;
      }
    }
  }
  const double length = best_length;
  out.baseline = length;

  // The remaining two points, ordered by source identifier.
  int remaining[2] = {0, 0};
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == best_a || i == best_b) continue;
    remaining[count++] = i;
  }
  if (ids[remaining[0]] > ids[remaining[1]]) std::swap(remaining[0], remaining[1]);
  const int c = remaining[0];
  const int d = remaining[1];

  const double ex = (points[best_b][0] - points[best_a][0]) / length;
  const double ey = (points[best_b][1] - points[best_a][1]) / length;
  // Counter clockwise perpendicular.
  const double px = -ey;
  const double py = ex;

  const auto project = [&](int index, double& x, double& y) {
    const double dx = points[index][0] - points[best_a][0];
    const double dy = points[index][1] - points[best_a][1];
    x = (dx * ex + dy * ey) / length;
    y = (dx * px + dy * py) / length;
  };

  double cx = 0.0;
  double cy = 0.0;
  double dx_c = 0.0;
  double dy_c = 0.0;
  project(c, cx, cy);
  project(d, dx_c, dy_c);

  const std::array<int, 4> base_roles{best_a, best_b, c, d};
  const std::array<double, 4> base_plus{cx, cy, dx_c, dy_c};
  const std::array<double, 4> base_minus{cx, -cy, dx_c, -dy_c};

  const auto best_variant = [&](const std::array<double, 4>& base, Variant& chosen) {
    const std::array<Variant, 4> variants = enumerate_variants(base, base_roles);
    int best = 0;
    for (int i = 1; i < 4; ++i) {
      if (less4(variants[i].value, variants[best].value)) best = i;
    }
    chosen = variants[best];
  };

  Variant plus{};
  Variant minus{};
  best_variant(base_plus, plus);
  best_variant(base_minus, minus);
  out.chirality_plus = plus.value;
  out.roles_plus = plus.roles;
  out.chirality_minus = minus.value;
  out.roles_minus = minus.roles;
  out.canonical = less4(out.chirality_plus, out.chirality_minus) ? out.chirality_plus
                                                                  : out.chirality_minus;
  return out;
}

std::array<double, 4> canonical_descriptor(const std::array<Vec2, 4>& points,
                                           const std::array<U64, 4>& ids) {
  return describe_quad(points, ids).canonical;
}

}  // namespace acsd::catalogue
