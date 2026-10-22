#pragma once
// Small dense linear algebra used by the science modules.
//
// The implementation is self contained (no external BLAS/LAPACK dependency) so
// that the numerical certificates required by EXECUTION (SVD orthogonality
// defects, reconstruction residual, rank classification with a scale aware
// threshold) can be produced for every decomposition.
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace acsd::numerics {

struct Matrix {
  std::size_t rows = 0;
  std::size_t cols = 0;
  std::vector<double> data;  // row-major

  Matrix() = default;
  Matrix(std::size_t r, std::size_t c) : rows(r), cols(c), data(r * c, 0.0) {}
  Matrix(std::size_t r, std::size_t c, std::vector<double> values)
      : rows(r), cols(c), data(std::move(values)) {}

  double& operator()(std::size_t i, std::size_t j) { return data[i * cols + j]; }
  double operator()(std::size_t i, std::size_t j) const { return data[i * cols + j]; }
  bool empty() const { return rows == 0 || cols == 0; }
  std::size_t size() const { return rows * cols; }
  std::vector<double> column(std::size_t j) const;
  void set_column(std::size_t j, const std::vector<double>& values);
};

Matrix identity_matrix(std::size_t n);
Matrix transpose(const Matrix& a);
Matrix matmul(const Matrix& a, const Matrix& b);
std::vector<double> matvec(const Matrix& a, const std::vector<double>& x);
double frobenius_norm(const Matrix& a);
double frobenius_norm_diff(const Matrix& a, const Matrix& b);
double max_abs(const Matrix& a);
double column_two_norm(const Matrix& a, std::size_t j);

struct SvdResult {
  Matrix u;                              // rows x k, orthonormal columns (k = min(rows, cols))
  Matrix v;                              // cols x k, orthonormal columns
  std::vector<double> singular_values;   // descending, length k
  double delta_u = 0.0;                  // ||U^T U - I||_F
  double delta_v = 0.0;                  // ||V^T V - I||_F
  double residual = 0.0;                 // ||A - U S V^T||_F
  double sigma_error_bound = 0.0;        // uniform bound on |sigma_computed - sigma_true|
  double spectral_norm = 0.0;
  std::size_t rank = 0;                  // classified rank at the requested threshold
  bool converged = false;
};

// One-sided Jacobi SVD.  `rank_relative` classifies the numerical rank.
SvdResult jacobi_svd(const Matrix& a, double rank_relative = 1e-12);

struct LeastSquaresResult {
  std::vector<double> solution;
  std::vector<double> residuals;   // per right-hand side residual norms
  double scaled_residual = 0.0;     // ||D^T r||_inf style measure
  std::size_t rank = 0;
  bool converged = false;
  bool rank_certified = false;
  double rank_error = 0.0;
  std::vector<double> singular_values;
};

// Householder QR least squares with column pivoting and column scaling.
LeastSquaresResult least_squares(const Matrix& a, const std::vector<std::vector<double>>& rhs,
                                  double rank_relative = 1e-12);
LeastSquaresResult least_squares(const Matrix& a, const std::vector<double>& rhs,
                                  double rank_relative = 1e-12);

// Solves a symmetric positive definite system by Cholesky; nullopt when the
// matrix is not positive definite to working precision.
std::optional<std::vector<double>> cholesky_solve(const Matrix& a, const std::vector<double>& b);
bool cholesky_factor(const Matrix& a, Matrix& l, double& min_diagonal);

struct EigenResult {
  std::vector<double> eigenvalues;  // ascending
  Matrix eigenvectors;               // columns are the eigenvectors
  double delta_u = 0.0;
  double residual = 0.0;
  bool converged = false;
};

// Jacobi eigenvalue iteration for symmetric matrices.
EigenResult symmetric_eigen(const Matrix& a);

// Rank classification helper shared by the fitting modules.
struct RankReport {
  std::size_t columns = 0;
  std::size_t rank = 0;
  std::vector<double> singular_values;
  std::vector<std::vector<double>> nullspace;
  double singular_error_bound = 0.0;
  bool certified = false;
  std::string note;
};

RankReport classify_rank(const SvdResult& svd, std::size_t columns, double rank_relative);

}  // namespace acsd::numerics
