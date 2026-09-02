/* AstroCS noop module — 握手负/正测 (BLD-003 SKELETON)
 *
 * 覆盖 (BLD-003 可加载性验收 + ABI-001 握手规则):
 *   - query 成功: host_abi 匹配 + host 带 allocator + out_api 非空 → ACS_OK,
 *     out_api 非空;
 *   - host_abi 失配 → ACS_ERR_ABI_MISMATCH (不降级猜测);
 *   - host 缺失/allocator 缺失 → ACS_ERR_ABI_MISMATCH;
 *   - out_api 缺失 → ACS_ERR_PARAM;
 *   - describe: module_id 匹配返回 descriptor, module_id 字段与
 *     ASTROCS_NOOP_MODULE_ID 一致; 未知 module_id → ACS_ERR_PARAM。
 *
 * 纯 C11; 退出码 0=全 PASS。测试期望由独立断言生成 (不调用生产实现做 oracle)。
 */
#include "astrocs/noop/types.h"
#include "astrocs/abi/module_api_v1.h"

#include <stdio.h>
#include <string.h>

/* astrocs_module_query_v1 声明来自 module_api_v1.h; 本测试 TU 不定义
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

/* 最小 host (仅 allocator 字段; 本测试只握手, 不触发 execute) */
static void* fake_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud; (void)size; (void)align;
    return NULL;   /* 不被调用 */
}
static void fake_free(void* ud, void* p) { (void)ud; (void)p; }

static acs_allocator_v1 g_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    fake_alloc, fake_free, NULL
};
static acs_host_api_v1 g_host;

static void host_init(void) {
    memset(&g_host, 0, sizeof(g_host));
    g_host.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    g_host.head.abi_version = ACS_ABI_VERSION_V1;
    g_host.allocator = &g_alloc;
}

int main(void) {
    host_init();
    const acs_module_api_v1* api = NULL;

    /* 1. 正测: 合法握手 */
    CHECK_ST(ACS_OK, astrocs_module_query_v1(ACS_ABI_VERSION_V1, &g_host, &api),
             "query ok");
    CHECK(api != NULL);
    if (api) {
        acs_module_descriptor_v1 desc;
        memset(&desc, 0, sizeof(desc));
        acs_str_v1 want;
        want.head.struct_size = (uint32_t)sizeof(acs_str_v1);
        want.head.abi_version = ACS_ABI_VERSION_V1;
        want.data = ASTROCS_NOOP_MODULE_ID;
        want.size = (uint64_t)strlen(ASTROCS_NOOP_MODULE_ID);
        CHECK_ST(ACS_OK, api->describe(api, want, &desc), "describe ok");
        CHECK(desc.module_id.data != NULL);
        CHECK(desc.module_id.size == strlen(ASTROCS_NOOP_MODULE_ID));
        CHECK(memcmp(desc.module_id.data, ASTROCS_NOOP_MODULE_ID,
                     desc.module_id.size) == 0);

        /* 未知 module_id → PARAM */
        acs_str_v1 other = { { (uint32_t)sizeof(acs_str_v1), ACS_ABI_VERSION_V1 },
                             "astrocs.phase1.calibration", 26 };
        CHECK_ST(ACS_ERR_PARAM, api->describe(api, other, &desc), "describe unknown id");
    }

    /* 2. 负测: host_abi 失配 */
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1 + 1, &g_host, &api),
             "abi mismatch");

    /* 3. 负测: host NULL */
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1, NULL, &api),
             "host null");

    /* 4. 负测: allocator 缺失 */
    acs_host_api_v1 h2 = g_host;
    h2.allocator = NULL;
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1, &h2, &api),
             "no allocator");

    /* 5. 负测: out_api NULL */
    CHECK_ST(ACS_ERR_PARAM,
             astrocs_module_query_v1(ACS_ABI_VERSION_V1, &g_host, NULL),
             "out null");

    if (g_failures) {
        fprintf(stderr, "SELFTEST FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("noop_handshake_test: ALL PASS (module=%s)\n", ASTROCS_NOOP_MODULE_ID);
    return 0;
}
