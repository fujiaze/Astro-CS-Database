// tests/unit/cpu008_worker_advisor_test.cpp — CPU-008 (V7.1) 自适应线程建议单测
// 覆盖(worker_advisor.h 合同):
//   正例: limits 派生(affinity∩cgroup/job) / profile 行驱动 workers+block /
//         无 profile 动态多线程回落 / L2 派生 block / plan(astrocs.resource-plan/v1)
//         与 granted trace(astrocs.worker-grant/v1, V8-CPU-003 字段)序列化。
//   负向样例(全部"不被使用"):
//     - no fixed cores: fixed_cores/fixed_workers/workers_fixed 声明拒用, 选择不变;
//       profile workers=32 在 cap=2 下被钳制(不写死 16/32)。
//     - cgroup/Windows job 限制模拟: hw cgroup_cpu_limit=2 → workers≤2(低资源不超配)。
//     - 用户上限不无视: user_max_workers=3, profile=8 → 3; user_max=1 → workers=1
//       仅在用户/资源系统性约束下允许。
//     - worker=1 仅 tiny/I/O 允许: compute/memory + cap≥2 + profile workers=1 →
//       floor 2(single_thread_rejected_floor2); tiny/io → 1 合法。
//     - 内存预算: headroom/per_worker 钳制, 低内存不超配。
//     - profile 域隔离: benchmark-report/v1 文本作 profile 输入 → 视为无行回落,
//       resource-plan/v1 与 cpu-profile/v2 互不渗透。
#include "worker_advisor.h"

#include <cstdio>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using astrocs::backend_host::ResourceLimitsV1;
using astrocs::backend_host::LimitsResult;
using astrocs::backend_host::WorkerAdvice;
using astrocs::backend_host::GrantInput;
using astrocs::backend_host::derive_limits_v1;
using astrocs::backend_host::advise_kernel_v1;
using astrocs::backend_host::build_resource_plan_v1;
using astrocs::backend_host::worker_grant_trace_v1;

static int failures = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

namespace {

// 最小 hw 画像(与 hardware_inspect 字段名同构)
std::string make_hw(uint32_t avail, uint32_t cgroup = 0,
                    const char* extra_key = nullptr, int extra_val = 0) {
    nlohmann::json j;
    j["kind"] = "astrocs_hardware_inspect";
    j["available_logical_cpus"] = avail;
    j["affinity_count"] = avail;
    j["cgroup_cpu_limit"] = cgroup;   // 0=无显式限制
    j["numa_nodes"] = 1;
    j["ram_bytes"] = 8ull << 30;
    if (extra_key) j[extra_key] = extra_val;
    return j.dump();
}

// 最小 profile(v2 结构; 本层只读 kernels[id].{workers,block}, 不校验身份)
std::string make_profile(const std::string& kernel_id, uint32_t workers, uint64_t block) {
    nlohmann::json j;
    j["schema"] = "astrocs.cpu-profile/v2";
    j["kernels"][kernel_id] = {
        {"provider", "baseline"},
        {"workers", workers},
        {"block", block},
        {"workload_class", "compute"},
    };
    return j.dump();
}

const std::string kKernel = "calibration-pixel-transform";

}  // namespace

