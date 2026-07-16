// ============================================================================
// orchestrator.cpp - Orchestrator 核心类实现 (骨架)
// 功能: 管线编排, 串联 5 个阶段 (CALIBRATE -> PLATESOLVE -> PSF -> PHOTOMETRIC -> DRIZZLE)
//
// 当前版本: 骨架实现
//   - 各 run_stage_* 方法仅输出日志, 返回 true
//   - load_config 简单读取 JSON 文件 (后续 Task 引入 JSON 库完善)
//   - 检查点为骨架 (返回 true)
//   - 内存使用查询返回 0 (后续 Task 实现)
// 后续 Task 将通过动态加载各模块 DLL 实现具体逻辑
// ============================================================================

#include "orchestrator.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <functional>

// 重命名 aio_pipeline.h 的 PipelineStage typedef 为 AioPipelineStage,
// 避免与 orchestrator.h 的 enum class PipelineStage 冲突
// (aio_pipeline.h 定义了 C 风格 typedef enum {...} PipelineStage;)
// astro_image_io.h / hp_drizzle_api.h / hp_stack_api.h 都会 include aio_pipeline.h
#define PipelineStage AioPipelineStage

// AIO 图像数据结构与元数据 (用于 run_stage_read_fits)
#include "astro_image_io.h"

// healpix_drizzle C API (用于 run_stage_drizzle)
#include "hp_drizzle_api.h"

// healpix_stack C API (用于 run_stage_gradient_sphere / run_stage_stack)
#include "hp_stack_api.h"

#undef PipelineStage

// ipv_solver C API (用于 run_stage_platesolve)
#include "ipv_api.h"

// gaia_client C API (PLATESOLVE 依赖: GaiaClient 句柄 + 锥形查询)
#include "gaia_client.h"

// star_detector C API (PLATESOLVE 依赖: StarDetector 句柄 + 星点检测)
#include "star_detector.h"

// dynamic_psf C API (用于 run_stage_psf)
#include "dynamic_psf.h"

// photometric_calib C API (用于 run_stage_photometric)
#include "photometric_calib.h"

// gradient_2d C API (用于 run_stage_gradient_2d)
#include "gradient_2d.h"

// snr_estimator C API (用于 run_stage_snr)
#include "snr_estimator.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 辅助: 阶段名称 / 状态名称
// ============================================================================
std::string Orchestrator::stage_name(PipelineStage stage) {
    switch (stage) {
        case PipelineStage::CALIBRATE:   return "CALIBRATE";
        case PipelineStage::PLATESOLVE:  return "PLATESOLVE";
        case PipelineStage::PHOTOMETRIC: return "PHOTOMETRIC";
        case PipelineStage::DRIZZLE:     return "DRIZZLE";
        case PipelineStage::STACK:       return "STACK";
        default:                         return "UNKNOWN";
    }
}

std::string Orchestrator::state_name(TaskState state) {
    switch (state) {
        case TaskState::IDLE:        return "IDLE";
        case TaskState::RUNNING:     return "RUNNING";
        case TaskState::PAUSED:      return "PAUSED";
        case TaskState::INTERRUPTED: return "INTERRUPTED";
        case TaskState::COMPLETED:   return "COMPLETED";
        case TaskState::FAILED:      return "FAILED";
        default:                     return "UNKNOWN";
    }
}

// ============================================================================
// stage_name_v2 - spec §2.3.2 两段流水线 10 节点阶段名称
// ============================================================================
std::string Orchestrator::stage_name_v2(PipelineStageV2 stage) {
    switch (stage) {
        case PipelineStageV2::READ_FITS:       return "READ_FITS";
        case PipelineStageV2::CALIBRATE:       return "CALIBRATE";
        case PipelineStageV2::PLATESOLVE:      return "PLATESOLVE";
        case PipelineStageV2::PSF:             return "PSF";
        case PipelineStageV2::PHOTOMETRIC:     return "PHOTOMETRIC";
        case PipelineStageV2::GRADIENT_2D:     return "GRADIENT_2D";
        case PipelineStageV2::SNR:             return "SNR";
        case PipelineStageV2::DRIZZLE:         return "DRIZZLE";
        case PipelineStageV2::GRADIENT_SPHERE: return "GRADIENT_SPHERE";
        case PipelineStageV2::STACK:           return "STACK";
        default:                               return "UNKNOWN";
    }
}

// ============================================================================
// 构造 / 析构
// ============================================================================
Orchestrator::Orchestrator() {
    start_time_ = std::chrono::steady_clock::now();
    // 初始化日志系统 (默认 lib/orchestrator/logs, INFO 级别)
    Logger::instance().init("lib/orchestrator/logs", LogLevel::INFO);
    LOG_INFO("orchestrator", "初始化编排器 (骨架版本)");
    LOG_INFO("orchestrator", "可用线程数: " + std::to_string(std::thread::hardware_concurrency()));
}

Orchestrator::~Orchestrator() {
    // 确保工作线程已结束
    if (worker_thread_.joinable()) {
        interrupt();
        worker_thread_.join();
    }
    // 释放 PLATESOLVE 环境 (gaia_client + star_detector + ipv_solver 句柄)
    cleanup_platesolve_env();
    LOG_INFO("orchestrator", "析构完成");
}

// ============================================================================
// load_config - 加载 JSON 配置文件 (骨架: 简单读取文件内容)
// ============================================================================
bool Orchestrator::load_config(const std::string& config_path, std::string& error_msg) {
    LOG_INFO("orchestrator", "加载配置: " + config_path);

    if (config_path.empty()) {
        error_msg = "配置路径为空";
        LOG_ERROR("orchestrator", error_msg);
        return false;
    }

    if (!fs::exists(config_path)) {
        error_msg = "配置文件不存在: " + config_path;
        LOG_ERROR("orchestrator", error_msg);
        return false;
    }

    // 读取整个文件到字符串
    std::ifstream ifs(config_path, std::ios::binary);
    if (!ifs.is_open()) {
        error_msg = "无法打开配置文件: " + config_path;
        LOG_ERROR("orchestrator", error_msg);
        return false;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();
    ifs.close();

    LOG_INFO("orchestrator", "配置文件大小: " + std::to_string(content.size()) + " 字节");

    // TODO: 后续 Task 引入 JSON 库, 解析各字段到 config_ 成员
    // 当前骨架: 仅把整个 JSON 文本存入 calib_params_json (便于后续解析)
    config_.calib_params_json = content;

    // 默认日志目录与输出目录 (若配置中未指定)
    if (config_.log_dir.empty()) {
        config_.log_dir = "lib/orchestrator/logs";
    }
    if (config_.output_dir.empty()) {
        config_.output_dir = "output";
    }

    // Task 3: 设置检查点目录为 <output_dir>/.checkpoint/
    std::string ckpt_dir = (fs::path(config_.output_dir) / ".checkpoint").string();
    checkpoint_mgr_.set_checkpoint_dir(ckpt_dir);
    LOG_INFO("orchestrator", "检查点目录: " + ckpt_dir);

    // Task 4: 解析日志级别 (大小写不敏感) 并设置 Logger
    // 当前骨架: 直接读取 config_.log_level 字段 (后续 Task 引入 JSON 库时从配置文件解析)
    if (!config_.log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(config_.log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("orchestrator", "日志级别设置为: " + config_.log_level);
    }

    LOG_INFO("orchestrator", "配置加载完成 (骨架: 字段解析待后续 Task 实现)");
    return true;
}

// ============================================================================
// run_single - 单帧端到端处理 (骨架: 串行调用 5 个阶段)
// Task 3: 集成检查点断点续传
//   - 如 config_.fresh_start=true, 删除现有检查点重新开始
//   - 如存在检查点且启用检查点, 从 get_resume_stage 恢复
//   - 每个阶段完成后调用 checkpoint_mgr_.update_stage
//   - 全部完成后标记 fully_completed
// ============================================================================
TaskResult Orchestrator::run_single(const std::string& fits_path) {
    TaskResult result;
    result.success = false;
    result.frame_name = fits_path;

    LOG_INFO("orchestrator", "========== 开始处理: " + fits_path + " ==========");

    if (fits_path.empty()) {
        result.error_msg = "FITS 路径为空";
        LOG_ERROR("orchestrator", result.error_msg);
        state_ = TaskState::FAILED;
        return result;
    }

    if (!fs::exists(fits_path)) {
        result.error_msg = "FITS 文件不存在: " + fits_path;
        LOG_ERROR("orchestrator", result.error_msg);
        state_ = TaskState::FAILED;
        return result;
    }

    // 提取帧名 (只取文件名部分作为检查点 key)
    std::string frame_name = fs::path(fits_path).filename().string();

    // Task 3: 检查点断点续传
    int resume_stage_id = 0;  // 默认从阶段 0 开始
    if (config_.enable_checkpoint) {
        if (config_.fresh_start) {
            // fresh_start 模式: 删除现有检查点重新开始
            if (checkpoint_mgr_.exists(frame_name)) {
                LOG_INFO("orchestrator", "fresh_start: 删除现有检查点 " + frame_name);
                checkpoint_mgr_.remove(frame_name);
            }
        } else if (checkpoint_mgr_.exists(frame_name)) {
            // 检查点存在: 检查是否已完成
            int rs = checkpoint_mgr_.get_resume_stage(frame_name);
            if (rs < 0) {
                LOG_INFO("orchestrator", "检查点显示已全部完成, 跳过: " + frame_name);
                result.success = true;
                result.error_msg = "已通过检查点 (fully_completed)";
                state_ = TaskState::COMPLETED;
                return result;
            }
            resume_stage_id = rs;
            LOG_INFO("orchestrator", "检查点恢复: 从阶段 " + std::to_string(resume_stage_id)
                     + " 继续 (" + frame_name + ")");
        }
    }

    // 进入运行状态
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_frame_ = fits_path;
        current_stage_ = PipelineStage::CALIBRATE;
        start_time_ = std::chrono::steady_clock::now();
    }
    state_ = TaskState::RUNNING;

    // 串行调用各阶段 (骨架实现)
    // stage_id 与 PipelineStage 对应: 0=CALIBRATE, 1=PLATESOLVE, 2=PHOTOMETRIC, 3=DRIZZLE
    // 注: PSF_FIT 复用 PLATESOLVE 枚举, 此处单独编号 (run_stage_psf 在阶段 1 之后)
    auto run_stage_with_timing = [&](PipelineStage stage,
                                      int stage_id,
                                      bool (Orchestrator::*fn)(TaskResult&),
                                      const char* name) -> bool {
        // 检查点续传: 跳过已完成阶段
        if (config_.enable_checkpoint && !config_.fresh_start &&
            stage_id < resume_stage_id) {
            LOG_INFO("orchestrator", "---------- 阶段: " + std::string(name) + " (检查点跳过, 已完成) ----------");
            StageTiming st;
            st.stage = stage;
            st.stage_name = name;
            st.duration_sec = 0.0;
            st.success = true;
            result.timings.push_back(st);
            return true;
        }

        LOG_INFO("orchestrator", "---------- 阶段: " + std::string(name) + " ----------");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_stage_ = stage;
        }
        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        StageTiming st;
        st.stage = stage;
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));

        // Task 3: 阶段完成后更新检查点 (仅对 4 个标准阶段编号)
        if (config_.enable_checkpoint && stage_id >= 0) {
            checkpoint_mgr_.update_stage(frame_name, stage_id, name, dur, ok);
            if (!ok) {
                LOG_WARN("orchestrator", "阶段失败, 检查点已保存 (可断点续传)");
            }
        }
        return ok;
    };

    bool ok = true;
    ok = ok && run_stage_with_timing(PipelineStage::CALIBRATE, 0,
                                      &Orchestrator::run_stage_calibrate, "CALIBRATE");
    ok = ok && run_stage_with_timing(PipelineStage::PLATESOLVE, 1,
                                      &Orchestrator::run_stage_platesolve, "PLATESOLVE");
    // PSF_FIT 不纳入 4 阶段检查点编号 (stage_id=-1 表示不记录检查点)
    ok = ok && run_stage_with_timing(PipelineStage::PLATESOLVE, -1,
                                      &Orchestrator::run_stage_psf, "PSF_FIT");
    ok = ok && run_stage_with_timing(PipelineStage::PHOTOMETRIC, 2,
                                      &Orchestrator::run_stage_photometric, "PHOTOMETRIC");
    ok = ok && run_stage_with_timing(PipelineStage::DRIZZLE, 3,
                                      &Orchestrator::run_stage_drizzle, "DRIZZLE");

    result.success = ok;
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;

    // Task 3: 全部成功后, 检查点已由 update_stage 自动标记 fully_completed
    // (current_stage_id >= 4 时 fully_completed=true)
    // 失败时保留检查点, 下次运行可断点续传

    LOG_INFO("orchestrator", "========== 处理" + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}

// ============================================================================
// run_batch - 批量处理 (遍历目录下 FITS 文件)
// ============================================================================
std::vector<TaskResult> Orchestrator::run_batch(const std::string& dir_path) {
    std::vector<TaskResult> results;

    LOG_INFO("orchestrator", "========== 批量处理目录: " + dir_path + " ==========");

    if (dir_path.empty()) {
        LOG_ERROR("orchestrator", "目录路径为空");
        return results;
    }

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        LOG_ERROR("orchestrator", "目录不存在或不是目录: " + dir_path);
        return results;
    }

    // 收集 FITS 文件 (扩展名 .fits / .fit / .fts)
    std::vector<std::string> fits_files;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        // 转小写
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".fits" || ext == ".fit" || ext == ".fts") {
            fits_files.push_back(entry.path().string());
        }
    }

    std::sort(fits_files.begin(), fits_files.end());
    LOG_INFO("orchestrator", "发现 " + std::to_string(fits_files.size()) + " 个 FITS 文件");

    // 逐个处理
    for (size_t i = 0; i < fits_files.size(); ++i) {
        LOG_INFO("orchestrator", "进度: " + std::to_string(i + 1) + "/" + std::to_string(fits_files.size()));

        // 检查中断标志
        if (state_ == TaskState::INTERRUPTED) {
            LOG_WARN("orchestrator", "已中断, 跳过剩余文件");
            break;
        }

        TaskResult r = run_single(fits_files[i]);
        results.push_back(r);
    }

    // 汇总
    size_t n_ok = 0;
    for (const auto& r : results) {
        if (r.success) ++n_ok;
    }
    LOG_INFO("orchestrator", "========== 批量处理完成: " + std::to_string(n_ok) + "/" + std::to_string(results.size()) + " 成功 ==========");
    return results;
}

