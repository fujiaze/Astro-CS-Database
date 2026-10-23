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

struct NormalizeRuntime {
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
inline Result<Done> ingest_array(Context& source, NormalizeRuntime& runtime,
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

inline Result<Done> ingest_index(Context& source, NormalizeRuntime& runtime,
                                 const Ref<Index>& index) {
  if (!index) return model::ok_result(Done{});
  return ingest_array(source, runtime, index->ids);
}

inline Result<Done> ingest_noise(Context& source, NormalizeRuntime& runtime,
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
                                      NormalizeRuntime& runtime,
                                      const Ref<CovarianceAction>& covariance) {
  if (!covariance) return model::ok_result(Done{});
  return ingest_noise(source, runtime, covariance->model);
}

inline Result<Done> ingest_pixel_response(Context& source,
                                          NormalizeRuntime& runtime,
                                          const Ref<PixelResponse>& response) {
  if (!response) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, response->gains);
  if (!step.value) return step;
  return ingest_noise(source, runtime, response->uncertainty);
}

inline Result<Done> ingest_footprints(Context& source,
                                      NormalizeRuntime& runtime,
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

inline Result<Done> ingest_raw(Context& source, NormalizeRuntime& runtime,
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
                                       NormalizeRuntime& runtime,
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

inline Result<Done> ingest_response(Context& source, NormalizeRuntime& runtime,
                                    const Ref<Response>& response) {
  if (!response) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, response->wavelength_nm);
  if (!step.value) return step;
  step = ingest_array(source, runtime, response->throughput);
  if (!step.value) return step;
  return ingest_noise(source, runtime, response->shape_noise);
}

inline Result<Done> ingest_spectrum(Context& source, NormalizeRuntime& runtime,
                                    const Ref<Spectrum>& spectrum) {
  if (!spectrum) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, spectrum->wavelength_nm);
  if (!step.value) return step;
  step = ingest_array(source, runtime, spectrum->flux_W_m2_nm);
  if (!step.value) return step;
  return ingest_covariance(source, runtime, spectrum->noise);
}

inline Result<Done> ingest_basis(Context& source, NormalizeRuntime& runtime,
                                 const Ref<SpectralBasis>& basis) {
  if (!basis) return model::ok_result(Done{});
  Result<Done> step = ingest_array(source, runtime, basis->knots_nm);
  if (!step.value) return step;
  return ingest_array(source, runtime, basis->basis_coefficients);
}

inline Result<Done> ingest_star(Context& source, NormalizeRuntime& runtime,
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
                                       NormalizeRuntime& runtime,
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

}  // namespace acsd::workflows
