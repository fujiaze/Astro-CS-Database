// Independent validation of the ACSD v2 coordinates and geometry module.
//
// The program only uses the public declaration contract
// (include/acsd/contracts.hpp) and the public free functions.  It does not
// include any header of src/, it re-implements the tangent plane model and the
// spherical chart from the scientific foundation documents, and it drives the
// published API through its own in-memory Resources and Storage.
//
// Outputs (JSON) are written next to this file:
//   validation/geo/geo_coordinates.json
//   validation/geo/geo_geometry.json
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace {

using acsd::Box;
using acsd::Code;
using acsd::Context;
using acsd::HealpixLeaf;
using acsd::Interval;
using acsd::PixelCenter0;
using acsd::PixelEdge0;
using acsd::Ref;
using acsd::Result;
using acsd::SkyICRS;
using acsd::U32;
using acsd::U64;

constexpr double kPi = 3.14159265358979323846;

// The dyadic endpoints of the contract are decoded here without any helper of
// the implementation tree.
double dyadic_double(const acsd::Dyadic& value) {
  double result = 0.0;
  const std::string& text = value.signed_integer;
  std::size_t index = 0;
  bool negative = false;
  if (index < text.size() && (text[index] == '-' || text[index] == '+')) {
    negative = text[index] == '-';
    ++index;
  }
  for (; index < text.size(); ++index) {
    if (text[index] < '0' || text[index] > '9') break;
    result = result * 10.0 + static_cast<double>(text[index] - '0');
  }
  if (negative) result = -result;
  return std::ldexp(result, value.exponent2);
}

// Little endian encoding of doubles, written independently of the base.
std::vector<std::byte> encode_doubles(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &values[i], 8);
    for (int b = 0; b < 8; ++b) {
      bytes[i * 8 + static_cast<std::size_t>(b)] =
          static_cast<std::byte>((bits >> (8 * b)) & 0xffu);
    }
  }
  return bytes;
}
constexpr double kDeg = kPi / 180.0;

// ---------------------------------------------------------------------------
// The public result helpers are not part of the contract header, so the
// validation program uses its own copies of the same convention.
// ---------------------------------------------------------------------------
template <class T>
bool good(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}
template <class T>
const T& value_of(const Result<T>& result) {
  return *result.value;
}

// ---------------------------------------------------------------------------
// In-memory Resources and Storage (the public interfaces of contracts.hpp).
// ---------------------------------------------------------------------------
class TestResources final : public acsd::Resources {
 public:
  explicit TestResources(const acsd::Limits& limits) : limits_(limits) {}
  acsd::Result<acsd::Reservation> reserve(U64 bytes, U32 threads) override {
    if (bytes > limits_.bytes || threads > limits_.threads) {
      acsd::Result<acsd::Reservation> out;
      out.code = Code::resource_exhausted;
      return out;
    }
    acsd::Result<acsd::Reservation> out;
    out.code = Code::ok;
    out.value = acsd::Reservation{};
    return out;
  }
  acsd::Result<U64> consume_work(U64 units) override {
    if (work_used_ + units > limits_.work) {
      acsd::Result<U64> out;
      out.code = Code::resource_exhausted;
      return out;
    }
    work_used_ += units;
    acsd::Result<U64> out;
    out.code = Code::ok;
    out.value = work_used_;
    return out;
  }
  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = work_used_;
    return usage;
  }

 private:
  acsd::Limits limits_;
  U64 work_used_ = 0;
};

class TestStorage final : public acsd::Storage {
 public:
  acsd::Result<acsd::ArrayRef> copy(Context& context, acsd::DType dtype,
                                     std::span<const std::byte> bytes, std::span<const U64> shape,
                                     const acsd::Unit& unit, acsd::Id index) override {
    (void)context;
    acsd::Result<acsd::ArrayRef> out;
    const std::size_t count = bytes.size();
    auto owner = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<acsd::Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->block_ids.push_back(index);
    array->identity.object_id = index;
    array->identity.content_hash = index;
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    store_[index] = owner;
    out.code = Code::ok;
    out.value = array;
    return out;
  }
  acsd::Result<acsd::ArrayView> read(Context& context, acsd::ArrayRef array,
                                      acsd::BlockRequest range) const override {
    (void)context;
    acsd::Result<acsd::ArrayView> out;
    const auto it = store_.find(array ? array->index_id : acsd::Id());
    if (it == store_.end()) {
      out.code = Code::invalid_request;
      return out;
    }
    const std::size_t bytes_per_element = it->second->size() /
                                          std::max<std::size_t>(1, array->shape.empty()
                                                                         ? 1
                                                                         : array->shape[0]);
    const std::size_t begin = static_cast<std::size_t>(range.begin) * bytes_per_element;
    const std::size_t count = static_cast<std::size_t>(range.count) * bytes_per_element;
    if (begin + count > it->second->size()) {
      out.code = Code::invalid_request;
      return out;
    }
    acsd::ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(it->second->data() + begin, count);
    auto owner = it->second;
    view.lease = acsd::Lease{std::shared_ptr<const void>(owner.get(), [owner](const void*) { (void)owner; })};
    out.code = Code::ok;
    out.value = view;
    return out;
  }
  acsd::Result<acsd::ArrayRef> open(Context& context, std::string root, const acsd::Array& description,
                                     std::span<const acsd::BlockDescriptor> blocks) override {
    (void)context;
    (void)root;
    (void)description;
    (void)blocks;
    acsd::Result<acsd::ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  mutable std::map<acsd::Id, std::shared_ptr<std::vector<std::byte>>> store_;
};

class NoCancellation final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// ---------------------------------------------------------------------------
// The independent tangent plane oracle (OBSERVATION_COORDINATES section 3).
// ---------------------------------------------------------------------------
struct Oracle {
  double crpix[2] = {1.0, 1.0};
  double cd[4] = {0.0, 0.0, 0.0, 0.0};  // row major, rad/pixel
  double alpha0 = 0.0;
  double delta0 = 0.0;
  std::map<std::pair<int, int>, double> sip_a;
  std::map<std::pair<int, int>, double> sip_b;

  static double evaluate(const std::map<std::pair<int, int>, double>& terms, double u, double v) {
    double total = 0.0;
    for (const auto& entry : terms) {
      double factor = entry.second;
      for (int i = 0; i < entry.first.first; ++i) factor *= u;
      for (int j = 0; j < entry.first.second; ++j) factor *= v;
      total += factor;
    }
    return total;
  }

