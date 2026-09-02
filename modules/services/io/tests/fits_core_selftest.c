/* IO-001 fits_core 自检驱动 — modules/services/io/tests/fits_core_selftest.c
 *
 * 覆盖验收映射 (docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md §12):
 *   - 非法 header (SIMPLE 缺失/无 END/BITPIX 非法)  → BAD_HEADER
 *   - 截断 (header 内/数据区一半)                   → TRUNCATED
 *   - dtype/shape/unit mismatch                     → MISMATCH
 *   - checksum error (篡改后 verify)                → CHECKSUM
 *   - NaN/Inf (strict 策略)                         → NANINF
 *   - 取消 (注入置位回调)                           → CANCELLED
 *   - 磁盘满/写失败 (只读目录)                      → IO/DISKFULL
 *   - read/write bytes 记录                         → *_bytes_*_v1 断言 + hook
 *   - 正常往返: 原子写 → header/plane/chunk 读回     → OK + 逐元素一致
 *
 * 纯 C11; 无第三方依赖; 退出码 0=全 PASS。
 */
#include "astrocs/io/fits_stream_v1.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#define getpid _getpid
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

static char g_dir[512];

static const char* tmp_path(const char* name) {
  static char buf[600];
  snprintf(buf, sizeof(buf), "%s/%s", g_dir, name);
  return buf;
}

/* trace hooks: 累计回调 + cancel 开关 */
static uint64_t g_cb_read = 0, g_cb_write = 0;
static int g_cancel_flag = 0;
static void on_read(void* ud, uint64_t b) { (void)ud; g_cb_read += b; }
static void on_write(void* ud, uint64_t b) { (void)ud; g_cb_write += b; }
static int is_cancel(void* ud) { (void)ud; return g_cancel_flag; }

static acs_fio_trace_hooks_v1 g_hooks = ACS_FIO_TRACE_HOOKS(NULL, on_read, on_write, is_cancel);

/* ── 1. 正常往返: F32 2x3 单平面 ── */
static void test_roundtrip_f32(void) {
  const char* path = tmp_path("rt_f32.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  acs_fio_header_v1 hdr;
  float data[6] = {1.0f, -2.5f, 3.25f, 4e10f, -1e-30f, 123.456f};
  float back[6] = {0};
  float chunk[2] = {0};
  int64_t got = 0;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;

  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 3; /* NAXIS1 */
  decl.naxis_n[1] = 2; /* NAXIS2 */
  st = acs_fio_writer_begin_v1(path, &decl, "ct", 0, &g_hooks, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "writer_begin");
  st = acs_fio_write_plane_v1(wr, 0, data, sizeof(data), &g_hooks, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "write_plane");
  st = acs_fio_writer_end_v1(wr, 1, 0, 1, &g_hooks, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "writer_end");
  CHECK(acs_fio_writer_bytes_written_v1(wr) >= sizeof(data));

  /* header 读回 */
  st = acs_fio_reader_open_v1(path, &g_hooks, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "reader_open");
  memset(&hdr, 0, sizeof(hdr));
  hdr.struct_size = (uint32_t)sizeof(hdr);
  hdr.abi_version = ACS_FIO_ABI_VERSION_V1;
  st = acs_fio_get_header_v1(rd, &hdr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "get_header");
  CHECK(hdr.bitpix == ACS_FIO_BITPIX_F32);
  CHECK(hdr.naxis == 2);
  CHECK(hdr.naxis_n[0] == 3 && hdr.naxis_n[1] == 2);
  {
    int found_bunit = 0, i;
    for (i = 0; i < hdr.keyword_count; i++)
      if (strcmp(hdr.keywords[i].name, "BUNIT") == 0) found_bunit = 1;
    CHECK(found_bunit);
  }
  /* plane 读回 */
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, NULL, back, 6, 0, &got, &g_hooks,
                             err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "read_plane");
  CHECK(got == 6);
  {
    int i;
    for (i = 0; i < 6; i++) CHECK(back[i] == data[i]);
  }
  CHECK(acs_fio_reader_bytes_read_v1(rd) > 0);
  /* chunk 读回 */
  memset(chunk, 0, sizeof(chunk));
  st = acs_fio_read_chunk_v1(rd, 0, 2, 2, chunk, 2, 0, &got, &g_hooks, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "read_chunk");
  CHECK(got == 2);
  CHECK(chunk[0] == data[2] && chunk[1] == data[3]);
  acs_fio_reader_close_v1(rd);
  CHECK(g_cb_read > 0 && g_cb_write > 0); /* hook 回调被触发 */
  remove(path);
}

