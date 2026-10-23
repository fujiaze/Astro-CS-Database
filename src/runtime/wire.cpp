// Wire transport: the request-local object table and the atomic response
// publisher.
//
// The rules are fixed by FOUNDATION_API "统一wire调用与局部对象表": there is no
// process-global model session, references are resolved only through the
// request/response-local WireObjects table, and result_path is committed by
// temporary file + rename so a failed publish never leaves a half product.
#include "internal.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace detail {

struct WireObjectTable {
  struct Entry {
    std::shared_ptr<const void> object;
    const void* cookie = nullptr;
  };
  mutable std::mutex mutex;
  std::map<Id, Entry> aliases;
  std::unordered_map<const void*, Id> interned;
  U64 next_id = 1;
};

namespace {

Result<Done> wire_error(Code code, std::string origin, std::string reason) {
  Result<Done> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  Issue issue;
  issue.code = code;
  issue.origin = std::move(origin);
  issue.reason = std::move(reason);
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

// ---------------------------------------------------------------------------
// Minimal deterministic JSON writer (runtime links no JSON library)
// ---------------------------------------------------------------------------

void write_string(std::string& out, std::string_view text) {
  out.push_back('"');
  for (unsigned char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          static const char hex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(hex[(c >> 4) & 0x0f]);
          out.push_back(hex[c & 0x0f]);
        } else {
          out.push_back(static_cast<char>(c));
        }
    }
  }
  out.push_back('"');
}

void write_u64(std::string& out, U64 value) { out += std::to_string(value); }

const char* code_name(Code code) {
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
  return "internal";
}

const char* validity_name(Validity value) {
  switch (value) {
    case Validity::certified: return "certified";
    case Validity::approximate: return "approximate";
    case Validity::invalid: return "invalid";
  }
  return "invalid";
}

const char* science_name(ScienceState value) {
  switch (value) {
    case ScienceState::bounded: return "bounded";
    case ScienceState::unconverged: return "unconverged";
    case ScienceState::conditional: return "conditional";
    case ScienceState::selection_unresolved: return "selection_unresolved";
    case ScienceState::geometry_solve_unresolved:
      return "geometry_solve_unresolved";
  }
  return "bounded";
}

const char* noise_name(NoiseState value) {
  switch (value) {
    case NoiseState::known: return "known";
    case NoiseState::conditional: return "conditional";
    case NoiseState::approximate: return "approximate";
    case NoiseState::unknown: return "unknown";
  }
  return "unknown";
}

const char* execution_name(ExecutionState value) {
  switch (value) {
    case ExecutionState::complete: return "complete";
    case ExecutionState::cancelled: return "cancelled";
    case ExecutionState::failed: return "failed";
  }
  return "failed";
}

const char* capability_name(Capability value) {
  switch (value) {
    case Capability::supported: return "supported";
    case Capability::unsupported: return "unsupported";
    case Capability::unavailable: return "unavailable";
  }
  return "unsupported";
}

const char* dtype_name(DType value) {
  switch (value) {
    case DType::f64: return "f64";
    case DType::f32: return "f32";
    case DType::u64: return "u64";
    case DType::u32: return "u32";
    case DType::u16: return "u16";
    case DType::u8: return "u8";
    case DType::dyadic: return "dyadic";
  }
  return "f64";
}

const char* codec_name(BlockDescriptor::Codec value) {
  return value == BlockDescriptor::Codec::zstd ? "zstd" : "raw";
}

void write_unit(std::string& out, const Unit& unit) {
  out += "{\"scale\":";
  write_string(out, unit.scale);
  out += ",\"sr_power\":";
  out += std::to_string(unit.sr_power);
  out += ",\"pixel_power\":";
  out += std::to_string(unit.pixel_power);
  out += ",\"nm_power\":";
  out += std::to_string(unit.nm_power);
  out += ",\"multiplier\":";
  out += std::to_string(unit.multiplier);
  out += ",\"linear_power\":";
  out += std::to_string(unit.linear_power);
  out.push_back('}');
}

