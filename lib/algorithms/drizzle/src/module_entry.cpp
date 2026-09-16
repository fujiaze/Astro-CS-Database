/* module_entry.cpp - astrocs.p1.drizzle 模块 C ABI v1 adapter
 *
 * 迁移任务 P1-DRZ-IMPL; 对齐先例 CAT-GAIA-IMPL (babe752d)。
 * 技术性偏差 T-3: legacy 头 hp_drizzle_api.h:14 include <cstdint> (C++ 头),
 * 纯 C TU 不可 include → 本 TU 以 C++ 编译, 全部导出面经 extern "C"
 * 保持 C ABI 不变 (唯一导出 astrocs_module_query_v1, 符号名不 mangle)。
 *
 * 职责: 把 lib/healpix_db/healpix_drizzle/ 的 legacy C 接口
 *   (hp_drizzle_api.h 六导出, API-DRZ-001 冻结) 包装成九操作模块 vtable;
 * 唯一导出 astrocs_module_query_v1 (导出面净化经链接 version-script:
 * legacy 六符号降 local, 见 CMakeLists.txt)。
 *
 * 科学纪律: scientific_change=false —— 本文件只做 config/manifest 解析、
 * PipelineFrame 组装、线程租借注入 (host executor lease → OMP ICV)、
 * 结果/产物转发; 不触碰 Eriksson 剖分、面积权重、NaN-Inf 传播语义
 * (SCI-DRZ-001 / drizzle_engine / hp_drizzle_api 生产源零改动)。
 *
 * v1 op 词表 (types.h): drizzle (帧→HiPS/legacy .hiss) | reverse (球→面)。
 * 其余四 legacy 导出 (fits_to_ahpx / reverse_capability / reverse_version)
 * 为工具通道, 不进模块 op 面 (迁移不裁剪 legacy 导出, 见迁移报告)。
 */
#define _POSIX_C_SOURCE 200809L

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define DRZ_ACCESS _access
#else
#include <unistd.h>
#define DRZ_ACCESS access
#endif

#ifdef _OPENMP
#include <omp.h>
#define DRZ_OMP_SET(n)   omp_set_num_threads(n)
#define DRZ_OMP_GET()    omp_get_max_threads()
#else
#define DRZ_OMP_SET(n)   ((void)0)
#define DRZ_OMP_GET()    1
#endif

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/drizzle/types.h"
#include "hp_drizzle_api.h"       /* legacy C API (extern "C"; C11 可直接 include) */
#include "aio_pipeline.h"         /* PipelineFrame 组装 (C API) */

/* ═══════════════════ 1. 基础 helper (对齐 gaia 先例) ═══════════════════ */

static const char kModuleId[]  = ASTROCS_DRIZZLE_MODULE_ID;
static const char kVersion[]   = "1.0";
static const char kBuildId[]   = ASTROCS_DRIZZLE_BUILD_ID;
static const char kSciId[]     = ASTROCS_DRIZZLE_SCI_ID;
static const char kAlgId[]     = ASTROCS_DRIZZLE_ALG_ID;
static const char kApiId[]     = ASTROCS_DRIZZLE_API_ID;

static acs_str_v1 acs_str_from(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

static void efill(acs_error_info_v1* err, acs_status st, int32_t domain,
                  uint32_t detail, const char* msg) {
    if (!err) return;
    memset(err, 0, sizeof(*err));
    err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err->head.abi_version = ACS_ABI_VERSION_V1;
    err->status = st;
    err->domain = domain;
    err->message_utf8 = msg;
    err->message_bytes = msg ? (uint32_t)strlen(msg) : 0;
    err->detail_code = detail;
}

/* strbuf 写 N 字节 (lifecycle_v1.h 冻结截断语义: size=所需; cap=0 只问尺寸;
 * 不足 → PARAM + BUFFER_TOO_SMALL) */
static acs_status strbuf_write(acs_strbuf_v1* out, const char* data, uint64_t n,
                               acs_error_info_v1* err) {
    if (!out) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: null output buffer");
        return ACS_ERR_PARAM;
    }
    out->size = n;
    if (!out->data || out->cap == 0) return ACS_OK;   /* 尺寸查询 */
    uint64_t room = out->cap - 1;                     /* 留 NUL 位 */
    uint64_t wr = n < room ? n : room;
    if (wr > 0) memcpy(out->data, data, (size_t)wr);
    out->data[wr] = '\0';
    if (n >= out->cap) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_BUFFER_TOO_SMALL, "drizzle: output buffer too small");
        return ACS_ERR_PARAM;
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

static int json_get_i32(const char* obj, const char* key, int* found) {
    double d = json_get_f64(obj, key, found);
    if (found && *found) {
        if (d < -2147483648.0 || d > 2147483647.0) { *found = 0; return 0; }
        return (int)d;
    }
    return 0;
}

/* "key":[f64,...] 数字数组 → malloc 数组 (调用方 free); 缺失/空 → NULL,
 * 语法坏 → NULL 且 *out_n=-1 */
static double* json_get_f64_array(const char* obj, const char* key, int* out_n) {
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
        if (*q == '-' || (*q >= '0' && *q <= '9')) {
            count++; char* end; strtod(q, &end); q = end;
        }
        else q++;
    }
    if (*q != ']') { *out_n = -1; return NULL; }
    double* arr = (double*)malloc((size_t)(count > 0 ? count : 1) * sizeof(double));
    if (!arr) { *out_n = -1; return NULL; }
    q = p; int i = 0;
    while (*q && *q != ']' && i < count) {
        if (*q == '-' || (*q >= '0' && *q <= '9')) {
            char* end; arr[i++] = strtod(q, &end); q = end;
        }
        else q++;
    }
    *out_n = count;
    return arr;
}

/* ───────── base64 (RFC 4648; 输出 NUL 结尾; 返回所需字节数含 NUL) ───────── */

static uint64_t b64_encoded_len(uint64_t n) {
    return ((n + 2) / 3) * 4 + 1;
}

static void b64_encode(const uint8_t* src, uint64_t n, char* dst) {
    static const char tab[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint64_t i = 0, o = 0;
    while (i + 3 <= n) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8) | src[i+2];
        dst[o++] = tab[(v >> 18) & 63];
        dst[o++] = tab[(v >> 12) & 63];
        dst[o++] = tab[(v >> 6) & 63];
        dst[o++] = tab[v & 63];
        i += 3;
    }
    uint64_t rem = n - i;
    if (rem == 1) {
        uint32_t v = (uint32_t)src[i] << 16;
        dst[o++] = tab[(v >> 18) & 63];
        dst[o++] = tab[(v >> 12) & 63];
        dst[o++] = '=';
        dst[o++] = '=';
    } else if (rem == 2) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i+1] << 8);
        dst[o++] = tab[(v >> 18) & 63];
        dst[o++] = tab[(v >> 12) & 63];
        dst[o++] = tab[(v >> 6) & 63];
        dst[o++] = '=';
    }
    dst[o] = '\0';
}

