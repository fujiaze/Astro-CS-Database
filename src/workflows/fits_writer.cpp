// FITS writing for the published products.
//
// The writer produces the standard FITS layout: 2880 byte header blocks of
// 80 character cards followed by the data unit, padded with zero bytes to a
// multiple of the block size.  The float64 data is written in the big endian
// order of the FITS standard.
//
// The writing of the products is an explicit implementation of the FITS
// standard: the header blocks of 2880 bytes with the 80 character cards and
// the big endian float64 data unit.
//
// The installed CFITSIO 4.6.2 is only used for reading (fits_read_img, the
// short name ffgpv: the element reader ffgpxv of this build is the reader of
// the binary tables, not of the images).  Its writing path is not usable: the
// call fits_write_pix (ffppx) reports success but leaves the data unit at
// zero.  The probe and the evidence are in
// validation/apps/fits_cfitsio_probe.txt.
//
// Two facts of the API of the library that the reading code respects: the
// axis lengths of ffgisz are written directly into the array (the first entry
// is the length of the first axis) and the datatype of ffgky and ffpky is the
// first argument.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
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
//
// The standard right justifies a number and a logical in the value field
// (the value ends in column 30) and left justifies a quoted string (the
// value starts in column 11).  A strict reader (astropy) refuses a file
// whose SIMPLE card does not follow that rule.
std::string card(const std::string& keyword, const std::string& value, const std::string& comment,
                 bool right_justified = true, bool* fits = nullptr) {
  if (fits) *fits = true;
  if (keyword.size() > 8) {
    if (fits) *fits = false;
    return std::string();
  }
  std::string text = keyword;
  text.resize(8, ' ');
  text += "= ";
  if (right_justified && text.size() + value.size() < 30) {
    text.append(30 - text.size() - value.size(), ' ');
  }
  text += value;
  if (text.size() < 30) text.resize(30, ' ');
  if (!comment.empty()) text += " / " + comment;
  // Every card is exactly 80 characters long; a value that does not fit is a
  // product defect, never padded away.  The old code truncated a long ACSUNIT
  // string and published an unparsable card.
  if (text.size() > 80) {
    if (fits) *fits = false;
    return std::string();
  }
  text.resize(80, ' ');
  return text;
}

// A quoted FITS string value (single quotes doubled).
std::string quoted_string(const std::string& value) {
  std::string text = "'";
  for (char character : value) {
    if (character == '\'') text += "''";
    else text += character;
  }
  text += "'";
  return text;
}

// A long string card.  FITS limits a quoted value field to 68 characters; a
// longer value is split with the standard CONTINUE convention (the last
// character of each non-final chunk is '&'; astropy and cfitsio parse it).
bool append_long_string(std::vector<std::string>& header, const std::string& keyword,
                        const std::string& value) {
  if (keyword.size() > 8) return false;
  std::string escaped;
  for (char character : value) {
    if (character == '\'') escaped += "''";
    else escaped += character;
  }
  const std::size_t chunk = 66;
  std::size_t offset = 0;
  bool first = true;
  while (offset < escaped.size() || first) {
    const std::size_t count = std::min(chunk, escaped.size() - offset);
    const bool more = offset + count < escaped.size();
    std::string content = escaped.substr(offset, count);
    if (more) content += "&";
    std::string text;
    if (first) {
      text = keyword;
      text.resize(8, ' ');
      text += "= '";
      first = false;
    } else {
      text = "CONTINUE  '";
    }
    text += content;
    text += "'";
    if (text.size() > 80) return false;
    text.resize(80, ' ');
    header.push_back(text);
    offset += count;
  }
  return true;
}

