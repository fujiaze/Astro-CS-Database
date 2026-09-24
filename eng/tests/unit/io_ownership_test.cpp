// IO-002 单元测试: AIOImageData canonical deleter 全覆盖
// (配合 eng/tools/check_aio_ownership.py 静态扫描; 此处验证 deleter 行为)
//
// 退出语义(M8-F-002 修复): 任一 CHECK 失败 → main 返回非零; 修复前唯一 CHECK
// 只 ++failures 且 main 恒 return 0, 该 ctest 不可能失败(门恒绿)。
// 回归锁 failure_sets_nonzero_exit: 以子进程注入一次必然失败断言, 断言其退出码
// 非零 —— 防止"只 ++failures, 恒 return 0"回归。
#include "astro_image_io.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// ---- 最小合法 FITS 夹具(真实分配路径输入) ----------------------------------
// aio_read 不支持的格式不会分配 AIOImageData; 只有真实解析成功才走
// alloc_image_data() + 填充 + 返回非空指针, 从而覆盖 canonical deleter 的
// 真分配→释放路径。BITPIX=-32(float32), NAXIS=2, NAXIS1=w, NAXIS2=h。
static void put_card(char* block, int idx, const char* key, const char* val) {
  char* p = block + static_cast<std::size_t>(idx) * 80;
  std::memset(p, ' ', 80);
  char tmp[81];
  if (val != nullptr) {
    std::snprintf(tmp, sizeof(tmp), "%-8s= %20s", key, val);
  } else {
    std::snprintf(tmp, sizeof(tmp), "%-8s", key);   // END 卡(值域留空)
  }
  const std::size_t n = std::strlen(tmp) > 80 ? 80 : std::strlen(tmp);
  std::memcpy(p, tmp, n);
}

static bool write_minimal_fits(const std::string& path, int w, int h) {
  char header[2880];
  std::memset(header, ' ', sizeof(header));
  char v[32];
  put_card(header, 0, "SIMPLE", "T");
  put_card(header, 1, "BITPIX", "-32");
  put_card(header, 2, "NAXIS", "2");
  std::snprintf(v, sizeof(v), "%d", w); put_card(header, 3, "NAXIS1", v);
  std::snprintf(v, sizeof(v), "%d", h); put_card(header, 4, "NAXIS2", v);
  put_card(header, 5, "END", nullptr);
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) return false;
  bool ok = std::fwrite(header, 1, sizeof(header), f) == sizeof(header);
  const std::size_t nbytes = static_cast<std::size_t>(w) * h * 4;
  std::string data(nbytes, '\0');
  ok = ok && std::fwrite(data.data(), 1, data.size(), f) == data.size();
  const std::size_t pad = (2880 - (nbytes % 2880)) % 2880;
  if (pad > 0) {
    std::string zeros(pad, '\0');
    ok = ok && std::fwrite(zeros.data(), 1, zeros.size(), f) == zeros.size();
  }
  std::fclose(f);
  return ok;
}

static const int kW = 3;
static const int kH = 2;

// 子进程注入一次必然失败断言, 返回其退出码(0 = 失败未传导 → 回归锁不通过)。
static int probe_failure_exit(const char* exe) {
  const std::string cmd = std::string("\"") + exe + "\" --inject-failure";
  return std::system(cmd.c_str());
}

int main(int argc, char** argv) {
  // 注入入口: 只给回归锁子进程使用, 绝不进入正常用例。
  if (argc > 1 && std::strcmp(argv[1], "--inject-failure") == 0) {
    CHECK(false);   // 必然失败: 退出码必须非零
    return failures == 0 ? 0 : 1;
  }

  // 1) aio_free_image_data 对空指针安全 (canonical deleter null-safe)
  aio_free_image_data(nullptr);

  // 2) aio_read 对不存在文件返回 nullptr (空路径安全) 且 nullptr 释放安全
  AIOImageData* missing = aio_read("/nonexistent/path/nope.fits");
  CHECK(missing == nullptr);
  aio_free_image_data(missing);

  // 3) 真分配→释放: 最小合法 FITS 经 aio_read 真实分配 AIOImageData,
  //    校验几何后用 canonical deleter 释放; 反复 create/free(结构含多指针字段)
  //    不应把堆状态弄坏(重复分配+释放后仍可再次成功解析)。
  const std::string fits_path =
      (fs::temp_directory_path() / "acsd_io002_ownership_b2.fits").string();
  CHECK(write_minimal_fits(fits_path, kW, kH));
  {
    bool all_ok = true;
    for (int i = 0; i < 16; ++i) {
      AIOImageData* im = aio_read(fits_path.c_str());
      if (im == nullptr) { all_ok = false; break; }
      const bool geom_ok = aio_get_width(im) == kW && aio_get_height(im) == kH &&
                           aio_get_channels(im) >= 1 &&
                           aio_get_pixel_data(im) != nullptr;
      if (!geom_ok) all_ok = false;
      aio_free_image_data(im);
      if (!geom_ok) break;
    }
    CHECK(all_ok);
    // 3b) 两个实例同时存活, 逆序释放: deleter 是逐实例的(非全局单例)
    AIOImageData* a = aio_read(fits_path.c_str());
    AIOImageData* b = aio_read(fits_path.c_str());
    CHECK(a != nullptr && b != nullptr && a != b);
    aio_free_image_data(b);
    if (a != nullptr) CHECK(aio_get_width(a) == kW);   // b 释放后 a 仍有效
    aio_free_image_data(a);
  }
  std::error_code ec;
  fs::remove(fits_path, ec);

  // 4) 回归锁 failure_sets_nonzero_exit (M8-F-002): 失败必须传导非零退出码。
  //    子进程走 --inject-failure 分支; rc==0 说明 main 又把失败吞成 PASS。
  {
    const int rc = probe_failure_exit(argv[0]);
    CHECK(rc != 0);
    std::printf("failure_sets_nonzero_exit: injected rc=%d (期望非零)\n", rc);
  }

  if (failures == 0) {
    std::printf("IO-002 TESTS PASS (canonical deleter null-safe + 真分配/释放全覆盖; "
                "ownership scan 见 eng/tools/check_aio_ownership.py)\n");
    return 0;
  }
  std::fprintf(stderr, "IO-002 TESTS FAIL (%d)\n", failures);
  return 1;   // M8-F-002: 失败必须非零退出(修复前恒 return 0 → 门不可能红)
}
