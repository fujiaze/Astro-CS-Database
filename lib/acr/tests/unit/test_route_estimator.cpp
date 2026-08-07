// lib/acr/tests/unit/test_route_estimator.cpp
//
// BenchmarkRouteEstimator 单测（08 计划 D/F）：
//   - 二维插值与范围检查；
//   - Mixed 共享池模拟；
//   - 最低 score 选择、queue delay 切换、VRAM 不足、无画像安全回退。
#include <gtest/gtest.h>

#include "routing/benchmark_route_estimator.hpp"
#include "routing/route_profile_v2.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace astro::compute::routing;

namespace {

// 构造测试 Profile：
//   OpenMP ~2ns/item、GPU Direct ~0.5ns/item、Mixed ~0.8ns/item（帧数线性）。
//   chunk 服务曲线：CPU 64K=0.5ms / 256K=1.5ms；GPU 256K=0.8ms / 1M=2.0ms。
RouteProfileV2 make_test_profile() {
    RouteProfileV2 p;
    p.schema_version = "acr-operation-route-profile-2";
    p.profile_state = "qualified";
    p.fingerprint_cpu = "test-cpu";
    p.fingerprint_compiler = "test";
    p.fingerprint_runtime_kernel_hash = "0123456789abcdef0123456789abcdef";

    OperationRouteProfile op;
    op.operation_id = "synthetic.weighted_integration.fp64acc";
    op.cpu_chunk_candidates = {1u << 16, 1u << 18};
    op.gpu_chunk_candidates = {1u << 18, 1u << 20};
    op.mixed_fixed_overhead_ms = 0.5;
    op.mixed_per_token_ms = 0.001;
    op.qualified = true;
    op.qualification_reason = "test";
    op.cpu_chunk_service.push_back({1u << 16, 16u, 0.5});
    op.cpu_chunk_service.push_back({1u << 18, 16u, 1.5});
    op.gpu_chunk_service.push_back({1u << 18, 16u, 0.8});
    op.gpu_chunk_service.push_back({1u << 20, 16u, 2.0});

    RouteScenarioProfile sc;
    sc.scenario_id = "resident_host_output";
    const std::vector<std::uint64_t> items{
        1u << 18, 1u << 20, 1u << 22, 1u << 24};
    const std::vector<std::uint32_t> frames{4u, 16u, 32u};
    auto fill_path = [&](RoutePath& path, double ns_per_item) {
        for (std::uint64_t n : items) {
            for (std::uint32_t f : frames) {
                RouteSamplePoint s;
                s.output_items = n;
                s.frame_count = f;
                s.reuse_count = 1;
                s.input_bytes = f * n * 4u + f * 4u;
                s.output_bytes = n * 4u;
                s.median_ms = n * ns_per_item * 1e-6 *
                              (static_cast<double>(f) / 16.0);
                s.p90_ms = s.median_ms * 1.1;
                path.samples.push_back(s);
            }
        }
        path.eligible = true;
        path.min_output_items = items.front();
        path.max_output_items = items.back();
        path.frame_counts = frames;
        path.allow_tail_extrapolation = false;
        path.median_error_ratio = 0.03;
        path.p95_error_ratio = 0.05;
    };
    fill_path(sc.openmp, 2.0);
    fill_path(sc.gpu_direct, 0.5);
    fill_path(sc.mixed, 0.8);
    op.scenarios.push_back(std::move(sc));
    p.operations.push_back(std::move(op));
    return p;
}

RouteRequest make_request(std::uint64_t items, std::uint32_t frames) {
    RouteRequest req;
    req.operation_id = "synthetic.weighted_integration.fp64acc";
    req.output_items = items;
    req.frame_count = frames;
    req.input_bytes = frames * items * 4u + frames * 4u;
    req.output_bytes = items * 4u;
    req.input_residency = InputResidency::DeviceResident;
    req.output_policy = OutputMaterialization::HostRequired;
    req.reuse_count_hint = 1;
    return req;
}

} // anonymous namespace

TEST(RouteEstimator, InterpolateKnownPoint) {
    RouteProfileV2 p = make_test_profile();
    const OperationRouteProfile& op = p.operations.front();
    const RouteScenarioProfile& sc = op.scenarios.front();
    double med = 0.0, p90 = 0.0;
    // 1M items × 16 frames：GPU 0.5ns*1M = 0.5ms
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_e2e(
        sc.gpu_direct, 1u << 20, 16u, med, p90));
    EXPECT_NEAR(med, 0.5, 0.05);
}

