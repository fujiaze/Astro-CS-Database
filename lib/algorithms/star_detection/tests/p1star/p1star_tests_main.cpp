// ============================================================================
// p1star_tests_main.cpp — P1-STAR-TEST core 执行器入口 (main 唯一性拆分 TU)
// ----------------------------------------------------------------------------
// 结构先例 P1-HIPS-TEST (commit c19b4a59 p1hips_tests_main.cpp) 同构: core
// 组可执行 p1star_tests 的 main 与组注册表独立成 TU, 使 p1star_selfcheck_test
// 共享 core TUs (units/properties/oracle/negative) 而不重复定义 main。
// argv 子进程模式: "oom-child <N>" 为 negative 组 OOM 注入探测入口
// (oom_probe_once fork+execv /proc/self/exe 重入, 依赖环境 LD_PRELOAD=
// sdet_oom_interposer.so 与 OOM_FAIL_AT=<N>)。
// ============================================================================
#include "star_detector.h"

#include <cstdio>
#include <cstring>

#include "p1star_fixtures.hpp"
#include "p1star_test_main.hpp"

namespace p1star {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
int oom_child_main(const char* n_str);  // p1star_tests_core.cpp (negative 组)
}  // namespace p1star

int main(int argc, char** argv) {
    if (argc > 2 && std::strcmp(argv[1], "oom-child") == 0)
        return p1star::oom_child_main(argv[2]);
    static const p1star::TestGroup groups[] = {
        {"units", p1star::test_units},
        {"properties", p1star::test_properties},
        {"oracle", p1star::test_oracle},
        {"negative", p1star::test_negative},
    };
    return p1star::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
