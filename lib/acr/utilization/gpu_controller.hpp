// lib/acr/utilization/gpu_controller.hpp — GPU 队列/批次软目标控制器
// Phase G（08_RESOURCE_CONTROL_SPEC.md §3、§5）：
//   1. GPU 软目标通过 queue/stream 深度、batch 大小、kernel 时长、提交节奏控制
//   2. 多 GPU 独立控制
//   3. 利用率 API 不可用时按队列预算估算并明确标记 estimated=true
//   4. 不允许无界排队
//   5. 控制器不修改 hardware-profile
//   6. 目标 95% 允许稳定波动，不承诺每毫秒精确
//   7. 公共头不暴露 cuda*/nvml* 类型（PIMPL）
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== GPU 控制决策 =====
struct GpuControlDecision {
    std::string backend;                  // "cuda:0" 等
    std::uint32_t queue_depth{1};         // 建议队列深度
    std::uint32_t batch_size{1};          // 建议 batch 大小（kernel launch 数）
    std::uint32_t max_queue_depth{8};     // 不允许无界排队的上限
    double target_ratio{0.95};
    double actual_ratio{0.0};
    bool actual_estimated{false};         // true=队列预算估算, false=NVML 实读
    double error_ratio{0.0};
    bool throttle{false};                 // 是否节流（暂停提交新任务）
    bool valid{false};
    std::uint64_t timestamp_ns{0};
};

// ===== GpuController =====
class GpuController {
public:
    GpuController();
    ~GpuController();
    GpuController(const GpuController&) = delete;
    GpuController& operator=(const GpuController&) = delete;

    // ---- 目标 ----
    // 设置全局目标利用率（0.0-1.0）
    void set_target(double target_ratio) noexcept;
    double target() const noexcept;

    // 设置无界排队上限（默认 8）。queue_depth 不会建议超过此值。
    void set_max_queue_depth(std::uint32_t max_depth) noexcept;
    std::uint32_t max_queue_depth() const noexcept;

    // ---- Backend 注册 ----
    // 注册一个 GPU backend（"cuda:0", "cuda:1", ...）。多 GPU 独立控制。
    void register_backend(const std::string& backend);
    std::vector<std::string> backends() const;

    // ---- 队列深度报告（worker 提交时调用，用于无 NVML 时的估算）----
    void report_queue_depth(const std::string& backend, std::uint32_t depth);

    // ---- 实际利用率采样 + 控制决策 ----
    // 内部读取 NVML（或队列预算估算），做出控制决策。多 GPU 独立。
    // 返回每个已注册 backend 的决策。
    std::vector<GpuControlDecision> sample_and_decide();

    // 单 backend 采样决策
    GpuControlDecision sample_and_decide(const std::string& backend);

    // 测试/注入接口：用外部提供的 actual_ratio 做决策。
    // estimated=true 时标记为估算。
    GpuControlDecision decide_with_actual(const std::string& backend,
                                          double actual_ratio, bool estimated);

    // ---- NVML 状态 ----
    bool nvml_available() const noexcept;
    std::size_t gpu_count() const noexcept;

    // 重新加载 NVML
    bool reload_nvml();

    // ---- 取消与响应 ----
    void request_cancel() noexcept;
    bool cancelled() const noexcept;
    void clear_cancel() noexcept;

    // ---- 状态 ----
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
