// lib/acr/utilization/memory_budget.hpp — RAM/VRAM 容量限制
// Phase G（08_RESOURCE_CONTROL_SPEC.md §4）：
// 1. RAM 限制：limit = min(total*ratio, total-fixed_reserve)
// 2. VRAM 限制：同上公式（per-GPU 独立）
// 3. 25 §7：RAM 固定保留默认 2048 MiB，VRAM 固定保留默认 512 MiB
// 4. ratio 默认 0.95（90% 是旧值，spec §1 用 0.95）
// 5. 读取实际可用内存（GlobalMemoryStatusEx + NVML）
// 6. 达到上限：停止新提交、缩小块、释放可重建缓存、选择低内存路径、回退其他设备或明确失败
// 7. 控制器不修改 hardware-profile
// 8. 公共头不暴露第三方类型（PIMPL）
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== 内存预算配置 =====
struct MemoryBudgetConfig {
    double ram_ratio{0.95};               // RAM 容量比例
    double vram_ratio{0.95};              // VRAM 容量比例
    double pinned_ratio{0.95};            // pinned staging 容量比例（默认与 RAM 一致）
    std::uint64_t ram_fixed_reserve_bytes{2048ULL * 1024 * 1024};  // 2048 MiB
    std::uint64_t vram_fixed_reserve_bytes{512ULL * 1024 * 1024};  // 512 MiB
    std::uint64_t pinned_fixed_reserve_bytes{256ULL * 1024 * 1024}; // 256 MiB
};

// ===== 单 GPU VRAM 预算 =====
struct GpuMemoryBudget {
    std::string backend;
    std::uint64_t total_vram{0};
    std::uint64_t limit_vram{0};
    std::uint64_t used_vram{0};
    bool vram_exceeded{false};
    bool estimated{false};               // true=无 NVML 估算
    bool valid{false};
};

// ===== 内存预算决策 =====
struct MemoryBudget {
    std::uint64_t total_ram{0};
    std::uint64_t limit_ram{0};          // RAM 限额
    std::uint64_t used_ram{0};
    std::uint64_t avail_ram{0};          // 实际可用（GlobalMemoryStatusEx）
    bool ram_exceeded{false};
    bool ram_valid{false};
    // 06 号规范 §5：pinned staging 独立记账（默认并入 RAM 采样，可注入）
    std::uint64_t pinned_limit{0};
    std::uint64_t pinned_used{0};
    bool pinned_valid{false};
    bool pinned_exceeded{false};
    // 多 GPU VRAM
    std::vector<GpuMemoryBudget> gpus;
};

// ===== MemoryBudgetController =====
class MemoryBudgetController {
public:
    MemoryBudgetController();
    ~MemoryBudgetController();
    MemoryBudgetController(const MemoryBudgetController&) = delete;
    MemoryBudgetController& operator=(const MemoryBudgetController&) = delete;

    // ---- 配置 ----
    void configure(const MemoryBudgetConfig& cfg) noexcept;
    const MemoryBudgetConfig& config() const noexcept;

    // ---- Backend 注册 ----
    void register_backend(const std::string& backend);
    std::vector<std::string> backends() const;

    // ---- 实际内存采样 ----
    // 内部读取 GlobalMemoryStatusEx（RAM）+ NVML（VRAM）。
    // used_ram 通过 GlobalMemoryStatusEx 计算（total - avail）。
    // VRAM 通过 NVML nvmlDeviceGetMemoryInfo；无 NVML 时 estimated=true。
    MemoryBudget sample();

    // 测试/注入接口：用外部提供的 used 值计算预算（不读取系统）。
    // ram_total/vram_total 为 0 时使用上次采样到的系统总量。
    MemoryBudget report_with(std::uint64_t used_ram,
                             std::uint64_t used_vram,
                             const std::string& backend = "cuda:0");

    // 06 号规范 §5：pinned staging 注入（独立于 RAM 的记账）
    MemoryBudget report_pinned(std::uint64_t used_pinned,
                               std::uint64_t total_ram_for_limit = 0);

    // ---- 限额计算 ----
    // limit = min(total*ratio, total-fixed_reserve)
    static std::uint64_t compute_limit(std::uint64_t total, double ratio,
                                       std::uint64_t fixed_reserve) noexcept;

    // ---- 达到上限时的建议动作 ----
    enum class ExceedAction {
        None                = 0,
        StopNewSubmit       = 1,  // 停止新提交
        ShrinkBlock         = 2,  // 缩小块
        ReleaseCache        = 3,  // 释放可重建缓存
        LowMemoryPath       = 4,  // 选择低内存路径
        FallbackOtherDevice = 5,  // 回退其他设备
        Fail                = 6,  // 明确失败
    };
    // 根据超限程度建议动作
    static ExceedAction suggest_action(std::uint64_t used,
                                       std::uint64_t limit,
                                       std::uint64_t total) noexcept;

    // ---- NVML 状态 ----
    bool nvml_available() const noexcept;
    bool reload_nvml();

    // ---- 状态 JSON ----
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
