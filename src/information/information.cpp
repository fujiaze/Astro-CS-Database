// The public information entry points declared in include/acsd/contracts.hpp.
//
// Scientific contract (STATISTICS_SKY_COADD sections 1 to 3.1):
//   * three distinct estimators: V_F (reference source, L^2), V_B,p (actual
//     scene leaf, L^2 sr^-2) and V_ctrl (control statistic);
//   * one propagation of the same random variable: y = R D alpha d gives
//     C_y = R D alpha C_d D alpha^T R^T;
//   * the reference source SNR gives 1/V_F and never 1/V_B;
//   * a PSD covariance has an exact null space that is an exact constraint
//     U0^T y = U0^T [h,Z] theta, so the constraint is solved first and the
//     GLS is restricted to the white noise subspace;
//   * a flux that is absorbed by the background is not identifiable and has no
//     finite variance; an exact constraint may determine the flux exactly and
//     then the variance is exactly zero.
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#include "acsd/contracts.hpp"
#include "information/internal.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd {
namespace {

using information::apply_action_full;
using information::action_codomain_size;
using information::action_domain_size;
using information::complete_orthogonal_factorisation;
using information::dense_action;
using information::dense_covariance_factor;
using numerics::Matrix;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

std::size_t count_elements(const std::vector<U64>& shape) {
  std::size_t total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    total *= static_cast<std::size_t>(extent);
  }
  return total;
}

// Reads an f64 array completely.
Result<std::vector<double>> read_all_f64(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no array supplied",
                                            "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  return operators::read_f64(context, array, 0, count);
}

// Reads a u64 array completely.
Result<std::vector<U64>> read_all_u64(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U64>>(Code::invalid_request, "no array supplied",
                                         "information.read", "the array cannot be read");
  }
  if (array->dtype != DType::u64) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the array is not u64",
                                         "information.read", "the array cannot be read");
  }
  const U64 count = static_cast<U64>(count_elements(array->shape));
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<U64>>(view.code, "the array cannot be read",
                                         "information.read", "the array cannot be read");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

// Row of a leaf inside a leaf index; -1 when the leaf is not part of the index.
Result<int> leaf_row_in_index(Context& context, const Index& index, const HealpixLeaf& leaf) {
  if (!index.ids) {
    return model::fail<int>(Code::invalid_model, "the leaf index has no leaf identities",
                             "information.leaf", "the leaf cannot be located");
  }
  Result<std::vector<U64>> ids = read_all_u64(context, index.ids);
  if (!ok(ids)) {
    return model::fail<int>(ids.code, "the leaf identities cannot be read", "information.leaf",
                             "the leaf cannot be located");
  }
  const std::vector<U64>& flat = val(ids);
  if (flat.size() % 2 != 0) {
    return model::fail<int>(Code::invalid_model, "the leaf identities are malformed",
                             "information.leaf", "the leaf cannot be located");
  }
  const std::size_t rows = flat.size() / 2;
  for (std::size_t row = 0; row < rows; ++row) {
    const U64 order = flat[2 * row];
    const U64 ipix = flat[2 * row + 1];
    if (order == leaf.order && ipix == leaf.ipix) return model::ok_result(static_cast<int>(row));
  }
  return model::ok_result(-1);
}


// Converts the internal rank report into the declared object; the singular
// values and the null space are stored as immutable arrays.
Result<RankReport> make_rank_report(Context& context, const numerics::RankReport& source,
                                    const Id& index_id) {
  RankReport out;
  out.columns = source.columns;
  out.rank = source.rank;
  out.singular_error_bound = source.singular_error_bound;
  out.certified = source.certified;
  Result<ArrayRef> singular = operators::write_f64(
      context, source.singular_values, std::vector<U64>{static_cast<U64>(source.singular_values.size())},
      model::unit_dimensionless(), index_id);
  if (!ok(singular)) {
    return model::fail<RankReport>(singular.code, "the singular values cannot be stored",
                                   "estimate_flux_information", "the rank report is incomplete");
  }
  out.singular_values = val(singular);
  std::vector<double> flat;
  std::vector<U64> shape{0, source.columns};
  if (!source.nullspace.empty()) {
    shape[0] = static_cast<U64>(source.nullspace.size());
    for (const std::vector<double>& row : source.nullspace) {
      for (double value : row) flat.push_back(value);
    }
  }
  Result<ArrayRef> nullspace = operators::write_f64(context, flat, shape,
                                                    model::unit_dimensionless(), index_id);
  if (!ok(nullspace)) {
    return model::fail<RankReport>(nullspace.code, "the null space cannot be stored",
                                   "estimate_flux_information", "the rank report is incomplete");
  }
  out.nullspace = val(nullspace);
  return model::ok_result(std::move(out));
}

}  // namespace

