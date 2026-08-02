// lib/acr/utilization/config_hot_read.hpp — 配置热读取边界
// Phase G（08_RESOURCE_CONTROL_SPEC.md §6）：
//   1. 区分可热更新 vs 静态配置
//   2. 可热更新：utilization target、io_budget、memory_ratio、backend enable、fallback policy
//   3. 静态：thread count、GPU device、ISA level
//   4. 热读取线程安全（atomic + mutex）
//   5. **不得提供 CPU/GPU share 参数**（spec §6 明确禁止）
//   6. 公共头不暴露第三方类型
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace astro::compute::utilization {

// ===== 配置项分类 =====
enum class ConfigKind : std::uint8_t {
    HotMutable  = 0,   // 运行时可热更新
    ColdStatic  = 1,   // 仅启动时可设，运行时不可改
};

// ===== 回退策略 =====
enum class FallbackPolicy : std::uint8_t {
    Strict        = 0,  // 严格：无可用 backend 即失败
    PreferCpu     = 1,  // 回退 CPU
    PreferOtherGpu= 2,  // 回退其他 GPU
    BestEffort    = 3,  // 尽力：任意可用 backend
};

// ===== 热读取配置项 =====
struct HotConfig {
    // ---- HotMutable ----
    // 资源利用率目标（不是任务比例！spec §1 明确：不表示 CPU/GPU 任务比例）
    double cpu_target_ratio{0.95};
    double gpu_target_ratio{0.95};
    double io_target_ratio{0.95};   // I/O 软目标
    // 容量上限
    double ram_ratio{0.95};
    double vram_ratio{0.95};
    std::uint64_t memory_fixed_reserve_bytes{512ULL * 1024 * 1024};
    double io_budget_mbps{0.0};
    // backend 启用（"cuda:0" → true/false）
    std::unordered_map<std::string, bool> backend_enabled;
    // 回退策略
    FallbackPolicy fallback_policy{FallbackPolicy::BestEffort};
    // 控制窗口
    std::uint32_t cpu_control_window_ms{200};
    std::uint32_t gpu_max_queue_depth{8};

    // ---- ColdStatic（仅启动时设，运行时冻结）----
    std::uint32_t max_threads{0};     // 0 = 自动（hardware profile 决定）
    std::string gpu_backend;          // 主 GPU backend（如 "cuda:0"）
    std::string isa_level;            // ISA level（启动时门禁）

    // ---- 禁止字段（明确不存在，编译期保证）----
    // 不提供 cpu_share / gpu_share / device weight / 任务比例参数
    // spec §1: 这些参数不表示 CPU/GPU 任务比例
};

// ===== ConfigHotReader =====
// 线程安全的热读取配置容器。
// HotMutable 项可随时更新；ColdStatic 项仅启动时设置后冻结。
class ConfigHotReader {
public:
    ConfigHotReader();
    ~ConfigHotReader();
    ConfigHotReader(const ConfigHotReader&) = delete;
    ConfigHotReader& operator=(const ConfigHotReader&) = delete;

    // 启动时设置初始配置（ColdStatic 项此后冻结）
    void init(const HotConfig& cfg);

    // 热更新 HotMutable 项（ColdStatic 不变）
    void update_hot(const HotConfig& cfg);

    // 读取当前配置（线程安全快照）
    HotConfig read() const;

    // ---- 单项热更新便捷接口 ----
    void set_cpu_target(double ratio) noexcept;
    void set_gpu_target(double ratio) noexcept;
    void set_io_target(double ratio) noexcept;
    void set_ram_ratio(double ratio) noexcept;
    void set_vram_ratio(double ratio) noexcept;
    void set_io_budget(double mbps) noexcept;
    void set_memory_reserve(std::uint64_t bytes) noexcept;
    void set_cpu_control_window_ms(std::uint32_t ms) noexcept;
    void set_gpu_max_queue_depth(std::uint32_t depth) noexcept;
    void set_fallback_policy(FallbackPolicy policy) noexcept;

    // backend 启用/禁用（热更新）
    void set_backend_enabled(const std::string& backend, bool enabled) noexcept;
    bool is_backend_enabled(const std::string& backend) const;

    // 是否已初始化（init 调用过）
    bool initialized() const noexcept;

    // 列出所有已配置 backend
    std::vector<std::string> configured_backends() const;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
