// ============================================================================
// resource_monitor.h - 资源监测框架与动态成本估算器 (H-001)
//
// 规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
// ResourceMonitor: CPU、RSS、Commit、I/O、活跃阶段
// FrameCostEstimator: 按图像尺寸、星点、Gaia数量、Nside估算阶段峰值
//
// 契约: engineering_authoritative/contracts/resource_profile.schema.json
//
// Python 原型: engineering_authoritative/evidence/H-001/resource_monitor.py
// engineering_authoritative/evidence/H-001/cost_estimator.py
//
// 设计说明:
// 本头文件为 C++ 移植骨架, 对应 Python 原型的接口定义。
// 实现策略: Windows 平台使用 GlobalMemoryStatusEx + GetProcessMemoryInfo +
// PDH (Performance Data Helper) 采集 CPU/IO; psutil 原型验证概念后移植。
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <deque>
#include <memory>
#include <optional>

namespace astrocs {

// ============================================================================
// Stage1 阶段名称常量
// ============================================================================

namespace stage {
    constexpr const char* READ_FITS    = "READ_FITS";
    constexpr const char* CALIBRATE    = "CALIBRATE";
    constexpr const char* PLATESOLVE   = "PLATESOLVE";
    constexpr const char* PSF          = "PSF";
    constexpr const char* PHOTOMETRIC  = "PHOTOMETRIC";
    constexpr const char* SNR          = "SNR";
    constexpr const char* DRIZZLE      = "DRIZZLE";
} // namespace stage

// 高内存阶段集合 (需内存预约, H-002 使用)
inline bool is_high_memory_stage(const std::string& s) {
    return s == stage::PLATESOLVE || s == stage::DRIZZLE;
}

// 高 CPU 阶段集合
inline bool is_high_cpu_stage(const std::string& s) {
    return s == stage::CALIBRATE || s == stage::PLATESOLVE || s == stage::DRIZZLE;
}

// ============================================================================
// 资源快照
// ============================================================================

struct ResourceSnapshot {
    double timestamp_sec;              // 采样时间戳
    double cpu_percent;                // CPU 使用率 (0-100)
    int64_t rss_bytes;                 // 进程 RSS
    int64_t commit_bytes;              // 系统已提交内存
    int64_t commit_limit_bytes;        // 系统提交上限
    double disk_read_bytes_per_sec;    // 磁盘读速率
    double disk_write_bytes_per_sec;   // 磁盘写速率
    std::optional<double> temperature_c;  // 温度 (无可获取时 nullopt)
};

// ============================================================================
// 资源统计摘要 (滚动窗口)
// ============================================================================

struct ResourceSummary {
    double cpu_mean;
    double cpu_max;
    double cpu_p95;
    int64_t rss_mean;
    int64_t rss_max;
    int64_t commit_mean;
    int64_t commit_max;
    int64_t commit_limit;
    double disk_read_mean;
    double disk_write_mean;
    std::optional<double> temperature_mean;
    int n_samples;
    double window_sec;
};

// ============================================================================
// ResourceMonitor 资源监测器
// ============================================================================

class ResourceMonitor {
public:
    static constexpr double DEFAULT_WINDOW_SEC = 60.0;
    static constexpr double DEFAULT_SAMPLE_INTERVAL = 0.5;

    ResourceMonitor(double window_sec = DEFAULT_WINDOW_SEC,
                    double sample_interval = DEFAULT_SAMPLE_INTERVAL);
    ~ResourceMonitor();

    // 生命周期
    void start();
    void stop();

    // 手动采样 (无后台线程)
    ResourceSnapshot sample_once();

    // 活跃阶段追踪
    void mark_stage_start(const std::string& stage_name);
    void mark_stage_end(const std::string& stage_name);
    std::vector<std::string> get_active_stages() const;

    // 查询
    std::optional<ResourceSnapshot> get_snapshot() const;
    std::vector<ResourceSnapshot> get_samples() const;
    std::optional<ResourceSummary> get_summary() const;

    double get_cpu_load() const;
    int64_t get_rss_bytes() const;
    int64_t get_commit_bytes() const;
    int64_t get_available_memory() const;

private:
    double window_sec_;
    double sample_interval_;
    mutable std::mutex mutex_;
    std::deque<ResourceSnapshot> samples_;
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    std::map<std::string, double> active_stages_;  // stage -> start_time

    // 平台相关采集 (实现于 resource_monitor.cpp)
    ResourceSnapshot do_sample();
    void loop();
};

// ============================================================================
// 帧参数 (成本估算输入)
// ============================================================================

struct FrameParams {
    std::string frame_id;
    int image_w = 0;
    int image_h = 0;
    int n_stars = 0;           // 0=未知, 用默认值
    int n_gaia = 0;
    int nside = 0;             // 0=自适应
    double pixel_scale_arcsec = 1.0;
    bool is_wide_field = false;

    int64_t n_pixels() const { return static_cast<int64_t>(image_w) * image_h; }
    int64_t image_buffer_bytes() const { return n_pixels() * 4; }
};

// ============================================================================
// 阶段成本预测
// ============================================================================

struct StageCost {
    std::string stage;
    int64_t predicted_peak_bytes;
    int64_t uncertainty_bytes;
    double predicted_duration_sec;
    double cpu_intensity;     // 0.0-1.0
    double io_intensity;      // 0.0-1.0
    bool is_high_memory;
    std::string model_notes;
};

// 整帧成本预测
struct FrameCostEstimate {
    std::string frame_id;
    std::map<std::string, StageCost> stages;
    int64_t total_predicted_peak_bytes;
    double total_predicted_duration_sec;
    std::string worst_stage;
    int64_t uncertainty_bytes;
};

// ============================================================================
// FrameCostEstimator 动态成本估算器
// ============================================================================

class FrameCostEstimator {
public:
    FrameCostEstimator();

    // 单阶段估算
    StageCost estimate_read_fits(const FrameParams& p) const;
    StageCost estimate_calibrate(const FrameParams& p) const;
    StageCost estimate_platesolve(const FrameParams& p) const;
    StageCost estimate_psf(const FrameParams& p) const;
    StageCost estimate_photometric(const FrameParams& p) const;
    StageCost estimate_snr(const FrameParams& p) const;
    StageCost estimate_drizzle(const FrameParams& p) const;

    // 全帧估算
    FrameCostEstimate estimate(const FrameParams& p) const;

    // 最坏下一帧内存需求 (H-002 准入控制使用)
    int64_t estimate_worst_next_frame(const FrameParams& p) const;

private:
    // B-002 校准常量 (实现于 resource_monitor.cpp)
    // 详见 Python 原型 cost_estimator.py CostModelConstants
};

} // namespace astrocs
