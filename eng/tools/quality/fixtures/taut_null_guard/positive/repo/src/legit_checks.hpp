// 夹具 · 正例面: 断言宏的故障名守卫在**调用点存在可空实参** ⇒ 合法判空。
#ifndef FIXT_POS_H
#define FIXT_POS_H
#include <cstdio>
#include <string>
#include <vector>
namespace fixpos {
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
}  // namespace fixpos

#define FIXPOS_CHECK(cs, cond, faultname)                                   \
    do {                                                                    \
        if ((cs).fault_reported && (faultname) != nullptr) {                \
            ++(cs).failures;                                                \
        } else if (fixpos::FaultRegistry::instance().injected(faultname)) { \
            (cs).fault_reported = true;                                     \
            ++(cs).failures;                                                \
        } else if (!(cond)) {                                               \
            ++(cs).failures;                                                \
        }                                                                   \
    } while (0)
#endif
