#pragma once
// Production overlap kernel: affine-strip cut cell with exact rational
// clipping (SPHERICAL_DRIZZLE section 6) plus the target domain containment
// fallback of the same document (section 3, algorithm A).
//
// The kernel never replaces an undecided mass by zero: a block that cannot be
// decided inside the budget keeps its full mass as an unresolved domain.
#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/exact.hpp"
#include "model/hash.hpp"

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

// The chart enclosure of a detector box: the face of the equal-area chart and
// the enclosure of the chart coordinates over the box.
struct ChartBox {
  bool valid = false;
  U32 face = 0;
  double s_lo = 0.0;
  double s_hi = 0.0;
  double t_lo = 0.0;
  double t_hi = 0.0;
  double e_s = 0.0;
  double e_t = 0.0;
};
bool chart_enclosure(const WcsModel& wcs, double u0, double u1, double v0, double v1, ChartBox& out);

// Overlap of a rectangular detector region with the leaves of the given order.
//
// The work is charged to the shared ledger of the context (one unit per
// processed block plus one unit per candidate cell), so that the caller sees
// the real cost of the geometry and the shared budget bounds the kernel.
KernelOutcome cut_cell_overlap(Context& context, const WcsModel& wcs, const Box& detector_rect,
                               U32 order, const GeometryBudget& budget);

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

// The larger and the smaller of two exact dyadics.
Dyadic dyadic_max(const Dyadic& a, const Dyadic& b);
Dyadic dyadic_min(const Dyadic& a, const Dyadic& b);

// Exact dyadic arithmetic for the conservation checks of the area ledger
// (sum(a_p) + coarse - A = 0 and a_p <= min(A, Omega_H)); the operands are
// brought onto the common finer grid, so the result is exact.
Dyadic dyadic_add(const Dyadic& a, const Dyadic& b);
Dyadic dyadic_sub(const Dyadic& a, const Dyadic& b);
int dyadic_compare(const Dyadic& a, const Dyadic& b);

// Deterministic content digest of a geometry object (P0-3).
//
// Every field enters the SHA-256 stream with its name, so two different
// objects cannot alias by concatenation, and a change of any geometry input
// (order, pixfrac, primitive, constraint, revision dependency) changes the
// content hash.  The digest is the only source of the object_id of the
// geometry/drizzle objects; a constant string made every geometry revision
// hash-identical and disabled the dependency invalidation.
class ContentDigest {
 public:
  ContentDigest();
  void add(std::string_view key, std::string_view value);
  void add_u64(std::string_view key, U64 value);
  void add_i64(std::string_view key, std::int64_t value);
  void add_f64(std::string_view key, double value);
  void add_leaf(std::string_view key, const HealpixLeaf& leaf);
  void add_dyadic(std::string_view key, const Dyadic& value);
  void add_area(std::string_view key, const SphericalAreaInterval& area);
  // The exact payload of a CSR operator (offsets, ascending columns, values).
  // The LinearAction of the operators module does not carry an identity of its
  // own (its Model base identity is left empty by make_csr_action), so the
  // geometry digests its coefficients directly.
  void add_csr(std::string_view key, const std::vector<U64>& offsets,
               const std::vector<U64>& indices, const std::vector<double>& values);
  std::string hex() const;

 private:
  model::Sha256 hash_;
};

}  // namespace acsd::geometry