// ---------------------------------------------------------------------------
// estimate_flux_information
// ---------------------------------------------------------------------------

Result<Information> estimate_flux_information(Context& context, const InformationRequest& request) {
  if (!request.h) {
    return model::fail<Information>(Code::invalid_request, "no flux template supplied",
                                    "estimate_flux_information", "the information is unknown");
  }
  if (!request.covariance) {
    return model::fail<Information>(Code::invalid_request, "no covariance supplied",
                                    "estimate_flux_information", "the information is unknown");
  }
  Result<std::vector<double>> h = read_all_f64(context, request.h);
  if (!ok(h)) {
    return model::fail<Information>(h.code, "the flux template cannot be read",
                                    "estimate_flux_information", "the information is unknown");
  }
  const std::size_t n = val(h).size();
  if (n == 0) {
    return model::fail<Information>(Code::invalid_request, "the flux template is empty",
                                    "estimate_flux_information", "the information is unknown");
  }
  for (double value : val(h)) {
    if (!std::isfinite(value)) {
      return model::fail<Information>(Code::invalid_model, "the flux template is not finite",
                                      "estimate_flux_information", "the information is unknown");
    }
  }

  const double rank_relative =
      request.solve.rank_relative > 0.0 ? request.solve.rank_relative : 1e-12;

  Information information;
  information.flux_variance.unit = request.covariance->model
                                       ? request.covariance->model->variance_unit
                                       : model::unit_variance("unknown");
  information.flux_variance.state = NoiseState::known;

  // An unknown model term with a non zero influence makes the complete
  // variance unknown; the known part stays available for diagnostics.
  if (information::covariance_has_unknown_term(request.covariance)) {
    information.flux_variance.value = std::nullopt;
    information.flux_variance.state = NoiseState::unknown;
    information.flux_variance.unknown_terms.push_back("unknown_model_term");
    Result<Information> out = model::ok_result(std::move(information));
    model::add_issue(out.assessment, Code::unavailable,
                     "an unknown model term influences the covariance",
                     "estimate_flux_information", "the flux variance is unknown");
    out.assessment.noise = NoiseState::unknown;
    out.assessment.condition = request.reference.condition;
    return out;
  }

  // ---- design ------------------------------------------------------------
  Matrix design(static_cast<std::size_t>(n), 1);
  for (std::size_t i = 0; i < n; ++i) design(i, 0) = val(h)[i];
  std::size_t columns = 1;
  if (request.background) {
    const U64 background_columns = action_domain_size(request.background);
    const U64 background_rows = action_codomain_size(request.background);
    if (background_columns == 0 || background_rows != n) {
      return model::fail<Information>(Code::invalid_model,
                                      "the background action has an inconsistent shape",
                                      "estimate_flux_information", "the information is unknown");
    }
    Result<Matrix> background =
        dense_action(context, request.background, background_rows, background_columns);
    if (!ok(background)) {
      return model::fail<Information>(background.code, "the background action cannot be applied",
                                      "estimate_flux_information", "the information is unknown");
    }
    columns = 1 + static_cast<std::size_t>(background_columns);
    design = Matrix(static_cast<std::size_t>(n), columns);
    for (std::size_t i = 0; i < n; ++i) design(i, 0) = val(h)[i];
    for (std::size_t j = 0; j < background_columns; ++j) {
      for (std::size_t i = 0; i < n; ++i) {
        design(i, 1 + j) = val(background)(i, j);
      }
    }
  }

  // ---- covariance range and exact null space ------------------------------
  Result<Matrix> factor = dense_covariance_factor(context, request.covariance);
  if (!ok(factor)) {
    return model::fail<Information>(factor.code, "the covariance factor cannot be built",
                                    "estimate_flux_information", "the information is unknown");
  }
  if (val(factor).rows != n) {
    return model::fail<Information>(Code::invalid_model,
                                    "the covariance does not act on the template space",
                                    "estimate_flux_information", "the information is unknown");
  }
  Result<information::RangeSplit> split =
      information::split_covariance_range(val(factor), rank_relative);
  if (!ok(split)) {
    return model::fail<Information>(split.code, "the covariance null space cannot be identified",
                                    "estimate_flux_information", "the information is unknown");
  }
  const Matrix& range_basis = val(split).range_basis;
  const Matrix& null_basis = val(split).null_basis;
  const std::size_t rank = val(split).rank;
  const std::vector<double>& sigma = val(split).singular_values;

  // A_c = Q^T A (the rows in the white noise subspace), D = U0^T A (the exact
  // rows of the constraint).
  Matrix ac(rank, columns);
  for (std::size_t i = 0; i < rank; ++i) {
    for (std::size_t j = 0; j < columns; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < n; ++k) sum += range_basis(k, i) * design(k, j);
      ac(i, j) = sum;
    }
  }
  Matrix d(null_basis.cols, columns);
  if (null_basis.cols > 0) {
    for (std::size_t i = 0; i < null_basis.cols; ++i) {
      for (std::size_t j = 0; j < columns; ++j) {
        double sum = 0.0;
        for (std::size_t k = 0; k < n; ++k) sum += null_basis(k, i) * design(k, j);
        d(i, j) = sum;
      }
    }
  }

  // The whitened design of the white noise subspace.
  Matrix whitened(rank, columns);
  for (std::size_t i = 0; i < rank; ++i) {
    const double inverse = sigma[i] > 0.0 ? 1.0 / sigma[i] : 0.0;
    for (std::size_t j = 0; j < columns; ++j) whitened(i, j) = ac(i, j) * inverse;
  }

  // The exact constraint fixes the component of the parameters in the row
  // space of D; the free directions are the null space of D.
  Matrix free(columns, 0);
  if (d.rows > 0) {
    numerics::SvdResult svd = numerics::jacobi_svd(d, rank_relative);
    free = Matrix(columns, columns - svd.rank, 0.0);
    for (std::size_t j = svd.rank; j < svd.v.cols; ++j) {
      for (std::size_t i = 0; i < columns; ++i) free(i, j - svd.rank) = svd.v(i, j);
    }
  } else {
    free = Matrix(columns, columns, 0.0);
    for (std::size_t i = 0; i < columns; ++i) free(i, i) = 1.0;
  }

  // The flux is estimable only when the unit vector of the flux lies in the
  // row space of the design; otherwise the flux is absorbed by the background
  // and has no finite variance.
  Matrix projector(columns, columns, 0.0);
  {
    numerics::SvdResult svd = numerics::jacobi_svd(design, rank_relative);
    const std::size_t design_rank = svd.rank;
    for (std::size_t j = 0; j < design_rank; ++j) {
      for (std::size_t i = 0; i < columns; ++i) {
        for (std::size_t k = 0; k < columns; ++k) {
          projector(i, k) += svd.v(i, j) * svd.v(k, j);
        }
      }
    }
    double residual = 0.0;
    for (std::size_t i = 0; i < columns; ++i) {
      const double target = (i == 0 ? 1.0 : 0.0);
      const double diff = target - projector(i, 0);
      residual += diff * diff;
    }
    residual = std::sqrt(residual);
    const double tolerance = 1e-8 * std::max(1.0, static_cast<double>(columns));
    if (residual > tolerance) {
      information.flux_variance.value = std::nullopt;
      information.flux_variance.state = NoiseState::unknown;
      Result<RankReport> report =
          make_rank_report(context, numerics::classify_rank(
                                         numerics::jacobi_svd(whitened, rank_relative), columns,
                                         rank_relative),
                           request.reference.id);
      if (ok(report)) information.rank = val(report);
      Result<Information> out = model::ok_result(std::move(information));
      model::add_issue(out.assessment, Code::not_identifiable,
                       "the flux template is absorbed by the background and the exact constraints",
                       "estimate_flux_information", "there is no finite flux variance");
      out.assessment.science = ScienceState::conditional;
      out.assessment.condition = request.reference.condition;
      return out;
    }
  }

  // The component of the flux in the free directions and the covariance of
  // the free parameters.
  std::vector<double> component(columns, 0.0);
  for (std::size_t i = 0; i < columns; ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < free.cols; ++j) sum += free(i, j) * ((i == 0) ? 1.0 : 0.0);
    component[i] = sum;
  }
  Matrix reduced(rank, free.cols, 0.0);
  for (std::size_t i = 0; i < rank; ++i) {
    for (std::size_t j = 0; j < free.cols; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < columns; ++k) sum += whitened(i, k) * free(k, j);
      reduced(i, j) = sum;
    }
  }
  // The rank of the free design is classified on the reduced Jacobian and not
  // on its Gram matrix.  The singular values of the Gram matrix are the
  // squares of the singular values of the Jacobian, so a relative threshold
  // on the Gram matrix would cut the modes whose singular value is below the
  // square root of the threshold and would report an estimable mode as not
  // estimable.
  numerics::SvdResult reduced_svd = numerics::jacobi_svd(reduced, rank_relative);
  const numerics::RankReport reduced_rank =
      numerics::classify_rank(reduced_svd, free.cols, rank_relative);
  const std::size_t effective = reduced_rank.rank;
  double variance = 0.0;
  if (effective > 0) {
    // The flux component has to lie in the range of the free design.
    std::vector<double> projected(free.cols, 0.0);
    for (std::size_t j = 0; j < effective; ++j) {
      double sum = 0.0;
      for (std::size_t i = 0; i < free.cols; ++i) sum += reduced_svd.v(i, j) * component[i];
      for (std::size_t i = 0; i < free.cols; ++i) projected[i] += sum * reduced_svd.v(i, j);
    }
    double residual = 0.0;
    for (std::size_t i = 0; i < free.cols; ++i) {
      const double diff = component[i] - projected[i];
      residual += diff * diff;
    }
    residual = std::sqrt(residual);
    const double tolerance = 1e-8 * std::max(1.0, static_cast<double>(free.cols));
    if (residual > tolerance) {
      information.flux_variance.value = std::nullopt;
      information.flux_variance.state = NoiseState::unknown;
      Result<RankReport> report =
          make_rank_report(context, numerics::classify_rank(
                                         numerics::jacobi_svd(whitened, rank_relative), columns,
                                         rank_relative),
                           request.reference.id);
      if (ok(report)) information.rank = val(report);
      Result<Information> out = model::ok_result(std::move(information));
      model::add_issue(out.assessment, Code::not_identifiable,
                       "the free directions of the difference model do not carry the flux",
                       "estimate_flux_information", "there is no finite flux variance");
      out.assessment.science = ScienceState::conditional;
      out.assessment.condition = request.reference.condition;
      return out;
    }
    // The variance of the flux is c^T (R^T R)^+ c, which is the sum of the
    // squares of the projections on the right singular vectors of the
    // reduced Jacobian, divided by their singular values.
    double norm = 0.0;
    for (std::size_t j = 0; j < effective; ++j) {
      const double singular = reduced_svd.singular_values[j];
      if (!(singular > 0.0)) continue;
      double sum = 0.0;
      for (std::size_t i = 0; i < free.cols; ++i) sum += reduced_svd.v(i, j) * component[i];
      const double scale = sum / singular;
      norm += scale * scale;
    }
    variance = norm;
  }

  information.flux_variance.value = variance;
  information.flux_variance.state = NoiseState::known;
  {
    Result<RankReport> report =
        make_rank_report(context,
                         numerics::classify_rank(numerics::jacobi_svd(whitened, rank_relative),
                                                  columns, rank_relative),
                         request.reference.id);
    if (!ok(report)) {
      return model::fail<Information>(report.code, "the rank report cannot be stored",
                                      "estimate_flux_information", "the information is unknown");
    }
    information.rank = val(report);
  }
  if (!val(split).rank_certified) {
    information.rank.certified = false;
  }

  Result<Information> out = model::ok_result(std::move(information));
  out.assessment.condition = request.reference.condition;
  out.assessment.noise = NoiseState::known;
  model::set_included(out.assessment, "reference_source_flux_variance");
  model::set_included(out.assessment, "background_projection");
  if (rank < n) {
    model::set_included(out.assessment, "exact_covariance_constraints");
    model::set_omitted(out.assessment, "constraint_consistency_check_needs_the_data");
  }
  return out;
}

