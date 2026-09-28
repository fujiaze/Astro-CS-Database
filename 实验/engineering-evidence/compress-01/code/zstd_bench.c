/* COMPRESS-01: zstd 多档压缩率/时长基准 (in-process, 无进程启动开销)
 *
 * 用法: zstd_bench <file...> [--levels -7,-5,-3,-1,1,3,5,9,12,15,19,22]
 *                   [--reps-c 5] [--reps-d 9] [--shuffle 4|8|0] [--csv out.csv]
 * 输出: 每 (file, level) 一行 CSV: file,level,shuffle,orig,comp,ratio,c_ms,d_ms,c_mbps,d_mbps,ok
 * 计时: CLOCK_MONOTONIC, 取中位数; 每档先 1 次预热 (不计时).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <zstd.h>

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static int cmp_d(const void* a, const void* b) {
  double x = *(const double*)a, y = *(const double*)b;
  return (x > y) - (x < y);
}
static double median(double* v, int n) { qsort(v, n, sizeof(double), cmp_d); return v[n / 2]; }

/* byte shuffle: 把 n 字节按 esize 分组做转置 (BLOSCAR shuffle) */
static void shuffle_bytes(const unsigned char* src, unsigned char* dst, size_t n, int esize) {
  size_t nelem = n / esize;
  for (int b = 0; b < esize; b++)
    for (size_t i = 0; i < nelem; i++) dst[(size_t)b * nelem + i] = src[i * esize + b];
}
static void unshuffle_bytes(const unsigned char* src, unsigned char* dst, size_t n, int esize) {
  size_t nelem = n / esize;
  for (int b = 0; b < esize; b++)
    for (size_t i = 0; i < nelem; i++) dst[i * esize + b] = src[(size_t)b * nelem + i];
}

int main(int argc, char** argv) {
  const char* levels_s = "-7,-5,-3,-1,1,3,5,9,12,15,19,22";
  int reps_c = 5, reps_d = 9, shuffle = 0;
  const char* csv_path = NULL;
  const char* files[4096];
  int nf = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--levels") && i + 1 < argc) levels_s = argv[++i];
    else if (!strcmp(argv[i], "--reps-c") && i + 1 < argc) reps_c = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--reps-d") && i + 1 < argc) reps_d = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--shuffle") && i + 1 < argc) shuffle = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else files[nf++] = argv[i];
  }
  int levels[64]; int nl = 0;
  { char* s = strdup(levels_s); char* tok = strtok(s, ",");
    while (tok && nl < 64) { levels[nl++] = atoi(tok); tok = strtok(NULL, ","); } }

  FILE* csv = csv_path ? fopen(csv_path, "w") : stdout;
  fprintf(csv, "file,level,shuffle,orig,comp,ratio,c_ms,d_ms,c_mbps,d_mbps,ok\n");

  ZSTD_CCtx* cctx = ZSTD_createCCtx();
  ZSTD_DCtx* dctx = ZSTD_createDCtx();
  for (int fi = 0; fi < nf; fi++) {
    FILE* f = fopen(files[fi], "rb");
    if (!f) { fprintf(stderr, "open fail %s\n", files[fi]); continue; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* src = malloc(sz);
    if (fread(src, 1, sz, f) != (size_t)sz) { fprintf(stderr, "read fail %s\n", files[fi]); fclose(f); free(src); continue; }
    fclose(f);
    unsigned char* shuf = NULL, *unshuf = NULL, *shuf2 = NULL;
    const unsigned char* input = src;
    if (shuffle > 0 && (sz % shuffle) == 0) {
      shuf = malloc(sz); shuf2 = malloc(sz);
      shuffle_bytes(src, shuf, sz, shuffle); input = shuf;
    }
    size_t bound = ZSTD_compressBound(sz);
    unsigned char* comp = malloc(bound);
    unsigned char* dec = malloc(sz);
    const char* base = strrchr(files[fi], '/'); base = base ? base + 1 : files[fi];
    for (int li = 0; li < nl; li++) {
      int lv = levels[li];
      /* 预热 + 正确性 */
      size_t csz = ZSTD_compressCCtx(cctx, comp, bound, input, sz, lv);
      if (ZSTD_isError(csz)) { fprintf(stderr, "compress err %s lv=%d: %s\n", base, lv, ZSTD_getErrorName(csz)); continue; }
      size_t dsz = ZSTD_decompressDCtx(dctx, dec, sz, comp, csz);
      int ok = (dsz == (size_t)sz);
      if (ok && shuf) { unshuf = malloc(sz); unshuffle_bytes(dec, unshuf, sz, shuffle); ok = (memcmp(unshuf, src, sz) == 0); free(unshuf); unshuf = NULL; }
      else if (ok) ok = (memcmp(dec, src, sz) == 0);
      double* tc = malloc(sizeof(double) * reps_c);
      for (int r = 0; r < reps_c; r++) {
        double t0 = now_ms();
        ZSTD_compressCCtx(cctx, comp, bound, input, sz, lv);
        tc[r] = now_ms() - t0;
      }
      double* td = malloc(sizeof(double) * reps_d);
      for (int r = 0; r < reps_d; r++) {
        double t0 = now_ms();
        ZSTD_decompressDCtx(dctx, dec, sz, comp, csz);
        td[r] = now_ms() - t0;
      }
      double cm = median(tc, reps_c), dm = median(td, reps_d);
      fprintf(csv, "%s,%d,%d,%ld,%zu,%.6f,%.4f,%.4f,%.3f,%.3f,%d\n", base, lv, shuffle, sz, csz,
              (double)csz / (double)sz, cm, dm, (sz / 1048576.0) / (cm / 1000.0), (sz / 1048576.0) / (dm / 1000.0), ok);
      free(tc); free(td);
      fflush(csv);
    }
    free(comp); free(dec); free(src); free(shuf); free(shuf2);
  }
  ZSTD_freeCCtx(cctx); ZSTD_freeDCtx(dctx);
  if (csv != stdout) fclose(csv);
  return 0;
}
