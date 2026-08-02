// lib/acr/utilization/actual_tracker.hpp — 实际利用率记录器
// Phase G（08_RESOURCE_CONTROL_SPEC.md §7、§19 §7）：
//   1. 记录实际值（actual）和控制误差（error = actual - target）
//   2. 提供 history（最近 N 次记录）用于趋势分析
//   3. 计算 average / p95 / max / min error
//   4. 记录 worker 参与（每个 worker 是否活跃）
//   5. 记录系统响应（取消、状态查询）
//   6. 不伪报：actual 由 controller 实读，tracker 仅记录
//   7. 线程安全（多 worker 报告）
//   8. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== 单次利用率样本 =====
struct UtilizationSample {
    std::uint64_t timestamp_ns{0};     // 纳秒时间戳
    double actual_ratio{0.0};
    double target_ratio{0.0};
    double error_ratio{0.0};
    bool estimated{false};             // actual 是否为估算
    std::string backend;               // "cpu" / "cuda:0" / ...
    // worker 参与快照（采样时刻）
    std::uint32_t worker_registered{0};
    std::uint32_t worker_active{0};
    std::uint32_t worker_idle{0};
    // 系统响应
    bool cancelled{false};             // 采样时控制器是否处于取消状态
};

// ===== Worker 参与历史条目 =====
struct WorkerParticipationEntry {
    std::uint64_t timestamp_ns{0};
    std::uint32_t registered{0};
    std::uint32_t active{0};
    std::uint32_t idle{0};
};

// ===== 统计摘要 =====
struct UtilizationStats {
    std::size_t sample_count{0};
    double average_actual{0.0};
    double p95_actual{0.0};
    double max_actual{0.0};
    double min_actual{0.0};
    double average_error{0.0};
    double max_error{0.0};
    double min_error{0.0};
    double average_p95_error{0.0};     // |error| 的 p95
    std::uint32_t estimated_count{0};  // 估算样本数
    std::uint32_t cancelled_count{0};  // 取消状态样本数
};

// ===== ActualTracker =====
class ActualTracker {
public:
    ActualTracker();
    ~ActualTracker();
    ActualTracker(const ActualTracker&) = delete;
    ActualTracker& operator=(const ActualTracker&) = delete;

    // 设置历史记录容量（默认 1024 个样本）
    void set_capacity(std::size_t cap) noexcept;
    std::size_t capacity() const noexcept;

    // 记录一次样本
    void record(const UtilizationSample& sample);

    // 记录 worker 参与快照（独立于 utilization sample）
    void record_worker_participation(std::uint32_t registered,
                                     std::uint32_t active,
                                     std::uint32_t idle);

    // 获取最近 N 个样本（N <= capacity）
    std::vector<UtilizationSample> recent(std::size_t n) const;

    // 获取所有样本（按时间顺序）
    std::vector<UtilizationSample> all() const;

    // 获取 worker 参与历史
    std::vector<WorkerParticipationEntry> worker_history() const;

    // 样本总数
    std::size_t sample_count() const noexcept;

    // 计算统计摘要（所有样本）
    UtilizationStats stats() const;

    // 旧接口兼容（average_error / max_error / min_error）
    double average_error() const noexcept;
    double max_error() const noexcept;
    double min_error() const noexcept;

    // 状态 JSON（含统计摘要 + 最近样本）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
