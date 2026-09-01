// P1-005 NoiseModel 实现 (SCI-NOISE-001)
#include "noise_model.h"

#include <algorithm>
#include <cmath>

namespace astrocs::phase1 {

using astrocs::core::Error;
using astrocs::core::ErrorDomain;

astrocs::core::Result<NoiseResult> NoiseModel::estimate(
    const std::vector<float>& pixels) const {
  NoiseResult r;
  if (pixels.empty()) {
    r.valid = false;
    r.reason = "empty pixel set";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  // median + MAD -> sigma (SCI 公式)
  std::vector<double> vals;
  vals.reserve(pixels.size());
  for (float v : pixels) {
    if (std::isfinite(v)) vals.push_back(v);
  }
  if (vals.size() < 3) {
    r.valid = false;
    r.reason = "insufficient finite pixels";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  std::vector<double> sorted = vals;
  std::sort(sorted.begin(), sorted.end());
  r.background = sorted[sorted.size() / 2];
  std::vector<double> dev;
  dev.reserve(vals.size());
  for (double v : vals) dev.push_back(std::fabs(v - r.background));
  std::sort(dev.begin(), dev.end());
  const double mad = dev[dev.size() / 2];
  r.sigma = kMadToSigma * mad;
  r.variance = r.sigma * r.sigma;
  if (r.variance < kVarianceFloor) r.variance = kVarianceFloor;  // clamp (SCI §4)
  r.ivar = 1.0 / r.variance;
  r.valid = true;
  return astrocs::core::Result<NoiseResult>::ok(r);
}

astrocs::core::Result<NoiseResult> NoiseModel::gain_variance(
    double signal, double gain, double read_noise_e) {
  NoiseResult r;
  // 零 gain 或无效 read noise (<=0): 诊断路径不入生产 (SCI §4)
  if (gain <= 0.0 || read_noise_e <= 0.0 || !std::isfinite(signal)) {
    r.valid = false;
    r.reason = (gain <= 0.0) ? "zero/invalid gain"
              : (read_noise_e <= 0.0) ? "invalid read_noise" : "invalid signal";
    return astrocs::core::Result<NoiseResult>::ok(r);
  }
  // variance = signal/gain + read_noise² (e⁻ 域 → ADU²)
  double var = signal / gain + read_noise_e * read_noise_e / (gain * gain);
  if (var < kVarianceFloor) var = kVarianceFloor;
  r.variance = var;
  r.ivar = 1.0 / var;
  r.sigma = std::sqrt(var);
  r.valid = true;
  return astrocs::core::Result<NoiseResult>::ok(r);
}

}  // namespace astrocs::phase1
