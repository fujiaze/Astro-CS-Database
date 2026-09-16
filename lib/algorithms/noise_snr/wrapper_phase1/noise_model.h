// AstroCS Phase1 — Noise/SNR (SCI-NOISE-001)
// SCI 公式 reference: σ_bg = 1.482602218505602·MAD; variance; ivar=1/variance。
// 合同: variance 与 inverse variance 不混; blank sky/低高信号/负值/零 gain/无效 read noise。
#pragma once

#include "astrocs/core/contracts.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::phase1 {

// SCI-NOISE-001 常数 (冻结)
constexpr double kMadToSigma = 1.482602218505602;
constexpr double kVarianceFloor = 1e-12;

struct NoiseResult {
  double variance = 0.0;       // ADU² (随机分量)
  double ivar = 0.0;           // 1/variance (ADU⁻²)
  double sigma = 0.0;          // √variance (ADU)
  double background = 0.0;     // ADU (median)
  bool valid = false;
  std::string reason;          // 无效原因 (空=成功)
};

// NoiseModel: 输入像素集 → variance/ivar (SCI 公式; 不做诊断 gain 模型)。
class NoiseModel {
 public:
  // estimate: 空白背景像素集 (ADU); 返回 variance/ivar。
  astrocs::core::Result<NoiseResult> estimate(const std::vector<float>& pixels) const;

  // Poisson+read noise 解析模型 (SCI §10 诊断): variance = signal/gain + read_noise²
  // 零 gain 或无效 read_noise (<=0) → 返回无效 (诊断路径不入生产)。
  static astrocs::core::Result<NoiseResult> gain_variance(
      double signal, double gain, double read_noise_e);
};

}  // namespace astrocs::phase1
