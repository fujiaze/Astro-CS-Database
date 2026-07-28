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

// snr_estimator C API (用于 run_stage_snr)
#include "snr_estimator.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 简易 JSON 字段提取 (避免引入 nlohmann::json 依赖)
// 与 hp_stack_api.cpp 中的 findKey/extractNum 同风格, 文件作用域私有
// ============================================================================
namespace {

// 查找 "key": 后的值位置, 找不到返回 std::string::npos
size_t orc_findJsonKey(const std::string& s, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    size_t pos = s.find(pat);
    if (pos == std::string::npos) return std::string::npos;
    pos += pat.size();
    // 跳过空白
    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) pos++;
    if (pos >= s.size() || s[pos] != ':') return std::string::npos;
    pos++;
    // 跳过空白
    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) pos++;
    return pos;
}

// 从 pos 提取字符串值 (pos 指向 '"')
std::string orc_extractJsonStr(const std::string& s, size_t pos) {
    std::string r;
    if (pos >= s.size() || s[pos] != '"') return r;
    pos++;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) { r += s[pos+1]; pos += 2; }
        else { r += s[pos]; pos++; }
    }
    return r;
}

// 从 pos 提取数字 (整数或浮点)
double orc_extractJsonNum(const std::string& s, size_t pos) {
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size()) {
        char c = s[pos];
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') pos++;
        else break;
    }
    return std::strtod(s.c_str() + start, nullptr);
}

