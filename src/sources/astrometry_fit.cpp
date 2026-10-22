// The nonlinear fit of the blind astrometric solver.
//
// The model is the plane plus SIP of the coordinate contract.  The residuals
// are the differential astrometry residuals (the right ascension times the
// cosine of the declination and the declination) between the catalogue
// direction and the direction predicted by the model.  The Jacobian is
// analytic, the step comes from a QR based least squares and the step length
// is reduced by a line search.
//
// The SIP order is raised from zero to the requested order.  A higher order is
// only accepted when the added terms are fully estimable (the classified rank
// covers every parameter) and when the residual of the held out group of
// sources improves.  The held out group is selected by the hash of the source
// identifier modulo five.
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "astrometry_common.hpp"
#include "numerics/linalg.hpp"

namespace acsd::sources {
namespace {

// The parameters of the fit.
std::vector<double> to_vector(const WcsParameters& parameters) {
  std::vector<double> values;
  values.reserve(wcs_parameter_count(parameters.sip_order));
  values.push_back(parameters.crpix_x);
  values.push_back(parameters.crpix_y);
  values.push_back(parameters.ra0);
  values.push_back(parameters.dec0);
  values.push_back(parameters.cd[0]);
  values.push_back(parameters.cd[1]);
  values.push_back(parameters.cd[2]);
  values.push_back(parameters.cd[3]);
  values.insert(values.end(), parameters.sip_a.begin(), parameters.sip_a.end());
  values.insert(values.end(), parameters.sip_b.begin(), parameters.sip_b.end());
  return values;
}

void from_vector(const std::vector<double>& values, WcsParameters& parameters) {
  parameters.crpix_x = values[0];
  parameters.crpix_y = values[1];
  parameters.ra0 = values[2];
  parameters.dec0 = values[3];
  for (std::size_t k = 0; k < 4; ++k) parameters.cd[k] = values[4 + k];
  // The order of the SIP is recovered from the length of the vector, so that
  // the restored parameters always describe the same model as the vector.
  const std::size_t terms = (values.size() - 8) / 2;
  parameters.sip_order = 0;
  for (U32 order = 2; order <= 9; ++order) {
    if (sip_term_count(order) == terms) {
      parameters.sip_order = order;
      break;
    }
  }
  if (sip_term_count(parameters.sip_order) != terms) return;
  parameters.sip_a.assign(values.begin() + 8, values.begin() + 8 + static_cast<std::ptrdiff_t>(terms));
  parameters.sip_b.assign(values.begin() + 8 + static_cast<std::ptrdiff_t>(terms),
                          values.begin() + 8 + static_cast<std::ptrdiff_t>(2 * terms));
}

// The residuals of the fit and their analytic Jacobian.
void evaluate(const WcsParameters& parameters, const std::vector<MatchPair>& pairs,
              std::vector<double>& residuals, numerics::Matrix* jacobian) {
  residuals.clear();
  residuals.reserve(pairs.size() * 2);
  if (jacobian != nullptr) {
    jacobian->rows = pairs.size() * 2;
    jacobian->cols = wcs_parameter_count(parameters.sip_order);
    jacobian->data.assign(jacobian->rows * jacobian->cols, 0.0);
  }
  for (std::size_t index = 0; index < pairs.size(); ++index) {
    const MatchPair& pair = pairs[index];
    const Vec3 predicted = wcs_forward(parameters, pair.pixel_x, pair.pixel_y);
    const Vec3& catalogue = pair.sky;
    const double ra = std::atan2(catalogue[1], catalogue[0]);
    const double dec = std::asin(std::clamp(catalogue[2], -1.0, 1.0));
    const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
    const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
    const double difference[3] = {predicted[0] - catalogue[0], predicted[1] - catalogue[1],
                                  predicted[2] - catalogue[2]};
    const double denominator = catalogue[0] * predicted[0] + catalogue[1] * predicted[1] +
                               catalogue[2] * predicted[2];
    if (!(std::abs(denominator) > 1e-9)) {
      residuals.push_back(0.0);
      residuals.push_back(0.0);
      continue;
    }
    const double delta_ra = (difference[0] * east[0] + difference[1] * east[1] +
                             difference[2] * east[2]) / denominator;
    const double delta_dec = (difference[0] * north[0] + difference[1] * north[1] +
                              difference[2] * north[2]) / denominator;
    residuals.push_back(delta_ra);
    residuals.push_back(delta_dec);
    if (jacobian == nullptr) continue;

    double derivative[3][16] = {};
    wcs_forward_jacobian(parameters, pair.pixel_x, pair.pixel_y, derivative);
    for (std::size_t column = 0; column < jacobian->cols; ++column) {
      const double dp[3] = {derivative[0][column], derivative[1][column], derivative[2][column]};
      const double projection_east = dp[0] * east[0] + dp[1] * east[1] + dp[2] * east[2];
      const double projection_north = dp[0] * north[0] + dp[1] * north[1] + dp[2] * north[2];
      const double projection_catalogue = dp[0] * catalogue[0] + dp[1] * catalogue[1] +
                                         dp[2] * catalogue[2];
      (*jacobian)(2 * index, column) =
          (projection_east - delta_ra * projection_catalogue) / denominator;
      (*jacobian)(2 * index + 1, column) =
          (projection_north - delta_dec * projection_catalogue) / denominator;
    }
  }
}

// The classified rank of the Jacobian.
//
// The columns are equilibrated before the rank is classified: the parameters
// of the model live on very different scales (the reference point in pixels,
// the angles in radians and the SIP coefficients in inverse pixels), and the
// raw scaling of the columns would decide the rank instead of the geometry.
// The norm of every column of the matrix.
std::vector<double> column_scales(const numerics::Matrix& matrix) {
  std::vector<double> scales(matrix.cols, 1.0);
  for (std::size_t j = 0; j < matrix.cols; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < matrix.rows; ++i) {
      norm = std::max(norm, std::fabs(matrix(i, j)));
    }
    if (norm > 0.0) scales[j] = norm;
  }
  return scales;
}

U32 jacobian_rank(const numerics::Matrix& jacobian, double rank_relative) {
  if (jacobian.rows == 0 || jacobian.cols == 0) return 0;
  std::vector<double> scales(jacobian.cols, 1.0);
  for (std::size_t j = 0; j < jacobian.cols; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < jacobian.rows; ++i) {
      norm = std::max(norm, std::fabs(jacobian(i, j)));
    }
    if (norm > 0.0) scales[j] = norm;
  }
  numerics::Matrix scaled(jacobian.rows, jacobian.cols);
  for (std::size_t i = 0; i < jacobian.rows; ++i) {
    for (std::size_t j = 0; j < jacobian.cols; ++j) scaled(i, j) = jacobian(i, j) / scales[j];
  }
  const numerics::SvdResult svd = numerics::jacobi_svd(scaled, rank_relative);
  if (!svd.converged) return 0;
  const numerics::RankReport report = numerics::classify_rank(svd, jacobian.cols, rank_relative);
  return static_cast<U32>(report.rank);
}

