// Frozen process registry: typed codec binding, one registration per
// (process_id, operation_id), build-time validation and the stable manifest.
//
// The rules implemented here are fixed by
// docs/engineering/rebuild/standards/FOUNDATION_API.md ("计划、注册和Host",
// "统一wire调用与局部对象表") and docs/engineering/rebuild/SYSTEM.md ("一次登记
// 与typed绑定").  There is exactly one registry per build; the module
// descriptors are the source and the exported manifest is a build product.
#include "internal.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace acsd::rt {

namespace {

// The legitimate static module dependencies come from the CMake target table
// in SYSTEM.md ("源码依赖、构建与交付").  A dependency is either one of these
// modules, a "service.*" service scope, or another registered process (which is
// a real static edge subject to the cycle check).
const std::set<Id>& legal_modules() {
  static const std::set<Id> modules = {
      "model",        "numerics",    "coordinates", "observation",
      "catalogue",    "sources",     "response",    "photometry",
      "geometry",     "operators",   "uncertainty", "information",
      "sky",          "coadd",       "projection",  "storage",
      "resources",    "runtime",     "workflows"};
  return modules;
}

bool starts_with(const std::string& text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

template <class T>
Result<T> registry_failure(Code code, std::string origin, std::string reason,
                           std::string impact = {}) {
  Result<T> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  Issue issue;
  issue.code = code;
  issue.origin = std::move(origin);
  issue.reason = std::move(reason);
  issue.impact = std::move(impact);
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

bool document_exists(const Id& document) {
  if (document.empty()) return false;
  std::error_code ec;
  const std::filesystem::path path(document);
  if (path.is_absolute()) {
    return std::filesystem::is_regular_file(path, ec);
  }
  // Descriptors name the process document repository-relative.  The build and
  // the tests run from the source root; ACSD_SOURCE_ROOT is an explicit
  // fallback for out-of-tree invocations of the manifest exporter.
  if (std::filesystem::is_regular_file(path, ec)) return true;
  ec.clear();
  if (const char* root = std::getenv("ACSD_SOURCE_ROOT"); root && *root) {
    return std::filesystem::is_regular_file(std::filesystem::path(root) / path,
                                            ec);
  }
  ec.clear();
#if defined(ACSD_SOURCE_ROOT)
  // The build defines the checkout root for the runtime translation units, so a
  // manifest exporter invoked from the build directory can still verify the
  // repository-relative process documents.
  return std::filesystem::is_regular_file(
      std::filesystem::path(ACSD_SOURCE_ROOT) / path, ec);
#else
  return false;
#endif
}

// ---------------------------------------------------------------------------
// Stable manifest JSON (hand written: runtime links no JSON library)
// ---------------------------------------------------------------------------

void append_json_string(std::string& out, std::string_view text) {
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

void append_type_id(std::string& out, const TypeId& type) {
  out += "{\"schema\":";
  append_json_string(out, type.schema);
  out += ",\"revision\":";
  out += std::to_string(type.revision);
  out.push_back('}');
}

struct ProcessView {
  const ProcessDescriptor* descriptor = nullptr;
  std::vector<const detail::ErasedOperation*> operations;
};

std::string build_manifest(const std::vector<ProcessView>& processes) {
  std::string out;
  out.reserve(4096);
  out += "{\n  \"protocol\": \"acsd.process/2\",\n";
  out += "  \"backend\": \"cpu\",\n";
  out += "  \"processes\": [";
  for (std::size_t i = 0; i < processes.size(); ++i) {
    const ProcessView& view = processes[i];
    const ProcessDescriptor& descriptor = *view.descriptor;
    out += i == 0 ? "\n" : ",\n";
    out += "    {\n      \"process\": ";
    append_json_string(out, descriptor.process_id);
    out += ",\n      \"module\": ";
    append_json_string(out, descriptor.module);
    out += ",\n      \"semantic_revision\": ";
    out += std::to_string(descriptor.semantic_revision);
    out += ",\n      \"document\": ";
    append_json_string(out, descriptor.document);
    out += ",\n      \"dependencies\": [";
    for (std::size_t d = 0; d < descriptor.dependencies.size(); ++d) {
      if (d != 0) out += ", ";
      append_json_string(out, descriptor.dependencies[d]);
    }
    out += "],\n      \"probes\": [";
    std::vector<ProbeDeclaration> probes = descriptor.probes;
    std::sort(probes.begin(), probes.end(),
              [](const ProbeDeclaration& a, const ProbeDeclaration& b) {
                return a.id < b.id;
              });
    for (std::size_t p = 0; p < probes.size(); ++p) {
      if (p != 0) out += ", ";
      out += "{\"id\": ";
      append_json_string(out, probes[p].id);
      out += ", \"kind\": ";
      append_json_string(out, probes[p].kind);
      out += ", \"semantic_revision\": ";
      out += std::to_string(probes[p].semantic_revision);
      out.push_back('}');
    }
    out += "],\n      \"operations\": [";
    for (std::size_t o = 0; o < view.operations.size(); ++o) {
      const detail::ErasedOperation& operation = *view.operations[o];
      out += o == 0 ? "\n" : ",\n";
      out += "        {\"operation\": ";
      append_json_string(out, operation.operation_id);
      out += ", \"input_type\": ";
      append_type_id(out, operation.input_type);
      out += ", \"output_type\": ";
      append_type_id(out, operation.output_type);
      out += ", \"backends\": [\"cpu\"]}";
    }
    out += view.operations.empty() ? "]\n    }" : "\n      ]\n    }";
  }
  out += processes.empty() ? "]\n}\n" : "\n  ]\n}\n";
  return out;
}

}  // namespace

namespace detail {

// ---------------------------------------------------------------------------
// Builder core
// ---------------------------------------------------------------------------

class BuilderCore {
 public:
  std::mutex mutex;
  bool frozen = false;
  std::vector<ErasedCodec> types;
  std::vector<ProcessDescriptor> processes;
  struct RegisteredOperation {
    Id process;
    ErasedOperation operation;
  };
  std::vector<RegisteredOperation> operations;
};

}  // namespace detail

struct ProcessRegistration::Impl {
  std::shared_ptr<detail::BuilderCore> builder;
  ProcessDescriptor descriptor;
};

ProcessRegistration::ProcessRegistration(std::shared_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

namespace detail {

Result<Done> register_type_erased(std::shared_ptr<BuilderCore> builder,
                                  ErasedCodec codec) {
  if (!builder)
    return registry_failure<Done>(Code::invalid_request, "registry.register_type",
                                  "the registry builder is not initialized");
  std::lock_guard<std::mutex> lock(builder->mutex);
  if (builder->frozen)
    return registry_failure<Done>(
        Code::invalid_request, "registry.register_type",
        "the registry builder is frozen; types must be registered before "
        "freeze()");
  builder->types.push_back(std::move(codec));
  Result<Done> out;
  out.code = Code::ok;
  out.value = Done{};
  return out;
}

Result<std::shared_ptr<ProcessRegistration>> begin_process(
    std::shared_ptr<BuilderCore> builder, ProcessDescriptor descriptor) {
  if (!builder)
    return registry_failure<std::shared_ptr<ProcessRegistration>>(
        Code::invalid_request, "registry.begin",
        "the registry builder is not initialized");
  std::lock_guard<std::mutex> lock(builder->mutex);
  if (builder->frozen)
    return registry_failure<std::shared_ptr<ProcessRegistration>>(
        Code::invalid_request, "registry.begin",
        "the registry builder is frozen; processes must be registered before "
        "freeze()");
  auto impl = std::make_shared<ProcessRegistration::Impl>();
  impl->builder = builder;
  impl->descriptor = descriptor;
  builder->processes.push_back(std::move(descriptor));
  std::shared_ptr<ProcessRegistration> registration(
      new ProcessRegistration(std::move(impl)));
  Result<std::shared_ptr<ProcessRegistration>> out;
  out.code = Code::ok;
  out.value = std::move(registration);
  return out;
}

Result<Done> add_operation_erased(
    std::shared_ptr<BuilderCore> builder,
    std::shared_ptr<ProcessRegistration> registration,
    ErasedOperation operation) {
  if (!builder || !registration)
    return registry_failure<Done>(
        Code::invalid_request, "registry.add",
        "add() needs a live ProcessRegistration created by begin()");
  std::lock_guard<std::mutex> lock(builder->mutex);
  if (builder->frozen)
    return registry_failure<Done>(
        Code::invalid_request, "registry.add",
        "the registry builder is frozen; operations must be registered before "
        "freeze()");
  if (registration->impl_ == nullptr ||
      registration->impl_->builder.get() != builder.get())
    return registry_failure<Done>(
        Code::invalid_request, "registry.add",
        "the ProcessRegistration belongs to a different registry builder");
  BuilderCore::RegisteredOperation record;
  record.process = registration->impl_->descriptor.process_id;
  record.operation = std::move(operation);
  builder->operations.push_back(std::move(record));
  Result<Done> out;
  out.code = Code::ok;
  out.value = Done{};
  return out;
}

namespace {

using TypeKey = std::pair<Id, U32>;

Result<Done> validate_duplicate_processes(
    const std::vector<ProcessDescriptor>& processes) {
  std::set<Id> seen;
  for (const ProcessDescriptor& descriptor : processes) {
    if (descriptor.process_id.empty())
      return registry_failure<Done>(Code::invalid_model, "registry.freeze",
                                    "a ProcessDescriptor has an empty "
                                    "process_id");
    if (!seen.insert(descriptor.process_id).second)
      return registry_failure<Done>(
          Code::identity_conflict, "registry.freeze",
          "duplicate process registration for process_id '" +
              descriptor.process_id + "'");
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_duplicate_operations(
    const std::vector<BuilderCore::RegisteredOperation>& operations) {
  std::set<std::pair<Id, Id>> seen;
  for (const auto& record : operations) {
    if (record.operation.operation_id.empty())
      return registry_failure<Done>(Code::invalid_model, "registry.freeze",
                                    "process '" + record.process +
                                        "' has an operation with an empty "
                                        "operation_id");
    if (!seen.insert({record.process, record.operation.operation_id}).second)
      return registry_failure<Done>(
          Code::identity_conflict, "registry.freeze",
          "duplicate operation '" + record.operation.operation_id +
              "' in process '" + record.process + "'");
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_types(const std::vector<ErasedCodec>& types,
                            std::map<TypeKey, const ErasedCodec*>& by_id) {
  std::map<TypeKey, const void*> cookie_by_id;
  std::map<const void*, TypeKey> id_by_cookie;
  for (const ErasedCodec& codec : types) {
    if (codec.id.schema.empty())
      return registry_failure<Done>(Code::invalid_model, "registry.freeze",
                                    "a TypeCodec has an empty TypeId schema");
    if (codec.schema_json.empty())
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "TypeCodec '" + codec.id.schema +
              "' has an empty schema_json; a wire type must carry its schema");
    if (!codec.decode || !codec.encode)
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "TypeCodec '" + codec.id.schema +
              "' is missing its decode and/or encode hook");
    if (codec.cookie == nullptr)
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "TypeCodec '" + codec.id.schema +
              "' has no C++ type cookie; registration may only be created "
              "through RegistryBuilder::register_type<T>");
    TypeKey key{codec.id.schema, codec.id.revision};
    auto [slot, inserted] = by_id.emplace(key, &codec);
    if (!inserted) {
      bool same_cookie = slot->second->cookie == codec.cookie;
      return registry_failure<Done>(
          Code::identity_conflict, "registry.freeze",
          "duplicate TypeId '" + codec.id.schema + "'/revision " +
              std::to_string(codec.id.revision) + (same_cookie
                  ? " registered twice"
                  : " is bound to two different C++ types"));
    }
    auto [cookie_slot, cookie_inserted] =
        id_by_cookie.emplace(codec.cookie, key);
    if (!cookie_inserted && (cookie_slot->second != key))
      return registry_failure<Done>(
          Code::identity_conflict, "registry.freeze",
          "one C++ type is bound to two TypeIds ('" + cookie_slot->second.first +
              "' and '" + codec.id.schema +
              "'); a TypeId and a C++ type are one-to-one");
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_descriptors(const std::vector<ProcessDescriptor>& processes,
                                  const std::set<Id>& process_ids) {
  std::map<Id, Id> document_owner;
  for (const ProcessDescriptor& descriptor : processes) {
    if (!descriptor.document.empty()) {
      auto [slot, inserted] =
          document_owner.emplace(descriptor.document, descriptor.process_id);
      if (!inserted)
        return registry_failure<Done>(
            Code::identity_conflict, "registry.freeze",
            "process '" + descriptor.process_id + "' and process '" +
                slot->second + "' share document '" + descriptor.document +
                "'; a process owns one unique document");
    }
    if (descriptor.module.empty())
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "process '" + descriptor.process_id + "' has an empty module");
    if (descriptor.semantic_revision == 0)
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "process '" + descriptor.process_id +
              "' has semantic_revision 0; a scientific identity starts at 1");
    if (descriptor.document.empty())
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "process '" + descriptor.process_id + "' declares no document");
    if (!document_exists(descriptor.document))
      return registry_failure<Done>(
          Code::missing_dependency, "registry.freeze",
          "process '" + descriptor.process_id + "' document '" +
              descriptor.document +
              "' does not exist; every process needs its unique document");
    std::set<Id> probe_ids;
    for (const ProbeDeclaration& probe : descriptor.probes) {
      if (probe.id.empty())
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            "process '" + descriptor.process_id + "' declares a probe with an "
            "empty id");
      if (probe.kind.empty())
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            "process '" + descriptor.process_id + "' probe '" + probe.id +
                "' has an empty kind");
      if (probe.semantic_revision == 0)
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            "process '" + descriptor.process_id + "' probe '" + probe.id +
                "' has semantic_revision 0");
      if (!probe_ids.insert(probe.id).second)
        return registry_failure<Done>(
            Code::identity_conflict, "registry.freeze",
            "process '" + descriptor.process_id + "' declares probe '" +
                probe.id + "' twice");
    }
    for (const Id& dependency : descriptor.dependencies) {
      if (dependency.empty())
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            "process '" + descriptor.process_id +
                "' declares an empty dependency");
      if (process_ids.count(dependency) != 0) continue;  // static process edge
      if (legal_modules().count(dependency) != 0) continue;
      if (starts_with(dependency, "service.")) continue;
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "process '" + descriptor.process_id + "' depends on unknown module '" +
              dependency +
              "'; legal modules are the CMake targets in SYSTEM.md plus "
              "\"service.*\" and registered process ids");
    }
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_process_operation_coverage(
    const std::vector<ProcessDescriptor>& processes,
    const std::vector<BuilderCore::RegisteredOperation>& operations) {
  for (const ProcessDescriptor& descriptor : processes) {
    bool any = false;
    for (const auto& record : operations)
      if (record.process == descriptor.process_id) {
        any = true;
        break;
      }
    if (!any)
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          "process '" + descriptor.process_id +
              "' registers no operation; an empty process is not a scientific "
              "step");
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_probe_owners(
    const std::vector<ProcessDescriptor>& processes) {
  std::map<Id, Id> owner;
  for (const ProcessDescriptor& descriptor : processes) {
    for (const ProbeDeclaration& probe : descriptor.probes) {
      auto [slot, inserted] = owner.emplace(probe.id, descriptor.process_id);
      if (!inserted)
        return registry_failure<Done>(
            Code::identity_conflict, "registry.freeze",
            "probe '" + probe.id + "' is declared by both process '" +
                slot->second + "' and process '" + descriptor.process_id +
                "'; a probe has exactly one owner");
    }
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_dependency_cycles(
    const std::vector<ProcessDescriptor>& processes,
    const std::set<Id>& process_ids) {
  std::map<Id, std::vector<Id>> edges;
  for (const ProcessDescriptor& descriptor : processes)
    for (const Id& dependency : descriptor.dependencies)
      if (process_ids.count(dependency) != 0)
        edges[descriptor.process_id].push_back(dependency);

  std::map<Id, int> color;  // 0 unseen, 1 on stack, 2 done
  std::vector<Id> stack;
  std::function<Result<Done>(const Id&)> visit =
      [&](const Id& node) -> Result<Done> {
    color[node] = 1;
    stack.push_back(node);
    for (const Id& next : edges[node]) {
      if (color[next] == 1) {
        std::ostringstream path;
        bool in_cycle = false;
        for (const Id& id : stack) {
          if (id == next) in_cycle = true;
          if (!in_cycle) continue;
          if (path.tellp() > 0) path << " -> ";
          path << id;
        }
        path << " -> " << next;
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            "static process dependency cycle: " + path.str());
      }
      if (color[next] == 0) {
        Result<Done> nested = visit(next);
        if (nested.code != Code::ok) return nested;
      }
    }
    stack.pop_back();
    color[node] = 2;
    return Result<Done>{Code::ok, Done{}, {}, {}, {}};
  };
  for (const ProcessDescriptor& descriptor : processes) {
    if (color[descriptor.process_id] == 0) {
      Result<Done> result = visit(descriptor.process_id);
      if (result.code != Code::ok) return result;
    }
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

Result<Done> validate_operations(
    const std::vector<BuilderCore::RegisteredOperation>& operations,
    const std::map<TypeKey, const ErasedCodec*>& codecs) {
  for (const auto& record : operations) {
    const ErasedOperation& operation = record.operation;
    const std::string label =
        "process '" + record.process + "' operation '" +
        operation.operation_id + "'";
    if (!operation.validate || !operation.bounds || !operation.plan ||
        !operation.run || !operation.finalize)
      return registry_failure<Done>(
          Code::invalid_model, "registry.freeze",
          label + " does not provide the complete validate/bounds/plan/run/"
                  "finalize hook set");
    if (operation.input_type.schema.empty())
      return registry_failure<Done>(Code::invalid_model, "registry.freeze",
                                    label + " has an empty input TypeId");
    if (operation.output_type.schema.empty())
      return registry_failure<Done>(Code::invalid_model, "registry.freeze",
                                    label + " has an empty output TypeId");
    auto check_binding = [&](const TypeId& type, const void* cookie,
                             const char* direction) -> Result<Done> {
      auto found = codecs.find({type.schema, type.revision});
      if (found == codecs.end())
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            label + " references " + direction + " type '" + type.schema +
                "'/revision " + std::to_string(type.revision) +
                " but no codec is registered for that TypeId");
      if (found->second->cookie != cookie)
        return registry_failure<Done>(
            Code::invalid_model, "registry.freeze",
            label + " " + direction + " TypeId '" + type.schema +
                "' is bound to a different C++ type than the operation's " +
                direction + " parameter; the typed binding does not match");
      return Result<Done>{Code::ok, Done{}, {}, {}, {}};
    };
    Result<Done> input = check_binding(operation.input_type,
                                       operation.input_cookie, "input");
    if (input.code != Code::ok) return input;
    Result<Done> output = check_binding(operation.output_type,
                                        operation.output_cookie, "output");
    if (output.code != Code::ok) return output;
  }
  return Result<Done>{Code::ok, Done{}, {}, {}, {}};
}

}  // namespace

Result<std::shared_ptr<const Registry>> freeze_registry(
    std::shared_ptr<BuilderCore> builder) {
  if (!builder)
    return registry_failure<std::shared_ptr<const Registry>>(
        Code::invalid_request, "registry.freeze",
        "the registry builder is not initialized");
  std::lock_guard<std::mutex> lock(builder->mutex);
  if (builder->frozen)
    return registry_failure<std::shared_ptr<const Registry>>(
        Code::identity_conflict, "registry.freeze",
        "the registry is already frozen; freeze() may succeed exactly once");

  std::set<Id> process_ids;
  for (const ProcessDescriptor& descriptor : builder->processes)
    process_ids.insert(descriptor.process_id);

  std::map<TypeKey, const ErasedCodec*> codecs;
  const std::vector<Result<Done> (*)(
      const std::vector<ProcessDescriptor>&, const std::set<Id>&)>
      descriptor_checks = {validate_descriptors};
  (void)descriptor_checks;

  auto fail = [](const Result<Done>& failure)
      -> Result<std::shared_ptr<const Registry>> {
    return registry_failure<std::shared_ptr<const Registry>>(
        failure.code, "registry.freeze",
        failure.assessment.issues.empty()
            ? "registry validation failed"
            : failure.assessment.issues.front().reason);
  };

  Result<Done> step = validate_duplicate_processes(builder->processes);
  if (step.code != Code::ok) return fail(step);
  step = validate_duplicate_operations(builder->operations);
  if (step.code != Code::ok) return fail(step);
  step = validate_types(builder->types, codecs);
  if (step.code != Code::ok) return fail(step);
  step = validate_descriptors(builder->processes, process_ids);
  if (step.code != Code::ok) return fail(step);
  step = validate_process_operation_coverage(builder->processes,
                                             builder->operations);
  if (step.code != Code::ok) return fail(step);
  step = validate_probe_owners(builder->processes);
  if (step.code != Code::ok) return fail(step);
  step = validate_dependency_cycles(builder->processes, process_ids);
  if (step.code != Code::ok) return fail(step);
  step = validate_operations(builder->operations, codecs);
  if (step.code != Code::ok) return fail(step);

  auto core = std::make_shared<RegistryCore>();
  for (const auto& record : builder->operations) {
    RegistryCore::Entry entry;
    for (const ProcessDescriptor& candidate : builder->processes)
      if (candidate.process_id == record.process) entry.descriptor = candidate;
    entry.erased = record.operation;
    core->operations.emplace(
        std::make_pair(record.process, record.operation.operation_id),
        std::move(entry));
  }
  for (const ErasedCodec& codec : builder->types)
    core->codecs.emplace(std::make_pair(codec.id.schema, codec.id.revision),
                         codec);

  // One sorted view per process; processes/operations sorted by their stable
  // ids so the manifest is byte-identical for the same frozen registry.
  std::vector<ProcessView> views;
  for (const ProcessDescriptor& descriptor : builder->processes) {
    ProcessView view;
    view.descriptor = &descriptor;
    views.push_back(view);
  }
  std::sort(views.begin(), views.end(),
            [](const ProcessView& a, const ProcessView& b) {
              return a.descriptor->process_id < b.descriptor->process_id;
            });
  for (ProcessView& view : views) {
    for (const auto& record : builder->operations)
      if (record.process == view.descriptor->process_id)
        view.operations.push_back(&record.operation);
    std::sort(view.operations.begin(), view.operations.end(),
              [](const ErasedOperation* a, const ErasedOperation* b) {
                return a->operation_id < b->operation_id;
              });
  }
  core->manifest_json = build_manifest(views);

  builder->frozen = true;
  std::shared_ptr<const Registry> registry(new Registry(std::move(core)));
  Result<std::shared_ptr<const Registry>> out;
  out.code = Code::ok;
  out.value = std::move(registry);
  return out;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Public registry facade
// ---------------------------------------------------------------------------

RegistryBuilder::RegistryBuilder()
    : core_(std::make_shared<detail::BuilderCore>()) {}

RegistryBuilder::~RegistryBuilder() = default;

Result<std::shared_ptr<ProcessRegistration>> RegistryBuilder::begin(
    ProcessDescriptor descriptor) {
  return detail::begin_process(core_, std::move(descriptor));
}

Result<std::shared_ptr<const Registry>> RegistryBuilder::freeze() {
  return detail::freeze_registry(core_);
}

ProcessRegistration::~ProcessRegistration() = default;

Registry::~Registry() = default;

Registry::Registry(std::shared_ptr<const detail::RegistryCore> core) noexcept
    : core_(std::move(core)) {}

bool Registry::has(Id process, Id operation) const {
  if (!core_) return false;
  return core_->operations.count(
             {std::move(process), std::move(operation)}) != 0;
}

std::string Registry::manifest_json() const {
  if (!core_)
    return "{\n  \"protocol\": \"acsd.process/2\",\n  \"backend\": \"cpu\",\n"
           "  \"processes\": []\n}\n";
  return core_->manifest_json;
}

}  // namespace acsd::rt