void write_issue(std::string& out, const Issue& issue) {
  out += "{\"code\":";
  write_string(out, code_name(issue.code));
  out += ",\"scope\":{\"frame\":";
  write_string(out, issue.scope.frame);
  out += ",\"functional\":";
  write_string(out, issue.scope.functional);
  out += ",\"block\":";
  if (issue.scope.block)
    out += std::to_string(*issue.scope.block);
  else
    out += "null";
  out += ",\"leaves\":[";
  for (std::size_t i = 0; i < issue.scope.leaves.size(); ++i) {
    if (i != 0) out.push_back(',');
    out += "{\"order\":";
    out += std::to_string(issue.scope.leaves[i].order);
    out += ",\"ipix\":";
    out += std::to_string(issue.scope.leaves[i].ipix);
    out.push_back('}');
  }
  out += "]},\"origin\":";
  write_string(out, issue.origin);
  out += ",\"reason\":";
  write_string(out, issue.reason);
  out += ",\"impact\":";
  write_string(out, issue.impact);
  out += ",\"retry_condition\":";
  write_string(out, issue.retry_condition);
  out.push_back('}');
}

void write_assessment(std::string& out, const Assessment& assessment) {
  out += "{\"complete\":";
  out += assessment.complete ? "true" : "false";
  out += ",\"validity\":";
  write_string(out, validity_name(assessment.validity));
  out += ",\"science\":";
  write_string(out, science_name(assessment.science));
  out += ",\"noise\":";
  write_string(out, noise_name(assessment.noise));
  out += ",\"included\":[";
  for (std::size_t i = 0; i < assessment.included.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_string(out, assessment.included[i]);
  }
  out += "],\"omitted\":[";
  for (std::size_t i = 0; i < assessment.omitted.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_string(out, assessment.omitted[i]);
  }
  out += "],\"omitted_position_bound_rad\":";
  if (assessment.omitted_position_bound_rad)
    out += std::to_string(*assessment.omitted_position_bound_rad);
  else
    out += "null";
  out += ",\"issues\":[";
  for (std::size_t i = 0; i < assessment.issues.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_issue(out, assessment.issues[i]);
  }
  out += "],\"condition\":{\"estimator\":";
  write_string(out, assessment.condition.estimator);
  out += ",\"kind\":";
  write_string(out, assessment.condition.kind);
  out += ",\"fixed_objects\":[";
  for (std::size_t i = 0; i < assessment.condition.fixed_objects.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_string(out, assessment.condition.fixed_objects[i]);
  }
  out += "],\"assumptions\":[";
  for (std::size_t i = 0; i < assessment.condition.assumptions.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_string(out, assessment.condition.assumptions[i]);
  }
  out += "]},\"execution\":";
  write_string(out, execution_name(assessment.execution));
  out += ",\"capability\":";
  write_string(out, capability_name(assessment.capability));
  out.push_back('}');
}

void write_type_id(std::string& out, const TypeId& type) {
  out += "{\"schema\":";
  write_string(out, type.schema);
  out += ",\"revision\":";
  out += std::to_string(type.revision);
  out.push_back('}');
}

void write_block(std::string& out, const BlockDescriptor& block) {
  out += "{\"id\":";
  write_string(out, block.id);
  out += ",\"path\":";
  write_string(out, block.path);
  out += ",\"dtype\":";
  write_string(out, dtype_name(block.dtype));
  out += ",\"shape\":[";
  for (std::size_t i = 0; i < block.shape.size(); ++i) {
    if (i != 0) out.push_back(',');
    write_u64(out, block.shape[i]);
  }
  out += "],\"index_id\":";
  write_string(out, block.index_id);
  out += ",\"unit\":";
  write_unit(out, block.unit);
  out += ",\"begin\":";
  write_u64(out, block.begin);
  out += ",\"count\":";
  write_u64(out, block.count);
  out += ",\"codec\":";
  write_string(out, codec_name(block.codec));
  out += ",\"raw_bytes\":";
  write_u64(out, block.raw_bytes);
  out += ",\"stored_bytes\":";
  write_u64(out, block.stored_bytes);
  out += ",\"raw_sha256\":";
  write_string(out, block.raw_sha256);
  out.push_back('}');
}

