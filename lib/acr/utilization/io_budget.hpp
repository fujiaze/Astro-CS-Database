// lib/acr/utilization/io_budget.hpp — I/O 预算控制
// Phase G：记录实际 I/O 吞吐量，与预算比较。
//
// 设计：
//   1. budget_mbps = 0 表示不限速
//   2. 实际 I/O 通过 record_io(bytes, duration_ns) 记录，控制器计算吞吐量
//   3. 超过预算时记录并标记 exceeded，不强制节流（软目标）
//   4. 公共头不暴露第三方类型
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
    bool valid{false};              // 是否有实际 I/O 记录
    std::uint64_t total_bytes{0};
    std::uint64_t total_duration_ns{0};
    std::uint64_t record_count{0};
};

// ===== IoBudgetController =====
class IoBudgetController {
public:
    IoBudgetController();
    ~IoBudgetController();
    IoBudgetController(const IoBudgetController&) = delete;
    IoBudgetController& operator=(const IoBudgetController&) = delete;

    void configure(const IoBudgetConfig& cfg) noexcept;
    const IoBudgetConfig& config() const noexcept;

    // 记录一次 I/O 操作（bytes + 耗时纳秒）。线程安全。
    void record_io(std::uint64_t bytes, std::uint64_t duration_ns);

    // 采样：基于累计 I/O 计算实际吞吐量（Mbps），返回决策
    // 窗口：自上次 sample() 以来的累计 / 时间
    IoBudgetDecision sample();

    // 测试/注入接口：直接报告实际吞吐量
    IoBudgetDecision report(double actual_mbps) const;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
