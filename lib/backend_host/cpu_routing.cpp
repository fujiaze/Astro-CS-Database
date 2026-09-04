// lib/backend_host/cpu_routing.cpp — CPU-004/CPU-005 逐 kernel 路由与数值自测实现
#include "cpu_routing.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>

#include <nlohmann/json.hpp>

#include "cpu_features.h"
#include "hardware_inspect.h"
#include "profile_gen.h"

namespace astrocs::backend_host {

namespace {

// 机器可检测 feature bits(与 cpu_features.h 一致)
uint64_t detected_features() { return astrocs_cpu_detect_features_v1(); }

// 任何伪造/半写/类型篡改 profile 都不得越过本层抛异常(CPU-005 验收:
// "伪造 profile … 均退回"; 类型异常/越界/损坏 JSON 一律按不可信处理)。
bool parse_profile(const std::string& text, nlohmann::json* out) {
    if (!out) return false;
    try {
        *out = nlohmann::json::parse(text);
    } catch (...) {
        return false;
    }
    return out->is_object();
}

// 类型安全的字段读取: 类型不符/缺字段/损坏 → 返回 def 不抛异常。
template <typename T>
T jget(const nlohmann::json& o, const char* key, const T& def) {
    try {
        if (!o.is_object() || !o.contains(key)) return def;
        const nlohmann::json& v = o[key];
        if (v.is_null()) return def;
        return v.get<T>();
    } catch (...) {
        return def;
    }
}
template <typename T>
T jget_ref(const nlohmann::json& o, const char* key, const T& def) {
    try {
        if (!o.is_object() || !o.contains(key)) return def;
        const nlohmann::json& v = o[key];
        if (v.is_null()) return def;
        return v.get_ref<const T&>();
    } catch (...) {
        return def;
    }
}

bool provider_supported(const std::string& provider) {
    const uint64_t feats = detected_features();
    if (provider == "baseline") return true;
    if (provider == "avx2") return (feats & (ACS_FEAT_AVX2 | ACS_FEAT_FMA)) ==
                                   (ACS_FEAT_AVX2 | ACS_FEAT_FMA);
    if (provider == "avx512") return (feats & ACS_FEAT_AVX512F) != 0;
    return false;
}

uint32_t logical_available_from_hw(const std::string& hw_json) {
    nlohmann::json hw;
    if (!parse_profile(hw_json, &hw)) return 1;
    try {
        return static_cast<uint32_t>(hw.value("available_logical_cpus", 1u));
    } catch (...) {
        return 1;
    }
}

bool json_is_finite(double v) { return std::isfinite(v); }

// profile kernels[kernel_id] 行读取辅助; 返回 false=无行/损坏
bool kernel_row(const nlohmann::json& prof, const std::string& kernel_id,
                const nlohmann::json** out) {
    if (out) *out = nullptr;
    try {
        if (!prof.is_object() || !prof.contains("kernels") || !prof["kernels"].is_object())
            return false;
        if (!prof["kernels"].contains(kernel_id)) return false;
        const nlohmann::json& kp = prof["kernels"][kernel_id];
        if (!kp.is_object()) return false;
        if (out) *out = &kp;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

ProfileVerdict validate_profile_v2_for_machine(const std::string& profile_json,
                                               const std::string& current_commit,
                                               const std::string& hw_json) {
    ProfileVerdict v;
    nlohmann::json prof;
    if (!parse_profile(profile_json, &prof)) {
        v.stale_reason = "profile malformed JSON";
        return v;
    }
    // 结构校验(复用 verify_profile_v2; 含 schema/必填字段/版本/commit/workers/block)。
    // verify_profile_v2 (CPU-003 文件) 仅捕获 parse_error; 类型篡改字段会令其
    // .value()/比较抛 type_error —— 本层必须捕获, 伪造 profile 一律退回 (CPU-005)。
    std::string err;
    try {
        err = verify_profile_v2(profile_json, current_commit);
    } catch (...) {
        err = "verify_profile_v2 raised (forged profile type)";
    }
    if (!err.empty()) {
        v.stale_reason = err;
        return v;
    }
    // 机器一致性: arch/quota_signature (verify 已保证 host/build 存在且非空)
    const nlohmann::json& hst = prof["host"];
    if (jget_ref<std::string>(hst, "arch", "") != "amd64") {
        v.stale_reason = "host.arch != amd64";
        return v;
    }
    nlohmann::json hw;
    if (!parse_profile(hw_json, &hw)) {
        v.stale_reason = "hardware inspect unavailable";
        return v;
    }
    try {
        const std::string hw_q = jget_ref<std::string>(hw, "quota_signature", "");
        const std::string prof_q = jget_ref<std::string>(hst, "quota_signature", "");
        if (hw_q.empty() || prof_q.empty() || hw_q != prof_q) {
            v.stale_reason = "quota_signature mismatch (rerun 'astrocs benchmark cpu')";
            return v;
        }
        const uint32_t hw_avail = static_cast<uint32_t>(hw.value("available_logical_cpus", 0u));
        const uint32_t prof_avail = static_cast<uint32_t>(hst.value("logical_available", 0u));
        if (hw_avail != prof_avail) {
            v.stale_reason = "logical_available mismatch (affinity/cgroup changed)";
            return v;
        }
    } catch (...) {
        v.stale_reason = "hardware inspect unavailable";
        return v;
    }
    v.valid = true;
    return v;
}

bool route_kernel_from_profile(const std::string& profile_json,
                               const std::string& kernel_id,
                               const std::string& hw_json,
                               KernelRoute* out_route) {
    if (!out_route) return false;
    nlohmann::json prof;
    if (!parse_profile(profile_json, &prof)) {
        *out_route = conservative_route(kernel_id, logical_available_from_hw(hw_json));
        out_route->fallback_reason = "profile malformed";
        return false;
    }
    const nlohmann::json* krow = nullptr;
    if (!kernel_row(prof, kernel_id, &krow)) {
        // 该 kernel 无 profile 记录 → 保守 baseline 但多线程
        *out_route = conservative_route(kernel_id, logical_available_from_hw(hw_json));
        out_route->fallback_reason = "kernel not in profile";
        return true;   // profile 整体有效, 该 kernel 回退
    }
    const std::string provider = jget_ref<std::string>(*krow, "provider", "baseline");
    KernelRoute r;
    r.kernel_id = kernel_id;
    r.workers = jget<uint32_t>(*krow, "workers", 1u);
    r.block = jget<uint64_t>(*krow, "block", 1ull);
    r.self_test_sha256 = jget_ref<std::string>(*krow, "self_test_sha256", "");
    if (!provider_supported(provider)) {
        // provider 不可用 → 回 baseline(保守)但保留 profile 的 workers(多线程)
        r.provider = "baseline";
        r.fallback_reason = "provider '" + provider + "' unsupported on this CPU";
        if (r.workers < 1) r.workers = 1;
        *out_route = r;
        return true;
    }
    r.provider = provider;
    r.fallback_reason = krow->contains("fallback_reason") && !(*krow)["fallback_reason"].is_null()
        ? jget_ref<std::string>(*krow, "fallback_reason", "") : "";
    if (r.workers < 1) r.workers = 1;
    *out_route = r;
    return true;
}

KernelRoute conservative_route(const std::string& kernel_id, uint32_t available_cpus) {
    KernelRoute r;
    r.kernel_id = kernel_id;
    r.provider = "baseline";
    r.workers = available_cpus > 0 ? available_cpus : 1;   // ≥1; 可用≥2 不退 1(15 §4)
    r.block = 1;
    r.fallback_reason = "no_valid_profile";
    return r;
}

// ───────────────────────── CPU-005 ─────────────────────────

/* 注册 kernel_id 集 (与 backend_table.inc / profile_gen_v2 kSpecs 的 12 kernel 对齐;
 * 事实源交叉校验在 driver/runner 层与 provider kernel_list 完成)。 */
const std::vector<std::string>& registered_kernel_ids_v1() {
    static const std::vector<std::string> kIds = {
        "calibration-pixel-transform", "noise-snr-reductions", "wcs-psf-batch",
        "drizzle-overlap", "drizzle-accumulate", "drizzle-normalize",
        "upm-spmv", "upm-residual", "upm-weight-update",
        "rejection-statistics", "integration-accumulate", "hips-bulk-transform",
    };
    return kIds;
}

IdentityCheck check_profile_identity_v1(const std::string& profile_json,
                                        const std::string& hw_json,
                                        const std::string& current_commit) {
    IdentityCheck out;
    nlohmann::json prof, hw;
    if (!parse_profile(profile_json, &prof) || !parse_profile(hw_json, &hw)) {
        out.reason = "profile/hw malformed JSON";
        return out;
    }
    if (!prof.contains("host") || !prof["host"].is_object() ||
        !prof.contains("build") || !prof["build"].is_object()) {
        out.reason = "profile missing host/build";
        return out;
    }
    const auto& hst = prof["host"];
    const auto& bd = prof["build"];
    // build hash 面
    const std::string sc = jget_ref<std::string>(bd, "source_commit", "");
    if (current_commit.empty() || sc != current_commit) {
        out.reason = "build.source_commit mismatch";
        return out;
    }
    // CPU 身份 + XCR0 + OS
    const auto same_str = [](const nlohmann::json& a, const nlohmann::json& b) -> int {
        try {
            if (a.is_null() || b.is_null()) return -1;
            return a.get<std::string>() == b.get<std::string>() ? 1 : 0;
        } catch (...) { return -1; }
    };
    const auto same_num = [](const nlohmann::json& a, const nlohmann::json& b) -> int {
        try {
            if (a.is_null() || b.is_null()) return -1;
            return a.get<double>() == b.get<double>() ? 1 : 0;
        } catch (...) { return -1; }
    };
    if (hst.contains("vendor") && hw.contains("vendor")) {
        const int eq = same_str(hst["vendor"], hw["vendor"]);
        if (eq == 0) { out.reason = "host.vendor changed"; return out; }
    }
    if (hst.contains("family") && hw.contains("family")) {
        const int eq = same_num(hst["family"], hw["family"]);
        if (eq == 0) { out.reason = "host.family changed"; return out; }
    }
    if (hst.contains("model") && hw.contains("model")) {
        const int eq = same_num(hst["model"], hw["model"]);
        if (eq == 0) { out.reason = "host.model changed"; return out; }
    }
    if (hst.contains("stepping") && hw.contains("stepping")) {
        const int eq = same_num(hst["stepping"], hw["stepping"]);
        if (eq == 0) { out.reason = "host.stepping changed"; return out; }
    }
    if (hst.contains("xcr0") && hw.contains("xcr0")) {
        const int eq = same_num(hst["xcr0"], hw["xcr0"]);
        if (eq == 0) { out.reason = "host.xcr0 changed (OS 不再保存 AVX/AVX-512 状态)"; return out; }
    }
    if (hst.contains("os_abi") && hw.contains("os")) {
        const std::string hw_os = jget_ref<std::string>(hw["os"], "name", "");
        const int eq = same_str(hst["os_abi"], nlohmann::json(hw_os));
        if (eq == 0) { out.reason = "host.os_abi changed"; return out; }
    }
    // features: profile features 数组必须是当前 hw feature_names 子集 (机器只会变少;
    // profile 在"至少这些 feature"的机器上测得; 现机缺任一 → CPU hash 变化)
    if (hst.contains("features") && hst["features"].is_array()) {
        std::vector<std::string> hw_names;
        try {
            if (hw.contains("feature_names") && hw["feature_names"].is_array())
                for (const auto& f : hw["feature_names"])
                    hw_names.push_back(f.get<std::string>());
            for (const auto& f : hst["features"]) {
                const std::string name = f.get<std::string>();
                if (std::find(hw_names.begin(), hw_names.end(), name) == hw_names.end()) {
                    out.reason = "host.features changed (missing " + name + ")";
                    return out;
                }
            }
        } catch (...) {
            out.reason = "host.features malformed";
            return out;
        }
    }
    // benchmark 二进制 hash (build 绑定; hw.cli_sha256 与 profile 记录一致)
    const std::string pb = jget_ref<std::string>(bd, "benchmark_binary_sha256", "");
    if (!pb.empty()) {
        const std::string hb = jget_ref<std::string>(hw, "cli_sha256", "");
        if (!hb.empty() && pb != hb) {
            out.reason = "build.benchmark_binary_sha256 changed";
            return out;
        }
    }
    out.valid = true;
    return out;
}

bool profile_kernel_benchmark_valid(const std::string& profile_json,
                                    const std::string& kernel_id,
                                    std::string* reason_out,
                                    double* median_out, double* mad_out) {
    nlohmann::json prof;
    if (!parse_profile(profile_json, &prof)) {
        if (reason_out) *reason_out = "profile malformed JSON";
        return false;
    }
    const nlohmann::json* kp = nullptr;
    if (!kernel_row(prof, kernel_id, &kp)) {
        if (reason_out) *reason_out = "kernel not in profile (no benchmark record)";
        return false;
    }
    const std::string ct = jget_ref<std::string>(*kp, "correctness_test", "");
    if (ct != "oracle:pass") {
        if (reason_out) *reason_out = "correctness_test != oracle:pass (" + ct + ")";
        return false;
    }
    const std::string fr = jget_ref<std::string>(*kp, "fallback_reason", "");
    if (!fr.empty()) {
        if (reason_out) *reason_out = "profile kernel fell back (" + fr + ")";
        return false;
    }
    const double med = jget<double>(*kp, "median", -1.0);
    const double mad = jget<double>(*kp, "mad", -1.0);
    if (!(med > 0.0) || !json_is_finite(med)) {
        if (reason_out) *reason_out = "median not finite positive (NaN/Inf/<=0)";
        return false;
    }
    if (!(mad >= 0.0) || !json_is_finite(mad)) {
        if (reason_out) *reason_out = "mad not finite non-negative (NaN/Inf)";
        return false;
    }
    if (median_out) *median_out = med;
    if (mad_out) *mad_out = mad;
    return true;
}

GainCheck check_gain_sufficient_v1(double baseline_median_ns,
                                   double cand_median_ns,
                                   double min_gain_rel) {
    GainCheck g;
    if (!(baseline_median_ns > 0.0) || !json_is_finite(baseline_median_ns) ||
        !(cand_median_ns > 0.0) || !json_is_finite(cand_median_ns)) {
        g.reason = "baseline/cand median not finite positive";
        return g;
    }
    g.gain_rel = (baseline_median_ns - cand_median_ns) / baseline_median_ns;
    if (!(g.gain_rel > min_gain_rel)) {
        g.reason = "gain " + std::to_string(g.gain_rel) +
                   " < min_gain_rel " + std::to_string(min_gain_rel) +
                   " (low benefit -> baseline)";
        return g;
    }
    g.sufficient = true;
    g.reason = "gain " + std::to_string(g.gain_rel) + " >= " +
               std::to_string(min_gain_rel);
    return g;
}

/* decide_kernel_v1: 固定 query→self_test→eligible→benchmark→select。
 * 语义 (CPU-005 / 15 §3 对齐 profile_gen_v2 冻结规则):
 *   - v2 profile 每 kernel 只记录 winner 的 median/mad (无 baseline 配对行);
 *   - 无 live_rows 时: 该行是 CPU-003 已按噪声门限(<3% → 保守)优选的 winner,
 *     benchmark 阶段只做结构/数值有限性复验 (NaN/Inf/缺行 → baseline), select 信任
 *     profile 已做过的噪声门限 (detail 标注 profile_trusted);
 *   - live_rows 提供 (spot benchmark / CPU-006 再测) 时: benchmark 阶段取实测行,
 *     select 按冻结门限 min_gain_rel 相对实测 baseline 重判: 任何候选相对 baseline
 *     收益不足 → 保持 baseline (低收益退回); NaN/非有限/oracle 失败行一律丢弃,
 *     丢弃到无可比行 → baseline (数值 mismatch)。
 */
KernelDecision decide_kernel_v1(const std::string& profile_json,
                                const std::string& kernel_id,
                                const std::string& hw_json,
                                const std::string& current_commit,
                                const std::vector<ProviderEvidence>& providers,
                                const std::vector<BenchRow>* live_rows,
                                double min_gain_rel) {
    KernelDecision d;
    d.kernel_id = kernel_id;
    const auto fallback = [&](const std::string& stage, const std::string& why) {
        d.provider = "baseline";
        d.stage = stage;
        d.ok = false;
        d.detail = why;
        d.fallback_reason = why;
        d.self_test_sha256.clear();
    };

    // ── query ──
    // profile 合格性 + build/CPU/OS 身份 hash 变化 全在此阶段拒绝。
    const ProfileVerdict pv = validate_profile_v2_for_machine(
        profile_json, current_commit, hw_json);
    if (!pv.valid) { fallback("query", std::string("profile invalid: ") + pv.stale_reason); return d; }
    const IdentityCheck idc = check_profile_identity_v1(profile_json, hw_json, current_commit);
    if (!idc.valid) { fallback("query", std::string("identity changed: ") + idc.reason); return d; }
    d.stage = "query";
    d.detail = "profile valid + identity ok";

    // ── self_test / eligible ──
    // provider 证据(query+self_test)由加载层提供; 本层只读证据做逐 kernel 判定。
    nlohmann::json prof;
    try { prof = nlohmann::json::parse(profile_json); } catch (...) { /* handled */ }
    const nlohmann::json* kp = nullptr;
    const bool has_row = kernel_row(prof, kernel_id, &kp);
    const std::string want_provider = has_row
        ? jget_ref<std::string>(*kp, "provider", "baseline") : "baseline";

    // 实际已注册该 kernel 且 self_test 通过的 provider (逐 kernel 粒度!)
    std::vector<std::string> eligible;    // 变体候选 (不含 baseline)
    std::string want_sha;
    for (const auto& p : providers) {
        if (!p.query_ok) continue;
        if (!p.self_test_ok) continue;    // self_test 失败 → 不参与路由 (15 §2)
        if (std::find(p.kernels.begin(), p.kernels.end(), kernel_id) != p.kernels.end()) {
            if (p.id != "baseline") eligible.push_back(p.id);
            if (p.id == want_provider && !p.self_test_sha256.empty())
                want_sha = p.self_test_sha256;
        }
    }
    d.self_test_sha256 = want_sha;

    if (has_row && want_provider != "baseline") {
        // profile 想要变体: 必须已 query+self_test+在表内 (self_test 阶段)
        bool ok_st = false;
        for (const auto& p : providers) {
            if (p.id != want_provider || !p.query_ok || !p.self_test_ok) continue;
            if (std::find(p.kernels.begin(), p.kernels.end(), kernel_id) == p.kernels.end())
                continue;
            ok_st = true;
            // profile 记录的 provider self-test hash 必须与实际 evidence 一致;
            // 错误 hash (build/DSO 身份漂移) → baseline (CPU-005 验收)。
            const std::string prof_sha =
                jget_ref<std::string>(*kp, "self_test_sha256", "");
            const std::string ev_sha = p.self_test_sha256;
            if (!prof_sha.empty() && !ev_sha.empty() && prof_sha != ev_sha) {
                fallback("self_test", "self_test_sha256 mismatch (profile " +
                         prof_sha.substr(0, 12) + " vs provider " +
                         ev_sha.substr(0, 12) + ")");
                return d;
            }
            break;
        }
        if (!ok_st) {
            fallback("self_test", "provider '" + want_provider +
                     "' not query/self_test/kernel-registered on this machine");
            return d;
        }
        d.stage = "self_test";
        d.detail = "provider " + want_provider + " query+self_test ok";
    } else {
        d.stage = "self_test";
        d.detail = "baseline only for this kernel (no variant registered)";
    }

    // eligible 阶段: 候选集合 = loaded ∩ 注册该 kernel 的变体
    if (has_row && want_provider != "baseline") {
        bool in_elig = false;
        for (const auto& id : eligible) if (id == want_provider) { in_elig = true; break; }
        if (!in_elig) {
            fallback("eligible", "provider '" + want_provider + "' not eligible for kernel");
            return d;
        }
    }
    d.stage = "eligible";
    d.detail = "eligible: " +
               (eligible.empty() ? std::string("(none)") : eligible[0]) +
               (eligible.size() > 1 ? " +" + std::to_string(eligible.size() - 1) : "");

    // ── benchmark ──
    // profile 行必须完整 (oracle:pass + 有限 median/mad); 缺 benchmark/NaN → baseline。
    std::string breason;
    double prof_med = 0.0, prof_mad = 0.0;
    if (!profile_kernel_benchmark_valid(profile_json, kernel_id, &breason,
                                        &prof_med, &prof_mad)) {
        fallback("benchmark", "profile benchmark invalid: " + breason);
        return d;
    }
    // live_rows 提供现场 spot benchmark → 取实测; 否则信任 profile 记录行。
    std::vector<BenchRow> rows;
    bool trusted_profile_rows = false;
    if (live_rows && !live_rows->empty()) {
        for (const auto& r : *live_rows) {
            if (!(r.median_ns > 0.0) || !json_is_finite(r.median_ns) ||
                !json_is_finite(r.mad_ns) || !r.oracle_pass) continue;  // NaN/mismatch 丢弃
            rows.push_back(r);
        }
        if (rows.empty()) {
            fallback("benchmark", "live benchmark rows all NaN/mismatch -> baseline");
            return d;
        }
    } else {
        trusted_profile_rows = true;
        rows.push_back({"baseline", prof_med, prof_mad, true});
        d.median_ns = prof_med;
        d.mad_ns = prof_mad;
    }
    d.stage = "benchmark";
    d.detail = "benchmark rows: " + std::to_string(rows.size()) +
               (trusted_profile_rows ? " (profile trusted)" : " (live)");

    // ── select ──
    if (trusted_profile_rows) {
        // profile 记录行 (winner 已按 08/CPU-003 冻结噪声门限优选)。结构/身份/数值
        // 有限性全部通过 → 信任 profile 的 provider 选择。detail 标注 profile_trusted。
        d.provider = want_provider;   // has_row 已保证; 无行→profile_kernel_benchmark_valid 已拒
        d.stage = "select";
        d.ok = true;
        d.detail = "selected " + want_provider + " (profile_trusted; 08 margin applied upstream)";
        return d;
    }
    // live rows: 以 baseline 行 median 为参照 (缺 baseline 行 → 保守 baseline)。
    double base_med = -1.0;
    for (const auto& r : rows)
        if (r.provider == "baseline") { base_med = r.median_ns; break; }
    if (!(base_med > 0.0)) {
        fallback("select", "no baseline benchmark row -> baseline");
        return d;
    }
    if (has_row && want_provider != "baseline") {
        for (const auto& r : rows)
            if (r.provider == want_provider) {
                const GainCheck g = check_gain_sufficient_v1(base_med, r.median_ns, min_gain_rel);
                d.gain_rel = g.gain_rel;
                if (!g.sufficient) {
                    fallback("select", "low benefit: " + g.reason);
                    return d;
                }
                d.provider = want_provider;
                d.stage = "select";
                d.ok = true;
                d.detail = "selected " + want_provider + " (live gain " +
                           std::to_string(d.gain_rel) + " >= " +
                           std::to_string(min_gain_rel) + ")";
                return d;
            }
        fallback("select", "no live benchmark row for provider '" + want_provider +
                 "' -> baseline");
        return d;
    }
    // profile 目标为 baseline (或无 profile 行): 选 baseline。
    d.provider = "baseline";
    d.stage = "select";
    d.ok = true;
    d.detail = "selected baseline (no variant benefit claimed by profile)";
    return d;
}

std::string kernel_decision_to_json(const KernelDecision& d) {
    nlohmann::json j = {
        {"provider", d.provider},
        {"stage", d.stage},
        {"ok", d.ok},
        {"detail", d.detail},
        {"median_ns", d.median_ns},
        {"mad_ns", d.mad_ns},
        {"gain_rel", d.gain_rel},
        {"self_test_sha256", d.self_test_sha256.empty()
            ? nlohmann::json(nullptr) : nlohmann::json(d.self_test_sha256)},
        {"fallback_reason", d.fallback_reason.empty()
            ? nlohmann::json(nullptr) : nlohmann::json(d.fallback_reason)},
    };
    return j.dump();
}

std::string build_route_table_v1(const std::string& profile_json,
                                 const std::string& hw_json,
                                 const std::string& current_commit,
                                 const std::vector<ProviderEvidence>& providers,
                                 const std::vector<BenchRow>* live_rows,
                                 double min_gain_rel,
                                 const std::vector<std::string>& kernel_ids) {
    std::vector<std::string> ids = kernel_ids;
    if (ids.empty()) {
        try {
            const auto prof = nlohmann::json::parse(profile_json);
            if (prof.contains("kernels") && prof["kernels"].is_object())
                for (auto it = prof["kernels"].begin(); it != prof["kernels"].end(); ++it)
                    ids.push_back(it.key());
        } catch (...) { /* empty */ }
    }
    nlohmann::json routes = nlohmann::json::object();
    bool any_variant = false;
    bool any_fallback = false;
    for (const auto& kid : ids) {
        const KernelDecision d = decide_kernel_v1(
            profile_json, kid, hw_json, current_commit, providers, live_rows, min_gain_rel);
        if (d.provider != "baseline") any_variant = true;
        if (!d.fallback_reason.empty()) any_fallback = true;
        routes[kid] = nlohmann::json::parse(kernel_decision_to_json(d));
    }
    nlohmann::json out = {
        {"routes", routes},
        {"decision", any_variant ? "variant_selected" : "all_baseline"},
        {"any_fallback", any_fallback},
    };
    return out.dump(2) + "\n";
}

}  // namespace astrocs::backend_host