  SkyICRS forward(double x, double y) const {
    const double u = x + 1.0 - crpix[0];
    const double v = y + 1.0 - crpix[1];
    const double a = evaluate(sip_a, u, v);
    const double b = evaluate(sip_b, u, v);
    const double qx = cd[0] * (u + a) + cd[1] * (v + b);
    const double qy = cd[2] * (u + a) + cd[3] * (v + b);
    const double c[3] = {std::cos(delta0) * std::cos(alpha0), std::cos(delta0) * std::sin(alpha0),
                          std::sin(delta0)};
    const double e[3] = {-std::sin(alpha0), std::cos(alpha0), 0.0};
    const double north[3] = {-std::sin(delta0) * std::cos(alpha0),
                              -std::sin(delta0) * std::sin(alpha0), std::cos(delta0)};
    const double s = std::sqrt(1.0 + qx * qx + qy * qy);
    SkyICRS out;
    out.nx = (c[0] + qx * e[0] + qy * north[0]) / s;
    out.ny = (c[1] + qx * e[1] + qy * north[1]) / s;
    out.nz = (c[2] + qx * e[2] + qy * north[2]) / s;
    return out;
  }

  // Newton inversion of the same model, written independently.
  bool inverse(const SkyICRS& direction, double& x, double& y) const {
    const double dc = direction.nx * std::cos(delta0) * std::cos(alpha0) +
                      direction.ny * std::cos(delta0) * std::sin(alpha0) +
                      direction.nz * std::sin(delta0);
    if (!(dc > 0.0)) return false;
    const double de = -direction.nx * std::sin(alpha0) + direction.ny * std::cos(alpha0);
    const double dn = -direction.nx * std::sin(delta0) * std::cos(alpha0) -
                      direction.ny * std::sin(delta0) * std::sin(alpha0) +
                      direction.nz * std::cos(delta0);
    const double qx = de / dc;
    const double qy = dn / dc;
    const double det = cd[0] * cd[3] - cd[1] * cd[2];
    if (det == 0.0) return false;
    const double z0 = (cd[3] * qx - cd[1] * qy) / det;
    const double z1 = (-cd[2] * qx + cd[0] * qy) / det;
    double u = z0;
    double v = z1;
    for (int iteration = 0; iteration < 64; ++iteration) {
      const double a = evaluate(sip_a, u, v);
      const double b = evaluate(sip_b, u, v);
      const double ru = u + a - z0;
      const double rv = v + b - z1;
      if (std::max(std::fabs(ru), std::fabs(rv)) < 1e-13) break;
      double au = 0.0;
      double av = 0.0;
      double bu = 0.0;
      double bv = 0.0;
      gradient(sip_a, u, v, au, av);
      gradient(sip_b, u, v, bu, bv);
      const double j00 = 1.0 + au;
      const double j01 = av;
      const double j10 = bu;
      const double j11 = 1.0 + bv;
      const double jdet = j00 * j11 - j01 * j10;
      if (jdet == 0.0) return false;
      const double du = (-j11 * ru + j01 * rv) / jdet;
      const double dv = (j10 * ru - j00 * rv) / jdet;
      u += du;
      v += dv;
    }
    x = u + crpix[0] - 1.0;
    y = v + crpix[1] - 1.0;
    return true;
  }

  static void gradient(const std::map<std::pair<int, int>, double>& terms, double u, double v,
                       double& du, double& dv) {
    du = 0.0;
    dv = 0.0;
    for (const auto& entry : terms) {
      const int i = entry.first.first;
      const int j = entry.first.second;
      if (i > 0) {
        double factor = entry.second * i;
        for (int k = 1; k < i; ++k) factor *= u;
        for (int k = 0; k < j; ++k) factor *= v;
        du += factor;
      }
      if (j > 0) {
        double factor = entry.second * j;
        for (int k = 0; k < i; ++k) factor *= u;
        for (int k = 1; k < j; ++k) factor *= v;
        dv += factor;
      }
    }
  }
};

double angle_between(const SkyICRS& a, const SkyICRS& b) {
  const double dot = a.nx * b.nx + a.ny * b.ny + a.nz * b.nz;
  const double cx = a.ny * b.nz - a.nz * b.ny;
  const double cy = a.nz * b.nx - a.nx * b.nz;
  const double cz = a.nx * b.ny - a.ny * b.nx;
  return std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz), dot);
}

// The solid angle of a tangent plane rectangle centred on the tangent point.
double analytic_rectangle_area(double half_x, double half_y) {
  return 4.0 * std::atan(half_x * half_y / std::sqrt(1.0 + half_x * half_x + half_y * half_y));
}

