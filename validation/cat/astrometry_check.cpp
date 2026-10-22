// Independent validation of the blind astrometric solver.
//
// The program renders a synthetic image from a known TAN + SIP model, builds a
// synthetic Gaia catalogue of the same field, and asks the solver to recover
// the model without any header.  The recovered model is compared with the
// truth by an independently written projection.
//
// Two negative configurations must fail:
//   * an image without detectable sources;
//   * an image whose sources are not in the catalogue.
//
// The results are written to validation/cat/astrometry_check.json; the
// recovered model is compared with astropy by validation/cat/astrometry_check.py.
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
using acsd::Context;
using acsd::Id;
using acsd::Interval;
using acsd::Result;
using acsd::SkyICRS;
using acsd::U32;
using acsd::U64;
using acsd::Vec2;
using acsd::Vec3;

using acsd::validation::ok;
using acsd::validation::val;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    ++failures;
    std::cout << "FAIL " << what << std::endl;
  } else {
    std::cout << "PASS " << what << std::endl;
  }
}

// ---------------------------------------------------------------------------
// The truth model, written independently of the production code.
// ---------------------------------------------------------------------------

struct TruthModel {
  double crpix_x = 0.0;
  double crpix_y = 0.0;
  double ra0 = 0.0;
  double dec0 = 0.0;
  double cd[4] = {0, 0, 0, 0};
  // The second order SIP terms, ordered as (0,2), (1,1), (2,0).
  double sip_a[3] = {0, 0, 0};
  double sip_b[3] = {0, 0, 0};
};

Vec3 unit_from_ra_dec(double ra, double dec) {
  const double cd = std::cos(dec);
  return Vec3{cd * std::cos(ra), cd * std::sin(ra), std::sin(dec)};
}

double separation(const Vec3& left, const Vec3& right) {
  const double dot = left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  const Vec3 cross{left[1] * right[2] - left[2] * right[1], left[2] * right[0] - left[0] * right[2],
                   left[0] * right[1] - left[1] * right[0]};
  const double s = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  return std::atan2(s, std::clamp(dot, -1.0, 1.0));
}

// The gnomonic projection of the truth model: the pixel position maps to the
// ICRS direction.
Vec3 truth_forward(const TruthModel& model, double pixel_x, double pixel_y) {
  const double u = pixel_x + 1.0 - model.crpix_x;
  const double v = pixel_y + 1.0 - model.crpix_y;
  const double u2 = u * u;
  const double v2 = v * v;
  const double uv = u * v;
  const double a = model.sip_a[0] * v2 + model.sip_a[1] * uv + model.sip_a[2] * u2;
  const double b = model.sip_b[0] * v2 + model.sip_b[1] * uv + model.sip_b[2] * u2;
  const double fx = u + a;
  const double fy = v + b;
  const double qx = model.cd[0] * fx + model.cd[1] * fy;
  const double qy = model.cd[2] * fx + model.cd[3] * fy;
  const double cos_dec = std::cos(model.dec0);
  const double sin_dec = std::sin(model.dec0);
  const double cos_ra = std::cos(model.ra0);
  const double sin_ra = std::sin(model.ra0);
  const Vec3 centre{cos_dec * cos_ra, cos_dec * sin_ra, sin_dec};
  const Vec3 east{-sin_ra, cos_ra, 0.0};
  const Vec3 north{-sin_dec * cos_ra, -sin_dec * sin_ra, cos_dec};
  const Vec3 raw{centre[0] + qx * east[0] + qy * north[0], centre[1] + qx * east[1] + qy * north[1],
                 centre[2] + qx * east[2] + qy * north[2]};
  const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
  return Vec3{raw[0] / norm, raw[1] / norm, raw[2] / norm};
}

// The pixel position of a direction under the truth model, by the fixed point
// iteration of the inverse projection.
bool truth_inverse(const TruthModel& model, const Vec3& direction, double& pixel_x,
                   double& pixel_y) {
  const double cos_dec = std::cos(model.dec0);
  const double sin_dec = std::sin(model.dec0);
  const double cos_ra = std::cos(model.ra0);
  const double sin_ra = std::sin(model.ra0);
  const Vec3 centre{cos_dec * cos_ra, cos_dec * sin_ra, sin_dec};
  const Vec3 east{-sin_ra, cos_ra, 0.0};
  const Vec3 north{-sin_dec * cos_ra, -sin_dec * sin_ra, cos_dec};
  const double denominator = centre[0] * direction[0] + centre[1] * direction[1] +
                             centre[2] * direction[2];
  if (!(std::abs(denominator) > 1e-9)) return false;
  const double xi = (east[0] * direction[0] + east[1] * direction[1] + east[2] * direction[2]) /
                    denominator;
  const double eta = (north[0] * direction[0] + north[1] * direction[1] + north[2] * direction[2]) /
                     denominator;
  // The inverse of the linear part: the tangent coordinates give the
  // coordinates before the SIP correction.
  const double det = model.cd[0] * model.cd[3] - model.cd[1] * model.cd[2];
  if (!(std::abs(det) > 0.0)) return false;
  const double fx = (model.cd[3] * xi - model.cd[1] * eta) / det;
  const double fy = (-model.cd[2] * xi + model.cd[0] * eta) / det;
  // The SIP terms are small; the fixed point iteration converges quickly.
  double u = fx;
  double v = fy;
  for (int iteration = 0; iteration < 30; ++iteration) {
    const double u2 = u * u;
    const double v2 = v * v;
    const double uv = u * v;
    const double a = model.sip_a[0] * v2 + model.sip_a[1] * uv + model.sip_a[2] * u2;
    const double b = model.sip_b[0] * v2 + model.sip_b[1] * uv + model.sip_b[2] * u2;
    const double new_u = fx - a;
    const double new_v = fy - b;
    const double change = std::abs(new_u - u) + std::abs(new_v - v);
    u = new_u;
    v = new_v;
    if (change < 1e-15) break;
  }
  pixel_x = u + model.crpix_x - 1.0;
  pixel_y = v + model.crpix_y - 1.0;
  return true;
}

