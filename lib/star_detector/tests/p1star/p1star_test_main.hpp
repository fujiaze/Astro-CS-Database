// ============================================================================
// p1star_test_main.hpp — P1-STAR-TEST 执行器框架 (p1cal/p1cos/p1drz 同谱系)
// ----------------------------------------------------------------------------
// 合同锚: docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11.4
// TEST-STAR-DESIGN-001 (P1-STAR-DOC 冻结); ALG-STARDET-001 §2/§11.1。
// 控制包任务: P1-STAR-TEST (SA-P1ST-T, queue 49, lock-P1-STAR)。
//
// FaultRegistry: 环境变量 ASTROCS_P1STAR_FAULT=<name>[,<name>...] 命中的
// CHECK 断言翻转 (通过→失败) 并打印 "FAULT-INJECT <name>"; 自检组用它演示
// "注入必败", 其余组在无注入时必须全绿。验收关键词:
// unit/property/oracle/negative/performance。
//
// 注意: 被测面为 lib/star_detector 生产 sdet_* C API (5 src 独立编译)。
// sdet_log 会向 stderr 打印并在 CWD 下创建 lib/star_detector/logs/ —
// CTest 注册均带 WORKING_DIRECTORY 隔离 (见 CMakeLists.txt)。
// ============================================================================
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p1star {

struct CheckState {
    int failures = 0;
    int checks = 0;
    // ASTROCS_P1STAR_FAULT 命中集合 (构造时解析)
    std::vector<std::string> faults;

    CheckState() {
        const char* f = std::getenv("ASTROCS_P1STAR_FAULT");
        if (f && *f) {
            std::string s(f);
            for (std::size_t pos = 0; pos < s.size();) {
                std::size_t comma = s.find(',', pos);
                if (comma == std::string::npos) comma = s.size();
                std::string item = s.substr(pos, comma - pos);
                if (!item.empty()) faults.push_back(item);
                pos = comma + 1;
            }
        }
    }

    bool fault_hit(const char* name) const {
        for (const auto& f : faults)
            if (f == name) return true;
        return false;
    }

    void record(bool ok, const char* name, const char* file, int line) {
        ++checks;
        if (fault_hit(name)) {
            ok = !ok;
            std::printf("FAULT-INJECT %s\n", name);
        }
        if (!ok) {
            ++failures;
            std::printf("FAIL %s (%s:%d)\n", name, file, line);
        } else {
            std::printf("ok %s\n", name);
        }
    }
};

}  // namespace p1star

#define P1STAR_CHECK(cs, cond, name) \
    (cs).record(static_cast<bool>(cond), (name), __FILE__, __LINE__)
#define P1STAR_CHECK_EQ(cs, a, b, name) \
    (cs).record((a) == (b), (name), __FILE__, __LINE__)

// 组 runner: argv[1] 选组; 无参 = 全组顺序执行 (单执行器多组可单跑)。
namespace p1star {

using GroupFn = int (*)();

struct TestGroup {
    const char* name;
    GroupFn fn;
};

inline int run_all_groups(const TestGroup* groups, std::size_t n, int argc, char** argv) {
    const char* want = (argc > 1) ? argv[1] : nullptr;
    bool found = false;
    for (std::size_t i = 0; i < n; ++i) {
        if (want && std::strcmp(want, groups[i].name) != 0) continue;
        found = true;
        std::printf("== group %s ==\n", groups[i].name);
        int rc = groups[i].fn();
        std::fflush(stdout);
        if (rc != 0) return rc;
    }
    if (want && !found) {
        std::printf("unknown group '%s'\n", want);
        return 2;
    }
    return 0;
}

}  // namespace p1star
