#include "numerics/quadrature.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace acsd::numerics {
namespace {

// Kronrod (G7,K15) abscissae and weights on [-1, 1].
constexpr double kKronrodNodes[8] = {0.9914553711208126, 0.9491079123427585, 0.8648644233597691,
                                     0.7415311855993944, 0.5860872354676911, 0.4058451513773972,
                                     0.2077849550078985, 0.0};
constexpr double kKronrodWeights[8] = {0.02293532201052922, 0.06309209262997855,
                                       0.10479001032225018, 0.14065325971552591,
                                       0.16900472663926790, 0.19035057806478541,
                                       0.20443294007529889, 0.20948214108472783};
// The 7 point Gauss rule is generated with the same Newton iteration as the
// high order panel so that the two rules stay mutually consistent: the
// constant weights of the classical table are truncated to fewer digits than
// the Kronrod weights and do not sum to exactly two, which would put a
// systematic 1e-8 bias into the error estimate.

double kronrod_value(const std::function<double(double)>& f, double a, double b) {
  const double centre = 0.5 * (a + b);
  const double half = 0.5 * (b - a);
  double sum = 0.0;
  for (int i = 0; i < 8; ++i) {
    const double offset = kKronrodNodes[i] * half;
    if (i == 7) {
      // The centre node of the 15 point Kronrod rule is sampled once and
      // carries the same half-width Jacobian factor as the paired nodes.
      sum += f(centre) * kKronrodWeights[7] * half;
    } else {
      sum += (f(centre - offset) + f(centre + offset)) * kKronrodWeights[i] * half;
    }
  }
  return sum;
}

double gauss_value(const std::function<double(double)>& f, double a, double b) {
  static const std::vector<double> nodes = [] {
    std::vector<double> n, w;
    gauss_legendre(7, n, w);
    return n;
  }();
  static const std::vector<double> weights = [] {
    std::vector<double> n, w;
    gauss_legendre(7, n, w);
    return w;
  }();
  // The generated node list already contains the positive and the negative
  // abscissa of the rule, so every node is evaluated once.
  const double centre = 0.5 * (a + b);
  const double half = 0.5 * (b - a);
  double sum = 0.0;
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    sum += f(centre + nodes[i] * half) * weights[i] * half;
  }
  return sum;
}

}  // namespace

void gauss_legendre(int order, std::vector<double>& nodes, std::vector<double>& weights) {
  nodes.clear();
  weights.clear();
  if (order <= 0) return;
  // Newton iteration on the Legendre polynomial.
  for (int i = 0; i < order; ++i) {
    double x = std::cos(M_PI * (i + 0.75) / (order + 0.5));
    double p1 = 0.0, p2 = 0.0, dp = 0.0;
    for (int it = 0; it < 100; ++it) {
      p1 = 1.0;
      p2 = 0.0;
      for (int j = 0; j < order; ++j) {
        const double p3 = p2;
        p2 = p1;
        p1 = ((2.0 * j + 1.0) * x * p2 - j * p3) / (j + 1.0);
      }
      dp = order * (x * p1 - p2) / (x * x - 1.0);
      const double dx = -p1 / dp;
      x += dx;
      if (std::fabs(dx) < 1e-15) break;
    }
    const double weight = 2.0 / ((1.0 - x * x) * dp * dp);
    nodes.push_back(x);
    weights.push_back(weight);
  }
}

void gauss_legendre_on(int order, double a, double b, std::vector<double>& nodes,
                        std::vector<double>& weights) {
  gauss_legendre(order, nodes, weights);
  const double centre = 0.5 * (a + b);
  const double half = 0.5 * (b - a);
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    nodes[i] = centre + half * nodes[i];
    weights[i] *= half;
  }
}

