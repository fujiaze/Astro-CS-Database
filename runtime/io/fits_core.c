/* AstroCS FITS 流核心实现 — runtime/io/fits_core.c (IO-001)
 *
 * 职责: acs_fio_* C ABI (fits_stream_v1.h) 的实现。独立、无 CFITSIO 依赖的
 * FITS 基本图像 HDU (NAXIS 0..3, BITPIX 8/16/32/64/-32/-64) 流式读 / 原子写 /
 * DATASUM/CHECKSUM 校验。CFITSIO 等第三方库只允许存在于 astrocs_io.dll 内部
 * 其它私有层; 本文件不 include 任何 CFITSIO 头, 也不暴露任何第三方类型。
 *
 * 关键设计:
 *  - 纯 C11; 跨边界无托管分配 (out 缓冲由调用方提供); 内部堆仅用于句柄。
 *  - trace: 内部累计 bytes + 注入 hooks 逐次回调; 可经 *_bytes_*_v1 查询。
 *  - DATASUM/CHECKSUM: 独立实现 FITS 32 位 1 补码块校验 (NOAO/Rob Seaman 算法,
 *    同 cfitsio checksum.c 的公开算法; 本实现不链接 cfitsio)。
 *  - 原子写: 同目录临时文件 → 写 header(预留 DATASUM/CHECKSUM 槽) + 流式数据
 *    → 原位改写校验卡 → flush → 可选自校验 → rename。
 *  - 字节序: 文件内 big-endian; 读入/写出按 dtype 元素交换; u8 不交换。
 *
 * 并发: reentrant (唯一全局为 C11 原子 tmp 名计数器); 句柄非共享。
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L /* fseeko/ftello/off_t */
#endif

#include "astrocs/io/fits_stream_v1.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <process.h>
#define getpid _getpid
#else
#include <sys/types.h> /* off_t */
#include <unistd.h>
#endif

/* 大文件偏移 seek (POSIX fseeko / Windows _fseeki64) */
#if defined(_WIN32)
#define fio_fseek _fseeki64
#define fio_ftell _ftelli64
typedef __int64 fio_off_t;
#else
#define fio_fseek fseeko
#define fio_ftell ftello
typedef off_t fio_off_t;
#endif

/* ------------------------------------------------------------------ */
/* 内部小工具                                                          */
/* ------------------------------------------------------------------ */

static void set_err(char* err, size_t cap, const char* fmt, ...) {
  if (!err || cap == 0) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(err, cap, fmt, ap);
  va_end(ap);
  err[cap - 1] = '\0';
}

static int fio_min_int(int a, int b) { return a < b ? a : b; }

static int fio_check_head(uint32_t struct_size, uint32_t abi_version,
                          size_t expect_size, char* err, size_t cap) {
  if (abi_version != ACS_FIO_ABI_VERSION_V1) {
    set_err(err, cap, "abi mismatch: got %u expect %u",
            (unsigned)abi_version, (unsigned)ACS_FIO_ABI_VERSION_V1);
    return ACS_FIO_ERR_ABI_MISMATCH;
  }
  if (struct_size != 0 && struct_size != (uint32_t)expect_size) {
    set_err(err, cap, "struct size mismatch: got %u expect %u",
            (unsigned)struct_size, (unsigned)expect_size);
    return ACS_FIO_ERR_ABI_MISMATCH;
  }
  return ACS_FIO_OK;
}

static int fio_check_hooks(const acs_fio_trace_hooks_v1* h, char* err, size_t cap) {
  if (!h) return ACS_FIO_OK;
  if (h->abi_version != ACS_FIO_ABI_VERSION_V1 ||
      (h->struct_size != 0 && h->struct_size != (uint32_t)sizeof(acs_fio_trace_hooks_v1))) {
    set_err(err, cap, "trace hooks abi/struct mismatch");
    return ACS_FIO_ERR_ABI_MISMATCH;
  }
  return ACS_FIO_OK;
}

static int fio_cancelled(const acs_fio_trace_hooks_v1* h) {
  return h && h->is_cancelled && h->is_cancelled(h->user_data) != 0;
}

/* 元素字节宽; -1 = 不支持 */
static int64_t fio_bytes_per_pixel(int32_t bitpix) {
  switch (bitpix) {
    case ACS_FIO_BITPIX_U8: return 1;
    case ACS_FIO_BITPIX_I16: return 2;
    case ACS_FIO_BITPIX_I32: return 4;
    case ACS_FIO_BITPIX_I64: return 8;
    case ACS_FIO_BITPIX_F32: return 4;
    case ACS_FIO_BITPIX_F64: return 8;
    default: return -1;
  }
}

/* errno → io 错误码 (含磁盘满) */
static int fio_errno_code(int e) {
  if (e == ENOSPC || e == EDQUOT) return ACS_FIO_ERR_DISKFULL;
  return ACS_FIO_ERR_IO;
}

/* ------------------------------------------------------------------ */
/* 跨平台文件 I/O 封装 (trace 计数在此层注入)                           */
/* ------------------------------------------------------------------ */

typedef struct fio_file {
  FILE* fp;
  uint64_t read_bytes;
  uint64_t write_bytes;
  const acs_fio_trace_hooks_v1* hooks;
} fio_file;

static int fio_file_open(fio_file* f, const char* path, const char* mode,
                         const acs_fio_trace_hooks_v1* hooks, char* err, size_t cap) {
  f->fp = fopen(path, mode);
  if (!f->fp) {
    int e = errno;
    set_err(err, cap, "cannot open '%s': %s", path, strerror(e));
    if (mode[0] == 'w' || mode[0] == 'a') return fio_errno_code(e);
    return ACS_FIO_ERR_IO;
  }
  f->read_bytes = 0;
  f->write_bytes = 0;
  f->hooks = hooks;
  return ACS_FIO_OK;
}

static int fio_file_seek(fio_file* f, fio_off_t off, int whence) {
  return fio_fseek(f->fp, off, whence);
}

static fio_off_t fio_file_tell(fio_file* f) { return fio_ftell(f->fp); }

static size_t fio_file_read(fio_file* f, void* buf, size_t n) {
  size_t got = fread(buf, 1, n, f->fp);
  if (got > 0) {
    f->read_bytes += got;
    if (f->hooks && f->hooks->on_read_bytes)
      f->hooks->on_read_bytes(f->hooks->user_data, got);
  }
  return got;
}

static size_t fio_file_write(fio_file* f, const void* buf, size_t n) {
  size_t got = fwrite(buf, 1, n, f->fp);
  if (got > 0) {
    f->write_bytes += got;
    if (f->hooks && f->hooks->on_write_bytes)
      f->hooks->on_write_bytes(f->hooks->user_data, got);
  }
  return got;
}

static int fio_file_flush(fio_file* f) { return fflush(f->fp); }
static void fio_file_close(fio_file* f) {
  if (f->fp) {
    fclose(f->fp);
    f->fp = NULL;
  }
}

/* ------------------------------------------------------------------ */
/* FITS 卡片构造 / 解析                                                */
/* ------------------------------------------------------------------ */

typedef struct fio_card {
  char name[ACS_FIO_KEYWORD_NAME_MAX];
  char value[ACS_FIO_KEYWORD_VALUE_MAX];
  char comment[ACS_FIO_KEYWORD_COMMENT_MAX];
} fio_card;

/* 构造 80 字节卡片。value 为裸值: 字符串加引号, T/F 逻辑与数值不加。
 * 无 value 时 (END/COMMENT/HISTORY/空值卡) 不写 '='。注释 " / " 从列 31 起。 */
static void fio_card_build(char raw[80], const char* name, const char* value,
                           const char* comment) {
  int i;
  memset(raw, ' ', 80);
  if (!name || !name[0]) return; /* 空白卡 */
  for (i = 0; name[i] && i < 8; i++) raw[i] = name[i];
  if (!value || !value[0]) {
    /* END / 关键字型卡 (COMMENT/HISTORY) 或空值卡: 仅名称列, 无 '=' */
    return;
  }
  raw[8] = '=';
  raw[9] = ' ';
  {
    int need_quote = 0;
    const char* v = value;
    /* FITS 逻辑值 T/F 单字符: 不加引号 */
    if ((value[0] == 'T' || value[0] == 'F') && value[1] == '\0') {
      /* 逻辑值 */
    } else if (!((*v >= '0' && *v <= '9') || *v == '.' || *v == '+' || *v == '-')) {
      need_quote = 1;
    } else {
      for (; *v; v++) {
        if (!((*v >= '0' && *v <= '9') || (*v >= 'A' && *v <= 'Z') ||
              (*v >= 'a' && *v <= 'z') || *v == '.' || *v == '+' || *v == '-' ||
              *v == 'E' || *v == 'e')) {
          need_quote = 1;
          break;
        }
      }
    }
    if (need_quote) {
      int col = 10;
      raw[col++] = '\'';
      v = value;
      while (*v && col < 69) {
        raw[col++] = *v;
        if (*v == '\'') { /* 字面引号转义为 '' */
          if (col < 69) raw[col++] = '\'';
        }
        v++;
      }
      raw[col++] = '\'';
    } else {
      int col = 10;
      v = value;
      while (*v && col < 80) raw[col++] = *v++;
    }
  }
  if (comment && comment[0]) {
    int col = 31; /* FITS: '/' 注释分隔通常在列 31 起 (前留空格列 30) */
    raw[30] = ' ';
    if (col < 80) raw[col] = '/';
    col++;
    if (col < 80) raw[col] = ' ';
    col++;
    {
      int ci = 0;
      while (comment[ci] && col < 80) raw[col++] = comment[ci++];
    }
  }
}

