#include "acsd/contracts.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include "coordinates/tan_sip.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/interval.hpp"
#include "numerics/linalg.hpp"

namespace acsd {
namespace {

using Iv = numerics::Interval;

constexpr double kDegToRad = M_PI / 180.0;
constexpr double kTan60 = 1.7320508075688772;  // tan(60 deg)
constexpr double kMaxSipOrder = 9;
constexpr double kCdConditionLimit = 1.0e8;

// ---------------------------------------------------------------------------
// Header access
// ---------------------------------------------------------------------------

std::string upper(std::string key) {
  for (char& ch : key) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return key;
}

class HeaderMap {
 public:
  explicit HeaderMap(const HeaderFields& header) {
    for (const HeaderField& field : header) {
      const std::string key = upper(field.key);
      for (const auto& entry : entries_) {
        if (entry.first == key) {
          duplicate_ = true;
          break;
        }
      }
      entries_.emplace_back(key, field);
    }
  }

  bool duplicate() const { return duplicate_; }

  const std::vector<std::pair<std::string, HeaderField>>& entries() const { return entries_; }

  const HeaderField* find(const std::string& key) const {
    for (const auto& entry : entries_) {
      if (entry.first == key) return &entry.second;
    }
    return nullptr;
  }

  bool has(const std::string& key) const { return find(key) != nullptr; }

  bool get_double(const std::string& key, double& out) const {
    const HeaderField* field = find(key);
    if (field == nullptr) return false;
    if (const double* value = std::get_if<double>(&field->value)) {
      out = *value;
      return true;
    }
    if (const std::int64_t* value = std::get_if<std::int64_t>(&field->value)) {
      out = static_cast<double>(*value);
      return true;
    }
    return false;
  }

  bool get_int(const std::string& key, std::int64_t& out) const {
    const HeaderField* field = find(key);
    if (field == nullptr) return false;
    if (const std::int64_t* value = std::get_if<std::int64_t>(&field->value)) {
      out = *value;
      return true;
    }
    if (const double* value = std::get_if<double>(&field->value)) {
      if (!std::isfinite(*value) || std::floor(*value) != *value) return false;
      out = static_cast<std::int64_t>(*value);
      return true;
    }
    return false;
  }

  bool get_string(const std::string& key, std::string& out) const {
    const HeaderField* field = find(key);
    if (field == nullptr) return false;
    if (const std::string* value = std::get_if<std::string>(&field->value)) {
      out = *value;
      return true;
    }
    return false;
  }

 private:
  std::vector<std::pair<std::string, HeaderField>> entries_;
  bool duplicate_ = false;
};

bool finite(double value) { return std::isfinite(value); }

// ---------------------------------------------------------------------------
// Small 2x2 helpers (row major Mat2)
// ---------------------------------------------------------------------------

bool mat2_invert(const Mat2& m, Mat2& out, double& det) {
  det = m[0] * m[3] - m[1] * m[2];
  if (!finite(det) || det == 0.0) return false;
  const double inv = 1.0 / det;
  out[0] = m[3] * inv;
  out[1] = -m[1] * inv;
  out[2] = -m[2] * inv;
  out[3] = m[0] * inv;
  return true;
}

bool mat2_multiply(const Mat2& a, const Mat2& b, Mat2& out) {
  out[0] = a[0] * b[0] + a[1] * b[2];
  out[1] = a[0] * b[1] + a[1] * b[3];
  out[2] = a[2] * b[0] + a[3] * b[2];
  out[3] = a[2] * b[1] + a[3] * b[3];
  return finite(out[0]) && finite(out[1]) && finite(out[2]) && finite(out[3]);
}

double mat2_condition_2(const Mat2& m) {
  numerics::Matrix a(2, 2);
  a(0, 0) = m[0];
  a(0, 1) = m[1];
  a(1, 0) = m[2];
  a(1, 1) = m[3];
  const numerics::SvdResult svd = numerics::jacobi_svd(a);
  if (svd.singular_values.size() != 2) return std::numeric_limits<double>::infinity();
  const double smax = svd.singular_values[0];
  const double smin = svd.singular_values[1];
  if (!(smin > 0.0)) return std::numeric_limits<double>::infinity();
  return smax / smin;
}

// ---------------------------------------------------------------------------
// Model evaluation shared by the public entry points
// ---------------------------------------------------------------------------

struct TangentState {
  Vec2 q{};
  double s = 1.0;
  SkyICRS n{};
};

TangentState tangent_state(const WcsModel& wcs, double u, double v) {
  TangentState state;
  state.q = coordinates::tangent_offset(wcs, u, v);
  state.s = std::sqrt(1.0 + state.q[0] * state.q[0] + state.q[1] * state.q[1]);
  state.n = coordinates::direction_from_tangent(wcs, state.q);
  return state;
}

double angle_between(const SkyICRS& a, const SkyICRS& b) {
  const double dot = a.nx * b.nx + a.ny * b.ny + a.nz * b.nz;
  const double cx = a.ny * b.nz - a.nz * b.ny;
  const double cy = a.nz * b.nx - a.nx * b.nz;
  const double cz = a.nx * b.ny - a.ny * b.nx;
  const double cross = std::sqrt(cx * cx + cy * cy + cz * cz);
  return std::atan2(cross, dot);
}

}  // namespace

// ---------------------------------------------------------------------------
// Pixel origin conversions: the only +-1 boundary of the system.
// ---------------------------------------------------------------------------

FitsPixel1 to_fits(PixelCenter0 point) { return FitsPixel1{point.x + 1.0, point.y + 1.0}; }

FitsPixel1 to_fits(PixelEdge0 point) { return FitsPixel1{point.x + 1.0, point.y + 1.0}; }

PixelCenter0 from_fits_center(FitsPixel1 point) { return PixelCenter0{point.x - 1.0, point.y - 1.0}; }

PixelEdge0 from_fits_edge(FitsPixel1 point) { return PixelEdge0{point.x - 1.0, point.y - 1.0}; }

// ---------------------------------------------------------------------------
// make_wcs
// ---------------------------------------------------------------------------

namespace {

struct SipAxis {
  U32 order = 0;
  std::vector<std::pair<std::pair<U32, U32>, double>> terms;  // (i,j) -> coefficient
};

bool parse_sip_axis(const HeaderMap& header, const std::string& prefix, bool suffix_sip,
                     SipAxis& axis, std::string& reason) {
  const std::string order_key = prefix + "_ORDER";
  std::int64_t declared_order = 0;
  const bool order_present = header.get_int(order_key, declared_order);

  // Reject coefficients outside the supported index range (i + j > 9).
  for (const auto& entry : header.entries()) {
    const std::string& key = entry.first;
    if (key.size() <= prefix.size() + 3) continue;
    if (key.compare(0, prefix.size(), prefix) != 0) continue;
    if (key[prefix.size()] != '_') continue;
    if (key.find('_', prefix.size() + 1) == std::string::npos) continue;
    char* end = nullptr;
    const long i_value = std::strtol(key.c_str() + prefix.size() + 1, &end, 10);
    if (end == key.c_str() + prefix.size() + 1 || *end != '_') continue;
    const char* j_start = end + 1;
    const long j_value = std::strtol(j_start, &end, 10);
    if (end == j_start) continue;
    if (i_value < 0 || j_value < 0) continue;
    if (static_cast<U32>(i_value + j_value) > kMaxSipOrder) {
      reason = "sip_term_above_supported_order";
      return false;
    }
  }

  // Collect the raw coefficients.
  std::vector<std::pair<std::pair<U32, U32>, double>> raw;
  for (U32 i = 0; i <= kMaxSipOrder; ++i) {
    for (U32 j = 0; i + j <= kMaxSipOrder; ++j) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%s_%u_%u", prefix.c_str(), i, j);
      double value = 0.0;
      if (!header.get_double(buffer, value)) continue;
      if (!finite(value)) {
        reason = "non_finite_sip_coefficient";
        return false;
      }
      if (i + j < 2) {
        reason = "low_order_forward_term";
        return false;
      }
      raw.emplace_back(std::make_pair(i, j), value);
    }
  }

