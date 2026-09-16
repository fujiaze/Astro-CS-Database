// P1-004 Photometer 实现
#include "photometer.h"

#include <algorithm>
#include <cmath>

namespace astrocs::phase1 {

using astrocs::core::Error;
using astrocs::core::ErrorDomain;

Photometer::Photometer(double aperture_radius_px, double sky_annulus_inner,
                       double sky_annulus_outer)
    : aperture_radius_(aperture_radius_px), sky_inner_(sky_annulus_inner),
      sky_outer_(sky_annulus_outer) {}

astrocs::core::Result<PhotometryResult> Photometer::measure(
    const float* image, int w, int h, double cx, double cy) const {
  if (!image || w <= 0 || h <= 0) {
    return astrocs::core::Result<PhotometryResult>::fail(
        Error(ErrorDomain::DATA, "photometer: bad image dims"));
  }
  PhotometryResult r;

  // 中心合法性: 必须在图像内 (越界 = 显式失败, 不留貌似有效结果)
  if (cx < 0 || cx >= w || cy < 0 || cy >= h) {
    r.valid = false;
    r.failure_reason = "center out of bounds";
    return astrocs::core::Result<PhotometryResult>::ok(r);  // 合法结果含失败标志
  }

  // 1) sky 环背景: 中位数 (annulus 内像素)
  // 真实链路性能修复: 原实现对每个源都整帧扫描两次 (w*h), 145k 源 × 16M px
  // ⇒ 数小时不可完成 (真实 CLI phase1 从未跑完 phot 节点)。此处仅把循环界收缩
  // 到 annulus/aperture 的外接矩形 (±1 px 余量), 逐像素判定与外接矩形外的贡献
  // 集完全相同 → sky_vals/sum 的取值与行主序累加顺序不变, 结果逐位一致;
  // 科学公式、容差、边界判定一律未改。
  const int y0_sky = std::max(0, static_cast<int>(std::floor(cy - sky_outer_)) - 1);
  const int y1_sky = std::min(h - 1, static_cast<int>(std::ceil(cy + sky_outer_)) + 1);
  const int x0_sky = std::max(0, static_cast<int>(std::floor(cx - sky_outer_)) - 1);
  const int x1_sky = std::min(w - 1, static_cast<int>(std::ceil(cx + sky_outer_)) + 1);
  std::vector<double> sky_vals;
  sky_vals.reserve(1024);
  for (int y = y0_sky; y <= y1_sky; ++y)
    for (int x = x0_sky; x <= x1_sky; ++x) {
      const double d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      const double d = std::sqrt(d2);
      if (d >= sky_inner_ && d <= sky_outer_) {
        sky_vals.push_back(image[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]);
      }
    }
  if (sky_vals.empty()) {
    r.valid = false;
    r.failure_reason = "no sky annulus pixels";
    return astrocs::core::Result<PhotometryResult>::ok(r);
  }
  std::sort(sky_vals.begin(), sky_vals.end());
  r.background = sky_vals[sky_vals.size() / 2];

  // 2) aperture 积分 (背景扣除) —— 循环界收缩同 §1 (结果逐位一致)
  double sum = 0;
  int n_in = 0;
  const int y0_ap = std::max(0, static_cast<int>(std::floor(cy - aperture_radius_)) - 1);
  const int y1_ap = std::min(h - 1, static_cast<int>(std::ceil(cy + aperture_radius_)) + 1);
  const int x0_ap = std::max(0, static_cast<int>(std::floor(cx - aperture_radius_)) - 1);
  const int x1_ap = std::min(w - 1, static_cast<int>(std::ceil(cx + aperture_radius_)) + 1);
  for (int y = y0_ap; y <= y1_ap; ++y)
    for (int x = x0_ap; x <= x1_ap; ++x) {
      const double d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
      if (d2 <= aperture_radius_ * aperture_radius_) {
        const double v = image[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] - r.background;
        sum += v;
        ++n_in;
      }
    }
  if (n_in <= 0) {
    r.valid = false;
    r.failure_reason = "aperture empty";
    return astrocs::core::Result<PhotometryResult>::ok(r);
  }
  r.flux = sum;
  // Poisson + read noise 简化误差: sqrt(sum + n*sigma_sky^2)
  double sky_sigma = 0;
  {
    const size_t n = sky_vals.size();
    const double med = sky_vals[n / 2];
    std::vector<double> dev;
    for (double v : sky_vals) dev.push_back(std::fabs(v - med));
    std::sort(dev.begin(), dev.end());
    // MAD→σ 冻结常数 (NOISE_MODEL.md §5:46 = 1/Φ⁻¹(3/4); 原 4 位截断 1.4826
    // 相对差 -1.50e-6, 同一模块内不得多套常数 — M3-A-006)
    sky_sigma = 1.482602218505602 * dev[dev.size() / 2];
  }
  r.flux_error = std::sqrt(std::max(sum, 0.0) + n_in * sky_sigma * sky_sigma);
  r.snr = r.flux_error > 0 ? r.flux / r.flux_error : 0.0;
  r.valid = true;
  return astrocs::core::Result<PhotometryResult>::ok(r);
}

}  // namespace astrocs::phase1
