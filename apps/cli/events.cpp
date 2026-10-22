// The JSONL event stream and the cancellation source of the CLI.
#include "cli.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace acsd::cli {
namespace {

// The signal handler only sets the atomic flag of the global source; the
// control thread turns it into a cancellation request.
std::atomic<bool> g_signalled{false};
CancellationSource* g_source = nullptr;

void handle_signal(int) { g_signalled.store(true); }

}  // namespace

EventWriter::EventWriter(std::ostream& stream, std::string run_id)
    : stream_(stream), run_id_(std::move(run_id)) {}

void EventWriter::emit(const std::string& event, const std::string& frame,
                       const std::optional<U64>& block, const std::string& code,
                       const std::optional<json>& assessment, U64 completed, U64 total,
                       const std::string& message) {
  json line;
  line["protocol"] = "acsd.events/2";
  line["run_id"] = run_id_;
  line["seq"] = sequence_++;
  line["event"] = event;
  line["frame_id"] = frame.empty() ? json(nullptr) : json(frame);
  line["block"] = block.has_value() ? json(*block) : json(nullptr);
  line["code"] = code;
  line["assessment"] = assessment.has_value() ? *assessment : json(nullptr);
  line["completed"] = completed;
  line["total"] = total;
  line["message"] = message;
  stream_ << line.dump() << "\n";
  stream_.flush();
}

void EventWriter::run_started(U64 total, const std::string& message) {
  emit("run_started", {}, std::nullopt, "ok", std::nullopt, 0, total, message);
}

void EventWriter::frame_started(const std::string& frame, U64 total) {
  emit("frame_started", frame, std::nullopt, "ok", std::nullopt, 0, total,
       "the frame " + frame + " started");
}

void EventWriter::block_progress(const std::string& frame, U64 block, U64 completed, U64 total) {
  emit("block_progress", frame, block, "ok", std::nullopt, completed, total,
       "the block " + std::to_string(block) + " is done");
}

void EventWriter::frame_finished(const std::string& frame, const std::string& code, U64 completed,
                                 U64 total, const std::string& message) {
  emit("frame_finished", frame, std::nullopt, code, std::nullopt, completed, total, message);
}

void EventWriter::product_published(const std::string& frame, const std::string& path) {
  emit("product_published", frame, std::nullopt, "ok", std::nullopt, 1, 1,
       "the product was published at " + path);
}

void EventWriter::issue(const std::string& code, const std::string& message) {
  emit("issue", {}, std::nullopt, code, std::nullopt, 0, 0, message);
}

void EventWriter::run_finished(int exit_code, const json& summary) {
  json line;
  line["protocol"] = "acsd.events/2";
  line["run_id"] = run_id_;
  line["seq"] = sequence_++;
  line["event"] = "run_finished";
  line["frame_id"] = json(nullptr);
  line["block"] = json(nullptr);
  line["code"] = exit_code == kSuccess ? "ok" : "failed";
  line["assessment"] = json(nullptr);
  line["completed"] = 0;
  line["total"] = 0;
  line["message"] = "the run finished";
  line["exit_code"] = exit_code;
  line["summary"] = summary;
  stream_ << line.dump() << "\n";
  stream_.flush();
}

void CancellationSource::request() { requested_.store(true); }

void watch_control_stream(CancellationSource& source, const std::string& run_id) {
  g_source = &source;
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (g_signalled.exchange(false)) source.request();
    if (line.empty()) continue;
    json message;
    try {
      message = json::parse(line);
    } catch (const std::exception&) {
      continue;
    }
    if (!message.is_object()) continue;
    if (message.value("protocol", std::string()) != "acsd.control/2") continue;
    if (message.value("command", std::string()) != "cancel") continue;
    const std::string target = message.value("run_id", std::string());
    if (!target.empty() && target != run_id) continue;
    source.request();
    return;
  }
  if (g_signalled.exchange(false)) source.request();
}

}  // namespace acsd::cli
