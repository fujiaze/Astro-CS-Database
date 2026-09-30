// P1-SESSION-TEST · 故障注入必败自检 (selfcheck): 注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS\n// 的占位"。本可执行三阶段 (对齐 p1hips/p1cos 先例):
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入 A: 子进程以 ACSD_P1SESS_FAULT=u1_manifest_kind 重跑 units
//      → 必 FAIL rc=1 (排除恒 PASS 侧; 子进程 stderr 有 FAULT-INJECT 行)。
//   3) 注入 B: 子进程以 ACSD_P1SESS_FAULT=n1_validate_reject 重跑
//      negative 组 → 必 FAIL (第二注入点覆盖)。
// 注入名与 faultname 注册处 (各测试 TU P1SESS_CHECK 第三参) 对齐;
// ACSD_P1SESS_SELFCHECK_FAULT 可覆盖阶段 2 注入名。
// 注: POSIX fork/execve (Linux CI 主路径); Windows CI 本组不注册。
#ifdef _WIN32
// WIN-PORT: MSVC 无 <sys/wait.h>/<unistd.h>；子进程重跑改用 CRT spawn（见下方 fork 块）。
#include <cstdint>
#include <cstdlib>   // _pgmptr（等价 /proc/self/exe 的可执行文件路径）
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "p1sess_test_main.hpp"

namespace p1sess {
int run_units();
int run_negative();
}  // namespace p1sess

namespace {

// 注入子进程入口: execve 重入后 argv[1] = 组名, 直接跑该组并回传 rc。
// (FaultRegistry 由 ACSD_P1SESS_FAULT 初始化, 机制与主执行器一致;
// 不经 run_all_groups, 必须显式 init —— 对齐 p1hips 先例, 防注入静默失效。)
int run_injected_group(const char* group) {
    p1sess::init_fault_registry_from_env();
    if (std::strcmp(group, "units") == 0) return p1sess::run_units();
    if (std::strcmp(group, "negative") == 0) return p1sess::run_negative();
    return 127;
}

// 子进程注入重跑: execve /proc/self/exe <group> + 注入环境
// 返回子进程 rc (执行失败 → 127)
int run_injected_child(const char* group, const std::string& fault_name) {
    const std::string fault_env = "ACSD_P1SESS_FAULT=" + fault_name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "p1sess_selfcheck";
    char arg1[32];
    std::snprintf(arg1, sizeof(arg1), "%s", group);
    char* child_argv[] = {arg0, arg1, nullptr};
    char* child_env[] = {fbuf.data(), nullptr};

    #ifdef _WIN32
    // WIN-PORT: Windows 无 fork/execve/waitpid。_spawnve(_P_WAIT, ...) 给出等价语义：以同一
    // 可执行文件（_pgmptr）与同一 argv/env 起子进程并阻塞等待，直接取子进程退出码 ——
    // 与 POSIX 支路「注入环境重跑 + 等子进程 + 取退出码」同一判据面。
    const intptr_t rc_win = _spawnve(_P_WAIT, _pgmptr, child_argv, child_env);
    return rc_win < 0 ? 127 : static_cast<int>(rc_win);
    #else
    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 127;
    }
    if (pid == 0) {
        execve("/proc/self/exe", child_argv, child_env);
        _exit(127);  // execve 失败
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    #endif
}

// guard-path 相 (TAUT-NULL-GUARD): 证明「故障名不可用」这条失败路径**可被触发**。
// 旧写法 (faultname) != nullptr 是对字面量地址的编译期恒真比较, 该路径从未被执行过
//（恒真门没有证据资格, AGENTS.md §5）。三态: 合法名⇒0 失败（行为与改写前一致）;
// 空串名⇒1 失败（此前不可达的路径）; nullptr 名⇒0 失败（「非注入点」约定仍然有效）。
static int check_guard_path() {
    {
        p1sess::CheckState cs;
        P1SESS_CHECK(cs, 1 == 1, "guard_path_positive");
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL positive: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        p1sess::CheckState cs;
        P1SESS_CHECK(cs, 1 == 1, "");
        if (cs.failures != 1 || cs.fault_reported) {
            std::fprintf(stderr,
                         "GUARD-PATH FAIL empty_faultname: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        std::fprintf(stdout, "GUARD-PATH empty_faultname reached (failures=1)\n");
    }
    {
        p1sess::CheckState cs;
        P1SESS_CHECK_MSG(cs, 1 == 1, "", "guard path msg %d", 1);
        if (cs.failures != 1) {
            std::fprintf(stderr, "GUARD-PATH FAIL empty_faultname_msg: %d\n", cs.failures);
            return 1;
        }
    }

    {
        p1sess::CheckState cs;
        P1SESS_CHECK(cs, 1 == 1, nullptr);
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL null_faultname: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        p1sess::FaultRegistry::instance().active.push_back("guard_path_live");
        p1sess::CheckState cs;
        P1SESS_CHECK(cs, 1 == 1, "guard_path_live");
        if (cs.failures != 1 || !cs.fault_reported) {
            std::fprintf(stderr, "GUARD-PATH FAIL live injection: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        p1sess::FaultRegistry::instance().active.pop_back();
    }
    std::fprintf(stdout, "GUARD-PATH PASS (empty name reachable + null convention kept)\n");
    return 0;
}
int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS (registry 初始化: env 未设 → 空 → 无注入)
    p1sess::init_fault_registry_from_env();
    {
        const int rc = p1sess::run_units();
        if (rc != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n", rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
    }

    // 阶段 2: 注入 units 组 → 必 FAIL
    {
        const char* fault = std::getenv("ACSD_P1SESS_SELFCHECK_FAULT");
        const std::string name = fault ? fault : "u1_manifest_kind";
        const int child_rc = run_injected_child("units", name);
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject '%s' 后 units 仍 PASS — 恒 PASS 占位, 注入机制失效\n",
                         name.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase2: fault-inject '%s' → child rc=%d (必败验证通过)\n",
                     name.c_str(), child_rc);
    }

    // 阶段 3: 注入 negative 组 → 必 FAIL (第二注入点)
    {
        const int child_rc = run_injected_child("negative", "n1_validate_reject");
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject 'n1_validate_reject' 后 negative 仍 PASS — 恒 PASS 占位\n");
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3: fault-inject 'n1_validate_reject' → child rc=%d (第二注入点通过)\n",
                     child_rc);
    }

    // guard-path 相: 失败路径可被触发的证据（TAUT-NULL-GUARD 判据要求）
    {
        const int guard_rc = check_guard_path();
        if (guard_rc != 0) {
            std::fprintf(stderr, "GUARD-PATH phase FAIL (rc=%d)\n", guard_rc);
            return 1;
        }
    }
    std::fprintf(stdout, "P1SESS SELFCHECK PASS (baseline PASS + 2 injections FAIL)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 注入子进程模式: execve 重入 (argv[1] = 组名) → 只跑该组并回传 rc。
    // 不进入 run_selfcheck, 否则将 fork 递归 (资源耗尽, /tmp 目录爆炸)。
    // (c19b4a59 p1hips 先例同款修复: argv 子进程模式 + registry 初始化。)
    if (argc >= 2 && argv[1][0] != '\0' && argv[1][0] != '-')
        return run_injected_group(argv[1]);
    return run_selfcheck();
}
