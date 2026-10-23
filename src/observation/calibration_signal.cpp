// The module-side coordinator of the observation.calibrate signal pipeline:
// read one calibration block, run the unique per-sample kernel into four typed
// buffers and append them to the four private writers.  begin/seal/abort own
// the partial-success cleanup of the four writers.
//
// Only public runtime API is used (read_async/allocate/freeze/begin_async/
// append_async/seal_async/abort_async/settle_all).  No pool, ledger, thread or
// output path is created here.
#include <acsd/modules/observation.hpp>
#include <acsd/runtime/diagnostics.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "model/result.hpp"
#include "runtime/internal.hpp"
#include "model/units.hpp"
#include "observation/calibration_kernel.hpp"
#include "observation/observation.hpp"

namespace acsd::modules {
namespace {

using NativeRef = Ref<NativeObservation>;

Result<rt::Done> make_failure(Code code, const char* reason, const char* impact) {
  Result<rt::Done> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, reason, "observation.calibrate", impact);
  return out;
}

template <class T>
Result<T> make_failure(Code code, const char* reason, const char* impact) {
  Result<T> out;
  out.code = code;
  out.assessment = model::make_assessment(code, Capability::supported);
  out.assessment.execution = ExecutionState::failed;
  model::add_issue(out.assessment, code, reason, "observation.calibrate", impact);
  return out;
}

// Merges the secondary cleanup issues into the original scientific failure.
template <class T>
Result<T> with_cleanup(Result<T> primary, const Assessment& cleanup) {
  primary.assessment.issues.insert(primary.assessment.issues.end(),
                                   cleanup.issues.begin(), cleanup.issues.end());
  return primary;
}

// Aborts every supplied writer, never only the first failure, and reports the
// first cleanup error with the merged secondary issues.
rt::Task<rt::Done> abort_writers(rt::InvocationContext context,
                                 std::vector<rt::WriterRef> writers) {
  std::vector<rt::Task<rt::Done>> tasks;
  tasks.reserve(writers.size());
  for (rt::WriterRef& writer : writers) {
    if (writer) tasks.push_back(rt::abort_async(context, std::move(writer)));
  }
  if (tasks.empty()) return rt::ready<rt::Done>(context, model::ok_result(rt::Done{}));
  return rt::then(context, rt::settle_all(context, std::move(tasks)),
                  [context](std::vector<Result<rt::Done>> results) -> rt::Task<rt::Done> {
                    Result<rt::Done> out = model::ok_result(rt::Done{});
                    for (Result<rt::Done>& result : results) {
                      if (result.code != Code::ok) {
                        if (out.code == Code::ok) {
                          out.code = result.code;
                          out.assessment = result.assessment;
                        } else {
                          out.assessment.issues.insert(out.assessment.issues.end(),
                                                       result.assessment.issues.begin(),
                                                       result.assessment.issues.end());
                        }
                      }
                    }
                    return rt::ready<rt::Done>(context, std::move(out));
                  });
}

// Checks that a read block covers exactly the requested flattened range.
bool block_matches(const BlockContext& block, const rt::Block<double>& values) {
  return values.range.begin == block.range.begin && values.range.count == block.range.count &&
         values.values.size() == block.range.count;
}
bool block_matches(const BlockContext& block, const rt::Block<std::uint16_t>& values) {
  return values.range.begin == block.range.begin && values.range.count == block.range.count &&
         values.values.size() == block.range.count;
}

// Reads the masters that actually exist.  An absent map stays empty and is
// never materialised as a constant full-size block.
rt::Task<CalibrationBlock> read_master_at(rt::InvocationContext context, BlockContext block,
                                          Ref<CalibrateInput> input, CalibrationBlock data,
                                          std::size_t level) {
  while (level < 3) {
    ArrayRef array;
    if (level == 0) array = input->request.bias;
    if (level == 1) array = input->request.dark;
    if (level == 2) array = input->request.flat;
    if (!array) {
      ++level;
      continue;
    }
    rt::Task<rt::Block<double>> read =
        rt::read_async<double>(context, block.arena, array, block.range);
    return rt::then(
        context, std::move(read),
        [context, block, input, data = std::move(data), level](rt::Block<double> values) mutable
            -> rt::Task<CalibrationBlock> {
          if (!block_matches(block, values)) {
            return rt::ready<CalibrationBlock>(
                context, make_failure<CalibrationBlock>(
                             Code::invalid_model, "a master block does not cover the requested range",
                             "the calibration block cannot be read"));
          }
          if (level == 0) data.bias = std::move(values);
          if (level == 1) data.dark = std::move(values);
          if (level == 2) data.flat = std::move(values);
          return read_master_at(context, block, input, std::move(data), level + 1);
        });
  }
  return rt::ready<CalibrationBlock>(context, model::ok_result(std::move(data)));
}

}  // namespace

