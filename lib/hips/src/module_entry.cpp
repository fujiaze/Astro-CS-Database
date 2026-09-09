/* module_entry.cpp - astrocs.p1.hips_writer 模块 C ABI v1 adapter
 *
 * 迁移任务 P1-HIPS-IMPL; 对齐先例 lib/drizzle/src/module_entry.cpp
 * (P1-DRZ-IMPL, 2c065ace) 与 lib/gaia_xpsd_client/src/module_entry.cpp
 * (CAT-GAIA-IMPL, babe752d)。TU 以 C++ 编译 (aio_hips.h 为纯 C 头亦兼容),
 * 全部导出面经 extern "C" 保持 C ABI 不变; 唯一导出 astrocs_module_query_v1
 * (legacy aio_hips_* 九符号经链接 version-script/DEF 降 local, 见 CMakeLists)。
 *
 * 职责: 把 lib/astro_image_io/src/hips/aio_hips_writer.cpp 的 legacy C 接口
 *   (aio_hips.h 九导出, API-HIPS-001 冻结) 包装成九操作模块 vtable。
 *
 * 科学纪律: scientific_change=false —— 本文件只做 config/manifest 解析、
 *   base64 平面解码、线程租约注入 (host executor lease; 事务内串行, HI-3)、
 *   结果/产物转发; 不触碰产品语义 (surface brightness/support/variance 公式、
 *   MOC UNIQ、层次累加顺序 —— ALG-HIPS-001..005, 生产源零改动)。
 *
 * v1 op 词表 (types.h): write_product (叶级 tile 集 + SNR 点集 → 完整 HiPS
 *   产品树, 单事务: product_begin → 逐 tile write_*_tile → write_snr_points
 *   → set_drizzle_provenance → finalize; 生产序列与 astro_sphere_sink.cpp
 *   §2 同构)。legacy aio_hips_write (HISS 中转批量兼容通道) 不进模块 op 面,
 *   但九导出符号全量编译进 DLL 且 ABI 合同不裁剪 (仅导出面净化)。
 *
 * 线程纪律: 单事务串行 (AioHipsProductSet 句柄非线程安全, module.yaml HI-3);
 *   事务并行度由 host executor 跨 instance 并发承载。execute 期 acquire(1)
 *   硬租约 (cpu_heavy; executor 缺失/acquire 失败 → ACS_ERR_BUDGET detail
 *   105, P1-COS 同款, 禁无租约运行); 本模块不开 OMP 并行区。
 */
#define _POSIX_C_SOURCE 200809L

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define HIPS_ACCESS _access
#define HIPS_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define HIPS_ACCESS access
#define HIPS_MKDIR(p) mkdir(p, 0777)
static void hips_msleep(long ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/hips/types.h"
#include "astrocs/hips/publish.h"   /* AIO-002: 原子发布原语 (staging→校验→
                                    * fsync→原子 promote; RAII discard) */
#include "aio_hips.h"             /* legacy C API (extern "C"; 九导出) */

/* ═══════════════════ 1. 基础 helper (对齐 drizzle/gaia 先例) ═══════════════════ */

static const char kModuleId[]  = ASTROCS_HIPS_MODULE_ID;
static const char kVersion[]   = "1.0";
static const char kBuildId[]   = ASTROCS_HIPS_BUILD_ID;
static const char kSciId[]     = ASTROCS_HIPS_SCI_ID;
static const char kAlgId[]     = ASTROCS_HIPS_ALG_ID;
static const char kApiId[]     = ASTROCS_HIPS_API_ID;

static acs_str_v1 acs_str_from(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

/* 动态错误消息缓冲 (thread_local; message_utf8 借入语义要求消息存活至
 * host 读取 —— 栈缓冲跨调用返回失效, 先例 gaia/drizzle 用调用点栈缓冲,
 * 本模块统一 thread_local 静态缓冲, 跨实例并发 execute 亦安全)。 */
static thread_local char hips_err_msg[1024];

static acs_status efill(acs_error_info_v1* err, acs_status st, int32_t domain,
                        uint32_t detail, const char* msg) {
    if (!err) return st;
    memset(err, 0, sizeof(*err));
    err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err->head.abi_version = ACS_ABI_VERSION_V1;
    err->status = st;
    err->domain = domain;
    err->message_utf8 = msg;
    err->message_bytes = msg ? (uint32_t)strlen(msg) : 0;
    err->detail_code = detail;
    return st;
}

static const char* hips_msgf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(hips_err_msg, sizeof(hips_err_msg), fmt, ap);
    va_end(ap);
    return hips_err_msg;
}

/* strbuf 写 N 字节 (lifecycle_v1.h 冻结截断语义: size=所需; cap=0 且
 * data=NULL 只问尺寸; 不足 → PARAM + BUFFER_TOO_SMALL)。 */
static acs_status strbuf_write(acs_strbuf_v1* out, const char* data, uint64_t n,
                               acs_error_info_v1* err) {
    if (!out) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "hips: null output buffer");
    }
    out->size = n;
    if (!out->data || out->cap == 0) return ACS_OK;   /* 尺寸查询 */
    uint64_t room = out->cap - 1;                     /* 留 NUL 位 */
    uint64_t wr = n < room ? n : room;
    if (wr > 0) memcpy(out->data, data, (size_t)wr);
    out->data[wr] = '\0';
    if (n >= out->cap) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_BUFFER_TOO_SMALL,
                     "hips: output buffer too small");
    }
    return ACS_OK;
}

static acs_status strbuf_write_cstr(acs_strbuf_v1* out, const char* s,
                                    acs_error_info_v1* err) {
    return strbuf_write(out, s, s ? (uint64_t)strlen(s) : 0, err);
}

/* ───────── mini JSON 读取器 (config/manifest 借入; 只读不分配) ───────── */

static int json_find_str(const char* obj, const char* key,
                         const char** out_val, uint64_t* out_len) {
    if (!obj || !key) return 0;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(obj, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    p++;
    const char* start = p;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) p++;
        p++;
    }
    if (*p != '"') return 0;
    *out_val = start;
    *out_len = (uint64_t)(p - start);
    return 1;
}

