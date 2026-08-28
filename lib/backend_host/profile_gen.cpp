// lib/backend_host/profile_gen.cpp — cpu_profile.json 生成实现 (06 §5) — BENCH-004/005
#include "profile_gen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include <nlohmann/json.hpp>

#include "astrocs/common_abi_v1.h"
#include "baseline_kernels.h"
#include "bench_harness.h"
#include "hardware_inspect.h"
#include "sha256.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
int astrocs_backend_get_api_v1(uint32_t, uint32_t, const astrocs_host_services_v1*,
                               astrocs_backend_api_v1*);
}

namespace astrocs::backend_host {

namespace {

uint32_t lcg_state = 11u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

}  // namespace

std::string generate_profile_json(const std::string& mode, const std::string& build_id,
                                  const std::string& commit, const std::string& backend_sha) {
    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    astrocs_host_state_set_budget_v1(state, 2, 2, &host);
    astrocs_backend_api_v1 api{};
    astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &api);

    const std::string hw_json = hardware_inspect_json_v1(build_id);
    const nlohmann::json hw_full = nlohmann::json::parse(hw_json);
    const std::string fingerprint_src =
        hw_full.value("vendor", std::string()) + "|" + std::to_string(hw_full.value("family", 0)) +
        "|" + std::to_string(hw_full.value("model", 0)) + "|" +
        std::to_string(hw_full.value("stepping", 0)) + "|" +
        std::to_string(hw_full.value("feature_bits", 0ull)) + "|" +
        std::to_string(hw_full.value("xcr0", 0ull)) + "|" +
        std::to_string(hw_full.value("available_logical_cpus", 0u));
    crypto::Sha256 fh;
    fh.update(fingerprint_src.data(), fingerprint_src.size());

    nlohmann::json j;
    j["schema_version"] = 1;
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        gmtime_r(&t, &tm);
        char ts[40];
        std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                      tm.tm_sec);
        j["created_at_utc"] = ts;
    }
    j["mode"] = mode;
    j["hardware"] = {
        {"fingerprint", fh.final_hex()},
        {"architecture", hw_full.value("architecture", "amd64")},
        {"vendor", hw_full.value("vendor", "")},
        {"family", hw_full.value("family", 0)},
        {"model", hw_full.value("model", 0)},
        {"stepping", hw_full.value("stepping", 0)},
        {"feature_bits", hw_full.value("feature_bits", 0ull)},
        {"xcr0", hw_full.value("xcr0", 0ull)},
        {"available_logical_cpus", hw_full.value("available_logical_cpus", 1u)},
        {"affinity", hw_full.value("affinity", nlohmann::json::array())},
    };
    j["build"] = {
        {"version", build_id},
        {"commit", commit},
        {"cli_sha256", hw_full.value("cli_sha256", "")},
        {"abi_version", ACS_ABI_VERSION_V1},
        {"backend_sha256", backend_sha},
    };
    const auto mem = bench_memory(2u << 20, 3);
    j["memory_benchmark"] = {
        {"read_gbs", mem.read_gbs}, {"write_gbs", mem.write_gbs}, {"copy_gbs", mem.copy_gbs},
        {"triad32_gbs", mem.triad32_gbs}, {"triad64_gbs", mem.triad64_gbs}};

    // 逐 kernel 矩阵: quick=calibration medium; full=全部 12 注册 kernel(medium 域)
    const uint32_t W = 512, H = 512, N = W * H;
    std::vector<float> in0(N), in1(N), in2(N), in3(N), out(N), out1(N);
    for (uint32_t i = 0; i < N; ++i) {
        in0[i] = lcg_f(); in1[i] = lcg_f() * 0.01f;
        in2[i] = lcg_f() * 0.01f; in3[i] = 1.0f + lcg_f() * 0.001f;
    }
    std::vector<double> expected(N);
    for (uint32_t i = 0; i < N; ++i)
        expected[i] = (static_cast<double>(in0[i]) - in1[i] - 2.0 * in2[i]) * in3[i];
    acs_baseline_params_v1 p;
    std::memset(&p, 0, sizeof(p));
    p.head.struct_size = sizeof(p);
    p.head.abi_version = ACS_ABI_VERSION_V1;
    p.op = ACS_KOP_CALIBRATION; p.w = W; p.h = H; p.k = 2.0f;
    p.in0 = {in0.data(), in0.size()};
    p.in1 = {in1.data(), in1.size()};
    p.in2 = {in2.data(), in2.size()};
    p.in3 = {in3.data(), in3.size()};
    p.out0 = {out.data(), out.size()};
    p.out1 = {out1.data(), out1.size()};

    const uint32_t kernel_count = (mode == "full") ? api.kernel_count : 1u;
    bool all_pass = true;
    nlohmann::json kernels = nlohmann::json::array();
    for (uint32_t i = 0; i < kernel_count && i < api.kernel_count; ++i) {
        auto r = bench_kernel(&host, api.backend_id, api.kernels[i].fn, p, expected, 2e-4, 2, 9);
        if (r.verdict != "OK") all_pass = false;
        kernels.push_back({
            {"kernel_id", std::string(api.kernels[i].algorithm_id)},
            {"kernel_version", std::string(api.kernels[i].kernel_version)},
            {"precision", "fp32"},
            {"size_class", "medium"},
            {"backend_id", r.backend_id},
            {"workers", r.workers},
            {"block_size", 0},
            {"oracle_status", r.verdict == "OK" ? "pass" : "fail"},
            {"measurements", {
                 {"median_ns", r.median_ns}, {"mad_ns", r.mad_ns},
                 {"p05_ns", r.p05_ns}, {"p95_ns", r.p95_ns},
                 {"correctness_hash", r.correctness_hash}}},
        });
    }
    j["kernels"] = kernels;
    j["verdict"] = all_pass ? "PASS" : "FAIL";
    astrocs_host_services_destroy_state_v1(state);
    return j.dump(2) + "\n";
}

}  // namespace astrocs::backend_host