// ============================================================================
// 状态控制: pause / resume / interrupt
// ============================================================================
void Orchestrator::pause() {
    LOG_INFO("orchestrator", "请求暂停");
    state_ = TaskState::PAUSED;
}

void Orchestrator::resume() {
    LOG_INFO("orchestrator", "请求恢复");
    state_ = TaskState::RUNNING;
}

void Orchestrator::interrupt() {
    LOG_INFO("orchestrator", "请求中断");
    state_ = TaskState::INTERRUPTED;
}

// ============================================================================
// 状态查询
// ============================================================================
double Orchestrator::get_elapsed_time() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - start_time_).count();
}

size_t Orchestrator::get_memory_usage() const {
    // 骨架实现: 返回 0 (后续 Task 实现实际内存统计)
    return 0;
}

int Orchestrator::get_thread_count() const {
    unsigned int hc = std::thread::hardware_concurrency();
    if (config_.threads > 0) {
        return config_.threads;
    }
    return (hc > 0) ? static_cast<int>(hc) : 1;
}

// ============================================================================
// 检查点 (Task 3: 通过 CheckpointManager 实现持久化)
// ============================================================================
bool Orchestrator::save_checkpoint(const std::string& frame_name) {
    LOG_INFO("orchestrator", "保存检查点: " + frame_name);
    // 保存当前进度 (如无现有数据则创建空检查点)
    CheckpointData data;
    if (!checkpoint_mgr_.load(frame_name, data)) {
        data.frame_name = frame_name;
        data.fits_path = current_frame_;
        data.current_stage_id = 0;
        data.fully_completed = false;
    }
    return checkpoint_mgr_.save(frame_name, data);
}

bool Orchestrator::load_checkpoint(const std::string& frame_name, PipelineStage& resume_from) {
    LOG_INFO("orchestrator", "加载检查点: " + frame_name);
    int rs = checkpoint_mgr_.get_resume_stage(frame_name);
    if (rs < 0) {
        // 已全部完成, resume_from 设为 STACK (无下一阶段)
        resume_from = PipelineStage::STACK;
        LOG_INFO("orchestrator", "检查点显示已全部完成");
        return true;
    }
    if (rs == 0 && !checkpoint_mgr_.exists(frame_name)) {
        // 检查点不存在, 从 CALIBRATE 开始
        resume_from = PipelineStage::CALIBRATE;
        return false;
    }
    // 将 stage_id 映射到 PipelineStage 枚举
    // 0=CALIBRATE, 1=PLATESOLVE, 2=PHOTOMETRIC, 3=DRIZZLE
    switch (rs) {
        case 0:  resume_from = PipelineStage::CALIBRATE;   break;
        case 1:  resume_from = PipelineStage::PLATESOLVE;  break;
        case 2:  resume_from = PipelineStage::PHOTOMETRIC; break;
        case 3:  resume_from = PipelineStage::DRIZZLE;     break;
        default: resume_from = PipelineStage::CALIBRATE;   break;
    }
    LOG_INFO("orchestrator", "恢复起点: stage_id=" + std::to_string(rs)
             + " (" + stage_name(resume_from) + ")");
    return true;
}

// ============================================================================
// set_checkpoint_dir - 设置检查点目录 (Task 3)
// ============================================================================
void Orchestrator::set_checkpoint_dir(const std::string& dir) {
    LOG_INFO("orchestrator", "设置检查点目录: " + dir);
    checkpoint_mgr_.set_checkpoint_dir(dir);
}

// ============================================================================
// init_dlls - 初始化 DLL 加载
// 调用 dll_loader_.load_all(lib_base_dir), 收集错误信息, 设置 dlls_loaded_ 标志
// lib_base_dir 为空时自动推导项目根目录 (orchestrator.exe 位于 lib/orchestrator/cpp/)
// ============================================================================
bool Orchestrator::init_dlls(const std::string& lib_base_dir, std::string& error_msg) {
    std::string base_dir = lib_base_dir;

    // 自动推导项目根目录: orchestrator.exe 位于 <root>/lib/orchestrator/cpp/
    if (base_dir.empty()) {
#ifdef _WIN32
        char exe_path[MAX_PATH] = {0};
        if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
            std::string ep(exe_path);
            // exe_path = <root>/lib/orchestrator/cpp/orchestrator.exe
            // 向上 4 级目录得到 <root> (去掉 cpp/orchestrator/lib 三级 + 文件名)
            size_t pos = std::string::npos;
            for (int i = 0; i < 4; ++i) {
                pos = ep.find_last_of("\\/", pos == std::string::npos ? std::string::npos : pos - 1);
                if (pos == std::string::npos) break;
            }
            if (pos != std::string::npos) {
                base_dir = ep.substr(0, pos);
                LOG_INFO("orchestrator", "自动推导项目根目录: " + base_dir);
            }
        }
#endif
        if (base_dir.empty()) {
            base_dir = ".";  // 回退到当前目录
        }
    }

    LOG_INFO("orchestrator", "初始化 DLL 加载 (lib_base_dir=" + base_dir + ")");

    // 保存项目根目录 (供 PLATESOLVE 推导 GaiaDR3SP 数据目录使用)
    project_root_dir_ = base_dir;

    bool ok = dll_loader_.load_all(base_dir);
    dlls_loaded_ = ok;

    if (!ok) {
        // 收集所有失败模块的错误信息 (spec §2.3.2 10 节点)
        std::stringstream ss;
        ss << "部分模块加载失败: ";
        bool first = true;
        std::vector<ModuleId> ids = {
            ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
            ModuleId::PSF, ModuleId::PHOTOMETRIC, ModuleId::GRADIENT_2D,
            ModuleId::SNR, ModuleId::DRIZZLE, ModuleId::GRADIENT_SPHERE,
            ModuleId::STACK
        };
        for (auto id : ids) {
            if (!dll_loader_.is_loaded(id)) {
                if (!first) ss << "; ";
                first = false;
                ss << dll_loader_.get_info(id).name << "="
                   << dll_loader_.get_error(id);
            }
        }
        error_msg = ss.str();
        LOG_WARN("orchestrator", error_msg);
        LOG_WARN("orchestrator", "加载失败的阶段将被跳过");
        return false;
    }

    // 加载成功, 输出各模块版本信息 (spec §2.3.2 10 节点)
    LOG_INFO("orchestrator", "全部 10 个模块加载成功");
    for (auto id : {ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
                    ModuleId::PSF, ModuleId::PHOTOMETRIC, ModuleId::GRADIENT_2D,
                    ModuleId::SNR, ModuleId::DRIZZLE, ModuleId::GRADIENT_SPHERE,
                    ModuleId::STACK}) {
        std::string name = dll_loader_.get_info(id).name;
        std::string ver = dll_loader_.get_version(id);
        LOG_INFO("orchestrator", "  " + name + " 版本: " + ver);
    }

    // 设置 CALIBRATE 模块的线程数 (其他模块暂无 set_num_threads 接口)
    int threads = get_thread_count();
    dll_loader_.set_num_threads(ModuleId::CALIBRATE, threads);
    LOG_INFO("orchestrator", "CALIBRATE 线程数: " + std::to_string(threads));

    return true;
}

// ============================================================================
// 各阶段实现 (骨架: 检查 DLL 加载状态, 输出日志, 返回 true)
// ============================================================================
bool Orchestrator::run_stage_calibrate(TaskResult& result) {
    LOG_INFO("orchestrator", "[CALIBRATE] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::CALIBRATE)) {
        LOG_WARN("orchestrator", "[CALIBRATE] CALIBRATE DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[CALIBRATE] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 获取函数指针
    auto fn_calibrate = dll_loader_.get_function<int (*)(
        const float*, int, int,
        const float*, const float*, const float*,
        float*, int, float, float*)>(
        ModuleId::CALIBRATE, "ac_calibrate_frame");
    auto fn_get_block_data = dll_loader_.get_function<void* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block_data");
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");
    auto fn_add_block_move = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block_move");

    if (!fn_calibrate || !fn_get_block_data || !fn_get_block || !fn_remove_block || !fn_add_block_move) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 函数指针获取失败");
        result.error_msg = "[CALIBRATE] 函数指针获取失败";
        return false;
    }

    // 从 frame_ 读取 data 块
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[CALIBRATE] data 块不存在");
        result.error_msg = "[CALIBRATE] data 块不存在";
        return false;
    }
    int width = data_block->dims[1];
    int height = data_block->dims[0];
    float* light = static_cast<float*>(data_block->data);
    int64_t n_pix = static_cast<int64_t>(width) * height;

    LOG_INFO("orchestrator", "[CALIBRATE] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 分配输出缓冲 (ac_calibrate_frame 要求调用者分配)
    float* out = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    if (out == nullptr) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 分配输出缓冲失败");
        result.error_msg = "[CALIBRATE] 分配输出缓冲失败";
        return false;
    }

    // 调用 ac_calibrate_frame
    // TODO: master_dark/flat/bias 应从 config_json 加载, 当前传 nullptr 走退化路径 (out=light)
    float actual_k = 0.0f;
    int ret = fn_calibrate(light, width, height,
                           nullptr, nullptr, nullptr,  // master_dark/flat/bias
                           out, 0, 1.0f, &actual_k);

    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] ac_calibrate_frame 失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] ac_calibrate_frame 失败";
        std::free(out);
        return false;
    }

    // 替换 data 块: remove 旧块 + add_block_move 新块 (转移所有权)
    fn_remove_block(frame_, "data");
    int dims[2] = {height, width};
    ret = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT32,
                            out, n_pix, dims, 2, "校准后 Light 像素");
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 写回 data 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] 写回 data 块失败";
        std::free(out);
        return false;
    }
    // out 所有权已转移给 frame_, 不再 free

    LOG_INFO("orchestrator", "[CALIBRATE] 完成 (退化路径: 无 master frames, out=light)");
    return true;
}

// ============================================================================
// 辅助: RA/DEC 字符串解析
// 支持 "HH MM SS.S" / "HH:MM:SS.S" / 浮点度 三种格式
// ============================================================================
static double parse_ra_hms(const char* s) {
    if (s == nullptr || s[0] == '\0') return 0.0;
    std::string str(s);
    std::replace(str.begin(), str.end(), ':', ' ');
    std::istringstream iss(str);
    double h, m, sec;
    if (iss >> h >> m >> sec) {
        return (h + m / 60.0 + sec / 3600.0) * 15.0;
    }
    // 单个浮点数 (度)
    iss.clear();
    iss.str(str);
    double deg;
    if (iss >> deg) return deg;
    return 0.0;
}

static double parse_dec_dms(const char* s) {
    if (s == nullptr || s[0] == '\0') return 0.0;
    std::string str(s);
    double sign = 1.0;
    if (!str.empty() && str[0] == '+') {
        str = str.substr(1);
    } else if (!str.empty() && str[0] == '-') {
        sign = -1.0;
        str = str.substr(1);
    }
    std::replace(str.begin(), str.end(), ':', ' ');
    std::istringstream iss(str);
    double d, m, sec;
    if (iss >> d >> m >> sec) {
        return sign * (d + m / 60.0 + sec / 3600.0);
    }
    iss.clear();
    iss.str(str);
    double deg;
    if (iss >> deg) return sign * deg;
    return 0.0;
}

// ============================================================================
// 辅助: 滤光片名称映射 (FITS FILTER 关键字 → filters.json 名称)
// ============================================================================
static std::string map_filter_name(const std::string& fits_filter) {
    // 大小写不敏感比较的辅助
    auto ieq = [](const std::string& a, const char* b) {
        return std::equal(a.begin(), a.end(), b, b + std::strlen(b),
            [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); });
    };
    if (ieq(fits_filter, "Red") || ieq(fits_filter, "R")) return "Baader R";
    if (ieq(fits_filter, "Green") || ieq(fits_filter, "G")) return "Baader G";
    if (ieq(fits_filter, "Blue") || ieq(fits_filter, "B")) return "Baader B";
    if (ieq(fits_filter, "Lum") || ieq(fits_filter, "L") || ieq(fits_filter, "Luminance"))
        return "Baader UV/IR Cut / L CMOS Optimized";
    // 未匹配时原样返回 (可能本身就是 filters.json 中的名称)
    return fits_filter;
}

// ============================================================================
// 辅助: 从 filters.json 加载指定滤光片的波长与透过率数组
// 简化 JSON 解析: 定位 "filter_name" 键 → 提取 wavelength_nm 和 value 数组
// 返回: true=成功, false=失败
// ============================================================================
static bool load_filter_curve(const std::string& json_path,
                               const std::string& filter_name,
                               std::vector<double>& out_wl,
                               std::vector<double>& out_trans) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        LOG_ERROR("orchestrator", "无法打开 filters.json: " + json_path);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // 定位 "filter_name" 键 (作为 JSON 对象键, 带引号)
    std::string key = "\"" + filter_name + "\"";
    size_t pos = content.find(key);
    if (pos == std::string::npos) {
        LOG_ERROR("orchestrator", "滤光片 '" + filter_name + "' 未在 filters.json 中找到");
        return false;
    }

    // 从 key 之后查找 "wavelength_nm" 数组
    auto extract_array = [&content, &pos](const std::string& arr_key,
                                           std::vector<double>& out) -> bool {
        size_t kpos = content.find(arr_key, pos);
        if (kpos == std::string::npos) return false;
        size_t bracket_start = content.find('[', kpos);
        if (bracket_start == std::string::npos) return false;
        size_t bracket_end = content.find(']', bracket_start);
        if (bracket_end == std::string::npos) return false;

        std::string arr_str = content.substr(bracket_start + 1,
                                              bracket_end - bracket_start - 1);
        std::replace(arr_str.begin(), arr_str.end(), ',', ' ');
        std::istringstream iss(arr_str);
        out.clear();
        double v;
        while (iss >> v) out.push_back(v);
        return !out.empty();
    };

    if (!extract_array("\"wavelength_nm\"", out_wl) ||
        !extract_array("\"value\"", out_trans)) {
        LOG_ERROR("orchestrator", "解析滤光片 " + filter_name + " 的数组失败");
        return false;
    }
    if (out_wl.size() != out_trans.size()) {
        LOG_ERROR("orchestrator", "滤光片 " + filter_name +
                  " 数组长度不一致: wl=" + std::to_string(out_wl.size()) +
                  " trans=" + std::to_string(out_trans.size()));
        return false;
    }
    return true;
}

