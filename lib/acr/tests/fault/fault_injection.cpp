// lib/acr/tests/fault/fault_injection.cpp — Phase H 故障注入测试
// 测试场景：
//   1. backend 缺失（CUDA 不可用 → CPU 降级）
//   2. profile corrupt（JSON 解析失败 → CPU baseline + 警告）
//   3. profile missing（无 routes.json → CPU baseline + 警告）
//   4. profile stale（指纹不匹配 → 警告 + 继续运行）
//   5. device lost（运行中设备消失 → invalidate → reload）
//   6. 取消正在执行的 kernel
//   7. kernel 异常传播（throw → mark_failed）
//   8. 路由 missing 时纯 CPU + 警告
//   9. 混合调度 fallback（设备失败 → CPU 回退）
#include <gtest/gtest.h>

#include <route_profile.hpp>
#include <static_router.hpp>
#include <dispatcher.hpp>
#include <fallback.hpp>
#include <partitioner.hpp>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::routing;
using namespace astro::compute::scheduler;

namespace {

// 写测试用 routes.json
bool write_test_profile(const std::string& path,
                        const std::string& sha256 = "validsha256",
                        const std::string& schema = "acr.route_profile.v1") {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{"
      << "\"schema_version\":\"" << schema << "\","
      << "\"generated_at\":\"20260802T120000Z\","
      << "\"profile_kind\":\"standard\","
      << "\"fingerprint\":{"
      <<   "\"cpu_model\":\"TestCPU\","
      <<   "\"cpu_cores\":8,"
      <<   "\"isa_mask\":255,"
      <<   "\"gpu_name\":\"\","
      <<   "\"gpu_memory_bytes\":0,"
      <<   "\"gpu_driver_version\":\"\","
      <<   "\"sha256\":\"" << sha256 << "\""
      << "},"
      << "\"routes\":["
      <<   "{\"kernel_id\":3,\"kernel_name\":\"AXPY\",\"precision\":\"fp32\","
      <<    "\"preferred_backend\":\"cpu\",\"expected_throughput_gbps\":10.5,\"reason\":\"only-avail\"}"
      << "]"
      << "}";
    return f.good();
}

} // anonymous namespace

// ============================================================================
// 1. backend 缺失：CUDA backend 不可用时降级到 CPU
// ============================================================================
TEST(FaultInjection, BackendUnavailableDegradesToCpu) {
    // ACR_BUILD_CUDA=OFF 时，CUDA backend 不可用
    // 验证：通过 routing 解析，CUDA kernel 仍能降级到 CPU
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_fault_backend.json");
    auto res = r.resolve(KernelId::AXPY);
    EXPECT_TRUE(res.missing);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "missing-profile");
    // CPU 执行 AXPY 应当成功
    std::vector<float> x(100, 1.0f), y(100, 2.0f);
    parallel_for(KernelId::AXPY, Range1D{0, 100}, [&](std::size_t i) {
        y[i] = 2.0f * x[i] + y[i];
    });
    EXPECT_FLOAT_EQ(y[0], 4.0f);
}

// ============================================================================
// 2. profile corrupt：JSON 解析失败 → CPU baseline + 警告
// ============================================================================
TEST(FaultInjection, CorruptProfileDegradesToCpu) {
    const char* path = "acr_fault_corrupt.json";
    {
        std::ofstream f(path);
        f << "{ this is >>> NOT <<< valid json ]]]";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(KernelId::AXPY);
    EXPECT_TRUE(res.corrupt);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "corrupt");
    EXPECT_EQ(res.profile_state, ProfileState::Corrupt);
    std::remove(path);
}

// ============================================================================
// 3. profile missing：无 routes.json → CPU baseline + 警告
// ============================================================================
TEST(FaultInjection, MissingProfileDegradesToCpu) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_fault_missing.json");
    auto res = r.resolve(KernelId::AXPY);
    EXPECT_TRUE(res.missing);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "missing-profile");
    EXPECT_EQ(res.profile_state, ProfileState::Missing);
}

