#include "coordinates/tan_sip.hpp"

#include <algorithm>
#include <cmath>

namespace acsd::coordinates {
namespace {

Iv iv_abs_max(const Iv& v) {
  const double lo = std::fabs(v.lower);
  const double hi = std::fabs(v.upper);
  return Iv(std::min(lo, hi), std::max(lo, hi));
}

Iv iv_abs(const Iv& v) { return iv_abs_max(v); }

}  // namespace

// ---------------------------------------------------------------------------
// Polynomial evaluation
// ---------------------------------------------------------------------------

double sip_value(const Polynomial2D& poly, double u, double v) {
  double total = 0.0;
  for (const Term2& term : poly.terms) {
    double factor = term.coefficient;
    for (U32 i = 0; i < term.x_power; ++i) factor *= u;
    for (U32 j = 0; j < term.y_power; ++j) factor *= v;
    total += factor;
  }
  return total;
}

void sip_grad(const Polynomial2D& poly, double u, double v, double& du, double& dv) {
  double gu = 0.0;
  double gv = 0.0;
  for (const Term2& term : poly.terms) {
    double factor = term.coefficient;
    for (U32 i = 0; i < term.x_power; ++i) factor *= u;
    for (U32 j = 0; j < term.y_power; ++j) factor *= v;
    if (term.x_power > 0) {
      double scaled = term.coefficient * static_cast<double>(term.x_power);
      for (U32 i = 1; i < term.x_power; ++i) scaled *= u;
      for (U32 j = 0; j < term.y_power; ++j) scaled *= v;
      gu += scaled;
    }
    if (term.y_power > 0) {
      double scaled = term.coefficient * static_cast<double>(term.y_power);
      for (U32 i = 0; i < term.x_power; ++i) scaled *= u;
      for (U32 j = 1; j < term.y_power; ++j) scaled *= v;
      gv += scaled;
    }
  }
  du = gu;
  dv = gv;
}

void sip_hessian(const Polynomial2D& poly, double u, double v, double& duu, double& duv, double& dvv) {
  double uu = 0.0;
  double uv = 0.0;
  double vv = 0.0;
  for (const Term2& term : poly.terms) {
    const double xp = static_cast<double>(term.x_power);
    const double yp = static_cast<double>(term.y_power);
    if (term.x_power >= 2) {
      double scaled = term.coefficient * xp * (xp - 1.0);
      for (U32 i = 2; i < term.x_power; ++i) scaled *= u;
      for (U32 j = 0; j < term.y_power; ++j) scaled *= v;
      uu += scaled;
    }
    if (term.x_power >= 1 && term.y_power >= 1) {
      double scaled = term.coefficient * xp * yp;
      for (U32 i = 1; i < term.x_power; ++i) scaled *= u;
      for (U32 j = 1; j < term.y_power; ++j) scaled *= v;
      uv += scaled;
    }
    if (term.y_power >= 2) {
      double scaled = term.coefficient * yp * (yp - 1.0);
      for (U32 i = 0; i < term.x_power; ++i) scaled *= u;
      for (U32 j = 2; j < term.y_power; ++j) scaled *= v;
      vv += scaled;
    }
  }
  duu = uu;
  duv = uv;
  dvv = vv;
}

Iv sip_value_iv(const Polynomial2D& poly, Iv u, Iv v) {
  Iv total(0.0, 0.0);
  for (const Term2& term : poly.terms) {
    Iv factor(term.coefficient, term.coefficient);
    for (U32 i = 0; i < term.x_power; ++i) factor = numerics::interval_mul(factor, u);
    for (U32 j = 0; j < term.y_power; ++j) factor = numerics::interval_mul(factor, v);
    total = numerics::interval_add(total, factor);
  }
  return total;
}

Iv sip_grad_u_iv(const Polynomial2D& poly, Iv u, Iv v) {
  Iv total(0.0, 0.0);
  for (const Term2& term : poly.terms) {
    if (term.x_power == 0) continue;
    Iv factor(term.coefficient * static_cast<double>(term.x_power),
              term.coefficient * static_cast<double>(term.x_power));
    for (U32 i = 1; i < term.x_power; ++i) factor = numerics::interval_mul(factor, u);
    for (U32 j = 0; j < term.y_power; ++j) factor = numerics::interval_mul(factor, v);
    total = numerics::interval_add(total, factor);
  }
  return total;
}

Iv sip_grad_v_iv(const Polynomial2D& poly, Iv u, Iv v) {
  Iv total(0.0, 0.0);
  for (const Term2& term : poly.terms) {
    if (term.y_power == 0) continue;
    Iv factor(term.coefficient * static_cast<double>(term.y_power),
              term.coefficient * static_cast<double>(term.y_power));
    for (U32 i = 0; i < term.x_power; ++i) factor = numerics::interval_mul(factor, u);
    for (U32 j = 1; j < term.y_power; ++j) factor = numerics::interval_mul(factor, v);
    total = numerics::interval_add(total, factor);
  }
  return total;
}

// ---------------------------------------------------------------------------
// Local model evaluation
// ---------------------------------------------------------------------------

double cd_det(const Mat2& cd) { return cd[0] * cd[3] - cd[1] * cd[2]; }

double mat2_norm_inf(const Mat2& m) {
  const double row0 = std::fabs(m[0]) + std::fabs(m[1]);
  const double row1 = std::fabs(m[2]) + std::fabs(m[3]);
  return std::max(row0, row1);
}

Vec2 tangent_offset(const WcsModel& wcs, double u, double v) {
  const double a = sip_value(wcs.sip_a, u, v);
  const double b = sip_value(wcs.sip_b, u, v);
  const double x = u + a;
  const double y = v + b;
  const Mat2& cd = wcs.cd_rad_per_pixel;
  return {cd[0] * x + cd[1] * y, cd[2] * x + cd[3] * y};
}

SkyICRS direction_from_tangent(const WcsModel& wcs, const Vec2& q) {
  // |c + q_x e + q_y north| = sqrt(1 + q.q) because (c, e, north) is an
  // orthonormal right handed frame.
  const double s = std::sqrt(1.0 + q[0] * q[0] + q[1] * q[1]);
  SkyICRS n;
  n.nx = (wcs.c[0] + q[0] * wcs.e[0] + q[1] * wcs.north[0]) / s;
  n.ny = (wcs.c[1] + q[0] * wcs.e[1] + q[1] * wcs.north[1]) / s;
  n.nz = (wcs.c[2] + q[0] * wcs.e[2] + q[1] * wcs.north[2]) / s;
  return n;
}

double solid_angle_density(const WcsModel& wcs, double u, double v) {
  const double a = sip_value(wcs.sip_a, u, v);
  const double b = sip_value(wcs.sip_b, u, v);
  double au = 0.0;
  double av = 0.0;
  double bu = 0.0;
  double bv = 0.0;
  sip_grad(wcs.sip_a, u, v, au, av);
  sip_grad(wcs.sip_b, u, v, bu, bv);
  const double det_j = (1.0 + au) * (1.0 + bv) - av * bu;
  const Vec2 q = tangent_offset(wcs, u, v);
  const double one_plus = 1.0 + q[0] * q[0] + q[1] * q[1];
  const double det_c = std::fabs(cd_det(wcs.cd_rad_per_pixel));
  return det_c * std::fabs(det_j) / (one_plus * std::sqrt(one_plus));
  (void)a;
  (void)b;
}

Iv solid_angle_density_iv(const WcsModel& wcs, Iv u, Iv v) {
  const Iv au = sip_grad_u_iv(wcs.sip_a, u, v);
  const Iv av = sip_grad_v_iv(wcs.sip_a, u, v);
  const Iv bu = sip_grad_u_iv(wcs.sip_b, u, v);
  const Iv bv = sip_grad_v_iv(wcs.sip_b, u, v);
  const Iv one_au = numerics::interval_add(Iv(1.0, 1.0), au);
  const Iv one_bv = numerics::interval_add(Iv(1.0, 1.0), bv);
  Iv det_j = numerics::interval_sub(numerics::interval_mul(one_au, one_bv),
                                    numerics::interval_mul(av, bu));
  const Iv abs_det_j = iv_abs(det_j);

  const Iv a = sip_value_iv(wcs.sip_a, u, v);
  const Iv b = sip_value_iv(wcs.sip_b, u, v);
  const Iv x = numerics::interval_add(u, a);
  const Iv y = numerics::interval_add(v, b);
  const Mat2& cd = wcs.cd_rad_per_pixel;
  const Iv qx = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), x),
                                       numerics::interval_mul(Iv(cd[1], cd[1]), y));
  const Iv qy = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), x),
                                       numerics::interval_mul(Iv(cd[3], cd[3]), y));
  const Iv qq = numerics::interval_add(numerics::interval_mul(qx, qx),
                                       numerics::interval_mul(qy, qy));
  const Iv one_plus = numerics::interval_add(Iv(1.0, 1.0), qq);
  const Iv root = numerics::interval_sqrt(one_plus);
  const Iv denominator = numerics::interval_mul(one_plus, root);
  const double det_c = std::fabs(cd_det(wcs.cd_rad_per_pixel));
  const Iv numerator = numerics::interval_scale(abs_det_j, det_c);
  return numerics::interval_div(numerator, denominator);
}


