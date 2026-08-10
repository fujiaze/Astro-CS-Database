// lib/acr/tests/unit/test_route_calibration.cpp
//
// 标定评估纯函数测试（Dispatcher Finalization 08 计划 A / 07 测试 A）：
//   - Probe 阶段允许选择插值模型；
//   - Final 阶段绝对不改 interpolation_id / samples / adaptive_rounds；
//   - Final 即使实际值变化也不重新选择模型。
#include <gtest/gtest.h>

#include "route_profile_calibration.hpp"

#include "routing/route_profile_v2.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace astro::compute::weighted_integration;
using astro::compute::routing::RoutePath;
using astro::compute::routing::RouteSamplePoint;

namespace {

// 构造一条覆盖 256K/1M/4M/16M × 4/16/32 帧网格的路径。
// 实际时间服从幂律 t = c * items^1.5 * (frames/16)^0.5（loglog 模型应更优）。
RoutePath make_grid_path() {
    RoutePath path;
    const std::vector<std::uint64_t> items{1u << 18, 1u << 20, 1u << 22,
                                           1u << 24};
    const std::vector<std::uint32_t> frames{4u, 16u, 32u};
    const double c = 5.82e-9;
    for (std::uint64_t n : items) {
        for (std::uint32_t f : frames) {
            RouteSamplePoint s;
            s.output_items = n;
            s.frame_count = f;
            s.reuse_count = 1;
            s.median_ms =
                c * std::pow(static_cast<double>(n), 1.5) *
                std::sqrt(static_cast<double>(f) / 16.0);
            s.p90_ms = s.median_ms * 1.1;
            s.cpu_items = n;
            s.cpu_chunks = 1;
            s.gpu_items = 0;
            s.gpu_chunks = 0;
            s.absolute_peak_vram_bytes = 0;
            path.samples.push_back(s);
        }
    }
    path.min_output_items = items.front();
    path.max_output_items = items.back();
    path.frame_counts = frames;
    path.interpolation_id = "piecewise-linear-items-frames";
    return path;
}

RouteEvalPoint make_point(std::uint64_t items, std::uint32_t frames,
                          double ms) {
    RouteEvalPoint p;
    p.items = items;
    p.frames = frames;
    p.sample.output_items = items;
    p.sample.frame_count = frames;
    p.sample.median_ms = ms;
    p.sample.p90_ms = ms * 1.1;
    return p;
}

} // anonymous namespace

TEST(RouteCalibration, ProbeSelectsBetterInterpolator) {
    RoutePath path = make_grid_path();
    // probe 点由同一幂律生成：loglog 插值应明显优于 linear
    std::vector<RouteEvalPoint> probe{
        make_point(1u << 19, 8u, 5.82e-9 * std::pow(1u << 19, 1.5) *
                                        std::sqrt(8.0 / 16.0)),
        make_point(1u << 21, 24u, 5.82e-9 * std::pow(1u << 21, 1.5) *
                                         std::sqrt(24.0 / 16.0)),
        make_point(1u << 19, 24u, 5.82e-9 * std::pow(1u << 19, 1.5) *
                                         std::sqrt(24.0 / 16.0)),
        make_point(1u << 21, 8u, 5.82e-9 * std::pow(1u << 21, 1.5) *
                                        std::sqrt(8.0 / 16.0)),
    };
    const std::string frozen = path.interpolation_id;
    RouteErrorEval ev = select_model_on_probe(path, probe);
    EXPECT_GT(ev.count, 0u);
    // Probe 允许修改 interpolation_id（选择更优模型）
    EXPECT_EQ(path.interpolation_id, "piecewise-loglog-items-frames-time");
    EXPECT_NE(path.interpolation_id, frozen);
}

TEST(RouteCalibration, FinalHoldoutNeverMutatesModel) {
    RoutePath path = make_grid_path();
    std::vector<RouteEvalPoint> probe{
        make_point(1u << 19, 8u, 0.30),
        make_point(1u << 21, 24u, 2.60),
    };
    select_model_on_probe(path, probe);

    // 冻结模型状态
    const std::string frozen_interp = path.interpolation_id;
    const std::vector<RouteSamplePoint> frozen_samples = path.samples;
    const std::uint32_t frozen_adaptive = path.adaptive_rounds_used;
    const std::uint64_t frozen_min = path.min_output_items;
    const std::uint64_t frozen_max = path.max_output_items;
    const std::vector<std::uint32_t> frozen_frames = path.frame_counts;

    std::vector<RouteEvalPoint> final_pts{
        make_point(786432u, 12u, 0.85),
        make_point(2359296u, 20u, 4.20),
        make_point(6553600u, 20u, 11.0),
        make_point(9437184u, 12u, 15.0),
        make_point(12845056u, 28u, 22.0),
        make_point(16777216u, 24u, 28.0),
        make_point(1048576u, 10u, 1.30),
        make_point(1638400u, 20u, 2.80),
    };
    RouteErrorEval ev = evaluate_fixed_model_on_final(path, final_pts);
    EXPECT_EQ(ev.count, final_pts.size());
    // Final 绝不修改任何模型字段
    EXPECT_EQ(path.interpolation_id, frozen_interp);
    EXPECT_EQ(path.samples, frozen_samples);
    EXPECT_EQ(path.adaptive_rounds_used, frozen_adaptive);
    EXPECT_EQ(path.min_output_items, frozen_min);
    EXPECT_EQ(path.max_output_items, frozen_max);
    EXPECT_EQ(path.frame_counts, frozen_frames);
}

TEST(RouteCalibration, FinalDoesNotReselectEvenWhenActualsShift) {
    RoutePath path = make_grid_path();
    // 强制冻结为 linear（模拟 Probe 选择结果）
    path.interpolation_id = "piecewise-linear-items-frames";
    const std::string frozen_interp = path.interpolation_id;

    std::vector<RouteEvalPoint> final_pts{
        make_point(786432u, 12u, 0.85),
        make_point(2359296u, 20u, 4.20),
        make_point(6553600u, 20u, 11.0),
        make_point(9437184u, 12u, 15.0),
        make_point(12845056u, 28u, 22.0),
        make_point(16777216u, 24u, 28.0),
        make_point(1048576u, 10u, 1.30),
        make_point(1638400u, 20u, 2.80),
    };
    evaluate_fixed_model_on_final(path, final_pts);
    // 即使这些 actual 使 loglog 在"若重新选择"时更优，Final 也不改模型
    EXPECT_EQ(path.interpolation_id, frozen_interp);
}

TEST(RouteCalibration, ErrorGateConstants) {
    EXPECT_TRUE(route_gate_passed_errors(0.10, 0.15));
    EXPECT_TRUE(route_gate_passed_errors(0.09, 0.15));
    EXPECT_FALSE(route_gate_passed_errors(0.11, 0.15));
    EXPECT_FALSE(route_gate_passed_errors(0.10, 0.16));
}
