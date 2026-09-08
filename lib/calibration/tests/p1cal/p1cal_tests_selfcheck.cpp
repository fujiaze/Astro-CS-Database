// P1-CAL-TEST · 故障注入自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS
// 的占位"。本可执行两阶段:
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入: 子进程以 ASTROCS_P1CAL_FAULT=<name> 重跑本二进制 → 必 FAIL
//      (排除恒 PASS 侧), stderr 含 FAULT-INJECT 行。
// 注入名与 faultname 注册处 (p1cal_tests_core.cpp P1CAL_CHECK 第三参) 对齐:
//   const_field_bitwise / gradient_std_oracle / darkopt_actual_k_identity /
//   out_not_written_on_param_error / clip_rejects_spike / determinism_bitwise /
//   hot_mask_filtered_iso_plus_L / median_fix_oracle / idw_fix_oracle / ...
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int p1cal_run_core_groups(int argc, char** argv);

namespace {

int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS
    {
        char arg0[] = "p1cal_selfcheck";
        char arg1[] = "units";
        char* argv[] = {arg0, arg1, nullptr};
        const int rc = p1cal_run_core_groups(2, argv);
        if (rc != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n", rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
    }

    // 阶段 2: 子进程注入 → 必 FAIL
    const char* fault = std::getenv("P1CAL_SELFCHECK_FAULT");
    const std::string name = fault ? fault : "const_field_bitwise";
    std::string fault_env = "ASTROCS_P1CAL_FAULT=" + name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "p1cal_selfcheck";
    char arg1[] = "units";
    char* child_argv[] = {arg0, arg1, nullptr};
    char* child_env[] = {fbuf.data(), nullptr};

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 1;
    }
    if (pid == 0) {
        execve("/proc/self/exe", child_argv, child_env);
        _exit(127);  // execve 失败
    }
    int status = 0;
    waitpid(pid, &status, 0);
    const int child_rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (child_rc == 0) {
        std::fprintf(stderr,
                     "SELFCHECK: fault-inject '%s' 后 units 仍 PASS — 恒 PASS 占位, 注入机制失效\n",
                     name.c_str());
        return 1;
    }
    std::fprintf(stdout,
                 "SELFCHECK phase2: fault-inject '%s' → child rc=%d (必败验证通过)\n",
                 name.c_str(), child_rc);
    std::fprintf(stdout, "P1CAL SELFCHECK PASS (baseline + fault-injection both verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 带注入环境重入: 子进程直接跑指定组 (FaultRegistry 已由框架装载)
    if (std::getenv("ASTROCS_P1CAL_FAULT") != nullptr && argc >= 2) {
        return p1cal_run_core_groups(argc, argv);
    }
    return run_selfcheck();
}
