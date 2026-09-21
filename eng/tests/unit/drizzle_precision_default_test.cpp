// eng/tests/unit/drizzle_precision_default_test.cpp
// RESCUE-FD-02 (P1): hp_drizzle_run 库边界 precision_mode 缺省语义回归。
//
// 缺陷: precision_mode==-1 且帧头无 "PRECISION" KV 时，库静默取 FP32 累积
//       (config.precision_mode=0)，与宪章 §5.3「Drizzle 采用 float64 累积」分歧。
// 修复: 缺省改为 FP64（参数 -1 + 无 KV → FP64，宪章一致）；帧头显式
//       "fp32"/"fp64" 仍生效；未知 PRECISION 值 fail-closed（非零返回），
//       不再存在任何静默 FP32 通路。
//
// 断言（库边界, 不依赖 module_adapters 节点）:
//   T1  -1 + 无 KV          → rc=0, HISS metadata precision_mode=1 (FP64)
//   T2  -1 + PRECISION=fp32 → rc=0, precision_mode=0
//   T3  -1 + PRECISION=fp64 → rc=0, precision_mode=1
//   T4  显式 0 (无 KV)      → precision_mode=0
//   T5  显式 1 (无 KV)      → precision_mode=1
//   T6  -1 + PRECISION=bogus→ rc!=0 (fail-closed, 不静默 FP32)
//   T7  显式 FP32/FP64 累积等价性 (逐 tile signal max_rel < 1e-5)
#include "hp_drizzle_api.h"
#include "aio_healpix_io.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define FD02_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define FD02_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)
#define CHECK_MSG(cond, msg)                                              \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

namespace {

constexpr int W = 64, H = 64;

std::vector<float> make_image() {
  std::vector<float> img((size_t)W * H, 0.0f);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const double dx = (double)x - 32.0, dy = (double)y - 32.0;
      img[(size_t)y * W + x] =
          (float)(1000.0 * std::exp(-(dx * dx + dy * dy) / 18.0) + 10.0);
    }
  return img;
}

// M2a-H-1: 真 FP64 输入图像 —— 值带 binary32 不可精确表示的尾数，用于验证
// 实际累加域确实是 binary64（FLOAT32 累加会把值量化到 f32）。
std::vector<double> make_image_f64() {
  std::vector<double> img((size_t)W * H, 0.0);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const double dx = (double)x - 32.0, dy = (double)y - 32.0;
      img[(size_t)y * W + x] = 1000.0 * std::exp(-(dx * dx + dy * dy) / 18.0) +
                               10.0 + 1.0 / (7.0 + (double)(x * 37 + y));
    }
  return img;
}

// 与 drizzle_adapter_test 同构的最小 TAN 帧 (dims[0]=H, dims[1]=W)。
bool set_frame_kvs(PipelineFrame* f) {
  const struct { const char* k; const char* v; } kvs[] = {
      {"CRVAL1", "202.5"}, {"CRVAL2", "47.2"},
      {"CRPIX1", "32.0"}, {"CRPIX2", "32.0"},
      {"CD1_1", "-0.0002777778"}, {"CD1_2", "0.0"},
      {"CD2_1", "0.0"}, {"CD2_2", "0.0002777778"},
      {"CTYPE1", "RA---TAN"}, {"CTYPE2", "DEC--TAN"},
      {"PHOTSCAL", "1.0"}, {"PHOTAPPL", "1.0"},
      {"FILTER", "g"}, {"EXPTIME", "30.0"},
  };
  for (const auto& kv : kvs)
    if (aio_frame_kv_set(f, "header", kv.k, kv.v) != 0) return false;
  return true;
}
PipelineFrame* make_frame(const std::vector<float>& img) {
  PipelineFrame* f = aio_pipeline_frame_create();
  if (!f) return nullptr;
  int dims[2] = {H, W};
  if (aio_frame_add_block(f, "data", AIO_BLOCK_FLOAT32, (void*)img.data(),
                          (int64_t)(size_t)W * H, dims, 2, "fd02") != 0 ||
      !set_frame_kvs(f)) {
    aio_pipeline_frame_destroy(f);
    return nullptr;
  }
  return f;
}
// M2a-H-1: FLOAT64 data 块 → 真 binary64 累加域（与 precision_mode=1 一致）。
PipelineFrame* make_frame_f64(const std::vector<double>& img) {
  PipelineFrame* f = aio_pipeline_frame_create();
  if (!f) return nullptr;
  int dims[2] = {H, W};
  if (aio_frame_add_block(f, "data", AIO_BLOCK_FLOAT64, (void*)img.data(),
                          (int64_t)(size_t)W * H, dims, 2, "fd02") != 0 ||
      !set_frame_kvs(f)) {
    aio_pipeline_frame_destroy(f);
    return nullptr;
  }
  return f;
}