double cost_of(const std::vector<double>& residuals) {
  double total = 0.0;
  for (double value : residuals) total += value * value;
  return 0.5 * total;
}

// The deterministic hash of a source identifier.
U64 source_hash(U64 source_id) {
  U64 value = source_id + 0x9e3779b97f4a7c15ull;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
  return value ^ (value >> 31);
}

}  // namespace

// The fit of one SIP order.
FitOutcome fit_order(const std::vector<MatchPair>& training, const std::vector<MatchPair>& holdout,
                      const WcsParameters& initial, U32 sip_order, U32 max_iterations) {
  FitOutcome outcome;
  outcome.sip_order = sip_order;

  WcsParameters parameters = initial;
  parameters.sip_order = sip_order;
  const std::size_t terms = sip_term_count(sip_order);
  parameters.sip_a.assign(terms, 0.0);
  parameters.sip_b.assign(terms, 0.0);

  std::vector<double> values = to_vector(parameters);
  std::vector<double> residuals;
  std::vector<double> candidate_residuals;
  evaluate(parameters, training, residuals, nullptr);
  double cost = cost_of(residuals);
  if (sip_order == 0 && training.size() > 10) {
    // The predicted pixel of the first pairs under the initial model.
    for (std::size_t k = 0; k < std::min<std::size_t>(6, training.size()); ++k) {
      const Vec3 predicted = wcs_forward(parameters, training[k].pixel_x, training[k].pixel_y);
      const double separation = std::acos(std::clamp(
          predicted[0] * training[k].sky[0] + predicted[1] * training[k].sky[1] +
              predicted[2] * training[k].sky[2],
          -1.0, 1.0));
      const Vec3& cc = training[k].sky;
      const double rra = std::atan2(cc[1], cc[0]);
      const double rdec = std::asin(std::clamp(cc[2], -1.0, 1.0));
      const Vec3 e2{-std::sin(rra), std::cos(rra), 0.0};
      const Vec3 n2{-std::sin(rdec) * std::cos(rra), -std::sin(rdec) * std::sin(rra), std::cos(rdec)};
      const double dden = cc[0] * predicted[0] + cc[1] * predicted[1] + cc[2] * predicted[2];
      const double dra = ((predicted[0] - cc[0]) * e2[0] + (predicted[1] - cc[1]) * e2[1] +
                          (predicted[2] - cc[2]) * e2[2]) / dden;
      const double ddec = ((predicted[0] - cc[0]) * n2[0] + (predicted[1] - cc[1]) * n2[1] +
                           (predicted[2] - cc[2]) * n2[2]) / dden;
      // The tangent coordinates of the catalogue direction under the initial
      // model and the coordinates implied by the model.
      {
        const Vec3& c = training[k].sky;
        const double ra = std::atan2(c[1], c[0]);
        const double dec = std::asin(std::clamp(c[2], -1.0, 1.0));
        const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
        const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
        const double denominator = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
        const double xi = (east[0] * c[0] + east[1] * c[1] + east[2] * c[2]) / denominator;
        const double eta = (north[0] * c[0] + north[1] * c[1] + north[2] * c[2]) / denominator;
        const double u = training[k].pixel_x + 1.0 - parameters.crpix_x;
        const double v = training[k].pixel_y + 1.0 - parameters.crpix_y;
        const double q_x = parameters.cd[0] * u + parameters.cd[1] * v;
        const double q_y = parameters.cd[2] * u + parameters.cd[3] * v;
        {
          const Vec3 p2 = wcs_forward(parameters, training[k].pixel_x, training[k].pixel_y);
          const double sep = std::acos(std::clamp(
              p2[0] * training[k].sky[0] + p2[1] * training[k].sky[1] + p2[2] * training[k].sky[2],
              -1.0, 1.0));
        }
      }
    }
  }

  for (U32 iteration = 0; iteration < max_iterations; ++iteration) {
    numerics::Matrix jacobian;
    evaluate(parameters, training, residuals, &jacobian);
    // The columns of the Jacobian are equilibrated before the step is solved:
    // the parameters of the model live on very different scales, and the
    // unequilibrated step is dominated by the scale of the columns.
    const std::vector<double> scales = column_scales(jacobian);
    numerics::Matrix equilibrated(jacobian.rows, jacobian.cols);
    for (std::size_t i = 0; i < jacobian.rows; ++i) {
      for (std::size_t j = 0; j < jacobian.cols; ++j) equilibrated(i, j) = jacobian(i, j) / scales[j];
    }
    const numerics::LeastSquaresResult solution =
        numerics::least_squares(equilibrated, residuals, 1e-12);
    outcome.rank = jacobian_rank(jacobian, 1e-12);
    if (!solution.converged || solution.solution.size() != values.size()) {
      outcome.reason = "the least squares step did not converge";
      break;
    }
    if (outcome.rank < values.size()) {
      outcome.reason = "the parameters are not fully estimable";
      break;
    }
    if (false) {
      // The finite difference check of the analytic Jacobian.
      const std::size_t pair = 0;
      for (std::size_t column = 0; column < values.size(); ++column) {
        const double step_size = 1e-7 * std::max(1.0, std::fabs(values[column]));
        std::vector<double> plus = values;
        std::vector<double> minus = values;
        plus[column] += step_size;
        minus[column] -= step_size;
        WcsParameters plus_parameters;
        WcsParameters minus_parameters;
        from_vector(plus, plus_parameters);
        from_vector(minus, minus_parameters);
        const Vec3 plus_direction = wcs_forward(plus_parameters, training[pair].pixel_x, training[pair].pixel_y);
        const Vec3 minus_direction = wcs_forward(minus_parameters, training[pair].pixel_x, training[pair].pixel_y);
        const Vec3& c = training[pair].sky;
        const double rra = std::atan2(c[1], c[0]);
        const double rdec = std::asin(std::clamp(c[2], -1.0, 1.0));
        const Vec3 e3{-std::sin(rra), std::cos(rra), 0.0};
        const Vec3 n3{-std::sin(rdec) * std::cos(rra), -std::sin(rdec) * std::sin(rra), std::cos(rdec)};
        const double den = c[0] * c[0] + c[1] * c[1] + c[2] * c[2];
        const double d_plus = ((plus_direction[0] - c[0]) * e3[0] + (plus_direction[1] - c[1]) * e3[1] +
                               (plus_direction[2] - c[2]) * e3[2]) / den;
        const double d_minus = ((minus_direction[0] - c[0]) * e3[0] + (minus_direction[1] - c[1]) * e3[1] +
                                (minus_direction[2] - c[2]) * e3[2]) / den;
        const double numeric = (d_plus - d_minus) / (2.0 * step_size);
        const double analytic = jacobian(2 * pair, column);
      }
    }
    // The residual is the model minus the catalogue, so the step of the Gauss
    // Newton iteration is subtracted.
    std::vector<double> step(values.size(), 0.0);
    for (std::size_t k = 0; k < values.size(); ++k) step[k] = -solution.solution[k] / scales[k];
    bool improved = false;
    for (int attempt = 0; attempt < 40; ++attempt) {
      const double scale = std::pow(0.5, attempt);
      std::vector<double> candidate = values;
      for (std::size_t k = 0; k < candidate.size(); ++k) {
        candidate[k] += scale * step[k];
      }
      WcsParameters candidate_parameters;
      from_vector(candidate, candidate_parameters);
      evaluate(candidate_parameters, training, candidate_residuals, nullptr);
      const double candidate_cost = cost_of(candidate_residuals);
      if (candidate_cost < cost) {
        const double change =
            std::abs(candidate[2] - values[2]) + std::abs(candidate[3] - values[3]);
        values = candidate;
        cost = candidate_cost;
        improved = true;
        if (change < 1e-12) iteration = max_iterations;
        break;
      }
    }
    if (!improved) {
      // The step of the Gauss Newton iteration is regularised: the plane and
      // the SIP terms are only weakly separated when the reference point is
      // free, and the undamped step is then dominated by the flat direction.
      bool damped = false;
      for (double lambda : {1e-10, 1e-8, 1e-6, 1e-4, 1e-2, 1.0}) {
        numerics::Matrix damped_design(equilibrated.rows + equilibrated.cols, equilibrated.cols);
        std::vector<double> damped_residuals(equilibrated.rows + equilibrated.cols, 0.0);
        for (std::size_t i = 0; i < equilibrated.rows; ++i) {
          for (std::size_t j = 0; j < equilibrated.cols; ++j) {
            damped_design(i, j) = equilibrated(i, j);
          }
          damped_residuals[i] = residuals[i];
        }
        for (std::size_t j = 0; j < equilibrated.cols; ++j) {
          damped_design(equilibrated.rows + j, j) = std::sqrt(lambda);
        }
        const numerics::LeastSquaresResult damped_solution =
            numerics::least_squares(damped_design, damped_residuals, 1e-12);
        if (!damped_solution.converged || damped_solution.solution.size() != values.size()) continue;
        for (std::size_t k = 0; k < values.size(); ++k) {
          step[k] = -damped_solution.solution[k] / scales[k];
        }
        for (int attempt = 0; attempt < 24; ++attempt) {
          const double factor = std::pow(0.5, attempt);
          std::vector<double> candidate = values;
          for (std::size_t k = 0; k < candidate.size(); ++k) candidate[k] += factor * step[k];
          WcsParameters candidate_parameters;
          from_vector(candidate, candidate_parameters);
          evaluate(candidate_parameters, training, candidate_residuals, nullptr);
          const double candidate_cost = cost_of(candidate_residuals);
          if (candidate_cost < cost) {
            values = candidate;
            cost = candidate_cost;
            improved = true;
            damped = true;
            break;
          }
        }
        if (damped) break;
      }
      if (!damped) {
        outcome.reason = "the line search did not reduce the residual";
        break;
      }
    }
    from_vector(values, parameters);
    outcome.converged = true;
  }

  evaluate(parameters, training, residuals, nullptr);
  outcome.rms_rad = residuals.empty() ? 0.0 : std::sqrt(cost_of(residuals) / residuals.size());
  outcome.parameters = parameters;
  outcome.estimable = outcome.rank >= values.size();
  if (!holdout.empty()) {
    std::vector<double> holdout_residuals;
    evaluate(parameters, holdout, holdout_residuals, nullptr);
    outcome.holdout_rms_rad =
        holdout_residuals.empty() ? 0.0 : std::sqrt(cost_of(holdout_residuals) / holdout_residuals.size());
  }
  if (!outcome.converged && outcome.reason.empty()) {
    outcome.reason = "the fit did not converge";
  }
  return outcome;
}

