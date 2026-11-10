// Independent exact-rational containment oracle for the interval primitives.
//
// This is NOT a replacement for the directed-rounding proof: it is an
// independent experiment.  Every binary64 value is converted to an exact
// rational with boost::multiprecision::cpp_int; the exact result of +, -, * or
// / is formed in rational arithmetic (no floating point at all) and compared
// with the interval endpoints.  sqrt is checked by squaring the endpoints as
// exact rationals.  A failure means the implementation does not enclose the
// true value for that input, which is a counterexample to the containment
// claim.  A pass on N random cases is evidence, not a proof over all inputs.
#include "numerics/interval.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

using boost::multiprecision::cpp_int;

struct Rat {
  cpp_int num;
  cpp_int den;  // always a positive power of two
};

Rat from_double(double value) {
  if (value == 0.0) return Rat{cpp_int(0), cpp_int(1)};
  int exponent = 0;
  const double fraction = std::frexp(value, &exponent);
  // value = fraction * 2^exponent, |fraction| in [0.5, 1).
  const double scaled = std::ldexp(fraction, 53);
  const long long mantissa = static_cast<long long>(scaled);
  cpp_int num(mantissa);
  cpp_int den(1);
  int power = exponent - 53;
  if (power >= 0) {
    num <<= power;
  } else {
    den <<= -power;
  }
  return Rat{num, den};
}

Rat normalize(Rat value) {
  if (value.den < 0) {
    value.num = -value.num;
    value.den = -value.den;
  }
  return value;
}

Rat add(const Rat& a, const Rat& b) {
  return normalize(Rat{a.num * b.den + b.num * a.den, a.den * b.den});
}
Rat sub(const Rat& a, const Rat& b) {
  return normalize(Rat{a.num * b.den - b.num * a.den, a.den * b.den});
}
Rat mul(const Rat& a, const Rat& b) { return normalize(Rat{a.num * b.num, a.den * b.den}); }
Rat div(const Rat& a, const Rat& b) { return normalize(Rat{a.num * b.den, a.den * b.num}); }


int compare(const Rat& a, const Rat& b) {
  const cpp_int left = a.num * b.den;
  const cpp_int right = b.num * a.den;
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

bool within(const acsd::numerics::Interval& interval, const Rat& exact) {
  return compare(from_double(interval.lower), exact) <= 0 &&
         compare(exact, from_double(interval.upper)) <= 0;
}

double random_double(std::mt19937_64& rng) {
  // Keep the exponent inside a range where the exact rational stays modest.
  std::uniform_int_distribution<int> exponent(-200, 200);
  std::uniform_real_distribution<double> mantissa(-1.0, 1.0);
  return std::ldexp(mantissa(rng) + (mantissa(rng) == 0.0 ? 0.5 : 0.0), exponent(rng));
}

}  // namespace

int main() {
  std::mt19937_64 rng(20261110);
  int failures = 0;
  int checks = 0;
  const int rounds = 20000;
  for (int i = 0; i < rounds; ++i) {
    const double a = random_double(rng);
    const double b = random_double(rng);
    if (!std::isfinite(a) || !std::isfinite(b)) continue;
    const Rat ra = from_double(a), rb = from_double(b);

    const auto add_interval = acsd::numerics::interval_add({a, a}, {b, b});
    const auto sub_interval = acsd::numerics::interval_sub({a, a}, {b, b});
    const auto mul_interval = acsd::numerics::interval_mul({a, a}, {b, b});
    ++checks;
    if (!within(add_interval, add(ra, rb))) {
      std::printf("FAIL add %.17g + %.17g -> [%.17g, %.17g]\n", a, b, add_interval.lower,
                  add_interval.upper);
      ++failures;
    }
    if (!within(sub_interval, sub(ra, rb))) {
      std::printf("FAIL sub %.17g - %.17g -> [%.17g, %.17g]\n", a, b, sub_interval.lower,
                  sub_interval.upper);
      ++failures;
    }
    if (!within(mul_interval, mul(ra, rb))) {
      std::printf("FAIL mul %.17g * %.17g -> [%.17g, %.17g]\n", a, b, mul_interval.lower,
                  mul_interval.upper);
      ++failures;
    }
    if (b != 0.0) {
      const auto div_interval = acsd::numerics::interval_div({a, a}, {b, b});
      if (!within(div_interval, div(ra, rb))) {
        std::printf("FAIL div %.17g / %.17g -> [%.17g, %.17g]\n", a, b, div_interval.lower,
                    div_interval.upper);
        ++failures;
      }
    }
    if (a > 0.0) {
      const auto sqrt_interval = acsd::numerics::interval_sqrt({a, a});
      const Rat lo = from_double(sqrt_interval.lower);
      const Rat hi = from_double(sqrt_interval.upper);
      const Rat lo_square = mul(lo, lo);
      const Rat hi_square = mul(hi, hi);
      if (compare(lo_square, ra) > 0 || compare(hi_square, ra) < 0) {
        std::printf("FAIL sqrt %.17g -> [%.17g, %.17g]\n", a, sqrt_interval.lower,
                    sqrt_interval.upper);
        ++failures;
      }
    }
  }

  // Targeted counterexamples from docs/audit/IMPLEMENTATION-STATUS.md.
  {
    const auto sum = acsd::numerics::interval_add({0.1, 0.1}, {0.2, 0.2});
    if (!within(sum, add(from_double(0.1), from_double(0.2)))) {
      std::printf("FAIL 0.1+0.2 enclosure\n");
      ++failures;
    }
    const auto power = acsd::numerics::interval_pow({2.0, 3.0}, {2.0, 3.0});
    if (!(power.lower <= 4.0 && power.upper >= 27.0)) {
      std::printf("FAIL pow([2,3],[2,3]) does not contain 4 and 27: [%.17g, %.17g]\n",
                  power.lower, power.upper);
      ++failures;
    }
    const auto sine = acsd::numerics::interval_sin({1000.0, 1002.0});
    if (!(sine.upper >= 1.0)) {
      std::printf("FAIL sin([1000,1002]) does not contain 1: [%.17g, %.17g]\n", sine.lower,
                  sine.upper);
      ++failures;
    }
    const auto negative = acsd::numerics::interval_sin({-1001.0, -999.0});
    if (!(negative.lower <= -1.0)) {
      std::printf("FAIL sin([-1001,-999]) does not contain -1\n");
      ++failures;
    }
    // The angle tends to -pi as y tends to zero from below on the negative
    // x axis.  A four-corner sample misses values below atan2(y_lower, x_lower).
    const auto angle = acsd::numerics::interval_atan2({-1.0, 0.0}, {-2.0, -0.5});
    if (!(angle.lower <= -3.141592653589793 && angle.upper >= 3.141592653589793)) {
      std::printf("FAIL atan2 negative-axis crossing: [%.17g, %.17g]\n", angle.lower,
                  angle.upper);
      ++failures;
    }
    const auto integer_pow = acsd::numerics::interval_pow({-2.0, -1.0}, {2.0, 2.0});
    if (!(integer_pow.lower <= 1.0 && integer_pow.upper >= 4.0)) {
      std::printf("FAIL (-2,-1)^2 does not contain [1,4]\n");
      ++failures;
    }
    const auto undefined_pow = acsd::numerics::interval_pow({-2.0, -1.0}, {0.5, 0.5});
    if (!(undefined_pow.lower > undefined_pow.upper)) {
      std::printf("FAIL non-integer power of a negative base is not refused\n");
      ++failures;
    }
  }

  std::printf("interval oracle: %d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