  if (!suffix_sip && (order_present || !raw.empty())) {
    reason = "sip_coefficients_without_sip_ctype";
    return false;
  }

  U32 order = 0;
  if (order_present) {
    if (declared_order < 0 || declared_order > static_cast<std::int64_t>(kMaxSipOrder)) {
      reason = "illegal_sip_order";
      return false;
    }
    order = static_cast<U32>(declared_order);
  } else {
    for (const auto& entry : raw) order = std::max(order, entry.first.first + entry.first.second);
  }

  if (order_present && !raw.empty() && order < 2) {
    reason = "illegal_sip_order";
    return false;
  }

  for (const auto& entry : raw) {
    const U32 total = entry.first.first + entry.first.second;
    if (total > order) {
      reason = "sip_term_above_declared_order";
      return false;
    }
  }

  axis.order = order;
  axis.terms = std::move(raw);
  return true;
}

Polynomial2D to_public_polynomial(const SipAxis& axis) {
  Polynomial2D poly;
  poly.order = axis.order;
  for (const auto& entry : axis.terms) {
    Term2 term;
    term.x_power = entry.first.first;
    term.y_power = entry.first.second;
    term.coefficient = entry.second;
    poly.terms.push_back(term);
  }
  // Canonical order: total degree first, then x power (OBJECTS "几何 recipe" rule).
  std::stable_sort(poly.terms.begin(), poly.terms.end(), [](const Term2& a, const Term2& b) {
    const U32 ta = a.x_power + a.y_power;
    const U32 tb = b.x_power + b.y_power;
    if (ta != tb) return ta < tb;
    return a.x_power > b.x_power;
  });
  return poly;
}

// Iv enclosure of q over a detector box.
Iv tangent_bound(const WcsModel& wcs, const Box& domain, double& q_norm_upper) {
  const Iv u(domain.lower[0] + 1.0 - wcs.crpix_fits1[0],
                   domain.upper[0] + 1.0 - wcs.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - wcs.crpix_fits1[1],
                   domain.upper[1] + 1.0 - wcs.crpix_fits1[1]);
  const Iv a = coordinates::sip_value_iv(wcs.sip_a, u, v);
  const Iv b = coordinates::sip_value_iv(wcs.sip_b, u, v);
  const Iv x = numerics::interval_add(u, a);
  const Iv y = numerics::interval_add(v, b);
  const Mat2& cd = wcs.cd_rad_per_pixel;
  const Iv qx = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), x),
                                             numerics::interval_mul(Iv(cd[1], cd[1]), y));
  const Iv qy = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), x),
                                             numerics::interval_mul(Iv(cd[3], cd[3]), y));
  const Iv qq = numerics::interval_add(numerics::interval_mul(qx, qx),
                                             numerics::interval_mul(qy, qy));
  const Iv root = numerics::interval_sqrt(qq);
  q_norm_upper = root.upper;
  return qq;
}

struct InjectivityOutcome {
  bool certified = false;
  Mat2 preconditioner{1.0, 0.0, 0.0, 1.0};
  double eta_lower = 0.0;
  double eta_upper = 0.0;
  double lipschitz_upper = 0.0;
  double preconditioner_norm = 1.0;
  std::string reason;
};

// L = sup_U ||grad D||_inf  (induced infinity norm of the 2x2 gradient).
double lipschitz_bound(const WcsModel& wcs, const Box& domain) {
  const Iv u(domain.lower[0] + 1.0 - wcs.crpix_fits1[0],
                   domain.upper[0] + 1.0 - wcs.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - wcs.crpix_fits1[1],
                   domain.upper[1] + 1.0 - wcs.crpix_fits1[1]);
  const Iv au = coordinates::sip_grad_u_iv(wcs.sip_a, u, v);
  const Iv av = coordinates::sip_grad_v_iv(wcs.sip_a, u, v);
  const Iv bu = coordinates::sip_grad_u_iv(wcs.sip_b, u, v);
  const Iv bv = coordinates::sip_grad_v_iv(wcs.sip_b, u, v);
  auto sup_abs = [](const Iv& value) {
    return std::max(std::fabs(value.lower), std::fabs(value.upper));
  };
  const double row0 = sup_abs(au) + sup_abs(av);
  const double row1 = sup_abs(bu) + sup_abs(bv);
  return std::max(row0, row1);
}

