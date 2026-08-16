// lib/acr/examples/weighted_integration/route_profile_calibration.hpp
//
// ACR Benchmark 驱动路由（ d026ea30...c178537）：
// 加权积分 Operation Route Profile v2 标定。
//
// 生成内容（08 计划 A/B/C）：
// - 4 场景（cold_host_output / resident_host_output /
// resident_device_output / resident_reuse4_host_output）的
// OpenMP E2E、GPU Direct E2E、Mixed E2E 原始样本；
// - CPU/GPU chunk 服务曲线（候选块真实执行）；
// - holdout（不参与拟合）预测误差；
// - validated domain、指纹与 Profile v2 序列化。
#pragma once

#include "../../routing/route_profile_v2.hpp"
#include "../../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace astro::compute::weighted_integration {

struct CalibrationEnv {
    std::string cpu_fingerprint;
    std::string gpu_name;
    std::string compiler;
    std::string kernel_hash;
    // 权威发布元数据（05_PROFILE_PUBLICATION.md）
    std::string calibration_preset;   // standard 才可发布；quick 写 tmp
    std::string calibration_head;     // 生成 Profile 的 git HEAD
    std::string calibration_run_id;   // 唯一运行标识
    int openmp_threads{1};
    bool gpu_available{false};
};

// 标定入口：生成加权积分 Profile v2（standard 标定矩阵 + holdout）。
// profile 输出到 profile_out_path（含 .chunk_candidates 独立报告）。
// 返回 nullopt 表示标定失败（GPU 不可用等）。
bool calibrate_route_profile_v2(
    const CalibrationEnv& env,
    void* gpu_handle,                    // 桥接 executor（可为 null）
    astro::compute::cuda::bridge::BridgeApi* bapi,  // 可为 null
    const std::string& output_path,      // operation-route-profile-v2.json
    astro::compute::routing::RouteProfileV2& out);

// ===== 标定评估纯函数（04_PROFILE_CALIBRATION_AND_VALIDATION.md）=====
// 供单测直接验证"Probe 可选模型 / Final 绝对不改模型"语义。

// 一个验证点（Probe 或 Final 共用）：坐标 + 该点实际中位样本。
struct RouteEvalPoint {
    std::uint64_t items{0};
    std::uint32_t frames{0};
    routing::RouteSamplePoint sample;
};

// 误差评估结果。
struct RouteErrorEval {
    double median{0.0};
    double max{0.0};
    std::size_t count{0};
    std::size_t worst_index{0};
    double worst_error{0.0};
};

// 候选插值器预测（linear/loglog；返回 -1 = 范围外）。供标定内部与测试使用。
double predict_path(const routing::RoutePath& path, std::uint64_t items,
                    std::uint32_t frames, const std::string& interp_id);

// 误差门：median<=10%、max<=15%（04 号规范 F；禁止放宽）。
bool route_gate_passed_errors(double median, double max);

// Probe 阶段：在候选插值器（linear/loglog）中按中位误差选择并写回
// path.interpolation_id。允许修改模型；返回误差统计（worst 供补点）。
RouteErrorEval select_model_on_probe(
    routing::RoutePath& path,
    const std::vector<RouteEvalPoint>& probe);

// Final 阶段：使用已冻结的 path.interpolation_id 评估。
// 禁止修改 interpolation_id / samples / validated domain / adaptive_rounds。
RouteErrorEval evaluate_fixed_model_on_final(
    const routing::RoutePath& path,
    const std::vector<RouteEvalPoint>& final_pts);

} // namespace astro::compute::weighted_integration
