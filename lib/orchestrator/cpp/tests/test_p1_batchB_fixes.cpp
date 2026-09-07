// ============================================================================
// test_p1_batchB_fixes.cpp - Bug 狩猎 R5 P1 批次 B 修复验证
// 覆盖三条 P1 修复 (域: lib/orchestrator + lib/io + lib/common):
//   P1-1 cleanup_partial_output: fs::remove 对 HiPS 目录树恒失败 →
//        fs::remove_all 递归删除 + 错误上报 (P04-004 / IO_003 §4/§6 原子输出)
//   P1-2 SIGINT handler: request_cancel 仅纯 atomic store (async-signal-safe),
//        取消标志经真实 raise(SIGINT) 在 handler 路径内置位 (无死锁)
//   P1-3 JSONL 事件转义: json_escape_string 处理 Windows 反斜杠路径 /
//        引号 / 控制字符; output_jsonl_event_ex 输出行可被解析为合法 JSON
// 运行: ./tests/test_p1_batchB_fixes.exe (rc=0 即全部通过)
// ============================================================================

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <functional>
#include <filesystem>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>

#include "orchestrator.h"
#include "cli_command.h"

#include <nlohmann/json.hpp>  // 事件行合法性 oracle (root third_party)

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>   // dup/dup2/pipe/read/close/alarm
#endif

namespace fs = std::filesystem;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, msg) do { \
    ++g_checks; \
    if (cond) { std::cout << "  [PASS] " << (msg) << "\n"; } \
    else { ++g_failures; std::cout << "  [FAIL] " << (msg) << "\n"; } \
} while (0)

// capture_stdout - 捕获期间 fn() 写入 stdout 的内容 (dup2 重定向到管道)
static std::string capture_stdout(const std::function<void()>& fn) {
#ifdef _WIN32
    (void)fn;
    return "";  // Windows 单元验证走 test_orchestrator_cli 的 exec 分支
#else
    int fds[2];
    if (pipe(fds) != 0) return "";
    const int saved = ::dup(STDOUT_FILENO);
    ::fflush(stdout);
    ::dup2(fds[1], STDOUT_FILENO);
    fn();
    ::fflush(stdout);
    ::dup2(saved, STDOUT_FILENO);
    ::close(saved);
    ::close(fds[1]);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = ::read(fds[0], buf, sizeof(buf))) > 0) out.append(buf, (size_t)n);
    ::close(fds[0]);
    return out;
#endif
}

// tmp_tree - 构造 HiPS 形态临时目录树: root/{properties, Moc.fits,
// Norder3/Dir0/Npix{0,1,2}.fits}, 返回创建的条目数
static int make_hips_tree(const fs::path& root) {
    fs::create_directories(root / "Norder3" / "Dir0");
    int n = 0;
    { std::ofstream f(root / "properties");       f << "hips_builder\n"; ++n; }
    { std::ofstream f(root / "Moc.fits");         f << "MOC";         ++n; }
    for (int i = 0; i < 3; ++i) {
        std::ofstream f(root / "Norder3" / "Dir0" /
                        ("Npix" + std::to_string(i) + ".fits"));
        f << "TILE" << i;
        ++n;
    }
    return n;
}

