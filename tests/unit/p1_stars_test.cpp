// P1-003 单元测试: StarDetector 5 场景 + completeness/false positive/tie breaker
#include "star_detector.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using astrocs::phase1::StarCatalog;
using astrocs::phase1::StarDetector;
using astrocs::phase1::StarSource;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void add_gaussian(std::vector<float>& img, int w, int h,
                         double cx, double cy, double amp, double sigma,
                         double bg) {
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      double dx = x - cx, dy = y - cy;
      double v = bg + amp * std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
      img[static_cast<size_t>(y) * w + x] = static_cast<float>(v);
    }
}

static void test_isolated_gaussian() {
  const int w = 64, h = 64;
  std::vector<float> img(static_cast<size_t>(w) * h, 100.0f);  // bg=100
  add_gaussian(img, w, h, 32.0, 32.0, 800.0, 2.0, 100.0);
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok()) {
    StarCatalog& c = r.value();
    CHECK(c.n_detected == 1);          // completeness 1/1
    CHECK(c.sources.size() == 1);
    CHECK(std::fabs(c.sources[0].x - 32.0) < 0.5);   // centroid 准确
    CHECK(std::fabs(c.sources[0].y - 32.0) < 0.5);
    CHECK(c.sources[0].fwhm_px > 0);
    CHECK(c.sources[0].snr > 5.0);
    CHECK(c.sources[0].quality == 0);  // 干净
  }
}

static void test_moffat_overlap() {
  const int w = 96, h = 96;
  std::vector<float> img(static_cast<size_t>(w) * h, 50.0f);
  add_gaussian(img, w, h, 40.0, 48.0, 800.0, 2.0, 50.0);
  add_gaussian(img, w, h, 56.0, 48.0, 800.0, 2.0, 50.0);
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok()) {
    // 重叠星: 若被去重合并为 1 (峰在中间), 至少检测到 >=1
    CHECK(r.value().n_detected >= 1);
    CHECK(r.value().n_detected <= 2);
  }
}

static void test_saturated_star() {
  const int w = 64, h = 64;
  std::vector<float> img(static_cast<size_t>(w) * h, 100.0f);
  add_gaussian(img, w, h, 32.0, 32.0, 60000.0, 3.0, 100.0);  // 极高幅 → 饱和
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok()) {
    CHECK(r.value().n_saturated >= 1);  // 质量位 1
    CHECK((r.value().sources[0].quality & 1) != 0);
  }
}

static void test_edge_star() {
  const int w = 48, h = 48;
  std::vector<float> img(static_cast<size_t>(w) * h, 100.0f);
  add_gaussian(img, w, h, 1.5, 24.0, 800.0, 2.0, 100.0);  // 边缘
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok() && !r.value().sources.empty()) {
    CHECK((r.value().sources[0].quality & 2) != 0);  // 边缘位
    CHECK(r.value().n_edge >= 1);
  }
}

static void test_pure_noise_no_false_positive() {
  const int w = 64, h = 64;
  std::vector<float> img(static_cast<size_t>(w) * h);
  std::mt19937 rng(42);
  std::normal_distribution<float> dist(100.0f, 3.0f);
  for (auto& v : img) v = dist(rng);
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok()) {
    // 纯噪声 (σ=3, 5σ 阈值): 不应有显著误报
    CHECK(r.value().n_detected <= 2);   // 允许极少数 5σ 概率事件, 但应接近 0
  }
}

static void test_tie_breaker_dedup() {
  // 两个等强峰相距 5px (都检测到): 排序 tie breaker flux 降序、同 flux 取更左
  const int w = 40, h = 40;
  std::vector<float> img(static_cast<size_t>(w) * h, 10.0f);
  add_gaussian(img, w, h, 15.0, 20.0, 500.0, 1.5, 10.0);
  add_gaussian(img, w, h, 22.0, 20.0, 500.0, 1.5, 10.0);
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok() && r.value().sources.size() == 2) {
    // 等强 (flux 近似): tie breaker 使更左者排前
    CHECK(r.value().sources[0].x < r.value().sources[1].x);
  }
}

static void test_catalog_fields() {
  const int w = 64, h = 64;
  std::vector<float> img(static_cast<size_t>(w) * h, 100.0f);
  add_gaussian(img, w, h, 32.0, 32.0, 800.0, 2.0, 100.0);
  StarDetector det;
  auto r = det.detect(img.data(), w, h);
  CHECK(r.ok());
  if (r.ok()) {
    const StarSource& s = r.value().sources[0];
    // catalog 带坐标/单位/质量字段
    CHECK(s.id.find("src-") == 0);
    CHECK(s.flux > 0);          // ADU
    CHECK(s.fwhm_px > 0);       // px
    CHECK(s.ellipticity >= 0);
    CHECK(s.snr > 0);
    CHECK(r.value().background > 0);  // ADU
    CHECK(r.value().noise_sigma > 0);
  }
}

int main() {
  test_isolated_gaussian();
  test_moffat_overlap();
  test_saturated_star();
  test_edge_star();
  test_pure_noise_no_false_positive();
  test_tie_breaker_dedup();
  test_catalog_fields();
  if (failures == 0) {
    std::printf("P1-003 TESTS PASS (孤立/重叠/饱和/边缘/纯噪声 + completeness/fp/tie-breaker/catalog)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-003 TESTS FAIL (%d)\n", failures);
  return 1;
}
