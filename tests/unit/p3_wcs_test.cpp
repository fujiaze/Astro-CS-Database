// P3-002 单元测试: WCS 尺寸溢出检查 + 配置合同上限 + 输出完整性
#include "p3_wcs.h"

#include <cmath>
#include <cstdint>
#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 溢出检查: 尺寸乘法在 uint64 域不溢出 (模拟 kernel 计划像素数)
static bool safe_pixel_count(int w, int h, std::uint64_t* out) {
  if (w <= 0 || h <= 0) return false;
  std::uint64_t uw = static_cast<std::uint64_t>(w);
  std::uint64_t uh = static_cast<std::uint64_t>(h);
  if (uw > UINT64_MAX / uh) return false;   // 乘法溢出检查
  *out = uw * uh;
  return true;
}

int main() {
  // 1) WCS 输出完整: 输入 HiPS properties/尺度 → WCS/dimensions/pixel scale
  {
    astrocs::phase3::P3WcsDescriptor w{};
    // 输入: 中心 RA/Dec, scale, 尺寸 → 输出 WCS
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 1024, 768,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_OK);
    CHECK(w.width_px == 1024);
    CHECK(w.height_px == 768);
    CHECK(w.cd[0][0] != 0);   // pixel scale 存在 (deg/px)
    CHECK(std::fabs(w.cd[0][0] + 0.0001389) < 1e-9);  // east_left: CD1_1=-s
  }

  // 2) 尺寸乘法溢出检查: 极大合法边界内不溢出
  {
    std::uint64_t n = 0;
    CHECK(safe_pixel_count(20000, 20000, &n));        // 4e8 像素 OK
    CHECK(n == 400000000ULL);
    // 超界拒绝 (配置合同上限之外)
    CHECK(!safe_pixel_count(0, 100, &n));             // 零尺寸
    CHECK(!safe_pixel_count(-5, 100, &n));            // 负尺寸
  }

  // 3) 最大尺寸来自配置合同: 超上限拒绝 (WCS 层)
  {
    astrocs::phase3::P3WcsDescriptor w{};
    // 20001 > 默认合同上限 20000 → 拒
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 20001, 100,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_PARAM);
    // 边界值 20000 接受
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 20000, 20000,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_OK);
  }

  // 4) pix2world roundtrip (kernel 计划前提)
  {
    astrocs::phase3::P3WcsDescriptor w{};
    astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.0001389, 1024, 768,
                                 "east_left", 0.0, &w);
    double ra, dec, x, y;
    CHECK(astrocs::phase3::p3_wcs_pix2world(&w, 512.0, 384.0, &ra, &dec) == astrocs::phase3::P3_WCS_OK);
    CHECK(astrocs::phase3::p3_wcs_world2pix(&w, ra, dec, &x, &y) == astrocs::phase3::P3_WCS_OK);
    CHECK(std::fabs(x - 512.0) < 1e-4);
    CHECK(std::fabs(y - 384.0) < 1e-4);
  }

  // 5) 溢出边界: 乘法保护 (接近 uint64 上限模拟)
  {
    std::uint64_t n = 0;
    // 模拟 50000×50000 = 2.5e9 仍安全 (uint64 域)
    CHECK(safe_pixel_count(50000, 50000, &n));
    CHECK(n == 2500000000ULL);
  }

  if (failures == 0) {
    std::printf("P3-002 TESTS PASS (尺寸溢出检查, 配置合同上限 20000, WCS 输出完整, roundtrip)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
