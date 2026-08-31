# P1-004: Phase1 数值与资源联合门

任务 ID: P1-004
Gate: G4
依赖: P1-002; P1-003; MON-003
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P1-004：

> 同一次 current commit 运行所有 P1 Oracle 和 ≥10s Drizzle/calibration 资源 workload；
> science summary 和 resource summary 互相引用 run ID。
> 数值对但资源失败仍 FAIL，资源好但数值错也 FAIL。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 同 commit 运行所有 P1 Oracle | test_01 跑 6 个 Oracle 测试(calibration/drizzle/noise/wcs_psf/reproject/gaps)全绿 | c03 |
| 资源 workload 执行 | test_02 CLI run phase3 成功产 resource_summary.json | c03 |
| science/resource 互相引用 run ID | test_03 manifest.run_id == resource_summary.run_id; resource_summary.json 新增 run_id 字段 | c02/c03 |
| 同 commit 门 | test_04 CLI --version commit == git HEAD(同批) | c03 |
| 联合语义 | 数值对但资源失败 FAIL、资源好但数值错 FAIL(两门同测) | c03 设计 |

## 实现文件

- `cli/resource_recorder.h`：`write_all` 增加 run_id 参数, resource_summary.json 输出 run_id
- `cli/commands.cpp`：write_all 传 `ev.run_id()`(science manifest 同 run_id)
- `tests/backend/test_p1004_joint_gate.py`（新）：4 组联合门断言

## 测试结果

- `test_p1004_joint_gate.py`: 4/4 PASS(6 Oracle 全绿 + 资源 workload + run_id 关联 + 同 commit)
- `ctest`: 56/56 PASS
- 实跑: resource_summary.json run_id == astrocs_run_*.json run_id(同一 run)

## 说明

- run_id 由 JsonlEmitter(make_run_id) 生成; science manifest 与 resource summary 共用该 ID。
- 同 commit 门用 CLI --version 的 g<sha12> 与 git HEAD 比对(version_generated.h 由 CMake 配置时生成)。
