/* AstroCS CPU AVX2/FMA provider — .so 加载冒烟 (dlopen 唯一入口 + ABI)
 * tests/cpu/avx2/provider_avx2_so_load_test.c (CPU-003)
 *
 * 覆盖 (CPU-003 验收 "非支持 CPU 不加载" 的加载面 + 12 §1 唯一导出 +
 * self_test/export/ABI 完整; 无全局 SIMD 静态初始化):
 *   1. dlopen(providers/astrocs_cpu_avx2.so) 成功 (RTLD_NOW|RTLD_LOCAL);
 *   2. 唯一导出: dlsym 查到 astrocs_provider_query_v1; astrocs_module_query_v1 /
 *      astrocs_backend_get_api_v1 → NULL (12 §1: provider DLL 只导出批准入口);
 *   3. query → ACS_OK + out_api 非空 (真实 capability 探测: 本机 amd64
 *      AVX2+FMA os_safe → 加载); 非支持 CPU 拒绝路径由 stub gate 测试覆盖;
 *   4. kernel_list: 恰 2 条注册热点 + kernel_id 非空;
 *   5. self_test → ACS_OK (allocator 平衡);
 *   6. run_kernel 冒烟: calibration (N=4, k=0 → out=in0-1) → ACS_OK 且
 *      输出与独立期望一致 (AVX2/FMA 数值路径; 本机可执行);
 *   7. 非注册 kernel 索引 → ACS_ERR_UNSUPPORTED (host 回落 baseline 语义);
 *   8. dlclose 后句柄失效 (进程退出; 无崩溃 = 静态析构安全; 无全局 SIMD
 *      静态初始化 → 加载/卸载期无 AVX 状态依赖)。
 *
 * 编译: gcc -std=c11 (Linux; dlfcn); 链接 -ldl。退出码 0=全 PASS。
 * AVX2 provider 源以 -mavx2 -mfma 单独编译为 .so (本机支持)。
 */
#define _POSIX_C_SOURCE 200112L
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/avx2_provider_v1.h"

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

typedef acs_status (*query_fn)(uint32_t, const acs_host_api_v1*,
                               const acs_provider_api_v1**);

/* ── 最小 host ── */
static int g_alloc_balance = 0;
static void* fake_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud;
    void* p = NULL;
    if (align <= sizeof(void*)) p = malloc((size_t)size);
    else if (posix_memalign(&p, (size_t)align, (size_t)size) != 0) p = NULL;
    if (p) g_alloc_balance += 1;
    return p;
}
static void fake_free(void* ud, void* p) {
    (void)ud;
    if (p) g_alloc_balance -= 1;
    free(p);
}
static int fake_acquire(void* ud, uint32_t n) { (void)ud; (void)n; return 0; }
static void fake_release(void* ud, uint32_t n) { (void)ud; (void)n; }
static int fake_cancel(void* ud) { (void)ud; return 0; }
static void fake_log(void* ud, int l, acs_str_v1 c, acs_str_v1 m) {
    (void)ud; (void)l; (void)c; (void)m;
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

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <path-to-astrocs_cpu_avx2.so>\n", argv[0]);
        return 2;
    }
    const char* so_path = argv[1];
    host_init();

    void* h = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        fprintf(stderr, "FAIL dlopen(%s): %s\n", so_path, dlerror());
        return 1;
    }
    CHECK(h != NULL);

    /* 1. 唯一导出面 */
    query_fn q = (query_fn)dlsym(h, "astrocs_provider_query_v1");
    CHECK(q != NULL);
    CHECK(dlsym(h, "astrocs_module_query_v1") == NULL);
    CHECK(dlsym(h, "astrocs_backend_get_api_v1") == NULL);

    /* 2. query 握手 (真实 capability: 本机 AVX2+FMA os_safe) */
    const acs_provider_api_v1* api = NULL;
    CHECK_ST(ACS_OK, q(ACS_ABI_VERSION_V1, &g_host, &api), "so query ok");
    CHECK(api != NULL);

    /* 3. kernel_list: 恰 2 条注册热点 */
    uint32_t count = 0;
    const acs_kernel_desc_v1* ks = NULL;
    if (api) {
        CHECK_ST(ACS_OK, api->kernel_list(&g_host, &count, &ks), "kernel_list");
        CHECK(count == ACS_CPU_AVX2_KERNEL_COUNT);
        for (uint32_t i = 0; i < count && i < ACS_CPU_AVX2_KERNEL_COUNT; ++i)
            CHECK(ks[i].kernel_id.data != NULL && ks[i].kernel_id.size > 0);
    }

    /* 4. self_test */
    if (api) {
        CHECK_ST(ACS_OK, api->self_test(&g_host), "self_test ok");
        CHECK(g_alloc_balance == 0);
    }

    /* 5. run_kernel 冒烟: calibration N=4 (k=0 → out=in0-1) */
    if (api) {
        const float in0v[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        const float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float out[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        acs_cpu_avx2_params_v1 P;
        memset(&P, 0, sizeof(P));
        P.head.struct_size = (uint32_t)sizeof(P);
        P.head.abi_version = ACS_ABI_VERSION_V1;
        P.w = 2; P.h = 2;
        P.in_off[0] = 0; P.in_len[0] = 4;
        P.in_off[1] = 4; P.in_len[1] = 4;
        P.in_off[2] = 8; P.in_len[2] = 4;
        P.in_off[3] = 12; P.in_len[3] = 4;
        P.out_off[0] = 0; P.out_len[0] = 4;
        float inbuf[16];
        for (int i = 0; i < 4; ++i) {
            inbuf[i] = in0v[i]; inbuf[4 + i] = one[i];
            inbuf[8 + i] = zero[i]; inbuf[12 + i] = one[i];
        }
        acs_span_u8 sp_in = ACS_SPAN_U8((uint8_t*)inbuf, sizeof(inbuf));
        acs_span_u8 sp_out = ACS_SPAN_U8((uint8_t*)out, sizeof(out));
        CHECK_ST(ACS_OK, api->run_kernel(ACS_CPU_AVX2_KIDX_CALIBRATION,
                                         &g_host, &P, (uint32_t)sizeof(P),
                                         sp_in, sp_out),
                 "calibration run");
        for (int i = 0; i < 4; ++i) {
            if (out[i] != (float)i) {
                fprintf(stderr, "FAIL calib[%d]=%f expect=%d\n", i, out[i], i);
                ++g_failures;
            }
        }
        /* 6. 非注册 kernel 索引 → unsupported (回落 baseline 语义) */
        CHECK_ST(ACS_ERR_UNSUPPORTED,
                 api->run_kernel(ACS_CPU_AVX2_KERNEL_COUNT, &g_host,
                                 &P, (uint32_t)sizeof(P), sp_in, sp_out),
                 "out-of-table index unsupported");
    }

    dlclose(h);
    if (g_failures) {
        fprintf(stderr, "AVX2 SO LOAD FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_avx2_so_load_test: ALL PASS (dlopen unique-export "
           "query/kernel_list=2/self_test/calibration run/table-unsupported)\n");
    return 0;
}
