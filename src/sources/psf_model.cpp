// The PSF model of the sources module: parameterisation, density, analytic
// tail bounds and the single pixel integral implementation.
#include "sources/psf_internal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/quadrature.hpp"

namespace acsd::sources::psfmodel {
namespace {

constexpr double kPi = 3.14159265358979323846;

// The Gauss-Legendre nodes and weights of the rules used by the block
// quadrature.
void gauss_rule(int order, std::vector<double>& nodes, std::vector<double>& weights) {
  numerics::gauss_legendre(order, nodes, weights);
}

struct RectRule {
  std::vector<double> nodes;
  std::vector<double> weights;
};

const RectRule& coarse_rule() {
  static const RectRule rule = [] {
    RectRule out;
    gauss_rule(3, out.nodes, out.weights);
    return out;
  }();
  return rule;
}

const RectRule& fine_rule() {
  static const RectRule rule = [] {
    RectRule out;
    gauss_rule(6, out.nodes, out.weights);
    return out;
  }();
  return rule;
}

double rectangle_estimate(const std::function<double(double, double)>& f, double x0, double x1,
                          double y0, double y1, const RectRule& rule) {
  const double hx = 0.5 * (x1 - x0);
  const double hy = 0.5 * (y1 - y0);
  const double cx = 0.5 * (x1 + x0);
  const double cy = 0.5 * (y1 + y0);
  double sum = 0.0;
  for (std::size_t i = 0; i < rule.nodes.size(); ++i) {
    const double x = cx + hx * rule.nodes[i];
    double inner = 0.0;
    for (std::size_t j = 0; j < rule.nodes.size(); ++j) {
      const double y = cy + hy * rule.nodes[j];
      inner += rule.weights[j] * f(x, y);
    }
    sum += rule.weights[i] * inner;
  }
  return sum * hx * hy;
}

// The hard limit on the number of panels of one two dimensional integral.
// The bisection below is four-way, so a depth limit alone allows 4^depth
// panels: with depth 18 that is 6.8e10, which is not a termination guarantee.
// A starved integral returns converged = false together with the bound it has,
// and the caller refuses to certify the template.
constexpr std::size_t kMaximumPanels = 4096;

// The adaptive block quadrature.  Every block carries its own error estimate;
// the total error is the sum of the block errors.
Integral adaptive_block(const std::function<double(double, double)>& f, double x0, double x1,
                        double y0, double y1, double tolerance, int depth, int max_depth,
                        int& panels) {
  const double fine = rectangle_estimate(f, x0, x1, y0, y1, fine_rule());
  const double coarse = rectangle_estimate(f, x0, x1, y0, y1, coarse_rule());
  const double error = std::fabs(fine - coarse);
  ++panels;
  // A non-finite value cannot be refined: the block is returned as is, not
  // converged, and the error is reported as infinite so that no caller can
  // mistake it for a certified integral.
  if (!std::isfinite(fine) || !std::isfinite(coarse)) {
    Integral out;
    out.value = 0.0;
    out.error = std::numeric_limits<double>::infinity();
    out.converged = false;
    out.panels = panels;
    return out;
  }
  if (error <= tolerance || depth >= max_depth ||
      static_cast<std::size_t>(panels) >= kMaximumPanels) {
    Integral out;
    out.value = fine;
    out.error = error;
    out.converged = error <= tolerance;
    out.panels = panels;
    return out;
  }
  const double xm = 0.5 * (x0 + x1);
  const double ym = 0.5 * (y0 + y1);
  Integral a = adaptive_block(f, x0, xm, y0, ym, 0.25 * tolerance, depth + 1, max_depth, panels);
  Integral b = adaptive_block(f, xm, x1, y0, ym, 0.25 * tolerance, depth + 1, max_depth, panels);
  Integral c = adaptive_block(f, x0, xm, ym, y1, 0.25 * tolerance, depth + 1, max_depth, panels);
  Integral d = adaptive_block(f, xm, x1, ym, y1, 0.25 * tolerance, depth + 1, max_depth, panels);
  Integral out;
  out.value = a.value + b.value + c.value + d.value;
  out.error = a.error + b.error + c.error + d.error;
  out.converged = a.converged && b.converged && c.converged && d.converged;
  out.panels = panels;
  return out;
}

// The even-odd membership test of the footprint rings.
bool inside_rings(const std::vector<std::vector<PixelEdge0>>& rings, double x, double y) {
  bool inside = false;
  for (const std::vector<PixelEdge0>& ring : rings) {
    for (std::size_t i = 0; i + 1 < ring.size(); ++i) {
      const double x0 = ring[i].x;
      const double y0 = ring[i].y;
      const double x1 = ring[i + 1].x;
      const double y1 = ring[i + 1].y;
      if ((y0 > y) != (y1 > y)) {
        const double t = (y - y0) / (y1 - y0);
        if (x < x0 + t * (x1 - x0)) inside = !inside;
      }
    }
  }
  return inside;
}

}  // namespace

std::size_t family_parameter_count(PsfRequest::Family family) {
  switch (family) {
    case PsfRequest::Family::gaussian: return 3;
    case PsfRequest::Family::moffat: return 4;
    case PsfRequest::Family::airy: return 1;
  }
  return 0;
}

std::size_t basis_term_count(U32 degree) {
  if (degree == 0) return 1;
  if (degree == 1) return 3;
  if (degree == 2) return 6;
  return 0;
}

std::vector<double> basis_values(const Basis& basis, U32 degree, double x, double y) {
  const double u = (x - basis.center[0]) / basis.scale[0];
  const double v = (y - basis.center[1]) / basis.scale[1];
  std::vector<double> values;
  values.push_back(1.0);
  if (degree == 0) return values;
  values.push_back(u);
  values.push_back(v);
  if (degree == 1) return values;
  values.push_back(u * u);
  values.push_back(u * v);
  values.push_back(v * v);
  return values;
}

std::vector<double> local_parameters(PsfRequest::Family family,
                                     const std::vector<double>& coefficients,
                                     const std::vector<double>& basis_vector) {
  const std::size_t count = family_parameter_count(family);
  std::vector<double> local(count, 0.0);
  for (std::size_t r = 0; r < count; ++r) {
    double sum = 0.0;
    for (std::size_t k = 0; k < basis_vector.size(); ++k) {
      sum += coefficients[r * basis_vector.size() + k] * basis_vector[k];
    }
    local[r] = sum;
  }
  return local;
}

namespace {

// The bounds of the local parameters: the width of the profile in pixels, the
// correlation through atanh and the exponent of the Moffat.
constexpr double kLogWidthLower = -6.907755278982137;   // log(1e-3)
constexpr double kLogWidthUpper = 6.907755278982137;    // log(1e3)
constexpr double kCorrelationLimit = 9.0;                // tanh(9) is below one
constexpr double kLogExponentLower = -6.907755278982137;
constexpr double kLogExponentUpper = 6.907755278982137;

}  // namespace

bool local_parameters_valid(PsfRequest::Family family, const std::vector<double>& local,
                            std::string& reason) {
  if (local.empty()) {
    reason = "the model has no local parameters";
    return false;
  }
  if (!std::isfinite(local[0])) {
    reason = "the width of the profile is not a finite number";
    return false;
  }
  if (family == PsfRequest::Family::airy) {
    if (!(std::exp(local[0]) > 0.0) || !std::isfinite(std::exp(local[0]))) {
      reason = "the first dark ring radius of the Airy pattern is not positive";
      return false;
    }
    return true;
  }
  if (local.size() < 3) {
    reason = "the model has too few local parameters";
    return false;
  }
  if (!(std::exp(local[0]) > 0.0) || !std::isfinite(std::exp(local[0]))) {
    reason = "the width of the profile along x is not positive and finite";
    return false;
  }
  if (!(std::exp(local[1]) > 0.0) || !std::isfinite(std::exp(local[1]))) {
    reason = "the width of the profile along y is not positive and finite";
    return false;
  }
  if (!std::isfinite(local[2])) {
    reason = "the correlation of the profile is not a finite number";
    return false;
  }
  // The correlation is the hyperbolic tangent of the parameter, so the
  // parameter itself may be large; the tangent saturates at one, and a
  // correlation of one is not a density.
  const double rho = std::tanh(local[2]);
  if (!(std::fabs(rho) < 1.0)) {
    reason = "the correlation of the profile reaches one, so the model is not a density";
    return false;
  }
  if (family == PsfRequest::Family::moffat) {
    if (local.size() < 4) {
      reason = "the Moffat model has too few local parameters";
      return false;
    }
    if (!(std::exp(local[3]) > 0.0) || !std::isfinite(std::exp(local[3]))) {
      reason = "the exponent of the Moffat profile is not positive and finite";
      return false;
    }
  }
  return true;
}

double local_parameter_margin(PsfRequest::Family family, const std::vector<double>& local,
                              std::size_t parameter) {
  if (family == PsfRequest::Family::airy) {
    if (parameter == 0) {
      const double value = local[0];
      return std::max(kLogWidthUpper - value, value - kLogWidthLower);
    }
    return 0.0;
  }
  // The margin is the distance to the nearest bound of the valid domain.
  if (parameter == 0) {
    return std::min(kLogWidthUpper - local[0], local[0] - kLogWidthLower);
  }
  if (parameter == 1) {
    return std::min(kLogWidthUpper - local[1], local[1] - kLogWidthLower);
  }
  if (parameter == 2) {
    return std::min(kCorrelationLimit - local[2], local[2] + kCorrelationLimit);
  }
  if (parameter == 3) {
    return std::min(kLogExponentUpper - local[3], local[3] - kLogExponentLower);
  }
  return 0.0;
}

double density(PsfRequest::Family family, const std::vector<double>& local, double dx, double dy) {
  // An invalid parameter does not describe a density.  The value is poisoned
  // so that a caller that skipped the check cannot use a zero as if it were
  // a computed density; the builders check the parameters first and report a
  // named domain failure.
  const double invalid = std::numeric_limits<double>::quiet_NaN();
  if (family == PsfRequest::Family::airy) {
    const double r0 = std::exp(local[0]);
    if (!(r0 > 0.0)) return invalid;
    const double k = numerics::bessel_j1_first_zero() / r0;
    const double r = std::sqrt(dx * dx + dy * dy);
    if (r <= 0.0) return k * k / (4.0 * kPi);
    const double kr = k * r;
    const double ratio = 2.0 * numerics::bessel_j1(kr) / kr;
    return k * k / (4.0 * kPi) * ratio * ratio;
  }
  const double sx = std::exp(local[0]);
  const double sy = std::exp(local[1]);
  const double rho = std::tanh(local[2]);
  if (!(sx > 0.0) || !(sy > 0.0) || !(1.0 - rho * rho > 0.0)) return invalid;
  const double cross = rho * sx * sy;
  const double determinant = sx * sx * sy * sy - cross * cross;
  if (!(determinant > 0.0)) return invalid;
  const double ixx = sy * sy / determinant;
  const double iyy = sx * sx / determinant;
  const double ixy = -cross / determinant;
  const double m2 = ixx * dx * dx + 2.0 * ixy * dx * dy + iyy * dy * dy;
  if (family == PsfRequest::Family::gaussian) {
    if (m2 < 0.0) return invalid;
    return std::exp(-0.5 * m2) / (2.0 * kPi * std::sqrt(determinant));
  }
  const double beta_minus_one = std::exp(local[3]);
  const double beta = 1.0 + beta_minus_one;
  return beta_minus_one * std::pow(1.0 + m2, -beta) / (kPi * std::sqrt(determinant));
}

double max_metric_eigenvalue(PsfRequest::Family family, const std::vector<double>& local) {
  if (family == PsfRequest::Family::airy) return 0.0;
  const double sx = std::exp(local[0]);
  const double sy = std::exp(local[1]);
  const double rho = std::tanh(local[2]);
  const double cross = rho * sx * sy;
  const double determinant = sx * sx * sy * sy - cross * cross;
  if (!(determinant > 0.0)) return std::numeric_limits<double>::infinity();
  const double a = sy * sy / determinant;
  const double d = sx * sx / determinant;
  const double b = -cross / determinant;
  const double middle = 0.5 * (a + d);
  const double radius = std::sqrt(std::max(0.0, 0.25 * (a - d) * (a - d) + b * b));
  return middle + radius;
}

double tail_mass(PsfRequest::Family family, const std::vector<double>& local, double radius,
                 bool euclidean_radius) {
  if (radius <= 0.0) return 1.0;
  if (family == PsfRequest::Family::airy) {
    const double r0 = std::exp(local[0]);
    if (!(r0 > 0.0)) return 1.0;
    const double k = numerics::bessel_j1_first_zero() / r0;
    const double kr = k * radius;
    const double j0 = numerics::bessel_j0(kr);
    const double j1 = numerics::bessel_j1(kr);
    const double tail = j0 * j0 + j1 * j1;
    return tail < 0.0 ? 0.0 : (tail > 1.0 ? 1.0 : tail);
  }
  // The elliptical radius of the shape metric.
  const double metric = euclidean_radius ? radius * radius * max_metric_eigenvalue(family, local)
                                          : radius * radius;
  if (family == PsfRequest::Family::gaussian) {
    return std::exp(-0.5 * metric);
  }
  const double beta = 1.0 + std::exp(local[3]);
  return std::pow(1.0 + metric, 1.0 - beta);
}

Integral integrate_rectangle(PsfRequest::Family family, const std::vector<double>& local,
                              double px, double py, double x0, double x1, double y0, double y1,
                              double absolute_tolerance) {
  Integral out;
  if (!(x1 > x0) || !(y1 > y0)) {
    out.converged = true;
    return out;
  }
  const std::function<double(double, double)> f = [&](double x, double y) {
    return density(family, local, x - px, y - py);
  };
  if (family == PsfRequest::Family::gaussian && std::fabs(std::tanh(local[2])) < 1.0e-12) {
    // The separable case: the integral over the rectangle is the product of
    // the two error function differences; the normalisation cancels.
    const double sx = std::exp(local[0]);
    const double sy = std::exp(local[1]);
    const double ax = numerics::erf((x0 - px) / (std::sqrt(2.0) * sx));
    const double bx = numerics::erf((x1 - px) / (std::sqrt(2.0) * sx));
    const double ay = numerics::erf((y0 - py) / (std::sqrt(2.0) * sy));
    const double by = numerics::erf((y1 - py) / (std::sqrt(2.0) * sy));
    // The one dimensional integral of the Gaussian is sigma*sqrt(2*pi)/2 times
    // the difference of the error functions, so the normalisation leaves a
    // factor of one quarter.
    out.value = 0.25 * (bx - ax) * (by - ay);
    out.error = 8.0 * std::numeric_limits<double>::epsilon() *
                std::max(1.0, std::fabs(out.value));
    out.converged = true;
    out.panels = 1;
    return out;
  }
  if (family == PsfRequest::Family::gaussian) {
    // A correlated Gaussian is still integrable in one dimension: the
    // conditional distribution of y given x is Gaussian, so the integral over
    // the rectangle is
    //   int_{x0}^{x1} N_x(x) [Phi((y1 - mu(x))/s) - Phi((y0 - mu(x))/s)] dx
    // with the marginal N_x, the conditional mean mu(x) = py + rho sy dx / sx
    // and the conditional width s = sy sqrt(1 - rho^2).  This is exact up to
    // the one dimensional quadrature, and it is far cheaper than refining a
    // near singular two dimensional integrand; the two dimensional path stays
    // for the profiles that have no such reduction (Moffat, Airy).
    const double sx = std::exp(local[0]);
    const double sy = std::exp(local[1]);
    const double rho = std::tanh(local[2]);
    const double conditional = sy * std::sqrt(std::max(0.0, 1.0 - rho * rho));
    if (conditional > 0.0 && std::isfinite(conditional) && sx > 0.0) {
      const double tolerance = std::max(absolute_tolerance, 1.0e-14);
      const std::function<double(double)> integrand = [&](double x) {
        const double dx = x - px;
        const double marginal = std::exp(-0.5 * dx * dx / (sx * sx)) / (std::sqrt(2.0 * kPi) * sx);
        const double mean = py + rho * (sy / sx) * dx;
        const double upper = 0.5 * (1.0 + numerics::erf((y1 - mean) / (std::sqrt(2.0) * conditional)));
        const double lower = 0.5 * (1.0 + numerics::erf((y0 - mean) / (std::sqrt(2.0) * conditional)));
        return marginal * (upper - lower);
      };
      const numerics::QuadratureResult quadrature =
          numerics::adaptive_integrate(integrand, x0, x1, tolerance, 0.0, 200);
      Integral out;
      out.value = quadrature.value;
      out.error = std::isfinite(quadrature.absolute_error) ? quadrature.absolute_error
                                                           : std::numeric_limits<double>::infinity();
      out.converged = quadrature.converged;
      out.panels = quadrature.panels;
      return out;
    }
  }
  int panels = 0;
  const double tolerance = std::max(absolute_tolerance, 1.0e-14);
  Integral result = adaptive_block(f, x0, x1, y0, y1, tolerance, 0, 18, panels);
  return result;
}

Integral integrate_polygon(PsfRequest::Family family, const std::vector<double>& local,
                           double px, double py, const std::vector<std::vector<PixelEdge0>>& rings,
                           double absolute_tolerance) {
  Integral out;
  double x0 = std::numeric_limits<double>::infinity();
  double x1 = -std::numeric_limits<double>::infinity();
  double y0 = std::numeric_limits<double>::infinity();
  double y1 = -std::numeric_limits<double>::infinity();
  for (const std::vector<PixelEdge0>& ring : rings) {
    for (const PixelEdge0& vertex : ring) {
      x0 = std::min(x0, vertex.x);
      x1 = std::max(x1, vertex.x);
      y0 = std::min(y0, vertex.y);
      y1 = std::max(y1, vertex.y);
    }
  }
  if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1)) {
    out.converged = true;
    return out;
  }
  const std::function<double(double, double)> f = [&](double x, double y) {
    if (!inside_rings(rings, x, y)) return 0.0;
    return density(family, local, x - px, y - py);
  };
  int panels = 0;
  const double tolerance = std::max(absolute_tolerance, 1.0e-14);
  return adaptive_block(f, x0, x1, y0, y1, tolerance, 0, 18, panels);
}

