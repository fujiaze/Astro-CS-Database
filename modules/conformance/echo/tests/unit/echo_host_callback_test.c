/* AstroCS echo conformance module — host callback 正/负测试 (ABI-005)
 *
 * 覆盖 (ABI-005 验收: 每个 host callback 有正/负测试):
 *   - query/describe/validate_config/plan/create/destroy 生命周期正/负;
 *   - host allocator: create 经 host allocator 分配; artifact_read 的
 *     read_all/缓冲经同一 allocator; destroy 归还 —— alloc/free 计数平衡;
 *   - artifact read: host.artifacts 全服务正测 (open/query/read_all/
 *     manifest_parse/get_str/get_u64/close); artifacts==NULL 负测;
 *     open 失败负测; storage_uri 缺失负测;
 *   - artifact write: ABI v1 无写回调 → write_blocked 恒 ACS_ERR_UNSUPPORTED
 *     (detail 105), 不绕过 host 自开文件 (FORBID-002) —— 并驱动 host logger
 *     (WARN) 正测;
 *   - cancel: host cancel 未置位 echo 正测; host cancel 置位 → CANCELLED;
 *     request_cancel 幂等置位 → 后续 execute CANCELLED; host cancel==NULL
 *     + cancel_poll 负测 (detail 104);
 *   - executor lease: acquire/release 正测 (workers 归还); budget denied →
 *     ACS_ERR_BUDGET; executor==NULL 负测 (detail 103); workers 越界负测;
 *     lease 输出缓冲不足 → PARAM+BUFFER_TOO_SMALL 且租借归还 (acquired==0);
 *   - config_query: host 提供 → 正测; host.config_query==NULL → UNSUPPORTED;
 *   - error 动作: request_error 稳定错误码/域映射;
 *   - metrics 形态: inspect 输出 exec_count/bytes_count/host_loaded; echo
 *     action 字节数精确累计 (bytes_count = Σ 成功回显输入字节; exec_count+1)。
 *   - 输出 JSON 缓冲不足 → PARAM + detail BUFFER_TOO_SMALL (strbuf.size=所需)。
 *
 * 独立断言 (11 §5/§6): 期望值由测试侧常数/行为产生, 不调用生产实现作 oracle。
 * 纯 C11; -Wall -Wextra 零告警; 退出码 0=全 PASS。
 */
#include "astrocs/abi/lifecycle_v1.h"
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/echo/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* astrocs_module_query_v1 声明来自 module_api_v1.h; 本 TU 不定义
 * ASTROCS_ABI_SHARED → ASTROCS_EXPORT 为空 → 普通外部符号引用 (链接 .so)。 */

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

/* ───────── host fake 状态 ───────── */

static int g_alloc_calls = 0;
static int g_free_calls = 0;

static void* t_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud; (void)align;
    g_alloc_calls++;
    if (size == 0) size = 1;
    return malloc((size_t)size);
}
static void t_free(void* ud, void* p) {
    (void)ud;
    if (p) g_free_calls++;
    free(p);
}
static acs_allocator_v1 g_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    t_alloc, t_free, NULL
};

static struct { int calls; int last_level; char comp[64]; char msg[256]; } g_log;
static void t_log(void* ud, int level, acs_str_v1 comp, acs_str_v1 msg) {
    (void)ud;
    g_log.calls++;
    g_log.last_level = level;
    memcpy(g_log.comp, comp.data ? comp.data : "", comp.size < 63 ? comp.size : 63);
    g_log.comp[comp.size < 63 ? (size_t)comp.size : 63] = 0;
    memcpy(g_log.msg, msg.data ? msg.data : "", msg.size < 255 ? msg.size : 255);
    g_log.msg[msg.size < 255 ? (size_t)msg.size : 255] = 0;
}
static acs_logger_v1 g_logger = {
    { (uint32_t)sizeof(acs_logger_v1), ACS_ABI_VERSION_V1 },
    t_log, NULL
};

static struct { int flag; } g_cancel;
static int t_is_cancelled(void* ud) {
    (void)ud;
    return g_cancel.flag;
}
static acs_cancel_v1 g_cancel_svc = {
    { (uint32_t)sizeof(acs_cancel_v1), ACS_ABI_VERSION_V1 },
    t_is_cancelled, NULL
};

