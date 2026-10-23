// S-FLOOR: the progress floor F covers the simultaneous live set of a cohort
// (input 8 + output 8 + scratch 8 = 24 bytes) and R is reserved separately; 24
// bytes completes, 23 is rejected before run starts.
//
// Fixed configuration (SCHEDULER.md S-FLOOR row): one stream region with two
// stages and one edge; the three 8-byte payloads are simultaneously live.  The
// test process returns plan.persistent=8 and plan.progress_floor=24 and its
// run() really allocates the three 8-byte payloads from ctx.progress.
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

struct FloorIn {
  std::int64_t unused = 0;
};
struct FloorOut {
  bool ran = false;
  std::uint64_t allocated = 0;
  bool progress_present = false;
};

std::atomic<int> g_run_calls{0};
std::atomic<unsigned long long> g_allocated{0};
std::atomic<int> g_progress_present{0};

Result<rt::Done> validate(rt::InvocationContext, Ref<FloorIn>) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}

Result<rt::PlanBounds> bounds(rt::InvocationContext, Ref<FloorIn>) {
  Result<rt::PlanBounds> out;
  out.code = Code::ok;
  out.value = rt::PlanBounds{1, 1, 1, false};
  return out;
}

Result<rt::InvocationPlan> plan(rt::InvocationContext, Ref<FloorIn>,
                                rt::PlanOptions options) {
  Result<rt::InvocationPlan> out;
  out.code = Code::ok;
  rt::InvocationPlan value;
  value.input_hash = vrt::zero_hash();
  value.request_hash = vrt::zero_hash();
  value.algorithm_hash = vrt::zero_hash();
  value.options = options;
  value.elements = 1;
  value.block_elements = 1;
  value.cohorts = 1;
  value.persistent.host_bytes = 8;
  value.progress_floor.host_bytes = 24;
  value.control_bytes = 8;
  value.elastic = rt::ResourceVector{};
  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = 1;
  region.persistent.host_bytes = 8;
  region.progress_floor.host_bytes = 24;
  region.stages.push_back(vrt::make_stage(0, 8, 0, 1));   // input 8
  region.stages.push_back(vrt::make_stage(1, 8, 8, 1));   // scratch 8 + output 8
  region.edges.push_back(vrt::make_edge(10, 1, 8, 0, 1));
  value.regions.push_back(std::move(region));
  out.value = std::move(value);
  return out;
}

rt::Task<FloorOut> run(rt::InvocationContext ctx, Ref<FloorIn>,
                       Ref<rt::InvocationPlan> invocation_plan) {
  g_run_calls.fetch_add(1);
  Result<FloorOut> out;
  out.code = Code::ok;
  FloorOut value;
  value.ran = true;
  value.progress_present = static_cast<bool>(ctx.progress);
  g_progress_present.store(value.progress_present ? 1 : 0);
  const acsd::Unit unit = vrt::dimensionless_unit();
  const auto index = vrt::make_index(24, "validation.floor.index");
  std::uint64_t allocated = 0;
  if (ctx.progress) {
    // Input 8 + output 8 + scratch 8 simultaneously live, as the plan says.
    std::vector<rt::Buffer<std::uint8_t>> live;
    live.reserve(3);
    for (int i = 0; i < 3; ++i) {
      auto buffer = rt::allocate<std::uint8_t>(ctx, ctx.progress,
                                               acsd::BlockRequest{0, 8}, index, unit,
                                               vrt::zero_hash());
      if (buffer.code != Code::ok) {
        out.code = Code::resource_exhausted;
        out.value = value;
        return rt::ready<FloorOut>(ctx, std::move(out));
      }
      allocated += 8;
      live.push_back(std::move(*buffer.value));
    }
    // The three payloads are alive together here; the vector release is the
    // "sink consumed the block" hand-off.
    value.allocated = allocated;
  } else {
    out.code = Code::resource_exhausted;
  }
  g_allocated.store(allocated);
  value.allocated = allocated;
  out.value = value;
  (void)invocation_plan;
  return rt::ready<FloorOut>(ctx, std::move(out));
}

rt::Task<FloorOut> finalize(rt::InvocationContext ctx, Ref<FloorIn>,
                            Ref<rt::InvocationPlan>, Result<FloorOut> partial) {
  return rt::ready<FloorOut>(ctx, std::move(partial));
}

Result<FloorIn> decode_in(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<FloorIn> out;
  out.code = Code::ok;
  out.value = FloorIn{};
  return out;
}
Result<std::string> encode_in(rt::InvocationContext, const FloorIn&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}
Result<FloorOut> decode_out(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<FloorOut> out;
  out.code = Code::ok;
  out.value = FloorOut{};
  return out;
}
Result<std::string> encode_out(rt::InvocationContext, const FloorOut&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}

Result<rt::Done> register_types(rt::RegistryBuilder& builder) {
  auto in = builder.register_type<FloorIn>(
      rt::TypeCodec<FloorIn>{{"acsd.validation.floor.in", 1},
                             "{\"type\":\"object\"}", &decode_in, &encode_in});
  if (in.code != Code::ok) return in;
  return builder.register_type<FloorOut>(
      rt::TypeCodec<FloorOut>{{"acsd.validation.floor.out", 1},
                              "{\"type\":\"object\"}", &decode_out, &encode_out});
}

