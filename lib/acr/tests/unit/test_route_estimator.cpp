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
    p.calibration_preset = "standard";
    p.calibration_head = "test-head";
    p.calibration_run_id = "test-run";
    p.generated_utc = "2026-08-09T00:00:00Z";
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
                s.cpu_items = n;
                s.cpu_chunks = 1;
                s.gpu_items = n;
                s.gpu_chunks = 1;
                s.timed_d2h_bytes = n * 4u;
                s.absolute_peak_vram_bytes = n * 4u;
                path.samples.push_back(s);
            }
        }
        path.model_available = true;
        path.model_trusted = true;
        path.eligible = true;
        path.min_output_items = items.front();
        path.max_output_items = items.back();
        path.frame_counts = frames;
        path.allow_tail_extrapolation = false;
        path.final_holdout_count = 8;
        path.final_median_error_ratio = 0.03;
        path.final_max_error_ratio = 0.05;
        path.median_error_ratio = 0.03;
        path.max_error_ratio = 0.05;
        path.p95_error_ratio = 0.05;
        path.metrics_complete = true;
    };
    auto make_scene = [&](const char* id) {
        RouteScenarioProfile sc;
        sc.scenario_id = id;
        fill_path(sc.openmp, 2.0);
        fill_path(sc.gpu_direct, 0.5);
        fill_path(sc.mixed, 0.8);
        sc.scenario_qualified = true;
        sc.routing_trusted = true;
        sc.qualification_reason = "test";
        sc.final_holdout_count = 8;
        sc.route_replay_count = 8;
        sc.route_replay_max_slowdown_ratio = 1.0;
        return sc;
    };
    op.scenarios.push_back(make_scene("cold_host_output"));
    op.scenarios.push_back(make_scene("resident_host_output"));
    op.scenarios.push_back(make_scene("resident_reuse4_host_output"));
    op.qualified = true;
    op.qualification_reason = "test";
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

// ===== Dispatcher Finalization（06/08 计划 B）：iterator 边界 =====
TEST(RouteEstimator, InterpolateE2eUnusableHighFrameReturnsFalse) {
    // frame_counts 含 32 帧（hi 边界），但 32 帧样本只剩 16M 单点，
    // 请求 1M×32 帧时该帧对请求 items 不可用 → usable 不含 32，
    // lower_bound == usable.end()。修复前 `it != frames.end()` 会在
    // usable.end() 上解引用（UB）；修复后必须安全返回 false。
    RouteProfileV2 p = make_test_profile();
    RoutePath path = p.operations.front().scenarios.front().gpu_direct;
    std::vector<RouteSamplePoint> kept;
    for (const auto& s : path.samples) {
        if (s.frame_count == 32u) {
            if (s.output_items == (1u << 24)) kept.push_back(s);
        } else {
            kept.push_back(s);
        }
    }
    path.samples = std::move(kept);
    path.frame_counts = {4u, 16u, 32u};
    double med = 0.0, p90 = 0.0;
    // 1M × 32 帧：usable={4,16}，lower_bound==end → 不崩溃、返回 false
    EXPECT_FALSE(BenchmarkRouteEstimator::interpolate_e2e(
        path, 1u << 20, 32u, med, p90));
    // 16M × 32 帧：32 帧单点精确命中 → true
    EXPECT_TRUE(BenchmarkRouteEstimator::interpolate_e2e(
        path, 1u << 24, 32u, med, p90));
}

TEST(RouteEstimator, InterpolateE2eAdaptiveSingleFrameExcluded) {
    // adaptive 加入的单点帧（12 帧）只覆盖小 items：请求大 items 时该帧
    // 不得参与相邻插值，退回 4/16/32 基础网格（BDR Reviewed 08 计划 F）。
    RouteProfileV2 p = make_test_profile();
    RoutePath path = p.operations.front().scenarios.front().gpu_direct;
    RouteSamplePoint single = path.samples.front();
    single.output_items = 1u << 18;   // 256K 单点（domain 最小值）
    single.frame_count = 12u;
    single.median_ms = 0.2;
    single.p90_ms = 0.22;
    path.samples.push_back(single);
    path.frame_counts.push_back(12u);
    double med = 0.0, p90 = 0.0;
    // 1M × 12 帧：12 帧单点不可用 → 用 4/16 帧相邻插值
    //（4 帧 0.125ms、16 帧 0.5ms，w=(12-4)/(16-4)=2/3 → 0.375ms）
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_e2e(
        path, 1u << 20, 12u, med, p90));
    EXPECT_NEAR(med, 0.125 + 0.375 * (2.0 / 3.0), 0.05);
    // 256K × 12 帧：12 帧单点精确命中
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_e2e(
        path, 1u << 18, 12u, med, p90));
    EXPECT_NEAR(med, 0.2, 1e-9);
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