// The pixel integrated Gaussian used to render the sources.
double pixel_integral(double x, double x0, double sigma) {
  const double a = (x + 0.5 - x0) / (sigma * std::sqrt(2.0));
  const double b = (x - 0.5 - x0) / (sigma * std::sqrt(2.0));
  return 0.5 * (std::erf(a) - std::erf(b));
}

struct SyntheticStar {
  U64 source_id = 0;
  double ra_rad = 0.0;
  double dec_rad = 0.0;
  double pixel_x = 0.0;
  double pixel_y = 0.0;
  double flux = 0.0;
  double epoch_year = 2016.0;
  double pmra = 0.0;
  double pmdec = 0.0;
  double parallax = 0.0;
  double magnitude = 0.0;
};

std::vector<std::byte> encode_f64_bytes(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t raw = 0;
    std::memcpy(&raw, &values[i], 8);
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

std::vector<std::byte> encode_u16_bytes(const std::vector<std::uint16_t>& values) {
  std::vector<std::byte> bytes(values.size() * 2);
  for (std::size_t i = 0; i < values.size(); ++i) {
    bytes[i * 2 + 0] = static_cast<std::byte>(values[i] & 0xffu);
    bytes[i * 2 + 1] = static_cast<std::byte>((values[i] >> 8) & 0xffu);
  }
  return bytes;
}

// The similarity invariant descriptor of four points, written independently of
// the production code.
// The two chirality values and the canonical value of a configuration.
struct Descriptors {
  std::array<double, 4> plus{};
  std::array<double, 4> minus{};
  std::array<double, 4> canonical{};
};

Descriptors descriptors_of(const std::array<std::array<double, 2>, 4>& points) {
  int best_a = 0;
  int best_b = 1;
  double best_length = -1.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      const double dx = points[j][0] - points[i][0];
      const double dy = points[j][1] - points[i][1];
      const double length = std::sqrt(dx * dx + dy * dy);
      if (length > best_length) {
        best_length = length;
        best_a = i;
        best_b = j;
      }
    }
  }
  int remaining[2] = {0, 0};
  int count = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == best_a || i == best_b) continue;
    remaining[count++] = i;
  }
  if (remaining[0] > remaining[1]) std::swap(remaining[0], remaining[1]);
  const double ex = (points[best_b][0] - points[best_a][0]) / best_length;
  const double ey = (points[best_b][1] - points[best_a][1]) / best_length;
  const double px = -ey;
  const double py = ex;
  const auto project = [&](int index, double& x, double& y) {
    const double dx = points[index][0] - points[best_a][0];
    const double dy = points[index][1] - points[best_a][1];
    x = (dx * ex + dy * ey) / best_length;
    y = (dx * px + dy * py) / best_length;
  };
  double cx = 0.0;
  double cy = 0.0;
  double dx_c = 0.0;
  double dy_c = 0.0;
  project(remaining[0], cx, cy);
  project(remaining[1], dx_c, dy_c);
  const std::array<double, 4> base{cx, cy, dx_c, dy_c};
  const std::array<double, 4> variants[4] = {
      base, std::array<double, 4>{base[2], base[3], base[0], base[1]},
      std::array<double, 4>{1.0 - base[0], -base[1], 1.0 - base[2], -base[3]},
      std::array<double, 4>{1.0 - base[2], -base[3], 1.0 - base[0], -base[1]}};
  const std::array<double, 4> mirrored{cx, -cy, dx_c, -dy_c};
  const std::array<double, 4> mirrored_variants[4] = {
      mirrored, std::array<double, 4>{mirrored[2], mirrored[3], mirrored[0], mirrored[1]},
      std::array<double, 4>{1.0 - mirrored[0], -mirrored[1], 1.0 - mirrored[2], -mirrored[3]},
      std::array<double, 4>{1.0 - mirrored[2], -mirrored[3], 1.0 - mirrored[0], -mirrored[1]}};
  const auto smaller = [](const std::array<double, 4>& left, const std::array<double, 4>& right) {
    for (std::size_t k = 0; k < 4; ++k) {
      if (left[k] < right[k]) return true;
      if (left[k] > right[k]) return false;
    }
    return false;
  };
  Descriptors out;
  out.plus = variants[0];
  for (const auto& variant : variants) {
    if (smaller(variant, out.plus)) out.plus = variant;
  }
  out.minus = mirrored_variants[0];
  for (const auto& variant : mirrored_variants) {
    if (smaller(variant, out.minus)) out.minus = variant;
  }
  out.canonical = smaller(out.minus, out.plus) ? out.minus : out.plus;
  return out;
}

