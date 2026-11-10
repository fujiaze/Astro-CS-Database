#include "geometry/exact_fraction.hpp"

namespace acsd::geometry {
namespace {

// The number of trailing zero bits of |value| (0 for zero), found by a binary
// search over the shifts: every step is one big-integer shift and compare.
unsigned trailing_zero_bits(const model::BigInt& value) {
  if (value.is_zero()) return 0;
  unsigned low = 0;
  unsigned high = static_cast<unsigned>(value.bit_length());
  while (low + 1 < high) {
    const unsigned middle = low + (high - low) / 2;
    if (((value >> middle) << middle) == value) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return low;
}

// Binary (Stein) greatest common divisor of |a| and |b|.
//
// The clipping produces denominators that are dyadic factors times small odd
// determinants, and the naive Euclid of model::BigInt::gcd needs O(bits) big
// divisions on them (about 4 ms for a 935 bit denominator in the measured
// cut-cell case).  The binary algorithm only subtracts and shifts, which is
// two orders of magnitude faster on the same inputs; the result is the exact
// gcd, so the reduced fractions are unchanged.
model::BigInt binary_gcd(model::BigInt a, model::BigInt b) {
  a = a.abs();
  b = b.abs();
  if (a.is_zero()) return b;
  if (b.is_zero()) return a;
  const unsigned a_zeros = trailing_zero_bits(a);
  const unsigned b_zeros = trailing_zero_bits(b);
  const unsigned shift = a_zeros < b_zeros ? a_zeros : b_zeros;
  a = a >> a_zeros;
  b = b >> b_zeros;
  while (!(a == b)) {
    if (a > b) {
      a = a - b;
      a = a >> trailing_zero_bits(a);
    } else {
      b = b - a;
      b = b >> trailing_zero_bits(b);
    }
  }
  return shift == 0 ? a : (a << shift);
}

}  // namespace

ExactFraction::ExactFraction(std::int64_t numerator, std::int64_t denominator) {
  model::BigInt top(numerator);
  model::BigInt bottom(denominator);
  if (bottom.is_zero()) {
    // A zero denominator is not a number; the callers never build one.
    num_ = model::BigInt(0);
    den_ = model::BigInt(1);
    return;
  }
  if (bottom.negative()) {
    top = top.negated();
    bottom = bottom.negated();
  }
  num_ = top;
  den_ = bottom;
}

ExactFraction ExactFraction::from_bigint(model::BigInt numerator, model::BigInt denominator) {
  ExactFraction out;
  if (denominator.is_zero()) {
    out.num_ = model::BigInt(0);
    out.den_ = model::BigInt(1);
    return out;
  }
  if (denominator.negative()) {
    numerator = numerator.negated();
    denominator = denominator.negated();
  }
  out.num_ = numerator;
  out.den_ = denominator;
  return out;
}

ExactFraction ExactFraction::from_double_exact(double value) {
  const model::Rational exact = model::Rational::from_double_exact(value);
  return from_bigint(exact.numerator(), exact.denominator());
}

void ExactFraction::reduce_if_large() {
  if (num_.is_zero()) {
    den_ = model::BigInt(1);
    return;
  }
  if (den_.bit_length() <= kMaxFractionBits) return;
  const model::BigInt g = binary_gcd(num_, den_);
  if (!g.is_zero() && !(g == model::BigInt(1))) {
    num_ = num_ / g;
    den_ = den_ / g;
  }
}

ExactFraction ExactFraction::operator+(const ExactFraction& other) const {
  ExactFraction out = from_bigint(num_ * other.den_ + other.num_ * den_, den_ * other.den_);
  out.reduce_if_large();
  return out;
}

ExactFraction ExactFraction::operator-(const ExactFraction& other) const {
  ExactFraction out = from_bigint(num_ * other.den_ - other.num_ * den_, den_ * other.den_);
  out.reduce_if_large();
  return out;
}

ExactFraction ExactFraction::operator*(const ExactFraction& other) const {
  ExactFraction out = from_bigint(num_ * other.num_, den_ * other.den_);
  out.reduce_if_large();
  return out;
}

ExactFraction ExactFraction::operator/(const ExactFraction& other) const {
  ExactFraction out = from_bigint(num_ * other.den_, den_ * other.num_);
  out.reduce_if_large();
  return out;
}

ExactFraction ExactFraction::operator-() const {
  ExactFraction out;
  out.num_ = num_.negated();
  out.den_ = den_;
  return out;
}

}  // namespace acsd::geometry
