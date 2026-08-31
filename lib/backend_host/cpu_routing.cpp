// lib/backend_host/cpu_routing.cpp — CPU-004 (G3) 逐 kernel 自适应路由实现
#include "cpu_routing.h"

#include <nlohmann/json.hpp>

#include "cpu_features.h"
#include "hardware_inspect.h"
#include "profile_gen.h"

namespace astrocs::backend_host {

namespace {

// 机器可检测 feature bits(与 cpu_features.h 一致)
uint64_t detected_features() { return astrocs_cpu_detect_features_v1(); }

bool provider_supported(const std::string& provider) {
    const uint64_t feats = detected_features();
    if (provider == "baseline") return true;
    if (provider == "avx2") return (feats & (ACS_FEAT_AVX2 | ACS_FEAT_FMA)) ==
                                   (ACS_FEAT_AVX2 | ACS_FEAT_FMA);
    if (provider == "avx512") return (feats & ACS_FEAT_AVX512F) != 0;
    return false;
}

uint32_t logical_available_from_hw(const std::string& hw_json) {
    try {
        const auto hw = nlohmann::json::parse(hw_json);
        return static_cast<uint32_t>(hw.value("available_logical_cpus", 1u));
    } catch (...) {
        return 1;
    }
}

}  // namespace

ProfileVerdict validate_profile_v2_for_machine(const std::string& profile_json,
                                               const std::string& current_commit,
                                               const std::string& hw_json) {
    ProfileVerdict v;
    nlohmann::json prof;
    try {
        prof = nlohmann::json::parse(profile_json);
    } catch (...) {
        v.stale_reason = "profile malformed JSON";
        return v;
    }
    // 结构校验(复用 verify_profile_v2; 含 schema/必填字段/版本/commit/workers/block)
    const std::string err = verify_profile_v2(profile_json, current_commit);
    if (!err.empty()) {
        v.stale_reason = err;
        return v;
    }
    // 机器一致性: arch/quota_signature
    const auto& hst = prof["host"];
    if (hst.value("arch", "") != "amd64") {
        v.stale_reason = "host.arch != amd64";
        return v;
    }
    try {
        const auto hw = nlohmann::json::parse(hw_json);
        const std::string hw_q = hw.value("quota_signature", "");
        const std::string prof_q = hst.value("quota_signature", "");
        if (hw_q.empty() || prof_q.empty() || hw_q != prof_q) {
            v.stale_reason = "quota_signature mismatch (rerun 'astrocs benchmark cpu')";
            return v;
        }
        const uint32_t hw_avail = static_cast<uint32_t>(hw.value("available_logical_cpus", 0u));
        const uint32_t prof_avail = static_cast<uint32_t>(hst.value("logical_available", 0u));
        if (hw_avail != prof_avail) {
            v.stale_reason = "logical_available mismatch (affinity/cgroup changed)";
            return v;
        }
    } catch (...) {
        v.stale_reason = "hardware inspect unavailable";
        return v;
    }
    v.valid = true;
    return v;
}

bool route_kernel_from_profile(const std::string& profile_json,
                               const std::string& kernel_id,
                               const std::string& hw_json,
                               KernelRoute* out_route) {
    if (!out_route) return false;
    nlohmann::json prof;
    try {
        prof = nlohmann::json::parse(profile_json);
    } catch (...) {
        *out_route = conservative_route(kernel_id, logical_available_from_hw(hw_json));
        out_route->fallback_reason = "profile malformed";
        return false;
    }
    if (!prof.contains("kernels") || !prof["kernels"].is_object() ||
        !prof["kernels"].contains(kernel_id)) {
        // 该 kernel 无 profile 记录 → 保守 baseline 但多线程
        *out_route = conservative_route(kernel_id, logical_available_from_hw(hw_json));
        out_route->fallback_reason = "kernel not in profile";
        return true;   // profile 整体有效, 该 kernel 回退
    }
    const auto& kp = prof["kernels"][kernel_id];
    const std::string provider = kp.value("provider", "baseline");
    KernelRoute r;
    r.kernel_id = kernel_id;
    r.workers = static_cast<uint32_t>(kp.value("workers", 1u));
    r.block = static_cast<uint64_t>(kp.value("block", 1ull));
    r.self_test_sha256 = kp.value("self_test_sha256", "");
    if (!provider_supported(provider)) {
        // provider 不可用 → 回 baseline(保守)但保留 profile 的 workers(多线程)
        r.provider = "baseline";
        r.fallback_reason = "provider '" + provider + "' unsupported on this CPU";
        if (r.workers < 1) r.workers = 1;
        *out_route = r;
        return true;
    }
    r.provider = provider;
    r.fallback_reason = kp.contains("fallback_reason") && !kp["fallback_reason"].is_null()
        ? kp["fallback_reason"].get<std::string>() : "";
    if (r.workers < 1) r.workers = 1;
    *out_route = r;
    return true;
}

KernelRoute conservative_route(const std::string& kernel_id, uint32_t available_cpus) {
    KernelRoute r;
    r.kernel_id = kernel_id;
    r.provider = "baseline";
    r.workers = available_cpus > 0 ? available_cpus : 1;   // ≥1; 可用≥2 不退 1(08 §4-8)
    r.block = 1;
    r.fallback_reason = "no_valid_profile";
    return r;
}

}  // namespace astrocs::backend_host
