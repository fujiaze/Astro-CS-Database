// bind_wcs and bind_observation_model: the immutable revisions of a native
// observation.
//
// Both entry points never modify the object they receive.  They build a new
// revision of the observation, share the native arrays with their parent and
// keep the random roots, the influences, the context and the frame.  The
// revision number increases and the content hash is recomputed, so a caller
// that holds the old object keeps reading the old model.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "observation/observation.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "bind_wcs";

// A new revision of an object: the logical identity stays, the payload hash
// and the revision number change.
Identity revise(const Identity& parent, const std::string& payload) {
  Identity identity = parent;
  identity.content_hash = model::content_id(model::sha256_hex(payload));
  identity.revision = parent.revision + 1;
  return identity;
}

std::string describe(const std::string& tag, const Id& frame, U64 count) {
  return tag + ":" + frame + ":" + std::to_string(count);
}

}  // namespace

Result<Ref<NativeObservation>> bind_wcs(Context& context, Ref<NativeObservation> observation,
                                        Ref<WcsModel> wcs, Ref<AstrometricSolution> solution) {
  if (!observation) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no observation supplied",
                                               kOrigin, "the frame model cannot be bound");
  }
  if (!wcs) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no frame model supplied",
                                               kOrigin, "the frame model cannot be bound");
  }
  if (!observation->footprints) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the observation has no detector footprint", kOrigin,
                                               "the frame model cannot be bound");
  }
  if (observation->footprints->detector_frame != wcs->detector_frame) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the frame model and the footprint set use different detectors", kOrigin,
        "the frame model cannot be bound");
  }
  if (solution) {
    if (!solution->wcs) {
      return model::fail<Ref<NativeObservation>>(
          Code::invalid_model, "the astrometric solution carries no frame model", kOrigin,
          "the frame model cannot be bound");
    }
    if (solution->wcs->identity.content_hash != wcs->identity.content_hash) {
      return model::fail<Ref<NativeObservation>>(
          Code::identity_conflict, "the astrometric solution belongs to another frame model", kOrigin,
          "the frame model cannot be bound");
    }
  }
  // The whole detector domain has to be covered by the frame model.
  const Box& domain = wcs->domain;
  const double width = static_cast<double>(observation->footprints->width);
  const double height = static_cast<double>(observation->footprints->height);
  if (domain.lower[0] > -0.5 || domain.lower[1] > -0.5 ||
      domain.upper[0] < width - 0.5 || domain.upper[1] < height - 0.5) {
    return model::fail<Ref<NativeObservation>>(
        Code::domain, "the frame model does not cover the whole detector domain", kOrigin,
        "the frame model cannot be bound");
  }

  // The footprint set gets a new revision that carries the frame model and the
  // astrometric solution; the arrays of the observation are shared.
  auto footprints = std::make_shared<FootprintSet>(*observation->footprints);
  footprints->wcs = wcs;
  footprints->astrometry = solution;
  footprints->identity = revise(observation->footprints->identity,
                                describe("footprints", observation->frame, width * height) + ":" +
                                    wcs->identity.content_hash);

  auto bound = std::make_shared<NativeObservation>(*observation);
  bound->footprints = footprints;
  bound->identity = revise(observation->identity,
                            describe("native", observation->frame, observation->native_index->size) +
                                ":" + wcs->identity.content_hash);
  bound->assessment = observation->assessment;
  bound->assessment.complete = true;
  model::set_included(bound->assessment, "wcs_bound_to_the_observation");
  if (solution) {
    model::set_included(bound->assessment, "astrometric_solution_bound");
    if (!solution->parameter_noise && solution->influence.from_measurements == nullptr) {
      // The solution carries neither a parameter covariance nor an influence:
      // the astrometric error is unknown and must not be filled with zero.
      model::set_omitted(bound->assessment, "astrometric_parameter_error_is_unknown");
      model::add_issue(bound->assessment, Code::unavailable,
                        "the astrometric parameters carry no error identity", kOrigin,
                        "the astrometric uncertainty is unknown");
    }
    if (!solution->model_bias_rad.has_value()) {
      model::set_omitted(bound->assessment, "astrometric_model_bias_is_unknown");
    }
  } else {
    // An externally supplied frame model without a solution has no declared
    // error identity either.
    model::set_omitted(bound->assessment, "astrometric_parameter_error_is_unknown");
  }

  (void)context;
  return model::ok_result(std::shared_ptr<const NativeObservation>(bound));
}

