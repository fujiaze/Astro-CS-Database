/* CAT-GAIA-IMPL 模块适配器测试 (tests/unit/gaia_adapter_test.c)
 *
 * 覆盖 (迁移模板 <prefix>-IMPL 验收):
 *   A. C ABI 入口契约: host_abi 失配 → ACS_ERR_ABI_MISMATCH; out_api=NULL →
 *      ACS_ERR_PARAM; 正常 query → 唯一导出面 astrocs_module_query_v1。
 *   B. 导出面净化: dlsym(legacy 符号) 必须失败 (GAIA_EXPORT= 本地化, ABI-006)。
 *   C. 生命周期: describe / validate_config(词表+有限值+detail_code) /
 *      plan(真实 work_units 元数据推导) / create / execute / inspect /
 *      request_cancel(幂等) / destroy。
 *   D. direct-vs-plugin bitwise (GAIA_QUERY.md §5 冻结等价): 同 fixture
 *      (${GAIA_CAT_CLEAN}, seed 固定) 上 legacy 直接调用 vs DLL 经 strbuf
 *      manifest (base64) 输出逐位一致 —— cone / solver / spectrum / photometry
 *      / by_coords 五通道; 光谱参数 (start/step/count) 一致。
 *   E. 租借/预算/取消: executor lease acquire/release 计数与 leased_workers;
 *      acquire 拒绝 → ACS_ERR_BUDGET; host cancel 置位 → 文件边界取消
 *      ACS_ERR_CANCELLED 且无输出外泄。
 *
 * 编译: #include "gaia_client.c" (共址直接路径, 不定义 GAIA_ALLOC_TEST —
 * 不需要分配钩子; 与 gaia_cat_test 同型)。DLL 经环境变量
 * ASTROCS_GAIA_DLL_PATH 加载 (CMake test properties 注入)。
 */
#include "gaia_client.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/abi/host_api_v1.h"
#include "astrocs/gaia/types.h"

/* 入口函数指针 (头内仅声明, 无 typedef; dlsym 需要) */
typedef acs_status (*gaia_entry_fn)(uint32_t, const acs_host_api_v1*,
                                    const acs_module_api_v1**);

/* fixture 参考真值 (构建期生成; 与 gaia_cat_test 同一 include 路径) */
#include "gaia_cat_manifest.h"

static int g_fail = 0;
#define CHECK(cond, name) do { \
    if (cond) printf("[PASS] %s\n", (name)); \
    else { printf("[FAIL] %s (line %d)\n", (name), __LINE__); g_fail++; } \
} while (0)

/* ── 测试侧 mini JSON 读取 (与 adapter 同风格; 只读) ── */
static int tjson_num(const char* j, const char* key, double* out) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '-' && (*p < '0' || *p > '9')) return 0;
    *out = strtod(p, NULL);
    return 1;
}
static int tjson_str(const char* j, const char* key, char* buf, size_t cap) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap) buf[i++] = *p++;
    buf[i] = '\0';
    return *p == '"';
}
static int tjson_bool(const char* j, const char* key) {
    char pat[64]; snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(j, pat);
    if (!p) return -1;
    p += strlen(pat);
    while (*p == ' ' || *p == ':') p++;
    if (!strncmp(p, "true", 4)) return 1;
    if (!strncmp(p, "false", 5)) return 0;
    return -1;
}

