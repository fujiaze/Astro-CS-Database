// C-CAL Host chain case: driver -> registry -> pool/Storage -> Native.
//
// It uses only the public runtime/science headers (plus the test harness).  The
// input arrays are produced through the public Storage writers, so the same
// registration path a real driver uses is exercised; the typed invocation goes
// through acsd::generated::register_processes, a frozen registry and
// ProcessHost.  Evidence is the raw JSON document consumed by
// validation/runtime/run_cases.py.
#include <acsd/contracts.hpp>
#include <acsd/modules/observation.hpp>
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/runtime/process.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "harness.hpp"

#include <atomic>
#include <cstdlib>
#include <new>

// Test-only heap metering for the unaccounted std::vector materialization of
// the calibrate path.  The runtime arena uses aligned operator new, which this
// plain operator new/delete pair deliberately does not intercept; therefore the
// counter measures exactly the unmanaged std::vector/heap allocations, not the
// ledger-metered arena buffers.
namespace {
constexpr std::size_t kHeapHeader = alignof(std::max_align_t);
std::atomic<std::size_t> g_heap_live{0};
std::atomic<std::size_t> g_heap_peak{0};

void heap_add(std::size_t bytes) {
  const std::size_t live = g_heap_live.fetch_add(bytes, std::memory_order_relaxed) + bytes;
  std::size_t peak = g_heap_peak.load(std::memory_order_relaxed);
  while (live > peak &&
         !g_heap_peak.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {
  }
}
void heap_remove(std::size_t bytes) {
  g_heap_live.fetch_sub(bytes, std::memory_order_relaxed);
}
std::size_t heap_live() { return g_heap_live.load(std::memory_order_relaxed); }
std::size_t heap_peak() { return g_heap_peak.load(std::memory_order_relaxed); }
void heap_reset_peak() { g_heap_peak.store(heap_live(), std::memory_order_relaxed); }
}  // namespace

void* operator new(std::size_t size) {
  void* raw = std::malloc(size + kHeapHeader);
  if (!raw) throw std::bad_alloc();
  *static_cast<std::size_t*>(raw) = size;
  heap_add(size);
  return static_cast<char*>(raw) + kHeapHeader;
}
void operator delete(void* pointer) noexcept {
  if (!pointer) return;
  char* raw = static_cast<char*>(pointer) - kHeapHeader;
  const std::size_t size = *reinterpret_cast<std::size_t*>(raw);
  heap_remove(size);
  std::free(raw);
}
void operator delete(void* pointer, std::size_t) noexcept { operator delete(pointer); }
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* pointer) noexcept { operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { operator delete(pointer); }

namespace {

namespace rt = acsd::rt;
using acsd::ArrayRef;
using acsd::Calibration;
using acsd::Code;
using acsd::DType;
using acsd::FootprintSet;
using acsd::Index;
using acsd::NativeObservation;
using acsd::NoiseModel;
using acsd::NoiseRoot;
using acsd::RawObservation;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using acsd::Unit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

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
Result<T> fail_value(Code code, std::string reason) {
  Result<T> out;
  out.code = code;
  out.assessment.complete = false;
  out.assessment.validity = acsd::Validity::invalid;
  out.assessment.execution = acsd::ExecutionState::failed;
  acsd::Issue issue;
  issue.code = code;
  issue.origin = "cal_host";
  issue.reason = std::move(reason);
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

// A test-only LinearAction used as a shared master factor: it is a pure fixture
// with its own identity, it never drives a production algorithm.
class FixtureIdentityAction final : public acsd::LinearAction {
 public:
  explicit FixtureIdentityAction(Ref<Index> index) { spaces.domain = spaces.codomain = index;
    spaces.input = adu_unit();
    spaces.output = adu_unit();
    identity.object_id = "acsd.LinearAction:fixture_identity";
    identity.content_hash = "sha256:" + std::string(64, 'c');
    identity.schema_id = "acsd.LinearAction";
    identity.schema_major = 2;
    identity.schema_minor = 0;
    assessment.complete = true;
  }
  Result<ArrayRef> apply(acsd::Context&, ArrayRef values) const override {
    return ok_value(values);
  }
  Result<ArrayRef> transpose_apply(acsd::Context&, ArrayRef values) const override {
    return ok_value(values);
  }
  Result<std::vector<acsd::Id>> dependency_blocks(acsd::BlockRequest,
                                                  bool) const override {
    return ok_value(std::vector<acsd::Id>{});
  }
  Result<acsd::ActionRecipe> serialize_recipe() const override {
    acsd::ActionRecipe recipe;
    recipe.kind = acsd::ActionRecipe::Kind::identity;
    recipe.algorithm = "fixture_identity_v1";
    recipe.version = 1;
    recipe.spaces = spaces;
    return ok_value(recipe);
  }
  Result<std::vector<acsd::ExactInterval>> row_enclosure(acsd::Context&,
                                                         U64) const override {
    return ok_value(std::vector<acsd::ExactInterval>{});
  }
};

struct Driver {
  rt::InvocationContext context;
  rt::ArenaRef arena;
  Ref<Index> index;
  Ref<FootprintSet> footprints;

  ArrayRef make_array(DType dtype, const std::vector<double>& values, const Unit& unit) {
    const U64 count = static_cast<U64>(values.size());
    rt::BuildSpec spec;
    spec.layout.dtype = DType::f64;
    spec.layout.shape = {count};
    spec.layout.unit = unit;
    spec.layout.index = index;
    spec.placement = rt::Placement::memory;
    spec.buffer_bytes = count * sizeof(double);
    spec.metadata_bytes = 512;
    Result<rt::WriterRef> writer = rt::wait_external(
        rt::begin_async(context, arena, spec));
    if (writer.code != Code::ok || !writer.value) return nullptr;
    Result<rt::Buffer<double>> buffer =
        rt::allocate<double>(context, arena, acsd::BlockRequest{0, count}, index, unit,
                             "sha256:" + std::string(64, 'd'));
    if (buffer.code != Code::ok || !buffer.value) return nullptr;
    for (U64 i = 0; i < count; ++i) buffer.value->values[i] = values[i];
    Result<rt::Block<double>> block = rt::freeze(std::move(*buffer.value));
    if (block.code != Code::ok || !block.value) return nullptr;
    Result<U64> appended = rt::wait_external(
        rt::append_async(context, *writer.value, std::move(*block.value)));
    if (appended.code != Code::ok) return nullptr;
    Result<ArrayRef> sealed = rt::wait_external(rt::seal_async(context, *writer.value));
    if (sealed.code != Code::ok) return nullptr;
    return sealed.value.value_or(nullptr);
  }

  ArrayRef make_flags_with_unit(const std::vector<std::uint16_t>& values, const Unit& unit) {
    const U64 count = static_cast<U64>(values.size());
    rt::BuildSpec spec;
    spec.layout.dtype = DType::u16;
    spec.layout.shape = {count};
    spec.layout.unit = unit;
    spec.layout.index = index;
    spec.placement = rt::Placement::memory;
    spec.buffer_bytes = count * sizeof(std::uint16_t);
    spec.metadata_bytes = 256;
    Result<rt::WriterRef> writer = rt::wait_external(
        rt::begin_async(context, arena, spec));
    if (writer.code != Code::ok || !writer.value) return nullptr;
    Result<rt::Buffer<std::uint16_t>> buffer = rt::allocate<std::uint16_t>(
        context, arena, acsd::BlockRequest{0, count}, index, unit,
        "sha256:" + std::string(64, 'e'));
    if (buffer.code != Code::ok || !buffer.value) return nullptr;
    for (U64 i = 0; i < count; ++i) buffer.value->values[i] = values[i];
    Result<rt::Block<std::uint16_t>> block = rt::freeze(std::move(*buffer.value));
    if (block.code != Code::ok || !block.value) return nullptr;
    Result<U64> appended = rt::wait_external(
        rt::append_async(context, *writer.value, std::move(*block.value)));
    if (appended.code != Code::ok) return nullptr;
    Result<ArrayRef> sealed = rt::wait_external(rt::seal_async(context, *writer.value));
    if (sealed.code != Code::ok) return nullptr;
    return sealed.value.value_or(nullptr);
  }

  ArrayRef make_flags(const std::vector<std::uint16_t>& values) {
    return make_flags_with_unit(values, dq_unit());
  }

  Ref<RawObservation> make_raw(const std::vector<double>& raw,
                               const std::vector<std::uint16_t>& dq, U64 width, U64 height) {
    auto observation = std::make_shared<RawObservation>();
    observation->adu = make_array(DType::f64, raw, adu_unit());
    observation->dq = make_flags(dq);
    observation->width = width;
    observation->height = height;
    observation->source_index = index;
    observation->footprints = footprints;
    observation->frame = "frame0";
    observation->context.epoch_julian_year = 2016.0;
    observation->context.observation_jd_tdb = 2457500.5;
    observation->identity.object_id = "acsd.RawObservation:frame0";
    observation->identity.content_hash = "sha256:" + std::string(64, 'f');
    observation->identity.schema_id = "acsd.RawObservation";
    observation->identity.schema_major = 2;
    observation->identity.schema_minor = 0;
    observation->assessment.complete = true;
    return observation;
  }

  ArrayRef make_map(const std::vector<double>& values) {
    const U64 count = static_cast<U64>(values.size());
    return make_array(DType::f64, values, adu_unit());
  }
};

Result<std::vector<double>> read_f64(rt::InvocationContext context, rt::ArenaRef arena,
                                     ArrayRef array) {
  if (!array) {
    return fail_value<std::vector<double>>(Code::invalid_request, "no array");
  }
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<rt::Block<double>> block =
      rt::wait_external(rt::read_async<double>(context, arena, array, acsd::BlockRequest{0, count}));
  if (block.code != Code::ok || !block.value) {
    return fail_value<std::vector<double>>(block.code, "the array cannot be read");
  }
  return ok_value(std::vector<double>(block.value->values.begin(),
                                              block.value->values.end()));
}

Result<std::vector<std::uint16_t>> read_u16(rt::InvocationContext context, rt::ArenaRef arena,
                                            ArrayRef array) {
  if (!array) {
    return fail_value<std::vector<std::uint16_t>>(Code::invalid_request, "no array");
  }
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<rt::Block<std::uint16_t>> block = rt::wait_external(
      rt::read_async<std::uint16_t>(context, arena, array, acsd::BlockRequest{0, count}));
  if (block.code != Code::ok || !block.value) {
    return fail_value<std::vector<std::uint16_t>>(block.code, "the array cannot be read");
  }
  return ok_value(std::vector<std::uint16_t>(block.value->values.begin(),
                                                     block.value->values.end()));
}

}  // namespace

int main() {
  vrt::Recorder recorder("cal_host", "validation/runtime/calibration/cal_host.cpp");

  // Force the strong module registration into the link: the generated assembly
  // references register_calibration weakly so an unlinked observation module
  // keeps its M0 shell.
  // A volatile function-pointer store is a real relocation: the strong module
  // registration is linked even though the generated assembly only references
  // it weakly.
  volatile auto forced_registration = &acsd::modules::register_calibration;
  recorder.value("host.registration_linked", forced_registration != nullptr);

  rt::PoolConfig config;
  config.process = vrt::limits(512ull << 20, 100000000, 1ull << 20);
  config.cpu_threads = 2;
  config.read_slots = 2;
  config.write_slots = 2;
  config.scratch_root = vrt::scratch_root("cal_host");
  Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
  recorder.check("cal.host.services", services.code == Code::ok && services.value, "ok",
                 vrt::code_name(services.code), "one Services instance");
  if (!services.value) return recorder.finish();

  Result<rt::InvocationContext> command =
      rt::open_command(*services.value, "validation.cal.host", vrt::limits(512ull << 20,
                                                                          100000000, 1ull << 20));
  recorder.check("cal.host.command", command.code == Code::ok && command.value.has_value(),
                 "ok", vrt::code_name(command.code), "the driver command scope");
  if (!command.value) return recorder.finish();
  rt::InvocationContext context = *command.value;

  Result<rt::ArenaRef> reserved = rt::reserve(context, vrt::bytes(96ull << 20));
  recorder.check("cal.host.arena", reserved.code == Code::ok && reserved.value, "ok",
                 vrt::code_name(reserved.code), "the driver input-array arena");
  if (!reserved.value) return recorder.finish();
  Driver driver;
  driver.context = context;
  driver.arena = *reserved.value;
  driver.index = make_index(3);
  driver.footprints = make_footprints(driver.index, 3, 1);

  // C-CAL-1 input.
  const std::vector<double> ccal1_raw{10.0, 20.0, -2.0};
  const std::vector<std::uint16_t> ccal1_dq{0, 0, 0};
  Ref<RawObservation> raw1 = driver.make_raw(ccal1_raw, ccal1_dq, 3, 1);
  Calibration request1;
  request1.bias = driver.make_map({2.0, 2.0, 2.0});
  request1.dark = driver.make_map({4.0, 4.0, 4.0});
  request1.flat = driver.make_map({2.0, 2.0, 2.0});
  request1.dark_contains_bias = true;
  request1.exposure_seconds = 2.0;
  request1.dark_exposure_seconds = 1.0;
  request1.flat_normalization = 2.0;
  recorder.check("cal.host.input_arrays",
                 raw1 && raw1->adu && raw1->dq && request1.bias && request1.dark &&
                     request1.flat,
                 "all input arrays are registered", "some array is null",
                 "Storage writers produced the input blocks");

  // Registry assembly from the generated entry points.
  rt::RegistryBuilder builder;
  Result<rt::Done> types = acsd::generated::register_builtin_types(builder);
  Result<rt::Done> processes = acsd::generated::register_processes(builder);
  Result<std::shared_ptr<const rt::Registry>> frozen = builder.freeze();
  recorder.check("cal.host.builtin_types", types.code == Code::ok, "ok",
                 vrt::code_name(types.code), "generated register_builtin_types");
  recorder.check("cal.host.register_processes", processes.code == Code::ok, "ok",
                 vrt::code_name(processes.code), "generated register_processes calls the module");
  recorder.check("cal.host.freeze", frozen.code == Code::ok && frozen.value, "ok",
                 vrt::code_name(frozen.code), "the frozen registry");
  if (!frozen.value) return recorder.finish();
  recorder.check("cal.host.registered",
                 (*frozen.value)->has("observation.calibrate", "calibrate_observation"),
                 "observation.calibrate/calibrate_observation present", "not found",
                 "the module registration replaced the M0 shell");
  Result<std::shared_ptr<rt::ProcessHost>> host = rt::make_host(*services.value, *frozen.value);
  recorder.check("cal.host.make_host", host.code == Code::ok && host.value, "ok",
                 vrt::code_name(host.code), "the ProcessHost");
  if (!host.value) return recorder.finish();

  auto make_input = [](Ref<RawObservation> raw, const Calibration& request) {
    auto input = std::make_shared<acsd::modules::CalibrateInput>();
    input->raw = std::move(raw);
    input->request = request;
    return input;
  };

  // Plan numbers observed through the typed hooks.
  {
    auto input = make_input(raw1, request1);
    Result<rt::PlanBounds> bounds = acsd::modules::bounds_calibration(context, input);
    if (bounds.code == Code::ok && bounds.value) {
      rt::PlanOptions options;
      options.block_elements = bounds.value->max_block_elements;
      options.window = 1;
      options.placement = rt::Placement::memory;
      Result<rt::InvocationPlan> plan =
          acsd::modules::plan_calibration(context, input, options);
      if (plan.code == Code::ok && plan.value) {
        recorder.value("plan.R", plan.value->persistent.host_bytes);
        recorder.value("plan.F", plan.value->progress_floor.host_bytes);
        recorder.check("cal.host.plan_numbers",
                       plan.value->persistent.host_bytes == 33170 &&
                           plan.value->progress_floor.host_bytes == 2166988,
                       "R=33170, F=2166988",
                       "R=" + std::to_string(plan.value->persistent.host_bytes) + ", F=" +
                           std::to_string(plan.value->progress_floor.host_bytes),
                       "the frozen plan formula of the walkthrough");
      } else {
        recorder.check("cal.host.plan_numbers", false, "ok", vrt::code_name(plan.code),
                       "plan");
      }
    } else {
      recorder.check("cal.host.plan_numbers", false, "ok", vrt::code_name(bounds.code),
                     "bounds");
    }
  }

  auto invoke = [&](Ref<acsd::modules::CalibrateInput> input) {
    rt::Task<Ref<NativeObservation>> task =
        (*host.value)->invoke<acsd::modules::CalibrateInput, Ref<NativeObservation>>(
            context, "observation.calibrate", "calibrate_observation", input);
    return rt::wait_external(std::move(task));
  };

  // C-CAL-1 through the Host chain.
  Result<Ref<NativeObservation>> first = invoke(make_input(raw1, request1));
  std::string first_issues;
  for (const auto& issue : first.assessment.issues) {
    if (!first_issues.empty()) first_issues += " | ";
    first_issues += issue.origin + ":" + issue.reason;
  }
  recorder.value("host.c_cal_1.issues", first_issues);
  recorder.check("cal.host.c_cal_1", first.code == Code::ok && first.value, "ok",
                 vrt::code_name(first.code), first_issues.empty() ? "the typed invocation returns a NativeObservation" : first_issues);
  Ref<NativeObservation> native1;
  if (first.value) {
    native1 = *first.value;
    recorder.check("cal.host.c_cal_1_metadata",
                   native1->frame == "frame0" &&
                       native1->context.epoch_julian_year == 2016.0 &&
                       native1->native_index &&
                       native1->native_index->identity.object_id ==
                           "acsd.Index:detector0" &&
                       native1->physical_collection && native1->noise != nullptr &&
                       native1->error != nullptr && native1->regrouping != nullptr,
                   "frame/context/index/physical/NoiseModel/ErrorRecord/regrouping preserved",
                   "some field is missing", "the shared finish assembly through the bridge");
    Result<std::vector<double>> values = read_f64(context, driver.arena, native1->integrated_values);
    Result<std::vector<double>> lower = read_f64(context, driver.arena, native1->numerical_lower);
    Result<std::vector<double>> upper = read_f64(context, driver.arena, native1->numerical_upper);
    Result<std::vector<std::uint16_t>> flags = read_u16(context, driver.arena, native1->dq);
    bool exact = values.code == Code::ok && values.value && values.value->size() == 3 &&
                 (*values.value)[0] == 4.0 && (*values.value)[1] == 14.0 &&
                 (*values.value)[2] == -8.0;
    bool enclosed = lower.value && upper.value && (*lower.value)[0] <= 4.0 &&
                    (*upper.value)[0] >= 4.0 && (*lower.value)[1] <= 14.0 &&
                    (*upper.value)[1] >= 14.0 && (*lower.value)[2] <= -8.0 &&
                    (*upper.value)[2] >= -8.0;
    bool dq_ok = flags.value && flags.value->size() == 3 && (*flags.value)[0] == 0;
    if (values.value) {
      recorder.value("host.c_cal_1.x0", (*values.value)[0]);
      recorder.value("host.c_cal_1.x1", (*values.value)[1]);
      recorder.value("host.c_cal_1.x2", (*values.value)[2]);
    }
    const std::string expected_identity =
        "acsd.NativeObservation:5ebea39ffb9039d44e3cc4f82142f010ed40edff318e5fb50162aaa898554106";
    const bool same_identity =
        native1 && native1->identity.object_id == expected_identity;
    recorder.value("host.c_cal_1.identity",
                   native1 ? native1->identity.object_id : "");
    recorder.check("cal.host.c_cal_1_identity", same_identity,
                   "the identity the synchronous calibrate_observation computes",
                   same_identity ? "same" : "different",
                   "same values and DQ => same NativeObservation content identity");
    recorder.check("cal.host.c_cal_1_values", exact, "[4,14,-8]",
                   exact ? "[4,14,-8]" : "different values",
                   "the Host path returns the same kernel output");
    recorder.check("cal.host.c_cal_1_enclosure", enclosed, "lower <= exact <= upper",
                   enclosed ? "contained" : "not contained",
                   "the outward enclosure survives the block pipeline");
    recorder.check("cal.host.c_cal_1_dq", dq_ok, "0", dq_ok ? "0" : "different flags",
                   "DQ is preserved through append/seal");
  }

  // C-CAL-2 through the Host chain: the dark does not contain the bias.
  {
    auto input = make_input(raw1, request1);
    input->request.dark_contains_bias = false;
    Result<Ref<NativeObservation>> second = invoke(input);
    Ref<NativeObservation> native2 = second.value ? *second.value : nullptr;
    Result<std::vector<double>> values =
        native2 ? read_f64(context, driver.arena, native2->integrated_values)
                : fail_value<std::vector<double>>(Code::internal, "no native");
    const bool exact = second.code == Code::ok && values.value && values.value->size() == 3 &&
                       (*values.value)[0] == 0.0 && (*values.value)[1] == 10.0 &&
                       (*values.value)[2] == -12.0;
    if (values.value) {
      recorder.value("host.c_cal_2.x0", (*values.value)[0]);
      recorder.value("host.c_cal_2.x1", (*values.value)[1]);
      recorder.value("host.c_cal_2.x2", (*values.value)[2]);
    }
    recorder.check("cal.host.c_cal_2", exact, "[0,10,-12]",
                   exact ? "[0,10,-12]" : "different values",
                   "dark without bias is scaled directly through the same kernel");
  }

  // C-CAL-DQ through the Host chain: flat<=0 yields no value and a named issue.
  {
    std::vector<double> raw_values{10.0, 20.0, 30.0};
    std::vector<std::uint16_t> dq_values{0, 0, 0};
    Ref<RawObservation> raw_dq = driver.make_raw(raw_values, dq_values, 3, 1);
    Calibration request;
    request.flat = driver.make_map({2.0, 0.0, 2.0});
    request.flat_normalization = 1.0;
    request.exposure_seconds = 1.0;
    Result<Ref<NativeObservation>> result = invoke(make_input(raw_dq, request));
    bool behaved = false;
    if (result.code == Code::ok && result.value) {
      Ref<NativeObservation> native = *result.value;
      Result<std::vector<double>> values =
          read_f64(context, driver.arena, native->integrated_values);
      Result<std::vector<std::uint16_t>> flags = read_u16(context, driver.arena, native->dq);
      bool issue_named = false;
      for (const auto& issue : native->assessment.issues) {
        if (issue.code == Code::invalid_model && issue.reason.find("flat") != std::string::npos)
          issue_named = true;
      }
      behaved = values.value && values.value->size() == 3 && std::isnan((*values.value)[1]) &&
                !std::isnan((*values.value)[0]) && flags.value &&
                ((*flags.value)[1] & 1u) != 0u && issue_named;
      recorder.value("host.c_cal_dq.value1", std::isnan((*values.value)[1]) ? "nan" : "finite");
    }
    recorder.check("cal.host.c_cal_dq", behaved,
                   "NaN value + bad flag + named flat Issue", behaved ? "behaved" : "different",
                   "a non-positive flat is never repaired through the pipeline");
  }

  // C-CAL-ROOT: two frames share the same bias array and the same master root.
  {
    auto factor = std::make_shared<FixtureIdentityAction>(driver.index);
    auto shared_root = std::make_shared<NoiseRoot>();
    shared_root->shared_id = acsd::SharedId{"acsd.native", "master_flat", "1", "amplitude"};
    shared_root->coordinates = driver.index;
    shared_root->unit = adu_unit();
    shared_root->factor = factor;
    shared_root->identity = factor->identity;
    auto master_noise = std::make_shared<NoiseModel>();
    master_noise->state = acsd::NoiseState::known;
    master_noise->shared_factors.push_back(shared_root);
    master_noise->influence.push_back(factor);
    master_noise->variance_unit = acsd::Unit{"ADU", 0, 0, 0, 1.0, 2};
    Calibration with_master = request1;
    with_master.master_noise = master_noise;
    Result<Ref<NativeObservation>> a = invoke(make_input(raw1, with_master));
    Result<Ref<NativeObservation>> b = invoke(make_input(raw1, with_master));
    bool shared = false;
    if (a.value && b.value && (*a.value)->noise && (*b.value)->noise &&
        (*a.value)->noise->shared_factors.size() == 1 &&
        (*b.value)->noise->shared_factors.size() == 1) {
      const auto& root_a = (*a.value)->noise->shared_factors.front();
      const auto& root_b = (*b.value)->noise->shared_factors.front();
      shared = root_a->shared_id.external_namespace == root_b->shared_id.external_namespace &&
               root_a->shared_id.entity == root_b->shared_id.entity &&
               root_a->shared_id.model_revision == root_b->shared_id.model_revision &&
               root_a->shared_id.component == root_b->shared_id.component &&
               (*a.value)->noise->influence.size() == 1 &&
               (*b.value)->noise->influence.size() == 1;
      recorder.value("host.root.shared_id", root_a->shared_id.entity);
      recorder.value("host.root.influences", static_cast<U64>((*a.value)->noise->influence.size()));
    }
    recorder.check("cal.host.c_cal_root", shared,
                   "both invocations keep the same shared Identity and one influence",
                   shared ? "same root" : "different/missing root",
                   "two process invocations do not create new random roots");
  }

  // DEV_MODE OFF: two invocations of the same input must produce the same
  // scientific identity; no probe value may change the result.
  {
    Result<Ref<NativeObservation>> a = invoke(make_input(raw1, request1));
    Result<Ref<NativeObservation>> b = invoke(make_input(raw1, request1));
    const bool same = a.value && b.value &&
                      (*a.value)->identity.object_id == (*b.value)->identity.object_id &&
                      (*a.value)->identity.content_hash == (*b.value)->identity.content_hash;
    recorder.check("cal.host.dev_off_deterministic", same,
                   "same NativeObservation identity", same ? "same" : "different",
                   "ACSD_DEV_MODE=0 evaluated no probe and changed no science result");
    recorder.value("host.dev_off_identity", a.value ? (*a.value)->identity.object_id : "");
  }

  // C-CAL-H: a frame crossing the 1 MiB canonical array boundary with three
  // full 65536-element blocks and a short tail block.  The nominal value of an
  // unmapped calibration is x = raw / 1, so exact FP64 equality is a valid
  // oracle and the block boundaries must not alter it.  This block also records
  // the declared work upper bound vs the measured work, and the unaccounted
  // heap peak vs the plan's materialization term.
  {
    const U64 side_width = 400;
    const U64 side_height = 500;  // N = 200000
    std::vector<double> raw_values(static_cast<std::size_t>(side_width * side_height));
    std::vector<std::uint16_t> dq_values(raw_values.size(), 0);
    for (std::size_t i = 0; i < raw_values.size(); ++i) {
      raw_values[i] = 1000.0 + 0.25 * static_cast<double>(i);
    }
    const Ref<Index> saved_index = driver.index;
    const Ref<FootprintSet> saved_footprints = driver.footprints;
    driver.index = make_index(side_width * side_height);
    driver.footprints = make_footprints(driver.index, side_width, side_height);
    Ref<RawObservation> raw_large =
        driver.make_raw(raw_values, dq_values, side_width, side_height);
    driver.index = saved_index;
    driver.footprints = saved_footprints;
    Calibration plain;
    plain.exposure_seconds = 1.0;
    plain.flat_normalization = 1.0;

    // Plan first: the work/materialization bounds are part of the evidence.
    U64 declared_work = 0;
    U64 declared_materialization = 0;
    Result<rt::InvocationPlan> plan;
    Result<rt::PlanBounds> bounds =
        acsd::modules::bounds_calibration(context, make_input(raw_large, plain));
    if (bounds.code == Code::ok && bounds.value) {
      rt::PlanOptions options;
      options.block_elements = bounds.value->max_block_elements;
      options.window = 1;
      options.placement = rt::Placement::memory;
      plan = acsd::modules::plan_calibration(context, make_input(raw_large, plain), options);
      if (plan.code == Code::ok && plan.value) {
        const rt::InvocationPlan& value = *plan.value;
        U64 per_cohort = 0;
        for (const rt::StageSpec& stage : value.regions[0].stages) {
          per_cohort += stage.work_upper;
        }
        declared_work = per_cohort * value.cohorts;
        declared_materialization = 92 * value.elements;
        recorder.value("host.c_cal_h.plan_R", value.persistent.host_bytes);
        recorder.value("host.c_cal_h.plan_F", value.progress_floor.host_bytes);
        recorder.value("host.c_cal_h.declared_work", declared_work);
        recorder.value("host.c_cal_h.declared_materialization", declared_materialization);
        recorder.check("cal.host.c_cal_h_plan",
                       value.cohorts == 4 && value.block_elements == 65536 &&
                           value.persistent.host_bytes == 26832768 &&
                           value.progress_floor.host_bytes == 6623232,
                       "4 cohorts, n=65536, R=26832768, F=6623232",
                       "c=" + std::to_string(value.cohorts) +
                           ", n=" + std::to_string(value.block_elements),
                       "the serial plan with S(n), work bounds and no pipe edges");
      }
    }

    const U64 work_before = rt::snapshot(context.scope).work;
    heap_reset_peak();
    const std::size_t live_before = heap_live();
    Result<Ref<NativeObservation>> result = invoke(make_input(raw_large, plain));
    const U64 measured_work = rt::snapshot(context.scope).work - work_before;
    const std::size_t unaccounted_peak = heap_peak() - live_before;
    recorder.value("host.c_cal_h.measured_work", measured_work);
    recorder.value("host.c_cal_h.unaccounted_heap_peak", static_cast<U64>(unaccounted_peak));
    recorder.check("cal.host.c_cal_h_work_covered",
                   declared_work != 0 && measured_work <= declared_work,
                   "measured work <= declared stage work totals",
                   "measured=" + std::to_string(measured_work) +
                       " declared=" + std::to_string(declared_work),
                   "the plan work_upper covers read/write/assembly work (P1-8b)");
    recorder.check("cal.host.c_cal_h_materialization_covered",
                   declared_materialization != 0 &&
                       unaccounted_peak <= declared_materialization + (8ull << 20),
                   "unmanaged heap peak within materialization bound + 8 MiB overhead",
                   "peak=" + std::to_string(unaccounted_peak) +
                       " declared=" + std::to_string(declared_materialization),
                   "allocator measurement supports the unaccounted term; not a peak proof");

    std::string large_issues;
    for (const auto& issue : result.assessment.issues) {
      if (!large_issues.empty()) large_issues += " | ";
      large_issues += issue.origin + ":" + issue.reason;
    }
    recorder.value("host.c_cal_h.code", vrt::code_name(result.code));
    recorder.value("host.c_cal_h.issues", large_issues);
    bool exact = false;
    U64 count = 0;
    if (result.code == Code::ok && result.value) {
      Ref<NativeObservation> native = *result.value;
      Result<std::vector<double>> values =
          read_f64(context, driver.arena, native->integrated_values);
      count = values.value ? static_cast<U64>(values.value->size()) : 0;
      const std::size_t probes[5] = {0, 65535, 65536, 131072,
                                     raw_values.size() - 1};
      exact = count == raw_values.size();
      for (std::size_t index : probes) {
        if (values.value && (*values.value)[index] != raw_values[index]) exact = false;
      }
      if (values.value) {
        recorder.value("host.c_cal_h.last", (*values.value).back());
        recorder.value("host.c_cal_h.block_boundary",
                       (*values.value)[65536]);
      }
    }
    recorder.check("cal.host.c_cal_h", exact && count == raw_values.size(),
                   "N=200000 values exact across 65536/65536/65536/3392 blocks",
                   "N=" + std::to_string(count), "tail block and block boundaries");
  }

  // C-CAL-ORDER: the plan is serial and honestly declares no pipe edge.  The
  // Host admission uses window=1 by contract; a window/edge the composition
  // does not actually use must be rejected rather than advertised (P1-8c).
  {
    auto input = make_input(raw1, request1);
    rt::PlanOptions options;
    options.block_elements = 1;
    options.window = 2;
    options.placement = rt::Placement::memory;
    Result<rt::InvocationPlan> rejected =
        acsd::modules::plan_calibration(context, input, options);
    recorder.value("host.order.window2_code", vrt::code_name(rejected.code));
    recorder.check("cal.host.c_cal_order_window_not_declared",
                   rejected.code == Code::invalid_request, "invalid_request",
                   vrt::code_name(rejected.code),
                   "a window without pipe credit is rejected");
    options.window = 1;
    Result<rt::InvocationPlan> serial =
        acsd::modules::plan_calibration(context, input, options);
    bool declared_serial = false;
    if (serial.code == Code::ok && serial.value) {
      declared_serial = serial.value->regions.size() == 1 &&
                        serial.value->regions[0].edges.empty() &&
                        serial.value->regions[0].cohorts == 3 &&
                        serial.value->options.window == 1;
      recorder.value("host.order.serial_edges",
                     static_cast<U64>(serial.value->regions[0].edges.size()));
    }
    recorder.check("cal.host.c_cal_order_serial_declared", declared_serial,
                   "one serial region, 3 cohorts, no pipe edge",
                   declared_serial ? "serial" : "different plan",
                   "the plan does not advertise unused pipes");
  }
  recorder.note("C-CAL-ORDER: serial plan (window=1, no edge); real pipe credit is future work");

  recorder.note("C-CAL-CLEAN/C-CAL-WRITE need injected cancellation/short writes; they are "
                "covered by the runtime verification cases, not fabricated here");

  // P1-4: the synchronous entry point and the typed Host path must return the
  // same metadata verdict.  The typed Host validate hook is the shared
  // validate_calibration; cal_equivalence additionally compares the synchronous
  // entry point against it.  Each case below runs the real typed invocation and
  // asserts that its code equals the typed validate code (and that the run did
  // not happen for invalid metadata).
  {
    struct Case {
      const char* name;
      Ref<RawObservation> raw;
      Calibration request;
      Code expected;
    };
    std::vector<Case> cases;
    {
      auto quoted = std::make_shared<RawObservation>(*raw1);
      quoted->dq = driver.make_flags_with_unit(ccal1_dq, adu_unit());
      cases.push_back(Case{"dq_not_dimensionless", quoted, request1, Code::invalid_model});
    }
    {
      auto bad_exposure = request1;
      bad_exposure.exposure_seconds = 0.0;
      cases.push_back(
          Case{"exposure_zero", raw1, bad_exposure, Code::invalid_model});
    }
    {
      auto bad_flat = request1;
      bad_flat.flat = nullptr;
      bad_flat.flat_normalization = 0.0;
      cases.push_back(
          Case{"absent_flat_zero_normalization", raw1, bad_flat, Code::invalid_model});
    }
    {
      auto no_footprints = std::make_shared<RawObservation>(*raw1);
      no_footprints->footprints = nullptr;
      cases.push_back(
          Case{"missing_footprints", no_footprints, request1, Code::unavailable});
    }
    {
      auto bad_shape = std::make_shared<RawObservation>(*raw1);
      bad_shape->width = 4;  // N=3 index no longer covers the frame
      cases.push_back(Case{"shape_mismatch", bad_shape, request1, Code::invalid_model});
    }
    for (const Case& item : cases) {
      auto input = make_input(item.raw, item.request);
      Result<rt::Done> typed = acsd::modules::validate_calibration(context, input);
      Result<Ref<NativeObservation>> hosted = invoke(input);
      const bool same = typed.code == item.expected && hosted.code == typed.code;
      recorder.value(std::string("host.p1_4.") + item.name + ".typed",
                     vrt::code_name(typed.code));
      recorder.value(std::string("host.p1_4.") + item.name + ".host",
                     vrt::code_name(hosted.code));
      recorder.check(std::string("cal.host.p1_4_") + item.name, same,
                     std::string(vrt::code_name(item.expected)),
                     std::string(vrt::code_name(hosted.code)) + "/" +
                         vrt::code_name(typed.code),
                     "sync and typed metadata validation are one verdict");
    }
  }

  const rt::ScopeSnapshot snapshot = rt::snapshot(context.scope);
  recorder.resource("peak_host_bytes", snapshot.peak_allocated.host_bytes);
  recorder.resource("work", snapshot.work);
  recorder.resource("read_bytes", snapshot.read_bytes);
  recorder.resource("written_bytes", snapshot.written_bytes);

  Result<rt::Done> closed_command = rt::wait_external(rt::close_command(context));
  recorder.check("cal.host.close_command", closed_command.code == Code::ok, "ok",
                 vrt::code_name(closed_command.code), "the command scope drains");
  Result<rt::Done> closed_host = rt::wait_external((*host.value)->close_async());
  recorder.check("cal.host.close_host", closed_host.code == Code::ok, "ok",
                 vrt::code_name(closed_host.code), "the host closes after draining");

  recorder.resource("checks", static_cast<long long>(recorder.attempts()));
  recorder.resource("successes", static_cast<long long>(recorder.successes()));
  return recorder.finish();
}
