// The minimal fixture of the observation path: the fields that the public
// entries need, and nothing else.
//
// The file is written for the independent verification: the fields can be
// copied into a driver that only includes <acsd/contracts.hpp> and that
// implements its own Storage, Resources and Cancellation.
//
// Two conventions matter and are easy to get wrong:
//
//  * The local cell of a native sample is centred on the sample
//    ([-0.5, 0.5] in the local coordinates), because the positions of the
//    sources are PixelCenter0 values, that is the centre of the sample.  A
//    cell of [0, 1] shifts every footprint by half a pixel relative to the
//    positions.  A cell that is not set at all is the degenerate box
//    {0, 0} x {0, 0}: it has no area, the pixel integral of every sample is
//    zero and the design matrix of the PSF fit collapses to the background
//    column alone (rank one).
//
//  * The mapping from the samples to the detector is the identity matrix with
//    a zero offset for a detector whose samples are laid out row by row.  The
//    sample with the index r lies at (r % width, r / width).
#ifndef ACSD_VALIDATION_OBS_FIXTURE_HPP
#define ACSD_VALIDATION_OBS_FIXTURE_HPP

#include <memory>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd_validation {

// The minimal detector footprint set of a frame of the given size.  The
// fields that are not set here are not needed by the public entries:
//   wcs                 the frame model is passed separately to the entries
//   pixel_response      the photometry path of the first release uses a
//                       single band piecewise constant photosite response, the
//                       PSF shape fit does not read the response
//   astrometry          the astrometric uncertainty is not part of the PSF
//                       shape fit
//   inline_footprints   the detector footprint of a regular detector is the
//                       local cell of every sample
//   sample_response_gain the gain of every sample is uniform
inline std::shared_ptr<acsd::FootprintSet> make_minimal_footprints(
    std::shared_ptr<const acsd::Index> index, acsd::U64 width, acsd::U64 height,
    const acsd::Id& detector_frame) {
  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->index = index;
  footprints->width = width;
  footprints->height = height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->offset = {0.0, 0.0};
  footprints->local_cell.lower = {-0.5, -0.5};
  footprints->local_cell.upper = {0.5, 0.5};
  footprints->detector_frame = detector_frame;
  footprints->identity.object_id = "footprints:" + detector_frame;
  footprints->identity.content_hash = "sha256:" + std::string(64, '0');
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->assessment.complete = true;
  footprints->assessment.execution = acsd::ExecutionState::complete;
  return footprints;
}

// The number of parameters of the PSF shape fit for a given number of
// sources and a spatial degree.  The design matrix of the fit has one row per
// native sample of the window, so the number of rows has to exceed the
// number of columns, and the rows have to be independent.
inline acsd::U64 psf_parameter_count(acsd::U32 spatial_degree, std::size_t sources) {
  const acsd::U64 terms = static_cast<acsd::U64>((spatial_degree + 1) * (spatial_degree + 2) / 2);
  // The shape of the profile (three parameters per family) and the spatial
  // polynomial of that degree.
  const acsd::U64 shape = 3 * terms;
  // Two continuous positions and one flux per source, and the background.
  return shape + 3 * static_cast<acsd::U64>(sources) + 1;
}

}  // namespace acsd_validation

#endif  // ACSD_VALIDATION_OBS_FIXTURE_HPP
