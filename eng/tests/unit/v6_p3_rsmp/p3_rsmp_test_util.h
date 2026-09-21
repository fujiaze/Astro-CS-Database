// eng/tests/unit/v6_p3_rsmp/p3_rsmp_test_util.h
// 共址测试工具：CHECK 宏 + 门结果断言。无第三方测试框架（与仓库既有单测一致）。
#ifndef P3_RSMP_TEST_UTIL_H
#define P3_RSMP_TEST_UTIL_H

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "p3_rsmp.h"

namespace p3test {
inline int g_checks = 0;
inline int g_failures = 0;

inline void note_failure(const char* file, int line, const std::string& what) {
  std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, what.c_str());
  ++g_failures;
}

inline bool has_code(const std::vector<astrocs::p3rsmp::GateResult>& v, const std::string& code) {
  for (const auto& g : v) {
    if (g.code == code) return true;
  }
  return false;
}

inline int finish(const char* suite) {
  std::printf("%s: checks=%d failures=%d -> %s\n", suite, g_checks, g_failures,
              g_failures == 0 ? "PASS" : "FAIL");
  return g_failures == 0 ? 0 : 1;
}
}  // namespace p3test

#define P3_CHECK(cond)                                                       \
  do {                                                                       \
    ++p3test::g_checks;                                                      \
    if (!(cond)) p3test::note_failure(__FILE__, __LINE__, #cond);            \
  } while (0)

#define P3_CHECK_NEAR(a, b, tol)                                              \
  do {                                                                       \
    ++p3test::g_checks;                                                      \
    const double p3_a = (double)(a);                                         \
    const double p3_b = (double)(b);                                         \
    const double p3_t = (double)(tol);                                       \
    if (!(std::fabs(p3_a - p3_b) <= p3_t)) {                                 \
      char p3_buf[256];                                                      \
      std::snprintf(p3_buf, sizeof(p3_buf),                                  \
                    "%s ~= %s (got %.17g vs %.17g, tol %g)", #a, #b, p3_a, p3_b, p3_t); \
      p3test::note_failure(__FILE__, __LINE__, p3_buf);                      \
    }                                                                        \
  } while (0)

#define P3_CHECK_CODE(gate_results, code)                                     \
  do {                                                                       \
    ++p3test::g_checks;                                                      \
    if (!p3test::has_code((gate_results), (code))) {                         \
      p3test::note_failure(__FILE__, __LINE__,                               \
                           std::string("expected gate code ") + (code));     \
    }                                                                        \
  } while (0)

#define P3_CHECK_NO_VIOLATION(gate_results)                                   \
  do {                                                                       \
    ++p3test::g_checks;                                                      \
    if (!(gate_results).empty()) {                                          \
      p3test::note_failure(__FILE__, __LINE__,                               \
                           std::string("expected no violation, got ") +      \
                               (gate_results).front().code);                 \
    }                                                                        \
  } while (0)

#endif  // P3_RSMP_TEST_UTIL_H