static int json_copy_str(const char* obj, const char* key,
                         char* dst, uint64_t dst_cap) {
    const char* v; uint64_t n;
    if (!json_find_str(obj, key, &v, &n)) return 0;
    if (n == 0) return 0;
    uint64_t c = n < dst_cap - 1 ? n : dst_cap - 1;
    memcpy(dst, v, (size_t)c);
    dst[c] = '\0';
    return 1;
}

static double json_get_f64(const char* obj, const char* key, int* found) {
    if (found) *found = 0;
    if (!obj || !key) return 0.0;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(obj, pat);
    if (!p) return 0.0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0.0;
    char* end = NULL;
    double d = strtod(p, &end);
    if (end == p) return 0.0;
    if (found) *found = 1;
    return d;
}

static uint64_t json_get_u64(const char* obj, const char* key, int* found) {
    int f = 0;
    double d = json_get_f64(obj, key, &f);
    if (!f || d < 0.0) { if (found) *found = 0; return 0; }
    if (found) *found = 1;
    return (uint64_t)d;
}

/* "key":[u64,...] 数组 → malloc 数组 (调用方 free); 缺失 → NULL*out_n=0,
 * 语法坏 → NULL 且 *out_n=-1 */
static uint64_t* json_get_u64_array(const char* obj, const char* key, int* out_n) {
    *out_n = 0;
    if (!obj || !key) return NULL;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(obj, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '[') return NULL;
    p++;
    int count = 0; const char* q = p;
    while (*q && *q != ']') {
        if (*q >= '0' && *q <= '9') { count++; char* end; strtoull(q, &end, 10); q = end; }
        else q++;
    }
    if (*q != ']') { *out_n = -1; return NULL; }
    uint64_t* arr = (uint64_t*)malloc((size_t)(count > 0 ? count : 1) * sizeof(uint64_t));
    if (!arr) { *out_n = -1; return NULL; }
    q = p; int i = 0;
    while (*q && *q != ']' && i < count) {
        if (*q >= '0' && *q <= '9') {
            char* end; arr[i++] = strtoull(q, &end, 10); q = end;
        } else q++;
    }
    *out_n = count;
    return arr;
}

/* base64 解码 (RFC 4648 标准; 失败返回 0, 不产生部分输出) */
static int b64_decode(const char* s, uint64_t n, uint8_t** out, uint64_t* out_n) {
    *out = NULL; *out_n = 0;
    if (n == 0) return 1;                       /* 空串 = 空平面 (合法) */
    if (n % 4 != 0) return 0;
    uint64_t pad = 0;
    while (pad < n && s[n - 1 - pad] == '=') pad++;
    if (pad > 2) return 0;
    static const int8_t T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    uint64_t body = n - pad;
    uint64_t cap = (body / 4) * 3 + (pad == 1 ? 2 : (pad == 2 ? 1 : 0));
    uint8_t* buf = (uint8_t*)malloc((size_t)(cap > 0 ? cap : 1));
    if (!buf) return 0;
    uint64_t o = 0;
    uint32_t acc = 0; int bits = 0;
    for (uint64_t i = 0; i < body; ++i) {
        int8_t v = T[(uint8_t)s[i]];
        if (v < 0) { free(buf); return 0; }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; buf[o++] = (uint8_t)((acc >> bits) & 0xFF); }
    }
    if (o != cap) { free(buf); return 0; }
    *out = buf; *out_n = o;
    return 1;
}

/* "key":"<b64>" → malloc 字节 (调用方 free); 缺失 → NULL*out_n=0 */
static int json_get_b64(const char* obj, const char* key,
                        uint8_t** out, uint64_t* out_n) {
    *out = NULL; *out_n = 0;
    const char* v; uint64_t n;
    if (!json_find_str(obj, key, &v, &n)) return 0;
    return b64_decode(v, n, out, out_n);
}

static void json_append_escaped(char** w, const char* s) {
    char* p = *w;
    for (const char* q = s; *q; ++q) {
        if (*q == '"' || *q == '\\') *p++ = '\\';
        *p++ = *q;
    }
    *w = p;
}

/* ═══════════════════ 2. config (create/execute; 词表 types.h) ═══════════════════ */

typedef struct {
    char op[32];
    char out_dir[1024];
    uint32_t nside;
    uint32_t tile_width;
    int32_t data_type;
    int32_t flags;
    char creator_did[256];
    char obs_title[256];
    char obs_filter[256];
    char obs_date[64];
    double exposure_s;
    uint32_t moc_order;
    uint32_t max_workers;      /* 0 = 无 config 上限 (事务恒串行, 仅作回显) */
} hips_cfg;

static void hips_cfg_free(hips_cfg* c) {
    (void)c;   /* POD; 字符串定长, 无堆所有权 */
}

static uint32_t hips_ilog2_u32(uint32_t n) {
    uint32_t r = 0;
    while ((1u << r) < n) r++;
    return r;
}

