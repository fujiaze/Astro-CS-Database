// validate_calibration / bounds_calibration / plan_calibration of the
// observation.calibrate process.
//
// validate reads metadata only: it never opens an Array block, so a cold disk
// input cannot turn the synchronous validation into payload I/O.  The actual
// NaN/DQ consistency and the per-value flat range are checked by the CPU
// stage after the block hash, in calibrate_block.
//
// The plan is the fixed resource model of CALIBRATION_WALKTHROUGH.md, with two
// deliberate corrections after review (P1-8):
//
//   * the region is serial: the current composition (stream_map_ordered) does
//     not use rt::make_pipe/permit/credit, so the plan declares NO pipe edges
//     and restricts PlanOptions.window to 1.  Declaring an edge we never use
//     would overstate the scheduler capability; a future pipe integration adds
//     the edges together with the real credit handling.
//   * S is a function of n (storage staging + encode buffer + fixed codec
//     state), not a 1 MiB constant; the allocator measurement of cal_host is
//     recorded as support, not as a proof of the peak.
//
//   n = block_elements, N = elements, cohorts = ceil(N/n)
//   one stream region (id 0) with serial stages 0 read, 1 calibrate, 2 write
//   input payload per cohort  raw+bias+dark+flat (4 x 8n) + raw DQ 2n = 34n
//   output payload per cohort value+lower+upper (3 x 8n) + DQ 2n = 26n
//   progress_floor F = 60n + S(n) + control
//   persistent R = final output (memory placement) + final NoiseModel arrays
//                   + unaccounted frame materialization + metadata/control
//
// Unaccounted std::vector materialization (NOT arena-metered, charged
// conservatively to R; this is an upper estimate, not a measured peak):
//   synchronous calibrate_observation:
//     raw 8N, DQ 2N, bias 8N, dark 8N, flat 8N, value/lower/upper 24N, DQ 2N,
//     inverse_flat 8N, assembly flat copy 8N, raw copy 8N, bias copy 8N
//     = 92N (kFrameMaterializationBytesPerElement)
//   runtime finish_native reads back value 8N, DQ 2N, flat 8N, raw 8N and bias
//   8N and the shared assembly copies flat/raw/bias/inverse_flat (another 32N),
//   = 66N, i.e. below the same 92N term.  The noise-model weight vectors and
//   the LinearAction diagonal copies are counted in noise_model_bytes().
#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "acsd/modules/observation.hpp"
#include "model/result.hpp"
#include "observation/calibration_kernel.hpp"
#include "observation/observation.hpp"

namespace acsd::modules {

using rt::Done;

namespace {

constexpr const char* kOrigin = "observation.calibrate";

// The storage working set of one cohort that is not one of the explicit input/
// output payloads:
//   * one canonical I/O block staging buffer and its decompression buffer
//     (the 1 MiB canonical array block of OBJECTS.md), read one array at a
//     time;
//   * the append encode vector (one block, 8n bytes worst case), which the
//     runtime allocates as an unmanaged std::vector;
//   * fixed hash/zstd/control state.
// The function is linear in n and is charged in F; the cal_host allocator
// measurement is recorded as support, not as a peak proof.
constexpr U64 kCanonicalIoBlockBytes = 1ull << 20;   // 1 MiB
constexpr U64 kStorageFixedWorkspace = 64ull << 10;  // 64 KiB
constexpr U64 kFrameMaterializationBytesPerElement = 92;

// One native sample = one work unit of the calibration stage.  Storage work is
// 1 + bytes/4096 (runtime.storage work_units), and begin/append/seal each
// consume their own share; see the helpers below.
constexpr U64 kBeginWorkPerWriter = 16;
constexpr U64 kSealWorkPerWriter = 16;

// Fixed control-node budget of one cohort plan and of the final assembly.
constexpr U64 kCohortControlBytes = 4ull << 10;    // 4 KiB
constexpr U64 kStageControlBytes = 1ull << 10;     // 1 KiB
constexpr U64 kPlanControlBytes = 16ull << 10;     // 16 KiB
constexpr U64 kFinalMetadataBytes = 16ull << 10;   // 16 KiB
constexpr U64 kCleanupBytes = 64ull << 10;         // 4 writer states + nodes
constexpr U64 kCleanupWorkUpper = 64;

constexpr U64 kDefaultBlockElements = 65536;
// The serial composition declares exactly one window: no pipe credit exists.
constexpr U32 kSerialWindow = 1;

U64 saturating_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b) return std::numeric_limits<U64>::max();
  return a * b;
}

U64 saturating_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b) return std::numeric_limits<U64>::max();
  return a + b;
}

U64 work_units(U64 bytes) { return 1 + bytes / 4096; }

U64 storage_workspace_upper(U64 n) {
  return saturating_add(
      saturating_mul(2, kCanonicalIoBlockBytes),
      saturating_add(saturating_mul(n, 8), kStorageFixedWorkspace));
}

