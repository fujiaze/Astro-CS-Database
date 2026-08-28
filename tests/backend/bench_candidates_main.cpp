// tests/backend/bench_candidates_main.cpp — BENCH-003 候选扫描驱动
// 输出: MEMORY 行 + KERNEL 行(size_class×align×worker 候选)+SAMPLES 文件引用(原始样本)。
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

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

uint32_t lcg_state = 5u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    const char* samples_path = argv[1];
    std::ofstream samples_file(samples_path, std::ios::binary | std::ios::trunc);   // 原始样本引用

    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    astrocs_backend_api_v1 api{};
    astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &api);

    // ── 内存带宽基线(读/写/拷贝/triad32/64) ──
    const auto mem = astrocs::backend_host::bench_memory(4u << 20, 5);
    std::printf("MEMORY read=%.1f write=%.1f copy=%.1f triad32=%.1f triad64=%.1f rss=%llu\n",
                mem.read_gbs, mem.write_gbs, mem.copy_gbs, mem.triad32_gbs, mem.triad64_gbs,
                (unsigned long long)mem.rss_delta_bytes);

    const uint32_t l2 = 512u << 10;   // L2 画像来自 hardware inspect; 此处用作派生输入
    // block 候选(由 L2 派生的几何序列; 机器无关, 无固定数值)
    for (uint64_t b : astrocs::backend_host::block_candidates(l2, 4))
        std::printf("BLOCK_CAND %llu\n", (unsigned long long)b);

    // ── kernel 候选扫描: calibration × size(small/medium/large)×align(对齐/偏移)×worker 候选 ──
    const struct { const char* cls; uint32_t n; } sizes[] = {
        {"small", 4096}, {"medium", 262144}, {"large", 1048576}};
    for (const auto& sz : sizes) {
        for (int align_kind = 0; align_kind < 2; ++align_kind) {
            const uint32_t off = align_kind ? 1 : 0;   // 0=64B 对齐, 1=偏移 4B(非对齐)
            std::vector<float> in0(sz.n + 8), in1(sz.n + 8), in2(sz.n + 8), in3(sz.n + 8), out(sz.n);
            for (uint32_t i = 0; i < sz.n; ++i) {
                in0[i] = lcg_f(); in1[i] = lcg_f() * 0.01f;
                in2[i] = lcg_f() * 0.01f; in3[i] = 1.0f + lcg_f() * 0.001f;
            }
            double best = 1e18;
            uint32_t best_w = 0;
            for (uint32_t wc : astrocs::backend_host::worker_candidates(2u)) {
                astrocs_host_state_set_budget_v1(state, wc, wc, &host);
                acs_baseline_params_v1 p;
                std::memset(&p, 0, sizeof(p));
                p.head.struct_size = sizeof(p);
                p.head.abi_version = ACS_ABI_VERSION_V1;
                p.op = ACS_KOP_CALIBRATION;
                p.w = sz.n; p.h = 1; p.k = 2.0f;
                p.in0 = {in0.data() + off, in0.size() - off};
                p.in1 = {in1.data() + off, in1.size() - off};
                p.in2 = {in2.data() + off, in2.size() - off};
                p.in3 = {in3.data() + off, in3.size() - off};
                p.out0 = {out.data(), out.size()};
                std::vector<double> samples;
                std::ostringstream srow;
                for (int r = 0; r < 5; ++r) {
                    const auto t0 = std::chrono::steady_clock::now();
                    const acs_status rc = api.kernels[0].fn(&host, &p, sizeof(p), nullptr, nullptr);
                    const auto t1 = std::chrono::steady_clock::now();
                    if (rc != ACS_OK) { std::printf("SCAN_RC %d\n", (int)rc); return 3; }
                    samples.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count());
                }
                std::sort(samples.begin(), samples.end());
                const double med = samples[samples.size() / 2];
                if (med < best) { best = med; best_w = wc; }
                std::printf("CAND %s align%d w%u %.0f\n", sz.cls, align_kind, wc, med);
                srow << sz.cls << " align" << align_kind << " w" << wc;
                for (double s : samples) srow << " " << s;
                srow << "\n";
                samples_file << srow.str();
            }
            std::printf("BEST %s align%d w%u %.0f\n", sz.cls, align_kind, best_w, best);
        }
    }
    std::printf("CANDIDATES_DONE\n");
    astrocs_host_services_destroy_state_v1(state);
    return 0;
}
