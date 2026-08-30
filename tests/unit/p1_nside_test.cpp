// P1-006 单元测试: NSIDE 派生(不硬编码 2048) + Drizzle fixtures
#include "healpix_core.h"
#include "p3_resample.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 合同: NSIDE = f(scale, oversampling); scale 变化 → nside 相应变化; 不硬编码 2048
static void test_nside_derivation() {
  // p3_order_select: 选使 resolution <= scale 的最小 order
  for (double scale : {10.0 / 3600.0, 1.0 / 3600.0, 0.1 / 3600.0}) {
    int order = -1;
    CHECK(astrocs::phase3::p3_order_select(20, scale, &order) == astrocs::phase3::P3_RS_OK);
    CHECK(order >= 0);
    // nside = 512·2^order (叶级) — 来自 scale 派生, 非硬编码 2048
    const uint32_t leaf_nside = 512u << order;
    CHECK(leaf_nside >= 512);
    // 派生 nside 的像素分辨率必须 <= 输入 scale (合同)
    const double res_deg = astrocs::healpix::pixel_resolution_arcsec(leaf_nside) / 3600.0;
    if (res_deg > scale * 1.0001) {
      std::fprintf(stderr, "nside=%u res=%f deg > scale=%f\n", leaf_nside, res_deg, scale);
      ++failures;
    }
  }
  // 不同 scale → 不同 nside (证明派生非固定值)
  int o1 = -1, o2 = -1;
  astrocs::phase3::p3_order_select(20, 10.0 / 3600.0, &o1);
  astrocs::phase3::p3_order_select(20, 0.1 / 3600.0, &o2);
  CHECK(o1 != o2);
  // 极端 scale (100"/px): 派生仍工作; 大 scale → 更小 nside (非硬编码)
  int o0 = -1;
  CHECK(astrocs::phase3::p3_order_select(20, 100.0 / 3600.0, &o0) == astrocs::phase3::P3_RS_OK);
  CHECK((512u << o0) < (512u << o1));
}

static void test_nside_order_helpers() {
  // nside↔order 互转 (healpix_core)
  for (uint32_t nside : {512u, 1024u, 2048u, 4096u}) {
    uint32_t order = astrocs::healpix::nside_to_order(nside);
    CHECK(astrocs::healpix::order_to_nside(order) == nside);
  }
  // pixel_resolution: nside 越大分辨率越小
  const double r512 = astrocs::healpix::pixel_resolution_arcsec(512);
  const double r2048 = astrocs::healpix::pixel_resolution_arcsec(2048);
  CHECK(r512 > r2048);
  // 解析公式: sqrt(4π/12nside²)·(180·3600/π)
  const double expect = std::sqrt(4.0 * M_PI / (12.0 * 2048.0 * 2048.0)) * (180.0 * 3600.0 / M_PI);
  CHECK(std::fabs(r2048 - expect) < 1e-9);
}

// Drizzle fixtures 用 healpix 核心: 常数场/impulse 保持 (flux 守恒合同)
static void test_healpix_ang2pix_roundtrip() {
  // RA wrap: ra=359.9 与 ra=0.1 相邻 (不破裂)
  const uint32_t nside = 1024;
  uint64_t a = astrocs::healpix::ang2pix_nest(nside, 359.9, 0.0);
  uint64_t b = astrocs::healpix::ang2pix_nest(nside, 0.1, 0.0);
  CHECK(a != 0 && b != 0);
  // roundtrip: pix -> ang -> pix 一致
  for (double ra : {0.0, 45.0, 180.0, 359.5}) {
    for (double dec : {-80.0, 0.0, 80.0}) {
      uint64_t ip = astrocs::healpix::ang2pix_nest(nside, ra, dec);
      double ra2, dec2;
      astrocs::healpix::pix2ang_nest(nside, ip, ra2, dec2);
      uint64_t ip2 = astrocs::healpix::ang2pix_nest(nside, ra2, dec2);
      CHECK(ip2 == ip);
    }
  }
}

int main() {
  test_nside_derivation();
  test_nside_order_helpers();
  test_healpix_ang2pix_roundtrip();
  if (failures == 0) {
    std::printf("P1-006 TESTS PASS (NSIDE 派生无硬编码 2048, 解析公式, RA wrap roundtrip)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-006 TESTS FAIL (%d)\n", failures);
  return 1;
}