static struct { int deny; int acquired; uint32_t max_workers; } g_exec;
static int t_acquire(void* ud, uint32_t n) {
    (void)ud;
    if (g_exec.deny) return 1;
    g_exec.acquired += (int)n;
    return 0;
}
static void t_release(void* ud, uint32_t n) {
    (void)ud;
    g_exec.acquired -= (int)n;
}
static acs_executor_v1 g_exec_svc = {
    { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
    8u, 8u, t_acquire, t_release, NULL
};

/* ───────── artifact host fake (服务句柄; 内容=测试自产 manifest) ───────── */
static const char kArtJson[] =
    "{\"type_id\":\"astrocs.manifest.echo\",\"schema_version\":3,"
    "\"producer\":\"echo-unit-test\"}";
static const char kDigest[] =
    "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6e7f8a9b0c1d2e3f4a5b6c7d8e9f0a1b2";
static struct { char last_uri[256]; int read_calls; int parse_calls;
                int get_str_calls; int get_u64_calls; int open_calls; } g_art;

typedef struct art_handle_s { char uri[256]; } art_handle_s;
typedef struct man_handle_s { char* own; } man_handle_s;

/* 独立 JSON 顶层标量提取 (测试侧; 只支持本 fixture 形状) */
static const char* own_json_str(const char* js, const char* key, uint64_t* out_n) {
    size_t kl = strlen(key);
    const char* p = js;
    while ((p = strstr(p, key)) != NULL) {
        if (p > js && p[-1] == '"' && p[kl] == '"') {
            const char* q = p + kl + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == ':') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                if (*q == '"') {
                    const char* e = q + 1;
                    while (*e && *e != '"') e++;
                    *out_n = (uint64_t)(e - (q + 1));
                    return q + 1;
                }
            }
        }
        p += kl;
    }
    *out_n = 0;
    return NULL;
}

static acs_status fake_artifact_open(const acs_artifact_service_v1* self,
                                     acs_str_v1 uri,
                                     acs_str_v1 expected_digest_hex,
                                     acs_artifact_handle_v1** out,
                                     acs_error_info_v1* err) {
    (void)self; (void)expected_digest_hex;
    if (out) *out = NULL;
    if (err) { memset(err, 0, sizeof(*err)); }
    g_art.open_calls++;
    if (uri.data && strstr(uri.data, "missing")) {
        if (err) {
            err->head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
            err->head.abi_version = ACS_ABI_VERSION_V1;
            err->status = ACS_ERR_IO; err->domain = ACS_ERR_DOMAIN_IO;
            err->message_utf8 = "fake: artifact missing"; err->message_bytes = 23;
        }
        return ACS_ERR_IO;
    }
    art_handle_s* h = (art_handle_s*)malloc(sizeof(art_handle_s));
    if (!h) return ACS_ERR_NOMEM;
    memset(h, 0, sizeof(*h));
    if (uri.data && uri.size < sizeof(h->uri)) {
        memcpy(h->uri, uri.data, (size_t)uri.size);
        h->uri[uri.size] = 0;
    }
    memcpy(g_art.last_uri, h->uri, sizeof(g_art.last_uri));
    *out = (acs_artifact_handle_v1*)h;
    return ACS_OK;
}
static void fake_artifact_close(acs_artifact_handle_v1* h) {
    free(h);
}
static acs_str_v1 svn(const char* s) {
    acs_str_v1 v;
    v.head.struct_size = (uint32_t)sizeof(acs_str_v1);
    v.head.abi_version = ACS_ABI_VERSION_V1;
    v.data = s;
    v.size = s ? (uint64_t)strlen(s) : 0;
    return v;
}
static acs_str_v1 fake_query_uri(const acs_artifact_handle_v1* h) {
    const art_handle_s* a = (const art_handle_s*)h;
    return svn(a->uri);
}
static acs_str_v1 fake_query_type_id(const acs_artifact_handle_v1* h) {
    (void)h;
    return svn("astrocs.artifact.echo");
}
static acs_str_v1 fake_query_digest(const acs_artifact_handle_v1* h) {
    (void)h;
    return svn(kDigest);
}
static uint64_t fake_query_size(const acs_artifact_handle_v1* h) {
    (void)h;
    return (uint64_t)strlen(kArtJson);
}
static acs_status fake_read_all(const acs_artifact_handle_v1* h,
                                const acs_allocator_v1* alloc,
                                acs_span_u8* out_data,
                                acs_error_info_v1* err) {
    (void)h;
    g_art.read_calls++;
    memset(out_data, 0, sizeof(*out_data));
    if (!alloc || !alloc->alloc) {
        if (err) { err->status = ACS_ERR_PARAM; err->domain = ACS_ERR_DOMAIN_CONFIG; }
        return ACS_ERR_PARAM;
    }
    uint64_t n = (uint64_t)strlen(kArtJson);
    uint8_t* buf = (uint8_t*)alloc->alloc(alloc->user_data, n, 1);
    if (!buf) return ACS_ERR_NOMEM;
    memcpy(buf, kArtJson, (size_t)n);
    out_data->data = buf;
    out_data->count = n;
    return ACS_OK;
}
static acs_status fake_manifest_parse(const acs_artifact_service_v1* self,
                                      acs_str_v1 json_utf8,
                                      acs_manifest_handle_v1** out,
                                      acs_error_info_v1* err) {
    (void)self;
    g_art.parse_calls++;
    if (out) *out = NULL;
    if (err) memset(err, 0, sizeof(*err));
    if (!json_utf8.data) return ACS_ERR_PARAM;
    man_handle_s* mh = (man_handle_s*)malloc(sizeof(man_handle_s));
    if (!mh) return ACS_ERR_NOMEM;
    mh->own = (char*)malloc((size_t)json_utf8.size + 1);
    if (!mh->own) { free(mh); return ACS_ERR_NOMEM; }
    memcpy(mh->own, json_utf8.data, (size_t)json_utf8.size);
    mh->own[json_utf8.size] = 0;
    *out = (acs_manifest_handle_v1*)mh;
    return ACS_OK;
}
static void fake_manifest_destroy(acs_manifest_handle_v1* h) {
    if (!h) return;
    man_handle_s* mh = (man_handle_s*)h;
    free(mh->own);
    free(mh);
}
static int fake_manifest_get_str(const acs_manifest_handle_v1* h,
                                 acs_str_v1 key,
                                 acs_str_v1* out_value) {
    const man_handle_s* mh = (const man_handle_s*)h;
    g_art.get_str_calls++;
    uint64_t n = 0;
    const char* v = own_json_str(mh->own, key.data, &n);
    if (!v) return 0;
    /* 值借入句柄内 own 缓冲 (所有权=句柄; 有效至 destroy) */
    out_value->head.struct_size = (uint32_t)sizeof(acs_str_v1);
    out_value->head.abi_version = ACS_ABI_VERSION_V1;
    out_value->data = v;
    out_value->size = n;
    return 1;
}
static int fake_manifest_get_u64(const acs_manifest_handle_v1* h,
                                 acs_str_v1 key,
                                 uint64_t* out_value) {
    const man_handle_s* mh = (const man_handle_s*)h;
    g_art.get_u64_calls++;
    size_t kl = strlen(key.data ? key.data : "");
    const char* p = mh->own;
    const char* js = mh->own;
    while ((p = strstr(p, key.data ? key.data : "")) != NULL) {
        if (p > js && p[-1] == '"' && p[kl] == '"') {
            const char* q = p + kl + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == ':') {
                q++;
                while (*q == ' ' || *q == '\t') q++;
                if (*q >= '0' && *q <= '9') {
                    uint64_t val = 0;
                    while (*q >= '0' && *q <= '9') {
                        val = val * 10 + (uint64_t)(*q - '0');
                        q++;
                    }
                    *out_value = val;
                    return 1;
                }
            }
        }
        p += kl;
    }
    return 0;
}
static acs_artifact_service_v1 g_arts = {
    { (uint32_t)sizeof(acs_artifact_service_v1), ACS_ABI_VERSION_V1 },
    fake_artifact_open, fake_artifact_close,
    fake_query_uri, fake_query_type_id, fake_query_digest, fake_query_size,
    fake_read_all,
    fake_manifest_parse, fake_manifest_destroy,
    fake_manifest_get_str, fake_manifest_get_u64
};

