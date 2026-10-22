// The quad matching of the blind astrometric solver.
//
// The descriptor of an observed quadruple is the same written definition as the
// one of the catalogue index (the longest baseline, the projection of the two
// remaining points, the enumeration over the A/B swap, the C/D swap and the
// mirror).  It is implemented here independently of the catalogue module,
// which is only reached through the declared provider interface; the
// validation compares the two implementations on the same configurations.
//
// The correspondence between a catalogue quadruple and an observed quadruple
// is established by fitting an affine map from the catalogue tangent plane to
// the detector; all 24 correspondences are tried and the smallest residual
// wins.  The affine map absorbs the mirror, so the handness of the solution is
// decided by the sign of its determinant.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include "acsd/contracts.hpp"
#include "astrometry_common.hpp"
#include "numerics/linalg.hpp"

namespace acsd::sources {
namespace {

// The lexicographic comparison of two four vectors.
bool less4(const std::array<double, 4>& left, const std::array<double, 4>& right) {
  for (std::size_t i = 0; i < 4; ++i) {
    if (left[i] < right[i]) return true;
    if (left[i] > right[i]) return false;
  }
  return false;
}

}  // namespace

std::array<double, 4> observed_descriptor(const std::array<std::array<double, 2>, 4>& points) {
  // The longest baseline; the tie is broken by the point index.
  int best_a = 0;
  int best_b = 1;
  double best_length = -1.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const double dx = points[j][0] - points[i][0];
      const double dy = points[j][1] - points[i][1];
      const double length = std::sqrt(dx * dx + dy * dy);
      if (length > best_length) {
        best_length = length;
        best_a = i;
        best_b = j;
      }
    }
  }
  if (!(best_length > 0.0)) return std::array<double, 4>{0.0, 0.0, 0.0, 0.0};

  int remaining[2] = {0, 0};
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == best_a || i == best_b) continue;
    remaining[count++] = i;
  }
  if (remaining[0] > remaining[1]) std::swap(remaining[0], remaining[1]);
  const int c = remaining[0];
  const int d = remaining[1];

  const double ex = (points[best_b][0] - points[best_a][0]) / best_length;
  const double ey = (points[best_b][1] - points[best_a][1]) / best_length;
  const double px = -ey;
  const double py = ex;
  const auto project = [&](int index, double& x, double& y) {
    const double dx = points[index][0] - points[best_a][0];
    const double dy = points[index][1] - points[best_a][1];
    x = (dx * ex + dy * ey) / best_length;
    y = (dx * px + dy * py) / best_length;
  };
  double cx = 0.0;
  double cy = 0.0;
  double dx_c = 0.0;
  double dy_c = 0.0;
  project(c, cx, cy);
  project(d, dx_c, dy_c);

  const std::array<double, 4> base{cx, cy, dx_c, dy_c};
  const std::array<double, 4> variants[4] = {
      base,
      std::array<double, 4>{base[2], base[3], base[0], base[1]},
      std::array<double, 4>{1.0 - base[0], -base[1], 1.0 - base[2], -base[3]},
      std::array<double, 4>{1.0 - base[2], -base[3], 1.0 - base[0], -base[1]}};
  const std::array<double, 4> mirrored{cx, -cy, dx_c, -dy_c};
  const std::array<double, 4> mirrored_variants[4] = {
      mirrored,
      std::array<double, 4>{mirrored[2], mirrored[3], mirrored[0], mirrored[1]},
      std::array<double, 4>{1.0 - mirrored[0], -mirrored[1], 1.0 - mirrored[2], -mirrored[3]},
      std::array<double, 4>{1.0 - mirrored[2], -mirrored[3], 1.0 - mirrored[0], -mirrored[1]}};

  std::array<double, 4> best = variants[0];
  for (const auto& variant : variants) {
    if (less4(variant, best)) best = variant;
  }
  for (const auto& variant : mirrored_variants) {
    if (less4(variant, best)) best = variant;
  }
  return best;
}

double affine_determinant(const double matrix[6]) {
  return matrix[1] * matrix[5] - matrix[2] * matrix[4];
}

