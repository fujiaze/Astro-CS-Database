// apply_scale: applies the fitted photometric scale to a native observation.
//
// Scientific contract: docs/science/foundation/GAIA_RESPONSE.md §1-§3 and
// docs/engineering/rebuild/SYSTEM.md N3 / "apply_scale" in API.md.
//
// Two different objects are kept strictly apart:
//
//   * the source end amplitude  a(source) = exp(h(source)^T theta) is the
//     native ADU per unit L of the star and is carried by the new kernel
//     through `source_scale`;
//   * the pixel end factor        alpha(u) = exp(-h(u)^T theta) multiplies
//     every native sample and is applied to the signal, to the template
//     (PSF density) and to the noise chain.
//
// The spatial factor is applied on the most original physical parent, sample
// by sample, and the physical regrouping G is applied afterwards, so that a
// physical bin never uses the alpha of its centre (SYSTEM N3).
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "photometry/spatial_basis.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "apply_scale";

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array,
                                            const std::string& what) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " array is missing",
                                             kOrigin, "the scale cannot be applied");
  }
  U64 total = 1;
  for (U64 extent : array->shape) {
    if (extent == 0) {
      total = 0;
      break;
    }
    if (total > (~0ull) / extent) {
      return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " shape overflows",
                                               kOrigin, "the scale cannot be applied");
    }
    total *= extent;
  }
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, total});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the " + what + " array cannot be read",
                                             kOrigin, "the scale cannot be applied");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<ArrayRef> copy_f64(Context& context, const std::vector<double>& values,
                          const std::vector<U64>& shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

// The detector position of a native sample, following the footprint set
// convention (sample centre = (row % width, row / width), transformed by the
// detector_from_sample matrix and the offset).
void sample_position(const FootprintSet& footprints, U64 row, double& x, double& y) {
  const U64 width = footprints.width > 0 ? footprints.width : 1;
  const double column = static_cast<double>(row % width);
  const double line = static_cast<double>(row / width);
  const Mat2& m = footprints.detector_from_sample;
  x = m[0] * column + m[1] * line + footprints.offset[0];
  y = m[2] * column + m[3] * line + footprints.offset[1];
}

// The diagonal action of the spatial factor on a set of native samples.
Result<Ref<LinearAction>> make_diagonal_action(Context& context, const std::vector<double>& factors,
                                                 const Ref<Index>& index) {
  Result<ArrayRef> values =
      copy_f64(context, factors, {static_cast<U64>(factors.size())}, model::unit_dimensionless(),
               index ? index->identity.object_id : Id());
  if (!ok(values)) {
    return model::fail<Ref<LinearAction>>(values.code, "the spatial factors cannot be stored", kOrigin,
                                          "the scale cannot be applied");
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "canonical_diagonal_v1";
  recipe.version = 1;
  recipe.arrays = {"spatial_factor"};
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = model::unit_dimensionless();
  recipe.spaces.output = model::unit_dimensionless();
  const std::vector<ArrayRef> arrays{val(values)};
  Result<Ref<LinearAction>> action =
      make_action(context, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (!ok(action)) {
    return model::fail<Ref<LinearAction>>(action.code, "the spatial factor action cannot be built",
                                          kOrigin, "the scale cannot be applied");
  }
  return action;
}

Result<std::vector<double>> apply_action(Context& context, Ref<LinearAction> action,
                                         const std::vector<double>& values, const std::string& what) {
  if (!action) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the " + what + " action is missing",
                                             kOrigin, "the scale cannot be applied");
  }
  Result<ArrayRef> input =
      copy_f64(context, values, {static_cast<U64>(values.size())}, model::unit_dimensionless(), Id());
  if (!ok(input)) {
    return model::fail<std::vector<double>>(input.code, "the " + what + " input cannot be stored",
                                             kOrigin, "the scale cannot be applied");
  }
  Result<ArrayRef> mapped = apply(context, action, val(input));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the " + what + " action cannot be applied",
                                             kOrigin, "the scale cannot be applied");
  }
  Result<ArrayView> view =
      context.storage.read(context, val(mapped), BlockRequest{0, static_cast<U64>(values.size())});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the " + what + " output cannot be read",
                                             kOrigin, "the scale cannot be applied");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

