// module_entry.cpp — astrocs_p1_calibration 模块 C ABI v1 adapter (P1-CAL-IMPL)
//
// 对齐先例: lib/gaia_xpsd_client/src/module_entry.c (CAT-GAIA-IMPL, babe752d)
//           lib/drizzle/src/module_entry.cpp (P1-DRZ-IMPL, C++ TU + extern "C")
//
// 冻结合同:
//   - 唯一导出 astrocs_module_query_v1 (12 §1 / ABI-006); vtable 九操作时序
//     query → describe/validate_config/plan → create → execute* → inspect
//     → request_cancel → destroy (module_api_v1.h / lifecycle_v1.h)。
//   - 科学域零改动 (scientific_change=false): 本文件只做 config/manifest 解析、
//     内存平面装配、线程租约注入与事务输出; 算法全部转发 legacy AC_API
//     (ac_generate_master_* / ac_calibrate_frame* / ac_correct_frame*)。
//   - 线程模型 host_executor_lease: execute 期经 host executor acquire/release
//     租借, 注入途径 = legacy ac_set_num_threads (OMP ICV; drizzle T-2 同型);
//     acquire 失败 → 单线程降级 (drizzle 先例, 非致命); 不私建线程池
//     (FORBID-003)。
//   - 异常屏障 (模板规则 7 / DISP-CAL-001 整改): legacy 生产源 ac_api.cpp 的
//     extern "C" 段无 try/catch, std::bad_alloc 可穿越 —— 本 adapter 将全部
//     九个跨边界回调以 try/catch 全包裹, 任何 C++ 异常转 ACS_ERR_EXCEPTION(10)
//     + detail 130, 不穿越 DLL 边界。
//   - 事务性: 失败/取消路径不写输出 manifest; execute 完成后实例回 CREATED
//     可重复调用 (四态状态机, lifecycle_v1.h)。
//   - 数据平面 v1: 输入/输出均为 inline base64 平面 (drizzle manifest 口径);
//     master_bias/master_dark/master_flat 的 config 存储路径形态仅当 host
//     注入 artifacts 服务时经 artifact_open/read_all 解析 (FORBID-002: 模块
//     不直接触碰文件路径); 无 artifacts 服务且 manifest 无 inline 平面时按
//     legacy NULL 语义 (恒等路径, DISP 登记行为保持)。
//
// HEAD 语义偏差处置 (P1-CAL-TEST 注册, scientific_change=false 判定):
//   DEV-1 全 NaN flat 恒等输出 / DEV-2 dark·bias NULL 静默恒等 / DEV-3 NaN
//   参与统计 —— 三者均保持 legacy 现状 (DEV-1/2 有 ALG-CAL §8 冻结默认语义;
//   DEV-3 无目标语义 → 维持 DISP-CAL-004), adapter 不做修正。
//
// 并发: 同实例 execute 互斥 (state 护栏); inspect 并发只读; request_cancel
// 原子置位。全部内部状态单写者 = execute 调用线程。

#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/calibration/types.h"
#include "astro_calibration.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#define CAL_OMP_GET() omp_get_max_threads()
#define CAL_OMP_SET(n) omp_set_num_threads((n))
#else
#define CAL_OMP_GET() 1
#define CAL_OMP_SET(n) ((void)(n))
#endif

/* ====================================================================== */
/* 基础工具: 状态填充 / strbuf 两阶段提交 / base64                          */
/* ====================================================================== */

namespace acs_cal {

typedef std::vector<uint8_t> bytes;

/* op 分派类别 (词表 10 op → 3 类 × f64) */
typedef enum {
    OPK_MASTER = 0,     /* generate_master_bias/dark/flat (± _f64) */
    OPK_CALIBRATE = 1,  /* calibrate_frame (± _f64) */
    OPK_CORRECT = 2     /* correct_frame (± _f64) */
} op_kind_t;

acs_status efill(acs_error_info_v1* err, acs_status st, int32_t domain,
                 const char* msg, uint32_t detail) {
    if (err) {
        std::memset(err, 0, sizeof(*err));
        err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err->head.abi_version = ACS_ABI_VERSION_V1;
        err->status = st;
        err->domain = domain;
        err->message_utf8 = msg;
        err->message_bytes = msg ? (uint32_t)std::strlen(msg) : 0u;
        err->detail_code = detail;
    }
    return st;
}

/* strbuf 两阶段 (drizzle/gaia 口径): data==NULL 或 cap==0 → 尺寸探测,
 * size=所需, 返回 ACS_OK; cap>0 不足 → PARAM + BUFFER_TOO_SMALL, size=所需,
 * 写前缀 + NUL; 足够 → 整写 + NUL。 */
acs_status strbuf_commit(acs_strbuf_v1* sb, const char* src, uint64_t needed,
                         acs_error_info_v1* err) {
    if (!sb) return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                          "output strbuf required", ACS_DIAG_ECODE_NULL_CALLBACK);
    sb->size = needed;
    if (!sb->data || sb->cap == 0u) return ACS_OK;
    if (sb->cap < needed + 1u) {
        const uint64_t n = (sb->cap > 0u) ? sb->cap - 1u : 0u;
        if (n > 0u) std::memcpy(sb->data, src, (size_t)((n < needed) ? n : needed));
        sb->data[n] = '\0';
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     "output buffer too small", ACS_DIAG_ECODE_BUFFER_TOO_SMALL);
    }
    std::memcpy(sb->data, src, (size_t)needed);
    sb->data[needed] = '\0';
    return ACS_OK;
}

/* ── base64 (RFC 4648 标准字母表; 严格解码) ── */

int b64_value(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* 返回解码字节数; 非法字符/长度 → -1。'=' 只允许结尾补齐。 */
int64_t b64_decode_len(const char* s, uint64_t n) {
    uint64_t out = 0;
    int group = 0;
    int pad = 0;
    for (uint64_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
        if (c == '=') {
            if (group == 0 || pad >= 2) return -1;
            ++pad;
            continue;
        }
        if (pad > 0) return -1;   /* 补齐后出现数据 */
        if (b64_value((unsigned char)c) < 0) return -1;
        if (++group == 4) {
            out += 3;
            group = 0;
        }
    }
    if (pad > 0 && group + pad != 4) return -1;
    if (group == 1) return -1;    /* 孤立单字符组 */
    if (group == 2) out += 1;
    if (group == 3) out += 2;
    return (int64_t)out;
}

int b64_decode(const char* s, uint64_t n, uint8_t* out, uint64_t cap, uint64_t* out_len) {
    const int64_t need = b64_decode_len(s, n);
    if (need < 0 || (uint64_t)need > cap) return -1;
    uint32_t acc = 0;
    int bits = 0;
    uint64_t w = 0;
    int pad = 0;
    for (uint64_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (c == '\r' || c == '\n' || c == '\t' || c == ' ') continue;
        if (c == '=') { ++pad; continue; }
        if (pad > 0) return -1;
        acc = (acc << 6) | (uint32_t)b64_value((unsigned char)c);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (w >= cap) return -1;
            out[w++] = (uint8_t)((acc >> bits) & 0xFFu);
        }
    }
    *out_len = w;
    return (w == (uint64_t)need) ? 0 : -1;
}

uint64_t b64_enc_len(uint64_t raw) {
    return ((raw + 2u) / 3u) * 4u;
}

