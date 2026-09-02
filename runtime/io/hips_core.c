/* AstroCS HiPS 输入读取核心实现 — runtime/io/hips_core.c (IO-002)
 *
 * 职责: acs_hips_* C ABI (hips_input_v1.h) 的实现。读取磁盘上已发布的 IVOA
 * HiPS 1.4 兼容子产品目录: properties 解析校验、NESTED tile address 布局
 * (NorderK/DirD/NpixN.fits)、tile width/order 校验、FITS-only 科学平面读取
 * (复用 IO-001 fits_core)、partial tree、MOC optional hint、缺 tile 状态。
 *
 * 关键设计:
 *  - 纯 C11; 无 CFITSIO 依赖 (tile FITS 打开/header/plane 读取经 IO-001
 *    acs_fio_reader_*); 跨边界无托管分配 (out 缓冲由调用方提供); 内部堆仅
 *    用于句柄与 MOC 列表。
 *  - properties: 逐行 key=value (去首尾空白, '#'/空行跳过, 同 aio reader 语义);
 *    必填键缺失/值非法 → ACS_HIPS_ERR_PROPERTIES。
 *  - 布局: tile 路径 Norder{K}/Dir{D}/Npix{N}.fits, D=ipix/10000, N=ipix%10000;
 *    ipix 越界 [0, 12*4^K) → ADDRESS。tile FITS 头 (经 fits_core get_header)
 *    PIXTYPE/ORDERING/COORDSYS/NSIDE/FIRSTPIX/LASTPIX 存在时校验一致, 不符 →
 *    TILE_INVALID。BITPIX ∉ {8,-32,-64} → TILE_INVALID (布局不符拒绝)。
 *  - 缺 tile: order-K 域内文件不存在 → TILE_MISSING; 绝不父 order 静默回退。
 *  - MOC optional hint: Moc.fits (BINTABLE + UNIQ 列 + MOCORDER) 存在且 MOCORDER
 *    等于 hips_order 时, 提取叶级 NESTED ipix 列表供 *_tile_count/_ipix;
 *    缺失/损坏/order 不符 → 无 hint (不失败, partial tree 合法)。
 *
 * 并发: reentrant (无全局状态); 句柄非共享。
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "astrocs/io/hips_input_v1.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define hips_fseeko _fseeki64
#define hips_ftello _ftelli64
#else
#include <unistd.h>
#define hips_fseeko fseeko
#define hips_ftello ftello
#endif

/* ------------------------------------------------------------------ */
/* 内部小工具                                                          */
/* ------------------------------------------------------------------ */

static void hips_set_err(char* err, size_t cap, const char* fmt, ...) {
  if (!err || cap == 0) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(err, cap, fmt, ap);
  va_end(ap);
  err[cap - 1] = '\0';
}

/* FITS 字符串卡值比较: 容忍两侧填充空格 (astropy/cfitsio 字符串字段按 8 字符
 * 填充), 大小写不敏感 (PIXTYPE/ORDERING/COORDSYS/TTYPEn 等关键字值)。 */
static int hips_card_val_eq(const char* expect, const char* actual) {
  size_t en = expect ? strlen(expect) : 0;
  size_t an = actual ? strlen(actual) : 0;
  size_t i;
  while (en > 0 && expect[en - 1] == ' ') --en;
  while (an > 0 && actual[an - 1] == ' ') --an;
  while (en > 0 && expect[0] == ' ') { ++expect; --en; }
  while (an > 0 && actual[0] == ' ') { ++actual; --an; }
  if (en != an) return 0;
  for (i = 0; i < en; ++i) {
    if (tolower((unsigned char)expect[i]) != tolower((unsigned char)actual[i]))
      return 0;
  }
  return 1;
}

