// P1-003 StarDetector 实现
#include "star_detector.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

namespace astrocs::phase1 {

using astrocs::core::Error;
using astrocs::core::ErrorDomain;

StarDetector::StarDetector(double detection_sigma) : detection_sigma_(detection_sigma) {}

bool StarDetector::estimate_background(const float* image, int w, int h,
                                       double* bg, double* sigma) {
  if (!image || w <= 0 || h <= 0 || !bg || !sigma) return false;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<double> vals(n);
  for (size_t i = 0; i < n; ++i) vals[i] = image[i];
  // sigma-clip 2 轮: median ± 3σ
  std::vector<double> keep = vals;
  for (int round = 0; round < 2; ++round) {
    std::vector<double> sorted = keep;
    std::sort(sorted.begin(), sorted.end());
    const double med = sorted[sorted.size() / 2];
    std::vector<double> dev;
    dev.reserve(keep.size());
    for (double v : keep) dev.push_back(std::fabs(v - med));
    std::sort(dev.begin(), dev.end());
    const double mad = dev[dev.size() / 2];
    const double s = 1.4826 * mad;
    std::vector<double> filtered;
    filtered.reserve(keep.size());
    for (double v : keep)
      if (std::fabs(v - med) <= 3.0 * (s > 0 ? s : 1e-9)) filtered.push_back(v);
    if (filtered.empty()) break;
    keep = std::move(filtered);
  }
  std::vector<double> sorted = keep;
  std::sort(sorted.begin(), sorted.end());
  *bg = sorted[sorted.size() / 2];
  double sum = 0;
  for (double v : keep) sum += (v - *bg) * (v - *bg);
  *sigma = std::sqrt(sum / static_cast<double>(keep.size() > 0 ? keep.size() : 1));
  if (*sigma < 1e-9) *sigma = 1e-9;
  return true;
}

astrocs::core::Result<StarCatalog> StarDetector::detect(const float* image, int w, int h) const {
  if (!image || w <= 0 || h <= 0) {
    return astrocs::core::Result<StarCatalog>::fail(
        Error(ErrorDomain::DATA, "star_detector: bad image dims"));
  }
  StarCatalog cat;
  if (!estimate_background(image, w, h, &cat.background, &cat.noise_sigma)) {
    return astrocs::core::Result<StarCatalog>::fail(
        Error(ErrorDomain::DATA, "star_detector: background estimation failed"));
  }
  const double thr = cat.background + detection_sigma_ * cat.noise_sigma;

  // 1) 局部峰候选: 3x3 局部最大且 > thr
  struct Cand { int x, y; double val; };
  std::vector<Cand> cands;
  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      const double v = image[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
      if (v < thr) continue;
      bool local_max = true;
      for (int dy = -1; dy <= 1 && local_max; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) continue;
          if (image[static_cast<size_t>(y + dy) * static_cast<size_t>(w) + static_cast<size_t>(x + dx)] >= v) { local_max = false; break; }
        }
      if (local_max) cands.push_back({x, y, v});
    }
  }

  // 2) 去重: flux 降序 (tie breaker: 更左优先); 邻域 3x3 内只留最强
  std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
    if (a.val != b.val) return a.val > b.val;
    return a.x < b.x || (a.x == b.x && a.y < b.y);
  });
  std::vector<Cand> kept;
  std::vector<std::vector<bool>> taken(static_cast<size_t>(h),
                                       std::vector<bool>(static_cast<size_t>(w), false));
  for (const auto& c : cands) {
    if (taken[static_cast<size_t>(c.y)][static_cast<size_t>(c.x)]) continue;
    kept.push_back(c);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        int ny = c.y + dy, nx = c.x + dx;
        if (ny >= 0 && ny < h && nx >= 0 && nx < w)
          taken[static_cast<size_t>(ny)][static_cast<size_t>(nx)] = true;
      }
  }

  // 3) 每候选: 质心 + 二阶矩 (FWHM/ellipticity) + 质量位
  uint32_t idx = 0;
  for (const auto& c : kept) {
    StarSource s;
    s.x = c.x; s.y = c.y;
    // 5x5 窗口质心 (背景扣除)
    double m00 = 0, m10 = 0, m01 = 0, m20 = 0, m02 = 0, m11 = 0;
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) {
        int ny = c.y + dy, nx = c.x + dx;
        if (ny < 0 || ny >= h || nx < 0 || nx >= w) { s.quality |= 2; continue; }  // 边缘
        const double v = image[static_cast<size_t>(ny) * static_cast<size_t>(w) + static_cast<size_t>(nx)] - cat.background;
        if (v <= 0) continue;
        const double px = nx, py = ny;
        m00 += v; m10 += v * px; m01 += v * py;
        m20 += v * px * px; m02 += v * py * py; m11 += v * px * py;
      }
    if (m00 <= 0) continue;
    s.flux = m00;
    s.x = m10 / m00; s.y = m01 / m00;
    const double mu20 = m20 / m00 - s.x * s.x;
    const double mu02 = m02 / m00 - s.y * s.y;
    const double mu11 = m11 / m00 - s.x * s.y;
    const double theta = 0.5 * std::atan2(2 * mu11, mu20 - mu02);
    const double cos2 = std::cos(theta), sin2 = std::sin(theta);
    const double a2 = mu20 * cos2 * cos2 + 2 * mu11 * sin2 * cos2 + mu02 * sin2 * sin2;
    const double b2 = mu20 * sin2 * sin2 - 2 * mu11 * sin2 * cos2 + mu02 * cos2 * cos2;
    const double a = std::sqrt(std::max(a2, 1e-12));
    const double b = std::sqrt(std::max(b2, 1e-12));
    s.fwhm_px = 2.3548 * 0.5 * (a + b);
    s.ellipticity = (a >= b) ? (1.0 - b / a) : (1.0 - a / b);
    const double peak = image[static_cast<size_t>(c.y) * static_cast<size_t>(w) + static_cast<size_t>(c.x)];
    s.snr = (peak - cat.background) / cat.noise_sigma;
    // 饱和: 绝对幅值接近/超过 16bit 满井 (ADU 域; 不因高 SNR 误判)
    if (peak > 50000.0) s.quality |= 1;
    s.id = "src-" + std::to_string(idx++);
    cat.sources.push_back(std::move(s));
    if (s.quality & 1) ++cat.n_saturated;
    if (s.quality & 2) ++cat.n_edge;
  }
  cat.n_detected = static_cast<uint32_t>(cat.sources.size());
  return astrocs::core::Result<StarCatalog>::ok(std::move(cat));
}

}  // namespace astrocs::phase1
