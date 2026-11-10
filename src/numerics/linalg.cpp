#include "numerics/linalg.hpp"

#include "numerics/interval.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace acsd::numerics {
namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();

// Upper bound of the Frobenius norm of a matrix of intervals.  Every square
// and every accumulation is enclosed with the directed-rounding primitives;
// the returned value is therefore an upper bound, not the round-to-nearest
// norm that a field called "error bound" must not silently be.
double enclosed_frobenius_upper(const Matrix& matrix) {
  Interval sum(0.0, 0.0);
  for (double value : matrix.data) {
    const Interval square = interval_mul(Interval(value, value), Interval(value, value));
    sum = interval_add(sum, square);
  }
  return interval_sqrt(sum).upper;
}

// Upper bound of ||Q^T Q - I||_F for a factor whose columns should be
// orthonormal.  The bound is what the certificate uses; the naive point value
// can be smaller than the true defect because every dot product is rounded.
double orthogonality_defect_upper(const Matrix& q) {
  Interval sum(0.0, 0.0);
  for (std::size_t i = 0; i < q.cols; ++i) {
    for (std::size_t j = 0; j < q.cols; ++j) {
      Interval dot(0.0, 0.0);
      for (std::size_t k = 0; k < q.rows; ++k) {
        dot = interval_add(dot, interval_mul(Interval(q(k, i), q(k, i)),
                                             Interval(q(k, j), q(k, j))));
      }
      if (i == j) dot = interval_sub(dot, Interval(1.0, 1.0));
      sum = interval_add(sum, interval_mul(dot, dot));
    }
  }
  return interval_sqrt(sum).upper;
}

// Upper bound of ||A - U S V^T||_F.
double reconstruction_residual_upper(const Matrix& a, const SvdResult& svd) {
  Interval sum(0.0, 0.0);
  for (std::size_t i = 0; i < a.rows; ++i) {
    for (std::size_t j = 0; j < a.cols; ++j) {
      Interval entry(0.0, 0.0);
      for (std::size_t k = 0; k < svd.singular_values.size(); ++k) {
        const Interval left(svd.u(i, k), svd.u(i, k));
        const Interval sigma(svd.singular_values[k], svd.singular_values[k]);
        const Interval right(svd.v(j, k), svd.v(j, k));
        entry = interval_add(entry, interval_mul(left, interval_mul(sigma, right)));
      }
      const Interval difference = interval_sub(entry, Interval(a(i, j), a(i, j)));
      sum = interval_add(sum, interval_mul(difference, difference));
    }
  }
  return interval_sqrt(sum).upper;
}

// The standard perturbation bound for a computed SVD:
//   |sigma_i(A) - sigma_i(U S V^T)| <= e_R,
//   orthogonality defects add e_U ||S||_2 sqrt(1 + delta_V)
//   and e_V ||S||_2.
// Every operation below is outward: the inputs are already upper bounds and
// the arithmetic uses interval primitives.
double sigma_error_upper(double residual, double delta_u, double delta_v,
                         double spectral_upper) {
  const double e_u =
      delta_u >= 1.0 ? std::numeric_limits<double>::infinity()
                     : delta_u / (1.0 + std::sqrt(1.0 - delta_u));
  const double e_v =
      delta_v >= 1.0 ? std::numeric_limits<double>::infinity()
                     : delta_v / (1.0 + std::sqrt(1.0 - delta_v));
  if (!std::isfinite(e_u) || !std::isfinite(e_v) || !std::isfinite(spectral_upper)) {
    return std::numeric_limits<double>::infinity();
  }
  const Interval one_plus_delta_v =
      interval_add(Interval(1.0, 1.0), Interval(delta_v, delta_v));
  const Interval first =
      interval_mul(Interval(e_u, e_u),
                   interval_mul(Interval(spectral_upper, spectral_upper),
                                interval_sqrt(one_plus_delta_v)));
  const Interval second =
      interval_mul(Interval(e_v, e_v), Interval(spectral_upper, spectral_upper));
  const Interval total =
      interval_add(interval_add(Interval(residual, residual), first), second);
  return total.upper;
}

}  // namespace