SampleFootprint sample_footprint(const FootprintSet& footprints, U64 row) {
  SampleFootprint out;
  if (!footprints.index || row >= footprints.index->size) return out;
  if (footprints.width == 0 || footprints.height == 0) return out;
  const double sx = static_cast<double>(row % footprints.width);
  const double sy = static_cast<double>(row / footprints.width);
  const Mat2& m = footprints.detector_from_sample;
  // The four corners of the local cell are transformed by the same affine map
  // as the sample centre.
  const double cx[2] = {footprints.local_cell.lower[0], footprints.local_cell.upper[0]};
  const double cy[2] = {footprints.local_cell.lower[1], footprints.local_cell.upper[1]};
  double min_x = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      const double px = sx + cx[i];
      const double py = sy + cy[j];
      const double dx = m[0] * px + m[1] * py + footprints.offset[0];
      const double dy = m[2] * px + m[3] * py + footprints.offset[1];
      min_x = std::min(min_x, dx);
      max_x = std::max(max_x, dx);
      min_y = std::min(min_y, dy);
      max_y = std::max(max_y, dy);
    }
  }
  out.x0 = min_x;
  out.x1 = max_x;
  out.y0 = min_y;
  out.y1 = max_y;
  out.valid = true;
  return out;
}

double support_radius(PsfRequest::Family family,
                       const std::vector<std::vector<double>>& locals) {
  double width = 0.0;
  for (const std::vector<double>& local : locals) {
    if (local.empty()) continue;
    if (family == PsfRequest::Family::airy) {
      width = std::max(width, std::exp(local[0]));
    } else {
      width = std::max(width, std::max(std::exp(local[0]), std::exp(local[1])));
    }
  }
  return 10.0 * width;
}

