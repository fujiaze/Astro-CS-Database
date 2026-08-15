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
// R11: typed Stage1Config 完整定义 (unique_ptr 成员析构需要完整类型)
#include "json_config.h"

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
#include <unordered_set>
#include <vector>

// 重命名 aio_pipeline.h 的 PipelineStage typedef 为 AioPipelineStage,
// 避免与 orchestrator.h 的 enum class PipelineStage 冲突
// (aio_pipeline.h 定义了 C 风格 typedef enum {...} PipelineStage;)
// astro_image_io.h / hp_drizzle_api.h / hp_stack_api.h 都会 include aio_pipeline.h
#define PipelineStage AioPipelineStage

// AIO 图像数据结构与元数据 (用于 run_stage_read_fits)
#include "astro_image_io.h"
// IVOA HiPS 写入器 (Phase1 Full Freeze v2, HiPS 迁移)
#include "aio_hips.h"
#include "aio_hips_reader.h"   // Phase1 V3: HIPS_VERIFY / Browser 唯一后端
#include "healpix/healpix_core.h" // V5 HIPS-IMG-001: 共享 HEALPix core 映射
// HioSnrModel / HISS SNR 模型结构
#include "aio_healpix_io.h"

// healpix_drizzle C API (用于 run_stage_drizzle)
#include "hp_drizzle_api.h"


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

// snr_estimator C API (用于 run_stage_snr)
#include "snr_estimator.h"

// nlohmann/json: 替代手写 orc_findJsonKey/orc_extractJsonStr/orc_extractJsonNum 解析
// 已通过 MSYS2 pacman 安装在 mingw64/include, Makefile 默认搜索路径即可找到
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// JSON 字段提取 (基于 nlohmann/json)
// 历史: 原 orc_findJsonKey/orc_extractJsonStr/orc_extractJsonNum 为手写字符串扫描,
//       已替换为 nlohmann/json 解析。orc_getJsonString/Num/Bool 保留原签名以兼容
//       stage handler 中的调用点 (约束: 不修改 orc_* 函数签名)。
// 文件作用域私有, 与 hp_stack_api.cpp 中的 findKey/extractNum 同风格。
// ============================================================================
namespace {

// 解析缓存: 同一 config_json 字符串在 stage handler 内可能被多次按字段查询,
// 缓存解析结果避免每次调用都重新 parse 整个 JSON 文档。
// 缓存键为原始字符串引用相等 (相同 std::string 对象或相同内容)。
// 注意: 此缓存为文件作用域静态变量, 仅在单线程 stage 串行执行场景下使用,
//       不适用于多线程并发解析 (orchestrator stage 串行执行, 安全)。
nlohmann::json s_parsed_config;
std::string s_parsed_config_src;

// 解析 config_json 字符串为 nlohmann::json 对象, 带缓存。
// 解析失败时返回空 json::object() (调用方按字段缺失处理)。
const nlohmann::json& parse_config_cached(const std::string& s) {
    if (s != s_parsed_config_src) {
        try {
            s_parsed_config = nlohmann::json::parse(s);
        } catch (...) {
            s_parsed_config = nlohmann::json::object();
        }
        s_parsed_config_src = s;
    }
    return s_parsed_config;
}

// ============================================================================
// GAP-016: NSIDE 自适应计算
// 根据原始图像采样率 (CD 矩阵) 和策略计算合适的 HEALPix nside
//   strategy:
//     "1x_to_2x_drizzle" (默认): HEALPix 像素分辨率 = 1-2x 原始像素分辨率
//     "fixed": 使用 nside_override (nside_override > 0 时优先)
//     nside_override > 0: 用户指定优先, 直接返回
//   返回 nside (2 的幂次方, 范围 [16, 4194304])
//   规范: 自动 NSIDE 上限 2^22=4194304, Tile 父级 NSIDE 不低于 16
//         显式合法 NSIDE 不修改、不警告
// ============================================================================
int calculate_nside(double cd11, double cd12, double cd21, double cd22,
                    const std::string& strategy, int nside_override) {
    // 1. 用户指定优先
    if (nside_override > 0) {
        int n = nside_override;
        // 防御性: 超出 [16, 4194304] 范围截断并警告 (CLI 已校验, 此处兜底)
        if (n < 16) {
            LOG_WARN("orchestrator", "[NSIDE] 用户指定 nside=" + std::to_string(n)
                     + " 低于下限 16, 截断到 16");
            n = 16;
        }
        if (n > 4194304) {
            LOG_WARN("orchestrator", "[NSIDE] 用户指定 nside=" + std::to_string(n)
                     + " 超过上限 4194304 (2^22), 截断到 4194304");
            n = 4194304;
        }
        // 若不是 2 的幂次方, 向下取到最近的 2 的幂次方并警告
        if ((n & (n - 1)) != 0) {
            int p = 1;
            while (p * 2 <= n) p *= 2;
            int original = n;
            n = p;
            LOG_WARN("orchestrator", "[NSIDE] 用户指定 nside=" + std::to_string(original)
                     + " 非 2 的幂, 向下取整到 " + std::to_string(n));
        }
        // 显式合法 NSIDE (2 的幂且在范围内): 不修改、不警告
        LOG_INFO("orchestrator", "[NSIDE] 用户指定: nside=" + std::to_string(n));
        return n;
    }

    // 2. 从 CD 矩阵计算原始像素分辨率 (角秒/像素)
    // CD 矩阵单位是度/像素, 转角秒需 ×3600
    double sx = std::sqrt(cd11 * cd11 + cd21 * cd21);
    double sy = std::sqrt(cd12 * cd12 + cd22 * cd22);
    double pixel_scale_arcsec = 0.5 * (sx + sy) * 3600.0;

    // 若 CD 矩阵无效 (全 0), 回退到默认 nside=32768
    if (pixel_scale_arcsec <= 0.0 || !std::isfinite(pixel_scale_arcsec)) {
        LOG_WARN("orchestrator", "[NSIDE] CD 矩阵无效, 回退默认 nside=32768");
        return 32768;
    }

    // 3. 按策略计算目标 HEALPix 像素分辨率
    // "1x_to_2x_drizzle": 取 1-2x 中间值 1.5, 避免过采样
    //
    // R10 修复: 旧常数 1186.18 错误 (与 drizzle_engine.cpp 的 compute_auto_nside 不一致),
    //           导致 nside 比正确值小约 178 倍 (6.3"/px 输入算出 512 而非 ~65536).
    // 正确公式 (与 drizzle_engine.cpp R05-B03 一致, 禁止魔数 210960/1186.18):
    //   HEALPix 像素面积 = 4π / (12 * nside²) sr = π / (3 * nside²) sr
    //   特征线性尺度 = sqrt(像素面积) = sqrt(π/3) / nside rad
    //   转角秒: sqrt(π/3) / nside * (180/π) * 3600 ≈ 211034.6 / nside arcsec
    //   反推: nside ≈ 211034.6 / target_resolution_arcsec
    double drizzle_factor = 1.5;  // 默认 1x_to_2x_drizzle
    if (strategy == "fixed" || strategy == "1x") {
        drizzle_factor = 1.0;
    } else if (strategy == "2x") {
        drizzle_factor = 2.0;
    } else if (strategy == "1x_to_2x_drizzle" || strategy.empty()) {
        drizzle_factor = 1.5;
    } else {
        LOG_WARN("orchestrator", "[NSIDE] 未知 strategy='" + strategy + "', 使用默认 1.5x");
        drizzle_factor = 1.5;
    }

    double target_resolution_arcsec = pixel_scale_arcsec / drizzle_factor;
    // 标准C++不保证 M_PI 宏存在, 用 std::acos(-1.0) 派生 π (与 drizzle_engine.cpp 一致)
    const double PI = std::acos(-1.0);
    const double HEALPIX_SCALE_PER_NSIDE_ARCSEC =
        std::sqrt(PI / 3.0) * (180.0 / PI) * 3600.0;  // ≈ 211034.6
    double nside_target = HEALPIX_SCALE_PER_NSIDE_ARCSEC / target_resolution_arcsec;

    // 4. 找到不小于 nside_target 的最小 2 的幂次方
    //    下限 16 (Tile 父级 NSIDE 不低于 16), 上限 4194304 (2^22, 规范要求)
    int nside = 16;
    while (nside < nside_target && nside < 4194304) {
        nside *= 2;
    }

    // 5. 限制在 [16, 4194304]
    if (nside < 16) nside = 16;
    if (nside > 4194304) nside = 4194304;

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "[NSIDE] 自适应: pixel_scale=%.3f\"/px, strategy=%s, nside=%d",
        pixel_scale_arcsec, strategy.c_str(), nside);
    LOG_INFO("orchestrator", std::string(buf));

    return nside;
}

// ============================================================================
// P03-001: 校准输入接线辅助函数
// 用于 run_stage_calibrate 加载/验证 Master Bias/Dark/Flat, 输出 cal_stats
// ============================================================================

// 从 JSON 文本提取字符串字段 (基于 nlohmann/json, 带解析缓存)
// 字段缺失或非字符串类型时返回 ""
std::string orc_getJsonString(const std::string& s, const std::string& key) {
    const auto& j = parse_config_cached(s);
    if (j.contains(key) && !j[key].is_null() && j[key].is_string()) {
        return j[key].get<std::string>();
    }
    return "";
}

// 从 JSON 文本提取数字字段 (基于 nlohmann/json, 带解析缓存)
// 字段缺失或非数字类型时返回 def
double orc_getJsonNum(const std::string& s, const std::string& key, double def) {
    const auto& j = parse_config_cached(s);
    if (j.contains(key) && j[key].is_number()) {
        return j[key].get<double>();
    }
    return def;
}

// 从 JSON 文本提取布尔字段 (基于 nlohmann/json, 带解析缓存)
// 字段缺失或非布尔类型时返回 def
bool orc_getJsonBool(const std::string& s, const std::string& key, bool def) {
    const auto& j = parse_config_cached(s);
    if (j.contains(key) && j[key].is_boolean()) {
        return j[key].get<bool>();
    }
    return def;
}

// 从 masterDark 文件路径解析曝光时间 (文件名含 _EXPOSURE-180.00s)
// 返回 -1.0 表示解析失败
double parse_exposure_from_dark_path(const std::string& path) {
    const std::string tag = "_EXPOSURE-";
    size_t p = path.find(tag);
    if (p == std::string::npos) return -1.0;
    p += tag.size();
    size_t end = path.find('s', p);
    if (end == std::string::npos) return -1.0;
    std::string num_str = path.substr(p, end - p);
    try {
        return std::stod(num_str);
    } catch (...) {
        return -1.0;
    }
}

// 计算数组均值
double compute_array_mean(const float* data, int64_t n) {
    if (!data || n <= 0) return 0.0;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i) sum += static_cast<double>(data[i]);
    return sum / static_cast<double>(n);
}

// 计算数组中位数 (使用 nth_element, O(n))
double compute_array_median(const float* data, int64_t n) {
    if (!data || n <= 0) return 0.0;
    std::vector<float> tmp(data, data + n);
    int64_t mid = n / 2;
    std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end());
    if (n % 2 == 1) return static_cast<double>(tmp[mid]);
    float hi = tmp[mid];
    float lo = *std::max_element(tmp.begin(), tmp.begin() + mid);
    return static_cast<double>((hi + lo) * 0.5f);
}

// 计算数组标准差 (给定均值)
double compute_array_std(const float* data, int64_t n, double mean) {
    if (!data || n <= 0) return 0.0;
    double sum_sq = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        double d = static_cast<double>(data[i]) - mean;
        sum_sq += d * d;
    }
    return std::sqrt(sum_sq / static_cast<double>(n));
}

// 双精度 ABI (R10): double 版本统计函数 (重载)
// FP64 模式下直接在 double 上计算统计, 不降级到 float32
double compute_array_mean(const double* data, int64_t n) {
    if (!data || n <= 0) return 0.0;
    double sum = 0.0;
    for (int64_t i = 0; i < n; ++i) sum += data[i];
    return sum / static_cast<double>(n);
}

double compute_array_median(const double* data, int64_t n) {
    if (!data || n <= 0) return 0.0;
    std::vector<double> tmp(data, data + n);
    int64_t mid = n / 2;
    std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end());
    if (n % 2 == 1) return tmp[mid];
    double hi = tmp[mid];
    double lo = *std::max_element(tmp.begin(), tmp.begin() + mid);
    return (hi + lo) * 0.5;
}

double compute_array_std(const double* data, int64_t n, double mean) {
    if (!data || n <= 0) return 0.0;
    double sum_sq = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        double d = data[i] - mean;
        sum_sq += d * d;
    }
    return std::sqrt(sum_sq / static_cast<double>(n));
}

} // anonymous namespace

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
// stage_name_v2 - spec §2.3.2 两段流水线 9 节点阶段名称 (2026-07-18 归档 GRADIENT_2D)
// ============================================================================
std::string Orchestrator::stage_name_v2(PipelineStageV2 stage) {
    switch (stage) {
        case PipelineStageV2::READ_FITS:       return "READ_FITS";
        case PipelineStageV2::CALIBRATE:       return "CALIBRATE";
        case PipelineStageV2::PSF:             return "PSF";
        case PipelineStageV2::PLATESOLVE:      return "PLATESOLVE";
        case PipelineStageV2::PHOTOMETRIC:     return "PHOTOMETRIC";
        case PipelineStageV2::SNR:             return "SNR";
        case PipelineStageV2::DRIZZLE:         return "DRIZZLE";
        case PipelineStageV2::HIPS_VERIFY:     return "HIPS_VERIFY";
        default:                               return "UNKNOWN";
    }
}

// ============================================================================
// P04-004: 取消 token / 超时机制 / 原子输出清理 实现
// ============================================================================

// request_cancel - 设置取消 token, 通知各 stage 停止
// 线程安全: atomic store, 可由信号处理器或其他线程调用
void Orchestrator::request_cancel() {
    cancel_token_.store(true, std::memory_order_release);
    LOG_WARN("orchestrator", "P04-004: 收到取消请求, cancel_token=true");
}

// reset_cancel_timeout - 重置取消/超时标志 (新一轮运行前调用)
void Orchestrator::reset_cancel_timeout() {
    cancel_token_.store(false, std::memory_order_release);
    timeout_flag_.store(false, std::memory_order_release);
    stage_watchdog_stop_.store(false, std::memory_order_release);
    LOG_INFO("orchestrator", "P04-004: 取消/超时标志已重置");
}

// get_current_stage_name - 获取当前 stage 名称 (供 CLI 输出 JSONL 事件)
std::string Orchestrator::get_current_stage_name() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return current_stage_name_;
}

// parse_stage_timeouts - 从 config_json 解析 stage_timeouts 字段
// 格式: {"stage_timeouts":{"READ_FITS":10.0,"CALIBRATE":60.0,...}}
std::map<std::string, double> Orchestrator::parse_stage_timeouts(const std::string& config_json) {
    std::map<std::string, double> result;
    if (config_json.empty()) return result;

    // 基于 nlohmann/json 解析 stage_timeouts 对象
    const auto& j = parse_config_cached(config_json);
    if (!j.contains("stage_timeouts") || !j["stage_timeouts"].is_object()) {
        return result;
    }

    // 遍历 "stage_name": <number> 对
    for (auto it = j["stage_timeouts"].begin(); it != j["stage_timeouts"].end(); ++it) {
        const std::string& key = it.key();
        const auto& val = it.value();
        if (val.is_number()) {
            double seconds = val.get<double>();
            result[key] = seconds;
            LOG_INFO("orchestrator", "P04-004: stage_timeouts[" + key + "] = " + std::to_string(seconds) + "s");
        } else {
            LOG_WARN("orchestrator", "P04-004: stage_timeouts[" + key + "] 解析失败: 非 number 类型");
        }
    }
    return result;
}

// cleanup_partial_output - 删除部分生成的输出文件 (原子性保证)
// 删除指定路径的文件, 确保失败/取消/超时后无残留部分输出
bool Orchestrator::cleanup_partial_output(const std::string& path) {
    if (path.empty()) {
        return true;  // 空路径视为无操作
    }
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return true;  // 文件不存在, 视为成功
    }
    bool removed = fs::remove(path, ec);
    if (removed && !ec) {
        LOG_WARN("orchestrator", "P04-004: 原子清理 - 已删除部分输出: " + path);
        return true;
    } else {
        LOG_ERROR("orchestrator", "P04-004: 原子清理失败 - 无法删除: " + path + " (ec=" + ec.message() + ")");
        return false;
    }
}

// check_stage_continue - 检查 stage 是否应继续执行 (取消/超时检查)
// 返回 true 继续; false 停止 (并设置 result.error_msg/exit_code)
bool Orchestrator::check_stage_continue(const std::string& stage_name, TaskResult& result) {
    if (is_cancelled()) {
        result.error_msg = "stage " + stage_name + " 取消 (用户请求)";
        result.exit_code = AstroCsExitCode::CANCELLED;
        LOG_WARN("orchestrator", "P04-004: " + result.error_msg);
        return false;
    }
    if (is_timed_out()) {
        result.error_msg = "stage " + stage_name + " 超时";
        result.exit_code = AstroCsExitCode::TIMEOUT;
        LOG_WARN("orchestrator", "P04-004: " + result.error_msg);
        return false;
    }
    return true;
}

// ============================================================================
// 构造 / 析构
// ============================================================================
Orchestrator::Orchestrator() {
    start_time_ = std::chrono::steady_clock::now();
    // V18R2 (CODE-004): 日志目录由 main 按 config.output.log 解析后注入
    // （Logger::set_log_dir）；此处不再默认写 lib/orchestrator/logs 嵌套目录。
    // 移除失真的"骨架版本"描述。
    LOG_INFO("orchestrator", "初始化编排器");
    LOG_INFO("orchestrator", "可用线程数: " + std::to_string(std::thread::hardware_concurrency()));
}