void b64_encode(const uint8_t* src, uint64_t n, char* out) {
    static const char T[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint64_t o = 0;
    uint64_t i = 0;
    for (; i + 3u <= n; i += 3u) {
        const uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) |
                           (uint32_t)src[i + 2];
        out[o++] = T[(v >> 18) & 63u];
        out[o++] = T[(v >> 12) & 63u];
        out[o++] = T[(v >> 6) & 63u];
        out[o++] = T[v & 63u];
    }
    const uint64_t rem = n - i;
    if (rem == 1u) {
        const uint32_t v = (uint32_t)src[i] << 16;
        out[o++] = T[(v >> 18) & 63u];
        out[o++] = T[(v >> 12) & 63u];
        out[o++] = '=';
        out[o++] = '=';
    } else if (rem == 2u) {
        const uint32_t v = ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8);
        out[o++] = T[(v >> 18) & 63u];
        out[o++] = T[(v >> 12) & 63u];
        out[o++] = T[(v >> 6) & 63u];
        out[o++] = '=';
    }
}

}  // namespace acs_cal

/* ====================================================================== */
/* JSON 配置解析 (迷你严格解析器; 15-键词表, 9 §10 冻结禁止)                 */
/* ====================================================================== */

namespace acs_cal {

/* 值: 数字 (含整数) / 布尔 / 字符串 / null; 拒对象与数组 (schema 禁止嵌套)。
 * 重复键 → PARAM + CONFIG_SCHEMA (9 §10)。unknown 键 → PARAM + CONFIG_SCHEMA。 */

struct jc_keyval {
    const char*  key;    /* 借入 */
    uint32_t     klen;
    const char*  val;    /* 借入 (标量值 / 数组时=元素数) */
    uint32_t     vlen;
    char         kind;   /* 'n' 数字 / 'b' 布尔 / 's' 字符串 / '0' null / 'S' 字符串数组 */
    const char** aux;    /* kind='S': 元素对 [ptr,len] 连续存储起点 */
    int          aux_n;  /* kind='S': 元素数 */
};

struct jc_doc {
    const char* s;
    uint64_t    n;
    uint64_t    i;
};

int jc_ws(jc_doc* d) {
    while (d->i < d->n) {
        const char c = d->s[d->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++d->i;
        else break;
    }
    return 0;
}

int jc_peek(jc_doc* d) {
    jc_ws(d);
    return (d->i < d->n) ? (unsigned char)d->s[d->i] : -1;
}

/* 转义感知字符串字面量 (仅到结束引号; 不反解内容 —— 值经 size 定位, 语义值
 * 调用方按需借入; 配置键词表无转义字符, 含转义即非词表键 → 拒) */
int jc_string(jc_doc* d, const char** out, uint32_t* olen) {
    if (jc_peek(d) != '"') return -1;
    ++d->i;
    const uint64_t start = d->i;
    while (d->i < d->n) {
        const char c = d->s[d->i];
        if (c == '"') {
            *out = d->s + start;
            *olen = (uint32_t)(d->i - start);
            ++d->i;
            return 0;
        }
        if (c == '\\' || (unsigned char)c < 0x20) return -1; /* 词表禁转义/控制符 */
        ++d->i;
    }
    return -1;
}

int jc_number(jc_doc* d, const char** out, uint32_t* olen) {
    jc_ws(d);
    const uint64_t start = d->i;
    if (d->i < d->n && (d->s[d->i] == '-' || d->s[d->i] == '+')) ++d->i;
    int digits = 0;
    int dots = 0;
    while (d->i < d->n) {
        const char c = d->s[d->i];
        if (c >= '0' && c <= '9') { ++digits; ++d->i; }
        else if (c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
            if (c == '.') ++dots;
            ++d->i;
        } else break;
    }
    if (digits == 0 || dots > 1) return -1;
    *out = d->s + start;
    *olen = (uint32_t)(d->i - start);
    return 0;
}

int jc_literal(jc_doc* d, const char* lit, uint32_t len) {
    jc_ws(d);
    if (d->i + len <= d->n && std::memcmp(d->s + d->i, lit, len) == 0) {
        d->i += len;
        return 0;
    }
    return -1;
}

/* 顶层对象 → keyval 数组 (上限 16; config 词表 15 键 / manifest 13 键)。
 * 字符串值 kind='s'; 字符串数组 (master 多帧) kind='S', 元素借入表 aux
 * (容量 manifest 数组键最多 3: master_bias/dark/flat; 每键条目上限
 * ACS_CAL_MAX_STACK=64)。kind='A' 未支持 (对象/嵌套) → 拒。 */
#define ACS_CAL_MAX_STACK 64

int jc_parse_object(jc_doc* d, jc_keyval* kvs, int* nkv, int maxkv,
                    const char** aux, int naux_cap) {
    int naux = 0;
    *nkv = 0;
    if (jc_peek(d) != '{') return -1;
    ++d->i;
    if (jc_peek(d) == '}') { ++d->i; return 0; }
    for (;;) {
        const char* k = NULL;
        uint32_t kl = 0;
        if (jc_string(d, &k, &kl) != 0) return -1;
        if (jc_peek(d) != ':') return -1;
        ++d->i;
        const int c = jc_peek(d);
        const char* v = NULL;
        uint32_t vl = 0;
        char kind;
        if (c == '"') {
            if (jc_string(d, &v, &vl) != 0) return -1;
            kind = 's';
        } else if (c == '[') {
            ++d->i;
            int cnt = 0;
            const uint64_t aux0 = naux;
            if (jc_peek(d) == ']') {
                ++d->i;
            } else {
                for (;;) {
                    const char* el = NULL;
                    uint32_t el_l = 0;
                    if (jc_peek(d) != '"') return -1;   /* 仅字符串数组 */
                    if (jc_string(d, &el, &el_l) != 0) return -1;
                    if (naux >= naux_cap || cnt >= ACS_CAL_MAX_STACK) return -1;
                    aux[naux++] = el;
                    aux[naux++] = (const char*)(uintptr_t)el_l;
                    ++cnt;
                    const int ec = jc_peek(d);
                    if (ec == ',') { ++d->i; continue; }
                    if (ec == ']') { ++d->i; break; }
                    return -1;
                }
            }
            v = (const char*)(uintptr_t)cnt;
            vl = 0;
            kind = 'S';
            (void)aux0;
        } else if (c == 't') {
            if (jc_literal(d, "true", 4) != 0) return -1;
            v = d->s + (d->i - 4); vl = 4; kind = 'b';
        } else if (c == 'f') {
            if (jc_literal(d, "false", 5) != 0) return -1;
            v = d->s + (d->i - 5); vl = 5; kind = 'b';
        } else if (c == 'n') {
            if (jc_literal(d, "null", 4) != 0) return -1;
            v = d->s + (d->i - 4); vl = 4; kind = '0';
        } else {
            if (jc_number(d, &v, &vl) != 0) return -1;
            kind = 'n';
        }
        if (*nkv >= maxkv) return -1;   /* 超词表规模 */
        for (int q = 0; q < *nkv; ++q) {
            if (kvs[q].klen == kl && std::memcmp(kvs[q].key, k, kl) == 0)
                return -1;              /* 重复键: 9 §10 */
        }
        kvs[*nkv].key = k;
        kvs[*nkv].klen = kl;
        kvs[*nkv].val = v;
        kvs[*nkv].vlen = vl;
        kvs[*nkv].kind = kind;
        kvs[*nkv].aux_n = (kind == 'S') ? (int)(uintptr_t)v : 0;
        kvs[*nkv].aux = (kind == 'S') ? aux + (naux - 2 * kvs[*nkv].aux_n) : NULL;
        ++*nkv;
        const int nc = jc_peek(d);
        if (nc == ',') { ++d->i; continue; }
        if (nc == '}') { ++d->i; return 0; }
        return -1;
    }
}

/* 词表静态表 */
struct cfg_meta {
    const char* name;
    char        kind;   /* 期望 kind: 'n' / 'b' / 's' */
};

const cfg_meta CFG_VOCAB[] = {
    { "op",                 's' },
    { "width",              'n' },
    { "height",             'n' },
    { "frames",             'n' },
    { "sigma_low",          'n' },
    { "sigma_high",         'n' },
    { "max_iterations",     'n' },
    { "combine",            's' },
    { "dark_optimization",  'b' },
    { "dark_scale_factor",  'n' },
    { "hot_sigma",          'n' },
    { "cold_sigma",         'n' },
    { "method",             's' },
    { "max_structure_size", 'n' },
    { "max_workers",        'n' }
};
const int CFG_VOCAB_N = (int)(sizeof(CFG_VOCAB) / sizeof(CFG_VOCAB[0]));

int cfg_index(const char* k, uint32_t kl) {
    for (int i = 0; i < CFG_VOCAB_N; ++i) {
        if (std::strlen(CFG_VOCAB[i].name) == kl &&
            std::memcmp(CFG_VOCAB[i].name, k, kl) == 0) return i;
    }
    return -1;
}

int kv_find(const jc_keyval* kvs, int nkv, const char* name) {
    const uint32_t l = (uint32_t)std::strlen(name);
    for (int i = 0; i < nkv; ++i) {
        if (kvs[i].klen == l && std::memcmp(kvs[i].key, name, l) == 0) return i;
    }
    return -1;
}

/* 类型校验 + 词表校验 + 数值性校验。返回 ACS_OK / ACS_ERR_PARAM。 */
acs_status cfg_check(const jc_keyval* kvs, int nkv, acs_error_info_v1* err) {
    for (int i = 0; i < nkv; ++i) {
        const int vi = cfg_index(kvs[i].key, kvs[i].klen);
        if (vi < 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "unknown config key (declared 15-key vocabulary)",
                         ACS_DIAG_ECODE_CONFIG_SCHEMA);
        }
        const char want = CFG_VOCAB[vi].kind;
        if (kvs[i].kind != want) {
            /* 类型错 = PARAM_TYPE (types.h 102; 未知键才是 CONFIG_SCHEMA) */
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "config value type mismatch", 102);
        }
        if (want == 'n') {
            double d = 0.0;
            std::sscanf(kvs[i].val, "%lf", &d);   /* 已通过 jc_number 词法 */
            if (!(std::isfinite(d))) {
                return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                             "config numeric value not finite", 103);
            }
        }
    }
    return ACS_OK;
}