// ---------------------------------------------------------------------------
// Gradients and enclosures used by the geometry kernel
// ---------------------------------------------------------------------------

void solid_angle_density_gradient(const WcsModel& wcs, double u, double v, double& g, double& dgu,
                                  double& dgv) {
  double au = 0.0;
  double av = 0.0;
  double bu = 0.0;
  double bv = 0.0;
  sip_grad(wcs.sip_a, u, v, au, av);
  sip_grad(wcs.sip_b, u, v, bu, bv);
  double auu = 0.0;
  double auv = 0.0;
  double avv = 0.0;
  double buu = 0.0;
  double buv = 0.0;
  double bvv = 0.0;
  sip_hessian(wcs.sip_a, u, v, auu, auv, avv);
  sip_hessian(wcs.sip_b, u, v, buu, buv, bvv);

  const double det_j = (1.0 + au) * (1.0 + bv) - av * bu;
  const double ddet_du = auu * (1.0 + bv) + au * buv - auv * bu - av * buu;
  const double ddet_dv = auv * (1.0 + bv) + au * bvv - avv * bu - av * buv;

  const Vec2 q = tangent_offset(wcs, u, v);
  const double one_plus = 1.0 + q[0] * q[0] + q[1] * q[1];
  const double det_c = std::fabs(cd_det(wcs.cd_rad_per_pixel));
  const double abs_det_j = std::fabs(det_j);
  const double sign_det = (det_j < 0.0) ? -1.0 : 1.0;

  // dq/du = C (1 + A_u, B_u)^T and dq/dv = C (A_v, 1 + B_v)^T.
  const Mat2& cd = wcs.cd_rad_per_pixel;
  const double dqdu[2] = {cd[0] * (1.0 + au) + cd[1] * bu, cd[2] * (1.0 + au) + cd[3] * bu};
  const double dqdv[2] = {cd[0] * av + cd[1] * (1.0 + bv), cd[2] * av + cd[3] * (1.0 + bv)};
  const double dqq_du = 2.0 * (q[0] * dqdu[0] + q[1] * dqdu[1]);
  const double dqq_dv = 2.0 * (q[0] * dqdv[0] + q[1] * dqdv[1]);

  const double factor = det_c * std::pow(one_plus, -1.5);
  const double d_factor = det_c * (-1.5) * std::pow(one_plus, -2.5);
  g = factor * abs_det_j;
  dgu = factor * sign_det * ddet_du + d_factor * abs_det_j * dqq_du;
  dgv = factor * sign_det * ddet_dv + d_factor * abs_det_j * dqq_dv;
}

