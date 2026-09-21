/* module_entry.cpp - astrocs.p1.noise 模块 C ABI v1 adapter
 *
 * 迁移任务 P1-NOISE-IMPL; 对齐先例 lib/algorithms/calibration/src/module_entry.cpp
 * (P1-CAL-IMPL, adf820ac)、lib/algorithms/cosmetic/src/module_entry.cpp (P1-COS-IMPL,
 * 948dfcba)、lib/algorithms/drizzle/hips/src/module_entry.cpp (P1-HIPS-IMPL, 1959dc89)。
 * TU 以 C++ 编译 (生产头 snr_estimator.h 为 C++ extern "C" 头), 全部导出面
 * 经 extern "C" 保持 C ABI 不变; 唯一导出 astrocs_module_query_v1
 * (legacy snr_noise_* 七符号经链接 version-script/DEF 降 local, 见 CMakeLists)。
 *
 * 职责: 把 lib/algorithms/noise_snr/cpp/src/noise_model.cpp 的 legacy C 接口
 *   (API-NOISE-001 七导出: snr_noise_model_v1(+_f64)/_default_config/_fill/
 *   _free/snr_noise_scale_law/snr_noise_gain_variance) 包装成三操作模块
 *   vtable (词表 types.h):
 *     estimate_noise_model = build(+可选内联 fill)+free 单事务 (README §5
 *       调用时序 build → fill → free; g_model_floor 注册表在事务内全程
 *       有效, 内联 fill 使用真实 build floor —— 与 direct 通道 bitwise 一致);
 *     fill_noise_field     = fill 独立通道 (模型 round-trip 重建影子实例,
 *       注册表 miss → floor 回退 1e-12, DISP-NOISE-002 现状忠实跨 ABI);
 *     noise_diagnostic     = scale_law / gain_variance 标量诊断 (tiny 串行)。
 *
 * 科学纪律: scientific_change=false —— 本文件只做 eng/packaging/config/manifest 解析、
 *   base64 平面编解码、线程租约注入 (host executor lease)、结果转发;
 *   不触碰 blank-sky 稳健方差公式 (1.482602218505602·MAD、5σ cosmic 裁剪、
 *   fixed conservative 掩膜、LS 平面场)、scale law、Poisson 诊断公式
 *   (ALG-NOISE-001..003, 生产源零改动)。kTrimMeanToSigma (noise_model.cpp:37)
 *   挂账项不触碰 (本 TU 不引用该常数)。
 *
 * 线程纪律: 事务内串行 (生产 noise_model_impl 无 omp pragma, 单线程现状;
 *   DISP 面并行轴整改归 INT, 本迁移不改归约顺序 —— 约束 E.1/E.3);
 *   execute 期 host executor 硬租约 acquire(1) 占坑 (cpu_heavy; executor
 *   缺失/acquire 失败 → ACS_ERR_BUDGET detail 105, P1-CAL/COS 同款,
 *   禁无租约运行亦禁降级单线程借口 —— 本事务本就串行, 租约为预算占坑);
 *   事务级并行 = host executor 跨 instance 并发。
 *
 * 取消纪律: entry 粒度 (DISP-NOISE-004 登记生产无取消检查点, adapter 不在生产
 *   调用内部注入检查 —— 科学面零改动; P1-COS 取消检查点宏同款三查: entry
 *   cancel 预检 → 硬租约 acquire → manifest 解析 → 科学调用前最后一查)。
 *   失败/取消路径输出面零写入 (事务性, P1-CAL/COS 同款)。
 */
#define _POSIX_C_SOURCE 200809L

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/noise/types.h"
#include "snr_estimator.h"        /* legacy 生产 C API (extern "C"; 七导出) */

/* ═══════════════════ 1. 基础 helper (对齐 cal/cos/hips 先例) ═══════════════════ */

static const char kModuleId[]  = ASTROCS_NOISE_MODULE_ID;
static const char kVersion[]   = "1.0";
static const char kBuildId[]   = ASTROCS_NOISE_BUILD_ID;
static const char kSciId[]     = ASTROCS_NOISE_SCI_ID;
static const char kAlgId[]     = ASTROCS_NOISE_ALG_ID;
static const char kApiId[]     = ASTROCS_NOISE_API_ID;

/* 异常屏障细分码: types.h noise_ecode NOISE_ECODE_EXCEPTION (=130, P1-CAL/COS
 * 同款: 全 vtable try/catch → ACS_ERR_EXCEPTION + detail 130) */

static acs_str_v1 acs_str_from(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}

/* 动态错误消息缓冲 (thread_local; message_utf8 借入语义要求消息存活至
 * host 读取 —— 先例 cal/cos/hips 统一 thread_local 静态缓冲)。 */
static thread_local char noise_err_msg[1024];

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

static const char* noise_msgf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(noise_err_msg, sizeof(noise_err_msg), fmt, ap);
    va_end(ap);
    return noise_err_msg;
}

/* strbuf 写 N 字节 (lifecycle_v1.h 冻结截断语义: size=所需; cap=0 且
 * data=NULL 只问尺寸; 不足 → PARAM + BUFFER_TOO_SMALL, 尽力写前缀+NUL)。 */
static acs_status strbuf_write(acs_strbuf_v1* out, const char* data, uint64_t n,
                               acs_error_info_v1* err) {
    if (!out) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CALLBACK, "noise: null output buffer");
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
                     "noise: output buffer too small");
    }
    return ACS_OK;
}

static acs_status strbuf_write_cstr(acs_strbuf_v1* out, const char* s,
                                    acs_error_info_v1* err) {
    return strbuf_write(out, s, s ? (uint64_t)strlen(s) : 0, err);
}

/* ───────── mini JSON 读取器 (eng/packaging/config/manifest 借入; 只读不分配) ───────── */

