# CPU-002 计划: 能力/配额检测

## 目标
Linux：CPUID、OSXSAVE/XGETBV、sched affinity、cpuset、cgroup v1/v2 quota 取交集。
输出 vendor/family/model/stepping、features、XCR0、cache/NUMA、logical_available、
quota signature。不得用 CPU 型号白名单或仅 hardware_concurrency。

## 既有基础（lib/backend_host/hardware_inspect.cpp 已实现）
- CPUID feature bits（cpu_features.h：SSE2/SSE4_1/AVX/AVX2/FMA/AVX512F，位定义冻结）
- OSXSAVE + XGETBV(XCR0) 实测（仅 OS 保存对应状态才置位）
- sched_getaffinity 有效 CPU + cgroup v2 cpu.max quota/period ∩ 取交集
- vendor/family/model/stepping、microcode、cache/NUMA、page size、OS
- feature_names、logical_cpus_configured、affinity 列表

## 本任务补充
1. **cgroup v1 回退**：v2 无 cpu.max 时探测 `/sys/fs/cgroup/cpu/cpu.cfs_quota_us`
   + `cpu.cfs_period_us` → 有效 CPU 上限（与 affinity ∩）。
2. **quota_signature**：affinity_count|cgroup_limit|avail|feature_bits|xcr0 的
   SHA-256 指纹 → 能力/配额状态可复现校验（benchmark/profile 路由依赖）。

## 验收
- `astrocs hardware inspect --json` 输出含 12 个必需字段（vendor/family/model/
  stepping/feature_bits/xcr0/available_logical_cpus/affinity_count/cgroup_cpu_limit/
  quota_signature/numa_nodes/os）
- feature_bits=63（本机 SSE2..AVX512F 全支持）、xcr0=255、available=2
- 与 CPU-001 同 commit 交付；core ctest 16/16 无回归
