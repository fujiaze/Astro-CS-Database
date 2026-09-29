// 夹具 · 负例面: 四种可静态定值的空指针守卫形态，判据必须逐条判红并给出行号。
#include "taut_checks.hpp"
#include <cstddef>

static int g_slot = 0;
static int helper_fn() { return 0; }

int taut_macro_param_guard() {
    fixtaut::CheckState cs;
    FIXTAUT_CHECK(cs, true, "taut_case_a");     // 实参全是字面量 ⇒ 宏形参守卫恒真
    FIXTAUT_CHECK(cs, true, "taut_case_b");
    return cs.failures;
}

int taut_literal_guard() {
    // TAUT:always_true_guard
    if ("literal-address" != nullptr) return 1;
    // TAUT:always_false_guard
    if (&g_slot == nullptr) return 2;
    // TAUT:always_true_guard
    if (&helper_fn != nullptr) return 3;
    // TAUT:always_false_guard
    if (nullptr == &g_slot) return 4;
    return 0;
}