// 运行一次库调用; prec_kv==nullptr → 不写 PRECISION KV。
// f64_frame=true → FLOAT64 data 块（FP64 请求的合法输入，M2a-H-1）。
int run_case(const std::string& out_path, int param, const char* prec_kv,
             bool f64_frame = false) {
  PipelineFrame* f = nullptr;
  std::vector<float> img;
  std::vector<double> img64;
  if (f64_frame) {
    img64 = make_image_f64();
    f = make_frame_f64(img64);
  } else {
    img = make_image();
    f = make_frame(img);
  }
  if (!f) return -999;
  if (prec_kv) aio_frame_kv_set(f, "header", "PRECISION", prec_kv);
  HpDrizzleResult res;
  std::memset(&res, 0, sizeof(res));
  const int rc = hp_drizzle_run(f, 512, 1, 1.0, out_path.c_str(), &res, param);
  aio_pipeline_frame_destroy(f);
  if (rc != 0)
    std::fprintf(stderr, "  run_case(param=%d kv=%s) rc=%d err=%s\n", param,
                 prec_kv ? prec_kv : "(none)", rc, res.error_msg);
  return rc;
}

bool hiss_precision(const std::string& path, int* out) {
  uint32_t ns = 0, tn = 0, dp = 0, nl = 0;
  uint64_t nt = 0, npx = 0;
  char* meta = nullptr;
  uint64_t* tips = nullptr;
  if (aio_hiss_inspect(path.c_str(), &ns, &tn, &dp, &nl, &nt, &npx, &meta,
                       &tips) != 0) {
    if (meta) aio_hio_free(meta);
    if (tips) aio_hio_free(tips);
    return false;
  }
  bool ok = false;
  if (meta && meta[0]) {
    try {
      const json mj = json::parse(meta);
      *out = mj.value("precision_mode", -1);
      ok = true;
    } catch (...) { ok = false; }
  }
  if (meta) aio_hio_free(meta);
  if (tips) aio_hio_free(tips);
  return ok;
}

// 逐 tile 比较 FP32/FP64 HISS 的 signal (相对误差)。
bool signals_equivalent(const std::string& f32_path, const std::string& f64_path,
                        double* max_rel) {
  uint32_t ns0=0,tn0=0,dp0=0,nl0=0, ns1=0,tn1=0,dp1=0,nl1=0;
  uint64_t nt0=0,npx0=0, nt1=0,npx1=0;
  char* m0=nullptr; char* m1=nullptr; uint64_t* t0=nullptr; uint64_t* t1=nullptr;
  if (aio_hiss_inspect(f32_path.c_str(), &ns0,&tn0,&dp0,&nl0,&nt0,&npx0,&m0,&t0)!=0 ||
      aio_hiss_inspect(f64_path.c_str(), &ns1,&tn1,&dp1,&nl1,&nt1,&npx1,&m1,&t1)!=0) {
    if(m0)aio_hio_free(m0); if(m1)aio_hio_free(m1);
    if(t0)aio_hio_free(t0); if(t1)aio_hio_free(t1);
    return false;
  }
  bool ok = (nt0 == nt1 && nl0 == nl1 && nt0 > 0);
  double worst = 0.0;
  if (ok) {
    for (uint64_t t = 0; t < nt0 && ok; ++t) {
      if (t0[t] != t1[t]) { ok = false; break; }
      float* s32 = nullptr; double* s64 = nullptr;
      uint32_t n32 = 0, n64 = 0;
      const int r32 = aio_hiss_read_tile_signal(f32_path.c_str(), t0[t], &s32, &n32);
      const int r64 = aio_hiss_read_tile_signal_f64(f64_path.c_str(), t1[t], &s64, &n64);
      if (r32 != 0 || r64 != 0 || n32 != n64 || n32 != nl0) {
        if (s32) aio_hio_free(s32);
        if (s64) aio_hio_free(s64);
        ok = false; break;
      }
      for (uint32_t i = 0; i < n32; ++i) {
        const double a = s32[i], b = s64[i];
        const double denom = std::max(1.0, std::fabs(b));
        worst = std::max(worst, std::fabs(a - b) / denom);
      }
      if (s32) aio_hio_free(s32);
      if (s64) aio_hio_free(s64);
    }
  }
  if (m0) aio_hio_free(m0);
  if (m1) aio_hio_free(m1);
  if (t0) aio_hio_free(t0);
  if (t1) aio_hio_free(t1);
  *max_rel = worst;
  return ok;
}

// M2a-H-1 签名：统计 f64 signal tile 中「不可被 binary32 精确表示」的非零值。
// FP64 请求若实际 binary32 累加 → 值经 f32 量化 → 计数恒 0。
uint64_t count_f64_nonfloat(const std::string& path) {
  uint32_t ns = 0, tn = 0, dp = 0, nl = 0;
  uint64_t nt = 0, npx = 0;
  char* m = nullptr;
  uint64_t* tips = nullptr;
  if (aio_hiss_inspect(path.c_str(), &ns, &tn, &dp, &nl, &nt, &npx, &m, &tips) != 0) {
    if (m) aio_hio_free(m);
    if (tips) aio_hio_free(tips);
    return 0;
  }
  uint64_t n_nonfloat = 0;
  for (uint64_t t = 0; t < nt; ++t) {
    double* s = nullptr;
    uint32_t n = 0;
    if (aio_hiss_read_tile_signal_f64(path.c_str(), tips[t], &s, &n) == 0) {
      for (uint32_t i = 0; i < n; ++i)
        if (s[i] != 0.0 && (double)(float)s[i] != s[i]) ++n_nonfloat;
      if (s) aio_hio_free(s);
    }
  }
  if (m) aio_hio_free(m);
  if (tips) aio_hio_free(tips);
  return n_nonfloat;
}

}  // namespace

