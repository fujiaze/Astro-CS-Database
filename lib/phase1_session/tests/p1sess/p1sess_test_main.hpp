// P1-SESSION-TEST · 测试执行器框架 (单执行器 + 测试组注册 + FaultRegistry)
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION; 依赖
// P1-SESSION-DOC 闭环 2026-09-07)。合同锚: lib/phase1_session/README.md
// (API-P1-SESSION / DATA-P1-SESSION 冻结) §3 四段编排 + §5 错误取消传播;
// 工程控制 tasks/05_PHASE1_TASKS.md §3 P1-SESSION-TEST 验收:
//   "每声明节点恰好调用一次; 缺任何模块/错误 ABI/坏 artifact/取消不会写
//    完成 manifest; 仅 Phase1 run ID"。
//
// 单跑方式 (每个测试组 == 独立 ctest 名, 二进制内按位置参数单跑):
//   ./p1sess_tests units|properties|oracle|negative|performance
//   ./p1sess_tests all
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ASTROCS_P1SESS_FAULT=<regname>[,<regname>...]
//   每个注册的 fault 使对应 CHECK 组在报告阶段确定性翻转 → 二进制 rc=1,
//   输出 "FAULT-INJECT <name>" 行。selfcheck 可执行对齐 p1hips/p1cos 先例。
//
// 被测面: lib/phase1_session/p1_session.cpp 五导出 C API (API-P1-SESSION,
// p1_session_create/validate/run/inspect/destroy), 静态库 astrocs_phase1_session
// (根 CMakeLists.txt:448)。生产源只读, 本任务仅新增测试。
#ifndef P1SESS_TEST_MAIN_HPP
#define P1SESS_TEST_MAIN_HPP

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1sess {

// ---------------------------------------------------------------------------
// FaultRegistry (对齐 p1hips_test_main.hpp / p1cos_test_main.hpp 先例)
// ---------------------------------------------------------------------------
struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    // 由 main() 启动时从 ASTROCS_P1SESS_FAULT 初始化
    std::vector<std::string> active;

    bool injected(const char* name) const {
        for (const auto& s : active)
            if (s == name) return true;
        return false;
    }
};

// FaultRegistry env 初始化 (主执行器与 selfcheck 注入子进程共用; 对齐
// p1hips 先例 init_fault_registry_from_env —— selfcheck 子进程模式经
// execve 重入, 不经过 run_all_groups, 必须显式调用本函数, 否则注入名单
// 为空 → 注入静默失效)。
inline void init_fault_registry_from_env() {
    const char* f = std::getenv("ASTROCS_P1SESS_FAULT");
    if (!f) return;
    std::string s = f;
    std::size_t pos = 0;
    while (pos <= s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok =
            s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
        if (!tok.empty()) FaultRegistry::instance().active.push_back(tok);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

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

#define P1SESS_CHECK(cs, cond, faultname)                                  \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            /* 已注入本组: 后续 CHECK 全部计为失败 (测试必败) */            \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1sess::FaultRegistry::instance().injected(faultname)) { \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",               \
                         __FILE__, __LINE__, #cond);                       \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

#define P1SESS_CHECK_MSG(cs, cond, faultname, ...)                         \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1sess::FaultRegistry::instance().injected(faultname)) { \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s — ",              \
                         __FILE__, __LINE__, #cond);                       \
            std::fprintf(stderr, __VA_ARGS__);                             \
            std::fprintf(stderr, "\n");                                    \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

// 整数相等 CHECK (带 got/want 打印)
#define P1SESS_CHECK_EQ(cs, got, want)                                     \
    do {                                                                   \
        const long long g_ = (long long)(got);                             \
        const long long w_ = (long long)(want);                             \
        if (!(g_ == w_)) {                                                 \
            std::fprintf(stderr, "CHECK failed %s:%d: got=%lld want=%lld (%s)\n", \
                         __FILE__, __LINE__,                              \
                         static_cast<long long>(g_),                      \
                         static_cast<long long>(w_), #got);               \
            ++(cs).failures;                                              \
        }                                                                  \
    } while (0)

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

// 全链 main 框架: 解析组名 → 跑组 → rc
inline int run_all_groups(const p1sess::TestGroup* groups, std::size_t n, int argc, char** argv) {
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--", 0) != 0) group = a;
    }
    // ASTROCS_P1SESS_FAULT: 逗号分隔故障注入名单 (主执行器路径)
    init_fault_registry_from_env();
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1sess] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1sess] group %s FAIL rc=%d\n", groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1sess] group %s PASS\n", groups[i].name);
        }
        std::fflush(stdout);
    }
    if (total_fail == 0) {
        std::fprintf(stdout, "P1SESS TESTS PASS (group=%s)\n", group.c_str());
        return 0;
    }
    std::fprintf(stderr, "P1SESS TESTS FAIL (%d group(s) failed, group=%s)\n", total_fail, group.c_str());
    return 1;
}

}  // namespace p1sess

#endif  // P1SESS_TEST_MAIN_HPP
