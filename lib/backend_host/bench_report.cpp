// lib/backend_host/bench_report.cpp — CPU-006 benchmark report 实现
// 层次合同见 bench_report.h: 本文件是 CPU-003 测量引擎(generate_profile_v2)之上的
// 审计聚合层。测量链唯一实现(generate_profile_v2), 本层只聚合规格字段、判定阈值门
// 并提供可复读校验。从不写生产 profile(schema 隔离; verify_profile_v2 拒绝本报告)。
#include "bench_report.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>

#include <nlohmann/json.hpp>

#include "bench_harness.h"
#include "hardware_inspect.h"
#include "profile_gen.h"
#include "sha256.h"

namespace astrocs::backend_host {

namespace {

using nlohmann::json;

std::string utc_now() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char ts[40];
    std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                  tm.tm_sec);
    return ts;
}

/* 规格字段: kernel sizes(小/中/大)与 workload_class 语义表。
 * 唯一用途: 把 raw 候选里的 (kernel_id,size_class) 完整呈现给审计者;
 * workload_class 与 profile_gen_v2 引擎 kSpecs 表逐条一致(同域注释冻结);
 * 此表不参与选择(选择只看测量值), 仅供 heavy 判定与展示。 */
struct KernelMeta {
    const char* kernel_id;
    const char* workload_class;   // "compute" | "memory"
};

const KernelMeta kKernelMeta[] = {
    {"calibration-pixel-transform", "compute"},
    {"noise-snr-reductions",        "memory"},
    {"wcs-psf-batch",               "compute"},
    {"drizzle-overlap",             "compute"},
    {"drizzle-accumulate",          "memory"},
    {"drizzle-normalize",           "memory"},
    {"upm-spmv",                    "memory"},
    {"upm-residual",                "compute"},
    {"upm-weight-update",           "compute"},
    {"rejection-statistics",        "memory"},
    {"integration-accumulate",      "memory"},
    {"hips-bulk-transform",         "compute"},
};

const char* kernel_workload(const std::string& kernel_id) {
    for (const auto& m : kKernelMeta)
        if (kernel_id == m.kernel_id) return m.workload_class;
    return "compute";   // 未知 kernel 保守视为 heavy
}

bool is_heavy(const std::string& workload) {
    return workload == "compute" || workload == "memory";
}

bool provider_rank(const std::string& p) {   // 保守序: baseline(0) < avx2(1) < avx512(2)
    if (p == "baseline") return 0;
    if (p == "avx2") return 1;
    return 2;
}

const char* kOutlierPolicyText =
    "no-point-removal: median/MAD robust over 7 samples; p05/p95 recorded for audit; "
    "winner with MAD/median > 0.35 flagged dispersion and cannot pass thresholds";

}  // namespace

const char* benchmark_outlier_policy() { return kOutlierPolicyText; }

std::string benchmark_suite_mode(const std::string& suite) {
    if (suite == "quick") return "quick";
    if (suite == "release") return "full";   // release = 全 kernel × 全规模
    return "";
}

std::map<std::string, std::map<std::string, AggWinner>> aggregate_benchmark_kernels(
    const std::vector<RawCandidate>& raw) {
    // 1) 按 (kernel_id, size_class) 分桶
    std::map<std::string, std::map<std::string, std::vector<const RawCandidate*>>> buckets;
    for (const auto& c : raw) buckets[c.kernel_id][c.size_class].push_back(&c);

    std::map<std::string, std::map<std::string, AggWinner>> out;
    for (const auto& [kid, by_size] : buckets) {
        const bool heavy = is_heavy(kernel_workload(kid));
        for (const auto& [sc, cands] : by_size) {
            AggWinner w;
            w.candidates_all = static_cast<uint32_t>(cands.size());
            std::vector<const RawCandidate*> ok;
            for (const auto* c : cands)
                if (c->oracle_pass && c->median_ns > 0) ok.push_back(c);
            w.candidates_ok = static_cast<uint32_t>(ok.size());
            if (ok.empty()) {
                // 无合格候选 → baseline + 原因(无 benchmark/自检失败时 baseline 语义)
                w.provider = "baseline";
                w.fallback_reason = "no oracle-pass candidate";
                out[kid][sc] = w;
                continue;
            }
            // heavy 重计算禁止单线程: 存在 >=2 worker 合格候选时禁选 workers==1
            std::vector<const RawCandidate*> eligible = ok;
            if (heavy) {
                bool any_multi = false;
                for (const auto* c : ok)
                    if (c->workers >= 2) { any_multi = true; break; }
                if (any_multi) {
                    eligible.clear();
                    for (const auto* c : ok)
                        if (c->workers >= 2) eligible.push_back(c);
                }
            }
            // best = min median; 平手(<1e-2 相对差) → 保守 provider/更少 worker
            const RawCandidate* best = eligible[0];
            for (const auto* c : eligible) {
                if (c->median_ns < best->median_ns) { best = c; continue; }
                const double rel = std::fabs(c->median_ns - best->median_ns) /
                                   std::max(best->median_ns, 1.0);
                if (rel < 1e-2) {
                    if (provider_rank(c->provider) < provider_rank(best->provider) ||
                        (provider_rank(c->provider) == provider_rank(best->provider) &&
                         c->workers < best->workers))
                        best = c;
                }
            }
            w.provider = best->provider;
            w.workers = best->workers;
            w.block = best->block;
            w.median_ns = best->median_ns;
            w.mad_ns = best->mad_ns;
            w.p05_ns = best->p05_ns;
            w.p95_ns = best->p95_ns;
            w.dispersion_ok =
                best->median_ns > 0 && (best->mad_ns / best->median_ns) <= kMadMedianRelMax;
            out[kid][sc] = w;
        }
    }
    return out;
}