TEST(RouteEstimator, InterpolateBetweenFrames) {
    RouteProfileV2 p = make_test_profile();
    const OperationRouteProfile& op = p.operations.front();
    const RouteScenarioProfile& sc = op.scenarios.front();
    double med = 0.0, p90 = 0.0;
    // 1M items × 10 frames：介于 4（0.125ms）与 16（0.5ms）之间
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_e2e(
        sc.gpu_direct, 1u << 20, 10u, med, p90));
    EXPECT_NEAR(med, 0.125 + 0.375 * (6.0 / 12.0), 0.05);
}

TEST(RouteEstimator, OutOfDomainNotExtrapolated) {
    RouteProfileV2 p = make_test_profile();
    const OperationRouteProfile& op = p.operations.front();
    const RouteScenarioProfile& sc = op.scenarios.front();
    double med = 0.0, p90 = 0.0;
    // 低于最小样本：不无条件外推
    EXPECT_FALSE(BenchmarkRouteEstimator::interpolate_e2e(
        sc.gpu_direct, 1u << 16, 16u, med, p90));
}

TEST(RouteEstimator, MixedSimulationProducesMakespan) {
    RouteProfileV2 p = make_test_profile();
    const OperationRouteProfile& op = p.operations.front();
    std::uint64_t cpu_chunk = 0, gpu_chunk = 0;
    const double makespan = BenchmarkRouteEstimator::simulate_mixed(
        op.cpu_chunk_service, op.gpu_chunk_service,
        4u << 20, 16u, 0.0, 0.0, cpu_chunk, gpu_chunk);
    ASSERT_GT(makespan, 0.0);
    EXPECT_GT(cpu_chunk, 0u);
    EXPECT_GT(gpu_chunk, 0u);
}

TEST(RouteEstimator, DecidePrefersFasterGpuDirect) {
    RouteProfileV2 p = make_test_profile();
    BenchmarkRouteEstimator est;
    est.set_profile(&p);
    RouteDecision d = est.decide(make_request(4u << 20, 16u));
    EXPECT_EQ(d.chosen, RouteKind::GpuDirect);
    EXPECT_TRUE(d.gpu_direct.feasible);
    EXPECT_LT(d.gpu_direct.score_ms, d.openmp.score_ms);
}

TEST(RouteEstimator, QueueDelaySwitchesToOpenMP) {
    RouteProfileV2 p = make_test_profile();
    BenchmarkRouteEstimator est;
    est.set_profile(&p);
    RouteRequest req = make_request(4u << 20, 16u);
    req.queues.gpu_delay_ms = 100.0;  // GPU 排队 100ms
    RouteDecision d = est.decide(req);
    EXPECT_EQ(d.chosen, RouteKind::OpenMP);
}

TEST(RouteEstimator, VramInsufficientExcludesGpu) {
    RouteProfileV2 p = make_test_profile();
    BenchmarkRouteEstimator est;
    est.set_profile(&p);
    RouteRequest req = make_request(4u << 20, 16u);
    req.memory.vram_available_bytes = 1024;  // 不足
    RouteDecision d = est.decide(req);
    EXPECT_FALSE(d.gpu_direct.feasible);
    EXPECT_FALSE(d.mixed.feasible);
    EXPECT_EQ(d.chosen, RouteKind::OpenMP);
}

TEST(RouteEstimator, NoProfileFallsBackToOpenMP) {
    BenchmarkRouteEstimator est;
    est.set_profile(nullptr);
    RouteDecision d = est.decide(make_request(4u << 20, 16u));
    EXPECT_EQ(d.chosen, RouteKind::OpenMP);
    EXPECT_TRUE(d.openmp.feasible);
    EXPECT_EQ(d.reason, "no-route-profile");
}

TEST(RouteProfileV2, SerializeContainsKeyFields) {
    RouteProfileV2 p = make_test_profile();
    std::string err;
    ASSERT_TRUE(validate_route_profile_v2(p, err)) << err;
    const std::string s = serialize_route_profile_v2(p);
    EXPECT_FALSE(s.empty());
    EXPECT_NE(s.find("acr-operation-route-profile-2"), std::string::npos);
    EXPECT_NE(s.find("resident_host_output"), std::string::npos);
}
