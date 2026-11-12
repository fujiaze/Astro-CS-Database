#pragma once
// Streaming overlap of a detector pixel grid with the equal-area leaves.
//
// compute_overlap materialises one Overlaps object per source.  That is exact
// but it does not scale to a 4096x4096 frame: the per-source primitive tables
// dominate both the runtime bookkeeping and the memory.  This function walks
// the same pixels with the same certified affine chart model and emits the
// same (source, leaf, a_jp, A_j, Omega_j) terms one by one, so the caller can
// accumulate them directly (drizzle_grid) and never hold them all.
//
// The per-pixel result is the union of two exact statements:
//   * a pixel whose certified chart enclosure lies inside one leaf cell has
//     a_jp = A_j exactly (w = 1);
//   * a pixel that crosses a cell boundary is clipped against the four
//     inner/outer half spaces in interval arithmetic; ambiguous clips fall
//     back to the exact rational kernel of the same pixel.
#include <functional>

#include "acsd/contracts.hpp"
#include "acsd/model.hpp"

namespace acsd::geometry {

struct PixelTerm {
  U32 order = 0;
  U64 ipix = 0;
  double overlap_lower = 0.0;
  double overlap_upper = 0.0;
  double drop_lower = 0.0;
  double drop_upper = 0.0;
  double native_lower = 0.0;
  double native_upper = 0.0;
};

using PixelTermSink = std::function<Result<rt::Done>(U64 source_row, const PixelTerm&)>;

Result<rt::Done> pixel_grid_overlap(Context& context, const WcsModel& wcs, U64 width, U64 height,
                                    U64 first_row, U64 row_count, U32 order, double pixfrac,
                                    const GeometryBudget& budget, const PixelTermSink& sink);

}  // namespace acsd::geometry