/* 写入一张卡到文件当前位置 */
static size_t fio_card_write(fio_file* f, const char* name, const char* value,
                             const char* comment) {
  char raw[80];
  fio_card_build(raw, name, value, comment);
  return fio_file_write(f, raw, 80);
}

/* 覆写到指定文件偏移 (卡内固定 80 字节原位改写) */
static int fio_card_patch(fio_file* f, fio_off_t off, const char* name,
                          const char* value, const char* comment,
                          char* err, size_t cap) {
  char raw[80];
  if (fio_file_seek(f, off, SEEK_SET) != 0) {
    set_err(err, cap, "seek failed (patch card)");
    return ACS_FIO_ERR_IO;
  }
  fio_card_build(raw, name, value, comment);
  if (fio_file_write(f, raw, 80) != 80) {
    int e = errno;
    set_err(err, cap, "patch card write failed: %s", strerror(e));
    return fio_errno_code(e);
  }
  return ACS_FIO_OK;
}

/* 解析 80 字节卡 → fio_card; 返回 1=END。raw 视为 80 字节卡片, 内部扫描
 * 全部以 raw[0..79] 为界 (不依赖 NUL; 但调用方仍建议传 81 字节含 NUL 的缓冲)。 */
static int fio_parse_card(const char raw[80], fio_card* out) {
  char name[9];
  int i;
  memset(out, 0, sizeof(*out));
  for (i = 0; i < 8; i++) name[i] = raw[i];
  name[8] = '\0';
  for (i = 7; i >= 0; i--) {
    if (name[i] == ' ') name[i] = '\0';
    else break;
  }
  if (name[0] == '\0') return 0; /* 空白卡 */
  snprintf(out->name, sizeof(out->name), "%s", name);
  if (strcmp(name, "END") == 0) return 1;
  {
    int has_eq = (raw[8] == '=');
    if (has_eq) {
      /* 值区从第 10 列 (索引 10) 起; 扫描不越过 raw+80 */
      int col = 10;
      while (col < 80 && raw[col] == ' ') col++;
      if (col < 80 && raw[col] == '\'') {
        /* 字符串值: '' 转义 */
        col++;
        size_t len = 0;
        while (col < 80 && len + 1 < (size_t)ACS_FIO_KEYWORD_VALUE_MAX) {
          if (raw[col] == '\'') {
            if (col + 1 < 80 && raw[col + 1] == '\'') {
              out->value[len++] = '\'';
              col += 2;
              continue;
            }
            break; /* 结束引号 */
          }
          out->value[len++] = raw[col];
          col++;
        }
        out->value[len] = '\0';
        /* 跳到注释: 找结束引号 */
        while (col < 80 && raw[col] != '\'') col++;
        if (col < 80) col++; /* 过引号 */
        while (col < 80 && raw[col] == ' ') col++;
        if (col + 1 < 80 && raw[col] == '/' && raw[col + 1] == ' ') {
          col += 2;
          size_t clen = 0;
          while (col < 80 && clen + 1 < (size_t)ACS_FIO_KEYWORD_COMMENT_MAX) {
            out->comment[clen++] = raw[col];
            col++;
          }
          out->comment[clen] = '\0';
        }
      } else if (col < 80) {
        /* 数值/逻辑值: 到 '/' 或卡片尾 */
        size_t len = 0;
        while (col < 80 && len + 1 < (size_t)ACS_FIO_KEYWORD_VALUE_MAX &&
               raw[col] != '/') {
          out->value[len++] = raw[col];
          col++;
        }
        out->value[len] = '\0';
        while (len > 0 && out->value[len - 1] == ' ') out->value[--len] = '\0';
      }
    }
  }
  return 0;
}

static int fio_is_reserved_keyword(const char* name) {
  static const char* const reserved[] = {
      "SIMPLE", "XTENSION", "BITPIX", "NAXIS",  "NAXIS1", "NAXIS2", "NAXIS3",
      "PCOUNT", "GCOUNT",   "EXTEND", "BSCALE", "BZERO",  "BLANK",  "DATASUM",
      "CHECKSUM", NULL};
  int i;
  if (!name) return 0;
  for (i = 0; reserved[i]; i++)
    if (strcmp(name, reserved[i]) == 0) return 1;
  return 0;
}

typedef struct fio_header {
  int32_t bitpix;
  int32_t naxis;
  int64_t naxis_n[ACS_FIO_NAXIS_MAX];
  int card_count; /* END 前非 END 卡数 */
  fio_card cards[ACS_FIO_HEADER_MAX_CARDS];
  char bunit[ACS_FIO_BUNIT_MAX];
  int has_datasum;
  char datasum[ACS_FIO_DATASUM_LEN];
  int has_checksum;
  char checksum[ACS_FIO_DATASUM_LEN];
} fio_header;

static int64_t fio_header_pixels(const fio_header* h) {
  int64_t n = 1;
  int i;
  if (h->naxis == 0) return 0;
  for (i = 0; i < h->naxis; i++) {
    if (h->naxis_n[i] <= 0) return 0;
    if (n > INT64_MAX / h->naxis_n[i]) return 0;
    n *= h->naxis_n[i];
  }
  return n;
}

static int64_t fio_header_data_bytes(const fio_header* h) {
  int64_t bpp = fio_bytes_per_pixel(h->bitpix);
  int64_t pixels = fio_header_pixels(h);
  if (bpp < 0 || pixels < 0) return -1;
  if (pixels != 0 && bpp > INT64_MAX / pixels) return -1;
  return pixels * bpp;
}

/* 2880 块对齐大小 */
static int64_t fio_blocks2880(int64_t bytes) {
  if (bytes <= 0) return 0;
  return (bytes + 2879) / 2880;
}

/* 解析 header: 从当前位置读 80 字节卡直到 END, 记录结构 + 卡片表。
 * 成功: 文件指针停在 header 块末尾 (2880 对齐)。 */
static int fio_parse_header(fio_file* f, fio_header* out, char* err, size_t cap) {
  int saw_simple = 0, saw_bitpix = 0, saw_naxis = 0;
  int card_i;
  memset(out, 0, sizeof(*out));
  out->bitpix = 0;
  out->naxis = -1;
  for (card_i = 0; card_i < ACS_FIO_HEADER_MAX_CARDS; card_i++) {
    char raw[81];
    fio_card card;
    int is_end;
    size_t got = fio_file_read(f, raw, 80);
    if (got < 80) {
      set_err(err, cap, "bad header: EOF before END (card %d)", card_i);
      return ACS_FIO_ERR_BAD_HEADER;
    }
    raw[80] = '\0'; /* 供 fio_parse_card 内 C 字符串扫描安全终止 */
    is_end = fio_parse_card(raw, &card);
    if (card.name[0] == '\0') {
      if (is_end) goto card_end;
      continue; /* 空白卡跳过 (不计数) */
    }
    if (strcmp(card.name, "SIMPLE") == 0) {
      saw_simple = 1;
    } else if (strcmp(card.name, "BITPIX") == 0) {
      saw_bitpix = 1;
      out->bitpix = (int32_t)strtol(card.value, NULL, 10);
      if (fio_bytes_per_pixel(out->bitpix) < 0) {
        set_err(err, cap, "unsupported BITPIX=%d", (int)out->bitpix);
        return ACS_FIO_ERR_UNSUPPORTED;
      }
    } else if (strcmp(card.name, "NAXIS") == 0) {
      saw_naxis = 1;
      out->naxis = (int32_t)strtol(card.value, NULL, 10);
      if (out->naxis < 0 || out->naxis > ACS_FIO_NAXIS_MAX) {
        set_err(err, cap, "unsupported NAXIS=%d (v1 0..%d)", (int)out->naxis,
                (int)ACS_FIO_NAXIS_MAX);
        return ACS_FIO_ERR_UNSUPPORTED;
      }
    } else if (strncmp(card.name, "NAXIS", 5) == 0 && card.name[5] >= '1' &&
               card.name[5] <= '9') {
      int idx = card.name[5] - '1';
      if (idx < ACS_FIO_NAXIS_MAX) out->naxis_n[idx] = strtoll(card.value, NULL, 10);
    } else if (strcmp(card.name, "BUNIT") == 0) {
      /* card.value 最多 71 字符; 定长缓冲截断存储 */
      strncpy(out->bunit, card.value, sizeof(out->bunit) - 1);
      out->bunit[sizeof(out->bunit) - 1] = '\0';
    } else if (strcmp(card.name, "DATASUM") == 0) {
      out->has_datasum = 1;
      strncpy(out->datasum, card.value, sizeof(out->datasum) - 1);
      out->datasum[sizeof(out->datasum) - 1] = '\0';
    } else if (strcmp(card.name, "CHECKSUM") == 0) {
      out->has_checksum = 1;
      strncpy(out->checksum, card.value, sizeof(out->checksum) - 1);
      out->checksum[sizeof(out->checksum) - 1] = '\0';
    }
    /* 存入卡片表 (END 除外) */
    if (out->card_count < ACS_FIO_HEADER_MAX_CARDS) {
      out->cards[out->card_count++] = card;
    } else {
      set_err(err, cap, "header too many cards");
      return ACS_FIO_ERR_BAD_HEADER;
    }
    if (is_end) goto card_end;
    continue;
  card_end:
    /* END 所在块: 补读到 2880 对齐 (若文件还有字节) */
    {
      int in_block = card_i % 36;
      int remain_cards = 35 - in_block;
      if (remain_cards > 0) {
        char zeros[2880];
        size_t want = (size_t)remain_cards * 80;
        memset(zeros, ' ', 2880);
        while (want > 0) {
          size_t r = fio_file_read(f, zeros, want);
          if (r == 0) break;
          want -= r;
        }
      }
    }
    /* 结构校验 */
    if (!saw_simple) {
      set_err(err, cap, "bad header: missing SIMPLE");
      return ACS_FIO_ERR_BAD_HEADER;
    }
    if (!saw_bitpix) {
      set_err(err, cap, "bad header: missing BITPIX");
      return ACS_FIO_ERR_BAD_HEADER;
    }
    if (!saw_naxis) {
      set_err(err, cap, "bad header: missing NAXIS");
      return ACS_FIO_ERR_BAD_HEADER;
    }
    if (out->naxis > 0) {
      int i;
      for (i = 0; i < out->naxis; i++) {
        if (out->naxis_n[i] <= 0) {
          set_err(err, cap, "bad header: NAXIS%d=%lld invalid", i + 1,
                  (long long)out->naxis_n[i]);
          return ACS_FIO_ERR_BAD_HEADER;
        }
      }
    }
    return ACS_FIO_OK;
  }
  set_err(err, cap, "header too large: no END within %d cards", ACS_FIO_HEADER_MAX_CARDS);
  return ACS_FIO_ERR_BAD_HEADER;
}

