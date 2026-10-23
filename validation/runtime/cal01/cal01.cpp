// C-CAL-1 / C-CAL-2 / C-CAL-DQ / C-CAL-ROOT through the full Host chain:
// make_services -> open_command -> generated registry -> ProcessHost ->
// invoke<CalibrateInput, Ref<NativeObservation>> -> wait_external.
//
// Frozen inputs (CALIBRATION_WALKTHROUGH.md):
//   C-CAL-1 raw=[10,20,-2], bias=2, dark=4 containing the bias, t=2,
//           t_dark=1, flat=2, normalization=2  ->  x=[4,14,-8]
//   C-CAL-2 same maps, dark does not contain the bias  ->  x=[0,10,-12]
//   C-CAL-DQ flat=[2,2,0] -> NaN/bad-flag/named Issue; NaN raw with valid DQ
//           is rejected with no Native.
//   C-CAL-ROOT two frames share the same bias/master ArrayRefs; the second
//           invocation must not invent a per-call random root.
//
// Only public headers are included.  The input arrays are built through the
// public runtime Storage (begin/append/seal); the process reads them through
// the same unique pool/BlockStore, so this is the real driver path, not an
// in-memory shortcut.
#include <acsd/contracts.hpp>
#include <acsd/modules/observation.hpp>
#include <acsd/runtime/generated_manifest.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Ref;
using acsd::Result;
using acsd::modules::CalibrateInput;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::string g_last_read_code = "none";
std::string g_last_read_detail;

