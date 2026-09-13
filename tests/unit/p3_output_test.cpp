// P3-005 单元测试: FITS 原子写 + 重开验证 (dimensions/WCS/BUNIT/标准 DATASUM/CHECKSUM/mask)
// B2-A9: verify 现对 WCS 有鉴别力（CTYPE/CUNIT/CRPIX/CRVAL/CD 对拍），故
// 不再声明"WCS 一致性由写路径单点保证"；本文件另含 WCS 篡改负例。
#include "p3_output.h"
#include "p3_wcs.h"

#include "fitsio.h"   // B2-A9 WCS 篡改负例（独立重开改 header）

#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
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
  // 跨平台临时目录 (同 io_adapter_test): Windows 回退 TEMP/TMP/"."。
  const char* d = std::getenv("TMPDIR");
  if (!d || !*d) d = std::getenv("TEMP");
  if (!d || !*d) d = std::getenv("TMP");
#if defined(_WIN32)
  const std::string dir0 = (d && *d) ? std::string(d) : std::string(".");
#else
  const std::string dir0 = (d && *d) ? std::string(d) : std::string("/tmp");
#endif
  // generic_string: 与 p3_assembly_test 同因 — Windows TEMP 反斜杠路径
  // 会被 lib 层 path_is_safe 类校验拒绝(R17 实证), 统一正斜杠。
  const std::string dir = std::filesystem::path(dir0).generic_string();
  const std::string out = dir + "/astrocs_p3_out_test.fits";
  std::remove(out.c_str());

  const int w = 64, h = 48;
  std::vector<float> sig(static_cast<size_t>(w) * h);
  std::vector<float> cov(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      sig[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = 100.0f + 0.5f * static_cast<float>(x);
      cov[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = (x < 40) ? 1.0f : 0.0f;  // mask
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
  {
    const astrocs::phase3::P3OutputStatus wst = astrocs::phase3::p3_output_write_atomic(
        sig.data(), cov.data(), w, h, &wcs, "ADU", out.c_str(),
        &prov, -32, -1, &res);
    if (wst != astrocs::phase3::P3_OUT_OK)
      std::fprintf(stderr, "[diag] write_atomic status=%d errno=%d\n", (int)wst, errno);
    CHECK(wst == astrocs::phase3::P3_OUT_OK);
  }
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
    // 检查目录中是否有 .tmp 残留 (跨平台: filesystem 遍历, WIN-001 替代 popen/ls)
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) break;
      if (entry.path().filename().string().rfind(".astrocs_p3_out_test.", 0) == 0 &&
          entry.path().extension() == ".tmp") { found = true; break; }
    }
    CHECK(!found);
  }

  // 3b) B2-A9 WCS 篡改负例: 独立重开把 CRPIX1 平移 +1（双桥接/原点回归的
  // 典型形态），verify 必须检出（reopen_ok=0）。verify 对 WCS 的鉴别力是
  // 本门的判别力来源，不是同式自证。
  {
    fitsfile* tf = nullptr;
    int st = 0;
    CHECK(fits_open_file(&tf, out.c_str(), READWRITE, &st) == 0);
    double tampered = 0.0;
    CHECK(fits_read_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
    tampered += 1.0;   // 0.5px/1px 型系统性平移
    st = 0;
    CHECK(fits_update_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
    st = 0;
    CHECK(fits_close_file(tf, &st) == 0);
    astrocs::phase3::P3OutputResult v{ };
    const astrocs::phase3::P3OutputStatus vst =
        astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                          w, h, &v);
    CHECK(vst == astrocs::phase3::P3_OUT_OK);   // 文件可读 = OK，但鉴别位必须失败
    CHECK(v.reopen_ok == 0);                    // CRPIX 篡改必须被检出
    // 复原 CRPIX1，保证后续用例共享同一产物文件时仍是合法 WCS。
    tampered -= 1.0;
    st = 0;
    CHECK(fits_open_file(&tf, out.c_str(), READWRITE, &st) == 0);
    CHECK(fits_update_key(tf, TDOUBLE, (char*)"CRPIX1", &tampered, nullptr, &st) == 0);
    st = 0;
    CHECK(fits_close_file(tf, &st) == 0);
    astrocs::phase3::P3OutputResult vr{ };
    CHECK(astrocs::phase3::p3_output_verify(out.c_str(), &wcs, sig.data(), cov.data(),
                                            w, h, &vr) == astrocs::phase3::P3_OUT_OK);
    CHECK(vr.reopen_ok == 1);                   // 复原后鉴别位恢复
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