static int json_find_str(const char* obj, const char* key,
                         const char** out_val, uint64_t* out_len) {
    const char* dummy_v = NULL;
    uint64_t dummy_n = 0;
    if (!out_val) out_val = &dummy_v;
    if (!out_len) out_len = &dummy_n;
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

/* 数值读取: 不预检首字符 (允许 nan/inf/NaN/Inf 字面量, DISP-NOISE-005
 * scale_law alpha 直传语义需要); end==p 才算未找到。 */
static double json_get_f64(const char* obj, const char* key, int* found) {
    if (found) *found = 0;
    if (!obj || !key) return 0.0;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(obj, pat);
    if (!p) return 0.0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    char* end = NULL;
    double d = strtod(p, &end);
    if (end == p) return 0.0;
    if (found) *found = 1;
    return d;
}

static uint64_t json_get_u64(const char* obj, const char* key, int* found) {
    if (found) *found = 0;
    if (!obj || !key) return 0;
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(obj, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0;
    char* end = NULL;
    /* 负数: 借道 double 再判负 (u64 字段负值=未提供) */
    if (*p == '-') {
        double d = strtod(p, &end);
        if (end == p || d < 0.0) return 0;
        if (found) *found = 1;
        return (uint64_t)d;
    }
    unsigned long long u = strtoull(p, &end, 10);
    if (end == p) return 0;
    if (found) *found = 1;
    return (uint64_t)u;
}

/* base64 解码 (RFC 4648 标准; 失败返回 0, 不产生部分输出; hips 同款) */
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

/* ───────── base64 编码 (输出平面; RFC 4648 标准) ───────── */

static uint64_t b64_enc_len(uint64_t bytes) {
    return 4ull * ((bytes + 2ull) / 3ull);
}

static void b64_encode(const uint8_t* src, uint64_t n, char* dst) {
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint64_t i = 0, o = 0;
    while (i + 3 <= n) {
        uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) |
                     (uint32_t)src[i + 2];
        dst[o++] = T[(v >> 18) & 63]; dst[o++] = T[(v >> 12) & 63];
        dst[o++] = T[(v >> 6) & 63];  dst[o++] = T[v & 63];
        i += 3;
    }
    if (i < n) {
        uint32_t v = (uint32_t)src[i] << 16;
        int rem = (int)(n - i);
        if (rem == 2) v |= (uint32_t)src[i + 1] << 8;
        dst[o++] = T[(v >> 18) & 63]; dst[o++] = T[(v >> 12) & 63];
        dst[o++] = (rem == 2) ? T[(v >> 6) & 63] : '=';
        dst[o++] = '=';
    }
    dst[o] = '\0';
}

static void json_append_escaped(char** w, const char* s) {
    char* p = *w;
    for (const char* q = s; *q; ++q) {
        if (*q == '"' || *q == '\\') *p++ = '\\';
        *p++ = *q;
    }
    *w = p;
}

/* 输出 manifest 平面字段: "key":"<b64>" (f64/f32 数组 native 字节序)。
 * 失败=分配失败返回 0 (调用方走 NOMEM), 成功返回 1。 */
static int json_append_plane(char** w, const char* key,
                             const void* data, uint64_t bytes) {
    uint64_t el = b64_enc_len(bytes);
    char* buf = (char*)malloc((size_t)(el + 1));
    if (!buf) return 0;
    b64_encode((const uint8_t*)data, bytes, buf);
    char* p = *w;
    p += snprintf(p, 64, "\"%s\":\"", key);
    memcpy(p, buf, (size_t)el);
    p += el;
    *p++ = '"';
    *p++ = ',';
    *p = '\0';
    *w = p;
    free(buf);
    return 1;
}

/* f64 标量 bitwise 通道: scalars_base64 = f64×K native 数组 (对拍主口径);
 * plane_tail 变体写闭合 "}" (diag 标量 op 输出收口用) */
static int json_append_scalars(char** w, const double* v, uint32_t k) {
    return json_append_plane(w, NOISE_O_KEY_SCALARS_B64, v,
                             (uint64_t)k * sizeof(double));
}

/* f32/f64 平面 base64 尾部: 写 "key":"<b64>"} (闭合 JSON; diag 标量输出) */
static int json_append_plane_tail(char** w, const char* key,
                                  const void* data, uint64_t bytes) {
    uint64_t el = b64_enc_len(bytes);
    char* buf = (char*)malloc((size_t)(el + 1));
    if (!buf) return 0;
    b64_encode((const uint8_t*)data, bytes, buf);
    char* p = *w;
    p += snprintf(p, 64, "\"%s\":\"", key);
    memcpy(p, buf, (size_t)el);
    p += el;
    *p++ = '"';
    *p++ = '}';
    *p = '\0';
    *w = p;
    free(buf);
    return 1;
}

/* ═══════════════════ 2. config (create/execute; 词表 types.h) ═══════════════════ */

typedef struct {
    char op[32];
    int dtype;                 /* 0=f32 (snr_noise_model_v1) 1=f64 (_f64) */
    uint64_t h, w;             /* >=1 (build/fill 形状; plan 期 work_units 事实) */
    int fill_variance;         /* 0/1 build 内联 fill / fill op 输出 */
    int fill_ivar;             /* 0/1 */
    /* SnrNoiseModelConfig 副本 (default_config 填充 + 显式键覆盖) */
    SnrNoiseModelConfig cfg;
    int cfg_present;           /* config 显式提供过任一 cfg 键 (记录) */
    uint64_t variance_floor_given; /* 显式 variance_floor 回显用 */
    double variance_floor;
    /* diagnostic op */
    char subop[32];
    double alpha;
    int alpha_given, variance_given, ivar_given;
    double variance_in, ivar_in, signal_in, gain_in, rn_in;
    int signal_given, gain_given;
    uint64_t max_workers;      /* 0 = 无 config 上限 (事务恒串行, 仅作回显) */
} noise_cfg;

static void noise_cfg_free(noise_cfg* c) {
    (void)c;   /* POD; 无堆所有权 */
}

static acs_status noise_cfg_parse(const char* json, noise_cfg* c,
                                  acs_error_info_v1* err) {
    memset(c, 0, sizeof(*c));
    if (!json || !json[0]) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_NULL_CONFIG, "noise: empty config");
    }
    if (!json_copy_str(json, NOISE_CFG_KEY_OP, c->op, sizeof(c->op))) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     ACS_DIAG_ECODE_CONFIG_SCHEMA, "noise: config missing op");
    }
    if (strcmp(c->op, ASTROCS_NOISE_OP_ESTIMATE) != 0 &&
        strcmp(c->op, ASTROCS_NOISE_OP_FILL) != 0 &&
        strcmp(c->op, ASTROCS_NOISE_OP_DIAG) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     NOISE_ECODE_OP_UNKNOWN, "noise: op outside vocabulary");
    }

    /* ── estimate_noise_model / fill_noise_field: 形状键 ── */
    if (strcmp(c->op, ASTROCS_NOISE_OP_DIAG) != 0) {
        int f = 0;
        uint64_t dt = json_get_u64(json, NOISE_CFG_KEY_DTYPE, &f);
        if (f && dt > 1ull) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_TYPE,
                         "noise: dtype invalid (0=f32 1=f64)");
        }
        if (!f) {
            if (strcmp(c->op, ASTROCS_NOISE_OP_ESTIMATE) == 0) {
                return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                             NOISE_ECODE_PARAM_MISSING,
                             "noise: config missing dtype (estimate op)");
            }
            dt = 0;   /* fill op 不需要 dtype (输出恒 f32) */
        }
        c->dtype = (int)dt;
        uint64_t h = json_get_u64(json, NOISE_CFG_KEY_H, &f);
        if (!f) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_MISSING, "noise: config missing h");
        }
        uint64_t w = json_get_u64(json, NOISE_CFG_KEY_W, &f);
        if (!f) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_MISSING, "noise: config missing w");
        }
        if (h == 0 || w == 0 || h > 0x7fffffffull || w > 0x7fffffffull) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_RANGE,
                         "noise: h/w outside production int domain");
        }
        c->h = h; c->w = w;
        if (json_get_u64(json, NOISE_CFG_KEY_FILL_VARIANCE, &f) && f)
            c->fill_variance = 1;
        if (json_get_u64(json, NOISE_CFG_KEY_FILL_IVAR, &f) && f)
            c->fill_ivar = 1;
        if (strcmp(c->op, ASTROCS_NOISE_OP_FILL) == 0 &&
            !c->fill_variance && !c->fill_ivar) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_RANGE,
                         "noise: fill op requires fill_variance or fill_ivar");
        }

        /* SnrNoiseModelConfig: 生产 default_config 填充 (零复制默认面,
         * 与生产 cfg=NULL 路径 bitwise 等价) + 显式键覆盖 */
        if (snr_noise_model_v1_default_config(&c->cfg) != 0) {
            return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                         NOISE_ECODE_EXCEPTION, "noise: default_config rejected");
        }
        double d;
        if ((d = json_get_f64(json, NOISE_CFG_KEY_PATCH_GX, &f), f)) {
            c->cfg.patch_grid_x = (int)d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_PATCH_GY, &f), f)) {
            c->cfg.patch_grid_y = (int)d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_MASK_R0, &f), f)) {
            c->cfg.source_mask_radius_px = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_MASK_SCALE, &f), f)) {
            c->cfg.mask_radius_scale = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_GAIN, &f), f)) {
            c->cfg.gain_e_per_adu = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_READNOISE, &f), f)) {
            c->cfg.read_noise_e = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_SATURATION, &f), f)) {
            c->cfg.saturation_level = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_CLIP_SIGMA, &f), f)) {
            c->cfg.cosmic_clip_sigma = d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_MIN_SAMPLES, &f), f)) {
            c->cfg.min_patch_samples = (int)d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_CLIP_ROUNDS, &f), f)) {
            c->cfg.max_clip_rounds = (int)d; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_USE_GAIN_MODEL, &f), f)) {
            c->cfg.use_gain_model = (d != 0.0) ? 1u : 0u; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_SPATIAL_FIELD, &f), f)) {
            c->cfg.enable_spatial_field = (d != 0.0) ? 1u : 0u; c->cfg_present = 1; }
        if ((d = json_get_f64(json, NOISE_CFG_KEY_VARIANCE_FLOOR, &f), f)) {
            /* FIX-405 G3-6 (钳位 fail-open → fail-closed): variance_floor 是保护
             * 下限, 非法值会让 std::max 钳位静默失效（NaN 比较恒 false; ≤0 等于
             * 不设防）。此处显式拒绝, 不把非法 floor 传进科学层。 */
            if (!std::isfinite(d) || d <= 0.0) {
                return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                             ACS_DIAG_ECODE_NONE,
                             noise_msgf("noise: %s must be finite and > 0 (got %g)",
                                        NOISE_CFG_KEY_VARIANCE_FLOOR, d));
            }
            c->cfg.variance_floor = d; c->cfg_present = 1;
            c->variance_floor_given = 1; c->variance_floor = d; }
        c->max_workers = json_get_u64(json, NOISE_CFG_KEY_MAX_WORKERS, &f);
        return ACS_OK;
    }

    /* ── noise_diagnostic: subop 词表 ── */
    if (!json_copy_str(json, NOISE_CFG_KEY_DIAG_SUBOP, c->subop, sizeof(c->subop))) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     NOISE_ECODE_PARAM_MISSING,
                     "noise: diagnostic missing subop");
    }
    if (strcmp(c->subop, "scale_law") == 0) {
        c->alpha = json_get_f64(json, NOISE_CFG_KEY_ALPHA, &c->alpha_given);
        if (!c->alpha_given) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_MISSING,
                         "noise: scale_law missing alpha");
        }
        /* alpha 值域零校验 (NaN/负直传 = 生产 DISP-NOISE-005 现状) */
        c->variance_in = json_get_f64(json, NOISE_CFG_KEY_VARIANCE,
                                      &c->variance_given);
        c->ivar_in = json_get_f64(json, NOISE_CFG_KEY_IVAR, &c->ivar_given);
        return ACS_OK;
    }
    if (strcmp(c->subop, "gain_variance") == 0) {
        c->signal_in = json_get_f64(json, NOISE_CFG_KEY_SIGNAL,
                                    &c->signal_given);
        if (!c->signal_given) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_MISSING,
                         "noise: gain_variance missing signal");
        }
        c->gain_in = json_get_f64(json, NOISE_CFG_KEY_GAIN, &c->gain_given);
        if (!c->gain_given) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         NOISE_ECODE_PARAM_MISSING,
                         "noise: gain_variance missing gain_e_per_adu");
        }
        c->rn_in = json_get_f64(json, NOISE_CFG_KEY_READNOISE, &c->alpha_given);
        return ACS_OK;
    }
    return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                 NOISE_ECODE_OP_UNKNOWN,
                 "noise: diagnostic subop outside vocabulary");
}

