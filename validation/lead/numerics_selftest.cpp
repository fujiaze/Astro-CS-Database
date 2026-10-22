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

  std::printf("%s: %d failure(s)\n", failures == 0 ? "numerics self test PASS" : "numerics self test FAIL",
              failures);
  return failures == 0 ? 0 : 1;
}
