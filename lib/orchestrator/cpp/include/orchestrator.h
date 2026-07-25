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

// PipelineFrame 命名块容器 (stage1 流水线帧)
// 位于 lib/astro_image_io/include/, 需 Makefile 添加 -I 路径
// 重命名 aio_pipeline.h 的 PipelineStage typedef 为 AioPipelineStage,
// 避免与本文件的 enum class PipelineStage 冲突
// (aio_pipeline.h 定义了 C 风格 typedef enum {...} PipelineStage;)
#define PipelineStage AioPipelineStage
#include "aio_pipeline.h"
#undef PipelineStage

// 管线阶段枚举 (旧版, 5 阶段, 向后兼容, 供 run_single/run_batch 使用)
enum class PipelineStage {
    CALIBRATE   = 0,
    PLATESOLVE  = 1,
    PHOTOMETRIC = 2,
    DRIZZLE     = 3,
    STACK       = 4
};

// ============================================================================
// 新版管线阶段枚举 (spec §2.3.2 两段流水线 9 节点)
// 2026-07-18: 归档 GRADIENT_2D 节点 (stage1 不做曲面拟合和图像亮度修正,
//             那是 stage2 马赛克阶段的事; PHOTOMETRIC 已完成测光坐标系校准)
// 供 stage1/stage2 CLI 命令使用
// 第一段: 单帧预处理 (stage 0-6, FITS -> .hiss)
// 第二段: 多帧合并 (stage 7-8, .hiss -> .hcsd)
// ============================================================================
enum class PipelineStageV2 {
    // 第一段: 单帧预处理
    READ_FITS       = 0,  // aio_read_fits -> PipelineFrame
    CALIBRATE       = 1,  // calibration.dll (dark/bias/flat + 坏点修复)
    PLATESOLVE      = 2,  // ipv_solver.dll (WCS/SIP)
    PSF             = 3,  // dynamic_psf.dll (PSF 拟合)
    PHOTOMETRIC     = 4,  // photometric_calib.dll (F_syn 积分 + IRLS+Tukey 求 scale + 应用到图像)
    SNR             = 5,  // snr_estimator.dll (异常值剔除 + 测光不确定度 + 帧SNR基准)
    DRIZZLE         = 6,  // healpix_drizzle.dll (nside 1-2x, SNR同步转换, 落盘 .hiss)
    // 第二段: 多帧合并
    GRADIENT_SPHERE = 7,  // healpix_stack.dll hp_stack_gradient_corrected (球面梯度校准)
    STACK           = 8   // healpix_stack.dll (Winsorized sigma clip + SNR²加权叠加 -> .hcsd)
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

    // ========================================================================
    // spec §2.3 两段流水线 API (stage1/stage2 CLI 命令调用)
    // ========================================================================

    // stage1: 单帧预处理 (FITS -> .hiss, stage 0-7)
    // 参数:
    //   fits_path: 输入 FITS 文件路径
    //   output_hiss: 输出 .hiss 文件路径
    //   config_json: stage1 配置 JSON (含 gaia_data_dir, calibration_dir, filter 等)
    // 返回: TaskResult (success=true 表示全部 8 个 stage 执行成功)
    TaskResult run_stage1(const std::string& fits_path,
                          const std::string& output_hiss,
                          const std::string& config_json = "");

    // stage2: 多帧合并 (.hiss -> .hcsd, stage 8-9)
    // 参数:
    //   hiss_dir: 输入 .hiss 文件目录 (目录下所有 .hiss 文件作为输入)
    //   output_hcsd: 输出 .hcsd 文件路径
    //   config_json: stage2 配置 JSON (含 stack 参数等)
    // 返回: TaskResult (success=true 表示 GRADIENT_SPHERE + STACK 全部成功)
    TaskResult run_stage2(const std::string& hiss_dir,
                          const std::string& output_hcsd,
                          const std::string& config_json = "");

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

