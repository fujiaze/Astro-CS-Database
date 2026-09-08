// lib/backend_host/worker_advisor.cpp — CPU-008 自适应线程建议(资源面)
// 实现合同见 worker_advisor.h。本文件只做派生与记录:
//   - 一切 workers/block 由输入(探测核数/限制/profile 实测行)派生,
//     无 2/16/32 等固定核数分支(no fixed cores);
//   - profile 行值是 benchmark 选择(CPU-006 采集/CPU-003 冻结), 本层透传;
//   - 用户/系统上限是硬约束, 任何来源的建议不得越过(no_bypass_user_cap)。
#include "worker_advisor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <nlohmann/json.hpp>

#include "bench_harness.h"

namespace astrocs::backend_host {

namespace {

using nlohmann::json;

bool parse_json_text(const std::string& text, json* out) {
    if (out) *out = json::object();
    if (text.empty()) return false;
    try {
        *out = json::parse(text);
        return out->is_object();
    } catch (...) {
        return false;
    }
}

// 固定核数声明的词面(验收关键词 no fixed cores 的输入侧拒绝点)
bool has_fixed_cores_claim(const json& j, std::string* key_out) {
    static const char* kKeys[] = {"fixed_cores", "fixed_workers", "workers_fixed"};
    for (const char* k : kKeys) {
        if (j.contains(k)) {
            if (key_out) *key_out = k;
            return true;
        }
    }
    return false;
}

// L2 字节解析: hw["cache"] 数组中 level=="2"(或 "L2") 的 "size" 字符串
// ("256K"/"512K"/"1024K"/"2M" 等, Linux sysfs 单位 K/M; 纯数字按字节)。
bool l2_bytes_from_hw(const json& hw, uint64_t* out) {
    if (out) *out = 0;
    try {
        if (!hw.contains("cache") || !hw["cache"].is_array()) return false;
        for (const auto& c : hw["cache"]) {
            if (!c.is_object()) continue;
            const std::string lvl = c.value("level", "");
            const std::string lvl_norm = (lvl.size() > 1 && (lvl[0] == 'L' || lvl[0] == 'l'))
                                             ? lvl.substr(1) : lvl;
            if (lvl_norm != "2") continue;
            const std::string sz = c.value("size", "");
            if (sz.empty()) return false;
            char* end = nullptr;
            const double val = std::strtod(sz.c_str(), &end);
            if (!std::isfinite(val) || val <= 0 || end == sz.c_str()) return false;
            double bytes = val;
            if (*end == 'K' || *end == 'k') bytes *= 1024.0;
            else if (*end == 'M' || *end == 'm') bytes *= 1024.0 * 1024.0;
            else if (*end == 'G' || *end == 'g') bytes *= 1024.0 * 1024.0 * 1024.0;
            else if (*end != '\0') return false;
            if (out) *out = static_cast<uint64_t>(bytes);
            return true;
        }
    } catch (...) {
    }
    return false;
}

// 硬上限 = min(全部生效上限); 每个 >0 上限都是硬约束。
// chain 记录实际收紧的钳制源("affinity|cgroup_job|user_cap|"), 机器可查。
uint32_t hard_cap_v1(const ResourceLimitsV1& lim, std::string* chain) {
    uint32_t cap = lim.available_cpus > 0 ? lim.available_cpus : 1u;
    if (chain) *chain += "affinity|";
    const auto record = [&](uint32_t v, const char* tag) {
        if (v == 0 || v >= cap) return;
        cap = v;
        if (chain) { *chain += tag; *chain += "|"; }
    };
    record(lim.cgroup_or_job_cpus, "cgroup_job");
    record(lim.user_max_workers, "user_cap");
    return cap < 1 ? 1u : cap;
}

// 内存预算钳制(低资源不超配): workers ≤ max(1, headroom/per_worker)
uint32_t mem_cap_v1(const ResourceLimitsV1& lim) {
    if (lim.ram_headroom_bytes == 0 || lim.per_worker_mem_bytes == 0) return 0;
    return static_cast<uint32_t>(std::max<uint64_t>(
        1u, lim.ram_headroom_bytes / lim.per_worker_mem_bytes));
}

bool is_heavy_class(const std::string& wc) {
    return wc == "compute" || wc == "memory";
}

}  // namespace

LimitsResult derive_limits_v1(const std::string& hw_json, uint32_t user_max_workers) {
    LimitsResult r;
    ResourceLimitsV1& lim = r.limits;
    lim.user_max_workers = user_max_workers;

    json hw;
    if (!parse_json_text(hw_json, &hw) ||
        !hw.contains("available_logical_cpus") || !hw["available_logical_cpus"].is_number_unsigned()) {
        // 保守回落: 一切未知 → 单 worker 可运行(不阻塞), 全部显式上限清零
        lim.available_cpus = 1;
        r.ok = false;
        r.reason = "hardware_inspect_unavailable_conservative_workers_1";
        return r;
    }
    try {
        std::string fixed_key;
        uint64_t l2 = 0;
        const bool l2_ok = l2_bytes_from_hw(hw, &l2);
        if (has_fixed_cores_claim(hw, &fixed_key)) {
            r.reason = "fixed_claim_rejected:" + fixed_key;
            r.limits.fixed_claim_rejected = true;
            r.limits.available_cpus =
                static_cast<uint32_t>(hw["available_logical_cpus"].get<uint64_t>());
            r.limits.cgroup_or_job_cpus = hw.value("cgroup_cpu_limit", 0u);
            r.limits.numa_nodes = hw.value("numa_nodes", 0u);
            r.limits.ram_bytes = hw.value("ram_bytes", 0ull);
            if (l2_ok) r.limits.l2_bytes = l2;
            r.ok = r.limits.available_cpus > 0;
            if (!r.ok) r.limits.available_cpus = 1;
            return r;
        }
        lim.available_cpus = static_cast<uint32_t>(hw["available_logical_cpus"].get<uint64_t>());
        lim.cgroup_or_job_cpus = hw.value("cgroup_cpu_limit", 0u);
        lim.numa_nodes = hw.value("numa_nodes", 0u);
        lim.ram_bytes = hw.value("ram_bytes", 0ull);
        if (l2_ok) lim.l2_bytes = l2;
        r.ok = lim.available_cpus > 0;
        if (!r.ok) {
            lim.available_cpus = 1;
            r.reason = "available_logical_cpus_zero_conservative_workers_1";
        }
    } catch (...) {
        // 字段类型篡改等: 保守回落, 不抛错不阻塞
        r.limits = ResourceLimitsV1{};
        r.limits.user_max_workers = user_max_workers;
        r.limits.available_cpus = 1;
        r.ok = false;
        r.reason = "hardware_inspect_field_error_conservative_workers_1";
    }
    return r;
}

WorkerAdvice advise_kernel_v1(const std::string& profile_json,
                              const std::string& kernel_id,
                              const std::string& workload_class,
                              const std::string& size_class,
                              const ResourceLimitsV1& limits) {
    WorkerAdvice a;
    a.kernel_id = kernel_id;
    a.used_fixed_claim = limits.fixed_claim_rejected;

    // workload 归一: 空/未知 → compute(保守动态多线程; 重计算禁止单线程)
    std::string wc = workload_class;
    if (wc != "tiny" && wc != "io" && wc != "compute" && wc != "memory") wc = "compute";
    a.workload_class = wc;

    std::string chain;
    if (a.used_fixed_claim) chain += "fixed_claim_rejected|";
    uint32_t cap = hard_cap_v1(limits, &chain);
    const uint32_t mem_cap = mem_cap_v1(limits);
    if (mem_cap > 0 && mem_cap < cap) {
        cap = mem_cap;
        chain += "mem_budget|";
    }

    // profile 行读取(benchmark 选择; 合法性由 CPU-007 装载链保证)。
    // schema 门: 只消费 astrocs.cpu-profile/v2 —— 外域文本(benchmark-report/v1、
    // resource-plan/v1 等)一律视为无行/不可读, 与 CPU-006/本层 plan 结构性隔离。
    bool have_row = false;
    uint32_t pw = 0;
    uint64_t pb = 0;
    if (profile_json.empty()) {
        chain += "no_profile|dynamic_multithread|";
    } else {
        try {
            const json prof = json::parse(profile_json);
            const std::string schema = prof.is_object() && prof.contains("schema") &&
                                               prof["schema"].is_string()
                                           ? prof["schema"].get<std::string>()
                                           : std::string();
            if (schema != "astrocs.cpu-profile/v2") {
                chain += "profile_foreign_schema|";
            } else if (prof.contains("kernels") && prof["kernels"].is_object() &&
                       prof["kernels"].contains(kernel_id) &&
                       prof["kernels"][kernel_id].is_object()) {
                const json& kp = prof["kernels"][kernel_id];
                pw = kp.value("workers", 0u);
                pb = kp.value("block", 0ull);
                if (pw > 0) {
                    have_row = true;
                    chain += "profile|";
                }
            }
        } catch (...) {
            // profile 文本损坏: 按 V8-CPU-002 语义回落(动态多线程), 不阻塞
            chain += "profile_unreadable|";
        }
    }

    if (have_row) {
        a.workers = pw;
        if (pb > 0) {
            a.block = pb;
            a.block_source = "profile";
        }
        // worker=1 红线: compute/memory 且硬上限≥2 时不得为 1(2 核 heavy 1→2 有扩展)
        if (a.workers == 1 && is_heavy_class(wc) && cap >= 2) {
            a.workers = 2;
            chain += "single_thread_rejected_floor2|";
        }
    } else {
        // 无 profile/无行/外域: compute/memory → 动态多线程 workers=硬上限
        // (与 no_profile_policy/V8-CPU-002 一致, 不退 1);
        // tiny/io → 单 worker 捆绑(I/O 串行化合同, worker=1 允许类)。
        a.workers = is_heavy_class(wc) ? cap : 1u;
        chain += is_heavy_class(wc) ? "fallback_dynamic_multithread|"
                                    : "fallback_io_tiny_single_worker|";
    }

    // 硬约束钳制(最后一步; 用户上限不可被任何建议越过)
    if (a.workers > cap) {
        a.workers = cap;
        chain += "capped|";
    }
    if (a.workers < 1) a.workers = 1;

    // worker=1 合规判定: tiny/io 允许; heavy 仅在系统性约束(cap=1)下允许
    a.worker1_allowed = (a.workers == 1) ? (!is_heavy_class(wc) || cap <= 1) : true;
    if (!a.worker1_allowed) chain += "worker1_not_allowed|";

    // block 派生(profile 未给出时): limits.l2_bytes → block_candidates 中位
    // (复用 CPU-006 bench_harness 几何序列, 不复制; elt_size 取 double 域保守 8)。
    if (a.block == 0) {
        if (limits.l2_bytes > 0) {
            const std::vector<uint64_t> cand =
                block_candidates(limits.l2_bytes, 8ull /* FP64 元素保守值 */);
            if (!cand.empty()) {
                a.block = cand[cand.size() / 2];
                a.block_source = "derived_l2";
            }
        }
        if (a.block == 0) a.block_source = "deferred";
    }

    a.reason = chain;
    // 去掉结尾多余的 '|'
    while (!a.reason.empty() && a.reason.back() == '|') a.reason.pop_back();
    // size_class 透传(规格: 不改变选择, 仅记录)
    if (!size_class.empty()) a.reason += "|size_class=" + size_class;
    return a;
}

std::string build_resource_plan_v1(const std::vector<WorkerAdvice>& advice,
                                   const ResourceLimitsV1& limits,
                                   const std::string& build_id) {
    json plan;
    plan["schema"] = "astrocs.resource-plan/v1";
    plan["build_id"] = build_id;
    plan["generated_utc"] = "";   // 由 CLI 层注入时间戳(库层不取钟, 同域一致)
    plan["limits"] = {
        {"available_cpus", limits.available_cpus},
        {"cgroup_or_job_cpus", limits.cgroup_or_job_cpus},
        {"user_max_workers", limits.user_max_workers},
        {"numa_nodes", limits.numa_nodes},
        {"ram_bytes", limits.ram_bytes},
        {"l2_bytes", limits.l2_bytes},
        {"ram_headroom_bytes", limits.ram_headroom_bytes},
        {"per_worker_mem_bytes", limits.per_worker_mem_bytes},
    };
    json rows = json::array();
    for (const auto& a : advice) {
        rows.push_back({
            {"kernel_id", a.kernel_id},
            {"workload_class", a.workload_class},
            {"workers", a.workers},
            {"block", a.block},
            {"block_source", a.block_source},
            {"worker1_allowed", a.worker1_allowed},
            {"reason", a.reason},
        });
    }
    plan["resources"] = rows;
    return plan.dump();
}

std::string worker_grant_trace_v1(const GrantInput& g) {
    json t;
    t["schema"] = "astrocs.worker-grant/v1";
    t["build_id"] = g.build_id;
    t["kernel_id"] = g.kernel_id;
    t["provider"] = g.provider;
    t["isa"] = g.isa;
    t["planned_workers"] = g.planned_workers;
    t["granted_workers"] = g.granted_workers;
    t["block"] = g.block;
    t["planned_equals_granted"] = (g.planned_workers == g.granted_workers);
    if (!g.reason.empty()) t["reason"] = g.reason;
    return t.dump();
}

}  // namespace astrocs::backend_host
