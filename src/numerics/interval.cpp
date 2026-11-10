#pragma STDC FENV_ACCESS ON
// Directed-rounding interval arithmetic.
//
// Arithmetic contract (docs/engineering/rebuild/standards/NUMERICS.md):
//   * a binary64 input denotes its exact binary rational value;
//   * for +, -, *, / and sqrt the enclosing endpoints are computed *inside*
//     the directed floating-point rounding mode.  Passing an already rounded
//     double to a helper that changes the rounding mode afterwards is not an
//     enclosure, so every primitive below performs the operation itself after
//     the mode has been installed;
//   * when the platform cannot be shown to honour the directed rounding mode,
//     the implementation falls back to a one-ulp outward expansion of the
//     round-to-nearest result and reports `outward_ulp_expansion`.  That
//     fallback is still an enclosure for +, -, *, / and sqrt because the
//     exact result lies within half an ulp of the rounded one;
//   * sin, cos, exp, log, atan, atan2 and pow are *not* covered by the
//     directed hardware mode.  They are evaluated in long double and widened
//     by a conservative margin; their guarantee is stated separately by
//     `interval_primitive_note`.  They are never used to certify a production
//     geometric or photometric path; a consumer that needs a certified
//     enclosure for one of them must provide a directed multiprecision
//     implementation.
#include "numerics/interval.hpp"

#include <cfenv>
#include <cstdlib>
#include <string>

#if defined(__FAST_MATH__)
#error "interval arithmetic requires -fno-fast-math: directed rounding must be observable"
#endif

namespace acsd::numerics {
namespace {

bool detect_directed_rounding() {
  volatile double a = 0.1;
  volatile double b = 0.2;
  volatile double one = 1.0;
  volatile double three = 3.0;
  if (std::fesetround(FE_DOWNWARD) != 0) return false;
  const double sum_down = a + b;
  const double quot_down = one / three;
  if (std::fesetround(FE_UPWARD) != 0) {
    std::fesetround(FE_TONEAREST);
    return false;
  }
  const double sum_up = a + b;
  const double quot_up = one / three;
  std::fesetround(FE_TONEAREST);
  const double sum_nearest = a + b;
  const double quot_nearest = one / three;
  return sum_down < sum_nearest && sum_nearest < sum_up && quot_down < quot_nearest &&
         quot_nearest < quot_up;
}

// The rounding mode is thread state.  A guard restores the previous mode even
// when an operation leaves through an unexpected path.
class RoundingGuard {
 public:
  explicit RoundingGuard(int mode) : previous_(std::fegetround()) {
    (void)std::fesetround(mode);
  }
  ~RoundingGuard() { (void)std::fesetround(previous_); }
  RoundingGuard(const RoundingGuard&) = delete;
  RoundingGuard& operator=(const RoundingGuard&) = delete;

 private:
  int previous_;
};

const bool kDirected = detect_directed_rounding();

double add_down(double a, double b) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = a + b;
  return result;
}
double add_up(double a, double b) {
  RoundingGuard guard(FE_UPWARD);
  volatile double result = a + b;
  return result;
}
double sub_down(double a, double b) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = a - b;
  return result;
}
double sub_up(double a, double b) {
  RoundingGuard guard(FE_UPWARD);
  volatile double result = a - b;
  return result;
}
double mul_down(double a, double b) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = a * b;
  return result;
}
double mul_up(double a, double b) {
  RoundingGuard guard(FE_UPWARD);
  volatile double result = a * b;
  return result;
}
double div_down(double a, double b) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = a / b;
  return result;
}
double div_up(double a, double b) {
  RoundingGuard guard(FE_UPWARD);
  volatile double result = a / b;
  return result;
}
double sqrt_down(double a) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = std::sqrt(a);
  return result;
}
double sqrt_up(double a) {
  RoundingGuard guard(FE_UPWARD);
  volatile double result = std::sqrt(a);
  return result;
}

double next_down(double value) {
  return std::nextafter(value, -std::numeric_limits<double>::infinity());
}

