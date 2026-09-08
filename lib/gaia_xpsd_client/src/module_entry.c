/* AstroCS catalog gaia service module — C ABI adapter (CAT-GAIA-IMPL)
 *
 * 文件: lib/gaia_xpsd_client/src/module_entry.c
 *
 * 角色 (12_DLL_ABI_AND_LOADER_STANDARD §1/§2; MODULE_MIGRATION_TEMPLATE
 * "<prefix>-IMPL"; GAIA_QUERY.md §3.1 迁移合同):
 *   把 legacy 生产实现 (src/gaia_client.c, 12 个 GAIA_EXPORT C API) 包装为
 *   模块 C ABI v1 DLL astrocs_catalog_gaia —— 唯一导出 astrocs_module_query_v1;
 *   legacy 符号经 -DGAIA_EXPORT= 本地化 (不进导出面, ABI-006 全查)。
 *
 * 生命周期 (lifecycle_v1.h 冻结时序): query → describe/validate_config/plan
 *   → create → execute* → inspect → request_cancel → destroy。
 *   同实例 execute 互斥 (STATE 拒绝); inspect 并发安全 (只读诊断);
 *   request_cancel 幂等置位。
 *
 * 迁移要点:
 *   - plan(): 由输入元数据 (XPSD 目录头+树节点, 不解压数据块/不执行查询) 经
 *     gaia_client_collect_plan_stats 真实推导 work_units/IO/memory/parallel
 *     axis=file/min-max workers=1..file_count —— 禁空转假 plan。
 *   - execute(): heavy 工作 = 查询 (并行轴=文件); 经 host executor 租借
 *     min(max_workers, executor->max_workers, file_count) 线程
 *     (gaia_set_worker_lease 注入; 无 executor 的技术预览路径保持历史默认
 *     team)。cancel 检查点 = 文件循环边界 (gaia_set_cancel_checkpoint)。
 *   - 输出事务 sink: ABI v1 无 artifact 写回调 → 结果以 manifest JSON 经
 *     调用方 strbuf 两阶段提交 (尺寸查询→写入; 不足→PARAM+BUFFER_TOO_SMALL,
 *     不半写)。行数据 base64 (GaiaStar 等结构体原样字节, 供 direct-vs-plugin
 *     bitwise 对比与消费方解码)。
 *   - 输入 typed: config JSON 键词表冻结于 include/astrocs/gaia/types.h;
 *     catalog_dir 为数据集 port (catalog.xpsd_dir), 模块不自拼路径。
 *
 * 编译合同: 纯 C11; 无 STL/异常/RTTI; -fno-exceptions 亦可编译;
 *   ASTROCS_ABI_SHARED+ASTROCS_ABI_EXPORTS 由 CMake target 定义。
 */
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/gaia/types.h"
#include "gaia_client.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif

/* ───────── 静态字符串 (descriptor 输出; 所有权=module 静态) ───────── */

static const char kModuleId[] = ASTROCS_GAIA_MODULE_ID;
static const char kVersion[]  = ASTROCS_GAIA_VERSION;
static const char kBuildId[]  = ASTROCS_GAIA_BUILD_ID;
static const char kSciId[]    = ASTROCS_GAIA_SCI_ID;
static const char kAlgId[]    = ASTROCS_GAIA_ALG_ID;
static const char kApiId[]    = ASTROCS_GAIA_API_ID;
/* 日志组件名（host log 通道标识；inspect/describe JSON 不输出, 保留给
 * 未来 host->log 接入; 防 -Wunused 用 attribute） */
#if defined(__GNUC__)
static const char kLogComponent[] __attribute__((unused)) = "astrocs.catalog.gaia";
#else
static const char kLogComponent[] = "astrocs.catalog.gaia";
#endif

/* ───────── 基础 helper (对齐 lifecycle_v1.h 诊断/strbuf 冻结语义) ───────── */

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

/* strbuf 写 N 字节 (lifecycle_v1.h 截断语义: size=所需; cap>0 写前缀+NUL;
 * 不足 → PARAM + BUFFER_TOO_SMALL) */
static acs_status strbuf_write(acs_strbuf_v1* out, const char* data, uint64_t n,
                               acs_error_info_v1* err) {
    if (!out) { efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                      ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: null output buffer");
                return ACS_ERR_PARAM; }
    out->size = n;
    if (!out->data || out->cap == 0) return ACS_OK;   /* 只问尺寸 */
    uint64_t room = out->cap - 1;                     /* 留 NUL 位 */
    uint64_t wr = n < room ? n : room;
    if (wr > 0) memcpy(out->data, data, (size_t)wr);
    out->data[wr] = '\0';
    if (n >= out->cap) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_BUFFER_TOO_SMALL, "gaia: output buffer too small");
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

static acs_status strbuf_write_cstr(acs_strbuf_v1* out, const char* s,
                                    acs_error_info_v1* err) {
    return strbuf_write(out, s, s ? (uint64_t)strlen(s) : 0, err);
}

/* ───────── mini JSON 读取器 (config 借入; 只读不分配) ───────── */

/* 在 obj 顶层找 "key":"..." (转义最小支持 \\ \" \/) → 返回值首指针与长度 */
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
        if (*p == '\\' && p[1]) p++;   /* 跳过转义对 */
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

