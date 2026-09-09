// AIO-001 · 故障注入必败自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 模式对齐: lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp。
// 验收 (模板): "故障注入能让测试失败" + "不得写永远 PASS 的占位"。三阶段:
//   1) 基线: 无注入跑 units+negative → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入 A: 子进程 ASTROCS_AIO_FAULT=n1_hash_value_flip 重跑 units → 必 FAIL。
//   3) 注入 B: 子进程 ASTROCS_AIO_FAULT=n2_verify_mismatch_shortcut 重跑
//      negative → 必 FAIL; 注入 C: n3_file_size_skip 重跑 negative → 必 FAIL。
// 注入名与 aio_abi.cpp FaultRegistry 注册处对齐;
// ASTROCS_AIO_SELFCHECK_FAULT 可覆盖阶段 2 注入名。
#include "aio_abi_test_main.hpp"

#include <sys/wait.h>
#include <unistd.h>

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
    const std::string fault_env = "ASTROCS_AIO_FAULT=" + fault_name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "aio_abi_selfcheck";
    char arg1[32];
    std::snprintf(arg1, sizeof(arg1), "%s", group);
    char* child_argv[] = {arg0, arg1, nullptr};
    char* child_env[] = {fbuf.data(), nullptr};

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
        const char* fault = std::getenv("ASTROCS_AIO_SELFCHECK_FAULT");
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
