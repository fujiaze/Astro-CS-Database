// tests/unit/p3_sampler_cache_test.cpp — P30 回归: Phase3 tile 缓存正确性守护
//
// 记录本用例守护的**真根因** (P30, 真实 M42 T2+Blue 实测):
//   修复前 p3 导出对"覆盖之外"的 tile 每个像素都调 fits_open_file (无负缓存),
//   且每个行带 worker 的 tile 缓存容量恒为默认 8 (max_tiles 只作用于主
//   sampler) → 全平面约 2.8e8 次失败 open 全局串行 (进程级 cfitsio_io_mutex),
//   16 核预算实测只用 1.7–2.5 核。
//
// 断言 (机器可守护, 不依赖墙钟):
//   A. 未覆盖区域的**重复采样不线性增长底层 open 次数** —— 同一组缺失 tile
//      重复采样 R 次, 真实失败 open 次数保持"每 tile 一次"(负缓存去重);
//      R 增大 open 次数**不增加**。
//   B. 阴性对照: 关闭负缓存 (p3_sampler_set_absent_cache(s,0), 精确退化为修复前
//      语义) 后, 同一采样的 open 次数线性增长 ⇒ 若 A 所依赖的机制被移除或
//      计数器失效, 本用例必红。
//   C. 跨 worker 共享缓存: attach 到同一主 sampler 的多个 worker 必须看到
//      **同一份** 缓存统计 (hits/cap/evictions), 且经其采样会改变主 sampler
//      观测; 未 attach 的 worker 不共享 (对照)。
//   D. 缓存有界: 容量 = max_tiles 时必须发生逐出且常驻数不超过容量。
//   E. 缓存状态不改变任何像素值 (开/关负缓存、不同容量下采样值逐位相同)。
#include "p3_output.h"
#include "p3_resample.h"
#include "aio_hips.h"
#include "healpix_core.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                                \
    }                                                                            \
  } while (0)

using astrocs::phase3::P3CacheStats;
using astrocs::phase3::P3Sampler;

static bool build_mini_hips(const std::string& hips, uint32_t n_present_tiles) {
  std::error_code ec;
  std::filesystem::remove_all(hips, ec);
  std::filesystem::create_directories(hips, ec);
  if (ec) return false;
  const uint32_t W = 512;
  AioHipsProductSet* ps = aio_hips_product_begin(
      hips.c_str(), 512, W, AIO_HIPS_FLOAT32, AIO_HIPS_PRODUCT_SIGNAL,
      "did:test:p3cache", "P30 cache regression", "NONE", 0.0,
      "2026-08-30T00:00:00Z", 0);
  if (!ps) return false;
  for (uint32_t k = 0; k < n_present_tiles; ++k) {
    std::vector<float> flux(static_cast<size_t>(W) * W, 100.0f + static_cast<float>(k));
    std::vector<float> area(static_cast<size_t>(W) * W, 1.0f);
    AstroSphereTileView view{};
    view.parent_ipix = k;          // Norder K tile ipix
    view.leaf_order = 9;           // nside=512 → K=0
    view.width = W;
    view.data_type = AIO_HIPS_FLOAT32;
    view.flux_sum = flux.data();
    view.covered_area = area.data();
    view.valid_mask = nullptr;
    view.var_num_sum = nullptr;
    aio_hips_tile_view_abi_init(&view);
    if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
      aio_hips_finalize(ps);
      return false;
    }
  }
  return aio_hips_finalize(ps) == 0;
}

// nside=512 的 HiPS ⇒ tile order K=0, 12 个 base tile。
// 用 base 像素中心 (pix2ang_nest(1,k)) 作为采样方向, bilinear 四角落在同一 tile。
static void tile_center(uint32_t k, double* ra, double* dec) {
  astrocs::healpix::pix2ang_nest(1, k, *ra, *dec);
}

static const uint32_t kPresent = 4;      // 只写 tile 0..3
static const uint32_t kAll = 12;         // order-0 全部 tile

