// lib/acr/utilization/memory_budget.hpp — RAM/VRAM 容量限制
// Phase G：limit = min(total*ratio, total-fixed_reserve)
//
// 设计（控制包 08_UTILIZATION_CONTROL_SPEC.md）：
//   1. RAM 限制：limit = min(total*ratio, total-fixed_reserve)
//   2. VRAM 限制：同上公式（per-GPU 独立）
//   3. fixed_reserve 默认 512 MB
//   4. ratio 默认 0.9（90%）
//   5. 公共头不暴露第三方类型
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== 内存预算配置 =====
struct MemoryBudgetConfig {
    double ram_ratio{0.9};              // RAM 容量比例
    double vram_ratio{0.9};             // VRAM 容量比例
    std::uint64_t fixed_reserve_bytes{512ULL * 1024 * 1024};  // 512 MB
};

// ===== 内存预算决策 =====
struct MemoryBudget {
    std::uint64_t total_ram{0};
    std::uint64_t limit_ram{0};          // RAM 限额
    std::uint64_t used_ram{0};
    std::uint64_t total_vram{0};
    std::uint64_t limit_vram{0};         // VRAM 限额
    std::uint64_t used_vram{0};
    bool ram_exceeded{false};
    bool vram_exceeded{false};
};

// ===== MemoryBudgetController =====
class MemoryBudgetController {
public:
    MemoryBudgetController();
    ~MemoryBudgetController();

    // 配置
    void configure(const MemoryBudgetConfig& cfg) noexcept;
    const MemoryBudgetConfig& config() const noexcept;

    // 设置系统总内存（RAM/VRAM），用于计算 limit
    void set_system_memory(std::uint64_t total_ram, std::uint64_t total_vram) noexcept;

    // 报告当前使用量，返回预算决策（是否超限）
    MemoryBudget report(std::uint64_t used_ram, std::uint64_t used_vram) const;

    // 计算 limit = min(total*ratio, total-fixed_reserve)
    static std::uint64_t compute_limit(std::uint64_t total, double ratio,
                                       std::uint64_t fixed_reserve) noexcept;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