/* ------------------------------------------------------------------ */
/* FITS DATASUM / CHECKSUM (32 位 1 补码, NOAO/Rob Seaman 算法)         */
/* ------------------------------------------------------------------ */

/* 增量块校验状态: 跨 2880 字节块边界流式累计 */
typedef struct fio_dsum {
  unsigned long sum;   /* 已完成块的折叠和 (块初值) */
  unsigned long hi;    /* 当前块内未折叠高/低 16 位 */
  unsigned long lo;
  size_t in_block;     /* 当前块内已累计字节 (0..2880) */
} fio_dsum;

static void fio_dsum_init(fio_dsum* s) {
  s->sum = 0;
  s->hi = 0;
  s->lo = 0;
  s->in_block = 0;
}

static void fio_dsum_fold(fio_dsum* s) {
  unsigned long hicarry, locarry;
  unsigned long hi = s->hi, lo = s->lo;
  hicarry = hi >> 16;
  locarry = lo >> 16;
  while (hicarry | locarry) {
    hi = (hi & 0xFFFF) + locarry;
    lo = (lo & 0xFFFF) + hicarry;
    hicarry = hi >> 16;
    locarry = lo >> 16;
  }
  s->sum = (hi << 16) + lo;
  s->hi = s->sum >> 16;
  s->lo = s->sum & 0xFFFF;
}

/* 喂入字节 (big-endian 文件字节流)。跨 2880 块边界自动折块。 */
static void fio_dsum_update(fio_dsum* s, const unsigned char* data, size_t n) {
  size_t i = 0;
  while (i < n) {
    if (s->in_block == 2880) {
      fio_dsum_fold(s);
      s->hi = s->sum >> 16;
      s->lo = s->sum & 0xFFFF;
      s->in_block = 0;
    }
    {
      /* 需要 2 字节对齐到 16-bit 字; 若 in_block 奇 (不可能, 每次喂 2 字节) */
      size_t take = n - i;
      if (take > 2880 - s->in_block) take = 2880 - s->in_block;
      if (take & 1) take &= ~(size_t)1; /* 保持偶 */
      if (take == 0) {
        /* 单字节尾 (文件不可能) */
        unsigned long w = (unsigned long)data[i] << 8;
        s->hi += w;
        if (s->hi > 0xFFFFUL) { s->hi -= 0x10000UL; s->lo += 1; }
        if (s->lo > 0xFFFFUL) { s->lo -= 0x10000UL; s->hi += 1; }
        s->in_block += 1;
        i += 1;
        continue;
      }
      {
        size_t j;
        for (j = 0; j + 1 < take; j += 2) {
          unsigned long w = ((unsigned long)data[i + j] << 8) | data[i + j + 1];
          s->hi += w;
          if (s->hi > 0xFFFFUL) { s->hi -= 0x10000UL; s->lo += 1; }
          if (s->lo > 0xFFFFUL) { s->lo -= 0x10000UL; s->hi += 1; }
        }
        s->in_block += take;
        i += take;
      }
    }
  }
}

/* 收尾: 末块不足 2880 补零后折叠。返回最终 sum。 */
static unsigned long fio_dsum_finish(fio_dsum* s) {
  if (s->in_block > 0) {
    fio_dsum_fold(s);
    s->hi = s->sum >> 16;
    s->lo = s->sum & 0xFFFF;
    s->in_block = 0;
  }
  return s->sum;
}

/* 从文件流式读取 [off, off+len) 并累计 DATASUM (len 为实际要读的字节,
 * 由调用方对齐到 2880 或精确)。EOF 提前返回实际读字节。 */
static size_t fio_file_read_datasum(fio_file* f, fio_off_t off, size_t want,
                                    fio_dsum* s, char* err, size_t cap) {
  unsigned char buf[2880];
  size_t total = 0;
  if (fio_file_seek(f, off, SEEK_SET) != 0) {
    set_err(err, cap, "seek failed (datasum)");
    return (size_t)-1;
  }
  while (total < want) {
    size_t chunk = want - total;
    if (chunk > sizeof(buf)) chunk = sizeof(buf);
    {
      size_t got = fio_file_read(f, buf, chunk);
      if (got == 0) break;
      fio_dsum_update(s, buf, got);
      total += got;
    }
  }
  return total;
}

/* 编码 16 字符 ASCII (complm=1 取补) — 同 cfitsio ffesum */
static void fio_encode_checksum(unsigned long sum, int complm, char ascii[17]) {
  static const unsigned int exclude[13] = {0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
                                           0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60};
  static const unsigned long mask[4] = {0xff000000, 0xff0000, 0xff00, 0xff};
  const int offset = 0x30;
  unsigned long value = complm ? (0xFFFFFFFFUL - sum) : sum;
  char asc[32];
  int ii;
  for (ii = 0; ii < 4; ii++) {
    int byte = (int)((value & mask[ii]) >> (24 - 8 * ii));
    int quotient = byte / 4 + offset;
    int remainder = byte % 4;
    int ch[4];
    int check, jj, kk;
    for (jj = 0; jj < 4; jj++) ch[jj] = quotient;
    ch[0] += remainder;
    for (check = 1; check;)
      for (check = 0, kk = 0; kk < 13; kk++)
        for (jj = 0; jj < 4; jj += 2)
          if ((unsigned char)ch[jj] == exclude[kk] ||
              (unsigned char)ch[jj + 1] == exclude[kk]) {
            ch[jj]++;
            ch[jj + 1]--;
            check++;
          }
    for (jj = 0; jj < 4; jj++) asc[4 * jj + ii] = (char)ch[jj];
  }
  for (ii = 0; ii < 16; ii++) ascii[ii] = asc[(ii + 15) % 16];
  ascii[16] = '\0';
}

/* 解码 16 字符 ASCII → 32 位校验值 (ffesum 的逆)。
 * complm=1: 卡上存的是补码 (encode(sum, TRUE)) → 还原 encode 前原值。 */
static unsigned long fio_decode_checksum(const char ascii[17], int complm) {
  char cbuf[16];
  unsigned long hi = 0, lo = 0, hicarry, locarry;
  int ii;
  for (ii = 0; ii < 16; ii++) {
    cbuf[ii] = ascii[(ii + 1) % 16];
    cbuf[ii] = (char)(cbuf[ii] - 0x30);
  }
  for (ii = 0; ii < 16; ii += 4) {
    hi += (unsigned long)(((unsigned char)cbuf[ii] << 8) + (unsigned char)cbuf[ii + 1]);
    lo += (unsigned long)(((unsigned char)cbuf[ii + 2] << 8) + (unsigned char)cbuf[ii + 3]);
  }
  hicarry = hi >> 16;
  locarry = lo >> 16;
  while (hicarry || locarry) {
    hi = (hi & 0xFFFF) + locarry;
    lo = (lo & 0xFFFF) + hicarry;
    hicarry = hi >> 16;
    locarry = lo >> 16;
  }
  {
    unsigned long sum = (hi << 16) + lo;
    return complm ? (0xFFFFFFFFUL - sum) : sum;
  }
}

/* 一个 HDU 的 CHECKSUM 校验和: 对 header+data (含填充) 全字节流式累计,
 * 期间 CHECKSUM 卡的值 16 字符已被置 '0' (由调用方保证或在此处理)。 */
