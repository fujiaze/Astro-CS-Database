// P1-NOISE-TEST · main 入口 (单执行器: 转发 core 组注册表)
//
// 组注册表 (units/properties/oracle/negative/scale_law/fill) 位于
// p1noise_tests_core.cpp (test_* 组函数为 TU 内部符号); main 仅转发。
// selfcheck 重入亦经由 p1noise_run_core_groups (见 p1noise_tests_selfcheck.cpp)。
#include "p1noise_test_main.hpp"

int p1noise_run_core_groups(int argc, char** argv);

int main(int argc, char** argv) {
    return p1noise_run_core_groups(argc, argv);
}
