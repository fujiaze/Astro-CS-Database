#include "geometry/drizzle_core.hpp"

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

// An upper bound of the solid angle of one true leaf: Omega_H = pi / (3 N^2)
// (SPHERICAL_DRIZZLE Eq. (4)), rounded upward.  It bounds every overlap mass
// a_jp from above and caps the support union.
double leaf_solid_angle_upper(U32 order) {
  const double nside = static_cast<double>(1ull << order);
  const double inf = std::numeric_limits<double>::infinity();
  const double pi_upper = std::nextafter(kPi, inf);
  const double denominator = 3.0 * nside * nside;
  return std::nextafter(pi_upper / denominator, inf);
}

}  // namespace

DrizzleAccumulator::DrizzleAccumulator() = default;

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
  const double weight_nominal = midpoint(weight);

  LeafState& leaf = leaves_[{term.order, term.ipix}];
  const Iv f_term = iv_mul(weight, Iv{term.value, term.value});
  const Iv f_current{leaf.f_lower, leaf.f_upper};
  const Iv f_next = iv_add(f_current, f_term);
  leaf.f_lower = f_next.lower;
  leaf.f_upper = f_next.upper;
  const Iv n_term = iv_mul(weight, omega);
  const Iv n_current = iv_add(Iv{leaf.n_lower, leaf.n_upper}, n_term);
  leaf.n_lower = n_current.lower;
  leaf.n_upper = n_current.upper;
  leaf.f_nominal += weight_nominal * term.value;
  leaf.n_nominal += weight_nominal * midpoint(omega);
  leaf.union_lower = std::max(leaf.union_lower, overlap.lower);
  const Iv union_next = iv_add(Iv{leaf.union_upper, leaf.union_upper},
                            Iv{overlap.upper, overlap.upper});
  leaf.union_upper = union_next.upper;
  leaf.members.emplace_back(term.source_row, weight_nominal);
  if (drop.lower <= 0.0) any_drop_lower_not_positive_ = true;
  return model::ok_result(rt::Done{});
}

DrizzleLeafOutput DrizzleAccumulator::finish() const {
  DrizzleLeafOutput out;
  out.any_drop_lower_not_positive = any_drop_lower_not_positive_;
  out.row_offsets.push_back(0);
  for (const auto& entry : leaves_) {
    const HealpixLeaf leaf{entry.first.first, entry.first.second};
    const LeafState& state = entry.second;
    out.leaves.push_back(leaf);
    const double omega_leaf = leaf_solid_angle_upper(leaf.order);
    out.area_lower.push_back(state.union_lower);
    out.area_upper.push_back(std::min(state.union_upper, omega_leaf));

    const Iv f{state.f_lower, state.f_upper};
    const Iv n{state.n_lower, state.n_upper};
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
    const double brightness_nominal = state.f_nominal / state.n_nominal;
    out.normalizer.push_back(state.n_nominal);
    out.signal.push_back(brightness_nominal);
    out.signal_lower.push_back(brightness.lower);
    out.signal_upper.push_back(brightness.upper);
    out.support_states.push_back(1);
    out.normalization_states.push_back(1);
    out.observational_states.push_back(1);
    if (!std::isfinite(brightness.lower) || !std::isfinite(brightness.upper)) {
      out.any_missing_error = true;
    }
    // R_pj = w_jp / N_p, merged per source row in ascending order.
    std::vector<std::pair<U64, double>> members = state.members;
    std::sort(members.begin(), members.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::size_t index = 0;
    while (index < members.size()) {
      const U64 source = members[index].first;
      double r = 0.0;
      while (index < members.size() && members[index].first == source) {
        r += members[index].second / state.n_nominal;
        ++index;
      }
      out.row_indices.push_back(source);
      out.row_values.push_back(r);
    }
    out.row_offsets.push_back(static_cast<U64>(out.row_indices.size()));
  }
  return out;
}

}  // namespace acsd::geometry
