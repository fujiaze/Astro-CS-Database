// The TAN and SIP model used by the blind astrometric fit.
//
// The convention is the one of the coordinate contract (OBSERVATION_COORDINATES
// §3) and the one implemented by the coordinates module:
//   u = p0 + 1 - CRPIX, the SIP polynomials A and B in pixel units, the linear
//   part applied after SIP,
//   q = CD * (u + A, v + B),
//   n = (c + q_x e + q_y n_N) / |c + q_x e + q_y n_N|,
// with c the direction of CRVAL, e the local east and n_N the local north.
// The tangent plane coordinates are used directly, which reproduces the FITS
// gnomonic projection exactly.
#include <cmath>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "astrometry_common.hpp"
#include "numerics/linalg.hpp"
#include "numerics/linalg.hpp"

namespace acsd::sources {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;

void local_frame(double ra0, double dec0, Vec3& centre, Vec3& east, Vec3& north) {
  const double cos_dec = std::cos(dec0);
  const double sin_dec = std::sin(dec0);
  const double cos_ra = std::cos(ra0);
  const double sin_ra = std::sin(ra0);
  centre = Vec3{cos_dec * cos_ra, cos_dec * sin_ra, sin_dec};
  east = Vec3{-sin_ra, cos_ra, 0.0};
  north = Vec3{-sin_dec * cos_ra, -sin_dec * sin_ra, cos_dec};
}

}  // namespace

U32 sip_term_count(U32 order) {
  if (order < 2) return 0;
  return (order + 1) * (order + 2) / 2 - 3;
}

std::vector<std::array<U32, 2>> sip_term_powers(U32 order) {
  std::vector<std::array<U32, 2>> powers;
  if (order < 2) return powers;
  for (U32 total = 2; total <= order; ++total) {
    for (U32 x_power = 0; x_power <= total; ++x_power) {
      powers.push_back({x_power, total - x_power});
    }
  }
  return powers;
}

U32 wcs_parameter_count(U32 sip_order) { return 8 + 2 * sip_term_count(sip_order); }

double sip_value(const std::vector<double>& coefficients, std::size_t index,
                 const std::array<U32, 2>& powers, double u, double v) {
  double u_power = 1.0;
  for (U32 k = 0; k < powers[0]; ++k) u_power *= u;
  double v_power = 1.0;
  for (U32 k = 0; k < powers[1]; ++k) v_power *= v;
  if (index >= coefficients.size()) return 0.0;
  return coefficients[index] * u_power * v_power;
}

void sip_derivatives(const std::vector<double>& coefficients, std::size_t index,
                     const std::array<U32, 2>& powers, double u, double v, double& d_du,
                     double& d_dv) {
  d_du = 0.0;
  d_dv = 0.0;
  if (index >= coefficients.size()) return;
  const double coefficient = coefficients[index];
  if (powers[0] > 0) {
    double u_power = 1.0;
    for (U32 k = 1; k < powers[0]; ++k) u_power *= u;
    double v_power = 1.0;
    for (U32 k = 0; k < powers[1]; ++k) v_power *= v;
    d_du += coefficient * static_cast<double>(powers[0]) * u_power * v_power;
  }
  if (powers[1] > 0) {
    double u_power = 1.0;
    for (U32 k = 0; k < powers[0]; ++k) u_power *= u;
    double v_power = 1.0;
    for (U32 k = 1; k < powers[1]; ++k) v_power *= v;
    d_dv += coefficient * static_cast<double>(powers[1]) * u_power * v_power;
  }
}

Vec3 wcs_forward(const WcsParameters& parameters, double pixel_x, double pixel_y) {
  const double u = pixel_x + 1.0 - parameters.crpix_x;
  const double v = pixel_y + 1.0 - parameters.crpix_y;
  const std::vector<std::array<U32, 2>> powers = sip_term_powers(parameters.sip_order);
  double a = 0.0;
  double b = 0.0;
  for (std::size_t k = 0; k < powers.size(); ++k) {
    a += sip_value(parameters.sip_a, k, powers[k], u, v);
    b += sip_value(parameters.sip_b, k, powers[k], u, v);
  }
  const double fx = u + a;
  const double fy = v + b;
  const double q_x = parameters.cd[0] * fx + parameters.cd[1] * fy;
  const double q_y = parameters.cd[2] * fx + parameters.cd[3] * fy;
  Vec3 centre{};
  Vec3 east{};
  Vec3 north{};
  local_frame(parameters.ra0, parameters.dec0, centre, east, north);
  const Vec3 raw{centre[0] + q_x * east[0] + q_y * north[0],
                 centre[1] + q_x * east[1] + q_y * north[1],
                 centre[2] + q_x * east[2] + q_y * north[2]};
  const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
  if (!(norm > 0.0)) return Vec3{0.0, 0.0, 0.0};
  return Vec3{raw[0] / norm, raw[1] / norm, raw[2] / norm};
}

