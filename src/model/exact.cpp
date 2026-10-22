#include "model/exact.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace acsd::model {
namespace {

constexpr std::uint64_t kBase = 1ull << 32;

std::vector<std::uint32_t> from_u64(std::uint64_t v) {
  std::vector<std::uint32_t> out;
  if (v == 0) return out;
  out.push_back(static_cast<std::uint32_t>(v & 0xffffffffu));
  if (v >> 32) out.push_back(static_cast<std::uint32_t>(v >> 32));
  return out;
}

}  // namespace

void BigInt::trim() {
  while (!limbs_.empty() && limbs_.back() == 0) limbs_.pop_back();
  if (limbs_.empty()) negative_ = false;
}

BigInt::BigInt(std::int64_t value) {
  if (value == 0) return;
  negative_ = value < 0;
  std::uint64_t magnitude = negative_ ? (~static_cast<std::uint64_t>(value) + 1u)
                                      : static_cast<std::uint64_t>(value);
  limbs_ = from_u64(magnitude);
}

BigInt BigInt::from_string(std::string_view text) {
  BigInt out;
  std::size_t i = 0;
  bool neg = false;
  if (i < text.size() && (text[i] == '-' || text[i] == '+')) { neg = (text[i] == '-'); ++i; }
  if (i >= text.size()) throw std::invalid_argument("BigInt::from_string: empty");
  BigInt acc;
  for (; i < text.size(); ++i) {
    const char c = text[i];
    if (c < '0' || c > '9') throw std::invalid_argument("BigInt::from_string: bad digit");
    acc = acc * BigInt(10) + BigInt(c - '0');
  }
  if (neg) acc = acc.negated();
  out = acc;
  return out;
}

BigInt BigInt::pow2(unsigned exponent) {
  BigInt out;
  out.limbs_.assign((exponent / 32) + 1, 0u);
  out.limbs_[exponent / 32] = (1u << (exponent % 32));
  out.trim();
  return out;
}

BigInt BigInt::negated() const {
  BigInt out = *this;
  if (!out.limbs_.empty()) out.negative_ = !out.negative_;
  return out;
}

BigInt BigInt::abs() const {
  BigInt out = *this;
  out.negative_ = false;
  return out;
}

