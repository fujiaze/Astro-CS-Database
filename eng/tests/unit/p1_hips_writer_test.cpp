// P1-007 单元测试: HiPS writer 合法产品 + FITS 头实卡断言 + 跨运行确定性 + IVOA tile 布局
//
// M2b-F-02 订正: 原断言含恒真析取 (tile.find("XTENSION= 'IMAGE   '") || true) 与
// 同路径重复读充当"确定性证据", 二者都不构成可判红的门。现改为:
//   1) properties 必需键逐项合取断言 (缺任一键即红);
//   2) tile 头按 FITS 实卡逐字断言 (主 HDU: SIMPLE/BITPIX/NAXIS/NAXIS1/NAXIS2 +
//      HiPS 科学卡 PIXTYPE/ORDERING), 不再用不存在的 XTENSION 卡;
//   3) 确定性 = 同输入**独立构建两次**(不同输出目录) 的 tile 逐字节相同;
//   4) IVOA REC-HIPS-1.0 §4.1 布局实证: 10302@order6 类例 (order5/nside16384 下
//      parent_ipix=10302) 必须落 Norder5/Dir10000/Npix10302.fits, 且旧非标准
//      路径 Norder5/Dir1/Npix302.fits **不得**存在 (M2b-B-01)。
// 另含 M1a-B-005 (hips_frame 标准值域) / M2b-B-02 (MOC 强制键) / M2b-B-03
// (hips_pixel_scale 单位为度) 的产物级断言。
#include "aio_hips.h"

#include <cmath>
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
  if (!d || !*d) d = std::getenv("TEMP");
  if (!d || !*d) d = std::getenv("TMP");
#if defined(_WIN32)
  return (d && *d) ? std::string(d) : std::string(".");
#else
  return (d && *d) ? std::string(d) : std::string("/tmp");
#endif
}

static std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static bool file_exists(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return f.good();
}

// 构建一个单 tile 产品; nside/parent_ipix 决定 tile 阶与实际落盘路径。
static bool build_tile_product(const std::string& dir, uint32_t nside,
                               uint64_t parent_ipix) {
  (void)std::system(("rm -rf \"" + dir + "\"").c_str());
  (void)std::system(("mkdir -p \"" + dir + "\"").c_str());

  const uint32_t W = 512;
  int leaf_order = 9;
  for (uint32_t n = nside; n > 512; n >>= 1u) ++leaf_order;

  std::vector<float> flux(static_cast<size_t>(W) * W, 100.0f);
  std::vector<float> area(static_cast<size_t>(W) * W, 1.0f);
  flux[0] = 50.0f;  // 变化值

  AstroSphereTileView view{};
  view.parent_ipix = parent_ipix;
  view.leaf_order = static_cast<uint32_t>(leaf_order);
  view.width = W;
  view.data_type = AIO_HIPS_FLOAT32;
  view.flux_sum = flux.data();
  view.covered_area = area.data();
  view.valid_mask = nullptr;
  view.var_num_sum = nullptr;
  aio_hips_tile_view_abi_init(&view);

  AioHipsProductSet* ps = aio_hips_product_begin(
      dir.c_str(), nside, W, AIO_HIPS_FLOAT32, AIO_HIPS_PRODUCT_ALL_V19,
      "did:test:p1", "P1-007 test", "NONE", 0.0, "2026-08-30T00:00:00Z", 0);
  if (!ps) {
    std::fprintf(stderr, "product_begin 失败: %s\n", aio_hips_last_error());
    return false;
  }
  int wrc = aio_hips_write_signal_support_tile(ps, &view);
  if (wrc != 0) {
    std::fprintf(stderr, "write rc=%d err=%s\n", wrc, aio_hips_last_error());
    return false;
  }
  if (aio_hips_set_drizzle_provenance(ps, 0.8, 1.0) != 0) return false;
  int frc = aio_hips_finalize(ps);
  if (frc != 0) {
    std::fprintf(stderr, "finalize rc=%d err=%s\n", frc, aio_hips_last_error());
    return false;
  }
  return true;
}

// FITS 主头实卡断言 (CFITSIO 固定 80 列卡格式, 逐字)。
static void check_primary_cards(const std::string& tile) {
  CHECK(tile.find("SIMPLE  =                    T") != std::string::npos);
  CHECK(tile.find("BITPIX  =                  -32") != std::string::npos);
  CHECK(tile.find("NAXIS   =                    2") != std::string::npos);
  CHECK(tile.find("NAXIS1  =                  512") != std::string::npos);
  CHECK(tile.find("NAXIS2  =                  512") != std::string::npos);
  CHECK(tile.find("PIXTYPE = 'HEALPIX '") != std::string::npos);
  CHECK(tile.find("ORDERING= 'NESTED  '") != std::string::npos);
  CHECK(tile.find("COORDSYS= 'C       '") != std::string::npos);
  // 校验和必须是真值 (非全零占位): 全零串是重算前的基准态, 不是交付值。
  CHECK(tile.find("CHECKSUM= '") != std::string::npos);
  CHECK(tile.find("CHECKSUM= '0000000000000000'") == std::string::npos);
  CHECK(tile.find("DATASUM = '") != std::string::npos);
}