Result<rt::Done> register_process(rt::RegistryBuilder& builder) {
  auto types = register_types(builder);
  if (types.code != Code::ok) return types;
  rt::ProcessDescriptor descriptor;
  descriptor.process_id = "validation.floor.process";
  descriptor.module = "validation.runtime";
  descriptor.document = "validation/runtime/README.md";
  descriptor.semantic_revision = 1;
  auto process = builder.begin(std::move(descriptor));
  if (process.code != Code::ok || !process.value) {
    Result<rt::Done> out;
    out.code = process.code;
    return out;
  }
  rt::Operation<FloorIn, FloorOut> operation{
      "run",
      {"acsd.validation.floor.in", 1},
      {"acsd.validation.floor.out", 1},
      &validate,
      &bounds,
      &plan,
      &run,
      &finalize};
  return builder.add<FloorIn, FloorOut>(*process.value, std::move(operation));
}

}  // namespace

int main() {
  vrt::Recorder recorder("floor01", "validation/runtime/floor01/floor01.cpp");

  auto services = vrt::start_services(64ull << 20, 100, 2, 2, 2,
                                      vrt::scratch_root("floor01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("floor.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }

  rt::RegistryBuilder builder;
  recorder.expect_code("floor.register_process_ok", "ok", register_process(builder));
  auto frozen = builder.freeze();
  recorder.expect_code("floor.freeze_ok", "ok", frozen);
  if (frozen.code != Code::ok || !frozen.value) return recorder.finish();
  auto host = rt::make_host(*services.value, *frozen.value);
  recorder.expect_code("floor.make_host_ok", "ok", host);
  if (host.code != Code::ok || !host.value) return recorder.finish();

  const auto input = std::make_shared<FloorIn>();

  // --- exactly R+F = 32 bytes (R=8, F=24) must complete. -------------------
  auto command = rt::open_command(*services.value, "validation.floor.exact",
                                  vrt::limits(32, 100, 0, 1, 1));
  recorder.expect_code("floor.open_exact_command_ok", "ok", command);
  if (command.code == Code::ok) {
    auto result = rt::wait_external(
        (*host.value)->invoke<FloorIn, FloorOut>(*command.value,
                                                 "validation.floor.process", "run",
                                                 input));
    recorder.check("floor.exact_32_ok", result.code == Code::ok, "ok",
                   vrt::code_of(result));
    recorder.check("floor.exact_run_called", g_run_calls.load() == 1, "1",
                   std::to_string(g_run_calls.load()));
    recorder.check("floor.progress_arena_present", g_progress_present.load() == 1,
                   "ctx.progress != null", std::to_string(g_progress_present.load()));
    recorder.check("floor.exact_allocated_24", g_allocated.load() == 24, "24",
                   std::to_string(g_allocated.load()));
    const rt::ScopeSnapshot snapshot = rt::snapshot(command.value->scope);
    recorder.value("floor.held", static_cast<long long>(snapshot.held.host_bytes));
    recorder.value("floor.claimed", static_cast<long long>(snapshot.claimed.host_bytes));
    recorder.value("floor.allocated", static_cast<long long>(snapshot.allocated.host_bytes));
    recorder.value("floor.peak", static_cast<long long>(snapshot.peak_allocated.host_bytes));
    recorder.check("floor.peak_covers_24",
                   snapshot.peak_allocated.host_bytes >= 24,
                   "peak_allocated.host_bytes >= 24",
                   std::to_string(snapshot.peak_allocated.host_bytes));
    recorder.check("floor.held_allocated_separate",
                   snapshot.held.host_bytes >= snapshot.allocated.host_bytes,
                   "held >= allocated",
                   std::to_string(snapshot.held.host_bytes) + " >= " +
                       std::to_string(snapshot.allocated.host_bytes),
                   "the plan's R is held even when it is not physically committed");
    auto closed = rt::wait_external(rt::close_command(*command.value));
    recorder.expect_code("floor.close_exact_command_ok", "ok", closed);
  }
  const int run_calls_after_exact = g_run_calls.load();

  // --- R+F-1 = 31 bytes must be rejected before run. -----------------------
  auto small = rt::open_command(*services.value, "validation.floor.small",
                                 vrt::limits(31, 100, 0, 1, 1));
  recorder.expect_code("floor.open_small_command_ok", "ok", small);
  if (small.code == Code::ok) {
    auto result = rt::wait_external(
        (*host.value)->invoke<FloorIn, FloorOut>(*small.value,
                                                 "validation.floor.process", "run",
                                                 input));
    recorder.check("floor.small_31_rejected_resource_exhausted",
                   result.code == Code::resource_exhausted, "resource_exhausted",
                   vrt::code_of(result));
    recorder.check("floor.small_31_before_run", g_run_calls.load() == run_calls_after_exact,
                   std::to_string(run_calls_after_exact) + " run calls",
                   std::to_string(g_run_calls.load()),
                   "the progress floor may never be silently shrunk");
    recorder.check("floor.small_31_no_value", !result.value.has_value(), "no value",
                   result.value ? "value present" : "no value");
    auto closed = rt::wait_external(rt::close_command(*small.value));
    recorder.expect_code("floor.close_small_command_ok", "ok", closed);
  }

  auto host_closed = rt::wait_external((*host.value)->close_async());
  recorder.expect_code("floor.host_close_ok", "ok", host_closed);
  return recorder.finish();
}