// eta = sup_U ||I - H J_F||_inf for a fixed preconditioner H.
bool eta_bound(const WcsModel& wcs, const Box& domain, const Mat2& h, double& eta_lo,
               double& eta_hi) {
  const Iv u(domain.lower[0] + 1.0 - wcs.crpix_fits1[0],
                   domain.upper[0] + 1.0 - wcs.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - wcs.crpix_fits1[1],
                   domain.upper[1] + 1.0 - wcs.crpix_fits1[1]);
  const Iv au = coordinates::sip_grad_u_iv(wcs.sip_a, u, v);
  const Iv av = coordinates::sip_grad_v_iv(wcs.sip_a, u, v);
  const Iv bu = coordinates::sip_grad_u_iv(wcs.sip_b, u, v);
  const Iv bv = coordinates::sip_grad_v_iv(wcs.sip_b, u, v);
  // J_F = [[1+au, av],[bu, 1+bv]]
  const Iv j00 = numerics::interval_add(Iv(1.0, 1.0), au);
  const Iv j01 = av;
  const Iv j10 = bu;
  const Iv j11 = numerics::interval_add(Iv(1.0, 1.0), bv);
  // (H J_F)[row][col] = sum_k H[row][k] J_F[k][col]
  auto apply = [&](int row, int col) {
    Iv sum(0.0, 0.0);
    for (int k = 0; k < 2; ++k) {
      const Iv j = (k == 0) ? (col == 0 ? j00 : j01) : (col == 0 ? j10 : j11);
      const double hrc = h[static_cast<std::size_t>(row * 2 + k)];
      sum = numerics::interval_add(sum, numerics::interval_mul(Iv(hrc, hrc), j));
    }
    return sum;
  };
  const Iv e00 = numerics::interval_sub(Iv(1.0, 1.0), apply(0, 0));
  const Iv e01 = numerics::interval_neg(apply(0, 1));
  const Iv e10 = numerics::interval_neg(apply(1, 0));
  const Iv e11 = numerics::interval_sub(Iv(1.0, 1.0), apply(1, 1));
  auto abs_upper = [](const Iv& value) {
    return std::max(std::fabs(value.lower), std::fabs(value.upper));
  };
  auto abs_lower = [](const Iv& value) {
    double lo = 0.0;
    double hi = 0.0;
    if (value.lower <= 0.0 && value.upper >= 0.0) {
      lo = 0.0;
      hi = std::max(std::fabs(value.lower), std::fabs(value.upper));
    } else {
      lo = std::min(std::fabs(value.lower), std::fabs(value.upper));
      hi = std::max(std::fabs(value.lower), std::fabs(value.upper));
    }
    return Iv(lo, hi);
  };
  const Iv row0 = numerics::interval_add(abs_lower(e00), abs_lower(e01));
  const Iv row1 = numerics::interval_add(abs_lower(e10), abs_lower(e11));
  const double upper = std::max(row0.upper, row1.upper);
  const double lower = std::max(row0.lower, row1.lower);
  eta_lo = lower;
  eta_hi = upper;
  return finite(upper);
}

InjectivityOutcome build_certificate(const WcsModel& wcs, const Box& domain) {
  InjectivityOutcome outcome;
  const double lipschitz = lipschitz_bound(wcs, domain);
  outcome.lipschitz_upper = lipschitz;

  // Preferred preconditioner: H = J_F(u_c)^-1 at the centre of the domain.
  const double uc = 0.5 * (domain.lower[0] + domain.upper[0]) + 1.0 - wcs.crpix_fits1[0];
  const double vc = 0.5 * (domain.lower[1] + domain.upper[1]) + 1.0 - wcs.crpix_fits1[1];
  double au = 0.0;
  double av = 0.0;
  double bu = 0.0;
  double bv = 0.0;
  coordinates::sip_grad(wcs.sip_a, uc, vc, au, av);
  coordinates::sip_grad(wcs.sip_b, uc, vc, bu, bv);
  const Mat2 jac{{1.0 + au, av, bu, 1.0 + bv}};
  Mat2 inverse{};
  double det = 0.0;
  if (mat2_invert(jac, inverse, det) && std::fabs(det) > 0.0) {
    double lo = 0.0;
    double hi = 0.0;
    if (eta_bound(wcs, domain, inverse, lo, hi)) {
      outcome.preconditioner = inverse;
      outcome.preconditioner_norm = coordinates::mat2_norm_inf(inverse);
      outcome.eta_lower = lo;
      outcome.eta_upper = hi;
      if (hi < 1.0) {
        outcome.certified = true;
        return outcome;
      }
    }
  }

  // Fall back to the plain Lipschitz condition L = sup ||grad D||_inf < 1.
  outcome.preconditioner = Mat2{1.0, 0.0, 0.0, 1.0};
  outcome.preconditioner_norm = 1.0;
  outcome.eta_lower = 0.0;
  outcome.eta_upper = lipschitz;
  if (lipschitz < 1.0) {
    outcome.certified = true;
    return outcome;
  }
  outcome.reason = "injectivity_unresolved";
  return outcome;
}

std::string hash_wcs(const WcsModel& wcs, double eta_upper, const Mat2& preconditioner) {
  model::Sha256 hash;
  hash.field("schema", "acsd.WcsModel");
  hash.field_f64("crpix0", wcs.crpix_fits1[0]);
  hash.field_f64("crpix1", wcs.crpix_fits1[1]);
  for (double value : wcs.cd_rad_per_pixel) hash.field_f64("cd", value);
  for (double value : wcs.c) hash.field_f64("c", value);
  for (double value : wcs.e) hash.field_f64("e", value);
  for (double value : wcs.north) hash.field_f64("n", value);
  hash.field("sip_a_order", wcs.sip_a.order);
  for (const Term2& term : wcs.sip_a.terms) {
    hash.field("a", static_cast<std::uint64_t>(term.x_power));
    hash.field("a", static_cast<std::uint64_t>(term.y_power));
    hash.field_f64("a", term.coefficient);
  }
  hash.field("sip_b_order", wcs.sip_b.order);
  for (const Term2& term : wcs.sip_b.terms) {
    hash.field("b", static_cast<std::uint64_t>(term.x_power));
    hash.field("b", static_cast<std::uint64_t>(term.y_power));
    hash.field_f64("b", term.coefficient);
  }
  hash.field_f64("domain_lo_x", wcs.domain.lower[0]);
  hash.field_f64("domain_hi_x", wcs.domain.upper[0]);
  hash.field_f64("domain_lo_y", wcs.domain.lower[1]);
  hash.field_f64("domain_hi_y", wcs.domain.upper[1]);
  hash.field_f64("eta", eta_upper);
  for (double value : preconditioner) hash.field_f64("h", value);
  return hash.hex();
}

}  // namespace

