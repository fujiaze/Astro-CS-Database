// IO-003 单元测试: CFITSIO reentrant 现场校验 + per-worker 独立 reader 并发
#include "fitsio.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 合成 32x16 单扩展 FITS
static std::string make_fits(const std::string& path) {
  fitsfile* fptr = nullptr;
  int status = 0;
  char cpath[512];
  std::snprintf(cpath, sizeof(cpath), "!%s", path.c_str());
  if (fits_create_file(&fptr, cpath, &status)) return "create";
  int naxis = 2;
  long naxes[2] = {32, 16};
  if (fits_create_img(fptr, FLOAT_IMG, naxis, naxes, &status)) {
    fits_close_file(fptr, &status); return "img";
  }
  std::vector<float> data(32 * 16);
  for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<float>(i) * 0.5f;
  long fpixel[2] = {1, 1};
  if (fits_write_pix(fptr, TFLOAT, fpixel, 32 * 16, data.data(), &status)) {
    fits_close_file(fptr, &status); return "write";
  }
  if (fits_close_file(fptr, &status)) return "close";
  return "";
}

static uint64_t read_hash(const std::string& path) {
  // 每 worker 独立 fitsfile*: 打开->读->校验->关闭
  fitsfile* fptr = nullptr;
  int status = 0;
  if (fits_open_file(&fptr, path.c_str(), READONLY, &status)) return 0;
  long naxes[2] = {0, 0};
  if (fits_get_img_size(fptr, 2, naxes, &status)) {
    fits_close_file(fptr, &status); return 0;
  }
  std::vector<float> data(static_cast<size_t>(naxes[0] * naxes[1]));
  long fpixel[2] = {1, 1};
  if (fits_read_pix(fptr, TFLOAT, fpixel, naxes[0] * naxes[1], nullptr, data.data(), nullptr, &status)) {
    fits_close_file(fptr, &status); return 0;
  }
  if (fits_close_file(fptr, &status)) return 0;
  uint64_t h = 1469598103934665603ULL;
  for (float v : data) {
    unsigned char* b = reinterpret_cast<unsigned char*>(&v);
    for (int i = 0; i < 4; ++i) { h ^= b[i]; h *= 1099511628211ULL; }
  }
  return h;
}

int main() {
  // 1) doctor 现场校验: fits_is_reentrant() 必须返回 1 (显式 reentrant 构建)
  CHECK(fits_is_reentrant() == 1);

  // 2) 合成 FITS 供并发读
  std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp")
                     + "/astrocs_io003_tile.fits";
  std::remove(path.c_str());
  std::string err = make_fits(path);
  CHECK(err.empty());

  // 3) 4 worker 独立 reader 并发读同一 tile; hash 必须一致
  uint64_t ref = read_hash(path);
  CHECK(ref != 0);
  std::atomic<int> mismatches{0};
  std::vector<std::thread> workers;
  for (int w = 0; w < 4; ++w) {
    workers.emplace_back([&, w] {
      for (int round = 0; round < 5; ++round) {
        uint64_t h = read_hash(path);
        if (h != ref) ++mismatches;
      }
    });
  }
  for (auto& t : workers) t.join();
  CHECK(mismatches.load() == 0);

  // 4) 2/8 worker 压力变体 (8 超订 2 核, 验证无崩溃且 hash 仍一致)
  for (int wcount : {2, 8}) {
    std::atomic<int> mm{0};
    std::vector<std::thread> w2;
    for (int w = 0; w < wcount; ++w) {
      w2.emplace_back([&, w] {
        for (int round = 0; round < 3; ++round) {
          if (read_hash(path) != ref) ++mm;
        }
      });
    }
    for (auto& t2 : w2) t2.join();
    CHECK(mm.load() == 0);
  }

  // 5) 每 worker 独立 fitsfile* (无共享句柄): read_hash 每次独立打开
  std::remove(path.c_str());
  if (failures == 0) {
    std::printf("IO-004 TESTS PASS (fits_is_reentrant=%d, 2/4/8 worker 压力 hash 全一致)\n",
                fits_is_reentrant());
    return 0;
  }
  std::fprintf(stderr, "IO-004 TESTS FAIL (%d)\n", failures);
  return 1;
}