int kv_u64(const jc_keyval* kvs, int nkv, const char* name, uint64_t* out) {
    const int i = kv_find(kvs, nkv, name);
    if (i < 0 || kvs[i].kind != 'n') return -1;
    /* 词法保证 digits>=0; 负/浮点 → 视作类型不符 */
    uint64_t acc = 0;
    int neg = 0;
    uint32_t j = 0;
    if (kvs[i].val[j] == '-') { neg = 1; ++j; }
    for (; j < kvs[i].vlen; ++j) {
        const char c = kvs[i].val[j];
        if (c < '0' || c > '9') return -1;   /* 浮点/科学计数 → 非整数 */
        if (acc > (UINT64_MAX - (uint64_t)(c - '0')) / 10u) return -1;
        acc = acc * 10u + (uint64_t)(c - '0');
    }
    if (neg) return -1;
    *out = acc;
    return 0;
}

int kv_f64(const jc_keyval* kvs, int nkv, const char* name, double* out) {
    const int i = kv_find(kvs, nkv, name);
    if (i < 0 || kvs[i].kind != 'n') return -1;
    char buf[64];
    if (kvs[i].vlen >= sizeof(buf)) return -1;
    std::memcpy(buf, kvs[i].val, kvs[i].vlen);
    buf[kvs[i].vlen] = '\0';
    char* end = NULL;
    const double d = std::strtod(buf, &end);
    if (end == buf || !std::isfinite(d)) return -1;
    *out = d;
    return 0;
}

int kv_str(const jc_keyval* kvs, int nkv, const char* name,
           const char** out, uint32_t* olen) {
    const int i = kv_find(kvs, nkv, name);
    if (i < 0 || kvs[i].kind != 's') return -1;
    *out = kvs[i].val;
    *olen = kvs[i].vlen;
    return 0;
}

int kv_bool(const jc_keyval* kvs, int nkv, const char* name, int* out) {
    const int i = kv_find(kvs, nkv, name);
    if (i < 0) return -1;
    if (kvs[i].kind != 'b') return -1;
    *out = (kvs[i].vlen == 4) ? 1 : 0;   /* "true"=4 / "false"=5 */
    return 0;
}

}  // namespace acs_cal

/* ====================================================================== */
/* op 词表 / 实例 / vtable 主逻辑                                            */
/* ====================================================================== */