FitDiagnostics fit_diagnostics(const std::vector<MatchPair>& pairs, const WcsParameters& parameters) {
  FitDiagnostics diagnostics;
  evaluate(parameters, pairs, diagnostics.residuals, &diagnostics.jacobian);
  const std::size_t rows = diagnostics.jacobian.rows;
  const std::size_t columns = diagnostics.jacobian.cols;
  // The normal matrix is built from the equilibrated Jacobian: the parameters
  // of the model live on very different scales and the normal matrix of the
  // raw Jacobian is not usable in double precision.
  const std::vector<double> scales = column_scales(diagnostics.jacobian);
  numerics::Matrix normal(columns, columns);
  for (std::size_t i = 0; i < columns; ++i) {
    for (std::size_t j = 0; j < columns; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < rows; ++k) {
        sum += (diagnostics.jacobian(k, i) / scales[i]) * (diagnostics.jacobian(k, j) / scales[j]);
      }
      normal(i, j) = sum;
    }
  }
  diagnostics.covariance = numerics::Matrix(columns, columns);
  diagnostics.normal_inverse = numerics::Matrix(columns, columns);
  // The inverse of the normal matrix, column by column.
  std::vector<std::vector<double>> inverse(columns);
  bool invertible = rows > columns;
  for (std::size_t column = 0; column < columns && invertible; ++column) {
    std::vector<double> unit(columns, 0.0);
    unit[column] = 1.0;
    const std::optional<std::vector<double>> solution = numerics::cholesky_solve(normal, unit);
    if (!solution.has_value() || solution->size() != columns) {
      invertible = false;
      break;
    }
    inverse[column] = *solution;
  }
  if (invertible) {
    for (std::size_t i = 0; i < columns; ++i) {
      for (std::size_t j = 0; j < columns; ++j) {
        diagnostics.normal_inverse(i, j) = inverse[j][i];
      }
    }
    double residual_sum = 0.0;
    for (double value : diagnostics.residuals) residual_sum += value * value;
    const double scale = residual_sum / static_cast<double>(rows - columns);
    for (std::size_t i = 0; i < columns; ++i) {
      for (std::size_t j = 0; j < columns; ++j) {
        diagnostics.covariance(i, j) = inverse[j][i] * scale / (scales[i] * scales[j]);
      }
    }
    diagnostics.covariance_valid = true;
    diagnostics.reduced_chi_square = scale;
  }
  return diagnostics;
}

