// Independent registry negative and positive cases (VERIFICATION REG01).
//
// This test links only the registry translation unit: it exercises the frozen
// registration contract (duplicate keys, typed codec binding, hooks, document,
// probe ownership, static dependency cycle, immutability) without a live pool.
// ProcessHost lifecycle cases live in a separate test that needs Services.
#include <acsd/runtime/process.hpp>

#include <cstdio>
#include <filesystem>
#include <set>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

template <class T>
TypeCodec<T> good_codec(const Id& schema, std::string schema_json = "{}") {
  TypeCodec<T> codec;
  codec.id = {schema, 1};
  codec.schema_json = std::move(schema_json);
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
    out.value = "{}";
    return out;
  };
  return codec;
}

template <class In, class Out>
Operation<In, Out> operation(const Id& operation_id, const Id& input_schema,
                             const Id& output_schema) {
  Operation<In, Out> value;
  value.operation_id = operation_id;
  value.input_type = {input_schema, 1};
  value.output_type = {output_schema, 1};
  // The hook bodies are never executed here.  They return default-constructed
  // Task handles so this test does not need the executor translation unit.
  value.validate = [](InvocationContext, Ref<In>) -> Result<Done> { return {}; };
  value.bounds = [](InvocationContext, Ref<In>) -> Result<PlanBounds> {
    Result<PlanBounds> out;
    out.code = Code::ok;
    out.value = PlanBounds{};
    return out;
  };
  value.plan = [](InvocationContext, Ref<In>, PlanOptions)
      -> Result<InvocationPlan> {
    Result<InvocationPlan> out;
    out.code = Code::ok;
    out.value = InvocationPlan{};
    return out;
  };
  value.run = [](InvocationContext, Ref<In>, Ref<InvocationPlan>) -> Task<Out> {
    return Task<Out>{};
  };
  value.finalize = [](InvocationContext, Ref<In>, Ref<InvocationPlan>,
                      Result<Out>) -> Task<Out> { return Task<Out>{}; };
  return value;
}

ProcessDescriptor descriptor(const Id& process, const Id& module,
                             const Id& document,
                             std::vector<Id> dependencies = {},
                             std::vector<ProbeDeclaration> probes = {}) {
  ProcessDescriptor value;
  value.process_id = process;
  value.module = module;
  value.document = document;
  value.semantic_revision = 1;
  value.dependencies = std::move(dependencies);
  value.probes = std::move(probes);
  return value;
}

std::string first_reason(const Result<std::shared_ptr<const Registry>>& frozen) {
  if (frozen.assessment.issues.empty()) return "<no issue>";
  return frozen.assessment.issues.front().reason;
}

Code freeze_code(RegistryBuilder& builder,
                 std::shared_ptr<const Registry>* registry = nullptr) {
  Result<std::shared_ptr<const Registry>> frozen = builder.freeze();
  if (frozen.code == Code::ok && frozen.value && registry != nullptr)
    *registry = *frozen.value;
  return frozen.code;
}

std::string freeze_reason(RegistryBuilder& builder) {
  Result<std::shared_ptr<const Registry>> frozen = builder.freeze();
  if (frozen.assessment.issues.empty()) return "<no issue>";
  const Issue& issue = frozen.assessment.issues.front();
  return issue.origin + ": " + issue.reason;
}

std::string temporary_document(const std::string& name) {
  static const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "acsd-registry-negative";
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  const std::filesystem::path path = directory / name;
  std::ofstream stream(path, std::ios::trunc);
  stream << "# registry negative fixture\n";
  return path.string();
}

bool contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

// A conventional valid assembly used as the base of most cases.
struct Fixture {
  RegistryBuilder builder;
  std::shared_ptr<ProcessRegistration> process;
  Id document;
};