int main() {
  const std::string base = tmp_dir() + "/astrocs_p1_hips_test";
  const std::string dirA = base + "/a";
  const std::string dirB = base + "/b";
  const std::string dirC = base + "/c";

  // ---- A) order0 tile: 实卡 + properties 必需键 ----
  CHECK(build_tile_product(dirA, 512, 0));

  {
    const std::string props = read_file(dirA + "/signal/properties");
    CHECK(!props.empty());
    // 必需键逐项合取 (缺任一即红); 不再是 "A || B" 形式。
    CHECK(props.find("hips_version") != std::string::npos);
    CHECK(props.find("creator_did") != std::string::npos);
    CHECK(props.find("obs_title") != std::string::npos);
    CHECK(props.find("hips_order") != std::string::npos);
    CHECK(props.find("hips_tile_width=512") != std::string::npos);
    // P0-19 (IVOA HiPS 1.0 §4.4.1 关键字表): hips_frame 标准值 = equatorial;
    // icrs 非法, 仅可作读侧兼容别名, 不得出现在写侧产物。
    CHECK(props.find("hips_frame=equatorial") != std::string::npos);
    CHECK(props.find("hips_frame=icrs") == std::string::npos);
    CHECK(props.find("hips_status=") != std::string::npos);
    CHECK(props.find("ASTROCS_DRIZZLE_PIXFRAC") != std::string::npos);
    // M2b-B-03: hips_pixel_scale 单位=度 (IVOA REC-HIPS-1.0 §4.4.1):
    // (180/π)·sqrt(π/3)/nside, nside=512 ⇒ 0.114516 (旧实现写 412.258369 角秒)。
    const double scale_deg = (180.0 / M_PI) * std::sqrt(M_PI / 3.0) / 512.0;
    char want[32];
    std::snprintf(want, sizeof(want), "%.6f", scale_deg);
    const std::string want_kv = std::string("hips_pixel_scale=") + want;
    CHECK(props.find(want_kv) != std::string::npos);
    CHECK(props.find("hips_pixel_scale=412.258369") == std::string::npos);
  }

  {
    const std::string tile = read_file(dirA + "/signal/Norder0/Dir0/Npix0.fits");
    CHECK(tile.size() > 2880);
    check_primary_cards(tile);
  }

  // ---- B) MOC 强制键 (IVOA REC-MOC 2.0 §6 Table 3) ----
  {
    const std::string moc = read_file(dirA + "/signal/Moc.fits");
    CHECK(!moc.empty());
    CHECK(moc.find("ORDERING= 'NUNIQ") != std::string::npos);
    CHECK(moc.find("COORDSYS= 'C       '") != std::string::npos);
    CHECK(moc.find("MOCORDER=") != std::string::npos);
  }

  // ---- C) 确定性: 同输入独立构建两次, tile 逐字节相同 ----
  CHECK(build_tile_product(dirB, 512, 0));
  {
    const std::string a = read_file(dirA + "/signal/Norder0/Dir0/Npix0.fits");
    const std::string b = read_file(dirB + "/signal/Norder0/Dir0/Npix0.fits");
    CHECK(!a.empty());
    CHECK(a == b);  // 跨目录独立构建的逐字节确定性 (非同一路径重复读)
  }

  // ---- D) IVOA §4.1 布局: parent_ipix=10302 @ order5 -> Dir10000/Npix10302.fits ----
  CHECK(build_tile_product(dirC, 16384u, 10302u));
  {
    const std::string std_path = dirC + "/signal/Norder5/Dir10000/Npix10302.fits";
    const std::string legacy_path = dirC + "/signal/Norder5/Dir1/Npix302.fits";
    CHECK(file_exists(std_path));
    CHECK(!file_exists(legacy_path));
    const std::string tile = read_file(std_path);
    CHECK(tile.size() > 2880);
    check_primary_cards(tile);
    const std::string props = read_file(dirC + "/signal/properties");
    CHECK(props.find("hips_order=5") != std::string::npos);
  }

  if (failures == 0) {
    std::printf("P1-007 TESTS PASS (实卡断言 + 必需键合取 + 独立双构建确定性 + IVOA tile 布局 + MOC 强制键)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-007 TESTS FAIL (%d)\n", failures);
  return 1;
}