static acs_status hips_cfg_parse(const char* json, hips_cfg* c,
                                 acs_error_info_v1* err) {
    memset(c, 0, sizeof(*c));
    if (!json || !json[0]) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CONFIG, "hips: empty config");
    }
    if (!json_copy_str(json, HIPS_CFG_KEY_OP, c->op, sizeof(c->op))) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_CONFIG_SCHEMA, "hips: config missing op");
    }
    if (strcmp(c->op, ASTROCS_HIPS_OP_WRITE_PRODUCT) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_UNKNOWN_OP, "hips: unknown op");
    }
    if (!json_copy_str(json, HIPS_CFG_KEY_OUT_DIR, c->out_dir, sizeof(c->out_dir)) ||
        !c->out_dir[0]) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_MISSING_FIELD, "hips: config missing out_dir");
    }
    int f = 0;
    uint64_t nside = json_get_u64(json, HIPS_CFG_KEY_NSIDE, &f);
    if (!f || nside < 512 || nside > (1ull << 24) || (nside & (nside - 1)) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_BAD_VALUE,
                     "hips: nside invalid (must be power of 2, >=512)");
    }
    c->nside = (uint32_t)nside;
    uint64_t tw = json_get_u64(json, HIPS_CFG_KEY_TILE_WIDTH, &f);
    if (!f || tw != 512) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_BAD_VALUE, "hips: tile_width != 512");
    }
    c->tile_width = (uint32_t)tw;
    int dt = (int)json_get_f64(json, HIPS_CFG_KEY_DATA_TYPE, &f);
    if (!f || (dt != AIO_HIPS_FLOAT32 && dt != AIO_HIPS_FLOAT64)) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_BAD_VALUE, "hips: data_type invalid");
    }
    c->data_type = dt;
    int fl = (int)json_get_f64(json, HIPS_CFG_KEY_FLAGS, &f);
    const int32_t allowed = AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                            AIO_HIPS_PRODUCT_SNR | AIO_HIPS_PRODUCT_VARIANCE |
                            AIO_HIPS_PRODUCT_IVAR;
    if (!f || fl <= 0 || (fl & ~allowed) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_BAD_VALUE, "hips: flags invalid");
    }
    c->flags = fl;
    /* 可选键 (缺省与 legacy product_begin NULL 语义对齐) */
    json_copy_str(json, HIPS_CFG_KEY_CREATOR_DID, c->creator_did,
                  sizeof(c->creator_did));
    if (!c->creator_did[0])
        snprintf(c->creator_did, sizeof(c->creator_did), "ivo://astrocs/phase1");
    json_copy_str(json, HIPS_CFG_KEY_OBS_TITLE, c->obs_title, sizeof(c->obs_title));
    if (!c->obs_title[0])
        snprintf(c->obs_title, sizeof(c->obs_title), "AstroCS Phase1");
    json_copy_str(json, HIPS_CFG_KEY_OBS_FILTER, c->obs_filter, sizeof(c->obs_filter));
    json_copy_str(json, HIPS_CFG_KEY_OBS_DATE, c->obs_date, sizeof(c->obs_date));
    c->exposure_s = json_get_f64(json, HIPS_CFG_KEY_EXPOSURE_S, &f);
    if (!f) c->exposure_s = 0.0;
    if (!(c->exposure_s >= 0.0)) {   /* NaN/负 → 拒绝 */
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     HIPS_ECODE_BAD_VALUE, "hips: exposure_s invalid");
    }
    c->moc_order = (uint32_t)json_get_u64(json, HIPS_CFG_KEY_MOC_ORDER, &f);
    if (!f) c->moc_order = 0;
    c->max_workers = (uint32_t)json_get_u64(json, HIPS_CFG_KEY_MAX_WORKERS, &f);
    if (!f) c->max_workers = 0;
    return ACS_OK;
}

/* ═══════════════════ 3. manifest 解码 (execute 输入; 顶层平铺 v1) ═══════════════════ */

typedef struct {
    uint64_t* parent_ipix;     /* n_tiles × u64 (NESTED 叶级 tile 父单元) */
    uint64_t n_tiles;
    uint8_t* flux;             /* n_tiles·W²·dtype (native 字节序) */
    uint8_t* coverage;
    uint8_t* valid_mask;       /* 可选; n_tiles·W² (has_mask 位图非全 0 时必填) */
    uint8_t* var_num;          /* 可选; n_tiles·W²·dtype */
    int has_mask;
    int has_var;
    /* SNR 点集 (Catalogue HiPS; 可空) */
    AioHipsSnrPoint* snr;
    uint64_t n_snr;
    /* drizzle provenance (可选) */
    double prov_pixfrac;
    double prov_scale;
    int has_prov;
} hips_rows;

static void hips_rows_free(hips_rows* r) {
    if (!r) return;
    free(r->parent_ipix); free(r->flux); free(r->coverage);
    free(r->valid_mask); free(r->var_num); free(r->snr);
    memset(r, 0, sizeof(*r));
}

/* 位图 has_x_base64: 任一非 0 → 需要 x 平面 (per-tile 可选性) */
static int hips_bitmap_any(const char* man, const char* key, uint64_t n_tiles) {
    uint8_t* b = NULL; uint64_t bn = 0;
    if (!json_get_b64(man, key, &b, &bn)) return 0;
    int any = 0;
    if (b && bn == n_tiles && n_tiles > 0) {
        for (uint64_t i = 0; i < n_tiles; ++i) any |= (b[i] != 0);
    }
    free(b);
    return any;
}