std::unique_ptr<Fixture> make_fixture() {
  auto fixture = std::make_unique<Fixture>();
  fixture->document = temporary_document("acquire.md");
  check(fixture->builder.register_type<U32>(good_codec<U32>("acsd.test.In")).code ==
            Code::ok,
        "fixture: register input type");
  check(
      fixture->builder.register_type<U64>(good_codec<U64>("acsd.test.Out")).code ==
          Code::ok,
      "fixture: register output type");
  Result<std::shared_ptr<ProcessRegistration>> begun = fixture->builder.begin(
      descriptor("test.acquire", "observation", fixture->document, {},
                 {{"test.acquire.span", "span", 1}}));
  check(begun.code == Code::ok && begun.value != nullptr,
        "fixture: begin process");
  if (begun.value != nullptr) {
    fixture->process = *begun.value;
    check(fixture->builder
              .add(fixture->process,
                   operation<U32, U64>("acquire", "acsd.test.In",
                                       "acsd.test.Out"))
              .code == Code::ok,
          "fixture: add operation");
  }
  return fixture;
}

// ---------------------------------------------------------------------------
// Positive contract
// ---------------------------------------------------------------------------

void positive_contract() {
  auto fixture = make_fixture();
  std::shared_ptr<const Registry> registry;
  check(freeze_code(fixture->builder, &registry) == Code::ok,
        "positive: freeze succeeds");
  check(registry != nullptr, "positive: freeze returns a registry");
  if (registry == nullptr) return;
  check(registry->has("test.acquire", "acquire"),
        "positive: has registered (process, operation)");
  check(!registry->has("test.acquire", "unknown"),
        "positive: unknown operation is absent");
  check(!registry->has("unknown.process", "acquire"),
        "positive: unknown process is absent");

  const std::string manifest = registry->manifest_json();
  check(contains(manifest, "\"protocol\": \"acsd.process/2\""),
        "manifest: protocol acsd.process/2");
  check(contains(manifest, "\"process\": \"test.acquire\""),
        "manifest: registered process id");
  check(contains(manifest, "\"operation\": \"acquire\""),
        "manifest: registered operation id");
  check(contains(manifest, "\"schema\":\"acsd.test.In\""),
        "manifest: input TypeId schema");
  check(contains(manifest, "\"revision\":1"), "manifest: TypeId revision");
  check(contains(manifest, "\"document\": "),
        "manifest: document field");
  check(contains(manifest, "\"probes\""), "manifest: probes field");
  check(contains(manifest, "\"dependencies\""),
        "manifest: dependencies field");
  check(contains(manifest, "\"backend\": \"cpu\""),
        "manifest: backend field");
  check(contains(manifest, "\"backends\": [\"cpu\"]"),
        "manifest: operation backend capability");
  check(contains(manifest, "test.acquire.span"),
        "manifest: probe owner declaration");

  // A default-constructed registry is empty, not a crash or a fabricated
  // process.
  Registry empty;
  check(!empty.has("test.acquire", "acquire"),
        "positive: default Registry has no identity");
  check(contains(empty.manifest_json(), "acsd.process/2"),
        "positive: default Registry still reports the protocol");
}

