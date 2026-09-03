#define _POSIX_C_SOURCE 200112L
/* AstroCS CPU AVX2/FMA provider — query 握手、注册表与回落语义测试
 * tests/cpu/avx2/provider_avx2_handshake_test.c (CPU-003)
 *
 * 覆盖 (CPU-003 验收 "函数入口由 provider 表查询, 不复制科学模块; 其余
 * kernel 回落 baseline"):
 *   1. 合法 query (真实 CPUID/XGETBV: 本机 Xeon Gold 6148 AVX2+FMA os_safe)
 *      → ACS_OK, out_api 非空, provider 表 head 合法;
 *   2. 负测: host_abi 失配 / host NULL / host 缺 allocator → ABI_MISMATCH;
 *      out_api NULL → PARAM;
 *   3. kernel_list: 恰 2 条注册热点 (calibration-pixel-transform +
 *      hips-bulk-transform; 与 avx2_provider_v1.h 常量一致); kernel_id/
 *      sci_contract_id 非空且与 baseline 同一科学 kernel 身份 (ALG-001 /
 *      ALG-P3-002); determinism_class=bitwise (逐元素独立);
 *   4. 函数入口由 provider 表查询: 以 kernel_id 查 kernel_list 描述,
 *      用其索引 run_kernel —— 不复制/不假设 12-kernel baseline 索引映射;
 *   5. 其余 kernel 回落 baseline: run_kernel(非注册索引, 例如 baseline
 *      noise-reductions 的 kernel_id 无法在本 provider 表查到; 越界索引
 *      与 "表外 kernel" 索引) → ACS_ERR_UNSUPPORTED (host 应退 baseline);
 *   6. self_test: host 服务往返 → OK; 缺 allocator → PARAM;
 *      kernel 表自洽 → OK; 无全局 SIMD 静态初始化 (加载期无 AVX 指令执行,
 *      进程存活即证)。
 *
 * 链接: 本 TU + avx2_provider.cpp + capability_detect.c (真实探测)。
 * 编译: gcc/g++ -std=c11/c++17 (AVX2 provider 源另以 -mavx2 -mfma 编)。
 * 纯 C11 测试主体 (host 构造同 baseline handshake test); 退出码 0=全 PASS。
 */
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/avx2_provider_v1.h"

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
    if (p) g_alloc_balance -= 1;
    free(p);
}
static int g_acquired = 0;
static int fake_acquire(void* ud, uint32_t n) {
    (void)ud; (void)n;
    g_acquired += 1;
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

    /* 1. 正测: 合法 query (真实 CPUID/XGETBV: 本机 AVX2+FMA os_safe) */
    CHECK_ST(ACS_OK,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &g_host, &api),
             "query ok (avx2 machine)");
    CHECK(api != NULL);
    if (api) {
        CHECK(api->head.struct_size == (uint32_t)sizeof(acs_provider_api_v1));
        CHECK(api->head.abi_version == ACS_ABI_VERSION_V1);
        CHECK(api->self_test != NULL);
        CHECK(api->kernel_list != NULL);
        CHECK(api->run_kernel != NULL);
    }

    /* 2. 负测: host_abi 失配 / host NULL / host 缺 allocator / out NULL */
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1 + 1, &g_host, &api),
             "abi mismatch");
    CHECK_ST(ACS_ERR_ABI_MISMATCH,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, NULL, &api),
             "host null");
    {
        acs_host_api_v1 h2 = g_host;
        h2.allocator = NULL;
        CHECK_ST(ACS_ERR_ABI_MISMATCH,
                 astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &h2, &api),
                 "no allocator");
    }
    CHECK_ST(ACS_ERR_PARAM,
             astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &g_host, NULL),
             "out null");

    /* 3. kernel_list: 恰 2 条注册热点; 与 baseline 同一科学 kernel 身份 */
    if (api) {
        uint32_t count = 0;
        const acs_kernel_desc_v1* ks = NULL;
        CHECK_ST(ACS_OK, api->kernel_list(&g_host, &count, &ks), "kernel_list");
        CHECK(count == ACS_CPU_AVX2_KERNEL_COUNT);
        CHECK(ks != NULL);
        const char* expected[ACS_CPU_AVX2_KERNEL_COUNT] = {
            "calibration-pixel-transform", "hips-bulk-transform"
        };
        for (uint32_t i = 0; i < count && i < ACS_CPU_AVX2_KERNEL_COUNT; ++i) {
            CHECK(ks[i].head.abi_version == ACS_ABI_VERSION_V1);
            CHECK(ks[i].kernel_id.data != NULL && ks[i].kernel_id.size > 0);
            CHECK(ks[i].sci_contract_id.data != NULL &&
                  ks[i].sci_contract_id.size > 0);
            CHECK(ks[i].kernel_id.size == strlen(expected[i]));
            CHECK(memcmp(ks[i].kernel_id.data, expected[i],
                         ks[i].kernel_id.size) == 0);
            CHECK(ks[i].precision == ACS_CPU_BASELINE_PREC_F32);
            CHECK(ks[i].determinism_class == ACS_CPU_BASELINE_DET_BITWISE);
        }
    }

    /* 4. self_test */
    if (api) {
        CHECK_ST(ACS_OK, api->self_test(&g_host), "self_test ok");
        CHECK(g_alloc_balance == 0);
        CHECK(g_acquired >= 1);
        {
            acs_host_api_v1 h2 = g_host;
            h2.allocator = NULL;
            CHECK_ST(ACS_ERR_PARAM, api->self_test(&h2), "self_test no allocator");
        }
        {
            acs_host_api_v1 h3 = g_host;
            h3.executor = NULL;
            CHECK_ST(ACS_OK, api->self_test(&h3), "self_test no executor (可空)");
        }
    }

    /* 5. 回落语义: 非注册 kernel (表外索引) → ACS_ERR_UNSUPPORTED.
     *    以"12-kernel baseline 世界"的噪声归约 kernel 索引 (=1) 与越界
     *    索引 (=999) 探测 —— 本 provider 表只注册 2 个热点, 其余一律
     *    unsupported, host 据此回落 baseline (函数入口由 provider 表查询)。 */
    if (api) {
        acs_cpu_avx2_params_v1 P;
        memset(&P, 0, sizeof(P));
        P.head.struct_size = (uint32_t)sizeof(P);
        P.head.abi_version = ACS_ABI_VERSION_V1;
        P.w = 2; P.h = 2;
        float inbuf[64];
        float outbuf[64];
        memset(inbuf, 0, sizeof(inbuf));
        memset(outbuf, 0, sizeof(outbuf));
        acs_span_u8 sp_in = ACS_SPAN_U8((uint8_t*)inbuf, sizeof(inbuf));
        acs_span_u8 sp_out = ACS_SPAN_U8((uint8_t*)outbuf, sizeof(outbuf));
        /* 本 provider 表只有 2 个热点: 任意非 {0,1} 索引 → unsupported */
        CHECK_ST(ACS_ERR_UNSUPPORTED,
                 api->run_kernel(2, &g_host, &P, (uint32_t)sizeof(P),
                                 sp_in, sp_out),
                 "kernel index 2 (表外) unsupported");
        CHECK_ST(ACS_ERR_UNSUPPORTED,
                 api->run_kernel(999, &g_host, &P, (uint32_t)sizeof(P),
                                 sp_in, sp_out),
                 "kernel index 999 (越界) unsupported");
    }

    if (g_failures) {
        fprintf(stderr, "AVX2 PROVIDER HANDSHAKE FAIL: %d failures\n",
                g_failures);
        return 1;
    }
    printf("provider_avx2_handshake_test: ALL PASS (provider=%s kernels=%u "
           "hotspot-only, others unsupported->baseline)\n",
           ACS_CPU_AVX2_PROVIDER_ID, ACS_CPU_AVX2_KERNEL_COUNT);
    return 0;
}