namespace acs_cal {

struct op_entry {
    const char* name;
    op_kind_t   kind;
    int         is_f64;
    int         is_flat;   /* master_flat: legacy 签名无 combine 参数 */
};

/* 科学导出 1:1 (10); ac_set_num_threads/ac_version 为工具通道不进 op 面 */
const op_entry OP_VOCAB[] = {
    { "calibrate_frame",        OPK_CALIBRATE, 0, 0 },
    { "calibrate_frame_f64",    OPK_CALIBRATE, 1, 0 },
    { "correct_frame",          OPK_CORRECT,   0, 0 },
    { "correct_frame_f64",      OPK_CORRECT,   1, 0 },
    { "generate_master_bias",   OPK_MASTER,    0, 0 },
    { "generate_master_dark",   OPK_MASTER,    0, 0 },
    { "generate_master_flat",   OPK_MASTER,    0, 1 },
    { "generate_master_bias_f64", OPK_MASTER,  1, 0 },
    { "generate_master_dark_f64", OPK_MASTER,  1, 0 },
    { "generate_master_flat_f64", OPK_MASTER,  1, 1 }
};
const int OP_VOCAB_N = (int)(sizeof(OP_VOCAB) / sizeof(OP_VOCAB[0]));

/* 每类 op 必需 config 键 (master ops 不发明默认值: sigma/iter 由调用方明确
 * 给出; flat 的 legacy 签名无 combine → 不要求; bias/dark 要求 combine)。 */
int op_required_keys(const op_entry* e, const char** keys, char* kinds) {
    int n = 0;
    keys[n] = "op";           kinds[n++] = 's';
    keys[n] = "width";        kinds[n++] = 'n';
    keys[n] = "height";       kinds[n++] = 'n';
    if (e->kind == OPK_MASTER) {
        keys[n] = "frames";         kinds[n++] = 'n';
        keys[n] = "sigma_low";      kinds[n++] = 'n';
        keys[n] = "sigma_high";     kinds[n++] = 'n';
        keys[n] = "max_iterations"; kinds[n++] = 'n';
        if (!e->is_flat) {
            keys[n] = "combine";    kinds[n++] = 's';
        }
    } else if (e->kind == OPK_CALIBRATE) {
        keys[n] = "dark_optimization"; kinds[n++] = 'b';
        keys[n] = "dark_scale_factor"; kinds[n++] = 'n';
    } else {   /* OPK_CORRECT */
        keys[n] = "hot_sigma";          kinds[n++] = 'n';
        keys[n] = "cold_sigma";         kinds[n++] = 'n';
        keys[n] = "method";             kinds[n++] = 's';
        keys[n] = "max_structure_size"; kinds[n++] = 'n';
    }
    return n;
}

/* 合同默认 (README §5 / ALG-CAL §8): execute 期缺席可回默认的键;
 * 必需键不在其列 (validate_config 已强制)。 */
struct exec_cfg {
    int        op_index;      /* OP_VOCAB 索引 */
    uint64_t   width;
    uint64_t   height;
    uint64_t   frames;
    double     sigma_low;
    double     sigma_high;
    uint64_t   max_iterations;
    int        combine;       /* AC_COMBINE_MEAN / AC_COMBINE_MEDIAN */
    int        dark_optimization;
    double     dark_scale_factor;
    double     hot_sigma;
    double     cold_sigma;
    int        method;        /* AC_METHOD_MEDIAN / AC_METHOD_BILINEAR */
    uint64_t   max_structure_size;
    uint64_t   max_workers;   /* 0 = 未给 (租约上限生效) */
};

/* 精确字符串匹配 (不截断): 词表串 → legacy 常量
 *   combine: "mean"→AC_COMBINE_MEAN(0) / "median"→AC_COMBINE_MEDIAN(1)
 *   method:  "median"→AC_METHOD_MEDIAN(0) / "bilinear"→AC_METHOD_BILINEAR(1) */
int sv_eq(const char* v, uint32_t vl, const char* lit) {
    const uint32_t ll = (uint32_t)std::strlen(lit);
    return (vl == ll && std::memcmp(v, lit, vl) == 0);
}

int parse_combine(const char* v, uint32_t l) {
    if (sv_eq(v, l, "mean")) return AC_COMBINE_MEAN;
    if (sv_eq(v, l, "median")) return AC_COMBINE_MEDIAN;
    return -1;   /* 词表外 combine */
}

int parse_method(const char* v, uint32_t l) {
    if (sv_eq(v, l, "median")) return AC_METHOD_MEDIAN;
    if (sv_eq(v, l, "bilinear")) return AC_METHOD_BILINEAR;
    return -1;
}

/* 执行上下文 (execute 一次一具; create 期 cfg 已 parse 成 exec_cfg 雏形) */
struct cal_exec_ctx {
    const acs_host_api_v1* host;
    exec_cfg               c;         /* create 期 cfg */
    int                    has_cfg;
    /* 租约状态 (execute 期) */
    uint32_t               leased;
    int                    lease_active;
    int                    omp_prev;
    /* legacy 调用统计 */
    int                    legacy_rc;
};

/* 实例 (host allocator calloc; ACS_OP_CREATE → 返回) */
struct cal_inst {
    acs_head               head;
    int                    state;      /* ACS_LC_STATE_* */
    volatile int32_t       cancel_req;
    uint64_t               exec_count;
    int                    last_op;    /* ACS_OP_* */
    acs_status             last_status;
    uint32_t               last_detail;
    /* create 期 config 快照 (exec 期 config 可覆盖) */
    exec_cfg               cfg;
    int                    has_cfg;
    const acs_host_api_v1* host;
};

/* ── config → exec_cfg (validate 语义: 必需键/范围) ── */

acs_status cfg_fill(const jc_keyval* kvs, int nkv, const op_entry* e,
                    exec_cfg* c, acs_error_info_v1* err) {
    std::memset(c, 0, sizeof(*c));
    c->op_index = (int)(e - OP_VOCAB);
    const char* keys[10];
    char kinds[10];
    const int rn = op_required_keys(e, keys, kinds);
    for (int i = 0; i < rn; ++i) {
        if (kv_find(kvs, nkv, keys[i]) < 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "required config key missing (op-dependent)",
                         101);
        }
    }
    uint64_t u = 0;
    double   f = 0.0;
    if (kv_u64(kvs, nkv, "width", &u) != 0 || u == 0 || u > 100000000u)
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     "width must be positive integer", 103);
    c->width = u;
    if (kv_u64(kvs, nkv, "height", &u) != 0 || u == 0 || u > 100000000u)
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     "height must be positive integer", 103);
    c->height = u;
    if (e->kind == OPK_MASTER) {
        if (kv_u64(kvs, nkv, "frames", &u) != 0 || u == 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "frames must be positive", 103);
        c->frames = u;
        if (kv_f64(kvs, nkv, "sigma_low", &f) != 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "sigma_low numeric required", 103);
        c->sigma_low = f;
        if (kv_f64(kvs, nkv, "sigma_high", &f) != 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "sigma_high numeric required", 103);
        c->sigma_high = f;
        if (kv_u64(kvs, nkv, "max_iterations", &u) != 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "max_iterations positive integer required", 103);
        c->max_iterations = u;
        const char* cv = NULL;
        uint32_t cl = 0;
        kv_str(kvs, nkv, "combine", &cv, &cl);
        const int cc = parse_combine(cv, cl);
        if (cc < 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "combine must be \"mean\"|\"median\"", 103);
        c->combine = cc;
    }
    if (e->kind == OPK_CALIBRATE) {
        int b = 0;
        if (kv_bool(kvs, nkv, "dark_optimization", &b) != 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "dark_optimization must be true|false", 102);
        c->dark_optimization = b;
        if (kv_f64(kvs, nkv, "dark_scale_factor", &f) != 0 || !(f >= 0.0) ||
            !std::isfinite(f))
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "dark_scale_factor finite >=0 required", 103);
        c->dark_scale_factor = f;
    }
    if (e->kind == OPK_CORRECT) {
        if (kv_f64(kvs, nkv, "hot_sigma", &f) != 0 || !std::isfinite(f))
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "hot_sigma finite required", 103);
        c->hot_sigma = f;
        if (kv_f64(kvs, nkv, "cold_sigma", &f) != 0 || !std::isfinite(f))
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "cold_sigma finite required", 103);
        c->cold_sigma = f;
        const char* mv = NULL;
        uint32_t ml = 0;
        kv_str(kvs, nkv, "method", &mv, &ml);
        const int mm = parse_method(mv, ml);
        if (mm < 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "method must be \"median\"|\"bilinear\"", 103);
        c->method = mm;
        if (kv_u64(kvs, nkv, "max_structure_size", &u) != 0 || u == 0)
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                         "max_structure_size positive required", 103);
        c->max_structure_size = u;
    }
    /* 可选 */
    if (kv_u64(kvs, nkv, "max_workers", &u) == 0) c->max_workers = u;
    return ACS_OK;
}

/* op 词表查找 → 静态表索引; 未命中 → -1 */
int op_lookup(const char* s, uint32_t l) {
    for (int i = 0; i < OP_VOCAB_N; ++i) {
        if ((uint32_t)std::strlen(OP_VOCAB[i].name) == l &&
            std::memcmp(OP_VOCAB[i].name, s, l) == 0) {
            return i;
        }
    }
    return -1;
}

}  // namespace acs_cal

/* ====================================================================== */
/* ====================================================================== */

namespace acs_cal {

/* master 输入: aux 数组元素逐个 base64 解码并拼接为 n*w*h*esz 连续栈 */
acs_status decode_stack(const jc_keyval* fr, uint64_t plane_bytes,
                        bytes* stack, uint64_t* n_out, acs_error_info_v1* err) {
    if (fr->kind != 'S' || fr->aux_n < 1) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest \"frames\" must be a non-empty base64 array", 110);
    }
    stack->resize(fr->aux_n * plane_bytes);
    for (int i = 0; i < fr->aux_n; ++i) {
        const char* b64 = fr->aux[2 * i];
        const uint64_t bl = (uint64_t)(uintptr_t)fr->aux[2 * i + 1];
        const int64_t dl = b64_decode_len(b64, bl);
        if (dl < 0 || (uint64_t)dl != plane_bytes) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest frame base64 invalid or size != width*height*dtype",
                         (dl < 0) ? 112u : 111u);
        }
        uint64_t wl = 0;
        if (b64_decode(b64, bl, stack->data() + i * plane_bytes,
                       plane_bytes, &wl) != 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest frame base64 decode failed", 112);
        }
    }
    *n_out = (uint64_t)fr->aux_n;
    return ACS_OK;
}