rt::Task<CalibrationBlock> read_calibration_block(BlockContext block, Ref<CalibrateInput> input) {
  const rt::InvocationContext context = block.invocation;
  if (!input || !input->raw) {
    return rt::ready<CalibrationBlock>(
        context, make_failure<CalibrationBlock>(Code::invalid_request,
                                                "the calibration input is missing",
                                                "the block cannot be read"));
  }
  if (!block.arena) {
    return rt::ready<CalibrationBlock>(
        context, make_failure<CalibrationBlock>(Code::invalid_request,
                                                "the block has no bound arena",
                                                "the block cannot be read"));
  }
  if (block.range.count == 0) {
    return rt::ready<CalibrationBlock>(
        context, make_failure<CalibrationBlock>(Code::invalid_request,
                                                "the block range is empty",
                                                "the block cannot be read"));
  }
  rt::Task<rt::Block<double>> raw_read =
      rt::read_async<double>(context, block.arena, input->raw->adu, block.range);
  return rt::then(
      context, std::move(raw_read),
      [context, block, input](rt::Block<double> raw_values) -> rt::Task<CalibrationBlock> {
        if (!block_matches(block, raw_values)) {
          return rt::ready<CalibrationBlock>(
              context, make_failure<CalibrationBlock>(
                           Code::invalid_model, "the raw block does not cover the requested range",
                           "the calibration block cannot be read"));
        }
        rt::Task<rt::Block<std::uint16_t>> dq_read =
            rt::read_async<std::uint16_t>(context, block.arena, input->raw->dq, block.range);
        return rt::then(
            context, std::move(dq_read),
            [context, block, input, raw_values = std::move(raw_values)](
                rt::Block<std::uint16_t> dq_values) mutable -> rt::Task<CalibrationBlock> {
              if (!block_matches(block, dq_values)) {
                return rt::ready<CalibrationBlock>(
                    context, make_failure<CalibrationBlock>(
                                 Code::invalid_model,
                                 "the quality block does not cover the requested range",
                                 "the calibration block cannot be read"));
              }
              CalibrationBlock data;
              data.raw = std::move(raw_values);
              data.dq = std::move(dq_values);
              data.index = input->raw->source_index;
              data.policy = input->request;
              return read_master_at(context, block, input, std::move(data), 0);
            });
      });
}