/* 在 obj 顶层找 "key":<num> → strtod; *found 置 1 */
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

/* "key":[f64,...] 数字数组 → malloc 数组 (调用方 free); 失败/缺失返回 NULL,
 * *out_n=元素数 (语法坏 → NULL 且 *out_n=-1) */
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
    /* 两遍: 先数元素 */
    int count = 0; const char* q = p;
    while (*q && *q != ']') {
        if (*q == '-' || (*q >= '0' && *q <= '9')) { count++; strtod(q, (char**)&q); }
        else q++;
    }
    if (*q != ']') { *out_n = -1; return NULL; }
    double* arr = (double*)malloc((size_t)(count > 0 ? count : 1) * sizeof(double));
    if (!arr) { *out_n = -1; return NULL; }
    q = p; int i = 0;
    while (*q && *q != ']' && i < count) {
        if (*q == '-' || (*q >= '0' && *q <= '9')) { arr[i++] = strtod(q, (char**)&q); }
        else q++;
    }
    *out_n = count;
    return arr;
}

/* ───────── config 解析与校验 (validate_config/plan/create 共享) ───────── */

typedef struct {
    char op[32];
    char catalog_dir[1024];
    int  db_type;               /* GaiaDbType; 0=AUTO */
    uint32_t max_workers;       /* 0=未指定 (用 host executor 上限) */
    double ra, dec, radius_deg, mag_low, mag_high;
    double match_radius_arcsec;
    double *ra_list, *dec_list; /* by_coords; create 所有权, destroy 释放 */
    int n_coords;
} gaia_cfg;

/* config 键必需性 + 有限值校验 (detail 102=缺失, 103=非有限;
 * README 已知限制 7 前置条件强化为稳定错误) */
static acs_status gaia_cfg_check_params(const gaia_cfg* c, int ra_ok, int dec_ok,
                                        int rad_ok, int lo_ok, int hi_ok,
                                        int mr_ok, acs_error_info_v1* err) {
    int need_ra = 0, need_dec = 0, need_rad = 0, need_lo = 0, need_hi = 0, need_mr = 0;
    if (!strcmp(c->op, ASTROCS_GAIA_OP_CONE) ||
        !strcmp(c->op, ASTROCS_GAIA_OP_CONE_SPEC) ||
        !strcmp(c->op, ASTROCS_GAIA_OP_CONE_PHOT)) {
        need_ra = need_dec = need_rad = need_lo = need_hi = 1;
    } else if (!strcmp(c->op, ASTROCS_GAIA_OP_CONE_SOLVER)) {
        need_ra = need_dec = need_rad = need_hi = 1;
    } else if (!strcmp(c->op, ASTROCS_GAIA_OP_SPEC_BY_COORDS)) {
        need_mr = need_lo = need_hi = 1;
        if (c->n_coords <= 0 || !c->ra_list || !c->dec_list) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_GAIA_ECODE_PARAM_MISSING, "gaia: missing ra_list/dec_list");
            return ACS_ERR_PARAM;
        }
    }
    if ((need_ra && !ra_ok) || (need_dec && !dec_ok) || (need_rad && !rad_ok) ||
        (need_lo && !lo_ok) || (need_hi && !hi_ok) || (need_mr && !mr_ok)) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_GAIA_ECODE_PARAM_MISSING, "gaia: missing required param(s)");
        return ACS_ERR_PARAM;
    }
    if ((need_ra && !isfinite(c->ra)) || (need_dec && !isfinite(c->dec)) ||
        (need_rad && !isfinite(c->radius_deg)) ||
        (need_lo && !isfinite(c->mag_low)) || (need_hi && !isfinite(c->mag_high)) ||
        (need_mr && !isfinite(c->match_radius_arcsec))) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_GAIA_ECODE_PARAM_NOT_FINITE, "gaia: non-finite param");
        return ACS_ERR_PARAM;
    }
    return ACS_OK;
}

static int op_is_known(const char* op) {
    return !strcmp(op, ASTROCS_GAIA_OP_CONE) ||
           !strcmp(op, ASTROCS_GAIA_OP_CONE_SOLVER) ||
           !strcmp(op, ASTROCS_GAIA_OP_CONE_SPEC) ||
           !strcmp(op, ASTROCS_GAIA_OP_CONE_PHOT) ||
           !strcmp(op, ASTROCS_GAIA_OP_SPEC_BY_COORDS);
}