/* config_query fake: 返回 host 侧 module 配置 JSON (所有权=host 静态) */
static acs_status fake_config_query(const acs_host_api_v1* self,
                                    acs_str_v1 module_id,
                                    acs_str_v1 key,
                                    acs_strbuf_v1* out,
                                    acs_error_info_v1* err) {
    (void)self; (void)module_id; (void)key; (void)err;
    static const char resp[] =
        "{\"module_id\":\"astrocs.conformance.echo\",\"source\":\"fake-host\"}";
    uint64_t n = (uint64_t)strlen(resp);
    out->size = n;
    if (!out->data || out->cap == 0) return ACS_OK;
    uint64_t room = out->cap - 1;
    uint64_t wr = n < room ? n : room;
    memcpy(out->data, resp, (size_t)wr);
    out->data[wr] = 0;
    return ACS_OK;
}
static acs_status fake_registry_lookup(const acs_host_api_v1* self,
                                       acs_str_v1 module_id,
                                       acs_str_v1* out_json_utf8,
                                       acs_error_info_v1* err) {
    (void)self; (void)module_id; (void)out_json_utf8;
    if (err) {
        err->status = ACS_ERR_UNSUPPORTED; err->domain = ACS_ERR_DOMAIN_CONFIG;
    }
    return ACS_ERR_UNSUPPORTED;
}

static acs_host_api_v1 g_host;
static void host_init(void) {
    memset(&g_host, 0, sizeof(g_host));
    g_host.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    g_host.head.abi_version = ACS_ABI_VERSION_V1;
    g_host.allocator = &g_alloc;
    g_host.logger = &g_logger;
    g_host.cancel = &g_cancel_svc;
    g_host.executor = &g_exec_svc;
    g_host.artifacts = &g_arts;
    g_host.config_query = fake_config_query;
    g_host.registry_lookup_module = fake_registry_lookup;
    g_log.calls = 0; g_log.last_level = -1;
    g_cancel.flag = 0;
    g_exec.deny = 0; g_exec.acquired = 0; g_exec.max_workers = 8;
    memset(&g_art, 0, sizeof(g_art));
}

/* 小工具: create 实例 + 执行 + destroy (每次独立实例, host 隔离) */
typedef struct {
    acs_module_instance_v1* inst;
    acs_host_api_v1 host;      /* 快照宿主 (create 引用); 存活至 destroy */
    acs_status rc;
    char out[4096];
    acs_error_info_v1 err;
} ctx_s;

