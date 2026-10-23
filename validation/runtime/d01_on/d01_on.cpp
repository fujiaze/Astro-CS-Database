// D01-ON: with ACSD_DEV_MODE=1 the try-only probe entries and macros must not
// change the scientific code, model or usage; an unselected probe is a no-op
// and does not even construct its payload.
//
// Publicly observable in this build: probe_selected, try_record, try_snapshot,
// allocate_span_id, steady_ns, probe_construction_failed and the two macros.
// A selected diagnostic sink, its quota and its disk-failure policy have no
// public configuration entry in the frozen headers; that part of D01 is
// recorded as not covered rather than fabricated.
#include <acsd/contracts.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Ref;
using acsd::Result;

struct ProbeIn {
  std::int64_t value = 0;
};
struct ProbeOut {
  std::int64_t value = 0;
  std::uint64_t span_a = 0;
  std::uint64_t span_b = 0;
};

std::atomic<int> g_run_calls{0};
std::atomic<int> g_exploding_payloads{0};

Result<ProbeIn> decode_in(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<ProbeIn> out;
  out.code = Code::ok;
  out.value = ProbeIn{};
  return out;
}
Result<std::string> encode_in(rt::InvocationContext, const ProbeIn&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}
Result<ProbeOut> decode_out(rt::InvocationContext, std::string_view, rt::WireObjects&) {
  Result<ProbeOut> out;
  out.code = Code::ok;
  out.value = ProbeOut{};
  return out;
}
Result<std::string> encode_out(rt::InvocationContext, const ProbeOut&, rt::WireObjects&) {
  Result<std::string> out;
  out.code = Code::ok;
  out.value = "{}";
  return out;
}

Result<rt::Done> validate(rt::InvocationContext, Ref<ProbeIn>) {
  Result<rt::Done> out;
  out.code = Code::ok;
  out.value = rt::Done{};
  return out;
}
Result<rt::PlanBounds> bounds(rt::InvocationContext, Ref<ProbeIn>) {
  Result<rt::PlanBounds> out;
  out.code = Code::ok;
  out.value = rt::PlanBounds{1, 64, 1, false};
  return out;
}
Result<rt::InvocationPlan> plan(rt::InvocationContext, Ref<ProbeIn>, rt::PlanOptions options) {
  Result<rt::InvocationPlan> out;
  out.code = Code::ok;
  out.value = vrt::make_plan(0, {}, {}, 0, 0);
  out.value->options = options;
  return out;
}

rt::SnapshotPayload exploding_payload() {
  ++g_exploding_payloads;
  rt::SnapshotPayload payload;
  payload.coordinate_space = "detector";
  return payload;
}

rt::Task<ProbeOut> run(rt::InvocationContext ctx, Ref<ProbeIn> input,
                       Ref<rt::InvocationPlan>) {
  g_run_calls.fetch_add(1);
  Result<ProbeOut> out;
  out.code = Code::ok;
  ProbeOut value;
  value.value = input->value * 3;
  value.span_a = rt::allocate_span_id(ctx);
  value.span_b = rt::allocate_span_id(ctx);

  // The unselected probe path must not construct the payload expression.
  const auto tags = rt::ProbeTags{};
  ACSD_DEV_SNAPSHOT(ctx, "validation.d01.snapshot", tags, exploding_payload());
  rt::PerfRecord record;
  record.probe_id = "validation.d01.span";
  record.kind = rt::PerfKind::span;
  record.start_steady_ns = rt::steady_ns();
  record.wall_ns = 1;
  record.count = 1;
  record.bytes = 8;
  record.span_id = value.span_a;
  ACSD_DEV_PERF(ctx, record);
  rt::try_record(ctx, record);
  const acsd::Unit unit = vrt::dimensionless_unit();
  acsd::BlockRequest range{0, 0};
  const acsd::U64 shape[1] = {0};
  const acsd::ExactInterval interval{acsd::Dyadic{"0", 0}, acsd::Dyadic{"1", 0}, 64};
  rt::SnapshotField fields[2];
  fields[0].name = "x";
  fields[0].unit = "ADU";
  fields[0].value = 3.0;
  fields[1].name = "unknown";
  fields[1].unit = "unknown";
  fields[1].value = std::monostate{};
  rt::SnapshotPayload payload;
  payload.views = std::span<const rt::SnapshotView>{};
  payload.fields = std::span<const rt::SnapshotField>(fields, 2);
  payload.coordinate_space = "detector";
  payload.index_hash = "sha256:" + std::string(64, '0');
  rt::try_snapshot(ctx, "validation.d01.snapshot", tags, payload);
  (void)interval;
  (void)range;
  (void)shape;
  (void)unit;
  out.value = value;
  return rt::ready<ProbeOut>(ctx, std::move(out));
}

rt::Task<ProbeOut> finalize(rt::InvocationContext ctx, Ref<ProbeIn>,
                            Ref<rt::InvocationPlan>, Result<ProbeOut> partial) {
  return rt::ready<ProbeOut>(ctx, std::move(partial));
}