/* 解析 config; 校验失败返回非 0 acs_status (err 已填) */
static acs_status gaia_cfg_parse(const char* config_json, gaia_cfg* c,
                                 acs_error_info_v1* err) {
    memset(c, 0, sizeof(*c));
    if (!config_json || !*config_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CONFIG, "gaia: empty config");
        return ACS_ERR_PARAM;
    }
    if (!json_copy_str(config_json, ASTROCS_GAIA_CFG_KEY_OP,
                       c->op, sizeof(c->op))) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_CONFIG_SCHEMA, "gaia: missing op");
        return ACS_ERR_PARAM;
    }
    if (!op_is_known(c->op)) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_GAIA_ECODE_OP_UNKNOWN, "gaia: unknown op");
        return ACS_ERR_PARAM;
    }
    if (!json_copy_str(config_json, ASTROCS_GAIA_CFG_KEY_CATALOG_DIR,
                       c->catalog_dir, sizeof(c->catalog_dir)) ||
        !c->catalog_dir[0]) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_GAIA_ECODE_CATALOG_DIR_MISSING, "gaia: missing catalog_dir");
        return ACS_ERR_PARAM;
    }
    int f = 0;
    int ra_ok = 0, dec_ok = 0, rad_ok = 0, lo_ok = 0, hi_ok = 0, mr_ok = 0;
    c->ra          = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_RA, &ra_ok);
    c->dec         = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_DEC, &dec_ok);
    c->radius_deg  = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_RADIUS, &rad_ok);
    c->mag_low     = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_MAG_LOW, &lo_ok);
    c->mag_high    = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_MAG_HIGH, &hi_ok);
    c->match_radius_arcsec =
               json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_MATCH_RADIUS, &mr_ok);
    {
        int fdb = 0;
        double db = json_get_f64(config_json, ASTROCS_GAIA_CFG_KEY_DB_TYPE, &fdb);
        c->db_type = fdb ? (int)db : 0;
        if (c->db_type < 0 || c->db_type > 2) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_DIAG_ECODE_CONFIG_SCHEMA, "gaia: bad db_type");
            return ACS_ERR_PARAM;
        }
    }
    {
        int fw = 0;
        uint64_t w = json_get_u64(config_json, ASTROCS_GAIA_CFG_KEY_MAX_WORKERS, &fw);
        c->max_workers = fw ? (uint32_t)w : 0;
    }
    if (!strcmp(c->op, ASTROCS_GAIA_OP_SPEC_BY_COORDS)) {
        int nr = 0, nd = 0;
        c->ra_list  = json_get_f64_array(config_json, ASTROCS_GAIA_CFG_KEY_RA_LIST, &nr);
        c->dec_list = json_get_f64_array(config_json, ASTROCS_GAIA_CFG_KEY_DEC_LIST, &nd);
        if (nr < 0 || nd < 0 || nr != nd) {
            free(c->ra_list); free(c->dec_list);
            c->ra_list = NULL; c->dec_list = NULL;
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_DIAG_ECODE_CONFIG_SCHEMA, "gaia: bad ra_list/dec_list");
            return ACS_ERR_PARAM;
        }
        c->n_coords = nr;
    }
    acs_status pchk = gaia_cfg_check_params(c, ra_ok, dec_ok, rad_ok,
                                            lo_ok, hi_ok, mr_ok, err);
    if (pchk != ACS_OK) {
        free(c->ra_list); free(c->dec_list);
        c->ra_list = NULL; c->dec_list = NULL;
        c->n_coords = 0;
        return pchk;
    }
    return ACS_OK;
}

static void gaia_cfg_free(gaia_cfg* c) {
    free(c->ra_list);
    free(c->dec_list);
    c->ra_list = NULL;
    c->dec_list = NULL;
    c->n_coords = 0;
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

/* JSON 字符串值内联 (catalog_dir 等借用值; 只处理 " 与 \, 控制字符降为 '?') */
static void json_append_escaped(char** w, const char* s) {
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { *(*w)++ = '\\'; *(*w)++ = *s; }
        else if ((unsigned char)*s < 0x20) *(*w)++ = '?';
        else *(*w)++ = *s;
    }
}

/* ───────── 实例 ───────── */

typedef struct {
    uint32_t state;                 /* ACS_LC_STATE_* */
    int executing;                  /* 同实例 execute 互斥旗标 (state 机护栏) */
    gaia_cfg cfg;                   /* create 时解析的 config 副本 */
    GaiaClient* client;             /* legacy 实例 (缓存生命周期=实例) */
    const acs_host_api_v1* host;
    volatile int cancel_req;        /* request_cancel 置位; execute 读 */
    int cancel_active;              /* execute 期=1 (cancel fn 注入期) */
    uint64_t exec_count;
    acs_status last_status;
    char last_op[32];
    uint32_t last_workers;
} gaia_inst;

/* execute 期 cancel 轮询 (文件循环边界检查点; 原子语义读) */
static int gaia_inst_cancel_poll(void* ud) {
    gaia_inst* inst = (gaia_inst*)ud;
    if (!inst) return 0;
    if (inst->cancel_req) return 1;
    if (inst->host && inst->host->cancel && inst->host->cancel->is_cancelled)
        return inst->host->cancel->is_cancelled(inst->host->cancel->user_data) ? 1 : 0;
    return 0;
}

/* ───────── describe ───────── */

static acs_status gaia_describe(const acs_module_api_v1* self,
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
    out_desc->version = acs_str_from(kVersion);
    out_desc->build_id = acs_str_from(kBuildId);
    out_desc->sci_id = acs_str_from(kSciId);
    out_desc->alg_id = acs_str_from(kAlgId);
    out_desc->api_id = acs_str_from(kApiId);
    out_desc->phase = 0;              /* service/catalog (module.yaml 注册表) */
    out_desc->config_schema_ver = 1;  /* data schema version, 与 ABI 分开 (12 §4) */
    out_desc->execution_class = 1;    /* io_bound (module.yaml resource_class) */
    out_desc->parallel_ok = 1;        /* 并行轴=文件, 经 host executor */
    out_desc->flags = 0;
    return ACS_OK;
}

