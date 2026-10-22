// The contract driver implementation.
#include "driver.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"

namespace acsd::contract {
namespace {

using model::content_id;

// A cancellation source that is never requested by the driver.
class NeverCancelled final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

json build_info() {
  json node;
  node["name"] = "acsd-contract";
  node["version"] = "2.0.0";
  node["standard"] = "acsd.contract/1";
#if defined(__VERSION__)
  node["compiler"] = __VERSION__;
#else
  node["compiler"] = "unknown";
#endif
  return node;
}

json algorithm_entry(const std::string& name, const std::string& version) {
  json node;
  node["name"] = name;
  node["version"] = version;
  node["source_hash"] = content_id(model::sha256_hex(name + "/" + version));
  return node;
}

// Reads the f64 payload of an array.
Result<std::vector<double>> read_values(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no array supplied",
                                            "contract.decode", "the array cannot be read");
  }
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  return operators::read_f64(context, array, 0, count);
}

json encode_array_result(const Array& array, const std::vector<double>& values) {
  json node;
  node["type"] = "Array";
  node["dtype"] = "f64";
  node["shape"] = array.shape;
  node["unit"] = encode_unit(array.unit);
  node["index_id"] = array.index_id;
  node["values"] = values;
  return node;
}

json encode_ref_value(const Id& hash) { return json{{"$ref", hash}}; }

class Driver {
 public:
  explicit Driver(const json& envelope, std::filesystem::path base_directory)
      : envelope_(envelope), base_directory_(std::move(base_directory)) {}

  int run(const std::filesystem::path& response_path);

 private:
  json failure_body(Code code, const std::string& reason, const std::string& origin);
  std::filesystem::path resolve_root(const std::string& declared) const;
  json envelope_header() const;

  Result<TypedObject> argument(const std::string& name);
  Result<Ref<LinearAction>> linear_action_argument(const std::string& name);
  Result<ArrayRef> array_argument(const std::string& name);
  Result<Ref<CovarianceAction>> covariance_argument(const std::string& name);

  json dispatch();
  json run_to_fits();
  json run_from_fits(bool center);
  json run_apply(bool transpose);
  json run_quadratic_form();
  json run_covariance_apply();
  json run_make_covariance();
  json run_make_wcs();
  json run_make_footprint_provider();
  json run_open_product();
  json run_build_export();
  json run_export_signal();
  json run_integrate_response();
  json run_control_covariance();
  json run_measure();
  json run_compute_overlap();
  json run_forward();
  json run_inverse();
  json run_derivative();
  json run_solid_angle_jacobian();
  json run_storage_copy();
  json run_storage_read();
  json run_storage_open();

  void write_result(const json& result);

  json envelope_;
  std::filesystem::path base_directory_;
  json arguments_;
  json request_;
  std::string request_id_;
  std::string operation_;
  Limits limits_;
  std::string output_root_;
  std::string input_root_;
  std::vector<BlockDescriptor> input_blocks_;

  std::unique_ptr<resources::CpuResources> resources_;
  std::unique_ptr<storage::LocalStorage> storage_;
  std::unique_ptr<NeverCancelled> cancellation_;
  std::unique_ptr<Context> context_;
  std::unique_ptr<DecodeContext> decoder_;

  json result_;
  std::vector<BlockDescriptor> output_blocks_;
};


// Extracts the first issue reason of a failed result so that the response
// keeps the concrete cause of a decoding failure.
template <class T>
std::string reason_of(const Result<T>& result) {
  if (!result.assessment.issues.empty()) return result.assessment.issues.front().reason;
  return "the request cannot be decoded";
}

std::filesystem::path Driver::resolve_root(const std::string& declared) const {
  std::filesystem::path path(declared);
  if (path.is_absolute()) return path;
  return base_directory_ / path;
}

json Driver::envelope_header() const {
  json node;
  node["protocol"] = "acsd.contract.response";
  node["major"] = 1;
  node["request_id"] = request_id_;
  node["operation"] = operation_;
  return node;
}

