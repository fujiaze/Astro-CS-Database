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
// 新版管线阶段枚举 (spec §2.3.2 两段流水线 10 节点)
// 2026-07-18: 归档 GRADIENT_2D 节点 (stage1 不做曲面拟合和图像亮度修正,
//             那是 stage2 马赛克阶段的事; PHOTOMETRIC 已完成测光坐标系校准)
// 2026-08-03: 新增 HISS_VERIFY 阶段 (drizzle 后验证 .hiss 文件完整性)
// 供 stage1/stage2 CLI 命令使用
// 第一段: 单帧预处理 (stage 0-7, FITS -> .hiss)
// 第二段: 多帧合并 (stage 8-9, .hiss -> .hcsd)
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
    HISS_VERIFY     = 7,  // 验证 .hiss 文件完整性 (可打开/metadata 正确/Tile 可读/signal/support 非全零)
    // 第二段: 多帧合并
    GRADIENT_SPHERE = 8,  // healpix_stack.dll hp_stack_gradient_corrected (球面梯度校准)
    STACK           = 9   // healpix_stack.dll (Winsorized sigma clip + SNR²加权叠加 -> .hcsd)
};

// ============================================================================
// 精度模式 (R10: FP32/FP64 双模式)
// FP32 (默认): signal 子块为 IEEE 754 binary32 (float)
// FP64:        signal 子块为 IEEE 754 binary64 (double), 精度模式写入 HISS metadata
//              drizzle engine 内部已用 double 累加, FP64 模式下 signal 输出 float64
//              (通过 hp_drizzle_run precision_mode 参数传递, HissWriter::add_tile_f64 输出)
// ============================================================================
enum class PrecisionMode : uint8_t {
    FP32 = 0,  // IEEE 754 binary32 (默认)
    FP64 = 1,  // IEEE 754 binary64
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

// ============================================================================
// AstroCS 进程退出码 (P03-003 / P04-002 扩展)
// 必需阶段/DLL/块失败必须返回非零退出码, 禁止静默跳过 (return true on skip)
// 0=成功; 1=通用错误; 2=DLL 加载失败; 3=必需块缺失;
// 4=校准失败; 5=PlateSolve 失败; 6=Drizzle 失败;
// 7=配置错误; 8=文件 I/O 错误;
// 9=超时 (P04-004 用); 10=用户取消 (P04-004 用);
// 20-29=模块特定非退出码 (star_detect/psf/photometric/snr/stack/hiss/hcsd/abi/input)
// 100+=模块扩展码 (预留)
// 与 engineering/contracts/error_code_registry.csv 一致
// ============================================================================
namespace AstroCsExitCode {
    constexpr int SUCCESS           = 0;
    constexpr int GENERIC_ERROR     = 1;
    constexpr int DLL_LOAD_FAILED   = 2;
    constexpr int BLOCK_MISSING     = 3;
    constexpr int CALIBRATE_FAILED  = 4;
    constexpr int PLATESOLVE_FAILED = 5;
    constexpr int DRIZZLE_FAILED    = 6;
    constexpr int CONFIG_ERROR      = 7;
    constexpr int FILE_IO_ERROR     = 8;
    constexpr int TIMEOUT           = 9;   // P04-004: 操作超时
    constexpr int CANCELLED         = 10;  // P04-004: 用户取消

    // 模块特定非退出码 (20-29, 不直接作为进程退出码, 但出现在 JSONL error.numeric_code)
    constexpr int STAR_DETECT_FAILED    = 20;
    constexpr int PSF_FAILED             = 21;
    constexpr int PHOTOMETRIC_FAILED     = 22;
    constexpr int SNR_FAILED             = 23;
    constexpr int STACK_FAILED          = 24;
    constexpr int HISS_INVALID           = 25;
    constexpr int HCSD_INVALID           = 26;
    constexpr int MODULE_ABI_UNSUPPORTED = 27;
    constexpr int INPUT_INVALID          = 28;
    constexpr int MODULE_SPECIFIC_BASE   = 100;

    // 字符串错误码 (供 JSONL error.code 字段使用, 稳定契约)
    inline const char* error_code_string(int code) {
        switch (code) {
            case SUCCESS:               return "ASTROCS_SUCCESS";
            case GENERIC_ERROR:         return "ASTROCS_INTERNAL";
            case DLL_LOAD_FAILED:       return "ASTROCS_MODULE_MISSING";
            case BLOCK_MISSING:         return "ASTROCS_BLOCK_MISSING";
            case CALIBRATE_FAILED:      return "ASTROCS_CALIBRATION_MISSING";
            case PLATESOLVE_FAILED:     return "ASTROCS_PLATESOLVE_FAILED";
            case DRIZZLE_FAILED:        return "ASTROCS_DRIZZLE_FAILED";
            case CONFIG_ERROR:          return "ASTROCS_CONFIG_INVALID";
            case FILE_IO_ERROR:         return "ASTROCS_FILE_IO_ERROR";
            case TIMEOUT:               return "ASTROCS_TIMEOUT";
            case CANCELLED:             return "ASTROCS_CANCELLED";
            case STAR_DETECT_FAILED:    return "ASTROCS_STAR_DETECT_FAILED";
            case PSF_FAILED:            return "ASTROCS_PSF_FAILED";
            case PHOTOMETRIC_FAILED:    return "ASTROCS_PHOTOMETRIC_FAILED";
            case SNR_FAILED:            return "ASTROCS_SNR_FAILED";
            case STACK_FAILED:          return "ASTROCS_STACK_FAILED";
            case HISS_INVALID:          return "ASTROCS_HISS_INVALID";
            case HCSD_INVALID:          return "ASTROCS_HCSD_INVALID";
            case MODULE_ABI_UNSUPPORTED: return "ASTROCS_MODULE_ABI_UNSUPPORTED";
            case INPUT_INVALID:         return "ASTROCS_INPUT_INVALID";
            default:                    return "ASTROCS_INTERNAL";
        }
    }

