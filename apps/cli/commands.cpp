// The three commands of the CLI and the diagnostics.
#include "cli.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include <fitsio.h>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"

namespace acsd::cli {
namespace {

using model::fail;
using model::ok_result;

bool has(const json& node, const char* key) {
  return node.is_object() && node.contains(key) && !node.at(key).is_null();
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

double double_field(const json& node, const char* key, double fallback = 0.0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_number() ? value.get<double>() : fallback;
}

Unit decode_unit(const json& node) {
  Unit unit;
  unit.scale = string_field(node, "scale", "1");
  unit.sr_power = static_cast<std::int32_t>(u64_field(node, "sr_power", 0));
  unit.pixel_power = static_cast<std::int32_t>(u64_field(node, "pixel_power", 0));
  unit.nm_power = static_cast<std::int32_t>(u64_field(node, "nm_power", 0));
  unit.multiplier = double_field(node, "multiplier", 1.0);
  unit.linear_power = static_cast<std::int32_t>(u64_field(node, "linear_power", 0));
  return unit;
}

Arithmetic decode_arithmetic(const json& node) {
  Arithmetic arithmetic;
  arithmetic.mode = string_field(node, "mode", "outward") == "approximate"
                        ? Arithmetic::Mode::approximate
                        : Arithmetic::Mode::outward;
  arithmetic.initial_bits = static_cast<U32>(u64_field(node, "initial_bits", 64));
  arithmetic.max_bits = static_cast<U32>(u64_field(node, "max_bits", 256));
  arithmetic.allow_target_fallback = has(node, "allow_target_fallback") &&
                                     node.at("allow_target_fallback").is_boolean() &&
                                     node.at("allow_target_fallback").get<bool>();
  return arithmetic;
}

GeometryBudget decode_budget(const json& node) {
  GeometryBudget budget;
  budget.area_absolute_sr = double_field(node, "area_absolute_sr", 0.0);
  budget.normalizer_min_sr = double_field(node, "normalizer_min_sr", 0.0);
  budget.max_work = u64_field(node, "max_work", 0);
  budget.max_bytes = u64_field(node, "max_bytes", 0);
  budget.max_depth = static_cast<U32>(u64_field(node, "max_depth", 64));
  budget.arithmetic = decode_arithmetic(has(node, "arithmetic") ? node.at("arithmetic") : json::object());
  const json& limits = node.contains("per_leaf_absolute_area") ? node.at("per_leaf_absolute_area")
                                                                : json::array();
  if (limits.is_array()) {
    for (const json& item : limits) {
      LeafAreaLimit limit;
      const json& leaf = item.contains("leaf") ? item.at("leaf") : json::object();
      limit.leaf.order = static_cast<U32>(u64_field(leaf, "order", 0));
      limit.leaf.ipix = u64_field(leaf, "ipix", 0);
      limit.absolute_sr = double_field(item, "absolute_sr", 0.0);
      budget.per_leaf_absolute_area.push_back(limit);
    }
  }
  return budget;
}

Polynomial2D decode_polynomial(const json& node) {
  Polynomial2D polynomial;
  polynomial.order = static_cast<U32>(u64_field(node, "order", 0));
  const json& terms = node.contains("terms") ? node.at("terms") : json::array();
  if (terms.is_array()) {
    for (const json& item : terms) {
      Term2 term;
      term.x_power = static_cast<U32>(u64_field(item, "x_power", 0));
      term.y_power = static_cast<U32>(u64_field(item, "y_power", 0));
      term.coefficient = double_field(item, "coefficient", 0.0);
      polynomial.terms.push_back(term);
    }
  }
  return polynomial;
}

Box decode_box(const json& node) {
  Box box;
  const json& lower = node.contains("lower") ? node.at("lower") : json::array();
  const json& upper = node.contains("upper") ? node.at("upper") : json::array();
  if (lower.is_array() && lower.size() == 2) {
    box.lower = {lower[0].get<double>(), lower[1].get<double>()};
  }
  if (upper.is_array() && upper.size() == 2) {
    box.upper = {upper[0].get<double>(), upper[1].get<double>()};
  }
  return box;
}

AstrometricContext decode_astrometric(const json& node) {
  AstrometricContext context;
  context.epoch_julian_year = double_field(node, "epoch_julian_year", 0.0);
  context.observation_jd_tdb = double_field(node, "observation_jd_tdb", 0.0);
  const json& omitted = node.contains("omitted") ? node.at("omitted") : json::array();
  if (omitted.is_array()) {
    for (const json& item : omitted) {
      if (item.is_string()) context.omitted.push_back(item.get<std::string>());
    }
  }
  return context;
}

// The WCS model of the export target.
Ref<WcsModel> decode_wcs(const json& node) {
  auto wcs = std::make_shared<WcsModel>();
  const json& crpix = node.contains("crpix_fits1") ? node.at("crpix_fits1") : json::array();
  if (crpix.is_array() && crpix.size() == 2) {
    wcs->crpix_fits1 = {crpix[0].get<double>(), crpix[1].get<double>()};
  }
  const json& cd = node.contains("cd_rad_per_pixel") ? node.at("cd_rad_per_pixel") : json::array();
  if (cd.is_array() && cd.size() == 4) {
    wcs->cd_rad_per_pixel = {cd[0].get<double>(), cd[1].get<double>(), cd[2].get<double>(),
                             cd[3].get<double>()};
  }
  const json& c = node.contains("c") ? node.at("c") : json::array();
  if (c.is_array() && c.size() == 3) {
    wcs->c = {c[0].get<double>(), c[1].get<double>(), c[2].get<double>()};
  }
  const json& e = node.contains("e") ? node.at("e") : json::array();
  if (e.is_array() && e.size() == 3) {
    wcs->e = {e[0].get<double>(), e[1].get<double>(), e[2].get<double>()};
  }
  const json& north = node.contains("north") ? node.at("north") : json::array();
  if (north.is_array() && north.size() == 3) {
    wcs->north = {north[0].get<double>(), north[1].get<double>(), north[2].get<double>()};
  }
  if (has(node, "sip_a")) wcs->sip_a = decode_polynomial(node.at("sip_a"));
  if (has(node, "sip_b")) wcs->sip_b = decode_polynomial(node.at("sip_b"));
  if (has(node, "domain")) wcs->domain = decode_box(node.at("domain"));
  if (has(node, "epoch_frame_observer")) {
    wcs->epoch_frame_observer = decode_astrometric(node.at("epoch_frame_observer"));
  }
  wcs->detector_frame = string_field(node, "detector_frame", "acsd.target");
  wcs->identity = model::make_identity("acsd.WcsModel", 2, 0,
                                        model::sha256_hex("acsd.cli.target.wcs"), "acsd");
  wcs->assessment = model::make_assessment(Code::ok);
  wcs->assessment.complete = true;
  return std::shared_ptr<const WcsModel>(wcs);
}

ExportRequest decode_export_request(const json& node) {
  ExportRequest request;
  const json& target = node.contains("target") ? node.at("target") : json::object();
  const json& model = target.contains("model") ? target.at("model") : json::object();
  request.target.model = decode_wcs(model);
  request.target.width = u64_field(target, "width", 0);
  request.target.height = u64_field(target, "height", 0);
  const std::string quantity = string_field(node, "quantity", "supported_mean");
  if (quantity == "supported_flux") {
    request.quantity = ExportRequest::Quantity::supported_flux;
  } else if (quantity == "extrapolated_full_flux") {
    request.quantity = ExportRequest::Quantity::extrapolated_full_flux;
  } else {
    request.quantity = ExportRequest::Quantity::supported_mean;
  }
  const std::string policy = string_field(node, "support_policy", "dyadic_cells");
  if (policy == "primitives") {
    request.support_policy = SupportMap::Kind::primitives;
  } else if (policy == "full_leaf_approx") {
    request.support_policy = SupportMap::Kind::full_leaf_approx;
  } else if (policy == "effective_area_approx") {
    request.support_policy = SupportMap::Kind::effective_area_approx;
  } else {
    request.support_policy = SupportMap::Kind::dyadic_cells;
  }
  request.assume_uniform_uncovered = has(node, "assume_uniform_uncovered") &&
                                     node.at("assume_uniform_uncovered").is_boolean() &&
                                     node.at("assume_uniform_uncovered").get<bool>();
  const std::string dtype = string_field(node, "output_dtype", "f64");
  request.output_dtype = dtype == "f32" ? DType::f32 : DType::f64;
  if (has(node, "budget")) request.budget = decode_budget(node.at("budget"));
  return request;
}

// The exit code of a failed call.
int exit_code_of(Code code) {
  switch (code) {
    case Code::ok: return kSuccess;
    case Code::invalid_request:
    case Code::invalid_model:
    case Code::duplicate_source:
    case Code::identity_conflict: return kInvalidInput;
    case Code::unsupported:
    case Code::unavailable:
    case Code::not_identifiable:
    case Code::unsolved:
    case Code::no_support:
    case Code::support_unresolved: return kUnsupported;
    case Code::cancelled: return kCancelled;
    case Code::resource_exhausted:
    case Code::precision_exhausted:
    case Code::nonconverged:
    case Code::io_error:
    case Code::corrupt_product:
    case Code::hash_mismatch:
    case Code::missing_dependency:
    case Code::invalid_covariance:
    case Code::internal: return kFailure;
    case Code::domain:
    case Code::geometry_solve_unresolved:
    case Code::selection_unresolved: return kPartial;
  }
  return kFailure;
}

// The run summary of one command.
json make_summary(U64 total, U64 published, U64 unsolved, U64 failed, U64 cancelled,
                  const json& frames) {
  json summary;
  summary["inputs_total"] = total;
  summary["published"] = published;
  summary["unsolved"] = unsolved;
  summary["failed"] = failed;
  summary["cancelled"] = cancelled;
  summary["frames"] = frames;
  return summary;
}

std::string hex_digest(const std::string& text) {
  return model::sha256_hex(text);
}

}  // namespace

// ---------------------------------------------------------------------------
// export
// ---------------------------------------------------------------------------

int run_export(Config& config, EventWriter& events) {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(1, 0, 0, 1, 0, json::array()));
    return kInvalidInput;
  }
  const json& projection = val(scientific).request.at("projection");
  const ExportRequest request = decode_export_request(projection);
  if (request.target.width == 0 || request.target.height == 0) {
    events.issue("invalid_request", "the export target is empty");
    events.run_finished(kInvalidInput, make_summary(1, 0, 0, 1, 0, json::array()));
    return kInvalidInput;
  }

  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  CancellationSource cancellation;
  Context context{resources, cancellation, storage};