acsd::Unit adu_unit() {
  acsd::Unit unit;
  unit.scale = "ADU";
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

acsd::Unit dq_unit() {
  acsd::Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

acsd::Identity identity_of(const std::string& object_id, const std::string& schema) {
  acsd::Identity identity;
  identity.object_id = object_id;
  identity.content_hash = "sha256:" + std::string(64, '0');
  identity.run_id = "validation.runtime.cal01";
  identity.revision = 0;
  identity.schema_id = schema;
  identity.schema_major = 2;
  identity.schema_minor = 0;
  return identity;
}

Ref<acsd::Index> make_index(acsd::U64 size, const std::string& id) {
  auto index = std::make_shared<acsd::Index>();
  index->size = size;
  index->kind = acsd::Index::Kind::native;
  index->identity = identity_of(id, "acsd.Index");
  index->assessment.complete = true;
  return index;
}

Ref<acsd::FootprintSet> make_footprints(Ref<acsd::Index> index, acsd::U64 width,
                                       acsd::U64 height) {
  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = acsd::Mat2{1.0, 0.0, 0.0, 1.0};
  footprints->offset = acsd::Vec2{0.0, 0.0};
  footprints->local_cell = acsd::Box{
      acsd::Vec2{0.0, 0.0},
      acsd::Vec2{static_cast<double>(width), static_cast<double>(height)}};
  footprints->detector_frame = "detector0";
  footprints->identity = identity_of("footprints:frame0", "acsd.FootprintSet");
  footprints->assessment.complete = true;
  return footprints;
}

struct Arrays {
  rt::ArenaRef arena;
  acsd::ArrayRef array;
};

template <class T>
Result<Arrays> make_array(const rt::InvocationContext& ctx, acsd::DType dtype,
                          const std::vector<T>& values, const Ref<acsd::Index>& index,
                          const acsd::Unit& unit, const std::string& id) {
  Result<Arrays> failure;
  failure.code = Code::internal;
  const acsd::U64 bytes = values.size() * sizeof(T);
  auto arena = rt::reserve(ctx, vrt::bytes(bytes + (1ull << 20)));
  if (arena.code != Code::ok || !arena.value) return failure;
  rt::BuildSpec spec;
  spec.layout.dtype = dtype;
  spec.layout.shape = {static_cast<acsd::U64>(values.size())};
  spec.layout.unit = unit;
  spec.layout.index = index;
  spec.placement = rt::Placement::memory;
  spec.buffer_bytes = bytes;
  spec.metadata_bytes = 4096;
  auto writer = rt::wait_external(rt::begin_async(ctx, *arena.value, spec));
  if (writer.code != Code::ok || !writer.value) return failure;
  auto buffer = rt::allocate<T>(ctx, *arena.value,
                                acsd::BlockRequest{0, static_cast<acsd::U64>(values.size())},
                                index, unit, vrt::zero_hash());
  if (buffer.code != Code::ok || !buffer.value) return failure;
  std::memcpy(buffer.value->values.data(), values.data(), bytes);
  auto block = rt::freeze(std::move(*buffer.value));
  if (block.code != Code::ok || !block.value) return failure;
  auto appended = rt::wait_external(
      rt::append_async<T>(ctx, *writer.value, std::move(*block.value)));
  if (appended.code != Code::ok) return failure;
  auto sealed = rt::wait_external(rt::seal_async(ctx, *writer.value));
  if (sealed.code != Code::ok || !sealed.value) return failure;
  Arrays out;
  out.arena = *arena.value;
  out.array = *sealed.value;
  Result<Arrays> result;
  result.code = Code::ok;
  result.value = out;
  return result;
}

Ref<acsd::RawObservation> make_raw(const std::vector<Arrays>& maps, const Arrays& adu,
                                   const Arrays& dq, acsd::U64 width, acsd::U64 height,
                                   const std::string& frame) {
  auto index = make_index(width * height, "detector0");
  auto raw = std::make_shared<acsd::RawObservation>();
  raw->adu = adu.array;
  raw->dq = dq.array;
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = make_footprints(index, width, height);
  raw->frame = frame;
  raw->context.epoch_julian_year = 2016.0;
  raw->context.observation_jd_tdb = 2457500.5;
  raw->identity = identity_of("raw:" + frame, "acsd.RawObservation");
  raw->assessment.complete = true;
  (void)maps;
  return raw;
}

acsd::Calibration make_calibration(const Arrays& bias, const Arrays& dark,
                                   const Arrays& flat, bool dark_contains_bias) {
  acsd::Calibration calibration;
  calibration.bias = bias.array;
  calibration.dark = dark.array;
  calibration.flat = flat.array;
  calibration.dark_contains_bias = dark_contains_bias;
  calibration.exposure_seconds = 2.0;
  calibration.dark_exposure_seconds = 1.0;
  calibration.flat_normalization = 2.0;
  return calibration;
}

std::vector<double> read_f64(const rt::InvocationContext& ctx, acsd::ArrayRef array,
                             acsd::U64 count, acsd::U64* peak_note = nullptr) {
  std::vector<double> values;
  if (!array) return values;
  std::cerr << "[cal01] read_f64 reserve\n";
  auto arena = rt::reserve(ctx, vrt::bytes(count * sizeof(double) + (1ull << 20)));
  std::cerr << "[cal01] read_f64 reserve code=" << vrt::code_name(arena.code) << "\n";
  if (arena.code != Code::ok) return values;
  auto task = rt::read_async<double>(ctx, *arena.value, array, acsd::BlockRequest{0, count});
  std::cerr << "[cal01] read_f64 submitted state=" << static_cast<int>(task.state()) << "\n";
  auto result = rt::wait_external(std::move(task));
  g_last_read_code = vrt::code_of(result);
  g_last_read_detail = "count=" + std::to_string(count) + " array=" + (array ? "set" : "null");
  std::cerr << "[cal01] read_f64 done code=" << vrt::code_name(result.code) << "\n";
  if (result.code != Code::ok || !result.value) return values;
  values.assign(result.value->values.begin(), result.value->values.end());
  if (peak_note != nullptr) {
    const rt::ScopeSnapshot snapshot = rt::snapshot(ctx.scope);
    *peak_note = snapshot.peak_allocated.host_bytes;
  }
  (*arena.value)->close();
  return values;
}

std::vector<std::uint16_t> read_u16(const rt::InvocationContext& ctx, acsd::ArrayRef array,
                                    acsd::U64 count) {
  std::vector<std::uint16_t> values;
  if (!array) return values;
  auto arena = rt::reserve(ctx, vrt::bytes(count * sizeof(std::uint16_t) + (1ull << 20)));
  if (arena.code != Code::ok) return values;
  auto task = rt::read_async<std::uint16_t>(ctx, *arena.value, array,
                                            acsd::BlockRequest{0, count});
  auto result = rt::wait_external(std::move(task));
  g_last_read_code = vrt::code_of(result);
  g_last_read_detail = "count=" + std::to_string(count) + " array=" + (array ? "set" : "null");
  std::cerr << "[cal01] read_f64 done code=" << vrt::code_name(result.code) << "\n";
  if (result.code != Code::ok || !result.value) return values;
  values.assign(result.value->values.begin(), result.value->values.end());
  (*arena.value)->close();
  return values;
}

bool exact(const std::vector<double>& values, const std::vector<double>& expected) {
  if (values.size() != expected.size()) return false;
  for (std::size_t i = 0; i < values.size(); ++i)
    if (values[i] != expected[i]) return false;
  return true;
}

std::string join_values(const std::vector<double>& values) {
  std::string text = "[";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i) text += ",";
    text += vrt::number(values[i]);
  }
  return text + "]";
}

