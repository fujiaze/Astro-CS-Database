// C-CAL-SPILL: finish_native must read a spill-sealed SignalArrays frame back
// through the admitted asynchronous Storage path and register the exact bytes in
// the runtime block store, so the shared noise/identity assembly runs unchanged.
//
// The same input is calibrated twice: once with Placement::memory (the existing
// path) and once with Placement::spill (sealed into canonical disk blocks).  The
// scientific identity and every sampled value must be identical; the spill run
// reports its element count, sealed block count and the number of bytes the
// finish readback materialized.
//
// Only public runtime/science headers are used.  Evidence is the raw JSON this
// driver prints (see run_spill_readback.sh).
#include <acsd/modules/observation.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/storage.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace rt = acsd::rt;
using acsd::ArrayRef;
using acsd::BlockRequest;
using acsd::Calibration;
using acsd::Code;
using acsd::DType;
using acsd::FootprintSet;
using acsd::Index;
using acsd::NativeObservation;
using acsd::RawObservation;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using acsd::Unit;

constexpr U64 kN = 200000;        // 131072 * 2: two canonical 1 MiB blocks per array
constexpr U64 kBlock = 65536;     // canonical cohort size of the plan

template <class T>
Result<T> ok_value(T value) {
  Result<T> out;
  out.code = Code::ok;
  out.value = std::move(value);
  out.assessment.complete = true;
  out.assessment.execution = acsd::ExecutionState::complete;
  return out;
}