// ============================================================================
// GAP-016: NSIDE 自适应计算
// 根据原始图像采样率 (CD 矩阵) 和策略计算合适的 HEALPix nside
//   strategy:
//     "1x_to_2x_drizzle" (默认): HEALPix 像素分辨率 = 1-2x 原始像素分辨率
//     "fixed": 使用 nside_override (nside_override > 0 时优先)
//   nside_override > 0: 用户指定优先, 直接返回
// 返回 nside (2 的幂次方, 范围 [64, 131072])
// ============================================================================
int calculate_nside(double cd11, double cd12, double cd21, double cd22,
                    const std::string& strategy, int nside_override) {
    // 1. 用户指定优先
    if (nside_override > 0) {
        // 校验为 2 的幂次方且在范围内
        int n = nside_override;
        if (n < 64) n = 64;
        if (n > 131072) n = 131072;
        // 若不是 2 的幂次方, 向下取到最近的 2 的幂次方
        if ((n & (n - 1)) != 0) {
            int p = 1;
            while (p * 2 <= n) p *= 2;
            n = p;
        }
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
    // HEALPix 像素分辨率 (arcsec) ≈ 3600*60*sqrt(3) / (3*nside) = 1186.18 / nside
    // 反推: nside ≈ 1186.18 / target_resolution_arcsec
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
    // nside = 1186.18 / target_resolution_arcsec
    // 推导: HEALPix 像素面积 = 4π/(12*nside²) sr = π/(3*nside²) sr
    //       像素边长 ≈ sqrt(π/(3*nside²)) rad = sqrt(π/3)/nside rad
    //                ≈ 1.0233/nside rad ≈ 58.6/nside deg ≈ 3517.6/nside * sqrt(3)/3 arcsec
    // 简化用: resolution_arcsec ≈ 3517.6 / nside * sqrt(3)/3 ≈ 1186.18 / nside (近似)
    // 反推 nside ≈ 1186.18 / target_resolution_arcsec
    double nside_target = 1186.18 / target_resolution_arcsec;

    // 4. 找到不小于 nside_target 的最小 2 的幂次方
    int nside = 64;
    while (nside < nside_target && nside < 131072) {
        nside *= 2;
    }

    // 5. 限制在 [64, 131072]
    if (nside < 64) nside = 64;
    if (nside > 131072) nside = 131072;

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

// 从 JSON 文本提取字符串字段 (复用 orc_findJsonKey/orc_extractJsonStr)
std::string orc_getJsonString(const std::string& s, const std::string& key) {
    size_t pos = orc_findJsonKey(s, key);
    if (pos == std::string::npos) return "";
    return orc_extractJsonStr(s, pos);
}

// 从 JSON 文本提取数字字段 (复用 orc_findJsonKey/orc_extractJsonNum)
double orc_getJsonNum(const std::string& s, const std::string& key, double def) {
    size_t pos = orc_findJsonKey(s, key);
    if (pos == std::string::npos) return def;
    return orc_extractJsonNum(s, pos);
}

// 从 JSON 文本提取布尔字段
bool orc_getJsonBool(const std::string& s, const std::string& key, bool def) {
    size_t pos = orc_findJsonKey(s, key);
    if (pos == std::string::npos) return def;
    while (pos < s.size() && (s[pos]==' '||s[pos]=='\t'||s[pos]=='\n'||s[pos]=='\r')) pos++;
    if (pos + 4 <= s.size() && s[pos]=='t' && s[pos+1]=='r' && s[pos+2]=='u' && s[pos+3]=='e') return true;
    if (pos + 5 <= s.size() && s[pos]=='f' && s[pos+1]=='a' && s[pos+2]=='l' && s[pos+3]=='s' && s[pos+4]=='e') return false;
    return def;
}

// 格式化曝光时间字符串 (2 位小数, 如 "180.00s")
std::string format_exposure_2f(double exptime) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", exptime);
    return std::string(buf) + "s";
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

// 从 calibration_dir + 帧尺寸自动推导 Master Bias 路径
// 文件名格式: masterBias_BIN-1_<W>x<H>.xisf
std::string derive_master_bias_path(const std::string& dir, int w, int h) {
    if (dir.empty()) return "";
    std::string filename = "masterBias_BIN-1_" + std::to_string(w) + "x" + std::to_string(h) + ".xisf";
    return (fs::path(dir) / filename).string();
}

// 从 calibration_dir + 帧尺寸 + 曝光时间自动推导 Master Dark 路径
// 文件名格式: masterDark_BIN-1_<W>x<H>_EXPOSURE-<exptime.2f>s.xisf
std::string derive_master_dark_path(const std::string& dir, int w, int h, double exptime) {
    if (dir.empty() || exptime <= 0) return "";
    std::string filename = "masterDark_BIN-1_" + std::to_string(w) + "x" + std::to_string(h) +
                           "_EXPOSURE-" + format_exposure_2f(exptime) + ".xisf";
    return (fs::path(dir) / filename).string();
}

// 从 calibration_dir + 帧尺寸 + 滤光片自动推导 Master Flat 路径
// 文件名格式: masterFlat_BIN-1_<W>x<H>_FILTER-<Filter>_mono.xisf
std::string derive_master_flat_path(const std::string& dir, int w, int h, const std::string& filter_name) {
    if (dir.empty() || filter_name.empty()) return "";
    std::string filename = "masterFlat_BIN-1_" + std::to_string(w) + "x" + std::to_string(h) +
                           "_FILTER-" + filter_name + "_mono.xisf";
    return (fs::path(dir) / filename).string();
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
        case PipelineStageV2::PLATESOLVE:      return "PLATESOLVE";
        case PipelineStageV2::PSF:             return "PSF";
        case PipelineStageV2::PHOTOMETRIC:     return "PHOTOMETRIC";
        case PipelineStageV2::SNR:             return "SNR";
        case PipelineStageV2::DRIZZLE:         return "DRIZZLE";
        case PipelineStageV2::GRADIENT_SPHERE: return "GRADIENT_SPHERE";
        case PipelineStageV2::STACK:           return "STACK";
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

    // 查找 "stage_timeouts" 字段
    size_t pos = orc_findJsonKey(config_json, "stage_timeouts");
    if (pos == std::string::npos) return result;

    // 跳过空白, 期望 '{'
    while (pos < config_json.size() && (config_json[pos]==' '||config_json[pos]=='\t'||config_json[pos]=='\n'||config_json[pos]=='\r')) pos++;
    if (pos >= config_json.size() || config_json[pos] != '{') return result;
    pos++; // 跳过 '{'

    // 解析 "stage_name": <number> 对
    while (pos < config_json.size()) {
        // 跳过空白与逗号
        while (pos < config_json.size() && (config_json[pos]==' '||config_json[pos]=='\t'||config_json[pos]=='\n'||config_json[pos]=='\r'||config_json[pos]==',')) pos++;
        if (pos >= config_json.size() || config_json[pos] == '}') break;

        // 解析 key (字符串)
        if (config_json[pos] != '"') { pos++; continue; }
        std::string key = orc_extractJsonStr(config_json, pos);
        // 跳过 key 字符串
        pos = config_json.find('"', pos + 1) + 1;
        // 跳过空白与冒号
        while (pos < config_json.size() && (config_json[pos]==' '||config_json[pos]=='\t'||config_json[pos]=='\n'||config_json[pos]=='\r')) pos++;
        if (pos < config_json.size() && config_json[pos] == ':') pos++;
        while (pos < config_json.size() && (config_json[pos]==' '||config_json[pos]=='\t'||config_json[pos]=='\n'||config_json[pos]=='\r')) pos++;

        // 解析 number
        std::string num_str;
        while (pos < config_json.size() && (std::isdigit((unsigned char)config_json[pos]) || config_json[pos]=='.' || config_json[pos]=='-' || config_json[pos]=='+' || config_json[pos]=='e' || config_json[pos]=='E')) {
            num_str += config_json[pos];
            pos++;
        }
        if (!num_str.empty()) {
            try {
                double seconds = std::stod(num_str);
                result[key] = seconds;
                LOG_INFO("orchestrator", "P04-004: stage_timeouts[" + key + "] = " + std::to_string(seconds) + "s");
            } catch (...) {
                LOG_WARN("orchestrator", "P04-004: stage_timeouts[" + key + "] 解析失败: " + num_str);
            }
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

    // P03-002: 把整个 JSON 文本存入 calib_params_json (供各 stage handler 按需解析)
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
    // 空时在 init_platesolve_env 中默认为 project_root_dir_/GaiaDR3SP
    std::string cfg_gaia_dir = orc_getJsonString(content, "gaia_data_dir");
    if (!cfg_gaia_dir.empty()) {
        config_gaia_data_dir_ = cfg_gaia_dir;
        LOG_INFO("orchestrator", "配置 Gaia 数据目录: " + cfg_gaia_dir);
    }

    LOG_INFO("orchestrator", "配置加载完成 (P03-002: log_level/threads/gaia_data_dir 已解析)");
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
        // 收集所有失败模块的错误信息 (spec §2.3.2 9 节点, 2026-07-18 归档 GRADIENT_2D)
        std::stringstream ss;
        ss << "部分模块加载失败: ";
        bool first = true;
        std::vector<ModuleId> ids = {
            ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
            ModuleId::PSF, ModuleId::PHOTOMETRIC,
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

    // 加载成功, 输出各模块版本信息 (spec §2.3.2 9 节点, 2026-07-18 归档 GRADIENT_2D)
    LOG_INFO("orchestrator", "全部 9 个模块加载成功");
    for (auto id : {ModuleId::AIO, ModuleId::CALIBRATE, ModuleId::PLATESOLVE,
                    ModuleId::PSF, ModuleId::PHOTOMETRIC,
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
    // AIO 读取 .xisf Master 文件函数
    auto fn_read_xisf = dll_loader_.get_function<AIOImageData* (*)(const char*)>(
        ModuleId::AIO, "aio_read_xisf");
    auto fn_get_pixels = dll_loader_.get_function<float* (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_pixel_data");
    auto fn_get_width = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_width");
    auto fn_get_height = dll_loader_.get_function<int (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_height");
    auto fn_get_metadata = dll_loader_.get_function<AIOImageMetadata (*)(const AIOImageData*)>(
        ModuleId::AIO, "aio_get_metadata");
    auto fn_free_image = dll_loader_.get_function<void (*)(AIOImageData*)>(
        ModuleId::AIO, "aio_free_image_data");

    if (!fn_calibrate || !fn_get_block_data || !fn_get_block || !fn_remove_block ||
        !fn_add_block_move || !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double ||
        !fn_read_xisf || !fn_get_pixels || !fn_get_width || !fn_get_height ||
        !fn_get_metadata || !fn_free_image) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 函数指针获取失败");
        result.error_msg = "[CALIBRATE] 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 1. 从 frame_ 读取 data 块 (FLOAT32 [H,W])
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
    float* light = static_cast<float*>(data_block->data);
    int64_t n_pix = static_cast<int64_t>(width) * height;

    LOG_INFO("orchestrator", "[CALIBRATE] 图像: " + std::to_string(width) + "x" + std::to_string(height)
             + " (" + std::to_string(n_pix) + " 像素)");

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

    // 3. 从 current_config_json_ 解析 calibration 段字段
    std::string bias_path, dark_path, flat_path;
    bool require_size = true;
    bool require_exposure = true;
    double exposure_tol = 0.5;
    bool require_temp = false;
    double temp_tol = 1.0;
    bool allow_no_calib = false;
    int dark_opt = 0;
    double dark_k = 1.0;

    if (!current_config_json_.empty()) {
        bias_path = orc_getJsonString(current_config_json_, "master_bias_path");
        dark_path = orc_getJsonString(current_config_json_, "master_dark_path");
        flat_path = orc_getJsonString(current_config_json_, "master_flat_path");
        require_size = orc_getJsonBool(current_config_json_, "require_size_match", true);
        require_exposure = orc_getJsonBool(current_config_json_, "require_exposure_match", true);
        exposure_tol = orc_getJsonNum(current_config_json_, "exposure_tolerance_s", 0.5);
        require_temp = orc_getJsonBool(current_config_json_, "require_temperature_match", false);
        temp_tol = orc_getJsonNum(current_config_json_, "temperature_tolerance_c", 1.0);
        allow_no_calib = orc_getJsonBool(current_config_json_, "allow_no_calibration", false);
        dark_opt = static_cast<int>(orc_getJsonNum(current_config_json_, "dark_optimization", 0.0));
        dark_k = orc_getJsonNum(current_config_json_, "dark_scale_factor", 1.0);
    }

    // 4. 从 calibration_dir (顶层字段) 自动推导 Master 路径 (若 master_*_path 为空)
    std::string calib_dir;
    if (!current_config_json_.empty()) {
        calib_dir = orc_getJsonString(current_config_json_, "calibration_dir");
    }
    if (calib_dir.empty()) {
        calib_dir = "testdata/T4 calibration files";
    }
    // 相对路径基于 project_root_dir_ 解析
    if (!calib_dir.empty() && !fs::path(calib_dir).is_absolute()) {
        calib_dir = (fs::path(project_root_dir_) / calib_dir).string();
    }
    LOG_INFO("orchestrator", "[CALIBRATE] calibration_dir: " + calib_dir);

    if (bias_path.empty()) {
        bias_path = derive_master_bias_path(calib_dir, width, height);
        LOG_INFO("orchestrator", "[CALIBRATE] 自动推导 master_bias_path: " + bias_path);
    }
    if (dark_path.empty()) {
        dark_path = derive_master_dark_path(calib_dir, width, height, frame_exptime);
        LOG_INFO("orchestrator", "[CALIBRATE] 自动推导 master_dark_path: " + dark_path);
    }
    if (flat_path.empty()) {
        flat_path = derive_master_flat_path(calib_dir, width, height, frame_filter);
        LOG_INFO("orchestrator", "[CALIBRATE] 自动推导 master_flat_path: " + flat_path);
    }

    // 5. 加载 Master 文件 (aio_read_xisf)
    AIOImageData* bias_img = nullptr;
    AIOImageData* dark_img = nullptr;
    AIOImageData* flat_img = nullptr;
    float* master_bias = nullptr;
    float* master_dark = nullptr;
    float* master_flat = nullptr;
    int bias_w = 0, bias_h = 0;
    int dark_w = 0, dark_h = 0;
    int flat_w = 0, flat_h = 0;
    double dark_exptime = -1.0;
    double dark_temp = 0.0;
    bool dark_has_temp = false;

    auto load_master = [&](const std::string& path, AIOImageData*& img,
                           float*& pix, int& w, int& h) -> bool {
        if (path.empty()) return false;
        if (!fs::exists(path)) {
            LOG_WARN("orchestrator", "[CALIBRATE] Master 文件不存在: " + path);
            return false;
        }
        img = fn_read_xisf(path.c_str());
        if (img == nullptr) {
            LOG_ERROR("orchestrator", "[CALIBRATE] 读取 Master 文件失败: " + path);
            return false;
        }
        w = fn_get_width(img);
        h = fn_get_height(img);
        pix = fn_get_pixels(img);
        LOG_INFO("orchestrator", "[CALIBRATE] 加载 Master: " + path
                 + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");
        return true;
    };

    bool has_bias = load_master(bias_path, bias_img, master_bias, bias_w, bias_h);
    bool has_dark = load_master(dark_path, dark_img, master_dark, dark_w, dark_h);
    bool has_flat = load_master(flat_path, flat_img, master_flat, flat_w, flat_h);

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
    float* out = static_cast<float*>(std::malloc(n_pix * sizeof(float)));
    if (out == nullptr) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 分配输出缓冲失败");
        result.error_msg = "[CALIBRATE] 分配输出缓冲失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }

    // 9. 调用 ac_calibrate_frame (实际应用 Bias/Dark/Flat)
    float actual_k = 0.0f;
    LOG_INFO("orchestrator", "[CALIBRATE] 调用 ac_calibrate_frame: dark_opt="
             + std::to_string(dark_opt) + ", k_init=" + std::to_string(dark_k)
             + ", has_bias=" + (has_bias ? "true" : "false")
             + ", has_dark=" + (has_dark ? "true" : "false")
             + ", has_flat=" + (has_flat ? "true" : "false"));
    int ret = fn_calibrate(light, width, height,
                           master_dark, master_flat, master_bias,
                           out, dark_opt, static_cast<float>(dark_k), &actual_k);

    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] ac_calibrate_frame 失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] ac_calibrate_frame 失败: ret=" + std::to_string(ret);
        result.exit_code = AstroCsExitCode::CALIBRATE_FAILED;
        std::free(out);
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }

    // 10. 计算校准前/后图像统计 + Master 均值
    double light_mean = compute_array_mean(light, n_pix);
    double light_median = compute_array_median(light, n_pix);
    double light_std = compute_array_std(light, n_pix, light_mean);
    double out_mean = compute_array_mean(out, n_pix);
    double out_median = compute_array_median(out, n_pix);
    double out_std = compute_array_std(out, n_pix, out_mean);
    double bias_mean = has_bias ? compute_array_mean(master_bias, n_pix) : 0.0;
    double dark_mean = has_dark ? compute_array_mean(master_dark, n_pix) : 0.0;
    double flat_mean = has_flat ? compute_array_mean(master_flat, n_pix) : 0.0;

    LOG_INFO("orchestrator", "[CALIBRATE] 校准统计: light_mean=" + std::to_string(light_mean)
             + " -> out_mean=" + std::to_string(out_mean)
             + ", bias_mean=" + std::to_string(bias_mean)
             + ", dark_mean=" + std::to_string(dark_mean)
             + ", flat_mean=" + std::to_string(flat_mean)
             + ", actual_k=" + std::to_string(actual_k));

    // 11. 替换 data 块 (转移所有权)
    //     P03-003: data 块写回失败必须返回非零 (退出码 3, 必需块缺失)
    fn_remove_block(frame_, "data");
    int dims[2] = {height, width};
    ret = fn_add_block_move(frame_, "data", AIO_BLOCK_FLOAT32,
                            out, n_pix, dims, 2, "校准后 Light 像素 (Bias/Dark/Flat 已应用)");
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[CALIBRATE] 写回 data 块失败: ret=" + std::to_string(ret));
        result.error_msg = "[CALIBRATE] 写回 data 块失败";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        std::free(out);
        if (bias_img) fn_free_image(bias_img);
        if (dark_img) fn_free_image(dark_img);
        if (flat_img) fn_free_image(flat_img);
        return false;
    }
    // out 所有权已转移给 frame_

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
        GetProcAddress(gaia_h, "gaia_client_create_ex"));
    if (fn_gaia_create_ex == nullptr) {
        error_msg = "gaia_client_create_ex 函数未找到";
        LOG_ERROR("orchestrator", "[PLATESOLVE] " + error_msg);
        cleanup_platesolve_env();
        return false;
    }
    // P03-002: 优先使用 config 中配置的 gaia_data_dir
    std::string gaia_data_dir;
    if (!config_gaia_data_dir_.empty()) {
        gaia_data_dir = config_gaia_data_dir_;
        // 相对路径基于 project_root_dir_ 解析
        if (!fs::path(gaia_data_dir).is_absolute()) {
            gaia_data_dir = (fs::path(project_root_dir_) / gaia_data_dir).string();
        }
    } else {
        gaia_data_dir = project_root_dir_ + "/GaiaDR3SP";
    }
    LOG_INFO("orchestrator", "[PLATESOLVE] Gaia 数据目录: " + gaia_data_dir
             + (config_gaia_data_dir_.empty() ? " (默认)" : " (来自 config)"));
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
    // P03-002: maxStars 从 config 读取 (默认 2000)
    int cfg_max_stars = 2000;
    if (!current_config_json_.empty()) {
        // 优先 platesolve.max_stars, 其次 psf.max_stars
        cfg_max_stars = static_cast<int>(orc_getJsonNum(current_config_json_, "max_stars", 2000.0));
        if (cfg_max_stars <= 0) cfg_max_stars = 2000;
    }
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
// P09-002 INTERNAL_DETECTION_SHARED_EXPORT: callback 导出 sdet_detect_ex 检测结果
// (历史命名: P02-003 路径 B; 正式命名见 capabilities["internal_detection_shared_export"])
// ============================================================================
// INTERNAL_DETECTION_SHARED_EXPORT callback 上下文 (POD-like, 供 C callback 通过 user_data 访问)
// star_det v1 格式: FLOAT64 [N,6] (x_px, y_px, flux, mag, saturated, has_saturated)
namespace {
struct PathBCallbackCtx {
    std::vector<double> detections_buf;  // 复制的检测结果 [N*6]
    int n_detected = 0;
    bool copied = false;
};

// INTERNAL_DETECTION_SHARED_EXPORT callback 函数 (签名匹配 IpvDetectionCallback)
// 在 sdet_detect_ex 之后、选星之前同步调用; 返回后源指针失效, 必须在此复制
void path_b_detection_callback(const double* detections, int n_detections, void* user_data) {
    PathBCallbackCtx* ctx = static_cast<PathBCallbackCtx*>(user_data);
    if (ctx == nullptr) return;
    if (detections == nullptr || n_detections <= 0) {
        ctx->n_detected = 0;
        ctx->copied = false;
        return;
    }
    // 立即复制检测结果到本地缓冲区 (callback 返回后源指针失效)
    ctx->detections_buf.assign(
        detections, detections + static_cast<size_t>(n_detections) * 6);
    ctx->n_detected = n_detections;
    ctx->copied = true;
}
}  // namespace

// ============================================================================
// run_stage_platesolve - PLATESOLVE 阶段实现 (ipv_solver.dll 内存接口)
// 流程 (P09-002 INTERNAL_DETECTION_SHARED_EXPORT, 历史 P02-003 路径 B: callback 导出):
//   1. 读取 PipelineFrame "data" 块 (FLOAT32 [H,W])
//   2. 读取 "header" KV 块的 OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ
//   3. 调用 ipv_solve_from_memory_with_callback 求解 WCS+SIP
//      (内部 sdet_detect_ex 仅调用 1 次, callback 同步导出检测结果)
//   4. 将 WCS+SIP 结果写回 "header" KV 块
//   5. 用 callback 导出的检测结果写入 "star_det" 块 (FLOAT32 [N,4])
//      (避免原路径在 PLATESOLVE 内显式第二次调用 sdet_detect_ex)
//   6. (可选) 写入 "gaia_cat" 块 (FLOAT64 [N,3]: ra,dec,mag)
// 约束: 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据
// ============================================================================
bool Orchestrator::run_stage_platesolve(TaskResult& result) {
    LOG_INFO("orchestrator", "[PLATESOLVE] 开始");

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
    auto fn_add_block = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, AioBlockType,
        const void*, int64_t, const int*, int, const char*)>(
        ModuleId::AIO, "aio_frame_add_block");
    auto fn_remove_block = dll_loader_.get_function<int (*)(PipelineFrame*, const char*)>(
        ModuleId::AIO, "aio_frame_remove_block");

    if (!fn_get_block || !fn_kv_get || !fn_kv_get_double || !fn_kv_set || !fn_kv_set_double) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] AIO 函数指针获取失败");
        result.error_msg = "[PLATESOLVE] AIO 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 1. 读取 data 块 (FLOAT32 [H,W])
    //    P03-003: data 是必需块 (CALIBRATE 产出), 缺失必须失败 (退出码 3)
    const AioBlock* data_block = fn_get_block(frame_, "data");
    if (data_block == nullptr) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] data 块不存在 (必需块)");
        result.error_msg = "[PLATESOLVE] data 块不存在 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }
    int height = data_block->dims[0];
    int width = data_block->dims[1];
    const float* pixels = static_cast<const float*>(data_block->data);
    LOG_INFO("orchestrator", "[PLATESOLVE] 图像: " + std::to_string(width) + "x" + std::to_string(height));

    // 2. 从 header KV 读取初始指向 (OBJCTRA/OBJCTDEC/FOCALLEN/XPIXSZ)
    //    约束: 必须使用 OBJCTRA/OBJCTDEC 作为初始指向, 无论是否已有 WCS 数据
    //    P03-002: 支持从 config 覆盖 (initial_ra/initial_dec/focal_length/pixel_size)
    const char* objctra = fn_kv_get(frame_, "header", "OBJCTRA");
    const char* objctdec = fn_kv_get(frame_, "header", "OBJCTDEC");
    double focal_length = fn_kv_get_double(frame_, "header", "FOCALLEN", 0.0);
    double pixel_size = fn_kv_get_double(frame_, "header", "XPIXSZ", 0.0);

    // P03-002: config 覆盖 (空/0 时使用 FITS header 值)
    std::string cfg_initial_ra = orc_getJsonString(current_config_json_, "initial_ra");
    std::string cfg_initial_dec = orc_getJsonString(current_config_json_, "initial_dec");
    double cfg_focal = orc_getJsonNum(current_config_json_, "focal_length", 0.0);
    double cfg_pixel = orc_getJsonNum(current_config_json_, "pixel_size", 0.0);
    if (!cfg_initial_ra.empty()) objctra = cfg_initial_ra.c_str();
    if (!cfg_initial_dec.empty()) objctdec = cfg_initial_dec.c_str();
    if (cfg_focal > 0.0) focal_length = cfg_focal;
    if (cfg_pixel > 0.0) pixel_size = cfg_pixel;

    double ra0 = parse_ra_hms(objctra);
    double dec0 = parse_dec_dms(objctdec);

    LOG_INFO("orchestrator", "[PLATESOLVE] 初始指向: OBJCTRA='" + std::string(objctra ? objctra : "")
             + "' -> ra0=" + std::to_string(ra0) + "deg, OBJCTDEC='" + std::string(objctdec ? objctdec : "")
             + "' -> dec0=" + std::to_string(dec0) + "deg");
    LOG_INFO("orchestrator", "[PLATESOLVE] 焦距=" + std::to_string(focal_length)
             + "mm, 像素尺寸=" + std::to_string(pixel_size) + "um"
             + (!cfg_initial_ra.empty() || cfg_focal > 0.0 ? " (部分来自 config)" : ""));

    // 3. 调用 ipv_solve_from_memory_with_callback (P09-002 INTERNAL_DETECTION_SHARED_EXPORT)
    //    与 ipv_solve_from_memory 算法等价, 区别: callback 同步导出 sdet_detect_ex 结果
    //    callback 为 NULL 时行为与 ipv_solve_from_memory 完全一致
    auto fn_ipv_solve_cb = dll_loader_.get_function<int (*)(
        void*, const float*, int, int, double, double, double, double,
        const IpvParams*, IpvDetectionCallback, void*, IpvWcsResult*)>(
        ModuleId::PLATESOLVE, "ipv_solve_from_memory_with_callback");
    auto fn_get_default_params = dll_loader_.get_function<void (*)(IpvParams*)>(
        ModuleId::PLATESOLVE, "ipv_get_default_params");

    if (!fn_ipv_solve_cb || !fn_get_default_params) {
        LOG_ERROR("orchestrator", "[PLATESOLVE] ipv 函数指针获取失败 (ipv_solve_from_memory_with_callback)");
        result.error_msg = "[PLATESOLVE] ipv 函数指针获取失败 (INTERNAL_DETECTION_SHARED_EXPORT)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
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

    // P09-002 INTERNAL_DETECTION_SHARED_EXPORT: callback 上下文, 用于接收 sdet_detect_ex 检测结果
    PathBCallbackCtx cb_ctx;

    LOG_INFO("orchestrator", "[PLATESOLVE] 调用 ipv_solve_from_memory_with_callback (INTERNAL_DETECTION_SHARED_EXPORT callback 导出) ...");
    int ret = fn_ipv_solve_cb(ipv_solver_handle_,
                              pixels, width, height,
                              ra0, dec0, focal_length, pixel_size,
                              &params, path_b_detection_callback, &cb_ctx, &wcs_result);

    LOG_INFO("orchestrator", "[PLATESOLVE] callback 导出: n_detected=" + std::to_string(cb_ctx.n_detected)
             + ", copied=" + (cb_ctx.copied ? "true" : "false")
             + ", wcs_result.n_detected=" + std::to_string(wcs_result.n_detected));

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

    // 6. 写入 star_det 块 (FLOAT32 [N,4]: x, y, flux, mag)
    //    P09-002 INTERNAL_DETECTION_SHARED_EXPORT (历史 P02-003 路径 B):
    //       使用 callback 导出的检测结果 (避免原路径第二次 sdet_detect_ex)
    //    原路径: ipv_solve_from_memory 内部 sdet_detect_ex (1次) + 此处显式 sdet_detect_ex (2次)
    //    INTERNAL_DETECTION_SHARED_EXPORT: ipv_solve_from_memory_with_callback 内部 sdet_detect_ex (1次) + callback 导出
    //    sdet_detect_ex 调用次数: 2 -> 1, 减少重复计算和内存占用
    //    P03-003: star_det 是必需块 (PSF 依赖), 写入失败必须返回非零退出码
    if (fn_add_block && fn_remove_block && cb_ctx.copied && cb_ctx.n_detected > 0
        && !cb_ctx.detections_buf.empty()) {
        int64_t n_stars = cb_ctx.n_detected;
        float* star_det = static_cast<float*>(std::malloc(n_stars * 4 * sizeof(float)));
        if (star_det == nullptr) {
            LOG_ERROR("orchestrator", "[PLATESOLVE] 分配 star_det 缓冲失败 (必需块)");
            result.error_msg = "[PLATESOLVE] 分配 star_det 缓冲失败";
            result.exit_code = AstroCsExitCode::GENERIC_ERROR;
            return false;
        }
        for (int64_t i = 0; i < n_stars; ++i) {
            // star_det v1: [x_px, y_px, flux, mag, saturated, has_saturated] (FLOAT64)
            // star_det 块: [x, y, flux, mag] (FLOAT32, PSF 期望格式)
            star_det[i * 4 + 0] = static_cast<float>(cb_ctx.detections_buf[i * 6 + 0]);  // x
            star_det[i * 4 + 1] = static_cast<float>(cb_ctx.detections_buf[i * 6 + 1]);  // y
            star_det[i * 4 + 2] = static_cast<float>(cb_ctx.detections_buf[i * 6 + 2]);  // flux
            star_det[i * 4 + 3] = static_cast<float>(cb_ctx.detections_buf[i * 6 + 3]);  // mag
        }
        int dims[2] = {static_cast<int>(n_stars), 4};
        fn_remove_block(frame_, "star_det");
        int r = fn_add_block(frame_, "star_det", AIO_BLOCK_FLOAT32,
                             star_det, n_stars * 4, dims, 2,
                             "星点检测结果 (INTERNAL_DETECTION_SHARED_EXPORT callback 导出): x,y,flux,mag");
        if (r == 0) {
            LOG_INFO("orchestrator", "[PLATESOLVE] star_det 块已写入 (INTERNAL_DETECTION_SHARED_EXPORT): "
                     + std::to_string(n_stars) + " 颗星");
        } else {
            LOG_ERROR("orchestrator", "[PLATESOLVE] star_det 块写入失败 (必需块): add_block ret="
                     + std::to_string(r));
            result.error_msg = "[PLATESOLVE] star_det 块写入失败 (必需块)";
            result.exit_code = AstroCsExitCode::BLOCK_MISSING;
            std::free(star_det);
            return false;
        }
        std::free(star_det);
    } else {
        // P03-003: callback 未导出检测结果, star_det 必需块缺失, 必须失败
        LOG_ERROR("orchestrator", "[PLATESOLVE] callback 未导出检测结果 (n_detected="
                 + std::to_string(cb_ctx.n_detected) + ", copied="
                 + (cb_ctx.copied ? "true" : "false")
                 + "), star_det 必需块未写入");
        result.error_msg = "[PLATESOLVE] callback 未导出检测结果, star_det 必需块未写入";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        return false;
    }

    // P02-006: 删除无消费者 gaia_cat 二次查询
    // 原代码在 PLATESOLVE 求解完成后, 调用 gaia_client_cone_search_for_solver 写 "gaia_cat" 块,
    // 但下游 stage (PSF / PHOTOMETRIC / SNR / DRIZZLE) 均未读取 "gaia_cat" 块, 属于无消费者死代码。
    // 区分两条 Gaia 查询语义 (docs/05 §9):
    //   - Astrometry query: PLATESOLVE 内部由 ipv_solve_from_memory_with_callback 完成
    //     (匹配星等几何 + WCS 求解), 结果以 star_det / WCS 形式落盘, 不需要 gaia_cat。
    //   - Photometry query: PHOTOMETRIC 阶段独立调用 pc_calibrate_simple_with_gaia,
    //     内部按需 cone search + DR3SP 光谱 + 滤光片 + QE 积分, 不依赖 gaia_cat 块。
    // 删除此处二次查询可节省一次 cone search 调用 (GaiaClient 进程内缓存仍按 TTL 命中)。

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
    //    P03-003: star_det 是必需块 (PLATESOLVE INTERNAL_DETECTION_SHARED_EXPORT 产出), 缺失必须失败 (退出码 3)
    const AioBlock* star_det_block = fn_get_block(frame_, "star_det");
    if (star_det_block == nullptr || star_det_block->dims[0] <= 0) {
        LOG_ERROR("orchestrator", "[PSF] star_det 块不存在或为空 (必需块, PLATESOLVE 应产出)");
        result.error_msg = "[PSF] star_det 块不存在或为空 (必需块)";
        result.exit_code = AstroCsExitCode::BLOCK_MISSING;
        std::free(pixels_u16);
        return false;
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

    // P03-002: 从 current_config_json_ 解析 psf 参数 (默认值与原硬编码一致)
    int fit_radius = 8;
    int max_iter = 100;
    double tolerance = 1e-6;
    if (!current_config_json_.empty()) {
        fit_radius = static_cast<int>(orc_getJsonNum(current_config_json_, "fit_radius", 8.0));
        max_iter = static_cast<int>(orc_getJsonNum(current_config_json_, "max_iter", 100.0));
        tolerance = orc_getJsonNum(current_config_json_, "tolerance", 1e-6);
        if (fit_radius < 0) fit_radius = 0;  // 0 = 自动
        if (max_iter <= 0) max_iter = 100;
        if (tolerance <= 0.0) tolerance = 1e-6;
    }

    DPSFFitParams params;
    params.fitRadius = fit_radius;
    params.maxIter = max_iter;
    params.tolerance = tolerance;

    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "[PSF] 调用 dpsf_fit_batch (fitRadius=%d, maxIter=%d, tol=%.1e) ...",
            fit_radius, max_iter, tolerance);
        LOG_INFO("orchestrator", std::string(buf));
    }
    DPSFFitResult* results = nullptr;
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

    // 3. 从 header KV 读取 FILTER + WCS + SIP
    std::string filter_str;
    const char* filter_cstr = fn_kv_get(frame_, "header", "FILTER");
    if (filter_cstr) filter_str = filter_cstr;
    std::string filter_name = map_filter_name(filter_str);
    LOG_INFO("orchestrator", "[PHOTOMETRIC] FILTER='" + filter_str + "' -> '" + filter_name + "'");

    // 加载滤光片曲线
    // P03-002: 优先从 config 读取 filters_json 路径, 空时使用默认值
    std::string filters_json = orc_getJsonString(current_config_json_, "filters_json");
    if (filters_json.empty()) {
        filters_json = project_root_dir_ + "/lib/photometric_calib/data/response_curves/filters.json";
    } else if (!fs::path(filters_json).is_absolute()) {
        filters_json = (fs::path(project_root_dir_) / filters_json).string();
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
        // P03-002: 优先从 config 读取 qe_curves_json 路径, 空时使用默认值
        std::string qe_json = orc_getJsonString(current_config_json_, "qe_curves_json");
        if (qe_json.empty()) {
            qe_json = project_root_dir_ + "/lib/photometric_calib/data/response_curves/qe_curves.json";
        } else if (!fs::path(qe_json).is_absolute()) {
            qe_json = (fs::path(project_root_dir_) / qe_json).string();
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

    // P03-002: 从 current_config_json_ 解析 photometric 参数 (mag_min/mag_max/fov_radius_deg)
    // 默认值与原硬编码一致: mag_min=6.0, mag_max=16.0, fov_radius_deg=0(自动计算)
    double mag_min = 6.0;
    double mag_max = 16.0;
    double cfg_fov_radius = 0.0;
    if (!current_config_json_.empty()) {
        mag_min = orc_getJsonNum(current_config_json_, "mag_min", 6.0);
        mag_max = orc_getJsonNum(current_config_json_, "mag_max", 16.0);
        cfg_fov_radius = orc_getJsonNum(current_config_json_, "fov_radius_deg", 0.0);
        if (mag_min < 0) mag_min = 0.0;
        if (mag_max <= mag_min) mag_max = 16.0;
    }
    // P03-002: 若 config 指定 fov_radius_deg > 0, 覆盖自动计算值
    if (cfg_fov_radius > 0.0 && cfg_fov_radius < 30.0) {
        fov_radius_deg = cfg_fov_radius;
        LOG_INFO("orchestrator", "[PHOTOMETRIC] FOV 半径 (来自 config): " + std::to_string(fov_radius_deg) + "deg");
    }

    // 4. 调用 pc_calibrate_simple_with_gaia (DLL 内部: 锥形搜索+光谱积分+星匹配+scale 校正)
    // 签名扩展 (GAP-012): 新增 qe_wl/qe_trans/qe_count 三个参数, 位于 filter 参数之后 spectrum 参数之前
    auto fn_pc_calib = dll_loader_.get_function<int (*)(
        void*, double, double, double, double, double,
        const double*, const double*, int,
        const double*, const double*, int,
        const double*, int,
        const float*, int, int,
        const double*, const double*, const double*, const int*, int,
        double, double, double, double, double, double, double, double,
        int, const double*, const double*, const double*, const double*,
        float*, int*, double*, double*, PhotometricDiag*)>(
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
    // P12-001: 分阶段诊断结构体 (出参, DLL 内部填充 8 阶段字段)
    PhotometricDiag diag = {};

    // SIP 指针 (无 SIP 时传 nullptr)
    const double* sip_a_ptr = (wcs.sip_order > 0) ? wcs.sip_a : nullptr;
    const double* sip_b_ptr = (wcs.sip_order > 0) ? wcs.sip_b : nullptr;
    const double* sip_ap_ptr = (wcs.has_ap) ? wcs.sip_ap : nullptr;
    const double* sip_bp_ptr = (wcs.has_ap) ? wcs.sip_bp : nullptr;

    {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
            "[PHOTOMETRIC] 调用 pc_calibrate_simple_with_gaia (mag_min=%.1f, mag_max=%.1f, fov=%.3fdeg) ...",
            mag_min, mag_max, fov_radius_deg);
        LOG_INFO("orchestrator", std::string(buf));
    }
    int ret = fn_pc_calib(
        reinterpret_cast<void*>(reinterpret_cast<GaiaClient*>(gaia_client_handle_)),
        wcs.crval1, wcs.crval2, fov_radius_deg,
        mag_min, mag_max,  // P03-002: 从 config 读取 (DLL 内部会自适应迭代)
        filter_wl.data(), filter_trans.data(), (int)filter_wl.size(),
        qe_wl.empty() ? nullptr : qe_wl.data(),
        qe_trans.empty() ? nullptr : qe_trans.data(),
        (int)qe_wl.size(),
        spectrum_wl.data(), (int)spectrum_wl.size(),
        pixels, width, height,
        psf_cx.data(), psf_cy.data(), psf_flux.data(), psf_status.data(), n_psf,
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
    // out_pixels 所有权已转移给 frame_

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
    // 输出到 .hiss 同目录 (current_output_path_ 的父目录)
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

    // 2) 从 current_config_json_ 解析 nside_strategy 和 nside_override
    //    默认: strategy="1x_to_2x_drizzle", override=0 (自适应)
    //    P03-002: 新增 pixfrac 解析 (默认 1.0 避免缝隙)
    std::string nside_strategy = "1x_to_2x_drizzle";
    int nside_override = 0;
    double pixfrac = 1.0;
    int nested = 1;
    if (!current_config_json_.empty()) {
        size_t p = orc_findJsonKey(current_config_json_, "nside_strategy");
        if (p != std::string::npos) {
            std::string s = orc_extractJsonStr(current_config_json_, p);
            if (!s.empty()) nside_strategy = s;
        }
        p = orc_findJsonKey(current_config_json_, "nside_override");
        if (p != std::string::npos) {
            nside_override = (int)orc_extractJsonNum(current_config_json_, p);
        }
        // P03-002: 解析 pixfrac (在 drizzle 段或顶层)
        // 优先在 drizzle 段查找, 找不到再在顶层查找
        double pf = orc_getJsonNum(current_config_json_, "pixfrac", -1.0);
        if (pf >= 0.0 && pf <= 1.0) {
            pixfrac = pf;
        }
        // P03-002: 解析 nested (默认 true)
        bool nest_val = orc_getJsonBool(current_config_json_, "nested", true);
        nested = nest_val ? 1 : 0;
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

    // 调用 hp_drizzle_run
    // P03-002: pixfrac 从 config 读取 (默认 1.0), nested 从 config 读取 (默认 true)
    // output_path = current_output_path_ (.hiss 路径)
    HpDrizzleResult driz_result;
    std::memset(&driz_result, 0, sizeof(HpDrizzleResult));
    LOG_INFO("orchestrator", "[DRIZZLE] 输出: " + current_output_path_);

    int ret = fn_drizzle(frame_, nside, nested, pixfrac,
                         current_output_path_.c_str(), &driz_result);
    if (ret != 0) {
        std::string err = driz_result.error_msg[0] != '\0'
            ? std::string(driz_result.error_msg)
            : std::to_string(ret);
        LOG_ERROR("orchestrator", "[DRIZZLE] hp_drizzle_run 失败: " + err);
        result.error_msg = "[DRIZZLE] hp_drizzle_run 失败: " + err;
        result.exit_code = AstroCsExitCode::DRIZZLE_FAILED;
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
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
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
    float* pixels = fn_get_pixels(image);
    LOG_INFO("orchestrator", "[READ_FITS] 图像尺寸: " + std::to_string(width) + "x" + std::to_string(height));

    if (width <= 0 || height <= 0 || pixels == nullptr) {
        LOG_ERROR("orchestrator", "[READ_FITS] 像素数据无效");
        result.error_msg = "[READ_FITS] 像素数据无效";
        result.exit_code = AstroCsExitCode::FILE_IO_ERROR;
        fn_free(image);
        return false;
    }

    // 添加 data 块 (FLOAT32 [H,W], 拷贝)
    // P03-003: data 是必需块, 写入失败必须返回非零 (退出码 3)
    int dims[2] = {height, width};
    int ret = fn_add_block(frame_, "data", AIO_BLOCK_FLOAT32,
                           pixels, static_cast<int64_t>(width) * height,
                           dims, 2, "校准前 Light 像素");
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
    LOG_INFO("orchestrator", "[SNR] 开始");

    // P03-003: SNR 是可选 stage (drizzle 不强依赖 snr_model 块), 允许降级
    // 但降级必须记录到 photo_stats/SNR_STATUS, 禁止静默跳过
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::SNR)) {
        LOG_WARN("orchestrator", "[SNR] SNR DLL 未加载, 降级跳过 (记录到 photo_stats)");
        // 尝试写入 photo_stats 标记 (AIO 可能可用)
        if (dll_loader_.is_loaded(ModuleId::AIO)) {
            auto fn_kv_set_skipped = dll_loader_.get_function<int (*)(
                PipelineFrame*, const char*, const char*, const char*)>(
                ModuleId::AIO, "aio_frame_kv_set");
            if (fn_kv_set_skipped && frame_) {
                fn_kv_set_skipped(frame_, "photo_stats", "SNR_STATUS", "SKIPPED_NO_DLL");
            }
        }
        return true;  // 可选 stage, 允许降级继续
    }

    // P03-003: frame_ 为空属于内部错误, 不再静默跳过
    if (frame_ == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] frame_ 为空 (PipelineFrame 未初始化)");
        result.error_msg = "[SNR] frame_ 为空 (PipelineFrame 未初始化)";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 获取函数指针 (GAP-011: 改用 snr_extract_model / snr_free_model)
    auto fn_extract = dll_loader_.get_function<int (*)(
        const double*, int, double,
        const SnrWcsParams*, SnrModel*)>(
        ModuleId::SNR, "snr_extract_model");
    auto fn_free = dll_loader_.get_function<void (*)(SnrModel*)>(
        ModuleId::SNR, "snr_free_model");
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
    // P03-003: 用于在降级时写入 photo_stats/SNR_STATUS
    auto fn_kv_set = dll_loader_.get_function<int (*)(
        PipelineFrame*, const char*, const char*, const char*)>(
        ModuleId::AIO, "aio_frame_kv_set");

    if (!fn_extract || !fn_free || !fn_get_block || !fn_remove_block
        || !fn_add_block_move || !fn_kv_get_double) {
        LOG_ERROR("orchestrator", "[SNR] 函数指针获取失败");
        result.error_msg = "[SNR] 函数指针获取失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 读取 psf 块 (FLOAT64 [N,9], 每行 [status,B,flux,cx,cy,fwhm,A,mad,eccentricity])
    // P03-003: psf 块缺失时降级 (记录到 photo_stats), 不静默跳过
    const AioBlock* psf_block = fn_get_block(frame_, "psf");
    if (psf_block == nullptr) {
        LOG_WARN("orchestrator", "[SNR] psf 块不存在 (PSF 阶段未产出), 降级跳过");
        if (fn_kv_set) fn_kv_set(frame_, "photo_stats", "SNR_STATUS", "SKIPPED_NO_PSF");
        return true;  // 可选 stage, 允许降级继续
    }
    int n_stars = psf_block->dims[0];
    const double* psf_data = static_cast<const double*>(psf_block->data);

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

    // 调用 snr_extract_model 提取稀疏控制点
    SnrModel model = {};
    int ret = fn_extract(psf_data, n_stars, sigma_residual, &wcs, &model);
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

    LOG_INFO("orchestrator", "[SNR] 提取稀疏控制点: n_points=" + std::to_string(model.n_points)
             + ", snr_phot=" + std::to_string(model.snr_phot)
             + ", median_snr=" + std::to_string(model.median_snr)
             + ", idw_power=" + std::to_string(model.idw_power));

    // 序列化 SnrModel 到 "snr_model" 块 (AIO_BLOCK_RAW 类型)
    // 格式 (与 hp_drizzle_api.cpp 行 409-480 期望一致):
    //   [n_points:   uint32 (4B)]
    //   [points:     n_points × 20B]  // SnrControlPoint: ra(double 8B)+dec(double 8B)+snr_psf(float 4B)
    //   [snr_phot:   double 8B]
    //   [median_snr: double 8B]
    //   [idw_power:  double 8B]
    // 总字节数 = 4 + n_points*20 + 24
    uint32_t n_points = model.n_points;
    size_t payload_size = 4 + (size_t)n_points * 20 + 24;
    // fn_add_block_move 要求 data 必须是 malloc 分配 (frame 用 free() 释放)
    uint8_t* buffer = static_cast<uint8_t*>(std::malloc(payload_size));
    if (buffer == nullptr) {
        LOG_ERROR("orchestrator", "[SNR] 分配 snr_model 缓冲失败 (size=" + std::to_string(payload_size) + ")");
        result.error_msg = "[SNR] 分配 snr_model 缓冲失败";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        fn_free(&model);
        return false;
    }
    uint8_t* p = buffer;
    std::memcpy(p, &n_points, 4);                          p += 4;
    std::memcpy(p, model.points, (size_t)n_points * 20);  p += (size_t)n_points * 20;
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
        fn_free(&model);
        return true;  // 可选块写入失败, 允许降级继续
    }
    // buffer 所有权已转移给 frame_, 不能再 free

    // 释放 SnrModel 内部资源 (points 数组, 由 snr_estimator DLL 分配)
    fn_free(&model);

    LOG_INFO("orchestrator", "[SNR] 完成 (snr_model 块已写入, payload=" + std::to_string(payload_size) + "B)");
    return true;
}

// stage 8: GRADIENT_SPHERE - 球面梯度校准 (healpix_stack.dll hp_stack_gradient_corrected)
bool Orchestrator::run_stage_gradient_sphere(TaskResult& result) {
    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 开始");

    // P03-003: GRADIENT_SPHERE 是 stage2 必需 stage, DLL 未加载必须失败 (退出码 2)
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::GRADIENT_SPHERE)) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] GRADIENT_SPHERE DLL 未加载 (必需模块)");
        result.error_msg = "[GRADIENT_SPHERE] DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // P03-003: 必需输入缺失必须失败 (退出码 8)
    if (stage2_hiss_files_.empty()) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] 无 .hiss 输入文件");
        result.error_msg = "[GRADIENT_SPHERE] 无 .hiss 输入文件";
        result.exit_code = AstroCsExitCode::FILE_IO_ERROR;
        return false;
    }

    // 获取函数指针 (GAP-017: 签名新增 sigma_clip_method/winsorize_low/high_pct 3 个参数)
    auto fn_gradient = dll_loader_.get_function<int (*)(
        const char**, int, const char*, const char*,
        double, int, int, double,
        const char*, double, double)>(
        ModuleId::GRADIENT_SPHERE, "hp_stack_gradient_corrected");

    if (!fn_gradient) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] hp_stack_gradient_corrected 函数未找到");
        result.error_msg = "[GRADIENT_SPHERE] 函数未找到";
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    // 构建 hiss_paths 数组 (const char**)
    int n_frames = static_cast<int>(stage2_hiss_files_.size());
    std::vector<const char*> hiss_paths;
    hiss_paths.reserve(n_frames);
    for (const auto& p : stage2_hiss_files_) {
        hiss_paths.push_back(p.c_str());
    }

    // GAP-017: 从 current_config_json_ (stage2 config) 解析 sigma_clip 参数
    // 默认: sigma_clip_method="standard", sigma=3.0, max_iter=5
    //       winsorize_low_pct=0.05, winsorize_high_pct=0.95 (winsorized 模式下生效)
    std::string sigma_clip_method = "standard";
    double sigma = 3.0;
    int max_iter = 5;
    double winsorize_low_pct = 0.05;
    double winsorize_high_pct = 0.95;
    if (!current_config_json_.empty()) {
        size_t p = orc_findJsonKey(current_config_json_, "sigma_clip_method");
        if (p != std::string::npos) {
            std::string s = orc_extractJsonStr(current_config_json_, p);
            if (!s.empty()) sigma_clip_method = s;
        }
        p = orc_findJsonKey(current_config_json_, "sigma_clip_sigma");
        if (p != std::string::npos) {
            sigma = orc_extractJsonNum(current_config_json_, p);
            if (sigma <= 0.0) sigma = 3.0;
        }
        p = orc_findJsonKey(current_config_json_, "sigma_clip_max_iter");
        if (p != std::string::npos) {
            int v = (int)orc_extractJsonNum(current_config_json_, p);
            if (v > 0) max_iter = v;
        }
        p = orc_findJsonKey(current_config_json_, "winsorize_low_pct");
        if (p != std::string::npos) {
            winsorize_low_pct = orc_extractJsonNum(current_config_json_, p);
        }
        p = orc_findJsonKey(current_config_json_, "winsorize_high_pct");
        if (p != std::string::npos) {
            winsorize_high_pct = orc_extractJsonNum(current_config_json_, p);
        }
    }

    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 帧数: " + std::to_string(n_frames)
             + " 输出: " + current_output_hcsd_
             + " sigma_clip=" + sigma_clip_method
             + " sigma=" + std::to_string(sigma)
             + " max_iter=" + std::to_string(max_iter));

    // P03-002: 从 config 解析 gradient_sphere 参数
    // gaia_data_dir: 空时传 nullptr (跳过星拒绝)
    // gradient_max_iter: 默认 10
    // gradient_lambda: 默认 1e-4
    std::string cfg_gaia_dir = orc_getJsonString(current_config_json_, "gaia_data_dir");
    int gradient_max_iter = static_cast<int>(orc_getJsonNum(current_config_json_, "gradient_max_iter", 10.0));
    double gradient_lambda = orc_getJsonNum(current_config_json_, "gradient_lambda", 1.0e-4);
    if (gradient_max_iter <= 0) gradient_max_iter = 10;
    if (gradient_lambda <= 0.0) gradient_lambda = 1.0e-4;
    // 相对路径基于 project_root_dir_ 解析
    if (!cfg_gaia_dir.empty() && !fs::path(cfg_gaia_dir).is_absolute()) {
        cfg_gaia_dir = (fs::path(project_root_dir_) / cfg_gaia_dir).string();
    }
    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] gradient_max_iter=" + std::to_string(gradient_max_iter)
             + " gradient_lambda=" + std::to_string(gradient_lambda)
             + " gaia_data_dir=" + (cfg_gaia_dir.empty() ? "(null, 跳过星拒绝)" : cfg_gaia_dir));

    // 调用 hp_stack_gradient_corrected (GAP-017: 传入 sigma_clip_method + winsorize 参数)
    // P03-002: gradient_max_iter/gradient_lambda/gaia_data_dir 从 config 读取
    const char* method_cstr = sigma_clip_method.c_str();
    const char* gaia_dir_cstr = cfg_gaia_dir.empty() ? nullptr : cfg_gaia_dir.c_str();
    int ret = fn_gradient(hiss_paths.data(), n_frames,
                          gaia_dir_cstr,  // P03-002: 从 config 读取, 空时 nullptr
                          current_output_hcsd_.c_str(),
                          sigma, max_iter, gradient_max_iter, gradient_lambda,
                          method_cstr, winsorize_low_pct, winsorize_high_pct);
    if (ret != 0) {
        LOG_ERROR("orchestrator", "[GRADIENT_SPHERE] hp_stack_gradient_corrected 失败: ret=" + std::to_string(ret));
        result.error_msg = "[GRADIENT_SPHERE] 失败: " + std::to_string(ret);
        result.exit_code = AstroCsExitCode::GENERIC_ERROR;
        return false;
    }

    LOG_INFO("orchestrator", "[GRADIENT_SPHERE] 完成");
    return true;
}

