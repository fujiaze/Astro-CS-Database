#pragma once
// Directed-rounding interval arithmetic.
//
// The interval primitives use the hardware rounding mode to compute rigorous
// outward enclosures.  A runtime self test verifies on the actual platform
// that the rounding mode is honoured; when it is not, the implementation
// falls back to an outward ulp expansion and reports the weaker guarantee
// through `interval_primitive_status()` so that callers can downgrade a
// certificate from `certified` to `approximate` instead of over-claiming.
#include <cmath>
#include <limits>
#include <string>

namespace acsd::numerics {

struct Interval {
  double lower = 0.0;
  double upper = 0.0;

  Interval() = default;
  Interval(double lo, double hi) : lower(lo), upper(hi) {}
  double width() const { return upper - lower; }
  double midpoint() const { return 0.5 * (lower + upper); }
  bool contains(double value) const { return lower <= value && value <= upper; }
  bool contains(const Interval& other) const {
    return lower <= other.lower && upper >= other.upper;
  }
  bool is_finite() const { return std::isfinite(lower) && std::isfinite(upper); }
};

enum class IntervalPrimitiveStatus {
  directed_rounding,     // hardware rounding mode verified on this platform
  outward_ulp_expansion  // fallback: enclosure widened by one ulp per operation
};

IntervalPrimitiveStatus interval_primitive_status();
const char* interval_primitive_status_name();
std::string interval_primitive_note();

Interval interval_point(double value);
Interval interval_hull(double a, double b);
Interval interval_add(const Interval& a, const Interval& b);
Interval interval_sub(const Interval& a, const Interval& b);
Interval interval_mul(const Interval& a, const Interval& b);
Interval interval_div(const Interval& a, const Interval& b);
Interval interval_neg(const Interval& a);
Interval interval_scale(const Interval& a, double factor);
Interval interval_abs(const Interval& a);
Interval interval_sqrt(const Interval& a);
Interval interval_exp(const Interval& a);
Interval interval_log(const Interval& a);
Interval interval_sin(const Interval& a);
Interval interval_cos(const Interval& a);
Interval interval_atan(const Interval& a);
Interval interval_atan2(const Interval& y, const Interval& x);
Interval interval_pow(const Interval& base, const Interval& exponent);

// Intersection; returns an empty interval when the inputs are disjoint.
Interval interval_intersect(const Interval& a, const Interval& b);
Interval interval_hull(const Interval& a, const Interval& b);
bool interval_disjoint(const Interval& a, const Interval& b);
double interval_lower(const Interval& a);
double interval_upper(const Interval& a);

}  // namespace acsd::numerics