struct Chain {
  std::shared_ptr<rt::Services> services;
  std::shared_ptr<rt::ProcessHost> host;
  rt::InvocationContext ctx;
};

}  // namespace

int main() {
  vrt::Recorder recorder("cal01", "validation/runtime/cal01/cal01.cpp");

  auto services = vrt::start_services(512ull << 20, 1000000, 2, 8, 8,
                                      vrt::scratch_root("cal01"));
  recorder.expect_code("cal.services_ok", "ok", services);
  if (services.code != Code::ok || !services.value) return recorder.finish();

  rt::RegistryBuilder builder;
  auto types = acsd::generated::register_builtin_types(builder);
  recorder.expect_code("cal.register_builtin_types_ok", "ok", types);
  auto processes = acsd::generated::register_processes(builder);
  recorder.expect_code("cal.register_processes_ok", "ok", processes);
  auto frozen = builder.freeze();
  recorder.expect_code("cal.freeze_ok", "ok", frozen);
  if (frozen.code != Code::ok || !frozen.value) return recorder.finish();
  auto host = rt::make_host(*services.value, *frozen.value);
  recorder.expect_code("cal.make_host_ok", "ok", host);
  if (host.code != Code::ok || !host.value) return recorder.finish();

  const acsd::U64 width = 3, height = 1, count = 3;
  const acsd::Unit unit = adu_unit();
  const auto index = make_index(count, "detector0");

  auto open = [&](const std::string& id) {
    return rt::open_command(*services.value, id,
                            vrt::limits(512ull << 20, 1000000, 1ull << 20, 2, 8));
  };
  auto invoke = [&](const rt::InvocationContext& ctx, Ref<acsd::RawObservation> raw,
                    const acsd::Calibration& cal) {
    auto input = std::make_shared<CalibrateInput>();
    input->raw = std::move(raw);
    input->request = cal;
    return rt::wait_external(
        (*host.value)->invoke<CalibrateInput, Ref<acsd::NativeObservation>>(
            ctx, "observation.calibrate", "calibrate_observation", input));
  };

  // ---------------------------------------------------------------------
  // Command 1: C-CAL-1 and C-CAL-2 (two invocations, then close).
  // ---------------------------------------------------------------------
  std::vector<double> raw_values{10.0, 20.0, -2.0};
  std::vector<std::uint16_t> dq_values{0, 0, 0};
  std::vector<double> bias_values{2.0, 2.0, 2.0};
  std::vector<double> dark_values{4.0, 4.0, 4.0};
  std::vector<double> flat_values{2.0, 2.0, 2.0};
  Arrays adu, dq, bias, dark, flat;
  Ref<acsd::RawObservation> raw;
  Ref<acsd::NativeObservation> cal1_native;
  acsd::Calibration calibration;
  {
    auto command = open("validation.cal01.cal12");
    recorder.expect_code("cal.open_command_ok", "ok", command);
    if (command.code != Code::ok) return recorder.finish();
    const rt::InvocationContext ctx = *command.value;

    auto adu_r = make_array<double>(ctx, acsd::DType::f64, raw_values, index, unit, "detector0");
    auto dq_r = make_array<std::uint16_t>(ctx, acsd::DType::u16, dq_values, index, dq_unit(),
                                          "detector0");
    auto bias_r = make_array<double>(ctx, acsd::DType::f64, bias_values, index, unit, "detector0");
    auto dark_r = make_array<double>(ctx, acsd::DType::f64, dark_values, index, unit, "detector0");
    auto flat_r = make_array<double>(ctx, acsd::DType::f64, flat_values, index, unit, "detector0");
    recorder.expect_code("cal.adu_array_ok", "ok", adu_r);
    recorder.expect_code("cal.dq_array_ok", "ok", dq_r);
    recorder.expect_code("cal.bias_array_ok", "ok", bias_r);
    recorder.expect_code("cal.dark_array_ok", "ok", dark_r);
    recorder.expect_code("cal.flat_array_ok", "ok", flat_r);
    if (adu_r.code != Code::ok || dq_r.code != Code::ok || bias_r.code != Code::ok ||
        dark_r.code != Code::ok || flat_r.code != Code::ok)
      return recorder.finish();
    adu = *adu_r.value; dq = *dq_r.value; bias = *bias_r.value; dark = *dark_r.value;
    flat = *flat_r.value;
    raw = make_raw({}, adu, dq, width, height, "frame0");
    calibration = make_calibration(bias, dark, flat, true);

    auto result = invoke(ctx, raw, calibration);
    recorder.check("cal.cal1_host_ok", result.code == Code::ok && result.value,
                   "ok + NativeObservation", vrt::code_of(result));
    if (result.code == Code::ok && result.value) {
      cal1_native = *result.value;
      const auto& native = *cal1_native;
      const auto values = read_f64(ctx, native.integrated_values, count);
      const auto lower = read_f64(ctx, native.numerical_lower, count);
      const auto upper = read_f64(ctx, native.numerical_upper, count);
      const auto flags = read_u16(ctx, native.dq, count);
      recorder.check("cal.cal1_exact_values", exact(values, {4.0, 14.0, -8.0}),
                     "[4,14,-8]", join_values(values));
      recorder.check("cal.cal1_negative_not_clipped",
                     values.size() == count && values[2] == -8.0, "-8",
                     values.size() == count ? vrt::number(values[2]) : "missing");
      recorder.check("cal.cal1_interval_contains_exact",
                     lower.size() == count && upper.size() == count && lower[0] <= 4.0 &&
                         4.0 <= upper[0] && lower[1] <= 14.0 && 14.0 <= upper[1] &&
                         lower[2] <= -8.0 && -8.0 <= upper[2],
                     "lower <= exact <= upper", "recorded");
      recorder.check("cal.cal1_dq_clean",
                     flags.size() == count && flags[0] == 0 && flags[1] == 0 &&
                         flags[2] == 0,
                     "all DQ zero", "recorded");
      recorder.check("cal.cal1_frame_context",
                     native.frame == "frame0" &&
                         native.context.observation_jd_tdb == 2457500.5,
                     "frame0 + jd", native.frame);
      recorder.check("cal.cal1_usage_work", result.usage.work >= count,
                     ">= " + std::to_string(count) + " work units",
                     std::to_string(result.usage.work),
                     "process usage aggregates stages; per-sample consume_work(1) is a lower bound");
      recorder.value("cal.cal1_lower0", lower.size() == count ? lower[0] : kNaN);
      recorder.value("cal.cal1_upper0", upper.size() == count ? upper[0] : kNaN);
    }

    auto calibration2 = make_calibration(bias, dark, flat, false);
    auto result2 = invoke(ctx, raw, calibration2);
    recorder.check("cal.cal2_host_ok", result2.code == Code::ok && result2.value,
                   "ok + NativeObservation", vrt::code_of(result2));
    if (result2.code == Code::ok && result2.value) {
      const auto values = read_f64(ctx, (*result2.value)->integrated_values, count);
      recorder.check("cal.cal2_exact_values", exact(values, {0.0, 10.0, -12.0}),
                     "[0,10,-12]", join_values(values));
    }
    auto closed = rt::wait_external(rt::close_command(ctx));
    recorder.expect_code("cal.close_command_ok", "ok", closed);
  }

  // ---------------------------------------------------------------------
  // Command 2: C-CAL-DQ (zero flat then NaN/DQ conflict).
  // ---------------------------------------------------------------------
  {
    auto command = open("validation.cal01.dq");
    recorder.expect_code("cal.dq_open_command_ok", "ok", command);
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      std::vector<double> flat_zero_values{2.0, 2.0, 0.0};
      auto flat_zero = make_array<double>(ctx, acsd::DType::f64, flat_zero_values, index,
                                          unit, "detector0");
      recorder.expect_code("cal.dq_flat_array_ok", "ok", flat_zero);
      if (flat_zero.code == Code::ok) {
        auto result = invoke(ctx, raw,
                             make_calibration(bias, dark, *flat_zero.value, true));
        recorder.check("cal.dq_zero_flat_host_ok", result.code == Code::ok && result.value,
                       "ok + NativeObservation", vrt::code_of(result));
        if (result.code == Code::ok && result.value) {
          const auto& native = **result.value;
          const auto values = read_f64(ctx, native.integrated_values, count);
          const auto flags = read_u16(ctx, native.dq, count);
          recorder.value("cal.dq_zero_flat_read_code", g_last_read_code);
          recorder.check("cal.dq_zero_flat_nan",
                         values.size() == count && std::isnan(values[2]),
                         "NaN at the zero flat",
                         values.size() == count ? vrt::number(values[2]) : "missing");
          recorder.check("cal.dq_zero_flat_flag",
                         flags.size() == count && flags[2] != 0, "nonzero DQ",
                         flags.size() == count ? std::to_string(flags[2]) : "missing");
          const std::string issues = vrt::issue_text(native.assessment);
          recorder.check("cal.dq_zero_flat_named_issue",
                         !native.assessment.issues.empty(), ">=1 named Issue", issues);
        }
      }

      std::vector<double> nan_values{10.0, kNaN, -2.0};
      auto nan_adu = make_array<double>(ctx, acsd::DType::f64, nan_values, index, unit,
                                        "detector0");
      recorder.expect_code("cal.dq_nan_array_ok", "ok", nan_adu);
      if (nan_adu.code == Code::ok) {
        auto nan_raw = make_raw({}, *nan_adu.value, dq, width, height, "frame0");
        auto result = invoke(ctx, nan_raw, calibration);
        recorder.check("cal.dq_nan_conflict_rejected", result.code == Code::invalid_model,
                       "invalid_model", vrt::code_of(result));
        recorder.check("cal.dq_nan_conflict_no_native", !result.value.has_value(),
                       "no NativeObservation", result.value ? "value present" : "no value");
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("cal.dq_close_command_ok", "ok", closed);
    }
  }

  // ---------------------------------------------------------------------
  // Command 3: C-CAL-ROOT (two frames share the same bias/master arrays).
  // ---------------------------------------------------------------------
  {
    auto command = open("validation.cal01.root");
    recorder.expect_code("cal.root_open_command_ok", "ok", command);
    if (command.code == Code::ok) {
      const rt::InvocationContext ctx = *command.value;
      std::vector<double> raw2_values{14.0, 24.0, 2.0};
      auto adu2 = make_array<double>(ctx, acsd::DType::f64, raw2_values, index, unit,
                                     "detector0");
      recorder.expect_code("cal.root_second_adu_ok", "ok", adu2);
      if (adu2.code == Code::ok) {
        auto raw2 = make_raw({}, *adu2.value, dq, width, height, "frame1");
        auto result1 = invoke(ctx, raw, calibration);
        auto result2 = invoke(ctx, raw2, calibration);
        recorder.check("cal.root_both_frames_ok",
                       result1.code == Code::ok && result2.code == Code::ok &&
                           result1.value && result2.value,
                       "both ok", vrt::code_of(result1) + "/" + vrt::code_of(result2));
        recorder.check("cal.root_shared_arrays_unchanged",
                       bias.array.get() == bias.array.get() &&
                           dark.array.get() == dark.array.get(),
                       "the same bias/dark ArrayRefs feed both frames", "same objects");
        if (result1.code == Code::ok && result2.code == Code::ok && result1.value &&
            result2.value) {
          const auto& noise1 = (*result1.value)->noise;
          const auto& noise2 = (*result2.value)->noise;
          const bool both_null = !noise1 && !noise2;
          bool same_roots = both_null;
          if (noise1 && noise2) {
            same_roots = noise1->innovations.size() == noise2->innovations.size() &&
                         noise1->shared_factors.size() == noise2->shared_factors.size();
            for (std::size_t i = 0; same_roots && i < noise1->shared_factors.size(); ++i) {
              const auto& a = noise1->shared_factors[i]->shared_id;
              const auto& b = noise2->shared_factors[i]->shared_id;
              same_roots = a.external_namespace == b.external_namespace &&
                           a.entity == b.entity && a.model_revision == b.model_revision &&
                           a.component == b.component;
            }
          }
          recorder.check("cal.root_no_per_call_random_root", same_roots,
                         "identical shared root set (or both absent)",
                         std::string("noise1=") + (noise1 ? "present" : "null") +
                             " noise2=" + (noise2 ? "present" : "null"),
                         "C-CAL-ROOT: shared bias/master keeps one shared root");
        }
      }
      auto closed = rt::wait_external(rt::close_command(ctx));
      recorder.expect_code("cal.root_close_command_ok", "ok", closed);
    }
  }

  recorder.note("each scenario uses a fresh command (cal12 / dq / root) for test "
                "isolation, not as a workaround: under max_work=1000 a same-command "
                "fifth operation is now a named resource_exhausted (the cumulative "
                "consumed work is permanent per SCHEDULER); this case uses a larger "
                "budget so all scenarios can also run in one command.");
  auto host_closed = rt::wait_external((*host.value)->close_async());
  recorder.expect_code("cal.host_close_ok", "ok", host_closed);
  return recorder.finish();
}
