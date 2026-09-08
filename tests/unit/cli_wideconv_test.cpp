// B13-R13-4 单元测试: WideCharToMultiByte 两段式换码缓冲计算 (cli_common.h
// utf8_from_wide_*) — 修复 main.cpp:59 与 commands.cpp:1218 的 1 字节越界写
// (分配 n-1 却传 cbMultiByte=n → 尾 NUL 越界)。
// 本测试用模拟 API 复刻 Win32 契约: query(cb=0)→返回含 NUL 字节数;
// convert(cap)→cap < 所需字节数即失败(返回0), 否则写入含 NUL 的全部字节。
// 历史错误序列在本契约下必然"容量不足失败或越界"; 正确序列产码与内容可验证。
#include "cli_common.h"

#include <string>
#include <vector>

#include <cstdio>

using namespace astrocs;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 模拟 WideCharToMultiByte(CP_UTF8, ..., -1, dst, cbMultiByte, ...) 契约。
// wide_utf8: 该"宽串"对应的真实 UTF-8 字节 (含尾 NUL 的总长 = utf8.size()+1)。
struct WideConvSim {
  std::string utf8;  // 不含 NUL
  // 查询: 返回含 NUL 的总字节数 (n)
  int query() const { return static_cast<int>(utf8.size()) + 1; }
  // 转换: dst 容量 cap; cap >= n 才成功 (写入含 NUL 全部 n 字节); 否则 0。
  // 返回写入字节数 (成功=含NUL); 模拟中显式检查不越界。
  int convert(char* dst, int cap, int* wrote_oob) const {
    const int n = query();
    if (cap < n) { *wrote_oob = cap - n; return 0; }  // 契约: 容量不足失败
    for (int i = 0; i < n; ++i) dst[i] = i < static_cast<int>(utf8.size()) ? utf8[i] : '\0';
    *wrote_oob = 0;
    return n;
  }
};

// 正确序列 (helper 三函数驱动): 分配 n → 转 n → resize(n-1)。
static bool convert_correctly(const WideConvSim& api, std::string* out, int* oob) {
  const int n = api.query();
  if (!utf8_from_wide_should_convert(n)) { out->clear(); *oob = 0; return true; }
  std::string s(utf8_from_wide_alloc_bytes(n), '\0');
  int wrote = api.convert(s.data(), n, oob);
  if (wrote <= 0) return false;
  s.resize(utf8_from_wide_final_len(n));
  *out = std::move(s);
  return true;
}

static void test_boundary_math() {
  // 历史缺陷数学: n=13 → 旧代码 s(n-1=12) 但 cb=13 → 1 字节越界。
  // helper: alloc=13, convert cap=13 (恰好), final=12。边界逐点:
  CHECK(utf8_from_wide_final_len(-1) == 0);   // API 失败 → 空串
  CHECK(utf8_from_wide_final_len(0) == 0);
  CHECK(utf8_from_wide_final_len(1) == 0);    // 仅 NUL (空串)
  CHECK(utf8_from_wide_final_len(2) == 1);    // 1 字符
  CHECK(utf8_from_wide_final_len(13) == 12);
  CHECK(utf8_from_wide_alloc_bytes(-1) == 0);
  CHECK(utf8_from_wide_alloc_bytes(1) == 0);
  CHECK(utf8_from_wide_alloc_bytes(13) == 13);  // 关键: 含 NUL 的完整容量
  CHECK(!utf8_from_wide_should_convert(-1));
  CHECK(!utf8_from_wide_should_convert(1));
  CHECK(utf8_from_wide_should_convert(2));
  CHECK(utf8_from_wide_should_convert(13));
}

static void test_conversion_sequences() {
  const std::vector<std::string> cases = {"", "a", "hello.exe", "C:\\astro\\astrocs.exe",
                                          "中文路径/天体.fits"};
  for (const auto& want : cases) {
    WideConvSim api{want};
    std::string got;
    int oob = 0;
    CHECK(convert_correctly(api, &got, &oob));
    CHECK(oob == 0);
    CHECK(got == want);  // 去 NUL 后逐字节等于期望 UTF-8
  }
}

// 历史错误序列在契约下的后果: 分配 n-1 却请求 cb=n → cap<n → 按契约失败
// (真实 API 中参数为指针容量检查, 越界写发生在实现层 — 本测试固化"错误序列
// 与正确序列容量请求不同"这一可静态区分事实)。
static void test_legacy_sequence_rejected() {
  WideConvSim api{"C:\\x\\astrocs.exe"};  // n = 18+1 = 19
  const int n = api.query();
  const size_t legacy_alloc = utf8_from_wide_final_len(n);      // 旧: n-1
  const int legacy_cb = n;                                       // 旧: 传 n
  int oob = 0;
  std::vector<char> buf(legacy_alloc);
  const int wrote = api.convert(buf.data(), static_cast<int>(legacy_alloc), &oob);
  (void)legacy_cb;
  CHECK(wrote == 0);          // 容量不足 → 失败 (契约层)
  CHECK(oob == -1);           // 差 1 字节 = 历史越界量的模拟投影
}

int main() {
  test_boundary_math();
  test_conversion_sequences();
  test_legacy_sequence_rejected();
  if (failures == 0) {
    std::printf("B13-R13-4 CLI WIDECONV TESTS PASS\n");
    return 0;
  }
  std::fprintf(stderr, "B13-R13-4 CLI WIDECONV TESTS FAIL (%d)\n", failures);
  return 1;
}