double next_up(double value) {
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

bool has_nan(double value) { return std::isnan(value); }

Interval whole_line() {
  return Interval(-std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity());
}

Interval invalid_domain() {
  // The public Interval has no separate empty representation.  A reversed
  // interval is the documented invalid/empty marker (see interval_intersect).
  return Interval(1.0, -1.0);
}

// Fallback for a platform whose binary64 arithmetic does not honour the
// rounding mode.  One ulp on each side contains the exact result of one
// correctly rounded operation (half an ulp) plus the representation error.
Interval widen(Interval value) {
  if (!value.is_finite()) return value;
  return Interval(next_down(value.lower), next_up(value.upper));
}

bool any_nan4(const double (&values)[4]) {
  for (double value : values) {
    if (has_nan(value)) return true;
  }
  return false;
}

constexpr long double kPi = 3.141592653589793238462643383279502884L;
constexpr long double kHalfPi = kPi / 2.0L;

// The transcendental bounds below are evaluated in long double and widened by
// this margin.  It is many orders of magnitude larger than the observed error
// of the platform libm (the glibc functions are faithfully rounded), so the
// result is a conservative enclosure *provided* the platform libm error stays
// below the margin.  This is a conditional property, not a directed-rounding
// certificate.
constexpr long double kLibmMargin = 1.0e-15L;

// Integer power of an interval by binary exponentiation.  The caller has
// already established that the exponent is an exact integer and that the
// domain is legal.
Interval integer_power(const Interval& base, long long exponent) {
  const bool negative = exponent < 0;
  unsigned long long n =
      negative ? static_cast<unsigned long long>(-(exponent + 1)) + 1ULL
               : static_cast<unsigned long long>(exponent);
  Interval result(1.0, 1.0);
  Interval factor = base;
  while (n > 0) {
    if (n & 1ULL) result = interval_mul(result, factor);
    n >>= 1ULL;
    if (n > 0) factor = interval_mul(factor, factor);
  }
  if (negative) return interval_div(Interval(1.0, 1.0), result);
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
             ? "arithmetic primitives (+,-,*,/,sqrt) execute inside the hardware directed "
               "rounding mode and enclose the exact binary64 result; sin/cos/exp/log/atan/"
               "atan2/pow are evaluated in long double with a 1e-15 conservative margin and "
               "are NOT directed-rounding certified"
             : "arithmetic primitives fall back to a one-ulp outward expansion of the "
               "round-to-nearest result; enclosures are conditional and sin/cos/exp/log/"
               "atan/atan2/pow are conservative long-double bounds, not certified";
}

Interval interval_point(double value) { return Interval(value, value); }

Interval interval_hull(double a, double b) {
  if (has_nan(a) || has_nan(b)) return whole_line();
  if (a <= b) return Interval(a, b);
  return Interval(b, a);
}

Interval interval_add(const Interval& a, const Interval& b) {
  if (!kDirected) {
    const double lo = a.lower + b.lower, hi = a.upper + b.upper;
    if (has_nan(lo) || has_nan(hi)) return whole_line();
    return widen(Interval(lo, hi));
  }
  if ((std::isinf(a.lower) && a.lower < 0 && std::isinf(b.upper) && b.upper > 0) ||
      (std::isinf(a.upper) && a.upper > 0 && std::isinf(b.lower) && b.lower < 0)) {
    return whole_line();
  }
  const double lo = add_down(a.lower, b.lower);
  const double hi = add_up(a.upper, b.upper);
  if (has_nan(lo) || has_nan(hi)) return whole_line();
  return Interval(lo, hi);
}

Interval interval_sub(const Interval& a, const Interval& b) {
  if (!kDirected) {
    const double lo = a.lower - b.upper, hi = a.upper - b.lower;
    if (has_nan(lo) || has_nan(hi)) return whole_line();
    return widen(Interval(lo, hi));
  }
  const double lo = sub_down(a.lower, b.upper);
  const double hi = sub_up(a.upper, b.lower);
  if (has_nan(lo) || has_nan(hi)) return whole_line();
  return Interval(lo, hi);
}

Interval interval_mul(const Interval& a, const Interval& b) {
  if (!kDirected) {
    const double products[4] = {a.lower * b.lower, a.lower * b.upper, a.upper * b.lower,
                                a.upper * b.upper};
    if (any_nan4(products)) return whole_line();
    double lo = products[0], hi = products[0];
    for (double value : products) {
      if (value < lo) lo = value;
      if (value > hi) hi = value;
    }
    return widen(Interval(lo, hi));
  }
  const double down_products[4] = {mul_down(a.lower, b.lower), mul_down(a.lower, b.upper),
                                   mul_down(a.upper, b.lower), mul_down(a.upper, b.upper)};
  const double up_products[4] = {mul_up(a.lower, b.lower), mul_up(a.lower, b.upper),
                                 mul_up(a.upper, b.lower), mul_up(a.upper, b.upper)};
  if (any_nan4(down_products) || any_nan4(up_products)) return whole_line();
  double lo = down_products[0], hi = up_products[0];
  for (double value : down_products) {
    if (value < lo) lo = value;
  }
  for (double value : up_products) {
    if (value > hi) hi = value;
  }
  return Interval(lo, hi);
}

Interval interval_div(const Interval& a, const Interval& b) {
  if (!b.is_finite()) return whole_line();
  if (b.lower <= 0.0 && b.upper >= 0.0) {
    // Division by an interval containing zero is not bounded.
    return whole_line();
  }
  if (!kDirected) {
    const double quotients[4] = {a.lower / b.lower, a.lower / b.upper, a.upper / b.lower,
                                 a.upper / b.upper};
    if (any_nan4(quotients)) return whole_line();
    double lo = quotients[0], hi = quotients[0];
    for (double value : quotients) {
      if (value < lo) lo = value;
      if (value > hi) hi = value;
    }
    return widen(Interval(lo, hi));
  }
  const double down_quotients[4] = {div_down(a.lower, b.lower), div_down(a.lower, b.upper),
                                    div_down(a.upper, b.lower), div_down(a.upper, b.upper)};
  const double up_quotients[4] = {div_up(a.lower, b.lower), div_up(a.lower, b.upper),
                                  div_up(a.upper, b.lower), div_up(a.upper, b.upper)};
  if (any_nan4(down_quotients) || any_nan4(up_quotients)) return whole_line();
  double lo = down_quotients[0], hi = up_quotients[0];
  for (double value : down_quotients) {
    if (value < lo) lo = value;
  }
  for (double value : up_quotients) {
    if (value > hi) hi = value;
  }
  return Interval(lo, hi);
}

Interval interval_neg(const Interval& a) { return Interval(-a.upper, -a.lower); }

Interval interval_scale(const Interval& a, double factor) {
  return interval_mul(a, Interval(factor, factor));
}

Interval interval_abs(const Interval& a) {
  if (has_nan(a.lower) || has_nan(a.upper)) return whole_line();
  if (a.lower >= 0.0) return a;
  if (a.upper <= 0.0) return Interval(-a.upper, -a.lower);
  return Interval(0.0, std::max(-a.lower, a.upper));
}

Interval interval_sqrt(const Interval& a) {
  if (!a.is_finite()) {
    if (a.upper < 0.0) return invalid_domain();
    return Interval(0.0, std::numeric_limits<double>::infinity());
  }
  if (a.upper < 0.0) return invalid_domain();
  // The domain of sqrt intersected with [a.lower, a.upper].
  const double lower = a.lower > 0.0 ? a.lower : 0.0;
  if (!kDirected) {
    return widen(Interval(std::sqrt(lower), std::sqrt(a.upper)));
  }
  return Interval(sqrt_down(lower), sqrt_up(a.upper));
}

Interval interval_exp(const Interval& a) {
  if (!a.is_finite()) {
    if (a.upper == -std::numeric_limits<double>::infinity()) return Interval(0.0, 0.0);
    return whole_line();
  }
  const long double lo = std::exp(static_cast<long double>(a.lower));
  const long double hi = std::exp(static_cast<long double>(a.upper));
  const double out_lo =
      static_cast<double>(lo - kLibmMargin * std::max(1.0L, std::fabs(lo)));
  const double out_hi =
      static_cast<double>(hi + kLibmMargin * std::max(1.0L, std::fabs(hi)));
  return Interval(std::max(0.0, out_lo), out_hi);
}

Interval interval_log(const Interval& a) {
  if (a.lower <= 0.0) return invalid_domain();
  const long double lo = std::log(static_cast<long double>(a.lower));
  const long double hi = std::log(static_cast<long double>(a.upper));
  return Interval(static_cast<double>(lo - kLibmMargin * std::max(1.0L, std::fabs(lo))),
                  static_cast<double>(hi + kLibmMargin * std::max(1.0L, std::fabs(hi))));
}

Interval interval_sin(const Interval& a) {
  if (!a.is_finite()) return Interval(-1.0, 1.0);
  const long double lower = static_cast<long double>(a.lower);
  const long double upper = static_cast<long double>(a.upper);
  const long double width = upper - lower;
  if (width >= 2.0L * kPi) return Interval(-1.0, 1.0);
  const long double magnitude = std::max(std::fabs(lower), std::fabs(upper));
  if (magnitude > 1.0e9L) {
    // The phase of a huge argument cannot be decided at this precision.  A
    // coarse [-1,1] enclosure is honest; a narrow bound would be fabricated.
    return Interval(-1.0, 1.0);
  }
  const long double margin = kLibmMargin * std::max(1.0L, magnitude);
  const long double sin_lower = std::sin(lower);
  const long double sin_upper = std::sin(upper);
  long double lo = std::min(sin_lower, sin_upper) - margin;
  long double hi = std::max(sin_lower, sin_upper) + margin;
  // Every maximum of sin lies at pi/2 + 2k*pi and every minimum at
  // -pi/2 + 2k*pi.  Scan the integer phases that can meet the interval; the
  // three extra indices absorb the error of the division by pi/2.
  const long long first = static_cast<long long>(std::floor(lower / kHalfPi)) - 3;
  const long long last = static_cast<long long>(std::ceil(upper / kHalfPi)) + 3;
  for (long long k = first; k <= last; ++k) {
    const long double critical = static_cast<long double>(k) * kHalfPi;
    if (critical > lower - margin && critical < upper + margin) {
      if (k % 4 == 1 || k % 4 == -3) hi = 1.0;   // pi/2 + 2*pi*n
      if (k % 4 == 3 || k % 4 == -1) lo = -1.0;  // -pi/2 + 2*pi*n
    }
  }
  return Interval(static_cast<double>(std::max(-1.0L, lo)),
                  static_cast<double>(std::min(1.0L, hi)));
}

Interval interval_cos(const Interval& a) {
  if (!a.is_finite()) return Interval(-1.0, 1.0);
  const long double lower = static_cast<long double>(a.lower);
  const long double upper = static_cast<long double>(a.upper);
  const long double width = upper - lower;
  if (width >= 2.0L * kPi) return Interval(-1.0, 1.0);
  const long double magnitude = std::max(std::fabs(lower), std::fabs(upper));
  if (magnitude > 1.0e9L) return Interval(-1.0, 1.0);
  const long double margin = kLibmMargin * std::max(1.0L, magnitude);
  const long double cos_lower = std::cos(lower);
  const long double cos_upper = std::cos(upper);
  long double lo = std::min(cos_lower, cos_upper) - margin;
  long double hi = std::max(cos_lower, cos_upper) + margin;
  // cos has maxima at 2k*pi and minima at pi + 2k*pi.
  const long long first = static_cast<long long>(std::floor(lower / kHalfPi)) - 3;
  const long long last = static_cast<long long>(std::ceil(upper / kHalfPi)) + 3;
  for (long long k = first; k <= last; ++k) {
    const long double critical = static_cast<long double>(k) * kHalfPi;
    if (critical > lower - margin && critical < upper + margin) {
      if (k % 4 == 0) hi = 1.0;                  // 2*pi*n
      if (k % 4 == 2 || k % 4 == -2) lo = -1.0;  // pi + 2*pi*n
    }
  }
  return Interval(static_cast<double>(std::max(-1.0L, lo)),
                  static_cast<double>(std::min(1.0L, hi)));
}

Interval interval_atan(const Interval& a) {
  if (!a.is_finite()) {
    if (a.lower == -std::numeric_limits<double>::infinity()) {
      return Interval(static_cast<double>(-kHalfPi), static_cast<double>(kHalfPi));
    }
    return whole_line();
  }
  const long double lo = std::atan(static_cast<long double>(a.lower));
  const long double hi = std::atan(static_cast<long double>(a.upper));
  return Interval(static_cast<double>(lo - kLibmMargin),
                  static_cast<double>(hi + kLibmMargin));
}

Interval interval_atan2(const Interval& y, const Interval& x) {
  if (!y.is_finite() || !x.is_finite()) return whole_line();
  // The origin is not in the domain: every direction is possible in its
  // neighbourhood, so the closed principal-value range is the honest answer.
  if (x.lower <= 0.0 && x.upper >= 0.0 && y.lower <= 0.0 && y.upper >= 0.0) {
    return Interval(static_cast<double>(-kPi), static_cast<double>(kPi));
  }
  // Crossing the negative real axis changes the branch: angles approach +pi
  // from above and -pi from below, so only the closed principal range
  // contains both.  Four corner samples alone miss these limits.
  if (x.upper < 0.0 && y.lower < 0.0 && y.upper > 0.0) {
    return Interval(static_cast<double>(-kPi), static_cast<double>(kPi));
  }
  const long double ys[2] = {static_cast<long double>(y.lower),
                             static_cast<long double>(y.upper)};
  const long double xs[2] = {static_cast<long double>(x.lower),
                             static_cast<long double>(x.upper)};
  long double lo = std::atan2(ys[0], xs[0]);
  long double hi = lo;
  for (long double yy : ys) {
    for (long double xx : xs) {
      const long double value = std::atan2(yy, xx);
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
  }
  if (x.upper < 0.0) {
    // On the strictly negative x axis the angle tends to -pi as y tends to
    // zero from below and to +pi as y reaches zero from above.  The endpoint
    // zero is included, so the corresponding limit must be closed in the
    // enclosure.  Corners alone can miss the limit because it is not attained
    // in the interior.
    if (y.lower < 0.0 && y.upper >= 0.0) lo = -kPi;
    if (y.upper > 0.0 || (y.upper == 0.0 && y.lower < 0.0)) hi = kPi;
  }
  return Interval(static_cast<double>(lo - kLibmMargin),
                  static_cast<double>(hi + kLibmMargin));
}

Interval interval_pow(const Interval& base, const Interval& exponent) {
  if (!base.is_finite() || !exponent.is_finite()) return whole_line();
  if (base.lower > 0.0) {
    // The general positive-base power is exp(Y * log(X)).  The composition
    // keeps the enclosure even when the intermediate intervals are wide.
    return interval_exp(interval_mul(exponent, interval_log(base)));
  }
  if (base.lower >= 0.0) {
    // The base contains zero.  A positive exponent keeps the result finite
    // (0^positive = 0); a non-positive exponent can diverge or uses the 0^0
    // convention, so the enclosure is stated explicitly.
    if (exponent.lower > 0.0) {
      const long double upper = std::pow(static_cast<long double>(base.upper),
                                         static_cast<long double>(exponent.upper));
      const double out_hi = static_cast<double>(
          upper + kLibmMargin * std::max(1.0L, std::fabs(upper)));
      return Interval(0.0, out_hi);
    }
    if (exponent.lower == 0.0 && exponent.upper == 0.0) {
      // x^0 = 1 for x > 0, and the 0^0 convention is 1.
      return Interval(1.0, 1.0);
    }
    return Interval(0.0, std::numeric_limits<double>::infinity());
  }
  // A partially negative base only has a real power for an exact integer
  // exponent.  Anything else is outside the real domain.
  if (exponent.lower == exponent.upper) {
    const long double value = static_cast<long double>(exponent.lower);
    const long double rounded = std::nearbyint(value);
    if (rounded == value && std::fabs(rounded) <= 9007199254740992.0L) {
      const long long n = static_cast<long long>(rounded);
      if (n < 0 && base.lower <= 0.0 && base.upper >= 0.0) {
        return invalid_domain();
      }
      return integer_power(base, n);
    }
  }
  return invalid_domain();
}

Interval interval_intersect(const Interval& a, const Interval& b) {
  const double lo = std::max(a.lower, b.lower);
  const double hi = std::min(a.upper, b.upper);
  if (lo > hi) return invalid_domain();
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
