# P1-003: 移除 CLI 直连 Drizzle 与重复 FITS 解析

任务 ID: P1-003
Gate: G4
依赖: P1-001
平台: Linux
变更类别: architecture

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P1-003：

> 删除 CLI 的 `spawn_frame_from_fits`、直接 CFITSIO header 解析和 `hp_drizzle_run_hips`
> production call。必要的 FITS→Artifact 由 I/O module 完成，Drizzle 只消费类型化 frame。
> test wrapper 选择 IR preset。调用图和 negative fixture 证明无法绕过。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| CLI 无 hp_drizzle_run_hips 直连 | CLI 二进制 nm 检查 0 符号; commands.cpp 无该符号 | c01 #1/#2 |
| CLI 无 spawn_frame_from_fits | 同上(nm + 源码 grep) | c01 #1/#2 |
| CLI 无直接 CFITSIO header 解析 | commands.cpp 无 fits_open/fits_read/cfitsio 直连调用 | 源码检查 |
| cmd_drizzle 仅测试 preset | 生产调用 → ARGS(2) + 提示用 test synthetic / phase2 run | c01 #3 |
| 生产 callgraph 无 drizzle 直连 | check_prod_reachability REACH_PASS acr=0(仅 Runtime 拥有者) | c01 #4 |
| test wrapper 经 preset/Runtime | drizzle 命令引导到 `test synthetic --group drizzle`; 生产走 phase2 run | c01 #3 |

## 实现文件

- `tests/cli/test_p1003_drizzle_path.py`（新）：4 组负例(nm 符号/源码 grep/命令拒绝/callgraph)

## 测试结果

- `test_p1003_drizzle_path.py`: 4/4 PASS
- `check_prod_reachability`: REACH_PASS binary=astrocs acr=0
- 前期 CHK-001 已建 PROD_REACHABILITY.json/dot(T-NEGATIVE-DRIZZLE 负例)

## 说明

- CLI 生产路径仅 Runtime 拥有者(PipelineIR/ModuleRegistry/Scheduler/RunContext/ThreadBudget);
  Drizzle 只消费类型化 frame(模块端口 DATA-P1-CAL → DATA-P1-STACK), FITS 读写由 I/O 模块承担。