static acs_status hips_rows_parse(const char* manifest, const hips_cfg* c,
                                  hips_rows* r, acs_error_info_v1* err) {
    memset(r, 0, sizeof(*r));
    int f = 0;
    uint64_t n_tiles = json_get_u64(manifest, HIPS_M_KEY_N_TILES, &f);
    if (!f || n_tiles > 65535ull) {
        hips_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     HIPS_ECODE_MANIFEST_FIELD, "hips: manifest missing n_tiles");
    }
    r->n_tiles = n_tiles;
    uint64_t leaf_order = json_get_u64(manifest, HIPS_M_KEY_LEAF_ORDER, &f);
    if (!f || leaf_order != hips_ilog2_u32(c->nside)) {
        hips_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     HIPS_ECODE_MANIFEST_FIELD,
                     "hips: manifest leaf_order != log2(nside)");
    }
    uint64_t width = json_get_u64(manifest, HIPS_M_KEY_WIDTH, &f);
    if (!f || width != c->tile_width) {
        hips_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     HIPS_ECODE_MANIFEST_FIELD,
                     "hips: manifest width != tile_width");
    }
    const uint64_t tile_elems = width * width;
    const uint64_t dtype_sz = (c->data_type == AIO_HIPS_FLOAT64) ? 8 : 4;

    /* parent_ipix: JSON 数组 (u64 整数; strtoull 精确) */
    int an = 0;
    r->parent_ipix = json_get_u64_array(manifest, HIPS_M_KEY_PARENT, &an);
    if (!r->parent_ipix || an < 0 || (uint64_t)an != n_tiles) {
        hips_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     HIPS_ECODE_MANIFEST_FIELD,
                     "hips: manifest parent_ipix missing/length mismatch");
    }

    /* 必填平面: flux / coverage (拼接流, 每 tile W² 元素, native 字节序) */
    struct { const char* key; uint8_t** dst; uint64_t elem; } planes[] = {
        { HIPS_M_KEY_FLUX_B64,     &r->flux,     dtype_sz },
        { HIPS_M_KEY_COVERAGE_B64, &r->coverage, dtype_sz },
    };
    for (int i = 0; i < 2; ++i) {
        uint64_t ln = 0;
        if (!json_get_b64(manifest, planes[i].key, planes[i].dst, &ln)) {
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_DATA_DECODE,
                         "hips: manifest base64 decode failed");
        }
        uint64_t want = n_tiles * tile_elems * planes[i].elem;
        if (!*planes[i].dst || ln != want) {
            char m[160];
            snprintf(m, sizeof(m),
                     "hips: manifest plane size mismatch (%s): got %llu want %llu",
                     planes[i].key, (unsigned long long)ln,
                     (unsigned long long)want);
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_MANIFEST_FIELD, m);
        }
    }

    /* 可选: valid_mask / var_num (per-tile 位图非全 0 时必填) */
    r->has_mask = hips_bitmap_any(manifest, HIPS_M_KEY_HAS_MASK_B64, n_tiles);
    if (r->has_mask) {
        uint64_t ln = 0;
        if (!json_get_b64(manifest, HIPS_M_KEY_MASK_B64, &r->valid_mask, &ln) ||
            ln != n_tiles * tile_elems) {
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_MANIFEST_FIELD,
                         "hips: manifest valid_mask missing/mismatch");
        }
    }
    r->has_var = hips_bitmap_any(manifest, HIPS_M_KEY_HAS_VAR_B64, n_tiles);
    if (r->has_var) {
        uint64_t ln = 0;
        if (!json_get_b64(manifest, HIPS_M_KEY_VARNUM_B64, &r->var_num, &ln) ||
            ln != n_tiles * tile_elems * dtype_sz) {
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_MANIFEST_FIELD,
                         "hips: manifest var_num missing/mismatch");
        }
    }

    /* SNR 点集 (可空; 六平面 SoA → AoS 组装) */
    uint64_t m = json_get_u64(manifest, HIPS_M_KEY_SNR_COUNT, &f);
    if (!f) m = 0;
    if (m > 0) {
        if (m > 100000000ull) {
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_MANIFEST_FIELD, "hips: snr_count too large");
        }
        uint8_t* ra = NULL; uint8_t* dec = NULL; uint8_t* sv = NULL;
        uint8_t* sid = NULL; uint8_t* qf = NULL; uint8_t* st = NULL;
        uint64_t lra = 0, ldec = 0, lsv = 0, lsid = 0, lqf = 0, lst = 0;
        int ok =
            json_get_b64(manifest, HIPS_M_KEY_SNR_RA_B64,  &ra,  &lra)  && ra  && lra == m * 8 &&
            json_get_b64(manifest, HIPS_M_KEY_SNR_DEC_B64, &dec, &ldec) && dec && ldec == m * 8 &&
            json_get_b64(manifest, HIPS_M_KEY_SNR_VAL_B64, &sv,  &lsv)  && sv  && lsv == m * 8 &&
            json_get_b64(manifest, HIPS_M_KEY_SNR_ID_B64,  &sid, &lsid) && sid && lsid == m * 8 &&
            json_get_b64(manifest, HIPS_M_KEY_SNR_QF_B64,  &qf,  &lqf)  && qf  && lqf == m * 4 &&
            json_get_b64(manifest, HIPS_M_KEY_SNR_ST_B64,  &st,  &lst)  && st  && lst == m * 4;
        if (!ok) {
            free(ra); free(dec); free(sv); free(sid); free(qf); free(st);
            hips_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         HIPS_ECODE_MANIFEST_FIELD,
                         "hips: manifest snr planes missing/mismatch");
        }
        r->snr = (AioHipsSnrPoint*)malloc((size_t)m * sizeof(AioHipsSnrPoint));
        if (!r->snr) {
            free(ra); free(dec); free(sv); free(sid); free(qf); free(st);
            hips_rows_free(r);
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE, "hips: snr alloc failed");
        }
        for (uint64_t i = 0; i < m; ++i) {
            memcpy(&r->snr[i].ra_deg,  &ra[i * 8], 8);
            memcpy(&r->snr[i].dec_deg, &dec[i * 8], 8);
            memcpy(&r->snr[i].snr,     &sv[i * 8], 8);
            memcpy(&r->snr[i].star_id, &sid[i * 8], 8);
            memcpy(&r->snr[i].quality_flags,      &qf[i * 4], 4);
            memcpy(&r->snr[i].photometric_status, &st[i * 4], 4);
        }
        r->n_snr = m;
        free(ra); free(dec); free(sv); free(sid); free(qf); free(st);
    }

    /* drizzle provenance (可选; 双键齐才生效) */
    r->prov_pixfrac = json_get_f64(manifest, HIPS_M_KEY_PROV_PIXFRAC, &f);
    int f2 = 0;
    r->prov_scale = json_get_f64(manifest, HIPS_M_KEY_PROV_SCALE, &f2);
    r->has_prov = (f && f2) ? 1 : 0;
    return ACS_OK;
}

/* ═══════════════════ 4. describe / validate_config / plan ═══════════════════ */

static acs_status hips_describe(const acs_module_api_v1* self,
                                acs_str_v1 module_id,
                                acs_module_descriptor_v1* out_desc) {
    (void)self;
    if (!out_desc) return ACS_ERR_PARAM;
    if (module_id.size != strlen(kModuleId) ||
        memcmp(module_id.data, kModuleId, strlen(kModuleId)) != 0)
        return ACS_ERR_ABI_MISMATCH;
    memset(out_desc, 0, sizeof(*out_desc));
    out_desc->head.struct_size = (uint32_t)sizeof(acs_module_descriptor_v1);
    out_desc->head.abi_version = ACS_ABI_VERSION_V1;
    out_desc->module_id = acs_str_from(kModuleId);
    out_desc->version   = acs_str_from(kVersion);
    out_desc->build_id  = acs_str_from(kBuildId);
    out_desc->sci_id    = acs_str_from(kSciId);
    out_desc->alg_id    = acs_str_from(kAlgId);
    out_desc->api_id    = acs_str_from(kApiId);
    out_desc->phase = 1;              /* module.yaml phase_scope: phase1 */
    out_desc->config_schema_ver = ASTROCS_HIPS_CONFIG_SCHEMA_VER;
    out_desc->execution_class = 0;    /* cpu_heavy (module.yaml resource_class) */
    out_desc->parallel_ok = 0;        /* 单事务串行 (HI-3); 并行度=跨 instance */
    out_desc->flags = 0;
    return ACS_OK;
}

