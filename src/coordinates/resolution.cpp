#include "coordinates/resolution.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "coordinates/healpix.hpp"
#include "coordinates/tan_sip.hpp"

namespace acsd::coordinates {
namespace {

constexpr double kArcsecPerRadian = 180.0 / M_PI * 3600.0;

// The adaptive sampling constants are the ones of the main production
// implementation (drizzle_engine.cpp).  They are part of the decision: a
// silent change of depth or of the ratio threshold changes which local scales
// are sampled.
constexpr int kAdaptiveMaxDepth = 5;      // 4^5 = 1024 leaf cells
constexpr double kAdaptiveRatio = 1.25;   // max/min above this triggers a split
constexpr double kAdaptiveMinCellPx = 4.0;
constexpr double kHalfStepPx = 0.5;       // central difference over one pixel

SkyICRS forward_sky(const WcsModel& wcs, double x, double y) {
  const Vec2 uv = pixel_to_uv(wcs, x, y);
  const Vec2 q = tangent_offset(wcs, uv[0], uv[1]);
  return direction_from_tangent(wcs, q);
}

bool finite_direction(const SkyICRS& n) {
  return std::isfinite(n.nx) && std::isfinite(n.ny) && std::isfinite(n.nz);
}

double dot(const SkyICRS& a, const SkyICRS& b) {
  return a.nx * b.nx + a.ny * b.ny + a.nz * b.nz;
}

SkyICRS cross(const SkyICRS& a, const SkyICRS& b) {
  SkyICRS out;
  out.nx = a.ny * b.nz - a.nz * b.ny;
  out.ny = a.nz * b.nx - a.nx * b.nz;
  out.nz = a.nx * b.ny - a.ny * b.nx;
  return out;
}

bool normalize_direction(SkyICRS& n) {
  const double norm = std::sqrt(dot(n, n));
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  n.nx /= norm;
  n.ny /= norm;
  n.nz /= norm;
  return true;
}

// The local angular pixel scale: the smallest singular value of the tangent
// plane Jacobian built from five samples (the centre and the four one-pixel
// neighbours), exactly as the main production compute_auto_nside_ex.
bool local_scale_arcsec(const WcsModel& wcs, double x, double y, double& arcsec) {
  const SkyICRS centre = forward_sky(wcs, x, y);
  if (!finite_direction(centre)) return false;
  const SkyICRS xm = forward_sky(wcs, x - kHalfStepPx, y);
  const SkyICRS xp = forward_sky(wcs, x + kHalfStepPx, y);
  const SkyICRS ym = forward_sky(wcs, x, y - kHalfStepPx);
  const SkyICRS yp = forward_sky(wcs, x, y + kHalfStepPx);
  if (!finite_direction(xm) || !finite_direction(xp) || !finite_direction(ym) ||
      !finite_direction(yp)) {
    return false;
  }
  // An orthonormal tangent basis at the centre.  The singular values of the
  // 2x2 Jacobian do not depend on the choice of an orthonormal basis, so any
  // stable construction is valid (including at the poles, where the sky
  // longitude basis degenerates).
  SkyICRS reference{0.0, 0.0, 1.0};
  if (std::fabs(dot(centre, reference)) > 0.9) reference = SkyICRS{1.0, 0.0, 0.0};
  SkyICRS east = cross(reference, centre);
  if (!normalize_direction(east)) return false;
  SkyICRS north = cross(centre, east);
  if (!normalize_direction(north)) return false;
  // Gnomonic (tangent plane) projection xi = (v.east)/(v.centre).
  auto project = [&](const SkyICRS& v, double& xi, double& eta) {
    const double denominator = dot(v, centre);
    if (!(std::fabs(denominator) > 1e-15)) return false;
    xi = dot(v, east) / denominator;
    eta = dot(v, north) / denominator;
    return true;
  };
  double xi_xm = 0, eta_xm = 0, xi_xp = 0, eta_xp = 0;
  double xi_ym = 0, eta_ym = 0, xi_yp = 0, eta_yp = 0;
  if (!project(xm, xi_xm, eta_xm) || !project(xp, xi_xp, eta_xp) ||
      !project(ym, xi_ym, eta_ym) || !project(yp, xi_yp, eta_yp)) {
    return false;
  }
  const double step = 2.0 * kHalfStepPx;
  const double j11 = (xi_xp - xi_xm) / step;
  const double j12 = (xi_yp - xi_ym) / step;
  const double j21 = (eta_xp - eta_xm) / step;
  const double j22 = (eta_yp - eta_ym) / step;
  if (!std::isfinite(j11) || !std::isfinite(j12) || !std::isfinite(j21) ||
      !std::isfinite(j22)) {
    return false;
  }
  const double a = j11 * j11 + j21 * j21;
  const double d = j12 * j12 + j22 * j22;
  const double determinant = j11 * j22 - j12 * j21;
  const double trace = a + d;
  double discriminant = trace * trace - 4.0 * determinant * determinant;
  if (discriminant < 0.0) discriminant = 0.0;
  double lambda_min = 0.5 * (trace - std::sqrt(discriminant));
  if (lambda_min < 0.0) lambda_min = 0.0;
  const double sigma_min = std::sqrt(lambda_min);  // tangent units per pixel
  arcsec = sigma_min * kArcsecPerRadian;
  return std::isfinite(arcsec) && arcsec > 0.0;
}

struct QuadTreeSample {
  const WcsModel& wcs;
  double finest = std::numeric_limits<double>::infinity();
  double coarsest = 0.0;
  U64 valid = 0;
  U64 invalid = 0;
};

void sample_quadtree(QuadTreeSample& state, double x0, double y0, double x1, double y1, int depth) {
  // The nine points are evaluated once per visited cell and the cell is split
  // only when its own scale ratio exceeds the main threshold.
  const double xc = 0.5 * (x0 + x1);
  const double yc = 0.5 * (y0 + y1);
  const double xs[9] = {x0, x1, x0, x1, xc, xc, x0, x1, xc};
  const double ys[9] = {y0, y0, y1, y1, y0, y1, yc, yc, yc};
  double local_min = std::numeric_limits<double>::infinity();
  double local_max = 0.0;
  for (int i = 0; i < 9; ++i) {
    double arcsec = 0.0;
    if (!local_scale_arcsec(state.wcs, xs[i], ys[i], arcsec)) {
      ++state.invalid;
      continue;
    }
    ++state.valid;
    local_min = std::min(local_min, arcsec);
    local_max = std::max(local_max, arcsec);
    state.finest = std::min(state.finest, arcsec);
    state.coarsest = std::max(state.coarsest, arcsec);
  }
  const double cell_width = x1 - x0;
  const double cell_height = y1 - y0;
  const bool too_deep = depth >= kAdaptiveMaxDepth;
  const bool too_small = cell_width < kAdaptiveMinCellPx || cell_height < kAdaptiveMinCellPx;
  const bool no_data = local_min >= std::numeric_limits<double>::infinity() || local_max <= 0.0;
  const bool uniform = !no_data && local_max / local_min <= kAdaptiveRatio;
  if (too_deep || too_small || no_data || uniform) return;
  sample_quadtree(state, x0, y0, xc, yc, depth + 1);
  sample_quadtree(state, xc, y0, x1, yc, depth + 1);
  sample_quadtree(state, x0, yc, xc, y1, depth + 1);
  sample_quadtree(state, xc, yc, x1, y1, depth + 1);
}

}  // namespace

PixelScaleProbe probe_pixel_scale(const WcsModel& wcs, U64 width, U64 height) {
  PixelScaleProbe probe;
  if (width == 0 || height == 0) {
    probe.reason = "the detector has no pixels";
    return probe;
  }
  // The sampled region is the interior of the detector; the half pixel margin
  // keeps the one-pixel central differences inside the frame.
  const double margin = 0.5;
  const double x0 = margin;
  const double y0 = margin;
  const double x1 = std::max(static_cast<double>(width) - margin, x0 + 1.0);
  const double y1 = std::max(static_cast<double>(height) - margin, y0 + 1.0);
  QuadTreeSample state{wcs};
  sample_quadtree(state, x0, y0, x1, y1, 0);
  probe.finest_arcsec = state.finest;
  probe.coarsest_arcsec = state.coarsest;
  probe.valid_samples = state.valid;
  probe.invalid_samples = state.invalid;
  probe.valid = state.valid > 0 && std::isfinite(state.finest) && state.finest > 0.0;
  if (!probe.valid) {
    probe.reason = "no sample of the WCS produced a finite positive pixel scale";
  }
  return probe;
}

DrizzleCapacityEstimate estimate_drizzle_capacity(const WcsModel& wcs, U64 width, U64 height,
                                                  U32 order) {
  DrizzleCapacityEstimate estimate;
  if (order > kMaxOrder) return estimate;
  const PixelScaleProbe probe = probe_pixel_scale(wcs, width, height);
  if (!probe.valid) return estimate;
  const double pixels = static_cast<double>(width) * static_cast<double>(height);
  const double leaf = kHealpixScalePerNsideArcsec / std::ldexp(1.0, static_cast<int>(order));
  const double ratio = probe.coarsest_arcsec / leaf;
  estimate.pixel_leaf_ratio = ratio;
  // A detector pixel is rotated against the local chart axes.  Its chart-axis
  // bounding box is up to sqrt(2) times its side, so a pixel of r leaf widths
  // spans at most about sqrt(2)*r + 1 cells per axis; the two dimensional term
  // count is that squared with a 20% margin.  The same bound is enforced by the
  // accumulator.
  const double axis_ratio = ratio * 1.4142135623730951;
  const double terms_per_pixel = (axis_ratio + 1.0) * (axis_ratio + 1.0) * 1.20;
  // Covered leaf area plus a one-leaf-wide perimeter; 15% margin.
  estimate.maximum_terms = static_cast<U64>(pixels * terms_per_pixel) + 1024;
  U64 leaves = static_cast<U64>(pixels * ratio * ratio * 1.15) +
               static_cast<U64>(8.0 * std::sqrt(pixels) * (ratio + 1.0)) + 1024;
  if (leaves > estimate.maximum_terms) leaves = estimate.maximum_terms;
  estimate.maximum_leaves = leaves;
  // The aggregate table is allocated per touched 4096-leaf block, so the
  // declared bytes must count full blocks (a partial block still costs 4096
  // entries); 69 bytes of table per leaf plus 20 bytes of member per term.
  const U64 blocks = (leaves + 4095ull) / 4096ull;
  const U64 table_leaves = blocks * 4096ull;
  estimate.accumulation_bytes = table_leaves * 69ull + estimate.maximum_terms * 20ull;
  // 91 bytes per published leaf array entry plus 16 bytes per CSR term.
  estimate.output_bytes = leaves * 91ull + estimate.maximum_terms * 16ull;
  estimate.declared_bytes = estimate.accumulation_bytes + estimate.output_bytes +
                            64ull * 1024 * 1024;
  estimate.valid = true;
  return estimate;
}

LeafOrderDecision decide_leaf_order(const WcsModel& wcs, U64 width, U64 height) {
  LeafOrderDecision decision;
  const PixelScaleProbe probe = probe_pixel_scale(wcs, width, height);
  decision.finest_arcsec = probe.finest_arcsec;
  if (!probe.valid) {
    decision.reason = probe.reason;
    return decision;
  }
  // nside such that sqrt(pi/3)/nside <= finest, i.e. nside >= 211076.3/finest.
  const double required = kHealpixScalePerNsideArcsec / probe.finest_arcsec;
  decision.required_nside = required;
  double nside = 1.0;
  while (nside < required) nside *= 2.0;
  bool clamped = false;
  const double minimum = static_cast<double>(1ull << kMinimumAutoOrder);
  const double maximum = static_cast<double>(1ull << kMaximumAutoOrder);
  if (nside < minimum) {
    nside = minimum;
    clamped = true;
  }
  if (nside > maximum) {
    nside = maximum;
    clamped = true;
  }
  U32 order = 0;
  double value = nside;
  while (value > 1.0) {
    value *= 0.5;
    ++order;
  }
  decision.order = order;
  decision.nside = nside;
  decision.clamped = clamped;
  decision.leaf_arcsec = kHealpixScalePerNsideArcsec / nside;
  decision.oversample = decision.leaf_arcsec / probe.finest_arcsec;
  decision.valid = true;
  if (clamped) {
    decision.reason = nside <= minimum
                          ? "the required nside is below the main clamp 2^4"
                          : "the required nside is above the main clamp 2^22";
  }
  return decision;
}

}  // namespace acsd::coordinates
