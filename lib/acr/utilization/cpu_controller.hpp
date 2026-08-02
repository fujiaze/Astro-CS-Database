// lib/acr/utilization/cpu_controller.hpp — CPU 利用率软目标控制器
// Phase G：95% 软占用目标（不是永久停用线程）。
//
// 设计（控制包 08_UTILIZATION_CONTROL_SPEC.md）：
//   1. 软占用目标：50/80/95/100% 都支持，可调
//   2. 所有线程可参与：通过批次大小/队列深度/优先级/让步控制
//   3. 不满足时记录而非伪报：记录实际值和控制误差
//   4. 系统保持可响应：不耗尽所有资源
//   5. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace astro::compute::utilization {

// ===== 控制策略 =====
enum class ControlStrategy : std::uint8_t {
    BatchSize   = 0,  // 调整批次大小（更小批次 → 更频繁让步）
    QueueDepth  = 1,  // 调整队列深度
    Priority    = 2,  // 调整任务优先级
    Yield       = 3,  // 在 chunk 之间让步（std::this_thread::yield）
};

// ===== CPU 控制决策 =====
struct CpuControlDecision {
    std::uint32_t batch_size{1};       // 建议批次大小
    std::uint32_t queue_depth{1};      // 建议队列深度
    int priority{0};                   // 建议优先级（0=normal, +高 -低）
    bool should_yield{false};          // 是否在 chunk 间让步
    double target_ratio{0.95};         // 当前目标比例
    double actual_ratio{0.0};          // 当前实际比例
    double error_ratio{0.0};           // 控制误差 = actual - target
};

// ===== CpuController =====
class CpuController {
public:
    CpuController();
    ~CpuController();

    // 设置目标利用率（0.0 - 1.0）
    void set_target(double target_ratio) noexcept;
    double target() const noexcept;

    // 设置控制策略
    void set_strategy(ControlStrategy s) noexcept;

    // 根据当前实际利用率做出控制决策
    // actual_ratio: 当前观察到的利用率（0.0-1.0）
    CpuControlDecision decide(double actual_ratio) const;

    // 生成状态 JSON（acr-status 用）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
