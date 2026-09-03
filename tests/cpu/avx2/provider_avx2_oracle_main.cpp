// tests/cpu/avx2/provider_avx2_oracle_main.cpp (CPU-003)
//
// AVX2/FMA provider 对照 oracle 运行器: dlopen 两个独立 provider .so
// (baseline 无 -mavx*; avx2 以 -mavx2 -mfma 单独编译), 按 kernel_id 在
// 各自 provider 表查询函数入口 (CPU-003 "函数入口由 provider 表查询"),
// 对 profile 指定热点 (calibration-pixel-transform / hips-bulk-transform):
//   1) avx2 双跑 budget 1 vs 4 → 输出逐位相同 (determinism bitwise;
//      每输出元素独立无跨线程归约);
//   2) worker 观测: executor stub 记录实际授予 (budget=1 → 1; budget=4 →
//      ≥2);
//   3) baseline 对照容差: avx2 vs baseline 同输入输出 相对差 ≤ 2e-4
//      (ALG oracle 同规; baseline 已与 f64 独立参考一致, CPU-002);
//   4) FMA 归约顺序记录: 打印 avx2 相对 baseline 的 max-ULP 差 (逐元素
//      f32 bits 差; 两 kernel 均无跨项/跨线程归约 → 预期小 ulp);
//   5) 非热点 kernel (如 noise-snr-reductions) 在 avx2 表查不到 →
//      记录 AVX2_NOT_FOUND (回落 baseline 语义演示)。
//
// 链接: 本 TU (dlopen 两个 .so; 不含任何 provider 源 — 避免唯一导出符号
// 冲突; 产品形态: host 分别加载 provider DLL)。
// 编译: g++ -std=c++17 -O2 (本 TU 无 -mavx*; 对照对象 .so 各自旗标)。
// 输出: 每 op 打印 AVX2A/AVX2B/BASE hex 行 + DET/ACQ/ULP; Python runner 判。
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/baseline_provider_v1.h"
#include "astrocs/cpu/avx2_provider_v1.h"

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

