// lib/acr/cost/cost_estimator.hpp — ACR 成本估算器
// Phase F1+F2：根据 TaskDescriptor + HardwareProfile 推算各设备成本与块大小。
//
// 设计（控制包 07_STATIC_ROUTING_AND_MIXED_EXECUTION.md §3-§4）：
//   1. 成本模型：T_device(chunk) = queue_wait + launch_or_submit + transfer_if_needed
//                                    + compute_from_profile + local_merge_or_sync
//   2. 无画像时 CPU fallback：用保守峰值带宽/overhead 估算 + 警告（非阻断）
//   3. 最小有效块（Phase F2）：满足计算时间 > N×launch 开销、GPU 传输可被收益覆盖、
//      不超过 RAM/VRAM、Tile 边界合法
//   4. CostEstimator 不拥有 profile，只读引用 HardwareProfileReader 的结果
//   5. 公共头不暴露第三方类型
//   6. 线程安全：所有方法 const，无内部可变状态
#pragma once

#include "astro/compute/hardware_profile.hpp"
#include "astro/compute/task_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute {

// 前向声明（完整类型在 core/task_descriptor.hpp）
struct TaskDescriptor;

namespace profile { class HardwareProfileReader; }

namespace cost {

// ===== DeviceId 复用（与 hardware_profile.hpp 一致）=====
using ::astro::compute::DeviceId;

// ===== 单设备成本估算结果 =====
struct DeviceCost {
    DeviceId device_id{kHwInvalidDeviceId};
    std::string backend;                    // "cpu" / "cuda:0" / ...
    std::string device_name;

    // 成本分解（纳秒）
    double total_cost_ns{0.0};              // 总耗时
    double compute_cost_ns{0.0};            // 计算成本（来自画像曲线）
    double transfer_cost_ns{0.0};           // 传输成本（GPU 才有；H2D + D2H）
    double launch_cost_ns{0.0};             // 提交/launch 固定开销
    double merge_cost_ns{0.0};              // 局部结果合并成本
    double queue_wait_ns{0.0};              // 队列等待（运行时由 Dispatcher 填充）

    // 块大小建议
    std::size_t min_effective_chunk{0};     // 最小有效块（小于此值不如 CPU）
    std::size_t recommended_chunk{0};       // 推荐块大小（介于最小与最大之间）
    std::size_t estimated_chunk_count{0};   // 按推荐块大小估算的 chunk 数
    std::size_t max_chunk_by_memory{0};     // 受 VRAM/RAM 限制的最大块

    // 可行性
    bool feasible{true};                    // 是否可行（VRAM 不够/设备不可用则 false）
    bool profile_available{false};          // 该设备是否有画像曲线
    std::string reason;                     // 诊断说明（"fallback-peak"/"profile-curve"/...）

    // 预计吞吐（GB/s，诊断用）
    double predicted_throughput_gbps{0.0};
};

// ===== 任务总成本估算（跨所有候选设备）=====
struct CostEstimate {
    std::vector<DeviceCost> per_device;     // 每个候选设备的成本
    DeviceId preferred_device{kHwInvalidDeviceId};   // 最优设备
    std::size_t total_work_size{0};         // 总工作量（元素数）
    bool profile_available{false};          // 是否有画像（任一设备有曲线即 true）
    bool profile_stale{false};              // 画像是否过期
    std::string fallback_reason;            // 无画像/降级原因（"no-profile"/"stale"/"corrupt"）
    std::string estimate_summary;           // 人类可读摘要
};

// ===== CostEstimator =====
// 根据 TaskDescriptor + HardwareProfile 推算成本与块大小。
// 不持有可变状态；线程安全可并发调用。
class CostEstimator {
public:
    CostEstimator();
    ~CostEstimator();

    // 设置硬件画像（CostEstimator 不拥有 profile，只读引用）。
    // nullptr 表示无画像，走 CPU fallback。
    // profile 由 HardwareProfileReader 提供，CostEstimator 不参与加载。
    // 注意：调用方需保证 profile 在 estimate() 调用期间有效；
    // 若 profile_reader invalidate_cache() 后旧指针悬空，需重新 set_profile。
    void set_profile(const HardwareProfile* profile) noexcept;
    const HardwareProfile* profile() const noexcept;

    // ===== 从 HardwareProfileReader 刷新画像（用于 invalidate 后重新加载）=====
    // 调用 reader.get_profile_or_cpu_fallback() 获取当前画像指针。
    void refresh_from_reader(profile::HardwareProfileReader& reader);

