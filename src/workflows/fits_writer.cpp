// FITS writing for the published products.
//
// The writer produces the standard FITS layout: 2880 byte header blocks of
// 80 character cards followed by the data unit, padded with zero bytes to a
// multiple of the block size.  The float64 data is written in the big endian
// order of the FITS standard.
//
// The installed CFITSIO 4.6.2 is used to read the products back (the
// validation program of validation/apps reads a published image with
// CFITSIO), while the writing path is explicit here: the library creation
// path of this installation (ffinit + ffcrim + ffppx) writes an unbounded
// file, which is reproduced in validation/apps/fits_cfitsio_probe.txt.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "workflows/fits_writer.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::workflows {
namespace {

using model::fail;
using model::ok_result;

constexpr std::size_t kBlock = 2880;

// One header card: the keyword, the value and an optional comment.
std::string card(const std::string& keyword, const std::string& value,
                 const std::string& comment) {
  std::string text = keyword;
  text.resize(8, ' ');
  text += "= ";
  text += value;
  if (text.size() < 30) text.resize(30, ' ');
  if (!comment.empty()) text += " / " + comment;
  // Every card is exactly 80 characters long.
  text.resize(80, ' ');
  return text;
}

std::string numeric_value(double value) {
  std::ostringstream stream;
  if (value == static_cast<double>(static_cast<long long>(value)) && std::fabs(value) < 1e15) {
    stream << static_cast<long long>(value);
  } else {
    stream.precision(12);
    stream << std::scientific << value;
  }
  std::string text = stream.str();
  if (text.size() > 20) {
    std::ostringstream alternative;
    alternative.precision(12);
    alternative << std::scientific << value;
    text = alternative.str();
  }
  return text;
}

std::string string_value(const std::string& value) {
  std::string text = "'";
  for (char character : value) {
    if (character == '\'') text += "''";
    else text += character;
  }
  text += "'";
  if (text.size() < 8) text.resize(8, ' ');
  return text;
}

std::string header_block(const std::vector<std::string>& cards) {
  std::string block;
  for (const std::string& entry : cards) block += entry;
  block += "END";
  block.resize(kBlock, ' ');
  return block;
}

// The big endian float64 payload of an image.
std::vector<unsigned char> encode_big_endian(const std::vector<double>& values) {
  std::vector<unsigned char> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &values[i], sizeof(bits));
    for (int byte = 0; byte < 8; ++byte) {
      bytes[i * 8 + byte] = static_cast<unsigned char>((bits >> (8 * (7 - byte))) & 0xffull);
    }
  }
  return bytes;
}

Result<std::string> write_bytes(const std::filesystem::path& path,
                                const std::vector<unsigned char>& bytes) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return fail<std::string>(Code::io_error, "the FITS file cannot be opened", "workflows.fits",
                             "the product cannot be published");
  }
  if (!bytes.empty()) {
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }
  stream.flush();
  if (!stream) {
    return fail<std::string>(Code::io_error, "the FITS file cannot be written", "workflows.fits",
                             "the product cannot be published");
  }
  return ok_result(std::string());
}

// The number of tiles of one order.
U64 tile_count(U32 tile_order) { return 12ull * (1ull << (2 * tile_order)); }

// The linear anchor of a face in the chart.
//
// The equatorial faces 0..3 use the cylindrical chart with the face centre at
// (L_f * 45 degrees, 0); the polar faces 4..11 use the azimuthal chart with
// the face centre at (L_f * 45 degrees, -45 degrees) in the north and at
// (L_f * 45 degrees, +45 degrees) in the south.  The face 6 keeps its own
// legal native centre (180 degrees): the longitude interruption is not
// patched with an array offset.
struct FaceAnchor {
  double centre_longitude_deg;
  double centre_latitude_deg;
};

FaceAnchor face_anchor(U32 face) {
  static const int kLf[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};
  FaceAnchor anchor;
  anchor.centre_longitude_deg = 45.0 * kLf[face];
  if (face < 4) {
    anchor.centre_latitude_deg = 0.0;
  } else if (face < 8) {
    anchor.centre_latitude_deg = -45.0;
  } else {
    anchor.centre_latitude_deg = 45.0;
  }
  return anchor;
}

