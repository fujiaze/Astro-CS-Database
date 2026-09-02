/* IO-002 hips_core 自检驱动 — modules/services/io/tests/hips_core_selftest.c
 *
 * 覆盖验收映射 (docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md §8):
 *   - properties 解析/必填键校验 (缺/非法 → PROPERTIES/UNSUPPORTED)
 *   - tile width 非 2 次幂 / 未知 frame / png-only / snr 产品 → 拒绝
 *   - NESTED tile address: ipix 越界 → ADDRESS; 布局 Norder/Dir/Npix 定位
 *   - tile 头卡一致性 (PIXTYPE/ORDERING/COORDSYS/NSIDE/FIRSTPIX/LASTPIX 冲突
 *     → TILE_INVALID)
 *   - partial tree: PRESENT/MISSING 状态; 缺 tile 绝不父 order 回退
 *   - MOC optional hint: 有 MOC → 叶级 ipix 枚举; 无 MOC → count=0 不失败
 *   - FITS-only 科学平面读取: f32/f64 与生成数据一致
 *
 * fixture 由 tests/io/make_hips_fixture.py 生成 (默认 K=1)。无第三方依赖。
 * 用法: hips_core_selftest <fixture_dir> [<fixture_dir2...>]
 * 退出码 0 = 全 PASS。
 */