static acs_status hips_validate_config(const acs_module_api_v1* self,
                                       acs_str_v1 config_json,
                                       acs_error_info_v1* err) {
    (void)self;
    hips_cfg tmp;
    acs_status st = hips_cfg_parse(config_json.data, &tmp, err);
    hips_cfg_free(&tmp);
    return st;
}

/* plan (真实 work_units; 元数据推导, 禁空转):
 * work_units = 1 (单 execute = 单 HiPS 事务; 多事务扇出由 orchestrator 编排)。
 * 事务内串行 (product set 句柄非线程安全, HI-3) → max_workers=1;
 * 事务级并行 = host executor 跨 instance 并发。
 * memory 估算 (config 元数据推导, 无魔数):
 *   hierarchy_accumulator_bytes ≈ 12·(nside/2)²·8B·2 (祖先累加 f64
 *     signal+coverage, 全覆盖上界; ALG-HIPS-003 层次归并)
 *   io_out_bytes ≈ nside²·dtype_bytes·n_products (FITS 主产物近似) */

static acs_status hips_plan(const acs_module_api_v1* self,
                            acs_str_v1 node_id,
                            acs_str_v1 config_json,
                            acs_strbuf_v1* out_plan_json,
                            acs_error_info_v1* err) {
    (void)self;
    if (!out_plan_json) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "hips: null plan buffer");
    }
    hips_cfg c;
    acs_status st = hips_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) return st;

    uint64_t n_pix_hp = 12ull * (uint64_t)c.nside * (uint64_t)c.nside;
    uint64_t hier_acc = 12ull * ((uint64_t)c.nside / 2ull) * ((uint64_t)c.nside / 2ull)
                        * 8ull * 2ull;             /* f64 signal+coverage 祖先累加 */
    int n_products = 0;
    {
        int32_t fl = c.flags;
        while (fl) { n_products += (fl & 1); fl >>= 1; }   /* popcount(产品位) */
    }
    uint64_t dtype_sz = (c.data_type == AIO_HIPS_FLOAT64) ? 8 : 4;
    uint64_t io_out = (uint64_t)c.nside * (uint64_t)c.nside * dtype_sz
                      * (uint64_t)n_products;

    char node[256];
    uint64_t node_len = node_id.size < sizeof(node) - 1 ? node_id.size
                                                        : sizeof(node) - 1;
    if (node_id.data && node_len) memcpy(node, node_id.data, (size_t)node_len);
    node[node_len] = '\0';

    char buf[1280];
    char* w = buf;
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
        "{\"plan_version\":%u,\"module_id\":\"%s\",\"node_id\":\"",
        ASTROCS_HIPS_PLAN_VERSION, kModuleId);
    json_append_escaped(&w, node);
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
        "\",\"op\":\"%s\",\"work_units\":1,"
        "\"work_units_basis\":\"single_transaction_execute\","
        "\"parallel_axis\":\"instance\","
        "\"min_workers\":1,\"max_workers\":1,"
        "\"cancel_support\":\"entry\","
        "\"determinism\":\"fixed_reduction_order\","
        "\"nside\":%u,\"tile_width\":%u,\"data_type\":%d,\"flags\":%d,"
        "\"n_products\":%d,\"n_healpix_pixels\":%llu,"
        "\"memory_hierarchy_accumulator_bytes\":%llu,"
        "\"io_out_estimate_bytes\":%llu}",
        c.op, (unsigned)c.nside, (unsigned)c.tile_width, (int)c.data_type,
        (int)c.flags, n_products, (unsigned long long)n_pix_hp,
        (unsigned long long)hier_acc, (unsigned long long)io_out);
    hips_cfg_free(&c);
    return strbuf_write_cstr(out_plan_json, buf, err);
}

/* ═══════════════════ 5. create / destroy / inspect / cancel ═══════════════════ */

typedef struct {
    uint32_t state;                /* ACS_LC_STATE_* */
    const acs_host_api_v1* host;
    hips_cfg cfg;                  /* create 期校验副本 (execute 期以传入 config 重解析) */
    int executing;
    int cancel_req;
    uint64_t exec_count;
    acs_status last_status;
    char last_op[32];
    int legacy_last_code;
    uint32_t last_workers;
    uint64_t last_n_tiles;
    uint64_t last_n_written;
    uint64_t last_n_var;
    uint64_t last_n_snr;
    int last_prov_set;
} hips_inst;

static acs_status hips_create(const acs_module_api_v1* self,
                              acs_str_v1 config_json,
                              const acs_host_api_v1* host,
                              acs_module_instance_v1** out,
                              acs_error_info_v1* err) {
    (void)self;
    if (!out) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "hips: null out");
    }
    *out = NULL;
    if (!host || !host->allocator) {
        return efill(err, ACS_ERR_ABI_MISMATCH, ACS_ERR_DOMAIN_INTERNAL,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "hips: host allocator required");
    }
    hips_cfg c;
    acs_status st = hips_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) { hips_cfg_free(&c); return st; }

    hips_inst* inst = (hips_inst*)calloc(1, sizeof(hips_inst));
    if (!inst) {
        hips_cfg_free(&c);
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "hips: inst alloc failed");
    }
    inst->state = ACS_LC_STATE_CREATED;
    inst->host = host;
    inst->cfg = c;
    inst->last_status = ACS_OK;
    inst->legacy_last_code = 0;
    *out = (acs_module_instance_v1*)inst;
    return ACS_OK;
}

