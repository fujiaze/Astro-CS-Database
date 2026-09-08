# CPU-006: 提供可审计硬件基准（benchmark report）

任务 ID: CPU-006
Gate: G3
依赖: CPU-005（已闭环）
平台: Linux+Windows
变更类别: performance
权威规格: `工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3/tasks/04_CPU_RESOURCE_TASKS.md` CPU-006 小节 + `工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/tasks/03_P0_REMEDIATION_TASKS.md` §V8-CPU-001

## 目标（V7.1 原文要点）

实现 `astrocs benchmark cpu --suite quick|release --output profile.json`：记录 CPU 指纹、
逻辑/物理核、cache/NUMA/affinity、内存带宽、kernel sizes、warmup、重复轮次、median/MAD、
workers、provider、输入 hash、编译器。验收约束：

- 不会改变生产 profile，直到全部 self_test + 阈值通过；
- 没有 benchmark 时 baseline；
- 异常值剔除规则预先固定；
- 命令有 timeout。

## 设计（层次合同：不复制测量引擎）

新增 **聚合审计层** `lib/backend_host/bench_report.{h,cpp}`，测量链唯一实现复用 CPU-003
`generate_profile_v2()`（Oracle 门 kOracleRelTol=2e-4 冻结 → self_test → memory 带宽 →
逐 kernel × 规模 × provider × workers × block → 3 warmup + 7 measure → median/MAD →
winner），零复制漂移。suite→mode 映射：`quick→quick`（代表 kernel × medium）、
`release→full`（12 kernel × {small,medium,large}）。

- schema **`astrocs.benchmark-report/v1`**，与生产 profile（`astrocs.cpu-profile/v2`）
  结构性隔离：`verify_profile_v2` 拒绝 report 文本，report 面任何实现都不写生产 profile。
- **预冻结异常值剔除规则**（单一出处 `benchmark_outlier_policy()`）：不逐点剔除，
  median/MAD 天然稳健（7 samples），p05/p95 记录供审计；胜出候选 MAD/median > 0.35
  判 dispersion、不得通过阈值门。report 与 verify 共用同一规则文本。
- **阈值门 `benchmark_report_verdict`**（纯函数，预冻结）：
  `eligible_for_profile = all_oracle_passed && thresholds_passed && !timeout_reached`；
  生产 profile 仅在 eligible 时才可由 CLI 写入（本层结构性不落盘）。
- **heavy 禁止单线程**（V8-CPU-001 同语义）：聚合选择对 compute/memory kernel 强制
  workers>=2，当存在 >=2 worker 的 oracle-pass 候选；单核机（无多 worker 候选）允许 1。
- **无 benchmark/自检失败回落 baseline**：无任何 oracle-pass 候选 → provider=baseline、
  workers=0、median=0、fallback_reason 记因（verify 强制回落项不得伪造测量值）。
- **timeout**：`BenchReportOptions::deadline_ns` lib 侧预算标注（超时 → eligible=false）；
  命令层由 shell `timeout` 承担（证据 c02/c03）。
- **workers/budget 无硬编码**：引擎 budget 来自 hardware_inspect 的
  `available_logical_cpus`（affinity∩cgroup）；report 记录 runtime load 上下文与 quota。

CLI 接线（`benchmark cpu` 子命令）属 CLI-004 并行域：`cli/` 本任务禁改，lib API 已就绪。

## 规格字段映射（验收关键词 benchmark report）

