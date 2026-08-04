// lib/acr/tests/classic/classic_main.cpp — classic 测试套件入口
// 显式 runtime_init/shutdown（避免依赖 gtest_main，便于扩展）
// 引用 run_eXX() 强制链接器从静态库中提取 TEST() 注册的 object 文件
#include <gtest/gtest.h>

#include <vector>

#include "astro/compute/acr.hpp"
#include "classic_common.hpp"
#include "../fault/exit_safe.hpp"

// run_eXX 定义在 acr_classic_experiments 静态库，extern "C" 避免名称修饰
extern "C" std::vector<astro::compute::classic::CaseResult> run_e01();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e02();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e03();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e04();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e05();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e06();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e07();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e08();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e09();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e10();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e11();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e12();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e13();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e14();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e15();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e16();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e17();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e18();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e19();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e20();
extern "C" std::vector<astro::compute::classic::CaseResult> run_e21();

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    astro::compute::runtime_init();

    // 引用 run_eXX 函数指针，强制链接器提取 object 文件（含 TEST() 静态注册器）
    // 不实际调用，仅防止 dead-stripping
    [[maybe_unused]] volatile auto p01 = &run_e01;
    [[maybe_unused]] volatile auto p02 = &run_e02;
    [[maybe_unused]] volatile auto p03 = &run_e03;
    [[maybe_unused]] volatile auto p04 = &run_e04;
    [[maybe_unused]] volatile auto p05 = &run_e05;
    [[maybe_unused]] volatile auto p06 = &run_e06;
    [[maybe_unused]] volatile auto p07 = &run_e07;
    [[maybe_unused]] volatile auto p08 = &run_e08;
    [[maybe_unused]] volatile auto p09 = &run_e09;
    [[maybe_unused]] volatile auto p10 = &run_e10;
    [[maybe_unused]] volatile auto p11 = &run_e11;
    [[maybe_unused]] volatile auto p12 = &run_e12;
    [[maybe_unused]] volatile auto p13 = &run_e13;
    [[maybe_unused]] volatile auto p14 = &run_e14;
    [[maybe_unused]] volatile auto p15 = &run_e15;
    [[maybe_unused]] volatile auto p16 = &run_e16;
    [[maybe_unused]] volatile auto p17 = &run_e17;
    [[maybe_unused]] volatile auto p18 = &run_e18;
    [[maybe_unused]] volatile auto p19 = &run_e19;
    [[maybe_unused]] volatile auto p20 = &run_e20;
    [[maybe_unused]] volatile auto p21 = &run_e21;

    int result = RUN_ALL_TESTS();
    astro::compute::test::exit_after_tests(result);
}