void stable_manifest_order() {
  RegistryBuilder builder;
  const Id document = temporary_document("order.md");
  check(builder.register_type<U32>(good_codec<U32>("acsd.test.In")).code ==
            Code::ok,
        "order: input type");
  check(builder.register_type<U64>(good_codec<U64>("acsd.test.Out")).code ==
            Code::ok,
        "order: output type");
  const Id alpha_document = temporary_document("order-alpha.md");
  auto zulu = builder.begin(descriptor("test.zulu", "observation", document));
  auto alpha = builder.begin(descriptor("test.alpha", "observation", alpha_document));
  check(zulu.code == Code::ok && alpha.code == Code::ok, "order: begin");
  if (zulu.code == Code::ok && alpha.code == Code::ok) {
    check(builder.add(*zulu.value,
                      operation<U32, U64>("zeta", "acsd.test.In",
                                          "acsd.test.Out")).code == Code::ok,
          "order: add zeta");
    check(builder.add(*zulu.value,
                      operation<U32, U64>("alpha", "acsd.test.In",
                                          "acsd.test.Out")).code == Code::ok,
          "order: add alpha");
    check(builder.add(*alpha.value,
                      operation<U32, U64>("beta", "acsd.test.In",
                                          "acsd.test.Out")).code == Code::ok,
          "order: add beta");
  }
  std::shared_ptr<const Registry> registry;
  check(freeze_code(builder, &registry) == Code::ok, "order: freeze");
  if (registry == nullptr) return;
  const std::string manifest = registry->manifest_json();
  const std::size_t alpha_process = manifest.find("\"process\": \"test.alpha\"");
  const std::size_t zulu_process = manifest.find("\"process\": \"test.zulu\"");
  const std::size_t alpha_operation = manifest.find("\"operation\": \"alpha\"");
  const std::size_t zeta_operation = manifest.find("\"operation\": \"zeta\"");
  check(alpha_process != std::string::npos &&
            zulu_process != std::string::npos &&
            alpha_process < zulu_process,
        "order: processes sorted by process_id");
  check(alpha_operation != std::string::npos &&
            zeta_operation != std::string::npos &&
            alpha_operation < zeta_operation,
        "order: operations sorted by operation_id");

  // Deterministic bytes: the same registry assembled in a different insertion
  // order must produce the identical manifest (it is the build product).
  RegistryBuilder reordered;
  reordered.register_type<U32>(good_codec<U32>("acsd.test.In"));
  reordered.register_type<U64>(good_codec<U64>("acsd.test.Out"));
  auto reordered_alpha =
      reordered.begin(descriptor("test.alpha", "observation", alpha_document));
  auto reordered_zulu =
      reordered.begin(descriptor("test.zulu", "observation", document));
  if (reordered_alpha.code == Code::ok && reordered_zulu.code == Code::ok) {
    reordered.add(*reordered_zulu.value,
                  operation<U32, U64>("zeta", "acsd.test.In", "acsd.test.Out"));
    reordered.add(*reordered_zulu.value,
                  operation<U32, U64>("alpha", "acsd.test.In", "acsd.test.Out"));
    reordered.add(*reordered_alpha.value,
                  operation<U32, U64>("beta", "acsd.test.In", "acsd.test.Out"));
  }
  std::shared_ptr<const Registry> again;
  check(freeze_code(reordered, &again) == Code::ok, "order: reordered freeze");
  if (again != nullptr)
    check(again->manifest_json() == manifest,
          "order: manifest bytes are independent of registration order");
}

// ---------------------------------------------------------------------------
// Negative registry cases
// ---------------------------------------------------------------------------

void duplicate_process() {
  const Id document = temporary_document("duplicate-process.md");
  RegistryBuilder builder;
  check(builder.begin(descriptor("dup.process", "observation", document)).code ==
            Code::ok,
        "duplicate process: first begin");
  check(builder.begin(descriptor("dup.process", "observation", document)).code ==
            Code::ok,
        "duplicate process: second begin is deferred to freeze");
  check(freeze_code(builder) == Code::identity_conflict,
        "duplicate process: freeze is identity_conflict");
  RegistryBuilder second;
  const Id other_document = temporary_document("duplicate-process-2.md");
  second.begin(descriptor("dup.process", "observation", other_document));
  second.begin(descriptor("dup.process", "observation", other_document));
  check(contains(freeze_reason(second), "duplicate process registration"),
        "duplicate process: named error message");
}

void duplicate_operation() {
  auto fixture = make_fixture();
  check(fixture->builder
            .add(fixture->process,
                 operation<U32, U64>("acquire", "acsd.test.In",
                                     "acsd.test.Out"))
            .code == Code::ok,
        "duplicate operation: second add is deferred to freeze");
  check(freeze_code(fixture->builder) == Code::identity_conflict,
        "duplicate operation: freeze is identity_conflict");
}

void duplicate_type_id() {
  RegistryBuilder builder;
  check(builder.register_type<U32>(good_codec<U32>("same.type")).code ==
            Code::ok,
        "type conflict: first registration");
  check(builder.register_type<U64>(good_codec<U64>("same.type")).code ==
            Code::ok,
        "type conflict: second registration is deferred to freeze");
  check(freeze_code(builder) == Code::identity_conflict,
        "same TypeId with a different C++ type: identity_conflict");
}

void repeated_type_id() {
  RegistryBuilder builder;
  check(builder.register_type<U32>(good_codec<U32>("same.type")).code ==
            Code::ok,
        "repeated type: first");
  check(builder.register_type<U32>(good_codec<U32>("same.type")).code ==
            Code::ok,
        "repeated type: second is deferred to freeze");
  check(freeze_code(builder) == Code::identity_conflict,
        "repeated TypeId: identity_conflict");
}