/* ── base64 解码 (RFC 4648; 返回 malloc 字节; *out_n=字节数) ── */
static int b64_val(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static uint8_t* b64_decode(const char* s, uint64_t* out_n) {
    uint64_t len = strlen(s);
    while (len && s[len-1] == '=') len--;
    uint64_t full = (len / 4) * 4, rem = len - full;
    uint64_t n = (len / 4) * 3 + (rem ? rem - 1 : 0);
    uint8_t* out = (uint8_t*)malloc((size_t)(n ? n : 1));
    if (!out) return NULL;
    uint64_t o = 0, i = 0;
    while (i + 3 < full) {
        int a = b64_val(s[i]), b = b64_val(s[i+1]), c = b64_val(s[i+2]), d = b64_val(s[i+3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) { free(out); return NULL; }
        uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12) | ((uint32_t)c << 6) | (uint32_t)d;
        out[o++] = (uint8_t)(v >> 16);
        out[o++] = (uint8_t)(v >> 8);
        out[o++] = (uint8_t)v;
        i += 4;
    }
    if (rem >= 2) {
        int a = b64_val(s[i]), b = b64_val(s[i+1]);
        if (a < 0 || b < 0) { free(out); return NULL; }
        uint32_t v = ((uint32_t)a << 18) | ((uint32_t)b << 12);
        out[o++] = (uint8_t)(v >> 16);
        if (rem == 3) {
            int c = b64_val(s[i+2]);
            if (c < 0) { free(out); return NULL; }
            v |= (uint32_t)c << 6;
            out[o++] = (uint8_t)(v >> 8);
        }
    }
    *out_n = n;
    return out;
}

/* ── strbuf helper: 尺寸查询→分配→二遍 ── */
static char* exec_collect(acs_status (*fn)(acs_module_instance_v1*, acs_str_v1,
                                            acs_str_v1, acs_strbuf_v1*,
                                            acs_error_info_v1*),
                          acs_module_instance_v1* inst,
                          const char* cfg, acs_status* rc) {
    acs_str_v1 cfgs = { { (uint32_t)sizeof(acs_str_v1), ACS_ABI_VERSION_V1 }, cfg, strlen(cfg) };
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err.head.abi_version = ACS_ABI_VERSION_V1;
    acs_strbuf_v1 sb;
    memset(&sb, 0, sizeof(sb));
    sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
    sb.head.abi_version = ACS_ABI_VERSION_V1;
    *rc = fn(inst, cfgs, cfgs, &sb, &err);        /* 一遍: 尺寸 */
    if (*rc != ACS_OK) return NULL;
    sb.data = (char*)malloc((size_t)sb.size + 1);
    sb.cap = sb.size + 1;
    if (!sb.data) { *rc = ACS_ERR_NOMEM; return NULL; }
    acs_status st2 = fn(inst, cfgs, cfgs, &sb, &err);  /* 二遍: 写入 */
    if (st2 != ACS_OK) { free(sb.data); *rc = st2; return NULL; }
    return sb.data;
}

/* ── host 侧桩 ── */
typedef struct { int acquire_calls, release_calls, acquire_fail; uint32_t max_workers; } ex_stub;
static int ex_acquire(void* ud, uint32_t n) {
    ex_stub* s = (ex_stub*)ud;
    s->acquire_calls++;
    return s->acquire_fail ? -1 : 0;
    (void)n;
}
static void ex_release(void* ud, uint32_t n) { ((ex_stub*)ud)->release_calls++; (void)n; }
static int cancel_always(void* ud) { (void)ud; return 1; }

static const acs_host_api_v1* make_host(ex_stub* ex, acs_cancel_v1* cn, void* cn_ud) {
    /* 返回静态存储 host (测试进程内安全) */
    static acs_host_api_v1 h;
    static acs_allocator_v1 al;      /* allocator 不被 gaia adapter 使用 (legacy 自管理) */
    static acs_executor_v1 exv;
    static acs_cancel_v1 cnv;
    memset(&h, 0, sizeof(h));
    memset(&al, 0, sizeof(al));
    memset(&exv, 0, sizeof(exv));
    memset(&cnv, 0, sizeof(cnv));
    h.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    h.head.abi_version = ACS_ABI_VERSION_V1;
    al.head.struct_size = (uint32_t)sizeof(acs_allocator_v1);
    al.head.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = &al;
    if (ex) {
        exv.head.struct_size = (uint32_t)sizeof(acs_executor_v1);
        exv.head.abi_version = ACS_ABI_VERSION_V1;
        exv.user_data = ex;
        exv.max_workers = ex->max_workers;
        exv.acquire = ex_acquire;
        exv.release = ex_release;
        h.executor = &exv;
    }
    if (cn) {
        cnv.head.struct_size = (uint32_t)sizeof(acs_cancel_v1);
        cnv.head.abi_version = ACS_ABI_VERSION_V1;
        cnv.user_data = cn_ud;
        cnv.is_cancelled = cancel_always;
        h.cancel = &cnv;
    }
    return &h;
}

/* ── manifest 辅助: 找一颗 fixture 星 (含可选光谱) ── */
static const GaiaCatRefStar* find_star(int has_spec, int nth) {
    int seen = 0;
    for (int i = 0; i < G_MANIFEST_N; i++) {
        if ((G_MANIFEST[i].has_spec != 0) == (has_spec != 0)) {
            if (seen == nth) return &G_MANIFEST[i];
            seen++;
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: gaia_adapter_test <catalog_dir>\n");
        return 2;
    }
    const char* dir = argv[1];
    printf("== gaia_adapter_test: catalog_dir=%s ==\n", dir);

    /* ── 加载 DLL ── */
    const char* dll = getenv("ASTROCS_GAIA_DLL_PATH");
    if (!dll || !dll[0]) { printf("[FAIL] ASTROCS_GAIA_DLL_PATH not set\n"); return 2; }
#ifdef _WIN32
    HMODULE so = LoadLibraryA(dll);
    void* sym_entry = so ? (void*)GetProcAddress(so, "astrocs_module_query_v1") : NULL;
    void* sym_legacy = so ? (void*)GetProcAddress(so, "gaia_client_create") : NULL;
#else
    void* so = dlopen(dll, RTLD_NOW);
    if (!so) printf("[FAIL] dlopen: %s\n", dlerror());
    void* sym_entry = so ? dlsym(so, "astrocs_module_query_v1") : NULL;
    void* sym_legacy = so ? dlsym(so, "gaia_client_create") : NULL;
#endif
    CHECK(so != NULL, "dlopen astrocs_catalog_gaia");
    if (!so || !sym_entry) return 2;
    gaia_entry_fn entry = (gaia_entry_fn)sym_entry;

    /* A. 入口契约 */
    const acs_module_api_v1* api = NULL;
    CHECK(entry(999u, NULL, &api) == ACS_ERR_ABI_MISMATCH, "entry: host_abi mismatch -> ABI_MISMATCH");
    CHECK(entry(ACS_ABI_VERSION_V1, NULL, NULL) == ACS_ERR_PARAM, "entry: null out_api -> PARAM");
    CHECK(entry(ACS_ABI_VERSION_V1, NULL, &api) == ACS_OK && api != NULL,
          "entry: ok query (host 可 NULL 于探针)");
    /* B. 导出面净化: legacy 符号不得从 DLL 泄出 */
    CHECK(sym_legacy == NULL, "exports: legacy gaia_client_create NOT exported");

    /* C. describe */
    acs_module_descriptor_v1 desc;
    memset(&desc, 0, sizeof(desc));
    acs_str_v1 mid = { { (uint32_t)sizeof(acs_str_v1), ACS_ABI_VERSION_V1 },
                       ASTROCS_GAIA_MODULE_ID, strlen(ASTROCS_GAIA_MODULE_ID) };
    CHECK(api->describe(api, mid, &desc) == ACS_OK, "describe: ok");
    CHECK(desc.head.struct_size == sizeof(acs_module_descriptor_v1) &&
          desc.head.abi_version == ACS_ABI_VERSION_V1, "describe: acs_head filled");
    CHECK(desc.module_id.size == strlen(ASTROCS_GAIA_MODULE_ID) &&
          !memcmp(desc.module_id.data, ASTROCS_GAIA_MODULE_ID, desc.module_id.size),
          "describe: module_id == astrocs.catalog.gaia");
    CHECK(desc.sci_id.size == strlen(ASTROCS_GAIA_SCI_ID) &&
          !memcmp(desc.sci_id.data, ASTROCS_GAIA_SCI_ID, desc.sci_id.size),
          "describe: sci_id == SCI-AST-001");
    CHECK(desc.alg_id.size == strlen(ASTROCS_GAIA_ALG_ID) &&
          !memcmp(desc.alg_id.data, ASTROCS_GAIA_ALG_ID, desc.alg_id.size),
          "describe: alg_id == ALG-GAIA-001");
    CHECK(desc.api_id.size == strlen(ASTROCS_GAIA_API_ID) &&
          !memcmp(desc.api_id.data, ASTROCS_GAIA_API_ID, desc.api_id.size),
          "describe: api_id == API-GAIA-001");

    /* C. validate_config 负例 (detail_code 冻结) */
    acs_error_info_v1 err;
    memset(&err, 0, sizeof(err));
    err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err.head.abi_version = ACS_ABI_VERSION_V1;
    acs_str_v1 sj = { { (uint32_t)sizeof(acs_str_v1), ACS_ABI_VERSION_V1 }, NULL, 0 };
    const char* j;

    j = NULL;
    sj.data = j; sj.size = 0;
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM, "validate: null config -> PARAM");
    j = "{\"catalog_dir\":\"x\"}";
    sj.data = j; sj.size = strlen(j);
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM &&
          err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA, "validate: missing op -> PARAM/schema");
    j = "{\"op\":\"nope\",\"catalog_dir\":\"x\"}";
    sj.data = j; sj.size = strlen(j);
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM &&
          err.detail_code == ACS_GAIA_ECODE_OP_UNKNOWN, "validate: unknown op -> detail 100");
    j = "{\"op\":\"cone_search\"}";
    sj.data = j; sj.size = strlen(j);
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM &&
          err.detail_code == ACS_GAIA_ECODE_CATALOG_DIR_MISSING, "validate: missing catalog_dir -> detail 101");
    j = "{\"op\":\"cone_search\",\"catalog_dir\":\"x\",\"ra\":1.0,\"dec\":2.0}";
    sj.data = j; sj.size = strlen(j);
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM &&
          err.detail_code == ACS_GAIA_ECODE_PARAM_MISSING, "validate: missing params -> detail 102");
    j = "{\"op\":\"cone_search\",\"catalog_dir\":\"x\",\"ra\":1e999,\"dec\":2.0,"
        "\"radius_deg\":1.0,\"mag_low\":0.0,\"mag_high\":0.0}";
    sj.data = j; sj.size = strlen(j);
    CHECK(api->validate_config(api, sj, &err) == ACS_ERR_PARAM &&
          err.detail_code == ACS_GAIA_ECODE_PARAM_NOT_FINITE, "validate: non-finite ra -> detail 103");

    /* C. plan: 真实 work_units (fixture: 2 xpsd 文件) */
    char plan_cfg[512];
    snprintf(plan_cfg, sizeof(plan_cfg),
             "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
             "\"ra\":200.0,\"dec\":0.0,\"radius_deg\":2.0,"
             "\"mag_low\":-100.0,\"mag_high\":100.0}", dir);
    sj.data = plan_cfg; sj.size = strlen(plan_cfg);
    acs_str_v1 nid = { { (uint32_t)sizeof(acs_str_v1), ACS_ABI_VERSION_V1 }, "n1", 2 };
    acs_strbuf_v1 pb;
    memset(&pb, 0, sizeof(pb));
    pb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
    pb.head.abi_version = ACS_ABI_VERSION_V1;
    memset(&err, 0, sizeof(err));
    err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err.head.abi_version = ACS_ABI_VERSION_V1;
    acs_status pst = api->plan(api, nid, sj, &pb, &err);   /* 尺寸遍 */
    char* plan_json = NULL;
    if (pst == ACS_OK) {
        plan_json = (char*)malloc((size_t)pb.size + 1);
        pb.data = plan_json; pb.cap = pb.size + 1;
        pst = api->plan(api, nid, sj, &pb, &err);
    }
    CHECK(pst == ACS_OK && plan_json != NULL, "plan: ok (两阶段 strbuf)");
    double files = 0, wu = 0, mn = 0, mx = 0;
    char axis[32] = {0};
    if (plan_json) {
        CHECK(tjson_num(plan_json, "file_count", &files) && files == 2.0,
              "plan: file_count == 2 (fixture gaia_sp+gaia_dr3)");
        CHECK(tjson_num(plan_json, "work_units", &wu) && wu > 0,
              "plan: work_units > 0 (真实元数据推导)");
        CHECK(tjson_num(plan_json, "min_workers", &mn) && mn == 1.0,
              "plan: min_workers == 1");
        CHECK(tjson_num(plan_json, "max_workers", &mx) && mx == files,
              "plan: max_workers == file_count (并行轴=文件)");
        CHECK(tjson_str(plan_json, "parallel_axis", axis, sizeof(axis)) &&
              !strcmp(axis, "file"), "plan: parallel_axis == file");
    } else {
        while (g_fail++ < 4) printf("[FAIL] plan json missing\n");
    }
    free(plan_json);

    j = "{\"op\":\"cone_search\",\"catalog_dir\":\"/nonexistent_gaia_dir_zz\","
        "\"ra\":200.0,\"dec\":0.0,\"radius_deg\":2.0,"
        "\"mag_low\":-100.0,\"mag_high\":100.0}";
    sj.data = j; sj.size = strlen(j);
    memset(&err, 0, sizeof(err));
    err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    err.head.abi_version = ACS_ABI_VERSION_V1;
    acs_strbuf_v1 pbad;
    memset(&pbad, 0, sizeof(pbad));
    pbad.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
    pbad.head.abi_version = ACS_ABI_VERSION_V1;
    CHECK(api->plan(api, nid, sj, &pbad, &err) == ACS_ERR_IO, "plan: bad catalog_dir -> IO");

    /* ── direct client (legacy 路径, 同进程共址) ── */
    GaiaClient* direct = gaia_client_create_ex(dir, GAIA_DB_AUTO);
    CHECK(direct != NULL, "direct: gaia_client_create_ex");
    if (!direct) return 2;
    int spec_start = 0, spec_step = 0, spec_count = 0;
    gaia_client_get_spectrum_params(direct, &spec_start, &spec_step, &spec_count);
    CHECK(spec_count == GAIA_CAT_WL, "direct: spec_count == 343");

    const acs_host_api_v1* plain_host = make_host(NULL, NULL, NULL);

    /* D1. cone_search bitwise */
    {
        const GaiaCatRefStar* ref = find_star(0, 0);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":3.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        GaiaStar* ds = NULL; int dn = 0;
        int drc = gaia_client_cone_search(direct, ref->ra, ref->dec, 3.0,
                                          -100.0, 100.0, &ds, &dn);
        CHECK(drc == 0 && dn > 0 && ds != NULL, "cone: direct ok");
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        CHECK(create_ok, "cone: create ok");
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(rc2 == ACS_OK && out != NULL, "cone: execute ok");
        if (out) {
            double cnt = 0; int ok = tjson_num(out, "count", &cnt);
            CHECK(ok && (int)cnt == dn, "cone: count match (direct==plugin)");
            uint64_t bn = 0;
            char* b64 = NULL;
            /* 提取 data_base64 值 */
            const char* key = "\"data_base64\":\"";
            const char* p = out ? strstr(out, key) : NULL;
            if (p) {
                p += strlen(key);
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
            }
            uint8_t* rows = b64 ? b64_decode(b64, &bn) : NULL;
            CHECK(rows != NULL && bn == (uint64_t)dn * sizeof(GaiaStar),
                  "cone: manifest row bytes == direct");
            if (rows && bn == (uint64_t)dn * sizeof(GaiaStar))
                CHECK(memcmp(rows, ds, (size_t)bn) == 0,
                      "cone: BITWISE rows direct==plugin");
            /* fixture 星存在 (sanity) */
            int found = 0;
            for (int i = 0; i < dn; i++)
                if (ds[i].ra == ref->ra && ds[i].dec == ref->dec) found = 1;
            CHECK(found, "cone: fixture star present");
            free(b64); free(rows); free(out);
        }
        free(ds);
        if (inst) api->destroy(inst);
    }

    /* D2. cone_search_for_solver bitwise (3 数组) */
    {
        const GaiaCatRefStar* ref = find_star(0, 1);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search_for_solver\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":3.0,"
                 "\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        double *dra = NULL, *ddec = NULL; float* dmag = NULL; int dn = 0;
        int drc = gaia_client_cone_search_for_solver(direct, ref->ra, ref->dec, 3.0,
                                                     100.0, &dra, &ddec, &dmag, &dn);
        CHECK(drc == 0 && dn > 0, "solver: direct ok");
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(create_ok && rc2 == ACS_OK && out != NULL, "solver: create+execute ok");
        if (out) {
            double cnt = 0;
            CHECK(tjson_num(out, "count", &cnt) && (int)cnt == dn, "solver: count match");
            const char* keys[3] = { "\"solver_ra_base64\":\"", "\"solver_dec_base64\":\"",
                                    "\"solver_mag_base64\":\"" };
            void* want[3] = { dra, ddec, dmag };
            uint64_t wantb[3] = { (uint64_t)dn * sizeof(double),
                                  (uint64_t)dn * sizeof(double),
                                  (uint64_t)dn * sizeof(float) };
            const char* names[3] = { "ra", "dec", "mag" };
            for (int k = 0; k < 3; k++) {
                const char* p = strstr(out, keys[k]);
                if (!p) { printf("[FAIL] solver: missing %s b64\n", names[k]); g_fail++; continue; }
                p += strlen(keys[k]);
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                char* b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
                uint64_t bn = 0;
                uint8_t* got = b64_decode(b64, &bn);
                char nm[64];
                snprintf(nm, sizeof(nm), "solver: BITWISE %s array", names[k]);
                CHECK(got && bn == wantb[k] && memcmp(got, want[k], (size_t)bn) == 0, nm);
                free(b64); free(got);
            }
            free(out);
        }
        free(dra); free(ddec); free(dmag);
        if (inst) api->destroy(inst);
    }

    /* D3. cone_search_with_spectrum bitwise (rows + spectra + params) */
    {
        const GaiaCatRefStar* ref = find_star(1, 0);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search_with_spectrum\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":3.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        GaiaSpectrumStar* ds = NULL; uint8_t* dsp = NULL; int dn = 0;
        int drc = gaia_client_cone_search_with_spectrum(direct, ref->ra, ref->dec, 3.0,
                                                        -100.0, 100.0, &ds, &dsp, &dn);
        CHECK(drc == 0 && dn > 0 && ds && dsp, "spectrum: direct ok");
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(create_ok && rc2 == ACS_OK && out != NULL, "spectrum: create+execute ok");
        if (out) {
            double cnt = 0, sc = 0, sst = 0, sstep = 0;
            CHECK(tjson_num(out, "count", &cnt) && (int)cnt == dn, "spectrum: count match");
            CHECK(tjson_num(out, "spec_count", &sc) && (int)sc == spec_count,
                  "spectrum: spec_count match");
            CHECK(tjson_num(out, "spec_start_nm", &sst) && (int)sst == spec_start &&
                  tjson_num(out, "spec_step_nm", &sstep) && (int)sstep == spec_step,
                  "spectrum: params match (start/step)");
            const char* krow = "\"data_base64\":\"";
            const char* kspec = "\"spectra_base64\":\"";
            uint64_t bn = 0, sn = 0;
            uint8_t *rows = NULL, *spec = NULL;
            const char* p = strstr(out, krow);
            if (p) {
                p += strlen(krow);
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                char* b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
                rows = b64_decode(b64, &bn);
                free(b64);
            }
            p = strstr(out, kspec);
            if (p) {
                p += strlen(kspec);
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                char* b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
                spec = b64_decode(b64, &sn);
                free(b64);
            }
            CHECK(rows && bn == (uint64_t)dn * sizeof(GaiaSpectrumStar) &&
                  memcmp(rows, ds, (size_t)bn) == 0, "spectrum: BITWISE rows");
            uint64_t want_sn = (uint64_t)dn * (uint64_t)spec_count;
            CHECK(spec && sn == want_sn && memcmp(spec, dsp, (size_t)sn) == 0,
                  "spectrum: BITWISE spectra bytes");
            free(rows); free(spec); free(out);
        }
        free(ds); free(dsp);
        if (inst) api->destroy(inst);
    }

    /* D4. cone_search_with_photometry bitwise */
    {
        const GaiaCatRefStar* ref = find_star(0, 2);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search_with_photometry\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":3.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        GaiaPhotometryStar* ds = NULL; int dn = 0;
        int drc = gaia_client_cone_search_with_photometry(direct, ref->ra, ref->dec, 3.0,
                                                          -100.0, 100.0, &ds, &dn);
        CHECK(drc == 0 && dn > 0 && ds, "photometry: direct ok");
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(create_ok && rc2 == ACS_OK && out != NULL, "photometry: create+execute ok");
        if (out) {
            double cnt = 0;
            CHECK(tjson_num(out, "count", &cnt) && (int)cnt == dn, "photometry: count match");
            const char* p = strstr(out, "\"data_base64\":\"");
            uint64_t bn = 0;
            uint8_t* rows = NULL;
            if (p) {
                p += strlen("\"data_base64\":\"");
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                char* b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
                rows = b64_decode(b64, &bn);
                free(b64);
            }
            CHECK(rows && bn == (uint64_t)dn * sizeof(GaiaPhotometryStar) &&
                  memcmp(rows, ds, (size_t)bn) == 0, "photometry: BITWISE rows");
            free(rows); free(out);
        }
        free(ds);
        if (inst) api->destroy(inst);
    }

    /* D5. query_spectrum_by_coords bitwise (rows + spectra + match_idx) */
    {
        enum { NC = 8 };
        const GaiaCatRefStar* refs[NC];
        char ral[256] = {0}, del[256] = {0};
        for (int i = 0; i < NC; i++) {
            refs[i] = find_star(1, i);
            char a[40], b[40];
            snprintf(a, sizeof(a), "%s%.17g", i ? "," : "", refs[i]->ra);
            snprintf(b, sizeof(b), "%s%.17g", i ? "," : "", refs[i]->dec);
            strcat(ral, a);
            strcat(del, b);
        }
        char cfg[768];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"query_spectrum_by_coords\",\"catalog_dir\":\"%s\","
                 "\"ra_list\":[%s],\"dec_list\":[%s],"
                 "\"match_radius_arcsec\":60.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ral, del);
        GaiaSpectrumStar* ds = NULL; uint8_t* dsp = NULL; int* didx = NULL; int dn = 0;
        int drc = gaia_client_query_spectrum_by_coords(direct,
                                                       (const double*)0, (const double*)0, 0,
                                                       60.0, -100.0, 100.0,
                                                       &ds, &dsp, &didx, &dn);
        (void)drc; /* 直接调用用真实数组重发 */
        double ra_arr[NC], de_arr[NC];
        for (int i = 0; i < NC; i++) { ra_arr[i] = refs[i]->ra; de_arr[i] = refs[i]->dec; }
        drc = gaia_client_query_spectrum_by_coords(direct, ra_arr, de_arr, NC,
                                                   60.0, -100.0, 100.0,
                                                   &ds, &dsp, &didx, &dn);
        CHECK(drc == 0 && dn == NC, "by_coords: direct ok");
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(create_ok && rc2 == ACS_OK && out != NULL, "by_coords: create+execute ok");
        if (out) {
            double cnt = 0;
            CHECK(tjson_num(out, "count", &cnt) && (int)cnt == dn, "by_coords: count match");
            const char* keys[3] = { "\"data_base64\":\"", "\"spectra_base64\":\"",
                                    "\"match_idx_base64\":\"" };
            void* want[3] = { ds, dsp, didx };
            uint64_t wantb[3] = { (uint64_t)dn * sizeof(GaiaSpectrumStar),
                                  (uint64_t)dn * (uint64_t)spec_count,
                                  (uint64_t)dn * sizeof(int) };
            const char* names[3] = { "rows", "spectra", "match_idx" };
            for (int k = 0; k < 3; k++) {
                const char* p = strstr(out, keys[k]);
                if (!p) { printf("[FAIL] by_coords: missing %s\n", names[k]); g_fail++; continue; }
                p += strlen(keys[k]);
                const char* e = strchr(p, '"');
                uint64_t L = e ? (uint64_t)(e - p) : 0;
                char* b64 = (char*)malloc((size_t)L + 1);
                memcpy(b64, p, (size_t)L); b64[L] = '\0';
                uint64_t bn = 0;
                uint8_t* got = b64_decode(b64, &bn);
                char nm[64];
                snprintf(nm, sizeof(nm), "by_coords: BITWISE %s", names[k]);
                CHECK(got && bn == wantb[k] && memcmp(got, want[k], (size_t)bn) == 0, nm);
                free(b64); free(got);
            }
            free(out);
        }
        free(ds); free(dsp); free(didx);
        if (inst) api->destroy(inst);
    }

    /* E1. executor 租借: acquire/release 计数 + leased_workers/leased */
    {
        ex_stub ex;
        memset(&ex, 0, sizeof(ex));
        ex.max_workers = 2;
        const acs_host_api_v1* h = make_host(&ex, NULL, NULL);
        const GaiaCatRefStar* ref = find_star(0, 3);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":2.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, h, &inst, &err) == ACS_OK);
        acs_status rc2 = ACS_ERR_INTERNAL;
        char* out = create_ok ? exec_collect(api->execute, inst, cfg, &rc2) : NULL;
        CHECK(create_ok && rc2 == ACS_OK && out != NULL, "lease: execute ok");
        CHECK(ex.acquire_calls == 2 && ex.release_calls == 2,
              "lease: acquire/release balanced (两阶段各租借一次)");
        if (out) {
            CHECK(tjson_bool(out, "leased") == 1, "lease: leased==true");
            double lw = 0;
            CHECK(tjson_num(out, "leased_workers", &lw) && lw == 2.0,
                  "lease: leased_workers==2 (executor max)");
            free(out);
        }
        if (inst) api->destroy(inst);
    }

    /* E2. 预算耗尽: acquire 拒绝 → ACS_ERR_BUDGET */
    {
        ex_stub ex;
        memset(&ex, 0, sizeof(ex));
        ex.max_workers = 2;
        ex.acquire_fail = 1;
        const acs_host_api_v1* h = make_host(&ex, NULL, NULL);
        const GaiaCatRefStar* ref = find_star(0, 4);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":2.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, h, &inst, &err) == ACS_OK);
        acs_strbuf_v1 sb;
        memset(&sb, 0, sizeof(sb));
        sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
        sb.head.abi_version = ACS_ABI_VERSION_V1;
        acs_status rc2 = create_ok ? api->execute(inst, sj, sj, &sb, &err) : ACS_ERR_INTERNAL;
        CHECK(rc2 == ACS_ERR_BUDGET, "budget: acquire fail -> ACS_ERR_BUDGET");
        CHECK(ex.release_calls == 0, "budget: no release on failed acquire");
        if (inst) api->destroy(inst);
    }

    /* E3. 取消: host cancel 置位 → ACS_ERR_CANCELLED, 无输出外泄 */
    {
        int cn_state = 0;
        (void)cn_state;
        const acs_host_api_v1* h = make_host(NULL, (acs_cancel_v1*)1, NULL);
        const GaiaCatRefStar* ref = find_star(0, 5);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":2.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, h, &inst, &err) == ACS_OK);
        acs_strbuf_v1 sb;
        memset(&sb, 0, sizeof(sb));
        sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
        sb.head.abi_version = ACS_ABI_VERSION_V1;
        acs_status rc2 = create_ok ? api->execute(inst, sj, sj, &sb, &err) : ACS_ERR_INTERNAL;
        CHECK(rc2 == ACS_ERR_CANCELLED, "cancel: host flag -> ACS_ERR_CANCELLED");
        CHECK(sb.data == NULL, "cancel: no output buffer written (事务性)");
        /* request_cancel 幂等 */
        CHECK(api->request_cancel(inst) == ACS_OK &&
              api->request_cancel(inst) == ACS_OK, "cancel: request_cancel idempotent");
        /* 取消后状态恢复: 无 host cancel 再执行成功 */
        const acs_host_api_v1* h2 = make_host(NULL, NULL, NULL);
        sj.data = cfg; sj.size = strlen(cfg);
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        /* 复用同实例: 需重新 create (cancel 后状态机仍 CREATED) — 直接再 execute */
        acs_status rc3 = api->execute(inst, sj, sj, &sb, &err);
        CHECK(rc3 == ACS_OK, "cancel: instance reusable after cancel");
        if (inst) api->destroy(inst);
    }

    /* C. inspect: exec_count 与 module_id */
    {
        const GaiaCatRefStar* ref = find_star(0, 6);
        char cfg[512];
        snprintf(cfg, sizeof(cfg),
                 "{\"op\":\"cone_search\",\"catalog_dir\":\"%s\","
                 "\"ra\":%.17g,\"dec\":%.17g,\"radius_deg\":1.0,"
                 "\"mag_low\":-100.0,\"mag_high\":100.0}", dir, ref->ra, ref->dec);
        acs_module_instance_v1* inst = NULL;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        sj.data = cfg; sj.size = strlen(cfg);
        int create_ok = (api->create(api, sj, plain_host, &inst, &err) == ACS_OK);
        acs_strbuf_v1 sb;
        memset(&sb, 0, sizeof(sb));
        sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
        sb.head.abi_version = ACS_ABI_VERSION_V1;
        acs_status rc2 = create_ok ? api->execute(inst, sj, sj, &sb, &err) : ACS_ERR_INTERNAL;
        (void)rc2;
        acs_strbuf_v1 ib;
        memset(&ib, 0, sizeof(ib));
        ib.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
        ib.head.abi_version = ACS_ABI_VERSION_V1;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        acs_status ist = api->inspect(inst, &ib, &err);
        char* insp = NULL;
        if (ist == ACS_OK) {
            insp = (char*)malloc((size_t)ib.size + 1);
            ib.data = insp; ib.cap = ib.size + 1;
            ist = api->inspect(inst, &ib, &err);
        }
        CHECK(ist == ACS_OK && insp != NULL, "inspect: ok");
        if (insp) {
            CHECK(strstr(insp, ASTROCS_GAIA_MODULE_ID) != NULL, "inspect: module_id present");
            double ec = 0;
            CHECK(tjson_num(insp, "exec_count", &ec) && ec >= 1.0, "inspect: exec_count>=1");
            free(insp);
        }
        if (inst) api->destroy(inst);
    }

    gaia_client_destroy(direct);
#ifdef _WIN32
    FreeLibrary(so);
#else
    dlclose(so);
#endif

    printf("== gaia_adapter_test: %s (fail=%d) ==\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail ? 1 : 0;
}
