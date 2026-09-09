// P1-PHOT-TEST · 测试执行器框架 (单执行器 + 单测组注册 + 故障注入)
//
// 控制包任务: P1-PHOT-TEST (lock-P1-PHOT; 依赖 P1-PHOT-DOC 闭环)。合同锚:
// docs/algorithms/PHOTOMETRIC_FIT.md §13.4 TEST-PHOT-DESIGN-001 (P1-PHOT-DOC
// 冻结, 2026-09-07) + docs/science/PHOTOMETRY.md §11 (SCI-PHOT-001, FROZEN
// T103 2026-08-23); 矩阵行 P1-PHOT (MOD-astrocs-phase1-photometry,
// TEST-PHOT-DESIGN-001 → TEST-P1-PHOT-001); 模块合同
// lib/photometric_calib/README.md r1 (API-PHOT-001 六导出)。
//
// 单跑方式 (每个测试组 == 独立 ctest 名, 二进制内按位置参数单跑):
//   ./p1phot_tests units|properties|oracle|negative
//   ./p1phot_tests all
//
// 故障注入 (模板 <prefix>-TEST 验收: "故障注入能让测试失败"):
//   ASTROCS_P1PHOT_FAULT=<regname>[,<regname>...]
//   每个注册的 fault 使对应 CHECK 在报告阶段确定性翻转 → 二进制 rc=1,
//   输出 "FAULT-INJECT <name>" 行。
//   例: ASTROCS_P1PHOT_FAULT=u1_scale_injection ./p1phot_tests units
// 模式对齐先例: lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp
// (FaultRegistry + 组 runner + note_injected 一次性报告, commit c19b4a59)。
#ifndef P1PHOT_TEST_MAIN_HPP
#define P1PHOT_TEST_MAIN_HPP

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1phot {

struct FaultRegistry {
    static FaultRegistry& instance() {
        static FaultRegistry r;
        return r;
    }
    // 由 main() 启动时从 ASTROCS_P1PHOT_FAULT 初始化
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

#define P1PHOT_CHECK(cs, cond, faultname)                                  \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            /* 已注入本组: 后续 CHECK 全部计为失败 (测试必败) */            \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1phot::FaultRegistry::instance().injected(faultname)) { \
            (cs).note_injected(faultname);                                 \
            ++(cs).failures;                                               \
        } else if (!(cond)) {                                              \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n",               \
                         __FILE__, __LINE__, #cond);                       \
            ++(cs).failures;                                               \
        }                                                                  \
    } while (0)

#define P1PHOT_CHECK_MSG(cs, cond, faultname, ...)                         \
    do {                                                                   \
        if ((cs).fault_reported && (faultname) != nullptr) {               \
            ++(cs).failures;                                               \
        } else if (faultname != nullptr &&                                 \
                   p1phot::FaultRegistry::instance().injected(faultname)) { \
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

// float/double 位级比较 (bitwise 容差口径: 仅在数学可满足处使用 —
// P1-NOISE-TEST 先例纪律; NaN payload 传播不保证 bitwise, NaN 单独断言)
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

// 相对误差门 (got vs want): NaN 对 NaN 视为相等 (显式 NaN 语义用例单独断言)
inline bool rel_close_d(double got, double want, double rtol) {
    if (std::isnan(want)) return std::isnan(got);
    if (std::isinf(want)) return got == want;
    if (want == 0.0) return got == 0.0;
    const double err = std::fabs(got - want);
    return err <= rtol * std::fabs(want);
}

inline bool rel_close_f(float got, double want, double rtol) {
    return rel_close_d((double)got, want, rtol);
}

struct TestGroup {
    const char* name;
    int (*fn)(void);
};

// ASTROCS_P1PHOT_FAULT: 逗号分隔故障注入名单 → FaultRegistry
inline void init_fault_registry_from_env() {
    const char* f = std::getenv("ASTROCS_P1PHOT_FAULT");
    if (!f) return;
    std::string s = f;
    std::size_t pos = 0;
    while (pos < s.size()) {
        const std::size_t comma = s.find(',', pos);
        const std::string tok = s.substr(pos, (comma == std::string::npos ? s.size() : comma) - pos);
        if (!tok.empty()) p1phot::FaultRegistry::instance().active.push_back(tok);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
}

// 全链 main 框架: 解析组名 → 跑组 → rc
inline int run_all_groups(const p1phot::TestGroup* groups, std::size_t n, int argc, char** argv) {
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
        std::fprintf(stdout, "[p1phot] group %s ...\n", groups[i].name);
        std::fflush(stdout);
        const int rc = groups[i].fn();
        if (rc != 0) {
            std::fprintf(stderr, "[p1phot] group %s FAIL rc=%d\n", groups[i].name, rc);
            total_fail += 1;
        } else {
            std::fprintf(stdout, "[p1phot] group %s PASS\n", groups[i].name);
        }
        std::fflush(stdout);
    }
    if (total_fail == 0) {
        std::fprintf(stdout, "P1PHOT TESTS PASS (group=%s)\n", group.c_str());
        return 0;
    }
    std::fprintf(stderr, "P1PHOT TESTS FAIL (%d group(s) failed, group=%s)\n", total_fail, group.c_str());
    return 1;
}

}  // namespace p1phot

#endif  // P1PHOT_TEST_MAIN_HPP
