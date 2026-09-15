#ifndef ASTROCS_V6_P1_CAL_TEST_SUPPORT_HPP
#define ASTROCS_V6_P1_CAL_TEST_SUPPORT_HPP

/* 极简断言框架（不依赖 gtest）：正例 + 负例逐条登记，任一失败 rc!=0。
 * 零用例 / 全 skip 视为失败（qa policy：executed_cases==0 -> rc!=0）。 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace v6test {

struct CaseLedger {
  int executed = 0;
  int failed = 0;
  std::vector<std::string> failures;
};

inline CaseLedger& ledger() {
  static CaseLedger l;
  return l;
}

inline void record(bool ok, const std::string& name, const std::string& detail) {
  ledger().executed += 1;
  if (!ok) {
    ledger().failed += 1;
    ledger().failures.push_back(name + " :: " + detail);
    std::printf("[FAIL] %s :: %s\n", name.c_str(), detail.c_str());
  } else {
    std::printf("[ ok ] %s\n", name.c_str());
  }
}

inline bool close_rel(double a, double b, double rtol, double atol = 0.0) {
  const double diff = std::fabs(a - b);
  const double scale = std::fmax(std::fabs(a), std::fabs(b));
  return diff <= atol + rtol * scale;
}

/* 故障注入辅助：仅在测试中改环境变量。 */
inline void set_fault(const char* mode) {
#if defined(_WIN32)
  _putenv_s("ASTROCS_V6_CAL_FAULT", mode);
#else
  setenv("ASTROCS_V6_CAL_FAULT", mode, 1);
#endif
}
inline void clear_fault(void) {
#if defined(_WIN32)
  _putenv_s("ASTROCS_V6_CAL_FAULT", "");
#else
  unsetenv("ASTROCS_V6_CAL_FAULT");
#endif
}

}  // namespace v6test

#define V6_CHECK(cond, name)                                              \
  ::v6test::record((cond), (name), "condition false")

#define V6_CHECK_MSG(cond, name, msg) ::v6test::record((cond), (name), (msg))

#define V6_CHECK_CLOSE(got, exp, rtol, name)                              \
  ::v6test::record(::v6test::close_rel((got), (exp), (rtol)), (name),     \
                   "got=" + std::to_string(got) + " exp=" + std::to_string(exp))

#endif
