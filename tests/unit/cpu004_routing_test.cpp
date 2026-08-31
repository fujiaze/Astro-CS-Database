// tests/unit/cpu004_routing_test.cpp — CPU-004 (G3) 逐 kernel 自适应路由单元测试
// 覆盖: v2 profile 机器一致性校验(stale 判定); 逐 kernel 路由(provider/workers/block);
//       unsupported provider → baseline 但保留多线程; 无 profile → 保守 baseline 多线程。
#include "cpu_routing.h"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 合成 v2 profile JSON(host 与某 hw_json 匹配; 由调用方传入 quota/avail)
static std::string make_profile(const std::string& quota, uint32_t avail,
                                const std::string& commit,
                                const std::string& kernel_provider,
                                uint32_t kernel_workers) {
    return
        "{\"schema\":\"astrocs.cpu-profile/v2\","
        "\"profile_id\":\"sha256:" + std::string(64, 'a') + "\","
        "\"created_utc\":\"2026-09-01T00:00:00Z\","
        "\"host\":{\"arch\":\"amd64\",\"vendor\":\"x\",\"family\":6,\"model\":1,"
        "\"stepping\":1,\"os_abi\":\"linux\",\"features\":[\"sse2\"],\"xcr0\":\"255\","
        "\"logical_available\":" + std::to_string(avail) + ",\"quota_signature\":\"" + quota + "\"},"
        "\"build\":{\"astrocs_version\":\"0.10.0-alpha.2\",\"source_commit\":\"" + commit + "\","
        "\"benchmark_binary_sha256\":\"" + std::string(64, 'b') + "\","
        "\"runtime_build_id\":\"r\",\"provider_build_ids\":{\"baseline\":\"loaded\"}},"
        "\"memory_bandwidth\":{\"copy\":1.0,\"read\":1.0,\"write\":1.0,\"triad\":1.0},"
        "\"raw_samples_sha256\":\"" + std::string(64, 'c') + "\","
        "\"kernels\":{\"calibration-pixel-transform\":{"
        "\"workload_class\":\"compute\",\"provider\":\"" + kernel_provider + "\","
        "\"workers\":" + std::to_string(kernel_workers) + ",\"block\":8192,"
        "\"correctness_test\":\"oracle:pass\","
        "\"self_test_sha256\":\"" + std::string(64, 'd') + "\","
        "\"median\":100.0,\"mad\":1.0,\"fallback_reason\":null}}}";
}

static std::string make_hw(const std::string& quota, uint32_t avail) {
    return "{\"available_logical_cpus\":" + std::to_string(avail) +
           ",\"quota_signature\":\"" + quota + "\",\"arch\":\"amd64\"}";
}

int main() {
    const std::string commit = std::string(40, '1');
    const std::string quota = std::string(64, '9');
    const std::string hw = make_hw(quota, 4);

    // 1) profile 机器一致性: 匹配 → valid
    {
        const std::string prof = make_profile(quota, 4, commit, "baseline", 4);
        const auto v = astrocs::backend_host::validate_profile_v2_for_machine(prof, commit, hw);
        CHECK(v.valid);
        CHECK(v.stale_reason.empty());
    }
    // 2) quota_signature 不匹配 → stale
    {
        const std::string prof = make_profile(std::string(64, '0'), 4, commit, "baseline", 4);
        const auto v = astrocs::backend_host::validate_profile_v2_for_machine(prof, commit, hw);
        CHECK(!v.valid);
        CHECK(v.stale_reason.find("quota_signature") != std::string::npos);
    }
    // 3) logical_available 不匹配 → stale
    {
        const std::string prof = make_profile(quota, 8, commit, "baseline", 4);
        const auto v = astrocs::backend_host::validate_profile_v2_for_machine(prof, commit, hw);
        CHECK(!v.valid);
        CHECK(v.stale_reason.find("logical_available") != std::string::npos);
    }
    // 4) commit 不匹配 → stale(verify_profile_v2 内部)
    {
        const std::string prof = make_profile(quota, 4, std::string(40, '0'), "baseline", 4);
        const auto v = astrocs::backend_host::validate_profile_v2_for_machine(prof, commit, hw);
        CHECK(!v.valid);
    }
    // 5) 逐 kernel 路由: provider=baseline workers=4 → 原样
    {
        const std::string prof = make_profile(quota, 4, commit, "baseline", 4);
        astrocs::backend_host::KernelRoute r;
        const bool ok = astrocs::backend_host::route_kernel_from_profile(
            prof, "calibration-pixel-transform", hw, &r);
        CHECK(ok);
        CHECK(r.provider == "baseline");
        CHECK(r.workers == 4);
        CHECK(r.block == 8192);
        CHECK(r.self_test_sha256.size() == 64);
    }
    // 6) kernel 不在 profile → 保守 baseline + 多线程
    {
        const std::string prof = make_profile(quota, 4, commit, "baseline", 4);
        astrocs::backend_host::KernelRoute r;
        const bool ok = astrocs::backend_host::route_kernel_from_profile(
            prof, "no-such-kernel", hw, &r);
        CHECK(ok);
        CHECK(r.provider == "baseline");
        CHECK(r.workers == 4);   // 保守但多线程(08 §4-8)
        CHECK(r.fallback_reason.find("kernel not in profile") != std::string::npos);
    }
    // 7) 无 profile(空串) → conservative_route baseline + workers=avail
    {
        astrocs::backend_host::KernelRoute r;
        const bool ok = astrocs::backend_host::route_kernel_from_profile("", "k", hw, &r);
        CHECK(!ok);   // 整体不可用
        CHECK(r.provider == "baseline");
        CHECK(r.workers == 4);
        CHECK(r.fallback_reason.find("malformed") != std::string::npos);
    }
    // 8) conservative_route: available=1 → workers=1; available=2 → workers=2(不退 1)
    {
        auto r1 = astrocs::backend_host::conservative_route("k", 1);
        CHECK(r1.workers == 1);
        auto r2 = astrocs::backend_host::conservative_route("k", 2);
        CHECK(r2.workers == 2);
    }
    // 9) unsupported provider → baseline 但保留多线程(08 §4-7/§4-8)
    {
        const std::string prof = make_profile(quota, 4, commit, "fancy-isa", 4);
        astrocs::backend_host::KernelRoute r;
        const bool ok = astrocs::backend_host::route_kernel_from_profile(
            prof, "calibration-pixel-transform", hw, &r);
        CHECK(ok);
        CHECK(r.provider == "baseline");
        CHECK(r.workers == 4);   // 保守 ≠ 单线程
        CHECK(r.fallback_reason.find("unsupported") != std::string::npos);
    }
    // 10) 损坏 profile JSON → 整体不可用 + 保守多线程
    {
        astrocs::backend_host::KernelRoute r;
        const bool ok = astrocs::backend_host::route_kernel_from_profile(
            "{not json", "k", hw, &r);
        CHECK(!ok);
        CHECK(r.provider == "baseline");
        CHECK(r.workers == 4);
    }

    if (failures == 0) {
        std::printf("CPU-004 TESTS PASS (v2 profile 机器一致性/逐 kernel 路由/unsupported 回退/保守多线程)\n");
        return 0;
    }
    std::fprintf(stderr, "CPU-004 TESTS FAIL (%d)\n", failures);
    return 1;
}