std::vector<U64> candidate_rows(const FootprintSet& footprints,
                                const std::vector<PixelCenter0>& positions, double radius) {
  std::vector<U64> rows;
  if (positions.empty() || !footprints.index) return rows;
  const U64 count = footprints.index->size;
  for (U64 row = 0; row < count; ++row) {
    const SampleFootprint footprint = sample_footprint(footprints, row);
    if (!footprint.valid) continue;
    bool close = false;
    for (const PixelCenter0& position : positions) {
      const double dx = std::max({footprint.x0 - position.x, position.x - footprint.x1, 0.0});
      const double dy = std::max({footprint.y0 - position.y, position.y - footprint.y1, 0.0});
      if (dx * dx + dy * dy <= radius * radius) {
        close = true;
        break;
      }
    }
    if (close) rows.push_back(row);
  }
  return rows;
}

Result<TemplateBlock> build_templates_from_local(Context& context, PsfRequest::Family family,
                                                  const std::vector<std::vector<double>>& locals,
                                                  const std::vector<PixelCenter0>& positions,
                                                  const FootprintSet& footprints,
                                                  const std::vector<U64>& rows,
                                                  double absolute_tolerance) {
  TemplateBlock block;
  block.rows = rows;
  const std::size_t sources = positions.size();
  const std::size_t samples = rows.size();
  block.values.assign(samples * sources, 0.0);
  block.errors.assign(samples * sources, 0.0);
  block.tails.assign(sources, 0.0);
  if (sources == 0 || samples == 0) {
    return model::ok_result(std::move(block));
  }
  Result<U64> work = context.resources.consume_work(samples * sources);
  if (!ok(work)) {
    return model::fail<TemplateBlock>(work.code, "the template exceeds the work budget",
                                       "sources.psf", "the template cannot be built");
  }
  for (std::size_t s = 0; s < sources; ++s) {
    // The cancellation is checked for every source and for every block of
    // samples: a cancelled build returns no partial template.
    if (context.cancellation.requested()) {
      return model::fail<TemplateBlock>(Code::cancelled, "cancelled", "sources.psf",
                                        "the template cannot be built");
    }
    const std::vector<double>& local = locals[s];
    // The parameters have to describe a density.  An invalid parameter is a
    // named domain failure, never a template of zeros.
    std::string reason;
    if (!local_parameters_valid(family, local, reason)) {
      return model::fail<TemplateBlock>(Code::domain, reason, "sources.psf",
                                        "the model is not a density and the template cannot be "
                                        "built");
    }
    double mass = 0.0;
    for (std::size_t k = 0; k < samples; ++k) {
      if ((k & 0x3F) == 0 && context.cancellation.requested()) {
        return model::fail<TemplateBlock>(Code::cancelled, "cancelled", "sources.psf",
                                          "the template cannot be built");
      }
      const SampleFootprint footprint = sample_footprint(footprints, rows[k]);
      if (!footprint.valid) continue;
      Integral integral;
      if (footprint.rings.empty()) {
        integral = integrate_rectangle(family, local, positions[s].x, positions[s].y, footprint.x0,
                                       footprint.x1, footprint.y0, footprint.y1, absolute_tolerance);
      } else {
        integral = integrate_polygon(family, local, positions[s].x, positions[s].y, footprint.rings,
                                     absolute_tolerance);
      }
      block.values[k * sources + s] = integral.value;
      block.errors[k * sources + s] = integral.error;
      mass += integral.value;
    }
    // The native positive model is normalised over the whole plane, so the
    // mass outside the integrated region is one minus the integrated mass.
    const double tail = 1.0 - mass;
    block.tails[s] = tail > 0.0 ? tail : 0.0;
  }
  return model::ok_result(std::move(block));
}