static acs_status hips_inspect(const acs_module_instance_v1* inst_raw,
                               acs_strbuf_v1* out_json,
                               acs_error_info_v1* err) {
    const hips_inst* inst = (const hips_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"module_id\":\"%s\",\"state\":%u,\"executing\":%d,"
             "\"cancel_req\":%d,\"exec_count\":%llu,\"last_status\":%d,"
             "\"last_op\":\"%s\",\"last_workers\":%u,"
             "\"legacy_last_code\":%d,"
             "\"last_n_tiles\":%llu,\"last_n_tiles_written\":%llu,"
             "\"last_n_tiles_variance\":%llu,\"last_n_snr_points\":%llu,"
             "\"drizzle_provenance_set\":%d,"
             "\"cancel_support\":\"entry\","
             "\"threads_note\":\"single transaction serial (HI-3); "
             "host executor lease x instance\"}",
             kModuleId, (unsigned)inst->state, inst->executing,
             inst->cancel_req, (unsigned long long)inst->exec_count,
             (int)inst->last_status, inst->last_op,
             (unsigned)inst->last_workers, inst->legacy_last_code,
             (unsigned long long)inst->last_n_tiles,
             (unsigned long long)inst->last_n_written,
             (unsigned long long)inst->last_n_var,
             (unsigned long long)inst->last_n_snr,
             inst->last_prov_set);
    return strbuf_write_cstr(out_json, buf, err);
}

static acs_status hips_request_cancel(acs_module_instance_v1* inst_raw) {
    hips_inst* inst = (hips_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state == ACS_LC_STATE_DESTROYED) return ACS_ERR_STATE;
    inst->cancel_req = 1;              /* 幂等单向置位 (ABI-002) */
    return ACS_OK;
}

static void hips_destroy(acs_module_instance_v1* inst_raw) {
    hips_inst* inst = (hips_inst*)inst_raw;
    if (!inst) return;                 /* inst=NULL 空操作 */
    hips_cfg_free(&inst->cfg);
    inst->state = ACS_LC_STATE_DESTROYED;
    free(inst);
}

/* ═══════════════════ 6. execute — write_product (单事务) ═══════════════════ */

/* legacy 错误码 → (ACS_ERR, domain) 映射 (代码原样进 message "legacy_code=%d",
 * 不重解释科学语义; DISP-HIPS-007 错误码无集中枚举为登记缺陷, 不消化) */
static acs_status hips_legacy_status(int code, int32_t* domain) {
    if (code == 0) { *domain = ACS_ERR_DOMAIN_IO; return ACS_OK; }
    /* writer 无集中错误码枚举: 按函数族签名归类 (begin NULL/写盘负值);
     * 域判定: 参数类 (null/视图不匹配/值域) → PARAM+SCIENCE_PRECONDITION,
     * 写盘类 (FITS/文件) → PARAM+DATA|IO。保守统一 PARAM + 域按符号位:
     * writer 全部失败路径均为负 int 或 NULL, 无 errno 转发 → 映射 PARAM,
     * 域=SCIENCE_PRECONDITION (科学前置失败), 细节看 message。 */
    (void)domain;
    return ACS_ERR_PARAM;
}

/* cancel 检查点 helper (tile 间安全点与 finalize 前共用) */
static int hips_cancel_requested(const hips_inst* inst) {
    if (inst->cancel_req) return 1;
    if (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled &&
        inst->host->cancel->is_cancelled(inst->host->cancel->user_data))
        return 1;
    return 0;
}

/* 事务 sink 产物存在性计数 (输出 manifest artifacts; DRZ 同款手法) */
static int hips_artifact_exists(const char* dir, const char* sub) {
    if (!dir || !dir[0]) return 0;
    char p[1200];
    snprintf(p, sizeof(p), "%s/%s", dir, sub);
    return HIPS_ACCESS(p, F_OK) == 0;
}

