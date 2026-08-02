// lib/acr/routing/route_profile.hpp — 运行时只读路由 profile 数据结构
// Phase E：StaticRouteResolver 的输入数据契约（routes.json 反序列化产物）。
//
// 设计（控制包 06_STATIC_ROUTING_SPEC.md）：
//   1. 运行时只读：正式运行不修改 profile，不在线学习
//   2. profile 三态：Missing / Stale / Corrupt / Valid
//   3. 设备指纹：CPU 型号 + 核心数 + ISA + GPU 型号 + 显存 + 驱动版本，SHA-256 哈希
//   4. stale 检测：当前指纹 != profile 指纹（不强制重新 benchmark，仅警告）
//   5. corrupt 检测：JSON 解析失败或 schema_version 不匹配
//   6. 公共头不暴露第三方类型（手写解析，无 nlohmann/json 依赖）
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::routing {

// 复用 qualification 的 ProfileState / ProfileKind 定义（避免重复）
// 这里用独立枚举以保持 routing 模块自包含（不依赖 qualification 头）
enum class ProfileState : std::uint8_t {
    Missing  = 0,
    Stale    = 1,
    Corrupt  = 2,
    Valid    = 3,
};

const char* profile_state_str(ProfileState s) noexcept;

// ===== 路由条目（运行时视角，只读）=====
struct RouteEntryView {
    std::uint32_t kernel_id{0};
    std::string kernel_name;
    std::string precision;
    std::string preferred_backend;   // profile 推荐的 backend
    double expected_throughput_gbps{0.0};
    std::string reason;
};

// ===== 设备指纹（运行时视角，只读）=====
struct DeviceFingerprintView {
    std::string cpu_model;
    std::uint32_t cpu_cores{0};
    std::uint64_t isa_mask{0};
    std::string gpu_name;
    std::uint64_t gpu_memory_bytes{0};
    std::string gpu_driver_version;
    std::string sha256;
};

// ===== Profile（运行时视角，只读档案）=====
struct RouteProfile {
    std::string schema_version;
    std::string generated_at;
    std::string profile_kind;          // "quick" / "standard" / "full"
    DeviceFingerprintView fingerprint;
    std::vector<RouteEntryView> routes;
    // raw_results 字段在 routes.json 中可选，运行时不解析（不暴露）
};

// ===== JSON 反序列化（手写极简解析器，仅支持 routes.json 子集）=====
// 解析失败时返回 false（不抛异常，corrupt 由调用方判定）。
// 注意：此解析器只处理 schema_version / generated_at / profile_kind /
//       fingerprint / routes 字段，raw_results 不解析（节省内存）。
bool parse_route_profile(const std::string& json, RouteProfile& out) noexcept;

// ===== 指纹比较（用于 stale 检测）=====
// 仅比较 sha256 字段（指纹本身已是关键字段的哈希）。
bool fingerprint_matches(const DeviceFingerprintView& a,
                         const DeviceFingerprintView& b) noexcept;

// ===== 当前设备指纹查询（调用 topology）=====
DeviceFingerprintView query_current_fingerprint();

} // namespace astro::compute::routing
