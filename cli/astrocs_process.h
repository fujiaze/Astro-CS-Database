// cli/astrocs_process.h — 跨平台子进程创建(argv 数组传参, 零 shell 解析)
// Bughunt P1 R8-B: 替换 cli/commands.cpp 的 std::system 字符串拼接族。
//  - std::system 走 shell: 路径空格断裂、引号/元字符注入、timeout 参数未消毒、
//    Windows cmd.exe 语义差异 → 全部通过 argv 直接传参消除。
//  - exit code 语义: 正常结束 = 子进程退出码(0..255, Windows 保留低 8 位之外的
//    位信息不映射); 超时/信号/启动失败 = 负数错误码(见 RunResult)。
// 平台: POSIX fork/execvpe; Windows CreateProcessA。无第三平台。
#ifndef ASTROCS_CLI_PROCESS_H
#define ASTROCS_CLI_PROCESS_H

#include <map>
#include <string>
#include <vector>

namespace astrocs::process {

// 一次子进程执行的完整结果。
struct RunResult {
    int exit_code = -1;        // 正常结束: 子进程退出码; 异常: 见 status 枚举
    bool exited = false;       // true=子进程正常退出(exit/_exit); false=超时/信号/启动失败
    bool timed_out = false;    // 超时杀死
    bool spawn_failed = false; // 进程创建/可执行文件打开失败
    std::string error;         // spawn_failed / 信号终止时的人类可读原因(UTF-8, 无敏感路径)
};

// argv 数组传参启动子进程(零 shell 解析):
//   argv: 以 nullptr 语义结束前的参数向量({exe, arg1, ...}; 不含尾随占位)。
//   extra_env: 追加到当前进程环境的键值对(空=继承环境)。
//   timeout_s: >0 时超时杀进程树并返回 timed_out; <=0 时不限时(禁止用于生产路径)。
//   cwd_utf8: 非空时切换子进程工作目录(POSIX chdir in child; Windows 变宽字符后传 lpCurrentDirectory)。
//   devnull_stdio: true 时子进程 stdout/stderr 重定向到系统空设备
//     (CLI --events-jsonl 模式 stdout 只能是 JSON 事件, 04 §3; 渲染器等噪声子进程必须启用)。
// 线程安全: 可并发调用(无共享静态态); 进程内同时至多数百并发子进程属调用方责任。
RunResult run_process(const std::vector<std::string>& argv,
                      const std::map<std::string, std::string>& extra_env = {},
                      double timeout_s = 0.0,
                      const std::string& cwd_utf8 = {},
                      bool devnull_stdio = false);

// 便捷判断: 子进程正常退出且 exit_code==0。
bool ok(const RunResult& r);

}  // namespace astrocs::process

#endif  // ASTROCS_CLI_PROCESS_H
