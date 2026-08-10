// lib/acr/qualification/profile_generator.hpp — 从 benchmark 结果生成画像
// Phase E3：聚合样本 → 生成 HardwareProfile → 序列化 hardware-profile.json（多维能力曲线）。
//
// 设计：
//   1. profile_generator 不修改 benchmark 原始样本，仅做 median/stddev 聚合
//   2. 设备指纹：调用 generate_hardware_report 提取关键字段并 SHA-256
//   3. JSON 序列化手写（与 diagnostics/hardware_report.cpp 风格一致，无第三方依赖）
//   4. hardware-profile.json 是只读档案：运行时不修改
//   5. benchmark 结果按 kernel 类型映射到对应能力曲线族（Copy/Triad→memory，AXPY→arithmetic，Dot→reduction）
#pragma once

#include "profile_schema.hpp"
#include "astro/compute/hardware_profile.hpp"

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

    // ===== Phase E3：生成 HardwareProfile（hardware-profile.json，新权威路径）=====
    // 从 benchmark 结果 + 当前硬件生成 HardwareProfile。
    // benchmark 结果按 kernel 类型映射到能力曲线族：
    //   Copy/Triad  → memory curve（MainMem:host:copy/triad）
    //   AXPY        → arithmetic curve（fp32:add:baseline）
    //   Dot         → reduction curve（dot:fp32）
    //   其他 kernel → arithmetic curve（兜底，按 precision 分配）
    // GPU backend 结果 → device_id=1+，曲线按 backend 名归入对应 DeviceProfile
    // 同时填充固定开销（submit/launch/event/alloc/merge，用保守估算）
    HardwareProfile generate_hardware_profile(const std::vector<KernelBenchmarkResult>& results,
                                              ProfileKind kind) const;

    // 序列化 HardwareProfile 为 JSON 字符串（hardware-profile.json 格式）
    static std::string serialize_hardware_profile(const HardwareProfile& hp);

    // 写入 hardware-profile.json 到文件（覆盖）
    static bool write_hardware_profile_to_file(const std::string& path,
                                                const HardwareProfile& hp);

private:
    // 聚合样本为 median / stddev
    static void aggregate(KernelBenchmarkResult& r);

    // 从当前硬件提取设备指纹
    DeviceFingerprint build_fingerprint() const;

    // Phase E3：从 benchmark 结果构建 DeviceProfile 列表（按 backend 分组）
    // CPU backend ("cpu") → device_id=0；GPU backend ("cuda:N") → device_id=N+1
    std::vector<DeviceProfile> build_device_profiles(
        const std::vector<KernelBenchmarkResult>& results,
        ProfileKind kind) const;

    // Phase E3：将单个 KernelBenchmarkResult 映射到 DeviceProfile 的能力曲线
    // 按 kernel_id 决定族与 key，更新 device 的对应曲线
    void map_result_to_curves(DeviceProfile& device,
                              const KernelBenchmarkResult& r,
                              ProfileKind kind) const;

    // Phase E3：填充固定开销（保守估算，因当前 benchmark_driver 不测 overhead）
    void fill_default_overheads(DeviceProfile& device) const;
};

} // namespace astro::compute::qualification
