// ============================================================================
// cli_repl.cpp - 交互式 REPL 实现
// 功能: 启动交互式命令行, 解析用户输入, 调用 Orchestrator 对应方法
// ============================================================================

#include "cli_repl.h"
#include "logger.h"

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// 构造 / 析构
// ============================================================================
CliRepl::CliRepl() {
    std::cout << "[repl] 交互式编排器已就绪" << std::endl;
}

CliRepl::~CliRepl() {
    std::cout << "[repl] 退出交互式编排器" << std::endl;
}

// ============================================================================
// run - REPL 主循环
// ============================================================================
void CliRepl::run() {
    print_help();

    std::string line;
    while (true) {
        print_prompt();
        line = read_line();
        if (line.empty()) {
            continue;
        }

        // 解析命令与参数
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        std::string args;
        std::getline(iss, args);
        // 去掉 args 前导空白
        size_t p = args.find_first_not_of(" \t");
        if (p != std::string::npos) {
            args = args.substr(p);
        } else {
            args.clear();
        }

        // 命令分发 (大小写不敏感)
        std::string cmd_lower = cmd;
        std::transform(cmd_lower.begin(), cmd_lower.end(), cmd_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (cmd_lower == "exit" || cmd_lower == "quit") {
            handle_exit();
            break;
        } else if (cmd_lower == "help" || cmd_lower == "?") {
            print_help();
        } else if (cmd_lower == "load") {
            handle_load(args);
        } else if (cmd_lower == "run") {
            handle_run(args);
        } else if (cmd_lower == "run-batch") {
            handle_run_batch(args);
        } else if (cmd_lower == "status") {
            handle_status();
        } else if (cmd_lower == "pause") {
            handle_pause();
        } else if (cmd_lower == "resume") {
            handle_resume();
        } else if (cmd_lower == "interrupt") {
            handle_interrupt();
        } else if (cmd_lower == "checkpoint") {
            handle_checkpoint(args);
        } else if (cmd_lower == "log") {
            handle_log(args);
        } else {
            std::cout << "[repl] 未知命令: " << cmd
                      << " (输入 help 查看可用命令)" << std::endl;
        }
    }
}

// ============================================================================
// 命令处理
// ============================================================================
void CliRepl::handle_load(const std::string& args) {
    if (args.empty()) {
        std::cout << "[repl] 用法: load <config.json>" << std::endl;
        return;
    }
    std::string err;
    if (orchestrator_.load_config(args, err)) {
        std::cout << "[repl] 配置加载成功: " << args << std::endl;
    } else {
        std::cout << "[repl] 配置加载失败: " << err << std::endl;
    }
}

void CliRepl::handle_run(const std::string& args) {
    if (args.empty()) {
        std::cout << "[repl] 用法: run <frame.fts>" << std::endl;
        return;
    }
    TaskResult r = orchestrator_.run_single(args);
    if (r.success) {
        std::cout << "[repl] 处理成功: " << args << std::endl;
        if (!r.output_ahpx_path.empty()) {
            std::cout << "  输出文件: " << r.output_ahpx_path << std::endl;
        }
    } else {
        std::cout << "[repl] 处理失败: " << args
                  << " (" << r.error_msg << ")" << std::endl;
    }
}

void CliRepl::handle_run_batch(const std::string& args) {
    if (args.empty()) {
        std::cout << "[repl] 用法: run-batch <dir>" << std::endl;
        return;
    }
    std::vector<TaskResult> results = orchestrator_.run_batch(args);
    size_t n_ok = 0;
    for (const auto& r : results) {
        if (r.success) ++n_ok;
    }
    std::cout << "[repl] 批量完成: " << n_ok << "/" << results.size() << " 成功" << std::endl;
}

void CliRepl::handle_status() {
    // 状态枚举转字符串 (本地实现, 避免依赖 Orchestrator 私有方法)
    auto state_str = [](TaskState s) -> const char* {
        switch (s) {
            case TaskState::IDLE:        return "IDLE";
            case TaskState::RUNNING:     return "RUNNING";
            case TaskState::PAUSED:      return "PAUSED";
            case TaskState::INTERRUPTED: return "INTERRUPTED";
            case TaskState::COMPLETED:   return "COMPLETED";
            case TaskState::FAILED:      return "FAILED";
            default:                     return "UNKNOWN";
        }
    };
    auto stage_str = [](PipelineStage s) -> const char* {
        switch (s) {
            case PipelineStage::CALIBRATE:   return "CALIBRATE";
            case PipelineStage::PLATESOLVE:  return "PLATESOLVE";
            case PipelineStage::PHOTOMETRIC: return "PHOTOMETRIC";
            case PipelineStage::DRIZZLE:     return "DRIZZLE";
            case PipelineStage::STACK:       return "STACK";
            default:                         return "UNKNOWN";
        }
    };
    std::cout << "[repl] 状态:" << std::endl;
    std::cout << "  state:          " << state_str(orchestrator_.get_state()) << std::endl;
    std::cout << "  current_frame:  " << orchestrator_.get_current_frame() << std::endl;
    std::cout << "  current_stage:  " << stage_str(orchestrator_.get_current_stage()) << std::endl;
    std::cout << "  elapsed_time:   " << orchestrator_.get_elapsed_time() << "s" << std::endl;
    std::cout << "  memory_usage:   " << orchestrator_.get_memory_usage() << " bytes" << std::endl;
    std::cout << "  thread_count:   " << orchestrator_.get_thread_count() << std::endl;
}

void CliRepl::handle_pause() {
    orchestrator_.pause();
    std::cout << "[repl] 已发送暂停请求" << std::endl;
}

void CliRepl::handle_resume() {
    orchestrator_.resume();
    std::cout << "[repl] 已发送恢复请求" << std::endl;
}

void CliRepl::handle_interrupt() {
    orchestrator_.interrupt();
    std::cout << "[repl] 已发送中断请求" << std::endl;
}

void CliRepl::handle_checkpoint(const std::string& args) {
    // 子命令: checkpoint list / checkpoint clear / checkpoint <frame>
    if (args.empty()) {
        std::cout << "[repl] 用法:" << std::endl;
        std::cout << "  checkpoint <frame>   - 保存/查询指定帧的检查点" << std::endl;
        std::cout << "  checkpoint list      - 列出所有检查点" << std::endl;
        std::cout << "  checkpoint clear     - 清除所有检查点" << std::endl;
        return;
    }

    // 解析子命令 (大小写不敏感)
    std::string sub = args;
    std::transform(sub.begin(), sub.end(), sub.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (sub == "list") {
        // 列出所有检查点
        CheckpointManager& mgr = orchestrator_.get_checkpoint_manager();
        std::vector<std::string> list = mgr.list_all();
        if (list.empty()) {
            std::cout << "[repl] 当前无检查点 (目录: "
                      << mgr.get_checkpoint_dir() << ")" << std::endl;
        } else {
            std::cout << "[repl] 检查点列表 (" << list.size()
                      << " 个, 目录: " << mgr.get_checkpoint_dir()
                      << "):" << std::endl;
            for (const auto& name : list) {
                // 显示每条检查点的恢复起点
                int rs = mgr.get_resume_stage(name);
                std::cout << "  " << name;
                if (rs < 0) {
                    std::cout << "  [已完成]";
                } else {
                    std::cout << "  [resume=" << rs << "]";
                }
                std::cout << std::endl;
            }
        }
        return;
    }

    if (sub == "clear") {
        // 清除所有检查点
        CheckpointManager& mgr = orchestrator_.get_checkpoint_manager();
        std::vector<std::string> list = mgr.list_all();
        if (list.empty()) {
            std::cout << "[repl] 当前无检查点可清除" << std::endl;
        } else {
            mgr.clear_all();
            std::cout << "[repl] 已清除 " << list.size() << " 个检查点" << std::endl;
        }
        return;
    }

    // 默认: 保存/查询指定帧的检查点
    // 阶段枚举转字符串 (本地实现, 避免依赖 Orchestrator 私有方法)
    auto stage_str = [](PipelineStage s) -> const char* {
        switch (s) {
            case PipelineStage::CALIBRATE:   return "CALIBRATE";
            case PipelineStage::PLATESOLVE:  return "PLATESOLVE";
            case PipelineStage::PHOTOMETRIC: return "PHOTOMETRIC";
            case PipelineStage::DRIZZLE:     return "DRIZZLE";
            case PipelineStage::STACK:       return "STACK";
            default:                         return "UNKNOWN";
        }
    };

    if (orchestrator_.save_checkpoint(args)) {
        std::cout << "[repl] 检查点已保存: " << args << std::endl;
        PipelineStage from;
        if (orchestrator_.load_checkpoint(args, from)) {
            std::cout << "[repl] 检查点恢复点: " << static_cast<int>(from)
                      << " (" << stage_str(from) << ")" << std::endl;
        }
    } else {
        std::cout << "[repl] 检查点保存失败" << std::endl;
    }
}

void CliRepl::handle_log(const std::string& args) {
    // 子命令: log level <LEVEL> / log path
    if (args.empty()) {
        std::cout << "[repl] 用法:" << std::endl;
        std::cout << "  log level <LEVEL>   - 设置日志级别 (DEBUG/INFO/WARN/ERROR)" << std::endl;
        std::cout << "  log path            - 显示当前日志文件路径" << std::endl;
        return;
    }

    // 解析子命令 (大小写不敏感)
    std::istringstream iss(args);
    std::string sub;
    iss >> sub;
    std::transform(sub.begin(), sub.end(), sub.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (sub == "level") {
        std::string level_str;
        if (!(iss >> level_str)) {
            std::cout << "[repl] 用法: log level <LEVEL> (LEVEL=DEBUG/INFO/WARN/ERROR)" << std::endl;
            return;
        }
        LogLevel lvl = Logger::string_to_level(level_str);
        Logger::instance().set_level(lvl);
        std::cout << "[repl] 日志级别设置为: " << Logger::level_to_string(lvl) << std::endl;
        LOG_INFO("repl", "日志级别设置为: " + level_str);
        return;
    }

    if (sub == "path") {
        std::string path = Logger::instance().get_log_file_path();
        if (path.empty()) {
            std::cout << "[repl] 日志文件尚未创建 (输出一条日志后才会创建)" << std::endl;
        } else {
            std::cout << "[repl] 当前日志文件: " << path << std::endl;
        }
        return;
    }

    std::cout << "[repl] 未知子命令: " << sub
              << " (可用: level / path)" << std::endl;
}

void CliRepl::handle_exit() {
    std::cout << "[repl] 再见" << std::endl;
}

// ============================================================================
// 辅助方法
// ============================================================================
void CliRepl::print_help() {
    std::cout << "============================================================" << std::endl;
    std::cout << "Orchestrator 交互式 REPL (骨架版本)" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "可用命令:" << std::endl;
    std::cout << "  load <config.json>     - 加载配置文件" << std::endl;
    std::cout << "  run <frame.fts>        - 单帧处理" << std::endl;
    std::cout << "  run-batch <dir>        - 批量处理目录" << std::endl;
    std::cout << "  status                 - 查询当前状态" << std::endl;
    std::cout << "  pause                  - 暂停" << std::endl;
    std::cout << "  resume                 - 恢复" << std::endl;
    std::cout << "  interrupt              - 中断" << std::endl;
    std::cout << "  checkpoint <frame>     - 保存/查询指定帧的检查点" << std::endl;
    std::cout << "  checkpoint list        - 列出所有检查点" << std::endl;
    std::cout << "  checkpoint clear       - 清除所有检查点" << std::endl;
    std::cout << "  log level <LEVEL>      - 设置日志级别 (DEBUG/INFO/WARN/ERROR)" << std::endl;
    std::cout << "  log path               - 显示当前日志文件路径" << std::endl;
    std::cout << "  help                   - 显示帮助" << std::endl;
    std::cout << "  exit                   - 退出" << std::endl;
    std::cout << "============================================================" << std::endl;
}

void CliRepl::print_prompt() {
    std::cout << "orchestrator> " << std::flush;
}

std::string CliRepl::read_line() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        // EOF (Ctrl+D / Ctrl+Z)
        std::cout << std::endl;
        return "exit";
    }
    // 去掉行首 UTF-8 BOM (EF BB BF, PowerShell 管道输入可能带 BOM)
    if (line.size() >= 3 &&
        static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB &&
        static_cast<unsigned char>(line[2]) == 0xBF) {
        line.erase(0, 3);
    }
    return line;
}
