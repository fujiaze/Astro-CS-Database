// astro/compute/topology.hpp — ACR 硬件发现与 ISA 安全门禁公共 API
// Phase C：hwloc 拓扑/NUMA/PCI + cpu_features ISA 检测 + 设备指纹。
//
// 设计（ADR-003 hwloc / ADR-004 cpu_features）：
//   1. 公共头不暴露 hwloc / cpu_features 类型（PIMPL + 自有 IsaLevel mask）
//   2. 无 hwloc 时降级返回 {"status":"unavailable"}，不抛异常（ADR-009 降级策略）
//   3. cpu_features 缺失时用 __builtin_cpu_supports 降级（仅 GCC/Clang）
//   4. has_isa() 安全门禁：加载 ISA 插件前必须校验，不支持永不执行
//   5. AVX-512 子集以独立 bit 表达，禁止合并为单一 "AVX-512" 标志（ADR-004）
//   6. baseline 路径（无任何扩展）永远可用，不依赖任何 ISA 检测
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace astro::compute {

// ===== ISA 等级（位掩码，AVX-512 子集独立 bit）=====
enum class IsaLevel : std::uint64_t {
    None      = 0,
    SSE       = 1ULL << 0,
    SSE2      = 1ULL << 1,
    SSE3      = 1ULL << 2,
    SSSE3     = 1ULL << 3,
    SSE41     = 1ULL << 4,
    SSE42     = 1ULL << 5,
    AVX       = 1ULL << 6,
    AVX2      = 1ULL << 7,
    FMA       = 1ULL << 8,
    AVX512F   = 1ULL << 9,
    AVX512CD  = 1ULL << 10,
    AVX512BW  = 1ULL << 11,
    AVX512DQ  = 1ULL << 12,
    AVX512VL  = 1ULL << 13,
};

constexpr IsaLevel operator|(IsaLevel a, IsaLevel b) noexcept {
    return static_cast<IsaLevel>(static_cast<std::uint64_t>(a) | static_cast<std::uint64_t>(b));
}
constexpr IsaLevel operator&(IsaLevel a, IsaLevel b) noexcept {
    return static_cast<IsaLevel>(static_cast<std::uint64_t>(a) & static_cast<std::uint64_t>(b));
}
constexpr bool any(IsaLevel a) noexcept { return static_cast<std::uint64_t>(a) != 0; }

// ===== CpuIsaCaps：CPU ISA 能力 mask + 安全门禁 =====
// 简单值类型，无第三方依赖。检测在构造时完成（cpu_features 或 __builtin_cpu_supports）。
class CpuIsaCaps {
public:
    CpuIsaCaps() noexcept;
    ~CpuIsaCaps() = default;

    // 安全门禁：查询是否支持某个 ISA level（单个 bit）。
    // 传入组合 mask 时，仅当全部 bit 都支持才返回 true。
    bool has(IsaLevel level) const noexcept;
    // has() 的语义别名，供 ISA 插件加载入口调用（ADR-004 门禁边界）。
    bool has_isa(IsaLevel level) const noexcept { return has(level); }

    std::uint64_t mask() const noexcept { return mask_; }
    std::string to_json() const;

private:
    std::uint64_t mask_{0};
};

// ===== HwlocTopology：hwloc 拓扑封装（PIMPL）=====
// 枚举 package/core/PU/cache/NUMA/PCI，提供 JSON 序列化。
// 无 hwloc 时 available()=false，to_json() 返回 {"status":"unavailable"}。
class HwlocTopology {
public:
    HwlocTopology();
    ~HwlocTopology();
    HwlocTopology(HwlocTopology&&) noexcept;
    HwlocTopology& operator=(HwlocTopology&&) noexcept;
    HwlocTopology(const HwlocTopology&) = delete;
    HwlocTopology& operator=(const HwlocTopology&) = delete;

    bool available() const noexcept;
    std::string to_json() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ===== 自由函数：设备指纹 =====
// detect_topology()：hwloc JSON（CPU vendor/model、core 列表、NUMA、cache 层级、PCI）。
//                     无 hwloc 返回 {"status":"unavailable"}，不抛。
std::string detect_topology();

// detect_isa_caps()：CPU ISA 能力 mask JSON（SSE/AVX/AVX2/FMA/AVX-512 子集）。
std::string detect_isa_caps();

// GPU 描述回调（Phase D 注册，Phase C 仅声明接口）。
// 回调返回 GPU UUID/PCI/driver/SM 等 JSON 片段；未注册时返回 "null"。
using GpuReportCallback = std::string(*)();

// 注册 GPU 报告回调（Phase D 调用）。线程安全，仅首次生效。
void register_gpu_report_callback(GpuReportCallback cb);

// 重置 GPU 回调为 nullptr（仅供单元测试隔离全局状态，正式运行不得调用）。
void reset_gpu_report_callback_for_testing();

// generate_hardware_report()：合并 hwloc + cpu_features + GPU 回调为完整 hardware.json。
// schema：CPU vendor/model/stepping/ISA mask/cache/NUMA/GPU UUID/PCI/driver/compiler/build。
std::string generate_hardware_report();

} // namespace astro::compute
