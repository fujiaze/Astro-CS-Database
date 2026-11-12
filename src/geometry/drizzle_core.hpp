#pragma once
// The single implementation of the spherical drizzle accumulation.
//
// Both entry points of the geometry module use this accumulator:
//
//   * build_drizzle(...)        walks the Overlaps contributions of a
//                               general (possibly large) detector footprint;
//   * drizzle_grid(...)         walks the pixel grid directly and never
//                               materialises one OverlapContribution per
//                               pixel (the 4096x4096/order-13 products need
//                               ~17 million sources, and the per-source
//                               interval records do not fit the 12 GiB
//                               budget).
//
// The definition of the estimand (SPHERICAL_DRIZZLE) is here once:
//   w_jp = a_jp / A_j,  F_p = sum_j w_jp x_j,  N_p = sum_j w_jp Omega_j,
//   B_p = F_p / N_p,   R_pj = w_jp / N_p,
// with the same outward interval enclosure for a_jp, A_j and Omega_j.
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <acsd/runtime/task.hpp>

#include "acsd/contracts.hpp"


#include "acsd/model.hpp"

namespace acsd::geometry {

// One (source, leaf) contribution.  The areas are the three enclosures that
// the overlap owner (kernel or pixel grid) publishes: the overlap of the drop
// region with the leaf, the drop region itself and the native source region.
struct DrizzleTerm {
  U64 source_row = 0;
  U32 order = 0;
  U64 ipix = 0;
  double overlap_lower = 0.0;
  double overlap_upper = 0.0;
  double drop_lower = 0.0;
  double drop_upper = 0.0;
  double omega_lower = 0.0;
  double omega_upper = 0.0;
  double value = 0.0;
};

// The numeric payload of the drizzle.  The caller attaches the model objects
// (index, support map, response, error record, assessment) because those carry
// schema identities that are not part of the estimator.
struct DrizzleLeafOutput {
  std::vector<HealpixLeaf> leaves;
  std::vector<double> normalizer;
  std::vector<double> signal;
  std::vector<double> signal_lower;
  std::vector<double> signal_upper;
  std::vector<std::uint8_t> support_states;
  std::vector<std::uint8_t> normalization_states;
  std::vector<std::uint8_t> observational_states;
  std::vector<double> area_lower;
  std::vector<double> area_upper;
  std::vector<U64> row_offsets;
  std::vector<U64> row_indices;
  std::vector<double> row_values;
  bool any_unresolved = false;
  bool any_absent = false;
  bool any_present = false;
  bool any_missing_error = false;
  bool any_drop_lower_not_positive = false;
};

class DrizzleAccumulator {
 public:
  DrizzleAccumulator();

  // Adds one contribution.  The source value is read by the caller; the
  // contribution must satisfy the same enclosures that compute_overlap
  // publishes (ordered, non-negative areas, source row inside the native
  // values).  A malformed term is rejected instead of being clamped.
  Result<rt::Done> add(const DrizzleTerm& term);

  // Builds the per-leaf rows.  The leaf order is the sorted (order, ipix)
  // order, the row columns are the source rows in ascending order; both are
  // deterministic and independent of the order in which add() was called.
  DrizzleLeafOutput finish() const;

 private:
  struct LeafState {
    double f_lower = 0.0;
    double f_upper = 0.0;
    double n_lower = 0.0;
    double n_upper = 0.0;
    double f_nominal = 0.0;
    double n_nominal = 0.0;
    double union_lower = 0.0;
    double union_upper = 0.0;
    std::vector<std::pair<U64, double>> members;
  };
  std::map<std::pair<U32, U64>, LeafState> leaves_;
  bool any_drop_lower_not_positive_ = false;
};

}  // namespace acsd::geometry
