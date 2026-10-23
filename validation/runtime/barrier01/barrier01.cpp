// S-BARRIER: with E=0, two stream cohorts submit their contributions to the
// R-side ordered accumulator; only after all contributions and EOF may the
// barrier region borrow F and run.  The plan's total progress floor is the
// maximum of the sequential region floors, never the sum.
//
// Fixed configuration (SCHEDULER.md S-BARRIER row): stream region 0 with two
// cohorts and floor 24, barrier region 1 with floor 16, one region edge
// ordered_accumulator with expected_contributions=2.  R (accumulator+metadata)
// is 24.  R+max(F)=48 must be admitted; R+max(F)-1=47 must be rejected; a host
// that charges R+sum(F)=64 would reject 48 and be caught by the same case.
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

struct BarrierIn {
  std::int64_t unused = 0;
};
struct BarrierOut {
  std::int64_t contributions = 0;
  std::int64_t barrier_seen = 0;
  std::int64_t region0_calls = 0;
  std::int64_t region1_calls = 0;
};

std::atomic<int> g_run_calls{0};
std::atomic<int> g_contributions{0};
std::atomic<int> g_barrier_seen{-1};
std::atomic<int> g_region0_calls{0};
std::atomic<int> g_region1_calls{0};
std::atomic<int> g_region_bindings{0};

Result<BarrierIn> decode_in(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<BarrierIn> out;
  out.code = Code::ok;
  out.value = BarrierIn{};
  return out;
}
Result<std::string> encode_in(rt::InvocationContext, const BarrierIn&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}
Result<BarrierOut> decode_out(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<BarrierOut> out;
  out.code = Code::ok;
  out.value = BarrierOut{};
  return out;
}
Result<std::string> encode_out(rt::InvocationContext, const BarrierOut&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}

Result<rt::Done> validate(rt::InvocationContext, Ref<BarrierIn>) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}
Result<rt::PlanBounds> bounds(rt::InvocationContext, Ref<BarrierIn>) {
  Result<rt::PlanBounds> out;
  out.code = Code::ok;
  out.value = rt::PlanBounds{1, 1, 1, false};
  return out;
}

rt::InvocationPlan make_barrier_plan(rt::PlanOptions options) {
  rt::InvocationPlan plan;
  plan.input_hash = vrt::zero_hash();
  plan.request_hash = vrt::zero_hash();
  plan.algorithm_hash = vrt::zero_hash();
  plan.options = options;
  plan.elements = 1;
  plan.block_elements = 1;
  plan.cohorts = 2;
  plan.persistent.host_bytes = 24;   // R: accumulator 16 + metadata 8
  plan.progress_floor.host_bytes = 24;  // max(region0=24, region1=16)
  plan.control_bytes = 8;
  plan.elastic = rt::ResourceVector{};  // E = 0
  plan.cleanup_bytes = 0;
  plan.cleanup_work_upper = 0;

  rt::RegionSpec stream;
  stream.id = 0;
  stream.kind = rt::RegionKind::stream;
  stream.cohorts = 2;
  stream.persistent.host_bytes = 0;
  stream.progress_floor.host_bytes = 24;
  stream.stages.push_back(vrt::make_stage(0, 8, 0, 1));
  stream.stages.push_back(vrt::make_stage(1, 8, 8, 1));
  stream.edges.push_back(vrt::make_edge(10, 1, 8, 0, 1));
  plan.regions.push_back(std::move(stream));

  rt::RegionSpec barrier;
  barrier.id = 1;
  barrier.kind = rt::RegionKind::barrier;
  barrier.cohorts = 1;
  barrier.persistent.host_bytes = 0;
  barrier.progress_floor.host_bytes = 16;
  barrier.stages.push_back(vrt::make_stage(0, 8, 8, 1));
  plan.regions.push_back(std::move(barrier));

  rt::RegionEdge edge;
  edge.from = 0;
  edge.to = 1;
  edge.transfer = rt::RegionTransfer::ordered_accumulator;
  edge.accumulator_bytes = 16;
  edge.expected_contributions = 2;
  plan.region_edges.push_back(edge);
  return plan;
}

Result<rt::InvocationPlan> plan(rt::InvocationContext, Ref<BarrierIn>,
                                rt::PlanOptions options) {
  Result<rt::InvocationPlan> out;
  out.code = Code::ok;
  out.value = make_barrier_plan(options);
  return out;
}