// ---------------------------------------------------------------------------
// The independent HEALPix chart (SPHERICAL_DRIZZLE section 2) used to check
// the leaf membership of the production kernel.
// ---------------------------------------------------------------------------
constexpr int kJ[12] = {2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
constexpr int kL[12] = {1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7};

SkyICRS chart_point(int face, double s, double t) {
  const int j = kJ[face];
  const int l = kL[face];
  const double r = j - s - t;
  const double d = s - t;
  double z = 0.0;
  double phi = 0.0;
  if (r < 1.0) {
    z = 1.0 - r * r / 3.0;
    phi = (kPi / 4.0) * (l + d / r);
  } else if (r <= 3.0) {
    z = (2.0 - r) * 2.0 / 3.0;
    phi = (kPi / 4.0) * (l + d);
  } else {
    z = -1.0 + (4.0 - r) * (4.0 - r) / 3.0;
    phi = (kPi / 4.0) * (l + d / (4.0 - r));
  }
  const double sin_theta = std::sqrt(std::max(0.0, 1.0 - z * z));
  SkyICRS out;
  out.nx = sin_theta * std::cos(phi);
  out.ny = sin_theta * std::sin(phi);
  out.nz = z;
  return out;
}

bool chart_inverse(const SkyICRS& direction, int& face, double& s, double& t) {
  const double norm = std::sqrt(direction.nx * direction.nx + direction.ny * direction.ny +
                                direction.nz * direction.nz);
  if (!(norm > 0.0)) return false;
  const double z = direction.nz / norm;
  const double phi = std::atan2(direction.ny / norm, direction.nx / norm);
  std::vector<int> candidates;
  if (z > 2.0 / 3.0) {
    candidates = {0, 1, 2, 3};
  } else if (z < -2.0 / 3.0) {
    candidates = {8, 9, 10, 11};
  } else {
    candidates = {4, 5, 6, 7};
    if (z >= 0.0) {
      for (int f = 0; f < 4; ++f) candidates.push_back(f);
    } else {
      for (int f = 8; f < 12; ++f) candidates.push_back(f);
    }
  }
  for (int f : candidates) {
    const int j = kJ[f];
    const int l = kL[f];
    double shifted = phi;
    const double centre = l * kPi / 4.0;
    while (shifted - centre > kPi / 4.0) shifted -= 2.0 * kPi;
    while (centre - shifted > kPi / 4.0) shifted += 2.0 * kPi;
    double r = 0.0;
    double d = 0.0;
    if (z > 2.0 / 3.0) {
      r = std::sqrt(3.0 * (1.0 - z));
      d = r * (4.0 * shifted / kPi - l);
    } else if (z < -2.0 / 3.0) {
      const double w = std::sqrt(3.0 * (1.0 + z));
      r = 4.0 - w;
      d = (4.0 * shifted / kPi - l) * w;
    } else {
      r = 2.0 - 1.5 * z;
      d = 4.0 * shifted / kPi - l;
    }
    const double cs = (j - r + d) / 2.0;
    const double ct = (j - r - d) / 2.0;
    if (cs < 0.0 || cs > 1.0 || ct < 0.0 || ct > 1.0) continue;
    face = f;
    s = cs;
    t = ct;
    return true;
  }
  return false;
}

U64 nested_index(U32 order, U64 ix, U64 iy) {
  U64 p = 0;
  for (U32 i = 0; i < order; ++i) {
    if ((ix >> i) & 1ull) p |= 1ull << (2 * i);
    if ((iy >> i) & 1ull) p |= 1ull << (2 * i + 1);
  }
  return p;
}

HealpixLeaf oracle_leaf(U32 order, const SkyICRS& direction) {
  int face = 0;
  double s = 0.0;
  double t = 0.0;
  HealpixLeaf leaf{order, 0};
  if (!chart_inverse(direction, face, s, t)) return leaf;
  const U64 nside = 1ull << order;
  U64 ix = static_cast<U64>(std::floor(s * static_cast<double>(nside)));
  U64 iy = static_cast<U64>(std::floor(t * static_cast<double>(nside)));
  if (ix >= nside) ix = nside - 1;
  if (iy >= nside) iy = nside - 1;
  leaf.ipix = static_cast<U64>(face) * nside * nside + nested_index(order, ix, iy);
  return leaf;
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
class Json {
 public:
  void number(const std::string& key, double value) {
    std::ostringstream stream;
    if (std::isfinite(value)) {
      stream << std::setprecision(17) << value;
    } else {
      stream << "null";
    }
    items_.push_back("\"" + key + "\": " + stream.str());
  }
  void integer(const std::string& key, long long value) {
    items_.push_back("\"" + key + "\": " + std::to_string(value));
  }
  void text(const std::string& key, const std::string& value) {
    items_.push_back("\"" + key + "\": \"" + value + "\"");
  }
  void boolean(const std::string& key, bool value) {
    items_.push_back("\"" + key + "\": " + (value ? "true" : "false"));
  }
  void raw(const std::string& key, const std::string& value) {
    items_.push_back("\"" + key + "\": " + value);
  }
  std::string str() const {
    std::string out = "{\n";
    for (std::size_t i = 0; i < items_.size(); ++i) {
      out += "  " + items_[i];
      if (i + 1 < items_.size()) out += ",";
      out += "\n";
    }
    out += "}\n";
    return out;
  }

 private:
  std::vector<std::string> items_;
};

std::string code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "unknown";
}

acsd::HeaderFields make_header(bool sip, double cd_deg, double crpix_x, double crpix_y,
                               double crval_x, double crval_y, const std::string& radesys,
                               bool with_lonpole, double lonpole, bool with_pc) {
  acsd::HeaderFields header;
  header.push_back({"CTYPE1", std::string(sip ? "RA---TAN-SIP" : "RA---TAN")});
  header.push_back({"CTYPE2", std::string(sip ? "DEC--TAN-SIP" : "DEC--TAN")});
  header.push_back({"CRPIX1", crpix_x});
  header.push_back({"CRPIX2", crpix_y});
  header.push_back({"CRVAL1", crval_x});
  header.push_back({"CRVAL2", crval_y});
  header.push_back({"RADESYS", radesys});
  if (with_pc) {
    header.push_back({"CDELT1", cd_deg});
    header.push_back({"CDELT2", cd_deg});
    header.push_back({"PC1_1", 1.0});
    header.push_back({"PC1_2", 0.0});
    header.push_back({"PC2_1", 0.0});
    header.push_back({"PC2_2", 1.0});
  } else {
    header.push_back({"CD1_1", cd_deg});
    header.push_back({"CD1_2", 0.0});
    header.push_back({"CD2_1", 0.0});
    header.push_back({"CD2_2", cd_deg});
  }
  if (with_lonpole) header.push_back({"LONPOLE", lonpole});
  return header;
}

// The provider used by the validation program: it exposes the same WcsModel
// and a boundary enclosure built from the oracle, so that the geometry calls
// are driven by an independent model.
class OracleProvider final : public acsd::FootprintProvider {
 public:
  explicit OracleProvider(Ref<acsd::WcsModel> model, const Oracle& oracle)
      : model_(std::move(model)), oracle_(oracle) {}

  Ref<acsd::WcsModel> model() const override { return model_; }

  acsd::Result<acsd::EnclosedSkyRegion> forward_interval(Context& context, Box domain) const override {
    (void)context;
    acsd::Result<acsd::EnclosedSkyRegion> out;
    acsd::EnclosedSkyRegion region;
    region.xyz[0] = Interval(1.0, -1.0);
    region.xyz[1] = Interval(1.0, -1.0);
    region.xyz[2] = Interval(1.0, -1.0);
    const double corners[4][2] = {{domain.lower[0], domain.lower[1]},
                                  {domain.upper[0], domain.lower[1]},
                                  {domain.upper[0], domain.upper[1]},
                                  {domain.lower[0], domain.upper[1]}};
    for (const auto& corner : corners) {
      const SkyICRS direction = oracle_.forward(corner[0], corner[1]);
      region.xyz[0] = Interval(std::min(region.xyz[0].lower, direction.nx),
                               std::max(region.xyz[0].upper, direction.nx));
      region.xyz[1] = Interval(std::min(region.xyz[1].lower, direction.ny),
                               std::max(region.xyz[1].upper, direction.ny));
      region.xyz[2] = Interval(std::min(region.xyz[2].lower, direction.nz),
                               std::max(region.xyz[2].upper, direction.nz));
    }
    out.code = Code::ok;
    out.value = region;
    return out;
  }

  acsd::Result<acsd::DerivativeResult> derivative_interval(Context& context, Box domain) const override {
    (void)context;
    (void)domain;
    acsd::Result<acsd::DerivativeResult> out;
    out.code = Code::unsupported;
    return out;
  }

  acsd::Result<acsd::EnclosedSkyRegion> boundary_interval(Context& context,
                                                          const acsd::CurvedEdge& curve,
                                                          Interval parameter) const override {
    (void)context;
    (void)parameter;
    acsd::Result<acsd::EnclosedSkyRegion> out;
    acsd::EnclosedSkyRegion region;
    region.xyz[0] = Interval(1.0, -1.0);
    region.xyz[1] = Interval(1.0, -1.0);
    region.xyz[2] = Interval(1.0, -1.0);
    for (const auto& control : curve.bernstein) {
      const double x = dyadic_double(control[0].lower);
      const double y = dyadic_double(control[1].lower);
      const SkyICRS direction = oracle_.forward(x, y);
      region.xyz[0] = Interval(std::min(region.xyz[0].lower, direction.nx),
                               std::max(region.xyz[0].upper, direction.nx));
      region.xyz[1] = Interval(std::min(region.xyz[1].lower, direction.ny),
                               std::max(region.xyz[1].upper, direction.ny));
      region.xyz[2] = Interval(std::min(region.xyz[2].lower, direction.nz),
                               std::max(region.xyz[2].upper, direction.nz));
    }
    out.code = Code::ok;
    out.value = region;
    return out;
  }

  acsd::Result<acsd::SphericalAreaInterval> measure(Context& context,
                                                    const acsd::DetectorFootprint& source,
                                                    const acsd::GeometryBudget& request) const override {
    (void)context;
    (void)source;
    (void)request;
    acsd::Result<acsd::SphericalAreaInterval> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  Ref<acsd::WcsModel> model_;
  Oracle oracle_;
};

}  // namespace

int main(int argc, char** argv) {
  const std::string directory = argc > 1 ? argv[1] : ".";
  std::cout << "ACSD v2 geo validation\n";

  acsd::Limits limits;
  limits.bytes = 64ull * 1024ull * 1024ull;
  limits.work = 10000000ull;
  limits.threads = 1;
  limits.max_depth = 32;
  limits.max_precision_bits = 256;
  TestResources resources(limits);
  TestStorage storage;
  NoCancellation cancellation;
  Context context{resources, cancellation, storage};

  acsd::NumericBudget budget;
  budget.tolerance.absolute = 1e-10;
  budget.tolerance.relative = 1e-12;
  budget.max_iterations = 64;

  // =====================================================================
  // 1. The coordinates
  // =====================================================================
  Json coordinates;
  coordinates.text("case", "C01-like tangent plane and SIP validation");
  coordinates.text("truth_type", "analytic_exact");

  // 1a. A plain TAN WCS against the independent oracle.
  {
    const double cd_deg = 0.01;
    acsd::HeaderFields header = make_header(false, cd_deg, 17.293, 41.617, 210.0, 34.0, "ICRS",
                                            false, 0.0, false);
    const Box domain{{-0.5, -0.5}, {63.5, 63.5}};
    acsd::AstrometricContext astrometric;
    astrometric.epoch_julian_year = 2016.0;
    Result<Ref<acsd::WcsModel>> model =
        acsd::make_wcs(context, header, domain, astrometric, "detector-A", budget);
    coordinates.boolean("tan_admitted", good(model));
    if (good(model)) {
      const Ref<acsd::WcsModel>& wcs = value_of(model);
      Oracle oracle;
      oracle.crpix[0] = 17.293;
      oracle.crpix[1] = 41.617;
      oracle.alpha0 = 210.0 * kDeg;
      oracle.delta0 = 34.0 * kDeg;
      const double cd = cd_deg * kDeg;
      oracle.cd[0] = cd;
      oracle.cd[3] = cd;

      double max_angle = 0.0;
      double max_roundtrip = 0.0;
      double max_polynomial_residual = 0.0;
      double max_sky_residual = 0.0;
      std::uint64_t samples = 0;
      for (int i = 0; i < 40; ++i) {
        for (int j = 0; j < 40; ++j) {
          const double x = -0.25 + 0.125 * i + 0.01 * j;
          const double y = -0.25 + 0.125 * j;
          Result<SkyICRS> forward_result = acsd::forward(context, wcs, PixelCenter0{x, y}, budget);
          if (!good(forward_result)) continue;
          const SkyICRS produced = value_of(forward_result);
          const SkyICRS expected = oracle.forward(x, y);
          max_angle = std::max(max_angle, angle_between(produced, expected));
          Result<acsd::InverseResult> inverse_result = acsd::inverse(context, wcs, expected, budget);
          if (good(inverse_result)) {
            const acsd::InverseResult& inverse_value = value_of(inverse_result);
            max_roundtrip = std::max(max_roundtrip, std::max(std::fabs(inverse_value.point.x - x),
                                                             std::fabs(inverse_value.point.y - y)));
            max_polynomial_residual =
                std::max(max_polynomial_residual, inverse_value.polynomial_residual_pixel.upper);
            max_sky_residual = std::max(max_sky_residual, inverse_value.sky_residual_rad.upper);
          }
          ++samples;
        }
      }
      coordinates.integer("forward_samples", static_cast<long long>(samples));
      coordinates.number("max_forward_angle_vs_oracle_rad", max_angle);
      coordinates.number("max_inverse_roundtrip_pixel", max_roundtrip);
      coordinates.number("max_polynomial_residual_pixel", max_polynomial_residual);
      coordinates.number("max_forward_sky_residual_rad", max_sky_residual);
      coordinates.number("certificate_eta_upper", wcs->injectivity.eta.upper);
      coordinates.number("certificate_eta_lower", wcs->injectivity.eta.lower);
      coordinates.boolean("certificate_below_one", wcs->injectivity.eta.upper < 1.0);
      coordinates.number("normalization_error_rad_upper",
                         dyadic_double(wcs->normalization_error_rad.upper));

      // The derivative against a central difference (an external diagnostic).
      double max_relative = 0.0;
      for (int i = 0; i < 12; ++i) {
        for (int j = 0; j < 12; ++j) {
          const double x = 1.0 + 0.5 * i;
          const double y = 2.0 + 0.5 * j;
          Result<acsd::DerivativeResult> derivative_result =
              acsd::derivative(context, wcs, PixelCenter0{x, y}, budget);
          if (!good(derivative_result)) continue;
          const acsd::DerivativeResult& derivative_value = value_of(derivative_result);
          const double step = 1e-4;
          Result<SkyICRS> px = acsd::forward(context, wcs, PixelCenter0{x + step, y}, budget);
          Result<SkyICRS> mx = acsd::forward(context, wcs, PixelCenter0{x - step, y}, budget);
          Result<SkyICRS> py = acsd::forward(context, wcs, PixelCenter0{x, y + step}, budget);
          Result<SkyICRS> my = acsd::forward(context, wcs, PixelCenter0{x, y - step}, budget);
          if (!good(px) || !good(mx) || !good(py) || !good(my)) continue;
          const SkyICRS dx = {0.5 * (value_of(px).nx - value_of(mx).nx) / step,
                              0.5 * (value_of(px).ny - value_of(mx).ny) / step,
                              0.5 * (value_of(px).nz - value_of(mx).nz) / step};
          const SkyICRS dy = {0.5 * (value_of(py).nx - value_of(my).nx) / step,
                              0.5 * (value_of(py).ny - value_of(my).ny) / step,
                              0.5 * (value_of(py).nz - value_of(my).nz) / step};
          for (int row = 0; row < 3; ++row) {
            const double fd_u = row == 0 ? dx.nx : (row == 1 ? dx.ny : dx.nz);
            const double fd_v = row == 0 ? dy.nx : (row == 1 ? dy.ny : dy.nz);
            const double an_u = derivative_value.jacobian[static_cast<std::size_t>(row * 2 + 0)];
            const double an_v = derivative_value.jacobian[static_cast<std::size_t>(row * 2 + 1)];
            const double scale = std::max({1e-12, std::fabs(fd_u), std::fabs(fd_v),
                                           std::fabs(an_u), std::fabs(an_v)});
            max_relative = std::max(max_relative, std::max(std::fabs(an_u - fd_u), std::fabs(an_v - fd_v)) / scale);
          }
        }
      }
      coordinates.number("max_derivative_relative_difference_vs_central_difference", max_relative);
    }
  }

  // 1b. The solid angle of a tangent plane footprint against the analytic
  // spherical rectangle.
  {
    const double cd_deg = 0.5;
    acsd::HeaderFields header = make_header(false, cd_deg, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                            false);
    const Box domain{{-1.0, -1.0}, {1.0, 1.0}};
    acsd::AstrometricContext astrometric;
    Result<Ref<acsd::WcsModel>> model =
        acsd::make_wcs(context, header, domain, astrometric, "detector-B", budget);
    coordinates.boolean("area_wcs_admitted", good(model));
    if (good(model)) {
      const Ref<acsd::WcsModel>& wcs = value_of(model);
      const double half = 0.5 * cd_deg * kDeg;
      const double expected = analytic_rectangle_area(half, half);
      coordinates.number("analytic_rectangle_area_sr", expected);

      // The detector footprint of the sample at row 0.
      auto index = std::make_shared<acsd::Index>();
      index->size = 1;
      index->kind = acsd::Index::Kind::native;
      index->namespace_id = "acsd.validation";
      index->sources.push_back(acsd::SourceIdentity{"validation", "exposure", "detector", "channel", 0});
      auto footprints = std::make_shared<acsd::FootprintSet>();
      footprints->index = index;
      footprints->width = 1;
      footprints->height = 1;
      footprints->detector_from_sample = acsd::Mat2{1.0, 0.0, 0.0, 1.0};
      footprints->offset = acsd::Vec2{0.0, 0.0};
      footprints->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
      footprints->wcs = wcs;
      footprints->detector_frame = "detector-B";
      Result<Ref<acsd::DetectorFootprint>> footprint_result =
          acsd::detector_footprint(context, footprints, 0);
      coordinates.boolean("detector_footprint_built", good(footprint_result));
      if (good(footprint_result)) {
        const Ref<acsd::DetectorFootprint>& footprint = value_of(footprint_result);
        OracleProvider provider(wcs, Oracle{});
        acsd::GeometryBudget geometry_budget;
        geometry_budget.max_work = 100000;
        geometry_budget.max_depth = 24;
        geometry_budget.area_absolute_sr = 1e-12;
        Result<acsd::SphericalAreaInterval> area =
            acsd::measure(context, provider, footprint, geometry_budget);
        coordinates.boolean("area_computed", good(area));
        if (good(area)) {
          const acsd::SphericalAreaInterval& value = value_of(area);
          coordinates.number("measured_area_lower_sr", value.lower_sr);
          coordinates.number("measured_area_upper_sr", value.upper_sr);
          coordinates.boolean("analytic_area_inside_interval",
                              value.lower_sr <= expected && expected <= value.upper_sr);
          coordinates.number("area_relative_width", (value.upper_sr - value.lower_sr) / expected);
        }
      }
    }
  }

  // 1c. SIP: forward against the oracle with distortion, and the certificate.
  {
    acsd::HeaderFields header = make_header(true, 0.01, 17.293, 41.617, 210.0, 34.0, "ICRS", false,
                                            0.0, false);
    header.push_back({"A_ORDER", static_cast<std::int64_t>(3)});
    header.push_back({"B_ORDER", static_cast<std::int64_t>(2)});
    header.push_back({"A_0_2", 1.5e-4});
    header.push_back({"A_1_1", -2.0e-5});
    header.push_back({"A_2_0", 3.0e-6});
    header.push_back({"B_0_2", -4.0e-5});
    header.push_back({"B_1_1", 1.0e-5});
    const Box domain{{-0.5, -0.5}, {63.5, 63.5}};
    acsd::AstrometricContext astrometric;
    Result<Ref<acsd::WcsModel>> model =
        acsd::make_wcs(context, header, domain, astrometric, "detector-C", budget);
    coordinates.boolean("sip_wcs_admitted", good(model));
    if (good(model)) {
      const Ref<acsd::WcsModel>& wcs = value_of(model);
      Oracle oracle;
      oracle.crpix[0] = 17.293;
      oracle.crpix[1] = 41.617;
      oracle.alpha0 = 210.0 * kDeg;
      oracle.delta0 = 34.0 * kDeg;
      const double cd = 0.01 * kDeg;
      oracle.cd[0] = cd;
      oracle.cd[3] = cd;
      oracle.sip_a[std::make_pair(0, 2)] = 1.5e-4;
      oracle.sip_a[std::make_pair(1, 1)] = -2.0e-5;
      oracle.sip_a[std::make_pair(2, 0)] = 3.0e-6;
      oracle.sip_b[std::make_pair(0, 2)] = -4.0e-5;
      oracle.sip_b[std::make_pair(1, 1)] = 1.0e-5;
      double max_angle = 0.0;
      double max_roundtrip = 0.0;
      for (int i = 0; i < 25; ++i) {
        for (int j = 0; j < 25; ++j) {
          const double x = 0.5 * i;
          const double y = 0.5 * j;
          Result<SkyICRS> forward_result = acsd::forward(context, wcs, PixelCenter0{x, y}, budget);
          if (!good(forward_result)) continue;
          const SkyICRS expected = oracle.forward(x, y);
          max_angle = std::max(max_angle, angle_between(value_of(forward_result), expected));
          Result<acsd::InverseResult> inverse_result = acsd::inverse(context, wcs, expected, budget);
          if (good(inverse_result)) {
            const acsd::InverseResult& inverse_value = value_of(inverse_result);
            max_roundtrip = std::max(max_roundtrip, std::max(std::fabs(inverse_value.point.x - x),
                                                             std::fabs(inverse_value.point.y - y)));
          }
        }
      }
      coordinates.number("sip_max_forward_angle_vs_oracle_rad", max_angle);
      coordinates.number("sip_max_inverse_roundtrip_pixel", max_roundtrip);
      coordinates.number("sip_certificate_eta_upper", wcs->injectivity.eta.upper);
      coordinates.boolean("sip_certificate_below_one", wcs->injectivity.eta.upper < 1.0);
    }
  }

  // 1d. The origin conversions.
  {
    const acsd::FitsPixel1 fits = acsd::to_fits(PixelCenter0{3.25, -7.5});
    const acsd::PixelCenter0 back = acsd::from_fits_center(fits);
    const acsd::PixelEdge0 edge = acsd::from_fits_edge(acsd::FitsPixel1{0.5, 2.0});
    coordinates.number("to_fits_x", fits.x);
    coordinates.number("to_fits_y", fits.y);
    coordinates.number("from_fits_center_x", back.x);
    coordinates.number("from_fits_center_y", back.y);
    coordinates.number("from_fits_edge_x", edge.x);
    coordinates.number("from_fits_edge_y", edge.y);
    coordinates.boolean("origin_roundtrip_exact",
                        back.x == 3.25 && back.y == -7.5 && edge.x == -0.5 && edge.y == 1.0);
  }

  // 1e. The negative cases.
  {
    acsd::AstrometricContext astrometric;
    const Box domain{{-0.5, -0.5}, {10.5, 10.5}};
    struct Case {
      const char* name;
      acsd::HeaderFields header;
      Box domain;
    };
    std::vector<Case> cases;
    cases.push_back({"non_icrs_reference_frame", make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "FK5",
                                                             false, 0.0, false), domain});
    cases.push_back({"missing_reference_frame", make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "", false,
                                                             0.0, false), domain});
    {
      acsd::HeaderFields header = make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      header.push_back({"CTYPE3", std::string("FREQ")});
      cases.push_back({"extra_axis", header, domain});
    }
    {
      acsd::HeaderFields header = make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      header[0].value = std::string("RA---TPV");
      cases.push_back({"tpv_distortion", header, domain});
    }
    {
      acsd::HeaderFields header = make_header(true, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      header.push_back({"A_ORDER", static_cast<std::int64_t>(3)});
      header.push_back({"A_1_0", 1.0e-3});
      cases.push_back({"low_order_forward_term", header, domain});
    }
    {
      acsd::HeaderFields header = make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      cases.push_back({"domain_too_wide", header, Box{{-20000.0, -20000.0}, {20000.0, 20000.0}}});
    }
    {
      acsd::HeaderFields header = make_header(false, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      header.push_back({"CDELT1", 0.01});
      cases.push_back({"cd_and_cdelt_mixed", header, domain});
    }
    {
      // A folded SIP: the displacement gradient exceeds one over the domain.
      acsd::HeaderFields header = make_header(true, 0.01, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                               false);
      header.push_back({"A_ORDER", static_cast<std::int64_t>(2)});
      header.push_back({"A_0_2", 5.0});
      cases.push_back({"folded_sip_rejected", header, Box{{-5.0, -5.0}, {5.0, 5.0}}});
    }
    std::string negative;
    for (std::size_t index = 0; index < cases.size(); ++index) {
      const Case& entry = cases[index];
      Result<Ref<acsd::WcsModel>> model =
          acsd::make_wcs(context, entry.header, entry.domain, astrometric, "detector-N", budget);
      if (index > 0) negative += ", ";
      negative += std::string("{\"case\": \"") + entry.name + "\", \"code\": \"" +
                 code_name(model.code) + "\"}";
    }
    coordinates.raw("negative_cases", "[" + negative + "]");
  }

  {
    std::ofstream out(directory + "/geo_coordinates.json");
    out << coordinates.str();
  }
  std::cout << "wrote " << directory << "/geo_coordinates.json\n";

  // =====================================================================
  // 2. The geometry
  // =====================================================================
  Json geometry;
  geometry.text("case", "G01-like overlap validation");
  geometry.text("truth_type", "analytic_exact and deterministic_enclosure");

  // 2a. The overlap of a pixel against the HEALPix leaves, with the total
  // area closure against the analytic tangent plane rectangle.
  {
    const double cd_deg = 0.5;
    acsd::HeaderFields header = make_header(false, cd_deg, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0,
                                            false);
    const Box domain{{-1.0, -1.0}, {1.0, 1.0}};
    acsd::AstrometricContext astrometric;
    Result<Ref<acsd::WcsModel>> model =
        acsd::make_wcs(context, header, domain, astrometric, "detector-G", budget);
    geometry.boolean("geometry_wcs_admitted", good(model));
    if (good(model)) {
      const Ref<acsd::WcsModel>& wcs = value_of(model);
      Oracle oracle;
      oracle.crpix[0] = 1.0;
      oracle.crpix[1] = 1.0;
      oracle.alpha0 = 0.0;
      oracle.delta0 = 0.0;
      const double cd = cd_deg * kDeg;
      oracle.cd[0] = cd;
      oracle.cd[3] = cd;

      auto index = std::make_shared<acsd::Index>();
      index->size = 1;
      index->kind = acsd::Index::Kind::native;
      index->namespace_id = "acsd.validation";
      index->sources.push_back(
          acsd::SourceIdentity{"validation", "exposure", "detector", "channel", 0});
      auto footprints = std::make_shared<acsd::FootprintSet>();
      footprints->index = index;
      footprints->width = 1;
      footprints->height = 1;
      footprints->detector_from_sample = acsd::Mat2{1.0, 0.0, 0.0, 1.0};
      footprints->offset = acsd::Vec2{0.0, 0.0};
      footprints->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
      footprints->wcs = wcs;
      footprints->detector_frame = "detector-G";
      Result<Ref<acsd::DetectorFootprint>> footprint_result =
          acsd::detector_footprint(context, footprints, 0);
      if (good(footprint_result)) {
        const Ref<acsd::DetectorFootprint>& footprint = value_of(footprint_result);
        OracleProvider provider(wcs, oracle);
        acsd::GeometryBudget geometry_budget;
        geometry_budget.max_work = 200000;
        geometry_budget.max_depth = 24;
        geometry_budget.area_absolute_sr = 1e-12;
        Result<Ref<acsd::Overlaps>> overlaps =
            acsd::compute_overlap(context, provider, footprint, 8, 1.0, geometry_budget);
        geometry.boolean("overlap_computed", good(overlaps));
        if (good(overlaps)) {
          const Ref<acsd::Overlaps>& value = value_of(overlaps);
          Box footprint_box{{1e300, 1e300}, {-1e300, -1e300}};
          for (const acsd::Ring& ring : footprint->detector_rings) {
            for (const acsd::PixelEdge0& vertex : ring.vertices) {
              footprint_box.lower[0] = std::min(footprint_box.lower[0], vertex.x);
              footprint_box.upper[0] = std::max(footprint_box.upper[0], vertex.x);
              footprint_box.lower[1] = std::min(footprint_box.lower[1], vertex.y);
              footprint_box.upper[1] = std::max(footprint_box.upper[1], vertex.y);
            }
          }
          double lower_sum = 0.0;
          double upper_sum = 0.0;
          double max_leaf_estimate_error = 0.0;
          int leaves_checked = 0;
          for (const acsd::OverlapContribution& contribution : value->contributions) {
            lower_sum += contribution.overlap_area.lower_sr;
            upper_sum += contribution.overlap_area.upper_sr;
            // An independent estimate of the leaf overlap by sampling the leaf
            // cell on a fine grid and testing the membership with the oracle.
            const U64 nside = 1ull << contribution.leaf.order;
            const U64 face = contribution.leaf.ipix / (nside * nside);
            const U64 pixel = contribution.leaf.ipix % (nside * nside);
            U64 ix = 0;
            U64 iy = 0;
            for (U32 i = 0; i < contribution.leaf.order; ++i) {
              if ((pixel >> (2 * i)) & 1ull) ix |= 1ull << i;
              if ((pixel >> (2 * i + 1)) & 1ull) iy |= 1ull << i;
            }
            const int samples = 48;
            int inside = 0;
            for (int a = 0; a < samples; ++a) {
              for (int b = 0; b < samples; ++b) {
                const double s = (static_cast<double>(ix) + (a + 0.5) / samples) /
                                 static_cast<double>(nside);
                const double t = (static_cast<double>(iy) + (b + 0.5) / samples) /
                                 static_cast<double>(nside);
                const SkyICRS direction = chart_point(static_cast<int>(face), s, t);
                double x = 0.0;
                double y = 0.0;
                if (!oracle.inverse(direction, x, y)) continue;
                if (x >= footprint_box.lower[0] && x <= footprint_box.upper[0] &&
                    y >= footprint_box.lower[1] && y <= footprint_box.upper[1]) {
                  ++inside;
                }
              }
            }
            const double cell_area = kPi / (3.0 * static_cast<double>(nside * nside));
            const double estimate = cell_area * inside / (samples * samples);
            const double reference = 0.5 * (contribution.overlap_area.lower_sr +
                                            contribution.overlap_area.upper_sr);
            max_leaf_estimate_error =
                std::max(max_leaf_estimate_error, std::fabs(estimate - reference) / cell_area);
            ++leaves_checked;
          }
          const double half = 0.5 * cd_deg * kDeg;
          const double analytic = analytic_rectangle_area(half, half);
          geometry.integer("leaf_count", static_cast<long long>(value->contributions.size()));
          geometry.integer("leaves_sampled", leaves_checked);
          geometry.number("sum_leaf_lower_sr", lower_sum);
          geometry.number("sum_leaf_upper_sr", upper_sum);
          geometry.number("analytic_drop_area_sr", analytic);
          geometry.boolean("analytic_area_inside_leaf_sum",
                           lower_sum <= analytic && analytic <= upper_sum);
          geometry.number("max_leaf_grid_estimate_difference_over_cell_area", max_leaf_estimate_error);
          geometry.text("termination",
                        value->status.termination == acsd::Termination::tolerance_met
                            ? "tolerance_met"
                            : (value->status.termination == acsd::Termination::work ? "work"
                               : (value->status.termination == acsd::Termination::depth ? "depth"
                                  : (value->status.termination == acsd::Termination::precision
                                         ? "precision"
                                         : "unsupported"))));
          geometry.integer("unresolved_domain_count",
                           static_cast<long long>(value->coarse_candidates.size()));

          // The same call with an exhausted work budget: the unresolved mass
          // must be preserved, not dropped.
          acsd::GeometryBudget starved = geometry_budget;
          starved.max_work = 1;
          starved.max_depth = 1;
          Result<Ref<acsd::Overlaps>> starved_overlaps =
              acsd::compute_overlap(context, provider, footprint, 13, 1.0, starved);
          geometry.boolean("starved_overlap_returned", good(starved_overlaps));
          if (good(starved_overlaps)) {
            const Ref<acsd::Overlaps>& starved_value = value_of(starved_overlaps);
            double starved_upper = 0.0;
            for (const acsd::OverlapContribution& contribution : starved_value->contributions) {
              starved_upper += contribution.overlap_area.upper_sr;
            }
            double unresolved = 0.0;
            for (const acsd::UnresolvedDomain& domain : starved_value->coarse_candidates) {
              unresolved += dyadic_double(domain.mass_sr.upper);
            }
            geometry.text("starved_termination",
                          starved_value->status.termination == acsd::Termination::work ? "work"
                              : (starved_value->status.termination == acsd::Termination::depth
                                     ? "depth"
                                     : "other"));
            geometry.integer("starved_unresolved_domains",
                             static_cast<long long>(starved_value->coarse_candidates.size()));
            geometry.number("starved_unresolved_mass_sr", unresolved);
            geometry.boolean("starved_mass_not_zeroed", unresolved > 0.0);
            geometry.boolean("starved_bounds_still_valid",
                             starved_upper + unresolved >= analytic * 0.5);
          }

          // build_drizzle on the same overlap.
          auto observation = std::make_shared<acsd::NativeObservation>();
          const std::vector<double> signal_values{1.0};
          const std::vector<std::byte> bytes = encode_doubles(signal_values);
          const std::vector<U64> shape{1};
          Result<acsd::ArrayRef> array = context.storage.copy(
              context, acsd::DType::f64, bytes, shape, acsd::Unit{"L", 0, 0, 0, 1.0, 1},
              "acsd.validation.native");
          if (good(array)) {
            observation->integrated_values = value_of(array);
            observation->native_index = index;
            observation->unit = acsd::Unit{"L", 0, 0, 0, 1.0, 1};
            acsd::DrizzleRequest drizzle;
            drizzle.order = 8;
            drizzle.pixfrac = 1.0;
            drizzle.budget = geometry_budget;
            Result<acsd::DrizzleResult> drizzle_result =
                acsd::build_drizzle(context, value, observation, drizzle);
            geometry.boolean("drizzle_built", good(drizzle_result));
            if (!good(drizzle_result)) {
              geometry.text("drizzle_code", code_name(drizzle_result.code));
              for (const acsd::Issue& issue : drizzle_result.assessment.issues) {
                geometry.text("drizzle_issue", issue.reason + " @ " + issue.origin);
              }
            }
            if (good(drizzle_result)) {
              const acsd::DrizzleResult& drizzle_value = value_of(drizzle_result);
              geometry.boolean("drizzle_operator_present", drizzle_value.op != nullptr);
              geometry.boolean("drizzle_estimate_present", drizzle_value.estimate != nullptr);
              geometry.boolean("drizzle_status_bounded",
                               drizzle_value.op->status.termination ==
                                   acsd::Termination::tolerance_met);
            }
          }
        }
      }
    }
  }

  // 2b. The containment fallback A on the hard configurations: a footprint
  // whose corner touches the tangent point exactly (a chart cell boundary runs
  // through that corner) and a very narrow sliver.  Both must keep their mass:
  // the published interval may not collapse and the analytic reference must
  // stay inside the published bounds.
  {
    auto run_fallback_case = [&](const char* name, const acsd::Box& local_cell,
                                 const acsd::Box& detector_rect) {
      const double cd_deg = 0.5;
      acsd::HeaderFields header =
          make_header(false, cd_deg, 1.0, 1.0, 0.0, 0.0, "ICRS", false, 0.0, false);
      const Box domain{{-4.0, -4.0}, {4.0, 4.0}};
      acsd::AstrometricContext astrometric;
      Result<Ref<acsd::WcsModel>> model =
          acsd::make_wcs(context, header, domain, astrometric, "detector-G", budget);
      if (!good(model)) {
        geometry.boolean(std::string(name) + "_computed", false);
        return;
      }
      const Ref<acsd::WcsModel>& wcs = value_of(model);
      Oracle oracle;
      oracle.crpix[0] = 1.0;
      oracle.crpix[1] = 1.0;
      oracle.alpha0 = 0.0;
      oracle.delta0 = 0.0;
      const double cd = cd_deg * kDeg;
      oracle.cd[0] = cd;
      oracle.cd[3] = cd;
      auto index = std::make_shared<acsd::Index>();
      index->size = 1;
      index->kind = acsd::Index::Kind::native;
      index->namespace_id = "acsd.validation";
      index->sources.push_back(
          acsd::SourceIdentity{"validation", "exposure", "detector", "channel", 0});
      auto footprints = std::make_shared<acsd::FootprintSet>();
      footprints->index = index;
      footprints->width = 1;
      footprints->height = 1;
      footprints->detector_from_sample = acsd::Mat2{1.0, 0.0, 0.0, 1.0};
      footprints->offset = acsd::Vec2{0.0, 0.0};
      footprints->local_cell = local_cell;
      footprints->wcs = wcs;
      footprints->detector_frame = "detector-G";
      Result<Ref<acsd::DetectorFootprint>> footprint_result =
          acsd::detector_footprint(context, footprints, 0);
      if (!good(footprint_result)) {
        geometry.boolean(std::string(name) + "_computed", false);
        return;
      }
      const Ref<acsd::DetectorFootprint>& footprint = value_of(footprint_result);
      OracleProvider provider(wcs, oracle);
      acsd::GeometryBudget geometry_budget;
      geometry_budget.max_work = 200000;
      geometry_budget.max_depth = 24;
      geometry_budget.area_absolute_sr = 1e-12;
      Result<Ref<acsd::Overlaps>> overlaps =
          acsd::compute_overlap(context, provider, footprint, 8, 1.0, geometry_budget);
      geometry.boolean(std::string(name) + "_computed", good(overlaps));
      if (!good(overlaps)) return;
      const Ref<acsd::Overlaps>& value = value_of(overlaps);
      double lower_sum = 0.0;
      double upper_sum = 0.0;
      for (const acsd::OverlapContribution& contribution : value->contributions) {
        lower_sum += contribution.overlap_area.lower_sr;
        upper_sum += contribution.overlap_area.upper_sr;
      }
      // The independent reference: the solid angle density enclosure of the
      // detector region times its area.
      Result<acsd::Interval> density = acsd::solid_angle_jacobian(context, wcs, detector_rect, budget);
      const double area = (detector_rect.upper[0] - detector_rect.lower[0]) *
                          (detector_rect.upper[1] - detector_rect.lower[1]);
      double reference_lower = 0.0;
      double reference_upper = 0.0;
      if (good(density)) {
        reference_lower = density.value->lower * area;
        reference_upper = density.value->upper * area;
      }
      geometry.integer(std::string(name) + "_leaf_count",
                       static_cast<long long>(value->contributions.size()));
      geometry.number(std::string(name) + "_sum_lower_sr", lower_sum);
      geometry.number(std::string(name) + "_sum_upper_sr", upper_sum);
      geometry.number(std::string(name) + "_reference_lower_sr", reference_lower);
      geometry.number(std::string(name) + "_reference_upper_sr", reference_upper);
      geometry.boolean(std::string(name) + "_reference_inside_bounds",
                       lower_sum <= reference_upper && reference_lower <= upper_sum);
      geometry.boolean(std::string(name) + "_mass_not_lost", upper_sum > 0.0);
    };
    // The corner of the footprint sits exactly on the tangent point, where a
    // chart cell boundary runs: the touching cells stay undecided.
    run_fallback_case("tangent_corner", acsd::Box{{0.0, 0.0}, {1.0, 1.0}},
                      acsd::Box{{0.0, 0.0}, {1.0, 1.0}});
    // A very narrow sliver of the detector: the cells it crosses stay
    // undecided and keep their mass.
    run_fallback_case("sliver", acsd::Box{{0.0, 0.0}, {0.05, 1.0}},
                      acsd::Box{{0.0, 0.0}, {0.05, 1.0}});
  }

  {
    std::ofstream out(directory + "/geo_geometry.json");
    out << geometry.str();
  }
  std::cout << "wrote " << directory << "/geo_geometry.json\n";
  return 0;
}