/* ───────── validate_config ───────── */

static acs_status gaia_validate_config(const acs_module_api_v1* self,
                                       acs_str_v1 config_json,
                                       acs_error_info_v1* err) {
    (void)self;
    gaia_cfg tmp;
    acs_status st = gaia_cfg_parse(config_json.data, &tmp, err);
    gaia_cfg_free(&tmp);   /* 解析出的数组释放 (validate 无分配保留) */
    return st;
}

/* ───────── plan: 真实 work_units (元数据遍历, 禁空转) ───────── */

static acs_status gaia_plan(const acs_module_api_v1* self,
                            acs_str_v1 node_id,
                            acs_str_v1 config_json,
                            acs_strbuf_v1* out_plan_json,
                            acs_error_info_v1* err) {
    (void)self;
    (void)node_id;
    if (!out_plan_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: null plan buffer");
        return ACS_ERR_PARAM;
    }
    gaia_cfg c;
    acs_status st = gaia_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) { gaia_cfg_free(&c); return st; }

    /* plan 不执行科学计算: 只打开数据集读头+树节点 (元数据级只读 I/O) */
    GaiaClient* client = gaia_client_create_ex(c.catalog_dir, (GaiaDbType)c.db_type);
    if (!client) {
        gaia_cfg_free(&c);
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
              ACS_GAIA_ECODE_CATALOG_OPEN_FAIL, "gaia: catalog open failed");
        return ACS_ERR_IO;
    }
    GaiaPlanStats stats;
    if (gaia_client_collect_plan_stats(client, &stats) != 0) {
        gaia_client_destroy(client);
        gaia_cfg_free(&c);
        efill(err, ACS_ERR_INTERNAL, ACS_ERR_DOMAIN_INTERNAL,
              ACS_GAIA_ECODE_CATALOG_OPEN_FAIL, "gaia: plan stats failed");
        return ACS_ERR_INTERNAL;
    }
    gaia_client_destroy(client);

    /* memory 估计: mmap 常驻 + 每 worker scratch + 查询/块缓存合同上限 */
    const long long kBlockCacheCap = (long long)(4ULL * 1024 * 1024 * 1024);
    const long long kQueryCacheCap = 64LL * 200000LL * (long long)(sizeof(double) * 3);
    long long mem_est = stats.mmap_bytes
                      + stats.max_block_bytes * (stats.file_count > 0 ? stats.file_count : 1)
                      + kBlockCacheCap + kQueryCacheCap;

    char buf[1024];
    char* w = buf;
    *w++ = '{';
    w += snprintf(w, (size_t)(buf + sizeof(buf) - w),
                  "\"module_id\":\"%s\",\"plan_version\":%d,\"op\":\"%s\","
                  "\"file_count\":%d,\"db_type\":%d,\"spec_file_count\":%d,"
                  "\"work_units\":%lld,\"work_unit\":\"leaf_block_bytes\","
                  "\"leaf_blocks\":%lld,"
                  "\"io_bytes_estimate\":%lld,\"compressed_bytes\":%lld,"
                  "\"mmap_bytes\":%lld,\"memory_bytes_estimate\":%lld,"
                  "\"scratch_per_worker_bytes\":%lld,"
                  "\"parallel_axis\":\"file\",\"min_workers\":1,"
                  "\"max_workers\":%d}",
                  kModuleId, ASTROCS_GAIA_PLAN_VERSION, c.op,
                  stats.file_count, stats.db_type, stats.spec_file_count,
                  (long long)stats.work_units_bytes,
                  (long long)stats.leaf_blocks,
                  (long long)stats.work_units_bytes,
                  (long long)stats.compressed_bytes,
                  (long long)stats.mmap_bytes,
                  mem_est,
                  (long long)stats.max_block_bytes,
                  stats.file_count);
    *w = '\0';
    gaia_cfg_free(&c);

    st = strbuf_write_cstr(out_plan_json, buf, err);
    return st;
}

/* ───────── create ───────── */

static acs_status gaia_create(const acs_module_api_v1* self,
                              acs_str_v1 config_json,
                              const acs_host_api_v1* host,
                              acs_module_instance_v1** out,
                              acs_error_info_v1* err) {
    (void)self;
    if (!out) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: null out");
        return ACS_ERR_PARAM;
    }
    *out = NULL;
    if (!host || !host->allocator) {
        efill(err, ACS_ERR_ABI_MISMATCH, ACS_ERR_DOMAIN_INTERNAL,
              ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: host allocator required");
        return ACS_ERR_ABI_MISMATCH;
    }
    gaia_cfg c;
    acs_status st = gaia_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) { gaia_cfg_free(&c); return st; }

    gaia_inst* inst = (gaia_inst*)calloc(1, sizeof(gaia_inst));
    if (!inst) {
        gaia_cfg_free(&c);
        efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
              ACS_DIAG_ECODE_NONE, "gaia: inst alloc failed");
        return ACS_ERR_NOMEM;
    }
    inst->state = ACS_LC_STATE_CREATED;
    inst->host = host;
    inst->cfg = c;   /* 接管 ra_list/dec_list 所有权 */
    inst->last_status = ACS_OK;

    inst->client = gaia_client_create_ex(c.catalog_dir, (GaiaDbType)c.db_type);
    if (!inst->client) {
        free(inst);
        gaia_cfg_free(&c);
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
              ACS_GAIA_ECODE_CATALOG_OPEN_FAIL, "gaia: catalog open failed");
        return ACS_ERR_IO;
    }
    *out = (acs_module_instance_v1*)inst;
    return ACS_OK;
}