/* 解码 base64 (返回字节数, 失败 -1)。dst 需 ≥ 3*n/4+2 字节 */
static int64_t b64_decode(const char* src, uint64_t src_len, uint8_t* dst) {
    static int8_t rev[256];
    static int init = 0;
    if (!init) {
        memset(rev, -1, sizeof(rev));
        const char* tab =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; i++) rev[(unsigned char)tab[i]] = (int8_t)i;
        init = 1;
    }
    uint64_t o = 0;
    uint32_t acc = 0;
    int bits = 0, pad = 0;
    for (uint64_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        if (c == '=') { pad++; continue; }
        if (pad) return -1;                 /* '=' 后仍有数据 */
        int8_t v = rev[c];
        if (v < 0) return -1;               /* 非法字符 */
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            dst[o++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    if (pad > 2) return -1;
    return (int64_t)o;
}

/* JSON 字符串值内联 (只处理 " 与 \, 控制字符降为 '?') */
static void json_append_escaped(char** w, const char* s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { *(*w)++ = '\\'; *(*w)++ = *s; }
        else if ((unsigned char)*s < 0x20) *(*w)++ = '?';
        else *(*w)++ = *s;
    }
}

/* f64 以 "%.17g" 写入 JSON (往返精确; 与 aio_frame_kv_set_double 同口径) */
static void json_append_f64(char** w, double d) {
    *w += snprintf(*w, 40, "%.17g", d);
}

/* ═══════════════════ 2. config 解析与校验 ═══════════════════ */

typedef struct {
    char     op[16];              /* ASTROCS_DRIZZLE_OP_* */
    uint32_t nside;               /* 2 的幂 */
    int      nested;              /* 0/1 */
    double   pixfrac;             /* [0,1] (DISP-DRZ-003 口径, 不改语义) */
    int      precision_mode;      /* 0/1/-1 */
    char*    output_path;         /* 可选 (legacy .hiss) */
    char*    hips_dir;            /* 可选 (HiPS 产品集根目录) */
    uint32_t max_workers;         /* 0=不设限 */
    /* reverse op 专属 */
    int32_t  rev_width, rev_height;
    double   rev_scale;
    double   rev_ra0, rev_dec0;
    char     rev_proj[16];
    int      have_scale, have_ra0, have_dec0;
} drz_cfg;

static void drz_cfg_free(drz_cfg* c) {
    if (!c) return;
    free(c->output_path); c->output_path = NULL;
    free(c->hips_dir);    c->hips_dir = NULL;
}

static int is_pow2_u32(uint32_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

/* 解析 + 词表/有限性校验 (validate_config/plan/create/execute 共享;
 * detail 码见 types.h DRZ_ECODE_*) */
static acs_status drz_cfg_parse(const char* json, drz_cfg* c,
                                acs_error_info_v1* err) {
    memset(c, 0, sizeof(*c));
    if (!json || !json[0]) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CONFIG, "drizzle: empty config");
        return ACS_ERR_PARAM;
    }

    /* op (必填) */
    if (!json_copy_str(json, DRZ_CFG_KEY_OP, c->op, sizeof(c->op))) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_MISSING_FIELD, "drizzle: config missing 'op'");
        return ACS_ERR_PARAM;
    }
    if (strcmp(c->op, ASTROCS_DRIZZLE_OP_DRIZZLE) != 0 &&
        strcmp(c->op, ASTROCS_DRIZZLE_OP_REVERSE) != 0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_UNKNOWN_OP, "drizzle: unknown op (v1: drizzle|reverse)");
        return ACS_ERR_PARAM;
    }

    int found = 0;
    double pixfrac = json_get_f64(json, DRZ_CFG_KEY_PIXFRAC, &found);
    if (!found) pixfrac = 1.0;                     /* API-DRZ-001 默认 1.0 */
    if (!isfinite(pixfrac) || pixfrac < 0.0 || pixfrac > 1.0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_BAD_VALUE, "drizzle: pixfrac must be finite in [0,1]");
        return ACS_ERR_PARAM;
    }
    c->pixfrac = pixfrac;

    double prec = json_get_f64(json, DRZ_CFG_KEY_PRECISION, &found);
    if (!found) prec = -1.0;                       /* -1=auto (legacy KV 读取) */
    if (!isfinite(prec) || prec < -1.0 || prec > 1.0 ||
        prec != (double)(int)prec) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_BAD_VALUE, "drizzle: precision_mode must be -1|0|1");
        return ACS_ERR_PARAM;
    }
    c->precision_mode = (int)prec;

    c->max_workers = (uint32_t)json_get_u64(json, DRZ_CFG_KEY_MAX_WORKERS, &found);
    (void)found;   /* 缺省=0 (不设限) */

    char path_buf[1024];
    if (json_copy_str(json, DRZ_CFG_KEY_OUTPUT_PATH, path_buf, sizeof(path_buf))) {
        c->output_path = strdup(path_buf);
        if (!c->output_path) {
            efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                  ACS_DIAG_ECODE_NONE, "drizzle: strdup output_path");
            return ACS_ERR_NOMEM;
        }
    }
    if (json_copy_str(json, DRZ_CFG_KEY_HIPS_DIR, path_buf, sizeof(path_buf))) {
        c->hips_dir = strdup(path_buf);
        if (!c->hips_dir) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                  ACS_DIAG_ECODE_NONE, "drizzle: strdup hips_dir");
            return ACS_ERR_NOMEM;
        }
    }

    if (strcmp(c->op, ASTROCS_DRIZZLE_OP_DRIZZLE) == 0) {
        double nside = json_get_f64(json, DRZ_CFG_KEY_NSIDE, &found);
        if (!found) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_MISSING_FIELD, "drizzle: op=drizzle requires 'nside'");
            return ACS_ERR_PARAM;
        }
        if (!isfinite(nside) || nside < 1.0 || nside > 4294967295.0 ||
            nside != (double)(uint32_t)nside) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_BAD_VALUE, "drizzle: nside must be u32");
            return ACS_ERR_PARAM;
        }
        c->nside = (uint32_t)nside;
        if (!is_pow2_u32(c->nside)) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_BAD_VALUE, "drizzle: nside must be power of 2");
            return ACS_ERR_PARAM;
        }
        double nested = json_get_f64(json, DRZ_CFG_KEY_NESTED, &found);
        c->nested = (found && nested != 0.0) ? 1 : 0;
        return ACS_OK;   /* output 路径双空 → execute 前置拒绝 (见 execute) */
    }

    /* ── op=reverse 专属字段 ── */
    double w = json_get_f64(json, DRZ_CFG_KEY_REV_WIDTH, &found);
    if (!found) {
        drz_cfg_free(c);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_MISSING_FIELD, "drizzle: op=reverse requires 'width'");
        return ACS_ERR_PARAM;
    }
    double h = json_get_f64(json, DRZ_CFG_KEY_REV_HEIGHT, &found);
    if (!found) {
        drz_cfg_free(c);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_MISSING_FIELD, "drizzle: op=reverse requires 'height'");
        return ACS_ERR_PARAM;
    }
    if (!isfinite(w) || !isfinite(h) || w < 1.0 || h < 1.0 ||
        w > 2147483647.0 || h > 2147483647.0 ||
        w != (double)(int)w || h != (double)(int)h) {
        drz_cfg_free(c);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_BAD_VALUE, "drizzle: reverse width/height must be i32 >= 1");
        return ACS_ERR_PARAM;
    }
    c->rev_width = (int32_t)w;
    c->rev_height = (int32_t)h;

    double sc = json_get_f64(json, DRZ_CFG_KEY_REV_SCALE, &found);
    if (found) {
        if (!isfinite(sc) || sc <= 0.0) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_BAD_VALUE, "drizzle: pixel_scale_arcsec must be > 0");
            return ACS_ERR_PARAM;
        }
        c->rev_scale = sc;
        c->have_scale = 1;
    }
    double ra0 = json_get_f64(json, DRZ_CFG_KEY_REV_RA0, &found);
    if (found) {
        if (!isfinite(ra0)) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_BAD_VALUE, "drizzle: ra_center_deg must be finite");
            return ACS_ERR_PARAM;
        }
        c->rev_ra0 = ra0;
        c->have_ra0 = 1;
    }
    double dec0 = json_get_f64(json, DRZ_CFG_KEY_REV_DEC0, &found);
    if (found) {
        if (!isfinite(dec0)) {
            drz_cfg_free(c);
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  DRZ_ECODE_BAD_VALUE, "drizzle: dec_center_deg must be finite");
            return ACS_ERR_PARAM;
        }
        c->rev_dec0 = dec0;
        c->have_dec0 = 1;
    }
    json_copy_str(json, DRZ_CFG_KEY_REV_PROJ, c->rev_proj, sizeof(c->rev_proj));
    /* pixfrac 对 reverse 为 (0,1] (REV-101); [0,1] 校验已做, 端点语义由
     * legacy 自身校验 (错误码 3, 原样转发) */
    return ACS_OK;
}