std::vector<double> Matrix::column(std::size_t j) const {
  std::vector<double> out(rows);
  for (std::size_t i = 0; i < rows; ++i) out[i] = data[i * cols + j];
  return out;
}

void Matrix::set_column(std::size_t j, const std::vector<double>& values) {
  for (std::size_t i = 0; i < rows && i < values.size(); ++i) data[i * cols + j] = values[i];
}

Matrix identity_matrix(std::size_t n) {
  Matrix m(n, n);
  for (std::size_t i = 0; i < n; ++i) m(i, i) = 1.0;
  return m;
}

Matrix transpose(const Matrix& a) {
  Matrix t(a.cols, a.rows);
  for (std::size_t i = 0; i < a.rows; ++i)
    for (std::size_t j = 0; j < a.cols; ++j) t(j, i) = a(i, j);
  return t;
}

Matrix matmul(const Matrix& a, const Matrix& b) {
  Matrix out(a.rows, b.cols);
  for (std::size_t i = 0; i < a.rows; ++i) {
    for (std::size_t k = 0; k < a.cols; ++k) {
      const double aik = a(i, k);
      if (aik == 0.0) continue;
      for (std::size_t j = 0; j < b.cols; ++j) out(i, j) += aik * b(k, j);
    }
  }
  return out;
}

std::vector<double> matvec(const Matrix& a, const std::vector<double>& x) {
  std::vector<double> out(a.rows, 0.0);
  for (std::size_t i = 0; i < a.rows; ++i) {
    double sum = 0.0;
    for (std::size_t j = 0; j < a.cols; ++j) sum += a(i, j) * x[j];
    out[i] = sum;
  }
  return out;
}

double frobenius_norm(const Matrix& a) {
  double sum = 0.0;
  for (double v : a.data) sum += v * v;
  return std::sqrt(sum);
}

double frobenius_norm_diff(const Matrix& a, const Matrix& b) {
  if (a.rows != b.rows || a.cols != b.cols) return std::numeric_limits<double>::infinity();
  double sum = 0.0;
  for (std::size_t i = 0; i < a.data.size(); ++i) {
    const double d = a.data[i] - b.data[i];
    sum += d * d;
  }
  return std::sqrt(sum);
}

double max_abs(const Matrix& a) {
  double m = 0.0;
  for (double v : a.data) m = std::max(m, std::fabs(v));
  return m;
}

double column_two_norm(const Matrix& a, std::size_t j) {
  double sum = 0.0;
  for (std::size_t i = 0; i < a.rows; ++i) sum += a(i, j) * a(i, j);
  return std::sqrt(sum);
}

