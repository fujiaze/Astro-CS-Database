// validate_calibration / bounds_calibration / plan_calibration of the
// observation.calibrate process.
//
// validate reads metadata only: it never opens an Array block, so a cold disk
// input cannot turn the synchronous validation into payload I/O.  The actual
// NaN/DQ consistency and the per-value flat range are checked by the CPU
// stage after the block hash, in calibrate_block.
//
// The plan is the fixed resource model of CALIBRATION_WALKTHROUGH.md:
//   n = block_elements, N = elements, cohorts = ceil(N/n)
//   one stream region (id 0) with stages 0 read, 1 calibrate, 2 write
//   edges 0->1 and 1->2, window = PlanOptions.window (default 1)
//   input payload per cohort  raw+bias+dark+flat (4 x 8n) + raw DQ 2n = 34n
//   output payload per cohort value+lower+upper (3 x 8n) + DQ 2n = 26n
//   progress_floor F = 60n + S + control
//   persistent R = final output (memory placement) + final NoiseModel arrays
//                   + ErrorRecord/metadata/control
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

// The provisional conservative upper bound of the CPU/Storage working set of
// one cohort (interval primitive scratch, hashing and codec state).  The
// implementation must replace it with the measured/allocated upper bound from
// the numerics/Storage workspace entry once those services expose it; until
// then it is deliberately larger than the actual per-sample workspace.
constexpr U64 kStorageWorkspaceUpper = 1ull << 20;  // 1 MiB

// Fixed control-node budget of one cohort plan and of the final assembly.
constexpr U64 kCohortControlBytes = 4ull << 10;    // 4 KiB
constexpr U64 kStageControlBytes = 1ull << 10;     // 1 KiB
constexpr U64 kPlanControlBytes = 16ull << 10;     // 16 KiB
constexpr U64 kFinalMetadataBytes = 16ull << 10;   // 16 KiB
constexpr U64 kCleanupBytes = 64ull << 10;         // 4 writer states + nodes
constexpr U64 kCleanupWorkUpper = 64;

constexpr U64 kDefaultBlockElements = 65536;
constexpr U32 kMaxWindow = 4;

U64 saturating_mul(U64 a, U64 b) {
  if (a == 0 || b == 0) return 0;
  if (a > std::numeric_limits<U64>::max() / b) return std::numeric_limits<U64>::max();
  return a * b;
}

U64 saturating_add(U64 a, U64 b) {
  if (a > std::numeric_limits<U64>::max() - b) return std::numeric_limits<U64>::max();
  return a + b;
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

// The declared root identity of a noise model: every concatenated root must
// carry a complete SharedId and a factor, otherwise the shared/cross term
// cannot be merged later.
Result<Done> validate_noise_model_metadata(const Ref<NoiseModel>& model, const char* label) {
  if (!model) return model::ok_result(Done{});
  const std::size_t roots = model->innovations.size() + model->shared_factors.size();
  if (model->influence.size() < roots) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label +
                                 " noise model has fewer influences than roots",
                             kOrigin, "the calibration cannot be planned");
  }
  auto check_root = [&](const Ref<NoiseRoot>& root, const char* what) -> Result<Done> {
    if (!root || !root->factor) {
      return model::fail<Done>(Code::invalid_model,
                               std::string("the ") + label + " " + what +
                                   " root has no factor",
                               kOrigin, "the noise model cannot be merged");
    }
    const SharedId& id = root->shared_id;
    if (id.external_namespace.empty() || id.entity.empty() || id.model_revision.empty() ||
        id.component.empty()) {
      return model::fail<Done>(Code::invalid_model,
                               std::string("the ") + label + " " + what +
                                   " root has an incomplete shared identity",
                               kOrigin, "the noise root cannot be merged by identity");
    }
    return model::ok_result(Done{});
  };
  for (const Ref<NoiseRoot>& root : model->innovations) {
    Result<Done> check = check_root(root, "innovation");
    if (!ok(check)) return check;
  }
  for (const Ref<NoiseRoot>& root : model->shared_factors) {
    Result<Done> check = check_root(root, "shared");
    if (!ok(check)) return check;
  }
  // A shared root must appear once: two identical identities would be counted
  // as two independent random roots.
  for (std::size_t i = 0; i < model->shared_factors.size(); ++i) {
    for (std::size_t j = i + 1; j < model->shared_factors.size(); ++j) {
      const SharedId& a = model->shared_factors[i]->shared_id;
      const SharedId& b = model->shared_factors[j]->shared_id;
      if (a.external_namespace == b.external_namespace && a.entity == b.entity &&
          a.model_revision == b.model_revision && a.component == b.component) {
        return model::fail<Done>(Code::invalid_model,
                                 std::string("the ") + label +
                                     " noise model lists the same shared root twice",
                                 kOrigin, "the cross term would be counted twice");
      }
    }
  }
  return model::ok_result(Done{});
}