/* ═══════════════════ 3. 实例 ═══════════════════ */

typedef struct {
    uint32_t state;                 /* ACS_LC_STATE_* */
    int executing;                  /* 同实例 execute 互斥旗标 (ABI-002) */
    drz_cfg cfg;                    /* create 时解析的 config 副本 */
    const acs_host_api_v1* host;
    volatile int cancel_req;        /* request_cancel 置位; execute 入口检查 */
    uint64_t exec_count;
    acs_status last_status;
    char last_op[16];
    uint32_t last_workers;
    int legacy_last_code;           /* 上次 legacy 错误码 (0=成功) */
} drz_inst;

/* ═══════════════════ 4. describe / validate_config ═══════════════════ */
/* MOD-001: describe 空 module_id 合同见 drz_describe 内注释 (loader empty 调用)。 */

static acs_status drz_describe(const acs_module_api_v1* self,
                               acs_str_v1 module_id,
                               acs_module_descriptor_v1* out_desc) {
    (void)self;
    if (!out_desc) return ACS_ERR_PARAM;
    /* MOD-001 加载验证对齐: ABI-003 安全 loader 以 empty module_id 调 describe
     * (secure_loader.c §5 既成合同; BLD-003 noop 先例同语义), 空=不指名, 直接
     * 返回静态描述; 非空仍严格校验 (错 ID → MISMATCH, 既有 adapter 测试锚不变)。
     * 空 ID 拒绝曾使科学 DLL 在安装树内被自家 loader 必拒 (DESCRIPTOR_MISMATCH),
     * 本行为修复经 tests/abi/mod001_install_load_check.py 安装树逐 unit 加载闭环。 */
    if (module_id.size != 0 &&
        (module_id.size != strlen(kModuleId) ||
         memcmp(module_id.data, kModuleId, strlen(kModuleId)) != 0))
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
    out_desc->config_schema_ver = ASTROCS_DRIZZLE_CONFIG_SCHEMA_VER;
    out_desc->execution_class = 0;    /* cpu_heavy (module.yaml resource_class) */
    out_desc->parallel_ok = 1;        /* 帧内 OpenMP; 线程经 host executor 租借 */
    out_desc->flags = 0;
    return ACS_OK;
}

static acs_status drz_validate_config(const acs_module_api_v1* self,
                                      acs_str_v1 config_json,
                                      acs_error_info_v1* err) {
    (void)self;
    drz_cfg tmp;
    acs_status st = drz_cfg_parse(config_json.data, &tmp, err);
    drz_cfg_free(&tmp);
    return st;
}

/* ═══════════════════ 5. plan (真实 work_units; 元数据推导, 禁空转) ═══════════════════
 *
 * v1 并行轴 = 帧内 OpenMP (TileAccumulator 归并按线程序, 固定次序确定);
 * work_units = 单帧 execute = 1 (帧通道逐帧调用; 多帧扇出由 orchestrator 编排)。
 * memory/io 估算公式 (全部由 config 元数据推导, 无魔数):
 *   accumulator_bytes = 12*nside^2 * 8B * 6   (6 个 f64 累加器/像素)
 *   io_out_bytes      ≈ 12*nside^2 * dtype_bytes * 3 产品 (signal/support/snr)
 */

