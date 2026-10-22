// The configuration of the CLI: the config file and the scientific file.
#include "cli.hpp"

#include <fstream>
#include <sstream>

#include "model/result.hpp"
#include "model/units.hpp"

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

std::string read_text(const std::filesystem::path& path, std::string& error) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot read " + path.string();
    return {};
  }
  std::ostringstream text;
  text << stream.rdbuf();
  return text.str();
}

}  // namespace

Result<Config> load_config(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return fail<Config>(Code::invalid_request, "the configuration file does not exist: " + path.string(),
                        "cli.config", "the command cannot run");
  }
  std::string error;
  const std::string text = read_text(path, error);
  if (!error.empty()) {
    return fail<Config>(Code::invalid_request, error, "cli.config", "the command cannot run");
  }
  json document;
  try {
    document = json::parse(text);
  } catch (const std::exception& failure) {
    return fail<Config>(Code::invalid_request,
                        std::string("the configuration is not valid JSON: ") + failure.what(),
                        "cli.config", "the command cannot run");
  }
  if (!document.is_object()) {
    return fail<Config>(Code::invalid_request, "the configuration is not an object", "cli.config",
                        "the command cannot run");
  }
  Config config;
  config.base = path.parent_path();
  if (config.base.empty()) config.base = std::filesystem::path(".");
  config.schema = string_field(document, "schema");
  if (config.schema != "acsd.config/2") {
    return fail<Config>(Code::invalid_request, "the configuration schema is not acsd.config/2",
                        "cli.config", "the command cannot run");
  }
  config.command = string_field(document, "command");
  if (config.command != "normalize" && config.command != "mosaic" && config.command != "export") {
    return fail<Config>(Code::invalid_request, "the command is unknown", "cli.config",
                        "the command cannot run");
  }
  const json& inputs = document.at("inputs");
  if (!inputs.is_array() || inputs.empty()) {
    return fail<Config>(Code::invalid_request, "the configuration has no input", "cli.config",
                        "the command cannot run");
  }
  for (const json& item : inputs) {
    InputEntry entry;
    entry.path = string_field(item, "path");
    if (entry.path.empty()) {
      return fail<Config>(Code::invalid_request, "an input has no path", "cli.config",
                          "the command cannot run");
    }
    entry.expected_hash = string_field(item, "expected_hash");
    entry.frame_id = string_field(item, "frame_id");
    entry.calibration = string_field(item, "calibration");
    entry.response = string_field(item, "response");
    if (config.command == "normalize" && (entry.frame_id.empty() || entry.response.empty())) {
      return fail<Config>(Code::invalid_request,
                          "a normalize input needs a frame identifier and a response", "cli.config",
                          "the command cannot run");
    }
    if (config.command != "normalize" && entry.expected_hash.empty()) {
      return fail<Config>(Code::invalid_request, "an input needs the expected hash", "cli.config",
                          "the command cannot run");
    }
    config.inputs.push_back(entry);
  }
  if (config.command == "export" && config.inputs.size() != 1) {
    return fail<Config>(Code::invalid_request, "the export command takes exactly one input",
                        "cli.config", "the command cannot run");
  }
  config.scientific = string_field(document, "scientific");
  if (config.scientific.empty()) {
    return fail<Config>(Code::invalid_request, "the configuration has no scientific file",
                        "cli.config", "the command cannot run");
  }
  config.output = string_field(document, "output");
  if (config.output.empty()) {
    return fail<Config>(Code::invalid_request, "the configuration has no output", "cli.config",
                        "the command cannot run");
  }
  config.catalogue = string_field(document, "catalogue");
  if (config.command == "normalize" && config.catalogue.empty()) {
    return fail<Config>(Code::invalid_request, "the normalize command needs a catalogue",
                        "cli.config", "the command cannot run");
  }
  const json& resources = document.at("resources");
  config.resources.bytes = u64_field(resources, "bytes", 0);
  config.resources.work = u64_field(resources, "work", 0);
  config.resources.spill_bytes = u64_field(resources, "spill_bytes", 0);
  config.resources.threads = static_cast<U32>(u64_field(resources, "threads", 1));
  config.resources.max_depth = static_cast<U32>(u64_field(resources, "max_depth", 64));
  config.resources.max_precision_bits =
      static_cast<U32>(u64_field(resources, "max_precision_bits", 64));
  config.resources.scratch_directory = string_field(resources, "scratch_directory", ".");
  if (config.resources.bytes == 0) {
    return fail<Config>(Code::invalid_request, "the byte budget is zero", "cli.config",
                        "the command cannot run");
  }
  config.product_profile = string_field(document, "product_profile", "full");
  if (config.product_profile != "full" && config.product_profile != "signal_only" &&
      config.product_profile != "external_inputs") {
    return fail<Config>(Code::invalid_request, "the product profile is unknown", "cli.config",
                        "the command cannot run");
  }
  if (has(document, "publication")) {
    const json& publication = document.at("publication");
    config.signal_dtype = string_field(publication, "signal_dtype", "f64");
    config.tile_log2_width = static_cast<U32>(u64_field(publication, "tile_log2_width", 9));
    if (has(publication, "durable")) {
      config.durable = publication.at("durable").is_boolean() && publication.at("durable").get<bool>();
    }
  }
  return ok_result(config);
}

