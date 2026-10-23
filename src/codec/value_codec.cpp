// Shared value-level JSON codecs.
//
// This library owns the canonical JSON representation of the flat scientific
// value types (units, intervals, boxes, astrometry context, numeric budgets,
// conditions, codes, ...).  It is linked by both the contract driver
// (apps/contract) and the runtime typed codecs, so a value has exactly one
// encoder.  The wire decoder generated from include/acsd/model.hpp adds
// strict field presence and object-alias handling on top; the tolerant
// decoders here are the contract-driver semantics and stay byte compatible
// with the existing validation envelopes.
#include "codec/value_codec.hpp"

#include <cmath>
#include <cstring>
#include <sstream>

#include "model/hash.hpp"

namespace acsd::codec {

using model::content_id;

const json kNull = json(nullptr);

bool has(const json& node, const char* key) {
  return node.is_object() && node.contains(key) && !node.at(key).is_null();
}

json field(const json& node, const char* key) {
  if (!node.is_object() || !node.contains(key)) return kNull;
  return node.at(key);
}

std::string string_field(const json& node, const char* key, const std::string& fallback) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (!value.is_string()) return fallback;
  return value.get<std::string>();
}

double double_field(const json& node, const char* key, double fallback) {
  if (!has(node, key)) return fallback;
  return decode_f64(node.at(key));
}

bool bool_field(const json& node, const char* key, bool fallback) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (!value.is_boolean()) return fallback;
  return value.get<bool>();
}

U64 u64_field(const json& node, const char* key, U64 fallback) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (value.is_number_unsigned()) return value.get<U64>();
  if (value.is_number_integer()) {
    const std::int64_t signed_value = value.get<std::int64_t>();
    if (signed_value < 0) return fallback;
    return static_cast<U64>(signed_value);
  }
  return fallback;
}

std::int32_t i32_field(const json& node, const char* key, std::int32_t fallback) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (value.is_number_integer()) return value.get<std::int32_t>();
  return fallback;
}

std::string canonical_json(const json& node) { return node.dump(); }

Id canonical_hash(const json& node) {
  return content_id(model::sha256_hex(canonical_json(node)));
}

json encode_f64(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  std::ostringstream stream;
  stream << std::hex;
  for (int i = 0; i < 8; ++i) {
    stream.width(2);
    stream.fill('0');
    stream << ((bits >> (8 * i)) & 0xffull);
  }
  return json{{"f64", stream.str()}};
}