Result<Ref<WcsModel>> make_wcs(Context& context, const HeaderFields& header, Box domain,
                                const AstrometricContext& astrometric_context, Id detector_frame,
                                const NumericBudget& request) {
  using model::fail;
  using model::ok_result;

  if (context.cancellation.requested()) {
    return fail<Ref<WcsModel>>(Code::cancelled, "cancelled_before_admission", "make_wcs");
  }
  const U64 work = 512;
  Result<U64> charged = context.resources.consume_work(work);
  if (!ok(charged)) {
    return fail<Ref<WcsModel>>(Code::resource_exhausted, "work_budget_exhausted", "make_wcs");
  }

  if (!finite(domain.lower[0]) || !finite(domain.lower[1]) || !finite(domain.upper[0]) ||
      !finite(domain.upper[1]) || domain.lower[0] > domain.upper[0] ||
      domain.lower[1] > domain.upper[1]) {
    return fail<Ref<WcsModel>>(Code::invalid_request, "invalid_detector_domain", "make_wcs");
  }

  const HeaderMap map(header);
  if (map.duplicate()) {
    return fail<Ref<WcsModel>>(Code::invalid_request, "duplicate_header_key", "make_wcs");
  }

  // --- projection type -----------------------------------------------------
  std::string ctype1;
  std::string ctype2;
  if (!map.get_string("CTYPE1", ctype1) || !map.get_string("CTYPE2", ctype2)) {
    return fail<Ref<WcsModel>>(Code::invalid_request, "missing_ctype", "make_wcs");
  }
  for (const char* extra : {"CTYPE3", "CTYPE4", "CTYPE5"}) {
    if (map.has(extra)) {
      return fail<Ref<WcsModel>>(Code::unsupported, "extra_wcs_axis", "make_wcs");
    }
  }
  const std::string upper1 = upper(ctype1);
  const std::string upper2 = upper(ctype2);
  if (upper1.rfind("RA---", 0) != 0 || upper2.rfind("DEC--", 0) != 0) {
    return fail<Ref<WcsModel>>(Code::unsupported, "unsupported_axis_order", "make_wcs");
  }
  const std::string code1 = upper1.substr(5);
  const std::string code2 = upper2.substr(5);
  if (code1.rfind("TAN", 0) != 0 || code2.rfind("TAN", 0) != 0) {
    return fail<Ref<WcsModel>>(Code::unsupported, "unsupported_projection", "make_wcs");
  }
  for (const std::string* code : {&code1, &code2}) {
    const std::string suffix = code->substr(3);
    if (!suffix.empty() && suffix != "-SIP") {
      return fail<Ref<WcsModel>>(Code::unsupported, "unsupported_distortion", "make_wcs");
    }
  }
  const bool sip_suffix = code1.size() >= 7 ? (code1.substr(3) == "-SIP") : false;
  const bool sip_suffix2 = code2.size() >= 7 ? (code2.substr(3) == "-SIP") : false;
  if (sip_suffix != sip_suffix2) {
    return fail<Ref<WcsModel>>(Code::invalid_model, "sip_suffix_mismatch", "make_wcs");
  }

  // --- reference frame -----------------------------------------------------
  std::string radesys;
  if (!map.get_string("RADESYS", radesys)) {
    return fail<Ref<WcsModel>>(Code::unsupported, "missing_reference_frame", "make_wcs");
  }
  if (upper(radesys) != "ICRS") {
    return fail<Ref<WcsModel>>(Code::unsupported, "reference_frame_not_icrs", "make_wcs");
  }

  // --- units ---------------------------------------------------------------
  for (const char* key : {"CUNIT1", "CUNIT2"}) {
    std::string unit;
    if (map.get_string(key, unit) && upper(unit) != "DEG") {
      return fail<Ref<WcsModel>>(Code::unsupported, "unsupported_cunit", "make_wcs");
    }
  }

  // --- reference point -----------------------------------------------------
  double crpix[2] = {0.0, 0.0};
  double crval[2] = {0.0, 0.0};
  for (int axis = 0; axis < 2; ++axis) {
    const std::string pkey = axis == 0 ? "CRPIX1" : "CRPIX2";
    const std::string vkey = axis == 0 ? "CRVAL1" : "CRVAL2";
    if (!map.get_double(pkey, crpix[axis]) || !map.get_double(vkey, crval[axis])) {
      return fail<Ref<WcsModel>>(Code::invalid_request, "missing_crpix_or_crval", "make_wcs");
    }
    if (!finite(crpix[axis]) || !finite(crval[axis])) {
      return fail<Ref<WcsModel>>(Code::invalid_model, "non_finite_reference_point", "make_wcs");
    }
  }
  crval[0] *= kDegToRad;
  crval[1] *= kDegToRad;

  // --- linear transform ----------------------------------------------------
  const bool cd_present = map.has("CD1_1") || map.has("CD1_2") || map.has("CD2_1") || map.has("CD2_2");
  const bool cdelt_present = map.has("CDELT1") || map.has("CDELT2");
  const bool pc_present = map.has("PC1_1") || map.has("PC1_2") || map.has("PC2_1") || map.has("PC2_2");
  const bool crota_present = map.has("CROTA") || map.has("CROTA1") || map.has("CROTA2");
  if (cd_present && (cdelt_present || pc_present)) {
    return fail<Ref<WcsModel>>(Code::unsupported, "ambiguous_linear_transform", "make_wcs");
  }
  if (crota_present && (cd_present || cdelt_present)) {
    return fail<Ref<WcsModel>>(Code::unsupported, "crota_mixed_with_cd", "make_wcs");
  }
  if (crota_present) {
    return fail<Ref<WcsModel>>(Code::unsupported, "crota_without_linear_transform", "make_wcs");
  }

  Mat2 cd_deg{};
  if (cd_present) {
    // The off diagonal elements default to zero when the header omits them.
    if (!map.get_double("CD1_1", cd_deg[0]) || !map.get_double("CD1_2", cd_deg[1]) ||
        !map.get_double("CD2_1", cd_deg[2]) || !map.get_double("CD2_2", cd_deg[3])) {
      cd_deg[0] = 0.0;
      cd_deg[1] = 0.0;
      cd_deg[2] = 0.0;
      cd_deg[3] = 0.0;
      if (!map.get_double("CD1_1", cd_deg[0]) || !map.get_double("CD2_2", cd_deg[3])) {
        return fail<Ref<WcsModel>>(Code::invalid_request, "incomplete_cd_matrix", "make_wcs");
      }
    }
  } else if (cdelt_present) {
    double cdelt[2] = {0.0, 0.0};
    if (!map.get_double("CDELT1", cdelt[0]) || !map.get_double("CDELT2", cdelt[1])) {
      return fail<Ref<WcsModel>>(Code::invalid_request, "incomplete_cdelt", "make_wcs");
    }
    double pc[4] = {1.0, 0.0, 0.0, 1.0};
    if (pc_present) {
      if (!map.get_double("PC1_1", pc[0]) || !map.get_double("PC1_2", pc[1]) ||
          !map.get_double("PC2_1", pc[2]) || !map.get_double("PC2_2", pc[3])) {
        return fail<Ref<WcsModel>>(Code::invalid_request, "incomplete_pc_matrix", "make_wcs");
      }
    }
    // CD = diag(CDELT) * PC
    cd_deg[0] = cdelt[0] * pc[0];
    cd_deg[1] = cdelt[0] * pc[1];
    cd_deg[2] = cdelt[1] * pc[2];
    cd_deg[3] = cdelt[1] * pc[3];
  } else {
    return fail<Ref<WcsModel>>(Code::invalid_request, "missing_linear_transform", "make_wcs");
  }
  for (double value : cd_deg) {
    if (!finite(value)) {
      return fail<Ref<WcsModel>>(Code::invalid_model, "non_finite_cd", "make_wcs");
    }
  }

  // --- SIP -----------------------------------------------------------------
  SipAxis axis_a;
  SipAxis axis_b;
  std::string reason;
  if (!parse_sip_axis(map, "A", sip_suffix, axis_a, reason)) {
    return fail<Ref<WcsModel>>(reason == "non_finite_sip_coefficient" ||
                                       reason == "low_order_forward_term" ||
                                       reason == "illegal_sip_order" ||
                                       reason == "sip_term_above_declared_order"
                                   ? Code::invalid_model
                                   : Code::invalid_model,
                               reason, "make_wcs");
  }
  if (!parse_sip_axis(map, "B", sip_suffix, axis_b, reason)) {
    return fail<Ref<WcsModel>>(Code::invalid_model, reason, "make_wcs");
  }

  // --- canonical tangent basis and LONPOLE normalisation -------------------
  const double alpha0 = crval[0];
  const double delta0 = crval[1];
  Vec3 c{};
  Vec3 east{};
  Vec3 north{};
  c[0] = std::cos(delta0) * std::cos(alpha0);
  c[1] = std::cos(delta0) * std::sin(alpha0);
  c[2] = std::sin(delta0);
  east[0] = -std::sin(alpha0);
  east[1] = std::cos(alpha0);
  east[2] = 0.0;
  north[0] = -std::sin(delta0) * std::cos(alpha0);
  north[1] = -std::sin(delta0) * std::sin(alpha0);
  north[2] = std::cos(delta0);

  double lonpole_deg = 180.0;
  bool lonpole_defaulted = true;
  if (map.get_double("LONPOLE", lonpole_deg)) {
    lonpole_defaulted = false;
    if (!finite(lonpole_deg)) {
      return fail<Ref<WcsModel>>(Code::invalid_model, "non_finite_lonpole", "make_wcs");
    }
  } else {
    // FITS phi_p rule: at the exact north pole the default is 0 degrees.
    lonpole_deg = (crval[1] == M_PI_2) ? 0.0 : 180.0;
  }
  const double rho = (lonpole_deg - 180.0) * kDegToRad;
  const double cos_rho = std::cos(rho);
  const double sin_rho = std::sin(rho);
  const Mat2 rotation{{cos_rho, sin_rho, -sin_rho, cos_rho}};
  Mat2 cd_rad{};
  if (!mat2_multiply(rotation, cd_deg, cd_rad)) {
    return fail<Ref<WcsModel>>(Code::invalid_model, "non_finite_canonical_cd", "make_wcs");
  }
  cd_rad[0] *= kDegToRad;
  cd_rad[1] *= kDegToRad;
  cd_rad[2] *= kDegToRad;
  cd_rad[3] *= kDegToRad;

  Mat2 cd_inverse{};
  double cd_det = 0.0;
  if (!mat2_invert(cd_rad, cd_inverse, cd_det)) {
    return fail<Ref<WcsModel>>(Code::invalid_model, "singular_cd_matrix", "make_wcs");
  }
  const double condition = mat2_condition_2(cd_rad);
  if (!(condition <= kCdConditionLimit)) {
    return fail<Ref<WcsModel>>(Code::domain, "cd_condition_exceeds_limit", "make_wcs",
                               "cond2(CD) above the first-period limit of 1e8");
  }

  WcsModel model;
  model.crpix_fits1 = {crpix[0], crpix[1]};
  model.cd_rad_per_pixel = cd_rad;
  model.c = c;
  model.e = east;
  model.north = north;
  model.sip_a = to_public_polynomial(axis_a);
  model.sip_b = to_public_polynomial(axis_b);
  model.domain = domain;
  model.epoch_frame_observer = astrometric_context;
  model.original_header = header;
  model.detector_frame = detector_frame;

  // --- admissible domain: sup |q| <= tan(60 deg) ---------------------------
  double q_norm_upper = 0.0;
  tangent_bound(model, domain, q_norm_upper);
  if (!(q_norm_upper <= kTan60)) {
    return fail<Ref<WcsModel>>(Code::domain, "tangent_domain_exceeds_60_degrees", "make_wcs",
                               "sup |q| over the requested detector box is above tan(60 deg)");
  }

  // --- injectivity certificate ---------------------------------------------
  const InjectivityOutcome outcome = build_certificate(model, domain);
  if (!outcome.certified) {
    return fail<Ref<WcsModel>>(Code::domain, "injectivity_unresolved", "make_wcs",
                               "no global single shot certificate on the requested detector box");
  }
  model.injectivity.preconditioner = outcome.preconditioner;
  model.injectivity.eta = Interval(outcome.eta_lower, outcome.eta_upper);
  model.injectivity.domain = domain;
  model.injectivity.arithmetic = request.arithmetic;
  model.injectivity.proof_hash =
      model::content_id(hash_wcs(model, outcome.eta_upper, outcome.preconditioner));

  // --- normalisation error identity ----------------------------------------
  // The rotation of Eq. (5) is carried out in binary64; the deviation from the
  // exact rotation is an explicit numerical error of the parsed model and is
  // never reported as zero.
  {
    const long double rho_hi = static_cast<long double>(lonpole_deg - 180.0) *
                               static_cast<long double>(kDegToRad);
    const long double cos_ref = std::cos(rho_hi);
    const long double sin_ref = std::sin(rho_hi);
    const double d00 = static_cast<double>(cos_ref) - cos_rho;
    const double d01 = static_cast<double>(sin_ref) - sin_rho;
    const double d10 = -static_cast<double>(sin_ref) - (-sin_rho);
    const double d11 = static_cast<double>(cos_ref) - cos_rho;
    const double delta_norm = std::max(std::fabs(d00) + std::fabs(d01),
                                       std::fabs(d10) + std::fabs(d11));
    double error = delta_norm * q_norm_upper;
    if (!(error > 0.0)) {
      error = std::ldexp(1.0, -100);  // strictly positive, never silently zero
    }
    model.normalization_error_rad = model::exact_from_interval(Interval(0.0, error));
  }

  model.identity = model::make_identity("acsd.WcsModel", 2, 0,
                                        hash_wcs(model, outcome.eta_upper, outcome.preconditioner),
                                        "acsd");
  model.assessment = model::make_assessment(Code::ok);
  model.assessment.complete = true;
  model.assessment.validity = numerics::interval_primitive_status() ==
                                      numerics::IntervalPrimitiveStatus::directed_rounding
                                  ? Validity::certified
                                  : Validity::approximate;
  model.assessment.science = ScienceState::bounded;
  model.assessment.capability = Capability::supported;
  model.assessment.condition = model::make_condition("wcs_tan_sip", "fixed_numeric_model");
  model::set_included(model.assessment, "tan_sip_forward_inverse");
  model::set_included(model.assessment, "global_injectivity_certificate");
  model::set_included(model.assessment, "tangent_domain_60_degrees");
  model::set_omitted(model.assessment, "astrometric_parameter_error");
  model::set_omitted(model.assessment, "distortion_outside_sip");
  if (lonpole_defaulted) {
    model::set_included(model.assessment, "lonpole_default_phi_p_rule");
  }
  if (!sip_suffix && axis_a.order == 0 && axis_b.order == 0) {
    model::set_included(model.assessment, "no_sip_distortion");
  }

  return ok_result(std::make_shared<const WcsModel>(std::move(model)));
}

