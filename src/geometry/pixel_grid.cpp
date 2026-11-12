#include "geometry/pixel_grid.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "coordinates/chart.hpp"
#include "coordinates/tan_sip.hpp"
#include "geometry/kernel.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/interval.hpp"

namespace acsd::geometry {
namespace {

using numerics::Interval;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct LocalVertex {
  Interval x;
  Interval y;
};

// The local coordinates of the drop rectangle.  The rectangle is the detector
// pixel contracted about its centre by pixfrac; the coordinates are shifted to
// the pixel centre, so the shoelace area does not suffer cancellation.
using LocalPolygon = std::vector<LocalVertex>;

Interval interval_of(double value) { return Interval(value, value); }

// Clips a convex polygon by  a*x + b*y <= c.  Returns false when the sign of a
// vertex on the boundary cannot be certified; the caller then uses the exact
// rational kernel for this pixel instead of clamping an undecided sign.
bool clip_half_space(LocalPolygon& polygon, double a, double b, const Interval& c) {
  if (polygon.empty()) return true;
  LocalPolygon result;
  result.reserve(polygon.size() + 2);
  const std::size_t count = polygon.size();
  auto evaluate = [&](const LocalVertex& vertex) -> Interval {
    return numerics::interval_sub(
        numerics::interval_add(numerics::interval_scale(vertex.x, a),
                               numerics::interval_scale(vertex.y, b)),
        c);
  };
  for (std::size_t i = 0; i < count; ++i) {
    const LocalVertex& current = polygon[i];
    const LocalVertex& next = polygon[(i + 1) % count];
    const Interval f_current = evaluate(current);
    const Interval f_next = evaluate(next);
    if (!(f_current.upper <= 0.0) && !(f_current.lower > 0.0)) return false;
    if (!(f_next.upper <= 0.0) && !(f_next.lower > 0.0)) return false;
    const bool inside_current = f_current.upper <= 0.0;
    const bool inside_next = f_next.upper <= 0.0;
    if (inside_current) result.push_back(current);
    if (inside_current != inside_next) {
      const Interval denominator = numerics::interval_sub(f_current, f_next);
      if (denominator.lower <= 0.0 && denominator.upper >= 0.0) return false;
      const Interval t = numerics::interval_div(f_current, denominator);
      LocalVertex crossing;
      crossing.x = numerics::interval_add(
          current.x, numerics::interval_mul(t, numerics::interval_sub(next.x, current.x)));
      crossing.y = numerics::interval_add(
          current.y, numerics::interval_mul(t, numerics::interval_sub(next.y, current.y)));
      result.push_back(crossing);
    }
  }
  polygon = std::move(result);
  return true;
}

struct PolygonArea {
  Interval area;
  Interval moment_x;
  Interval moment_y;
};

// The shoelace area and first moments of a polygon whose vertices are
// interval boxes.  The enclosure over independent vertex choices contains the
// value of the true polygon.
PolygonArea polygon_area(const LocalPolygon& polygon) {
  PolygonArea out{interval_of(0.0), interval_of(0.0), interval_of(0.0)};
  if (polygon.size() < 3) return out;
  const std::size_t count = polygon.size();
  Interval twice_area = interval_of(0.0);
  Interval moment_x = interval_of(0.0);
  Interval moment_y = interval_of(0.0);
  for (std::size_t i = 0; i < count; ++i) {
    const LocalVertex& a = polygon[i];
    const LocalVertex& b = polygon[(i + 1) % count];
    const Interval cross = numerics::interval_sub(numerics::interval_mul(a.x, b.y),
                                                  numerics::interval_mul(b.x, a.y));
    twice_area = numerics::interval_add(twice_area, cross);
    const Interval sum_x = numerics::interval_add(a.x, b.x);
    const Interval sum_y = numerics::interval_add(a.y, b.y);
    moment_x = numerics::interval_add(moment_x, numerics::interval_mul(sum_x, cross));
    moment_y = numerics::interval_add(moment_y, numerics::interval_mul(sum_y, cross));
  }
  out.area = numerics::interval_scale(twice_area, 0.5);
  out.moment_x = numerics::interval_scale(moment_x, 1.0 / 6.0);
  out.moment_y = numerics::interval_scale(moment_y, 1.0 / 6.0);
  return out;
}

// The integral of the affine density model g0 + gu*u + gv*v over the polygon
// (u,v are the local coordinates), widened by the model residual e_g*area.
// The result is clamped into [0, +inf): a solid angle is non-negative, and the
// clamp only widens the enclosure from below to zero.
Interval integrated_area(const PolygonArea& polygon, const PixelBlockModel& model) {
  Interval value = numerics::interval_scale(polygon.area, model.g0);
  value = numerics::interval_add(value, numerics::interval_scale(polygon.moment_x, model.gu));
  value = numerics::interval_add(value, numerics::interval_scale(polygon.moment_y, model.gv));
  const Interval residual = numerics::interval_scale(
      numerics::interval_abs(polygon.area), model.e_g);
  value = numerics::interval_add(value, Interval(-residual.upper, residual.upper));
  if (value.lower < 0.0) value.lower = 0.0;
  if (value.upper < 0.0) value.upper = 0.0;
  return value;
}

bool emit_term(const PixelTermSink& sink, U64 source_row, U32 order, U64 ipix,
               const Interval& overlap, const Interval& drop, const Interval& native,
               Result<rt::Done>& failure) {
  if (!(overlap.upper > 0.0)) return true;
  PixelTerm term;
  term.order = order;
  term.ipix = ipix;
  term.overlap_lower = overlap.lower;
  term.overlap_upper = overlap.upper;
  term.drop_lower = drop.lower;
  term.drop_upper = drop.upper;
  term.native_lower = native.lower;
  term.native_upper = native.upper;
  Result<rt::Done> added = sink(source_row, term);
  if (!ok(added)) {
    failure = added;
    return false;
  }
  return true;
}

// The exact fallback: the same cut cell kernel and footprint areas that
// compute_overlap uses for one source row.  It is only entered for pixels that
// the streaming model refuses (face seam, chart case boundary, polar apex,
// undecidable interval sign).
Result<rt::Done> exact_pixel_terms(Context& context, const WcsModel& wcs, U64 x, U64 y, U64 width,
                                   U32 order, double pixfrac, const GeometryBudget& budget,
                                   U64 source_row, const PixelTermSink& sink) {
  using model::fail;
  using model::ok_result;
  const double x0 = static_cast<double>(x) - 0.5;
  const double y0 = static_cast<double>(y) - 0.5;
  const double x1 = static_cast<double>(x) + 0.5;
  const double y1 = static_cast<double>(y) + 0.5;
  const double h = 0.5 * pixfrac;
  const double cx = 0.5 * (x0 + x1);
  const double cy = 0.5 * (y0 + y1);
  const Box box{{cx - h, cy - h}, {cx + h, cy + h}};
  const KernelOutcome kernel = cut_cell_overlap(context, wcs, box, order, budget);
  if (kernel.termination != Termination::tolerance_met) {
    return fail<rt::Done>(Code::resource_exhausted, "pixel_overlap_not_certified", "pixel_grid",
                          "the per-pixel cut cell kernel did not reach its tolerance");
  }
  // Native and (contracting) drop area of this one pixel.
  const std::vector<std::vector<std::array<double, 2>>> native_rings{
      {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}};
  const std::vector<std::vector<std::array<double, 2>>> drop_rings{
      {{cx - h, cy - h}, {cx + h, cy - h}, {cx + h, cy + h}, {cx - h, cy + h}}};
  const AreaBound native = footprint_area(wcs, native_rings);
  const AreaBound drop = footprint_area(wcs, drop_rings);
  const Interval native_interval(dyadic_to_double(native.lower), dyadic_to_double(native.upper));
  const Interval drop_interval(dyadic_to_double(drop.lower), dyadic_to_double(drop.upper));
  if (!(drop_interval.upper > 0.0)) {
    return fail<rt::Done>(Code::invalid_model, "empty_drop_area", "pixel_grid",
                          "the drop area of a pixel is not positive");
  }
  (void)width;
  for (const LeafAreaBound& leaf : kernel.leaves) {
    const Interval overlap(dyadic_to_double(leaf.lower), dyadic_to_double(leaf.upper));
    Result<rt::Done> failure;
    if (!emit_term(sink, source_row, leaf.leaf.order, leaf.leaf.ipix, overlap, drop_interval,
                   native_interval, failure)) {
      return failure;
    }
  }
  return ok_result(rt::Done{});
}

}  // namespace

Result<rt::Done> pixel_grid_overlap(Context& context, const WcsModel& wcs, U64 width, U64 height,
                                    U64 first_row, U64 row_count, U32 order, double pixfrac,
                                    const GeometryBudget& budget, const PixelTermSink& sink) {
  using model::fail;
  using model::ok_result;
  if (!sink) {
    return fail<rt::Done>(Code::invalid_request, "null_pixel_sink", "pixel_grid");
  }
  if (!(pixfrac > 0.0) || pixfrac > 1.0) {
    return fail<rt::Done>(Code::invalid_request, "invalid_pixfrac", "pixel_grid");
  }
  if (order == 0 || order > 24) {
    return fail<rt::Done>(Code::invalid_request, "order_out_of_range", "pixel_grid");
  }
  if (first_row >= height) return ok_result(rt::Done{});
  const U64 last_row = std::min(height, first_row + row_count);
  const U64 nside = 1ull << order;
  const double inv_nside = 1.0 / static_cast<double>(nside);
  const double crpix_u = wcs.crpix_fits1[0];
  const double crpix_v = wcs.crpix_fits1[1];
  const double h = 0.5 * pixfrac;

  U64 pixel_index = first_row * width;
  for (U64 y = first_row; y < last_row; ++y) {
    for (U64 x = 0; x < width; ++x, ++pixel_index) {
      if ((pixel_index & 0x3fffull) == 0 && context.cancellation.requested()) {
        return fail<rt::Done>(Code::cancelled, "cancelled", "pixel_grid");
      }
      // The traversal itself is charged to the shared ledger: one unit per
      // pixel plus the candidate cell units added by the boundary clips.
      Result<U64> charged = context.resources.consume_work(1);
      if (!ok(charged)) {
        return fail<rt::Done>(Code::resource_exhausted, "pixel_grid_work_exhausted",
                              "pixel_grid",
                              "the shared work budget is exhausted before the frame was walked");
      }
      const double u = static_cast<double>(x) + 1.0 - crpix_u;
      const double v = static_cast<double>(y) + 1.0 - crpix_v;
      PixelBlockModel model;
      bool apex = false;
      bool chart_domain = false;
      const bool modelled =
          build_pixel_model(wcs, u - 0.5, u + 0.5, v - 0.5, v + 0.5, model, apex, chart_domain);
      bool exact = !modelled;
      if (modelled) {
        const long long ix = static_cast<long long>(std::floor(model.s0 * static_cast<double>(nside)));
        const long long iy = static_cast<long long>(std::floor(model.t0 * static_cast<double>(nside)));
        const bool contained =
            ix >= 0 && iy >= 0 && ix < static_cast<long long>(nside) &&
            iy < static_cast<long long>(nside) &&
            model.s_lo > static_cast<double>(ix) * inv_nside &&
            model.s_hi < static_cast<double>(ix + 1) * inv_nside &&
            model.t_lo > static_cast<double>(iy) * inv_nside &&
            model.t_hi < static_cast<double>(iy + 1) * inv_nside;
        if (contained) {
          // a_jp = A_drop, w = 1.  The native area is the integral of the
          // density model over the pixel; the residual e_g bounds it.
          const Interval native(model.g0 - model.e_g, model.g0 + model.e_g);
          const double drop_scale = pixfrac * pixfrac;
          const Interval drop(native.lower * drop_scale, native.upper * drop_scale);
          const HealpixLeaf leaf = coordinates::chart_leaf(
              order, model.face, static_cast<U64>(ix), static_cast<U64>(iy));
          if (leaf.ipix == std::numeric_limits<U64>::max()) {
            exact = true;
          } else {
            Result<rt::Done> failure;
            if (!emit_term(sink, pixel_index, order, leaf.ipix, drop, drop, native, failure)) {
              return failure;
            }
            continue;
          }
        }
      }
      if (!exact) {
        // Boundary pixel: clip the local drop rectangle against every candidate
        // cell.  A pixel is clipped only against the cells its certified chart
        // enclosure touches.
        const long long last = static_cast<long long>(nside) - 1;
        long long ix_lo =
            static_cast<long long>(std::floor(model.s_lo * static_cast<double>(nside)));
        long long ix_hi =
            static_cast<long long>(std::floor(model.s_hi * static_cast<double>(nside)));
        long long iy_lo =
            static_cast<long long>(std::floor(model.t_lo * static_cast<double>(nside)));
        long long iy_hi =
            static_cast<long long>(std::floor(model.t_hi * static_cast<double>(nside)));
        if (ix_lo < 0 || iy_lo < 0 || ix_hi > last || iy_hi > last || ix_hi < ix_lo ||
            iy_hi < iy_lo) {
          exact = true;
        } else {
          const Interval native(model.g0 - model.e_g, model.g0 + model.e_g);
          const double drop_scale = pixfrac * pixfrac;
          const Interval drop(native.lower * drop_scale, native.upper * drop_scale);
          for (long long iy = iy_lo; iy <= iy_hi; ++iy) {
            for (long long ix = ix_lo; ix <= ix_hi; ++ix) {
              Interval inner_area;
              Interval outer_area;
              LocalPolygon inner;
              LocalPolygon outer;
              inner.push_back({interval_of(-h), interval_of(-h)});
              inner.push_back({interval_of(h), interval_of(-h)});
              inner.push_back({interval_of(h), interval_of(h)});
              inner.push_back({interval_of(-h), interval_of(h)});
              outer = inner;
              bool decided = true;
              auto clip_four = [&](bool s_axis, double bound_lower, double bound_upper,
                                   bool outer_polygon) -> bool {
                const double gu = s_axis ? model.gs_u : model.gt_u;
                const double gv = s_axis ? model.gs_v : model.gt_v;
                const double centre = s_axis ? model.s0 : model.t0;
                const double error = s_axis ? model.e_s : model.e_t;
                LocalPolygon& polygon = outer_polygon ? outer : inner;
                const Interval offset_upper = numerics::interval_sub(
                    interval_of(bound_upper + (outer_polygon ? error : -error)),
                    interval_of(centre));
                const Interval offset_lower = numerics::interval_sub(
                    interval_of(bound_lower - (outer_polygon ? error : -error)),
                    interval_of(centre));
                const double c_hi = outer_polygon ? offset_upper.upper : offset_upper.lower;
                const double c_lo = outer_polygon ? -offset_lower.lower : -offset_lower.upper;
                if (!clip_half_space(polygon, gu, gv, interval_of(c_hi))) return false;
                if (!clip_half_space(polygon, -gu, -gv, interval_of(c_lo))) return false;
                return true;
              };
              if (!clip_four(true, static_cast<double>(ix) * inv_nside,
                             static_cast<double>(ix + 1) * inv_nside, false)) {
                decided = false;
              }
              if (decided && !clip_four(false, static_cast<double>(iy) * inv_nside,
                                        static_cast<double>(iy + 1) * inv_nside, false)) {
                decided = false;
              }
              if (decided && !clip_four(true, static_cast<double>(ix) * inv_nside,
                                        static_cast<double>(ix + 1) * inv_nside, true)) {
                decided = false;
              }
              if (decided && !clip_four(false, static_cast<double>(iy) * inv_nside,
                                        static_cast<double>(iy + 1) * inv_nside, true)) {
                decided = false;
              }
              if (!decided) {
                exact = true;
                break;
              }
              const Interval inner_density = integrated_area(polygon_area(inner), model);
              const Interval outer_density = integrated_area(polygon_area(outer), model);
              inner_area = inner_density;
              outer_area = outer_density;
              if (inner_area.lower < 0.0) inner_area.lower = 0.0;
              if (outer_area.lower < 0.0) outer_area.lower = 0.0;
              if (!(outer_area.upper > 0.0)) continue;
              const HealpixLeaf leaf =
                  coordinates::chart_leaf(order, model.face, static_cast<U64>(ix),
                                          static_cast<U64>(iy));
              if (leaf.ipix == std::numeric_limits<U64>::max()) {
                exact = true;
                break;
              }
              Result<rt::Done> failure;
              if (!emit_term(sink, pixel_index, order, leaf.ipix, inner_area, drop, native,
                             failure)) {
                return failure;
              }
            }
            if (exact) break;
          }
          if (!exact) continue;
        }
      }
      Result<rt::Done> fallback =
          exact_pixel_terms(context, wcs, x, y, width, order, pixfrac, budget, pixel_index, sink);
      if (!ok(fallback)) return fallback;
    }
  }
  return ok_result(rt::Done{});
}

}  // namespace acsd::geometry
