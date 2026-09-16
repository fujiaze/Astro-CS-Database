// ============================================================================
// admission_controller.h - 内存预约、CPU回滞和准入控制 (H-002)
//
// 规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
// MemoryBudgetManager: 预约、释放、安全余量和误差系数
// AdmissionController: CPU回滞、内存门限、阶段兼容矩阵
// 准入公式: reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget
// 压力处理: 停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap
//
// Python 原型: engineering_authoritative/evidence/H-002/admission_controller.py
// 依赖: H-001 resource_monitor.h (ResourceMonitor + FrameCostEstimator)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <optional>
#include <set>

#include "resource_monitor.h"

namespace astrocs {

// ============================================================================
// 准入决策
// ============================================================================

enum class AdmissionDecision {
    ADMIT,   // 准入
    DEFER,   // 推迟 (资源不足)
    REJECT,  // 拒绝 (参数无效)
};

struct AdmissionResult {
    AdmissionDecision decision;
    std::string reason;
    std::string frame_id;
    std::string stage;
    int64_t budget_bytes;
    int64_t reserved_bytes;
    int64_t predicted_peak_bytes;
    int64_t uncertainty_bytes;
    int64_t os_margin_bytes;
    int64_t worst_next_frame_bytes;
    int64_t total_required_bytes;
    int64_t available_bytes;
    double cpu_load;
    int max_concurrent;
};

// ============================================================================
// 内存预约记录
// ============================================================================

struct MemoryReservation {
    std::string frame_id;
    std::string stage;
    int64_t reserved_bytes;
    double timestamp_sec;
    std::optional<int64_t> actual_peak_bytes;
};

// ============================================================================
// MemoryBudgetManager 内存预算管理器
// ============================================================================

class MemoryBudgetManager {
public:
    MemoryBudgetManager(int64_t total_budget_bytes,
                        int64_t os_margin_bytes = 2LL * 1024 * 1024 * 1024,
                        double uncertainty_factor = 1.0);

    int64_t get_reserved() const;
    int64_t total_budget() const { return total_budget_; }
    int64_t os_margin() const { return os_margin_; }

    bool reserve(const std::string& frame_id, const std::string& stage, int64_t bytes);
    std::optional<MemoryReservation> release(const std::string& frame_id);
    void set_actual_peak(const std::string& frame_id, int64_t actual_bytes);

    // 预算检查 (不实际预约)
    // 公式: reserved + predicted + uncertainty*factor + os_margin + worst_next <= budget
    struct AllocationCheck {
        bool can_allocate;
        int64_t total_required;
        int64_t available;
    };
    AllocationCheck can_allocate(int64_t predicted_peak,
                                  int64_t uncertainty,
                                  int64_t worst_next_frame) const;

    std::vector<MemoryReservation> get_active_reservations() const;
    std::map<std::string, std::string> get_summary() const;

private:
    int64_t total_budget_;
    int64_t os_margin_;
    double uncertainty_factor_;
    mutable std::mutex mutex_;
    std::map<std::string, MemoryReservation> reservations_;
};

// ============================================================================
// CPUBackpressure CPU回滞控制器
// ============================================================================

class CPUBackpressure {
public:
    static constexpr double LOAD_HIGH_THRESHOLD = 90.0;      // 停止投喂
    static constexpr double LOAD_THROTTLE_THRESHOLD = 70.0;   // 开始回滞
    static constexpr double LOAD_FULL_SPEED_THRESHOLD = 60.0; // 全速

    explicit CPUBackpressure(int max_concurrent = 2,
                             const ResourceMonitor* monitor = nullptr);

    void set_monitor(const ResourceMonitor* monitor) { monitor_ = monitor; }
    void update_load(double cpu_percent);

    int get_max_concurrent() const;
    bool is_throttled() const;
    bool is_feeding_stopped() const;

    struct Status {
        double cpu_load;
        int max_concurrent;
        int configured_max;
        bool is_throttled;
        bool feeding_stopped;
    };
    Status get_status() const;

private:
    int configured_max_;
    const ResourceMonitor* monitor_;
    mutable std::mutex mutex_;
    std::vector<double> load_history_;
    static constexpr size_t HISTORY_MAX = 20;

    double get_current_load() const;
};

// ============================================================================
// 阶段兼容矩阵
// ============================================================================

bool stages_compatible(const std::string& stage_a, const std::string& stage_b);

// ============================================================================
// AdmissionController 准入控制器
// ============================================================================

class AdmissionController {
public:
    AdmissionController(MemoryBudgetManager& budget,
                        const FrameCostEstimator& estimator,
                        CPUBackpressure& cpu_bp,
                        const ResourceMonitor* monitor = nullptr);

    // 准入决策
    AdmissionResult admit(const FrameParams& params,
                          const std::string& stage = "DRIZZLE",
                          const FrameParams* worst_next_frame_params = nullptr);

    // 释放预约
    std::optional<MemoryReservation> release(const std::string& frame_id,
                                              std::optional<int64_t> actual_peak = std::nullopt);

    // 状态查询
    std::map<std::string, std::string> get_status() const;

private:
    MemoryBudgetManager& budget_;
    const FrameCostEstimator& estimator_;
    CPUBackpressure& cpu_bp_;
    const ResourceMonitor* monitor_;
    mutable std::mutex mutex_;
};

// ============================================================================
// 压力处理状态机
// ============================================================================

enum class PressureLevel {
    NORMAL = 0,         // 正常运行
    THROTTLE = 1,       // CPU 回滞
    STOP_ADMISSION = 2, // 停止准入
    WAIT_RELEASE = 3,   // 等待内存释放
    CLEAR_CACHE = 4,    // 清理可重建缓存
    PAUSE = 5,          // 暂停管线
    SPILL = 6,          // 显式 spill (H-003)
    OS_SWAP = 7,        // 最后手段 (应避免)
};

class PressureHandler {
public:
    explicit PressureHandler(const AdmissionController& controller);

    PressureLevel assess() const;
    PressureLevel get_level() const { return level_; }
    std::string get_action() const;

private:
    const AdmissionController& controller_;
    mutable std::mutex mutex_;
    mutable PressureLevel level_{PressureLevel::NORMAL};
};

} // namespace astrocs
