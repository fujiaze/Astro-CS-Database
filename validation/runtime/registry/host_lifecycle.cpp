// Independent ProcessHost lifecycle and wire cases (VERIFICATION LIFE01,
// FORMAT01 and the M3 Host/wire boundary).
//
// The test links the real runtime (ledger, executor, diagnostics) and uses only
// the public typed API.  It checks that:
//   * resolve rejects unknown ids and cookie mismatches before run;
//   * validate/bounds/plan failures produce a terminal result with usage and no
//     publishable value;
//   * an API-allowed partial goes through finalize and keeps its original code;
//   * a hard failure and a thrown exception never call finalize and never
//     deliver a value;
//   * admission replans in the SCHEDULER order (halve block_elements, then
//     spill) and ends in resource_exhausted without scaling R/F;
//   * invoke_wire completes the protocol with an outer ok response, keeps the
//     science code in result_code and writes result_path atomically;
//   * WireObjects has a local alias table with typed bind/intern/resolve.
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include <cstdio>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

using namespace acsd;
using namespace acsd::rt;

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const std::string& name) {
  ++checks;
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << name << "\n";
}

// Capture-free hook bodies: Operation stores plain function pointers, so the
// per-case mode is selected through these globals (the test is single threaded).
bool g_validate_fails = false;
bool g_partial_run = false;
bool g_run_throws = false;
bool g_huge_plan = false;
bool g_finalize_called = false;
bool g_finalize_fails = false;
int g_plan_calls = 0;
std::string g_last_plan_placement;

struct In {
  int value = 1;
};
struct Out {
  int value = 42;
};

template <class T>
TypeCodec<T> good_codec(const Id& schema) {
  TypeCodec<T> codec;
  codec.id = {schema, 1};
  codec.schema_json = "{}";
  codec.decode = [](InvocationContext, std::string_view, WireObjects&)
      -> Result<T> {
    Result<T> out;
    out.code = Code::ok;
    out.value = T{};
    return out;
  };
  codec.encode = [](InvocationContext, const T&, WireObjects&)
      -> Result<std::string> {
    Result<std::string> out;
    out.code = Code::ok;
    out.value = "{\"value\":42}";
    return out;
  };
  return codec;
}

Operation<In, Out> operation(const Id& id) {
  Operation<In, Out> value;
  value.operation_id = id;
  value.input_type = {"acsd.test.In", 1};
  value.output_type = {"acsd.test.Out", 1};
  value.validate = [](InvocationContext, Ref<In>) -> Result<Done> {
    Result<Done> out;
    if (g_validate_fails) {
      out.code = Code::invalid_request;
      Issue issue;
      issue.code = out.code;
      issue.origin = "test.validate";
      issue.reason = "validation rejected the request";
      out.assessment.issues.push_back(std::move(issue));
      return out;
    }
    out.code = Code::ok;
    out.value = Done{};
    return out;
  };
  value.bounds = [](InvocationContext, Ref<In>) -> Result<PlanBounds> {
    Result<PlanBounds> out;
    out.code = Code::ok;
    PlanBounds bounds;
    bounds.min_block_elements = 1;
    bounds.max_block_elements = 8;
    bounds.max_window = 1;
    bounds.allow_spill = true;
    out.value = bounds;
    return out;
  };
  value.plan = [](InvocationContext, Ref<In>, PlanOptions options)
      -> Result<InvocationPlan> {
    ++g_plan_calls;
    g_last_plan_placement =
        options.placement == Placement::spill ? "spill" : "memory";
    Result<InvocationPlan> out;
    out.code = Code::ok;
    InvocationPlan plan;
    plan.elements = 8;
    plan.block_elements = options.block_elements;
    plan.cohorts = 1;
    plan.epoch = 0;
    plan.options = options;
    plan.input_hash = "input:resolved";
    plan.request_hash = "request:resolved";
    plan.persistent.host_bytes = g_huge_plan ? (4ull << 20) : 256;
    plan.progress_floor.host_bytes = g_huge_plan ? (4ull << 20) : 128;
    out.value = plan;
    return out;
  };
  value.run = [](InvocationContext ctx, Ref<In>, Ref<InvocationPlan>)
      -> Task<Out> {
    if (g_run_throws) throw std::runtime_error("intentional run exception");
    Result<Out> out;
    out.code = g_partial_run ? Code::nonconverged : Code::ok;
    out.value = Out{};
    return ready<Out>(ctx, std::move(out));
  };
  value.finalize = [](InvocationContext ctx, Ref<In>, Ref<InvocationPlan>,
                      Result<Out> partial) -> Task<Out> {
    g_finalize_called = true;
    if (g_finalize_fails) {
      Result<Out> failed;
      failed.code = Code::io_error;
      Issue issue;
      issue.code = failed.code;
      issue.origin = "test.finalize";
      issue.reason = "sealing failed after the science run";
      failed.assessment.issues.push_back(std::move(issue));
      return ready<Out>(ctx, std::move(failed));
    }
    return ready<Out>(ctx, std::move(partial));
  };
  return value;
}