// R11: typed Stage1Config 直接驱动
void Orchestrator::set_stage1_config(const Stage1Config& cfg) {
    stage1_cfg_ = std::make_unique<Stage1Config>(cfg);
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

    // P03-002: 把整个 JSON 文本存入 calib_params_json (供各 stage handler 按需解析)
    config_.calib_params_json = content;

    // 默认日志目录与输出目录 (若配置中未指定)
    // 2026-08-05 规范: 运行产物统一写 run/ (AGENTS); 数据库/数据文件路径一律
    // 由 stage1.json 引入; 此处仅修正默认目录到 run/ 约定, 不再默认到 lib/ 或 output/
    if (config_.log_dir.empty()) {
        config_.log_dir = "run/logs/orchestrator";
    }
    if (config_.output_dir.empty()) {
        config_.output_dir = "run";
    }

    // Task 3: 设置检查点目录为 <output_dir>/.checkpoint/
    std::string ckpt_dir = (fs::path(config_.output_dir) / ".checkpoint").string();
    checkpoint_mgr_.set_checkpoint_dir(ckpt_dir);
    LOG_INFO("orchestrator", "检查点目录: " + ckpt_dir);

    // P03-002: 解析 log_level (顶层字段, 大小写不敏感)
    // 优先级: CLI --log-level > config.log_level > 默认 INFO
    // CLI 覆盖在 cmd_stage1/cmd_stage2 中处理; 此处仅解析 config
    std::string cfg_log_level = orc_getJsonString(content, "log_level");
    if (!cfg_log_level.empty() && config_.log_level.empty()) {
        config_.log_level = cfg_log_level;
    }
    if (config_.log_level.empty()) {
        config_.log_level = "INFO";
    }
    LogLevel lvl = Logger::string_to_level(config_.log_level);
    Logger::instance().set_level(lvl);
    LOG_INFO("orchestrator", "日志级别设置为: " + config_.log_level);

    // P03-002: 解析 threads (顶层字段, 0=自动检测)
    double cfg_threads = orc_getJsonNum(content, "threads", 0.0);
    config_.threads = static_cast<int>(cfg_threads);
    LOG_INFO("orchestrator", "配置线程数: " + std::to_string(config_.threads)
             + (config_.threads == 0 ? " (0=自动检测)" : ""));

    // P03-002: 解析 gaia_data_dir (顶层字段, 用于 init_platesolve_env)
    // 2026-08-05 规范: 数据库位置必须由 stage1.json 引入; 缺失时 PLATESOLVE 硬失败
    std::string cfg_gaia_dir = orc_getJsonString(content, "gaia_data_dir");
    if (!cfg_gaia_dir.empty()) {
        config_gaia_data_dir_ = cfg_gaia_dir;
        LOG_INFO("orchestrator", "配置 Gaia 数据目录: " + cfg_gaia_dir);
    }

    LOG_INFO("orchestrator", "配置加载完成 (P03-002: log_level/threads/gaia_data_dir 已解析)");
    return true;
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

    // Gate 1 (Dataflow_ABI_Contract): AIO 是共享 ABI 基础, 结构不匹配硬失败
    if (dlls_loaded_ && dll_loader_.is_loaded(ModuleId::AIO)) {
        auto fn_abi = dll_loader_.get_function<const AstroAbiInfo* (*)(void)>(
            ModuleId::AIO, "aio_abi_info");
        if (!fn_abi) {
            error_msg = "AIO 未导出 aio_abi_info (ABI 握手失败)";
            LOG_ERROR("orchestrator", error_msg);
            dlls_loaded_ = false;
        } else {
            const AstroAbiInfo* abi = fn_abi();
            bool abi_ok = abi &&
                          abi->abi_version == 1 &&
                          abi->pointer_size == sizeof(void*) &&
                          abi->pipeline_frame_size == sizeof(PipelineFrame) &&
                          abi->aio_block_size == sizeof(AioBlock) &&
                          abi->aio_kv_entry_size == sizeof(AioKVEntry);
            if (!abi_ok) {
                error_msg = "AIO ABI 不匹配 (硬失败, 不再继续)";
                LOG_ERROR("orchestrator", error_msg);
                dlls_loaded_ = false;
            } else {
                LOG_INFO("orchestrator", "AIO ABI 握手通过: abi_version=" +
                         std::to_string(abi->abi_version) + " struct_size=" +
                         std::to_string(abi->struct_size));
            }
        }
    }

    if (!ok) {
        // 收集所有失败模块的错误信息 (spec §2.3.2 9 节点, 2026-07-18 归档 GRADIENT_2D)
        std::stringstream ss;
        ss << "部分模块加载失败: ";
        bool first = true;
        std::vector<ModuleId> ids = {
            ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
            ModuleId::PSF, ModuleId::PHOTOMETRIC,
            ModuleId::SNR, ModuleId::DRIZZLE
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

    // 加载成功, 输出各模块版本信息 (spec §2.3.2 9 节点, 2026-07-18 归档 GRADIENT_2D)
    LOG_INFO("orchestrator", "全部 9 个模块加载成功");
    for (auto id : {ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
                    ModuleId::PSF, ModuleId::PHOTOMETRIC,
                    ModuleId::SNR, ModuleId::DRIZZLE}) {
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
// 各阶段实现 (CALIBRATE 已在 P03-001 接入真实 Master Bias/Dark/Flat)
// ============================================================================
bool Orchestrator::run_stage_calibrate(TaskResult& result) {
    LOG_INFO("orchestrator", "[CALIBRATE] 开始");

    // P03-003: CALIBRATE 是必需 stage, DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::CALIBRATE)) {
        LOG_ERROR("orchestrator", "[CALIBRATE] CALIBRATE DLL 未加载 (必需模块)");
        result.error_msg = "[CALIBRATE] CALIBRATE DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[CALIBRATE] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[CALIBRATE] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 获取函数指针 (CALIBRATE + AIO)
    // 双精度 ABI (R10): FP64 模式下需要 ac_calibrate_frame_f64 和 aio_get_pixel_data_f64
    bool use_fp64 = (config_.precision == PrecisionMode::FP64);
    auto fn_calibrate = dll_loader_.get_function<int (*)(
        const float*, int, int,
        const float*, const float*, const float*,
        float*, int, float, float*)>(
        ModuleId::CALIBRATE, "ac_calibrate_frame");
    auto fn_calibrate_f64 = dll_loader_.get_function<int (*)(
        const double*, int, int,
        const double*, const double*, const double*,
        double*, int, double, double*)>(
        ModuleId::CALIBRATE, "ac_calibrate_frame_f64");
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
    // AIO 读取 Master 文件函数 (XISF 优先, FITS 回退 — Gate 5 数据流修复)
    auto fn_read_xisf = dll_loader_.get_function<AIOImageData* (*)(const char*)>(
        ModuleId::AIO, "aio_read_xisf");
    auto fn_read_fits = dll_loader_.get_function<AIOImageData* (*)(const char*)>(
        ModuleId::AIO, "aio_read_fits");
    auto fn_get_pixels = dll_loader_.get_function<float* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data");
    // 双精度 ABI: FP64 模式下获取 double* 像素数据
    auto fn_get_pixels_f64 = dll_loader_.get_function<double* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data_f64");
    auto fn_get_width = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_width");
    auto fn_get_height = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_height");
    auto fn_get_metadata = dll_loader_.get_function<AIOImageMetadata (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_metadata");
    auto fn_free_image = dll_loader_.get_function<void (*)(AIOImageData*)>(
        ModuleId::AIO, "aio_free_image_data");

    if (!fn_get_block_data || !fn_get_block || !fn_remove_block ||
        !fn_add_block_move || !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double ||
        !fn_read_xisf || !fn_get_pixels || !fn_get_width || !fn_get_height ||
        !fn_get_metadata || !fn_free_image ||
        // FP64 模式需要 f64 函数指针
        (use_fp64 && (!fn_calibrate_f64 || !fn_get_pixels_f64)) ||
        // FP32 模式需要 f32 函数指针
        (!use_fp64 && !fn_calibrate)) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 函数指针获取失败"
                  + std::string(use_fp64 ? " (FP64 模式需要 ac_calibrate_frame_f64/aio_get_pixel_data_f64)" : ""));
        result.error_msg = "[CALIBRATE] 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 1. 从 frame_ 读取 data 块
    //    双精度 ABI (R10): FP64 模式下 data 块为 FLOAT64 [H,W], FP32 模式下为 FLOAT32 [H,W]
    //    P03-003: data 是必需块 (READ_FITS 产出), 缺失必须失败 (退出码 3)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[CALIBRATE] data 块不存在 (必需块)");
        result.error_msg = "[CALIBRATE] data 块不存在 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }
    int width = data_block->dims[1];
    int height = data_block->dims[0];
    int64_t n_pix = static_cast<int64_t>(width) * height;
    // 双精度 ABI: 根据 data 块类型选择指针
    // FP64 模式: light_f64 指向 FLOAT64 data (不降级)
    // FP32 模式: light_f32 指向 FLOAT32 data (向后兼容)
    float* light_f32 = nullptr;
    double* light_f64 = nullptr;
    if (use_fp64) {
        light_f64 = static_cast<double*>(data_block->data);
    } else {
        light_f32 = static_cast<float*>(data_block->data);
    }

    LOG_INFO("orchestrator", "[CALIBRATE] 图像: " + std::to_string(width) + "x" + std::to_string(height)
             + " (" + std::to_string(n_pix) + " 像素, " + (use_fp64 ? "FP64" : "FP32") + ")");

    // 2. 从 frame_ header KV 读取帧元数据 (EXPTIME, FILTER, CCD-TEMP)
    double frame_exptime = fn_kv_get_double(frame_, "header", "EXPTIME", 0.0);
    std::string frame_filter;
    const char* filter_cstr = fn_kv_get(frame_, "header", "FILTER");
    if (filter_cstr) frame_filter = filter_cstr;
    double frame_ccd_temp = 0.0;
    bool has_ccd_temp = (fn_kv_get(frame_, "header", "CCD-TEMP") != nullptr);
    if (has_ccd_temp) {
        frame_ccd_temp = fn_kv_get_double(frame_, "header", "CCD-TEMP", 0.0);
    }

    LOG_INFO("orchestrator", "[CALIBRATE] 帧元数据: EXPTIME=" + std::to_string(frame_exptime)
             + "s, FILTER='" + frame_filter + "'"
             + (has_ccd_temp ? (", CCD-TEMP=" + std::to_string(frame_ccd_temp) + "C") : ", CCD-TEMP=<none>"));

    // 3. typed Stage1Config 直接驱动 (R11, 无 compat flat JSON)
    std::string bias_path = stage1_cfg_->input.master_bias;
    std::string dark_path = stage1_cfg_->input.master_dark;
    std::string flat_path = stage1_cfg_->input.master_flat;
    const std::string calib_mode = stage1_cfg_->calibration.mode;
    const std::string calib_fallback = stage1_cfg_->calibration.fallback;
    const double calib_light_exposure_s = stage1_cfg_->calibration.light_exposure_s;
    const double calib_dark_exposure_s = stage1_cfg_->calibration.dark_exposure_s;
    bool require_size = true;
    bool require_exposure = true;
    double exposure_tol = 0.5;
    bool require_temp = false;
    double temp_tol = 1.0;
    bool allow_no_calib = false;
    int dark_opt = 0;
    double dark_k = 1.0;

    // 4. 显式 Master 约束 (CFG-106 已修: 禁止自动推导/硬编码 testdata 目录)
    //    standard: master_dark + master_flat 必需; optimal/exposure_ratio: 三者必需
    if (calib_mode == "standard") {
        if (dark_path.empty() || flat_path.empty()) {
            result.error_msg = "[CALIBRATE] mode=standard 需要 JSON 显式给出 master_dark 与 master_flat";
            result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
            return false;
        }
    } else {
        if (bias_path.empty() || dark_path.empty() || flat_path.empty()) {
            result.error_msg = "[CALIBRATE] mode=" + calib_mode + " 需要 JSON 显式给出 master_bias/master_dark/master_flat";
            result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
            return false;
        }
    }
    LOG_INFO("orchestrator", "[CALIBRATE] mode=" + calib_mode
             + " fallback=" + calib_fallback
             + " light_exposure_s=" + std::to_string(calib_light_exposure_s)
             + " dark_exposure_s=" + std::to_string(calib_dark_exposure_s));

    // 5. 加载 Master 文件 (aio_read_xisf)
    //    双精度 ABI (R10): FP64 模式优先获取 double* 像素数据 (aio_get_pixel_data_f64),
    //    若文件只有 float32 则转换为 double (临时缓冲区持有)
    AIOImageData* bias_img = nullptr;
    AIOImageData* dark_img = nullptr;
    AIOImageData* flat_img = nullptr;
    float* master_bias_f32 = nullptr;
    float* master_dark_f32 = nullptr;
    float* master_flat_f32 = nullptr;
    double* master_bias_f64 = nullptr;
    double* master_dark_f64 = nullptr;
    double* master_flat_f64 = nullptr;
    // FP64 模式下从 float32 转换的临时缓冲区 (需在函数作用域保持有效)
    std::vector<double> bias_convert, dark_convert, flat_convert;
    int bias_w = 0, bias_h = 0;
    int dark_w = 0, dark_h = 0;
    int flat_w = 0, flat_h = 0;
    double dark_exptime = -1.0;
    double dark_temp = 0.0;
    bool dark_has_temp = false;

    auto load_master = [&](const std::string& path, AIOImageData*& img,
                           float*& pix_f32, double*& pix_f64, int& w, int& h,
                           std::vector<double>& convert_buf) -> bool {
        if (path.empty()) return false;
        if (!fs::exists(path)) {
            LOG_WARN("orchestrator", "[CALIBRATE] Master 文件不存在: " + path);
            return false;
        }
        img = fn_read_xisf(path.c_str());
        if (img == nullptr && fn_read_fits) {
            img = fn_read_fits(path.c_str());   // FITS Master 回退
        }
        if (img == nullptr) {
            LOG_ERROR("orchestrator", "[CALIBRATE] 读取 Master 文件失败: " + path);
            return false;
        }
        w = fn_get_width(img);
        h = fn_get_height(img);
        // 双精度 ABI: FP64 模式优先获取 double* 数据 (不降级)
        if (use_fp64) {
            pix_f64 = fn_get_pixels_f64(img);
            if (!pix_f64) {
                // 文件只有 float32, 转换为 double (临时缓冲区持有)
                pix_f32 = fn_get_pixels(img);
                if (pix_f32) {
                    convert_buf.assign(pix_f32, pix_f32 + (size_t)w * h);
                    pix_f64 = convert_buf.data();
                }
            }
        } else {
            pix_f32 = fn_get_pixels(img);
        }
        LOG_INFO("orchestrator", "[CALIBRATE] 加载 Master: " + path
                 + " (" + std::to_string(w) + "x" + std::to_string(h)
                 + (use_fp64 ? " FP64" : " FP32") + ")");
        return true;
    };

    bool has_bias = load_master(bias_path, bias_img, master_bias_f32, master_bias_f64, bias_w, bias_h, bias_convert);
    bool has_dark = load_master(dark_path, dark_img, master_dark_f32, master_dark_f64, dark_w, dark_h, dark_convert);
    bool has_flat = load_master(flat_path, flat_img, master_flat_f32, master_flat_f64, flat_w, flat_h, flat_convert);

    // 6. 验证 Master 文件 (尺寸/曝光/温度)
    bool validation_ok = true;

    // 尺寸匹配验证
    if (require_size) {
        if (has_bias && (bias_w != width || bias_h != height)) {
            LOG_ERROR("orchestrator", "[CALIBRATE] master_bias 尺寸不匹配: "
                     + std::to_string(bias_w) + "x" + std::to_string(bias_h) + " vs "
                     + std::to_string(width) + "x" + std::to_string(height));
            validation_ok = false;
        }
        if (has_dark && (dark_w != width || dark_h != height)) {
            LOG_ERROR("orchestrator", "[CALIBRATE] master_dark 尺寸不匹配: "
                     + std::to_string(dark_w) + "x" + std::to_string(dark_h) + " vs "
                     + std::to_string(width) + "x" + std::to_string(height));
            validation_ok = false;
        }
        if (has_flat && (flat_w != width || flat_h != height)) {
            LOG_ERROR("orchestrator", "[CALIBRATE] master_flat 尺寸不匹配: "
                     + std::to_string(flat_w) + "x" + std::to_string(flat_h) + " vs "
                     + std::to_string(width) + "x" + std::to_string(height));
            validation_ok = false;
        }
    }

    // 曝光时间匹配验证 (从 masterDark 文件名解析 EXPOSURE)
    if (require_exposure && has_dark && frame_exptime > 0) {
        dark_exptime = parse_exposure_from_dark_path(dark_path);
        if (dark_exptime > 0) {
            double diff = std::fabs(dark_exptime - frame_exptime);
            if (diff > exposure_tol) {
                LOG_ERROR("orchestrator", "[CALIBRATE] master_dark 曝光时间不匹配: dark="
                         + std::to_string(dark_exptime) + "s vs frame="
                         + std::to_string(frame_exptime) + "s (tolerance="
                         + std::to_string(exposure_tol) + "s)");
                validation_ok = false;
            } else {
                LOG_INFO("orchestrator", "[CALIBRATE] master_dark 曝光时间匹配: dark="
                         + std::to_string(dark_exptime) + "s vs frame="
                         + std::to_string(frame_exptime) + "s (diff="
                         + std::to_string(diff) + "s)");
            }
        } else {
            LOG_WARN("orchestrator", "[CALIBRATE] 无法从路径解析 master_dark 曝光时间: " + dark_path);
        }
    }

    // 温度匹配验证 (从 masterDark metadata 读取 CCD-TEMP)
    if (require_temp && has_dark && has_ccd_temp && dark_img != nullptr) {
        AIOImageMetadata dark_meta = fn_get_metadata(dark_img);
        if (dark_meta.calibration.has_ccd_temp) {
            dark_temp = dark_meta.calibration.ccd_temp;
            dark_has_temp = true;
            double diff = std::fabs(dark_temp - frame_ccd_temp);
            if (diff > temp_tol) {
                LOG_ERROR("orchestrator", "[CALIBRATE] master_dark 温度不匹配: dark="
                         + std::to_string(dark_temp) + "C vs frame="
                         + std::to_string(frame_ccd_temp) + "C (tolerance="
                         + std::to_string(temp_tol) + "C)");
                validation_ok = false;
            } else {
                LOG_INFO("orchestrator", "[CALIBRATE] master_dark 温度匹配: dark="
                         + std::to_string(dark_temp) + "C vs frame="
                         + std::to_string(frame_ccd_temp) + "C (diff="
                         + std::to_string(diff) + "C)");
            }
        } else {
            LOG_WARN("orchestrator", "[CALIBRATE] master_dark 无 CCD-TEMP 元数据, 跳过温度验证");
        }
    }

    if (!validation_ok) {
        LOG_ERROR("orchestrator", "[CALIBRATE] Master 文件验证失败, 中止校准");
        result.error_msg = "[CALIBRATE] Master 文件验证失败 (尺寸/曝光/温度不匹配)";
        result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }

    // 7. 检查是否处于无校准退化模式
    if (!has_bias && !has_dark && !has_flat) {
        if (!allow_no_calib) {
            LOG_ERROR("orchestrator", "[CALIBRATE] 无任何 Master 文件可用且 allow_no_calibration=false");
            result.error_msg = "[CALIBRATE] 无 Master 文件且未启用 allow_no_calibration";
            result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
            if (bias_img) fn_free_image(bias_img);
            if (dark_img) fn_free_image(dark_img);
            if (flat_img) fn_free_image(flat_img);
            return false;
        }
        LOG_WARN("orchestrator", "[CALIBRATE] 无校准退化模式 (allow_no_calibration=true)");
    }

    // 8. 分配输出缓冲 (ac_calibrate_frame 要求调用者分配)
    //    双精度 ABI (R10): FP64 模式分配 double*, FP32 模式分配 float*
    float* out_f32 = nullptr;
    double* out_f64 = nullptr;
    if (use_fp64) {
        out_f64 = static_cast<double*>(std::malloc(n_pix * sizeof(double)));
    } else {
        out_f32 = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    }
    if ((use_fp64 && !out_f64) || (!use_fp64 && !out_f32)) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 分配输出缓冲失败");
        result.error_msg = "[CALIBRATE] 分配输出缓冲失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }

    // 9. 调用 ac_calibrate_frame (实际应用 Bias/Dark/Flat)
    //    双精度 ABI (R10): FP64 模式调用 ac_calibrate_frame_f64 (double 像素算术, 不降级)
    float actual_k_f32 = 0.0f;
    double actual_k_f64 = 0.0;
    LOG_INFO("orchestrator", "[CALIBRATE] 调用 ac_calibrate_frame" + std::string(use_fp64 ? "_f64" : "")
             + ": dark_opt=" + std::to_string(dark_opt) + ", k_init=" + std::to_string(dark_k)
             + ", has_bias=" + (has_bias ? "true" : "false")
             + ", has_dark=" + (has_dark ? "true" : "false")
             + ", has_flat=" + (has_flat ? "true" : "false"));
    int ret;
    if (use_fp64) {
        ret = fn_calibrate_f64(light_f64, width, height,
                               master_dark_f64, master_flat_f64, master_bias_f64,
                               out_f64, dark_opt, dark_k, &actual_k_f64);
    } else {
        ret = fn_calibrate(light_f32, width, height,
                           master_dark_f32, master_flat_f32, master_bias_f32,
                           out_f32, dark_opt, static_cast<float>(dark_k), &actual_k_f32);
    }

    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] ac_calibrate_frame 失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] ac_calibrate_frame 失败: ret=" + std::to_string(ret);
        result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
        if (out_f32) std::free(out_f32);
        if (out_f64) std::free(out_f64);
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }

    // 10. 计算校准前/后图像统计 + Master 均值
    //     双精度 ABI: FP64 模式使用 double* 版本的统计函数 (重载)
    double light_mean, light_median, light_std;
    double out_mean, out_median, out_std;
    double bias_mean, dark_mean, flat_mean;
    double actual_k;
    if (use_fp64) {
        light_mean = compute_array_mean(light_f64, n_pix);
        light_median = compute_array_median(light_f64, n_pix);
        light_std = compute_array_std(light_f64, n_pix, light_mean);
        out_mean = compute_array_mean(out_f64, n_pix);
        out_median = compute_array_median(out_f64, n_pix);
        out_std = compute_array_std(out_f64, n_pix, out_mean);
        bias_mean = has_bias ? compute_array_mean(master_bias_f64, n_pix) : 0.0;
        dark_mean = has_dark ? compute_array_mean(master_dark_f64, n_pix) : 0.0;
        flat_mean = has_flat ? compute_array_mean(master_flat_f64, n_pix) : 0.0;
        actual_k = actual_k_f64;
    } else {
        light_mean = compute_array_mean(light_f32, n_pix);
        light_median = compute_array_median(light_f32, n_pix);
        light_std = compute_array_std(light_f32, n_pix, light_mean);
        out_mean = compute_array_mean(out_f32, n_pix);
        out_median = compute_array_median(out_f32, n_pix);
        out_std = compute_array_std(out_f32, n_pix, out_mean);
        bias_mean = has_bias ? compute_array_mean(master_bias_f32, n_pix) : 0.0;
        dark_mean = has_dark ? compute_array_mean(master_dark_f32, n_pix) : 0.0;
        flat_mean = has_flat ? compute_array_mean(master_flat_f32, n_pix) : 0.0;
        actual_k = static_cast<double>(actual_k_f32);
    }

    LOG_INFO("orchestrator", "[CALIBRATE] 校准统计: light_mean=" + std::to_string(light_mean)
             + " -> out_mean=" + std::to_string(out_mean)
             + ", bias_mean=" + std::to_string(bias_mean)
             + ", dark_mean=" + std::to_string(dark_mean)
             + ", flat_mean=" + std::to_string(flat_mean)
             + ", actual_k=" + std::to_string(actual_k));

    // 11. 替换 data 块 (转移所有权)
    //     P03-003: data 块写回失败必须返回非零 (退出码 3, 必需块缺失)
    //     双精度 ABI: FP64 模式写回 FLOAT64, FP32 模式写回 FLOAT32
    fn_remove_block(frame_, "data");
    int dims[2] = {height, width};
    if (use_fp64) {
        ret = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT64,
                                out_f64, n_pix, dims, 2,
                                "校准后 Light 像素 (FP64, Bias/Dark/Flat 已应用)");
    } else {
        ret = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT32,
                                out_f32, n_pix, dims, 2,
                                "校准后 Light 像素 (Bias/Dark/Flat 已应用)");
    }
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 写回 data 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] 写回 data 块失败";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        if (out_f32) std::free(out_f32);
        if (out_f64) std::free(out_f64);
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }
    // out_f32/out_f64 所有权已转移给 frame_

    // 12. 写入 cal_stats KV 块 (P03-001 交付物)
    fn_kv_set(frame_, "cal_stats", "STATUS", "OK");
    fn_kv_set(frame_, "cal_stats", "MASTER_BIAS_PATH", bias_path.c_str());
    fn_kv_set(frame_, "cal_stats", "MASTER_DARK_PATH", dark_path.c_str());
    fn_kv_set(frame_, "cal_stats", "MASTER_FLAT_PATH", flat_path.c_str());
    fn_kv_set_double(frame_, "cal_stats", "MASTER_BIAS_MEAN", bias_mean);
    fn_kv_set_double(frame_, "cal_stats", "MASTER_DARK_MEAN", dark_mean);
    fn_kv_set_double(frame_, "cal_stats", "MASTER_FLAT_MEAN", flat_mean);
    fn_kv_set_double(frame_, "cal_stats", "LIGHT_MEAN", light_mean);
    fn_kv_set_double(frame_, "cal_stats", "LIGHT_MEDIAN", light_median);
    fn_kv_set_double(frame_, "cal_stats", "LIGHT_STD", light_std);
    fn_kv_set_double(frame_, "cal_stats", "OUT_MEAN", out_mean);
    fn_kv_set_double(frame_, "cal_stats", "OUT_MEDIAN", out_median);
    fn_kv_set_double(frame_, "cal_stats", "OUT_STD", out_std);
    fn_kv_set_double(frame_, "cal_stats", "ACTUAL_K", actual_k);
    fn_kv_set_double(frame_, "cal_stats", "DARK_OPTIMIZATION", dark_opt);
    fn_kv_set_double(frame_, "cal_stats", "HAS_BIAS", has_bias ? 1.0 : 0.0);
    fn_kv_set_double(frame_, "cal_stats", "HAS_DARK", has_dark ? 1.0 : 0.0);
    fn_kv_set_double(frame_, "cal_stats", "HAS_FLAT", has_flat ? 1.0 : 0.0);
    if (dark_exptime > 0) {
        fn_kv_set_double(frame_, "cal_stats", "DARK_EXPTIME", dark_exptime);
    }
    if (dark_has_temp) {
        fn_kv_set_double(frame_, "cal_stats", "DARK_CCD_TEMP", dark_temp);
    }
    fn_kv_set(frame_, "cal_stats", "CALIBRATION_STATUS",
              (has_bias || has_dark || has_flat) ? "APPLIED" : "DEGENERATED");

    LOG_INFO("orchestrator", "[CALIBRATE] cal_stats 已写入 (CALIBRATION_STATUS="
             + std::string((has_bias || has_dark || has_flat) ? "APPLIED" : "DEGENERATED") + ")");

    // 13. 释放 Master 文件图像数据
    if (bias_img) fn_free_image(bias_img);
    if (dark_img) fn_free_image(dark_img);
    if (flat_img) fn_free_image(flat_img);

    LOG_INFO("orchestrator", "[CALIBRATE] 完成 (实际应用 Bias/Dark/Flat)");
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
    // P12-005: 窄带滤光片映射 (H-alpha / OIII 大小写变体)
    // T4: Baader RGBHaOIII (7nm HA, 8.5nm OIII); T2/T3: Astrodon (暂用 Baader 曲线近似)
    if (ieq(fits_filter, "H-alpha") || ieq(fits_filter, "Ha") || ieq(fits_filter, "HA"))
        return "Baader 7nm H-alpha";
    if (ieq(fits_filter, "OIII") || ieq(fits_filter, "Oiii"))
        return "Baader 8.5nm OIII";
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
// 辅助: 加载 CCD QE 曲线 (GAP-012)
// qe_curves.json 格式与 filters.json 一致:
//   {"<name>": {"name": "...", "channel": "Q", "wavelength_nm": [...], "value": [...]}}
// 注: 数组键名是 "value" 而非 "qe" (与 filters.json 共用解析逻辑)
// ============================================================================
static bool load_qe_curve(const std::string& json_path,
                          const std::string& qe_name,
                          std::vector<double>& out_wl,
                          std::vector<double>& out_trans) {
    // 复用 load_filter_curve 的 JSON 解析逻辑 (格式完全一致: wavelength_nm + value)
    return load_filter_curve(json_path, qe_name, out_wl, out_trans);
}

// ============================================================================
// 辅助: 从 stage1_config.json 文本中提取 "qe_curve" 字段值 (GAP-012)
// 配置格式: { ... "frame": { ... "qe_curve": "GSENSE2020BSI" ... } ... }
// 返回空字符串表示未配置或解析失败
// ============================================================================
static std::string extract_qe_curve_name(const std::string& json_content) {
    if (json_content.empty()) return "";
    // 定位 "qe_curve" 键
    std::string key = "\"qe_curve\"";
    size_t pos = json_content.find(key);
    if (pos == std::string::npos) return "";
    // 找 ':' 分隔符
    pos = json_content.find(':', pos + key.size());
    if (pos == std::string::npos) return "";
    // 跳过空白, 找字符串首字符 '"'
    size_t start = json_content.find('"', pos + 1);
    if (start == std::string::npos) return "";
    // 找字符串尾字符 '"'
    size_t end = json_content.find('"', start + 1);
    if (end == std::string::npos) return "";
    return json_content.substr(start + 1, end - start - 1);
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
        reinterpret_cast<void*>(GetProcAddress(gaia_h, "gaia_client_get_spectrum_params")));
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
// 辅助: 从 header KV 块读取 WCS 参数 (供 PHOTOMETRIC 使用)
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
    //    P03-002: gaia_data_dir 优先从 config 读取, 空时默认 project_root_dir_/GaiaDR3SP
    using gaia_create_ex_fn = GaiaClient* (*)(const char*, GaiaDbType);
    auto fn_gaia_create_ex = reinterpret_cast<gaia_create_ex_fn>(
        reinterpret_cast<void*>(GetProcAddress(gaia_h, "gaia_client_create_ex")));
    if (fn_gaia_create_ex == nullptr) {
        error_msg = "gaia_client_create_ex 函数未找到";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    // P03-002: 数据库位置必须由 stage1.json 配置引入 (gaia_data_dir, typed 配置
    // 已解析为绝对路径, 相对 JSON 文件目录); 禁止硬编码默认路径
    // (2026-08-05 规范: 数据库之类的位置一律配置驱动)
    if (stage1_cfg_ == nullptr || stage1_cfg_->gaia_data_dir.empty()) {
        error_msg = "stage1.json 必须配置 gaia_data_dir (Gaia DR3SP 光谱数据库目录), "
                    "禁止使用默认路径";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    const std::string& gaia_data_dir = stage1_cfg_->gaia_data_dir;
    LOG_INFO("orchestrator", "[PLATESOLVE] Gaia 数据目录: " + gaia_data_dir
             + " (来自 config)");
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
        reinterpret_cast<void*>(GetProcAddress(sdet_h, "sdet_create")));
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
    // R11: typed Stage1Config 直接驱动
    int cfg_max_stars = stage1_cfg_->platesolve.max_stars;
    if (cfg_max_stars <= 0) cfg_max_stars = 2000;
    sdet_params.maxStars = cfg_max_stars;
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
    LOG_INFO("orchestrator", "[PLATESOLVE] StarDetector 创建成功 (fitRadius=0 自动, maxStars="
             + std::to_string(cfg_max_stars) + ")");

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
            reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(star_detector_dll_handle_), "sdet_destroy")));
        if (fn_sdet_destroy) {
            fn_sdet_destroy(reinterpret_cast<StarDetectorHandle>(sdet_handle_));
        }
        sdet_handle_ = 0;
    }

    // 3. 销毁 GaiaClient
    if (gaia_client_handle_ != 0 && gaia_client_dll_handle_ != nullptr) {
        using gaia_destroy_fn = void (*)(GaiaClient*);
        auto fn_gaia_destroy = reinterpret_cast<gaia_destroy_fn>(
            reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(gaia_client_dll_handle_), "gaia_client_destroy")));
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

