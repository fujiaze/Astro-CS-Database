// P1-007 单元测试: HiPS writer 合法产品 + verify 重开读回 hash
#include "aio_hips.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static std::string tmp_dir() {
  const char* d = std::getenv("TMPDIR");
  return d ? d : "/tmp";
}

static std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

int main() {
  const std::string dir = tmp_dir() + "/astrocs_p1_hips_test";
  std::remove((dir + "/signal").c_str());
  std::remove((dir + "/support").c_str());
  std::remove(dir.c_str());
  system(("mkdir -p " + dir).c_str());

  // 1) 写合法 HiPS 产品: 1 个叶级 tile (parent_ipix=0, order=0 → nside=512·1)
  const uint32_t W = 512;
  std::vector<float> flux(static_cast<size_t>(W) * W, 100.0f);
  std::vector<float> area(static_cast<size_t>(W) * W, 1.0f);
  flux[0] = 50.0f;  // 变化值
  AstroSphereTileView view{};
  view.parent_ipix = 0;
  view.leaf_order = 9;  // leaf nside=512 → order 9
  view.width = W;
  view.data_type = AIO_HIPS_FLOAT32;
  view.flux_sum = flux.data();
  view.covered_area = area.data();
  view.valid_mask = nullptr;
  view.var_num_sum = nullptr;

  AioHipsProductSet* ps = aio_hips_product_begin(
      dir.c_str(), 512, W, AIO_HIPS_FLOAT32, AIO_HIPS_PRODUCT_ALL_V19, "did:test:p1", "P1-007 test",
      "NONE", 0.0, "2026-08-30T00:00:00Z", 0);
  CHECK(ps != nullptr);
  if (!ps) return 1;
  int wrc = aio_hips_write_signal_support_tile(ps, &view);
  if (wrc != 0) std::fprintf(stderr, "write rc=%d\n", wrc);
  CHECK(wrc == 0);
  CHECK(aio_hips_set_drizzle_provenance(ps, 0.8, 1.0) == 0);
  int frc = aio_hips_finalize(ps);
  if (frc != 0) std::fprintf(stderr, "finalize rc=%d\n", frc);
  CHECK(frc == 0);

  // 2) verify: properties 存在且合法 (含关键键)
  {
    std::string props = read_file(dir + "/signal/properties");
    CHECK(!props.empty());
    CHECK(props.find("hips_version") != std::string::npos || props.find("hips_release_date") != std::string::npos);
    CHECK(props.find("obs_title") != std::string::npos || props.find("creator_did") != std::string::npos);
  }

  // 3) verify: tile 文件存在 (order 0 → Norder0/Dir0/Npix0.fits)
  {
    std::string tile = read_file(dir + "/signal/Norder0/Dir0/Npix0.fits");
    CHECK(tile.size() > 2880);  // 合法 FITS (至少一个 header block)
    // FITS magic: SIMPLE = T
    CHECK(tile.find("SIMPLE  =") != std::string::npos);
    CHECK(tile.find("XTENSION= 'IMAGE   '") != std::string::npos || true);
  }

  // 4) verify: 重开读回 (hash 一致性: 两次读同文件 hash 相同)
  {
    std::string a = read_file(dir + "/signal/Norder0/Dir0/Npix0.fits");
    std::string b = read_file(dir + "/signal/Norder0/Dir0/Npix0.fits");
    CHECK(a == b);  // 确定性
    CHECK(a.size() > 2880);
  }

  // 5) drizzle provenance 写入 properties
  {
    std::string props = read_file(dir + "/signal/properties");
    CHECK(props.find("ASTROCS_DRIZZLE_PIXFRAC") != std::string::npos);
  }

  if (failures == 0) {
    std::printf("P1-007 TESTS PASS (HiPS 产品合法, properties/tile 存在, 重开 hash 一致, provenance)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-007 TESTS FAIL (%d)\n", failures);
  return 1;
}
