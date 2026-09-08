// P1-DRZ-TEST · 故障注入必败自检 (模板 <prefix>-TEST 验收: 注入可失败)
//
// 两阶段 (fork + execve /proc/self/exe, 对齐 p1cal/p1cos selfcheck):
//   baseline: 无注入跑 core 全组 → 必 PASS (排除恒败侧)
//   injection: ASTROCS_P1DRZ_FAULT=<name> 重跑 → 必 FAIL + FAULT-INJECT 行
//              (排除恒 PASS 侧)
// 注册表 (p1drz_test_main.hpp): flux_closure, uniformity, impulse,
//   nonfinite, determinism, variance, negative_matrix, sip_active, adu_inverse
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int p1drz_run_core_groups(int argc, char** argv);

namespace {

struct InjectionCase {
    const char* name;
};

const InjectionCase k_injections[] = {
    {"flux_closure"}, {"uniformity"},  {"impulse"},
    {"nonfinite"},    {"determinism"}, {"variance"},
    {"negative_matrix"}, {"sip_active"}, {"adu_inverse"},
};

// 子进程跑 core 组 (注入环境由 execve 传入; execve env 全量替换, baseline
// 相以空 env 兜底清除注入变量)。setenv/unsetenv 为 POSIX (无 std:: 前缀)
int run_child_phase(const char* fault) {
    if (fault && ::setenv("ASTROCS_P1DRZ_FAULT", fault, 1) != 0)
        return 126;
    if (!fault) ::unsetenv("ASTROCS_P1DRZ_FAULT");
    char arg0[] = "p1drz_selfcheck_phase";
    char* argv2[3] = {arg0, const_cast<char*>("all"), nullptr};
    return p1drz_run_core_groups(2, argv2);
}

}  // namespace

int main(int argc, char** argv) {
    // -- 注入相: execve 子进程以 env=ASTROCS_P1DRZ_FAULT=<name> 重入, 直接
    //    跑 core 组 (**不得**走 run_child_phase(nullptr) — 那会 unsetenv
    //    清掉注入变量, 使注入失效)
    if (argc >= 1 && std::getenv("ASTROCS_P1DRZ_FAULT") != nullptr) {
        char arg0[] = "p1drz_selfcheck_phase";
        char* argv2[3] = {arg0, const_cast<char*>("all"), nullptr};
        return p1drz_run_core_groups(2, argv2);
    }

    // -- baseline: 无注入必 PASS --
    {
        const int rc = run_child_phase(nullptr);
        if (rc != 0) {
            std::fprintf(stderr,
                         "[p1drz-selfcheck] baseline FAIL rc=%d (恒败侧!)\n", rc);
            return 1;
        }
        std::fprintf(stdout, "[p1drz-selfcheck] baseline PASS\n");
    }

    // -- injection: 每个注册 fault 必 FAIL --
    int failed = 0;
    for (const auto& c : k_injections) {
        std::string fault_env = "ASTROCS_P1DRZ_FAULT=" + std::string(c.name);
        std::vector<char> env_buf(fault_env.begin(), fault_env.end());
        env_buf.push_back('\0');
        char* child_env[2] = {env_buf.data(), nullptr};

        char arg0[] = "p1drz_selfcheck_phase";
        char arg1[] = "all";
        char* child_argv[3] = {arg0, arg1, nullptr};

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
        const int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (rc == 0) {
            std::fprintf(stderr,
                         "[p1drz-selfcheck] injection %s 未使测试失败 (恒 PASS 侧!)\n",
                         c.name);
            ++failed;
        } else if (rc == 127) {
            std::fprintf(stderr, "[p1drz-selfcheck] injection %s execve 失败\n",
                         c.name);
            ++failed;
        } else {
            std::fprintf(stdout, "[p1drz-selfcheck] injection %s → rc=%d (必败 ✓)\n",
                         c.name, rc);
        }
    }

    std::fprintf(stdout,
                 "[p1drz-selfcheck] == P1-DRZ-TEST selfcheck: baseline PASS + %zu 注入"
                 ", %d 失败 ==\n",
                 sizeof(k_injections) / sizeof(k_injections[0]), failed);
    return failed == 0 ? 0 : 1;
}
