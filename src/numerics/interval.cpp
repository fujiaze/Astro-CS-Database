#pragma STDC FENV_ACCESS ON
#include "numerics/interval.hpp"

#include <cfenv>
#include <cstdlib>

namespace acsd::numerics {
namespace {

bool detect_directed_rounding() {
  volatile double a = 1.0;
  volatile double b = 3.0;
  if (std::fesetround(FE_DOWNWARD) != 0) return false;
  const double down = a / b;
  if (std::fesetround(FE_UPWARD) != 0) { std::fesetround(FE_TONEAREST); return false; }
  const double up = a / b;
  std::fesetround(FE_TONEAREST);
  return up > down;
}

// RAII rounding mode switch.
class RoundingGuard {
 public:
  explicit RoundingGuard(int mode) : previous_(std::fegetround()) { std::fesetround(mode); }
  ~RoundingGuard() { std::fesetround(previous_); }
  RoundingGuard(const RoundingGuard&) = delete;
  RoundingGuard& operator=(const RoundingGuard&) = delete;

 private:
  int previous_;
};

const bool kDirected = detect_directed_rounding();

double down(double value) {
  RoundingGuard guard(FE_DOWNWARD);
  return value;
}

double up(double value) {
  RoundingGuard guard(FE_UPWARD);
  return value;
}

double next_down(double value) {
  return std::nextafter(value, -std::numeric_limits<double>::infinity());
}

double next_up(double value) {
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

Interval expand_outward(Interval result) {
  if (!result.is_finite()) return result;
  result.lower = next_down(result.lower);
  result.upper = next_up(result.upper);
  return result;
}

}  // namespace

IntervalPrimitiveStatus interval_primitive_status() {
  return kDirected ? IntervalPrimitiveStatus::directed_rounding
                    : IntervalPrimitiveStatus::outward_ulp_expansion;
}

const char* interval_primitive_status_name() {
  return kDirected ? "directed_rounding" : "outward_ulp_expansion";
}

std::string interval_primitive_note() {
  return kDirected
             ? "interval primitives use the verified hardware directed rounding mode"
             : "interval primitives fall back to outward ulp expansion; enclosures are "
               "approximate rather than certified";
}

Interval interval_point(double value) { return Interval(value, value); }

Interval interval_hull(double a, double b) {
  if (a <= b) return Interval(a, b);
  return Interval(b, a);
}

Interval interval_add(const Interval& a, const Interval& b) {
  if (!kDirected) {
    const double lo = a.lower + b.lower, hi = a.upper + b.upper;
    return expand_outward(Interval(lo, hi));
  }
  return Interval(down(a.lower + b.lower), up(a.upper + b.upper));
}

Interval interval_sub(const Interval& a, const Interval& b) {
  if (!kDirected) {
    const double lo = a.lower - b.upper, hi = a.upper - b.lower;
    return expand_outward(Interval(lo, hi));
  }
  return Interval(down(a.lower - b.upper), up(a.upper - b.lower));
}

Interval interval_mul(const Interval& a, const Interval& b) {
  const double p[4] = {a.lower * b.lower, a.lower * b.upper, a.upper * b.lower, a.upper * b.upper};
  double lo = p[0], hi = p[0];
  for (double v : p) { if (v < lo) lo = v; if (v > hi) hi = v; }
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_div(const Interval& a, const Interval& b) {
  if (b.lower <= 0.0 && b.upper >= 0.0) {
    // Division by an interval containing zero is not bounded.
    return Interval(-std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity());
  }
  const double p[4] = {a.lower / b.lower, a.lower / b.upper, a.upper / b.lower, a.upper / b.upper};
  double lo = p[0], hi = p[0];
  for (double v : p) { if (v < lo) lo = v; if (v > hi) hi = v; }
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_neg(const Interval& a) { return Interval(-a.upper, -a.lower); }

Interval interval_scale(const Interval& a, double factor) {
  return interval_mul(a, Interval(factor, factor));
}

Interval interval_abs(const Interval& a) {
  if (a.lower >= 0.0) return a;
  if (a.upper <= 0.0) return Interval(-a.upper, -a.lower);
  return Interval(0.0, std::max(-a.lower, a.upper));
}

Interval interval_sqrt(const Interval& a) {
  if (a.upper < 0.0) return Interval(-std::numeric_limits<double>::infinity(),
                                      -std::numeric_limits<double>::infinity());
  const double lo = std::sqrt(std::max(0.0, a.lower));
  const double hi = std::sqrt(std::max(0.0, a.upper));
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_exp(const Interval& a) {
  const double lo = std::exp(a.lower), hi = std::exp(a.upper);
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_log(const Interval& a) {
  if (a.lower <= 0.0) {
    return Interval(-std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity());
  }
  const double lo = std::log(a.lower), hi = std::log(a.upper);
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_sin(const Interval& a) {
  if (a.width() >= 2.0 * M_PI) return Interval(-1.0, 1.0);
  const double lo = std::sin(a.lower), hi = std::sin(a.upper);
  Interval result = interval_hull(lo, hi);
  // Local extremum inside the interval may exceed the endpoint values.
  for (double k = -8.0; k <= 8.0; ++k) {
    const double candidate = M_PI / 2.0 + k * M_PI;
    if (candidate > a.lower && candidate < a.upper) result.upper = 1.0;
    const double candidate2 = -M_PI / 2.0 + k * M_PI;
    if (candidate2 > a.lower && candidate2 < a.upper) result.lower = -1.0;
  }
  if (!kDirected) return expand_outward(result);
  return Interval(down(result.lower), up(result.upper));
}

Interval interval_cos(const Interval& a) {
  if (a.width() >= 2.0 * M_PI) return Interval(-1.0, 1.0);
  const double lo = std::cos(a.lower), hi = std::cos(a.upper);
  Interval result = interval_hull(lo, hi);
  for (double k = -8.0; k <= 8.0; ++k) {
    const double candidate = k * M_PI;
    if (candidate > a.lower && candidate < a.upper) result.upper = 1.0;
    const double candidate2 = M_PI + k * M_PI;
    if (candidate2 > a.lower && candidate2 < a.upper) result.lower = -1.0;
  }
  if (!kDirected) return expand_outward(result);
  return Interval(down(result.lower), up(result.upper));
}

Interval interval_atan(const Interval& a) {
  const double lo = std::atan(a.lower), hi = std::atan(a.upper);
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_atan2(const Interval& y, const Interval& x) {
  double lo = 0.0, hi = 0.0;
  bool first = true;
  const double ys[2] = {y.lower, y.upper};
  const double xs[2] = {x.lower, x.upper};
  for (double yy : ys) {
    for (double xx : xs) {
      const double v = std::atan2(yy, xx);
      if (first) { lo = hi = v; first = false; }
      else { lo = std::min(lo, v); hi = std::max(hi, v); }
    }
  }
  if (!kDirected) return expand_outward(Interval(lo, hi));
  return Interval(down(lo), up(hi));
}

Interval interval_pow(const Interval& base, const Interval& exponent) {
  if (base.lower > 0.0) {
    const double lo = std::pow(base.lower, exponent.upper);
    const double hi = std::pow(base.upper, exponent.lower);
    if (!kDirected) return expand_outward(Interval(lo, hi));
    return Interval(down(lo), up(hi));
  }
  if (exponent.lower == exponent.upper && exponent.lower == 2.0) {
    return interval_mul(base, base);
  }
  return Interval(-std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity());
}

Interval interval_intersect(const Interval& a, const Interval& b) {
  const double lo = std::max(a.lower, b.lower);
  const double hi = std::min(a.upper, b.upper);
  if (lo > hi) return Interval(1.0, -1.0);  // empty marker
  return Interval(lo, hi);
}

Interval interval_hull(const Interval& a, const Interval& b) {
  return Interval(std::min(a.lower, b.lower), std::max(a.upper, b.upper));
}

bool interval_disjoint(const Interval& a, const Interval& b) {
  return a.upper < b.lower || b.upper < a.lower;
}

double interval_lower(const Interval& a) { return a.lower; }
double interval_upper(const Interval& a) { return a.upper; }

}  // namespace acsd::numerics
