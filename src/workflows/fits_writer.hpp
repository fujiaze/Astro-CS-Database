#pragma once
// FITS writing for the published products.
//
// The writer uses the system CFITSIO library.  Two products exist:
//
//   * the export product carries a single two dimensional image (signal.fits)
//     with the target WCS;
//   * the normalize and mosaic products carry the HiPS tiles of every order
//     with the standard HiPS orientation (row = W - 1 - x, col = y).
//
// The HPX header of a HiPS tile is not the centred TAN header: the reference
// pixel is obtained from the linear offset of the tile inside its face, and
// the face 6 longitude interruption keeps its own legal native projection
// centre instead of being patched with an array offset.
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::workflows {

// One header card of the written image.
using HeaderCard = std::pair<std::string, std::string>;

// Writes a two dimensional float64 image.  The values are in row major order
// (row 0 first) and the file is padded to the FITS block size.
Result<std::string> write_fits_image(const std::filesystem::path& path, U64 width, U64 height,
                                     const std::vector<double>& values,
                                     const std::vector<HeaderCard>& cards);

// Writes the HiPS tiles of one order below `root`.  The tiles keep the
// standard orientation; the value of a leaf without support is NaN.
Result<std::string> write_hips_order(const std::filesystem::path& root, U32 order,
                                      U32 tile_log2_width,
                                      const std::vector<HealpixLeaf>& leaves,
                                      const std::vector<double>& values, const std::string& bunit);

// Writes the two dimensional export image with its target WCS.  The image
// keeps the size of the target: the given centre is placed at
// ((width - 1) / 2, (height - 1) / 2) and the reference pixel is
// CRPIX = ((width + 1) / 2, (height + 1) / 2).
Result<std::string> write_export_image(Context& context, const std::filesystem::path& root,
                                       const ExportResult& image);

// Writes the HiPS properties file.
Result<std::string> write_hips_properties(const std::filesystem::path& root, U32 order,
                                          U32 tile_log2_width, const std::string& bunit);

}  // namespace acsd::workflows
