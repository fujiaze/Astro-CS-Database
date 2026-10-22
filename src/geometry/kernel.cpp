#include "geometry/kernel.hpp"

#include "geometry/profile.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <utility>

#include "coordinates/chart.hpp"
#include "coordinates/tan_sip.hpp"
#include "geometry/exact_polygon.hpp"
#include "model/units.hpp"

namespace acsd::geometry {
namespace {

using Iv = numerics::Interval;

constexpr U64 kMaxCandidatesPerBlock = 4096;
constexpr double kPi = 3.14159265358979323846;

double abs_max(const Iv& value) {
  return std::max(std::fabs(value.lower), std::fabs(value.upper));
}

// Exact floor and ceiling of a rational.
Rational floor_rational(const Rational& value) {
  if (value.is_zero()) return value;
  BigInt quotient = value.numerator() / value.denominator();
  const BigInt remainder = value.numerator() % value.denominator();
  if (value.sign() < 0 && !remainder.is_zero()) quotient = quotient - BigInt(1);
  return Rational::from_bigint(quotient, BigInt(1));
}

Rational ceil_rational(const Rational& value) {
  if (value.is_zero()) return value;
  BigInt quotient = value.numerator() / value.denominator();
  const BigInt remainder = value.numerator() % value.denominator();
  if (value.sign() > 0 && !remainder.is_zero()) quotient = quotient + BigInt(1);
  return Rational::from_bigint(quotient, BigInt(1));
}

// floor(value * 2^bits) as an exact integer.
BigInt scaled_floor(const Rational& value, int bits) {
  BigInt numerator = value.numerator();
  const BigInt denominator = value.denominator();
  if (bits > 0) numerator = numerator << static_cast<unsigned>(bits);
  BigInt quotient = numerator / denominator;
  const BigInt remainder = numerator % denominator;
  if (value.sign() < 0 && !remainder.is_zero()) quotient = quotient - BigInt(1);
  return quotient;
}

BigInt scaled_ceil(const Rational& value, int bits) {
  BigInt numerator = value.numerator();
  const BigInt denominator = value.denominator();
  if (bits > 0) numerator = numerator << static_cast<unsigned>(bits);
  BigInt quotient = numerator / denominator;
  const BigInt remainder = numerator % denominator;
  if (value.sign() > 0 && !remainder.is_zero()) quotient = quotient + BigInt(1);
  return quotient;
}

// The maximum deviation of an interval from a representative value.
double deviation(const Iv& value, double representative) {
  return std::max(std::fabs(value.lower - representative), std::fabs(value.upper - representative));
}

// ---------------------------------------------------------------------------
// Block model: the affine model of the chart coordinates and of the solid
// angle density over one detector sub rectangle.
// ---------------------------------------------------------------------------

struct BlockModel {
  U32 face = 0;
  double cu = 0.0;
  double cv = 0.0;
  double s0 = 0.0;
  double t0 = 0.0;
  double gs_u = 0.0;
  double gs_v = 0.0;
  double gt_u = 0.0;
  double gt_v = 0.0;
  double e_s = 0.0;
  double e_t = 0.0;
  double s_lo = 0.0;
  double s_hi = 0.0;
  double t_lo = 0.0;
  double t_hi = 0.0;
  double g0 = 0.0;
  double gu = 0.0;
  double gv = 0.0;
  double e_g = 0.0;
};

struct Block {
  double u0 = 0.0;
  double u1 = 0.0;
  double v0 = 0.0;
  double v1 = 0.0;
  U32 depth = 0;
};

SkyICRS forward_point(const WcsModel& wcs, double u, double v) {
  const Vec2 q = coordinates::tangent_offset(wcs, u, v);
  return coordinates::direction_from_tangent(wcs, q);
}

bool build_block_model(const WcsModel& wcs, const Block& block, BlockModel& model,
                       bool& outcome_reason_apex) {
  ++geometry::kernel_counts().chart_models;
  model.cu = 0.5 * (block.u0 + block.u1);
  model.cv = 0.5 * (block.v0 + block.v1);
  const double hu = 0.5 * (block.u1 - block.u0);
  const double hv = 0.5 * (block.v1 - block.v0);

  const SkyICRS centre = forward_point(wcs, model.cu, model.cv);
  coordinates::ChartPoint chart;
  if (!coordinates::chart_inverse(centre, chart)) return false;
  model.face = chart.face;
  model.s0 = chart.s;
  model.t0 = chart.t;

  const Iv u(block.u0, block.u1);
  const Iv v(block.v0, block.v1);
  Iv nx;
  Iv ny;
  Iv nz;
  coordinates::forward_interval_iv(wcs, u, v, nx, ny, nz);
  if (!nx.is_finite() || !ny.is_finite() || !nz.is_finite()) return false;

  // The angular radius of the block on the sky, from the enclosure of J_W.
  Iv jw[3][2];
  coordinates::jacobian_interval_iv(wcs, u, v, jw);
  double dn_max = 0.0;
  for (int col = 0; col < 2; ++col) {
    double magnitude = 0.0;
    for (int row = 0; row < 3; ++row) magnitude += abs_max(jw[row][col]);
    dn_max = std::max(dn_max, magnitude * (col == 0 ? hu : hv));
  }
  const double angle_bound = dn_max * (1.0 + 1.0e-9) + 1.0e-12;

  // The chart coordinates of a face are defined with respect to the longitude
  // of that face, so the chart API must be fed the face relative longitude
  // (the same branch that chart_inverse uses), never the absolute one.
  const double phi_absolute = std::atan2(centre.ny, centre.nx);
  const double phi_centre = coordinates::chart_face_longitude(phi_absolute, model.face);
  const double branch_centre = static_cast<double>(coordinates::chart_l(model.face)) * kPi / 4.0;
  const double branch_half = kPi / 4.0;
  const Iv phi(phi_centre - angle_bound, phi_centre + angle_bound);
  const Iv z = nz;
  // The block must stay inside the branch of the face; otherwise the chart
  // case is ambiguous and the block has to be refined.
  if (phi.lower < branch_centre - branch_half || phi.upper > branch_centre + branch_half) {
    return false;
  }

  // The chart case must be unambiguous over the whole block.
  const double two_thirds = 2.0 / 3.0;
  const bool pure_band = z.lower >= -two_thirds && z.upper <= two_thirds;
  const bool pure_north = z.lower > two_thirds;
  const bool pure_south = z.upper < -two_thirds;

  Iv s;
  Iv t;
  if (!coordinates::chart_interval(model.face, z, phi, s, t)) return false;
  if (!s.is_finite() || !t.is_finite()) return false;
  model.s_lo = s.lower;
  model.s_hi = s.upper;
  model.t_lo = t.lower;
  model.t_hi = t.upper;

  // The analytic gradient of the chart at the centre.
  double ds_dz = 0.0;
  double ds_dphi = 0.0;
  double dt_dz = 0.0;
  double dt_dphi = 0.0;
  if (!coordinates::chart_partials(centre.nz, phi_centre, ds_dz, ds_dphi, dt_dz, dt_dphi)) { return false; }
  double jacobian_matrix[3][2];
  coordinates::jacobian(wcs, model.cu, model.cv, jacobian_matrix);
  const double nxy2 = centre.nx * centre.nx + centre.ny * centre.ny;
  if (!(nxy2 > 0.0)) return false;
  const double dphi_du = (-centre.ny * jacobian_matrix[0][0] + centre.nx * jacobian_matrix[1][0]) / nxy2;
  const double dphi_dv = (-centre.ny * jacobian_matrix[0][1] + centre.nx * jacobian_matrix[1][1]) / nxy2;
  const double dz_du = jacobian_matrix[2][0];
  const double dz_dv = jacobian_matrix[2][1];
  model.gs_u = ds_dz * dz_du + ds_dphi * dphi_du;
  model.gs_v = ds_dz * dz_dv + ds_dphi * dphi_dv;
  model.gt_u = dt_dz * dz_du + dt_dphi * dphi_du;
  model.gt_v = dt_dz * dz_dv + dt_dphi * dphi_dv;

  // The residual bound of the affine chart model (SPHERICAL_DRIZZLE Eq. 16):
  //   |e| <= mag(f(c) - f0) + sum_k mag(d_k f(U) - J0_k) * h_k
  // With f0 = f(c) only the second order remainder survives.
  Iv ds_dz_iv;
  Iv ds_dphi_iv;
  Iv dt_dz_iv;
  Iv dt_dphi_iv;
  if (!coordinates::chart_partials_interval(z, phi, model.face, ds_dz_iv, ds_dphi_iv, dt_dz_iv,
                                            dt_dphi_iv)) {
    // The block touches the apex of a polar face: the chart derivative of the
    // face is singular there, so the affine cut cell model cannot be built.
    // The reason is named, never silent.
    outcome_reason_apex = true;
    return false;
  }
  const Iv nxy2_iv = numerics::interval_add(numerics::interval_mul(nx, nx),
                                            numerics::interval_mul(ny, ny));
  if (nxy2_iv.lower <= 0.0) return false;
  const Iv dphi_du_iv =
      numerics::interval_div(numerics::interval_add(numerics::interval_mul(numerics::interval_neg(ny),
                                                                           jw[0][0]),
                                                   numerics::interval_mul(nx, jw[1][0])),
                            nxy2_iv);
  const Iv dphi_dv_iv =
      numerics::interval_div(numerics::interval_add(numerics::interval_mul(numerics::interval_neg(ny),
                                                                           jw[0][1]),
                                                   numerics::interval_mul(nx, jw[1][1])),
                            nxy2_iv);
  const Iv ds_du_iv = numerics::interval_add(numerics::interval_mul(ds_dz_iv, jw[2][0]),
                                             numerics::interval_mul(ds_dphi_iv, dphi_du_iv));
  const Iv ds_dv_iv = numerics::interval_add(numerics::interval_mul(ds_dz_iv, jw[2][1]),
                                             numerics::interval_mul(ds_dphi_iv, dphi_dv_iv));
  const Iv dt_du_iv = numerics::interval_add(numerics::interval_mul(dt_dz_iv, jw[2][0]),
                                             numerics::interval_mul(dt_dphi_iv, dphi_du_iv));
  const Iv dt_dv_iv = numerics::interval_add(numerics::interval_mul(dt_dz_iv, jw[2][1]),
                                             numerics::interval_mul(dt_dphi_iv, dphi_dv_iv));
  model.e_s = deviation(ds_du_iv, model.gs_u) * hu + deviation(ds_dv_iv, model.gs_v) * hv;
  model.e_t = deviation(dt_du_iv, model.gt_u) * hu + deviation(dt_dv_iv, model.gt_v) * hv;

  // The solid angle density model.
  double g = 0.0;
  double dgu = 0.0;
  double dgv = 0.0;
  coordinates::solid_angle_density_gradient(wcs, model.cu, model.cv, g, dgu, dgv);
  model.g0 = g;
  model.gu = dgu;
  model.gv = dgv;
  const Iv g_range = coordinates::solid_angle_density_iv(wcs, u, v);
  if (!g_range.is_finite()) return false;
  const double g_span = std::fabs(model.gu) * hu + std::fabs(model.gv) * hv;
  model.e_g = std::max(std::fabs(g_range.lower - (g + g_span)), std::fabs(g_range.upper - (g - g_span)));
  if (model.e_g < 0.0) model.e_g = 0.0;
  return true;
}

// Exact integration of the affine density model over a polygon.
void integrate_affine(const RationalPolygon& polygon, const BlockModel& model, Rational& lower,
                      Rational& upper) {
  lower = Rational(0, 1);
  upper = Rational(0, 1);
  if (polygon.size() < 3) return;
  const Rational area = polygon_signed_area(polygon);
  const Rational moment_u = polygon_moment_x(polygon);
  const Rational moment_v = polygon_moment_y(polygon);
  const Rational cu = Rational::from_double_exact(model.cu);
  const Rational cv = Rational::from_double_exact(model.cv);
  const Rational g0 = Rational::from_double_exact(model.g0);
  const Rational gu = Rational::from_double_exact(model.gu);
  const Rational gv = Rational::from_double_exact(model.gv);
  const Rational eg = Rational::from_double_exact(model.e_g);
  const Rational centre = g0 * area + gu * (moment_u - cu * area) + gv * (moment_v - cv * area);
  lower = centre - eg * area;
  upper = centre + eg * area;
  if (lower.sign() < 0) lower = Rational(0, 1);
  if (upper.sign() < 0) upper = Rational(0, 1);
}

struct HalfSpace {
  Rational a;
  Rational b;
  Rational c;
};

// The affine chart model satisfies  s_model(u,v) = (gs_u u + gs_v v) + k.
HalfSpace chart_half_space_le(const BlockModel& model, bool s_axis, double bound) {
  const double gu = s_axis ? model.gs_u : model.gt_u;
  const double gv = s_axis ? model.gs_v : model.gt_v;
  const double value = s_axis ? model.s0 : model.t0;
  HalfSpace space;
  space.a = Rational::from_double_exact(gu);
  space.b = Rational::from_double_exact(gv);
  // s_model(u,v) = gu*u + gv*v + (value - gu*cu - gv*cv), so
  // s_model <= bound  <=>  gu*u + gv*v <= bound - value + gu*cu + gv*cv.
  const double offset = bound - value + gu * model.cu + gv * model.cv;
  space.c = Rational::from_double_exact(offset);
  return space;
}

HalfSpace chart_half_space_ge(const BlockModel& model, bool s_axis, double bound) {
  HalfSpace space = chart_half_space_le(model, s_axis, bound);
  space.a = -space.a;
  space.b = -space.b;
  space.c = -space.c;
  return space;
}

RationalPolygon rectangle_polygon(const Block& block) {
  RationalPolygon polygon;
  polygon.push_back({Rational::from_double_exact(block.u0), Rational::from_double_exact(block.v0)});
  polygon.push_back({Rational::from_double_exact(block.u1), Rational::from_double_exact(block.v0)});
  polygon.push_back({Rational::from_double_exact(block.u1), Rational::from_double_exact(block.v1)});
  polygon.push_back({Rational::from_double_exact(block.u0), Rational::from_double_exact(block.v1)});
  return polygon;
}

}  // namespace

// ---------------------------------------------------------------------------
// The dyadic ledger
// ---------------------------------------------------------------------------

void DyadicLedger::add(const Rational& value, bool upper) {
  if (value.is_zero()) return;
  const BigInt scaled = upper ? scaled_ceil(value, kScaleBits) : scaled_floor(value, kScaleBits);
  if (empty_) {
    sum_ = scaled;
    empty_ = false;
    return;
  }
  sum_ = sum_ + scaled;
}

void DyadicLedger::add(const Dyadic& value, bool upper) {
  add(Rational::from_double_exact(dyadic_to_double(value)), upper);
}

Dyadic DyadicLedger::dyadic() const {
  Dyadic out;
  out.signed_integer = sum_.to_string();
  out.exponent2 = -kScaleBits;
  return out;
}

double DyadicLedger::to_double() const {
  if (empty_) return 0.0;
  const auto scaled = sum_.to_double();
  if (!scaled) return 0.0;
  return std::ldexp(*scaled, -kScaleBits);
}

Dyadic rational_to_dyadic(const Rational& value, bool upper) {
  Dyadic out;
  const BigInt scaled = upper ? scaled_ceil(value, DyadicLedger::kScaleBits)
                               : scaled_floor(value, DyadicLedger::kScaleBits);
  out.signed_integer = scaled.to_string();
  out.exponent2 = -DyadicLedger::kScaleBits;
  return out;
}

namespace {

// The integer value of a dyadic, scaled to the common exponent.
BigInt scaled_to(const Dyadic& value, int exponent) {
  BigInt integer = BigInt::from_string(value.signed_integer);
  const int difference = exponent - value.exponent2;
  if (difference > 0) integer = integer << static_cast<unsigned>(difference);
  return integer;
}

}  // namespace

Dyadic dyadic_max(const Dyadic& a, const Dyadic& b) {
  const int exponent = std::min(a.exponent2, b.exponent2);
  const BigInt left = scaled_to(a, exponent);
  const BigInt right = scaled_to(b, exponent);
  return compare(left, right) >= 0 ? a : b;
}

Dyadic dyadic_min(const Dyadic& a, const Dyadic& b) {
  const int exponent = std::min(a.exponent2, b.exponent2);
  const BigInt left = scaled_to(a, exponent);
  const BigInt right = scaled_to(b, exponent);
  return compare(left, right) <= 0 ? a : b;
}

double dyadic_to_double(const Dyadic& value) {
  double result = 0.0;
  const std::string& text = value.signed_integer;
  std::size_t index = 0;
  bool negative = false;
  if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
    negative = text[index] == '-';
    ++index;
  }
  for (; index < text.size(); ++index) {
    if (text[index] < '0' || text[index] > '9') break;
    result = result * 10.0 + static_cast<double>(text[index] - '0');
  }
  if (negative) result = -result;
  return std::ldexp(result, value.exponent2);
}

