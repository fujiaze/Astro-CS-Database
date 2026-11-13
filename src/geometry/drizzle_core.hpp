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
//
// Storage of the accumulation: the leaves of one HEALPix order are grouped in
// blocks of 4096 consecutive nested indices.  A touched block owns one dense
// per-leaf aggregate table (eight doubles and a member head per leaf) and only
// touched blocks exist.  The members of every leaf are a singly linked list in
// one flat vector of 20 bytes per (source, leaf) term.  This is the main
// production structure (touched tiles), and it is what makes the automatic
// order of issue #126 feasible: a 4096x4096 frame at order 18 has ~24 million
// leaves and ~80 million (source, leaf) terms, for which a per-leaf std::map
// node plus a per-leaf member vector costs several extra GiB of node and
// allocator overhead, while a flat per-term vector of records needs ~6 GiB
// before the output arrays are even built.
//
// The interval sums of a leaf are evaluated in add() order, exactly as before;
// the CSR columns of the leaf are sorted by source row at finish(), so the
// published enclosures and rows are unchanged by the storage change.
#include <array>
#include <cstdint>
#include <map>
#include <memory>
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

// The bounds the caller enforces on the accumulation (0 = unbounded).  They
// are derived from the certified pixel/leaf scale ratio and are the same
// numbers the invocation plan declares to the resource ledger.
struct DrizzleCapacity {
  U64 maximum_leaves = 0;
  U64 maximum_terms = 0;
};

class DrizzleAccumulator {
 public:
  static constexpr U32 kBlockOrder = 12;
  static constexpr U64 kBlockLeaves = 1ull << kBlockOrder;  // 4096
  static constexpr U64 kBlockMask = kBlockLeaves - 1;

  explicit DrizzleAccumulator(DrizzleCapacity capacity = {});

  // Adds one contribution.  The source value is read by the caller; the
  // contribution must satisfy the same enclosures that compute_overlap
  // publishes (ordered, non-negative areas, source row inside the native
  // values).  A malformed term is rejected instead of being clamped.  When a
  // declared capacity would be exceeded the term is refused with a named
  // resource failure instead of growing past the plan.
  Result<rt::Done> add(const DrizzleTerm& term);

  U64 leaves() const { return leaves_; }
  U64 contributions() const { return sources_.size(); }

  // Builds the per-leaf rows and releases the accumulation storage.  The leaf
  // order is the sorted (order, ipix) order, the row columns are the source
  // rows in ascending order; both are deterministic and independent of the
  // order in which add() was called.
  DrizzleLeafOutput finish();

 private:
  struct Block {
    // Per leaf: the outward enclosures of F and N, their nominal values and
    // the support union.
    std::array<double, kBlockLeaves> f_lower{};
    std::array<double, kBlockLeaves> f_upper{};
    std::array<double, kBlockLeaves> n_lower{};
    std::array<double, kBlockLeaves> n_upper{};
    std::array<double, kBlockLeaves> f_nominal{};
    std::array<double, kBlockLeaves> n_nominal{};
    std::array<double, kBlockLeaves> union_lower{};
    std::array<double, kBlockLeaves> union_upper{};
    std::array<std::uint32_t, kBlockLeaves> member_head{};  // index + 1, 0 = none
    std::array<std::uint8_t, kBlockLeaves> present{};
  };

  static double leaf_solid_angle_upper(U32 order);

  DrizzleCapacity capacity_;
  U64 leaves_ = 0;
  // (order, block index) -> the dense per-leaf aggregate table of that block.
  std::map<std::pair<U32, U64>, std::unique_ptr<Block>> blocks_;
  // The flat member lists: the source row, the certified nominal weight and
  // the next member of the same leaf (index + 1, 0 = end of list).
  std::vector<U64> sources_;
  std::vector<double> weights_;
  std::vector<std::uint32_t> next_member_;
  bool any_drop_lower_not_positive_ = false;
};

}  // namespace acsd::geometry