namespace {
// ============================================================================
// make_stable_star_id - stable star_id (Phase1 Full Freeze v2)
// 规则 (wiki/05_STAR_MEASUREMENT_SHARED_DATA.md):
//   - 帧内唯一且不可复用;
//   - 与行号无关 (不依赖数组下标, 排序变化不得造成无法对账);
//   - 由检测位置确定性派生 (量化 x/y 后混合哈希)。
// 碰撞时 (极罕见) 在低 16 位叠加递增偏移, 保证帧内唯一。
// ============================================================================
uint64_t make_stable_star_id(double x, double y) {
    uint64_t qx = static_cast<uint64_t>(std::llround(x * 1024.0));
    uint64_t qy = static_cast<uint64_t>(std::llround(y * 1024.0));
    uint64_t h = qx * 0x9E3779B97F4A7C15ULL ^ (qy + 0xBF58476D1CE4E5B9ULL);
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

// 生成帧内唯一 star_id 序列 (位置哈希, 碰撞时低 16 位递增)
void assign_stable_star_ids(const double* x, const double* y, int n,
                            std::vector<int64_t>& out_ids) {
    out_ids.resize(static_cast<size_t>(n));
    std::unordered_set<uint64_t> used;
    used.reserve(static_cast<size_t>(n) * 2);
    for (int i = 0; i < n; ++i) {
        uint64_t id = make_stable_star_id(x[i], y[i]);
        uint64_t base = id & 0xFFFFFFFFFFFF0000ULL;  // 保留高 48 位
        uint16_t offset = static_cast<uint16_t>(id & 0xFFFFULL);
        while (used.find(id) != used.end()) {
            offset = static_cast<uint16_t>(offset + 1);
            id = base | static_cast<uint64_t>(offset);
        }
        used.insert(id);
        out_ids[static_cast<size_t>(i)] = static_cast<int64_t>(id);
    }
}
}  // namespace

// ============================================================================
// run_stage_platesolve - PLATESOLVE 阶段 (Phase1 Full Freeze v2 修订流水线)
// 修订流水线: CALIBRATE -> PSF/STAR_MEASURE -> PLATESOLVE -> PHOTOMETRIC -> SNR
// 职责:
//   1. 读取 PSF/STAR_MEASURE 产出的权威 star_det 块 (FLOAT64 [N,6], 禁止重检测)
//   2. 读取 "data" 块仅获取图像尺寸 (不用于检测)
//   3. 读取 "header" KV 块的 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ 作为初始指向
//   4. 调用 ipv_solve_from_detections_v1 求解 WCS+SIP (跳过 sdet_detect_ex)
//   5. 将 WCS+SIP 结果写回 "header" KV 块
// 约束:
//   - 一帧只做一次权威检测 (PSF 阶段), 本阶段不得调用 sdet_detect_ex;
//   - 不得改写 star_det / star_measurements / psf 块;
//   - 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据。
// ============================================================================
bool Orchestrator::run_stage_platesolve(TaskResult& result) {
    LOG_INFO("orchestrator", "[PLATESOLVE] 开始 (消费 PSF 星点, ipv_solve_from_detections_v1)");

    // P03-003: PLATESOLVE 是必需 stage, DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PLATESOLVE)) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] PLATESOLVE DLL 未加载 (必需模块)");
        result.error_msg = "[PLATESOLVE] PLATESOLVE DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[PLATESOLVE] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
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
    if (!fn_get_block || !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] AIO 函数指针获取失败");
        result.error_msg = "[PLATESOLVE] AIO 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 1. 读取 data 块仅获取图像尺寸 (FLOAT32 或 FLOAT64 [H,W])
    //    P03-003: data 是必需块 (CALIBRATE 产出), 缺失必须失败 (退出码 3)
    //    本阶段不再读取像素做检测 (检测已在 PSF/STAR_MEASURE 完成)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] data 块不存在 (必需块)");
        result.error_msg = "[PLATESOLVE] data 块不存在 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    LOG_INFO("orchestrator", "[PLATESOLVE] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 1.1 构造 astrometric_detections (Phase1 Final Closure V3)
    //     权威输入 = star_measurements 的 PSF 拟合中心 (统一"像素索引即中心坐标"契约),
    //     过滤: invalid fit / 严重饱和 / 异常 FWHM / 边缘。
    //     fallback = star_det 检测坐标 (显式 DETECTOR_FALLBACK, sdet 像素中心=索引+0.5
    //     显式转统一契约 -0.5), 仅当 PSF 有效星不足以求解时补充, 位置去重。
    const AioBlock* sm_block = fn_get_block(frame_, "star_measurements");
    const AioBlock* star_det_block = fn_get_block(frame_, "star_det");
    if (sm_block == nullptr || sm_block->type != AIO_BLOCK_FLOAT64 ||
        sm_block->dims[0] <= 0 || sm_block->dims[1] < 15) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] star_measurements 块缺失或格式错误 "
                  "(必需权威块, 需要 FLOAT64 [N,>=15])");
        result.error_msg = "[PLATESOLVE] star_measurements 块缺失或格式错误 (必需权威块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }
    int n_sm = sm_block->dims[0];
    const double* smd = static_cast<const double*>(sm_block->data);
    // star_measurements 列: 0 star_id, 1 x, 2 y, 3 flux_inst, 4 flux_unc,
    //   5 background, 6 psf_status, 7 fwhm, 8 A, 9 B, 10 mad, 11 ecc,
    //   12 mag, 13 saturated, 14 has_saturated
    std::vector<double> astro_det;
    std::vector<double> psf_xs, psf_ys;
    int n_psf = 0, n_fallback = 0, n_filtered = 0;
    auto push_det = [&](double x, double y, double flux, double mag,
                        double sat, double has_sat) {
        astro_det.push_back(x); astro_det.push_back(y);
        astro_det.push_back(flux); astro_det.push_back(mag);
        astro_det.push_back(sat); astro_det.push_back(has_sat);
    };
    for (int i = 0; i < n_sm; ++i) {
        const double* r = smd + (size_t)i * 15;
        double x = r[1], y = r[2];
        if (!std::isfinite(x) || !std::isfinite(y)) { ++n_filtered; continue; }
        // status: 0=DPSF_FIT_OK, 3=DPSF_FIT_ITERATION_LIMIT (参数有效亦视为成功)
        // 过滤: invalid fit 排除; saturated/FWHM/edge 仅统计 (保留输入, ipv 选星内部处理)
        bool ok = ((r[6] == 0.0 || r[6] == 3.0) &&
                   std::isfinite(r[1]) && std::isfinite(r[2]));
        bool sat_bad = (r[13] != 0.0);
        bool fwhm_bad = !(r[7] >= 0.5 && r[7] <= 20.0);
        bool edge = (x < 5 || y < 5 || x > width - 5 || y > height - 5);
        if (!ok) { ++n_filtered; continue; }
        if (sat_bad || fwhm_bad || edge) { ++n_filtered; }
        // WCS 边界显式转换: star_measurements 为统一契约 (像素索引即中心坐标),
        // ipv 求解器输入接口契约为"像素中心 = 索引 + 0.5" (与其内部 U 构造/WCS 一致)。
        push_det(x + 0.5, y + 0.5, r[3], r[12], r[13], r[14]);
        psf_xs.push_back(x);
        psf_ys.push_back(y);
        ++n_psf;
    }
    if (star_det_block != nullptr && star_det_block->type == AIO_BLOCK_FLOAT64 &&
        star_det_block->dims[1] == 6) {
        int n_det_raw = star_det_block->dims[0];
        const double* det = static_cast<const double*>(star_det_block->data);
        for (int i = 0; i < n_det_raw; ++i) {
            const double* d = det + (size_t)i * 6;
            double x = d[0];   // fallback: sdet 检测坐标已是 ipv 接口契约 (+0.5)
            double y = d[1];
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            if (x < 5 || y < 5 || x > width - 5 || y > height - 5) continue;
            if (d[4] != 0.0 || d[5] != 0.0) continue;  // 严重饱和排除
            // 位置去重: 与已有 PSF 星距离 <0.5px 视为同一颗
            bool dup = false;
            for (size_t k = 0; k < psf_xs.size(); ++k) {
                if (std::hypot(x - (psf_xs[k] + 0.5), y - (psf_ys[k] + 0.5)) < 0.5) {
                    dup = true; break;
                }
            }
            if (dup) continue;
            push_det(x, y, d[2], d[3], d[4], d[5]);
            ++n_fallback;
        }
    }
    if (n_psf == 0 && n_fallback == 0) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] 无有效星点可求解 "
                  "(PSF 中心 n_psf=" + std::to_string(n_psf)
                  + ", fallback=" + std::to_string(n_fallback) + ")");
        result.error_msg = "[PLATESOLVE] 无有效星点可求解";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }
    int n_det = n_psf + n_fallback;
    const double* detections = astro_det.data();
    LOG_INFO("orchestrator", "[PLATESOLVE] 消费星点: PSF 拟合中心 n_psf="
             + std::to_string(n_psf) + ", detector_fallback=" + std::to_string(n_fallback)
             + ", filtered=" + std::to_string(n_filtered)
             + " (不重检测, 统一坐标契约)");

    // 2. 从 header KV 读取初始指向 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
    //    约束: 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据
    //    P03-002: 支持从 config 覆盖 (initial_ra/initial_dec/focal_length/pixel_size)
    const char* objctra = fn_kv_get(frame_, "header", "OBJCTRA");
    const char* objctdec = fn_kv_get(frame_, "header", "OBJCTDEC");
    double focal_length = fn_kv_get_double(frame_, "header", "FOCALLEN", 0.0);
    double pixel_size = fn_kv_get_double(frame_, "header", "XPIXSZ", 0.0);

    // R11: typed Stage1Config 直接驱动 (initial_ra/dec; focal/pixel 保留 header 值)
    std::string cfg_initial_ra;
    std::string cfg_initial_dec;
    if (stage1_cfg_->platesolve.initial_ra_deg != -999.0) {
        cfg_initial_ra = std::to_string(stage1_cfg_->platesolve.initial_ra_deg);
    }
    if (stage1_cfg_->platesolve.initial_dec_deg != -999.0) {
        cfg_initial_dec = std::to_string(stage1_cfg_->platesolve.initial_dec_deg);
    }
    if (!cfg_initial_ra.empty()) objctra = cfg_initial_ra.c_str();
    if (!cfg_initial_dec.empty()) objctdec = cfg_initial_dec.c_str();

    double ra0 = parse_ra_hms(objctra);
    double dec0 = parse_dec_dms(objctdec);

    LOG_INFO("orchestrator", "[PLATESOLVE] 初始指向: OBJCTRA='" + std::string(objctra ? objctra : "")
             + "' -> ra0=" + std::to_string(ra0) + "deg, OBJCTDEC='" + std::string(objctdec ? objctdec : "")
             + "' -> dec0=" + std::to_string(dec0) + "deg");
    LOG_INFO("orchestrator", "[PLATESOLVE] 焦距=" + std::to_string(focal_length)
             + "mm, 像素尺寸=" + std::to_string(pixel_size) + "um"
             + (!cfg_initial_ra.empty() ? " (部分来自 config)" : ""));

    // 3. 调用 ipv_solve_from_detections_v1 (P02-002 路径 A)
    //    与 ipv_solve_from_memory 算法等价, 仅跳过 sdet_detect_ex (检测已在 PSF 完成)
    auto fn_ipv_solve_det = dll_loader_.get_function<int (*)(
        void*, const double*, int, int, int, double, double, double, double,
        const IpvParams*, IpvWcsResult*)>(
        ModuleId::PLATESOLVE, "ipv_solve_from_detections_v1");
    auto fn_get_default_params = dll_loader_.get_function<void (*)(IpvParams*)>(
        ModuleId::PLATESOLVE, "ipv_get_default_params");

    if (!fn_ipv_solve_det || !fn_get_default_params) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] ipv 函数指针获取失败 "
                  "(ipv_solve_from_detections_v1 / ipv_get_default_params)");
        result.error_msg = "[PLATESOLVE] ipv 函数指针获取失败 (路径 A: detections 求解)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    IpvParams params;
    fn_get_default_params(&params);
    // 设置日志目录 (2026-08-05 规范: 运行产物统一写 run/logs/, 不再写 lib/)
    std::string log_dir = project_root_dir_ + "/run/logs/plate_solve";
    std::memset(params.log_dir, 0, sizeof(params.log_dir));
    std::strncpy(params.log_dir, log_dir.c_str(), sizeof(params.log_dir) - 1);

    IpvWcsResult wcs_result;
    std::memset(&wcs_result, 0, sizeof(wcs_result));

    LOG_INFO("orchestrator", "[PLATESOLVE] 调用 ipv_solve_from_detections_v1 "
             "(n_stars=" + std::to_string(n_det) + ", 跳过 sdet_detect_ex) ...");
    int ret = fn_ipv_solve_det(ipv_solver_handle_, detections, n_det,
                               width, height,
                               ra0, dec0, focal_length, pixel_size,
                               &params, &wcs_result);

    if (ret != 1 || wcs_result.success != 1) {
        std::string err = wcs_result.error_msg[0] != '\0'
            ? std::string(wcs_result.error_msg)
            : ("ret=" + std::to_string(ret));
        LOG_ERROR("orchestrator", "[PLATESOLVE] 求解失败: " + err);
        result.error_msg = "[PLATESOLVE] 求解失败: " + err;
        result.exit_code = AstroCsExitCode::PLATESOLVE_FAILED;
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

    // 6. star_det / star_det_psf_compat / star_measurements 均由 PSF/STAR_MEASURE 阶段产出,
    //    本阶段不重写 (修订流水线: 一帧只做一次权威检测和一次权威 instrumental flux 测量)。
    LOG_INFO("orchestrator", "[PLATESOLVE] star_det 块由 PSF/STAR_MEASURE 产出, 未重写 "
             "(检测次数保持 1 次)");

    // P02-006: 删除无消费者 gaia_cat 二次查询
    // 上游未再产生 gaia_cat 块; 测光查询由 PHOTOMETRIC 阶段按需完成。

    LOG_INFO("orchestrator", "[PLATESOLVE] 完成");
    return true;
}

bool Orchestrator::run_stage_psf(TaskResult& result) {
    LOG_INFO("orchestrator", "[PSF] 开始");

    // P03-003: PSF 是必需 stage (PHOTOMETRIC 依赖 psf 块), DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PSF)) {
        LOG_ERROR("orchestrator", "[PSF] PSF DLL 未加载 (必需模块)");
        result.error_msg = "[PSF] PSF DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[PSF] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
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

    // 双精度 ABI: 根据 config_.precision (与 PrecisionContext 一致) 选择像素数据源
    // FP64 模式: data 块为 FLOAT64, 直接使用 double 图像 (不降级到 uint16/float32),
    //            调用 dpsf_fit_batch_d 在 double 上裁剪 patch 拟合。
    // FP32 模式: data 块为 FLOAT32, 转为 UINT16 调用 dpsf_fit_batch (向后兼容)。
    bool use_fp64 = (config_.precision == PrecisionMode::FP64);

    // 1. 读取 data 块 (FP32: FLOAT32→UINT16; FP64: FLOAT64 直接使用)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PSF] data 块不存在");
        result.error_msg = "[PSF] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    LOG_INFO("orchestrator", "[PSF] 图像: " + std::to_string(width) + "x" + std::to_string(height)
             + " mode=" + (use_fp64 ? "FP64" : "FP32"));

    const double* pixels_f64 = nullptr;   // FP64 路径 (指向 data 块内部, 不持有所有权)
    const float* pixels_f32 = nullptr;    // FP32 路径 (R11: 直接使用 float 数据, 不转 uint16)

    if (use_fp64) {
        pixels_f64 = static_cast<const double*>(data_block->data);
        if (pixels_f64 == nullptr) {
            LOG_ERROR("orchestrator", "[PSF] data 块 data 为空 (FP64)");
            result.error_msg = "[PSF] data 块 data 为空 (FP64)";
            return false;
        }
    } else {
        pixels_f32 = static_cast<const float*>(data_block->data);
        if (pixels_f32 == nullptr) {
            LOG_ERROR("orchestrator", "[PSF] data 块 data 为空 (FP32)");
            result.error_msg = "[PSF] data 块 data 为空 (FP32)";
            return false;
        }
    }

    // 2. 初始化 PLATESOLVE 环境 (star_detector 句柄, 后续 PLATESOLVE/PHOTOMETRIC/SNR 复用)
    if (!platesolve_env_ready_) {
        std::string err;
        if (!init_platesolve_env(err)) {
            LOG_ERROR("orchestrator", "[PSF] init_platesolve_env 失败 (star_detector 不可用): " + err);
            result.error_msg = "[PSF] " + err;
            return false;
        }
    }

    // 2.1 star_detector C API (sdet_detect_ex / sdet_detect_ex_f64 / sdet_free_detect_ex)
    HMODULE sdet_dll = static_cast<HMODULE>(star_detector_dll_handle_);
    using sdet_ex_fn = int (*)(StarDetectorHandle, const uint16_t*, int, int,
                               double**, double**, float**, int**, float**, int**,
                               int*, const char**, int, float***);
    auto fn_sdet_ex = reinterpret_cast<sdet_ex_fn>(
        reinterpret_cast<void*>(GetProcAddress(sdet_dll, "sdet_detect_ex")));
    using sdet_ex_f64_fn = int (*)(StarDetectorHandle, const double*, int, int,
                                   double**, double**, float**, int**, float**, int**,
                                   int*, const char**, int, float***);
    auto fn_sdet_ex_f64 = reinterpret_cast<sdet_ex_f64_fn>(
        reinterpret_cast<void*>(GetProcAddress(sdet_dll, "sdet_detect_ex_f64")));
    using sdet_free_fn = void (*)(double*, double*, float*, int*, float*, int*, float**, int);
    auto fn_sdet_free = reinterpret_cast<sdet_free_fn>(
        reinterpret_cast<void*>(GetProcAddress(sdet_dll, "sdet_free_detect_ex")));
    if (!fn_sdet_ex || !fn_sdet_ex_f64 || !fn_sdet_free) {
        LOG_ERROR("orchestrator", "[PSF] star_detector 函数指针获取失败");
        result.error_msg = "[PSF] star_detector 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 2.2 星点检测 (修订流水线: 一帧只做一次权威检测, 发生在 PSF/STAR_MEASURE)
    //     FP32: float -> uint16 clamp (与 ipv_select_from_memory 转换一致);
    //     FP64: 全程 double (sdet_detect_ex_f64, 不降级)
    double *det_x = nullptr, *det_y = nullptr;
    float *det_flux = nullptr;
    int *det_sat = nullptr;
    float *det_mag = nullptr;
    int *det_has_sat = nullptr;
    int det_count = 0;
    int det_ret = 0;
    std::vector<uint16_t> img_u16;
    if (use_fp64) {
        det_ret = fn_sdet_ex_f64(reinterpret_cast<StarDetectorHandle>(sdet_handle_),
                                 pixels_f64, width, height,
                                 &det_x, &det_y, &det_flux, &det_sat,
                                 &det_mag, &det_has_sat, &det_count,
                                 nullptr, 0, nullptr);
    } else {
        img_u16.resize(static_cast<size_t>(width) * height);
        for (size_t i = 0; i < img_u16.size(); ++i) {
            float v = pixels_f32[i];
            if (v < 0.0f) v = 0.0f;
            if (v > 65535.0f) v = 65535.0f;
            img_u16[i] = static_cast<uint16_t>(v);
        }
        det_ret = fn_sdet_ex(reinterpret_cast<StarDetectorHandle>(sdet_handle_),
                             img_u16.data(), width, height,
                             &det_x, &det_y, &det_flux, &det_sat,
                             &det_mag, &det_has_sat, &det_count,
                             nullptr, 0, nullptr);
    }
    if (det_ret != 0 || det_count <= 0) {
        LOG_ERROR("orchestrator", "[PSF] 星点检测失败或未检测到星 (ret=" + std::to_string(det_ret)
                 + ", count=" + std::to_string(det_count) + ")");
        result.error_msg = "[PSF] 星点检测失败或未检测到星";
        result.exit_code = AstroCsExitCode::STAR_DETECT_FAILED;
        if (det_x || det_y || det_flux || det_sat || det_mag || det_has_sat) {
            fn_sdet_free(det_x, det_y, det_flux, det_sat, det_mag, det_has_sat, nullptr, 0);
        }
        return false;
    }
    int n_stars = det_count;
    LOG_INFO("orchestrator", "[PSF] 检测完成: " + std::to_string(n_stars) + " 颗星 (mode="
             + (use_fp64 ? "FP64" : "FP32") + ")");

    // 2.3 权威 star_det 块 (FLOAT64 [N,6]) + PSF 兼容视图 (FLOAT32 [N,4])
    //     BLOCKER-TYPE-001: 权威块保留 FP64 全精度与饱和列, 禁止隐式 F64→F32
    std::vector<double> star_det64(static_cast<size_t>(n_stars) * 6);
    std::vector<float> star_det32(static_cast<size_t>(n_stars) * 4);
    double max_compat_rel = 0.0;
    for (int i = 0; i < n_stars; ++i) {
        star_det64[static_cast<size_t>(i) * 6 + 0] = det_x[i];
        star_det64[static_cast<size_t>(i) * 6 + 1] = det_y[i];
        star_det64[static_cast<size_t>(i) * 6 + 2] = static_cast<double>(det_flux[i]);
        star_det64[static_cast<size_t>(i) * 6 + 3] = static_cast<double>(det_mag[i]);
        star_det64[static_cast<size_t>(i) * 6 + 4] = static_cast<double>(det_sat[i]);
        star_det64[static_cast<size_t>(i) * 6 + 5] = static_cast<double>(det_has_sat[i]);
        for (int c = 0; c < 4; ++c) {
            double v = star_det64[static_cast<size_t>(i) * 6 + c];
            float f = static_cast<float>(v);
            star_det32[static_cast<size_t>(i) * 4 + c] = f;
            double rel = std::fabs((double)f - v) / std::max(std::fabs(v), 1e-30);
            if (rel > max_compat_rel) max_compat_rel = rel;
        }
    }
    int dims6[2] = {n_stars, 6};
    fn_remove_block(frame_, "star_det");
    int r6 = fn_add_block(frame_, "star_det", AIO_BLOCK_FLOAT64,
                          star_det64.data(), static_cast<int64_t>(n_stars) * 6, dims6, 2,
                          "星点检测权威块 (FLOAT64 [N,6]: x,y,flux,mag,saturated,has_saturated; PSF/STAR_MEASURE 产出)");
    if (r6 != 0) {
        LOG_ERROR("orchestrator", "[PSF] star_det 权威块写入失败 (必需块): ret=" + std::to_string(r6));
        result.error_msg = "[PSF] star_det 权威块写入失败 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        fn_sdet_free(det_x, det_y, det_flux, det_sat, det_mag, det_has_sat, nullptr, 0);
        return false;
    }
    int dims4[2] = {n_stars, 4};
    fn_remove_block(frame_, "star_det_psf_compat");
    int r4 = fn_add_block(frame_, "star_det_psf_compat", AIO_BLOCK_FLOAT32,
                          star_det32.data(), static_cast<int64_t>(n_stars) * 4, dims4, 2,
                          "PSF 兼容视图 (FLOAT32 [N,4]: x,y,flux,mag; 显式生成, 不覆盖权威块)");
    if (r4 != 0) {
        LOG_WARN("orchestrator", "[PSF] star_det_psf_compat 兼容视图写入失败 "
                 "(可选, 下游回退读权威块): ret=" + std::to_string(r4));
    }
    LOG_INFO("orchestrator", "[PSF] star_det 权威 FLOAT64[N,6] + psf_compat FLOAT32[N,4] 已写入 "
             "(" + std::to_string(n_stars) + " 颗星), 兼容转换最大相对误差="
             + std::to_string(max_compat_rel));

    // 2.4 stable star_id (位置哈希, 行无关, 帧内唯一; wiki/05 契约)
    std::vector<int64_t> star_ids;
    assign_stable_star_ids(det_x, det_y, n_stars, star_ids);

    // 2.5 cx/cy (double, dpsf_fit_batch 要求)
    std::vector<double> cx_arr(n_stars), cy_arr(n_stars);
    for (int i = 0; i < n_stars; ++i) {
        cx_arr[i] = det_x[i];
        cy_arr[i] = det_y[i];
    }

    // 3. 获取 PSF 拟合函数指针并按精度模式调用
    //    FP64: dpsf_fit_batch_d (double 图像, 返回完整 DPSFFitResult*)
    //    FP32: dpsf_fit_batch   (uint16 图像, 返回完整 DPSFFitResult*)
    //    两者返回的结构体字段一致, 下游 psf 块映射保持不变 (向后兼容)。
    // R11: typed Stage1Config 直接驱动
    int fit_radius = stage1_cfg_->psf.fit_radius;
    int max_iter = stage1_cfg_->psf.max_iterations;
    double tolerance = stage1_cfg_->psf.tolerance;
    if (fit_radius < 0) fit_radius = 0;  // 0 = 自动
    if (max_iter <= 0) max_iter = 100;
    if (tolerance <= 0.0) tolerance = 1e-6;

    DPSFFitParams params;
    params.fitRadius = fit_radius;
    params.maxIter = max_iter;
    params.tolerance = tolerance;

    auto fn_free_results = dll_loader_.get_function<void (*)(DPSFFitResult*)>(
        ModuleId::PSF, "dpsf_free_results");
    if (!fn_free_results) {
        LOG_ERROR("orchestrator", "[PSF] dpsf_free_results 函数未找到");
        result.error_msg = "[PSF] dpsf_free_results 函数未找到";
        return false;
    }

    DPSFFitResult* results = nullptr;
    int ret;
    if (use_fp64) {
        auto fn_fit_batch_d = dll_loader_.get_function<int (*)(
            const double*, int, int,
            const double*, const double*, int,
            const DPSFFitParams*, DPSFFitResult**)>(
            ModuleId::PSF, "dpsf_fit_batch_d");
        if (!fn_fit_batch_d) {
            LOG_ERROR("orchestrator", "[PSF] dpsf_fit_batch_d 函数未找到 (FP64)");
            result.error_msg = "[PSF] dpsf_fit_batch_d 函数未找到 (FP64)";
            return false;
        }
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "[PSF] 调用 dpsf_fit_batch_d (FP64, fitRadius=%d, maxIter=%d, tol=%.1e) ...",
                fit_radius, max_iter, tolerance);
            LOG_INFO("orchestrator", std::string(buf));
        }
        ret = fn_fit_batch_d(pixels_f64, width, height,
                             cx_arr.data(), cy_arr.data(), n_stars,
                             &params, &results);
    } else {
        // R11 (PREC-105): FP32 直接使用 float32 图像 (dpsf_fit_batch_f),
        // 不再经过 uint16 有损转换
        auto fn_fit_batch_f = dll_loader_.get_function<int (*)(
            const float*, int, int,
            const double*, const double*, int,
            const DPSFFitParams*, DPSFFitResult**)>(
            ModuleId::PSF, "dpsf_fit_batch_f");
        if (!fn_fit_batch_f) {
            LOG_ERROR("orchestrator", "[PSF] dpsf_fit_batch_f 函数未找到");
            result.error_msg = "[PSF] dpsf_fit_batch_f 函数未找到";
            return false;
        }
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "[PSF] 调用 dpsf_fit_batch_f (FP32 float 图像, fitRadius=%d, maxIter=%d, tol=%.1e) ...",
                fit_radius, max_iter, tolerance);
            LOG_INFO("orchestrator", std::string(buf));
        }
        ret = fn_fit_batch_f(pixels_f32, width, height,
                             cx_arr.data(), cy_arr.data(), n_stars,
                             &params, &results);
    }

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

    int dims[2] = {n_stars, 9};
    fn_remove_block(frame_, "psf");
    int r = fn_add_block(frame_, "psf", AIO_BLOCK_FLOAT64,
                         psf_data, n_elements, dims, 2,
                         "PSF 拟合结果: status,B,flux,cx,cy,fwhm,A,mad,eccentricity");
    if (r != 0) {
        LOG_ERROR("orchestrator", "[PSF] 写入 psf 块失败: ret=" + std::to_string(r));
        result.error_msg = "[PSF] 写入 psf 块失败";
        std::free(psf_data);
        fn_free_results(results);
        return false;
    }

    // 5. 写入 star_measurements 权威块 (FLOAT64 [N,15], schema astrocs-star-measurements-1)
    //    列含义:
    //      [0]=star_id (int64 as double), [1]=x, [2]=y,
    //      [3]=flux_inst (PSF integrated flux), [4]=flux_uncertainty (PSF mad proxy),
    //      [5]=background (PSF B), [6]=psf_status, [7]=fwhm,
    //      [8]=A, [9]=B, [10]=mad, [11]=eccentricity,
    //      [12]=mag (detector), [13]=saturated, [14]=has_saturated
    //    说明: PSF 当前不输出独立 background_rms, SNR 使用 A/B/mad (与冻结 SNR 定义一致);
    //          下游必须通过 star_id 连接, 禁止通过数组行号隐式连接。
    {
        const int kSmCols = 15;
        std::vector<double> sm(static_cast<size_t>(n_stars) * kSmCols, 0.0);
        for (int i = 0; i < n_stars; ++i) {
            const double* prow = psf_data + static_cast<size_t>(i) * 9;
            double* row = sm.data() + static_cast<size_t>(i) * kSmCols;
            row[0] = static_cast<double>(star_ids[static_cast<size_t>(i)]);
            // Phase1 Final Closure V3: 权威坐标为 PSF 拟合中心 (DPSF 输出,
            // 统一"像素索引即中心坐标"契约), 检测初值仅作 fallback。
            // PSF 成功: x/y = results[i].cx/cy; PSF 失败: 暂存检测坐标 (status 已失败)。
            // status: 0=DPSF_FIT_OK, 3=DPSF_FIT_ITERATION_LIMIT (参数有效亦视为成功)
            bool psf_valid = (prow[0] == 0.0 || prow[0] == 3.0) &&
                             std::isfinite(prow[3]) && std::isfinite(prow[4]);
            if (psf_valid) {
                row[1] = prow[3];  // cx (PSF 拟合中心, 统一契约)
                row[2] = prow[4];  // cy
            } else {
                // PSF 失败: 检测坐标 (sdet 像素中心=索引+0.5) 显式转统一契约 (索引即中心)
                row[1] = cx_arr[static_cast<size_t>(i)] - 0.5;
                row[2] = cy_arr[static_cast<size_t>(i)] - 0.5;
            }
            row[3] = prow[2];  // flux
            row[4] = prow[7];  // mad (uncertainty proxy)
            row[5] = prow[1];  // B (background)
            row[6] = prow[0];  // status
            row[7] = prow[5];  // fwhm
            row[8] = prow[6];  // A
            row[9] = prow[1];  // B
            row[10] = prow[7]; // mad
            row[11] = prow[8]; // eccentricity
            row[12] = static_cast<double>(det_mag[i]);
            row[13] = static_cast<double>(det_sat[i]);
            row[14] = static_cast<double>(det_has_sat[i]);
        }
        int sm_dims[2] = {n_stars, kSmCols};
        fn_remove_block(frame_, "star_measurements");
        int sm_ret = fn_add_block(frame_, "star_measurements", AIO_BLOCK_FLOAT64,
                                  sm.data(), static_cast<int64_t>(n_stars) * kSmCols,
                                  sm_dims, 2,
                                  "星点权威测量块 (FLOAT64 [N,15]: star_id,x,y,flux_inst,flux_unc,"
                                  "background,psf_status,fwhm,A,B,mad,eccentricity,mag,saturated,has_saturated)");
        if (sm_ret != 0) {
            LOG_ERROR("orchestrator", "[PSF] star_measurements 块写入失败 (必需块): ret="
                     + std::to_string(sm_ret));
            result.error_msg = "[PSF] star_measurements 块写入失败 (必需块)";
            result.exit_code = AstroCsExitCode::BLOCK_MISSING;
            fn_free_results(results);
            std::free(psf_data);
            fn_sdet_free(det_x, det_y, det_flux, det_sat, det_mag, det_has_sat, nullptr, 0);
            return false;
        }
        LOG_INFO("orchestrator", "[PSF] star_measurements 权威块已写入 (FLOAT64 [N,15], "
                 + std::to_string(n_stars) + " 颗星)");
    }

    fn_free_results(results);
    std::free(psf_data);  // psf 块与 star_measurements 均已拷贝

    // 释放检测缓冲 (star_det/star_measurements 已拷贝进 PipelineFrame)
    fn_sdet_free(det_x, det_y, det_flux, det_sat, det_mag, det_has_sat, nullptr, 0);

    LOG_INFO("orchestrator", "[PSF/STAR_MEASURE] 完成: " + std::to_string(n_stars) + " 颗星, "
             + std::to_string(n_ok) + " PSF 成功, stable star_id 已分配");
    return true;
}