static void ctx_init(ctx_s* c, const acs_module_api_v1* api,
                     const acs_host_api_v1* host) {
    memset(c, 0, sizeof(*c));
    c->host = *host;
    c->err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    c->err.head.abi_version = ACS_ABI_VERSION_V1;
    c->rc = api->create(api, svn(""), &c->host,
                        (acs_module_instance_v1**)&c->inst, &c->err);
    c->err.status = 0; c->err.domain = 0; c->err.detail_code = 0;
}

static void ctx_exec(ctx_s* c, const acs_module_api_v1* api,
                     const char* input, const char* config) {
    acs_strbuf_v1 sb;
    memset(&sb, 0, sizeof(sb));
    sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
    sb.head.abi_version = ACS_ABI_VERSION_V1;
    sb.data = c->out;
    sb.cap = sizeof(c->out);
    memset(&c->err, 0, sizeof(c->err));
    c->err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
    c->err.head.abi_version = ACS_ABI_VERSION_V1;
    c->rc = api->execute(c->inst, svn(input), svn(config), &sb, &c->err);
}

static void ctx_destroy(ctx_s* c, const acs_module_api_v1* api) {
    if (c->inst) api->destroy(c->inst);
    c->inst = NULL;
}

/* cfg 构造 helper */
static void cfg_build(char* dst, size_t cap, const char* action,
                      const char* extra) {
    if (extra && extra[0])
        snprintf(dst, cap, "{\"action\":\"%s\",%s}", action, extra);
    else
        snprintf(dst, cap, "{\"action\":\"%s\"}", action);
}