// The full fit with the escalation of the SIP order.
FitOutcome fit_wcs(const std::vector<MatchPair>& pairs, const WcsParameters& initial,
                    U32 requested_sip_order, U32 max_iterations) {
  FitOutcome outcome;

  // The held out group: the sources whose identifier hash falls on one group
  // of five.  The group with the most members is held out so that the training
  // set stays as large as possible.
  std::array<std::size_t, 5> group_sizes{0, 0, 0, 0, 0};
  for (const MatchPair& pair : pairs) {
    ++group_sizes[source_hash(pair.source_id) % 5];
  }
  std::size_t holdout_group = 0;
  for (std::size_t group = 1; group < 5; ++group) {
    if (group_sizes[group] > group_sizes[holdout_group]) holdout_group = group;
  }
  std::vector<MatchPair> training;
  std::vector<MatchPair> holdout;
  for (const MatchPair& pair : pairs) {
    if (source_hash(pair.source_id) % 5 == holdout_group) {
      holdout.push_back(pair);
    } else {
      training.push_back(pair);
    }
  }
  if (training.size() < 3 || holdout.empty()) {
    outcome.reason = "the training or the held out group is too small";
    return outcome;
  }

  // The plane without SIP.
  FitOutcome current = fit_order(training, holdout, initial, 0, max_iterations);
  outcome = current;
  if (!current.converged) return outcome;

  // The escalation of the SIP order.
  for (U32 order = 2; order <= requested_sip_order; ++order) {
    if (wcs_parameter_count(order) * 3 > training.size()) {
      outcome.reason = "the training set does not constrain every parameter";
      break;
    }
    FitOutcome candidate = fit_order(training, holdout, current.parameters, order, max_iterations);
    if (!candidate.converged) {
      outcome.reason = "the SIP fit did not converge: " + candidate.reason;
      break;
    }
    if (!candidate.estimable) {
      outcome.reason = "the SIP terms are not fully estimable";
      break;
    }
    if (!(candidate.holdout_rms_rad < current.holdout_rms_rad)) {
      outcome.reason = "the held out residual did not improve";
      break;
    }
    current = candidate;
    outcome = candidate;
  }
  outcome.reason = outcome.converged ? "the fit converged" : outcome.reason;
  return outcome;
}

}  // namespace acsd::sources