void forward_interval_iv(const WcsModel& wcs, Iv u, Iv v, Iv& nx, Iv& ny, Iv& nz) {
  const Iv a = sip_value_iv(wcs.sip_a, u, v);
  const Iv b = sip_value_iv(wcs.sip_b, u, v);
  const Iv x = numerics::interval_add(u, a);
  const Iv y = numerics::interval_add(v, b);
  const Mat2& cd = wcs.cd_rad_per_pixel;
  const Iv qx = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), x),
                                       numerics::interval_mul(Iv(cd[1], cd[1]), y));
  const Iv qy = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), x),
                                       numerics::interval_mul(Iv(cd[3], cd[3]), y));
  const Iv qq = numerics::interval_add(numerics::interval_mul(qx, qx), numerics::interval_mul(qy, qy));
  const Iv s = numerics::interval_sqrt(numerics::interval_add(Iv(1.0, 1.0), qq));
  nx = numerics::interval_div(numerics::interval_add(Iv(wcs.c[0], wcs.c[0]),
                                                     numerics::interval_add(
                                                         numerics::interval_mul(qx, Iv(wcs.e[0], wcs.e[0])),
                                                         numerics::interval_mul(qy, Iv(wcs.north[0], wcs.north[0])))),
                              s);
  ny = numerics::interval_div(numerics::interval_add(Iv(wcs.c[1], wcs.c[1]),
                                                     numerics::interval_add(
                                                         numerics::interval_mul(qx, Iv(wcs.e[1], wcs.e[1])),
                                                         numerics::interval_mul(qy, Iv(wcs.north[1], wcs.north[1])))),
                              s);
  nz = numerics::interval_div(numerics::interval_add(Iv(wcs.c[2], wcs.c[2]),
                                                     numerics::interval_add(
                                                         numerics::interval_mul(qx, Iv(wcs.e[2], wcs.e[2])),
                                                         numerics::interval_mul(qy, Iv(wcs.north[2], wcs.north[2])))),
                              s);
}

