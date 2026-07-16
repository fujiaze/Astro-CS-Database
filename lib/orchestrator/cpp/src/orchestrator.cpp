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

    // 检查 header KV 是否已含 WCS (FITS 文件可能已带 WCS)
    auto fn_kv_get = dll_loader_.get_function<const char* (*)(
        const PipelineFrame*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_get");
    if (fn_kv_get) {
        const char* crval1 = fn_kv_get(frame_, "header", "CRVAL1");
        const char* cd11 = fn_kv_get(frame_, "header", "CD1_1");
        if (crval1 != nullptr && cd11 != nullptr) {
            LOG_INFO("orchestrator", "[PLATESOLVE] header 已含 WCS, 跳过 plate solve");
            return true;
        }
    }

    // TODO: 实际调用 ipv_solve_from_memory 需要:
    //   1. gaia_client handle (orchestrator 未加载 gaia_client.dll)
    //   2. star_detector handle (orchestrator 未加载 star_detector.dll)
    //   3. ra0/dec0/focal_length/pixel_size (需从 config 或 header 解析)
    // 当前保留骨架, 后续 Task 集成 gaia_client + star_detector DLL 后实现
    LOG_WARN("orchestrator", "[PLATESOLVE] 保留骨架: 需要 gaia_client + star_detector handle (未加载)");
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

    // TODO: dpsf_fit_batch 需要 star_det 块 (星点坐标), 由 star_detector 生成
    //   orchestrator 未加载 star_detector.dll, PLATESOLVE 也未注入 detector handle
    //   所以 star_det 块不存在, PSF 拟合无法执行
    // 当前保留骨架, 后续 Task 集成 star_detector 后实现
    LOG_WARN("orchestrator", "[PSF] 保留骨架: 需要 star_det 块 (star_detector 未加载)");
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

    // TODO: pc_calibrate_simple_with_gaia 需要 gaia_client handle (orchestrator 未加载)
    //       pc_calibrate_simple 需要预计算 gaia_fsyn 数组 (需先锥形搜索+光谱积分)
    //   两者都依赖 gaia_client.dll, 当前保留骨架
    //   后续 Task 集成 gaia_client 后实现完整测光校准
    LOG_WARN("orchestrator", "[PHOTOMETRIC] 保留骨架: 需要 gaia_client handle (未加载)");
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

    // TODO: gradient_2d_calibrate 需要:
    //   1. gaia_ra/dec/mag/fsyn 数组 (需 gaia_client 锥形搜索+光谱积分)
    //   2. psf_cx/cy/flux/status 数组 (需 PSF 阶段成功, 当前 PSF 为骨架)
    //   3. WCS + SIP 参数 (需 PLATESOLVE 成功或 FITS 自带 WCS)
    //   依赖 gaia_client + star_detector + PSF, 当前均未就绪
    //   后续 Task 集成完整前置链后实现
    LOG_WARN("orchestrator", "[GRADIENT_2D] 保留骨架: 需要 gaia 数据 + psf 块 (前置依赖未就绪)");
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