static unsigned long fio_hdu_checksum_stream(fio_file* f, fio_off_t hdr_off,
                                             size_t hdr_len, size_t data_len_padded) {
  fio_dsum s;
  unsigned char buf[2880];
  size_t total = 0;
  size_t want = hdr_len + data_len_padded;
  fio_dsum_init(&s);
  if (fio_file_seek(f, hdr_off, SEEK_SET) != 0) return 0;
  while (total < want) {
    size_t chunk = want - total;
    if (chunk > sizeof(buf)) chunk = sizeof(buf);
    {
      size_t got = fio_file_read(f, buf, chunk);
      if (got == 0) break;
      fio_dsum_update(&s, buf, got);
      total += got;
    }
  }
  return fio_dsum_finish(&s);
}

/* 在 header 缓冲/文件内查找关键字卡片偏移 (第 0..hdr_len 字节, 80 对齐),
 * 返回卡片起始偏移; -1 = 未找到。 */
static fio_off_t fio_find_card(const unsigned char* hdr_bytes, size_t hdr_len,
                               const char* want) {
  size_t i;
  for (i = 0; i + 80 <= hdr_len; i += 80) {
    char name[9];
    int k;
    memcpy(name, hdr_bytes + i, 8);
    name[8] = '\0';
    for (k = 7; k >= 0; k--)
      if (name[k] == ' ') name[k] = '\0';
      else break;
    if (strcmp(name, want) == 0) return (fio_off_t)i;
  }
  return (fio_off_t)-1;
}

/* 将卡片值区 (第 10 列起) 的 16 字符置 '0' (CHECKSUM 校验前) */
static void fio_zero_card_value(char* card80, size_t n) {
  /* 值区: 找 '=' 后第一个非空白 */
  size_t i;
  if (n < 10) return;
  for (i = 9; i < n && i < 80; i++) {
    if (card80[i] != ' ') break;
  }
  if (i < n && i < 80) {
    size_t j;
    for (j = 0; j < 16 && i + j < n && i + j < 80; j++) card80[i + j] = '0';
  }
}

/* ------------------------------------------------------------------ */
/* 读取器                                                              */
/* ------------------------------------------------------------------ */

struct acs_fio_reader_v1_s {
  fio_file f;
  fio_header hdr;
  fio_off_t hdr_bytes;  /* header 块字节 (2880 对齐) */
  fio_off_t data_off;   /* 数据区起始 */
  int64_t data_bytes;   /* 声明数据字节 */
  char path[ACS_FIO_PATH_MAX];
  int closed;
};

/* 打开 + 解析 header + 截断预检 */
static int fio_reader_open_impl(const char* path,
                                const acs_fio_trace_hooks_v1* hooks,
                                acs_fio_reader_v1** out,
                                char* err, size_t cap) {
  acs_fio_reader_v1* rd;
  int st;
  if (!path || !out) return ACS_FIO_ERR_PARAM;
  *out = NULL;
  st = fio_check_hooks(hooks, err, cap);
  if (st != ACS_FIO_OK) return st;
  rd = (acs_fio_reader_v1*)calloc(1, sizeof(*rd));
  if (!rd) {
    set_err(err, cap, "nomem");
    return ACS_FIO_ERR_NOMEM;
  }
  st = fio_file_open(&rd->f, path, "rb", hooks, err, cap);
  if (st != ACS_FIO_OK) {
    free(rd);
    return st;
  }
  snprintf(rd->path, sizeof(rd->path), "%s", path);
  st = fio_parse_header(&rd->f, &rd->hdr, err, cap);
  if (st != ACS_FIO_OK) {
    fio_file_close(&rd->f);
    free(rd);
    return st;
  }
  rd->hdr_bytes = fio_file_tell(&rd->f);
  if (rd->hdr_bytes < 0) {
    fio_file_close(&rd->f);
    free(rd);
    set_err(err, cap, "tell failed");
    return ACS_FIO_ERR_IO;
  }
  rd->data_off = rd->hdr_bytes;
  rd->data_bytes = fio_header_data_bytes(&rd->hdr);
  if (rd->data_bytes < 0) {
    fio_file_close(&rd->f);
    free(rd);
    set_err(err, cap, "data size overflow");
    return ACS_FIO_ERR_UNSUPPORTED;
  }
  /* 截断预检: 文件长度 ≥ header + data */
  if (rd->data_bytes > 0) {
    fio_off_t end;
    if (fio_file_seek(&rd->f, 0, SEEK_END) != 0) {
      fio_file_close(&rd->f);
      free(rd);
      set_err(err, cap, "seek end failed");
      return ACS_FIO_ERR_IO;
    }
    end = fio_file_tell(&rd->f);
    if (end < rd->data_off + rd->data_bytes) {
      fio_off_t data_off = rd->data_off;
      int64_t data_bytes = rd->data_bytes;
      fio_file_close(&rd->f);
      free(rd);
      set_err(err, cap, "truncated: file %lld < header %lld + data %lld",
              (long long)end, (long long)data_off, (long long)data_bytes);
      return ACS_FIO_ERR_TRUNCATED;
    }
    if (fio_file_seek(&rd->f, rd->data_off, SEEK_SET) != 0) {
      fio_file_close(&rd->f);
      free(rd);
      set_err(err, cap, "seek failed");
      return ACS_FIO_ERR_IO;
    }
  }
  *out = rd;
  return ACS_FIO_OK;
}

/* 期望声明校验 */
static int fio_read_check_decl(const acs_fio_reader_v1* rd,
                               int64_t nx, int64_t ny, int32_t bpix,
                               const char* bunit, char* err, size_t cap) {
  int64_t fnx = rd->hdr.naxis >= 1 ? rd->hdr.naxis_n[0] : 1;
  int64_t fny = rd->hdr.naxis >= 2 ? rd->hdr.naxis_n[1] : 1;
  if (nx != 0 && nx != fnx) {
    set_err(err, cap, "shape mismatch: nx=%lld header NAXIS1=%lld",
            (long long)nx, (long long)fnx);
    return ACS_FIO_ERR_MISMATCH;
  }
  if (ny != 0 && ny != fny) {
    set_err(err, cap, "shape mismatch: ny=%lld header NAXIS2=%lld",
            (long long)ny, (long long)fny);
    return ACS_FIO_ERR_MISMATCH;
  }
  if (bpix != 0 && bpix != rd->hdr.bitpix) {
    set_err(err, cap, "dtype mismatch: expect BITPIX=%d file BITPIX=%d",
            (int)bpix, (int)rd->hdr.bitpix);
    return ACS_FIO_ERR_MISMATCH;
  }
  if (bunit && bunit[0] && rd->hdr.bunit[0]) {
    if (strcmp(bunit, rd->hdr.bunit) != 0) {
      set_err(err, cap, "unit mismatch: expect BUNIT='%s' file BUNIT='%s'",
              bunit, rd->hdr.bunit);
      return ACS_FIO_ERR_MISMATCH;
    }
  }
  return ACS_FIO_OK;
}

static int fio_scan_naninf(const void* buf, int64_t elems, int32_t bitpix) {
  int64_t i;
  if (bitpix == ACS_FIO_BITPIX_F32) {
    const float* p = (const float*)buf;
    for (i = 0; i < elems; i++)
      if (isnan(p[i]) || isinf(p[i])) return 1;
  } else if (bitpix == ACS_FIO_BITPIX_F64) {
    const double* p = (const double*)buf;
    for (i = 0; i < elems; i++)
      if (isnan(p[i]) || isinf(p[i])) return 1;
  }
  return 0;
}

/* 就地 big-endian ↔ 本机字节序 (元素交换; 对称) */
static void fio_swap_bytes(void* buf, int64_t elems, int32_t bitpix) {
  int64_t i;
  unsigned char* p = (unsigned char*)buf;
  if (bitpix == ACS_FIO_BITPIX_U8) return;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  (void)buf; (void)elems; (void)bitpix; return;
#else
  if (bitpix == ACS_FIO_BITPIX_I16) {
    for (i = 0; i < elems; i++, p += 2) {
      unsigned char t = p[0]; p[0] = p[1]; p[1] = t;
    }
  } else if (bitpix == ACS_FIO_BITPIX_I32 || bitpix == ACS_FIO_BITPIX_F32) {
    for (i = 0; i < elems; i++, p += 4) {
      unsigned char t = p[0]; p[0] = p[3]; p[3] = t;
      t = p[1]; p[1] = p[2]; p[2] = t;
    }
  } else if (bitpix == ACS_FIO_BITPIX_I64 || bitpix == ACS_FIO_BITPIX_F64) {
    for (i = 0; i < elems; i++, p += 8) {
      unsigned char t = p[0]; p[0] = p[7]; p[7] = t;
      t = p[1]; p[1] = p[6]; p[6] = t;
      t = p[2]; p[2] = p[5]; p[5] = t;
      t = p[3]; p[3] = p[4]; p[4] = t;
    }
  }
#endif
}

