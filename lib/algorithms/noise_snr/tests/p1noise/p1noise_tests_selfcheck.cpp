// P1-NOISE-TEST · 故障注入自检 (selfcheck): 验证注入机制必败 + 基线对照
//
// 验收 (模板 <prefix>-TEST): "故障注入能让测试失败" + "不得写永远 PASS\n// 的占位"。本可执行两阶段:
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
#include "p1noise_test_main.hpp"

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

int p1noise_run_core_groups(int argc, char** argv);

namespace {

// guard-path 相 (TAUT-NULL-GUARD): 证明「故障名不可用」这条失败路径**可被触发**。
// 旧写法 (faultname) != nullptr 是对字面量地址的编译期恒真比较, 该路径从未被执行过
//（恒真门没有证据资格, AGENTS.md §5）。三态: 合法名⇒0 失败（行为与改写前一致）;
// 空串名⇒1 失败（此前不可达的路径）; nullptr 名⇒0 失败（「非注入点」约定仍然有效）。
static int check_guard_path() {
    {
        p1noise::CheckState cs;
        P1NOISE_CHECK(cs, 1 == 1, "guard_path_positive");
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL positive: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        p1noise::CheckState cs;
        P1NOISE_CHECK(cs, 1 == 1, "");
        if (cs.failures != 1 || cs.fault_reported) {
            std::fprintf(stderr,
                         "GUARD-PATH FAIL empty_faultname: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        std::fprintf(stdout, "GUARD-PATH empty_faultname reached (failures=1)\n");
    }

    {
        p1noise::CheckState cs;
        P1NOISE_CHECK(cs, 1 == 1, nullptr);
        if (cs.failures != 0) {
            std::fprintf(stderr, "GUARD-PATH FAIL null_faultname: failures=%d\n", cs.failures);
            return 1;
        }
    }
    {
        p1noise::FaultRegistry::instance().active.push_back("guard_path_live");
        p1noise::CheckState cs;
        P1NOISE_CHECK(cs, 1 == 1, "guard_path_live");
        if (cs.failures != 1 || !cs.fault_reported) {
            std::fprintf(stderr, "GUARD-PATH FAIL live injection: failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        p1noise::FaultRegistry::instance().active.pop_back();
    }
    std::fprintf(stdout, "GUARD-PATH PASS (empty name reachable + null convention kept)\n");
    return 0;
}
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

    #ifdef _WIN32
    // WIN-PORT: Windows 无 fork/execve/waitpid。_spawnve(_P_WAIT, ...) 给出等价语义：以同一
    // 可执行文件（_pgmptr）与同一 argv/env 起子进程并阻塞等待，直接取子进程退出码 ——
    // 与 POSIX 支路「注入环境重跑 + 等子进程 + 取退出码」同一判据面。
    const intptr_t rc_win = _spawnve(_P_WAIT, _pgmptr, child_argv, child_env);
    const int child_rc = rc_win < 0 ? 127 : static_cast<int>(rc_win);
    #else
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
    #endif
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
        #ifdef _WIN32
        // WIN-PORT: Windows 无 fork/execve/waitpid。_spawnve(_P_WAIT, ...) 给出等价语义：以同一
        // 可执行文件（_pgmptr）与同一 argv/env 起子进程并阻塞等待，直接取子进程退出码 ——
        // 与 POSIX 支路「注入环境重跑 + 等子进程 + 取退出码」同一判据面。
        const intptr_t rc_win = _spawnve(_P_WAIT, _pgmptr, n7_argv, n7_env);
        const int rc2 = rc_win < 0 ? 127 : static_cast<int>(rc_win);
        #else
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
        #endif
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

    // 阶段 4/5: adaptive 组（SCI-VAR-ADAPT-01 §5d）双向必败。
    // 目的: 证明自校准判据的两条断言**都能红** ——
    //   4) 「纯噪声 ⇒ 不剔任何 patch」被注入后 adaptive 必失败（判据非恒真）;
    //   5) 「结构污染 ⇒ 必剔且改善」被注入后 adaptive 必失败（备择侧非空断言）。
    {
        const char* faults[2] = {"a1_no_reject_pure_noise", "a2_structure_rejected"};
        for (int fi = 0; fi < 2; ++fi) {
            char a0[] = "p1noise_selfcheck";
            char a1[] = "adaptive";
            char* av[] = {a0, a1, nullptr};
            if (p1noise_run_core_groups(2, av) != 0) {
                std::fprintf(stderr, "SELFCHECK: baseline adaptive FAIL — 恒 FAIL 侧不通过\n");
                return 1;
            }
            std::string envs = std::string("ASTROCS_P1NOISE_FAULT=") + faults[fi];
            std::vector<char> buf(envs.begin(), envs.end());
            buf.push_back('\0');
            char* env[] = {buf.data(), nullptr};
            #ifdef _WIN32
            // WIN-PORT: Windows 无 fork/execve/waitpid。_spawnve(_P_WAIT, ...) 给出等价语义：以同一
            // 可执行文件（_pgmptr）与同一 argv/env 起子进程并阻塞等待，直接取子进程退出码 ——
            // 与 POSIX 支路「注入环境重跑 + 等子进程 + 取退出码」同一判据面。
            const intptr_t rc_win = _spawnve(_P_WAIT, _pgmptr, av, env);
            const int rc3 = rc_win < 0 ? 127 : static_cast<int>(rc_win);
            #else
            const pid_t pid3 = fork();
            if (pid3 < 0) {
                std::perror("fork");
                return 1;
            }
            if (pid3 == 0) {
                execve("/proc/self/exe", av, env);
                _exit(127);
            }
            int st3 = 0;
            waitpid(pid3, &st3, 0);
            const int rc3 = WIFEXITED(st3) ? WEXITSTATUS(st3) : -1;
            #endif
            if (rc3 == 0) {
                std::fprintf(stderr,
                             "SELFCHECK: fault-inject '%s' 后 adaptive 仍 PASS — 该判据恒真,"
                             " 无证据资格\n", faults[fi]);
                return 1;
            }
            std::fprintf(stdout,
                         "SELFCHECK phase4/%d: baseline adaptive PASS + fault-inject '%s'"
                         " → child rc=%d (判据能红验证通过)\n", fi + 1, faults[fi], rc3);
        }
    }

    // guard-path 相: 失败路径可被触发的证据（TAUT-NULL-GUARD 判据要求）
    {
        const int guard_rc = check_guard_path();
        if (guard_rc != 0) {
            std::fprintf(stderr, "GUARD-PATH phase FAIL (rc=%d)\n", guard_rc);
            return 1;
        }
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