int BigInt::compare_magnitude(const std::vector<std::uint32_t>& a,
                              const std::vector<std::uint32_t>& b) {
  if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
  for (std::size_t i = a.size(); i-- > 0;) {
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

std::vector<std::uint32_t> BigInt::add_magnitude(const std::vector<std::uint32_t>& a,
                                                  const std::vector<std::uint32_t>& b) {
  std::vector<std::uint32_t> out;
  out.reserve(std::max(a.size(), b.size()) + 1);
  std::uint64_t carry = 0;
  for (std::size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
    std::uint64_t sum = carry;
    if (i < a.size()) sum += a[i];
    if (i < b.size()) sum += b[i];
    out.push_back(static_cast<std::uint32_t>(sum & 0xffffffffu));
    carry = sum >> 32;
  }
  if (carry) out.push_back(static_cast<std::uint32_t>(carry));
  return out;
}

// Requires |a| >= |b|.
std::vector<std::uint32_t> BigInt::sub_magnitude(const std::vector<std::uint32_t>& a,
                                                  const std::vector<std::uint32_t>& b) {
  std::vector<std::uint32_t> out(a.size());
  std::int64_t borrow = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::int64_t diff = static_cast<std::int64_t>(a[i]) - borrow -
                        (i < b.size() ? static_cast<std::int64_t>(b[i]) : 0);
    if (diff < 0) { diff += static_cast<std::int64_t>(kBase); borrow = 1; } else { borrow = 0; }
    out[i] = static_cast<std::uint32_t>(diff);
  }
  while (!out.empty() && out.back() == 0) out.pop_back();
  return out;
}

std::vector<std::uint32_t> BigInt::mul_magnitude(const std::vector<std::uint32_t>& a,
                                                  const std::vector<std::uint32_t>& b) {
  if (a.empty() || b.empty()) return {};
  std::vector<std::uint64_t> acc(a.size() + b.size(), 0);
  for (std::size_t i = 0; i < a.size(); ++i) {
    std::uint64_t carry = 0;
    for (std::size_t j = 0; j < b.size(); ++j) {
      const std::uint64_t cur = acc[i + j] + static_cast<std::uint64_t>(a[i]) * b[j] + carry;
      acc[i + j] = cur & 0xffffffffu;
      carry = cur >> 32;
    }
    std::size_t k = i + b.size();
    while (carry) {
      const std::uint64_t cur = acc[k] + carry;
      acc[k] = cur & 0xffffffffu;
      carry = cur >> 32;
      ++k;
    }
  }
  std::vector<std::uint32_t> out(acc.size());
  for (std::size_t i = 0; i < acc.size(); ++i) out[i] = static_cast<std::uint32_t>(acc[i]);
  while (!out.empty() && out.back() == 0) out.pop_back();
  return out;
}

BigInt operator+(const BigInt& a, const BigInt& b) {
  BigInt out;
  if (a.negative() == b.negative()) {
    out.limbs_ = BigInt::add_magnitude(a.limbs_, b.limbs_);
    out.negative_ = a.negative();
  } else {
    const int c = BigInt::compare_magnitude(a.limbs_, b.limbs_);
    if (c == 0) return BigInt();
    if (c > 0) { out.limbs_ = BigInt::sub_magnitude(a.limbs_, b.limbs_); out.negative_ = a.negative(); }
    else { out.limbs_ = BigInt::sub_magnitude(b.limbs_, a.limbs_); out.negative_ = b.negative(); }
  }
  out.trim();
  return out;
}

BigInt operator-(const BigInt& a, const BigInt& b) { return a + b.negated(); }

BigInt operator*(const BigInt& a, const BigInt& b) {
  BigInt out;
  out.limbs_ = BigInt::mul_magnitude(a.limbs_, b.limbs_);
  out.negative_ = (a.negative() != b.negative());
  out.trim();
  return out;
}

BigInt operator/(const BigInt& a, const BigInt& b) {
  if (b.limbs_.empty()) throw std::domain_error("BigInt division by zero");
  // Simple schoolbook long division on magnitudes; adequate for the ledger sizes.
  BigInt quotient;
  std::vector<std::uint32_t> a_limbs = a.limbs_;
  std::vector<std::uint32_t> rem;
  for (std::size_t i = a_limbs.size(); i-- > 0;) {
    rem.insert(rem.begin(), a_limbs[i]);
    while (!rem.empty() && rem.back() == 0) rem.pop_back();
    if (BigInt::compare_magnitude(rem, b.limbs_) < 0) {
      quotient.limbs_.insert(quotient.limbs_.begin(), 0u);
      continue;
    }
    std::uint32_t digit = 0;
    while (BigInt::compare_magnitude(rem, b.limbs_) >= 0) {
      rem = BigInt::sub_magnitude(rem, b.limbs_);
      ++digit;
      if (digit == 0xffffffffu) break;
    }
    quotient.limbs_.insert(quotient.limbs_.begin(), digit);
  }
  quotient.negative_ = (a.negative() != b.negative());
  quotient.trim();
  return quotient;
}

BigInt operator%(const BigInt& a, const BigInt& b) {
  if (b.limbs_.empty()) throw std::domain_error("BigInt modulo by zero");
  BigInt q = a / b;
  return a - q * b;
}

BigInt BigInt::operator<<(unsigned bits) const {
  BigInt out;
  if (limbs_.empty()) return out;
  const unsigned limb_shift = bits / 32;
  const unsigned bit_shift = bits % 32;
  out.limbs_.assign(limbs_.size() + limb_shift + 1, 0u);
  for (std::size_t i = 0; i < limbs_.size(); ++i) {
    const std::uint64_t v = static_cast<std::uint64_t>(limbs_[i]) << bit_shift;
    out.limbs_[i + limb_shift] |= static_cast<std::uint32_t>(v & 0xffffffffu);
    out.limbs_[i + limb_shift + 1] |= static_cast<std::uint32_t>(v >> 32);
  }
  out.negative_ = negative_;
  out.trim();
  return out;
}

BigInt BigInt::operator>>(unsigned bits) const {
  // Arithmetic shift: floor(value / 2^bits) for negative values.
  BigInt out;
  if (limbs_.empty()) return out;
  const unsigned limb_shift = bits / 32;
  const unsigned bit_shift = bits % 32;
  if (limb_shift >= limbs_.size()) {
    if (negative_) return BigInt(-1);
    return out;
  }
  out.limbs_.assign(limbs_.size() - limb_shift, 0u);
  for (std::size_t i = 0; i < out.limbs_.size(); ++i) {
    std::uint64_t v = limbs_[i + limb_shift] >> bit_shift;
    if (bit_shift != 0 && i + limb_shift + 1 < limbs_.size()) {
      v |= static_cast<std::uint64_t>(limbs_[i + limb_shift + 1]) << (32 - bit_shift);
    }
    out.limbs_[i] = static_cast<std::uint32_t>(v & 0xffffffffu);
  }
  out.negative_ = negative_;
  out.trim();
  if (negative_) {
    // Check whether any bit was shifted out; if so subtract one.
    bool lost = false;
    for (std::size_t i = 0; i < limb_shift && !lost; ++i) lost = limbs_[i] != 0;
    if (!lost && bit_shift != 0) lost = (limbs_[limb_shift] & ((1u << bit_shift) - 1u)) != 0;
    if (lost) {
      BigInt one(1);
      out = out - one;
    }
  }
  return out;
}

int compare(const BigInt& a, const BigInt& b) {
  if (a.negative() != b.negative()) return a.negative() ? -1 : 1;
  const int c = BigInt::compare_magnitude(a.limbs_, b.limbs_);
  return a.negative() ? -c : c;
}

BigInt BigInt::gcd(const BigInt& other) const {
  BigInt a = abs();
  BigInt b = other.abs();
  while (!b.is_zero()) {
    BigInt r = a % b;
    a = b;
    b = r;
  }
  return a;
}

std::size_t BigInt::bit_length() const {
  if (limbs_.empty()) return 0;
  std::size_t bits = (limbs_.size() - 1) * 32;
  std::uint32_t top = limbs_.back();
  while (top) { ++bits; top >>= 1; }
  return bits;
}

std::optional<double> BigInt::to_double() const {
  if (limbs_.empty()) return 0.0;
  const std::size_t bits = bit_length();
  if (bits > 1024) return std::nullopt;
  // Compute value = mantissa * 2^exp with 54 bits of mantissa.
  const std::size_t shift = bits > 54 ? bits - 54 : 0;
  BigInt mantissa = *this;
  if (shift) mantissa = mantissa >> shift;
  const int exp = static_cast<int>(shift);
  long double value = 0.0L;
  for (std::size_t i = mantissa.limbs_.size(); i-- > 0;) {
    value = value * 4294967296.0L + static_cast<long double>(mantissa.limbs_[i]);
  }
  long double scaled = std::ldexp(value, exp);
  double result = static_cast<double>(scaled);
  if (!std::isfinite(result)) return std::nullopt;
  return result;
}

double BigInt::to_double_floor() const {
  return Rational::from_bigint(*this, BigInt(1)).to_double_floor();
}

double BigInt::to_double_ceil() const {
  return Rational::from_bigint(*this, BigInt(1)).to_double_ceil();
}

std::string BigInt::to_string() const {
  if (limbs_.empty()) return "0";
  std::vector<std::uint32_t> work = limbs_;
  std::string digits;
  while (!work.empty()) {
    std::uint64_t rem = 0;
    for (std::size_t i = work.size(); i-- > 0;) {
      const std::uint64_t cur = (rem << 32) | work[i];
      work[i] = static_cast<std::uint32_t>(cur / 1000000000ull);
      rem = cur % 1000000000ull;
    }
    while (!work.empty() && work.back() == 0) work.pop_back();
    char buf[16];
    if (work.empty()) {
      std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(rem));
    } else {
      std::snprintf(buf, sizeof(buf), "%09llu", static_cast<unsigned long long>(rem));
    }
    digits.insert(0, buf);
  }
  return negative_ ? ("-" + digits) : digits;
}