// ---------------------------------------------------------------------------
// forward
// ---------------------------------------------------------------------------

namespace {

Result<SkyICRS> forward_common(Context& context, const WcsModel& wcs, double x, double y) {
  using model::fail;
  using model::ok_result;
  if (context.cancellation.requested()) {
    return fail<SkyICRS>(Code::cancelled, "cancelled", "forward");
  }
  if (x < wcs.domain.lower[0] || x > wcs.domain.upper[0] || y < wcs.domain.lower[1] ||
      y > wcs.domain.upper[1]) {
    return fail<SkyICRS>(Code::domain, "point_outside_detector_domain", "forward");
  }
  const Vec2 uv = coordinates::pixel_to_uv(wcs, x, y);
  const Vec2 q = coordinates::tangent_offset(wcs, uv[0], uv[1]);
  const double q_norm = std::sqrt(q[0] * q[0] + q[1] * q[1]);
  if (!(q_norm <= kTan60)) {
    return fail<SkyICRS>(Code::domain, "outside_tangent_hemisphere", "forward");
  }
  const SkyICRS n = coordinates::direction_from_tangent(wcs, q);
  if (!finite(n.nx) || !finite(n.ny) || !finite(n.nz)) {
    return fail<SkyICRS>(Code::invalid_model, "non_finite_direction", "forward");
  }
  return ok_result(n);
}

}  // namespace