void append_header(const std::vector<HeaderCard>& cards, std::vector<std::string>& header) {
  for (const HeaderCard& entry : cards) {
    const bool numeric = entry.first == "CRPIX1" || entry.first == "CRPIX2" ||
                         entry.first == "CRVAL1" || entry.first == "CRVAL2" ||
                         entry.first == "CD1_1" || entry.first == "CD1_2" ||
                         entry.first == "CD2_1" || entry.first == "CD2_2";
    if (numeric) {
      header.push_back(card(entry.first, numeric_value(std::stod(entry.second)), ""));
    } else {
      header.push_back(card(entry.first, string_value(entry.second), ""));
    }
  }
}

}  // namespace

Result<std::string> write_fits_image(const std::filesystem::path& path, U64 width, U64 height,
                                     const std::vector<double>& values,
                                     const std::vector<HeaderCard>& cards) {
  if (width == 0 || height == 0) {
    return fail<std::string>(Code::invalid_request, "the image is empty", "workflows.fits",
                             "the product cannot be published");
  }
  if (values.size() != width * height) {
    return fail<std::string>(Code::invalid_model, "the image size does not match the pixel count",
                             "workflows.fits", "the product cannot be published");
  }
  std::vector<std::string> header;
  header.push_back(card("SIMPLE", "T", "file does conform to FITS standard"));
  header.push_back(card("BITPIX", "-64", "number of bits per data pixel"));
  header.push_back(card("NAXIS", "2", "number of data axes"));
  header.push_back(card("NAXIS1", std::to_string(width), "length of data axis 1"));
  header.push_back(card("NAXIS2", std::to_string(height), "length of data axis 2"));
  append_header(cards, header);
  std::vector<unsigned char> bytes;
  const std::string block = header_block(header);
  bytes.insert(bytes.end(), block.begin(), block.end());
  const std::vector<unsigned char> data = encode_big_endian(values);
  bytes.insert(bytes.end(), data.begin(), data.end());
  const std::size_t padding = (kBlock - (bytes.size() % kBlock)) % kBlock;
  bytes.insert(bytes.end(), padding, 0);
  return write_bytes(path, bytes);
}

Result<std::string> write_tile_file(const std::filesystem::path& path, U64 tile_width,
                                    U64 tile_count_in_file, const std::vector<double>& pixels,
                                    const std::vector<HeaderCard>& cards) {
  std::vector<std::string> header;
  header.push_back(card("SIMPLE", "T", "file does conform to FITS standard"));
  header.push_back(card("BITPIX", "-64", "number of bits per data pixel"));
  header.push_back(card("NAXIS", "3", "number of data axes"));
  header.push_back(card("NAXIS1", std::to_string(tile_width), "length of data axis 1"));
  header.push_back(card("NAXIS2", std::to_string(tile_width), "length of data axis 2"));
  header.push_back(card("NAXIS3", std::to_string(tile_count_in_file), "length of data axis 3"));
  append_header(cards, header);
  std::vector<unsigned char> bytes;
  const std::string block = header_block(header);
  bytes.insert(bytes.end(), block.begin(), block.end());
  const std::vector<unsigned char> data = encode_big_endian(pixels);
  bytes.insert(bytes.end(), data.begin(), data.end());
  const std::size_t padding = (kBlock - (bytes.size() % kBlock)) % kBlock;
  bytes.insert(bytes.end(), padding, 0);
  return write_bytes(path, bytes);
}