// ---------------------------------------------------------------------------
// evaluate_scene_variance
// ---------------------------------------------------------------------------

Result<std::vector<Variance>> evaluate_scene_variance(Context& context, const FramesProvider& frames,
                                                      Id frame,
                                                      std::span<const HealpixLeaf> leaves) {
  Result<Ref<FrameDescription>> described = frames.describe_frame(context, frame);
  if (!ok(described)) {
    return model::fail<std::vector<Variance>>(Code::unavailable,
                                              "the frame cannot be described",
                                              "evaluate_scene_variance",
                                              "the scene variance is unknown");
  }
  const FrameDescription description = *val(described);
  std::vector<Variance> out;
  out.reserve(leaves.size());
  const Unit unknown_unit = model::unit_variance("unknown");

  if (!description.noise) {
    for (std::size_t i = 0; i < leaves.size(); ++i) {
      Variance variance;
      variance.value = std::nullopt;
      variance.unit = unknown_unit;
      variance.state = NoiseState::unknown;
      variance.unknown_terms.push_back("frame_noise_absent");
      out.push_back(variance);
    }
    return model::ok_result(std::move(out));
  }

  const bool unknown = information::covariance_has_unknown_term(description.noise);
  const Unit variance_unit = description.noise->model
                                 ? description.noise->model->variance_unit
                                 : unknown_unit;
  const U64 covariance_size = description.noise->input_output.domain
                                  ? description.noise->input_output.domain->size
                                  : 0;

  // The leaf operator maps the native signal to the leaf space, so its
  // codomain carries the leaves and its domain the native coordinates.  The
  // functional of a leaf in the native space is the transposed operator.
  const U64 leaf_rows = action_codomain_size(description.native_to_leaf);
  const U64 native_rows = action_domain_size(description.native_to_leaf);
  enum class Space { kLeaf, kNative, kUnknown };
  Space space = Space::kUnknown;
  if (covariance_size != 0) {
    if (leaf_rows != 0 && covariance_size == leaf_rows) {
      space = Space::kLeaf;
    } else if (native_rows != 0 && covariance_size == native_rows) {
      space = Space::kNative;
    }
  } else if (!description.native_to_leaf) {
    space = Space::kLeaf;
  }
  if (space == Space::kUnknown) {
    return model::fail<std::vector<Variance>>(Code::invalid_model,
                                              "the covariance does not match the frame geometry",
                                              "evaluate_scene_variance",
                                              "the scene variance is unknown");
  }

  for (const HealpixLeaf& leaf : leaves) {
    // The cancellation checkpoint of the scene variance: the evaluation stops
    // with a named failure and without a partial result.
    if (context.cancellation.requested()) {
      return model::fail<std::vector<Variance>>(Code::cancelled, "cancelled",
                                                 "evaluate_scene_variance",
                                                 "the scene variance is cancelled");
    }
    Variance variance;
    variance.unit = variance_unit;
    if (unknown) {
      variance.value = std::nullopt;
      variance.state = NoiseState::unknown;
      variance.unknown_terms.push_back("unknown_model_term");
      out.push_back(variance);
      continue;
    }
    int row = -1;
    if (description.index) {
      Result<int> located = leaf_row_in_index(context, *description.index, leaf);
      if (!ok(located)) {
        return model::fail<std::vector<Variance>>(located.code, "the leaf index cannot be read",
                                                  "evaluate_scene_variance",
                                                  "the scene variance is unknown");
      }
      row = val(located);
    }
    if (row < 0) {
      variance.value = std::nullopt;
      variance.state = NoiseState::unknown;
      variance.unknown_terms.push_back("leaf_not_in_frame");
      out.push_back(variance);
      continue;
    }
    std::vector<double> functional;
    if (space == Space::kLeaf) {
      functional.assign(static_cast<std::size_t>(leaf_rows == 0 ? row + 1 : leaf_rows), 0.0);
      functional[static_cast<std::size_t>(row)] = 1.0;
    } else {
      functional.assign(static_cast<std::size_t>(native_rows), 0.0);
      std::vector<double> selector(static_cast<std::size_t>(leaf_rows), 0.0);
      selector[static_cast<std::size_t>(row)] = 1.0;
      Result<std::vector<double>> row_vector =
          information::transpose_apply_full(context, description.native_to_leaf, selector);
      if (!ok(row_vector)) {
        return model::fail<std::vector<Variance>>(row_vector.code,
                                                  "the leaf operator cannot be applied",
                                                  "evaluate_scene_variance",
                                                  "the scene variance is unknown");
      }
      if (val(row_vector).size() != functional.size()) {
        return model::fail<std::vector<Variance>>(Code::invalid_model,
                                                  "the leaf operator has an inconsistent shape",
                                                  "evaluate_scene_variance",
                                                  "the scene variance is unknown");
      }
      functional = val(row_vector);
    }
    Result<double> value =
        uncertainty::quadratic_form_value(context, description.noise, functional);
    if (!ok(value)) {
      return model::fail<std::vector<Variance>>(value.code, "the variance cannot be computed",
                                                "evaluate_scene_variance",
                                                "the scene variance is unknown");
    }
    variance.value = val(value);
    variance.state = description.noise->model ? description.noise->model->state : NoiseState::known;
    out.push_back(variance);
  }
  Result<std::vector<Variance>> result = model::ok_result(std::move(out));
  // The two dimensions of the frame are named, so that a reviewer can see
  // which space the variance was evaluated in.
  model::set_included(result.assessment,
                      "leaf_functional_dimension_" + std::to_string(leaf_rows));
  model::set_included(result.assessment,
                      "native_coordinate_dimension_" + std::to_string(native_rows));
  model::set_included(result.assessment, space == Space::kLeaf ? "scene_variance_in_leaf_space"
                                                               : "scene_variance_in_native_space");
  return result;
}

