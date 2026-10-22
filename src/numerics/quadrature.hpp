#pragma once
// Deterministic quadrature and special functions used by the science modules.
#include <functional>
#include <vector>

#include "numerics/interval.hpp"

namespace acsd::numerics {

// Gauss-Legendre nodes and weights on [-1, 1] for the requested order.
void gauss_legendre(int order, std::vector<double>& nodes, std::vector<double>& weights);
// Same nodes mapped onto [a, b].
void gauss_legendre_on(int order, double a, double b, std::vector<double>& nodes,
                        std::vector<double>& weights);

struct QuadratureResult {
  double value = 0.0;
  double absolute_error = 0.0;
  bool converged = false;
  int panels = 0;
};

// Adaptive Gauss-Kronrod (G7,K15) integration with an absolute and relative
// tolerance.  `integrand` must be deterministic.
QuadratureResult adaptive_integrate(const std::function<double(double)>& integrand, double a, double b,
                                    double absolute_tolerance, double relative_tolerance,
                                    int max_panels = 200);

// Adaptive integration of an interval valued integrand; the returned interval
// is the hull of all panel values.
struct IntervalQuadratureResult {
  Interval value;
  double absolute_error = 0.0;
  bool converged = false;
  int panels = 0;
};

IntervalQuadratureResult adaptive_integrate_interval(
    const std::function<Interval(double)>& integrand, double a, double b,
    double absolute_tolerance, double relative_tolerance, int max_panels = 200);

// ------------------------------------------------------------------
// Special functions
// ------------------------------------------------------------------

double erf(double x);
double erfc(double x);
// Bessel functions of the first kind J0 and J1, plus their derivatives.
double bessel_j0(double x);
double bessel_j1(double x);
double bessel_j0_prime(double x);
double bessel_j1_prime(double x);
// First positive zero of J1 (Airy first dark ring parameter).
double bessel_j1_first_zero();
// Encircled energy of an Airy pattern: 1 - J0(x)^2 - J1(x)^2 is the tail
// fraction beyond radius r for a first zero at r0 when x = J1,1 * r / r0.
double airy_tail_fraction(double radius_over_r0);

}  // namespace acsd::numerics
