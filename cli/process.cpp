// cli/process.cpp — 跨平台子进程创建实现(argv 数组传参, 零 shell 解析)
// 见 process.h 头注释。POSIX: fork/execvp/waitpid 轮询; Windows: CreateProcessA。
// 已知平台限制(与项目既有 CLI 一致): Windows 用 ANSI API(GetModuleFileNameA 同级),
// 非 ASCII 路径由系统 ANSI 代码页解释; Linux amd64 是本轮技术验证平台。
#include "astrocs_process.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <thread>
#else
#include <windows.h>

#include <vector>
#endif

namespace astrocs::process {

namespace {
constexpr double kPollIntervalMs = 10.0;  // POSIX waitpid(WNOHANG) 轮询间隔
}

#if defined(_WIN32)

namespace {
// Windows 命令行引用规则(与 CommandLineToArgvW 解析互逆): 含空格/tab/引号的
// 参数加引号, 内部引号与尾随反斜杠按微软规则转义。
std::string quote_windows_arg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos) return a;
    std::string out;
    out += '"';
    size_t backslashes = 0;
    for (const char c : a) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out += '"';
        } else {
            out.append(backslashes, '\\');
            out += c;
        }
        backslashes = 0;
    }
    out.append(backslashes * 2, '\\');
    out += '"';
    return out;
}
}  // namespace

RunResult run_process(const std::vector<std::string>& argv,
                      const std::map<std::string, std::string>& extra_env,
                      double timeout_s, const std::string& cwd_utf8,
                      bool devnull_stdio) {
    RunResult r;
    if (argv.empty() || argv[0].empty()) {
        r.spawn_failed = true;
        r.error = "run_process: empty argv";
        return r;
    }
    // 附加环境变量: 父进程设置后由子进程继承(CLI 此处单线程调用, 无竞争窗口)。
    for (const auto& [k, v] : extra_env) SetEnvironmentVariableA(k.c_str(), v.c_str());

    std::string cmdline;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) cmdline += ' ';
        cmdline += quote_windows_arg(argv[i]);
    }
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    if (devnull_stdio) {
        // STARTF_USESTDHANDLES + 无效句柄 → 子进程 stdio 指向空设备等效语义:
        // 用 NUL 设备句柄保证 GetStdHandle 侧行为一致。
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        const HANDLE nul = CreateFileA("NUL", GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       &sa, OPEN_EXISTING, 0, nullptr);
        if (nul != INVALID_HANDLE_VALUE) {
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput = nul;
            si.hStdOutput = nul;
            si.hStdError = nul;
        }
    }
    PROCESS_INFORMATION pi{};
    const char* cwd_ptr = cwd_utf8.empty() ? nullptr : cwd_utf8.c_str();
    const BOOL created = CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, devnull_stdio,
                                        CREATE_NO_WINDOW, nullptr, cwd_ptr, &si, &pi);
    if (!created) {
        r.spawn_failed = true;
        r.error = "CreateProcessA failed (gle=" + std::to_string(GetLastError()) + ")";
        return r;
    }
    const DWORD wait_ms = timeout_s > 0
                              ? static_cast<DWORD>(timeout_s * 1000.0)
                              : INFINITE;
    const DWORD wr = WaitForSingleObject(pi.hProcess, wait_ms);
    if (wr == WAIT_FAILED) {
        r.spawn_failed = true;
        r.error = "WaitForSingleObject failed (gle=" + std::to_string(GetLastError()) + ")";
    } else if (wr == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, static_cast<UINT>(-1));
        WaitForSingleObject(pi.hProcess, 5000);
        r.timed_out = true;
    } else {
        DWORD code = 0;
        if (GetExitCodeProcess(pi.hProcess, &code)) {
            r.exited = true;
            r.exit_code = static_cast<int>(code);
        } else {
            r.error = "GetExitCodeProcess failed (gle=" + std::to_string(GetLastError()) + ")";
        }
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return r;
}

#else  // POSIX

RunResult run_process(const std::vector<std::string>& argv,
                      const std::map<std::string, std::string>& extra_env,
                      double timeout_s, const std::string& cwd_utf8,
                      bool devnull_stdio) {
    RunResult r;
    if (argv.empty() || argv[0].empty()) {
        r.spawn_failed = true;
        r.error = "run_process: empty argv";
        return r;
    }
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    const bool bounded = timeout_s > 0;
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(timeout_s));

    const pid_t pid = ::fork();
    if (pid < 0) {
        r.spawn_failed = true;
        r.error = std::string("fork: ") + std::strerror(errno);
        return r;
    }
    if (pid == 0) {
        // child: exec 失败只能 _exit(约定 127=exec 失败, 126=chdir 失败)。
        for (const auto& [k, v] : extra_env) ::setenv(k.c_str(), v.c_str(), 1);
        if (devnull_stdio) {
            const int devnull = ::open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                ::dup2(devnull, STDOUT_FILENO);
                ::dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO) ::close(devnull);
            }
        }
        if (!cwd_utf8.empty() && ::chdir(cwd_utf8.c_str()) != 0) ::_exit(126);
        ::execvp(cargv[0], cargv.data());
        ::_exit(127);
    }
    // parent: 轮询 waitpid(WNOHANG); EINTR 重试; 超时 SIGKILL 并收割。
    int status = 0;
    for (;;) {
        const pid_t wr = ::waitpid(pid, &status, WNOHANG);
        if (wr == pid) break;
        if (wr < 0) {
            if (errno == EINTR) continue;
            r.spawn_failed = true;
            r.error = std::string("waitpid: ") + std::strerror(errno);
            return r;
        }
        if (bounded && std::chrono::steady_clock::now() >= deadline) {
            ::kill(pid, SIGKILL);
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            r.timed_out = true;
            return r;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<long long>(kPollIntervalMs)));
    }
    if (WIFEXITED(status)) {
        r.exited = true;
        r.exit_code = WEXITSTATUS(status);
        return r;
    }
    if (WIFSIGNALED(status)) {
        r.error = "terminated by signal " + std::to_string(WTERMSIG(status));
        return r;
    }
    r.error = "unknown wait status";
    return r;
}

#endif

bool ok(const RunResult& r) { return r.exited && r.exit_code == 0; }

}  // namespace astrocs::process
