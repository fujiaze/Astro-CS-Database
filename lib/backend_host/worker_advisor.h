// lib/backend_host/worker_advisor.h — CPU-008 自适应线程建议(资源面)
// 规格(V7.1 FINAL3 04_CPU_RESOURCE_TASKS.md CPU-008):
//   根据探测核数、进程/job/NUMA/内存预算和 profile 选择 worker 上限与 block size;
//   不能写死 2/16/32 或无视用户上限。将选择写入 plan, 实际 granted 写 trace。
//   验收: cgroup/Windows job 限制模拟; 低资源 Linux 不超配; 2 核合成 heavy
//   1→2 worker 有可测扩展; worker=1 仅 tiny/I/O 允许。
// 验收关键词: no fixed cores —— 本层任何输出都由输入派生, 无任何固定核数
//   (2/16/32 等)分支; 输入侧出现的 fixed_cores/fixed_workers/workers_fixed
//   声明一律拒绝使用(reason 记 fixed_claim_rejected), 选择与该声明无关。
// 补充(V8.1 03_P0_REMEDIATION_TASKS.md V8-CPU-003): trace 记录 build ID、
//   kernel ID、ISA、worker budget —— 由 worker_grant_trace_v1 承载。
//
// 层次合同(不复制校验链): 本文件是 CPU-006 bench_report(逐 kernel×workers×block
// 实测)与 CPU-007 profile_store(存储生命周期)之上的**资源建议层**。
//   - profile 行读取: 只按 kernels[kernel_id].{workers,block} 取 benchmark 选择,
//     合法性由 CPU-007 装载链保证; 本层不校验 schema/身份。
//   - block 候选派生: 复用 bench_harness::block_candidates(L2 几何序列), 不复制。
//   - 无 profile/失配: 按 V8-CPU-002 语义回落 —— generic ISA + 动态多线程,
//     workers=可用上限(与 bench_harness::no_profile_policy 一致), 本层不退 1。
// 与 CPU-006 report(astrocs.benchmark-report/v1)/CPU-007 store(astrocs.cpu-profile/v2)
// 结构性隔离: 本层产出的 plan/trace 是新面 astrocs.resource-plan/v1 与
// astrocs.worker-grant/v1, 三种 schema 互不渗透。
//
// worker=1 语义(规格红线"重计算禁止单线程"与"worker=1 仅 tiny/I/O 允许"):
//   - tiny/io 类: workers=1 允许(I/O 捆绑或微任务, 无重计算可并行);
//   - compute/memory 类且硬上限≥2: workers 不得为 1 —— profile 实测行给出 1 时
//     floor 到 2(reason 记 single_thread_rejected_floor2); 仅系统性资源约束
//     (用户上限=1 / 内存预算=1 / 资源未知保守回落)允许 workers=1, reason 必记。
#ifndef ASTROCS_WORKER_ADVISOR_H
#define ASTROCS_WORKER_ADVISOR_H

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs::backend_host {

// ── 资源上限(全部派生; 零固定核数) ──
// available_cpus: 有效 affinity∩显式限制(与 hardware_inspect available_logical_cpus
//   同义; 调用方也可直接注入探测值, 便于 cgroup/job 限制模拟)。
// cgroup_or_job_cpus: cgroup cpu.max 折算核数或 Windows Job Object 限制;
//   0=无显式限制(affinity 即全部)。
// user_max_workers: 用户显式上限; 0=未设置; >0 时不得被任何路径无视。
// numa_nodes/ram_bytes/l2_bytes: 画像透传(0=未知); l2_bytes 由 hw["cache"]
//   level=="2" 的 size 派生, 供 block fallback 派生(block_candidates 中位)。
// ram_headroom_bytes: 本 run 内存预算(调用方从 ram 派生或注入; 0=不做内存钳制)。
// per_worker_mem_bytes: 每 worker 工作集估计(0=未知 → 不做内存钳制)。
//   内存钳制: workers ≤ max(1, ram_headroom/per_worker_mem) —— "低资源不超配"。
struct ResourceLimitsV1 {
    uint32_t available_cpus = 0;
    uint32_t cgroup_or_job_cpus = 0;
    uint32_t user_max_workers = 0;
    uint32_t numa_nodes = 0;
    uint64_t ram_bytes = 0;
    uint64_t l2_bytes = 0;
    uint64_t ram_headroom_bytes = 0;
    uint64_t per_worker_mem_bytes = 0;
    bool fixed_claim_rejected = false;  // 输入侧含固定核数声明且已拒用
};

// 从 hardware_inspect JSON(hw_json) 派生 limits + 用户上限。
// hw_json 解析失败/缺 available_logical_cpus → ok=false + 保守 limits
// (available_cpus=1, 全部上限清零), reason 记原因; 不抛错不阻塞。
// hw_json 中 fixed_cores/fixed_workers/workers_fixed 字段: 忽略并在 reason 记
// fixed_claim_rejected(不改变任何派生值)。
struct LimitsResult {
    ResourceLimitsV1 limits;
    bool ok = false;
    std::string reason; // ok=false 时的原因 / ok=true 时为空或 fixed_claim_rejected
};
LimitsResult derive_limits_v1(const std::string& hw_json, uint32_t user_max_workers);

// ── 逐 kernel worker/block 建议 ──
// workload_class 词表(与 profile v2 workload_class 对齐): "tiny"|"compute"|
//   "memory"|"io"; 空/未知 → 按 "compute"(保守动态多线程)。
// size_class: "small"|"medium"|"large"|"" (透传到 reason; 不改变选择)。
// 选择链(机器可查, "|"-连接):
//   profile   profile 行 workers/block(benchmark 实测选择)
//   fallback  无 profile/无行 → workers=硬上限(动态多线程, 不退 1)
//   cgroup/job/user/mem 生效的钳制源逐个记录
//   single_thread_rejected_floor2  profile 行 workers=1 但类为 compute/memory
//   fixed_claim_rejected           输入含固定核数声明, 已拒用
// block: profile 行 block>0 → 用之; 否则 hw L2 可解析 → block_candidates 中位;
//   否则 0(运行时派生, plan 中明确标记 block_source)。
struct WorkerAdvice {
    std::string kernel_id;
    std::string workload_class;   // 归一化后 class(tiny|compute|memory|io)
    uint32_t workers = 1;         // ≥1
    uint64_t block = 0;           // 0=运行时派生
    std::string reason;           // 选择链
    bool worker1_allowed = false; // workers=1 是否合规(tiny/io 或系统性约束)
    bool used_fixed_claim = false;// 输入含固定核数声明且被拒
    std::string block_source;     // "profile"|"derived_l2"|"deferred"
};
WorkerAdvice advise_kernel_v1(const std::string& profile_json,   // 空串=无 profile
                              const std::string& kernel_id,
                              const std::string& workload_class,
                              const std::string& size_class,
                              const ResourceLimitsV1& limits);

// ── 选择写入 plan(规格: "将选择写入 plan") ──
// 输出 astrocs.resource-plan/v1 JSON: limits 摘要 + 每条 advice 的 resources 行。
// plan 是"建议上限"记录, 不是观测; 实际占用由 worker_grant_trace_v1 落 trace。
std::string build_resource_plan_v1(const std::vector<WorkerAdvice>& advice,
                                   const ResourceLimitsV1& limits,
                                   const std::string& build_id);

// ── 实际 granted 写 trace(规格: "实际 granted 写 trace") ──
// V8-CPU-003: trace 记录 build ID、kernel ID、ISA、worker budget。
// granted_workers 由执行层(Runtime lease 实际发放)传入 —— 本层只做记录与
// 事实序列化, 不代替观测(观测面在 MON-001/V8-MON-001); planned≠granted
// 时 plan/trace 对照即可发现, 不静默改写。
struct GrantInput {
    std::string build_id;
    std::string kernel_id;
    std::string provider;        // 实际 provider(如 "baseline")
    std::string isa;             // 实际 ISA(如 "sse2"/"avx2"/"avx512")
    uint32_t planned_workers = 0;  // plan 建议(worker budget)
    uint32_t granted_workers = 0;  // 实际 granted(执行层观测传入)
    uint64_t block = 0;
    std::string reason;          // granted≠planned 时的原因等
};
std::string worker_grant_trace_v1(const GrantInput& g);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_WORKER_ADVISOR_H
