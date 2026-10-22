#pragma once
// Units, dyadic endpoints, identity and assessment helpers for ACSD v2.
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/exact.hpp"

namespace acsd::model {

// ---------------------------------------------------------------------------
// Units
// ---------------------------------------------------------------------------

Unit unit_dimensionless();
Unit unit_adu(const std::string& identity);
Unit unit_signal(const std::string& identity);          // L, linear power 1
Unit unit_variance(const std::string& identity);        // L^2, linear power 2
Unit unit_sr();
Unit unit_sr_per_pixel();
Unit unit_per_sr();                                     // 1/sr
Unit unit_nm();
Unit unit_seconds();
bool units_equal(const Unit& a, const Unit& b);
bool unit_is_dimensionless(const Unit& a);
bool unit_has_linear_power(const Unit& a, int power);
// Multiplies two units: linear powers add, exponents add, multiplier multiplies.
Unit unit_multiply(const Unit& a, const Unit& b);
Unit unit_square(const Unit& a);
std::string unit_to_string(const Unit& a);

// ---------------------------------------------------------------------------
// Dyadic endpoints (ExactInterval)
// ---------------------------------------------------------------------------

// Converts a binary64 value into the exact dyadic pair used by ExactInterval.
Dyadic dyadic_from_double(double value);
double dyadic_to_double(const Dyadic& value);
ExactInterval exact_point(double value);
ExactInterval exact_from_interval(const Interval& value);
ExactInterval exact_widen(const ExactInterval& value, double outward_ulp);
ExactInterval exact_add(const ExactInterval& a, const ExactInterval& b);
ExactInterval exact_mul(const ExactInterval& a, const ExactInterval& b);
bool exact_less(const ExactInterval& a, const ExactInterval& b);
bool exact_contains(const ExactInterval& a, const ExactInterval& b);
std::string exact_to_string(const ExactInterval& value);

// ---------------------------------------------------------------------------
// Intervals
// ---------------------------------------------------------------------------

Interval interval_point(double value);
Interval interval_from_bounds(double lower, double upper);
bool interval_valid(const Interval& value);
double interval_width(const Interval& value);
Interval interval_add(const Interval& a, const Interval& b);
Interval interval_mul(const Interval& a, const Interval& b);
Interval interval_scale(const Interval& a, double factor);
bool interval_contains(const Interval& outer, const Interval& inner);
bool interval_contains_zero(const Interval& value);
std::string interval_to_string(const Interval& value);

// ---------------------------------------------------------------------------
// Identity and assessment helpers
// ---------------------------------------------------------------------------

Identity make_identity(const std::string& schema_id, U32 major, U32 minor,
                       const std::string& content_hash_hex, const std::string& run_id);
Assessment make_assessment(Code code, Capability capability = Capability::supported);
void add_issue(Assessment& assessment, Code code, const std::string& reason,
               const std::string& origin = {}, const std::string& impact = {},
               const std::string& retry = {});
void set_included(Assessment& assessment, std::string item);
void set_omitted(Assessment& assessment, std::string item);
Condition make_condition(const std::string& estimator, const std::string& kind);

// Convenience helpers used by every module so that failure paths share one
// consistent representation.
template <class T>
Result<T> fail(Code code, const std::string& reason, const std::string& origin = {},
                const std::string& impact = {}, const std::string& retry = {}) {
  Result<T> out;
  out.code = code;
  out.assessment = make_assessment(code, code == Code::unsupported ? Capability::unsupported
                                             : code == Code::unavailable ? Capability::unavailable
                                                                        : Capability::supported);
  add_issue(out.assessment, code, reason, origin, impact, retry);
  out.assessment.execution = ExecutionState::failed;
  return out;
}

template <class T>
Result<T> ok_result(T value) {
  Result<T> out;
  out.code = Code::ok;
  out.value = std::move(value);
  out.assessment = make_assessment(Code::ok);
  out.assessment.complete = true;
  out.assessment.execution = ExecutionState::complete;
  return out;
}

// Linear algebra helpers on the small fixed size vectors used by contracts.
double vec_norm2(const std::array<double, 6>& v);
double mat2_det(const Mat2& m);
Mat2 mat2_inverse(const Mat2& m);
bool mat2_finite(const Mat2& m);
bool vec3_finite(const Vec3& v);
double vec3_dot(const Vec3& a, const Vec3& b);
Vec3 vec3_cross(const Vec3& a, const Vec3& b);
double vec3_norm(const Vec3& a);
Vec3 vec3_normalize(const Vec3& a);
double angle_between(const Vec3& a, const Vec3& b);

}  // namespace acsd::model