// ============================================================================
// 辅助: 从 gaia_client 获取光谱波长数组
// spectrum_wl = [start_nm + i*step_nm for i in range(count)]
// ============================================================================
static bool build_spectrum_wl(void* gaia_dll, intptr_t client_handle,
                               std::vector<double>& out_wl) {
    if (gaia_dll == nullptr || client_handle == 0) return false;
    HMODULE gaia_h = static_cast<HMODULE>(gaia_dll);
    using get_params_fn = int (*)(GaiaClient*, int*, int*, int*);
    auto fn_get_params = reinterpret_cast<get_params_fn>(
        GetProcAddress(gaia_h, "gaia_client_get_spectrum_params"));
    if (!fn_get_params) {
        LOG_ERROR("orchestrator", "gaia_client_get_spectrum_params 函数未找到");
        return false;
    }
    int start_nm = 0, step_nm = 0, count = 0;
    int ret = fn_get_params(reinterpret_cast<GaiaClient*>(client_handle),
                            &start_nm, &step_nm, &count);
    // 注意: gaia_client_get_spectrum_params 使用布尔约定 (1=成功, 0=失败),
    // 而非错误码约定 (0=成功, 非0=失败)。此处 ret==1 表示成功找到光谱数据。
    if (ret != 1 || count <= 0 || step_nm <= 0) {
        LOG_ERROR("orchestrator", "gaia_client_get_spectrum_params 失败: ret="
                  + std::to_string(ret) + " count=" + std::to_string(count));
        return false;
    }
    out_wl.resize(count);
    for (int i = 0; i < count; ++i) {
        out_wl[i] = static_cast<double>(start_nm + i * step_nm);
    }
    return true;
}

// ============================================================================
// 辅助: 从 header KV 块读取 WCS 参数 (供 PHOTOMETRIC / GRADIENT_2D 使用)
// ============================================================================
struct WcsHeaderParams {
    double crval1 = 0, crval2 = 0;
    double crpix1 = 0, crpix2 = 0;
    double cd11 = 0, cd12 = 0, cd21 = 0, cd22 = 0;
    int sip_order = 0;
    double sip_a[36] = {0}, sip_b[36] = {0};
    double sip_ap[36] = {0}, sip_bp[36] = {0};
    bool has_ap = false;
};

static bool read_wcs_from_header(
    const std::function<const char*(const char*, const char*)>& fn_kv_get,
    const std::function<double(const char*, const char*, double)>& fn_kv_get_double,
    WcsHeaderParams& wcs) {
    wcs.crval1 = fn_kv_get_double("header", "CRVAL1", 0.0);
    wcs.crval2 = fn_kv_get_double("header", "CRVAL2", 0.0);
    wcs.crpix1 = fn_kv_get_double("header", "CRPIX1", 0.0);
    wcs.crpix2 = fn_kv_get_double("header", "CRPIX2", 0.0);
    wcs.cd11 = fn_kv_get_double("header", "CD1_1", 0.0);
    wcs.cd12 = fn_kv_get_double("header", "CD1_2", 0.0);
    wcs.cd21 = fn_kv_get_double("header", "CD2_1", 0.0);
    wcs.cd22 = fn_kv_get_double("header", "CD2_2", 0.0);

    if (wcs.crval1 == 0.0 && wcs.crval2 == 0.0 &&
        wcs.crpix1 == 0.0 && wcs.crpix2 == 0.0) {
        return false;  // WCS 缺失
    }

    // SIP 前向系数 (A/B)
    auto read_sip = [&](const char* order_key, const char* prefix, double* coeffs) -> int {
        const char* order_str = fn_kv_get("header", order_key);
        if (order_str == nullptr) return 0;
        int order = std::atoi(order_str);
        if (order <= 0) return 0;
        char key[16];
        for (int i = 0; i <= order; ++i) {
            for (int j = 0; j <= order - i; ++j) {
                int idx = i * 6 + j;
                if (idx < 36) {
                    std::snprintf(key, sizeof(key), "%s_%d_%d", prefix, i, j);
                    coeffs[idx] = fn_kv_get_double("header", key, 0.0);
                }
            }
        }
        return order;
    };

    int a_order = read_sip("A_ORDER", "A", wcs.sip_a);
    int b_order = read_sip("B_ORDER", "B", wcs.sip_b);
    wcs.sip_order = std::max(a_order, b_order);

    int ap_order = read_sip("AP_ORDER", "AP", wcs.sip_ap);
    int bp_order = read_sip("BP_ORDER", "BP", wcs.sip_bp);
    wcs.has_ap = (ap_order > 0 && bp_order > 0);
    return true;
}

// ============================================================================
// 辅助: 为 GRADIENT_2D 预计算 Gaia F_syn 数组
// (gradient_2d_calibrate 需要 gaia_fsyn, 但不接收 gaia_client handle;
//  photometric_calib.dll 的 spectrum_integrator 是内部 C++ 函数未导出 C API,
//  此处用线性插值+梯形积分做简化版 F_syn 计算, 与 pc_calibrate_simple_with_gaia
//  内部的 Akima+Simpson 结果可能有微小差异, 但 GRADIENT_2D 做空间梯度拟合是
//  相对测量, 系统偏移会抵消, 简化积分足够)
// ============================================================================
static bool compute_gaia_fsyn_for_gradient(
    void* gaia_dll, intptr_t client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const std::vector<double>& filter_wl,
    const std::vector<double>& filter_trans,
    const std::vector<double>& spectrum_wl,
    std::vector<double>& out_ra,
    std::vector<double>& out_dec,
    std::vector<double>& out_mag,
    std::vector<double>& out_fsyn) {

    if (gaia_dll == nullptr || client_handle == 0) return false;
    HMODULE gaia_h = static_cast<HMODULE>(gaia_dll);

    // 1. 锥形搜索 (带光谱)
    using cone_spec_fn = int (*)(GaiaClient*, double, double, double, double, double,
        GaiaSpectrumStar**, uint8_t**, int*);
    auto fn_cone_spec = reinterpret_cast<cone_spec_fn>(
        GetProcAddress(gaia_h, "gaia_client_cone_search_with_spectrum"));
    if (!fn_cone_spec) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] gaia_client_cone_search_with_spectrum 函数未找到");
        return false;
    }

    // 自适应星等查询 (与 pc_calibrate_simple_with_gaia 一致: 12->16 逐步增大)
    static const double mag_max_arr[] = {12.0, 13.0, 14.0, 15.0, 16.0};
    GaiaSpectrumStar* spec_stars = nullptr;
    uint8_t* spectra_buf = nullptr;
    int n_gaia = 0;
    int rc = 0;

    for (int i = 0; i < 5; ++i) {
        double mag_max_try = mag_max_arr[i];
        rc = fn_cone_spec(reinterpret_cast<GaiaClient*>(client_handle),
                          ra_center, dec_center, radius_deg, mag_min, mag_max_try,
                          &spec_stars, &spectra_buf, &n_gaia);
        if (rc != 0) {
            LOG_ERROR("orchestrator", "[GRADIENT_2D] 锥形搜索失败 rc=" + std::to_string(rc));
            if (spec_stars) { std::free(spec_stars); spec_stars = nullptr; }
            if (spectra_buf) { std::free(spectra_buf); spectra_buf = nullptr; }
            return false;
        }
        LOG_INFO("orchestrator", "[GRADIENT_2D] 自适应星等查询: mag_max="
                 + std::to_string(mag_max_try) + ", n_gaia=" + std::to_string(n_gaia));
        if (n_gaia >= 2000 || i == 4) break;
        if (spec_stars) { std::free(spec_stars); spec_stars = nullptr; }
        if (spectra_buf) { std::free(spectra_buf); spectra_buf = nullptr; }
    }

    if (n_gaia <= 0 || spec_stars == nullptr || spectra_buf == nullptr) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] 无光谱星 (n_gaia=" + std::to_string(n_gaia) + ")");
        if (spec_stars) std::free(spec_stars);
        if (spectra_buf) std::free(spectra_buf);
        return false;
    }

    int spec_count = (int)spectrum_wl.size();
    int filt_count = (int)filter_wl.size();

    // 2. 滤光片曲线线性插值到光谱网格
    std::vector<double> filter_on_grid(spec_count, 0.0);
    for (int i = 0; i < spec_count; ++i) {
        double wl = spectrum_wl[i];
        if (wl < filter_wl[0] || wl > filter_wl[filt_count - 1]) {
            filter_on_grid[i] = 0.0;  // 范围外
            continue;
        }
        int lo = 0, hi = filt_count - 2;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (wl >= filter_wl[mid] && wl <= filter_wl[mid + 1]) {
                double dx = filter_wl[mid + 1] - filter_wl[mid];
                double t = (dx > 0.0) ? (wl - filter_wl[mid]) / dx : 0.0;
                filter_on_grid[i] = filter_trans[mid] * (1.0 - t) + filter_trans[mid + 1] * t;
                break;
            }
            if (wl < filter_wl[mid]) hi = mid - 1;
            else lo = mid + 1;
        }
    }

    // 3. 预计算 weighted_wl[i] = λ_i × T(λ_i)
    std::vector<double> weighted_wl(spec_count);
    for (int i = 0; i < spec_count; ++i) {
        weighted_wl[i] = spectrum_wl[i] * filter_on_grid[i];
    }

    // 4. 计算每颗星的 F_syn (梯形积分: ∫ S(λ)·T(λ)·λ dλ × 10^(-0.4*mag_g))
    out_ra.resize(n_gaia);
    out_dec.resize(n_gaia);
    out_mag.resize(n_gaia);
    out_fsyn.resize(n_gaia, 0.0);

    int n_valid = 0;
    for (int i = 0; i < n_gaia; ++i) {
        out_ra[i] = spec_stars[i].ra;
        out_dec[i] = spec_stars[i].dec;
        out_mag[i] = spec_stars[i].magG;

        double mag_factor = std::pow(10.0, -0.4 * spec_stars[i].magG);
        const uint8_t* spec_i = spectra_buf + (size_t)i * spec_count;

        double integral = 0.0;
        for (int j = 0; j < spec_count - 1; ++j) {
            double s0 = (double)spec_i[j] * mag_factor * weighted_wl[j];
            double s1 = (double)spec_i[j + 1] * mag_factor * weighted_wl[j + 1];
            integral += 0.5 * (s0 + s1) * (spectrum_wl[j + 1] - spectrum_wl[j]);
        }
        out_fsyn[i] = integral;
        if (integral > 0.0 && std::isfinite(integral)) ++n_valid;
    }

    LOG_INFO("orchestrator", "[GRADIENT_2D] F_syn 计算完成: " + std::to_string(n_valid)
             + "/" + std::to_string(n_gaia) + " 颗有效");

    std::free(spec_stars);
    std::free(spectra_buf);
    return true;
}

