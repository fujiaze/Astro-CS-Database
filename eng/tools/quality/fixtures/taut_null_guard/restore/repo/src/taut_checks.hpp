// 夹具 · 恢复面: 同一文件按处置口径改写 —— 守卫改为运行期可触发的故障名前置条件。
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

// 故障名前置条件: 判定的是**字符串内容**（运行期可失败的性质），
// 而不是「字面量地址是否为空」这种编译期恒真的比较。
inline bool fault_name_usable(const char* name) {
    return name != nullptr && name[0] != '\0';
}
}  // namespace fixtaut

#define FIXTAUT_CHECK(cs, cond, faultname)                                   \
    do {                                                                     \
        const char* const fn_ = (faultname);                                 \
        if (!fixtaut::fault_name_usable(fn_)) {                              \
            std::fprintf(stderr, "CHECK illegal faultname %s:%d\n",         \
                         __FILE__, __LINE__);                                \
            ++(cs).failures;                                                 \
        } else if ((cs).fault_reported) {                                    \
            ++(cs).failures;                                                 \
        } else if (fixtaut::FaultRegistry::instance().injected(fn_)) {       \
            (cs).fault_reported = true;                                      \
            ++(cs).failures;                                                 \
        } else if (!(cond)) {                                                \
            ++(cs).failures;                                                 \
        }                                                                    \
    } while (0)
#endif
