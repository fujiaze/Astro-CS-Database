#include "geometry/containment.hpp"

#include "geometry/profile.hpp"

#include <algorithm>
#include <cmath>
#include <map>

#include "coordinates/chart.hpp"
#include "coordinates/healpix.hpp"
#include "coordinates/tan_sip.hpp"
#include "model/units.hpp"

namespace acsd::geometry {
namespace {

using model::BigInt;
using model::Rational;

// The number of segments per detector edge.  The segments bound the boundary
// curve of the source region in the chart; a finer subdivision gives a tighter
// separation test.
constexpr U64 kSegmentsPerEdge = 8;

BigInt big_pow(U64 base, U64 exponent) {
  BigInt result(1);
  BigInt factor(base);
  while (exponent > 0) {
    if ((exponent & 1ull) != 0ull) result = result * factor;
    factor = factor * factor;
    exponent >>= 1;
  }
  return result;
}

// floor(2^scale / denominator) for a positive denominator.
BigInt scaled_reciprocal(const BigInt& denominator, unsigned scale) {
  return BigInt::pow2(scale) / denominator;
}

// The alternating atan series  sum_k (-1)^k / ((2k+1) m^(2k+1))  evaluated on
// the dyadic grid 2^-scale.  The returned interval encloses the true value:
// the truncation error is bounded by the first omitted term and the rounding
// error of every term by one grid step.
void atan_reciprocal(U64 m, unsigned scale, U64 terms, BigInt& lower, BigInt& upper) {
  BigInt sum(0);
  BigInt next_term = scaled_reciprocal(BigInt(m), scale);
  for (U64 k = 0; k < terms; ++k) {
    const BigInt denominator = BigInt(2 * k + 1) * big_pow(m, 2 * k + 1);
    const BigInt term = scaled_reciprocal(denominator, scale);
    if ((k & 1ull) == 0ull) {
      sum = sum + term;
    } else {
      sum = sum - term;
    }
    next_term = term;
  }
  // The remainder of the alternating series is bounded by the first omitted
  // term, which is smaller than the last computed one.
  const BigInt remainder = next_term;
  lower = sum - remainder;
  upper = sum + remainder;
}

// A detector space segment.
struct Segment {
  double u0 = 0.0;
  double u1 = 0.0;
  double v0 = 0.0;
  double v1 = 0.0;
};

// The number of segments one detector edge is split into.
//
// The separation test resolves a cell only when the chart enclosure of a
// boundary segment stays below one target cell.  The edge is therefore split
// adaptively: the number of segments follows the chart extent of the edge
// (the curvature proxy of the boundary in the chart) and is bounded by the
// geometry budget.  A tangency, where the boundary runs along a cell
// boundary, gets the finest subdivision the budget allows.
U64 segments_for_edge(const WcsModel& wcs, double u0, double u1, double v0, double v1, U32 order,
                      U64 min_segments, U64 max_segments) {
  ChartBox box;
  if (!chart_enclosure(wcs, u0, u1, v0, v1, box)) return min_segments;
  const double nside = static_cast<double>(1ull << order);
  // The chart extent of the edge, widened by the residual of the affine chart
  // model over that edge.  The residual is the curvature proxy: where the
  // boundary runs tangentially the chart model is the least accurate, so the
  // edge is refined the most.
  const double span_s = (box.s_hi - box.s_lo + 2.0 * box.e_s) * nside;
  const double span_t = (box.t_hi - box.t_lo + 2.0 * box.e_t) * nside;
  const double cells = std::max(span_s, span_t);
  if (!(cells > 1.0)) return min_segments;
  const double wanted = std::ceil(cells);
  const double limited = std::min(wanted, static_cast<double>(max_segments));
  if (limited < static_cast<double>(min_segments)) return min_segments;
  return static_cast<U64>(limited);
}

// The segments of the boundary of the detector rectangle, with the adaptive
// number of segments per edge.
std::vector<Segment> boundary_segments(const WcsModel& wcs, const Box& detector_rect, U32 order,
                                       U64 min_segments, U64 max_segments, U64& total) {
  const double x0 = detector_rect.lower[0];
  const double x1 = detector_rect.upper[0];
  const double y0 = detector_rect.lower[1];
  const double y1 = detector_rect.upper[1];
  std::vector<Segment> segments;
  const U64 horizontal = segments_for_edge(wcs, x0, x1, y0, y0, order, min_segments, max_segments);
  const U64 vertical = segments_for_edge(wcs, x0, x0, y0, y1, order, min_segments, max_segments);
  const double dx = (x1 - x0) / static_cast<double>(horizontal);
  const double dy = (y1 - y0) / static_cast<double>(vertical);
  for (U64 i = 0; i < horizontal; ++i) {
    const double a = x0 + dx * static_cast<double>(i);
    const double b = a + dx;
    segments.push_back(Segment{a, b, y0, y0});
    segments.push_back(Segment{a, b, y1, y1});
  }
  for (U64 i = 0; i < vertical; ++i) {
    const double a = y0 + dy * static_cast<double>(i);
    const double b = a + dy;
    segments.push_back(Segment{x0, x0, a, b});
    segments.push_back(Segment{x1, x1, a, b});
  }
  total = segments.size();
  return segments;
}

// The chart enclosure of one boundary segment.  A segment whose chart model is
// not provable (a chart singularity or a face boundary crossing) makes the
// separation test fail for every cell: the cells stay undecided.
struct SegmentEnclosure {
  bool valid = false;
  double s_lo = 0.0;
  double s_hi = 0.0;
  double t_lo = 0.0;
  double t_hi = 0.0;
};

SegmentEnclosure segment_enclosure(const WcsModel& wcs, const Segment& segment) {
  SegmentEnclosure out;
  ++geometry::containment_counts().chart_models;
  ChartBox box;
  if (!chart_enclosure(wcs, segment.u0, segment.u1, segment.v0, segment.v1, box)) return out;
  out.valid = true;
  out.s_lo = box.s_lo;
  out.s_hi = box.s_hi;
  out.t_lo = box.t_lo;
  out.t_hi = box.t_hi;
  return out;
}

// True when two closed intervals overlap.
bool overlaps(double a_lo, double a_hi, double b_lo, double b_hi) {
  return !(a_hi < b_lo || b_hi < a_lo);
}

// The membership of the preimage of a direction in the detector rectangle.
enum class Membership { inside, outside, undecided };

Membership membership_of(const Box& root_box, const Box& detector_rect) {
  const bool disjoint = root_box.upper[0] < detector_rect.lower[0] ||
                        detector_rect.upper[0] < root_box.lower[0] ||
                        root_box.upper[1] < detector_rect.lower[1] ||
                        detector_rect.upper[1] < root_box.lower[1];
  if (disjoint) return Membership::outside;
  const bool contained = root_box.lower[0] >= detector_rect.lower[0] &&
                         root_box.upper[0] <= detector_rect.upper[0] &&
                         root_box.lower[1] >= detector_rect.lower[1] &&
                         root_box.upper[1] <= detector_rect.upper[1];
  if (contained) return Membership::inside;
  return Membership::undecided;
}

}  // namespace

DyadicInterval pi_enclosure() {
  constexpr unsigned kScale = static_cast<unsigned>(DyadicLedger::kScaleBits);
  BigInt a_lower;
  BigInt a_upper;
  BigInt b_lower;
  BigInt b_upper;
  atan_reciprocal(5, kScale, 48, a_lower, a_upper);
  atan_reciprocal(239, kScale, 12, b_lower, b_upper);
  // Machin: pi = 16 atan(1/5) - 4 atan(1/239).
  const BigInt lower = (a_lower * BigInt(16)) - (b_upper * BigInt(4));
  const BigInt upper = (a_upper * BigInt(16)) - (b_lower * BigInt(4));
  DyadicInterval out;
  out.lower.signed_integer = lower.to_string();
  out.lower.exponent2 = -static_cast<std::int32_t>(kScale);
  out.upper.signed_integer = upper.to_string();
  out.upper.exponent2 = -static_cast<std::int32_t>(kScale);
  return out;
}

ContainmentOutcome target_containment(Context& context, const WcsModel& wcs, Ref<WcsModel> model_ref,
                                      const Box& detector_rect, U32 order,
                                      const GeometryBudget& budget) {
  ContainmentOutcome outcome;
  const U64 nside = 1ull << order;
  const double inv_nside = 1.0 / static_cast<double>(nside);
  const double crpix_u = wcs.crpix_fits1[0];
  const double crpix_v = wcs.crpix_fits1[1];

  // The base variables of the detector rectangle.
  const double u0 = detector_rect.lower[0] + 1.0 - crpix_u;
  const double u1 = detector_rect.upper[0] + 1.0 - crpix_u;
  const double v0 = detector_rect.lower[1] + 1.0 - crpix_v;
  const double v1 = detector_rect.upper[1] + 1.0 - crpix_v;

  ChartBox box;
  ++geometry::containment_counts().chart_models;
  if (!chart_enclosure(wcs, u0, u1, v0, v1, box)) return outcome;

  // The chart enclosure of the boundary of the source region.  The boundary is
  // refined adaptively: the number of segments per edge follows the chart
  // extent of that edge and is bounded by the geometry budget.
  const U64 min_segments = kSegmentsPerEdge;
  const U64 max_segments = std::max<U64>(min_segments, budget.max_work / 4096ull);
  U64 segment_count = 0;
  std::vector<SegmentEnclosure> boundary;
  {
    geometry::StageTimer timer(geometry::containment_timings().boundary_segmentation_ns);
    for (const Segment& segment :
         boundary_segments(wcs, detector_rect, order, min_segments, max_segments, segment_count)) {
      // The cancellation checkpoint of the boundary segmentation.
      if (context.cancellation.requested()) {
        outcome.complete = false;
        outcome.cancelled = true;
        return outcome;
      }
      boundary.push_back(segment_enclosure(wcs, segment));
    }
  }
  outcome.boundary_segments = segment_count;
  geometry::containment_counts().boundary_segments += segment_count;
  const bool boundary_provable = std::all_of(boundary.begin(), boundary.end(),
                                              [](const SegmentEnclosure& s) { return s.valid; });
  if (!boundary_provable) {
    // The containment of the cells cannot be proven; the caller keeps the
    // unresolved mass of the region.
    return outcome;
  }

  // The exact area of one cell of the equal-area chart: pi / (3 nside^2),
  // evaluated on the ledger grid.
  const DyadicInterval pi = pi_enclosure();
  const BigInt divisor = BigInt(3) * BigInt(nside) * BigInt(nside);
  const BigInt cell_area_lower = BigInt::from_string(pi.lower.signed_integer) / divisor;
  const BigInt cell_area_upper = BigInt::from_string(pi.upper.signed_integer) / divisor;

  const long long ix_lo = static_cast<long long>(std::floor(box.s_lo * static_cast<double>(nside)));
  const long long ix_hi = static_cast<long long>(std::floor(box.s_hi * static_cast<double>(nside)));
  const long long iy_lo = static_cast<long long>(std::floor(box.t_lo * static_cast<double>(nside)));
  const long long iy_hi = static_cast<long long>(std::floor(box.t_hi * static_cast<double>(nside)));
  const double cells = static_cast<double>(ix_hi - ix_lo + 1) * static_cast<double>(iy_hi - iy_lo + 1);
  if (cells > 1.0e6) return outcome;

  const U64 max_work = budget.max_work > 0 ? budget.max_work : 1000000ull;
  NumericBudget inverse_budget;
  inverse_budget.max_iterations = 64;

  // The cell area as a dyadic.
  Dyadic cell_area;
  cell_area.signed_integer = cell_area_lower.to_string();
  cell_area.exponent2 = -DyadicLedger::kScaleBits;

  std::map<std::pair<U32, U64>, std::pair<BigInt, BigInt>> ledgers;
  BigInt undecided(0);

  for (long long iy = iy_lo; iy <= iy_hi; ++iy) {
    for (long long ix = ix_lo; ix <= ix_hi; ++ix) {
      if (outcome.work >= max_work) return outcome;
      ++outcome.work;
      // The shared work ledger: one unit per decided cell plus one unit per
      // boundary segment that the separation test has to exclude.
      {
        const U64 units = 1 + boundary.size();
        Result<U64> charged;
        {
          geometry::StageTimer timer(geometry::containment_timings().work_metering_ns);
          charged = context.resources.consume_work(units);
        }
        ++geometry::containment_counts().work_charges;
        geometry::containment_counts().work_units += units;
        if (charged.code != Code::ok) return outcome;
      }
      const double s_lo = static_cast<double>(ix) * inv_nside;
      const double s_hi = static_cast<double>(ix + 1) * inv_nside;
      const double t_lo = static_cast<double>(iy) * inv_nside;
      const double t_hi = static_cast<double>(iy + 1) * inv_nside;

      // The separation test: the boundary of the source region must not meet
      // the cell.
      bool separated = true;
      {
        geometry::StageTimer timer(geometry::containment_timings().separation_test_ns);
      for (const SegmentEnclosure& segment : boundary) {
        if (overlaps(s_lo, s_hi, segment.s_lo, segment.s_hi) &&
            overlaps(t_lo, t_hi, segment.t_lo, segment.t_hi)) {
          separated = false;
          break;
        }
      }
      }

      CellDecision decision = CellDecision::undecided;
      if (separated) {
        // The membership test: the preimage of the cell centre.
        const double s_mid = 0.5 * (s_lo + s_hi);
        const double t_mid = 0.5 * (t_lo + t_hi);
        const SkyICRS direction = coordinates::healpix_face_point(box.face, s_mid, t_mid);
        ++geometry::containment_counts().inverse_calls;
        Result<InverseResult> inverted;
        {
          geometry::StageTimer timer(geometry::containment_timings().membership_test_ns);
          inverted = acsd::inverse(context, model_ref, direction, inverse_budget);
        }
        if (inverted.code == Code::ok) {
          const Membership membership = membership_of(inverted.value->root_box, detector_rect);
          if (membership == Membership::inside) {
            decision = CellDecision::inside;
          } else if (membership == Membership::outside) {
            decision = CellDecision::outside;
          }
        }
      }

      if (decision == CellDecision::undecided) {
        ++outcome.cells_undecided;
        ++geometry::containment_counts().cells_undecided;
      } else {
        ++outcome.cells_decided;
        ++geometry::containment_counts().cells_decided;
      }
      const HealpixLeaf leaf = coordinates::chart_leaf(order, box.face, static_cast<U64>(ix),
                                                        static_cast<U64>(iy));
      auto& entry = ledgers[{leaf.order, leaf.ipix}];
      if (decision == CellDecision::inside) {
        entry.first = entry.first + cell_area_lower;
        entry.second = entry.second + cell_area_upper;
      } else if (decision == CellDecision::undecided) {
        // The whole cell mass is kept as the error bound.
        entry.second = entry.second + cell_area_upper;
        undecided = undecided + cell_area_upper;
      }
    }
  }

  outcome.leaves.reserve(ledgers.size());
  for (const auto& entry : ledgers) {
    ContainmentLeaf leaf;
    leaf.leaf = HealpixLeaf{entry.first.first, entry.first.second};
    leaf.decision = entry.second.first.is_zero() ? CellDecision::outside : CellDecision::inside;
    leaf.lower.signed_integer = entry.second.first.to_string();
    leaf.lower.exponent2 = -DyadicLedger::kScaleBits;
    leaf.upper.signed_integer = entry.second.second.to_string();
    leaf.upper.exponent2 = -DyadicLedger::kScaleBits;
    outcome.leaves.push_back(leaf);
  }
  outcome.undecided_mass.signed_integer = undecided.to_string();
  outcome.undecided_mass.exponent2 = -DyadicLedger::kScaleBits;
  outcome.complete = true;
  return outcome;
}

}  // namespace acsd::geometry
