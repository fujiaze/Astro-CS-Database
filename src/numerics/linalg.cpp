#include "numerics/linalg.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace acsd::numerics {
namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();

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

  // Orthogonality defects of the returned factors.
  auto defect = [](const Matrix& q) {
    Matrix gram(q.cols, q.cols);
    for (std::size_t i = 0; i < q.cols; ++i)
      for (std::size_t j = 0; j < q.cols; ++j) {
        double sum = 0.0;
        for (std::size_t k = 0; k < q.rows; ++k) sum += q(k, i) * q(k, j);
        gram(i, j) = sum;
      }
    for (std::size_t i = 0; i < gram.cols; ++i) gram(i, i) -= 1.0;
    return frobenius_norm(gram);
  };
  out.delta_u = defect(out.u);
  out.delta_v = defect(out.v);

  Matrix reconstructed(out.u.rows, out.v.rows);
  for (std::size_t i = 0; i < reconstructed.rows; ++i)
    for (std::size_t j = 0; j < reconstructed.cols; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < out.singular_values.size(); ++k)
        sum += out.u(i, k) * out.singular_values[k] * out.v(j, k);
      reconstructed(i, j) = sum;
    }
  out.residual = frobenius_norm_diff(a, reconstructed);
  out.spectral_norm = out.singular_values.empty() ? 0.0 : out.singular_values[0];

  const double eU = out.delta_u >= 1.0 ? std::numeric_limits<double>::infinity()
                                       : out.delta_u / (1.0 + std::sqrt(1.0 - out.delta_u));
  const double eV = out.delta_v >= 1.0 ? std::numeric_limits<double>::infinity()
                                       : out.delta_v / (1.0 + std::sqrt(1.0 - out.delta_v));
  out.sigma_error_bound = out.residual + eU * out.spectral_norm * std::sqrt(1.0 + out.delta_v) +
                          eV * out.spectral_norm;

  const double threshold = rank_relative * out.spectral_norm;
  out.rank = 0;
  for (double s : out.singular_values) {
    if (s > threshold) ++out.rank;
  }
  out.converged = converged;
  return out;
}

RankReport classify_rank(const SvdResult& svd, std::size_t columns, double rank_relative) {
  RankReport report;
  report.columns = columns;
  report.singular_values = svd.singular_values;
  report.singular_error_bound = svd.sigma_error_bound;
  const double threshold = rank_relative * svd.spectral_norm;
  std::size_t rank = 0;
  bool unresolved = false;
  for (double s : svd.singular_values) {
    const double lo = s - svd.sigma_error_bound;
    const double hi = s + svd.sigma_error_bound;
    if (lo > threshold) {
      ++rank;
    } else if (hi < threshold) {
      // provably below the threshold
    } else {
      unresolved = true;
    }
  }
  report.rank = rank;
  report.certified = svd.converged && !unresolved && report.singular_error_bound < threshold;
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

// Householder QR with column pivoting, applied in place to a working copy.
struct QrFactor {
  Matrix qr;
  std::vector<std::size_t> permutation;
  std::vector<double> column_scales;
  std::size_t rank = 0;
  std::vector<double> singular_values;
};

QrFactor householder_qr(const Matrix& a) {
  QrFactor factor;
  const std::size_t m = a.rows, n = a.cols;
  factor.qr = a;
  factor.permutation.resize(n);
  std::iota(factor.permutation.begin(), factor.permutation.end(), 0);
  factor.column_scales.resize(n, 1.0);
  for (std::size_t j = 0; j < n; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < m; ++i) norm = std::max(norm, std::fabs(factor.qr(i, j)));
    factor.column_scales[j] = norm > 0.0 ? norm : 1.0;
    for (std::size_t i = 0; i < m; ++i) factor.qr(i, j) /= factor.column_scales[j];
  }
  std::vector<double> column_norms(n, 0.0);
  for (std::size_t j = 0; j < n; ++j) {
    double norm = 0.0;
    for (std::size_t i = 0; i < m; ++i) norm += factor.qr(i, j) * factor.qr(i, j);
    column_norms[j] = std::sqrt(norm);
  }
  for (std::size_t k = 0; k < n; ++k) {
    // Column pivoting.
    std::size_t pivot = k;
    for (std::size_t j = k + 1; j < n; ++j)
      if (column_norms[j] > column_norms[pivot]) pivot = j;
    if (pivot != k) {
      for (std::size_t i = 0; i < m; ++i) std::swap(factor.qr(i, k), factor.qr(i, pivot));
      std::swap(column_norms[k], column_norms[pivot]);
      std::swap(factor.column_scales[k], factor.column_scales[pivot]);
      std::swap(factor.permutation[k], factor.permutation[pivot]);
    }
    double norm = 0.0;
    for (std::size_t i = k; i < m; ++i) norm += factor.qr(i, k) * factor.qr(i, k);
    norm = std::sqrt(norm);
    if (norm == 0.0) continue;
    if (factor.qr(k, k) > 0.0) norm = -norm;
    for (std::size_t i = k; i < m; ++i) factor.qr(i, k) /= -norm;
    factor.qr(k, k) += 1.0;
    for (std::size_t j = k + 1; j < n; ++j) {
      double s = 0.0;
      for (std::size_t i = k; i < m; ++i) s += factor.qr(i, k) * factor.qr(i, j);
      s = -s / factor.qr(k, k);
      for (std::size_t i = k; i < m; ++i) factor.qr(i, j) += s * factor.qr(i, k);
    }
    for (std::size_t i = k + 1; i < m; ++i) factor.qr(i, k) = 0.0;
    column_norms[k] = norm;
  }
  // Rank from the trailing 2x2 block of R.
  double rnorm = 0.0;
  for (std::size_t j = 0; j < n; ++j) rnorm = std::max(rnorm, std::fabs(factor.qr(j, j)));
  const double tol = (m > 0 && n > 0) ? std::max(m, n) * kEps * rnorm : 0.0;
  factor.rank = 0;
  for (std::size_t j = 0; j < n; ++j)
    if (std::fabs(factor.qr(j, j)) > tol) ++factor.rank;
  factor.singular_values.resize(n);
  for (std::size_t j = 0; j < n; ++j) factor.singular_values[j] = std::fabs(factor.qr(j, j));
  return factor;
}

}  // namespace

