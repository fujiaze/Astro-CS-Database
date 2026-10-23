#pragma once
// Runtime plumbing for the fixed workflow entry points.
//
// The workflow is orchestration only: it freezes one registry, opens one
// command scope and invokes the registered typed operations.  The helpers here
// do not implement science; they (a) copy the workflow's immutable input
// arrays into the runtime BlockStore so the ProcessHost science bridge can read
// them, (b) expose a Storage view that publishes arrays produced by the Host,
// and (c) record the registry plan of every invocation for the validation
// trace.
#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <acsd/modules/generated/status.hpp>
#include <acsd/runtime/generated_manifest.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include "model/array.hpp"
#include "model/result.hpp"
#include "runtime/internal.hpp"

namespace acsd::workflows {

using rt::Done;
using rt::InvocationContext;
using rt::PlanOptions;
using rt::Registry;
using rt::Services;

struct WorkflowRuntime {
  std::shared_ptr<rt::Services> services;
  std::shared_ptr<const rt::Registry> registry;
  InvocationContext command;
  std::shared_ptr<rt::ProcessHost> host;
  rt::ArenaRef arena;
  rt::PlanOptions options{1, 1, rt::Placement::memory};
  std::set<Id> ingested;
  std::size_t steps = 0;
};

inline U64 runtime_element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    if (total > (~0ull) / extent) return 0;
    total *= extent;
  }
  return total;
}

// Copies one immutable array into the runtime BlockStore.  The identity of the
// array is unchanged: downstream science reads the same bytes under the same
// object id, so a second formula or a second identity is impossible.
inline Result<Done> ingest_array(Context& source, WorkflowRuntime& runtime,
                                 const ArrayRef& array) {
  if (!array) return model::ok_result(Done{});
  const Id id = array->identity.object_id;
  if (runtime.ingested.count(id) != 0) return model::ok_result(Done{});
  const U64 count = runtime_element_count(array->shape);
  if (count == 0) {
    runtime.ingested.insert(id);
    return model::ok_result(Done{});
  }
  Result<ArrayView> view =
      source.storage.read(source, array, BlockRequest{0, count});
  if (!view.value || view.code != Code::ok)
    return model::fail<Done>(view.code == Code::ok ? Code::missing_dependency
                                                   : view.code,
                             "the input array cannot be read for the runtime",
                             "workflow.ingest", "the invocation cannot read it");
  std::vector<std::byte> bytes(view.value->bytes.begin(),
                               view.value->bytes.end());
  auto data = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
  runtime.services->blocks->register_array(id, data, array->dtype,
                                           array->shape);
  runtime.ingested.insert(id);
  return model::ok_result(Done{});
}

inline Result<Done> ingest_index(Context& source, WorkflowRuntime& runtime,
                                 const Ref<Index>& index) {
  if (!index) return model::ok_result(Done{});
  return ingest_array(source, runtime, index->ids);
}

inline Result<Done> ingest_noise(Context& source, WorkflowRuntime& runtime,
                                 const Ref<NoiseModel>& model) {
  if (!model) return model::ok_result(Done{});
  for (const auto& root : model->innovations) {
    if (!root) continue;
    Result<Done> step = ingest_index(source, runtime, root->coordinates);
    if (!step.value) return step;
  }
  for (const auto& root : model->shared_factors) {
    if (!root) continue;
    Result<Done> step = ingest_index(source, runtime, root->coordinates);
    if (!step.value) return step;
  }
  return model::ok_result(Done{});
}

inline Result<Done> ingest_covariance(Context& source,
                                      WorkflowRuntime& runtime,
                                      const Ref<CovarianceAction>& covariance) {
  if (!covariance) return model::ok_result(Done{});
  return ingest_noise(source, runtime, covariance->model);
}

inline Result<Done> ingest_pixel_response(Context& source,
                                          WorkflowRuntime& runtime,
                                          const Ref<PixelResponse>& response) {
  if (!response) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, response->gains);
  if (!step.value) return step;
  return ingest_noise(source, runtime, response->uncertainty);
}

