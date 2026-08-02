// lib/acr/qualification/profile_generator.hpp — 从 benchmark 结果生成 routes.json
// Phase E：聚合样本 → 选最优 backend → 生成 ProfileBundle → 序列化 JSON。
//
// 设计：
//   1. profile_generator 不修改 benchmark 原始样本，仅做 median/stddev 聚合
//   2. 路由选择：每个 (kernel, precision) 选 throughput 最大的 backend；无 backend 时回退 CPU
//   3. 设备指纹：调用 generate_hardware_report 提取关键字段并 SHA-256
//   4. JSON 序列化手写（与 diagnostics/hardware_report.cpp 风格一致，无第三方依赖）
//   5. 生成 routes.json 是只读档案：运行时不修改
#pragma once

#include "profile_schema.hpp"

#include <string>
#include <vector>

namespace astro::compute {
class CpuIsaCaps;  // forward declared from topology.hpp
}

namespace astro::compute::qualification {

// ===== ProfileGenerator =====
class ProfileGenerator {
public:
    ProfileGenerator();
    ~ProfileGenerator();

    // 从 benchmark 结果 + 当前硬件生成 ProfileBundle
    // 硬件指纹从 topology 获取（CPU 型号/核心/ISA/GPU）
    ProfileBundle generate(const std::vector<KernelBenchmarkResult>& results,
                           ProfileKind kind) const;

    // 序列化为 JSON 字符串（routes.json 格式）
    static std::string serialize(const ProfileBundle& bundle);

    // 写入文件（覆盖）
    static bool write_to_file(const std::string& path, const ProfileBundle& bundle);

private:
    // 聚合样本为 median / stddev
    static void aggregate(KernelBenchmarkResult& r);

    // 从当前硬件提取设备指纹
    DeviceFingerprint build_fingerprint() const;
};

} // namespace astro::compute::qualification