Result<Ref<NativeObservation>> bind_observation_model(Context& context,
                                                      Ref<NativeObservation> observation,
                                                      Ref<EffectivePSF> kernel,
                                                      Ref<Response> response) {
  if (!observation) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no observation supplied",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (!kernel) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no PSF kernel supplied",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (!response) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request, "no response supplied",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (kernel->frame != observation->frame) {
    return model::fail<Ref<NativeObservation>>(
        Code::identity_conflict, "the PSF kernel belongs to another frame", "bind_observation_model",
        "the model cannot be bound");
  }
  if (observation->response && !observation->scale_applications.empty() &&
      observation->response->identity.content_hash != response->identity.content_hash) {
    // A scaled observation cannot change its response behind the back of the
    // applied scale.
    return model::fail<Ref<NativeObservation>>(
        Code::identity_conflict, "the observation is already scaled with another response",
        "bind_observation_model", "the model cannot be bound");
  }
  if (!kernel->detector_samples) {
    return model::fail<Ref<NativeObservation>>(Code::unavailable,
                                               "the PSF kernel has no native samples",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (kernel->detector_samples->detector_frame != observation->footprints->detector_frame) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the PSF kernel and the observation use different detectors",
        "bind_observation_model", "the model cannot be bound");
  }
  // The response has to describe a wavelength range that its own samples
  // cover; an unsupported or empty response is refused.
  if (!(response->support_nm.lower <= response->support_nm.upper)) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                               "the response support is not an interval",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (!response->wavelength_nm || response->wavelength_nm->shape.empty()) {
    return model::fail<Ref<NativeObservation>>(Code::unavailable,
                                               "the response has no wavelength samples",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }
  if (!response->throughput || response->throughput->shape.empty()) {
    return model::fail<Ref<NativeObservation>>(Code::unavailable, "the response has no throughput",
                                               "bind_observation_model",
                                               "the model cannot be bound");
  }

  auto bound = std::make_shared<NativeObservation>(*observation);
  bound->kernel = kernel;
  bound->response = response;
  bound->identity = revise(observation->identity,
                            describe("native", observation->frame, observation->native_index->size) +
                                ":" + kernel->identity.content_hash + ":" +
                                response->identity.content_hash);
  bound->assessment = observation->assessment;
  bound->assessment.complete = true;
  model::set_included(bound->assessment, "psf_kernel_bound");
  model::set_included(bound->assessment, "response_bound");
  // The photon or energy convention and the support of the response belong to
  // the identity of the model, so a cache key is self contained.
  model::set_included(bound->assessment,
                       std::string("response_convention=") +
                           (response->convention == Response::Convention::photon_throughput
                                ? "photon_throughput"
                                : "includes_photon_conversion"));
  model::set_included(bound->assessment,
                       "response_support_nm=[" + std::to_string(response->support_nm.lower) + "," +
                           std::to_string(response->support_nm.upper) + "]");
  if (!response->shape_noise) {
    model::set_omitted(bound->assessment, "the_shape_noise_of_the_response_is_unknown");
  }
  if (observation->noise && observation->noise->state == NoiseState::unknown) {
    model::set_omitted(bound->assessment, "the_complete_noise_variance_is_unknown");
  }

  (void)context;
  return model::ok_result(std::shared_ptr<const NativeObservation>(bound));
}

}  // namespace acsd
