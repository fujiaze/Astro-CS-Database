#include "geometry/exact_fraction.hpp"

namespace acsd::geometry {

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
  const model::BigInt g = num_.gcd(den_);
  if (!g.is_zero() && !(g == model::BigInt(1))) {
    num_ = num_ / g;
    den_ = den_ / g;
  }
}

ExactFraction ExactFraction::operator+(const ExactFraction& other) const {
  return from_bigint(num_ * other.den_ + other.num_ * den_, den_ * other.den_);
}

ExactFraction ExactFraction::operator-(const ExactFraction& other) const {
  return from_bigint(num_ * other.den_ - other.num_ * den_, den_ * other.den_);
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
