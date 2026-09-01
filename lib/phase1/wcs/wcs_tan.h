// AstroCS Phase1 — WCS TAN 投影 (SCI-WCS-001)
// pixel <-> sky roundtrip (ICRS, deg)。合同: 已知 WCS 参数往返误差 < 1e-6 deg。
#pragma once

#include <cmath>

namespace astrocs::phase1 {

// TAN 投影 WCS (简化: 无畸变; 满足合成星场 roundtrip 验证)
struct WcsTan {
  double crpix1 = 0, crpix2 = 0;   // 参考像素 (1-based)
  double crval1 = 0, crval2 = 0;   // 参考天球坐标 (deg, RA/Dec)
  double cd11 = 0, cd12 = 0;       // CD 矩阵 (deg/px)
  double cd21 = 0, cd22 = 0;

  // pixel -> sky (ICRS deg)
  void pix2sky(double x, double y, double* ra, double* dec) const;
  // sky -> pixel
  void sky2pix(double ra, double dec, double* x, double* y) const;
};

}  // namespace astrocs::phase1
