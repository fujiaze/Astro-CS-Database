// Canonical JSON encoding of the product objects.
#include "workflows/product_io.hpp"

#include <cstring>
#include <iomanip>
#include <sstream>

#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::workflows {
namespace {

const json kNull = json(nullptr);

bool has(const json& node, const char* key) {
  return node.is_object() && node.contains(key) && !node.at(key).is_null();
}

const json& field(const json& node, const char* key) {
  static const json empty = json(nullptr);
  if (!node.is_object() || !node.contains(key)) return empty;
  return node.at(key);
}

std::string string_field(const json& node, const char* key, const std::string& fallback = {}) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_string() ? value.get<std::string>() : fallback;
}

U64 u64_field(const json& node, const char* key, U64 fallback = 0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (value.is_number_unsigned()) return value.get<U64>();
  if (value.is_number_integer()) {
    const std::int64_t signed_value = value.get<std::int64_t>();
    return signed_value < 0 ? fallback : static_cast<U64>(signed_value);
  }
  return fallback;
}

std::int32_t i32_field(const json& node, const char* key, std::int32_t fallback = 0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_number_integer() ? value.get<std::int32_t>() : fallback;
}

double double_field(const json& node, const char* key, double fallback = 0.0) {
  if (!has(node, key)) return fallback;
  return read_f64(node.at(key));
}

bool bool_field(const json& node, const char* key, bool fallback = false) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_boolean() ? value.get<bool>() : fallback;
}

std::vector<std::string> string_list(const json& node, const char* key) {
  std::vector<std::string> values;
  const json& list = field(node, key);
  if (list.is_array()) {
    for (const json& item : list) {
      if (item.is_string()) values.push_back(item.get<std::string>());
    }
  }
  return values;
}

std::vector<Id> id_list(const json& node, const char* key) {
  std::vector<Id> values;
  const json& list = field(node, key);
  if (list.is_array()) {
    for (const json& item : list) {
      if (item.is_string()) values.push_back(item.get<std::string>());
    }
  }
  return values;
}

const char* validity_name(Validity validity) {
  switch (validity) {
    case Validity::certified: return "certified";
    case Validity::approximate: return "approximate";
    case Validity::invalid: return "invalid";
  }
  return "invalid";
}

Validity validity_from(const std::string& name) {
  if (name == "certified") return Validity::certified;
  if (name == "approximate") return Validity::approximate;
  return Validity::invalid;
}

const char* science_name(ScienceState science) {
  switch (science) {
    case ScienceState::bounded: return "bounded";
    case ScienceState::unconverged: return "unconverged";
    case ScienceState::conditional: return "conditional";
    case ScienceState::selection_unresolved: return "selection_unresolved";
    case ScienceState::geometry_solve_unresolved: return "geometry_solve_unresolved";
  }
  return "unconverged";
}

ScienceState science_from(const std::string& name) {
  if (name == "bounded") return ScienceState::bounded;
  if (name == "conditional") return ScienceState::conditional;
  if (name == "selection_unresolved") return ScienceState::selection_unresolved;
  if (name == "geometry_solve_unresolved") return ScienceState::geometry_solve_unresolved;
  return ScienceState::unconverged;
}

const char* noise_name(NoiseState state) {
  switch (state) {
    case NoiseState::known: return "known";
    case NoiseState::conditional: return "conditional";
    case NoiseState::approximate: return "approximate";
    case NoiseState::unknown: return "unknown";
  }
  return "unknown";
}

NoiseState noise_from(const std::string& name) {
  if (name == "known") return NoiseState::known;
  if (name == "conditional") return NoiseState::conditional;
  if (name == "approximate") return NoiseState::approximate;
  return NoiseState::unknown;
}

const char* execution_name(ExecutionState state) {
  switch (state) {
    case ExecutionState::complete: return "complete";
    case ExecutionState::cancelled: return "cancelled";
    case ExecutionState::failed: return "failed";
  }
  return "failed";
}

ExecutionState execution_from(const std::string& name) {
  if (name == "complete") return ExecutionState::complete;
  if (name == "cancelled") return ExecutionState::cancelled;
  return ExecutionState::failed;
}

const char* capability_name(Capability capability) {
  switch (capability) {
    case Capability::supported: return "supported";
    case Capability::unsupported: return "unsupported";
    case Capability::unavailable: return "unavailable";
  }
  return "supported";
}

Capability capability_from(const std::string& name) {
  if (name == "unsupported") return Capability::unsupported;
  if (name == "unavailable") return Capability::unavailable;
  return Capability::supported;
}


// The stable lower case names of the error codes and of the quantity kinds.
const char* code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "internal";
}

Code code_from_name(const std::string& name) {
  static const Code kAll[] = {Code::ok,
                              Code::invalid_request,
                              Code::invalid_model,
                              Code::domain,
                              Code::unsupported,
                              Code::unavailable,
                              Code::unsolved,
                              Code::not_identifiable,
                              Code::invalid_covariance,
                              Code::duplicate_source,
                              Code::identity_conflict,
                              Code::hash_mismatch,
                              Code::corrupt_product,
                              Code::missing_dependency,
                              Code::no_support,
                              Code::support_unresolved,
                              Code::geometry_solve_unresolved,
                              Code::selection_unresolved,
                              Code::nonconverged,
                              Code::precision_exhausted,
                              Code::resource_exhausted,
                              Code::io_error,
                              Code::cancelled,
                              Code::internal};
  for (Code code : kAll) {
    if (name == code_name(code)) return code;
  }
  return Code::internal;
}

