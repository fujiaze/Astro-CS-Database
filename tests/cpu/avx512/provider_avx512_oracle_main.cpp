// tests/cpu/avx512/provider_avx512_oracle_main.cpp (CPU-004)
//
// AVX-512 provider 三路对照 oracle 运行器: dlopen 三个独立 provider .so
// (baseline 无 -mavx*; avx2 以 -mavx2 -mfma 单独编译; avx512 以
// -mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl 单独编译),
// 按 kernel_id 在各自 provider 表查询函数入口 (CPU-004 "函数入口由
// provider 表查询, 不复制科学模块"), 对 AVX-512 注册的唯一热点
// hips-bulk-transform (ALG-P3-002; ISA-004 实测 avx512 +29.5% vs baseline,
// ≈avx2 +28.3% 同档, 唯一 SHIP):
//   1) avx512 双跑 budget 1 vs 4 → 输出逐位相同 (determinism bitwise;
//      每输出元素独立无跨线程归约);
//   2) worker 观测: executor stub 记录实际授予 (budget=1 → 1; budget=4 →
//      ≥2);
//   3) baseline/AVX2/AVX512 三路对照容差: avx512 vs baseline 与 avx2 vs
//      baseline 同输入输出 相对差 ≤ 2e-4 (ALG oracle 同规; baseline 已与
//      f64 独立参考一致, CPU-002);
//   4) FMA/AVX-512 归约顺序记录: 打印 avx512/avx2 相对 baseline 的
//      max-ULP 差 (逐元素 f32 bits 差; hips 无跨项/跨线程归约 → 预期小
//      ulp; 记录不设阈值);
//   5) 非注册 kernel (calibration-pixel-transform, ISA-004 实测 avx512
//      +3.8% 远低 avx2 +11.7% → NOT_SHIPPED; drizzle-accumulate −22.5% →
//      NOT_SHIPPED) 在 avx512 表查不到 → 记录 AVX512_NOT_FOUND (每 kernel
//      可退回 avx2/baseline 语义; 防 AVX-512 降频使全局性能变差)。
//
// 链接: 本 TU (dlopen 三个 .so; 不含任何 provider 源 — 避免唯一导出符号
// 冲突; 产品形态: host 分别加载 provider DLL)。
// 编译: g++ -std=c++17 -O2 (本 TU 无 -mavx*; 对照对象 .so 各自旗标)。
// 输出: 每 op 打印 AVX512A/AVX512B/AVX2/BASE hex 行 + DET/ACQ/REL/ULP;
// Python runner 判。
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/baseline_provider_v1.h"
#include "astrocs/cpu/avx2_provider_v1.h"
#include "astrocs/cpu/avx512_provider_v1.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <vector>

typedef acs_status (*query_fn)(uint32_t, const acs_host_api_v1*,
                               const acs_provider_api_v1**);

/* ── host stub ── */
static int g_alloc_balance = 0;
static void* stub_alloc(void* ud, uint64_t size, uint64_t align) {
    (void)ud;
    void* p = nullptr;
    if (align <= sizeof(void*)) p = malloc((size_t)size);
    else if (posix_memalign(&p, (size_t)align, (size_t)size) != 0) p = nullptr;
    if (p) g_alloc_balance += 1;
    return p;
}
static void stub_free(void* ud, void* p) {
    (void)ud;
    if (p) g_alloc_balance -= 1;
    free(p);
}
struct BudgetStub {
    uint32_t max_budget;
    uint32_t granted;
    uint32_t last_attempt;
};
static int stub_acquire(void* ud, uint32_t n) {
    auto* b = static_cast<BudgetStub*>(ud);
    b->last_attempt = n;
    if (n > b->max_budget) return 1;
    b->granted = n;
    return 0;
}
static void stub_release(void* ud, uint32_t n) { (void)ud; (void)n; }
static int stub_cancel(void* ud) { (void)ud; return 0; }
static void stub_log(void* ud, int level, acs_str_v1 c, acs_str_v1 m) {
    (void)ud; (void)level; (void)c; (void)m;
}
static void make_host(acs_host_api_v1* host, BudgetStub* bs) {
    static acs_allocator_v1 alloc;
    static acs_executor_v1 exec;
    static acs_cancel_v1 cancel;
    static acs_logger_v1 logger;
    alloc = { { (uint32_t)sizeof(acs_allocator_v1), ACS_ABI_VERSION_V1 },
              stub_alloc, stub_free, nullptr };
    exec = { { (uint32_t)sizeof(acs_executor_v1), ACS_ABI_VERSION_V1 },
             4u, 4u, stub_acquire, stub_release, bs };
    cancel = { { (uint32_t)sizeof(acs_cancel_v1), ACS_ABI_VERSION_V1 },
               stub_cancel, nullptr };
    logger = { { (uint32_t)sizeof(acs_logger_v1), ACS_ABI_VERSION_V1 },
               stub_log, nullptr };
    std::memset(host, 0, sizeof(*host));
    host->head.struct_size = (uint32_t)sizeof(acs_host_api_v1);
    host->head.abi_version = ACS_ABI_VERSION_V1;
    host->allocator = &alloc;
    host->executor = &exec;
    host->cancel = &cancel;
    host->logger = &logger;
}

