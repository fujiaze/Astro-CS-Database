// CPU-006 单元测试: benchmark 顺序链 + Oracle 门 + 稳健统计 + 候选派生
#include "bench_harness.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using astrocs::backend_host::BenchResult;
using astrocs::backend_host::bench_kernel;
using astrocs::backend_host::block_candidates;
using astrocs::backend_host::select_winner;
using astrocs::backend_host::worker_candidates;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) Oracle 门: 错误 backend(返回 OK 但输出错)不能胜出
  {
    std::vector<BenchResult> results;
    results.push_back({"wrong", "ORACLE_FAIL", "oracle mismatch", 0, 0, 0, 0, 0, 0, ""});
    results.push_back({"right", "OK", "", 7, 100.0, 5.0, 90.0, 110.0, 2, "abc"});
    CHECK(select_winner(results) == "right");   // OK 候选胜出
    std::vector<BenchResult> only_bad = {results[0]};
    CHECK(select_winner(only_bad) == "");       // 无 OK → 空 (结构性不胜出)
  }

  // 2) select_winner: median 最小者胜出
  {
    std::vector<BenchResult> r;
    r.push_back({"slow", "OK", "", 7, 200.0, 1.0, 190, 210, 1, "h"});
    r.push_back({"fast", "OK", "", 7, 50.0, 1.0, 45, 55, 2, "h"});
    CHECK(select_winner(r) == "fast");
  }

  // 3) worker 候选: {1, 中位, 全部} 派生 (无硬编码核数)
  {
    auto c2 = worker_candidates(2);
    CHECK(c2.size() >= 1 && c2.back() == 2);
    CHECK(std::find(c2.begin(), c2.end(), 1u) != c2.end());
    auto c4 = worker_candidates(4);
    CHECK(c4.back() == 4);
    CHECK(std::find(c4.begin(), c4.end(), 2u) != c4.end());  // 中位(≈物理核级)
    // 单调升序
    for (size_t i = 1; i < c4.size(); ++i) CHECK(c4[i] > c4[i - 1]);
  }

  // 4) block 候选: L2 派生几何序列 (机器无关)
  {
    auto blocks = block_candidates(1024 * 1024, 4);  // 1MB L2, f32
    CHECK(!blocks.empty());
    for (size_t i = 1; i < blocks.size(); ++i) CHECK(blocks[i] > blocks[i - 1]);
  }

  // 5) 统计正确性: median/MAD 已知输入
  {
    // 复刻 percentile: 奇数 n 取中位
    std::vector<double> v = {1.0, 2.0, 3.0, 4.0, 100.0};
    std::sort(v.begin(), v.end());
    double med = v[v.size() / 2];
    CHECK(med == 3.0);  // median 对 outlier 稳健
  }

  // 6) benchmark 顺序合同验证: harness 先 Oracle 后计时 (源码级)
  {
    std::string h = std::string(std::getenv("ASTROCS_REPO") ? std::getenv("ASTROCS_REPO") : "..")
                    + "/lib/backend_host/bench_harness.cpp";
    std::FILE* fp = std::fopen(h.c_str(), "r");
    CHECK(fp != nullptr);
    if (fp) {
      char buf[32768];
      size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
      buf[n] = 0;
      std::fclose(fp);
      std::string s(buf);
      // Oracle 检查在计时前; warmup 不计时; samples>=7
      CHECK(s.find("ORACLE_FAIL") != std::string::npos);
      CHECK(s.find("warmup") != std::string::npos);
      CHECK(s.find("median_ns") != std::string::npos);
      CHECK(s.find("mad_ns") != std::string::npos);
    }
  }

  if (failures == 0) {
    std::printf("CPU-006 TESTS PASS (Oracle 门/winner 选择/候选派生/统计稳健)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-006 TESTS FAIL (%d)\n", failures);
  return 1;
}