static acs_status drz_plan(const acs_module_api_v1* self,
                           acs_str_v1 node_id,
                           acs_str_v1 config_json,
                           acs_strbuf_v1* out_plan_json,
                           acs_error_info_v1* err) {
    (void)self;
    if (!out_plan_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: null plan buffer");
        return ACS_ERR_PARAM;
    }
    drz_cfg c;
    acs_status st = drz_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) return st;

    uint64_t n_pix_hp = 12ull * (uint64_t)c.nside * (uint64_t)c.nside;
    uint64_t acc_bytes = n_pix_hp * 8ull * 6ull;   /* 6×f64 累加器 */
    uint64_t io_out_bytes = (c.precision_mode == 1) ? n_pix_hp * 8ull * 3ull
                                                    : n_pix_hp * 4ull * 3ull;

    char node[256];
    uint64_t node_len = node_id.size < sizeof(node) - 1 ? node_id.size
                                                        : sizeof(node) - 1;
    if (node_id.data && node_len) {
        memcpy(node, node_id.data, (size_t)node_len);
    }
    node[node_len] = '\0';

    char buf[1280];
    char* w = buf;
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
        "{\"plan_version\":%u,\"module_id\":\"%s\",\"node_id\":\"",
        ASTROCS_DRIZZLE_PLAN_VERSION, kModuleId);
    json_append_escaped(&w, node);
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
        "\",\"op\":\"%s\",\"work_units\":1,"
        "\"work_units_basis\":\"single_frame_execute\","
        "\"parallel_axis\":\"omp_threads\","
        "\"min_workers\":1,\"max_workers\":%u,"
        "\"determinism\":\"fixed_reduction_order\",",
        c.op, (unsigned)(c.max_workers > 0 ? c.max_workers : 0));
    if (strcmp(c.op, ASTROCS_DRIZZLE_OP_DRIZZLE) == 0) {
        w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
            "\"nside\":%u,\"nested\":%d,\"n_healpix_pixels\":%llu,"
            "\"memory_accumulator_bytes\":%llu,\"io_out_estimate_bytes\":%llu,",
            (unsigned)c.nside, c.nested, (unsigned long long)n_pix_hp,
            (unsigned long long)acc_bytes, (unsigned long long)io_out_bytes);
    } else {
        w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
            "\"width\":%d,\"height\":%d,"
            "\"memory_plane_bytes\":%llu,\"io_out_estimate_bytes\":%llu,",
            (int)c.rev_width, (int)c.rev_height,
            (unsigned long long)((uint64_t)c.rev_width * (uint64_t)c.rev_height * 8ull * 2ull),
            (unsigned long long)((uint64_t)c.rev_width * (uint64_t)c.rev_height *
                                 (c.precision_mode == 1 ? 8ull : 4ull) * 2ull));
    }
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w), "\"pixfrac\":");
    json_append_f64(&w, c.pixfrac);
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
        ",\"precision_mode\":%d}", c.precision_mode);

    drz_cfg_free(&c);
    return strbuf_write_cstr(out_plan_json, buf, err);
}

/* ═══════════════════ 6. create / destroy / inspect / cancel ═══════════════════ */

static acs_status drz_create(const acs_module_api_v1* self,
                             acs_str_v1 config_json,
                             const acs_host_api_v1* host,
                             acs_module_instance_v1** out,
                             acs_error_info_v1* err) {
    (void)self;
    if (!out) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: null out");
        return ACS_ERR_PARAM;
    }
    *out = NULL;
    if (!host || !host->allocator) {
        efill(err, ACS_ERR_ABI_MISMATCH, ACS_ERR_DOMAIN_INTERNAL,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: host allocator required");
        return ACS_ERR_ABI_MISMATCH;
    }
    drz_cfg c;
    acs_status st = drz_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) { drz_cfg_free(&c); return st; }

    drz_inst* inst = (drz_inst*)calloc(1, sizeof(drz_inst));
    if (!inst) {
        drz_cfg_free(&c);
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
              ACS_DIAG_ECODE_NONE, "drizzle: inst alloc failed");
        return ACS_ERR_NOMEM;
    }
    inst->state = ACS_LC_STATE_CREATED;
    inst->host = host;
    inst->cfg = c;                     /* 接管 output_path/hips_dir 所有权 */
    inst->last_status = ACS_OK;
    inst->legacy_last_code = 0;
    *out = (acs_module_instance_v1*)inst;
    return ACS_OK;
}

static acs_status drz_inspect(const acs_module_instance_v1* inst_raw,
                              acs_strbuf_v1* out_json,
                              acs_error_info_v1* err) {
    const drz_inst* inst = (const drz_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"module_id\":\"%s\",\"state\":%u,\"executing\":%d,"
             "\"cancel_req\":%d,\"exec_count\":%llu,\"last_status\":%d,"
             "\"last_op\":\"%s\",\"last_workers\":%u,"
             "\"legacy_last_code\":%d,"
             "\"cancel_support\":\"entry\","
             "\"threads_note\":\"host executor lease -> OMP ICV\"}",
             kModuleId, (unsigned)inst->state, inst->executing,
             inst->cancel_req, (unsigned long long)inst->exec_count,
             (int)inst->last_status, inst->last_op,
             (unsigned)inst->last_workers, inst->legacy_last_code);
    return strbuf_write_cstr(out_json, buf, err);
}

static acs_status drz_request_cancel(acs_module_instance_v1* inst_raw) {
    drz_inst* inst = (drz_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state == ACS_LC_STATE_DESTROYED) return ACS_ERR_STATE;
    inst->cancel_req = 1;              /* 幂等单向置位 (ABI-002) */
    return ACS_OK;
}

static void drz_destroy(acs_module_instance_v1* inst_raw) {
    drz_inst* inst = (drz_inst*)inst_raw;
    if (!inst) return;                 /* inst=NULL 空操作 */
    drz_cfg_free(&inst->cfg);
    inst->state = ACS_LC_STATE_DESTROYED;
    free(inst);
}

/* ═══════════════════ 7. execute — drizzle op (帧通道) ═══════════════════
 *
 * manifest (输入; typed artifact 输入 v1 = inline base64 行数据, 顶层平铺):
 * {"op":"drizzle","width":W,"height":H,"dtype":"f32"|"f64",
 *  "data_base64":"<W*H 元素行数据>",
 *  "header":{"CRVAL1":"...","PHOTSCAL":"1.0",...}}   ← header 对象键值
 * config 承载 nside/nested/pixfrac/precision_mode/output_path/hips_dir;
 * manifest 头键直接进帧 header KV (hp_drizzle_api.cpp 冻结键词表)。
 */

/* header 键词表 = WCS(线性/SIP 阶) + 测光 + 元数据 (hp_drizzle_api.cpp 读取键) */
static const char* const kHeaderKeys[] = {
    "CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2",
    "CD1_1", "CD1_2", "CD2_1", "CD2_2",
    "CDELT1", "CDELT2", "CROTA1", "CROTA2",
    "CTYPE1", "CTYPE2",
    "A_ORDER", "B_ORDER", "AP_ORDER", "BP_ORDER",
    "PHOTSCAL", "PHOTAPPL", "PRECISION",
    "FILTER", "EXPTIME", "DATE-OBS",
    "OBJECT", "RADESYS", "EQUINOX", "INSTRUME", "TELESCOP",
    "XPIXSZ", "YPIXSZ", "XBINNING", "YBINNING", "GAIN", "OFFSET",
    "src_pixel_scale_arcsec"
};
#define DRZ_HEADER_KEY_N ((int)(sizeof(kHeaderKeys) / sizeof(kHeaderKeys[0])))
/* SIP 系数词表: {A,B,AP,BP}_{i}_{j}, i,j ∈ [0,5] (P0-1 order≤5 冻结) */
#define DRZ_SIP_ORDER_MAX 5