/* 固定 LCG (与 CPU-002/003 oracle 同式; 输入确定性) */
static uint32_t g_lcg = 42u;
static float lcg_f() {
    g_lcg = g_lcg * 1664525u + 1013904223u;
    return static_cast<float>(g_lcg % 2000u) / 100.0f;
}

static void print_span(const char* tag, const float* v, size_t n) {
    std::printf("%s", tag);
    for (size_t i = 0; i < n; ++i) {
        uint32_t bits;
        std::memcpy(&bits, &v[i], 4);
        std::printf(" %08x", bits);
    }
    std::printf("\n");
}

/* kernel_id → 索引 (provider 表查询; 查不到返回 UINT32_MAX) */
static uint32_t find_kernel(const acs_provider_api_v1* api,
                            const acs_host_api_v1* host, const char* id) {
    uint32_t count = 0;
    const acs_kernel_desc_v1* ks = nullptr;
    if (api->kernel_list(host, &count, &ks) != ACS_OK || ks == nullptr)
        return UINT32_MAX;
    for (uint32_t i = 0; i < count; ++i) {
        if (ks[i].kernel_id.size == strlen(id) &&
            memcmp(ks[i].kernel_id.data, id, ks[i].kernel_id.size) == 0)
            return i;
    }
    return UINT32_MAX;
}

