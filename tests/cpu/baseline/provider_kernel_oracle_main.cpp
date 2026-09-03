// tests/cpu/baseline/provider_kernel_oracle_main.cpp (CPU-002)
//
// 科学 oracle 运行器: 12 个注册 kernel × 固定合成数据 × budget 1 vs 4 双跑。
//   1) Oracle: 输入以 hex(float 位型) 输出 → Python 独立参考实现 (f64 运算,
//      容差 2e-4 相对) 比对 (test_abi_kernels 同规);
//   2) 确定性: budget 1 与 budget 4 输出逐位相同 (输出元素独立, 无跨线程
//      归约, ARCH-004 §4; 1/N worker tests);
//   3) worker 观测: executor stub 记录实际租借授予 (budget=1 → 1;
//      budget=4 → ≥2, 本机单 CPU 除外由 runner 判定)。
//
// 链接: 本 TU + baseline_provider.cpp + capability_detect.c (真实探测)。
// 编译: g++ -std=c++17 -O2 (无 -mavx*); 输出退出码 0=运行完成 (判定在 Python)。
#include "astrocs/abi/module_api_v1.h"
#include "astrocs/cpu/baseline_provider_v1.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <vector>

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
    uint32_t max_budget;   /* 本次允许最大 worker 租借 */
    uint32_t granted;      /* 最近一次成功租借 */
    uint32_t last_attempt;
};
static int stub_acquire(void* ud, uint32_t n) {
    auto* b = static_cast<BudgetStub*>(ud);
    b->last_attempt = n;
    if (n > b->max_budget) return 1;   /* 预算不足 */
    b->granted = n;
    return 0;
}
static void stub_release(void* ud, uint32_t n) {
    (void)ud; (void)n;
}
static int stub_cancel(void* ud) { (void)ud; return 0; }
static void stub_log(void* ud, int level, acs_str_v1 c, acs_str_v1 m) {
    (void)ud; (void)level; (void)c; (void)m;
}

/* 构造 host (预算 stub 持 4; acquire 时按 budget 拒绝) */
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

/* 固定 LCG (legacy oracle 同式; 输入确定性) */
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

/* 一个 op 的输入/输出布局与执行 */
struct OpCase {
    const char* name;
    uint32_t kidx;
    float k;
    uint32_t aux0, aux1;
};