/* ═══════════════════ 3. manifest 解码 (execute 输入; 顶层平铺 v1) ═══════════════════ */

typedef struct {
    uint8_t* data;             /* h·w·dtype (f32/f64 native) */
    uint64_t n_data;
    uint8_t* mask;             /* 可选; h·w f32 (≠0=源) */
    uint64_t n_mask;
    uint8_t* star_x;           /* 可选; n·f64 */
    uint8_t* star_y;           /* 可选; n·f64 */
    uint64_t n_stars;
    /* fill_noise_field: 模型 round-trip 字段 */
    uint8_t* ctrl_x; uint8_t* ctrl_y; uint8_t* ctrl_sig;
    uint8_t* ctrl_var; uint8_t* ctrl_ivar;
    uint64_t n_ctrl;
} noise_rows;

static void noise_rows_free(noise_rows* r) {
    if (!r) return;
    free(r->data); free(r->mask);
    free(r->star_x); free(r->star_y);
    free(r->ctrl_x); free(r->ctrl_y); free(r->ctrl_sig);
    free(r->ctrl_var); free(r->ctrl_ivar);
    memset(r, 0, sizeof(*r));
}

static acs_status noise_rows_parse_estimate(const char* manifest,
                                            const noise_cfg* c,
                                            noise_rows* r,
                                            acs_error_info_v1* err) {
    memset(r, 0, sizeof(*r));
    const uint64_t hw = c->h * c->w;
    const uint64_t bytes = (c->dtype == 1) ? 8ull : 4ull;

    if (!json_get_b64(manifest, NOISE_M_KEY_DATA_B64, &r->data, &r->n_data)) {
        noise_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     NOISE_ECODE_B64_INVALID, "noise: data_base64 decode failed");
    }
    if (!r->data || r->n_data != hw * bytes) {
        uint64_t got = r->data ? r->n_data : 0;
        noise_rows_free(r);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     got ? NOISE_ECODE_MANIFEST_DIMS : NOISE_ECODE_MANIFEST_MISSING,
                     noise_msgf("noise: data plane size mismatch: got %llu want %llu",
                                (unsigned long long)got,
                                (unsigned long long)(hw * bytes)));
    }
    /* source_mask 可选 (f32 h·w; ≠0=源; DISP-NOISE-006 互斥语义生产直传) */
    if (json_find_str(manifest, NOISE_M_KEY_MASK_B64, NULL, NULL)) {
        if (!json_get_b64(manifest, NOISE_M_KEY_MASK_B64, &r->mask, &r->n_mask) ||
            !r->mask || r->n_mask != hw * 4ull) {
            uint64_t got = r->mask ? r->n_mask : 0;
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         got ? NOISE_ECODE_MANIFEST_DIMS : NOISE_ECODE_B64_INVALID,
                         noise_msgf("noise: source_mask plane mismatch: got %llu want %llu",
                                    (unsigned long long)got,
                                    (unsigned long long)(hw * 4ull)));
        }
    }
    /* star 通道可选 (f64×N 等长; 非有限坐标生产跳过) */
    int has_x = json_find_str(manifest, NOISE_M_KEY_STAR_X_B64, NULL, NULL);
    int has_y = json_find_str(manifest, NOISE_M_KEY_STAR_Y_B64, NULL, NULL);
    if (has_x || has_y) {
        if (!has_x || !has_y) {
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         NOISE_ECODE_MANIFEST_MISSING,
                         "noise: star_x/star_y must be provided together");
        }
        if (!json_get_b64(manifest, NOISE_M_KEY_STAR_X_B64, &r->star_x,
                          &r->n_stars) || !r->star_x) {
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         NOISE_ECODE_B64_INVALID, "noise: star_x decode failed");
        }
        uint64_t ny = 0;
        if (!json_get_b64(manifest, NOISE_M_KEY_STAR_Y_B64, &r->star_y, &ny) ||
            !r->star_y || ny != r->n_stars) {
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         NOISE_ECODE_MANIFEST_DIMS,
                         "noise: star_y missing/length mismatch");
        }
        if (r->n_stars % 8ull != 0ull || r->n_stars / 8ull > 0x7fffffffull) {
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         NOISE_ECODE_MANIFEST_DIMS,
                         "noise: star plane size invalid");
        }
        int f = 0;
        uint64_t n_stars = json_get_u64(manifest, NOISE_M_KEY_N_STARS, &f);
        r->n_stars = r->n_stars / 8ull;
        if (f && n_stars != r->n_stars) {
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         NOISE_ECODE_MANIFEST_DIMS,
                         "noise: n_stars != star plane length");
        }
    }
    return ACS_OK;
}

static acs_status noise_rows_parse_fill(const char* manifest,
                                        const noise_cfg* c,
                                        noise_rows* r,
                                        acs_error_info_v1* err) {
    memset(r, 0, sizeof(*r));
    (void)c;
    int f = 0;
    uint64_t n = json_get_u64(manifest, NOISE_M_KEY_N_CTRL, &f);
    if (!f) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     NOISE_ECODE_MANIFEST_MISSING,
                     "noise: fill manifest missing n_control_points");
    }
    if (n > 0x7fffffffull) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     NOISE_ECODE_PARAM_RANGE,
                     "noise: n_control_points outside production int domain");
    }
    r->n_ctrl = n;
    const uint64_t want = n * 8ull;
    struct { const char* key; uint8_t** dst; } planes[5] = {
        { NOISE_M_KEY_CTRL_X_B64,    &r->ctrl_x },
        { NOISE_M_KEY_CTRL_Y_B64,    &r->ctrl_y },
        { NOISE_M_KEY_CTRL_SIG_B64,  &r->ctrl_sig },
        { NOISE_M_KEY_CTRL_VAR_B64,  &r->ctrl_var },
        { NOISE_M_KEY_CTRL_IVAR_B64, &r->ctrl_ivar },
    };
    for (int i = 0; i < 5; ++i) {
        uint64_t ln = 0;
        if (!json_get_b64(manifest, planes[i].key, planes[i].dst, &ln) ||
            !*planes[i].dst || ln != want) {
            uint64_t got = *planes[i].dst ? ln : 0;
            noise_rows_free(r);
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         got ? NOISE_ECODE_MANIFEST_DIMS : NOISE_ECODE_MANIFEST_MISSING,
                         noise_msgf("noise: fill plane size mismatch (%s): got %llu want %llu",
                                    planes[i].key, (unsigned long long)got,
                                    (unsigned long long)want));
        }
    }
    return ACS_OK;
}

/* ═══════════════════ 4. describe / validate_config / plan ═══════════════════ */