SvdResult jacobi_svd(const Matrix& a, double rank_relative) {
  SvdResult out;
  if (a.rows == 0 || a.cols == 0) return out;
  const bool transposed = a.rows < a.cols;
  const Matrix work = transposed ? transpose(a) : a;
  const std::size_t m = work.rows;
  const std::size_t n = work.cols;

  Matrix u = work;
  Matrix v = identity_matrix(n);

  const int max_sweeps = 60;
  bool converged = false;
  for (int sweep = 0; sweep < max_sweeps && !converged; ++sweep) {
    double off = 0.0;
    for (std::size_t p = 0; p + 1 < n; ++p) {
      for (std::size_t q = p + 1; q < n; ++q) {
        double alpha = 0.0, beta = 0.0, gamma = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
          alpha += u(i, p) * u(i, p);
          beta += u(i, q) * u(i, q);
          gamma += u(i, p) * u(i, q);
        }
        if (alpha <= 0.0 || beta <= 0.0) continue;
        const double denom = std::sqrt(alpha * beta);
        if (std::fabs(gamma) <= kEps * denom) continue;
        off += gamma * gamma;
        const double zeta = (beta - alpha) / (2.0 * gamma);
        const double t = (zeta >= 0.0 ? 1.0 : -1.0) /
                         (std::fabs(zeta) + std::sqrt(1.0 + zeta * zeta));
        const double c = 1.0 / std::sqrt(1.0 + t * t);
        const double s = c * t;
        for (std::size_t i = 0; i < m; ++i) {
          const double up = u(i, p), uq = u(i, q);
          u(i, p) = c * up - s * uq;
          u(i, q) = s * up + c * uq;
        }
        for (std::size_t i = 0; i < n; ++i) {
          const double vp = v(i, p), vq = v(i, q);
          v(i, p) = c * vp - s * vq;
          v(i, q) = s * vp + c * vq;
        }
      }
    }
    if (off <= kEps * kEps) converged = true;
  }

  std::vector<double> sigma(n, 0.0);
  for (std::size_t j = 0; j < n; ++j) {
    sigma[j] = column_two_norm(u, j);
    if (sigma[j] > 0.0) {
      for (std::size_t i = 0; i < m; ++i) u(i, j) /= sigma[j];
    }
  }
  // A zero singular value leaves the corresponding column of U undefined.
  // Completing it with an orthonormal direction keeps U a basis, so the
  // orthogonality defect and the spectral error bound stay finite for a
  // rank-deficient input.  Without this completion ||U^T U - I|| = 1 and no
  // rank could ever be certified.
  for (std::size_t j = 0; j < n; ++j) {
    if (sigma[j] > 0.0) continue;
    double best_norm = -1.0;
    std::vector<double> best(m, 0.0);
    for (std::size_t basis = 0; basis < m; ++basis) {
      std::vector<double> candidate(m, 0.0);
      candidate[basis] = 1.0;
      for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t k = 0; k < n; ++k) {
          if (k == j) continue;
          if (sigma[k] <= 0.0 && k > j) continue;  // not completed yet
          double dot = 0.0;
          for (std::size_t i = 0; i < m; ++i) dot += candidate[i] * u(i, k);
          for (std::size_t i = 0; i < m; ++i) candidate[i] -= dot * u(i, k);
        }
      }
      double norm = 0.0;
      for (double value : candidate) norm += value * value;
      norm = std::sqrt(norm);
      if (norm > best_norm) {
        best_norm = norm;
        best = candidate;
      }
    }
    if (best_norm > 1.0e-12) {
      for (std::size_t i = 0; i < m; ++i) u(i, j) = best[i] / best_norm;
    }
  }
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(),
            [&](std::size_t x, std::size_t y) { return sigma[x] > sigma[y]; });

  Matrix u_sorted(m, n), v_sorted(n, n);
  out.singular_values.resize(n);
  for (std::size_t k = 0; k < n; ++k) {
    const std::size_t src = order[k];
    out.singular_values[k] = sigma[src];
    for (std::size_t i = 0; i < m; ++i) u_sorted(i, k) = u(i, src);
    for (std::size_t i = 0; i < n; ++i) v_sorted(i, k) = v(i, src);
  }
  // The decomposition was computed on the transposed matrix when the input is
  // wider than it is tall.  The singular vectors of the original matrix are
  // recovered directly: for a = U S V^T and a^T = V S U^T the left factors
  // swap roles, so the left factor of a is the right factor of a^T and the
  // right factor of a is the left factor of a^T.
  if (transposed) {
    out.u = v_sorted;   // m x m
    out.v = u_sorted;   // n x m
  } else {
    out.u = u_sorted;   // m x k
    out.v = v_sorted;   // k x n
  }

  // Orthogonality defects and reconstruction residual are evaluated with the
  // directed-rounding primitives, so the certificate does not rest on a
  // round-to-nearest residual that may be smaller than the true defect.
  out.delta_u = orthogonality_defect_upper(out.u);
  out.delta_v = orthogonality_defect_upper(out.v);
  out.residual = reconstruction_residual_upper(a, out);

  // The spectral norm is bounded above by the Frobenius norm, which is
  // available outward.  The tighter upper bound |sigma_max| + residual would
  // need the true sigma_max, so it is not used here: a loose but valid bound
  // keeps the certificate honest.
  out.spectral_norm = out.singular_values.empty() ? 0.0 : out.singular_values[0];
  const double spectral_upper = enclosed_frobenius_upper(a);
  out.sigma_error_bound =
      sigma_error_upper(out.residual, out.delta_u, out.delta_v, spectral_upper);

  const double threshold_upper = rank_relative * (out.spectral_norm + out.sigma_error_bound);
  out.rank = 0;
  for (double s : out.singular_values) {
    if (s - out.sigma_error_bound > threshold_upper) ++out.rank;
  }
  out.converged = converged && std::isfinite(out.sigma_error_bound);
  return out;
}