void wcs_forward_jacobian(const WcsParameters& parameters, double pixel_x, double pixel_y,
                          double jacobian[3][16]) {
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 16; ++column) jacobian[row][column] = 0.0;
  }
  const U32 columns = wcs_parameter_count(parameters.sip_order);
  const double u = pixel_x + 1.0 - parameters.crpix_x;
  const double v = pixel_y + 1.0 - parameters.crpix_y;
  const std::vector<std::array<U32, 2>> powers = sip_term_powers(parameters.sip_order);

  double a = 0.0;
  double b = 0.0;
  double da_du = 0.0;
  double da_dv = 0.0;
  double db_du = 0.0;
  double db_dv = 0.0;
  for (std::size_t k = 0; k < powers.size(); ++k) {
    a += sip_value(parameters.sip_a, k, powers[k], u, v);
    b += sip_value(parameters.sip_b, k, powers[k], u, v);
    double d_du = 0.0;
    double d_dv = 0.0;
    sip_derivatives(parameters.sip_a, k, powers[k], u, v, d_du, d_dv);
    da_du += d_du;
    da_dv += d_dv;
    sip_derivatives(parameters.sip_b, k, powers[k], u, v, d_du, d_dv);
    db_du += d_du;
    db_dv += d_dv;
  }
  const double fx = u + a;
  const double fy = v + b;
  const double dfx_du = 1.0 + da_du;
  const double dfx_dv = da_dv;
  const double dfy_du = db_du;
  const double dfy_dv = 1.0 + db_dv;

  const double q_x = parameters.cd[0] * fx + parameters.cd[1] * fy;
  const double q_y = parameters.cd[2] * fx + parameters.cd[3] * fy;
  Vec3 centre{};
  Vec3 east{};
  Vec3 north{};
  local_frame(parameters.ra0, parameters.dec0, centre, east, north);
  const Vec3 raw{centre[0] + q_x * east[0] + q_y * north[0],
                 centre[1] + q_x * east[1] + q_y * north[1],
                 centre[2] + q_x * east[2] + q_y * north[2]};
  const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
  if (!(norm > 0.0)) return;
  const Vec3 direction{raw[0] / norm, raw[1] / norm, raw[2] / norm};

  // The derivatives with respect to the tangent plane coordinates.
  const Vec3 d_q_x{east[0] - q_x * direction[0], east[1] - q_x * direction[1],
                   east[2] - q_x * direction[2]};
  const Vec3 d_q_y{north[0] - q_y * direction[0], north[1] - q_y * direction[1],
                   north[2] - q_y * direction[2]};

  // The derivative of a unit vector with respect to a perturbation of the
  // numerator.
  const auto project = [&](const Vec3& d) {
    const double dot = direction[0] * d[0] + direction[1] * d[1] + direction[2] * d[2];
    return Vec3{(d[0] - dot * direction[0]) / norm, (d[1] - dot * direction[1]) / norm,
                (d[2] - dot * direction[2]) / norm};
  };

  const double cd11 = parameters.cd[0];
  const double cd12 = parameters.cd[1];
  const double cd21 = parameters.cd[2];
  const double cd22 = parameters.cd[3];

  // CRPIX.
  {
    const double scale_q_x = -cd11 * dfx_du - cd12 * dfy_du;
    const double scale_q_y = -cd21 * dfx_du - cd22 * dfy_du;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][0] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }
  {
    const double scale_q_x = -cd11 * dfx_dv - cd12 * dfy_dv;
    const double scale_q_y = -cd21 * dfx_dv - cd22 * dfy_dv;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][1] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }

  // The fiducial point.
  {
    const double cos_dec = std::cos(parameters.dec0);
    const double sin_dec = std::sin(parameters.dec0);
    const double cos_ra = std::cos(parameters.ra0);
    const double sin_ra = std::sin(parameters.ra0);
    // The derivative of the fiducial point with respect to the right
    // ascension: d/d ra (cos dec cos ra, cos dec sin ra, sin dec).
    const Vec3 d_centre{-cos_dec * sin_ra, cos_dec * cos_ra, 0.0};
    const Vec3 d_east{-cos_ra, -sin_ra, 0.0};
    const Vec3 d_north{sin_dec * sin_ra, -sin_dec * cos_ra, 0.0};
    const Vec3 d_raw{d_centre[0] + q_x * d_east[0] + q_y * d_north[0],
                     d_centre[1] + q_x * d_east[1] + q_y * d_north[1],
                     d_centre[2] + q_x * d_east[2] + q_y * d_north[2]};
    const Vec3 projected = project(d_raw);
    for (std::size_t row = 0; row < 3; ++row) jacobian[row][2] = projected[row];
  }
  {
    const double cos_dec = std::cos(parameters.dec0);
    const double sin_dec = std::sin(parameters.dec0);
    const double cos_ra = std::cos(parameters.ra0);
    const double sin_ra = std::sin(parameters.ra0);
    const Vec3 d_centre{-sin_dec * cos_ra, -sin_dec * sin_ra, cos_dec};
    const Vec3 d_north{-cos_dec * cos_ra, -cos_dec * sin_ra, -sin_dec};
    const Vec3 d_raw{d_centre[0] + q_y * d_north[0], d_centre[1] + q_y * d_north[1],
                     d_centre[2] + q_y * d_north[2]};
    const Vec3 projected = project(d_raw);
    for (std::size_t row = 0; row < 3; ++row) jacobian[row][3] = projected[row];
  }

  // The linear part.
  {
    const double scale_q_x = fx;
    const double scale_q_y = 0.0;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][4] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }
  {
    const double scale_q_x = fy;
    const double scale_q_y = 0.0;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][5] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }
  {
    const double scale_q_x = 0.0;
    const double scale_q_y = fx;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][6] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }
  {
    const double scale_q_x = 0.0;
    const double scale_q_y = fy;
    for (std::size_t row = 0; row < 3; ++row) {
      jacobian[row][7] = scale_q_x * d_q_x[row] + scale_q_y * d_q_y[row];
    }
  }

  // The SIP coefficients.
  for (std::size_t k = 0; k < powers.size(); ++k) {
    double u_power = 1.0;
    for (U32 p = 0; p < powers[k][0]; ++p) u_power *= u;
    double v_power = 1.0;
    for (U32 p = 0; p < powers[k][1]; ++p) v_power *= v;
    const double monomial = u_power * v_power;
    const double scale_a_x = cd11 * monomial;
    const double scale_a_y = cd21 * monomial;
    const std::size_t column = 8 + k;
    if (column < columns) {
      for (std::size_t row = 0; row < 3; ++row) {
        jacobian[row][column] = scale_a_x * d_q_x[row] + scale_a_y * d_q_y[row];
      }
    }
    const double scale_b_x = cd12 * monomial;
    const double scale_b_y = cd22 * monomial;
    const std::size_t column_b = 8 + powers.size() + k;
    if (column_b < columns) {
      for (std::size_t row = 0; row < 3; ++row) {
        jacobian[row][column_b] = scale_b_x * d_q_x[row] + scale_b_y * d_q_y[row];
      }
    }
  }
}