// stage 9: STACK - Winsorized sigma clip + SNR²加权叠加 (healpix_stack.dll)
bool Orchestrator::run_stage_stack(TaskResult& result) {
    LOG_INFO("orchestrator", "[STACK] 开始");

    // P03-003: STACK 是 stage2 必需 stage, DLL 未加载必须失败 (退出码 2)
    // 注: 当前 STACK 是骨架, .hcsd 由 GRADIENT_SPHERE 生成, 但 DLL 加载仍必需
    if (!dlls_loaded_ || !dll_loader_.is_loaded(ModuleId::STACK)) {
        LOG_ERROR("orchestrator", "[STACK] STACK DLL 未加载 (必需模块)");
        result.error_msg = "[STACK] STACK DLL 未加载 (必需模块)";
        result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
        return false;
    }

    // GRADIENT_SPHERE 阶段已通过 hp_stack_gradient_corrected 完成完整流程:
    //   采样 → Gauss-Seidel 梯度拟合 → 校正叠加 → .hcsd 输出
    // hp_stack_run 接收 PipelineFrame 数组 (非 .hiss 文件), 不适用于 stage2
    // 当前保留骨架, .hcsd 已由 GRADIENT_SPHERE 生成
    LOG_INFO("orchestrator", "[STACK] 跳过 (骨架): .hcsd 已由 GRADIENT_SPHERE 生成");
    return true;
}

