// tests/backend/profile_gen_main.cpp — cpu_profile.json 生成器 (06 §5, BENCH-004)
// 用法: profile_gen --out <p.json> --mode quick|full --version V --commit C --backend-sha S
// 生成对 cpu_profile.schema.json 有效的 profile(hardware 子集+build+memory+kernels+verdict)。
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <fstream>
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

namespace {

uint32_t lcg_state = 11u;
float lcg_f() {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return static_cast<float>(lcg_state % 2000u) / 100.0f;
}

std::string arg_of(int argc, char** argv, const std::string& key, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; ++i)
        if (key == argv[i]) return argv[i + 1];
    return def;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out_path = arg_of(argc, argv, "--out");
    const std::string mode = arg_of(argc, argv, "--mode", "quick");
    const std::string version = arg_of(argc, argv, "--version", "0.9.0-alpha.1");
    const std::string commit = arg_of(argc, argv, "--commit", "0");
    const std::string backend_sha = arg_of(argc, argv, "--backend-sha", "0");
    if (out_path.empty()) return 2;

    astrocs_host_services_v1 host;
    void* state = nullptr;
    astrocs_host_services_default_v1(&host, &state);
    astrocs_host_state_set_budget_v1(state, 2, 2, &host);
    astrocs_backend_api_v1 api{};
    astrocs_backend_get_api_v1(ACS_ABI_VERSION_V1, sizeof(astrocs_host_services_v1), &host, &api);

    // hardware 子集(与 cpu_profile.schema hardware 对象同构)
    const std::string hw_json = astrocs::backend_host::hardware_inspect_json_v1(version);
    const nlohmann::json hw_full = nlohmann::json::parse(hw_json);
    const std::string fingerprint_src =
        hw_full.value("vendor", std::string()) + "|" + std::to_string(hw_full.value("family", 0)) +
        "|" + std::to_string(hw_full.value("model", 0)) + "|" +
        std::to_string(hw_full.value("stepping", 0)) + "|" +
        std::to_string(hw_full.value("feature_bits", 0ull)) + "|" +
        std::to_string(hw_full.value("xcr0", 0ull)) + "|" +
        std::to_string(hw_full.value("available_logical_cpus", 0u));
    astrocs::crypto::Sha256 fh;
    fh.update(fingerprint_src.data(), fingerprint_src.size());

    nlohmann::json j;
    j["schema_version"] = 1;
    {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        gmtime_r(&t, &tm);
        char ts[32];
        std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
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
        {"version", version},
        {"commit", commit},
        {"cli_sha256", hw_full.value("cli_sha256", "")},
        {"abi_version", ACS_ABI_VERSION_V1},
        {"backend_sha256", backend_sha},
    };
    const auto mem = astrocs::backend_host::bench_memory(2u << 20, 3);
    j["memory_benchmark"] = {
        {"read_gbs", mem.read_gbs}, {"write_gbs", mem.write_gbs}, {"copy_gbs", mem.copy_gbs},
        {"triad32_gbs", mem.triad32_gbs}, {"triad64_gbs", mem.triad64_gbs}};

    // 逐 kernel: calibration medium(正确性筛选→预热→9 样本)
    const uint32_t W = 512, H = 512, N = W * H;
    std::vector<float> in0(N), in1(N), in2(N), in3(N), out(N);
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
    auto r = astrocs::backend_host::bench_kernel(&host, "baseline", api.kernels[0].fn, p,
                                                 expected, 2e-4, 2, 9);
    nlohmann::json k = {
        {"kernel_id", "calibration-pixel-transform"},
        {"kernel_version", "1.0.0"},
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
    };
    j["kernels"] = nlohmann::json::array({k});
    j["verdict"] = (r.verdict == "OK") ? "PASS" : "FAIL";

    std::ofstream f(out_path, std::ios::binary | std::ios::trunc);
    f << j.dump(2) << "\n";
    std::printf("%s\n", out_path.c_str());
    astrocs_host_services_destroy_state_v1(state);
    return 0;
}
