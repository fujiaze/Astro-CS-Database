// 夹具 · 负例面: 复刻历史形态 —— 守卫操作数是「全仓每个调用点都传字符串字面量」的宏形参。
#ifndef FIXT_TAUT_H
#define FIXT_TAUT_H
#include <cstdio>
#include <string>
#include <vector>
namespace fixtaut {
struct FaultRegistry {
    static FaultRegistry& instance() { static FaultRegistry r; return r; }
    std::vector<std::string> active;
    bool injected(const char* name) const {
        if (name == nullptr) return false;
        for (const auto& s : active)
            if (s == name) return true;
        return false;
    }
};
struct CheckState { int failures = 0; bool fault_reported = false; };
}  // namespace fixtaut

// TAUT:macro_fault_guard_without_negative_case
#define FIXTAUT_CHECK(cs, cond, faultname)                                   \
    do {                                                                     \
        /* TAUT:macro_param_always_nonnull */                                \
        if ((cs).fault_reported && (faultname) != nullptr) {                 \
            ++(cs).failures;                                                 \
        /* TAUT:macro_param_always_nonnull */                                \
        } else if (faultname != nullptr &&                                   \
                   fixtaut::FaultRegistry::instance().injected(faultname)) { \
            (cs).fault_reported = true;                                      \
            ++(cs).failures;                                                 \
        } else if (!(cond)) {                                                \
            ++(cs).failures;                                                 \
        }                                                                    \
    } while (0)
#endif
