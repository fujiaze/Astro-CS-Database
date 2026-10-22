// JSON encoding and decoding for the contract driver.
#include "json_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd::contract {
namespace {

using model::content_id;

const json kNull = json(nullptr);

bool has(const json& node, const char* key) {
  return node.is_object() && node.contains(key) && !node.at(key).is_null();
}

json field(const json& node, const char* key) {
  if (!node.is_object() || !node.contains(key)) return kNull;
  return node.at(key);
}

std::string string_field(const json& node, const char* key, const std::string& fallback = {}) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (!value.is_string()) return fallback;
  return value.get<std::string>();
}

double double_field(const json& node, const char* key, double fallback = 0.0) {
  if (!has(node, key)) return fallback;
  return decode_f64(node.at(key));
}

bool bool_field(const json& node, const char* key, bool fallback = false) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (!value.is_boolean()) return fallback;
  return value.get<bool>();
}

U64 u64_field(const json& node, const char* key, U64 fallback = 0) {
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

std::int32_t i32_field(const json& node, const char* key, std::int32_t fallback = 0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (value.is_number_integer()) return value.get<std::int32_t>();
  return fallback;
}

// The payload of a typed object: {"type": T, "data": {...}} or, for arrays,
// {"type": "Array", "dtype": ...}.
json payload_of(const json& node) {
  if (node.is_object() && node.contains("data")) return node.at("data");
  return node;
}

std::string type_of(const json& node) {
  if (!node.is_object() || !node.contains("type")) return {};
  const json& value = node.at("type");
  if (!value.is_string()) return {};
  return value.get<std::string>();
}

std::string reference_name(const json& node) {
  if (node.is_string()) return node.get<std::string>();
  if (node.is_object() && node.contains("$ref")) {
    const json& value = node.at("$ref");
    if (value.is_string()) return value.get<std::string>();
  }
  return {};
}

}  // namespace

// ---------------------------------------------------------------------------
// Canonical JSON and hashing
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

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
}  // namespace

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

// ---------------------------------------------------------------------------
// Plain values
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Model level encoders
// ---------------------------------------------------------------------------

json encode_identity(const Identity& identity) {
  json node;
  node["object_id"] = identity.object_id;
  node["content_hash"] = identity.content_hash;
  node["run_id"] = identity.run_id;
  node["revision"] = identity.revision;
  node["schema_id"] = identity.schema_id;
  node["schema_major"] = identity.schema_major;
  node["schema_minor"] = identity.schema_minor;
  return node;
}

json encode_condition(const Condition& condition) {
  json node;
  node["estimator"] = condition.estimator;
  node["kind"] = condition.kind;
  node["fixed_objects"] = condition.fixed_objects;
  node["assumptions"] = condition.assumptions;
  return node;
}

json encode_assessment(const Assessment& assessment) {
  json node;
  node["complete"] = assessment.complete;
  node["validity"] = json(assessment.validity == Validity::certified
                              ? "certified"
                              : (assessment.validity == Validity::approximate ? "approximate"
                                                                        : "invalid"));
  node["science"] = json(assessment.science == ScienceState::bounded
                              ? "bounded"
                              : (assessment.science == ScienceState::unconverged
                                     ? "unconverged"
                                     : (assessment.science == ScienceState::conditional
                                            ? "conditional"
                                            : (assessment.science == ScienceState::selection_unresolved
                                                   ? "selection_unresolved"
                                                   : "geometry_solve_unresolved"))));
  node["noise"] = json(noise_state_name(assessment.noise));
  node["included"] = assessment.included;
  node["omitted"] = assessment.omitted;
  json issues = json::array();
  for (const Issue& issue : assessment.issues) {
    json entry;
    entry["code"] = encode_code(issue.code);
    entry["origin"] = issue.origin;
    entry["reason"] = issue.reason;
    entry["impact"] = issue.impact;
    entry["retry_condition"] = issue.retry_condition;
    issues.push_back(entry);
  }
  node["issues"] = issues;
  node["condition"] = encode_condition(assessment.condition);
  node["execution"] = json(assessment.execution == ExecutionState::complete
                                ? "complete"
                                : (assessment.execution == ExecutionState::cancelled ? "cancelled"
                                                                                   : "failed"));
  node["capability"] = json(assessment.capability == Capability::supported
                                 ? "supported"
                                 : (assessment.capability == Capability::unsupported
                                        ? "unsupported"
                                        : "unavailable"));
  return node;
}

json encode_footprint_provider(const WcsModel& wcs) {
  json node;
  node["wcs"] = json{{"$ref", wcs.identity.object_id}};
  node["kind"] = "canonical_tan_sip";
  return node;
}

json encode_overlaps(const Overlaps& overlaps) {
  json contributions = json::array();
  for (const OverlapContribution& contribution : overlaps.contributions) {
    json entry;
    entry["source"] = json{{"dataset", contribution.source.dataset},
                            {"exposure", contribution.source.exposure},
                            {"detector", contribution.source.detector},
                            {"channel", contribution.source.channel},
                            {"sample", contribution.source.sample}};
    entry["leaf"] = json{{"order", contribution.leaf.order}, {"ipix", contribution.leaf.ipix}};
    entry["native_area_sr"] = json{{"lower", encode_f64(contribution.native_area.lower_sr)},
                                  {"upper", encode_f64(contribution.native_area.upper_sr)}};
    entry["drop_area_sr"] = json{{"lower", encode_f64(contribution.drop_area.lower_sr)},
                                 {"upper", encode_f64(contribution.drop_area.upper_sr)}};
    entry["overlap_area_sr"] = json{{"lower", encode_f64(contribution.overlap_area.lower_sr)},
                                   {"upper", encode_f64(contribution.overlap_area.upper_sr)}};
    contributions.push_back(entry);
  }
  json node;
  node["contributions"] = contributions;
  node["status"] = "resolved";
  return node;
}