// ============================================================================
// init_platesolve_env - 初始化 PLATESOLVE 环境
// 加载 gaia_client.dll + star_detector.dll, 创建 handle, 创建 ipv_solver 实例
// 复用至 Orchestrator 析构 (cleanup_platesolve_env 释放)
// ============================================================================
bool Orchestrator::init_platesolve_env(std::string& error_msg) {
    LOG_INFO("orchestrator", "[PLATESOLVE] 初始化环境 (gaia_client + star_detector + ipv_solver)");

    // 检查前置条件: PLATESOLVE 模块已加载 (ipv_solver.dll)
    if (!dll_loader_.is_loaded(ModuleId::PLATESOLVE)) {
        error_msg = "PLATESOLVE (ipv_solver.dll) 未加载";
        return false;
    }

    // 1. 加载 gaia_client.dll (位于 lib/photometric_calib/cpp/)
    std::string gaia_dll_path = project_root_dir_ + "/lib/photometric_calib/cpp/gaia_client.dll";
    HMODULE gaia_h = LoadLibraryExA(gaia_dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (gaia_h == nullptr) {
        error_msg = "加载 gaia_client.dll 失败: " + gaia_dll_path;
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        return false;
    }
    gaia_client_dll_handle_ = gaia_h;
    LOG_INFO("orchestrator", "[PLATESOLVE] gaia_client.dll 加载成功");

    // 2. 加载 star_detector.dll (位于 lib/star_detector/)
    std::string sdet_dll_path = project_root_dir_ + "/lib/star_detector/star_detector.dll";
    HMODULE sdet_h = LoadLibraryExA(sdet_dll_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (sdet_h == nullptr) {
        error_msg = "加载 star_detector.dll 失败: " + sdet_dll_path;
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    star_detector_dll_handle_ = sdet_h;
    LOG_INFO("orchestrator", "[PLATESOLVE] star_detector.dll 加载成功");

    // 3. 创建 GaiaClient (使用 GaiaDR3SP 数据目录, db_type=GAIA_DB_DR3SP=2)
    using gaia_create_ex_fn = GaiaClient* (*)(const char*, GaiaDbType);
    auto fn_gaia_create_ex = reinterpret_cast<gaia_create_ex_fn>(
        GetProcAddress(gaia_h, "gaia_client_create_ex"));
    if (fn_gaia_create_ex == nullptr) {
        error_msg = "gaia_client_create_ex 函数未找到";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    std::string gaia_data_dir = project_root_dir_ + "/GaiaDR3SP";
    GaiaClient* gaia_client = fn_gaia_create_ex(gaia_data_dir.c_str(), GAIA_DB_DR3SP);
    if (gaia_client == nullptr) {
        error_msg = "GaiaClient 创建失败 (data_dir=" + gaia_data_dir + ")";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    gaia_client_handle_ = reinterpret_cast<intptr_t>(gaia_client);
    LOG_INFO("orchestrator", "[PLATESOLVE] GaiaClient 创建成功 (data_dir=" + gaia_data_dir + ")");

    // 4. 创建 StarDetector (使用默认参数, fitRadius=0 表示自动)
    using sdet_create_fn = StarDetectorHandle (*)(const SDetParams*);
    auto fn_sdet_create = reinterpret_cast<sdet_create_fn>(
        GetProcAddress(sdet_h, "sdet_create"));
    if (fn_sdet_create == nullptr) {
        error_msg = "sdet_create 函数未找到";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    SDetParams sdet_params;
    std::memset(&sdet_params, 0, sizeof(sdet_params));
    sdet_params.structureLayers = 5;
    sdet_params.hotPixelFilterRadius = 1;
    sdet_params.iterativeClipSigma = 9.0f;
    sdet_params.iterativeMaxRounds = 5;
    sdet_params.medianFilterDetail = 1;
    sdet_params.maxStars = 2000;
    sdet_params.fitRadius = 0;  // 0 = 自动
    sdet_params.fwhmClipSigma = 3.0f;
    sdet_params.maxAxisRatio = 2.0f;
    StarDetectorHandle sdet = fn_sdet_create(&sdet_params);
    if (sdet == nullptr) {
        error_msg = "StarDetector 创建失败";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    sdet_handle_ = reinterpret_cast<intptr_t>(sdet);
    LOG_INFO("orchestrator", "[PLATESOLVE] StarDetector 创建成功 (fitRadius=0 自动)");

    // 5. 创建 IPVSolver 实例 (通过 PLATESOLVE 模块的 ipv_solve_create)
    auto fn_ipv_create = dll_loader_.get_function<void* (*)()>(
        ModuleId::PLATESOLVE, "ipv_solve_create");
    auto fn_ipv_set_gaia = dll_loader_.get_function<void (*)(void*, intptr_t)>(
        ModuleId::PLATESOLVE, "ipv_set_gaia_handle");
    auto fn_ipv_set_detector = dll_loader_.get_function<void (*)(void*, intptr_t)>(
        ModuleId::PLATESOLVE, "ipv_set_detector_handle");

    if (!fn_ipv_create || !fn_ipv_set_gaia || !fn_ipv_set_detector) {
        error_msg = "ipv 函数指针获取失败 (ipv_solve_create/ipv_set_gaia_handle/ipv_set_detector_handle)";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }

    ipv_solver_handle_ = fn_ipv_create();
    if (ipv_solver_handle_ == nullptr) {
        error_msg = "ipv_solve_create 返回 nullptr";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    fn_ipv_set_gaia(ipv_solver_handle_, gaia_client_handle_);
    fn_ipv_set_detector(ipv_solver_handle_, sdet_handle_);
    LOG_INFO("orchestrator", "[PLATESOLVE] IPVSolver 创建成功, 已注入 Gaia + StarDetector 句柄");

    platesolve_env_ready_ = true;
    LOG_INFO("orchestrator", "[PLATESOLVE] 环境初始化完成");
    return true;
}

// ============================================================================
// cleanup_platesolve_env - 释放 PLATESOLVE 环境
// 销毁顺序: ipv_solver -> star_detector -> gaia_client -> 卸载 DLL
// ============================================================================
void Orchestrator::cleanup_platesolve_env() {
    if (!platesolve_env_ready_ && ipv_solver_handle_ == nullptr &&
        gaia_client_handle_ == 0 && sdet_handle_ == 0 &&
        gaia_client_dll_handle_ == nullptr && star_detector_dll_handle_ == nullptr) {
        return;
    }
    LOG_INFO("orchestrator", "[PLATESOLVE] 释放环境资源");

    // 1. 销毁 ipv_solver 实例
    if (ipv_solver_handle_ != nullptr && dll_loader_.is_loaded(ModuleId::PLATESOLVE)) {
        auto fn_ipv_destroy = dll_loader_.get_function<void (*)(void*)>(
            ModuleId::PLATESOLVE, "ipv_solve_destroy");
        if (fn_ipv_destroy) fn_ipv_destroy(ipv_solver_handle_);
        ipv_solver_handle_ = nullptr;
    }

    // 2. 销毁 StarDetector
    if (sdet_handle_ != 0 && star_detector_dll_handle_ != nullptr) {
        using sdet_destroy_fn = void (*)(StarDetectorHandle);
        auto fn_sdet_destroy = reinterpret_cast<sdet_destroy_fn>(
            GetProcAddress(static_cast<HMODULE>(star_detector_dll_handle_), "sdet_destroy"));
        if (fn_sdet_destroy) {
            fn_sdet_destroy(reinterpret_cast<StarDetectorHandle>(sdet_handle_));
        }
        sdet_handle_ = 0;
    }

    // 3. 销毁 GaiaClient
    if (gaia_client_handle_ != 0 && gaia_client_dll_handle_ != nullptr) {
        using gaia_destroy_fn = void (*)(GaiaClient*);
        auto fn_gaia_destroy = reinterpret_cast<gaia_destroy_fn>(
            GetProcAddress(static_cast<HMODULE>(gaia_client_dll_handle_), "gaia_client_destroy"));
        if (fn_gaia_destroy) {
            fn_gaia_destroy(reinterpret_cast<GaiaClient*>(gaia_client_handle_));
        }
        gaia_client_handle_ = 0;
    }

    // 4. 卸载 DLL
    if (star_detector_dll_handle_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(star_detector_dll_handle_));
        star_detector_dll_handle_ = nullptr;
    }
    if (gaia_client_dll_handle_ != nullptr) {
        FreeLibrary(static_cast<HMODULE>(gaia_client_dll_handle_));
        gaia_client_dll_handle_ = nullptr;
    }
    platesolve_env_ready_ = false;
}

// ============================================================================
// run_stage_platesolve - PLATESOLVE 阶段实现 (ipv_solver.dll 内存接口)
// 流程:
//   1. 读取 PipelineFrame "data" 块 (FLOAT32 [H,W])
//   2. 读取 "header" KV 块的 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ
//   3. 调用 ipv_solve_from_memory 求解 WCS+SIP
//   4. 将 WCS+SIP 结果写回 "header" KV 块
//   5. (可选) 写入 "star_det" 块 (FLOAT32 [N,4]: x,y,flux,mag)
//   6. (可选) 写入 "gaia_cat" 块 (FLOAT64 [N,3]: ra,dec,mag)
// 约束: 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据
// ============================================================================
bool Orchestrator::run_stage_platesolve(TaskResult& result) {
    LOG_INFO("orchestrator", "[PLATESOLVE] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PLATESOLVE)) {
        LOG_WARN("orchestrator", "[PLATESOLVE] PLATESOLVE DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[PLATESOLVE] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 初始化 PLATESOLVE 环境 (首次调用时加载 gaia_client + star_detector + ipv_solver)
    if (!platesolve_env_ready_) {
        std::string err;
        if (!init_platesolve_env(err)) {
            LOG_ERROR("orchestrator", "[PLATESOLVE] 环境初始化失败: " + err);
            result.error_msg = "[PLATESOLVE] 环境初始化失败: " + err;
            return false;
        }
    }

    // 获取 AIO 函数指针
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_kv_get = dll_loader_.get_function<const char* (*)(
        const PipelineFrame*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_get");
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");
    auto fn_kv_set = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_set");
    auto fn_kv_set_double = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_set_double");
    auto fn_add_block = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        const void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");

    if (!fn_get_block || !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] AIO 函数指针获取失败");
        result.error_msg = "[PLATESOLVE] AIO 函数指针获取失败";
        return false;
    }

    // 1. 读取 data 块 (FLOAT32 [H,W])
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] data 块不存在");
        result.error_msg = "[PLATESOLVE] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const float* pixels = static_cast<const float*>(data_block->data);
    LOG_INFO("orchestrator", "[PLATESOLVE] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 2. 从 header KV 读取初始指向 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
    //    约束: 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据
    const char* objctra = fn_kv_get(frame_, "header", "OBJCTRA");
    const char* objctdec = fn_kv_get(frame_, "header", "OBJCTDEC");
    double focal_length = fn_kv_get_double(frame_, "header", "FOCALLEN", 0.0);
    double pixel_size = fn_kv_get_double(frame_, "header", "XPIXSZ", 0.0);

    double ra0 = parse_ra_hms(objctra);
    double dec0 = parse_dec_dms(objctdec);

    LOG_INFO("orchestrator", "[PLATESOLVE] 初始指向: OBJCTRA='" + std::string(objctra ? objctra : "")
             + "' -> ra0=" + std::to_string(ra0) + "deg, OBJCTDEC='" + std::string(objctdec ? objctdec : "")
             + "' -> dec0=" + std::to_string(dec0) + "deg");
    LOG_INFO("orchestrator", "[PLATESOLVE] 焦距=" + std::to_string(focal_length)
             + "mm, 像素尺寸=" + std::to_string(pixel_size) + "um");

    // 3. 调用 ipv_solve_from_memory (内存接口, 无临时文件)
    auto fn_ipv_solve = dll_loader_.get_function<int (*)(
        void*, const float*, int, int, double, double, double, double,
        const IpvParams*, IpvWcsResult*)>(
        ModuleId::PLATESOLVE, "ipv_solve_from_memory");
    auto fn_get_default_params = dll_loader_.get_function<void (*)(IpvParams*)>(
        ModuleId::PLATESOLVE, "ipv_get_default_params");

    if (!fn_ipv_solve || !fn_get_default_params) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] ipv 函数指针获取失败");
        result.error_msg = "[PLATESOLVE] ipv 函数指针获取失败";
        return false;
    }

    IpvParams params;
    fn_get_default_params(&params);
    // 设置日志目录 (lib/plate_solve/logs)
    std::string log_dir = project_root_dir_ + "/lib/plate_solve/logs";
    std::memset(params.log_dir, 0, sizeof(params.log_dir));
    std::strncpy(params.log_dir, log_dir.c_str(), sizeof(params.log_dir) - 1);

    IpvWcsResult wcs_result;
    std::memset(&wcs_result, 0, sizeof(wcs_result));

    LOG_INFO("orchestrator", "[PLATESOLVE] 调用 ipv_solve_from_memory ...");
    int ret = fn_ipv_solve(ipv_solver_handle_,
                           pixels, width, height,
                           ra0, dec0, focal_length, pixel_size,
                           &params, &wcs_result);

    if (ret != 1 || wcs_result.success != 1) {
        std::string err = wcs_result.error_msg[0] != '\0'
            ? std::string(wcs_result.error_msg)
            : ("ret=" + std::to_string(ret));
        LOG_ERROR("orchestrator", "[PLATESOLVE] 求解失败: " + err);
        result.error_msg = "[PLATESOLVE] 求解失败: " + err;
        return false;
    }

    LOG_INFO("orchestrator", "[PLATESOLVE] 求解成功: rms=" + std::to_string(wcs_result.rms_arcsec)
             + "arcsec, n_pairs=" + std::to_string(wcs_result.n_pairs)
             + ", n_detected=" + std::to_string(wcs_result.n_detected)
             + ", n_catalog=" + std::to_string(wcs_result.n_catalog)
             + ", trans_order=" + std::to_string(wcs_result.trans_order));

    // 4. 写入 WCS 到 header KV 块
    // CTYPE (使用求解结果, 为空则根据 sip_order 推导)
    std::string ctype1(wcs_result.ctype1);
    std::string ctype2(wcs_result.ctype2);
    if (ctype1.empty()) ctype1 = (wcs_result.sip_order > 0) ? "RA---TAN-SIP" : "RA---TAN";
    if (ctype2.empty()) ctype2 = (wcs_result.sip_order > 0) ? "DEC--TAN-SIP" : "DEC--TAN";

    fn_kv_set(frame_, "header", "CTYPE1", ctype1.c_str());
    fn_kv_set(frame_, "header", "CTYPE2", ctype2.c_str());
    fn_kv_set_double(frame_, "header", "CRVAL1", wcs_result.crval[0]);
    fn_kv_set_double(frame_, "header", "CRVAL2", wcs_result.crval[1]);
    fn_kv_set_double(frame_, "header", "CRPIX1", wcs_result.crpix[0]);
    fn_kv_set_double(frame_, "header", "CRPIX2", wcs_result.crpix[1]);
    fn_kv_set_double(frame_, "header", "CD1_1", wcs_result.cd[0]);
    fn_kv_set_double(frame_, "header", "CD1_2", wcs_result.cd[1]);
    fn_kv_set_double(frame_, "header", "CD2_1", wcs_result.cd[2]);
    fn_kv_set_double(frame_, "header", "CD2_2", wcs_result.cd[3]);
    fn_kv_set(frame_, "header", "RADESYS", "ICRS");
    fn_kv_set_double(frame_, "header", "EQUINOX", 2000.0);

    LOG_INFO("orchestrator", "[PLATESOLVE] WCS 已写入: CTYPE1=" + ctype1
             + ", CRVAL=(" + std::to_string(wcs_result.crval[0]) + ", " + std::to_string(wcs_result.crval[1])
             + "), CRPIX=(" + std::to_string(wcs_result.crpix[0]) + ", " + std::to_string(wcs_result.crpix[1]) + ")");

    // 5. 写入 SIP 系数 (前向 A/B + 逆向 AP/BP)
    if (wcs_result.sip_order > 0) {
        int order = wcs_result.sip_order;
        fn_kv_set_double(frame_, "header", "A_ORDER", static_cast<double>(order));
        fn_kv_set_double(frame_, "header", "B_ORDER", static_cast<double>(order));
        char key[16];
        for (int i = 0; i <= order; ++i) {
            for (int j = 0; j <= order - i; ++j) {
                int idx = i * 6 + j;
                if (idx < 36) {
                    std::snprintf(key, sizeof(key), "A_%d_%d", i, j);
                    fn_kv_set_double(frame_, "header", key, wcs_result.sip_a[idx]);
                    std::snprintf(key, sizeof(key), "B_%d_%d", i, j);
                    fn_kv_set_double(frame_, "header", key, wcs_result.sip_b[idx]);
                }
            }
        }

        // 逆向 SIP (AP/BP)
        if (wcs_result.sip_ap_order > 0) {
            int ap_order = wcs_result.sip_ap_order;
            fn_kv_set_double(frame_, "header", "AP_ORDER", static_cast<double>(ap_order));
            fn_kv_set_double(frame_, "header", "BP_ORDER", static_cast<double>(ap_order));
            for (int i = 0; i <= ap_order; ++i) {
                for (int j = 0; j <= ap_order - i; ++j) {
                    int idx = i * 6 + j;
                    if (idx < 36) {
                        std::snprintf(key, sizeof(key), "AP_%d_%d", i, j);
                        fn_kv_set_double(frame_, "header", key, wcs_result.sip_ap[idx]);
                        std::snprintf(key, sizeof(key), "BP_%d_%d", i, j);
                        fn_kv_set_double(frame_, "header", key, wcs_result.sip_bp[idx]);
                    }
                }
            }
        }
        LOG_INFO("orchestrator", "[PLATESOLVE] SIP 已写入: order=" + std::to_string(order)
                 + ", ap_order=" + std::to_string(wcs_result.sip_ap_order));
    } else {
        LOG_INFO("orchestrator", "[PLATESOLVE] 无 SIP 系数 (sip_order=0)");
    }

    // 6. (可选) 写入 star_det 块 (FLOAT32 [N,4]: x, y, flux, mag)
    //    失败不致命, 仅记录警告
    if (fn_add_block && fn_remove_block && star_detector_dll_handle_ != nullptr && sdet_handle_ != 0) {
        HMODULE sdet_h = static_cast<HMODULE>(star_detector_dll_handle_);
        using sdet_detect_ex_fn = int (*)(StarDetectorHandle, const uint16_t*, int, int,
            double**, double**, float**, int**, float**, int**, int*,
            const char**, int, float***);
        using sdet_free_fn = void (*)(double*, double*, float*, int*, float*, int*, float**, int);
        auto fn_sdet_detect_ex = reinterpret_cast<sdet_detect_ex_fn>(
            GetProcAddress(sdet_h, "sdet_detect_ex"));
        auto fn_sdet_free = reinterpret_cast<sdet_free_fn>(
            GetProcAddress(sdet_h, "sdet_free_detect_ex"));

        if (fn_sdet_detect_ex && fn_sdet_free) {
            // 将 float 像素转换为 uint16 (clip 0-65535, 截断)
            int64_t n_pix = static_cast<int64_t>(width) * height;
            uint16_t* pixels_u16 = static_cast<uint16_t*>(std::malloc(n_pix * sizeof(uint16_t)));
            if (pixels_u16 != nullptr) {
                for (int64_t i = 0; i < n_pix; ++i) {
                    float v = pixels[i];
                    if (v < 0.0f) v = 0.0f;
                    if (v > 65535.0f) v = 65535.0f;
                    pixels_u16[i] = static_cast<uint16_t>(v);
                }

                double *out_x = nullptr, *out_y = nullptr;
                float *out_flux = nullptr, *out_mag = nullptr;
                int *out_saturated = nullptr, *out_has_saturated = nullptr;
                int out_count = 0;

                int sdet_ret = fn_sdet_detect_ex(
                    reinterpret_cast<StarDetectorHandle>(sdet_handle_),
                    pixels_u16, width, height,
                    &out_x, &out_y, &out_flux, &out_saturated,
                    &out_mag, &out_has_saturated, &out_count,
                    nullptr, 0, nullptr);

                if (sdet_ret == 0 && out_count > 0 && out_x && out_y && out_flux && out_mag) {
                    // 写入 star_det 块 (FLOAT32 [N,4]: x, y, flux, mag)
                    int64_t n_stars = out_count;
                    float* star_det = static_cast<float*>(std::malloc(n_stars * 4 * sizeof(float)));
                    if (star_det != nullptr) {
                        for (int i = 0; i < out_count; ++i) {
                            star_det[i * 4 + 0] = static_cast<float>(out_x[i]);
                            star_det[i * 4 + 1] = static_cast<float>(out_y[i]);
                            star_det[i * 4 + 2] = out_flux[i];
                            star_det[i * 4 + 3] = out_mag[i];
                        }
                        int dims[2] = {out_count, 4};
                        fn_remove_block(frame_, "star_det");
                        int r = fn_add_block(frame_, "star_det", AIO_BLOCK_FLOAT32,
                                             star_det, n_stars * 4, dims, 2,
                                             "星点检测结果: x,y,flux,mag");
                        if (r == 0) {
                            LOG_INFO("orchestrator", "[PLATESOLVE] star_det 块已写入: "
                                     + std::to_string(out_count) + " 颗星");
                        } else {
                            LOG_WARN("orchestrator", "[PLATESOLVE] star_det 块写入失败 (add_block ret="
                                     + std::to_string(r) + ")");
                        }
                        std::free(star_det);
                    }
                    fn_sdet_free(out_x, out_y, out_flux, out_saturated, out_mag,
                                 out_has_saturated, nullptr, 0);
                } else {
                    LOG_WARN("orchestrator", "[PLATESOLVE] 星点检测未找到星点 (sdet_ret="
                             + std::to_string(sdet_ret) + ", count=" + std::to_string(out_count) + ")");
                }
                std::free(pixels_u16);
            }
        } else {
            LOG_WARN("orchestrator", "[PLATESOLVE] sdet_detect_ex 函数未找到, star_det 块未写入");
        }
    }

    // 7. (可选) 写入 gaia_cat 块 (FLOAT64 [N,3]: ra, dec, mag)
    //    使用求解后的 WCS 中心计算 FOV 半径, 锥形查询 Gaia 星表
    //    失败不致命, 仅记录警告
    if (fn_add_block && fn_remove_block && gaia_client_dll_handle_ != nullptr && gaia_client_handle_ != 0) {
        HMODULE gaia_h = static_cast<HMODULE>(gaia_client_dll_handle_);
        using gaia_cone_fn = int (*)(GaiaClient*, double, double, double, double,
            double**, double**, float**, int*);
        auto fn_gaia_cone = reinterpret_cast<gaia_cone_fn>(
            GetProcAddress(gaia_h, "gaia_client_cone_search_for_solver"));

        if (fn_gaia_cone) {
            // 计算 FOV 半径: sqrt(|det(CD)|) * sqrt(w^2 + h^2) / 2 * 1.2 (20% 余量)
            double cd_det = std::abs(wcs_result.cd[0] * wcs_result.cd[3] -
                                     wcs_result.cd[1] * wcs_result.cd[2]);
            double pixel_scale_deg = (cd_det > 0) ? std::sqrt(cd_det) : 0.0;
            double fov_radius_deg = pixel_scale_deg * std::sqrt(
                static_cast<double>(width) * width + static_cast<double>(height) * height) / 2.0 * 1.2;

            if (fov_radius_deg > 0.0 && fov_radius_deg < 30.0) {
                double *out_ra = nullptr, *out_dec = nullptr;
                float *out_mag = nullptr;
                int out_count = 0;
                int gaia_ret = fn_gaia_cone(
                    reinterpret_cast<GaiaClient*>(gaia_client_handle_),
                    wcs_result.crval[0], wcs_result.crval[1],
                    fov_radius_deg, 18.0,  // mag_high=18.0
                    &out_ra, &out_dec, &out_mag, &out_count);

                if (gaia_ret == 0 && out_count > 0 && out_ra && out_dec && out_mag) {
                    int64_t n_gaia = out_count;
                    double* gaia_cat = static_cast<double*>(std::malloc(n_gaia * 3 * sizeof(double)));
                    if (gaia_cat != nullptr) {
                        for (int i = 0; i < out_count; ++i) {
                            gaia_cat[i * 3 + 0] = out_ra[i];
                            gaia_cat[i * 3 + 1] = out_dec[i];
                            gaia_cat[i * 3 + 2] = static_cast<double>(out_mag[i]);
                        }
                        int dims[2] = {out_count, 3};
                        fn_remove_block(frame_, "gaia_cat");
                        int r = fn_add_block(frame_, "gaia_cat", AIO_BLOCK_FLOAT64,
                                             gaia_cat, n_gaia * 3, dims, 2,
                                             "Gaia 星表: ra,dec,mag");
                        if (r == 0) {
                            LOG_INFO("orchestrator", "[PLATESOLVE] gaia_cat 块已写入: "
                                     + std::to_string(out_count) + " 颗星, FOV半径="
                                     + std::to_string(fov_radius_deg) + "deg");
                        } else {
                            LOG_WARN("orchestrator", "[PLATESOLVE] gaia_cat 块写入失败 (add_block ret="
                                     + std::to_string(r) + ")");
                        }
                        std::free(gaia_cat);
                    }
                    // 释放 C 端内存 (gaia_client 使用 malloc 分配)
                    std::free(out_ra);
                    std::free(out_dec);
                    std::free(out_mag);
                } else {
                    LOG_WARN("orchestrator", "[PLATESOLVE] Gaia 锥形查询返回空 (ret="
                             + std::to_string(gaia_ret) + ", count=" + std::to_string(out_count) + ")");
                }
            } else {
                LOG_WARN("orchestrator", "[PLATESOLVE] FOV 半径异常 ("
                         + std::to_string(fov_radius_deg) + "deg), gaia_cat 块未写入");
            }
        } else {
            LOG_WARN("orchestrator", "[PLATESOLVE] gaia_client_cone_search_for_solver 函数未找到, gaia_cat 块未写入");
        }
    }

    LOG_INFO("orchestrator", "[PLATESOLVE] 完成");
    return true;
}

bool Orchestrator::run_stage_psf(TaskResult& result) {
    LOG_INFO("orchestrator", "[PSF] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PSF)) {
        LOG_WARN("orchestrator", "[PSF] PSF DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[PSF] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 获取 AIO 函数指针
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");
    auto fn_add_block = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        const void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block");

    if (!fn_get_block || !fn_remove_block || !fn_add_block) {
        LOG_ERROR("orchestrator", "[PSF] AIO 函数指针获取失败");
        result.error_msg = "[PSF] AIO 函数指针获取失败";
        return false;
    }

    // 1. 读取 data 块 (FLOAT32 [H,W]) → 转为 UINT16 (dpsf_fit_batch 要求 uint16)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] data 块不存在");
        result.error_msg = "[PSF] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const float* pixels = static_cast<const float*>(data_block->data);
    int64_t n_pix = static_cast<int64_t>(width) * height;
    LOG_INFO("orchestrator", "[PSF] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    uint16_t* pixels_u16 = static_cast<uint16_t*>(std::malloc(n_pix * sizeof(uint16_t)));
    if (pixels_u16 == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] 分配 uint16 缓冲失败");
        result.error_msg = "[PSF] 分配 uint16 缓冲失败";
        return false;
    }
    for (int64_t i = 0; i < n_pix; ++i) {
        float v = pixels[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 65535.0f) v = 65535.0f;
        pixels_u16[i] = static_cast<uint16_t>(v);
    }

    // 2. 读取 star_det 块 (FLOAT32 [N,4]: x, y, flux, mag)
    const AioBlock* star_det_block = fn_get_block(frame_, "star_det");
    if (star_det_block == nullptr || star_det_block->dims[0] <= 0) {
        LOG_WARN("orchestrator", "[PSF] star_det 块不存在或为空, 跳过 PSF 拟合");
        std::free(pixels_u16);
        return true;
    }
    int n_stars = star_det_block->dims[0];
    const float* star_det_data = static_cast<const float*>(star_det_block->data);
    LOG_INFO("orchestrator", "[PSF] star_det: " + std::to_string(n_stars) + " 颗星");

    // 提取 cx/cy 数组 (double, dpsf_fit_batch 要求)
    std::vector<double> cx_arr(n_stars), cy_arr(n_stars);
    for (int i = 0; i < n_stars; ++i) {
        cx_arr[i] = static_cast<double>(star_det_data[i * 4 + 0]);
        cy_arr[i] = static_cast<double>(star_det_data[i * 4 + 1]);
    }

    // 3. 调用 dpsf_fit_batch
    auto fn_fit_batch = dll_loader_.get_function<int (*)(
        const uint16_t*, int, int,
        const double*, const double*, int,
        const DPSFFitParams*, DPSFFitResult**)>(
        ModuleId::PSF, "dpsf_fit_batch");
    auto fn_free_results = dll_loader_.get_function<void (*)(DPSFFitResult*)>(
        ModuleId::PSF, "dpsf_free_results");

    if (!fn_fit_batch || !fn_free_results) {
        LOG_ERROR("orchestrator", "[PSF] dpsf_fit_batch/dpsf_free_results 函数未找到");
        result.error_msg = "[PSF] DPSF 函数指针获取失败";
        std::free(pixels_u16);
        return false;
    }

    DPSFFitParams params;
    params.fitRadius = 8;
    params.maxIter = 100;
    params.tolerance = 1e-6;

    DPSFFitResult* results = nullptr;
    LOG_INFO("orchestrator", "[PSF] 调用 dpsf_fit_batch (fitRadius=8, maxIter=100) ...");
    int ret = fn_fit_batch(pixels_u16, width, height,
                           cx_arr.data(), cy_arr.data(), n_stars,
                           &params, &results);
    std::free(pixels_u16);  // 图像不再需要

    if (ret != 0 || results == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] dpsf_fit_batch 失败: ret=" + std::to_string(ret));
        result.error_msg = "[PSF] dpsf_fit_batch 失败: ret=" + std::to_string(ret);
        if (results) fn_free_results(results);
        return false;
    }

    // 统计成功数
    int n_ok = 0;
    for (int i = 0; i < n_stars; ++i) {
        if (results[i].status == DPSF_FIT_OK) ++n_ok;
    }
    LOG_INFO("orchestrator", "[PSF] 拟合完成: " + std::to_string(n_ok) + "/" + std::to_string(n_stars)
             + " 成功 (" + std::to_string(static_cast<int>(100.0 * n_ok / std::max(1, n_stars))) + "%)");

    // 4. 写入 psf 块 (FLOAT64 [N,9])
    // 列含义 (与 snr_estimator.h 一致):
    //   [0]=status, [1]=B, [2]=flux, [3]=cx, [4]=cy,
    //   [5]=fwhm(平均), [6]=A, [7]=mad, [8]=eccentricity
    int64_t n_elements = static_cast<int64_t>(n_stars) * 9;
    double* psf_data = static_cast<double*>(std::malloc(n_elements * sizeof(double)));
    if (psf_data == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] 分配 psf 块失败");
        result.error_msg = "[PSF] 分配 psf 块失败";
        fn_free_results(results);
        return false;
    }
    for (int i = 0; i < n_stars; ++i) {
        const DPSFFitResult& r = results[i];
        double* row = psf_data + i * 9;
        row[0] = static_cast<double>(r.status);
        row[1] = r.B;
        row[2] = r.flux;
        row[3] = r.cx;
        row[4] = r.cy;
        row[5] = (r.fwhm_x + r.fwhm_y) * 0.5;  // 平均 FWHM
        row[6] = r.A;
        row[7] = r.mad;
        row[8] = r.eccentricity;
    }
    fn_free_results(results);

    int dims[2] = {n_stars, 9};
    fn_remove_block(frame_, "psf");
    int r = fn_add_block(frame_, "psf", AIO_BLOCK_FLOAT64,
                         psf_data, n_elements, dims, 2,
                         "PSF 拟合结果: status,B,flux,cx,cy,fwhm,A,mad,eccentricity");
    if (r != 0) {
        LOG_ERROR("orchestrator", "[PSF] 写入 psf 块失败: ret=" + std::to_string(r));
        result.error_msg = "[PSF] 写入 psf 块失败";
        std::free(psf_data);
        return false;
    }
    std::free(psf_data);  // add_block 已拷贝

    LOG_INFO("orchestrator", "[PSF] 完成: " + std::to_string(n_stars) + " 颗星, "
             + std::to_string(n_ok) + " 成功");
    return true;
}

bool Orchestrator::run_stage_photometric(TaskResult& result) {
    LOG_INFO("orchestrator", "[PHOTOMETRIC] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PHOTOMETRIC)) {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] PHOTOMETRIC DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 确保 PLATESOLVE 环境已初始化 (复用 gaia_client_handle_)
    if (!platesolve_env_ready_) {
        std::string err;
        if (!init_platesolve_env(err)) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] init_platesolve_env 失败: " + err);
            result.error_msg = "[PHOTOMETRIC] " + err;
            return false;
        }
    }

    // 获取 AIO 函数指针
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");
    auto fn_add_block_move = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block_move");
    auto fn_kv_get = dll_loader_.get_function<const char* (*)(
        const PipelineFrame*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_get");
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");
    auto fn_kv_set = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_set");
    auto fn_kv_set_double = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_set_double");

    if (!fn_get_block || !fn_remove_block || !fn_add_block_move ||
        !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] AIO 函数指针获取失败");
        result.error_msg = "[PHOTOMETRIC] AIO 函数指针获取失败";
        return false;
    }

    // 1. 读取 data 块 (FLOAT32 [H,W])
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] data 块不存在");
        result.error_msg = "[PHOTOMETRIC] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const float* pixels = static_cast<const float*>(data_block->data);
    LOG_INFO("orchestrator", "[PHOTOMETRIC] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 2. 读取 psf 块 (FLOAT64 [N,9])
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr || psf_block->dims[0] <= 0) {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] psf 块不存在或为空, 跳过测光校准");
        return true;
    }
    int n_psf = psf_block->dims[0];
    const double* psf_data = static_cast<const double*>(psf_block->data);
    LOG_INFO("orchestrator", "[PHOTOMETRIC] psf: " + std::to_string(n_psf) + " 颗星");

    // 提取 psf_cx/cy/flux/status 数组
    // PSF 列: [0]=status, [1]=B, [2]=flux, [3]=cx, [4]=cy, [5]=fwhm, [6]=A, [7]=mad, [8]=eccentricity
    std::vector<double> psf_cx(n_psf), psf_cy(n_psf), psf_flux(n_psf);
    std::vector<int> psf_status(n_psf);
    for (int i = 0; i < n_psf; ++i) {
        const double* row = psf_data + i * 9;
        psf_status[i] = static_cast<int>(row[0]);
        psf_flux[i] = row[2];
        psf_cx[i] = row[3];
        psf_cy[i] = row[4];
    }

    // 3. 从 header KV 读取 FILTER + WCS + SIP
    std::string filter_str;
    const char* filter_cstr = fn_kv_get(frame_, "header", "FILTER");
    if (filter_cstr) filter_str = filter_cstr;
    std::string filter_name = map_filter_name(filter_str);
    LOG_INFO("orchestrator", "[PHOTOMETRIC] FILTER='" + filter_str + "' -> '" + filter_name + "'");

    // 加载滤光片曲线
    std::string filters_json = project_root_dir_ + "/lib/photometric_calib/data/response_curves/filters.json";
    std::vector<double> filter_wl, filter_trans;
    if (!load_filter_curve(filters_json, filter_name, filter_wl, filter_trans)) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 加载滤光片曲线失败");
        result.error_msg = "[PHOTOMETRIC] 加载滤光片曲线失败";
        return false;
    }
    LOG_INFO("orchestrator", "[PHOTOMETRIC] 滤光片: " + std::to_string(filter_wl.size()) + " 点");

    // 构建 spectrum_wl
    std::vector<double> spectrum_wl;
    if (!build_spectrum_wl(gaia_client_dll_handle_, gaia_client_handle_, spectrum_wl)) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 构建 spectrum_wl 失败");
        result.error_msg = "[PHOTOMETRIC] 构建 spectrum_wl 失败";
        return false;
    }
    LOG_INFO("orchestrator", "[PHOTOMETRIC] spectrum_wl: " + std::to_string(spectrum_wl.size()) + " 点");

    // 读取 WCS + SIP
    WcsHeaderParams wcs;
    auto fn_kv_get_l = [&](const char* block, const char* key) -> const char* {
        return fn_kv_get(frame_, block, key);
    };
    auto fn_kv_get_double_l = [&](const char* block, const char* key, double def) -> double {
        return fn_kv_get_double(frame_, block, key, def);
    };
    if (!read_wcs_from_header(fn_kv_get_l, fn_kv_get_double_l, wcs)) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] WCS 缺失 (需 PLATESOLVE 先执行)");
        result.error_msg = "[PHOTOMETRIC] WCS 缺失";
        return false;
    }

    // 计算 FOV 半径 (度)
    double cd_det = std::abs(wcs.cd11 * wcs.cd22 - wcs.cd12 * wcs.cd21);
    double pixel_scale_deg = (cd_det > 0) ? std::sqrt(cd_det) : 0.0;
    double fov_radius_deg = pixel_scale_deg * std::sqrt(
        static_cast<double>(width) * width + static_cast<double>(height) * height) / 2.0 * 1.2;
    if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] FOV 半径异常 (" + std::to_string(fov_radius_deg) + "deg), 钳位");
        fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
    }
    LOG_INFO("orchestrator", "[PHOTOMETRIC] FOV 半径: " + std::to_string(fov_radius_deg) + "deg");

    // 4. 调用 pc_calibrate_simple_with_gaia (DLL 内部: 锥形搜索+光谱积分+星匹配+scale 校正)
    auto fn_pc_calib = dll_loader_.get_function<int (*)(
        void*, double, double, double, double, double,
        const double*, const double*, int,
        const double*, int,
        const float*, int, int,
        const double*, const double*, const double*, const int*, int,
        double, double, double, double, double, double, double, double,
        int, const double*, const double*, const double*, const double*,
        float*, int*, double*, double*)>(
        ModuleId::PHOTOMETRIC, "pc_calibrate_simple_with_gaia");

    if (!fn_pc_calib) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] pc_calibrate_simple_with_gaia 函数未找到");
        result.error_msg = "[PHOTOMETRIC] pc_calibrate_simple_with_gaia 函数未找到";
        return false;
    }

    int64_t n_pix = static_cast<int64_t>(width) * height;
    float* out_pixels = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    if (out_pixels == nullptr) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 分配输出缓冲失败");
        result.error_msg = "[PHOTOMETRIC] 分配输出缓冲失败";
        return false;
    }

    int out_n_matched = 0;
    double out_scale = 0.0, out_sigma = 0.0;

    // SIP 指针 (无 SIP 时传 nullptr)
    const double* sip_a_ptr = (wcs.sip_order > 0) ? wcs.sip_a : nullptr;
    const double* sip_b_ptr = (wcs.sip_order > 0) ? wcs.sip_b : nullptr;
    const double* sip_ap_ptr = (wcs.has_ap) ? wcs.sip_ap : nullptr;
    const double* sip_bp_ptr = (wcs.has_ap) ? wcs.sip_bp : nullptr;

    LOG_INFO("orchestrator", "[PHOTOMETRIC] 调用 pc_calibrate_simple_with_gaia ...");
    int ret = fn_pc_calib(
        reinterpret_cast<void*>(reinterpret_cast<GaiaClient*>(gaia_client_handle_)),
        wcs.crval1, wcs.crval2, fov_radius_deg,
        6.0, 16.0,  // mag_min, mag_max (DLL 内部会自适应迭代)
        filter_wl.data(), filter_trans.data(), (int)filter_wl.size(),
        spectrum_wl.data(), (int)spectrum_wl.size(),
        pixels, width, height,
        psf_cx.data(), psf_cy.data(), psf_flux.data(), psf_status.data(), n_psf,
        wcs.crval1, wcs.crval2, wcs.crpix1, wcs.crpix2,
        wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
        wcs.sip_order, sip_a_ptr, sip_b_ptr, sip_ap_ptr, sip_bp_ptr,
        out_pixels, &out_n_matched, &out_scale, &out_sigma);

    if (ret != 0) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] pc_calibrate_simple_with_gaia 失败: ret=" + std::to_string(ret));
        result.error_msg = "[PHOTOMETRIC] pc_calibrate_simple_with_gaia 失败: ret=" + std::to_string(ret);
        std::free(out_pixels);
        return false;
    }

    LOG_INFO("orchestrator", "[PHOTOMETRIC] 完成: n_matched=" + std::to_string(out_n_matched)
             + ", scale=" + std::to_string(out_scale)
             + ", sigma_residual=" + std::to_string(out_sigma));

    // 5. 更新 data 块 (替换为标定后像素, 转移所有权)
    fn_remove_block(frame_, "data");
    int dims[2] = {height, width};
    int r = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT32,
                              out_pixels, n_pix, dims, 2,
                              "测光标定后像素 (I_cal = I * scale)");
    if (r != 0) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 更新 data 块失败: ret=" + std::to_string(r));
        result.error_msg = "[PHOTOMETRIC] 更新 data 块失败";
        std::free(out_pixels);
        return false;
    }
    // out_pixels 所有权已转移给 frame_

    // 6. 写入 photo_stats KV 块 (N_MATCHED, SCALE_FACTOR, SIGMA_RESIDUAL 供 SNR 使用)
    fn_kv_set(frame_, "photo_stats", "STATUS", "OK");
    fn_kv_set_double(frame_, "photo_stats", "N_MATCHED", static_cast<double>(out_n_matched));
    fn_kv_set_double(frame_, "photo_stats", "SCALE_FACTOR", out_scale);
    fn_kv_set_double(frame_, "photo_stats", "SIGMA_RESIDUAL", out_sigma);

    LOG_INFO("orchestrator", "[PHOTOMETRIC] photo_stats 已写入");
    return true;
}