static int hips_strcaseeq(const char* a, const char* b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

/* 就地去首尾空白 (含 \r); 返回处理后的字符串长度 */
static size_t hips_trim(char* s) {
  size_t n = strlen(s);
  size_t i = 0, w = 0;
  while (i < n && isspace((unsigned char)s[i])) ++i;
  for (; i < n; ++i) {
    if (isspace((unsigned char)s[i])) {
      size_t j = i;
      while (j < n && isspace((unsigned char)s[j])) ++j;
      if (j == n) break;
      i = j - 1;
      continue;
    }
    s[w++] = s[i];
  }
  s[w] = '\0';
  return w;
}

/* 严格十进制整数解析 (整串数字) */
static int hips_parse_int(const char* s, int64_t* out) {
  char* end = NULL;
  long long v;
  if (!s || !*s) return 0;
  v = strtoll(s, &end, 10);
  if (!end || *end != '\0') return 0;
  *out = (int64_t)v;
  return 1;
}

static int hips_is_pow2(uint64_t v) { return v != 0 && (v & (v - 1)) == 0; }

static int hips_file_exists(const char* path) {
  FILE* f = fopen(path, "rb");
  if (f) {
    fclose(f);
    return 1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* 句柄                                                               */
/* ------------------------------------------------------------------ */

typedef struct hips_prop {
  char key[ACS_HIPS_PROP_KEY_MAX];
  char value[ACS_HIPS_PROP_VALUE_MAX];
} hips_prop;

struct acs_hips_handle_v1_s {
  char dir[ACS_HIPS_PATH_MAX]; /* 子产品目录 (含 properties/Moc.fits/tiles) */
  int32_t order;               /* hips_order K */
  int32_t tile_width;          /* hips_tile_width TW */
  uint64_t nside;              /* 2^(K+9) (tile 像素 HEALPix 分辨率) */
  hips_prop props[ACS_HIPS_PROP_MAX];
  int32_t prop_count;
  uint64_t* moc_tiles;         /* MOC optional hint: 叶级 NESTED ipix (order==K) */
  int64_t moc_count;
  acs_fio_trace_hooks_v1 hooks;
  int has_hooks;
};

static const char* hips_prop_get(acs_hips_handle_v1 h, const char* key) {
  int i;
  if (!h || !key) return NULL;
  for (i = 0; i < h->prop_count; ++i)
    if (strcmp(h->props[i].key, key) == 0) return h->props[i].value;
  return NULL;
}

#define ACS_HIPS_PROP_LINE_MAX 512

/* 解析 properties 文本文件; 返回键数或 -1 (IO/超限) */
static int hips_parse_properties_file(const char* path, hips_prop* props,
                                      int max_props, char* err, size_t cap) {
  FILE* f;
  char line[ACS_HIPS_PROP_LINE_MAX];
  int count = 0;
  f = fopen(path, "rb");
  if (!f) {
    hips_set_err(err, cap, "properties 缺失/不可读: %s", path);
    return -1;
  }
  while (fgets(line, (int)sizeof(line), f)) {
    char* eq;
    char* v;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
    if (n == 0 || line[0] == '#') continue;
    eq = strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    hips_trim(line);
    if (!line[0]) continue;
    v = eq + 1;
    hips_trim(v);
    if (count >= max_props) {
      fclose(f);
      hips_set_err(err, cap, "properties 键数超限 %d", max_props);
      return -1;
    }
    snprintf(props[count].key, sizeof(props[count].key), "%s", line);
    snprintf(props[count].value, sizeof(props[count].value), "%s", v);
    ++count;
  }
  fclose(f);
  return count;
}

/* 必填键校验 (合同 §3.1); 0=OK 否则错误码 */
static int hips_validate_properties(acs_hips_handle_v1 h, char* err, size_t cap) {
  const char* ver = hips_prop_get(h, "hips_version");
  const char* order_s = hips_prop_get(h, "hips_order");
  const char* tw_s = hips_prop_get(h, "hips_tile_width");
  const char* fmt = hips_prop_get(h, "hips_tile_format");
  const char* frame = hips_prop_get(h, "hips_frame");
  int64_t order = -1, tw = -1;

  if (!ver) {
    hips_set_err(err, cap, "properties 缺 hips_version");
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (strncmp(ver, "1.4", 3) != 0) {
    hips_set_err(err, cap, "hips_version='%s' 不支持 (合同接受 1.4)", ver);
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (!order_s || !hips_parse_int(order_s, &order) ||
      order < ACS_HIPS_ORDER_MIN || order > ACS_HIPS_ORDER_MAX) {
    hips_set_err(err, cap, "hips_order 缺失/非法: '%s'", order_s ? order_s : "(null)");
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (!tw_s || !hips_parse_int(tw_s, &tw) || tw < 1 ||
      tw > ACS_HIPS_TILE_WIDTH_MAX || !hips_is_pow2((uint64_t)tw)) {
    hips_set_err(err, cap, "hips_tile_width 缺失/非2次幂/越界: '%s'",
                 tw_s ? tw_s : "(null)");
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (!fmt) {
    hips_set_err(err, cap, "properties 缺 hips_tile_format");
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (!hips_strcaseeq(fmt, "fits")) {
    hips_set_err(err, cap, "hips_tile_format='%s' 非 fits (科学平面 FITS-only)", fmt);
    return ACS_HIPS_ERR_UNSUPPORTED;
  }
  if (!frame) {
    hips_set_err(err, cap, "properties 缺 hips_frame");
    return ACS_HIPS_ERR_PROPERTIES;
  }
  if (!hips_strcaseeq(frame, "equatorial")) {
    hips_set_err(err, cap, "hips_frame='%s' 非 equatorial (未知 frame 拒绝)", frame);
    return ACS_HIPS_ERR_UNSUPPORTED;
  }
  h->order = (int32_t)order;
  h->tile_width = (int32_t)tw;
  h->nside = 1ULL << ((uint64_t)order + 9u);
  return ACS_HIPS_OK;
}

/* 目录拼接: base + '/' + product (product 可 NULL/空 = base 本身) */
static int hips_dir_join(const char* base, const char* product,
                         char* out, size_t cap, char* err, size_t err_cap) {
  size_t n = strlen(base);
  if (n == 0 || n >= cap) {
    hips_set_err(err, err_cap, "base_dir 非法/过长");
    return ACS_HIPS_ERR_PARAM;
  }
  memcpy(out, base, n);
  out[n] = '\0';
  while (n > 0 && (out[n - 1] == '/' || out[n - 1] == '\\')) out[--n] = '\0';
  if (product && product[0]) {
    size_t pn = strlen(product);
    if (n + 1 + pn + 1 > cap) {
      hips_set_err(err, err_cap, "product 路径过长");
      return ACS_HIPS_ERR_PARAM;
    }
    out[n++] = '/';
    memcpy(out + n, product, pn + 1);
  }
  return ACS_HIPS_OK;
}

/* ------------------------------------------------------------------ */
/* 轻量 FITS BINTABLE 头/数据区解析 (仅 MOC: UNIQ 列)                */
/* ------------------------------------------------------------------ */

typedef struct moc_card {
  char name[9];
  char value[80];
  int found;
} moc_card;

/* 解析一个 2880 块内卡片; 遇 END 记录 {END,found} 并停止。 */
static void moc_parse_block(const unsigned char* blk, moc_card* cards, int* ncards) {
  int i;
  *ncards = 0;
  for (i = 0; i < 36; ++i) {
    const unsigned char* raw = blk + (size_t)i * 80;
    char name[9];
    int j;
    for (j = 0; j < 8; ++j) name[j] = (char)raw[j];
    name[8] = '\0';
    for (j = 7; j >= 0; --j) {
      if (name[j] == ' ') name[j] = '\0';
      else break;
    }
    if (!name[0]) continue;
    snprintf(cards[*ncards].name, sizeof(cards[*ncards].name), "%s", name);
    cards[*ncards].found = 1;
    if (strcmp(name, "END") == 0) {
      ++*ncards;
      return;
    }
    if (raw[8] == '=') {
      int col = 10;
      char val[80] = {0};
      size_t len = 0;
      while (col < 80 && raw[col] == ' ') ++col;
      if (col < 80 && raw[col] == '\'') {
        ++col;
        while (col < 80 && len + 1 < sizeof(val)) {
          if (raw[col] == '\'') {
            if (col + 1 < 80 && raw[col + 1] == '\'') { val[len++] = '\''; col += 2; continue; }
            break;
          }
          val[len++] = (char)raw[col++];
        }
        val[len] = '\0';
      } else {
        while (col < 80 && len + 1 < sizeof(val) && raw[col] != '/')
          val[len++] = (char)raw[col++];
        val[len] = '\0';
        while (len > 0 && val[len - 1] == ' ') val[--len] = '\0';
      }
      snprintf(cards[*ncards].value, sizeof(cards[*ncards].value), "%s", val);
    }
    ++*ncards;
  }
}

static const moc_card* moc_find(const moc_card* cards, int n, const char* name) {
  int i;
  for (i = 0; i < n; ++i)
    if (strcmp(cards[i].name, name) == 0) return &cards[i];
  return NULL;
}

/* 读一块; 返回实际读字节 (2880=完整)。 */
static size_t moc_read_block(FILE* f, unsigned char* blk) {
  return fread(blk, 1, 2880, f);
}

/* 在一个 header (跨块, 直至 END) 上收集全部卡。
  * 返回: 0=OK 且 out_cards/out_n 有效; -1=未找到 END (坏头); -2=内存不足。 */
static int moc_read_header(FILE* f, moc_card** out_cards, int* out_n) {
  moc_card* acc = NULL;
  int acc_n = 0, acc_cap = 0;
  int block_i;
  int ended = 0;

  for (block_i = 0; block_i < 32 && !ended; ++block_i) {
    moc_card cards[36];
    int n = 0;
    unsigned char blk[2880];
    int i;
    if (moc_read_block(f, blk) != 2880) break;
    moc_parse_block(blk, cards, &n);
    for (i = 0; i < n; ++i) {
      if (acc_n >= acc_cap) {
        int newcap = acc_cap == 0 ? 64 : acc_cap * 2;
        moc_card* na = (moc_card*)realloc(acc, (size_t)newcap * sizeof(moc_card));
        if (!na) { free(acc); return -2; }
        acc = na;
        acc_cap = newcap;
      }
      acc[acc_n++] = cards[i];
      if (strcmp(cards[i].name, "END") == 0) { ended = 1; break; }
    }
  }
  if (!ended) { free(acc); return -1; }
  *out_cards = acc;
  *out_n = acc_n;
  return 0;
}

static int moc_tform_width(const char* tform) {
  char c = tform && tform[0] ? (char)toupper((unsigned char)tform[0]) : '\0';
  switch (c) {
    case 'B': return 1;
    case 'I': return 2;
    case 'J': return 4;
    case 'K': return 8;
    case 'E': return 4;
    case 'D': return 8;
    case 'A': {
      long rep = 1;
      if (tform[1] >= '0' && tform[1] <= '9') rep = strtol(tform + 1, NULL, 10);
      return (int)(rep < 1 ? 1 : rep);
    }
    default: return 0;
  }
}

/* 解析 Moc.fits; 0=OK (optional: 无 MOC/无叶级 hint 均 OK); 非 0=解析失败
 * (同样按 optional 处理, 调用方忽略)。成功且 order 匹配时填充 h->moc_tiles。 */
static int hips_parse_moc(acs_hips_handle_v1 h, char* err, size_t cap) {
  (void)err; (void)cap;
  char path[ACS_HIPS_PATH_MAX];
  FILE* f = NULL;
  moc_card* cards = NULL;
  int ncards = 0;
  int64_t bitpix = 8, naxis = 0, nax[3] = {0, 0, 0};
  int64_t data_bytes = 0, blocks = 0, k;
  int64_t mh_naxis1 = 0, mh_naxis2 = 0, mocorder = -1, tfields = 0;
  int64_t uniq_off = -1, uniq_bytes = 0;
  const moc_card* c;
  uint64_t base_uniq, npix_order;
  int64_t row;
  int rc = ACS_HIPS_OK;
  int64_t data_start = -1; /* 扩展数据区绝对偏移 (文件位置) */

  if (strlen(h->dir) + 1 + 9 + 1 > sizeof(path)) { rc = ACS_HIPS_OK; goto done; }
  memcpy(path, h->dir, strlen(h->dir));
  path[strlen(h->dir)] = '/';
  memcpy(path + strlen(h->dir) + 1, "Moc.fits", 9);
  f = fopen(path, "rb");
  if (!f) return ACS_HIPS_OK; /* 无 MOC: optional */

  /* --- primary header --- */
  rc = moc_read_header(f, &cards, &ncards);
  if (rc != 0) { rc = ACS_HIPS_OK; goto done; }
  if ((c = moc_find(cards, ncards, "BITPIX")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) bitpix = v;
  }
  if ((c = moc_find(cards, ncards, "NAXIS")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) naxis = v;
  }
  if (naxis > 3) naxis = 3;
  for (k = 0; k < naxis; ++k) {
    char nm[9];
    snprintf(nm, sizeof(nm), "NAXIS%lld", (long long)(k + 1));
    c = moc_find(cards, ncards, nm);
    if (c && c->found) { int64_t v; if (hips_parse_int(c->value, &v)) nax[k] = v; }
  }
  free(cards); cards = NULL; ncards = 0;
  /* primary 数据区跳块 (读到 header 后文件位置已是块边界); NAXIS=0 无数据区 */
  data_bytes = 0;
  if (naxis > 0) {
    data_bytes = (bitpix < 0) ? (-bitpix / 8) : (bitpix / 8);
    for (k = 0; k < naxis; ++k) data_bytes *= nax[k];
  }
  blocks = (data_bytes + 2879) / 2880;
  if (hips_fseeko(f, blocks * 2880, SEEK_CUR) != 0) { rc = ACS_HIPS_OK; goto done; }

  /* --- 扩展 header (BINTABLE) --- */
  rc = moc_read_header(f, &cards, &ncards);
  if (rc != 0) { rc = ACS_HIPS_OK; goto done; }
  if (!moc_find(cards, ncards, "XTENSION")) {
    /* 非扩展 HDU (纯 primary) → 无 MOC hint */
    rc = ACS_HIPS_OK; goto done;
  }
  c = moc_find(cards, ncards, "XTENSION");
  if (c && !hips_card_val_eq("BINTABLE", c->value)) { rc = ACS_HIPS_OK; goto done; }
  /* 数据区起点 = 当前文件位置 (读 header 已停在块边界) */
  data_start = (off_t)hips_ftello(f);
  if (data_start < 0) { rc = ACS_HIPS_OK; goto done; }

  /* --- 解析扩展头字段 --- */
  if ((c = moc_find(cards, ncards, "NAXIS1")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) mh_naxis1 = v;
  }
  if ((c = moc_find(cards, ncards, "NAXIS2")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) mh_naxis2 = v;
  }
  if ((c = moc_find(cards, ncards, "MOCORDER")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) mocorder = v;
  }
  if ((c = moc_find(cards, ncards, "TFIELDS")) && c->found) {
    int64_t v; if (hips_parse_int(c->value, &v)) tfields = v;
  }
  if (tfields < 1) tfields = 1;
  if (mocorder != (int64_t)h->order) { rc = ACS_HIPS_OK; goto done; } /* order 不符 → 无叶级 hint */
  {
    int64_t t, off = 0;
    for (t = 1; t <= tfields; ++t) {
      char nm[16], nm2[16];
      const moc_card *ct, *cf;
      snprintf(nm, sizeof(nm), "TTYPE%d", (int)t);
      snprintf(nm2, sizeof(nm2), "TFORM%d", (int)t);
      ct = moc_find(cards, ncards, nm);
      cf = moc_find(cards, ncards, nm2);
      if (!cf || !cf->found) { rc = ACS_HIPS_OK; goto done; }
      if (ct && ct->found && hips_card_val_eq("UNIQ", ct->value)) {
        int w = moc_tform_width(cf->value);
        if (w == 4 || w == 8) { uniq_off = off; uniq_bytes = w; break; }
      }
      off += moc_tform_width(cf->value);
      if (off < 0) { rc = ACS_HIPS_OK; goto done; }
    }
  }
  if (uniq_off < 0 || mh_naxis1 <= 0 || mh_naxis2 <= 0) { rc = ACS_HIPS_OK; goto done; }

  /* --- 数据区读取 (绝对定位) --- */
  base_uniq = 4ULL * (1ULL << (2ULL * (uint64_t)h->order));
  npix_order = 12ULL * (1ULL << (2ULL * (uint64_t)h->order));
  {
    size_t cap_tiles = (size_t)mh_naxis2;
    uint64_t* arr = NULL;
    int64_t n = 0;
    if (cap_tiles > 0) {
      arr = (uint64_t*)malloc(cap_tiles * sizeof(uint64_t));
      if (!arr) { rc = ACS_HIPS_ERR_NOMEM; goto done; }
    }
    for (row = 0; row < mh_naxis2; ++row) {
      unsigned char buf[8];
      uint64_t u = 0;
      int i;
      if (hips_fseeko(f, data_start + row * mh_naxis1 + uniq_off, SEEK_SET) != 0) break;
      if (fread(buf, 1, (size_t)uniq_bytes, f) != (size_t)uniq_bytes) break;
      if (uniq_bytes == 8) {
        u = 0;
        for (i = 0; i < 8; ++i) u = (u << 8) | (uint64_t)buf[i]; /* big-endian */
      } else {
        u = ((uint64_t)buf[0] << 24) | ((uint64_t)buf[1] << 16) |
            ((uint64_t)buf[2] << 8) | (uint64_t)buf[3];
      }
      if (u >= base_uniq) {
        uint64_t ipix = u - base_uniq;
        if (ipix < npix_order) arr[n++] = ipix;
      }
    }
    h->moc_tiles = arr;
    h->moc_count = n;
    if (n == 0) { free(arr); h->moc_tiles = NULL; }
  }
  rc = ACS_HIPS_OK;
done:
  free(cards);
  if (f) fclose(f);
  return rc;
}

/* 安全拼接 dir + "/" + rel 到 path (防截断; 返回 0=OK) */
static int hips_join_path(char* path, size_t path_cap,
                          const char* dir, const char* rel) {
  size_t dn = strlen(dir), rn = strlen(rel);
  if (dn + 1 + rn + 1 > path_cap) return ACS_HIPS_ERR_PARAM;
  memcpy(path, dir, dn);
  path[dn] = '/';
  memcpy(path + dn + 1, rel, rn + 1);
  return ACS_HIPS_OK;
}

/* ------------------------------------------------------------------ */
/* tile 定位 / 校验                                                    */
/* ------------------------------------------------------------------ */

/* 组装 tile 相对路径 (含 Norder{DirNpix} 子目录结构)。返回 0=OK。 */
static int hips_tile_rel(acs_hips_handle_v1 h, uint64_t ipix,
                         char* rel, size_t rel_cap, char* err, size_t cap) {
  uint64_t npix_order = 12ULL * (1ULL << (2ULL * (uint64_t)h->order));
  uint64_t D, N;
  if (ipix >= npix_order) {
    hips_set_err(err, cap, "ipix=%llu 越界 (order %d 域 [0,%llu))",
                 (unsigned long long)ipix, (int)h->order,
                 (unsigned long long)npix_order);
    return ACS_HIPS_ERR_ADDRESS;
  }
  D = ipix / 10000u;
  N = ipix % 10000u;
  snprintf(rel, rel_cap, "Norder%d/Dir%llu/Npix%llu.fits", (int)h->order,
           (unsigned long long)D, (unsigned long long)N);
  return ACS_HIPS_OK;
}

/* tile FITS 校验 (复用 fits_core 打开 + header):
 * 0=PRESENT (校验全过); 非 0: ACS_HIPS_ERR_TILE_MISSING / TILE_INVALID / ADDRESS。
 * *out_bitpix 返回文件 BITPIX (校验过; 合法时 ∈ {8,-32,-64})。 */
static int hips_tile_validate(acs_hips_handle_v1 h, uint64_t ipix,
                              int32_t* out_bitpix, char* err, size_t cap) {
  char path[ACS_HIPS_PATH_MAX];
  char rel[512];
  acs_fio_reader_v1* rd = NULL;
  acs_fio_header_v1 hdr;
  int st, i;
  int32_t bitpix;

  st = hips_tile_rel(h, ipix, rel, sizeof(rel), err, cap);
  if (st != ACS_HIPS_OK) return st;
  st = hips_join_path(path, sizeof(path), h->dir, rel);
  if (st != ACS_HIPS_OK) return st;
  if (!hips_file_exists(path)) {
    hips_set_err(err, cap, "tile 缺失: %s (不做父 order 回退)", rel);
    return ACS_HIPS_ERR_TILE_MISSING;
  }
  memset(&hdr, 0, sizeof(hdr));
  hdr.struct_size = (uint32_t)sizeof(hdr);
  hdr.abi_version = ACS_FIO_ABI_VERSION_V1;
  st = acs_fio_reader_open_v1(path, h->has_hooks ? &h->hooks : NULL, &rd, err, cap);
  if (st != ACS_FIO_OK) {
    hips_set_err(err, cap, "tile 打开失败 (rc=%d): %s", st, rel);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  st = acs_fio_get_header_v1(rd, &hdr, err, cap);
  if (st != ACS_FIO_OK) {
    acs_fio_reader_close_v1(rd);
    hips_set_err(err, cap, "tile header 读取失败 (rc=%d): %s", st, rel);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  acs_fio_reader_close_v1(rd);

  /* 布局: 2D 方阵 TW×TW */
  if (hdr.naxis != 2 || hdr.naxis_n[0] != h->tile_width ||
      hdr.naxis_n[1] != h->tile_width) {
    hips_set_err(err, cap, "tile 尺寸不符 (NAXIS=%d, %lldx%lld, 期望 %dx%d)",
                 (int)hdr.naxis, (long long)hdr.naxis_n[0],
                 (long long)hdr.naxis_n[1], (int)h->tile_width,
                 (int)h->tile_width);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  bitpix = hdr.bitpix;
  if (bitpix != ACS_FIO_BITPIX_U8 && bitpix != ACS_FIO_BITPIX_F32 &&
      bitpix != ACS_FIO_BITPIX_F64) {
    hips_set_err(err, cap, "tile BITPIX=%d 不支持 (仅 8/-32/-64)", (int)bitpix);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  /* 头卡一致性 (存在则校验): PIXTYPE/ORDERING/COORDSYS/NSIDE/FIRSTPIX/LASTPIX */
  for (i = 0; i < hdr.keyword_count && i < ACS_FIO_HEADER_MAX_CARDS; ++i) {
    const char* nm = hdr.keywords[i].name;
    const char* val = hdr.keywords[i].value;
    if (strcmp(nm, "PIXTYPE") == 0 && !hips_card_val_eq("HEALPIX", val)) {
      hips_set_err(err, cap, "tile PIXTYPE='%s' 非 HEALPIX", val);
      return ACS_HIPS_ERR_TILE_INVALID;
    }
    if (strcmp(nm, "ORDERING") == 0 && !hips_card_val_eq("NESTED", val)) {
      hips_set_err(err, cap, "tile ORDERING='%s' 非 NESTED (本合同只支持 NESTED)", val);
      return ACS_HIPS_ERR_TILE_INVALID;
    }
    if (strcmp(nm, "COORDSYS") == 0 && !hips_card_val_eq("C", val)) {
      hips_set_err(err, cap, "tile COORDSYS='%s' 非 C (equatorial)", val);
      return ACS_HIPS_ERR_TILE_INVALID;
    }
    if (strcmp(nm, "NSIDE") == 0) {
      int64_t nsv;
      if (hips_parse_int(val, &nsv) && (uint64_t)nsv != h->nside) {
        hips_set_err(err, cap, "tile NSIDE=%lld 与 order %d (nside=%llu) 不符",
                     (long long)nsv, (int)h->order,
                     (unsigned long long)h->nside);
        return ACS_HIPS_ERR_TILE_INVALID;
      }
    }
    if (strcmp(nm, "FIRSTPIX") == 0) {
      int64_t fv;
      if (hips_parse_int(val, &fv) && fv != 0) {
        hips_set_err(err, cap, "tile FIRSTPIX=%lld 非 0", (long long)fv);
        return ACS_HIPS_ERR_TILE_INVALID;
      }
    }
    if (strcmp(nm, "LASTPIX") == 0) {
      int64_t lv;
      int64_t expect = (int64_t)h->tile_width * h->tile_width - 1;
      if (hips_parse_int(val, &lv) && lv != expect) {
        hips_set_err(err, cap, "tile LASTPIX=%lld 非 %lld", (long long)lv,
                     (long long)expect);
        return ACS_HIPS_ERR_TILE_INVALID;
      }
    }
  }
  if (out_bitpix) *out_bitpix = bitpix;
  return ACS_HIPS_OK;
}

/* ------------------------------------------------------------------ */
/* 公共 API                                                            */
/* ------------------------------------------------------------------ */

int acs_hips_open_v1(const char* base_dir_utf8,
                     const char* product,
                     const acs_fio_trace_hooks_v1* hooks,
                     acs_hips_handle_v1* out,
                     char* err, size_t err_cap) {
  acs_hips_handle_v1 h;
  int st, n;
  const char* prod = product && product[0] ? product : NULL;

  if (!base_dir_utf8 || !out) return ACS_HIPS_ERR_PARAM;
  *out = NULL;
  if (hooks) {
    if (hooks->abi_version != ACS_FIO_ABI_VERSION_V1 ||
        (hooks->struct_size != 0 &&
         hooks->struct_size != (uint32_t)sizeof(acs_fio_trace_hooks_v1))) {
      hips_set_err(err, err_cap, "trace hooks abi/struct mismatch");
      return ACS_HIPS_ERR_ABI_MISMATCH;
    }
  }
  /* 科学平面 FITS-only: snr (TSV catalogue HiPS) 不在支持域 */
  if (prod && (strcmp(prod, "signal") != 0 && strcmp(prod, "support") != 0 &&
               strcmp(prod, "variance") != 0 && strcmp(prod, "ivar") != 0)) {
    hips_set_err(err, err_cap, "未知产品 '%s' (支持 signal/support/variance/ivar)",
                 prod);
    return ACS_HIPS_ERR_UNSUPPORTED;
  }
  h = (acs_hips_handle_v1)calloc(1, sizeof(*h));
  if (!h) return ACS_HIPS_ERR_NOMEM;
  st = hips_dir_join(base_dir_utf8, prod, h->dir, sizeof(h->dir), err, err_cap);
  if (st != ACS_HIPS_OK) { free(h); return st; }
  /* properties 位于 <子产品目录>/properties */
  {
    char prop_path[ACS_HIPS_PATH_MAX];
    size_t dn = strlen(h->dir);
    if (dn + 1 + 11 + 1 > sizeof(prop_path)) {
      free(h);
      hips_set_err(err, err_cap, "子产品目录路径过长");
      return ACS_HIPS_ERR_PARAM;
    }
    memcpy(prop_path, h->dir, dn);
    prop_path[dn] = '/';
    memcpy(prop_path + dn + 1, "properties", 11); /* "properties" + NUL */
    n = hips_parse_properties_file(prop_path, h->props, ACS_HIPS_PROP_MAX,
                                   err, err_cap);
  }
  if (n < 0) {
    /* parse 失败: -1 = 文件缺失/IO/超限 → PROPERTIES (缺 properties) */
    free(h);
    return ACS_HIPS_ERR_PROPERTIES;
  }
  h->prop_count = n;
  st = hips_validate_properties(h, err, err_cap);
  if (st != ACS_HIPS_OK) { free(h); return st; }
  if (hooks) {
    h->hooks = *hooks;
    h->has_hooks = 1;
  }
  (void)hips_parse_moc(h, err, err_cap); /* optional; 失败忽略 */
  *out = h;
  return ACS_HIPS_OK;
}

void acs_hips_close_v1(acs_hips_handle_v1 h) {
  if (!h) return;
  free(h->moc_tiles);
  free(h);
}

int acs_hips_props_get_v1(acs_hips_handle_v1 h, const char* key,
                          char* out, size_t out_cap,
                          char* err, size_t err_cap) {
  const char* v;
  if (!h || !key || !out || out_cap == 0) return ACS_HIPS_ERR_PARAM;
  v = hips_prop_get(h, key);
  if (!v) {
    hips_set_err(err, err_cap, "key '%s' 不存在", key);
    return ACS_HIPS_ERR_PARAM;
  }
  snprintf(out, out_cap, "%s", v);
  return ACS_HIPS_OK;
}

int acs_hips_props_serialize_v1(acs_hips_handle_v1 h,
                                char* out, size_t out_cap, size_t* out_len,
                                char* err, size_t err_cap) {
  size_t need = 0;
  int i;
  if (!h || !out_len) return ACS_HIPS_ERR_PARAM;
  for (i = 0; i < h->prop_count; ++i)
    need += strlen(h->props[i].key) + 1 + strlen(h->props[i].value) + 1;
  need += 1; /* NUL */
  *out_len = need;
  if (!out || out_cap == 0) return ACS_HIPS_OK; /* 只求长度 */
  if (out_cap < need) {
    hips_set_err(err, err_cap, "serialize 缓冲不足 (需 %zu)", need);
    return ACS_HIPS_ERR_PARAM;
  }
  {
    char* p = out;
    for (i = 0; i < h->prop_count; ++i) {
      size_t k = strlen(h->props[i].key);
      size_t v = strlen(h->props[i].value);
      memcpy(p, h->props[i].key, k); p += k;
      *p++ = '=';
      memcpy(p, h->props[i].value, v); p += v;
      *p++ = '\n';
    }
    *p = '\0';
  }
  return ACS_HIPS_OK;
}

int acs_hips_get_order_v1(acs_hips_handle_v1 h, int32_t* out_order) {
  if (!h || !out_order) return ACS_HIPS_ERR_PARAM;
  *out_order = h->order;
  return ACS_HIPS_OK;
}

int acs_hips_get_tile_width_v1(acs_hips_handle_v1 h, int32_t* out_width) {
  if (!h || !out_width) return ACS_HIPS_ERR_PARAM;
  *out_width = h->tile_width;
  return ACS_HIPS_OK;
}

int acs_hips_tile_count_v1(acs_hips_handle_v1 h, int64_t* out_count) {
  if (!h || !out_count) return ACS_HIPS_ERR_PARAM;
  *out_count = h->moc_count;
  return ACS_HIPS_OK;
}

int acs_hips_tile_ipix_v1(acs_hips_handle_v1 h, int64_t index, uint64_t* out_ipix) {
  if (!h || !out_ipix) return ACS_HIPS_ERR_PARAM;
  if (index < 0 || index >= h->moc_count) {
    return ACS_HIPS_ERR_PARAM;
  }
  *out_ipix = h->moc_tiles[index];
  return ACS_HIPS_OK;
}

int acs_hips_tile_exists_v1(acs_hips_handle_v1 h, uint64_t ipix, int* out_exists) {
  char rel[512];
  char path[ACS_HIPS_PATH_MAX];
  int st;
  if (!h || !out_exists) return ACS_HIPS_ERR_PARAM;
  st = hips_tile_rel(h, ipix, rel, sizeof(rel), NULL, 0);
  if (st != ACS_HIPS_OK) return st;
  st = hips_join_path(path, sizeof(path), h->dir, rel);
  if (st != ACS_HIPS_OK) return st;
  *out_exists = hips_file_exists(path) ? 1 : 0;
  return ACS_HIPS_OK;
}

int acs_hips_tile_status_v1(acs_hips_handle_v1 h, uint64_t ipix,
                            int32_t* out_status,
                            char* err, size_t err_cap) {
  int st;
  if (!h || !out_status) return ACS_HIPS_ERR_PARAM;
  st = hips_tile_validate(h, ipix, NULL, err, err_cap);
  if (st == ACS_HIPS_OK) {
    *out_status = ACS_HIPS_TILE_PRESENT;
    return ACS_HIPS_OK;
  }
  if (st == ACS_HIPS_ERR_TILE_MISSING) {
    *out_status = ACS_HIPS_TILE_MISSING;
    return ACS_HIPS_OK; /* MISSING 是正常状态, 非错误 */
  }
  if (st == ACS_HIPS_ERR_ADDRESS) {
    return st; /* ipix 越界 → 错误 */
  }
  *out_status = ACS_HIPS_TILE_INVALID;
  return ACS_HIPS_OK;
}

/* 读 plane 到 out (dtype 由调用方决定, 与文件 BITPIX 转换):
 * 文件 -32 → f32; -64 → f64; 8 → 提升。 */
static int hips_read_plane_impl(acs_hips_handle_v1 h, uint64_t ipix,
                                int want_f64,
                                void* out, int64_t out_elem_capacity,
                                int64_t* out_got, char* err, size_t err_cap) {
  char rel[512];
  char path[ACS_HIPS_PATH_MAX];
  int32_t bitpix = 0;
  acs_fio_reader_v1* rd = NULL;
  int64_t tw = h->tile_width;
  int64_t need = tw * tw;
  int st;
  void* scratch = NULL;
  int64_t got = 0;

  if (!h || !out || !out_got) return ACS_HIPS_ERR_PARAM;
  if (out_elem_capacity < need) {
    hips_set_err(err, err_cap, "out 容量 %lld < tile 元素数 %lld",
                 (long long)out_elem_capacity, (long long)need);
    return ACS_HIPS_ERR_PARAM;
  }
  *out_got = 0;
  st = hips_tile_validate(h, ipix, &bitpix, err, err_cap);
  if (st == ACS_HIPS_ERR_TILE_MISSING || st == ACS_HIPS_ERR_TILE_INVALID) {
    hips_set_err(err, err_cap, "tile 状态拒绝 (rc=%d): %s", st,
                 st == ACS_HIPS_ERR_TILE_MISSING ? "缺失" : "非法");
    return st;
  }
  if (st != ACS_HIPS_OK) return st;
  st = hips_tile_rel(h, ipix, rel, sizeof(rel), err, err_cap);
  if (st != ACS_HIPS_OK) return st;
  st = hips_join_path(path, sizeof(path), h->dir, rel);
  if (st != ACS_HIPS_OK) return st;

  /* 分配文件 dtype scratch (按文件 BITPIX 字节宽) */
  {
    size_t bytes = (size_t)(need * (bitpix == ACS_FIO_BITPIX_F64 ? 8 :
                                    bitpix == ACS_FIO_BITPIX_F32 ? 4 : 1));
    scratch = malloc(bytes);
    if (!scratch) return ACS_HIPS_ERR_NOMEM;
  }
  st = acs_fio_reader_open_v1(path, h->has_hooks ? &h->hooks : NULL, &rd, err, err_cap);
  if (st != ACS_FIO_OK) {
    hips_set_err(err, err_cap, "tile 打开失败 (rc=%d)", st);
    free(scratch);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  {
    int64_t got_local = 0;
    st = acs_fio_read_plane_v1(rd, 0, 0, 0, 0 /* 按文件 */, NULL,
                               scratch, need, 0 /* strict_nan=0 放行 */,
                               &got_local, h->has_hooks ? &h->hooks : NULL,
                               err, err_cap);
    acs_fio_reader_close_v1(rd);
    if (st != ACS_FIO_OK) {
      hips_set_err(err, err_cap, "tile plane 读取失败 (rc=%d)", st);
      free(scratch);
      return ACS_HIPS_ERR_TILE_INVALID;
    }
    got = got_local;
  }
  if (got != need) {
    hips_set_err(err, err_cap, "tile plane 元素数 %lld ≠ %lld", (long long)got,
                 (long long)need);
    free(scratch);
    return ACS_HIPS_ERR_TILE_INVALID;
  }
  /* dtype 转换 */
  if (bitpix == ACS_FIO_BITPIX_F32) {
    const float* src = (const float*)scratch;
    int64_t i;
    if (!want_f64) {
      memcpy(out, scratch, (size_t)need * sizeof(float));
    } else {
      double* dst = (double*)out;
      for (i = 0; i < need; ++i) dst[i] = (double)src[i];
    }
  } else if (bitpix == ACS_FIO_BITPIX_F64) {
    const double* src = (const double*)scratch;
    int64_t i;
    if (want_f64) {
      memcpy(out, scratch, (size_t)need * sizeof(double));
    } else {
      float* dst = (float*)out;
      for (i = 0; i < need; ++i) dst[i] = (float)src[i];
    }
  } else { /* u8 */
    const unsigned char* src = (const unsigned char*)scratch;
    int64_t i;
    if (!want_f64) {
      float* dst = (float*)out;
      for (i = 0; i < need; ++i) dst[i] = (float)src[i];
    } else {
      double* dst = (double*)out;
      for (i = 0; i < need; ++i) dst[i] = (double)src[i];
    }
  }
  free(scratch);
  *out_got = need;
  return ACS_HIPS_OK;
}

int acs_hips_read_tile_plane_f32_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    float* out, int64_t out_elem_capacity,
                                    int64_t* out_got,
                                    char* err, size_t err_cap) {
  return hips_read_plane_impl(h, ipix, 0, out, out_elem_capacity, out_got,
                              err, err_cap);
}

int acs_hips_read_tile_plane_f64_v1(acs_hips_handle_v1 h, uint64_t ipix,
                                    double* out, int64_t out_elem_capacity,
                                    int64_t* out_got,
                                    char* err, size_t err_cap) {
  return hips_read_plane_impl(h, ipix, 1, out, out_elem_capacity, out_got,
                              err, err_cap);
}