const char* quantity_kind_name(QuantityRequest::Kind kind) {
  switch (kind) {
    case QuantityRequest::Kind::brightness: return "brightness";
    case QuantityRequest::Kind::assigned_flux: return "assigned_flux";
    case QuantityRequest::Kind::supported_flux: return "supported_flux";
    case QuantityRequest::Kind::aperture: return "aperture";
    case QuantityRequest::Kind::covariance: return "covariance";
    case QuantityRequest::Kind::area: return "area";
  }
  return "brightness";
}

}  // namespace

Id decode_ref(const json& node) {
  if (node.is_object() && node.contains("$ref")) {
    const json& value = node.at("$ref");
    if (value.is_string()) return value.get<std::string>();
  }
  return Id();
}

namespace {

json encode_ref(const Id& hash) {
  if (hash.empty()) return kNull;
  return json{{"$ref", hash}};
}

Id decode_ref(const json& node) {
  if (node.is_object() && node.contains("$ref")) {
    const json& value = node.at("$ref");
    if (value.is_string()) return value.get<std::string>();
  }
  return Id();
}

}  // namespace

std::string canonical_text(const json& node) { return node.dump(); }

Id canonical_id(const json& node) {
  return model::content_id(model::sha256_hex(canonical_text(node)));
}

json f64_value(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (int i = 0; i < 8; ++i) {
    stream << std::setw(2) << ((bits >> (8 * i)) & 0xffull);
  }
  return json{{"f64", stream.str()}};
}

double read_f64(const json& node) {
  if (node.is_number()) return node.get<double>();
  if (node.is_object() && node.contains("f64")) {
    const std::string hex = node.at("f64").get<std::string>();
    if (hex.size() != 16) return 0.0;
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
      bits |= static_cast<std::uint64_t>(std::stoul(hex.substr(static_cast<std::size_t>(2 * i), 2),
                                                   nullptr, 16))
              << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
  }
  return 0.0;
}