/* 固定 LCG (与 CPU-002 oracle 同式; 输入确定性) */
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
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: %s <baseline.so> <avx2.so>\n", argv[0]);
        return 2;
    }
    void* hb = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    void* ha = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!hb || !ha) {
        std::fprintf(stderr, "dlopen fail: %s / %s\n",
                     hb ? "" : dlerror(), ha ? "" : dlerror());
        return 2;
    }
    auto qb = (query_fn)dlsym(hb, "astrocs_provider_query_v1");
    auto qa = (query_fn)dlsym(ha, "astrocs_provider_query_v1");
    if (!qb || !qa) return 2;

    acs_host_api_v1 host;
    BudgetStub bs{ 4, 0, 0 };
    make_host(&host, &bs);
    const acs_provider_api_v1* apib = nullptr;
    const acs_provider_api_v1* apia = nullptr;
    if (qb(ACS_ABI_VERSION_V1, &host, &apib) != ACS_OK ||
        qa(ACS_ABI_VERSION_V1, &host, &apia) != ACS_OK || !apib || !apia) {
        std::printf("QUERY_FAIL\n");
        return 3;
    }

    /* 非热点 kernel 回落演示: noise-snr-reductions 在 avx2 表查不到 */
    const uint32_t noise_a = find_kernel(apia, &host, "noise-snr-reductions");
    const uint32_t noise_b = find_kernel(apib, &host, "noise-snr-reductions");
    std::printf("NONHOT_AVX2 %s\n", noise_a == UINT32_MAX ? "NOT_FOUND" : "FOUND");
    std::printf("NONHOT_BASE  %s\n", noise_b == UINT32_MAX ? "NOT_FOUND" : "FOUND");

    /* 两个热点 op 用例 (kernel_id; 各自 provider 表内查询) */
    struct HotCase {
        const char* kid;   /* kernel_id (provider 表查询键) */
        float k;
        uint32_t aux0, aux1;
        uint32_t w, h;
        uint32_t in0_n, in1_n, in2_n, in3_n;
        bool has_out1;
    };
    const HotCase cases[] = {
        { "calibration-pixel-transform", 1.25f, 0, 0, 64, 64,
          4096, 4096, 4096, 4096, false },
        { "hips-bulk-transform", 0.5f, 64, 64, 48, 48,
          4096, 0, 0, 0, false }
    };

    for (const HotCase& op : cases) {
        const uint32_t idx_b = find_kernel(apib, &host, op.kid);
        const uint32_t idx_a = find_kernel(apia, &host, op.kid);
        if (idx_b == UINT32_MAX || idx_a == UINT32_MAX) {
            std::printf("OP %s\nKERNEL_NOT_IN_TABLE\n", op.kid);
            return 4;
        }
        const uint32_t N = op.w * op.h;
        std::vector<float> in0(op.in0_n), in1(op.in1_n), in2(op.in2_n),
                           in3(op.in3_n);
        for (float& v : in0) v = lcg_f();
        for (float& v : in1) v = lcg_f();
        for (float& v : in2) v = lcg_f();
        for (float& v : in3) v = lcg_f();

        /* 输出: avx2 pass A/B (budget 1/4) + baseline (budget 1) */
        std::vector<float> av2A(N), av2B(N), base(N);
        uint32_t acq[2] = { 0, 0 };

        /* 组装连续输入缓冲 */
        std::vector<uint8_t> in_bytes;
        std::vector<uint64_t> in_off(4, 0), in_len(4, 0);
        const std::vector<float>* iv[4] = { &in0, &in1, &in2, &in3 };
        uint64_t cur = 0;
        for (int s = 0; s < 4; ++s) {
            const size_t n = iv[s]->size();
            if (n) {
                in_off[s] = cur;
                in_len[s] = (uint64_t)n;
                if (in_bytes.size() < (cur + n) * 4)
                    in_bytes.resize((cur + n) * 4);
                std::memcpy(in_bytes.data() + cur * 4, iv[s]->data(), n * 4);
                cur += n;
            } else {
                in_off[s] = 0; in_len[s] = 0;
            }
        }

        const auto run_once = [&](const acs_provider_api_v1* api, uint32_t kidx,
                                  uint32_t budget, float* out) -> int {
            bs.max_budget = budget;
            bs.granted = 0; bs.last_attempt = 0;
            std::vector<uint8_t> out_bytes(N * 4, 0);
            acs_cpu_baseline_params_v1 P;
            std::memset(&P, 0, sizeof(P));
            P.head.struct_size = (uint32_t)sizeof(P);
            P.head.abi_version = ACS_ABI_VERSION_V1;
            P.w = op.w; P.h = op.h; P.k = op.k;
            P.aux0 = op.aux0; P.aux1 = op.aux1;
            for (int s = 0; s < 4; ++s) {
                P.in_off[s] = in_off[s]; P.in_len[s] = in_len[s];
            }
            P.out_off[0] = 0; P.out_len[0] = N;
            if (op.has_out1) { P.out_off[1] = N; P.out_len[1] = N; }
            acs_span_u8 sp_in = ACS_SPAN_U8(in_bytes.data(), in_bytes.size());
            acs_span_u8 sp_out = ACS_SPAN_U8(out_bytes.data(), out_bytes.size());
            const acs_status rc = api->run_kernel(kidx, &host, &P, sizeof(P),
                                                  sp_in, sp_out);
            if (rc != ACS_OK) {
                std::printf("RC %d\n", (int)rc);
                return 1;
            }
            std::memcpy(out, out_bytes.data(), N * 4);
            if (budget == 1) acq[0] = bs.granted;
            else acq[1] = bs.granted;
            return 0;
        };

        std::printf("OP %s\n", op.kid);
        if (run_once(apia, idx_a, 1, av2A.data())) return 5;
        if (run_once(apia, idx_a, 4, av2B.data())) return 5;
        if (run_once(apib, idx_b, 1, base.data())) return 5;

        print_span("AVX2A", av2A.data(), N);
        print_span("AVX2B", av2B.data(), N);
        print_span("BASE", base.data(), N);

        /* determinism: budget 1 vs 4 逐位 */
        const bool det_ok = std::memcmp(av2A.data(), av2B.data(), N * 4) == 0;
        std::printf("DET %s\n", det_ok ? "OK" : "FAIL");
        std::printf("ACQ %u %u\n", acq[0], acq[1]);

        /* baseline 对照: max rel diff + max ulp (FMA 归约顺序记录) */
        float max_rel = 0.0f, max_ulp = 0.0f;
        for (uint32_t i = 0; i < N; ++i) {
            const float r = rel_diff(av2A[i], base[i]);
            if (r > max_rel) max_rel = r;
            uint32_t ba, bb;
            std::memcpy(&ba, &av2A[i], 4);
            std::memcpy(&bb, &base[i], 4);
            const float u = (float)std::abs((int64_t)ba - (int64_t)bb);
            if (u > max_ulp) max_ulp = u;
        }
        std::printf("REL_MAX %.9g\n", (double)max_rel);
        std::printf("ULP_MAX %.1f\n", (double)max_ulp);
    }

    dlclose(ha);
    dlclose(hb);
    std::printf("AVX2_ORACLE_DONE alloc_balance=%d\n", g_alloc_balance);
    return 0;
}
