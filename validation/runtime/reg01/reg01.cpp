// REG01: two typed test processes register, freeze and invoke; every ambiguity
// and wrong-call path must be rejected by name before science runs.
//
// Independent criteria (docs/engineering/rebuild/VERIFICATION.md REG01 row and
// FOUNDATION_API.md "计划、注册和 Host"):
//   * freeze validates duplicate process/operation keys, missing schema/codec,
//     TypeId<->C++ type mismatch, missing document and static dependency cycles;
//   * an unfrozen builder cannot become a ProcessHost;
//   * a known process with an unknown operation, and an unknown process, are
//     unsupported;
//   * a wrong Request type is invalid_request *before* run;
//   * the two positive typed invocations return the values the test process
//     computes itself (no science implementation involved).
//
// The test process bodies only move an integer; they are fixtures, not
// production algorithms.  Only <acsd/contracts.hpp> is included.
#include <acsd/contracts.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Ref;
using acsd::Result;
using acsd::U32;

struct AlphaIn {
  std::int64_t value = 0;
};
struct AlphaOut {
  std::int64_t value = 0;
};
struct BetaIn {
  std::int64_t value = 0;
};
struct BetaOut {
  std::int64_t value = 0;
};
struct WrongIn {
  std::int64_t value = 0;
};

std::atomic<int> g_alpha_runs{0};
std::atomic<int> g_beta_runs{0};

std::string schema(const char* name) {
  return std::string("{\"$id\":\"") + name +
         "\",\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"integer\"}},"
         "\"required\":[\"value\"]}";
}

template <class T>
Result<T> decode_value(rt::InvocationContext, std::string_view text, rt::WireObjects&) {
  Result<T> out;
  const std::size_t colon = text.find(':');
  const std::size_t begin = text.find_first_of("-0123456789", colon == std::string::npos ? 0 : colon);
  if (begin == std::string::npos) {
    out.code = Code::invalid_model;
    return out;
  }
  std::size_t end = begin;
  while (end < text.size() &&
         (text[end] == '-' || (text[end] >= '0' && text[end] <= '9')))
    ++end;
  try {
    T value;
    value.value = std::stoll(std::string(text.substr(begin, end - begin)));
    out.code = Code::ok;
    out.value = value;
  } catch (...) {
    out.code = Code::invalid_model;
  }
  return out;
}

template <class T>
Result<std::string> encode_value(rt::InvocationContext, const T& value, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{\"value\":" + std::to_string(value.value) + "}";
  return out;
}

template <class T>
Result<rt::Done> validate_any(rt::InvocationContext, Ref<T>) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}

template <class In>
Result<rt::PlanBounds> bounds_any(rt::InvocationContext, Ref<In>) {
  Result<rt::PlanBounds> out;
  out.code = Code::ok;
  out.value = rt::PlanBounds{1, 64, 1, false};
  return out;
}

template <class In>
Result<rt::InvocationPlan> plan_any(rt::InvocationContext, Ref<In>, rt::PlanOptions options) {
  Result<rt::InvocationPlan> out;
  out.code = Code::ok;
  out.value = vrt::make_plan(0, {}, {}, 0, 0);
  out.value->options = options;
  return out;
}

rt::Task<AlphaOut> run_alpha(rt::InvocationContext ctx, Ref<AlphaIn> input,
                             Ref<rt::InvocationPlan>) {
  g_alpha_runs.fetch_add(1);
  Result<AlphaOut> out;
  out.code = Code::ok;
  out.value = AlphaOut{input->value + 1};
  return rt::ready<AlphaOut>(ctx, std::move(out));
}

rt::Task<BetaOut> run_beta(rt::InvocationContext ctx, Ref<BetaIn> input,
                           Ref<rt::InvocationPlan>) {
  g_beta_runs.fetch_add(1);
  Result<BetaOut> out;
  out.code = Code::ok;
  out.value = BetaOut{input->value * 100};
  return rt::ready<BetaOut>(ctx, std::move(out));
}