json encode_unit(const Unit& unit) {
  json node;
  node["scale"] = unit.scale;
  node["sr_power"] = unit.sr_power;
  node["pixel_power"] = unit.pixel_power;
  node["nm_power"] = unit.nm_power;
  node["multiplier"] = f64_value(unit.multiplier);
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

json encode_condition(const Condition& condition) {
  json node;
  node["estimator"] = condition.estimator;
  node["kind"] = condition.kind;
  node["fixed_objects"] = condition.fixed_objects;
  node["assumptions"] = condition.assumptions;
  return node;
}

Condition decode_condition(const json& node) {
  Condition condition;
  condition.estimator = string_field(node, "estimator");
  condition.kind = string_field(node, "kind");
  condition.fixed_objects = id_list(node, "fixed_objects");
  condition.assumptions = string_list(node, "assumptions");
  return condition;
}

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

Identity decode_identity(const json& node) {
  Identity identity;
  identity.object_id = string_field(node, "object_id");
  identity.content_hash = string_field(node, "content_hash");
  identity.run_id = string_field(node, "run_id");
  identity.revision = u64_field(node, "revision", 0);
  identity.schema_id = string_field(node, "schema_id");
  identity.schema_major = static_cast<U32>(u64_field(node, "schema_major", 2));
  identity.schema_minor = static_cast<U32>(u64_field(node, "schema_minor", 0));
  return identity;
}

json encode_assessment(const Assessment& assessment) {
  json node;
  node["complete"] = assessment.complete;
  node["validity"] = json(validity_name(assessment.validity));
  node["science"] = json(science_name(assessment.science));
  node["noise"] = json(noise_name(assessment.noise));
  node["included"] = assessment.included;
  node["omitted"] = assessment.omitted;
  json issues = json::array();
  for (const Issue& issue : assessment.issues) {
    json entry;
    entry["code"] = json(code_name(issue.code));
    entry["origin"] = issue.origin;
    entry["reason"] = issue.reason;
    entry["impact"] = issue.impact;
    entry["retry_condition"] = issue.retry_condition;
    entry["scope_frame"] = issue.scope.frame;
    entry["scope_functional"] = issue.scope.functional;
    issues.push_back(entry);
  }
  node["issues"] = issues;
  node["condition"] = encode_condition(assessment.condition);
  node["execution"] = json(execution_name(assessment.execution));
  node["capability"] = json(capability_name(assessment.capability));
  return node;
}

Assessment decode_assessment(const json& node) {
  Assessment assessment;
  assessment.complete = bool_field(node, "complete", false);
  assessment.validity = validity_from(string_field(node, "validity", "invalid"));
  assessment.science = science_from(string_field(node, "science", "unconverged"));
  assessment.noise = noise_from(string_field(node, "noise", "unknown"));
  assessment.included = string_list(node, "included");
  assessment.omitted = string_list(node, "omitted");
  assessment.condition = decode_condition(field(node, "condition"));
  assessment.execution = execution_from(string_field(node, "execution", "failed"));
  assessment.capability = capability_from(string_field(node, "capability", "supported"));
  const json& issues = field(node, "issues");
  if (issues.is_array()) {
    for (const json& item : issues) {
      Issue issue;
      issue.code = code_from_name(string_field(item, "code", "internal"));
      issue.origin = string_field(item, "origin");
      issue.reason = string_field(item, "reason");
      issue.impact = string_field(item, "impact");
      issue.retry_condition = string_field(item, "retry_condition");
      issue.scope.frame = string_field(item, "scope_frame");
      issue.scope.functional = string_field(item, "scope_functional");
      assessment.issues.push_back(issue);
    }
  }
  return assessment;
}

json encode_estimator(const Estimator& estimator) {
  json node;
  node["id"] = estimator.id;
  node["quantity"] = estimator.quantity;
  node["unit"] = encode_unit(estimator.unit);
  node["condition"] = encode_condition(estimator.condition);
  node["reference_sed"] = estimator.reference_sed.has_value() ? json(*estimator.reference_sed) : kNull;
  return node;
}

Estimator decode_estimator(const json& node) {
  Estimator estimator;
  estimator.id = string_field(node, "id");
  estimator.quantity = string_field(node, "quantity");
  estimator.unit = decode_unit(field(node, "unit"));
  estimator.condition = decode_condition(field(node, "condition"));
  if (has(node, "reference_sed")) estimator.reference_sed = string_field(node, "reference_sed");
  return estimator;
}

json encode_error_record(const ErrorRecord& record) {
  json node;
  node["identity"] = encode_identity(record.identity);
  node["assessment"] = encode_assessment(record.assessment);
  node["unknown_model_terms"] = record.unknown_model_terms;
  node["linearization_selection"] = encode_condition(record.linearization_selection);
  node["requested"] = json::array();
  for (const QuantityRequest& request : record.requested) {
    json entry;
    entry["id"] = request.id;
    entry["kind"] = json(quantity_kind_name(request.kind));
    entry["tolerance_absolute"] = f64_value(request.tolerance.absolute);
    entry["tolerance_relative"] = f64_value(request.tolerance.relative);
    entry["tolerance_floor"] = f64_value(request.tolerance.absolute_floor);
    entry["unit"] = encode_unit(request.tolerance.unit);
    entry["condition"] = encode_condition(request.condition);
    node["requested"].push_back(entry);
  }
  json achieved = json::array();
  for (const ExactInterval& interval : record.achieved) {
    json entry;
    entry["lower"] = interval.lower.signed_integer;
    entry["lower_exponent2"] = interval.lower.exponent2;
    entry["upper"] = interval.upper.signed_integer;
    entry["upper_exponent2"] = interval.upper.exponent2;
    entry["precision_bits"] = interval.precision_bits;
    achieved.push_back(entry);
  }
  node["achieved"] = achieved;
  json bias = json::array();
  for (const ExactInterval& interval : record.finite_model_bias) {
    json entry;
    entry["lower"] = interval.lower.signed_integer;
    entry["lower_exponent2"] = interval.lower.exponent2;
    entry["upper"] = interval.upper.signed_integer;
    entry["upper_exponent2"] = interval.upper.exponent2;
    entry["precision_bits"] = interval.precision_bits;
    bias.push_back(entry);
  }
  node["finite_model_bias"] = bias;
  node["stochastic_noise"] = encode_ref(record.stochastic_noise ? record.stochastic_noise->identity.object_id : Id());
  return node;
}

ErrorRecord decode_error_record(const json& node) {
  ErrorRecord record;
  record.identity = decode_identity(field(node, "identity"));
  record.assessment = decode_assessment(field(node, "assessment"));
  record.unknown_model_terms = string_list(node, "unknown_model_terms");
  record.linearization_selection = decode_condition(field(node, "linearization_selection"));
  const json& achieved = field(node, "achieved");
  if (achieved.is_array()) {
    for (const json& item : achieved) {
      ExactInterval interval;
      interval.lower.signed_integer = string_field(item, "lower", "0");
      interval.lower.exponent2 = i32_field(item, "lower_exponent2", 0);
      interval.upper.signed_integer = string_field(item, "upper", "0");
      interval.upper.exponent2 = i32_field(item, "upper_exponent2", 0);
      interval.precision_bits = static_cast<U32>(u64_field(item, "precision_bits", 64));
      record.achieved.push_back(interval);
    }
  }
  const json& bias = field(node, "finite_model_bias");
  if (bias.is_array()) {
    for (const json& item : bias) {
      ExactInterval interval;
      interval.lower.signed_integer = string_field(item, "lower", "0");
      interval.lower.exponent2 = i32_field(item, "lower_exponent2", 0);
      interval.upper.signed_integer = string_field(item, "upper", "0");
      interval.upper.exponent2 = i32_field(item, "upper_exponent2", 0);
      interval.precision_bits = static_cast<U32>(u64_field(item, "precision_bits", 64));
      record.finite_model_bias.push_back(interval);
    }
  }
  return record;
}

json encode_index(const Index& index) {
  json node;
  node["identity"] = encode_identity(index.identity);
  node["assessment"] = encode_assessment(index.assessment);
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
  node["order"] = index.order;
  node["namespace_id"] = index.namespace_id;
  node["ids"] = encode_ref(index.ids ? index.ids->identity.object_id : Id());
  json partitions = json::array();
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
    partitions.push_back(entry);
  }
  node["partitions"] = partitions;
  json sources = json::array();
  for (const SourceIdentity& source : index.sources) {
    json entry;
    entry["dataset"] = source.dataset;
    entry["exposure"] = source.exposure;
    entry["detector"] = source.detector;
    entry["channel"] = source.channel;
    entry["sample"] = source.sample;
    sources.push_back(entry);
  }
  node["sources"] = sources;
  return node;
}