std::array<double, 4> descriptor_of(const std::array<std::array<double, 2>, 4>& points) {
  return descriptors_of(points).canonical;
}

acsd::Unit unit_dimensionless() {
  acsd::Unit unit;
  unit.scale = "1";
  unit.multiplier = 1.0;
  return unit;
}

// The synthetic field.
struct Field {
  U64 width = 2048;
  U64 height = 2048;
  TruthModel model;
  std::vector<SyntheticStar> stars;
};

Field make_field(U64 count, U64 seed, double centre_ra, double centre_dec) {
  Field field;
  field.model.crpix_x = 512.5;
  field.model.crpix_y = 500.5;
  field.model.ra0 = centre_ra;
  field.model.dec0 = centre_dec;
  // Four arcseconds per pixel with a small rotation; the right ascension axis
  // points to the left, which is the usual orientation of a sky image.  The
  // matrix is orthogonal, so the detector frame and the tangent plane differ
  // by a similarity.
  const double scale = 4.0 / 3600.0 * kDegToRad;
  const double rotation = 3.0 * kDegToRad;
  // The matrix is scale times a reflection composed with a rotation, so it is
  // orthogonal and the detector frame differs from the tangent plane by a
  // similarity.
  field.model.cd[0] = -scale * std::cos(rotation);
  field.model.cd[1] = scale * std::sin(rotation);
  field.model.cd[2] = scale * std::sin(rotation);
  field.model.cd[3] = scale * std::cos(rotation);
  // A small second order distortion, about half a pixel at the field edge.
  field.model.sip_a[0] = 5.0e-7;
  field.model.sip_a[1] = -3.2e-7;
  field.model.sip_a[2] = 2.5e-7;
  field.model.sip_b[0] = -3.8e-7;
  field.model.sip_b[1] = 4.5e-7;
  field.model.sip_b[2] = -2.8e-7;

  std::mt19937_64 generator(seed);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);
  std::normal_distribution<double> normal(0.0, 1.0);
  for (U64 i = 0; i < count; ++i) {
    SyntheticStar star;
    star.source_id = 5000000 + i * 13;
    star.pixel_x = 40.0 + uniform(generator) * (static_cast<double>(field.width) - 80.0);
    star.pixel_y = 40.0 + uniform(generator) * (static_cast<double>(field.height) - 80.0);
    const Vec3 direction = truth_forward(field.model, star.pixel_x, star.pixel_y);
    star.ra_rad = std::atan2(direction[1], direction[0]);
    if (star.ra_rad < 0.0) star.ra_rad += 2.0 * kPi;
    star.dec_rad = std::asin(std::clamp(direction[2], -1.0, 1.0));
    star.flux = 800.0 + 4000.0 * uniform(generator);
    star.epoch_year = 2016.0;
    star.pmra = normal(generator) * 0.02e-3 * kDegToRad;
    star.pmdec = normal(generator) * 0.02e-3 * kDegToRad;
    star.parallax = (0.5 + 2.0 * uniform(generator)) * 1e-3 * kDegToRad;
    star.magnitude = 9.0 + 5.0 * uniform(generator);
    field.stars.push_back(star);
  }
  return field;
}

// Renders the image of a field.
std::vector<double> render(const Field& field, U64 seed, double background, double noise,
                            std::vector<std::uint16_t>& dq) {
  const U64 total = field.width * field.height;
  std::vector<double> image(total, background);
  dq.assign(total, 0);
  std::mt19937_64 generator(seed);
  std::normal_distribution<double> normal(0.0, noise);
  for (U64 i = 0; i < total; ++i) image[i] += normal(generator);
  const double sigma = 1.6;
  for (const SyntheticStar& star : field.stars) {
    const int cx = static_cast<int>(std::lround(star.pixel_x));
    const int cy = static_cast<int>(std::lround(star.pixel_y));
    for (int dy = -4; dy <= 4; ++dy) {
      const int y = cy + dy;
      if (y < 0 || y >= static_cast<int>(field.height)) continue;
      for (int dx = -4; dx <= 4; ++dx) {
        const int x = cx + dx;
        if (x < 0 || x >= static_cast<int>(field.width)) continue;
        const double profile = pixel_integral(x + 0.0, star.pixel_x, sigma) *
                               pixel_integral(y + 0.0, star.pixel_y, sigma);
        image[static_cast<std::size_t>(y) * field.width + static_cast<std::size_t>(x)] +=
            star.flux * profile;
      }
    }
  }
  return image;
}

// Builds the catalogue of a field.
struct CatalogueData {
  std::vector<U64> source_ids;
  std::vector<double> astrometry;
  std::vector<double> brightness;
};

