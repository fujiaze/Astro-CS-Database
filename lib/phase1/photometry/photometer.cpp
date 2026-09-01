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
  std::vector<double> sky_vals;
  sky_vals.reserve(1024);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
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

  // 2) aperture 积分 (背景扣除)
  double sum = 0;
  int n_in = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
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
    sky_sigma = 1.4826 * dev[dev.size() / 2];
  }
  r.flux_error = std::sqrt(std::max(sum, 0.0) + n_in * sky_sigma * sky_sigma);
  r.snr = r.flux_error > 0 ? r.flux / r.flux_error : 0.0;
  r.valid = true;
  return astrocs::core::Result<PhotometryResult>::ok(r);
}

}  // namespace astrocs::phase1
