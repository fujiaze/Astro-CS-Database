// lib/backend_host/cpu_routing.h — CPU-004/CPU-005 逐 kernel 路由与 provider 数值自测
// 规格: CPU-004; CPU-005; 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md §2/§3
// Runtime 按 kernel ID + workload class 读取 profile; 核对 arch/CPU/XCR0/OS ABI/quota/
// runtime/provider/benchmark schema/build IDs。无 profile、stale、损坏或 unsupported
// provider 时逐 kernel 回 baseline; 保守意味着最低 ISA, 不意味着单线程。
//
// CPU-005 (provider 数值自测与路由表; 04_CPU_RESOURCE_TASKS.md CPU-005):
//   固定的执行序 query → self_test → eligible → benchmark → select;
//   路由以 kernel_id 粒度 (禁止全局 preferred_isa, 禁止机械指令集堆砌);
//   profile 不合格 / build/CPU/OS hash 变化 / 缺 benchmark / NaN 或数值 mismatch /
//   低收益 (相对冻结噪声门限不足) → 一律 baseline。
//   决策各阶段证据 (stage/detail/provider/self_test_sha256/gain) 逐项可审计,
//   运行 trace 以 JSON 路由表输出, 显示每个 kernel 实际 provider。
#ifndef ASTROCS_CPU_ROUTING_H
#define ASTROCS_CPU_ROUTING_H

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::backend_host {

// 单 kernel 路由决策
struct KernelRoute {
    std::string kernel_id;        // 如 "calibration-pixel-transform"
    std::string provider;         // "baseline"|"avx2"|"avx512"
    uint32_t workers = 1;         // ≥1; available≥2 时 heavy 不退 1
    uint64_t block = 1;
    std::string fallback_reason;  // 空=profile 驱动; 非空=回退原因(诊断)
    std::string self_test_sha256; // 该 provider 的 self-test hash(64hex)
};

// profile 校验结果 (CPU-004): 整体是否可信任
struct ProfileVerdict {
    bool valid = false;           // schema/host/build 一致
    std::string stale_reason;     // 非空=stale/损坏原因
};

// ── CPU-005 逐 kernel 决策证据 (路由表 / trace 单行) ──
struct KernelDecision {
    std::string kernel_id;
    std::string provider;         // 决策结果 "baseline"|"avx2"|"avx512"
    std::string stage;            // query|self_test|eligible|benchmark|select
    bool ok = false;              // 该 kernel 决策是否为非 baseline 且证据完整
    std::string detail;           // 阶段证据 / 退回原因 (可读 + 机器可查)
    double median_ns = 0.0, mad_ns = 0.0;   // 决策所依据的 benchmark 行
    double gain_rel = 0.0;        // select 阶段相对 baseline 收益 (正=快)
    std::string self_test_sha256; // 所选 provider 的 self-test hash (64hex)
    std::string fallback_reason;  // 非空 = 退回 baseline 原因
};

// 实际加载的 provider 证据 (driver/加载层经 query+self_test 后提供; 本层不重复 dlopen)。
// query/self_test 失败 → ok=false + 对应 reason; kernels = provider 表内实际注册 kernel_id。
struct ProviderEvidence {
    std::string id;                 // "baseline"|"avx2"|"avx512"
    bool query_ok = false;          // query 握手/能力门通过 (os_safe 平面)
    bool self_test_ok = false;      // provider self_test 通过
    std::string self_test_sha256;   // 实际 DSO/build 身份 hash (64hex; host 实测)
    std::vector<std::string> kernels;  // kernel_list 表内 kernel_id
    std::string reason;             // 非空 = query/self_test 失败原因
};

// 实测 benchmark 候选行 (driver 或 CPU-006 采集; 单位 ns)。
struct BenchRow {
    std::string provider;           // "baseline"|"avx2"|"avx512"
    double median_ns = 0.0;
    double mad_ns = 0.0;
    bool oracle_pass = true;        // 数值对照 (容差 2e-4) 通过
};

// ── CPU-005 build/CPU/OS 身份校验 (profile 绑定; 15 §2/§3) ──
// profile 记录 host/build 身份 (vendor|family|model|stepping|xcr0|os_abi|
// source_commit|benchmark_binary_sha256 + features); 当前机器由 hw_json 给出。
// 覆盖 CPU-004 validate (arch/quota/available) 之外的: CPU 身份字段、XCR0、
// OS 名、feature 包含、source_commit、benchmark 二进制 hash。
struct IdentityCheck {
    bool valid = false;
    std::string reason;             // 非空=失效原因
};
IdentityCheck check_profile_identity_v1(const std::string& profile_json,
                                        const std::string& hw_json,
                                        const std::string& current_commit);

// ── CPU-005 benchmark 行完整性 (stage=benchmark) ──
// profile kernels[kernel_id] 必须: 存在; correctness_test=="oracle:pass";
// fallback_reason 空; median>0 且 median/mad 均为有限数 (NaN/Inf → mismatch)。
// 返回 false + reason (缺 benchmark / NaN mismatch / oracle 失败)。
bool profile_kernel_benchmark_valid(const std::string& profile_json,
                                    const std::string& kernel_id,
                                    std::string* reason_out,
                                    double* median_out, double* mad_out);

// ── CPU-005 低收益/噪声裕量判定 (stage=select) ──
// 纯函数: 给定 baseline 与候选实测 median (ns), 计算候选相对 baseline 收益
//   gain = (baseline - cand) / baseline;  正=候选更快。
// gain < min_gain_rel (冻结噪声门限) 或 baseline/cand 非有限正 → insufficient。
struct GainCheck {
    bool sufficient = false;
    double gain_rel = 0.0;
    std::string reason;
};
GainCheck check_gain_sufficient_v1(double baseline_median_ns,
                                   double cand_median_ns,
                                   double min_gain_rel);

// ── CPU-005 全流程决策 (query→self_test→eligible→benchmark→select; 单 kernel) ──
// providers: 已 dlopen + query 的 provider 证据 (含 self_test 结果与 kernel 表)。
// live_rows: 可选实测候选 (该 kernel 现场 spot benchmark); 为 nullptr 时 benchmark
//            阶段只校验 profile 记录行 (结构完整性), 不重测 —— select 取 profile 行;
//            非空时 benchmark 阶段取实测行, select 按噪声门限在 baseline 之上择优
//            (任何候选相对 baseline 收益 < min_gain_rel → 保持 baseline; 无候选
//            或 NaN/非有限 → baseline)。现场/记录不一致时以证据更完整者为准并在
//            detail 注明。
// 顺序固定: query → self_test → eligible → benchmark → select; 任一阶段失败即回
// baseline 并记录 stage/fallback_reason (profile 不合格/build/CPU/OS hash 变化/
// 缺 benchmark/NaN/低收益 一律 baseline)。纯函数; threadsafe=yes。
KernelDecision decide_kernel_v1(const std::string& profile_json,
                                const std::string& kernel_id,
                                const std::string& hw_json,
                                const std::string& current_commit,
                                const std::vector<ProviderEvidence>& providers,
                                const std::vector<BenchRow>* live_rows,
                                double min_gain_rel);

// ── CPU-005 路由表 trace ──
// 对 registered_kernel_ids_v1 全表或 profile kernels 键集逐 kernel decide, 输出
// JSON 对象 {"routes": {"<kernel_id>": {provider,stage,ok,detail,gain_rel,
// self_test_sha256,fallback_reason,median_ns,mad_ns}}, "decision":"…"}。
// kernel_ids 为空 → 取 profile kernels 键集 (缺 profile → 空表)。
std::string build_route_table_v1(const std::string& profile_json,
                                 const std::string& hw_json,
                                 const std::string& current_commit,
                                 const std::vector<ProviderEvidence>& providers,
                                 const std::vector<BenchRow>* live_rows,
                                 double min_gain_rel,
                                 const std::vector<std::string>& kernel_ids);

// 单 kernel 决策 JSON 序列化 (trace 消费; 不抛异常)。
std::string kernel_decision_to_json(const KernelDecision& d);

// ── CPU-004 兼容层 (保留原签名) ──
ProfileVerdict validate_profile_v2_for_machine(const std::string& profile_json,
                                               const std::string& current_commit,
                                               const std::string& hw_json);
bool route_kernel_from_profile(const std::string& profile_json,
                               const std::string& kernel_id,
                               const std::string& hw_json,
                               KernelRoute* out_route);
KernelRoute conservative_route(const std::string& kernel_id, uint32_t available_cpus);

// 固定 kernel_id 集 (12 注册 kernel; 与 backend_table.inc / profile_gen_v2 kSpecs 对齐)
extern const std::vector<std::string>& registered_kernel_ids_v1();

}  // namespace astrocs::backend_host

#endif  // ASTROCS_CPU_ROUTING_H