int main() {
    const uint32_t W = 4, H = 2, N = W * H, FR = 3;
    const OpCase ops[] = {
        { "calibration", ACS_CPU_KIDX_CALIBRATION, 2.0f, 0, 0 },
        { "noise", ACS_CPU_KIDX_NOISE_REDUCTIONS, 0.0f, FR, 0 },
        { "psf", ACS_CPU_KIDX_PSF_BATCH, 5.0f, 0, 0 },
        { "driz_overlap", ACS_CPU_KIDX_DRIZZLE_OVERLAP, 0.0f, 0, 0 },
        { "driz_accum", ACS_CPU_KIDX_DRIZZLE_ACCUMULATE, 0.0f, FR, 0 },
        { "driz_norm", ACS_CPU_KIDX_DRIZZLE_NORMALIZE, 0.0f, 0, 0 },
        { "spmv", ACS_CPU_KIDX_UPM_SPMV, 0.0f, 6, 4 },
        { "residual", ACS_CPU_KIDX_UPM_RESIDUAL, 0.0f, 0, 0 },
        { "weight_upd", ACS_CPU_KIDX_UPM_WEIGHT_UPDATE, 0.25f, 0, 0 },
        { "rejection", ACS_CPU_KIDX_REJECTION_STATS, 3.0f, FR, 0 },
        { "integration", ACS_CPU_KIDX_INTEGRATION_ACCUM, 0.0f, FR, 0 },
        { "hips", ACS_CPU_KIDX_HIPS_BULK, 0.5f, 4, 3 }
    };

    acs_host_api_v1 host;
    BudgetStub bs{ 4, 0, 0 };
    make_host(&host, &bs);

    const acs_provider_api_v1* api = nullptr;
    if (astrocs_provider_query_v1(ACS_ABI_VERSION_V1, &host, &api) != ACS_OK) {
        std::printf("QUERY_FAIL\n");
        return 2;
    }
    uint32_t kcount = 0;
    const acs_kernel_desc_v1* kdesc = nullptr;
    if (api->kernel_list(&host, &kcount, &kdesc) != ACS_OK || kcount != 12) {
        std::printf("KERNEL_COUNT %u\n", kcount);
        return 2;
    }

    for (const OpCase& op : ops) {
        /* 输入元素布局 (槽位连续: 全部从 0 偏移的连续大缓冲) */
        uint64_t in0_n = N, in1_n = N, in2_n = N, in3_n = N;
        bool has_out1 = false;
        switch (op.kidx) {
        case ACS_CPU_KIDX_NOISE_REDUCTIONS:
        case ACS_CPU_KIDX_REJECTION_STATS:
        case ACS_CPU_KIDX_DRIZZLE_ACCUMULATE:
        case ACS_CPU_KIDX_INTEGRATION_ACCUM:
            in0_n = (uint64_t)N * op.aux0; in1_n = (uint64_t)N * op.aux0;
            in2_n = 0; in3_n = 0;
            has_out1 = (op.kidx == ACS_CPU_KIDX_NOISE_REDUCTIONS);
            break;
        case ACS_CPU_KIDX_PSF_BATCH: in0_n = 2; in1_n = 0; in2_n = 0; in3_n = 0; break;
        case ACS_CPU_KIDX_DRIZZLE_OVERLAP: in2_n = 0; in3_n = 0; break;
        case ACS_CPU_KIDX_DRIZZLE_NORMALIZE:
        case ACS_CPU_KIDX_UPM_RESIDUAL: in2_n = 0; in3_n = 0; break;
        case ACS_CPU_KIDX_UPM_WEIGHT_UPDATE: in1_n = 0; in2_n = 0; in3_n = 0; break;
        case ACS_CPU_KIDX_UPM_SPMV:
            in0_n = op.aux0; in1_n = op.aux0; in2_n = (uint64_t)N + 1; in3_n = op.aux1;
            break;
        case ACS_CPU_KIDX_HIPS_BULK:
            in0_n = (uint64_t)op.aux0 * op.aux1; in1_n = 0; in2_n = 0; in3_n = 0;
            break;
        default: break;
        }
        /* 连续输入缓冲 (槽位偏移即元素偏移; len 由 op 决定) */
        std::vector<float> in0(in0_n), in1(in1_n), in2(in2_n), in3(in3_n);
        for (float& v : in0) v = lcg_f();
        for (float& v : in1) v = lcg_f();
        for (float& v : in2) v = lcg_f();
        for (float& v : in3) v = lcg_f();
        if (op.kidx == ACS_CPU_KIDX_UPM_SPMV) {
            for (uint32_t k = 0; k < op.aux0; ++k)      /* col ∈ [0,ncols) 整数 */
                in1[k] = static_cast<float>(k % op.aux1);
            in2[0] = 0.0f;
            for (uint32_t r = 1; r <= N; ++r)           /* 行指针单调非减 ≤ nnz */
                in2[r] = static_cast<float>(
                    (static_cast<uint32_t>(in2[r - 1]) + 1u) % (op.aux0 + 1));
            in2[N] = static_cast<float>(op.aux0);
            /* 保证覆盖到 nnz: 单调 +1 可能停在 nnz; 重置单调数列使末值=nnz */
            for (uint32_t r = 1; r <= N; ++r)
                in2[r] = static_cast<float>(
                    (uint32_t)((uint64_t)op.aux0 * r) / N);
            in2[N] = static_cast<float>(op.aux0);
        }
        std::printf("OP %s\n", op.name);
        print_span("IN0", in0.data(), in0.size());
        if (in1_n) print_span("IN1", in1.data(), in1.size());
        if (in2_n) print_span("IN2", in2.data(), in2.size());
        if (in3_n) print_span("IN3", in3.data(), in3.size());

        /* 输出缓冲 (out0 N; out1 N if has_out1) */
        std::vector<float> outA0(N), outB0(N), outA1(N), outB1(N);
        uint32_t observed_granted[2] = { 0, 0 };

        for (int pass = 0; pass < 2; ++pass) {
            bs.max_budget = (pass == 0) ? 1u : 4u;
            bs.granted = 0; bs.last_attempt = 0;

            std::vector<uint8_t> in_bytes, out_bytes;
            const auto push_in = [&](const std::vector<float>& v, uint64_t off) {
                if (v.empty()) return off;
                if (in_bytes.size() < (off + v.size()) * 4)
                    in_bytes.resize((off + v.size()) * 4);
                std::memcpy(in_bytes.data() + off * 4, v.data(), v.size() * 4);
                return off + v.size();
            };
            uint64_t o0 = push_in(in0, 0);
            uint64_t o1 = push_in(in1, o0);
            uint64_t o2 = push_in(in2, o1);
            (void)push_in(in3, o2);

            std::vector<float>& out0 = (pass == 0) ? outA0 : outB0;
            std::vector<float>& out1 = (pass == 0) ? outA1 : outB1;
            out_bytes.resize((N + (has_out1 ? N : 0)) * 4);
            std::memset(out_bytes.data(), 0, out_bytes.size());

            acs_cpu_baseline_params_v1 P;
            std::memset(&P, 0, sizeof(P));
            P.head.struct_size = (uint32_t)sizeof(P);
            P.head.abi_version = ACS_ABI_VERSION_V1;
            P.w = W; P.h = H; P.k = op.k; P.aux0 = op.aux0; P.aux1 = op.aux1;
            /* 槽位: 连续缓冲; 未用槽 (len=0) 必须 off=0 (合同) */
            P.in_off[0] = 0; P.in_len[0] = in0_n;
            if (in1_n) { P.in_off[1] = o0; P.in_len[1] = in1_n; }
            if (in2_n) { P.in_off[2] = o1; P.in_len[2] = in2_n; }
            if (in3_n) { P.in_off[3] = o2; P.in_len[3] = in3_n; }
            P.out_off[0] = 0; P.out_len[0] = N;
            if (has_out1) { P.out_off[1] = N; P.out_len[1] = N; }

            acs_span_u8 sp_in = ACS_SPAN_U8(in_bytes.data(), in_bytes.size());
            acs_span_u8 sp_out = ACS_SPAN_U8(out_bytes.data(), out_bytes.size());
            const acs_status rc = api->run_kernel(op.kidx, &host, &P, sizeof(P),
                                                  sp_in, sp_out);
            if (rc != ACS_OK) {
                std::printf("RC %d\n", (int)rc);
                return 3;
            }
            std::memcpy(out0.data(), out_bytes.data(), N * 4);
            if (has_out1)
                std::memcpy(out1.data(), out_bytes.data() + N * 4, N * 4);
            observed_granted[pass] = bs.granted;
        }
        print_span("OUTA", outA0.data(), N);
        if (has_out1) print_span("OUT1A", outA1.data(), N);
        print_span("OUTB", outB0.data(), N);
        if (has_out1) print_span("OUT1B", outB1.data(), N);
        std::printf("DET %s\n",
                    std::memcmp(outA0.data(), outB0.data(), N * 4) == 0 &&
                            (!has_out1 ||
                             std::memcmp(outA1.data(), outB1.data(), N * 4) == 0)
                        ? "OK" : "FAIL");
        std::printf("ACQ %u %u\n", observed_granted[0], observed_granted[1]);
    }
    std::printf("ORACLE_RUNNER_DONE alloc_balance=%d\n", g_alloc_balance);
    return 0;
}
