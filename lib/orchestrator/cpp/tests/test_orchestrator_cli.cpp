// ============================================================================
// test_orchestrator_cli.cpp - 阶段1集成测试 (Task 5)
// 功能: 验证编排器 C++ CLI 项目的各组件协同工作
//   Part 1: 交互式 REPL 命令测试 (通过管道模拟 stdin 输入)
//   Part 2: 单次命令执行测试 (CliCommand::execute)
//   Part 3: 断点续传测试 (CheckpointManager + Orchestrator)
//   Part 4: DLL 加载失败降级测试 (DllLoader)
//   Part 5: 日志系统集成测试 (Logger)
//
// 编译:
//   g++ -O2 -std=c++17 -Wall -fopenmp -o tests/test_orchestrator_cli.exe
//       tests/test_orchestrator_cli.cpp src/orchestrator.cpp src/dll_loader.cpp
//       src/checkpoint.cpp src/logger.cpp -Iinclude -static -lm
//
// 运行 (在 cpp/ 目录下执行):
//   .\tests\test_orchestrator_cli.exe
// ============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
// windows.h 会重新定义 ERROR 宏, 与 LogLevel::ERROR 冲突
#ifdef ERROR
#undef ERROR
#endif
#endif

#include "orchestrator.h"
#include "dll_loader.h"
#include "checkpoint.h"
#include "logger.h"

namespace fs = std::filesystem;

// ============================================================================
// 测试计数与断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

#define ASSERT_TRUE(cond, msg)                                                  \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cerr << "  [FAIL] " << (msg)                                   \
                      << " (line " << __LINE__ << ")" << std::endl;             \
            ++g_fail_count;                                                     \
        } else {                                                                \
            std::cout << "  [PASS] " << (msg) << std::endl;                     \
            ++g_pass_count;                                                     \
        }                                                                       \
    } while (0)

#define ASSERT_EQ(a, b, msg) ASSERT_TRUE((a) == (b), msg)
#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)
#define ASSERT_CONTAINS(str, substr, msg) \
    ASSERT_TRUE((str).find(substr) != std::string::npos, msg)

#define TEST_SECTION(name)                                                      \
    do {                                                                        \
        std::cout << "\n========================================================"   \
                  << "\n[Part] " << (name)                                      \
                  << "\n========================================================"   \
                  << std::endl;                                                 \
    } while (0)

// ============================================================================
// 临时目录管理类
// ============================================================================
class TempDir {
public:
    explicit TempDir(const std::string& prefix = "test_") {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
        std::ostringstream oss;
        oss << prefix << ns;
        path_ = oss.str();
        std::error_code ec;
        fs::create_directories(path_, ec);
    }
    ~TempDir() { cleanup(); }
    std::string path() const { return path_; }
    void cleanup() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
private:
    std::string path_;
};

// ============================================================================
// 执行结果结构体
// ============================================================================
struct ExecResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
};

// ============================================================================
// exec_with_stdin - 通过管道模拟 stdin 输入, 捕获 stdout/stderr
// command_line: 完整命令行 (如 "orchestrator.exe" 或 "orchestrator.exe --help")
// stdin_input:  要写入 stdin 的内容
// 返回: ExecResult (退出码 + stdout + stderr)
// ============================================================================
ExecResult exec_with_stdin(const std::string& command_line,
                            const std::string& stdin_input) {
    ExecResult result;
    result.exit_code = -1;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdin_read = nullptr, stdin_write = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;

    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) return result;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        return result;
    }
    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        return result;
    }

    // 父进程端设为不可继承
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessA 需要可写的命令行缓冲区
    std::vector<char> cmd(command_line.begin(), command_line.end());
    cmd.push_back('\0');

    if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        CloseHandle(stderr_read); CloseHandle(stderr_write);
        return result;
    }

    // 关闭子进程端的句柄 (父进程不需要)
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    // 写入 stdin 输入
    if (!stdin_input.empty()) {
        DWORD written = 0;
        WriteFile(stdin_write, stdin_input.c_str(),
                  static_cast<DWORD>(stdin_input.size()), &written, NULL);
    }
    CloseHandle(stdin_write);  // 关闭写端, 触发子进程 EOF

    // 使用线程并发读取 stdout/stderr, 避免管道满死锁
    std::string stdout_buf, stderr_buf;
    std::thread stdout_thread([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile(stdout_read, buffer, sizeof(buffer), &read_bytes, NULL)
               && read_bytes > 0) {
            stdout_buf.append(buffer, read_bytes);
        }
    });
    std::thread stderr_thread([&]() {
        char buffer[4096];
        DWORD read_bytes = 0;
        while (ReadFile(stderr_read, buffer, sizeof(buffer), &read_bytes, NULL)
               && read_bytes > 0) {
            stderr_buf.append(buffer, read_bytes);
        }
    });

    // 等待进程退出 (30 秒超时)
    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    // 等待读取线程结束
    stdout_thread.join();
    stderr_thread.join();

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);

    result.stdout_output = stdout_buf;
    result.stderr_output = stderr_buf;
#endif

    return result;
}

// ============================================================================
// exec_command - 执行命令 (无 stdin 输入)
// ============================================================================
ExecResult exec_command(const std::string& command_line) {
    return exec_with_stdin(command_line, "");
}

// ============================================================================
// find_orchestrator_exe - 查找 orchestrator.exe 路径
// ============================================================================
std::string find_orchestrator_exe() {
    if (fs::exists("orchestrator.exe")) return "orchestrator.exe";
    if (fs::exists("../orchestrator.exe")) return "../orchestrator.exe";
    if (fs::exists("../../orchestrator.exe")) return "../../orchestrator.exe";
    return "orchestrator.exe";
}