json encode_wcs_model(const WcsModel& wcs) {
  json node;
  // The identity travels with the payload: the next request decodes the same
  // object with the same identity.
  node["identity"] = encode_identity(wcs.identity);
  node["crpix_fits1"] = json::array({encode_f64(wcs.crpix_fits1[0]), encode_f64(wcs.crpix_fits1[1])});
  node["cd_rad_per_pixel"] =
      json::array({encode_f64(wcs.cd_rad_per_pixel[0]), encode_f64(wcs.cd_rad_per_pixel[1]),
                   encode_f64(wcs.cd_rad_per_pixel[2]), encode_f64(wcs.cd_rad_per_pixel[3])});
  node["c"] = json::array({encode_f64(wcs.c[0]), encode_f64(wcs.c[1]), encode_f64(wcs.c[2])});
  node["e"] = json::array({encode_f64(wcs.e[0]), encode_f64(wcs.e[1]), encode_f64(wcs.e[2])});
  node["north"] =
      json::array({encode_f64(wcs.north[0]), encode_f64(wcs.north[1]), encode_f64(wcs.north[2])});
  node["domain"] = json{{"lower", json::array({encode_f64(wcs.domain.lower[0]),
                                               encode_f64(wcs.domain.lower[1])})},
                        {"upper", json::array({encode_f64(wcs.domain.upper[0]),
                                               encode_f64(wcs.domain.upper[1])})}};
  node["detector_frame"] = wcs.detector_frame;
  json header = json::array();
  for (const HeaderField& field : wcs.original_header) {
    json entry;
    entry["key"] = field.key;
    if (std::holds_alternative<double>(field.value)) {
      entry["value"] = encode_f64(std::get<double>(field.value));
    } else if (std::holds_alternative<std::string>(field.value)) {
      entry["value"] = std::get<std::string>(field.value);
    } else {
      entry["value"] = nullptr;
    }
    header.push_back(entry);
  }
  node["original_header"] = header;
  return node;
}

json encode_usage(const Usage& usage) {
  json node;
  node["work"] = usage.work;
  node["peak_payload_bytes"] = usage.peak_payload_bytes;
  node["read_bytes"] = usage.read_bytes;
  node["written_bytes"] = usage.written_bytes;
  node["spill_bytes"] = usage.spill_bytes;
  node["elapsed_seconds"] = usage.elapsed_seconds;
  return node;
}

json encode_index(const Index& index) {
  json node;
  node["size"] = index.size;
  const char* kind = "native";
  switch (index.kind) {
    case Index::Kind::native: kind = "native"; break;
    case Index::Kind::leaf: kind = "leaf"; break;
    case Index::Kind::parameter: kind = "parameter"; break;
    case Index::Kind::control: kind = "control"; break;
    case Index::Kind::pixel: kind = "pixel"; break;
    case Index::Kind::latent: kind = "latent"; break;
  }
  node["kind"] = json(kind);
  node["ids"] = nullptr;
  node["sources"] = json::array();
  node["order"] = index.order;
  node["partitions"] = json::array();
  for (const SourcePartition& partition : index.partitions) {
    json entry;
    entry["dataset"] = partition.dataset;
    entry["exposure"] = partition.exposure;
    entry["detector"] = partition.detector;
    entry["channel"] = partition.channel;
    entry["index_begin"] = partition.index_begin;
    entry["count"] = partition.count;
    entry["first_sample"] = partition.first_sample;
    entry["sample_stride"] = partition.sample_stride;
    node["partitions"].push_back(entry);
  }
  node["namespace_id"] = index.namespace_id;
  return node;
}

json encode_array(const Array& array, const std::vector<double>& values) {
  json node;
  node["dtype"] = json(array.dtype == DType::f64
                           ? "f64"
                           : (array.dtype == DType::f32
                                  ? "f32"
                                  : (array.dtype == DType::u64
                                         ? "u64"
                                         : (array.dtype == DType::u32
                                                ? "u32"
                                                : (array.dtype == DType::u16
                                                       ? "u16"
                                                       : (array.dtype == DType::u8 ? "u8" : "dyadic"))))));
  node["shape"] = array.shape;
  node["unit"] = encode_unit(array.unit);
  node["index_id"] = array.index_id;
  if (!values.empty()) node["values"] = values;
  return node;
}

json encode_variance(const Variance& variance) {
  json node;
  node["value"] = variance.value.has_value() ? json(*variance.value) : kNull;
  node["unit"] = encode_unit(variance.unit);
  node["state"] = json(noise_state_name(variance.state));
  if (variance.numeric_enclosure.has_value()) {
    json enclosure;
    enclosure["lower"] = variance.numeric_enclosure->lower;
    enclosure["upper"] = variance.numeric_enclosure->upper;
    node["numeric_enclosure"] = enclosure;
  } else {
    node["numeric_enclosure"] = kNull;
  }
  node["unknown_terms"] = variance.unknown_terms;
  return node;
}

json encode_support_map(const SupportMap& support) {
  json node;
  const char* kind = "primitives";
  switch (support.kind) {
    case SupportMap::Kind::primitives: kind = "primitives"; break;
    case SupportMap::Kind::dyadic_cells: kind = "dyadic_cells"; break;
    case SupportMap::Kind::full_leaf_approx: kind = "full_leaf_approx"; break;
    case SupportMap::Kind::effective_area_approx: kind = "effective_area_approx"; break;
  }
  node["kind"] = json(kind);
  node["primitives"] = support.primitives;
  node["geometry_snapshot"] = support.geometry_snapshot;
  return node;
}

// ---------------------------------------------------------------------------
// Decode context
// ---------------------------------------------------------------------------

DecodeContext::DecodeContext(Context& context, json input_objects, std::string input_root,
                             std::vector<BlockDescriptor> blocks)
    : context_(context),
      input_objects_(std::move(input_objects)),
      input_root_(std::move(input_root)),
      blocks_(std::move(blocks)) {}

void DecodeContext::publish(const std::string& type, const Id& hash, const json& data) {
  if (hash.empty()) return;
  published_[hash] = json{{"type", type}, {"data", data}};
}