Index decode_index(const json& node) {
  Index index;
  index.identity = decode_identity(field(node, "identity"));
  index.assessment = decode_assessment(field(node, "assessment"));
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
  index.namespace_id = string_field(node, "namespace_id");
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
  return index;
}

json encode_array_descriptor(const Array& array) {
  json node;
  node["identity"] = encode_identity(array.identity);
  node["assessment"] = encode_assessment(array.assessment);
  const char* dtype = "f64";
  switch (array.dtype) {
    case DType::f64: dtype = "f64"; break;
    case DType::f32: dtype = "f32"; break;
    case DType::u64: dtype = "u64"; break;
    case DType::u32: dtype = "u32"; break;
    case DType::u16: dtype = "u16"; break;
    case DType::u8: dtype = "u8"; break;
    case DType::dyadic: dtype = "dyadic"; break;
  }
  node["dtype"] = json(dtype);
  node["shape"] = array.shape;
  node["unit"] = encode_unit(array.unit);
  node["index_id"] = array.index_id;
  node["block_ids"] = array.block_ids;
  return node;
}

Array decode_array_descriptor(const json& node) {
  Array array;
  array.identity = decode_identity(field(node, "identity"));
  array.assessment = decode_assessment(field(node, "assessment"));
  const std::string dtype = string_field(node, "dtype", "f64");
  if (dtype == "f32") {
    array.dtype = DType::f32;
  } else if (dtype == "u64") {
    array.dtype = DType::u64;
  } else if (dtype == "u32") {
    array.dtype = DType::u32;
  } else if (dtype == "u16") {
    array.dtype = DType::u16;
  } else if (dtype == "u8") {
    array.dtype = DType::u8;
  } else if (dtype == "dyadic") {
    array.dtype = DType::dyadic;
  } else {
    array.dtype = DType::f64;
  }
  const json& shape = field(node, "shape");
  if (shape.is_array()) {
    for (const json& item : shape) array.shape.push_back(item.get<U64>());
  }
  array.unit = decode_unit(field(node, "unit"));
  array.index_id = string_field(node, "index_id");
  array.block_ids = id_list(node, "block_ids");
  return array;
}

json encode_support_map(const SupportMap& support) {
  json node;
  node["identity"] = encode_identity(support.identity);
  node["assessment"] = encode_assessment(support.assessment);
  const char* kind = "primitives";
  switch (support.kind) {
    case SupportMap::Kind::primitives: kind = "primitives"; break;
    case SupportMap::Kind::dyadic_cells: kind = "dyadic_cells"; break;
    case SupportMap::Kind::full_leaf_approx: kind = "full_leaf_approx"; break;
    case SupportMap::Kind::effective_area_approx: kind = "effective_area_approx"; break;
  }
  node["kind"] = json(kind);
  node["index"] = encode_ref(support.index ? support.index->identity.object_id : Id());
  node["primitives"] = support.primitives;
  node["states"] = encode_ref(support.states ? support.states->identity.object_id : Id());
  node["area_lower_sr"] = encode_ref(support.area_lower_sr ? support.area_lower_sr->identity.object_id : Id());
  node["area_upper_sr"] = encode_ref(support.area_upper_sr ? support.area_upper_sr->identity.object_id : Id());
  node["geometry_snapshot"] = support.geometry_snapshot;
  node["cell_face"] = encode_ref(support.cell_face ? support.cell_face->identity.object_id : Id());
  node["cell_depth"] = encode_ref(support.cell_depth ? support.cell_depth->identity.object_id : Id());
  node["cell_x"] = encode_ref(support.cell_x ? support.cell_x->identity.object_id : Id());
  node["cell_y"] = encode_ref(support.cell_y ? support.cell_y->identity.object_id : Id());
  node["cell_state"] = encode_ref(support.cell_state ? support.cell_state->identity.object_id : Id());
  node["normalization_states"] =
      encode_ref(support.normalization_states ? support.normalization_states->identity.object_id : Id());
  node["observational_states"] =
      encode_ref(support.observational_states ? support.observational_states->identity.object_id : Id());
  return node;
}