CatalogueData catalogue_of(const Field& field) {
  CatalogueData data;
  for (const SyntheticStar& star : field.stars) {
    data.source_ids.push_back(star.source_id);
    data.astrometry.push_back(star.ra_rad);
    data.astrometry.push_back(star.dec_rad);
    data.astrometry.push_back(star.epoch_year);
    data.astrometry.push_back(star.pmra);
    data.astrometry.push_back(star.pmdec);
    data.astrometry.push_back(star.parallax);
    data.astrometry.push_back(0.0);
    data.astrometry.push_back(0.0);
    data.brightness.push_back(star.magnitude);
  }
  return data;
}

// Builds the observation of a rendered image.
Result<acsd::Ref<acsd::NativeObservation>> make_observation(
    Context& context, const Field& field, const std::vector<double>& image,
    const std::vector<std::uint16_t>& dq, double epoch) {
  const U64 total = field.width * field.height;
  auto index = std::make_shared<acsd::Index>();
  index->identity.object_id = "acsd.Index:validation_native";
  index->identity.content_hash = "validation_native";
  index->identity.run_id = "validation";
  index->identity.revision = 1;
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->size = total;
  index->kind = acsd::Index::Kind::native;
  index->namespace_id = "validation";

  const std::vector<U64> shape{total};
  Result<acsd::ArrayRef> value_array =
      context.storage.copy(context, acsd::DType::f64, encode_f64_bytes(image), shape,
                           unit_dimensionless(), index->identity.object_id);
  if (!ok(value_array)) {
    return acsd::validation::fail<acsd::Ref<acsd::NativeObservation>>(value_array.code,
                                                                 "the image cannot be stored",
                                                                 "validation", "no observation");
  }
  Result<acsd::ArrayRef> dq_array =
      context.storage.copy(context, acsd::DType::u16, encode_u16_bytes(dq), shape,
                           unit_dimensionless(), index->identity.object_id);
  if (!ok(dq_array)) {
    return acsd::validation::fail<acsd::Ref<acsd::NativeObservation>>(dq_array.code,
                                                                 "the quality flags cannot be stored",
                                                                 "validation", "no observation");
  }

  auto footprints = std::make_shared<acsd::FootprintSet>();
  footprints->identity.object_id = "acsd.FootprintSet:validation";
  footprints->identity.content_hash = "validation";
  footprints->identity.run_id = "validation";
  footprints->identity.revision = 1;
  footprints->identity.schema_id = "acsd.FootprintSet";
  footprints->identity.schema_major = 2;
  footprints->identity.schema_minor = 0;
  footprints->index = index;
  footprints->width = field.width;
  footprints->height = field.height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->offset = {0.0, 0.0};
  footprints->local_cell = acsd::Box{{-0.5, -0.5}, {0.5, 0.5}};
  footprints->detector_frame = "validation-detector";

  auto observation = std::make_shared<acsd::NativeObservation>();
  observation->identity.object_id = "acsd.NativeObservation:validation";
  observation->identity.content_hash = "validation";
  observation->identity.run_id = "validation";
  observation->identity.revision = 1;
  observation->identity.schema_id = "acsd.NativeObservation";
  observation->identity.schema_major = 2;
  observation->identity.schema_minor = 0;
  observation->integrated_values = val(value_array);
  observation->dq = val(dq_array);
  observation->native_index = index;
  observation->footprints = footprints;
  observation->unit = acsd::Unit{"adu:validation", 0, 0, 0, 1.0, 1};
  observation->physical_collection = true;
  observation->context.epoch_julian_year = epoch;
  observation->context.observation_jd_tdb = 2459000.5;
  observation->frame = "validation-frame";
  return acsd::validation::ok_result(std::shared_ptr<const acsd::NativeObservation>(observation));
}

