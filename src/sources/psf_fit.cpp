// fit_psf: the PSF shape fit of the sources module.
//
// The shape parameters are fitted as a polynomial of degree at most two in the
// normalised detector coordinates.  Every source contributes its continuous
// position and its total flux; a constant background absorbs the sky.  The
// non linear step is a Gauss-Newton step with an Armijo backtracking line
// search.  A rank deficient design is reported as not identifiable; the
// implementation never reduces the requested degree behind the user's back.
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
#include "observation/observation.hpp"
#include "sources/psf_internal.hpp"

namespace acsd {
namespace {

constexpr const char* kOrigin = "fit_psf";

using numerics::Matrix;

Result<std::vector<double>> read_f64_array(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is missing", kOrigin,
                                            "the array cannot be read");
  }
  const U64 count = array->shape.empty() ? 0 : array->shape[0];
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

Index make_parameter_index(U64 size) {
  Index index;
  index.size = size;
  index.kind = Index::Kind::parameter;
  index.namespace_id = "psf-parameters";
  index.identity.object_id = "index:psf-parameters:" + std::to_string(size);
  index.identity.content_hash = "sha256:" + std::string(64, '0');
  index.identity.schema_id = "acsd.Index";
  index.identity.schema_major = 2;
  index.identity.schema_minor = 0;
  index.assessment.complete = true;
  index.assessment.execution = ExecutionState::complete;
  return index;
}

}  // namespace