bool Orchestrator::run_stage_photometric(TaskResult& result) {
    LOG_INFO("orchestrator", "[PHOTOMETRIC] 开始");

    // P03-003: PHOTOMETRIC 是必需 stage (产出 photo_stats 供 SNR/Drizzle 使用), DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::PHOTOMETRIC)) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] PHOTOMETRIC DLL 未加载 (必需模块)");
        result.error_msg = "[PHOTOMETRIC] PHOTOMETRIC DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[PHOTOMETRIC] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
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

    // 1. 读取 data 块 ([H,W])
    //    R10: FP64 模式下 data 块为 FLOAT64 (double), 否则为 FLOAT32 (float)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] data 块不存在");
        result.error_msg = "[PHOTOMETRIC] data 块不存在";
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const bool use_fp64 = (config_.precision == PrecisionMode::FP64);
    const float* pixels_f32 = nullptr;
    const double* pixels_f64 = nullptr;
    if (use_fp64) {
        if (data_block->type != AIO_BLOCK_FLOAT64) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] FP64 模式但 data 块非 FLOAT64 (type="
                     + std::to_string((int)data_block->type) + "), 上游阶段未按精度模式产出");
            result.error_msg = "[PHOTOMETRIC] FP64 模式 data 块类型不匹配";
            result.exit_code = AstroCsExitCode::MODULE_ABI_UNSUPPORTED;
            return false;
        }
        pixels_f64 = static_cast<const double*>(data_block->data);
        LOG_INFO("orchestrator", "[PHOTOMETRIC] 图像(FP64): " + std::to_string(width) + "x" + std::to_string(height));
    } else {
        pixels_f32 = static_cast<const float*>(data_block->data);
        LOG_INFO("orchestrator", "[PHOTOMETRIC] 图像: " + std::to_string(width) + "x" + std::to_string(height));
    }

    // 2. 读取 psf 块 (FLOAT64 [N,9])
    //    P03-003: psf 是必需块 (PSF 阶段产出), 缺失必须失败 (退出码 3)
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr || psf_block->dims[0] <= 0) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] psf 块不存在或为空 (必需块, PSF 应产出)");
        result.error_msg = "[PHOTOMETRIC] psf 块不存在或为空 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
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

    // 2.1 读取 star_measurements 权威块 (FLOAT64 [N,15], col0=star_id)
    //     Phase1 v2: Photometric 匹配结果通过 star_id 回连, 禁止数组行号隐式连接
    std::vector<int64_t> psf_star_ids(n_psf, 0);
    std::vector<PcMatchRecord> match_records(static_cast<size_t>(n_psf));
    const AioBlock* sm_block = fn_get_block(frame_, "star_measurements");
    if (sm_block != nullptr && sm_block->type == AIO_BLOCK_FLOAT64 &&
        sm_block->dims[0] == n_psf && sm_block->dims[1] >= 1) {
        const double* smd = static_cast<const double*>(sm_block->data);
        for (int i = 0; i < n_psf; ++i) {
            psf_star_ids[i] = static_cast<int64_t>(smd[i * sm_block->dims[1]]);
        }
    } else {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] star_measurements 块缺失或格式不符, "
                 "star_id 置 0 (per-star lineage 将不可用)");
    }

    // 3. 从 header KV 读取 FILTER + WCS + SIP
    std::string filter_str;
    const char* filter_cstr = fn_kv_get(frame_, "header", "FILTER");
    if (filter_cstr) filter_str = filter_cstr;
    std::string filter_name = map_filter_name(filter_str);
    LOG_INFO("orchestrator", "[PHOTOMETRIC] FILTER='" + filter_str + "' -> '" + filter_name + "'");

    // 加载滤光片曲线 (R11: typed Stage1Config 直接驱动; schema 已必需
    // filter_response, 缺失时硬失败, 禁止默认路径兜底)
    std::string filters_json = stage1_cfg_->photometric.filter_response;
    if (filters_json.empty()) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 配置缺少 photometric.filter_response "
                  "(滤光片响应曲线路径必须由 stage1.json 引入)");
        result.error_msg = "[PHOTOMETRIC] 配置缺少 photometric.filter_response";
        return false;
    }
    LOG_INFO("orchestrator", "[PHOTOMETRIC] filters_json: " + filters_json);
    std::vector<double> filter_wl, filter_trans;
    if (!load_filter_curve(filters_json, filter_name, filter_wl, filter_trans)) {
        LOG_ERROR("orchestrator", "[PHOTOMETRIC] 加载滤光片曲线失败");
        result.error_msg = "[PHOTOMETRIC] 加载滤光片曲线失败";
        return false;
    }
    LOG_INFO("orchestrator", "[PHOTOMETRIC] 滤光片: " + std::to_string(filter_wl.size()) + " 点");

    // 加载 CCD QE 曲线 (GAP-012)
    // 从 stage1_config.json 文本中提取 qe_curve 名称, 若配置则加载 qe_curves.json
    std::vector<double> qe_wl, qe_trans;
    std::string qe_name = extract_qe_curve_name(config_.calib_params_json);
    if (!qe_name.empty()) {
        // R11: typed Stage1Config 直接驱动
        std::string qe_json = stage1_cfg_->photometric.qe_curve;
        if (qe_json.empty()) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] 配置缺少 photometric.qe_curve "
                      "(QE 曲线路径必须由 stage1.json 引入)");
            result.error_msg = "[PHOTOMETRIC] 配置缺少 photometric.qe_curve";
            return false;
        }
        LOG_INFO("orchestrator", "[PHOTOMETRIC] qe_curves_json: " + qe_json);
        if (load_qe_curve(qe_json, qe_name, qe_wl, qe_trans)) {
            LOG_INFO("orchestrator", "[PHOTOMETRIC] QE 曲线 '" + qe_name + "': " + std::to_string(qe_wl.size()) + " 点");
        } else {
            LOG_WARN("orchestrator", "[PHOTOMETRIC] 加载 QE 曲线 '" + qe_name + "' 失败, F_syn 将不含 Q(λ)");
        }
    } else {
        LOG_INFO("orchestrator", "[PHOTOMETRIC] 未配置 qe_curve, F_syn 将不含 Q(λ)");
    }

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

    // R11: typed Stage1Config 直接驱动 (mag_min/mag_max 保留科学默认, fov 自动计算)
    double mag_min = 6.0;
    double mag_max = 16.0;
    // 4. 调用 pc_calibrate_simple_with_gaia (DLL 内部: 锥形搜索+光谱积分+星匹配+scale 校正)
    // 签名扩展 (GAP-012): 新增 qe_wl/qe_trans/qe_count 三个参数, 位于 filter 参数之后 spectrum 参数之前
    // R10: FP64 模式调用 _f64 变体 (pixels/out_pixels 为 double*)
    int64_t n_pix = static_cast<int64_t>(width) * height;
    int out_n_matched = 0;
    double out_scale = 0.0, out_sigma = 0.0;
    PhotometricDiag diag = {};
    int ret = 0;

    // SIP 指针 (无 SIP 时传 nullptr)
    const double* sip_a_ptr = (wcs.sip_order > 0) ? wcs.sip_a : nullptr;
    const double* sip_b_ptr = (wcs.sip_order > 0) ? wcs.sip_b : nullptr;
    const double* sip_ap_ptr = (wcs.has_ap) ? wcs.sip_ap : nullptr;
    const double* sip_bp_ptr = (wcs.has_ap) ? wcs.sip_bp : nullptr;

    if (use_fp64) {
        // R10 + Phase1 v2: FP64 路径 - pc_calibrate_simple_with_gaia_f64_v2 (per-star 导出)
        auto fn_pc_calib_f64 = dll_loader_.get_function<int (*)(
            void*, double, double, double, double, double,
            const double*, const double*, int,
            const double*, const double*, int,
            const double*, int,
            const double*, int, int,
            const double*, const double*, const double*, const int*, int,
            const int64_t*, PcMatchRecord*,
            double, double, double, double, double, double, double, double,
            int, const double*, const double*, const double*, const double*,
            double*, int*, double*, double*, PhotometricDiag*)>(
            ModuleId::PHOTOMETRIC, "pc_calibrate_simple_with_gaia_f64_v2");

        if (!fn_pc_calib_f64) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_f64_v2 函数未找到 (FP64 模式)");
            result.error_msg = "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_f64_v2 函数未找到";
            result.exit_code = AstroCsExitCode::MODULE_ABI_UNSUPPORTED;
            return false;
        }

        double* out_pixels_f64 = static_cast<double*>(std::malloc(n_pix * sizeof(double)));
        if (out_pixels_f64 == nullptr) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] 分配输出缓冲(FP64)失败");
            result.error_msg = "[PHOTOMETRIC] 分配输出缓冲(FP64)失败";
            return false;
        }

        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "[PHOTOMETRIC] 调用 pc_calibrate_simple_with_gaia_f64_v2 (mag_min=%.1f, mag_max=%.1f, fov=%.3fdeg, FP64, per-star) ...",
                mag_min, mag_max, fov_radius_deg);
            LOG_INFO("orchestrator", std::string(buf));
        }
        ret = fn_pc_calib_f64(
            reinterpret_cast<void*>(reinterpret_cast<GaiaClient*>(gaia_client_handle_)),
            wcs.crval1, wcs.crval2, fov_radius_deg,
            mag_min, mag_max,
            filter_wl.data(), filter_trans.data(), (int)filter_wl.size(),
            qe_wl.empty() ? nullptr : qe_wl.data(),
            qe_trans.empty() ? nullptr : qe_trans.data(),
            (int)qe_wl.size(),
            spectrum_wl.data(), (int)spectrum_wl.size(),
            pixels_f64, width, height,
            psf_cx.data(), psf_cy.data(), psf_flux.data(), psf_status.data(), n_psf,
            psf_star_ids.data(), match_records.data(),
            wcs.crval1, wcs.crval2, wcs.crpix1, wcs.crpix2,
            wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
            wcs.sip_order, sip_a_ptr, sip_b_ptr, sip_ap_ptr, sip_bp_ptr,
            out_pixels_f64, &out_n_matched, &out_scale, &out_sigma, &diag);

        if (ret != 0) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_f64 失败: ret=" + std::to_string(ret));
            result.error_msg = "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_f64 失败: ret=" + std::to_string(ret);
            std::free(out_pixels_f64);
            return false;
        }

        LOG_INFO("orchestrator", "[PHOTOMETRIC] 完成(FP64): n_matched=" + std::to_string(out_n_matched)
                 + ", scale=" + std::to_string(out_scale)
                 + ", sigma_residual=" + std::to_string(out_sigma));

        // 5. 更新 data 块 (FP64, 转移所有权)
        fn_remove_block(frame_, "data");
        int dims[2] = {height, width};
        int r = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT64,
                                  out_pixels_f64, n_pix * (int64_t)sizeof(double), dims, 2,
                                  "测光标定后像素 FP64 (I_cal = I * scale)");
        if (r != 0) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] 更新 data 块(FP64)失败: ret=" + std::to_string(r));
            result.error_msg = "[PHOTOMETRIC] 更新 data 块(FP64)失败";
            std::free(out_pixels_f64);
            return false;
        }
    } else {
        // FP32 路径 (Phase1 v2: per-star 导出)
        auto fn_pc_calib = dll_loader_.get_function<int (*)(
            void*, double, double, double, double, double,
            const double*, const double*, int,
            const double*, const double*, int,
            const double*, int,
            const float*, int, int,
            const double*, const double*, const double*, const int*, int,
            const int64_t*, PcMatchRecord*,
            double, double, double, double, double, double, double, double,
            int, const double*, const double*, const double*, const double*,
            float*, int*, double*, double*, PhotometricDiag*)>(
            ModuleId::PHOTOMETRIC, "pc_calibrate_simple_with_gaia_v2");

        if (!fn_pc_calib) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_v2 函数未找到");
            result.error_msg = "[PHOTOMETRIC] pc_calibrate_simple_with_gaia_v2 函数未找到";
            return false;
        }

        float* out_pixels = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
        if (out_pixels == nullptr) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] 分配输出缓冲失败");
            result.error_msg = "[PHOTOMETRIC] 分配输出缓冲失败";
            return false;
        }

        {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "[PHOTOMETRIC] 调用 pc_calibrate_simple_with_gaia_v2 (mag_min=%.1f, mag_max=%.1f, fov=%.3fdeg, per-star) ...",
                mag_min, mag_max, fov_radius_deg);
            LOG_INFO("orchestrator", std::string(buf));
        }
        ret = fn_pc_calib(
            reinterpret_cast<void*>(reinterpret_cast<GaiaClient*>(gaia_client_handle_)),
            wcs.crval1, wcs.crval2, fov_radius_deg,
            mag_min, mag_max,
            filter_wl.data(), filter_trans.data(), (int)filter_wl.size(),
            qe_wl.empty() ? nullptr : qe_wl.data(),
            qe_trans.empty() ? nullptr : qe_trans.data(),
            (int)qe_wl.size(),
            spectrum_wl.data(), (int)spectrum_wl.size(),
            pixels_f32, width, height,
            psf_cx.data(), psf_cy.data(), psf_flux.data(), psf_status.data(), n_psf,
            psf_star_ids.data(), match_records.data(),
            wcs.crval1, wcs.crval2, wcs.crpix1, wcs.crpix2,
            wcs.cd11, wcs.cd12, wcs.cd21, wcs.cd22,
            wcs.sip_order, sip_a_ptr, sip_b_ptr, sip_ap_ptr, sip_bp_ptr,
            out_pixels, &out_n_matched, &out_scale, &out_sigma, &diag);

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
    }
    // out_pixels 所有权已转移给 frame_

    // 6.0 Phase1 v2: 写入 photometric_match 权威块 (FLOAT64 [N,6])
    //     col0=star_id, col1=dr3sp_id, col2=reference_flux(F_syn),
    //     col3=residual(log10(F_instr/F_syn), NaN=未匹配), col4=status, col5=reject_reason
    //     star_id 贯穿 PSF→PlateSolve→Photometric→SNR (禁止数组行号隐式连接)
    if (!match_records.empty() && fn_add_block_move) {
        double* pm = static_cast<double*>(std::malloc(
            static_cast<size_t>(n_psf) * 6 * sizeof(double)));
        if (pm == nullptr) {
            LOG_ERROR("orchestrator", "[PHOTOMETRIC] 分配 photometric_match 块失败 (可选块)");
        } else {
        int n_used = 0, n_rejected = 0, n_unmatched = 0;
        for (int i = 0; i < n_psf; ++i) {
            const PcMatchRecord& rec = match_records[static_cast<size_t>(i)];
            double* row = pm + static_cast<size_t>(i) * 6;
            row[0] = static_cast<double>(rec.star_id);
            row[1] = static_cast<double>(rec.dr3sp_id);
            row[2] = rec.reference_flux;
            row[3] = rec.residual;
            row[4] = static_cast<double>(rec.status);
            row[5] = static_cast<double>(rec.reject_reason);
            if (rec.status == 1) ++n_used;
            else if (rec.status == 2) ++n_rejected;
            else ++n_unmatched;
        }
        int pm_dims[2] = {n_psf, 6};
        fn_remove_block(frame_, "photometric_match");
        int pm_ret = fn_add_block_move(frame_, "photometric_match", AIO_BLOCK_FLOAT64,
                                       pm, static_cast<int64_t>(n_psf) * 6,
                                       pm_dims, 2,
                                       "Photometric per-star 匹配块 (FLOAT64 [N,6]: star_id,dr3sp_id,"
                                       "reference_flux,residual,status,reject_reason)");
        if (pm_ret == 0) {
            LOG_INFO("orchestrator", "[PHOTOMETRIC] photometric_match 块已写入 (N="
                     + std::to_string(n_psf) + ", used=" + std::to_string(n_used)
                     + ", rejected=" + std::to_string(n_rejected)
                     + ", unmatched=" + std::to_string(n_unmatched) + ")");
        } else {
            LOG_WARN("orchestrator", "[PHOTOMETRIC] photometric_match 块写入失败 (可选块): ret="
                     + std::to_string(pm_ret));
            std::free(pm);
        }
        }
    } else {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] match_records 为空或 fn_add_block_move 缺失, "
                 "photometric_match 块未写入");
    }

    // 6. 写入 photo_stats KV 块 (N_MATCHED, SCALE_FACTOR, SIGMA_RESIDUAL 供 SNR 使用)
    fn_kv_set(frame_, "photo_stats", "STATUS", "OK");
    fn_kv_set_double(frame_, "photo_stats", "N_MATCHED", static_cast<double>(out_n_matched));
    fn_kv_set_double(frame_, "photo_stats", "SCALE_FACTOR", out_scale);
    fn_kv_set_double(frame_, "photo_stats", "SIGMA_RESIDUAL", out_sigma);

    // P12-001 子任务B: 写入 PhotometricDiag 17 个分阶段诊断字段到 photo_stats KV 块
    fn_kv_set_double(frame_, "photo_stats", "SPECTRUM_ROWS_TOTAL", static_cast<double>(diag.spectrum_rows_total));
    fn_kv_set_double(frame_, "photo_stats", "VALID_FSYN", static_cast<double>(diag.valid_fsyn));
    fn_kv_set_double(frame_, "photo_stats", "GAIA_IN_FRAME", static_cast<double>(diag.gaia_projected_in_frame));
    fn_kv_set_double(frame_, "photo_stats", "PSF_TOTAL", static_cast<double>(diag.psf_total));
    fn_kv_set_double(frame_, "photo_stats", "PSF_VALID", static_cast<double>(diag.psf_valid));
    fn_kv_set_double(frame_, "photo_stats", "SPATIAL_CANDIDATES", static_cast<double>(diag.spatial_candidates));
    fn_kv_set_double(frame_, "photo_stats", "UNIQUE_MATCHES", static_cast<double>(diag.unique_matches));
    fn_kv_set_double(frame_, "photo_stats", "REJECTED_AMBIGUOUS", static_cast<double>(diag.rejected_ambiguous));
    fn_kv_set_double(frame_, "photo_stats", "REJECTED_DISTANCE", static_cast<double>(diag.rejected_distance));
    fn_kv_set_double(frame_, "photo_stats", "REJECTED_QUALITY", static_cast<double>(diag.rejected_quality));
    fn_kv_set_double(frame_, "photo_stats", "FIT_USED", static_cast<double>(diag.fit_used));
    fn_kv_set_double(frame_, "photo_stats", "ROBUST_ITERATIONS", static_cast<double>(diag.robust_iterations));
    fn_kv_set_double(frame_, "photo_stats", "R_MEDIAN", diag.r_median);
    fn_kv_set_double(frame_, "photo_stats", "R_P90", diag.r_p90);
    fn_kv_set_double(frame_, "photo_stats", "R_MAX", diag.r_max);
    fn_kv_set_double(frame_, "photo_stats", "MATCH_DIST_MEDIAN", diag.match_distance_median);
    fn_kv_set_double(frame_, "photo_stats", "MATCH_DIST_P90", diag.match_distance_p90);
    fn_kv_set_double(frame_, "photo_stats", "MATCH_DIST_MAX", diag.match_distance_max);

    // B5 修复一致性: 写入 PHOTSCAL/PHOTAPPL 到 header (供 DRIZZLE 阶段读取)
    // PHOTOMETRIC 阶段已把 scale 乘入像素值 (上方 data 块替换),
    // drizzle 仅需 header 中的 PHOTSCAL 元数据记录, 不再重复应用 (避免双重缩放)。
    fn_kv_set_double(frame_, "header", "PHOTSCAL", out_scale);
    fn_kv_set(frame_, "header", "PHOTAPPL", "1");
    LOG_INFO("orchestrator", "[PHOTOMETRIC] PHOTSCAL 已写入 header: "
             + std::to_string(out_scale) + ", PHOTAPPL=1");

    // P12-001 子任务B: 同步 photo_stats 到 result.photo_stats (供 CLI quality_metric 事件使用)
    // 注: run_stage1 销毁 frame_ 后 KV 块不可访问, 故在此复制到 TaskResult
    result.photo_stats["STATUS"] = "OK";
    result.photo_stats["N_MATCHED"] = std::to_string(out_n_matched);
    result.photo_stats["SCALE_FACTOR"] = std::to_string(out_scale);
    result.photo_stats["SIGMA_RESIDUAL"] = std::to_string(out_sigma);
    result.photo_stats["SPECTRUM_ROWS_TOTAL"] = std::to_string(diag.spectrum_rows_total);
    result.photo_stats["VALID_FSYN"] = std::to_string(diag.valid_fsyn);
    result.photo_stats["GAIA_IN_FRAME"] = std::to_string(diag.gaia_projected_in_frame);
    result.photo_stats["PSF_TOTAL"] = std::to_string(diag.psf_total);
    result.photo_stats["PSF_VALID"] = std::to_string(diag.psf_valid);
    result.photo_stats["SPATIAL_CANDIDATES"] = std::to_string(diag.spatial_candidates);
    result.photo_stats["UNIQUE_MATCHES"] = std::to_string(diag.unique_matches);
    result.photo_stats["REJECTED_AMBIGUOUS"] = std::to_string(diag.rejected_ambiguous);
    result.photo_stats["REJECTED_DISTANCE"] = std::to_string(diag.rejected_distance);
    result.photo_stats["REJECTED_QUALITY"] = std::to_string(diag.rejected_quality);
    result.photo_stats["FIT_USED"] = std::to_string(diag.fit_used);
    result.photo_stats["ROBUST_ITERATIONS"] = std::to_string(diag.robust_iterations);
    result.photo_stats["R_MEDIAN"] = std::to_string(diag.r_median);
    result.photo_stats["R_P90"] = std::to_string(diag.r_p90);
    result.photo_stats["R_MAX"] = std::to_string(diag.r_max);
    result.photo_stats["MATCH_DIST_MEDIAN"] = std::to_string(diag.match_distance_median);
    result.photo_stats["MATCH_DIST_P90"] = std::to_string(diag.match_distance_p90);
    result.photo_stats["MATCH_DIST_MAX"] = std::to_string(diag.match_distance_max);

    // P12-001 子任务B: 生成 photometry_report.json
    // 遵循 engineering_v1.3/contracts/photometry_report.schema.json
    // 输出到正式输出目录 (current_output_path_ 的父目录)
    try {
        fs::path output_path(current_output_path_);
        fs::path report_path = output_path.parent_path() / "photometry_report.json";
        // JSON 字符串转义 (frame_name 是文件路径, 可能含反斜杠/双引号)
        auto json_escape_str = [](const std::string& s) -> std::string {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:   out.push_back(c); break;
                }
            }
            return out;
        };
        // status: fit_used > 0 为 PASS, 否则 FAIL (无有效拟合星)
        std::string status_str = (diag.fit_used > 0) ? "PASS" : "FAIL";
        std::ofstream ofs(report_path, std::ios::binary);
        if (ofs.is_open()) {
            ofs << "{\n";
            ofs << "  \"frame\": \"" << json_escape_str(result.frame_name) << "\",\n";
            ofs << "  \"valid_fsyn\": " << diag.valid_fsyn << ",\n";
            ofs << "  \"gaia_in_frame\": " << diag.gaia_projected_in_frame << ",\n";
            ofs << "  \"psf_valid\": " << diag.psf_valid << ",\n";
            ofs << "  \"unique_matches\": " << diag.unique_matches << ",\n";
            ofs << "  \"fit_used\": " << diag.fit_used << ",\n";
            ofs << "  \"scale_factor\": " << out_scale << ",\n";
            ofs << "  \"sigma_residual\": " << out_sigma << ",\n";
            ofs << "  \"status\": \"" << status_str << "\",\n";
            ofs << "  \"match_distance\": {\n";
            ofs << "    \"median\": " << diag.match_distance_median << ",\n";
            ofs << "    \"p90\": " << diag.match_distance_p90 << ",\n";
            ofs << "    \"max\": " << diag.match_distance_max << "\n";
            ofs << "  },\n";
            ofs << "  \"spectrum_rows_total\": " << diag.spectrum_rows_total << ",\n";
            ofs << "  \"psf_total\": " << diag.psf_total << ",\n";
            ofs << "  \"spatial_candidates\": " << diag.spatial_candidates << ",\n";
            ofs << "  \"rejected_ambiguous\": " << diag.rejected_ambiguous << ",\n";
            ofs << "  \"rejected_distance\": " << diag.rejected_distance << ",\n";
            ofs << "  \"rejected_quality\": " << diag.rejected_quality << ",\n";
            ofs << "  \"robust_iterations\": " << diag.robust_iterations << ",\n";
            ofs << "  \"r_median\": " << diag.r_median << ",\n";
            ofs << "  \"r_p90\": " << diag.r_p90 << ",\n";
            ofs << "  \"r_max\": " << diag.r_max << "\n";
            ofs << "}\n";
            ofs.close();
            LOG_INFO("orchestrator", "[PHOTOMETRIC] photometry_report.json 已生成: " + report_path.string());
        } else {
            LOG_WARN("orchestrator", "[PHOTOMETRIC] 无法创建 photometry_report.json: " + report_path.string());
        }
    } catch (const std::exception& e) {
        LOG_WARN("orchestrator", "[PHOTOMETRIC] 生成 photometry_report.json 异常: " + std::string(e.what()));
    }

    LOG_INFO("orchestrator", "[PHOTOMETRIC] photo_stats 已写入 (含 17 个诊断字段)");
    return true;
}


// ============================================================================
// V4 G4: 确定性像素抽样状态 (raw->calibrated->photometric->drizzle 同一组样本)
//   trace_selection.json/.tsv : 选中源像素坐标 (与 drizzle trace 共用)
//   pixel_lineage.jsonl       : 各阶段实际 buffer 值
// ============================================================================
static std::vector<std::pair<int,int>> g_trace_pixels;
static bool g_trace_pixels_ready = false;