Result<rt::Done> register_process(rt::RegistryBuilder& builder) {
  auto in = builder.register_type<ProbeIn>(
      rt::TypeCodec<ProbeIn>{{"acsd.validation.probe.in", 1}, "{\"type\":\"object\"}",
                             &decode_in, &encode_in});
  if (in.code != Code::ok) return in;
  auto out_type = builder.register_type<ProbeOut>(
      rt::TypeCodec<ProbeOut>{{"acsd.validation.probe.out", 1}, "{\"type\":\"object\"}",
                              &decode_out, &encode_out});
  if (out_type.code != Code::ok) return out_type;
  rt::ProcessDescriptor descriptor;
  descriptor.process_id = "validation.d01.process";
  descriptor.module = "validation.runtime";
  descriptor.document = "validation/runtime/README.md";
  auto process = builder.begin(std::move(descriptor));
  if (process.code != Code::ok || !process.value) {
    Result<rt::Done> out;
    out.code = process.code;
    return out;
  }
  rt::Operation<ProbeIn, ProbeOut> operation{
      "run", {"acsd.validation.probe.in", 1}, {"acsd.validation.probe.out", 1},
      &validate, &bounds, &plan, &run, &finalize};
  return builder.add<ProbeIn, ProbeOut>(*process.value, std::move(operation));
}

}  // namespace

int main() {
  vrt::Recorder recorder("d01_on", "validation/runtime/d01_on/d01_on.cpp");

  auto services = vrt::start_services(64ull << 20, 1000, 1, 1, 1,
                                      vrt::scratch_root("d01_on"));
  if (services.code != Code::ok || !services.value) {
    recorder.check("d01_on.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  rt::RegistryBuilder builder;
  recorder.expect_code("d01_on.register_process_ok", "ok", register_process(builder));
  auto frozen = builder.freeze();
  recorder.expect_code("d01_on.freeze_ok", "ok", frozen);
  if (frozen.code != Code::ok) return recorder.finish();
  auto host = rt::make_host(*services.value, *frozen.value);
  recorder.expect_code("d01_on.make_host_ok", "ok", host);
  if (host.code != Code::ok) return recorder.finish();

  auto command = rt::open_command(*services.value, "validation.d01.command",
                                  vrt::limits(64ull << 20, 1000, 0, 1, 1));
  recorder.expect_code("d01_on.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;

  // Unselected probe: probe_selected must be false and try_* must be a no-op.
  const auto tags = rt::ProbeTags{};
  const bool selected = rt::probe_selected(ctx, "validation.d01.unselected", tags);
  recorder.check("d01_on.unselected_probe_false", !selected, "false",
                 selected ? "true" : "false");
  rt::try_record(ctx, rt::PerfRecord{});
  rt::try_snapshot(ctx, "validation.d01.unselected", tags, rt::SnapshotPayload{});
  recorder.check("d01_on.try_entries_noexcept", true, "no exception", "returned normally",
                 "try_record/try_snapshot are noexcept and must not throw");

  const auto before_payloads = g_exploding_payloads.load();
  auto first = std::make_shared<ProbeIn>();
  first->value = 14;
  auto baseline = rt::wait_external(
      (*host.value)->invoke<ProbeIn, ProbeOut>(ctx, "validation.d01.process", "run", first));
  recorder.check("d01_on.baseline_invoke_ok", baseline.code == Code::ok, "ok",
                 vrt::code_of(baseline));
  const std::int64_t baseline_value =
      baseline.code == Code::ok && baseline.value ? baseline.value->value : -1;
  const acsd::U64 baseline_work = baseline.usage.work;
  recorder.check("d01_on.unselected_payload_not_constructed",
                 g_exploding_payloads.load() == before_payloads,
                 "0 extra payload constructions",
                 std::to_string(g_exploding_payloads.load() - before_payloads),
                 "the macro checks probe_selected before constructing the payload");

  auto second = std::make_shared<ProbeIn>();
  second->value = 14;
  auto repeat = rt::wait_external(
      (*host.value)->invoke<ProbeIn, ProbeOut>(ctx, "validation.d01.process", "run", second));
  recorder.check("d01_on.repeat_invoke_ok", repeat.code == Code::ok, "ok",
                 vrt::code_of(repeat));
  recorder.check("d01_on.science_code_unchanged", repeat.code == baseline.code,
                 "same Code", vrt::code_name(repeat.code));
  recorder.check("d01_on.science_value_unchanged",
                 repeat.value && repeat.value->value == baseline_value,
                 std::to_string(baseline_value),
                 repeat.value ? std::to_string(repeat.value->value) : "no value",
                 "probes must not change the scientific result");
  recorder.check("d01_on.science_usage_unchanged", repeat.usage.work == baseline_work,
                 std::to_string(baseline_work), std::to_string(repeat.usage.work));
  if (baseline.value) {
    recorder.value("d01_on.span_a", static_cast<long long>(baseline.value->span_a));
    recorder.value("d01_on.span_b", static_cast<long long>(baseline.value->span_b));
    recorder.value("d01_on.dev_mode_consistent",
                   baseline.value->span_a != 0 && baseline.value->span_b != 0);
  }
  recorder.check("d01_on.span_ids_allocated",
                 baseline.value && baseline.value->span_a != 0 &&
                     baseline.value->span_b != 0 && baseline.value->span_a != baseline.value->span_b,
                 "two distinct non-zero span ids",
                 baseline.value ? std::to_string(baseline.value->span_a) + "/" +
                                      std::to_string(baseline.value->span_b)
                                : "no value");
  recorder.check("d01_on.run_calls", g_run_calls.load() == 2, "2",
                 std::to_string(g_run_calls.load()));

  recorder.note("selected sink / D quota / diagnostic disk failure have no public "
                "configuration entry in the frozen headers; not covered and not faked.");

  (*host.value)->close_async();
  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("d01_on.close_command_ok", "ok", closed);
  return recorder.finish();
}
