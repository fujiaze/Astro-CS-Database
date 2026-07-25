// ============================================================================
// cli_command.cpp - 单次命令执行模式实现
// 功能: 解析命令行参数并执行单次任务, 输出 JSON 结果到 stdout
// ============================================================================

#include "cli_command.h"
#include "logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// 辅助: JSON 字符串转义
// ============================================================================
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// 辅助: 阶段枚举转字符串
static const char* stage_str(PipelineStage s) {
    switch (s) {
        case PipelineStage::CALIBRATE:   return "CALIBRATE";
        case PipelineStage::PLATESOLVE:  return "PLATESOLVE";
        case PipelineStage::PHOTOMETRIC: return "PHOTOMETRIC";
        case PipelineStage::DRIZZLE:     return "DRIZZLE";
        case PipelineStage::STACK:       return "STACK";
        default:                         return "UNKNOWN";
    }
}

// ============================================================================
// execute - 解析命令行参数并执行
// ============================================================================
int CliCommand::execute(int argc, char* argv[]) {
    // orchestrator --help / -h
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string sub = argv[1];
    // 转小写
    std::transform(sub.begin(), sub.end(), sub.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (sub == "--help" || sub == "-h" || sub == "help") {
        print_usage();
        return 0;
    }

    if (sub == "run") {
        // orchestrator run <fits> [--config <json>] [--threads <N>] [--fresh] [--log-level <LEVEL>]
        std::string fits_path;
        std::string config_path;
        int threads = 0;
        bool fresh = false;
        std::string log_level;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--threads" && i + 1 < argc) {
                threads = std::atoi(argv[++i]);
            } else if (a == "--fresh") {
                fresh = true;
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else if (fits_path.empty()) {
                fits_path = a;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (fits_path.empty()) {
            LOG_ERROR("cli", "错误: 缺少 FITS 文件路径");
            print_usage();
            return 1;
        }
        return cmd_run(fits_path, config_path, threads, fresh, log_level);
    }

    if (sub == "run-batch") {
        // orchestrator run-batch <dir> [--config <json>] [--threads <N>] [--fresh] [--log-level <LEVEL>]
        std::string dir_path;
        std::string config_path;
        int threads = 0;
        bool fresh = false;
        std::string log_level;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--threads" && i + 1 < argc) {
                threads = std::atoi(argv[++i]);
            } else if (a == "--fresh") {
                fresh = true;
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else if (dir_path.empty()) {
                dir_path = a;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (dir_path.empty()) {
            LOG_ERROR("cli", "错误: 缺少目录路径");
            print_usage();
            return 1;
        }
        return cmd_run_batch(dir_path, config_path, threads, fresh, log_level);
    }

    if (sub == "status") {
        return cmd_status();
    }

    // spec §2.3.3 stage1: 单帧预处理 (FITS -> .hiss, stage 0-7)
    if (sub == "stage1") {
        // orchestrator stage1 --frame <fits> --output <hiss>
        //     [--gaia-data <dir>] [--calibration-dir <dir>]
        //     [--filter <name>] [--config <json>] [--log-level <LEVEL>]
        std::string fits_path, output_hiss;
        std::string config_path, log_level;
        // 以下参数当前仅记录日志 (后续 Task 传入 stage1_config)
        std::string gaia_data, calibration_dir, filter_name;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--frame" && i + 1 < argc) {
                fits_path = argv[++i];
            } else if (a == "--output" && i + 1 < argc) {
                output_hiss = argv[++i];
            } else if (a == "--gaia-data" && i + 1 < argc) {
                gaia_data = argv[++i];
            } else if (a == "--calibration-dir" && i + 1 < argc) {
                calibration_dir = argv[++i];
            } else if (a == "--filter" && i + 1 < argc) {
                filter_name = argv[++i];
            } else if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (fits_path.empty()) {
            LOG_ERROR("cli", "错误: stage1 缺少 --frame <fits> 参数");
            print_usage();
            return 1;
        }
        if (output_hiss.empty()) {
            LOG_ERROR("cli", "错误: stage1 缺少 --output <hiss> 参数");
            print_usage();
            return 1;
        }
        // gaia_data/calibration_dir/filter_name 当前仅记录日志 (后续 Task 集成)
        if (!gaia_data.empty())        LOG_INFO("cli", "stage1 gaia-data: " + gaia_data);
        if (!calibration_dir.empty())  LOG_INFO("cli", "stage1 calibration-dir: " + calibration_dir);
        if (!filter_name.empty())      LOG_INFO("cli", "stage1 filter: " + filter_name);

        return cmd_stage1(fits_path, output_hiss, config_path, log_level);
    }

    // spec §2.3.3 stage2: 多帧合并 (.hiss -> .hcsd, stage 8-9)
    if (sub == "stage2") {
        // orchestrator stage2 --frames <hiss_dir> --output <hcsd>
        //     [--config <json>] [--log-level <LEVEL>]
        std::string hiss_dir, output_hcsd;
        std::string config_path, log_level;

        for (int i = 2; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--frames" && i + 1 < argc) {
                hiss_dir = argv[++i];
            } else if (a == "--output" && i + 1 < argc) {
                output_hcsd = argv[++i];
            } else if (a == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (a == "--log-level" && i + 1 < argc) {
                log_level = argv[++i];
            } else if (a.rfind("--", 0) == 0) {
                LOG_ERROR("cli", "未知参数: " + a);
                print_usage();
                return 1;
            } else {
                LOG_ERROR("cli", "多余的位置参数: " + a);
                print_usage();
                return 1;
            }
        }

        if (hiss_dir.empty()) {
            LOG_ERROR("cli", "错误: stage2 缺少 --frames <dir> 参数");
            print_usage();
            return 1;
        }
        if (output_hcsd.empty()) {
            LOG_ERROR("cli", "错误: stage2 缺少 --output <hcsd> 参数");
            print_usage();
            return 1;
        }

        return cmd_stage2(hiss_dir, output_hcsd, config_path, log_level);
    }

    LOG_ERROR("cli", "未知子命令: " + std::string(argv[1]));
    print_usage();
    return 1;
}

// ============================================================================
// cmd_run - 单帧处理
// ============================================================================
int CliCommand::cmd_run(const std::string& fits_path,
                        const std::string& config_path,
                        int threads,
                        bool fresh,
                        const std::string& log_level) {
    Orchestrator orch;

    // 加载配置 (可选)
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
    }

    // 设置线程数 (通过配置, 当前骨架: 不直接设置, 后续 Task 完善)
    if (threads > 0) {
        LOG_INFO("cli", "请求线程数: " + std::to_string(threads) + " (骨架: 配置传递待完善)");
    }

    // Task 3: 设置 fresh_start (忽略检查点重新开始)
    if (fresh) {
        LOG_INFO("cli", "启用 fresh_start: 忽略检查点重新开始");
        orch.set_fresh_start(true);
    }

    // Task 4: 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_single(fits_path);
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// cmd_run_batch - 批量处理
// ============================================================================
int CliCommand::cmd_run_batch(const std::string& dir_path,
                              const std::string& config_path,
                              int threads,
                              bool fresh,
                              const std::string& log_level) {
    Orchestrator orch;

    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
    }

    // 检查目录是否存在 (Task 5: 集成测试要求 nonexistent_dir 返回错误)
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        LOG_ERROR("cli", "目录不存在或不是目录: " + dir_path);
        std::cout << "{" << std::endl;
        std::cout << "  \"total\": 0," << std::endl;
        std::cout << "  \"success_count\": 0," << std::endl;
        std::cout << "  \"failed_count\": 0," << std::endl;
        std::cout << "  \"results\": []" << std::endl;
        std::cout << "}" << std::endl;
        return 8;  // P03-003: 文件 I/O 错误 (原为 4)
    }

    if (threads > 0) {
        LOG_INFO("cli", "请求线程数: " + std::to_string(threads) + " (骨架: 配置传递待完善)");
    }

    // Task 3: 设置 fresh_start (忽略检查点重新开始)
    if (fresh) {
        LOG_INFO("cli", "启用 fresh_start: 忽略检查点重新开始");
        orch.set_fresh_start(true);
    }

    // Task 4: 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    std::vector<TaskResult> results = orch.run_batch(dir_path);
    output_json_batch(results);

    // P03-003: 任意失败则返回非零 (优先使用 TaskResult.exit_code, 否则用 1)
    for (const auto& r : results) {
        if (!r.success) return r.exit_code != 0 ? r.exit_code : 1;
    }
    return 0;
}

