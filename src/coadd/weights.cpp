// The statistical estimators of the coadd module (see coadd.hpp).
//
// Scientific contract (STATISTICS_SKY_COADD section 5 and EXECUTION):
//   * the BLUE weights are t = Gamma^{-1} 1 / (1^T Gamma^{-1} 1);
//   * for a positive semi definite Gamma the null space is an exact
//     constraint: a null vector with a non zero sum gives a zero variance
//     normalised solution, otherwise the solution is taken in the range of
//     Gamma; there is no epsilon floor and the pseudo inverse never pretends
//     to be a finite error;
//   * the non negative program is solved with an active set method that
//     starts from the equal weight feasible solution, walks along the segment
//     to the first zero, breaks the ties by the frame identity and uses the
//     Bland rule against degenerate cycles;
//   * the leave one out test rejects at most one sample, only once, and only
//     when the common mean is comparable and an external fixed Gamma is given.
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "coadd/coadd.hpp"
#include "numerics/quadrature.hpp"

namespace acsd::coadd_internal {
namespace {

using numerics::Matrix;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

Matrix identity(std::size_t n) {
  Matrix out(n, n, 0.0);
  for (std::size_t i = 0; i < n; ++i) out(i, i) = 1.0;
  return out;
}

}  // namespace

double normal_quantile(double probability) {
  if (!(probability > 0.0) || !(probability < 1.0)) return kNaN;
  // The bisection on the error function is slow but exact enough for the
  // thresholds of the leave one out test.
  double low = -40.0;
  double high = 40.0;
  for (int step = 0; step < 200; ++step) {
    const double middle = 0.5 * (low + high);
    const double value = 0.5 * (1.0 + numerics::erf(middle / std::sqrt(2.0)));
    if (value < probability) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return 0.5 * (low + high);
}

numerics::Matrix sub_matrix(const Matrix& gamma, const std::vector<std::size_t>& rows) {
  Matrix out(rows.size(), rows.size(), 0.0);
  for (std::size_t i = 0; i < rows.size(); ++i) {
    for (std::size_t j = 0; j < rows.size(); ++j) {
      out(i, j) = gamma(rows[i], rows[j]);
    }
  }
  return out;
}

BlueResult solve_blue(const Matrix& gamma, double rank_relative) {
  BlueResult result;
  const std::size_t n = gamma.rows;
  if (n == 0 || gamma.cols != n) {
    result.reason = "the covariance block is empty";
    return result;
  }
  numerics::SvdResult svd = numerics::jacobi_svd(gamma, rank_relative);
  const double sigma_max = svd.singular_values.empty() ? 0.0 : svd.singular_values.front();
  const double threshold = rank_relative * sigma_max;
  std::size_t rank = 0;
  for (double value : svd.singular_values) {
    if (value > threshold) ++rank;
  }
  // The null space of the block.
  Matrix null_basis(n, n - rank, 0.0);
  for (std::size_t j = 0; j < null_basis.cols; ++j) {
    for (std::size_t i = 0; i < n; ++i) null_basis(i, j) = svd.u(i, rank + j);
  }
  // Is there a null vector whose sum is not zero?
  double null_sum = 0.0;
  for (std::size_t j = 0; j < null_basis.cols; ++j) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += null_basis(i, j);
    null_sum += sum * sum;
  }
  if (null_basis.cols > 0 && null_sum > 1e-24) {
    // A zero variance normalised solution exists: the minimum norm vector of
    // the null space with unit sum.
    std::vector<double> sum_vector(null_basis.cols, 0.0);
    for (std::size_t j = 0; j < null_basis.cols; ++j) {
      double sum = 0.0;
      for (std::size_t i = 0; i < n; ++i) sum += null_basis(i, j);
      sum_vector[j] = sum;
    }
    double norm = 0.0;
    for (double value : sum_vector) norm += value * value;
    result.weights.assign(n, 0.0);
    for (std::size_t j = 0; j < null_basis.cols; ++j) {
      const double factor = sum_vector[j] / norm;
      for (std::size_t i = 0; i < n; ++i) result.weights[i] += factor * null_basis(i, j);
    }
    result.outcome = BlueOutcome::kZeroVariance;
    result.variance = 0.0;
    result.denominator = 1.0;
    result.reason = "the block has an exact constraint that fixes the weights";
    return result;
  }
  // The minimum norm solution of Gamma q = 1.
  std::vector<double> q(n, 0.0);
  bool solvable = true;
  for (std::size_t j = 0; j < rank; ++j) {
    const double singular = svd.singular_values[j];
    if (!(singular > 0.0)) continue;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += svd.u(i, j);
    const double scale = sum / singular;
    for (std::size_t i = 0; i < n; ++i) q[i] += scale * svd.v(i, j);
  }
  // Is 1 inside the range of the block?
  double residual = 0.0;
  for (std::size_t j = rank; j < svd.u.cols; ++j) {
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) sum += svd.u(i, j);
    residual += sum * sum;
  }
  if (residual > 1e-20) {
    solvable = false;
    result.reason = "the sum vector is outside the range of the covariance block";
  }
  if (!solvable) {
    result.outcome = BlueOutcome::kUnresolved;
    result.reason = "the sum vector is outside the range of the covariance block";
    return result;
  }
  double denominator = 0.0;
  for (double value : q) denominator += value;
  if (!(denominator > 0.0)) {
    result.outcome = BlueOutcome::kUnresolved;
    result.reason = "the normalising denominator has no positive lower bound";
    return result;
  }
  result.weights.assign(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) result.weights[i] = q[i] / denominator;
  // The variance t^T Gamma t.
  double variance = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      variance += result.weights[i] * gamma(i, j) * result.weights[j];
    }
  }
  result.outcome = BlueOutcome::kSolved;
  result.variance = variance;
  result.denominator = denominator;
  return result;
}