    // ========================================================================
    // stage1/stage2 流水线状态 (Task: stage handler 实现)
    // ========================================================================
    // PipelineFrame: stage1 流水线帧 (run_stage1 开始创建, 结束销毁)
    // 各 stage handler 通过 frame_ 读写命名块 (header/data/psf/snr 等)
    PipelineFrame* frame_ = nullptr;
    // 当前 stage1 输入 FITS 路径 (run_stage_read_fits 使用)
    std::string current_fits_path_;
    // 当前 stage1 输出 .hiss 路径 (run_stage_drizzle 使用)
    std::string current_output_path_;
    // stage2 输入 .hiss 文件列表 (run_stage_gradient_sphere / run_stage_stack 使用)
    std::vector<std::string> stage2_hiss_files_;
    // stage2 输出 .hcsd 路径 (run_stage_stack 使用)
    std::string current_output_hcsd_;
    // 当前 stage 配置 JSON (run_stage1/stage2 参数, 供 stage handler 读取)
    // GAP-016/GAP-017: run_stage_drizzle 读 nside_strategy/nside_override,
    //                  run_stage_gradient_sphere 读 sigma_clip_method 等
    std::string current_config_json_;
    // P03-002: 从 config 解析的 Gaia 数据目录 (init_platesolve_env 使用)
    // 空时默认为 project_root_dir_/GaiaDR3SP
    std::string config_gaia_data_dir_;

    // ========================================================================
    // PLATESOLVE 环境资源 (ipv_solver + gaia_client + star_detector)
    // 说明: ipv_solver.dll 依赖 gaia_client.dll 与 star_detector.dll 的句柄,
    //       这些 DLL 不在 DllLoader 的 10 模块枚举中, 需在 run_stage_platesolve
    //       首次执行时单独加载并创建句柄, 复用至 Orchestrator 析构.
    // ========================================================================
    std::string project_root_dir_;            // 项目根目录 (GaiaDR3SP 数据目录推导基准)
    void* gaia_client_dll_handle_ = nullptr;  // gaia_client.dll 的 HMODULE
    void* star_detector_dll_handle_ = nullptr;// star_detector.dll 的 HMODULE
    intptr_t gaia_client_handle_ = 0;         // GaiaClient* (由 gaia_client_create_ex 返回)
    intptr_t sdet_handle_ = 0;                // StarDetectorHandle (由 sdet_create 返回)
    void* ipv_solver_handle_ = nullptr;       // ipv solver 实例 (由 ipv_solve_create 返回)
    bool platesolve_env_ready_ = false;       // 环境是否已初始化成功

    // 初始化 PLATESOLVE 环境 (加载 DLL + 创建 handle), 失败返回 false
    bool init_platesolve_env(std::string& error_msg);
    // 释放 PLATESOLVE 环境 (销毁 handle + 卸载 DLL)
    void cleanup_platesolve_env();

    // 内部方法 (后续 Task 实现具体逻辑)
    bool run_stage_calibrate(TaskResult& result);
    bool run_stage_platesolve(TaskResult& result);
    bool run_stage_psf(TaskResult& result);
    bool run_stage_photometric(TaskResult& result);
    bool run_stage_drizzle(TaskResult& result);

    // spec §2.3 两段流水线 9 节点新增 handler (2026-07-18 归档 GRADIENT_2D)
    // stage 0: READ_FITS (aio_read_fits -> PipelineFrame)
    bool run_stage_read_fits(TaskResult& result);
    // stage 5: SNR (snr_estimator.dll)
    bool run_stage_snr(TaskResult& result);
    // stage 7: GRADIENT_SPHERE (healpix_stack.dll hp_stack_gradient_corrected)
    bool run_stage_gradient_sphere(TaskResult& result);
    // stage 8: STACK (healpix_stack.dll, Winsorized sigma clip + SNR²加权叠加)
    bool run_stage_stack(TaskResult& result);

    // 辅助方法
    static std::string stage_name(PipelineStage stage);
    static std::string state_name(TaskState state);
    // 新增: PipelineStageV2 阶段名称
    static std::string stage_name_v2(PipelineStageV2 stage);
};