static acs_status hips_execute_write_product(
    hips_inst* inst, const char* manifest, const hips_cfg* c,
    acs_strbuf_v1* out_manifest_json, acs_error_info_v1* err) {
    if (!out_manifest_json) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "hips: null output buffer");
    }

    /* 检查次序 (P1-COS-IMPL 同款): entry cancel 预检 → 硬租约 acquire →
     * rows 解析 → begin; 未下盘阶段失败 = 事务性干净, 不触数据平面。
     * inst->cancel_req (request_cancel 已置位) → STATE (非法再执行);
     * host cancel 信号 → CANCELLED (事务化中止)。 */
    if (inst->cancel_req) {
        inst->last_status = ACS_ERR_STATE;
        return efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
                     ACS_DIAG_ECODE_ILLEGAL_STATE,
                     "hips: instance cancel requested (no execute)");
    }
    if (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled &&
        inst->host->cancel->is_cancelled(inst->host->cancel->user_data)) {
        inst->last_status = ACS_ERR_CANCELLED;
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     ACS_DIAG_ECODE_NONE, "hips: cancel requested before execute");
    }
    /* host executor 租借 (FORBID-003 禁私建线程池)。事务内串行 (HI-3) →
     * 恒 acquire(1) 占坑; cpu_heavy 硬租约: executor 缺失/acquire 失败 →
     * ACS_ERR_BUDGET detail 105 (P1-COS 同款, 禁无租约运行)。 */
    if (!inst->host || !inst->host->executor) {
        return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE, 105,
                     "cpu_heavy module requires host executor");
    }
    const acs_executor_v1* ex = inst->host->executor;
    if (!ex->acquire || !ex->release ||
        ex->acquire(ex->user_data, 1) != 0) {
        return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE, 105,
                     "executor lease unavailable for cpu_heavy op");
    }
    uint32_t leased = 1;

    hips_rows rows;
    acs_status st = hips_rows_parse(manifest, c, &rows, err);
    if (st != ACS_OK) {
        ex->release(ex->user_data, leased);
        return st;
    }

    /* ── AIO-002 原子发布: staging 目录建立 (兄弟目录, kill 残留自愈) ──
     * out_dir 语义 = 发布目标根; writer 全量写入 staging, finalize 后
     * fsync 树 + 原子 rename。任一失败/取消 → discard staging → out_dir
     * 根无 partial (全有或全无; DISP-HIPS-001/004 事务化收口)。 */
    char stage_path[1100];
    int prc = aio_publish_stage_create_v1(c->out_dir, stage_path,
                                          sizeof(stage_path));
    if (prc != AIO_PUBLISH_OK) {
        ex->release(ex->user_data, leased);
        hips_rows_free(&rows);
        inst->last_status = ACS_ERR_IO;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_HIPS_OP_WRITE_PRODUCT);
        return efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
                     HIPS_ECODE_PUBLISH_STAGE,
                     hips_msgf("hips: staging create failed (%d)", prc));
    }

    /* kill 中断测试锚点 (注入名见 publish.h; 正常运行零行为差异) */
    if (hips_publish_fault_slow_write_v1()) {
#ifdef _WIN32
        Sleep(600);
#else
        hips_msleep(600);
#endif
    }

    /* 1) product_begin (写目录 = staging; 参数 create/execute config 固化;
     *    失败 NULL+last_error) */
    AioHipsProductSet* ps = aio_hips_product_begin(
        stage_path, (int)c->nside, (int)c->tile_width, c->data_type, c->flags,
        c->creator_did,
        c->obs_title[0] ? c->obs_title : NULL,
        c->obs_filter[0] ? c->obs_filter : NULL,
        c->exposure_s,
        c->obs_date[0] ? c->obs_date : NULL,
        (int)c->moc_order);
    if (!ps) {
        const char* le = aio_hips_last_error();
        aio_publish_stage_discard_v1(c->out_dir);   /* 空 staging 收口 */
        ex->release(ex->user_data, leased);
        hips_rows_free(&rows);
        inst->last_status = ACS_ERR_IO;
        inst->legacy_last_code = -1;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_HIPS_OP_WRITE_PRODUCT);
        return efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO, HIPS_ECODE_BEGIN_REJECT,
                     hips_msgf("hips: product_begin failed: %s",
                               le && le[0] ? le : "(no detail)"));
    }

    /* 2) 逐 tile signal/support (+variance; -5/-2 = 无有效方差跳过不中止,
     *    生产语义 astro_sphere_sink.cpp:127-133) */
    const uint64_t tile_elems = (uint64_t)c->tile_width * (uint64_t)c->tile_width;
    const uint64_t dtype_sz = (c->data_type == AIO_HIPS_FLOAT64) ? 8 : 4;
    const uint32_t leaf_order = hips_ilog2_u32(c->nside);
    uint64_t n_written = 0, n_var = 0;
    int cancelled_mid = 0;
    int fail_rc = 0;
    const char* fail_stage = NULL;
    for (uint64_t i = 0; i < rows.n_tiles; ++i) {
        AstroSphereTileView v;
        memset(&v, 0, sizeof(v));
        v.parent_ipix = rows.parent_ipix[i];
        v.leaf_order = leaf_order;
        v.width = (uint32_t)c->tile_width;
        v.data_type = c->data_type;
        v.flux_sum = rows.flux ? (const void*)(rows.flux + i * tile_elems * dtype_sz) : NULL;
        v.covered_area = rows.coverage
            ? (const void*)(rows.coverage + i * tile_elems * dtype_sz) : NULL;
        v.valid_mask = rows.has_mask && rows.valid_mask
            ? (const uint8_t*)(rows.valid_mask + i * tile_elems) : NULL;
        v.var_num_sum = rows.has_var && rows.var_num
            ? (const void*)(rows.var_num + i * tile_elems * dtype_sz) : NULL;
        int rc = aio_hips_write_signal_support_tile(ps, &v);
        if (rc != 0) { fail_rc = rc; fail_stage = "signal_support_tile"; break; }
        n_written++;
        if (rows.has_var) {
            rc = aio_hips_write_variance_tile(ps, &v);
            if (rc == -5 || rc == -2) {
                /* 该 tile 无有效方差数据 → 跳过不中止 (生产同款) */
            } else if (rc != 0) {
                fail_rc = rc; fail_stage = "variance_tile"; break;
            } else {
                n_var++;
            }
        }
        /* cancel 安全点 (tile 间; abort 事务, staging 整树确定性丢弃) */
        if (hips_cancel_requested(inst)) {
            cancelled_mid = 1;
            break;
        }
    }

    /* 3) SNR 点集 (非空且未取消) */
    if (!fail_rc && !cancelled_mid && rows.n_snr > 0) {
        int rc = aio_hips_write_snr_points(ps, rows.snr, (int)rows.n_snr);
        if (rc != 0) { fail_rc = rc; fail_stage = "snr_points"; }
    }

    /* 4) drizzle provenance (可选; 生产 sink 总是调, 值域 legacy 校验 rc 2) */
    if (!fail_rc && !cancelled_mid && rows.has_prov) {
        int rc = aio_hips_set_drizzle_provenance(ps, rows.prov_pixfrac,
                                                 rows.prov_scale);
        if (rc != 0) { fail_rc = rc; fail_stage = "drizzle_provenance"; }
        else inst->last_prov_set = 1;
    }

    /* 5) finalize (cancel 请求 → abort 事务而非 finalize)
     * legacy 所有权: finalize 成功 = 句柄已在内部释放 (README §5; 成功路径
     * finalize 内 delete ps) → finalized_ok 后 ps 悬空禁触碰; 失败/取消
     * 路径才允许 abort 释放。 */
    int finalized_ok = 0;
    if (!fail_rc && !cancelled_mid) {
        if (hips_cancel_requested(inst)) {
            cancelled_mid = 1;
        } else {
            int rc = aio_hips_finalize(ps);
            if (rc != 0) { fail_rc = rc; fail_stage = "finalize"; }
            else finalized_ok = 1;
        }
    }

    /* ── AIO-002 发布门 ──
     * 失败/取消: abort + discard staging → out_dir 根无 partial。
     * 成功: fsync staging 树 (ENOSPC 暴露点) → 原子 rename promote
     * (全有或全无) → 输出 manifest 报告 out_dir。publish 原语失败
     * (fsync/promote) 亦 discard → 无成功对象。 */
    int publish_fail = 0;
    int publish_rc = AIO_PUBLISH_OK;
    if (!fail_rc && !cancelled_mid) {
        publish_rc = aio_publish_tree_fsync_v1(stage_path, NULL, NULL);
        if (publish_rc != AIO_PUBLISH_OK) {
            publish_fail = 1;
            fail_stage = "publish_fsync";
        } else {
            publish_rc = aio_publish_promote_v1(c->out_dir, stage_path);
            if (publish_rc != AIO_PUBLISH_OK) {
                publish_fail = 1;
                fail_stage = "publish_promote";
            }
        }
        if (publish_fail)
            aio_publish_stage_discard_v1(c->out_dir);
    } else {
        aio_hips_abort(ps);                    /* 未 finalize → 句柄有效 */
        aio_publish_stage_discard_v1(c->out_dir);
    }

    if (fail_rc || cancelled_mid || publish_fail) {
        ex->release(ex->user_data, leased);
        hips_rows_free(&rows);
        inst->executing = 0;
        inst->legacy_last_code = fail_rc;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_HIPS_OP_WRITE_PRODUCT);
        inst->last_workers = leased;
        if (cancelled_mid) {
            inst->last_status = ACS_ERR_CANCELLED;
            return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                         ACS_DIAG_ECODE_NONE,
                         "hips: cancel requested mid-transaction "
                         "(staging discarded; no partial tree)");
        }
        if (publish_fail) {
            inst->last_status = ACS_ERR_IO;
            return efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
                         HIPS_ECODE_PUBLISH_REJECT,
                         hips_msgf("hips: publish failed at %s (rc=%d); "
                                   "staging discarded, no partial tree",
                                   fail_stage ? fail_stage : "?", publish_rc));
        }
        const char* le = aio_hips_last_error();
        int32_t dom = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
        acs_status cst = hips_legacy_status(fail_rc, &dom);
        inst->last_status = cst;
        return efill(err, cst, dom, HIPS_ECODE_LEGACY_REJECT,
                     hips_msgf("hips: legacy_code=%d at %s: %s", fail_rc,
                               fail_stage ? fail_stage : "?",
                               le && le[0] ? le : "(no detail)"));
    }

    ex->release(ex->user_data, leased);

    /* 统计回填先于 rows 释放 (rows.n_* 在输出 manifest 中还要使用) */
    inst->exec_count++;
    inst->legacy_last_code = 0;
    inst->last_workers = leased;
    inst->last_n_tiles = rows.n_tiles;
    inst->last_n_written = n_written;
    inst->last_n_var = n_var;
    inst->last_n_snr = rows.n_snr;
    hips_rows_free(&rows);
    snprintf(inst->last_op, sizeof(inst->last_op), "%s",
             ASTROCS_HIPS_OP_WRITE_PRODUCT);

    /* 输出 manifest (统计面 + 事务 sink 产物 URI 存在性探测) */
    uint64_t total = 384u;
    total += strlen(c->out_dir) + 64u;
    static const char* const kSubs[] = {
        "signal", "support", "snr", "variance", "ivar"
    };
    for (int i = 0; i < 5; ++i)
        if (hips_artifact_exists(c->out_dir, kSubs[i]))
            total += strlen(c->out_dir) + 32u;

    char* buf = (char*)malloc((size_t)total + 1);
    if (!buf) {
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "hips: out manifest alloc failed");
    }
    char* w = buf;
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
        "{\"op\":\"%s\",\"out_dir\":\"", ASTROCS_HIPS_OP_WRITE_PRODUCT);
    json_append_escaped(&w, c->out_dir);
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
        "\",\"nside\":%u,\"tile_width\":%u,\"data_type\":%d,\"flags\":%d,"
        "\"n_tiles\":%llu,\"n_tiles_written\":%llu,\"n_tiles_variance\":%llu,"
        "\"n_snr_points\":%llu,\"drizzle_provenance_set\":%d,"
        "\"workers\":%u,\"artifacts\":[",
        (unsigned)c->nside, (unsigned)c->tile_width, (int)c->data_type,
        (int)c->flags, (unsigned long long)inst->last_n_tiles,
        (unsigned long long)n_written, (unsigned long long)n_var,
        (unsigned long long)inst->last_n_snr, inst->last_prov_set,
        (unsigned)leased);
    int first = 1;
    for (int i = 0; i < 5; ++i) {
        if (!hips_artifact_exists(c->out_dir, kSubs[i])) continue;
        char p[1200];
        snprintf(p, sizeof(p), "%s/%s", c->out_dir, kSubs[i]);
        if (!first) *w++ = ',';
        *w++ = '"';
        json_append_escaped(&w, p);
        *w++ = '"';
        first = 0;
    }
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)), "]");

    acs_status ret = strbuf_write(out_manifest_json, buf, (uint64_t)(w - buf), err);
    free(buf);
    inst->last_status = ret;
    return ret;
}

