// tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h
//
// 独立 Oracle（IMPL-P3-RSMP-001）。
// 独立性约束（docs/validation/v6/ORACLE_AND_ZERO_CASE_POLICY.md §1）：
//   * 本头文件只使用 C++ 标准库，**不 include / 不链接 / 不执行任何生产实现**
//     （不 include lib/algorithms/resample 的头，不调用 astrocs::p3rsmp::*）。
//   * 真值来源：解析式（双线性误差界、面积重叠归一）与固定种子 Monte Carlo
//     （SEED=20260915）。
//   * 线性代数独立转写：一般式直接用三重循环显式矩阵乘；线性方程解用带部分主元的高斯消元
//     （与生产 Cholesky 不同算法）→ 交叉校验 C_y 与 Q/W。
#ifndef P3_RSMP_ORACLE_H
#define P3_RSMP_ORACLE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace p3oracle {

constexpr unsigned long long kSeed = 20260915ULL;

struct Mat {
  int r = 0;
  int c = 0;
  std::vector<double> a;
  Mat() = default;
  Mat(int r_, int c_) : r(r_), c(c_), a(static_cast<std::size_t>(r_) * c_, 0.0) {}
  double& operator()(int i, int j) { return a[static_cast<std::size_t>(i) * c + j]; }
  double operator()(int i, int j) const { return a[static_cast<std::size_t>(i) * c + j]; }
};

inline Mat matmul(const Mat& A, const Mat& B) {
  Mat C(A.r, B.c);
  for (int i = 0; i < A.r; ++i) {
    for (int k = 0; k < A.c; ++k) {
      const double aik = A(i, k);
      if (aik == 0.0) continue;
      for (int j = 0; j < B.c; ++j) C(i, j) += aik * B(k, j);
    }
  }
  return C;
}

inline Mat transpose(const Mat& A) {
  Mat T(A.c, A.r);
  for (int i = 0; i < A.r; ++i)
    for (int j = 0; j < A.c; ++j) T(j, i) = A(i, j);
  return T;
}

inline std::vector<double> matvec(const Mat& A, const std::vector<double>& x) {
  std::vector<double> y(static_cast<std::size_t>(A.r), 0.0);
  for (int i = 0; i < A.r; ++i) {
    double s = 0.0;
    for (int j = 0; j < A.c; ++j) s += A(i, j) * x[static_cast<std::size_t>(j)];
    y[static_cast<std::size_t>(i)] = s;
  }
  return y;
}

inline std::vector<double> diagonal(const Mat& A) {
  const int n = (A.r < A.c) ? A.r : A.c;
  std::vector<double> d(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) d[static_cast<std::size_t>(i)] = A(i, i);
  return d;
}

inline double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double s = 0.0;
  for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) s += a[i] * b[i];
  return s;
}

// 显式三重循环的 C_out = op C_in opᵀ（与生产 sparse 传播不同实现）。
inline Mat covariance(const Mat& op, const Mat& c_in) {
  return matmul(matmul(op, c_in), transpose(op));
}

struct Ov {
  int o = 0;
  int i = 0;
  double area = 0.0;
};

inline Mat build_R(const std::vector<Ov>& ov, const std::vector<double>& omega_out, int n_out,
                   int n_in) {
  Mat R(n_out, n_in);
  for (const Ov& e : ov) {
    if (e.area > 0.0) R(e.o, e.i) += e.area / omega_out[static_cast<std::size_t>(e.o)];
  }
  return R;
}
inline Mat build_S(const std::vector<Ov>& ov, const std::vector<double>& omega_in, int n_out,
                   int n_in) {
  Mat S(n_out, n_in);
  for (const Ov& e : ov) {
    if (e.area > 0.0) S(e.o, e.i) += e.area / omega_in[static_cast<std::size_t>(e.i)];
  }
  return S;
}

// 带部分主元的高斯消元（独立于生产 Cholesky）。失败返回 false。
inline bool solve(const Mat& A, const std::vector<double>& b, std::vector<double>* x) {
  const int n = A.r;
  if (A.c != n || static_cast<int>(b.size()) != n) return false;
  std::vector<std::vector<double>> m(static_cast<std::size_t>(n),
                                     std::vector<double>(static_cast<std::size_t>(n) + 1, 0.0));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = A(i, j);
    m[static_cast<std::size_t>(i)][static_cast<std::size_t>(n)] = b[static_cast<std::size_t>(i)];
  }
  for (int col = 0; col < n; ++col) {
    int piv = col;
    for (int r2 = col + 1; r2 < n; ++r2) {
      if (std::fabs(m[static_cast<std::size_t>(r2)][static_cast<std::size_t>(col)]) >
          std::fabs(m[static_cast<std::size_t>(piv)][static_cast<std::size_t>(col)])) {
        piv = r2;
      }
    }
    if (std::fabs(m[static_cast<std::size_t>(piv)][static_cast<std::size_t>(col)]) < 1e-300) {
      return false;
    }
    if (piv != col) std::swap(m[static_cast<std::size_t>(piv)], m[static_cast<std::size_t>(col)]);
    const double d = m[static_cast<std::size_t>(col)][static_cast<std::size_t>(col)];
    for (int r2 = col + 1; r2 < n; ++r2) {
      const double f = m[static_cast<std::size_t>(r2)][static_cast<std::size_t>(col)] / d;
      if (f == 0.0) continue;
      for (int cc = col; cc <= n; ++cc) {
        m[static_cast<std::size_t>(r2)][static_cast<std::size_t>(cc)] -=
            f * m[static_cast<std::size_t>(col)][static_cast<std::size_t>(cc)];
      }
    }
  }
  x->assign(static_cast<std::size_t>(n), 0.0);
  for (int i = n - 1; i >= 0; --i) {
    double s = m[static_cast<std::size_t>(i)][static_cast<std::size_t>(n)];
    for (int j = i + 1; j < n; ++j) {
      s -= m[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] * (*x)[static_cast<std::size_t>(j)];
    }
    (*x)[static_cast<std::size_t>(i)] = s / m[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)];
  }
  return true;
}

