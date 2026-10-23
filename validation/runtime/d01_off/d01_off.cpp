// D01-OFF: with ACSD_DEV_MODE=0 the probe macros evaluate no argument at all.
//
// Two independent observables:
//   1. compile-time: the macro arguments use identifiers and expressions that
//      do not exist in this build; if the OFF expansion referenced them the
//      translation unit would not compile;
//   2. run-time: the ctx expression has a side effect counter that must stay 0,
//      and a payload constructor that would abort must never run.
//
// This case needs no runtime library: the OFF macros expand to an empty block.
#include <acsd/contracts.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "harness.hpp"

namespace {

int g_ctx_evaluations = 0;
int g_payload_constructions = 0;

// The ctx expression of the macro.  In a correct OFF build it is never read.
int ctx_expression_with_side_effect() {
  ++g_ctx_evaluations;
  return 7;
}

// The payload expression.  In a correct OFF build its identifiers need not
// exist, and even if they did the constructor must not run.
struct PayloadThatMustNotBeConstructed {
  PayloadThatMustNotBeConstructed() {
    ++g_payload_constructions;
    std::abort();
  }
};

}  // namespace

int main() {
  vrt::Recorder recorder("d01_off", "validation/runtime/d01_off/d01_off.cpp");

#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
  recorder.check("d01_off.compiled_with_off", false, "ACSD_DEV_MODE=0",
                 "ACSD_DEV_MODE is ON in this translation unit",
                 "build.sh must compile this case with -DACSD_DEV_MODE=0");
#else
  recorder.check("d01_off.compiled_with_off", true, "ACSD_DEV_MODE=0", "off");
#endif

  // Arguments below are deliberately not declared anywhere in this build.
  ACSD_DEV_PERF(ctx_expression_with_side_effect(),
                this_perf_record_does_not_exist_in_this_build);
  ACSD_DEV_SNAPSHOT(ctx_expression_with_side_effect(),
                    this_probe_id_does_not_exist,
                    this_probe_tags_does_not_exist,
                    this_snapshot_payload_does_not_exist);
  // A valid expression is also used, to prove the absence of evaluation is not
  // an artefact of an undeclared identifier.
  ACSD_DEV_PERF(ctx_expression_with_side_effect(), PayloadThatMustNotBeConstructed{});
  ACSD_DEV_SNAPSHOT(ctx_expression_with_side_effect(), "validation.d01.off",
                    "tags", PayloadThatMustNotBeConstructed{});

  recorder.check("d01_off.ctx_not_evaluated", g_ctx_evaluations == 0,
                 "0 ctx evaluations", std::to_string(g_ctx_evaluations),
                 "the OFF macro must not evaluate its ctx argument");
  recorder.check("d01_off.payload_not_constructed", g_payload_constructions == 0,
                 "0 payload constructions", std::to_string(g_payload_constructions),
                 "a payload constructor that aborts must never run");
  recorder.note("compile-only zero-evaluation is proven by this TU compiling with "
                "undeclared identifiers; the run-time counters cover the same macros "
                "with declared arguments.");

  return recorder.finish();
}