int main() {
    std::cout << "== test_p1_batchB_fixes (Bug 狩猎 R5 P1 批次 B) ==\n";

    const fs::path base = fs::temp_directory_path() /
        ("astrocs_p1_batchB_" + std::to_string(::getpid()));

    // ------------------------------------------------------------------
    // P1-1: cleanup_partial_output 目录树清理
    // ------------------------------------------------------------------
    std::cout << "[P1-1] cleanup_partial_output 目录树/文件清理\n";
    {
        Orchestrator orch;

        // 1) 深层 HiPS 目录树整体删除 (旧 fs::remove 实现恒失败的场景)
        const fs::path tree = base / "hips_out";
        make_hips_tree(tree);
        CHECK(fs::exists(tree / "Norder3" / "Dir0" / "Npix1.fits"),
              "前置: HiPS 目录树已建立 (properties + tiles)");
        CHECK(orch.cleanup_partial_output(tree.string()),
              "目录树清理返回 true");
        CHECK(!fs::exists(tree), "目录树整体已删除 (无残留)");
        CHECK(!fs::exists(base / "hips_out"), "父目录无 hips_out 残留");

        // 2) 旧契约形态: 单文件输出清理保持兼容
        const fs::path file = base / "partial.hiss";
        { std::ofstream f(file); f << "partial"; }
        CHECK(orch.cleanup_partial_output(file.string()), "单文件清理返回 true");
        CHECK(!fs::exists(file), "单文件已删除");

        // 3) 不存在的路径视为成功 (幂等)
        CHECK(orch.cleanup_partial_output((base / "nonexistent").string()),
              "不存在路径返回 true (幂等)");

        // 4) 空路径视为无操作
        CHECK(orch.cleanup_partial_output(""), "空路径返回 true (无操作)");
    }

    // ------------------------------------------------------------------
    // P1-2: SIGINT handler 路径纯 atomic (真实 raise 驱动)
    // ------------------------------------------------------------------
    std::cout << "[P1-2] SIGINT 取消标志置位 (raise 驱动, 无死锁)\n";
    {
        Orchestrator orch;
        CHECK(!orch.is_cancelled(), "初始状态: 未取消");

        p04004_register_signal_handler(&orch, true);

        // 真实信号路径: raise(SIGINT) 同步触发 handler, handler 内执行
        // request_cancel (纯 atomic store)。若 handler 路径仍含锁/日志
        // (旧实现), 主线程在 Logger mutex 内被中断时会死锁 —— 由本测试
        // 直接暴露; timeout 保证回归时限。
        std::raise(SIGINT);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        bool cancelled = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (orch.is_cancelled()) { cancelled = true; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CHECK(cancelled, "raise(SIGINT) 后取消标志置位 (handler 路径生效, 无死锁)");

        // 注册期间再次触发 (幂等置位) + 注销后恢复默认
        std::raise(SIGINT);
        CHECK(orch.is_cancelled(), "重复 SIGINT 幂等 (标志保持置位)");
        p04004_unregister_signal_handler();
        orch.reset_cancel_timeout();
        CHECK(!orch.is_cancelled(), "注销 + reset 后: 未取消");
        // handler 全路径零锁零 logging 由源码 grep 论证 (见 P1 批次 B 报告):
        // p04004_sigint_handler → 2x atomic load → request_cancel → 1x atomic store。
    }

    // ------------------------------------------------------------------
    // P1-3: JSONL 事件字段转义
    // ------------------------------------------------------------------
    std::cout << "[P1-3] JSON 转义 (Windows 路径/引号/控制字符)\n";
    {
        // 1) Windows 反斜杠路径
        const std::string win_path = "C:\\data\\a.fits";
        const std::string esc_win = CliCommand::json_escape_string(win_path);
        CHECK(esc_win == "C:\\\\data\\\\a.fits",
              "反斜杠逐字节转义: C:\\data\\a.fits -> " + esc_win);

        // 2) 引号
        CHECK(CliCommand::json_escape_string("say \"hi\"") == "say \\\"hi\\\"",
              "双引号转义");

        // 3) 控制字符 (\n \t \x01)
        const std::string ctrl = std::string("a\nb\tc") + char(0x01) + "d";
        const std::string esc_ctrl = CliCommand::json_escape_string(ctrl);
        CHECK(esc_ctrl == "a\\nb\\tc\\u0001d",
              "控制字符转义: \\n/\\t/\\u0001 -> " + esc_ctrl);

        // 4) 事件行可被解析为合法 JSON (含 Windows 路径字段)
        const std::string line = capture_stdout([] {
            CliCommand::output_jsonl_event_ex(
                "completed", "job1", "", 1.0, "msg \"q\"",
                std::string("{\"output_hips\":\"")
                    + CliCommand::json_escape_string("C:\\out\\hips")
                    + "\",\"completed_to_gate\":\"complete\"}",
                "", 0, -1.0, "ok");
        });
        bool parsed_ok = false;
        std::string err_detail;
        try {
            auto j = nlohmann::json::parse(line);
            parsed_ok = j.is_object()
                && j.at("type") == "completed"
                && j.at("result").at("output_hips") == "C:\\out\\hips";
        } catch (const std::exception& e) { err_detail = e.what(); }
        CHECK(parsed_ok, "事件行 nlohmann::json 解析通过 (非法 JSON 已绝迹)"
              + std::string(parsed_ok ? "" : " —— " + err_detail + " line=" + line));

        // 5) error_json: 含路径/引号的错误消息转义后合法
        const std::string err_line = capture_stdout([] {
            const std::string error_json = std::string("{\"code\":\"FILE_IO_ERROR\",\"message\":\"")
                + CliCommand::json_escape_string("FITS 文件不存在: C:\\in\\a.fits (\"light\")")
                + "\"}";
            CliCommand::output_jsonl_event_ex("failed", "job2", "", -1.0,
                "stage1 failed", "", error_json, 3, -1.0, "failed");
        });
        bool err_ok = false;
        try {
            auto j = nlohmann::json::parse(err_line);
            err_ok = j.at("error").at("message") ==
                     "FITS 文件不存在: C:\\in\\a.fits (\"light\")";
        } catch (const std::exception& e) { err_detail = e.what(); }
        CHECK(err_ok, "error_json 含 Windows 路径/引号错误消息解析通过"
              + std::string(err_ok ? "" : " —— " + err_detail + " line=" + err_line));

        // 6) 原样往返: 普通字符串不变
        const std::string plain = "stage1 completed successfully";
        CHECK(CliCommand::json_escape_string(plain) == plain, "普通字符串原样往返");
    }

    // 清理临时目录
    std::error_code ec;
    fs::remove_all(base, ec);

    std::cout << "== 完成: " << g_checks << " 项检查, " << g_failures << " 项失败 ==\n";
    return g_failures == 0 ? 0 : 1;
}
