#include "photometry/joint_fit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "photometry/spatial_basis.hpp"

namespace acsd::photometry {
namespace {

constexpr const char* kOrigin = "fit_photometry";

// ---------------------------------------------------------------------------
// Linear algebra helpers
// ---------------------------------------------------------------------------

std::vector<double> solve_lower(const numerics::Matrix& lower, const std::vector<double>& rhs) {
  const std::size_t n = lower.rows;
  std::vector<double> x(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    double sum = rhs[i];
    for (std::size_t j = 0; j < i; ++j) {
      sum -= lower(i, j) * x[j];
    }
    x[i] = sum / lower(i, i);
  }
  return x;
}

// Solves the symmetric positive (semi) definite system; falls back to the
// eigen pseudo inverse when the matrix is singular.
Result<std::vector<double>> solve_symmetric(const numerics::Matrix& matrix,
                                             const std::vector<double>& rhs, double rank_relative) {
  const std::size_t n = matrix.rows;
  numerics::Matrix factor;
  double min_diagonal = 0.0;
  if (n > 0 && numerics::cholesky_factor(matrix, factor, min_diagonal)) {
    const std::vector<double> intermediate = solve_lower(factor, rhs);
    // Back substitution with the transposed factor.
    std::vector<double> x(n, 0.0);
    for (std::size_t i = n; i-- > 0;) {
      double sum = intermediate[i];
      for (std::size_t j = i + 1; j < n; ++j) {
        sum -= factor(j, i) * x[j];
      }
      x[i] = sum / factor(i, i);
    }
    return model::ok_result(std::move(x));
  }
  numerics::EigenResult eigen = numerics::symmetric_eigen(matrix);
  if (!eigen.converged) {
    return model::fail<std::vector<double>>(Code::internal,
                                             "the normal matrix eigen decomposition did not converge",
                                             kOrigin, "the latent coefficients cannot be solved");
  }
  double scale = 0.0;
  for (double value : eigen.eigenvalues) {
    scale = std::max(scale, std::fabs(value));
  }
  const double tolerance = std::max(rank_relative * scale, std::numeric_limits<double>::min());
  for (double value : eigen.eigenvalues) {
    if (value < -tolerance) {
      return model::fail<std::vector<double>>(Code::invalid_covariance,
                                               "the normal matrix is not positive semi definite", kOrigin,
                                               "the latent coefficients cannot be solved");
    }
  }
  std::vector<double> x(n, 0.0);
  for (std::size_t mode = 0; mode < eigen.eigenvalues.size(); ++mode) {
    if (eigen.eigenvalues[mode] <= tolerance) {
      continue;
    }
    double projection = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      projection += eigen.eigenvectors(i, mode) * rhs[i];
    }
    projection /= eigen.eigenvalues[mode];
    for (std::size_t i = 0; i < n; ++i) {
      x[i] += projection * eigen.eigenvectors(i, mode);
    }
  }
  return model::ok_result(std::move(x));
}

// A whitening operator W with W^T W = Sigma^+ on the range of Sigma.
struct Whitener {
  numerics::Matrix matrix;     // n x n
  numerics::Matrix projector;  // null space projector (zero when full rank)
  bool has_null_space = false;
};

Result<Whitener> build_whitener(const numerics::Matrix& sigma, double rank_relative,
                                 std::string& note) {
  Whitener out;
  const std::size_t n = sigma.rows;
  out.matrix = numerics::Matrix(n, n);
  out.projector = numerics::Matrix(n, n);
  if (n == 0) {
    return model::ok_result(std::move(out));
  }
  numerics::Matrix factor;
  double min_diagonal = 0.0;
  if (numerics::cholesky_factor(sigma, factor, min_diagonal)) {
    for (std::size_t column = 0; column < n; ++column) {
      std::vector<double> unit(n, 0.0);
      unit[column] = 1.0;
      const std::vector<double> solved = solve_lower(factor, unit);
      for (std::size_t row = 0; row < n; ++row) {
        out.matrix(row, column) = solved[row];
      }
    }
    return model::ok_result(std::move(out));
  }
  // Not positive definite: use the symmetric eigen decomposition.  A
  // significantly negative eigenvalue is an invalid covariance; a zero mode is
  // an exact constraint that is projected out (the consistency of the data
  // with that constraint is checked by the caller).
  numerics::EigenResult eigen = numerics::symmetric_eigen(sigma);
  if (!eigen.converged) {
    return model::fail<Whitener>(Code::internal, "the covariance eigen decomposition did not converge",
                                  kOrigin, "the covariance cannot be whitened");
  }
  double scale = 0.0;
  for (double value : eigen.eigenvalues) {
    scale = std::max(scale, std::fabs(value));
  }
  const double tolerance = std::max(rank_relative * scale, std::numeric_limits<double>::min());
  std::vector<std::size_t> positive;
  std::vector<std::size_t> null_modes;
  for (std::size_t i = 0; i < eigen.eigenvalues.size(); ++i) {
    if (eigen.eigenvalues[i] < -tolerance) {
      return model::fail<Whitener>(Code::invalid_covariance,
                                    "the covariance has a significantly negative eigenvalue", kOrigin,
                                    "the covariance cannot be whitened");
    }
    if (eigen.eigenvalues[i] > tolerance) {
      positive.push_back(i);
    } else {
      null_modes.push_back(i);
    }
  }
  if (positive.empty()) {
    return model::fail<Whitener>(Code::invalid_covariance, "the covariance has no positive direction",
                                  kOrigin, "the covariance cannot be whitened");
  }
  // W = V1 Lambda1^{-1/2} V1^T
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < n; ++column) {
      double sum = 0.0;
      for (std::size_t mode : positive) {
        sum += eigen.eigenvectors(row, mode) * eigen.eigenvectors(column, mode) /
               std::sqrt(eigen.eigenvalues[mode]);
      }
      out.matrix(row, column) = sum;
    }
  }
  for (std::size_t mode : null_modes) {
    for (std::size_t row = 0; row < n; ++row) {
      for (std::size_t column = 0; column < n; ++column) {
        out.projector(row, column) +=
            eigen.eigenvectors(row, mode) * eigen.eigenvectors(column, mode);
      }
    }
    out.has_null_space = true;
    note =
        "a covariance mode was classified as null and projected out; the consistency of the data "
        "with that exact constraint was checked against the numerical enclosures";
  }
  return model::ok_result(std::move(out));
}

