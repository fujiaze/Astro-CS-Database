#include "model/units.hpp"

#include <algorithm>
#include <cstring>

#include "model/hash.hpp"

namespace acsd::model {
namespace {

Unit make_unit(std::string scale, int sr_power, int pixel_power, int nm_power, double multiplier,
               int linear_power) {
  Unit u;
  u.scale = std::move(scale);
  u.sr_power = sr_power;
  u.pixel_power = pixel_power;
  u.nm_power = nm_power;
  u.multiplier = multiplier;
  u.linear_power = linear_power;
  return u;
}

}  // namespace

Unit unit_dimensionless() { return make_unit("1", 0, 0, 0, 1.0, 0); }
Unit unit_adu(const std::string& identity) {
  return make_unit(identity, 0, 0, 0, 1.0, 1);
}
Unit unit_signal(const std::string& identity) {
  return make_unit(identity, 0, 0, 0, 1.0, 1);
}
Unit unit_variance(const std::string& identity) {
  return make_unit(identity, 0, 0, 0, 1.0, 2);
}
Unit unit_sr() { return make_unit("sr", 1, 0, 0, 1.0, 0); }
Unit unit_sr_per_pixel() { return make_unit("sr/pixel", 1, -1, 0, 1.0, 0); }
Unit unit_per_sr() { return make_unit("1/sr", -1, 0, 0, 1.0, 0); }
Unit unit_nm() { return make_unit("nm", 0, 0, 1, 1.0, 0); }
Unit unit_seconds() { return make_unit("s", 0, 0, 0, 1.0, 0); }

bool units_equal(const Unit& a, const Unit& b) {
  return a.scale == b.scale && a.sr_power == b.sr_power && a.pixel_power == b.pixel_power &&
         a.nm_power == b.nm_power && a.linear_power == b.linear_power &&
         a.multiplier == b.multiplier;
}

bool unit_is_dimensionless(const Unit& a) {
  return a.sr_power == 0 && a.pixel_power == 0 && a.nm_power == 0 && a.linear_power == 0 &&
         a.multiplier == 1.0 && (a.scale.empty() || a.scale == "1");
}

bool unit_has_linear_power(const Unit& a, int power) { return a.linear_power == power; }

Unit unit_multiply(const Unit& a, const Unit& b) {
  Unit out;
  out.scale = a.scale == b.scale ? a.scale : (a.scale + "*" + b.scale);
  out.sr_power = a.sr_power + b.sr_power;
  out.pixel_power = a.pixel_power + b.pixel_power;
  out.nm_power = a.nm_power + b.nm_power;
  out.multiplier = a.multiplier * b.multiplier;
  out.linear_power = a.linear_power + b.linear_power;
  return out;
}

Unit unit_square(const Unit& a) { return unit_multiply(a, a); }

std::string unit_to_string(const Unit& a) {
  std::string out = a.scale.empty() ? "1" : a.scale;
  if (a.sr_power != 0) out += "^sr" + std::to_string(a.sr_power);
  if (a.pixel_power != 0) out += "^px" + std::to_string(a.pixel_power);
  if (a.nm_power != 0) out += "^nm" + std::to_string(a.nm_power);
  if (a.linear_power != 0) out += "^L" + std::to_string(a.linear_power);
  return out;
}

// ---------------------------------------------------------------------------
// Dyadic endpoints
// ---------------------------------------------------------------------------

Dyadic dyadic_from_double(double value) {
  Dyadic out;
  if (!std::isfinite(value)) { out.signed_integer = "0"; out.exponent2 = 0; return out; }
  if (value == 0.0) {
    out.signed_integer = std::signbit(value) ? "-0" : "0";
    out.exponent2 = 0;
    return out;
  }
  int exponent = 0;
  const double mantissa = std::frexp(value, &exponent);
  const double scaled = std::ldexp(mantissa, 53);
  const auto integral = static_cast<std::int64_t>(scaled);
  BigInt numerator(integral);
  const int shift = exponent - 53;
  if (shift >= 0) {
    numerator = numerator << static_cast<unsigned>(shift);
    out.signed_integer = numerator.to_string();
    out.exponent2 = 0;
  } else {
    out.signed_integer = numerator.to_string();
    out.exponent2 = shift;
  }
  // Normalise: no trailing zeros in the decimal representation are required,
  // but the exponent must be the smallest one keeping the value integral.
  return out;
}

double dyadic_to_double(const Dyadic& value) {
  if (value.signed_integer.empty()) return 0.0;
  const Rational r = Rational::from_bigint(BigInt::from_string(value.signed_integer),
                                            BigInt::pow2(static_cast<unsigned>(-value.exponent2)));
  return r.to_double();
}

ExactInterval exact_point(double value) {
  ExactInterval out;
  const Dyadic d = dyadic_from_double(value);
  out.lower = d;
  out.upper = d;
  out.precision_bits = 53;
  return out;
}

ExactInterval exact_from_interval(const Interval& value) {
  ExactInterval out;
  out.lower = dyadic_from_double(value.lower);
  out.upper = dyadic_from_double(value.upper);
  out.precision_bits = 53;
  return out;
}

ExactInterval exact_widen(const ExactInterval& value, double outward_ulp) {
  ExactInterval out = value;
  const double lower = dyadic_to_double(value.lower) - outward_ulp;
  const double upper = dyadic_to_double(value.upper) + outward_ulp;
  out.lower = dyadic_from_double(lower);
  out.upper = dyadic_from_double(upper);
  out.precision_bits = std::max<U32>(value.precision_bits, 53u);
  return out;
}

namespace {

Rational dyadic_rational(const Dyadic& d) {
  if (d.signed_integer.empty()) return Rational();
  const BigInt numerator = BigInt::from_string(d.signed_integer);
  if (d.exponent2 >= 0) return Rational::from_bigint(numerator << static_cast<unsigned>(d.exponent2),
                                                      BigInt(1));
  return Rational::from_bigint(numerator, BigInt::pow2(static_cast<unsigned>(-d.exponent2)));
}

}  // namespace

ExactInterval exact_add(const ExactInterval& a, const ExactInterval& b) {
  const Rational lo = dyadic_rational(a.lower) + dyadic_rational(b.lower);
  const Rational hi = dyadic_rational(a.upper) + dyadic_rational(b.upper);
  ExactInterval out;
  out.lower.signed_integer = lo.numerator().to_string();
  out.lower.exponent2 = 0;
  out.upper.signed_integer = hi.numerator().to_string();
  out.upper.exponent2 = 0;
  out.precision_bits = std::max(a.precision_bits, b.precision_bits);
  // Keep the exponent representation simple and exact.
  auto compact = [](Dyadic& d) {
    Rational r = dyadic_rational(d);
    d.signed_integer = r.numerator().to_string();
    d.exponent2 = 0;
  };
  compact(out.lower);
  compact(out.upper);
  return out;
}

ExactInterval exact_mul(const ExactInterval& a, const ExactInterval& b) {
  const Rational alo = dyadic_rational(a.lower), ahi = dyadic_rational(a.upper);
  const Rational blo = dyadic_rational(b.lower), bhi = dyadic_rational(b.upper);
  const Rational p1 = alo * blo, p2 = alo * bhi, p3 = ahi * blo, p4 = ahi * bhi;
  Rational lo = p1, hi = p1;
  for (const Rational* p : {&p2, &p3, &p4}) {
    if (compare(*p, lo) < 0) lo = *p;
    if (compare(*p, hi) > 0) hi = *p;
  }
  ExactInterval out;
  out.lower.signed_integer = lo.numerator().to_string();
  out.lower.exponent2 = 0;
  out.upper.signed_integer = hi.numerator().to_string();
  out.upper.exponent2 = 0;
  out.precision_bits = std::max(a.precision_bits, b.precision_bits);
  return out;
}

bool exact_less(const ExactInterval& a, const ExactInterval& b) {
  return compare(dyadic_rational(a.upper), dyadic_rational(b.lower)) < 0;
}

bool exact_contains(const ExactInterval& a, const ExactInterval& b) {
  return compare(dyadic_rational(a.lower), dyadic_rational(b.lower)) <= 0 &&
         compare(dyadic_rational(a.upper), dyadic_rational(b.upper)) >= 0;
}

std::string exact_to_string(const ExactInterval& value) {
  return "[" + value.lower.signed_integer + "," + value.upper.signed_integer + "]@" +
         std::to_string(value.precision_bits) + "b";
}

// ---------------------------------------------------------------------------
// Intervals
// ---------------------------------------------------------------------------

Interval interval_point(double value) { return Interval{value, value}; }

Interval interval_from_bounds(double lower, double upper) {
  if (lower > upper) std::swap(lower, upper);
  return Interval{lower, upper};
}

bool interval_valid(const Interval& value) {
  return std::isfinite(value.lower) && std::isfinite(value.upper) && value.lower <= value.upper;
}

double interval_width(const Interval& value) { return value.upper - value.lower; }

Interval interval_add(const Interval& a, const Interval& b) {
  return Interval{a.lower + b.lower, a.upper + b.upper};
}

Interval interval_mul(const Interval& a, const Interval& b) {
  const double p1 = a.lower * b.lower, p2 = a.lower * b.upper;
  const double p3 = a.upper * b.lower, p4 = a.upper * b.upper;
  return Interval{std::min({p1, p2, p3, p4}), std::max({p1, p2, p3, p4})};
}

Interval interval_scale(const Interval& a, double factor) {
  return interval_mul(a, Interval{factor, factor});
}

bool interval_contains(const Interval& outer, const Interval& inner) {
  return outer.lower <= inner.lower && outer.upper >= inner.upper;
}

bool interval_contains_zero(const Interval& value) {
  return value.lower <= 0.0 && value.upper >= 0.0;
}

std::string interval_to_string(const Interval& value) {
  return "[" + std::to_string(value.lower) + "," + std::to_string(value.upper) + "]";
}

// ---------------------------------------------------------------------------
// Identity and assessment
// ---------------------------------------------------------------------------

Identity make_identity(const std::string& schema_id, U32 major, U32 minor,
                       const std::string& content_hash_hex, const std::string& run_id) {
  Identity id;
  id.object_id = schema_id + ":" + content_hash_hex;
  id.content_hash = content_id(content_hash_hex);
  id.run_id = run_id;
  id.revision = 1;
  id.schema_id = schema_id;
  id.schema_major = major;
  id.schema_minor = minor;
  return id;
}

Assessment make_assessment(Code code, Capability capability) {
  Assessment a;
  a.complete = false;
  a.validity = Validity::certified;
  a.science = ScienceState::bounded;
  a.noise = NoiseState::unknown;
  a.execution = code == Code::ok ? ExecutionState::complete : ExecutionState::failed;
  a.capability = capability;
  return a;
}

void add_issue(Assessment& assessment, Code code, const std::string& reason,
               const std::string& origin, const std::string& impact, const std::string& retry) {
  Issue issue;
  issue.code = code;
  issue.origin = origin;
  issue.reason = reason;
  issue.impact = impact;
  issue.retry_condition = retry;
  assessment.issues.push_back(std::move(issue));
}

void set_included(Assessment& assessment, std::string item) {
  assessment.included.push_back(std::move(item));
}

void set_omitted(Assessment& assessment, std::string item) {
  assessment.omitted.push_back(std::move(item));
}

Condition make_condition(const std::string& estimator, const std::string& kind) {
  Condition c;
  c.estimator = estimator;
  c.kind = kind;
  return c;
}

// ---------------------------------------------------------------------------
// Small vector helpers
// ---------------------------------------------------------------------------

double vec_norm2(const std::array<double, 6>& v) {
  double sum = 0.0;
  for (double x : v) sum += x * x;
  return std::sqrt(sum);
}

double mat2_det(const Mat2& m) { return m[0] * m[3] - m[1] * m[2]; }

Mat2 mat2_inverse(const Mat2& m) {
  const double det = mat2_det(m);
  Mat2 out{0.0, 0.0, 0.0, 0.0};
  const double inv = 1.0 / det;
  out[0] = m[3] * inv;
  out[1] = -m[1] * inv;
  out[2] = -m[2] * inv;
  out[3] = m[0] * inv;
  return out;
}

bool mat2_finite(const Mat2& m) {
  for (double x : m) if (!std::isfinite(x)) return false;
  return true;
}

bool vec3_finite(const Vec3& v) {
  for (double x : v) if (!std::isfinite(x)) return false;
  return true;
}

double vec3_dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

Vec3 vec3_cross(const Vec3& a, const Vec3& b) {
  return Vec3{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double vec3_norm(const Vec3& a) { return std::sqrt(vec3_dot(a, a)); }

Vec3 vec3_normalize(const Vec3& a) {
  const double n = vec3_norm(a);
  if (n == 0.0) return a;
  return Vec3{a[0] / n, a[1] / n, a[2] / n};
}

double angle_between(const Vec3& a, const Vec3& b) {
  const Vec3 na = vec3_normalize(a), nb = vec3_normalize(b);
  const double cross_norm = vec3_norm(vec3_cross(na, nb));
  const double dot = std::clamp(vec3_dot(na, nb), -1.0, 1.0);
  return std::atan2(cross_norm, dot);
}

}  // namespace acsd::model
