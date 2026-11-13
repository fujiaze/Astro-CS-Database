#include "geometry/drizzle_core.hpp"

#include "coordinates/healpix.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "model/result.hpp"
#include "numerics/interval.hpp"

namespace acsd::geometry {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The directed-rounding interval helpers of SPHERICAL_DRIZZLE.  They are the
// same primitives used by build_drizzle; the science formula below is the only
// consumer, so there is no second enclosure implementation.
struct Iv {
  double lower = 0.0;
  double upper = 0.0;
};

numerics::Interval as_interval(const Iv& value) {
  return numerics::Interval(value.lower, value.upper);
}

Iv iv_mul(const Iv& a, const Iv& b) {
  const numerics::Interval product = numerics::interval_mul(as_interval(a), as_interval(b));
  return Iv{product.lower, product.upper};
}

Iv iv_add(const Iv& a, const Iv& b) {
  const numerics::Interval sum = numerics::interval_add(as_interval(a), as_interval(b));
  return Iv{sum.lower, sum.upper};
}

Iv iv_div(const Iv& a, const Iv& b) {
  if (!(std::fabs(b.lower) > 0.0) || b.lower <= 0.0) {
    return Iv{-std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()};
  }
  const numerics::Interval quotient = numerics::interval_div(as_interval(a), as_interval(b));
  return Iv{quotient.lower, quotient.upper};
}

double midpoint(const Iv& value) { return 0.5 * (value.lower + value.upper); }

}  // namespace

DrizzleAccumulator::DrizzleAccumulator(DrizzleCapacity capacity) : capacity_(capacity) {}

double DrizzleAccumulator::leaf_solid_angle_upper(U32 order) {
  // An upper bound of the solid angle of one true leaf: Omega_H = pi / (3 N^2)
  // (SPHERICAL_DRIZZLE Eq. (4)), rounded upward.  It bounds every overlap mass
  // a_jp from above and caps the support union.
  const double nside = static_cast<double>(1ull << order);
  const double inf = std::numeric_limits<double>::infinity();
  const double pi_upper = std::nextafter(kPi, inf);
  const double denominator = 3.0 * nside * nside;
  return std::nextafter(pi_upper / denominator, inf);
}

Result<rt::Done> DrizzleAccumulator::add(const DrizzleTerm& term) {
  using model::fail;
  const Iv overlap{term.overlap_lower, term.overlap_upper};
  const Iv drop{term.drop_lower, term.drop_upper};
  const Iv omega{term.omega_lower, term.omega_upper};
  if (!(overlap.lower <= overlap.upper) || !(drop.lower <= drop.upper) ||
      !(omega.lower <= omega.upper)) {
    return fail<rt::Done>(Code::invalid_model, "invalid_area_interval", "drizzle.add",
                          "the overlap, drop or native area enclosure is inverted");
  }
  if (overlap.lower < 0.0 || drop.upper < 0.0 || omega.lower < 0.0) {
    return fail<rt::Done>(Code::invalid_model, "negative_area", "drizzle.add",
                          "an area enclosure is negative");
  }
  Iv weight;
  if (drop.lower > 0.0) {
    weight.lower = overlap.lower / drop.upper;
    weight.upper = std::min(1.0, overlap.upper / drop.lower);
    if (!(weight.lower <= weight.upper)) {
      return fail<rt::Done>(Code::invalid_model, "empty_weight_interval", "drizzle.add",
                            "the overlap enclosure does not intersect the drop enclosure");
    }
  } else if (drop.upper > 0.0) {
    // Only A > 0 is certified (its lower bound rounded to zero): w = a/A is
    // still bounded below by a_L/A_U and above by 1.
    weight.lower = overlap.lower / drop.upper;
    weight.upper = 1.0;
  } else {
    // A is not bounded above at all: [0,1] is the only valid enclosure.
    weight = Iv{0.0, 1.0};
  }
  if (term.order > coordinates::kMaxOrder) {
    return fail<rt::Done>(Code::invalid_request, "order_out_of_range", "drizzle.add");
  }
  const U64 block_index = term.ipix >> kBlockOrder;
  const U64 slot = term.ipix & kBlockMask;
  const auto key = std::make_pair(term.order, block_index);
  auto found = blocks_.find(key);
  if (found == blocks_.end()) {
    auto created = std::make_unique<Block>();
    found = blocks_.emplace(key, std::move(created)).first;
  }
  Block& block = *found->second;
  const std::size_t leaf_slot = static_cast<std::size_t>(slot);
  if (block.present[leaf_slot] == 0) {
    // The leaf cap is enforced for every new leaf, not only when a new block is
    // created: a block holds up to 4096 leaves, and the former check let a
    // single block exceed the declared cap by up to 4095 leaves.
    if (capacity_.maximum_leaves != 0 && leaves_ + 1 > capacity_.maximum_leaves) {
      return fail<rt::Done>(Code::resource_exhausted, "drizzle_leaf_capacity_exhausted",
                            "drizzle.add",
                            "the accumulation would exceed the leaf capacity of the plan");
    }
    block.present[leaf_slot] = 1;
    block.member_head[leaf_slot] = 0;
    ++leaves_;
  }
  // The interval sums of one leaf are accumulated in add() order, exactly as
  // the former per-leaf map did.
  const Iv f_term = iv_mul(weight, Iv{term.value, term.value});
  const Iv f_sum = iv_add(Iv{block.f_lower[leaf_slot], block.f_upper[leaf_slot]}, f_term);
  block.f_lower[leaf_slot] = f_sum.lower;
  block.f_upper[leaf_slot] = f_sum.upper;
  const Iv n_term = iv_mul(weight, omega);
  const Iv n_sum = iv_add(Iv{block.n_lower[leaf_slot], block.n_upper[leaf_slot]}, n_term);
  block.n_lower[leaf_slot] = n_sum.lower;
  block.n_upper[leaf_slot] = n_sum.upper;
  block.f_nominal[leaf_slot] += midpoint(weight) * term.value;
  block.n_nominal[leaf_slot] += midpoint(weight) * midpoint(omega);
  block.union_lower[leaf_slot] = std::max(block.union_lower[leaf_slot], overlap.lower);
  block.union_upper[leaf_slot] =
      iv_add(Iv{block.union_upper[leaf_slot], block.union_upper[leaf_slot]},
             Iv{overlap.upper, overlap.upper})
          .upper;
  if (capacity_.maximum_terms != 0 && sources_.size() + 1 > capacity_.maximum_terms) {
    return fail<rt::Done>(Code::resource_exhausted, "drizzle_term_capacity_exhausted",
                          "drizzle.add",
                          "the accumulation would exceed the term capacity of the plan");
  }
  if (sources_.size() >= 0xffffffffull) {
    return fail<rt::Done>(Code::resource_exhausted, "drizzle_member_cursor_overflow",
                          "drizzle.add",
                          "the flat member list exceeds the 32 bit leaf cursor");
  }
  sources_.push_back(term.source_row);
  weights_.push_back(midpoint(weight));
  next_member_.push_back(block.member_head[leaf_slot]);
  block.member_head[leaf_slot] = static_cast<std::uint32_t>(sources_.size());
  if (drop.lower <= 0.0) any_drop_lower_not_positive_ = true;
  return model::ok_result(rt::Done{});
}