Result<SkyICRS> forward(Context& context, Ref<WcsModel> wcs, PixelCenter0 point,
                        const NumericBudget& request) {
  (void)request;
  if (!wcs) {
    return model::fail<SkyICRS>(Code::invalid_model, "null_wcs_model", "forward");
  }
  return forward_common(context, *wcs, point.x, point.y);
}

Result<SkyICRS> forward(Context& context, Ref<WcsModel> wcs, PixelEdge0 point,
                        const NumericBudget& request) {
  (void)request;
  if (!wcs) {
    return model::fail<SkyICRS>(Code::invalid_model, "null_wcs_model", "forward");
  }
  return forward_common(context, *wcs, point.x, point.y);
}

// ---------------------------------------------------------------------------
// inverse
// ---------------------------------------------------------------------------

Result<InverseResult> inverse(Context& context, Ref<WcsModel> wcs, SkyICRS sky,
                               const NumericBudget& request) {
  using model::fail;
  using model::ok_result;
  if (!wcs) {
    return fail<InverseResult>(Code::invalid_model, "null_wcs_model", "inverse");
  }
  if (context.cancellation.requested()) {
    return fail<InverseResult>(Code::cancelled, "cancelled", "inverse");
  }
  const WcsModel& model = *wcs;
  const double norm = std::sqrt(sky.nx * sky.nx + sky.ny * sky.ny + sky.nz * sky.nz);
  if (!finite(norm) || !(norm > 0.0)) {
    return fail<InverseResult>(Code::invalid_model, "non_unit_sky_direction", "inverse");
  }
  const SkyICRS target{sky.nx / norm, sky.ny / norm, sky.nz / norm};

  // dc = n . c must be strictly positive: the inverse tangent plane exists only
  // on the visible hemisphere of the fiducial point.
  const double dc = target.nx * model.c[0] + target.ny * model.c[1] + target.nz * model.c[2];
  if (!(dc > 0.0)) {
    return fail<InverseResult>(Code::domain, "outside_tangent_hemisphere", "inverse",
                               "n.c <= 0: the direction is not on the visible hemisphere");
  }

  const double de = target.nx * model.e[0] + target.ny * model.e[1] + target.nz * model.e[2];
  const double dn = target.nx * model.north[0] + target.ny * model.north[1] +
                    target.nz * model.north[2];
  const Vec2 q{de / dc, dn / dc};

  Mat2 cd_inverse{};
  double cd_det = 0.0;
  if (!mat2_invert(model.cd_rad_per_pixel, cd_inverse, cd_det)) {
    return fail<InverseResult>(Code::invalid_model, "singular_cd_matrix", "inverse");
  }
  const Vec2 z{cd_inverse[0] * q[0] + cd_inverse[1] * q[1],
               cd_inverse[2] * q[0] + cd_inverse[3] * q[1]};

  // Damped Newton with the analytic Jacobian of F(u) = u + D(u).
  const U32 max_iterations = std::max<U32>(1, std::min<U32>(64, request.max_iterations == 0
                                                                  ? 50
                                                                  : request.max_iterations));
  const double tolerance = 1.0e-11;
  double u = z[0];
  double v = z[1];
  double residual = std::numeric_limits<double>::infinity();
  bool converged = false;
  for (U32 iteration = 0; iteration < max_iterations; ++iteration) {
    const double a = coordinates::sip_value(model.sip_a, u, v);
    const double b = coordinates::sip_value(model.sip_b, u, v);
    const double ru = (u + a) - z[0];
    const double rv = (v + b) - z[1];
    residual = std::max(std::fabs(ru), std::fabs(rv));
    if (residual <= tolerance) {
      converged = true;
      break;
    }
    double au = 0.0;
    double av = 0.0;
    double bu = 0.0;
    double bv = 0.0;
    coordinates::sip_grad(model.sip_a, u, v, au, av);
    coordinates::sip_grad(model.sip_b, u, v, bu, bv);
    const double j00 = 1.0 + au;
    const double j01 = av;
    const double j10 = bu;
    const double j11 = 1.0 + bv;
    const double det = j00 * j11 - j01 * j10;
    if (!finite(det) || det == 0.0) {
      return fail<InverseResult>(Code::invalid_model, "singular_sip_jacobian", "inverse");
    }
    double lambda = 1.0;
    bool improved = false;
    for (int attempt = 0; attempt < 40; ++attempt) {
      const double du = (-j11 * ru + j01 * rv) / det;
      const double dv = (j10 * ru - j00 * rv) / det;
      const double trial_u = u + lambda * du;
      const double trial_v = v + lambda * dv;
      const double ta = coordinates::sip_value(model.sip_a, trial_u, trial_v);
      const double tb = coordinates::sip_value(model.sip_b, trial_u, trial_v);
      const double tru = (trial_u + ta) - z[0];
      const double trv = (trial_v + tb) - z[1];
      const double trial = std::max(std::fabs(tru), std::fabs(trv));
      if (trial < residual) {
        u = trial_u;
        v = trial_v;
        improved = true;
        break;
      }
      lambda *= 0.5;
    }
    if (!improved) break;
  }
  if (!converged) {
    const double a = coordinates::sip_value(model.sip_a, u, v);
    const double b = coordinates::sip_value(model.sip_b, u, v);
    residual = std::max(std::fabs((u + a) - z[0]), std::fabs((v + b) - z[1]));
    if (residual > tolerance) {
      return fail<InverseResult>(Code::nonconverged, "inverse_newton_not_converged", "inverse",
                                 "damped Newton did not reach the pixel residual tolerance");
    }
  }

  // Root enclosure: ||u* - u|| <= ||H|| * eps / (1 - eta).
  const Mat2& h = model.injectivity.preconditioner;
  const double h_norm = coordinates::mat2_norm_inf(h);
  const double eta = model.injectivity.eta.upper;
  const double eps = residual * (1.0 + 1.0e-12) + 1.0e-15;
  const double radius = h_norm * eps / std::max(1.0e-16, 1.0 - eta);
  const double center_x = u + 1.0 - model.crpix_fits1[0];
  const double center_y = v + 1.0 - model.crpix_fits1[1];
  Box root_box{{center_x - radius, center_y - radius}, {center_x + radius, center_y + radius}};
  if (root_box.lower[0] < model.domain.lower[0] || root_box.upper[0] > model.domain.upper[0] ||
      root_box.lower[1] < model.domain.lower[1] || root_box.upper[1] > model.domain.upper[1]) {
    return fail<InverseResult>(Code::domain, "inverse_domain_boundary_unresolved", "inverse",
                               "the certified root box leaves the admitted detector domain");
  }

  // Forward sky residual: recomputed independently from the polynomial residual.
  const SkyICRS forward_sky = coordinates::direction_from_tangent(
      model, coordinates::tangent_offset(model, u, v));
  const double sky_angle = angle_between(forward_sky, target);
  const double sky_bound = sky_angle * (1.0 + 1.0e-9) + 1.0e-15;
  const double normalisation = std::fabs(norm - 1.0);

  InverseResult result;
  result.point = PixelCenter0{center_x, center_y};
  result.root_box = root_box;
  result.polynomial_residual_pixel = Interval(0.0, eps);
  result.sky_residual_rad = Interval(0.0, sky_bound + normalisation);

  Result<InverseResult> out = ok_result(result);
  model::set_included(out.assessment, "damped_newton_analytic_jacobian");
  model::set_included(out.assessment, "root_box_polynomial_residual");
  model::set_included(out.assessment, "forward_sky_residual");
  model::set_omitted(out.assessment, "astrometric_parameter_error");
  return out;
}