// The value of a numeric card.
//
// The FITS standard writes a floating point value with an uppercase exponent
// and a mantissa of at most twelve significant digits ("1.234567890123E+23");
// a strict reader (astropy) refuses the lowercase exponent of the C++ stream.
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
  // The exponent of the standard is uppercase.
  for (char& character : text) {
    if (character == 'e') character = 'E';
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
  // Pad to the next 2880 byte boundary.  The former resize(kBlock) truncated
  // the block whenever the cards plus END exceeded one record: a 4096 export
  // header with 37 cards silently lost its CONTINUE card and its END marker,
  // so astropy refused the file ("missing END card").  A FITS header is never
  // truncated, it is only padded.
  const std::size_t records = (block.size() + kBlock - 1) / kBlock;
  block.resize(records * kBlock, ' ');
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

bool append_header(const std::vector<HeaderCard>& cards, std::vector<std::string>& header) {
  bool fits = true;
  for (const HeaderCard& entry : cards) {
    const bool sip_numeric =
        entry.first == "A_ORDER" || entry.first == "B_ORDER" ||
        entry.first.rfind("A_", 0) == 0 || entry.first.rfind("B_", 0) == 0;
    const bool numeric = sip_numeric || entry.first == "CRPIX1" || entry.first == "CRPIX2" ||
                         entry.first == "CRVAL1" || entry.first == "CRVAL2" ||
                         entry.first == "CD1_1" || entry.first == "CD1_2" ||
                         entry.first == "CD2_1" || entry.first == "CD2_2";
    if (numeric) {
      bool card_fits = true;
      const std::string text = card(entry.first, numeric_value(std::stod(entry.second)), "", true,
                                    &card_fits);
      if (card_fits) header.push_back(text);
      else fits = false;
    } else {
      bool card_fits = true;
      const std::string text = card(entry.first, quoted_string(entry.second), "", false,
                                    &card_fits);
      if (card_fits) {
        header.push_back(text);
      } else if (!append_long_string(header, entry.first, entry.second)) {
        fits = false;
      }
    }
  }
  return fits;
}

// ---------------------------------------------------------------------------
// The FITS unit of a signal.
//
// BUNIT is an external FITS semantics field: it states the unit of the values,
// not the identity of the internal model that produced them.  The rules are:
//
//   * BUNIT never carries an internal identity, a detector id or an
//     application id such as "acsd.native:x01-00*scale/x01-scale".  A reader
//     outside this project cannot interpret it and an external tool that tries
//     to parse a unit would either reject the file or mis-parse the string.
//   * If the declared scale IS a unit ("L/sr", "ADU", "Jy", possibly behind a
//     fixture/provider prefix such as "fixture:L/sr"), that unit is written.
//   * Otherwise the physical dimension really known from the Unit contract is
//     written: the linear power (the signal of this pipeline is a detector
//     linear signal, so ADU for linear power one) and the inverse powers of sr,
//     pixel and nm.  A physical unit (Jy, L) needs a reference SED and is not
//     claimed here.
//
// The exact internal scale is kept next to BUNIT in ACSUNIT and, when BUNIT is
// not that unit, the non standard nature is stated explicitly in ACSSTD.  The
// unit is never presented as a standard one when it is not.
// ---------------------------------------------------------------------------
struct FitsUnitText {
  std::string bunit;    // the value of BUNIT
  std::string acsunit;  // the exact internal unit text (empty when trivial)
  bool nonstandard = false;
};

std::string lowercase_ascii(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return text;
}

// A standard unit token this writer can prove.  The list is explicit: a
// detector identity ("detector0") or an internal unit ("acsd.native:...") is
// not a unit and must not pass as one.  A physical unit (Jy, L) is accepted
// here because the caller declared it; this writer never invents it.
bool is_standard_unit_token(const std::string& text) {
  const std::string token = lowercase_ascii(text);
  static const char* const kUnits[] = {
      "1",        "1/sr",      "adu",      "counts",  "count",   "photon",   "photons",
      "e-",       "jy",        "jansky",   "mjy",     "ujy",     "mag",      "k",
      "kelvin",   "s",         "sec",      "second",  "nm",      "um",       "a",
      "angstrom", "sr",        "deg",      "l",       "l/sr",    "j/sr",
      "erg/s/cm2/hz", "erg/s/cm^2/hz", "w/m2/hz", "w/m2/nm", "adu/sr", "rayleigh"};
  for (const char* unit : kUnits) {
    if (token == unit) return true;
  }
  return false;
}

// The unit part of a declared scale: the text itself, or the part behind the
// provider/fixture prefix ("fixture:L/sr" -> "L/sr").  Empty when the scale is
// an internal identity.
std::string standard_unit_part(const std::string& scale) {
  if (is_standard_unit_token(scale)) return scale;
  const std::size_t colon = scale.rfind(':');
  if (colon != std::string::npos) {
    const std::string tail = scale.substr(colon + 1);
    if (is_standard_unit_token(tail)) return tail;
  }
  return std::string();
}

std::string linear_power_base(int linear_power) {
  if (linear_power == 0) return "1";
  if (linear_power == 1) return "ADU";
  return "ADU^" + std::to_string(linear_power);
}

void append_power_suffix(std::string& text, const char* name, int power) {
  if (power == 0) return;
  text += "/" + std::string(name);
  if (power != -1) text += "^" + std::to_string(-power);
}

FitsUnitText fits_unit_text(const Unit& unit) {
  FitsUnitText out;
  out.bunit = linear_power_base(unit.linear_power);
  append_power_suffix(out.bunit, "SR", unit.sr_power);
  append_power_suffix(out.bunit, "PIXEL", unit.pixel_power);
  append_power_suffix(out.bunit, "NM", unit.nm_power);
  if (unit.scale.empty() || unit.scale == "1") return out;
  const std::string exact = model::unit_to_string(unit);
  const std::string declared = standard_unit_part(unit.scale);
  if (!declared.empty()) {
    // The scale is a unit (possibly behind a provider prefix): write it.
    out.bunit = declared;
    if (unit.sr_power < 0) {
      const std::string lower = lowercase_ascii(declared);
      const bool has_sr = lower.find("/sr") != std::string::npos ||
                          lower.find("sr^-") != std::string::npos;
      if (!has_sr) out.bunit += "/SR";
    }
    if (declared != unit.scale) out.acsunit = exact;
    return out;
  }
  out.acsunit = exact;
  out.nonstandard = true;
  return out;
}

// The same decision when only the unit scale text is available (the HiPS tile
// writer receives the identity string, not the Unit).  The "/sr" suffix is
// still recovered from the text because the unit multiplication of this
// pipeline records it there.
FitsUnitText fits_unit_text_from_scale(const std::string& scale) {
  FitsUnitText out;
  if (scale.empty() || scale == "1") {
    out.bunit = "1";
    return out;
  }
  const std::string declared = standard_unit_part(scale);
  if (!declared.empty()) {
    out.bunit = declared;
    if (declared != scale) out.acsunit = scale;
    return out;
  }
  const bool per_sr = scale.find("/sr") != std::string::npos ||
                      scale.find("sr^-1") != std::string::npos;
  out.bunit = per_sr ? "ADU/SR" : "ADU";
  out.acsunit = scale;
  out.nonstandard = true;
  return out;
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
  if (!append_header(cards, header)) {
    return fail<std::string>(Code::invalid_model,
                             "a FITS header card does not fit in 80 columns",
                             "workflows.fits", "the product cannot be published");
  }
  const std::string block = header_block(header);
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return fail<std::string>(Code::io_error, "the FITS file cannot be opened", "workflows.fits",
                             "the product cannot be published");
  }
  stream.write(block.data(), static_cast<std::streamsize>(block.size()));
  // The data unit is streamed in bounded chunks: a 4096x4096 image is 128 MiB
  // and the product writer must not hold a second full copy.
  constexpr std::size_t kChunk = 8192;
  std::vector<unsigned char> buffer(kChunk * 8);
  std::size_t offset = 0;
  while (offset < values.size()) {
    const std::size_t count = std::min(kChunk, values.size() - offset);
    for (std::size_t i = 0; i < count; ++i) {
      std::uint64_t bits = 0;
      std::memcpy(&bits, &values[offset + i], sizeof(bits));
      for (int byte = 0; byte < 8; ++byte) {
        buffer[i * 8 + byte] = static_cast<unsigned char>((bits >> (8 * (7 - byte))) & 0xffull);
      }
    }
    stream.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(count * 8));
    offset += count;
  }
  const std::size_t data_bytes = values.size() * 8;
  const std::size_t padding = (kBlock - ((block.size() + data_bytes) % kBlock)) % kBlock;
  if (padding != 0) {
    const std::vector<unsigned char> zeros(padding, 0);
    stream.write(reinterpret_cast<const char*>(zeros.data()),
                 static_cast<std::streamsize>(zeros.size()));
  }
  stream.flush();
  if (!stream) {
    return fail<std::string>(Code::io_error, "the FITS file cannot be written", "workflows.fits",
                             "the product cannot be published");
  }
  return ok_result(std::string());
}