Result<ScientificFile> load_scientific(const Config& config) {
  const std::filesystem::path path = config.base / config.scientific;
  std::string error;
  const std::string text = read_text(path, error);
  if (!error.empty()) {
    return fail<ScientificFile>(Code::invalid_request, error, "cli.scientific",
                                "the command cannot run");
  }
  json document;
  try {
    document = json::parse(text);
  } catch (const std::exception& failure) {
    return fail<ScientificFile>(Code::invalid_request,
                                std::string("the scientific file is not valid JSON: ") + failure.what(),
                                "cli.scientific", "the command cannot run");
  }
  if (!document.is_object()) {
    return fail<ScientificFile>(Code::invalid_request, "the scientific file is not an object",
                                "cli.scientific", "the command cannot run");
  }
  ScientificFile scientific;
  scientific.command = string_field(document, "command");
  if (scientific.command != config.command) {
    return fail<ScientificFile>(Code::invalid_request,
                                "the scientific file does not belong to the command",
                                "cli.scientific", "the command cannot run");
  }
  if (!has(document, "request")) {
    return fail<ScientificFile>(Code::invalid_request, "the scientific file has no request",
                                "cli.scientific", "the command cannot run");
  }
  scientific.request = document.at("request");
  if (!scientific.request.is_object()) {
    return fail<ScientificFile>(Code::invalid_request, "the request is not an object", "cli.scientific",
                                "the command cannot run");
  }
  // The I/O fields are injected by the CLI: a scientific file that carries
  // them conflicts with the configuration.
  const std::vector<std::string> forbidden = config.command == "normalize"
                                                 ? std::vector<std::string>{"frames", "outputs"}
                                                 : (config.command == "mosaic"
                                                        ? std::vector<std::string>{"inputs", "output"}
                                                        : std::vector<std::string>{"input", "output"});
  for (const std::string& field : forbidden) {
    if (scientific.request.contains(field)) {
      return fail<ScientificFile>(Code::invalid_request,
                                  "the scientific file must not carry the injected field " + field,
                                  "cli.scientific", "the command cannot run");
    }
  }
  if (has(document, "frame_options")) {
    scientific.frame_options = document.at("frame_options");
    if (!scientific.frame_options.is_array()) {
      return fail<ScientificFile>(Code::invalid_request, "the frame options are not a list",
                                  "cli.scientific", "the command cannot run");
    }
  }
  return ok_result(scientific);
}

}  // namespace acsd::cli
