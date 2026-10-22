#pragma once
// Canonical two dimensional TAN (+ optional SIP) model.
//
// The scientific definitions are owned by
// docs/science/foundation/OBSERVATION_COORDINATES.md (sections 2, 3, 3.1,
// 3.2, 3.3).  This header only exposes the internal helpers that the
// coordinates module shares with the geometry module; the public fields and
// function signatures stay in include/acsd/contracts.hpp.
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/interval.hpp"

namespace acsd::coordinates {

using Iv = numerics::Interval;

// ---------------------------------------------------------------------------
// Polynomial helpers over the public Polynomial2D type.
// ---------------------------------------------------------------------------

double sip_value(const Polynomial2D& poly, double u, double v);
void sip_grad(const Polynomial2D& poly, double u, double v, double& du, double& dv);
void sip_hessian(const Polynomial2D& poly, double u, double v, double& duu, double& duv, double& dvv);

Iv sip_value_iv(const Polynomial2D& poly, Iv u, Iv v);
Iv sip_grad_u_iv(const Polynomial2D& poly, Iv u, Iv v);
Iv sip_grad_v_iv(const Polynomial2D& poly, Iv u, Iv v);

// ---------------------------------------------------------------------------
// Local model evaluation (all quantities in the canonical model of WcsModel).
// ---------------------------------------------------------------------------

// Detector pixel (PixelCenter0 / PixelEdge0 share the same continuous axis)
// to the SIP base variables u = p0 + 1 - CRPIX.
inline Vec2 pixel_to_uv(const WcsModel& wcs, double x, double y) {
  return {x + 1.0 - wcs.crpix_fits1[0], y + 1.0 - wcs.crpix_fits1[1]};
}

// q = CD_rad * (u + A, v + B).
Vec2 tangent_offset(const WcsModel& wcs, double u, double v);

// Unit direction of the tangent plane offset q.
SkyICRS direction_from_tangent(const WcsModel& wcs, const Vec2& q);

// g = |det CD| * |det J_F| / (1 + q.q)^(3/2)   [sr / pixel^2]
double solid_angle_density(const WcsModel& wcs, double u, double v);
Iv solid_angle_density_iv(const WcsModel& wcs, Iv u, Iv v);

// Gradient of g with respect to the SIP base variables (u, v).  It is
// generated analytically from the same model as the value: the SIP Hessian and
// the normalised tangent plane, never a finite difference.
void solid_angle_density_gradient(const WcsModel& wcs, double u, double v, double& g, double& dgu,
                                  double& dgv);

// Unit direction and its enclosure over an interval of the base variables.
void forward_interval_iv(const WcsModel& wcs, Iv u, Iv v, Iv& nx, Iv& ny, Iv& nz);

// J_W = d n / d (x, y) as a 3x2 row major matrix.
void jacobian(const WcsModel& wcs, double u, double v, double jacobian[3][2]);
void jacobian_interval_iv(const WcsModel& wcs, Iv u, Iv v, Iv jacobian[3][2]);

// det(CD) with the sign of the handiness preserved.
double cd_det(const Mat2& cd);

// Induced infinity norm of a 2x2 matrix stored row major.
double mat2_norm_inf(const Mat2& m);

}  // namespace acsd::coordinates