Result<std::string> write_hips_order(const std::filesystem::path& root, U32 order,
                                      U32 tile_log2_width, const std::vector<HealpixLeaf>& leaves,
                                      const std::vector<double>& values, const std::string& bunit) {
  if (tile_log2_width < 6 || tile_log2_width > 9) {
    return fail<std::string>(Code::invalid_request, "the tile width is outside the allowed range",
                             "workflows.hips", "the product cannot be published");
  }
  if (order < tile_log2_width) {
    return fail<std::string>(Code::invalid_request, "the leaf order is below the tile width",
                             "workflows.hips", "the product cannot be published");
  }
  const U32 tile_order = order - tile_log2_width;
  const U64 tile_width = 1ull << tile_log2_width;
  const U64 tiles = tile_count(tile_order);
  const U64 nside = 1ull << order;

  // The signal of every leaf of the order, indexed by the nested index.
  std::vector<double> sky(nside * nside * 12, std::nan(""));
  for (std::size_t i = 0; i < leaves.size() && i < values.size(); ++i) {
    if (leaves[i].order != order) continue;
    if (leaves[i].ipix >= sky.size()) continue;
    sky[leaves[i].ipix] = values[i];
  }

  const U64 group_size = 10000;
  for (U64 first = 0; first < tiles; first += group_size) {
    const U64 count = std::min(group_size, tiles - first);
    std::vector<double> pixels(static_cast<std::size_t>(tile_width) * tile_width * count,
                               std::nan(""));
    std::vector<HeaderCard> cards;
    for (U64 tile = first; tile < first + count; ++tile) {
      const U32 face = static_cast<U32>(tile / (1ull << (2 * tile_order)));
      const U64 in_face = tile % (1ull << (2 * tile_order));
      U64 tile_ix = 0;
      U64 tile_iy = 0;
      for (U32 level = 0; level < tile_order; ++level) {
        const U64 shift = tile_order - 1 - level;
        if ((in_face >> (2 * level)) & 1ull) tile_ix |= 1ull << shift;
        if ((in_face >> (2 * level + 1)) & 1ull) tile_iy |= 1ull << shift;
      }
      // The reference pixel of the tile comes from the linear offset of the
      // tile inside its face; the centred TAN reference is not used.
      const FaceAnchor anchor = face_anchor(face);
      const double s_first = static_cast<double>(tile_ix) / static_cast<double>(1ull << tile_order) +
                             (1.0 - 0.5 / static_cast<double>(tile_width));
      const double t_first = static_cast<double>(tile_iy) / static_cast<double>(1ull << tile_order) +
                             0.5 / static_cast<double>(tile_width);
      const double u = 45.0 * (s_first - t_first);
      const double v = 45.0 * (s_first + t_first) + anchor.centre_latitude_deg;
      const double n = static_cast<double>(tile_width);
      const double crpix1 = 1.0 - n * (v - anchor.centre_latitude_deg - u) / 90.0;
      const double crpix2 = 1.0 + n * (u + v - anchor.centre_latitude_deg) / 90.0;
      if (tile == first) {
        cards.clear();
        cards.emplace_back("CTYPE1", "RA---HPX");
        cards.emplace_back("CTYPE2", "DEC--HPX");
        cards.emplace_back("CUNIT1", "deg");
        cards.emplace_back("CUNIT2", "deg");
        cards.emplace_back("RADESYS", "ICRS");
        cards.emplace_back("CRVAL1", numeric_value(anchor.centre_longitude_deg));
        cards.emplace_back("CRVAL2", numeric_value(anchor.centre_latitude_deg));
        cards.emplace_back("CRPIX1", numeric_value(crpix1));
        cards.emplace_back("CRPIX2", numeric_value(crpix2));
        cards.emplace_back("CD1_1", numeric_value(-45.0 / n));
        cards.emplace_back("CD1_2", numeric_value(-45.0 / n));
        cards.emplace_back("CD2_1", numeric_value(45.0 / n));
        cards.emplace_back("CD2_2", numeric_value(-45.0 / n));
        cards.emplace_back("BUNIT", bunit);
      }
      for (U64 dy = 0; dy < tile_width; ++dy) {
        for (U64 dx = 0; dx < tile_width; ++dx) {
          const U64 ix = tile_ix * tile_width + dx;
          const U64 iy = tile_iy * tile_width + dy;
          U64 nested = 0;
          for (U32 level = 0; level < order; ++level) {
            const U64 shift = order - 1 - level;
            if ((ix >> shift) & 1ull) nested |= 1ull << (2 * level);
            if ((iy >> shift) & 1ull) nested |= 1ull << (2 * level + 1);
          }
          const U64 ipix = static_cast<U64>(face) * nside * nside + nested;
          // The standard orientation: row = W - 1 - x, col = y.
          const std::size_t row = static_cast<std::size_t>(tile_width - 1 - dx);
          const std::size_t col = static_cast<std::size_t>(iy % tile_width);
          const std::size_t offset =
              (static_cast<std::size_t>(tile - first) * tile_width + row) * tile_width + col;
          if (ipix < sky.size() && offset < pixels.size()) pixels[offset] = sky[ipix];
        }
      }
    }
    const std::filesystem::path directory =
        root / ("Norder" + std::to_string(order)) / ("Dir" + std::to_string(first));
    const std::filesystem::path file = directory / ("Npix" + std::to_string(first) + ".fits");
    Result<std::string> written = write_tile_file(file, tile_width, count, pixels, cards);
    if (!ok(written)) return written;
  }
  return ok_result(std::string());
}

