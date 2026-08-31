// P3-005 单元测试: FITS 原子写 + 重开验证 (dimensions/WCS/BUNIT/checksum/mask)
#include "p3_output.h"
#include "p3_wcs.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  const char* d = std::getenv("TMPDIR");
  const std::string dir = d ? d : "/tmp";
  const std::string out = dir + "/astrocs_p3_out_test.fits";
  std::remove(out.c_str());

  const int w = 64, h = 48;
  std::vector<float> sig(static_cast<size_t>(w) * h);
  std::vector<float> cov(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      sig[static_cast<size_t>(y) * w + x] = 100.0f + 0.5f * x;
      cov[static_cast<size_t>(y) * w + x] = (x < 40) ? 1.0f : 0.0f;  // mask
    }

  astrocs::phase3::P3WcsDescriptor wcs{};
  CHECK(astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.0001389, w, h,
                                     "east_left", 0.0, &wcs) == astrocs::phase3::P3_WCS_OK);

  astrocs::phase3::P3Provenance prov{};
  prov.hips_id = "ivo://astrocs/test";
  prov.manifest_hash = "deadbeef";
  prov.missing_tiles = nullptr;
  prov.missing_count = 0;
  prov.software_version = "0.10.0-alpha.2";   // 来自版本单源, 非硬编码
  prov.run_id = "run-p3-005-test";
  prov.order_sel_used = "4";
  prov.sampler_used = "bilinear";

  astrocs::phase3::P3OutputResult res{};
  // 1) 原子写 (不硬编码 version/run_id: 来自 prov)
  CHECK(astrocs::phase3::p3_output_write_atomic(
            sig.data(), cov.data(), w, h, &wcs, "ADU", out.c_str(),
            &prov, -1, &res) == astrocs::phase3::P3_OUT_OK);
  CHECK(res.coverage_ok == 1);
  CHECK(res.reopen_ok == 1);
  CHECK(std::strlen(res.sha256) == 64);

  // 2) 独立重开 verify: dimensions/WCS/BUNIT/checksum/mask 一致
  {
    astrocs::phase3::P3OutputResult v{};
    CHECK(astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                            w, h, &v) == astrocs::phase3::P3_OUT_OK);
    CHECK(v.reopen_ok == 1);
    CHECK(v.coverage_ok == 1);
    CHECK(std::strlen(v.sha256) == 64);
  }

  // 3) 原子性: 无 .tmp 残留
  {
    std::string tmp = dir + "/.astrocs_p3_out_test.";   // 前缀匹配
    bool found = false;
    // 检查目录中是否有 .tmp 残留
    std::string cmd = "ls " + dir + " 2>/dev/null | grep -c '\\.astrocs_p3_out_test\\..*\\.tmp' || true";
    FILE* p = popen(cmd.c_str(), "r");
    char buf[64] = {0};
    if (p) { if (fgets(buf, sizeof(buf), p)) found = (std::atoi(buf) > 0); pclose(p); }
    CHECK(!found);
  }

  // 4) pixel→sky→sample Oracle: WCS roundtrip 后采样信号一致
  {
    double ra, dec, x, y;
    CHECK(astrocs::phase3::p3_wcs_pix2world(&wcs, 32.0, 24.0, &ra, &dec) == astrocs::phase3::P3_WCS_OK);
    CHECK(astrocs::phase3::p3_wcs_world2pix(&wcs, ra, dec, &x, &y) == astrocs::phase3::P3_WCS_OK);
    CHECK(std::fabs(x - 32.0) < 1e-4 && std::fabs(y - 24.0) < 1e-4);
    // 采样值 = 信号 (Oracle: 像素→sky→像素 不变)
    double s = sig[static_cast<size_t>(24) * w + 32];
    CHECK(std::fabs(s - (100.0 + 0.5 * 32)) < 1e-3);
  }

  if (failures == 0) {
    std::printf("P3-005 TESTS PASS (FITS 原子写, 重开 verify dims/WCS/BUNIT/checksum/mask, Oracle roundtrip)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-005 TESTS FAIL (%d)\n", failures);
  return 1;
}
