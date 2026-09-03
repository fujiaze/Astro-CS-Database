#define _POSIX_C_SOURCE 200112L
/* AstroCS CPU baseline provider — query 握手与 capability 门测试
 * tests/cpu/baseline/provider_handshake_test.c (CPU-002)
 *
 * 覆盖 (CPU-002 验收: provider query/self_test/export/ABI 完整):
 *   1. 合法 query: host_abi=ACS_ABI_VERSION_V1 + host(带 allocator) →
 *      ACS_OK, out_api 非空; provider 表 head 合法;
 *   2. 负测: host_abi 失配 → ACS_ERR_ABI_MISMATCH (不降级猜测);
 *      host NULL / host 缺 allocator → ACS_ERR_ABI_MISMATCH;
 *      out_api NULL → ACS_ERR_PARAM;
 *   3. kernel_list: 返回 12 条; kernel_id/sci_contract_id 全部非空;
 *      precision/determinism_class 合法 (对照 baseline_provider_v1.h 常量);
 *   4. self_test: host 服务往返通过 → ACS_OK (无 allocator → PARAM);
 *   5. capability 门 (capability 不足 → 拒绝): 生产 query 经真实 CPUID
 *      (本机 amd64 SSE2 恒备 → 成功); 能力不足负测由独立 capability 门
 *      stub 测试覆盖 (provider_capability_gate_test.c), 不重复探测。
 *
 * 链接: 本 TU + baseline_provider.cpp + capability_detect.c (真实探测)。
 * 纯 C11; 退出码 0=全 PASS。独立期望断言生成, 不调用生产实现做 oracle。
 */
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/baseline_provider_v1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ── 最小 host (allocator + executor + cancel + logger) ── */
static int g_alloc_balance = 0;
static void* fake_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud;
    void* p = NULL;
    if (align <= sizeof(void*)) {
        p = malloc((size_t)size);
    } else {
        if (posix_memalign(&p, (size_t)align, (size_t)size) != 0) p = NULL;
    }
    if (p) g_alloc_balance += 1;
    return p;
}
static void fake_free(void* ud, void* p) {
    (void)ud;
    g_alloc_balance -= 1;
    free(p);
}
static int g_acquired = 0;
static int fake_acquire(void* ud, uint32_t n) {
    (void)ud; (void)n;
    g_acquired += 1;   /* 计数往返 (单测断言 1) */
    return 0;
}
static void fake_release(void* ud, uint32_t n) { (void)ud; (void)n; }
static int fake_cancel(void* ud) { (void)ud; return 0; }
static void fake_log(void* ud, int level, acs_str_v1 comp, acs_str_v1 msg) {
    (void)ud; (void)level; (void)comp; (void)msg;
}