    // ===== 主入口：估算任务成本 =====
    // 返回每个候选设备的成本 + 推荐设备。
    // 无画像时仅返回 CPU fallback 成本（用保守估算）。
    CostEstimate estimate(const TaskDescriptor& task) const;

    // ===== Phase F2：推算单设备的最小有效块大小 =====
    // 满足以下条件的最小 chunk_size：
    //   1. 计算时间 >= kMinComputeToLaunchRatio × launch 开销（默认 10×）
    //   2. GPU：传输时间 <= 计算时间 × kTransferGainRatio（默认 0.5，即传输可被收益覆盖）
    //   3. chunk_size × bytes_per_item <= available_memory_bytes
    //   4. chunk_size >= 1
    //   5. Tile 任务：chunk_size 受 tile_w × tile_h 约束
    // 若无法满足（如 launch 开销为 0 或设备不可用），返回默认块 1024。
    std::size_t compute_min_effective_chunk(const TaskDescriptor& task, DeviceId device) const;

    // ===== 推算单设备的推荐块大小 =====
    // 介于最小有效块和最大可行块之间，目标使计算/launch 比达到 kTargetComputeRatio（默认 50×）。
    // 受 VRAM/RAM 上限和 Tile 边界约束。
    std::size_t compute_recommended_chunk(const TaskDescriptor& task, DeviceId device) const;

    // ===== 推算单设备的最大块（受内存约束）=====
    std::size_t compute_max_chunk_by_memory(const TaskDescriptor& task, DeviceId device) const;

    // ===== 单设备成本估算（不含 queue_wait，queue_wait 由 Dispatcher 运行时填）=====
    DeviceCost estimate_for_device(const TaskDescriptor& task, DeviceId device) const;

    // ===== 成本模型常量（公开，便于测试调整）=====
    static constexpr std::size_t kDefaultMinChunk = 1024;          // 默认最小块
    static constexpr std::size_t kDefaultRecommendedChunk = 65536; // 默认推荐块
    static constexpr double kMinComputeToLaunchRatio = 10.0;       // 计算 >= 10×launch
    static constexpr double kTargetComputeRatio = 50.0;            // 目标计算/launch = 50×
    static constexpr double kTransferGainRatio = 0.5;              // 传输 <= 0.5×计算
    static constexpr double kCpuFallbackBandwidthGbps = 20.0;      // 无画像时保守 CPU 带宽
    static constexpr double kGpuFallbackBandwidthGbps = 500.0;     // 无画像时保守 GPU 带宽
    static constexpr double kGpuFallbackPcieGbps = 12.0;           // 无画像时保守 PCIe 带宽
    static constexpr double kCpuFallbackLaunchNs = 1000.0;         // 无画像时 CPU launch 开销
    static constexpr double kGpuFallbackLaunchNs = 5000.0;         // 无画像时 GPU launch 开销

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ===== 全局 CostEstimator 单例（runtime.cpp 使用）=====
// 首次调用时从 global_profile_reader 加载画像。
// 线程安全初始化，之后只读。
CostEstimator& global_cost_estimator();

// ===== 工具：TaskDescriptor → 画像曲线 key 推导 =====
// 根据 TaskTraits.task_class + access + intensity 推算画像曲线族与 key。
// 返回 {family, key}；找不到合适曲线时返回 {Arithmetic, "fp32:add:baseline"} 兜底。
struct CurveLookup {
    CapabilityFamily family{CapabilityFamily::Arithmetic};
    CurveKey key;
    MemoryLevel mem_level{MemoryLevel::MainMem};
    MemoryResidency residency{MemoryResidency::Host};
    HwPrecision precision{HwPrecision::Fp32};
    bool valid{false};
};

CurveLookup lookup_curve_for_task(const TaskDescriptor& task) noexcept;

// ===== 工具：DeviceId → backend 字符串 =====
inline std::string device_id_to_backend(DeviceId id) {
    if (id == kHwCpuDeviceId) return "cpu";
    return "cuda:" + std::to_string(id - 1);
}

// ===== 工具：backend 字符串 → DeviceId =====
inline DeviceId backend_to_device_id(const std::string& backend) {
    if (backend == "cpu" || backend.empty()) return kHwCpuDeviceId;
    if (backend.rfind("cuda:", 0) == 0) {
        try {
            int idx = std::stoi(backend.substr(5));
            return static_cast<DeviceId>(idx + 1);
        } catch (...) { return kHwInvalidDeviceId; }
    }
    return kHwInvalidDeviceId;
}

} // namespace astro::compute::cost
} // namespace astro::compute