AffineHypothesis fit_affine(const std::array<Vec3, 4>& catalogue_sky, const Vec3& centre,
                             const std::array<std::array<double, 2>, 4>& observed) {
  AffineHypothesis hypothesis;

  // The catalogue points in the tangent plane of the quadruple centre.
  double ra = std::atan2(centre[1], centre[0]);
  double dec = std::asin(std::clamp(centre[2], -1.0, 1.0));
  if (ra < 0.0) ra += 2.0 * 3.14159265358979323846;
  const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
  const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
  std::array<std::array<double, 2>, 4> tangent{};
  for (std::size_t k = 0; k < 4; ++k) {
    const double dc = centre[0] * catalogue_sky[k][0] + centre[1] * catalogue_sky[k][1] +
                      centre[2] * catalogue_sky[k][2];
    if (!(std::abs(dc) > 1e-12)) return hypothesis;
    tangent[k][0] = (east[0] * catalogue_sky[k][0] + east[1] * catalogue_sky[k][1] +
                     east[2] * catalogue_sky[k][2]) / dc;
    tangent[k][1] = (north[0] * catalogue_sky[k][0] + north[1] * catalogue_sky[k][1] +
                     north[2] * catalogue_sky[k][2]) / dc;
  }

  // All 24 correspondences between the catalogue roles and the observed
  // points.
  int permutation[4] = {0, 1, 2, 3};
  do {
    numerics::Matrix design(4, 3);
    std::vector<double> target_x(4);
    std::vector<double> target_y(4);
    for (std::size_t k = 0; k < 4; ++k) {
      design(k, 0) = tangent[k][0];
      design(k, 1) = tangent[k][1];
      design(k, 2) = 1.0;
      target_x[k] = observed[permutation[k]][0];
      target_y[k] = observed[permutation[k]][1];
    }
    // The two right hand sides are solved separately: the multiple right
    // hand side entry point of the numerics does not return the solution.
    const numerics::LeastSquaresResult solution_x = numerics::least_squares(design, target_x, 1e-12);
    const numerics::LeastSquaresResult solution_y = numerics::least_squares(design, target_y, 1e-12);
    if (!solution_x.converged || !solution_y.converged) continue;
    if (solution_x.solution.size() != 3 || solution_y.solution.size() != 3) continue;
    double residual = 0.0;
    for (std::size_t k = 0; k < 4; ++k) {
      const double px = solution_x.solution[0] * tangent[k][0] +
                        solution_x.solution[1] * tangent[k][1] + solution_x.solution[2];
      const double py = solution_y.solution[0] * tangent[k][0] +
                        solution_y.solution[1] * tangent[k][1] + solution_y.solution[2];
      const double dx = px - target_x[k];
      const double dy = py - target_y[k];
      residual += dx * dx + dy * dy;
    }
    residual = std::sqrt(residual / 4.0);
    if (!hypothesis.valid || residual < hypothesis.residual_pixel) {
      hypothesis.valid = true;
      hypothesis.residual_pixel = residual;
      // The affine is stored as px = m0 + m1 xi + m2 eta and
      // py = m3 + m4 xi + m5 eta; the solution of the design matrix
      // [xi, eta, 1] carries the coefficients in the order
      // [xi, eta, constant].
      hypothesis.matrix[0] = solution_x.solution[2];
      hypothesis.matrix[1] = solution_x.solution[0];
      hypothesis.matrix[2] = solution_x.solution[1];
      hypothesis.matrix[3] = solution_y.solution[2];
      hypothesis.matrix[4] = solution_y.solution[0];
      hypothesis.matrix[5] = solution_y.solution[1];
      for (std::size_t k = 0; k < 4; ++k) hypothesis.assignment[k] = permutation[k];
    }
  } while (std::next_permutation(permutation, permutation + 4));

  if (!hypothesis.valid) return hypothesis;
  hypothesis.centre = centre;
  const double determinant = std::abs(affine_determinant(hypothesis.matrix));
  hypothesis.scale_rad_pixel = std::sqrt(determinant);
  return hypothesis;
}

}  // namespace acsd::sources