int main() {
  // ── 1. derive_limits_v1: 正常画像 ──
  {
    const LimitsResult r = derive_limits_v1(make_hw(4), 0);
    CHECK(r.ok);
    CHECK(r.limits.available_cpus == 4);
    CHECK(r.limits.cgroup_or_job_cpus == 0);
    CHECK(!r.limits.fixed_claim_rejected);
  }

  // ── 2. cgroup/Windows job 限制模拟: hw cgroup_cpu_limit=2 ──
  //   (job object 语义同字段: 显式进程级上限; affinity=8 但显式限制 2)
  {
    const LimitsResult r = derive_limits_v1(make_hw(8, 2), 0);
    CHECK(r.ok);
    CHECK(r.limits.available_cpus == 8);
    CHECK(r.limits.cgroup_or_job_cpus == 2);
    const WorkerAdvice a = advise_kernel_v1("", kKernel, "compute", "medium", r.limits);
    CHECK(a.workers == 2);                    // 低资源不超配: 8 核机器不超发 2
    CHECK(a.reason.find("cgroup_job") != std::string::npos);
    CHECK(a.reason.find("fallback_dynamic_multithread") != std::string::npos);
  }

  // ── 3. 用户上限不无视 ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 8;
    lim.user_max_workers = 3;
    const WorkerAdvice a = advise_kernel_v1(make_profile(kKernel, 8, 4096),
                                            kKernel, "compute", "medium", lim);
    CHECK(a.workers == 3);                    // profile=8 不得越过用户上限
    CHECK(a.reason.find("user_cap") != std::string::npos);
    CHECK(a.reason.find("capped") != std::string::npos);
  }

  // ── 4. derive_limits_v1: 损坏/缺失 hw 保守回落(不抛错) ──
  {
    const LimitsResult r1 = derive_limits_v1("", 0);
    CHECK(!r1.ok);
    CHECK(r1.limits.available_cpus == 1);
    CHECK(r1.reason.find("conservative") != std::string::npos);
    const LimitsResult r2 = derive_limits_v1("{broken json", 0);
    CHECK(!r2.ok && r2.limits.available_cpus == 1);
    const LimitsResult r3 = derive_limits_v1(make_hw(0), 0);
    CHECK(!r3.ok && r3.limits.available_cpus == 1);
    // 字段类型篡改(available 为负数) → 保守
    const LimitsResult r4 = derive_limits_v1("{\"available_logical_cpus\": -1}", 0);
    CHECK(!r4.ok && r4.limits.available_cpus == 1);
  }

  // ── 5. no fixed cores: 固定核数声明拒用(负向样例) ──
  {
    const LimitsResult rf = derive_limits_v1(make_hw(4, 0, "fixed_cores", 16), 0);
    CHECK(rf.ok);
    CHECK(rf.limits.fixed_claim_rejected);
    CHECK(rf.reason.find("fixed_claim_rejected:fixed_cores") != std::string::npos);
    CHECK(rf.limits.available_cpus == 4);     // 派生值不受声明影响
    const WorkerAdvice af = advise_kernel_v1(make_profile(kKernel, 4, 0),
                                             kKernel, "compute", "medium", rf.limits);
    CHECK(af.used_fixed_claim);
    CHECK(af.reason.find("fixed_claim_rejected") != std::string::npos);
    CHECK(af.workers == 4);                   // 选择与 fixed_cores=16 声明无关
    // workers_fixed 词面同样拒用
    const LimitsResult rw = derive_limits_v1(make_hw(4, 0, "workers_fixed", 2), 0);
    CHECK(rw.limits.fixed_claim_rejected);
    CHECK(rw.reason.find("workers_fixed") != std::string::npos);
    // fixed 声明值(32)不得使选择超配
    const LimitsResult r32 = derive_limits_v1(make_hw(4, 0, "fixed_workers", 32), 0);
    const WorkerAdvice a32 = advise_kernel_v1("", kKernel, "compute", "medium", r32.limits);
    CHECK(a32.workers == 4);                  // 不是 32: no fixed cores
  }

  // ── 6. 不写死 2/16/32: 上限随输入派生(扫描) ──
  for (uint32_t avail = 1; avail <= 48; ++avail) {
    ResourceLimitsV1 lim;
    lim.available_cpus = avail;
    const WorkerAdvice a = advise_kernel_v1("", kKernel, "compute", "medium", lim);
    CHECK(a.workers == avail);                // 无 profile: workers=有效上限, 非固定值
    CHECK(a.workers != 2 || avail == 2);
  }

  // ── 7. profile 行驱动: workers/block 透传 ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 16;
    const WorkerAdvice a = advise_kernel_v1(make_profile(kKernel, 3, 4096),
                                            kKernel, "compute", "medium", lim);
    CHECK(a.workers == 3);
    CHECK(a.block == 4096);
    CHECK(a.block_source == "profile");
    CHECK(a.reason.find("profile|") != std::string::npos);
    // 16 核机器不写死 16: 尊重 profile benchmark 选择
    const WorkerAdvice b = advise_kernel_v1(make_profile(kKernel, 16, 4096),
                                            kKernel, "compute", "medium", lim);
    CHECK(b.workers == 16);
  }

  // ── 8. worker=1 仅 tiny/I/O 允许(负向样例: heavy 单线程被拒) ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 4;
    // compute + profile workers=1 → floor 2
    const WorkerAdvice a = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                            kKernel, "compute", "medium", lim);
    CHECK(a.workers == 2);
    CHECK(a.reason.find("single_thread_rejected_floor2") != std::string::npos);
    // memory 类同样红线
    const WorkerAdvice m = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                            kKernel, "memory", "large", lim);
    CHECK(m.workers == 2);
    // tiny/io → 1 合法
    const WorkerAdvice t = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                            kKernel, "tiny", "small", lim);
    CHECK(t.workers == 1 && t.worker1_allowed);
    const WorkerAdvice io = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                             kKernel, "io", "small", lim);
    CHECK(io.workers == 1 && io.worker1_allowed);
    // 未知 class 归一 compute(保守动态多线程) → 单线程同样被拒
    const WorkerAdvice u = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                            kKernel, "", "medium", lim);
    CHECK(u.workload_class == "compute");
    CHECK(u.workers == 2);
    // 系统性约束(cap=1)下 heavy=1 允许但显式标记
    ResourceLimitsV1 lim1;
    lim1.available_cpus = 1;
    const WorkerAdvice s = advise_kernel_v1(make_profile(kKernel, 1, 0),
                                            kKernel, "compute", "medium", lim1);
    CHECK(s.workers == 1 && s.worker1_allowed);
  }

  // ── 9. 用户上限=1: 用户主权优先, 允许 workers=1(不无视用户上限) ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 8;
    lim.user_max_workers = 1;
    const WorkerAdvice a = advise_kernel_v1(make_profile(kKernel, 4, 0),
                                            kKernel, "compute", "medium", lim);
    CHECK(a.workers == 1);
    CHECK(a.worker1_allowed);
    CHECK(a.reason.find("user_cap") != std::string::npos);
  }

  // ── 10. 内存预算钳制: 低资源不超配 ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 64;
    lim.ram_headroom_bytes = 1ull << 30;      // 1GiB 预算
    lim.per_worker_mem_bytes = 512ull << 20;  // 每 worker 512MiB
    const WorkerAdvice a = advise_kernel_v1("", kKernel, "compute", "medium", lim);
    CHECK(a.workers == 2);                    // 1GiB/512MiB=2, 不是 64
    CHECK(a.reason.find("mem_budget") != std::string::npos);
    // 预算不足以支撑 1 个 worker 的完整工作集 → 保守 1(系统性约束)
    ResourceLimitsV1 lim2;
    lim2.available_cpus = 64;
    lim2.ram_headroom_bytes = 256ull << 20;
    lim2.per_worker_mem_bytes = 512ull << 20;
    const WorkerAdvice b = advise_kernel_v1("", kKernel, "compute", "medium", lim2);
    CHECK(b.workers == 1 && b.worker1_allowed);
    // 每 worker 未知(0) → 不做内存钳制
    ResourceLimitsV1 lim3;
    lim3.available_cpus = 64;
    lim3.ram_headroom_bytes = 1ull << 30;
    const WorkerAdvice c = advise_kernel_v1("", kKernel, "compute", "medium", lim3);
    CHECK(c.workers == 64);
  }

  // ── 11. block 派生: L2 → block_candidates 中位; 无 L2 → deferred ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 4;
    lim.l2_bytes = 1ull << 20;                // 1MiB
    const WorkerAdvice a = advise_kernel_v1("", kKernel, "compute", "medium", lim);
    // block_candidates(1MiB, 8): base=1MiB/64=16384; 序列 {1024,4096,16384,65536};
    // 中位=index 2 → 16384(独立于被测代码路径计算)
    CHECK(a.block == 16384);
    CHECK(a.block_source == "derived_l2");
    ResourceLimitsV1 lim0;
    lim0.available_cpus = 4;
    const WorkerAdvice b = advise_kernel_v1("", kKernel, "compute", "medium", lim0);
    CHECK(b.block == 0 && b.block_source == "deferred");
  }

  // ── 12. profile 域隔离(负向样例): report/v1 文本作 profile 输入 → 无行回落 ──
  {
    const std::string report = nlohmann::json{
        {"schema", "astrocs.benchmark-report/v1"},
        {"kernels", {{"calibration-pixel-transform", {{"workers", 32}}}}},
    }.dump();
    ResourceLimitsV1 lim;
    lim.available_cpus = 4;
    const WorkerAdvice a = advise_kernel_v1(report, kKernel, "compute", "medium", lim);
    CHECK(a.workers == 4);                    // report 不是 profile: 回落动态多线程
    CHECK(a.reason.find("fallback_dynamic_multithread") != std::string::npos);
    // 损坏 profile 文本 → 同样回落, 不抛错
    const WorkerAdvice b = advise_kernel_v1("{not json", kKernel, "compute", "medium", lim);
    CHECK(b.workers == 4);
    CHECK(b.reason.find("profile_unreadable") != std::string::npos);
    // 行存在但 workers=0(非法行) → 视为无行
    const WorkerAdvice c = advise_kernel_v1(make_profile(kKernel, 0, 0),
                                            kKernel, "compute", "medium", lim);
    CHECK(c.workers == 4);
  }

  // ── 13. plan: 选择写入 plan(astrocs.resource-plan/v1) ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 8;
    lim.user_max_workers = 4;
    std::vector<WorkerAdvice> advice;
    advice.push_back(advise_kernel_v1(make_profile(kKernel, 3, 4096),
                                      kKernel, "compute", "medium", lim));
    advice.push_back(advise_kernel_v1("", "io-sidecar", "io", "small", lim));
    const std::string plan = build_resource_plan_v1(advice, lim, "0.11.0-alpha.test");
    const nlohmann::json pj = nlohmann::json::parse(plan);
    CHECK(pj["schema"] == "astrocs.resource-plan/v1");
    CHECK(pj["build_id"] == "0.11.0-alpha.test");
    CHECK(pj["limits"]["available_cpus"] == 8);
    CHECK(pj["limits"]["user_max_workers"] == 4);
    CHECK(pj["resources"].is_array() && pj["resources"].size() == 2);
    CHECK(pj["resources"][0]["kernel_id"] == kKernel);
    CHECK(pj["resources"][0]["workers"] == 3);
    CHECK(pj["resources"][0]["block"] == 4096);
    CHECK(pj["resources"][1]["workload_class"] == "io");
    CHECK(pj["resources"][1]["workers"] == 1);  // io 允许 1
    // plan 不是 profile/report: schema 互不渗透
    CHECK(plan.find("astrocs.cpu-profile/v2") == std::string::npos);
    CHECK(plan.find("astrocs.benchmark-report/v1") == std::string::npos);
  }

  // ── 12b. tiny/io 无 profile 行 → 单 worker 捆绑(缺省 io 语义) ──
  {
    ResourceLimitsV1 lim;
    lim.available_cpus = 8;
    const WorkerAdvice t = advise_kernel_v1("", "io-sidecar", "io", "small", lim);
    CHECK(t.workers == 1 && t.worker1_allowed);
    CHECK(t.reason.find("fallback_io_tiny_single_worker") != std::string::npos);
    const WorkerAdvice c = advise_kernel_v1("", kKernel, "compute", "medium", lim);
    CHECK(c.workers == 8);                    // heavy 仍动态多线程
  }

  // ── 14. trace: 实际 granted 写 trace(V8-CPU-003: build ID/kernel/ISA/budget) ──
  {
    GrantInput g;
    g.build_id = "0.11.0-alpha.test";
    g.kernel_id = kKernel;
    g.provider = "baseline";
    g.isa = "sse2";
    g.planned_workers = 2;
    g.granted_workers = 2;
    g.block = 4096;
    const nlohmann::json tj = nlohmann::json::parse(worker_grant_trace_v1(g));
    CHECK(tj["schema"] == "astrocs.worker-grant/v1");
    CHECK(tj["build_id"] == "0.11.0-alpha.test");
    CHECK(tj["kernel_id"] == kKernel);
    CHECK(tj["provider"] == "baseline");
    CHECK(tj["isa"] == "sse2");
    CHECK(tj["planned_workers"] == 2);
    CHECK(tj["granted_workers"] == 2);
    CHECK(tj["planned_equals_granted"] == true);
    // granted≠planned: 如实记录, 不静默改写
    GrantInput g2 = g;
    g2.granted_workers = 1;
    g2.reason = "lease_denied_partial";
    const nlohmann::json t2 = nlohmann::json::parse(worker_grant_trace_v1(g2));
    CHECK(t2["granted_workers"] == 1);
    CHECK(t2["planned_equals_granted"] == false);
    CHECK(t2["reason"] == "lease_denied_partial");
    CHECK(t2["planned_workers"] == 2);        // planned 保留, 可对照发现差异
  }

  if (failures == 0) {
    std::printf("cpu008_worker_advisor_test: ALL PASS\n");
    return 0;
  }
  std::printf("cpu008_worker_advisor_test: %d FAILURE(S)\n", failures);
  return 1;
}
