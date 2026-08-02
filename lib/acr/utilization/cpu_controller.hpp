// lib/acr/utilization/cpu_controller.hpp — CPU 利用率软目标控制器
// Phase G（08_RESOURCE_CONTROL_SPEC.md §2、§5）：
//   1. 软占用目标：50/80/95/100% 都支持，可调
//   2. 所有逻辑线程可参与：通过批次大小/队列深度/优先级/让步控制
//      —— 不通过永久少开一个线程实现 95%
//   3. 读取实际 CPU 利用率（GetSystemTimes），不接受人工输入伪报
//   4. 系统保持可响应：取消、状态查询、GPU submit 响应
//   5. 控制器不修改 hardware-profile（只读，且不持有其引用）
//   6. 记录所有 worker 是否参与（worker 参与位图）
//   7. 控制窗口 100-500ms；线程错峰让步，不做全线程长时间同步睡眠
//   8. 公共头不暴露第三方类型（PIMPL）
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== 控制策略 =====
enum class ControlStrategy : std::uint8_t {
    BatchSize   = 0,  // 调整批次大小（更小批次 → 更频繁让步）
    QueueDepth  = 1,  // 调整队列深度
    Priority    = 2,  // 调整任务优先级
    Yield       = 3,  // 在 chunk 之间让步（错峰 yield，非全线程同步睡眠）
};

// ===== Worker 参与记录 =====
struct WorkerParticipation {
    std::uint32_t registered_count{0};     // 已注册 worker 总数
    std::uint32_t active_count{0};         // 当前活跃 worker 数
    std::uint32_t idle_count{0};           // 当前空闲 worker 数
    std::vector<std::uint32_t> worker_ids; // 已注册 worker ID 列表
    // 每个 worker 的活跃状态（与 worker_ids 同序）
    std::vector<bool> active_flags;
};

// ===== CPU 控制决策 =====
struct CpuControlDecision {
    std::uint32_t batch_size{1};       // 建议批次大小
    std::uint32_t queue_depth{1};      // 建议队列深度
    int priority{0};                   // 建议优先级（0=normal, +高 -低）
    bool should_yield{false};          // 是否在 chunk 间让步（错峰）
    std::uint32_t yield_stride{1};     // 让步步幅：每 N 个 chunk 让步一次（错峰）
    double target_ratio{0.95};         // 当前目标比例
    double actual_ratio{0.0};          // 当前实际比例（GetSystemTimes 实读）
    bool actual_estimated{false};      // 实际值是否为估算（CPU 永远 false）
    double error_ratio{0.0};           // 控制误差 = actual - target
    bool valid{false};                 // actual 是否有效（首次采样无基线时 false）
    std::uint64_t timestamp_ns{0};
};

// ===== CpuController =====
class CpuController {
public:
    CpuController();
    ~CpuController();
    CpuController(const CpuController&) = delete;
    CpuController& operator=(const CpuController&) = delete;

    // ---- 目标与策略 ----
    // 设置目标利用率（0.0-1.0）。支持 0.50/0.80/0.95/1.00 等任意点。
    void set_target(double target_ratio) noexcept;
    double target() const noexcept;

    // 设置控制策略
    void set_strategy(ControlStrategy s) noexcept;
    ControlStrategy strategy() const noexcept;

    // 设置控制窗口（毫秒）。默认 200ms，范围 100-500ms。
    void set_control_window_ms(std::uint32_t ms) noexcept;
    std::uint32_t control_window_ms() const noexcept;

    // ---- Worker 参与记录 ----
    // 注册一个 worker（返回 worker_id）。所有逻辑线程均可注册。
    std::uint32_t register_worker();
    // 注销一个 worker
    void unregister_worker(std::uint32_t worker_id);
    // 标记 worker 当前活跃（正在执行任务）
    void mark_worker_active(std::uint32_t worker_id);
    // 标记 worker 当前空闲（等待任务）
    void mark_worker_idle(std::uint32_t worker_id);
    // 获取 worker 参与快照
    WorkerParticipation worker_participation() const;

    // ---- 实际利用率采样 + 控制决策 ----
    // 内部读取实际 CPU 利用率（GetSystemTimes），做出控制决策。
    // 首次调用返回 valid=false（建立基线）。
    CpuControlDecision sample_and_decide();

    // 测试/注入接口：用外部提供的 actual_ratio 做决策（标记 estimated=true）。
    // 生产路径应使用 sample_and_decide()。
    CpuControlDecision decide_with_actual(double actual_ratio);

    // ---- 取消与响应 ----
    // 请求控制器进入取消状态（停止新提交建议）
    void request_cancel() noexcept;
    bool cancelled() const noexcept;
    // 清除取消状态
    void clear_cancel() noexcept;

    // ---- 状态 ----
    // 生成状态 JSON（acr-status 用）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