json Driver::failure_body(Code code, const std::string& reason, const std::string& origin) {
  json body = envelope_header();
  json result;
  result["code"] = encode_code(code);
  result["value"] = nullptr;
  Assessment assessment = model::make_assessment(code);
  model::add_issue(assessment, code, reason, origin, "the request cannot be served");
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(Usage{});
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

Result<TypedObject> Driver::argument(const std::string& name) {
  if (!arguments_.is_object() || !arguments_.contains(name)) {
    return model::fail<TypedObject>(Code::invalid_request,
                                    "the argument " + name + " is missing", "contract.decode",
                                    "the request cannot be decoded");
  }
  return decoder_->decode_node(arguments_.at(name));
}

Result<Ref<LinearAction>> Driver::linear_action_argument(const std::string& name) {
  Result<TypedObject> node = argument(name);
  if (!ok(node)) {
    return model::fail<Ref<LinearAction>>(node.code, reason_of(node),
                                          "contract.decode", "the request cannot be decoded");
  }
  if (val(node).type != "LinearAction") {
    return model::fail<Ref<LinearAction>>(Code::invalid_request,
                                          "the argument " + name + " is not an action",
                                          "contract.decode", "the request cannot be decoded");
  }
  return model::ok_result(std::static_pointer_cast<const LinearAction>(val(node).value));
}

Result<ArrayRef> Driver::array_argument(const std::string& name) {
  Result<TypedObject> node = argument(name);
  if (!ok(node)) {
    return model::fail<ArrayRef>(node.code, reason_of(node),
                                 "contract.decode", "the request cannot be decoded");
  }
  if (val(node).type != "Array") {
    return model::fail<ArrayRef>(Code::invalid_request, "the argument " + name + " is not an array",
                                 "contract.decode", "the request cannot be decoded");
  }
  return model::ok_result(std::static_pointer_cast<const Array>(val(node).value));
}

Result<Ref<CovarianceAction>> Driver::covariance_argument(const std::string& name) {
  Result<TypedObject> node = argument(name);
  if (!ok(node)) {
    return model::fail<Ref<CovarianceAction>>(node.code, reason_of(node),
                                              "contract.decode", "the request cannot be decoded");
  }
  if (val(node).type != "CovarianceAction") {
    return model::fail<Ref<CovarianceAction>>(Code::invalid_request,
                                              "the argument " + name + " is not a covariance",
                                              "contract.decode", "the request cannot be decoded");
  }
  return model::ok_result(std::static_pointer_cast<const CovarianceAction>(val(node).value));
}

void Driver::write_result(const json& result) { result_ = result; }

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

json Driver::run_to_fits() {
  Result<TypedObject> node = argument("point");
  if (!ok(node)) {
    return failure_body(node.code, "the point argument cannot be decoded", "contract.decode");
  }
  FitsPixel1 fitted{};
  if (val(node).type == "PixelCenter0") {
    const PixelCenter0* point = static_cast<const PixelCenter0*>(val(node).value.get());
    fitted = to_fits(*point);
  } else if (val(node).type == "PixelEdge0") {
    const PixelEdge0* point = static_cast<const PixelEdge0*>(val(node).value.get());
    fitted = to_fits(*point);
  } else {
    return failure_body(Code::invalid_request,
                        "to_fits needs a PixelCenter0 or a PixelEdge0 point", "contract.decode");
  }
  json body = envelope_header();
  json result;
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "FitsPixel1"}, {"data", json{{"x", fitted.x}, {"y", fitted.y}}}};
  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_from_fits(bool center) {
  Result<TypedObject> node = argument("point");
  if (!ok(node)) {
    return failure_body(node.code, "the point argument cannot be decoded", "contract.decode");
  }
  if (val(node).type != "FitsPixel1") {
    return failure_body(Code::invalid_request, "a FitsPixel1 point is required", "contract.decode");
  }
  const FitsPixel1* point = static_cast<const FitsPixel1*>(val(node).value.get());
  json body = envelope_header();
  json result;
  result["code"] = encode_code(Code::ok);
  if (center) {
    const PixelCenter0 converted = from_fits_center(*point);
    result["value"] = json{{"type", "PixelCenter0"},
                            {"data", json{{"x", converted.x}, {"y", converted.y}}}};
  } else {
    const PixelEdge0 converted = from_fits_edge(*point);
    result["value"] = json{{"type", "PixelEdge0"},
                            {"data", json{{"x", converted.x}, {"y", converted.y}}}};
  }
  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_apply(bool transpose) {
  Result<Ref<LinearAction>> action = linear_action_argument("action");
  if (!ok(action)) {
    return failure_body(action.code, reason_of(action), "contract.decode");
  }
  Result<ArrayRef> values = array_argument("values");
  if (!ok(values)) {
    return failure_body(values.code, reason_of(values), "contract.decode");
  }
  Result<ArrayRef> mapped =
      transpose ? transpose_apply(*context_, val(action), val(values))
                : apply(*context_, val(action), val(values));
  if (!ok(mapped)) {
    return failure_body(mapped.code, "the action cannot be applied", "contract.apply");
  }
  Result<std::vector<double>> numbers = read_values(*context_, val(mapped));
  if (!ok(numbers)) {
    return failure_body(numbers.code, "the result cannot be read", "contract.apply");
  }
  json body = envelope_header();
  json result;
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_array_result(*val(mapped), val(numbers));
  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_quadratic_form() {
  Result<Ref<CovarianceAction>> covariance = covariance_argument("covariance");
  if (!ok(covariance)) {
    return failure_body(covariance.code, reason_of(covariance), "contract.decode");
  }
  Result<ArrayRef> functional = array_argument("functional");
  if (!ok(functional)) {
    return failure_body(functional.code, reason_of(functional), "contract.decode");
  }
  Result<Variance> variance = quadratic_form(*context_, val(covariance), val(functional));
  json body = envelope_header();
  json result;
  if (!ok(variance)) {
    result["code"] = encode_code(variance.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(variance.assessment);
    result["usage"] = encode_usage(variance.usage.work ? variance.usage : resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  // The public API returns the variance value without a numeric enclosure.
  // The driver reports the degenerate enclosure of the value it received and
  // states that the numerical bound of the computation is not part of the
  // public result; it never widens or narrows a scientific quantity.
  json data = encode_variance(val(variance));
  Assessment assessment = variance.assessment;
  if (!data.contains("numeric_enclosure") || data.at("numeric_enclosure").is_null()) {
    if (val(variance).value.has_value()) {
      data["numeric_enclosure"] = json{{"lower", *val(variance).value},
                                       {"upper", *val(variance).value}};
      assessment.included.push_back("numeric_enclosure_reported_as_the_returned_point");
      assessment.omitted.push_back("numeric_enclosure_of_the_internal_computation");
    }
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "Variance"}, {"data", data}};
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_covariance_apply() {
  Result<Ref<CovarianceAction>> covariance = covariance_argument("covariance");
  if (!ok(covariance)) {
    return failure_body(covariance.code, reason_of(covariance), "contract.decode");
  }
  Result<ArrayRef> values = array_argument("values");
  if (!ok(values)) {
    return failure_body(values.code, reason_of(values), "contract.decode");
  }
  Result<ArrayRef> mapped = covariance_apply(*context_, val(covariance), val(values));
  if (!ok(mapped)) {
    return failure_body(mapped.code, "the covariance cannot be applied", "contract.apply");
  }
  Result<std::vector<double>> numbers = read_values(*context_, val(mapped));
  if (!ok(numbers)) {
    return failure_body(numbers.code, "the result cannot be read", "contract.apply");
  }
  json body = envelope_header();
  json result;
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_array_result(*val(mapped), val(numbers));
  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  result["assessment"] = encode_assessment(assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_make_covariance() {
  Result<TypedObject> node = argument("model");
  if (!ok(node)) {
    return failure_body(node.code, "the noise model cannot be decoded", "contract.decode");
  }
  if (val(node).type != "NoiseModel") {
    return failure_body(Code::invalid_request, "the argument model is not a noise model",
                        "contract.decode");
  }
  Ref<NoiseModel> model = std::static_pointer_cast<const NoiseModel>(val(node).value);
  Result<Ref<CovarianceAction>> covariance = make_covariance(*context_, model);
  json body = envelope_header();
  json result;
  if (!ok(covariance)) {
    result["code"] = encode_code(covariance.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(covariance.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(val(covariance)->identity.object_id);
  result["assessment"] = encode_assessment(val(covariance)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}


// The footprint provider of a world coordinate model.

// The world coordinate model of a provider argument.
Ref<WcsModel> provider_wcs(const TypedObject& provider) {
  if (provider.type != "FootprintProvider" || !provider.value) return nullptr;
  return std::static_pointer_cast<const WcsModel>(provider.value);
}



// The spectral prediction of a response against a basis.
json Driver::run_integrate_response() {
  Result<TypedObject> response_node = argument("response");
  if (!ok(response_node)) {
    return failure_body(response_node.code, "the response argument cannot be decoded",
                        "contract.decode");
  }
  Result<TypedObject> basis_node = argument("basis");
  if (!ok(basis_node)) {
    return failure_body(basis_node.code, "the basis argument cannot be decoded", "contract.decode");
  }
  if (val(response_node).type != "Response" || val(basis_node).type != "SpectralBasis") {
    return failure_body(Code::invalid_request, "the arguments have the wrong type",
                        "contract.decode");
  }
  Ref<Response> response = std::static_pointer_cast<const Response>(val(response_node).value);
  Ref<SpectralBasis> basis = std::static_pointer_cast<const SpectralBasis>(val(basis_node).value);
  const NumericBudget budget = decode_numeric_budget(request_);
  json body = envelope_header();
  json result;
  Result<Ref<SpectralPrediction>> prediction =
      integrate_response(*context_, response, basis, budget);
  if (!ok(prediction)) {
    result["code"] = encode_code(prediction.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(prediction.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  const Id hash = model::sha256_hex("acsd.SpectralPrediction:" + response->identity.object_id);
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(hash);
  result["assessment"] = encode_assessment(val(prediction)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  decoder_->publish("SpectralPrediction", hash, json{{"basis", json{{"$ref", basis->identity.object_id}}},
                                                     {"response", json{{"$ref", response->identity.object_id}}},
                                                     {"support_nm", json{{"lower", encode_f64(val(prediction)->support_nm.lower)},
                                                                        {"upper", encode_f64(val(prediction)->support_nm.upper)}}}});
  body["result"] = result;
  return body;
}

// The control covariance of a covariance action under a transformation.
json Driver::run_control_covariance() {
  Result<TypedObject> covariance_node = argument("covariance");
  if (!ok(covariance_node)) {
    return failure_body(covariance_node.code, "the covariance argument cannot be decoded",
                        "contract.decode");
  }
  Result<TypedObject> transform_node = argument("T");
  if (!ok(transform_node)) {
    return failure_body(transform_node.code, "the T argument cannot be decoded", "contract.decode");
  }
  if (val(covariance_node).type != "CovarianceAction" ||
      val(transform_node).type != "LinearAction") {
    return failure_body(Code::invalid_request, "the arguments have the wrong type",
                        "contract.decode");
  }
  Ref<CovarianceAction> covariance =
      std::static_pointer_cast<const CovarianceAction>(val(covariance_node).value);
  Ref<LinearAction> transform = std::static_pointer_cast<const LinearAction>(val(transform_node).value);
  json body = envelope_header();
  json result;
  Result<Ref<CovarianceAction>> control = control_covariance(*context_, covariance, transform);
  if (!ok(control)) {
    result["code"] = encode_code(control.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(control.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  const Id hash = model::sha256_hex("acsd.CovarianceAction:control:" +
                                    covariance->identity.object_id);
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(hash);
  result["assessment"] = encode_assessment(val(control)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  decoder_->publish("CovarianceAction", hash, json{{"model", json{{"$ref", covariance->identity.object_id}}}});
  body["result"] = result;
  return body;
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


// The location of a product: the directory and the manifest hash.
ProductLocation decode_location(const json& node) {
  ProductLocation location;
  if (node.is_object()) {
    if (node.contains("directory") && node.at("directory").is_string()) {
      location.directory = node.at("directory").get<std::string>();
    }
    if (node.contains("manifest_hash") && node.at("manifest_hash").is_string()) {
      location.manifest_hash = node.at("manifest_hash").get<std::string>();
    }
  }
  return location;
}

// Opens a published product.
json Driver::run_open_product() {
  const json& node = arguments_.contains("location") ? arguments_.at("location") : json::object();
  ProductLocation location = decode_location(node);
  json body = envelope_header();
  json result;
  Result<Ref<ProductProvider>> product = open_product(*context_, location);
  if (!ok(product)) {
    result["code"] = encode_code(product.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(product.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  Ref<ProductGraph> graph = val(product)->graph();
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"directory", location.directory},
                          {"manifest_hash", location.manifest_hash}};
  result["assessment"] = encode_assessment(graph ? graph->assessment : model::make_assessment(Code::ok));
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  // The graph and its signal are published: the next request takes them as
  // the input objects.
  if (graph) {
    decoder_->publish("ProductGraph", graph->identity.object_id,
                      json{{"stage", graph->stage == ProductGraph::Stage::normalize ? "normalize"
                                                  : graph->stage == ProductGraph::Stage::mosaic
                                                      ? "mosaic"
                                                      : "export_image"},
                           {"profile", graph->profile},
                           {"inputs", graph->inputs}});
    if (graph->signal) {
      decoder_->publish("SignalEstimate", graph->signal->identity.object_id,
                        encode_signal_estimate(*graph->signal));
      if (graph->signal->support) {
        decoder_->publish("SupportMap", graph->signal->support->identity.object_id,
                          encode_support_map(*graph->signal->support));
      }
    }
  }
  body["result"] = result;
  return body;
}

// Builds the projection of a product onto a target.
json Driver::run_build_export() {
  const json& location_node = arguments_.contains("product") ? arguments_.at("product")
                                                              : json::object();
  ProductLocation location = decode_location(location_node);
  Result<TypedObject> request_node = argument("request");
  if (!ok(request_node)) {
    return failure_body(request_node.code, "the export request cannot be decoded",
                        "contract.decode");
  }
  if (val(request_node).type != "ExportRequest") {
    return failure_body(Code::invalid_request, "the argument request is not an ExportRequest",
                        "contract.decode");
  }
  Ref<ExportRequest> request = std::static_pointer_cast<const ExportRequest>(val(request_node).value);
  json body = envelope_header();
  json result;
  Result<Ref<ProductProvider>> product = open_product(*context_, location);
  if (!ok(product)) {
    result["code"] = encode_code(product.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(product.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  Result<ExportResult> export_result = build_export(*context_, *val(product), *request);
  if (!ok(export_result)) {
    result["code"] = encode_code(export_result.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(export_result.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  const ExportResult& image = val(export_result);
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"quantity", request->quantity == ExportRequest::Quantity::supported_mean
                                          ? "supported_mean"
                                          : request->quantity == ExportRequest::Quantity::supported_flux
                                              ? "supported_flux"
                                              : "extrapolated_full_flux"},
                          {"width", request->target.width},
                          {"height", request->target.height}};
  result["assessment"] = encode_assessment(image.estimate ? image.estimate->assessment
                                                          : model::make_assessment(Code::ok));
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  // The row operator and the estimate are published: the next request
  // serialises them without reprojecting.
  if (image.Q) {
    Result<ActionRecipe> recipe = image.Q->serialize_recipe();
    if (ok(recipe)) {
      decoder_->publish("LinearAction", image.Q->identity.object_id,
                        json{{"recipe", encode_action_recipe(val(recipe))}});
    }
  }
  if (image.estimate) {
    decoder_->publish("SignalEstimate", image.estimate->identity.object_id,
                      encode_signal_estimate(*image.estimate));
    if (image.estimate->support) {
      decoder_->publish("SupportMap", image.estimate->support->identity.object_id,
                        encode_support_map(*image.estimate->support));
    }
  }
  body["result"] = result;
  return body;
}

// Publishes a projected image as a product.
json Driver::run_export_signal() {
  Result<TypedObject> estimate_node = argument("estimate");
  if (!ok(estimate_node)) {
    return failure_body(estimate_node.code, "the estimate cannot be decoded", "contract.decode");
  }
  if (val(estimate_node).type != "SignalEstimate") {
    return failure_body(Code::invalid_request, "the argument estimate is not a SignalEstimate",
                        "contract.decode");
  }
  Ref<SignalEstimate> estimate =
      std::static_pointer_cast<const SignalEstimate>(val(estimate_node).value);
  Result<TypedObject> publish_node = argument("publish");
  if (!ok(publish_node)) {
    return failure_body(publish_node.code, "the publish request cannot be decoded",
                        "contract.decode");
  }
  if (val(publish_node).type != "PublishRequest") {
    return failure_body(Code::invalid_request, "the argument publish is not a PublishRequest",
                        "contract.decode");
  }
  Ref<PublishRequest> publish =
      std::static_pointer_cast<const PublishRequest>(val(publish_node).value);
  ExportResult image;
  image.estimate = estimate;
  json body = envelope_header();
  json result;
  Result<ProductLocation> location = export_signal(*context_, image, *publish);
  if (!ok(location)) {
    result["code"] = encode_code(location.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(location.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"directory", val(location).directory},
                          {"manifest_hash", val(location).manifest_hash}};
  result["assessment"] = encode_assessment(model::make_assessment(Code::ok));
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_make_footprint_provider() {
  Result<TypedObject> wcs_node = argument("wcs");
  if (!ok(wcs_node)) {
    return failure_body(wcs_node.code, "the wcs argument cannot be decoded", "contract.decode");
  }
  if (val(wcs_node).type != "WcsModel") {
    return failure_body(Code::invalid_request, "the argument wcs is not a WcsModel",
                        "contract.decode");
  }
  Ref<WcsModel> wcs = std::static_pointer_cast<const WcsModel>(val(wcs_node).value);
  Result<Ref<FootprintProvider>> provider = make_footprint_provider(*context_, wcs);
  json body = envelope_header();
  json result;
  if (!ok(provider)) {
    result["code"] = encode_code(provider.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(provider.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  const Id hash = model::sha256_hex("acsd.FootprintProvider:" + wcs->identity.object_id);
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(hash);
  result["assessment"] = encode_assessment(val(provider)->model()->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  decoder_->publish("FootprintProvider", hash, encode_footprint_provider(*wcs));
  body["result"] = result;
  return body;
}

// The area of a detector footprint against the provider.
json Driver::run_measure() {
  Result<TypedObject> provider_node = argument("provider");
  if (!ok(provider_node)) {
    return failure_body(provider_node.code, "the provider argument cannot be decoded",
                        "contract.decode");
  }
  Result<TypedObject> source_node = argument("source");
  if (!ok(source_node)) {
    return failure_body(source_node.code, "the source argument cannot be decoded",
                        "contract.decode");
  }
  Ref<DetectorFootprint> source =
      std::static_pointer_cast<const DetectorFootprint>(val(source_node).value);
  const GeometryBudget budget = decode_geometry_budget(request_);
  json body = envelope_header();
  json result;
  // The provider is rebuilt from the world coordinate model it carries.
  Ref<WcsModel> wcs = provider_wcs(val(provider_node));
  if (!wcs) {
    return failure_body(Code::invalid_request, "the provider argument has no world coordinate model",
                        "contract.decode");
  }
  Result<Ref<FootprintProvider>> provider = make_footprint_provider(*context_, wcs);
  if (!ok(provider)) {
    result["code"] = encode_code(provider.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(provider.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  Result<SphericalAreaInterval> area = measure(*context_, *val(provider), source, budget);
  if (!ok(area)) {
    result["code"] = encode_code(area.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(area.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"lower_sr", encode_f64(val(area).lower_sr)},
                          {"upper_sr", encode_f64(val(area).upper_sr)}};
  result["assessment"] = encode_assessment(area.assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

// The overlap of a detector footprint against the leaves of an order.
json Driver::run_compute_overlap() {
  Result<TypedObject> provider_node = argument("provider");
  if (!ok(provider_node)) {
    return failure_body(provider_node.code, "the provider argument cannot be decoded",
                        "contract.decode");
  }
  Result<TypedObject> source_node = argument("source");
  if (!ok(source_node)) {
    return failure_body(source_node.code, "the source argument cannot be decoded",
                        "contract.decode");
  }
  Ref<DetectorFootprint> source =
      std::static_pointer_cast<const DetectorFootprint>(val(source_node).value);
  const U32 order = request_.contains("order") ? static_cast<U32>(request_.at("order").get<U32>())
                                                 : 6u;
  const double pixfrac = request_.contains("pixfrac") ? decode_f64(request_.at("pixfrac")) : 1.0;
  const GeometryBudget budget = decode_geometry_budget(request_);
  json body = envelope_header();
  json result;
  Ref<WcsModel> wcs = provider_wcs(val(provider_node));
  if (!wcs) {
    return failure_body(Code::invalid_request, "the provider argument has no world coordinate model",
                        "contract.decode");
  }
  Result<Ref<FootprintProvider>> provider = make_footprint_provider(*context_, wcs);
  if (!ok(provider)) {
    result["code"] = encode_code(provider.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(provider.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  Result<Ref<Overlaps>> overlaps =
      compute_overlap(*context_, *val(provider), source, order, pixfrac, budget);
  if (!ok(overlaps)) {
    result["code"] = encode_code(overlaps.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(overlaps.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  const Id hash = model::sha256_hex("acsd.Overlaps:" + std::to_string(order));
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(hash);
  result["assessment"] = encode_assessment(val(overlaps)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  decoder_->publish("Overlaps", hash, encode_overlaps(*val(overlaps)));
  body["result"] = result;
  return body;
}

json Driver::run_make_wcs() {
  if (!arguments_.is_object() || !arguments_.contains("header")) {
    return failure_body(Code::invalid_request, "the header argument is missing", "contract.decode");
  }
  const HeaderFields header = decode_header_fields(arguments_.at("header"));
  const json& domain_node = arguments_.at("domain");
  const Box domain = decode_box(domain_node);
  const AstrometricContext astrometric =
      decode_astrometric_context(arguments_.value("astrometric_context", json::object()));
  const std::string detector_frame =
      arguments_.value("detector_frame", std::string("detector"));
  const NumericBudget budget = decode_numeric_budget(request_);
  Result<Ref<WcsModel>> wcs = make_wcs(*context_, header, domain, astrometric, detector_frame, budget);
  json body = envelope_header();
  json result;
  if (!ok(wcs)) {
    result["code"] = encode_code(wcs.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(wcs.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(val(wcs)->identity.object_id);
  result["assessment"] = encode_assessment(val(wcs)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  // The model is published in the object table of the response: the next
  // request takes it as the input object.
  decoder_->publish("WcsModel", val(wcs)->identity.object_id, encode_wcs_model(*val(wcs)));
  body["result"] = result;
  return body;
}

json Driver::run_forward() {
  Result<TypedObject> wcs_node = argument("wcs");
  if (!ok(wcs_node)) {
    return failure_body(wcs_node.code, "the wcs argument cannot be decoded", "contract.decode");
  }
  if (val(wcs_node).type != "WcsModel") {
    return failure_body(Code::invalid_request, "the argument wcs is not a WcsModel",
                        "contract.decode");
  }
  Ref<WcsModel> wcs = std::static_pointer_cast<const WcsModel>(val(wcs_node).value);
  Result<TypedObject> point_node = argument("point");
  if (!ok(point_node)) {
    return failure_body(point_node.code, "the point argument cannot be decoded", "contract.decode");
  }
  const NumericBudget budget = decode_numeric_budget(request_);
  Result<SkyICRS> sky = model::fail<SkyICRS>(Code::invalid_request, "the point type is unknown",
                                             "contract.decode", "the point cannot be forwarded");
  if (val(point_node).type == "PixelCenter0") {
    const PixelCenter0* point = static_cast<const PixelCenter0*>(val(point_node).value.get());
    sky = forward(*context_, wcs, *point, budget);
  } else if (val(point_node).type == "PixelEdge0") {
    const PixelEdge0* point = static_cast<const PixelEdge0*>(val(point_node).value.get());
    sky = forward(*context_, wcs, *point, budget);
  }
  json body = envelope_header();
  json result;
  if (!ok(sky)) {
    result["code"] = encode_code(sky.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(sky.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "SkyICRS"},
                          {"data", json{{"nx", val(sky).nx},
                                        {"ny", val(sky).ny},
                                        {"nz", val(sky).nz}}}};
  result["assessment"] = encode_assessment(sky.assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_inverse() {
  Result<TypedObject> wcs_node = argument("wcs");
  if (!ok(wcs_node)) {
    return failure_body(wcs_node.code, "the wcs argument cannot be decoded", "contract.decode");
  }
  Ref<WcsModel> wcs = std::static_pointer_cast<const WcsModel>(val(wcs_node).value);
  Result<TypedObject> sky_node = argument("sky");
  if (!ok(sky_node)) {
    return failure_body(sky_node.code, "the sky argument cannot be decoded", "contract.decode");
  }
  const SkyICRS sky = *static_cast<const SkyICRS*>(val(sky_node).value.get());
  const NumericBudget budget = decode_numeric_budget(request_);
  Result<InverseResult> inverse_result = inverse(*context_, wcs, sky, budget);
  json body = envelope_header();
  json result;
  if (!ok(inverse_result)) {
    result["code"] = encode_code(inverse_result.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(inverse_result.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  json data;
  data["point"] = json{{"x", val(inverse_result).point.x}, {"y", val(inverse_result).point.y}};
  data["root_box"] = json{{"lower", json::array({val(inverse_result).root_box.lower[0],
                                                 val(inverse_result).root_box.lower[1]})},
                          {"upper", json::array({val(inverse_result).root_box.upper[0],
                                                 val(inverse_result).root_box.upper[1]})}};
  data["polynomial_residual_pixel"] = json{{"lower", val(inverse_result).polynomial_residual_pixel.lower},
                                           {"upper", val(inverse_result).polynomial_residual_pixel.upper}};
  data["sky_residual_rad"] = json{{"lower", val(inverse_result).sky_residual_rad.lower},
                                   {"upper", val(inverse_result).sky_residual_rad.upper}};
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "InverseResult"}, {"data", data}};
  result["assessment"] = encode_assessment(inverse_result.assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_derivative() {
  Result<TypedObject> wcs_node = argument("wcs");
  if (!ok(wcs_node)) {
    return failure_body(wcs_node.code, "the wcs argument cannot be decoded", "contract.decode");
  }
  Ref<WcsModel> wcs = std::static_pointer_cast<const WcsModel>(val(wcs_node).value);
  Result<TypedObject> point_node = argument("point");
  if (!ok(point_node)) {
    return failure_body(point_node.code, "the point argument cannot be decoded", "contract.decode");
  }
  if (val(point_node).type != "PixelCenter0") {
    return failure_body(Code::invalid_request, "a PixelCenter0 point is required",
                        "contract.decode");
  }
  const PixelCenter0* point = static_cast<const PixelCenter0*>(val(point_node).value.get());
  const NumericBudget budget = decode_numeric_budget(request_);
  Result<DerivativeResult> derivative_result = derivative(*context_, wcs, *point, budget);
  json body = envelope_header();
  json result;
  if (!ok(derivative_result)) {
    result["code"] = encode_code(derivative_result.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(derivative_result.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  json data;
  data["jacobian"] = val(derivative_result).jacobian;
  json enclosure = json::array();
  for (const Interval& entry : val(derivative_result).enclosure) {
    enclosure.push_back(json{{"lower", entry.lower}, {"upper", entry.upper}});
  }
  data["enclosure"] = enclosure;
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "DerivativeResult"}, {"data", data}};
  result["assessment"] = encode_assessment(derivative_result.assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_solid_angle_jacobian() {
  Result<TypedObject> wcs_node = argument("wcs");
  if (!ok(wcs_node)) {
    return failure_body(wcs_node.code, "the wcs argument cannot be decoded", "contract.decode");
  }
  Ref<WcsModel> wcs = std::static_pointer_cast<const WcsModel>(val(wcs_node).value);
  Result<TypedObject> domain_node = argument("domain");
  if (!ok(domain_node)) {
    return failure_body(domain_node.code, "the domain argument cannot be decoded",
                        "contract.decode");
  }
  const Box domain = *static_cast<const Box*>(val(domain_node).value.get());
  const NumericBudget budget = decode_numeric_budget(request_);
  Result<Interval> jacobian = solid_angle_jacobian(*context_, wcs, domain, budget);
  json body = envelope_header();
  json result;
  if (!ok(jacobian)) {
    result["code"] = encode_code(jacobian.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(jacobian.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "Interval"},
                          {"data", json{{"lower", val(jacobian).lower},
                                        {"upper", val(jacobian).upper}}}};
  result["assessment"] = encode_assessment(jacobian.assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_storage_copy() {
  const std::string dtype_name = arguments_.value("dtype", std::string("f64"));
  DType dtype = DType::f64;
  if (dtype_name == "u64") dtype = DType::u64;
  else if (dtype_name == "u32") dtype = DType::u32;
  else if (dtype_name == "u16") dtype = DType::u16;
  else if (dtype_name == "u8") dtype = DType::u8;
  else if (dtype_name == "f32") dtype = DType::f32;
  std::vector<std::byte> bytes;
  const json& byte_node = arguments_.value("bytes", json::array());
  if (byte_node.is_array()) {
    for (const json& item : byte_node) {
      bytes.push_back(static_cast<std::byte>(item.get<unsigned int>() & 0xffu));
    }
  }
  std::vector<U64> shape;
  const json& shape_node = arguments_.value("shape", json::array());
  if (shape_node.is_array()) {
    for (const json& item : shape_node) shape.push_back(item.get<U64>());
  }
  const Unit unit = decode_unit(arguments_.value("unit", json::object()));
  const std::string index = arguments_.value("index", std::string("index"));
  Result<ArrayRef> array = context_->storage.copy(*context_, dtype, bytes, shape, unit, index);
  json body = envelope_header();
  json result;
  if (!ok(array)) {
    result["code"] = encode_code(array.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(array.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(val(array)->identity.object_id);
  result["assessment"] = encode_assessment(val(array)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_storage_read() {
  Result<ArrayRef> array = array_argument("array");
  if (!ok(array)) {
    return failure_body(array.code, "the array argument cannot be decoded", "contract.decode");
  }
  const BlockRequest range = decode_block_request(arguments_.value("range", json::object()));
  Result<ArrayView> view = context_->storage.read(*context_, val(array), range);
  json body = envelope_header();
  json result;
  if (!ok(view)) {
    result["code"] = encode_code(view.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(view.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  Result<std::vector<double>> numbers =
      operators::read_f64(*context_, val(array), range.begin, range.count);
  if (!ok(numbers)) {
    return failure_body(numbers.code, "the block cannot be read", "contract.decode");
  }
  json data;
  data["dtype"] = "f64";
  data["begin"] = range.begin;
  data["count"] = range.count;
  data["values"] = val(numbers);
  result["code"] = encode_code(Code::ok);
  result["value"] = json{{"type", "ArrayView"}, {"data", data}};
  result["assessment"] = encode_assessment(model::make_assessment(Code::ok));
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::run_storage_open() {
  const std::string root = arguments_.value("root", std::string("."));
  Result<TypedObject> description_node = argument("description");
  if (!ok(description_node)) {
    return failure_body(description_node.code, "the description cannot be decoded",
                        "contract.decode");
  }
  if (val(description_node).type != "Array") {
    return failure_body(Code::invalid_request, "the description is not an array",
                        "contract.decode");
  }
  const Array* description = static_cast<const Array*>(val(description_node).value.get());
  std::vector<BlockDescriptor> descriptors;
  const json& blocks_node = arguments_.value("blocks", json::array());
  if (blocks_node.is_array()) {
    for (const json& item : blocks_node) {
      BlockDescriptor descriptor;
      descriptor.id = item.value("id", std::string());
      descriptor.path = item.value("path", std::string());
      const std::string dtype = item.value("dtype", std::string("f64"));
      descriptor.dtype = dtype == "u64" ? DType::u64
                          : dtype == "u32" ? DType::u32
                          : dtype == "u16" ? DType::u16
                          : dtype == "u8"  ? DType::u8
                          : dtype == "f32" ? DType::f32
                                           : DType::f64;
      const json& shape = item.value("shape", json::array());
      if (shape.is_array()) {
        for (const json& extent : shape) descriptor.shape.push_back(extent.get<U64>());
      }
      descriptor.index_id = item.value("index_id", std::string());
      descriptor.unit = decode_unit(item.value("unit", json::object()));
      descriptor.begin = item.value("begin", U64{0});
      descriptor.count = item.value("count", U64{0});
      descriptor.codec = item.value("codec", std::string("raw")) == "zstd"
                             ? BlockDescriptor::Codec::zstd
                             : BlockDescriptor::Codec::raw;
      descriptor.raw_bytes = item.value("raw_bytes", U64{0});
      descriptor.stored_bytes = item.value("stored_bytes", U64{0});
      descriptor.raw_sha256 = item.value("raw_sha256", std::string());
      descriptors.push_back(descriptor);
    }
  }
  Result<ArrayRef> array = context_->storage.open(*context_, root, *description, descriptors);
  json body = envelope_header();
  json result;
  if (!ok(array)) {
    result["code"] = encode_code(array.code);
    result["value"] = nullptr;
    result["assessment"] = encode_assessment(array.assessment);
    result["usage"] = encode_usage(resources_->usage());
    result["dependencies"] = json::array();
    body["result"] = result;
    return body;
  }
  result["code"] = encode_code(Code::ok);
  result["value"] = encode_ref_value(val(array)->identity.object_id);
  result["assessment"] = encode_assessment(val(array)->assessment);
  result["usage"] = encode_usage(resources_->usage());
  result["dependencies"] = json::array();
  body["result"] = result;
  return body;
}

json Driver::dispatch() {
  if (operation_ == "to_fits") return run_to_fits();
  if (operation_ == "from_fits_center") return run_from_fits(true);
  if (operation_ == "from_fits_edge") return run_from_fits(false);
  if (operation_ == "apply") return run_apply(false);
  if (operation_ == "transpose_apply") return run_apply(true);
  if (operation_ == "quadratic_form") return run_quadratic_form();
  if (operation_ == "covariance_apply") return run_covariance_apply();
  if (operation_ == "make_covariance") return run_make_covariance();
  if (operation_ == "make_wcs") return run_make_wcs();
  if (operation_ == "make_footprint_provider") return run_make_footprint_provider();
  if (operation_ == "open_product") return run_open_product();
  if (operation_ == "build_export") return run_build_export();
  if (operation_ == "export_signal") return run_export_signal();
  if (operation_ == "integrate_response") return run_integrate_response();
  if (operation_ == "control_covariance") return run_control_covariance();
  if (operation_ == "measure") return run_measure();
  if (operation_ == "compute_overlap") return run_compute_overlap();
  if (operation_ == "forward") return run_forward();
  if (operation_ == "inverse") return run_inverse();
  if (operation_ == "derivative") return run_derivative();
  if (operation_ == "solid_angle_jacobian") return run_solid_angle_jacobian();
  if (operation_ == "storage.copy") return run_storage_copy();
  if (operation_ == "storage.read") return run_storage_read();
  if (operation_ == "storage.open") return run_storage_open();
  return failure_body(Code::unsupported, "the operation " + operation_ + " is not implemented",
                      "contract.dispatch");
}

int Driver::run(const std::filesystem::path& response_path) {
  request_id_ = envelope_.value("request_id", std::string());
  operation_ = envelope_.value("operation", std::string());
  arguments_ = envelope_.value("arguments", json::object());
  request_ = envelope_.value("request", json::object());
  // The input and output roots are resolved against the directory of the
  // request file, never against the process working directory.
  const std::string declared_output = envelope_.value("output_root", std::string("."));
  const std::string declared_input = envelope_.value("input_root", std::string("."));
  output_root_ = resolve_root(declared_output);
  input_root_ = resolve_root(declared_input);
  limits_ = decode_limits(envelope_.value("resources", json::object()));

  resources_ = std::make_unique<resources::CpuResources>(limits_);
  storage_ = std::make_unique<storage::LocalStorage>();
  cancellation_ = std::make_unique<NeverCancelled>();
  context_ = std::make_unique<Context>(Context{*resources_, *cancellation_, *storage_});

  // The input blocks are declared by the envelope; their paths are resolved
  // against the input root, never against the process working directory.
  const json& blocks_node = envelope_.value("blocks", json::array());
  if (blocks_node.is_array()) {
    for (const json& item : blocks_node) {
      BlockDescriptor descriptor;
      descriptor.id = item.value("id", std::string());
      descriptor.path = item.value("path", std::string());
      const std::string dtype = item.value("dtype", std::string("f64"));
      descriptor.dtype = dtype == "u64" ? DType::u64
                          : dtype == "u32" ? DType::u32
                          : dtype == "u16" ? DType::u16
                          : dtype == "u8"  ? DType::u8
                          : dtype == "f32" ? DType::f32
                                           : DType::f64;
      const json& shape = item.value("shape", json::array());
      if (shape.is_array()) {
        for (const json& extent : shape) descriptor.shape.push_back(extent.get<U64>());
      }
      descriptor.index_id = item.value("index_id", std::string());
      descriptor.unit = decode_unit(item.value("unit", json::object()));
      descriptor.begin = item.value("begin", U64{0});
      descriptor.count = item.value("count", U64{0});
      descriptor.codec = item.value("codec", std::string("raw")) == "zstd"
                             ? BlockDescriptor::Codec::zstd
                             : BlockDescriptor::Codec::raw;
      descriptor.raw_bytes = item.value("raw_bytes", U64{0});
      descriptor.stored_bytes = item.value("stored_bytes", U64{0});
      descriptor.raw_sha256 = item.value("raw_sha256", std::string());
      input_blocks_.push_back(descriptor);
    }
  }

  decoder_ = std::make_unique<DecodeContext>(*context_, envelope_.value("input_objects", json::object()),
                                             input_root_, input_blocks_);

  // The driver charges the decoding and the dispatch to the shared ledger.
  const U64 decoded_objects = envelope_.value("input_objects", json::object()).size();
  if (decoded_objects > 0) {
    Result<U64> charged = resources_->consume_work(decoded_objects + 1);
    (void)charged;
  }

  json body = dispatch();

  // Input hashes: the content id of every decoded input object.
  json input_hashes = json::object();
  for (const auto& entry : decoder_->decoded()) {
    if (!entry.second.hash.empty()) input_hashes[entry.first] = entry.second.hash;
  }
  body["input_hashes"] = input_hashes;
  body["resolved_request_hash"] = canonical_hash(request_);
  body["build"] = build_info();
  json algorithms = json::array();
  algorithms.push_back(algorithm_entry("acsd-contract", "1"));
  algorithms.push_back(algorithm_entry(operation_, "1"));
  body["algorithm_set"] = algorithms;
  body["arithmetic"] = encode_arithmetic(Arithmetic{Arithmetic::Mode::outward, 64, 256, true});
  body["objects"] = decoder_->published();
  json blocks = json::array();
  for (const BlockDescriptor& descriptor : output_blocks_) {
    json entry;
    entry["id"] = descriptor.id;
    entry["path"] = descriptor.path;
    entry["dtype"] = descriptor.dtype == DType::u64 ? "u64"
                       : descriptor.dtype == DType::u32 ? "u32"
                       : descriptor.dtype == DType::u16 ? "u16"
                       : descriptor.dtype == DType::u8  ? "u8"
                       : descriptor.dtype == DType::f32 ? "f32"
                                                        : "f64";
    entry["shape"] = descriptor.shape;
    entry["index_id"] = descriptor.index_id;
    entry["unit"] = encode_unit(descriptor.unit);
    entry["begin"] = descriptor.begin;
    entry["count"] = descriptor.count;
    entry["codec"] = descriptor.codec == BlockDescriptor::Codec::zstd ? "zstd" : "raw";
    entry["raw_bytes"] = descriptor.raw_bytes;
    entry["stored_bytes"] = descriptor.stored_bytes;
    entry["raw_sha256"] = descriptor.raw_sha256;
    blocks.push_back(entry);
  }
  body["blocks"] = blocks;
  body["output_root"] = output_root_;
  json resources;
  const Usage usage = resources_->usage();
  resources["work"] = usage.work;
  resources["bytes"] = limits_.bytes;
  resources["threads"] = limits_.threads;
  resources["read_bytes"] = usage.read_bytes;
  resources["written_bytes"] = usage.written_bytes;
  resources["spill_bytes"] = usage.spill_bytes;
  resources["peak_payload_bytes"] = usage.peak_payload_bytes;
  const Usage final_usage = resources_->usage();
  resources["work"] = final_usage.work;
  resources["peak_payload_bytes"] = final_usage.peak_payload_bytes;
  resources["read_bytes"] = final_usage.read_bytes;
  resources["written_bytes"] = final_usage.written_bytes;
  resources["spill_bytes"] = final_usage.spill_bytes;
  resources["elapsed_seconds"] = final_usage.elapsed_seconds;
  resources["byte_budget"] = limits_.bytes;
  resources["work_budget"] = limits_.work;
  body["resources"] = resources;

  std::error_code ec;
  std::filesystem::create_directories(response_path.parent_path(), ec);
  std::ofstream stream(response_path, std::ios::trunc);
  if (!stream) return 2;
  stream << body.dump(2) << "\n";
  stream.flush();
  if (!stream) return 2;

  // A decoding or syntax failure is reported with exit code 2; a scientific
  // nonconvergence or an unsupported operation is a normal response.
  const std::string code = body["result"]["code"].get<std::string>();
  if (code == "invalid_request" || code == "invalid_model" || code == "missing_dependency" ||
      code == "hash_mismatch" || code == "corrupt_product" || code == "io_error" ||
      code == "identity_conflict" || code == "internal") {
    return 2;
  }
  return 0;
}

}  // namespace

bool operation_supported(const std::string& operation) {
  static const char* kSupported[] = {"to_fits",
                                     "from_fits_center",
                                     "from_fits_edge",
                                     "apply",
                                     "transpose_apply",
                                     "quadratic_form",
                                     "covariance_apply",
                                     "make_covariance",
                                     "make_wcs",
                                     "make_footprint_provider",
                                     "open_product",
                                     "build_export",
                                     "export_signal",
                                     "integrate_response",
                                     "control_covariance",
                                     "measure",
                                     "compute_overlap",
                                     "forward",
                                     "inverse",
                                     "derivative",
                                     "solid_angle_jacobian",
                                     "storage.copy",
                                     "storage.read",
                                     "storage.open"};
  for (const char* name : kSupported) {
    if (operation == name) return true;
  }
  return false;
}

int run_contract(const json& envelope, const std::filesystem::path& response_path,
                 const std::filesystem::path& base_directory, std::string& error) {
  if (!envelope.is_object()) {
    error = "the request envelope is not an object";
    return 2;
  }
  if (envelope.value("protocol", std::string()) != "acsd.contract") {
    error = "the request protocol is not acsd.contract";
    return 2;
  }
  if (envelope.value("major", 0) != 1) {
    error = "the request major version is not supported";
    return 2;
  }
  for (const char* required : {"request_id", "operation", "input_objects", "arguments", "request",
                               "resources", "output_root"}) {
    if (!envelope.contains(required)) {
      error = std::string("the request envelope misses ") + required;
      return 2;
    }
  }
  Driver driver(envelope, base_directory);
  const int code = driver.run(response_path);
  if (code != 0) error = "the request could not be served";
  return code;
}

int run_contract_file(const std::filesystem::path& request_path,
                      const std::filesystem::path& response_path, std::string& error) {
  std::ifstream stream(request_path);
  if (!stream) {
    error = "cannot read " + request_path.string();
    return 2;
  }
  json envelope;
  try {
    stream >> envelope;
  } catch (const std::exception& failure) {
    error = std::string("the request file is not valid JSON: ") + failure.what();
    return 2;
  }
  std::filesystem::path base = request_path.parent_path();
  if (base.empty()) base = std::filesystem::path(".");
  return run_contract(envelope, response_path, base, error);
}

}  // namespace acsd::contract