int main() {
  const char* d = std::getenv("TMPDIR");
  if (!d || !*d) d = std::getenv("TEMP");
  if (!d || !*d) d = std::getenv("TMP");
#if defined(_WIN32)
  const std::string dir0 = (d && *d) ? std::string(d) : std::string(".");
#else
  const std::string dir0 = (d && *d) ? std::string(d) : std::string("/tmp");
#endif
  const std::string dir = std::filesystem::path(dir0).generic_string();
  const std::string hips = dir + "/astrocs_p3_sampler_cache";
  CHECK(build_mini_hips(hips, kPresent));

  // ── T1: 几何前提自检 (覆盖/未覆盖的判定必须与 fixture 一致) ──────────────
  {
    P3Sampler s{};
    std::string err;
    int order = -1;
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, &order, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    CHECK(order == 0);   // nside=512 ⇒ K=0; fixture 前提 (失败即用例前提失效)
    for (uint32_t k = 0; k < kAll; ++k) {
      double ra = 0, dec = 0;
      tile_center(k, &ra, &dec);
      float v = 0;
      int c = 0;
      CHECK(astrocs::phase3::p3_sample_bilinear(&s, ra, dec, &v, &c) ==
            astrocs::phase3::P3_RS_OK);
      if (k < kPresent) {
        CHECK(c == 1);
        CHECK(std::fabs(v - (100.0f + static_cast<float>(k))) < 1e-3f);
      } else {
        CHECK(c == 0);   // 未写出的 tile ⇒ 覆盖率 0
        CHECK(std::isnan(v));
      }
    }
    astrocs::phase3::p3_sampler_close(&s);
  }

  // ── T2: A —— 重复采样不线性增长 open 次数 (负缓存去重) ───────────────────
  {
    P3Sampler s{};
    std::string err;
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    const uint32_t reps1 = 8, reps2 = 64;
    auto sample_absent = [&](uint32_t reps) {
      for (uint32_t r = 0; r < reps; ++r)
        for (uint32_t k = kPresent; k < kAll; ++k) {
          double ra = 0, dec = 0;
          tile_center(k, &ra, &dec);
          float v = 0;
          int c = -1;
          CHECK(astrocs::phase3::p3_sample_bilinear(&s, ra, dec, &v, &c) ==
                astrocs::phase3::P3_RS_OK);
          CHECK(c == 0);
        }
    };
    sample_absent(reps1);
    P3CacheStats a{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &a);
    const unsigned long long open_after_small = a.open_failures;
    const uint32_t distinct_absent = kAll - kPresent;   // 8
    CHECK(open_after_small <= distinct_absent);   // 每 tile 至多一次真实 open
    CHECK(open_after_small > 0);                  // 确实发生过失败 open
    sample_absent(reps2);                         // 8 倍重复
    P3CacheStats b{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &b);
    // 核心断言: 重复 8 倍, 底层 open 次数**完全不变** (修复前 = 线性增长)
    CHECK(b.open_failures == open_after_small);
    CHECK(b.absent_reads == b.open_failures);     // 每次失败都被负缓存吸收
    CHECK(b.absent_entries >= distinct_absent);

    // ── T3: B —— 阴性对照: 关掉负缓存 ⇒ 同一采样线性增长 (本用例必须能判红) ──
    P3CacheStats before{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &before);
    astrocs::phase3::p3_sampler_set_absent_cache(&s, 0);
    sample_absent(reps2);
    P3CacheStats after{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &after);
    const unsigned long long grew = after.open_failures - before.open_failures;
    // 关闭负缓存后 open 次数 ≈ 采样数 × 四角, 必须远超"去重后每 tile 一次"
    CHECK(grew >= 20ULL * (open_after_small ? open_after_small : 1ULL));
    CHECK(grew >= static_cast<unsigned long long>(reps2) * distinct_absent / 2ULL);
    astrocs::phase3::p3_sampler_set_absent_cache(&s, 1);   // 复位
    astrocs::phase3::p3_sampler_close(&s);
  }

  // ── T4/T5: C/D —— 跨 worker 共享缓存 + 容量有界 ──────────────────────────
  {
    P3Sampler s{};
    std::string err;
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    const int cap = 2;
    astrocs::phase3::p3_sampler_set_max_tiles(&s, cap);

    P3Sampler w1{}, w2{};
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &w1, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &w2, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    astrocs::phase3::p3_sampler_attach_cache(&w1, &s);
    astrocs::phase3::p3_sampler_attach_cache(&w2, &s);

    P3CacheStats base{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &base);
    CHECK(base.cap_tiles == static_cast<unsigned long long>(cap));   // 容量已传播

    // 经 w1 采样全部 4 个 present tile (容量 2 ⇒ 必然逐出)
    for (uint32_t k = 0; k < kPresent; ++k) {
      double ra = 0, dec = 0;
      tile_center(k, &ra, &dec);
      float v = 0;
      int c = 0;
      CHECK(astrocs::phase3::p3_sample_bilinear(&w1, ra, dec, &v, &c) ==
            astrocs::phase3::P3_RS_OK);
      CHECK(c == 1);
    }
    P3CacheStats via_s{}, via_w1{}, via_w2{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &via_s);
    astrocs::phase3::p3_sampler_cache_stats(&w1, &via_w1);
    astrocs::phase3::p3_sampler_cache_stats(&w2, &via_w2);
    // 同一实例: 三个句柄看到逐字段相同的统计
    CHECK(via_s.hits == via_w1.hits && via_w1.hits == via_w2.hits);
    CHECK(via_s.misses == via_w1.misses && via_w1.misses == via_w2.misses);
    CHECK(via_s.cap_tiles == via_w2.cap_tiles);
    CHECK(via_s.resident_tiles <= via_s.cap_tiles);          // D: 有界
    CHECK(via_s.resident_tiles == static_cast<unsigned long long>(cap));
    CHECK(via_s.evictions >= kPresent - static_cast<uint32_t>(cap));  // D: 确实逐出

    // 经 w2 采样"最近载入的 tile"(仍在共享 LRU 中) ⇒ 必须命中**同一份**共享缓存:
    // w2 的热缓存是空的, 命中只可能来自共享实例 ⇒ 主 sampler 观测到 hits 增长。
    {
      double ra = 0, dec = 0;
      tile_center(kPresent - 1, &ra, &dec);
      float v = 0;
      int c = 0;
      CHECK(astrocs::phase3::p3_sample_bilinear(&w2, ra, dec, &v, &c) ==
            astrocs::phase3::P3_RS_OK);
      CHECK(c == 1);
    }
    P3CacheStats after_w2{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &after_w2);
    CHECK(after_w2.hits > via_s.hits);                                 // 共享命中
    CHECK(after_w2.hits + after_w2.misses > via_s.hits + via_s.misses);

    // 对照: 未 attach 的 worker 不共享 —— 其采样不得改变主 sampler 统计
    P3Sampler w3{};
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &w3, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    const P3CacheStats snap = after_w2;
    for (uint32_t k = 0; k < kPresent; ++k) {
      double ra = 0, dec = 0;
      tile_center(k, &ra, &dec);
      float v = 0;
      int c = 0;
      CHECK(astrocs::phase3::p3_sample_bilinear(&w3, ra, dec, &v, &c) ==
            astrocs::phase3::P3_RS_OK);
    }
    P3CacheStats after_w3{};
    astrocs::phase3::p3_sampler_cache_stats(&s, &after_w3);
    CHECK(after_w3.hits == snap.hits && after_w3.misses == snap.misses &&
          after_w3.evictions == snap.evictions);

    astrocs::phase3::p3_sampler_close(&w3);
    astrocs::phase3::p3_sampler_close(&w2);
    astrocs::phase3::p3_sampler_close(&w1);
    astrocs::phase3::p3_sampler_close(&s);
  }

  // ── T6: E —— 缓存状态不改变像素值 ───────────────────────────────────────
  {
    double ra = 0, dec = 0;
    tile_center(0, &ra, &dec);
    P3Sampler a{}, b{};
    std::string err;
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &a, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &b, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    astrocs::phase3::p3_sampler_set_max_tiles(&a, 1);
    astrocs::phase3::p3_sampler_set_absent_cache(&b, 0);
    for (uint32_t k = 0; k < kAll; ++k) {
      double r2 = 0, d2 = 0;
      tile_center(k, &r2, &d2);
      float va = 0, vb = 0;
      int ca = -1, cb = -1;
      CHECK(astrocs::phase3::p3_sample_bilinear(&a, r2, d2, &va, &ca) ==
            astrocs::phase3::P3_RS_OK);
      CHECK(astrocs::phase3::p3_sample_bilinear(&b, r2, d2, &vb, &cb) ==
            astrocs::phase3::P3_RS_OK);
      CHECK(ca == cb);
      CHECK((std::isnan(va) && std::isnan(vb)) || va == vb);
    }
    // 同一像素经共享 worker 采样亦逐位相同
    P3Sampler w{};
    CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &w, nullptr, nullptr, &err) ==
          astrocs::phase3::P3_RS_OK);
    astrocs::phase3::p3_sampler_attach_cache(&w, &a);
    const std::pair<double, double> pts[3] = {{ra, dec}, {ra, dec}, {ra, dec}};
    float prev = 0;
    int first = 1;
    for (const auto& p : pts) {
      float v = 0;
      int c = 0;
      CHECK(astrocs::phase3::p3_sample_bilinear(&w, p.first, p.second, &v, &c) ==
            astrocs::phase3::P3_RS_OK);
      if (first) { prev = v; first = 0; }
      CHECK(v == prev);
    }
    astrocs::phase3::p3_sampler_close(&w);
    astrocs::phase3::p3_sampler_close(&b);
    astrocs::phase3::p3_sampler_close(&a);
  }

  if (failures == 0) std::fprintf(stdout, "P3 SAMPLER CACHE PASS\n");
  else std::fprintf(stdout, "P3 SAMPLER CACHE FAIL (%d checks)\n", failures);
  return failures == 0 ? 0 : 1;
}
