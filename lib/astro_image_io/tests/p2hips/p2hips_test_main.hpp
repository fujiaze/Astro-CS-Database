// ============================================================================
// p2hips/p2hips_test_main.hpp — SCI-F3-001 harness (组 runner + 故障注入框架)
// ----------------------------------------------------------------------------
// 合同锚: DATA-UNC-001 §30.2 (DATA-P2-REJ-001) / §30.3 (DATA-P2-PROV-001),
// docs/contracts/DATA_SEMANTICS.md:2259-2305。
// 被测面: lib/astro_image_io/src/hips/aio_hips_writer.cpp +
//         lib/astro_image_io/src/hips/aio_hips_reader.cpp (astrocs_hips)。
// 结构对齐 P1-HIPS-TEST 先例 (lib/astro_image_io/tests/p1hips/)。
// ============================================================================
#ifndef P2HIPS_TEST_MAIN_HPP
#define P2HIPS_TEST_MAIN_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace p2hips {

inline int g_failures = 0;
inline int g_checks = 0;

inline void check_impl(bool cond, const char* expr, const char* msg,
                       const char* file, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::fprintf(stderr, "CHECK failed %s:%d: %s -- %s\n", file, line, expr,
                     msg);
    }
}

#define P2H_CHECK(cond, msg)     ::p2hips::check_impl((cond), #cond, (msg), __FILE__, __LINE__)

// 故障注入面 (等价缺陷注入必败): 未设置环境变量时零行为差异。
// 注入名由被测实现读取 (aio_hips_writer.cpp fault_injected)。
//   ASTROCS_HIPS_PROV_FAULT = missing_key | value_drift
//   ASTROCS_HIPS_DIAG_FAULT = sentinel    | skip_write
inline bool injected(const char* var, const char* name) {
    const char* v = std::getenv(var);
    return v && name && std::strcmp(v, name) == 0;
}

using Group = void (*)();

inline int run(int argc, char** argv, const std::vector<std::pair<const char*, Group>>& groups) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <group> [base_dir]\n", argv[0]);
        return 2;
    }
    const std::string want = argv[1];
    for (const auto& g : groups) {
        if (want == g.first) {
            g.second();
            if (g_failures == 0) {
                std::printf("P2HIPS[%s] PASS checks=%d\n", g.first, g_checks);
                return 0;
            }
            std::fprintf(stderr, "P2HIPS[%s] FAIL %d/%d\n", g.first, g_failures, g_checks);
            return 1;
        }
    }
    std::fprintf(stderr, "unknown group: %s\n", want.c_str());
    return 2;
}

}  // namespace p2hips

#endif  // P2HIPS_TEST_MAIN_HPP
