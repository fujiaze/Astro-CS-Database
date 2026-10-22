#pragma once
// Production overlap kernel: affine-strip cut cell with exact rational
// clipping (SPHERICAL_DRIZZLE section 6) plus the target domain containment
// fallback of the same document (section 3, algorithm A).
//
// The kernel never replaces an undecided mass by zero: a block that cannot be
// decided inside the budget keeps its full mass as an unresolved domain.
#include <array>
#include <map>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/exact.hpp"

namespace acsd::geometry {

using model::BigInt;
using model::Rational;

// Exact dyadic ledger.
//
// Every mass is quantised outward onto the fixed grid 2^-120 before it enters
// the ledger (SPHERICAL_DRIZZLE section 6: the accumulated masses are kept on
// a fixed dyadic grid so that the sum never loses a bit and the denominators
// of the exact clipping cannot explode).  The running sum is an exact integer
// on that grid.
class DyadicLedger {
 public:
  static constexpr int kScaleBits = 120;

  void add(const Rational& value, bool upper);
  void add(const Dyadic& value, bool upper);
  bool empty() const { return empty_; }
  // The accumulated value as an exact dyadic.
  Dyadic dyadic() const;
  // The accumulated integer at the grid 2^-kScaleBits.
  const BigInt& scaled() const { return sum_; }
  // The accumulated value converted to binary64 for display purposes only.
  double to_double() const;

 private:
  BigInt sum_;
  bool empty_ = true;
};

struct LeafAreaBound {
  HealpixLeaf leaf;
  Dyadic lower;
  Dyadic upper;
};

struct KernelOutcome {
  std::vector<LeafAreaBound> leaves;
  std::vector<UnresolvedDomain> unresolved;
  Termination termination = Termination::tolerance_met;
  U64 work = 0;
  std::string reason;
  Dyadic total_lower;
  Dyadic total_upper;
  Dyadic unresolved_lower;
  Dyadic unresolved_upper;
};

// Overlap of a rectangular detector region with the leaves of the given order.
KernelOutcome cut_cell_overlap(const WcsModel& wcs, const Box& detector_rect, U32 order,
                               const GeometryBudget& budget);

// Area enclosure of a set of detector rings (outer rings counter clockwise,
// holes clockwise).  The result is an exact dyadic interval in steradian.
struct AreaBound {
  Dyadic lower;
  Dyadic upper;
};
AreaBound footprint_area(const WcsModel& wcs,
                         const std::vector<std::vector<std::array<double, 2>>>& rings);

// The exact rational area bounds behind an AreaBound.
struct RationalAreaBound {
  Rational lower;
  Rational upper;
};
RationalAreaBound footprint_area_rational(const WcsModel& wcs,
                                          const std::vector<std::vector<std::array<double, 2>>>& rings);

// Converts an exact dyadic into the binary64 value used for display.
double dyadic_to_double(const Dyadic& value);

// Converts an exact rational into the fixed dyadic grid, outward.
Dyadic rational_to_dyadic(const Rational& value, bool upper);

}  // namespace acsd::geometry
