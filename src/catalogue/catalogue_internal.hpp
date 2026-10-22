#pragma once
// Internal helpers of the offline Gaia catalogue module.
//
// The public surface of this module is the three declared entry points
// (build_catalogue_index, make_catalogue_provider) plus the CatalogueProvider
// implementation.  Everything below is private to src/catalogue and is never
// included by other modules: cross module exchange happens exclusively through
// include/acsd/contracts.hpp.
//
// Science references (docs/science/foundation + docs/engineering/rebuild):
//   * OBSERVATION_COORDINATES.md §2  - ICRS positions are propagated with the
//     proper motion, the parallax and the observation time; pmRA*cosDec
//     convention; known RV adds the perspective acceleration; missing
//     information is recorded as an ignored item with a position budget.
//   * SYSTEM.md "盲解 catalogue/XP 公开能力与索引" - the order 6 sky sharding,
//     the 64 brightest stars per shard, the 12 nearest neighbours per anchor,
//     the 16 quads per anchor, the baseline filter, the source tuple
//     de-duplication, the two mirror canonical descriptors and the KD tree.
//   * OBJECTS.md - the array / index identity rules used for the exported
//     index arrays.
#include <array>
#include <cstdint>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::catalogue {

// ---------------------------------------------------------------------------
// Catalogue rows
// ---------------------------------------------------------------------------

// One parsed row of the catalogue columns.
struct StarRow {
  U64 source_id = 0;
  double ra_rad = 0.0;
  double dec_rad = 0.0;
  double epoch_year = 0.0;
  double pmra_cosdec_rad_year = 0.0;
  double pmdec_rad_year = 0.0;
  double parallax_rad = 0.0;
  double rv_m_s = 0.0;
  bool rv_present = false;
  double brightness_mag = 0.0;
  Vec3 unit{};  // ICRS unit vector at the catalogue epoch
};

// A propagated catalogue position.
struct PropagatedPosition {
  Vec3 unit{};              // ICRS unit vector at the requested epoch
  double ra_rad = 0.0;
  double dec_rad = 0.0;
  double parallax_rad = 0.0;  // parallax at the requested epoch (rad)
};

// The observer description used for the topocentric correction.
struct ObserverState {
  bool barycentric_available = false;
  Vec3 barycentric_au{};      // observer barycentric position (au)
  bool geodetic_available = false;
  Vec3 geodetic{};            // (longitude, latitude, height) rad, rad, m
};

// Propagates one catalogue row to `target_epoch_year` (Julian year).
//
// The propagation follows the linear space motion r(t) = r0 + v dt with the
// proper motion carried in the tangent plane (pmRA*cosDec convention) and the
// radial velocity along the line of sight.  When the parallax is unknown the
// distance is infinite and the motion degenerates into the exact great circle
// rotation, which is the limit of the same expression.
PropagatedPosition propagate_star(const StarRow& star, double target_epoch_year);

// Applies the topocentric correction.  Returns false when the observer is not
// known; the caller must then record the ignored item and the position budget.
bool topocentric_direction(const PropagatedPosition& propagated, const ObserverState& observer,
                           Vec3& out_unit, double& parallax_rad);

// Upper bound (rad) of the topocentric direction error caused by an unknown
// observer position: the parallax of the star is the relevant length scale.
double observer_omission_bound_rad(const StarRow& star);

// ---------------------------------------------------------------------------
// Sky geometry helpers (gnomonic tangent plane)
// ---------------------------------------------------------------------------

Vec3 unit_from_ra_dec(double ra_rad, double dec_rad);
void ra_dec_from_unit(const Vec3& unit, double& ra_rad, double& dec_rad);
double angular_separation(const Vec3& a, const Vec3& b);

// Local east/north basis of a direction.
void local_basis(const Vec3& centre, Vec3& east, Vec3& north);

// Exact gnomonic projection of `target` onto the tangent plane at `centre`.
Vec2 tangent_project(const Vec3& centre, const Vec3& east, const Vec3& north, const Vec3& target);

// Inverse gnomonic projection.
Vec3 tangent_unproject(const Vec3& centre, const Vec3& east, const Vec3& north, const Vec2& xy);

// ---------------------------------------------------------------------------
// Quad descriptors
// ---------------------------------------------------------------------------

// The similarity invariant descriptor of a four point configuration.
//
// The longest baseline endpoints A and B define the frame (the length tie is
// broken by the source identifier).  The two remaining points C and D are
// projected on that frame, giving the four numbers
//   ((C-A).e/|AB|, (C-A).e_perp/|AB|, (D-A).e/|AB|, (D-A).e_perp/|AB|)
// with e the unit baseline direction and e_perp its counter clockwise
// perpendicular.  The enumeration over the A/B swap, the C/D swap and the
// mirror yields the lexicographically smallest four vector.
//
// The descriptor is stored for both chirality conventions (counter clockwise
// and clockwise e_perp); the lexicographic minimum of the two is the mirror
// invariant canonical descriptor.
struct QuadDescriptor {
  std::array<double, 4> canonical{};       // mirror invariant minimum
  std::array<double, 4> chirality_plus{};  // e_perp counter clockwise
  std::array<double, 4> chirality_minus{}; // e_perp clockwise
  std::array<int, 4> roles_plus{};         // role -> index of the input point
  std::array<int, 4> roles_minus{};
  double baseline = 0.0;  // length of the longest baseline (same unit as the input)
  Vec3 centre{};          // centroid direction (unit vector)
};

// Computes the descriptor of four points given in a two dimensional tangent
// plane.  `ids` only serves the deterministic tie break of the baseline.
QuadDescriptor describe_quad(const std::array<Vec2, 4>& points, const std::array<U64, 4>& ids);

// The mirror invariant canonical descriptor of four points given in a tangent
// plane; used by the blind solver to compare an observed configuration with
// the catalogue.
std::array<double, 4> canonical_descriptor(const std::array<Vec2, 4>& points,
                                           const std::array<U64, 4>& ids);

// ---------------------------------------------------------------------------
// Column parsing
// ---------------------------------------------------------------------------

// Parses the catalogue columns into rows.  The arrays are read through the
// public storage interface; the returned failure carries the storage code.
Result<std::vector<StarRow>> parse_columns(Context& context, const CatalogueColumns& columns,
                                            std::string& reason);

// The number of spectral coefficients of every source.
U64 spectral_coefficient_count(const CatalogueColumns& columns);

// ---------------------------------------------------------------------------
// Shared constants
// ---------------------------------------------------------------------------
// The algorithm identity of the offline index.  It is stored in
// CatalogueMetadata.index_algorithm.
inline constexpr const char* kCatalogueIndexAlgorithm = "acsd.catalogue.quad_index.v1";

// Maximum page size accepted by the catalogue provider.
inline constexpr U32 kMaxPageSize = 4096;

}  // namespace acsd::catalogue