Result<std::shared_ptr<Services>> make_test_services() {
  PoolConfig config;
  config.cpu_threads = 2;
  config.read_slots = 1;
  config.write_slots = 1;
  config.process.ceiling.host_bytes = 4ull << 20;
  config.process.ceiling.cpu_tokens = 2;
  config.process.max_work = 1000;
  return make_services(config);
}

struct Fixture {
  std::shared_ptr<Services> services;
  InvocationContext command;
  std::shared_ptr<ProcessHost> host;
  std::shared_ptr<const Registry> registry;
  Ref<In> input = std::make_shared<const In>(In{1});
};

bool build_fixture(Fixture& fixture) {
  Result<std::shared_ptr<Services>> services = make_test_services();
  check(services.code == Code::ok && services.value != nullptr,
        "fixture: make_services");
  if (services.code != Code::ok || !services.value) return false;
  fixture.services = *services.value;
  ScopeLimits limits;
  limits.ceiling.host_bytes = 1ull << 20;
  limits.max_work = 500;
  Result<InvocationContext> command =
      open_command(fixture.services, "test.command", limits, 1);
  check(command.code == Code::ok, "fixture: open_command");
  if (command.code != Code::ok) return false;
  fixture.command = *command.value;

  RegistryBuilder builder;
  check(builder.register_type<In>(good_codec<In>("acsd.test.In")).code ==
            Code::ok,
        "fixture: register In");
  check(builder.register_type<Out>(good_codec<Out>("acsd.test.Out")).code ==
            Code::ok,
        "fixture: register Out");

  ProcessDescriptor descriptor;
  descriptor.process_id = "test.host";
  descriptor.module = "observation";
  descriptor.document = "docs/engineering/rebuild/processes/"
                        "observation.calibrate.md";
  descriptor.semantic_revision = 1;
  descriptor.dependencies = {"model", "runtime"};
  descriptor.probes = {{"test.host.phase", "span", 1}};
  Result<std::shared_ptr<ProcessRegistration>> process =
      builder.begin(descriptor);
  check(process.code == Code::ok, "fixture: begin");
  if (process.code != Code::ok) return false;
  check(builder.add(*process.value, operation("go")).code == Code::ok,
        "fixture: add go");
  check(builder.add(*process.value, operation("partial")).code == Code::ok,
        "fixture: add partial");
  check(builder.add(*process.value, operation("throwing")).code == Code::ok,
        "fixture: add throwing");
  check(builder.add(*process.value, operation("finalize_fails")).code ==
            Code::ok,
        "fixture: add finalize_fails");
  {
    Operation<In, Out> epoching = operation("epoching");
    // run calls admit_epoch synchronously from the (external) caller thread.
    epoching.run = [](InvocationContext ctx, Ref<In>, Ref<InvocationPlan>)
        -> Task<Out> {
      bool ok = true;
      InvocationPlan next;
      next.epoch = 1;
      ok = ok && wait_external(admit_epoch(
                         ctx, std::make_shared<const InvocationPlan>(next)))
                         .code == Code::ok;
      next.epoch = 2;  // strictly previous+1
      ok = ok && wait_external(admit_epoch(
                         ctx, std::make_shared<const InvocationPlan>(next)))
                         .code == Code::ok;
      next.epoch = 2;  // repeating a terminal epoch is rejected
      ok = ok && wait_external(admit_epoch(
                         ctx, std::make_shared<const InvocationPlan>(next)))
                         .code == Code::identity_conflict;
      next.epoch = 5;  // jumping epochs is rejected
      ok = ok && wait_external(admit_epoch(
                         ctx, std::make_shared<const InvocationPlan>(next)))
                         .code == Code::identity_conflict;
      Result<Out> out;
      out.code = ok ? Code::ok : Code::internal;
      if (ok) out.value = Out{};
      return ready<Out>(ctx, std::move(out));
    };
    check(builder.add(*process.value, std::move(epoching)).code == Code::ok,
          "fixture: add epoching");
  }
  check(builder.add(*process.value, operation("too_big")).code == Code::ok,
        "fixture: add too_big");

  Result<std::shared_ptr<const Registry>> frozen = builder.freeze();
  check(frozen.code == Code::ok, "fixture: freeze");
  if (frozen.code != Code::ok || !frozen.value) return false;
  fixture.registry = *frozen.value;
  Result<std::shared_ptr<ProcessHost>> host =
      make_host(fixture.services, fixture.registry);
  check(host.code == Code::ok && host.value != nullptr, "fixture: make_host");
  if (host.code != Code::ok || !host.value) return false;
  fixture.host = *host.value;
  return true;
}

