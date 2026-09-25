// P1-PHOT-TEST · main (组注册)
//
// 组表: units / properties / oracle / negative (core 组单执行器);
// performance 与 selfcheck 为独立可执行 (时长敏感 + fork 重入隔离)。
// 单跑: ./p1phot_tests units   |  ctest -R p1phot_units
// 全跑: ./p1phot_tests all
#include "p1phot_test_main.hpp"

namespace p1phot {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
int test_determinism();
int test_spatial();   // PHOT-MXY-01: 低阶乘性空间增益 m(x,y)
}  // namespace p1phot

int main(int argc, char** argv) {
    const p1phot::TestGroup groups[] = {
        {"units", p1phot::test_units},
        {"properties", p1phot::test_properties},
        {"oracle", p1phot::test_oracle},
        {"negative", p1phot::test_negative},
        // 到达顺序不变性 + 线程数扫描 (1/2/4/8) + 负例注入。判据缺口见
        // p1phot_tests_determinism.cpp 文件头: §13.4 I5 只扫线程数、样本序恒定。
        {"determinism", p1phot::test_determinism},
        // PHOT-MXY-01: m(x,y) 空间增益（正例 + 四条能红能绿负例见 p1phot_tests_spatial.cpp）
        {"spatial", p1phot::test_spatial},
    };
    return p1phot::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]),
                                  argc, argv);
}