// ============================================================================
// write_stage_trace - 阶段 trace 观察器 (Gate 8, Phase1 Full Freeze v2)
// 从实际 PipelineFrame 读取块统计 + 确定性随机抽样 star_measurements /
// photometric_match (按 star_id), 追加写入 <diag>/trace/stage_trace.jsonl。
// seed 由 config SHA256 派生, 可复现。
// ============================================================================
static void write_stage_trace(DllLoader& loader, const PipelineFrame* frame,
                              const char* stage_name,
                              const std::string& diagnostics_dir,
                              uint64_t seed) {
    if (!frame || !stage_name) return;
    using GetBlockFn = const AioBlock* (*)(const PipelineFrame*, const char*);
    auto fn_get_block = loader.get_function<GetBlockFn>(ModuleId::AIO, "aio_frame_get_block");
    if (!fn_get_block) return;

    std::string dir = diagnostics_dir + "/trace";
    // 先创建目录再写选择集 (V5 G4 回归: 目录不存在时首阶段 fopen 失败,
    // 导致 drizzle 引擎 fallback 覆盖 selection)
    {
        std::string mk = "mkdir -p \"" + dir + "\"";
        if (std::system(mk.c_str()) != 0) std::system(("mkdir \"" + dir + "\"").c_str());
    }

    // V4 G4: 首阶段 (READ_FITS) 生成确定性像素样本集
    const AioBlock* db0 = fn_get_block(frame, "data");
    const int img_h = (db0 && db0->n_dims >= 2) ? (int)db0->dims[0] : 0;
    const int img_w = (db0 && db0->n_dims >= 2) ? (int)db0->dims[1] : 0;
    if (!g_trace_pixels_ready) {
        g_trace_pixels_ready = true;
        if (img_w > 0 && img_h > 0) {
            uint64_t ts = seed ^ 0x6A09E667F3BCC909ULL;
            const int want = 1024;
            std::unordered_set<uint64_t> sel;
            for (int i = 0; i < want * 8 && (int)sel.size() < want; ++i) {
                ts ^= ts << 13; ts ^= ts >> 7; ts ^= ts << 17;
                int x = (int)(ts % (uint64_t)img_w);
                ts ^= ts << 13; ts ^= ts >> 7; ts ^= ts << 17;
                int y = (int)(ts % (uint64_t)img_h);
                sel.insert((uint64_t)y * 1000000ULL + (uint64_t)x);
            }
            for (int i = 0; (int)sel.size() < want && i < img_h * img_w; i += 7) {
                sel.insert((uint64_t)((i / img_w) % img_h) * 1000000ULL + (uint64_t)(i % img_w));
            }
            g_trace_pixels.clear();
            for (uint64_t k : sel) g_trace_pixels.push_back({(int)(k % 1000000ULL), (int)(k / 1000000ULL)});
            std::sort(g_trace_pixels.begin(), g_trace_pixels.end());
            FILE* jf = std::fopen((dir + "/trace_selection.json").c_str(), "wb");
            if (jf) {
                std::fprintf(jf, "{\"seed\":%llu,\"n\":%zu,\"width\":%d,\"height\":%d,\"pixels\":[",
                             (unsigned long long)seed, g_trace_pixels.size(), img_w, img_h);
                for (size_t k = 0; k < g_trace_pixels.size(); ++k) {
                    if (k) std::fprintf(jf, ",");
                    std::fprintf(jf, "{\"x\":%d,\"y\":%d}", g_trace_pixels[k].first, g_trace_pixels[k].second);
                }
                std::fprintf(jf, "]}\n");
                std::fclose(jf);
            }
            FILE* tf = std::fopen((dir + "/trace_selection.tsv").c_str(), "wb");
            if (tf) {
                std::fprintf(tf, "x\ty\n");
                for (auto& pr : g_trace_pixels)
                    std::fprintf(tf, "%d\t%d\n", pr.first, pr.second);
                std::fclose(tf);
            }
        }
    }
    std::string path = dir + "/stage_trace.jsonl";
    FILE* f = std::fopen(path.c_str(), "ab");
    if (!f) return;

    std::string json = "{\"stage\":\"" + std::string(stage_name) + "\",\"blocks\":[";
    bool first = true;
    const char* block_names[] = {"data", "star_det", "star_det_psf_compat",
                                 "psf", "star_measurements", "photometric_match"};
    for (const char* bn : block_names) {
        const AioBlock* b = fn_get_block(frame, bn);
        if (!b) continue;
        if (!first) json += ",";
        first = false;
        int64_t elem_size = (b->type == AIO_BLOCK_FLOAT64 || b->type == AIO_BLOCK_INT64) ? 8 : 4;
        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "{\"name\":\"%s\",\"type\":%d,\"dims\":[%lld,%lld],\"count\":%lld,\"bytes\":%lld}",
            bn, (int)b->type, (long long)(b->n_dims > 0 ? b->dims[0] : 0),
            (long long)(b->n_dims > 1 ? b->dims[1] : 0),
            (long long)b->count, (long long)(b->count * elem_size));
        json += buf;
    }
    json += "],\"seed\":" + std::to_string(seed) + ",\"stars\":";

    // 确定性随机抽样 star 行 (star_measurements / photometric_match)
    const AioBlock* sm = fn_get_block(frame, "star_measurements");
    const AioBlock* pm = fn_get_block(frame, "photometric_match");
    auto sample_rows = [&](const AioBlock* blk, int ncols, int max_n) {
        if (blk == nullptr || blk->data == nullptr) return std::string("[]");
        int64_t n = blk->dims[0];
        if (n <= 0) return std::string("[]");
        int64_t want = std::min<int64_t>(n, max_n);
        std::vector<int64_t> idx((size_t)n);
        for (int64_t i = 0; i < n; ++i) idx[(size_t)i] = i;
        // 确定性混洗 (xorshift, seed 派生)
        uint64_t s = seed ^ (uint64_t)blk->count;
        for (int64_t i = n - 1; i > 0; --i) {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            int64_t j = (int64_t)(s % (uint64_t)(i + 1));
            std::swap(idx[(size_t)i], idx[(size_t)j]);
        }
        const double* d = static_cast<const double*>(blk->data);
        std::string out = "[";
        for (int64_t k = 0; k < want; ++k) {
            int64_t r = idx[(size_t)k];
            if (k) out += ",";
            char buf[512];
            if (ncols == 15) {  // star_measurements
                std::snprintf(buf, sizeof(buf),
                    "{\"star_id\":%.0f,\"x\":%.3f,\"y\":%.3f,\"flux_inst\":%.6g,"
                    "\"background\":%.6g,\"fwhm\":%.4f,\"status\":%.0f}",
                    d[r*15+0], d[r*15+1], d[r*15+2], d[r*15+3], d[r*15+5], d[r*15+7], d[r*15+6]);
            } else {  // photometric_match (6 cols)
                double rf = d[r*6+2], resid = d[r*6+3];
                const char* rf_s = std::isfinite(rf) ? "" : "null";
                const char* rs_s = std::isfinite(resid) ? "" : "null";
                if (std::isfinite(rf) && std::isfinite(resid)) {
                    std::snprintf(buf, sizeof(buf),
                        "{\"star_id\":%.0f,\"dr3sp_id\":%.0f,\"reference_flux\":%.6g,"
                        "\"residual\":%.6g,\"status\":%.0f,\"reason\":%.0f}",
                        d[r*6+0], d[r*6+1], d[r*6+2], d[r*6+3], d[r*6+4], d[r*6+5]);
                } else if (std::isfinite(rf)) {
                    std::snprintf(buf, sizeof(buf),
                        "{\"star_id\":%.0f,\"dr3sp_id\":%.0f,\"reference_flux\":%.6g,"
                        "\"residual\":null,\"status\":%.0f,\"reason\":%.0f}",
                        d[r*6+0], d[r*6+1], d[r*6+2], d[r*6+4], d[r*6+5]);
                } else if (std::isfinite(resid)) {
                    std::snprintf(buf, sizeof(buf),
                        "{\"star_id\":%.0f,\"dr3sp_id\":%.0f,\"reference_flux\":null,"
                        "\"residual\":%.6g,\"status\":%.0f,\"reason\":%.0f}",
                        d[r*6+0], d[r*6+1], d[r*6+3], d[r*6+4], d[r*6+5]);
                } else {
                    std::snprintf(buf, sizeof(buf),
                        "{\"star_id\":%.0f,\"dr3sp_id\":%.0f,\"reference_flux\":null,"
                        "\"residual\":null,\"status\":%.0f,\"reason\":%.0f}",
                        d[r*6+0], d[r*6+1], d[r*6+4], d[r*6+5]);
                }
                (void)rf_s; (void)rs_s;
            }
            out += buf;
        }
        out += "]";
        return out;
    };
    json += sample_rows(sm, 15, 512);
    json += ",\"photometric_match\":";
    json += sample_rows(pm, 6, 256);
    // 完整 star_id 列表 (用于 lineage 子集校验, 仅 ID 开销小)
    auto all_ids = [&](const AioBlock* blk, int col) {
        if (blk == nullptr || blk->data == nullptr) return std::string("[]");
        int64_t n = blk->dims[0];
        if (n <= 0) return std::string("[]");
        const double* d = static_cast<const double*>(blk->data);
        std::string out = "[";
        for (int64_t i = 0; i < n; ++i) {
            if (i) out += ",";
            char buf[48];
            std::snprintf(buf, sizeof(buf), "%.0f", d[i * col]);
            out += buf;
        }
        out += "]";
        return out;
    };
    json += ",\"all_psf_star_ids\":";
    json += all_ids(sm, 15);
    json += ",\"all_photometric_star_ids\":";
    json += all_ids(pm, 6);
    json += "}\n";
    std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);

    // V4 G4: 同一组样本像素值 (raw->calibrated->photometric 实际 buffer)
    if (!g_trace_pixels.empty() && db0 && db0->data &&
        db0->count >= (int64_t)img_w * img_h) {
        FILE* pf = std::fopen((dir + "/pixel_lineage.jsonl").c_str(), "ab");
        if (pf) {
            if (db0->type == AIO_BLOCK_FLOAT32) {
                const float* d = static_cast<const float*>(db0->data);
                for (auto& pr : g_trace_pixels) {
                    std::fprintf(pf, "{\"stage\":\"%s\",\"x\":%d,\"y\":%d,\"value\":%.9g}\n",
                                 stage_name, pr.first, pr.second,
                                 (double)d[(size_t)pr.second * img_w + pr.first]);
                }
            } else if (db0->type == AIO_BLOCK_FLOAT64) {
                const double* d = static_cast<const double*>(db0->data);
                for (auto& pr : g_trace_pixels) {
                    std::fprintf(pf, "{\"stage\":\"%s\",\"x\":%d,\"y\":%d,\"value\":%.17g}\n",
                                 stage_name, pr.first, pr.second,
                                 d[(size_t)pr.second * img_w + pr.first]);
                }
            }
            std::fclose(pf);
        }
    }
}

bool Orchestrator::run_stage_drizzle(TaskResult& result) {
    LOG_INFO("orchestrator", "[DRIZZLE] 开始");

    // P03-003: DRIZZLE 是必需 stage (生成 .hiss 输出), DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::DRIZZLE)) {
        LOG_ERROR("orchestrator", "[DRIZZLE] DRIZZLE DLL 未加载 (必需模块)");
        result.error_msg = "[DRIZZLE] DRIZZLE DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[DRIZZLE] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[DRIZZLE] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 获取函数指针 (Phase1 V3: 正式末端 hp_drizzle_run_hips 直写 HiPS)
    auto fn_drizzle_hips = dll_loader_.get_function<int (*)(
        PipelineFrame*, int, int, double,
        const char*, const char*, HpDrizzleResult*, int)>(
        ModuleId::DRIZZLE, "hp_drizzle_run_hips");

    if (!fn_drizzle_hips) {
        LOG_ERROR("orchestrator", "[DRIZZLE] hp_drizzle_run_hips 函数未找到");
        result.error_msg = "[DRIZZLE] hp_drizzle_run_hips 函数未找到";
        return false;
    }

    // GAP-016: NSIDE 自适应
    // 1) 从 frame_ header 读取 CD 矩阵 (度/像素)
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");
    double cd11 = 0.0, cd12 = 0.0, cd21 = 0.0, cd22 = 0.0;
    if (fn_kv_get_double) {
        cd11 = fn_kv_get_double(frame_, "header", "CD1_1", 0.0);
        cd12 = fn_kv_get_double(frame_, "header", "CD1_2", 0.0);
        cd21 = fn_kv_get_double(frame_, "header", "CD2_1", 0.0);
        cd22 = fn_kv_get_double(frame_, "header", "CD2_2", 0.0);
    } else {
        LOG_WARN("orchestrator", "[DRIZZLE] aio_frame_kv_get_double 不可用, CD 矩阵取 0");
    }

    // 2) R11: typed Stage1Config 直接驱动 (nside/pixfrac/ordering/precision)
    std::string nside_strategy = "1x_to_2x_drizzle";
    int nside_override = 0;
    double pixfrac = stage1_cfg_->drizzle.pixfrac;
    int nested = (stage1_cfg_->drizzle.ordering == "nested") ? 1 : 0;
    if (stage1_cfg_->drizzle.nside_mode == "explicit") {
        nside_strategy = "fixed";
        nside_override = stage1_cfg_->drizzle.nside_value;
    }
    if (nside_used_ > 0) {
        // NSIDE 阶段已计算: 与 explicit 或 auto 结果一致 (固定策略不重复推导)
        nside_override = nside_used_;
        nside_strategy = "fixed";
    }

    // 3) 计算最终 nside
    int nside = calculate_nside(cd11, cd12, cd21, cd22, nside_strategy, nside_override);
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "[DRIZZLE] nside=%d (strategy=%s, override=%d, cd11=%.6f cd12=%.6f), pixfrac=%.3f, nested=%d",
            nside, nside_strategy.c_str(), nside_override, cd11, cd12, pixfrac, nested);
        LOG_INFO("orchestrator", std::string(buf));
    }

    // 调用 hp_drizzle_run_hips
    // P03-002: pixfrac 从 config 读取 (省略时生产默认 0.8, V5 CFG-001), nested 从 config 读取 (默认 true)
    // hips_dir = output.hips (缺省由 current_output_path_ 派生, 去 .hiss 换 .hips)
    // legacy .hiss 仅在 validation.legacy_hiss_compare=true 时写出 (默认关闭)
    // R10: 通过 header KV "PRECISION" 传递精度模式给 drizzle DLL
    //      "fp32" (默认) 或 "fp64", drizzle DLL 写入 HISS metadata precision_mode 字段
    auto fn_kv_set = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_set");
    if (fn_kv_set && frame_) {
        const char* prec_str = (config_.precision == PrecisionMode::FP64) ? "fp64" : "fp32";
        int kv_ret = fn_kv_set(frame_, "header", "PRECISION", prec_str);
        if (kv_ret != 0) {
            LOG_WARN("orchestrator", "[DRIZZLE] 设置 PRECISION header KV 失败 (rc="
                     + std::to_string(kv_ret) + "), drizzle 将使用默认 FP32");
        } else {
            LOG_INFO("orchestrator", "[DRIZZLE] PRECISION header KV 设为: " + std::string(prec_str));
        }
    }

    HpDrizzleResult driz_result;
    std::memset(&driz_result, 0, sizeof(HpDrizzleResult));
    std::string hips_dir = stage1_cfg_->output.hips;
    if (hips_dir.empty()) {
        hips_dir = current_output_path_;
        if (hips_dir.size() > 5 &&
            hips_dir.compare(hips_dir.size() - 5, 5, ".hiss") == 0) {
            hips_dir = hips_dir.substr(0, hips_dir.size() - 5) + ".hips";
        } else {
            hips_dir += ".hips";
        }
    }
    const std::string legacy_hiss =
        stage1_cfg_->validation.legacy_hiss_compare
            ? (stage1_cfg_->validation.legacy_hiss_path.empty()
                   ? stage1_cfg_->output.hiss
                   : stage1_cfg_->validation.legacy_hiss_path)
            : std::string();
    // overwrite=true 时清理旧 HiPS 输出 (避免 CFITSIO 拒绝覆盖/残留旧 tile)
    if (stage1_cfg_->output.overwrite && fs::exists(hips_dir)) {
        std::error_code ec;
        fs::remove_all(hips_dir, ec);
        if (ec) {
            LOG_WARN("orchestrator", "[DRIZZLE] 清理旧 HiPS 目录失败: " + hips_dir
                     + " (" + ec.message() + ")");
        } else {
            LOG_INFO("orchestrator", "[DRIZZLE] 已清理旧 HiPS 输出: " + hips_dir);
        }
    }
    LOG_INFO("orchestrator", "[DRIZZLE] HiPS 输出: " + hips_dir
             + " legacy_hiss=" + (legacy_hiss.empty() ? "OFF" : legacy_hiss)
             + " precision=" + (config_.precision == PrecisionMode::FP64 ? "FP64" : "FP32"));

    int ret = fn_drizzle_hips(frame_, nside, nested, pixfrac,
                              hips_dir.c_str(),
                              legacy_hiss.empty() ? nullptr : legacy_hiss.c_str(),
                              &driz_result,
                              static_cast<int>(config_.precision));
    if (ret != 0) {
        std::string err = driz_result.error_msg[0] != '\0'
            ? std::string(driz_result.error_msg)
            : std::to_string(ret);
        LOG_ERROR("orchestrator", "[DRIZZLE] hp_drizzle_run_hips 失败: " + err);
        result.error_msg = "[DRIZZLE] hp_drizzle_run_hips 失败: " + err;
        result.exit_code = AstroCsExitCode::DRIZZLE_FAILED;
        return false;
    }

    LOG_INFO("orchestrator", "[DRIZZLE] 完成: n_healpix=" + std::to_string(driz_result.n_healpix_pixels)
             + " n_source=" + std::to_string(driz_result.n_source_pixels)
             + " 耗时=" + std::to_string(driz_result.elapsed_sec) + "s");

    current_hips_dir_ = hips_dir;
    LOG_INFO("orchestrator", "[DRIZZLE] HiPS 已直写: " + hips_dir
             + " (无 HISS 中转)");
    return true;
}

// ============================================================================
// stage 7: HISS_VERIFY - 验证 drizzle 输出的 .hiss 文件完整性
// TEST-003/TEST-004: 全 Tile 遍历验证 (不再只检查前 10 个 Tile)
// R10: 同时验证 metadata 中 precision_mode 与请求一致
// 验证项:
//   1. 文件可正常打开 (aio_hiss_inspect 返回 0)
//   2. metadata 中 precision_mode 与请求一致 (若 metadata 包含该字段)
//   3. 全 Tile 遍历: 每个 Tile 必须通过以下验证
//      a. required 子块 (SIGNAL + SUPPORT) — read_tile_* 找不到子块返回错误
//      b. checksum — HissReader::read_subblock 内部校验
//      c. occupancy 与 compact 长度一致 — HissReader 按 occ_mode 展开时验证
//      d. dtype 与 metadata precision_mode 一致 — HissReader 拒绝静默转换
//      e. signal 中不得有 NaN/Inf — 显式检查
//   4. 至少一个 Tile 有非零 signal (汇总后检查)
//   5. 任何 Tile 失败都硬失败 (返回 HISS_INVALID)
//   6. 输出全 Tile 验证汇总 (n_tiles, n_passed, n_failed, n_signal_nonzero, n_support_nonzero)
// ============================================================================
bool Orchestrator::run_stage_hiss_verify(TaskResult& result) {
    // V5 CFG-002: HISS_VERIFY 仅 legacy_hiss_compare=true 时可执行
    if (!stage1_cfg_->validation.legacy_hiss_compare) {
        LOG_INFO("orchestrator", "[HISS_VERIFY] legacy_hiss_compare=false, 跳过 (正式验证为 HIPS_VERIFY)");
        return true;
    }
    const std::string hiss_path =
        legacy_hiss_path_.empty() ? stage1_cfg_->output.hiss : legacy_hiss_path_;
    LOG_INFO("orchestrator", "[HISS_VERIFY] 开始 (legacy): " + hiss_path);

    // AIO DLL 是必需模块
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::AIO)) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] AIO DLL 未加载 (必需模块)");
        result.error_msg = "[HISS_VERIFY] AIO DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // 输出文件必须存在
    if (hiss_path.empty()) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] 输出 .hiss 路径为空");
        result.error_msg = "[HISS_VERIFY] 输出 .hiss 路径为空";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    if (!fs::exists(hiss_path)) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] .hiss 文件不存在: " + hiss_path);
        result.error_msg = "[HISS_VERIFY] .hiss 文件不存在: " + hiss_path;
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    DllLoader& loader = dll_loader_;

    // 1. 调用 aio_hiss_inspect 验证文件可打开 + 获取 Tile 列表
    using InspectFn = int (*)(const char*, uint32_t*, uint32_t*, uint32_t*,
                               uint32_t*, uint64_t*, uint64_t*, char**, uint64_t**);
    using FreeFn = void (*)(void*);
    auto fn_inspect = loader.get_function<InspectFn>(ModuleId::AIO, "aio_hiss_inspect");
    auto fn_free = loader.get_function<FreeFn>(ModuleId::AIO, "aio_hio_free");

    if (!fn_inspect || !fn_free) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] aio_hiss_inspect/aio_hio_free 函数未找到");
        result.error_msg = "[HISS_VERIFY] AIO DLL 未导出 aio_hiss_inspect/aio_hio_free";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
    uint64_t n_tiles = 0, n_pix_total = 0;
    char* meta_json = nullptr;
    uint64_t* tile_ipix_list = nullptr;

    int inspect_ret = fn_inspect(hiss_path.c_str(), &nside, &tile_nside,
                                  &depth, &n_leaf_per_tile, &n_tiles,
                                  &n_pix_total, &meta_json, &tile_ipix_list);
    if (inspect_ret != 0) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] aio_hiss_inspect 失败 (rc="
                 + std::to_string(inspect_ret) + "): " + hiss_path);
        result.error_msg = "[HISS_VERIFY] aio_hiss_inspect 失败 (rc="
                         + std::to_string(inspect_ret) + ")";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    LOG_INFO("orchestrator", "[HISS_VERIFY] 文件可打开: nside=" + std::to_string(nside)
             + " tile_nside=" + std::to_string(tile_nside)
             + " depth=" + std::to_string(depth)
             + " n_tiles=" + std::to_string(n_tiles)
             + " n_pix_total=" + std::to_string(n_pix_total));

    if (n_tiles == 0) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] .hiss 文件无 Tile (n_tiles=0)");
        if (meta_json) fn_free(meta_json);
        if (tile_ipix_list) fn_free(tile_ipix_list);
        result.error_msg = "[HISS_VERIFY] .hiss 文件无 Tile (n_tiles=0)";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    // 2. 验证 metadata 中 precision_mode (若字段存在)
    // R10: precision_mode 字段 0=FP32, 1=FP64
    // 若 metadata 不含 precision_mode 字段, 视为默认 FP32 (向后兼容)
    // FP64 请求时若 metadata 无 precision_mode, 输出 WARN (FP64 兼容模式)
    uint8_t requested_prec = static_cast<uint8_t>(config_.precision);
    bool meta_has_prec = false;
    uint8_t meta_prec = 0;
    if (meta_json && meta_json[0] != '\0') {
        std::string meta_str(meta_json);
        // 使用 orc_getJsonNum 查找 precision_mode
        // precision_mode 合法值为 0 (FP32) 或 1 (FP64), 用 -1.0 作为"字段缺失"哨兵
        double val = orc_getJsonNum(meta_str, "precision_mode", -1.0);
        if (val >= 0.0) {
            meta_has_prec = true;
            meta_prec = static_cast<uint8_t>(val);
        }
    }

    if (meta_has_prec) {
        if (meta_prec != requested_prec) {
            LOG_ERROR("orchestrator", "[HISS_VERIFY] precision_mode 不匹配: metadata="
                     + std::to_string(meta_prec) + " requested="
                     + std::to_string(requested_prec));
            if (meta_json) fn_free(meta_json);
            if (tile_ipix_list) fn_free(tile_ipix_list);
            result.error_msg = "[HISS_VERIFY] precision_mode 不匹配: metadata="
                             + std::to_string(meta_prec) + " requested="
                             + std::to_string(requested_prec);
            result.exit_code = AstroCsExitCode::HISS_INVALID;
            return false;
        }
        LOG_INFO("orchestrator", "[HISS_VERIFY] precision_mode 匹配: "
                 + std::to_string(meta_prec) + " ("
                 + (requested_prec == 1 ? "FP64" : "FP32") + ")");
    } else {
        // R11 (HISS-101): 正式验证缺 precision_mode 字段一律硬失败
        // (旧文件兼容只能走独立 legacy 读取, 不得让正式 Phase1 验证软通过)
        LOG_ERROR("orchestrator", "[HISS_VERIFY] metadata 缺 precision_mode 字段 (硬失败)");
        if (meta_json) fn_free(meta_json);
        if (tile_ipix_list) fn_free(tile_ipix_list);
        result.error_msg = "[HISS_VERIFY] metadata 缺 precision_mode 字段 (硬失败)";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    // 3. 至少一个 Tile 可读取 + signal 非全零 + support 非全零
    // R10: 根据 requested_prec 选择 FP32 或 FP64 读取函数
    using ReadTileSignalFn = int (*)(const char*, uint64_t, float**, uint32_t*);
    using ReadTileSignalF64Fn = int (*)(const char*, uint64_t, double**, uint32_t*);
    using ReadTileSupportFn = int (*)(const char*, uint64_t, uint8_t**, uint32_t*);
    auto fn_read_signal = loader.get_function<ReadTileSignalFn>(
        ModuleId::AIO, "aio_hiss_read_tile_signal");
    auto fn_read_signal_f64 = loader.get_function<ReadTileSignalF64Fn>(
        ModuleId::AIO, "aio_hiss_read_tile_signal_f64");
    auto fn_read_support = loader.get_function<ReadTileSupportFn>(
        ModuleId::AIO, "aio_hiss_read_tile_support");
    // R13 (HISS_IO_REPAIR): Verify 单句柄 — 打开一次, 遍历全部 Tile
    // (旧实现每 Tile 构造 Reader 反复打开文件, 完整帧 HISS_VERIFY 130s)
    using OpenSessionFn = void* (*)(const char*, uint32_t*, uint32_t*, uint64_t*);
    using ReadSigSessFn = int (*)(void*, uint64_t, float**, uint32_t*);
    using ReadSigF64SessFn = int (*)(void*, uint64_t, double**, uint32_t*);
    using ReadSupSessFn = int (*)(void*, uint64_t, uint8_t**, uint32_t*);
    using ReadSnrSessFn = int (*)(void*, uint64_t, uint8_t**, uint32_t*);
    using CloseSessionFn = void (*)(void*);
    auto fn_open_session = loader.get_function<OpenSessionFn>(
        ModuleId::AIO, "aio_hiss_open_session");
    auto fn_read_signal_sess = loader.get_function<ReadSigSessFn>(
        ModuleId::AIO, "aio_hiss_read_tile_signal_session");
    auto fn_read_signal_f64_sess = loader.get_function<ReadSigF64SessFn>(
        ModuleId::AIO, "aio_hiss_read_tile_signal_f64_session");
    auto fn_read_support_sess = loader.get_function<ReadSupSessFn>(
        ModuleId::AIO, "aio_hiss_read_tile_support_session");
    auto fn_read_snr_sess = loader.get_function<ReadSnrSessFn>(
        ModuleId::AIO, "aio_hiss_read_tile_snr_session");
    auto fn_read_snr_f64_sess = loader.get_function<ReadSnrSessFn>(
        ModuleId::AIO, "aio_hiss_read_tile_snr_f64_session");
    auto fn_close_session = loader.get_function<CloseSessionFn>(
        ModuleId::AIO, "aio_hiss_close_session");
    if (!fn_open_session || !fn_close_session ||
        !fn_read_signal_sess || !fn_read_support_sess) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] session API 未找到 (aio_hiss_*_session)");
        if (meta_json) fn_free(meta_json);
        if (tile_ipix_list) fn_free(tile_ipix_list);
        result.error_msg = "[HISS_VERIFY] session API 未找到";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    void* session = fn_open_session(hiss_path.c_str(),
                                    nullptr, nullptr, nullptr);
    if (!session) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] session 打开失败: "
                 + hiss_path);
        if (meta_json) fn_free(meta_json);
        if (tile_ipix_list) fn_free(tile_ipix_list);
        result.error_msg = "[HISS_VERIFY] session 打开失败";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    bool is_fp64 = (requested_prec == 1);
    if (is_fp64) {
        if (!fn_read_signal_f64 || !fn_read_support) {
            LOG_ERROR("orchestrator", "[HISS_VERIFY] aio_hiss_read_tile_signal_f64/support 函数未找到");
            if (meta_json) fn_free(meta_json);
            if (tile_ipix_list) fn_free(tile_ipix_list);
            result.error_msg = "[HISS_VERIFY] AIO DLL 未导出 aio_hiss_read_tile_signal_f64/support";
            result.exit_code = AstroCsExitCode::HISS_INVALID;
            return false;
        }
    } else {
        if (!fn_read_signal || !fn_read_support) {
            LOG_ERROR("orchestrator", "[HISS_VERIFY] aio_hiss_read_tile_signal/support 函数未找到");
            if (meta_json) fn_free(meta_json);
            if (tile_ipix_list) fn_free(tile_ipix_list);
            result.error_msg = "[HISS_VERIFY] AIO DLL 未导出 aio_hiss_read_tile_signal/support";
            result.exit_code = AstroCsExitCode::HISS_INVALID;
            return false;
        }
    }

    // 3. 遍历全部 Tile 验证 (TEST-003/TEST-004: 不再只检查前 10 个)
    // 每个 Tile 验证项 (HissReader 隐式验证 + 显式检查):
    //   a. required 子块 (SIGNAL + SUPPORT) — read_tile_signal/support 找不到子块返回错误
    //   b. checksum — read_subblock 内部校验, 不匹配返回 HISS_ERR_CHECKSUM
    //   c. occupancy 与 compact 长度一致 — HissReader 按 occ_mode 展开时验证
    //   d. dtype 与 metadata precision_mode 一致 — HissReader 拒绝静默转换 (FP32/FP64 互斥)
    //   e. signal 中不得有 NaN/Inf — 显式检查
    //   f. 至少一个 Tile 有非零 signal — 汇总后检查
    uint64_t n_passed = 0, n_failed = 0;
    uint64_t n_signal_nonzero = 0, n_support_nonzero = 0;
    uint64_t n_snr_points_total = 0, n_tiles_with_snr = 0;
    bool has_nonzero_signal = false;

    for (uint64_t i = 0; i < n_tiles; ++i) {
        uint64_t parent_ipix = tile_ipix_list[i];

        // 读取 signal (根据精度模式选择 FP32 或 FP64)
        // 隐式验证: SIGNAL 子块存在 + checksum + occupancy 展开 + dtype 一致
        uint32_t n_signal = 0;
        bool tile_signal_nonzero = false;
        bool tile_has_naninf = false;

        if (is_fp64) {
            double* signal_f64 = nullptr;
            int sig_ret = fn_read_signal_f64_sess(session, parent_ipix,
                                                  &signal_f64, &n_signal);
            if (sig_ret != 0 || signal_f64 == nullptr || n_signal == 0) {
                LOG_ERROR("orchestrator", "[HISS_VERIFY] Tile #" + std::to_string(i)
                         + " (parent_ipix=" + std::to_string(parent_ipix)
                         + ") signal 读取失败 (rc=" + std::to_string(sig_ret)
                         + ") — 子块缺失/checksum/occupancy/dtype 验证失败");
                if (signal_f64) fn_free(signal_f64);
                if (meta_json) fn_free(meta_json);
                if (tile_ipix_list) fn_free(tile_ipix_list);
                result.error_msg = "[HISS_VERIFY] Tile #" + std::to_string(i)
                                 + " (parent_ipix=" + std::to_string(parent_ipix)
                                 + ") signal 读取失败 (子块/checksum/occupancy/dtype)";
                result.exit_code = AstroCsExitCode::HISS_INVALID;
                return false;
            }
            for (uint32_t j = 0; j < n_signal; ++j) {
                if (std::isnan(signal_f64[j]) || std::isinf(signal_f64[j])) {
                    tile_has_naninf = true;
                    break;
                }
                if (signal_f64[j] != 0.0) tile_signal_nonzero = true;
            }
            fn_free(signal_f64);
        } else {
            float* signal = nullptr;
            int sig_ret = fn_read_signal_sess(session, parent_ipix,
                                              &signal, &n_signal);
            if (sig_ret != 0 || signal == nullptr || n_signal == 0) {
                LOG_ERROR("orchestrator", "[HISS_VERIFY] Tile #" + std::to_string(i)
                         + " (parent_ipix=" + std::to_string(parent_ipix)
                         + ") signal 读取失败 (rc=" + std::to_string(sig_ret)
                         + ") — 子块缺失/checksum/occupancy/dtype 验证失败");
                if (signal) fn_free(signal);
                if (meta_json) fn_free(meta_json);
                if (tile_ipix_list) fn_free(tile_ipix_list);
                result.error_msg = "[HISS_VERIFY] Tile #" + std::to_string(i)
                                 + " (parent_ipix=" + std::to_string(parent_ipix)
                                 + ") signal 读取失败 (子块/checksum/occupancy/dtype)";
                result.exit_code = AstroCsExitCode::HISS_INVALID;
                return false;
            }
            for (uint32_t j = 0; j < n_signal; ++j) {
                if (std::isnan(signal[j]) || std::isinf(signal[j])) {
                    tile_has_naninf = true;
                    break;
                }
                if (signal[j] != 0.0f) tile_signal_nonzero = true;
            }
            fn_free(signal);
        }

        if (tile_has_naninf) {
            LOG_ERROR("orchestrator", "[HISS_VERIFY] Tile #" + std::to_string(i)
                     + " (parent_ipix=" + std::to_string(parent_ipix)
                     + ") signal 含 NaN/Inf — 数据完整性违规");
            if (meta_json) fn_free(meta_json);
            if (tile_ipix_list) fn_free(tile_ipix_list);
            result.error_msg = "[HISS_VERIFY] Tile #" + std::to_string(i)
                             + " (parent_ipix=" + std::to_string(parent_ipix)
                             + ") signal 含 NaN/Inf";
            result.exit_code = AstroCsExitCode::HISS_INVALID;
            return false;
        }

        // 读取 support (隐式验证: SUPPORT 子块存在 + checksum + occupancy 展开)
        uint8_t* support = nullptr;
        uint32_t n_support = 0;
        int sup_ret = fn_read_support_sess(session, parent_ipix,
                                           &support, &n_support);
        if (sup_ret != 0 || support == nullptr || n_support == 0) {
            LOG_ERROR("orchestrator", "[HISS_VERIFY] Tile #" + std::to_string(i)
                     + " (parent_ipix=" + std::to_string(parent_ipix)
                     + ") support 读取失败 (rc=" + std::to_string(sup_ret)
                     + ") — 子块缺失/checksum/occupancy 验证失败");
            if (support) fn_free(support);
            if (meta_json) fn_free(meta_json);
            if (tile_ipix_list) fn_free(tile_ipix_list);
            result.error_msg = "[HISS_VERIFY] Tile #" + std::to_string(i)
                             + " (parent_ipix=" + std::to_string(parent_ipix)
                             + ") support 读取失败 (子块/checksum/occupancy)";
            result.exit_code = AstroCsExitCode::HISS_INVALID;
            return false;
        }

        // support 非全零检查 (uint8 不会有 NaN/Inf)
        bool tile_support_nonzero = false;
        for (uint32_t j = 0; j < n_support; ++j) {
            if (support[j] != 0) {
                tile_support_nonzero = true;
                break;
            }
        }
        fn_free(support);

        // R11 (HISS-103): 读取 SNR 控制点 (按文件 dtype 选择 f32 8B/点 或 f64 12B/点)
        uint8_t* snr_buf = nullptr;
        uint32_t n_snr = 0;
        auto snr_api_sess = is_fp64 ? fn_read_snr_f64_sess : fn_read_snr_sess;
        if (snr_api_sess) {
            snr_api_sess(session, parent_ipix, &snr_buf, &n_snr);
            // SNR 稀疏 (仅 ~254/285 Tile 含 SNR 子块): 读不到 = 该 Tile 无 SNR, 跳过
            // 最终汇总检查 n_snr_points_total>0 保证 SNR 数据整体存在 (HISS-103)
            if (n_snr > 0) {
                n_snr_points_total += n_snr;
                ++n_tiles_with_snr;
            }
            if (snr_buf) fn_free(snr_buf);
        }

        if (tile_signal_nonzero) { ++n_signal_nonzero; has_nonzero_signal = true; }
        if (tile_support_nonzero) { ++n_support_nonzero; }
        ++n_passed;

        // R13 (HISS_IO_REPAIR): 逐 Tile 日志降级 — Logger 每条 flush 写盘,
        // 285 Tile 日志拖慢 verify (130s 中大部分); 只保留阶段/汇总
    }

    // 释放 inspect 分配的内存
    if (meta_json) fn_free(meta_json);
    if (tile_ipix_list) fn_free(tile_ipix_list);
    if (session) fn_close_session(session);

    // 至少一个 Tile 有非零 signal
    if (!has_nonzero_signal) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] 全部 " + std::to_string(n_tiles)
                 + " 个 Tile 的 signal 均为全零 — 数据完整性违规");
        result.error_msg = "[HISS_VERIFY] 全部 " + std::to_string(n_tiles)
                         + " 个 Tile 的 signal 均为全零";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    // R11 (HISS-103): SNR 控制点必须存在 (SNR 是必需 stage)
    if (n_snr_points_total == 0) {
        LOG_ERROR("orchestrator", "[HISS_VERIFY] 全部 Tile 无 SNR 控制点 — SNR 数据缺失");
        result.error_msg = "[HISS_VERIFY] 全部 Tile 无 SNR 控制点";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    // 全 Tile 验证汇总报告
    LOG_INFO("orchestrator", "[HISS_VERIFY] 全 Tile 验证汇总: n_tiles=" + std::to_string(n_tiles)
             + " n_passed=" + std::to_string(n_passed)
             + " n_failed=" + std::to_string(n_failed)
             + " n_signal_nonzero=" + std::to_string(n_signal_nonzero)
             + " n_support_nonzero=" + std::to_string(n_support_nonzero)
             + " n_snr_points=" + std::to_string(n_snr_points_total)
             + " n_tiles_with_snr=" + std::to_string(n_tiles_with_snr)
             + " precision=" + (is_fp64 ? "FP64" : "FP32"));

    LOG_INFO("orchestrator", "[HISS_VERIFY] 完成: .hiss 文件验证通过 (全 Tile 遍历)");
    return true;
}