template <class In, class Out>
rt::Task<Out> finalize_passthrough(rt::InvocationContext ctx, Ref<In>,
                                   Ref<rt::InvocationPlan>, Result<Out> partial) {
  return rt::ready<Out>(ctx, std::move(partial));
}

Result<rt::Done> register_alpha_types(rt::RegistryBuilder& builder) {
  auto in = builder.register_type<AlphaIn>(
      rt::TypeCodec<AlphaIn>{{"acsd.validation.alpha.in", 1},
                             schema("acsd.validation.alpha.in"), &decode_value<AlphaIn>,
                             &encode_value<AlphaIn>});
  if (in.code != Code::ok) return in;
  return builder.register_type<AlphaOut>(
      rt::TypeCodec<AlphaOut>{{"acsd.validation.alpha.out", 1},
                              schema("acsd.validation.alpha.out"),
                              &decode_value<AlphaOut>, &encode_value<AlphaOut>});
}

Result<rt::Done> register_beta_types(rt::RegistryBuilder& builder) {
  auto in = builder.register_type<BetaIn>(
      rt::TypeCodec<BetaIn>{{"acsd.validation.beta.in", 1},
                            schema("acsd.validation.beta.in"), &decode_value<BetaIn>,
                            &encode_value<BetaIn>});
  if (in.code != Code::ok) return in;
  return builder.register_type<BetaOut>(
      rt::TypeCodec<BetaOut>{{"acsd.validation.beta.out", 1},
                             schema("acsd.validation.beta.out"),
                             &decode_value<BetaOut>, &encode_value<BetaOut>});
}

rt::ProcessDescriptor descriptor_for(const std::string& process_id,
                                     std::vector<acsd::Id> dependencies = {}) {
  rt::ProcessDescriptor descriptor;
  descriptor.process_id = process_id;
  descriptor.module = "validation.runtime";
  // Each process has its own document: the frozen registry rejects two
  // processes that share one document (and processes with zero operations).
  descriptor.document = "validation/runtime/reg01/docs/" + process_id + ".md";
  descriptor.semantic_revision = 1;
  descriptor.dependencies = std::move(dependencies);
  return descriptor;
}

rt::Operation<AlphaIn, AlphaOut> alpha_operation(const std::string& operation_id) {
  return rt::Operation<AlphaIn, AlphaOut>{
      operation_id,
      {"acsd.validation.alpha.in", 1},
      {"acsd.validation.alpha.out", 1},
      &validate_any<AlphaIn>,
      &bounds_any<AlphaIn>,
      &plan_any<AlphaIn>,
      &run_alpha,
      &finalize_passthrough<AlphaIn, AlphaOut>};
}

rt::Operation<BetaIn, BetaOut> beta_operation(const std::string& operation_id) {
  return rt::Operation<BetaIn, BetaOut>{
      operation_id,
      {"acsd.validation.beta.in", 1},
      {"acsd.validation.beta.out", 1},
      &validate_any<BetaIn>,
      &bounds_any<BetaIn>,
      &plan_any<BetaIn>,
      &run_beta,
      &finalize_passthrough<BetaIn, BetaOut>};
}

// Register alpha (and optionally beta) completely.  Returns the first failure.
Result<rt::Done> register_alpha_process(rt::RegistryBuilder& builder,
                                        const std::string& process_id,
                                        const std::string& operation_id) {
  auto types = register_alpha_types(builder);
  if (types.code != Code::ok) return types;
  auto process = builder.begin(descriptor_for(process_id));
  if (process.code != Code::ok || !process.value) {
    Result<rt::Done> out;
    out.code = process.code;
    return out;
  }
  return builder.add<AlphaIn, AlphaOut>(*process.value,
                                        alpha_operation(operation_id));
}