static acs_status noise_describe(const acs_module_api_v1* self,
                                 acs_str_v1 module_id,
                                 acs_module_descriptor_v1* out_desc) {
    try {
        (void)self;
        if (!out_desc) return ACS_ERR_PARAM;
        /* MOD-001 加载验证对齐: ABI-003 安全 loader 以 empty module_id 调 describe
         * (secure_loader.c §5 既成合同; BLD-003 noop 先例同语义), 空=不指名, 直接
         * 返回静态描述; 非空仍严格校验 (错 ID → MISMATCH, 既有 adapter 测试锚不变)。
         * 空 ID 拒绝曾使科学 DLL 在安装树内被自家 loader 必拒 (DESCRIPTOR_MISMATCH),
         * 本行为修复经 eng/tests/abi/mod001_install_load_check.py 安装树逐 unit 加载闭环。 */
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
        out_desc->config_schema_ver = ASTROCS_NOISE_CONFIG_SCHEMA_VER;
        out_desc->execution_class = 0;    /* cpu_heavy (module.yaml resource_class) */
        out_desc->parallel_ok = 0;        /* 事务内串行 (生产单线程现状, E.3);
                                             并行度=host executor 跨 instance */
        out_desc->flags = 0;
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_EXCEPTION;
    }
}

static acs_status noise_validate_config(const acs_module_api_v1* self,
                                        acs_str_v1 config_json,
                                        acs_error_info_v1* err) {
    try {
        (void)self;
        noise_cfg tmp;
        acs_status st = noise_cfg_parse(config_json.data, &tmp, err);
        noise_cfg_free(&tmp);
        return st;
    } catch (...) {
        return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                     NOISE_ECODE_EXCEPTION, "exception in validate_config");
    }
}

/* plan (module 级; work_units=数据事实, 不执行科学计算; P1-CAL 同款 9 键):
 *   estimate_noise_model / fill_noise_field: work_units = h·w 像素
 *     (blank-sky patch 扫描与 fill 逐像素场均为 O(h·w), ALG-NOISE-001/002);
 *     事务内串行 (生产无 omp pragma, 归约顺序固定) → parallel_axis=instance;
 *     memory_bytes_estimate = h·w·(1+8) (掩膜 u8 + 全帧兜底样本 f64 上界,
 *     config 元数据推导, 无魔数);
 *     io_bytes_estimate = 0 (v1 inline base64, 无 host I/O; P1-CAL/COS 同款)。
 *   noise_diagnostic: work_units = 1 标量 (tiny 串行, RT-005 tiny 标记)。 */
static acs_status noise_plan(const acs_module_api_v1* self,
                             acs_str_v1 node_id,
                             acs_str_v1 config_json,
                             acs_strbuf_v1* out_plan_json,
                             acs_error_info_v1* err) {
    try {
        (void)self;
        if (!out_plan_json) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         ACS_DIAG_ECODE_NULL_CALLBACK, "noise: null plan buffer");
        }
        if (!config_json.data || config_json.size == 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         ACS_DIAG_ECODE_NULL_CONFIG, "noise: config required");
        }
        noise_cfg c;
        acs_status st = noise_cfg_parse(config_json.data, &c, err);
        if (st != ACS_OK) return st;

        char node[256];
        uint64_t node_len = node_id.size < sizeof(node) - 1 ? node_id.size
                                                            : sizeof(node) - 1;
        if (node_id.data && node_len) memcpy(node, node_id.data, (size_t)node_len);
        node[node_len] = '\0';

        const int is_diag = (strcmp(c.op, ASTROCS_NOISE_OP_DIAG) == 0);
        const int is_build = (strcmp(c.op, ASTROCS_NOISE_OP_ESTIMATE) == 0);
        uint64_t work_units = is_diag ? 1ull : (c.h * c.w);
        const char* work_unit = is_diag ? "scalar" : "pixel";
        const char* axis = is_diag ? "none" : "instance";
        uint64_t dtype_sz = (c.dtype == 1) ? 8ull : 4ull;
        /* build: 掩膜 u8 + 全帧兜底样本 f64 上界; fill: 输出 f32 平面;
         * diag: 标量 */
        uint64_t mem = is_diag ? 16ull
                               : (is_build ? c.h * c.w * (1ull + 8ull)
                                           : c.h * c.w * 4ull);

        char buf[768];
        int n = snprintf(buf, sizeof(buf),
            "{\"plan_version\":%u,\"module_id\":\"%s\",\"node_id\":\"",
            ASTROCS_NOISE_PLAN_VERSION, kModuleId);
        char* w = buf + n;
        json_append_escaped(&w, node);
        w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
            "\",\"op\":\"%s\",\"work_unit\":\"%s\",\"work_units\":%llu,"
            "\"parallel_axis\":\"%s\",\"min_workers\":1,"
            "\"memory_bytes_estimate\":%llu,\"io_bytes_estimate\":0,"
            "\"cancel_support\":\"entry\"}",
            c.op, work_unit, (unsigned long long)work_units, axis,
            (unsigned long long)mem);
        noise_cfg_free(&c);
        return strbuf_write_cstr(out_plan_json, buf, err);
    } catch (...) {
        return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                     NOISE_ECODE_EXCEPTION, "exception in plan");
    }
}

/* ═══════════════════ 5. create / destroy / inspect / cancel ═══════════════════ */

typedef struct {
    acs_head head;                 /* struct_size/abi_version (CAL 同款) */
    uint32_t state;                /* ACS_LC_STATE_* */
    const acs_host_api_v1* host;
    noise_cfg cfg;                 /* create 期校验副本 (execute 期以传入 config 重解析) */
    int executing;
    int cancel_req;
    uint64_t exec_count;
    acs_status last_status;
    char last_op[32];
    int last_rc;                   /* 生产 legacy 返回码 (0/1 成功面; 3 失败) */
    uint32_t last_workers;
    uint64_t last_n_ctrl;
    int last_floor_fallback;
} noise_inst;

static acs_status noise_create(const acs_module_api_v1* self,
                               acs_str_v1 config_json,
                               const acs_host_api_v1* host,
                               acs_module_instance_v1** out,
                               acs_error_info_v1* err) {
    try {
        (void)self;
        if (!out) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         ACS_DIAG_ECODE_NULL_CALLBACK, "noise: null out");
        }
        *out = NULL;
        if (!host || !host->allocator) {
            return efill(err, ACS_ERR_ABI_MISMATCH, ACS_ERR_DOMAIN_INTERNAL,
                         ACS_DIAG_ECODE_NULL_CALLBACK,
                         "noise: host allocator required");
        }
        noise_cfg c;
        acs_status st = noise_cfg_parse(config_json.data, &c, err);
        if (st != ACS_OK) { noise_cfg_free(&c); return st; }

        noise_inst* inst = (noise_inst*)host->allocator->alloc(
            host->allocator->user_data, (uint64_t)sizeof(noise_inst), 16);
        if (!inst) {
            noise_cfg_free(&c);
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE, "noise: inst alloc failed");
        }
        memset(inst, 0, sizeof(*inst));
        inst->head.struct_size = (uint32_t)sizeof(noise_inst);
        inst->head.abi_version = ACS_ABI_VERSION_V1;
        inst->state = ACS_LC_STATE_CREATED;
        inst->host = host;
        inst->cfg = c;
        inst->last_status = ACS_OK;
        inst->last_rc = 0;
        *out = (acs_module_instance_v1*)inst;
        return ACS_OK;
    } catch (...) {
        return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                     NOISE_ECODE_EXCEPTION, "exception in create");
    }
}

static acs_status noise_inspect(const acs_module_instance_v1* inst_raw,
                                acs_strbuf_v1* out_json,
                                acs_error_info_v1* err) {
    try {
        const noise_inst* inst = (const noise_inst*)inst_raw;
        if (!inst) return ACS_ERR_PARAM;
        char buf[640];
        snprintf(buf, sizeof(buf),
                 "{\"module_id\":\"%s\",\"state\":%u,\"executing\":%d,"
                 "\"cancel_req\":%d,\"exec_count\":%llu,\"last_status\":%d,"
                 "\"last_op\":\"%s\",\"last_legacy_rc\":%d,"
                 "\"last_workers\":%u,\"last_n_control_points\":%llu,"
                 "\"last_floor_fallback\":%d,"
                 "\"cancel_support\":\"entry\","
                 "\"threads_note\":\"single transaction serial "
                 "(production single-thread现状); "
                 "host executor lease x instance\"}",
                 kModuleId, (unsigned)inst->state, inst->executing,
                 inst->cancel_req, (unsigned long long)inst->exec_count,
                 (int)inst->last_status, inst->last_op,
                 inst->last_rc, (unsigned)inst->last_workers,
                 (unsigned long long)inst->last_n_ctrl,
                 inst->last_floor_fallback);
        return strbuf_write_cstr(out_json, buf, err);
    } catch (...) {
        return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                     NOISE_ECODE_EXCEPTION, "exception in inspect");
    }
}