bool Orchestrator::run_stage_drizzle(TaskResult& result) {
    LOG_INFO("orchestrator", "[DRIZZLE] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::DRIZZLE)) {
        LOG_WARN("orchestrator", "[DRIZZLE] DRIZZLE DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[DRIZZLE] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 获取函数指针
    auto fn_drizzle = dll_loader_.get_function<int (*)(
        PipelineFrame*, int, int, double,
        const char*, HpDrizzleResult*)>(
        ModuleId::DRIZZLE, "hp_drizzle_run");

    if (!fn_drizzle) {
        LOG_ERROR("orchestrator", "[DRIZZLE] hp_drizzle_run 函数未找到");
        result.error_msg = "[DRIZZLE] hp_drizzle_run 函数未找到";
        return false;
    }

    // 调用 hp_drizzle_run
    // nside=32768 (默认), nested=1 (NESTED), pixfrac=1.0 (避免缝隙)
    // output_path = current_output_path_ (.hiss 路径)
    HpDrizzleResult driz_result;
    std::memset(&driz_result, 0, sizeof(HpDrizzleResult));
    LOG_INFO("orchestrator", "[DRIZZLE] 输出: " + current_output_path_);

    int ret = fn_drizzle(frame_, 32768, 1, 1.0,
                         current_output_path_.c_str(), &driz_result);
    if (ret != 0) {
        std::string err = driz_result.error_msg[0] != '\0'
            ? std::string(driz_result.error_msg)
            : std::to_string(ret);
        LOG_ERROR("orchestrator", "[DRIZZLE] hp_drizzle_run 失败: " + err);
        result.error_msg = "[DRIZZLE] hp_drizzle_run 失败: " + err;
        return false;
    }

    LOG_INFO("orchestrator", "[DRIZZLE] 完成: n_healpix=" + std::to_string(driz_result.n_healpix_pixels)
             + " n_source=" + std::to_string(driz_result.n_source_pixels)
             + " 耗时=" + std::to_string(driz_result.elapsed_sec) + "s");
    return true;
}

// ============================================================================
// spec §2.3.2 两段流水线新增 stage handler (骨架)
// ============================================================================

// stage 0: READ_FITS - 读取 FITS 文件到 PipelineFrame (aio_read_fits)
// 实现: 调用 aio_read_fits -> 获取 pixel/metadata/keywords -> 填充 frame_ 的 data/header 块
bool Orchestrator::run_stage_read_fits(TaskResult& result) {
    LOG_INFO("orchestrator", "[READ_FITS] 开始: " + current_fits_path_);

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::AIO)) {
        LOG_ERROR("orchestrator", "[READ_FITS] AIO DLL 未加载");
        result.error_msg = "[READ_FITS] AIO DLL 未加载";
        return false;
    }

    // 获取 AIO 函数指针
    auto fn_read_fits = dll_loader_.get_function<AIOImageData* (*)(const char*)>(
        ModuleId::AIO, "aio_read_fits");
    auto fn_get_pixels = dll_loader_.get_function<float* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data");
    auto fn_get_width = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_width");
    auto fn_get_height = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_height");
    auto fn_get_metadata = dll_loader_.get_function<AIOImageMetadata (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_metadata");
    auto fn_get_kw_count = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_keyword_count");
    auto fn_get_kw = dll_loader_.get_function<AIOFITSKeyword (*)(const AIOImageData*, int)>(
        ModuleId::AIO, "aio_get_keyword");
    auto fn_free = dll_loader_.get_function<void (*)(AIOImageData*)>(
        ModuleId::AIO, "aio_free_image_data");
    auto fn_add_block = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        const void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block");
    auto fn_kv_set = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_set");
    auto fn_kv_set_double = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_set_double");

    if (!fn_read_fits || !fn_get_pixels || !fn_get_width || !fn_get_height ||
        !fn_get_metadata || !fn_free || !fn_add_block || !fn_kv_set) {
        LOG_ERROR("orchestrator", "[READ_FITS] AIO 函数指针获取失败");
        result.error_msg = "[READ_FITS] AIO 函数指针获取失败";
        return false;
    }

    // 读取 FITS 文件
    AIOImageData* image = fn_read_fits(current_fits_path_.c_str());
    if (image == nullptr) {
        LOG_ERROR("orchestrator", "[READ_FITS] aio_read_fits 返回 nullptr: " + current_fits_path_);
        result.error_msg = "[READ_FITS] 读取 FITS 失败";
        return false;
    }

    int width = fn_get_width(image);
    int height = fn_get_height(image);
    float* pixels = fn_get_pixels(image);
    LOG_INFO("orchestrator", "[READ_FITS] 图像尺寸: " + std::to_string(width) + "x" + std::to_string(height));

    if (width <= 0 || height <= 0 || pixels == nullptr) {
        LOG_ERROR("orchestrator", "[READ_FITS] 像素数据无效");
        result.error_msg = "[READ_FITS] 像素数据无效";
        fn_free(image);
        return false;
    }

    // 添加 data 块 (FLOAT32 [H,W], 拷贝)
    int dims[2] = {height, width};
    int ret = fn_add_block(frame_, "data", AIO_BLOCK_FLOAT32,
                           pixels, static_cast<int64_t>(width) * height,
                           dims, 2, "校准前 Light 像素");
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[READ_FITS] 添加 data 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[READ_FITS] 添加 data 块失败";
        fn_free(image);
        return false;
    }

    // 填充 header KV 块 (FITS keywords + WCS + 观测元数据)
    if (fn_get_kw_count && fn_get_kw) {
        int n_kw = fn_get_kw_count(image);
        LOG_INFO("orchestrator", "[READ_FITS] FITS 关键字数: " + std::to_string(n_kw));
        for (int i = 0; i < n_kw; ++i) {
            AIOFITSKeyword kw = fn_get_kw(image, i);
            // 跳过空 key
            if (kw.name[0] == '\0') continue;
            fn_kv_set(frame_, "header", kw.name, kw.value);
        }
    }

    // 写入 WCS + 观测元数据到 header KV (供后续 stage 使用)
    if (fn_get_metadata) {
        AIOImageMetadata meta = fn_get_metadata(image);
        // WCS
        if (meta.wcs.has_wcs) {
            fn_kv_set_double(frame_, "header", "CRPIX1", meta.wcs.crpix1);
            fn_kv_set_double(frame_, "header", "CRPIX2", meta.wcs.crpix2);
            fn_kv_set_double(frame_, "header", "CRVAL1", meta.wcs.crval1);
            fn_kv_set_double(frame_, "header", "CRVAL2", meta.wcs.crval2);
            fn_kv_set(frame_, "header", "CTYPE1", meta.wcs.ctype1);
            fn_kv_set(frame_, "header", "CTYPE2", meta.wcs.ctype2);
            fn_kv_set_double(frame_, "header", "CD1_1", meta.wcs.cd1_1);
            fn_kv_set_double(frame_, "header", "CD1_2", meta.wcs.cd1_2);
            fn_kv_set_double(frame_, "header", "CD2_1", meta.wcs.cd2_1);
            fn_kv_set_double(frame_, "header", "CD2_2", meta.wcs.cd2_2);
            if (meta.wcs.has_cdelt1) fn_kv_set_double(frame_, "header", "CDELT1", meta.wcs.cdelt1);
            if (meta.wcs.has_cdelt2) fn_kv_set_double(frame_, "header", "CDELT2", meta.wcs.cdelt2);
            if (meta.wcs.has_equinox) fn_kv_set_double(frame_, "header", "EQUINOX", meta.wcs.equinox);
            if (meta.wcs.radesys[0] != '\0') fn_kv_set(frame_, "header", "RADESYS", meta.wcs.radesys);
        }
        // 观测元数据
        if (meta.observation.object_name[0] != '\0')
            fn_kv_set(frame_, "header", "OBJECT", meta.observation.object_name);
        if (meta.observation.observat[0] != '\0')
            fn_kv_set(frame_, "header", "OBSERVAT", meta.observation.observat);
        if (meta.observation.has_focallen)
            fn_kv_set_double(frame_, "header", "FOCALLEN", meta.observation.focallen);
        if (meta.observation.has_xpixsz)
            fn_kv_set_double(frame_, "header", "XPIXSZ", meta.observation.xpixsz);
        if (meta.observation.has_aperture)
            fn_kv_set_double(frame_, "header", "APERTURE", meta.observation.aperture);
        // 校准元数据
        if (meta.calibration.exptime > 0)
            fn_kv_set_double(frame_, "header", "EXPTIME", meta.calibration.exptime);
        if (meta.calibration.filter_name[0] != '\0')
            fn_kv_set(frame_, "header", "FILTER", meta.calibration.filter_name);
        if (meta.calibration.gain > 0)
            fn_kv_set_double(frame_, "header", "GAIN", meta.calibration.gain);
        if (meta.calibration.has_ccd_temp)
            fn_kv_set_double(frame_, "header", "CCD-TEMP", meta.calibration.ccd_temp);
        if (meta.calibration.frame_type[0] != '\0')
            fn_kv_set(frame_, "header", "IMAGETYP", meta.calibration.frame_type);
    }

    fn_free(image);
    LOG_INFO("orchestrator", "[READ_FITS] 完成");
    return true;
}

