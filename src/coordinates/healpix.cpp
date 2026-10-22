#include "coordinates/healpix.hpp"

#include <algorithm>
#include <cmath>

namespace acsd::coordinates {
namespace {

// Equal area face chart parameters from the scientific specification.
struct FaceParam {
  int j;   // polar or equatorial type: 2 equatorial, 3 north, 4 south
  int l;   // face longitude offset index
};

constexpr FaceParam kFaces[12] = {{2, 1}, {2, 3}, {2, 5}, {2, 7},
                                  {3, 0}, {3, 2}, {3, 4}, {3, 6},
                                  {4, 1}, {4, 3}, {4, 5}, {4, 7}};

SkyICRS face_point(const FaceParam& face, double s, double t) {
  const double r = static_cast<double>(face.j) - s - t;
  const double d = s - t;
  double z = 0.0;
  double phi = 0.0;
  if (r < 1.0) {
    z = 1.0 - r * r / 3.0;
    phi = (M_PI / 4.0) * (face.l + d / r);
  } else if (r <= 3.0) {
    z = (2.0 - r) * 2.0 / 3.0;
    phi = (M_PI / 4.0) * (face.l + d);
  } else {
    z = -1.0 + (4.0 - r) * (4.0 - r) / 3.0;
    phi = (M_PI / 4.0) * (face.l + d / (4.0 - r));
  }
  const double sin_theta = std::sqrt(std::max(0.0, 1.0 - z * z));
  SkyICRS out;
  out.nx = sin_theta * std::cos(phi);
  out.ny = sin_theta * std::sin(phi);
  out.nz = z;
  return out;
}

}  // namespace

bool healpix_valid(U32 order, U64 ipix) {
  return order <= kMaxOrder && ipix < npix_for(order);
}

U32 healpix_face(U32 order, U64 ipix) {
  const U64 nside = nside_for(order);
  return static_cast<U32>(ipix / (nside * nside));
}

std::array<U64, 2> healpix_face_xy(U32 order, U64 ipix) {
  const U64 nside = nside_for(order);
  const U64 p = ipix % (nside * nside);
  U64 ix = 0, iy = 0;
  for (U32 i = 0; i < order; ++i) {
    const U64 bit = (p >> (2 * i)) & 1ull;
    if (bit) ix |= (1ull << i);
  }
  for (U32 i = 0; i < order; ++i) {
    const U64 bit = (p >> (2 * i + 1)) & 1ull;
    if (bit) iy |= (1ull << i);
  }
  return {ix, iy};
}

U64 healpix_face_index(U32 order, U64 ix, U64 iy) {
  U64 p = 0;
  for (U32 i = 0; i < order; ++i) {
    if ((ix >> i) & 1ull) p |= (1ull << (2 * i));
    if ((iy >> i) & 1ull) p |= (1ull << (2 * i + 1));
  }
  return p;
}

SkyICRS healpix_face_point(U32 face, double s, double t) {
  return face_point(kFaces[face % 12], s, t);
}

SkyICRS healpix_center(U32 order, U64 ipix) {
  const U64 nside = nside_for(order);
  const auto xy = healpix_face_xy(order, ipix);
  const double s = (static_cast<double>(xy[0]) + 0.5) / static_cast<double>(nside);
  const double t = (static_cast<double>(xy[1]) + 0.5) / static_cast<double>(nside);
  return healpix_face_point(healpix_face(order, ipix), s, t);
}

double healpix_face_area() { return M_PI / 3.0; }

double healpix_leaf_area(U32 order) {
  const double nside = static_cast<double>(nside_for(order));
  return (M_PI / 3.0) / (nside * nside);
}

std::vector<HealpixLeaf> healpix_all_leaves(U32 order) {
  std::vector<HealpixLeaf> leaves;
  const U64 count = npix_for(order);
  leaves.reserve(static_cast<std::size_t>(count));
  for (U64 ipix = 0; ipix < count; ++ipix) leaves.push_back(HealpixLeaf{order, ipix});
  return leaves;
}

bool healpix_lookup(U32 order, const SkyICRS& direction, HealpixLeaf& leaf) {
  const double norm = std::sqrt(direction.nx * direction.nx + direction.ny * direction.ny +
                                direction.nz * direction.nz);
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const double nx = direction.nx / norm;
  const double ny = direction.ny / norm;
  const double nz = direction.nz / norm;
  const double phi = std::atan2(ny, nx);
  double tt = phi / (M_PI / 2.0);
  if (tt < 0.0) tt += 4.0;
  const double z = nz;
  const U64 nside = nside_for(order);
  U64 face = 0;
  U64 ix = 0, iy = 0;
  if (std::fabs(z) <= 2.0 / 3.0) {
    const double jp = std::floor(static_cast<double>(nside) * (0.5 + tt - 0.75 * z));
    const double jm = std::floor(static_cast<double>(nside) * (0.5 + tt + 0.75 * z));
    const U64 ifp = static_cast<U64>(jp) / nside;
    const U64 ifm = static_cast<U64>(jm) / nside;
    U64 ifinal = 0;
    if (ifp == ifm) {
      ifinal = ifp | 4ull;
    } else if (ifp < ifm) {
      ifinal = ifp;
    } else {
      ifinal = ifm + 8ull;
    }
    face = ifinal;
    const U64 ixp = static_cast<U64>(jp) % nside;
    const U64 ixm = static_cast<U64>(jm) % nside;
    ix = ixm;
    iy = nside - 1 - ixp;
  } else {
    const bool north = z > 0.0;
    const U64 sector = static_cast<U64>(std::floor(tt));
    const double tp = tt - static_cast<double>(sector);
    const double tmp = static_cast<double>(nside) * std::sqrt(3.0 * (1.0 - std::fabs(z)));
    const U64 jp = std::min<U64>(nside - 1, static_cast<U64>(std::floor(tp * tmp)));
    const U64 jm = std::min<U64>(nside - 1, static_cast<U64>(std::floor((1.0 - tp) * tmp)));
    if (north) {
      face = sector % 4ull;
      ix = nside - jm - 1;
      iy = nside - jp - 1;
    } else {
      face = 8ull + (sector % 4ull);
      ix = jp;
      iy = jm;
    }
  }
  if (face >= 12) return false;
  const U64 ipix = face * nside * nside + healpix_face_index(order, ix, iy);
  leaf.order = order;
  leaf.ipix = ipix;
  return true;
}

bool healpix_candidate_leaves(U32 order, const SkyICRS& direction,
                              std::vector<HealpixLeaf>& leaves) {
  leaves.clear();
  const double norm = std::sqrt(direction.nx * direction.nx + direction.ny * direction.ny +
                                direction.nz * direction.nz);
  if (!(norm > 0.0) || !std::isfinite(norm)) return false;
  const double nx = direction.nx / norm;
  const double ny = direction.ny / norm;
  const double nz = direction.nz / norm;
  const double phi = std::atan2(ny, nx);
  double tt = phi / (M_PI / 2.0);
  if (tt < 0.0) tt += 4.0;
  const double z = nz;
  const U64 nside = nside_for(order);
  if (std::fabs(z) <= 2.0 / 3.0) {
    // The longitude may wrap and the two bounding indices may differ; take the
    // union of the possible members.
    const double lo = std::floor(static_cast<double>(nside) * (0.5 + tt - 0.75 * z));
    const double hi = std::floor(static_cast<double>(nside) * (0.5 + tt + 0.75 * z));
    for (int delta = -1; delta <= 1; ++delta) {
      for (double base : {lo, hi}) {
        const double shifted = base + delta * static_cast<double>(nside);
        if (shifted < 0.0) continue;
        const U64 index = static_cast<U64>(shifted);
        const U64 ifinal = index / nside;
        const U64 ix = index % nside;
        const U64 iy = nside - 1 - ix;
        const U64 face_candidates[3] = {ifinal, ifinal | 4ull, ifinal + 8ull};
        for (U64 face : face_candidates) {
          if (face >= 12) continue;
          const U64 ipix = face * nside * nside + healpix_face_index(order, ix, iy);
          leaves.push_back(HealpixLeaf{order, ipix});
        }
      }
    }
  } else {
    const bool north = z > 0.0;
    const U64 sector = static_cast<U64>(std::floor(tt));
    const double tp = tt - static_cast<double>(sector);
    const double tmp = static_cast<double>(nside) * std::sqrt(3.0 * (1.0 - std::fabs(z)));
    const U64 jp = std::min<U64>(nside - 1, static_cast<U64>(std::floor(tp * tmp)));
    const U64 jm = std::min<U64>(nside - 1, static_cast<U64>(std::floor((1.0 - tp) * tmp)));
    for (int ds = -1; ds <= 1; ++ds) {
      const U64 sec = (sector + static_cast<U64>(ds)) % 4ull;
      U64 ix = 0, iy = 0;
      if (north) {
        ix = nside - jm - 1;
        iy = nside - jp - 1;
      } else {
        ix = jp;
        iy = jm;
      }
      const U64 face = north ? sec : 8ull + sec;
      const U64 ipix = face * nside * nside + healpix_face_index(order, ix, iy);
      leaves.push_back(HealpixLeaf{order, ipix});
    }
  }
  std::sort(leaves.begin(), leaves.end(), [](const HealpixLeaf& a, const HealpixLeaf& b) {
    return a.ipix < b.ipix;
  });
  leaves.erase(std::unique(leaves.begin(), leaves.end(),
                           [](const HealpixLeaf& a, const HealpixLeaf& b) {
                             return a.order == b.order && a.ipix == b.ipix;
                           }),
               leaves.end());
  return true;
}

}  // namespace acsd::coordinates
