// P1-WCS-TEST · 故障注入自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS
// 的占位"。本可执行四阶段 (对齐先例 c19b4a59 三阶段 + 任务规格
// "fork/execve 双注入点必败自检, argv 子进程模式"):
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入点 A (env): fork + execve(/proc/self/exe, ["p1wcs_selfcheck",
//      "units"], env=ASTROCS_P1WCS_FAULT=u1_f1_cd_relative) → 必 FAIL。
//   3) 注入点 B (argv): fork + execve(/proc/self/exe, ["p1wcs_selfcheck",
//      "oracle", "--fault=o1_f2_sip_vs_oracle"], 无 env) → 必 FAIL
//      (覆盖 --fault= argv 通道)。
//   4) 正控: 注入名单为空的重入 (units, 无 env 无 fault) → 必 PASS
//      (排除"任何重入都失败"的假注入)。
// ASTROCS_P1WCS_SELFCHECK_FAULT_A/B 可覆盖阶段 2/3 注入名。
#include "p1wcs_test_main.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1wcs {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
}  // namespace p1wcs

namespace {

const p1wcs::TestGroup kGroups[] = {
    {"units", p1wcs::test_units},
    {"properties", p1wcs::test_properties},
    {"oracle", p1wcs::test_oracle},
    {"negative", p1wcs::test_negative},
};

// fork + execve 重入: 子进程以 argv 模式跑指定组 (env_extra 可空)。
// 返回子进程 rc (执行失败 → 127)。
int run_child(const std::string& group, const std::string& fault_arg,
              const std::string& fault_env) {
    std::vector<std::string> args;
    args.push_back("p1wcs_selfcheck");
    args.push_back(group);
    if (!fault_arg.empty()) args.push_back(fault_arg);

    std::vector<char*> child_argv;
    for (auto& a : args) child_argv.push_back(a.data());
    child_argv.push_back(nullptr);

    std::string env_pair;
    std::vector<char*> child_env;
    if (!fault_env.empty()) {
        env_pair = "ASTROCS_P1WCS_FAULT=" + fault_env;
        child_env.push_back(env_pair.data());
    }
    child_env.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("p1wcs selfcheck fork");
        return 127;
    }
    if (pid == 0) {
        execve("/proc/self/exe", child_argv.data(), child_env.data());
        _exit(127);  // execve 失败
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS
    {
        const int rc = p1wcs::test_units();
        if (rc != 0) {
            std::fprintf(stderr,
                         "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n",
                         rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
    }

    // 阶段 2: 注入点 A (env) → units 必 FAIL
    {
        const char* f = std::getenv("ASTROCS_P1WCS_SELFCHECK_FAULT_A");
        const std::string name = f ? f : "u1_f1_cd_relative";
        const int child_rc = run_child("units", "", name);
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: env fault-inject '%s' 后 units 仍 PASS — "
                         "恒 PASS 占位, 注入机制失效\n",
                         name.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase2: env fault-inject '%s' → child rc=%d "
                     "(必败验证通过)\n",
                     name.c_str(), child_rc);
    }

    // 阶段 3: 注入点 B (argv --fault=) → oracle 必 FAIL
    {
        const char* f = std::getenv("ASTROCS_P1WCS_SELFCHECK_FAULT_B");
        const std::string name = f ? f : "o1_f2_sip_vs_oracle";
        const int child_rc =
            run_child("oracle", "--fault=" + name, "");
        if (child_rc == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: argv fault-inject '%s' 后 oracle 仍 PASS — "
                         "argv 注入通道失效\n",
                         name.c_str());
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3: argv fault-inject '%s' → child rc=%d "
                     "(必败验证通过)\n",
                     name.c_str(), child_rc);
    }

    // 阶段 4: 正控重入 (无注入) → 必 PASS (排除假注入/假重入)
    {
        const int child_rc = run_child("units", "", "");
        if (child_rc != 0) {
            std::fprintf(stderr,
                         "SELFCHECK: 无注入重入 units FAIL (rc=%d) — 重入路径受损\n",
                         child_rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase4: reentry-without-fault PASS\n");
    }

    std::fprintf(stdout,
                 "P1WCS SELFCHECK PASS (baseline + env/argv fault-injection both verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 注入子进程模式: execve 重入 (argv[1] = 组名) → 走 run_all_groups
    // 真实执行器 (env + --fault= 双通道解析, registry 统一初始化)。
    // 不进入 run_selfcheck, 否则将 fork 递归 (资源耗尽)。
    if (argc >= 2 && argv[1][0] != '\0' && argv[1][0] != '-') {
        return p1wcs::run_all_groups(kGroups, 4, argc, argv);
    }
    return run_selfcheck();
}