// ---------------------------------------------------------------------------
// derivative
// ---------------------------------------------------------------------------

Result<DerivativeResult> derivative(Context& context, Ref<WcsModel> wcs, PixelCenter0 point,
                                    const NumericBudget& request) {
  using model::fail;
  using model::ok_result;
  (void)request;
  if (!wcs) {
    return fail<DerivativeResult>(Code::invalid_model, "null_wcs_model", "derivative");
  }
  if (context.cancellation.requested()) {
    return fail<DerivativeResult>(Code::cancelled, "cancelled", "derivative");
  }
  const WcsModel& model = *wcs;
  if (point.x < model.domain.lower[0] || point.x > model.domain.upper[0] ||
      point.y < model.domain.lower[1] || point.y > model.domain.upper[1]) {
    return fail<DerivativeResult>(Code::domain, "point_outside_detector_domain", "derivative");
  }
  const Vec2 uv = coordinates::pixel_to_uv(model, point.x, point.y);
  const Iv u(uv[0], uv[0]);
  const Iv v(uv[1], uv[1]);
  const Iv au = coordinates::sip_grad_u_iv(model.sip_a, u, v);
  const Iv av = coordinates::sip_grad_v_iv(model.sip_a, u, v);
  const Iv bu = coordinates::sip_grad_u_iv(model.sip_b, u, v);
  const Iv bv = coordinates::sip_grad_v_iv(model.sip_b, u, v);
  const Iv a = coordinates::sip_value_iv(model.sip_a, u, v);
  const Iv b = coordinates::sip_value_iv(model.sip_b, u, v);
  const Iv x = numerics::interval_add(u, a);
  const Iv y = numerics::interval_add(v, b);
  const Mat2& cd = model.cd_rad_per_pixel;
  const Iv qx = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), x),
                                             numerics::interval_mul(Iv(cd[1], cd[1]), y));
  const Iv qy = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), x),
                                             numerics::interval_mul(Iv(cd[3], cd[3]), y));
  const Iv qq = numerics::interval_add(numerics::interval_mul(qx, qx),
                                             numerics::interval_mul(qy, qy));
  const Iv s = numerics::interval_sqrt(numerics::interval_add(Iv(1.0, 1.0), qq));

  // J_F = I + grad D  with  J_F = [[1 + dA/du, dA/dv], [dB/du, 1 + dB/dv]]
  const Iv j00 = numerics::interval_add(Iv(1.0, 1.0), au);
  const Iv j11 = numerics::interval_add(Iv(1.0, 1.0), bv);
  const Iv jf[2][2] = {{j00, av}, {bu, j11}};
  // K = C J_F
  Iv k[2][2];
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 2; ++col) {
      k[row][col] = numerics::interval_add(
          numerics::interval_mul(Iv(cd[row * 2], cd[row * 2]), jf[0][col]),
          numerics::interval_mul(Iv(cd[row * 2 + 1], cd[row * 2 + 1]), jf[1][col]));
    }
  }
  // n and the projector (I - n n^T) with n evaluated in interval arithmetic.
  const Iv nx0 = numerics::interval_add(
      Iv(model.c[0], model.c[0]),
      numerics::interval_add(numerics::interval_mul(qx, Iv(model.e[0], model.e[0])),
                             numerics::interval_mul(qy, Iv(model.north[0], model.north[0]))));
  const Iv ny0 = numerics::interval_add(
      Iv(model.c[1], model.c[1]),
      numerics::interval_add(numerics::interval_mul(qx, Iv(model.e[1], model.e[1])),
                             numerics::interval_mul(qy, Iv(model.north[1], model.north[1]))));
  const Iv nz0 = numerics::interval_add(
      Iv(model.c[2], model.c[2]),
      numerics::interval_add(numerics::interval_mul(qx, Iv(model.e[2], model.e[2])),
                             numerics::interval_mul(qy, Iv(model.north[2], model.north[2]))));
  const Iv nx = numerics::interval_div(nx0, s);
  const Iv ny = numerics::interval_div(ny0, s);
  const Iv nz = numerics::interval_div(nz0, s);

  // J_W = (I - n n^T) E K / s ; E = [e, north] as a 3x2 matrix.
  DerivativeResult result;
  const Iv n_cols[3] = {nx, ny, nz};
  for (int col = 0; col < 2; ++col) {
    // Column of E K.
    Iv m[3];
    for (int row = 0; row < 3; ++row) {
      const double e0 = row == 0 ? model.e[0] : (row == 1 ? model.e[1] : model.e[2]);
      const double e1 = row == 0 ? model.north[0] : (row == 1 ? model.north[1] : model.north[2]);
      m[row] = numerics::interval_add(numerics::interval_mul(Iv(e0, e0), k[0][col]),
                                      numerics::interval_mul(Iv(e1, e1), k[1][col]));
    }
    // t = n^T (E K) column.
    Iv t(0.0, 0.0);
    for (int k = 0; k < 3; ++k) t = numerics::interval_add(t, numerics::interval_mul(n_cols[k], m[k]));
    for (int row = 0; row < 3; ++row) {
      const Iv value =
          numerics::interval_div(numerics::interval_sub(m[row], numerics::interval_mul(n_cols[row], t)), s);
      const std::size_t index = static_cast<std::size_t>(row * 2 + col);
      result.jacobian[index] = value.midpoint();
      result.enclosure[index] = Interval(value.lower, value.upper);
    }
  }
  return ok_result(result);
}