Rational::Rational(std::int64_t numerator, std::int64_t denominator) {
  num_ = BigInt(numerator);
  den_ = BigInt(denominator);
  if (den_.is_zero()) throw std::domain_error("Rational with zero denominator");
  normalize();
}

Rational Rational::from_bigint(BigInt numerator, BigInt denominator) {
  Rational out;
  if (denominator.is_zero()) throw std::domain_error("Rational with zero denominator");
  out.num_ = numerator;
  out.den_ = denominator;
  out.normalize();
  return out;
}

Rational Rational::from_double_exact(double value) {
  if (!std::isfinite(value)) throw std::domain_error("Rational from non-finite double");
  if (value == 0.0) return Rational();
  int exponent = 0;
  const double mantissa = std::frexp(value, &exponent);  // |mantissa| in [0.5,1)
  // value = mantissa * 2^exponent with mantissa having 53 significant bits.
  const double scaled = std::ldexp(mantissa, 53);
  const auto integral = static_cast<std::int64_t>(scaled);
  BigInt numerator(integral);
  const int shift = exponent - 53;
  BigInt denominator(1);
  if (shift >= 0) numerator = numerator << static_cast<unsigned>(shift);
  else denominator = BigInt::pow2(static_cast<unsigned>(-shift));
  Rational out;
  out.num_ = numerator;
  out.den_ = denominator;
  out.normalize();
  return out;
}