Result<std::vector<double>> apply_covariance_vector(Context& context,
                                                     Ref<CovarianceAction> covariance,
                                                     const std::vector<double>& values) {
  if (!covariance) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no covariance supplied", kOrigin,
                                             "the covariance cannot be applied");
  }
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  const std::vector<std::byte> bytes = model::encode_f64(values);
  Result<ArrayRef> input =
      context.storage.copy(context, DType::f64, bytes, shape, model::unit_dimensionless(), Id());
  if (!ok(input)) {
    return model::fail<std::vector<double>>(input.code, "the covariance input cannot be stored",
                                             kOrigin, "the covariance cannot be applied");
  }
  Result<ArrayRef> mapped = covariance_apply(context, covariance, val(input));
  if (!ok(mapped)) {
    return model::fail<std::vector<double>>(mapped.code, "the covariance cannot be applied", kOrigin,
                                             "the covariance cannot be applied");
  }
  Result<ArrayView> view = context.storage.read(context, val(mapped), BlockRequest{0, shape[0]});
  if (!ok(view)) {
    return model::fail<std::vector<double>>(view.code, "the covariance output cannot be read", kOrigin,
                                             "the covariance cannot be applied");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<numerics::Matrix> dense_covariance(Context& context, Ref<CovarianceAction> covariance,
                                            std::size_t n) {
  numerics::Matrix out(n, n);
  std::vector<double> unit(n, 0.0);
  for (std::size_t column = 0; column < n; ++column) {
    std::fill(unit.begin(), unit.end(), 0.0);
    unit[column] = 1.0;
    Result<std::vector<double>> response = apply_covariance_vector(context, covariance, unit);
    if (!ok(response)) {
      return model::fail<numerics::Matrix>(response.code, "the covariance cannot be applied", kOrigin,
                                            "the covariance cannot be read");
    }
    for (std::size_t row = 0; row < n && row < val(response).size(); ++row) {
      out(row, column) = (val(response))[row];
    }
  }
  return model::ok_result(std::move(out));
}

double median_of(std::vector<double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const std::size_t n = values.size();
  if (n % 2 == 1) {
    return values[n / 2];
  }
  return 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

// ---------------------------------------------------------------------------
// The assembled joint problem
// ---------------------------------------------------------------------------

struct Problem {
  std::size_t frames = 0;
  std::size_t stars = 0;
  std::size_t terms = 0;
  std::size_t latent_dim = 0;
  std::size_t n_theta = 0;
  std::size_t n_meas = 0;
  std::size_t n_latent = 0;
  std::size_t n_rows = 0;

  std::vector<std::size_t> meas_frame;
  std::vector<std::size_t> meas_star;
  std::vector<double> meas_counts;
  std::vector<double> meas_variance;
  std::vector<double> meas_low;
  std::vector<double> meas_high;
  std::vector<bool> meas_enclosed;
  std::vector<std::vector<double>> meas_terms;  // basis terms per measurement
  std::vector<std::vector<double>> frame_v;
  std::vector<std::vector<double>> frame_dv;
  std::vector<std::vector<double>> gaia;  // measured XP coefficients per star

  std::vector<std::vector<std::size_t>> frame_meas;  // measurement indices per frame
  std::vector<std::vector<std::size_t>> star_meas;   // measurement indices per star

  Whitener counts_whitener;
  std::vector<Whitener> gaia_whitener;  // one per star
  bool counts_couple_sources = false;
};

struct Evaluation {
  std::vector<double> amplitude;  // A_sk per measurement
  std::vector<double> model;      // m_sk per measurement
  std::vector<double> latent;     // profiled latent coefficients
  std::vector<double> residual;   // whitened residual
  numerics::Matrix jacobian;      // whitened jacobian (optional)
  double chi_square = 0.0;
};

// Solves the profiled latent coefficients for a given theta.
//
// The model is linear in the latent block, so the profiled optimum is the
// solution of a linear least squares problem.  When the joint counts
// covariance does not couple different sources the problem splits into one
// small system per source, which is exactly the per source Schur elimination
// prescribed by the science document.  Otherwise the dense joint system is
// solved (bounded by the dense budget).
Result<std::vector<double>> solve_latent(const Problem& problem, const std::vector<double>& theta,
                                          const FitSettings& settings,
                                          std::vector<double>& amplitude) {
  const std::size_t d = problem.latent_dim;
  amplitude.assign(problem.n_meas, 0.0);
  for (std::size_t i = 0; i < problem.n_meas; ++i) {
    const std::size_t frame = problem.meas_frame[i];
    double exponent = 0.0;
    for (std::size_t term = 0; term < problem.terms; ++term) {
      exponent += theta[frame * problem.terms + term] * problem.meas_terms[i][term];
    }
    if (exponent > 700.0) {
      return model::fail<std::vector<double>>(Code::nonconverged,
                                               "the photometric scale overflowed the exponential",
                                               kOrigin, "the latent coefficients cannot be solved");
    }
    if (exponent < -700.0) {
      exponent = -700.0;
    }
    amplitude[i] = std::exp(exponent);
  }

  std::vector<double> latent(problem.n_latent, 0.0);

  if (!problem.counts_couple_sources) {
    // Per source elimination.
    for (std::size_t star = 0; star < problem.stars; ++star) {
      const std::vector<std::size_t>& rows = problem.star_meas[star];
      const std::size_t m = rows.size();
      // Inverse covariance block of the counts rows of this source.
      numerics::Matrix inverse(m, m);
      for (std::size_t r = 0; r < m; ++r) {
        for (std::size_t c = 0; c < m; ++c) {
          double sum = 0.0;
          for (std::size_t k = 0; k < m; ++k) {
            sum += problem.counts_whitener.matrix(rows[k], rows[r]) *
                   problem.counts_whitener.matrix(rows[k], rows[c]);
          }
          inverse(r, c) = sum;
        }
      }
      // V_s (m x d) and the weighted data.
      numerics::Matrix normal(d, d);
      std::vector<double> rhs(d, 0.0);
      for (std::size_t r = 0; r < m; ++r) {
        const std::size_t i = rows[r];
        const std::vector<double>& v = problem.frame_v[problem.meas_frame[i]];
        // The normal equations of the latent block:
        //   (A^2 V V^T Sigma^-1 + G^T G) c = A V Sigma^-1 C + G^T G c_Gaia
        // The right hand side carries the amplitude once, the normal matrix
        // carries it twice.
        const double scale = inverse(r, r) * amplitude[i];
        for (std::size_t a = 0; a < d; ++a) {
          rhs[a] += scale * v[a] * problem.meas_counts[i];
          for (std::size_t b = 0; b < d; ++b) {
            normal(a, b) += scale * amplitude[i] * v[a] * v[b];
          }
        }
      }
      // The XP measurement block: G^T G c_Gaia.
      const Whitener& gaia_whitener = problem.gaia_whitener[star];
      for (std::size_t a = 0; a < d; ++a) {
        for (std::size_t b = 0; b < d; ++b) {
          double gram = 0.0;
          double data = 0.0;
          for (std::size_t k = 0; k < d; ++k) {
            gram += gaia_whitener.matrix(k, a) * gaia_whitener.matrix(k, b);
            data += gaia_whitener.matrix(k, a) * gaia_whitener.matrix(k, b) * problem.gaia[star][k];
          }
          normal(a, b) += gram;
          rhs[a] += data;
        }
      }
      Result<std::vector<double>> solution =
          solve_symmetric(normal, rhs, settings.rank_relative);
      if (!ok(solution)) {
        return model::fail<std::vector<double>>(solution.code,
                                                 "the latent spectral coefficients could not be solved",
                                                 kOrigin, "the latent coefficients cannot be solved");
      }
      for (std::size_t a = 0; a < d; ++a) {
        latent[star * d + a] = (val(solution))[a];
      }
    }
    return model::ok_result(std::move(latent));
  }

  // Dense joint solve (the counts covariance couples different sources).
  const std::size_t n_rows = problem.n_rows;
  numerics::Matrix design(n_rows, problem.n_latent);
  std::vector<double> rhs(n_rows, 0.0);
  for (std::size_t star = 0; star < problem.stars; ++star) {
    const std::vector<std::size_t>& rows = problem.star_meas[star];
    for (std::size_t r = 0; r < rows.size(); ++r) {
      const std::size_t i = rows[r];
      const std::vector<double>& v = problem.frame_v[problem.meas_frame[i]];
      for (std::size_t a = 0; a < d; ++a) {
        double sum = 0.0;
        for (std::size_t k = 0; k < problem.n_meas; ++k) {
          const double entry = (problem.meas_star[k] == star) ? amplitude[k] * v[a] : 0.0;
          sum += problem.counts_whitener.matrix(k, rows[r]) * entry;
        }
        design(rows[r], star * d + a) = -sum;
      }
    }
    const std::size_t row0 = problem.n_meas + star * d;
    const Whitener& gaia_whitener = problem.gaia_whitener[star];
    for (std::size_t i = 0; i < d; ++i) {
      for (std::size_t a = 0; a < d; ++a) {
        double sum = 0.0;
        for (std::size_t k = 0; k < d; ++k) {
          const double entry = (k == i) ? 1.0 : 0.0;
          sum += gaia_whitener.matrix(k, a) * entry;
        }
        design(row0 + i, star * d + a) = -sum;
      }
    }
  }
  for (std::size_t i = 0; i < problem.n_meas; ++i) {
    double sum = 0.0;
    for (std::size_t k = 0; k < problem.n_meas; ++k) {
      sum += problem.counts_whitener.matrix(i, k) * problem.meas_counts[k];
    }
    rhs[i] = -sum;
  }
  for (std::size_t star = 0; star < problem.stars; ++star) {
    const std::size_t row0 = problem.n_meas + star * d;
    const Whitener& gaia_whitener = problem.gaia_whitener[star];
    for (std::size_t i = 0; i < d; ++i) {
      double sum = 0.0;
      for (std::size_t k = 0; k < d; ++k) {
        sum += gaia_whitener.matrix(k, i) * problem.gaia[star][k];
      }
      rhs[row0 + i] = -sum;
    }
  }
  numerics::LeastSquaresResult solution = numerics::least_squares(design, rhs, settings.rank_relative);
  if (!solution.converged || solution.solution.size() != problem.n_latent) {
    return model::fail<std::vector<double>>(Code::nonconverged,
                                             "the latent spectral coefficients could not be solved",
                                             kOrigin, "the latent coefficients cannot be solved");
  }
  return model::ok_result(std::move(solution.solution));
}

// Evaluates the profiled objective at a given theta.
Result<Evaluation> evaluate(const Problem& problem, const std::vector<double>& theta,
                            const FitSettings& settings, bool with_jacobian,
                            const std::vector<double>* fixed_latent = nullptr) {
  Evaluation out;
  if (fixed_latent != nullptr) {
    // The nuisance block is held at the profiled optimum: the numerical
    // enclosure of the parameters describes the numerical uncertainty of the
    // measured data, not the statistical uncertainty of the nuisance.
    out.latent = *fixed_latent;
    out.amplitude.assign(problem.n_meas, 0.0);
    for (std::size_t i = 0; i < problem.n_meas; ++i) {
      const std::size_t frame = problem.meas_frame[i];
      double exponent = 0.0;
      for (std::size_t term = 0; term < problem.terms; ++term) {
        exponent += theta[frame * problem.terms + term] * problem.meas_terms[i][term];
      }
      if (exponent > 700.0) {
        return model::fail<Evaluation>(Code::nonconverged,
                                       "the photometric scale overflowed the exponential", kOrigin,
                                       "the joint objective cannot be evaluated");
      }
      if (exponent < -700.0) {
        exponent = -700.0;
      }
      out.amplitude[i] = std::exp(exponent);
    }
  } else {
    Result<std::vector<double>> latent = solve_latent(problem, theta, settings, out.amplitude);
    if (!ok(latent)) {
      return model::fail<Evaluation>(latent.code,
                                     "the latent spectral coefficients could not be solved", kOrigin,
                                     "the joint objective cannot be evaluated");
    }
    out.latent = val(latent);
  }
  const std::size_t d = problem.latent_dim;

  // Unwhitened residuals.
  std::vector<double> raw(problem.n_rows, 0.0);
  out.model.assign(problem.n_meas, 0.0);
  for (std::size_t i = 0; i < problem.n_meas; ++i) {
    const std::size_t frame = problem.meas_frame[i];
    const std::size_t star = problem.meas_star[i];
    const std::vector<double>& v = problem.frame_v[frame];
    double flux = 0.0;
    for (std::size_t j = 0; j < d; ++j) {
      flux += v[j] * out.latent[star * d + j];
    }
    out.model[i] = out.amplitude[i] * flux;
    raw[i] = problem.meas_counts[i] - out.model[i];
  }
  for (std::size_t star = 0; star < problem.stars; ++star) {
    for (std::size_t j = 0; j < d; ++j) {
      raw[problem.n_meas + star * d + j] = problem.gaia[star][j] - out.latent[star * d + j];
    }
  }

  // Whiten the residual.
  out.residual.assign(problem.n_rows, 0.0);
  for (std::size_t i = 0; i < problem.n_meas; ++i) {
    double sum = 0.0;
    for (std::size_t k = 0; k < problem.n_meas; ++k) {
      sum += problem.counts_whitener.matrix(i, k) * raw[k];
    }
    out.residual[i] = sum;
  }
  for (std::size_t star = 0; star < problem.stars; ++star) {
    const std::size_t row0 = problem.n_meas + star * d;
    const Whitener& whitener = problem.gaia_whitener[star];
    for (std::size_t i = 0; i < d; ++i) {
      double sum = 0.0;
      for (std::size_t k = 0; k < d; ++k) {
        sum += whitener.matrix(i, k) * raw[row0 + k];
      }
      out.residual[row0 + i] = sum;
    }
  }

  // Optional Huber weights on the whitened residual.  The same weight is
  // applied to the jacobian row, so that the iteration is a genuine
  // iteratively reweighted least squares step.
  std::vector<double> row_weight(out.residual.size(), 1.0);
  if (settings.huber && settings.huber_delta > 0.0) {
    for (std::size_t i = 0; i < out.residual.size(); ++i) {
      const double magnitude = std::fabs(out.residual[i]);
      if (magnitude > settings.huber_delta) {
        row_weight[i] = std::sqrt(settings.huber_delta / magnitude);
        out.residual[i] *= row_weight[i];
      }
    }
  }

  double chi_square = 0.0;
  for (double value : out.residual) {
    chi_square += value * value;
  }
  out.chi_square = chi_square;

  if (!with_jacobian) {
    return model::ok_result(std::move(out));
  }

  // Whitened jacobian: rows [counts, gaia], columns [theta, latent].
  const std::size_t n_columns = problem.n_theta + problem.n_latent;
  out.jacobian = numerics::Matrix(problem.n_rows, n_columns);
  const numerics::Matrix& counts_whitener = problem.counts_whitener.matrix;
  for (std::size_t row = 0; row < problem.n_meas; ++row) {
    // Theta columns: only the measurements of the frame contribute.
    for (std::size_t frame = 0; frame < problem.frames; ++frame) {
      const std::vector<std::size_t>& rows = problem.frame_meas[frame];
      for (std::size_t term = 0; term < problem.terms; ++term) {
        double sum = 0.0;
        for (std::size_t k : rows) {
          sum += counts_whitener(row, k) * (-out.model[k] * problem.meas_terms[k][term]);
        }
        out.jacobian(row, frame * problem.terms + term) = sum;
      }
    }
    // Latent columns: only the measurements of the star contribute.
    for (std::size_t star = 0; star < problem.stars; ++star) {
      const std::vector<std::size_t>& rows = problem.star_meas[star];
      for (std::size_t j = 0; j < d; ++j) {
        double sum = 0.0;
        for (std::size_t k : rows) {
          const std::vector<double>& v = problem.frame_v[problem.meas_frame[k]];
          sum += counts_whitener(row, k) * (-out.amplitude[k] * v[j]);
        }
        out.jacobian(row, problem.n_theta + star * d + j) = sum;
      }
    }
  }
  for (std::size_t star = 0; star < problem.stars; ++star) {
    const std::size_t row0 = problem.n_meas + star * d;
    const numerics::Matrix& whitener = problem.gaia_whitener[star].matrix;
    for (std::size_t i = 0; i < d; ++i) {
      for (std::size_t j = 0; j < d; ++j) {
        out.jacobian(row0 + i, problem.n_theta + star * d + j) = -whitener(i, j);
      }
    }
  }
  // Apply the Huber row weights to the jacobian as well.
  for (std::size_t row = 0; row < problem.n_rows; ++row) {
    if (row_weight[row] == 1.0) {
      continue;
    }
    for (std::size_t column = 0; column < n_columns; ++column) {
      out.jacobian(row, column) *= row_weight[row];
    }
  }
  return model::ok_result(std::move(out));
}

// The reduced (profiled) jacobian of the theta block:
//   R = J_theta - J_c (J_c^T J_c)^{-1} J_c^T J_theta
numerics::Matrix reduced_jacobian(const numerics::Matrix& jacobian, std::size_t n_theta,
                                  std::size_t n_latent, double rank_relative) {
  const std::size_t rows = jacobian.rows;
  // The reduced jacobian has one row per observation, like the jacobian it
  // reduces: only the first n_theta rows would drop the observations of the
  // other frames from the rank and the step.
  numerics::Matrix reduced(rows, n_theta);
  if (n_latent == 0) {
    for (std::size_t i = 0; i < n_theta; ++i) {
      for (std::size_t j = 0; j < n_theta; ++j) {
        reduced(i, j) = jacobian(i, j);
      }
    }
    return reduced;
  }
  // Normal matrix of the latent block and the mixed block.
  numerics::Matrix latent_normal(n_latent, n_latent);
  numerics::Matrix mixed(n_latent, n_theta);
  for (std::size_t i = 0; i < n_latent; ++i) {
    for (std::size_t j = 0; j < n_latent; ++j) {
      double sum = 0.0;
      for (std::size_t r = 0; r < rows; ++r) {
        sum += jacobian(r, n_theta + i) * jacobian(r, n_theta + j);
      }
      latent_normal(i, j) = sum;
    }
    for (std::size_t j = 0; j < n_theta; ++j) {
      double sum = 0.0;
      for (std::size_t r = 0; r < rows; ++r) {
        sum += jacobian(r, n_theta + i) * jacobian(r, j);
      }
      mixed(i, j) = sum;
    }
  }
  // Solve (J_c^T J_c) x = J_c^T J_theta by a symmetric solve.
  std::vector<double> flat(n_latent * n_theta, 0.0);
  for (std::size_t i = 0; i < n_latent; ++i) {
    for (std::size_t j = 0; j < n_theta; ++j) {
      flat[i * n_theta + j] = mixed(i, j);
    }
  }
  std::vector<std::vector<double>> columns(n_theta);
  for (std::size_t j = 0; j < n_theta; ++j) {
    std::vector<double> rhs(n_latent, 0.0);
    for (std::size_t i = 0; i < n_latent; ++i) {
      rhs[i] = flat[i * n_theta + j];
    }
    Result<std::vector<double>> solution = solve_symmetric(latent_normal, rhs, rank_relative);
    if (!ok(solution)) {
      // A singular latent block means the latent is not identifiable; the
      // caller reports the rank deficiency through the rank of the reduced
      // jacobian, which stays zero here.
      return numerics::Matrix(n_theta, n_theta);
    }
    columns[j] = val(solution);
  }
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t j = 0; j < n_theta; ++j) {
      double sum = jacobian(row, j);
      for (std::size_t l = 0; l < n_latent; ++l) {
        sum -= jacobian(row, n_theta + l) * columns[j][l];
      }
      reduced(row, j) = sum;
    }
  }
  return reduced;
}