// Composes the noise influences with an action on their output.
Result<Ref<NoiseModel>> scale_noise(Context& context, Ref<NoiseModel> noise,
                                     Ref<LinearAction> factor) {
  if (!noise) {
    return model::fail<Ref<NoiseModel>>(Code::invalid_request, "no noise model supplied", kOrigin,
                                         "the scale cannot be applied");
  }
  auto scaled = std::make_shared<NoiseModel>();
  scaled->variance_unit = noise->variance_unit;
  scaled->condition = noise->condition;
  scaled->innovations = noise->innovations;
  scaled->shared_factors = noise->shared_factors;
  scaled->unknown_terms = noise->unknown_terms;
  scaled->state = noise->state;
  for (const Ref<LinearAction>& influence : noise->influence) {
    Result<Ref<LinearAction>> composed = compose(context, factor, influence);
    if (!ok(composed)) {
      return model::fail<Ref<NoiseModel>>(composed.code, "the noise chain cannot be scaled", kOrigin,
                                           "the scale cannot be applied");
    }
    scaled->influence.push_back(val(composed));
  }
  return model::ok_result(std::shared_ptr<const NoiseModel>(scaled));
}

}  // namespace

Result<Ref<NativeObservation>> apply_scale(Context& context, Ref<PhotoSolution> solution,
                                           Ref<NativeObservation> observation, Id frame) {
  if (!solution || !observation) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_request,
                                               "a solution and an observation are required", kOrigin,
                                               "the scale cannot be applied");
  }
  const auto frame_it = std::find(solution->frames.begin(), solution->frames.end(), frame);
  if (frame_it == solution->frames.end()) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_request, "the requested frame is not part of the photometric solution", kOrigin,
        "the scale cannot be applied");
  }
  if (observation->frame != frame) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_request, "the observation does not belong to the requested frame", kOrigin,
        "the scale cannot be applied");
  }
  if (std::find(observation->scale_applications.begin(), observation->scale_applications.end(),
                solution->application_id) != observation->scale_applications.end()) {
    return model::fail<Ref<NativeObservation>>(
        Code::identity_conflict, "the photometric scale was already applied to this observation",
        kOrigin, "the scale cannot be applied twice");
  }
  if (!observation->footprints) {
    return model::fail<Ref<NativeObservation>>(Code::invalid_model, "the observation has no footprints",
                                               kOrigin, "the scale cannot be applied");
  }

  // ---------------------------------------------------------------------
  // The chain of physical parents, from the root to the current observation.
  // ---------------------------------------------------------------------
  std::vector<Ref<NativeObservation>> chain;
  {
    Ref<NativeObservation> current = observation;
    while (current) {
      chain.push_back(current);
      current = current->native_parent;
      if (chain.size() > 64) {
        return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                   "the native parent chain is too long", kOrigin,
                                                   "the scale cannot be applied");
      }
    }
    std::reverse(chain.begin(), chain.end());
  }
  const Ref<NativeObservation>& root = chain.front();

  // ---------------------------------------------------------------------
  // The spatial factor of the frame, sample by sample, on the root parent.
  // ---------------------------------------------------------------------
  const std::size_t frame_index = static_cast<std::size_t>(frame_it - solution->frames.begin());
  const std::size_t terms = photometry::basis_term_count(solution->spatial_basis);
  const std::size_t root_samples =
      root->native_index ? root->native_index->size : 0;

  std::vector<double> theta(terms, 0.0);
  {
    Result<std::vector<double>> values = read_f64_array(context, solution->theta, "parameters");
    if (!ok(values)) {
      return model::fail<Ref<NativeObservation>>(values.code, "the parameters cannot be read", kOrigin,
                                                 "the scale cannot be applied");
    }
    if (val(values).size() < (frame_index + 1) * terms) {
      return model::fail<Ref<NativeObservation>>(Code::invalid_model,
                                                 "the parameter array does not match the frames",
                                                 kOrigin, "the scale cannot be applied");
    }
    for (std::size_t term = 0; term < terms; ++term) {
      theta[term] = val(values)[frame_index * terms + term];
    }
  }

  // The parameter box gives the interval of the factor; without it the
  // numerical certificate of the scaled values is not unconditional.
  std::vector<double> theta_lower(theta);
  std::vector<double> theta_upper(theta);
  bool has_parameter_box = false;
  if (solution->parameter_lower && solution->parameter_upper) {
    Result<std::vector<double>> lower = read_f64_array(context, solution->parameter_lower, "parameters");
    Result<std::vector<double>> upper = read_f64_array(context, solution->parameter_upper, "parameters");
    if (ok(lower) && ok(upper) && val(lower).size() >= (frame_index + 1) * terms &&
        val(upper).size() >= (frame_index + 1) * terms) {
      for (std::size_t term = 0; term < terms; ++term) {
        theta_lower[term] = val(lower)[frame_index * terms + term];
        theta_upper[term] = val(upper)[frame_index * terms + term];
      }
      has_parameter_box = true;
    }
  }

  // The spatial terms are reduced with the training weighted mean that the
  // fit actually used, so that the applied factor and the fitted field are
  // the same field.
  std::vector<double> spatial_mean;
  {
    Result<std::vector<double>> mean =
        read_f64_array(context, solution->spatial_basis.weighted_mean, "basis weighted mean");
    if (ok(mean)) {
      spatial_mean = val(mean);
    }
  }

  std::vector<double> factors(root_samples, 1.0);
  std::vector<double> factor_low(root_samples, 1.0);
  std::vector<double> factor_high(root_samples, 1.0);
  {
    std::vector<double> terms_buffer;
    for (U64 row = 0; row < root_samples; ++row) {
      double x = 0.0;
      double y = 0.0;
      sample_position(*root->footprints, row, x, y);
      photometry::basis_terms(solution->spatial_basis, spatial_mean, x, y, terms_buffer);
      double exponent = 0.0;
      double exponent_low = 0.0;
      double exponent_high = 0.0;
      for (std::size_t term = 0; term < terms; ++term) {
        exponent += theta[term] * terms_buffer[term];
        const double a = theta_lower[term] * terms_buffer[term];
        const double b = theta_upper[term] * terms_buffer[term];
        exponent_low += std::min(a, b);
        exponent_high += std::max(a, b);
      }
      factors[row] = std::exp(-exponent);
      factor_low[row] = std::exp(-exponent_high);
      factor_high[row] = std::exp(-exponent_low);
    }
  }

  // ---------------------------------------------------------------------
  // The signal: the factor acts on the root samples, the physical regrouping
  // afterwards.
  // ---------------------------------------------------------------------
  Result<std::vector<double>> root_values = read_f64_array(context, root->integrated_values, "values");
  if (!ok(root_values)) {
    return model::fail<Ref<NativeObservation>>(root_values.code, "the native values cannot be read",
                                               kOrigin, "the scale cannot be applied");
  }
  if (val(root_values).size() != root_samples) {
    return model::fail<Ref<NativeObservation>>(
        Code::invalid_model, "the native values do not match the native index", kOrigin,
        "the scale cannot be applied");
  }
  std::vector<double> scaled = val(root_values);
  for (U64 row = 0; row < root_samples; ++row) {
    scaled[row] *= factors[row];
  }

  // The transfer action of the whole chain: G_chain o D_alpha.
  Ref<LinearAction> transfer;
  {
    Result<Ref<LinearAction>> factor_action =
        make_diagonal_action(context, factors, root->native_index);
    if (!ok(factor_action)) {
      return model::fail<Ref<NativeObservation>>(factor_action.code,
                                                 "the spatial factor action cannot be built", kOrigin,
                                                 "the scale cannot be applied");
    }
    transfer = val(factor_action);
  }

  // The noise of every level: the root noise is scaled by the factor, and
  // every level above is transformed by its own regrouping.
  Ref<NoiseModel> noise;
  {
    Ref<NoiseModel> level_noise = root->noise;
    if (level_noise) {
      Result<Ref<LinearAction>> factor_action =
          make_diagonal_action(context, factors, root->native_index);
      if (!ok(factor_action)) {
        return model::fail<Ref<NativeObservation>>(factor_action.code,
                                                   "the spatial factor action cannot be built", kOrigin,
                                                   "the scale cannot be applied");
      }
      Result<Ref<NoiseModel>> scaled_noise =
          scale_noise(context, level_noise, val(factor_action));
      if (!ok(scaled_noise)) {
        return model::fail<Ref<NativeObservation>>(scaled_noise.code, "the noise chain cannot be scaled",
                                                   kOrigin, "the scale cannot be applied");
      }
      level_noise = val(scaled_noise);
    }
    for (std::size_t level = 1; level < chain.size(); ++level) {
      Ref<LinearAction> regrouping = chain[level]->regrouping;
      if (!regrouping) {
        return model::fail<Ref<NativeObservation>>(
            Code::invalid_model, "a regrouped level has no regrouping action", kOrigin,
            "the scale cannot be applied");
      }
      Result<std::vector<double>> mapped =
          apply_action(context, regrouping, scaled, "regrouping");
      if (!ok(mapped)) {
        return model::fail<Ref<NativeObservation>>(mapped.code, "the regrouping cannot be applied",
                                                   kOrigin, "the scale cannot be applied");
      }
      scaled = val(mapped);
      Result<Ref<LinearAction>> composed = compose(context, regrouping, transfer);
      if (!ok(composed)) {
        return model::fail<Ref<NativeObservation>>(composed.code, "the transfer cannot be composed",
                                                   kOrigin, "the scale cannot be applied");
      }
      transfer = val(composed);
      if (level_noise) {
        Result<Ref<NoiseModel>> transformed = scale_noise(context, level_noise, regrouping);
        if (!ok(transformed)) {
          return model::fail<Ref<NativeObservation>>(transformed.code,
                                                     "the noise chain cannot be transformed", kOrigin,
                                                     "the scale cannot be applied");
        }
        level_noise = val(transformed);
      }
    }
    noise = level_noise;
  }

  // ---------------------------------------------------------------------
  // The numerical enclosure of the scaled values.
  // ---------------------------------------------------------------------
  Result<std::vector<double>> root_low = read_f64_array(context, root->numerical_lower, "values");
  Result<std::vector<double>> root_high = read_f64_array(context, root->numerical_upper, "values");
  bool has_enclosure = ok(root_low) && ok(root_high) && val(root_low).size() == root_samples &&
                       val(root_high).size() == root_samples;

  // ---------------------------------------------------------------------
  // The new observation.
  // ---------------------------------------------------------------------
  const Id native_index_id =
      observation->native_index ? observation->native_index->identity.object_id : Id();
  Result<ArrayRef> values =
      copy_f64(context, scaled, {static_cast<U64>(scaled.size())}, observation->unit, native_index_id);
  if (!ok(values)) {
    return model::fail<Ref<NativeObservation>>(values.code, "the scaled values cannot be stored",
                                               kOrigin, "the scale cannot be applied");
  }
  auto scaled_observation = std::make_shared<NativeObservation>();
  scaled_observation->integrated_values = val(values);
  scaled_observation->dq = observation->dq;
  scaled_observation->native_index = observation->native_index;
  scaled_observation->footprints = observation->footprints;
  scaled_observation->regrouping = observation->regrouping;
  scaled_observation->scale_applications = observation->scale_applications;
  scaled_observation->scale_applications.push_back(solution->application_id);
  scaled_observation->noise = noise;
  scaled_observation->unit = observation->unit;
  scaled_observation->response = observation->response;
  scaled_observation->context = observation->context;
  scaled_observation->frame = observation->frame;
  scaled_observation->native_parent = observation->native_parent;
  scaled_observation->physical_collection = observation->physical_collection;

  // The numerical enclosure of the scaled values: the factor interval acts on
  // the enclosure of the parent values.
  if (has_enclosure) {
    std::vector<double> low(root_samples, 0.0);
    std::vector<double> high(root_samples, 0.0);
    for (U64 row = 0; row < root_samples; ++row) {
      const double a = (val(root_low))[row];
      const double b = (val(root_high))[row];
      const double products[4] = {a * factor_low[row], a * factor_high[row], b * factor_low[row],
                                  b * factor_high[row]};
      low[row] = *std::min_element(products, products + 4);
      high[row] = *std::max_element(products, products + 4);
    }
    // The regrouping of the levels is a linear map; the enclosure of the
    // intermediate levels is not reconstructed here, so the published
    // enclosure is the one of the root level and is marked conditional when a
    // regrouping is present.
    Result<ArrayRef> low_array = copy_f64(context, low, {static_cast<U64>(low.size())},
                                           observation->unit, native_index_id);
    Result<ArrayRef> high_array = copy_f64(context, high, {static_cast<U64>(high.size())},
                                            observation->unit, native_index_id);
    if (ok(low_array) && ok(high_array)) {
      scaled_observation->numerical_lower = val(low_array);
      scaled_observation->numerical_upper = val(high_array);
    }
  }

  // The kernel: the new effective PSF carries the source end amplitude and
  // the transfer that applies the spatial factor to the template.
  if (observation->kernel) {
    auto kernel = std::make_shared<EffectivePSF>();
    kernel->family = observation->kernel->family;
    kernel->parameters = observation->kernel->parameters;
    kernel->spatial_domain = observation->kernel->spatial_domain;
    kernel->wavelength_nm = observation->kernel->wavelength_nm;
    kernel->tail_mass_upper = observation->kernel->tail_mass_upper;
    kernel->spatial_basis = observation->kernel->spatial_basis;
    kernel->achromatic = observation->kernel->achromatic;
    kernel->detector_samples = observation->kernel->detector_samples;
    kernel->influence = observation->kernel->influence;
    kernel->parameter_lower = observation->kernel->parameter_lower;
    kernel->parameter_upper = observation->kernel->parameter_upper;
    kernel->error = observation->kernel->error;
    kernel->source_scale = solution;
    kernel->frame = frame;
    kernel->regroup_scale_project = transfer;
    scaled_observation->kernel = kernel;
  }

  // The error record of the scaled observation.
  {
    auto error = std::make_shared<ErrorRecord>();
    error->linearization_selection = solution->assessment.condition;
    if (solution->error) {
      error->unknown_model_terms = solution->error->unknown_model_terms;
    }
    if (!has_enclosure) {
      error->unknown_model_terms.push_back(
          "the parent observation has no numerical enclosure; the scaled values carry no "
          "unconditional numerical certificate");
    }
    if (!has_parameter_box) {
      error->unknown_model_terms.push_back(
          "the photometric solution has no certified parameter box; the numerical enclosure of the "
          "scaled values is conditional on the nominal parameters");
    }
    if (chain.size() > 1) {
      error->unknown_model_terms.push_back(
          "the numerical enclosure of the regrouped levels was not reconstructed from the parent "
          "enclosures");
    }
    scaled_observation->error = error;
  }

  model::Sha256 hasher;
  hasher.field("application", solution->application_id);
  hasher.field("frame", frame);
  hasher.field("observation", observation->identity.content_hash);
  for (double value : factors) {
    hasher.update_f64(value);
  }
  scaled_observation->identity = model::make_identity("acsd.NativeObservation", 2, 0,
                                                       model::content_id(hasher.hex()), "acsd");
  scaled_observation->assessment = model::make_assessment(Code::ok);
  scaled_observation->assessment.complete = has_enclosure && has_parameter_box;
  scaled_observation->assessment.execution = ExecutionState::complete;
  scaled_observation->assessment.science =
      (has_enclosure && has_parameter_box) ? ScienceState::bounded : ScienceState::conditional;
  scaled_observation->assessment.condition =
      model::make_condition("scaled_native_observation", "photometric_scale");
  model::set_included(scaled_observation->assessment, "per_native_sample_spatial_factor");
  model::set_included(scaled_observation->assessment, "physical_regrouping_after_the_factor");
  model::set_included(scaled_observation->assessment, "noise_chain_scaled_with_the_same_factor");
  if (!has_enclosure || !has_parameter_box) {
    model::add_issue(scaled_observation->assessment, Code::unavailable,
                     "the scaled values have no unconditional numerical certificate", kOrigin,
                     "the scale is conditional", "provide a parent enclosure and a certified parameter box");
  }
  return model::ok_result(std::shared_ptr<const NativeObservation>(scaled_observation));
}

}  // namespace acsd