static acs_status noise_request_cancel(acs_module_instance_v1* inst_raw) {
    try {
        noise_inst* inst = (noise_inst*)inst_raw;
        if (!inst) return ACS_ERR_PARAM;
        if (inst->state == ACS_LC_STATE_DESTROYED) return ACS_ERR_STATE;
        inst->cancel_req = 1;          /* 幂等单向置位 (ABI-002; CAL 同款) */
        if (inst->state == ACS_LC_STATE_EXECUTING) {
            inst->state = ACS_LC_STATE_CANCELLING;
        }
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_EXCEPTION;
    }
}

static void noise_destroy(acs_module_instance_v1* inst_raw) {
    noise_inst* inst = (noise_inst*)inst_raw;
    if (!inst) return;                 /* inst=NULL 空操作 */
    if (inst->head.abi_version != ACS_ABI_VERSION_V1) return;
    if (inst->state == ACS_LC_STATE_DESTROYED) return;   /* double → 忽略 */
    const acs_host_api_v1* host = inst->host;
    inst->state = ACS_LC_STATE_DESTROYED;
    if (host && host->allocator && host->allocator->free) {
        host->allocator->free(host->allocator->user_data, inst);
    }
}

/* ═══════════════════ 6. execute — 三操作 (单事务; 失败/取消零写入) ═══════════════════ */

/* host cancel 轮询 (入口三查 + 科学调用前 checkpoint; P1-COS 宏同款) */
#define NOISE_CANCELLED_NOW(inst)                                        \
    ((inst)->cancel_req ||                                               \
     ((inst)->host && (inst)->host->cancel &&                            \
      (inst)->host->cancel->is_cancelled &&                              \
      (inst)->host->cancel->is_cancelled(                                \
          (inst)->host->cancel->user_data)))

static acs_status noise_check_entry_cancel(noise_inst* inst,
                                           acs_error_info_v1* err) {
    if (inst->cancel_req) {
        inst->last_status = ACS_ERR_STATE;
        return efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
                     ACS_DIAG_ECODE_ILLEGAL_STATE,
                     "noise: instance cancel requested (no execute)");
    }
    if (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled &&
        inst->host->cancel->is_cancelled(inst->host->cancel->user_data)) {
        inst->last_status = ACS_ERR_CANCELLED;
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     ACS_DIAG_ECODE_NONE, "noise: cancel requested before execute");
    }
    return ACS_OK;
}

/* host executor 硬租约 (FORBID-003): acquire(1) 占坑; 缺失/失败 →
 * ACS_ERR_BUDGET detail 105 (cpu_heavy, P1-CAL/COS 同款, 禁无租约运行) */
static acs_status noise_acquire_lease(noise_inst* inst,
                                      const acs_executor_v1** ex_out,
                                      uint32_t* leased_out,
                                      acs_error_info_v1* err) {
    if (!inst->host || !inst->host->executor) {
        return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE, 105,
                     "cpu_heavy module requires host executor");
    }
    const acs_executor_v1* ex = inst->host->executor;
    if (!ex->acquire || !ex->release || ex->acquire(ex->user_data, 1) != 0) {
        return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE, 105,
                     "executor lease unavailable for cpu_heavy op");
    }
    *ex_out = ex;
    *leased_out = 1;
    return ACS_OK;
}

/* 生产 legacy 返回码 → status 映射 (rc 原样进 message "legacy_code=%d",
 * 不重解释科学语义; rc=1 完全退化=成功面科学结果不映射错误) */
static acs_status noise_legacy_status(int rc, int32_t* domain) {
    if (rc == 0 || rc == 1) { *domain = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION; return ACS_OK; }
    /* rc=3: 参数非法/内部异常 (生产 C ABI 门面 catch-all) → PARAM/BACKEND/120
     * (P1-CAL/COS legacy 拒绝同款域) */
    (void)domain;
    return ACS_ERR_PARAM;
}

/* ── 6a. estimate_noise_model: build(+可选内联 fill)+free 单事务 ── */