std::string serialize_response(const WireResponse& response) {
  std::string out;
  out.reserve(4096);
  out += "{\n  \"protocol\": \"acsd.process/2\",\n  \"request_id\": ";
  write_string(out, response.request_id);
  out += ",\n  \"process_id\": ";
  write_string(out, response.process);
  out += ",\n  \"operation_id\": ";
  write_string(out, response.operation);
  out += ",\n  \"invocation\": ";
  write_string(out, response.invocation);
  out += ",\n  \"input_hash\": ";
  write_string(out, response.input_hash);
  out += ",\n  \"request_hash\": ";
  write_string(out, response.request_hash);
  out += ",\n  \"backend\": ";
  write_string(out, response.backend);
  out += ",\n  \"output_type\": ";
  write_type_id(out, response.output_type);
  out += ",\n  \"result_code\": ";
  write_string(out, code_name(response.result_code));
  out += ",\n  \"assessment\": ";
  write_assessment(out, response.assessment);
  out += ",\n  \"resources\": {\"held\":{";
  out += "\"host_bytes\":" + std::to_string(response.resources.held.host_bytes);
  out += ",\"pinned_bytes\":" +
         std::to_string(response.resources.held.pinned_bytes);
  out += ",\"device_bytes\":" +
         std::to_string(response.resources.held.device_bytes);
  out += ",\"spill_bytes\":" +
         std::to_string(response.resources.held.spill_bytes);
  out += ",\"cpu_tokens\":" +
         std::to_string(response.resources.held.cpu_tokens);
  out += ",\"io_slots\":" +
         std::to_string(response.resources.held.io_slots);
  out += "},\"peak_allocated\":{";
  out += "\"host_bytes\":" +
         std::to_string(response.resources.peak_allocated.host_bytes);
  out += ",\"pinned_bytes\":" +
         std::to_string(response.resources.peak_allocated.pinned_bytes);
  out += ",\"device_bytes\":" +
         std::to_string(response.resources.peak_allocated.device_bytes);
  out += ",\"spill_bytes\":" +
         std::to_string(response.resources.peak_allocated.spill_bytes);
  out += ",\"cpu_tokens\":" +
         std::to_string(response.resources.peak_allocated.cpu_tokens);
  out += ",\"io_slots\":" +
         std::to_string(response.resources.peak_allocated.io_slots);
  out += "},\"work\":";
  write_u64(out, response.resources.work);
  out += ",\"read_bytes\":";
  write_u64(out, response.resources.read_bytes);
  out += ",\"written_bytes\":";
  write_u64(out, response.resources.written_bytes);
  out += ",\"spill_written_bytes\":";
  write_u64(out, response.resources.spill_written_bytes);
  out += ",\"cleanup_work\":";
  write_u64(out, response.resources.cleanup_work);
  out += ",\"elapsed_ns\":";
  write_u64(out, response.resources.elapsed_ns);
  out += "},\n  \"dependencies\": [";
  for (std::size_t i = 0; i < response.dependencies.size(); ++i) {
    if (i != 0) out += ", ";
    write_string(out, response.dependencies[i]);
  }
  out += "],\n  \"value\": ";
  if (response.value_json && !response.value_json->empty())
    out += *response.value_json;
  else
    out += "null";
  out += ",\n  \"objects\": ";
  if (!response.objects_json.empty())
    out += response.objects_json;
  else
    out += "{}";
  out += ",\n  \"blocks\": [";
  for (std::size_t i = 0; i < response.blocks.size(); ++i) {
    if (i != 0) out += ", ";
    write_block(out, response.blocks[i]);
  }
  out += "],\n  \"artifact_lease\": null\n}\n";
  return out;
}

}  // namespace
}  // namespace detail

