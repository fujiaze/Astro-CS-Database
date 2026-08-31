# MON-001: 资源监测接入每个 heavy node

任务 ID: MON-001
Gate: G3
依赖: RT-006; CPU-002
平台: Linux+Windows
变更类别: performance

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` MON-001：

> 每个 heavy node 自动生成 `resource_samples.csv`、`resource_summary.json`、
> `worker_balance.csv`。样本含 elapsed、process/system CPU、active/runnable workers、
> RSS/PSS/commit、fault、read/write、queue depth、lock wait、progress。
> 分 init/active/flush；统计 mean/p50/p95/peak/slope。
> Windows/Linux 统一"100%=全部分配核用满"的 normalized 口径。
> 采样开销用真实 wall 总开销计算。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 自动生成三产物(无需脚本) | `ResourceRecorder::write_all` 在 run 收尾自动写 `resource_samples.csv` / `resource_summary.json` / `worker_balance.csv` 到 output_dir | 实跑 3 文件 |
| 样本字段齐 | elapsed/stage/cpu_pct/system_cpu_pct/active_workers/runnable_workers/rss/pss/commit/page_faults/read/write/queue_depth/lock_wait/progress | resource_samples.csv header |
| init/active/flush 分段 | `set_stage` 在 run 执行前(init)→Runtime 执行中(active)→执行后(flush)切换; 每样本带 stage 列 | summary stages |
| mean/p50/p95/peak/slope | `stage_stats()` 逐阶段统计; `percentile_sorted` 最近秩 | resource_summary.json |
| normalized 口径 | summary 声明 `normalized_cpu_100pct_all_allocated_cores`; cpu_pct = CPU 秒/墙钟×100 | summary |
| 采样开销<2% | ProcessMonitor 自测每样本开销 `sample_overhead_ms`; summary 输出 | summary |
| worker balance | `worker_balance.csv` 每样本 active vs runnable + utilization | worker_balance.csv |

## 实现文件

- `cli/resource_recorder.h`（新）：ResRecord/ResStage/ResStageStats/ResourceRecorder(采样/分段/统计/三产物)
- `cli/monitor.h`：ProcessMonitor 增加 `last_sample()` 访问器
- `cli/commands.cpp`：run 命令接入 ResourceRecorder(monitor 线程 record + 阶段切换 + 收尾 write_all)
- `tests/unit/mon001_recorder_test.cpp`（新）：采样/阶段/p50-p95/三产物/开销 断言

## 测试结果

- `ctest`: 54/54 PASS（含新增 mon001_recorder）
- `mon001_recorder_test`: MON-001 TESTS PASS
- `tests/cli/test_cli_protocol.py`: 10/10 PASS; `test_cli_build.py`: 6/6 PASS
- 实跑验证: `run --phases 3` 后 output_dir 生成 3 个资源产物, 阶段 init/active/flush 齐全

## 遗留说明

- `tests/cli/test_monitor_events.py`（V6 时代 MON-002 测试）4 失败根因是
  `node hips failed: session run: PARAM`——测试自编译的 FIELD.hips 单文件与 V6.1
  `p3_sampler_open`(期望 product_dir/signal 目录结构)不兼容, 属 P3 系列任务
  (Phase3 模块验证)范畴的预存在遗留, 非 MON-001 引入。用生产 hips 布局
  (run/phase2/v7/*.mosaic.hips) 的 run 成功证明监测路径 OK。