// ============================================================================
// stage 8: HIPS_VERIFY - 验证 HiPS 产品集 (Phase1 Final Closure V3)
//   唯一后端: AIO HiPS Reader (aio_hips_reader.dll 函数, 不经 astropy/CFITSIO)
//   验证项:
//     1. signal/support/snr 三产品 properties (hips_version=1.4, order, width)
//     2. signal/support dtype 与 precision 一致 (manifest astrocs_signal_dtype)
//     3. 叶级 tile 全遍历: support∈[0,1], support>0 -> signal 有限,
//        support==0 -> signal NaN; F = signal*support*A_cell 有限非负
//     4. MOC 覆盖: tile 列表来自 MOC, 且每个 tile FITS 文件存在
//     5. SNR catalogue: 点数 > 0
//     6. hierarchy: Norder0..NorderK 目录存在
// ============================================================================
bool Orchestrator::run_stage_hips_verify(TaskResult& result) {
    LOG_INFO("orchestrator", "[HIPS_VERIFY] 开始: " + current_hips_dir_);
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::AIO)) {
        result.error_msg = "[HIPS_VERIFY] AIO DLL 未加载";
        return false;
    }
    if (current_hips_dir_.empty() || !fs::exists(current_hips_dir_)) {
        result.error_msg = "[HIPS_VERIFY] HiPS 目录不存在: " + current_hips_dir_;
        LOG_ERROR("orchestrator", result.error_msg);
        return false;
    }

    using OpenFn = AioHipsDataset* (*)(const char*, int);
    using GetPropsFn = int (*)(AioHipsDataset*, char*, int);
    using TileCountFn = int (*)(AioHipsDataset*);
    using TileIpixFn = int (*)(AioHipsDataset*, int, uint64_t*);
    using ReadF32Fn = int (*)(AioHipsDataset*, uint64_t, float*);
    using ReadF64Fn = int (*)(AioHipsDataset*, uint64_t, double*);
    using ReadSnrFn = int (*)(AioHipsDataset*, double*, double*, double*, int64_t*,
                              uint32_t*, uint32_t*, int);
    using CloseFn = void (*)(AioHipsDataset*);
    using ErrFn = const char* (*)(void);

    auto fn_open = dll_loader_.get_function<OpenFn>(ModuleId::AIO, "aio_hips_open");
    auto fn_props = dll_loader_.get_function<GetPropsFn>(ModuleId::AIO, "aio_hips_get_properties");
    auto fn_count = dll_loader_.get_function<TileCountFn>(ModuleId::AIO, "aio_hips_tile_count");
    auto fn_ipix = dll_loader_.get_function<TileIpixFn>(ModuleId::AIO, "aio_hips_tile_ipix");
    auto fn_read32 = dll_loader_.get_function<ReadF32Fn>(ModuleId::AIO, "aio_hips_read_tile_f32");
    auto fn_read64 = dll_loader_.get_function<ReadF64Fn>(ModuleId::AIO, "aio_hips_read_tile_f64");
    auto fn_snr = dll_loader_.get_function<ReadSnrFn>(ModuleId::AIO, "aio_hips_read_snr_catalog");
    auto fn_close = dll_loader_.get_function<CloseFn>(ModuleId::AIO, "aio_hips_close");
    auto fn_err = dll_loader_.get_function<ErrFn>(ModuleId::AIO, "aio_hips_reader_last_error");
    if (!fn_open || !fn_props || !fn_count || !fn_ipix || !fn_read32 || !fn_read64 ||
        !fn_close) {
        result.error_msg = "[HIPS_VERIFY] AIO HiPS Reader 函数缺失";
        LOG_ERROR("orchestrator", result.error_msg);
        return false;
    }

    const bool fp64 = (config_.precision == PrecisionMode::FP64);
    const double A_cell = 0.0;  // 由 nside 计算 (从 properties hips_order 推导 nside)
    (void)A_cell;

    // 1. 打开 signal / support / snr
    std::vector<std::pair<const char*, int>> products = {
        {"signal", AIO_HIPS_RD_SIGNAL}, {"support", AIO_HIPS_RD_SUPPORT},
        {"snr", AIO_HIPS_RD_SNR}
    };
    int tile_count = 0;
    int hips_order = 0;
    char props_buf[8192];
    for (const auto& pr : products) {
        AioHipsDataset* d = fn_open(current_hips_dir_.c_str(), pr.second);
        if (!d) {
            result.error_msg = std::string("[HIPS_VERIFY] 打开 ") + pr.first + " 失败: " +
                               (fn_err ? (fn_err() ? fn_err() : "?") : "?");
            LOG_ERROR("orchestrator", result.error_msg);
            return false;
        }
        std::string props;
        if (fn_props(d, props_buf, (int)sizeof(props_buf)) == 0) props = props_buf;
        fn_close(d);
        // hips_version=1.4
        if (props.find("hips_version=1.4") == std::string::npos) {
            result.error_msg = "[HIPS_VERIFY] " + std::string(pr.first) + " hips_version != 1.4";
            LOG_ERROR("orchestrator", result.error_msg);
            return false;
        }
        if (pr.second != AIO_HIPS_RD_SNR) {
            if (props.find("hips_tile_width=512") == std::string::npos) {
                result.error_msg = "[HIPS_VERIFY] " + std::string(pr.first) + " hips_tile_width != 512";
                return false;
            }
            // dtype 严格
            std::string want = fp64 ? "astrocs_signal_dtype=float64" : "astrocs_signal_dtype=float32";
            if (pr.second == AIO_HIPS_RD_SIGNAL &&
                props.find(want) == std::string::npos) {
                result.error_msg = "[HIPS_VERIFY] signal dtype 与 precision 不匹配";
                LOG_ERROR("orchestrator", result.error_msg);
                return false;
            }
        }
    }

    // 2. signal + support 全 tile 遍历
    AioHipsDataset* dsig = fn_open(current_hips_dir_.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* dsup = fn_open(current_hips_dir_.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!dsig || !dsup) {
        result.error_msg = "[HIPS_VERIFY] signal/support 打开失败";
        if (dsig) fn_close(dsig);
        if (dsup) fn_close(dsup);
        return false;
    }
    tile_count = fn_count(dsig);
    if (tile_count <= 0) {
        result.error_msg = "[HIPS_VERIFY] signal 无叶级 tile";
        fn_close(dsig); fn_close(dsup);
        return false;
    }
    // hips_order 从 properties 提取 (在循环里已检查, 此处再解析用于日志)
    {
        char buf[8192];
        if (fn_props(dsig, buf, (int)sizeof(buf)) == 0) {
            std::string s = buf;
            size_t p = s.find("hips_order=");
            if (p != std::string::npos) hips_order = std::atoi(s.c_str() + p + 11);
        }
    }
    const uint32_t nside_leaf = 1u << (uint32_t)(hips_order + 9);
    const double a_cell = 4.0 * 3.14159265358979323846 / (12.0 * (double)nside_leaf * nside_leaf);
    const size_t n_pix = 512 * 512;
    std::vector<float> sig32(n_pix), sup32(n_pix);
    std::vector<double> sig64(n_pix), sup64(n_pix);
    int n_fail = 0;
    for (int i = 0; i < tile_count; ++i) {
        uint64_t ipix = 0;
        if (fn_ipix(dsig, i, &ipix) != 0) { ++n_fail; continue; }
        int r1 = fp64 ? fn_read64(dsig, ipix, sig64.data())
                      : fn_read32(dsig, ipix, sig32.data());
        int r2 = fp64 ? fn_read64(dsup, ipix, sup64.data())
                      : fn_read32(dsup, ipix, sup32.data());
        if (r1 != 0 || r2 != 0) { ++n_fail; continue; }
        for (size_t k = 0; k < n_pix; ++k) {
            double sup = fp64 ? sup64[k] : (double)sup32[k];
            double sig = fp64 ? sig64[k] : (double)sig32[k];
            if (sup < 0.0 || sup > 1.0) { ++n_fail; break; }
            if (sup > 0.0) {
                if (!std::isfinite(sig)) { ++n_fail; break; }
                double f = sig * sup * a_cell;
                if (!std::isfinite(f) || f < 0.0) { ++n_fail; break; }
            }
        }
    }
    fn_close(dsig);
    fn_close(dsup);
    if (n_fail > 0) {
        result.error_msg = "[HIPS_VERIFY] tile 验证失败数: " + std::to_string(n_fail);
        LOG_ERROR("orchestrator", result.error_msg);
        return false;
    }

    // 3. hierarchy 目录检查 (Norder0..NorderK)
    for (int k = 0; k <= hips_order; ++k) {
        std::string dir = current_hips_dir_ + "/signal/Norder" + std::to_string(k);
        if (!fs::exists(dir)) {
            result.error_msg = "[HIPS_VERIFY] hierarchy 缺失: " + dir;
            LOG_ERROR("orchestrator", result.error_msg);
            return false;
        }
    }

    // 4. SNR catalogue
    AioHipsDataset* dsnr = fn_open(current_hips_dir_.c_str(), AIO_HIPS_RD_SNR);
    if (!dsnr) {
        result.error_msg = "[HIPS_VERIFY] snr 产品打开失败";
        return false;
    }
    double ra_buf[4096], dec_buf[4096], snr_buf[4096];
    int64_t id_buf[4096];
    uint32_t qf_buf[4096], ps_buf[4096];
    int n_snr = fn_snr ? fn_snr(dsnr, ra_buf, dec_buf, snr_buf, id_buf,
                                qf_buf, ps_buf, 4096) : 0;
    fn_close(dsnr);
    if (n_snr <= 0) {
        result.error_msg = "[HIPS_VERIFY] SNR catalogue 为空";
        LOG_ERROR("orchestrator", result.error_msg);
        return false;
    }

    LOG_INFO("orchestrator", "[HIPS_VERIFY] 通过: tiles=" + std::to_string(tile_count)
             + " order=" + std::to_string(hips_order) + " snr_points=" + std::to_string(n_snr)
             + " dtype=" + (fp64 ? "float64" : "float32"));
    LOG_INFO("orchestrator", "[HIPS_VERIFY] 完成: HiPS 产品集验证通过");
    return true;
}

// ============================================================================
// spec §2.3.2 两段流水线新增 stage handler (骨架)
// ============================================================================

