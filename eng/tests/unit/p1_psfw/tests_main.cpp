/* tests_main.cpp - 执行器: 按注册组名单跑或 all; 汇总 rc。 */
#include "p1psfw_test_main.hpp"

int main(int argc, char** argv) {
    ::p1psfw_test::init_faults();
    const std::string mode = (argc > 1) ? argv[1] : "all";
    int groups_run = 0;
    if (mode == "list") {
        for (const auto& g : ::p1psfw_test::registry())
            std::printf("%s\n", g.name.c_str());
        return 0;
    }
    for (const auto& g : ::p1psfw_test::registry()) {
        if (mode != "all" && mode != g.name) continue;
        ++groups_run;
        std::printf("== group %s ==\n", g.name.c_str());
        g.fn(mode);
    }
    if (groups_run == 0) {
        std::fprintf(stderr, "unknown group: %s\n", mode.c_str());
        return 2;
    }
    std::printf("checks: pass=%d fail=%d\n", ::p1psfw_test::g_pass, ::p1psfw_test::g_fail);
    return ::p1psfw_test::g_fail == 0 ? 0 : 1;
}
