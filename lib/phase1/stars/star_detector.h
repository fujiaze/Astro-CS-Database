// AstroCS Phase1 — P1-003 StarDetector (SCI-PSF-001 / SCI-PHOT-001)
// 场景: 孤立 Gaussian/Moffat、重叠星、饱和星、边缘星、纯噪声。
// 合同: 输入图像 f32 (ADU) + 背景估计; 输出 catalog (坐标/单位/质量字段)。
#pragma once

#include "astrocs/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::phase1 {

struct StarSource {
  double x = 0.0;          // 像素坐标 (单位: px, 原点左上)
  double y = 0.0;
  double flux = 0.0;       // 单位: ADU (积分)
  double fwhm_px = 0.0;    // 单位: px
  double ellipticity = 0.0;  // 1 - b/a
  double snr = 0.0;        // 检测 SNR
  uint8_t quality = 0;     // 质量位: 1=饱和 2=边缘 4=重叠 0=干净
  std::string id;          // "src-<idx>"
};

struct StarCatalog {
  std::vector<StarSource> sources;
  double background = 0.0;   // ADU
  double noise_sigma = 0.0;  // ADU
  uint32_t n_detected = 0;
  uint32_t n_saturated = 0;
  uint32_t n_edge = 0;
};

// StarDetector: 局部峰 + 质心/二阶矩; 去重带 tie breaker (flux 降序, 同 flux 取更左)。
// 纯噪声场景: 无显著峰 → 空 catalog (不误报)。
class StarDetector {
 public:
  explicit StarDetector(double detection_sigma = 5.0);

  // image: f32 行主序 w*h; detect 返回 catalog (失败→Result error)。
  astrocs::core::Result<StarCatalog> detect(const float* image, int w, int h) const;

  // 工具: 背景/噪声估计 (sigma-clipped median + MAD)
  static bool estimate_background(const float* image, int w, int h,
                                  double* bg, double* sigma);

 private:
  double detection_sigma_;
};

}  // namespace astrocs::phase1