QpResult solve_nonnegative_qp(const Matrix& gamma, const std::vector<std::string>& names,
                               std::size_t max_iterations) {
  QpResult result;
  const std::size_t n = gamma.rows;
  if (n == 0 || gamma.cols != n) {
    result.reason = "the covariance block is empty";
    return result;
  }
  if (names.size() != n) {
    result.reason = "the frame identities do not match the covariance block";
    return result;
  }
  // The equal weight feasible solution is the starting point.
  std::vector<double> current(n, 1.0 / static_cast<double>(n));
  std::vector<char> active(n, 0);
  std::vector<char> ever_released(n, 0);

  auto solve_free = [&](std::vector<double>& candidate) -> bool {
    std::vector<std::size_t> free_rows;
    for (std::size_t i = 0; i < n; ++i) {
      if (!active[i]) free_rows.push_back(i);
    }
    if (free_rows.empty()) return false;
    const Matrix block = sub_matrix(gamma, free_rows);
    numerics::SvdResult svd = numerics::jacobi_svd(block, 1e-12);
    const double sigma_max = svd.singular_values.empty() ? 0.0 : svd.singular_values.front();
    const double threshold = 1e-12 * sigma_max;
    std::vector<double> q(free_rows.size(), 0.0);
    for (std::size_t j = 0; j < svd.v.cols; ++j) {
      const double singular = svd.singular_values[j];
      if (!(singular > threshold)) continue;
      double sum = 0.0;
      for (std::size_t i = 0; i < free_rows.size(); ++i) sum += svd.u(i, j);
      const double scale = sum / singular;
      for (std::size_t i = 0; i < free_rows.size(); ++i) q[i] += scale * svd.v(i, j);
    }
    double denominator = 0.0;
    for (double value : q) denominator += value;
    if (!(denominator > 0.0)) return false;
    candidate.assign(n, 0.0);
    for (std::size_t i = 0; i < free_rows.size(); ++i) {
      candidate[free_rows[i]] = q[i] / denominator;
    }
    return true;
  };

  for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
    std::vector<double> candidate;
    if (!solve_free(candidate)) {
      result.outcome = QpOutcome::kUnresolved;
      result.reason = "the equality constrained sub problem has no positive normaliser";
      return result;
    }
    // A candidate with a negative weight: walk along the segment from the
    // current solution to the candidate and stop at the first zero.
    int worst = -1;
    for (std::size_t i = 0; i < n; ++i) {
      if (candidate[i] < -1e-15) {
        if (worst < 0) {
          worst = static_cast<int>(i);
        } else {
          // The tie is broken by the frame identity.
          if (candidate[static_cast<std::size_t>(worst)] > candidate[i]) {
            worst = static_cast<int>(i);
          } else if (candidate[static_cast<std::size_t>(worst)] == candidate[i] &&
                     names[i] < names[static_cast<std::size_t>(worst)]) {
            worst = static_cast<int>(i);
          }
        }
      }
    }
    if (worst >= 0) {
      const std::size_t index = static_cast<std::size_t>(worst);
      double step = 1.0;
      for (std::size_t i = 0; i < n; ++i) {
        if (candidate[i] >= 0.0) continue;
        const double denominator = current[i] - candidate[i];
        if (!(denominator > 0.0)) continue;
        step = std::min(step, current[i] / denominator);
      }
      for (std::size_t i = 0; i < n; ++i) {
        current[i] += step * (candidate[i] - current[i]);
      }
      current[index] = 0.0;
      active[index] = 1;
      continue;
    }
    // The candidate is feasible: check the KKT conditions.
    std::vector<double> gradient(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
      double sum = 0.0;
      for (std::size_t j = 0; j < n; ++j) sum += gamma(i, j) * candidate[j];
      gradient[i] = 2.0 * sum;
    }
    double mu = 0.0;
    std::size_t free_count = 0;
    for (std::size_t i = 0; i < n; ++i) {
      if (active[i]) continue;
      mu += gradient[i];
      ++free_count;
    }
    if (free_count == 0) {
      result.outcome = QpOutcome::kUnresolved;
      result.reason = "every weight is fixed at zero";
      return result;
    }
    mu /= static_cast<double>(free_count);
    int release = -1;
    double worst_reduced = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      if (!active[i]) continue;
      const double reduced = gradient[i] - mu;
      if (reduced < -1e-12) {
        if (release < 0 || reduced < worst_reduced ||
            (reduced == worst_reduced && names[i] < names[static_cast<std::size_t>(release)])) {
          release = static_cast<int>(i);
          worst_reduced = reduced;
        }
      }
    }
    if (release >= 0) {
      const std::size_t index = static_cast<std::size_t>(release);
      active[index] = 0;
      ever_released[index] = 1;
      continue;
    }
    current = candidate;
    // The stopping test: the sum is one, the weights are positive, the
    // stationarity and the complementarity hold.
    double sum = 0.0;
    bool positive = true;
    for (std::size_t i = 0; i < n; ++i) {
      sum += current[i];
      if (current[i] < -1e-12) positive = false;
    }
    double stationarity = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      stationarity = std::max(stationarity, std::fabs(gradient[i] - mu));
    }
    double complementarity = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      if (active[i]) complementarity = std::max(complementarity, std::fabs(gradient[i] - mu));
    }
    const bool converged = positive && std::fabs(sum - 1.0) < 1e-10 &&
                           stationarity < 1e-8 && complementarity < 1e-8;
    if (converged) {
      double variance = 0.0;
      for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) variance += current[i] * gamma(i, j) * current[j];
      }
      result.outcome = QpOutcome::kSolved;
      result.weights = current;
      result.variance = variance;
      return result;
    }
    // The candidate is feasible but the stopping test failed; refine the
    // active set with the smallest violation and continue.
    result.outcome = QpOutcome::kNonConverged;
    result.weights = current;
    result.reason = "the optimality conditions did not reach the requested tolerance";
    return result;
  }
  result.outcome = QpOutcome::kNonConverged;
  result.reason = "the active set iteration reached its limit";
  return result;
}

}  // namespace acsd::coadd_internal