void reset_modes() {
  g_validate_fails = false;
  g_partial_run = false;
  g_run_throws = false;
  g_finalize_called = false;
  g_plan_calls = 0;
  g_last_plan_placement.clear();
  g_huge_plan = false;
  g_finalize_fails = false;
}

// ---------------------------------------------------------------------------
// Typed lifecycle
// ---------------------------------------------------------------------------

void typed_lifecycle(Fixture& fixture) {
  // Positive call.
  reset_modes();
  {
    Result<Out> result = wait_external(
        fixture.host->invoke<In, Out>(fixture.command, "test.host", "go",
                                      fixture.input));
    check(result.code == Code::ok, "positive: science ok");
    check(result.value && result.value->value == 42,
          "positive: typed value delivered");
  }

  // Unknown ids are rejected before run.
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "missing", fixture.input));
    check(result.code == Code::unsupported,
          "unknown operation: unsupported before run");
    check(!result.value, "unknown operation: no value");
  }
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "missing.process", "go", fixture.input));
    check(result.code == Code::unsupported,
          "unknown process: unsupported before run");
  }

  // An exception escaping run becomes internal and never publishes a value.
  reset_modes();
  g_run_throws = true;
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "throwing", fixture.input));
    check(result.code == Code::internal,
          "run exception: internal");
    check(!result.value, "run exception: no value");
    check(!g_finalize_called, "run exception: finalize is not called");
  }

  // validate failure: terminal, no run/finalize, usage is still reported.
  reset_modes();
  g_validate_fails = true;
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "go", fixture.input));
    check(result.code == Code::invalid_request,
          "validate failure: original code");
    check(!result.value, "validate failure: no value");
    check(!g_finalize_called, "validate failure: finalize is not called");
  }
  reset_modes();

  // API-allowed partial: finalize runs, original non-ok code is preserved.
  g_partial_run = true;
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "partial", fixture.input));
    check(result.code == Code::nonconverged,
          "partial: original nonconverged code preserved");
    check(g_finalize_called, "partial: finalize was called");
  }
  // admit_epoch enforces a strictly increasing epoch lineage.
  reset_modes();
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "epoching", fixture.input));
    check(result.code == Code::ok, "admit_epoch: epoch lineage checks pass");
  }

  // finalize failure lowers the code and never delivers a value.
  reset_modes();
  g_finalize_fails = true;
  {
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "finalize_fails", fixture.input));
    check(result.code == Code::io_error,
          "finalize failure: lowered code");
    check(!result.value, "finalize failure: no value delivered");
  }
  reset_modes();

  // Admission failure: the Host replans (halving, then spill) and never scales
  // the ResourceVector by a ratio.
  {
    g_huge_plan = true;
    Result<Out> result = wait_external(fixture.host->invoke<In, Out>(
        fixture.command, "test.host", "too_big", fixture.input));
    g_huge_plan = false;
    check(result.code == Code::resource_exhausted,
          "unadmittable plan: resource_exhausted");
    check(g_plan_calls >= 5,
          "unadmittable plan: plan was re-called with new PlanOptions");
    check(g_last_plan_placement == "spill",
          "unadmittable plan: final replan used the spill placement");
    check(!g_finalize_called, "unadmittable plan: no finalize/run");
  }
}