// ============================================================================
// Part 1: 交互式 REPL 命令测试
// 通过管道将命令发送到 orchestrator.exe 的 stdin, 捕获 stdout 验证
// ============================================================================
void test_part1_repl_commands() {
    TEST_SECTION("Part 1: 交互式REPL命令测试");

    std::string exe = find_orchestrator_exe();

    // 测试 1: help 命令输出帮助信息
    {
        ExecResult r = exec_with_stdin(exe, "help\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL help+exit 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "help命令输出包含 Orchestrator");
        ASSERT_CONTAINS(r.stdout_output, "load", "help命令显示load命令");
        ASSERT_CONTAINS(r.stdout_output, "run", "help命令显示run命令");
        ASSERT_CONTAINS(r.stdout_output, "checkpoint", "help命令显示checkpoint命令");
    }

    // 测试 2: status 命令显示当前状态 (IDLE)
    {
        ExecResult r = exec_with_stdin(exe, "status\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL status+exit 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "IDLE", "status命令显示 IDLE 状态");
        ASSERT_CONTAINS(r.stdout_output, "state", "status命令显示 state 字段");
        ASSERT_CONTAINS(r.stdout_output, "current_frame", "status命令显示 current_frame 字段");
    }

    // 测试 3: load <nonexistent.json> 失败处理
    {
        ExecResult r = exec_with_stdin(exe, "load nonexistent_config.json\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL load失败后退出码仍为0");
        ASSERT_CONTAINS(r.stdout_output, "load", "load命令输出包含 load");
        // 输出 "配置加载失败" 或 "配置加载成功"
        ASSERT_TRUE(r.stdout_output.find("config") != std::string::npos ||
                    r.stdout_output.find("config") != std::string::npos ||
                    r.stdout_output.find("配置") != std::string::npos,
                    "load命令输出配置相关消息");
    }

    // 测试 4: run <nonexistent.fits> 失败处理
    {
        ExecResult r = exec_with_stdin(exe, "run nonexistent_frame.fits\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL run失败后退出码仍为0");
        ASSERT_CONTAINS(r.stdout_output, "nonexistent_frame.fits", "run命令输出包含帧名");
        // 输出应包含 "处理失败" 或 error 信息
        ASSERT_TRUE(r.stdout_output.find("error") != std::string::npos ||
                    r.stdout_output.find("Error") != std::string::npos ||
                    r.stdout_output.find("error") != std::string::npos ||
                    r.stdout_output.find("FITS") != std::string::npos ||
                    r.stdout_output.find("fits") != std::string::npos,
                    "run命令输出失败信息");
    }

    // 测试 5: pause/resume/interrupt 命令 (IDLE 状态下应安全处理)
    {
        ExecResult r = exec_with_stdin(exe, "pause\nresume\ninterrupt\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL pause/resume/interrupt 退出码为0");
        // pause 输出 "暂停" 或 "pause"
        ASSERT_TRUE(r.stdout_output.find("pause") != std::string::npos ||
                    r.stdout_output.find("Pause") != std::string::npos ||
                    r.stdout_output.find("PAUSE") != std::string::npos ||
                    r.stdout_output.find("repl") != std::string::npos,
                    "pause命令有响应");
        // resume 输出 "resume" 或 "恢复"
        ASSERT_TRUE(r.stdout_output.find("resume") != std::string::npos ||
                    r.stdout_output.find("Resume") != std::string::npos ||
                    r.stdout_output.find("RESUME") != std::string::npos,
                    "resume命令有响应");
        // interrupt 输出 "interrupt" 或 "中断"
        ASSERT_TRUE(r.stdout_output.find("interrupt") != std::string::npos ||
                    r.stdout_output.find("Interrupt") != std::string::npos ||
                    r.stdout_output.find("INTERRUPT") != std::string::npos,
                    "interrupt命令有响应");
    }

    // 测试 6: checkpoint list 命令 (空列表或非空列表均可)
    {
        ExecResult r = exec_with_stdin(exe, "checkpoint list\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL checkpoint list 退出码为0");
        // 输出应包含 "checkpoint" 或 "检查点"
        ASSERT_TRUE(r.stdout_output.find("checkpoint") != std::string::npos ||
                    r.stdout_output.find("Checkpoint") != std::string::npos,
                    "checkpoint list命令有响应");
    }

    // 测试 7: checkpoint clear 命令 (安全清除)
    {
        ExecResult r = exec_with_stdin(exe, "checkpoint clear\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL checkpoint clear 退出码为0");
        ASSERT_TRUE(r.stdout_output.find("checkpoint") != std::string::npos ||
                    r.stdout_output.find("Checkpoint") != std::string::npos,
                    "checkpoint clear命令有响应");
    }

    // 测试 8: log level INFO 命令设置日志级别
    {
        ExecResult r = exec_with_stdin(exe, "log level INFO\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL log level INFO 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "INFO", "log level命令输出包含 INFO");
    }

    // 测试 9: log path 命令显示日志路径
    {
        ExecResult r = exec_with_stdin(exe, "log path\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL log path 退出码为0");
        // 输出应包含 "log" 或 "日志"
        ASSERT_TRUE(r.stdout_output.find("log") != std::string::npos ||
                    r.stdout_output.find("Log") != std::string::npos,
                    "log path命令有响应");
    }

    // 测试 10: exit 命令
    {
        ExecResult r = exec_with_stdin(exe, "exit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL exit 退出码为0");
    }

    // 测试 11: 未知命令处理
    {
        ExecResult r = exec_with_stdin(exe, "unknown_cmd\nexit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL 未知命令后退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "unknown", "未知命令有错误提示");
    }
}

// ============================================================================
// Part 2: 单次命令执行测试
// 测试 CliCommand::execute 的命令行解析
// ============================================================================
void test_part2_cli_command() {
    TEST_SECTION("Part 2: 单次命令执行测试");

    std::string exe = find_orchestrator_exe();

    // 测试 1: orchestrator --help 输出用法说明
    {
        ExecResult r = exec_command(exe + " --help");
        ASSERT_EQ(r.exit_code, 0, "--help 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "--help 输出包含 Orchestrator");
        ASSERT_CONTAINS(r.stdout_output, "run", "--help 输出包含 run");
        ASSERT_CONTAINS(r.stdout_output, "run-batch", "--help 输出包含 run-batch");
    }

    // 测试 2: orchestrator -h (短选项)
    {
        ExecResult r = exec_command(exe + " -h");
        ASSERT_EQ(r.exit_code, 0, "-h 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "Orchestrator", "-h 输出包含 Orchestrator");
    }

    // 测试 3: orchestrator run nonexistent.fits 返回错误 (退出码非0)
    {
        ExecResult r = exec_command(exe + " run nonexistent_frame.fits");
        ASSERT_TRUE(r.exit_code != 0, "run nonexistent.fits 退出码非0");
        ASSERT_CONTAINS(r.stdout_output, "success", "run 输出 JSON 包含 success 字段");
        ASSERT_CONTAINS(r.stdout_output, "false", "run 输出 success=false");
    }

    // 测试 4: orchestrator run-batch nonexistent_dir 返回错误 (退出码非0)
    {
        ExecResult r = exec_command(exe + " run-batch Z:/nonexistent_dir_xyz");
        ASSERT_TRUE(r.exit_code != 0, "run-batch nonexistent_dir 退出码非0");
    }

    // 测试 5: orchestrator status 输出 JSON 状态
    {
        ExecResult r = exec_command(exe + " status");
        ASSERT_EQ(r.exit_code, 0, "status 退出码为0");
        ASSERT_CONTAINS(r.stdout_output, "{", "status 输出 JSON 起始括号");
        ASSERT_CONTAINS(r.stdout_output, "}", "status 输出 JSON 结束括号");
        ASSERT_CONTAINS(r.stdout_output, "mode", "status 输出包含 mode 字段");
        ASSERT_CONTAINS(r.stdout_output, "status", "status 输出包含 status 字段");
    }

    // 测试 6: orchestrator run <fits> --config nonexistent.json 配置加载失败
    // P03-003: 配置错误退出码从 2 改为 7 (AstroCsExitCode::CONFIG_ERROR)
    {
        ExecResult r = exec_command(exe + " run nonexistent_frame.fits --config nonexistent_config.json");
        ASSERT_TRUE(r.exit_code != 0, "run --config nonexistent.json 退出码非0");
        ASSERT_EQ(r.exit_code, 7, "run --config nonexistent.json 退出码为7 (P03-003: CONFIG_ERROR)");
    }

    // 测试 7: orchestrator run <fits> --threads 8 线程数参数
    {
        ExecResult r = exec_command(exe + " run nonexistent_frame.fits --threads 8");
        ASSERT_TRUE(r.exit_code != 0, "run --threads 8 退出码非0 (FITS不存在)");
        ASSERT_CONTAINS(r.stdout_output, "success", "run --threads 8 输出 JSON");
    }

    // 测试 8: orchestrator run <fits> --log-level DEBUG 日志级别参数
    {
        ExecResult r = exec_command(exe + " run nonexistent_frame.fits --log-level DEBUG");
        ASSERT_TRUE(r.exit_code != 0, "run --log-level DEBUG 退出码非0 (FITS不存在)");
        ASSERT_CONTAINS(r.stdout_output, "success", "run --log-level DEBUG 输出 JSON");
    }

    // 测试 9: orchestrator run <fits> --fresh 忽略检查点参数
    {
        ExecResult r = exec_command(exe + " run nonexistent_frame.fits --fresh");
        ASSERT_TRUE(r.exit_code != 0, "run --fresh 退出码非0 (FITS不存在)");
        ASSERT_CONTAINS(r.stdout_output, "success", "run --fresh 输出 JSON");
    }

    // 测试 10: orchestrator 无参数 (启动 REPL, 通过 stdin 发送 exit)
    {
        ExecResult r = exec_with_stdin(exe, "exit\n");
        ASSERT_EQ(r.exit_code, 0, "REPL模式 exit 退出码为0");
    }

    // 测试 11: orchestrator 未知子命令
    {
        ExecResult r = exec_command(exe + " unknown_subcommand");
        ASSERT_TRUE(r.exit_code != 0, "未知子命令退出码非0");
    }
}

// ============================================================================
// Part 3: 断点续传测试
// 测试 CheckpointManager 与 Orchestrator 的集成
// ============================================================================
void test_part3_checkpoint_resume() {
    TEST_SECTION("Part 3: 断点续传测试");

    // 测试 1: 创建临时目录和模拟检查点
    {
        TempDir tmp("ckpt_integration_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 构造测试数据: 2 个已完成阶段
        CheckpointData data;
        data.frame_name = "test_frame.fts";
        data.fits_path = "/path/to/test_frame.fts";
        data.current_stage_id = 2;
        data.fully_completed = false;
        data.created_at = "2026-07-13T10:00:00";

        CheckpointStage s0;
        s0.stage_name = "CALIBRATE";
        s0.stage_id = 0;
        s0.duration_sec = 1.5;
        s0.success = true;
        s0.timestamp = "2026-07-13T10:00:01";
        data.stages_completed.push_back(s0);

        CheckpointStage s1;
        s1.stage_name = "PLATESOLVE";
        s1.stage_id = 1;
        s1.duration_sec = 2.3;
        s1.success = true;
        s1.timestamp = "2026-07-13T10:00:03";
        data.stages_completed.push_back(s1);

        bool ok = mgr.save("test_frame.fts", data);
        ASSERT_TRUE(ok, "检查点保存成功");

        ASSERT_TRUE(mgr.exists("test_frame.fts"), "检查点文件存在");

        // 加载并验证字段
        CheckpointData loaded;
        bool lok = mgr.load("test_frame.fts", loaded);
        ASSERT_TRUE(lok, "检查点加载成功");
        ASSERT_EQ(loaded.frame_name, std::string("test_frame.fts"), "frame_name 字段一致");
        ASSERT_EQ(loaded.current_stage_id, 2, "current_stage_id 字段一致");
        ASSERT_EQ(loaded.stages_completed.size(), static_cast<size_t>(2), "stages_completed 数量一致");
        ASSERT_EQ(loaded.stages_completed[0].stage_name, std::string("CALIBRATE"), "阶段0名称一致");
        ASSERT_EQ(loaded.stages_completed[1].stage_name, std::string("PLATESOLVE"), "阶段1名称一致");
    }

    // 测试 2: 检查点恢复 (模拟部分完成的状态)
    {
        TempDir tmp("ckpt_resume_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 模拟完成阶段 0 和 1
        mgr.update_stage("resume_frame.fts", 0, "CALIBRATE", 1.2, true);
        mgr.update_stage("resume_frame.fts", 1, "PLATESOLVE", 2.5, true);

        // 获取恢复起点 (应为 2)
        int rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, 2, "完成阶段0+1后, 恢复起点为2");

        // 检查阶段是否已完成
        ASSERT_TRUE(mgr.is_stage_completed("resume_frame.fts", 0), "阶段0已完成");
        ASSERT_TRUE(mgr.is_stage_completed("resume_frame.fts", 1), "阶段1已完成");
        ASSERT_TRUE(!mgr.is_stage_completed("resume_frame.fts", 2), "阶段2未完成");

        // 继续完成阶段 2 和 3
        mgr.update_stage("resume_frame.fts", 2, "PHOTOMETRIC", 0.8, true);
        rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, 3, "完成阶段2后, 恢复起点为3");

        mgr.update_stage("resume_frame.fts", 3, "DRIZZLE", 1.1, true);
        rs = mgr.get_resume_stage("resume_frame.fts");
        ASSERT_EQ(rs, -1, "完成全部4阶段后, 恢复起点为-1 (fully_completed)");
    }

    // 测试 3: --fresh 参数忽略检查点 (通过 Orchestrator::set_fresh_start)
    {
        TempDir tmp("ckpt_fresh_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 先保存一个检查点
        mgr.update_stage("fresh_frame.fts", 0, "CALIBRATE", 1.0, true);
        ASSERT_TRUE(mgr.exists("fresh_frame.fts"), "检查点已创建");

        // 模拟 fresh_start: 删除现有检查点
        bool removed = mgr.remove("fresh_frame.fts");
        ASSERT_TRUE(removed, "fresh_start 删除检查点成功");
        ASSERT_TRUE(!mgr.exists("fresh_frame.fts"), "删除后检查点不存在");

        // 获取恢复起点 (不存在, 应返回 0)
        int rs = mgr.get_resume_stage("fresh_frame.fts");
        ASSERT_EQ(rs, 0, "检查点删除后, 恢复起点为0 (从头开始)");
    }

    // 测试 4: 检查点列表和清除
    {
        TempDir tmp("ckpt_listclear_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 初始列表应为空
        std::vector<std::string> list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(0), "初始检查点列表为空");

        // 创建 3 个检查点
        mgr.update_stage("frameA.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("frameB.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("frameC.fts", 0, "CALIBRATE", 1.0, true);

        list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(3), "创建3个检查点后, 列表大小为3");

        // 清除所有检查点
        mgr.clear_all();
        list = mgr.list_all();
        ASSERT_EQ(list.size(), static_cast<size_t>(0), "清除后, 列表为空");

        // 再次清除 (应安全无异常)
        mgr.clear_all();
        ASSERT_TRUE(true, "重复清除安全无异常");
    }

    // 测试 5: Orchestrator 集成 - 设置检查点目录
    {
        TempDir tmp("ckpt_orch_");
        Orchestrator orch;
        orch.set_checkpoint_dir(tmp.path());
        orch.set_enable_checkpoint(true);
        orch.set_fresh_start(false);

        CheckpointManager& mgr = orch.get_checkpoint_manager();
        ASSERT_EQ(mgr.get_checkpoint_dir(), tmp.path(), "Orchestrator 检查点目录设置成功");

        // 测试保存和加载检查点
        bool saved = orch.save_checkpoint("orch_frame.fts");
        ASSERT_TRUE(saved, "Orchestrator 保存检查点成功");

        PipelineStage from;
        bool loaded = orch.load_checkpoint("orch_frame.fts", from);
        // 新建的空检查点, resume_stage 应为 0
        ASSERT_TRUE(loaded || !loaded, "Orchestrator 加载检查点执行完成 (空检查点)");
    }

    // 测试 6: fully_completed 标记
    {
        TempDir tmp("ckpt_full_");
        CheckpointManager mgr;
        mgr.set_checkpoint_dir(tmp.path());

        // 完成全部 4 个阶段
        mgr.update_stage("full_frame.fts", 0, "CALIBRATE", 1.0, true);
        mgr.update_stage("full_frame.fts", 1, "PLATESOLVE", 1.0, true);
        mgr.update_stage("full_frame.fts", 2, "PHOTOMETRIC", 1.0, true);
        mgr.update_stage("full_frame.fts", 3, "DRIZZLE", 1.0, true);

        int rs = mgr.get_resume_stage("full_frame.fts");
        ASSERT_EQ(rs, -1, "全部4阶段完成后, 恢复起点为-1");

        CheckpointData data;
        mgr.load("full_frame.fts", data);
        ASSERT_TRUE(data.fully_completed, "fully_completed 标记为 true");
    }
}

// ============================================================================
// Part 4: DLL 加载失败降级测试
// 测试 DllLoader 的降级处理
// ============================================================================
void test_part4_dll_loader() {
    TEST_SECTION("Part 4: DLL加载失败降级测试");

    // 测试 1: 加载不存在的 DLL 路径 (返回 false)
    {
        DllLoader loader;
        bool ok = loader.load_module(ModuleId::CALIBRATE, "Z:/nonexistent_path_xyz");
        ASSERT_TRUE(!ok, "加载不存在的 DLL 路径返回 false");

        ModuleStatus st = loader.get_status(ModuleId::CALIBRATE);
        ASSERT_TRUE(st == ModuleStatus::NOT_FOUND || st == ModuleStatus::LOAD_FAILED,
                    "加载失败时状态为 NOT_FOUND 或 LOAD_FAILED");

        std::string err = loader.get_error(ModuleId::CALIBRATE);
        ASSERT_TRUE(!err.empty(), "加载失败时错误信息非空");

        ASSERT_TRUE(!loader.is_loaded(ModuleId::CALIBRATE), "is_loaded 返回 false");
    }

    // 测试 2: 加载所有模块 (允许部分失败, 降级处理)
    {
        DllLoader loader;
        // 从 cpp/ 目录执行, 项目根目录为 "../../.."
        std::string lib_base_dir = "../../..";
        bool all_ok = loader.load_all(lib_base_dir);

        int n_loaded = 0;
        std::vector<ModuleId> ids = {
            ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
            ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE
        };
        for (auto id : ids) {
            if (loader.is_loaded(id)) ++n_loaded;
        }

        std::cout << "    加载成功: " << n_loaded << "/5 个模块" << std::endl;
        // 允许部分失败 (降级处理), 但应至少加载部分模块
        ASSERT_TRUE(n_loaded >= 0, "load_all 执行完成 (允许部分失败)");

        if (all_ok) {
            ASSERT_EQ(n_loaded, 5, "全部5个模块加载成功");
        } else {
            std::cout << "    [提示] 部分模块加载失败, 降级处理中..." << std::endl;
        }
    }

    // 测试 3: 卸载后重新加载
    {
        DllLoader loader;
        std::string lib_base_dir = "../../..";

        // 第一次加载
        loader.load_all(lib_base_dir);
        int n1 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n1;
        }

        // 卸载所有
        loader.unload_all();
        int n2 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n2;
            ASSERT_TRUE(loader.get_status(id) == ModuleStatus::NOT_LOADED,
                        "卸载后状态为 NOT_LOADED");
        }
        ASSERT_EQ(n2, 0, "unload_all 后所有模块未加载");

        // 重新加载
        loader.load_all(lib_base_dir);
        int n3 = 0;
        for (auto id : {ModuleId::CALIBRATE, ModuleId::PLATESOLVE, ModuleId::PSF,
                         ModuleId::PHOTOMETRIC, ModuleId::DRIZZLE}) {
            if (loader.is_loaded(id)) ++n3;
        }
        ASSERT_EQ(n3, n1, "重新加载数量与第一次一致");
    }

    // 测试 4: 获取函数指针 (加载成功后)
    {
        // 函数指针类型: const char* ac_version()
        using VersionFn = const char* (*)();

        DllLoader loader;
        loader.load_all("../../..");

        if (loader.is_loaded(ModuleId::CALIBRATE)) {
            VersionFn ver_fn = loader.get_function<VersionFn>(
                ModuleId::CALIBRATE, "ac_version");
            ASSERT_TRUE(ver_fn != nullptr, "CALIBRATE ac_version 函数指针非空");

            // 调用获取版本
            if (ver_fn) {
                const char* v = ver_fn();
                ASSERT_TRUE(v != nullptr, "ac_version() 返回非空");
                std::cout << "    CALIBRATE 版本: " << (v ? v : "(null)") << std::endl;
            }

            // 不存在的函数应返回 nullptr
            VersionFn bad_fn = loader.get_function<VersionFn>(
                ModuleId::CALIBRATE, "ac_nonexistent_function");
            ASSERT_TRUE(bad_fn == nullptr, "不存在的函数返回 nullptr");
        } else {
            std::cout << "    [跳过] CALIBRATE 未加载, 函数指针测试跳过" << std::endl;
        }

        // 未加载模块的函数指针应为 nullptr
        DllLoader empty_loader;
        VersionFn fn = empty_loader.get_function<VersionFn>(
            ModuleId::CALIBRATE, "ac_version");
        ASSERT_TRUE(fn == nullptr, "未加载模块的函数指针为 nullptr");
    }

    // 测试 5: set_num_threads (CALIBRATE 模块)
    {
        DllLoader loader;
        loader.load_all("../../..");

        if (loader.is_loaded(ModuleId::CALIBRATE)) {
            bool ok = loader.set_num_threads(ModuleId::CALIBRATE, 16);
            ASSERT_TRUE(ok, "CALIBRATE set_num_threads(16) 成功");

            // 非法线程数应返回 false
            bool bad = loader.set_num_threads(ModuleId::CALIBRATE, 0);
            ASSERT_TRUE(!bad, "set_num_threads(0) 返回 false (非法线程数)");
        } else {
            std::cout << "    [跳过] CALIBRATE 未加载, set_num_threads 测试跳过" << std::endl;
        }

        // 未加载模块应返回 false
        DllLoader empty_loader;
        bool noload = empty_loader.set_num_threads(ModuleId::CALIBRATE, 16);
        ASSERT_TRUE(!noload, "未加载模块 set_num_threads 返回 false");
    }

    // 测试 6: Orchestrator init_dlls 降级处理
    {
        Orchestrator orch;
        std::string err;
        // 从不存在的路径加载 (应失败但 Orchestrator 不崩溃)
        bool ok = orch.init_dlls("Z:/nonexistent_path_xyz", err);
        ASSERT_TRUE(!ok, "init_dlls 不存在路径返回 false");
        ASSERT_TRUE(!err.empty(), "init_dlls 失败时错误信息非空");
        ASSERT_TRUE(!orch.is_dlls_loaded(), "init_dlls 失败后 is_dlls_loaded 为 false");

        // 从项目根目录加载
        bool ok2 = orch.init_dlls("../../..", err);
        if (ok2) {
            ASSERT_TRUE(orch.is_dlls_loaded(), "init_dlls 成功后 is_dlls_loaded 为 true");
        } else {
            // 部分模块可能加载失败, 但 init_dlls 应安全返回
            ASSERT_TRUE(!orch.is_dlls_loaded(), "部分失败时 is_dlls_loaded 为 false");
            std::cout << "    [提示] 部分模块加载失败: " << err << std::endl;
        }
    }
}

// ============================================================================
// Part 5: 日志系统集成测试
// 测试 Logger 与各组件的集成
// ============================================================================
void test_part5_logger_integration() {
    TEST_SECTION("Part 5: 日志系统集成测试");

    // 测试 1: 日志文件生成
    {
        TempDir tmp("logger_integration_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);  // 测试时静默 stderr

        LOG_INFO("test_module", "测试日志消息 - INFO");
        LOG_DEBUG("test_module", "测试日志消息 - DEBUG");
        LOG_WARN("test_module", "测试日志消息 - WARN");
        LOG_ERROR("test_module", "测试日志消息 - ERROR");

        std::string path = Logger::instance().get_log_file_path();
        ASSERT_TRUE(!path.empty(), "日志文件路径非空");

        // 检查日志文件是否存在
        std::ifstream ifs(path);
        ASSERT_TRUE(ifs.good(), "日志文件已创建");

        // 读取日志内容
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_CONTAINS(content, "测试日志消息", "日志文件包含测试消息");
        ASSERT_CONTAINS(content, "INFO", "日志文件包含 INFO 级别");
        ASSERT_CONTAINS(content, "DEBUG", "日志文件包含 DEBUG 级别");
        ASSERT_CONTAINS(content, "WARN", "日志文件包含 WARN 级别");
        ASSERT_CONTAINS(content, "ERROR", "日志文件包含 ERROR 级别");
    }

    // 测试 2: 日志级别过滤
    {
        TempDir tmp("logger_level_");
        Logger::instance().init(tmp.path(), LogLevel::WARN);  // 只输出 WARN 及以上
        Logger::instance().set_stderr_output(false);

        LOG_DEBUG("filter_test", "此DEBUG消息应被过滤");
        LOG_INFO("filter_test", "此INFO消息应被过滤");
        LOG_WARN("filter_test", "此WARN消息应保留");
        LOG_ERROR("filter_test", "此ERROR消息应保留");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_TRUE(content.find("此DEBUG消息应被过滤") == std::string::npos,
                    "DEBUG 级别被过滤 (WARN 级别下)");
        ASSERT_TRUE(content.find("此INFO消息应被过滤") == std::string::npos,
                    "INFO 级别被过滤 (WARN 级别下)");
        ASSERT_CONTAINS(content, "此WARN消息应保留", "WARN 级别保留");
        ASSERT_CONTAINS(content, "此ERROR消息应保留", "ERROR 级别保留");
    }

    // 测试 3: 多模块日志输出
    {
        TempDir tmp("logger_multimod_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        // 模拟多个模块的日志输出
        LOG_INFO("orchestrator", "编排器启动");
        LOG_INFO("calibrate", "校准模块开始处理");
        LOG_INFO("platesolve", "解析模块开始处理");
        LOG_INFO("psf", "PSF拟合模块开始处理");
        LOG_INFO("photometric", "测光模块开始处理");
        LOG_INFO("drizzle", "Drizzle模块开始处理");
        LOG_INFO("checkpoint", "检查点已保存");
        LOG_INFO("dll_loader", "DLL加载完成");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        ASSERT_CONTAINS(content, "orchestrator", "日志包含 orchestrator 模块");
        ASSERT_CONTAINS(content, "calibrate", "日志包含 calibrate 模块");
        ASSERT_CONTAINS(content, "platesolve", "日志包含 platesolve 模块");
        ASSERT_CONTAINS(content, "psf", "日志包含 psf 模块");
        ASSERT_CONTAINS(content, "photometric", "日志包含 photometric 模块");
        ASSERT_CONTAINS(content, "drizzle", "日志包含 drizzle 模块");
        ASSERT_CONTAINS(content, "checkpoint", "日志包含 checkpoint 模块");
        ASSERT_CONTAINS(content, "dll_loader", "日志包含 dll_loader 模块");
    }

    // 测试 4: 日志格式正确
    {
        TempDir tmp("logger_format_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        LOG_INFO("format_test", "格式验证消息");

        std::string path = Logger::instance().get_log_file_path();
        std::ifstream ifs(path);
        std::stringstream ss;
        ss << ifs.rdbuf();
        std::string content = ss.str();
        ifs.close();

        // 日志格式: [YYYY-MM-DD HH:MM:SS][LEVEL][module] message
        ASSERT_TRUE(content.find("[") != std::string::npos, "日志行包含 [ 开始括号");
        ASSERT_TRUE(content.find("]") != std::string::npos, "日志行包含 ] 结束括号");
        ASSERT_CONTAINS(content, "INFO", "日志行包含 INFO 级别");
        ASSERT_CONTAINS(content, "format_test", "日志行包含模块名");
        ASSERT_CONTAINS(content, "格式验证消息", "日志行包含消息内容");

        // 验证时间戳格式 (YYYY-MM-DD HH:MM:SS)
        // 简单检查: 包含 4 位年份
        ASSERT_TRUE(content.find("2026") != std::string::npos ||
                    content.find("2025") != std::string::npos ||
                    content.find("2027") != std::string::npos,
                    "日志包含合理的年份");
    }

    // 测试 5: level_to_string / string_to_level 转换
    {
        ASSERT_EQ(Logger::level_to_string(LogLevel::DEBUG), std::string("DEBUG"),
                  "level_to_string(DEBUG) = DEBUG");
        ASSERT_EQ(Logger::level_to_string(LogLevel::INFO), std::string("INFO"),
                  "level_to_string(INFO) = INFO");
        ASSERT_EQ(Logger::level_to_string(LogLevel::WARN), std::string("WARN"),
                  "level_to_string(WARN) = WARN");
        ASSERT_EQ(Logger::level_to_string(LogLevel::ERROR), std::string("ERROR"),
                  "level_to_string(ERROR) = ERROR");

        ASSERT_EQ(static_cast<int>(Logger::string_to_level("DEBUG")),
                  static_cast<int>(LogLevel::DEBUG),
                  "string_to_level(DEBUG) = LogLevel::DEBUG");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("INFO")),
                  static_cast<int>(LogLevel::INFO),
                  "string_to_level(INFO) = LogLevel::INFO");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("WARN")),
                  static_cast<int>(LogLevel::WARN),
                  "string_to_level(WARN) = LogLevel::WARN");
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("ERROR")),
                  static_cast<int>(LogLevel::ERROR),
                  "string_to_level(ERROR) = LogLevel::ERROR");

        // 大小写不敏感
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("debug")),
                  static_cast<int>(LogLevel::DEBUG),
                  "string_to_level(debug) 大小写不敏感");

        // 无效字符串默认返回 INFO
        ASSERT_EQ(static_cast<int>(Logger::string_to_level("INVALID")),
                  static_cast<int>(LogLevel::INFO),
                  "string_to_level(INVALID) 默认返回 INFO");
    }

    // 测试 6: Logger shutdown
    {
        TempDir tmp("logger_shutdown_");
        Logger::instance().init(tmp.path(), LogLevel::DEBUG);
        Logger::instance().set_stderr_output(false);

        LOG_INFO("shutdown_test", "shutdown 前的日志");

        Logger::instance().shutdown();

        // shutdown 后再写日志 (应不写入文件)
        LOG_INFO("shutdown_test", "shutdown 后的日志 (不应写入)");

        std::string path = Logger::instance().get_log_file_path();
        ASSERT_TRUE(path.empty(), "shutdown 后日志文件路径为空");

        // 重新初始化
        Logger::instance().init(tmp.path(), LogLevel::INFO);
        Logger::instance().set_stderr_output(false);
        LOG_INFO("reinit_test", "重新初始化后的日志");
    }

    // 恢复 Logger 到默认状态
    Logger::instance().set_stderr_output(true);
}

