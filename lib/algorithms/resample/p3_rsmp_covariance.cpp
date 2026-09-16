// lib/algorithms/resample/p3_rsmp_covariance.cpp
// covariance 传播 C_y = R C_x Rᵀ（FZ-FORMULA-COV-PROP；唯一来源，禁权重标量反推）。
#include "p3_rsmp.h"

#include <cmath>

namespace astrocs {
namespace p3rsmp {

DenseMatrix DenseMatrix::identity(int n) {
  DenseMatrix m(n, n);
  for (int i = 0; i < n; ++i) m(i, i) = 1.0;
  return m;
}
DenseMatrix DenseMatrix::diagonal(const std::vector<double>& d) {
  const int n = static_cast<int>(d.size());
  DenseMatrix m(n, n);
  for (int i = 0; i < n; ++i) m(i, i) = d[static_cast<std::size_t>(i)];
  return m;
}

DenseMatrix propagate_covariance(const SparseOperator& op, const DenseMatrix& c_in) {
  const int no = op.n_out;
  const int ni = op.n_in;
  // 防御：维度必须自洽，否则返回空矩阵（调用方 fail-closed）。
  if (c_in.rows != ni || c_in.cols != ni) return DenseMatrix();
  // M = R C_x  (n_out × n_in)，M[o][k] = Σ_i R_oi C_x[i][k]
  DenseMatrix m(no, ni);
  for (std::size_t e = 0; e < op.weight.size(); ++e) {
    const int o = op.out_idx[e];
    const int i = op.in_idx[e];
    const double w = op.weight[e];
    for (int k = 0; k < ni; ++k) {
      m(o, k) += w * c_in(i, k);
    }
  }
  // C_y = M Rᵀ  → C_y[o][p] = Σ_i M[o][i] R[p][i]
  DenseMatrix cy(no, no);
  for (std::size_t e = 0; e < op.weight.size(); ++e) {
    const int p = op.out_idx[e];
    const int i = op.in_idx[e];
    const double w = op.weight[e];
    for (int o = 0; o < no; ++o) {
      cy(o, p) += m(o, i) * w;
    }
  }
  return cy;
}

std::vector<double> matrix_diagonal(const DenseMatrix& m) {
  const int n = (m.rows < m.cols) ? m.rows : m.cols;
  std::vector<double> d(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) d[static_cast<std::size_t>(i)] = m(i, i);
  return d;
}

DenseMatrix correlation_kernel(const DenseMatrix& cy) {
  const int n = cy.rows;
  DenseMatrix rho(n, n);
  std::vector<double> sd(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    const double v = cy(i, i);
    sd[static_cast<std::size_t>(i)] = (v > 0.0) ? std::sqrt(v) : kNaN;
  }
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      if (i == j) {
        rho(i, j) = (sd[static_cast<std::size_t>(i)] > 0.0) ? 1.0 : kNaN;
      } else {
        const double den = sd[static_cast<std::size_t>(i)] * sd[static_cast<std::size_t>(j)];
        rho(i, j) = (den > 0.0) ? cy(i, j) / den : 0.0;
      }
    }
  }
  return rho;
}

double max_abs_offdiag_correlation(const DenseMatrix& cy) {
  const DenseMatrix rho = correlation_kernel(cy);
  double mx = 0.0;
  for (int i = 0; i < rho.rows; ++i) {
    for (int j = 0; j < rho.cols; ++j) {
      if (i == j) continue;
      const double v = std::fabs(rho(i, j));
      if (std::isnan(v)) continue;
      if (v > mx) mx = v;
    }
  }
  return mx;
}

CholeskyResult cholesky_factor(const DenseMatrix& a) {
  CholeskyResult r;
  if (a.rows != a.cols || a.rows <= 0) {
    r.reason = "matrix_not_square";
    return r;
  }
  const int n = a.rows;
  r.L = DenseMatrix(n, n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j <= i; ++j) {
      double sum = a(i, j);
      for (int k = 0; k < j; ++k) {
        sum -= r.L(i, k) * r.L(j, k);
      }
      if (i == j) {
        if (!(sum > 0.0) || std::isnan(sum)) {
          r.reason = "covariance_not_positive_definite";
          r.ok = false;
          return r;
        }
        r.L(i, i) = std::sqrt(sum);
      } else {
        r.L(i, j) = sum / r.L(j, j);
      }
    }
  }
  r.ok = true;
  return r;
}

Status cholesky_solve(const DenseMatrix& c, const std::vector<double>& b, std::vector<double>* z,
                      std::string* reason) {
  if (z == nullptr) return Status::InvalidArgument;
  if (static_cast<int>(b.size()) != c.rows) {
    if (reason) *reason = "rhs_size_mismatch";
    return Status::InvalidArgument;
  }
  const CholeskyResult f = cholesky_factor(c);
  if (!f.ok) {
    if (reason) *reason = f.reason;
    return Status::Reject;  // 非正定/奇异 → fail-closed，禁伪逆静默
  }
  const int n = c.rows;
  std::vector<double> y(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    double sum = b[static_cast<std::size_t>(i)];
    for (int k = 0; k < i; ++k) sum -= f.L(i, k) * y[static_cast<std::size_t>(k)];
    y[static_cast<std::size_t>(i)] = sum / f.L(i, i);
  }
  z->assign(static_cast<std::size_t>(n), 0.0);
  for (int i = n - 1; i >= 0; --i) {
    double sum = y[static_cast<std::size_t>(i)];
    for (int k = i + 1; k < n; ++k) sum -= f.L(k, i) * (*z)[static_cast<std::size_t>(k)];
    (*z)[static_cast<std::size_t>(i)] = sum / f.L(i, i);
  }
  return Status::Ok;
}

}  // namespace p3rsmp
}  // namespace astrocs
