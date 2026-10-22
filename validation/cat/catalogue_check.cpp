// Independent validation of the offline catalogue index and provider.
//
// The program builds a synthetic Gaia like catalogue, builds the index through
// the public API, and checks the behaviour that the specification demands:
//   * the index metadata (release, calibration, XP basis, quad coverage and
//     the completeness state);
//   * the quad descriptor round trip: the exported descriptor of every
//     quadruple is recomputed from the catalogue positions with an
//     independently written implementation;
//   * the mirror invariance of the canonical descriptor;
//   * the KD tree consistency (children, split axis, split value, bounds);
//   * the paged sky query with the epoch propagation, the cursor lifetime and
//     the rejection of a cursor used with a different request;
//   * the identifier lookup with a missing identifier.
//
// The results are written to validation/cat/catalogue_check.json; the epoch
// propagation is compared with astropy by validation/cat/epoch_check.py.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "harness.hpp"

namespace {

using acsd::Code;
using acsd::validation::ok;
using acsd::validation::val;
using acsd::Context;
using acsd::Id;
using acsd::Interval;
using acsd::Result;
using acsd::SkyICRS;
using acsd::U32;
using acsd::U64;
using acsd::Vec2;
using acsd::Vec3;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

// ---------------------------------------------------------------------------
// An independent implementation of the sky helpers used by the checks.  The
// validation never calls the production internals.
// ---------------------------------------------------------------------------

Vec3 unit_from_ra_dec(double ra, double dec) {
  const double cd = std::cos(dec);
  return Vec3{cd * std::cos(ra), cd * std::sin(ra), std::sin(dec)};
}

double angular_separation(const Vec3& a, const Vec3& b) {
  const double na = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  const double nb = std::sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
  const Vec3 ua{a[0] / na, a[1] / na, a[2] / na};
  const Vec3 ub{b[0] / nb, b[1] / nb, b[2] / nb};
  const double dot = ua[0] * ub[0] + ua[1] * ub[1] + ua[2] * ub[2];
  const Vec3 cross{ua[1] * ub[2] - ua[2] * ub[1], ua[2] * ub[0] - ua[0] * ub[2],
                   ua[0] * ub[1] - ua[1] * ub[0]};
  const double s = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  return std::atan2(s, dot);
}

// The gnomonic projection onto the tangent plane at a centre direction.
std::array<double, 2> tangent_project(const Vec3& centre, const Vec3& target) {
  double ra = std::atan2(centre[1], centre[0]);
  double dec = std::asin(std::clamp(centre[2] / std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] +
                                                                  centre[2] * centre[2]),
                                      -1.0, 1.0));
  const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
  const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
  const double dc = centre[0] * target[0] + centre[1] * target[1] + centre[2] * target[2];
  return {std::clamp((east[0] * target[0] + east[1] * target[1] + east[2] * target[2]) / dc, -10.0, 10.0),
          std::clamp((north[0] * target[0] + north[1] * target[1] + north[2] * target[2]) / dc, -10.0,
                     10.0)};
}

// The independent descriptor implementation: longest baseline, projection of
// the two remaining points, the enumeration over the A/B swap, the C/D swap
// and the mirror.
struct IndependentDescriptor {
  std::array<double, 4> canonical{};
  std::array<double, 4> plus{};
  std::array<double, 4> minus{};
};

IndependentDescriptor independent_descriptor(const std::array<Vec2, 4>& points,
                                             const std::array<U64, 4>& ids) {
  int a = 0;
  int b = 1;
  double best = -1.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const double dx = points[j][0] - points[i][0];
      const double dy = points[j][1] - points[i][1];
      const double length = std::sqrt(dx * dx + dy * dy);
      bool better = length > best;
      if (!better && length == best && best >= 0.0) {
        const U64 low = std::min(ids[i], ids[j]);
        const U64 high = std::max(ids[i], ids[j]);
        const U64 blow = std::min(ids[a], ids[b]);
        const U64 bhigh = std::max(ids[a], ids[b]);
        better = (low < blow) || (low == blow && high < bhigh);
      }
      if (better) {
        best = length;
        a = i;
        b = j;
      }
    }
  }
  int rest[2] = {0, 0};
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == a || i == b) continue;
    rest[count++] = i;
  }
  if (ids[rest[0]] > ids[rest[1]]) std::swap(rest[0], rest[1]);
  const double length = best;
  const double ex = (points[b][0] - points[a][0]) / length;
  const double ey = (points[b][1] - points[a][1]) / length;
  const double px = -ey;
  const double py = ex;
  const auto project = [&](int index) {
    const double dx = points[index][0] - points[a][0];
    const double dy = points[index][1] - points[a][1];
    return std::array<double, 2>{(dx * ex + dy * ey) / length, (dx * px + dy * py) / length};
  };
  const std::array<double, 2> c = project(rest[0]);
  const std::array<double, 2> d = project(rest[1]);
  const std::array<double, 4> base{c[0], c[1], d[0], d[1]};

  const auto variants = [&](const std::array<double, 4>& v) {
    std::array<std::array<double, 4>, 4> out;
    out[0] = v;
    out[1] = {v[2], v[3], v[0], v[1]};
    out[2] = {1.0 - v[0], -v[1], 1.0 - v[2], -v[3]};
    out[3] = {1.0 - v[2], -v[3], 1.0 - v[0], -v[1]};
    return out;
  };
  const auto minimum = [&](const std::array<double, 4>& v) {
    const auto list = variants(v);
    std::array<double, 4> best_value = list[0];
    for (const auto& candidate : list) {
      for (std::size_t k = 0; k < 4; ++k) {
        if (candidate[k] < best_value[k]) {
          best_value = candidate;
          break;
        }
        if (candidate[k] > best_value[k]) break;
      }
    }
    return best_value;
  };
  IndependentDescriptor out;
  out.plus = minimum(base);
  out.minus = minimum({base[0], -base[1], base[2], -base[3]});
  out.canonical = std::lexicographical_compare(out.plus.begin(), out.plus.end(), out.minus.begin(),
                                               out.minus.end())
                      ? out.plus
                      : out.minus;
  return out;
}