int main() {
  std::error_code ec;
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path(ec) /
      ("astrocs_fd02_" + std::to_string(FD02_GETPID));
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  char p1[512], p2[512], p3[512], p4[512], p5[512], p6[512];
  std::snprintf(p1, sizeof(p1), "%s/default_nokv.hiss", dir.string().c_str());
  std::snprintf(p2, sizeof(p2), "%s/kv_fp32.hiss", dir.string().c_str());
  std::snprintf(p3, sizeof(p3), "%s/kv_fp64.hiss", dir.string().c_str());
  std::snprintf(p4, sizeof(p4), "%s/explicit_fp32.hiss", dir.string().c_str());
  std::snprintf(p5, sizeof(p5), "%s/explicit_fp64.hiss", dir.string().c_str());
  std::snprintf(p6, sizeof(p6), "%s/kv_bogus.hiss", dir.string().c_str());
  char p7[512];
  std::snprintf(p7, sizeof(p7), "%s/reject_f32_fp64.hiss", dir.string().c_str());

  int prec = -99;
  // T1: 缺省 (-1, 无 KV) → FP64 (宪章 §5.3)
  CHECK_MSG(run_case(p1, -1, nullptr, true) == 0, "T1 default run must succeed");
  CHECK_MSG(hiss_precision(p1, &prec), "T1 HISS metadata readable");
  CHECK_MSG(prec == 1, ("T1 -1 + no PRECISION KV must accumulate FP64 (got " +
                        std::to_string(prec) + ")").c_str());

  // T2/T3: 帧头 KV 显式
  prec = -99;
  CHECK(run_case(p2, -1, "fp32") == 0);
  CHECK(hiss_precision(p2, &prec) && prec == 0);
  prec = -99;
  CHECK(run_case(p3, -1, "fp64", true) == 0);
  CHECK(hiss_precision(p3, &prec) && prec == 1);

  // T4/T5: 参数显式 0/1
  prec = -99;
  CHECK(run_case(p4, 0, nullptr) == 0);
  CHECK(hiss_precision(p4, &prec) && prec == 0);
  prec = -99;
  CHECK(run_case(p5, 1, nullptr, true) == 0);
  CHECK(hiss_precision(p5, &prec) && prec == 1);

  // T6: 未知 PRECISION 值 → fail-closed (绝不静默 FP32)
  const int rc6 = run_case(p6, -1, "bogus");
  CHECK_MSG(rc6 != 0, "T6 unknown PRECISION KV must be rejected (no silent FP32)");

  // T8 (M2a-H-1): 请求 FP64 而 data 块 FLOAT32 → 显式拒绝
  // （fail-closed，禁止元数据声称 FP64 而实际 binary32 累加）。
  CHECK_MSG(run_case(p7, 1, nullptr, false) != 0,
            "T8 FLOAT32 data block + precision_mode=1 must be rejected");

  // T7 (M2a-H-1 修正): FP32 与真 FP64 累积不再数值等价——旧断言 max_rel<1e-5
  // 编码的正是「FP64 请求实际按 binary32 累加」的缺陷。修正后 FP64 走真
  // binary64 域，二者应同尺度但存在可辨差异（T9 给出 f64 签名佐证）。
  double max_rel = 1e9;
  CHECK_MSG(signals_equivalent(p4, p5, &max_rel),
            "T7 FP32/FP64 HISS readable and same tile set");
  CHECK_MSG(max_rel > 0.0,
            ("T7 FP32 vs true-FP64 must differ max_rel=" +
             std::to_string(max_rel)).c_str());
  CHECK_MSG(max_rel < 1e-3,
            ("T7 FP32 vs true-FP64 same-scale max_rel=" +
             std::to_string(max_rel)).c_str());

  // T9 (M2a-H-1 / drizzle_precision_consistency): FP64 请求必须产生真 binary64
  // 累加签名（f32 不可精确表示）。修复前节点恒提交 FLOAT32 块 → binary32 累加
  // → 计数 == 0（红）；修复后 > 0（绿）。
  const uint64_t n_nonfloat = count_f64_nonfloat(p5);
  std::printf("drizzle precision: p5 f64 nonfloat-count=%llu\n",
              (unsigned long long)n_nonfloat);
  CHECK_MSG(n_nonfloat > 0,
            "T9 FP64 request must yield binary32-inexact (true FP64) accumulation");

  std::filesystem::remove_all(dir, ec);
  if (failures == 0) {
    std::printf("RESCUE-FD-02 DRIZZLE PRECISION DEFAULT TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RESCUE-FD-02 DRIZZLE PRECISION DEFAULT TESTS FAIL (%d)\n",
               failures);
  return 1;
}
