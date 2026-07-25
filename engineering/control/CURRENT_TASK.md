# 当前任务：P03-002 配置参数端到端追踪

读取 `tasks/P03-002.md` 并执行。追踪所有配置参数从 CLI/config 到各阶段 DLL 入口的端到端传递路径，确保参数无丢失、无静默覆盖。

## 上一任务完成情况

- P03-001 真实校准输入接线: DONE (VERDICT: PASS)
  - 证据: evidence/P03-001/
  - 关键变更: orchestrator.cpp 重写 run_stage_calibrate, stage1_config.json 新增 calibration 段
  - 6/6 测试通过 (3 正面 + 3 负面), cal_stats KV 块 (22 字段) 正确输出
  - 残留建议已转移至 P03-003 (严格失败与禁止静默跳过)

## P03-002 依赖

- P03-001 (DONE)
- P02-007 (DONE)

## 执行步骤

1. 列出所有 stage1/stage2 配置参数 (stage1_config.json 全字段)
2. 追踪每个参数从 CLI -> Orchestrator::load_config -> 各 stage handler -> DLL 入口的传递路径
3. 标识未消费者 (配置存在但未传入 DLL) 和静默覆盖 (代码硬编码覆盖配置值)
4. 输出参数追踪矩阵和缺陷清单

完成独立复核后, 更新状态并进入依赖满足的下一任务。