// ---------------------------------------------------------------------------
// The chart enclosure of a detector box
// ---------------------------------------------------------------------------

bool chart_enclosure(const WcsModel& wcs, double u0, double u1, double v0, double v1,
                     ChartBox& out) {
  Block block;
  block.u0 = u0;
  block.u1 = u1;
  block.v0 = v0;
  block.v1 = v1;
  BlockModel model;
  bool apex = false;
  if (!build_block_model(wcs, block, model, apex)) return false;
  out.valid = true;
  out.face = model.face;
  out.s_lo = model.s_lo;
  out.s_hi = model.s_hi;
  out.t_lo = model.t_lo;
  out.t_hi = model.t_hi;
  out.e_s = model.e_s;
  out.e_t = model.e_t;
  return true;
}

// ---------------------------------------------------------------------------
// The cut cell kernel
// ---------------------------------------------------------------------------

KernelOutcome cut_cell_overlap(Context& context, const WcsModel& wcs, const Box& detector_rect,
                               U32 order, const GeometryBudget& budget) {
  KernelOutcome outcome;
  const U64 nside = 1ull << order;
  const double inv_nside = 1.0 / static_cast<double>(nside);
  const double crpix_u = wcs.crpix_fits1[0];
  const double crpix_v = wcs.crpix_fits1[1];

  std::deque<Block> queue;
  Block root;
  root.u0 = detector_rect.lower[0] + 1.0 - crpix_u;
  root.u1 = detector_rect.upper[0] + 1.0 - crpix_u;
  root.v0 = detector_rect.lower[1] + 1.0 - crpix_v;
  root.v1 = detector_rect.upper[1] + 1.0 - crpix_v;
  root.depth = 0;
  queue.push_back(root);

  struct LeafLedger {
    DyadicLedger lower;
    DyadicLedger upper;
  };
  std::map<std::pair<U32, U64>, LeafLedger> ledgers;
  DyadicLedger total_lower;
  DyadicLedger total_upper;
  DyadicLedger unresolved_upper;

  const U32 max_depth = std::max<U32>(1, budget.max_depth);
  const U64 max_work = budget.max_work > 0 ? budget.max_work : 1000000ull;
  U64 cells_visited = 0;
  U64 cells_pending = 0;

  auto keep_unresolved = [&](const Block& block) {
    const Iv u(block.u0, block.u1);
    const Iv v(block.v0, block.v1);
    const Iv g_range = coordinates::solid_angle_density_iv(wcs, u, v);
    const double width = (block.u1 - block.u0) * (block.v1 - block.v0);
    const double mass = g_range.is_finite() ? std::max(0.0, g_range.upper) * width : 0.0;
    UnresolvedDomain domain;
    domain.detector = Box{{block.u0 + crpix_u - 1.0, block.v0 + crpix_v - 1.0},
                          {block.u1 + crpix_u - 1.0, block.v1 + crpix_v - 1.0}};
    domain.mass_sr = model::exact_from_interval(Interval(0.0, mass));
    unresolved_upper.add(Rational::from_double_exact(mass), true);
    outcome.unresolved.push_back(domain);
  };

  while (!queue.empty()) {
    if (outcome.work >= max_work) {
      outcome.termination = Termination::work;
      outcome.reason = "work_budget_exhausted";
      break;
    }
    // The cancellation checkpoint of the block loop: the queue is drained at
    // a safety boundary, so a cancellation stops the kernel with a named
    // result and the undecided mass of the remaining blocks is kept.
    if (context.cancellation.requested()) {
      for (const Block& pending : queue) keep_unresolved(pending);
      queue.clear();
      outcome.termination = Termination::unsupported;
      outcome.reason = "cancelled";
      break;
    }
    const Block block = queue.front();
    queue.pop_front();
    ++outcome.work;

    // The shared work ledger: one unit for the block before it is processed
    // and one unit for every candidate cell after it.  The science budget and
    // the shared ledger are the same limit, so a caller never sees one
    // exhausted while the other still reports budget.
    {
      Result<U64> charged;
      {
        geometry::StageTimer work_timer(geometry::kernel_timings().work_metering_ns);
        charged = context.resources.consume_work(1 + cells_pending);
      }
      ++geometry::kernel_counts().work_charges;
      geometry::kernel_counts().work_units += 1 + cells_pending;
      cells_pending = 0;
      if (charged.code != Code::ok || outcome.work > max_work) {
        // The remaining blocks stay undecided and keep their mass.
        for (const Block& pending : queue) keep_unresolved(pending);
        queue.clear();
        outcome.termination = Termination::work;
        outcome.reason = "shared_work_budget_exhausted";
        break;
      }
    }
    BlockModel model;
    bool apex = false;
    bool modelled = false;
    {
      geometry::StageTimer timer(geometry::kernel_timings().chart_model_ns);
      modelled = build_block_model(wcs, block, model, apex);
    }
    if (!modelled) {
      if (apex && block.depth + 1 >= max_depth) {
        outcome.termination = Termination::unsupported;
        outcome.reason = "chart_apex_singular";
      }
      if (block.depth + 1 < max_depth) {
        const double cu = 0.5 * (block.u0 + block.u1);
        const double cv = 0.5 * (block.v0 + block.v1);
        queue.push_back(Block{block.u0, cu, block.v0, cv, block.depth + 1});
        queue.push_back(Block{cu, block.u1, block.v0, cv, block.depth + 1});
        queue.push_back(Block{block.u0, cu, cv, block.v1, block.depth + 1});
        queue.push_back(Block{cu, block.u1, cv, block.v1, block.depth + 1});
        continue;
      }
      keep_unresolved(block);
      continue;
    }

    ++geometry::kernel_counts().blocks;
    // The candidate cells of the block.
    long long ix_lo = 0;
    long long ix_hi = 0;
    long long iy_lo = 0;
    long long iy_hi = 0;
    {
      geometry::StageTimer enumeration_timer(geometry::kernel_timings().candidate_enumeration_ns);
      ix_lo = static_cast<long long>(std::floor(model.s_lo * static_cast<double>(nside)));
      ix_hi = static_cast<long long>(std::floor(model.s_hi * static_cast<double>(nside)));
      iy_lo = static_cast<long long>(std::floor(model.t_lo * static_cast<double>(nside)));
      iy_hi = static_cast<long long>(std::floor(model.t_hi * static_cast<double>(nside)));
    }
    const double cells = static_cast<double>(ix_hi - ix_lo + 1) * static_cast<double>(iy_hi - iy_lo + 1);
    if (cells > static_cast<double>(kMaxCandidatesPerBlock)) {
      if (block.depth + 1 < max_depth) {
        const double cu = 0.5 * (block.u0 + block.u1);
        const double cv = 0.5 * (block.v0 + block.v1);
        queue.push_back(Block{block.u0, cu, block.v0, cv, block.depth + 1});
        queue.push_back(Block{cu, block.u1, block.v0, cv, block.depth + 1});
        queue.push_back(Block{block.u0, cu, cv, block.v1, block.depth + 1});
        queue.push_back(Block{cu, block.u1, cv, block.v1, block.depth + 1});
        continue;
      }
      keep_unresolved(block);
      continue;
    }

    const RationalPolygon rectangle = rectangle_polygon(block);
    geometry::StageTimer clip_timer(geometry::kernel_timings().rational_clip_ns);
    bool cancelled_in_cells = false;
    for (long long iy = iy_lo; iy <= iy_hi && !cancelled_in_cells; ++iy) {
      for (long long ix = ix_lo; ix <= ix_hi; ++ix) {
        // The cancellation checkpoint of the candidate cell loop: the cell
        // that is being processed is the safety boundary.
        if (context.cancellation.requested()) {
          outcome.termination = Termination::unsupported;
          outcome.reason = "cancelled";
          cancelled_in_cells = true;
          break;
        }
        const HealpixLeaf leaf = coordinates::chart_leaf(order, model.face, static_cast<U64>(ix),
                                                          static_cast<U64>(iy));
        const double s_lo = static_cast<double>(ix) * inv_nside;
        const double s_hi = static_cast<double>(ix + 1) * inv_nside;
        const double t_lo = static_cast<double>(iy) * inv_nside;
        const double t_hi = static_cast<double>(iy + 1) * inv_nside;

        ++geometry::kernel_counts().candidate_cells;
        StageCounts& counts = geometry::kernel_counts();
        RationalPolygon inner = rectangle;
        HalfSpace space = chart_half_space_le(model, true, s_hi - model.e_s);
        inner = clip_half_space(inner, space.a, space.b, space.c);
        space = chart_half_space_ge(model, true, s_lo + model.e_s);
        inner = clip_half_space(inner, space.a, space.b, space.c);
        space = chart_half_space_le(model, false, t_hi - model.e_t);
        inner = clip_half_space(inner, space.a, space.b, space.c);
        space = chart_half_space_ge(model, false, t_lo + model.e_t);
        inner = clip_half_space(inner, space.a, space.b, space.c);

        RationalPolygon outer = rectangle;
        space = chart_half_space_le(model, true, s_hi + model.e_s);
        outer = clip_half_space(outer, space.a, space.b, space.c);
        space = chart_half_space_ge(model, true, s_lo - model.e_s);
        outer = clip_half_space(outer, space.a, space.b, space.c);
        space = chart_half_space_le(model, false, t_hi + model.e_t);
        outer = clip_half_space(outer, space.a, space.b, space.c);
        space = chart_half_space_ge(model, false, t_lo - model.e_t);
        outer = clip_half_space(outer, space.a, space.b, space.c);

        counts.rational_clips += 8;
        Rational inner_lower(0, 1);
        Rational inner_upper(0, 1);
        Rational outer_lower(0, 1);
        Rational outer_upper(0, 1);
        integrate_affine(inner, model, inner_lower, inner_upper);
        integrate_affine(outer, model, outer_lower, outer_upper);

        {
        }
        ++cells_visited;
        ++cells_pending;
        ++outcome.work;
        {
          geometry::StageTimer dyadic_timer(geometry::kernel_timings().dyadic_merge_ns);
          LeafLedger& ledger = ledgers[{leaf.order, leaf.ipix}];
          ledger.lower.add(inner_lower, false);
          ledger.upper.add(outer_upper, true);
          total_lower.add(inner_lower, false);
          total_upper.add(outer_upper, true);
        }
      }
    }

    // The candidate cells of the block are charged to the shared ledger after
    // they are processed; an exhausted ledger stops the kernel with a named
    // work termination and the undecided mass of the remaining blocks is kept.
    {
      Result<U64> charged;
      {
        geometry::StageTimer work_timer(geometry::kernel_timings().work_metering_ns);
        charged = context.resources.consume_work(cells_pending);
      }
      ++geometry::kernel_counts().work_charges;
      geometry::kernel_counts().work_units += cells_pending;
      cells_pending = 0;
      if (charged.code != Code::ok || outcome.work > max_work) {
        // The work budget is exhausted: the remaining blocks stay undecided
        // and keep their mass, and no bound is certified.
        for (const Block& pending : queue) keep_unresolved(pending);
        queue.clear();
        outcome.termination = Termination::work;
        outcome.reason = "shared_work_budget_exhausted";
        break;
      }
    }
  }

  outcome.leaves.reserve(ledgers.size());
  for (const auto& entry : ledgers) {
    LeafAreaBound bound;
    bound.leaf = HealpixLeaf{entry.first.first, entry.first.second};
    bound.lower = entry.second.lower.dyadic();
    bound.upper = entry.second.upper.dyadic();
    outcome.leaves.push_back(bound);
  }
  outcome.total_lower = total_lower.dyadic();
  outcome.total_upper = total_upper.dyadic();
  outcome.unresolved_upper = unresolved_upper.dyadic();
  outcome.unresolved_lower = Dyadic{"0", 0};
  if (outcome.termination == Termination::tolerance_met && !outcome.unresolved.empty()) {
    outcome.termination = Termination::depth;
    outcome.reason = "unresolved_domains_remain";
  }
  return outcome;
}

