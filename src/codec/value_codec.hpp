#pragma once
// Shared value-level JSON codecs.
//
// One implementation of the canonical JSON shape for the flat scientific value
// types, linked by both apps/contract and the generated runtime typed codecs.
// The value level deliberately knows nothing about arrays, providers or the
// object table: those travel as object-table aliases and are handled by the
// generated wire codecs (src/runtime/generated) on top of this library.
#include <acsd/model.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace acsd::codec {

using json = nlohmann::json;
using Done = std::monostate;

// ---------------------------------------------------------------------------
// Canonical flat value codecs (shared with the contract driver)
// ---------------------------------------------------------------------------

extern const json kNull;

bool has(const json& node, const char* key);
json field(const json& node, const char* key);
std::string string_field(const json& node, const char* key,
                         const std::string& fallback = {});
double double_field(const json& node, const char* key, double fallback = 0.0);
bool bool_field(const json& node, const char* key, bool fallback = false);
U64 u64_field(const json& node, const char* key, U64 fallback = 0);
std::int32_t i32_field(const json& node, const char* key,
                       std::int32_t fallback = 0);

std::string canonical_json(const json& node);
Id canonical_hash(const json& node);
json encode_f64(double value);
double decode_f64(const json& node);

json encode_code(Code code);
Code decode_code(const json& node);
const char* noise_state_name(NoiseState state);
NoiseState noise_state_from(const std::string& name);

json encode_unit(const Unit& unit);
Unit decode_unit(const json& node);
json encode_interval(const Interval& interval);
Interval decode_interval(const json& node);
json encode_exact_interval(const ExactInterval& exact);
ExactInterval decode_exact_interval(const json& node);
json encode_box(const Box& box);
Box decode_box(const json& node);
json encode_pixel_center(const PixelCenter0& point);
PixelCenter0 decode_pixel_center(const json& node);
json encode_pixel_edge(const PixelEdge0& point);
PixelEdge0 decode_pixel_edge(const json& node);
json encode_fits_pixel(const FitsPixel1& point);
FitsPixel1 decode_fits_pixel(const json& node);
json encode_sky(const SkyICRS& sky);
SkyICRS decode_sky(const json& node);
json encode_leaf(const HealpixLeaf& leaf);
HealpixLeaf decode_leaf(const json& node);
json encode_tolerance(const Tolerance& tolerance);
Tolerance decode_tolerance(const json& node);
json encode_arithmetic(const Arithmetic& arithmetic);
Arithmetic decode_arithmetic(const json& node);
json encode_numeric_budget(const NumericBudget& budget);
NumericBudget decode_numeric_budget(const json& node);
json encode_solve_policy(const SolvePolicy& policy);
SolvePolicy decode_solve_policy(const json& node);
json encode_astrometric_context(const AstrometricContext& context);
AstrometricContext decode_astrometric_context(const json& node);
json encode_header_fields(const HeaderFields& fields);
HeaderFields decode_header_fields(const json& node);
json encode_block_request(const BlockRequest& request);
BlockRequest decode_block_request(const json& node);
json encode_limits(const Limits& limits);
Limits decode_limits(const json& node);
json encode_condition(const Condition& condition);
Condition decode_condition(const json& node);

// ---------------------------------------------------------------------------
// Strict wire helpers used by the generated codecs
// ---------------------------------------------------------------------------

template <class T>
Result<T> failure(Code code, std::string_view origin, std::string reason,
                  std::string_view impact = {}) {
  Result<T> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  Issue issue;
  issue.code = code;
  issue.origin = std::string(origin);
  issue.reason = std::move(reason);
  issue.impact = std::string(impact);
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

template <class T>
Result<T> ok(T value) {
  Result<T> out;
  out.code = Code::ok;
  out.assessment.complete = true;
  out.assessment.validity = Validity::certified;
  out.value = std::move(value);
  return out;
}

template <class T, class U>
Result<T> remap(const Result<U>& from) {
  Result<T> out;
  out.code = from.code;
  out.assessment = from.assessment;
  out.usage = from.usage;
  out.dependencies = from.dependencies;
  return out;
}

const json* member(const json& node, std::string_view key) noexcept;
Result<const json*> require_member(const json& node, std::string_view key,
                                   std::string_view origin);
Result<Done> reject_unknown(const json& node,
                            std::initializer_list<std::string_view> allowed,
                            std::string_view origin);
Result<bool> decode_bool(const json& node, std::string_view origin);
Result<std::uint8_t> decode_u8(const json& node, std::string_view origin);
Result<std::uint16_t> decode_u16(const json& node, std::string_view origin);
Result<U32> decode_u32(const json& node, std::string_view origin);
Result<U64> decode_u64(const json& node, std::string_view origin);
Result<std::int32_t> decode_i32(const json& node, std::string_view origin);
Result<std::int64_t> decode_i64(const json& node, std::string_view origin);
Result<double> decode_double(const json& node, std::string_view origin);
json encode_double(double value);
Result<std::string> decode_string(const json& node, std::string_view origin);
// An Id is a non-empty UTF-8 identity string (include/acsd/model.hpp).
Result<Id> decode_id(const json& node, std::string_view origin);
Result<json> parse_json_object(std::string_view text, std::string_view origin);

}  // namespace acsd::codec