// ---------------------------------------------------------------------------
// solid_angle_jacobian
// ---------------------------------------------------------------------------

Result<Interval> solid_angle_jacobian(Context& context, Ref<WcsModel> wcs, Box domain,
                                       const NumericBudget& request) {
  using model::fail;
  using model::ok_result;
  (void)request;
  if (!wcs) {
    return fail<Interval>(Code::invalid_model, "null_wcs_model", "solid_angle_jacobian");
  }
  if (context.cancellation.requested()) {
    return fail<Interval>(Code::cancelled, "cancelled", "solid_angle_jacobian");
  }
  const WcsModel& model = *wcs;
  if (!finite(domain.lower[0]) || !finite(domain.lower[1]) || !finite(domain.upper[0]) ||
      !finite(domain.upper[1]) || domain.lower[0] > domain.upper[0] ||
      domain.lower[1] > domain.upper[1]) {
    return fail<Interval>(Code::invalid_request, "invalid_domain", "solid_angle_jacobian");
  }
  const Iv u(domain.lower[0] + 1.0 - model.crpix_fits1[0],
                   domain.upper[0] + 1.0 - model.crpix_fits1[0]);
  const Iv v(domain.lower[1] + 1.0 - model.crpix_fits1[1],
                   domain.upper[1] + 1.0 - model.crpix_fits1[1]);
  const Iv g = coordinates::solid_angle_density_iv(model, u, v);
  if (!g.is_finite()) {
    return fail<Interval>(Code::precision_exhausted, "non_finite_solid_angle_enclosure",
                          "solid_angle_jacobian");
  }
  return ok_result(Interval(g.lower, g.upper));
}

}  // namespace acsd