Result<TypedObject> DecodeContext::resolve_reference(const std::string& name) {
  auto it = decoded_.find(name);
  if (it != decoded_.end()) return model::ok_result(it->second);
  if (!input_objects_.is_object() || !input_objects_.contains(name)) {
    return model::fail<TypedObject>(Code::missing_dependency,
                                    "the reference " + name + " is not declared",
                                    "contract.decode", "the request cannot be decoded");
  }
  if (std::find(decoding_.begin(), decoding_.end(), name) != decoding_.end()) {
    return model::fail<TypedObject>(Code::identity_conflict,
                                    "the reference " + name + " closes a cycle",
                                    "contract.decode", "the request cannot be decoded");
  }
  decoding_.push_back(name);
  Result<TypedObject> decoded = decode_node(input_objects_.at(name));
  decoding_.pop_back();
  if (!ok(decoded)) return decoded;
  decoded_[name] = val(decoded);
  return decoded;
}

// ---------------------------------------------------------------------------
// Typed object decoding
// ---------------------------------------------------------------------------

namespace {

DType dtype_from(const std::string& name) {
  if (name == "f64") return DType::f64;
  if (name == "f32") return DType::f32;
  if (name == "u64") return DType::u64;
  if (name == "u32") return DType::u32;
  if (name == "u16") return DType::u16;
  if (name == "u8") return DType::u8;
  return DType::dyadic;
}

std::string dtype_name(DType dtype) {
  switch (dtype) {
    case DType::f64: return "f64";
    case DType::f32: return "f32";
    case DType::u64: return "u64";
    case DType::u32: return "u32";
    case DType::u16: return "u16";
    case DType::u8: return "u8";
    case DType::dyadic: return "dyadic";
  }
  return "f64";
}


// Stores a list of numbers as a float64 array in the storage of the context.
Result<ArrayRef> store_f64_values(Context& context, const json& values, const std::string& name,
                                  const std::vector<U64>& shape = {}) {
  if (!values.is_array()) {
    return model::fail<ArrayRef>(Code::invalid_model, "the array is not a list", "contract.decode",
                                  "the array cannot be stored");
  }
  std::vector<double> data;
  data.reserve(values.size());
  for (const json& item : values) data.push_back(decode_f64(item));
  const std::vector<std::byte> bytes = model::encode_f64(data);
  const std::vector<U64> extent =
      shape.empty() ? std::vector<U64>{static_cast<U64>(data.size())} : shape;
  return context.storage.copy(context, DType::f64, bytes, extent, model::unit_dimensionless(), name);
}

Identity identity_for(const std::string& schema_id, const json& payload) {
  // A payload that carries its identity keeps it: the object of a response
  // and the object of the next request are the same object.
  const json& identity = field(payload, "identity");
  if (identity.is_object() && identity.contains("object_id")) {
    Identity result;
    result.object_id = string_field(identity, "object_id");
    result.content_hash = string_field(identity, "content_hash");
    result.run_id = string_field(identity, "run_id");
    result.revision = u64_field(identity, "revision", 0);
    if (!result.object_id.empty()) return result;
  }
  return model::make_identity(schema_id, 2, 0, model::sha256_hex(canonical_json(payload)),
                              "acsd-contract");
}

Index decode_index_object(const json& node) {
  Index index;
  index.size = u64_field(node, "size", 0);
  const std::string kind = string_field(node, "kind", "native");
  if (kind == "leaf") {
    index.kind = Index::Kind::leaf;
  } else if (kind == "parameter") {
    index.kind = Index::Kind::parameter;
  } else if (kind == "control") {
    index.kind = Index::Kind::control;
  } else if (kind == "pixel") {
    index.kind = Index::Kind::pixel;
  } else if (kind == "latent") {
    index.kind = Index::Kind::latent;
  } else {
    index.kind = Index::Kind::native;
  }
  index.order = static_cast<U32>(u64_field(node, "order", 0));
  index.namespace_id = string_field(node, "namespace_id", "namespace");
  const json& partitions = field(node, "partitions");
  if (partitions.is_array()) {
    for (const json& item : partitions) {
      SourcePartition partition;
      partition.dataset = string_field(item, "dataset");
      partition.exposure = string_field(item, "exposure");
      partition.detector = string_field(item, "detector");
      partition.channel = string_field(item, "channel");
      partition.index_begin = u64_field(item, "index_begin", 0);
      partition.count = u64_field(item, "count", 0);
      partition.first_sample = u64_field(item, "first_sample", 0);
      partition.sample_stride = u64_field(item, "sample_stride", 1);
      index.partitions.push_back(partition);
    }
  }
  const json& sources = field(node, "sources");
  if (sources.is_array()) {
    for (const json& item : sources) {
      SourceIdentity source;
      source.dataset = string_field(item, "dataset");
      source.exposure = string_field(item, "exposure");
      source.detector = string_field(item, "detector");
      source.channel = string_field(item, "channel");
      source.sample = u64_field(item, "sample", 0);
      index.sources.push_back(source);
    }
  }
  index.identity = identity_for("acsd.Index", node);
  index.assessment = model::make_assessment(Code::ok);
  index.assessment.complete = true;
  return index;
}

}  // namespace