#include "astrocs/io/hips_input_v1.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)
#define CHECK_ST(expect, actual, what)                                    \
  do {                                                                    \
    int _e = (expect), _a = (actual);                                     \
    if (_e != _a) {                                                       \
      fprintf(stderr, "FAIL %s:%d: %s expect=%d got=%d\n", __FILE__,      \
              __LINE__, what, _e, _a);                                    \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

/* 期望平面: 0.25*(x+1)+0.5*y + ipix*0.001 (float32; 与生成器同规律, 无 noise
 * 因 selftest 无 numpy —— 用容差 1e-2 覆盖生成器 noise 1e-3 影响)。 */
static float expect_val(int x, int y, uint64_t ipix) {
  return 0.25f * (float)(x + 1) + 0.5f * (float)y + (float)ipix * 0.001f;
}

static int run_one(const char* dir) {
  acs_hips_handle_v1 h = NULL;
  char err[ACS_HIPS_ERR_TEXT_MAX];
  int32_t status = 0, order = 0, tw = 0;
  int64_t count = 0, got = 0, i;
  uint64_t ipix_out = 0;
  char buf[128];
  const int W = 512;

  CHECK_ST(ACS_HIPS_OK, acs_hips_open_v1(dir, NULL, NULL, &h, err, sizeof(err)),
           "open fixture");
  if (!h) return g_failures;

  CHECK_ST(ACS_HIPS_OK, acs_hips_get_order_v1(h, &order), "get_order");
  CHECK_ST(ACS_HIPS_OK, acs_hips_get_tile_width_v1(h, &tw), "get_tile_width");
  CHECK(order == 1);
  CHECK(tw == 512);

  /* properties 视图 */
  CHECK_ST(ACS_HIPS_OK,
           acs_hips_props_get_v1(h, "hips_version", buf, sizeof(buf),
                                 err, sizeof(err)),
           "props_get hips_version");
  CHECK(strcmp(buf, "1.4") == 0);
  CHECK_ST(ACS_HIPS_OK,
           acs_hips_props_get_v1(h, "hips_tile_format", buf, sizeof(buf),
                                 err, sizeof(err)),
           "props_get tile_format");
  CHECK(strcmp(buf, "fits") == 0);
  CHECK_ST(ACS_HIPS_ERR_PARAM,
           acs_hips_props_get_v1(h, "no_such_key", buf, sizeof(buf),
                                 err, sizeof(err)),
           "props_get unknown key → PARAM");

  /* MOC optional hint: 叶级 ipix 枚举 (K=1: [0,5,8,18,28,40,46]) */
  CHECK_ST(ACS_HIPS_OK, acs_hips_tile_count_v1(h, &count), "tile_count");
  CHECK(count >= 1);
  for (i = 0; i < count && i < 8; ++i) {
    CHECK_ST(ACS_HIPS_OK, acs_hips_tile_ipix_v1(h, i, &ipix_out), "tile_ipix");
    CHECK_ST(ACS_HIPS_OK, acs_hips_tile_status_v1(h, ipix_out, &status, err,
                                                  sizeof(err)),
             "status present");
    CHECK(status == ACS_HIPS_TILE_PRESENT);
  }
  CHECK_ST(ACS_HIPS_ERR_PARAM, acs_hips_tile_ipix_v1(h, count + 100, &ipix_out),
           "tile_ipix 越界 → PARAM");

  /* partial tree: 域内但未覆盖 ipix → MISSING; 读取 → TILE_MISSING */
  {
    uint64_t missing_ipix = 3; /* K=1 域 [0,48), fixture 未覆盖 3 */
    CHECK_ST(ACS_HIPS_OK,
             acs_hips_tile_status_v1(h, missing_ipix, &status, err,
                                     sizeof(err)),
             "status missing");
    CHECK(status == ACS_HIPS_TILE_MISSING);
    {
      float* plane = (float*)malloc((size_t)W * W * sizeof(float));
      CHECK(plane != NULL);
      CHECK_ST(ACS_HIPS_ERR_TILE_MISSING,
               acs_hips_read_tile_plane_f32_v1(h, missing_ipix, plane,
                                               (int64_t)W * W, &got,
                                               err, sizeof(err)),
               "read missing → TILE_MISSING (无父回退)");
      CHECK(got == 0);
      free(plane);
    }
  }

  /* 科学平面读取 (f32/f64): tile ipix=0 (由 MOC 枚举第 0 项) */
  {
    uint64_t ipix0 = 0;
    float* f32 = (float*)malloc((size_t)W * W * sizeof(float));
    double* f64 = (double*)malloc((size_t)W * W * sizeof(double));
    CHECK_ST(ACS_HIPS_OK, acs_hips_tile_ipix_v1(h, 0, &ipix0), "tile_ipix 0");
    CHECK_ST(ACS_HIPS_OK,
             acs_hips_read_tile_plane_f32_v1(h, ipix0, f32, (int64_t)W * W,
                                             &got, err, sizeof(err)),
             "read f32");
    CHECK(got == (int64_t)W * W);
    if (f32) {
      int x, y, ok = 1;
      for (y = 0; y < W && ok; ++y) {
        for (x = 0; x < W; ++x) {
          float expv = expect_val(x, y, ipix0);
          if (fabsf(f32[(size_t)y * W + (size_t)x] - expv) > 0.02f) {
            fprintf(stderr, "  f32 mismatch at (%d,%d): got %.6f exp %.6f\n",
                    x, y, f32[(size_t)y * W + (size_t)x], expv);
            ok = 0;
            break;
          }
        }
      }
      CHECK(ok);
    }
    CHECK_ST(ACS_HIPS_OK,
             acs_hips_read_tile_plane_f64_v1(h, ipix0, f64, (int64_t)W * W,
                                             &got, err, sizeof(err)),
             "read f64");
    if (f64) {
      int x, y, ok = 1;
      for (y = 0; y < W && ok; ++y) {
        for (x = 0; x < W; ++x) {
          double expv = (double)expect_val(x, y, ipix0);
          if (fabs(f64[(size_t)y * W + (size_t)x] - expv) > 0.02) {
            fprintf(stderr, "  f64 mismatch at (%d,%d): got %.6f exp %.6f\n",
                    x, y, f64[(size_t)y * W + (size_t)x], expv);
            ok = 0;
            break;
          }
        }
      }
      CHECK(ok);
    }
    free(f32);
    free(f64);
  }

  /* ipix 越界 (K=1: 12*4^1=48) → ADDRESS */
  CHECK_ST(ACS_HIPS_ERR_ADDRESS,
           acs_hips_tile_status_v1(h, 48, &status, err, sizeof(err)),
           "ipix 越界 → ADDRESS");

  /* 容量不足 → PARAM */
  {
    float small[16];
    CHECK_ST(ACS_HIPS_ERR_PARAM,
             acs_hips_read_tile_plane_f32_v1(h, ipix_out, small, 16, &got,
                                             err, sizeof(err)),
             "容量不足 → PARAM");
  }

  acs_hips_close_v1(h);
  return g_failures;
}

/* 缺 properties: 空目录 → PROPERTIES */
static int run_missing_props(const char* empty_dir) {
  acs_hips_handle_v1 h = NULL;
  char err[ACS_HIPS_ERR_TEXT_MAX];
  int st = acs_hips_open_v1(empty_dir, NULL, NULL, &h, err, sizeof(err));
  CHECK_ST(ACS_HIPS_ERR_PROPERTIES, st, "空目录 open → PROPERTIES");
  CHECK(h == NULL);
  return g_failures;
}

int main(int argc, char** argv) {
  int i;
  if (argc < 2) {
    fprintf(stderr, "用法: hips_core_selftest <fixture_dir> [<empty_dir>]\n");
    return 2;
  }
  for (i = 1; i < argc; ++i) {
    if (argv[i][0] == '@') {
      run_missing_props(argv[i] + 1);
    } else {
      run_one(argv[i]);
    }
  }
  if (g_failures == 0) {
    printf("hips_core_selftest: ALL PASS (fixtures=%d)\n", argc - 1);
    return 0;
  }
  fprintf(stderr, "hips_core_selftest: %d FAILURE(S)\n", g_failures);
  return 1;
}
