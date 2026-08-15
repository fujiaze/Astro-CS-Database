// lib/acr/scheduler/fallback.hpp — 失败任务回退策略
// Phase F：设备失败时，未执行的 chunk 回退到 CPU，已执行的不重放。
//
// 设计（ 07_WORK_CONSERVING_DISPATCHER_SPEC.md）：
// 1. 失败回退不重放：已 mark_done 的 chunk 不再执行
// 2. 未执行 chunk 回退到 CPU（CoverageBitmap 跟踪）
// 3. 回退后通过 Event 通知（status = DeviceLost 或 KernelFailed）
// 4. 公共头不暴露第三方类型
#pragma once

#include "partitioner.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astro::compute::scheduler {

// ===== 失败回退策略 =====
enum class FallbackStrategy : std::uint8_t {
    None         = 0,  // 不回退（直接失败）
    ToCpu        = 1,  // 回退到 CPU
    ToNextDevice = 2,  // 回退到下一个可用设备（如多 GPU）
};

// ===== 回退决策结果 =====
struct FallbackDecision {
    FallbackStrategy strategy{FallbackStrategy::None};
    std::string target_backend;          // 回退目标 backend
    std::vector<std::size_t> pending_chunks;  // 待回退的 chunk 索引
    bool skip_already_done{true};        // 跳过已完成的 chunk（不重放）
};

// ===== FallbackPolicy =====
class FallbackPolicy {
public:
    FallbackPolicy();
    ~FallbackPolicy();

    // 决定回退策略
    // device_failed: 失败的 backend
    // bitmap: coverage bitmap（已完成的 chunk）
    // available_backends: 其他可用 backend（不含失败 device）
    FallbackDecision decide(const std::string& device_failed,
                            const CoverageBitmap& bitmap,
                            const std::vector<std::string>& available_backends) const;

    // 设置策略偏好（默认 ToCpu）
    void set_strategy(FallbackStrategy s) noexcept;
    FallbackStrategy strategy() const noexcept;

private:
    FallbackStrategy strategy_{FallbackStrategy::ToCpu};
};

} // namespace astro::compute::scheduler