/* float 相对差 |a-b|/max(1,|b|) */
static float rel_diff(float a, float b) {
    const float denom = std::fabs(b) > 1.0f ? std::fabs(b) : 1.0f;
    return std::fabs(a - b) / denom;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: %s <baseline.so> <avx2.so> <avx512.so>\n",
                     argv[0]);
        return 2;
    }
    void* hb = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    void* h2 = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    void* h5 = dlopen(argv[3], RTLD_NOW | RTLD_LOCAL);
    if (!hb || !h2 || !h5) {
        std::fprintf(stderr, "dlopen fail: %s / %s / %s\n",
                     hb ? "" : dlerror(), h2 ? "" : dlerror(),
                     h5 ? "" : dlerror());
        return 2;
    }
    auto qb = (query_fn)dlsym(hb, "astrocs_provider_query_v1");
    auto q2 = (query_fn)dlsym(h2, "astrocs_provider_query_v1");
    auto q5 = (query_fn)dlsym(h5, "astrocs_provider_query_v1");
    if (!qb || !q2 || !q5) return 2;

    acs_host_api_v1 host;
    BudgetStub bs{ 4, 0, 0 };
    make_host(&host, &bs);
    const acs_provider_api_v1* apib = nullptr;
    const acs_provider_api_v1* apia = nullptr;
    const acs_provider_api_v1* apx = nullptr;
    if (qb(ACS_ABI_VERSION_V1, &host, &apib) != ACS_OK ||
        q2(ACS_ABI_VERSION_V1, &host, &apia) != ACS_OK ||
        q5(ACS_ABI_VERSION_V1, &host, &apx) != ACS_OK ||
        !apib || !apia || !apx) {
        std::printf("QUERY_FAIL\n");
        return 3;
    }

    /* 非注册 kernel 回落演示: calibration-pixel-transform (ISA-004 avx512
     * +3.8% 远低 avx2 +11.7% → NOT_SHIPPED) 在 avx512 表查不到, 在
     * avx2/baseline 表查到 → 每 kernel 可退回语义 */
    const uint32_t cal_x = find_kernel(apx, &host, "calibration-pixel-transform");
    const uint32_t cal_a = find_kernel(apia, &host, "calibration-pixel-transform");
    const uint32_t cal_b = find_kernel(apib, &host, "calibration-pixel-transform");
    std::printf("NONHOT_AVX512 %s\n",
                cal_x == UINT32_MAX ? "NOT_FOUND" : "FOUND");
    std::printf("NONHOT_AVX2  %s\n",
                cal_a == UINT32_MAX ? "NOT_FOUND" : "FOUND");
    std::printf("NONHOT_BASE  %s\n",
                cal_b == UINT32_MAX ? "NOT_FOUND" : "FOUND");

    /* AVX-512 唯一注册热点用例 (kernel_id; 各自 provider 表内查询) */
    struct HotCase {
        const char* kid;   /* kernel_id (provider 表查询键) */
        float k;
        uint32_t aux0, aux1;   /* iw, ih (源尺寸) */
        uint32_t w, h;         /* 目标输出域; N=w*h */
    };
    const HotCase cases[] = {
        { "hips-bulk-transform", 0.5f, 64, 64, 48, 48 },
        { "hips-bulk-transform", 0.37f, 40, 48, 96, 80 }
    };

    /* 注意: AVX-512 表只注册 1 个热点 kernel (hips-bulk-transform), 两个
     * 用例同名 → OP 行必须带用例序号, 否则 runner 按 kernel_id 去重会
     * 把两次运行合并 (op 数 1 != 2)。输出键唯一: "<kid>#<用例序号>"。 */
    uint32_t op_seq = 0;
    for (const HotCase& op : cases) {
        const uint32_t idx_b = find_kernel(apib, &host, op.kid);
        const uint32_t idx_a = find_kernel(apia, &host, op.kid);
        const uint32_t idx_x = find_kernel(apx, &host, op.kid);
        if (idx_b == UINT32_MAX || idx_a == UINT32_MAX ||
            idx_x == UINT32_MAX) {
            std::printf("OP %s#%u\nKERNEL_NOT_IN_TABLE\n", op.kid, op_seq);
            return 4;
        }
        const uint32_t N = op.w * op.h;
        const uint64_t src_n = (uint64_t)op.aux0 * (uint64_t)op.aux1;
        std::vector<float> src(src_n);
        for (auto& v : src) v = lcg_f();

        /* 输出: avx512 pass A/B (budget 1/4) + avx2 + baseline (budget 1) */
        std::vector<float> axA(N), axB(N), a2(N), base(N);
        uint32_t acq[2] = { 0, 0 };

        std::vector<uint8_t> in_bytes(src_n * 4);
        std::memcpy(in_bytes.data(), src.data(), src_n * 4);

        const auto run_once = [&](const acs_provider_api_v1* api, uint32_t kidx,
                                  uint32_t budget, float* out,
                                  bool record_acq) -> int {
            bs.max_budget = budget;
            bs.granted = 0; bs.last_attempt = 0;
            std::vector<uint8_t> out_bytes(N * 4, 0);
            acs_cpu_baseline_params_v1 P;
            std::memset(&P, 0, sizeof(P));
            P.head.struct_size = (uint32_t)sizeof(P);
            P.head.abi_version = ACS_ABI_VERSION_V1;
            P.w = op.w; P.h = op.h; P.k = op.k;
            P.aux0 = op.aux0; P.aux1 = op.aux1;
            P.in_off[0] = 0; P.in_len[0] = (uint64_t)src_n;
            P.out_off[0] = 0; P.out_len[0] = N;
            acs_span_u8 sp_in = ACS_SPAN_U8(in_bytes.data(), in_bytes.size());
            acs_span_u8 sp_out = ACS_SPAN_U8(out_bytes.data(), out_bytes.size());
            const acs_status rc = api->run_kernel(kidx, &host, &P, sizeof(P),
                                                  sp_in, sp_out);
            if (rc != ACS_OK) {
                std::printf("RC %d\n", (int)rc);
                return 1;
            }
            std::memcpy(out, out_bytes.data(), N * 4);
            if (record_acq) {
                if (budget == 1) acq[0] = bs.granted;
                else acq[1] = bs.granted;
            }
            return 0;
        };

        std::printf("OP %s#%u\n", op.kid, op_seq);
        ++op_seq;
        if (run_once(apx, idx_x, 1, axA.data(), true)) return 5;
        if (run_once(apx, idx_x, 4, axB.data(), true)) return 5;
        if (run_once(apia, idx_a, 1, a2.data(), false)) return 5;
        if (run_once(apib, idx_b, 1, base.data(), false)) return 5;

        print_span("AVX512A", axA.data(), N);
        print_span("AVX512B", axB.data(), N);
        print_span("AVX2", a2.data(), N);
        print_span("BASE", base.data(), N);

        /* determinism: budget 1 vs 4 逐位 */
        const bool det_ok = std::memcmp(axA.data(), axB.data(), N * 4) == 0;
        std::printf("DET %s\n", det_ok ? "OK" : "FAIL");
        std::printf("ACQ %u %u\n", acq[0], acq[1]);

        /* 对照: avx512 vs baseline / avx2 vs baseline 的 max rel diff +
         * max ulp (FMA/AVX-512 归约顺序记录) */
        float rel_x = 0.0f, rel_a = 0.0f, ulp_x = 0.0f, ulp_a = 0.0f;
        for (uint32_t i = 0; i < N; ++i) {
            const float rx = rel_diff(axA[i], base[i]);
            const float ra = rel_diff(a2[i], base[i]);
            if (rx > rel_x) rel_x = rx;
            if (ra > rel_a) rel_a = ra;
            uint32_t bx, ba, bb;
            std::memcpy(&bx, &axA[i], 4);
            std::memcpy(&ba, &a2[i], 4);
            std::memcpy(&bb, &base[i], 4);
            const float ux = (float)std::abs((int64_t)bx - (int64_t)bb);
            const float ua = (float)std::abs((int64_t)ba - (int64_t)bb);
            if (ux > ulp_x) ulp_x = ux;
            if (ua > ulp_a) ulp_a = ua;
        }
        std::printf("REL512_MAX %.9g\n", (double)rel_x);
        std::printf("REL2_MAX   %.9g\n", (double)rel_a);
        std::printf("ULP512_MAX %.1f\n", (double)ulp_x);
        std::printf("ULP2_MAX   %.1f\n", (double)ulp_a);
    }

    dlclose(h5);
    dlclose(h2);
    dlclose(hb);
    std::printf("AVX512_ORACLE_DONE alloc_balance=%d\n", g_alloc_balance);
    return 0;
}
