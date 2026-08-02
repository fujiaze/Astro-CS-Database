// lib/acr/utilization/io_budget.hpp — I/O 预算控制
// Phase G：I/O 带宽预算（MB/s 限速）。
//
// 设计：
//   1. budget_mbps = 0 表示不限速
//   2. 实际 I/O 超过预算时记录，不强制节流（软目标）
//   3. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace astro::compute::utilization {

// ===== I/O 预算配置 =====
struct IoBudgetConfig {
    double budget_mbps{0.0};     // 0 = 不限速
    double warn_threshold{0.9};  // 90% 触发警告
};

// ===== I/O 预算决策 =====
struct IoBudgetDecision {
    double budget_mbps{0.0};
    double actual_mbps{0.0};
    double utilization_ratio{0.0};  // actual / budget
    bool exceeded{false};
    bool warn{false};
};

// ===== IoBudgetController =====
class IoBudgetController {
public:
    IoBudgetController();
    ~IoBudgetController();

    void configure(const IoBudgetConfig& cfg) noexcept;
    const IoBudgetConfig& config() const noexcept;

    // 报告实际吞吐量（MB/s），返回决策
    IoBudgetDecision report(double actual_mbps) const;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
