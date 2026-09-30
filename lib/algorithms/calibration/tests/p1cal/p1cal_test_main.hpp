// P1-CAL-TEST · 测试执行器框架 (单执行器 + 单测组注册 + 故障注入)
//
// 单跑方式 (每个测试组 == 独立可执行 ctest 名, 二进制内按 --group 单跑):
//   ./p1cal_tests units|properties|negative|cosmetic|perf
//   ./p1cal_tests all
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ACSD_P1CAL_FAULT=<regname>[,<regname>...]
//   每个注册的 fault 使对应 CHECK 组在报告阶段确定性翻转 → 二进制 rc=1,
//   输出 "FAULT-INJECT <name>" 行。可注入 registry 见 p1cal_faults.hpp。
//   例: ACSD_P1CAL_FAULT=darkopt_actual_k ./p1cal_tests negative
#ifndef P1CAL_TEST_MAIN_HPP
#define P1CAL_TEST_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace p1cal {

struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    // 由 main() 启动时从 ACSD_P1CAL_FAULT 初始化
    std::vector<std::string> active;

    bool injected(const char* name) const {
        if (name == nullptr) return false;
        for (const auto& s : active)
            if (s == name) return true;
        return false;
    }
};

struct CheckState {
    int failures = 0;
    bool fault_reported = false;
    // 故障注入报告: 每个 fault 名只在首个 CHECK 触发一次
    void note_injected(const char* name) {
        if (fault_reported) return;
        std::fprintf(stderr, "FAULT-INJECT %s (deterministic failure injection)\n", name);
        fault_reported = true;
    }
};

// 故障名的可注入性判定（**运行期**）。
//
// 旧写法是 (faultname) != nullptr: 本仓全部调用点都传字符串字面量, 字面量地址
// 永不为 null ⇒ 该比较编译期恒真, 「故障名不可用」这条失败路径**从未被执行过**
//（恒真门没有证据资格, AGENTS.md §5）。改为对**故障名内容**的运行期判定, 并把
// 注入与否交给注册表实际查询（不是对字面量地址的常量比较）:
//   * nullptr = 明确的「本断言不属于任何注入点」约定（注入分支不生效, 断言照常求值）;
//   * 空串名 = 注入注册错误（永远匹配不到任何注入名）⇒ **判失败**; 该路径可被触发,
//     证据见本套件 selfcheck 的 guard-path 相。
inline bool fault_name_usable(const char* name) {
    return name != nullptr && name[0] != '\0';
}

#define P1CAL_CHECK(cs, cond, faultname)                                   \
    do {                                                                   \
        const char* const fn_ = (faultname);                              \
        if (fn_ != nullptr && !p1cal::fault_name_usable(fn_)) {           \
            std::fprintf(stderr, "CHECK failed %s:%d: empty fault name\n",\
                         __FILE__, __LINE__);                             \
            ++(cs).failures;                                              \
        } else if (p1cal::FaultRegistry::instance().injected(fn_)) {      \
            (cs).note_injected(fn_);                                      \
            ++(cs).failures;                                               \
        } else if ((cs).fault_reported && p1cal::fault_name_usable(fn_)) {\
            /* 已注入本组: 后续具名 CHECK 全部计为失败 (测试必败) */                         \
            ++(cs).failures;                                              \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",               \
                         __FILE__, __LINE__, #cond);                       \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

#define P1CAL_CHECK_EQ(cs, got, want)                                      \
    do {                                                                   \
        const auto g_ = (got), w_ = (want);                                \
        if (!(g_ == w_)) {                                                 \
            std::fprintf(stderr, "CHECK failed %s:%d: got=%lld want=%lld (%s)\n", \
                         __FILE__, __LINE__,                             \
                         static_cast<long long>(g_),                     \
                         static_cast<long long>(w_), #got);              \
            ++(cs).failures;                                             \
        }                                                                  \
    } while (0)

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

// 全链 main 框架: 解析 --group → 跑组 → rc
inline int run_all_groups(const p1cal::TestGroup* groups, std::size_t n, int argc, char** argv) {
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--", 0) != 0) group = a;
    }
    // ACSD_P1CAL_FAULT: 逗号分隔故障注入名单
    if (const char* f = std::getenv("ACSD_P1CAL_FAULT")) {
        std::string s = f;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t comma = s.find(',', pos);
            const std::string tok = s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
            if (!tok.empty()) p1cal::FaultRegistry::instance().active.push_back(tok);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1cal] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1cal] group %s FAIL rc=%d\n", groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1cal] group %s PASS\n", groups[i].name);
        }
        std::fflush(stdout);
    }
    if (total_fail == 0) {
        std::fprintf(stdout, "P1CAL TESTS PASS (group=%s)\n", group.c_str());
        return 0;
    }
    std::fprintf(stderr, "P1CAL TESTS FAIL (%d group(s) failed, group=%s)\n", total_fail, group.c_str());
    return 1;
}

}  // namespace p1cal

#endif  // P1CAL_TEST_MAIN_HPP
