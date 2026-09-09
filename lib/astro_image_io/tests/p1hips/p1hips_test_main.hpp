// P1-HIPS-TEST · 测试执行器框架 (单执行器 + 单测组注册 + 故障注入)
//
// 控制包任务: P1-HIPS-TEST (SA-P1H-T, queue 39, lock-P1-HIPS; 依赖
// P1-HIPS-DOC 闭环)。合同锚: docs/algorithms/HIPS_WRITER.md §9
// TEST-HIPS-DESIGN-001 (P1-HIPS-DOC 冻结, 2026-09-07); 矩阵行 P1-HIPS
// (MOD astrocs.phase1.hips-writer, TEST-P1-HIPS-001); 模块登记页
// docs/modules/registry/astrocs.phase1.hips-writer.md。
//
// 单跑方式 (每个测试组 == 独立可执行 ctest 名, 二进制内按 --group 单跑):
//   ./p1hips_tests units|properties|oracle|negative
//   ./p1hips_tests all
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ASTROCS_P1HIPS_FAULT=<regname>[,<regname>...]
//   每个注册的 fault 使对应 CHECK 组在报告阶段确定性翻转 → 二进制 rc=1,
//   输出 "FAULT-INJECT <name>" 行。
//   例: ASTROCS_P1HIPS_FAULT=i1_leaf_signal_bitwise ./p1hips_tests units
// 模式对齐先例: lib/cosmetic/tests/p1cos/p1cos_test_main.hpp (FaultRegistry
// + 组 runner + note_injected 一次性报告)。
#ifndef P1HIPS_TEST_MAIN_HPP
#define P1HIPS_TEST_MAIN_HPP

#include <dirent.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace p1hips {

struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    // 由 main() 启动时从 ASTROCS_P1HIPS_FAULT 初始化
    std::vector<std::string> active;

    bool injected(const char* name) const {
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

#define P1HIPS_CHECK(cs, cond, faultname)                                  \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            /* 已注入本组: 后续 CHECK 全部计为失败 (测试必败) */            \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1hips::FaultRegistry::instance().injected(faultname)) { \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",               \
                         __FILE__, __LINE__, #cond);                       \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

#define P1HIPS_CHECK_MSG(cs, cond, faultname, ...)                         \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1hips::FaultRegistry::instance().injected(faultname)) { \
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

#define P1HIPS_CHECK_EQ(cs, got, want)                                     \
    do {                                                                   \
        const long long g_ = (long long)(got);                             \
        const long long w_ = (long long)(want);                            \
        if (!(g_ == w_)) {                                                 \
            std::fprintf(stderr, "CHECK failed %s:%d: got=%lld want=%lld (%s)\n", \
                         __FILE__, __LINE__,                             \
                         static_cast<long long>(g_),                     \
                         static_cast<long long>(w_), #got);              \
            ++(cs).failures;                                             \
        }                                                                  \
    } while (0)

// float 位级比较 (bitwise 容差口径: NaN 布局与 -0.0f 亦须一致)
inline bool bits_eq_f(float a, float b) {
    std::uint32_t ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof(ua));
    std::memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

inline bool bits_eq_d(double a, double b) {
    std::uint64_t ua = 0, ub = 0;
    std::memcpy(&ua, &a, sizeof(ua));
    std::memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}

// f32 相对误差 (存储舍入界 rtol=1e-7, HIPS_WRITER.md §9)
inline bool rel_close_f(float got, double want, double rtol) {
    if (std::isnan(want)) return std::isnan(got);
    if (std::isinf(want)) return got == (float)want;
    if (want == 0.0) return got == 0.0f;
    const double err = std::fabs((double)got - want);
    return err <= rtol * std::fabs(want);
}

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

// ASTROCS_P1HIPS_FAULT: 逗号分隔故障注入名单 → FaultRegistry
inline void init_fault_registry_from_env() {
    const char* f = std::getenv("ASTROCS_P1HIPS_FAULT");
    if (!f) return;
    std::string s = f;
    std::size_t pos = 0;
    while (pos < s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok = s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
        if (!tok.empty()) p1hips::FaultRegistry::instance().active.push_back(tok);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

// 全链 main 框架: 解析 --group → 跑组 → rc
inline int run_all_groups(const p1hips::TestGroup* groups, std::size_t n, int argc, char** argv) {
    std::string group = "all";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--group" && i + 1 < argc) group = argv[++i];
        else if (a.rfind("--", 0) != 0) group = a;
    }
    init_fault_registry_from_env();
    int total_fail = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (group != "all" && group != groups[i].name) continue;
        std::fprintf(stdout, "[p1hips] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1hips] group %s FAIL rc=%d\n", groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1hips] group %s PASS\n", groups[i].name);
        }
        std::fflush(stdout);
    }
    if (total_fail == 0) {
        std::fprintf(stdout, "P1HIPS TESTS PASS (group=%s)\n", group.c_str());
        return 0;
    }
    std::fprintf(stderr, "P1HIPS TESTS FAIL (%d group(s) failed, group=%s)\n", total_fail, group.c_str());
    return 1;
}

// 共用助手 (定义于 p1hips_tests_units.cpp, properties/selfcheck 链引用):
// 写一个完整 f64 产品 (F1 常数 + F3 边界 tiles), 返回 finalize rc
int write_full_f64_product(const std::string& dir, int flags,
                           const char* obs_date, bool set_prov);

// POSIX 目录树递归收集 (测试工具级; units/oracle 共用)
void walk_dir(const std::string& dir, std::vector<std::string>& files);

// 目录树聚合哈希 (跳过文件名含 skip1/skip2 的文件; properties 时间戳
// 合同上不跨运行复现 → 确定性比对须跳过)。
// 定义于 p1hips_tests_units.cpp; properties 确定性组引用。
bool tree_digest(const std::string& dir, std::uint64_t& digest,
                 const char* skip1 = nullptr, const char* skip2 = nullptr);

}  // namespace p1hips

#endif  // P1HIPS_TEST_MAIN_HPP