Result<TemplateBlock> build_templates(Context& context, PsfRequest::Family family,
                                       const std::vector<double>& coefficients, const Basis& basis,
                                       const std::vector<PixelCenter0>& positions,
                                       const FootprintSet& footprints,
                                       const std::vector<U64>& rows, double absolute_tolerance) {
  std::vector<std::vector<double>> locals;
  locals.reserve(positions.size());
  for (const PixelCenter0& position : positions) {
    const std::vector<double> basis_vector = basis_values(basis, basis.degree, position.x, position.y);
    locals.push_back(local_parameters(family, coefficients, basis_vector));
  }
  return build_templates_from_local(context, family, locals, positions, footprints, rows,
                                    absolute_tolerance);
}

std::vector<double> solve_scaled(const numerics::SvdResult& svd, const std::vector<double>& rhs,
                                  const std::vector<double>& column_scale) {
  const std::size_t count = svd.singular_values.size();
  std::vector<double> projected(count, 0.0);
  for (std::size_t i = 0; i < count; ++i) {
    const double sigma = svd.singular_values[i];
    if (!(sigma > 0.0)) continue;
    double sum = 0.0;
    for (std::size_t k = 0; k < svd.u.rows && k < rhs.size(); ++k) sum += svd.u(k, i) * rhs[k];
    projected[i] = sum / sigma;
  }
  std::vector<double> solution(svd.v.cols, 0.0);
  for (std::size_t c = 0; c < svd.v.cols; ++c) {
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) sum += svd.v(c, i) * projected[i];
    solution[c] = sum;
  }
  for (std::size_t c = 0; c < solution.size() && c < column_scale.size(); ++c) {
    solution[c] = column_scale[c] > 0.0 ? solution[c] / column_scale[c] : 0.0;
  }
  return solution;
}

