// P1-HIPS-TEST · main 入口 (单执行器: 四组注册表)
//
// 单跑方式 (对齐 p1cos/p1drz 先例):
//   ./p1hips_tests units|properties|oracle|negative
//   ./p1hips_tests all
// 故障注入: ASTROCS_P1HIPS_FAULT=<name>[,<name>...] → rc=1 + FAULT-INJECT 行
#include "p1hips_test_main.hpp"

namespace p1hips {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
}  // namespace p1hips

int main(int argc, char** argv) {
    const p1hips::TestGroup groups[] = {
        {"units", p1hips::test_units},
        {"properties", p1hips::test_properties},
        {"oracle", p1hips::test_oracle},
        {"negative", p1hips::test_negative},
    };
    return p1hips::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]),
                                  argc, argv);
}