// stage 5: GRADIENT_2D - step4 C++化 (gradient_2d.dll)
bool Orchestrator::run_stage_gradient_2d(TaskResult& result) {
    LOG_INFO("orchestrator", "[GRADIENT_2D] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::GRADIENT_2D)) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] GRADIENT_2D DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 确保 PLATESOLVE 环境已初始化 (复用 gaia_client_handle_)
    if (!platesolve_env_ready_) {
        std::string err;
        if (!init_platesolve_env(err)) {
            LOG_ERROR("orchestrator", "[GRADIENT_2D] init_platesolve_env 失败: " + err);
            result.error_msg = "[GRADIENT_2D] " + err;
            return false;
        }
    }

    // 获取 AIO 函数指针
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");
    auto fn_add_block_move = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block_move");
    auto fn_kv_get = dll_loader_.get_function<const char* (*)(
        const PipelineFrame*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_get");
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");
    auto fn_kv_set_double = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_set_double");

    if (!fn_get_block || !fn_remove_block || !fn_add_block_move || !fn_kv_get || !fn_kv_get_double) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] AIO 函数指针获取失败");
        result.error_msg = "[GRADIENT_2D] AIO 函数指针获取失败";
        return false;
    }

    // 1. 读取 data 块 (FLOAT32 [H,W])
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] data 块不存在");
        result.error_msg = "[GRADIENT_2D] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const float* pixels = static_cast<const float*>(data_block->data);
    LOG_INFO("orchestrator", "[GRADIENT_2D] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 2. 读取 psf 块 (FLOAT64 [N,9])
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr || psf_block->dims[0] <= 0) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] psf 块不存在或为空, 跳过梯度校正");
        return true;
    }
    int n_psf = psf_block->dims[0];
    const double* psf_data = static_cast<const double*>(psf_block->data);

    // 提取 psf_cx/cy/flux/status 数组
    std::vector<double> psf_cx(n_psf), psf_cy(n_psf), psf_flux(n_psf);
    std::vector<int> psf_status(n_psf);
    for (int i = 0; i < n_psf; ++i) {
        const double* row = psf_data + i * 9;
        psf_status[i] = static_cast<int>(row[0]);
        psf_flux[i] = row[2];
        psf_cx[i] = row[3];
        psf_cy[i] = row[4];
    }

    // 3. 从 header KV 读取 FILTER + WCS + SIP
    std::string filter_str;
    const char* filter_cstr = fn_kv_get(frame_, "header", "FILTER");
    if (filter_cstr) filter_str = filter_cstr;
    std::string filter_name = map_filter_name(filter_str);

    std::string filters_json = project_root_dir_ + "/lib/photometric_calib/data/response_curves/filters.json";
    std::vector<double> filter_wl, filter_trans;
    if (!load_filter_curve(filters_json, filter_name, filter_wl, filter_trans)) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] 加载滤光片曲线失败");
        result.error_msg = "[GRADIENT_2D] 加载滤光片曲线失败";
        return false;
    }

    std::vector<double> spectrum_wl;
    if (!build_spectrum_wl(gaia_client_dll_handle_, gaia_client_handle_, spectrum_wl)) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] 构建 spectrum_wl 失败");
        result.error_msg = "[GRADIENT_2D] 构建 spectrum_wl 失败";
        return false;
    }

    WcsHeaderParams wcs;
    auto fn_kv_get_l = [&](const char* block, const char* key) -> const char* {
        return fn_kv_get(frame_, block, key);
    };
    auto fn_kv_get_double_l = [&](const char* block, const char* key, double def) -> double {
        return fn_kv_get_double(frame_, block, key, def);
    };
    if (!read_wcs_from_header(fn_kv_get_l, fn_kv_get_double_l, wcs)) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] WCS 缺失");
        result.error_msg = "[GRADIENT_2D] WCS 缺失";
        return false;
    }

    // 计算 FOV 半径
    double cd_det = std::abs(wcs.cd11 * wcs.cd22 - wcs.cd12 * wcs.cd21);
    double pixel_scale_deg = (cd_det > 0) ? std::sqrt(cd_det) : 0.0;
    double fov_radius_deg = pixel_scale_deg * std::sqrt(
        static_cast<double>(width) * width + static_cast<double>(height) * height) / 2.0 * 1.2;
    if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
        fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
    }
    LOG_INFO("orchestrator", "[GRADIENT_2D] FOV 半径: " + std::to_string(fov_radius_deg) + "deg");

    // 4. 预计算 Gaia F_syn 数组 (gradient_2d_calibrate 需要 gaia_fsyn)
    std::vector<double> gaia_ra, gaia_dec, gaia_mag, gaia_fsyn;
    if (!compute_gaia_fsyn_for_gradient(
            gaia_client_dll_handle_, gaia_client_handle_,
            wcs.crval1, wcs.crval2, fov_radius_deg,
            6.0, 16.0, filter_wl, filter_trans, spectrum_wl,
            gaia_ra, gaia_dec, gaia_mag, gaia_fsyn)) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] 无法获取 Gaia F_syn 数据, 跳过梯度校正");
        return true;
    }
    int n_gaia = (int)gaia_ra.size();
    LOG_INFO("orchestrator", "[GRADIENT_2D] Gaia 星: " + std::to_string(n_gaia) + " 颗");

    // 5. 调用 gradient_2d_calibrate
    auto fn_gradient = dll_loader_.get_function<int (*)(
        const float*, int, int,
        const double*, const double*, const double*, const double*, int,
        const double*, const double*, const double*, const int*, int,
        double, double, double, double, double, double, double, double,
        int, const double*, const double*, const double*, const double*,
        double, double, int,
        float*, Gradient2DResult*)>(
        ModuleId::GRADIENT_2D, "gradient_2d_calibrate");

    if (!fn_gradient) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] gradient_2d_calibrate 函数未找到");
        result.error_msg = "[GRADIENT_2D] gradient_2d_calibrate 函数未找到";
        return false;
    }

    int64_t n_pix = static_cast<int64_t>(width) * height;
    float* out_pixels = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    if (out_pixels == nullptr) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] 分配输出缓冲失败");
        result.error_msg = "[GRADIENT_2D] 分配输出缓冲失败";
        return false;
    }

    const double* sip_a_ptr = (wcs.sip_order > 0) ? wcs.sip_a : nullptr;
    const double* sip_b_ptr = (wcs.sip_order > 0) ? wcs.sip_b : nullptr;
    const double* sip_ap_ptr = (wcs.has_ap) ? wcs.sip_ap : nullptr;
    const double* sip_bp_ptr = (wcs.has_ap) ? wcs.sip_bp : nullptr;

    Gradient2DResult g2d_result;
    std::memset(&g2d_result, 0, sizeof(g2d_result));

    LOG_INFO("orchestrator", "[GRADIENT_2D] 调用 gradient_2d_calibrate ...");
    int ret = fn_gradient(
        pixels, width, height,
        gaia_ra.data(), gaia_dec.data(), gaia_mag.data(), gaia_fsyn.data(), n_gaia,
        psf_cx.data(), psf_cy.data(), psf_flux.data(), psf_status.data(), n_psf,
        wcs.crval1, wcs.crval2, wcs.crpix1, wcs.crpix2,
        wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
        wcs.sip_order, sip_a_ptr, sip_b_ptr, sip_ap_ptr, sip_bp_ptr,
        3.0, 3.0, 3,  // match_radius_px, outlier_sigma, max_order
        out_pixels, &g2d_result);

    // -1=参数无效, -3=数值异常: 失败; -2=匹配星数<6: 退化恒等校正 (out_pixels 仍有效)
    if (ret == -1 || ret == -3) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] gradient_2d_calibrate 失败: ret=" + std::to_string(ret));
        result.error_msg = "[GRADIENT_2D] 失败: ret=" + std::to_string(ret);
        std::free(out_pixels);
        return false;
    }
    if (ret == -2) {
        LOG_WARN("orchestrator", "[GRADIENT_2D] 退化: 匹配星数 < 6, 恒等校正");
    }

    LOG_INFO("orchestrator", "[GRADIENT_2D] 完成: n_matched=" + std::to_string(g2d_result.n_matched)
             + ", scale=" + std::to_string(g2d_result.scale_factor)
             + ", rms=" + std::to_string(g2d_result.rms_residual)
             + ", order=" + std::to_string(g2d_result.mult_order)
             + ", R²=" + std::to_string(g2d_result.mult_r_squared)
             + ", sigma=" + std::to_string(g2d_result.sigma_residual));

    // 6. 更新 data 块 (替换为校正后像素, 转移所有权)
    fn_remove_block(frame_, "data");
    int dims[2] = {height, width};
    int r = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT32,
                              out_pixels, n_pix, dims, 2,
                              "2D 梯度校正后像素");
    if (r != 0) {
        LOG_ERROR("orchestrator", "[GRADIENT_2D] 更新 data 块失败: ret=" + std::to_string(r));
        result.error_msg = "[GRADIENT_2D] 更新 data 块失败";
        std::free(out_pixels);
        return false;
    }
    // out_pixels 所有权已转移给 frame_

    // 追加梯度校正信息到 photo_stats KV
    if (fn_kv_set_double) {
        fn_kv_set_double(frame_, "photo_stats", "G2D_N_MATCHED", static_cast<double>(g2d_result.n_matched));
        fn_kv_set_double(frame_, "photo_stats", "G2D_SCALE", g2d_result.scale_factor);
        fn_kv_set_double(frame_, "photo_stats", "G2D_RMS", g2d_result.rms_residual);
        fn_kv_set_double(frame_, "photo_stats", "G2D_R_SQUARED", g2d_result.mult_r_squared);
        fn_kv_set_double(frame_, "photo_stats", "G2D_SIGMA_RESIDUAL", g2d_result.sigma_residual);
    }

    return true;
}