Result<TypedObject> DecodeContext::decode_node(const json& node) {
  const std::string reference = reference_name(node);
  if (!reference.empty() && !(node.is_object() && node.contains("type"))) {
    return resolve_reference(reference);
  }
  const std::string type = type_of(node);
  if (type.empty()) {
    return model::fail<TypedObject>(Code::invalid_request, "the object has no type tag",
                                    "contract.decode", "the request cannot be decoded");
  }
  const json payload = payload_of(node);

  if (type == "Index") {
    Index index = decode_index_object(payload);
    publish("Index", index.identity.object_id, encode_index(index));
    return model::ok_result(TypedObject{"Index",
                                        std::make_shared<const Index>(index),
                                        index.identity.object_id});
  }

  if (type == "Array") {
    const std::string dtype_text = string_field(payload, "dtype", "f64");
    const DType dtype = dtype_from(dtype_text);
    std::vector<U64> shape;
    const json& shape_node = field(payload, "shape");
    if (shape_node.is_array()) {
      for (const json& item : shape_node) shape.push_back(item.get<U64>());
    }
    Unit unit = decode_unit(field(payload, "unit"));
    const std::string index_alias = string_field(payload, "index_id");
    Id index_id = index_alias;
    if (!index_alias.empty()) {
      Result<TypedObject> resolved = resolve_reference(index_alias);
      if (!ok(resolved)) return model::fail<TypedObject>(resolved.code, resolved.assessment.issues.empty()
                                                                ? "the array index is missing"
                                                                : resolved.assessment.issues.front().reason,
                                                          "contract.decode",
                                                          "the request cannot be decoded");
      const Index* index = static_cast<const Index*>(val(resolved).value.get());
      index_id = index->identity.object_id;
    }
    const json& values_node = field(payload, "values");
    const json& block_ids_node = field(payload, "block_ids");
    if (values_node.is_array()) {
      const std::size_t count = values_node.size();
      std::vector<std::byte> bytes;
      if (dtype == DType::f64) {
        std::vector<double> values;
        values.reserve(count);
        for (const json& item : values_node) {
          const double value = decode_f64(item);
          if (!std::isfinite(value)) {
            return model::fail<TypedObject>(Code::invalid_request,
                                            "the array carries a non finite value",
                                            "contract.decode", "the request cannot be decoded");
          }
          values.push_back(value);
        }
        bytes = model::encode_f64(values);
      } else if (dtype == DType::u64) {
        std::vector<U64> values;
        for (const json& item : values_node) values.push_back(item.get<U64>());
        bytes = model::encode_u64(values);
      } else if (dtype == DType::u32) {
        std::vector<U32> values;
        for (const json& item : values_node) values.push_back(item.get<U32>());
        bytes = model::encode_u32(values);
      } else if (dtype == DType::u16) {
        std::vector<std::uint16_t> values;
        for (const json& item : values_node) values.push_back(item.get<std::uint16_t>());
        bytes = model::encode_u16(values);
      } else if (dtype == DType::u8) {
        std::vector<std::uint8_t> values;
        for (const json& item : values_node) values.push_back(item.get<std::uint8_t>());
        bytes = model::encode_u8(values);
      } else {
        return model::fail<TypedObject>(Code::unsupported,
                                        "dyadic arrays are not supported by the driver",
                                        "contract.decode", "the request cannot be decoded");
      }
      Result<ArrayRef> array = context_.storage.copy(context_, dtype, bytes, shape, unit, index_id);
      if (!ok(array)) {
        return model::fail<TypedObject>(array.code, "the array cannot be stored: " +
                                                        (array.assessment.issues.empty()
                                                             ? std::string("unknown")
                                                             : array.assessment.issues.front().reason) +
                                                        " dtype=" + dtype_name(dtype) +
                                                        " elements=" + std::to_string(values_node.size()) +
                                                        " bytes=" + std::to_string(bytes.size()) +
                                                        " shape0=" + std::to_string(shape.empty() ? 0 : shape[0]),
                                        "contract.decode", "the request cannot be decoded");
      }
      publish("Array", val(array)->identity.object_id, encode_array(*val(array), {}));
      return model::ok_result(TypedObject{"Array", val(array), val(array)->identity.object_id});
    }
    if (block_ids_node.is_array()) {
      std::vector<BlockDescriptor> descriptors;
      for (const json& item : block_ids_node) {
        const std::string block_id = item.is_string() ? item.get<std::string>() : "";
        bool found = false;
        for (const BlockDescriptor& descriptor : blocks_) {
          if (descriptor.id == block_id) {
            descriptors.push_back(descriptor);
            found = true;
            break;
          }
        }
        if (!found) {
          return model::fail<TypedObject>(Code::missing_dependency,
                                          "the block " + block_id + " is not declared",
                                          "contract.decode", "the request cannot be decoded");
        }
      }
      Array description;
      description.dtype = dtype;
      description.shape = shape;
      description.unit = unit;
      description.index_id = index_id;
      Result<ArrayRef> array = context_.storage.open(context_, input_root_, description, descriptors);
      if (!ok(array)) {
        return model::fail<TypedObject>(array.code, array.assessment.issues.empty()
                                                          ? "the array cannot be opened"
                                                          : array.assessment.issues.front().reason,
                                        "contract.decode", "the request cannot be decoded");
      }
      publish("Array", val(array)->identity.object_id, encode_array(*val(array), {}));
      return model::ok_result(TypedObject{"Array", val(array), val(array)->identity.object_id});
    }
    return model::fail<TypedObject>(Code::invalid_request, "the array has neither values nor blocks",
                                    "contract.decode", "the request cannot be decoded");
  }

  if (type == "ErrorRecord") {
    ErrorRecord record;
    record.unknown_model_terms.clear();
    const json& condition = field(payload, "linearization_selection");
    record.linearization_selection.estimator = string_field(condition, "estimator");
    record.linearization_selection.kind = string_field(condition, "kind");
    const json& assumptions = field(condition, "assumptions");
    if (assumptions.is_array()) {
      for (const json& item : assumptions) {
        if (item.is_string()) record.linearization_selection.assumptions.push_back(item.get<std::string>());
      }
    }
    record.identity = identity_for("acsd.ErrorRecord", payload);
    record.assessment = model::make_assessment(Code::ok);
    record.assessment.complete = true;
    publish("ErrorRecord", record.identity.object_id, payload);
    return model::ok_result(TypedObject{"ErrorRecord",
                                        std::make_shared<const ErrorRecord>(record),
                                        record.identity.object_id});
  }

  if (type == "LinearAction") {
    const json& recipe_node = field(node, "recipe");
    ActionRecipe recipe;
    const std::string kind = string_field(recipe_node, "kind", "identity");
    if (kind == "identity") {
      recipe.kind = ActionRecipe::Kind::identity;
    } else if (kind == "diagonal") {
      recipe.kind = ActionRecipe::Kind::diagonal;
    } else if (kind == "csr") {
      recipe.kind = ActionRecipe::Kind::csr;
    } else if (kind == "compose") {
      recipe.kind = ActionRecipe::Kind::compose;
    } else if (kind == "sum") {
      recipe.kind = ActionRecipe::Kind::sum;
    } else if (kind == "transpose") {
      recipe.kind = ActionRecipe::Kind::transpose;
    } else if (kind == "factor_covariance") {
      recipe.kind = ActionRecipe::Kind::factor_covariance;
    } else if (kind == "solve") {
      recipe.kind = ActionRecipe::Kind::solve;
    } else if (kind == "sky") {
      recipe.kind = ActionRecipe::Kind::sky;
    } else if (kind == "selection") {
      recipe.kind = ActionRecipe::Kind::selection;
    } else if (kind == "leaf_merge") {
      recipe.kind = ActionRecipe::Kind::leaf_merge;
    } else {
      return model::fail<TypedObject>(Code::unsupported, "unknown action recipe " + kind,
                                      "contract.decode", "the request cannot be decoded");
    }
    recipe.algorithm = string_field(recipe_node, "algorithm", "unknown_v1");
    recipe.version = static_cast<U32>(u64_field(recipe_node, "version", 1));
    const json& spaces = field(recipe_node, "spaces");
    recipe.spaces.input = decode_unit(field(spaces, "input"));
    recipe.spaces.output = decode_unit(field(spaces, "output"));
    const std::string domain_name = reference_name(field(spaces, "domain"));
    if (!domain_name.empty()) {
      Result<TypedObject> domain = resolve_reference(domain_name);
      if (!ok(domain)) {
        return model::fail<TypedObject>(domain.code, "the action domain cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      recipe.spaces.domain = std::static_pointer_cast<const Index>(val(domain).value);
    }
    const std::string codomain_name = reference_name(field(spaces, "codomain"));
    if (!codomain_name.empty()) {
      Result<TypedObject> codomain = resolve_reference(codomain_name);
      if (!ok(codomain)) {
        return model::fail<TypedObject>(codomain.code, "the action codomain cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      recipe.spaces.codomain = std::static_pointer_cast<const Index>(val(codomain).value);
    }
    const std::string error_name = reference_name(field(recipe_node, "error_record"));
    if (!error_name.empty()) {
      Result<TypedObject> error = resolve_reference(error_name);
      if (!ok(error)) {
        return model::fail<TypedObject>(error.code, "the error record cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      recipe.error_record = std::static_pointer_cast<const ErrorRecord>(val(error).value);
    }

    std::vector<Ref<LinearAction>> children;
    const json& children_node = field(node, "children");
    if (children_node.is_array()) {
      for (const json& item : children_node) {
        const std::string name = reference_name(item);
        if (name.empty()) {
          return model::fail<TypedObject>(Code::invalid_request, "a child action is not a reference",
                                          "contract.decode", "the request cannot be decoded");
        }
        Result<TypedObject> child = resolve_reference(name);
        if (!ok(child)) {
          return model::fail<TypedObject>(child.code, "a child action cannot be resolved",
                                          "contract.decode", "the request cannot be decoded");
        }
        children.push_back(std::static_pointer_cast<const LinearAction>(val(child).value));
      }
    }
    std::vector<ArrayRef> arrays;
    const json& arrays_node = field(node, "arrays");
    if (arrays_node.is_array()) {
      for (const json& item : arrays_node) {
        const std::string name = reference_name(item);
        if (name.empty()) {
          return model::fail<TypedObject>(Code::invalid_request, "an array is not a reference",
                                          "contract.decode", "the request cannot be decoded");
        }
        Result<TypedObject> array = resolve_reference(name);
        if (!ok(array)) {
          return model::fail<TypedObject>(array.code, "an array cannot be resolved: " +
                                                          (array.assessment.issues.empty()
                                                               ? std::string("unknown")
                                                               : array.assessment.issues.front().reason),
                                          "contract.decode", "the request cannot be decoded");
        }
        arrays.push_back(std::static_pointer_cast<const Array>(val(array).value));
      }
    }
    // The recipe names the same dependencies; the names must match the given
    // order so that the persisted recipe stays reopenable.
    const json& recipe_children = field(recipe_node, "children");
    const json& recipe_arrays = field(recipe_node, "arrays");
    recipe.children.clear();
    if (recipe_children.is_array()) {
      for (const json& item : recipe_children) {
        if (item.is_string()) recipe.children.push_back(item.get<std::string>());
      }
    }
    recipe.arrays.clear();
    if (recipe_arrays.is_array()) {
      for (const json& item : recipe_arrays) {
        if (item.is_string()) recipe.arrays.push_back(item.get<std::string>());
      }
    }
    Result<Ref<LinearAction>> action = make_action(context_, recipe, children, arrays);
    if (!ok(action)) {
      return model::fail<TypedObject>(action.code, action.assessment.issues.empty()
                                                          ? "the action cannot be built"
                                                          : action.assessment.issues.front().reason,
                                      "contract.decode", "the request cannot be decoded");
    }
    publish("LinearAction", val(action)->identity.object_id, recipe_node);
    return model::ok_result(TypedObject{"LinearAction", val(action), val(action)->identity.object_id});
  }

  if (type == "NoiseRoot") {
    NoiseRoot root;
    const json& shared = field(payload, "shared_id");
    root.shared_id.external_namespace = string_field(shared, "external_namespace");
    root.shared_id.entity = string_field(shared, "entity");
    root.shared_id.model_revision = string_field(shared, "model_revision");
    root.shared_id.component = string_field(shared, "component");
    root.unit = decode_unit(field(payload, "unit"));
    const std::string coordinates = reference_name(field(payload, "coordinates"));
    if (!coordinates.empty()) {
      Result<TypedObject> index = resolve_reference(coordinates);
      if (!ok(index)) {
        return model::fail<TypedObject>(index.code, "the root coordinates cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      root.coordinates = std::static_pointer_cast<const Index>(val(index).value);
    }
    const std::string factor = reference_name(field(payload, "factor"));
    if (!factor.empty()) {
      Result<TypedObject> action = resolve_reference(factor);
      if (!ok(action)) {
        return model::fail<TypedObject>(action.code, "the root factor cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      root.factor = std::static_pointer_cast<const LinearAction>(val(action).value);
    }
    root.identity = identity_for("acsd.NoiseRoot", payload);
    root.assessment = model::make_assessment(Code::ok);
    root.assessment.complete = true;
    publish("NoiseRoot", root.identity.object_id, payload);
    return model::ok_result(TypedObject{"NoiseRoot",
                                        std::make_shared<const NoiseRoot>(root),
                                        root.identity.object_id});
  }

  if (type == "NoiseModel") {
    NoiseModel noise;
    noise.variance_unit = decode_unit(field(payload, "variance_unit"));
    const json& condition = field(payload, "condition");
    noise.condition.estimator = string_field(condition, "estimator");
    noise.condition.kind = string_field(condition, "kind");
    const json& assumptions = field(condition, "assumptions");
    if (assumptions.is_array()) {
      for (const json& item : assumptions) {
        if (item.is_string()) noise.condition.assumptions.push_back(item.get<std::string>());
      }
    }
    const json& state = field(payload, "state");
    noise.state = noise_state_from(state.is_string() ? state.get<std::string>() : "known");
    const json& innovations = field(payload, "innovations");
    if (innovations.is_array()) {
      for (const json& item : innovations) {
        const std::string name = reference_name(item);
        Result<TypedObject> root = resolve_reference(name);
        if (!ok(root)) {
          return model::fail<TypedObject>(root.code, "an innovation root cannot be resolved",
                                          "contract.decode", "the request cannot be decoded");
        }
        noise.innovations.push_back(std::static_pointer_cast<const NoiseRoot>(val(root).value));
      }
    }
    const json& shared_factors = field(payload, "shared_factors");
    if (shared_factors.is_array()) {
      for (const json& item : shared_factors) {
        const std::string name = reference_name(item);
        Result<TypedObject> root = resolve_reference(name);
        if (!ok(root)) {
          return model::fail<TypedObject>(root.code, "a shared root cannot be resolved",
                                          "contract.decode", "the request cannot be decoded");
        }
        noise.shared_factors.push_back(std::static_pointer_cast<const NoiseRoot>(val(root).value));
      }
    }
    const json& influence = field(payload, "influence");
    if (influence.is_array()) {
      for (const json& item : influence) {
        const std::string name = reference_name(item);
        Result<TypedObject> action = resolve_reference(name);
        if (!ok(action)) {
          return model::fail<TypedObject>(action.code, "an influence cannot be resolved",
                                          "contract.decode", "the request cannot be decoded");
        }
        noise.influence.push_back(std::static_pointer_cast<const LinearAction>(val(action).value));
      }
    }
    noise.identity = identity_for("acsd.NoiseModel", payload);
    noise.assessment = model::make_assessment(Code::ok);
    noise.assessment.complete = true;
    publish("NoiseModel", noise.identity.object_id, payload);
    return model::ok_result(TypedObject{"NoiseModel",
                                        std::make_shared<const NoiseModel>(noise),
                                        noise.identity.object_id});
  }

  if (type == "CovarianceAction") {
    CovarianceAction covariance;
    const std::string model_name = reference_name(field(payload, "model"));
    if (!model_name.empty()) {
      Result<TypedObject> noise = resolve_reference(model_name);
      if (!ok(noise)) {
        return model::fail<TypedObject>(noise.code, "the noise model cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      covariance.model = std::static_pointer_cast<const NoiseModel>(val(noise).value);
    }
    const std::string influence_name = reference_name(field(payload, "influence"));
    if (!influence_name.empty()) {
      Result<TypedObject> action = resolve_reference(influence_name);
      if (!ok(action)) {
        return model::fail<TypedObject>(action.code, "the influence cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      covariance.influence = std::static_pointer_cast<const LinearAction>(val(action).value);
    }
    const json& spaces = field(payload, "input_output");
    covariance.input_output.input = decode_unit(field(spaces, "input"));
    covariance.input_output.output = decode_unit(field(spaces, "output"));
    const std::string domain_name = reference_name(field(spaces, "domain"));
    if (!domain_name.empty()) {
      Result<TypedObject> index = resolve_reference(domain_name);
      if (!ok(index)) {
        return model::fail<TypedObject>(index.code, "the covariance domain cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      covariance.input_output.domain = std::static_pointer_cast<const Index>(val(index).value);
    }
    const std::string codomain_name = reference_name(field(spaces, "codomain"));
    if (!codomain_name.empty()) {
      Result<TypedObject> index = resolve_reference(codomain_name);
      if (!ok(index)) {
        return model::fail<TypedObject>(index.code, "the covariance codomain cannot be resolved",
                                        "contract.decode", "the request cannot be decoded");
      }
      covariance.input_output.codomain = std::static_pointer_cast<const Index>(val(index).value);
    }
    covariance.identity = identity_for("acsd.CovarianceAction", payload);
    covariance.assessment = model::make_assessment(Code::ok);
    covariance.assessment.complete = true;
    publish("CovarianceAction", covariance.identity.object_id, payload);
    return model::ok_result(TypedObject{"CovarianceAction",
                                        std::make_shared<const CovarianceAction>(covariance),
                                        covariance.identity.object_id});
  }

  if (type == "WcsModel") {
    WcsModel wcs;
    const json& crpix = field(payload, "crpix_fits1");
    if (crpix.is_array() && crpix.size() == 2) {
      wcs.crpix_fits1 = {decode_f64(crpix[0]), decode_f64(crpix[1])};
    }
    const json& cd = field(payload, "cd_rad_per_pixel");
    if (cd.is_array() && cd.size() == 4) {
      wcs.cd_rad_per_pixel = {decode_f64(cd[0]), decode_f64(cd[1]), decode_f64(cd[2]),
                               decode_f64(cd[3])};
    }
    const json& c = field(payload, "c");
    if (c.is_array() && c.size() == 3) {
      wcs.c = {decode_f64(c[0]), decode_f64(c[1]), decode_f64(c[2])};
    }
    const json& e = field(payload, "e");
    if (e.is_array() && e.size() == 3) {
      wcs.e = {decode_f64(e[0]), decode_f64(e[1]), decode_f64(e[2])};
    }
    const json& north = field(payload, "north");
    if (north.is_array() && north.size() == 3) {
      wcs.north = {decode_f64(north[0]), decode_f64(north[1]), decode_f64(north[2])};
    }
    const json& sip_a = field(payload, "sip_a");
    wcs.sip_a.order = static_cast<U32>(u64_field(sip_a, "order", 0));
    const json& sip_a_terms = field(sip_a, "terms");
    if (sip_a_terms.is_array()) {
      for (const json& item : sip_a_terms) {
        Term2 term;
        term.x_power = static_cast<U32>(u64_field(item, "x_power", 0));
        term.y_power = static_cast<U32>(u64_field(item, "y_power", 0));
        term.coefficient = decode_f64(field(item, "coefficient"));
        wcs.sip_a.terms.push_back(term);
      }
    }
    const json& sip_b = field(payload, "sip_b");
    wcs.sip_b.order = static_cast<U32>(u64_field(sip_b, "order", 0));
    const json& sip_b_terms = field(sip_b, "terms");
    if (sip_b_terms.is_array()) {
      for (const json& item : sip_b_terms) {
        Term2 term;
        term.x_power = static_cast<U32>(u64_field(item, "x_power", 0));
        term.y_power = static_cast<U32>(u64_field(item, "y_power", 0));
        term.coefficient = decode_f64(field(item, "coefficient"));
        wcs.sip_b.terms.push_back(term);
      }
    }
    wcs.domain = decode_box(field(payload, "domain"));
    const json& certificate = field(payload, "injectivity");
    const json& preconditioner = field(certificate, "preconditioner");
    if (preconditioner.is_array() && preconditioner.size() == 4) {
      wcs.injectivity.preconditioner = {decode_f64(preconditioner[0]), decode_f64(preconditioner[1]),
                                       decode_f64(preconditioner[2]), decode_f64(preconditioner[3])};
    }
    wcs.injectivity.eta = decode_interval(field(certificate, "eta"));
    wcs.injectivity.domain = decode_box(field(certificate, "domain"));
    wcs.injectivity.arithmetic = decode_arithmetic(field(certificate, "arithmetic"));
    wcs.injectivity.proof_hash = string_field(certificate, "proof_hash");
    wcs.normalization_error_rad = decode_exact_interval(field(payload, "normalization_error_rad"));
    wcs.epoch_frame_observer = decode_astrometric_context(field(payload, "epoch_frame_observer"));
    wcs.original_header = decode_header_fields(field(payload, "original_header"));
    wcs.detector_frame = string_field(payload, "detector_frame");
    wcs.identity = identity_for("acsd.WcsModel", payload);
    wcs.assessment = model::make_assessment(Code::ok);
    wcs.assessment.complete = true;
    publish("WcsModel", wcs.identity.object_id, payload);
    return model::ok_result(TypedObject{"WcsModel",
                                        std::make_shared<const WcsModel>(wcs),
                                        wcs.identity.object_id});
  }

  if (type == "FootprintProvider") {
    const json& wcs_node = field(payload, "wcs");
    Result<TypedObject> wcs_object = resolve_reference(
        wcs_node.is_object() && wcs_node.contains("$ref") ? wcs_node.at("$ref").get<std::string>()
                                                          : std::string());
    if (!ok(wcs_object)) {
      return model::fail<TypedObject>(wcs_object.code,
                                       "the provider has no world coordinate model",
                                       "contract.decode", "the provider cannot be decoded");
    }
    const Id hash = identity_for("acsd.FootprintProvider", payload).object_id;
    publish("FootprintProvider", hash, payload);
    return model::ok_result(TypedObject{"FootprintProvider", val(wcs_object).value, hash});
  }

  if (type == "SpectralBasis") {
    SpectralBasis basis;
    const json& knots = field(payload, "knots_nm");
    const json& coefficients = field(payload, "basis_coefficients");
    Result<ArrayRef> knot_array = store_f64_values(context_, knots, "acsd.basis.knots");
    const U64 coefficient_count = u64_field(payload, "coefficient_count", 0);
    const U64 row_count = coefficient_count > 0 ? static_cast<U64>(coefficients.size()) / coefficient_count
                                                 : static_cast<U64>(coefficients.size());
    Result<ArrayRef> coefficient_array =
        store_f64_values(context_, coefficients, "acsd.basis.coefficients",
                         std::vector<U64>{row_count, coefficient_count});
    if (!ok(knot_array) || !ok(coefficient_array)) {
      return model::fail<TypedObject>(Code::invalid_model, "the basis arrays cannot be stored",
                                       "contract.decode", "the basis cannot be decoded");
    }
    basis.knots_nm = val(knot_array);
    basis.basis_coefficients = val(coefficient_array);
    basis.degree = static_cast<U32>(u64_field(payload, "degree", 0));
    basis.coefficient_count = static_cast<U32>(u64_field(payload, "coefficient_count", 0));
    basis.support_nm = decode_interval(field(payload, "support_nm"));
    basis.identity = identity_for("acsd.SpectralBasis", payload);
    basis.assessment = model::make_assessment(Code::ok);
    basis.assessment.complete = true;
    publish("SpectralBasis", basis.identity.object_id, payload);
    return model::ok_result(TypedObject{"SpectralBasis",
                                        std::make_shared<const SpectralBasis>(basis),
                                        basis.identity.object_id});
  }

  if (type == "Response") {
    Response response;
    const std::string convention = string_field(payload, "convention", "photon_throughput");
    response.convention = convention == "includes_photon_conversion"
                              ? Response::Convention::includes_photon_conversion
                              : Response::Convention::photon_throughput;
    const json& wavelengths = field(payload, "wavelength_nm");
    const json& throughput = field(payload, "throughput");
    Result<ArrayRef> wavelength_array = store_f64_values(context_, wavelengths, "acsd.response.wavelength");
    Result<ArrayRef> throughput_array = store_f64_values(context_, throughput, "acsd.response.throughput");
    if (!ok(wavelength_array) || !ok(throughput_array)) {
      return model::fail<TypedObject>(Code::invalid_model, "the response arrays cannot be stored",
                                       "contract.decode", "the response cannot be decoded");
    }
    response.wavelength_nm = val(wavelength_array);
    response.throughput = val(throughput_array);
    response.support_nm = decode_interval(field(payload, "support_nm"));
    response.provenance = string_field(payload, "provenance");
    response.normalization = string_field(payload, "normalization");
    response.identity = identity_for("acsd.Response", payload);
    response.assessment = model::make_assessment(Code::ok);
    response.assessment.complete = true;
    publish("Response", response.identity.object_id, payload);
    return model::ok_result(TypedObject{"Response", std::make_shared<const Response>(response),
                                        response.identity.object_id});
  }

  if (type == "DetectorFootprint") {
    DetectorFootprint footprint;
    const json& wcs_node = field(payload, "wcs");
    Result<TypedObject> wcs_object = resolve_reference(
        wcs_node.is_object() && wcs_node.contains("$ref") ? wcs_node.at("$ref").get<std::string>()
                                                          : std::string());
    if (!ok(wcs_object)) {
      return model::fail<TypedObject>(wcs_object.code, "the footprint has no world coordinate model",
                                       "contract.decode", "the footprint cannot be decoded");
    }
    footprint.wcs = std::static_pointer_cast<const WcsModel>(val(wcs_object).value);
    const json& rings = field(payload, "detector_rings");
    if (rings.is_array()) {
      for (const json& item : rings) {
        Ring ring;
        ring.hole = item.contains("hole") && item.at("hole").is_boolean() &&
                    item.at("hole").get<bool>();
        const json& vertices = field(item, "vertices");
        if (vertices.is_array()) {
          for (const json& vertex : vertices) {
            PixelEdge0 edge;
            edge.x = decode_f64(field(vertex, "x"));
            edge.y = decode_f64(field(vertex, "y"));
            ring.vertices.push_back(edge);
          }
        }
        footprint.detector_rings.push_back(ring);
      }
    }
    const json& source = field(payload, "source");
    footprint.source.dataset = string_field(source, "dataset");
    footprint.source.exposure = string_field(source, "exposure");
    footprint.source.detector = string_field(source, "detector");
    footprint.source.channel = string_field(source, "channel");
    footprint.source.sample = u64_field(source, "sample", 0);
    const json& centre = field(payload, "contraction_center");
    footprint.contraction_center.x = decode_f64(field(centre, "x"));
    footprint.contraction_center.y = decode_f64(field(centre, "y"));
    footprint.identity = identity_for("acsd.DetectorFootprint", payload);
    footprint.assessment = model::make_assessment(Code::ok);
    footprint.assessment.complete = true;
    publish("DetectorFootprint", footprint.identity.object_id, payload);
    return model::ok_result(TypedObject{"DetectorFootprint",
                                        std::make_shared<const DetectorFootprint>(footprint),
                                        footprint.identity.object_id});
  }

  // Plain value types that may be used directly as an argument.
  if (type == "PixelCenter0") {
    return model::ok_result(TypedObject{"PixelCenter0", std::make_shared<const PixelCenter0>(
                                                            decode_pixel_center(payload))});
  }
  if (type == "PixelEdge0") {
    return model::ok_result(TypedObject{"PixelEdge0",
                                         std::make_shared<const PixelEdge0>(decode_pixel_edge(payload))});
  }
  if (type == "FitsPixel1") {
    return model::ok_result(TypedObject{"FitsPixel1",
                                         std::make_shared<const FitsPixel1>(decode_fits_pixel(payload))});
  }
  if (type == "Box") {
    return model::ok_result(TypedObject{"Box", std::make_shared<const Box>(decode_box(payload))});
  }
  if (type == "SkyICRS") {
    return model::ok_result(TypedObject{"SkyICRS", std::make_shared<const SkyICRS>(decode_sky(payload))});
  }
  if (type == "Unit") {
    return model::ok_result(TypedObject{"Unit", std::make_shared<const Unit>(decode_unit(payload))});
  }
  if (type == "Interval") {
    return model::ok_result(TypedObject{"Interval",
                                         std::make_shared<const Interval>(decode_interval(payload))});
  }
  if (type == "ExactInterval") {
    return model::ok_result(TypedObject{"ExactInterval",
                                         std::make_shared<const ExactInterval>(decode_exact_interval(payload))});
  }
  if (type == "HealpixLeaf") {
    return model::ok_result(TypedObject{"HealpixLeaf",
                                         std::make_shared<const HealpixLeaf>(decode_leaf(payload))});
  }
  if (type == "Tolerance") {
    return model::ok_result(TypedObject{"Tolerance",
                                         std::make_shared<const Tolerance>(decode_tolerance(payload))});
  }
  if (type == "Arithmetic") {
    return model::ok_result(TypedObject{"Arithmetic",
                                         std::make_shared<const Arithmetic>(decode_arithmetic(payload))});
  }
  if (type == "NumericBudget") {
    return model::ok_result(TypedObject{"NumericBudget",
                                         std::make_shared<const NumericBudget>(decode_numeric_budget(payload))});
  }
  if (type == "SolvePolicy") {
    return model::ok_result(TypedObject{"SolvePolicy",
                                         std::make_shared<const SolvePolicy>(decode_solve_policy(payload))});
  }
  if (type == "AstrometricContext") {
    return model::ok_result(TypedObject{"AstrometricContext",
                                         std::make_shared<const AstrometricContext>(
                                             decode_astrometric_context(payload))});
  }
  if (type == "BlockRequest") {
    return model::ok_result(TypedObject{"BlockRequest",
                                         std::make_shared<const BlockRequest>(decode_block_request(payload))});
  }
  if (type == "Limits") {
    return model::ok_result(TypedObject{"Limits", std::make_shared<const Limits>(decode_limits(payload))});
  }
  if (type == "Variance") {
    return model::ok_result(TypedObject{"Variance",
                                         std::make_shared<const Variance>(
                                             Variance{decode_f64(field(payload, "value")),
                                                      decode_unit(field(payload, "unit")),
                                                      noise_state_from(string_field(payload, "state", "unknown")),
                                                      std::nullopt,
                                                      {}})});
  }
  if (type == "HeaderFields" || type == "HeaderField") {
    return model::ok_result(TypedObject{"HeaderFields",
                                         std::make_shared<const HeaderFields>(decode_header_fields(payload))});
  }

  return model::fail<TypedObject>(Code::unsupported, "the type " + type + " is not supported",
                                  "contract.decode", "the request cannot be decoded");
}

}  // namespace acsd::contract