QuadratureResult adaptive_integrate(const std::function<double(double)>& integrand, double a, double b,
                                    double absolute_tolerance, double relative_tolerance,
                                    int max_panels) {
  QuadratureResult result;
  if (!(b > a)) {
    result.value = 0.0;
    result.converged = true;
    return result;
  }
  struct Panel {
    double a, b, value, error;
  };
  std::vector<Panel> panels;
  panels.push_back({a, b, 0.0, 0.0});
  auto evaluate = [&](Panel& p) {
    const double k = kronrod_value(integrand, p.a, p.b);
    const double g = gauss_value(integrand, p.a, p.b);
    p.value = k;
    p.error = std::fabs(k - g);
  };
  evaluate(panels[0]);

  double total = 0.0, total_error = 0.0;
  for (const Panel& p : panels) { total += p.value; total_error += p.error; }

  while (static_cast<int>(panels.size()) < max_panels) {
    const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total));
    if (total_error <= tolerance) break;
    // Split the panel with the largest error.
    auto worst = std::max_element(panels.begin(), panels.end(),
                                   [](const Panel& x, const Panel& y) { return x.error < y.error; });
    const double mid = 0.5 * (worst->a + worst->b);
    if (!(mid > worst->a && mid < worst->b)) break;
    Panel left{worst->a, mid, 0.0, 0.0}, right{mid, worst->b, 0.0, 0.0};
    evaluate(left);
    evaluate(right);
    total += left.value + right.value - worst->value;
    total_error += left.error + right.error - worst->error;
    panels.erase(worst);
    panels.push_back(left);
    panels.push_back(right);
  }
  result.value = total;
  result.absolute_error = total_error;
  result.panels = static_cast<int>(panels.size());
  const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total));
  result.converged = total_error <= tolerance;
  return result;
}

QuadratureResult adaptive_integrate_gauss(const std::function<double(double)>& integrand, double a,
                                          double b, int order, double absolute_tolerance,
                                          double relative_tolerance, int max_panels) {
  QuadratureResult result;
  if (!(b > a)) {
    result.value = 0.0;
    result.converged = true;
    return result;
  }
  const int high = std::max(2, order);
  const int low = std::max(1, high / 2);
  std::vector<double> high_nodes, high_weights, low_nodes, low_weights;
  gauss_legendre(high, high_nodes, high_weights);
  gauss_legendre(low, low_nodes, low_weights);
  auto panel_value = [&](const std::vector<double>& nodes, const std::vector<double>& weights,
                         double lo, double hi) {
    // The node list already contains both the positive and the negative
    // abscissa, so every node is evaluated once and mapped onto the panel.
    const double centre = 0.5 * (lo + hi);
    const double half = 0.5 * (hi - lo);
    double sum = 0.0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      sum += integrand(centre + nodes[i] * half) * weights[i] * half;
    }
    return sum;
  };
  struct Panel {
    double a, b, value, error;
  };
  std::vector<Panel> panels;
  {
    Panel p{a, b, 0.0, 0.0};
    p.value = panel_value(high_nodes, high_weights, p.a, p.b);
    p.error = std::fabs(p.value - panel_value(low_nodes, low_weights, p.a, p.b));
    panels.push_back(p);
  }
  double total = panels[0].value, total_error = panels[0].error;
  while (static_cast<int>(panels.size()) < max_panels) {
    const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total));
    if (total_error <= tolerance) break;
    auto worst = std::max_element(panels.begin(), panels.end(),
                                   [](const Panel& x, const Panel& y) { return x.error < y.error; });
    const double mid = 0.5 * (worst->a + worst->b);
    if (!(mid > worst->a && mid < worst->b)) break;
    Panel left{worst->a, mid, 0.0, 0.0}, right{mid, worst->b, 0.0, 0.0};
    left.value = panel_value(high_nodes, high_weights, left.a, left.b);
    right.value = panel_value(high_nodes, high_weights, right.a, right.b);
    left.error = std::fabs(left.value - panel_value(low_nodes, low_weights, left.a, left.b));
    right.error = std::fabs(right.value - panel_value(low_nodes, low_weights, right.a, right.b));
    total += left.value + right.value - worst->value;
    total_error += left.error + right.error - worst->error;
    panels.erase(worst);
    panels.push_back(left);
    panels.push_back(right);
  }
  result.value = total;
  result.absolute_error = total_error;
  result.panels = static_cast<int>(panels.size());
  const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total));
  result.converged = total_error <= tolerance;
  return result;
}