// stage 6: SNR - SNR 估计 (snr_estimator.dll)
bool Orchestrator::run_stage_snr(TaskResult& result) {
    LOG_INFO("orchestrator", "[SNR] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::SNR)) {
        LOG_WARN("orchestrator", "[SNR] SNR DLL 未加载, 跳过");
        return true;
    }

    // run_single 旧版调用 (无 frame_): 保留骨架行为
    if (frame_ == nullptr) {
        LOG_WARN("orchestrator", "[SNR] frame_ 为空 (run_single 旧版), 跳过");
        return true;
    }

    // 获取函数指针
    auto fn_snr = dll_loader_.get_function<int (*)(
        const float*, int, int,
        const double*, int, double, float*)>(
        ModuleId::SNR, "snr_estimate");
    auto fn_get_block = dll_loader_.get_function<const AioBlock* (*)(const PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_get_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");
    auto fn_add_block_move = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block_move");
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");

    if (!fn_snr || !fn_get_block || !fn_remove_block || !fn_add_block_move) {
        LOG_ERROR("orchestrator", "[SNR] 函数指针获取失败");
        result.error_msg = "[SNR] 函数指针获取失败";
        return false;
    }

    // 读取 data 块
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] data 块不存在");
        result.error_msg = "[SNR] data 块不存在";
        return false;
    }
    int width = data_block->dims[1];
    int height = data_block->dims[0];
    float* pixels = static_cast<float*>(data_block->data);

    // 读取 psf 块 (FLOAT64 [N,9])
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr) {
        // psf 块不存在 (PSF 阶段为骨架), 跳过 SNR
        LOG_WARN("orchestrator", "[SNR] psf 块不存在 (PSF 阶段未执行), 跳过");
        return true;
    }
    int n_stars = psf_block->dims[0];
    double* psf = static_cast<double*>(psf_block->data);

    // 读取 sigma_residual (来自 photo_stats KV 块, 默认 0.0)
    double sigma_residual = 0.0;
    if (fn_kv_get_double) {
        sigma_residual = fn_kv_get_double(frame_, "photo_stats", "SIGMA_RESIDUAL", 0.0);
    }

    LOG_INFO("orchestrator", "[SNR] n_stars=" + std::to_string(n_stars)
             + " sigma_residual=" + std::to_string(sigma_residual));

    // 分配输出 SNR 缓冲
    int64_t n_pix = static_cast<int64_t>(width) * height;
    float* out_snr = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    if (out_snr == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] 分配输出缓冲失败");
        result.error_msg = "[SNR] 分配输出缓冲失败";
        return false;
    }

    // 调用 snr_estimate
    int ret = fn_snr(pixels, height, width, psf, n_stars, sigma_residual, out_snr);
    if (ret == 3) {
        LOG_ERROR("orchestrator", "[SNR] snr_estimate 失败: nullptr 参数");
        result.error_msg = "[SNR] snr_estimate 失败: nullptr 参数";
        std::free(out_snr);
        return false;
    }
    // ret=1 (n_stars<=0) 或 ret=2 (sigma<=0) 为退化路径, SNR 已填充, 继续写块

    // 写入 snr 块 (FLOAT32 [H,W], 转移所有权)
    fn_remove_block(frame_, "snr");
    int dims[2] = {height, width};
    ret = fn_add_block_move(frame_, "snr", AIO_BLOCK_FLOAT32,
                            out_snr, n_pix, dims, 2, "SNR 图 (乘法模型)");
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[SNR] 写入 snr 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[SNR] 写入 snr 块失败";
        std::free(out_snr);
        return false;
    }
    // out_snr 所有权已转移给 frame_

    LOG_INFO("orchestrator", "[SNR] 完成 (ret=" + std::to_string(ret) + ")");
    return true;
}