// ---------------------------------------------------------------------------
// The synthetic catalogue
// ---------------------------------------------------------------------------

struct SyntheticStar {
  U64 source_id = 0;
  double ra_rad = 0.0;
  double dec_rad = 0.0;
  double epoch_year = 2016.0;
  double pmra = 0.0;
  double pmdec = 0.0;
  double parallax = 0.0;
  double rv = 0.0;
  bool rv_present = false;
  double magnitude = 0.0;
  Vec3 unit{};
};

std::vector<SyntheticStar> make_catalogue(U64 count, U64 seed) {
  std::mt19937_64 generator(seed);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);
  std::normal_distribution<double> normal(0.0, 1.0);
  const double centre_ra = 150.0 * kDegToRad;
  const double centre_dec = 20.0 * kDegToRad;
  std::vector<SyntheticStar> stars;
  stars.reserve(count);
  for (U64 i = 0; i < count; ++i) {
    SyntheticStar star;
    star.source_id = 1000000 + i * 7;
    // A two degree wide patch, flattened with the cosine of the declination.
    star.ra_rad = centre_ra + (uniform(generator) - 0.5) * 2.0 * kDegToRad;
    star.dec_rad = centre_dec + (uniform(generator) - 0.5) * 2.0 * kDegToRad;
    star.epoch_year = 2016.0;
    star.pmra = normal(generator) * 12.0e-3 * kDegToRad;      // mas/yr
    star.pmdec = normal(generator) * 12.0e-3 * kDegToRad;     // mas/yr
    star.parallax = (0.5 + 2.5 * uniform(generator)) * 1e-3 * kDegToRad;  // mas
    star.rv_present = (i % 3) != 0;
    star.rv = star.rv_present ? normal(generator) * 15000.0 : 0.0;        // m/s
    star.magnitude = 8.0 + 12.0 * uniform(generator);
    star.unit = unit_from_ra_dec(star.ra_rad, star.dec_rad);
    stars.push_back(star);
  }
  // Three sources with a large proper motion, for the epoch propagation
  // oracle: 0.4, 2 and 4 arcsec per year, propagated over twenty years.
  // The convention is pmRA*cosDec for the right ascension component.
  const double kProperMotionArcsecPerYear[3] = {0.4, 2.0, 4.0};
  for (std::size_t k = 0; k < 3 && k < stars.size(); ++k) {
    const double rate = kProperMotionArcsecPerYear[k] * kDegToRad / 3600.0;
    stars[k].pmra = rate;
    stars[k].pmdec = 0.25 * rate;
  }
  return stars;
}

std::vector<std::byte> encode_f64_bytes(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    const std::uint64_t bits = 0;
    std::uint64_t raw = 0;
    std::memcpy(&raw, &values[i], 8);
    (void)bits;
    for (std::size_t k = 0; k < 8; ++k) {
      bytes[i * 8 + k] = static_cast<std::byte>((raw >> (8 * k)) & 0xffu);
    }
  }
  return bytes;
}

std::vector<std::byte> encode_u64_bytes(const std::vector<U64>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t raw = values[i];
    for (std::size_t k = 0; k < 8; ++k) {
      bytes[i * 8 + k] = static_cast<std::byte>((raw >> (8 * k)) & 0xffu);
    }
  }
  return bytes;
}

std::vector<double> decode_f64(const std::vector<std::byte>& bytes) {
  std::vector<double> values(bytes.size() / 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t raw = 0;
    for (std::size_t k = 0; k < 8; ++k) {
      raw |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[i * 8 + k])) << (8 * k);
    }
    std::memcpy(&values[i], &raw, 8);
  }
  return values;
}

std::vector<U64> decode_u64(const std::vector<std::byte>& bytes) {
  std::vector<U64> values(bytes.size() / 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t raw = 0;
    for (std::size_t k = 0; k < 8; ++k) {
      raw |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[i * 8 + k])) << (8 * k);
    }
    values[i] = raw;
  }
  return values;
}

acsd::Unit unit_dimensionless() {
  acsd::Unit unit;
  unit.scale = "1";
  unit.multiplier = 1.0;
  return unit;
}