// The inverse of the normal matrix of the reduced jacobian.
//
// The pseudo inverse R^+ of the reduced jacobian comes from the base
// (numerics::pseudo_inverse), so that the threshold of the singular values is
// the same everywhere in the tree.  For a reduced jacobian with a full column
// rank the inverse of the normal matrix is R^+ (R^+)^T, so the product below
// is the only place where the two are combined.
numerics::Matrix gram_inverse(const numerics::Matrix& pseudo) {
  const std::size_t columns = pseudo.rows;
  const std::size_t rows = pseudo.cols;
  numerics::Matrix out(columns, columns);
  for (std::size_t i = 0; i < columns; ++i) {
    for (std::size_t j = 0; j < columns; ++j) {
      double sum = 0.0;
      for (std::size_t l = 0; l < rows; ++l) {
        sum += pseudo(i, l) * pseudo(j, l);
      }
      out(i, j) = sum;
    }
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public entry of the module
// ---------------------------------------------------------------------------

Result<FitResult> run_joint_fit(Context& context, const std::vector<FrameInput>& frames,
                                 const std::vector<StarInput>& stars, const Basis& basis,
                                 const std::vector<double>& weighted_mean,
                                 const FitSettings& settings) {
  FitResult result;
  if (frames.empty() || stars.empty()) {
    result.failure = "the joint fit needs at least one frame and one star";
    return model::ok_result(std::move(result));
  }

  Problem problem;
  problem.frames = frames.size();
  problem.stars = stars.size();
  problem.terms = basis_term_count(basis);
  problem.n_theta = problem.frames * problem.terms;
  problem.latent_dim = stars.front().gaia.size();
  if (problem.latent_dim == 0) {
    result.failure = "the XP latent has no coefficient";
    return model::ok_result(std::move(result));
  }
  for (const StarInput& star : stars) {
    if (star.gaia.size() != problem.latent_dim) {
      result.failure = "the XP latent coefficients have inconsistent lengths";
      return model::ok_result(std::move(result));
    }
  }
  problem.n_latent = problem.stars * problem.latent_dim;

  // Measurement table.
  std::vector<double> terms_buffer;
  for (std::size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
    const FrameInput& frame = frames[frame_index];
    problem.frame_v.push_back(frame.v);
    problem.frame_dv.push_back(frame.dv);
    problem.frame_meas.emplace_back();
    for (std::size_t m = 0; m < frame.counts.size(); ++m) {
      problem.meas_frame.push_back(frame_index);
      problem.meas_star.push_back(frame.star[m]);
      problem.meas_counts.push_back(frame.counts[m]);
      problem.meas_variance.push_back(frame.variance[m]);
      problem.meas_low.push_back(frame.flux_low[m]);
      problem.meas_high.push_back(frame.flux_high[m]);
      problem.meas_enclosed.push_back(!frame.enclosure_missing &&
                                      frame.flux_low[m] <= frame.counts[m] &&
                                      frame.counts[m] <= frame.flux_high[m]);
      basis_terms(basis, weighted_mean, frame.position_x[m], frame.position_y[m], terms_buffer);
      problem.meas_terms.push_back(terms_buffer);
      problem.frame_meas[frame_index].push_back(problem.meas_counts.size() - 1);
    }
  }
  problem.n_meas = problem.meas_counts.size();
  problem.n_rows = problem.n_meas + problem.n_latent;
  problem.star_meas.assign(problem.stars, {});
  for (std::size_t i = 0; i < problem.n_meas; ++i) {
    problem.star_meas[problem.meas_star[i]].push_back(i);
  }
  for (const StarInput& star : stars) {
    problem.gaia.push_back(star.gaia);
  }

  if (problem.n_rows * (problem.n_theta + problem.n_latent) > settings.max_dense_elements) {
    result.failure =
        "the dense joint system exceeds the configured element budget; the sparse route is not "
        "implemented in this release";
    return model::ok_result(std::move(result));
  }

  // ---------------------------------------------------------------------
  // The fixed joint Gaussian covariance of [counts, c_Gaia].
  // ---------------------------------------------------------------------
  numerics::Matrix counts_covariance(problem.n_meas, problem.n_meas);
  bool joint_counts_covariance = false;
  for (std::size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
    const FrameInput& frame = frames[frame_index];
    if (!frame.measurements || !frame.measurements->joint_noise) {
      continue;
    }
    const Ref<CovarianceAction>& noise = frame.measurements->joint_noise;
    const std::size_t domain = noise->input_output.domain ? noise->input_output.domain->size : 0;
    if (domain != problem.n_meas) {
      continue;
    }
    Result<numerics::Matrix> dense = dense_covariance(context, noise, problem.n_meas);
    if (!ok(dense)) {
      continue;
    }
    counts_covariance = val(dense);
    joint_counts_covariance = true;
    break;
  }
  if (!joint_counts_covariance) {
    // The per measurement variance is used as the diagonal of the joint
    // covariance.  A missing variance is never replaced by zero.
    for (std::size_t i = 0; i < problem.n_meas; ++i) {
      counts_covariance(i, i) = problem.meas_variance[i];
    }
  }
  std::string whitener_note;
  Result<Whitener> counts_whitener =
      build_whitener(counts_covariance, settings.rank_relative, whitener_note);
  if (!ok(counts_whitener)) {
    result.failure = "the joint counts covariance cannot be whitened";
    return model::ok_result(std::move(result));
  }
  problem.counts_whitener = val(counts_whitener);
  if (!whitener_note.empty()) {
    result.notes.push_back(whitener_note);
  }

  for (std::size_t star = 0; star < problem.stars; ++star) {
    numerics::Matrix covariance(problem.latent_dim, problem.latent_dim);
    bool dense = false;
    if (stars[star].noise) {
      const std::size_t domain =
          stars[star].noise->input_output.domain ? stars[star].noise->input_output.domain->size : 0;
      if (domain == problem.latent_dim) {
        Result<numerics::Matrix> extracted =
            dense_covariance(context, stars[star].noise, problem.latent_dim);
        if (ok(extracted)) {
          covariance = val(extracted);
          dense = true;
        }
      }
    }
    if (!dense) {
      // Equal weights; the missing XP covariance is recorded as an unknown
      // model term rather than as a zero variance.
      for (std::size_t i = 0; i < problem.latent_dim; ++i) {
        covariance(i, i) = 1.0;
      }
      result.unknown_terms.push_back("the XP coefficient covariance of source " +
                                     std::to_string(stars[star].source_id) + " is unknown");
    }
    std::string note;
    Result<Whitener> whitener = build_whitener(covariance, settings.rank_relative, note);
    if (!ok(whitener)) {
      result.failure = "the XP coefficient covariance cannot be whitened";
      return model::ok_result(std::move(result));
    }
    problem.gaia_whitener.push_back(val(whitener));
    if (!note.empty()) {
      result.notes.push_back(note);
    }
  }

  // Does the joint counts covariance couple different sources?  If it does,
  // the per source elimination of the latent block is not valid.
  {
    double diagonal = 0.0;
    double off_block = 0.0;
    for (std::size_t i = 0; i < problem.n_meas; ++i) {
      for (std::size_t k = 0; k < problem.n_meas; ++k) {
        const double value = std::fabs(problem.counts_whitener.matrix(i, k));
        if (i == k) {
          diagonal = std::max(diagonal, value);
        } else if (problem.meas_star[i] != problem.meas_star[k]) {
          off_block = std::max(off_block, value);
        }
      }
    }
    problem.counts_couple_sources = off_block > 1e-12 * std::max(diagonal, 1e-300);
    if (problem.counts_couple_sources) {
      result.notes.push_back(
          "the joint counts covariance couples different sources; the latent block was eliminated "
          "with the dense joint system");
    }
  }

  // The unknown measurement variances are recorded: the fit falls back to
  // equal weights and the result stays conditional.
  for (const FrameInput& frame : frames) {
    if (!frame.variance_known) {
      result.unknown_terms.push_back(
          "the measurement variance of frame " + frame.frame +
          " is unknown; the joint fit used equal weights for the affected measurements");
      break;
    }
  }

  // ---------------------------------------------------------------------
  // Initial values
  // ---------------------------------------------------------------------
  std::vector<double> theta(problem.n_theta, 0.0);
  for (std::size_t frame_index = 0; frame_index < problem.frames; ++frame_index) {
    const FrameInput& frame = frames[frame_index];
    std::vector<double> ratios;
    for (std::size_t m = 0; m < frame.counts.size(); ++m) {
      const std::size_t star = frame.star[m];
      double flux = 0.0;
      for (std::size_t j = 0; j < problem.latent_dim; ++j) {
        flux += frame.v[j] * stars[star].gaia[j];
      }
      if (frame.counts[m] > 0.0 && flux > 0.0) {
        ratios.push_back(std::log(frame.counts[m] / flux));
      }
    }
    theta[frame_index * problem.terms] = ratios.empty() ? 0.0 : median_of(ratios);
  }

  // ---------------------------------------------------------------------
  // Gauss-Newton with Armijo backtracking
  // ---------------------------------------------------------------------
  Evaluation current;
  {
    Result<Evaluation> evaluated = evaluate(problem, theta, settings, true);
    if (!ok(evaluated)) {
      result.failure = "the joint objective cannot be evaluated at the initial values";
      return model::ok_result(std::move(result));
    }
    current = val(evaluated);
  }

  bool converged = false;
  std::string failure;
  std::vector<double> final_theta = theta;
  std::vector<double> final_latent = current.latent;
  numerics::Matrix reduced;

  for (std::uint32_t iteration = 0; iteration < settings.max_outer_iterations; ++iteration) {
    result.iterations = static_cast<int>(iteration) + 1;
    if (context.cancellation.requested()) {
      failure = "the call was cancelled";
      break;
    }

    // Score: the gradient of chi^2/2 with respect to theta at the profiled
    // latent.
    std::vector<double> score(problem.n_theta, 0.0);
    std::vector<double> column_scale(problem.n_theta, 0.0);
    for (std::size_t column = 0; column < problem.n_theta; ++column) {
      double gradient = 0.0;
      double norm = 0.0;
      for (std::size_t row = 0; row < problem.n_rows; ++row) {
        const double value = current.jacobian(row, column);
        gradient += value * current.residual[row];
        norm += value * value;
      }
      // The score is the gradient of chi^2/2 with respect to the parameter:
      // the jacobian already carries the sign of the residual derivative.
      score[column] = gradient;
      column_scale[column] = std::sqrt(norm);
    }

    reduced = reduced_jacobian(current.jacobian, problem.n_theta, problem.n_latent,
                               settings.rank_relative);
    numerics::SvdResult svd = numerics::jacobi_svd(reduced, settings.rank_relative);
    numerics::RankReport rank = numerics::classify_rank(svd, problem.n_theta, settings.rank_relative);
    if (rank.rank < problem.n_theta) {
      result.identifiable = false;
      result.rank = rank;
      result.theta = theta;
      result.latent = current.latent;
      result.chi_square = current.chi_square;
      result.failure = "the joint design matrix is rank deficient in the scale parameters";
      return model::ok_result(std::move(result));
    }

    // Gauss-Newton step: delta = -(R^T R)^{-1} score.
    // The pseudo inverse of the reduced jacobian comes from the base, so that
    // the threshold of the singular values is the same everywhere.  Only the
    // modes that the rank classification accepts enter the inverse: a
    // direction below the classification threshold would otherwise blow the
    // step up by the inverse of a numerically zero singular value.
    // R^+ (R^+)^T = (R^T R)^{-1} for a full column rank R, so the same matrix
    // serves the step and the parameter covariance.
    const numerics::Matrix inverse =
        numerics::pseudo_inverse(svd, problem.n_theta, settings.rank_relative);
    const numerics::Matrix normal_inverse = gram_inverse(inverse);
    std::vector<double> step(problem.n_theta, 0.0);
    for (std::size_t j = 0; j < problem.n_theta; ++j) {
      double sum = 0.0;
      for (std::size_t l = 0; l < problem.n_theta; ++l) {
        sum += normal_inverse(j, l) * score[l];
      }
      step[j] = -sum;
    }

    double scaled_gradient = 0.0;
    double scaled_step = 0.0;
    for (std::size_t j = 0; j < problem.n_theta; ++j) {
      scaled_gradient = std::max(scaled_gradient, std::fabs(score[j] * column_scale[j]));
      scaled_step = std::max(scaled_step, std::fabs(step[j] * column_scale[j]));
    }
    if (scaled_gradient <= settings.gradient_tolerance && scaled_step <= settings.step_tolerance) {
      converged = true;
      break;
    }

    // Armijo backtracking on the profiled objective.
    const double directional = 2.0 * [&] {
      double sum = 0.0;
      for (std::size_t j = 0; j < problem.n_theta; ++j) {
        sum += score[j] * step[j];
      }
      return sum;
    }();
    double factor = 1.0;
    bool accepted = false;
    Evaluation trial;
    if (iteration == 0) {
    }
    for (int backtrack = 0; backtrack < settings.max_backtracks; ++backtrack) {
      std::vector<double> candidate(problem.n_theta, 0.0);
      for (std::size_t j = 0; j < problem.n_theta; ++j) {
        candidate[j] = theta[j] + factor * step[j];
      }
      Result<Evaluation> evaluated = evaluate(problem, candidate, settings, false);
      if (ok(evaluated)) {
        const double change = val(evaluated).chi_square - current.chi_square;
        if (change <= settings.armijo * factor * directional) {
          trial = val(evaluated);
          accepted = true;
          theta = candidate;
          break;
        }
      }
      factor *= 0.5;
    }
    if (!accepted) {
      failure = "the Armijo backtracking did not find a decreasing step";
      break;
    }
    current = trial;
    Result<Evaluation> refreshed = evaluate(problem, theta, settings, true);
    if (!ok(refreshed)) {
      failure = "the joint objective cannot be evaluated after the update";
      break;
    }
    current = val(refreshed);
  }

  result.converged = converged;
  result.theta = theta;
  result.latent = current.latent;
  result.chi_square = current.chi_square;
  if (!converged && failure.empty()) {
    failure = "the outer iteration limit was reached before the stopping criterion was met";
  }
  result.failure = failure;
  if (!converged) {
    // A non converged fit never publishes an arbitrary scale.
    result.parameter_box_certified = false;
    return model::ok_result(std::move(result));
  }

  // ---------------------------------------------------------------------
  // Rank report, covariance and the numerical parameter box
  // ---------------------------------------------------------------------
  {
    numerics::SvdResult svd = numerics::jacobi_svd(reduced, settings.rank_relative);
    result.rank = numerics::classify_rank(svd, problem.n_theta, settings.rank_relative);
    if (result.rank.rank < problem.n_theta) {
      result.identifiable = false;
      result.failure = "the joint design matrix is rank deficient in the scale parameters";
      return model::ok_result(std::move(result));
    }
    // Covariance of theta: the inverse of the normal matrix of the reduced
    // jacobian, (R^T R)^{-1} = R^+ (R^+)^T, with the pseudo inverse of the
    // base.  The threshold of the singular values is therefore the same
    // everywhere in the tree.
    const numerics::Matrix inverse =
        numerics::pseudo_inverse(svd, problem.n_theta, settings.rank_relative);
    const numerics::Matrix covariance = gram_inverse(inverse);
    result.covariance.assign(problem.n_theta * problem.n_theta, 0.0);
    for (std::size_t i = 0; i < problem.n_theta; ++i) {
      for (std::size_t j = 0; j < problem.n_theta; ++j) {
        result.covariance[i * problem.n_theta + j] = covariance(i, j);
      }
    }

    // -----------------------------------------------------------------
    // Numerical parameter box
    //
    // The box is the profile interval of the numerical data: for every
    // parameter the largest excursion whose profiled model prediction still
    // lies inside the numerical enclosure of the measured flux.  The
    // enclosures of the measured flux and of the response integration are
    // combined; a missing enclosure never becomes a zero radius.
    // -----------------------------------------------------------------
    bool all_enclosed = true;
    for (std::size_t i = 0; i < problem.n_meas; ++i) {
      if (!problem.meas_enclosed[i]) {
        all_enclosed = false;
      }
    }
    if (!all_enclosed) {
      result.unknown_terms.push_back(
          "a measured flux has no numerical enclosure; the parameter box cannot be certified");
      return model::ok_result(std::move(result));
    }

    // The numerical radius of every measurement.
    std::vector<double> radius(problem.n_meas, 0.0);
    for (std::size_t i = 0; i < problem.n_meas; ++i) {
      const std::size_t frame = problem.meas_frame[i];
      const std::size_t star = problem.meas_star[i];
      double model_radius = 0.0;
      for (std::size_t q = 0; q < problem.latent_dim; ++q) {
        model_radius += std::fabs(current.latent[star * problem.latent_dim + q]) *
                        problem.frame_dv[frame][q];
      }
      model_radius *= std::fabs(current.amplitude[i]);
      radius[i] = 0.5 * (problem.meas_high[i] - problem.meas_low[i]) + model_radius;
    }

    // The profiled model prediction of one measurement at a trial theta.
    auto prediction = [&](const std::vector<double>& candidate, std::size_t measurement) -> double {
      Result<Evaluation> evaluated =
          evaluate(problem, candidate, settings, false, &current.latent);
      if (!ok(evaluated)) {
        return std::nan("");
      }
      return val(evaluated).model[measurement];
    };

    // Bisection for the largest excursion that keeps the prediction of the
    // measurement inside its enclosure.
    auto excursion = [&](std::size_t parameter, std::size_t measurement, double sign,
                         double search) -> double {
      // The excursion of one parameter in the given direction: the largest
      // |t| whose profiled model prediction stays inside the numerical
      // enclosure of the measurement.  The signed function
      // f(t) = sign * (prediction - target) increases with t on both sides.
      std::vector<double> trial(problem.n_theta, 0.0);
      // The target is the fitted prediction moved by the numerical radius of
      // the measurement: the excursion is the change of the model that the
      // numerical enclosure of the data allows.
      const double target = current.model[measurement] + sign * radius[measurement];
      auto evaluate_at = [&](double t) {
        for (std::size_t j = 0; j < problem.n_theta; ++j) {
          trial[j] = theta[j];
        }
        trial[parameter] = theta[parameter] + sign * t;
        return prediction(trial, measurement);
      };
      double low = 0.0;
      double high = search;
      const double value_low = evaluate_at(low);
      if (!std::isfinite(value_low)) {
        return 0.0;
      }
      const double value_high = evaluate_at(high);
      if (!std::isfinite(value_high)) {
        return 0.0;
      }
      const double f_low = sign * (value_low - target);
      const double f_high = sign * (value_high - target);
      if (f_low * f_high > 0.0) {
        // The target is not bracketed inside the search range: the excursion
        // stays below the range, which is reported as the range itself.
        return high;
      }
      for (int iteration = 0; iteration < 60; ++iteration) {
        const double middle = 0.5 * (low + high);
        const double value = evaluate_at(middle);
        if (!std::isfinite(value)) {
          return 0.0;
        }
        const double f_middle = sign * (value - target);
        if (f_middle * f_low > 0.0) {
          low = middle;
        } else {
          high = middle;
        }
      }
      return high;
    };

    // The sensitivity of the model of every measurement to every parameter:
    // d m_i / d theta_j = m_i * h_i(term) inside the frame of the measurement.
    std::vector<std::vector<double>> sensitivity(problem.n_theta,
                                                 std::vector<double>(problem.n_meas, 0.0));
    for (std::size_t j = 0; j < problem.n_theta; ++j) {
      const std::size_t frame = j / problem.terms;
      const std::size_t term = j % problem.terms;
      for (std::size_t i = 0; i < problem.n_meas; ++i) {
        if (problem.meas_frame[i] != frame) {
          continue;
        }
        sensitivity[j][i] = std::fabs(current.model[i] * problem.meas_terms[i][term]);
      }
    }

    // The box is asymmetric: the excursion below and above the solution are
    // computed separately.
    std::vector<double> bound_lower(problem.n_theta, 0.0);
    std::vector<double> bound_upper(problem.n_theta, 0.0);
    U64 work = 0;
    for (std::size_t j = 0; j < problem.n_theta; ++j) {
      double largest = 0.0;
      for (std::size_t i = 0; i < problem.n_meas; ++i) {
        largest = std::max(largest, sensitivity[j][i]);
      }
      if (largest <= 0.0) {
        continue;
      }
      double upper = std::numeric_limits<double>::infinity();
      double lower = std::numeric_limits<double>::infinity();
      for (std::size_t i = 0; i < problem.n_meas; ++i) {
        if (sensitivity[j][i] < 0.25 * largest) {
          continue;
        }
        if (work > 20000) {
          result.unknown_terms.push_back(
              "the parameter box search exceeded its work budget; the box is not certified");
          return model::ok_result(std::move(result));
        }
        work += 100;
        const double derivative = sensitivity[j][i];
        const double search = derivative > 0.0 ? 4.0 * radius[i] / derivative : 1e-6;
        upper = std::min(upper, excursion(j, i, +1.0, search));
        lower = std::min(lower, excursion(j, i, -1.0, search));
      }
      if (!std::isfinite(upper) || !std::isfinite(lower)) {
        continue;
      }
      bound_upper[j] = upper;
      bound_lower[j] = lower;
    }

    result.parameter_lower = theta;
    result.parameter_upper = theta;
    for (std::size_t j = 0; j < problem.n_theta; ++j) {
      result.parameter_lower[j] = theta[j] - bound_lower[j];
      result.parameter_upper[j] = theta[j] + bound_upper[j];
    }
    result.parameter_box_certified = true;
  }
  return model::ok_result(std::move(result));
}

}  // namespace acsd::photometry