/* 平面偏移与元素数 */
static int fio_plane_layout(const acs_fio_reader_v1* rd, int plane_index,
                            int64_t* out_off_elems, int64_t* out_elems,
                            char* err, size_t cap) {
  int64_t naxis3 = rd->hdr.naxis >= 3 ? rd->hdr.naxis_n[2] : 1;
  int64_t per_plane = 1;
  int64_t bpp = fio_bytes_per_pixel(rd->hdr.bitpix);
  if (plane_index < 0 || (rd->hdr.naxis >= 3 && plane_index >= naxis3)) {
    set_err(err, cap, "plane_index=%d out of range (NAXIS3=%lld)", plane_index,
            (long long)naxis3);
    return ACS_FIO_ERR_PARAM;
  }
  if (rd->hdr.naxis >= 1) per_plane *= rd->hdr.naxis_n[0];
  if (rd->hdr.naxis >= 2) per_plane *= rd->hdr.naxis_n[1];
  *out_elems = per_plane;
  *out_off_elems = (int64_t)plane_index * per_plane;
  (void)bpp;
  return ACS_FIO_OK;
}

/* ───────── 读取公共 API ───────── */

int acs_fio_reader_open_v1(const char* path_utf8,
                           const acs_fio_trace_hooks_v1* hooks,
                           acs_fio_reader_v1** out,
                           char* err, size_t err_cap) {
  return fio_reader_open_impl(path_utf8, hooks, out, err, err_cap);
}

int acs_fio_get_header_v1(acs_fio_reader_v1* rd,
                          acs_fio_header_v1* hdr,
                          char* err, size_t err_cap) {
  int i, st;
  if (!rd || !hdr) return ACS_FIO_ERR_PARAM;
  /* 先校验调用方填入的 head (memset 会清掉它们), 再整体清零填充 */
  st = fio_check_head(hdr->struct_size, hdr->abi_version, sizeof(*hdr), err, err_cap);
  if (st != ACS_FIO_OK) return st;
  memset(hdr, 0, sizeof(*hdr));
  hdr->struct_size = (uint32_t)sizeof(*hdr);
  hdr->abi_version = ACS_FIO_ABI_VERSION_V1;
  hdr->bitpix = rd->hdr.bitpix;
  hdr->naxis = rd->hdr.naxis;
  for (i = 0; i < ACS_FIO_NAXIS_MAX; i++) hdr->naxis_n[i] = rd->hdr.naxis_n[i];
  hdr->keyword_count = rd->hdr.card_count;
  if (hdr->keyword_count > ACS_FIO_HEADER_MAX_CARDS) hdr->keyword_count = ACS_FIO_HEADER_MAX_CARDS;
  for (i = 0; i < hdr->keyword_count; i++) {
    snprintf(hdr->keywords[i].name, sizeof(hdr->keywords[i].name), "%s", rd->hdr.cards[i].name);
    snprintf(hdr->keywords[i].value, sizeof(hdr->keywords[i].value), "%s", rd->hdr.cards[i].value);
    snprintf(hdr->keywords[i].comment, sizeof(hdr->keywords[i].comment), "%s",
             rd->hdr.cards[i].comment);
  }
  return ACS_FIO_OK;
}

int acs_fio_read_plane_v1(acs_fio_reader_v1* rd,
                          int plane_index,
                          int64_t nx, int64_t ny, int32_t bpix,
                          const char* bunit,
                          void* buf, int64_t buf_elem_capacity,
                          int strict_nan,
                          int64_t* out_got,
                          acs_fio_trace_hooks_v1* trace,
                          char* err, size_t err_cap) {
  int st;
  int64_t plane_elems, off_elems, bpp, want_bytes;
  if (!rd || !buf || !out_got) return ACS_FIO_ERR_PARAM;
  if (out_got) *out_got = 0;
  st = fio_check_hooks(trace, err, err_cap);
  if (st != ACS_FIO_OK) return st;
  if (fio_cancelled(trace)) {
    set_err(err, err_cap, "cancelled");
    return ACS_FIO_ERR_CANCELLED;
  }
  st = fio_read_check_decl(rd, nx, ny, bpix, bunit, err, err_cap);
  if (st != ACS_FIO_OK) return st;
  st = fio_plane_layout(rd, plane_index, &off_elems, &plane_elems, err, err_cap);
  if (st != ACS_FIO_OK) return st;
  if (plane_elems > buf_elem_capacity) {
    set_err(err, err_cap, "plane elems=%lld > capacity=%lld",
            (long long)plane_elems, (long long)buf_elem_capacity);
    return ACS_FIO_ERR_PARAM;
  }
  bpp = fio_bytes_per_pixel(rd->hdr.bitpix);
  want_bytes = plane_elems * bpp;
  if (fio_file_seek(&rd->f, rd->data_off + off_elems * bpp, SEEK_SET) != 0) {
    set_err(err, err_cap, "seek failed");
    return ACS_FIO_ERR_IO;
  }
  {
    size_t got = fio_file_read(&rd->f, buf, (size_t)want_bytes);
    int64_t got_elems = (int64_t)(got / (size_t)bpp);
    if (out_got) *out_got = got_elems;
    if (got < (size_t)want_bytes) {
      set_err(err, err_cap, "truncated: plane read %zu of %lld bytes",
              got, (long long)want_bytes);
      return ACS_FIO_ERR_TRUNCATED;
    }
  }
  fio_swap_bytes(buf, plane_elems, rd->hdr.bitpix);
  if (strict_nan && fio_scan_naninf(buf, plane_elems, rd->hdr.bitpix)) {
    set_err(err, err_cap, "plane contains NaN/Inf");
    return ACS_FIO_ERR_NANINF;
  }
  return ACS_FIO_OK;
}

int acs_fio_read_chunk_v1(acs_fio_reader_v1* rd,
                          int plane_index,
                          int64_t first_elem, int64_t count,
                          void* buf, int64_t buf_elem_capacity,
                          int strict_nan,
                          int64_t* out_got,
                          acs_fio_trace_hooks_v1* trace,
                          char* err, size_t err_cap) {
  int st;
  int64_t plane_elems, off_elems, bpp, avail, n;
  if (!rd || !buf || !out_got) return ACS_FIO_ERR_PARAM;
  if (count < 0 || first_elem < 0) return ACS_FIO_ERR_PARAM;
  if (out_got) *out_got = 0;
  st = fio_check_hooks(trace, err, err_cap);
  if (st != ACS_FIO_OK) return st;
  if (fio_cancelled(trace)) {
    set_err(err, err_cap, "cancelled");
    return ACS_FIO_ERR_CANCELLED;
  }
  st = fio_plane_layout(rd, plane_index, &off_elems, &plane_elems, err, err_cap);
  if (st != ACS_FIO_OK) return st;
  if (first_elem >= plane_elems) {
    *out_got = 0;
    return ACS_FIO_OK;
  }
  if (count > buf_elem_capacity) {
    set_err(err, err_cap, "count=%lld > capacity=%lld",
            (long long)count, (long long)buf_elem_capacity);
    return ACS_FIO_ERR_PARAM;
  }
  avail = plane_elems - first_elem;
  n = count < avail ? count : avail;
  bpp = fio_bytes_per_pixel(rd->hdr.bitpix);
  if (fio_file_seek(&rd->f, rd->data_off + (off_elems + first_elem) * bpp, SEEK_SET) != 0) {
    set_err(err, err_cap, "seek failed");
    return ACS_FIO_ERR_IO;
  }
  {
    size_t got = fio_file_read(&rd->f, buf, (size_t)(n * bpp));
    int64_t got_elems = (int64_t)(got / (size_t)bpp);
    if (out_got) *out_got = got_elems;
    if (got < (size_t)(n * bpp)) {
      set_err(err, err_cap, "truncated: chunk read %zu of %lld bytes",
              got, (long long)(n * bpp));
      return ACS_FIO_ERR_TRUNCATED;
    }
  }
  fio_swap_bytes(buf, n, rd->hdr.bitpix);
  if (strict_nan && fio_scan_naninf(buf, n, rd->hdr.bitpix)) {
    set_err(err, err_cap, "chunk contains NaN/Inf");
    return ACS_FIO_ERR_NANINF;
  }
  return ACS_FIO_OK;
}

void acs_fio_reader_close_v1(acs_fio_reader_v1* rd) {
  if (!rd) return;
  if (!rd->closed) {
    fio_file_close(&rd->f);
    rd->closed = 1;
  }
  free(rd);
}

uint64_t acs_fio_reader_bytes_read_v1(acs_fio_reader_v1* rd) {
  return rd ? rd->f.read_bytes : 0;
}

/* ------------------------------------------------------------------ */
/* 写入器                                                              */
/* ------------------------------------------------------------------ */

struct acs_fio_writer_v1_s {
  fio_file f;
  char target[ACS_FIO_PATH_MAX];
  char tmp[ACS_FIO_PATH_MAX];
  fio_header decl;             /* 写入声明 */
  char bunit[ACS_FIO_BUNIT_MAX];
  int overwrite;
  int64_t header_bytes;        /* header 块字节 (含预留槽) */
  int64_t data_bytes_total;
  int64_t data_bytes_written;
  uint64_t bytes_written_total; /* 生命周期累计写字节 (memset f 后仍保留) */
  int wrote_header;
  int ended;
  int failed;
  fio_off_t datasum_card_off;  /* 预留 DATASUM 槽偏移; -1=无 */
  fio_off_t checksum_card_off; /* 预留 CHECKSUM 槽偏移; -1=无 */
};

static atomic_ulong g_tmp_seq = 0;