acs_status decode_plane(const char* b64, uint64_t bl, uint64_t plane_bytes,
                        bytes* plane, acs_error_info_v1* err, const char* what) {
    const int64_t dl = b64_decode_len(b64, bl);
    if (dl < 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest plane base64 invalid", 112);
    }
    if ((uint64_t)dl != plane_bytes) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest plane size mismatch", 111);
    }
    plane->resize(plane_bytes);
    uint64_t wl = 0;
    if (b64_decode(b64, bl, plane->data(), plane_bytes, &wl) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest plane base64 decode failed", 112);
    }
    (void)what;
    return ACS_OK;
}

/* 数字 → JSON (实际 K/计数) */
std::string num_json(double v) {
    char b[40];
    std::snprintf(b, sizeof(b), "%.17g", v);
    return std::string(b);
}

/* 输出 manifest (键序冻结; 事务性: 全部成功才返回非空串) */
acs_status build_out_manifest(const exec_cfg& c, int is_f64,
                              const uint8_t* out_plane, uint64_t plane_bytes,
                              double actual_k, int64_t hot, int64_t cold,
                              std::string* out_json, acs_error_info_v1* err) {
    std::string s = "{\"schema_version\":1,\"width\":";
    {
        char b[32];
        std::snprintf(b, sizeof(b), "%llu", (unsigned long long)c.width);
        s += b;
        s += ",\"height\":";
        std::snprintf(b, sizeof(b), "%llu", (unsigned long long)c.height);
        s += b;
    }
    s += ",\"dtype\":\"";
    s += is_f64 ? "f64" : "f32";
    s += "\",\"out_base64\":\"";
    const uint64_t el = b64_enc_len(plane_bytes);
    const uint64_t o0 = s.size();
    s.resize(o0 + el);
    b64_encode(out_plane, plane_bytes, &s[o0]);
    s += "\",\"op\":\"";
    s += OP_VOCAB[c.op_index].name;
    s += "\"";
    if (c.op_index >= 0 && OP_VOCAB[c.op_index].kind == OPK_CALIBRATE) {
        s += ",\"actual_k\":";
        s += num_json(actual_k);
    }
    if (c.op_index >= 0 && OP_VOCAB[c.op_index].kind == OPK_CORRECT) {
        char b[32];
        s += ",\"hot\":";
        std::snprintf(b, sizeof(b), "%lld", (long long)hot);
        s += b;
        s += ",\"cold\":";
        std::snprintf(b, sizeof(b), "%lld", (long long)cold);
        s += b;
    }
    s += "}";
    *out_json = s;
    return ACS_OK;
}