// ---------------------------------------------------------------------------
// control_covariance
// ---------------------------------------------------------------------------

Result<Ref<CovarianceAction>> control_covariance(Context& context, Ref<CovarianceAction> covariance,
                                                   Ref<LinearAction> T) {
  if (!covariance || !covariance->model) {
    return model::fail<Ref<CovarianceAction>>(Code::invalid_request, "no covariance supplied",
                                              "control_covariance",
                                              "the control covariance cannot be built");
  }
  if (!T) {
    return model::fail<Ref<CovarianceAction>>(Code::invalid_request, "no control operator supplied",
                                              "control_covariance",
                                              "the control covariance cannot be built");
  }
  const NoiseModel& source = *covariance->model;
  std::vector<Ref<NoiseRoot>> roots;
  roots.insert(roots.end(), source.innovations.begin(), source.innovations.end());
  roots.insert(roots.end(), source.shared_factors.begin(), source.shared_factors.end());
  if (roots.size() != source.influence.size()) {
    return model::fail<Ref<CovarianceAction>>(Code::invalid_model,
                                              "the influence count does not match the root count",
                                              "control_covariance",
                                              "the control covariance cannot be built");
  }
  auto model = std::make_shared<NoiseModel>();
  model->variance_unit = source.variance_unit;
  model->condition = source.condition;
  model->state = source.state;
  model->unknown_terms = source.unknown_terms;
  model->innovations = source.innovations;
  model->shared_factors = source.shared_factors;
  model->influence.reserve(source.influence.size());
  for (const Ref<LinearAction>& influence : source.influence) {
    // The control statistic is T y, so the influence of every root becomes
    // T L_k; the shared identity of the root is kept, so the cross terms
    // between controls that share a root survive.
    Result<Ref<LinearAction>> composed = compose(context, T, influence);
    if (!ok(composed)) {
      return model::fail<Ref<CovarianceAction>>(composed.code,
                                                "the control operator cannot be composed",
                                                "control_covariance",
                                                "the control covariance cannot be built");
    }
    model->influence.push_back(val(composed));
  }
  model->identity = model::make_identity("acsd.NoiseModel", 2, 0,
                                         model::hash_index(Index::Kind::latent, roots.size(), 0, {}),
                                         "acsd");
  model->assessment = model::make_assessment(Code::ok);
  model->assessment.complete = true;
  model->assessment.noise = source.state;
  Ref<NoiseModel> owned = std::shared_ptr<const NoiseModel>(model);
  return make_covariance(context, owned);
}