// ============================================================================
// cmd_status - 状态查询 (单次模式下无运行实例, 仅输出提示)
// ============================================================================
int CliCommand::cmd_status() {
    std::cout << "{" << std::endl;
    std::cout << "  \"mode\": \"single-command\"," << std::endl;
    std::cout << "  \"status\": \"no running instance\"" << std::endl;
    std::cout << "}" << std::endl;
    return 0;
}

// ============================================================================
// cmd_stage1 - spec §2.3.3 单帧预处理 (FITS -> .hiss, stage 0-7)
// ============================================================================
int CliCommand::cmd_stage1(const std::string& fits_path,
                           const std::string& output_hiss,
                           const std::string& config_path,
                           const std::string& log_level) {
    Orchestrator orch;

    // 加载配置 (可选, 后续 Task 解析 stage1_config.json 各字段)
    std::string config_json;
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
        // 读取配置文件原始内容作为 config_json 传给 run_stage1
        std::ifstream ifs(config_path, std::ios::binary);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            config_json = ss.str();
        }
    }

    // 设置日志级别 (覆盖配置中的 log_level)
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_stage1(fits_path, output_hiss, config_json);
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// cmd_stage2 - spec §2.3.3 多帧合并 (.hiss -> .hcsd, stage 8-9)
// ============================================================================
int CliCommand::cmd_stage2(const std::string& hiss_dir,
                           const std::string& output_hcsd,
                           const std::string& config_path,
                           const std::string& log_level) {
    Orchestrator orch;

    // 加载配置 (可选, 后续 Task 解析 stage2_config.json 各字段)
    std::string config_json;
    if (!config_path.empty()) {
        std::string err;
        if (!orch.load_config(config_path, err)) {
            LOG_ERROR("cli", "配置加载失败: " + err);
            return 7;  // P03-003: 配置错误 (原为 2)
        }
        std::ifstream ifs(config_path, std::ios::binary);
        if (ifs.is_open()) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            config_json = ss.str();
        }
    }

    // 设置日志级别
    if (!log_level.empty()) {
        LogLevel lvl = Logger::string_to_level(log_level);
        Logger::instance().set_level(lvl);
        LOG_INFO("cli", "日志级别设置为: " + log_level);
    }

    TaskResult r = orch.run_stage2(hiss_dir, output_hcsd, config_json);
    output_json_result(r);
    // P03-003: 使用 TaskResult.exit_code 传递细分错误码 (失败时若 exit_code=0 用 1 兜底)
    return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1);
}

