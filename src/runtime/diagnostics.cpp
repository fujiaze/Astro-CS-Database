// Try-only development probes: bounded performance records and intermediate
// data snapshots.
//
// The compile switch ACSD_DEV_MODE is the single switch.  When it is OFF the
// factory returns no sink at all: no environment is read, no directory is
// created and every probe entry point is a constant no-op.  When it is ON a
// sink copies a selected record/snapshot into diagnostics-owned storage (D),
// enqueues it on the unique I/O lane and drops (with a counted reason) when a
// quota, queue, disk or construction limit is reached.  No scientific code,
// model or hash is ever changed by an entry point in this file.
#include <acsd/runtime/diagnostics.hpp>

#include "runtime/internal.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
#define ACSD_PROBES_ON 1
#else
#define ACSD_PROBES_ON 0
#endif

namespace acsd::rt {
namespace {

using detail::IoRequest;

// One fixed counter for construction failures which happen before or outside a
// sink (an exception thrown while evaluating a probe argument).  It is
// allocation-free and reported by every summary as an unknown-scope drop.
std::atomic<U64> g_construction_failed{0};

std::atomic<U64> g_sinks_created{0};

inline U64 now_ns() noexcept {
  return static_cast<U64>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

U64 read_env_u64(const char* name, U64 fallback) noexcept {
  const char* text = std::getenv(name);
  if (text == nullptr || *text == '\0') return fallback;
  U64 value = 0;
  const char* end = text + std::char_traits<char>::length(text);
  const auto parsed = std::from_chars(text, end, value);
  if (parsed.ec != std::errc{} || parsed.ptr != end) return fallback;
  return value;
}

std::string env_string(const char* name) {
  const char* text = std::getenv(name);
  if (text == nullptr) return {};
  return std::string(text);
}

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[7];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x",
                        static_cast<unsigned>(static_cast<unsigned char>(c)));
          out += buffer;
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

const char* dtype_name(DType dtype) noexcept {
  switch (dtype) {
    case DType::f64: return "f64";
    case DType::f32: return "f32";
    case DType::u64: return "u64";
    case DType::u32: return "u32";
    case DType::u16: return "u16";
    case DType::u8: return "u8";
    case DType::dyadic: return "dyadic";
  }
  return "unknown";
}

const char* kind_name(PerfKind kind) noexcept {
  switch (kind) {
    case PerfKind::span: return "span";
    case PerfKind::counter: return "counter";
    case PerfKind::gauge: return "gauge";
  }
  return "span";
}

// The M3 static probe registry.  It mirrors the initial registration table in
// standards/PROBES.md and is replaced by the generated process manifest once
// the registry owner lands.  A probe id which is not registered is never
// selected; the drop is counted instead of silently ignored.
bool registered_probe(std::string_view id) noexcept {
  static constexpr std::string_view kRegistered[] = {
      "normalize.calibration.signal",
      "normalize.astrometry.solution",
      "normalize.psf.model",
      "normalize.photometry.measurements",
      "normalize.photometry.solution",
      "normalize.drizzle.geometry",
      "normalize.drizzle.signal",
      "normalize.information.reference_flux",
      "normalize.information.scene_variance",
      "mosaic.sky.controls",
      "mosaic.sky.fit",
      "mosaic.selection.samples",
      "mosaic.weight.coefficients",
      "mosaic.integration.signal",
      "export.projection.overlaps",
      "export.fits.signal",
      "normalize.calibration.frame",
      "normalize.astrometry.search",
      "normalize.psf.fit",
      "normalize.photometry.fit",
      "normalize.response.integral",
      "normalize.drizzle.overlap",
      "operators.apply",
      "operators.transpose",
      "uncertainty.quadratic_form",
      "mosaic.sky.fit_component",
      "mosaic.weights.solve",
      "mosaic.integration.block",
      "export.projection.block",
      "io.read",
      "io.write",
      "resources.wait",
      "resources.payload_peak",
      "geometry.precision_upgrade",
      "observation.regroup.signal",
      "photometry.apply.signal",
      "coordinates.wcs.solution",
      "catalogue.query",
      "framework.invocation.phase",
      "framework.invocation.usage",
  };
  for (const std::string_view known : kRegistered)
    if (known == id) return true;
  return false;
}

std::string encode_tags(const ProbeTags& tags) {
  std::string out;
  out += "{\"frame\":";
  if (tags.frame) out += "\"" + json_escape(*tags.frame) + "\"";
  else out += "null";
  out += ",\"component\":";
  if (tags.component) out += "\"" + json_escape(*tags.component) + "\"";
  else out += "null";
  out += ",\"model_hash\":";
  if (tags.model_hash) out += "\"" + json_escape(*tags.model_hash) + "\"";
  else out += "null";
  out += ",\"leaf\":";
  if (tags.leaf) {
    out += "{\"order\":" + std::to_string(tags.leaf->order) +
           ",\"ipix\":" + std::to_string(tags.leaf->ipix) + "}";
  } else {
    out += "null";
  }
  out += ",\"roi\":";
  if (tags.roi) {
    out += "{\"lower\":[" + std::to_string(tags.roi->lower[0]) + "," +
           std::to_string(tags.roi->lower[1]);
    out += "],\"upper\":[" + std::to_string(tags.roi->upper[0]) + "," +
           std::to_string(tags.roi->upper[1]) + "]}";
  } else {
    out += "null";
  }
  out += ",\"attempt\":" + std::to_string(tags.attempt) + "}";
  return out;
}

std::string encode_optional_u64(const std::optional<U64>& value) {
  return value ? std::to_string(*value) : std::string("null");
}

std::string encode_dyadic(const Dyadic& value) {
  return "{\"integer\":\"" + json_escape(value.signed_integer) +
         "\",\"exponent2\":" + std::to_string(value.exponent2) + "}";
}

std::string encode_unit(const Unit& unit) {
  std::string out = "{\"scale\":\"" + json_escape(unit.scale) + "\"";
  out += ",\"multiplier\":" + std::to_string(unit.multiplier);
  out += ",\"sr_power\":" + std::to_string(unit.sr_power);
  out += ",\"pixel_power\":" + std::to_string(unit.pixel_power);
  out += ",\"nm_power\":" + std::to_string(unit.nm_power);
  out += ",\"linear_power\":" + std::to_string(unit.linear_power);
  out += "}";
  return out;
}

std::string encode_field(const SnapshotField& field) {
  std::string out = "{\"name\":\"" + json_escape(field.name) + "\",\"unit\":\"" +
                    json_escape(field.unit) + "\",\"value\":";
  if (std::holds_alternative<std::monostate>(field.value)) {
    out += "{\"kind\":\"null\"}";
  } else if (const auto* value = std::get_if<double>(&field.value)) {
    out += "{\"kind\":\"f64\",\"value\":" +
           (std::isfinite(*value) ? std::to_string(*value)
                                  : std::string("null")) + "}";
  } else if (const auto* value = std::get_if<U64>(&field.value)) {
    out += "{\"kind\":\"u64\",\"value\":" + std::to_string(*value) + "}";
  } else if (const auto* value = std::get_if<bool>(&field.value)) {
    out += std::string("{\"kind\":\"bool\",\"value\":") +
           (*value ? "true" : "false") + "}";
  } else if (const auto* value = std::get_if<std::string_view>(&field.value)) {
    out += "{\"kind\":\"string\",\"value\":\"" + json_escape(*value) + "\"}";
  } else if (const auto* value =
                 std::get_if<std::reference_wrapper<const ExactInterval>>(
                     &field.value)) {
    const ExactInterval& interval = value->get();
    out += "{\"kind\":\"exact_interval\",\"precision_bits\":" +
           std::to_string(interval.precision_bits) + ",\"lower\":" +
           encode_dyadic(interval.lower) + ",\"upper\":" +
           encode_dyadic(interval.upper) + "}";
  } else {
    out += "{\"kind\":\"unknown\"}";
  }
  out += "}";
  return out;
}

std::string encode_perf(const PerfRecord& record, U64 sequence) {
  std::string out;
  out.reserve(256);
  out += "{\"schema\":\"acsd.dev-perf/0\",\"non_scientific_product\":true"
         ",\"seq\":";
  out += std::to_string(sequence);
  out += ",\"probe_id\":\"" + json_escape(record.probe_id) + "\"";
  out += ",\"kind\":\"";
  out += kind_name(record.kind);
  out += "\",\"tags\":" + encode_tags(record.tags);
  out += ",\"start_steady_ns\":" + std::to_string(record.start_steady_ns);
  out += ",\"wall_ns\":" + encode_optional_u64(record.wall_ns);
  out += ",\"cpu_ns\":" + encode_optional_u64(record.cpu_ns);
  out += ",\"count\":" + encode_optional_u64(record.count);
  out += ",\"bytes\":" + encode_optional_u64(record.bytes);
  out += ",\"span_id\":" + encode_optional_u64(record.span_id);
  out += ",\"parent_span_id\":" + encode_optional_u64(record.parent_span_id);
  out += ",\"gauge\":";
  if (record.gauge && std::isfinite(*record.gauge))
    out += std::to_string(*record.gauge);
  else
    out += "null";
  out += "}";
  return out;
}

std::string encode_snapshot_meta(std::string_view probe_id,
                                 const ProbeTags& tags,
                                 const SnapshotPayload& payload, U64 sequence,
                                 const std::vector<std::string>& views,
                                 U64 blob_bytes) {
  std::string out;
  out.reserve(512 + views.size() * 192);
  out += "{\"schema\":\"acsd.dev-snapshot/0\",\"non_scientific_product\":true"
         ",\"seq\":";
  out += std::to_string(sequence);
  out += ",\"probe_id\":\"" + json_escape(probe_id) + "\"";
  out += ",\"tags\":" + encode_tags(tags);
  out += ",\"coordinate_space\":\"" + json_escape(payload.coordinate_space) +
         "\"";
  out += ",\"index_hash\":\"" + json_escape(payload.index_hash) + "\"";
  out += ",\"blob_bytes\":" + std::to_string(blob_bytes);
  out += ",\"views\":[";
  bool first = true;
  for (const std::string& view : views) {
    if (!first) out += ",";
    first = false;
    out += view;
  }
  out += "]";
  out += ",\"fields\":[";
  first = true;
  for (const SnapshotField& field : payload.fields) {
    if (!first) out += ",";
    first = false;
    out += encode_field(field);
  }
  out += "]}";
  return out;
}

std::string sanitize_name(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '.' || c == '_' ||
                      c == '-';
    out.push_back(safe ? c : '_');
  }
  if (out.empty()) out = "probe";
  return out;
}

bool write_text_file(const std::filesystem::path& path,
                     std::string_view text) noexcept {
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  if (!text.empty())
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  stream.flush();
  return static_cast<bool>(stream);
}

bool append_text_file(const std::filesystem::path& path,
                      std::string_view text) noexcept {
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  std::ofstream stream(path, std::ios::binary | std::ios::app);
  if (!stream) return false;
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  stream.flush();
  return static_cast<bool>(stream);
}

bool write_binary_file(const std::filesystem::path& path,
                       std::span<const std::byte> bytes) noexcept {
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  if (!bytes.empty())
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  stream.flush();
  return static_cast<bool>(stream);
}

struct Counters {
  std::atomic<U64> selected{0};
  std::atomic<U64> not_selected{0};
  std::atomic<U64> not_registered{0};
  std::atomic<U64> written{0};
  std::atomic<U64> dropped{0};
  std::atomic<U64> skipped_budget{0};
  std::atomic<U64> io_errors{0};
  std::atomic<U64> snapshots{0};
  std::atomic<U64> perf{0};
  std::atomic<U64> flush_ns{0};
  std::atomic<U64> bytes_written{0};
  std::atomic<U64> queue_peak{0};
};

struct QueuedItem {
  bool snapshot = false;
  std::string line;
  std::vector<std::byte> blob;
  std::string file;  // snapshot payload path relative to the diagnostics root
  U64 charge = 0;
};

struct SinkControl {
  mutable std::mutex mutex;
  std::condition_variable idle;
  std::deque<QueuedItem> queue;
  U64 queued_bytes = 0;
  bool flushing = false;
  bool closing = false;
  U64 sequence = 0;

