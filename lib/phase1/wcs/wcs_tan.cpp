// P1-004 WCS TAN 实现
#include "wcs_tan.h"

#include <cmath>

namespace astrocs::phase1 {

void WcsTan::pix2sky(double x, double y, double* ra, double* dec) const {
  const double dx = x - crpix1;
  const double dy = y - crpix2;
  const double xi = cd11 * dx + cd12 * dy;
  const double eta = cd21 * dx + cd22 * dy;
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
  const double eta = (c0 * sd - s0 * cd * std::cos(dra)) / den;
  const double xi2 = (cd * std::sin(dra)) / den;
  // 逆 CD: [dx dy]^T = CD^-1 [xi eta]^T
  const double det = cd11 * cd22 - cd12 * cd21;
  if (std::fabs(det) < 1e-30) { if (x) *x = crpix1; if (y) *y = crpix2; return; }
  if (x) *x = crpix1 + (cd22 * xi2 - cd12 * eta) / det;
  if (y) *y = crpix2 + (-cd21 * xi2 + cd11 * eta) / det;
}

}  // namespace astrocs::phase1