static acs_allocator_v1 g_alloc = {
    { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
    fake_alloc, fake_free, NULL
};
static acs_executor_v1 g_exec = {
    { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
    4, 4, fake_acquire, fake_release, NULL
};
static acs_cancel_v1 g_cancel = {
    { (uint32_t)sizeof(acs_cancel_v1), ACS_ABI_VERSION_V1 },
    fake_cancel, NULL
};
static acs_logger_v1 g_logger = {
    { (uint32_t)sizeof(acs_logger_v1), ACS_ABI_VERSION_V1 },
    fake_log, NULL
};

static acs_host_api_v1 g_host;
static void host_init(void) {
    memset(&g_host, 0, sizeof(g_host));
    g_host.head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    g_host.head.abi_version = ACS_ABI_VERSION_V1;
    g_host.allocator = &g_alloc;
    g_host.executor = &g_exec;
    g_host.cancel = &g_cancel;
    g_host.logger = &g_logger;
}

int main(void) {
    host_init();
    const acs_provider_api_v1* api = NULL;

    /* 1. 正测: 合法 query (本机 amd64: SSE2 恒 os_safe) */
    CHECK_ST(ACS_OK,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &g_host, &api),
             "query ok");
    CHECK(api != NULL);
    if (api) {
        CHECK(api->head.struct_size == (uint32_t)sizeof(acs_provider_api_v1));
        CHECK(api->head.abi_version == ACS_ABI_VERSION_V1);
        CHECK(api->self_test != NULL);
        CHECK(api->kernel_list != NULL);
        CHECK(api->run_kernel != NULL);
    }

    /* 2. 负测: host_abi 失配 */
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1 + 1, &g_host, &api),
             "abi mismatch");
    /* host NULL */
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, NULL, &api),
             "host null");
    /* host 缺 allocator */
    {
        acs_host_api_v1 h2 = g_host;
        h2.allocator = NULL;
        CHECK_ST(ACS_ERR_ABI_MISMATCH,
                 astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &h2, &api),
                 "no allocator");
    }
    /* out_api NULL */
    CHECK_ST(ACS_ERR_PARAM,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &g_host, NULL),
             "out null");

    /* 3. kernel_list */
    if (api) {
        uint32_t count = 0;
        const acs_kernel_desc_v1* ks = NULL;
        CHECK_ST(ACS_OK, api->kernel_list(&g_host, &count, &ks), "kernel_list");
        CHECK(count == ACS_CPU_BASELINE_KERNEL_COUNT);
        CHECK(ks != NULL);
        /* 12 条全部 kernel_id/sci_contract_id 非空; desc 与头常量一致 */
        const char* expected[ACS_CPU_BASELINE_KERNEL_COUNT] = {
            "calibration-pixel-transform", "noise-snr-reductions",
            "wcs-psf-batch", "drizzle-overlap", "drizzle-accumulate",
            "drizzle-normalize", "upm-spmv", "upm-residual",
            "upm-weight-update", "rejection-statistics",
            "integration-accumulate", "hips-bulk-transform"
        };
        for (uint32_t i = 0; i < count && i < ACS_CPU_BASELINE_KERNEL_COUNT; ++i) {
            CHECK(ks[i].head.abi_version == ACS_ABI_VERSION_V1);
            CHECK(ks[i].kernel_id.data != NULL && ks[i].kernel_id.size > 0);
            CHECK(ks[i].sci_contract_id.data != NULL &&
                  ks[i].sci_contract_id.size > 0);
            if (i < ACS_CPU_BASELINE_KERNEL_COUNT) {
                CHECK(ks[i].kernel_id.size == strlen(expected[i]));
                CHECK(memcmp(ks[i].kernel_id.data, expected[i],
                             ks[i].kernel_id.size) == 0);
            }
            CHECK(ks[i].precision == ACS_CPU_BASELINE_PREC_F32);
            CHECK(ks[i].determinism_class == ACS_CPU_BASELINE_DET_BITWISE ||
                  ks[i].determinism_class == ACS_CPU_BASELINE_DET_FIXED_ORDER);
        }
        /* kernel_id 唯一 */
        for (uint32_t i = 0; i < count; ++i)
            for (uint32_t j = i + 1; j < count; ++j) {
                CHECK(!(ks[i].kernel_id.size == ks[j].kernel_id.size &&
                        memcmp(ks[i].kernel_id.data, ks[j].kernel_id.data,
                               ks[i].kernel_id.size) == 0));
            }
    }

    /* 4. self_test (host 服务齐全 → OK; allocator 缺 → PARAM) */
    if (api) {
        CHECK_ST(ACS_OK, api->self_test(&g_host), "self_test ok");
        CHECK(g_alloc_balance == 0);   /* alloc/free 平衡 */
        CHECK(g_acquired >= 1);
        acs_host_api_v1 h2 = g_host;
        h2.allocator = NULL;
        CHECK_ST(ACS_ERR_PARAM, api->self_test(&h2), "self_test no allocator");
        acs_host_api_v1 h3 = g_host;
        h3.executor = NULL;
        CHECK_ST(ACS_OK, api->self_test(&h3), "self_test no executor (可空)");
    }

    if (g_failures) {
        fprintf(stderr, "PROVIDER HANDSHAKE FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_handshake_test: ALL PASS (provider=%s kernels=%u)\n",
           ACS_CPU_BASELINE_PROVIDER_ID, ACS_CPU_BASELINE_KERNEL_COUNT);
    return 0;
}