// stage 8: GRADIENT_SPHERE - 球面梯度校准 (healpix_stack.dll hp_stack_gradient_corrected)
bool Orchestrator::run_stage_gradient_sphere(TaskResult& result) {
    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::GRADIENT_SPHERE)) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] GRADIENT_SPHERE DLL 未加载");
        result.error_msg = "[GRADIENT_SPHERE] DLL 未加载";
        return false;
    }

    if (stage2_hiss_files_.empty()) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] 无 .hiss 输入文件");
        result.error_msg = "[GRADIENT_SPHERE] 无 .hiss 输入文件";
        return false;
    }

    // 获取函数指针
    auto fn_gradient = dll_loader_.get_function<int (*)(
        const char**, int, const char*, const char*,
        double, int, int, double)>(
        ModuleId::GRADIENT_SPHERE, "hp_stack_gradient_corrected");

    if (!fn_gradient) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] hp_stack_gradient_corrected 函数未找到");
        result.error_msg = "[GRADIENT_SPHERE] 函数未找到";
        return false;
    }

    // 构建 hiss_paths 数组 (const char**)
    int n_frames = static_cast<int>(stage2_hiss_files_.size());
    std::vector<const char*> hiss_paths;
    hiss_paths.reserve(n_frames);
    for (const auto& p : stage2_hiss_files_) {
        hiss_paths.push_back(p.c_str());
    }

    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 帧数: " + std::to_string(n_frames)
             + " 输出: " + current_output_hcsd_);

    // 调用 hp_stack_gradient_corrected
    // 默认参数: sigma=3.0, max_iter=5, gradient_max_iter=10, gradient_lambda=1e-4
    // gaia_data_dir 传 nullptr (跳过星拒绝)
    int ret = fn_gradient(hiss_paths.data(), n_frames,
                          nullptr,  // gaia_data_dir (跳过星拒绝)
                          current_output_hcsd_.c_str(),
                          3.0, 5, 10, 1e-4);
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] hp_stack_gradient_corrected 失败: ret=" + std::to_string(ret));
        result.error_msg = "[GRADIENT_SPHERE] 失败: " + std::to_string(ret);
        return false;
    }

    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 完成");
    return true;
}

// stage 9: STACK - Winsorized sigma clip + SNR²加权叠加 (healpix_stack.dll)
bool Orchestrator::run_stage_stack(TaskResult& result) {
    LOG_INFO("orchestrator", "[STACK] 开始");

    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::STACK)) {
        LOG_WARN("orchestrator", "[STACK] STACK DLL 未加载, 跳过");
        return true;
    }

    // GRADIENT_SPHERE 阶段已通过 hp_stack_gradient_corrected 完成完整流程:
    //   采样 → Gauss-Seidel 梯度拟合 → 校正叠加 → .hcsd 输出
    // hp_stack_run 接收 PipelineFrame 数组 (非 .hiss 文件), 不适用于 stage2
    // 当前保留骨架, .hcsd 已由 GRADIENT_SPHERE 生成
    LOG_INFO("orchestrator", "[STACK] 跳过: .hcsd 已由 GRADIENT_SPHERE 生成");
    return true;
}

// ============================================================================
// run_stage1 - spec §2.3.3 单帧预处理 (FITS -> .hiss, stage 0-7)
// 串行执行 8 个 stage, 各阶段调用对应 DLL 模块 (骨架), 输出 timings
// ============================================================================
TaskResult Orchestrator::run_stage1(const std::string& fits_path,
                                    const std::string& output_hiss,
                                    const std::string& config_json) {
    TaskResult result;
    result.success = false;
    result.frame_name = fits_path;

    LOG_INFO("orchestrator", "========== stage1: 单帧预处理 (FITS -> .hiss) ==========");
    LOG_INFO("orchestrator", "输入 FITS: " + fits_path);
    LOG_INFO("orchestrator", "输出 .hiss: " + output_hiss);
    if (!config_json.empty()) {
        LOG_INFO("orchestrator", "配置 JSON: " + config_json);
    }

    // 参数校验
    if (fits_path.empty()) {
        result.error_msg = "FITS 路径为空";
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }
    if (!fs::exists(fits_path)) {
        result.error_msg = "FITS 文件不存在: " + fits_path;
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }
    if (output_hiss.empty()) {
        result.error_msg = "输出 .hiss 路径为空";
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }

    // 加载 DLL (如果未加载, 允许部分模块加载失败继续执行)
    if (!dlls_loaded_) {
        std::string err;
        // lib_base_dir 留空, 使用相对路径 (项目根目录执行)
        if (!init_dlls("", err)) {
            LOG_WARN("orchestrator", "DLL 加载警告: " + err + " (部分阶段将跳过)");
        }
    }

    // 进入运行状态
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_frame_ = fits_path;
        current_stage_ = PipelineStage::CALIBRATE;
        start_time_ = std::chrono::steady_clock::now();
    }
    state_ = TaskState::RUNNING;

    // 创建 PipelineFrame (stage1 流水线帧, 各 stage handler 通过 frame_ 读写命名块)
    auto fn_frame_create = dll_loader_.get_function<PipelineFrame* (*)()>(
        ModuleId::AIO, "aio_pipeline_frame_create");
    auto fn_frame_destroy = dll_loader_.get_function<void (*)(PipelineFrame*)>(
        ModuleId::AIO, "aio_pipeline_frame_destroy");
    if (!fn_frame_create || !fn_frame_destroy) {
        result.error_msg = "[stage1] PipelineFrame 函数指针获取失败";
        LOG_ERROR("orchestrator", result.error_msg);
        state_ = TaskState::FAILED;
        return result;
    }
    frame_ = fn_frame_create();
    if (frame_ == nullptr) {
        result.error_msg = "[stage1] PipelineFrame 创建失败";
        LOG_ERROR("orchestrator", result.error_msg);
        state_ = TaskState::FAILED;
        return result;
    }
    // 设置 stage1 路径 (供 stage handler 使用)
    current_fits_path_ = fits_path;
    current_output_path_ = output_hiss;

    // 串行执行 stage 0-7 (lambda: 带计时调用 stage handler)
    auto run_v2_with_timing = [&](PipelineStageV2 stage, const char* name,
                                   bool (Orchestrator::*fn)(TaskResult&)) -> bool {
        LOG_INFO("orchestrator", "---------- stage1 阶段: " + std::string(name) + " ----------");
        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        StageTiming st;
        st.stage = PipelineStage::CALIBRATE;  // 复用旧枚举 (StageTiming 仍用旧枚举)
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));
        return ok;
    };

    bool ok = true;
    ok = ok && run_v2_with_timing(PipelineStageV2::READ_FITS,   "READ_FITS",
                                   &Orchestrator::run_stage_read_fits);
    ok = ok && run_v2_with_timing(PipelineStageV2::CALIBRATE,   "CALIBRATE",
                                   &Orchestrator::run_stage_calibrate);
    ok = ok && run_v2_with_timing(PipelineStageV2::PLATESOLVE,  "PLATESOLVE",
                                   &Orchestrator::run_stage_platesolve);
    ok = ok && run_v2_with_timing(PipelineStageV2::PSF,         "PSF",
                                   &Orchestrator::run_stage_psf);
    ok = ok && run_v2_with_timing(PipelineStageV2::PHOTOMETRIC, "PHOTOMETRIC",
                                   &Orchestrator::run_stage_photometric);
    ok = ok && run_v2_with_timing(PipelineStageV2::GRADIENT_2D, "GRADIENT_2D",
                                   &Orchestrator::run_stage_gradient_2d);
    ok = ok && run_v2_with_timing(PipelineStageV2::SNR,         "SNR",
                                   &Orchestrator::run_stage_snr);
    ok = ok && run_v2_with_timing(PipelineStageV2::DRIZZLE,     "DRIZZLE",
                                   &Orchestrator::run_stage_drizzle);

    // 销毁 PipelineFrame (无论成功失败)
    if (frame_ != nullptr && fn_frame_destroy) {
        fn_frame_destroy(frame_);
        frame_ = nullptr;
    }

    result.success = ok;
    result.output_ahpx_path = ok ? output_hiss : "";
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;

    LOG_INFO("orchestrator", "========== stage1 "
             + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}

// ============================================================================
// run_stage2 - spec §2.3.3 多帧合并 (.hiss -> .hcsd, stage 8-9)
// 串行执行 2 个 stage: GRADIENT_SPHERE -> STACK
// ============================================================================
TaskResult Orchestrator::run_stage2(const std::string& hiss_dir,
                                    const std::string& output_hcsd,
                                    const std::string& config_json) {
    TaskResult result;
    result.success = false;
    result.frame_name = hiss_dir;  // stage2 用目录作为输入标识

    LOG_INFO("orchestrator", "========== stage2: 多帧合并 (.hiss -> .hcsd) ==========");
    LOG_INFO("orchestrator", "输入 .hiss 目录: " + hiss_dir);
    LOG_INFO("orchestrator", "输出 .hcsd: " + output_hcsd);
    if (!config_json.empty()) {
        LOG_INFO("orchestrator", "配置 JSON: " + config_json);
    }

    // 参数校验
    if (hiss_dir.empty()) {
        result.error_msg = ".hiss 目录为空";
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }
    if (!fs::exists(hiss_dir) || !fs::is_directory(hiss_dir)) {
        result.error_msg = ".hiss 目录不存在或不是目录: " + hiss_dir;
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }
    if (output_hcsd.empty()) {
        result.error_msg = "输出 .hcsd 路径为空";
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }

    // 收集 .hiss 文件列表
    std::vector<std::string> hiss_files;
    for (const auto& entry : fs::directory_iterator(hiss_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (ext == ".hiss") {
            hiss_files.push_back(entry.path().string());
        }
    }
    std::sort(hiss_files.begin(), hiss_files.end());
    LOG_INFO("orchestrator", "发现 " + std::to_string(hiss_files.size()) + " 个 .hiss 文件");
    if (hiss_files.empty()) {
        result.error_msg = "目录下无 .hiss 文件: " + hiss_dir;
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }

    // 保存到成员变量供 stage handler 使用
    stage2_hiss_files_ = hiss_files;
    current_output_hcsd_ = output_hcsd;

    // 加载 DLL (如果未加载, stage2 仅需 GRADIENT_SPHERE/STACK 模块)
    if (!dlls_loaded_) {
        std::string err;
        if (!init_dlls("", err)) {
            LOG_WARN("orchestrator", "DLL 加载警告: " + err + " (部分阶段将跳过)");
        }
    }

    // 进入运行状态
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_frame_ = hiss_dir;
        current_stage_ = PipelineStage::STACK;
        start_time_ = std::chrono::steady_clock::now();
    }
    state_ = TaskState::RUNNING;

    // 串行执行 stage 8-9 (lambda: 带计时调用 stage handler)
    auto run_v2_with_timing = [&](PipelineStageV2 stage, const char* name,
                                   bool (Orchestrator::*fn)(TaskResult&)) -> bool {
        LOG_INFO("orchestrator", "---------- stage2 阶段: " + std::string(name) + " ----------");
        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        StageTiming st;
        st.stage = PipelineStage::STACK;  // stage2 使用 STACK 枚举
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));
        return ok;
    };

    bool ok = true;
    ok = ok && run_v2_with_timing(PipelineStageV2::GRADIENT_SPHERE, "GRADIENT_SPHERE",
                                   &Orchestrator::run_stage_gradient_sphere);
    ok = ok && run_v2_with_timing(PipelineStageV2::STACK,           "STACK",
                                   &Orchestrator::run_stage_stack);

    result.success = ok;
    result.output_ahpx_path = ok ? output_hcsd : "";
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;

    LOG_INFO("orchestrator", "========== stage2 "
             + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}