void missing_schema() {
  RegistryBuilder builder;
  TypeCodec<U32> codec = good_codec<U32>("acsd.test.In");
  codec.schema_json = std::string_view{};
  check(builder.register_type<U32>(codec).code == Code::ok,
        "missing schema: registration accepted, freeze validates");
  check(freeze_code(builder) == Code::invalid_model,
        "missing schema: invalid_model");
}

void missing_decode() {
  RegistryBuilder builder;
  TypeCodec<U32> codec = good_codec<U32>("acsd.test.In");
  codec.decode = nullptr;
  check(builder.register_type<U32>(codec).code == Code::invalid_model,
        "missing codec decode: register_type is invalid_model");
}

void missing_encode() {
  RegistryBuilder builder;
  TypeCodec<U32> codec = good_codec<U32>("acsd.test.In");
  codec.encode = nullptr;
  check(builder.register_type<U32>(codec).code == Code::invalid_model,
        "missing codec encode: register_type is invalid_model");
}

void missing_operation_codec() {
  auto fixture = make_fixture();
  // Replace the well-formed operation with one that names an unregistered
  // output TypeId.  The first fixture operation remains registered, which is
  // fine: freeze must reject the missing codec.
  check(fixture->builder
            .add(fixture->process,
                 operation<U32, U64>("acquire_missing", "acsd.test.In",
                                     "acsd.test.Unregistered"))
            .code == Code::ok,
        "missing operation codec: add");
  check(freeze_code(fixture->builder) == Code::invalid_model,
        "missing operation codec: invalid_model");
}

void wrong_cookie() {
  RegistryBuilder builder;
  const Id document = temporary_document("wrong-cookie.md");
  check(builder.register_type<U32>(good_codec<U32>("acsd.test.In")).code ==
            Code::ok,
        "wrong cookie: register U32 input");
  check(builder.register_type<U64>(good_codec<U64>("acsd.test.Out")).code ==
            Code::ok,
        "wrong cookie: register U64 output");
  auto process =
      builder.begin(descriptor("test.wrong", "observation", document));
  check(process.code == Code::ok, "wrong cookie: begin");
  if (process.code != Code::ok) return;
  // The operation's input C++ type is U64 while TypeId acsd.test.In is bound
  // to U32: a typed binding mismatch must fail before any run.
  check(builder.add(*process.value,
                    operation<U64, U64>("wrong", "acsd.test.In",
                                        "acsd.test.Out")).code == Code::ok,
        "wrong cookie: add");
  check(freeze_code(builder) == Code::invalid_model,
        "operation In/Out cookie mismatch: invalid_model");
}

void missing_hook() {
  auto fixture = make_fixture();
  Operation<U32, U64> broken = operation<U32, U64>(
      "broken_hook", "acsd.test.In", "acsd.test.Out");
  broken.plan = nullptr;
  check(fixture->builder.add(fixture->process, broken).code == Code::ok,
        "missing hook: add");
  check(freeze_code(fixture->builder) == Code::invalid_model,
        "incomplete validate/bounds/plan/run/finalize: invalid_model");
}

void missing_document() {
  RegistryBuilder builder;
  check(builder.register_type<U32>(good_codec<U32>("acsd.test.In")).code ==
            Code::ok,
        "missing document: register type");
  auto process = builder.begin(descriptor(
      "test.no_document", "observation", "/definitely/absent/process.md"));
  check(process.code == Code::ok, "missing document: begin");
  check(freeze_code(builder) == Code::missing_dependency,
        "descriptor document does not exist: missing_dependency");
  RegistryBuilder again;
  again.begin(descriptor("test.no_document", "observation",
                         "/definitely/absent/process.md"));
  check(contains(freeze_reason(again), "does not exist"),
        "missing document: named error message");
}