| 规格要求字段 | report 路径 | 来源（唯一实现） |
|---|---|---|
| CPU 指纹 | hardware.vendor/brand/family/model/stepping/microcode/feature_names/xcr0 | hardware_inspect_json_v1 |
| 逻辑/物理核 | hardware.logical_cpus_configured / logical_available / physical_cores_estimate(派生注明) | 同上 |
| cache/NUMA/affinity | hardware.cache / numa_nodes / affinity(+count) / cgroup_cpu_limit / quota_signature | 同上 |
| 内存带宽 | memory_bandwidth.{copy,read,write,triad} | 引擎 bench_memory |
| kernel sizes | kernels.*.sizes.{small,medium,large} | 引擎 size_mult |
| warmup/重复轮次 | measurement.warmup=3 / samples=7 / clock | 冻结常量 |
| median/MAD | kernels.*.sizes.*.{median_ns,mad_ns,p05_ns,p95_ns} | 引擎统计 |
| workers | kernels.*.sizes.*.workers（heavy≥2 门禁） | 聚合选择 |
| provider | kernels.*.sizes.*.provider（仅 oracle-pass 可胜出） | 聚合选择 |
| 输入 hash | input_samples_sha256（确定性 LCG 输入的候选序列化 hash） | 引擎 raw_samples_sha256 |
| 编译器 | hardware.compiler | hardware_inspect |
| build 绑定 | build.{astrocs_build, source_commit, benchmark_binary_sha256} | 调用方注入 |

## 实现文件

- `lib/backend_host/bench_report.h/cpp`（新）：suite 映射 / aggregate_benchmark_kernels /
  benchmark_report_verdict / generate_benchmark_report / verify_benchmark_report /
  benchmark_outlier_policy
- `CMakeLists.txt`：astrocs_cpu 源列表加入 bench_report.cpp
- `tests/unit/CMakeLists.txt`：注册 cpu006_bench_report 测试
- `tests/unit/cpu006_bench_report_test.cpp`（新）：8 组断言（Oracle 门回落 / heavy≥2 /
  平手保守 / 阈值门与 timeout / 非法 suite / 字段完整+verify 篡改负例×5 / release
  12×3 全量(环境变量门控) / quick 覆盖）

## 测试与实跑结果

- 单测快速面 `ctest -R cpu006_bench_report`：PASS（其余 cpu_bench/cpu003_profile_v2 回归同批 PASS）。
- benchmark 实跑（`ASTROCS_CPU006_ENABLE_RELEASE=1`，timeout 1500s，load<2 前置检查）：
  - quick（1 kernel × medium）与 release（12 kernel × 3 规模 = 36 项 × 12 候选 = 432 raw）
    全量生成并落盘 `bench_report_quick.json` / `bench_report_release.json`；
  - 本机 16 logical（affinity∩cgroup 实测，无 cgroup 限制），worker 候选实测最高 16；
  - release verdict：`all_oracle_passed=false`（hips-bulk-transform / wcs-psf-batch 两
    kernel oracle 全 FAIL）→ `eligible_for_profile=false`。该两 kernel FAIL 为 **V8.1
    在案已知项**（容差 2e-4 收紧 + verdict 修复后 f32/f64 结构性超差，owner 三选项裁决
    票在案，见 `evidence/v8_1_ci_control/STATE_RECONCILIATION.csv` CPU-006 行）；本任务
    不擅自放宽容差（kOracleRelTol=2e-4 冻结，docs/architecture/cpu/CPU_003_AVX2_PROVIDER.md §7），
    如实显形于 report——这正是"不会改变生产 profile 直到全部 self_test+阈值通过"的验收
    语义在真实硬件上的可执行证明。
  - report 本身通过 `verify_benchmark_report` 复读（含回落项 workers=0/median=0/记因约束）。

## 边界与并发纪律

- 未改：`docs/`、`tools/`、`ci/`、`.github/`、`tests/backend|cli/`、`cli/`、`lib/gaia_xpsd_client/`、`lib/phase2|phase3*`。
- 所有外部命令带 timeout，日志在 `run/local/agent_cpu006/`；构建/benchmark 前做
  `uptime`（load<2）+ `pgrep -f "ninja|pytest|ctest"` 重负载检查。
- SubAgent 不执行 git add/commit/push：`result_commit`、COMMITS.csv/TASK_LEDGER.csv
  CPU-006 行由主 agent 提交后按链回填。
