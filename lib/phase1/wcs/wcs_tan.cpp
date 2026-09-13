// P1-004 WCS TAN 实现
#include "wcs_tan.h"

#include <cmath>

namespace astrocs::phase1 {

void WcsTan::pix2sky(double x, double y, double* ra, double* dec) const {
  const double dx = x - crpix1;
  const double dy = y - crpix2;
  // B2-A1 (AUD-COORD F-01): CD 矩阵单位 deg/px ⇒ ξ/η 为**度**；TAN
  // (gnomonic) 反投影球面公式在**弧度**域，进入前必须显式 ·π/180。
  // 权威: FITS-WCS Paper I §2.2 (pixel → intermediate world) +
  // Paper II (Calabretta & Greisen 2002) TAN 反投影。
  // 同式参照: lib/photometric_calib/cpp/src/wcs_transform.cpp:110-111
  // (xi_rad = xi·D2R)、lib/snr_estimator/cpp/src/snr_estimator.cpp:449-450。
  const double d2r = M_PI / 180.0;
  const double xi = (cd11 * dx + cd12 * dy) * d2r;
  const double eta = (cd21 * dx + cd22 * dy) * d2r;
  const double R = std::sqrt(xi * xi + eta * eta);
  const double dec0 = crval2 * M_PI / 180.0;
  const double ra0 = crval1 * M_PI / 180.0;
  double dec_out, ra_out;
  if (R < 1e-12) {
    dec_out = dec0;
    ra_out = ra0;
  } else {
    const double rho = std::atan(R);
    const double cr = std::cos(rho), sr = std::sin(rho);
    dec_out = std::asin(cr * std::sin(dec0) +
                        (eta * sr * std::cos(dec0)) / R);
    ra_out = ra0 + std::atan2(xi * sr,
                              R * std::cos(dec0) * cr - eta * std::sin(dec0) * sr);
  }
  if (ra_out > M_PI) ra_out -= 2 * M_PI;
  if (ra_out < -M_PI) ra_out += 2 * M_PI;
  if (dec) *dec = dec_out * 180.0 / M_PI;
  if (ra) *ra = ra_out * 180.0 / M_PI;
}

void WcsTan::sky2pix(double ra, double dec, double* x, double* y) const {
  const double dec0 = crval2 * M_PI / 180.0;
  double dra = (ra - crval1) * M_PI / 180.0;
  while (dra > M_PI) dra -= 2 * M_PI;
  while (dra < -M_PI) dra += 2 * M_PI;
  const double ddec = dec * M_PI / 180.0;
  // 标准 TAN: xi = cos(dec)*sin(dra) / (sin(dec0)*sin(dec) + cos(dec0)*cos(dec)*cos(dra))
  // eta = (cos(dec0)*sin(dec) - sin(dec0)*cos(dec)*cos(dra)) / 同分母
  const double c0 = std::cos(dec0), s0 = std::sin(dec0);
  const double cd = std::cos(ddec), sd = std::sin(ddec);
  const double den = s0 * sd + c0 * cd * std::cos(dra);
  // 上式分子分母同为弧度商 ⇒ (xi2, eta) 为**弧度**域标准坐标; CD 矩阵为
  // deg/px, 故逆投影前必须显式 ×180/π 回到度域 (B2-A1: 修正与 pix2sky
  // 成对的 deg/rad 单位错; 否则正反两个错误相消, roundtrip 自洽掩盖 —
  // AUD-COORD F-01/F-06)。权威: FITS-WCS Paper I §2.2。
  const double r2d = 180.0 / M_PI;
  const double eta_deg = (c0 * sd - s0 * cd * std::cos(dra)) / den * r2d;
  const double xi_deg = (cd * std::sin(dra)) / den * r2d;
  // 逆 CD: [dx dy]^T = CD^-1 [xi eta]^T (度域)
  const double det = cd11 * cd22 - cd12 * cd21;
  if (std::fabs(det) < 1e-30) { if (x) *x = crpix1; if (y) *y = crpix2; return; }
  if (x) *x = crpix1 + (cd22 * xi_deg - cd12 * eta_deg) / det;
  if (y) *y = crpix2 + (-cd21 * xi_deg + cd11 * eta_deg) / det;
}

}  // namespace astrocs::phase1