rt::Task<SignalBlock> calibrate_block(BlockContext block, CalibrationBlock data) {
  const rt::InvocationContext context = block.invocation;
  if (!block.arena) {
    return rt::ready<SignalBlock>(
        context, make_failure<SignalBlock>(Code::invalid_request,
                                           "the block has no bound arena",
                                           "the block cannot be calibrated"));
  }
  const U64 count = block.range.count;
  if (data.raw.values.size() != count || data.dq.values.size() != count) {
    return rt::ready<SignalBlock>(
        context, make_failure<SignalBlock>(Code::invalid_model,
                                           "the input block does not cover the range",
                                           "the block cannot be calibrated"));
  }
  if ((data.bias && data.bias->values.size() != count) ||
      (data.dark && data.dark->values.size() != count) ||
      (data.flat && data.flat->values.size() != count)) {
    return rt::ready<SignalBlock>(
        context, make_failure<SignalBlock>(Code::invalid_model,
                                           "a master block does not cover the range",
                                           "the block cannot be calibrated"));
  }

  rt::TaskSpec spec;
  spec.key = block.key;
  spec.work_upper = count;
  // The execution segment really runs on a CPU worker, so it needs one CPU
  // token; capacity for the buffers is taken from the block arena below.
  spec.running.cpu_tokens = 1;
  spec.semantic = "observation.calibrate/calibrate_block/v1";
  return rt::submit_cpu<SignalBlock>(
      context, spec,
      [block, data = std::move(data)](rt::InvocationContext task) -> Result<SignalBlock> {
        const U64 count = block.range.count;
        const std::string identity_text =
            data.index ? observation::detector_identity(*data.index) : std::string("native");
        const Unit signal_unit = model::unit_signal(identity_text);
        const Unit dq_unit = model::unit_dimensionless();
        const Ref<Index> index = data.index;
        const Id model_hash = data.raw.model_hash;
        Result<rt::Buffer<double>> value_buffer = rt::allocate<double>(
            task, block.arena, block.range, index, signal_unit, model_hash);
        if (!ok(value_buffer)) {
          return make_failure<SignalBlock>(value_buffer.code, "the value buffer cannot be allocated",
                                           "the calibrated block is not written");
        }
        Result<rt::Buffer<double>> lower_buffer = rt::allocate<double>(
            task, block.arena, block.range, index, signal_unit, model_hash);
        if (!ok(lower_buffer)) {
          return make_failure<SignalBlock>(lower_buffer.code,
                                           "the lower bound buffer cannot be allocated",
                                           "the calibrated block is not written");
        }
        Result<rt::Buffer<double>> upper_buffer = rt::allocate<double>(
            task, block.arena, block.range, index, signal_unit, model_hash);
        if (!ok(upper_buffer)) {
          return make_failure<SignalBlock>(upper_buffer.code,
                                           "the upper bound buffer cannot be allocated",
                                           "the calibrated block is not written");
        }
        Result<rt::Buffer<std::uint16_t>> dq_buffer = rt::allocate<std::uint16_t>(
            task, block.arena, block.range, index, dq_unit, model_hash);
        if (!ok(dq_buffer)) {
          return make_failure<SignalBlock>(dq_buffer.code, "the quality buffer cannot be allocated",
                                           "the calibrated block is not written");
        }

        const observation::CalibrationSampleParameters parameters =
            observation::calibration_parameters(data.policy);
        SignalBlock signal;
        CalibrationSampleStats stats;
        for (U64 i = 0; i < count; ++i) {
          observation::CalibrationSampleInputs inputs;
          inputs.raw = data.raw.values[i];
          inputs.dq = data.dq.values[i];
          inputs.bias = data.bias ? (*data.bias).values[i] : 0.0;
          inputs.dark = data.dark ? (*data.dark).values[i] : 0.0;
          inputs.flat = data.flat ? (*data.flat).values[i] : 0.0;
          inputs.parameters = parameters;
          Result<observation::CalibratedSample> sample = observation::calibrate_sample(inputs);
          // Every native sample, including the data quality skips, consumes
          // one work unit of this execution segment.
          Result<rt::Done> consumed = rt::consume_work(task, 1);
          if (!ok(consumed)) {
            return make_failure<SignalBlock>(consumed.code,
                                             "the block work budget is exhausted",
                                             "the calibrated block is not written");
          }
          if (!ok(sample)) {
            const std::string reason =
                sample.assessment.issues.empty()
                    ? std::string("the sample cannot be calibrated")
                    : sample.assessment.issues.front().reason;
            return make_failure<SignalBlock>(sample.code, reason.c_str(),
                                             "the calibrated block is not written");
          }
          if (val(sample).repaired) ++stats.repaired;
          if (!val(sample).flat_usable) ++stats.bad_flat;
          value_buffer.value->values[i] = val(sample).value;
          lower_buffer.value->values[i] = val(sample).lower;
          upper_buffer.value->values[i] = val(sample).upper;
          dq_buffer.value->values[i] = val(sample).dq;
        }

        Result<rt::Block<double>> value = rt::freeze(std::move(*value_buffer.value));
        if (!ok(value)) {
          return make_failure<SignalBlock>(value.code, "the value block cannot be frozen",
                                           "the calibrated block is not written");
        }
        Result<rt::Block<double>> lower = rt::freeze(std::move(*lower_buffer.value));
        if (!ok(lower)) {
          return make_failure<SignalBlock>(lower.code, "the lower block cannot be frozen",
                                           "the calibrated block is not written");
        }
        Result<rt::Block<double>> upper = rt::freeze(std::move(*upper_buffer.value));
        if (!ok(upper)) {
          return make_failure<SignalBlock>(upper.code, "the upper block cannot be frozen",
                                           "the calibrated block is not written");
        }
        Result<rt::Block<std::uint16_t>> dq = rt::freeze(std::move(*dq_buffer.value));
        if (!ok(dq)) {
          return make_failure<SignalBlock>(dq.code, "the quality block cannot be frozen",
                                           "the calibrated block is not written");
        }
        signal.value = std::move(*value.value);
        signal.lower = std::move(*lower.value);
        signal.upper = std::move(*upper.value);
        signal.dq = std::move(*dq.value);
        signal.stats = stats;

#if defined(ACSD_DEV_MODE) && ACSD_DEV_MODE
        // The snapshot only views the blocks that are already frozen and in
        // hand; it never triggers a second read or a new calibration.
        {
          const U64 shape_value = count;
          const std::span<const U64> shape_span(&shape_value, 1);
          const std::string_view index_hash =
              index ? std::string_view(index->identity.object_id) : std::string_view{};
          const std::string_view model_hash_view = model_hash;
          auto make_view = [&](std::string_view name, DType dtype,
                               const auto& block) -> rt::SnapshotView {
            using Item = typename std::remove_reference_t<decltype(block.values)>::element_type;
            return rt::SnapshotView{
                name,
                index_hash,
                dtype,
                block.range,
                shape_span,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(block.values.data()),
                    block.values.size() * sizeof(Item)),
                std::cref(block.unit),
                std::cref(block.lease)};
          };
          const rt::SnapshotView views[4] = {
              make_view("value", DType::f64, signal.value),
              make_view("numerical_lower", DType::f64, signal.lower),
              make_view("numerical_upper", DType::f64, signal.upper),
              make_view("dq", DType::u16, signal.dq),
          };
          const rt::SnapshotField fields[3] = {
              {"exposure_ratio", "1", parameters.exposure_ratio},
              {"flat_normalization", "1", parameters.flat_normalization},
              {"samples", "1", static_cast<U64>(count)},
          };
          rt::SnapshotPayload payload;
          payload.views = views;
          payload.fields = fields;
          payload.coordinate_space = "detector";
          payload.index_hash = index_hash;
          rt::ProbeTags tags;
          tags.component = "observation.calibrate";
          if (!model_hash_view.empty()) tags.model_hash = model_hash_view;
          ACSD_DEV_SNAPSHOT(task, "normalize.calibration.signal", tags, payload);
        }
#endif
        return model::ok_result(std::move(signal));
      });
}

