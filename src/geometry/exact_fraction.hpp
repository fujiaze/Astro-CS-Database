#pragma once
// Exact fractions without a normalisation on every operation.
//
// The cut cell kernel clips the detector block by half spaces whose constants
// come from the binary64 affine model.  Every one of those constants is a
// binary64 number and therefore an exact dyadic rational, so the clipping can
// be carried out in exact arithmetic.  The exactness does not depend on the
// fractions being reduced: a fraction that is not in lowest terms denotes the
// same number.
//
// The reduction (a greatest common divisor on big integers) is by far the most
// expensive operation of the clipping, and it is not needed for the value of
// the result.  This type therefore keeps the numerator and the denominator as
// they come out of the arithmetic and reduces only when the denominator has
// grown past a bound, so the growth of the denominators stays bounded while
// the exact values are unchanged.
#include "model/exact.hpp"

namespace acsd::geometry {

// The bound on the denominator of a fraction: past this many bits the
// fraction is reduced, so that the growth of the denominators stays bounded.
inline constexpr std::size_t kMaxFractionBits = 1024;

// An exact fraction, not necessarily in lowest terms.
class ExactFraction {
 public:
  // The default value is the exact zero, whose denominator is one.
  ExactFraction() : num_(0), den_(1) {}
  ExactFraction(std::int64_t numerator, std::int64_t denominator = 1);
  static ExactFraction from_bigint(model::BigInt numerator, model::BigInt denominator);
  static ExactFraction from_double_exact(double value);

  const model::BigInt& numerator() const { return num_; }
  const model::BigInt& denominator() const { return den_; }
  bool is_zero() const { return num_.is_zero(); }
  int sign() const { return num_.sign(); }

  ExactFraction operator+(const ExactFraction& other) const;
  ExactFraction operator-(const ExactFraction& other) const;
  ExactFraction operator*(const ExactFraction& other) const;
  ExactFraction operator/(const ExactFraction& other) const;
  ExactFraction operator-() const;

  // The exact value as a reduced rational (the conversion is used when the
  // value leaves the geometry module).
  model::Rational to_rational() const { return model::Rational::from_bigint(num_, den_); }

 private:
  void reduce_if_large();

  model::BigInt num_;
  model::BigInt den_;
};

}  // namespace acsd::geometry
