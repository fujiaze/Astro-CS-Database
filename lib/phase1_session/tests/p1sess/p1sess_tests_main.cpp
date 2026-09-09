// P1-SESSION-TEST · core 组 main: 组注册与分发
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 组单跑: ./p1sess_tests units|properties|negative|performance
// 故障注入: ASTROCS_P1SESS_FAULT=<name>[,<name>...] (selfcheck 覆盖)
#include "p1sess_test_main.hpp"

namespace p1sess {
int run_units();
int run_properties();
int run_negative();
int run_performance();
}  // namespace p1sess

int main(int argc, char** argv) {
    static const p1sess::TestGroup groups[] = {
        {"units", p1sess::run_units},
        {"properties", p1sess::run_properties},
        {"negative", p1sess::run_negative},
        {"performance", p1sess::run_performance},
    };
    return p1sess::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
