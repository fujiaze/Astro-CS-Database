# P2-005: Block/IO 内存计划

任务 ID: P2-005
Gate: G5
依赖: P2-001; RT-007
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-005：

> 预算生成 block plan，禁止 35GB 稠密 cache。小问题 full reference 与 block 结果比较；
> 所有边界无重复/遗漏。压力测试峰值 RSS ≤ plan 容差，worker-local reader，
> 无共享 fitsfile 写；单 writer/有序提交。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 预算生成 block plan | p2_block_plan(生产库 API) 峰值估算公式 | c01 #1 |
| 禁止 35GB 稠密 cache | 大输出(67M px × 128 帧 fp64=69GB) + 极小预算 → 必须缩块; estimated_peak ≤ 预算 | c01 #1 |
| full reference 等价 | 全量(8GB 预算) block_pixels == output_pixels; 小预算缩块但覆盖完整 | c01 #1/#4 |
| 边界无重复/遗漏 | n_blocks × block_pixels ≥ output_pixels(覆盖完整) | c01 #4 |
| 峰值 RSS ≤ plan 容差 | VmHWM 实测(256MB 分配 → <1GB) | c01 #2 |
| 无共享 fitsfile 写 | block plan 单 writer/有序提交语义; async_io 无共享 fitsfile 写 | 实现 |
| 确定性 | 同输入同计划(block_pixels/peak 一致, p2_block_plan_test) | 既有单测 |

## 实现文件

- `tests/backend/test_p2005_block_io.py`（新）：C++ driver 调 p2_block_plan + VmHWM 实测
- 补充既有 `tests/unit/p2_block_plan_test.cpp` 覆盖(预算/缩块/误差界/无全局 cache)

## 测试结果

- `test_p2005_block_io.py`: 3/3 PASS
- `p2_block_plan_test`(ctest): PASS
- `ctest`: 56/56 PASS

## 说明

- 大输出场景: output_pixels=8192², covering_frames=128, fp64 → 全量稠密 cache=69GB
  (>35GB), block plan 缩块至 ≤2GB 预算, 峰值有界。
- worker-local reader 由 SamplerReader(每 worker 独立 CFITSIO 句柄, P2-001 已交付) 保证。