rt::Task<SignalWriter> begin_signal(rt::InvocationContext context, Ref<rt::InvocationPlan> plan,
                                    Ref<CalibrateInput> input) {
  if (!plan || !input || !input->raw || !input->raw->source_index) {
    return rt::ready<SignalWriter>(
        context, make_failure<SignalWriter>(Code::invalid_request,
                                            "the calibration plan or input is missing",
                                            "the signal writers are not created"));
  }
  struct BeginState {
    rt::InvocationContext context;
    Ref<CalibrateInput> input;
    U64 elements = 0;
    U64 block_elements = 0;
    std::vector<rt::WriterRef> created;
    std::shared_ptr<CalibrationSampleStats> stats;
    std::vector<rt::BuildSpec> specs;
  };
  auto state = std::make_shared<BeginState>();
  state->context = context;
  state->input = input;
  state->elements = plan->elements;
  state->block_elements = plan->block_elements;
  state->stats = std::make_shared<CalibrationSampleStats>();
  if (state->elements == 0 || state->block_elements == 0) {
    return rt::ready<SignalWriter>(
        context, make_failure<SignalWriter>(Code::invalid_request,
                                            "the plan has no bounded cohort",
                                            "the signal writers are not created"));
  }
  const std::string identity_text = observation::detector_identity(*input->raw->source_index);
  const Unit signal_unit = model::unit_signal(identity_text);
  const Unit dq_unit = model::unit_dimensionless();
  const Ref<Index> index = input->raw->source_index;
  const std::size_t element_bytes = sizeof(double);
  auto make_spec = [&](DType dtype, const Unit& unit, std::size_t item_bytes) {
    rt::BuildSpec spec;
    spec.layout.dtype = dtype;
    spec.layout.shape = {state->elements};
    spec.layout.unit = unit;
    spec.layout.index = index;
    spec.placement = plan->options.placement;
    spec.buffer_bytes = state->elements * item_bytes;
    spec.metadata_bytes = 512 + 8 * spec.layout.shape.size();
    return spec;
  };
  state->specs.push_back(make_spec(DType::f64, signal_unit, element_bytes));
  state->specs.push_back(make_spec(DType::f64, signal_unit, element_bytes));
  state->specs.push_back(make_spec(DType::f64, signal_unit, element_bytes));
  state->specs.push_back(make_spec(DType::u16, dq_unit, sizeof(std::uint16_t)));

  // Sequential creation: a failure drains and aborts every writer that was
  // already created, never only the failing one.
  auto create = std::make_shared<std::function<rt::Task<SignalWriter>(std::size_t)>>();
  *create = [state, create](std::size_t index_position) -> rt::Task<SignalWriter> {
    if (index_position >= state->specs.size()) {
      SignalWriter writer;
      writer.value = state->created[0];
      writer.lower = state->created[1];
      writer.upper = state->created[2];
      writer.dq = state->created[3];
      writer.stats = state->stats;
      return rt::ready<SignalWriter>(state->context, model::ok_result(std::move(writer)));
    }
    rt::Task<rt::WriterRef> begun =
        rt::begin_async(state->context, state->context.persistent, state->specs[index_position]);
    return rt::then(
        state->context, std::move(begun),
        [state, create, index_position](rt::WriterRef writer) -> rt::Task<SignalWriter> {
          state->created.push_back(std::move(writer));
          return (*create)(index_position + 1);
        });
  };
  rt::Task<SignalWriter> chain = (*create)(0);
  // The chain above has already bound the state; a begin failure returns a
  // non-ok Task that still has to abort the created writers.
  return rt::settle(
      context, std::move(chain),
      [state](Result<SignalWriter> result) -> rt::Task<SignalWriter> {
        if (result.code == Code::ok && result.value) {
          return rt::ready<SignalWriter>(state->context, std::move(result));
        }
        std::vector<rt::WriterRef> created = state->created;
        state->created.clear();
        return rt::settle(
            state->context, abort_writers(state->context, std::move(created)),
            [state, result = std::move(result)](Result<rt::Done> cleanup) mutable
            -> rt::Task<SignalWriter> {
              Result<SignalWriter> failed = with_cleanup(std::move(result), cleanup.assessment);
              return rt::ready<SignalWriter>(state->context, std::move(failed));
            });
      });
}