SupportMap decode_support_map(const json& node) {
  SupportMap support;
  support.identity = decode_identity(field(node, "identity"));
  support.assessment = decode_assessment(field(node, "assessment"));
  const std::string kind = string_field(node, "kind", "primitives");
  if (kind == "dyadic_cells") {
    support.kind = SupportMap::Kind::dyadic_cells;
  } else if (kind == "full_leaf_approx") {
    support.kind = SupportMap::Kind::full_leaf_approx;
  } else if (kind == "effective_area_approx") {
    support.kind = SupportMap::Kind::effective_area_approx;
  } else {
    support.kind = SupportMap::Kind::primitives;
  }
  support.primitives = id_list(node, "primitives");
  support.geometry_snapshot = string_field(node, "geometry_snapshot");
  return support;
}

json encode_signal_estimate(const SignalEstimate& estimate) {
  json node;
  node["identity"] = encode_identity(estimate.identity);
  node["assessment"] = encode_assessment(estimate.assessment);
  node["signal"] = encode_ref(estimate.signal ? estimate.signal->identity.object_id : Id());
  node["unit"] = encode_unit(estimate.unit);
  node["estimator"] = encode_estimator(estimate.estimator);
  node["numerical_lower"] =
      encode_ref(estimate.numerical_lower ? estimate.numerical_lower->identity.object_id : Id());
  node["numerical_upper"] =
      encode_ref(estimate.numerical_upper ? estimate.numerical_upper->identity.object_id : Id());
  node["support"] = encode_ref(estimate.support ? estimate.support->identity.object_id : Id());
  node["effective_response"] =
      encode_ref(estimate.effective_response ? estimate.effective_response->identity.object_id : Id());
  node["noise"] = encode_ref(estimate.noise ? estimate.noise->identity.object_id : Id());
  node["error"] = encode_ref(estimate.error ? estimate.error->identity.object_id : Id());
  return node;
}

SignalEstimate decode_signal_estimate(const json& node) {
  SignalEstimate estimate;
  estimate.identity = decode_identity(field(node, "identity"));
  estimate.assessment = decode_assessment(field(node, "assessment"));
  estimate.unit = decode_unit(field(node, "unit"));
  estimate.estimator = decode_estimator(field(node, "estimator"));
  return estimate;
}

json encode_noise_root(const NoiseRoot& root) {
  json node;
  node["identity"] = encode_identity(root.identity);
  node["assessment"] = encode_assessment(root.assessment);
  json shared;
  shared["external_namespace"] = root.shared_id.external_namespace;
  shared["entity"] = root.shared_id.entity;
  shared["model_revision"] = root.shared_id.model_revision;
  shared["component"] = root.shared_id.component;
  node["shared_id"] = shared;
  node["coordinates"] = encode_ref(root.coordinates ? root.coordinates->identity.object_id : Id());
  node["unit"] = encode_unit(root.unit);
  node["factor"] = encode_ref(root.factor ? root.factor->identity.object_id : Id());
  return node;
}

NoiseRoot decode_noise_root(const json& node) {
  NoiseRoot root;
  root.identity = decode_identity(field(node, "identity"));
  root.assessment = decode_assessment(field(node, "assessment"));
  const json& shared = field(node, "shared_id");
  root.shared_id.external_namespace = string_field(shared, "external_namespace");
  root.shared_id.entity = string_field(shared, "entity");
  root.shared_id.model_revision = string_field(shared, "model_revision");
  root.shared_id.component = string_field(shared, "component");
  root.unit = decode_unit(field(node, "unit"));
  return root;
}

json encode_action_recipe(const ActionRecipe& recipe) {
  json node;
  const char* kind = "identity";
  switch (recipe.kind) {
    case ActionRecipe::Kind::identity: kind = "identity"; break;
    case ActionRecipe::Kind::diagonal: kind = "diagonal"; break;
    case ActionRecipe::Kind::csr: kind = "csr"; break;
    case ActionRecipe::Kind::compose: kind = "compose"; break;
    case ActionRecipe::Kind::sum: kind = "sum"; break;
    case ActionRecipe::Kind::transpose: kind = "transpose"; break;
    case ActionRecipe::Kind::factor_covariance: kind = "factor_covariance"; break;
    case ActionRecipe::Kind::solve: kind = "solve"; break;
    case ActionRecipe::Kind::sky: kind = "sky"; break;
    case ActionRecipe::Kind::selection: kind = "selection"; break;
    case ActionRecipe::Kind::leaf_merge: kind = "leaf_merge"; break;
  }
  node["kind"] = json(kind);
  node["children"] = recipe.children;
  node["arrays"] = recipe.arrays;
  node["algorithm"] = recipe.algorithm;
  node["version"] = recipe.version;
  json spaces;
  spaces["domain"] = encode_ref(recipe.spaces.domain ? recipe.spaces.domain->identity.object_id : Id());
  spaces["codomain"] =
      encode_ref(recipe.spaces.codomain ? recipe.spaces.codomain->identity.object_id : Id());
  spaces["input"] = encode_unit(recipe.spaces.input);
  spaces["output"] = encode_unit(recipe.spaces.output);
  node["spaces"] = spaces;
  node["error_record"] = encode_ref(recipe.error_record ? recipe.error_record->identity.object_id : Id());
  node["solve"] = recipe.solve ? json("present") : kNull;
  return node;
}

