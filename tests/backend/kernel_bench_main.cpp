// tests/backend/kernel_bench_main.cpp — ISA 热点测量 harness (ISA-001/BENCH-002 种子)
// 用法: kernel_bench [--variant <dso>]
// 对热点候选 kernel 以大 N 跑 baseline fn; 若给 --variant 则同数据经 dlopen 的变体
// (直接 dlopen+handshake, 仅供测量; 生产选择仍须走 manifest 预检)再计时。
// 输出: BENCH <op> <median_ns_per_elt> / VARIANT <op> <median_ns_per_elt>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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

uint32_t lcg_state = 7u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

using KernelFn = acs_status (*)(const astrocs_host_services_v1*, const void*, uint32_t,
                                const void*, void*);
double bench_op(const astrocs_host_services_v1* host, KernelFn fn, acs_baseline_params_v1 p,
                int reps, uint32_t* workers_used) {
    std::vector<double> t;
    for (int r = 0; r < reps; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        const acs_status rc = fn(host, &p, sizeof(p), nullptr, nullptr);
        const auto t1 = std::chrono::steady_clock::now();
        if (rc != ACS_OK) { std::printf("BENCH_RC %d\n", (int)rc); return -1; }
        t.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
        *workers_used = p.workers_used;
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

const uint32_t W = 1u << 10, H = 1u << 10;   // 1M 像素域
const uint32_t FR = 3;

}  // namespace

int main(int argc, char** argv) {
    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    astrocs_host_state_set_budget_v1(state, 2, 2, &host);
    astrocs_backend_api_v1 api;
    std::memset(&api, 0, sizeof(api));
    if (astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1),
                                   &host, &api) != ACS_OK) return 2;

    void* vhandle = nullptr;
    astrocs_backend_api_v1 vapi;
    std::memset(&vapi, 0, sizeof(vapi));
    if (argc >= 3 && std::strcmp(argv[1], "--variant") == 0) {
#if defined(_WIN32)
        HMODULE m = LoadLibraryA(argv[2]);
        vhandle = (void*)m;
        if (vhandle) {
            auto g = (int (*)(uint32_t, uint32_t, const astrocs_host_services_v1*,
                              astrocs_backend_api_v1*))GetProcAddress(m, "astrocs_backend_get_api_v1");
            if (g) g(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &vapi);
        }
#else
        vhandle = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        if (vhandle) {
            auto g = (int (*)(uint32_t, uint32_t, const astrocs_host_services_v1*,
                              astrocs_backend_api_v1*))dlsym(vhandle, "astrocs_backend_get_api_v1");
            if (g) g(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &vapi);
        }
#endif
        if (!vhandle || !vapi.abi_version) {
            std::printf("VARIANT_LOAD_FAIL\n");
            return 3;
        }
        std::printf("VARIANT_LOADED %s\n", vapi.backend_id);
    }

    const uint32_t N = W * H;
    std::vector<float> in0(N * (FR + 1)), in1(N * (FR + 1)), in2(N + 1), in3(N), out(N), out1(N);
    for (auto& x : in0) x = lcg_f();
    for (auto& x : in1) x = lcg_f();
    for (auto& x : in2) x = lcg_f() * 0.01f;   // 小值(SPMV row_ptr 会覆写)
    for (auto& x : in3) x = lcg_f() * 0.01f;

    const struct { uint32_t op; const char* name; float k; uint32_t aux0, aux1; } ops[] = {
        {ACS_KOP_CALIBRATION, "calibration", 2.0f, 0, 0},
        {ACS_KOP_NOISE_REDUCTIONS, "noise", 0.0f, FR, 0},
        {ACS_KOP_DRIZZLE_ACCUMULATE, "driz_accum", 0.0f, FR, 0},
        {ACS_KOP_UPM_SPMV, "spmv", 0.0f, N / 2, N},          // nnz=512K, ncols=N
        {ACS_KOP_INTEGRATION_ACCUM, "integration", 0.0f, FR, 0},
        {ACS_KOP_HIPS_BULK, "hips", 0.999f, W, H},
    };
    for (const auto& op : ops) {
        acs_baseline_params_v1 p;
        std::memset(&p, 0, sizeof(p));
        p.head.struct_size = sizeof(p);
        p.head.abi_version = ACS_ABI_VERSION_V1;
        p.op = op.op; p.w = W; p.h = H; p.k = op.k; p.aux0 = op.aux0; p.aux1 = op.aux1;
        p.in0 = {in0.data(), in0.size()};
        p.in1 = {in1.data(), in1.size()};
        p.in2 = {in2.data(), in2.size()};
        p.in3 = {in3.data(), in3.size()};
        p.out0 = {out.data(), out.size()};
        p.out1 = {out1.data(), out1.size()};
        if (op.op == ACS_KOP_UPM_SPMV) {   // col 索引整数域 [0,N); 行指针单调末位=nnz
            for (uint32_t k = 0; k < op.aux0; ++k) in1[k] = static_cast<float>(k % N);
            in2[0] = 0;
            for (uint32_t r = 1; r <= N; ++r)
                in2[r] = static_cast<float>(r * op.aux0 / N);
        }
        uint32_t wu = 0;
        const double ns = bench_op(&host, api.kernels[0].fn, p, 5, &wu);
        std::printf("BENCH %s %.1f workers=%u\n", op.name, ns, wu);
        if (vapi.abi_version && (op.op == ACS_KOP_CALIBRATION ||
                                 op.op == ACS_KOP_DRIZZLE_ACCUMULATE ||
                                 op.op == ACS_KOP_HIPS_BULK)) {
            const double vns = bench_op(&host, vapi.kernels[0].fn, p, 5, &wu);
            std::printf("VARIANT %s %.1f workers=%u\n", op.name, vns, wu);
        }
    }
    if (vhandle) {
#if defined(_WIN32)
        FreeLibrary((HMODULE)vhandle);
#else
        dlclose(vhandle);
#endif
    }
    astrocs_host_services_destroy_state_v1(state);
    std::printf("BENCH_DONE\n");
    return 0;
}
