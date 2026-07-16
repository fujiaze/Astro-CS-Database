// ============================================================================
// cli_command.h - 单次命令执行模式
// 功能: 解析命令行参数并执行单次任务 (非交互式)
// 用途: 供脚本/批处理调用, 输出 JSON 格式结果到 stdout
//
// 支持命令:
//   orchestrator run <fits> [--config <json>] [--threads <N>] [--fresh]
//   orchestrator run-batch <dir> [--config <json>] [--threads <N>] [--fresh]
//   orchestrator stage1 --frame <fits> --output <hiss> [options]  (spec §2.3.3)
//   orchestrator stage2 --frames <dir> --output <hcsd> [options]  (spec §2.3.3)
//   orchestrator status
//   orchestrator --help
// ============================================================================

#pragma once

#include <string>
#include "orchestrator.h"

class CliCommand {
public:
    // 解析命令行参数并执行
    // 返回: 进程退出码 (0=成功, 非0=失败)
    static int execute(int argc, char* argv[]);

private:
    // 单次命令处理
    static int cmd_run(const std::string& fits_path,
                       const std::string& config_path,
                       int threads,
                       bool fresh,
                       const std::string& log_level = "");
    static int cmd_run_batch(const std::string& dir_path,
                             const std::string& config_path,
                             int threads,
                             bool fresh,
                             const std::string& log_level = "");
    static int cmd_status();

    // spec §2.3.3 两段流水线 CLI 命令
    // stage1: 单帧预处理 (FITS -> .hiss, stage 0-7)
    static int cmd_stage1(const std::string& fits_path,
                          const std::string& output_hiss,
                          const std::string& config_path,
                          const std::string& log_level = "");
    // stage2: 多帧合并 (.hiss -> .hcsd, stage 8-9)
    static int cmd_stage2(const std::string& hiss_dir,
                          const std::string& output_hcsd,
                          const std::string& config_path,
                          const std::string& log_level = "");

    // 输出 JSON 结果到 stdout
    static void output_json_result(const TaskResult& result);
    static void output_json_batch(const std::vector<TaskResult>& results);

    // 辅助
    static void print_usage();
};