RankReport classify_rank(const SvdResult& svd, std::size_t columns, double rank_relative) {
  RankReport report;
  report.columns = columns;
  report.singular_values = svd.singular_values;
  report.singular_error_bound = svd.sigma_error_bound;
  // The threshold itself depends on the (uncertain) spectral norm.  A value is
  // classified only when the decision holds for every threshold in the
  // interval implied by the singular-value bound; otherwise the rank is
  // reported unresolved instead of being silently decided by a point value.
  const double spectral_lower =
      std::max(0.0, svd.spectral_norm - svd.sigma_error_bound);
  const double spectral_upper = svd.spectral_norm + svd.sigma_error_bound;
  const double threshold_lower = rank_relative * spectral_lower;
  const double threshold_upper = rank_relative * spectral_upper;
  std::size_t rank = 0;
  bool unresolved = false;
  for (double s : svd.singular_values) {
    const double lo = s - svd.sigma_error_bound;
    const double hi = s + svd.sigma_error_bound;
    if (lo > threshold_upper) {
      ++rank;
    } else if (hi < threshold_lower) {
      // provably below every admissible threshold
    } else {
      unresolved = true;
    }
  }
  report.rank = rank;
  report.certified = svd.converged && !unresolved && std::isfinite(svd.sigma_error_bound) &&
                     svd.sigma_error_bound < threshold_lower;
  report.note = unresolved ? "rank_unresolved: singular value interval crosses the threshold"
                           : (report.certified ? "rank_certified" : "numerical_rank");
  // Null space basis: the right singular vectors whose singular value is at or
  // below the classification threshold.
  const std::size_t k = svd.v.cols;
  for (std::size_t j = rank; j < k; ++j) {
    std::vector<double> vector(svd.v.rows, 0.0);
    for (std::size_t i = 0; i < svd.v.rows; ++i) vector[i] = svd.v(i, j);
    report.nullspace.push_back(std::move(vector));
  }
  return report;
}

