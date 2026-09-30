// ACSD Phase1 — StarDetector 桥接层实现 (SCI-PSF-001 / SCI-PHOT-001)
// 场景: 孤立 Gaussian/Moffat、重叠星、饱和星、边缘星、纯噪声。
// 合同: 输入图像 f32 (ADU) + 背景估计; 输出 catalog (坐标/单位/质量字段)。
#pragma once

#include "acsd/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace acsd::phase1 {

struct StarSource {
  double x = 0.0;          // 像素坐标 (单位: px, 原点左上)
  double y = 0.0;
  double flux = 0.0;       // 单位: ADU (5x5 窗内背景扣除后的积分)
  // 单位 px。定义域 = **二阶矩高斯等效宽度**: fwhm_px = 2.3548 * 0.5*(a+b),
  // a/b 为 5x5 窗二阶协方差阵的主/次轴标准差。它**不是** PSF 拟合宽度:
  // 生产检测块 (star_det v1) 的 fwhm 是椭圆高斯**拟合**值 (2.3548*sigma_fit),
  // PSF 块是椭圆 Moffat4 (1.230310*sigma_moffat)。三者同列名、不同母函数,
  // 按 SCI-P1-STAR-001 §2 (DISP-STAR-007) 不可跨块比较; 跨块混用须先换算
  // (同 sx 下 FWHM_gauss/FWHM_moffat = 2.354820/1.230310 = 1.9140)。
  double fwhm_px = 0.0;
  double ellipticity = 0.0;  // 1 - b/a
  // 无量纲。定义 = (峰值像素 - 帧背景中位) / noise_sigma。
  // ⚠ 两点必须随引用同读 (SCI-P1-STAR-001 §5; ALG-STARDET-001 §11.4 F1):
  //   ① 分母 noise_sigma = 本类 estimate_background 的**第三 σ 估计器**
  //      (2 轮 median±3σ 裁剪后残差 RMS), **不是**未平滑原图的行差分
  //      bgnoise (FnNoise1 族)。两者同帧实测不相等, 凡写「sigma_bg」必须点名。
  //   ② 分子是**原始峰值像素减背景**, 不是拟合振幅 A_fit。本域唯一的
  //      SNR_peak 定义是 A_fit/σ_bg, 与此不同, 两口径不可互相代用。
  double snr = 0.0;
  uint8_t quality = 0;     // 质量位: 1=饱和 2=边缘 0=干净 (4=重叠 当前无生产者)
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
  acsd::core::Result<StarCatalog> detect(const float* image, int w, int h) const;

  // 工具: 背景/噪声估计 (sigma-clipped median + MAD)
  static bool estimate_background(const float* image, int w, int h,
                                  double* bg, double* sigma);

 private:
  double detection_sigma_;
};

}  // namespace acsd::phase1