inline Result<Done> ingest_footprints(Context& source,
                                      WorkflowRuntime& runtime,
                                      const Ref<FootprintSet>& footprints) {
  if (!footprints) return model::ok_result(Done{});
  Result<Done> step = ingest_index(source, runtime, footprints->index);
  if (!step.value) return step;
  step = ingest_pixel_response(source, runtime, footprints->pixel_response);
  if (!step.value) return step;
  step = ingest_array(source, runtime, footprints->sample_response_gain);
  if (!step.value) return step;
  if (footprints->exceptions) {
    for (const ArrayRef& array :
         {footprints->exceptions->sample_rows,
          footprints->exceptions->ring_offsets,
          footprints->exceptions->vertex_offsets,
          footprints->exceptions->vertices,
          footprints->exceptions->hole_flags}) {
      step = ingest_array(source, runtime, array);
      if (!step.value) return step;
    }
  }
  if (footprints->astrometry) {
    step = ingest_array(source, runtime, footprints->astrometry->parameters);
    if (!step.value) return step;
    step = ingest_covariance(source, runtime,
                             footprints->astrometry->parameter_noise);
    if (!step.value) return step;
    step = ingest_array(source, runtime,
                        footprints->astrometry->solver_detections);
    if (!step.value) return step;
  }
  return model::ok_result(Done{});
}

inline Result<Done> ingest_raw(Context& source, WorkflowRuntime& runtime,
                               const Ref<RawObservation>& raw) {
  if (!raw) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, raw->adu);
  if (!step.value) return step;
  step = ingest_array(source, runtime, raw->dq);
  if (!step.value) return step;
  step = ingest_index(source, runtime, raw->source_index);
  if (!step.value) return step;
  return ingest_footprints(source, runtime, raw->footprints);
}

inline Result<Done> ingest_calibration(Context& source,
                                       WorkflowRuntime& runtime,
                                       const Calibration& calibration) {
  Result<Done> step = ingest_array(source, runtime, calibration.bias);
  if (!step.value) return step;
  step = ingest_array(source, runtime, calibration.dark);
  if (!step.value) return step;
  step = ingest_array(source, runtime, calibration.flat);
  if (!step.value) return step;
  step = ingest_noise(source, runtime, calibration.raw_noise);
  if (!step.value) return step;
  return ingest_noise(source, runtime, calibration.master_noise);
}

inline Result<Done> ingest_response(Context& source, WorkflowRuntime& runtime,
                                    const Ref<Response>& response) {
  if (!response) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, response->wavelength_nm);
  if (!step.value) return step;
  step = ingest_array(source, runtime, response->throughput);
  if (!step.value) return step;
  return ingest_noise(source, runtime, response->shape_noise);
}

inline Result<Done> ingest_spectrum(Context& source, WorkflowRuntime& runtime,
                                    const Ref<Spectrum>& spectrum) {
  if (!spectrum) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, spectrum->wavelength_nm);
  if (!step.value) return step;
  step = ingest_array(source, runtime, spectrum->flux_W_m2_nm);
  if (!step.value) return step;
  return ingest_covariance(source, runtime, spectrum->noise);
}

inline Result<Done> ingest_basis(Context& source, WorkflowRuntime& runtime,
                                 const Ref<SpectralBasis>& basis) {
  if (!basis) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, basis->knots_nm);
  if (!step.value) return step;
  return ingest_array(source, runtime, basis->basis_coefficients);
}

inline Result<Done> ingest_star(Context& source, WorkflowRuntime& runtime,
                                const CatalogueStar& star) {
  Result<Done> step = ingest_array(source, runtime, star.xp_coefficients);
  if (!step.value) return step;
  step = ingest_basis(source, runtime, star.xp_basis);
  if (!step.value) return step;
  step = ingest_covariance(source, runtime, star.astrometric_noise);
  if (!step.value) return step;
  return ingest_covariance(source, runtime, star.xp_noise);
}

inline Result<Done> ingest_information(Context& source,
                                       WorkflowRuntime& runtime,
                                       const InformationRequest& request) {
  Result<Done> step = ingest_array(source, runtime, request.h);
  if (!step.value) return step;
  step = ingest_covariance(source, runtime, request.covariance);
  if (!step.value) return step;
  if (request.template_evaluation) {
    step = ingest_array(source, runtime, request.template_evaluation->values);
    if (!step.value) return step;
  }
  return model::ok_result(Done{});
}