rt::Task<rt::Done> append_signal(BlockContext block, SignalWriter writer, SignalBlock data) {
  const rt::InvocationContext context = block.invocation;
  if (!writer.value || !writer.lower || !writer.upper || !writer.dq) {
    return rt::ready<rt::Done>(context,
                               make_failure(Code::invalid_request,
                                            "the signal writer is incomplete",
                                            "the calibrated block is not appended"));
  }
  const U64 count = block.range.count;
  if (data.value.values.size() != count || data.lower.values.size() != count ||
      data.upper.values.size() != count || data.dq.values.size() != count) {
    return rt::ready<rt::Done>(context,
                               make_failure(Code::invalid_model,
                                            "the signal block does not cover the range",
                                            "the calibrated block is not appended"));
  }
  const U64 expected = block.range.begin + count;
  std::vector<rt::Task<U64>> appends;
  appends.push_back(rt::append_async(context, writer.value, std::move(data.value)));
  appends.push_back(rt::append_async(context, writer.lower, std::move(data.lower)));
  appends.push_back(rt::append_async(context, writer.upper, std::move(data.upper)));
  appends.push_back(rt::append_async(context, writer.dq, std::move(data.dq)));
  return rt::then(
      context, rt::when_all(context, std::move(appends)),
      [context, writer, expected, stats = data.stats](std::vector<U64> cumulative)
          -> rt::Task<rt::Done> {
        if (cumulative.size() != 4) {
          return rt::ready<rt::Done>(
              context, make_failure(Code::internal, "the append results are incomplete",
                                    "the calibrated block is not appended"));
        }
        for (U64 value : cumulative) {
          if (value != expected) {
            return rt::ready<rt::Done>(
                context, make_failure(Code::invalid_request,
                                      "the append did not reach the expected element count",
                                      "the writer is failed and must be aborted"));
          }
        }
        if (writer.stats) {
          writer.stats->bad_flat += stats.bad_flat;
          writer.stats->repaired += stats.repaired;
        }
        return rt::ready<rt::Done>(context, model::ok_result(rt::Done{}));
      });
}

