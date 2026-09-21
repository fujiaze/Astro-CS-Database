// P1-WCS-TEST · 组注册表 (单执行器)
#include "p1wcs_test_main.hpp"

namespace p1wcs {
int test_units();
int test_properties();
int test_oracle();
int test_negative();
int test_apbp();   // WCS-003: 布局扩展 + 迭代式反演冻结门
}  // namespace p1wcs

int main(int argc, char** argv) {
    static const p1wcs::TestGroup kGroups[] = {
        {"units", p1wcs::test_units},
        {"properties", p1wcs::test_properties},
        {"oracle", p1wcs::test_oracle},
        {"negative", p1wcs::test_negative},
        {"apbp", p1wcs::test_apbp},
    };
    return p1wcs::run_all_groups(kGroups, 5, argc, argv);
}
