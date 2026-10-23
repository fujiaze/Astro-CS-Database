// The observation.calibrate typed process adapter: descriptor, registration,
// the stream-region composition and the finalize hook.
//
// The adapter only sequences modules::begin_signal -> stream_map_ordered
// (read_calibration_block -> calibrate_block -> append_signal) -> seal_signal
// -> finish_native.  It repeats no pixel formula and creates no pool, thread,
// ledger or output path; every block/timeout/resource decision comes from the
// plan and the unique runtime.
#include <acsd/modules/observation.hpp>
#include <acsd/runtime/diagnostics.hpp>

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "model/result.hpp"
#include "model/units.hpp"
#include "model/units.hpp"
#include "observation/observation.hpp"
#include "runtime/internal.hpp"

namespace acsd::modules {

using rt::Done;

namespace {

using NativeRef = Ref<NativeObservation>;

Result<Done> adapter_failure(Code code, const char* reason, const char* impact) {
  Result<Done> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, reason, "observation.calibrate", impact);
  return out;
}

template <class T>
Result<T> adapter_failure(Code code, const char* reason, const char* impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, reason, "observation.calibrate", impact);
  return out;
}

rt::Task<NativeRef> run_calibration_region(rt::InvocationContext context,
                                           Ref<CalibrateInput> input,
                                           Ref<rt::InvocationPlan> plan) {
  return rt::then(
      context, begin_signal(context, plan, input),
      [context, input, plan](SignalWriter writer) -> rt::Task<NativeRef> {
        auto body = stream_map_ordered<CalibrationBlock, SignalBlock>(
            context, plan, 0,
            [input](BlockContext block) { return read_calibration_block(block, input); },
            [](BlockContext block, CalibrationBlock data) {
              return calibrate_block(block, std::move(data));
            },
            [writer](BlockContext block, SignalBlock data) {
              return append_signal(block, writer, std::move(data));
            });
        return rt::settle(
            context, std::move(body),
            [context, input, writer](Result<Done> completed) -> rt::Task<NativeRef> {
              if (completed.code != Code::ok || !completed.value) {
                return rt::settle(
                    context, abort_signal(context, writer),
                    [context, completed](Result<Done> cleanup) mutable -> rt::Task<NativeRef> {
                      // The original scientific/IO failure stays the primary
                      // code; the cleanup issues are secondary.
                      Result<NativeRef> failure = failure_like<NativeRef>(completed);
                      failure.assessment.issues.insert(failure.assessment.issues.end(),
                                                       cleanup.assessment.issues.begin(),
                                                       cleanup.assessment.issues.end());
                      return rt::ready<NativeRef>(context, std::move(failure));
                    });
              }
              return rt::then(context, seal_signal(context, writer),
                              [context, input](SignalArrays arrays) -> rt::Task<NativeRef> {
                                return finish_native(context, input, std::move(arrays));
                              });
            });
      });
}

rt::Task<NativeRef> run_calibration(rt::InvocationContext context, Ref<CalibrateInput> input,
                                    Ref<rt::InvocationPlan> plan) {
  if (!plan) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_request, "no invocation plan supplied",
                                            "the calibration cannot run"));
  }
  const U64 start_ns = rt::steady_ns();
  const U64 span_id = rt::allocate_span_id(context);
  rt::Task<NativeRef> region =
      rt::run_region<NativeRef>(context, plan, 0,
                                [input, plan](rt::InvocationContext region_context) {
                                  return run_calibration_region(region_context, input, plan);
                                });
  return rt::then(
      context, std::move(region),
      [context, input, start_ns, span_id](NativeRef value) -> rt::Task<NativeRef> {
#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        rt::PerfRecord record;
        record.probe_id = "normalize.calibration.frame";
        record.kind = rt::PerfKind::span;
        record.start_steady_ns = start_ns;
        record.wall_ns = rt::steady_ns() - start_ns;
        record.span_id = span_id;
        if (input && input->raw) record.tags.frame = std::string_view(input->raw->frame);
        record.tags.component = "observation.calibrate";
        ACSD_DEV_PERF(context, record);
#else
        (void)input;
        (void)start_ns;
        (void)span_id;
#endif
        return rt::ready<NativeRef>(context, model::ok_result(std::move(value)));
      });
}

rt::Task<NativeRef> finalize_calibration(rt::InvocationContext context, Ref<CalibrateInput> input,
                                         Ref<rt::InvocationPlan>, Result<NativeRef> result) {
  if (result.code != Code::ok || !result.value) {
    return rt::ready<NativeRef>(context, std::move(result));
  }
  return validate_native(context, input, *result.value);
}

}  // namespace