static acs_status drz_frame_set_header(PipelineFrame* frame, const char* manifest,
                                       acs_error_info_v1* err) {
    char val[512];
    for (int i = 0; i < DRZ_HEADER_KEY_N; i++) {
        if (json_copy_str(manifest, kHeaderKeys[i], val, sizeof(val))) {
            if (aio_frame_kv_set(frame, "header", kHeaderKeys[i], val) != 0) {
                efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
                      DRZ_ECODE_FRAME_BUILD, "drizzle: kv_set failed");
                return ACS_ERR_IO;
            }
        }
    }
    char key[16];
    for (int p = 0; p < 4; p++) {
        const char* pref = (p == 0) ? "A" : (p == 1) ? "B" : (p == 2) ? "AP" : "BP";
        for (int a = 0; a <= DRZ_SIP_ORDER_MAX; a++) {
            for (int b = 0; b <= DRZ_SIP_ORDER_MAX; b++) {
                snprintf(key, sizeof(key), "%s_%d_%d", pref, a, b);
                if (json_copy_str(manifest, key, val, sizeof(val))) {
                    if (aio_frame_kv_set(frame, "header", key, val) != 0) {
                        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
                              DRZ_ECODE_FRAME_BUILD, "drizzle: kv_set failed");
                        return ACS_ERR_IO;
                    }
                }
            }
        }
    }
    return ACS_OK;
}

typedef struct {
    void*    data;          /* malloc 解码缓冲 */
    int64_t  n_bytes;
    int      is_f64;
    int64_t  width, height;
} drz_rows;

static void drz_rows_free(drz_rows* r) { free(r->data); r->data = NULL; }

/* 解析 manifest 行数据块并 base64 解码 (长度=字节精确校验) */
static acs_status drz_rows_parse(const char* manifest, drz_rows* r,
                                 acs_error_info_v1* err) {
    memset(r, 0, sizeof(*r));
    int found = 0;
    double w = json_get_f64(manifest, "width", &found);
    if (!found) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: manifest missing width");
        return ACS_ERR_PARAM;
    }
    double h = json_get_f64(manifest, "height", &found);
    if (!found) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: manifest missing height");
        return ACS_ERR_PARAM;
    }
    if (!isfinite(w) || !isfinite(h) || w < 1.0 || h < 1.0 ||
        w > 2147483647.0 || h > 2147483647.0 ||
        w != (double)(int64_t)w || h != (double)(int64_t)h) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: rows width/height invalid");
        return ACS_ERR_PARAM;
    }
    char dtype[8];
    if (!json_copy_str(manifest, "dtype", dtype, sizeof(dtype))) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: manifest missing dtype");
        return ACS_ERR_PARAM;
    }
    if (strcmp(dtype, "f64") == 0) r->is_f64 = 1;
    else if (strcmp(dtype, "f32") == 0) r->is_f64 = 0;
    else {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD,
              "drizzle: dtype must be f32|f64 (单通道 Stage1)");
        return ACS_ERR_PARAM;
    }
    const char* b64; uint64_t b64_len;
    if (!json_find_str(manifest, "data_base64", &b64, &b64_len) || b64_len == 0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: manifest missing data_base64");
        return ACS_ERR_PARAM;
    }
    uint64_t n_elem = (uint64_t)w * (uint64_t)h;
    uint64_t need = n_elem * (r->is_f64 ? 8ull : 4ull);
    r->data = malloc((size_t)(need > 0 ? need : 1));
    if (!r->data) {
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
              ACS_DIAG_ECODE_NONE, "drizzle: rows alloc failed");
        return ACS_ERR_NOMEM;
    }
    int64_t got = b64_decode(b64, b64_len, (uint8_t*)r->data);
    if (got < 0 || (uint64_t)got != need) {
        free(r->data); r->data = NULL;
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_DATA_DECODE,
              "drizzle: data_base64 decode failed or length mismatch");
        return ACS_ERR_PARAM;
    }
    r->n_bytes = got;
    r->width = (int64_t)w;
    r->height = (int64_t)h;
    return ACS_OK;
}

/* 组装 PipelineFrame (typed artifact 输入 → 帧块); 失败时 *out=NULL */
static acs_status drz_frame_build(const drz_rows* r, const char* manifest,
                                  PipelineFrame** out, acs_error_info_v1* err) {
    *out = NULL;
    PipelineFrame* frame = aio_pipeline_frame_create();
    if (!frame) {
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
              DRZ_ECODE_FRAME_BUILD, "drizzle: frame create failed");
        return ACS_ERR_IO;
    }
    int dims[2];
    dims[0] = (int)r->height;          /* hp_drizzle_run: dims[0]=H, dims[1]=W */
    dims[1] = (int)r->width;
    int rc = aio_frame_add_block(frame, "data",
                                 r->is_f64 ? AIO_BLOCK_FLOAT64 : AIO_BLOCK_FLOAT32,
                                 r->data, r->width * r->height,
                                 dims, 2, "drizzle adapter input plane");
    if (rc != 0) {
        aio_pipeline_frame_destroy(frame);
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
              DRZ_ECODE_FRAME_BUILD, "drizzle: add data block failed");
        return ACS_ERR_IO;
    }
    acs_status st = drz_frame_set_header(frame, manifest, err);
    if (st != ACS_OK) { aio_pipeline_frame_destroy(frame); return st; }
    *out = frame;
    return ACS_OK;
}

/* legacy 错误码 → 稳定 acs_status (原样转发 message, 不重解释;
 * DISP-DRZ 缺陷登记在案, 本迁移不消化) */
static acs_status drz_legacy_status(int code, char* msg, size_t msg_cap,
                                    acs_error_info_v1* err,
                                    const char* legacy_err) {
    int32_t domain = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
    if (code <= -4 && code >= -8) domain = ACS_ERR_DOMAIN_DATA;
    else if (code == -1 || code == -2 || code == -10) domain = ACS_ERR_DOMAIN_CONFIG;
    snprintf(msg, msg_cap, "drizzle: legacy_code=%d: %s", code,
             legacy_err && legacy_err[0] ? legacy_err : "(no detail)");
    efill(err, ACS_ERR_PARAM, domain, DRZ_ECODE_LEGACY_REJECT, msg);
    return ACS_ERR_PARAM;
}

/* HiPS/legacy hiss 产物存在性计数 (事务 sink 报告) */
static int drz_artifact_exists(const char* dir, const char* sub) {
    if (!dir || !dir[0]) return 0;
    char p[1024];
    snprintf(p, sizeof(p), "%s/%s", dir, sub);
    return DRZ_ACCESS(p, F_OK) == 0;
}