// ============================================================================
// run_stage1 - spec §2.3.3 单帧预处理 (FITS -> .hiss, stage 0-7)
// 串行执行 8 个 stage, 各阶段调用对应 DLL 模块 (骨架), 输出 timings
// P04-004: 集成取消 token / stage 超时 / 原子输出清理
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

    // GAP-016: 保存 config_json 供 run_stage_drizzle 读取 nside_strategy/nside_override
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
    current_output_file_ = output_hiss;

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
    } atomic_guard{this, output_hiss, result.success, config_.allow_partial_output};

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

    // 加载 DLL (P03-003: 必需模块缺失必须失败, 不再静默继续)
    if (!dlls_loaded_) {
        std::string err;
        // lib_base_dir 留空, 使用相对路径 (项目根目录执行)
        if (!init_dlls("", err)) {
            // 检查必需模块 (AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/DRIZZLE) 是否加载
            // SNR 是可选模块, 缺失时允许降级继续
            bool aio_ok = dll_loader_.is_loaded(ModuleId::AIO);
            bool calibrate_ok = dll_loader_.is_loaded(ModuleId::CALIBRATE);
            bool platesolve_ok = dll_loader_.is_loaded(ModuleId::PLATESOLVE);
            bool psf_ok = dll_loader_.is_loaded(ModuleId::PSF);
            bool photometric_ok = dll_loader_.is_loaded(ModuleId::PHOTOMETRIC);
            bool drizzle_ok = dll_loader_.is_loaded(ModuleId::DRIZZLE);
            if (!aio_ok || !calibrate_ok || !platesolve_ok || !psf_ok || !photometric_ok || !drizzle_ok) {
                LOG_ERROR("orchestrator", "DLL 加载失败 (必需模块缺失): " + err);
                result.error_msg = "DLL 加载失败 (必需模块缺失): " + err;
                result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
                state_ = TaskState::FAILED;
                return result;
            }
            // 仅可选模块 (SNR) 缺失, 允许继续 (SNR 阶段会降级)
            LOG_WARN("orchestrator", "DLL 加载警告 (仅可选模块缺失): " + err + " (SNR 将降级)");
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
    // P03-003: 失败时若 exit_code 未设置, 按 stage 兜底设置默认退出码
    // P04-004: 在 stage 开始前/后检查 cancel_token_/timeout_flag_, 触发时停止
    //          启动 watchdog 线程检测 stage 超时
    auto run_v2_with_timing = [&](PipelineStageV2 stage, const char* name,
                                   bool (Orchestrator::*fn)(TaskResult&)) -> bool {
        // P04-004: stage 开始前检查取消/超时
        if (!check_stage_continue(name, result)) {
            // 记录 timings (跳过的 stage, duration=0, success=false)
            StageTiming st;
            st.stage = PipelineStage::CALIBRATE;
            st.stage_name = std::string(name) + " (skipped)";
            st.duration_sec = 0.0;
            st.success = false;
            result.timings.push_back(st);
            return false;
        }

        // P04-004: 设置当前 stage 名称 (供 CLI 输出 cancelled 事件)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_stage_name_ = name;
        }

        LOG_INFO("orchestrator", "---------- stage1 阶段: " + std::string(name) + " ----------");

        // P04-004: 启动 watchdog 线程 (如果该 stage 配置了超时)
        double timeout_sec = 0.0;
        auto it = config_.stage_timeouts.find(name);
        if (it != config_.stage_timeouts.end()) {
            timeout_sec = it->second;
        }
        std::thread watchdog;
        bool watchdog_active = false;
        if (timeout_sec > 0.0) {
            watchdog_active = true;
            LOG_INFO("orchestrator", "P04-004: 启动 watchdog for " + std::string(name)
                     + " timeout=" + std::to_string(timeout_sec) + "s");
            // 重置 watchdog 停止标志
            stage_watchdog_stop_.store(false, std::memory_order_release);
            watchdog = std::thread([this, name, timeout_sec]() {
                auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    std::chrono::duration<double>(timeout_sec));
                // P04-004: 超时 <= 100ms 时立即检查 deadline, 避免错过超时窗口
                int sleep_ms = (timeout_sec < 0.1) ? 1 : 100;
                // 每 sleep_ms 检查一次, 直到 deadline 或被通知停止
                while (std::chrono::steady_clock::now() < deadline) {
                    if (stage_watchdog_stop_.load(std::memory_order_acquire)) {
                        return;  // stage 已完成, watchdog 退出
                    }
                    if (is_cancelled()) return;  // 已取消, 不再触发超时
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
                // 到达 deadline, 触发超时标志
                if (!is_cancelled() &&
                    !stage_watchdog_stop_.load(std::memory_order_acquire)) {
                    timeout_flag_.store(true, std::memory_order_release);
                    LOG_WARN("orchestrator", "P04-004: stage " + std::string(name) + " 超时 ("
                             + std::to_string(timeout_sec) + "s), 触发 timeout_flag");
                }
            });
        }

        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        // P04-004: 通知 watchdog 停止并 join (避免线程泄漏)
        if (watchdog_active) {
            stage_watchdog_stop_.store(true, std::memory_order_release);
            if (watchdog.joinable()) {
                watchdog.join();
            }
        }

        StageTiming st;
        st.stage = PipelineStage::CALIBRATE;  // 复用旧枚举 (StageTiming 仍用旧枚举)
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));

        // P04-004: stage 执行后检查取消/超时 (可能在执行期间被设置)
        // 优先级: 取消 > 超时 > stage 失败
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

        // P03-003: 兜底 exit_code (stage handler 未设置时按 stage 类型推导)
        if (!ok && result.exit_code == AstroCsExitCode::SUCCESS) {
            switch (stage) {
                case PipelineStageV2::READ_FITS:   result.exit_code = AstroCsExitCode::FILE_IO_ERROR; break;
                case PipelineStageV2::CALIBRATE:   result.exit_code = AstroCsExitCode::CALIBRATE_FAILED; break;
                case PipelineStageV2::PLATESOLVE:  result.exit_code = AstroCsExitCode::PLATESOLVE_FAILED; break;
                case PipelineStageV2::PSF:         result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::PHOTOMETRIC: result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::SNR:         result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                case PipelineStageV2::DRIZZLE:     result.exit_code = AstroCsExitCode::DRIZZLE_FAILED; break;
                default:                            result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
            }
        }
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

    // P04-004: 原子输出清理 - 失败/取消/超时时删除部分输出文件
    // 默认 allow_partial_output=false (严格原子性), 删除部分输出
    // 若 allow_partial_output=true, 保留部分输出 (标记 partial)
    if (!ok) {
        if (!config_.allow_partial_output) {
            LOG_INFO("orchestrator", "P04-004: 原子性清理 - stage1 失败/取消/超时, 删除部分输出");
            cleanup_partial_output(output_hiss);
            result.output_ahpx_path = "";
        } else {
            LOG_WARN("orchestrator", "P04-004: allow_partial_output=true, 保留部分输出: " + output_hiss);
            result.output_ahpx_path = output_hiss;  // 标记为 partial 输出
        }
    } else {
        result.output_ahpx_path = output_hiss;
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

    // 加载 DLL (P03-003: stage2 必需模块 GRADIENT_SPHERE/STACK 缺失必须失败)
    if (!dlls_loaded_) {
        std::string err;
        if (!init_dlls("", err)) {
            // 检查 stage2 必需模块 (GRADIENT_SPHERE/STACK 共用 healpix_stack.dll)
            bool gs_ok = dll_loader_.is_loaded(ModuleId::GRADIENT_SPHERE);
            bool stack_ok = dll_loader_.is_loaded(ModuleId::STACK);
            if (!gs_ok || !stack_ok) {
                LOG_ERROR("orchestrator", "DLL 加载失败 (stage2 必需模块缺失): " + err);
                result.error_msg = "DLL 加载失败 (stage2 必需模块缺失): " + err;
                result.exit_code = AstroCsExitCode::DLL_LOAD_FAILED;
                return result;
            }
            // 仅 stage1 模块缺失, stage2 仍可继续
            LOG_WARN("orchestrator", "DLL 加载警告 (仅 stage1 模块缺失): " + err);
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
    // P04-004: 集成取消/超时检查与 watchdog
    auto run_v2_with_timing = [&](PipelineStageV2 stage, const char* name,
                                   bool (Orchestrator::*fn)(TaskResult&)) -> bool {
        // P04-004: stage 开始前检查取消/超时
        if (!check_stage_continue(name, result)) {
            StageTiming st;
            st.stage = PipelineStage::STACK;
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

        LOG_INFO("orchestrator", "---------- stage2 阶段: " + std::string(name) + " ----------");

        // P04-004: 启动 watchdog 线程 (如果该 stage 配置了超时)
        double timeout_sec = 0.0;
        auto it = config_.stage_timeouts.find(name);
        if (it != config_.stage_timeouts.end()) {
            timeout_sec = it->second;
        }
        std::thread watchdog;
        bool watchdog_active = false;
        if (timeout_sec > 0.0) {
            watchdog_active = true;
            LOG_INFO("orchestrator", "P04-004: 启动 watchdog for " + std::string(name)
                     + " timeout=" + std::to_string(timeout_sec) + "s");
            stage_watchdog_stop_.store(false, std::memory_order_release);
            watchdog = std::thread([this, name, timeout_sec]() {
                auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    std::chrono::duration<double>(timeout_sec));
                // P04-004: 超时 <= 100ms 时立即检查 deadline, 避免错过超时窗口
                int sleep_ms = (timeout_sec < 0.1) ? 1 : 100;
                while (std::chrono::steady_clock::now() < deadline) {
                    if (stage_watchdog_stop_.load(std::memory_order_acquire)) return;
                    if (is_cancelled()) return;
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }
                if (!is_cancelled() &&
                    !stage_watchdog_stop_.load(std::memory_order_acquire)) {
                    timeout_flag_.store(true, std::memory_order_release);
                    LOG_WARN("orchestrator", "P04-004: stage " + std::string(name) + " 超时 ("
                             + std::to_string(timeout_sec) + "s), 触发 timeout_flag");
                }
            });
        }

        auto t0 = std::chrono::steady_clock::now();
        bool ok = (this->*fn)(result);
        auto t1 = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(t1 - t0).count();

        // P04-004: 通知 watchdog 停止并 join
        if (watchdog_active) {
            stage_watchdog_stop_.store(true, std::memory_order_release);
            if (watchdog.joinable()) {
                watchdog.join();
            }
        }

        StageTiming st;
        st.stage = PipelineStage::STACK;  // stage2 使用 STACK 枚举
        st.stage_name = name;
        st.duration_sec = dur;
        st.success = ok;
        result.timings.push_back(st);
        LOG_INFO("orchestrator", "[" + std::to_string(dur) + "s] " + name
                 + (ok ? " 完成" : " 失败"));

        // P04-004: stage 执行后检查取消/超时
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

        // P03-003: 兜底 exit_code (stage handler 未设置时按 stage 类型推导)
        if (!ok && result.exit_code == AstroCsExitCode::SUCCESS) {
            switch (stage) {
                case PipelineStageV2::GRADIENT_SPHERE:
                case PipelineStageV2::STACK:
                    result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
                default:
                    result.exit_code = AstroCsExitCode::GENERIC_ERROR; break;
            }
        }
        return ok;
    };

    bool ok = true;
    ok = ok && run_v2_with_timing(PipelineStageV2::GRADIENT_SPHERE, "GRADIENT_SPHERE",
                                   &Orchestrator::run_stage_gradient_sphere);
    ok = ok && run_v2_with_timing(PipelineStageV2::STACK,           "STACK",
                                   &Orchestrator::run_stage_stack);

    result.success = ok;

    // P04-004: 原子输出清理 - 失败/取消/超时时删除部分输出文件
    if (!ok) {
        if (!config_.allow_partial_output) {
            LOG_INFO("orchestrator", "P04-004: 原子性清理 - stage2 失败/取消/超时, 删除部分输出");
            cleanup_partial_output(output_hcsd);
            result.output_ahpx_path = "";
        } else {
            LOG_WARN("orchestrator", "P04-004: allow_partial_output=true, 保留部分输出: " + output_hcsd);
            result.output_ahpx_path = output_hcsd;
        }
    } else {
        result.output_ahpx_path = output_hcsd;
    }
    state_ = ok ? TaskState::COMPLETED : TaskState::FAILED;

    LOG_INFO("orchestrator", "========== stage2 "
             + std::string(ok ? "完成 (成功)" : "失败") + " ==========");
    return result;
}