void wcs_pixel_jacobian(const WcsParameters& parameters, double pixel_x, double pixel_y,
                        double jacobian[3][2]) {
  for (std::size_t row = 0; row < 3; ++row) {
    jacobian[row][0] = 0.0;
    jacobian[row][1] = 0.0;
  }
  const double u = pixel_x + 1.0 - parameters.crpix_x;
  const double v = pixel_y + 1.0 - parameters.crpix_y;
  const std::vector<std::array<U32, 2>> powers = sip_term_powers(parameters.sip_order);
  double a = 0.0;
  double b = 0.0;
  double da_du = 0.0;
  double da_dv = 0.0;
  double db_du = 0.0;
  double db_dv = 0.0;
  for (std::size_t k = 0; k < powers.size(); ++k) {
    a += sip_value(parameters.sip_a, k, powers[k], u, v);
    b += sip_value(parameters.sip_b, k, powers[k], u, v);
    double d_du = 0.0;
    double d_dv = 0.0;
    sip_derivatives(parameters.sip_a, k, powers[k], u, v, d_du, d_dv);
    da_du += d_du;
    da_dv += d_dv;
    sip_derivatives(parameters.sip_b, k, powers[k], u, v, d_du, d_dv);
    db_du += d_du;
    db_dv += d_dv;
  }
  const double fx = u + a;
  const double fy = v + b;
  const double dfx_du = 1.0 + da_du;
  const double dfx_dv = da_dv;
  const double dfy_du = db_du;
  const double dfy_dv = 1.0 + db_dv;
  const double q_x = parameters.cd[0] * fx + parameters.cd[1] * fy;
  const double q_y = parameters.cd[2] * fx + parameters.cd[3] * fy;
  Vec3 centre{};
  Vec3 east{};
  Vec3 north{};
  local_frame(parameters.ra0, parameters.dec0, centre, east, north);
  const Vec3 raw{centre[0] + q_x * east[0] + q_y * north[0],
                 centre[1] + q_x * east[1] + q_y * north[1],
                 centre[2] + q_x * east[2] + q_y * north[2]};
  const double norm = std::sqrt(raw[0] * raw[0] + raw[1] * raw[1] + raw[2] * raw[2]);
  if (!(norm > 0.0)) return;
  const Vec3 direction{raw[0] / norm, raw[1] / norm, raw[2] / norm};
  const auto project = [&](const Vec3& d) {
    const double dot = direction[0] * d[0] + direction[1] * d[1] + direction[2] * d[2];
    return Vec3{(d[0] - dot * direction[0]) / norm, (d[1] - dot * direction[1]) / norm,
                (d[2] - dot * direction[2]) / norm};
  };
  // The tangent plane coordinates with respect to the pixel coordinates.
  const Vec3 d_q_x_pixel{parameters.cd[0] * dfx_du + parameters.cd[1] * dfy_du,
                          parameters.cd[2] * dfx_du + parameters.cd[3] * dfy_du, 0.0};
  const Vec3 d_q_y_pixel{parameters.cd[0] * dfx_dv + parameters.cd[1] * dfy_dv,
                          parameters.cd[2] * dfx_dv + parameters.cd[3] * dfy_dv, 0.0};
  const Vec3 d_p_q_x{east[0] - q_x * direction[0], east[1] - q_x * direction[1],
                     east[2] - q_x * direction[2]};
  const Vec3 d_p_q_y{north[0] - q_y * direction[0], north[1] - q_y * direction[1],
                     north[2] - q_y * direction[2]};
  const Vec3 d_p_px = project(Vec3{d_p_q_x[0] * d_q_x_pixel[0] + d_p_q_y[0] * d_q_y_pixel[0],
                                   d_p_q_x[1] * d_q_x_pixel[0] + d_p_q_y[1] * d_q_y_pixel[0],
                                   d_p_q_x[2] * d_q_x_pixel[0] + d_p_q_y[2] * d_q_y_pixel[0]});
  const Vec3 d_p_py = project(Vec3{d_p_q_x[0] * d_q_x_pixel[1] + d_p_q_y[0] * d_q_y_pixel[1],
                                   d_p_q_x[1] * d_q_x_pixel[1] + d_p_q_y[1] * d_q_y_pixel[1],
                                   d_p_q_x[2] * d_q_x_pixel[1] + d_p_q_y[2] * d_q_y_pixel[1]});
  jacobian[0][0] = d_p_px[0];
  jacobian[1][0] = d_p_px[1];
  jacobian[2][0] = d_p_px[2];
  jacobian[0][1] = d_p_py[0];
  jacobian[1][1] = d_p_py[1];
  jacobian[2][1] = d_p_py[2];
}