// ============================================================================
// output_json_result - 输出单帧 JSON 结果
// ============================================================================
void CliCommand::output_json_result(const TaskResult& result) {
    std::cout << "{" << std::endl;
    std::cout << "  \"success\": " << (result.success ? "true" : "false") << "," << std::endl;
    std::cout << "  \"frame_name\": \"" << json_escape(result.frame_name) << "\"," << std::endl;

    // timings 数组
    std::cout << "  \"timings\": [" << std::endl;
    for (size_t i = 0; i < result.timings.size(); ++i) {
        const auto& t = result.timings[i];
        std::cout << "    {\"stage\": \"" << stage_str(t.stage)
                  << "\", \"name\": \"" << json_escape(t.stage_name)
                  << "\", \"duration_sec\": " << t.duration_sec
                  << ", \"success\": " << (t.success ? "true" : "false") << "}";
        if (i + 1 < result.timings.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "  ]," << std::endl;

    // wcs_fields 对象
    std::cout << "  \"wcs_fields\": {";
    bool first = true;
    for (const auto& kv : result.wcs_fields) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << json_escape(kv.first) << "\": \""
                  << json_escape(kv.second) << "\"";
        first = false;
    }
    std::cout << "}," << std::endl;

    // photo_stats 对象
    std::cout << "  \"photo_stats\": {";
    first = true;
    for (const auto& kv : result.photo_stats) {
        if (!first) std::cout << ", ";
        std::cout << "\"" << json_escape(kv.first) << "\": \""
                  << json_escape(kv.second) << "\"";
        first = false;
    }
    std::cout << "}," << std::endl;

    std::cout << "  \"output_ahpx_path\": \"" << json_escape(result.output_ahpx_path) << "\"," << std::endl;
    std::cout << "  \"error_msg\": \"" << json_escape(result.error_msg) << "\"" << std::endl;
    std::cout << "}" << std::endl;
}

// ============================================================================
// output_json_batch - 输出批量 JSON 结果
// ============================================================================
void CliCommand::output_json_batch(const std::vector<TaskResult>& results) {
    size_t n_ok = 0;
    for (const auto& r : results) {
        if (r.success) ++n_ok;
    }

    std::cout << "{" << std::endl;
    std::cout << "  \"total\": " << results.size() << "," << std::endl;
    std::cout << "  \"success_count\": " << n_ok << "," << std::endl;
    std::cout << "  \"failed_count\": " << (results.size() - n_ok) << "," << std::endl;
    std::cout << "  \"results\": [" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::cout << "    {\"success\": " << (r.success ? "true" : "false")
                  << ", \"frame_name\": \"" << json_escape(r.frame_name) << "\""
                  << ", \"error_msg\": \"" << json_escape(r.error_msg) << "\"}";
        if (i + 1 < results.size()) std::cout << ",";
        std::cout << std::endl;
    }
    std::cout << "  ]" << std::endl;
    std::cout << "}" << std::endl;
}