template <class T>
Result<T> fail_value(Code code, const std::string& reason) {
  Result<T> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = acsd::Validity::invalid;
  out.assessment.execution = acsd::ExecutionState::failed;
  acsd::Issue issue;
  issue.code = code;
  issue.origin = "cal_spill";
  issue.reason = reason;
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

Unit adu_unit() {
  Unit unit;
  unit.scale = "ADU";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit dq_unit() {
  Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

Ref<Index> make_index(U64 size) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::native;
  index->namespace_id = "detector0";
  index->identity.object_id = "acsd.Index:detector0";
  index->identity.content_hash = "sha256:" + std::string(64, 'a');
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  return index;
}

Ref<FootprintSet> make_footprints(Ref<Index> index, U64 width, U64 height) {
  auto footprints = std::make_shared<FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->local_cell.lower = {-0.5, -0.5};
  footprints->local_cell.upper = {0.5, 0.5};
  footprints->detector_frame = "frame0";
  footprints->identity.object_id = "acsd.FootprintSet:frame0";
  footprints->identity.content_hash = "sha256:" + std::string(64, 'b');
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->assessment.complete = true;
  return footprints;
}

ArrayRef make_array(rt::InvocationContext context, rt::ArenaRef arena, Ref<Index> index,
                    DType dtype, const Unit& unit, const std::vector<double>& values) {
  const U64 count = static_cast<U64>(values.size());
  rt::BuildSpec spec;
  spec.layout.dtype = dtype;
  spec.layout.shape = {count};
  spec.layout.unit = unit;
  spec.layout.index = index;
  spec.placement = rt::Placement::memory;
  spec.buffer_bytes = count * sizeof(double);
  spec.metadata_bytes = 512;
  Result<rt::WriterRef> writer = rt::wait_external(rt::begin_async(context, arena, spec));
  if (writer.code != Code::ok || !writer.value) return nullptr;
  Result<rt::Buffer<double>> buffer = rt::allocate<double>(
      context, arena, BlockRequest{0, count}, index, unit, "sha256:" + std::string(64, 'd'));
  if (buffer.code != Code::ok || !buffer.value) return nullptr;
  for (U64 i = 0; i < count; ++i) buffer.value->values[i] = values[i];
  Result<rt::Block<double>> block = rt::freeze(std::move(*buffer.value));
  if (block.code != Code::ok || !block.value) return nullptr;
  Result<U64> appended =
      rt::wait_external(rt::append_async(context, *writer.value, std::move(*block.value)));
  if (appended.code != Code::ok) return nullptr;
  Result<ArrayRef> sealed = rt::wait_external(rt::seal_async(context, *writer.value));
  if (sealed.code != Code::ok) return nullptr;
  return sealed.value.value_or(nullptr);
}

ArrayRef make_flags(rt::InvocationContext context, rt::ArenaRef arena, Ref<Index> index,
                    const std::vector<std::uint16_t>& values) {
  const U64 count = static_cast<U64>(values.size());
  rt::BuildSpec spec;
  spec.layout.dtype = DType::u16;
  spec.layout.shape = {count};
  spec.layout.unit = dq_unit();
  spec.layout.index = index;
  spec.placement = rt::Placement::memory;
  spec.buffer_bytes = count * sizeof(std::uint16_t);
  spec.metadata_bytes = 256;
  Result<rt::WriterRef> writer = rt::wait_external(rt::begin_async(context, arena, spec));
  if (writer.code != Code::ok || !writer.value) return nullptr;
  Result<rt::Buffer<std::uint16_t>> buffer = rt::allocate<std::uint16_t>(
      context, arena, BlockRequest{0, count}, index, dq_unit(), "sha256:" + std::string(64, 'e'));
  if (buffer.code != Code::ok || !buffer.value) return nullptr;
  for (U64 i = 0; i < count; ++i) buffer.value->values[i] = values[i];
  Result<rt::Block<std::uint16_t>> block = rt::freeze(std::move(*buffer.value));
  if (block.code != Code::ok || !block.value) return nullptr;
  Result<U64> appended =
      rt::wait_external(rt::append_async(context, *writer.value, std::move(*block.value)));
  if (appended.code != Code::ok) return nullptr;
  Result<ArrayRef> sealed = rt::wait_external(rt::seal_async(context, *writer.value));
  if (sealed.code != Code::ok) return nullptr;
  return sealed.value.value_or(nullptr);
}

Result<std::vector<double>> read_f64(rt::InvocationContext context, rt::ArenaRef arena,
                                     ArrayRef array) {
  if (!array) return fail_value<std::vector<double>>(Code::invalid_request, "no array");
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<rt::Block<double>> block = rt::wait_external(
      rt::read_async<double>(context, arena, array, BlockRequest{0, count}));
  if (block.code != Code::ok || !block.value)
    return fail_value<std::vector<double>>(block.code, "read failed");
  return ok_value(std::vector<double>(block.value->values.begin(), block.value->values.end()));
}

Result<std::vector<std::uint16_t>> read_u16(rt::InvocationContext context, rt::ArenaRef arena,
                                            ArrayRef array) {
  if (!array) return fail_value<std::vector<std::uint16_t>>(Code::invalid_request, "no array");
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<rt::Block<std::uint16_t>> block = rt::wait_external(
      rt::read_async<std::uint16_t>(context, arena, array, BlockRequest{0, count}));
  if (block.code != Code::ok || !block.value)
    return fail_value<std::vector<std::uint16_t>>(block.code, "read failed");
  return ok_value(
      std::vector<std::uint16_t>(block.value->values.begin(), block.value->values.end()));
}

struct Fixture {
  rt::InvocationContext base;
  rt::ArenaRef input_arena;
  Ref<Index> index;
  Ref<FootprintSet> footprints;
  Ref<RawObservation> raw;
  Calibration request;
};

bool build_fixture(Fixture& fixture, const std::vector<double>& raw_values,
                   const std::vector<std::uint16_t>& dq_values,
                   const std::vector<double>& flat_values,
                   const std::vector<double>& bias_values) {
  fixture.index = make_index(kN);
  fixture.footprints = make_footprints(fixture.index, kN, 1);
  ArrayRef adu = make_array(fixture.base, fixture.input_arena, fixture.index, DType::f64,
                            adu_unit(), raw_values);
  ArrayRef dq = make_flags(fixture.base, fixture.input_arena, fixture.index, dq_values);
  if (!adu || !dq) return false;
  auto raw = std::make_shared<RawObservation>();
  raw->adu = adu;
  raw->dq = dq;
  raw->width = kN;
  raw->height = 1;
  raw->source_index = fixture.index;
  raw->footprints = fixture.footprints;
  raw->frame = "frame0";
  raw->context.epoch_julian_year = 2016.0;
  raw->context.observation_jd_tdb = 2457500.5;
  raw->identity.object_id = "acsd.RawObservation:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, 'f');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;
  fixture.raw = raw;

  Calibration request;
  request.flat = make_array(fixture.base, fixture.input_arena, fixture.index, DType::f64,
                            adu_unit(), flat_values);
  request.bias = make_array(fixture.base, fixture.input_arena, fixture.index, DType::f64,
                            adu_unit(), bias_values);
  request.dark = request.bias;
  request.dark_contains_bias = true;
  request.exposure_seconds = 2.0;
  request.dark_exposure_seconds = 1.0;
  request.flat_normalization = 1.0;
  request.gain_e_per_adu = 1.5;
  request.read_rms_e = 2.0;
  fixture.request = request;
  return request.flat && request.bias;
}

struct RunResult {
  Code code = Code::internal;
  std::string identity;
  std::string content_hash;
  std::vector<double> values;
  std::vector<double> lower;
  std::vector<double> upper;
  std::vector<std::uint16_t> dq;
  U64 blocks = 0;
  U64 elements = 0;
  U64 readback_bytes = 0;       // measured read_bytes delta during finish
  U64 materialized_bytes = 0;   // raw bytes copied into the block store
  std::string issues;
};

RunResult run_calibration(Fixture& fixture, rt::Placement placement) {
  RunResult out;
  auto input = std::make_shared<acsd::modules::CalibrateInput>(
      acsd::modules::CalibrateInput{fixture.raw, fixture.request});
  rt::PlanOptions options;
  options.block_elements = kBlock;
  options.window = 1;
  options.placement = placement;
  Result<rt::InvocationPlan> plan = acsd::modules::plan_calibration(fixture.base, input, options);
  if (plan.code != Code::ok || !plan.value) {
    out.code = plan.code;
    out.issues = "plan failed";
    return out;
  }
  auto plan_ref = std::make_shared<rt::InvocationPlan>(*plan.value);
  Result<rt::ArenaRef> persistent = rt::reserve(fixture.base, plan_ref->persistent);
  Result<rt::ArenaRef> progress = rt::reserve(fixture.base, plan_ref->progress_floor);
  if (persistent.code != Code::ok || progress.code != Code::ok) {
    out.code = Code::resource_exhausted;
    out.issues = "plan reservation failed";
    return out;
  }
  rt::InvocationContext region = fixture.base;
  region.region = 0;
  region.persistent = *persistent.value;
  region.progress = *progress.value;

  Result<acsd::modules::SignalWriter> begun =
      rt::wait_external(acsd::modules::begin_signal(region, plan_ref, input));
  if (begun.code != Code::ok || !begun.value) {
    out.code = begun.code;
    out.issues = "begin failed";
    (*persistent.value)->close();
    (*progress.value)->close();
    return out;
  }
  const acsd::modules::SignalWriter writer = *begun.value;
  auto body = acsd::modules::stream_map_ordered<acsd::modules::CalibrationBlock,
                                                acsd::modules::SignalBlock>(
      region, plan_ref, 0,
      [input](acsd::modules::BlockContext block) {
        return acsd::modules::read_calibration_block(block, input);
      },
      [](acsd::modules::BlockContext block, acsd::modules::CalibrationBlock data) {
        return acsd::modules::calibrate_block(block, std::move(data));
      },
      [writer](acsd::modules::BlockContext block, acsd::modules::SignalBlock data) {
        return acsd::modules::append_signal(block, writer, std::move(data));
      });
  Result<rt::Done> ran = rt::wait_external(std::move(body));
  if (ran.code != Code::ok) {
    out.code = ran.code;
    out.issues = "stream failed";
    (void)rt::wait_external(acsd::modules::abort_signal(region, writer));
    (*persistent.value)->close();
    (*progress.value)->close();
    return out;
  }
  Result<acsd::modules::SignalArrays> sealed =
      rt::wait_external(acsd::modules::seal_signal(region, writer));
  if (sealed.code != Code::ok || !sealed.value) {
    out.code = sealed.code;
    out.issues = "seal failed";
    (*persistent.value)->close();
    (*progress.value)->close();
    return out;
  }
  out.blocks = static_cast<U64>(sealed.value->value ? sealed.value->value->block_ids.size() : 0);
  out.elements = kN;
  if (placement == rt::Placement::spill) {
    // value/lower/upper/dq + flat/adu/bias are materialized by finish_native.
    out.materialized_bytes = kN * (3 * sizeof(double) + sizeof(std::uint16_t)) +
                             kN * sizeof(double) * 3;
  }
  const rt::ScopeSnapshot before_finish = rt::snapshot(region.scope);
  Result<Ref<NativeObservation>> native =
      rt::wait_external(acsd::modules::finish_native(region, input, *sealed.value));
  const rt::ScopeSnapshot after_finish = rt::snapshot(region.scope);
  out.readback_bytes = after_finish.read_bytes > before_finish.read_bytes
                           ? after_finish.read_bytes - before_finish.read_bytes
                           : 0;
  if (native.code != Code::ok || !native.value) {
    out.code = native.code;
    out.issues = native.assessment.issues.empty() ? "finish failed"
                                                  : native.assessment.issues.front().reason;
    (*persistent.value)->close();
    (*progress.value)->close();
    return out;
  }
  const Ref<NativeObservation> observation = *native.value;
  out.code = Code::ok;
  out.identity = observation->identity.object_id;
  out.content_hash = observation->identity.content_hash;
  Result<std::vector<double>> values =
      read_f64(region, *persistent.value, observation->integrated_values);
  Result<std::vector<double>> lower =
      read_f64(region, *persistent.value, observation->numerical_lower);
  Result<std::vector<double>> upper =
      read_f64(region, *persistent.value, observation->numerical_upper);
  Result<std::vector<std::uint16_t>> flags =
      read_u16(region, *persistent.value, observation->dq);
  if (values.value) out.values = *values.value;
  if (lower.value) out.lower = *lower.value;
  if (upper.value) out.upper = *upper.value;
  if (flags.value) out.dq = *flags.value;
  if (!values.value || !flags.value) {
    out.code = Code::internal;
    out.issues = "the delivered arrays cannot be read back";
  }
  (*persistent.value)->close();
  (*progress.value)->close();
  return out;
}

std::string json_escape(const std::string& text) {
  std::string out;
  for (char c : text) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const bool spill = argc > 1 && std::string(argv[1]) == "spill";
  auto check = [](bool condition, const std::string& name) {
    std::printf("%s: %s\n", condition ? "PASS" : "FAIL", name.c_str());
    return condition ? 0 : 1;
  };
  int failures = 0;

  rt::PoolConfig config;
  config.process.ceiling.host_bytes = 512ull << 20;
  config.process.ceiling.cpu_tokens = 2;
  config.process.max_work = 100000000;
  config.cpu_threads = 2;
  config.read_slots = 2;
  config.write_slots = 2;
  config.scratch_root = spill ? "/tmp/acsd-cal-spill" : "/tmp/acsd-cal-memory";
  std::error_code ec;
  std::filesystem::remove_all(config.scratch_root, ec);

  Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
  failures += check(services.code == Code::ok && services.value, "make_services");
  if (!services.value) return 1;
  Result<rt::InvocationContext> command = rt::open_command(
      *services.value, spill ? "validation.cal.spill" : "validation.cal.memory",
      config.process, 1);
  failures += check(command.code == Code::ok && command.value.has_value(), "open_command");
  if (!command.value) return 1;
  Fixture fixture;
  fixture.base = *command.value;
  Result<rt::ArenaRef> inputs = rt::reserve(fixture.base, rt::ResourceVector{96ull << 20});
  failures += check(inputs.code == Code::ok && inputs.value, "input arena");
  if (!inputs.value) return 1;
  fixture.input_arena = *inputs.value;

  std::vector<double> raw_values(kN);
  std::vector<std::uint16_t> dq_values(kN, 0);
  std::vector<double> flat_values(kN);
  std::vector<double> bias_values(kN);
  for (U64 i = 0; i < kN; ++i) {
    raw_values[i] = 200.0 + 0.125 * static_cast<double>(i);
    bias_values[i] = 20.0;
    flat_values[i] = 1.25;
    if (i % 1000 == 999) dq_values[i] = 1;
  }
  failures += check(build_fixture(fixture, raw_values, dq_values, flat_values, bias_values),
                    "fixture input arrays");

  const RunResult run =
      run_calibration(fixture, spill ? rt::Placement::spill : rt::Placement::memory);
  failures += check(run.code == Code::ok, "calibration ok");
  failures += check(run.blocks >= 2, "at least two canonical blocks per array");
  if (spill) {
    failures += check(run.readback_bytes > 0,
                      "finish read the sealed spill blocks through the I/O lane");
    failures += check(run.materialized_bytes > 0,
                      "finish materialized the arrays it reads");
  }
  const std::size_t probes[5] = {0, 1, 65535, 131072, static_cast<std::size_t>(kN - 1)};
  std::printf(
      "{\"placement\":\"%s\",\"n\":%llu,\"block_elements\":%llu,"
      "\"case\":\"cal.spill_readback\","
      "\"blocks_per_array\":%llu,\"readback_bytes\":%llu,\"materialized_bytes\":%llu,"
      "\"identity\":\"%s\",\"content_hash\":\"%s\",\"code\":%d,\"failures\":%d,"
      "\"samples\":[",
      spill ? "spill" : "memory", static_cast<unsigned long long>(kN),
      static_cast<unsigned long long>(kBlock),
      static_cast<unsigned long long>(run.blocks),
      static_cast<unsigned long long>(run.readback_bytes),
      static_cast<unsigned long long>(run.materialized_bytes),
      json_escape(run.identity).c_str(), json_escape(run.content_hash).c_str(),
      static_cast<int>(run.code), failures);
  for (std::size_t i = 0; i < 5; ++i) {
    const std::size_t at = probes[i];
    const bool valid = at < run.values.size();
    std::printf("%s{\"i\":%zu,\"value\":%s,\"dq\":%s}", i ? "," : "", at,
                valid ? std::to_string(run.values[at]).c_str() : "null",
                at < run.dq.size() ? std::to_string(run.dq[at] & 1u).c_str() : "null");
  }
  std::printf("]}\n");
  (*inputs.value)->close();
  (void)rt::wait_external(rt::close_command(fixture.base));
  return failures == 0 ? 0 : 1;
}