  PublishRequest publish;
  publish.destination = (config.base / config.output).string();
  publish.signal_dtype = config.signal_dtype == "f32" ? DType::f32 : DType::f64;
  publish.profile = config.product_profile;
  publish.tile_log2_width = config.tile_log2_width;
  publish.durable = config.durable;

  ExportWorkflowRequest workflow;
  workflow.input.directory = (config.base / config.inputs[0].path).string();
  workflow.input.manifest_hash = config.inputs[0].expected_hash;
  workflow.projection = request;
  workflow.output = publish;

  events.frame_started("export", 1);
  Result<ProductLocation> location = export_image(context, workflow);
  if (!ok(location)) {
    const std::string reason = location.assessment.issues.empty()
                                   ? "the export failed"
                                   : location.assessment.issues.front().reason;
    events.frame_finished("export", "failed", 0, 1, reason);
    events.run_finished(exit_code_of(location.code), make_summary(1, 0, 0, 1, 0, json::array()));
    return exit_code_of(location.code);
  }
  events.product_published("export", val(location).directory);
  events.frame_finished("export", "ok", 1, 1, "the export was published");
  events.run_finished(kSuccess, make_summary(1, 1, 0, 0, 0, json::array()));
  return kSuccess;
}

// ---------------------------------------------------------------------------
// normalize
// ---------------------------------------------------------------------------

