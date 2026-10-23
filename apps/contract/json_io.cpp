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

// The flat value codecs live in src/codec/value_codec.* and are shared with the
// runtime typed codecs; the driver-specific envelope (object table) stays here.
using codec::kNull;
using codec::has;
using codec::field;
using codec::string_field;
using codec::double_field;
using codec::bool_field;
using codec::u64_field;
using codec::i32_field;
using codec::canonical_json;
using codec::canonical_hash;
using codec::encode_f64;
using codec::decode_f64;
using codec::encode_code;
using codec::decode_code;
using codec::noise_state_name;
using codec::noise_state_from;
using codec::encode_unit;
using codec::decode_unit;
using codec::decode_interval;
using codec::decode_exact_interval;
using codec::decode_box;
using codec::decode_pixel_center;
using codec::decode_pixel_edge;
using codec::decode_fits_pixel;
using codec::decode_sky;
using codec::decode_leaf;
using codec::decode_tolerance;
using codec::decode_arithmetic;
using codec::encode_arithmetic;
using codec::decode_numeric_budget;
using codec::decode_solve_policy;
using codec::decode_astrometric_context;
using codec::decode_header_fields;
using codec::encode_header_fields;
using codec::decode_block_request;
using codec::decode_limits;
using codec::encode_condition;