// ---------------------------------------------------------------------------
// WireObjects
// ---------------------------------------------------------------------------

WireObjects::~WireObjects() = default;

Result<Id> WireObjects::alias_for_erased(std::shared_ptr<const void> object,
                                        const void* cookie) const {
  Result<Id> out;
  if (!table_ || !object) {
    out.code = Code::invalid_request;
    return out;
  }
  std::lock_guard<std::mutex> lock(table_->mutex);
  std::optional<Id> found;
  for (const auto& item : table_->aliases) {
    if (item.second.object.get() != object.get()) continue;
    if (item.second.cookie != cookie) continue;
    if (found) {
      // Two aliases of the same (object, interface) pair are ambiguous; the
      // producer must bind one alias per interface field.
      out.code = Code::identity_conflict;
      return out;
    }
    found = item.first;
  }
  if (!found) {
    out.code = Code::missing_dependency;
    return out;
  }
  out.code = Code::ok;
  out.value = std::move(*found);
  return out;
}

const void* WireObjects::cookie_erased(Id alias) const {
  if (!table_) return nullptr;
  std::lock_guard<std::mutex> lock(table_->mutex);
  auto found = table_->aliases.find(alias);
  if (found == table_->aliases.end()) return nullptr;
  return found->second.cookie;
}

Result<std::shared_ptr<const void>> WireObjects::resolve_erased(Id alias) const {
  Result<std::shared_ptr<const void>> out;
  if (!table_) {
    out.code = Code::invalid_request;
    return out;
  }
  std::lock_guard<std::mutex> lock(table_->mutex);
  auto found = table_->aliases.find(alias);
  if (found == table_->aliases.end()) {
    out.code = Code::missing_dependency;
    return out;
  }
  out.code = Code::ok;
  out.value = found->second.object;
  return out;
}

Result<Done> WireObjects::bind_erased(Id alias, std::shared_ptr<const void> object,
                                      const void* cookie) {
  if (!table_ || alias.empty() || !object)
    return detail::wire_error(Code::invalid_request, "wire.bind",
                              "bind needs a live object table, a non-empty "
                              "alias and a non-null object");
  std::lock_guard<std::mutex> lock(table_->mutex);
  auto found = table_->aliases.find(alias);
  if (found != table_->aliases.end()) {
    if (found->second.cookie != cookie)
      return detail::wire_error(
          Code::identity_conflict, "wire.bind",
          "alias '" + alias +
              "' is already bound to a different C++ type; one request-local "
              "alias has exactly one type");
    if (found->second.object.get() == object.get()) {
      Result<Done> out;
      out.code = Code::ok;
      out.value = Done{};
      return out;
    }
    return detail::wire_error(
        Code::identity_conflict, "wire.bind",
        "alias '" + alias +
            "' is already bound to another object; aliases are immutable "
            "within one request/response object table");
  }
  table_->aliases.emplace(std::move(alias),
                          detail::WireObjectTable::Entry{std::move(object),
                                                         cookie});
  Result<Done> out;
  out.code = Code::ok;
  out.value = Done{};
  return out;
}

Result<Id> WireObjects::intern_erased(std::shared_ptr<const void> object,
                                      const void* cookie) {
  Result<Id> out;
  if (!table_ || !object) {
    out.code = Code::invalid_request;
    return out;
  }
  std::lock_guard<std::mutex> lock(table_->mutex);
  auto found = table_->interned.find(object.get());
  if (found != table_->interned.end()) {
    auto entry = table_->aliases.find(found->second);
    if (entry != table_->aliases.end() && entry->second.cookie != cookie) {
      out.code = Code::identity_conflict;
      return out;
    }
    out.code = Code::ok;
    out.value = found->second;
    return out;
  }
  Id id = "object:" + std::to_string(table_->next_id++);
  table_->interned.emplace(object.get(), id);
  table_->aliases.emplace(id, detail::WireObjectTable::Entry{
                                  std::move(object), cookie});
  out.code = Code::ok;
  out.value = std::move(id);
  return out;
}