static acs_status noise_execute_estimate(noise_inst* inst, const char* manifest,
                                         const noise_cfg* c,
                                         acs_strbuf_v1* out_json,
                                         acs_error_info_v1* err) {
    const acs_executor_v1* ex = NULL;
    uint32_t leased = 0;
    acs_status st = noise_acquire_lease(inst, &ex, &leased, err);
    if (st != ACS_OK) return st;

    noise_rows rows;
    st = noise_rows_parse_estimate(manifest, c, &rows, err);
    if (st != ACS_OK) {
        ex->release(ex->user_data, leased);
        return st;
    }

    /* 科学调用前最后一查 (entry checkpoint; host cancel → 零写入返回) */
    if (NOISE_CANCELLED_NOW(inst)) {
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->last_status = ACS_ERR_CANCELLED;
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     ACS_DIAG_ECODE_NONE,
                     "noise: cancelled before scientific call");
    }

    /* 1) build (生产 snr_noise_model_v1/_f64; cfg=adapter 副本, 与生产
     *    cfg=NULL 路径 bitwise 等价 —— default_config 填充来源相同) */
    NoiseWeightModelV1 model;
    memset(&model, 0, sizeof(model));
    /* MASK-002 (claim SC-009): 本 adapter 的 manifest 合同只承载 star_x/star_y
     * 两个 plane (NOISE_M_KEY_STAR_X/Y_B64, 合同面未扩展) ⇒ 逐星 flux/fwhm
     * 以 NULL 传入, 模块按 SCI-NOISE-001 §5a 回调规则降级 (r=rmax → 天空预算
     * 收缩) 并置 MASK_LEGACY 位标。禁止在此凭空造 FWHM/F 冒充逐星半径。 */
    int rc = (c->dtype == 1)
                 ? snr_noise_model_v1_f64((const double*)rows.data,
                                          (int)c->h, (int)c->w,
                                          (const float*)rows.mask,
                                          (const double*)rows.star_x,
                                          (const double*)rows.star_y,
                                          nullptr,
                                          nullptr,
                                          (int)rows.n_stars, &c->cfg, &model)
                 : snr_noise_model_v1((const float*)rows.data,
                                      (int)c->h, (int)c->w,
                                      (const float*)rows.mask,
                                      (const double*)rows.star_x,
                                      (const double*)rows.star_y,
                                      nullptr,
                                      nullptr,
                                      (int)rows.n_stars, &c->cfg, &model);
    if (rc == 3) {
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->exec_count++;
        inst->last_rc = rc;
        inst->last_workers = leased;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_NOISE_OP_ESTIMATE);
        int32_t dom = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
        acs_status cst = noise_legacy_status(rc, &dom);
        inst->last_status = cst;
        return efill(err, cst, dom, NOISE_ECODE_LEGACY_REJECT,
                     noise_msgf("noise: legacy_code=%d at build", rc));
    }
    /* rc=0 (成功/退化兜底) 或 rc=1 (完全退化) 都继续 —— 科学结果面 */

    /* 2) 可选内联 fill (g_model_floor 注册表在事务内有效 → 真实 build
     *    floor, 与 direct 通道 bitwise 一致; DISP-NOISE-002 说明) */
    uint8_t* out_var = NULL;
    uint8_t* out_ivar = NULL;
    int do_fill = (c->fill_variance || c->fill_ivar);
    if (do_fill) {
        const uint64_t plane_bytes = c->h * c->w * 4ull;
        if (c->fill_variance) {
            out_var = (uint8_t*)malloc((size_t)plane_bytes);
            if (!out_var) {
                snr_noise_model_v1_free(&model);
                ex->release(ex->user_data, leased);
                noise_rows_free(&rows);
                inst->last_status = ACS_ERR_NOMEM;
                return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                             ACS_DIAG_ECODE_NONE,
                             "noise: fill variance plane alloc failed");
            }
        }
        if (c->fill_ivar) {
            out_ivar = (uint8_t*)malloc((size_t)plane_bytes);
            if (!out_ivar) {
                free(out_var);
                snr_noise_model_v1_free(&model);
                ex->release(ex->user_data, leased);
                noise_rows_free(&rows);
                inst->last_status = ACS_ERR_NOMEM;
                return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                             ACS_DIAG_ECODE_NONE,
                             "noise: fill ivar plane alloc failed");
            }
        }
        int frc = snr_noise_model_v1_fill(&model, (int)c->h, (int)c->w,
                                          (float*)out_var, (float*)out_ivar);
        if (frc != 0) {
            free(out_var); free(out_ivar);
            snr_noise_model_v1_free(&model);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->exec_count++;
            inst->last_rc = frc;
            inst->last_workers = leased;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                     ASTROCS_NOISE_OP_ESTIMATE);
            int32_t dom = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
            acs_status cst = noise_legacy_status(frc, &dom);
            inst->last_status = cst;
            return efill(err, cst, dom, NOISE_ECODE_LEGACY_REJECT,
                         noise_msgf("noise: legacy_code=%d at fill", frc));
        }
    }

    /* 3) 输出 manifest (统计 + 控制点平面 + scalars bitwise 通道)。
     * 大小预算: 骨架 1024 + 5×(b64(n·8)+key/引号 64B) + 2×(b64(h·w·4)+64B)
     * + 收口/NUL 余量 256 */
    const uint64_t n_ctrl = model.n_control_points;
    uint64_t total = 1024ull
                     + 5ull * (b64_enc_len(n_ctrl * 8ull) + 64ull)
                     + (do_fill ? (uint64_t)(c->fill_variance + c->fill_ivar) *
                                    (b64_enc_len(c->h * c->w * 4ull) + 64ull)
                                : 0ull)
                     + 256ull;
    char* buf = (char*)malloc((size_t)total);
    if (!buf) {
        free(out_var); free(out_ivar);
        snr_noise_model_v1_free(&model);
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "noise: out manifest alloc failed");
    }
    char* w = buf;
    /* FIX-405 G3-6: variance_floor 钳位状态显式登记（触发即状态, 不静默通过）。
     * variance_floor_clamped = build 期间被 floor 抬升的数值个数（全局兜底 +
     * 逐控制点）; variance_floor_status = "clamped" | "inactive"。 */
    const int64_t floor_clamped = snr_noise_model_v1_floor_clamp_count(&model);
    w += snprintf(w, (size_t)(total - (size_t)(w - buf)),
        "{\"op\":\"%s\",\"rc\":%d,\"dtype\":%d,\"h\":%llu,\"w\":%llu,"
        "\"n_control_points\":%llu,\"n_qualified_patches\":%u,"
        "\"n_rejected_patches\":%u,\"source\":%u,\"has_spatial_field\":%u,"
        "\"degenerate\":%u,\"%s\":%.17g,\"variance_floor_clamped\":%lld,"
        "\"variance_floor_status\":\"%s\",",
        ASTROCS_NOISE_OP_ESTIMATE, rc, c->dtype,
        (unsigned long long)c->h, (unsigned long long)c->w,
        (unsigned long long)n_ctrl,
        (unsigned)model.n_qualified_patches,
        (unsigned)model.n_rejected_patches,
        (unsigned)model.source, (unsigned)model.has_spatial_field,
        (unsigned)model.degenerate,
        NOISE_O_KEY_VARIANCE_FLOOR, c->cfg.variance_floor,
        (long long)floor_clamped,
        (floor_clamped > 0) ? "clamped" : "inactive");
    {
        /* 标量 bitwise 通道: [sigma_bg, variance_bg, ivar_bg, variance_floor 回显] */
        double sc[4];
        sc[0] = model.sigma_bg_global;
        sc[1] = model.variance_bg_global;
        sc[2] = model.ivar_bg_global;
        sc[3] = c->cfg.variance_floor;
        if (!json_append_scalars(&w, sc, 4)) {
            free(buf); free(out_var); free(out_ivar);
            snr_noise_model_v1_free(&model);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE,
                         "noise: scalars encode alloc failed");
        }
    }
    int ok = 1;
    if (n_ctrl > 0) {
        const void* planes[5] = { model.ctrl_x_px, model.ctrl_y_px,
                                  model.ctrl_sigma, model.ctrl_variance,
                                  model.ctrl_ivar };
        static const char* const keys[5] = {
            NOISE_O_KEY_CTRL_X_B64, NOISE_O_KEY_CTRL_Y_B64,
            NOISE_O_KEY_CTRL_SIG_B64, NOISE_O_KEY_CTRL_VAR_B64,
            NOISE_O_KEY_CTRL_IVAR_B64
        };
        for (int i = 0; i < 5 && ok; ++i)
            ok = json_append_plane(&w, keys[i], planes[i], n_ctrl * 8ull);
    }
    if (ok && do_fill) {
        const uint64_t plane_bytes = c->h * c->w * 4ull;
        if (c->fill_variance)
            ok = json_append_plane(&w, NOISE_O_KEY_OUT_VAR_B64, out_var,
                                   plane_bytes);
        if (ok && c->fill_ivar) {
            /* 最后一个字段用无尾逗号变体 */
            uint64_t el = b64_enc_len(plane_bytes);
            char* eb = (char*)malloc((size_t)(el + 1));
            if (!eb) {
                ok = 0;
            } else {
                b64_encode(out_ivar, plane_bytes, eb);
                w += snprintf(w, (size_t)(total - (size_t)(w - buf)),
                              "\"%s\":\"", NOISE_O_KEY_OUT_IVAR_B64);
                memcpy(w, eb, (size_t)el); w += el;
                strcpy(w, "\"}"); w += 2;
                free(eb);
            }
        } else if (ok) {
            strcpy(w - 1, "}");   /* 去尾逗号收口 */
        }
    } else if (ok) {
        strcpy(w - 1, "}");       /* 去尾逗号收口 */
    }
    free(out_var); free(out_ivar);
    snr_noise_model_v1_free(&model);   /* 事务收尾 (DISP-NOISE-009 泄漏防线) */
    ex->release(ex->user_data, leased);
    noise_rows_free(&rows);

    if (!ok) {
        free(buf);
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "noise: plane encode alloc failed");
    }

    inst->exec_count++;
    inst->last_rc = rc;
    inst->last_workers = leased;
    inst->last_n_ctrl = n_ctrl;
    inst->last_floor_fallback = 0;
    snprintf(inst->last_op, sizeof(inst->last_op), "%s",
             ASTROCS_NOISE_OP_ESTIMATE);
    inst->last_status = ACS_OK;

    acs_status ret = strbuf_write(out_json, buf, (uint64_t)(w - buf), err);
    free(buf);
    if (ret == ACS_OK) inst->last_status = ret;
    return ret;
}

/* ── 6b. fill_noise_field: 模型 round-trip 影子实例 + 独立 fill ──
 * FIX-405 G3-6 (钳位 fail-open → fail-closed): 影子实例由本层手工拼装,
 * 不在 g_model_floor 注册表内。原实现让 fill_impl 静默回退 1e-12 ⇒ manifest
 * 里配置的 variance_floor 在生产 fill 路径上被无声忽略（且 manifest 恒写
 * floor_fallback:1 掩盖了这一点）。现改为: 本层用 manifest 的 variance_floor
 * 经 snr_noise_model_v1_bind_variance_floor **显式绑定**, 绑定失败或 floor
 * 非法 ⇒ 显式错误 (不落任何平面); 绑定的 floor 必须有限且 > 0。
 * 对拍口径: direct 通道与影子通道现在都吃同一个显式 floor ⇒ 仍 bitwise 一致。 */