void jacobian(const WcsModel& wcs, double u, double v, double j[3][2]) {
  const Vec2 q = tangent_offset(wcs, u, v);
  const double s = std::sqrt(1.0 + q[0] * q[0] + q[1] * q[1]);
  const SkyICRS n = direction_from_tangent(wcs, q);
  double au = 0.0;
  double av = 0.0;
  double bu = 0.0;
  double bv = 0.0;
  sip_grad(wcs.sip_a, u, v, au, av);
  sip_grad(wcs.sip_b, u, v, bu, bv);
  const Mat2& cd = wcs.cd_rad_per_pixel;
  // K = C J_F
  double k[2][2];
  k[0][0] = cd[0] * (1.0 + au) + cd[1] * bu;
  k[0][1] = cd[0] * av + cd[1] * (1.0 + bv);
  k[1][0] = cd[2] * (1.0 + au) + cd[3] * bu;
  k[1][1] = cd[2] * av + cd[3] * (1.0 + bv);
  const double e[3] = {wcs.e[0], wcs.e[1], wcs.e[2]};
  const double nn[3] = {wcs.north[0], wcs.north[1], wcs.north[2]};
  for (int col = 0; col < 2; ++col) {
    double m[3];
    for (int row = 0; row < 3; ++row) {
      m[row] = e[row] * k[0][col] + nn[row] * k[1][col];
    }
    const double t = n.nx * m[0] + n.ny * m[1] + n.nz * m[2];
    for (int row = 0; row < 3; ++row) {
      j[row][col] = (m[row] - n.nx * (n.nx * m[0] + n.ny * m[1] + n.nz * m[2]) * (row == 0 ? 1.0 : 0.0) -
                     n.ny * (n.nx * m[0] + n.ny * m[1] + n.nz * m[2]) * (row == 1 ? 1.0 : 0.0) -
                     n.nz * (n.nx * m[0] + n.ny * m[1] + n.nz * m[2]) * (row == 2 ? 1.0 : 0.0)) /
                    s;
    }
    (void)t;
  }
}

