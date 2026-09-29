// 夹具 · 恢复面: 改写后必须重新全绿；并补上「失败路径可被触发」的可执行负例。
#include "taut_checks.hpp"
#include <cstddef>

static int g_slot = 0;
static int helper_fn() { return 0; }

int restored_macro_param_guard() {
    fixtaut::CheckState cs;
    FIXTAUT_CHECK(cs, true, "taut_case_a");     // 正常调用: 故障名合法，行为不变
    FIXTAUT_CHECK(cs, true, "taut_case_b");
    // 负例: 故障名非法 ⇒ 失败路径被真实走到（原先被恒真守卫静默跳过）
    FIXTAUT_CHECK(cs, true, nullptr);
    return cs.failures;
}

int restored_literal_guard() {
    const char* maybe_null = std::getenv("FIXTURE_SLOT");
    if (maybe_null == nullptr) return 1;        // 合法: 判的是运行期可空值
    if (g_slot == 0) return 2;                  // 合法: 判的是运行期值
    return helper_fn() == 0 ? 0 : 3;
}
