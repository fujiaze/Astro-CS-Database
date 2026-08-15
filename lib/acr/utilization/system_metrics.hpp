// lib/acr/utilization/system_metrics.hpp — 系统指标读取（内部头，不暴露到公共 API）
//
// Phase G（08_RESOURCE_CONTROL_SPEC.md §2/§3/§4）：
// 1. CPU 利用率：Windows GetSystemTimes（idle/kernel/user），返回自上次调用以来平均
// 2. RAM：GlobalMemoryStatusEx
// 3. GPU 利用率：NVML 动态加载（nvml.dll），不可用时按队列预算估算并标记 estimated=true
// 4. VRAM：NVML nvmlDeviceGetMemoryInfo，不可用时 estimated=true
//
// 设计约束：
// - 公共头不暴露 Windows API / NVML 第三方类型（PIMPL）
// - NVML 通过 LoadLibrary + GetProcAddress 动态加载，不依赖编译期 nvml.lib
// - 无 GPU 时明确标记 estimated=true，不伪报
// - 线程安全（多 worker 同时读取）
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute::utilization {

// ===== CPU 利用率样本 =====
struct CpuUtilizationSample {
    double ratio{0.0};            // 0.0-1.0（active = 1 - idle/(idle+kernel+user)）
    std::uint64_t timestamp_ns{0};
    bool valid{false};            // 首次调用返回 false（无前次基线）
};

// ===== GPU 利用率样本（单 GPU）=====
struct GpuUtilizationSample {
    std::string backend;          // "cuda:0" 等
    double ratio{0.0};            // 0.0-1.0
    bool estimated{false};        // true=队列预算估算, false=NVML 实读
    std::uint64_t timestamp_ns{0};
    bool valid{false};
};

// ===== RAM 样本 =====
struct MemorySample {
    std::uint64_t total_bytes{0};
    std::uint64_t avail_bytes{0};
    bool valid{false};
};

// ===== VRAM 样本（单 GPU）=====
struct GpuMemorySample {
    std::string backend;
    std::uint64_t total_bytes{0};
    std::uint64_t used_bytes{0};
    std::uint64_t free_bytes{0};
    bool estimated{false};        // true=无 NVML 估算
    bool valid{false};
};

// ===== SystemMetrics =====
// 系统指标读取器（PIMPL，内部封装 Windows API + NVML 动态加载）。
// 线程安全：内部用 mutex 保护采样基线与 NVML 句柄。
class SystemMetrics {
public:
    SystemMetrics();
    ~SystemMetrics();
    SystemMetrics(const SystemMetrics&) = delete;
    SystemMetrics& operator=(const SystemMetrics&) = delete;
    SystemMetrics(SystemMetrics&&) noexcept;
    SystemMetrics& operator=(SystemMetrics&&) noexcept;

    // ---- CPU ----
    // 读取 CPU 利用率（GetSystemTimes）。首次调用返回 valid=false（建立基线）。
    // 后续调用返回自上次调用以来的平均利用率。
    CpuUtilizationSample read_cpu_utilization();

    // ---- RAM ----
    // 读取系统 RAM（GlobalMemoryStatusEx）
    MemorySample read_ram();

    // ---- GPU ----
    // 读取所有已注册 backend 的 GPU 利用率。
    // NVML 可用：实读 nvmlDeviceGetUtilizationRates，estimated=false
    // NVML 不可用：按队列预算估算（queue_depth / max_queue_depth），estimated=true
    std::vector<GpuUtilizationSample> read_gpu_utilizations();

    // 读取所有已注册 backend 的 VRAM。
    // NVML 可用：实读 nvmlDeviceGetMemoryInfo，estimated=false
    // NVML 不可用：estimated=true，total_bytes=0
    std::vector<GpuMemorySample> read_gpu_memories();

    // ---- NVML 状态 ----
    bool nvml_available() const noexcept;

    // GPU 数量（NVML 可用时返回真实 GPU 数；否则返回已注册 backend 数）
    std::size_t gpu_count() const noexcept;

    // ---- Backend 注册与队列预算 ----
    // 注册一个 GPU backend（"cuda:0" 等）。NVML 不可用时用于队列预算估算。
    void register_backend(const std::string& backend);

    // 报告某 backend 的当前队列深度（worker 提交时调用，用于估算）
    void report_queue_depth(const std::string& backend, std::uint32_t depth);

    // 设置队列预算估算参数：max_queue_depth（达到此深度视为 100%）
    void set_queue_budget_max_depth(std::uint32_t max_depth) noexcept;
    std::uint32_t queue_budget_max_depth() const noexcept;

    // 重新加载 NVML（配置变更或首次延迟加载）
    bool reload_nvml();

    // 状态 JSON（acr-status 用）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
