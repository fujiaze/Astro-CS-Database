#pragma once
// Exact integer and rational arithmetic used by the geometry ledger and by
// exact half-space clipping.  No floating point is used anywhere inside this
// header: every operation is exact, so the produced enclosures are exact
// rational statements about the actual binary64 inputs.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace acsd::model {

// Arbitrary precision signed integer, sign-magnitude with little endian
// 32-bit limbs.
class BigInt {
 public:
  BigInt() = default;
  explicit BigInt(std::int64_t value);
  static BigInt from_string(std::string_view text);   // optional sign, decimal digits
  static BigInt pow2(unsigned exponent);

  bool is_zero() const { return limbs_.empty(); }
  bool negative() const { return negative_; }
  int sign() const { return limbs_.empty() ? 0 : (negative_ ? -1 : 1); }

  BigInt negated() const;
  BigInt abs() const;

  friend BigInt operator+(const BigInt& a, const BigInt& b);
  friend BigInt operator-(const BigInt& a, const BigInt& b);
  friend BigInt operator*(const BigInt& a, const BigInt& b);
  friend BigInt operator/(const BigInt& a, const BigInt& b);   // truncating
  friend BigInt operator%(const BigInt& a, const BigInt& b);
  BigInt operator<<(unsigned bits) const;
  BigInt operator>>(unsigned bits) const;                      // floor division by 2^bits
  friend int compare(const BigInt& a, const BigInt& b);
  friend bool operator==(const BigInt& a, const BigInt& b) { return compare(a, b) == 0; }
  friend bool operator<(const BigInt& a, const BigInt& b) { return compare(a, b) < 0; }
  friend bool operator>(const BigInt& a, const BigInt& b) { return compare(a, b) > 0; }
  friend bool operator<=(const BigInt& a, const BigInt& b) { return compare(a, b) <= 0; }
  friend bool operator>=(const BigInt& a, const BigInt& b) { return compare(a, b) >= 0; }

  BigInt gcd(const BigInt& other) const;
  // Round-to-nearest binary64 conversion (returns nullopt when not finite).
  std::optional<double> to_double() const;
  // Directed conversions: floor toward -inf and ceil toward +inf.
  double to_double_floor() const;
  double to_double_ceil() const;
  std::string to_string() const;
  std::size_t bit_length() const;

 private:
  static int compare_magnitude(const std::vector<std::uint32_t>& a,
                               const std::vector<std::uint32_t>& b);
  static std::vector<std::uint32_t> add_magnitude(const std::vector<std::uint32_t>& a,
                                                   const std::vector<std::uint32_t>& b);
  static std::vector<std::uint32_t> sub_magnitude(const std::vector<std::uint32_t>& a,
                                                   const std::vector<std::uint32_t>& b);
  static std::vector<std::uint32_t> mul_magnitude(const std::vector<std::uint32_t>& a,
                                                   const std::vector<std::uint32_t>& b);
  void trim();

  bool negative_ = false;
  std::vector<std::uint32_t> limbs_;  // little endian, no trailing zero limbs
};

// Exact rational number with a strictly positive denominator, always kept in
// lowest terms.
class Rational {
 public:
  // The default value is the exact zero, whose denominator is one.
  Rational();
  Rational(std::int64_t numerator, std::int64_t denominator = 1);
  static Rational from_bigint(BigInt numerator, BigInt denominator);
  static Rational from_double_exact(double value);        // binary64 is exactly a dyadic

  const BigInt& numerator() const { return num_; }
  const BigInt& denominator() const { return den_; }
  bool is_zero() const { return num_.is_zero(); }
  int sign() const { return num_.sign(); }

  friend Rational operator+(const Rational& a, const Rational& b);
  friend Rational operator-(const Rational& a, const Rational& b);
  friend Rational operator*(const Rational& a, const Rational& b);
  friend Rational operator/(const Rational& a, const Rational& b);
  friend Rational operator-(const Rational& a);
  friend int compare(const Rational& a, const Rational& b);
  friend bool operator==(const Rational& a, const Rational& b) { return compare(a, b) == 0; }
  friend bool operator<(const Rational& a, const Rational& b) { return compare(a, b) < 0; }
  friend bool operator<=(const Rational& a, const Rational& b) { return compare(a, b) <= 0; }
  friend bool operator>(const Rational& a, const Rational& b) { return compare(a, b) > 0; }
  friend bool operator>=(const Rational& a, const Rational& b) { return compare(a, b) >= 0; }

  double to_double() const;       // round to nearest
  double to_double_floor() const;
  double to_double_ceil() const;
  std::string to_string() const;

 private:
  void normalize();
  BigInt num_;
  BigInt den_;
};

}  // namespace acsd::model