Result<Done> check_array_metadata(const ArrayRef& array, U64 count, DType dtype,
                                  const char* label, bool check_dimensionless) {
  if (!array) return model::ok_result(Done{});
  if (array->dtype != dtype) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has the wrong dtype",
                             kOrigin, "the calibration cannot be planned");
  }
  if (array->index_id.empty()) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has no index", kOrigin,
                             "the calibration cannot be planned");
  }
  if (!observation::array_has_count(*array, count)) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array does not match the frame",
                             kOrigin, "the calibration cannot be planned");
  }
  if (array->unit.scale.empty()) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array has no unit", kOrigin,
                             "the calibration cannot be planned");
  }
  if (check_dimensionless && (array->unit.scale != "1" || array->unit.linear_power != 0)) {
    return model::fail<Done>(Code::invalid_model,
                             std::string("the ") + label + " array must be dimensionless", kOrigin,
                             "the data quality flags are not a physical quantity");
  }
  return model::ok_result(Done{});
}

// Metadata validation shared by validate/bounds/plan.
Result<Done> validate_metadata(const Ref<RawObservation>& raw, const Calibration& request) {
  if (!raw) {
    return model::fail<Done>(Code::invalid_request, "no raw observation supplied", kOrigin,
                             "the observation cannot be calibrated");
  }
  if (!raw->adu || !raw->dq) {
    return model::fail<Done>(Code::invalid_model,
                             "the raw observation has no data or quality array", kOrigin,
                             "the observation cannot be calibrated");
  }
  if (!raw->source_index) {
    return model::fail<Done>(Code::invalid_model, "the raw observation has no source index",
                             kOrigin, "the observation cannot be calibrated");
  }
  if (raw->width == 0 || raw->height == 0) {
    return model::fail<Done>(Code::invalid_model, "the frame has no samples", kOrigin,
                             "the observation cannot be calibrated");
  }
  if (raw->width > std::numeric_limits<U64>::max() / raw->height) {
    return model::fail<Done>(Code::invalid_model, "the frame size overflows", kOrigin,
                             "the observation cannot be calibrated");
  }
  const U64 count = raw->width * raw->height;
  if (raw->source_index->size != count) {
    return model::fail<Done>(Code::invalid_model, "the source index does not cover the frame",
                             kOrigin, "the observation cannot be calibrated");
  }
  Result<Done> adu =
      check_array_metadata(raw->adu, count, DType::f64, "raw data", false);
  if (!ok(adu)) return adu;
  Result<Done> dq = check_array_metadata(raw->dq, count, DType::u16, "raw quality", true);
  if (!ok(dq)) return dq;

  Result<Done> bias = check_array_metadata(request.bias, count, DType::f64, "bias", false);
  if (!ok(bias)) return bias;
  Result<Done> dark = check_array_metadata(request.dark, count, DType::f64, "dark", false);
  if (!ok(dark)) return dark;
  Result<Done> flat = check_array_metadata(request.flat, count, DType::f64, "flat", false);
  if (!ok(flat)) return flat;

  if (raw->frame.empty()) {
    return model::fail<Done>(Code::invalid_model, "the frame has no identity", kOrigin,
                             "the calibrated observation cannot keep the frame");
  }
  if (!raw->footprints) {
    return model::fail<Done>(Code::unavailable, "the frame has no detector footprint", kOrigin,
                             "the footprint capability is missing");
  }

  if (!std::isfinite(request.exposure_seconds) || !(request.exposure_seconds > 0.0)) {
    return model::fail<Done>(Code::invalid_model,
                             "the exposure time must be finite and positive", kOrigin,
                             "the observation cannot be calibrated");
  }
  if (!request.flat) {
    // An absent flat is the explicit normalisation 1, never a 0 that silently
    // becomes "no flat".
    if (request.flat_normalization != 1.0) {
      return model::fail<Done>(Code::invalid_model,
                               "an absent flat requires the explicit normalisation 1", kOrigin,
                               "the calibration cannot be planned");
    }
  } else {
    if (!std::isfinite(request.flat_normalization) || !(request.flat_normalization > 0.0)) {
      return model::fail<Done>(Code::invalid_model,
                               "the flat normalisation must be finite and positive", kOrigin,
                               "the observation cannot be calibrated");
    }
  }
  if (request.dark) {
    if (!std::isfinite(request.dark_exposure_seconds) || !(request.dark_exposure_seconds > 0.0)) {
      return model::fail<Done>(Code::invalid_model,
                               "the dark exposure time must be positive when a dark is used",
                               kOrigin, "the observation cannot be calibrated");
    }
    const double ratio = request.exposure_seconds / request.dark_exposure_seconds;
    if (!std::isfinite(ratio) || !(ratio > 0.0)) {
      return model::fail<Done>(Code::invalid_model, "the exposure ratio must be positive", kOrigin,
                               "the observation cannot be calibrated");
    }
  }
  Result<Done> raw_noise = validate_noise_model_metadata(request.raw_noise, "raw");
  if (!ok(raw_noise)) return raw_noise;
  Result<Done> master_noise = validate_noise_model_metadata(request.master_noise, "master");
  if (!ok(master_noise)) return master_noise;
  return model::ok_result(Done{});
}