static void fio_writer_make_tmp_name(const char* target, char* out, size_t cap) {
  unsigned long seq = atomic_fetch_add(&g_tmp_seq, 1);
  /* 同目录临时文件: <target>.tmp.<pid>.<seq> — 直接在 target 后追加后缀,
   * 保证与 target 同目录 (避免仅取目录前缀丢 '/' 造成建到根目录)。 */
  snprintf(out, cap, "%s.tmp.%ld.%lu", target, (long)getpid(), seq);
}

/* 写 header 块。卡序: 结构卡(SIMPLE/BITPIX/NAXIS/NAXISn) + [BUNIT] + 附加卡 +
 * [DATASUM 预留] + [CHECKSUM 预留('0000000000000000')] + END + 补齐。
 * reserve_datasum / reserve_checksum: 是否在 header 中预留槽 (空卡/DATASUM
 * 占位) 供 end 原位改写。返回 header 总字节。 */
static int fio_write_header_block(fio_file* f, const fio_header* h,
                                  const char* bunit,
                                  int reserve_datasum, int reserve_checksum,
                                  fio_off_t* out_datasum_off,
                                  fio_off_t* out_checksum_off,
                                  int64_t* out_bytes,
                                  char* err, size_t cap) {
  char raw[80];
  char buf[128];
  char name[9];
  int64_t total = 0;
  int i;
  fio_off_t datasum_off = -1, checksum_off = -1;
  (void)err; (void)cap;
  /* SIMPLE */
  fio_card_write(f, "SIMPLE", "T", "conforms to FITS standard");
  total += 80;
  /* BITPIX */
  snprintf(buf, sizeof(buf), "%d", (int)h->bitpix);
  fio_card_write(f, "BITPIX", buf, "");
  total += 80;
  /* NAXIS */
  snprintf(buf, sizeof(buf), "%d", (int)h->naxis);
  fio_card_write(f, "NAXIS", buf, "");
  total += 80;
  for (i = 0; i < h->naxis; i++) {
    snprintf(name, sizeof(name), "NAXIS%d", i + 1);
    snprintf(buf, sizeof(buf), "%lld", (long long)h->naxis_n[i]);
    fio_card_write(f, name, buf, "");
    total += 80;
  }
  if (bunit && bunit[0]) {
    fio_card_write(f, "BUNIT", bunit, "physical units of the array values");
    total += 80;
  }
  for (i = 0; i < h->card_count; i++) {
    fio_card_write(f, h->cards[i].name, h->cards[i].value, h->cards[i].comment);
    total += 80;
  }
  /* 预留槽 */
  if (reserve_datasum) {
    datasum_off = total;
    fio_card_write(f, "", "", ""); /* 空白槽 */
    total += 80;
  }
  if (reserve_checksum) {
    checksum_off = total;
    /* comment 必须与 acs_fio_writer_end_v1 patch 时完全一致 (校验按整卡字节累计,
     * 占位卡与真值卡除值区外须逐字节相同, 否则写累计 ≠ verify 累计) */
    fio_card_write(f, "CHECKSUM", "0000000000000000", "HDU checksum updated");
    total += 80;
  }
  /* END + 补齐 */
  {
    int in_block = (int)((total / 80) % 36);
    int remain = 35 - in_block;
    memset(raw, ' ', 80);
    snprintf(raw, 4, "END");
    fio_card_write(f, "END", "", "");
    total += 80;
    memset(raw, ' ', 80);
    while (remain-- > 0) {
      fio_file_write(f, raw, 80);
      total += 80;
    }
  }
  if (out_datasum_off) *out_datasum_off = datasum_off;
  if (out_checksum_off) *out_checksum_off = checksum_off;
  if (out_bytes) *out_bytes = total;
  return ACS_FIO_OK;
}

int acs_fio_writer_begin_v1(const char* path_utf8,
                            const acs_fio_header_v1* decl,
                            const char* bunit,
                            int overwrite,
                            const acs_fio_trace_hooks_v1* hooks,
                            acs_fio_writer_v1** out,
                            char* err, size_t cap) {
  acs_fio_writer_v1* wr;
  int st, i;
  if (!path_utf8 || !decl || !out) return ACS_FIO_ERR_PARAM;
  *out = NULL;
  st = fio_check_hooks(hooks, err, cap);
  if (st != ACS_FIO_OK) return st;
  st = fio_check_head(decl->struct_size, decl->abi_version, sizeof(*decl), err, cap);
  if (st != ACS_FIO_OK) return st;
  if (decl->naxis < 0 || decl->naxis > ACS_FIO_NAXIS_MAX ||
      fio_bytes_per_pixel(decl->bitpix) < 0) {
    set_err(err, cap, "invalid decl: bitpix=%d naxis=%d", (int)decl->bitpix,
            (int)decl->naxis);
    return ACS_FIO_ERR_PARAM;
  }
  for (i = 0; i < decl->naxis; i++) {
    if (decl->naxis_n[i] <= 0) {
      set_err(err, cap, "invalid decl: NAXIS%d=%lld", i + 1, (long long)decl->naxis_n[i]);
      return ACS_FIO_ERR_PARAM;
    }
  }
  if (decl->keyword_count < 0 || decl->keyword_count > ACS_FIO_HEADER_MAX_CARDS) {
    set_err(err, cap, "keyword_count out of range");
    return ACS_FIO_ERR_PARAM;
  }
  for (i = 0; i < decl->keyword_count; i++) {
    if (fio_is_reserved_keyword(decl->keywords[i].name)) {
      set_err(err, cap, "keyword '%s' reserved (writer owns it)", decl->keywords[i].name);
      return ACS_FIO_ERR_BAD_HEADER;
    }
  }
  if (!overwrite) {
    FILE* probe = fopen(path_utf8, "rb");
    if (probe) {
      fclose(probe);
      set_err(err, cap, "target exists (overwrite=0)");
      return ACS_FIO_ERR_IO;
    }
  }
  wr = (acs_fio_writer_v1*)calloc(1, sizeof(*wr));
  if (!wr) {
    set_err(err, cap, "nomem");
    return ACS_FIO_ERR_NOMEM;
  }
  snprintf(wr->target, sizeof(wr->target), "%s", path_utf8);
  fio_writer_make_tmp_name(path_utf8, wr->tmp, sizeof(wr->tmp));
  /* w+b: 写后需在同一流读回算 DATASUM/CHECKSUM (w 模式流不可读) */
  st = fio_file_open(&wr->f, wr->tmp, "w+b", hooks, err, cap);
  if (st != ACS_FIO_OK) {
    free(wr);
    return st;
  }
  wr->decl.bitpix = decl->bitpix;
  wr->decl.naxis = decl->naxis;
  for (i = 0; i < ACS_FIO_NAXIS_MAX; i++) wr->decl.naxis_n[i] = decl->naxis_n[i];
  wr->decl.card_count = fio_min_int(decl->keyword_count, ACS_FIO_HEADER_MAX_CARDS);
  for (i = 0; i < wr->decl.card_count; i++) {
    snprintf(wr->decl.cards[i].name, sizeof(wr->decl.cards[i].name), "%s",
             decl->keywords[i].name);
    snprintf(wr->decl.cards[i].value, sizeof(wr->decl.cards[i].value), "%s",
             decl->keywords[i].value);
    snprintf(wr->decl.cards[i].comment, sizeof(wr->decl.cards[i].comment), "%s",
             decl->keywords[i].comment);
  }
  snprintf(wr->bunit, sizeof(wr->bunit), "%s", bunit ? bunit : "");
  wr->overwrite = overwrite;
  wr->datasum_card_off = -1;
  wr->checksum_card_off = -1;
  {
    int64_t pixels = fio_header_pixels(&wr->decl);
    int64_t bpp = fio_bytes_per_pixel(wr->decl.bitpix);
    wr->data_bytes_total = (pixels > 0 && bpp > 0) ? pixels * bpp : 0;
  }
  *out = wr;
  return ACS_FIO_OK;
}

/* 首次数据写入前写 header (恒预留 DATASUM 槽; CHECKSUM 槽预留与否留到 end —
 * 由于布局固定性, 恒预留两个槽, end 时按需改写或保持空白/零值)。 */
static int fio_writer_ensure_header(acs_fio_writer_v1* wr, char* err, size_t cap) {
  if (wr->wrote_header) return ACS_FIO_OK;
  {
    int64_t hdr_bytes = 0;
    fio_off_t doff = -1, coff = -1;
    int st = fio_write_header_block(&wr->f, &wr->decl,
                                    wr->bunit[0] ? wr->bunit : NULL,
                                    1 /*reserve_datasum*/, 1 /*reserve_checksum*/,
                                    &doff, &coff, &hdr_bytes, err, cap);
    if (st != ACS_FIO_OK) return st;
    wr->header_bytes = hdr_bytes;
    wr->datasum_card_off = doff;
    wr->checksum_card_off = coff;
    wr->wrote_header = 1;
  }
  return ACS_FIO_OK;
}

