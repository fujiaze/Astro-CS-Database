// Self test of the shared numerical primitives.
//
// The checks use closed form integrals so that a wrong rule cannot cancel out:
// the Kronrod/Gauss panel must reproduce the exact integral of a constant, of
// a linear and of a quadratic integrand, and the Gauss-Legendre panel must
// agree with them.  A rule that double counts the centre node or that drops
// the half-width Jacobian factor fails these checks immediately.
#include "numerics/linalg.hpp"
#include "numerics/quadrature.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using acsd::numerics::adaptive_integrate;
using acsd::numerics::adaptive_integrate_gauss;

namespace {

int failures = 0;

void check(const char* name, double got, double want, double tolerance) {
  const double relative = want != 0.0 ? std::fabs(got - want) / std::fabs(want) : std::fabs(got - want);
  const bool ok = relative <= tolerance;
  if (!ok) ++failures;
  std::printf("%-6s %-28s got=%.17g want=%.17g relative=%.3g\n", ok ? "ok" : "FAIL", name, got, want,
              relative);
}

}  // namespace

int main() {
  const auto constant = [](double) { return 1.0; };
  const auto linear = [](double x) { return x; };
  const auto quadratic = [](double x) { return x * x; };
  const auto sine = [](double x) { return std::sin(x); };

  const double a = 540.0, b = 560.0;
  auto r1 = adaptive_integrate(constant, a, b, 0.0, 1e-13, 50);
  auto r2 = adaptive_integrate(linear, a, b, 0.0, 1e-13, 50);
  auto r3 = adaptive_integrate(quadratic, a, b, 0.0, 1e-13, 50);
  auto r4 = adaptive_integrate(sine, a, b, 0.0, 1e-13, 50);
  check("kronrod constant", r1.value, 20.0, 1e-14);
  check("kronrod linear", r2.value, 11000.0, 1e-14);
  check("kronrod quadratic", r3.value, 6050666.6666666667, 1e-14);
  check("kronrod sine", r4.value, std::cos(a) - std::cos(b), 1e-11);
  if (!r1.converged || !r2.converged || !r3.converged || !r4.converged) {
    std::printf("FAIL   kronrod convergence flag\n");
    ++failures;
  }

  auto g1 = adaptive_integrate_gauss(constant, a, b, 32, 0.0, 1e-13, 50);
  auto g3 = adaptive_integrate_gauss(quadratic, a, b, 32, 0.0, 1e-13, 50);
  check("gauss32 constant", g1.value, 20.0, 1e-14);
  check("gauss32 quadratic", g3.value, 6050666.6666666667, 1e-14);
  if (!g1.converged || !g3.converged) {
    std::printf("FAIL   gauss convergence flag\n");
    ++failures;
  }

  // The SVD must return usable factors for both tall and wide inputs, and the
  // reconstruction must reproduce the original matrix.
  {
    acsd::numerics::Matrix tall(4, 2);
    tall(0, 0) = 1.0; tall(0, 1) = 2.0; tall(1, 0) = 0.0; tall(1, 1) = 1.0;
    tall(2, 0) = 3.0; tall(2, 1) = 1.0; tall(3, 0) = 1.0; tall(3, 1) = 0.0;
    acsd::numerics::Matrix wide(2, 4);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 4; ++j) wide(i, j) = tall(j, i);
    for (int pass = 0; pass < 2; ++pass) {
      const acsd::numerics::Matrix& matrix = pass == 0 ? tall : wide;
      const char* label = pass == 0 ? "svd tall" : "svd wide";
      const auto svd = acsd::numerics::jacobi_svd(matrix);
      double worst = 0.0;
      for (std::size_t i = 0; i < matrix.rows; ++i)
        for (std::size_t j = 0; j < matrix.cols; ++j) {
          double sum = 0.0;
          for (std::size_t k = 0; k < svd.singular_values.size(); ++k)
            sum += svd.u(i, k) * svd.singular_values[k] * svd.v(j, k);
          worst = std::max(worst, std::fabs(sum - matrix(i, j)));
        }
      check(label, worst, 0.0, 1e-12);
      check((std::string(label) + " residual").c_str(), svd.residual, 0.0, 1e-12);
      if (svd.u.rows != matrix.rows || svd.v.rows != matrix.cols) {
        std::printf("FAIL   %s factor dimensions\n", label);
        ++failures;
      }
    }
  }

  // The least squares path must return the true minimiser: the reflector
  // vector must not be confused with the diagonal of R, and the column
  // scaling and the permutation must be undone in the right order.
  {
    acsd::numerics::Matrix a(3, 2);
    a(0, 0) = 1.0; a(0, 1) = 0.0;
    a(1, 0) = 0.0; a(1, 1) = 1.0;
    a(2, 0) = 1.0; a(2, 1) = 1.0;
    const auto fit = acsd::numerics::least_squares(a, std::vector<double>{1.0, 2.0, 3.0}, 1e-12);
    check("least squares x0", fit.solution[0], 1.0, 1e-10);
    check("least squares x1", fit.solution[1], 2.0, 1e-10);
    double residual = 0.0;
    const auto fitted = acsd::numerics::matvec(a, fit.solution);
    for (std::size_t i = 0; i < fitted.size(); ++i) {
      const double want = i == 0 ? 1.0 : (i == 1 ? 2.0 : 3.0);
      residual = std::max(residual, std::fabs(fitted[i] - want));
    }
    check("least squares residual", residual, 0.0, 1e-10);

    acsd::numerics::Matrix identity(2, 2);
    identity(0, 0) = 1.0; identity(1, 1) = 1.0;
    const auto exact = acsd::numerics::least_squares(identity, std::vector<double>{1.0, 2.0}, 1e-12);
    check("least squares identity", exact.solution[0], 1.0, 1e-12);

    // A design matrix whose columns have very different scales: the column
    // scaling and the permutation must be undone in the right order, which a
    // matrix with unit column scales cannot detect.
    acsd::numerics::Matrix design(4, 3);
    const double rows[4][3] = {{0.00156462, -0.00351515, 1.0},
                                {-0.00138654, 0.00403870, 1.0},
                                {-0.00189771, -0.00155467, 1.0},
                                {0.00171963, 0.00103113, 1.0}};
    for (std::size_t i = 0; i < 4; ++i)
      for (std::size_t j = 0; j < 3; ++j) design(i, j) = rows[i][j];
    const std::vector<double> rhs{755.0, 927.0, 939.0, 759.0};
    const auto scaled = acsd::numerics::least_squares(design, rhs, 1e-12);
    // Independent reference: the normal equations solved by Gaussian
    // elimination with partial pivoting.
    double normal[3][4] = {{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0}};
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < 3; ++j)
        for (std::size_t k = 0; k < 4; ++k) normal[i][j] += design(k, i) * design(k, j);
      for (std::size_t k = 0; k < 4; ++k) normal[i][3] += design(k, i) * rhs[k];
    }
    for (std::size_t col = 0; col < 3; ++col) {
      std::size_t pivot = col;
      for (std::size_t r = col + 1; r < 3; ++r)
        if (std::fabs(normal[r][col]) > std::fabs(normal[pivot][col])) pivot = r;
      for (std::size_t j = 0; j < 4; ++j) std::swap(normal[col][j], normal[pivot][j]);
      for (std::size_t r = col + 1; r < 3; ++r) {
        const double factor = normal[r][col] / normal[col][col];
        for (std::size_t j = col; j < 4; ++j) normal[r][j] -= factor * normal[col][j];
      }
    }
    double reference[3] = {0.0, 0.0, 0.0};
    for (std::size_t i = 3; i-- > 0;) {
      double sum = normal[i][3];
      for (std::size_t j = i + 1; j < 3; ++j) sum -= normal[i][j] * reference[j];
      reference[i] = sum / normal[i][i];
    }
    for (std::size_t i = 0; i < 3; ++i) {
      const double relative = std::fabs(scaled.solution[i] - reference[i]) /
                              std::max(1.0, std::fabs(reference[i]));
      if (relative > 1e-8) {
        std::printf("FAIL   least squares scaled column %zu got=%.12g want=%.12g\n", i,
                    scaled.solution[i], reference[i]);
        ++failures;
      } else {
        std::printf("ok     least squares scaled column %zu got=%.12g want=%.12g\n", i,
                    scaled.solution[i], reference[i]);
      }
    }

    const auto many = acsd::numerics::least_squares(
        identity, std::vector<std::vector<double>>{{1.0, 2.0}, {3.0, 4.0}}, 1e-12);
    if (many.solutions.size() != 2) {
      std::printf("FAIL   least squares multiple right hand sides\n");
      ++failures;
    } else {
      check("least squares multi rhs", many.solutions[1][1], 4.0, 1e-12);
    }
  }

  // The pseudo inverse must reproduce the least squares solution: for a
  // consistent system A x = b the pseudo inverse gives the minimum norm
  // solution, and A A^+ is the orthogonal projection onto the row space.
  {
    acsd::numerics::Matrix a(3, 2);
    a(0, 0) = 1.0; a(0, 1) = 2.0;
    a(1, 0) = 0.0; a(1, 1) = 0.0;
    a(2, 0) = 1.0; a(2, 1) = 0.0;
    const auto svd = acsd::numerics::jacobi_svd(a, 1e-12);
    const auto inverse = acsd::numerics::pseudo_inverse(svd, 2, 1e-12);
    // A A^+ A must reproduce A.
    const auto product = acsd::numerics::matmul(a, acsd::numerics::matmul(inverse, a));
    double worst = 0.0;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) {
        const double want = a(i, j);
        const double difference = std::fabs(product(i, j) - want);
        if (difference > worst) worst = difference;
      }
    if (worst < 1e-10) {
      std::printf("ok     pseudo inverse projects onto the row space\n");
    } else {
      std::printf("FAIL   pseudo inverse projects onto the row space (worst %g)\n", worst);
      ++failures;
    }
  }

  std::printf("%s: %d failure(s)\n", failures == 0 ? "numerics self test PASS" : "numerics self test FAIL",
              failures);
  return failures == 0 ? 0 : 1;
}