std::vector<HeaderField> wcs_header_fields(const WcsParameters& parameters) {
  std::vector<HeaderField> fields;
  const bool sip = parameters.sip_order > 0;
  fields.push_back(HeaderField{"CTYPE1", std::string(sip ? "RA---TAN-SIP" : "RA---TAN")});
  fields.push_back(HeaderField{"CTYPE2", std::string(sip ? "DEC--TAN-SIP" : "DEC--TAN")});
  fields.push_back(HeaderField{"RADESYS", std::string("ICRS")});
  fields.push_back(HeaderField{"CRPIX1", parameters.crpix_x});
  fields.push_back(HeaderField{"CRPIX2", parameters.crpix_y});
  fields.push_back(HeaderField{"CRVAL1", parameters.ra0 * kRadToDeg});
  fields.push_back(HeaderField{"CRVAL2", parameters.dec0 * kRadToDeg});
  fields.push_back(HeaderField{"CD1_1", parameters.cd[0] * kRadToDeg});
  fields.push_back(HeaderField{"CD1_2", parameters.cd[1] * kRadToDeg});
  fields.push_back(HeaderField{"CD2_1", parameters.cd[2] * kRadToDeg});
  fields.push_back(HeaderField{"CD2_2", parameters.cd[3] * kRadToDeg});
  fields.push_back(HeaderField{"LONPOLE", 180.0});
  if (sip) {
    fields.push_back(HeaderField{"A_ORDER", static_cast<std::int64_t>(parameters.sip_order)});
    fields.push_back(HeaderField{"B_ORDER", static_cast<std::int64_t>(parameters.sip_order)});
    const std::vector<std::array<U32, 2>> powers = sip_term_powers(parameters.sip_order);
    for (std::size_t k = 0; k < powers.size(); ++k) {
      fields.push_back(HeaderField{"A_" + std::to_string(powers[k][0]) + "_" +
                                        std::to_string(powers[k][1]),
                                    parameters.sip_a[k]});
      fields.push_back(HeaderField{"B_" + std::to_string(powers[k][0]) + "_" +
                                        std::to_string(powers[k][1]),
                                    parameters.sip_b[k]});
    }
  }
  return fields;
}

}  // namespace acsd::sources
