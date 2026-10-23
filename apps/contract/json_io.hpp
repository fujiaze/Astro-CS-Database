#pragma once
// JSON encoding and decoding for the contract driver.
//
// The driver never performs scientific computation: it decodes the typed
// objects of the request envelope, calls one public API entry point and
// encodes the result.  Every struct field name used here is the field name
// declared in include/acsd/contracts.hpp.
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "codec/value_codec.hpp"

#include "acsd/contracts.hpp"

namespace acsd::contract {

using json = nlohmann::json;

// A decoded typed object: the declared type name and the decoded payload.
struct TypedObject {
  std::string type;
  std::shared_ptr<const void> value;
  Id hash;  // content id of the decoded payload (empty for plain values)
  json payload;  // the payload the object was decoded from
};

// Decoding context: holds the request envelope, the storage and the objects
// that were decoded so far.  References are resolved by local name; a cycle is
// rejected instead of being hidden by a temporary file.
class DecodeContext {
 public:
  DecodeContext(Context& context, json input_objects, std::string input_root,
                std::vector<BlockDescriptor> blocks);

  // Decodes a node that is either {"$ref": "name"} / "name" or a typed object
  // {"type": "T", "data": {...}}.
  Result<TypedObject> decode_node(const json& node);
  // Decodes the object registered under `name`.
  Result<TypedObject> decode_named(const std::string& name);
  // Resolves a reference to an already decoded object.
  Result<TypedObject> resolve_reference(const std::string& name);
  // The reference resolution of a caller outside the decoder.
  Result<TypedObject> resolve(const std::string& name) { return resolve_reference(name); }

  Context& context() { return context_; }
  const std::vector<BlockDescriptor>& blocks() const { return blocks_; }
  const std::string& input_root() const { return input_root_; }

  // Registers an object for the response so that every referenced payload is
  // published in the response object table.
  void publish(const std::string& type, const Id& hash, const json& data);
  const std::map<std::string, json>& published() const { return published_; }
  const std::map<std::string, TypedObject>& decoded() const { return decoded_; }

 private:
  Context& context_;
  json input_objects_;
  std::string input_root_;
  std::vector<BlockDescriptor> blocks_;
  std::map<std::string, TypedObject> decoded_;
  std::vector<std::string> decoding_;
  std::map<std::string, json> published_;
};

// ---------------------------------------------------------------------------
// Value level helpers
// ---------------------------------------------------------------------------

using codec::encode_unit;
using codec::decode_unit;
using codec::encode_code;
using codec::decode_code;
json encode_identity(const Identity& identity);
json encode_assessment(const Assessment& assessment);
// The typed payload of a world coordinate model.  The object table of the
// response carries it, so that the result of make_wcs can be used as the
// input of the next request.
json encode_wcs_model(const WcsModel& wcs);
// The typed payload of a footprint provider: the world coordinate model it
// is built from.
json encode_footprint_provider(const WcsModel& wcs);
// The typed payload of an overlap result: the contributions of the leaves.
json encode_overlaps(const Overlaps& overlaps);
json encode_usage(const Usage& usage);
using codec::encode_condition;
json encode_index(const Index& index);
json encode_array(const Array& array, const std::vector<double>& values);
json encode_variance(const Variance& variance);
using codec::encode_header_fields;
json encode_support_map(const SupportMap& support);
// The typed payload of a signal estimate.
json encode_signal_estimate(const SignalEstimate& estimate);
// The typed payload of the recipe of an action.
json encode_action_recipe(const ActionRecipe& recipe);
// The typed payload of an effective point spread function.
json encode_effective_psf(const EffectivePSF& kernel);
// The typed payload of the measurements of the science sources.
json encode_source_measurements(const SourceMeasurements& measurements);

// Decoders for the plain (non model) contract types.
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
// The estimator of a signal: the identifier, the quantity, the unit and the
// condition.

using codec::encode_arithmetic;
using codec::decode_numeric_budget;
using codec::decode_solve_policy;
using codec::decode_astrometric_context;
using codec::decode_header_fields;
using codec::decode_block_request;
using codec::decode_limits;

// Canonical JSON text: keys sorted (nlohmann keeps objects ordered), no
// whitespace, no duplicate keys.
using codec::canonical_json;
using codec::canonical_hash;

// Converts a double into the canonical hexadecimal binary64 representation.
using codec::encode_f64;
// Reads a double either from a JSON number or from {"f64": "<16 hex>"}.
using codec::decode_f64;

}  // namespace acsd::contract
