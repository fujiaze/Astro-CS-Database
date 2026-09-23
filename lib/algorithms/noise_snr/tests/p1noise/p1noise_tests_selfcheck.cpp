// P1-NOISE-TEST · 故障注入自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS
// 的占位"。本可执行两阶段:
//   1) 基线: 无注入跑 units 组 → 必 PASS (排除恒 FAIL 侧)。
//   2) 注入: 子进程以 ASTROCS_P1NOISE_FAULT=<name> 重跑本二进制 → 必 FAIL
//      (排除恒 PASS 侧), stderr 含 FAULT-INJECT 行。
// 注入名与 faultname 注册处 (p1noise_tests_core.cpp P1NOISE_CHECK 第三参)
// 对齐: a1_sigma_rtol / a2_oracle_bitwise / b1_plane_ls_rtol /
// e1_gain_bitwise / i4_determinism / i1_ivar_reciprocal / i2_var_ge_floor /
// i3_degenerate_zero_ivar / i3_unavailable_zero_pair / o1_oracle_stats_bitwise /
// o2_mask_radius_monotone / o2_mask_radius_monotone_F / o2_mask_radius_monotone_fwhm
// / o3_gain_model_inert / o3_poisson_cross_rtol / o4_f64_parity /
// n3_all_mask_degenerate / n4_nonfinite_filtered / n5_nan_star_skipped /
// n6_fill_null_rc3 / n7_plane_pred_unavailable / n7_plane_pred_available_bitwise /
// f1_scale_law_bitwise /
// f2_scale_roundtrip / f4_scale_nan_passthrough / g1_const_fill /
// g2_plane_fill_refbitwise / g3_nullable_outputs / g4_nospatial_const / ...
//   (P1NOISE_SELFCHECK_FAULT 可覆盖注入名; 默认 a1_sigma_rtol)
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int p1noise_run_core_groups(int argc, char** argv);

namespace {

int run_selfcheck() {
    // 阶段 1: 基线 units 组必 PASS
    {
        char arg0[] = "p1noise_selfcheck";
        char arg1[] = "units";
        char* argv[] = {arg0, arg1, nullptr};
        const int rc = p1noise_run_core_groups(2, argv);
        if (rc != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline units FAIL (rc=%d) — 恒 FAIL 侧不通过\n", rc);
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase1: baseline units PASS (非恒FAIL)\n");
    }

    // 阶段 2: 子进程注入 → 必 FAIL
    const char* fault = std::getenv("P1NOISE_SELFCHECK_FAULT");
    const std::string name = fault ? fault : "a1_sigma_rtol";
    std::string fault_env = "ASTROCS_P1NOISE_FAULT=" + name;
    std::vector<char> fbuf(fault_env.begin(), fault_env.end());
    fbuf.push_back('\0');

    char arg0[] = "p1noise_selfcheck";
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

    // 阶段 3: negative 组基线必 PASS（排除恒 FAIL 侧），再注入 n7 判据必 FAIL。
    // 目的: 证明「平面预测 ≤ 0 ⇒ variance=0 ∧ ivar=0」这条判据**能红** ——
    //   注入后 negative 组必须失败，否则该判据是恒真门（无证据资格）。
    {
        char arg0[] = "p1noise_selfcheck";
        char arg1[] = "negative";
        char* argv[] = {arg0, arg1, nullptr};
        if (p1noise_run_core_groups(2, argv) != 0) {
            std::fprintf(stderr, "SELFCHECK: baseline negative FAIL — 恒 FAIL 侧不通过\n");
            return 1;
        }
        std::fprintf(stdout, "SELFCHECK phase3a: baseline negative PASS (非恒FAIL)\n");

        const std::string n7fault = "ASTROCS_P1NOISE_FAULT=n7_plane_pred_unavailable";
        std::vector<char> n7buf(n7fault.begin(), n7fault.end());
        n7buf.push_back('\0');
        char* n7_env[] = {n7buf.data(), nullptr};
        char n7a0[] = "p1noise_selfcheck";
        char n7a1[] = "negative";
        char* n7_argv[] = {n7a0, n7a1, nullptr};
        const pid_t pid2 = fork();
        if (pid2 < 0) {
            std::perror("fork");
            return 1;
        }
        if (pid2 == 0) {
            execve("/proc/self/exe", n7_argv, n7_env);
            _exit(127);
        }
        int st2 = 0;
        waitpid(pid2, &st2, 0);
        const int rc2 = WIFEXITED(st2) ? WEXITSTATUS(st2) : -1;
        if (rc2 == 0) {
            std::fprintf(stderr,
                         "SELFCHECK: fault-inject 'n7_plane_pred_unavailable' 后 negative 仍 PASS"
                         " — 该判据恒真, 无证据资格\n");
            return 1;
        }
        std::fprintf(stdout,
                     "SELFCHECK phase3b: fault-inject 'n7_plane_pred_unavailable' → child rc=%d"
                     " (n7 判据能红验证通过)\n", rc2);
    }

    std::fprintf(stdout, "P1NOISE SELFCHECK PASS (baseline + fault-injection both verified)\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // 带注入环境重入: 子进程直接跑指定组 (FaultRegistry 已由框架装载)
    if (std::getenv("ASTROCS_P1NOISE_FAULT") != nullptr && argc >= 2) {
        return p1noise_run_core_groups(argc, argv);
    }
    return run_selfcheck();
}