// ---------------------------------------------------------------------------
// The area of a detector footprint
// ---------------------------------------------------------------------------

RationalAreaBound footprint_area_rational(const WcsModel& wcs,
                                           const std::vector<std::vector<std::array<double, 2>>>& rings) {
  RationalAreaBound bound;
  bound.lower = Rational(0, 1);
  bound.upper = Rational(0, 1);
  if (rings.empty()) return bound;

  double u_lo = std::numeric_limits<double>::infinity();
  double u_hi = -std::numeric_limits<double>::infinity();
  double v_lo = std::numeric_limits<double>::infinity();
  double v_hi = -std::numeric_limits<double>::infinity();
  for (const auto& ring : rings) {
    for (const auto& vertex : ring) {
      u_lo = std::min(u_lo, vertex[0] + 1.0 - wcs.crpix_fits1[0]);
      u_hi = std::max(u_hi, vertex[0] + 1.0 - wcs.crpix_fits1[0]);
      v_lo = std::min(v_lo, vertex[1] + 1.0 - wcs.crpix_fits1[1]);
      v_hi = std::max(v_hi, vertex[1] + 1.0 - wcs.crpix_fits1[1]);
    }
  }
  if (!(u_hi >= u_lo) || !(v_hi >= v_lo)) return bound;

  Block block;
  block.u0 = u_lo;
  block.u1 = u_hi;
  block.v0 = v_lo;
  block.v1 = v_hi;
  BlockModel model;
  bool apex = false;
  if (!build_block_model(wcs, block, model, apex)) return bound;

  Rational area(0, 1);
  Rational moment_u(0, 1);
  Rational moment_v(0, 1);
  for (const auto& ring : rings) {
    RationalPolygon single;
    for (const auto& vertex : ring) {
      single.push_back({Rational::from_double_exact(vertex[0] + 1.0 - wcs.crpix_fits1[0]),
                         Rational::from_double_exact(vertex[1] + 1.0 - wcs.crpix_fits1[1])});
    }
    area = area + polygon_signed_area(single);
    moment_u = moment_u + polygon_moment_x(single);
    moment_v = moment_v + polygon_moment_y(single);
  }
  if (area.sign() < 0) {
    area = -area;
    moment_u = -moment_u;
    moment_v = -moment_v;
  }
  const Rational cu = Rational::from_double_exact(model.cu);
  const Rational cv = Rational::from_double_exact(model.cv);
  const Rational g0 = Rational::from_double_exact(model.g0);
  const Rational gu = Rational::from_double_exact(model.gu);
  const Rational gv = Rational::from_double_exact(model.gv);
  const Rational eg = Rational::from_double_exact(model.e_g);
  const Rational centre = g0 * area + gu * (moment_u - cu * area) + gv * (moment_v - cv * area);
  bound.lower = centre - eg * area;
  bound.upper = centre + eg * area;
  if (bound.lower.sign() < 0) bound.lower = Rational(0, 1);
  if (bound.upper.sign() < 0) bound.upper = Rational(0, 1);
  return bound;
}

AreaBound footprint_area(const WcsModel& wcs,
                         const std::vector<std::vector<std::array<double, 2>>>& rings) {
  const RationalAreaBound rational = footprint_area_rational(wcs, rings);
  AreaBound bound;
  bound.lower.signed_integer = scaled_floor(rational.lower, DyadicLedger::kScaleBits).to_string();
  bound.lower.exponent2 = -DyadicLedger::kScaleBits;
  bound.upper.signed_integer = scaled_ceil(rational.upper, DyadicLedger::kScaleBits).to_string();
  bound.upper.exponent2 = -DyadicLedger::kScaleBits;
  return bound;
}

}  // namespace acsd::geometry