    // 判断数字 code 是否可作为进程退出码 (0-10)
    inline bool is_process_exit_code(int code) {
        return code >= 0 && code <= 10;
    }
}

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
    // P03-003: 进程退出码 (AstroCsExitCode::SUCCESS=0 表示成功, 非零表示具体错误)
    // 失败时由各 stage handler 设置对应错误码, 由 cli_command 直接返回
    int exit_code = AstroCsExitCode::SUCCESS;
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

    // P04-004: stage 级超时配置 (秒, <=0 表示不限制)
    // key: stage 名称 (READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE/GRADIENT_SPHERE/STACK)
    // value: 该 stage 的最大允许秒数
    std::map<std::string, double> stage_timeouts;

    // P04-004: 是否允许输出 partial 结果 (取消/超时时)
    // false (默认): 严格原子性, 取消/超时时删除部分输出
    // true: 取消/超时时保留已生成的部分输出 (标记 partial=true)
    bool allow_partial_output = false;

    // R10: 精度模式 (FP32 默认, FP64 高精度模式)
    // 传播到 drizzle 和 HISS 写入, 写入 .hiss metadata 的 precision_mode 字段
    PrecisionMode precision = PrecisionMode::FP32;
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

    // P04-004: 设置 stage 超时配置 (从 JSON config 解析后调用)
    // stage_timeouts: key=stage 名称, value=超时秒数 (<=0 不限制)
    void set_stage_timeouts(const std::map<std::string, double>& stage_timeouts) {
        config_.stage_timeouts = stage_timeouts;
    }

    // P04-004: 设置是否允许 partial 输出 (取消/超时时保留部分结果)
    void set_allow_partial_output(bool allow) { config_.allow_partial_output = allow; }

    // R10: 设置精度模式 (FP32/FP64), 传播到 drizzle 和 HISS 写入
    void set_precision(PrecisionMode mode) { config_.precision = mode; }

    // R10: 获取当前精度模式
    PrecisionMode get_precision() const { return config_.precision; }

    // P04-004: 取消 token - 请求取消当前运行
    // 线程安全: 设置 cancel_token_ 为 true, 各 stage 检查后停止
    void request_cancel();

    // P04-004: 检查取消 token 是否被设置
    bool is_cancelled() const { return cancel_token_.load(std::memory_order_acquire); }

    // P04-004: 检查超时标志是否被触发 (由 stage watchdog 设置)
    bool is_timed_out() const { return timeout_flag_.load(std::memory_order_acquire); }

    // P04-004: 重置取消/超时标志 (新一轮运行前调用)
    void reset_cancel_timeout();

    // P04-004: 获取当前 stage 名称 (用于 watchdog 与日志)
    std::string get_current_stage_name() const;

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

    // P04-004: 取消 token (atomic, 线程安全)
    // 由 request_cancel() 设置, 各 stage 在关键点检查并停止
    std::atomic<bool> cancel_token_{false};
    // P04-004: 超时标志 (atomic, 线程安全)
    // 由 stage watchdog 线程设置, stage handler 在循环中检查
    std::atomic<bool> timeout_flag_{false};
    // P04-004: watchdog 停止标志 (atomic, 线程安全)
    // 由 run_v2_with_timing 在 stage 完成后设置, 通知 watchdog 立即退出
    std::atomic<bool> stage_watchdog_stop_{false};
    // P04-004: 当前 stage 名称 (用于 watchdog 与日志)
    // 由 run_v2_with_timing 在 stage 开始时设置
    std::string current_stage_name_;
    // P04-004: 当前 stage 输出文件路径 (用于原子性清理)
    // 由 run_stage1/run_stage2 在开始时设置, 失败/取消/超时时删除
    std::string current_output_file_;

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
    // stage 7: HISS_VERIFY (验证 drizzle 输出的 .hiss 文件完整性)
    // R10: 同时验证 metadata 中 precision_mode 与请求一致
    bool run_stage_hiss_verify(TaskResult& result);
    // stage 8: GRADIENT_SPHERE (healpix_stack.dll hp_stack_gradient_corrected)
    bool run_stage_gradient_sphere(TaskResult& result);
    // stage 9: STACK (healpix_stack.dll, Winsorized sigma clip + SNR²加权叠加)
    bool run_stage_stack(TaskResult& result);

    // 辅助方法
    static std::string stage_name(PipelineStage stage);
    static std::string state_name(TaskState state);
    // 新增: PipelineStageV2 阶段名称
    static std::string stage_name_v2(PipelineStageV2 stage);

    // P04-004: 解析 config_json 中的 stage_timeouts 字段
    // 格式: {"stage_timeouts":{"READ_FITS":10.0,"CALIBRATE":60.0,...}}
    // 返回: 解析得到的 stage->seconds 映射 (空 map 表示未配置或解析失败)
    static std::map<std::string, double> parse_stage_timeouts(const std::string& config_json);

    // P04-004: 原子输出清理 - 删除部分生成的输出文件
    // path: 要删除的文件路径 (通常为 current_output_file_)
    // 返回: true 如果文件不存在或成功删除; false 如果删除失败
    bool cleanup_partial_output(const std::string& path);

    // P04-004: 检查 stage 是否应继续执行 (取消/超时检查)
    // 返回: true 继续; false 应停止 (取消或超时)
    // stage_name: 当前 stage 名称 (用于日志)
    // result: 若停止, 设置 result.error_msg 和 result.exit_code
    bool check_stage_continue(const std::string& stage_name, TaskResult& result);
};