Result<std::string> write_export_image(Context& context, const std::filesystem::path& root,
                                       const ExportResult& image) {
  if (!image.estimate || !image.estimate->signal) {
    return fail<std::string>(Code::invalid_request, "the export has no signal", "workflows.fits",
                             "the product cannot be published");
  }
  if (!image.target.model) {
    return fail<std::string>(Code::invalid_request, "the export has no target wcs", "workflows.fits",
                             "the product cannot be published");
  }
  const U64 width = image.target.width;
  const U64 height = image.target.height;
  U64 count = 1;
  for (U64 extent : image.estimate->signal->shape) count *= extent;
  Result<ArrayView> view = context.storage.read(context, image.estimate->signal, BlockRequest{0, count});
  if (!ok(view)) {
    return fail<std::string>(Code::io_error, "the export signal cannot be read", "workflows.fits",
                             "the product cannot be published");
  }
  const std::vector<double> values = model::decode_f64(val(view).bytes);

  const WcsModel& wcs = *image.target.model;
  std::string ctype1 = "RA---TAN";
  std::string ctype2 = "DEC--TAN";
  for (const HeaderField& field : wcs.original_header) {
    if (field.key == "CTYPE1" && std::holds_alternative<std::string>(field.value)) {
      ctype1 = std::get<std::string>(field.value);
    }
    if (field.key == "CTYPE2" && std::holds_alternative<std::string>(field.value)) {
      ctype2 = std::get<std::string>(field.value);
    }
  }
  const double radians_to_degrees = 180.0 / M_PI;
  const double ra = std::atan2(wcs.c[1], wcs.c[0]) * radians_to_degrees;
  const double dec = std::asin(std::max(-1.0, std::min(1.0, wcs.c[2]))) * radians_to_degrees;
  std::vector<HeaderCard> cards;
  cards.emplace_back("CTYPE1", ctype1);
  cards.emplace_back("CTYPE2", ctype2);
  cards.emplace_back("CUNIT1", "deg");
  cards.emplace_back("CUNIT2", "deg");
  cards.emplace_back("RADESYS", "ICRS");
  cards.emplace_back("CRVAL1", numeric_value(ra));
  cards.emplace_back("CRVAL2", numeric_value(dec));
  // The given centre is placed at ((width - 1) / 2, (height - 1) / 2); the
  // reference pixel is CRPIX = ((width + 1) / 2, (height + 1) / 2).
  cards.emplace_back("CRPIX1", numeric_value(0.5 * (static_cast<double>(width) + 1.0)));
  cards.emplace_back("CRPIX2", numeric_value(0.5 * (static_cast<double>(height) + 1.0)));
  cards.emplace_back("CD1_1", numeric_value(wcs.cd_rad_per_pixel[0] * radians_to_degrees));
  cards.emplace_back("CD1_2", numeric_value(wcs.cd_rad_per_pixel[1] * radians_to_degrees));
  cards.emplace_back("CD2_1", numeric_value(wcs.cd_rad_per_pixel[2] * radians_to_degrees));
  cards.emplace_back("CD2_2", numeric_value(wcs.cd_rad_per_pixel[3] * radians_to_degrees));
  // The unit of the signal: the named linear scale with its solid angle
  // power.  A non standard model unit is never presented as a standard SI
  // unit.
  std::string bunit = image.estimate->unit.scale;
  if (image.estimate->unit.sr_power < 0) bunit += "/sr";
  cards.emplace_back("BUNIT", bunit);
  return write_fits_image(root / "signal.fits", width, height, values, cards);
}

Result<std::string> write_hips_properties(const std::filesystem::path& root, U32 order,
                                          U32 tile_log2_width, const std::string& bunit) {
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  std::ostringstream text;
  text << "hips_frame=equatorial\n";
  text << "hips_order=" << order << "\n";
  text << "hips_tile_width=" << (1u << tile_log2_width) << "\n";
  text << "hips_tile_format=fits\n";
  text << "dataproduct_type=image\n";
  text << "hips_builder=acsd-v2\n";
  text << "hips_version=2.0.0\n";
  text << "hips_data_release=2\n";
  if (!bunit.empty()) text << "hips_unit=" << bunit << "\n";
  const std::filesystem::path path = root / "properties";
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return fail<std::string>(Code::io_error, "the properties file cannot be written", "workflows.hips",
                             "the product cannot be published");
  }
  stream << text.str();
  stream.flush();
  if (!stream) {
    return fail<std::string>(Code::io_error, "the properties file cannot be flushed", "workflows.hips",
                             "the product cannot be published");
  }
  return ok_result(std::string());
}

}  // namespace acsd::workflows