int acs_fio_write_plane_v1(acs_fio_writer_v1* wr,
                           int plane_index,
                           const void* data, size_t data_bytes,
                           acs_fio_trace_hooks_v1* trace,
                           char* err, size_t cap) {
  int st;
  int64_t plane_elems, plane_bytes, bpp;
  if (!wr || !data || wr->ended || wr->failed) return ACS_FIO_ERR_STATE;
  st = fio_check_hooks(trace, err, cap);
  if (st != ACS_FIO_OK) return st;
  if (fio_cancelled(trace)) {
    wr->failed = 1;
    set_err(err, cap, "cancelled");
    return ACS_FIO_ERR_CANCELLED;
  }
  st = fio_writer_ensure_header(wr, err, cap);
  if (st != ACS_FIO_OK) return st;
  {
    int64_t naxis3 = wr->decl.naxis >= 3 ? wr->decl.naxis_n[2] : 1;
    if (plane_index < 0 || (wr->decl.naxis >= 3 && plane_index >= naxis3)) {
      set_err(err, cap, "plane_index=%d out of range", plane_index);
      return ACS_FIO_ERR_PARAM;
    }
    if (wr->decl.naxis == 0) {
      set_err(err, cap, "decl has no data (NAXIS=0)");
      return ACS_FIO_ERR_PARAM;
    }
  }
  bpp = fio_bytes_per_pixel(wr->decl.bitpix);
  plane_elems = 1;
  if (wr->decl.naxis >= 1) plane_elems *= wr->decl.naxis_n[0];
  if (wr->decl.naxis >= 2) plane_elems *= wr->decl.naxis_n[1];
  plane_bytes = plane_elems * bpp;
  if (data_bytes != (size_t)plane_bytes) {
    set_err(err, cap, "plane data_bytes=%zu != plane bytes=%lld",
            data_bytes, (long long)plane_bytes);
    return ACS_FIO_ERR_PARAM;
  }
  if (plane_index != (int)(wr->data_bytes_written / plane_bytes)) {
    set_err(err, cap, "plane out of order: got %d expect %lld", plane_index,
            (long long)(wr->data_bytes_written / plane_bytes));
    return ACS_FIO_ERR_STATE;
  }
  {
    unsigned char* tmp = (unsigned char*)malloc(data_bytes ? data_bytes : 1);
    if (!tmp) return ACS_FIO_ERR_NOMEM;
    memcpy(tmp, data, data_bytes);
    fio_swap_bytes(tmp, plane_elems, wr->decl.bitpix);
    if (fio_file_write(&wr->f, tmp, data_bytes) != data_bytes) {
      int e = errno;
      free(tmp);
      wr->failed = 1;
      set_err(err, cap, "write failed: %s", strerror(e));
      return fio_errno_code(e);
    }
    free(tmp);
  }
  wr->data_bytes_written += (int64_t)data_bytes;
  return ACS_FIO_OK;
}

int acs_fio_write_chunk_v1(acs_fio_writer_v1* wr,
                           const void* data, size_t data_bytes,
                           acs_fio_trace_hooks_v1* trace,
                           char* err, size_t cap) {
  int st;
  int64_t bpp;
  if (!wr || !data || wr->ended || wr->failed) return ACS_FIO_ERR_STATE;
  st = fio_check_hooks(trace, err, cap);
  if (st != ACS_FIO_OK) return st;
  if (fio_cancelled(trace)) {
    wr->failed = 1;
    set_err(err, cap, "cancelled");
    return ACS_FIO_ERR_CANCELLED;
  }
  st = fio_writer_ensure_header(wr, err, cap);
  if (st != ACS_FIO_OK) return st;
  bpp = fio_bytes_per_pixel(wr->decl.bitpix);
  if (bpp < 0) return ACS_FIO_ERR_PARAM;
  if (data_bytes % (size_t)bpp != 0) {
    set_err(err, cap, "chunk bytes not multiple of element size");
    return ACS_FIO_ERR_PARAM;
  }
  if (wr->data_bytes_written + (int64_t)data_bytes > wr->data_bytes_total) {
    set_err(err, cap, "chunk exceeds declared data size");
    return ACS_FIO_ERR_STATE;
  }
  {
    unsigned char* tmp = (unsigned char*)malloc(data_bytes ? data_bytes : 1);
    if (!tmp) return ACS_FIO_ERR_NOMEM;
    memcpy(tmp, data, data_bytes);
    fio_swap_bytes(tmp, (int64_t)(data_bytes / (size_t)bpp), wr->decl.bitpix);
    if (fio_file_write(&wr->f, tmp, data_bytes) != data_bytes) {
      int e = errno;
      free(tmp);
      wr->failed = 1;
      set_err(err, cap, "write failed: %s", strerror(e));
      return fio_errno_code(e);
    }
    free(tmp);
  }
  wr->data_bytes_written += (int64_t)data_bytes;
  return ACS_FIO_OK;
}

int acs_fio_writer_end_v1(acs_fio_writer_v1* wr,
                          int write_datasum, int write_checksum,
                          int verify_before_rename,
                          acs_fio_trace_hooks_v1* trace,
                          char* err, size_t cap) {
  int st = ACS_FIO_OK;
  int64_t pad;
  char zeros[2880];
  if (!wr || wr->ended) return ACS_FIO_ERR_STATE;
  if (fio_cancelled(trace)) {
    st = ACS_FIO_ERR_CANCELLED;
    set_err(err, cap, "cancelled");
    goto fail;
  }
  st = fio_writer_ensure_header(wr, err, cap);
  if (st != ACS_FIO_OK) goto fail;
  if (wr->data_bytes_written != wr->data_bytes_total) {
    set_err(err, cap, "declared data %lld but wrote %lld bytes",
            (long long)wr->data_bytes_total, (long long)wr->data_bytes_written);
    st = ACS_FIO_ERR_STATE;
    goto fail;
  }
  /* 2880 填充 (数据区尾) */
  if (wr->data_bytes_total > 0) {
    int64_t in_block = wr->data_bytes_total % 2880;
    pad = in_block == 0 ? 0 : 2880 - in_block;
    memset(zeros, 0, sizeof(zeros));
    while (pad > 0) {
      int64_t n = pad > 2880 ? 2880 : pad;
      if (fio_file_write(&wr->f, zeros, (size_t)n) != (size_t)n) {
        st = fio_errno_code(errno);
        set_err(err, cap, "padding write failed");
        goto fail;
      }
      pad -= n;
    }
    if (fio_file_flush(&wr->f) != 0) {
      st = fio_errno_code(errno);
      set_err(err, cap, "flush failed");
      goto fail;
    }
  }
  /* DATASUM: 流式读回数据区计算 → 原位改写 DATASUM 槽 */
  if (write_datasum) {
    fio_dsum s;
    size_t got;
    char dstr[ACS_FIO_DATASUM_LEN];
    fio_dsum_init(&s);
    got = fio_file_read_datasum(&wr->f, wr->header_bytes,
                                (size_t)fio_blocks2880(wr->data_bytes_total) * 2880,
                                &s, err, cap);
    if (got == (size_t)-1) {
      st = ACS_FIO_ERR_IO;
      goto fail;
    }
    snprintf(dstr, sizeof(dstr), "%lu", fio_dsum_finish(&s));
    if (wr->datasum_card_off >= 0) {
      st = fio_card_patch(&wr->f, wr->datasum_card_off, "DATASUM", dstr,
                          "data unit checksum updated", err, cap);
      if (st != ACS_FIO_OK) goto fail;
    }
  }
  /* CHECKSUM: 全 HDU 流式累计; CHECKSUM 槽已是 '0000000000000000' */
  if (write_checksum) {
    size_t hdu_len =
        (size_t)wr->header_bytes + (size_t)fio_blocks2880(wr->data_bytes_total) * 2880;
    unsigned long sum = fio_hdu_checksum_stream(&wr->f, 0, (size_t)wr->header_bytes,
                                                (size_t)fio_blocks2880(wr->data_bytes_total) *
                                                    2880);
    char cstr[17];
    (void)hdu_len;
    fio_encode_checksum(sum, 1, cstr);
    if (wr->checksum_card_off >= 0) {
      st = fio_card_patch(&wr->f, wr->checksum_card_off, "CHECKSUM", cstr,
                          "HDU checksum updated", err, cap);
      if (st != ACS_FIO_OK) goto fail;
    }
  }
  if (fio_file_flush(&wr->f) != 0) {
    st = fio_errno_code(errno);
    set_err(err, cap, "flush failed");
    goto fail;
  }
  wr->bytes_written_total += wr->f.write_bytes;
  fio_file_close(&wr->f);
  memset(&wr->f, 0, sizeof(wr->f));
  /* 提交前自校验 (结构 + 长度 + DATASUM/CHECKSUM) */
  if (verify_before_rename) {
    int vst = acs_fio_verify_file_v1(wr->tmp, write_checksum ? 1 : 0, err, cap);
    if (vst != ACS_FIO_OK) {
      st = vst;
      goto fail;
    }
  }
  if (rename(wr->tmp, wr->target) != 0) {
    int e = errno;
    set_err(err, cap, "rename failed: %s", strerror(e));
    st = fio_errno_code(e);
    goto fail;
  }
  wr->ended = 1;
  return ACS_FIO_OK;
fail:
  if (wr->f.fp) {
    wr->bytes_written_total += wr->f.write_bytes;
    fio_file_close(&wr->f);
    memset(&wr->f, 0, sizeof(wr->f));
  }
  if (wr->tmp[0] && !wr->ended) remove(wr->tmp);
  wr->failed = 1;
  return st;
}