namespace {

// Householder QR with column pivoting.
//
// The reflectors are kept separately from R: the classical implementation
// stores the reflector vector in the lower part of the working column, which
// destroys the diagonal of R, so the back substitution would read the
// reflector instead of the triangular factor.
struct QrFactor {
  Matrix r;                            // upper triangular factor
  std::vector<std::size_t> permutation;
  std::vector<double> column_scales;
  std::vector<std::vector<double>> reflectors;  // reflectors[k][i] is the i-th entry
  std::vector<double> taus;
  std::size_t rank = 0;
  std::vector<double> singular_values;
};

QrFactor householder_qr(const Matrix& a) {
  QrFactor factor;
  const std::size_t m = a.rows, n = a.cols;
  Matrix work = a;
  factor.permutation.resize(n);
  std::iota(factor.permutation.begin(), factor.permutation.end(), 0);
  factor.column_scales.resize(n, 1.0);
  for (std::size_t j = 0; j < n; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < m; ++i) norm = std::max(norm, std::fabs(work(i, j)));
    factor.column_scales[j] = norm > 0.0 ? norm : 1.0;
    for (std::size_t i = 0; i < m; ++i) work(i, j) /= factor.column_scales[j];
  }
  std::vector<double> column_norms(n, 0.0);
  for (std::size_t j = 0; j < n; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < m; ++i) norm += work(i, j) * work(i, j);
    column_norms[j] = std::sqrt(norm);
  }

  factor.reflectors.assign(n, {});
  factor.taus.assign(n, 0.0);
  const std::size_t steps = std::min(m, n);
  for (std::size_t k = 0; k < steps; ++k) {
    // Column pivoting.
    std::size_t pivot = k;
    for (std::size_t j = k + 1; j < n; ++j)
      if (column_norms[j] > column_norms[pivot]) pivot = j;
    if (pivot != k) {
      for (std::size_t i = 0; i < m; ++i) std::swap(work(i, k), work(i, pivot));
      std::swap(column_norms[k], column_norms[pivot]);
      std::swap(factor.column_scales[k], factor.column_scales[pivot]);
      std::swap(factor.permutation[k], factor.permutation[pivot]);
    }

    // Householder reflector for the current column (LAPACK dlarfg form).
    const double alpha = work(k, k);
    double tail = 0.0;
    for (std::size_t i = k + 1; i < m; ++i) tail += work(i, k) * work(i, k);
    tail = std::sqrt(tail);
    std::vector<double> reflector(m - k, 0.0);
    double tau = 0.0;
    if (tail > 0.0) {
      const double beta = (alpha >= 0.0 ? -1.0 : 1.0) * std::sqrt(alpha * alpha + tail * tail);
      tau = (beta - alpha) / beta;
      reflector[0] = 1.0;
      for (std::size_t i = k + 1; i < m; ++i) reflector[i - k] = work(i, k) / (alpha - beta);
      work(k, k) = beta;
      for (std::size_t i = k + 1; i < m; ++i) work(i, k) = 0.0;
      // Apply the reflector to the remaining columns.
      for (std::size_t j = k + 1; j < n; ++j) {
        double dot = work(k, j);
        for (std::size_t i = k + 1; i < m; ++i) dot += reflector[i - k] * work(i, j);
        work(k, j) -= tau * dot;
        for (std::size_t i = k + 1; i < m; ++i) work(i, j) -= tau * reflector[i - k] * dot;
      }
    }
    factor.reflectors[k] = std::move(reflector);
    factor.taus[k] = tau;
  }

  // Extract the triangular factor.
  factor.r = Matrix(m, n);
  for (std::size_t i = 0; i < m; ++i)
    for (std::size_t j = 0; j < n; ++j)
      factor.r(i, j) = (i <= j) ? work(i, j) : 0.0;

  double rnorm = 0.0;
  for (std::size_t j = 0; j < std::min(m, n); ++j)
    rnorm = std::max(rnorm, std::fabs(factor.r(j, j)));
  const double tol = (m > 0 && n > 0) ? static_cast<double>(std::max(m, n)) * kEps * rnorm : 0.0;
  factor.rank = 0;
  for (std::size_t j = 0; j < std::min(m, n); ++j)
    if (std::fabs(factor.r(j, j)) > tol) ++factor.rank;
  factor.singular_values.resize(n);
  for (std::size_t j = 0; j < n; ++j) factor.singular_values[j] = std::fabs(factor.r(j, j));
  return factor;
}

}  // namespace

