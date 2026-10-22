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

#include "acsd/contracts.hpp"

namespace acsd::contract {

using json = nlohmann::json;

// A decoded typed object: the declared type name and the decoded payload.
struct TypedObject {
  std::string type;
  std::shared_ptr<const void> value;
  Id hash;  // content id of the decoded payload (empty for plain values)
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

json encode_unit(const Unit& unit);
Unit decode_unit(const json& node);
json encode_code(Code code);
Code decode_code(const json& node);
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
json encode_condition(const Condition& condition);
json encode_index(const Index& index);
json encode_array(const Array& array, const std::vector<double>& values);
json encode_variance(const Variance& variance);
json encode_header_fields(const HeaderFields& fields);
json encode_support_map(const SupportMap& support);

// Decoders for the plain (non model) contract types.
Interval decode_interval(const json& node);
ExactInterval decode_exact_interval(const json& node);
Box decode_box(const json& node);
PixelCenter0 decode_pixel_center(const json& node);
PixelEdge0 decode_pixel_edge(const json& node);
FitsPixel1 decode_fits_pixel(const json& node);
SkyICRS decode_sky(const json& node);
HealpixLeaf decode_leaf(const json& node);
Tolerance decode_tolerance(const json& node);
Arithmetic decode_arithmetic(const json& node);
json encode_arithmetic(const Arithmetic& arithmetic);
NumericBudget decode_numeric_budget(const json& node);
SolvePolicy decode_solve_policy(const json& node);
AstrometricContext decode_astrometric_context(const json& node);
HeaderFields decode_header_fields(const json& node);
BlockRequest decode_block_request(const json& node);
Limits decode_limits(const json& node);

// Canonical JSON text: keys sorted (nlohmann keeps objects ordered), no
// whitespace, no duplicate keys.
std::string canonical_json(const json& node);
Id canonical_hash(const json& node);

// Converts a double into the canonical hexadecimal binary64 representation.
json encode_f64(double value);
// Reads a double either from a JSON number or from {"f64": "<16 hex>"}.
double decode_f64(const json& node);

}  // namespace acsd::contract
