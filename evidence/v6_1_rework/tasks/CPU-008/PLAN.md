# CPU-008 执行计划（V8.1 reverify 流程）

任务: CPU-008 自适应线程建议（SA-CPU-09 → 本轮 SubAgent SA-CPU8, thread-profile 锁内）
规格源: 工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/tasks/04_CPU_RESOURCE_TASKS.md CPU-008 小节;
        工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/tasks/03_P0_REMEDIATION_TASKS.md §V8-CPU-003。
基线: CPU-006 bench_report (5d4ab05d) + CPU-007 profile_store (410165f1)。

## 规格要点（原文摘要）
- 根据探测核数、进程/job/NUMA/内存预算和 profile 选择 worker 上限与 block size;
  不能写死 2/16/32 或无视用户上限; 将选择写入 plan, 实际 granted 写 trace。
- 验收: cgroup/Windows job 限制模拟; 低资源 Linux 不超配; 2 核合成 heavy 1→2 worker
  有可测扩展; worker=1 仅 tiny/I/O 允许。
- V8-CPU-003: trace 记录 build ID、kernel ID、ISA、worker budget; profile 修改必须
  改变实际选择。
- 验收关键词: no fixed cores。

## 落地面（新层, 结构性隔离）
lib/backend_host/worker_advisor.{h,cpp} —— 资源建议层:
- derive_limits_v1: hardware_inspect JSON(hw) + 用户上限 → ResourceLimitsV1
  (affinity/cgroup_or_job/numa/ram/l2); hw 损坏/缺失/字段篡改 → 保守 available=1
  不抛错; hw 中 fixed_cores/fixed_workers/workers_fixed 声明一律拒用(记录)。
- advise_kernel_v1: 硬上限=min(affinity, cgroup/job, user_cap, mem_budget);
  profile 行(仅 astrocs.cpu-profile/v2 schema) workers/block 透传为基准建议;
  无行/外域/损坏 → compute/memory 动态多线程回落(不退 1, 与 no_profile_policy/
  V8-CPU-002 一致), tiny/io → 单 worker 捆绑(worker=1 允许类);
  compute/memory + cap≥2 时 profile workers=1 结构性 floor 到 2
  (single_thread_rejected_floor2 —— 2 核 heavy 1→2 建议保证);
  block: profile 行 > 0 透传, 否则 hw L2 → bench_harness::block_candidates 中位
  (复用 CPU-006 几何序列, 不复制), 再否则 deferred。
- build_resource_plan_v1: 选择写入 plan(schema astrocs.resource-plan/v1)。
- worker_grant_trace_v1: 实际 granted 写 trace(schema astrocs.worker-grant/v1,
  含 build_id/kernel_id/provider/isa/planned/granted/worker budget 对照);
  granted 由执行层观测传入, 本层只记录不伪造(MON-001/V8-MON-001 域)。

## 验收映射
| 验收 | 落点 | 单测 |
|---|---|---|
| no fixed cores | 固定核数声明拒用+全派生无固定分支 | 单测 §5/§6(1..48 扫描, fixed=32 声明不影响选择) |
| cgroup/Windows job 限制模拟 | hw cgroup_cpu_limit 注入 → hard_cap | 单测 §2(8 核机器 cgroup=2 → 2) |
| 低资源不超配 | cgroup 钳制 + 内存预算钳制 | 单测 §10(1GiB/512MiB → 2) |
| 2 核 heavy 1→2 有可测扩展 | single_thread_rejected_floor2 结构保证(扩展实测属 CPU-006 bench 域) | 单测 §8 |
| worker=1 仅 tiny/I/O | tiny/io 允许 1; heavy 仅系统性约束(cap=1)下允许 | 单测 §8/§9 |
| 不能无视用户上限 | user_cap 参与 min 硬上限 | 单测 §3(8 核 profile=8 user=3 → 3) |
| 选择写入 plan | astrocs.resource-plan/v1 | 单测 §13 |
| granted 写 trace | astrocs.worker-grant/v1(V8-CPU-003 字段) | 单测 §14 |
| profile 修改改变实际选择 | profile 行透传(§7: 3→3, 16→16) | 单测 §7 |

## 边界与禁改确认
仅新增: lib/backend_host/worker_advisor.{h,cpp}; 修改: astrocs_cpu 源列表、
tests/unit 注册、本证据目录; 未触碰 cli/、lib/phase2/、lib/phase3_*、lib/calibration/、
lib/gaia_xpsd_client/、docs/、tools/、ci/、.github/、tests/backend/、tests/cli/;
未执行任何 git commit/push; 全部外部命令带 timeout 并记录 load 上下文。