LeastSquaresResult least_squares(const Matrix& a, const std::vector<double>& rhs,
                                  double rank_relative) {
  LeastSquaresResult out;
  const std::size_t m = a.rows, n = a.cols;
  if (m == 0 || n == 0) return out;
  if (m < n) {
    // Underdetermined: no unique solution.  Report the identifiable rank and
    // converged=false instead of indexing the triangular factor out of range
    // (or silently returning an arbitrary minimum-norm answer).
    out.solution.assign(n, 0.0);
    out.solutions.assign(1, out.solution);
    out.rank = m;
    out.converged = false;
    out.rank_certified = false;
    return out;
  }
  const QrFactor factor = householder_qr(a);

  // Apply the reflectors to the right hand side: Q^T b.
  std::vector<double> transformed(rhs.size(), 0.0);
  for (std::size_t i = 0; i < rhs.size() && i < m; ++i) transformed[i] = rhs[i];
  for (std::size_t k = 0; k < n; ++k) {
    const double tau = factor.taus[k];
    if (tau == 0.0) continue;
    const std::vector<double>& reflector = factor.reflectors[k];
    double dot = transformed[k];
    for (std::size_t i = k + 1; i < m; ++i) dot += reflector[i - k] * transformed[i];
    transformed[k] -= tau * dot;
    for (std::size_t i = k + 1; i < m; ++i) transformed[i] -= tau * reflector[i - k] * dot;
  }

  // Back substitution on the triangular factor.
  std::vector<double> y(n, 0.0);
  for (std::size_t k = n; k-- > 0;) {
    if (k >= factor.rank || factor.r(k, k) == 0.0) { y[k] = 0.0; continue; }
    double sum = transformed[k];
    for (std::size_t j = k + 1; j < n; ++j) sum -= factor.r(k, j) * y[j];
    y[k] = sum / factor.r(k, k);
  }

  // Undo the column scaling and the permutation.  The working matrix is
  // A' = A P S^{-1} (the columns are divided by their pivot scale and then
  // permuted), so A' y = b gives A (P S^{-1} y) = b and the solution of the
  // original system is x = P S^{-1} y.
  out.solution.assign(n, 0.0);
  for (std::size_t j = 0; j < n; ++j) {
    out.solution[factor.permutation[j]] = y[j] / factor.column_scales[j];
  }

  // Residuals are evaluated as enclosures.  The reported value is the upper
  // endpoint: a point residual computed in binary64 can be smaller than the
  // true residual and must not be called a bound.
  Interval residual_square(0.0, 0.0);
  double inf_bound = 0.0;
  for (std::size_t i = 0; i < rhs.size() && i < a.rows; ++i) {
    Interval fitted(0.0, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      fitted = interval_add(fitted, interval_mul(Interval(a(i, j), a(i, j)),
                                                 Interval(out.solution[j], out.solution[j])));
    }
    const Interval difference = interval_sub(Interval(rhs[i], rhs[i]), fitted);
    residual_square = interval_add(residual_square, interval_mul(difference, difference));
    const double magnitude =
        std::max(std::fabs(difference.lower), std::fabs(difference.upper));
    inf_bound = std::max(inf_bound, std::nextafter(magnitude, std::numeric_limits<double>::infinity()));
  }
  out.residuals.push_back(interval_sqrt(residual_square).upper);
  out.scaled_residual = inf_bound;

  const SvdResult svd = jacobi_svd(a, rank_relative);
  const RankReport rank_report = classify_rank(svd, n, rank_relative);
  out.rank = rank_report.rank;
  out.rank_certified = rank_report.certified;
  out.rank_error = svd.sigma_error_bound;
  out.singular_values = svd.singular_values;
  out.converged = std::isfinite(out.residuals.back()) && std::isfinite(out.rank_error);
  return out;
}

LeastSquaresResult least_squares(const Matrix& a, const std::vector<std::vector<double>>& rhs,
                                  double rank_relative) {
  LeastSquaresResult combined;
  combined.converged = true;
  combined.rank_certified = true;
  combined.rank_error = 0.0;
  bool first = true;
  for (const std::vector<double>& column : rhs) {
    LeastSquaresResult single = least_squares(a, column, rank_relative);
    combined.solutions.push_back(single.solution);
    combined.residuals.push_back(single.residuals.empty() ? 0.0 : single.residuals[0]);
    combined.scaled_residual = std::max(combined.scaled_residual, single.scaled_residual);
    if (first) {
      combined.rank = single.rank;
      combined.singular_values = single.singular_values;
      first = false;
    } else if (combined.rank != single.rank) {
      // The rank of the design is a property of the matrix, so a right hand
      // side cannot change it.  Keep the larger value only as a conservative
      // diagnostic and refuse the certificate.
      combined.rank = std::max(combined.rank, single.rank);
      combined.rank_certified = false;
    }
    combined.converged = combined.converged && single.converged;
    combined.rank_certified = combined.rank_certified && single.rank_certified;
    combined.rank_error = std::max(combined.rank_error, single.rank_error);
  }
  if (!combined.solutions.empty()) combined.solution = combined.solutions.front();
  return combined;
}

bool cholesky_factor(const Matrix& a, Matrix& l, double& min_diagonal) {
  const std::size_t n = a.rows;
  if (a.cols != n) return false;
  l = Matrix(n, n);
  min_diagonal = std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      double sum = a(i, j);
      for (std::size_t k = 0; k < j; ++k) sum -= l(i, k) * l(j, k);
      if (i == j) {
        if (!(sum > 0.0)) return false;
        l(i, j) = std::sqrt(sum);
        min_diagonal = std::min(min_diagonal, l(i, j));
      } else {
        l(i, j) = sum / l(j, j);
      }
    }
  }
  return true;
}