static acs_status drz_execute_drizzle(drz_inst* inst, const char* manifest,
                                      const drz_cfg* c,
                                      acs_strbuf_v1* out_manifest_json,
                                      acs_error_info_v1* err) {
    if (!c->hips_dir && !c->output_path) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              DRZ_ECODE_MISSING_FIELD,
              "drizzle: op=drizzle requires hips_dir or output_path (事务 sink 输出)");
        return ACS_ERR_PARAM;
    }
    if (!out_manifest_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: null output buffer");
        return ACS_ERR_PARAM;
    }

    drz_rows rows;
    acs_status st = drz_rows_parse(manifest, &rows, err);
    if (st != ACS_OK) return st;

    PipelineFrame* frame = NULL;
    st = drz_frame_build(&rows, manifest, &frame, err);
    if (st != ACS_OK) { drz_rows_free(&rows); return st; }

    /* host executor 租借 (FORBID-003: 禁私建线程池)。线程数注入途径:
     * legacy DrizzleConfig.threads=0 → omp_get_max_threads();
     * adapter 在调用线程置 OMP ICV = 租借值 (omp_set_num_threads),
     * 单帧并行区生效, 不改生产源 (技术整改点 T-2, 见迁移报告)。 */
    uint32_t leased = 0;
    int lease_active = 0;
    int omp_prev = -1;
    if (inst->host && inst->host->executor) {
        const acs_executor_v1* ex = inst->host->executor;
        uint32_t want = ex->max_workers;
        if (want == 0) want = ex->available_cpus;
        if (c->max_workers > 0 && c->max_workers < want) want = c->max_workers;
        if (want < 1) want = 1;
        if (ex->acquire && ex->acquire(ex->user_data, want) == 0) {
            leased = want;
            lease_active = 1;
            omp_prev = DRZ_OMP_GET();
            DRZ_OMP_SET((int)want);
        }
        /* acquire 失败 → 单线程降级执行 (BUDGET 缺额非致命; workers=0) */
    }

    HpDrizzleResult res;
    memset(&res, 0, sizeof(res));
    int rc = hp_drizzle_run_hips(frame, (int)c->nside, c->nested, c->pixfrac,
                                 c->hips_dir, c->output_path, &res,
                                 c->precision_mode);
    if (lease_active && inst->host->executor->release)
        inst->host->executor->release(inst->host->executor->user_data, leased);
    if (omp_prev > 0) DRZ_OMP_SET(omp_prev);

    aio_pipeline_frame_destroy(frame);
    drz_rows_free(&rows);

    inst->legacy_last_code = rc;
    snprintf(inst->last_op, sizeof(inst->last_op), "%s", ASTROCS_DRIZZLE_OP_DRIZZLE);

    if (rc != 0) {
        char msg[640];
        inst->last_status = drz_legacy_status(rc, msg, sizeof(msg), err, res.error_msg);
        return inst->last_status;
    }

    /* 输出 manifest (stats + 事务 sink 产物 URI) */
    uint64_t total = 256u;
    if (c->hips_dir) {
        total += strlen(c->hips_dir) + 64u;
        if (drz_artifact_exists(c->hips_dir, "signal")) total += strlen(c->hips_dir) + 24u;
        if (drz_artifact_exists(c->hips_dir, "support")) total += strlen(c->hips_dir) + 24u;
        if (drz_artifact_exists(c->hips_dir, "snr")) total += strlen(c->hips_dir) + 24u;
    }
    if (c->output_path) total += strlen(c->output_path) + 40u;
    char* buf = (char*)malloc((size_t)total + 1);
    if (!buf) {
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
              ACS_DIAG_ECODE_NONE, "drizzle: out manifest alloc failed");
        inst->last_status = ACS_ERR_NOMEM;
        return ACS_ERR_NOMEM;
    }
    char* w = buf;
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
        "{\"op\":\"drizzle\",\"nside\":%d,\"nested\":%d,\"pixfrac\":",
        res.nside, res.nested);
    json_append_f64(&w, res.pixfrac);
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
        ",\"precision_mode\":%d,\"n_healpix_pixels\":%lld,\"n_source_pixels\":%lld,"
        "\"workers\":%u,\"artifacts\":[",
        c->precision_mode, (long long)res.n_healpix_pixels,
        (long long)res.n_source_pixels, (unsigned)leased);
    int first = 1;
    if (c->hips_dir) {
        const char* subs[3] = { "signal", "support", "snr" };
        for (int i = 0; i < 3; i++) {
            if (!drz_artifact_exists(c->hips_dir, subs[i])) continue;
            char p[1024];
            snprintf(p, sizeof(p), "%s/%s", c->hips_dir, subs[i]);
            if (!first) *w++ = ',';
            *w++ = '"';
            json_append_escaped(&w, p);
            *w++ = '"';
            first = 0;
        }
    }
    if (c->output_path) {
        if (!first) *w++ = ',';
        *w++ = '"';
        json_append_escaped(&w, c->output_path);
        *w++ = '"';
        first = 0;
    }
    w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)), "]");

    acs_status ret = strbuf_write(out_manifest_json, buf, (uint64_t)(w - buf), err);
    free(buf);
    inst->exec_count++;
    inst->last_status = ret;
    inst->last_workers = leased;
    return ret;
}

/* ═══════════════════ 8. execute — reverse op (球面 → 平面) ═══════════════════
 *
 * manifest (顶层平铺):
 * {"op":"reverse","nside":...,"nested":1,"pixfrac":...,"output_fp64":0|1,
 *  "no_data_as_zero":0|1,"crval":[ra,dec],"crpix":[x,y],
 *  "cd":[c11,c12,c21,c22],"sip_order":0,"sip_ap_order":0,
 *  "sip_a":[36]...,"sip_b":[36]...,(可选)
 *  "n_leaf":N,"leaf_ipix_base64":"<u64×N>","leaf_dtype":"f32"|"f64",
 *  "leaf_signal_base64":"<dtype×N>","leaf_support_base64":"<f64×N>(可选)"}
 * 输出 manifest: stats + signal_plane_base64 + coverage_plane_base64。
 */

