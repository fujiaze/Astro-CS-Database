// P1-DRZ-TEST · 测试执行器框架 (模板 <prefix>-TEST 单测组 + 故障注入)
//
// 单跑: ./p1drz_tests units|properties|oracle|negative
//       ./p1drz_tests all
// CTest: p1drz_units / p1drz_properties / p1drz_oracle / p1drz_negative
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ACSD_P1DRZ_FAULT=<regname>[,<regname>...] → 对应 CHECK 组确定性翻转
//   → rc=1 + stderr "FAULT-INJECT <name>"。注册表:
//     flux_closure, uniformity, impulse, nonfinite, determinism, variance,
//     negative_matrix, sip_active, adu_inverse
//   (p1drz_tests_selfcheck 以 fork+execve 注入跑注入相, 验证必败。)
#ifndef P1DRZ_TEST_MAIN_HPP
#define P1DRZ_TEST_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1drz {

inline int g_p1drz_pass = 0;
inline int g_p1drz_fail = 0;

inline void drz_check(bool cond, const char* msg) {
    if (cond) {
        ++g_p1drz_pass;
    } else {
        ++g_p1drz_fail;
        std::fprintf(stderr, "CHECK failed: %s\n", msg);
    }
}

// 故障注入 CHECK: faultname 非空且被注入时, 该断言确定性翻转 (测试必败)
struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
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
    void note_injected(const char* name) {
        if (fault_reported) return;
        std::fprintf(stderr,
                     "FAULT-INJECT %s (deterministic failure injection)\n",
                     name);
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

#define P1DRZ_CHECK(cs, cond, faultname)                                  \
    do {                                                                  \
        const char* const fn_ = (faultname);                             \
        if (fn_ != nullptr && !p1drz::fault_name_usable(fn_)) {          \
            std::fprintf(stderr, "CHECK failed %s:%d: empty fault name\n", \
                         __FILE__, __LINE__);                            \
            ++(cs).failures;                                             \
        } else if (p1drz::FaultRegistry::instance().injected(fn_)) {     \
        (cs).note_injected(fn_);                                        \
        ++(cs).failures;                                                \
        } else if (!(cond)) {                                             \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",              \
                         __FILE__, __LINE__, #cond);                      \
            ++(cs).failures;                                              \
        }                                                                 \
    } while (0)

// 带消息 CHECK (科学断言: 阈值引用 ALG/SCI 段落 + 实测值)
#define P1DRZ_CHECK_MSG(cs, cond, faultname, ...)                        \
    do {                                                                  \
        char _m[512];                                                     \
        std::snprintf(_m, sizeof(_m), __VA_ARGS__);                       \
        const char* const fn_ = (faultname);                            \
        if (fn_ != nullptr && !p1drz::fault_name_usable(fn_)) {         \
            std::fprintf(stderr, "CHECK failed %s:%d: empty fault name\n", \
                         __FILE__, __LINE__);                           \
            ++(cs).failures;                                            \
        } else if (p1drz::FaultRegistry::instance().injected(fn_)) {    \
        (cs).note_injected(fn_);                                       \
        ++(cs).failures;                                               \
        } else if (!(cond)) {                                             \
            std::fprintf(stderr, "CHECK failed %s:%d: %s | %s\n",         \
                         __FILE__, __LINE__, #cond, _m);                  \
            ++(cs).failures;                                              \
        }                                                                 \
    } while (0)

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

inline int run_all_groups(const p1drz::TestGroup* groups, std::size_t n,
                          int argc, char** argv) {
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--", 0) != 0) group = a;
    }
    if (const char* f = std::getenv("ACSD_P1DRZ_FAULT")) {
        // 同进程重入 (selfcheck 注入相) 防御: 先清空上次注册, 避免 active
        // 跨相累加 (execve 路径天然全新进程, 此行为纯防御)
        p1drz::FaultRegistry::instance().active.clear();
        std::string s = f;
        std::size_t pos = 0;
        while (pos < s.size()) {
            const std::size_t comma = s.find(',', pos);
            const std::string tok =
                s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
            if (!tok.empty())
                p1drz::FaultRegistry::instance().active.push_back(tok);
            if (comma == std::string::npos) break;
            pos = comma + 1;
        }
    }
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1drz] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1drz] group %s FAIL rc=%d\n",
                         groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1drz] group %s PASS\n", groups[i].name);
        }
    }
    std::fprintf(stdout, "[p1drz] == P1-DRZ-TEST: %d 通过, %d 失败 (组级) ==\n",
                 (int)n - total_fail, total_fail);
    return total_fail == 0 ? 0 : 1;
}

}  // namespace p1drz

#endif  // P1DRZ_TEST_MAIN_HPP