ActionRecipe decode_action_recipe(const json& node) {
  ActionRecipe recipe;
  const std::string kind = string_field(node, "kind", "identity");
  if (kind == "diagonal") {
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
    recipe.kind = ActionRecipe::Kind::identity;
  }
  recipe.children = id_list(node, "children");
  recipe.arrays = id_list(node, "arrays");
  recipe.algorithm = string_field(node, "algorithm");
  recipe.version = static_cast<U32>(u64_field(node, "version", 1));
  const json& spaces = field(node, "spaces");
  recipe.spaces.input = decode_unit(field(spaces, "input"));
  recipe.spaces.output = decode_unit(field(spaces, "output"));
  return recipe;
}

json encode_linear_action(const LinearAction& action, const std::vector<Id>& children,
                          const std::vector<Id>& arrays) {
  json node;
  node["identity"] = encode_identity(action.identity);
  node["assessment"] = encode_assessment(action.assessment);
  Result<ActionRecipe> recipe = action.serialize_recipe();
  if (ok(recipe)) {
    node["recipe"] = encode_action_recipe(val(recipe));
  } else {
    node["recipe"] = kNull;
  }
  node["children"] = children;
  node["arrays"] = arrays;
  return node;
}

json encode_noise_model(const NoiseModel& model) {
  json node;
  node["identity"] = encode_identity(model.identity);
  node["assessment"] = encode_assessment(model.assessment);
  node["variance_unit"] = encode_unit(model.variance_unit);
  node["condition"] = encode_condition(model.condition);
  json innovations = json::array();
  for (const Ref<NoiseRoot>& root : model.innovations) {
    innovations.push_back(encode_ref(root ? root->identity.object_id : Id()));
  }
  node["innovations"] = innovations;
  json shared = json::array();
  for (const Ref<NoiseRoot>& root : model.shared_factors) {
    shared.push_back(encode_ref(root ? root->identity.object_id : Id()));
  }
  node["shared_factors"] = shared;
  json influence = json::array();
  for (const Ref<LinearAction>& action : model.influence) {
    influence.push_back(encode_ref(action ? action->identity.object_id : Id()));
  }
  node["influence"] = influence;
  node["unknown_terms"] = json::array();
  node["state"] = json(noise_name(model.state));
  return node;
}

NoiseModel decode_noise_model(const json& node) {
  NoiseModel model;
  model.identity = decode_identity(field(node, "identity"));
  model.assessment = decode_assessment(field(node, "assessment"));
  model.variance_unit = decode_unit(field(node, "variance_unit"));
  model.condition = decode_condition(field(node, "condition"));
  model.state = noise_from(string_field(node, "state", "unknown"));
  return model;
}

json encode_covariance_action(const CovarianceAction& covariance) {
  json node;
  node["identity"] = encode_identity(covariance.identity);
  node["assessment"] = encode_assessment(covariance.assessment);
  node["model"] = encode_ref(covariance.model ? covariance.model->identity.object_id : Id());
  node["influence"] = encode_ref(covariance.influence ? covariance.influence->identity.object_id : Id());
  json spaces;
  spaces["domain"] =
      encode_ref(covariance.input_output.domain ? covariance.input_output.domain->identity.object_id : Id());
  spaces["codomain"] = encode_ref(covariance.input_output.codomain
                                      ? covariance.input_output.codomain->identity.object_id
                                      : Id());
  spaces["input"] = encode_unit(covariance.input_output.input);
  spaces["output"] = encode_unit(covariance.input_output.output);
  node["input_output"] = spaces;
  node["absolute_factor_bound"] =
      encode_ref(covariance.absolute_factor_bound.has_value()
                     ? covariance.absolute_factor_bound.value()->identity.object_id
                     : Id());
  node["operator_norm_upper"] = covariance.operator_norm_upper.has_value()
                                     ? f64_value(*covariance.operator_norm_upper)
                                     : kNull;
  node["omitted_action_norm"] = covariance.omitted_action_norm.has_value()
                                    ? f64_value(*covariance.omitted_action_norm)
                                    : kNull;
  return node;
}

CovarianceAction decode_covariance_action(const json& node) {
  CovarianceAction covariance;
  covariance.identity = decode_identity(field(node, "identity"));
  covariance.assessment = decode_assessment(field(node, "assessment"));
  const json& spaces = field(node, "input_output");
  covariance.input_output.input = decode_unit(field(spaces, "input"));
  covariance.input_output.output = decode_unit(field(spaces, "output"));
  if (has(node, "operator_norm_upper")) {
    covariance.operator_norm_upper = read_f64(node.at("operator_norm_upper"));
  }
  if (has(node, "omitted_action_norm")) {
    covariance.omitted_action_norm = read_f64(node.at("omitted_action_norm"));
  }
  return covariance;
}

json encode_spatial_response(const SpatialResponse& response) {
  json node;
  node["identity"] = encode_identity(response.identity);
  node["assessment"] = encode_assessment(response.assessment);
  json actions = json::array();
  for (const Ref<LinearAction>& action : response.spatial_actions) {
    actions.push_back(encode_ref(action ? action->identity.object_id : Id()));
  }
  node["spatial_actions"] = actions;
  json responses = json::array();
  for (const Ref<Response>& item : response.responses) {
    responses.push_back(encode_ref(item ? item->identity.object_id : Id()));
  }
  node["responses"] = responses;
  node["weights"] = encode_ref(response.weights ? response.weights->identity.object_id : Id());
  node["reference_factors"] =
      encode_ref(response.reference_factors ? response.reference_factors->identity.object_id : Id());
  node["signed_response"] = response.signed_response;
  node["condition"] = encode_condition(response.condition);
  return node;
}