rt::Task<NativeRef> validate_native(rt::InvocationContext context, Ref<CalibrateInput> input,
                                    Ref<NativeObservation> native) {
  if (!input || !input->raw || !native) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_request,
                                            "the native observation or input is missing",
                                            "the calibrated result cannot be validated"));
  }
  const RawObservation& raw = *input->raw;
  if (!native->integrated_values || !native->numerical_lower || !native->numerical_upper ||
      !native->dq || !native->native_index || !native->noise || !native->regrouping ||
      !native->error) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the native observation is incomplete",
                                            "the calibrated result cannot be published"));
  }
  if (native->native_index->identity.object_id != raw.source_index->identity.object_id) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the native index differs from the source index",
                                            "the calibrated result cannot be published"));
  }
  if (native->frame != raw.frame || native->context.epoch_julian_year !=
                                        raw.context.epoch_julian_year ||
      native->context.observation_jd_tdb != raw.context.observation_jd_tdb) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the frame or astrometric context was not preserved",
                                            "the calibrated result cannot be published"));
  }
  if (!native->physical_collection) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the calibration is not a physical collection",
                                            "the calibrated result cannot be published"));
  }
  const std::vector<U64>& shape = native->integrated_values->shape;
  auto same_layout = [&](ArrayRef array, DType dtype, bool dimensionless) {
    if (!array || array->dtype != dtype) return false;
    if (array->shape != shape) return false;
    if (array->index_id != native->integrated_values->index_id) return false;
    if (dimensionless) return array->unit.scale == "1" && array->unit.linear_power == 0;
    return array->unit.scale == native->integrated_values->unit.scale &&
           array->unit.linear_power == native->integrated_values->unit.linear_power;
  };
  if (!same_layout(native->numerical_lower, DType::f64, false) ||
      !same_layout(native->numerical_upper, DType::f64, false) ||
      !same_layout(native->dq, DType::u16, true)) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the calibrated arrays are not co-indexed",
                                            "the calibrated result cannot be published"));
  }
  const std::size_t roots = native->noise->innovations.size() +
                            native->noise->shared_factors.size();
  if (native->noise->influence.size() < roots) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the noise model has fewer influences than roots",
                                            "the calibrated result cannot be published"));
  }
  if (!native->error->assessment.complete) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the numerical error record is not complete",
                                            "the calibrated result cannot be published"));
  }

  // The numeric enclosure is checked on the delivered payload: lower <= value
  // <= upper for every finite sample; a sample without a value (NaN because a
  // flat was invalid) must stay NaN in the value array.
  Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
      rt::detail::make_science_context(context, context.persistent ? context.persistent
                                                                   : context.progress);
  if (!ok(bridge)) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(bridge.code, "the science context bridge cannot be created",
                                            "the calibrated result cannot be validated"));
  }
  acsd::Context& science = (*bridge.value)->context();
  Result<std::vector<double>> values = observation::read_f64_values(science, native->integrated_values);
  Result<std::vector<double>> lower = observation::read_f64_values(science, native->numerical_lower);
  Result<std::vector<double>> upper = observation::read_f64_values(science, native->numerical_upper);
  if (!ok(values) || !ok(lower) || !ok(upper)) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the calibrated payload cannot be read back",
                                            "the calibrated result cannot be validated"));
  }
  if (val(values).size() != val(lower).size() || val(values).size() != val(upper).size()) {
    return rt::ready<NativeRef>(
        context, adapter_failure<NativeRef>(Code::invalid_model,
                                            "the calibrated arrays have different lengths",
                                            "the calibrated result cannot be validated"));
  }
  for (std::size_t i = 0; i < val(values).size(); ++i) {
    const double value = val(values)[i];
    if (std::isnan(value)) {
      // A sample without a value (invalid flat or non-finite expression) may
      // carry NaN endpoints or the unbounded div-by-zero enclosure; it must
      // never claim a finite numeric bound.
      if (std::isfinite(val(lower)[i]) && std::isfinite(val(upper)[i])) {
        return rt::ready<NativeRef>(
            context, adapter_failure<NativeRef>(Code::invalid_model,
                                                "a sample without a value claims a finite enclosure",
                                                "the calibrated result cannot be validated"));
      }
      continue;
    }
    if (!(val(lower)[i] <= value && value <= val(upper)[i])) {
      return rt::ready<NativeRef>(
          context, adapter_failure<NativeRef>(Code::invalid_model,
                                              "the numerical enclosure does not contain the value",
                                              "the calibrated result cannot be published"));
    }
  }
  return rt::ready<NativeRef>(context, model::ok_result(Ref<NativeObservation>(native)));
}

Result<rt::Done> register_calibration(rt::RegistryBuilder& builder) {
  rt::ProcessDescriptor descriptor{
      "observation.calibrate",
      "observation",
      "docs/engineering/rebuild/processes/observation.calibrate.md",
      1,
      {"model", "numerics", "operators", "uncertainty"},
      {{"normalize.calibration.signal", "snapshot_array", 1},
       {"normalize.calibration.frame", "span", 1}}};
  Result<std::shared_ptr<rt::ProcessRegistration>> process = builder.begin(std::move(descriptor));
  if (!ok(process)) {
    return adapter_failure<Done>(process.code, "the calibration process cannot be registered",
                                 "observation.calibrate is unavailable");
  }
  rt::Operation<CalibrateInput, NativeRef> operation{
      "calibrate_observation",
      {"acsd.input.Calibrate", 1},
      {"acsd.NativeObservation", 1},
      &validate_calibration,
      &bounds_calibration,
      &plan_calibration,
      &run_calibration,
      &finalize_calibration};
  return builder.add<CalibrateInput, NativeRef>(*process.value, std::move(operation));
}

}  // namespace acsd::modules