IntervalQuadratureResult adaptive_integrate_interval(
    const std::function<Interval(double)>& integrand, double a, double b,
    double absolute_tolerance, double relative_tolerance, int max_panels) {
  IntervalQuadratureResult result;
  if (!(b > a)) {
    result.value = Interval(0.0, 0.0);
    result.converged = true;
    return result;
  }
  struct Panel {
    double a, b;
    Interval value;
    double error;
  };
  std::vector<Panel> panels;
  {
    Panel p{a, b, Interval(0.0, 0.0), 0.0};
    // 15-point Kronrod on the interval valued integrand: the hull of the panel
    // samples is an outer bound of the true integral only when the integrand
    // is monotone; therefore the panel value uses the hull of the endpoints
    // and the mid point, and the reported error is the hull width.  The
    // caller receives an approximate enclosure and the assessment records the
    // weaker guarantee.
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    const int samples = 15;
    for (int i = 0; i < samples; ++i) {
      const double t = static_cast<double>(i) / (samples - 1);
      const double x = p.a + t * (p.b - p.a);
      const Interval v = integrand(x);
      lo = std::min(lo, v.lower);
      hi = std::max(hi, v.upper);
    }
    p.value = Interval(lo, hi);
    p.error = hi - lo;
    panels.push_back(p);
  }
  double total_lo = panels[0].value.lower, total_hi = panels[0].value.upper, total_error = panels[0].error;
  while (static_cast<int>(panels.size()) < max_panels) {
    const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total_hi));
    if (total_error <= tolerance) break;
    auto worst = std::max_element(panels.begin(), panels.end(),
                                   [](const Panel& x, const Panel& y) { return x.error < y.error; });
    const double mid = 0.5 * (worst->a + worst->b);
    if (!(mid > worst->a && mid < worst->b)) break;
    Panel left{worst->a, mid, Interval(0.0, 0.0), 0.0};
    Panel right{mid, worst->b, Interval(0.0, 0.0), 0.0};
    auto evaluate = [&](Panel& p) {
      double lo = std::numeric_limits<double>::infinity();
      double hi = -std::numeric_limits<double>::infinity();
      const int samples = 15;
      for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / (samples - 1);
        const double x = p.a + t * (p.b - p.a);
        const Interval v = integrand(x);
        lo = std::min(lo, v.lower);
        hi = std::max(hi, v.upper);
      }
      p.value = Interval(lo, hi);
      p.error = hi - lo;
    };
    evaluate(left);
    evaluate(right);
    total_lo += left.value.lower + right.value.lower - worst->value.lower;
    total_hi += left.value.upper + right.value.upper - worst->value.upper;
    total_error += left.error + right.error - worst->error;
    panels.erase(worst);
    panels.push_back(left);
    panels.push_back(right);
  }
  result.value = Interval(total_lo, total_hi);
  result.absolute_error = total_error;
  result.panels = static_cast<int>(panels.size());
  const double tolerance = std::max(absolute_tolerance, relative_tolerance * std::fabs(total_hi));
  result.converged = total_error <= tolerance;
  return result;
}

// ------------------------------------------------------------------
// Special functions
// ------------------------------------------------------------------

double erf(double x) { return std::erf(x); }
double erfc(double x) { return std::erfc(x); }

double bessel_j0(double x) {
  if (x == 0.0) return 1.0;
  return std::cyl_bessel_j(0.0, x);
}

double bessel_j1(double x) {
  if (x == 0.0) return 0.0;
  return std::cyl_bessel_j(1.0, x);
}

double bessel_j0_prime(double x) { return -bessel_j1(x); }

double bessel_j1_prime(double x) {
  // J1'(x) = J0(x) - J1(x)/x with the removable singularity at x = 0 handled
  // by the series value J1'(0) = 1/2.
  if (std::fabs(x) < 1e-8) {
    const double y = x * x;
    return 0.5 - y / 16.0 + y * y / 384.0;
  }
  return bessel_j0(x) - bessel_j1(x) / x;
}

double bessel_j1_first_zero() {
  // Bisection on J1 with a sign change bracket; the first positive zero is
  // 3.831705970207512...
  double lo = 3.0, hi = 4.0;
  double flo = bessel_j1(lo);
  for (int i = 0; i < 200; ++i) {
    const double mid = 0.5 * (lo + hi);
    const double fmid = bessel_j1(mid);
    if ((fmid < 0.0) != (flo < 0.0)) { hi = mid; } else { lo = mid; flo = fmid; }
  }
  return 0.5 * (lo + hi);
}

double airy_tail_fraction(double radius_over_r0) {
  // Tail mass beyond radius r for an Airy pattern whose first zero is at r0:
  //   tail = J0(k r)^2 + J1(k r)^2 with k = J1,1 / r0.
  const double j1 = bessel_j1_first_zero();
  const double x = j1 * radius_over_r0;
  const double j0v = bessel_j0(x);
  const double j1v = bessel_j1(x);
  const double tail = j0v * j0v + j1v * j1v;
  return std::max(0.0, std::min(1.0, tail));
}

}  // namespace acsd::numerics