rt::Task<BarrierOut> run(rt::InvocationContext ctx, Ref<BarrierIn>,
                         Ref<rt::InvocationPlan> invocation_plan) {
  g_run_calls.fetch_add(1);
  Result<BarrierOut> failure;
  failure.code = Code::internal;
  if (!invocation_plan) return rt::ready(ctx, std::move(failure));

  // Cohort 0 and cohort 1 of the stream region contribute to the R-side
  // accumulator, then the barrier region runs once.
  for (int cohort = 0; cohort < 2; ++cohort) {
    auto body = [cohort](rt::InvocationContext region_ctx) -> rt::Task<std::int64_t> {
      g_region0_calls.fetch_add(1);
      if (region_ctx.region && *region_ctx.region == 0) g_region_bindings.fetch_add(1);
      g_contributions.fetch_add(1);
      Result<std::int64_t> out;
      out.code = Code::ok;
      out.value = cohort;
      return rt::ready(region_ctx, std::move(out));
    };
    auto task = rt::run_region<std::int64_t>(ctx, invocation_plan, 0, body);
    auto result = rt::wait_external(std::move(task));
    if (result.code != Code::ok) return rt::ready(ctx, std::move(failure));
  }
  {
    auto body = [](rt::InvocationContext region_ctx) -> rt::Task<BarrierOut> {
      g_region1_calls.fetch_add(1);
      if (region_ctx.region && *region_ctx.region == 1) g_region_bindings.fetch_add(1);
      g_barrier_seen.store(g_contributions.load());
      Result<BarrierOut> out;
      out.code = Code::ok;
      BarrierOut value;
      value.contributions = g_contributions.load();
      value.barrier_seen = g_barrier_seen.load();
      value.region0_calls = g_region0_calls.load();
      value.region1_calls = g_region1_calls.load();
      out.value = value;
      return rt::ready(region_ctx, std::move(out));
    };
    return rt::run_region<BarrierOut>(ctx, invocation_plan, 1, body);
  }
}

rt::Task<BarrierOut> finalize(rt::InvocationContext ctx, Ref<BarrierIn>,
                              Ref<rt::InvocationPlan>, Result<BarrierOut> partial) {
  return rt::ready<BarrierOut>(ctx, std::move(partial));
}

Result<rt::Done> register_process(rt::RegistryBuilder& builder) {
  auto in = builder.register_type<BarrierIn>(
      rt::TypeCodec<BarrierIn>{{"acsd.validation.barrier.in", 1},
                               "{\"type\":\"object\"}", &decode_in, &encode_in});
  if (in.code != Code::ok) return in;
  auto out_type = builder.register_type<BarrierOut>(
      rt::TypeCodec<BarrierOut>{{"acsd.validation.barrier.out", 1},
                                "{\"type\":\"object\"}", &decode_out, &encode_out});
  if (out_type.code != Code::ok) return out_type;
  rt::ProcessDescriptor descriptor;
  descriptor.process_id = "validation.barrier.process";
  descriptor.module = "validation.runtime";
  descriptor.document = "validation/runtime/README.md";
  auto process = builder.begin(std::move(descriptor));
  if (process.code != Code::ok || !process.value) {
    Result<rt::Done> out;
    out.code = process.code;
    return out;
  }
  rt::Operation<BarrierIn, BarrierOut> operation{
      "run", {"acsd.validation.barrier.in", 1}, {"acsd.validation.barrier.out", 1},
      &validate, &bounds, &plan, &run, &finalize};
  return builder.add<BarrierIn, BarrierOut>(*process.value, std::move(operation));
}

}  // namespace

