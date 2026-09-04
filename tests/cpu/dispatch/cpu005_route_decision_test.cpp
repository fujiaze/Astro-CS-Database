// tests/cpu/dispatch/cpu005_route_decision_test.cpp — CPU-005 路由决策验收矩阵
// 规格: 04_CPU_RESOURCE_TASKS.md CPU-005; 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md
//       §2/§3 (冻结噪声门限 3% = 0.03, 见 profile_gen_v2.cpp select_winner 同源)。
//
// 固定执行序 query → self_test → eligible → benchmark → select; 任一阶段失败即回
// baseline 并在 KernelDecision.stage/fallback_reason 记录。路由以 kernel_id 粒度;
// profile 不合格 / build/CPU/OS hash 变化 / provider self_test hash 不符 / 缺
// benchmark / NaN 或数值 mismatch / 低收益(<3%) → 一律 baseline。伪造 profile
// (损坏 JSON / 字段类型篡改) 必须退回而非抛异常 (本层 catch, verify_profile_v2
// 仅捕 parse_error)。
//
// 本文件为纯函数单测: hw_json / profile / provider evidence / live_rows 全部合成,
// 不 dlopen、不依赖真实 CPUID(provider 可用性由 ProviderEvidence 显式模拟 ——
// 逐 kernel 判定只读该证据; CPUID/XCR0 真实门由 CPU-001 capability 层覆盖)。
#include "cpu_routing.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace bh = astrocs::backend_host;

static int failures = 0;
#define CHECK(cond)                                                           \
  do {                                                                        \
    if (!(cond)) {                                                            \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                             \
    }                                                                         \
  } while (0)

static const std::string COMMIT = std::string(40, '1');       // 当前 source commit
static const std::string SHA_A = std::string(64, 'a');        // avx512 provider hash
static const std::string SHA_B = std::string(64, 'b');        // avx2 provider hash
static const std::string SHA_C = std::string(64, 'c');        // baseline 行 hash
static const std::string QUOTA = std::string(64, '9');        // quota_signature

static const char* K_HIPS = "hips-bulk-transform";      // avx512 注册
static const char* K_DRZ = "drizzle-accumulate";        // avx2 注册
static const char* K_CAL = "calibration-pixel-transform";  // 仅 baseline

// 合成 hw_json (与 profile host 匹配的机器; feature_names ⊇ profile features)
static std::string make_hw(const std::string& os_name,
                           const std::vector<std::string>& extra_features,
                           const std::string& cli_sha = "") {
  nlohmann::json j;
  j["vendor"] = "x";
  j["family"] = 6;
  j["model"] = 1;
  j["stepping"] = 1;
  j["os"] = {{"name", os_name}};
  std::vector<std::string> f = {"sse2", "avx", "avx2", "fma", "avx512f",
                                "avx512cd", "avx512bw", "avx512dq", "avx512vl"};
  f.insert(f.end(), extra_features.begin(), extra_features.end());
  j["feature_names"] = f;
  j["xcr0"] = 255.0;                       // OS 保存 YMM+ZMM
  j["available_logical_cpus"] = 4;
  j["quota_signature"] = QUOTA;
  if (!cli_sha.empty()) j["cli_sha256"] = cli_sha;
  return j.dump();
}

static nlohmann::json base_host_json() {
  nlohmann::json h;
  h["arch"] = "amd64";
  h["vendor"] = "x";
  h["family"] = 6;
  h["model"] = 1;
  h["stepping"] = 1;
  h["os_abi"] = "linux";
  h["features"] = {"sse2", "avx", "avx2", "fma", "avx512f", "avx512cd",
                   "avx512bw", "avx512dq", "avx512vl"};
  h["xcr0"] = 255.0;
  h["logical_available"] = 4;
  h["quota_signature"] = QUOTA;
  return h;
}

static nlohmann::json base_build_json(const std::string& commit,
                                      const std::string& bench_sha = std::string(64, 'e')) {
  nlohmann::json b;
  b["astrocs_version"] = "0.10.0-alpha.2";
  b["source_commit"] = commit;
  b["benchmark_binary_sha256"] = bench_sha;
  b["runtime_build_id"] = "r1";
  b["provider_build_ids"] = {{"baseline", "b0"}, {"avx2", "b2"}, {"avx512", "b5"}};
  return b;
}

// 单 kernel 行; prov ∈ baseline|avx2|avx512; sha 为该行记录的 self_test hash
static nlohmann::json kernel_row_json(const std::string& prov, const std::string& sha,
                                      double median, double mad,
                                      const std::string& correctness = "oracle:pass",
                                      const std::string& fallback = "") {
  nlohmann::json k;
  k["workload_class"] = "compute";
  k["provider"] = prov;
  k["workers"] = 4;
  k["block"] = 8192;
  k["correctness_test"] = correctness;
  k["self_test_sha256"] = sha;
  k["median"] = median;
  k["mad"] = mad;
  k["fallback_reason"] = fallback.empty() ? nlohmann::json(nullptr)
                                          : nlohmann::json(fallback);
  return k;
}