/* ── 2. F64 双平面 + plane/chunk 索引语义 ── */
static void test_roundtrip_f64_2plane(void) {
  const char* path = tmp_path("rt_f64_2p.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  double p0[4] = {0.5, -1.25, 2.0, -3.75};
  double p1[4] = {100.0, -200.0, 300.5, -400.25};
  double back0[4] = {0}, back1[4] = {0};
  int64_t got = 0;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F64;
  decl.naxis = 3;
  decl.naxis_n[0] = 2;
  decl.naxis_n[1] = 2;
  decl.naxis_n[2] = 2;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  st = acs_fio_write_plane_v1(wr, 0, p0, sizeof(p0), NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "w p0");
  st = acs_fio_write_plane_v1(wr, 1, p1, sizeof(p1), NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "w p1");
  st = acs_fio_writer_end_v1(wr, 1, 1, 1, NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "end (datasum+checksum)");
  st = acs_fio_reader_open_v1(path, NULL, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "open");
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, NULL, back0, 4, 0, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "r p0");
  CHECK(got == 4 && back0[0] == p0[0] && back0[3] == p0[3]);
  st = acs_fio_read_plane_v1(rd, 1, 0, 0, 0, NULL, back1, 4, 0, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "r p1");
  CHECK(back1[0] == p1[0] && back1[3] == p1[3]);
  acs_fio_reader_close_v1(rd);
  /* checksum verify (写了 CHECKSUM) */
  st = acs_fio_verify_file_v1(path, 1, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "verify ok");
  remove(path);
}

/* ── 3. 负测: 非法 header ── */
static void write_raw(const char* path, const unsigned char* b, size_t n) {
  FILE* fp = fopen(path, "wb");
  CHECK(fp != NULL);
  if (fp) {
    fwrite(b, 1, n, fp);
    fclose(fp);
  }
}

/* 构造一张标准 FITS 卡: "NAME    = VALUE" 到 80 字节卡, 值列于 10 起 */
static void put_card(unsigned char* raw, int card_idx, const char* name,
                     const char* value) {
  char* card = (char*)raw + card_idx * 80;
  int i;
  for (i = 0; i < 80; i++) card[i] = ' ';
  snprintf(card, 80, "%-8s= %s", name, value);
}

static void test_bad_header_missing_simple(void) {
  unsigned char raw[2880];
  acs_fio_reader_v1* rd = NULL;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  memset(raw, ' ', sizeof(raw));
  put_card(raw, 0, "BITPIX", "-32");
  put_card(raw, 1, "NAXIS", "2");
  put_card(raw, 2, "NAXIS1", "3");
  put_card(raw, 3, "NAXIS2", "2");
  memcpy(raw + 4 * 80, "END", 3);
  write_raw(tmp_path("bad_nosimple.fits"), raw, sizeof(raw));
  st = acs_fio_reader_open_v1(tmp_path("bad_nosimple.fits"), NULL, &rd, err,
                              sizeof(err));
  CHECK_ST(ACS_FIO_ERR_BAD_HEADER, st, "open missing SIMPLE");
  remove(tmp_path("bad_nosimple.fits"));
}

static void test_bad_header_no_end(void) {
  unsigned char raw[2880];
  acs_fio_reader_v1* rd = NULL;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  memset(raw, ' ', sizeof(raw));
  put_card(raw, 0, "SIMPLE", "T");
  put_card(raw, 1, "BITPIX", "-32");
  put_card(raw, 2, "NAXIS", "0");
  /* 无 END */
  write_raw(tmp_path("bad_noend.fits"), raw, sizeof(raw));
  st = acs_fio_reader_open_v1(tmp_path("bad_noend.fits"), NULL, &rd, err,
                              sizeof(err));
  CHECK_ST(ACS_FIO_ERR_BAD_HEADER, st, "open no END");
  remove(tmp_path("bad_noend.fits"));
}

static void test_bad_header_bad_bitpix(void) {
  unsigned char raw[2880];
  acs_fio_reader_v1* rd = NULL;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  memset(raw, ' ', sizeof(raw));
  put_card(raw, 0, "SIMPLE", "T");
  put_card(raw, 1, "BITPIX", "7"); /* 非法 BITPIX */
  put_card(raw, 2, "NAXIS", "0");
  memcpy(raw + 3 * 80, "END", 3);
  write_raw(tmp_path("bad_bitpix.fits"), raw, sizeof(raw));
  st = acs_fio_reader_open_v1(tmp_path("bad_bitpix.fits"), NULL, &rd, err,
                              sizeof(err));
  CHECK_ST(ACS_FIO_ERR_UNSUPPORTED, st, "open bad bitpix");
  remove(tmp_path("bad_bitpix.fits"));
}

