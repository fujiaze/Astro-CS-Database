#pragma once
// Canonical JSON encoding of the product objects.
//
// The product format is a canonical object graph: keys are sorted by their
// UTF-8 bytes, the text carries no whitespace and no duplicate key, integers
// are plain decimal and every binary64 is written as the hexadecimal bit
// pattern so that the round trip is exact.
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "acsd/contracts.hpp"

namespace acsd::workflows {

using json = nlohmann::json;

// Canonical text of a JSON document.
std::string canonical_text(const json& node);
// Canonical content id of a JSON document.
Id canonical_id(const json& node);

// The canonical representation of one binary64 value.
json f64_value(double value);
// Reads a binary64 from a canonical value or from a plain number.
double read_f64(const json& node);

// The reference of an object: {"$ref": "hash"}.
Id decode_ref(const json& node);
json encode_unit(const Unit& unit);
Unit decode_unit(const json& node);
json encode_condition(const Condition& condition);
Condition decode_condition(const json& node);
json encode_assessment(const Assessment& assessment);
Assessment decode_assessment(const json& node);
json encode_identity(const Identity& identity);
Identity decode_identity(const json& node);
json encode_estimator(const Estimator& estimator);
Estimator decode_estimator(const json& node);
json encode_error_record(const ErrorRecord& record);
ErrorRecord decode_error_record(const json& node);
json encode_index(const Index& index);
Index decode_index(const json& node);
json encode_array_descriptor(const Array& array);
Array decode_array_descriptor(const json& node);
json encode_support_map(const SupportMap& support);
SupportMap decode_support_map(const json& node);
json encode_signal_estimate(const SignalEstimate& estimate);
SignalEstimate decode_signal_estimate(const json& node);
json encode_noise_model(const NoiseModel& model);
NoiseModel decode_noise_model(const json& node);
json encode_noise_root(const NoiseRoot& root);
NoiseRoot decode_noise_root(const json& node);
json encode_action_recipe(const ActionRecipe& recipe);
ActionRecipe decode_action_recipe(const json& node);
json encode_linear_action(const LinearAction& action, const std::vector<Id>& children,
                          const std::vector<Id>& arrays);
json encode_covariance_action(const CovarianceAction& covariance);
CovarianceAction decode_covariance_action(const json& node);
json encode_spatial_response(const SpatialResponse& response);
SpatialResponse decode_spatial_response(const json& node);
json encode_product_graph(const ProductGraph& graph);
ProductGraph decode_product_graph(const json& node);
json encode_manifest(const ProductManifest& manifest);
ProductManifest decode_manifest(const json& node);
json encode_block_descriptor(const BlockDescriptor& descriptor);
BlockDescriptor decode_block_descriptor(const json& node);
json encode_algorithm_identity(const AlgorithmIdentity& identity);

}  // namespace acsd::workflows