void seed_one(RegistryBuilder& builder,
              const std::shared_ptr<ProcessRegistration>& process,
              const char* name) {
  // One TypeId is registered exactly once per builder.
  static std::set<const void*> seeded;
  if (seeded.insert(&builder).second) {
    (void)builder.register_type<U32>(good_codec<U32>("acsd.test.In"));
    (void)builder.register_type<U64>(good_codec<U64>("acsd.test.Out"));
  }
  if (process)
    builder.add<U32, U64>(process, operation<U32, U64>(
                                       name, "acsd.test.In", "acsd.test.Out"));
}

void dependency_cycle() {
  const Id document = temporary_document("cycle.md");
  const Id document_b = temporary_document("cycle-b.md");
  RegistryBuilder builder;
  auto cycle_a = builder.begin(descriptor("test.cycle_a", "observation", document,
                                 {"test.cycle_b"}));
  check(cycle_a.code == Code::ok, "cycle: begin a");
  seed_one(builder, *cycle_a.value, "a");
  auto cycle_b = builder.begin(descriptor("test.cycle_b", "observation", document_b,
                                 {"test.cycle_a"}));
  check(cycle_b.code == Code::ok, "cycle: begin b");
  seed_one(builder, *cycle_b.value, "b");
  check(freeze_code(builder) == Code::invalid_model,
        "static dependency cycle: invalid_model");
  RegistryBuilder again;
  const Id other = temporary_document("cycle-2.md");
  const Id other_b = temporary_document("cycle-2-b.md");
  auto again_a =
      again.begin(descriptor("test.cycle_a", "observation", other, {"test.cycle_b"}));
  seed_one(again, *again_a.value, "a");
  auto again_b =
      again.begin(descriptor("test.cycle_b", "observation", other_b, {"test.cycle_a"}));
  seed_one(again, *again_b.value, "b");
  check(contains(freeze_reason(again), "dependency cycle"),
        "cycle: named error message");
}

void unknown_module_dependency() {
  const Id document = temporary_document("module.md");
  RegistryBuilder builder;
  check(builder.begin(descriptor("test.unknown_dep", "observation", document,
                                 {"not_a_cmake_target"}))
                .code == Code::ok,
        "unknown dependency: begin");
  check(freeze_code(builder) == Code::invalid_model,
        "unknown module dependency: invalid_model");
  RegistryBuilder again;
  const Id other = temporary_document("module-2.md");
  again.begin(descriptor("test.unknown_dep", "observation", other,
                         {"not_a_cmake_target"}));
  check(contains(freeze_reason(again), "unknown module"),
        "unknown dependency: named error message");
}

void valid_service_dependency() {
  const Id document = temporary_document("service.md");
  RegistryBuilder builder;
  auto service_dep = builder.begin(descriptor("test.service_dep", "storage", document,
                                 {"model", "service.storage",
                                  "runtime"}));
  check(service_dep.code == Code::ok, "service dependency: begin");
  seed_one(builder, *service_dep.value, "service");
  check(freeze_code(builder) == Code::ok,
        "service.* dependency is legal");
}

void probe_conflict() {
  const Id document = temporary_document("probe.md");
  const Id document_b = temporary_document("probe-b.md");
  RegistryBuilder builder;
  auto probe_a = builder.begin(descriptor("test.probe_a", "observation", document, {},
                                 {{"shared.probe", "span", 1}}));
  check(probe_a.code == Code::ok, "probe conflict: begin a");
  seed_one(builder, *probe_a.value, "pa");
  auto probe_b = builder.begin(descriptor("test.probe_b", "observation", document_b, {},
                                 {{"shared.probe", "span", 1}}));
  check(probe_b.code == Code::ok, "probe conflict: begin b");
  seed_one(builder, *probe_b.value, "pb");
  check(freeze_code(builder) == Code::identity_conflict,
        "probe id has exactly one owner: identity_conflict");
  RegistryBuilder again;
  const Id other = temporary_document("probe-2.md");
  const Id other_b = temporary_document("probe-2-b.md");
  auto again_a = again.begin(descriptor("test.probe_a", "observation", other, {},
                         {{"shared.probe", "span", 1}}));
  seed_one(again, *again_a.value, "pa");
  auto again_b = again.begin(descriptor("test.probe_b", "observation", other_b, {},
                         {{"shared.probe", "span", 1}}));
  seed_one(again, *again_b.value, "pb");
  check(contains(freeze_reason(again), "probe 'shared.probe'"),
        "probe conflict: named error message");
}

