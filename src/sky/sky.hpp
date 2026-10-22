#pragma once
// Internal helpers of the sky module.
//
// The module exchanges data with the other science modules only through the
// public contract, so the helpers below are private to src/sky.  They provide
// the gnomonic chart and the basis evaluation used by both the control model
// A and the output model E, the minimal HEALPix face geometry needed to place
// the leaf positions, and the sparse assembly of the linear actions.
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"

namespace acsd::sky {

// ---------------------------------------------------------------------------
// Spherical chart
// ---------------------------------------------------------------------------

struct ChartProjection {
  SkyICRS center;
  Vec3 east{1.0, 0.0, 0.0};
  Vec3 north{0.0, 1.0, 0.0};
  double maximum_radius_rad = 0.0;
};

bool make_chart_projection(const SkyICRS& center, double maximum_radius_rad, ChartProjection& out);
bool project_to_chart(const ChartProjection& projection, const SkyICRS& direction, Vec2& out);
bool unproject_from_chart(const ChartProjection& projection, const Vec2& point, SkyICRS& out);

// ---------------------------------------------------------------------------
// Basis
// ---------------------------------------------------------------------------

// Number of terms of a basis: the constant term plus the two dimensional
// polynomial terms up to the requested degree.
std::size_t basis_term_count(const Basis& basis);

// Evaluates the basis at a sky position.  The constant term is never reduced;
// the spatial terms are centred by the training weighted mean so that they do
// not mix with the constant.  The same basis object is used for the control
// model A and for the output model E.
std::vector<double> evaluate_basis(const Basis& basis, const ChartProjection& projection,
                                    const SkyICRS& position);

// ---------------------------------------------------------------------------
// Minimal HEALPix NESTED face geometry
//
// The normative module table does not allow src/sky to depend on
// src/coordinates, so the few HEALPix facts needed to place the leaf centres
// are implemented locally from the public formulas of the scientific
// foundation (SPHERICAL_DRIZZLE section 2).
// ---------------------------------------------------------------------------

SkyICRS local_leaf_center(U32 order, U64 ipix);
SkyICRS local_face_point(U32 face, double s, double t);
void local_leaf_face_xy(U32 order, U64 ipix, U32& face, U64& ix, U64& iy);
double local_leaf_radius_rad(U32 order);

// ---------------------------------------------------------------------------
// Linear action helpers
// ---------------------------------------------------------------------------

Result<std::vector<double>> apply_action_full(Context& context, Ref<LinearAction> action,
                                              const std::vector<double>& values);
U64 action_domain_size(Ref<LinearAction> action);
U64 action_codomain_size(Ref<LinearAction> action);


// Materialises an action as a dense row-major matrix.
Result<numerics::Matrix> dense_action(Context& context, Ref<LinearAction> action, U64 rows,
                                       U64 cols);

// Converts an action into a sparse matrix; dense materialisation is only used
// when the action has no sparse payload.
Result<operators::CsrMatrix> to_csr(Context& context, Ref<LinearAction> action, U64 rows, U64 cols);

// Builds a sparse action from a dense matrix.
Result<Ref<LinearAction>> dense_to_action(Context& context, const numerics::Matrix& matrix,
                                           const Id& domain_index, const Id& codomain_index,
                                           const Unit& unit);

// Builds the dense factor G of a covariance with C = G G^T.
Result<numerics::Matrix> dense_covariance_factor(Context& context, Ref<CovarianceAction> covariance);

// Builds the block diagonal assembly of several sparse matrices that share the
// same column space.
operators::CsrMatrix block_diagonal(const std::vector<operators::CsrMatrix>& blocks,
                                    const std::vector<U64>& row_offsets);

// Shifts the rows of a sparse matrix by an offset.
operators::CsrMatrix shift_rows(const operators::CsrMatrix& matrix, U64 offset);

}  // namespace acsd::sky