/* execute 主体 (状态护栏与租约在调用方) */
acs_status exec_run(cal_inst* inst, const exec_cfg& c,
                    const char* man_s, uint64_t man_n,
                    acs_strbuf_v1* out, acs_error_info_v1* err) {
    const op_entry* e = &OP_VOCAB[c.op_index];
    const int is_f64 = e->is_f64;
    const uint64_t esz = is_f64 ? 8u : 4u;

    /* ── manifest 解析 ── */
    if (!man_s || man_n == 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     "input manifest required", 110);
    }
    jc_doc md = { man_s, man_n, 0 };
    jc_keyval mkv[16];
    const char* maux[2 * 3 * ACS_CAL_MAX_STACK];
    int nmk = 0;
    if (jc_parse_object(&md, mkv, &nmk, 16, maux,
                        (int)(sizeof(maux) / sizeof(maux[0]))) != 0) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                     "manifest is not a flat JSON object", ACS_DIAG_ECODE_CONFIG_SCHEMA);
    }
    uint64_t u = 0;
    if (kv_u64(mkv, nmk, "schema_version", &u) != 0 || u != 1) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest schema_version must be 1", 110);
    }
    if (kv_u64(mkv, nmk, "width", &u) != 0 || u != c.width) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest width missing or != config width", 111);
    }
    if (kv_u64(mkv, nmk, "height", &u) != 0 || u != c.height) {
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                     "manifest height missing or != config height", 111);
    }
    {
        const char* dt = NULL;
        uint32_t dl = 0;
        if (kv_str(mkv, nmk, "dtype", &dt, &dl) != 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest dtype required", 110);
        }
        if (!sv_eq(dt, dl, is_f64 ? "f64" : "f32")) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest dtype does not match op precision", 111);
        }
    }
    const uint64_t plane_bytes = c.width * c.height * esz;

    /* ── 平面解码 ── */
    bytes light;        /* calibrate/correct 输入 */
    bytes stack;        /* master 输入 (n*w*h*esz) */
    uint64_t n_frames = 0;
    bytes pbias, pdark, pflat;
    const bool master = (e->kind == OPK_MASTER);
    if (master) {
        const int fri = kv_find(mkv, nmk, "frames");
        if (fri < 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest \"frames\" array required for master ops", 110);
        }
        acs_status st = decode_stack(&mkv[fri], plane_bytes, &stack,
                                     &n_frames, err);
        if (st != ACS_OK) return st;
    } else {
        const char* b64 = NULL;
        uint32_t bl = 0;
        if (kv_str(mkv, nmk, "data_base64", &b64, &bl) != 0) {
            return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_DATA,
                         "manifest data_base64 required", 110);
        }
        acs_status st = decode_plane(b64, bl, plane_bytes, &light, err, "data");
        if (st != ACS_OK) return st;
    }
    /* 可选 master 平面 (键缺席 = NULL → legacy 恒等路径, DEV-2 行为保持) */
    struct { const char* key; bytes* dst; } planes[3] = {
        { "master_bias", &pbias },
        { "master_dark", &pdark },
        { "master_flat", &pflat }
    };
    for (int i = 0; i < 3; ++i) {
        const int bi = kv_find(mkv, nmk, planes[i].key);
        if (bi < 0) continue;
        const char* b64 = mkv[bi].val;
        const uint32_t bl = mkv[bi].vlen;
        if (mkv[bi].kind != 's' || !b64) continue;   /* null → 缺席 */
        acs_status st = decode_plane(b64, bl, plane_bytes, planes[i].dst,
                                     err, planes[i].key);
        if (st != ACS_OK) return st;
    }

    /* ── 取消预检 (entry 粒度) ── */
    if (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled &&
        inst->host->cancel->is_cancelled(inst->host->cancel->user_data)) {
        return efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
                     "cancelled before execute start", ACS_DIAG_ECODE_NONE);
    }

    /* ── host executor 租约 (FORBID-003; cpu_heavy 必须租约: 纪律条款
     * "重计算禁止单线程" → acquire 失败/executor 缺失均为硬 BUDGET,
     * 不取 drizzle 降级单线程路径) ── */
    uint32_t leased = 0;
    int lease_active = 0;
    int omp_prev = -1;
    const acs_executor_v1* ex =
        (inst->host && inst->host->executor) ? inst->host->executor : NULL;
    if (!ex) {
        return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE,
                     "cpu_heavy module requires host executor", 105);
    }
    {
        uint32_t want = ex->max_workers ? ex->max_workers : ex->available_cpus;
        if (c.max_workers > 0 && c.max_workers < want) want = (uint32_t)c.max_workers;
        if (want < 1) want = 1;
        if (ex->acquire && ex->acquire(ex->user_data, want) == 0) {
            leased = want;
            lease_active = 1;
            omp_prev = CAL_OMP_GET();
            ac_set_num_threads((int)leased);   /* legacy 工具通道注入 OMP ICV */
        } else {
            return efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE,
                         "executor lease unavailable for cpu_heavy op", 105);
        }
    }

    /* ── legacy 科学调用 (1:1; 行为偏差三例均在 legacy 内, 保持) ── */
    bytes out_plane(plane_bytes);
    double actual_k = 0.0;
    int64_t hot = -1, cold = -1;
    int rc = AC_ERR_INTERNAL;
    if (master) {
        const int n = (int)n_frames;
        const int w = (int)c.width, h = (int)c.height;
        const int iters = (int)c.max_iterations;
        if (is_f64) {
            const double* st2 = (const double*)stack.data();
            double* o = (double*)out_plane.data();
            /* 显式分派 (按词表名; 不玩花活) */
            if (e->is_flat) {
                rc = ac_generate_master_flat_f64(
                    st2, n, w, h,
                    pbias.empty() ? (const double*)NULL : (const double*)pbias.data(),
                    o, c.sigma_low, c.sigma_high, iters);
            } else if (std::strcmp(e->name, "generate_master_bias_f64") == 0) {
                rc = ac_generate_master_bias_f64(st2, n, w, h, o,
                                                 c.sigma_low, c.sigma_high,
                                                 iters, c.combine);
            } else {
                rc = ac_generate_master_dark_f64(st2, n, w, h, o,
                                                 c.sigma_low, c.sigma_high,
                                                 iters, c.combine);
            }
        } else {
            const float* st2 = (const float*)stack.data();
            float* o = (float*)out_plane.data();
            if (e->is_flat) {
                rc = ac_generate_master_flat(
                    st2, n, w, h,
                    pbias.empty() ? (const float*)NULL : (const float*)pbias.data(),
                    o, (float)c.sigma_low, (float)c.sigma_high, iters);
            } else if (std::strcmp(e->name, "generate_master_bias") == 0) {
                rc = ac_generate_master_bias(st2, n, w, h, o,
                                             (float)c.sigma_low, (float)c.sigma_high,
                                             iters, c.combine);
            } else {
                rc = ac_generate_master_dark(st2, n, w, h, o,
                                             (float)c.sigma_low, (float)c.sigma_high,
                                             iters, c.combine);
            }
        }
    } else if (e->kind == OPK_CALIBRATE) {
        if (is_f64) {
            double ak = 0.0;
            rc = ac_calibrate_frame_f64(
                (const double*)light.data(), (int)c.width, (int)c.height,
                pdark.empty() ? (const double*)NULL : (const double*)pdark.data(),
                pflat.empty() ? (const double*)NULL : (const double*)pflat.data(),
                pbias.empty() ? (const double*)NULL : (const double*)pbias.data(),
                (double*)out_plane.data(),
                c.dark_optimization, c.dark_scale_factor, &ak);
            actual_k = ak;
        } else {
            float ak = 0.0f;
            rc = ac_calibrate_frame(
                (const float*)light.data(), (int)c.width, (int)c.height,
                pdark.empty() ? (const float*)NULL : (const float*)pdark.data(),
                pflat.empty() ? (const float*)NULL : (const float*)pflat.data(),
                pbias.empty() ? (const float*)NULL : (const float*)pbias.data(),
                (float*)out_plane.data(),
                c.dark_optimization, (float)c.dark_scale_factor, &ak);
            actual_k = (double)ak;
        }
    } else {   /* OPK_CORRECT */
        if (is_f64) {
            int nh = 0, nc = 0;
            rc = ac_correct_frame_f64(
                (const double*)light.data(), (int)c.width, (int)c.height,
                pdark.empty() ? (const double*)NULL : (const double*)pdark.data(),
                pbias.empty() ? (const double*)NULL : (const double*)pbias.data(),
                (double*)out_plane.data(),
                c.hot_sigma, c.cold_sigma, c.method,
                (int)c.max_structure_size, &nh, &nc);
            hot = nh; cold = nc;
        } else {
            int nh = 0, nc = 0;
            rc = ac_correct_frame(
                (const float*)light.data(), (int)c.width, (int)c.height,
                pdark.empty() ? (const float*)NULL : (const float*)pdark.data(),
                pbias.empty() ? (const float*)NULL : (const float*)pbias.data(),
                (float*)out_plane.data(),
                (float)c.hot_sigma, (float)c.cold_sigma, c.method,
                (int)c.max_structure_size, &nh, &nc);
            hot = nh; cold = nc;
        }
    }

    /* ── 还租 (先于错误映射; 保证平衡) ── */
    if (lease_active) {
        if (ex->release) ex->release(ex->user_data, leased);
        if (omp_prev > 0) CAL_OMP_SET(omp_prev);
    }

    if (rc != AC_OK) {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "legacy ac_* rejected (rc=%d)", rc);
        return efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_BACKEND, msg, 120);
    }

    /* ── 输出 manifest (事务: 此前任何路径不写 out) ── */
    std::string oj;
    acs_status st = build_out_manifest(c, is_f64, out_plane.data(), plane_bytes,
                                       actual_k, hot, cold, &oj, err);
    if (st != ACS_OK) return st;
    return strbuf_commit(out, oj.c_str(), (uint64_t)oj.size(), err);
}

}  // namespace acs_cal

/* ====================================================================== */
/* vtable 九操作 + 唯一导出入口 (异常屏障层)                                  */
/* ====================================================================== */

