#pragma once
// HEALPix NESTED geometry implemented directly from the scientific
// specification (equal area face charts, nested numbering).  The production
// index is built from these formulas; no external HEALPix library is used.
#include <array>
#include <cstdint>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::coordinates {

// Maximum supported order (HEALPix uses 0..29).
constexpr U32 kMaxOrder = 29;

// Number of pixels on one face for the given order.
constexpr U64 nside_for(U32 order) { return 1ull << order; }

// Total number of leaves for the given order.
constexpr U64 npix_for(U32 order) { return 12ull * (1ull << (2 * order)); }

// True when the leaf identity is inside the supported range.
bool healpix_valid(U32 order, U64 ipix);

// Position of a leaf inside its face, in [0, nside).
std::array<U64, 2> healpix_face_xy(U32 order, U64 ipix);

// Inverse of healpix_face_xy: the nested index inside the face.
U64 healpix_face_index(U32 order, U64 ix, U64 iy);

// Face number of a leaf.
U32 healpix_face(U32 order, U64 ipix);

// Unit vector at the centre of a leaf (ICRS).
SkyICRS healpix_center(U32 order, U64 ipix);

// Unit vector at the continuous face coordinates (s, t) in [0, 1].
SkyICRS healpix_face_point(U32 face, double s, double t);

// Solid angle of one leaf, in steradian.
double healpix_leaf_area(U32 order);

// Solid angle of one face, in steradian.
double healpix_face_area();

// Every leaf of the given order, in nested index order.
std::vector<HealpixLeaf> healpix_all_leaves(U32 order);

// Leaf containing the given direction, or false when the direction is not a
// unit vector.
bool healpix_lookup(U32 order, const SkyICRS& direction, HealpixLeaf& leaf);

// The nested index range of the leaves that may contain a direction.
bool healpix_candidate_leaves(U32 order, const SkyICRS& direction, std::vector<HealpixLeaf>& leaves);

}  // namespace acsd::coordinates