inline bool cholesky_lower(const Mat& A, Mat* L) {
  const int n = A.r;
  if (A.c != n) return false;
  *L = Mat(n, n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j <= i; ++j) {
      double s = A(i, j);
      for (int k = 0; k < j; ++k) s -= (*L)(i, k) * (*L)(j, k);
      if (i == j) {
        if (!(s > 0.0)) return false;
        (*L)(i, i) = std::sqrt(s);
      } else {
        (*L)(i, j) = s / (*L)(j, j);
      }
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// bilinear_4quad 独立 Oracle（ALG-P3-001_KERNEL_REGISTRY §3）
// 场 F=exp(-r²/(2s²))，s=3，h=1，网格 141×141，峰值在 (70.5,70.5)（像素中心之间的整数位，
// 即误差最大处）；输入样本在整数坐标，输出在 ix+0.5。
// ---------------------------------------------------------------------------
struct KernelOracle {
  double max_interp_err = 0.0;
  double analytic_bound = 0.0;
  double err_over_bound = 0.0;
  double max_weight_sum_dev = 0.0;
  double nearest_max_err = 0.0;
  double nearest_over_bound = 0.0;
  double zero_fill_error = 0.0;  // 把峰值邻域一个样本强制 0 的总影响（Σ|Δ|）
  bool boundary_fail_closed = true;
};

inline KernelOracle run_kernel_oracle() {
  const double s = 3.0;
  const int N = 141;
  const double x0 = 70.5;
  const double y0 = 70.5;
  const auto F = [&](double x, double y) {
    return std::exp(-(((x - x0) * (x - x0)) + ((y - y0) * (y - y0))) / (2.0 * s * s));
  };
  KernelOracle ko;
  ko.analytic_bound = (1.0 / 8.0) * (2.0 / (s * s));
  double mx = 0.0;
  double mdev = 0.0;
  double near = 0.0;
  for (int iy = 0; iy + 1 < N; ++iy) {
    for (int ix = 0; ix + 1 < N; ++ix) {
      const double x = ix + 0.5;
      const double y = iy + 0.5;
      const double w00 = 0.25;
      const double w10 = 0.25;
      const double w01 = 0.25;
      const double w11 = 0.25;
      mdev = std::max(mdev, std::fabs((w00 + w10 + w01 + w11) - 1.0));
      const double v = w00 * F(ix, iy) + w10 * F(ix + 1, iy) + w01 * F(ix, iy + 1) +
                       w11 * F(ix + 1, iy + 1);
      mx = std::max(mx, std::fabs(v - F(x, y)));
      const double vn = F(std::round(x), std::round(y));
      near = std::max(near, std::fabs(vn - F(x, y)));
    }
  }
  ko.max_interp_err = mx;
  ko.err_over_bound = mx / ko.analytic_bound;
  ko.max_weight_sum_dev = mdev;
  ko.nearest_max_err = near;
  ko.nearest_over_bound = near / ko.analytic_bound;
  // 零填影响：峰值邻域样本 (70,70) 被强制为 0；受影响的 4 个输出点权重各 0.25。
  ko.zero_fill_error = 4.0 * 0.25 * F(70.0, 70.0);
  return ko;
}

// ---------------------------------------------------------------------------
// Q/W 输出帧重算的固定种子 Monte Carlo：Var(F_hat) 应 == 1/W。
//   F_hat = (piᵀ C_y⁻¹ f) / (a piᵀ C_y⁻¹ pi)
// 预计算 q = C_y⁻¹ pi，则分子 = qᵀ f，分母 = a·qᵀ pi。
// ---------------------------------------------------------------------------
inline double mc_var_Fhat(const Mat& S, const std::vector<double>& pi, const Mat& c_d, double a,
                          std::size_t n_draws, double* mean_out, double* rel_err_out) {
  const Mat c_y = covariance(S, c_d);
  std::vector<double> q;
  if (!solve(c_y, pi, &q)) return NAN;
  const double denom = a * dot(q, pi);
  Mat L;
  if (!cholesky_lower(c_d, &L)) return NAN;
  std::mt19937_64 rng(kSeed);
  std::normal_distribution<double> gauss(0.0, 1.0);
  const int n = c_d.r;
  std::vector<double> gvec(static_cast<std::size_t>(n), 0.0);
  double sum = 0.0;
  double sum2 = 0.0;
  for (std::size_t t = 0; t < n_draws; ++t) {
    // d = L g：同一 g 向量在所有分量间共享（Cholesky 采样的正确口径）
    for (int i = 0; i < n; ++i) gvec[static_cast<std::size_t>(i)] = gauss(rng);
    const std::vector<double> d = matvec(L, gvec);
    const std::vector<double> f = matvec(S, d);
    const double fhat = dot(q, f) / denom;
    sum += fhat;
    sum2 += fhat * fhat;
  }
  const double mean = sum / static_cast<double>(n_draws);
  const double var = (sum2 - static_cast<double>(n_draws) * mean * mean) /
                     static_cast<double>(n_draws - 1);
  if (mean_out) *mean_out = mean;
  // W = a^2 * piᵀ C_y⁻¹ pi = a * denom；断言 |var*W - 1|
  if (rel_err_out) *rel_err_out = std::fabs(var * a * denom - 1.0);
  return var;
}

}  // namespace p3oracle

#endif  // P3_RSMP_ORACLE_H