/* ── 4. 负测: 截断 ── */
static void test_truncated_data(void) {
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  const char* path = tmp_path("trunc.fits");
  float data[64];
  int i;
  remove(path);
  for (i = 0; i < 64; i++) data[i] = (float)i;
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 8;
  decl.naxis_n[1] = 8;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  st = acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "w");
  st = acs_fio_writer_end_v1(wr, 0, 0, 0, NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "end");
  /* 截断: 保留 header + 一半数据 */
  {
    FILE* fp = fopen(path, "rb");
    long full_len;
    CHECK(fp != NULL);
    fseek(fp, 0, SEEK_END);
    full_len = ftell(fp);
    fclose(fp);
    CHECK(full_len > 3000);
    /* 截到 2880(header) + 128 字节数据 */
    {
      FILE* in = fopen(path, "rb");
      unsigned char* buf = (unsigned char*)malloc((size_t)full_len);
      FILE* out;
      size_t keep = 2880 + 128;
      CHECK(in != NULL);
      fread(buf, 1, (size_t)full_len, in);
      fclose(in);
      out = fopen(path, "wb");
      fwrite(buf, 1, keep, out);
      fclose(out);
      free(buf);
    }
  }
  st = acs_fio_reader_open_v1(path, NULL, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_ERR_TRUNCATED, st, "open truncated");
  remove(path);
}

/* ── 5. 负测: dtype / shape / unit mismatch ── */
static void test_mismatch(void) {
  const char* path = tmp_path("mm.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  float data[6] = {1, 2, 3, 4, 5, 6};
  double dbuf[6];
  float fbuf[6];
  int64_t got = 0;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 3;
  decl.naxis_n[1] = 2;
  st = acs_fio_writer_begin_v1(path, &decl, "adu", 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  acs_fio_writer_end_v1(wr, 0, 0, 0, NULL, err, sizeof(err));
  st = acs_fio_reader_open_v1(path, NULL, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "open");
  /* dtype mismatch: 期望 F64 */
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, ACS_FIO_BITPIX_F64, NULL, dbuf, 6, 0,
                             &got, NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_ERR_MISMATCH, st, "dtype mismatch");
  /* shape mismatch: nx=4 */
  st = acs_fio_read_plane_v1(rd, 0, 4, 0, 0, NULL, fbuf, 6, 0, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_ERR_MISMATCH, st, "shape mismatch");
  /* unit mismatch: 期望 'e-' */
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, "e-", fbuf, 6, 0, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_ERR_MISMATCH, st, "unit mismatch");
  acs_fio_reader_close_v1(rd);
  remove(path);
}

/* ── 6. checksum error: 写 DATASUM 后篡改 → verify CHECKSUM ── */
static void test_checksum_error(void) {
  const char* path = tmp_path("cs.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_header_v1 decl;
  float data[64];
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st, i;
  remove(path);
  for (i = 0; i < 64; i++) data[i] = (float)i;
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 8;
  decl.naxis_n[1] = 8;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  st = acs_fio_writer_end_v1(wr, 1, 0, 0, NULL, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "end");
  /* verify 通过 */
  st = acs_fio_verify_file_v1(path, 0, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "verify ok before tamper");
  /* 篡改数据区一个字节 (header 2880 后) */
  {
    FILE* fp = fopen(path, "r+b");
    CHECK(fp != NULL);
    if (fp) {
      unsigned char b;
      fseek(fp, 2880, SEEK_SET);
      fread(&b, 1, 1, fp);
      b ^= 0x01;
      fseek(fp, 2880, SEEK_SET);
      fwrite(&b, 1, 1, fp);
      fclose(fp);
    }
  }
  st = acs_fio_verify_file_v1(path, 0, err, sizeof(err));
  CHECK_ST(ACS_FIO_ERR_CHECKSUM, st, "verify after tamper");
  remove(path);
}

/* ── 7. NaN/Inf strict ── */
static void test_naninf(void) {
  const char* path = tmp_path("nan.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  float data[4] = {1.0f, NAN, 3.0f, INFINITY};
  float back[4];
  int64_t got = 0;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 2;
  decl.naxis_n[1] = 2;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  acs_fio_writer_end_v1(wr, 0, 0, 0, NULL, err, sizeof(err));
  st = acs_fio_reader_open_v1(path, NULL, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "open");
  /* strict=0: 放行 */
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, NULL, back, 4, 0, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "read lenient");
  /* strict=1: 拒绝 */
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, NULL, back, 4, 1, &got, NULL, err,
                             sizeof(err));
  CHECK_ST(ACS_FIO_ERR_NANINF, st, "read strict naninf");
  acs_fio_reader_close_v1(rd);
  remove(path);
}