DrizzleLeafOutput DrizzleAccumulator::finish() {
  DrizzleLeafOutput out;
  out.any_drop_lower_not_positive = any_drop_lower_not_positive_;
  out.row_offsets.push_back(0);
  std::vector<std::pair<U64, double>> members;
  for (const auto& entry : blocks_) {
    const U32 order = entry.first.first;
    const U64 block_index = entry.first.second;
    const Block& block = *entry.second;
    for (std::size_t slot = 0; slot < static_cast<std::size_t>(kBlockLeaves); ++slot) {
      if (block.present[slot] == 0) continue;
      const HealpixLeaf leaf{order, block_index * kBlockLeaves + slot};
      const Iv f{block.f_lower[slot], block.f_upper[slot]};
      const Iv n{block.n_lower[slot], block.n_upper[slot]};
      const double n_nominal = block.n_nominal[slot];
      out.leaves.push_back(leaf);
      out.area_lower.push_back(block.union_lower[slot]);
      out.area_upper.push_back(std::min(block.union_upper[slot],
                                        leaf_solid_angle_upper(order)));
      if (!(n.upper > 0.0)) {
        out.any_absent = true;
        out.normalizer.push_back(0.0);
        out.signal.push_back(std::numeric_limits<double>::quiet_NaN());
        out.signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
        out.signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
        out.support_states.push_back(0);
        out.normalization_states.push_back(0);
        out.observational_states.push_back(1);
        out.row_offsets.push_back(static_cast<U64>(out.row_indices.size()));
        continue;
      }
      if (!(n.lower > 0.0)) {
        out.any_unresolved = true;
        out.normalizer.push_back(std::numeric_limits<double>::quiet_NaN());
        out.signal.push_back(std::numeric_limits<double>::quiet_NaN());
        out.signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
        out.signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
        out.support_states.push_back(2);
        out.normalization_states.push_back(2);
        out.observational_states.push_back(1);
        out.row_offsets.push_back(static_cast<U64>(out.row_indices.size()));
        continue;
      }
      out.any_present = true;
      const Iv brightness = iv_div(f, n);
      out.normalizer.push_back(n_nominal);
      out.signal.push_back(block.f_nominal[slot] / n_nominal);
      out.signal_lower.push_back(brightness.lower);
      out.signal_upper.push_back(brightness.upper);
      out.support_states.push_back(1);
      out.normalization_states.push_back(1);
      out.observational_states.push_back(1);
      if (!std::isfinite(brightness.lower) || !std::isfinite(brightness.upper)) {
        out.any_missing_error = true;
      }
      // R_pj = w_jp / N_p, merged per source row in ascending order.
      members.clear();
      std::uint32_t cursor = block.member_head[slot];
      while (cursor != 0) {
        const std::size_t member = static_cast<std::size_t>(cursor) - 1;
        members.emplace_back(sources_[member], weights_[member]);
        cursor = next_member_[member];
      }
      std::sort(members.begin(), members.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });
      std::size_t index = 0;
      while (index < members.size()) {
        const U64 source = members[index].first;
        double r = 0.0;
        while (index < members.size() && members[index].first == source) {
          r += members[index].second / n_nominal;
          ++index;
        }
        out.row_indices.push_back(source);
        out.row_values.push_back(r);
      }
      out.row_offsets.push_back(static_cast<U64>(out.row_indices.size()));
    }
  }
  // Release the accumulation before the caller packages the output arrays.
  blocks_.clear();
  std::vector<U64>().swap(sources_);
  std::vector<double>().swap(weights_);
  std::vector<std::uint32_t>().swap(next_member_);
  return out;
}

}  // namespace acsd::geometry