  // Static policy resolved once at construction.
  std::function<void(IoRequest)> submit_io;
  std::filesystem::path root;
  U64 memory_bytes = 0;
  U64 disk_bytes = 0;
  U32 max_items = 0;
  U32 flush_timeout_ms = 1000;
  bool perf_enabled = false;
  bool snapshots_enabled = false;
  Counters counters;
};

void write_summary(const std::shared_ptr<SinkControl>& control) noexcept;

void perform_flush(std::shared_ptr<SinkControl> control) noexcept {
  const U64 start = now_ns();
  std::deque<QueuedItem> batch;
  {
    std::lock_guard<std::mutex> lock(control->mutex);
    batch.swap(control->queue);
    control->queued_bytes = 0;
  }
  U64 written_bytes = 0;
  for (QueuedItem& item : batch) {
    const U64 projected =
        control->counters.bytes_written.load() + written_bytes + item.charge;
    if (projected > control->disk_bytes) {
      control->counters.skipped_budget.fetch_add(1);
      continue;
    }
    bool ok = false;
    if (item.snapshot) {
      ok = write_binary_file(control->root / item.file, item.blob);
      if (ok)
        ok = append_text_file(control->root / "snapshots/index.jsonl",
                              item.line + "\n");
      if (ok) control->counters.snapshots.fetch_add(1);
    } else {
      ok = append_text_file(control->root / "perf/events.jsonl",
                            item.line + "\n");
      if (ok) control->counters.perf.fetch_add(1);
    }
    if (!ok) {
      control->counters.io_errors.fetch_add(1);
      continue;
    }
    written_bytes += item.charge;
    control->counters.bytes_written.fetch_add(item.charge);
    control->counters.written.fetch_add(1);
  }
  control->counters.flush_ns.fetch_add(now_ns() - start);

  bool reschedule = false;
  {
    std::lock_guard<std::mutex> lock(control->mutex);
    control->flushing = false;
    reschedule = !control->closing && !control->queue.empty();
    if (!reschedule) control->idle.notify_all();
  }
  write_summary(control);
  if (reschedule) {
    std::function<void(IoRequest)> submit;
    {
      std::lock_guard<std::mutex> lock(control->mutex);
      if (control->flushing || control->closing || control->queue.empty())
        return;
      control->flushing = true;
      submit = control->submit_io;
    }
    if (!submit) return;
    try {
      submit(IoRequest{true, [control]() { perform_flush(control); }});
    } catch (...) {
      std::lock_guard<std::mutex> lock(control->mutex);
      control->flushing = false;
      control->counters.dropped.fetch_add(1);
    }
  }
}

void schedule_flush(const std::shared_ptr<SinkControl>& control) noexcept {
  std::function<void(IoRequest)> submit;
  {
    std::lock_guard<std::mutex> lock(control->mutex);
    if (control->closing || control->flushing || control->queue.empty())
      return;
    if (!control->submit_io) return;
    control->flushing = true;
    submit = control->submit_io;
  }
  try {
    submit(IoRequest{true, [control]() { perform_flush(control); }});
  } catch (...) {
    std::lock_guard<std::mutex> lock(control->mutex);
    control->flushing = false;
    control->counters.dropped.fetch_add(1);
  }
}

void write_summary(const std::shared_ptr<SinkControl>& control) noexcept {
  std::string out;
  out += "{\"schema\":\"acsd.dev-summary/0\",\"non_scientific_product\":true";
  out += ",\"perf_enabled\":";
  out += control->perf_enabled ? "true" : "false";
  out += ",\"snapshots_enabled\":";
  out += control->snapshots_enabled ? "true" : "false";
  out += ",\"selected\":" + std::to_string(control->counters.selected.load());
  out += ",\"not_selected\":" +
         std::to_string(control->counters.not_selected.load());
  out += ",\"not_registered\":" +
         std::to_string(control->counters.not_registered.load());
  out += ",\"written\":" + std::to_string(control->counters.written.load());
  out += ",\"perf_records\":" + std::to_string(control->counters.perf.load());
  out += ",\"snapshots\":" + std::to_string(control->counters.snapshots.load());
  out += ",\"dropped\":" + std::to_string(control->counters.dropped.load());
  out += ",\"skipped_budget\":" +
         std::to_string(control->counters.skipped_budget.load());
  out += ",\"io_errors\":" +
         std::to_string(control->counters.io_errors.load());
  out += ",\"bytes_written\":" +
         std::to_string(control->counters.bytes_written.load());
  out += ",\"flush_wall_ns\":" +
         std::to_string(control->counters.flush_ns.load());
  out += ",\"queue_peak\":" +
         std::to_string(control->counters.queue_peak.load());
  out += ",\"construction_failed\":" +
         std::to_string(g_construction_failed.load());
  U64 queued = 0, depth = 0;
  {
    std::lock_guard<std::mutex> lock(control->mutex);
    queued = control->queued_bytes;
    depth = control->queue.size();
  }
  out += ",\"queue_bytes\":" + std::to_string(queued);
  out += ",\"queue_depth\":" + std::to_string(depth);
  out += "}";
  // Serialise summary replacement with neighbouring summary writes.
  static std::mutex summary_mutex;
  std::lock_guard<std::mutex> lock(summary_mutex);
  write_text_file(control->root / "summary.json", out);
}

#if ACSD_PROBES_ON

std::vector<std::string> split_selection(std::string_view text) {
  std::vector<std::string> out;
  std::string current;
  for (const char c : text) {
    if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (!current.empty()) out.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  if (!current.empty()) out.push_back(current);
  return out;
}

// The only implemented sink.  Everything it retains is a private copy: no
// borrowed span, Lease, ArrayRef or model reference survives the call.
class DevProbeSink final : public detail::ProbeSink {
 public:
  explicit DevProbeSink(std::shared_ptr<SinkControl> control)
      : control_(std::move(control)) {}

  ~DevProbeSink() override {
    std::unique_lock<std::mutex> lock(control_->mutex);
    control_->closing = true;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(control_->flush_timeout_ms);
    control_->idle.wait_until(lock, deadline,
                              [this] { return !control_->flushing; });
    std::deque<QueuedItem> leftover;
    leftover.swap(control_->queue);
    control_->queued_bytes = 0;
    control_->counters.dropped.fetch_add(leftover.size());
    lock.unlock();
    write_summary(control_);
  }

  bool enabled() const noexcept override {
    return control_->perf_enabled || control_->snapshots_enabled;
  }

  bool selected(const InvocationContext& ctx, std::string_view probe_id,
                const ProbeTags& tags) noexcept override {
    (void)ctx;
    (void)tags;
    try {
      if (!enabled()) return false;
      if (!registered_probe(probe_id)) {
        control_->counters.not_registered.fetch_add(1);
        return false;
      }
      if (!policy_selects(probe_id)) {
        control_->counters.not_selected.fetch_add(1);
        return false;
      }
      control_->counters.selected.fetch_add(1);
      return true;
    } catch (...) {
      g_construction_failed.fetch_add(1);
      return false;
    }
  }

  void record(const InvocationContext& ctx,
              const PerfRecord& record) noexcept override {
    (void)ctx;
    try {
      if (!control_->perf_enabled) return;
      if (!registered_probe(record.probe_id)) {
        control_->counters.not_registered.fetch_add(1);
        return;
      }
      if (!policy_selects(record.probe_id)) {
        control_->counters.not_selected.fetch_add(1);
        return;
      }
      U64 sequence = 0;
      {
        std::lock_guard<std::mutex> lock(control_->mutex);
        sequence = control_->sequence++;
      }
      enqueue(encode_perf(record, sequence), {}, false);
    } catch (...) {
      g_construction_failed.fetch_add(1);
    }
  }

  void snapshot(const InvocationContext& ctx, std::string_view probe_id,
                const ProbeTags& tags,
                const SnapshotPayload& payload) noexcept override {
    (void)ctx;
    try {
      if (!control_->snapshots_enabled) return;
      if (!registered_probe(probe_id)) {
        control_->counters.not_registered.fetch_add(1);
        return;
      }
      if (!policy_selects(probe_id)) {
        control_->counters.not_selected.fetch_add(1);
        return;
      }
      U64 blob_bytes = 0;
      for (const SnapshotView& view : payload.views) {
        if (view.bytes.size() > (~U64{0}) - blob_bytes) {
          control_->counters.skipped_budget.fetch_add(1);
          return;
        }
        blob_bytes += view.bytes.size();
      }
      U64 sequence = 0;
      {
        std::lock_guard<std::mutex> lock(control_->mutex);
        sequence = control_->sequence++;
        const U64 projected_bytes = control_->queued_bytes + blob_bytes;
        if (control_->queue.size() >= control_->max_items ||
            projected_bytes > control_->memory_bytes) {
          control_->counters.skipped_budget.fetch_add(1);
          return;
        }
      }
      std::vector<std::string> encoded_views;
      encoded_views.reserve(payload.views.size());
      std::vector<std::byte> blob;
      blob.reserve(static_cast<std::size_t>(blob_bytes));
      for (const SnapshotView& view : payload.views) {
        const U64 offset = blob.size();
        std::string encoded = "{\"name\":\"" + json_escape(view.name) + "\"";
        encoded += ",\"index_hash\":\"" + json_escape(view.index_hash) + "\"";
        encoded += ",\"dtype\":\"";
        encoded += dtype_name(view.dtype);
        encoded += "\",\"shape\":[";
        bool first = true;
        for (const U64 extent : view.shape) {
          if (!first) encoded += ",";
          first = false;
          encoded += std::to_string(extent);
        }
        encoded += "],\"range\":{\"begin\":" + std::to_string(view.range.begin) +
                   ",\"count\":" + std::to_string(view.range.count) + "}";
        encoded += ",\"unit\":" + encode_unit(view.unit.get());
        encoded += ",\"offset\":" + std::to_string(offset);
        encoded += ",\"bytes\":" + std::to_string(view.bytes.size());
        encoded += "}";
        encoded_views.push_back(std::move(encoded));
        blob.insert(blob.end(), view.bytes.begin(), view.bytes.end());
      }
      std::string line = encode_snapshot_meta(probe_id, tags, payload, sequence,
                                              encoded_views, blob_bytes);
      const std::string file = "snapshots/" + std::to_string(sequence) + "-" +
                               sanitize_name(probe_id) + ".bin";
      enqueue(std::move(line), std::move(blob), true, file);
    } catch (...) {
      g_construction_failed.fetch_add(1);
    }
  }

  void set_selection(std::string_view selection) {
    if (selection == "*") {
      select_all_ = true;
      return;
    }
    selected_ = split_selection(selection);
  }

 private:
  bool policy_selects(std::string_view probe_id) const noexcept {
    if (select_all_) return true;
    for (const std::string& id : selected_)
      if (id == probe_id) return true;
    return false;
  }

  void enqueue(std::string line, std::vector<std::byte> blob, bool snapshot,
               std::string file = {}) noexcept {
    const U64 charge = static_cast<U64>(line.size() + blob.size());
    {
      std::lock_guard<std::mutex> lock(control_->mutex);
      if (control_->closing) {
        control_->counters.dropped.fetch_add(1);
        return;
      }
      if (control_->queue.size() >= control_->max_items ||
          control_->queued_bytes + charge > control_->memory_bytes) {
        control_->counters.dropped.fetch_add(1);
        return;
      }
      QueuedItem item;
      item.snapshot = snapshot;
      item.line = std::move(line);
      item.blob = std::move(blob);
      item.file = std::move(file);
      item.charge = charge;
      control_->queued_bytes += charge;
      control_->queue.push_back(std::move(item));
      const U64 depth = control_->queue.size();
      if (depth > control_->counters.queue_peak.load())
        control_->counters.queue_peak.store(depth);
    }
    schedule_flush(control_);
  }

  std::shared_ptr<SinkControl> control_;
  std::vector<std::string> selected_;
  bool select_all_ = false;
};

std::shared_ptr<detail::ProbeSink> make_probe_sink_impl(
    const PoolConfig& config, std::function<void(IoRequest)> submit_io) {
  (void)config;
  auto control = std::make_shared<SinkControl>();
  control->submit_io = std::move(submit_io);
  control->memory_bytes = read_env_u64("ACSD_DEV_MEMORY_BYTES", 16u << 20);
  control->disk_bytes = read_env_u64("ACSD_DEV_DISK_BYTES", 256u << 20);
  const U64 max_items = read_env_u64("ACSD_DEV_MAX_ITEMS", 4096);
  control->max_items = static_cast<U32>(
      std::min<U64>(max_items, static_cast<U64>(~U32{0})));
  control->flush_timeout_ms = static_cast<U32>(
      std::min<U64>(read_env_u64("ACSD_DEV_FLUSH_TIMEOUT_MS", 1000), 60000));
  const bool on = env_string("ACSD_DEV_DISABLE") != "1";
  control->perf_enabled = on && env_string("ACSD_DEV_PERF") != "0";
  control->snapshots_enabled = on && env_string("ACSD_DEV_SNAPSHOT") != "0";

  std::string root = env_string("ACSD_DEV_DIR");
  if (root.empty()) {
    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    const std::string base = ec ? std::string("/tmp") : temp.string();
    root = base + "/acsd-dev-" +
             std::to_string(static_cast<unsigned long long>(now_ns()));
  }
  control->root = std::filesystem::path(root);
  std::error_code ec;
  std::filesystem::create_directories(control->root / "perf", ec);
  if (ec) return nullptr;

  auto sink = std::make_shared<DevProbeSink>(control);
  const std::string selection = env_string("ACSD_DEV_PROBES");
  sink->set_selection(selection);

  std::string manifest;
  manifest +=
      "{\"schema\":\"acsd.dev-diagnostics/0\",\"non_scientific_product\":true";
  manifest += ",\"dev_mode\":true";
  manifest += ",\"perf_enabled\":";
  manifest += control->perf_enabled ? "true" : "false";
  manifest += ",\"snapshots_enabled\":";
  manifest += control->snapshots_enabled ? "true" : "false";
  manifest += ",\"selection\":\"";
  manifest += selection == "*" ? "*" : json_escape(selection);
  manifest += "\",\"memory_bytes\":" + std::to_string(control->memory_bytes);
  manifest += ",\"disk_bytes\":" + std::to_string(control->disk_bytes);
  manifest += ",\"max_items\":" + std::to_string(control->max_items);
  manifest += ",\"root\":\"" + json_escape(control->root.string()) + "\"}";
  write_text_file(control->root / "debug_manifest.json", manifest);
  write_summary(control);
  g_sinks_created.fetch_add(1);
  return sink;
}

#endif  // ACSD_PROBES_ON

}  // namespace

namespace detail {

std::shared_ptr<ProbeSink> make_probe_sink(
    const PoolConfig& config, std::function<void(IoRequest)> submit_io,
    std::string& error) {
#if ACSD_PROBES_ON
  try {
    auto sink = make_probe_sink_impl(config, std::move(submit_io));
    if (!sink) {
      error = "diagnostic output directory cannot be created";
      return nullptr;
    }
    return sink;
  } catch (...) {
    error = "diagnostic sink construction failed";
    return nullptr;
  }
#else
  (void)config;
  (void)submit_io;
  (void)error;
  return nullptr;
#endif
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Services probe entry points (owned here so that ProbeSink stays internal to
// this translation unit).
// ---------------------------------------------------------------------------

bool Services::probe_enabled() const noexcept {
  return probes != nullptr && probes->enabled();
}

void Services::record_probe(const InvocationContext& ctx,
                            const PerfRecord& record) noexcept {
  if (!probes) return;
  probes->record(ctx, record);
}

void Services::snapshot_probe(const InvocationContext& ctx,
                              std::string_view probe_id, const ProbeTags& tags,
                              const SnapshotPayload& payload) noexcept {
  if (!probes) return;
  probes->snapshot(ctx, probe_id, tags, payload);
}

bool Services::probe_selected(const InvocationContext& ctx,
                              std::string_view probe_id,
                              const ProbeTags& tags) noexcept {
  if (!probes) return false;
  return probes->selected(ctx, probe_id, tags);
}

// ---------------------------------------------------------------------------
// Public probe entry points.  Every one is noexcept and try-only.
// ---------------------------------------------------------------------------

bool probe_selected(const InvocationContext& ctx, std::string_view probe_id,
                    const ProbeTags& tags) noexcept {
#if ACSD_PROBES_ON
  try {
    const std::shared_ptr<Services> services = ctx.services;
    if (!services) return false;
    return services->probe_selected(ctx, probe_id, tags);
  } catch (...) {
    g_construction_failed.fetch_add(1);
    return false;
  }
#else
  (void)ctx;
  (void)probe_id;
  (void)tags;
  return false;
#endif
}

void try_record(const InvocationContext& ctx, const PerfRecord& record) noexcept {
#if ACSD_PROBES_ON
  try {
    const std::shared_ptr<Services> services = ctx.services;
    if (!services) return;
    services->record_probe(ctx, record);
  } catch (...) {
    g_construction_failed.fetch_add(1);
  }
#else
  (void)ctx;
  (void)record;
#endif
}

void try_snapshot(const InvocationContext& ctx, std::string_view probe_id,
                  const ProbeTags& tags,
                  const SnapshotPayload& payload) noexcept {
#if ACSD_PROBES_ON
  try {
    const std::shared_ptr<Services> services = ctx.services;
    if (!services) return;
    services->snapshot_probe(ctx, probe_id, tags, payload);
  } catch (...) {
    g_construction_failed.fetch_add(1);
  }
#else
  (void)ctx;
  (void)probe_id;
  (void)tags;
  (void)payload;
#endif
}

U64 allocate_span_id(const InvocationContext& ctx) noexcept {
#if ACSD_PROBES_ON
  try {
    const std::shared_ptr<Services> services = ctx.services;
    if (!services) return 0;
    return services->next_span_id();
  } catch (...) {
    g_construction_failed.fetch_add(1);
    return 0;
  }
#else
  (void)ctx;
  return 0;
#endif
}

U64 steady_ns() noexcept { return now_ns(); }

void probe_construction_failed() noexcept {
  g_construction_failed.fetch_add(1);
}

}  // namespace acsd::rt