SpatialResponse decode_spatial_response(const json& node) {
  SpatialResponse response;
  response.identity = decode_identity(field(node, "identity"));
  response.assessment = decode_assessment(field(node, "assessment"));
  response.signed_response = bool_field(node, "signed_response", false);
  response.condition = decode_condition(field(node, "condition"));
  return response;
}

json encode_product_graph(const ProductGraph& graph) {
  json node;
  node["identity"] = encode_identity(graph.identity);
  node["assessment"] = encode_assessment(graph.assessment);
  const char* stage = "normalize";
  switch (graph.stage) {
    case ProductGraph::Stage::normalize: stage = "normalize"; break;
    case ProductGraph::Stage::mosaic: stage = "mosaic"; break;
    case ProductGraph::Stage::export_image: stage = "export_image"; break;
  }
  node["stage"] = json(stage);
  node["signal"] = encode_ref(graph.signal ? graph.signal->identity.object_id : Id());
  json closure = json::array();
  for (const Ref<Model>& model : graph.closure) {
    closure.push_back(encode_ref(model ? model->identity.object_id : Id()));
  }
  node["closure"] = closure;
  node["inputs"] = graph.inputs;
  node["profile"] = graph.profile;
  return node;
}

ProductGraph decode_product_graph(const json& node) {
  ProductGraph graph;
  graph.identity = decode_identity(field(node, "identity"));
  graph.assessment = decode_assessment(field(node, "assessment"));
  const std::string stage = string_field(node, "stage", "normalize");
  if (stage == "mosaic") {
    graph.stage = ProductGraph::Stage::mosaic;
  } else if (stage == "export_image") {
    graph.stage = ProductGraph::Stage::export_image;
  } else {
    graph.stage = ProductGraph::Stage::normalize;
  }
  graph.inputs = id_list(node, "inputs");
  graph.profile = string_field(node, "profile");
  return graph;
}

json encode_block_descriptor(const BlockDescriptor& descriptor) {
  json node;
  node["id"] = descriptor.id;
  node["path"] = descriptor.path;
  const char* dtype = "f64";
  switch (descriptor.dtype) {
    case DType::f64: dtype = "f64"; break;
    case DType::f32: dtype = "f32"; break;
    case DType::u64: dtype = "u64"; break;
    case DType::u32: dtype = "u32"; break;
    case DType::u16: dtype = "u16"; break;
    case DType::u8: dtype = "u8"; break;
    case DType::dyadic: dtype = "dyadic"; break;
  }
  node["dtype"] = json(dtype);
  node["shape"] = descriptor.shape;
  node["index_id"] = descriptor.index_id;
  node["unit"] = encode_unit(descriptor.unit);
  node["begin"] = descriptor.begin;
  node["count"] = descriptor.count;
  node["codec"] = json(descriptor.codec == BlockDescriptor::Codec::zstd ? "zstd" : "raw");
  node["raw_bytes"] = descriptor.raw_bytes;
  node["stored_bytes"] = descriptor.stored_bytes;
  node["raw_sha256"] = descriptor.raw_sha256;
  return node;
}

BlockDescriptor decode_block_descriptor(const json& node) {
  BlockDescriptor descriptor;
  descriptor.id = string_field(node, "id");
  descriptor.path = string_field(node, "path");
  const std::string dtype = string_field(node, "dtype", "f64");
  if (dtype == "f32") {
    descriptor.dtype = DType::f32;
  } else if (dtype == "u64") {
    descriptor.dtype = DType::u64;
  } else if (dtype == "u32") {
    descriptor.dtype = DType::u32;
  } else if (dtype == "u16") {
    descriptor.dtype = DType::u16;
  } else if (dtype == "u8") {
    descriptor.dtype = DType::u8;
  } else if (dtype == "dyadic") {
    descriptor.dtype = DType::dyadic;
  } else {
    descriptor.dtype = DType::f64;
  }
  const json& shape = field(node, "shape");
  if (shape.is_array()) {
    for (const json& item : shape) descriptor.shape.push_back(item.get<U64>());
  }
  descriptor.index_id = string_field(node, "index_id");
  descriptor.unit = decode_unit(field(node, "unit"));
  descriptor.begin = u64_field(node, "begin", 0);
  descriptor.count = u64_field(node, "count", 0);
  descriptor.codec = string_field(node, "codec", "raw") == "zstd" ? BlockDescriptor::Codec::zstd
                                                                 : BlockDescriptor::Codec::raw;
  descriptor.raw_bytes = u64_field(node, "raw_bytes", 0);
  descriptor.stored_bytes = u64_field(node, "stored_bytes", 0);
  descriptor.raw_sha256 = string_field(node, "raw_sha256");
  return descriptor;
}

