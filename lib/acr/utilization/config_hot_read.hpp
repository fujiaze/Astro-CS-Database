// lib/acr/utilization/config_hot_read.hpp — 配置热读取边界
// 26 号计划 §2：CPU/GPU/IO 利用率目标已移除（用户撤销精确利用率控制）。
// 只保留内存容量（RAM/VRAM 比例、固定保留量）、backend 启用与回退策略。
// **不得提供 CPU/GPU share 或利用率目标参数**。
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
    // 容量上限
    double ram_ratio{0.95};
    double vram_ratio{0.95};
    // 26 号计划 §9：RAM/VRAM 独立固定保留量（RAM 2048MiB、VRAM 512MiB 默认）
    std::uint64_t ram_fixed_reserve_bytes{2048ULL * 1024 * 1024};
    std::uint64_t vram_fixed_reserve_bytes{512ULL * 1024 * 1024};
    // backend 启用（"cuda:0" → true/false）
    std::unordered_map<std::string, bool> backend_enabled;
    // 回退策略
    FallbackPolicy fallback_policy{FallbackPolicy::BestEffort};

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
    void set_ram_ratio(double ratio) noexcept;
    void set_vram_ratio(double ratio) noexcept;
    void set_ram_reserve(std::uint64_t bytes) noexcept;
    void set_vram_reserve(std::uint64_t bytes) noexcept;
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