// Backwards-compatible name used by the normalize entry point.
using NormalizeRuntime = WorkflowRuntime;

// A Storage view for publishing: arrays produced inside the Host are read from
// the runtime BlockStore, everything else is the workflow's original storage.
class WorkflowStorage final : public Storage {
 public:
  WorkflowStorage(Storage& base, std::shared_ptr<rt::Services> services)
      : base_(base), services_(std::move(services)) {}

  Result<ArrayRef> copy(Context& context, DType dtype,
                        std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit,
                        Id index) override {
    return base_.copy(context, dtype, bytes, shape, unit, index);
  }

  Result<ArrayView> read(Context& context, ArrayRef array,
                         BlockRequest range) const override {
    if (array) {
      auto bytes = services_->blocks->find(array->identity.object_id);
      if (bytes) {
        const U64 count = runtime_element_count(array->shape);
        const U64 begin = std::min<U64>(range.begin, count);
        const U64 end = std::min<U64>(range.begin + range.count, count);
        if (begin <= end && (end - begin) * model::dtype_size(array->dtype) <=
                                bytes->size()) {
          const std::size_t offset =
              static_cast<std::size_t>(begin) *
              static_cast<std::size_t>(model::dtype_size(array->dtype));
          const std::size_t length =
              static_cast<std::size_t>(end - begin) *
              static_cast<std::size_t>(model::dtype_size(array->dtype));
          ArrayView view;
          view.dtype = array->dtype;
          view.shape = array->shape;
          view.bytes = std::span<const std::byte>(bytes->data() + offset,
                                                  length);
          return model::ok_result(view);
        }
      }
    }
    return base_.read(context, array, range);
  }

  Result<ArrayRef> open(Context& context, std::string root,
                        const Array& description,
                        std::span<const BlockDescriptor> blocks) override {
    return base_.open(context, std::move(root), description, blocks);
  }

 private:
  Storage& base_;
  std::shared_ptr<rt::Services> services_;
};


// Opens the one runtime used by a fixed workflow: frozen registry, one command
// scope, one host.  The pool/ledger lives exactly as long as the returned
// object; callers use it for every operation of the entry point.
inline Result<std::shared_ptr<WorkflowRuntime>> open_workflow_runtime(
    const Id& name, U64 host_bytes = 1ull << 30, U64 max_work = 4000000000ull,
    U32 cpu_tokens = 2, PlanOptions options = PlanOptions{1, 1, rt::Placement::memory}) {
  rt::PoolConfig pool;
  pool.cpu_threads = 2;
  pool.read_slots = 1;
  pool.write_slots = 1;
  pool.process.ceiling.host_bytes = host_bytes;
  pool.process.ceiling.cpu_tokens = cpu_tokens;
  pool.process.max_work = max_work;
  Result<std::shared_ptr<rt::Services>> services = rt::make_services(pool);
  if (!ok(services))
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        services.code == Code::ok ? Code::internal : services.code,
        "the workflow runtime cannot be created", "workflow.runtime",
        "the workflow cannot run");
  auto runtime = std::make_shared<WorkflowRuntime>();
  runtime->services = *services.value;
  rt::RegistryBuilder builder;
  Result<Done> registered = generated::register_builtin_types(builder);
  if (!registered.value)
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        registered.code, "the runtime type codecs cannot be registered",
        "workflow.runtime", "the workflow cannot run");
  registered = generated::register_processes(builder);
  if (!registered.value)
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        registered.code, "the runtime processes cannot be registered",
        "workflow.runtime", "the workflow cannot run");
  Result<std::shared_ptr<const rt::Registry>> frozen = builder.freeze();
  if (!frozen.value)
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        frozen.code, "the runtime registry cannot be frozen", "workflow.runtime",
        "the workflow cannot run");
  runtime->registry = *frozen.value;
  rt::ScopeLimits limits;
  limits.ceiling.host_bytes = host_bytes;
  limits.ceiling.cpu_tokens = cpu_tokens;
  limits.max_work = max_work;
  Result<InvocationContext> command = rt::open_command(runtime->services, name, limits, 1);
  if (!command.value)
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        command.code, "the workflow command scope cannot be opened",
        "workflow.runtime", "the workflow cannot run");
  runtime->command = *command.value;
  Result<std::shared_ptr<rt::ProcessHost>> host =
      rt::make_host(runtime->services, runtime->registry);
  if (!host.value)
    return model::fail<std::shared_ptr<WorkflowRuntime>>(
        host.code, "the process host cannot be created", "workflow.runtime",
        "the workflow cannot run");
  runtime->host = *host.value;
  runtime->options = options;
  return model::ok_result(runtime);
}