static acs_status noise_execute_fill(noise_inst* inst, const char* manifest,
                                     const noise_cfg* c,
                                     acs_strbuf_v1* out_json,
                                     acs_error_info_v1* err) {
    const acs_executor_v1* ex = NULL;
    uint32_t leased = 0;
    acs_status st = noise_acquire_lease(inst, &ex, &leased, err);
    if (st != ACS_OK) return st;

    noise_rows rows;
    st = noise_rows_parse_fill(manifest, c, &rows, err);
    if (st != ACS_OK) {
        ex->release(ex->user_data, leased);
        return st;
    }

    if (NOISE_CANCELLED_NOW(inst)) {
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->last_status = ACS_ERR_CANCELLED;
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     ACS_DIAG_ECODE_NONE,
                     "noise: cancelled before scientific call");
    }

    /* 影子模型重建 (round-trip; flags/全局标量自 manifest) */
    NoiseWeightModelV1 shadow;
    memset(&shadow, 0, sizeof(shadow));
    /* MASK-002 (claim SC-009): 手工拼装的跨边界模型必须自带 ABI 头部,
     * 否则 snr_noise_model_v1_fill fail-closed (rc=-9) 拒绝消费。 */
    snr_noise_model_v1_abi_stamp_model(&shadow);
    shadow.n_control_points = (uint32_t)rows.n_ctrl;
    if (rows.n_ctrl > 0) {
        shadow.ctrl_x_px = (double*)malloc((size_t)(rows.n_ctrl * 8ull));
        shadow.ctrl_y_px = (double*)malloc((size_t)(rows.n_ctrl * 8ull));
        shadow.ctrl_sigma = (double*)malloc((size_t)(rows.n_ctrl * 8ull));
        shadow.ctrl_variance = (double*)malloc((size_t)(rows.n_ctrl * 8ull));
        shadow.ctrl_ivar = (double*)malloc((size_t)(rows.n_ctrl * 8ull));
        if (!shadow.ctrl_x_px || !shadow.ctrl_y_px || !shadow.ctrl_sigma ||
            !shadow.ctrl_variance || !shadow.ctrl_ivar) {
            snr_noise_model_v1_free(&shadow);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         NOISE_ECODE_MODEL_REBUILD,
                         "noise: shadow model alloc failed");
        }
        memcpy(shadow.ctrl_x_px, rows.ctrl_x, (size_t)(rows.n_ctrl * 8ull));
        memcpy(shadow.ctrl_y_px, rows.ctrl_y, (size_t)(rows.n_ctrl * 8ull));
        memcpy(shadow.ctrl_sigma, rows.ctrl_sig, (size_t)(rows.n_ctrl * 8ull));
        memcpy(shadow.ctrl_variance, rows.ctrl_var, (size_t)(rows.n_ctrl * 8ull));
        memcpy(shadow.ctrl_ivar, rows.ctrl_ivar, (size_t)(rows.n_ctrl * 8ull));
    }
    {
        int f = 0;
        uint64_t u;
        u = json_get_u64(manifest, NOISE_M_KEY_SOURCE, &f);
        shadow.source = (uint8_t)(f ? (u & 0xffull) : 0u);
        u = json_get_u64(manifest, NOISE_M_KEY_HAS_SPATIAL, &f);
        shadow.has_spatial_field = (uint8_t)(f ? (u & 0xffull) : 0u);
        u = json_get_u64(manifest, NOISE_M_KEY_DEGENERATE, &f);
        shadow.degenerate = (uint8_t)(f ? (u & 0xffull) : 0u);
        shadow.sigma_bg_global = json_get_f64(manifest, NOISE_M_KEY_SIGMA_BG, &f);
        shadow.variance_bg_global = json_get_f64(manifest, NOISE_M_KEY_VARIANCE_BG, &f);
        shadow.ivar_bg_global = json_get_f64(manifest, NOISE_M_KEY_IVAR_BG, &f);
    }

    /* fill 平面输出缓冲 */
    const uint64_t plane_bytes = c->h * c->w * 4ull;
    uint8_t* out_var = NULL;
    uint8_t* out_ivar = NULL;
    if (c->fill_variance) {
        out_var = (uint8_t*)malloc((size_t)plane_bytes);
        if (!out_var) {
            snr_noise_model_v1_free(&shadow);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE,
                         "noise: fill variance plane alloc failed");
        }
    }
    if (c->fill_ivar) {
        out_ivar = (uint8_t*)malloc((size_t)plane_bytes);
        if (!out_ivar) {
            free(out_var);
            snr_noise_model_v1_free(&shadow);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE,
                         "noise: fill ivar plane alloc failed");
        }
    }

    /* FIX-405 G3-6: 显式绑定 fill 下限（配置的 variance_floor 必须真正生效,
     * 不得静默回退 1e-12）。floor 非法/缺失 ⇒ fail-closed, 不落平面。 */
    double floor_cfg = 0.0;
    {
        floor_cfg = json_get_f64(manifest, NOISE_O_KEY_VARIANCE_FLOOR, NULL);
        const int brc =
            snr_noise_model_v1_bind_variance_floor(&shadow, floor_cfg);
        if (brc != 0) {
            free(out_var); free(out_ivar);
            snr_noise_model_v1_free(&shadow);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->exec_count++;
            inst->last_rc = brc;
            inst->last_workers = leased;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                     ASTROCS_NOISE_OP_FILL);
            inst->last_status = ACS_ERR_PARAM;
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         ACS_DIAG_ECODE_NONE,
                         noise_msgf("noise: %s missing/invalid in model manifest "
                                    "(got %g; FIX-405 G3-6 fail-closed)",
                                    NOISE_O_KEY_VARIANCE_FLOOR, floor_cfg));
        }
    }
    int rc = snr_noise_model_v1_fill(&shadow, (int)c->h, (int)c->w,
                                     (float*)out_var, (float*)out_ivar);
    if (rc != 0) {
        free(out_var); free(out_ivar);
        snr_noise_model_v1_free(&shadow);
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->exec_count++;
        inst->last_rc = rc;
        inst->last_workers = leased;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_NOISE_OP_FILL);
        int32_t dom = ACS_ERR_DOMAIN_SCIENCE_PRECONDITION;
        acs_status cst = noise_legacy_status(rc, &dom);
        inst->last_status = cst;
        return efill(err, cst, dom, NOISE_ECODE_LEGACY_REJECT,
                     noise_msgf("noise: legacy_code=%d at fill", rc));
    }

    /* 输出 manifest (平面 + 标量 round-trip + floor_fallback 标记) */
    uint64_t total = 1024ull +
                     (uint64_t)(c->fill_variance + c->fill_ivar) *
                         (b64_enc_len(plane_bytes + 1ull) + 64ull) + 128ull;
    char* buf = (char*)malloc((size_t)total);
    if (!buf) {
        free(out_var); free(out_ivar);
        snr_noise_model_v1_free(&shadow);
        ex->release(ex->user_data, leased);
        noise_rows_free(&rows);
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "noise: out manifest alloc failed");
    }
    char* w = buf;
    w += snprintf(w, (size_t)(total - (size_t)(w - buf)),
        "{\"op\":\"%s\",\"rc\":0,\"h\":%llu,\"w\":%llu,"
        "\"n_control_points\":%llu,"
        /* FIX-405 G3-6: 原恒写 "floor_fallback":1（掩盖了 1e-12 静默回退）。
         * 现如实登记: 影子模型已显式绑定 manifest 的 variance_floor,
         * 回退路径不再存在 ⇒ floor_fallback=0 + 绑定值与状态。 */
        "\"floor_fallback\":0,\"variance_floor_bound\":1,"
        "\"variance_floor\":%.17g,\"variance_floor_status\":\"bound\",",
        ASTROCS_NOISE_OP_FILL,
        (unsigned long long)c->h, (unsigned long long)c->w,
        (unsigned long long)rows.n_ctrl, floor_cfg);
    {
        double sc[4];
        sc[0] = shadow.sigma_bg_global;
        sc[1] = shadow.variance_bg_global;
        sc[2] = shadow.ivar_bg_global;
        sc[3] = json_get_f64(manifest, NOISE_O_KEY_VARIANCE_FLOOR, NULL); /* 回显 (可 0) */
        if (!json_append_scalars(&w, sc, 4)) {
            free(buf); free(out_var); free(out_ivar);
            snr_noise_model_v1_free(&shadow);
            ex->release(ex->user_data, leased);
            noise_rows_free(&rows);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE,
                         "noise: scalars encode alloc failed");
        }
    }
    int ok = 1;
    if (c->fill_variance)
        ok = json_append_plane(&w, NOISE_O_KEY_OUT_VAR_B64, out_var, plane_bytes);
    if (ok && c->fill_ivar) {
        uint64_t el = b64_enc_len(plane_bytes);
        char* eb = (char*)malloc((size_t)(el + 1));
        if (!eb) {
            ok = 0;
        } else {
            b64_encode(out_ivar, plane_bytes, eb);
            w += snprintf(w, (size_t)(total - (size_t)(w - buf)),
                          "\"%s\":\"", NOISE_O_KEY_OUT_IVAR_B64);
            memcpy(w, eb, (size_t)el); w += el;
            strcpy(w, "\"}"); w += 2;
            free(eb);
        }
    } else if (ok) {
        strcpy(w - 1, "}");   /* 去尾逗号收口 */
    }
    free(out_var); free(out_ivar);
    snr_noise_model_v1_free(&shadow);
    ex->release(ex->user_data, leased);
    noise_rows_free(&rows);

    if (!ok) {
        free(buf);
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "noise: plane encode alloc failed");
    }

    inst->exec_count++;
    inst->last_rc = 0;
    inst->last_workers = leased;
    inst->last_n_ctrl = rows.n_ctrl;
    inst->last_floor_fallback = 1;
    snprintf(inst->last_op, sizeof(inst->last_op), "%s",
             ASTROCS_NOISE_OP_FILL);
    inst->last_status = ACS_OK;

    acs_status ret = strbuf_write(out_json, buf, (uint64_t)(w - buf), err);
    free(buf);
    if (ret == ACS_OK) inst->last_status = ret;
    return ret;
}