// 合成合法 v2 profile (三 kernel; hips=avx512 / drizzle=avx2 / calibration=baseline)
static std::string make_profile(const std::string& commit = COMMIT,
                                const std::string& bench_sha = std::string(64, 'e'),
                                const std::string& os_abi = "linux",
                                const std::string& vendor = "x") {
  nlohmann::json p;
  p["schema"] = "astrocs.cpu-profile/v2";
  p["profile_id"] = "sha256:" + std::string(64, 'f');
  p["created_utc"] = "2026-09-01T00:00:00Z";
  nlohmann::json h = base_host_json();
  h["os_abi"] = os_abi;
  h["vendor"] = vendor;
  p["host"] = h;
  p["build"] = base_build_json(commit, bench_sha);
  p["memory_bandwidth"] = {{"copy", 1.0}, {"read", 1.0}, {"write", 1.0}, {"triad", 1.0}};
  p["raw_samples_sha256"] = std::string(64, 'd');
  nlohmann::json ks;
  ks[K_HIPS] = kernel_row_json("avx512", SHA_A, 60.0, 2.0);   // baseline 100 → 快 40%
  ks[K_DRZ] = kernel_row_json("avx2", SHA_B, 70.0, 2.0);
  ks[K_CAL] = kernel_row_json("baseline", SHA_C, 100.0, 2.0);
  p["kernels"] = ks;
  return p.dump();
}

// provider evidence: id/query/self_test + kernel 注册表 (模拟加载层已 dlopen+query+self_test)
static bh::ProviderEvidence ev(const std::string& id, bool q, bool st,
                               const std::string& sha,
                               const std::vector<std::string>& kernels) {
  bh::ProviderEvidence e;
  e.id = id; e.query_ok = q; e.self_test_ok = st; e.self_test_sha256 = sha;
  e.kernels = kernels;
  return e;
}

static std::vector<bh::ProviderEvidence> full_providers() {
  return {ev("baseline", true, true, "", {K_HIPS, K_DRZ, K_CAL}),
          ev("avx2", true, true, SHA_B, {K_DRZ}),
          ev("avx512", true, true, SHA_A, {K_HIPS})};
}

// live spot-benchmark 行 (现场重测; CPU-006 再测路径)
static bh::BenchRow row(const std::string& prov, double med, double mad,
                        bool oracle = true) {
  bh::BenchRow r; r.provider = prov; r.median_ns = med; r.mad_ns = mad;
  r.oracle_pass = oracle; return r;
}

static bool is_baseline(const bh::KernelDecision& d) {
  return d.provider == "baseline" && !d.ok && !d.fallback_reason.empty();
}

