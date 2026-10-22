#pragma once
// The acsd command line interface.
//
// The CLI only decodes the configuration and the scientific request, builds
// the public objects and calls the three workflows.  It contains no science.
// The progress is reported as JSONL events on stdout, the human readable log
// goes to stderr.
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "acsd/contracts.hpp"

namespace acsd::cli {

using json = nlohmann::json;

// The exit codes of the CLI.
enum ExitCode : int {
  kSuccess = 0,
  kInvalidInput = 2,
  kUnsupported = 3,
  kFailure = 4,
  kCancelled = 5,
  kPartial = 6,
};

// One input of the configuration.
struct InputEntry {
  std::string path;
  std::string expected_hash;
  std::string frame_id;
  std::string calibration;
  std::string response;
};

// The parsed configuration.  Every path is relative to the directory of the
// configuration file.
struct Config {
  std::string schema;
  std::string command;
  std::vector<InputEntry> inputs;
  std::string scientific;
  std::string output;
  std::string catalogue;
  Limits resources;
  std::string product_profile = "full";
  std::string signal_dtype = "f64";
  U32 tile_log2_width = 9;
  bool durable = true;
  std::filesystem::path base;
};

// Reads and validates the configuration file.
Result<Config> load_config(const std::filesystem::path& path);

// The scientific request file of the configuration.
struct ScientificFile {
  std::string command;
  json request;
  json frame_options;
};
Result<ScientificFile> load_scientific(const Config& config);

// The JSONL event writer.  The sequence numbers are strictly increasing and
// every run writes exactly one terminal event.
class EventWriter {
 public:
  EventWriter(std::ostream& stream, std::string run_id);

  void emit(const std::string& event, const std::string& frame, const std::optional<U64>& block,
            const std::string& code, const std::optional<json>& assessment, U64 completed,
            U64 total, const std::string& message);
  void run_started(U64 total, const std::string& message);
  void frame_started(const std::string& frame, U64 total);
  void block_progress(const std::string& frame, U64 block, U64 completed, U64 total);
  void frame_finished(const std::string& frame, const std::string& code, U64 completed, U64 total,
                      const std::string& message);
  void product_published(const std::string& frame, const std::string& path);
  void issue(const std::string& code, const std::string& message);
  void run_finished(int exit_code, const json& summary);

  U64 sequence() const { return sequence_; }

 private:
  std::ostream& stream_;
  std::string run_id_;
  U64 sequence_ = 0;
};

// The cancellation source of one run.
class CancellationSource final : public Cancellation {
 public:
  void request();
  bool requested() const noexcept override { return requested_.load(); }

 private:
  std::atomic<bool> requested_{false};
};

// Reads the control protocol from stdin until the stream ends.  The line
// {"protocol":"acsd.control/2","command":"cancel","run_id":...} cancels the
// run.
void watch_control_stream(CancellationSource& source, const std::string& run_id);
// Stops the control thread (the run is over).
void stop_control_stream();
// Reads one line of the control stream with a timeout; false at the end.
bool read_control_line(std::string& line);

// The three commands.  The cancellation source is the one of the run: the
// control stream and the signal handler request the cancellation there.
int run_normalize(Config& config, EventWriter& events, CancellationSource& cancellation);
int run_mosaic(Config& config, EventWriter& events, CancellationSource& cancellation);
int run_export(Config& config, EventWriter& events, CancellationSource& cancellation);

// The diagnostics and the static templates.
int run_doctor(const Config* config, EventWriter& events);
void print_template(const std::string& command);

}  // namespace acsd::cli
