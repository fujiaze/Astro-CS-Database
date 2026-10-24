#pragma once
// Independent verification harness for validation/runtime.
//
// It emits one JSON evidence document on stdout.  It contains no production
// runtime code and includes only the public headers: the case programs call
// the public acsd::rt API; the harness only accounts for checks.
//
// JSON shape:
//   {case, source, status, attempts, successes, failures:[...], checks:[...],
//    values:{...}, resources:{...}, commands:[...], notes:[...]}
//
// `successes` is the number of *passing* checks; a case never passes merely
// because it ran.  run_cases.py additionally requires every check named in
// case.json to be present and successful and attempts to reach the frozen
// minimum.
#include <acsd/contracts.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace vrt {

using acsd::Code;
using acsd::Result;
using acsd::U32;
using acsd::U64;

inline const char* code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "unknown";
}

inline std::string issue_text(const acsd::Assessment& assessment) {
  if (assessment.issues.empty()) return "";
  std::string text;
  for (const acsd::Issue& issue : assessment.issues) {
    if (!text.empty()) text += " | ";
    text += code_name(issue.code);
    if (!issue.origin.empty()) text += ":" + issue.origin;
    if (!issue.reason.empty()) text += ":" + issue.reason;
  }
  return text;
}

template <class T>
inline std::string code_of(const Result<T>& r) {
  std::string text = code_name(r.code);
  const std::string extra = issue_text(r.assessment);
  if (!extra.empty()) text += "(" + extra + ")";
  return text;
}