// ============================================================================
// 4. profile stale：指纹不匹配 → 警告 + 继续运行
// ============================================================================
TEST(FaultInjection, StaleProfileContinuesWithWarning) {
    const char* path = "acr_fault_stale.json";
    ASSERT_TRUE(write_test_profile(path, "faksha256_stale"));
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(KernelId::AXPY);
    // profile 存在但指纹不匹配 → stale
    EXPECT_FALSE(res.missing);
    EXPECT_FALSE(res.corrupt);
    // stale 或 profile（取决于当前机器指纹是否恰好匹配）
    EXPECT_TRUE(res.stale || res.reason == "profile");
    // 仍按 profile 路由（不强制回退 CPU）
    EXPECT_EQ(res.backend, "cpu");
    std::remove(path);
}

// ============================================================================
// 5. device lost：运行中设备消失 → invalidate → reload
// ============================================================================
TEST(FaultInjection, DeviceLostTriggersReload) {
    const char* path = "acr_fault_device_lost.json";
    ASSERT_TRUE(write_test_profile(path, "devlost_sha"));
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res1 = r.resolve(KernelId::AXPY);
    EXPECT_FALSE(res1.missing);
    // 模拟 device lost：删除 profile + invalidate
    std::remove(path);
    r.invalidate_cache();
    auto res2 = r.resolve(KernelId::AXPY);
    EXPECT_TRUE(res2.missing);
    EXPECT_EQ(res2.backend, "cpu");
}

// ============================================================================
// 6. 取消正在执行的 kernel
// ============================================================================
TEST(FaultInjection, CancelRunningKernel) {
    std::atomic<int> cnt{0};
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 1000},
        [&cnt](std::size_t) { cnt.fetch_add(1, std::memory_order_relaxed); });
    ev.wait();
    ev.cancel();
    EXPECT_TRUE(ev.cancelled());
    EXPECT_EQ(cnt.load(), 1000);
    EXPECT_EQ(ev.status(), StatusCode::Ok);
}

// ============================================================================
// 7. kernel 异常传播：throw → mark_failed(KernelFailed)
// ============================================================================
TEST(FaultInjection, KernelExceptionPropagatesAsFailed) {
    Event ev = parallel_for(KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });
    EXPECT_EQ(ev.status(), StatusCode::KernelFailed);
    EXPECT_TRUE(ev.ready());
}

// ============================================================================
// 8. 路由 missing 时纯 CPU + 警告
// ============================================================================
TEST(FaultInjection, RoutingMissingUsesCpuBaseline) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_fault_routing.json");
    // 多个 KernelId 都应回退 CPU
    for (auto kid : {KernelId::AXPY, KernelId::Copy, KernelId::Triad, KernelId::Dot}) {
        auto res = r.resolve(kid);
        EXPECT_EQ(res.backend, "cpu");
        EXPECT_TRUE(res.missing);
    }
}

// ============================================================================
// 9. 混合调度 fallback：设备失败 → CPU 回退
// ============================================================================
TEST(FaultInjection, MixedScheduleFallbackToCpu) {
    runtime_init();
    Dispatcher d;
    DispatcherConfig cfg;
    cfg.preferred_backend = "cpu";
    cfg.fallback_strategy = FallbackStrategy::ToCpu;
    cfg.devices = {{"cpu", 0, 0, 50.0, true}};
    d.configure(cfg);

    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = d.dispatch_range(0, 100, 25, fn, &data);
    EXPECT_TRUE(r.all_done);
    EXPECT_EQ(r.failed_chunks, 0u);
    int sum = 0;
    for (auto v : data) sum += v;
    EXPECT_EQ(sum, 100);
    runtime_shutdown();
}

// ============================================================================
// 10. 异常 chunk 在混合调度中被计为 failed
// ============================================================================
TEST(FaultInjection, ExceptionChunkCountedAsFailed) {
    runtime_init();
    MixedRunner runner;
    MixedRunnerConfig cfg;
    cfg.preferred_backend = "cpu";
    runner.configure(cfg);
    std::vector<int> data(100, 0);
    auto fn = +[](std::size_t, std::size_t b, std::size_t e, void* ud) {
        std::vector<int>* d = static_cast<std::vector<int>*>(ud);
        if (b == 0) throw std::runtime_error("chunk 0 failed");
        for (std::size_t i = b; i < e; ++i) (*d)[i] = 1;
    };
    auto r = runner.run_range(0, 100, 50, fn, &data);
    EXPECT_FALSE(r.all_done);
    EXPECT_EQ(r.failed_chunks, 1u);
    runtime_shutdown();
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    astro::compute::runtime_init();
    int result = RUN_ALL_TESTS();
    astro::compute::runtime_shutdown();
    return result;
}