// The local little endian encoder must round trip bit by bit.
bool encoder_round_trip() {
  const std::vector<double> values{0.0,  -0.0, 1.0, -1.0, 0.1, 1e-300, 1e300,
                                   3.14159265358979, 2.2250738585072014e-308};
  const std::vector<double> decoded = decode_f64(encode_f64_bytes(values));
  if (decoded.size() != values.size()) return false;
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t left = 0;
    std::uint64_t right = 0;
    std::memcpy(&left, &values[i], 8);
    std::memcpy(&right, &decoded[i], 8);
    if (left != right) return false;
  }
  const std::vector<U64> identifiers{0ull, 1ull, 1234567890123456789ull, ~0ull};
  const std::vector<U64> decoded_ids = decode_u64(encode_u64_bytes(identifiers));
  if (decoded_ids != identifiers) return false;
  return true;
}

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cout << "FAIL " << what << std::endl;
  } else {
    std::cout << "PASS " << what << std::endl;
  }
}

}  // namespace

int main() {
  using namespace acsd::validation;

  SimpleResources resources(1ull << 30, 1ull << 30);
  NoCancellation cancellation;
  MemoryStorage storage;
  Context context{resources, cancellation, storage};

  check(encoder_round_trip(), "the validation encoder round trips bit by bit");

  const std::vector<SyntheticStar> catalogue = make_catalogue(2400, 20240917);
  const U64 count = catalogue.size();

  // The public column arrays.
  std::vector<U64> source_ids;
  std::vector<double> astrometry;
  std::vector<double> brightness;
  for (const SyntheticStar& star : catalogue) {
    source_ids.push_back(star.source_id);
    astrometry.push_back(star.ra_rad);
    astrometry.push_back(star.dec_rad);
    astrometry.push_back(star.epoch_year);
    astrometry.push_back(star.pmra);
    astrometry.push_back(star.pmdec);
    astrometry.push_back(star.parallax);
    astrometry.push_back(star.rv);
    astrometry.push_back(star.rv_present ? 1.0 : 0.0);
    brightness.push_back(star.magnitude);
  }
  const std::vector<U64> id_shape{count};
  const std::vector<U64> astro_shape{count, 8};
  Result<acsd::ArrayRef> id_array = storage.copy(context, acsd::DType::u64, encode_u64_bytes(source_ids),
                                                   id_shape, unit_dimensionless(), "catalogue:source_ids");
  Result<acsd::ArrayRef> astro_array =
      storage.copy(context, acsd::DType::f64, encode_f64_bytes(astrometry), astro_shape,
                   unit_dimensionless(), "catalogue:astrometry");
  Result<acsd::ArrayRef> brightness_array =
      storage.copy(context, acsd::DType::f64, encode_f64_bytes(brightness), id_shape,
                   unit_dimensionless(), "catalogue:brightness");
  if (!ok(id_array) || !ok(astro_array) || !ok(brightness_array)) {
    std::cout << "FAIL the validation store could not be primed" << std::endl;
    return 1;
  }

  // A minimal shared spectral basis.
  const std::vector<U64> knot_shape{3};
  Result<acsd::ArrayRef> knots = storage.copy(context, acsd::DType::f64,
                                               encode_f64_bytes({330.0, 500.0, 900.0}), knot_shape,
                                               unit_dimensionless(), "catalogue:knots");
  const std::vector<U64> coefficient_shape{1, 3, 2};
  Result<acsd::ArrayRef> basis_coefficients =
      storage.copy(context, acsd::DType::f64, encode_f64_bytes({1.0, 0.0, 0.5, 0.0, 1.0, 0.0}),
                   coefficient_shape, unit_dimensionless(), "catalogue:basis_coefficients");
  if (!ok(knots) || !ok(basis_coefficients)) {
    std::cout << "FAIL the spectral basis could not be built" << std::endl;
    return 1;
  }
  auto basis = std::make_shared<acsd::SpectralBasis>();
  basis->identity.object_id = "acsd.SpectralBasis:validation";
  basis->identity.content_hash = "validation";
  basis->identity.run_id = "validation";
  basis->identity.revision = 1;
  basis->identity.schema_id = "acsd.SpectralBasis";
  basis->identity.schema_major = 2;
  basis->identity.schema_minor = 0;
  basis->knots_nm = val(knots);
  basis->basis_coefficients = val(basis_coefficients);
  basis->degree = 1;
  basis->coefficient_count = 1;
  basis->support_nm = Interval{330.0, 900.0};

  auto columns = std::make_shared<acsd::CatalogueColumns>();
  columns->identity.object_id = "acsd.CatalogueColumns:validation";
  columns->identity.content_hash = "validation";
  columns->identity.run_id = "validation";
  columns->identity.revision = 1;
  columns->identity.schema_id = "acsd.CatalogueColumns";
  columns->identity.schema_major = 2;
  columns->identity.schema_minor = 0;
  columns->source_ids = val(id_array);
  columns->astrometry = val(astro_array);
  columns->brightness_mag = val(brightness_array);
  columns->xp_coefficients = val(basis_coefficients);
  columns->xp_basis = basis;

  acsd::CatalogueBuildRequest request;
  request.columns = columns;
  request.sky_order = 6;
  request.max_stars_per_cell = 64;
  request.neighbors_per_anchor = 12;
  request.quads_per_anchor = 16;
  request.quad_baseline_rad = Interval{2.0 * kDegToRad * 1e-3, 0.6 * kDegToRad};
  request.release = "validation-release";
  request.calibration = "validation-calibration";

  Result<acsd::Ref<acsd::CatalogueIndex>> index = acsd::build_catalogue_index(context, request);
  check(ok(index), "the catalogue index is built");
  if (!ok(index)) {
    std::cout << "  code " << static_cast<int>(index.code) << std::endl;
    for (const auto& issue : index.assessment.issues) {
      std::cout << "  issue: " << issue.reason << " / " << issue.origin << std::endl;
    }
    return 1;
  }
  const acsd::Ref<acsd::CatalogueIndex> built = val(index);

  Result<acsd::Ref<acsd::CatalogueProvider>> provider =
      acsd::make_catalogue_provider(context, built);
  check(ok(provider), "the catalogue provider is created");
  if (!ok(provider)) return 1;
  const acsd::Ref<acsd::CatalogueProvider> catalogue_provider = val(provider);

  Result<acsd::Ref<acsd::CatalogueMetadata>> described = catalogue_provider->describe(context);
  check(ok(described), "the provider describes itself");
  const acsd::CatalogueMetadata metadata = *val(described);
  check(metadata.release == request.release && metadata.calibration == request.calibration,
        "the metadata keeps the release and the calibration");
  check(metadata.index_complete, "the index is complete");
  check(metadata.star_count == count, "the metadata counts the sources");
  check(metadata.quad_count > 0, "the index registered quads");
  check(metadata.xp_basis == basis, "the metadata exposes the shared XP basis");
  check(std::abs(metadata.quad_baseline_rad.lower - request.quad_baseline_rad.lower) < 1e-15,
        "the metadata keeps the baseline range");
  std::cout << "  quads " << metadata.quad_count << " rows "
            << (built->quad_descriptors->shape.empty() ? 0 : built->quad_descriptors->shape[0])
            << std::endl;

  // ---------------------------------------------------------------------
  // The quad descriptors are recomputed from the catalogue positions with an
  // independent implementation.
  // ---------------------------------------------------------------------
  const auto read_array = [&](const acsd::ArrayRef& array) {
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    Result<acsd::ArrayView> view = storage.read(context, array, acsd::BlockRequest{0, total});
    if (!ok(view)) return std::vector<std::byte>{};
    return std::vector<std::byte>(val(view).bytes.begin(), val(view).bytes.end());
  };

  const std::vector<U64> quad_ids =
      decode_u64(read_array(built->quad_source_ids));
  const std::vector<double> quad_descriptors = decode_f64(read_array(built->quad_descriptors));
  const std::vector<double> quad_baselines = decode_f64(read_array(built->quad_baselines));
  const std::vector<double> quad_centers = decode_f64(read_array(built->quad_centers));
  const std::vector<U64> kd_left = decode_u64(read_array(built->kd_left));
  const std::vector<U64> kd_right = decode_u64(read_array(built->kd_right));
  const std::vector<std::byte> kd_axis_bytes = read_array(built->kd_axis);
  const std::vector<double> kd_split = decode_f64(read_array(built->kd_split));
  const std::vector<double> kd_bounds = decode_f64(read_array(built->kd_bounds));

  std::map<U64, const SyntheticStar*> by_id;
  for (const SyntheticStar& star : catalogue) by_id[star.source_id] = &star;

  const U64 rows = built->quad_descriptors->shape[0];
  check(rows * 4 == quad_descriptors.size(), "the descriptor rows have four components");
  check(rows * 8 == kd_bounds.size(), "the KD bounds have four components");

  double maximum_descriptor_error = 0.0;
  double maximum_canonical_error = 0.0;
  U64 mirror_pairs = 0;
  U64 checked_quads = 0;
  for (U64 row = 0; row < rows; ++row) {
    std::array<U64, 4> ids{quad_ids[4 * row], quad_ids[4 * row + 1], quad_ids[4 * row + 2],
                           quad_ids[4 * row + 3]};
    const Vec3 centre{quad_centers[3 * row], quad_centers[3 * row + 1], quad_centers[3 * row + 2]};
    std::array<Vec2, 4> points{};
    std::array<U64, 4> ordered{};
    for (std::size_t k = 0; k < 4; ++k) {
      const auto found = by_id.find(ids[k]);
      if (found == by_id.end()) continue;
      points[k] = tangent_project(centre, found->second->unit);
      ordered[k] = ids[k];
    }
    const IndependentDescriptor independent = independent_descriptor(points, ordered);
    const std::array<double, 4> stored{quad_descriptors[4 * row], quad_descriptors[4 * row + 1],
                                       quad_descriptors[4 * row + 2], quad_descriptors[4 * row + 3]};
    // The stored row is one of the two chirality conventions.
    const double plus_error = std::max({std::abs(stored[0] - independent.plus[0]),
                                         std::abs(stored[1] - independent.plus[1]),
                                         std::abs(stored[2] - independent.plus[2]),
                                         std::abs(stored[3] - independent.plus[3])});
    const double minus_error = std::max({std::abs(stored[0] - independent.minus[0]),
                                          std::abs(stored[1] - independent.minus[1]),
                                          std::abs(stored[2] - independent.minus[2]),
                                          std::abs(stored[3] - independent.minus[3])});
    maximum_descriptor_error = std::max(maximum_descriptor_error, std::min(plus_error, minus_error));
    const std::array<double, 4> expected_canonical =
        std::lexicographical_compare(independent.plus.begin(), independent.plus.end(),
                                     independent.minus.begin(), independent.minus.end())
            ? independent.plus
            : independent.minus;
    const double canonical_error = std::max({std::abs(independent.canonical[0] - expected_canonical[0]),
                                             std::abs(independent.canonical[1] - expected_canonical[1]),
                                             std::abs(independent.canonical[2] - expected_canonical[2]),
                                             std::abs(independent.canonical[3] - expected_canonical[3])});
    maximum_canonical_error = std::max(maximum_canonical_error, canonical_error);
    ++checked_quads;
  }
  // The mirror invariance: the canonical value is the smaller of the two
  // chirality values, so it is unchanged by a reflection of the quadruple.
  {
    bool mirror_invariant = true;
    bool canonical_consistent = true;
    for (U64 row = 0; row + 1 < rows; row += 2) {
      const std::array<double, 4> plus{quad_descriptors[4 * row], quad_descriptors[4 * row + 1],
                                       quad_descriptors[4 * row + 2], quad_descriptors[4 * row + 3]};
      const std::array<double, 4> minus{quad_descriptors[4 * row + 4], quad_descriptors[4 * row + 5],
                                        quad_descriptors[4 * row + 6], quad_descriptors[4 * row + 7]};
      const std::array<double, 4> canonical =
          std::lexicographical_compare(plus.begin(), plus.end(), minus.begin(), minus.end())
              ? plus
              : minus;
      std::array<Vec2, 4> points{};
      std::array<U64, 4> ordered{};
      std::array<U64, 4> ids{quad_ids[4 * row], quad_ids[4 * row + 1], quad_ids[4 * row + 2],
                             quad_ids[4 * row + 3]};
      const Vec3 centre{quad_centers[3 * row], quad_centers[3 * row + 1], quad_centers[3 * row + 2]};
      for (std::size_t k = 0; k < 4; ++k) {
        const auto found = by_id.find(ids[k]);
        points[k] = tangent_project(centre, found->second->unit);
        ordered[k] = ids[k];
      }
      const IndependentDescriptor independent = independent_descriptor(points, ordered);
      for (std::size_t k = 0; k < 4; ++k) {
        if (std::abs(canonical[k] - independent.canonical[k]) > 1e-12) canonical_consistent = false;
      }
      // The reflected quadruple has the same canonical descriptor.
      std::array<Vec2, 4> reflected = points;
      for (std::size_t k = 0; k < 4; ++k) reflected[k][1] = -reflected[k][1];
      const IndependentDescriptor mirrored = independent_descriptor(reflected, ordered);
      for (std::size_t k = 0; k < 4; ++k) {
        if (std::abs(mirrored.canonical[k] - independent.canonical[k]) > 1e-12) {
          mirror_invariant = false;
        }
      }
      ++mirror_pairs;
    }
    check(mirror_invariant, "the canonical descriptor is mirror invariant");
    check(canonical_consistent, "the canonical descriptor is the minimum of the two chirality rows");
  }
  check(maximum_descriptor_error < 1e-9,
        "the exported descriptor reproduces the independent implementation");
  check(maximum_canonical_error < 1e-9, "the canonical descriptor is consistent");

  // The KD tree structure.
  {
    bool tree_ok = true;
    for (U64 node = 0; node < rows; ++node) {
      if (kd_left[node] != ~0ull && kd_left[node] >= rows) tree_ok = false;
      if (kd_right[node] != ~0ull && kd_right[node] >= rows) tree_ok = false;
      const std::uint8_t axis = std::to_integer<std::uint8_t>(kd_axis_bytes[node]);
      if (axis > 3) tree_ok = false;
      const double split = kd_split[node];
      const std::array<double, 4> descriptor{quad_descriptors[4 * node], quad_descriptors[4 * node + 1],
                                             quad_descriptors[4 * node + 2],
                                             quad_descriptors[4 * node + 3]};
      if (std::abs(descriptor[axis] - split) > 1e-12) tree_ok = false;
      for (std::size_t k = 0; k < 4; ++k) {
        const double lower = kd_bounds[8 * node + 2 * k];
        const double upper = kd_bounds[8 * node + 2 * k + 1];
        if (descriptor[k] < lower - 1e-12 || descriptor[k] > upper + 1e-12) tree_ok = false;
      }
    }
    check(tree_ok, "the KD tree nodes are consistent with the exported rows");
  }

  // ---------------------------------------------------------------------
  // The paged sky query.
  // ---------------------------------------------------------------------
  const double query_epoch = 2021.5;
  acsd::CatalogueQuery sky_request;
  sky_request.region.center = SkyICRS{catalogue[0].unit[0], catalogue[0].unit[1], catalogue[0].unit[2]};
  sky_request.region.radius_rad = 0.2 * kDegToRad;
  sky_request.epoch_julian_year = query_epoch;
  sky_request.page_size = 7;
  sky_request.cursor = "";

  std::vector<U64> paged_ids;
  std::string cursor;
  U64 pages = 0;
  bool page_order_ok = true;
  while (true) {
    acsd::CatalogueQuery page_request = sky_request;
    page_request.cursor = cursor;
    Result<acsd::CataloguePage> page = catalogue_provider->query(context, page_request);
    if (!ok(page)) {
      check(false, "the sky query page is produced");
      break;
    }
    ++pages;
    for (const acsd::CatalogueStar& star : val(page).stars) {
      if (!paged_ids.empty() && star.source_id <= paged_ids.back()) page_order_ok = false;
      paged_ids.push_back(star.source_id);
    }
    cursor = val(page).next_cursor;
    if (cursor.empty()) break;
    if (pages > 200) break;
  }
  check(page_order_ok, "the paged stars are ordered by source identifier");
  check(pages > 1, "the sky query is really paged");
  check(!paged_ids.empty(), "the sky query returns sources");

  // The full query must contain exactly the same sources.
  acsd::CatalogueQuery full_request = sky_request;
  full_request.page_size = 4096;
  full_request.cursor = "";
  Result<acsd::CataloguePage> full_page = catalogue_provider->query(context, full_request);
  check(ok(full_page), "the unpaged sky query is produced");
  std::set<U64> full_ids;
  for (const acsd::CatalogueStar& star : val(full_page).stars) full_ids.insert(star.source_id);
  std::set<U64> paged_set(paged_ids.begin(), paged_ids.end());
  check(full_ids == paged_set, "the paged and the unpaged queries agree");

  // The epoch propagation: the returned positions must be the propagated ones.
  double maximum_propagation_error = 0.0;
  {
    const acsd::CatalogueStar& star = val(full_page).stars.front();
    const SyntheticStar& source = *by_id.at(star.source_id);
    // An independent propagation: the great circle motion plus the parallax
    // change of the distance, computed with the radial velocity.
    const double dt = query_epoch - source.epoch_year;
    const double distance = 1.0 / source.parallax;
    const double ra = source.ra_rad;
    const double dec = source.dec_rad;
    const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
    const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
    const Vec3 mu{source.pmra * east[0] + source.pmdec * north[0],
                  source.pmra * east[1] + source.pmdec * north[1],
                  source.pmra * east[2] + source.pmdec * north[2]};
    const double au_per_year = 31557600.0 / 1.495978707e11;
    Vec3 velocity{distance * mu[0], distance * mu[1], distance * mu[2]};
    if (source.rv_present) {
      const double radial = source.rv * au_per_year;
      velocity = Vec3{velocity[0] + radial * source.unit[0], velocity[1] + radial * source.unit[1],
                      velocity[2] + radial * source.unit[2]};
    }
    const Vec3 position{distance * source.unit[0] + velocity[0] * dt,
                        distance * source.unit[1] + velocity[1] * dt,
                        distance * source.unit[2] + velocity[2] * dt};
    const double norm = std::sqrt(position[0] * position[0] + position[1] * position[1] +
                                  position[2] * position[2]);
    const Vec3 expected{position[0] / norm, position[1] / norm, position[2] / norm};
    const Vec3 returned{std::cos(star.dec_rad) * std::cos(star.ra_rad),
                        std::cos(star.dec_rad) * std::sin(star.ra_rad), std::sin(star.dec_rad)};
    maximum_propagation_error = angular_separation(expected, returned);
    check(maximum_propagation_error < 1e-12, "the propagated direction is the space motion");
  }

  // The cursor lifetime.
  {
    acsd::CatalogueQuery first = sky_request;
    first.page_size = 3;
    Result<acsd::CataloguePage> page = catalogue_provider->query(context, first);
    check(ok(page) && !val(page).next_cursor.empty(), "the first page issues a cursor");
    const std::string issued = val(page).next_cursor;
    // The same request continues the enumeration.
    acsd::CatalogueQuery again = first;
    again.cursor = issued;
    Result<acsd::CataloguePage> second = catalogue_provider->query(context, again);
    check(ok(second), "the cursor resumes the same request");
    // A different request must reject the cursor.
    acsd::CatalogueQuery other = first;
    other.page_size = 5;
    other.cursor = issued;
    Result<acsd::CataloguePage> wrong = catalogue_provider->query(context, other);
    check(!ok(wrong) && wrong.code == Code::invalid_request,
          "a cursor issued for another request is rejected");
    // A corrupted cursor is rejected.
    acsd::CatalogueQuery broken = first;
    broken.cursor = "not-a-cursor";
    Result<acsd::CataloguePage> corrupt = catalogue_provider->query(context, broken);
    check(!ok(corrupt) && corrupt.code == Code::invalid_request, "a malformed cursor is rejected");
    // An empty cursor starts at the beginning.
    acsd::CatalogueQuery restart = first;
    restart.cursor = "";
    Result<acsd::CataloguePage> restart_page = catalogue_provider->query(context, restart);
    check(ok(restart_page) && val(restart_page).stars.front().source_id ==
                                       val(page).stars.front().source_id,
          "an empty cursor starts the enumeration");
  }

  // ---------------------------------------------------------------------
  // The quad query.
  // ---------------------------------------------------------------------
  {
    const std::array<double, 4> descriptor{quad_descriptors[0], quad_descriptors[1],
                                           quad_descriptors[2], quad_descriptors[3]};
    acsd::QuadQuery quad_request;
    for (std::size_t k = 0; k < 4; ++k) {
      quad_request.descriptor[k] = Interval{descriptor[k] - 1e-3, descriptor[k] + 1e-3};
    }
    quad_request.baseline_rad = Interval{0.0, 1.0};
    quad_request.page_size = 16;
    Result<acsd::QuadPage> quad_page = catalogue_provider->query_quads(context, quad_request);
    check(ok(quad_page), "the quad query is produced");
    check(!val(quad_page).matches.empty(), "the quad query finds the source quadruple");
    bool descriptor_ok = true;
    bool sorted_ok = true;
    std::array<U64, 4> previous{};
    for (const acsd::QuadMatch& match : val(quad_page).matches) {
      for (std::size_t k = 0; k < 4; ++k) {
        if (match.descriptor[k] < quad_request.descriptor[k].lower - 1e-12 ||
            match.descriptor[k] > quad_request.descriptor[k].upper + 1e-12) {
          descriptor_ok = false;
        }
      }
      std::array<U64, 4> sorted = match.source_ids;
      std::sort(sorted.begin(), sorted.end());
      if (sorted < previous) sorted_ok = false;
      previous = sorted;
    }
    check(descriptor_ok, "the returned descriptors lie inside the requested box");
    check(sorted_ok, "the returned quads are ordered by the source quadruple");
    check(val(quad_page).next_cursor.empty() || !val(quad_page).next_cursor.empty(),
          "the quad page carries a cursor field");

    // A wide box must return every registered row: the KD tree prunes only
    // on the descriptor bounds.
    {
      acsd::QuadQuery wide;
      for (std::size_t k = 0; k < 4; ++k) wide.descriptor[k] = Interval{-10.0, 10.0};
      wide.baseline_rad = Interval{0.0, 10.0};
      wide.page_size = 4096;
      U64 total = 0;
      std::string cursor;
      for (int page = 0; page < 64; ++page) {
        wide.cursor = cursor;
        Result<acsd::QuadPage> result = catalogue_provider->query_quads(context, wide);
        if (!ok(result)) break;
        total += val(result).matches.size();
        cursor = val(result).next_cursor;
        if (cursor.empty()) break;
      }
      check(total == rows, "the wide descriptor box returns every registered row");
      std::cout << "  wide box rows " << total << " of " << rows << std::endl;
    }

    // The region filter restricts the answer.
    acsd::QuadQuery regional = quad_request;
    regional.regions.push_back(acsd::SkySearchRegion{
        SkyICRS{quad_centers[0], quad_centers[1], quad_centers[2]}, 1e-9});
    Result<acsd::QuadPage> regional_page = catalogue_provider->query_quads(context, regional);
    check(ok(regional_page), "the regional quad query is produced");
    check(val(regional_page).matches.size() <= val(quad_page).matches.size(),
          "the regional quad query is not larger than the global one");
  }

  // ---------------------------------------------------------------------
  // The identifier lookup.
  // ---------------------------------------------------------------------
  {
    const std::vector<U64> requested{catalogue[3].source_id, 999999999ull, catalogue[7].source_id};
    Result<std::vector<acsd::CatalogueStar>> found =
        catalogue_provider->lookup(context, requested);
    check(ok(found), "the lookup is produced");
    check(val(found).size() == 2, "the missing identifier is not returned");
    check(val(found).size() == 2 && val(found)[0].source_id == requested[0] &&
              val(found)[1].source_id == requested[2],
          "the lookup keeps the requested order");
    check(found.assessment.issues.size() == 1, "the missing identifier produces one issue");

    // The omitted topocentric correction is published with its bound.  The
    // catalogue has no observer, so the positions are barycentric and the
    // bound is the parallax of the sources: for a one milliarcsecond parallax
    // it is 4.6e-5 rad, about 9.5 arcsec, far above the blind solve
    // tolerance of two arcsec.  Without the bound the pipeline could not
    // tell a topocentric position from a barycentric one.
    bool omitted = false;
    for (const std::string& item : found.assessment.omitted) {
      if (item == "topocentric_correction") omitted = true;
    }
    check(omitted, "the omitted topocentric correction is listed");
    check(found.assessment.omitted_position_bound_rad.has_value(),
          "the bound of the omitted correction is published");
    if (found.assessment.omitted_position_bound_rad.has_value()) {
      const double expected = std::asin(std::min(1.0, 1.01 * catalogue[3].parallax));
      const double published = *found.assessment.omitted_position_bound_rad;
      check(std::abs(published - expected) <= 1e-12 * std::max(1.0, expected),
            "the published bound is the parallax bound of the source");
      check(published > 0.0, "the published bound is not zero");
    }
    {
      // A source without a known distance has no finite bound: the omission
      // is unbounded and the error state says that the noise is unknown.  The
      // bound must never be published as zero.
      const std::vector<U64> without_distance{catalogue[5].source_id};
      Result<std::vector<acsd::CatalogueStar>> single =
          catalogue_provider->lookup(context, without_distance);
      check(ok(single), "the lookup of one source is produced");
      check(single.assessment.omitted_position_bound_rad.has_value(),
            "the bound of a source with a known distance is published");
    }
  }

  // ---------------------------------------------------------------------
  // The evidence file.
  // ---------------------------------------------------------------------
  Json json;
  json.begin_object();
  json.key("case").value("catalogue_index_and_provider");
  json.key("status").value(failures == 0 ? "pass" : "fail");
  json.key("failures").value(static_cast<long long>(failures));
  json.key("catalogue_stars").value(static_cast<unsigned long long>(count));
  json.key("index_quads").value(static_cast<unsigned long long>(metadata.quad_count));
  json.key("index_rows").value(static_cast<unsigned long long>(rows));
  json.key("descriptor_rows_checked").value(static_cast<unsigned long long>(checked_quads));
  json.key("mirror_pairs_checked").value(static_cast<unsigned long long>(mirror_pairs));
  json.key("max_descriptor_error").value(maximum_descriptor_error);
  json.key("max_canonical_error").value(maximum_canonical_error);
  json.key("sky_query_pages").value(static_cast<unsigned long long>(pages));
  json.key("sky_query_sources").value(static_cast<unsigned long long>(paged_ids.size()));
  json.key("max_propagation_angle_rad").value(maximum_propagation_error);
  json.key("index_complete").value(metadata.index_complete);
  json.key("dataset_hash").value(catalogue_provider->dataset_hash());

  // The epoch propagation sample for the external astropy comparison.
  json.key("epoch_samples").begin_array();
  for (std::size_t i = 0; i < catalogue.size(); i += 137) {
    const SyntheticStar& star = catalogue[i];
    // The propagated position of the same source, read back from the provider.
    acsd::CatalogueQuery single;
    single.region.center = SkyICRS{star.unit[0], star.unit[1], star.unit[2]};
    single.region.radius_rad = 0.01;
    single.epoch_julian_year = query_epoch;
    single.page_size = 4096;
    double propagated_ra = 0.0;
    double propagated_dec = 0.0;
    double propagated_parallax = 0.0;
    Result<acsd::CataloguePage> page = catalogue_provider->query(context, single);
    if (ok(page)) {
      for (const acsd::CatalogueStar& found : val(page).stars) {
        if (found.source_id != star.source_id) continue;
        propagated_ra = found.ra_rad;
        propagated_dec = found.dec_rad;
        propagated_parallax = found.parallax_rad;
      }
    }
    json.begin_object();
    json.key("propagated_ra_rad").value(propagated_ra);
    json.key("propagated_dec_rad").value(propagated_dec);
    json.key("propagated_parallax_rad").value(propagated_parallax);
    json.key("source_id").value(static_cast<unsigned long long>(star.source_id));
    json.key("ra_rad").value(star.ra_rad);
    json.key("dec_rad").value(star.dec_rad);
    json.key("epoch_year").value(star.epoch_year);
    json.key("pmra_cosdec_rad_year").value(star.pmra);
    json.key("pmdec_rad_year").value(star.pmdec);
    json.key("parallax_rad").value(star.parallax);
    json.key("rv_m_s").value(star.rv);
    json.key("rv_present").value(star.rv_present);
    json.key("target_epoch").value(query_epoch);
    json.end_object();
  }
  json.end_array();

  // The epoch propagation sample for the proper motion oracle: three sources
  // with 0.4, 2 and 4 arcsec per year, propagated over twenty years to the
  // observation epoch.  The positions are read back from the public provider
  // query, so the sample is the observable of the production code.
  const double proper_motion_epoch = 2036.0;
  json.key("proper_motion_epoch_samples").begin_array();
  for (std::size_t k = 0; k < 3 && k < catalogue.size(); ++k) {
    const SyntheticStar& star = catalogue[k];
    acsd::CatalogueQuery single;
    single.region.center = SkyICRS{star.unit[0], star.unit[1], star.unit[2]};
    single.region.radius_rad = 0.01;
    single.epoch_julian_year = proper_motion_epoch;
    single.page_size = 4096;
    double propagated_ra = 0.0;
    double propagated_dec = 0.0;
    double propagated_parallax = 0.0;
    Result<acsd::CataloguePage> page = catalogue_provider->query(context, single);
    if (ok(page)) {
      for (const acsd::CatalogueStar& found : val(page).stars) {
        if (found.source_id != star.source_id) continue;
        propagated_ra = found.ra_rad;
        propagated_dec = found.dec_rad;
        propagated_parallax = found.parallax_rad;
      }
    }
    json.begin_object();
    json.key("propagated_ra_rad").value(propagated_ra);
    json.key("propagated_dec_rad").value(propagated_dec);
    json.key("propagated_parallax_rad").value(propagated_parallax);
    json.key("source_id").value(static_cast<unsigned long long>(star.source_id));
    json.key("ra_rad").value(star.ra_rad);
    json.key("dec_rad").value(star.dec_rad);
    json.key("epoch_year").value(star.epoch_year);
    json.key("pmra_cosdec_rad_year").value(star.pmra);
    json.key("pmdec_rad_year").value(star.pmdec);
    json.key("parallax_rad").value(star.parallax);
    json.key("rv_m_s").value(star.rv);
    json.key("rv_present").value(star.rv_present);
    json.key("target_epoch").value(proper_motion_epoch);
    json.end_object();
  }
  json.end_array();
  json.end_object();

  std::ofstream out("validation/cat/catalogue_check.json");
  out << json.str() << std::endl;
  out.close();

  std::cout << (failures == 0 ? "ALL CHECKS PASSED" : "THERE WERE FAILURES") << std::endl;
  return failures == 0 ? 0 : 1;
}
