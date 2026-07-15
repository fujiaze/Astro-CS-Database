// ============================================================================
// orchestrator.h - 编排器核心类
// 功能: 管理管线阶段 (CALIBRATE -> PLATESOLVE -> PSF -> PHOTOMETRIC -> DRIZZLE)
//       支持单帧/批处理、暂停/恢复/中断、检查点续传
// 用途: 作为各 C++ DLL 模块的统一调度入口
//
// 设计说明:
//   本类为骨架实现, 各 run_stage_* 方法的具体逻辑将在后续 Task 中通过
//   动态加载各模块 DLL 实现。当前版本仅输出日志, 返回成功。
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

#include "dll_loader.h"
#include "checkpoint.h"
#include "logger.h"

// 管线阶段枚举 (与 aio_pipeline.h 的 PipelineStage 保持一致)
enum class PipelineStage {
    CALIBRATE   = 0,
    PLATESOLVE  = 1,
    PHOTOMETRIC = 2,
    DRIZZLE     = 3,
    STACK       = 4
};

// 任务状态
enum class TaskState {
    IDLE,        // 空闲
    RUNNING,     // 运行中
    PAUSED,      // 已暂停
    INTERRUPTED, // 已中断
    COMPLETED,   // 已完成
    FAILED       // 失败
};

// 阶段耗时记录
struct StageTiming {
    PipelineStage stage;
    std::string stage_name;
    double duration_sec;
    bool success;
};

// 任务结果
struct TaskResult {
    bool success;
    std::string frame_name;
    std::vector<StageTiming> timings;
    std::map<std::string, std::string> wcs_fields;   // WCS 字段
    std::map<std::string, std::string> photo_stats;  // 测光统计
    std::string output_ahpx_path;                    // 输出 .ahpx 路径
    std::string error_msg;
};

// 编排器配置
struct OrchestratorConfig {
    std::string calib_params_json;    // 校准参数 JSON
    std::string solve_params_json;    // 解析参数 JSON
    std::string photo_params_json;    // 测光参数 JSON
    std::string drizzle_params_json;  // drizzle 参数 JSON
    std::string log_dir;              // 日志目录
    std::string output_dir;           // 输出目录
    int threads = 0;                  // 线程数 (0=自动检测)
    bool enable_checkpoint = true;    // 启用检查点
    bool fresh_start = false;         // 忽略检查点重新开始
    std::string log_level = "INFO";   // 日志级别 (DEBUG/INFO/WARN/ERROR)
};

// ============================================================================
// Orchestrator 核心类
// ============================================================================
class Orchestrator {
public:
    Orchestrator();
    ~Orchestrator();

    // 加载配置 (从 JSON 文件读取参数)
    bool load_config(const std::string& config_path, std::string& error_msg);

    // 单帧处理
    TaskResult run_single(const std::string& fits_path);

    // 批量处理 (遍历目录下 FITS 文件)
    std::vector<TaskResult> run_batch(const std::string& dir_path);

    // 状态控制
    void pause();
    void resume();
    void interrupt();

    // 状态查询
    TaskState get_state() const { return state_; }
    std::string get_current_frame() const { return current_frame_; }
    PipelineStage get_current_stage() const { return current_stage_; }
    double get_elapsed_time() const;
    size_t get_memory_usage() const;
    int get_thread_count() const;

    // 检查点 (断点续传)
    bool save_checkpoint(const std::string& frame_name);
    bool load_checkpoint(const std::string& frame_name, PipelineStage& resume_from);

    // 新增: 设置检查点目录 (Task 3)
    void set_checkpoint_dir(const std::string& dir);

    // 新增: 设置 fresh_start 标志 (Task 3, 忽略检查点重新开始)
    void set_fresh_start(bool fresh) { config_.fresh_start = fresh; }

    // 新增: 设置启用检查点标志 (Task 3)
    void set_enable_checkpoint(bool en) { config_.enable_checkpoint = en; }

    // 新增: 获取 CheckpointManager 引用 (供 REPL/CLI 直接调用 list/clear 等)
    CheckpointManager& get_checkpoint_manager() { return checkpoint_mgr_; }

    // 新增: 初始化 DLL 加载
    // lib_base_dir: 项目根目录路径 (相对或绝对), 默认 ".." (从 cpp 目录上一级)
    bool init_dlls(const std::string& lib_base_dir, std::string& error_msg);

    // 新增: 获取 DLL 加载状态
    bool is_dlls_loaded() const { return dlls_loaded_; }

    // 新增: 获取 DllLoader 引用 (供高级用户直接调用)
    DllLoader& get_dll_loader() { return dll_loader_; }

private:
    OrchestratorConfig config_;
    std::atomic<TaskState> state_{TaskState::IDLE};
    std::string current_frame_;
    PipelineStage current_stage_{PipelineStage::CALIBRATE};
    std::mutex mutex_;
    std::thread worker_thread_;
    std::chrono::steady_clock::time_point start_time_;

    // DLL 加载器 (5 个模块: CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/DRIZZLE)
    DllLoader dll_loader_;
    bool dlls_loaded_ = false;

    // 检查点管理器 (断点续传, Task 3)
    CheckpointManager checkpoint_mgr_;

    // 内部方法 (后续 Task 实现具体逻辑)
    bool run_stage_calibrate(TaskResult& result);
    bool run_stage_platesolve(TaskResult& result);
    bool run_stage_psf(TaskResult& result);
    bool run_stage_photometric(TaskResult& result);
    bool run_stage_drizzle(TaskResult& result);

    // 辅助方法
    static std::string stage_name(PipelineStage stage);
    static std::string state_name(TaskState state);
};
