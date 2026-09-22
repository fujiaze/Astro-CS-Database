/* COMPRESS-01: FITS 原生压缩 (fpack 口径) 基准 —— 直接走 cfitsio 的
 * fits_img_compress / fits_img_decompress (fpack 内部即调用这两个), 因此
 * 结果与 fpack/funpack 命令行一致 (本机未安装 fpack 二进制, 故直接调库).
 *
 * 用法: fits_bench <file.bin...> [--codecs RICE_1,RICE_1_q0,GZIP_1,GZIP_2]
 *                    [--tile 512x512|512x1|default] [--reps-c 5] [--reps-d 9]
 *                    [--csv out.csv]
 * 输入 .bin = 512x512 big-endian f32 原始数据段.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "fitsio.h"

#define NX 512
#define NY 512
#define NPIX (NX * NY)

static double now_ms(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static int cmp_d(const void* a, const void* b) { double x = *(double*)a, y = *(double*)b; return (x > y) - (x < y); }
static double median(double* v, int n) { qsort(v, n, sizeof(double), cmp_d); return v[n / 2]; }

static const char* ferr(int st) { static char buf[80]; ffgerr(st, buf); return buf; }

/* 文件名 -> 去掉路径 */
static const char* base_of(const char* p) { const char* b = strrchr(p, '/'); return b ? b + 1 : p; }