Result<Done> require_input(const Ref<CalibrateInput>& input) {
  if (!input) {
    return model::fail<Done>(Code::invalid_request, "no calibration input supplied", kOrigin,
                             "the observation cannot be calibrated");
  }
  return validate_metadata(input->raw, input->request);
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
  bounds.max_window = kMaxWindow;
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
  if (options.window == 0 || options.window > kMaxWindow) {
    return model::fail<rt::InvocationPlan>(Code::invalid_request,
                                           "the window is outside the legal range", kOrigin,
                                           "the calibration cannot be planned");
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
  const U64 workspace = kStorageWorkspaceUpper;
  const U64 progress_bytes =
      saturating_add(saturating_add(input_payload, output_payload),
                     saturating_add(workspace, kCohortControlBytes));

  const U64 final_arrays =
      options.placement == rt::Placement::memory ? saturating_mul(26, elements) : 0;
  const U64 model_bytes = noise_model_bytes(input->request, elements);
  const U64 persistent_bytes =
      saturating_add(saturating_add(final_arrays, model_bytes),
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
  read_stage.work_upper = 0;

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
  write_stage.work_upper = 0;

  region.stages = {read_stage, calibrate_stage, write_stage};

  rt::EdgeSpec read_edge;
  read_edge.id = 0;
  read_edge.from = 0;
  read_edge.to = 1;
  read_edge.max_block_bytes = input_payload;
  read_edge.window.max_items = options.window;
  read_edge.window.max_queued_bytes = saturating_mul(input_payload, options.window);
  read_edge.window.order = rt::PipeOrder::ordered;

  rt::EdgeSpec write_edge;
  write_edge.id = 1;
  write_edge.from = 1;
  write_edge.to = 2;
  write_edge.max_block_bytes = output_payload;
  write_edge.window.max_items = options.window;
  write_edge.window.max_queued_bytes = saturating_mul(output_payload, options.window);
  write_edge.window.order = rt::PipeOrder::ordered;

  region.edges = {read_edge, write_edge};
  plan.regions = {region};
  plan.region_edges.clear();
  return model::ok_result(plan);
}

}  // namespace acsd::modules
