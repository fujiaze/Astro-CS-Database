// ============================================================================
// spill_manager.h - 高峰错峰、显式spill与恢复 (H-003)
//
// 规范来源: engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md
//   StageScheduler: 允许低峰阶段与高峰阶段错峰并行
//   SpillManager: 只spill已序列化、可恢复块
//   压力处理: 停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap
//   不得丢弃未持久化科学数据。
//
// Python 原型: engineering_authoritative/evidence/H-003/spill_manager.py
// 依赖: H-001 resource_monitor.h, H-002 admission_controller.h
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <optional>
#include <queue>

#include "resource_monitor.h"
#include "admission_controller.h"

namespace astrocs {

// ============================================================================
// 任务优先级
// ============================================================================

enum class TaskPriority {
    URGENT = 0,  // 紧急 (用户交互)
    HIGH = 1,    // 高 (Stage1 关键路径)
    NORMAL = 2,  // 正常 (批处理)
    LOW = 3,     // 低 (后台/可重建)
};

// ============================================================================
// 延迟任务记录
// ============================================================================

struct DeferredTask {
    std::string frame_id;
    std::string stage;
    TaskPriority priority;
    double deferred_at_sec;
    int defer_count = 0;

    // 优先队列比较: 优先级高 > 推迟次数少
    bool operator<(const DeferredTask& other) const {
        if (static_cast<int>(priority) != static_cast<int>(other.priority))
            return static_cast<int>(priority) > static_cast<int>(other.priority);
        return defer_count > other.defer_count;
    }
};

// ============================================================================
// PeakShifter 高峰错峰调度器
// ============================================================================

class PeakShifter {
public:
    static constexpr int PROMOTE_AFTER_DEFERS = 3;
    static constexpr double MAX_DEFER_SEC = 300.0;

    explicit PeakShifter(const PressureHandler& pressure_handler);

    // 检查任务是否应被推迟
    bool should_defer(TaskPriority priority) const;

    // 入队 (推迟)
    void defer(const DeferredTask& task);

    // 尝试恢复一个延迟任务 (压力降低时)
    std::optional<DeferredTask> try_resume();

    // 强制恢复全部 (紧急退出)
    std::vector<DeferredTask> drain_all();

    // 查询
    std::optional<DeferredTask> peek_next() const;
    size_t get_queue_size() const;

    struct Status {
        size_t queue_size;
        int total_deferred;
        int total_resumed;
        std::string pressure_level;
    };
    Status get_status() const;

private:
    const PressureHandler& pressure_handler_;
    mutable std::mutex mutex_;
    std::priority_queue<DeferredTask> queue_;
    std::atomic<int> total_deferred_{0};
    std::atomic<int> total_resumed_{0};
};

// ============================================================================
// Spill 记录
// ============================================================================

struct SpillRecord {
    std::string frame_id;
    std::string stage;
    std::string block_name;
    std::string spill_path;
    int64_t size_bytes;
    std::string checksum;          // SHA256 前 16 字符
    double spilled_at_sec;
    bool restored = false;
    std::optional<double> restored_at_sec;
};

// ============================================================================
// SpillManager 显式 spill 管理器
// ============================================================================

class SpillManager {
public:
    static constexpr const char* MANIFEST_FILENAME = "spill_manifest.json";

    explicit SpillManager(const std::string& spill_dir);

    // Spill (写出): 原子写入 + SHA256 校验
    SpillRecord spill(const std::string& frame_id,
                      const std::string& stage,
                      const std::string& block_name,
                      const std::vector<uint8_t>& data);

    // Restore (恢复): 读取 + 校验
    std::optional<std::vector<uint8_t>> restore(const std::string& frame_id,
                                                 const std::string& stage,
                                                 const std::string& block_name);

    // 恢复某帧全部 spill 块
    std::map<std::string, std::vector<uint8_t>> restore_frame(const std::string& frame_id);

    // 清理
    int cleanup_frame(const std::string& frame_id);
    int cleanup_all();

    // 查询
    bool has_spill(const std::string& frame_id, const std::string& stage, const std::string& block_name) const;
    std::vector<SpillRecord> get_records() const;
    int64_t get_total_spilled_bytes() const;

    struct Status {
        std::string spill_dir;
        size_t n_records;
        int64_t total_spilled_bytes;
        int n_restored;
    };
    Status get_status() const;

    // Spill 决策
    bool should_spill(PressureLevel level) const;

    // 选择应 spill 的块 (优先大块, 非当前阶段必需)
    std::vector<std::string> select_spill_blocks(
        const std::string& frame_id,
        const std::string& current_stage,
        const std::map<std::string, int64_t>& active_blocks  // block_name -> size_bytes
    ) const;

private:
    std::string spill_dir_;
    std::string manifest_path_;
    mutable std::mutex mutex_;
    std::map<std::string, SpillRecord> records_;  // key: "frame:stage:block"

    void save_manifest();
    void load_manifest();
    static std::string make_key(const std::string& frame_id, const std::string& stage, const std::string& block_name);
    static std::string compute_checksum(const std::vector<uint8_t>& data);
};

// ============================================================================
// RecoveryManager 恢复管理器
// ============================================================================

class RecoveryManager {
public:
    explicit RecoveryManager(SpillManager& spill_manager);

    // 恢复某帧全部 spill 块
    std::map<std::string, std::vector<uint8_t>> recover_frame(const std::string& frame_id);

    // 恢复单个块
    std::optional<std::vector<uint8_t>> recover_block(const std::string& frame_id,
                                                       const std::string& stage,
                                                       const std::string& block_name);

    // 可恢复性检查
    bool is_recoverable(const std::string& frame_id) const;

    // 获取恢复计划 (按 spill 时间排序)
    std::vector<SpillRecord> get_recovery_plan(const std::string& frame_id) const;

    // 帧完成清理
    int finalize_frame(const std::string& frame_id);

private:
    SpillManager& spill_manager_;
};

} // namespace astrocs