rt::Task<SignalArrays> seal_signal(rt::InvocationContext context, SignalWriter writer) {
  if (!writer.value || !writer.lower || !writer.upper || !writer.dq) {
    return rt::ready<SignalArrays>(
        context, make_failure<SignalArrays>(Code::invalid_request, "the signal writer is incomplete",
                                            "the calibrated frame cannot be sealed"));
  }
  std::vector<rt::Task<ArrayRef>> seals;
  seals.push_back(rt::seal_async(context, writer.value));
  seals.push_back(rt::seal_async(context, writer.lower));
  seals.push_back(rt::seal_async(context, writer.upper));
  seals.push_back(rt::seal_async(context, writer.dq));
  std::vector<rt::WriterRef> writers{writer.value, writer.lower, writer.upper, writer.dq};
  return rt::then(
      context, rt::settle_all(context, std::move(seals)),
      [context, writers = std::move(writers), stats = writer.stats](
          std::vector<Result<ArrayRef>> results) -> rt::Task<SignalArrays> {
        bool failed = false;
        Result<ArrayRef> first_failure;
        for (Result<ArrayRef>& result : results) {
          if (result.code != Code::ok || !result.value) {
            if (!failed) {
              failed = true;
              first_failure = std::move(result);
            }
          }
        }
        if (!failed) {
          SignalArrays arrays;
          arrays.value = results[0].value.value_or(nullptr);
          arrays.lower = results[1].value.value_or(nullptr);
          arrays.upper = results[2].value.value_or(nullptr);
          arrays.dq = results[3].value.value_or(nullptr);
          if (stats) arrays.stats = *stats;
          return rt::ready<SignalArrays>(context, model::ok_result(std::move(arrays)));
        }
        // A partially sealed frame is not deliverable: abort every writer (an
        // already sealed abort only discards the private staging) and release
        // the successful ArrayRefs by dropping them.
        Result<SignalArrays> failure;
        failure.code = first_failure.code;
        failure.assessment = first_failure.assessment;
        return rt::settle(
            context, abort_writers(context, writers),
            [context, failure = std::move(failure)](Result<rt::Done> cleanup) mutable
            -> rt::Task<SignalArrays> {
              return rt::ready<SignalArrays>(
                  context, with_cleanup(std::move(failure), cleanup.assessment));
            });
      });
}

rt::Task<rt::Done> abort_signal(rt::InvocationContext context, SignalWriter writer) {
  std::vector<rt::WriterRef> writers{writer.value, writer.lower, writer.upper, writer.dq};
  return rt::settle(
      context, abort_writers(context, std::move(writers)),
      [context](Result<rt::Done> cleanup) -> rt::Task<rt::Done> {
        return rt::ready<rt::Done>(context, std::move(cleanup));
      });
}

