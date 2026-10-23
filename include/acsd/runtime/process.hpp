#pragma once
// Fixed process descriptor, typed binding, registry and ProcessHost.
//
// A module declares one read-only ProcessDescriptor and one Operation per
// operation_id; the registry is frozen once and then invoked through the same
// typed binding for in-process, wire and workflow calls.  Host state machine:
// created -> validated -> planned -> running -> finalized -> completed, with
// failed / cancelling -> cancelled terminal states.
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/types.hpp>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace acsd::rt {

class WireObjects;
class ProcessRegistration;
class Registry;
class ProcessHost;

struct TypeId {
  Id schema;
  U32 revision = 1;
};

struct WireRequest {
  Id request_id, process, operation;
  TypeId input_type;
  std::string input_json, input_objects_json;
  Id input_root, artifact_root;
  std::vector<BlockDescriptor> blocks;
};

struct WireResponse {
  Id request_id, process, operation, invocation;
  TypeId output_type;
  Code result_code = Code::internal;
  Assessment assessment;
  ScopeSnapshot resources;
  std::vector<Id> dependencies;
  std::optional<std::string> value_json;
  std::string objects_json;
  std::vector<BlockDescriptor> blocks;
  Lease artifact_lease;
};

struct ProbeDeclaration {
  Id id, kind;
  U32 semantic_revision = 1;
};

struct ProcessDescriptor {
  Id process_id, module, document;
  U32 semantic_revision = 1;
  std::vector<Id> dependencies;
  std::vector<ProbeDeclaration> probes;
};

template <class In, class Out>
struct Operation {
  Id operation_id;
  TypeId input_type, output_type;
  Result<Done> (*validate)(InvocationContext, Ref<In>);
  Result<PlanBounds> (*bounds)(InvocationContext, Ref<In>);
  Result<InvocationPlan> (*plan)(InvocationContext, Ref<In>, PlanOptions);
  Task<Out> (*run)(InvocationContext, Ref<In>, Ref<InvocationPlan>);
  Task<Out> (*finalize)(InvocationContext, Ref<In>, Ref<InvocationPlan>,
                        Result<Out>);
};

template <class T>
struct TypeCodec {
  TypeId id;
  std::string_view schema_json;
  Result<T> (*decode)(InvocationContext, std::string_view, WireObjects&) =
      nullptr;
  Result<std::string> (*encode)(InvocationContext, const T&, WireObjects&) =
      nullptr;
};

namespace detail {

template <class T>
const void* type_cookie() noexcept {
  static const int cookie = 0;
  return &cookie;
}

class BuilderCore;
class RegistryCore;
class HostCore;
struct WireObjectTable;

struct ErasedCodec {
  TypeId id;
  const void* cookie = nullptr;
  std::string schema_json;
  std::function<Result<std::shared_ptr<const void>>(InvocationContext,
                                                   std::string_view,
                                                   WireObjects&)>
      decode;
  std::function<Result<std::string>(InvocationContext,
                                    std::shared_ptr<const void>)>
      encode;
};

struct ErasedOperation {
  Id operation_id;
  TypeId input_type, output_type;
  const void* input_cookie = nullptr;
  const void* output_cookie = nullptr;
  std::function<Result<Done>(InvocationContext, std::shared_ptr<const void>)>
      validate;
  std::function<Result<PlanBounds>(InvocationContext,
                                   std::shared_ptr<const void>)>
      bounds;
  std::function<Result<InvocationPlan>(InvocationContext,
                                       std::shared_ptr<const void>,
                                       PlanOptions)>
      plan;
  std::function<Task<std::shared_ptr<const void>>(
      InvocationContext, std::shared_ptr<const void>, Ref<InvocationPlan>)>
      run;
  std::function<Task<std::shared_ptr<const void>>(
      InvocationContext, std::shared_ptr<const void>, Ref<InvocationPlan>,
      Result<std::shared_ptr<const void>>)>
      finalize;
};

// Type-erased bridge between the templates below and the frozen registry.
Result<Done> register_type_erased(std::shared_ptr<BuilderCore>, ErasedCodec);
Result<std::shared_ptr<ProcessRegistration>> begin_process(
    std::shared_ptr<BuilderCore>, ProcessDescriptor);
Result<Done> add_operation_erased(std::shared_ptr<BuilderCore>,
                                  std::shared_ptr<ProcessRegistration>,
                                  ErasedOperation);
Result<std::shared_ptr<const Registry>> freeze_registry(
    std::shared_ptr<BuilderCore>);

Task<std::shared_ptr<const void>> host_invoke_erased(
    std::shared_ptr<HostCore>, InvocationContext, Id process, Id operation,
    std::shared_ptr<const void> input, const void* input_cookie,
    const void* output_cookie);

template <class T, class U>
Result<U> remap_failure(const Result<T>& from) {
  Result<U> out;
  out.code = from.code;
  out.assessment = from.assessment;
  out.usage = from.usage;
  out.dependencies = from.dependencies;
  return out;
}

template <class T>
std::shared_ptr<const void> box_owned(T value) {
  return std::static_pointer_cast<const void>(
      std::make_shared<T>(std::move(value)));
}

}  // namespace detail

class ProcessRegistration {
 public:
  ProcessRegistration() noexcept = default;
  ProcessRegistration(ProcessRegistration&&) noexcept = default;
  ProcessRegistration& operator=(ProcessRegistration&&) noexcept = default;
  ProcessRegistration(const ProcessRegistration&) = delete;
  ProcessRegistration& operator=(const ProcessRegistration&) = delete;
  ~ProcessRegistration();

 private:
  struct Impl;
  explicit ProcessRegistration(std::shared_ptr<Impl> impl) noexcept;
  std::shared_ptr<Impl> impl_;
  friend class RegistryBuilder;
  friend Result<Done> detail::add_operation_erased(
      std::shared_ptr<detail::BuilderCore>,
      std::shared_ptr<ProcessRegistration>, detail::ErasedOperation);
};

class Registry {
 public:
  Registry() noexcept = default;
  Registry(Registry&&) noexcept = default;
  Registry& operator=(Registry&&) noexcept = default;
  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;
  ~Registry();

  bool has(Id process, Id operation) const;
  // Stable JSON used by the build to write process-manifest.json and by the
  // coverage check; it is a build product, not a second handwritten registry.
  std::string manifest_json() const;

 private:
  struct Impl;
  explicit Registry(std::shared_ptr<const Impl> impl) noexcept;
  std::shared_ptr<const Impl> impl_;
  friend class ProcessHost;
  friend Result<std::shared_ptr<const Registry>> detail::freeze_registry(
      std::shared_ptr<detail::BuilderCore>);
  friend Result<std::shared_ptr<ProcessHost>> make_host(
      std::shared_ptr<Services>, std::shared_ptr<const Registry>);
};

class RegistryBuilder {
 public:
  RegistryBuilder();
  RegistryBuilder(RegistryBuilder&&) noexcept = default;
  RegistryBuilder& operator=(RegistryBuilder&&) noexcept = default;
  RegistryBuilder(const RegistryBuilder&) = delete;
  RegistryBuilder& operator=(const RegistryBuilder&) = delete;
  ~RegistryBuilder();

  template <class T>
  Result<Done> register_type(TypeCodec<T> codec) {
    detail::ErasedCodec erased;
    erased.id = codec.id;
    erased.cookie = detail::type_cookie<T>();
    erased.schema_json = std::string(codec.schema_json);
    auto decode = codec.decode;
    auto encode = codec.encode;
    erased.decode = [decode](InvocationContext ctx, std::string_view text,
                             WireObjects& objects)
        -> Result<std::shared_ptr<const void>> {
      if (!decode) {
        Result<std::shared_ptr<const void>> failure;
        failure.code = Code::unsupported;
        return failure;
      }
      Result<T> decoded = decode(ctx, text, objects);
      if (decoded.code != Code::ok || !decoded.value)
        return detail::remap_failure<T, std::shared_ptr<const void>>(decoded);
      Result<std::shared_ptr<const void>> out;
      out.code = Code::ok;
      out.value = detail::box_owned<T>(std::move(*decoded.value));
      out.assessment = decoded.assessment;
      out.usage = decoded.usage;
      out.dependencies = decoded.dependencies;
      return out;
    };
    erased.encode = [encode](InvocationContext ctx,
                             std::shared_ptr<const void> value)
        -> Result<std::string> {
      if (!encode) {
        Result<std::string> failure;
        failure.code = Code::unsupported;
        return failure;
      }
      return encode(ctx, *std::static_pointer_cast<const T>(value));
    };
    return detail::register_type_erased(core_, std::move(erased));
  }

  Result<std::shared_ptr<ProcessRegistration>> begin(ProcessDescriptor);

  template <class In, class Out>
  Result<Done> add(std::shared_ptr<ProcessRegistration> registration,
                   Operation<In, Out> operation) {
    auto shared = std::make_shared<Operation<In, Out>>(std::move(operation));
    detail::ErasedOperation erased;
    erased.operation_id = shared->operation_id;
    erased.input_type = shared->input_type;
    erased.output_type = shared->output_type;
    erased.input_cookie = detail::type_cookie<In>();
    erased.output_cookie = detail::type_cookie<Out>();
    erased.validate = [shared](InvocationContext ctx,
                               std::shared_ptr<const void> input) {
      return shared->validate(ctx, std::static_pointer_cast<const In>(input));
    };
    erased.bounds = [shared](InvocationContext ctx,
                             std::shared_ptr<const void> input) {
      return shared->bounds(ctx, std::static_pointer_cast<const In>(input));
    };
    erased.plan = [shared](InvocationContext ctx,
                           std::shared_ptr<const void> input,
                           PlanOptions options) {
      return shared->plan(ctx, std::static_pointer_cast<const In>(input),
                          options);
    };
    erased.run = [shared](InvocationContext ctx,
                          std::shared_ptr<const void> input,
                          Ref<InvocationPlan> plan)
        -> Task<std::shared_ptr<const void>> {
      return rt::then(
          ctx,
          shared->run(ctx, std::static_pointer_cast<const In>(input), plan),
          [ctx](Out value) -> Task<std::shared_ptr<const void>> {
            Result<std::shared_ptr<const void>> out;
            out.code = Code::ok;
            out.value = detail::box_owned<Out>(std::move(value));
            return rt::ready<std::shared_ptr<const void>>(ctx, std::move(out));
          });
    };
    erased.finalize = [shared](
                          InvocationContext ctx,
                          std::shared_ptr<const void> input,
                          Ref<InvocationPlan> plan,
                          Result<std::shared_ptr<const void>> partial)
        -> Task<std::shared_ptr<const void>> {
      Result<Out> typed = detail::remap_failure<std::shared_ptr<const void>, Out>(
          partial);
      if (partial.code == Code::ok && partial.value)
        typed.value = *std::static_pointer_cast<const Out>(partial.value);
      return rt::then(
          ctx,
          shared->finalize(ctx, std::static_pointer_cast<const In>(input),
                           plan, std::move(typed)),
          [ctx](Out value) -> Task<std::shared_ptr<const void>> {
            Result<std::shared_ptr<const void>> out;
            out.code = Code::ok;
            out.value = detail::box_owned<Out>(std::move(value));
            return rt::ready<std::shared_ptr<const void>>(ctx, std::move(out));
          });
    };
    return detail::add_operation_erased(
        core_, std::move(registration), std::move(erased));
  }

  Result<std::shared_ptr<const Registry>> freeze();

 private:
  std::shared_ptr<detail::BuilderCore> core_;
};

class ProcessHost {
 public:
  ProcessHost() noexcept = default;
  ProcessHost(ProcessHost&&) noexcept = default;
  ProcessHost& operator=(ProcessHost&&) noexcept = default;
  ProcessHost(const ProcessHost&) = delete;
  ProcessHost& operator=(const ProcessHost&) = delete;
  ~ProcessHost();

  template <class In, class Out>
  Task<Out> invoke(InvocationContext ctx, Id process, Id operation,
                   Ref<In> input) {
    auto erased = detail::host_invoke_erased(
        core_, ctx, std::move(process), std::move(operation),
        std::static_pointer_cast<const void>(input),
        detail::type_cookie<In>(), detail::type_cookie<Out>());
    return rt::then(
        ctx, std::move(erased),
        [ctx](std::shared_ptr<const void> value) -> Task<Out> {
          Result<Out> out;
          if (!value) {
            out.code = Code::internal;
            return rt::ready<Out>(ctx, std::move(out));
          }
          out.code = Code::ok;
          out.value = *std::static_pointer_cast<const Out>(value);
          return rt::ready<Out>(ctx, std::move(out));
        });
  }

  Task<WireResponse> invoke_wire(InvocationContext, Ref<WireRequest>);
  Task<Done> close_async();

 private:
  std::shared_ptr<detail::HostCore> core_;
  friend Result<std::shared_ptr<ProcessHost>> make_host(
      std::shared_ptr<Services>, std::shared_ptr<const Registry>);
};

class WireObjects {
 public:
  WireObjects() noexcept = default;
  WireObjects(WireObjects&&) noexcept = default;
  WireObjects& operator=(WireObjects&&) noexcept = default;
  WireObjects(const WireObjects&) = delete;
  WireObjects& operator=(const WireObjects&) = delete;
  ~WireObjects();

  template <class T>
  Result<Ref<T>> resolve(Id alias_or_hash) const {
    Result<std::shared_ptr<const void>> found = resolve_erased(alias_or_hash);
    if (found.code != Code::ok || !found.value)
      return detail::remap_failure<std::shared_ptr<const void>, Ref<T>>(found);
    Result<Ref<T>> out;
    out.code = Code::ok;
    out.value = std::static_pointer_cast<const T>(found.value);
    return out;
  }
  template <class T>
  Result<Done> bind(Id alias, Ref<T> object) {
    return bind_erased(std::move(alias),
                       std::static_pointer_cast<const void>(object),
                       detail::type_cookie<T>());
  }
  template <class T>
  Result<Id> intern(Ref<T> object) {
    return intern_erased(std::static_pointer_cast<const void>(object),
                         detail::type_cookie<T>());
  }

 private:
  Result<std::shared_ptr<const void>> resolve_erased(Id) const;
  Result<Done> bind_erased(Id, std::shared_ptr<const void>, const void*);
  Result<Id> intern_erased(std::shared_ptr<const void>, const void*);
  std::shared_ptr<detail::WireObjectTable> table_;
  friend Result<std::shared_ptr<WireObjects>> make_wire_objects(
      InvocationContext, std::shared_ptr<const Registry>, Ref<WireRequest>);
};

template <class T>
Task<T> run_region(InvocationContext, Ref<InvocationPlan>, U32 region_id,
                   std::function<Task<T>(InvocationContext)> body);

Task<Done> admit_epoch(InvocationContext, Ref<InvocationPlan>);

Result<std::shared_ptr<Services>> make_services(PoolConfig);
Result<InvocationContext> open_command(std::shared_ptr<Services>, Id command_id,
                                       ScopeLimits, U32 weight = 1);
Result<std::shared_ptr<ProcessHost>> make_host(
    std::shared_ptr<Services>, std::shared_ptr<const Registry>);
Task<Done> close_command(InvocationContext);

Result<std::shared_ptr<WireObjects>> make_wire_objects(
    InvocationContext, std::shared_ptr<const Registry>, Ref<WireRequest>);
Task<Done> publish_wire_response(InvocationContext, Id result_path,
                                 WireResponse);

// The body runs once the preceding regions of the same plan have completed.
// The first implementation schedules regions of one invocation in ascending
// id order, so every predecessor is already finished when this is reached; the
// admitted progress reservation is already bound to the invocation context.
template <class T>
Task<T> run_region(InvocationContext ctx, Ref<InvocationPlan> plan,
                   U32 region_id,
                   std::function<Task<T>(InvocationContext)> body) {
  Result<T> failure;
  failure.code = Code::invalid_request;
  if (!plan || !body) return ready<T>(ctx, std::move(failure));
  bool known = false;
  for (const auto& region : plan->regions)
    if (region.id == region_id) known = true;
  if (!known) return ready<T>(ctx, std::move(failure));
  InvocationContext region_ctx = ctx;
  region_ctx.region = region_id;
  return body(std::move(region_ctx));
}

}  // namespace acsd::rt