U64 frame_materialization_upper(U64 elements) {
  return saturating_mul(elements, kFrameMaterializationBytesPerElement);
}

// The reservations of the read stage: up to five input arrays (raw f64, raw
// u16, bias, dark, flat), each read_async reserves 64 + work_units(8n) (the
// runtime uses the f64 byte count even for the u16 DQ array).
U64 read_work_upper(U64 n) {
  return saturating_mul(5, saturating_add(64, work_units(saturating_mul(n, 8))));
}

// The write stage: four begin, four append-encode and four seal segments.
U64 write_work_upper(U64 n) {
  return saturating_add(
      saturating_mul(4, kBeginWorkPerWriter),
      saturating_add(saturating_mul(4, work_units(saturating_mul(n, 8))),
                     saturating_mul(4, kSealWorkPerWriter)));
}

std::string hex_id(const std::string& text) {
  return "sha256:" + model::sha256_hex(text);
}

std::string number_text(double value) {
  std::ostringstream out;
  out.precision(17);
  out << value;
  return out.str();
}

std::string array_text(const ArrayRef& array) {
  if (!array) return "<absent>";
  std::string text = array->identity.content_hash + "|" + array->index_id + "|" +
                     array->unit.scale + "|" + std::to_string(array->unit.linear_power) + "|";
  for (U64 extent : array->shape) text += std::to_string(extent) + ",";
  return text;
}

Result<Done> require_input(const Ref<CalibrateInput>& input) {
  if (!input) {
    return model::fail<Done>(Code::invalid_request, "no calibration input supplied", kOrigin,
                             "the observation cannot be calibrated");
  }
  return observation::validate_calibration_input(input->raw, input->request);
}

U64 element_count(const Ref<RawObservation>& raw) {
  return raw->width * raw->height;
}

// Upper bound of the final NoiseModel payload: the diagonal weight arrays and
// the cached action payload of the branch that is actually requested, plus the
// master flat Jacobian.  Both the Storage array and the action copy are
// counted; the two are distinct allocations.
U64 noise_model_bytes(const Calibration& request, U64 elements) {
  U64 arrays = 0;  // 8 bytes per element per weight array
  U64 actions = 0;
  auto add = [&](U64 units) {
    arrays = saturating_add(arrays, saturating_mul(units, 8));
    actions = saturating_add(actions, saturating_mul(units, 8));
  };
  if (request.raw_noise) {
    add(elements);  // inverse flat
  } else {
    const bool has_gain = request.gain_e_per_adu.has_value() &&
                          std::isfinite(*request.gain_e_per_adu) && *request.gain_e_per_adu > 0.0;
    const bool has_read = request.read_rms_e.has_value() &&
                          std::isfinite(*request.read_rms_e) && *request.read_rms_e >= 0.0;
    if (has_gain && has_read) {
      add(elements);  // inverse flat
      add(elements);  // shot
      add(elements);  // read
    } else {
      add(elements);  // unknown placeholder
    }
  }
  if (request.master_noise) add(elements);  // flat Jacobian
  return saturating_add(arrays, actions);
}

}  // namespace

Result<Done> validate_calibration(rt::InvocationContext, Ref<CalibrateInput> input) {
  return require_input(input);
}

Result<rt::PlanBounds> bounds_calibration(rt::InvocationContext, Ref<CalibrateInput> input) {
  Result<Done> validation = require_input(input);
  if (!ok(validation)) {
    return model::fail<rt::PlanBounds>(validation.code, "the calibration input is invalid", kOrigin,
                                       "the calibration cannot be planned");
  }
  const U64 elements = element_count(input->raw);
  rt::PlanBounds bounds;
  bounds.min_block_elements = 1;
  bounds.max_block_elements = std::min(elements, kDefaultBlockElements);
  if (bounds.max_block_elements == 0) bounds.max_block_elements = 1;
  bounds.max_window = kSerialWindow;
  bounds.allow_spill = true;
  return model::ok_result(bounds);
}