// ===== BDR Reviewed（08 计划 F）：2D chunk 服务插值 =====
TEST(RouteEstimator, ChunkService2DInterpolatesBetweenFrames) {
    // 构造 4/16/32 帧曲线：chunk 256K/1M，时间随帧数线性增长
    std::vector<ChunkServicePoint> curve{
        {256u << 10, 4u, 0.5, 0.6, 7},
        {1024u << 10, 4u, 2.0, 2.4, 7},
        {256u << 10, 16u, 1.0, 1.2, 7},
        {1024u << 10, 16u, 4.0, 4.8, 7},
        {256u << 10, 32u, 2.0, 2.4, 7},
        {1024u << 10, 32u, 8.0, 9.6, 7},
    };
    double ms = 0.0, p90 = 0.0;
    // 12 帧 = 4 帧与 16 帧之间（w=(12-4)/(16-4)=2/3）
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_chunk_service(
        curve, 256u << 10, 12u, ms, p90));
    EXPECT_NEAR(ms, 0.5 + (2.0 / 3.0) * 0.5, 1e-9);
    EXPECT_NEAR(p90, 0.6 + (2.0 / 3.0) * 0.6, 1e-9);
    // 24 帧 = 16 帧与 32 帧之间（w=0.5）
    ASSERT_TRUE(BenchmarkRouteEstimator::interpolate_chunk_service(
        curve, 1024u << 10, 24u, ms, p90));
    EXPECT_NEAR(ms, 6.0, 1e-9);
}

TEST(RouteEstimator, ChunkServiceOutOfFrameRangeRejected) {
    std::vector<ChunkServicePoint> curve{
        {256u << 10, 4u, 0.5, 0.6, 7},
        {256u << 10, 16u, 1.0, 1.2, 7},
        {256u << 10, 32u, 2.0, 2.4, 7},
    };
    double ms = 0.0, p90 = 0.0;
    // 帧数超出已测范围：拒绝，不得混用全部曲线
    EXPECT_FALSE(BenchmarkRouteEstimator::interpolate_chunk_service(
        curve, 256u << 10, 48u, ms, p90));
    EXPECT_FALSE(BenchmarkRouteEstimator::interpolate_chunk_service(
        curve, 256u << 10, 2u, ms, p90));
    // chunk 超出已测范围：拒绝（不无条件外推）
    EXPECT_FALSE(BenchmarkRouteEstimator::interpolate_chunk_service(
        curve, 4096u << 10, 16u, ms, p90));
}

TEST(RouteEstimator, ChunkCurveSanityRejectsAnomaly) {
    // 审计示例：262K×4帧=2.01ms，262K×16帧=0.44ms（工作量 4 倍反而快 4.5 倍）
    std::vector<ChunkServicePoint> bad{
        {262u << 10, 4u, 2.01, 2.2, 7},
        {262u << 10, 16u, 0.44, 0.5, 7},
        {262u << 10, 32u, 0.88, 1.0, 7},
    };
    std::string reason;
    EXPECT_FALSE(BenchmarkRouteEstimator::chunk_curve_sanity(bad, reason));
    EXPECT_FALSE(reason.empty());
    // 正常曲线通过
    std::vector<ChunkServicePoint> good{
        {262u << 10, 4u, 0.5, 0.6, 7},
        {262u << 10, 16u, 2.0, 2.4, 7},
        {262u << 10, 32u, 4.0, 4.8, 7},
    };
    EXPECT_TRUE(BenchmarkRouteEstimator::chunk_curve_sanity(good, reason));
}

// ===== BDR Reviewed（08 计划 A）：场景级资格 =====
TEST(RouteEstimator, ScenarioNotQualifiedFallsBackOpenMP) {
    RouteProfileV2 p = make_test_profile();
    p.operations.front().scenarios[1].scenario_qualified = false;
    p.operations.front().scenarios[1].routing_trusted = false;
    p.operations.front().scenarios[1].qualification_reason =
        "replay-not-within-10";
    // 单测直接验证 decide 的场景级检查（生产先查 op.qualified 后查场景）；
    // 保持 op.qualified=true 以便命中 scenario-not-qualified 分支。
    BenchmarkRouteEstimator est;
    est.set_profile(&p);
    RouteDecision d = est.decide(make_request(4u << 20, 16u));
    // 生产模式：场景未 qualified → 只 OpenMP fallback
    EXPECT_EQ(d.chosen, RouteKind::OpenMP);
    EXPECT_EQ(d.reason, "scenario-not-qualified: resident_host_output");
    EXPECT_TRUE(d.openmp.feasible);
    EXPECT_FALSE(d.gpu_direct.feasible);
}

TEST(RouteEstimator, DiagnosticModeUsesAvailableUntrusted) {
    RouteProfileV2 p = make_test_profile();
    // GPU 模型未 trusted 但仍 available（最终误差超门）
    p.operations.front().scenarios[1].gpu_direct.model_trusted = false;
    p.operations.front().scenarios[1].gpu_direct.eligible = false;
    p.operations.front().scenarios[1].gpu_direct.reason =
        "final-holdout-error-limit";
    p.operations.front().qualified = false;
    BenchmarkRouteEstimator est;
    est.set_profile(&p);
    RouteRequest req = make_request(4u << 20, 16u);
    // 生产：gpu 不可用 → OpenMP
    RouteDecision prod = est.decide(req);
    EXPECT_FALSE(prod.gpu_direct.feasible);
    // 诊断：model_available 仍参加预测（用于发现模型不足）
    RouteDecision diag = est.decide(req, /*diagnostic=*/true);
    EXPECT_TRUE(diag.gpu_direct.feasible);
    EXPECT_TRUE(diag.mixed.feasible);
}
