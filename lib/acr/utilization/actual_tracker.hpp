// lib/acr/utilization/actual_tracker.hpp — 实际利用率记录器
// Phase G：记录实际利用率，"不满足时记录而非伪报"。
//
// 设计：
//   1. 记录实际值（actual）和控制误差（error = actual - target）
//   2. 提供 history（最近 N 次记录）用于趋势分析
//   3. 线程安全（多 worker 报告）
//   4. 公共头不暴露第三方类型
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
    std::string backend;               // "cpu" / "cuda:0" / ...
};

// ===== ActualTracker =====
class ActualTracker {
public:
    ActualTracker();
    ~ActualTracker();

    // 设置历史记录容量（默认 1024 个样本）
    void set_capacity(std::size_t cap) noexcept;

    // 记录一次样本
    void record(const UtilizationSample& sample);

    // 获取最近 N 个样本（N <= capacity）
    std::vector<UtilizationSample> recent(std::size_t n) const;

    // 获取所有样本（按时间顺序）
    std::vector<UtilizationSample> all() const;

    // 样本总数
    std::size_t sample_count() const noexcept;

    // 计算 average / max / min error（所有样本）
    double average_error() const noexcept;
    double max_error() const noexcept;
    double min_error() const noexcept;

    // 状态 JSON（含统计摘要）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
