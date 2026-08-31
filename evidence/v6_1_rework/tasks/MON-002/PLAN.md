# MON-002: 资源门禁接入生产控制流

任务 ID: MON-002
Gate: G3
依赖: MON-001
平台: Linux+Windows
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` MON-002 与 `08_CPU_RESOURCE_ACCEPTANCE.md` §8：

> 生产 Runtime 在 first 10 s 和结束时调用 gate；失败返回统一 RESOURCE exit code
> 并输出 diagnosis。CPU-heavy active window≥10s：worker p50≥2（available≥2）、
> CPU p50≥90%、mean≥85%。memory-bound 只有带宽≥80% 本机 benchmark 且已检查搬运/布局
> 才可解释；I/O 串行仅短于 5s 或总时长<5%。禁止仅 emit event 不改变退出状态。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 生产控制流调用 gate | run 收尾(manifest 前)调用 gate; 首 10s 快速失败检查 | commands.cpp |
| RESOURCE exit code + diagnosis | 失败 → `astrocs::RESOURCE`(10) + diag 写 manifest=incomplete + gate error 事件 + stderr | gate 分支 |
| first 10s 快速失败 | active 窗 ≥10s 且 CPU p50<20% → exit 10(协作取消) | fast-fail 分支 |
| CPU-heavy≥10s: worker p50≥2 | available≥2 且 active workers p50<2 → FAIL | evaluate_gate SingleThreaded |
| CPU p50≥90% / mean≥85% | active 窗统计 → gate 判定 | evaluate_gate |
| memory 带宽≥80% 才解释 | `required_memory_bandwidth_frac` pre-frozen; achieved<required → FAIL | MemoryBandwidthLow |
| I/O 串行短于 5s 或 <5% | `io_is_short_serial` → 豁免; 否则须 bytes/ops/await 证据 | IoMissingEvidence |
| 禁止仅 emit event | gate 失败改变退出状态(exit 10)并 manifest=incomplete | CLI 分支 |

## 实现文件

- `cli/resource_gate.h`（V6 遗留已含 GateConfig/evaluate_gate/fast_fail_first10s/diag_message；本轮复用）
- `cli/commands.cpp`：run 收尾接入 gate（first-10s 快速失败 + 结束时 evaluate → exit 10 + diagnosis）
- `tests/unit/mon002_gate_test.cpp`（新）：11 组断言（compute 门禁/单线程/低核/短任务豁免/无标注/io 证据/memory/mixed/first-10s/锁退化）

## 测试结果

- `ctest`: 55/55 PASS（含新增 mon002_gate）
- `mon002_gate_test`: MON-002 TESTS PASS（11 组断言全过）
- `tests/cli/test_cli_protocol.py`: 10/10 PASS
- 实跑: 正常 run → gate ok 事件（active_wall/workers_p50/cpu_p50）+ exit 0; 短任务豁免

## 遗留说明

- gate 失败路径的 CLI 级端到端验证（真实 ≥10s 低 CPU workload → exit 10）属 MON-003
  （2 核合成 workload 验证门禁）范畴，MON-003 时构造合成 workload 完成。
