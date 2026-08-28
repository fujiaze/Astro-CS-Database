// tests/backend/bench_harness_main.cpp — BENCH-002 harness 驱动
// 用法: bench_harness_main [--cheat <dso>] [--op <name>]
// 输出: RESULT <backend> <verdict> <median> <mad> <p05> <p95> <hash> / SELECT <backend>
// 独立 scalar Oracle: double 逐元素参考(与 kernel f32 实现不同路径)。
#include <cmath>
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
#include "bench_harness.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
int astrocs_backend_get_api_v1(uint32_t, uint32_t, const astrocs_host_services_v1*,
                               astrocs_backend_api_v1*);
}

namespace {

uint32_t lcg_state = 99u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

using GetApiFn = int (*)(uint32_t, uint32_t, const astrocs_host_services_v1*,
                         astrocs_backend_api_v1*);

astrocs_backend_api_v1 load_dso_api(const char* path) {
    astrocs_backend_api_v1 a{};
#if defined(_WIN32)
    HMODULE m = LoadLibraryA(path);
    if (!m) return a;
    auto g = (GetApiFn)GetProcAddress(m, "astrocs_backend_get_api_v1");
#else
    void* m = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!m) return a;
    auto g = (GetApiFn)dlsym(m, "astrocs_backend_get_api_v1");
#endif
    astrocs_host_services_v1 h;
    void* st = nullptr;
    if (g && astrocs_host_services_default_v1(&h, &st) == ACS_OK)
        g(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &h, &a);
    return a;
}

}  // namespace

int main(int argc, char** argv) {
    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    astrocs_host_state_set_budget_v1(state, 2, 2, &host);

    astrocs_backend_api_v1 api;
    std::memset(&api, 0, sizeof(api));
    astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &api);

    // calibration 数据(N=64K): in0=light, in1=bias, in2=dark(k=2), in3=gain
    const uint32_t W = 256, H = 256, N = W * H;
    const float k = 2.0f;
    std::vector<float> light(N), bias(N), dark(N), gain(N), out(N);
    for (uint32_t i = 0; i < N; ++i) {
        light[i] = lcg_f(); bias[i] = lcg_f() * 0.01f;
        dark[i] = lcg_f() * 0.01f; gain[i] = 1.0f + lcg_f() * 0.001f;
    }
    // ── 独立 scalar Oracle: double 参考(与任何 kernel f32 实现都不同路径) ──
    std::vector<double> expected(N);
    for (uint32_t i = 0; i < N; ++i)
        expected[i] = (static_cast<double>(light[i]) - bias[i] - k * dark[i]) * gain[i];

    acs_baseline_params_v1 p;
    std::memset(&p, 0, sizeof(p));
    p.head.struct_size = sizeof(p);
    p.head.abi_version = ACS_ABI_VERSION_V1;
    p.op = ACS_KOP_CALIBRATION; p.w = W; p.h = H; p.k = k;
    p.in0 = {light.data(), light.size()};
    p.in1 = {bias.data(), bias.size()};
    p.in2 = {dark.data(), dark.size()};
    p.in3 = {gain.data(), gain.size()};
    p.out0 = {out.data(), out.size()};
    p.out1 = {nullptr, 0};

    std::vector<astrocs::backend_host::BenchResult> results;
    // 候选 1: baseline(正确)
    results.push_back(astrocs::backend_host::bench_kernel(
        &host, "baseline", api.kernels[0].fn, p, expected, 2e-4, 3, 9));
    // 候选 2: cheat DSO(错误但极快)
    astrocs_backend_api_v1 cheat{};
    const char* cheat_path = nullptr;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--cheat") == 0) cheat_path = argv[i + 1];
    if (cheat_path) {
        cheat = load_dso_api(cheat_path);
        if (cheat.abi_version && cheat.kernels)
            results.push_back(astrocs::backend_host::bench_kernel(
                &host, cheat.backend_id, cheat.kernels[0].fn, p, expected, 2e-4, 3, 9));
        else
            std::printf("CHEAT_LOAD_FAIL\n");
    }

    for (const auto& r : results)
        std::printf("RESULT %s %s %.0f %.0f %.0f %.0f %s %s\n", r.backend_id.c_str(),
                    r.verdict.c_str(), r.median_ns, r.mad_ns, r.p05_ns, r.p95_ns,
                    r.correctness_hash.substr(0, 16).c_str(), r.reason.c_str());
    std::printf("SELECT %s\n", astrocs::backend_host::select_winner(results).c_str());
    astrocs_host_services_destroy_state_v1(state);
    return 0;
}