static acs_status hips_execute(acs_module_instance_v1* inst_raw,
                               acs_str_v1 input_manifest_json,
                               acs_str_v1 config_json,
                               acs_strbuf_v1* out_manifest_json,
                               acs_error_info_v1* err) {
    hips_inst* inst = (hips_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state != ACS_LC_STATE_CREATED || inst->executing) {
        return efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
                     ACS_DIAG_ECODE_ILLEGAL_STATE, "hips: instance busy/state");
    }
    const char* mjson = input_manifest_json.data;
    if (!mjson || input_manifest_json.size == 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     ACS_DIAG_ECODE_NULL_CONFIG, "hips: empty input manifest");
    }
    hips_cfg c;
    acs_status st = hips_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) return st;

    inst->executing = 1;
    inst->state = ACS_LC_STATE_EXECUTING;
    st = hips_execute_write_product(inst, mjson, &c, out_manifest_json, err);
    hips_cfg_free(&c);
    inst->executing = 0;
    if (inst->state == ACS_LC_STATE_EXECUTING)
        inst->state = ACS_LC_STATE_CREATED;
    return st;
}

/* ═══════════════════ 7. 静态 vtable 与唯一导出入口 ═══════════════════ */

static const acs_module_api_v1 g_hips_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    hips_describe,
    hips_validate_config,
    hips_plan,
    hips_create,
    hips_execute,
    hips_inspect,
    hips_request_cancel,
    hips_destroy
};

/* 唯一导出入口 (ARC-001 §1.1; ABI-006 全查 exports):
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi,
                        const acs_host_api_v1* host,
                        const acs_module_api_v1** out_api) {
    (void)host;   /* allocator 必填在 create 期校验 (query 期 host 可 NULL 于探针) */
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (!out_api) return ACS_ERR_PARAM;
    *out_api = &g_hips_api;
    return ACS_OK;
}
#ifdef __cplusplus
} /* extern "C" */
#endif