int main() {
  const std::string hw = make_hw("linux", {});

  // ── T1: 合法 profile + 全 provider → 逐 kernel 决策 (kernel_id 粒度, 无全局 ISA) ──
  {
    const std::string prof = make_profile();
    const auto pr = full_providers();
    // hips: profile 行=avx512, evidence 一致 → select avx512 ok
    const auto dh = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(dh.provider == "avx512"); CHECK(dh.ok); CHECK(dh.stage == "select");
    CHECK(dh.fallback_reason.empty());
    CHECK(dh.self_test_sha256 == SHA_A);
    // drizzle: avx2
    const auto dd = bh::decide_kernel_v1(prof, K_DRZ, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(dd.provider == "avx2"); CHECK(dd.ok); CHECK(dd.stage == "select");
    // calibration: baseline (profile 无变体收益声明 → ok, 非 fallback)
    const auto dc = bh::decide_kernel_v1(prof, K_CAL, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(dc.provider == "baseline"); CHECK(dc.ok); CHECK(dc.stage == "select");
    // 同 profile 不同 kernel 选不同 provider → 证明 kernel_id 粒度 (无全局 preferred_isa)
    CHECK(dh.provider != dc.provider);
    std::printf("T1 PASS (kernel_id 粒度: %s=%s %s=%s %s=%s)\n",
                K_HIPS, dh.provider.c_str(), K_DRZ, dd.provider.c_str(),
                K_CAL, dc.provider.c_str());
  }

  // ── T2: 伪造 profile → baseline (损坏 JSON / 类型篡改不抛异常) ──
  {
    const auto pr = full_providers();
    const auto d1 = bh::decide_kernel_v1("{not json", K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d1)); CHECK(d1.stage == "query");
    // workers 为字符串 (类型篡改): verify_profile_v2 会抛 type_error → 本层 catch → baseline
    nlohmann::json prof = nlohmann::json::parse(make_profile());
    prof["kernels"][K_HIPS]["workers"] = "four";
    const auto d2 = bh::decide_kernel_v1(prof.dump(), K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d2)); CHECK(d2.stage == "query");
    CHECK(d2.fallback_reason.find("raised") != std::string::npos ||
          d2.fallback_reason.find("verify") != std::string::npos);
    std::printf("T2 PASS (伪造 profile/类型篡改 → baseline@query)\n");
  }

  // ── T3: build/source_commit hash 变化 → baseline@query ──
  {
    const auto pr = full_providers();
    const std::string wrong_commit = std::string(40, '2');
    const std::string prof = make_profile(wrong_commit);
    const auto d = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "query");
    CHECK(d.fallback_reason.find("source_commit") != std::string::npos ||
          d.fallback_reason.find("invalid") != std::string::npos);
    std::printf("T3 PASS (build hash 变化 → baseline@query)\n");
  }

  // ── T4: CPU hash 变化 (vendor) / OS hash 变化 (os_abi) / benchmark 二进制 hash ──
  {
    const auto pr = full_providers();
    const std::string prof_v = make_profile(COMMIT, std::string(64, 'e'), "linux", "other-vendor");
    const auto d1 = bh::decide_kernel_v1(prof_v, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d1)); CHECK(d1.stage == "query");
    CHECK(d1.fallback_reason.find("vendor") != std::string::npos);
    // OS 变化: profile os_abi=windows vs hw os=linux
    const std::string prof_o = make_profile(COMMIT, std::string(64, 'e'), "windows");
    const auto d2 = bh::decide_kernel_v1(prof_o, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d2)); CHECK(d2.stage == "query");
    CHECK(d2.fallback_reason.find("os_abi") != std::string::npos);
    // benchmark 二进制 hash 变化: profile 记录 vs hw cli_sha256
    const std::string prof_b = make_profile(COMMIT, std::string(64, 'e'));
    const std::string hw2 = make_hw("linux", {}, std::string(64, '3'));
    const auto d3 = bh::decide_kernel_v1(prof_b, K_HIPS, hw2, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d3)); CHECK(d3.stage == "query");
    CHECK(d3.fallback_reason.find("benchmark_binary") != std::string::npos);
    std::printf("T4 PASS (CPU/OS/benchmark hash 变化 → baseline@query)\n");
  }

  // ── T5: provider self_test hash 不符 (错误 hash) → baseline@self_test ──
  {
    std::vector<bh::ProviderEvidence> pr = full_providers();
    // avx512 evidence 实际 self_test hash 与 profile 行记录不同 (DSO 身份漂移)
    pr[2].self_test_sha256 = std::string(64, '7');
    const std::string prof = make_profile();
    const auto d = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "self_test");
    CHECK(d.fallback_reason.find("self_test_sha256") != std::string::npos);
    std::printf("T5 PASS (错误 self-test hash → baseline@self_test)\n");
  }

  // ── T6: provider query/self_test 失败 → baseline@self_test; 未注册 kernel 退回 ──
  {
    std::vector<bh::ProviderEvidence> pr = full_providers();
    pr[2].self_test_ok = false;            // avx512 self_test 失败
    const std::string prof = make_profile();
    const auto d = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "self_test");
    // avx512 想 hips 但未注册 avx2 → drizzle 保持 avx2 (互不影响)
    const auto dd = bh::decide_kernel_v1(prof, K_DRZ, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(dd.provider == "avx2"); CHECK(dd.ok);
    std::printf("T6 PASS (self_test 失败退回, kernel 间互不影响)\n");
  }

  // ── T7: 缺 benchmark (oracle:fail) → baseline@benchmark ──
  {
    const auto pr = full_providers();
    nlohmann::json prof = nlohmann::json::parse(make_profile());
    prof["kernels"][K_HIPS]["correctness_test"] = "oracle:fail";
    const auto d = bh::decide_kernel_v1(prof.dump(), K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "benchmark");
    CHECK(d.fallback_reason.find("correctness_test") != std::string::npos);
    // fallback_reason 非空 (该 kernel 上游已剔除) → benchmark 退回
    nlohmann::json prof2 = nlohmann::json::parse(make_profile());
    prof2["kernels"][K_HIPS]["fallback_reason"] = "upstream rejected";
    const auto d2 = bh::decide_kernel_v1(prof2.dump(), K_HIPS, hw, COMMIT, pr, nullptr, 0.03);
    CHECK(is_baseline(d2)); CHECK(d2.stage == "benchmark");
    std::printf("T7 PASS (缺 benchmark/oracle:fail/上游剔除 → baseline@benchmark)\n");
  }

  // ── T8: NaN mismatch (live rows 全 NaN / oracle 失败) → baseline ──
  {
    const auto pr = full_providers();
    const std::string prof = make_profile();
    const std::vector<bh::BenchRow> nan_rows = {
        row("baseline", std::numeric_limits<double>::quiet_NaN(), 1.0),
        row("avx512", std::numeric_limits<double>::infinity(), 1.0)};
    const auto d = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, &nan_rows, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "benchmark");
    // oracle 失败行丢弃 → 无可比行 → baseline
    const std::vector<bh::BenchRow> bad_rows = {
        row("baseline", 100.0, 1.0), row("avx512", 55.0, 1.0, false)};
    const auto d2 = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, &bad_rows, 0.03);
    CHECK(is_baseline(d2)); CHECK(d2.stage == "select");
    std::printf("T8 PASS (NaN/Inf/oracle-fail live rows → baseline)\n");
  }

  // ── T9: 低收益 (<3% 冻结门限) → baseline@select; 足够收益 → 选择 ──
  {
    const auto pr = full_providers();
    const std::string prof = make_profile();
    // gain=(100-98)/100=2% < 3% → 低收益退回
    const std::vector<bh::BenchRow> low = {row("baseline", 100.0, 1.0),
                                           row("avx512", 98.0, 1.0)};
    const auto d = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, &low, 0.03);
    CHECK(is_baseline(d)); CHECK(d.stage == "select");
    CHECK(d.fallback_reason.find("low benefit") != std::string::npos);
    // gain=(100-60)/100=40% ≥ 3% → 现场实测支持 avx512
    const std::vector<bh::BenchRow> good = {row("baseline", 100.0, 1.0),
                                            row("avx512", 60.0, 1.0)};
    const auto dg = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, &good, 0.03);
    CHECK(dg.provider == "avx512"); CHECK(dg.ok); CHECK(dg.stage == "select");
    CHECK(dg.gain_rel > 0.03);
    // 无 baseline 行 → 保守 baseline
    const std::vector<bh::BenchRow> nobase = {row("avx512", 60.0, 1.0)};
    const auto dn = bh::decide_kernel_v1(prof, K_HIPS, hw, COMMIT, pr, &nobase, 0.03);
    CHECK(is_baseline(dn));
    std::printf("T9 PASS (低收益退回 / 足够收益选择 / 无 baseline 保守)\n");
  }

  // ── T10: 路由表 trace (JSON) 显示每 kernel 实际 provider; 无全局 preferred_isa ──
  {
    const auto pr = full_providers();
    const std::string prof = make_profile();
    const std::string table = bh::build_route_table_v1(
        prof, hw, COMMIT, pr, nullptr, 0.03,
        {K_HIPS, K_DRZ, K_CAL});
    nlohmann::json t = nlohmann::json::parse(table);
    CHECK(t["decision"] == "variant_selected");
    CHECK(t["any_fallback"] == false);
    const auto& rt = t["routes"];
    CHECK(rt.contains(K_HIPS) && rt[K_HIPS]["provider"] == "avx512");
    CHECK(rt.contains(K_DRZ) && rt[K_DRZ]["provider"] == "avx2");
    CHECK(rt.contains(K_CAL) && rt[K_CAL]["provider"] == "baseline");
    // 全局字段不存在: 路由表只含 routes/decision/any_fallback; 每个 kernel 独立 provider
    CHECK(!t.contains("preferred_isa"));
    CHECK(!rt.contains("provider") && !rt.contains("preferred_isa"));
    // trace 显示实际 provider 证据字段
    CHECK(rt[K_HIPS].contains("stage") && rt[K_HIPS]["stage"] == "select");
    CHECK(rt[K_HIPS]["self_test_sha256"] == SHA_A);
    std::puts("TABLE_BEGIN");
    std::printf("%s", table.c_str());
    std::puts("TABLE_END");
    std::printf("T10 PASS (路由表 JSON per-kernel provider, 无全局 ISA)\n");
  }

  // ── T11: 无全局 preferred_isa=avx512 —— 编译期/源码静态断言见 runner (grep);
  //          运行时: 单 kernel 变体被逐 kernel 拒绝/允许互不影响, 已由 T1/T6 覆盖 ──

  if (failures == 0) {
    std::printf("CPU-005 ROUTE TESTS PASS (query→self_test→eligible→benchmark→select; "
                "伪造/错误hash/低收益/NaN/缺benchmark 全退回; kernel_id 粒度; 无全局ISA)\n");
    return 0;
  }
  std::fprintf(stderr, "CPU-005 ROUTE TESTS FAIL (%d)\n", failures);
  return 1;
}