// ============================================================================
// print_usage - 输出用法说明
// ============================================================================
void CliCommand::print_usage() {
    std::cout << "============================================================" << std::endl;
    std::cout << "Orchestrator CLI (两段流水线版本, spec §2.3)" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "用法:" << std::endl;
    std::cout << "  orchestrator                                - 启动交互式 REPL" << std::endl;
    std::cout << "  orchestrator --help                         - 显示帮助" << std::endl;
    std::cout << "  orchestrator run <fits> [options]           - 单帧处理 (旧版 5 阶段)" << std::endl;
    std::cout << "  orchestrator run-batch <dir> [options]      - 批量处理 (旧版 5 阶段)" << std::endl;
    std::cout << "  orchestrator stage1 --frame <fits> --output <hiss> [options]" << std::endl;
    std::cout << "                                              - 单帧预处理 (stage 0-7, spec §2.3.3)" << std::endl;
    std::cout << "  orchestrator stage2 --frames <dir> --output <hcsd> [options]" << std::endl;
    std::cout << "                                              - 多帧合并 (stage 8-9, spec §2.3.3)" << std::endl;
    std::cout << "  orchestrator status                         - 状态查询 (无运行实例)" << std::endl;
    std::cout << std::endl;
    std::cout << "run 选项:" << std::endl;
    std::cout << "  --config <json>       配置文件路径" << std::endl;
    std::cout << "  --threads <N>         线程数 (0=自动检测)" << std::endl;
    std::cout << "  --fresh               忽略检查点重新开始 (Task 3)" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO, Task 4)" << std::endl;
    std::cout << std::endl;
    std::cout << "run-batch 选项:" << std::endl;
    std::cout << "  --config <json>       配置文件路径" << std::endl;
    std::cout << "  --threads <N>         线程数 (0=自动检测)" << std::endl;
    std::cout << "  --fresh               忽略检查点重新开始 (Task 3)" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO, Task 4)" << std::endl;
    std::cout << std::endl;
    std::cout << "stage1 选项 (单帧预处理 FITS -> .hiss):" << std::endl;
    std::cout << "  --frame <fits>        输入 FITS 文件路径 (必选)" << std::endl;
    std::cout << "  --output <hiss>       输出 .hiss 文件路径 (必选)" << std::endl;
    std::cout << "  --gaia-data <dir>     Gaia 数据目录" << std::endl;
    std::cout << "  --calibration-dir <dir> 校准文件目录" << std::endl;
    std::cout << "  --filter <name>       滤镜名称" << std::endl;
    std::cout << "  --config <json>       stage1_config.json 配置文件路径" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO)" << std::endl;
    std::cout << std::endl;
    std::cout << "stage2 选项 (多帧合并 .hiss -> .hcsd):" << std::endl;
    std::cout << "  --frames <dir>        输入 .hiss 文件目录 (必选)" << std::endl;
    std::cout << "  --output <hcsd>       输出 .hcsd 文件路径 (必选)" << std::endl;
    std::cout << "  --config <json>       stage2_config.json 配置文件路径" << std::endl;
    std::cout << "  --log-level <LEVEL>   日志级别 (DEBUG/INFO/WARN/ERROR, 默认 INFO)" << std::endl;
    std::cout << "============================================================" << std::endl;
}
