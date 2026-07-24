// ============================================================================
// main.cpp - 程序入口
// 功能: 根据命令行参数决定启动模式
//   - 无参数: 启动交互式 REPL
//   - 有参数: 单次命令执行模式
// ============================================================================

#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

#include "cli_repl.h"
#include "cli_command.h"

int main(int argc, char* argv[]) {
    // Windows 下设置控制台为 UTF-8 (保证中文输出正确)
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 无参数: 启动交互式 REPL
    if (argc == 1) {
        CliRepl repl;
        repl.run();
        return 0;
    }

    // 有参数: 单次命令执行
    return CliCommand::execute(argc, argv);
}
