// tests/backend/kernel_oracle_main.cpp — ABI-003 Oracle 运行器
// 每个生产 kernel op 以固定合成数据跑两遍(budget=1 与 budget=4):
//   DET=OK  ⇔ 输出逐位相同(确定性不随 worker 数变化, ARCH-004 §4);
//   WORKERS 观测: budget=4 时 workers_used≥2(多线程)。
// 输入以 hex(float 位型)打印 → Python 独立参考实现比对(Oracle)。
#include <cstdio>
#include <cstring>
#include <vector>

#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
int astrocs_backend_get_api_v1(uint32_t, uint32_t, const astrocs_host_services_v1*,
                               astrocs_backend_api_v1*);
}

namespace {

uint32_t lcg_state = 42u;
float lcg_f() {  // [0, 20) 固定序列
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

void print_span(const char* tag, const std::vector<float>& v) {
    std::printf("%s", tag);
    for (float x : v) {
        uint32_t bits;
        std::memcpy(&bits, &x, 4);
        std::printf(" %08x", bits);
    }
    std::printf("\n");
}

struct Buffers {
    std::vector<float> in0, in1, in2, in3, out1;
};

}  // namespace

int main() {
    astrocs_host_services_v1 host;
    void* state = nullptr;
    if (astrocs_host_services_default_v1(&host, &state) != ACS_OK) return 2;

    astrocs_backend_api_v1 api;
    std::memset(&api, 0, sizeof(api));
    if (astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1),
                                   &host, &api) != ACS_OK) return 2;
    if (api.kernel_count != 12) { std::printf("KERNEL_COUNT %u\n", api.kernel_count); return 2; }

    const uint32_t W = 4, H = 2, N = W * H, FR = 3;
    const struct { uint32_t op; const char* name; float k; uint32_t aux0, aux1; } ops[] = {
        {ACS_KOP_CALIBRATION, "calibration", 2.0f, 0, 0},
        {ACS_KOP_NOISE_REDUCTIONS, "noise", 0.0f, FR, 0},
        {ACS_KOP_PSF_BATCH, "psf", 5.0f, 0, 0},
        {ACS_KOP_DRIZZLE_OVERLAP, "driz_overlap", 0.0f, 0, 0},
        {ACS_KOP_DRIZZLE_ACCUMULATE, "driz_accum", 0.0f, FR, 0},
        {ACS_KOP_DRIZZLE_NORMALIZE, "driz_norm", 0.0f, 0, 0},
        {ACS_KOP_UPM_SPMV, "spmv", 0.0f, 6, 4},
        {ACS_KOP_UPM_RESIDUAL, "residual", 0.0f, 0, 0},
        {ACS_KOP_UPM_WEIGHT_UPDATE, "weight_upd", 0.25f, 0, 0},
        {ACS_KOP_REJECTION_STATS, "rejection", 3.0f, FR, 0},
        {ACS_KOP_INTEGRATION_ACCUM, "integration", 0.0f, FR, 0},
        {ACS_KOP_HIPS_BULK, "hips", 0.5f, 4, 3},
    };

    for (const auto& op : ops) {
        Buffers b;
        uint32_t n_in0 = N, n_in1 = N, n_in2 = N, n_in3 = N;
        switch (op.op) {
        case ACS_KOP_NOISE_REDUCTIONS: case ACS_KOP_REJECTION_STATS:
        case ACS_KOP_DRIZZLE_ACCUMULATE: case ACS_KOP_INTEGRATION_ACCUM:
            n_in0 = N * op.aux0; n_in1 = N * op.aux0; n_in2 = 0; n_in3 = 0; break;
        case ACS_KOP_PSF_BATCH: n_in0 = 2; n_in1 = 0; n_in2 = 0; n_in3 = 0; break;
        case ACS_KOP_DRIZZLE_OVERLAP: n_in2 = 0; n_in3 = 0; break;
        case ACS_KOP_UPM_SPMV:
            n_in0 = op.aux0; n_in1 = op.aux0; n_in2 = N + 1; n_in3 = op.aux1; break;
        case ACS_KOP_UPM_RESIDUAL: case ACS_KOP_UPM_WEIGHT_UPDATE:
        case ACS_KOP_DRIZZLE_NORMALIZE: n_in2 = 0; n_in3 = 0; break;
        case ACS_KOP_HIPS_BULK:
            n_in0 = op.aux0 * op.aux1; n_in1 = 0; n_in2 = 0; n_in3 = 0; break;
        default: break;
        }
        auto fill = [](std::vector<float>& v, uint32_t n) {
            v.clear();
            for (uint32_t i = 0; i < n; ++i) v.push_back(lcg_f());
        };
        fill(b.in0, n_in0); fill(b.in1, n_in1); fill(b.in2, n_in2); fill(b.in3, n_in3);
        // SPMV: col 索引必须为整数域 [0,ncols); 行指针单调且末位=nnz
        if (op.op == ACS_KOP_UPM_SPMV) {
            for (uint32_t k = 0; k < op.aux0; ++k)
                b.in1[k] = static_cast<float>(k % op.aux1);
            b.in2[0] = 0;
            for (uint32_t r = 1; r <= N; ++r)
                b.in2[r] = static_cast<float>((static_cast<uint32_t>(b.in2[r - 1]) +
                                               (lcg_f() == 0 ? 0 : 1)) % (op.aux0 + 1));
            b.in2[N] = static_cast<float>(op.aux0);
        }
        std::printf("OP %s\n", op.name);
        print_span("IN0", b.in0);
        if (n_in1) print_span("IN1", b.in1);
        if (n_in2) print_span("IN2", b.in2);
        if (n_in3) print_span("IN3", b.in3);

        std::vector<float> out_a(N, 0.0f), out_b(N, 0.0f), out1(N, 0.0f);
        acs_baseline_params_v1 base;
        std::memset(&base, 0, sizeof(base));
        base.head.struct_size = sizeof(acs_baseline_params_v1);
        base.head.abi_version = ACS_ABI_VERSION_V1;
        base.op = op.op; base.w = W; base.h = H; base.k = op.k;
        base.aux0 = op.aux0; base.aux1 = op.aux1;
        base.in0 = {b.in0.data(), b.in0.size()};
        base.in1 = {b.in1.data(), b.in1.size()};
        base.in2 = {b.in2.data(), b.in2.size()};
        base.in3 = {b.in3.data(), b.in3.size()};

        uint32_t w_used[2] = {0, 0};
        std::vector<float>* outs[2] = {&out_a, &out_b};
        for (int pass = 0; pass < 2; ++pass) {
            astrocs_host_state_set_budget_v1(state, 1, pass == 0 ? 1u : 4u, &host);
            acs_baseline_params_v1 p = base;
            p.out0 = {outs[pass]->data(), outs[pass]->size()};
            p.out1 = {out1.data(), out1.size()};
            const acs_status rc = api.kernels[0].fn(&host, &p, sizeof(p), nullptr, nullptr);
            if (rc != ACS_OK) { std::printf("RC %d\n", (int)rc); return 3; }
            w_used[pass] = p.workers_used;
        }
        print_span("OUTA", out_a);
        print_span("OUTB", out_b);
        std::printf("WORKERS %u %u\n", w_used[0], w_used[1]);
        std::printf("DET %s\n",
                    std::memcmp(out_a.data(), out_b.data(), N * sizeof(float)) == 0 ? "OK" : "FAIL");
    }
    astrocs_host_services_destroy_state_v1(state);
    std::printf("ORACLE_RUNNER_DONE\n");
    return 0;
}