std::vector<double> solve_damped(const numerics::SvdResult& svd, const std::vector<double>& rhs,
                                  const std::vector<double>& column_scale, double damping) {
  const std::size_t count = svd.singular_values.size();
  std::vector<double> projected(count, 0.0);
  for (std::size_t i = 0; i < count; ++i) {
    const double sigma = svd.singular_values[i];
    const double factor = 1.0 / (sigma * sigma + damping);
    double sum = 0.0;
    for (std::size_t k = 0; k < svd.u.rows && k < rhs.size(); ++k) sum += svd.u(k, i) * rhs[k];
    projected[i] = sum * factor;
  }
  std::vector<double> solution(svd.v.cols, 0.0);
  for (std::size_t c = 0; c < svd.v.cols; ++c) {
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) sum += svd.v(c, i) * projected[i];
    solution[c] = sum;
  }
  for (std::size_t c = 0; c < solution.size() && c < column_scale.size(); ++c) {
    solution[c] = column_scale[c] > 0.0 ? solution[c] / column_scale[c] : 0.0;
  }
  return solution;
}

Result<PixelCenter0> detector_position(Context& context, Ref<WcsModel> wcs,
                                        const CatalogueStar& star,
                                        const AstrometricContext& context_epoch,
                                        const NumericBudget& budget) {
  // The catalogue module owns the propagation to the observation epoch (the
  // pmRA*cosDec convention, the parallax and the radial velocity).  The
  // sources module never propagates a second time: the position of the star
  // is used as the catalogue provider delivered it.
  const SkyICRS sky{std::cos(star.dec_rad) * std::cos(star.ra_rad),
                    std::cos(star.dec_rad) * std::sin(star.ra_rad), std::sin(star.dec_rad)};
  Result<InverseResult> inverted = inverse(context, wcs, sky, budget);
  if (!ok(inverted)) {
    return model::fail<PixelCenter0>(inverted.code,
                                      "the catalogue position cannot be inverted on the frame",
                                      "sources.psf", "the source has no detector position");
  }
  return model::ok_result(val(inverted).point);
}

double weighted_mean(const std::vector<double>& values, const std::vector<double>& weights) {
  double sum = 0.0;
  double total = 0.0;
  for (std::size_t i = 0; i < values.size() && i < weights.size(); ++i) {
    sum += weights[i] * values[i];
    total += weights[i];
  }
  return total > 0.0 ? sum / total : 0.0;
}

}  // namespace acsd::sources::psfmodel
