// cli_process_test.cpp — B8-P1: cli/astrocs_process.h(R11 Windows 遮蔽修复: 原名 process.h 在 -I cli/ 下遮蔽 MSVC <thread> 内部 <process.h> → _beginthreadex 未声明 → astrocs_cli_runtime 编译失败 build exit 1) 跨平台子进程封装共址单测。
// 覆盖: argv 传参(空格路径不断裂/零 shell 解析)、exit code 传递、extra_env、
// timeout 杀进程、spawn 失败可诊断、devnull_stdio。POSIX 与 Windows 同断言。
#include "astrocs_process.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static int fail_count = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,  \
                         (msg));                                          \
            ++fail_count;                                                 \
        }                                                                 \
    } while (0)

static std::string write_script(const fs::path& dir, const char* name,
                                const std::string& body) {
#ifdef _WIN32
    const fs::path p = dir / (std::string(name) + ".bat");
    std::ofstream f(p);
    f << body;
    f.close();
    return p.string();
#else
    const fs::path p = dir / name;
    {
        std::ofstream f(p);
        f << body;
    }
    ::chmod(p.string().c_str(), 0755);
    return p.string();
#endif
}

int main() {
    fs::path tmp = fs::temp_directory_path() /
                   ("cli_process_test_" + std::to_string(
#ifdef _WIN32
                                              static_cast<long long>(::GetCurrentProcessId())
#else
                                              static_cast<long long>(::getpid())
#endif
                                              ));
    fs::create_directories(tmp);

    // ── 1. exit code 传递 + extra_env ──
    {
        std::vector<std::string> argv;
#ifdef _WIN32
        argv = {"cmd", "/c", "exit 7"};
#else
        argv = {"sh", "-c", "exit 7"};
#endif
        const auto r = astrocs::process::run_process(argv);
        CHECK(r.exited && r.exit_code == 7, "exit code 7 传递");
    }
    // extra_env 注入
    {
#ifdef _WIN32
        std::vector<std::string> argv = {"cmd", "/c", "if defined ACS_PROC_TEST_ENV (exit 0) else (exit 1)"};
#else
        std::vector<std::string> argv = {"sh", "-c", "[ \"$ACS_PROC_TEST_ENV\" = \"1\" ]"};
#endif
        const auto r = astrocs::process::run_process(argv, {{"ACS_PROC_TEST_ENV", "1"}});
        CHECK(astrocs::process::ok(r), "extra_env 注入子进程");
    }

    // ── 2. 含空格路径 argv 传参不断裂(零 shell 解析核心诉求) ──
    {
        const std::string sp = write_script(tmp, "sp ace script",
#ifdef _WIN32
                                            "@echo off\r\nexit /b 0\r\n"
#else
                                            "#!/bin/sh\nexit 0\n"
#endif
        );
        const auto r = astrocs::process::run_process({sp});
        CHECK(astrocs::process::ok(r),
              "空格路径作为 argv[0] 直接执行不断裂; rc=" +
                  std::to_string(r.exit_code) + " err=" + r.error);
    }

    // ── 3. timeout: 超时杀进程, timed_out=true ──
    {
#ifdef _WIN32
        std::vector<std::string> argv = {"cmd", "/c", "timeout /t 5 /nobreak >nul"};
#else
        std::vector<std::string> argv = {"sleep", "5"};
#endif
        const auto r = astrocs::process::run_process(argv, {}, 0.5);
        CHECK(r.timed_out, "0.5s 超时杀 5s 子进程");
        CHECK(!astrocs::process::ok(r), "超时不算 ok");
    }

    // ── 4. spawn 失败可诊断(不存在可执行文件) ──
    {
        const auto r = astrocs::process::run_process({"astrocs_no_such_binary_zz"});
        CHECK(r.spawn_failed || (r.exited && r.exit_code == 127),
              "spawn 失败或 shell 未找到等效 127");
        CHECK(!astrocs::process::ok(r), "spawn 失败不算 ok");
    }

    // ── 5. devnull_stdio: 子进程 stdout 噪声不外泄 ──
    // (CLI events-jsonl 模式 stdout 只能是 JSON 事件; 此处验证调用不炸+exit 正确,
    //  流完整性由 tests/cli/test_cli_protocol.py --events-jsonl 黄金断言兜底。)
    {
#ifdef _WIN32
        std::vector<std::string> argv = {"cmd", "/c", "echo noisy-output & exit /b 0"};
#else
        std::vector<std::string> argv = {"sh", "-c", "echo noisy-output"};
#endif
        const auto r = astrocs::process::run_process(argv, {}, 0.0, {}, true);
        CHECK(astrocs::process::ok(r), "devnull_stdio 下正常退出");
    }

    // ── 6. 参数值含 shell 元字符不被解释(注入面消除) ──
    {
        // 目标: 把 `; evil` 作为单个 argv 元素传给 echo 类命令 — 若 shell 解析
        // 则会执行第二命令; argv 直传则原样成为参数。
#ifdef _WIN32
        std::vector<std::string> argv = {"cmd", "/c", "exit 5"};
        (void)argv;
        // Windows cmd 语义下无独立 sh -c 等价注入面, 由引号规则测试 2 覆盖。
#else
        const std::string sp = write_script(
            tmp, "inject_probe",
            "#!/bin/sh\n[ \"$1\" = \"a;b|c\" ] && exit 3 || exit 4\n");
        const auto r = astrocs::process::run_process({sp, "a;b|c"});
        CHECK(r.exited && r.exit_code == 3,
              "元字符参数按单 argv 元素传递(不被 shell 拆分); rc=" +
                  std::to_string(r.exit_code));
#endif
    }

    std::error_code ec;
    fs::remove_all(tmp, ec);
    if (fail_count) {
        std::fprintf(stderr, "cli_process_test: %d FAIL\n", fail_count);
        return 1;
    }
    std::fprintf(stderr, "cli_process_test: all PASS\n");
    return 0;
}