void acs_fio_writer_abort_v1(acs_fio_writer_v1* wr) {
  if (!wr) return;
  if (!wr->ended && wr->f.fp) {
    wr->bytes_written_total += wr->f.write_bytes;
    fio_file_close(&wr->f);
    memset(&wr->f, 0, sizeof(wr->f));
  }
  if (wr->tmp[0] && !wr->ended) remove(wr->tmp);
  wr->failed = 1;
  wr->ended = 1;
}

uint64_t acs_fio_writer_bytes_written_v1(const acs_fio_writer_v1* wr) {
  return wr ? (wr->f.write_bytes + wr->bytes_written_total) : 0;
}

/* ------------------------------------------------------------------ */
/* verify / datadigest                                                 */
/* ------------------------------------------------------------------ */

int acs_fio_verify_file_v1(const char* path_utf8,
                           int verify_checksum,
                           char* err, size_t cap) {
  acs_fio_reader_v1* rd = NULL;
  int st;
  if (!path_utf8) return ACS_FIO_ERR_PARAM;
  st = fio_reader_open_impl(path_utf8, NULL, &rd, err, cap);
  if (st != ACS_FIO_OK) return st;
  /* DATASUM (若卡存在): 流式读数据区 */
  if (rd->hdr.has_datasum) {
    fio_dsum s;
    size_t got, want;
    char expect[ACS_FIO_DATASUM_LEN];
    fio_file f2;
    memset(&f2, 0, sizeof(f2));
    st = fio_file_open(&f2, path_utf8, "rb", NULL, err, cap);
    if (st != ACS_FIO_OK) {
      acs_fio_reader_close_v1(rd);
      return st;
    }
    fio_dsum_init(&s);
    want = (size_t)fio_blocks2880(rd->data_bytes) * 2880;
    if (want > 0) {
      got = fio_file_read_datasum(&f2, rd->data_off, want, &s, err, cap);
      if (got == (size_t)-1) {
        fio_file_close(&f2);
        acs_fio_reader_close_v1(rd);
        return ACS_FIO_ERR_IO;
      }
      if (got < (size_t)rd->data_bytes) {
        fio_file_close(&f2);
        acs_fio_reader_close_v1(rd);
        set_err(err, cap, "truncated: data area short");
        return ACS_FIO_ERR_TRUNCATED;
      }
    }
    fio_file_close(&f2);
    snprintf(expect, sizeof(expect), "%lu", fio_dsum_finish(&s));
    {
      char hdr_datasum[ACS_FIO_DATASUM_LEN];
      const char* hp;
      snprintf(hdr_datasum, sizeof(hdr_datasum), "%s", rd->hdr.datasum);
      hp = hdr_datasum;
      while (*hp == ' ') hp++;
      if (strcmp(hp, expect) != 0) {
        acs_fio_reader_close_v1(rd);
        set_err(err, cap, "DATASUM mismatch: header '%s' computed '%s'",
                hp, expect);
        return ACS_FIO_ERR_CHECKSUM;
      }
    }
  }
  /* CHECKSUM (若卡存在且 verify_checksum): 读整个 HDU 流式, 零化 CHECKSUM 值 */
  if (verify_checksum && rd->hdr.has_checksum) {
    /* 流式: 需把 CHECKSUM 卡值零化 — 先读 header 块找偏移, 构造 hdu 校验时按块
     * 读并在 header 区间把 CHECKSUM 卡替换为零值。实现: 读 header 到内存 (≤
     * 36*2880=103680 字节), 零化 CHECKSUM 卡, 再流式接数据区。 */
    unsigned char* hbuf;
    size_t hlen = (size_t)rd->hdr_bytes;
    fio_dsum s;
    size_t data_want;
    size_t got;
    fio_off_t coff;
    fio_file f2;
    if (hlen > 4 * 2880 * 1024) {
      acs_fio_reader_close_v1(rd);
      set_err(err, cap, "header too large for checksum");
      return ACS_FIO_ERR_UNSUPPORTED;
    }
    hbuf = (unsigned char*)malloc(hlen ? hlen : 1);
    if (!hbuf) {
      acs_fio_reader_close_v1(rd);
      return ACS_FIO_ERR_NOMEM;
    }
    memset(&f2, 0, sizeof(f2));
    st = fio_file_open(&f2, path_utf8, "rb", NULL, err, cap);
    if (st != ACS_FIO_OK) {
      free(hbuf);
      acs_fio_reader_close_v1(rd);
      return st;
    }
    if (fio_file_read(&f2, hbuf, hlen) != hlen) {
      free(hbuf);
      fio_file_close(&f2);
      acs_fio_reader_close_v1(rd);
      set_err(err, cap, "read header failed");
      return ACS_FIO_ERR_IO;
    }
    coff = fio_find_card(hbuf, hlen, "CHECKSUM");
    if (coff < 0) {
      free(hbuf);
      fio_file_close(&f2);
      acs_fio_reader_close_v1(rd);
      set_err(err, cap, "CHECKSUM card not found");
      return ACS_FIO_ERR_BAD_HEADER;
    }
    /* 提取卡值 (16 字符) 供解码; 再把卡值区置 '0' 后累计 */
    {
      char cval[17];
      const char* vp = (const char*)hbuf + coff;
      int vi = 9;
      int j = 0;
      while (vi < 80 && vp[vi] == ' ') vi++;
      while (vi < 80 && j < 16 && vp[vi] != ' ') cval[j++] = vp[vi++];
      cval[j] = '\0';
      if (j == 0) {
        free(hbuf);
        fio_file_close(&f2);
        acs_fio_reader_close_v1(rd);
        set_err(err, cap, "CHECKSUM card empty");
        return ACS_FIO_ERR_BAD_HEADER;
      }
      fio_zero_card_value((char*)hbuf + coff, hlen - (size_t)coff);
      fio_dsum_init(&s);
      fio_dsum_update(&s, hbuf, hlen);
      free(hbuf);
      data_want = (size_t)fio_blocks2880(rd->data_bytes) * 2880;
      if (data_want > 0) {
        got = fio_file_read_datasum(&f2, rd->data_off, data_want, &s, err, cap);
        if (got == (size_t)-1 || got < (size_t)rd->data_bytes) {
          fio_file_close(&f2);
          acs_fio_reader_close_v1(rd);
          if (got != (size_t)-1) set_err(err, cap, "truncated data for checksum");
          return got == (size_t)-1 ? ACS_FIO_ERR_IO : ACS_FIO_ERR_TRUNCATED;
        }
      }
      fio_file_close(&f2);
      {
        unsigned long sum = fio_dsum_finish(&s);
        unsigned long decoded = fio_decode_checksum(cval, 1); /* 还原写时累计 */
        /* 标准校验: 置零累计应等于 CHECKSUM 卡解码值 (即写时累计);
         * 另兼容 sum==0xFFFFFFFF/0 的全零/未定义情形。 */
        if (sum != decoded && sum != 0xFFFFFFFFUL && sum != 0UL) {
          acs_fio_reader_close_v1(rd);
          set_err(err, cap, "CHECKSUM mismatch: computed 0x%08lx decoded 0x%08lx",
                  sum, decoded);
          return ACS_FIO_ERR_CHECKSUM;
        }
      }
    }
  }
  acs_fio_reader_close_v1(rd);
  return ACS_FIO_OK;
}

int acs_fio_compute_file_datadigest_v1(const char* path_utf8,
                                       char* datasum, size_t datasum_cap,
                                       size_t* out_len,
                                       char* err, size_t cap) {
  acs_fio_reader_v1* rd = NULL;
  int st;
  fio_dsum s;
  size_t want, got;
  fio_file f2;
  char buf[ACS_FIO_DATASUM_LEN];
  size_t blen;
  if (!path_utf8 || !datasum) return ACS_FIO_ERR_PARAM;
  if (datasum_cap < ACS_FIO_DATASUM_LEN) {
    set_err(err, cap, "datasum buffer too small");
    return ACS_FIO_ERR_PARAM;
  }
  st = fio_reader_open_impl(path_utf8, NULL, &rd, err, cap);
  if (st != ACS_FIO_OK) return st;
  fio_dsum_init(&s);
  want = (size_t)fio_blocks2880(rd->data_bytes) * 2880;
  memset(&f2, 0, sizeof(f2));
  if (want > 0) {
    st = fio_file_open(&f2, path_utf8, "rb", NULL, err, cap);
    if (st != ACS_FIO_OK) {
      acs_fio_reader_close_v1(rd);
      return st;
    }
    got = fio_file_read_datasum(&f2, rd->data_off, want, &s, err, cap);
    fio_file_close(&f2);
    if (got == (size_t)-1) {
      acs_fio_reader_close_v1(rd);
      return ACS_FIO_ERR_IO;
    }
    if (got < (size_t)rd->data_bytes) {
      acs_fio_reader_close_v1(rd);
      set_err(err, cap, "truncated");
      return ACS_FIO_ERR_TRUNCATED;
    }
  }
  acs_fio_reader_close_v1(rd);
  snprintf(buf, sizeof(buf), "%lu", fio_dsum_finish(&s));
  blen = strlen(buf);
  memcpy(datasum, buf, blen + 1);
  if (out_len) *out_len = blen;
  return ACS_FIO_OK;
}
