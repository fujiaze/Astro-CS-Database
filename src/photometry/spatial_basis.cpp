#include "photometry/spatial_basis.hpp"

#include <cmath>

namespace acsd::photometry {

std::size_t basis_term_count(const Basis& basis) {
  switch (basis.kind) {
    case Basis::Kind::constant:
      return 1;
    case Basis::Kind::plane:
      return 3;
    case Basis::Kind::polynomial:
      return basis.degree >= 2 ? 6 : (basis.degree == 1 ? 3 : 1);
  }
  return 1;
}

void basis_terms(const Basis& basis, const std::vector<double>& weighted_mean, double x, double y,
                 std::vector<double>& out) {
  const std::size_t terms = basis_term_count(basis);
  out.assign(terms, 0.0);
  if (terms == 0) {
    return;
  }
  const double scale_x = basis.scale[0];
  const double scale_y = basis.scale[1];
  const double u = (scale_x != 0.0) ? (x - basis.center[0]) / scale_x : 0.0;
  const double v = (scale_y != 0.0) ? (y - basis.center[1]) / scale_y : 0.0;
  out[0] = 1.0;
  if (terms >= 3) {
    out[1] = u;
    out[2] = v;
  }
  if (terms >= 6) {
    out[3] = u * u;
    out[4] = u * v;
    out[5] = v * v;
  }
  // The constant term is never reduced; the spatial terms are shifted by their
  // training weighted mean.
  for (std::size_t term = 1; term < terms; ++term) {
    double mean = 0.0;
    if (weighted_mean.size() == terms) {
      mean = weighted_mean[term];
    } else if (weighted_mean.size() + 1 == terms) {
      mean = weighted_mean[term - 1];
    }
    out[term] -= mean;
  }
}

}  // namespace acsd::photometry