int main(void) {
    host_init();
    const acs_module_api_v1* api = NULL;

    /* ═══ 1. query/describe 正/负 (ABI-001 握手 + ABI-005 descriptor 三方一致) ═══ */
    CHECK_ST(ACS_OK, astrocs_module_query_v1(ACS_ABI_VERSION_V1, &g_host, &api),
             "query ok");
    CHECK(api != NULL);
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1 + 1, &g_host, &api),
             "query abi mismatch");
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1, NULL, &api),
             "query host null");
    {
        acs_host_api_v1 h2 = g_host;
        h2.allocator = NULL;
        CHECK_ST(ACS_ERR_ABI_MISMATCH,
                 astrocs_module_query_v1(ACS_ABI_VERSION_V1, &h2, &api),
                 "query no allocator");
    }
    CHECK_ST(ACS_ERR_PARAM,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1, &g_host, NULL),
             "query out null");
    CHECK(api != NULL);

    if (api) {
        acs_module_descriptor_v1 desc;
        memset(&desc, 0, sizeof(desc));
        CHECK_ST(ACS_OK, api->describe(api, svn(""), &desc), "describe ok");
        CHECK(desc.module_id.data && desc.module_id.size ==
              strlen(ASTROCS_ECHO_MODULE_ID));
        CHECK(memcmp(desc.module_id.data, ASTROCS_ECHO_MODULE_ID,
                     desc.module_id.size) == 0);
        CHECK(desc.version.size == strlen(ASTROCS_ECHO_MODULE_VERSION) &&
              memcmp(desc.version.data, ASTROCS_ECHO_MODULE_VERSION,
                     desc.version.size) == 0);
        CHECK(desc.build_id.size == strlen(ASTROCS_ECHO_BUILD_ID) &&
              memcmp(desc.build_id.data, ASTROCS_ECHO_BUILD_ID,
                     desc.build_id.size) == 0);
        CHECK(desc.sci_id.size == 8 && memcmp(desc.sci_id.data, "SCI-NONE", 8) == 0);
        CHECK(desc.alg_id.size == 8 && memcmp(desc.alg_id.data, "ALG-NONE", 8) == 0);
        CHECK(desc.api_id.size == 11 &&
              memcmp(desc.api_id.data, "API-ABI-001", 11) == 0);
        CHECK(desc.phase == 0 && desc.execution_class == 2);
        CHECK(desc.head.abi_version == ACS_ABI_VERSION_V1);
        /* 未知 module_id → PARAM */
        CHECK_ST(ACS_ERR_PARAM,
                 api->describe(api, svn("astrocs.phase1.calibration"), &desc),
                 "describe unknown id");
        CHECK_ST(ACS_ERR_PARAM, api->describe(api, svn(""), NULL),
                 "describe out null");

        /* ═══ 2. validate_config 正/负 ═══ */
        acs_error_info_v1 err;
        memset(&err, 0, sizeof(err));
        err.head.struct_size = (uint32_t)sizeof(acs_error_info_v1);
        err.head.abi_version = ACS_ABI_VERSION_V1;
        CHECK_ST(ACS_OK, api->validate_config(
                     api, svn("{\"action\":\"echo\"}"), &err),
                 "validate echo ok");
        CHECK_ST(ACS_OK, api->validate_config(
                     api, svn("{\"action\":\"artifact_read\","
                              "\"storage_uri\":\"astrocs://echo/f\"}"), &err),
                 "validate artifact_read ok");
        CHECK_ST(ACS_OK, api->validate_config(
                     api, svn("{\"action\":\"request_error\","
                              "\"error_status\":4}"), &err),
                 "validate request_error ok");
        CHECK_ST(ACS_ERR_PARAM, api->validate_config(
                     api, svn("{\"action\":\"bogus_action\"}"), &err),
                 "validate unknown action");
        CHECK(err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA);
        CHECK_ST(ACS_ERR_PARAM, api->validate_config(
                     api, svn("{\"action\":\"request_error\"}"), &err),
                 "validate request_error missing error_status");
        CHECK_ST(ACS_ERR_PARAM, api->validate_config(
                     api, svn("{\"action\":\"request_error\","
                              "\"error_status\":99}"), &err),
                 "validate request_error error_status>70");
        CHECK_ST(ACS_ERR_PARAM, api->validate_config(api, svn(""), &err),
                 "validate empty config");

        /* ═══ 3. plan 正测 (输出 JSON; 缓冲不足) ═══ */
        {
            char pout[512];
            acs_strbuf_v1 pb;
            memset(&pb, 0, sizeof(pb));
            pb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
            pb.head.abi_version = ACS_ABI_VERSION_V1;
            pb.data = pout; pb.cap = sizeof(pout);
            memset(&err, 0, sizeof(err));
            CHECK_ST(ACS_OK, api->plan(api, svn("node-7"),
                                       svn("{\"action\":\"echo\"}"), &pb, &err),
                     "plan echo ok");
            CHECK(strstr(pout, ASTROCS_ECHO_MODULE_ID) != NULL);
            CHECK(strstr(pout, "\"action\":\"echo\"") != NULL);
            CHECK(strstr(pout, "\"node_id\":\"node-7\"") != NULL);
        }

        /* ═══ 4. create 正/负 (host allocator 正测: create 分配) ═══ */
        ctx_s c;
        ctx_init(&c, api, &g_host);
        CHECK_ST(ACS_OK, c.rc, "create ok");
        CHECK(c.inst != NULL);
        CHECK(g_alloc_calls >= 1);

        /* 4b. 缓冲不足 → PARAM + BUFFER_TOO_SMALL (echo action 回显长输入) */
        {
            const char* big = "{\"manifest\":\"0123456789abcdef0123456789abcdef\"}";
            acs_strbuf_v1 sb;
            memset(&sb, 0, sizeof(sb));
            sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
            sb.head.abi_version = ACS_ABI_VERSION_V1;
            char tiny[8];
            sb.data = tiny; sb.cap = sizeof(tiny);
            memset(&err, 0, sizeof(err));
            acs_status rc = api->execute(c.inst, svn(big),
                                         svn("{\"action\":\"echo\"}"), &sb, &err);
            CHECK_ST(ACS_ERR_PARAM, rc, "execute buffer too small");
            CHECK(err.detail_code == ACS_DIAG_ECODE_BUFFER_TOO_SMALL);
            CHECK(sb.size == strlen(big));   /* size=所需 */
        }

        /* ═══ 5. execute echo 正测 (输入往返一致 + inspect metrics) ═══ */
        ctx_exec(&c, api, "{\"hello\":\"world\",\"n\":1}",
                 "{\"action\":\"echo\"}");
        CHECK_ST(ACS_OK, c.rc, "execute echo ok");
        CHECK(strcmp(c.out, "{\"hello\":\"world\",\"n\":1}") == 0);
        {
            char iout[1024];
            acs_strbuf_v1 ib;
            memset(&ib, 0, sizeof(ib));
            ib.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
            ib.head.abi_version = ACS_ABI_VERSION_V1;
            ib.data = iout; ib.cap = sizeof(iout);
            memset(&err, 0, sizeof(err));
            CHECK_ST(ACS_OK, api->inspect(c.inst, &ib, &err), "inspect ok");
            CHECK(strstr(iout, "\"host_loaded\":1") != NULL);
            CHECK(strstr(iout, "\"exec_count\":1") != NULL);
            CHECK(strstr(iout, "\"action\":\"echo\"") != NULL);
            CHECK(strstr(iout, ASTROCS_ECHO_MODULE_ID) != NULL);
        }
        ctx_destroy(&c, api);

        /* ═══ 5b. echo action 字节数验证 (独立实例: inspect bytes_count 精确) ═══
         * echo 成功回显输入 → bytes_count += 输入字节数; exec_count += 1。
         * 独立实例避免被 §4b 失败回显污染 (共享实例 c 的 bytes_count 已含
         * 失败尝试字节, 无法精确断言)。 */
        {
            ctx_s cb;
            ctx_init(&cb, api, &g_host);
            const char* in1 = "{\"hello\":\"world\",\"n\":1}";   /* 23 字节 */
            ctx_exec(&cb, api, in1, "{\"action\":\"echo\"}");
            CHECK_ST(ACS_OK, cb.rc, "echo bytes ok #1");
            const char* in2 = "x";                                /* 1 字节 */
            ctx_exec(&cb, api, in2, "{\"action\":\"echo\"}");
            CHECK_ST(ACS_OK, cb.rc, "echo bytes ok #2");
            {
                char iout[1024];
                acs_strbuf_v1 ib;
                memset(&ib, 0, sizeof(ib));
                ib.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
                ib.head.abi_version = ACS_ABI_VERSION_V1;
                ib.data = iout; ib.cap = sizeof(iout);
                memset(&err, 0, sizeof(err));
                CHECK_ST(ACS_OK, api->inspect(cb.inst, &ib, &err), "inspect 5b ok");
                CHECK(strstr(iout, "\"exec_count\":2") != NULL);
                /* 23+1=24 字节精确累计 */
                CHECK(strstr(iout, "\"bytes_count\":24") != NULL);
            }
            ctx_destroy(&cb, api);
        }

        /* ═══ 6. host cancel 回调: echo 正测/置位 CANCELLED; request_cancel ═══ */
        {
            ctx_s c2;
            ctx_init(&c2, api, &g_host);
            g_cancel.flag = 0;
            ctx_exec(&c2, api, "in", "{\"action\":\"echo\"}");
            CHECK_ST(ACS_OK, c2.rc, "echo ok (no cancel)");
            /* host cancel 置位 → echo execute CANCELLED (安全点) */
            g_cancel.flag = 1;
            ctx_exec(&c2, api, "in", "{\"action\":\"echo\"}");
            CHECK_ST(ACS_ERR_CANCELLED, c2.rc, "echo cancelled (host flag)");
            CHECK(c2.err.domain == ACS_ERR_DOMAIN_CANCELLED);
            g_cancel.flag = 0;
            ctx_destroy(&c2, api);
        }
        {
            ctx_s c3;
            ctx_init(&c3, api, &g_host);
            g_cancel.flag = 0;
            /* request_cancel 幂等置位 → OK; 后续 execute 安全点 CANCELLED */
            CHECK_ST(ACS_OK, api->request_cancel(c3.inst), "request_cancel ok");
            ctx_exec(&c3, api, "in", "{\"action\":\"echo\"}");
            CHECK_ST(ACS_ERR_CANCELLED, c3.rc, "echo cancelled (request_cancel)");
            CHECK(c3.err.status == ACS_ERR_CANCELLED);
            ctx_destroy(&c3, api);
        }

        /* ═══ 7. artifact read 正测 (全服务链) ═══ */
        {
            ctx_s c4;
            ctx_init(&c4, api, &g_host);
            char cfg[512];
            cfg_build(cfg, sizeof(cfg), ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ,
                      "\"storage_uri\":\"astrocs://echo/fixture\"");
            ctx_exec(&c4, api, "", cfg);
            CHECK_ST(ACS_OK, c4.rc, "artifact_read ok");
            CHECK(strstr(c4.out, "\"type_id\":\"astrocs.manifest.echo\"") != NULL);
            CHECK(strstr(c4.out, "\"schema_version\":3") != NULL);
            CHECK(strstr(c4.out, "\"has_field\":1") != NULL);
            CHECK(strstr(c4.out, "\"bytes\":") != NULL);
            CHECK(g_art.open_calls == 1);
            CHECK(strcmp(g_art.last_uri, "astrocs://echo/fixture") == 0);
            CHECK(g_art.read_calls == 1);
            CHECK(g_art.parse_calls == 1);
            CHECK(g_art.get_str_calls >= 1);
            CHECK(g_art.get_u64_calls >= 1);
            int before_alloc = g_alloc_calls, before_free = g_free_calls;
            ctx_destroy(&c4, api);
            CHECK(g_free_calls - before_free == 1);   /* inst 释放 */
            (void)before_alloc;
        }
        /* 7b. artifacts==NULL 负测 (detail 101) */
        {
            acs_host_api_v1 hb = g_host;
            hb.artifacts = NULL;
            ctx_s c5;
            ctx_init(&c5, api, &hb);
            char cfg[512];
            cfg_build(cfg, sizeof(cfg), ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ,
                      "\"storage_uri\":\"astrocs://echo/fixture\"");
            ctx_exec(&c5, api, "", cfg);
            CHECK_ST(ACS_ERR_PARAM, c5.rc, "artifact_read no host artifacts");
            CHECK(c5.err.detail_code == ACS_ECHO_ECODE_ARTIFACT_MISSING);
            ctx_destroy(&c5, api);
        }
        /* 7c. storage_uri 缺失 负测 (config schema) */
        {
            ctx_s c6;
            ctx_init(&c6, api, &g_host);
            ctx_exec(&c6, api, "", "{\"action\":\"artifact_read\"}");
            CHECK_ST(ACS_ERR_PARAM, c6.rc, "artifact_read no storage_uri");
            CHECK(c6.err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA);
            ctx_destroy(&c6, api);
        }
        /* 7d. host artifact open 失败 → IO 透传 */
        {
            ctx_s c7;
            ctx_init(&c7, api, &g_host);
            char cfg[512];
            cfg_build(cfg, sizeof(cfg), ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ,
                      "\"storage_uri\":\"astrocs://echo/missing\"");
            ctx_exec(&c7, api, "", cfg);
            CHECK_ST(ACS_ERR_IO, c7.rc, "artifact_read open failed");
            CHECK(c7.err.domain == ACS_ERR_DOMAIN_IO);
            ctx_destroy(&c7, api);
        }

        /* ═══ 8. write_blocked (无写回调; FORBID-002) + logger WARN 正测 ═══ */
        {
            ctx_s c8;
            ctx_init(&c8, api, &g_host);
            g_log.calls = 0;
            ctx_exec(&c8, api, "", "{\"action\":\"write_blocked\"}");
            CHECK_ST(ACS_ERR_UNSUPPORTED, c8.rc, "write_blocked unsupported");
            CHECK(c8.err.detail_code == ACS_ECHO_ECODE_ARTIFACT_WRITE_UNSUPPORTED);
            CHECK(c8.err.domain == ACS_ERR_DOMAIN_DATA);
            CHECK(g_log.calls == 1);
            CHECK(g_log.last_level == ACS_LOG_WARN);
            CHECK(strstr(g_log.msg, "write not supported") != NULL);
            CHECK(strstr(g_log.comp, ASTROCS_ECHO_MODULE_ID) != NULL);
            ctx_destroy(&c8, api);
        }

        /* ═══ 9. cancel_poll: 未置位 OK / host 置位 CANCELLED / cancel==NULL 负测 ═══ */
        {
            ctx_s c9;
            ctx_init(&c9, api, &g_host);
            g_cancel.flag = 0;
            ctx_exec(&c9, api, "", "{\"action\":\"cancel_poll\"}");
            CHECK_ST(ACS_OK, c9.rc, "cancel_poll ok");
            CHECK(strstr(c9.out, "\"cancelled\":0") != NULL);
            g_cancel.flag = 1;
            ctx_exec(&c9, api, "", "{\"action\":\"cancel_poll\"}");
            CHECK_ST(ACS_ERR_CANCELLED, c9.rc, "cancel_poll cancelled");
            g_cancel.flag = 0;
            ctx_destroy(&c9, api);
        }
        {
            acs_host_api_v1 hb = g_host;
            hb.cancel = NULL;
            ctx_s c10;
            ctx_init(&c10, api, &hb);
            ctx_exec(&c10, api, "", "{\"action\":\"cancel_poll\"}");
            CHECK_ST(ACS_ERR_PARAM, c10.rc, "cancel_poll no host cancel");
            CHECK(c10.err.detail_code == ACS_ECHO_ECODE_CANCEL_MISSING);
            ctx_destroy(&c10, api);
        }

        /* ═══ 10. executor lease: 正测 acquire+release / budget denied / NULL 负测 ═══ */
        {
            ctx_s c11;
            ctx_init(&c11, api, &g_host);
            g_exec.deny = 0;
            ctx_exec(&c11, api, "",
                     "{\"action\":\"executor_lease\",\"workers\":2}");
            CHECK_ST(ACS_OK, c11.rc, "lease ok");
            CHECK(strstr(c11.out, "\"workers\":\"released\"") != NULL);
            CHECK(g_exec.acquired == 0);   /* release 归还 */
            g_exec.deny = 1;
            ctx_exec(&c11, api, "",
                     "{\"action\":\"executor_lease\",\"workers\":2}");
            CHECK_ST(ACS_ERR_BUDGET, c11.rc, "lease budget denied");
            g_exec.deny = 0;
            ctx_destroy(&c11, api);
        }
        {
            acs_host_api_v1 hb = g_host;
            hb.executor = NULL;
            ctx_s c12;
            ctx_init(&c12, api, &hb);
            ctx_exec(&c12, api, "", "{\"action\":\"executor_lease\"}");
            CHECK_ST(ACS_ERR_PARAM, c12.rc, "lease no host executor");
            CHECK(c12.err.detail_code == ACS_ECHO_ECODE_EXECUTOR_MISSING);
            ctx_destroy(&c12, api);
        }
        {
            ctx_s c13;
            ctx_init(&c13, api, &g_host);
            ctx_exec(&c13, api, "",
                     "{\"action\":\"executor_lease\",\"workers\":0}");
            CHECK_ST(ACS_ERR_PARAM, c13.rc, "lease workers=0 rejected");
            CHECK(c13.err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA);
            ctx_destroy(&c13, api);
        }
        /* 10b. lease 输出缓冲不足 → acquire 成功→release 归还→strbuf_write
         * 截断 → PARAM + BUFFER_TOO_SMALL; 租借不泄漏 (acquired==0) */
        {
            ctx_s c13b;
            ctx_init(&c13b, api, &g_host);
            g_exec.deny = 0;
            acs_strbuf_v1 sb;
            memset(&sb, 0, sizeof(sb));
            sb.head.struct_size = (uint32_t)sizeof(acs_strbuf_v1);
            sb.head.abi_version = ACS_ABI_VERSION_V1;
            char tiny[8];
            sb.data = tiny; sb.cap = sizeof(tiny);
            memset(&err, 0, sizeof(err));
            acs_status rc = api->execute(
                c13b.inst, svn(""), svn("{\"action\":\"executor_lease\","
                                        "\"workers\":1}"), &sb, &err);
            CHECK_ST(ACS_ERR_PARAM, rc, "lease output buffer too small");
            CHECK(err.detail_code == ACS_DIAG_ECODE_BUFFER_TOO_SMALL);
            CHECK(g_exec.acquired == 0);   /* 归还发生 (acquire=1 后 release) */
            ctx_destroy(&c13b, api);
        }

        /* ═══ 11. config_query: host 提供 → 正测; NULL → UNSUPPORTED ═══ */
        {
            ctx_s c14;
            ctx_init(&c14, api, &g_host);
            ctx_exec(&c14, api, "", "{\"action\":\"config_query\"}");
            CHECK_ST(ACS_OK, c14.rc, "config_query ok");
            CHECK(strstr(c14.out, "\"source\":\"fake-host\"") != NULL);
            ctx_destroy(&c14, api);
        }
        {
            acs_host_api_v1 hb = g_host;
            hb.config_query = NULL;
            ctx_s c15;
            ctx_init(&c15, api, &hb);
            ctx_exec(&c15, api, "", "{\"action\":\"config_query\"}");
            CHECK_ST(ACS_ERR_UNSUPPORTED, c15.rc, "config_query unsupported");
            CHECK(c15.err.detail_code == ACS_DIAG_ECODE_UNSUPPORTED_OP);
            ctx_destroy(&c15, api);
        }

        /* ═══ 12. error 动作: 稳定错误码 + 域映射 (IO→IO 域; PARAM→CONFIG 域) ═══ */
        {
            ctx_s c16;
            ctx_init(&c16, api, &g_host);
            ctx_exec(&c16, api, "",
                     "{\"action\":\"request_error\",\"error_status\":4}");
            CHECK_ST(ACS_ERR_IO, c16.rc, "request_error IO");
            CHECK(c16.err.domain == ACS_ERR_DOMAIN_IO);
            ctx_exec(&c16, api, "",
                     "{\"action\":\"request_error\",\"error_status\":1}");
            CHECK_ST(ACS_ERR_PARAM, c16.rc, "request_error PARAM");
            CHECK(c16.err.domain == ACS_ERR_DOMAIN_CONFIG);
            ctx_destroy(&c16, api);
        }

        /* ═══ 13. execute 未知 action → PARAM (模块级负测) ═══ */
        {
            ctx_s c17;
            ctx_init(&c17, api, &g_host);
            ctx_exec(&c17, api, "", "{\"action\":\"no_such_action\"}");
            CHECK_ST(ACS_ERR_PARAM, c17.rc, "execute unknown action");
            CHECK(c17.err.detail_code == ACS_DIAG_ECODE_CONFIG_SCHEMA);
            ctx_destroy(&c17, api);
        }

        /* ═══ 14. allocator 平衡: 实例 + artifact 读写全部归还 (free==alloc) ═══ */
        {
            g_alloc_calls = 0;
            g_free_calls = 0;
            ctx_s c18;
            ctx_init(&c18, api, &g_host);
            char cfg[512];
            cfg_build(cfg, sizeof(cfg), ASTROCS_ECHO_CFG_ACTION_ARTIFACT_READ,
                      "\"storage_uri\":\"astrocs://echo/fixture\"");
            ctx_exec(&c18, api, "", cfg);
            CHECK_ST(ACS_OK, c18.rc, "balance artifact_read ok");
            ctx_destroy(&c18, api);
            CHECK(g_free_calls == g_alloc_calls);
            CHECK(g_alloc_calls > 0);
        }

        /* ═══ 15. create 负测: host NULL / allocator 缺失 → PARAM ═══ */
        {
            acs_error_info_v1 e2;
            memset(&e2, 0, sizeof(e2));
            acs_module_instance_v1* inst = (acs_module_instance_v1*)0x1;
            CHECK_ST(ACS_ERR_PARAM, api->create(api, svn(""), NULL, &inst, &e2),
                     "create host null");
            CHECK(inst == NULL);
            acs_host_api_v1 hb = g_host;
            hb.allocator = NULL;
            inst = (acs_module_instance_v1*)0x1;
            memset(&e2, 0, sizeof(e2));
            CHECK_ST(ACS_ERR_PARAM, api->create(api, svn(""), &hb, &inst, &e2),
                     "create no allocator");
            CHECK(inst == NULL);
        }
    }

    if (g_failures) {
        fprintf(stderr, "echo_host_callback_test: FAIL %d failures\n", g_failures);
        return 1;
    }
    printf("echo_host_callback_test: ALL PASS (module=%s, checks driven "
           "through all host callbacks)\n", ASTROCS_ECHO_MODULE_ID);
    return 0;
}