Result<rt::Done> register_beta_process(rt::RegistryBuilder& builder,
                                       const std::string& process_id,
                                       const std::string& operation_id) {
  auto types = register_beta_types(builder);
  if (types.code != Code::ok) return types;
  auto process = builder.begin(descriptor_for(process_id));
  if (process.code != Code::ok || !process.value) {
    Result<rt::Done> out;
    out.code = process.code;
    return out;
  }
  return builder.add<BetaIn, BetaOut>(*process.value, beta_operation(operation_id));
}

bool rejected(Code code) {
  return code != Code::ok;
}

template <class T>
std::string code_text(const Result<T>& result) {
  return vrt::code_name(result.code);
}

struct Session {
  std::shared_ptr<rt::Services> services;
  Result<rt::InvocationContext> command;
  std::shared_ptr<rt::ProcessHost> host;
  std::shared_ptr<const rt::Registry> registry;
};

}  // namespace

int main() {
  vrt::Recorder recorder("reg01", "validation/runtime/reg01/reg01.cpp");
  recorder.command("built by validation/runtime/build.sh; run by run_cases.py");

  auto services = vrt::start_services(1ull << 26, 1000, 2, 2, 2,
                                      vrt::scratch_root("reg01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("services.make", false, "ok", vrt::code_of(services),
                   "the runtime cannot even create the unique service pool");
    return recorder.finish();
  }
  recorder.value("services", "created");
  auto services_ptr = *services.value;

  auto open = [&](const std::string& command_id, U32 weight = 1) {
    return rt::open_command(services_ptr, command_id,
                            vrt::limits(1ull << 26, 1000, 0, 2, 2), weight);
  };

  // -------------------------------------------------------------------------
  // Positive path: two processes, freeze, typed invoke.
  // -------------------------------------------------------------------------
  rt::RegistryBuilder builder;
  auto alpha = register_alpha_process(builder, "validation.reg.alpha", "run");
  recorder.expect_code("reg.alpha_process_registered", "ok", alpha);
  auto beta = register_beta_process(builder, "validation.reg.beta", "run");
  recorder.expect_code("reg.beta_process_registered", "ok", beta);

  auto frozen = builder.freeze();
  recorder.expect_code("reg.freeze_ok", "ok", frozen);
  if (frozen.code == Code::ok && frozen.value) {
    recorder.value("registry.has.alpha.run", (*frozen.value)->has("validation.reg.alpha", "run"));
    recorder.value("registry.has.beta.run", (*frozen.value)->has("validation.reg.beta", "run"));
    recorder.value("manifest.length",
                   static_cast<long long>((*frozen.value)->manifest_json().size()));
    recorder.check("reg.manifest_nonempty",
                   !(*frozen.value)->manifest_json().empty(), "non-empty manifest",
                   std::to_string((*frozen.value)->manifest_json().size()) + " bytes");

    auto host = rt::make_host(services_ptr, *frozen.value);
    recorder.expect_code("reg.make_host_ok", "ok", host);
    if (host.code == Code::ok && host.value) {
      auto command = open("validation.reg.command");
      recorder.expect_code("reg.open_command_ok", "ok", command);
      if (command.code == Code::ok) {
        auto alpha_in = std::make_shared<AlphaIn>();
        alpha_in->value = 41;
        g_alpha_runs.store(0);
        auto result = rt::wait_external(
            (*host.value)->invoke<AlphaIn, AlphaOut>(*command.value,
                                                  "validation.reg.alpha", "run",
                                                  alpha_in));
        recorder.check("reg.invoke_alpha_ok", result.code == Code::ok,
                       "ok", vrt::code_of(result));
        recorder.check("reg.invoke_alpha_value",
                       result.code == Code::ok && result.value &&
                           result.value->value == 42,
                       "42", result.value ? std::to_string(result.value->value)
                                           : "no value");
        recorder.check("reg.invoke_alpha_ran_once", g_alpha_runs.load() == 1, "1",
                       std::to_string(g_alpha_runs.load()));

        auto beta_in = std::make_shared<BetaIn>();
        beta_in->value = 7;
        auto beta_result = rt::wait_external(
            (*host.value)->invoke<BetaIn, BetaOut>(*command.value, "validation.reg.beta",
                                                "run", beta_in));
        recorder.check("reg.invoke_beta_ok", beta_result.code == Code::ok, "ok",
                       vrt::code_of(beta_result));
        recorder.check("reg.invoke_beta_value",
                       beta_result.code == Code::ok && beta_result.value &&
                           beta_result.value->value == 700,
                       "700", beta_result.value
                                  ? std::to_string(beta_result.value->value)
                                  : "no value");

        // Unknown process / unknown operation: both must be unsupported.
        auto unknown_process = rt::wait_external(
            (*host.value)->invoke<AlphaIn, AlphaOut>(*command.value,
                                                  "validation.reg.unknown", "run",
                                                  alpha_in));
        recorder.check("reg.unknown_process_unsupported",
                       unknown_process.code == Code::unsupported, "unsupported",
                       vrt::code_of(unknown_process));
        auto unknown_operation = rt::wait_external(
            (*host.value)->invoke<AlphaIn, AlphaOut>(*command.value,
                                                  "validation.reg.alpha",
                                                  "unknown_operation", alpha_in));
        recorder.check("reg.unknown_operation_unsupported",
                       unknown_operation.code == Code::unsupported, "unsupported",
                       vrt::code_of(unknown_operation));

        // Wrong Request type: rejected before run.
        g_alpha_runs.store(0);
        auto wrong = std::make_shared<WrongIn>();
        wrong->value = 5;
        auto wrong_result = rt::wait_external(
            (*host.value)->invoke<WrongIn, AlphaOut>(*command.value, "validation.reg.alpha",
                                                  "run", wrong));
        recorder.check("reg.wrong_request_invalid_request",
                       wrong_result.code == Code::invalid_request, "invalid_request",
                       vrt::code_of(wrong_result));
        recorder.check("reg.wrong_request_before_run", g_alpha_runs.load() == 0, "0",
                       std::to_string(g_alpha_runs.load()));

        auto closed = rt::wait_external(rt::close_command(*command.value));
        recorder.check("reg.close_command_ok", closed.code == Code::ok, "ok",
                       vrt::code_of(closed));
      }
      auto host_closed = rt::wait_external((*host.value)->close_async());
      recorder.check("reg.host_close_async_ok", host_closed.code == Code::ok, "ok",
                     vrt::code_of(host_closed));
    }
  }

  // -------------------------------------------------------------------------
  // Unfrozen builder: no ProcessHost may be produced from it.  The public API
  // only admits a ProcessHost from a frozen Registry, so the observable
  // negative is that a null registry is rejected and the builder carries no
  // host entry point.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder pending;
    auto type = pending.register_type<AlphaIn>(
        rt::TypeCodec<AlphaIn>{{"acsd.validation.alpha.in", 1},
                               schema("acsd.validation.alpha.in"),
                               &decode_value<AlphaIn>, &encode_value<AlphaIn>});
    recorder.expect_code("reg.unfrozen_type_register_ok", "ok", type);
    auto host = rt::make_host(services_ptr, nullptr);
    recorder.check("reg.unfrozen_make_host_rejected", host.code != Code::ok,
                   "non-ok", vrt::code_of(host),
                   "an unfrozen builder has no registry; make_host(nullptr) must fail");
  }

  // -------------------------------------------------------------------------
  // Duplicate process id.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder duplicate;
    auto dup_types = register_alpha_types(duplicate);
    auto first = duplicate.begin(descriptor_for("validation.reg.duplicate"));
    bool second_rejected = false;
    if (dup_types.code == Code::ok && first.code == Code::ok && first.value) {
      auto first_op = duplicate.add<AlphaIn, AlphaOut>(*first.value, alpha_operation("run"));
      recorder.expect_code("reg.duplicate_process.first_add_ok", "ok", first_op);
      auto second = duplicate.begin(descriptor_for("validation.reg.duplicate"));
      second_rejected = rejected(second.code);
      if (second.code == Code::ok && second.value)
        duplicate.add<AlphaIn, AlphaOut>(*second.value, alpha_operation("run"));
      recorder.value("reg.duplicate_process.second_begin_code",
                     vrt::code_name(second.code));
    }
    auto frozen_duplicate = duplicate.freeze();
    const bool freeze_rejected = rejected(frozen_duplicate.code);
    recorder.check("reg.duplicate_process_rejected", second_rejected || freeze_rejected,
                   "begin or freeze rejects duplicate process_id",
                   "begin2=" + std::string(second_rejected ? "rejected" : "accepted") +
                       ", freeze=" + vrt::code_name(frozen_duplicate.code),
                   "VERIFICATION REG01: duplicate process must be a named rejection");
  }

  // -------------------------------------------------------------------------
  // Duplicate operation id on one process.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder duplicate;
    auto types = register_alpha_types(duplicate);
    auto process = duplicate.begin(descriptor_for("validation.reg.dup_op"));
    bool second_rejected = false;
    std::string second_code = "not_run";
    if (types.code == Code::ok && process.code == Code::ok && process.value) {
      auto first = duplicate.add<AlphaIn, AlphaOut>(*process.value, alpha_operation("run"));
      recorder.expect_code("reg.duplicate_operation.first_add_ok", "ok", first);
      auto second = duplicate.add<AlphaIn, AlphaOut>(*process.value, alpha_operation("run"));
      second_rejected = rejected(second.code);
      second_code = vrt::code_name(second.code);
    }
    auto frozen_duplicate = duplicate.freeze();
    const bool freeze_rejected = rejected(frozen_duplicate.code);
    recorder.check("reg.duplicate_operation_rejected", second_rejected || freeze_rejected,
                   "add or freeze rejects duplicate operation_id",
                   "add2=" + second_code + ", freeze=" + vrt::code_name(frozen_duplicate.code));
  }

  // -------------------------------------------------------------------------
  // Missing codec: the operation input type is never registered.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder missing;
    auto process = missing.begin(descriptor_for("validation.reg.no_codec"));
    if (process.code == Code::ok && process.value) {
      auto added = missing.add<AlphaIn, AlphaOut>(*process.value, alpha_operation("run"));
      recorder.expect_code("reg.no_codec.add_ok", "ok", added);
    }
    auto frozen_missing = missing.freeze();
    recorder.check("reg.missing_codec_rejected", rejected(frozen_missing.code),
                   "freeze rejects an operation whose In/Out has no registered codec",
                   vrt::code_name(frozen_missing.code));
  }

  // -------------------------------------------------------------------------
  // Missing schema: a registered type with an empty schema JSON.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder empty_schema;
    auto type = empty_schema.register_type<AlphaIn>(
        rt::TypeCodec<AlphaIn>{{"acsd.validation.alpha.in", 1}, "", &decode_value<AlphaIn>,
                               &encode_value<AlphaIn>});
    bool rejected_at_register = rejected(type.code);
    auto out_type = empty_schema.register_type<AlphaOut>(
        rt::TypeCodec<AlphaOut>{{"acsd.validation.alpha.out", 1},
                                schema("acsd.validation.alpha.out"),
                                &decode_value<AlphaOut>, &encode_value<AlphaOut>});
    auto process = empty_schema.begin(descriptor_for("validation.reg.empty_schema"));
    if (out_type.code == Code::ok && process.code == Code::ok && process.value)
      empty_schema.add<AlphaIn, AlphaOut>(*process.value, alpha_operation("run"));
    auto frozen_empty = empty_schema.freeze();
    bool rejected_at_freeze = rejected(frozen_empty.code);
    recorder.check("reg.missing_schema_rejected", rejected_at_register || rejected_at_freeze,
                   "register_type or freeze rejects an empty schema_json",
                   std::string("register=") + vrt::code_name(type.code) +
                       ", freeze=" + vrt::code_name(frozen_empty.code));
  }

  // -------------------------------------------------------------------------
  // TypeId bound to a different C++ type (codec cookie mismatch).
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder mismatch;
    auto alpha_type = mismatch.register_type<AlphaIn>(
        rt::TypeCodec<AlphaIn>{{"acsd.validation.alpha.in", 1},
                               schema("acsd.validation.alpha.in"),
                               &decode_value<AlphaIn>, &encode_value<AlphaIn>});
    auto out_type = mismatch.register_type<AlphaOut>(
        rt::TypeCodec<AlphaOut>{{"acsd.validation.alpha.out", 1},
                                schema("acsd.validation.alpha.out"),
                                &decode_value<AlphaOut>, &encode_value<AlphaOut>});
    auto process = mismatch.begin(descriptor_for("validation.reg.type_mismatch"));
    if (alpha_type.code == Code::ok && out_type.code == Code::ok &&
        process.code == Code::ok && process.value) {
      rt::Operation<WrongIn, AlphaOut> operation{
          "run",
          {"acsd.validation.alpha.in", 1},  // TypeId registered for AlphaIn
          {"acsd.validation.alpha.out", 1},
          &validate_any<WrongIn>,
          &bounds_any<WrongIn>,
          &plan_any<WrongIn>,
          nullptr,  // run must not be needed for a registration rejection
          nullptr};
      auto added = mismatch.add<WrongIn, AlphaOut>(*process.value, operation);
      recorder.value("reg.type_mismatch.add_code", vrt::code_name(added.code));
    }
    auto frozen_mismatch = mismatch.freeze();
    recorder.check("reg.typeid_cpp_mismatch_rejected", rejected(frozen_mismatch.code),
                   "freeze rejects TypeId bound to a different C++ In type",
                   vrt::code_name(frozen_mismatch.code));
  }

  // -------------------------------------------------------------------------
  // Missing document.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder no_doc;
    auto types = register_alpha_types(no_doc);
    auto descriptor = descriptor_for("validation.reg.no_document");
    descriptor.document = "";
    auto process = no_doc.begin(std::move(descriptor));
    if (types.code == Code::ok && process.code == Code::ok && process.value) {
      auto added = no_doc.add<AlphaIn, AlphaOut>(*process.value, alpha_operation("run"));
      recorder.value("reg.no_document.add_code", vrt::code_name(added.code));
    }
    auto frozen_no_doc = no_doc.freeze();
    recorder.check("reg.missing_document_rejected", rejected(frozen_no_doc.code),
                   "begin or freeze rejects an empty ProcessDescriptor.document",
                   vrt::code_name(frozen_no_doc.code));
  }

  // -------------------------------------------------------------------------
  // Static dependency cycle between two registered processes.
  // -------------------------------------------------------------------------
  {
    rt::RegistryBuilder cycle;
    auto types = register_alpha_types(cycle);
    if (types.code == Code::ok) {
      auto a = cycle.begin(descriptor_for("validation.reg.cycle.a",
                                          {"validation.reg.cycle.b"}));
      auto b = cycle.begin(descriptor_for("validation.reg.cycle.b",
                                          {"validation.reg.cycle.a"}));
      if (a.code == Code::ok && a.value)
        cycle.add<AlphaIn, AlphaOut>(*a.value, alpha_operation("run"));
      if (b.code == Code::ok && b.value)
        cycle.add<AlphaIn, AlphaOut>(*b.value, alpha_operation("run"));
    }
    auto frozen_cycle = cycle.freeze();
    recorder.check("reg.dependency_cycle_rejected", rejected(frozen_cycle.code),
                   "freeze rejects a static dependency cycle",
                   vrt::code_name(frozen_cycle.code),
                   "A depends on B and B depends on A (ProcessDescriptor.dependencies)");
  }

  recorder.resource("checks", static_cast<long long>(recorder.attempts()));
  recorder.resource("successes", static_cast<long long>(recorder.successes()));
  return recorder.finish();
}