int run_normalize(Config& config, EventWriter& events) {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }
  // The frame options are joined with the configured inputs by the frame
  // identifier; a duplicate or unknown frame is a configuration error.
  std::map<std::string, const json*> options;
  for (const json& item : val(scientific).frame_options) {
    const std::string frame = string_field(item, "frame");
    if (frame.empty()) {
      events.issue("invalid_request", "a frame option has no frame identifier");
      events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                      json::array()));
      return kInvalidInput;
    }
    if (options.count(frame) != 0) {
      events.issue("invalid_request", "the frame " + frame + " has two option sets");
      events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                      json::array()));
      return kInvalidInput;
    }
    options[frame] = &item;
  }
  for (const InputEntry& input : config.inputs) {
    if (options.count(input.frame_id) == 0) continue;
  }

  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  CancellationSource cancellation;
  Context context{resources, cancellation, storage};

  // The catalogue of the run.
  const std::filesystem::path catalogue_path = config.base / config.catalogue;
  std::ifstream catalogue_stream(catalogue_path, std::ios::binary);
  if (!catalogue_stream) {
    events.issue("invalid_request", "the catalogue cannot be read: " + catalogue_path.string());
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }
  events.issue("unsupported",
               "the normalize command needs the catalogue provider and the observation pipeline of "
               "the science modules");
  events.run_finished(kUnsupported,
                       make_summary(config.inputs.size(), 0, config.inputs.size(), 0, 0, json::array()));
  return kUnsupported;
}

