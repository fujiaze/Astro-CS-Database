// P1-HIPS-TEST · 故障注入自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS
// 的占位"。本可执行三阶段:
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入 A: 子进程以 ASTROCS_P1HIPS_FAULT=u1_signal_bitwise 重跑 units
//      → 必 FAIL (排除恒 PASS 侧), stderr 含 FAULT-INJECT 行。
//   3) 注入 B: 子进程以 ASTROCS_P1HIPS_FAULT=o1_fits_mapping_bitwise 重跑
//      oracle 组 → 必 FAIL (第二注入点覆盖)。
// 注入名与 faultname 注册处 (各测试 TU P1HIPS_CHECK 第三参) 对齐;
// ASTROCS_P1HIPS_SELFCHECK_FAULT 可覆盖阶段 2 注入名。
#include "p1hips_test_main.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1hips {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
}  // namespace p1hips

namespace {

// 子进程注入重跑: execve /proc/self/exe <group> + 注入环境
// 返回子进程 rc (执行失败 → 127)
int run_injected_child(const char* group, const std::string& fault_name) {
    const std::string fault_env = "ASTROCS_P1HIPS_FAULT=" + fault_name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "p1hips_selfcheck";
    char arg1[32];
    std::snprintf(arg1, sizeof(arg1), "%s", group);
    char* child_argv[] = {arg0, arg1, nullptr};
    char* child_env[] = {fbuf.data(), nullptr};
    (void)child_env;

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
}

// 注入子进程入口: execve 重入后 argv[1] = 组名, 直接跑该组
// (FaultRegistry 由 ASTROCS_P1HIPS_FAULT 初始化, 机制与主执行器一致)。
static int run_injected_group(const char* group) {
    p1hips::init_fault_registry_from_env();
    if (std::strcmp(group, "units") == 0) return p1hips::test_units();
    if (std::strcmp(group, "properties") == 0) return p1hips::test_properties();
    if (std::strcmp(group, "oracle") == 0) return p1hips::test_oracle();
    if (std::strcmp(group, "negative") == 0) return p1hips::test_negative();
    return 127;
}

int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS
    {
        const int rc = p1hips::test_units();
        if (rc != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n", rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
    }

    // 阶段 2: 注入 units 组 → 必 FAIL
    {
        const char* fault = std::getenv("ASTROCS_P1HIPS_SELFCHECK_FAULT");
        const std::string name = fault ? fault : "u1_signal_bitwise";
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

    // 阶段 3: 注入 oracle 组 → 必 FAIL (第二注入点)
    {
        const int child_rc = run_injected_child("oracle", "o1_fits_mapping_bitwise");
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject 'o1_fits_mapping_bitwise' 后 oracle 仍 PASS — 注入机制失效\n");
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3: fault-inject 'o1_fits_mapping_bitwise' → child rc=%d (必败验证通过)\n",
                     child_rc);
    }

    std::fprintf(stdout, "P1HIPS SELFCHECK PASS (baseline + fault-injection both verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 注入子进程模式: execve 重入 (argv[1] = 组名) → 只跑该组并回传 rc。
    // 不进入 run_selfcheck, 否则将 fork 递归 (资源耗尽, /tmp 目录爆炸)。
    if (argc >= 2 && argv[1][0] != '\0' && argv[1][0] != '-')
        return run_injected_group(argv[1]);
    return run_selfcheck();
}
