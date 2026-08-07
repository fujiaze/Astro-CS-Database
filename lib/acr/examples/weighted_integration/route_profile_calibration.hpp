// lib/acr/examples/weighted_integration/route_profile_calibration.hpp
//
// ACR Benchmark 驱动路由（控制包 d026ea30...c178537）：
// 加权积分 Operation Route Profile v2 标定。
//
// 生成内容（08 计划 A/B/C）：
//   - 4 场景（cold_host_output / resident_host_output /
//     resident_device_output / resident_reuse4_host_output）的
//     OpenMP E2E、GPU Direct E2E、Mixed E2E 原始样本；
//   - CPU/GPU chunk 服务曲线（候选块真实执行）；
//   - holdout（不参与拟合）预测误差；
//   - validated domain、指纹与 Profile v2 序列化。
#pragma once

#include "../../routing/route_profile_v2.hpp"
#include "../../backends/cuda/bridge/cuda_bridge_api.hpp"

#include <cstdint>
#include <string>

namespace astro::compute::weighted_integration {

struct CalibrationEnv {
    std::string cpu_fingerprint;
    std::string gpu_name;
    std::string compiler;
    std::string kernel_hash;
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

} // namespace astro::compute::weighted_integration
