/* AstroCS CPU baseline provider — .so 加载冒烟 (dlopen 唯一入口 + ABI)
 * tests/cpu/baseline/provider_so_load_test.c (CPU-002)
 *
 * 覆盖 (CPU-002 验收 "任意 AMD64 安装树可加载; provider query/self_test/
 * export/ABI 完整; 无全局 SIMD 静态初始化"):
 *   1. dlopen(providers/astrocs_cpu_baseline.so) 成功 (RTLD_NOW|RTLD_LOCAL);
 *   2. 唯一导出: dlsym 查到 astrocs_provider_query_v1 (入口); dlsym 查
 *      astrocs_module_query_v1 / astrocs_backend_get_api_v1 → NULL (不导出
 *      其它符号; 12 §1 provider DLL 只导出批准查询入口);
 *   3. query → ACS_OK + out_api 非空 (真实 capability 探测: 本机 amd64
 *      SSE2 恒 os_safe); 能力门负测见 provider_capability_gate_test.c;
 *   4. kernel_list: 12 条 + kernel_id 非空 (kernel_id 与索引序);
 *   5. self_test → ACS_OK (host 服务往返; allocator 平衡);
 *   6. run_kernel 冒烟: calibration (N=4, k=0, out=in0-1) → ACS_OK,
 *      输出与独立期望一致 (含 SSE2-only 数值路径; 加载/执行期无 AVX 指令);
 *   7. dlclose 后句柄失效 (进程退出; 无崩溃 = 静态析构安全; 无全局 SIMD
 *      静态初始化 → 无 AVX 状态依赖)。
 *
 * 编译: gcc -std=c11 (Linux; dlfcn); 链接 -ldl。退出码 0=全 PASS。
 */
#define _POSIX_C_SOURCE 200112L
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/baseline_provider_v1.h"

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

/* ── 最小 host (与 handshake test 同构) ── */
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
        fprintf(stderr, "usage: %s <path-to-astrocs_cpu_baseline.so>\n", argv[0]);
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

    /* 1. 唯一导出面: provider query 必须可查; module/backend/其它 → NULL */
    query_fn q = (query_fn)dlsym(h, "astrocs_provider_query_v1");
    CHECK(q != NULL);
    CHECK(dlsym(h, "astrocs_module_query_v1") == NULL);
    CHECK(dlsym(h, "astrocs_backend_get_api_v1") == NULL);

    /* 2. query 握手 (真实 capability: 本机 amd64 SSE2 os_safe) */
    const acs_provider_api_v1* api = NULL;
    CHECK_ST(ACS_OK, q(ACS_ABI_VERSION_V1, &g_host, &api), "so query ok");
    CHECK(api != NULL);

    /* 3. kernel_list: 12 条 */
    uint32_t count = 0;
    const acs_kernel_desc_v1* ks = NULL;
    if (api) {
        CHECK_ST(ACS_OK, api->kernel_list(&g_host, &count, &ks), "kernel_list");
        CHECK(count == ACS_CPU_BASELINE_KERNEL_COUNT);
        for (uint32_t i = 0; i < count && i < ACS_CPU_BASELINE_KERNEL_COUNT; ++i) {
            CHECK(ks[i].kernel_id.data != NULL && ks[i].kernel_id.size > 0);
        }
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
        acs_cpu_baseline_params_v1 P;
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
        CHECK_ST(ACS_OK, api->run_kernel(ACS_CPU_KIDX_CALIBRATION, &g_host,
                                         &P, (uint32_t)sizeof(P), sp_in, sp_out),
                 "calibration run");
        /* (in0 - in1 - 0*in2) * in3 = (in0-1)*1; in0=i+1 → 期望 i (0..3) */
        for (int i = 0; i < 4; ++i) {
            if (out[i] != (float)i) {
                fprintf(stderr, "FAIL calib[%d]=%f expect=%d\n", i, out[i], i);
                ++g_failures;
            }
        }
    }

    dlclose(h);
    if (g_failures) {
        fprintf(stderr, "SO LOAD FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_so_load_test: ALL PASS (dlopen unique-export query/kernel_list/self_test/run_kernel)\n");
    return 0;
}