// ---------------------------------------------------------------------------
// reconstruct_scene_variance
// ---------------------------------------------------------------------------

namespace {

// Point in polygon test for a chart ring.
bool point_in_ring(const std::vector<PixelEdge0>& vertices, const Vec2& point) {
  if (vertices.size() < 3) return false;
  bool inside = false;
  for (std::size_t i = 0, j = vertices.size() - 1; i < vertices.size(); j = i++) {
    const double xi = vertices[i].x;
    const double yi = vertices[i].y;
    const double xj = vertices[j].x;
    const double yj = vertices[j].y;
    if (((yi > point[1]) != (yj > point[1])) &&
        (point[0] < (xj - xi) * (point[1] - yi) / (yj - yi) + xi)) {
      inside = !inside;
    }
  }
  return inside;
}

}  // namespace

Result<VariancePrediction> reconstruct_scene_variance(Context& context,
                                                       Ref<PrecisionControls> controls,
                                                       Ref<SupportedTriangulation> triangulation,
                                                       std::span<const SkyICRS> queries) {
  if (!controls || !triangulation) {
    return model::fail<VariancePrediction>(Code::invalid_request, "the inputs are incomplete",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  if (controls->kind != PrecisionControls::Kind::actual_scene) {
    return model::fail<VariancePrediction>(
        Code::invalid_request, "the controls are not actual scene controls",
        "reconstruct_scene_variance",
        "a reference source control cannot be renamed into an actual scene variance");
  }
  Result<std::vector<double>> values = read_all_f64(context, controls->values);
  if (!ok(values)) {
    return model::fail<VariancePrediction>(values.code, "the control values cannot be read",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  Result<std::vector<double>> log_errors = read_all_f64(context, controls->log_errors);
  if (!ok(log_errors)) {
    return model::fail<VariancePrediction>(log_errors.code, "the control errors cannot be read",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  Result<std::vector<double>> vertices = read_all_f64(context, triangulation->vertices);
  if (!ok(vertices)) {
    return model::fail<VariancePrediction>(vertices.code, "the chart vertices cannot be read",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  Result<std::vector<U64>> triangles = read_all_u64(context, triangulation->triangles);
  if (!ok(triangles)) {
    return model::fail<VariancePrediction>(triangles.code, "the triangles cannot be read",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  if (val(values).size() != val(log_errors).size()) {
    return model::fail<VariancePrediction>(Code::invalid_model,
                                           "the control values and errors disagree",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  if (val(vertices).size() % 2 != 0 || val(triangles).size() % 3 != 0) {
    return model::fail<VariancePrediction>(Code::invalid_model, "the triangulation is malformed",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  const std::size_t vertex_count = val(vertices).size() / 2;
  const std::size_t triangle_count = val(triangles).size() / 3;

  information::ChartProjection projection;
  if (!information::make_chart_projection(triangulation->chart, projection)) {
    return model::fail<VariancePrediction>(Code::invalid_model, "the chart is not usable",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }

  // The control positions are either chart coordinates (two columns) or sky
  // directions (three columns).  Project them into the chart of the
  // triangulation so that every control can be matched with its vertex and
  // keeps its own external error bound.
  Result<std::vector<double>> positions = read_all_f64(context, controls->positions);
  if (!ok(positions)) {
    return model::fail<VariancePrediction>(positions.code, "the control positions cannot be read",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  const std::size_t control_count = val(values).size();
  const std::size_t position_width = (control_count == 0) ? 0 : val(positions).size() / control_count;
  if (position_width != 2 && position_width != 3) {
    return model::fail<VariancePrediction>(Code::invalid_model,
                                           "the control positions have an unexpected shape",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  std::vector<int> control_vertex(control_count, -1);
  for (std::size_t i = 0; i < control_count; ++i) {
    Vec2 point{0.0, 0.0};
    if (position_width == 2) {
      point = Vec2{val(positions)[2 * i], val(positions)[2 * i + 1]};
    } else {
      const SkyICRS direction{val(positions)[3 * i], val(positions)[3 * i + 1],
                              val(positions)[3 * i + 2]};
      if (!information::project_to_chart(projection, direction, point)) continue;
    }
    for (std::size_t j = 0; j < vertex_count; ++j) {
      const double dx = val(vertices)[2 * j] - point[0];
      const double dy = val(vertices)[2 * j + 1] - point[1];
      if (std::fabs(dx) <= 1e-12 && std::fabs(dy) <= 1e-12) {
        control_vertex[i] = static_cast<int>(j);
        break;
      }
    }
  }

  // The inverse map: the control that owns a vertex of the mesh.
  std::vector<int> vertex_control(vertex_count, -1);
  for (std::size_t i = 0; i < control_count; ++i) {
    if (control_vertex[i] >= 0) {
      vertex_control[static_cast<std::size_t>(control_vertex[i])] = static_cast<int>(i);
    }
  }

  const bool has_curvature = controls->hessian_norm_upper.has_value();
  const double curvature = controls->hessian_norm_upper.value_or(0.0);
  if (has_curvature && !(curvature >= 0.0)) {
    return model::fail<VariancePrediction>(Code::invalid_model,
                                           "the Hessian bound is not a positive number",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }

  std::vector<double> prediction(queries.size(), kNaN);
  std::vector<double> lower(queries.size(), kNaN);
  std::vector<double> upper(queries.size(), kNaN);
  std::vector<std::uint8_t> states(queries.size(), 0);

  for (std::size_t q = 0; q < queries.size(); ++q) {
    Vec2 point{0.0, 0.0};
    if (!information::project_to_chart(projection, queries[q], point)) {
      states[q] = 0;
      continue;
    }
    bool in_hole = false;
    for (const Ring& hole : triangulation->holes) {
      if (point_in_ring(hole.vertices, point)) {
        in_hole = true;
        break;
      }
    }
    if (in_hole) {
      states[q] = 0;
      continue;
    }
    // Locate the triangle that contains the query point.
    const std::size_t* found = nullptr;
    std::vector<double> bary;
    for (std::size_t t = 0; t < triangle_count; ++t) {
      const U64 a = val(triangles)[3 * t];
      const U64 b = val(triangles)[3 * t + 1];
      const U64 c = val(triangles)[3 * t + 2];
      if (a >= vertex_count || b >= vertex_count || c >= vertex_count) continue;
      const double ax = val(vertices)[2 * a];
      const double ay = val(vertices)[2 * a + 1];
      const double bx = val(vertices)[2 * b];
      const double by = val(vertices)[2 * b + 1];
      const double cx = val(vertices)[2 * c];
      const double cy = val(vertices)[2 * c + 1];
      const double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
      if (!(std::fabs(area) > 0.0)) continue;
      const double l0 = ((bx - point[0]) * (cy - point[1]) - (by - point[1]) * (cx - point[0])) / area;
      const double l1 = ((cx - point[0]) * (ay - point[1]) - (cy - point[1]) * (ax - point[0])) / area;
      const double l2 = 1.0 - l0 - l1;
      const double tolerance = 1e-12;
      if (l0 >= -tolerance && l1 >= -tolerance && l2 >= -tolerance) {
        found = &val(triangles)[3 * t];
        bary = {l0, l1, l2};
        break;
      }
    }
    if (found == nullptr) {
      states[q] = 0;
      continue;
    }
    double log_value = 0.0;
    double delta = 0.0;
    bool usable = true;
    const U64 corners[3] = {found[0], found[1], found[2]};
    for (std::size_t k = 0; k < 3; ++k) {
      const int control = vertex_control[static_cast<std::size_t>(corners[k])];
      if (control < 0) {
        usable = false;
        break;
      }
      const double value = val(values)[static_cast<std::size_t>(control)];
      if (!(value > 0.0)) {
        usable = false;
        break;
      }
      log_value += bary[k] * std::log(value);
      delta += bary[k] * val(log_errors)[static_cast<std::size_t>(control)];
      const double dx = val(vertices)[2 * corners[k]] - point[0];
      const double dy = val(vertices)[2 * corners[k] + 1] - point[1];
      delta += 0.5 * curvature * bary[k] * (dx * dx + dy * dy);
    }
    if (!usable) {
      states[q] = 0;
      continue;
    }
    prediction[q] = std::exp(log_value);
    states[q] = 1;
    if (has_curvature) {
      lower[q] = std::exp(log_value - delta);
      upper[q] = std::exp(log_value + delta);
    }
  }

  VariancePrediction out;
  out.estimator = controls->estimator;
  out.conditional = !has_curvature;
  Result<ArrayRef> value_array = operators::write_f64(
      context, prediction, std::vector<U64>{static_cast<U64>(prediction.size())},
      controls->estimator.unit, controls->estimator.id);
  if (!ok(value_array)) {
    return model::fail<VariancePrediction>(value_array.code, "the prediction cannot be stored",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  out.values = val(value_array);
  Result<ArrayRef> lower_array = operators::write_f64(
      context, lower, std::vector<U64>{static_cast<U64>(lower.size())}, controls->estimator.unit,
      controls->estimator.id);
  if (!ok(lower_array)) {
    return model::fail<VariancePrediction>(lower_array.code, "the bounds cannot be stored",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  out.lower = val(lower_array);
  Result<ArrayRef> upper_array = operators::write_f64(
      context, upper, std::vector<U64>{static_cast<U64>(upper.size())}, controls->estimator.unit,
      controls->estimator.id);
  if (!ok(upper_array)) {
    return model::fail<VariancePrediction>(upper_array.code, "the bounds cannot be stored",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  out.upper = val(upper_array);
  const std::vector<std::byte> state_bytes = model::encode_u8(states);
  Result<ArrayRef> state_array = context.storage.copy(
      context, DType::u8, state_bytes, std::vector<U64>{static_cast<U64>(states.size())},
      model::unit_dimensionless(), controls->estimator.id);
  if (!ok(state_array)) {
    return model::fail<VariancePrediction>(state_array.code, "the support states cannot be stored",
                                           "reconstruct_scene_variance",
                                           "the reconstruction is not available");
  }
  out.support_states = val(state_array);

  Result<VariancePrediction> result = model::ok_result(std::move(out));
  result.assessment.condition = controls->condition;
  if (!has_curvature) {
    model::add_issue(result.assessment, Code::unavailable,
                     "no external Hessian bound is available, only a conditional prediction",
                     "reconstruct_scene_variance", "no certified interval is published");
    result.assessment.science = ScienceState::conditional;
    result.assessment.noise = NoiseState::conditional;
    model::set_omitted(result.assessment, "log_variance_curvature_bound");
  } else {
    result.assessment.science = ScienceState::bounded;
    result.assessment.noise = NoiseState::conditional;
    model::set_included(result.assessment, "external_log_error_bounds");
    model::set_included(result.assessment, "external_hessian_norm_upper");
  }
  return result;
}

}  // namespace acsd