double decode_f64(const json& node) {
  if (node.is_number()) return node.get<double>();
  if (node.is_object() && node.contains("f64")) {
    const std::string hex = node.at("f64").get<std::string>();
    if (hex.size() != 16) return std::nan("");
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
      const std::string byte = hex.substr(static_cast<std::size_t>(2 * i), 2);
      bits |= static_cast<std::uint64_t>(std::stoul(byte, nullptr, 16)) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  return std::nan("");
}

json encode_unit(const Unit& unit) {
  json node;
  node["scale"] = unit.scale;
  node["sr_power"] = unit.sr_power;
  node["pixel_power"] = unit.pixel_power;
  node["nm_power"] = unit.nm_power;
  node["multiplier"] = unit.multiplier;
  node["linear_power"] = unit.linear_power;
  return node;
}

Unit decode_unit(const json& node) {
  Unit unit;
  unit.scale = string_field(node, "scale", "1");
  unit.sr_power = i32_field(node, "sr_power", 0);
  unit.pixel_power = i32_field(node, "pixel_power", 0);
  unit.nm_power = i32_field(node, "nm_power", 0);
  unit.multiplier = double_field(node, "multiplier", 1.0);
  unit.linear_power = i32_field(node, "linear_power", 0);
  return unit;
}

Interval decode_interval(const json& node) {
  Interval interval;
  interval.lower = double_field(node, "lower", 0.0);
  interval.upper = double_field(node, "upper", 0.0);
  return interval;
}

ExactInterval decode_exact_interval(const json& node) {
  ExactInterval exact;
  const json& lower = field(node, "lower");
  const json& upper = field(node, "upper");
  if (lower.is_object()) {
    exact.lower.signed_integer = string_field(lower, "signed_integer", "0");
    exact.lower.exponent2 = i32_field(lower, "exponent2", 0);
  }
  if (upper.is_object()) {
    exact.upper.signed_integer = string_field(upper, "signed_integer", "0");
    exact.upper.exponent2 = i32_field(upper, "exponent2", 0);
  }
  exact.precision_bits = u64_field(node, "precision_bits", 64);
  return exact;
}

Box decode_box(const json& node) {
  Box box;
  const json& lower = field(node, "lower");
  const json& upper = field(node, "upper");
  if (lower.is_array() && lower.size() == 2) {
    box.lower = {decode_f64(lower[0]), decode_f64(lower[1])};
  }
  if (upper.is_array() && upper.size() == 2) {
    box.upper = {decode_f64(upper[0]), decode_f64(upper[1])};
  }
  return box;
}

PixelCenter0 decode_pixel_center(const json& node) {
  PixelCenter0 point;
  point.x = double_field(node, "x", 0.0);
  point.y = double_field(node, "y", 0.0);
  return point;
}

PixelEdge0 decode_pixel_edge(const json& node) {
  PixelEdge0 point;
  point.x = double_field(node, "x", 0.0);
  point.y = double_field(node, "y", 0.0);
  return point;
}

FitsPixel1 decode_fits_pixel(const json& node) {
  FitsPixel1 point;
  point.x = double_field(node, "x", 0.0);
  point.y = double_field(node, "y", 0.0);
  return point;
}

SkyICRS decode_sky(const json& node) {
  SkyICRS sky;
  sky.nx = double_field(node, "nx", 0.0);
  sky.ny = double_field(node, "ny", 0.0);
  sky.nz = double_field(node, "nz", 0.0);
  return sky;
}

HealpixLeaf decode_leaf(const json& node) {
  HealpixLeaf leaf;
  leaf.order = static_cast<U32>(u64_field(node, "order", 0));
  leaf.ipix = u64_field(node, "ipix", 0);
  return leaf;
}

Tolerance decode_tolerance(const json& node) {
  Tolerance tolerance;
  tolerance.absolute = double_field(node, "absolute", 0.0);
  tolerance.relative = double_field(node, "relative", 0.0);
  tolerance.absolute_floor = double_field(node, "absolute_floor", 0.0);
  tolerance.unit = decode_unit(field(node, "unit"));
  return tolerance;
}

Arithmetic decode_arithmetic(const json& node) {
  Arithmetic arithmetic;
  const std::string mode = string_field(node, "mode", "outward");
  arithmetic.mode = mode == "approximate" ? Arithmetic::Mode::approximate : Arithmetic::Mode::outward;
  arithmetic.initial_bits = static_cast<U32>(u64_field(node, "initial_bits", 64));
  arithmetic.max_bits = static_cast<U32>(u64_field(node, "max_bits", 256));
  arithmetic.allow_target_fallback = bool_field(node, "allow_target_fallback", false);
  return arithmetic;
}

json encode_arithmetic(const Arithmetic& arithmetic) {
  json node;
  node["mode"] = json(arithmetic.mode == Arithmetic::Mode::approximate ? "approximate" : "outward");
  node["initial_bits"] = arithmetic.initial_bits;
  node["max_bits"] = arithmetic.max_bits;
  node["allow_target_fallback"] = arithmetic.allow_target_fallback;
  return node;
}

NumericBudget decode_numeric_budget(const json& node) {
  NumericBudget budget;
  budget.tolerance = decode_tolerance(field(node, "tolerance"));
  budget.max_iterations = static_cast<U32>(u64_field(node, "max_iterations", 0));
  budget.arithmetic = decode_arithmetic(field(node, "arithmetic"));
  return budget;
}

SolvePolicy decode_solve_policy(const json& node) {
  SolvePolicy policy;
  policy.relative_residual = double_field(node, "relative_residual", 1e-10);
  policy.absolute_residual = double_field(node, "absolute_residual", 1e-12);
  policy.rank_relative = double_field(node, "rank_relative", 1e-12);
  policy.max_iterations = static_cast<U32>(u64_field(node, "max_iterations", 200));
  policy.dense_max_bytes = u64_field(node, "dense_max_bytes", 64ull * 1024 * 1024);
  policy.huber = bool_field(node, "huber", false);
  policy.huber_delta = double_field(node, "huber_delta", 1.345);
  return policy;
}

AstrometricContext decode_astrometric_context(const json& node) {
  AstrometricContext context;
  context.epoch_julian_year = double_field(node, "epoch_julian_year", 0.0);
  context.observation_jd_tdb = double_field(node, "observation_jd_tdb", 0.0);
  const json& barycentric = field(node, "barycentric_observer_au");
  if (barycentric.is_array() && barycentric.size() == 3) {
    context.barycentric_observer_au =
        Vec3{decode_f64(barycentric[0]), decode_f64(barycentric[1]), decode_f64(barycentric[2])};
  }
  const json& geodetic = field(node, "geodetic_lon_lat_height_rad_m");
  if (geodetic.is_array() && geodetic.size() == 3) {
    context.geodetic_lon_lat_height_rad_m =
        Vec3{decode_f64(geodetic[0]), decode_f64(geodetic[1]), decode_f64(geodetic[2])};
  }
  const json& omitted = field(node, "omitted");
  if (omitted.is_array()) {
    for (const json& item : omitted) {
      if (item.is_string()) context.omitted.push_back(item.get<std::string>());
    }
  }
  if (has(node, "omitted_position_bound_rad")) {
    context.omitted_position_bound_rad = decode_f64(node.at("omitted_position_bound_rad"));
  }
  return context;
}

HeaderFields decode_header_fields(const json& node) {
  HeaderFields fields;
  if (!node.is_array()) return fields;
  for (const json& item : node) {
    HeaderField entry;
    entry.key = string_field(item, "key");
    const json& value = field(item, "value");
    if (value.is_object() && value.contains("alternative")) {
      const std::string alternative = value.at("alternative").get<std::string>();
      const json& inner = value.at("value");
      if (alternative == "std::string") {
        entry.value = inner.get<std::string>();
      } else if (alternative == "double") {
        entry.value = decode_f64(inner);
      } else if (alternative == "std::int64_t") {
        entry.value = static_cast<std::int64_t>(inner.get<std::int64_t>());
      }
    } else if (value.is_string()) {
      entry.value = value.get<std::string>();
    } else if (value.is_number()) {
      entry.value = decode_f64(value);
    }
    fields.push_back(entry);
  }
  return fields;
}

json encode_header_fields(const HeaderFields& fields) {
  json node = json::array();
  for (const HeaderField& field : fields) {
    json entry;
    entry["key"] = field.key;
    if (std::holds_alternative<std::string>(field.value)) {
      entry["value"] = json{{"alternative", "std::string"},
                            {"value", std::get<std::string>(field.value)}};
    } else if (std::holds_alternative<double>(field.value)) {
      entry["value"] = json{{"alternative", "double"}, {"value", std::get<double>(field.value)}};
    } else {
      entry["value"] = json{{"alternative", "std::int64_t"},
                            {"value", std::get<std::int64_t>(field.value)}};
    }
    node.push_back(entry);
  }
  return node;
}

BlockRequest decode_block_request(const json& node) {
  BlockRequest request;
  request.begin = u64_field(node, "begin", 0);
  request.count = u64_field(node, "count", 0);
  return request;
}


Limits decode_limits(const json& node) {
  Limits limits;
  limits.bytes = u64_field(node, "bytes", 0);
  limits.work = u64_field(node, "work", 0);
  limits.spill_bytes = u64_field(node, "spill_bytes", 0);
  limits.threads = static_cast<U32>(u64_field(node, "threads", 1));
  limits.max_depth = static_cast<U32>(u64_field(node, "max_depth", 64));
  limits.max_precision_bits = static_cast<U32>(u64_field(node, "max_precision_bits", 64));
  limits.scratch_directory = string_field(node, "scratch_directory", ".");
  return limits;
}

namespace {
struct CodeName {
  Code code;
  const char* name;
};

const CodeName kCodeNames[] = {
    {Code::ok, "ok"},
    {Code::invalid_request, "invalid_request"},
    {Code::invalid_model, "invalid_model"},
    {Code::domain, "domain"},
    {Code::unsupported, "unsupported"},
    {Code::unavailable, "unavailable"},
    {Code::unsolved, "unsolved"},
    {Code::not_identifiable, "not_identifiable"},
    {Code::invalid_covariance, "invalid_covariance"},
    {Code::duplicate_source, "duplicate_source"},
    {Code::identity_conflict, "identity_conflict"},
    {Code::hash_mismatch, "hash_mismatch"},
    {Code::corrupt_product, "corrupt_product"},
    {Code::missing_dependency, "missing_dependency"},
    {Code::no_support, "no_support"},
    {Code::support_unresolved, "support_unresolved"},
    {Code::geometry_solve_unresolved, "geometry_solve_unresolved"},
    {Code::selection_unresolved, "selection_unresolved"},
    {Code::nonconverged, "nonconverged"},
    {Code::precision_exhausted, "precision_exhausted"},
    {Code::resource_exhausted, "resource_exhausted"},
    {Code::io_error, "io_error"},
    {Code::cancelled, "cancelled"},
    {Code::internal, "internal"},
};

}  // namespace

const char* noise_state_name(NoiseState state) {
  switch (state) {
    case NoiseState::known: return "known";
    case NoiseState::conditional: return "conditional";
    case NoiseState::approximate: return "approximate";
    case NoiseState::unknown: return "unknown";
  }
  return "unknown";
}

NoiseState noise_state_from(const std::string& name) {
  if (name == "known") return NoiseState::known;
  if (name == "conditional") return NoiseState::conditional;
  if (name == "approximate") return NoiseState::approximate;
  return NoiseState::unknown;
}

json encode_code(Code code) {
  for (const CodeName& entry : kCodeNames) {
    if (entry.code == code) return json(entry.name);
  }
  return json("internal");
}

Code decode_code(const json& node) {
  if (node.is_string()) {
    const std::string name = node.get<std::string>();
    for (const CodeName& entry : kCodeNames) {
      if (name == entry.name) return entry.code;
    }
  }
  return Code::invalid_request;
}

json encode_condition(const Condition& condition) {
  json node;
  node["estimator"] = condition.estimator;
  node["kind"] = condition.kind;
  node["fixed_objects"] = condition.fixed_objects;
  node["assumptions"] = condition.assumptions;
  return node;
}
// ---------------------------------------------------------------------------
// Encoders for the flat value types (one canonical shape per type)
// ---------------------------------------------------------------------------

json encode_interval(const Interval& interval) {
  return json{{"lower", encode_f64(interval.lower)},
              {"upper", encode_f64(interval.upper)}};
}

json encode_exact_interval(const ExactInterval& exact) {
  json lower;
  lower["signed_integer"] = exact.lower.signed_integer;
  lower["exponent2"] = exact.lower.exponent2;
  json upper;
  upper["signed_integer"] = exact.upper.signed_integer;
  upper["exponent2"] = exact.upper.exponent2;
  json node;
  node["lower"] = lower;
  node["upper"] = upper;
  node["precision_bits"] = exact.precision_bits;
  return node;
}

json encode_box(const Box& box) {
  return json{{"lower", json::array({encode_f64(box.lower[0]),
                                     encode_f64(box.lower[1])})},
              {"upper", json::array({encode_f64(box.upper[0]),
                                     encode_f64(box.upper[1])})}};
}

json encode_pixel_center(const PixelCenter0& point) {
  return json{{"x", encode_f64(point.x)}, {"y", encode_f64(point.y)}};
}

json encode_pixel_edge(const PixelEdge0& point) {
  return json{{"x", encode_f64(point.x)}, {"y", encode_f64(point.y)}};
}

json encode_fits_pixel(const FitsPixel1& point) {
  return json{{"x", encode_f64(point.x)}, {"y", encode_f64(point.y)}};
}

json encode_sky(const SkyICRS& sky) {
  return json{{"nx", encode_f64(sky.nx)}, {"ny", encode_f64(sky.ny)},
              {"nz", encode_f64(sky.nz)}};
}

json encode_leaf(const HealpixLeaf& leaf) {
  return json{{"order", leaf.order}, {"ipix", leaf.ipix}};
}

json encode_tolerance(const Tolerance& tolerance) {
  json node;
  node["absolute"] = encode_f64(tolerance.absolute);
  node["relative"] = encode_f64(tolerance.relative);
  node["absolute_floor"] = encode_f64(tolerance.absolute_floor);
  node["unit"] = encode_unit(tolerance.unit);
  return node;
}

json encode_numeric_budget(const NumericBudget& budget) {
  json node;
  node["tolerance"] = encode_tolerance(budget.tolerance);
  node["max_iterations"] = budget.max_iterations;
  node["arithmetic"] = encode_arithmetic(budget.arithmetic);
  return node;
}

json encode_solve_policy(const SolvePolicy& policy) {
  json node;
  node["relative_residual"] = encode_f64(policy.relative_residual);
  node["absolute_residual"] = encode_f64(policy.absolute_residual);
  node["rank_relative"] = encode_f64(policy.rank_relative);
  node["max_iterations"] = policy.max_iterations;
  node["dense_max_bytes"] = policy.dense_max_bytes;
  node["huber"] = policy.huber;
  node["huber_delta"] = encode_f64(policy.huber_delta);
  return node;
}

json encode_astrometric_context(const AstrometricContext& context) {
  json node;
  node["epoch_julian_year"] = encode_f64(context.epoch_julian_year);
  node["observation_jd_tdb"] = encode_f64(context.observation_jd_tdb);
  if (context.barycentric_observer_au.has_value()) {
    node["barycentric_observer_au"] =
        json::array({encode_f64((*context.barycentric_observer_au)[0]),
                     encode_f64((*context.barycentric_observer_au)[1]),
                     encode_f64((*context.barycentric_observer_au)[2])});
  } else {
    node["barycentric_observer_au"] = kNull;
  }
  if (context.geodetic_lon_lat_height_rad_m.has_value()) {
    node["geodetic_lon_lat_height_rad_m"] = json::array(
        {encode_f64((*context.geodetic_lon_lat_height_rad_m)[0]),
         encode_f64((*context.geodetic_lon_lat_height_rad_m)[1]),
         encode_f64((*context.geodetic_lon_lat_height_rad_m)[2])});
  } else {
    node["geodetic_lon_lat_height_rad_m"] = kNull;
  }
  node["omitted"] = context.omitted;
  node["omitted_position_bound_rad"] =
      context.omitted_position_bound_rad.has_value()
          ? encode_f64(*context.omitted_position_bound_rad)
          : kNull;
  return node;
}

json encode_block_request(const BlockRequest& request) {
  return json{{"begin", request.begin}, {"count", request.count}};
}

json encode_limits(const Limits& limits) {
  json node;
  node["bytes"] = limits.bytes;
  node["work"] = limits.work;
  node["spill_bytes"] = limits.spill_bytes;
  node["threads"] = limits.threads;
  node["max_depth"] = limits.max_depth;
  node["max_precision_bits"] = limits.max_precision_bits;
  node["scratch_directory"] = limits.scratch_directory;
  return node;
}

Condition decode_condition(const json& node) {
  Condition condition;
  condition.estimator = string_field(node, "estimator");
  condition.kind = string_field(node, "kind");
  const json& fixed_objects = field(node, "fixed_objects");
  if (fixed_objects.is_array()) {
    for (const json& item : fixed_objects) {
      if (item.is_string()) {
        condition.fixed_objects.push_back(item.get<std::string>());
      }
    }
  }
  const json& assumptions = field(node, "assumptions");
  if (assumptions.is_array()) {
    for (const json& item : assumptions) {
      if (item.is_string()) {
        condition.assumptions.push_back(item.get<std::string>());
      }
    }
  }
  return condition;
}

// ---------------------------------------------------------------------------
// Strict wire helpers (runtime typed codecs)
// ---------------------------------------------------------------------------

const json* member(const json& node, std::string_view key) noexcept {
  if (!node.is_object()) return nullptr;
  auto found = node.find(key);
  if (found == node.end()) return nullptr;
  return &*found;
}

Result<const json*> require_member(const json& node, std::string_view key,
                                   std::string_view origin) {
  if (!node.is_object()) {
    return failure<const json*>(Code::invalid_request, origin,
                                "the payload is not a JSON object");
  }
  auto found = node.find(key);
  if (found == node.end()) {
    return failure<const json*>(
        Code::invalid_request, origin,
        "required field '" + std::string(key) + "' is missing");
  }
  return ok<const json*>(&*found);
}

Result<Done> reject_unknown(const json& node,
                            std::initializer_list<std::string_view> allowed,
                            std::string_view origin) {
  if (!node.is_object()) {
    return failure<Done>(Code::invalid_request, origin,
                         "the payload is not a JSON object");
  }
  for (auto entry = node.begin(); entry != node.end(); ++entry) {
    bool known = false;
    for (std::string_view name : allowed) {
      if (entry.key() == name) {
        known = true;
        break;
      }
    }
    if (!known) {
      return failure<Done>(Code::invalid_request, origin,
                           "unknown field '" + entry.key() + "'");
    }
  }
  return ok<Done>(Done{});
}

Result<bool> decode_bool(const json& node, std::string_view origin) {
  if (!node.is_boolean()) {
    return failure<bool>(Code::invalid_request, origin, "expected a boolean");
  }
  return ok<bool>(node.get<bool>());
}

Result<std::uint8_t> decode_u8(const json& node, std::string_view origin) {
  if (!node.is_number_unsigned() && !node.is_number_integer()) {
    return failure<std::uint8_t>(Code::invalid_request, origin,
                                 "expected an integer");
  }
  const std::int64_t value = node.get<std::int64_t>();
  if (value < 0 || value > 255) {
    return failure<std::uint8_t>(Code::invalid_request, origin,
                                 "integer is outside 0..255");
  }
  return ok<std::uint8_t>(static_cast<std::uint8_t>(value));
}

Result<std::uint16_t> decode_u16(const json& node, std::string_view origin) {
  if (!node.is_number_unsigned() && !node.is_number_integer()) {
    return failure<std::uint16_t>(Code::invalid_request, origin,
                                  "expected an integer");
  }
  const std::int64_t value = node.get<std::int64_t>();
  if (value < 0 || value > 65535) {
    return failure<std::uint16_t>(Code::invalid_request, origin,
                                  "integer is outside 0..65535");
  }
  return ok<std::uint16_t>(static_cast<std::uint16_t>(value));
}

Result<U32> decode_u32(const json& node, std::string_view origin) {
  if (!node.is_number_unsigned() && !node.is_number_integer()) {
    return failure<U32>(Code::invalid_request, origin, "expected an integer");
  }
  const std::int64_t value = node.get<std::int64_t>();
  if (value < 0 || value > 4294967295ll) {
    return failure<U32>(Code::invalid_request, origin,
                        "integer is outside 0..4294967295");
  }
  return ok<U32>(static_cast<U32>(value));
}

Result<U64> decode_u64(const json& node, std::string_view origin) {
  if (!node.is_number_unsigned() && !node.is_number_integer()) {
    return failure<U64>(Code::invalid_request, origin, "expected an integer");
  }
  if (node.is_number_integer() && !node.is_number_unsigned()) {
    const std::int64_t value = node.get<std::int64_t>();
    if (value < 0) {
      return failure<U64>(Code::invalid_request, origin,
                          "integer is negative");
    }
    return ok<U64>(static_cast<U64>(value));
  }
  return ok<U64>(node.get<U64>());
}

Result<std::int32_t> decode_i32(const json& node, std::string_view origin) {
  if (!node.is_number_integer()) {
    return failure<std::int32_t>(Code::invalid_request, origin,
                                 "expected an integer");
  }
  const std::int64_t value = node.get<std::int64_t>();
  if (value < -2147483648ll || value > 2147483647ll) {
    return failure<std::int32_t>(Code::invalid_request, origin,
                                 "integer is outside int32");
  }
  return ok<std::int32_t>(static_cast<std::int32_t>(value));
}

Result<std::int64_t> decode_i64(const json& node, std::string_view origin) {
  if (!node.is_number_integer()) {
    return failure<std::int64_t>(Code::invalid_request, origin,
                                 "expected an integer");
  }
  return ok<std::int64_t>(node.get<std::int64_t>());
}

Result<double> decode_double(const json& node, std::string_view origin) {
  if (node.is_number()) return ok<double>(node.get<double>());
  if (node.is_object() && node.contains("f64")) {
    return ok<double>(decode_f64(node));
  }
  return failure<double>(Code::invalid_request, origin,
                         "expected a number or an {\"f64\": \"...\"} object");
}

json encode_double(double value) {
  if (std::isfinite(value)) return json(value);
  return encode_f64(value);
}

Result<std::string> decode_string(const json& node, std::string_view origin) {
  if (!node.is_string()) {
    return failure<std::string>(Code::invalid_request, origin,
                                "expected a string");
  }
  return ok<std::string>(node.get<std::string>());
}

Result<Id> decode_id(const json& node, std::string_view origin) {
  auto decoded = decode_string(node, origin);
  if (decoded.code != Code::ok) return codec::remap<Id>(decoded);
  if (decoded.value->empty()) {
    return failure<Id>(Code::invalid_request, origin, "an Id is empty");
  }
  return codec::ok<Id>(std::move(*decoded.value));
}

Result<json> parse_json_object(std::string_view text, std::string_view origin) {
  json parsed = json::parse(text.begin(), text.end(), nullptr, false);
  if (parsed.is_discarded()) {
    return failure<json>(Code::invalid_request, origin,
                         "the payload is not valid JSON");
  }
  if (!parsed.is_object()) {
    return failure<json>(Code::invalid_request, origin,
                         "the payload root is not a JSON object");
  }
  return ok<json>(std::move(parsed));
}

}  // namespace acsd::codec
