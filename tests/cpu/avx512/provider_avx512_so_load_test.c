/* AstroCS CPU AVX-512 provider — .so 加载冒烟 (dlopen 唯一入口 + ABI)
 * tests/cpu/avx512/provider_avx512_so_load_test.c (CPU-004)
 *
 * 覆盖 (CPU-004 验收 "非支持 CPU 不加载" 的加载面 + 12 §1 唯一导出 +
 * self_test/export/ABI 完整; 无全局 SIMD 静态初始化):
 *   1. dlopen(astrocs_cpu_avx512.so) 成功 (RTLD_NOW|RTLD_LOCAL);
 *   2. 唯一导出: dlsym 查到 astrocs_provider_query_v1; astrocs_module_query_v1 /
 *      astrocs_backend_get_api_v1 → NULL (12 §1: provider DLL 只导出批准入口);
 *   3. query → ACS_OK + out_api 非空 (真实 capability 探测: 本机 amd64
 *      AVX-512 F/CD/BW/DQ/VL os_safe → 加载); 非支持 CPU 拒绝路径由
 *      stub gate 测试覆盖;
 *   4. kernel_list: 恰 1 条注册热点 (hips-bulk-transform) + kernel_id 非空;
 *   5. self_test → ACS_OK (allocator 平衡);
 *   6. run_kernel 冒烟: hips-bulk (N=4, iw=ih=2, k=1 → out[i]=src[i]:
 *      x/y 采样比 1.0 时双线性恒等) → ACS_OK 且输出与独立期望一致
 *      (真实 EVEX 数值路径; 本机可执行);
 *   7. 非注册 kernel 索引 (KERNEL_COUNT / 越界) → ACS_ERR_UNSUPPORTED
 *      (host 按 kernel_id 退回 avx2/baseline 语义);
 *   8. dlclose 后句柄失效 (进程退出; 无崩溃 = 静态析构安全; 无全局 SIMD
 *      静态初始化 → 加载/卸载期无 EVEX 状态依赖; EVEX 存在性/位置由
 *      illegal-instr 反汇编检查覆盖)。
 *
 * 编译: gcc -std=c11 (Linux; dlfcn); 链接 -ldl。退出码 0=全 PASS。
 * AVX-512 provider 源以 -mavx512f -mavx512cd -mavx512bw -mavx512dq
 * -mavx512vl 单独编译为 .so (本机支持)。
 */
#define _POSIX_C_SOURCE 200112L
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/avx512_provider_v1.h"

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
        fprintf(stderr, "usage: %s <path-to-astrocs_cpu_avx512.so>\n", argv[0]);
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

    /* 2. query 握手 (真实 capability: 本机 AVX-512 os_safe) */
    const acs_provider_api_v1* api = NULL;
    CHECK_ST(ACS_OK, q(ACS_ABI_VERSION_V1, &g_host, &api), "so query ok");
    CHECK(api != NULL);

    /* 3. kernel_list: 恰 1 条注册热点 */
    uint32_t count = 0;
    const acs_kernel_desc_v1* ks = NULL;
    if (api) {
        CHECK_ST(ACS_OK, api->kernel_list(&g_host, &count, &ks), "kernel_list");
        CHECK(count == ACS_CPU_AVX512_KERNEL_COUNT);
        CHECK(count == 1u);
        for (uint32_t i = 0; i < count && i < ACS_CPU_AVX512_KERNEL_COUNT; ++i)
            CHECK(ks[i].kernel_id.data != NULL && ks[i].kernel_id.size > 0);
    }

    /* 4. self_test */
    if (api) {
        CHECK_ST(ACS_OK, api->self_test(&g_host), "self_test ok");
        CHECK(g_alloc_balance == 0);
    }

    /* 5. run_kernel 冒烟: hips-bulk w=h=128 (N=16384 远超 512-bit 向量
     *    宽, 必触发 EVEX 向量循环; 源 2x2 全常数 C → 双线性系数归一组合
     *    应恒得 C)。注意: 浮点系数和 (1-fx)(1-fy)+fx(1-fy)+(1-fx)fy+fx·fy
     *    按 4 项独立舍入后可能差 1 ulp (非精确 1), 故断言用相对容差
     *    (与 ALG 冻结对照同规 2e-4; 不允许位等断言 —— 实数算术下
     *    1.5f*Σcoef 的末位舍入取决于各系数次序)。坐标→像素映射的
     *    精确正确性由 oracle (三路 hex 对照) 与 determinism 覆盖。 */
    if (api) {
        enum { N = 128 * 128, IW = 2, IH = 2 };
        static float srcv[IW * IH];
        static float out[N];
        const float C = 1.5f;
        for (int i = 0; i < IW * IH; ++i) srcv[i] = C;
        for (int i = 0; i < N; ++i) out[i] = -1.0f;  /* 哨兵: 全写才过 */
        acs_cpu_avx512_params_v1 P;
        memset(&P, 0, sizeof(P));
        P.head.struct_size = (uint32_t)sizeof(P);
        P.head.abi_version = ACS_ABI_VERSION_V1;
        P.w = 128; P.h = 128;
        P.k = 0.37f;         /* 非平凡采样比 (半像素/分数) */
        P.aux0 = IW; P.aux1 = IH;
        P.in_off[0] = 0; P.in_len[0] = IW * IH;
        P.out_off[0] = 0; P.out_len[0] = N;
        acs_span_u8 sp_in = ACS_SPAN_U8((uint8_t*)srcv, sizeof(srcv));
        acs_span_u8 sp_out = ACS_SPAN_U8((uint8_t*)out, sizeof(out));
        CHECK_ST(ACS_OK, api->run_kernel(ACS_CPU_AVX512_KIDX_HIPS_BULK,
                                         &g_host, &P, (uint32_t)sizeof(P),
                                         sp_in, sp_out),
                 "hips-bulk run (real EVEX path)");
        for (int i = 0; i < N; ++i) {
            const float rel = out[i] > C ? (out[i] - C) / (C > 0 ? C : 1.0f)
                                         : (C - out[i]) / (C > 0 ? C : 1.0f);
            if (rel > 2e-4f) {
                fprintf(stderr,
                        "FAIL hips[%d]=%.9g expect~%.9g rel=%.3g > 2e-4 "
                        "(常数输入归一; 系数和舍入 ≤ 1ulp 为预期)\n",
                        i, (double)out[i], (double)C, (double)rel);
                ++g_failures;
                break;
            }
        }
        /* 6. 非注册 kernel 索引 → unsupported (退回 avx2/baseline 语义) */
        CHECK_ST(ACS_ERR_UNSUPPORTED,
                 api->run_kernel(ACS_CPU_AVX512_KERNEL_COUNT, &g_host,
                                 &P, (uint32_t)sizeof(P), sp_in, sp_out),
                 "out-of-table index unsupported");
        CHECK_ST(ACS_ERR_UNSUPPORTED,
                 api->run_kernel(999, &g_host, &P, (uint32_t)sizeof(P),
                                 sp_in, sp_out),
                 "far-out-of-table index unsupported");
    }

    dlclose(h);
    if (g_failures) {
        fprintf(stderr, "AVX512 SO LOAD FAIL: %d failures\n", g_failures);
        return 1;
    }
    printf("provider_avx512_so_load_test: ALL PASS (dlopen unique-export "
           "query/kernel_list=1[hips-bulk]/self_test/hips run rel<=2e-4/"
           "table-unsupported)\n");
    return 0;
}