rt::Task<NativeRef> finish_native(rt::InvocationContext context, Ref<CalibrateInput> input,
                                  SignalArrays arrays) {
  if (!input || !input->raw || !input->raw->source_index) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(Code::invalid_request, "the calibration input is missing",
                                         "the native observation is not delivered"));
  }
  if (!arrays.value || !arrays.lower || !arrays.upper || !arrays.dq) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(Code::invalid_model,
                                         "the sealed signal arrays are incomplete",
                                         "the native observation is not delivered"));
  }
  const RawObservation& raw = *input->raw;
  const std::vector<U64>& shape = arrays.value->shape;
  U64 count = 1;
  for (U64 extent : shape) {
    if (extent != 0 && count > std::numeric_limits<U64>::max() / extent) {
      return rt::ready<NativeRef>(context,
                                  make_failure<NativeRef>(Code::invalid_model,
                                                          "the array shape overflows",
                                                          "the native observation is not delivered"));
    }
    count *= extent;
  }
  if (count != raw.width * raw.height) {
    return rt::ready<NativeRef>(context,
                                make_failure<NativeRef>(Code::invalid_model,
                                                        "the signal arrays do not match the frame",
                                                        "the native observation is not delivered"));
  }
  auto same_layout = [&](ArrayRef array, DType dtype, bool dimensionless) {
    if (!array || array->dtype != dtype) return false;
    if (array->shape != shape) return false;
    if (array->index_id != arrays.value->index_id) return false;
    if (dimensionless) return array->unit.scale == "1" && array->unit.linear_power == 0;
    return array->unit.scale == arrays.value->unit.scale &&
           array->unit.linear_power == arrays.value->unit.linear_power;
  };
  if (!same_layout(arrays.lower, DType::f64, false) ||
      !same_layout(arrays.upper, DType::f64, false) ||
      !same_layout(arrays.dq, DType::u16, true)) {
    return rt::ready<NativeRef>(context,
                                make_failure<NativeRef>(Code::invalid_model,
                                                        "the signal arrays are not co-indexed",
                                                        "the native observation is not delivered"));
  }

  // The one existing noise/identity assembly (calibration_kernel.cpp) is fed
  // through the runtime science Context bridge: the calibrated value/quality
  // arrays and the original input planes are already registered in the block
  // store, so the assembly reads them without re-running the pixel kernel and
  // the NoiseModel arrays it creates are owned by that same storage.
  //
  // The bridge declaration currently lives in src/runtime/internal.hpp; a
  // public include/acsd/modules/context.hpp wrapper is expected to replace
  // this include.
  // The final model arrays belong to the persistent R budget of the plan, not
  // to the per-cohort progress lane.
  Result<std::shared_ptr<rt::detail::ScienceContext>> bridge =
      rt::detail::make_science_context(context, context.persistent ? context.persistent
                                                                   : context.progress);
  if (!ok(bridge)) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(bridge.code, "the science context bridge cannot be created",
                                         "the native observation is not delivered"));
  }
  acsd::Context& science = (*bridge.value)->context();
  Result<std::vector<double>> values = observation::read_f64_values(science, arrays.value);
  if (!ok(values)) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(values.code, "the value array cannot be read back",
                                         "the native observation is not delivered"));
  }
  Result<std::vector<std::uint16_t>> flags = observation::read_u16_values(science, arrays.dq);
  if (!ok(flags)) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(flags.code, "the quality array cannot be read back",
                                         "the native observation is not delivered"));
  }
  std::vector<double> flat;
  if (input->request.flat) {
    Result<std::vector<double>> read = observation::read_f64_values(science, input->request.flat);
    if (!ok(read)) {
      return rt::ready<NativeRef>(
          context, make_failure<NativeRef>(read.code, "the flat array cannot be read back",
                                           "the native observation is not delivered"));
    }
    flat = val(read);
  }
  const bool has_gain = input->request.gain_e_per_adu.has_value() &&
                        std::isfinite(*input->request.gain_e_per_adu) &&
                        *input->request.gain_e_per_adu > 0.0;
  const bool has_read = input->request.read_rms_e.has_value() &&
                        std::isfinite(*input->request.read_rms_e) &&
                        *input->request.read_rms_e >= 0.0;
  std::vector<double> raw_values;
  std::vector<double> bias_values;
  if (!input->request.raw_noise && has_gain && has_read) {
    Result<std::vector<double>> read = observation::read_f64_values(science, raw.adu);
    if (!ok(read)) {
      return rt::ready<NativeRef>(
          context, make_failure<NativeRef>(read.code, "the raw array cannot be read back",
                                           "the native observation is not delivered"));
    }
    raw_values = val(read);
    if (input->request.bias) {
      Result<std::vector<double>> bias = observation::read_f64_values(science, input->request.bias);
      if (!ok(bias)) {
        return rt::ready<NativeRef>(
            context, make_failure<NativeRef>(bias.code, "the bias array cannot be read back",
                                             "the native observation is not delivered"));
      }
      bias_values = val(bias);
    }
  }

  observation::CalibrationAssemblyInputs assembly;
  assembly.raw = raw_values.empty() ? std::span<const double>{}
                                    : std::span<const double>(raw_values);
  assembly.bias = bias_values.empty() ? std::span<const double>{}
                                      : std::span<const double>(bias_values);
  assembly.flat = flat.empty() ? std::span<const double>{} : std::span<const double>(flat);
  assembly.values = std::span<const double>(val(values));
  assembly.flags = std::span<const std::uint16_t>(val(flags));
  Result<Ref<NativeObservation>> native = observation::assemble_calibrated_native(
      science, input->raw, input->request, assembly, arrays.value, arrays.lower, arrays.upper,
      arrays.dq, arrays.stats);
  if (!ok(native)) {
    return rt::ready<NativeRef>(
        context, make_failure<NativeRef>(native.code, "the native observation cannot be assembled",
                                         "the numeric calibration is complete but the model is not"));
  }
  return rt::ready<NativeRef>(context, model::ok_result(std::move(val(native))));
}

}  // namespace acsd::modules
