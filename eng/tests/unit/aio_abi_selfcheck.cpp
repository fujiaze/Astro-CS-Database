// AIO-001 · 故障注入必败自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 模式对齐: lib/infrastructure/aio/tests/p1hips/p1hips_tests_selfcheck.cpp。
// 验收 (模板): "故障注入能让测试失败" + "不得写永远 PASS 的占位"。三阶段:
//   1) 基线: 无注入跑 units+negative → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入 A: 子进程 ACSD_AIO_FAULT=n1_hash_value_flip 重跑 units → 必 FAIL。
//   3) 注入 B: 子进程 ACSD_AIO_FAULT=n2_verify_mismatch_shortcut 重跑
//      negative → 必 FAIL; 注入 C: n3_file_size_skip 重跑 negative → 必 FAIL。
// 注入名与 aio_abi.cpp FaultRegistry 注册处对齐;
// ACSD_AIO_SELFCHECK_FAULT 可覆盖阶段 2 注入名。
#include "aio_abi_test_main.hpp"

#ifdef _WIN32
// WIN-PORT: MSVC 无 <sys/wait.h>/<unistd.h>；子进程重跑改用 CRT spawn（见下）。
#include <process.h>
#include <cstdlib>   // _pgmptr（等价 /proc/self/exe 的可执行文件路径）
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace aio_abi_test {
int test_units(CheckState& cs);
int test_negative(CheckState& cs);
}  // namespace aio_abi_test