BenchVerdict benchmark_report_verdict(
    const std::map<std::string, std::map<std::string, AggWinner>>& kernels,
    bool timeout_reached) {
    BenchVerdict v;
    v.all_oracle_passed = !kernels.empty();
    v.thresholds_passed = true;   // 阈值门: 任意 winner 离散度超限即 FAIL
    for (const auto& [kid, by_size] : kernels) {
        (void)kid;
        if (by_size.empty()) v.all_oracle_passed = false;
        for (const auto& [sc, w] : by_size) {
            (void)sc;
            if (w.candidates_ok == 0) v.all_oracle_passed = false;
            if (!w.dispersion_ok) v.thresholds_passed = false;
        }
    }
    v.thresholds_passed = v.thresholds_passed && v.all_oracle_passed;
    v.eligible_for_profile = v.thresholds_passed && !timeout_reached;
    return v;
}

BenchReportOutcome generate_benchmark_report(const BenchReportOptions& opt) {
    BenchReportOutcome out;
    const std::string mode = benchmark_suite_mode(opt.suite);
    if (mode.empty()) return out;   // 非法 suite → 空(负向)

    const auto t0 = std::chrono::steady_clock::now();
    ProfileBundle bundle = generate_profile_v2(mode, opt.build_id, opt.source_commit,
                                               opt.benchmark_binary_sha256, opt.backends_dir);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    const uint64_t elapsed_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    out.timeout_reached =
        opt.deadline_ns != 0 && elapsed_ns > opt.deadline_ns;

    out.raw = bundle.raw;
    out.raw_samples_sha256 = bundle.raw_samples_sha256;
    out.kernels = aggregate_benchmark_kernels(bundle.raw);
    out.verdict = benchmark_report_verdict(out.kernels, out.timeout_reached);
    out.production_profile_touched = false;   // 结构性: 本层从不落生产 profile

    // ── 规格字段: 硬件全量画像(CPU 指纹/逻辑核/cache/NUMA/affinity/编译器) ──
    const json hw = json::parse(hardware_inspect_json_v1(opt.build_id));
    const uint32_t avail = hw.value("available_logical_cpus", 1u);
    out.available_logical_cpus = avail;
    const json& smt = hw.value("smt", json::object());
    const bool smt_known = smt.value("known", false);
    const bool smt_enabled = smt.value("enabled", false);
    // 物理核: 容器内拓扑不可实测(physical_packages=null) → 显式派生估计并注明推导式;
    // 不把估计冒充实测(与 hardware_inspect physical_packages=null 同口径)。
    const uint32_t phys_est =
        (smt_known && smt_enabled) ? (avail + 1) / 2 : avail;

    json j;
    j["schema"] = "astrocs.benchmark-report/v1";
    j["report_id"] = "sha256:" + bundle.raw_samples_sha256;
    j["created_utc"] = utc_now();
    j["suite"] = opt.suite;
    j["suite_mode"] = mode;   // 引擎模式映射(quick→quick, release→full)

    // 规格字段: warmup/重复轮次/异常值剔除规则(预先固定, 唯一出处本层)
    j["measurement"] = {
        {"warmup", kBenchWarmup},
        {"samples", kBenchSamples},
        {"clock", "steady_clock"},
        {"outlier_policy", kOutlierPolicyText},
        {"outlier_policy_frozen", true},
        {"selection_rules", {
            {"oracle_gate", "only oracle-pass candidates can win"},
            {"avx512_min_gain_rel", 0.03},
            {"worker_min_gain_rel", 0.03},
            {"heavy_min_workers", 2},
        }},
    };

    // 规格字段: CPU 指纹、逻辑/物理核、cache/NUMA/affinity、内存、编译器
    j["hardware"] = {
        {"vendor", hw.value("vendor", "")},
        {"brand", hw.value("brand", "")},
        {"family", hw.value("family", 0)},
        {"model", hw.value("model", 0)},
        {"stepping", hw.value("stepping", 0)},
        {"microcode", hw.value("microcode", "")},
        {"feature_bits", hw.value("feature_bits", 0)},
        {"feature_names", hw.value("feature_names", json::array())},
        {"xcr0", hw.value("xcr0", 0)},
        {"logical_cpus_configured", hw.value("logical_cpus_configured", 0)},
        {"logical_available", avail},
        {"affinity", hw.value("affinity", json::array())},
        {"affinity_count", hw.value("affinity_count", 0)},
        {"cgroup_cpu_limit", hw.value("cgroup_cpu_limit", 0)},
        {"quota_signature", hw.value("quota_signature", "")},
        {"smt", smt},
        {"physical_cores_estimate", phys_est},
        {"physical_cores_derivation",
         (smt_known && smt_enabled)
             ? std::string("derived: ceil(logical_available/2); smt enabled")
             : std::string("derived: logical_available; smt disabled/unknown")},
        {"numa_nodes", hw.value("numa_nodes", 0)},
        {"cache", hw.value("cache", json::array())},
        {"ram_bytes", hw.value("ram_bytes", 0)},
        {"page_size", hw.value("page_size", 0)},
        {"os", hw.value("os", json::object())},
        {"compiler", hw.value("compiler", json::object())},
    };

    // 规格字段: 内存带宽(引擎同一 bench_memory 基线, 取自 profile 的同名段)
    const json prof = json::parse(bundle.json);
    j["memory_bandwidth"] = prof["memory_bandwidth"];

    // 规格字段: 输入 hash(确定性 LCG 输入的原始候选序列化 hash; 单一出处引擎)
    j["input_samples_sha256"] = bundle.raw_samples_sha256;

    // 规格字段: build/provider/编译器绑定
    j["build"] = {
        {"astrocs_build", opt.build_id},
        {"source_commit", opt.source_commit},
        {"benchmark_binary_sha256", opt.benchmark_binary_sha256},
    };

    // 规格字段: kernel sizes × workers × provider × median/MAD(仅 oracle-pass 胜出)
    json kernels = json::object();
    for (const auto& [kid, by_size] : out.kernels) {
        json kentry;
        kentry["workload_class"] = kernel_workload(kid);
        json sizes = json::object();
        for (const auto& [sc, w] : by_size) {
            sizes[sc] = {
                {"provider", w.provider},
                {"workers", w.workers},
                {"block", w.block},
                {"median_ns", w.median_ns},
                {"mad_ns", w.mad_ns},
                {"p05_ns", w.p05_ns},
                {"p95_ns", w.p95_ns},
                {"candidates_all", w.candidates_all},
                {"candidates_ok", w.candidates_ok},
                {"dispersion_ok", w.dispersion_ok},
                {"fallback_reason",
                 w.fallback_reason.empty() ? json(nullptr) : json(w.fallback_reason)},
            };
        }
        kentry["sizes"] = sizes;
        kernels[kid] = kentry;
    }
    j["kernels"] = kernels;

    // 规格字段: verdict 门(不会改变生产 profile 直到全部 self_test+阈值通过)
    j["verdict"] = {
        {"all_oracle_passed", out.verdict.all_oracle_passed},
        {"thresholds_passed", out.verdict.thresholds_passed},
        {"eligible_for_profile", out.verdict.eligible_for_profile},
        {"production_profile_touched", false},
        {"timeout_reached", out.timeout_reached},
    };
    // 规格字段: 命令 timeout 上下文(lib 预算标注; 命令层由 shell timeout 承担)
    j["timeout"] = {
        {"budget_ns", opt.deadline_ns},
        {"reached", out.timeout_reached},
        {"elapsed_ns", elapsed_ns},
    };
    out.json = j.dump(2) + "\n";
    return out;
}