int main() {
  vrt::Recorder recorder("barrier01", "validation/runtime/barrier01/barrier01.cpp");

  auto services = vrt::start_services(64ull << 20, 100, 2, 2, 2,
                                      vrt::scratch_root("barrier01"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("barrier.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  rt::RegistryBuilder builder;
  recorder.expect_code("barrier.register_process_ok", "ok", register_process(builder));
  auto frozen = builder.freeze();
  recorder.expect_code("barrier.freeze_ok", "ok", frozen);
  if (frozen.code != Code::ok) return recorder.finish();
  auto host = rt::make_host(*services.value, *frozen.value);
  recorder.expect_code("barrier.make_host_ok", "ok", host);
  if (host.code != Code::ok) return recorder.finish();
  const auto input = std::make_shared<BarrierIn>();

  const rt::InvocationPlan reference_plan = make_barrier_plan(rt::PlanOptions{});
  const acsd::U64 r_bytes = reference_plan.persistent.host_bytes;
  acsd::U64 sum_floor = 0;
  for (const auto& region : reference_plan.regions)
    sum_floor += region.progress_floor.host_bytes;
  const acsd::U64 max_floor = reference_plan.progress_floor.host_bytes;
  recorder.value("barrier.R", static_cast<long long>(r_bytes));
  recorder.value("barrier.max_F", static_cast<long long>(max_floor));
  recorder.value("barrier.sum_F", static_cast<long long>(sum_floor));
  recorder.check("barrier.plan_floor_is_max", max_floor == 24 && sum_floor == 40,
                 "plan floor 24, sum of regions 40", std::to_string(max_floor) + "/" +
                     std::to_string(sum_floor));
  recorder.check("barrier.elastic_zero",
                 rt::is_zero(reference_plan.elastic), "all zero elastic credits",
                 "E=0 fixed configuration");

  const acsd::U64 exact_ceiling = r_bytes + max_floor;  // 48
  auto exact = rt::open_command(*services.value, "validation.barrier.exact_max",
                                vrt::limits(exact_ceiling, 100, 0, 1, 1));
  recorder.expect_code("barrier.open_exact_command_ok", "ok", exact);
  if (exact.code == Code::ok) {
    auto result = rt::wait_external(
        (*host.value)->invoke<BarrierIn, BarrierOut>(*exact.value,
                                                     "validation.barrier.process", "run",
                                                     input));
    recorder.check("barrier.exact_R_plus_max_admitted", result.code == Code::ok, "ok",
                   vrt::code_of(result),
                   "a host that charges R+sum(F)=64 would have rejected this");
    recorder.check("barrier.run_called", g_run_calls.load() == 1, "1",
                   std::to_string(g_run_calls.load()));
    recorder.check("barrier.two_contributions", g_contributions.load() == 2, "2",
                   std::to_string(g_contributions.load()));
    recorder.check("barrier.barrier_sees_two_contributions",
                   g_barrier_seen.load() == 2, "2",
                   std::to_string(g_barrier_seen.load()));
    recorder.check("barrier.region0_calls", g_region0_calls.load() == 2, "2",
                   std::to_string(g_region0_calls.load()));
    recorder.check("barrier.region1_calls", g_region1_calls.load() == 1, "1",
                   std::to_string(g_region1_calls.load()));
    recorder.check("barrier.run_region_binds_region", g_region_bindings.load() == 3, "3",
                   std::to_string(g_region_bindings.load()));

    // run_region must reject an unknown region id before running the body.
    int unknown_body_runs = 0;
    auto unknown = rt::run_region<std::int64_t>(
        *exact.value, std::make_shared<rt::InvocationPlan>(reference_plan), 9,
        [&](rt::InvocationContext region_ctx) -> rt::Task<std::int64_t> {
          ++unknown_body_runs;
          Result<std::int64_t> out;
          out.code = Code::ok;
          out.value = 0;
          return rt::ready(region_ctx, std::move(out));
        });
    auto unknown_result = rt::wait_external(std::move(unknown));
    recorder.check("barrier.unknown_region_invalid_request",
                   unknown_result.code == Code::invalid_request && unknown_body_runs == 0,
                   "invalid_request and body not run",
                   vrt::code_of(unknown_result) + ", body runs=" +
                       std::to_string(unknown_body_runs));
    auto closed = rt::wait_external(rt::close_command(*exact.value));
    recorder.expect_code("barrier.close_exact_command_ok", "ok", closed);
  }

  const int run_calls_after_exact = g_run_calls.load();
  const acsd::U64 insufficient_ceiling = r_bytes + max_floor - 1;  // 47
  auto insufficient = rt::open_command(*services.value, "validation.barrier.insufficient",
                                       vrt::limits(insufficient_ceiling, 100, 0, 1, 1));
  recorder.expect_code("barrier.open_insufficient_command_ok", "ok", insufficient);
  if (insufficient.code == Code::ok) {
    auto result = rt::wait_external(
        (*host.value)->invoke<BarrierIn, BarrierOut>(*insufficient.value,
                                                     "validation.barrier.process", "run",
                                                     input));
    recorder.check("barrier.R_plus_max_minus_1_rejected",
                   result.code == Code::resource_exhausted, "resource_exhausted",
                   vrt::code_of(result));
    recorder.check("barrier.rejected_before_run", g_run_calls.load() == run_calls_after_exact,
                   std::to_string(run_calls_after_exact) + " run calls",
                   std::to_string(g_run_calls.load()));
    auto closed = rt::wait_external(rt::close_command(*insufficient.value));
    recorder.expect_code("barrier.close_insufficient_command_ok", "ok", closed);
  }

  auto host_closed = rt::wait_external((*host.value)->close_async());
  recorder.expect_code("barrier.host_close_ok", "ok", host_closed);
  recorder.note("cohort-to-accumulator hand-off and F release timing are not "
                "publicly observable; the admitted max(F) budget and the "
                "two-contribution barrier body are.");
  return recorder.finish();
}