/* ───────── execute: 租借 + 文件边界取消 + 事务 sink 输出 ───────── */

static acs_status gaia_execute(acs_module_instance_v1* inst_raw,
                               acs_str_v1 input_manifest_json,
                               acs_str_v1 config_json,
                               acs_strbuf_v1* out_manifest_json,
                               acs_error_info_v1* err) {
    (void)input_manifest_json;   /* v1: config 承载 op+参数 (types.h 合同) */
    gaia_inst* inst = (gaia_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state != ACS_LC_STATE_CREATED || inst->executing) {
        efill(err, ACS_ERR_STATE, ACS_ERR_DOMAIN_INTERNAL,
              ACS_DIAG_ECODE_ILLEGAL_STATE, "gaia: instance busy/state");
        return ACS_ERR_STATE;
    }
    /* execute 期 config 可覆盖 op 参数; 仍走同一解析 (词表/有限值校验同) */
    gaia_cfg c;
    acs_status st = gaia_cfg_parse(config_json.data, &c, err);
    if (st != ACS_OK) return st;

    inst->executing = 1;
    inst->cancel_req = 0;
    inst->cancel_active = 1;
    gaia_set_cancel_checkpoint(gaia_inst_cancel_poll, inst);

    /* host executor 租借 (并行轴=文件): min(请求, 授权, file_count);
     * 无 executor (技术预览 host) → 历史默认 team, 不私建线程池。 */
    uint32_t file_count = (uint32_t)gaia_client_get_file_count(inst->client);
    uint32_t leased = 0;
    int lease_active = 0;
    if (inst->host && inst->host->executor) {
        const acs_executor_v1* ex = inst->host->executor;
        uint32_t want = file_count > 0 ? file_count : 1;
        if (c.max_workers > 0 && c.max_workers < want) want = c.max_workers;
        if (ex->max_workers > 0 && ex->max_workers < want) want = ex->max_workers;
        if (want < 1) want = 1;
        if (ex->acquire && ex->acquire(ex->user_data, want) != 0) {
            gaia_set_cancel_checkpoint(NULL, NULL);
            inst->cancel_active = 0;
            inst->executing = 0;
            gaia_cfg_free(&c);
            inst->exec_count++;
            inst->last_status = ACS_ERR_BUDGET;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
            efill(err, ACS_ERR_BUDGET, ACS_ERR_DOMAIN_RESOURCE,
                  ACS_DIAG_ECODE_NONE, "gaia: thread budget exhausted");
            return ACS_ERR_BUDGET;
        }
        leased = want;
        lease_active = 1;
        gaia_set_worker_lease((int)leased);
    } else {
        gaia_set_worker_lease(0);   /* 历史 OpenMP 默认 team */
    }

    /* ── 分派 legacy 实现 ── */
    GaiaClient* cli = inst->client;
    void* row_ptr = NULL;          /* GaiaStar* / GaiaSpectrumStar* / GaiaPhotometryStar* */
    uint8_t* spectra = NULL;       /*光谱/光谱参数输出*/
    int* match_idx = NULL;
    double *solver_ra = NULL, *solver_dec = NULL;
    float *solver_mag = NULL;
    int count = 0;
    int spec_start = 0, spec_step = 0, spec_count = 0;
    int legacy_rc = -1;

    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE)) {
        GaiaStar* stars = NULL;
        legacy_rc = gaia_client_cone_search(cli, c.ra, c.dec, c.radius_deg,
                                            c.mag_low, c.mag_high, &stars, &count);
        row_ptr = stars;
    } else if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_SOLVER)) {
        legacy_rc = gaia_client_cone_search_for_solver(cli, c.ra, c.dec, c.radius_deg,
                                                       c.mag_high,
                                                       &solver_ra, &solver_dec,
                                                       &solver_mag, &count);
    } else if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_SPEC)) {
        GaiaSpectrumStar* stars = NULL;
        legacy_rc = gaia_client_cone_search_with_spectrum(cli, c.ra, c.dec, c.radius_deg,
                                                          c.mag_low, c.mag_high,
                                                          &stars, &spectra, &count);
        row_ptr = stars;
    } else if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_PHOT)) {
        GaiaPhotometryStar* stars = NULL;
        legacy_rc = gaia_client_cone_search_with_photometry(cli, c.ra, c.dec, c.radius_deg,
                                                            c.mag_low, c.mag_high,
                                                            &stars, &count);
        row_ptr = stars;
    } else { /* ASTROCS_GAIA_OP_SPEC_BY_COORDS */
        GaiaSpectrumStar* stars = NULL;
        legacy_rc = gaia_client_query_spectrum_by_coords(cli,
                                                         c.ra_list, c.dec_list, c.n_coords,
                                                         c.match_radius_arcsec,
                                                         c.mag_low, c.mag_high,
                                                         &stars, &spectra,
                                                         &match_idx, &count);
        row_ptr = stars;
    }

    int cancelled = gaia_inst_cancel_poll(inst);
    gaia_set_worker_lease(0);
    gaia_set_cancel_checkpoint(NULL, NULL);
    inst->cancel_active = 0;
    if (lease_active && inst->host && inst->host->executor)
        inst->host->executor->release(inst->host->executor->user_data, leased);

    /* 取消检查点命中: 不写输出 (事务性: 部分结果不外泄) */
    if (cancelled) {
        free(row_ptr); free(spectra); free(match_idx);
        free(solver_ra); free(solver_dec); free(solver_mag);
        gaia_cfg_free(&c);
        inst->executing = 0;
        inst->exec_count++;
        inst->last_status = ACS_ERR_CANCELLED;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
        efill(err, ACS_ERR_CANCELLED, ACS_ERR_DOMAIN_CANCELLED,
              ACS_DIAG_ECODE_NONE, "gaia: cancelled at file boundary");
        return ACS_ERR_CANCELLED;
    }
    if (legacy_rc != 0) {
        free(row_ptr); free(spectra); free(match_idx);
        free(solver_ra); free(solver_dec); free(solver_mag);
        gaia_cfg_free(&c);
        inst->executing = 0;
        inst->exec_count++;
        inst->last_status = ACS_ERR_IO;
        snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
        efill(err, ACS_ERR_IO, ACS_ERR_DOMAIN_IO,
              ACS_GAIA_ECODE_CATALOG_OPEN_FAIL, "gaia: legacy query failed");
        return ACS_ERR_IO;
    }

    /* 光谱参数 (spectrum ops; 失败非致命) */
    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_SPEC) ||
        !strcmp(c.op, ASTROCS_GAIA_OP_SPEC_BY_COORDS)) {
        gaia_client_get_spectrum_params(cli, &spec_start, &spec_step, &spec_count);
    }

    /* ── 组装输出 manifest (事务 sink: 两阶段 strbuf) ── */
    uint64_t row_bytes = 0;
    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE))            row_bytes = (uint64_t)count * sizeof(GaiaStar);
    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_SPEC) ||
        !strcmp(c.op, ASTROCS_GAIA_OP_SPEC_BY_COORDS))  row_bytes = (uint64_t)count * sizeof(GaiaSpectrumStar);
    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_PHOT))       row_bytes = (uint64_t)count * sizeof(GaiaPhotometryStar);

    uint64_t spec_bytes = (spectra && count > 0)
                        ? (uint64_t)count * (uint64_t)spec_count : 0;
    uint64_t b64_rows_cap = row_bytes ? b64_encoded_len(row_bytes) : 3;
    uint64_t b64_spec_cap = spec_bytes ? b64_encoded_len(spec_bytes) : 3;
    uint64_t b64_idx_cap  = match_idx ? b64_encoded_len((uint64_t)count * sizeof(int)) : 3;
    uint64_t b64_sra_cap  = solver_ra  ? b64_encoded_len((uint64_t)count * sizeof(double)) : 3;
    uint64_t b64_sdec_cap = solver_dec ? b64_encoded_len((uint64_t)count * sizeof(double)) : 3;
    uint64_t b64_smag_cap = solver_mag ? b64_encoded_len((uint64_t)count * sizeof(float)) : 3;

    /* 第一遍: 只算尺寸 (data=NULL → strbuf.size=所需) */
    char head[512];
    char* hw = head;
    *hw++ = '{';
    hw += snprintf(hw, (size_t)(head + sizeof(head) - hw),
                   "\"module_id\":\"%s\",\"op\":\"%s\",\"status\":0,"
                   "\"db_type\":%d,\"file_count\":%u,\"count\":%d,"
                   "\"leased_workers\":%u,\"leased\":%s,"
                   "\"schema\":",
                   kModuleId, c.op, gaia_client_get_db_type(cli),
                   (unsigned)file_count, count, (unsigned)leased,
                   lease_active ? "true" : "false");
    const char* schema = "{}";
    if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE))
        schema = "{\"ra\":\"f64\",\"dec\":\"f64\",\"magG\":\"f64\",\"magBP\":\"f64\","
                 "\"magRP\":\"f64\",\"parallax\":\"f32\",\"pmra\":\"f32\","
                 "\"pmdec\":\"f32\",\"source_id\":\"i64\"}";
    else if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_SPEC))
        schema = "{\"ra\":\"f64\",\"dec\":\"f64\",\"magG\":\"f64\","
                 "\"flux_min\":\"f32\",\"flux_mul\":\"f32\",\"spectra\":\"u8x343\"}";
    else if (!strcmp(c.op, ASTROCS_GAIA_OP_CONE_PHOT))
        schema = "{\"ra\":\"f64\",\"dec\":\"f64\",\"magG\":\"f64\","
                 "\"magBP\":\"f64\",\"magRP\":\"f64\"}";
    else if (!strcmp(c.op, ASTROCS_GAIA_OP_SPEC_BY_COORDS))
        schema = "{\"ra\":\"f64\",\"dec\":\"f64\",\"magG\":\"f64\","
                 "\"flux_min\":\"f32\",\"flux_mul\":\"f32\",\"spectra\":\"u8x343\","
                 "\"match_idx\":\"i32\"}";
    else /* solver */
        schema = "{\"ra\":\"f64\",\"dec\":\"f64\",\"mag\":\"f32\"}";
    size_t schema_len = strlen(schema);
    memcpy(hw, schema, schema_len);
    hw += schema_len;
    if (spec_bytes) {
        hw += snprintf(hw, (size_t)(head + sizeof(head) - hw),
                       ",\"spec_start_nm\":%d,\"spec_step_nm\":%d,\"spec_count\":%d",
                       spec_start, spec_step, spec_count);
    }
    hw += snprintf(hw, (size_t)(head + sizeof(head) - hw),
                   ",\"catalog_dir\":\"");
    json_append_escaped(&hw, c.catalog_dir);
    *hw++ = '"';
    const char* tail_keys[6];
    tail_keys[0] = ",\"data_base64\":\"";
    tail_keys[1] = "\",\"spectra_base64\":\"";
    tail_keys[2] = "\",\"match_idx_base64\":\"";
    tail_keys[3] = "\",\"solver_ra_base64\":\"";
    tail_keys[4] = "\",\"solver_dec_base64\":\"";
    tail_keys[5] = "\",\"solver_mag_base64\":\"";
    (void)tail_keys;

    uint64_t fixed_len = (uint64_t)(hw - head)
                       + b64_rows_cap - 1
                       + 18  /* ,"data_base64":"..." 由下式精确计: 先加键长 */
                       + 0;
    /* 精确两阶段: 直接把完整文本拼到动态缓冲会违背"调用方 buffer"; 因此
     * 第一遍用 data=NULL 取所需 size, 第二遍写入。尺寸 = 固定头 + 各段
     * (键 + base64)。这里先手工求总长。 */
    {
        const char* keys[] = {
            ",\"data_base64\":\"",          /* rows */
            ",\"spectra_base64\":\"",       /* spectra */
            ",\"match_idx_base64\":\"",     /* match_idx */
            ",\"solver_ra_base64\":\"",
            ",\"solver_dec_base64\":\"",
            ",\"solver_mag_base64\":\"" };
        uint64_t segs[6] = { row_bytes ? b64_rows_cap : 0,
                             spec_bytes ? b64_spec_cap : 0,
                             match_idx ? b64_idx_cap : 0,
                             solver_ra ? b64_sra_cap : 0,
                             solver_dec ? b64_sdec_cap : 0,
                             solver_mag ? b64_smag_cap : 0 };
        uint64_t total = (uint64_t)(hw - head);   /* 头部到此 (不含 "}") */
        for (int i = 0; i < 6; i++) {
            /* 键(含开引号) + base64 值 + 闭合引号; segs[i]=值长+NUL,
             * NUL 位恰好被闭合引号顶替 → 直接加 segs[i] */
            if (segs[i] > 0) total += (uint64_t)strlen(keys[i]) + segs[i];
        }
        total += 1; /* "}" */
        /* 两阶段契约: data=NULL/cap=0 → 只报所需 size (不 memcpy);
         * data!=NULL → 组装完整 JSON 后一次写入。head 是栈缓冲,
         * 长度仅 (hw-head) 字节, 严禁作为 total 长度源直接拷贝。 */
        if (!out_manifest_json) {
            efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
                  ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: null output buffer");
            free(row_ptr); free(spectra); free(match_idx);
            free(solver_ra); free(solver_dec); free(solver_mag);
            gaia_cfg_free(&c);
            inst->executing = 0;
            inst->exec_count++;
            inst->last_status = ACS_ERR_PARAM;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
            return ACS_ERR_PARAM;
        }
        if (out_manifest_json->cap == 0 || !out_manifest_json->data) {
            out_manifest_json->size = total;   /* 尺寸查询 */
            free(row_ptr); free(spectra); free(match_idx);
            free(solver_ra); free(solver_dec); free(solver_mag);
            gaia_cfg_free(&c);
            inst->executing = 0;
            inst->exec_count++;
            inst->last_status = ACS_OK;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
            inst->last_workers = leased;
            return ACS_OK;
        }
        (void)fixed_len;
        /* 第二遍: 若调用方给了缓冲, 组装完整 JSON 写入 */
        if (out_manifest_json->data && out_manifest_json->cap > 0) {
            char* big = (char*)malloc((size_t)total + 1);
            if (!big) {
                free(row_ptr); free(spectra); free(match_idx);
                free(solver_ra); free(solver_dec); free(solver_mag);
                gaia_cfg_free(&c);
                inst->executing = 0;
                inst->exec_count++;
                inst->last_status = ACS_ERR_NOMEM;
                efill(err, ACS_ERR_NOMEM, ACS_ERR_DOMAIN_RESOURCE,
                      ACS_DIAG_ECODE_NONE, "gaia: manifest alloc failed");
                return ACS_ERR_NOMEM;
            }
            memcpy(big, head, (size_t)(hw - head));
            char* w = big + (hw - head);
            if (row_bytes) {
                size_t kl = strlen(keys[0]);
                memcpy(w, keys[0], kl); w += kl;
                b64_encode((const uint8_t*)row_ptr, row_bytes, w);
                w += strlen(w);
                *w++ = '"';
            }
            if (spec_bytes) {
                size_t kl = strlen(keys[1]);
                memcpy(w, keys[1], kl); w += kl;
                b64_encode(spectra, spec_bytes, w);
                w += strlen(w);
                *w++ = '"';
            }
            if (match_idx) {
                size_t kl = strlen(keys[2]);
                memcpy(w, keys[2], kl); w += kl;
                b64_encode((const uint8_t*)match_idx, (uint64_t)count * sizeof(int), w);
                w += strlen(w);
                *w++ = '"';
            }
            if (solver_ra) {
                size_t kl = strlen(keys[3]);
                memcpy(w, keys[3], kl); w += kl;
                b64_encode((const uint8_t*)solver_ra, (uint64_t)count * sizeof(double), w);
                w += strlen(w);
                *w++ = '"';
            }
            if (solver_dec) {
                size_t kl = strlen(keys[4]);
                memcpy(w, keys[4], kl); w += kl;
                b64_encode((const uint8_t*)solver_dec, (uint64_t)count * sizeof(double), w);
                w += strlen(w);
                *w++ = '"';
            }
            if (solver_mag) {
                size_t kl = strlen(keys[5]);
                memcpy(w, keys[5], kl); w += kl;
                b64_encode((const uint8_t*)solver_mag, (uint64_t)count * sizeof(float), w);
                w += strlen(w);
                *w++ = '"';
            }
            *w++ = '}';
            *w = '\0';
            acs_status s2 = strbuf_write_cstr(out_manifest_json, big, err);
            free(big);
            free(row_ptr); free(spectra); free(match_idx);
            free(solver_ra); free(solver_dec); free(solver_mag);
            gaia_cfg_free(&c);
            inst->executing = 0;
            inst->exec_count++;
            inst->last_status = s2;
            snprintf(inst->last_op, sizeof(inst->last_op), "%s", c.op);
            inst->last_workers = leased;
            return s2;
        }
    }
}