// ---------------------------------------------------------------------------
// HiPS tiles
// ---------------------------------------------------------------------------
//
// The published tile layer keeps the standard NESTED identity of the signal
// index: tile T = ipix >> (2*tile_log2_width), the tile-local (ix,iy) are the
// standard LSB-first interleave of the leaf index (the same bit order as
// coordinates::healpix_face_index), and only tiles that actually contain a
// supported leaf are written.  One 64x64 image per tile, one tile per file.
// The old writer enumerated all 12*4^(order-tile_width) tiles, packed 10000
// planes per file and used a bit-reversed NESTED index: it produced a 6 GiB
// product whose sky positions were 27-32 degrees away from the signal.
Result<std::string> write_hips_order(const std::filesystem::path& root, U32 order,
                                      U32 tile_log2_width, const std::vector<HealpixLeaf>& leaves,
                                      const std::vector<double>& values,
                                      const std::vector<std::uint8_t>& states,
                                      const Unit& unit) {
  // The tiles carry the same physical unit as the product; the former call
  // passed only the declared scale string ("1" after a mosaic, a native
  // identity for a frame), so every tile was written with BUNIT=1 even when
  // the signal is ADU/SR.
  const FitsUnitText tile_unit = fits_unit_text(unit);
  const std::string& bunit = tile_unit.bunit;
  if (tile_log2_width < 6 || tile_log2_width > 9) {
    return fail<std::string>(Code::invalid_request, "the tile width is outside the allowed range",
                             "workflows.hips", "the product cannot be published");
  }
  if (order < tile_log2_width) {
    return fail<std::string>(Code::invalid_request, "the leaf order is below the tile width",
                             "workflows.hips", "the product cannot be published");
  }
  if (leaves.size() != values.size() || (!states.empty() && states.size() != leaves.size())) {
    return fail<std::string>(Code::invalid_model, "the signal and support sizes differ",
                             "workflows.hips", "the product cannot be published");
  }
  const U32 tile_order = order - tile_log2_width;
  const U64 tile_width = 1ull << tile_log2_width;
  const U64 nside = 1ull << order;
  const U64 nside_tile = 1ull << tile_order;
  const U64 npix = 12ull * nside * nside;

  // Group the supported leaves by tile identity.  A supported leaf with a
  // non-finite signal is a product defect: the tile layer never hides it.
  std::map<U64, std::vector<std::pair<U64, double>>> tiles;
  std::set<U64> seen;
  for (std::size_t i = 0; i < leaves.size(); ++i) {
    if (leaves[i].order != order || leaves[i].ipix >= npix) {
      return fail<std::string>(Code::invalid_model, "a leaf is outside the published index",
                               "workflows.hips", "the product cannot be published");
    }
    if (!seen.insert(leaves[i].ipix).second) {
      return fail<std::string>(Code::invalid_model, "a leaf appears twice in the signal index",
                               "workflows.hips", "the product cannot be published");
    }
    const bool supported = states.empty() || states[i] == 1;
    if (!supported) continue;
    if (!std::isfinite(values[i])) {
      return fail<std::string>(Code::invalid_model,
                               "a supported leaf carries no finite signal", "workflows.hips",
                               "the product cannot be published");
    }
    const U64 tile = leaves[i].ipix >> (2 * tile_log2_width);
    const U64 local = leaves[i].ipix % (nside_tile * nside_tile);
    U64 ix = 0;
    U64 iy = 0;
    for (U32 bit = 0; bit < tile_order; ++bit) {
      if ((local >> (2 * bit)) & 1ull) ix |= 1ull << bit;
      if ((local >> (2 * bit + 1)) & 1ull) iy |= 1ull << bit;
    }
    const U64 dx = ix % tile_width;
    const U64 dy = iy % tile_width;
    // Standard HiPS tile orientation: row = W-1-x, col = y.  The verified
    // HPX header below reproduces every tile pixel centre to <0.01 arcsec.
    const U64 pixel = (tile_width - 1 - dx) * tile_width + dy;
    tiles[tile].emplace_back(pixel, values[i]);
  }
  if (tiles.empty()) {
    return fail<std::string>(Code::no_support, "no supported leaf to publish", "workflows.hips",
                             "the product cannot be published");
  }

  const FitsUnitText unit_text = fits_unit_text(unit);
  std::vector<double> plane(tile_width * tile_width, std::nan(""));
  for (const auto& entry : tiles) {
    const U64 tile = entry.first;
    const U64 face = tile / (nside_tile * nside_tile);
    const U64 local_tile = tile % (nside_tile * nside_tile);
    if (face < 4 || face > 7) {
      // Only the four equatorial faces share the flat HPX chart.  The polar
      // faces are diamonds in the HPX plane; claiming the equatorial formula
      // there would place the tile at the wrong sky position.  Refuse instead.
      return fail<std::string>(Code::unsupported,
                               "a supported leaf lies on a polar HPX face", "workflows.hips",
                               "the tile layer cannot be published for this footprint");
    }
    U64 tile_ix = 0;
    U64 tile_iy = 0;
    for (U32 bit = 0; bit < tile_order; ++bit) {
      if ((local_tile >> (2 * bit)) & 1ull) tile_ix |= 1ull << bit;
      if ((local_tile >> (2 * bit + 1)) & 1ull) tile_iy |= 1ull << bit;
    }
    std::fill(plane.begin(), plane.end(), std::nan(""));
    for (const auto& pixel : entry.second) plane[pixel.first] = pixel.second;
    // The HPX reference of the equatorial face: lon = 90*(face-4), lat = 0.
    const double centre_lon = 90.0 * static_cast<double>(face - 4);
    // Signed arithmetic: tile_iy may exceed tile_ix, and the U64 subtraction
    // would wrap to 2^64 instead of a negative offset.
    const double a = (static_cast<double>(tile_ix) - static_cast<double>(tile_iy)) *
                         static_cast<double>(tile_width) +
                     static_cast<double>(tile_width) + 1.0;
    const double b = (static_cast<double>(tile_ix) + static_cast<double>(tile_iy)) *
                         static_cast<double>(tile_width) +
                     static_cast<double>(tile_width) - static_cast<double>(nside);
    const double crpix1 = 0.5 * (a - b);
    const double crpix2 = 0.5 * (a + b);
    const double cd = 45.0 / static_cast<double>(nside);
    std::vector<HeaderCard> cards;
    cards.emplace_back("CTYPE1", "RA---HPX");
    cards.emplace_back("CTYPE2", "DEC--HPX");
    cards.emplace_back("CUNIT1", "deg");
    cards.emplace_back("CUNIT2", "deg");
    cards.emplace_back("RADESYS", "ICRS");
    cards.emplace_back("CRVAL1", numeric_value(centre_lon));
    cards.emplace_back("CRVAL2", numeric_value(0.0));
    cards.emplace_back("CRPIX1", numeric_value(crpix1));
    cards.emplace_back("CRPIX2", numeric_value(crpix2));
    cards.emplace_back("CD1_1", numeric_value(-cd));
    cards.emplace_back("CD1_2", numeric_value(-cd));
    cards.emplace_back("CD2_1", numeric_value(cd));
    cards.emplace_back("CD2_2", numeric_value(-cd));
    cards.emplace_back("BUNIT", bunit);
    if (!tile_unit.acsunit.empty()) cards.emplace_back("ACSUNIT", tile_unit.acsunit);
    if (tile_unit.nonstandard) cards.emplace_back("ACSSTD", "nonstandard_model_unit");
    const std::filesystem::path directory =
        root / ("Norder" + std::to_string(order)) / ("Dir" + std::to_string(tile / 10000));
    const std::filesystem::path file = directory / ("Npix" + std::to_string(tile) + ".fits");
    Result<std::string> written =
        write_fits_image(file, tile_width, tile_width, plane, cards);
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
  const bool has_sip = wcs.sip_a.order > 0 || wcs.sip_b.order > 0;
  if (has_sip) {
    // The published image is the projection of the leaf signal through a
    // TAN+SIP model; a header without the SIP coefficients describes a
    // different (undistorted) sky and is off by up to ~1 arcsec at the field
    // corners.  The FITS SIP convention names the shifted coordinates u,v
    // (u = x - CRPIX1) with A_p_q for u^p v^q.
    if (ctype1.find("-SIP") == std::string::npos) ctype1 += "-SIP";
    if (ctype2.find("-SIP") == std::string::npos) ctype2 += "-SIP";
  }
  std::vector<HeaderCard> cards;
  cards.emplace_back("CTYPE1", ctype1);
  cards.emplace_back("CTYPE2", ctype2);
  cards.emplace_back("CUNIT1", "deg");
  cards.emplace_back("CUNIT2", "deg");
  cards.emplace_back("RADESYS", "ICRS");
  cards.emplace_back("CRVAL1", numeric_value(ra));
  cards.emplace_back("CRVAL2", numeric_value(dec));
  // The reference pixel is the one of the target model: the projection built
  // the image with u = x + 1 - CRPIX, so the header must state the same CRPIX.
  // Recentring the header on the image centre while the data were projected
  // around the model CRPIX would shift the WCS by the difference (the former
  // code did exactly that).
  cards.emplace_back("CRPIX1", numeric_value(wcs.crpix_fits1[0]));
  cards.emplace_back("CRPIX2", numeric_value(wcs.crpix_fits1[1]));
  cards.emplace_back("CD1_1", numeric_value(wcs.cd_rad_per_pixel[0] * radians_to_degrees));
  cards.emplace_back("CD1_2", numeric_value(wcs.cd_rad_per_pixel[1] * radians_to_degrees));
  cards.emplace_back("CD2_1", numeric_value(wcs.cd_rad_per_pixel[2] * radians_to_degrees));
  cards.emplace_back("CD2_2", numeric_value(wcs.cd_rad_per_pixel[3] * radians_to_degrees));
  if (has_sip) {
    cards.emplace_back("A_ORDER", numeric_value(static_cast<double>(wcs.sip_a.order)));
    for (const Term2& term : wcs.sip_a.terms) {
      cards.emplace_back("A_" + std::to_string(term.x_power) + "_" +
                             std::to_string(term.y_power),
                         numeric_value(term.coefficient));
    }
    cards.emplace_back("B_ORDER", numeric_value(static_cast<double>(wcs.sip_b.order)));
    for (const Term2& term : wcs.sip_b.terms) {
      cards.emplace_back("B_" + std::to_string(term.x_power) + "_" +
                             std::to_string(term.y_power),
                         numeric_value(term.coefficient));
    }
  }
  // BUNIT is the physical dimension of the signal and never the internal model
  // identity.  The exact unit (with the applied photometric scale) is kept in
  // ACSUNIT and the fact that it is not a standard FITS unit is stated in
  // ACSSTD, so the unit is not disguised as a standard one (defect P1-6).
  const FitsUnitText unit_text = fits_unit_text(image.estimate->unit);
  cards.emplace_back("BUNIT", unit_text.bunit);
  if (!unit_text.acsunit.empty()) cards.emplace_back("ACSUNIT", unit_text.acsunit);
  if (unit_text.nonstandard) cards.emplace_back("ACSSTD", "nonstandard_model_unit");
  return write_fits_image(root / "signal.fits", width, height, values, cards);
}

Result<std::string> write_hips_properties(const std::filesystem::path& root, U32 order,
                                          U32 tile_log2_width, const Unit& unit) {
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
  // hips_unit is external metadata as well: it carries the same standard
  // dimension, not the internal identity.  The exact unit and the explicit non
  // standard flag stay in the acsunit/acsunit_standard keys.
  const FitsUnitText unit_text = fits_unit_text(unit);
  if (!unit_text.bunit.empty()) text << "hips_unit=" << unit_text.bunit << "\n";
  if (!unit_text.acsunit.empty()) {
    text << "acsunit=" << unit_text.acsunit << "\n";
    text << "acsunit_standard=" << (unit_text.nonstandard ? "false" : "true") << "\n";
  }
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