/* ── 6c. noise_diagnostic: scale_law / gain_variance 标量 (tiny 串行) ── */

static acs_status noise_execute_diag(noise_inst* inst, const noise_cfg* c,
                                     acs_strbuf_v1* out_json,
                                     acs_error_info_v1* err) {
    const acs_executor_v1* ex = NULL;
    uint32_t leased = 0;
    acs_status st = noise_acquire_lease(inst, &ex, &leased, err);
    if (st != ACS_OK) return st;

    if (NOISE_CANCELLED_NOW(inst)) {
        ex->release(ex->user_data, leased);
        inst->last_status = ACS_ERR_CANCELLED;
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     ACS_DIAG_ECODE_NONE,
                     "noise: cancelled before scientific call");
    }

    char buf[768];
    if (strcmp(c->subop, "scale_law") == 0) {
        /* 生产 1:1: 指针可 NULL (未请求字段不变换); alpha NaN/负直传
         * (DISP-NOISE-005 现状, 零新增校验) */
        double variance = c->variance_in;
        double ivar = c->ivar_in;
        snr_noise_scale_law(c->alpha,
                            c->variance_given ? &variance : NULL,
                            c->ivar_given ? &ivar : NULL);
        double sc[3];
        uint32_t k = 0;
        char body[512];
        char* b = body;
        *b = '\0';
        if (c->variance_given) {
            sc[k++] = variance;
            b += snprintf(b, (size_t)(body + sizeof(body) - b),
                          "\"variance\":%.17g,", variance);
        }
        if (c->ivar_given) {
            sc[k++] = ivar;
            b += snprintf(b, (size_t)(body + sizeof(body) - b),
                          "\"ivar\":%.17g,", ivar);
        }
        sc[k++] = c->alpha;
        snprintf(buf, sizeof(buf),
                 "{\"op\":\"%s\",\"subop\":\"scale_law\",%s"
                 "\"alpha_text\":\"%.17g\",\"workers\":%u,",
                 ASTROCS_NOISE_OP_DIAG, body, c->alpha, (unsigned)leased);
        /* 拼 scalars_base64 (bitwise 主口径) 后收口 */
        char* w = buf + strlen(buf);
        if (!json_append_plane_tail(&w, NOISE_O_KEY_SCALARS_B64, sc,
                                    (uint64_t)k * sizeof(double))) {
            ex->release(ex->user_data, leased);
            inst->last_status = ACS_ERR_NOMEM;
            return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                         ACS_DIAG_ECODE_NONE, "noise: scalars encode failed");
        }
        /* json_append_plane_tail 已写 "}"; buf 容量: 768 ≥ 头+body+44+16 */
        inst->exec_count++;
        inst->last_rc = 0;
        inst->last_workers = leased;
        inst->last_floor_fallback = 0;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s",
                 ASTROCS_NOISE_OP_DIAG);
        ex->release(ex->user_data, leased);
        inst->last_status = ACS_OK;
        return strbuf_write_cstr(out_json, buf, err);
    }

    /* gain_variance: var_ADU = max(signal,0)/gain + (rn/gain)² (ALG-NOISE-003) */
    double gv = snr_noise_gain_variance(c->signal_in, c->gain_in, c->rn_in);
    snprintf(buf, sizeof(buf),
             "{\"op\":\"%s\",\"subop\":\"gain_variance\","
             "\"gain_variance_text\":\"%.17g\",\"workers\":%u,",
             ASTROCS_NOISE_OP_DIAG, gv, (unsigned)leased);
    char* w = buf + strlen(buf);
    double sc[1] = { gv };
    if (!json_append_plane_tail(&w, NOISE_O_KEY_SCALARS_B64, sc,
                                sizeof(double))) {
        ex->release(ex->user_data, leased);
        inst->last_status = ACS_ERR_NOMEM;
        return efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                     ACS_DIAG_ECODE_NONE, "noise: scalars encode failed");
    }
    inst->exec_count++;
    inst->last_rc = 0;
    inst->last_workers = leased;
    inst->last_floor_fallback = 0;
    snprintf(inst->last_op, sizeof(inst->last_op), "%s",
             ASTROCS_NOISE_OP_DIAG);
    ex->release(ex->user_data, leased);
    inst->last_status = ACS_OK;
    return strbuf_write_cstr(out_json, buf, err);
}

static acs_status noise_execute(acs_module_instance_v1* inst_raw,
                                acs_str_v1 input_manifest_json,
                                acs_str_v1 config_json,
                                acs_strbuf_v1* out_manifest_json,
                                acs_error_info_v1* err) {
    try {
        noise_inst* inst = (noise_inst*)inst_raw;
        if (!inst) return ACS_ERR_PARAM;
        if (inst->head.abi_version != ACS_ABI_VERSION_V1 ||
            inst->state == ACS_LC_STATE_DESTROYED) {
            return efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
                         ACS_DIAG_ECODE_ILLEGAL_STATE,
                         "noise: instance destroyed");
        }
        if (inst->state != ACS_LC_STATE_CREATED || inst->executing) {
            return efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
                         ACS_DIAG_ECODE_ILLEGAL_STATE, "noise: instance busy/state");
        }
        const char* mjson = input_manifest_json.data;
        if (!mjson || input_manifest_json.size == 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         ACS_DIAG_ECODE_NULL_CONFIG,
                         "noise: empty input manifest");
        }
        /* 事务起点: 输出面零写入承诺 (P1-CAL 同款; 失败/取消路径 out 零泄漏) */
        if (out_manifest_json) out_manifest_json->size = 0;
        noise_cfg c;
        acs_status st = noise_cfg_parse(config_json.data, &c, err);
        if (st != ACS_OK) return st;

        inst->executing = 1;
        inst->state = ACS_LC_STATE_EXECUTING;

        /* 入口取消预检 (P1-COS 三查次序: entry cancel → 硬租约 → 解析) */
        st = noise_check_entry_cancel(inst, err);
        if (st == ACS_OK) {
            if (strcmp(c.op, ASTROCS_NOISE_OP_ESTIMATE) == 0)
                st = noise_execute_estimate(inst, mjson, &c, out_manifest_json, err);
            else if (strcmp(c.op, ASTROCS_NOISE_OP_FILL) == 0)
                st = noise_execute_fill(inst, mjson, &c, out_manifest_json, err);
            else
                st = noise_execute_diag(inst, &c, out_manifest_json, err);
        }

        noise_cfg_free(&c);
        inst->executing = 0;
        if (inst->state == ACS_LC_STATE_EXECUTING ||
            inst->state == ACS_LC_STATE_CANCELLING)
            inst->state = ACS_LC_STATE_CREATED;   /* execute 完成回 CREATED (ABI-002) */
        return st;
    } catch (...) {
        return efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                     NOISE_ECODE_EXCEPTION, "exception in execute");
    }
}

/* ═══════════════════ 7. 静态 vtable 与唯一导出入口 ═══════════════════ */

static const acs_module_api_v1 g_noise_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    noise_describe,
    noise_validate_config,
    noise_plan,
    noise_create,
    noise_execute,
    noise_inspect,
    noise_request_cancel,
    noise_destroy
};

/* 唯一导出入口 (ARC-001 §1.1; ABI-006 全查 exports):
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi,
                        const acs_host_api_v1* host,
                        const acs_module_api_v1** out_api) {
    try {
        (void)host;   /* allocator 必填在 create 期校验 (query 期 host 可 NULL 于探针) */
        if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
        if (!out_api) return ACS_ERR_PARAM;
        *out_api = &g_noise_api;
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_EXCEPTION;
    }
}
#ifdef __cplusplus
} /* extern "C" */
#endif
