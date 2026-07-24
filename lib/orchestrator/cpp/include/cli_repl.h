// ============================================================================
// cli_repl.h - 交互式 REPL (Read-Eval-Print Loop)
// 功能: 启动交互式命令行, 支持用户输入命令并调用 Orchestrator 对应方法
// 用途: 提供交互式调试与手动操作入口
//
// 支持命令:
//   load <config.json>     - 加载配置文件
//   run <frame.fts>        - 单帧处理
//   run-batch <dir>        - 批量处理
//   status                 - 查询当前状态
//   pause                  - 暂停
//   resume                 - 恢复
//   interrupt              - 中断
//   checkpoint <frame>     - 查询/保存指定帧的检查点
//   checkpoint list        - 列出所有检查点
//   checkpoint clear       - 清除所有检查点
//   log level <LEVEL>      - 设置日志级别 (DEBUG/INFO/WARN/ERROR)
//   log path               - 显示当前日志文件路径
//   help                   - 显示帮助
//   exit                   - 退出
// ============================================================================

#pragma once

#include <string>
#include "orchestrator.h"

class CliRepl {
public:
    CliRepl();
    ~CliRepl();

    // 启动 REPL 主循环
    void run();

private:
    Orchestrator orchestrator_;

    // 命令处理
    void handle_load(const std::string& args);
    void handle_run(const std::string& args);
    void handle_run_batch(const std::string& args);
    void handle_status();
    void handle_pause();
    void handle_resume();
    void handle_interrupt();
    void handle_checkpoint(const std::string& args);
    void handle_log(const std::string& args);
    void handle_exit();

    // 辅助方法
    void print_help();
    void print_prompt();
    std::string read_line();
};