void repeated_freeze() {
  auto fixture = make_fixture();
  check(freeze_code(fixture->builder) == Code::ok, "repeated freeze: first");
  check(freeze_code(fixture->builder) == Code::identity_conflict,
        "repeated freeze: identity_conflict");
}

void immutable_after_freeze() {
  auto fixture = make_fixture();
  check(freeze_code(fixture->builder) == Code::ok, "immutable: freeze");
  check(fixture->builder.begin(descriptor("test.late", "observation",
                                         fixture->document))
                .code == Code::invalid_request,
        "immutable: begin after freeze is invalid_request");
  check(fixture->builder
                .register_type<U32>(good_codec<U32>("acsd.test.Late"))
                .code == Code::invalid_request,
        "immutable: register_type after freeze is invalid_request");
  check(fixture->builder
                .add(fixture->process,
                     operation<U32, U64>("late", "acsd.test.In",
                                         "acsd.test.Out"))
                .code == Code::invalid_request,
        "immutable: add after freeze is invalid_request");
}

void cross_builder_registration() {
  auto fixture = make_fixture();
  RegistryBuilder other;
  check(other
            .add(fixture->process,
                 operation<U32, U64>("foreign", "acsd.test.In",
                                     "acsd.test.Out"))
            .code == Code::invalid_request,
        "a ProcessRegistration cannot be added to another builder");
}

void foreign_registration_is_rejected() {
  RegistryBuilder builder;
  check(builder.add(nullptr, operation<U32, U64>("x", "a", "b")).code ==
            Code::invalid_request,
        "add with a null registration is invalid_request");
}

void empty_process_is_rejected() {
  const Id document = temporary_document("empty.md");
  RegistryBuilder builder;
  (void)builder.register_type<U32>(good_codec<U32>("acsd.test.In"));
  (void)builder.register_type<U64>(good_codec<U64>("acsd.test.Out"));
  check(builder.begin(descriptor("test.empty", "observation", document)).code ==
            Code::ok,
        "empty process: begin");
  check(freeze_code(builder) == Code::invalid_model,
        "a process without an operation cannot freeze");
  check(contains(freeze_reason(builder), "registers no operation"),
        "empty process: named error message");
}

void duplicate_document_is_rejected() {
  const Id document = temporary_document("shared-document.md");
  RegistryBuilder builder;
  (void)builder.register_type<U32>(good_codec<U32>("acsd.test.In"));
  (void)builder.register_type<U64>(good_codec<U64>("acsd.test.Out"));
  auto first = builder.begin(descriptor("test.doc_a", "observation", document));
  auto second = builder.begin(descriptor("test.doc_b", "observation", document));
  if (first.code == Code::ok)
    (void)builder.add<U32, U64>(
        first.value ? *first.value : nullptr,
        operation<U32, U64>("da", "acsd.test.In", "acsd.test.Out"));
  if (second.code == Code::ok)
    (void)builder.add<U32, U64>(
        second.value ? *second.value : nullptr,
        operation<U32, U64>("db", "acsd.test.In", "acsd.test.Out"));
  check(freeze_code(builder) == Code::identity_conflict,
        "two processes cannot share one document");
  check(contains(freeze_reason(builder), "share document"),
        "duplicate document: named error message");
}

}  // namespace

int main() {
  positive_contract();
  stable_manifest_order();
  duplicate_process();
  duplicate_operation();
  duplicate_type_id();
  repeated_type_id();
  missing_schema();
  missing_decode();
  missing_encode();
  missing_operation_codec();
  wrong_cookie();
  missing_hook();
  missing_document();
  dependency_cycle();
  unknown_module_dependency();
  valid_service_dependency();
  probe_conflict();
  repeated_freeze();
  immutable_after_freeze();
  cross_builder_registration();
  foreign_registration_is_rejected();
  empty_process_is_rejected();
  duplicate_document_is_rejected();

  if (failures != 0) {
    std::cerr << failures << " of " << checks << " registry checks failed\n";
    return 1;
  }
  std::cout << "registry negative/positive PASS: " << checks << " checks\n";
  return 0;
}