json encode_algorithm_identity(const AlgorithmIdentity& identity) {
  json node;
  node["name"] = identity.name;
  node["version"] = identity.version;
  node["source_hash"] = identity.source_hash;
  return node;
}

json encode_manifest(const ProductManifest& manifest) {
  json node;
  node["format"] = manifest.format;
  node["major"] = manifest.major;
  node["minor"] = manifest.minor;
  const char* stage = "normalize";
  switch (manifest.stage) {
    case ProductGraph::Stage::normalize: stage = "normalize"; break;
    case ProductGraph::Stage::mosaic: stage = "mosaic"; break;
    case ProductGraph::Stage::export_image: stage = "export_image"; break;
  }
  node["stage"] = json(stage);
  node["product_id"] = manifest.product_id;
  node["revision"] = manifest.revision;
  node["root_object"] = manifest.root_object;
  node["profile"] = manifest.profile;
  node["capabilities"] = manifest.capabilities;
  node["inputs"] = manifest.inputs;
  json objects = json::array();
  for (const ObjectEntry& entry : manifest.objects) {
    json item;
    item["hash"] = entry.hash;
    item["path"] = entry.path;
    item["type"] = entry.type;
    item["major"] = entry.major;
    item["minor"] = entry.minor;
    objects.push_back(item);
  }
  node["objects"] = objects;
  json blocks = json::array();
  for (const BlockDescriptor& descriptor : manifest.blocks) blocks.push_back(encode_block_descriptor(descriptor));
  node["blocks"] = blocks;
  node["configuration_hash"] = manifest.configuration_hash;
  json algorithms = json::array();
  for (const AlgorithmIdentity& identity : manifest.algorithm_set) {
    algorithms.push_back(encode_algorithm_identity(identity));
  }
  node["algorithm_set"] = algorithms;
  json arithmetic;
  arithmetic["mode"] = json(manifest.arithmetic.mode == Arithmetic::Mode::approximate ? "approximate"
                                                                                       : "outward");
  arithmetic["initial_bits"] = manifest.arithmetic.initial_bits;
  arithmetic["max_bits"] = manifest.arithmetic.max_bits;
  arithmetic["allow_target_fallback"] = manifest.arithmetic.allow_target_fallback;
  node["arithmetic"] = arithmetic;
  node["assessment"] = encode_assessment(manifest.assessment);
  node["complete"] = manifest.complete;
  return node;
}

ProductManifest decode_manifest(const json& node) {
  ProductManifest manifest;
  manifest.format = string_field(node, "format", "acsd.product");
  manifest.major = static_cast<U32>(u64_field(node, "major", 2));
  manifest.minor = static_cast<U32>(u64_field(node, "minor", 0));
  const std::string stage = string_field(node, "stage", "normalize");
  if (stage == "mosaic") {
    manifest.stage = ProductGraph::Stage::mosaic;
  } else if (stage == "export_image") {
    manifest.stage = ProductGraph::Stage::export_image;
  } else {
    manifest.stage = ProductGraph::Stage::normalize;
  }
  manifest.product_id = string_field(node, "product_id");
  manifest.revision = u64_field(node, "revision", 1);
  manifest.root_object = string_field(node, "root_object");
  manifest.profile = string_field(node, "profile");
  manifest.capabilities = string_list(node, "capabilities");
  manifest.inputs = id_list(node, "inputs");
  const json& objects = field(node, "objects");
  if (objects.is_array()) {
    for (const json& item : objects) {
      ObjectEntry entry;
      entry.hash = string_field(item, "hash");
      entry.path = string_field(item, "path");
      entry.type = string_field(item, "type");
      entry.major = static_cast<U32>(u64_field(item, "major", 2));
      entry.minor = static_cast<U32>(u64_field(item, "minor", 0));
      manifest.objects.push_back(entry);
    }
  }
  const json& blocks = field(node, "blocks");
  if (blocks.is_array()) {
    for (const json& item : blocks) manifest.blocks.push_back(decode_block_descriptor(item));
  }
  manifest.configuration_hash = string_field(node, "configuration_hash");
  const json& algorithms = field(node, "algorithm_set");
  if (algorithms.is_array()) {
    for (const json& item : algorithms) {
      AlgorithmIdentity identity;
      identity.name = string_field(item, "name");
      identity.version = string_field(item, "version");
      identity.source_hash = string_field(item, "source_hash");
      manifest.algorithm_set.push_back(identity);
    }
  }
  const json& arithmetic = field(node, "arithmetic");
  manifest.arithmetic.mode = string_field(arithmetic, "mode", "outward") == "approximate"
                                 ? Arithmetic::Mode::approximate
                                 : Arithmetic::Mode::outward;
  manifest.arithmetic.initial_bits = static_cast<U32>(u64_field(arithmetic, "initial_bits", 64));
  manifest.arithmetic.max_bits = static_cast<U32>(u64_field(arithmetic, "max_bits", 256));
  manifest.arithmetic.allow_target_fallback = bool_field(arithmetic, "allow_target_fallback", false);
  manifest.assessment = decode_assessment(field(node, "assessment"));
  manifest.complete = bool_field(node, "complete", false);
  return manifest;
}

}  // namespace acsd::workflows