void jacobian_interval_iv(const WcsModel& wcs, Iv u, Iv v, Iv jacobian[3][2]) {
  const Iv a = sip_value_iv(wcs.sip_a, u, v);
  const Iv b = sip_value_iv(wcs.sip_b, u, v);
  const Iv x = numerics::interval_add(u, a);
  const Iv y = numerics::interval_add(v, b);
  const Mat2& cd = wcs.cd_rad_per_pixel;
  const Iv qx = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), x),
                                       numerics::interval_mul(Iv(cd[1], cd[1]), y));
  const Iv qy = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), x),
                                       numerics::interval_mul(Iv(cd[3], cd[3]), y));
  const Iv qq = numerics::interval_add(numerics::interval_mul(qx, qx), numerics::interval_mul(qy, qy));
  const Iv s = numerics::interval_sqrt(numerics::interval_add(Iv(1.0, 1.0), qq));
  Iv nx;
  Iv ny;
  Iv nz;
  forward_interval_iv(wcs, u, v, nx, ny, nz);

  const Iv au = sip_grad_u_iv(wcs.sip_a, u, v);
  const Iv av = sip_grad_v_iv(wcs.sip_a, u, v);
  const Iv bu = sip_grad_u_iv(wcs.sip_b, u, v);
  const Iv bv = sip_grad_v_iv(wcs.sip_b, u, v);
  const Iv j00 = numerics::interval_add(Iv(1.0, 1.0), au);
  const Iv j11 = numerics::interval_add(Iv(1.0, 1.0), bv);
  Iv k[2][2];
  k[0][0] = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), j00),
                                   numerics::interval_mul(Iv(cd[1], cd[1]), bu));
  k[0][1] = numerics::interval_add(numerics::interval_mul(Iv(cd[0], cd[0]), av),
                                   numerics::interval_mul(Iv(cd[1], cd[1]), j11));
  k[1][0] = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), j00),
                                   numerics::interval_mul(Iv(cd[3], cd[3]), bu));
  k[1][1] = numerics::interval_add(numerics::interval_mul(Iv(cd[2], cd[2]), av),
                                   numerics::interval_mul(Iv(cd[3], cd[3]), j11));
  const double e[3] = {wcs.e[0], wcs.e[1], wcs.e[2]};
  const double nn[3] = {wcs.north[0], wcs.north[1], wcs.north[2]};
  for (int col = 0; col < 2; ++col) {
    Iv m[3];
    for (int row = 0; row < 3; ++row) {
      m[row] = numerics::interval_add(numerics::interval_mul(Iv(e[row], e[row]), k[0][col]),
                                      numerics::interval_mul(Iv(nn[row], nn[row]), k[1][col]));
    }
    Iv t(0.0, 0.0);
    t = numerics::interval_add(t, numerics::interval_mul(nx, m[0]));
    t = numerics::interval_add(t, numerics::interval_mul(ny, m[1]));
    t = numerics::interval_add(t, numerics::interval_mul(nz, m[2]));
    for (int row = 0; row < 3; ++row) {
      const Iv n_row = row == 0 ? nx : (row == 1 ? ny : nz);
      jacobian[row][col] = numerics::interval_div(numerics::interval_sub(m[row],
                                                                          numerics::interval_mul(n_row, t)),
                                                   s);
    }
  }
}

}  // namespace acsd::coordinates