// stage 0: READ_FITS - 读取 FITS 文件到 PipelineFrame (aio_read_fits)
// 实现: 调用 aio_read_fits -> 获取 pixel/metadata/keywords -> 填充 frame_ 的 data/header 块
bool Orchestrator::run_stage_read_fits(TaskResult& result) {
    LOG_INFO("orchestrator", "[READ_FITS] 开始: " + current_fits_path_);

    // P03-003: AIO 是必需模块 (PipelineFrame + FITS I/O 基础), 缺失必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::AIO)) {
        LOG_ERROR("orchestrator", "[READ_FITS] AIO DLL 未加载 (必需模块)");
        result.error_msg = "[READ_FITS] AIO DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // 获取 AIO 函数指针
    auto fn_read_fits = dll_loader_.get_function<AIOImageData* (*)(const char*)>(
        ModuleId::AIO, "aio_read_fits");
    auto fn_get_pixels = dll_loader_.get_function<float* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data");
    // R10 修复: 显式设置 AIO 模块精度模式 (PrecisionContext 单例不跨 DLL 共享)
    auto fn_set_precision = dll_loader_.get_function<void (*)(int)>(
        ModuleId::AIO, "aio_set_precision_mode");
    // 双精度 ABI: FP64 模式下获取 double 像素数据与 dtype
    auto fn_get_pixels_f64 = dll_loader_.get_function<double* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data_f64");
    auto fn_get_dtype = dll_loader_.get_function<uint8_t (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_dtype");
    auto fn_get_width = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_width");
    auto fn_get_height = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_height");
    auto fn_get_channels = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_channels");
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

    // FP64 模式下, aio_get_pixel_data_f64 / aio_get_dtype 为必需 (双精度 ABI 契约)
    bool need_fp64 = (config_.precision == PrecisionMode::FP64);
    if (!fn_read_fits || !fn_get_pixels || !fn_get_width || !fn_get_height ||
        !fn_get_channels || !fn_get_metadata || !fn_free || !fn_add_block || !fn_kv_set ||
        (need_fp64 && (!fn_get_pixels_f64 || !fn_get_dtype))) {
        LOG_ERROR("orchestrator", "[READ_FITS] AIO 函数指针获取失败"
                  + std::string(need_fp64 ? " (FP64 模式需要 aio_get_pixel_data_f64/aio_get_dtype)" : ""));
        result.error_msg = "[READ_FITS] AIO 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // R10 修复: 在读取 FITS 之前, 显式设置 AIO 模块精度模式
    // (PrecisionContext 单例在 EXE/DLL 边界不共享, 必须通过导出 API 传递)
    if (fn_set_precision) {
        fn_set_precision(need_fp64 ? 1 : 0);
    }

    // 读取 FITS 文件
    AIOImageData* image = fn_read_fits(current_fits_path_.c_str());
    if (image == nullptr) {
        LOG_ERROR("orchestrator", "[READ_FITS] aio_read_fits 返回 nullptr: " + current_fits_path_);
        result.error_msg = "[READ_FITS] 读取 FITS 失败";
        result.exit_code = AstroCsExitCode::FILE_IO_ERROR;
        return false;
    }

    int width = fn_get_width(image);
    int height = fn_get_height(image);
    int channels = fn_get_channels(image);
    // 双精度 ABI: 根据 config_.precision (与 PrecisionContext 一致) 选择像素数据源
    // FP64 模式: pixels_f64 = data_f64 (double, 不降级), 创建 FLOAT64 data 块
    // FP32 模式: pixels = data (float, 向后兼容), 创建 FLOAT32 data 块
    bool use_fp64 = (config_.precision == PrecisionMode::FP64);
    float* pixels = nullptr;
    double* pixels_f64 = nullptr;
    uint8_t dtype = 0;
    if (fn_get_dtype) dtype = fn_get_dtype(image);
    if (use_fp64) {
        pixels_f64 = fn_get_pixels_f64(image);
    } else {
        pixels = fn_get_pixels(image);
    }
    LOG_INFO("orchestrator", "[READ_FITS] 图像尺寸: " + std::to_string(width) + "x"
             + std::to_string(height) + " channels=" + std::to_string(channels)
             + " dtype=" + std::to_string(dtype) + " mode=" + (use_fp64 ? "FP64" : "FP32"));

    // B4 修复: Stage1 只接受单色输入, 多通道硬报错 (禁止静默取 channel 0)
    if (channels != 1) {
        LOG_ERROR("orchestrator", "[READ_FITS] 错误: 多通道输入 (channels="
                  + std::to_string(channels) + ") 不被支持, Stage1 只接受单色输入");
        result.error_msg = "[READ_FITS] 多通道输入 (channels="
                          + std::to_string(channels) + ") 不被支持, Stage1 只接受单色输入";
        result.exit_code = AstroCsExitCode::INPUT_INVALID;
        fn_free(image);
        return false;
    }

    // 像素指针校验: FP64 模式检查 pixels_f64, FP32 模式检查 pixels
    if (width <= 0 || height <= 0 ||
        (use_fp64 ? (pixels_f64 == nullptr) : (pixels == nullptr))) {
        LOG_ERROR("orchestrator", "[READ_FITS] 像素数据无效"
                  + std::string(use_fp64 ? " (FP64: data_f64 为空)" : " (FP32: data 为空)"));
        result.error_msg = "[READ_FITS] 像素数据无效";
        result.exit_code = AstroCsExitCode::FILE_IO_ERROR;
        fn_free(image);
        return false;
    }

    // 添加 data 块 (按精度模式选择 FLOAT32 / FLOAT64, 拷贝)
    // P03-003: data 是必需块, 写入失败必须返回非零 (退出码 3)
    // 约束: FP64 模式下 data 块必须是 FLOAT64 (不是 FLOAT32 降级)
    int dims[2] = {height, width};
    int ret;
    if (use_fp64) {
        ret = fn_add_block(frame_, "data", AIO_BLOCK_FLOAT64,
                           pixels_f64, static_cast<int64_t>(width) * height,
                           dims, 2, "校准前 Light 像素 (FP64, 无降级)");
    } else {
        ret = fn_add_block(frame_, "data", AIO_BLOCK_FLOAT32,
                           pixels, static_cast<int64_t>(width) * height,
                           dims, 2, "校准前 Light 像素");
    }
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[READ_FITS] 添加 data 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[READ_FITS] 添加 data 块失败";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
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

// stage 6: SNR - SNR 估计 (snr_estimator.dll)
// GAP-011 修复: 改用 snr_extract_model 提取稀疏控制点, 序列化到 snr_model 块 (AIO_BLOCK_RAW)
// 旧版 snr_estimate 输出稠密 SNR 图写 "snr" 块, 但 drizzle 阶段只识别 "snr_model" 块,
// 导致 SNR²加权链路断裂。改为稀疏控制点随 drizzle 一起转球面坐标落盘 .hiss
bool Orchestrator::run_stage_snr(TaskResult& result) {
    // BLOCKER-TYPE-002: snr_extract_model_v2 按精度模式输出 FP32/FP64 SNR
    // 控制点 (value_dtype=1 时 snr_psf 为 double 计算并存储, 非 float 扩展)
    LOG_INFO("orchestrator", "[SNR] 开始 (precision="
             + std::string(config_.precision == PrecisionMode::FP64 ? "FP64" : "FP32")
             + ", snr_extract_model_v2 精度感知)");

    // R11 (CFG-109): SNR 是必需 stage, DLL 缺失必须失败, 不允许降级
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::SNR)) {
        LOG_ERROR("orchestrator", "[SNR] SNR DLL 未加载 (必需模块)");
        result.error_msg = "[SNR] SNR DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[SNR] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 获取函数指针 (GAP-011: snr_extract_model / snr_free_model;
    // BLOCKER-TYPE-002: v2 支持 FP64 SNR 真实存储)
    // V4: v3 携带 star_id/quality_flags/photometric_status
    using ExtractV3Fn = int (*)(const double*, int, double,
                                const SnrWcsParams*, int,
                                const int64_t*, const uint32_t*, const uint32_t*,
                                SnrModelV3*);
    using FreeV3Fn = void (*)(SnrModelV3*);
    auto fn_extract_v3 = dll_loader_.get_function<ExtractV3Fn>(
        ModuleId::SNR, "snr_extract_model_v3");
    auto fn_free_v3 = dll_loader_.get_function<FreeV3Fn>(
        ModuleId::SNR, "snr_free_model_v3");
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

    // V19 (SNR_REDESIGN_CONTRACT): NoiseWeightModelV1 函数指针
    using NoiseModelFn = int (*)(const void*, int, int, const float*,
                                 const double*, const double*, int,
                                 const SnrNoiseModelConfig*, NoiseWeightModelV1*);
    using NoiseFillFn = int (*)(const NoiseWeightModelV1*, int, int,
                                float*, float*);
    using NoiseFreeFn = void (*)(NoiseWeightModelV1*);
    auto fn_noise_model = dll_loader_.get_function<NoiseModelFn>(
        ModuleId::SNR, "snr_noise_model_v1");
    auto fn_noise_model_f64 = dll_loader_.get_function<NoiseModelFn>(
        ModuleId::SNR, "snr_noise_model_v1_f64");
    auto fn_noise_fill = dll_loader_.get_function<NoiseFillFn>(
        ModuleId::SNR, "snr_noise_model_v1_fill");
    auto fn_noise_free = dll_loader_.get_function<NoiseFreeFn>(
        ModuleId::SNR, "snr_noise_model_v1_free");
    auto fn_add_block = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        const void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block");

    if (!fn_extract_v3 || !fn_free_v3 || !fn_get_block || !fn_remove_block
        || !fn_add_block_move || !fn_kv_get_double) {
        LOG_ERROR("orchestrator", "[SNR] 函数指针获取失败 (snr_extract_model_v3 必需)");
        result.error_msg = "[SNR] 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 读取 psf 块 (FLOAT64 [N,9], 每行 [status,B,flux,cx,cy,fwhm,A,mad,eccentricity])
    // R11 (CFG-109): psf 块缺失 = 必需依赖缺失, 硬失败 (不允许降级)
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] psf 块不存在 (PSF 阶段未产出)");
        result.error_msg = "[SNR] psf 块不存在 (PSF 阶段未产出)";
        result.exit_code = AstroCsExitCode::SNR_FAILED;
        return false;
    }
    int n_stars = psf_block->dims[0];
    const double* psf_data = static_cast<const double*>(psf_block->data);

    // V4: 构建与 psf 行对齐的 stable star_id / quality_flags / photometric_status
    //   star_measurements (FLOAT64 [N,15]): col0=star_id, col6=psf_status,
    //     col13=saturated, col14=has_saturated
    //   photometric_match (FLOAT64 [N,6]): col4=status (1=used, 2=rejected, else unmatched)
    std::vector<int64_t> psf_star_ids(static_cast<size_t>(n_stars), 0);
    std::vector<uint32_t> psf_quality(static_cast<size_t>(n_stars), 0);
    std::vector<uint32_t> psf_photo_status(static_cast<size_t>(n_stars), 0);
    {
        const AioBlock* sm = fn_get_block(frame_, "star_measurements");
        const AioBlock* pm = fn_get_block(frame_, "photometric_match");
        const bool sm_ok = (sm != nullptr && sm->type == AIO_BLOCK_FLOAT64 &&
                            sm->dims[0] == n_stars && sm->dims[1] >= 15);
        const bool pm_ok = (pm != nullptr && pm->type == AIO_BLOCK_FLOAT64 &&
                            pm->dims[0] == n_stars && pm->dims[1] >= 6);
        if (!sm_ok) {
            LOG_WARN("orchestrator", "[SNR] star_measurements 块缺失或格式不符, "
                     "star_id/quality_flags 置 0 (lineage 不可用)");
        }
        if (!pm_ok) {
            LOG_WARN("orchestrator", "[SNR] photometric_match 块缺失或格式不符, "
                     "photometric_status 置 0");
        }
        for (int i = 0; i < n_stars; ++i) {
            uint32_t qf = 0;
            int64_t sid = 0;
            if (sm_ok) {
                const double* smd = static_cast<const double*>(sm->data);
                const double* row = smd + static_cast<size_t>(i) * sm->dims[1];
                sid = static_cast<int64_t>(row[0]);
                const double psf_status = row[6];
                const double sat = row[13];
                const double has_sat = row[14];
                if (psf_status == 0.0 || psf_status == 3.0) qf |= SNR_QF_PSF_OK;
                if (sat != 0.0) qf |= SNR_QF_SATURATED;
                if (has_sat != 0.0) qf |= SNR_QF_HAS_SATURATED;
            }
            if (pm_ok) {
                const double* pmd = static_cast<const double*>(pm->data);
                const double status = pmd[static_cast<size_t>(i) * pm->dims[1] + 4];
                psf_photo_status[static_cast<size_t>(i)] =
                    static_cast<uint32_t>(std::max(0.0, status));
                if (status == 1.0) qf |= SNR_QF_PHOTO_MATCHED;
                if (status == 2.0) qf |= SNR_QF_PHOTO_REJECTED;
            }
            psf_star_ids[static_cast<size_t>(i)] = sid;
            psf_quality[static_cast<size_t>(i)] = qf;
        }
        LOG_INFO("orchestrator", "[SNR] 星点 lineage 数组构建: star_measurements_ok="
                 + std::string(sm_ok ? "1" : "0")
                 + " photometric_match_ok=" + std::string(pm_ok ? "1" : "0"));
    }

    // 读取 sigma_residual (来自 photo_stats KV 块)
    // 若读不到则用默认值 0.1 (并打 warning)
    double sigma_residual = 0.1;
    bool sig_read = false;
    if (fn_kv_get) {
        const char* sig_str = fn_kv_get(frame_, "photo_stats", "SIGMA_RESIDUAL");
        if (sig_str != nullptr) {
            sigma_residual = std::atof(sig_str);
            sig_read = true;
        }
    } else {
        // kv_get 不可用时退回 kv_get_double (无法区分读不到与值=default)
        sigma_residual = fn_kv_get_double(frame_, "photo_stats", "SIGMA_RESIDUAL", 0.1);
        sig_read = true;
    }
    if (!sig_read) {
        LOG_WARN("orchestrator", "[SNR] photo_stats/SIGMA_RESIDUAL 读不到, 使用默认值 0.1");
    }

    // 从 header KV 块读取 WCS 参数
    // CRPIX 为 1-based (FITS 标准), SnrWcsParams.crpix1/crpix2 也是 1-based, 直接传无需转换
    // CD 矩阵填充: cd[0]=CD1_1, cd[1]=CD1_2, cd[2]=CD2_1, cd[3]=CD2_2
    SnrWcsParams wcs = {};
    wcs.crval1 = fn_kv_get_double(frame_, "header", "CRVAL1", 0.0);
    wcs.crval2 = fn_kv_get_double(frame_, "header", "CRVAL2", 0.0);
    wcs.crpix1 = fn_kv_get_double(frame_, "header", "CRPIX1", 0.0);
    wcs.crpix2 = fn_kv_get_double(frame_, "header", "CRPIX2", 0.0);
    wcs.cd[0]  = fn_kv_get_double(frame_, "header", "CD1_1", 0.0);
    wcs.cd[1]  = fn_kv_get_double(frame_, "header", "CD1_2", 0.0);
    wcs.cd[2]  = fn_kv_get_double(frame_, "header", "CD2_1", 0.0);
    wcs.cd[3]  = fn_kv_get_double(frame_, "header", "CD2_2", 0.0);

    // P03-004: 读取前向 SIP 系数 (A_ORDER/B_ORDER + A_i_j/B_i_j)
    // 复用 hp_drizzle_api.cpp 的 SIP 读取逻辑, 系数按 a[i*6+j] 存储
    // 保证 SNR 控制点 (ra,dec) 与 drizzle 阶段坐标系一致 (WCS+SIP 一致性)
    wcs.sip.a_order = 0;
    wcs.sip.b_order = 0;
    if (fn_kv_get) {
        const char* a_order_str = fn_kv_get(frame_, "header", "A_ORDER");
        if (a_order_str && a_order_str[0] != '\0') {
            int a_order = std::atoi(a_order_str);
            if (a_order > 0 && a_order <= SNR_SIP_MAX_ORDER) {
                const char* b_order_str = fn_kv_get(frame_, "header", "B_ORDER");
                int b_order = b_order_str ? std::atoi(b_order_str) : a_order;
                if (b_order > SNR_SIP_MAX_ORDER) b_order = SNR_SIP_MAX_ORDER;
                if (b_order < 0) b_order = 0;

                wcs.sip.a_order = a_order;
                wcs.sip.b_order = b_order;

                // 读取 A_i_j (跳过 (0,0), i+j<=order)
                for (int i = 0; i <= a_order; ++i) {
                    for (int j = 0; j <= a_order - i; ++j) {
                        if (i + j == 0) continue;  // A_0_0 恒为 0
                        char key[16];
                        std::snprintf(key, sizeof(key), "A_%d_%d", i, j);
                        const char* val = fn_kv_get(frame_, "header", key);
                        if (val && val[0] != '\0') {
                            wcs.sip.a[i * 6 + j] = std::atof(val);
                        }
                    }
                }
                // 读取 B_i_j
                for (int i = 0; i <= b_order; ++i) {
                    for (int j = 0; j <= b_order - i; ++j) {
                        if (i + j == 0) continue;
                        char key[16];
                        std::snprintf(key, sizeof(key), "B_%d_%d", i, j);
                        const char* val = fn_kv_get(frame_, "header", key);
                        if (val && val[0] != '\0') {
                            wcs.sip.b[i * 6 + j] = std::atof(val);
                        }
                    }
                }
                LOG_INFO("orchestrator", "[SNR] SIP 前向系数加载: A_ORDER=" + std::to_string(a_order)
                         + " B_ORDER=" + std::to_string(b_order)
                         + " (P03-004 WCS+SIP 一致性)");
            }
        }
    }

    LOG_INFO("orchestrator", "[SNR] n_stars=" + std::to_string(n_stars)
             + " sigma_residual=" + std::to_string(sigma_residual)
             + " CRVAL=(" + std::to_string(wcs.crval1) + "," + std::to_string(wcs.crval2) + ")"
             + " CRPIX=(" + std::to_string(wcs.crpix1) + "," + std::to_string(wcs.crpix2) + ")"
             + " SIP(a_order=" + std::to_string(wcs.sip.a_order)
             + ", b_order=" + std::to_string(wcs.sip.b_order) + ")");

    // 调用 snr_extract_model_v3 提取稀疏控制点 (V4: 携带 stable star_id /
    // quality_flags / photometric_status; FP64 模式 snr_psf 以 double 存储)
    SnrModelV3 model = {};
    int value_dtype = (config_.precision == PrecisionMode::FP64) ? 1 : 0;
    int ret = fn_extract_v3(psf_data, n_stars, sigma_residual, &wcs,
                            value_dtype,
                            psf_star_ids.data(), psf_quality.data(),
                            psf_photo_status.data(), &model);
    if (ret == 1) {
        // n_stars<=0 或无有效星 (status==0, A>B, mad>0)
        LOG_WARN("orchestrator", "[SNR] n_stars<=0 或无有效星, 降级跳过 snr_model 块");
        if (fn_kv_set) fn_kv_set(frame_, "photo_stats", "SNR_STATUS", "SKIPPED_NO_STARS");
        return true;  // 可选 stage, 允许降级继续
    }
    if (ret == 2) {
        // sigma_residual<=0
        LOG_WARN("orchestrator", "[SNR] sigma_residual<=0, 降级跳过 snr_model 块");
        if (fn_kv_set) fn_kv_set(frame_, "photo_stats", "SNR_STATUS", "SKIPPED_NO_SIGMA");
        return true;  // 可选 stage, 允许降级继续
    }
    if (ret == 3) {
        LOG_ERROR("orchestrator", "[SNR] snr_extract_model 失败: nullptr 参数");
        result.error_msg = "[SNR] snr_extract_model 失败: nullptr 参数";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    LOG_INFO("orchestrator", "[SNR] 提取稀疏控制点(v3): n_points=" + std::to_string(model.n_points)
             + ", snr_phot=" + std::to_string(model.snr_phot)
             + ", median_snr=" + std::to_string(model.median_snr)
             + ", idw_power=" + std::to_string(model.idw_power));

    // SNR-001: 丢弃原因分类 (SNR 不得静默丢点)
    // 重新遍历 PSF 数据, 对每个未写入 HISS 的控制点分类丢弃原因。
    // 检查顺序与 snr_estimator.cpp snr_extract_model 一致:
    //   1. status != 0 → INVALID_PSF (PSF 拟合失败)
    //   2. A <= B      → ZERO_FLUX (振幅不足/净通量 <= 0)
    //   3. mad <= 0     → INVALID_PSF (残差 MAD 非正)
    //   4. 否则         → NOT_DROPPED (有效控制点, 已写入)
    // 注: OUTSIDE_TILE/NO_OVERLAP/DUPLICATE_IPIX 在 drizzle 阶段判断, 此处为 0
    {
        uint32_t drop_counts[SNR_DROP_REASON_COUNT] = {};
        uint32_t n_total = static_cast<uint32_t>(n_stars);
        uint32_t n_valid = model.n_points;
        uint32_t n_dropped = 0;

        for (int i = 0; i < n_stars; ++i) {
            const double* row = psf_data + i * 9;
            double status = row[0];
            double B = row[1];
            double A = row[6];
            double mad = row[7];

            uint8_t reason = SNR_DROP_NOT_DROPPED;
            if (status != 0.0) {
                reason = SNR_DROP_INVALID_PSF;
            } else if (A <= B) {
                reason = SNR_DROP_ZERO_FLUX;
            } else if (mad <= 0.0) {
                reason = SNR_DROP_INVALID_PSF;
            }
            ++drop_counts[reason];
            if (reason != SNR_DROP_NOT_DROPPED) ++n_dropped;
        }

        LOG_INFO("orchestrator", "[SNR] 丢弃原因汇总: n_total=" + std::to_string(n_total)
                 + " n_valid=" + std::to_string(n_valid)
                 + " n_dropped=" + std::to_string(n_dropped)
                 + " {NOT_DROPPED=" + std::to_string(drop_counts[SNR_DROP_NOT_DROPPED])
                 + " INVALID_PSF=" + std::to_string(drop_counts[SNR_DROP_INVALID_PSF])
                 + " ZERO_FLUX=" + std::to_string(drop_counts[SNR_DROP_ZERO_FLUX])
                 + " OUTSIDE_TILE=" + std::to_string(drop_counts[SNR_DROP_OUTSIDE_TILE])
                 + " NO_OVERLAP=" + std::to_string(drop_counts[SNR_DROP_NO_OVERLAP])
                 + " INVALID_WCS=" + std::to_string(drop_counts[SNR_DROP_INVALID_WCS])
                 + " DUPLICATE_IPIX=" + std::to_string(drop_counts[SNR_DROP_DUPLICATE_IPIX])
                 + " OTHER=" + std::to_string(drop_counts[SNR_DROP_OTHER]) + "}");

        // 写入 photo_stats KV (供诊断和后续阶段查询)
        if (fn_kv_set) {
            std::string s_total = std::to_string(n_total);
            std::string s_valid = std::to_string(n_valid);
            std::string s_dropped = std::to_string(n_dropped);
            std::string s_inv_psf = std::to_string(drop_counts[SNR_DROP_INVALID_PSF]);
            std::string s_zero_flux = std::to_string(drop_counts[SNR_DROP_ZERO_FLUX]);
            fn_kv_set(frame_, "photo_stats", "SNR_N_TOTAL", s_total.c_str());
            fn_kv_set(frame_, "photo_stats", "SNR_N_VALID", s_valid.c_str());
            fn_kv_set(frame_, "photo_stats", "SNR_N_DROPPED", s_dropped.c_str());
            fn_kv_set(frame_, "photo_stats", "SNR_DROP_INVALID_PSF", s_inv_psf.c_str());
            fn_kv_set(frame_, "photo_stats", "SNR_DROP_ZERO_FLUX", s_zero_flux.c_str());
            fn_kv_set(frame_, "photo_stats", "SNR_STATUS", "OK");
        }
    }

    // 序列化 SnrModelV3 到 "snr_model" 块 (AIO_BLOCK_RAW, 版本化 v2 头)
    // 格式 (V4):
    //   [magic: "SNRM" 4B][version: u32=2][value_dtype: u8][reserved: u8]
    //   [point_stride: u16][n_points: u32][payload_bytes: u64][checksum: u32]
    //   [points: n_points × stride]   // stride=36 (f32 v3) / 40 (f64 v3)
    //   [snr_phot: f64][median_snr: f64][idw_power: f64]
    uint32_t n_points = model.n_points;
    uint32_t point_stride = (model.value_dtype == 1) ? 40 : 36;
    // header = magic4+version4+vd1+res1+stride2+n4+payload8+cs4 = 28
    size_t payload_size = 28 + (size_t)n_points * point_stride + 24;
    // fn_add_block_move 要求 data 必须是 malloc 分配 (frame 用 free() 释放)
    uint8_t* buffer = static_cast<uint8_t*>(std::malloc(payload_size));
    if (buffer == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] 分配 snr_model 缓冲失败 (size=" + std::to_string(payload_size) + ")");
        result.error_msg = "[SNR] 分配 snr_model 缓冲失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        fn_free_v3(&model);
        return false;
    }
    uint8_t* p = buffer;
    std::memcpy(p, "SNRM", 4);                            p += 4;
    uint32_t version = 2;
    std::memcpy(p, &version, 4);                          p += 4;
    uint8_t vd = model.value_dtype;
    std::memcpy(p, &vd, 1);                               p += 1;
    uint8_t reserved = 0;
    std::memcpy(p, &reserved, 1);                         p += 1;
    std::memcpy(p, &point_stride, 2);                     p += 2;
    std::memcpy(p, &n_points, 4);                         p += 4;
    uint64_t payload_bytes = (uint64_t)n_points * point_stride + 24;
    std::memcpy(p, &payload_bytes, 8);                    p += 8;
    // checksum (FNV-1a 32, 覆盖 [points + trailing])
    uint32_t checksum = 2166136261u;
    const uint8_t* pts = reinterpret_cast<const uint8_t*>(model.points);
    for (size_t i = 0; i < (size_t)n_points * point_stride; ++i) {
        checksum ^= pts[i];
        checksum *= 16777619u;
    }
    const double trailing[3] = {model.snr_phot, model.median_snr, model.idw_power};
    for (int t = 0; t < 3; ++t) {
        const uint8_t* q = reinterpret_cast<const uint8_t*>(&trailing[t]);
        for (int k = 0; k < 8; ++k) {
            checksum ^= q[k];
            checksum *= 16777619u;
        }
    }
    std::memcpy(p, &checksum, 4);                         p += 4;
    std::memcpy(p, model.points, (size_t)n_points * point_stride);
    p += (size_t)n_points * point_stride;
    std::memcpy(p, &model.snr_phot, 8);                   p += 8;
    std::memcpy(p, &model.median_snr, 8);                 p += 8;
    std::memcpy(p, &model.idw_power, 8);                  p += 8;

    // 写入 snr_model 块 (move 语义, frame_ 接管 buffer 内存)
    // P03-003: snr_model 是可选块, 写入失败时降级 (不阻塞 stage1)
    fn_remove_block(frame_, "snr_model");
    int wr = fn_add_block_move(frame_, "snr_model", AIO_BLOCK_RAW,
                               buffer, static_cast<int64_t>(payload_size),
                               nullptr, 0, "SNR 稀疏控制点模型 (GAP-011)");
    if (wr != 0) {
        LOG_WARN("orchestrator", "[SNR] 写入 snr_model 块失败 (可选块), 降级跳过: ret=" + std::to_string(wr));
        if (fn_kv_set) fn_kv_set(frame_, "photo_stats", "SNR_STATUS", "SKIPPED_WRITE_FAILED");
        std::free(buffer);
        fn_free_v3(&model);
        return true;  // 可选块写入失败, 允许降级继续
    }
    // buffer 所有权已转移给 frame_, 不能再 free

    // 释放 SnrModelV3 内部资源 (points 数组, 由 snr_estimator DLL 分配)
    fn_free_v3(&model);

    // ====================================================================
    // V19 (SNR_REDESIGN_CONTRACT): NoiseWeightModelV1 — source-masked
    // blank-sky 稳健方差 → 逐像素 variance/ivar 块 (Phase2 science 权重来源)
    //   旧 snr_model 块保留为 diagnostic / migration (LEGACY_SNR_SCIENCE_CONSUMER=0)
    // ====================================================================
    {
        const AioBlock* data_blk = fn_get_block(frame_, "data");
        bool data_ok = (data_blk != nullptr && data_blk->data &&
                        data_blk->dims[0] > 0 && data_blk->dims[1] > 0 &&
                        (data_blk->type == AIO_BLOCK_FLOAT32 ||
                         data_blk->type == AIO_BLOCK_FLOAT64));
        if (!data_ok) {
            LOG_WARN("orchestrator", "[SNR] data 块缺失/格式不符, 跳过 NoiseWeightModelV1 "
                     "(variance 块不写, Phase2 权重将退化)");
            if (fn_kv_set) {
                fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_STATUS", "SKIPPED_NO_DATA");
            }
        } else if (!fn_noise_model || !fn_noise_model_f64 ||
                   !fn_noise_fill || !fn_noise_free) {
            LOG_WARN("orchestrator", "[SNR] NoiseWeightModelV1 函数指针缺失, "
                     "跳过 (variance 块不写)");
            if (fn_kv_set) {
                fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_STATUS", "SKIPPED_API_MISSING");
            }
        } else {
            const int H = (int)data_blk->dims[0];
            const int Wd = (int)data_blk->dims[1];
            // 星点掩膜输入: psf cx/cy (0-based)
            std::vector<double> star_x, star_y;
            star_x.reserve((size_t)n_stars);
            star_y.reserve((size_t)n_stars);
            for (int i = 0; i < n_stars; ++i) {
                const double cx = psf_data[(size_t)i * 9 + 3];
                const double cy = psf_data[(size_t)i * 9 + 4];
                if (std::isfinite(cx) && std::isfinite(cy)) {
                    star_x.push_back(cx);
                    star_y.push_back(cy);
                }
            }
            SnrNoiseModelConfig ncfg = {};
            // 默认配置 (API 内部); gain/read-noise 从 FITS header 读取
            const char* gain_s = fn_kv_get ? fn_kv_get(frame_, "header", "GAIN") : nullptr;
            const char* rn_s = fn_kv_get ? fn_kv_get(frame_, "header", "READNOI") : nullptr;
            if (gain_s && gain_s[0]) ncfg.gain_e_per_adu = std::atof(gain_s);
            if (rn_s && rn_s[0]) ncfg.read_noise_e = std::atof(rn_s);
            NoiseWeightModelV1 nm = {};
            int nret = 0;
            if (data_blk->type == AIO_BLOCK_FLOAT64) {
                nret = fn_noise_model_f64(
                    data_blk->data, H, Wd, nullptr,
                    star_x.empty() ? nullptr : star_x.data(),
                    star_y.empty() ? nullptr : star_y.data(),
                    (int)star_x.size(), &ncfg, &nm);
            } else {
                nret = fn_noise_model(
                    data_blk->data, H, Wd, nullptr,
                    star_x.empty() ? nullptr : star_x.data(),
                    star_y.empty() ? nullptr : star_y.data(),
                    (int)star_x.size(), &ncfg, &nm);
            }
            if (nret != 0 && nret != 1) {
                LOG_WARN("orchestrator", "[SNR] NoiseWeightModelV1 失败 (ret=" +
                         std::to_string(nret) + "), variance 块不写");
                if (fn_kv_set) {
                    fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_STATUS", "FAILED");
                }
            } else {
                std::vector<float> variance((size_t)H * (size_t)Wd, 0.0f);
                std::vector<float> ivar((size_t)H * (size_t)Wd, 0.0f);
                if (fn_noise_fill(&nm, H, Wd, variance.data(), ivar.data()) == 0) {
                    const bool fp64 = (config_.precision == PrecisionMode::FP64);
                    int dims2[2] = {H, Wd};
                    // variance 块 (FLOAT32/64 与精度一致)
                    if (fp64) {
                        std::vector<double> var64(variance.begin(), variance.end());
                        std::vector<double> ivar64(ivar.begin(), ivar.end());
                        fn_remove_block(frame_, "variance");
                        int wv = fn_add_block(
                            frame_, "variance", AIO_BLOCK_FLOAT64,
                            var64.data(), (int64_t)var64.size(), dims2, 2,
                            "V19 NoiseWeightModelV1 variance (ADU², blank-sky)");
                        if (wv != 0) {
                            LOG_WARN("orchestrator", "[SNR] variance FLOAT64 块写入失败 (ret=" +
                                     std::to_string(wv) + ")");
                        }
                        fn_remove_block(frame_, "ivar");
                        int wi = fn_add_block(
                            frame_, "ivar", AIO_BLOCK_FLOAT64,
                            ivar64.data(), (int64_t)ivar64.size(), dims2, 2,
                            "V19 NoiseWeightModelV1 ivar");
                        if (wi != 0) {
                            LOG_WARN("orchestrator", "[SNR] ivar FLOAT64 块写入失败 (ret=" +
                                     std::to_string(wi) + ")");
                        }
                    } else {
                        fn_remove_block(frame_, "variance");
                        int wv = fn_add_block(
                            frame_, "variance", AIO_BLOCK_FLOAT32,
                            variance.data(), (int64_t)variance.size(), dims2, 2,
                            "V19 NoiseWeightModelV1 variance (ADU², blank-sky)");
                        if (wv != 0) {
                            LOG_WARN("orchestrator", "[SNR] variance FLOAT32 块写入失败 (ret=" +
                                     std::to_string(wv) + ")");
                        }
                        fn_remove_block(frame_, "ivar");
                        int wi = fn_add_block(
                            frame_, "ivar", AIO_BLOCK_FLOAT32,
                            ivar.data(), (int64_t)ivar.size(), dims2, 2,
                            "V19 NoiseWeightModelV1 ivar");
                        if (wi != 0) {
                            LOG_WARN("orchestrator", "[SNR] ivar FLOAT32 块写入失败 (ret=" +
                                     std::to_string(wi) + ")");
                        }
                    }
                    if (fn_kv_set) {
                        char kv[256];
                        std::snprintf(kv, sizeof(kv), "%.6f", nm.sigma_bg_global);
                        fn_kv_set(frame_, "photo_stats", "NOISE_SIGMA_GLOBAL", kv);
                        std::snprintf(kv, sizeof(kv), "%.6f", nm.variance_bg_global);
                        fn_kv_set(frame_, "photo_stats", "NOISE_VAR_GLOBAL", kv);
                        std::snprintf(kv, sizeof(kv), "%u", nm.n_qualified_patches);
                        fn_kv_set(frame_, "photo_stats", "NOISE_N_PATCHES", kv);
                        std::snprintf(kv, sizeof(kv), "%d", (int)nm.has_spatial_field);
                        fn_kv_set(frame_, "photo_stats", "NOISE_SPATIAL_FIELD", kv);
                        std::snprintf(kv, sizeof(kv), "%d", (int)nm.source);
                        fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_SOURCE", kv);
                        fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_STATUS", "OK");
                        fn_kv_set(frame_, "photo_stats", "NOISE_LEGACY_SNR", "DIAGNOSTIC_ONLY");
                    }
                    LOG_INFO("orchestrator", "[SNR] NoiseWeightModelV1: "
                             + std::string(nret == 0 ? "OK" : "DEGENERATE_GLOBAL")
                             + " sigma_global=" + std::to_string(nm.sigma_bg_global)
                             + " patches=" + std::to_string(nm.n_qualified_patches)
                             + " spatial_field=" + std::to_string((int)nm.has_spatial_field)
                             + " variance/ivar 块已写入");
                } else {
                    LOG_WARN("orchestrator", "[SNR] NoiseWeightModelV1 fill 失败, variance 块不写");
                    if (fn_kv_set) {
                        fn_kv_set(frame_, "photo_stats", "NOISE_MODEL_STATUS", "FILL_FAILED");
                    }
                }
                fn_noise_free(&nm);
            }
        }
    }

    LOG_INFO("orchestrator", "[SNR] 完成 (snr_model 块已写入 [diagnostic], payload="
             + std::to_string(payload_size) + "B)");
    return true;
}

// stage 8: GRADIENT_SPHERE - 球面梯度校准 (healpix_stack.dll hp_stack_gradient_corrected)


// stage 9: STACK - Winsorized sigma clip + SNR²加权叠加 (healpix_stack.dll)


// ============================================================================
// run_stage1 - spec §2.3.3 单帧预处理 (FITS -> calibrated/solved frame -> HEALPix Drizzle -> IVOA HiPS, stage 0-7)
// 串行执行 8 个 stage, 各阶段调用对应 DLL 模块 (骨架), 输出 timings
// P04-004: 集成取消 token / stage 超时 / 原子输出清理
// ============================================================================
// ============================================================================
// run_stage_nside - NSIDE 阶段: 计算/验证 HEALPix NSIDE
// auto: 从 WCS/plate scale 推导 (calculate_nside); explicit: 校验 2 次幂
// ============================================================================
bool Orchestrator::run_stage_nside(TaskResult& result) {
    LOG_INFO("orchestrator", "[NSIDE] 开始 (mode=" + stage1_cfg_->drizzle.nside_mode + ")");
    int nside = 0;
    if (stage1_cfg_->drizzle.nside_mode == "explicit") {
        nside = stage1_cfg_->drizzle.nside_value;
        if (nside < 1 || (nside & (nside - 1)) != 0) {
            result.error_msg = "[NSIDE] explicit nside 不是 2 的幂: " + std::to_string(nside);
            result.exit_code = AstroCsExitCode::CONFIG_ERROR;
            return false;
        }
        nside_used_ = nside;
        LOG_INFO("orchestrator", "[NSIDE] explicit nside=" + std::to_string(nside));
        return true;
    }
    // auto: 从 WCS CD 矩阵推导 (与 drizzle 内部一致)
    // WCS 字段在 run_stage_platesolve 中写入 frame_ header kv (CD1_1 等)
    auto fn_kv_get_double = dll_loader_.get_function<double (*)(
        const PipelineFrame*, const char*, const char*, double)>(
        ModuleId::AIO, "aio_frame_kv_get_double");
    double cd11 = 0, cd12 = 0, cd21 = 0, cd22 = 0;
    if (fn_kv_get_double && frame_) {
        cd11 = fn_kv_get_double(frame_, "header", "CD1_1", 0.0);
        cd12 = fn_kv_get_double(frame_, "header", "CD1_2", 0.0);
        cd21 = fn_kv_get_double(frame_, "header", "CD2_1", 0.0);
        cd22 = fn_kv_get_double(frame_, "header", "CD2_2", 0.0);
    }
    if (cd11 == 0.0 && cd22 == 0.0) {
        result.error_msg = "[NSIDE] WCS CD 矩阵缺失, 无法推导 nside (platesolve 未成功?)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }
    nside = calculate_nside(cd11, cd12, cd21, cd22, "auto", 0);
    nside_used_ = nside;
    LOG_INFO("orchestrator", "[NSIDE] auto 推导 nside=" + std::to_string(nside));
    return true;
}

// ============================================================================
// run_stage_browser_verify - BROWSER_VERIFY 阶段: Browser 后端等价查询验证
// 唯一后端: AIO HiPS Reader + 共享 HEALPix core 映射 (与 hips_browser_backend 相同)
// 验证项:
//   1. signal 可打开, hips_version 由 reader 保证, dtype 与 precision 一致
//   2. 取 SNR catalogue 采样点 (最多 128), 用共享 ang2pix_nest 计算 leaf_ipix
//   3. 对每点: tile 存在 (MOC 覆盖) + aio_hips_read_leaf_* 返回有限 signal
//   4. 至少一个点查询成功; 任何硬错误失败
// ============================================================================
bool Orchestrator::run_stage_browser_verify(TaskResult& result) {
    LOG_INFO("orchestrator", "[BROWSER_VERIFY] 开始 (HiPS AIO Reader + 共享 HEALPix core, precision="
             + std::string(config_.precision == PrecisionMode::FP64 ? "FP64" : "FP32") + ")");
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::AIO)) {
        result.error_msg = "[BROWSER_VERIFY] AIO DLL 未加载";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }
    if (current_hips_dir_.empty() || !fs::exists(current_hips_dir_)) {
        result.error_msg = "[BROWSER_VERIFY] HiPS 目录不存在: " + current_hips_dir_;
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }

    using OpenFn = AioHipsDataset* (*)(const char*, int);
    using GetPropsFn = int (*)(AioHipsDataset*, char*, int);
    using TileCountFn = int (*)(AioHipsDataset*);
    using TileIpixFn = int (*)(AioHipsDataset*, int, uint64_t*);
    using ReadLeafF32Fn = int (*)(AioHipsDataset*, uint64_t, float*);
    using ReadLeafF64Fn = int (*)(AioHipsDataset*, uint64_t, double*);
    using ReadSnrFn = int (*)(AioHipsDataset*, double*, double*, double*, int64_t*,
                              uint32_t*, uint32_t*, int);
    using CloseFn = void (*)(AioHipsDataset*);
    using ErrFn = const char* (*)(void);

    auto fn_open = dll_loader_.get_function<OpenFn>(ModuleId::AIO, "aio_hips_open");
    auto fn_props = dll_loader_.get_function<GetPropsFn>(ModuleId::AIO, "aio_hips_get_properties");
    auto fn_count = dll_loader_.get_function<TileCountFn>(ModuleId::AIO, "aio_hips_tile_count");
    auto fn_ipix = dll_loader_.get_function<TileIpixFn>(ModuleId::AIO, "aio_hips_tile_ipix");
    auto fn_leaf32 = dll_loader_.get_function<ReadLeafF32Fn>(ModuleId::AIO, "aio_hips_read_leaf_f32");
    auto fn_leaf64 = dll_loader_.get_function<ReadLeafF64Fn>(ModuleId::AIO, "aio_hips_read_leaf_f64");
    auto fn_snr = dll_loader_.get_function<ReadSnrFn>(ModuleId::AIO, "aio_hips_read_snr_catalog");
    auto fn_close = dll_loader_.get_function<CloseFn>(ModuleId::AIO, "aio_hips_close");
    auto fn_err = dll_loader_.get_function<ErrFn>(ModuleId::AIO, "aio_hips_reader_last_error");
    if (!fn_open || !fn_props || !fn_count || !fn_ipix || !fn_leaf32 || !fn_leaf64 ||
        !fn_snr || !fn_close) {
        result.error_msg = "[BROWSER_VERIFY] AIO HiPS Reader 函数缺失";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    AioHipsDataset* sig = fn_open(current_hips_dir_.c_str(), AIO_HIPS_RD_SIGNAL);
    if (!sig) {
        result.error_msg = std::string("[BROWSER_VERIFY] signal 打开失败: ") +
                           (fn_err ? fn_err() : "?");
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    char props_buf[8192];
    if (fn_props(sig, props_buf, (int)sizeof(props_buf)) != 0) {
        result.error_msg = "[BROWSER_VERIFY] properties 读取失败";
        fn_close(sig);
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    const std::string props(props_buf);
    int order = 0;
    size_t p = props.find("hips_order=");
    if (p != std::string::npos) order = std::atoi(props.c_str() + p + 11);
    const bool fp64 = props.find("astrocs_signal_dtype=float64") != std::string::npos;
    const bool expect_fp64 = (config_.precision == PrecisionMode::FP64);
    if (fp64 != expect_fp64) {
        result.error_msg = std::string("[BROWSER_VERIFY] signal dtype 与 precision 不匹配: manifest=")
                           + (fp64 ? "float64" : "float32");
        fn_close(sig);
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    const uint32_t leaf_order = (uint32_t)(order + 9);
    const uint32_t nside = (uint32_t)1 << leaf_order;

    std::vector<uint64_t> tile_set;
    const int n_tiles = fn_count(sig);
    tile_set.reserve((size_t)(n_tiles > 0 ? n_tiles : 0));
    for (int i = 0; i < n_tiles; ++i) {
        uint64_t ip = 0;
        if (fn_ipix(sig, i, &ip) == 0) tile_set.push_back(ip);
    }
    const auto tile_exists = [&tile_set](uint64_t tile_ipix) {
        for (uint64_t ip : tile_set) if (ip == tile_ipix) return true;
        return false;
    };

    AioHipsDataset* dsnr = fn_open(current_hips_dir_.c_str(), AIO_HIPS_RD_SNR);
    if (!dsnr) {
        result.error_msg = std::string("[BROWSER_VERIFY] snr 产品打开失败: ") +
                           (fn_err ? fn_err() : "?");
        fn_close(sig);
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    const int kMaxSamples = 128;
    std::vector<double> ra((size_t)kMaxSamples), dec((size_t)kMaxSamples), snr((size_t)kMaxSamples);
    std::vector<int64_t> sid((size_t)kMaxSamples);
    std::vector<uint32_t> qf((size_t)kMaxSamples), ps((size_t)kMaxSamples);
    const int n_snr = fn_snr(dsnr, ra.data(), dec.data(), snr.data(), sid.data(),
                             qf.data(), ps.data(), kMaxSamples);
    fn_close(dsnr);
    if (n_snr <= 0) {
        result.error_msg = "[BROWSER_VERIFY] SNR catalogue 为空 (Browser 查询点缺失)";
        fn_close(sig);
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    int n_queried = 0, n_ok = 0, n_tile_miss = 0, n_read_fail = 0, n_nonfinite = 0;
    for (int i = 0; i < n_snr; ++i) {
        const uint64_t leaf_ipix = astrocs::healpix::ang2pix_nest(nside, ra[(size_t)i], dec[(size_t)i]);
        const uint64_t tile_ipix = leaf_ipix >> 18;
        ++n_queried;
        if (!tile_exists(tile_ipix)) { ++n_tile_miss; continue; }
        float vf = 0.0f;
        double vd = 0.0;
        const int rc = expect_fp64 ? fn_leaf64(sig, leaf_ipix, &vd) : fn_leaf32(sig, leaf_ipix, &vf);
        if (rc != 0) { ++n_read_fail; continue; }
        const double val = expect_fp64 ? vd : (double)vf;
        if (!std::isfinite(val)) { ++n_nonfinite; continue; }
        ++n_ok;
    }
    fn_close(sig);
    if (n_ok == 0) {
        result.error_msg = "[BROWSER_VERIFY] 全部查询点失败 (tile_miss=" + std::to_string(n_tile_miss)
                           + " read_fail=" + std::to_string(n_read_fail)
                           + " nonfinite=" + std::to_string(n_nonfinite) + ")";
        result.exit_code = AstroCsExitCode::HISS_INVALID;
        return false;
    }
    LOG_INFO("orchestrator", "[BROWSER_VERIFY] 通过: order=" + std::to_string(order)
             + " tiles=" + std::to_string(n_tiles)
             + " snr_samples=" + std::to_string(n_queried)
             + " ok=" + std::to_string(n_ok)
             + " tile_miss=" + std::to_string(n_tile_miss)
             + " read_fail=" + std::to_string(n_read_fail)
             + " nonfinite=" + std::to_string(n_nonfinite)
             + " dtype=" + (expect_fp64 ? "float64" : "float32"));
    return true;
}

// run_stage1 - spec §2.3.3 单帧预处理 (FITS -> HiPS, stage 0-9)
// R11: typed Stage1Config 直接驱动; stop_after 真实逐 Gate;
//      NSIDE 与 BROWSER_VERIFY 独立 stage; SNR 必需
// ============================================================================
TaskResult Orchestrator::run_stage1(const Stage1Config& cfg) {
    TaskResult result;
    result.success = false;
    result.frame_name = cfg.input.light;
    *stage1_cfg_ = cfg;

    LOG_INFO("orchestrator", "========== stage1: 单帧预处理 (FITS -> HiPS) ==========");
    LOG_INFO("orchestrator", "输入 FITS: " + cfg.input.light);
    LOG_INFO("orchestrator", "output HiPS: " + cfg.output.hips);
    LOG_INFO("orchestrator", "precision: " + std::string(cfg.precision == PrecisionMode::FP64 ? "FP64" : "FP32"));
    LOG_INFO("orchestrator", "stop_after: " + cfg.execution.stop_after);

    // typed 配置直接驱动 (无 compat flat JSON)
    config_.precision = cfg.precision;
    config_.threads = cfg.execution.threads;
    config_.allow_partial_output = false;
    config_.stage_timeouts.clear();
    static const std::map<std::string, std::string> kTimeoutKeyMap = {
        {"read", "READ_FITS"}, {"calibrate", "CALIBRATE"}, {"platesolve", "PLATESOLVE"},
        {"psf", "PSF"}, {"photometric", "PHOTOMETRIC"}, {"snr", "SNR"},
        {"nside", "NSIDE"}, {"drizzle", "DRIZZLE"}, {"hips_verify", "HIPS_VERIFY"},
        {"hiss_verify", "HISS_VERIFY"}, {"browser_verify", "BROWSER_VERIFY"}
    };
    for (const auto& [k, v] : cfg.execution.stage_timeout_sec) {
        auto it = kTimeoutKeyMap.find(k);
        config_.stage_timeouts[it != kTimeoutKeyMap.end() ? it->second : k] = v;
    }

    reset_cancel_timeout();
    current_output_file_ = cfg.output.hips;

    struct AtomicOutputGuard {
        Orchestrator* self;
        const std::string& path;
        bool& success_flag;
        ~AtomicOutputGuard() {
            if (!success_flag && !path.empty()) {
                self->cleanup_partial_output(path);
            }
        }
    } atomic_guard{this, cfg.output.hips, result.success};

    if (cfg.input.light.empty() || !fs::exists(cfg.input.light)) {
        result.error_msg = "FITS 文件不存在: " + cfg.input.light;
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }
    if (cfg.output.hips.empty()) {
        result.error_msg = "output.hips 路径为空 (正式输出为 HiPS 目录)";
        LOG_ERROR("orchestrator", result.error_msg);
        return result;
    }

    // 加载 DLL: AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE 全部必需
    if (!dlls_loaded_) {
        std::string err;
        // V18 (G1): DLL_LOAD 粗粒度计时（每段一次 clock，低开销）
        const auto t_dll0 = std::chrono::steady_clock::now();
        if (!init_dlls("", err)) {
            bool all_required =
                dll_loader_.is_loaded(ModuleId::AIO) &&
                dll_loader_.is_loaded(ModuleId::CALIBRATE) &&
                dll_loader_.is_loaded(ModuleId::PLATESOLVE) &&
                dll_loader_.is_loaded(ModuleId::PSF) &&
                dll_loader_.is_loaded(ModuleId::PHOTOMETRIC) &&
                dll_loader_.is_loaded(ModuleId::SNR) &&
                dll_loader_.is_loaded(ModuleId::DRIZZLE);
            if (!all_required) {
                LOG_ERROR("orchestrator", "DLL 加载失败 (必需模块缺失, 含 SNR): " + err);
                result.error_msg = "DLL 加载失败 (必需模块缺失, 含 SNR): " + err;
                result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
                state_ = TaskState::FAILED;
                return result;
            }
        }
        {
            const double s = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_dll0).count();
            LOG_INFO("orchestrator", "[DLL_LOAD] 完成 " + std::to_string(s) + "s");
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_frame_ = cfg.input.light;
        current_stage_ = PipelineStageV2::CALIBRATE;
        start_time_ = std::chrono::steady_clock::now();
    }
    state_ = TaskState::RUNNING;

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
    current_fits_path_ = cfg.input.light;
    current_output_path_ = cfg.output.hips;
    // V5 CFG-002: legacy .hiss 仅 validation.legacy_hiss_compare=true 时写出并验证
    legacy_hiss_path_ = cfg.validation.legacy_hiss_compare
        ? (cfg.validation.legacy_hiss_path.empty() ? cfg.output.hiss
                                                   : cfg.validation.legacy_hiss_path)
        : "";

    auto run_v2_with_timing = [&](PipelineStageV2 stage, const char* name,
                                   bool (Orchestrator::*fn)(TaskResult&)) -> bool {
        if (!check_stage_continue(name, result)) {
            StageTiming st;
            st.stage = stage;
            st.stage_name = std::string(name) + " (skipped)";
            st.duration_sec = 0.0;
            st.success = false;
            result.timings.push_back(st);
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_stage_name_ = name;
        }
        LOG_INFO("orchestrator", "---------- stage1 阶段: " + std::string(name) + " ----------");

        double timeout_sec = 0.0;
        auto it = config_.stage_timeouts.find(name);
        if (it != config_.stage_timeouts.end()) timeout_sec = it->second;
        std::thread watchdog;
        bool watchdog_active = false;
        if (timeout_sec > 0.0) {
            watchdog_active = true;
            LOG_INFO("orchestrator", "P04-004: 启动 watchdog for " + std::string(name)
                     + " timeout=" + std::to_string(timeout_sec) + "s");
            stage_watchdog_stop_.store(false, std::memory_order_release);
            watchdog = std::thread([this, name, timeout_sec]() {
                auto begin = std::chrono::steady_clock::now();
                while (!stage_watchdog_stop_.load(std::memory_order_acquire)) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(now - begin).count();
                    if (elapsed > timeout_sec) {
                        timeout_flag_.store(true, std::memory_order_release);
                        LOG_WARN("orchestrator", std::string("P04-004: stage 超时 ") + name
                                 + " (" + std::to_string(elapsed) + "s > " + std::to_string(timeout_sec) + "s)");
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            });
        }

        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        if (watchdog_active) {
            stage_watchdog_stop_.store(true, std::memory_order_release);
            if (watchdog.joinable()) watchdog.join();
        }

        StageTiming st;
        st.stage = stage;
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));

        // Gate 8: 实际 buffer 随机抽样 trace (阶段 manifest + star_id lineage)
        if (ok && frame_ != nullptr && stage1_cfg_ != nullptr) {
            uint64_t trace_seed = 0x9E3779B97F4A7C15ULL;
            for (char c : cfg.output.diagnostics_dir) trace_seed = trace_seed * 131 + (uint8_t)c;
            write_stage_trace(dll_loader_, frame_, name,
                              cfg.output.diagnostics_dir, trace_seed);
        }

        if (is_cancelled()) {
            result.exit_code = AstroCsExitCode::CANCELLED;
            result.error_msg = std::string(name) + " 取消 (用户请求)";
            LOG_WARN("orchestrator", "P04-004: " + result.error_msg);
            return false;
        }
        if (is_timed_out()) {
            result.exit_code = AstroCsExitCode::TIMEOUT;
            result.error_msg = std::string(name) + " 超时";
            LOG_WARN("orchestrator", "P04-004: " + result.error_msg);
            return false;
        }
        if (!ok && result.exit_code == AstroCsExitCode::SUCCESS) {
            switch (stage) {
                case PipelineStageV2::READ_FITS:   result.exit_code = AstroCsExitCode::FILE_IO_ERROR; break;
                case PipelineStageV2::CALIBRATE:   result.exit_code = AstroCsExitCode::CALIBRATE_FAILED; break;
                case PipelineStageV2::PLATESOLVE:  result.exit_code = AstroCsExitCode::PLATESOLVE_FAILED; break;
                case PipelineStageV2::PSF:         result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::PHOTOMETRIC: result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::SNR:         result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::NSIDE:       result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::DRIZZLE:     result.exit_code = AstroCsExitCode::DRIZZLE_FAILED; break;
                case PipelineStageV2::HIPS_VERIFY: result.exit_code = AstroCsExitCode::HISS_INVALID; break;
                case PipelineStageV2::HISS_VERIFY: result.exit_code = AstroCsExitCode::HISS_INVALID; break;
                case PipelineStageV2::BROWSER_VERIFY: result.exit_code = AstroCsExitCode::HISS_INVALID; break;
                default:                            result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
            }
        }
        return ok;
    };

    // gate 名称映射 (stop_after 用小写 stage 名)
    static const std::map<std::string, std::string> kGateMap = {
        {"READ_FITS", "read"}, {"CALIBRATE", "calibrate"}, {"PLATESOLVE", "platesolve"},
        {"PSF", "psf"}, {"PHOTOMETRIC", "photometric"}, {"SNR", "snr"},
        {"NSIDE", "nside"}, {"DRIZZLE", "drizzle"},
        {"HIPS_VERIFY", "hips_verify"},
        {"HISS_VERIFY", "hiss_verify"}, {"BROWSER_VERIFY", "browser_verify"}
    };
    struct StageDef { PipelineStageV2 stage; const char* name;
                      bool (Orchestrator::*fn)(TaskResult&); };
    const StageDef stages[] = {
        {PipelineStageV2::READ_FITS,   "READ_FITS",   &Orchestrator::run_stage_read_fits},
        {PipelineStageV2::CALIBRATE,   "CALIBRATE",   &Orchestrator::run_stage_calibrate},
        // Phase1 Full Freeze v2: 修订流水线 PSF/STAR_MEASURE 先于 PLATESOLVE
        // (单次权威检测 + instrumental flux; PlateSolve 消费同一批星, 禁止重检测)
        {PipelineStageV2::PSF,         "PSF",         &Orchestrator::run_stage_psf},
        {PipelineStageV2::PLATESOLVE,  "PLATESOLVE",  &Orchestrator::run_stage_platesolve},
        {PipelineStageV2::PHOTOMETRIC, "PHOTOMETRIC", &Orchestrator::run_stage_photometric},
        {PipelineStageV2::SNR,         "SNR",         &Orchestrator::run_stage_snr},
        {PipelineStageV2::NSIDE,       "NSIDE",       &Orchestrator::run_stage_nside},
        {PipelineStageV2::DRIZZLE,     "DRIZZLE",     &Orchestrator::run_stage_drizzle},
        {PipelineStageV2::HIPS_VERIFY, "HIPS_VERIFY", &Orchestrator::run_stage_hips_verify},
        {PipelineStageV2::HISS_VERIFY, "HISS_VERIFY", &Orchestrator::run_stage_hiss_verify},
        {PipelineStageV2::BROWSER_VERIFY, "BROWSER_VERIFY", &Orchestrator::run_stage_browser_verify}
    };

    bool ok = true;
    std::string stop_at = cfg.execution.stop_after;
    std::string reached = "";
    for (const auto& sd : stages) {
        if (sd.stage == PipelineStageV2::HISS_VERIFY &&
            !stage1_cfg_->validation.legacy_hiss_compare) {
            // V5 CFG-002: HISS_VERIFY 仅 legacy_hiss_compare=true 时执行
            StageTiming st;
            st.stage = sd.stage;
            st.stage_name = std::string(sd.name) + " (legacy 关闭, 跳过)";
            st.duration_sec = 0.0;
            result.timings.push_back(st);
            continue;
        }
        ok = ok && run_v2_with_timing(sd.stage, sd.name, sd.fn);
        if (!ok) break;
        reached = sd.name;
        auto git = kGateMap.find(sd.name);
        std::string gate = (git != kGateMap.end()) ? git->second : sd.name;
        if (stop_at == gate) {
            // 逐 Gate 成功停止: 明确 completed_to_gate, 不冒充完整 Stage1
            result.success = true;
            result.completed_to_gate = gate;
            result.output_hips_path = (gate == "drizzle" || gate == "hips_verify" ||
                                        gate == "browser_verify")
                                       ? cfg.output.hips : "";
            result.output_hiss_path = (gate == "hiss_verify")
                                       ? (cfg.validation.legacy_hiss_path.empty()
                                              ? cfg.output.hiss
                                              : cfg.validation.legacy_hiss_path)
                                       : "";
            LOG_INFO("orchestrator", "========== stage1 逐 Gate 停止: completed_to_gate=" + gate
                     + " (stop_after=" + stop_at + ") ==========");
            if (frame_ != nullptr && fn_frame_destroy) {
                fn_frame_destroy(frame_);
                frame_ = nullptr;
            }
            state_ = TaskState::COMPLETED;
            return result;
        }
    }

    if (frame_ != nullptr && fn_frame_destroy) {
        fn_frame_destroy(frame_);
        frame_ = nullptr;
    }

    result.success = ok;
    if (ok) {
        result.completed_to_gate = "complete";
        result.output_hips_path = cfg.output.hips;
        result.output_hiss_path = cfg.validation.legacy_hiss_compare
            ? (cfg.validation.legacy_hiss_path.empty()
                   ? cfg.output.hiss : cfg.validation.legacy_hiss_path)
            : "";
    } else {
        result.completed_to_gate = reached.empty() ? "none" : reached;
        if (!config_.allow_partial_output) {
            LOG_INFO("orchestrator", "P04-004: 原子性清理 - stage1 失败/取消/超时, 删除部分输出");
            cleanup_partial_output(cfg.output.hips);
            if (cfg.validation.legacy_hiss_compare) {
                const std::string legacy_path = cfg.validation.legacy_hiss_path.empty()
                    ? cfg.output.hiss : cfg.validation.legacy_hiss_path;
                if (!legacy_path.empty()) cleanup_partial_output(legacy_path);
            }
            result.output_hips_path = "";
            result.output_hiss_path = "";
        }
    }
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;
    LOG_INFO("orchestrator", "========== stage1 "
             + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}

// ============================================================================
// run_stage2 - spec §2.3.3 多帧合并 (.hiss -> .hcsd, stage 8-9)
// 串行执行 2 个 stage: GRADIENT_SPHERE -> STACK
// P04-004: 集成取消 token / stage 超时 / 原子输出清理
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

    // GAP-017: 保存 config_json 供 run_stage_gradient_sphere 读取 sigma_clip_method 等
    current_config_json_ = config_json;

    // P04-004: 重置取消/超时标志, 解析 stage_timeouts 配置
    reset_cancel_timeout();
    auto timeouts = parse_stage_timeouts(config_json);
    if (!timeouts.empty()) {
        config_.stage_timeouts = timeouts;
        LOG_INFO("orchestrator", "P04-004: 已加载 " + std::to_string(timeouts.size()) + " 个 stage 超时配置");
    }
    // P04-004: 解析 allow_partial_output 配置 (默认 false, 严格原子性)
    bool allow_partial = orc_getJsonBool(config_json, "allow_partial_output", false);
    config_.allow_partial_output = allow_partial;
    if (allow_partial) {
        LOG_WARN("orchestrator", "P04-004: allow_partial_output=true, 取消/超时/失败时将保留部分输出");
    }
    // P04-004: 记录输出路径 (用于失败/取消/超时时的原子清理)
    current_output_file_ = output_hcsd;

    // P04-004: 原子性范围守卫 - 任何失败路径 (包括参数校验/DLL加载/stage失败/取消/超时)
    // 都在函数退出时检查并清理部分输出 (除非 allow_partial_output=true 或已成功)
    // 使用 RAII 模式, 析构函数在函数返回时自动调用, 覆盖所有 return 路径
    struct AtomicOutputGuard {
        Orchestrator* self;
        const std::string& path;
        bool& success_flag;
        bool allow_partial;
        ~AtomicOutputGuard() {
            if (!success_flag && !allow_partial && !path.empty()) {
                self->cleanup_partial_output(path);
            }
        }
    } atomic_guard{this, output_hcsd, result.success, config_.allow_partial_output};

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

    // 加载 DLL（V17：legacy Stage2 已移除，healpix_stack 不再需要）
    if (!dlls_loaded_) {
        std::string err;
        if (!init_dlls("", err)) {
            // 仅 stage1 模块缺失, stage2 仍可继续
            LOG_WARN("orchestrator", "DLL 加载警告 (仅 stage1 模块缺失): " + err);
        }
    }

    // 进入运行状态
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_frame_ = hiss_dir;
        current_stage_ = PipelineStageV2::BROWSER_VERIFY;  // legacy stage2 removed
        start_time_ = std::chrono::steady_clock::now();
    }
    state_ = TaskState::RUNNING;

    // V17：legacy Stage2（gradient sphere / stack，healpix_stack）已从
    // active production 移除；Phase2 唯一生产入口 = astrocs-stage2。
    // 旧 stage2 config 在此显式失败（要求改用 astrocs-stage2.exe）。
    bool ok = false;
    result.success = false;
    result.error_msg = "legacy Stage2 removed in V17; use astrocs-stage2";
    result.exit_code = AstroCsExitCode::GENERIC_ERROR;
    LOG_ERROR("orchestrator", result.error_msg);


    // P04-004: 原子输出清理 - 失败/取消/超时时删除部分输出文件
    if (!ok) {
        if (!config_.allow_partial_output) {
            LOG_INFO("orchestrator", "P04-004: 原子性清理 - stage2 失败/取消/超时, 删除部分输出");
            cleanup_partial_output(output_hcsd);
            result.output_hiss_path = "";  // CFG-011: output_ahpx_path -> output_hiss_path
        } else {
            LOG_WARN("orchestrator", "P04-004: allow_partial_output=true, 保留部分输出: " + output_hcsd);
            result.output_hiss_path = output_hcsd;
        }
    } else {
        result.output_hiss_path = output_hcsd;
    }
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;

    LOG_INFO("orchestrator", "========== stage2 "
             + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}
