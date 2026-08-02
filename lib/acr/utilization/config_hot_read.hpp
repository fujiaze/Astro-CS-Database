// lib/acr/utilization/config_hot_read.hpp — 配置热读取边界
// Phase G：运行时热读取配置的边界定义（哪些可热更新、哪些不能）。
//
// 设计：
//   1. 区分可热更新 vs 静态配置
//   2. 可热更新：utilization target、io_budget、memory_ratio
//   3. 静态：thread count、GPU device、ISA level
//   4. 热读取线程安全（atomic + mutex）
//   5. 公共头不暴露第三方类型
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace astro::compute::utilization {

// ===== 配置项分类 =====
enum class ConfigKind : std::uint8_t {
    HotMutable  = 0,   // 运行时可热更新
    ColdStatic  = 1,   // 仅启动时可设，运行时不可改
};

// ===== 热读取配置项 =====
struct HotConfig {
    // HotMutable
    double cpu_target_ratio{0.95};
    double gpu_target_ratio{0.95};
    double ram_ratio{0.9};
    double vram_ratio{0.9};
    std::uint64_t memory_fixed_reserve_bytes{512ULL * 1024 * 1024};
    double io_budget_mbps{0.0};
    // ColdStatic（仅启动时设）
    std::uint32_t max_threads{0};
    std::string gpu_backend;
};

// ===== ConfigHotReader =====
// 线程安全的热读取配置容器。
// HotMutable 项可随时更新；ColdStatic 项仅启动时设置后冻结。
class ConfigHotReader {
public:
    ConfigHotReader();
    ~ConfigHotReader();

    // 启动时设置初始配置（ColdStatic 项此后冻结）
    void init(const HotConfig& cfg);

    // 热更新 HotMutable 项（ColdStatic 不变）
    void update_hot(const HotConfig& cfg);

    // 读取当前配置（线程安全快照）
    HotConfig read() const;

    // 单项热更新便捷接口
    void set_cpu_target(double ratio) noexcept;
    void set_gpu_target(double ratio) noexcept;
    void set_ram_ratio(double ratio) noexcept;
    void set_io_budget(double mbps) noexcept;

    // 是否已初始化（init 调用过）
    bool initialized() const noexcept;

    // 状态 JSON
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