// The fit of the PSF shape.
Result<Ref<EffectivePSF>> fit_psf(Context& context, Ref<NativeObservation> observation,
                                  Ref<WcsModel> wcs, std::span<const CatalogueStar> stars,
                                  const PsfRequest& request) {
  if (!observation || !observation->footprints || !observation->footprints->index) {
    return model::fail<Ref<EffectivePSF>>(Code::invalid_request,
                                          "the observation has no native footprint", kOrigin,
                                          "the fit cannot be built");
  }
  if (!wcs) {
    return model::fail<Ref<EffectivePSF>>(Code::invalid_request, "no frame model supplied", kOrigin,
                                          "the fit cannot be built");
  }
  if (stars.empty()) {
    return model::fail<Ref<EffectivePSF>>(Code::invalid_request, "no catalogue star supplied", kOrigin,
                                          "the fit cannot be built");
  }
  if (request.spatial_degree > 2) {
    return model::fail<Ref<EffectivePSF>>(Code::invalid_request,
                                          "the first release supports a spatial degree of at most two",
                                          kOrigin, "the fit cannot be built");
  }
  if (!(request.initial_width_pixel > 0.0)) {
    return model::fail<Ref<EffectivePSF>>(Code::invalid_request,
                                          "the initial width must be positive", kOrigin,
                                          "the fit cannot be built");
  }
  const FootprintSet& footprints = *observation->footprints;
  if (wcs->detector_frame != footprints.detector_frame) {
    return model::fail<Ref<EffectivePSF>>(
        Code::invalid_model, "the frame model and the footprint set use different detectors", kOrigin,
        "the fit cannot be built");
  }

  // The detector positions of the catalogue stars.
  std::vector<PixelCenter0> positions;
  std::vector<U64> source_ids;
  for (const CatalogueStar& star : stars) {
    // The catalogue position has to be at the observation epoch.  The
    // catalogue module owns the propagation (the pmRA*cosDec convention, the
    // parallax and the radial velocity); the PSF path uses the position that
    // the catalogue provider delivered for the requested epoch.  A star that
    // is not at the observation epoch and carries a proper motion is refused
    // instead of being propagated a second time.
    const double epoch_gap = std::fabs(observation->context.epoch_julian_year - star.epoch_year);
    if (epoch_gap > 1.0e-6 &&
        (star.pmra_cosdec_rad_year != 0.0 || star.pmdec_rad_year != 0.0)) {
      return model::fail<Ref<EffectivePSF>>(
          Code::invalid_model,
          "the catalogue position is not at the observation epoch; the catalogue provider has to "
          "deliver the position of the requested epoch (the pmRA*cosDec propagation with the "
          "parallax and the radial velocity belongs to the catalogue module)",
          kOrigin, "the fit cannot be built");
    }
    Result<PixelCenter0> position = sources::psfmodel::detector_position(context, wcs, star, observation->context,
                                                             request.budget);
    if (!ok(position)) continue;
    const PixelCenter0& point = val(position);
    if (point.x < footprints.local_cell.lower[0] - 1.0 ||
        point.y < footprints.local_cell.lower[1] - 1.0) continue;
    positions.push_back(point);
    source_ids.push_back(star.source_id);
  }
  if (positions.empty()) {
    return model::fail<Ref<EffectivePSF>>(Code::unavailable,
                                          "no catalogue star falls on the frame", kOrigin,
                                          "the fit cannot be built");
  }

  // The native data.
  Result<std::vector<double>> data = read_f64_array(context, observation->integrated_values);
  if (!ok(data)) {
    return model::fail<Ref<EffectivePSF>>(data.code, "the native values cannot be read", kOrigin,
                                          "the fit cannot be built");
  }
  Result<std::vector<std::uint16_t>> flags =
      observation::read_u16_values(context, observation->dq);
  std::vector<std::uint16_t> quality;
  if (ok(flags)) quality = val(flags);

  const U64 native_count = footprints.index->size;
  const std::size_t family_count = sources::psfmodel::family_parameter_count(request.family);
  const std::size_t term_count = sources::psfmodel::basis_term_count(request.spatial_degree);
  const std::size_t sources = positions.size();

  // The spatial basis: the normalised detector domain.
  const double domain_x0 = -0.5;
  const double domain_x1 = static_cast<double>(footprints.width) - 0.5;
  const double domain_y0 = -0.5;
  const double domain_y1 = static_cast<double>(footprints.height) - 0.5;
  Basis basis;
  basis.kind = request.spatial_degree == 0 ? Basis::Kind::constant : Basis::Kind::polynomial;
  basis.degree = request.spatial_degree;
  basis.normalization_domain.lower = {domain_x0, domain_y0};
  basis.normalization_domain.upper = {domain_x1, domain_y1};
  basis.center = {0.5 * (domain_x0 + domain_x1), 0.5 * (domain_y0 + domain_y1)};
  basis.scale = {0.5 * (domain_x1 - domain_x0), 0.5 * (domain_y1 - domain_y0)};

  // The window radius: the analytic tail bound of the positive model decides
  // how much of the profile has to be integrated.  For a Gaussian the mass
  // beyond the elliptical radius R is exp(-R^2/(2 sigma^2)), so the radius
  // that keeps the tail below the requested bound follows from the width.
  double window_radius = request.window_radius_pixel;
  const double tail_target =
      request.tail_absolute_flux > 0.0 && request.tail_absolute_flux < 1.0
          ? request.tail_absolute_flux
          : 1.0e-6;
  const double radius_from_tail =
      request.initial_width_pixel * std::sqrt(2.0 * std::log(1.0 / tail_target));
  if (window_radius > 0.0) {
    window_radius = std::max(window_radius, radius_from_tail);
  } else {
    window_radius = radius_from_tail;
  }

  // The fit samples: the native samples inside the window of any source.
  std::vector<U64> fit_rows;
  {
    std::vector<bool> selected(static_cast<std::size_t>(native_count), false);
    for (const PixelCenter0& point : positions) {
      const double half = window_radius;
      const double x0 = std::max(domain_x0, point.x - half);
      const double x1 = std::min(domain_x1, point.x + half);
      const double y0 = std::max(domain_y0, point.y - half);
      const double y1 = std::min(domain_y1, point.y + half);
      for (U64 row = 0; row < native_count; ++row) {
        if (selected[static_cast<std::size_t>(row)]) continue;
        const sources::psfmodel::SampleFootprint footprint = sources::psfmodel::sample_footprint(footprints, row);
        if (!footprint.valid) continue;
        if (footprint.x1 < x0 || footprint.x0 > x1) continue;
        if (footprint.y1 < y0 || footprint.y0 > y1) continue;
        selected[static_cast<std::size_t>(row)] = true;
        fit_rows.push_back(row);
      }
    }
  }
  if (fit_rows.empty()) {
    return model::fail<Ref<EffectivePSF>>(Code::no_support,
                                          "no native sample falls into the fit window", kOrigin,
                                          "the fit cannot be built");
  }
  std::sort(fit_rows.begin(), fit_rows.end());

  // The parameter vector: the shape polynomial, the source positions and
  // fluxes, and the background.
  const std::size_t shape_count = family_count * term_count;
  const std::size_t source_block = 3 * sources;
  const std::size_t parameter_count = shape_count + source_block + 1;
  std::vector<double> theta(parameter_count, 0.0);
  {
    const double width = std::log(request.initial_width_pixel);
    for (std::size_t r = 0; r < family_count; ++r) {
      double initial = 0.0;
      if (request.family == PsfRequest::Family::gaussian) {
        initial = (r == 0 || r == 1) ? width : 0.0;
      } else if (request.family == PsfRequest::Family::moffat) {
        initial = (r == 0 || r == 1) ? width : (r == 3 ? std::log(2.0) : 0.0);
      } else {
        initial = width;
      }
      theta[r * term_count] = initial;
    }
  }
  for (std::size_t s = 0; s < sources; ++s) {
    theta[shape_count + 3 * s] = positions[s].x;
    theta[shape_count + 3 * s + 1] = positions[s].y;
    theta[shape_count + 3 * s + 2] = 0.0;
  }
  {
    // The background starts at the median of the fit samples.
    std::vector<double> window_values;
    for (U64 row : fit_rows) {
      if (row < val(data).size() && std::isfinite(val(data)[static_cast<std::size_t>(row)])) {
        window_values.push_back(val(data)[static_cast<std::size_t>(row)]);
      }
    }
    std::sort(window_values.begin(), window_values.end());
    theta[parameter_count - 1] =
        window_values.empty() ? 0.0 : window_values[window_values.size() / 2];
  }

  // The shape coefficients of a parameter vector.
  auto shape_coefficients = [&](const std::vector<double>& parameters) {
    return std::vector<double>(parameters.begin(), parameters.begin() + static_cast<long>(shape_count));
  };
  auto local_of = [&](const std::vector<double>& parameters, const PixelCenter0& at, std::size_t s) {
    const std::vector<double> basis_vector =
        sources::psfmodel::basis_values(basis, request.spatial_degree, at.x, at.y);
    (void)s;
    return sources::psfmodel::local_parameters(request.family, shape_coefficients(parameters), basis_vector);
  };

  // The initial flux follows from the window sum: the total of the window
  // above the background divided by the collected template.
  {
    Result<sources::psfmodel::TemplateBlock> initial = sources::psfmodel::build_templates(
        context, request.family, shape_coefficients(theta), basis, positions, footprints, fit_rows,
        1.0e-10);
    if (ok(initial)) {
      const double background = theta[parameter_count - 1];
      for (std::size_t s = 0; s < sources; ++s) {
        double signal = 0.0;
        double template_sum = 0.0;
        for (std::size_t k = 0; k < fit_rows.size(); ++k) {
          const U64 row = fit_rows[k];
          if (row < val(data).size() && std::isfinite(val(data)[static_cast<std::size_t>(row)])) {
            signal += val(data)[static_cast<std::size_t>(row)] - background;
          }
          template_sum += val(initial).values[k * sources + s];
        }
        if (template_sum > 0.0) theta[shape_count + 3 * s + 2] = signal / template_sum;
      }
    }
  }

  const double tolerance =
      request.budget.tolerance.absolute > 0.0 ? request.budget.tolerance.absolute : 1.0e-10;
  // The stopping thresholds of EXECUTION.md: the score of the scaled columns
  // and the predicted step, both in the natural units of the parameters.  The
  // defaults of the specification are used when the budget names no absolute
  // tolerance.
  const double gradient_tolerance =
      request.budget.tolerance.absolute > 0.0 ? request.budget.tolerance.absolute : 1.0e-8;
  const double step_tolerance =
      request.budget.tolerance.absolute > 0.0 ? request.budget.tolerance.absolute : 1.0e-10;
  // The radius of the trust region in the natural units of the parameters.
  const double trust_radius = 1.0;
  const U32 max_iterations = request.budget.max_iterations > 0 ? request.budget.max_iterations : 100;

  double objective = 0.0;
  double gradient = 0.0;
  U32 iterations = 0;
  bool converged = false;
  bool local_minimum = false;
  bool zero_step_convergence = false;
  bool precision_exhausted = false;
  bool trust_region_applied = false;
  bool domain_constrained = false;
  bool damped_step_used = false;
  double damping_used = 0.0;
  std::string failure;
  numerics::SvdResult svd;
  std::vector<double> step(parameter_count, 0.0);

  for (U32 iteration = 0; iteration < max_iterations; ++iteration) {
    // The cancellation is checked in every iteration of the fit; a cancelled
    // fit returns no kernel at all.
    if (context.cancellation.requested()) {
      return model::fail<Ref<EffectivePSF>>(Code::cancelled, "cancelled", kOrigin,
                                            "the PSF fit was cancelled");
    }
    iterations = iteration + 1;
    const std::size_t rows = fit_rows.size();
    // The current positions of the sources are part of the model.
    std::vector<PixelCenter0> current_positions = positions;
    for (std::size_t s = 0; s < sources; ++s) {
      current_positions[s].x = theta[shape_count + 3 * s];
      current_positions[s].y = theta[shape_count + 3 * s + 1];
    }
    // The nominal template.
    Result<sources::psfmodel::TemplateBlock> nominal = sources::psfmodel::build_templates(
        context, request.family, shape_coefficients(theta), basis, current_positions, footprints,
        fit_rows, tolerance);
    if (!ok(nominal)) {
      // The reason of the builder is kept: a model outside of its valid
      // domain is a domain failure, not an unidentified design.
      return model::fail<Ref<EffectivePSF>>(nominal.code,
                                            nominal.assessment.issues.empty()
                                                ? "the pixel integral cannot be built"
                                                : nominal.assessment.issues.front().reason,
                                            kOrigin, "the fit cannot be built");
    }
    const sources::psfmodel::TemplateBlock& block = val(nominal);
    // The residuals.
    std::vector<double> residuals(rows, 0.0);
    for (std::size_t k = 0; k < rows; ++k) {
      const U64 row = fit_rows[k];
      double model = theta[parameter_count - 1];
      for (std::size_t s = 0; s < sources; ++s) {
        model += theta[shape_count + 3 * s + 2] * block.values[k * sources + s];
      }
      const double measured =
          row < val(data).size() ? val(data)[static_cast<std::size_t>(row)] : 0.0;
      residuals[k] = measured - model;
    }
    objective = 0.0;
    for (double r : residuals) objective += 0.5 * r * r;

    // The model has to have support on the fit samples.  A template without
    // any mass is a degenerate model, and the rank of such a design says
    // nothing about the estimability of the parameters.
    {
      double mass = 0.0;
      for (double value : block.values) mass += std::fabs(value);
      if (!(mass > 0.0)) {
        return model::fail<Ref<EffectivePSF>>(
            Code::nonconverged,
            "the model has no support on the fit samples; the design matrix of a degenerate model "
            "cannot be used to judge the estimability of the parameters",
            kOrigin, "the fit cannot be built");
      }
    }
    // The Jacobian.
    // The Jacobian carries the derivative of the model, so the Gauss-Newton
    // step solves J delta = residual.
    Matrix jacobian(rows, parameter_count);
    for (std::size_t k = 0; k < rows; ++k) {
      jacobian(k, parameter_count - 1) = 1.0;
      for (std::size_t s = 0; s < sources; ++s) {
        jacobian(k, shape_count + 3 * s + 2) = block.values[k * sources + s];
      }
    }
    // The derivative with respect to the continuous positions.
    const double position_step = 1.0e-3;
    std::vector<PixelCenter0> shifted = current_positions;
    for (std::size_t s = 0; s < sources; ++s) {
      for (int axis = 0; axis < 2; ++axis) {
        for (int sign = -1; sign <= 1; sign += 2) {
          shifted[s] = current_positions[s];
          if (axis == 0) shifted[s].x += sign * position_step;
          else shifted[s].y += sign * position_step;
          Result<sources::psfmodel::TemplateBlock> perturbed = sources::psfmodel::build_templates(
              context, request.family, shape_coefficients(theta), basis, shifted, footprints,
              fit_rows, tolerance);
          if (!ok(perturbed)) {
            return model::fail<Ref<EffectivePSF>>(perturbed.code,
                                                  "the pixel integral cannot be built", kOrigin,
                                                  "the fit cannot be built");
          }
          const double sign_value = static_cast<double>(sign);
          for (std::size_t k = 0; k < rows; ++k) {
            const double derivative =
                (val(perturbed).values[k * sources + s] - block.values[k * sources + s]) /
                (sign_value * position_step);
            // The two one sided differences form the central difference, so
            // they are averaged.
            jacobian(k, shape_count + 3 * s + (axis == 0 ? 0 : 1)) += 0.5 * derivative;
          }
        }
      }
      shifted[s] = current_positions[s];
    }
    // The derivative with respect to the shape parameters.  The local
    // parameters of every source are perturbed together, so each column of
    // the template carries its own derivative.
    const double parameter_step = 1.0e-4;
    for (std::size_t r = 0; r < family_count; ++r) {
      std::vector<std::vector<double>> locals;
      locals.reserve(sources);
      for (std::size_t s = 0; s < sources; ++s) {
        locals.push_back(local_of(theta, current_positions[s], s));
      }
      for (int sign = -1; sign <= 1; sign += 2) {
        std::vector<std::vector<double>> perturbed = locals;
        for (std::vector<double>& local : perturbed) local[r] += sign * parameter_step;
        Result<sources::psfmodel::TemplateBlock> block_perturbed =
            sources::psfmodel::build_templates_from_local(context, request.family, perturbed,
                                                          current_positions, footprints, fit_rows,
                                                          tolerance);
        if (!ok(block_perturbed)) {
          return model::fail<Ref<EffectivePSF>>(block_perturbed.code,
                                                "the pixel integral cannot be built", kOrigin,
                                                "the fit cannot be built");
        }
        const double sign_value = static_cast<double>(sign);
        for (std::size_t s = 0; s < sources; ++s) {
          const std::vector<double> basis_vector = sources::psfmodel::basis_values(
              basis, request.spatial_degree, current_positions[s].x, current_positions[s].y);
          for (std::size_t k = 0; k < rows; ++k) {
            const double derivative =
                (val(block_perturbed).values[k * sources + s] - block.values[k * sources + s]) /
                (sign_value * parameter_step);
            for (std::size_t t = 0; t < term_count; ++t) {
              // The two one sided differences form the central difference, so
              // they are averaged.
              jacobian(k, r * term_count + t) += 0.5 * derivative * basis_vector[t];
            }
          }
        }
      }
    }

    // The scaled Gauss-Newton step.
    std::vector<double> column_scale(parameter_count, 0.0);
    for (std::size_t c = 0; c < parameter_count; ++c) {
      double norm = 0.0;
      for (std::size_t k = 0; k < rows; ++k) norm += jacobian(k, c) * jacobian(k, c);
      column_scale[c] = std::sqrt(norm);
    }
    Matrix scaled(rows, parameter_count);
    for (std::size_t k = 0; k < rows; ++k) {
      for (std::size_t c = 0; c < parameter_count; ++c) {
        scaled(k, c) = column_scale[c] > 0.0 ? jacobian(k, c) / column_scale[c] : 0.0;
      }
    }
    svd = numerics::jacobi_svd(scaled, 1.0e-12);
    if (svd.rank < parameter_count) {
      // The design does not determine every parameter; the requested degree is
      // never reduced behind the user's back.
      std::string reason = "the design matrix is rank deficient; the singular values are";
      for (double value : svd.singular_values) reason += " " + std::to_string(value);
      return model::fail<Ref<EffectivePSF>>(Code::not_identifiable, reason, kOrigin,
                                            "the requested spatial degree is not estimable; a "
                                            "constant model can be requested instead");
    }
    // The score of the scaled columns.
    std::vector<double> score(parameter_count, 0.0);
    for (std::size_t c = 0; c < parameter_count; ++c) {
      double sum = 0.0;
      for (std::size_t k = 0; k < rows; ++k) sum += scaled(k, c) * residuals[k];
      score[c] = sum;
    }
    gradient = 0.0;
    for (double value : score) gradient = std::max(gradient, std::fabs(value));
    if (!std::isfinite(objective)) {
      // The likelihood has to be finite and valid before anything else.
      failure = "the objective is not a finite likelihood";
      break;
    }
    // The step in the scaled space: delta = J^+ residual, so the residual
    // itself is the right hand side of the least squares problem.
    step = sources::psfmodel::solve_scaled(svd, residuals, column_scale);
    // The natural units of the parameters: one e-fold of the width, one
    // pixel of the position and the scale of the data for the flux and the
    // background.  The scale is used twice: the trust region limits the step
    // before the line search, and the step tolerance is compared with the
    // step in the same units.
    std::vector<double> natural_scale(parameter_count, 1.0);
    for (std::size_t s = 0; s < sources; ++s) {
      natural_scale[shape_count + 3 * s + 2] =
          std::max(1.0, std::fabs(theta[shape_count + 3 * s + 2]));
    }
    natural_scale[parameter_count - 1] = std::max(1.0, std::fabs(theta[parameter_count - 1]));
    // The Gauss-Newton step is kept inside the trust region.  Without it the
    // linearisation of the profile is not valid far from the solution and the
    // step leaves the region where the model can be approximated.  The
    // Armijo backtracking still starts at the full step.
    {
      double largest = 0.0;
      for (std::size_t c = 0; c < parameter_count; ++c) {
        largest = std::max(largest, std::fabs(step[c]) / natural_scale[c]);
      }
      if (largest > trust_radius) {
        for (std::size_t c = 0; c < parameter_count; ++c) step[c] *= trust_radius / largest;
        trust_region_applied = true;
      }
    }
    // The step may not walk into a parameter region that does not describe a
    // density.  The margin of every local parameter bounds the change that
    // keeps the model valid, and the step is scaled down to that bound.  Every
    // step of the fit goes through this constraint, the pure one and the
    // damped one.
    auto constrain_to_valid_domain = [&](std::vector<double>& candidate) {
      double limit = 1.0;
      for (std::size_t s = 0; s < sources; ++s) {
        const std::vector<double> local = local_of(theta, current_positions[s], s);
        const std::vector<double> basis_vector = sources::psfmodel::basis_values(
            basis, request.spatial_degree, current_positions[s].x, current_positions[s].y);
        for (std::size_t r = 0; r < local.size(); ++r) {
          const double margin = sources::psfmodel::local_parameter_margin(request.family, local, r);
          // The step acts on the coefficients of the spatial polynomial, so
          // the change of the local parameter is the weighted sum of the
          // changes of its coefficients.
          double change = 0.0;
          for (std::size_t k = 0; k < basis_vector.size() && r * term_count + k < parameter_count;
               ++k) {
            change += std::fabs(candidate[r * term_count + k] * basis_vector[k]);
          }
          if (change > margin && change > 0.0) {
            // A parameter that sits on the boundary leaves no room for a step
            // in that direction.  The step is shortened, never frozen: when
            // every margin is exhausted the fit still moves by a fixed
            // fraction, and the line search rejects the trial if it leaves the
            // domain, so the iteration can never deadlock on the boundary.
            const double allowed = margin > 0.0 ? margin / change : 0.5;
            limit = std::min(limit, allowed);
          }
        }
      }
      if (limit < 1.0) {
        for (std::size_t c = 0; c < parameter_count; ++c) candidate[c] *= limit;
        domain_constrained = true;
      }
    };
    constrain_to_valid_domain(step);
    // The step in the natural units of the parameters: this is the step that
    // is compared with the step tolerance.
    double step_norm = 0.0;
    for (std::size_t c = 0; c < parameter_count; ++c) {
      step_norm = std::max(step_norm, std::fabs(step[c]) / natural_scale[c]);
    }
    // The stationarity of the current point is checked before the line
    // search: the score and the predicted step both have to reach the
    // requested thresholds, then a zero step is a convergence.  When the step
    // is rounded to zero but the score is still large, the precision of the
    // model is exhausted and the fit has to be escalated.
    if (gradient <= gradient_tolerance && step_norm <= step_tolerance) {
      converged = true;
      zero_step_convergence = true;
      break;
    }
    if (step_norm <= step_tolerance) {
      failure = "the step is rounded to zero while the score is still larger than the gradient "
                "tolerance; the precision of the model is exhausted";
      precision_exhausted = true;
      break;
    }
    // The reduction the linear model predicts for the full step.  It is the
    // reference of the trust region test below: a step that achieves much
    // less than the model predicts cannot improve the objective, no matter
    // how often it is repeated.
    double predicted_reduction = 0.0;
    for (std::size_t k = 0; k < rows; ++k) {
      double linear = 0.0;
      for (std::size_t c = 0; c < parameter_count; ++c) linear += jacobian(k, c) * step[c];
      predicted_reduction += 0.5 * linear * linear;
    }
    // The Armijo backtracking: the initial step is one and is halved at most
    // thirty two times.
    bool accepted = false;
    double next_objective = objective;
    double lambda = 1.0;
    for (U32 backtrack = 0; backtrack < 32; ++backtrack) {
      std::vector<double> trial = theta;
      for (std::size_t c = 0; c < parameter_count; ++c) trial[c] += lambda * step[c];
      // The trial objective.
      // The trial parameters have to describe a density.  A trial outside the
      // valid domain is an infeasible trial of the line search, not a failure
      // of the fit: the step is shortened until the model is a density again.
      std::vector<PixelCenter0> trial_positions = positions;
      for (std::size_t s = 0; s < sources; ++s) {
        trial_positions[s].x = trial[shape_count + 3 * s];
        trial_positions[s].y = trial[shape_count + 3 * s + 1];
      }
      Result<sources::psfmodel::TemplateBlock> trial_block = sources::psfmodel::build_templates(
          context, request.family, shape_coefficients(trial), basis, trial_positions, footprints,
          fit_rows, tolerance);
      if (!ok(trial_block)) {
        if (trial_block.code == Code::domain) {
          lambda *= 0.5;
          continue;
        }
        return model::fail<Ref<EffectivePSF>>(trial_block.code, "the pixel integral cannot be built",
                                              kOrigin, "the fit cannot be built");
      }
      double trial_objective = 0.0;
      for (std::size_t k = 0; k < rows; ++k) {
        const U64 row = fit_rows[k];
        double model = trial[parameter_count - 1];
        for (std::size_t s = 0; s < sources; ++s) {
          model += trial[shape_count + 3 * s + 2] * val(trial_block).values[k * sources + s];
        }
        const double measured =
            row < val(data).size() ? val(data)[static_cast<std::size_t>(row)] : 0.0;
        const double residual = measured - model;
        trial_objective += 0.5 * residual * residual;
      }
      if (trial_objective <= objective - 1.0e-4 * lambda * gradient * gradient ||
          trial_objective < objective) {
        theta = trial;
        next_objective = trial_objective;
        accepted = true;
        break;
      }
      lambda *= 0.5;
    }
    if (!accepted) {
      // The pure Gauss-Newton step cannot decrease the objective: the damped
      // step (Levenberg-Marquardt) is the documented fallback.  The damping
      // grows until the step decreases the objective.
      double damping = 1.0e-3;
      for (U32 attempt = 0; attempt < 12 && !accepted; ++attempt, damping *= 10.0) {
        std::vector<double> damped =
            sources::psfmodel::solve_damped(svd, residuals, column_scale, damping);
        constrain_to_valid_domain(damped);
        std::vector<double> trial = theta;
        for (std::size_t c = 0; c < parameter_count; ++c) trial[c] += damped[c];
        std::vector<PixelCenter0> trial_positions = positions;
        for (std::size_t s = 0; s < sources; ++s) {
          trial_positions[s].x = trial[shape_count + 3 * s];
          trial_positions[s].y = trial[shape_count + 3 * s + 1];
        }
        Result<sources::psfmodel::TemplateBlock> trial_block = sources::psfmodel::build_templates(
            context, request.family, shape_coefficients(trial), basis, trial_positions, footprints,
            fit_rows, tolerance);
        if (!ok(trial_block)) {
          if (trial_block.code == Code::domain) continue;
          return model::fail<Ref<EffectivePSF>>(trial_block.code,
                                                "the pixel integral cannot be built", kOrigin,
                                                "the fit cannot be built");
        }
        double trial_objective = 0.0;
        for (std::size_t k = 0; k < rows; ++k) {
          const U64 row = fit_rows[k];
          double model = trial[parameter_count - 1];
          for (std::size_t s = 0; s < sources; ++s) {
            model += trial[shape_count + 3 * s + 2] * val(trial_block).values[k * sources + s];
          }
          const double measured =
              row < val(data).size() ? val(data)[static_cast<std::size_t>(row)] : 0.0;
          const double residual = measured - model;
          trial_objective += 0.5 * residual * residual;
        }
        if (trial_objective < objective) {
          theta = trial;
          next_objective = trial_objective;
          accepted = true;
          step = damped;
          damped_step_used = true;
          damping_used = damping;
        }
      }
      if (!accepted) {
        // Neither the pure nor the damped step can improve the objective: the
        // fit reached a local minimum of the objective.  The state is
        // conditional, because the certificate of the parameters rests on that
        // minimum.
        converged = true;
        local_minimum = true;
        break;
      }
    }
    if (converged) break;
    // The trust region test: the reduction the step achieved against the
    // reduction the linear model predicted.  When the achieved reduction is
    // much smaller than the predicted one, the linearisation is exhausted and
    // the fit has reached the floor of the numerical precision of the model.
    {
      const double achieved = objective - next_objective;
      if (achieved <= 0.1 * predicted_reduction) {
        converged = true;
        local_minimum = true;
        break;
      }
    }
  }

  if (!converged && failure.empty()) {
    failure = "the iteration limit is reached before the score is small enough";
  }
  if (!converged) {
    const Code code = precision_exhausted ? Code::precision_exhausted : Code::nonconverged;
    return model::fail<Ref<EffectivePSF>>(code, failure, kOrigin,
                                          "the PSF fit did not converge");
  }

  // The tail bound of the positive model beyond the fit window.
  double tail_mass_upper = 0.0;
  for (std::size_t s = 0; s < sources; ++s) {
    const std::vector<double> local = local_of(theta, positions[s], s);
    const double metric = window_radius * window_radius * sources::psfmodel::max_metric_eigenvalue(request.family, local);
    const double tail = sources::psfmodel::tail_mass(request.family, local,
                                       request.family == PsfRequest::Family::airy ? window_radius
                                                                               : std::sqrt(metric),
                                       request.family == PsfRequest::Family::airy);
    tail_mass_upper = std::max(tail_mass_upper, tail);
  }

  // The numerical certificate of the parameters.  It is derived from the
  // Gauss-Newton linearisation, so it is conditional on that linearisation.
  // The bounds cover the shape coefficients, so they have the same shape as
  // the parameter array itself.
  std::vector<double> parameter_lower = shape_coefficients(theta);
  std::vector<double> parameter_upper = shape_coefficients(theta);
  for (std::size_t c = 0; c < shape_count; ++c) {
    const double radius =
        std::fabs(step[c]) + 1.0e-12 * std::max(1.0, std::fabs(parameter_lower[c]));
    parameter_lower[c] -= radius;
    parameter_upper[c] += radius;
  }

  const std::vector<U64> parameter_shape{static_cast<U64>(family_count),
                                         static_cast<U64>(term_count)};
  Result<ArrayRef> parameter_array =
      write_f64_array(context, shape_coefficients(theta), parameter_shape, model::unit_dimensionless(),
                      "psf-parameters");
  if (!ok(parameter_array)) {
    return model::fail<Ref<EffectivePSF>>(parameter_array.code, "the parameters cannot be stored",
                                          kOrigin, "the fit cannot be built");
  }
  Result<ArrayRef> lower_array =
      write_f64_array(context, parameter_lower, parameter_shape, model::unit_dimensionless(),
                      "psf-parameters");
  if (!ok(lower_array)) {
    return model::fail<Ref<EffectivePSF>>(lower_array.code, "the parameter bounds cannot be stored",
                                          kOrigin, "the fit cannot be built");
  }
  Result<ArrayRef> upper_array =
      write_f64_array(context, parameter_upper, parameter_shape, model::unit_dimensionless(),
                      "psf-parameters");
  if (!ok(upper_array)) {
    return model::fail<Ref<EffectivePSF>>(upper_array.code, "the parameter bounds cannot be stored",
                                          kOrigin, "the fit cannot be built");
  }

  auto error = std::make_shared<ErrorRecord>();
  error->linearization_selection = model::make_condition("psf_shape_fit", "gauss_newton_linearisation");
  error->unknown_model_terms.push_back("the_real_psf_shape_deviation_is_not_a_numerical_error");
  error->unknown_model_terms.push_back("the_parameter_box_is_conditional_on_the_gauss_newton_hessian");
  error->identity = model::make_identity("acsd.ErrorRecord", 2, 0, model::sha256_hex("fit_psf"), "acsd");
  error->assessment = model::make_assessment(Code::ok);
  error->assessment.complete = true;

  auto fitted = std::make_shared<EffectivePSF>();
  fitted->family = request.family;
  fitted->parameters = val(parameter_array);
  fitted->spatial_domain.lower = {domain_x0, domain_y0};
  fitted->spatial_domain.upper = {domain_x1, domain_y1};
  fitted->wavelength_nm.lower = 0.0;
  fitted->wavelength_nm.upper = 0.0;  // the achromatic kernel has no wavelength dependence
  fitted->tail_mass_upper = tail_mass_upper;
  fitted->spatial_basis = basis;
  fitted->achromatic = true;
  fitted->detector_samples = observation->footprints;
  fitted->regroup_scale_project = observation->regrouping;
  fitted->parameter_lower = val(lower_array);
  fitted->parameter_upper = val(upper_array);
  fitted->error = std::shared_ptr<const ErrorRecord>(error);
  fitted->source_scale = nullptr;
  fitted->frame = observation->frame;
  fitted->influence.condition = model::make_condition("psf_shape_fit", "gauss_newton_linearisation");
  fitted->influence.linearization_point = val(parameter_array);
  fitted->assessment = model::make_assessment(Code::ok);
  fitted->assessment.complete = true;
  fitted->assessment.science = ScienceState::conditional;
  model::set_included(fitted->assessment, "gauss_newton_with_armijo_backtracking");
  model::set_omitted(fitted->assessment, "the_parameter_box_is_conditional_on_the_gauss_newton_hessian");
  // The order of the steps is part of the record: the scale check of the
  // columns comes first, the trust region limits the step, and only then the
  // Armijo backtracking starts at the full step.
  model::set_included(
      fitted->assessment,
      "the_column_scale_check_precedes_the_trust_region_and_the_armijo_backtracking; the trust "
      "region radius is one natural unit (one e-fold of the width, one pixel of the position, the "
      "scale of the data for the flux and the background) and it was " +
          std::string(trust_region_applied ? "active" : "not active") + " in the last step");
  // The kind of step that was used in the last iteration.
  if (damped_step_used) {
    model::set_included(fitted->assessment,
                        "the last accepted step was a damped Gauss-Newton step (Levenberg-"
                        "Marquardt, damping " +
                            std::to_string(damping_used) +
                            "), because the pure Gauss-Newton step could not decrease the objective");
  } else {
    model::set_included(fitted->assessment, "the last accepted step was a pure Gauss-Newton step");
  }
  if (zero_step_convergence) {
    model::set_included(fitted->assessment,
                        "the fit converged with a zero step: the score and the predicted step both "
                        "reached the requested thresholds");
  }
  if (local_minimum) {
    // The reason has to be specific and reviewable, and the fit must not claim
    // that the requested accuracy was reached.
    model::add_issue(
        fitted->assessment, Code::unavailable,
        "the achieved decrease of the objective was at most a tenth of the decrease the linear "
        "model predicted, so the linearisation is not certified on that interval; the fit stopped "
        "at a local minimum of the objective and the requested accuracy is not claimed",
        kOrigin, "the parameter box is conditional on that local minimum");
    model::set_omitted(fitted->assessment, "the_requested_fit_accuracy_is_not_certified");
  }
  model::set_omitted(fitted->assessment, "chromatic_psf_is_not_modelled");
  fitted->identity = model::make_identity(
      "acsd.EffectivePSF", 2, 0,
      model::sha256_hex(std::to_string(family_count) + ":" + std::to_string(term_count)), "acsd");

  return model::ok_result(std::shared_ptr<const EffectivePSF>(fitted));
}

}  // namespace acsd