std::string verify_benchmark_report(const std::string& json_text) {
    json d;
    try {
        d = json::parse(json_text);
    } catch (const json::parse_error& e) {
        return std::string("malformed JSON: ") + e.what();
    }
    if (d.value("schema", "") != "astrocs.benchmark-report/v1")
        return "schema != astrocs.benchmark-report/v1";
    for (const char* k : {"report_id", "created_utc", "suite", "suite_mode", "measurement",
                          "hardware", "memory_bandwidth", "input_samples_sha256", "build",
                          "kernels", "verdict", "timeout"}) {
        if (!d.contains(k) || d[k].is_null()) return std::string("missing required '") + k + "'";
    }
    const std::string suite = d.value("suite", "");
    if (suite != "quick" && suite != "release") return "suite invalid";
    if (d.value("suite_mode", "") != benchmark_suite_mode(suite))
        return "suite_mode mismatch";
    const std::string rid = d.value("report_id", "");
    if (rid.rfind("sha256:", 0) != 0 || rid.size() != 71) return "report_id not sha256:<64hex>";
    const std::string in_hash = d.value("input_samples_sha256", "");
    if (in_hash.size() != 64) return "input_samples_sha256 not 64hex";

    const auto& meas = d["measurement"];
    if (meas.value("warmup", 0) != kBenchWarmup)
        return "measurement.warmup != 3 (frozen)";
    if (meas.value("samples", 0) < kBenchSamples)
        return "measurement.samples < 7 (frozen)";
    if (!meas.value("outlier_policy_frozen", false))
        return "measurement.outlier_policy_frozen != true";
    if (meas.value("outlier_policy", "") != benchmark_outlier_policy())
        return "measurement.outlier_policy != frozen rule";

    const auto& hw = d["hardware"];
    for (const char* k : {"vendor", "logical_available", "affinity", "affinity_count",
                          "smt", "numa_nodes", "cache", "compiler"}) {
        if (!hw.contains(k)) return std::string("hardware missing '") + k + "'";
    }
    const auto& compiler = hw["compiler"];
    if (compiler.value("id", "") .empty()) return "hardware.compiler.id empty";

    const auto& build = d["build"];
    const std::string commit = build.value("source_commit", "");
    if (commit.size() != 40) return "build.source_commit not 40hex";

    const auto& kernels = d["kernels"];
    if (!kernels.is_object() || kernels.empty()) return "kernels empty";
    for (const auto& [kid, kentry] : kernels.items()) {
        if (kentry.value("workload_class", "") != kernel_workload(kid))
            return "kernels." + kid + ".workload_class mismatch";
        const auto& sizes = kentry["sizes"];
        if (!sizes.is_object() || sizes.empty()) return "kernels." + kid + ".sizes empty";
        for (const auto& [sc, s] : sizes.items()) {
            const std::string prov = s.value("provider", "");
            if (prov != "baseline" && prov != "avx2" && prov != "avx512")
                return "kernels." + kid + "." + sc + ".provider invalid";
            if (s.value("mad_ns", -1.0) < 0) return "kernels." + kid + "." + sc + ".mad_ns < 0";
            const int c_all = s.value("candidates_all", 0);
            const int c_ok = s.value("candidates_ok", 0);
            if (c_all < c_ok) return "kernels." + kid + "." + sc + ".candidates_all < ok";
            if (c_ok == 0) {
                // 无合格候选 → 必须回落 baseline 且带原因(无 benchmark 时 baseline 语义);
                // 未测量: workers==0 且 median==0(不可伪造测量值)
                if (prov != "baseline")
                    return "kernels." + kid + "." + sc + ": no OK candidate must fallback baseline";
                if (!s.contains("fallback_reason") || s["fallback_reason"].is_null())
                    return "kernels." + kid + "." + sc + ": fallback_reason missing";
                if (s.value("workers", -1) != 0)
                    return "kernels." + kid + "." + sc + ": fallback workers must be 0";
                if (s.value("median_ns", -1.0) != 0.0)
                    return "kernels." + kid + "." + sc + ": fallback median must be 0";
            } else {
                if (s.value("workers", 0) < 1) return "kernels." + kid + "." + sc + ".workers < 1";
                if (s.value("median_ns", 0.0) <= 0)
                    return "kernels." + kid + "." + sc + ".median_ns <= 0";
            }
        }
    }

    const auto& v = d["verdict"];
    for (const char* k : {"all_oracle_passed", "thresholds_passed", "eligible_for_profile",
                          "production_profile_touched", "timeout_reached"}) {
        if (!v.contains(k) || !v[k].is_boolean())
            return std::string("verdict missing bool '") + k + "'";
    }
    if (v.value("production_profile_touched", true))
        return "verdict.production_profile_touched must be false";
    const bool reached = d["timeout"].value("reached", false);
    const bool eligible_expect =
        v.value("all_oracle_passed", false) && v.value("thresholds_passed", false) && !reached;
    if (v.value("eligible_for_profile", !eligible_expect) != eligible_expect)
        return "verdict.eligible_for_profile inconsistent with oracle/threshold/timeout";
    return "";
}

}  // namespace astrocs::backend_host