Result<rt::InvocationPlan> plan_calibration(rt::InvocationContext, Ref<CalibrateInput> input,
                                            rt::PlanOptions options) {
  Result<Done> validation = require_input(input);
  if (!ok(validation)) {
    return model::fail<rt::InvocationPlan>(validation.code, "the calibration input is invalid",
                                           kOrigin, "the calibration cannot be planned");
  }
  const U64 elements = element_count(input->raw);
  if (options.block_elements == 0 || options.block_elements > elements) {
    return model::fail<rt::InvocationPlan>(Code::invalid_request,
                                           "the block size is outside the legal range", kOrigin,
                                           "the calibration cannot be planned");
  }
  if (options.window != kSerialWindow) {
    return model::fail<rt::InvocationPlan>(
        Code::invalid_request,
        "the serial plan only accepts window 1; the pipe credit path is not declared", kOrigin,
        "the calibration cannot be planned with an unsupported window");
  }
  if (options.placement == rt::Placement::spill) {
    // Spill is allowed: the bounds advertise it and the four arrays have the
    // canonical block layout.  The fixed plan merely records the placement.
  }

  const U64 n = options.block_elements;
  if (n == 0 || elements > std::numeric_limits<U64>::max() - (n - 1)) {
    return model::fail<rt::InvocationPlan>(Code::resource_exhausted,
                                           "the cohort count overflows", kOrigin,
                                           "the calibration cannot be planned");
  }
  const U64 cohorts = (elements + n - 1) / n;

  const U64 input_payload = saturating_mul(34, n);   // 4 maps * 8n + DQ 2n
  const U64 output_payload = saturating_mul(26, n);  // 3 values * 8n + DQ 2n
  const U64 workspace = storage_workspace_upper(n);
  const U64 progress_bytes =
      saturating_add(saturating_add(input_payload, output_payload),
                     saturating_add(workspace, kCohortControlBytes));

  const U64 final_arrays =
      options.placement == rt::Placement::memory ? saturating_mul(26, elements) : 0;
  const U64 model_bytes = noise_model_bytes(input->request, elements);
  const U64 materialization = frame_materialization_upper(elements);
  const U64 persistent_bytes = saturating_add(
      saturating_add(saturating_add(final_arrays, model_bytes), materialization),
      saturating_add(kFinalMetadataBytes, kPlanControlBytes));
  if (persistent_bytes == std::numeric_limits<U64>::max() ||
      progress_bytes == std::numeric_limits<U64>::max()) {
    return model::fail<rt::InvocationPlan>(Code::resource_exhausted,
                                           "the resource model overflows", kOrigin,
                                           "the calibration cannot be planned");
  }

  rt::InvocationPlan plan;
  plan.options = options;
  plan.elements = elements;
  plan.block_elements = n;
  plan.cohorts = cohorts;
  plan.adaptive = false;
  plan.persistent.host_bytes = persistent_bytes;
  plan.progress_floor.host_bytes = progress_bytes;
  plan.elastic = {};
  plan.control_bytes = saturating_add(kPlanControlBytes, saturating_mul(cohorts, kCohortControlBytes));
  plan.cleanup_bytes = kCleanupBytes;
  plan.cleanup_work_upper = kCleanupWorkUpper;
  plan.input_hash = !input->raw->identity.content_hash.empty()
                        ? input->raw->identity.content_hash
                        : hex_id("raw|" + input->raw->frame + "|" + std::to_string(elements) +
                                 "|" + array_text(input->raw->adu) + "|" +
                                 array_text(input->raw->dq));
  const Calibration& request = input->request;
  plan.request_hash = hex_id(
      "request|" + array_text(request.bias) + "|" + array_text(request.dark) + "|" +
      array_text(request.flat) + "|" + std::to_string(request.dark_contains_bias) + "|" +
      number_text(request.exposure_seconds) + "|" + number_text(request.dark_exposure_seconds) +
      "|" + number_text(request.flat_normalization) + "|" +
      (request.gain_e_per_adu ? number_text(*request.gain_e_per_adu) : "-") + "|" +
      (request.read_rms_e ? number_text(*request.read_rms_e) : "-") + "|" +
      (request.raw_noise ? request.raw_noise->identity.content_hash : "-") + "|" +
      (request.master_noise ? request.master_noise->identity.content_hash : "-"));
  plan.algorithm_hash = hex_id("observation.calibrate|calibration_kernel_v1|1");

  rt::RegionSpec region;
  region.id = 0;
  region.kind = rt::RegionKind::stream;
  region.cohorts = cohorts;
  region.persistent.host_bytes = persistent_bytes;
  region.progress_floor.host_bytes = progress_bytes;

  rt::StageSpec read_stage;
  read_stage.id = 0;
  read_stage.output_upper.host_bytes = input_payload;
  read_stage.control_bytes = kStageControlBytes;
  // Covers the reservations/consumption of up to five correlated reads.
  read_stage.work_upper = read_work_upper(n);

  rt::StageSpec calibrate_stage;
  calibrate_stage.id = 1;
  calibrate_stage.predecessors = {0};
  calibrate_stage.scratch.host_bytes = workspace;
  calibrate_stage.output_upper.host_bytes = output_payload;
  calibrate_stage.control_bytes = kStageControlBytes;
  // One work unit per native sample, including the data quality skips.
  calibrate_stage.work_upper = n;

  rt::StageSpec write_stage;
  write_stage.id = 2;
  write_stage.predecessors = {1};
  write_stage.control_bytes = kStageControlBytes;
  // Four begin + four append-encode + four seal segments.
  write_stage.work_upper = write_work_upper(n);

  region.stages = {read_stage, calibrate_stage, write_stage};

  // No edges: the current composition is serial and uses no pipe/permit
  // credit.  Adding edges here without make_pipe in stream_map_ordered would
  // overstate the scheduler capability (P1-8c).  Stages keep their precedence
  // for diagnostics only.
  region.edges.clear();
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

}  // namespace acsd::modules
