// The acsd entry point.
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "cli.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"

namespace {

void print_usage() {
  std::cerr << "usage: acsd normalize|mosaic|export --json <config.json>\n"
               "       acsd --version --json\n"
               "       acsd doctor --json\n"
               "       acsd template <normalize|mosaic|export>\n";
}

std::string make_run_id() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const std::string stamp = std::to_string(now.count());
  return "run-" + acsd::model::sha256_hex(stamp).substr(0, 16);
}

}  // namespace

int main(int argc, char** argv) {
  std::string command;
  std::filesystem::path config_path;
  bool json_output = false;

  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--json") {
      json_output = true;
    } else if (argument == "--version") {
      command = "--version";
    } else if (argument == "--help" || argument == "-h") {
      print_usage();
      return 0;
    } else if (!argument.empty() && argument[0] == '-') {
      std::cerr << "acsd: unknown option " << argument << "\n";
      print_usage();
      return 2;
    } else if (command.empty()) {
      command = argument;
    } else if (argument == "--config" && i + 1 < argc) {
      config_path = argv[++i];
    } else if (config_path.empty() && argument.size() > 4 && argument.rfind("--", 0) == 0) {
      // The "--json <file>" form of the command line.
      config_path = argument.substr(2);
    } else {
      config_path = argument;
    }
  }

  if (command == "--version") {
    acsd::cli::EventWriter events(std::cout, make_run_id());
    events.run_started(0, "acsd 2.0.0");
    // The summary of a diagnostic command carries the same fields as the
    // summary of a scientific run; the counts are zero because the command
    // processes no input.
    acsd::cli::json summary;
    summary["inputs_total"] = 0;
    summary["published"] = 0;
    summary["unsolved"] = 0;
    summary["failed"] = 0;
    summary["cancelled"] = 0;
    summary["frames"] = acsd::cli::json::array();
    summary["version"] = "2.0.0";
    events.run_finished(0, summary);
    return 0;
  }

  if (command == "template") {
    if (config_path.empty()) {
      std::cerr << "acsd: the template command needs a command name\n";
      return 2;
    }
    acsd::cli::print_template(config_path.string());
    return 0;
  }

  if (command == "doctor") {
    acsd::cli::EventWriter events(std::cout, make_run_id());
    events.run_started(0, "the environment was inspected");
    return acsd::cli::run_doctor(nullptr, events);
  }

  if (command != "normalize" && command != "mosaic" && command != "export") {
    std::cerr << "acsd: unknown command " << command << "\n";
    print_usage();
    return 2;
  }
  if (config_path.empty()) {
    std::cerr << "acsd: the command needs a configuration file\n";
    print_usage();
    return 2;
  }

  const std::string run_id = make_run_id();
  acsd::cli::EventWriter events(std::cout, run_id);
  acsd::cli::CancellationSource cancellation;
  std::thread control([&cancellation, &run_id]() {
    acsd::cli::watch_control_stream(cancellation, run_id);
  });

  acsd::cli::Config config;
  acsd::Result<acsd::cli::Config> loaded = acsd::cli::load_config(config_path);
  if (!acsd::ok(loaded)) {
    events.issue("invalid_request", loaded.assessment.issues.empty()
                                          ? "the configuration is invalid"
                                          : loaded.assessment.issues.front().reason);
    events.run_finished(acsd::cli::kInvalidInput,
                        acsd::cli::json{{"inputs_total", 0}, {"published", 0}, {"unsolved", 0},
                                        {"failed", 0}, {"cancelled", 0}, {"frames", acsd::cli::json::array()}});
    acsd::cli::stop_control_stream();
    if (control.joinable()) control.join();
    return acsd::cli::kInvalidInput;
  }
  config = acsd::val(loaded);
  if (config.command != command) {
    events.issue("invalid_request", "the configuration belongs to another command");
    events.run_finished(acsd::cli::kInvalidInput,
                        acsd::cli::json{{"inputs_total", 0}, {"published", 0}, {"unsolved", 0},
                                        {"failed", 0}, {"cancelled", 0}, {"frames", acsd::cli::json::array()}});
    acsd::cli::stop_control_stream();
    if (control.joinable()) control.join();
    return acsd::cli::kInvalidInput;
  }

  events.run_started(config.inputs.size(), "the " + command + " command started");
  int code = acsd::cli::kInvalidInput;
  if (command == "normalize") {
    code = acsd::cli::run_normalize(config, events, cancellation);
  } else if (command == "mosaic") {
    code = acsd::cli::run_mosaic(config, events, cancellation);
  } else {
    code = acsd::cli::run_export(config, events, cancellation);
  }
  // The control thread ends with the run: the standard input may stay open.
  acsd::cli::stop_control_stream();
  if (control.joinable()) control.join();
  return code;
}
