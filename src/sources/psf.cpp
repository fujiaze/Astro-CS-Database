// fit_psf and evaluate_psf: the PSF model of the sources module.
//
// evaluate_psf is the only pixel integral implementation: it integrates the
// native positive density over the real collection footprint of the samples
// that the transfer actually needs, and it never renormalises a finite window
// to the total flux.  The finite window only loses mass, and the loss is
// reported as the native tail bound of the positive model.
//
// fit_psf fits the shape parameters as a polynomial of degree at most two in
// the normalised detector coordinates, together with the continuous position
// and the total flux of every source and a constant background.  The non
// linear step is a Gauss-Newton step with an Armijo backtracking line search
// (initial step one, halving, at most thirty two backtracking steps); the
// iteration count, the gradient and the step tolerance are explicit.  A rank
// deficient design is reported as not identifiable instead of being silently
// reduced.
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"
#include "sources/psf_internal.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "evaluate_psf";
constexpr U32 kMaxSourcePositions = 4096;

Index make_sub_index(U64 size, const Id& namespace_id, const std::vector<SourceIdentity>& sources) {
  Index index;
  index.size = size;
  index.kind = Index::Kind::native;
  index.namespace_id = namespace_id;
  if (size <= 4096) index.sources = sources;
  index.identity.object_id = "index:" + namespace_id + ":" + std::to_string(size);
  index.identity.content_hash = "sha256:" + std::string(64, '0');
  index.identity.schema_id = "acsd.Index";
  index.identity.schema_major = 2;
  index.identity.schema_minor = 0;
  index.assessment.complete = true;
  index.assessment.execution = ExecutionState::complete;
  return index;
}

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is missing", kOrigin,
                                            "the array cannot be read");
  }
  // The block request is a flattened element range, so the count is every
  // element of the array and not the first dimension: a [rows, sources]
  // template is read whole.
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  if (array->shape.empty()) count = 0;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the array cannot be read", kOrigin,
                                            "the array cannot be read");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<ArrayRef> write_f64_array(Context& context, const std::vector<double>& values,
                                  std::span<const U64> shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

// Applies an action to a plain vector and returns the result.
Result<std::vector<double>> apply_vector(Context& context, Ref<LinearAction> action,
                                          const std::vector<double>& values, const Id& index_id,
                                          const Unit& unit) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  Result<ArrayRef> input = write_f64_array(context, values, shape, unit, index_id);
  if (!ok(input)) {
    return model::fail<std::vector<double>>(input.code, "the input cannot be stored", kOrigin,
                                            "the action cannot be applied");
  }
  Result<ArrayRef> output = apply(context, action, val(input));
  if (!ok(output)) {
    return model::fail<std::vector<double>>(output.code, "the action cannot be applied", kOrigin,
                                            "the action cannot be applied");
  }
  return read_f64_array(context, val(output));
}

// The absolute value of a dyadic endpoint.
//
// The conversion of a negative rational in the base walks towards positive
// infinity instead of downwards, so the magnitude is converted and the sign
// is taken from the decimal representation.  The defect is in the model
// module and is reported there; the workaround keeps the row enclosure of a
// signed transfer usable.
double absolute_dyadic(const acsd::Dyadic& value) {
  acsd::Dyadic magnitude = value;
  if (!magnitude.signed_integer.empty() && magnitude.signed_integer.front() == '-') {
    magnitude.signed_integer.erase(0, 1);
  }
  return std::fabs(model::dyadic_to_double(magnitude));
}

// The absolute coefficient bound of one output row, when the action exposes
// it.  The result is empty when only a signed row is available.
Result<std::vector<double>> absolute_row(Context& context, Ref<LinearAction> action, U64 row) {
  std::vector<double> bound;
  Result<std::vector<ExactInterval>> enclosure = action->row_enclosure(context, row);
  if (!ok(enclosure)) return model::ok_result(bound);
  for (const ExactInterval& interval : val(enclosure)) {
    bound.push_back(std::max(absolute_dyadic(interval.lower), absolute_dyadic(interval.upper)));
  }
  return model::ok_result(std::move(bound));
}

}  // namespace