// ---------------------------------------------------------------------------
// Request-local object table and response publication
// ---------------------------------------------------------------------------

Result<std::shared_ptr<WireObjects>> make_wire_objects(
    InvocationContext ctx, std::shared_ptr<const Registry> registry,
    Ref<WireRequest> request) {
  Result<std::shared_ptr<WireObjects>> out;
  if (!registry || !request) {
    out.code = Code::invalid_request;
    return out;
  }
  if (cancellation_requested(ctx)) {
    out.code = Code::cancelled;
    return out;
  }
  if (!registry->has(request->process, request->operation)) {
    out.code = Code::unsupported;
    return out;
  }
  auto table = std::make_shared<detail::WireObjectTable>();
  auto objects = std::make_shared<WireObjects>();
  objects->table_ = table;

  // Blocks are external transport roots: ids must be unique and non-empty.  The
  // bytes themselves are validated by Storage when the typed codec reads them;
  // the table only records their roots for this request.
  std::set<Id> seen;
  for (const BlockDescriptor& block : request->blocks) {
    if (block.id.empty()) {
      out.code = Code::invalid_model;
      return out;
    }
    if (!seen.insert(block.id).second) {
      out.code = Code::identity_conflict;
      return out;
    }
  }
  // Byte validation and array-root registration belong to Storage; this layer
  // only establishes the request-local roots.  Registering a block twice (or
  // inventing bytes) is rejected by the frozen request identity.

  out.code = Code::ok;
  out.value = std::move(objects);
  return out;
}

Task<Done> publish_wire_response(InvocationContext ctx, Id result_path,
                                 WireResponse response) {
  Result<Done> out;
  if (result_path.empty()) {
    out = detail::wire_error(Code::invalid_request, "wire.publish",
                             "publish_wire_response needs a non-empty "
                             "result_path");
    return ready<Done>(ctx, std::move(out));
  }
  if (cancellation_requested(ctx)) {
    out = detail::wire_error(Code::cancelled, "wire.publish",
                             "the response was not published because the "
                             "invocation was cancelled");
    return ready<Done>(ctx, std::move(out));
  }
  const std::string payload = detail::serialize_response(response);
  const std::filesystem::path target(result_path);
  std::error_code error;
  if (!target.parent_path().empty())
    std::filesystem::create_directories(target.parent_path(), error);
  if (error) {
    out = detail::wire_error(Code::io_error, "wire.publish",
                             "cannot create the result directory: " +
                                 error.message());
    return ready<Done>(ctx, std::move(out));
  }
  static std::atomic<U64> temporary_counter{1};
  const std::filesystem::path temporary =
      target.string() + ".tmp." +
      std::to_string(temporary_counter.fetch_add(1));
  std::filesystem::remove(temporary, error);
  error.clear();
  {
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    if (!stream) {
      out = detail::wire_error(
          Code::io_error, "wire.publish",
          "cannot open the temporary response file: " +
              std::string(std::strerror(errno)));
      return ready<Done>(ctx, std::move(out));
    }
    stream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
    stream.flush();
    if (!stream) {
      stream.close();
      std::filesystem::remove(temporary, error);
      out = detail::wire_error(Code::io_error, "wire.publish",
                               "writing the response payload failed; the "
                               "temporary file was removed");
      return ready<Done>(ctx, std::move(out));
    }
  }
  std::filesystem::rename(temporary, target, error);
  if (error) {
    // On filesystems where rename does not replace an existing target, remove
    // the old file and retry once.  A failure removes the temporary file so no
    // half-written product remains.
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
      std::error_code cleanup;
      std::filesystem::remove(temporary, cleanup);
      out = detail::wire_error(Code::io_error, "wire.publish",
                               "cannot commit the response file atomically: " +
                                   error.message());
      return ready<Done>(ctx, std::move(out));
    }
  }
  out.code = Code::ok;
  out.value = Done{};
  return ready<Done>(ctx, std::move(out));
}

}  // namespace acsd::rt