int main(int argc, char** argv) {
  const char* codecs_s = "RICE_1,RICE_1_q0,GZIP_1,GZIP_2";
  const char* tile_s = "512x512";
  int reps_c = 5, reps_d = 9;
  const char* csv_path = NULL;
  const char* files[2048]; int nf = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--codecs") && i + 1 < argc) codecs_s = argv[++i];
    else if (!strcmp(argv[i], "--tile") && i + 1 < argc) tile_s = argv[++i];
    else if (!strcmp(argv[i], "--reps-c") && i + 1 < argc) reps_c = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--reps-d") && i + 1 < argc) reps_d = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else files[nf++] = argv[i];
  }
  char codecs[32][32]; int nc = 0;
  { char* s = strdup(codecs_s); char* t = strtok(s, ","); while (t && nc < 32) { snprintf(codecs[nc++], 32, "%s", t); t = strtok(NULL, ","); } }

  long tdims[2] = {NX, NY};
  int use_tile = 1;
  if (!strcmp(tile_s, "default")) use_tile = 0;
  else if (!strcmp(tile_s, "512x1")) { tdims[0] = NX; tdims[1] = 1; }
  else { tdims[0] = NX; tdims[1] = NY; }

  FILE* csv = csv_path ? fopen(csv_path, "w") : stdout;
  fprintf(csv, "file,codec,tile,orig,comp,ratio,c_ms,d_ms,c_mbps,d_mbps,bitwise_eq,lossless,status\n");

  const char* tmpdir = getenv("COMPRESS01_TMP");
  if (!tmpdir) tmpdir = "/tmp";
  char raw_path[1024], cmp_path[1024], unp_path[1024];
  snprintf(raw_path, sizeof raw_path, "%s/c01_raw.fits", tmpdir);
  snprintf(cmp_path, sizeof cmp_path, "%s/c01_cmp.fits", tmpdir);
  snprintf(unp_path, sizeof unp_path, "%s/c01_unp.fits", tmpdir);

  float* native = malloc(sizeof(float) * NPIX);
  for (int fi = 0; fi < nf; fi++) {
    FILE* f = fopen(files[fi], "rb");
    if (!f) { fprintf(stderr, "open fail %s\n", files[fi]); continue; }
    unsigned char* raw = malloc(NPIX * 4);
    if (fread(raw, 1, NPIX * 4, f) != NPIX * 4) { fprintf(stderr, "size != 1MiB: %s\n", files[fi]); fclose(f); free(raw); continue; }
    fclose(f);
    /* big-endian -> native */
    for (int i = 0; i < NPIX; i++) {
      uint32_t v = ((uint32_t)raw[i*4] << 24) | ((uint32_t)raw[i*4+1] << 16) | ((uint32_t)raw[i*4+2] << 8) | raw[i*4+3];
      memcpy(&native[i], &v, 4);
    }
    /* 写未压缩 FITS */
    int st = 0; fitsfile* of = NULL;
    remove(raw_path);
    if (fits_create_file(&of, raw_path, &st)) { fprintf(stderr, "create %s\n", ferr(st)); continue; }
    long naxes[2] = {NX, NY};
    fits_create_img(of, FLOAT_IMG, 2, naxes, &st);
    fits_write_img(of, TFLOAT, 1, NPIX, native, &st);
    fits_close_file(of, &st);
    long rawsz = 0; { FILE* g = fopen(raw_path, "rb"); fseek(g, 0, SEEK_END); rawsz = ftell(g); fclose(g); }

    for (int ci = 0; ci < nc; ci++) {
      /* cfitsio 对 float/double 图像**总是**施加量化 (默认 noise_bits=4, 与算法无关);
       * fpack 的 -q 0 即关量化 => 无损。_q0 后缀 = 无损口径, 无后缀 = fpack 默认 (有损)。 */
      int ctype = -1, noisebits = 4, do_quant = 1;
      const char* cn = codecs[ci];
      char base_cn[32]; snprintf(base_cn, sizeof base_cn, "%s", cn);
      { char* p = strstr(base_cn, "_q0"); if (p) { *p = 0; do_quant = 0; } }
      if (!strcmp(base_cn, "RICE_1")) { ctype = RICE_1; }
      else if (!strcmp(base_cn, "GZIP_1")) { ctype = GZIP_1; }
      else if (!strcmp(base_cn, "GZIP_2")) { ctype = GZIP_2; }
      else if (!strcmp(base_cn, "PLIO_1")) { ctype = PLIO_1; }
      else { fprintf(stderr, "unknown codec %s\n", cn); continue; }
      noisebits = do_quant ? 4 : 0;
      if (!do_quant && ctype == RICE_1) { /* 保留: 让 cfitsio 自己报 413, 作为负例证据 */ }

      /* --- 压缩 (计时) --- */
      double* tc = malloc(sizeof(double) * reps_c);
      long csz = 0; int compress_rc = 0;
      for (int r = 0; r <= reps_c; r++) {
        int s2 = 0; fitsfile *inf = NULL, *outf = NULL;
        remove(cmp_path);
        if (fits_open_file(&inf, raw_path, READONLY, &s2)) { fprintf(stderr, "open raw: %s\n", ferr(s2)); break; }
        if (fits_create_file(&outf, cmp_path, &s2)) { fprintf(stderr, "create cmp: %s\n", ferr(s2)); break; }
        fits_set_compression_type(outf, ctype, &s2);
        /* 无损口径: quantize_level=0 (= fpack -q 0)。实测 cfitsio 4.6.2 下 RICE_1 拒绝该设置
         * (status 413 "error compressing image"), 只有 GZIP_1/GZIP_2 支持 -> 如实记录。 */
        if (noisebits == 0) fits_set_quantize_level(outf, 0.0f, &s2);
        if (use_tile) fits_set_tile_dim(outf, 2, tdims, &s2);
        double t0 = now_ms();
        int cok = !fits_img_compress(inf, outf, &s2);
        if (!cok) { fprintf(stderr, "img_compress %s: %s\n", cn, ferr(s2)); compress_rc = s2; }
        double dt = now_ms() - t0;
        fits_close_file(outf, &s2); fits_close_file(inf, &s2);
        if (r == 0) { FILE* g = fopen(cmp_path, "rb"); if (g) { fseek(g, 0, SEEK_END); csz = ftell(g); fclose(g); } }
        if (compress_rc) { csz = -1; break; }
        else if (r > 0) tc[r - 1] = dt;   /* r==0 为预热, 不计时 (修复: 曾写成 tc[-1] 越界) */
      }

      /* --- 解压 (计时): fits_read_img 透明解压 --- */
      double* td = malloc(sizeof(double) * reps_d);
      float* back = malloc(sizeof(float) * NPIX);
      int bitwise = 0;
      if (compress_rc) { fprintf(csv, "%s,%s,%s,%ld,-1,0,0,0,0,0,0,fail,%d\n",
            base_of(files[fi]), cn, use_tile ? tile_s : "default", rawsz, compress_rc); fflush(csv);
        free(tc); free(td); free(back); continue; }
      for (int r = 0; r <= reps_d; r++) {
        int s2 = 0; fitsfile* cf = NULL;
        if (fits_open_file(&cf, cmp_path, READONLY, &s2)) { fprintf(stderr, "open cmp: %s\n", ferr(s2)); break; }
        fits_movabs_hdu(cf, 2, NULL, &s2);
        double t0 = now_ms();
        fits_read_img(cf, TFLOAT, 1, NPIX, NULL, back, NULL, &s2);
        double dt = now_ms() - t0;
        fits_close_file(cf, &s2);
        if (r == 0) bitwise = (memcmp(back, native, NPIX * 4) == 0);
        else td[r - 1] = dt;
      }
      double cm = median(tc, reps_c), dm = median(td, reps_d);
      fprintf(csv, "%s,%s,%s,%ld,%ld,%.6f,%.4f,%.4f,%.3f,%.3f,%d,%s,%d\n",
              base_of(files[fi]), cn, use_tile ? tile_s : "default", rawsz, csz,
              (double)csz / (double)rawsz, cm, dm,
              (rawsz / 1048576.0) / (cm / 1000.0), (rawsz / 1048576.0) / (dm / 1000.0),
              bitwise, bitwise ? "yes" : "no", csz > 0 ? 0 : compress_rc);
      fflush(csv);
      free(tc); free(td); free(back);
    }
    free(raw);
  }
  free(native);
  remove(raw_path); remove(cmp_path); remove(unp_path);
  if (csv != stdout) fclose(csv);
  return 0;
}