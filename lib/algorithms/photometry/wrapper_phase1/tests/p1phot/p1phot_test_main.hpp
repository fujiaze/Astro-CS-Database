// P1-PHOT-TEST · 测试框架 (CheckState / FaultRegistry / 分组运行 / 自检 fork)
//
// 模板纪律: 负面测试必须真实失败路径 + 故障注入自检 (注入必须致失败);
// 禁止永真占位; 断言失败即记录并计 FAIL, 最终退出码非 0。
// 框架形态沿用 p1cal 先例 (namespace p1phot, env ASTROCS_P1PHOT_FAULT)。
#ifndef P1PHOT_TEST_MAIN_HPP
#define P1PHOT_TEST_MAIN_HPP

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace p1phot {

// ---------------------------------------------------------------------------
// 故障注册表 (自检注入点): 环境变量 ASTROCS_P1PHOT_FAULT=<name> 命中时,
// 测试主体在注册点主动 abort — fork 子进程必须因此非 0 退出, 否则自检 FAIL
// ---------------------------------------------------------------------------
struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    void register_fault(const std::string& name) { faults_.insert({name, false}); }
    // 返回 true 且置位: 该故障已被环境激活 (主体内调用点应在检查后 abort)
    bool armed(const std::string& name) const {
        auto it = faults_.find(name);
        if (it == faults_.end()) return false;
        it->second = true;
        return armed_now_;
    }
    static std::string active_fault() {
        const char* e = std::getenv("ASTROCS_P1PHOT_FAULT");
        return e ? std::string(e) : std::string();
    }
    // 自检覆盖 (默认与 ASTROCS_P1PHOT_FAULT 同名; 子进程 execve 用)
    static std::string selfcheck_fault_env() {
        const char* e = std::getenv("P1PHOT_SELFCHECK_FAULT");
        return e ? std::string(e) : active_fault();
    }
    std::map<std::string, bool> faults_;
    bool armed_now_ = !active_fault().empty();
};

#define P1PHOT_FAULT_POINT(name)                                  \
    do {                                                          \
        ::p1phot::FaultRegistry::instance().register_fault(name); \
        if (::p1phot::FaultRegistry::instance().armed(name)) {    \
            std::fprintf(stderr, "[p1phot][fault-injected] %s\n", \
                         name);                                   \
            std::fflush(stderr);                                  \
            std::abort();                                         \
        }                                                         \
    } while (0)

// ---------------------------------------------------------------------------
// CheckState: 断言状态 (沿用 p1cal 形态)
// ---------------------------------------------------------------------------
struct CheckState {
    int total = 0, failed = 0;
    std::vector<std::string> failures;
};

#define P1PHOT_CHECK(cs, cond, name)                                        \
    do {                                                                    \
        ++(cs).total;                                                       \
        if (!(cond)) {                                                      \
            ++(cs).failed;                                                  \
            char _buf[256];                                                 \
            std::snprintf(_buf, sizeof(_buf), "%s (line %d)", name,         \
                          __LINE__);                                        \
            (cs).failures.push_back(_buf);                                  \
            std::fprintf(stderr, "[p1phot][FAIL] %s (line %d)\n", name,     \
                         __LINE__);                                         \
        } else {                                                            \
            std::fprintf(stderr, "[p1phot][ok] %s\n", name);                \
        }                                                                   \
        std::fflush(stderr);                                                \
    } while (0)

#define P1PHOT_CHECK_NEAR(cs, got, want, tol, name)                          \
    do {                                                                     \
        double _g = (got), _w = (want), _t = (tol);                          \
        double _d = _g > _w ? _g - _w : _w - _g;                             \
        P1PHOT_CHECK(cs, _d <= _t, name);                                    \
        if (_d > _t)                                                         \
            std::fprintf(stderr, "    got=%.17g want=%.17g diff=%.3g tol=%.3g\n", \
                         _g, _w, _d, _t);                                    \
    } while (0)

// ---------------------------------------------------------------------------
// 分组驱动: 各组实现注册为函数, main 按 --group=<g> 挑选或全跑
// ---------------------------------------------------------------------------
using GroupFn = void (*)(CheckState&);
struct GroupEntry { const char* name; GroupFn fn; };

inline std::vector<GroupEntry>& groups() {
    static std::vector<GroupEntry> g;
    return g;
}
struct GroupRegistrar {
    GroupRegistrar(const char* name, GroupFn fn) { groups().push_back({name, fn}); }
};

inline int run_all_groups(int argc, char** argv) {
    std::string want;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strncmp(a, "--group=", 8) == 0) want = a + 8;
    }
    CheckState cs;
    for (const GroupEntry& ge : groups()) {
        if (!want.empty() && want != ge.name) continue;
        std::fprintf(stderr, "[p1phot] === group %s ===\n", ge.name);
        CheckState local;
        ge.fn(local);
        cs.total += local.total;
        cs.failed += local.failed;
        for (const std::string& f : local.failures)
            cs.failures.push_back(std::string(ge.name) + "::" + f);
    }
    std::fprintf(stderr, "[p1phot] checks=%d failed=%d\n", cs.total, cs.failed);
    std::fflush(stderr);
    return cs.failed == 0 ? 0 : 1;
}

}  // namespace p1phot

#endif  // P1PHOT_TEST_MAIN_HPP
