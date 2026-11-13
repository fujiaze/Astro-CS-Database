#pragma once
// The native pixel scale of one frame and the HEALPix leaf order it requires.
//
// The scientific requirement is the one of the main production auto-NSIDE
// (`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`,
// `compute_auto_nside_ex`, issue #126 section 2): the local angular pixel scale
// is the smallest singular value of the 3D tangent-plane Jacobian of the final
// WCS/SIP map, sampled with the adaptive conservative quadtree of the main
// implementation; the leaf order is the smallest power of two whose HEALPix
// characteristic scale sqrt(pi/3)/nside is *not coarser* than the finest local
// native pixel scale, clamped to nside in [2^4, 2^22].
//
// This module only answers "what does the real WCS require"; it does not
// choose or write a product.  The caller (Normalize) owns the decision record.
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::coordinates {

// The characteristic linear scale of one HEALPix leaf:
// sqrt(4*pi/(12*nside^2)) = sqrt(pi/3)/nside rad, in arcsec.  This is the same
// constant as the main implementation (no magic 210960: it follows from the
// equal-area pixel formula).
constexpr double kHealpixScalePerNsideArcsec = 211076.3;

// The clamp of the automatic order (main: nside in [16, 4194304]).
constexpr U32 kMinimumAutoOrder = 4;
constexpr U32 kMaximumAutoOrder = 22;

// One frame's sampled pixel scale.
struct PixelScaleProbe {
  double finest_arcsec = 0.0;    // min over samples of the local pixel scale
  double coarsest_arcsec = 0.0;  // max over samples
  U64 valid_samples = 0;
  U64 invalid_samples = 0;
  bool valid = false;
  std::string reason;
};

// The order a frame requires, derived from the probe.
struct LeafOrderDecision {
  bool valid = false;
  U32 order = 0;             // nside = 2^order
  double nside = 0.0;        // 2^order as a double
  double finest_arcsec = 0.0;
  double leaf_arcsec = 0.0;  // characteristic scale of one leaf, arcsec
  double oversample = 0.0;   // leaf_arcsec / finest_arcsec (>= 1 when unclamped)
  double required_nside = 0.0;
  bool clamped = false;
  std::string reason;
};

// Samples the local angular pixel scale of the WCS over the detector.
PixelScaleProbe probe_pixel_scale(const WcsModel& wcs, U64 width, U64 height);

// Decides the minimal leaf order whose characteristic scale is not coarser
// than the finest native pixel scale.
LeafOrderDecision decide_leaf_order(const WcsModel& wcs, U64 width, U64 height);

// The accumulation capacity implied by the certified pixel/leaf scale ratio.
// A pixel of r leaf widths (r = pixel_scale / leaf_scale) intersects at most
// about (r + 1) cells per axis, so it emits (r + 1)^2 terms; the touched
// leaves are the covered leaf area plus a one-leaf perimeter.  The bounds are
// the ones the geometry accumulation enforces and the invocation plan declares
// to the resource ledger, so the declared and the actual storage agree.
struct DrizzleCapacityEstimate {
  bool valid = false;
  double pixel_leaf_ratio = 0.0;
  U64 maximum_leaves = 0;
  U64 maximum_terms = 0;
  U64 accumulation_bytes = 0;  // per-leaf aggregate table + member lists
  U64 output_bytes = 0;        // the published leaf arrays and CSR rows
  U64 declared_bytes = 0;      // accumulation + output + control
};
DrizzleCapacityEstimate estimate_drizzle_capacity(const WcsModel& wcs, U64 width, U64 height,
                                                  U32 order);

}  // namespace acsd::coordinates