Result<Ref<PsfEvaluation>> evaluate_psf(Context& context, Ref<EffectivePSF> psf,
                                       const PsfEvaluationRequest& request) {
  if (!psf) {
    return model::fail<Ref<PsfEvaluation>>(Code::invalid_request, "no PSF supplied", kOrigin,
                                           "the evaluation cannot be built");
  }
  if (!psf->detector_samples || !psf->detector_samples->index) {
    return model::fail<Ref<PsfEvaluation>>(Code::invalid_model,
                                           "the PSF has no native sample footprint", kOrigin,
                                           "the evaluation cannot be built");
  }
  if (request.source_positions.empty()) {
    return model::fail<Ref<PsfEvaluation>>(Code::invalid_request, "no source position requested",
                                           kOrigin, "the evaluation cannot be built");
  }
  if (request.source_positions.size() > kMaxSourcePositions) {
    return model::fail<Ref<PsfEvaluation>>(Code::invalid_request,
                                           "too many source positions in one request", kOrigin,
                                           "the evaluation cannot be built");
  }

  const FootprintSet& footprints = *psf->detector_samples;
  const Index& native_index = *footprints.index;
  const U64 native_count = native_index.size;

  // The parameters of the family: the polynomial coefficients in the
  // transformed parameter space.
  Result<std::vector<double>> parameters = read_f64_array(context, psf->parameters);
  if (!ok(parameters)) {
    return model::fail<Ref<PsfEvaluation>>(parameters.code, "the PSF parameters cannot be read",
                                           kOrigin, "the evaluation cannot be built");
  }
  const std::size_t family_count = psf->family == PsfRequest::Family::gaussian
                                       ? 3
                                       : (psf->family == PsfRequest::Family::moffat ? 4 : 1);
  if (family_count == 0 || val(parameters).empty() ||
      val(parameters).size() % family_count != 0) {
    return model::fail<Ref<PsfEvaluation>>(Code::invalid_model,
                                           "the PSF parameters do not match the family", kOrigin,
                                           "the evaluation cannot be built");
  }
  const std::size_t term_count = val(parameters).size() / family_count;
  const std::size_t expected_terms =
      psf->spatial_basis.degree == 0 ? 1 : (psf->spatial_basis.degree == 1 ? 3 : 6);
  if (term_count != expected_terms) {
    return model::fail<Ref<PsfEvaluation>>(
        Code::invalid_model, "the PSF parameters do not match the spatial basis degree", kOrigin,
        "the evaluation cannot be built");
  }

  // The transfer from the native samples to the current output samples.
  Ref<LinearAction> transfer = psf->regroup_scale_project;
  Ref<Index> output_base = footprints.index;
  U64 output_count = native_count;
  bool identity_transfer = true;
  if (transfer) {
    // The declared domain of the transfer is the public member of the action.
    const DomainUnits& spaces = transfer->spaces;
    // The kind of the transfer is only described by its recipe; an identity
    // transfer is the one that maps the samples onto themselves.
    Result<ActionRecipe> transfer_recipe = transfer->serialize_recipe();
    const bool identity_kind =
        ok(transfer_recipe) && val(transfer_recipe).kind == ActionRecipe::Kind::identity;
    if (!spaces.domain || !spaces.codomain) {
      return model::fail<Ref<PsfEvaluation>>(Code::invalid_model,
                                             "the transfer has no declared domain", kOrigin,
                                             "the evaluation cannot be built");
    }
    if (spaces.domain->size != native_count) {
      return model::fail<Ref<PsfEvaluation>>(
          Code::invalid_model, "the transfer does not act on the native samples", kOrigin,
          "the evaluation cannot be built");
    }
    output_base = spaces.codomain;
    output_count = spaces.codomain->size;
    identity_transfer = identity_kind || spaces.domain == spaces.codomain;
  }

  // The requested output rows.
  std::vector<U64> rows;
  if (request.output_rows.empty()) {
    rows.resize(static_cast<std::size_t>(output_count));
    for (U64 i = 0; i < output_count; ++i) rows[static_cast<std::size_t>(i)] = i;
  } else {
    U64 previous = 0;
    bool first = true;
    for (U64 row : request.output_rows) {
      if (row >= output_count) {
        return model::fail<Ref<PsfEvaluation>>(
            Code::invalid_request, "a requested output row is outside the transfer output", kOrigin,
            "the evaluation cannot be built");
      }
      if (!first && row <= previous) {
        return model::fail<Ref<PsfEvaluation>>(
            Code::invalid_request, "the requested output rows are not strictly increasing", kOrigin,
            "the evaluation cannot be built");
      }
      previous = row;
      first = false;
      rows.push_back(row);
    }
  }

  // The native samples that the requested output rows actually need.  A small
  // window must not ask for the whole frame.
  std::vector<U64> needed;
  if (identity_transfer) {
    needed = rows;
  } else {
    // The dependency scan only visits the native samples that can carry the
    // mass of the requested sources; the mass outside them is bounded by the
    // analytic tail of the positive model.
    std::vector<std::vector<double>> locals;
    for (const PixelCenter0& position : request.source_positions) {
      const std::vector<double> basis_vector = sources::psfmodel::basis_values(
          psf->spatial_basis, psf->spatial_basis.degree, position.x, position.y);
      locals.push_back(sources::psfmodel::local_parameters(psf->family, val(parameters), basis_vector));
    }
    const double radius = sources::psfmodel::support_radius(psf->family, locals);
    const std::vector<U64> candidates =
        sources::psfmodel::candidate_rows(footprints, request.source_positions, radius);
    Result<U64> work = context.resources.consume_work(candidates.size() + 1);
    if (!ok(work)) {
      return model::fail<Ref<PsfEvaluation>>(
          work.code, "the transfer dependency scan exceeds the work budget", kOrigin,
          "the evaluation cannot be built");
    }
    std::vector<double> unit(native_count, 0.0);
    for (U64 j : candidates) {
      std::fill(unit.begin(), unit.end(), 0.0);
      unit[j] = 1.0;
      Result<std::vector<double>> column =
          apply_vector(context, transfer, unit, native_index.identity.object_id, model::unit_dimensionless());
      if (!ok(column)) {
        return model::fail<Ref<PsfEvaluation>>(column.code,
                                               "the transfer cannot be applied", kOrigin,
                                               "the evaluation cannot be built");
      }
      if (val(column).size() != output_count) {
        return model::fail<Ref<PsfEvaluation>>(
            Code::invalid_model, "the transfer does not produce the declared output samples", kOrigin,
            "the evaluation cannot be built");
      }
      for (U64 row : rows) {
        if (val(column)[static_cast<std::size_t>(row)] != 0.0) {
          needed.push_back(j);
          break;
        }
      }
    }
  }

  // The pixel integral of the native positive model over the needed samples.
  const double tolerance =
      request.budget.tolerance.absolute > 0.0 ? request.budget.tolerance.absolute : 1.0e-12;
  Result<sources::psfmodel::TemplateBlock> block =
      sources::psfmodel::build_templates(context, psf->family, val(parameters), psf->spatial_basis,
                           request.source_positions, footprints, needed, tolerance);
  if (!ok(block)) {
    // The reason of the builder is kept: a model outside of its valid domain
    // is a domain failure and not a generic failure.
    return model::fail<Ref<PsfEvaluation>>(
        block.code,
        block.assessment.issues.empty() ? "the pixel integral cannot be built"
                                        : block.assessment.issues.front().reason,
        kOrigin, "the evaluation cannot be built");
  }
  const sources::psfmodel::TemplateBlock& template_block = val(block);
  const std::size_t sources = request.source_positions.size();

  // The output template and its numerical enclosure.
  std::vector<double> values(rows.size() * sources, 0.0);
  std::vector<double> lower(rows.size() * sources, 0.0);
  std::vector<double> upper(rows.size() * sources, 0.0);
  std::vector<double> tails(sources, 0.0);
  bool signed_transfer = false;

  for (std::size_t s = 0; s < sources; ++s) {
    tails[s] = template_block.tails[s];
    if (identity_transfer) {
      for (std::size_t k = 0; k < needed.size(); ++k) {
        const auto found = std::find(rows.begin(), rows.end(), needed[k]);
        if (found == rows.end()) continue;
        const std::size_t r = static_cast<std::size_t>(found - rows.begin());
        const double value = template_block.values[k * sources + s];
        const double error = template_block.errors[k * sources + s];
        values[r * sources + s] = value;
        lower[r * sources + s] = value - error;
        upper[r * sources + s] = value + error;
      }
      continue;
    }
    // The transfer is applied to the native template; the omitted native
    // samples contribute nothing because they are outside the dependency of
    // the requested rows.
    std::vector<double> native_template(native_count, 0.0);
    std::vector<double> native_error(native_count, 0.0);
    for (std::size_t k = 0; k < needed.size(); ++k) {
      native_template[static_cast<std::size_t>(needed[k])] = template_block.values[k * sources + s];
      native_error[static_cast<std::size_t>(needed[k])] = template_block.errors[k * sources + s];
    }
    Result<std::vector<double>> mapped = apply_vector(context, transfer, native_template,
                                                       native_index.identity.object_id, model::unit_dimensionless());
    if (!ok(mapped)) {
      return model::fail<Ref<PsfEvaluation>>(mapped.code, "the transfer cannot be applied", kOrigin,
                                             "the evaluation cannot be built");
    }
    Result<std::vector<double>> mapped_error = apply_vector(context, transfer, native_error,
                                                            native_index.identity.object_id,
                                                            model::unit_dimensionless());
    if (!ok(mapped_error)) {
      return model::fail<Ref<PsfEvaluation>>(mapped_error.code, "the transfer cannot be applied",
                                             kOrigin, "the evaluation cannot be built");
    }
    for (std::size_t r = 0; r < rows.size(); ++r) {
      const double value = val(mapped)[static_cast<std::size_t>(rows[r])];
      double error = std::fabs(val(mapped_error)[static_cast<std::size_t>(rows[r])]);
      Result<std::vector<double>> bound = absolute_row(context, transfer, rows[r]);
      if (ok(bound) && !val(bound).empty()) {
        // The absolute coefficient bound gives the rigorous radius.
        double rigorous = 0.0;
        for (std::size_t k = 0; k < needed.size(); ++k) {
          const U64 column = needed[k];
          if (column < val(bound).size()) {
            rigorous += val(bound)[static_cast<std::size_t>(column)] *
                       template_block.errors[k * sources + s];
          }
        }
        error = rigorous;
      }
      // A transfer that is not the identity combines the native samples with
      // signs.  The row enclosure of the action is an absolute coefficient
      // bound, so the radius of the output row is a rigorous absolute bound
      // but not the interval of the row.
      if (!identity_transfer) {
        signed_transfer = true;
      }
      values[r * sources + s] = value;
      lower[r * sources + s] = value - error;
      upper[r * sources + s] = value + error;
    }
  }

  // The output index is the exact sub index of the requested rows.
  std::vector<SourceIdentity> selected;
  if (rows.size() <= 4096 && output_base->sources.size() == output_base->size) {
    for (U64 row : rows) selected.push_back(output_base->sources[static_cast<std::size_t>(row)]);
  }
  auto output_index = std::make_shared<Index>(
      make_sub_index(rows.size(), output_base->namespace_id, selected));
  auto source_index = std::make_shared<Index>(make_sub_index(sources, "psf-sources", {}));

  const std::vector<U64> shape{static_cast<U64>(rows.size()), static_cast<U64>(sources)};
  const std::vector<U64> tail_shape{static_cast<U64>(sources)};
  const Id output_index_id = output_index->identity.object_id;
  const Id source_index_id = source_index->identity.object_id;
  Result<ArrayRef> value_array =
      write_f64_array(context, values, shape, model::unit_dimensionless(), output_index_id);
  if (!ok(value_array)) {
    return model::fail<Ref<PsfEvaluation>>(value_array.code, "the template cannot be stored", kOrigin,
                                           "the evaluation cannot be built");
  }
  Result<ArrayRef> lower_array =
      write_f64_array(context, lower, shape, model::unit_dimensionless(), output_index_id);
  if (!ok(lower_array)) {
    return model::fail<Ref<PsfEvaluation>>(lower_array.code,
                                           "the template bounds cannot be stored", kOrigin,
                                           "the evaluation cannot be built");
  }
  Result<ArrayRef> upper_array =
      write_f64_array(context, upper, shape, model::unit_dimensionless(), output_index_id);
  if (!ok(upper_array)) {
    return model::fail<Ref<PsfEvaluation>>(upper_array.code,
                                           "the template bounds cannot be stored", kOrigin,
                                           "the evaluation cannot be built");
  }
  Result<ArrayRef> tail_array =
      write_f64_array(context, tails, tail_shape, model::unit_dimensionless(), source_index_id);
  if (!ok(tail_array)) {
    return model::fail<Ref<PsfEvaluation>>(tail_array.code, "the tail bound cannot be stored",
                                           kOrigin, "the evaluation cannot be built");
  }

  auto error = std::make_shared<ErrorRecord>();
  error->linearization_selection =
      model::make_condition("psf_pixel_integral", "deterministic_quadrature");
  error->unknown_model_terms.push_back("the_real_psf_shape_deviation_is_not_a_numerical_error");
  if (signed_transfer) {
    error->unknown_model_terms.push_back("transfer_absolute_bound_missing");
  }
  error->identity = model::make_identity("acsd.ErrorRecord", 2, 0,
                                         model::sha256_hex("evaluate_psf"), "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto evaluation = std::make_shared<PsfEvaluation>();
  evaluation->psf = psf;
  evaluation->source_positions = request.source_positions;
  evaluation->values = val(value_array);
  evaluation->numerical_lower = val(lower_array);
  evaluation->numerical_upper = val(upper_array);
  evaluation->native_tail_upper = val(tail_array);
  evaluation->error = std::shared_ptr<const ErrorRecord>(error);
  evaluation->output_index = output_index;
  evaluation->source_index = source_index;
  evaluation->assessment = model::make_assessment(Code::ok);
  evaluation->assessment.complete = true;
  evaluation->assessment.validity = signed_transfer ? Validity::approximate : Validity::certified;
  model::set_included(evaluation->assessment, "psf_pixel_integral");
  model::set_omitted(evaluation->assessment, "the_window_is_not_renormalised_to_the_total_flux");
  if (signed_transfer) {
    // The reason is specific: the action exposes an absolute coefficient
    // bound, so the radius of the output row is an absolute bound.  A per row
    // interval needs the signed interval propagation of the transfer, which
    // the action does not expose.
    model::add_issue(
        evaluation->assessment, Code::unavailable,
        "the transfer is not the identity: the row enclosure of the action is an absolute "
        "coefficient bound, so the radius of the output row is an absolute bound and not the "
        "interval of the row; a per row interval needs the signed interval propagation of the "
        "transfer, which the action does not expose",
        kOrigin, "the numerical enclosure of the output row is conditional");
    model::set_omitted(evaluation->assessment, "the_per_row_interval_of_a_signed_transfer_is_not_available");
    evaluation->assessment.science = ScienceState::conditional;
  }
  evaluation->identity = model::make_identity(
      "acsd.PsfEvaluation", 2, 0,
      model::sha256_hex(std::to_string(rows.size()) + ":" + std::to_string(sources)), "acsd");

  return model::ok_result(std::shared_ptr<const PsfEvaluation>(evaluation));
}

}  // namespace acsd