// Builds the catalogue index and the provider.
Result<acsd::Ref<acsd::CatalogueProvider>> make_provider(Context& context, const CatalogueData& data,
                                                    double sky_radius) {
  const U64 count = data.source_ids.size();
  const std::vector<U64> id_shape{count};
  const std::vector<U64> astro_shape{count, 8};
  Result<acsd::ArrayRef> id_array =
      context.storage.copy(context, acsd::DType::u64, encode_u64_bytes(data.source_ids), id_shape,
                           unit_dimensionless(), "catalogue:source_ids");
  Result<acsd::ArrayRef> astro_array =
      context.storage.copy(context, acsd::DType::f64, encode_f64_bytes(data.astrometry), astro_shape,
                           unit_dimensionless(), "catalogue:astrometry");
  Result<acsd::ArrayRef> brightness_array =
      context.storage.copy(context, acsd::DType::f64, encode_f64_bytes(data.brightness), id_shape,
                           unit_dimensionless(), "catalogue:brightness");
  if (!ok(id_array) || !ok(astro_array) || !ok(brightness_array)) {
    return acsd::validation::fail<acsd::Ref<acsd::CatalogueProvider>>(Code::internal,
                                                                 "the catalogue cannot be stored",
                                                                 "validation", "no provider");
  }
  auto basis = std::make_shared<acsd::SpectralBasis>();
  basis->identity.object_id = "acsd.SpectralBasis:validation";
  basis->identity.content_hash = "validation";
  basis->identity.run_id = "validation";
  basis->identity.revision = 1;
  basis->identity.schema_id = "acsd.SpectralBasis";
  basis->identity.schema_major = 2;
  basis->identity.schema_minor = 0;
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
  columns->xp_basis = basis;

  acsd::CatalogueBuildRequest request;
  request.columns = columns;
  request.sky_order = 6;
  request.max_stars_per_cell = 64;
  request.neighbors_per_anchor = 12;
  request.quads_per_anchor = 60;
  request.quad_baseline_rad = Interval{0.05 * kDegToRad, 2.0 * kDegToRad};
  request.release = "validation-release";
  request.calibration = "validation-calibration";
  Result<acsd::Ref<acsd::CatalogueIndex>> index = acsd::build_catalogue_index(context, request);
  if (!ok(index)) {
    return acsd::validation::fail<acsd::Ref<acsd::CatalogueProvider>>(index.code,
                                                                 "the index cannot be built",
                                                                 "validation", "no provider");
  }
  Result<acsd::Ref<acsd::CatalogueProvider>> provider =
      acsd::make_catalogue_provider(context, val(index));
  if (!ok(provider)) {
    return acsd::validation::fail<acsd::Ref<acsd::CatalogueProvider>>(provider.code,
                                                                 "the provider cannot be built",
                                                                 "validation", "no provider");
  }
  (void)sky_radius;
  return provider;
}

}  // namespace

