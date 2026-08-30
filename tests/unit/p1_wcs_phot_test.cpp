// P1-004 单元测试: WCS roundtrip + Photometry + 失败不留空 catalog
#include "photometer.h"
#include "wcs_tan.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using astrocs::phase1::Photometer;
using astrocs::phase1::PhotometryResult;
using astrocs::phase1::WcsTan;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void add_psf(std::vector<float>& img, int w, int h,
                    double cx, double cy, double flux, double sigma, double bg) {
  // 高斯 PSF: 幅值=flux, 覆盖式写入 (图像已预填 bg)
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      double dx = x - cx, dy = y - cy;
      double g = std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
      img[static_cast<size_t>(y) * w + x] = static_cast<float>(bg + flux * g);
    }
}

static void test_wcs_roundtrip() {
  // 已知 WCS: 参考 (RA=150.0, Dec=+2.0), 尺度 0.5"/px → ~1.389e-4 deg/px
  WcsTan w;
  w.crpix1 = 512; w.crpix2 = 384;
  w.crval1 = 150.0; w.crval2 = 2.0;
  const double scale = 0.5 / 3600.0;  // 0.5 arcsec = deg
  w.cd11 = scale; w.cd12 = 0; w.cd21 = 0; w.cd22 = scale;

  // 像素网格 -> sky -> pixel roundtrip
  for (int i = 0; i < 8; ++i) {
    double x = 100.0 + i * 100.0;
    double y = 50.0 + i * 80.0;
    double ra, dec;
    w.pix2sky(x, y, &ra, &dec);
    double x2, y2;
    w.sky2pix(ra, dec, &x2, &y2);
    if (std::fabs(x2 - x) > 1e-6 || std::fabs(y2 - y) > 1e-6) {
      std::fprintf(stderr, "wcs roundtrip fail at (%f,%f): got (%f,%f)\n", x, y, x2, y2);
      ++failures; break;
    }
  }
  // 参考像素 -> crval
  double ra0, dec0;
  w.pix2sky(w.crpix1, w.crpix2, &ra0, &dec0);
  CHECK(std::fabs(ra0 - 150.0) < 1e-6);
  CHECK(std::fabs(dec0 - 2.0) < 1e-6);
}

static void test_photometry_known_flux() {
  const int w = 64, h = 64;
  const double bg = 100.0, sigma = 2.0, flux = 5000.0;
  std::vector<float> img(static_cast<size_t>(w) * h, static_cast<float>(bg));
  add_psf(img, w, h, 32.0, 32.0, flux, sigma, bg);
  Photometer phot;
  auto r = phot.measure(img.data(), w, h, 32.0, 32.0);
  CHECK(r.ok());
  if (r.ok()) {
    PhotometryResult& m = r.value();
    CHECK(m.valid);
    // 已知 PSF 解析总通量 = amp * 2πσ² (高斯积分); aperture r=4 捕获 ~85-95%
    const double analytic_total = flux * 2.0 * M_PI * sigma * sigma;
    CHECK(std::fabs(m.flux - analytic_total) / analytic_total < 0.20);
    CHECK(std::fabs(m.background - bg) < 5.0);
    CHECK(m.snr > 3.0);
  }
}

static void test_photometry_out_of_bounds_fails() {
  // 失败不留貌似有效的空 catalog: 越界 → valid=false + reason
  const int w = 32, h = 32;
  std::vector<float> img(static_cast<size_t>(w) * h, 10.0f);
  Photometer phot;
  auto r = phot.measure(img.data(), w, h, -5.0, 16.0);
  CHECK(r.ok());  // 结果合法
  CHECK(!r.value().valid);   // 但显式失败
  CHECK(!r.value().failure_reason.empty());
}

static void test_photometry_integration_regression() {
  // "解析成功但积分失败"回归 fixture: 中心在图像边缘 → 积分不完整但仍 valid,
  // 但越界中心必须显式失败 (不留貌似有效 catalog)
  const int w = 16, h = 16;
  const double bg = 50.0, flux = 1000.0;
  std::vector<float> img(static_cast<size_t>(w) * h, static_cast<float>(bg));
  add_psf(img, w, h, 15.5, 8.0, flux, 1.5, bg);  // 边缘中心
  Photometer phot;
  auto r = phot.measure(img.data(), w, h, 15.5, 8.0);
  CHECK(r.ok());
  if (r.ok()) {
    // 边缘中心仍在图像内 → valid 但 flux 低估 (aperture 截断); 不得误报成功为空
    CHECK(r.value().valid);
    CHECK(r.value().flux > 0);
  }
}

static void test_photometry_failed_center_explicit() {
  const int w = 16, h = 16;
  std::vector<float> img(static_cast<size_t>(w) * h, 0.0f);
  Photometer phot;
  // 中心完全出界 (y=100)
  auto r = phot.measure(img.data(), w, h, 8.0, 100.0);
  CHECK(r.ok());
  CHECK(!r.value().valid);
  CHECK(r.value().failure_reason.find("out of bounds") != std::string::npos);
}

int main() {
  test_wcs_roundtrip();
  test_photometry_known_flux();
  test_photometry_out_of_bounds_fails();
  test_photometry_integration_regression();
  test_photometry_failed_center_explicit();
  if (failures == 0) {
    std::printf("P1-004 TESTS PASS (WCS roundtrip 1e-6, photometry flux/背景, 失败显式)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
