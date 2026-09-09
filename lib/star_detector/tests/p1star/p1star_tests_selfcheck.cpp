// ============================================================================
// p1star_tests_selfcheck.cpp — P1-STAR-TEST 故障注入必败自检 (双向排除恒常)
// ----------------------------------------------------------------------------
// 验收 (MODULE_MIGRATION_TEMPLATE <prefix>-TEST): "故障注入能让测试失败" +
// "不得写永远 PASS 的占位"。结构先例 P1-HIPS-TEST (commit c19b4a59,
// p1hips_tests_selfcheck.cpp argv 子进程模式) 同构 — 本可执行三阶段:
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入 A: fork+execve /proc/self/exe units + ASTROCS_P1STAR_FAULT=
//      f1_detect_rc → units 必 FAIL (排除恒 PASS 侧, 注入点 1)。
//   3) 注入 B: fork+execve /proc/self/exe oracle + ASTROCS_P1STAR_FAULT=
//      f4_oracle_rc → oracle 必 FAIL (第二注入点覆盖)。
// 注入名与 p1star_tests_core.cpp 断言注册名对齐 (P1STAR_CHECK 第三参);
// ASTROCS_P1STAR_SELFCHECK_FAULT_A/B 可覆盖注入名。
// 子进程模式防递归: execve 重入后 argv[1]=组名 → 只跑该组并回传 rc,
// 不进入 run_selfcheck (否则 fork 递归资源耗尽, 先例登记缺陷)。
// ============================================================================
#include "star_detector.h"

#include <unistd.h>

#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1star {
int test_units();
int test_oracle();
}  // namespace p1star

namespace {

// 注入子进程: execve /proc/self/exe <group> + 注入环境 (fork+execve 双注入点
// 机制与先例一致; env 显式固定 OMP_NUM_THREADS=1 对齐 ctest 注册)。
int run_injected_child(const char* group, const std::string& fault_name) {
    const std::string fault_env = "ASTROCS_P1STAR_FAULT=" + fault_name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');
    const std::string omp_env = "OMP_NUM_THREADS=1";
    std::vector<char> obuf(omp_env.begin(), omp_env.end());
    obuf.push_back('\0');

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 127;
    }
    if (pid == 0) {
        char arg0[] = "p1star_selfcheck";
        char* child_argv[] = {arg0, const_cast<char*>(group), nullptr};
        char* child_env[] = {fbuf.data(), obuf.data(), nullptr};
        execve("/proc/self/exe", child_argv, child_env);
        _exit(127);  // execve 失败
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS (FaultRegistry 无注入 — ctest 注册
    // ASTROCS_P1STAR_FAULT= 置空; 防外部环境残留, 显式 unset)
    unsetenv("ASTROCS_P1STAR_FAULT");
    {
        const int rc = p1star::test_units();
        if (rc != 0) {
            std::fprintf(stderr,
                         "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n", rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
        std::fflush(stdout);
    }

    // 阶段 2: 注入 units 组 → 必 FAIL (注入点 1)
    {
        const char* fault = std::getenv("ASTROCS_P1STAR_SELFCHECK_FAULT_A");
        const std::string name = fault && *fault ? fault : "f1_detect_rc";
        const int child_rc = run_injected_child("units", name);
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject '%s' 后 units 仍 PASS — 恒 PASS 占位, "
                         "注入机制失效\n", name.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase2: fault-inject '%s' → child rc=%d (必败验证通过)\n",
                     name.c_str(), child_rc);
        std::fflush(stdout);
    }

    // 阶段 3: 注入 oracle 组 → 必 FAIL (注入点 2)
    {
        const char* fault = std::getenv("ASTROCS_P1STAR_SELFCHECK_FAULT_B");
        const std::string name = fault && *fault ? fault : "f4_oracle_rc";
        const int child_rc = run_injected_child("oracle", name);
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject '%s' 后 oracle 仍 PASS — 注入机制失效\n",
                         name.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3: fault-inject '%s' → child rc=%d (必败验证通过)\n",
                     name.c_str(), child_rc);
        std::fflush(stdout);
    }

    std::fprintf(stdout, "P1STAR SELFCHECK PASS (baseline + fault-injection both verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 注入子进程模式: execve 重入 (argv[1] = 组名) → 只跑该组并回传 rc。
    // FaultRegistry 由 CheckState 构造时从 ASTROCS_P1STAR_FAULT 初始化,
    // 机制与主执行器一致 (run_injected_child 注入)。
    if (argc >= 2 && argv[1][0] != '\0' && argv[1][0] != '-')
        return std::strcmp(argv[1], "units") == 0 ? p1star::test_units()
                                                  : p1star::test_oracle();
    return run_selfcheck();
}