void Rational::normalize() {
  if (den_.negative()) { den_ = den_.negated(); num_ = num_.negated(); }
  if (num_.is_zero()) { den_ = BigInt(1); return; }
  const BigInt g = num_.gcd(den_);
  if (!g.is_zero() && !(g == BigInt(1))) {
    num_ = num_ / g;
    den_ = den_ / g;
  }
}

Rational operator+(const Rational& a, const Rational& b) {
  return Rational::from_bigint(a.num_ * b.den_ + b.num_ * a.den_, a.den_ * b.den_);
}

Rational operator-(const Rational& a, const Rational& b) {
  return Rational::from_bigint(a.num_ * b.den_ - b.num_ * a.den_, a.den_ * b.den_);
}

Rational operator*(const Rational& a, const Rational& b) {
  return Rational::from_bigint(a.num_ * b.num_, a.den_ * b.den_);
}

Rational operator/(const Rational& a, const Rational& b) {
  if (b.num_.is_zero()) throw std::domain_error("Rational division by zero");
  return Rational::from_bigint(a.num_ * b.den_, a.den_ * b.num_);
}

Rational operator-(const Rational& a) { return Rational::from_bigint(a.num_.negated(), a.den_); }

int compare(const Rational& a, const Rational& b) {
  return compare(a.num_ * b.den_, b.num_ * a.den_);
}

double Rational::to_double() const {
  if (num_.is_zero()) return 0.0;
  const double lower = to_double_floor();
  const Rational lower_exact = Rational::from_double_exact(lower);
  const int c = compare(*this, lower_exact);
  if (c == 0) return lower;
  const double upper = std::nextafter(lower, std::numeric_limits<double>::infinity());
  const double half_ulp = (upper - lower) * 0.5;
  const Rational diff = *this - lower_exact;
  const double diff_value = diff.to_double();
  if (diff_value >= half_ulp) return upper;
  return lower;
}

double Rational::to_double_floor() const {
  if (num_.is_zero()) return 0.0;
  // Start from a coarse estimate and then walk to the largest binary64 that is
  // still <= the exact rational.  All comparisons are exact.
  double d = 0.0;
  const auto approx = num_.to_double();
  const auto approx_den = den_.to_double();
  if (approx && approx_den) {
    d = *approx / *approx_den;
    if (!std::isfinite(d)) d = 0.0;
  }
  if (compare(Rational::from_double_exact(d), *this) > 0) {
    while (compare(Rational::from_double_exact(d), *this) > 0) {
      const double previous = std::nextafter(d, -std::numeric_limits<double>::infinity());
      if (previous == d) return d;
      d = previous;
    }
  } else {
    while (true) {
      const double next = std::nextafter(d, std::numeric_limits<double>::infinity());
      if (next == d) break;
      if (compare(Rational::from_double_exact(next), *this) <= 0) d = next; else break;
    }
  }
  return d;
}

double Rational::to_double_ceil() const {
  if (num_.is_zero()) return 0.0;
  const double lower = to_double_floor();
  if (compare(*this, Rational::from_double_exact(lower)) == 0) return lower;
  return std::nextafter(lower, std::numeric_limits<double>::infinity());
}

std::string Rational::to_string() const {
  return num_.to_string() + "/" + den_.to_string();
}

}  // namespace acsd::model