/* ───────── inspect / request_cancel / destroy ───────── */

static acs_status gaia_inspect(const acs_module_instance_v1* inst_raw,
                               acs_strbuf_v1* out_json,
                               acs_error_info_v1* err) {
    const gaia_inst* inst = (const gaia_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (!out_json) {
        efill(err, ACS_ERR_PARAM, ACS_ERR_DOMAIN_CONFIG,
              ACS_DIAG_ECODE_NULL_CALLBACK, "gaia: null inspect buffer");
        return ACS_ERR_PARAM;
    }
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"module_id\":\"%s\",\"exec_count\":%llu,\"last_op\":\"%s\","
             "\"last_status\":%d,\"last_workers\":%u,"
             "\"catalog_dir\":\"",
             kModuleId, (unsigned long long)inst->exec_count,
             inst->last_op[0] ? inst->last_op : "", (int)inst->last_status,
             (unsigned)inst->last_workers);
    char* w = buf + strlen(buf);
    json_append_escaped(&w, inst->cfg.catalog_dir);
    snprintf(w, (size_t)(buf + sizeof(buf) - w),
             "\",cancel_req\":%d,\"state\":%u}",
             inst->cancel_req, (unsigned)inst->state);
    return strbuf_write_cstr(out_json, buf, err);
}