inline std::string json_escape(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 8);
  for (unsigned char c : in) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      default:
        if (c < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
          out += buffer;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

inline std::string quote(const std::string& text) {
  return "\"" + json_escape(text) + "\"";
}

inline std::string number(double value, int precision = 17) {
  std::ostringstream out;
  out.precision(precision);
  out << value;
  return out.str();
}

inline std::string integer(long long value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

inline std::string integer(unsigned long long value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

inline std::int64_t steady_ns() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

inline double now_seconds() {
  using namespace std::chrono;
  return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline void sleep_ms(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

struct Check {
  std::string name;
  bool pass = false;
  std::string expected;
  std::string actual;
  std::string detail;
};

class Recorder {
 public:
  Recorder(std::string case_id, std::string source)
      : case_id_(std::move(case_id)), source_(std::move(source)) {}

  void check(const std::string& name, bool pass, std::string expected,
             std::string actual, std::string detail = "") {
    ++attempts_;
    if (pass) ++successes_;
    checks_.push_back(
        Check{name, pass, std::move(expected), std::move(actual), std::move(detail)});
    if (!pass) {
      failures_.push_back(checks_.back());
      std::cerr << "[FAIL] " << name << " expected=" << checks_.back().expected
                << " actual=" << checks_.back().actual
                << (checks_.back().detail.empty() ? "" : " detail=" + checks_.back().detail)
                << "\n";
    } else {
      std::cerr << "[ok] " << name << "\n";
    }
  }

  void expect_code(const std::string& name, const std::string& expected_code,
                   const Result<acsd::rt::Done>& result) {
    check(name, code_name(result.code) == expected_code, expected_code,
          code_of(result));
  }

  template <class T>
  void expect_code(const std::string& name, const std::string& expected_code,
                   const Result<T>& result) {
    check(name, code_name(result.code) == expected_code, expected_code,
          code_of(result));
  }

  void value(const std::string& key, const std::string& text) {
    values_.push_back({key, quote(text)});
  }
  void value(const std::string& key, const char* text) {
    values_.push_back({key, quote(std::string(text))});
  }
  void value(const std::string& key, bool flag) {
    values_.push_back({key, flag ? "true" : "false"});
  }
  void value(const std::string& key, double v) {
    values_.push_back({key, number(v)});
  }
  template <class T,
            std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
  void value(const std::string& key, T v) {
    values_.push_back({key, integer(static_cast<unsigned long long>(v))});
  }
  template <class T>
  void value(const std::string& key, const std::optional<T>& maybe) {
    if (maybe)
      value(key, static_cast<long long>(*maybe));
    else
      values_.push_back({key, "null"});
  }

  template <class T,
            std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
  void resource(const std::string& key, T v) {
    resources_.push_back({key, integer(static_cast<unsigned long long>(v))});
  }

  void command(const std::string& text) { commands_.push_back(text); }
  void note(const std::string& text) { notes_.push_back(text); }

  int finish() {
    if (attempts_ == 0) {
      check("harness.produced_checks", false, ">0 checks", "0 checks",
            "a case with no assertions is not evidence");
    }
    std::ostringstream out;
    out << "{\n";
    out << "  \"case\": " << quote(case_id_) << ",\n";
    out << "  \"source\": " << quote(source_) << ",\n";
    out << "  \"status\": \"ran\",\n";
    out << "  \"attempts\": " << attempts_ << ",\n";
    out << "  \"successes\": " << successes_ << ",\n";
    out << "  \"failures\": [";
    for (std::size_t i = 0; i < failures_.size(); ++i) {
      if (i) out << ", ";
      const Check& c = failures_[i];
      out << "{\"check\": " << quote(c.name) << ", \"expected\": " << quote(c.expected)
          << ", \"actual\": " << quote(c.actual) << ", \"detail\": "
          << quote(c.detail) << "}";
    }
    out << "],\n";
    out << "  \"checks\": [\n";
    for (std::size_t i = 0; i < checks_.size(); ++i) {
      const Check& c = checks_[i];
      out << "    {\"check\": " << quote(c.name) << ", \"pass\": "
          << (c.pass ? "true" : "false") << ", \"expected\": " << quote(c.expected)
          << ", \"actual\": " << quote(c.actual) << ", \"detail\": "
          << quote(c.detail) << "}";
      out << (i + 1 == checks_.size() ? "\n" : ",\n");
    }
    out << "  ],\n";
    out << "  \"values\": {" << join(values_) << "},\n";
    out << "  \"resources\": {" << join(resources_) << "},\n";
    out << "  \"commands\": [";
    for (std::size_t i = 0; i < commands_.size(); ++i) {
      if (i) out << ", ";
      out << quote(commands_[i]);
    }
    out << "],\n";
    out << "  \"notes\": [";
    for (std::size_t i = 0; i < notes_.size(); ++i) {
      if (i) out << ", ";
      out << quote(notes_[i]);
    }
    out << "]\n";
    out << "}\n";
    std::cout << out.str();
    std::cout.flush();
    return 0;
  }

  int finish_with_exception(const std::string& where) {
    check("harness.no_exception", false, "no exception", "exception",
          "uncaught exception at " + where);
    return finish();
  }

  bool all_ok() const { return failures_.empty() && attempts_ > 0; }
  int attempts() const { return attempts_; }
  int successes() const { return successes_; }

 private:
  static std::string join(const std::vector<std::pair<std::string, std::string>>& items) {
    std::string out;
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (i) out += ", ";
      out += quote(items[i].first) + ": " + items[i].second;
    }
    if (out.empty()) out = "";
    return out;
  }

  std::string case_id_;
  std::string source_;
  int attempts_ = 0;
  int successes_ = 0;
  std::vector<Check> checks_;
  std::vector<Check> failures_;
  std::vector<std::pair<std::string, std::string>> values_;
  std::vector<std::pair<std::string, std::string>> resources_;
  std::vector<std::string> commands_;
  std::vector<std::string> notes_;
};

// ---------------------------------------------------------------------------
// Small public-DTO helpers (no science, no production headers).
// ---------------------------------------------------------------------------

inline acsd::Unit dimensionless_unit() {
  acsd::Unit unit;
  unit.scale = "dimensionless";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

inline acsd::Unit adu_unit() {
  acsd::Unit unit;
  unit.scale = "ADU";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

inline acsd::Ref<acsd::Index> make_index(U64 size, const std::string& object_id,
                                         acsd::Index::Kind kind = acsd::Index::Kind::native) {
  auto index = std::make_shared<acsd::Index>();
  index->size = size;
  index->kind = kind;
  index->order = 0;
  index->identity.object_id = object_id;
  index->identity.content_hash = "sha256:" + std::string(64, '0');
  index->identity.run_id = "validation.runtime";
  index->identity.revision = 0;
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  return index;
}

inline std::string zero_hash() { return "sha256:" + std::string(64, '0'); }

// A plan with one region and the given edges.  Public DTO only.
inline acsd::rt::InvocationPlan make_plan(U32 region_id,
                                          std::vector<acsd::rt::EdgeSpec> edges,
                                          std::vector<acsd::rt::StageSpec> stages = {},
                                          acsd::U64 floor_bytes = 0,
                                          acsd::U64 persistent_bytes = 0) {
  acsd::rt::InvocationPlan plan;
  plan.input_hash = zero_hash();
  plan.request_hash = zero_hash();
  plan.algorithm_hash = zero_hash();
  plan.elements = 1;
  plan.block_elements = 1;
  plan.cohorts = 1;
  plan.options = acsd::rt::PlanOptions{1, 1, acsd::rt::Placement::memory};
  acsd::rt::RegionSpec region;
  region.id = region_id;
  region.kind = acsd::rt::RegionKind::stream;
  region.cohorts = 1;
  region.progress_floor.host_bytes = floor_bytes;
  region.persistent.host_bytes = persistent_bytes;
  region.stages = std::move(stages);
  region.edges = std::move(edges);
  plan.regions.push_back(std::move(region));
  plan.progress_floor.host_bytes = floor_bytes;
  plan.persistent.host_bytes = persistent_bytes;
  return plan;
}

inline acsd::rt::EdgeSpec make_edge(U32 id, U32 max_items, U64 max_queued_bytes,
                                    U32 from = 0, U32 to = 0) {
  acsd::rt::EdgeSpec edge;
  edge.id = id;
  edge.from = from;
  edge.to = to;
  edge.max_block_bytes = max_queued_bytes;
  edge.window.max_items = max_items;
  edge.window.max_queued_bytes = max_queued_bytes;
  edge.window.order = acsd::rt::PipeOrder::ordered;
  return edge;
}

inline acsd::rt::StageSpec make_stage(U32 id, U64 scratch_bytes, U64 output_bytes,
                                      U64 work_upper = 0) {
  acsd::rt::StageSpec stage;
  stage.id = id;
  stage.scratch.host_bytes = scratch_bytes;
  stage.output_upper.host_bytes = output_bytes;
  stage.work_upper = work_upper;
  if (id > 0) stage.predecessors.push_back(id - 1);
  return stage;
}

inline acsd::rt::ScopeLimits limits(U64 host_bytes, U64 max_work = 0,
                                    U64 metadata_bytes = 0,
                                    U32 cpu_tokens = 0, U32 io_slots = 0,
                                    U64 pinned_bytes = 0) {
  acsd::rt::ScopeLimits out;
  out.ceiling.host_bytes = host_bytes;
  out.ceiling.pinned_bytes = pinned_bytes;
  out.max_work = max_work;
  out.metadata_bytes = metadata_bytes;
  out.ceiling.cpu_tokens = cpu_tokens;
  out.ceiling.io_slots = io_slots;
  return out;
}

inline acsd::rt::ResourceVector bytes(U64 host_bytes) {
  acsd::rt::ResourceVector vector;
  vector.host_bytes = host_bytes;
  return vector;
}

inline std::string scratch_root(const std::string& case_id) {
  const char* env = std::getenv("ACSD_SCRATCH");
  if (env != nullptr && *env != '\0') return std::string(env) + "/" + case_id;
  return "/tmp/acsd-verify/" + case_id;
}

inline Result<std::shared_ptr<acsd::rt::Services>> start_services(
    U64 host_bytes, U64 max_work, U32 cpu_threads, U32 read_slots,
    U32 write_slots, const std::string& scratch, U64 pinned_bytes = 0,
    acsd::rt::DispatchObserver dispatch_observer = nullptr) {
  acsd::rt::PoolConfig config;
  config.dispatch_observer = std::move(dispatch_observer);
  config.process = limits(host_bytes, max_work, 1ull << 20, 0, 0, pinned_bytes);
  config.cpu_threads = cpu_threads;
  config.read_slots = read_slots;
  config.write_slots = write_slots;
  config.scratch_root = scratch;
  return acsd::rt::make_services(config);
}

}  // namespace vrt