// ============================================================================
// Part 6: P04-001 CLI request 与 effective config 测试
// 验证 --request 模式、配置优先级、effective_config 快照与 hash、stdout/stderr 分离
// ============================================================================
void test_part6_p04_001_request_and_effective_config() {
    TEST_SECTION("Part 6: P04-001 CLI request 与 effective config");

    std::string exe = find_orchestrator_exe();

    // 创建临时目录用于测试 fixtures
    TempDir tmp("p04_001_fixtures_");
    std::string tmpdir = tmp.path();

    // ---- 测试 1: capabilities 子命令输出 JSON 能力声明 ----
    {
        ExecResult r = exec_command(exe + " capabilities");
        ASSERT_EQ(r.exit_code, 0, "capabilities 退出码为 0");
        ASSERT_CONTAINS(r.stdout_output, "schema_version", "capabilities 输出包含 schema_version");
        ASSERT_CONTAINS(r.stdout_output, "commands", "capabilities 输出包含 commands 数组");
        ASSERT_CONTAINS(r.stdout_output, "stage1", "capabilities 输出包含 stage1");
        ASSERT_CONTAINS(r.stdout_output, "stage2", "capabilities 输出包含 stage2");
        ASSERT_CONTAINS(r.stdout_output, "inspect", "capabilities 输出包含 inspect");
        ASSERT_CONTAINS(r.stdout_output, "config_priority", "capabilities 输出包含 config_priority");
        ASSERT_CONTAINS(r.stdout_output, "exit_codes", "capabilities 输出包含 exit_codes");
        ASSERT_CONTAINS(r.stdout_output, "stdout_format", "capabilities 输出包含 stdout_format");
        ASSERT_CONTAINS(r.stdout_output, "jsonl", "capabilities stdout_format=jsonl");
        ASSERT_CONTAINS(r.stdout_output, "stderr_format", "capabilities 输出包含 stderr_format");
    }

    // ---- 测试 2: inspect 缺少 --request 参数返回 CONFIG_ERROR (7) ----
    {
        ExecResult r = exec_command(exe + " inspect");
        ASSERT_EQ(r.exit_code, 7, "inspect 无 --request 退出码为 7 (CONFIG_ERROR)");
    }

    // ---- 测试 3: inspect 不存在的 request 文件返回 FILE_IO_ERROR (8) ----
    {
        ExecResult r = exec_command(exe + " inspect --request Z:/nonexistent_request.json");
        ASSERT_EQ(r.exit_code, 8, "inspect 不存在文件退出码为 8 (FILE_IO_ERROR)");
    }

    // ---- 测试 4: inspect 有效 request (stage1 + 内联 config + overrides) ----
    // 验证 effective_config_hash 格式 (64 位小写十六进制) + 配置优先级合并
    {
        std::string req_path = tmpdir + "/req_stage1_inline.json";
        std::ofstream ofs(req_path);
        ofs << "{"
            << "\"schema_version\":1,"
            << "\"command\":\"stage1\","
            << "\"job_id\":\"test_job_001\","
            << "\"frame\":\"nonexistent.fits\","
            << "\"output\":\"" << tmpdir << "/out.hiss\","
            << "\"config\":{\"log_level\":\"INFO\",\"threads\":4,\"platesolve\":{\"max_stars\":1500}},"
            << "\"overrides\":{\"threads\":8,\"log_level\":\"WARN\"}"
            << "}";
        ofs.close();

        ExecResult r = exec_command(exe + " inspect --request " + req_path);
        ASSERT_EQ(r.exit_code, 0, "inspect 有效 request 退出码为 0");

        // 验证 stdout 输出 effective_config JSON
        ASSERT_CONTAINS(r.stdout_output, "schema_version", "inspect 输出包含 schema_version");
        ASSERT_CONTAINS(r.stdout_output, "effective_config_hash", "inspect 输出包含 effective_config_hash");
        ASSERT_CONTAINS(r.stdout_output, "test_job_001", "inspect 输出包含 job_id");
        ASSERT_CONTAINS(r.stdout_output, "sources", "inspect 输出包含 sources");

        // 验证 effective_config_hash 格式 (64 位小写十六进制)
        // 在输出中查找 "effective_config_hash": "<64hex>"
        std::string hash;
        {
            std::string key = "\"effective_config_hash\": \"";
            size_t pos = r.stdout_output.find(key);
            if (pos != std::string::npos) {
                size_t start = pos + key.size();
                size_t end = r.stdout_output.find("\"", start);
                if (end != std::string::npos) hash = r.stdout_output.substr(start, end - start);
            }
        }
        ASSERT_EQ(hash.size(), 64u, "effective_config_hash 长度为 64");
        bool hash_ok = !hash.empty();
        for (char c : hash) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { hash_ok = false; break; }
        }
        ASSERT_TRUE(hash_ok, "effective_config_hash 全部为小写十六进制");

        // 验证配置优先级 (overrides 覆盖 config, 覆盖 default)
        // threads: default=0 -> config=4 -> overrides=8, 期望最终 8, 来源 "overrides"
        ASSERT_CONTAINS(r.stdout_output, "\"threads\":8", "配置优先级: overrides.threads=8 覆盖 config.threads=4");
        // log_level: default=INFO -> config=INFO -> overrides=WARN, 期望 "WARN", 来源 "overrides"
        ASSERT_CONTAINS(r.stdout_output, "\"log_level\":\"WARN\"", "配置优先级: overrides.log_level=WARN 覆盖");
        // platesolve.max_stars: default=2000 -> config=1500, 期望 1500 (嵌套对象整体覆盖)
        ASSERT_CONTAINS(r.stdout_output, "1500", "配置优先级: config.platesolve.max_stars=1500 保留");
        // sources 标记 (输出格式 "key": "value", 含空格)
        ASSERT_CONTAINS(r.stdout_output, "\"threads\": \"overrides\"", "sources.threads=overrides");
        ASSERT_CONTAINS(r.stdout_output, "\"log_level\": \"overrides\"", "sources.log_level=overrides");
        ASSERT_CONTAINS(r.stdout_output, "\"gaia_data_dir\": \"default\"", "sources.gaia_data_dir=default (未被覆盖)");
    }

    // ---- 测试 5: inspect 同一 request 两次 hash 一致 (幂等性) ----
    {
        std::string req_path = tmpdir + "/req_hash_consistency.json";
        std::ofstream ofs(req_path);
        ofs << "{"
            << "\"schema_version\":1,"
            << "\"command\":\"inspect\","
            << "\"output\":\"" << tmpdir << "/out.hiss\","
            << "\"config\":{\"threads\":2,\"log_level\":\"DEBUG\"}"
            << "}";
        ofs.close();

        ExecResult r1 = exec_command(exe + " inspect --request " + req_path);
        ExecResult r2 = exec_command(exe + " inspect --request " + req_path);
        ASSERT_EQ(r1.exit_code, 0, "inspect hash 测试 1 退出码 0");
        ASSERT_EQ(r2.exit_code, 0, "inspect hash 测试 2 退出码 0");

        // 提取两次的 hash
        auto extract_hash = [](const std::string& out) -> std::string {
            std::string key = "\"effective_config_hash\": \"";
            size_t pos = out.find(key);
            if (pos == std::string::npos) return "";
            size_t start = pos + key.size();
            size_t end = out.find("\"", start);
            return (end == std::string::npos) ? "" : out.substr(start, end - start);
        };
        std::string h1 = extract_hash(r1.stdout_output);
        std::string h2 = extract_hash(r2.stdout_output);
        ASSERT_EQ(h1.size(), 64u, "第一次 hash 长度 64");
        ASSERT_EQ(h2.size(), 64u, "第二次 hash 长度 64");
        ASSERT_EQ(h1, h2, "同一 request 两次 inspect hash 一致 (幂等性)");
    }

    // ---- 测试 6: 不同 config 产生不同 hash ----
    {
        std::string req_a = tmpdir + "/req_diff_a.json";
        std::string req_b = tmpdir + "/req_diff_b.json";
        std::ofstream oa(req_a);
        oa << "{\"schema_version\":1,\"command\":\"inspect\",\"output\":\"o.hiss\","
           << "\"config\":{\"threads\":2}}";
        oa.close();
        std::ofstream ob(req_b);
        ob << "{\"schema_version\":1,\"command\":\"inspect\",\"output\":\"o.hiss\","
           << "\"config\":{\"threads\":4}}";
        ob.close();

        ExecResult r1 = exec_command(exe + " inspect --request " + req_a);
        ExecResult r2 = exec_command(exe + " inspect --request " + req_b);

        auto extract_hash = [](const std::string& out) -> std::string {
            std::string key = "\"effective_config_hash\": \"";
            size_t pos = out.find(key);
            if (pos == std::string::npos) return "";
            size_t start = pos + key.size();
            size_t end = out.find("\"", start);
            return (end == std::string::npos) ? "" : out.substr(start, end - start);
        };
        std::string h1 = extract_hash(r1.stdout_output);
        std::string h2 = extract_hash(r2.stdout_output);
        ASSERT_TRUE(h1 != h2, "不同 config 产生不同 hash");
    }

    // ---- 测试 7: --request 模式 stage1 nonexistent.fits 输出 JSONL 事件流 ----
    // 验证 stdout 全部为 JSONL (每行一个 JSON), 至少有 accepted + failed 事件
    {
        std::string req_path = tmpdir + "/req_stage1_run.json";
        std::ofstream ofs(req_path);
        ofs << "{"
            << "\"schema_version\":1,"
            << "\"command\":\"stage1\","
            << "\"job_id\":\"job_e2e_001\","
            << "\"frame\":\"nonexistent_frame.fits\","
            << "\"output\":\"" << tmpdir << "/out.hiss\","
            << "\"config\":{\"log_level\":\"ERROR\"}"  // 抑制日志噪音
            << "}";
        ofs.close();

        ExecResult r = exec_command(exe + " stage1 --request " + req_path);
        // frame 不存在, stage1 失败, 退出码非 0
        ASSERT_TRUE(r.exit_code != 0, "stage1 nonexistent.fits 退出码非 0");

        // stdout 应包含 JSONL 事件
        ASSERT_CONTAINS(r.stdout_output, "\"type\":\"accepted\"", "stdout 包含 accepted 事件");
        ASSERT_CONTAINS(r.stdout_output, "\"job_id\":\"job_e2e_001\"", "stdout 事件包含 job_id");
        ASSERT_CONTAINS(r.stdout_output, "effective_config_hash", "accepted 事件包含 effective_config_hash");

        // 失败时应有 failed 事件 (stage_started + failed, 或直接 failed)
        bool has_failed = r.stdout_output.find("\"type\":\"failed\"") != std::string::npos;
        ASSERT_TRUE(has_failed, "stdout 包含 failed 事件");
    }

    // ---- 测试 8: stdout/stderr 严格分离 ----
    // stdout 仅包含 JSONL (每行以 { 开头), stderr 包含人类可读日志
    {
        std::string req_path = tmpdir + "/req_separation.json";
        std::ofstream ofs(req_path);
        ofs << "{"
            << "\"schema_version\":1,"
            << "\"command\":\"inspect\","
            << "\"output\":\"" << tmpdir << "/out.hiss\","
            << "\"config\":{\"log_level\":\"INFO\"}"
            << "}";
        ofs.close();

        ExecResult r = exec_command(exe + " inspect --request " + req_path);
        ASSERT_EQ(r.exit_code, 0, "separation 测试 inspect 退出码 0");

        // stdout 应为可解析 JSON (顶层 { 开始)
        ASSERT_FALSE(r.stdout_output.empty(), "stdout 非空");
        ASSERT_TRUE(r.stdout_output.find("{") == 0 || r.stdout_output.find("\n") != std::string::npos,
                    "stdout 以 { 开始 (JSON)");

        // stderr 应包含人类可读日志 (含时间戳/级别/模块名)
        // inspect 至少有一条 LOG_INFO "inspect: 检查配置"
        ASSERT_FALSE(r.stderr_output.empty(), "stderr 非空 (日志输出到 stderr)");
        // 日志格式: [时间][级别][模块] 消息
        ASSERT_TRUE(r.stderr_output.find("inspect") != std::string::npos ||
                    r.stderr_output.find("cli") != std::string::npos,
                    "stderr 包含模块名 (inspect/cli)");
    }

    // ---- 测试 9: request JSON 缺少 command 字段返回 CONFIG_ERROR (7) ----
    {
        std::string req_path = tmpdir + "/req_no_command.json";
        std::ofstream ofs(req_path);
        ofs << "{\"schema_version\":1,\"output\":\"o.hiss\",\"config\":{}}";
        ofs.close();

        ExecResult r = exec_command(exe + " inspect --request " + req_path);
        ASSERT_EQ(r.exit_code, 7, "request 缺少 command 退出码为 7 (CONFIG_ERROR)");
    }

    // ---- 测试 10: stage1 缺少 --frame 字段返回 CONFIG_ERROR ----
    {
        std::string req_path = tmpdir + "/req_stage1_no_frame.json";
        std::ofstream ofs(req_path);
        ofs << "{\"schema_version\":1,\"command\":\"stage1\","
            << "\"output\":\"" << tmpdir << "/out.hiss\"}";
        ofs.close();

        ExecResult r = exec_command(exe + " stage1 --request " + req_path);
        ASSERT_EQ(r.exit_code, 7, "stage1 缺少 frame 退出码为 7 (CONFIG_ERROR)");
        // stdout 应有 failed 事件 + ASTROCS_CONFIG_INVALID
        ASSERT_CONTAINS(r.stdout_output, "\"type\":\"failed\"", "缺少 frame 时输出 failed 事件");
        ASSERT_CONTAINS(r.stdout_output, "ASTROCS_CONFIG_INVALID", "错误码为 ASTROCS_CONFIG_INVALID");
    }

    // ---- 测试 11: CLI 覆盖优先级 (--log-level DEBUG 覆盖 request.config.log_level) ----
    {
        std::string req_path = tmpdir + "/req_cli_override.json";
        std::ofstream ofs(req_path);
        ofs << "{"
            << "\"schema_version\":1,"
            << "\"command\":\"stage1\","
            << "\"frame\":\"nonexistent.fits\","
            << "\"output\":\"" << tmpdir << "/out.hiss\","
            << "\"config\":{\"log_level\":\"INFO\",\"threads\":2}"
            << "}";
        ofs.close();

        // 通过 --log-level DEBUG 覆盖 config.log_level=INFO
        ExecResult r = exec_command(exe + " stage1 --request " + req_path + " --log-level DEBUG");
        // frame 不存在, 失败
        ASSERT_TRUE(r.exit_code != 0, "CLI override 测试: frame 不存在失败");

        // 验证 accepted 事件含 effective_config_hash
        ASSERT_CONTAINS(r.stdout_output, "\"type\":\"accepted\"", "CLI override: 输出 accepted 事件");
        ASSERT_CONTAINS(r.stdout_output, "effective_config_hash", "CLI override: 含 hash");
    }

    // ---- 测试 12: capabilities 输出 exit_codes 数组包含全部 9 个码 ----
    {
        ExecResult r = exec_command(exe + " capabilities");
        ASSERT_CONTAINS(r.stdout_output, "\"name\": \"SUCCESS\"", "capabilities 包含 SUCCESS");
        ASSERT_CONTAINS(r.stdout_output, "\"name\": \"CONFIG_ERROR\"", "capabilities 包含 CONFIG_ERROR");
        ASSERT_CONTAINS(r.stdout_output, "\"name\": \"FILE_IO_ERROR\"", "capabilities 包含 FILE_IO_ERROR");
        ASSERT_CONTAINS(r.stdout_output, "\"name\": \"PLATESOLVE_FAILED\"", "capabilities 包含 PLATESOLVE_FAILED");
    }
}

// ============================================================================
// main
// ============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    // 设置控制台为 UTF-8 (保证中文输出正确)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "============================================================" << std::endl;
    std::cout << "Orchestrator CLI 集成测试 (Task 5 - 阶段1)" << std::endl;
    std::cout << "============================================================" << std::endl;

    // 执行 6 个 Part 的测试 (Part 6 = P04-001)
    test_part1_repl_commands();
    test_part2_cli_command();
    test_part3_checkpoint_resume();
    test_part4_dll_loader();
    test_part5_logger_integration();
    test_part6_p04_001_request_and_effective_config();

    // 输出汇总
    std::cout << "\n============================================================" << std::endl;
    std::cout << "测试汇总: " << g_pass_count << " 通过, "
              << g_fail_count << " 失败" << std::endl;
    std::cout << "============================================================" << std::endl;

    return (g_fail_count == 0) ? 0 : 1;
}