namespace {










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





// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------




// ---------------------------------------------------------------------------
// Plain values
// ---------------------------------------------------------------------------




















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



json encode_effective_psf(const EffectivePSF& kernel) {
  json node;
  const char* family = "gaussian";
  switch (kernel.family) {
    case PsfRequest::Family::gaussian: family = "gaussian"; break;
    case PsfRequest::Family::moffat: family = "moffat"; break;
    case PsfRequest::Family::airy: family = "airy"; break;
  }
  node["family"] = family;
  node["spatial_domain"] = json{{"lower", json::array({encode_f64(kernel.spatial_domain.lower[0]),
                                                        encode_f64(kernel.spatial_domain.lower[1])})},
                                 {"upper", json::array({encode_f64(kernel.spatial_domain.upper[0]),
                                                        encode_f64(kernel.spatial_domain.upper[1])})}};
  node["wavelength_nm"] = json{{"lower", encode_f64(kernel.wavelength_nm.lower)},
                                {"upper", encode_f64(kernel.wavelength_nm.upper)}};
  node["tail_mass_upper"] = encode_f64(kernel.tail_mass_upper);
  node["achromatic"] = kernel.achromatic;
  node["spatial_basis"] = json{{"kind", kernel.spatial_basis.kind == Basis::Kind::plane
                                            ? "plane"
                                            : kernel.spatial_basis.kind == Basis::Kind::polynomial
                                                ? "polynomial"
                                                : "constant"},
                                {"degree", kernel.spatial_basis.degree}};
  return node;
}

json encode_source_measurements(const SourceMeasurements& measurements) {
  json node;
  node["frame"] = measurements.frame;
  json stars = json::array();
  for (const StarFlux& star : measurements.stars) {
    json entry;
    entry["gaia_source_id"] = star.gaia_source_id;
    entry["integrated_flux"] = encode_f64(star.integrated_flux);
    entry["definition"] = star.definition;
    entry["window"] = json{{"lower", json::array({encode_f64(star.window.lower[0]),
                                                  encode_f64(star.window.lower[1])})},
                            {"upper", json::array({encode_f64(star.window.upper[0]),
                                                  encode_f64(star.window.upper[1])})}};
    stars.push_back(entry);
  }
  node["stars"] = stars;
  return node;
}

json encode_signal_estimate(const SignalEstimate& estimate) {
  json node;
  node["identity"] = encode_identity(estimate.identity);
  node["assessment"] = encode_assessment(estimate.assessment);
  node["signal"] = json{{"$ref", estimate.signal ? estimate.signal->identity.object_id : Id()}};
  node["unit"] = encode_unit(estimate.unit);
  json estimator_node;
  estimator_node["id"] = estimate.estimator.id;
  estimator_node["quantity"] = estimate.estimator.quantity;
  estimator_node["unit"] = encode_unit(estimate.estimator.unit);
  estimator_node["reference_sed"] =
      estimate.estimator.reference_sed ? json(*estimate.estimator.reference_sed) : json(nullptr);
  node["estimator"] = estimator_node;
  node["numerical_lower"] =
      json{{"$ref", estimate.numerical_lower ? estimate.numerical_lower->identity.object_id : Id()}};
  node["numerical_upper"] =
      json{{"$ref", estimate.numerical_upper ? estimate.numerical_upper->identity.object_id : Id()}};
  node["support"] =
      json{{"$ref", estimate.support ? estimate.support->identity.object_id : Id()}};
  node["effective_response"] = json{
      {"$ref", estimate.effective_response ? estimate.effective_response->identity.object_id : Id()}};
  node["noise"] = json{{"$ref", estimate.noise ? estimate.noise->identity.object_id : Id()}};
  node["error"] = json{{"$ref", estimate.error ? estimate.error->identity.object_id : Id()}};
  return node;
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
  node["kind"] = kind;
  node["algorithm"] = recipe.algorithm;
  node["version"] = recipe.version;
  node["children"] = recipe.children;
  node["arrays"] = recipe.arrays;
  node["spaces"] = json{{"input", encode_unit(recipe.spaces.input)},
                        {"output", encode_unit(recipe.spaces.output)}};
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
  // The arrays and the index of the support travel with the payload: the next
  // request decodes the same support.
  node["index"] = json{{"$ref", support.index ? support.index->identity.object_id : Id()}};
  node["states"] = json{{"$ref", support.states ? support.states->identity.object_id : Id()}};
  node["area_lower_sr"] =
      json{{"$ref", support.area_lower_sr ? support.area_lower_sr->identity.object_id : Id()}};
  node["area_upper_sr"] =
      json{{"$ref", support.area_upper_sr ? support.area_upper_sr->identity.object_id : Id()}};
  node["normalization_states"] = json{
      {"$ref", support.normalization_states ? support.normalization_states->identity.object_id
                                           : Id()}};
  node["observational_states"] = json{
      {"$ref", support.observational_states ? support.observational_states->identity.object_id
                                            : Id()}};
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



// The estimator of a signal.
Estimator decode_estimator(const json& node) {
  Estimator estimator;
  estimator.id = string_field(node, "id");
  estimator.quantity = string_field(node, "quantity");
  estimator.unit = decode_unit(field(node, "unit"));
  estimator.reference_sed = string_field(node, "reference_sed");
  return estimator;
}

// The identifiers of a list field.
std::vector<Id> id_list(const json& node, const char* key) {
  std::vector<Id> identifiers;
  const json& list = field(node, key);
  if (!list.is_array()) return identifiers;
  for (const json& item : list) {
    if (item.is_string()) identifiers.push_back(item.get<std::string>());
  }
  return identifiers;
}

// The geometry budget of a request.
GeometryBudget decode_geometry_budget(const json& node) {
  GeometryBudget budget;
  const json& functionals = node.contains("requested_functionals") ? node.at("requested_functionals")
                                                                   : json::array();
  if (functionals.is_array()) {
    for (const json& item : functionals) {
      if (!item.is_string()) continue;
      QuantityRequest functional;
      functional.id = item.get<std::string>();
      functional.kind = QuantityRequest::Kind::area;
      budget.requested_functionals.push_back(functional);
    }
  }
  const json& per_leaf = node.contains("per_leaf_absolute_area") ? node.at("per_leaf_absolute_area")
                                                                 : json::array();
  if (per_leaf.is_array()) {
    for (const json& item : per_leaf) {
      LeafAreaLimit limit;
      limit.absolute_sr = decode_f64(item);
      budget.per_leaf_absolute_area.push_back(limit);
    }
  }
  budget.area_absolute_sr = node.contains("area_absolute_sr") ? decode_f64(node.at("area_absolute_sr"))
                                                              : 0.0;
  budget.normalizer_min_sr = node.contains("normalizer_min_sr")
                                 ? decode_f64(node.at("normalizer_min_sr"))
                                 : 0.0;
  budget.max_work = node.contains("max_work") ? static_cast<U64>(node.at("max_work").get<U64>())
                                               : 1000000ull;
  budget.max_bytes = node.contains("max_bytes") ? static_cast<U64>(node.at("max_bytes").get<U64>())
                                                 : 268435456ull;
  budget.max_depth = node.contains("max_depth")
                         ? static_cast<U32>(node.at("max_depth").get<U32>())
                         : 64u;
  budget.arithmetic = decode_arithmetic(node.contains("arithmetic") ? node.at("arithmetic")
                                                                     : json::object());
  return budget;
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
                                        wcs.identity.object_id,
                                        payload});
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

