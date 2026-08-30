// AstroCS Phase1 — P1-004 Photometry aperture 积分 (SCI-PHOT-001)
// 已知 flux/背景/PSF; "解析成功但积分失败"必须回归(积分校验)。
// 失败不留貌似有效的空 catalog。
#pragma once

#include "astrocs/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::phase1 {

struct PhotometryResult {
  double flux = 0.0;         // ADU (背景扣除后 aperture 积分)
  double flux_error = 0.0;   // ADU
  double background = 0.0;   // ADU/px
  double snr = 0.0;
  bool valid = false;        // 积分成功标志 (失败显式 false, 不伪造有效)
  std::string failure_reason;  // 失败原因 (空串=成功)
};

// Photometer: aperture 光栅积分; 失败显式置 valid=false + reason (不留空 catalog)。
class Photometer {
 public:
  explicit Photometer(double aperture_radius_px = 4.0,
                      double sky_annulus_inner = 6.0,
                      double sky_annulus_outer = 10.0);

  // measure: image 行主序; (cx,cy) 中心; 返回结果。
  astrocs::core::Result<PhotometryResult> measure(
      const float* image, int w, int h, double cx, double cy) const;

 private:
  double aperture_radius_;
  double sky_inner_;
  double sky_outer_;
};

}  // namespace astrocs::phase1