// ---------------------------------------------------------------------------
// mosaic
// ---------------------------------------------------------------------------

int run_mosaic(Config& config, EventWriter& events) {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }
  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  CancellationSource cancellation;
  Context context{resources, cancellation, storage};

  PublishRequest publish;
  publish.destination = (config.base / config.output).string();
  publish.signal_dtype = config.signal_dtype == "f32" ? DType::f32 : DType::f64;
  publish.profile = config.product_profile;
  publish.tile_log2_width = config.tile_log2_width;
  publish.durable = config.durable;

  MosaicRequest request;
  for (const InputEntry& input : config.inputs) {
    ProductLocation location;
    location.directory = (config.base / input.path).string();
    location.manifest_hash = input.expected_hash;
    request.inputs.push_back(location);
  }
  request.output = publish;

  events.frame_started("mosaic", config.inputs.size());
  Result<ProductLocation> location = mosaic(context, request);
  if (!ok(location)) {
    const std::string reason = location.assessment.issues.empty()
                                   ? "the mosaic failed"
                                   : location.assessment.issues.front().reason;
    events.frame_finished("mosaic", "failed", 0, config.inputs.size(), reason);
    events.run_finished(exit_code_of(location.code),
                        make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0, json::array()));
    return exit_code_of(location.code);
  }
  events.product_published("mosaic", val(location).directory);
  events.frame_finished("mosaic", "ok", config.inputs.size(), config.inputs.size(),
                        "the mosaic was published");
  events.run_finished(kSuccess, make_summary(config.inputs.size(), 1, 0, 0, 0, json::array()));
  return kSuccess;
}

// ---------------------------------------------------------------------------
// doctor and templates
// ---------------------------------------------------------------------------

int run_doctor(const Config* config, EventWriter& events) {
  json report;
  report["build"] = "acsd 2.0.0";
  report["protocol"] = "acsd.events/2";
  report["config_schema"] = "acsd.config/2";
  report["scientific_schema"] = "acsd.cli-science/2";
  report["product_format"] = "acsd.product/2";
  report["cfitsio"] = "4.6.2";
  report["json"] = "nlohmann_json";
  if (config) {
    report["command"] = config->command;
    report["inputs"] = config->inputs.size();
    report["product_profile"] = config->product_profile;
  }
  std::cout << report.dump(2) << "\n";
  events.run_finished(kSuccess, make_summary(config ? config->inputs.size() : 0, 0, 0, 0, 0, json::array()));
  return kSuccess;
}

void print_template(const std::string& command) {
  if (command == "normalize") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "normalize",
  "inputs": [
    {"path": "frame.fits", "frame_id": "frame-01", "response": "response.json"}
  ],
  "catalogue": "catalogue.json",
  "scientific": "normalize.scientific.json",
  "output": "products",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full",
  "publication": {"signal_dtype": "f64", "tile_log2_width": 9, "durable": true}
}
)";
    return;
  }
  if (command == "mosaic") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "mosaic",
  "inputs": [
    {"path": "products/frame-01", "expected_hash": "sha256:0000000000000000000000000000000000000000000000000000000000000000"}
  ],
  "scientific": "mosaic.scientific.json",
  "output": "products/mosaic",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full"
}
)";
    return;
  }
  if (command == "export") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "export",
  "inputs": [
    {"path": "products/frame-01", "expected_hash": "sha256:0000000000000000000000000000000000000000000000000000000000000000"}
  ],
  "scientific": "export.scientific.json",
  "output": "products/export",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full"
}
)";
    return;
  }
  std::cerr << "acsd: unknown template " << command << "\n";
}

}  // namespace acsd::cli
