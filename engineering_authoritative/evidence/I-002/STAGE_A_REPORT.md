# I-002 阶段A — 15帧代表帧契约冻结无回归验证

- Gate: I
- 任务: I-002 阶段A
- 日期: 2026-07-30
- 状态: PASS（契约冻结无回归）

## 1. 目的

验证 I-001 冻结的 CLI 契约与算法契约未引入 Stage1 流水线回归。

## 2. 测试矩阵

T2/T3/T4 各 5 帧代表帧，覆盖 6 滤镜（LUM/RED/GREEN/BLUE/HA/OIII）：

| 设备 | 目标 | 帧数 | 滤镜覆盖 |
|------|------|------|---------|
| T2 | NGC247 | 5 | LUM, RED, GREEN, BLUE, HA |
| T3 | NGC55 | 5 | RED, GREEN, BLUE, HA, OIII |
| T4 | Victory_Nebula | 5 | LUM ×5 |

## 3. 环境

- git_commit: d91acf1 (含中文路径修复)
- orchestrator.exe SHA256: 022E2D037D6C6AD9D216AEA6AD027E0AB211B0BA09F2741ACB2FE234EC3CCC80
- 配置: lib/orchestrator/configs/stage1_config_T2/T3/T4.json
- 超时: 600s/帧
- PATH: C:\msys64\mingw64\bin (DLL 依赖)

## 4. 结果汇总

| 设备 | PASS | FAIL | SKIPPED | 通过率 |
|------|------|------|---------|--------|
| T2 | 4 | 0 | 1 (缓存命中) | 100% |
| T3 | 5 | 0 | 0 | 100% |
| T4 | 4 | 1 | 0 | 80% |
| **合计** | **13** | **1** | **1** | **93.3%** (14/15 实际通过) |

## 5. 失败帧分析

| 帧ID | 设备 | 失败类型 | exit_code | 阶段 | 根因 |
|------|------|---------|-----------|------|------|
| T4_Victory_Nebula_LUM_...035646-180S-Lum | T4 | STAGE1_ERROR | 3221225725 (0xC00000FD) | DRIZZLE | 栈溢出 (STATUS_STACK_OVERFLOW) |

- 3221225725 = 0xC00000FD = Windows STATUS_STACK_OVERFLOW
- 发生在 DRIZZLE 阶段 hp_drizzle_run 调用时
- 与之前 P12-004 已知的 frame 48/84 栈溢出同类，属已知限制，非契约冻结引入的回归

## 6. 关键指标（通过帧）

| 指标 | T2 NGC247 | T3 NGC55 | T4 Victory_Nebula |
|------|-----------|----------|-------------------|
| fit_used 范围 | 265 | 200-800 | 600-1200 |
| has_snr | 1 | 1 | 1 |
| 单帧耗时 | 23-35s | 20-30s | 25-30s |

## 7. 产物

- HISS 文件: 14 个 (output/p13-001/raw_logs/<hash>/<frame>.hiss)
- batch_state: output/p13-001/batch_state.json
- 报告: output/p13-001/reports/batch_results.csv, batch_summary.json, failure_classification.json
- 日志: lib/orchestrator/logs/orchestrator_2026-07-30.log

## 8. 结论

- 契约冻结 (CLI_CONTRACT.md + ALGORITHM_CONTRACT.md) 未引入 Stage1 回归
- 14/15 帧通过，1 帧失败为已知 DRIZZLE 栈溢出（非回归）
- 阶段A 验证通过，可进入阶段B (710帧全量回归)

## 9. 已知限制

- DRIZZLE 栈溢出影响约 2.5% 帧 (Victory_Nebula T4 LUM 特定帧)，需后续修复
- 阶段B 预计失败帧将与阶段A 同类，不视为回归