/* ── 8. 取消 ── */
static void test_cancel(void) {
  const char* path = tmp_path("cancel.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 decl;
  float data[6];
  int64_t got = 0;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 3;
  decl.naxis_n[1] = 2;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  acs_fio_writer_end_v1(wr, 0, 0, 0, NULL, err, sizeof(err));
  /* 读时取消 */
  st = acs_fio_reader_open_v1(path, &g_hooks, &rd, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "open");
  g_cancel_flag = 1;
  st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0, NULL, data, 6, 0, &got, &g_hooks,
                             err, sizeof(err));
  CHECK_ST(ACS_FIO_ERR_CANCELLED, st, "read cancelled");
  g_cancel_flag = 0;
  acs_fio_reader_close_v1(rd);
  /* 写时取消 */
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 1, &g_hooks, &wr, err,
                               sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin2");
  g_cancel_flag = 1;
  st = acs_fio_write_plane_v1(wr, 0, data, sizeof(data), &g_hooks, err,
                              sizeof(err));
  CHECK_ST(ACS_FIO_ERR_CANCELLED, st, "write cancelled");
  g_cancel_flag = 0;
  acs_fio_writer_abort_v1(wr);
  remove(path);
}

/* ── 9. 磁盘满/写失败: 只读目录 ── */
static void test_write_denied(void) {
  const char* dir = tmp_path("ro_dir");
  char path[640];
  acs_fio_writer_v1* wr = NULL;
  acs_fio_header_v1 decl;
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  snprintf(path, sizeof(path), "%s/out.fits", dir);
#if defined(_WIN32)
  _mkdir(dir);
#else
  mkdir(dir, 0500); /* 只读目录 */
#endif
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 0;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK(st == ACS_FIO_ERR_IO || st == ACS_FIO_ERR_DISKFULL);
#if !defined(_WIN32)
  chmod(dir, 0700);
#endif
  rmdir(dir);
}

/* ── 10. abort 清理临时文件 ── */
static void test_abort_cleanup(void) {
  const char* path = tmp_path("abort.fits");
  acs_fio_writer_v1* wr = NULL;
  acs_fio_header_v1 decl;
  float data[4] = {1, 2, 3, 4};
  char err[ACS_FIO_ERR_TEXT_MAX];
  int st;
  remove(path);
  memset(&decl, 0, sizeof(decl));
  decl.struct_size = (uint32_t)sizeof(decl);
  decl.abi_version = ACS_FIO_ABI_VERSION_V1;
  decl.bitpix = ACS_FIO_BITPIX_F32;
  decl.naxis = 2;
  decl.naxis_n[0] = 2;
  decl.naxis_n[1] = 2;
  st = acs_fio_writer_begin_v1(path, &decl, NULL, 0, NULL, &wr, err, sizeof(err));
  CHECK_ST(ACS_FIO_OK, st, "begin");
  acs_fio_write_plane_v1(wr, 0, data, sizeof(data), NULL, err, sizeof(err));
  acs_fio_writer_abort_v1(wr);
  /* 目标与临时文件均不存在 */
  {
    FILE* t = fopen(path, "rb");
    CHECK(t == NULL);
    if (t) fclose(t);
  }
  remove(path);
}

int main(void) {
  const char* base = getenv("TMPDIR");
  snprintf(g_dir, sizeof(g_dir), "%s/astrocs_io_selftest_%ld", base ? base : "/tmp",
           (long)getpid());
#if defined(_WIN32)
  _mkdir(g_dir);
#else
  mkdir(g_dir, 0700);
#endif
  test_roundtrip_f32();
  test_roundtrip_f64_2plane();
  test_bad_header_missing_simple();
  test_bad_header_no_end();
  test_bad_header_bad_bitpix();
  test_truncated_data();
  test_mismatch();
  test_checksum_error();
  test_naninf();
  test_cancel();
  test_write_denied();
  test_abort_cleanup();
#if !defined(_WIN32)
  rmdir(g_dir);
#endif
  if (g_failures) {
    fprintf(stderr, "SELFTEST FAIL: %d failures\n", g_failures);
    return 1;
  }
  printf("fits_core_selftest: ALL PASS\n");
  return 0;
}
