#pragma once
// Spatial basis evaluation for the joint photometric fit.
//
// docs/engineering/rebuild/OBJECTS.md, `Basis`:
//   * the terms are ordered by total degree and, inside one degree, by
//     decreasing x power: 1, x, y, x^2, xy, y^2, ...
//   * u = (x - center.x) / scale.x and v = (y - center.y) / scale.y with
//     scale > 0;
//   * `weighted_mean` holds the training weighted mean of the spatial terms;
//     the constant term is never reduced.
//
// The reduced spatial terms are mean zero over the training set, so the
// constant term (the logarithm of the frame scale) and the spatial field are
// separated (GAIA_RESPONSE.md §3).
#include <cstddef>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::photometry {

// Number of terms of the requested basis (1, 3 or 6).
std::size_t basis_term_count(const Basis& basis);

// Evaluates the (reduced) basis terms at a detector position.  `weighted_mean`
// holds one entry per basis term; the constant term is ignored.
void basis_terms(const Basis& basis, const std::vector<double>& weighted_mean, double x, double y,
                 std::vector<double>& out);

}  // namespace acsd::photometry
