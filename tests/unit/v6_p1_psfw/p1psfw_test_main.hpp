/* p1psfw_test_main.hpp - IMPL-P1-PSFW-001 测试执行器框架
 * 单跑: ./v6_p1_psfw_tests <group>|all
 * 故障注入: ASTROCS_P1PSFW_FAULT=<name>[,<name>...] 使对应 CHECKF 确定性翻转 -> rc=1
 *   (selfcheck ctest 以 WILL_FAIL 断言注入相必败, 证明测试非恒真) */
#ifndef P1PSFW_TEST_MAIN_HPP
#define P1PSFW_TEST_MAIN_HPP

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1psfw_test {

inline int g_pass = 0;
inline int g_fail = 0;
inline std::vector<std::string> g_faults;

inline bool fault_active(const char* name) {
    for (const auto& s : g_faults)
        if (s == name) return true;
    return false;
}

inline void init_faults() {
    const char* e = std::getenv("ASTROCS_P1PSFW_FAULT");
    if (e == nullptr) return;
    const std::string s = e;
    std::size_t p = 0;
    while (p <= s.size()) {
        const std::size_t c = s.find(',', p);
        const std::string tok = s.substr(p, c == std::string::npos ? std::string::npos : c - p);
        if (!tok.empty()) g_faults.push_back(tok);
        if (c == std::string::npos) break;
        p = c + 1;
    }
}

inline void check_impl(const char* expr, const char* file, int line, bool cond) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
    }
}

inline void check_near_impl(const char* expr, const char* file, int line,
                            double got, double want, double rtol, double atol) {
    const double diff = std::fabs(got - want);
    const bool ok = diff <= atol + rtol * std::fabs(want);
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr, "FAIL %s:%d: %s (got=%.17g want=%.17g rtol=%.3g atol=%.3g)\n",
                     file, line, expr, got, want, rtol, atol);
    }
}

using GroupFn = void (*)(const std::string& mode);
struct Group { std::string name; GroupFn fn; };
inline std::vector<Group>& registry() {
    static std::vector<Group> r;
    return r;
}
struct Reg {
    Reg(const char* n, GroupFn f) { registry().push_back(Group{n, f}); }
};

}  /* namespace p1psfw_test */

#define P1_CHECK(cond) \
    ::p1psfw_test::check_impl(#cond, __FILE__, __LINE__, (cond))

/* 注入故障名 fault 时翻转断言 -> 测试必败 */
#define P1_CHECKF(fault, cond) \
    ::p1psfw_test::check_impl(#cond, __FILE__, __LINE__, \
        ::p1psfw_test::fault_active(fault) ? !(cond) : (cond))

#define P1_CHECK_NEAR(got, want, rtol) \
    ::p1psfw_test::check_near_impl(#got " ~= " #want, __FILE__, __LINE__, (got), (want), (rtol), 0.0)

#define P1_CHECK_NEAR_ABS(got, want, atol) \
    ::p1psfw_test::check_near_impl(#got " ~= " #want, __FILE__, __LINE__, (got), (want), 0.0, (atol))

#define P1PSFW_REGISTER(name)                                                       \
    static void p1psfw_group_##name(const std::string& mode);                       \
    static ::p1psfw_test::Reg p1psfw_reg_##name(#name, &p1psfw_group_##name);       \
    static void p1psfw_group_##name(const std::string& mode)

#endif /* P1PSFW_TEST_MAIN_HPP */