LeastSquaresResult least_squares(const Matrix& a, const std::vector<double>& rhs,
                                  double rank_relative) {
  LeastSquaresResult out;
  const std::size_t m = a.rows, n = a.cols;
  if (m == 0 || n == 0) return out;
  QrFactor factor = householder_qr(a);

  std::vector<double> scaled_rhs(rhs.size(), 0.0);
  for (std::size_t i = 0; i < rhs.size(); ++i) scaled_rhs[i] = rhs[i];

  std::vector<double> y(m, 0.0);
  for (std::size_t k = 0; k < m; ++k) {
    double s = 0.0;
    for (std::size_t i = k; i < m; ++i) s += factor.qr(i, k) * scaled_rhs[i];
    s = -s / factor.qr(k, k);
    for (std::size_t i = k; i < m; ++i) scaled_rhs[i] += s * factor.qr(i, k);
  }
  for (std::size_t i = 0; i < m; ++i) y[i] = scaled_rhs[i];

  std::vector<double> z(n, 0.0);
  const std::size_t r = std::min(factor.rank, n);
  for (std::size_t k = r; k-- > 0;) {
    double sum = y[k];
    for (std::size_t j = k + 1; j < n; ++j) sum -= factor.qr(k, j) * z[j];
    z[k] = sum / factor.qr(k, k);
  }
  // Map the pivoted solution back to the original column order.
  out.solution.assign(n, 0.0);
  for (std::size_t j = 0; j < n; ++j) out.solution[factor.permutation[j]] = z[j] / factor.column_scales[j];

  const std::vector<double> fitted = matvec(a, out.solution);
  double residual_norm = 0.0;
  double inf_norm = 0.0;
  for (std::size_t i = 0; i < rhs.size() && i < fitted.size(); ++i) {
    const double d = rhs[i] - fitted[i];
    residual_norm += d * d;
    inf_norm = std::max(inf_norm, std::fabs(d));
  }
  out.residuals.push_back(std::sqrt(residual_norm));
  out.scaled_residual = inf_norm;

  const SvdResult svd = jacobi_svd(a, rank_relative);
  out.rank = svd.rank;
  out.rank_certified = svd.converged;
  out.rank_error = svd.sigma_error_bound;
  out.singular_values = svd.singular_values;
  out.converged = true;
  return out;
}

LeastSquaresResult least_squares(const Matrix& a, const std::vector<std::vector<double>>& rhs,
                                  double rank_relative) {
  LeastSquaresResult combined;
  const std::size_t columns = rhs.empty() ? 0 : rhs[0].size();
  std::vector<double> combined_rhs(columns, 0.0);
  LeastSquaresResult first;
  if (!rhs.empty()) {
    first = least_squares(a, rhs[0], rank_relative);
    combined_rhs = rhs[0];
  }
  combined.rank = first.rank;
  combined.rank_certified = first.rank_certified;
  combined.rank_error = first.rank_error;
  combined.singular_values = first.singular_values;
  combined.converged = first.converged;
  combined.residuals = first.residuals;
  combined.scaled_residual = first.scaled_residual;
  for (std::size_t c = 1; c < rhs.size(); ++c) {
    LeastSquaresResult single = least_squares(a, rhs[c], rank_relative);
    combined.residuals.push_back(single.residuals.empty() ? 0.0 : single.residuals[0]);
    combined.scaled_residual = std::max(combined.scaled_residual, single.scaled_residual);
  }
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
  Matrix gram(n, n);
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) {
      double sum = 0.0;
      for (std::size_t k = 0; k < n; ++k) sum += v(k, i) * v(k, j);
      gram(i, j) = sum;
    }
  for (std::size_t i = 0; i < n; ++i) gram(i, i) -= 1.0;
  out.delta_u = frobenius_norm(gram);
  Matrix reconstructed = matmul(a, v);
  for (std::size_t j = 0; j < n; ++j)
    for (std::size_t i = 0; i < n; ++i) reconstructed(i, j) -= out.eigenvalues[j] * v(i, j);
  out.residual = frobenius_norm(reconstructed);
  out.converged = true;
  return out;
}

}  // namespace acsd::numerics