extern "C" {

/* ── describe (module 级; 静态串) ── */
static acs_status cal_describe(const acs_module_api_v1* self,
                               acs_str_v1 module_id,
                               acs_module_descriptor_v1* out_desc) {
    try {
        if (!out_desc) return ACS_ERR_PARAM;
        if (module_id.size != std::strlen(ASTROCS_CAL_MODULE_ID) ||
            std::memcmp(module_id.data, ASTROCS_CAL_MODULE_ID, module_id.size) != 0) {
            return ACS_ERR_ABI_MISMATCH;
        }
        std::memset(out_desc, 0, sizeof(*out_desc));
        out_desc->head.struct_size = (uint32_t)sizeof(acs_module_descriptor_v1);
        out_desc->head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->module_id.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->module_id.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->module_id.data = ASTROCS_CAL_MODULE_ID;
        out_desc->module_id.size = std::strlen(ASTROCS_CAL_MODULE_ID);
        out_desc->version.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->version.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->version.data = ASTROCS_CAL_VERSION;
        out_desc->version.size = std::strlen(ASTROCS_CAL_VERSION);
        out_desc->build_id.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->build_id.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->build_id.data = ASTROCS_CAL_BUILD_ID;
        out_desc->build_id.size = std::strlen(ASTROCS_CAL_BUILD_ID);
        out_desc->sci_id.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->sci_id.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->sci_id.data = ASTROCS_CAL_SCI_ID;
        out_desc->sci_id.size = std::strlen(ASTROCS_CAL_SCI_ID);
        out_desc->alg_id.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->alg_id.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->alg_id.data = ASTROCS_CAL_ALG_ID;
        out_desc->alg_id.size = std::strlen(ASTROCS_CAL_ALG_ID);
        out_desc->api_id.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        out_desc->api_id.head.abi_version = ACS_ABI_VERSION_V1;
        out_desc->api_id.data = ASTROCS_CAL_API_ID;
        out_desc->api_id.size = std::strlen(ASTROCS_CAL_API_ID);
        out_desc->phase = 1;
        out_desc->config_schema_ver = ASTROCS_CAL_CONFIG_SCHEMA_VER;
        out_desc->execution_class = 0;   /* cpu_heavy (module.yaml) */
        out_desc->parallel_ok = 1;       /* 经 host executor 租约 */
        out_desc->flags = 0;
        return ACS_OK;
    } catch (...) {
        return acs_cal::efill(NULL, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in describe", 130);
    }
}

/* ── validate_config (module 级; CLI-003: 不触发科学计算) ── */
static acs_status cal_validate_config(const acs_module_api_v1* self,
                                      acs_str_v1 config_json,
                                      acs_error_info_v1* err) {
    try {
        (void)self;
        if (!config_json.data || config_json.size == 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config JSON required", ACS_DIAG_ECODE_NULL_CONFIG);
        }
        acs_cal::jc_doc d = { config_json.data, config_json.size, 0 };
        acs_cal::jc_keyval kv[16];
        const char* aux[2 * 3 * ACS_CAL_MAX_STACK];
        int nkv = 0;
        if (acs_cal::jc_parse_object(&d, kv, &nkv, 16, aux,
                                     (int)(sizeof(aux) / sizeof(aux[0]))) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config is not a flat JSON object",
                                  ACS_DIAG_ECODE_CONFIG_SCHEMA);
        }
        acs_status st = acs_cal::cfg_check(kv, nkv, err);
        if (st != ACS_OK) return st;
        const char* opv = NULL;
        uint32_t opl = 0;
        if (acs_cal::kv_str(kv, nkv, "op", &opv, &opl) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op required", 101);
        }
        const int oi = acs_cal::op_lookup(opv, opl);
        if (oi < 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op outside vocabulary", 100);
        }
        acs_cal::exec_cfg c;
        return acs_cal::cfg_fill(kv, nkv, &acs_cal::OP_VOCAB[oi], &c, err);
    } catch (...) {
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in validate_config", 130);
    }
}

/* ── plan (module 级; work_units=数据事实, 不执行科学计算) ── */
static acs_status cal_plan(const acs_module_api_v1* self,
                           acs_str_v1 node_id,
                           acs_str_v1 config_json,
                           acs_strbuf_v1* out_plan_json,
                           acs_error_info_v1* err) {
    try {
        (void)self;
        if (!out_plan_json) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "plan output strbuf required",
                                  ACS_DIAG_ECODE_NULL_CALLBACK);
        }
        if (!config_json.data || config_json.size == 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config JSON required", ACS_DIAG_ECODE_NULL_CONFIG);
        }
        acs_cal::jc_doc d = { config_json.data, config_json.size, 0 };
        acs_cal::jc_keyval kv[16];
        const char* aux[2 * 3 * ACS_CAL_MAX_STACK];
        int nkv = 0;
        if (acs_cal::jc_parse_object(&d, kv, &nkv, 16, aux,
                                     (int)(sizeof(aux) / sizeof(aux[0]))) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config is not a flat JSON object",
                                  ACS_DIAG_ECODE_CONFIG_SCHEMA);
        }
        acs_status st = acs_cal::cfg_check(kv, nkv, err);
        if (st != ACS_OK) return st;
        const char* opv = NULL;
        uint32_t opl = 0;
        if (acs_cal::kv_str(kv, nkv, "op", &opv, &opl) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op required", 101);
        }
        const int oi = acs_cal::op_lookup(opv, opl);
        if (oi < 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op outside vocabulary", 100);
        }
        acs_cal::exec_cfg c;
        st = acs_cal::cfg_fill(kv, nkv, &acs_cal::OP_VOCAB[oi], &c, err);
        if (st != ACS_OK) return st;

        const acs_cal::op_entry* e = &acs_cal::OP_VOCAB[oi];
        const bool master = (e->kind == acs_cal::OPK_MASTER);
        /* work_units = 数据事实 (12 §4): master → frames; 帧级 → 像素 */
        const uint64_t work_units = master ? c.frames : (c.width * c.height);
        const char* axis = master ? "frame" : "pixel";

        char head[192];
        std::snprintf(head, sizeof(head),
                      "{\"plan_version\":%d,\"node_id\":\"%.*s\",\"op\":\"%s\","
                      "\"work_unit\":\"%s\",\"work_units\":%llu,"
                      "\"parallel_axis\":\"%s\",\"min_workers\":1,",
                      ASTROCS_CAL_PLAN_VERSION,
                      (int)(node_id.size > 96 ? 96 : node_id.size),
                      node_id.data ? node_id.data : "",
                      e->name, master ? "frame" : "pixel",
                      (unsigned long long)work_units, axis);
        char tail[160];
        std::snprintf(tail, sizeof(tail),
                      "\"memory_bytes_estimate\":%llu,\"io_bytes_estimate\":0,"
                      "\"cancel_support\":\"entry\"}",
                      (unsigned long long)(work_units *
                                           (e->is_f64 ? 8u : 4u)));
        const std::string js = std::string(head) + tail;
        return acs_cal::strbuf_commit(out_plan_json, js.c_str(),
                                      (uint64_t)js.size(), err);
    } catch (...) {
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in plan", 130);
    }
}

/* ── create (module 级 → 实例) ── */
static acs_status cal_create(const acs_module_api_v1* self,
                             acs_str_v1 config_json,
                             const acs_host_api_v1* host,
                             acs_module_instance_v1** out,
                             acs_error_info_v1* err) {
    try {
        (void)self;
        if (!out) return ACS_ERR_PARAM;
        *out = NULL;
        if (!host || !host->allocator) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "host allocator required",
                                  ACS_DIAG_ECODE_NULL_CALLBACK);
        }
        if (!config_json.data || config_json.size == 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config JSON required", ACS_DIAG_ECODE_NULL_CONFIG);
        }
        acs_cal::jc_doc d = { config_json.data, config_json.size, 0 };
        acs_cal::jc_keyval kv[16];
        const char* aux[2 * 3 * ACS_CAL_MAX_STACK];
        int nkv = 0;
        if (acs_cal::jc_parse_object(&d, kv, &nkv, 16, aux,
                                     (int)(sizeof(aux) / sizeof(aux[0]))) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config is not a flat JSON object",
                                  ACS_DIAG_ECODE_CONFIG_SCHEMA);
        }
        acs_status st = acs_cal::cfg_check(kv, nkv, err);
        if (st != ACS_OK) return st;
        const char* opv = NULL;
        uint32_t opl = 0;
        if (acs_cal::kv_str(kv, nkv, "op", &opv, &opl) != 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op required", 101);
        }
        const int oi = acs_cal::op_lookup(opv, opl);
        if (oi < 0) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "config.op outside vocabulary", 100);
        }
        acs_cal::exec_cfg c;
        st = acs_cal::cfg_fill(kv, nkv, &acs_cal::OP_VOCAB[oi], &c, err);
        if (st != ACS_OK) return st;

        acs_cal::cal_inst* inst = (acs_cal::cal_inst*)host->allocator->alloc(
            host->allocator->user_data, sizeof(acs_cal::cal_inst), 16);
        if (!inst) {
            return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                                  "instance allocation failed", 130);
        }
        std::memset(inst, 0, sizeof(*inst));
        inst->head.struct_size = (uint32_t)sizeof(acs_cal::cal_inst);
        inst->head.abi_version = ACS_ABI_VERSION_V1;
        inst->state = ACS_LC_STATE_CREATED;
        inst->host = host;
        inst->cfg = c;
        inst->has_cfg = 1;
        *out = (acs_module_instance_v1*)inst;
        return ACS_OK;
    } catch (...) {
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in create", 130);
    }
}

