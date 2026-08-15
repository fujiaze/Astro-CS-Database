// lib/acr/profile/profile_reader.hpp — HardwareProfileReader（运行时只读）
// Phase E2：读取 compute_profiles/<fingerprint>/hardware-profile.json
//
// 设计（ 06_QUALIFICATION_BENCHMARK_SPEC.md §15 + 07_STATIC_ROUTING_AND_MIXED_EXECUTION.md §9）：
// 1. 三态处理：
// - Missing（无 hardware-profile.json）：CPU fallback + 警告 "未标定，使用 CPU baseline"
// - Stale（指纹不匹配）：警告 "profile 过期" + 继续运行（不强制重新 benchmark）
// - Corrupt（JSON 解析失败）：警告 "profile 损坏" + CPU fallback
// 2. 运行时只读：不在线修改 profile，不在线学习
// 3. lazy load：首次 CostEstimator 调用时加载，之后内存缓存（线程安全）
// 4. 公共头不暴露第三方类型（手写极简 JSON 解析，无 nlohmann/json 依赖）
// 5. invalidate_cache 用于 acr-invalidate 后重新加载
#pragma once

#include "astro/compute/hardware_profile.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

namespace astro::compute::profile {

// ===== HardwareProfileReader =====
// 线程安全（内部 mutex 保护 lazy load）。profile 加载后只读。
class HardwareProfileReader {
public:
    HardwareProfileReader();
    ~HardwareProfileReader();

    // 指定 hardware-profile.json 路径（默认 "./hardware-profile.json"）
    // 必须在 get_profile 前调用；首次 get_profile 后再设置无效
    void set_profile_path(const std::string& path);

    // 强制重新加载 profile（用于 acr-invalidate 后重新读取）
    void invalidate_cache();

    // 获取已加载的 profile（首次调用触发 lazy load）
    // 返回 nullptr 表示 profile 未加载（Missing/Corrupt）
    // 三态通过 profile_state() 查询
    const HardwareProfile* get_profile();

    // 当前 profile 状态（不触发加载，仅在已加载时返回实际状态；否则返回 Missing）
    HwProfileState profile_state() const noexcept;

    // 是否已加载（不触发加载）
    bool loaded() const noexcept;

    // 当前 profile 路径
    const std::string& profile_path() const noexcept;

    // 生成 status JSON（acr-status 工具用）
    std::string status_json() const;

    // ===== 降级 API：无画像时返回 CPU-only fallback profile =====
    // 返回的 profile 包含一个 CPU DeviceProfile（无能力曲线），state=Missing
    // CostEstimator 据此走 CPU fallback + 警告
    const HardwareProfile& get_profile_or_cpu_fallback();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ===== 全局单例（CostEstimator 使用）=====
// 线程安全。首次调用触发 lazy load（默认路径 ./hardware-profile.json）。
HardwareProfileReader& global_profile_reader();

// ===== 工具：构造 CPU-only fallback profile（无画像时用）=====
HardwareProfile make_cpu_fallback_profile();

// ===== 工具：从 hardware_report JSON 提取设备指纹 SHA-256 =====
// 与 qualification::ProfileGenerator::build_fingerprint 一致逻辑
std::string compute_fingerprint_sha256();

} // namespace astro::compute::profile
