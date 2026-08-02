// lib/acr/utilization/gpu_controller.hpp — GPU 队列/批次软目标
// Phase G：GPU 软占用控制器。
//
// 设计：
//   1. GPU 软目标通过队列深度/批次大小控制
//   2. 多 GPU 时按设备独立控制
//   3. 不满足时记录而非伪报
//   4. 公共头不暴露 cuda*/hip* 类型
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
    double target_ratio{0.95};
    double actual_ratio{0.0};
    double error_ratio{0.0};
    bool throttle{false};                 // 是否节流（暂停提交新任务）
};

// ===== GpuController =====
class GpuController {
public:
    GpuController();
    ~GpuController();

    // 设置全局目标利用率（0.0-1.0）
    void set_target(double target_ratio) noexcept;
    double target() const noexcept;

    // 注册一个 GPU backend（"cuda:0", "cuda:1", ...）
    void register_backend(const std::string& backend);

    // 为指定 backend 做控制决策
    // actual_ratio: 该 backend 当前利用率
    GpuControlDecision decide(const std::string& backend, double actual_ratio) const;

    // 列出所有已注册 backends
    std::vector<std::string> backends() const;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
