// tests/unit/cpu006_bench_report_test.cpp — CPU-006 (G3, V7.1) benchmark report 单元测试
// 覆盖: report 生成(quick)与规格字段; verdict 阈值门; heavy 不得单线程;
//       无合格候选回落 baseline; verify 正负例; 生产 profile 结构性不被触碰;
//       suite/timeout 负向样例。
// release suite 全量实跑(12 kernel × 3 规模)耗时数分钟, 由环境变量
// ASTROCS_CPU006_ENABLE_RELEASE=1 门控: ctest 默认跳过; benchmark 实跑单独执行。
// 用法: cpu006_bench_report_test [output_dir]
//   output_dir 提供时把生成的 report JSON 落盘(quick_report.json / release_report.json)。
#include "bench_report.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "profile_gen.h"   // RawCandidate

using astrocs::backend_host::AggWinner;
using astrocs::backend_host::RawCandidate;
using astrocs::backend_host::aggregate_benchmark_kernels;

static int failures = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

namespace {

RawCandidate cand(const std::string& kid, const std::string& sc, const std::string& prov,
                  uint32_t workers, double median, bool pass = true) {
    RawCandidate c;
    c.kernel_id = kid;
    c.size_class = sc;
    c.provider = prov;
    c.workers = workers;
    c.block = 64;
    c.median_ns = median;
    c.mad_ns = median * 0.1;   // 离散度 10% < 35% 阈值
    c.p05_ns = median * 0.9;
    c.p95_ns = median * 1.1;
    c.oracle_pass = pass;
    return c;
}

std::map<std::string, std::map<std::string, AggWinner>> agg1(const std::vector<RawCandidate>& v) {
    return aggregate_benchmark_kernels(v);
}

void save_report(const std::string& dir, const std::string& name, const std::string& json) {
    if (dir.empty()) return;
    std::ofstream f(dir + "/" + name);
    f << json;
}

astrocs::backend_host::BenchReportOptions base_opts(const std::string& suite,
                                                    const char bin_byte) {
    astrocs::backend_host::BenchReportOptions opt;
    opt.suite = suite;
    opt.build_id = "0.0.0-alpha.0+gabcdef123456";   // 中性占位(与发布版本解耦)
    opt.source_commit = "abcdef1234567890abcdef1234567890abcdef12";
    opt.benchmark_binary_sha256 = std::string(64, bin_byte);
    opt.deadline_ns = 0;
    return opt;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string out_dir = argc > 1 ? argv[1] : "";
  const std::string calib = "calibration-pixel-transform";   // compute(heavy)
  const std::string noise = "noise-snr-reductions";          // memory(heavy)

  // 1) Oracle 门: 无合格候选 → baseline + fallback_reason(结构性不可胜出)
  {
    auto m = agg1({cand(calib, "medium", "avx2", 2, 100.0, /*pass=*/false)});
    const auto& w = m.at(calib).at("medium");
    CHECK(w.provider == "baseline");
    CHECK(w.candidates_ok == 0);
    CHECK(!w.fallback_reason.empty());
    CHECK(w.median_ns == 0.0);
  }

  // 2) heavy 禁止单线程: 存在 >=2 worker 合格候选时, 1-worker 即使更快也不得胜出
  {
    std::vector<RawCandidate> v;
    v.push_back(cand(calib, "medium", "baseline", 1, 50.0));    // 最快但单线程
    v.push_back(cand(calib, "medium", "baseline", 2, 80.0));    // 多线程(较慢)
    v.push_back(cand(calib, "medium", "avx2", 4, 90.0));
    auto m = agg1(v);
    const auto& w = m.at(calib).at("medium");
    CHECK(w.workers >= 2);                      // 重计算不退化为单线程
    CHECK(!w.provider.empty() && w.median_ns > 0);
    CHECK(w.dispersion_ok);                     // 10% MAD → 过阈值
  }
  // 2b) 仅有 1-worker 候选(单核机) → 允许 workers==1(机器无关语义)
  {
    auto m = agg1({cand(calib, "medium", "baseline", 1, 50.0)});
    CHECK(m.at(calib).at("medium").workers == 1);
  }

  // 3) winner = min median; 平手(<1e-2) → 保守 provider 优先
  {
    std::vector<RawCandidate> v;
    v.push_back(cand(calib, "medium", "avx2", 2, 100.0));
    v.push_back(cand(calib, "medium", "baseline", 2, 100.9));   // 差 0.9% → 保守胜
    auto m = agg1(v);
    CHECK(m.at(calib).at("medium").provider == "baseline");
    // 恰在阈值上(1%) → 不算平手, 快者胜
    std::vector<RawCandidate> v1;
    v1.push_back(cand(calib, "medium", "avx2", 2, 100.0));
    v1.push_back(cand(calib, "medium", "baseline", 2, 101.0));
    auto m1 = agg1(v1);
    CHECK(m1.at(calib).at("medium").provider == "avx2");
    // 差距大 → 快者胜
    std::vector<RawCandidate> v2;
    v2.push_back(cand(calib, "medium", "avx2", 2, 100.0));
    v2.push_back(cand(calib, "medium", "baseline", 2, 200.0));
    auto m2 = agg1(v2);
    CHECK(m2.at(calib).at("medium").provider == "avx2");
  }

  // 4) 阈值门: MAD/median > 0.35 → dispersion 不过 → thresholds/eligible FAIL
  {
    auto m = agg1({cand(calib, "medium", "baseline", 2, 100.0)});
    m[calib]["medium"].mad_ns = 40.0;   // 40% > 35%
    m[calib]["medium"].dispersion_ok = false;   // 离散度超限(重算)
    auto vd = astrocs::backend_host::benchmark_report_verdict(m, false);
    CHECK(!vd.thresholds_passed);
    CHECK(!vd.eligible_for_profile);
    // 修复离散度(重算 dispersion_ok) + oracle 全过 → eligible
    m[calib]["medium"].mad_ns = 10.0;
    m[calib]["medium"].dispersion_ok = true;   // 10% <= 35%
    auto vd2 = astrocs::backend_host::benchmark_report_verdict(m, false);
    CHECK(vd2.all_oracle_passed);
    CHECK(vd2.thresholds_passed);
    CHECK(vd2.eligible_for_profile);
    // timeout → eligible 不可
    auto vd3 = astrocs::backend_host::benchmark_report_verdict(m, true);
    CHECK(!vd3.eligible_for_profile);
    // 空 kernels → 全 false
    auto vd4 = astrocs::backend_host::benchmark_report_verdict(
        std::map<std::string, std::map<std::string, AggWinner>>{}, false);
    CHECK(!vd4.all_oracle_passed && !vd4.thresholds_passed && !vd4.eligible_for_profile);
  }

  // 5) suite 映射与非法 suite 负向样例
  {
    CHECK(astrocs::backend_host::benchmark_suite_mode("quick") == "quick");
    CHECK(astrocs::backend_host::benchmark_suite_mode("release") == "full");
    CHECK(astrocs::backend_host::benchmark_suite_mode("full").empty());    // 非法
    CHECK(astrocs::backend_host::benchmark_suite_mode("").empty());        // 非法
    auto bad = base_opts("full", 'a');                                      // 非法
    auto o = astrocs::backend_host::generate_benchmark_report(bad);
    CHECK(o.json.empty());                      // 非法 suite → 空 report(负向)
    CHECK(o.kernels.empty());
  }

  // 6) quick report 生成: 规格字段齐全 + verify 正例 + 篡改负向样例
  std::string quick_json;
  {
    auto o = astrocs::backend_host::generate_benchmark_report(base_opts("quick", 'a'));
    CHECK(!o.json.empty());
    CHECK(!o.production_profile_touched);       // 结构性: 不触碰生产 profile
    CHECK(!o.raw.empty());                      // 原始候选全量保存
    CHECK(o.raw_samples_sha256.size() == 64);
    CHECK(o.available_logical_cpus >= 1);
    quick_json = o.json;
    save_report(out_dir, "bench_report_quick.json", quick_json);
    // 规格字段(验收关键词 benchmark report)
    CHECK(o.json.find("\"schema\": \"astrocs.benchmark-report/v1\"") != std::string::npos);
    CHECK(o.json.find("\"suite\": \"quick\"") != std::string::npos);
    CHECK(o.json.find("\"warmup\": 3") != std::string::npos);
    CHECK(o.json.find("\"samples\": 7") != std::string::npos);
    CHECK(o.json.find("\"outlier_policy\"") != std::string::npos);
    CHECK(o.json.find("\"logical_available\"") != std::string::npos);
    CHECK(o.json.find("\"affinity\"") != std::string::npos);
    CHECK(o.json.find("\"numa_nodes\"") != std::string::npos);
    CHECK(o.json.find("\"cache\"") != std::string::npos);
    CHECK(o.json.find("\"memory_bandwidth\"") != std::string::npos);
    CHECK(o.json.find("\"compiler\"") != std::string::npos);
    CHECK(o.json.find("\"workers\"") != std::string::npos);
    CHECK(o.json.find("\"provider\"") != std::string::npos);
    CHECK(o.json.find("\"median_ns\"") != std::string::npos);
    CHECK(o.json.find("\"mad_ns\"") != std::string::npos);
    CHECK(o.json.find("\"input_samples_sha256\"") != std::string::npos);
    CHECK(o.json.find("\"eligible_for_profile\"") != std::string::npos);
    CHECK(o.json.find("\"production_profile_touched\": false") != std::string::npos);
    // 独立复读: 正例
    CHECK(astrocs::backend_host::verify_benchmark_report(o.json).empty());
    // 篡改 schema → 拒
    {
      const auto pos = o.json.find("astrocs.benchmark-report/v1");
      CHECK(pos != std::string::npos);
      std::string tampered = o.json;
      tampered.replace(pos, 33, "astrocs.benchmark-report/vX");
      CHECK(!astrocs::backend_host::verify_benchmark_report(tampered).empty());
    }
    // 篡改 outlier_policy → 拒(预冻结规则一致性)
    {
      const std::string key = "\"outlier_policy\": \"";
      const auto pos = o.json.find(key);
      CHECK(pos != std::string::npos);
      std::string tampered = o.json;
      tampered[pos + key.size()] = 'X';
      CHECK(!astrocs::backend_host::verify_benchmark_report(tampered).empty());
    }
    // 篡改 production_profile_touched=true → 拒(结构性: 不触碰生产 profile)
    {
      const std::string key = "\"production_profile_touched\": false";
      const auto pos = o.json.find(key);
      CHECK(pos != std::string::npos);
      std::string tampered = o.json;
      tampered.replace(pos, key.size(), "\"production_profile_touched\": true");
      CHECK(!astrocs::backend_host::verify_benchmark_report(tampered).empty());
    }
    // 篡改 suite → 拒
    {
      const std::string key = "\"suite\": \"quick\"";
      const auto pos = o.json.find(key);
      CHECK(pos != std::string::npos);
      std::string tampered = o.json;
      tampered.replace(pos, key.size(), "\"suite\": \"full\"");
      CHECK(!astrocs::backend_host::verify_benchmark_report(tampered).empty());
    }
    // 截断 → 拒(malformed)
    CHECK(!astrocs::backend_host::verify_benchmark_report(o.json.substr(0, 40)).empty());
    // 与 cpu-profile/v2 隔离: report 不是合法 v2 profile(生产面拒绝 report 文本)
    CHECK(!astrocs::backend_host::verify_profile_v2(o.json, "").empty());
  }

  // 7) release suite 全量实跑: 12 kernel × 3 规模。耗时数分钟 → 环境变量门控。
  if (std::getenv("ASTROCS_CPU006_ENABLE_RELEASE") != nullptr) {
    auto o = astrocs::backend_host::generate_benchmark_report(base_opts("release", 'b'));
    CHECK(!o.json.empty());
    CHECK(o.kernels.size() == 12);
    size_t entries = 0;
    for (const auto& [kid, by_size] : o.kernels) {
      (void)kid;
      CHECK(by_size.size() == 3);               // small/medium/large
      for (const auto& [sc, w] : by_size) {
        (void)sc;
        CHECK(w.candidates_all >= w.candidates_ok);
        CHECK(!w.provider.empty());
        ++entries;
      }
    }
    CHECK(entries == 36);
    CHECK(astrocs::backend_host::verify_benchmark_report(o.json).empty());
    save_report(out_dir, "bench_report_release.json", o.json);
    std::printf("release: kernels=%zu available=%u thresholds=%d eligible=%d\n",
                o.kernels.size(), o.available_logical_cpus,
                o.verdict.thresholds_passed ? 1 : 0,
                o.verdict.eligible_for_profile ? 1 : 0);
  } else {
    std::printf("release suite generation skipped (set ASTROCS_CPU006_ENABLE_RELEASE=1 to run)\n");
  }

  // 8) quick 覆盖: 1 kernel × 1 规模 + heavy 多线程覆盖(机器无关)
  {
    auto oq = astrocs::backend_host::generate_benchmark_report(base_opts("quick", 'c'));
    CHECK(oq.kernels.size() == 1);
    CHECK(oq.kernels.count("calibration-pixel-transform") == 1);
    for (const auto& [sc, w] : oq.kernels.at("calibration-pixel-transform")) {
      (void)sc;
      // heavy kernel: 存在多线程合格候选时聚合不得退 1(avail>=2 机器上)
      if (oq.available_logical_cpus >= 2) {
        CHECK(w.workers >= 2);
      }
      CHECK(w.candidates_ok >= 1);
      CHECK(w.dispersion_ok);
    }
  }

  if (failures == 0) {
    std::printf("CPU-006 BENCH-REPORT TESTS PASS (规格字段/verdict 门/heavy>=2/负向样例/release 门控)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-006 BENCH-REPORT TESTS FAIL (%d)\n", failures);
  return 1;
}