static acs_status drz_execute_reverse(drz_inst* inst, const char* manifest,
                                      const drz_cfg* c,
                                      acs_strbuf_v1* out_manifest_json,
                                      acs_error_info_v1* err) {
    if (!out_manifest_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "drizzle: null output buffer");
        return ACS_ERR_PARAM;
    }
    int found = 0;
    double nside = json_get_f64(manifest, "nside", &found);
    if (!found || !isfinite(nside) || nside < 1.0 ||
        nside > 4294967295.0 || nside != (double)(uint32_t)nside ||
        !is_pow2_u32((uint32_t)nside)) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD,
              "drizzle: reverse manifest nside must be pow2 u32");
        return ACS_ERR_PARAM;
    }
    double nested = json_get_f64(manifest, "nested", &found);
    double pixfrac = json_get_f64(manifest, "pixfrac", &found);
    if (!found) pixfrac = c->pixfrac;
    if (!isfinite(pixfrac) || pixfrac < 0.0 || pixfrac > 1.0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse pixfrac must be in [0,1]");
        return ACS_ERR_PARAM;
    }
    double out_fp64 = json_get_f64(manifest, "output_fp64", &found);
    if (!found) out_fp64 = (c->precision_mode == 1) ? 1.0 : 0.0;
    double no_data_as_zero = json_get_f64(manifest, "no_data_as_zero", &found);
    if (!found) no_data_as_zero = 1.0;

    /* WCS: crval/crpix/cd 数组 */
    int n_arr = 0;
    double* crval = json_get_f64_array(manifest, "crval", &n_arr);
    if (!crval || n_arr != 2) { free(crval);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse needs crval[2]");
        return ACS_ERR_PARAM; }
    double* crpix = json_get_f64_array(manifest, "crpix", &n_arr);
    if (!crpix || n_arr != 2) { free(crval); free(crpix);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse needs crpix[2]");
        return ACS_ERR_PARAM; }
    double* cd = json_get_f64_array(manifest, "cd", &n_arr);
    if (!cd || n_arr != 4) { free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse needs cd[4]");
        return ACS_ERR_PARAM; }
    int sip_order = json_get_i32(manifest, "sip_order", &found);
    if (!found) sip_order = 0;
    int sip_ap_order = json_get_i32(manifest, "sip_ap_order", &found);
    if (!found) sip_ap_order = 0;

    /* leaf 数据解码 */
    const char* b64; uint64_t b64_len;
    if (!json_find_str(manifest, "leaf_ipix_base64", &b64, &b64_len) || b64_len == 0) {
        free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse missing leaf_ipix_base64");
        return ACS_ERR_PARAM;
    }
    uint64_t n_leaf = json_get_u64(manifest, "n_leaf", &found);
    if (!found) n_leaf = b64_len / 12ull;   /* 容错: 缺省按 b64 保守估计, 长度校验兜底 */
    char leaf_dtype[8];
    int sig_is_f64 = 0;
    if (json_copy_str(manifest, "leaf_dtype", leaf_dtype, sizeof(leaf_dtype)))
        sig_is_f64 = (strcmp(leaf_dtype, "f64") == 0);
    else
        sig_is_f64 = (out_fp64 > 0.5) ? 1 : 0;
    uint64_t ipix_need = n_leaf * 8ull;
    uint64_t sig_need = n_leaf * (sig_is_f64 ? 8ull : 4ull);
    uint8_t* ipix_raw = (uint8_t*)malloc((size_t)(b64_len + 4 > ipix_need ? b64_len + 4 : ipix_need + 1));
    uint8_t* sig_raw = (uint8_t*)malloc((size_t)(sig_need > 0 ? sig_need : 1));
    if (!ipix_raw || !sig_raw) {
        free(ipix_raw); free(sig_raw); free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
              ACS_DIAG_ECODE_NONE, "drizzle: reverse leaf alloc failed");
        return ACS_ERR_NOMEM;
    }
    int64_t got = b64_decode(b64, b64_len, ipix_raw);
    if (got < 0 || (uint64_t)got != ipix_need) {
        free(ipix_raw); free(sig_raw); free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_DATA_DECODE, "drizzle: leaf_ipix_base64 decode failed");
        return ACS_ERR_PARAM;
    }
    if (!json_find_str(manifest, "leaf_signal_base64", &b64, &b64_len) || b64_len == 0) {
        free(ipix_raw); free(sig_raw); free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_MANIFEST_FIELD, "drizzle: reverse missing leaf_signal_base64");
        return ACS_ERR_PARAM;
    }
    got = b64_decode(b64, b64_len, sig_raw);
    if (got < 0 || (uint64_t)got != sig_need) {
        free(ipix_raw); free(sig_raw); free(crval); free(crpix); free(cd);
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              DRZ_ECODE_DATA_DECODE, "drizzle: leaf_signal_base64 decode failed");
        return ACS_ERR_PARAM;
    }
    /* support 可选 (f64×N) */
    double* support = NULL;
    uint8_t* sup_raw = NULL;
    if (json_find_str(manifest, "leaf_support_base64", &b64, &b64_len) && b64_len > 0) {
        sup_raw = (uint8_t*)malloc((size_t)(n_leaf * 8ull > 0 ? n_leaf * 8ull : 1));
        if (sup_raw) {
            int64_t got2 = b64_decode(b64, b64_len, sup_raw);
            if (got2 < 0 || (uint64_t)got2 != n_leaf * 8ull) {
                free(sup_raw); sup_raw = NULL;   /* 非法 support → 视为缺省 */
            } else {
                support = (double*)sup_raw;
            }
        }
    }

    size_t npix = (size_t)c->rev_width * (size_t)c->rev_height;
    size_t out_b = (out_fp64 > 0.5) ? sizeof(double) : sizeof(float);
    void* signal_out = calloc(npix ? npix : 1, out_b);
    void* coverage_out = calloc(npix ? npix : 1, out_b);
    double* sip_a = NULL; double* sip_b = NULL;
    double* sip_ap = NULL; double* sip_bp = NULL;
    if (!signal_out || !coverage_out) goto rev_fail_nomem;

    {
        HpReverseDrizzleInput rin;
        memset(&rin, 0, sizeof(rin));
        rin.nside = (int32_t)nside;
        rin.nested = (nested != 0.0) ? 1 : 0;
        rin.target_width = c->rev_width;
        rin.target_height = c->rev_height;
        rin.pixfrac = pixfrac;
        rin.output_fp64 = (out_fp64 > 0.5) ? 1 : 0;
        rin.crval[0] = crval[0]; rin.crval[1] = crval[1];
        rin.crpix[0] = crpix[0]; rin.crpix[1] = crpix[1];
        rin.cd[0] = cd[0]; rin.cd[1] = cd[1]; rin.cd[2] = cd[2]; rin.cd[3] = cd[3];
        rin.sip_order = sip_order;
        rin.sip_ap_order = sip_ap_order;
        rin.leaf_ipix = (const uint64_t*)ipix_raw;
        rin.n_leaf = (int64_t)n_leaf;
        if (sig_is_f64) rin.leaf_signal_f64 = (const double*)sig_raw;
        else rin.leaf_signal_f32 = (const float*)sig_raw;
        rin.leaf_support = support;
        rin.no_data_as_zero = (no_data_as_zero != 0.0) ? 1 : 0;
        /* SIP 系数 (可选: 定长数组 double[36], P0-1 order≤5; 缺省全 0) */
        int n36 = 0;
        sip_a = json_get_f64_array(manifest, "sip_a", &n36);
        if (sip_a && n36 != 36) { free(sip_a); sip_a = NULL; }
        if (sip_a) {
            sip_b = json_get_f64_array(manifest, "sip_b", &n36);
            if (sip_b && n36 != 36) { free(sip_b); sip_b = NULL; }
            sip_ap = json_get_f64_array(manifest, "sip_ap", &n36);
            if (sip_ap && n36 != 36) { free(sip_ap); sip_ap = NULL; }
            sip_bp = json_get_f64_array(manifest, "sip_bp", &n36);
            if (sip_bp && n36 != 36) { free(sip_bp); sip_bp = NULL; }
            if (sip_a) memcpy(rin.sip_a, sip_a, sizeof(rin.sip_a));
            if (sip_b) memcpy(rin.sip_b, sip_b, sizeof(rin.sip_b));
            if (sip_ap) memcpy(rin.sip_ap, sip_ap, sizeof(rin.sip_ap));
            if (sip_bp) memcpy(rin.sip_bp, sip_bp, sizeof(rin.sip_bp));
        }

        HpReverseDrizzleResult rres;
        memset(&rres, 0, sizeof(rres));
        int rc = hp_drizzle_reverse_run(&rin, signal_out, coverage_out, &rres);

        inst->legacy_last_code = rc;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s", ASTROCS_DRIZZLE_OP_REVERSE);

        free(sip_a); free(sip_b); free(sip_ap); free(sip_bp);
        free(sup_raw); free(ipix_raw); free(sig_raw);
        free(crval); free(crpix); free(cd);

        if (rc != 0) {
            free(signal_out); free(coverage_out);
            char msg[640];
            inst->last_status = drz_legacy_status(rc, msg, sizeof(msg), err,
                                                  rres.error_msg);
            return inst->last_status;
        }

        /* 输出 manifest (统计 + 平面 base64) */
        uint64_t sig_b64_cap = b64_encoded_len((uint64_t)npix * out_b);
        uint64_t total = 768u + sig_b64_cap * 2ull;
        char* buf = (char*)malloc((size_t)total + 1);
        if (!buf) {
            free(signal_out); free(coverage_out);
            efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                  ACS_DIAG_ECODE_NONE, "drizzle: reverse out alloc failed");
            inst->last_status = ACS_ERR_NOMEM;
            return ACS_ERR_NOMEM;
        }
        char* w = buf;
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
            "{\"op\":\"reverse\",\"width\":%d,\"height\":%d,\"nside\":%d,"
            "\"nested\":%d,\"pixfrac\":",
            (int)c->rev_width, (int)c->rev_height, (int32_t)nside, rin.nested);
        json_append_f64(&w, pixfrac);
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
            ",\"output_fp64\":%d,\"n_source_leaf\":%lld,"
            "\"n_target_pixel_touched\":%lld,\"n_candidates\":%lld,"
            "\"n_overlaps\":%lld,\"n_invalid_ipix\":%lld,\"n_nonfinite\":%lld,"
            "\"n_skipped_outside\":%lld,\"total_signal_in\":",
            (int)out_fp64, (long long)rres.n_source_leaf,
            (long long)rres.n_target_pixel_touched, (long long)rres.n_candidates,
            (long long)rres.n_overlaps, (long long)rres.n_invalid_ipix,
            (long long)rres.n_nonfinite, (long long)rres.n_skipped_outside);
        json_append_f64(&w, rres.total_signal_in);
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
            ",\"total_signal_out\":");
        json_append_f64(&w, rres.total_signal_out);
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
            ",\"signal_plane_base64\":\"");
        b64_encode((const uint8_t*)signal_out, (uint64_t)npix * out_b, w);
        w += sig_b64_cap - 1;
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)),
            ",\"coverage_plane_base64\":\"");
        b64_encode((const uint8_t*)coverage_out, (uint64_t)npix * out_b, w);
        w += sig_b64_cap - 1;
        w += snprintf(w, (size_t)(total + 1 - (size_t)(w - buf)), "\"}");
        free(signal_out); free(coverage_out);

        acs_status ret = strbuf_write(out_manifest_json, buf, (uint64_t)(w - buf), err);
        free(buf);
        inst->exec_count++;
        inst->last_status = ret;
        return ret;
    }