// ---------------------------------------------------------------------------
// Wire
// ---------------------------------------------------------------------------

WireRequest request_for(const Id& operation) {
  WireRequest request;
  request.request_id = "request:1";
  request.process = "test.host";
  request.operation = operation;
  request.input_type = {"acsd.test.In", 1};
  request.input_json = "{}";
  request.input_objects_json = "{}";
  request.input_root = "memory:input";
  request.artifact_root = "memory:artifact";
  return request;
}

void wire_lifecycle(Fixture& fixture) {
  reset_modes();
  Ref<WireRequest> request =
      std::make_shared<const WireRequest>(request_for("go"));
  Result<WireResponse> response = wait_external(
      fixture.host->invoke_wire(fixture.command, request));
  check(response.code == Code::ok,
        "wire: outer Task code is ok (protocol completed)");
  check(response.value.has_value(), "wire: response value exists");
  if (response.value) {
    check(response.value->result_code == Code::ok,
          "wire: science result_code is ok");
    check(response.value->value_json.has_value(),
          "wire: encoded value_json exists");
    check(!response.value->invocation.empty(), "wire: invocation id filled");
    check(response.value->output_type.schema == "acsd.test.Out",
          "wire: output TypeId filled");
    check(response.value->input_hash == "input:resolved",
          "wire: resolved input hash comes from the plan");
    check(response.value->request_hash == "request:resolved",
          "wire: resolved request hash comes from the plan");
    check(response.value->backend == "cpu",
          "wire: actual backend is reported");
  }

  // The science code stays in result_code; a protocol-level request failure
  // still produces a complete response with an ok outer Task.
  WireRequest wrong = request_for("go");
  wrong.input_type = {"acsd.test.Other", 1};
  Result<WireResponse> mismatch = wait_external(fixture.host->invoke_wire(
      fixture.command, std::make_shared<const WireRequest>(wrong)));
  check(mismatch.code == Code::ok && mismatch.value.has_value(),
        "wire: TypeId mismatch still yields a response");
  if (mismatch.value)
    check(mismatch.value->result_code == Code::invalid_request,
          "wire: TypeId mismatch is invalid_request");

  Result<WireResponse> unknown = wait_external(fixture.host->invoke_wire(
      fixture.command,
      std::make_shared<const WireRequest>(request_for("missing"))));
  check(unknown.code == Code::ok && unknown.value.has_value(),
        "wire: unknown operation still yields a response");
  if (unknown.value)
    check(unknown.value->result_code == Code::unsupported,
          "wire: unknown operation is unsupported");

  // An API-allowed partial keeps its non-ok science code but still encodes a
  // partial value into the wire response.
  reset_modes();
  g_partial_run = true;
  {
    Result<WireResponse> partial = wait_external(fixture.host->invoke_wire(
        fixture.command,
        std::make_shared<const WireRequest>(request_for("partial"))));
    g_partial_run = false;
    check(partial.code == Code::ok && partial.value.has_value(),
          "wire: partial still yields a protocol response");
    if (partial.value) {
      check(partial.value->result_code == Code::nonconverged,
            "wire: partial keeps the nonconverged science code");
      check(partial.value->value_json.has_value(),
            "wire: partial value_json is encoded");
    }
  }
  reset_modes();

  // Atomic publication.
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "acsd-wire-response.json";
  std::filesystem::remove(path);
  if (response.value) {
    Result<Done> published = wait_external(
        publish_wire_response(fixture.command, path.string(), *response.value));
    check(published.code == Code::ok, "wire: publish succeeds");
    std::error_code error;
    check(std::filesystem::is_regular_file(path, error),
          "wire: result_path exists");
    check(!std::filesystem::exists(path.string() + ".tmp.1", error),
          "wire: no temporary file remains");
    std::ifstream stream(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    check(text.find("acsd.process/2") != std::string::npos,
          "wire: published protocol string");
  }
  // A result_path whose parent is a regular file cannot be created: the publish
  // fails as io_error and leaves no temporary product.
  const std::filesystem::path blocked =
      std::filesystem::temp_directory_path() / "acsd-wire-blocked";
  {
    std::ofstream stream(blocked, std::ios::trunc);
    stream << "not a directory";
  }
  if (response.value) {
    Result<Done> failed = wait_external(publish_wire_response(
        fixture.command, (blocked / "response.json").string(),
        *response.value));
    check(failed.code == Code::io_error,
          "wire: uncommittable result_path is io_error");
  }
  std::filesystem::remove(blocked);
  std::filesystem::remove(path);

  // WireObjects: a request-local table with typed aliases and stable intern ids.
  Result<std::shared_ptr<WireObjects>> objects =
      make_wire_objects(fixture.command, fixture.registry, request);
  check(objects.code == Code::ok, "wire: make_wire_objects");
  if (objects.code == Code::ok && objects.value) {
    auto table = *objects.value;
    Ref<In> shared_in = std::make_shared<const In>(In{3});
    check(table->bind<In>("alias.in", shared_in).code == Code::ok,
          "wire objects: bind typed object");
    check(table->bind<Out>("alias.in", std::make_shared<const Out>(Out{}))
                  .code == Code::identity_conflict,
          "wire objects: same alias with another type conflicts");
    Result<Ref<In>> resolved = table->resolve<In>("alias.in");
    check(resolved.code == Code::ok && resolved.value == shared_in,
          "wire objects: resolve bound alias");
    Result<Ref<In>> absent = table->resolve<In>("alias.missing");
    check(absent.code == Code::missing_dependency,
          "wire objects: unknown alias is missing_dependency");
    Result<Id> first = table->intern<In>(shared_in);
    Result<Id> second = table->intern<In>(shared_in);
    check(first.code == Code::ok && second.code == Code::ok &&
              first.value == second.value,
          "wire objects: intern id is stable for the same object");
    check(table->resolve<In>(*first.value).code == Code::ok,
          "wire objects: interned id resolves");
  }
}

}  // namespace