/* ── execute (实例级; 可重复; 状态护栏 → 租约 → legacy → 事务输出) ── */
static acs_status cal_execute(acs_module_instance_v1* inst_raw,
                              acs_str_v1 input_manifest_json,
                              acs_str_v1 config_json,
                              acs_strbuf_v1* out_manifest_json,
                              acs_error_info_v1* err) {
    try {
        acs_cal::cal_inst* inst = (acs_cal::cal_inst*)inst_raw;
        if (!inst || inst->head.abi_version != ACS_ABI_VERSION_V1) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "instance handle invalid",
                                  ACS_DIAG_ECODE_NULL_CALLBACK);
        }
        if (inst->state != ACS_LC_STATE_CREATED) {
            return acs_cal::efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_CONFIG,
                                  "instance not in CREATED state",
                                  ACS_DIAG_ECODE_ILLEGAL_STATE);
        }
        if (out_manifest_json) {
            out_manifest_json->size = 0;   /* 事务性起点 */
        }
        inst->state = ACS_LC_STATE_EXECUTING;
        acs_status st = ACS_OK;
        acs_cal::exec_cfg c;
        int have_c = 0;
        if (config_json.data && config_json.size > 0) {
            /* execute 期 config 覆盖 create 期 (同 schema 全量校验) */
            acs_cal::jc_doc d = { config_json.data, config_json.size, 0 };
            acs_cal::jc_keyval kv[16];
            const char* aux[2 * 3 * ACS_CAL_MAX_STACK];
            int nkv = 0;
            if (acs_cal::jc_parse_object(&d, kv, &nkv, 16, aux,
                                         (int)(sizeof(aux) / sizeof(aux[0]))) != 0) {
                st = acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                    "config is not a flat JSON object",
                                    ACS_DIAG_ECODE_CONFIG_SCHEMA);
            } else {
                st = acs_cal::cfg_check(kv, nkv, err);
                if (st == ACS_OK) {
                    const char* opv = NULL;
                    uint32_t opl = 0;
                    if (acs_cal::kv_str(kv, nkv, "op", &opv, &opl) != 0) {
                        st = acs_cal::efill(err, ACS_ERR_PARAM,
                                            ACS_ERR_DOMAIN_CONFIG,
                                            "config.op required", 101);
                    } else {
                        const int oi = acs_cal::op_lookup(opv, opl);
                        if (oi < 0) {
                            st = acs_cal::efill(err, ACS_ERR_PARAM,
                                                ACS_ERR_DOMAIN_CONFIG,
                                                "config.op outside vocabulary", 100);
                        } else {
                            st = acs_cal::cfg_fill(kv, nkv,
                                                   &acs_cal::OP_VOCAB[oi], &c, err);
                            if (st == ACS_OK) have_c = 1;
                        }
                    }
                }
            }
        } else if (inst->has_cfg) {
            c = inst->cfg;
            have_c = 1;
        } else {
            st = acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                "no config available (create or execute)",
                                ACS_DIAG_ECODE_NULL_CONFIG);
        }
        if (st == ACS_OK && have_c) {
            st = acs_cal::exec_run(inst, c, input_manifest_json.data,
                                   input_manifest_json.size,
                                   out_manifest_json, err);
        }
        inst->exec_count += 1;
        inst->last_op = ACS_OP_EXECUTE;
        inst->last_status = st;
        inst->state = ACS_LC_STATE_CREATED;   /* 可重复 execute */
        return st;
    } catch (const std::bad_alloc&) {
        if (inst_raw) ((acs_cal::cal_inst*)inst_raw)->state = ACS_LC_STATE_CREATED;
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "bad_alloc in execute", 130);
    } catch (...) {
        if (inst_raw) ((acs_cal::cal_inst*)inst_raw)->state = ACS_LC_STATE_CREATED;
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in execute", 130);
    }
}

/* ── inspect (实例级; const 只读; 可并发) ── */
static acs_status cal_inspect(const acs_module_instance_v1* inst_raw,
                              acs_strbuf_v1* out_json,
                              acs_error_info_v1* err) {
    try {
        const acs_cal::cal_inst* inst = (const acs_cal::cal_inst*)inst_raw;
        if (!inst || inst->head.abi_version != ACS_ABI_VERSION_V1) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "instance handle invalid",
                                  ACS_DIAG_ECODE_NULL_CALLBACK);
        }
        if (!out_json) {
            return acs_cal::efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                                  "inspect output strbuf required",
                                  ACS_DIAG_ECODE_NULL_CALLBACK);
        }
        char b[256];
        std::snprintf(b, sizeof(b),
                      "{\"module_id\":\"%s\",\"state\":\"%s\","
                      "\"exec_count\":%llu,\"last_op\":%d,"
                      "\"last_status\":%d,\"cancel_requested\":%s}",
                      ASTROCS_CAL_MODULE_ID,
                      inst->state == ACS_LC_STATE_CREATED ? "created" :
                      inst->state == ACS_LC_STATE_EXECUTING ? "executing" :
                      inst->state == ACS_LC_STATE_CANCELLING ? "cancelling" :
                      inst->state == ACS_LC_STATE_DESTROYED ? "destroyed" : "unknown",
                      (unsigned long long)inst->exec_count, inst->last_op,
                      (int)inst->last_status,
                      inst->cancel_req ? "true" : "false");
        return acs_cal::strbuf_commit(out_json, b, std::strlen(b), err);
    } catch (...) {
        return acs_cal::efill(err, ACS_ERR_EXCEPTION, ACS_ERR_DOMAIN_INTERNAL,
                              "exception in inspect", 130);
    }
}

/* ── request_cancel (实例级; 原子置位; 幂等) ── */
static acs_status cal_request_cancel(acs_module_instance_v1* inst_raw) {
    try {
        acs_cal::cal_inst* inst = (acs_cal::cal_inst*)inst_raw;
        if (!inst) return ACS_ERR_PARAM;
        inst->cancel_req = 1;
        if (inst->state == ACS_LC_STATE_EXECUTING) {
            inst->state = ACS_LC_STATE_CANCELLING;
        }
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_EXCEPTION;
    }
}

/* ── destroy (实例级; inst=NULL 空操作; double-destroy 检测) ── */
static void cal_destroy(acs_module_instance_v1* inst_raw) {
    acs_cal::cal_inst* inst = (acs_cal::cal_inst*)inst_raw;
    if (!inst) return;
    if (inst->head.abi_version != ACS_ABI_VERSION_V1) return;
    if (inst->state == ACS_LC_STATE_DESTROYED) return;   /* double → 忽略 */
    const acs_host_api_v1* host = inst->host;
    inst->state = ACS_LC_STATE_DESTROYED;
    if (host && host->allocator && host->allocator->free) {
        host->allocator->free(host->allocator->user_data, inst);
    }
}

/* ── vtable (静态; head 由 query 填) ── */
static acs_module_api_v1 g_cal_module_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    &cal_describe,
    &cal_validate_config,
    &cal_plan,
    &cal_create,
    &cal_execute,
    &cal_inspect,
    &cal_request_cancel,
    &cal_destroy
};

/* ── 唯一导出 (ABI-006; host_abi 失配 → MISMATCH 不降级) ── */
acs_status astrocs_module_query_v1(uint32_t host_abi,
                                   const acs_host_api_v1* host,
                                   const acs_module_api_v1** out_api) {
    try {
        if (!out_api) return ACS_ERR_PARAM;
        *out_api = NULL;
        if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
        /* allocator 必填校验在 create 期 (query 期 host 可 NULL 于探针) */
        (void)host;
        *out_api = &g_cal_module_api;
        return ACS_OK;
    } catch (...) {
        return ACS_ERR_EXCEPTION;
    }
}

}  /* extern "C" */