static acs_status gaia_request_cancel(acs_module_instance_v1* inst_raw) {
    gaia_inst* inst = (gaia_inst*)inst_raw;
    if (!inst) return ACS_ERR_PARAM;
    if (inst->state == ACS_LC_STATE_DESTROYED) return ACS_ERR_STATE;
    inst->cancel_req = 1;   /* 幂等单向置位 (ABI-002) */
    return ACS_OK;
}

static void gaia_destroy(acs_module_instance_v1* inst_raw) {
    gaia_inst* inst = (gaia_inst*)inst_raw;
    if (!inst) return;
    gaia_cfg_free(&inst->cfg);
    if (inst->client) gaia_client_destroy(inst->client);
    inst->client = NULL;
    inst->state = ACS_LC_STATE_DESTROYED;
    free(inst);
}

/* ───────── 静态 vtable 与唯一导出入口 ───────── */

static const acs_module_api_v1 g_gaia_api = {
    { (uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1 },
    gaia_describe,
    gaia_validate_config,
    gaia_plan,
    gaia_create,
    gaia_execute,
    gaia_inspect,
    gaia_request_cancel,
    gaia_destroy
};

/* 唯一导出入口 (12 §1; ABI-006 全查 exports):
 * host_abi 失配 → ACS_ERR_ABI_MISMATCH, 不降级猜测。 */
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi,
                        const acs_host_api_v1* host,
                        const acs_module_api_v1** out_api) {
    (void)host;   /* allocator 必填在 create 期校验 (query 期 host 可 NULL 于探针) */
    if (host_abi != ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
    if (!out_api) return ACS_ERR_PARAM;
    *out_api = &g_gaia_api;
    return ACS_OK;
}