namespace {

using ::aio_abi_test::CheckState;
using ::aio_abi_test::FaultRegistry;

// 子进程注入重跑: execve /proc/self/exe <group> + 注入环境; 返回子进程 rc
int run_injected_child(const char* group, const std::string& fault_name) {
    const std::string fault_env = "ACSD_AIO_FAULT=" + fault_name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "aio_abi_selfcheck";
    char arg1[32];
    std::snprintf(arg1, sizeof(arg1), "%s", group);
    char* child_argv[] = {arg0, arg1, nullptr};
    char* child_env[] = {fbuf.data(), nullptr};

#ifdef _WIN32
    // WIN-PORT: Windows 无 fork/execve/waitpid。_spawnve(_P_WAIT, ...) 给出等价语义：
    // 用同一可执行文件（_pgmptr）以同一 argv/env 起子进程并阻塞等待，直接返回其
    // 退出码 —— 与 POSIX 支路的"注入环境 + 等子进程 + 取退出码"判据一致。
    const intptr_t rc = _spawnve(_P_WAIT, _pgmptr, child_argv, child_env);
    return rc < 0 ? 127 : static_cast<int>(rc);
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

// 注入子进程入口: execve 重入后 argv[1] = 组名, 只跑该组并回传 rc
static int run_injected_group(const char* group) {
    ::aio_abi_test::init_fault_registry_from_env();
    CheckState cs;
    if (std::strcmp(group, "units") == 0) {
        const int rc = ::aio_abi_test::test_units(cs);
        if (rc != 0 || cs.failures != 0) return 1;
        return 0;
    }
    if (std::strcmp(group, "negative") == 0) {
        const int rc = ::aio_abi_test::test_negative(cs);
        if (rc != 0 || cs.failures != 0) return 1;
        return 0;
    }
    return 127;
}

// guard-path 相 (TAUT-NULL-GUARD): 证明「故障名不可用」这条失败路径**可被触发**。
// 旧写法 (faultname) != nullptr 是对字面量地址的编译期恒真比较, 该路径从未被执行过
//（恒真门没有证据资格, AGENTS.md §5）。三态: 合法名⇒0 失败（行为与改写前一致）;
// 空串名⇒1 失败（此前不可达的路径）; nullptr 名⇒0 失败（「非注入点」约定仍然有效）。
static int check_guard_path() {
    {
        ::aio_abi_test::CheckState cs;
        AIO_CHECK(cs, 1 == 1, "guard_path_positive");
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL positive: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        ::aio_abi_test::CheckState cs;
        AIO_CHECK(cs, 1 == 1, "");
        if (cs.failures != 1 || cs.fault_reported) {
            std::fprintf(stderr,
                         "GUARD-PATH FAIL empty_faultname: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        std::fprintf(stdout, "GUARD-PATH empty_faultname reached (failures=1)\n");
    }
    {
        ::aio_abi_test::CheckState cs;
        AIO_CHECK_MSG(cs, 1 == 1, "", "guard path msg %d", 1);
        if (cs.failures != 1) {
            std::fprintf(stderr, "GUARD-PATH FAIL empty_faultname_msg: %d\n", cs.failures);
            return 1;
        }
    }

    {
        ::aio_abi_test::CheckState cs;
        AIO_CHECK(cs, 1 == 1, nullptr);
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL null_faultname: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        ::aio_abi_test::FaultRegistry::instance().active.push_back("guard_path_live");
        ::aio_abi_test::CheckState cs;
        AIO_CHECK(cs, 1 == 1, "guard_path_live");
        if (cs.failures != 1 || !cs.fault_reported) {
            std::fprintf(stderr, "GUARD-PATH FAIL live injection: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        ::aio_abi_test::FaultRegistry::instance().active.pop_back();
    }
    std::fprintf(stdout, "GUARD-PATH PASS (empty name reachable + null convention kept)\n");
    return 0;
}
int run_selfcheck() {
    // 阶段 1: 基线 units+negative 必 PASS
    {
        CheckState cs;
        int rc = ::aio_abi_test::test_units(cs);
        if (rc != 0 || cs.failures != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline units FAIL — 恒 FAIL 侧不通过\n");
            return 1;
        }
        CheckState cs2;
        rc = ::aio_abi_test::test_negative(cs2);
        if (rc != 0 || cs2.failures != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline negative FAIL — 恒 FAIL 侧不通过\n");
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units+negative PASS (非恒FAIL)\n");
    }

    // 阶段 2: 注入 units (sha256 值翻转) → 必 FAIL
    {
        const char* fault = std::getenv("ACSD_AIO_SELFCHECK_FAULT");
        const std::string name = fault ? fault : "n1_hash_value_flip";
        const int child_rc = run_injected_child("units", name);
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject '%s' 后 units 仍 PASS — 恒 PASS 占位, 注入机制失效\n",
                         name.c_str());
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase2: fault-inject '%s' → child rc=%d (必败验证通过)\n",
                     name.c_str(), child_rc);
    }

    // 阶段 3: 注入 negative (篡改短路) → 必 FAIL
    {
        const int child_rc = run_injected_child("negative", "n2_verify_mismatch_shortcut");
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject 'n2_verify_mismatch_shortcut' 后 negative 仍 PASS — 注入机制失效\n");
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3: fault-inject 'n2_verify_mismatch_shortcut' → child rc=%d (必败验证通过)\n",
                     child_rc);
    }

    // 阶段 4: 注入 negative (文件大小核对跳过) → 必 FAIL (第三注入点)
    {
        const int child_rc = run_injected_child("negative", "n3_file_size_skip");
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject 'n3_file_size_skip' 后 negative 仍 PASS — 注入机制失效\n");
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase4: fault-inject 'n3_file_size_skip' → child rc=%d (必败验证通过)\n",
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
    std::fprintf(stdout, "AIO-ABI SELFCHECK PASS (baseline + fault-injection all verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 注入子进程模式: execve 重入 (argv[1] = 组名) → 只跑该组并回传 rc。
    if (argc >= 2 && std::strcmp(argv[1], "units") == 0) return run_injected_group("units");
    if (argc >= 2 && std::strcmp(argv[1], "negative") == 0) return run_injected_group("negative");

    ::aio_abi_test::init_fault_registry_from_env();
    if (argc >= 2) {
        if (std::strcmp(argv[1], "selfcheck") == 0) return run_selfcheck();
        if (std::strcmp(argv[1], "all") == 0) {
            if (run_selfcheck() != 0) return 1;
            return 0;
        }
        std::fprintf(stderr, "usage: aio_abi_tests [units|negative|selfcheck|all]\n");
        return 127;
    }
    return run_selfcheck();
}