std::optional<std::vector<double>> cholesky_solve(const Matrix& a, const std::vector<double>& b) {
  Matrix l;
  double min_diag = 0.0;
  if (!cholesky_factor(a, l, min_diag)) return std::nullopt;
  const std::size_t n = a.rows;
  std::vector<double> y(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    double sum = b[i];
    for (std::size_t k = 0; k < i; ++k) sum -= l(i, k) * y[k];
    y[i] = sum / l(i, i);
  }
  std::vector<double> x(n, 0.0);
  for (std::size_t i = n; i-- > 0;) {
    double sum = y[i];
    for (std::size_t k = i + 1; k < n; ++k) sum -= l(k, i) * x[k];
    x[i] = sum / l(i, i);
  }
  return x;
}

EigenResult symmetric_eigen(const Matrix& a) {
  EigenResult out;
  const std::size_t n = a.rows;
  if (n == 0 || a.cols != n) return out;
  Matrix m = a;
  Matrix v = identity_matrix(n);
  for (int sweep = 0; sweep < 100; ++sweep) {
    double off = 0.0;
    for (std::size_t p = 0; p < n; ++p)
      for (std::size_t q = p + 1; q < n; ++q) off += m(p, q) * m(p, q);
    if (off <= 1e-30) break;
    for (std::size_t p = 0; p < n; ++p) {
      for (std::size_t q = p + 1; q < n; ++q) {
        if (std::fabs(m(p, q)) < 1e-300) continue;
        const double theta = (m(q, q) - m(p, p)) / (2.0 * m(p, q));
        const double t = (theta >= 0.0 ? 1.0 : -1.0) /
                         (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
        const double c = 1.0 / std::sqrt(t * t + 1.0);
        const double s = t * c;
        for (std::size_t k = 0; k < n; ++k) {
          const double mkp = m(k, p), mkq = m(k, q);
          m(k, p) = c * mkp - s * mkq;
          m(k, q) = s * mkp + c * mkq;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double mpk = m(p, k), mqk = m(q, k);
          m(p, k) = c * mpk - s * mqk;
          m(q, k) = s * mpk + c * mqk;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double vkp = v(k, p), vkq = v(k, q);
          v(k, p) = c * vkp - s * vkq;
          v(k, q) = s * vkp + c * vkq;
        }
      }
    }
  }
  out.eigenvalues.resize(n);
  for (std::size_t i = 0; i < n; ++i) out.eigenvalues[i] = m(i, i);
  out.eigenvectors = v;
  // Orthogonality defect and residual ||A V - V diag(lambda)||_F are bounded
  // outward.  A point residual can be smaller than the true one and cannot be
  // used to certify the spectral decomposition.
  out.delta_u = orthogonality_defect_upper(v);
  Interval sum(0.0, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) {
      Interval left(0.0, 0.0);
      for (std::size_t k = 0; k < n; ++k) {
        left = interval_add(left, interval_mul(Interval(a(i, k), a(i, k)),
                                               Interval(v(k, j), v(k, j))));
      }
      const Interval right(out.eigenvalues[j] * v(i, j), out.eigenvalues[j] * v(i, j));
      const Interval difference = interval_sub(left, right);
      sum = interval_add(sum, interval_mul(difference, difference));
    }
  }
  out.residual = interval_sqrt(sum).upper;
  out.converged = out.delta_u < 1.0 && std::isfinite(out.residual);
  return out;
}



Matrix pseudo_inverse(const SvdResult& svd, std::size_t columns, double rank_relative) {
  const std::size_t k = svd.singular_values.size();
  Matrix out(columns, svd.u.rows);
  if (k == 0 || svd.u.rows == 0) return out;
  const double largest = svd.singular_values.front();
  const double threshold = largest > 0.0 ? rank_relative * largest : 0.0;
  for (std::size_t i = 0; i < k; ++i) {
    const double sigma = svd.singular_values[i];
    if (!(sigma > threshold) || sigma <= 0.0) continue;
    const double reciprocal = 1.0 / sigma;
    for (std::size_t r = 0; r < svd.v.rows; ++r) {
      const double vr = svd.v(r, i);
      if (vr == 0.0) continue;
      for (std::size_t c = 0; c < svd.u.rows; ++c) {
        out(r, c) += vr * svd.u(c, i) * reciprocal;
      }
    }
  }
  return out;
}

}  // namespace acsd::numerics