rev_fail_nomem:
    free(sup_raw); free(ipix_raw); free(sig_raw);
    free(crval); free(crpix); free(cd);
    free(signal_out); free(coverage_out);
    efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
          ACS_DIAG_ECODE_NONE, "drizzle: reverse plane alloc failed");
    inst->last_status = ACS_ERR_NOMEM;
    return ACS_ERR_NOMEM;
}

/* ═══════════════════ 9. execute 编排 (状态机 + 入口取消) ═══════════════════ */

static acs_status drz_execute(acs_module_instance_v1* inst_raw,
                              acs_str_v1 input_manifest_json,
                              acs_str_v1 config_json,
                              acs_strbuf_v1* out_manifest_json,
                              acs_error_info_v1* err) {
    drz_inst* inst = (drz_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state != ACS_LC_STATE_CREATED || inst->executing) {
        efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
              ACS_DIAG_ECODE_ILLEGAL_STATE, "drizzle: instance busy/state");
        return ACS_ERR_STATE;
    }
    /* 入口取消检查点 (legacy 无中断点; inspect cancel_support=entry) */
    if (inst->cancel_req ||
        (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled &&
         inst->host->cancel->is_cancelled(inst->host->cancel->user_data))) {
        efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
              ACS_DIAG_ECODE_NONE, "drizzle: cancel requested before execute");
        inst->last_status = ACS_ERR_CANCELLED;
        return ACS_ERR_CANCELLED;
    }
    const char* mjson = input_manifest_json.data;
    if (!mjson || input_manifest_json.size == 0) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
              ACS_DIAG_ECODE_NULL_CONFIG, "drizzle: empty input manifest");
        return ACS_ERR_PARAM;
    }
    drz_cfg c;
    acs_status st = drz_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) return st;

    inst->executing = 1;
    inst->state = ACS_LC_STATE_EXECUTING;
    if (strcmp(c.op, ASTROCS_DRIZZLE_OP_DRIZZLE) == 0)
        st = drz_execute_drizzle(inst, mjson, &c, out_manifest_json, err);
    else
        st = drz_execute_reverse(inst, mjson, &c, out_manifest_json, err);
    drz_cfg_free(&c);
    inst->executing = 0;
    if (inst->state == ACS_LC_STATE_EXECUTING)
        inst->state = ACS_LC_STATE_CREATED;
    return st;
}

/* ═══════════════════ 10. 静态 vtable 与唯一导出入口 ═══════════════════ */

static const acs_module_api_v1 g_drz_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    drz_describe,
    drz_validate_config,
    drz_plan,
    drz_create,
    drz_execute,
    drz_inspect,
    drz_request_cancel,
    drz_destroy
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
    *out_api = &g_drz_api;
    return ACS_OK;
}
#ifdef __cplusplus
} /* extern "C" */
#endif