inline Result<Done> ingest_support(Context& source, WorkflowRuntime& runtime,
                                   const Ref<SupportMap>& support) {
  if (!support) return model::ok_result(Done{});
  Result<Done> step = ingest_index(source, runtime, support->index);
  if (!step.value) return step;
  for (const ArrayRef& array :
       {support->states, support->area_lower_sr, support->area_upper_sr,
        support->cell_face, support->cell_depth, support->cell_x, support->cell_y,
        support->cell_state, support->normalization_states,
        support->observational_states}) {
    step = ingest_array(source, runtime, array);
    if (!step.value) return step;
  }
  return model::ok_result(Done{});
}

inline Result<Done> ingest_signal(Context& source, WorkflowRuntime& runtime,
                                  const Ref<SignalEstimate>& estimate) {
  if (!estimate) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, estimate->signal);
  if (!step.value) return step;
  step = ingest_array(source, runtime, estimate->numerical_lower);
  if (!step.value) return step;
  step = ingest_array(source, runtime, estimate->numerical_upper);
  if (!step.value) return step;
  step = ingest_support(source, runtime, estimate->support);
  if (!step.value) return step;
  step = ingest_covariance(source, runtime, estimate->noise);
  if (!step.value) return step;
  if (estimate->effective_response) {
    const auto& response = *estimate->effective_response;
    for (const ArrayRef& array : {response.weights, response.reference_factors}) {
      step = ingest_array(source, runtime, array);
      if (!step.value) return step;
    }
    for (const Ref<Response>& item : response.responses) {
      step = ingest_response(source, runtime, item);
      if (!step.value) return step;
    }
  }
  return model::ok_result(Done{});
}

inline Result<Done> ingest_product_graph(Context& source, WorkflowRuntime& runtime,
                                         const Ref<ProductGraph>& graph) {
  if (!graph) return model::ok_result(Done{});
  return ingest_signal(source, runtime, graph->signal);
}


inline Result<Done> ingest_frame_description(Context& source, WorkflowRuntime& runtime,
                                             const Ref<FrameDescription>& description) {
  if (!description) return model::ok_result(Done{});
  Result<Done> step = ingest_index(source, runtime, description->index);
  if (!step.value) return step;
  step = ingest_support(source, runtime, description->support);
  if (!step.value) return step;
  step = ingest_covariance(source, runtime, description->noise);
  if (!step.value) return step;
  if (description->response) {
    const auto& response = *description->response;
    for (const ArrayRef& array : {response.weights, response.reference_factors}) {
      step = ingest_array(source, runtime, array);
      if (!step.value) return step;
    }
  }
  return model::ok_result(Done{});
}

inline Result<Done> ingest_frame_inputs(Context& source, WorkflowRuntime& runtime,
                                        const std::vector<Ref<FrameDescription>>& descriptions,
                                        const std::vector<ArrayRef>& signals,
                                        const std::vector<ArrayRef>& dq) {
  for (const Ref<FrameDescription>& description : descriptions) {
    Result<Done> step = ingest_frame_description(source, runtime, description);
    if (!step.value) return step;
  }
  for (const ArrayRef& signal : signals) {
    Result<Done> step = ingest_array(source, runtime, signal);
    if (!step.value) return step;
  }
  for (const ArrayRef& item : dq) {
    Result<Done> step = ingest_array(source, runtime, item);
    if (!step.value) return step;
  }
  return model::ok_result(Done{});
}

}  // namespace acsd::workflows