  if (type == "SupportMap") {
    SupportMap support;
    const std::string kind = string_field(payload, "kind", "primitives");
    if (kind == "dyadic_cells") {
      support.kind = SupportMap::Kind::dyadic_cells;
    } else if (kind == "full_leaf_approx") {
      support.kind = SupportMap::Kind::full_leaf_approx;
    } else if (kind == "effective_area_approx") {
      support.kind = SupportMap::Kind::effective_area_approx;
    } else {
      support.kind = SupportMap::Kind::primitives;
    }
    support.primitives = id_list(payload, "primitives");
    support.geometry_snapshot = string_field(payload, "geometry_snapshot");
    // The leaf index of the support is reopened: the product publishes it.
    const json& index_node = field(payload, "index");
    if (index_node.is_object() && index_node.contains("$ref")) {
      Result<TypedObject> referenced =
          resolve_reference(index_node.at("$ref").get<std::string>());
      if (!ok(referenced)) {
        return model::fail<TypedObject>(referenced.code,
                                         "the leaf index of the support is missing",
                                         "contract.decode", "the support cannot be decoded");
      }
      if (val(referenced).type != "Index") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the index of the support is not an index",
                                         "contract.decode", "the support cannot be decoded");
      }
      support.index = std::static_pointer_cast<const Index>(val(referenced).value);
    }
    support.identity = identity_for("acsd.SupportMap", payload);
    support.assessment = model::make_assessment(Code::ok);
    support.assessment.complete = true;
    publish("SupportMap", support.identity.object_id, payload);
    return model::ok_result(TypedObject{"SupportMap", std::make_shared<const SupportMap>(support),
                                        support.identity.object_id});
  }

  if (type == "SignalEstimate") {
    SignalEstimate estimate;
    estimate.identity = identity_for("acsd.SignalEstimate", payload);
    estimate.assessment = model::make_assessment(Code::ok);
    estimate.assessment.complete = true;
    estimate.unit = decode_unit(field(payload, "unit"));
    estimate.estimator = decode_estimator(field(payload, "estimator"));
    // The arrays of the estimate.
    const char* const arrays[3] = {"signal", "numerical_lower", "numerical_upper"};
    ArrayRef* const targets[3] = {&estimate.signal, &estimate.numerical_lower,
                                  &estimate.numerical_upper};
    for (int index = 0; index < 3; ++index) {
      const json& node = field(payload, arrays[index]);
      if (!node.is_object() || !node.contains("$ref")) continue;
      Result<TypedObject> referenced = resolve_reference(node.at("$ref").get<std::string>());
      if (!ok(referenced)) {
        return model::fail<TypedObject>(referenced.code,
                                        std::string("the estimate references the unknown object ") +
                                            node.at("$ref").get<std::string>(),
                                        "contract.decode", "the estimate cannot be decoded");
      }
      if (val(referenced).type != "Array") {
        return model::fail<TypedObject>(Code::invalid_request,
                                        "the estimate references an object that is not an array",
                                        "contract.decode", "the estimate cannot be decoded");
      }
      *targets[index] = std::static_pointer_cast<const Array>(val(referenced).value);
    }
    // The support map of the estimate.
    const json& support_node = field(payload, "support");
    if (support_node.is_object() && support_node.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(support_node.at("$ref").get<std::string>());
      if (!ok(referenced)) {
        return model::fail<TypedObject>(referenced.code,
                                        "the support map of the estimate is missing",
                                        "contract.decode", "the estimate cannot be decoded");
      }
      if (val(referenced).type != "SupportMap") {
        return model::fail<TypedObject>(Code::invalid_request,
                                        "the support of the estimate is not a support map",
                                        "contract.decode", "the estimate cannot be decoded");
      }
      estimate.support = std::static_pointer_cast<const SupportMap>(val(referenced).value);
    }
    publish("SignalEstimate", estimate.identity.object_id, payload);
    return model::ok_result(TypedObject{"SignalEstimate",
                                        std::make_shared<const SignalEstimate>(estimate),
                                        estimate.identity.object_id});
  }

  if (type == "ExportRequest") {
    ExportRequest request;
    const json& target = field(payload, "target");
    const json& model_node = field(target, "model");
    Result<TypedObject> wcs_object = resolve_reference(
        model_node.is_object() && model_node.contains("$ref")
            ? model_node.at("$ref").get<std::string>()
            : std::string());
    if (!ok(wcs_object)) {
      return model::fail<TypedObject>(wcs_object.code,
                                       "the export target has no world coordinate model",
                                       "contract.decode", "the request cannot be decoded");
    }
    request.target.model = std::static_pointer_cast<const WcsModel>(val(wcs_object).value);
    request.target.width = u64_field(target, "width", 0);
    request.target.height = u64_field(target, "height", 0);
    const std::string quantity = string_field(payload, "quantity", "supported_mean");
    if (quantity == "supported_flux") {
      request.quantity = ExportRequest::Quantity::supported_flux;
    } else if (quantity == "extrapolated_full_flux") {
      request.quantity = ExportRequest::Quantity::extrapolated_full_flux;
    } else {
      request.quantity = ExportRequest::Quantity::supported_mean;
    }
    const std::string policy = string_field(payload, "support_policy", "dyadic_cells");
    if (policy == "primitives") {
      request.support_policy = SupportMap::Kind::primitives;
    } else if (policy == "full_leaf_approx") {
      request.support_policy = SupportMap::Kind::full_leaf_approx;
    } else if (policy == "effective_area_approx") {
      request.support_policy = SupportMap::Kind::effective_area_approx;
    } else {
      request.support_policy = SupportMap::Kind::dyadic_cells;
    }
    request.assume_uniform_uncovered = payload.contains("assume_uniform_uncovered") &&
                                       payload.at("assume_uniform_uncovered").is_boolean() &&
                                       payload.at("assume_uniform_uncovered").get<bool>();
    const std::string dtype = string_field(payload, "output_dtype", "f64");
    request.output_dtype = dtype == "f32" ? DType::f32 : DType::f64;
    request.budget = decode_geometry_budget(field(payload, "budget"));
    publish("ExportRequest", "acsd.ExportRequest", payload);
    return model::ok_result(TypedObject{"ExportRequest",
                                        std::make_shared<const ExportRequest>(request),
                                        "acsd.ExportRequest"});
  }

  if (type == "SpectralPrediction") {
    SpectralPrediction prediction;
    const char* const arrays[2] = {"basis_integrals", "absolute_errors"};
    ArrayRef* const targets[2] = {&prediction.basis_integrals, &prediction.absolute_errors};
    for (int index = 0; index < 2; ++index) {
      const json& node = field(payload, arrays[index]);
      if (!node.is_object() || !node.contains("$ref")) continue;
      Result<TypedObject> referenced = resolve_reference(node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "Array") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         std::string("the array ") + arrays[index] +
                                             " of the prediction cannot be decoded",
                                         "contract.decode", "the prediction cannot be decoded");
      }
      *targets[index] = std::static_pointer_cast<const Array>(val(referenced).value);
    }
    prediction.support_nm = decode_interval(field(payload, "support_nm"));
    prediction.identity = identity_for("acsd.SpectralPrediction", payload);
    prediction.assessment = model::make_assessment(Code::ok);
    prediction.assessment.complete = true;
    publish("SpectralPrediction", prediction.identity.object_id, payload);
    return model::ok_result(
        TypedObject{"SpectralPrediction", std::make_shared<const SpectralPrediction>(prediction),
                    prediction.identity.object_id});
  }

  if (type == "PhotoSolution") {
    PhotoSolution solution;
    const json& theta = field(payload, "theta");
    if (theta.is_object() && theta.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(theta.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "Array") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the parameters of the solution cannot be decoded",
                                         "contract.decode", "the solution cannot be decoded");
      }
      solution.theta = std::static_pointer_cast<const Array>(val(referenced).value);
    }
    solution.application_id = string_field(payload, "application_id");
    solution.frames = id_list(payload, "frames");
    solution.spatial_basis.kind = Basis::Kind::constant;
    solution.identity = identity_for("acsd.PhotoSolution", payload);
    solution.assessment = model::make_assessment(Code::ok);
    solution.assessment.complete = true;
    publish("PhotoSolution", solution.identity.object_id, payload);
    return model::ok_result(
        TypedObject{"PhotoSolution", std::make_shared<const PhotoSolution>(solution),
                    solution.identity.object_id});
  }

  if (type == "CatalogueStars") {
    const json& rows = field(payload, "stars");
    if (!rows.is_array()) {
      return model::fail<TypedObject>(Code::invalid_request, "the catalogue stars are not a list",
                                       "contract.decode", "the stars cannot be decoded");
    }
    publish("CatalogueStars", "acsd.CatalogueStars", payload);
    return model::ok_result(TypedObject{"CatalogueStars", nullptr, "acsd.CatalogueStars", payload});
  }

  if (type == "FootprintSet") {
    FootprintSet set;
    set.width = u64_field(payload, "width", 0);
    set.height = u64_field(payload, "height", 0);
    const json& matrix = field(payload, "detector_from_sample");
    if (matrix.is_array() && matrix.size() == 4) {
      set.detector_from_sample = {decode_f64(matrix[0]), decode_f64(matrix[1]),
                                  decode_f64(matrix[2]), decode_f64(matrix[3])};
    }
    const json& offset = field(payload, "offset");
    if (offset.is_array() && offset.size() == 2) {
      set.offset = {decode_f64(offset[0]), decode_f64(offset[1])};
    }
    const json& cell = field(payload, "local_cell");
    if (cell.is_array() && cell.size() == 2) {
      set.local_cell = {{decode_f64(cell[0].at(0)), decode_f64(cell[0].at(1))},
                        {decode_f64(cell[1].at(0)), decode_f64(cell[1].at(1))}};
    }
    set.detector_frame = string_field(payload, "detector_frame");
    // The index and the world coordinate model of the set.
    const json& index_node = field(payload, "index");
    if (index_node.is_object() && index_node.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(index_node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "Index") {
        return model::fail<TypedObject>(Code::invalid_request, "the index of the footprint set "
                                                             "cannot be decoded",
                                         "contract.decode", "the set cannot be decoded");
      }
      set.index = std::static_pointer_cast<const Index>(val(referenced).value);
    }
    const json& wcs_node = field(payload, "wcs");
    if (wcs_node.is_object() && wcs_node.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(wcs_node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "WcsModel") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the world coordinate model of the footprint set cannot "
                                         "be decoded",
                                         "contract.decode", "the set cannot be decoded");
      }
      set.wcs = std::static_pointer_cast<const WcsModel>(val(referenced).value);
    }
    // The inline footprints of the sources of the set.
    const json& inline_footprints = field(payload, "inline_footprints");
    if (inline_footprints.is_array()) {
      for (const json& item : inline_footprints) {
        DetectorFootprint footprint;
        const json& wcs_node = field(item, "wcs");
        if (wcs_node.is_object() && wcs_node.contains("$ref")) {
          Result<TypedObject> referenced =
              resolve_reference(wcs_node.at("$ref").get<std::string>());
          if (!ok(referenced) || val(referenced).type != "WcsModel") {
            return model::fail<TypedObject>(Code::invalid_request,
                                             "the world coordinate model of a footprint cannot "
                                             "be decoded",
                                             "contract.decode", "the set cannot be decoded");
          }
          footprint.wcs = std::static_pointer_cast<const WcsModel>(val(referenced).value);
        }
        const json& rings = field(item, "detector_rings");
        if (rings.is_array()) {
          for (const json& ring : rings) {
            Ring entry;
            entry.hole = field(ring, "hole").is_boolean() ? field(ring, "hole").get<bool>() : false;
            const json& vertices = field(ring, "vertices");
            if (vertices.is_array()) {
              for (const json& vertex : vertices) {
                if (vertex.is_array() && vertex.size() == 2) {
                  entry.vertices.push_back(PixelEdge0{decode_f64(vertex[0]), decode_f64(vertex[1])});
                }
              }
            }
            footprint.detector_rings.push_back(entry);
          }
        }
        const json& source = field(item, "source");
        if (source.is_object()) {
          footprint.source.dataset = string_field(source, "dataset");
          footprint.source.exposure = string_field(source, "exposure");
          footprint.source.detector = string_field(source, "detector");
          footprint.source.channel = string_field(source, "channel");
          footprint.source.sample = u64_field(source, "sample", 0);
        }
        set.inline_footprints.push_back(std::make_shared<const DetectorFootprint>(footprint));
      }
    }
    set.identity = identity_for("acsd.FootprintSet", payload);
    set.assessment = model::make_assessment(Code::ok);
    set.assessment.complete = true;
    publish("FootprintSet", set.identity.object_id, payload);
    return model::ok_result(TypedObject{"FootprintSet", std::make_shared<const FootprintSet>(set),
                                        set.identity.object_id});
  }

  if (type == "EffectivePSF") {
    EffectivePSF kernel;
    const std::string family = string_field(payload, "family", "gaussian");
    kernel.family = family == "moffat"    ? PsfRequest::Family::moffat
                    : family == "airy"     ? PsfRequest::Family::airy
                                           : PsfRequest::Family::gaussian;
    if (field(payload, "parameters").is_array() ||
        (field(payload, "parameters").is_object() && field(payload, "parameters").contains("$ref"))) {
      Result<ArrayRef> parameter_array =
          store_f64_values(context_, field(payload, "parameters"), "acsd.psf.parameters");
      if (!ok(parameter_array)) {
        return model::fail<TypedObject>(parameter_array.code,
                                         "the parameters of the kernel cannot be stored",
                                         "contract.decode", "the kernel cannot be decoded");
      }
      kernel.parameters = val(parameter_array);
    }
    kernel.spatial_domain = decode_box(field(payload, "spatial_domain"));
    kernel.wavelength_nm = decode_interval(field(payload, "wavelength_nm"));
    kernel.tail_mass_upper = decode_f64(field(payload, "tail_mass_upper"));
    kernel.achromatic = !payload.contains("achromatic") ||
                        payload.at("achromatic").is_boolean() == false
                            ? true
                            : payload.at("achromatic").get<bool>();
    const json& basis = field(payload, "spatial_basis");
    const std::string kind = string_field(basis, "kind", "constant");
    kernel.spatial_basis.kind = kind == "plane"    ? Basis::Kind::plane
                                : kind == "polynomial" ? Basis::Kind::polynomial
                                                      : Basis::Kind::constant;
    kernel.spatial_basis.degree = static_cast<U32>(u64_field(basis, "degree", 0));
    kernel.spatial_basis.normalization_domain = decode_box(field(basis, "normalization_domain"));
    kernel.identity = identity_for("acsd.EffectivePSF", payload);
    kernel.assessment = model::make_assessment(Code::ok);
    kernel.assessment.complete = true;
    publish("EffectivePSF", kernel.identity.object_id, payload);
    return model::ok_result(TypedObject{"EffectivePSF", std::make_shared<const EffectivePSF>(kernel),
                                        kernel.identity.object_id});
  }

  if (type == "NativeObservation") {
    NativeObservation observation;
    observation.frame = string_field(payload, "frame");
    observation.physical_collection = !payload.contains("physical_collection") ||
                                     payload.at("physical_collection").is_boolean() == false
                                         ? true
                                         : payload.at("physical_collection").get<bool>();
    observation.unit = decode_unit(field(payload, "unit"));
    observation.scale_applications = id_list(payload, "scale_applications");
    // The arrays of the observation.
    const char* const arrays[2] = {"integrated_values", "dq"};
    ArrayRef* const targets[2] = {&observation.integrated_values, &observation.dq};
    for (int index = 0; index < 2; ++index) {
      const json& node = field(payload, arrays[index]);
      if (!node.is_object() || !node.contains("$ref")) continue;
      Result<TypedObject> referenced = resolve_reference(node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "Array") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         std::string("the array ") + arrays[index] +
                                             " of the observation cannot be decoded",
                                         "contract.decode", "the observation cannot be decoded");
      }
      *targets[index] = std::static_pointer_cast<const Array>(val(referenced).value);
    }
    // The index of the observation.
    const json& index_node = field(payload, "native_index");
    if (index_node.is_object() && index_node.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(index_node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "Index") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the index of the observation cannot be decoded",
                                         "contract.decode", "the observation cannot be decoded");
      }
      observation.native_index = std::static_pointer_cast<const Index>(val(referenced).value);
    }
    // The footprint set of the observation.
    const json& footprints_node = field(payload, "footprints");
    if (footprints_node.is_object() && footprints_node.contains("$ref")) {
      Result<TypedObject> referenced =
          resolve_reference(footprints_node.at("$ref").get<std::string>());
      if (!ok(referenced) || val(referenced).type != "FootprintSet") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the footprint set of the observation cannot be decoded",
                                         "contract.decode", "the observation cannot be decoded");
      }
      observation.footprints = std::static_pointer_cast<const FootprintSet>(val(referenced).value);
    }
    observation.identity = identity_for("acsd.NativeObservation", payload);
    observation.assessment = model::make_assessment(Code::ok);
    observation.assessment.complete = true;
    publish("NativeObservation", observation.identity.object_id, payload);
    return model::ok_result(
        TypedObject{"NativeObservation", std::make_shared<const NativeObservation>(observation),
                    observation.identity.object_id});
  }

  if (type == "SourceMeasurements") {
    SourceMeasurements measurements;
    measurements.frame = string_field(payload, "frame");
    const json& stars = field(payload, "stars");
    if (stars.is_array()) {
      for (const json& item : stars) {
        StarFlux star;
        star.gaia_source_id = u64_field(item, "gaia_source_id", 0);
        star.integrated_flux = decode_f64(field(item, "integrated_flux"));
        star.definition = string_field(item, "definition");
        star.window = decode_box(field(item, "window"));
        measurements.stars.push_back(star);
      }
    }
    measurements.identity = identity_for("acsd.SourceMeasurements", payload);
    measurements.assessment = model::make_assessment(Code::ok);
    measurements.assessment.complete = true;
    publish("SourceMeasurements", measurements.identity.object_id, payload);
    return model::ok_result(
        TypedObject{"SourceMeasurements", std::make_shared<const SourceMeasurements>(measurements),
                    measurements.identity.object_id});
  }

  if (type == "ProductGraph") {
    ProductGraph graph;
    const std::string stage = string_field(payload, "stage", "normalize");
    graph.stage = stage == "mosaic"    ? ProductGraph::Stage::mosaic
                  : stage == "export_image" ? ProductGraph::Stage::export_image
                                           : ProductGraph::Stage::normalize;
    graph.profile = string_field(payload, "profile", "full");
    graph.inputs = id_list(payload, "inputs");
    // The signal of the graph.
    const json& signal = field(payload, "signal");
    if (signal.is_object() && signal.contains("$ref")) {
      Result<TypedObject> referenced = resolve_reference(signal.at("$ref").get<std::string>());
      if (!ok(referenced)) {
        return model::fail<TypedObject>(referenced.code, "the signal of the graph is missing",
                                         "contract.decode", "the graph cannot be decoded");
      }
      if (val(referenced).type != "SignalEstimate") {
        return model::fail<TypedObject>(Code::invalid_request,
                                         "the signal of the graph is not a signal estimate",
                                         "contract.decode", "the graph cannot be decoded");
      }
      graph.signal = std::static_pointer_cast<const SignalEstimate>(val(referenced).value);
    }
    // The closure of the graph: the objects of the product.
    const json& closure = field(payload, "closure");
    if (closure.is_array()) {
      for (const json& item : closure) {
        const std::string name = item.is_object() && item.contains("$ref")
                                     ? item.at("$ref").get<std::string>()
                                     : std::string();
        if (name.empty()) continue;
        Result<TypedObject> referenced = resolve_reference(name);
        if (!ok(referenced)) {
          return model::fail<TypedObject>(referenced.code,
                                           "the closure of the graph names the unknown object " + name,
                                           "contract.decode", "the graph cannot be decoded");
        }
        graph.closure.push_back(
            std::static_pointer_cast<const Model>(val(referenced).value));
      }
    }
    graph.identity = identity_for("acsd.ProductGraph", payload);
    graph.assessment = model::make_assessment(Code::ok);
    graph.assessment.complete = true;
    publish("ProductGraph", graph.identity.object_id, payload);
    return model::ok_result(TypedObject{"ProductGraph", std::make_shared<const ProductGraph>(graph),
                                        graph.identity.object_id});
  }

  if (type == "ExportWorkflowRequest") {
    ExportWorkflowRequest request;
    const json& input = field(payload, "input");
    if (input.is_object()) {
      if (input.contains("directory") && input.at("directory").is_string()) {
        request.input.directory = input.at("directory").get<std::string>();
      }
      if (input.contains("manifest_hash") && input.at("manifest_hash").is_string()) {
        request.input.manifest_hash = input.at("manifest_hash").get<std::string>();
      }
    }
    const json& projection = field(payload, "projection");
    Result<TypedObject> projection_object = resolve_reference(
        projection.is_object() && projection.contains("$ref")
            ? projection.at("$ref").get<std::string>()
            : std::string());
    if (!ok(projection_object)) {
      return model::fail<TypedObject>(projection_object.code,
                                       "the export request is missing", "contract.decode",
                                       "the request cannot be decoded");
    }
    if (val(projection_object).type != "ExportRequest") {
      return model::fail<TypedObject>(Code::invalid_request,
                                       "the projection is not an ExportRequest", "contract.decode",
                                       "the request cannot be decoded");
    }
    request.projection = *std::static_pointer_cast<const ExportRequest>(val(projection_object).value);
    const json& output = field(payload, "output");
    Result<TypedObject> output_object = resolve_reference(
        output.is_object() && output.contains("$ref") ? output.at("$ref").get<std::string>()
                                                      : std::string());
    if (!ok(output_object)) {
      return model::fail<TypedObject>(output_object.code, "the publish request is missing",
                                       "contract.decode", "the request cannot be decoded");
    }
    if (val(output_object).type != "PublishRequest") {
      return model::fail<TypedObject>(Code::invalid_request,
                                       "the output is not a PublishRequest", "contract.decode",
                                       "the request cannot be decoded");
    }
    request.output = *std::static_pointer_cast<const PublishRequest>(val(output_object).value);
    publish("ExportWorkflowRequest", "acsd.ExportWorkflowRequest", payload);
    return model::ok_result(
        TypedObject{"ExportWorkflowRequest",
                    std::make_shared<const ExportWorkflowRequest>(request),
                    "acsd.ExportWorkflowRequest"});
  }

  if (type == "PublishRequest") {
    PublishRequest request;
    request.destination = string_field(payload, "destination");
    const std::string dtype = string_field(payload, "signal_dtype", "f64");
    request.signal_dtype = dtype == "f32" ? DType::f32 : DType::f64;
    request.profile = string_field(payload, "profile", "full");
    request.tile_log2_width = static_cast<U32>(u64_field(payload, "tile_log2_width", 6));
    request.durable = !payload.contains("durable") || payload.at("durable").is_boolean() == false
                          ? true
                          : payload.at("durable").get<bool>();
    publish("PublishRequest", "acsd.PublishRequest", payload);
    return model::ok_result(TypedObject{"PublishRequest",
                                        std::make_shared<const PublishRequest>(request),
                                        "acsd.PublishRequest"});
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