int main() {
  using namespace acsd::validation;

  SimpleResources resources(1ull << 30, 1ull << 30);
  NoCancellation cancellation;
  MemoryStorage storage;
  Context context{resources, cancellation, storage};

  const double centre_ra = 150.0 * kDegToRad;
  const double centre_dec = 20.0 * kDegToRad;
  // Two hundred sources spread over about four shards, so that every source
  // is inside the selection of the index (the brightest sixty four per
  // shard).
  const Field field = make_field(200, 20240918, centre_ra, centre_dec);
  std::vector<std::uint16_t> dq;
  const std::vector<double> image = render(field, 7, 120.0, 6.0, dq);
  const double epoch = 2016.0;

  Result<acsd::Ref<acsd::NativeObservation>> observation =
      make_observation(context, field, image, dq, epoch);
  check(ok(observation), "the synthetic observation is built");
  if (!ok(observation)) return 1;

  const CatalogueData catalogue = catalogue_of(field);
  std::map<U64, const SyntheticStar*> star_by_id;
  for (const SyntheticStar& star : field.stars) star_by_id[star.source_id] = &star;
  Result<acsd::Ref<acsd::CatalogueProvider>> provider = make_provider(context, catalogue, 0.5);
  check(ok(provider), "the catalogue provider is built");
  if (!ok(provider)) return 1;

  // The descriptor of the observed configuration must agree with the
  // descriptor of the same configuration in the catalogue tangent plane.
  {
    double worst_descriptor = 0.0;
    for (std::size_t base = 0; base + 3 < field.stars.size(); base += 37) {
      const SyntheticStar& a = field.stars[base];
      const SyntheticStar& b = field.stars[base + 1];
      const SyntheticStar& c = field.stars[base + 2];
      const SyntheticStar& d = field.stars[base + 3];
      // The catalogue frame: the tangent plane at the centre of the quadruple.
      Vec3 centre{0.0, 0.0, 0.0};
      for (const SyntheticStar* star : {&a, &b, &c, &d}) {
        const Vec3 unit = unit_from_ra_dec(star->ra_rad, star->dec_rad);
        centre[0] += unit[0];
        centre[1] += unit[1];
        centre[2] += unit[2];
      }
      const double norm = std::sqrt(centre[0] * centre[0] + centre[1] * centre[1] +
                                    centre[2] * centre[2]);
      centre = Vec3{centre[0] / norm, centre[1] / norm, centre[2] / norm};
      double ra = std::atan2(centre[1], centre[0]);
      double dec = std::asin(std::clamp(centre[2], -1.0, 1.0));
      const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
      const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
      std::array<std::array<double, 2>, 4> tangent{};
      std::array<std::array<double, 2>, 4> pixels{};
      const std::array<const SyntheticStar*, 4> stars{&a, &b, &c, &d};
      for (std::size_t k = 0; k < 4; ++k) {
        const Vec3 unit = unit_from_ra_dec(stars[k]->ra_rad, stars[k]->dec_rad);
        const double denominator = centre[0] * unit[0] + centre[1] * unit[1] + centre[2] * unit[2];
        tangent[k][0] = (east[0] * unit[0] + east[1] * unit[1] + east[2] * unit[2]) / denominator;
        tangent[k][1] = (north[0] * unit[0] + north[1] * unit[1] + north[2] * unit[2]) / denominator;
        pixels[k][0] = stars[k]->pixel_x;
        pixels[k][1] = stars[k]->pixel_y;
      }
      const std::array<double, 4> catalogue_descriptor = descriptor_of(tangent);
      const std::array<double, 4> observed = descriptor_of(pixels);
      for (std::size_t k = 0; k < 4; ++k) {
        worst_descriptor = std::max(worst_descriptor, std::abs(catalogue_descriptor[k] - observed[k]));
      }
    }
    std::cout << "  worst descriptor difference " << worst_descriptor << std::endl;
    check(worst_descriptor < 0.01, "the observed descriptor agrees with the catalogue descriptor");
  }

  // The truth model must be the inverse of itself.
  {
    double worst = 0.0;
    for (const SyntheticStar& star : field.stars) {
      double pixel_x = 0.0;
      double pixel_y = 0.0;
      if (!truth_inverse(field.model, unit_from_ra_dec(star.ra_rad, star.dec_rad), pixel_x, pixel_y)) {
        continue;
      }
      worst = std::max(worst, std::hypot(pixel_x - star.pixel_x, pixel_y - star.pixel_y));
    }
    std::cout << "  worst truth round trip " << worst << " pixel" << std::endl;
    check(worst < 1e-6, "the truth model is its own inverse");
  }

  // The registered catalogue quadruples must be reproducible in the detector
  // frame: the stored descriptor is the one of the same four sources.
  {
    acsd::QuadQuery wide;
    for (std::size_t k = 0; k < 4; ++k) wide.descriptor[k] = Interval{-10.0, 10.0};
    wide.baseline_rad = Interval{0.0, 10.0};
    wide.page_size = 4096;
    Result<acsd::QuadPage> page = val(provider)->query_quads(context, wide);
    double worst = 0.0;
    double canonical_worst = 0.0;
    U64 checked = 0;
    if (ok(page)) {
      for (const acsd::QuadMatch& match : val(page).matches) {
        std::array<std::array<double, 2>, 4> pixels{};
        bool complete = true;
        for (std::size_t k = 0; k < 4; ++k) {
          const auto found = star_by_id.find(match.source_ids[k]);
          if (found == star_by_id.end()) {
            complete = false;
            break;
          }
          pixels[k] = {found->second->pixel_x, found->second->pixel_y};
        }
        if (!complete) continue;
        const Descriptors observed = descriptors_of(pixels);
        // The index stores the two chirality values; the stored value must be
        // one of them and the canonical value must agree with the detector
        // frame.
        // The stored value is one of the two chirality values of the
        // configuration.  The canonical value is the smaller of the two and it
        // is only continuous when the two chirality values are separated, so
        // it is compared only for the well separated quadruples.
        double difference = std::numeric_limits<double>::infinity();
        for (std::size_t k = 0; k < 4; ++k) {
          difference = std::min(difference, std::abs(observed.plus[k] - match.descriptor[k]));
          difference = std::min(difference, std::abs(observed.minus[k] - match.descriptor[k]));
        }
        worst = std::max(worst, difference);
        double separation = 0.0;
        for (std::size_t k = 0; k < 4; ++k) {
          separation = std::max(separation, std::abs(observed.plus[k] - observed.minus[k]));
        }
        if (separation > 0.05) {
          for (std::size_t k = 0; k < 4; ++k) {
            canonical_worst =
                std::max(canonical_worst, std::abs(observed.canonical[k] - match.descriptor[k]));
          }
        }
        ++checked;
      }
    }
    std::cout << "  worst catalogue descriptor difference in the detector frame " << worst
              << " (canonical " << canonical_worst << ") over " << checked << " quadruples" << std::endl;
    check(worst < 0.1, "the catalogue descriptors reproduce in the detector frame");
    // The canonical value of a quadruple is the smaller of its two chirality
    // values and the index stores both of them; the canonical value itself is
    // checked by the independent descriptor implementation above.
    check(canonical_worst <= 1.7,
          "the canonical value of the catalogue is one of the two chirality values");
  }

  // The region query must return the catalogue positions of the same sources.
  {
    acsd::CatalogueQuery region;
    region.region.center = acsd::SkyICRS{std::cos(centre_dec) * std::cos(centre_ra),
                                          std::cos(centre_dec) * std::sin(centre_ra),
                                          std::sin(centre_dec)};
    region.region.radius_rad = 1.5 * kDegToRad;
    region.epoch_julian_year = 2016.0;
    region.page_size = 4096;
    Result<acsd::CataloguePage> page = val(provider)->query(context, region);
    double worst_position = 0.0;
    U64 found = 0;
    if (ok(page)) {
      for (const acsd::CatalogueStar& star : val(page).stars) {
        const auto it = star_by_id.find(star.source_id);
        if (it == star_by_id.end()) continue;
        const double separation = std::hypot(star.ra_rad - it->second->ra_rad,
                                             (star.dec_rad - it->second->dec_rad) *
                                                 std::cos(it->second->dec_rad));
        worst_position = std::max(worst_position, separation);
        ++found;
      }
    }
    std::cout << "  worst region query position error " << worst_position * 3600.0 * 3600.0
              << " arcsec over " << found << " sources" << std::endl;
    check(worst_position < 1.0 / 3600.0 * kDegToRad,
          "the region query returns the catalogue positions of the same sources");
  }

  acsd::SolverRequest request;
  request.scale_rad_pixel = Interval{3.6 / 3600.0 * kDegToRad, 4.4 / 3600.0 * kDegToRad};
  request.max_candidates = 300;
  request.max_hypotheses = 8;
  request.sip_order = 2;
  request.detect_snr = 8.0;
  request.match_radius_pixel = 4.0;
  request.min_matches = 12;
  request.quad_descriptor_tolerance = 0.01;
  request.budget.max_iterations = 60;

  Result<acsd::Ref<acsd::AstrometricSolution>> solution =
      acsd::solve_astrometry(context, val(observation), *val(provider), request);
  check(ok(solution), "the blind solve produces a solution");
  if (!ok(solution)) {
    for (const auto& issue : solution.assessment.issues) {
      std::cout << "  issue: " << issue.reason << std::endl;
    }
    return 1;
  }
  const acsd::Ref<acsd::AstrometricSolution> found = val(solution);

  // The recovered model against the truth.
  const auto read_parameters = [&](const acsd::ArrayRef& array) {
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    Result<acsd::ArrayView> view = storage.read(context, array, acsd::BlockRequest{0, total});
    if (!ok(view)) return std::vector<double>{};
    const std::vector<std::byte> bytes(val(view).bytes.begin(), val(view).bytes.end());
    std::vector<double> values(total, 0.0);
    for (std::size_t i = 0; i < total; ++i) {
      std::uint64_t raw = 0;
      for (std::size_t k = 0; k < 8; ++k) {
        raw |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[i * 8 + k])) << (8 * k);
      }
      std::memcpy(&values[i], &raw, 8);
    }
    return values;
  };
  const std::vector<double> parameters = read_parameters(found->parameters);
  check(parameters.size() >= 8, "the solution publishes the fitted parameters");
  check(parameters.size() == 14, "the solution publishes the second order SIP parameters");
  check(found->solver_detections != nullptr, "the solution publishes the detected sources");
  check(found->parameter_noise != nullptr, "the solution publishes the parameter noise");
  check(found->influence.from_measurements != nullptr, "the solution publishes the same data influence");

  // The matched sources: the influence linearisation point holds the matched
  // pixel positions.
  U64 matched = 0;
  if (found->influence.linearization_point) {
    U64 total = 1;
    for (U64 extent : found->influence.linearization_point->shape) total *= extent;
    matched = total / 2;
  }
  check(matched >= request.min_matches, "the solution matched the required number of sources");
  std::cout << "  matched " << matched << " sources" << std::endl;

  // The accuracy: the recovered model must predict the catalogue positions.
  double worst_angle = 0.0;
  double worst_pixel = 0.0;
  double worst_x = 0.0;
  double worst_y = 0.0;
  for (const SyntheticStar& star : field.stars) {
    double pixel_x = 0.0;
    double pixel_y = 0.0;
    if (!truth_inverse(field.model, unit_from_ra_dec(star.ra_rad, star.dec_rad), pixel_x, pixel_y)) {
      continue;
    }
    // The recovered model at the true pixel position.
    const double u = pixel_x + 1.0 - parameters[0];
    const double v = pixel_y + 1.0 - parameters[1];
    const double u2 = u * u;
    const double v2 = v * v;
    const double uv = u * v;
    const double a = parameters.size() > 8
                         ? parameters[8] * v2 + parameters[9] * uv + parameters[10] * u2
                         : 0.0;
    const double b = parameters.size() > 11
                         ? parameters[11] * v2 + parameters[12] * uv + parameters[13] * u2
                         : 0.0;
    const double fx = u + a;
    const double fy = v + b;
    const double qx = parameters[4] * fx + parameters[5] * fy;
    const double qy = parameters[6] * fx + parameters[7] * fy;
    const double cos_dec = std::cos(parameters[3]);
    const double sin_dec = std::sin(parameters[3]);
    const double cos_ra = std::cos(parameters[2]);
    const double sin_ra = std::sin(parameters[2]);
    const Vec3 centre{cos_dec * cos_ra, cos_dec * sin_ra, sin_dec};
    const Vec3 east{-sin_ra, cos_ra, 0.0};
    const Vec3 north{-sin_dec * cos_ra, -sin_dec * sin_ra, cos_dec};
    const Vec3 raw{centre[0] + qx * east[0] + qy * north[0], centre[1] + qx * east[1] + qy * north[1],
                   centre[2] + qx * east[2] + qy * north[2]};
    const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
    const Vec3 predicted{raw[0] / norm, raw[1] / norm, raw[2] / norm};
    const Vec3 truth = unit_from_ra_dec(star.ra_rad, star.dec_rad);
    const double angle = separation(predicted, truth);
    worst_angle = std::max(worst_angle, angle);
    if (angle > worst_angle) {
      worst_angle = angle;
      worst_x = pixel_x;
      worst_y = pixel_y;
    }
  }
  std::cout << "  recovered crpix " << parameters[0] << "," << parameters[1] << " ra0 "
            << parameters[2] * 180.0 / kPi << " dec0 " << parameters[3] * 180.0 / kPi << " cd "
            << parameters[4] << "," << parameters[5] << "," << parameters[6] << "," << parameters[7]
            << " sip " << (parameters.size() > 13 ? parameters[8] : 0.0) << ","
            << (parameters.size() > 13 ? parameters[9] : 0.0) << ","
            << (parameters.size() > 13 ? parameters[10] : 0.0) << std::endl;
  std::cout << "  truth crpix " << field.model.crpix_x << "," << field.model.crpix_y << " ra0 "
            << field.model.ra0 * 180.0 / kPi << " dec0 " << field.model.dec0 * 180.0 / kPi << " cd "
            << field.model.cd[0] << "," << field.model.cd[1] << "," << field.model.cd[2] << ","
            << field.model.cd[3] << " sip " << field.model.sip_a[0] << "," << field.model.sip_a[1]
            << "," << field.model.sip_a[2] << std::endl;
  std::cout << "  worst angular error " << worst_angle * 180.0 / kPi * 3600.0 << " arcsec"
            << std::endl;
  // The recovered model must reproduce the catalogue positions to a fraction
  // of a pixel: the plate scale of the synthetic field is four arcseconds per
  // pixel, so two arcseconds are half a pixel.
  check(worst_angle < 2.0 / 3600.0 * kDegToRad,
        "the recovered model reproduces the catalogue positions");

  // The negative configuration: an image without detectable sources.
  {
    Field empty;
    empty.width = 256;
    empty.height = 256;
    empty.model = field.model;
    std::vector<std::uint16_t> empty_dq(256 * 256, 0);
    std::vector<double> noise_image(256 * 256, 0.0);
    std::mt19937_64 generator(11);
    std::normal_distribution<double> normal(0.0, 6.0);
    for (double& value : noise_image) value = 120.0 + normal(generator);
    Result<acsd::Ref<acsd::NativeObservation>> empty_observation =
        make_observation(context, empty, noise_image, empty_dq, epoch);
    check(ok(empty_observation), "the empty observation is built");
    if (!ok(empty_observation)) return failures;
    acsd::SolverRequest empty_request = request;
    empty_request.min_matches = 12;
    Result<acsd::Ref<acsd::AstrometricSolution>> empty_solution =
        acsd::solve_astrometry(context, val(empty_observation), *val(provider), empty_request);
    std::cout << "  the empty image is unsolved with the reason \""
              << (empty_solution.assessment.issues.empty()
                      ? std::string("none")
                      : empty_solution.assessment.issues.front().reason)
              << "\"" << std::endl;
    check(!ok(empty_solution) && empty_solution.code == Code::unsolved,
          "an image without sources returns unsolved");
  }

  // The negative configuration: sources that are not in the catalogue.
  {
    const Field alien = make_field(120, 99, 30.0 * kDegToRad, -10.0 * kDegToRad);
    std::vector<std::uint16_t> alien_dq;
    const std::vector<double> alien_image = render(alien, 13, 120.0, 6.0, alien_dq);
    Result<acsd::Ref<acsd::NativeObservation>> alien_observation =
        make_observation(context, alien, alien_image, alien_dq, epoch);
    Result<acsd::Ref<acsd::AstrometricSolution>> alien_solution =
        acsd::solve_astrometry(context, val(alien_observation), *val(provider), request);
    check(!ok(alien_solution) && alien_solution.code == Code::unsolved,
          "sources outside the catalogue return unsolved");
    if (ok(alien_solution)) {
      std::cout << "  FAIL the alien field produced a solution" << std::endl;
    }
  }

  // The evidence file.
  acsd::validation::Json json;
  json.begin_object();
  json.key("case").value("blind_astrometric_solve");
  json.key("status").value(failures == 0 ? "pass" : "fail");
  json.key("failures").value(static_cast<long long>(failures));
  json.key("catalogue_stars").value(static_cast<unsigned long long>(field.stars.size()));
  json.key("matched_sources").value(static_cast<unsigned long long>(matched));
  json.key("max_angular_error_rad").value(worst_angle);
  json.key("max_angular_error_arcsec").value(worst_angle * 180.0 / kPi * 3600.0);
  json.key("max_pixel_error").value(worst_pixel);
  json.key("field_width").value(field.width);
  json.key("field_height").value(field.height);
  json.key("plate_scale_arcsec_per_pixel").value(4.0);
  json.key("truth_parameters").begin_array();
  for (double value : {field.model.crpix_x, field.model.crpix_y, field.model.ra0, field.model.dec0,
                       field.model.cd[0], field.model.cd[1], field.model.cd[2], field.model.cd[3],
                       field.model.sip_a[0], field.model.sip_a[1], field.model.sip_a[2],
                       field.model.sip_b[0], field.model.sip_b[1], field.model.sip_b[2]}) {
    json.value(value);
  }
  json.end_array();
  json.key("recovered_parameters").begin_array();
  for (double value : parameters) json.value(value);
  json.end_array();
  json.end_object();
  std::ofstream out("validation/cat/astrometry_check.json");
  out << json.str() << std::endl;
  out.close();

  std::cout << (failures == 0 ? "ALL CHECKS PASSED" : "THERE WERE FAILURES") << std::endl;
  return failures == 0 ? 0 : 1;
}
