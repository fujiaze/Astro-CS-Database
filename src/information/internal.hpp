#pragma once
// Internal helpers of the information module.
//
// The module may only exchange data through the public contract, so these
// helpers are private to src/information and are not part of any public
// surface.  They implement the dense linear algebra that the statistical
// contract requires (one propagation of the same random variable, an exact
// null space of a PSD covariance, a QR/SVD based projection) and the local
// spherical chart used by the sparse actual-scene reconstruction.
#include <cstddef>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"

namespace acsd::information {

// ---------------------------------------------------------------------------
// Linear action helpers
// ---------------------------------------------------------------------------

// Applies a linear action and returns the *complete* output.  The public
// uncertainty helper truncates the result to the input length, which is wrong
// for a rectangular action such as the control operator T.
Result<std::vector<double>> apply_action_full(Context& context, Ref<LinearAction> action,
                                              const std::vector<double>& values);

// Number of columns (domain) and rows (codomain) of an action.
U64 action_domain_size(Ref<LinearAction> action);
U64 action_codomain_size(Ref<LinearAction> action);

// Materialises a linear action as a dense row-major matrix.  `rows` and `cols`
// must agree with the action spaces; passing zero asks the helper to derive
// the shape from the action itself.
Result<numerics::Matrix> dense_action(Context& context, Ref<LinearAction> action, U64 rows,
                                       U64 cols);

// ---------------------------------------------------------------------------
// Covariance factor
// ---------------------------------------------------------------------------

// Builds the dense factor G with C = G G^T from the noise model of a
// covariance action.  The factor is the concatenation of the per root maps
// M_k = L_k F_k, so the null space of C is the orthogonal complement of the
// column space of G and is derived from the model structure, never from an
// arbitrary eigenvalue floor of the applied covariance.
Result<numerics::Matrix> dense_covariance_factor(Context& context, Ref<CovarianceAction> covariance);

// True when the noise model carries an unknown term with a non zero influence.
bool covariance_has_unknown_term(Ref<CovarianceAction> covariance);

// ---------------------------------------------------------------------------
// Orthogonal factorisation
// ---------------------------------------------------------------------------

// Complete orthogonal factorisation of the column space of `v` (N x m, m <= N).
// The returned matrix H is N x N orthogonal; its first m columns span the
// column space of v and the remaining columns span the orthogonal complement.
numerics::Matrix complete_orthogonal_factorisation(const numerics::Matrix& v);

struct RangeSplit {
  numerics::Matrix range_basis;    // N x r, orthonormal, spans range(G)
  numerics::Matrix null_basis;     // N x (N-r), orthonormal, spans the exact null space
  std::vector<double> singular_values;
  double rank_relative = 1e-12;
  bool rank_certified = false;
  std::size_t rank = 0;
};


// Splits the space into the range of the covariance factor and its exact null
// space.  The rank is classified with a scale aware threshold; when the
// singular values around the threshold are not separated the split is
// reported as uncertified instead of being silently accepted.
Result<RangeSplit> split_covariance_range(const numerics::Matrix& factor, double rank_relative);

// ---------------------------------------------------------------------------
// Spherical chart
// ---------------------------------------------------------------------------

// Gnomonic (tangent plane) chart around a chart centre.
struct ChartProjection {
  SkyICRS center;
  Vec3 east;    // unit vector, local x direction
  Vec3 north;   // unit vector, local y direction
  double maximum_radius_rad = 0.0;

  bool valid() const;
};

// Builds the tangent basis of a chart centre.  Returns false when the centre
// is not a finite unit vector.
bool make_chart_projection(const Chart& chart, ChartProjection& out);

// Projects a direction into chart coordinates.  Returns false when the
// direction is behind the tangent plane or outside the declared radius.
bool project_to_chart(const ChartProjection& projection, const SkyICRS& direction, Vec2& out);

// Inverse projection: chart coordinates back to a unit direction.
bool unproject_from_chart(const ChartProjection& projection, const Vec2& point, SkyICRS& out);

// Angular distance between two directions in radians.
double angular_distance(const SkyICRS& a, const SkyICRS& b);

// ---------------------------------------------------------------------------
// Local HEALPix NESTED face geometry
//
// The normative module table does not allow src/information to depend on
// src/coordinates, so the few HEALPix facts that the support containment test
// needs are implemented locally from the public formulas of the scientific
// foundation (SPHERICAL_DRIZZLE section 2).  They are used only to decide
// whether a chart triangle lies inside the observed support.
// ---------------------------------------------------------------------------

// Leaf containing a direction at the given order; false when the direction is
// not a finite unit vector.
bool local_leaf_lookup(U32 order, const SkyICRS& direction, U64& ipix);

// Centre direction of a leaf at the given order.
SkyICRS local_leaf_center(U32 order, U64 ipix);

// Direction of a point of an equal area face chart; s and t are in [0,1].
SkyICRS local_face_point(U32 face, double s, double t);

// Angular radius of a leaf, derived from its solid angle.
double local_leaf_radius_rad(U32 order);

}  // namespace acsd::information