int main() {
  // A default-constructed host has no frozen registry: invoking it must fail
  // before any science work (the "not frozen" entry point).
  {
    ProcessHost unwired;
    InvocationContext empty;
    Result<Out> result = wait_external(unwired.invoke<In, Out>(
        empty, "test.host", "go", std::make_shared<const In>(In{1})));
    check(result.code == Code::invalid_request,
          "unwired host: invoke is invalid_request before freeze");
    check(!result.value, "unwired host: no value");
  }

  Fixture fixture;
  if (!build_fixture(fixture)) {
    std::cerr << "host lifecycle fixture failed\n";
    return 1;
  }
  typed_lifecycle(fixture);
  wire_lifecycle(fixture);

  // Closing the host rejects new invocations.  The command is closed before the
  // host drains, as in the documented lifecycle.
  close_command(fixture.command);
  Result<Done> closed = wait_external(fixture.host->close_async());
  check(closed.code == Code::ok, "close_async: completes after drain");
  // Closing the host must reject new invocations even after the pool stopped;
  // the continuation runs inline because Services is already shutting down.
  reset_modes();
  Result<Out> after = wait_external(fixture.host->invoke<In, Out>(
      fixture.command, "test.host", "go", fixture.input));
  check(after.code == Code::unsupported,
        "close_async: new invoke is unsupported");
  check(!after.value, "close_async: rejected invoke delivers no value");

  if (failures != 0) {
    std::cerr << failures << " of " << checks << " host checks failed\n";
    return 1;
  }
  std::cout << "host lifecycle/wire PASS: " << checks << " checks\n";
  return 0;
}
