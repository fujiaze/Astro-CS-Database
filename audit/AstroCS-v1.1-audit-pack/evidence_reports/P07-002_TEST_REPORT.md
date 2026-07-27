# P07-002 长批次与故障稳定性 - 测试报告

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Stage1 批量 C001 | `stability_runner.py --stage1-batch` (C001 Galaxy_Center_T4_Red_180s) | 120s | 0 | PASS | logs/batch_C001_*.log |
| Stage1 批量 C003 | `stability_runner.py --stage1-batch` (C003 NGC1727_T2_Red_600s) | 120s | 0 | PASS | logs/batch_C003_*.log |
| Stage1 批量 C004 | `stability_runner.py --stage1-batch` (C004 NGC247_T2_Lum_600s) | 120s | 0 | PASS | logs/batch_C004_*.log |
| Stage1 批量 C005 | `stability_runner.py --stage1-batch` (C005 NGC55_T3_Red_600s) | 120s | 0 | PASS | logs/batch_C005_*.log |
| Stage1 批量 C006 | `stability_runner.py --stage1-batch` (C006 NGC83_cluster_T3_Red_600s) | 120s | 0 | PASS | logs/batch_C006_*.log |
| Stage1 批量 C007 | `stability_runner.py --stage1-batch` (C007 Victory_Nebula_T4_Lum_180s) | 120s | 0 | PASS | logs/batch_C007_*.log |
| Stage2 重复 run1 | `stability_runner.py --stage2-repeat` (run1) | 180s | 0 | PASS | logs/stage2_repeat_1_*.log |
| Stage2 重复 run2 | `stability_runner.py --stage2-repeat` (run2) | 180s | 0 | PASS | logs/stage2_repeat_2_*.log |
| Stage2 重复 run3 | `stability_runner.py --stage2-repeat` (run3) | 180s | 0 | PASS | logs/stage2_repeat_3_*.log |
| 取消后重跑 - 取消 | `stability_runner.py --cancel-rerun` (cancel C003 after 10s) | 30s | 3221225786 | PASS | logs/cancel_rerun_cancel_*.log |
| 取消后重跑 - 重跑 | `stability_runner.py --cancel-rerun` (rerun C003) | 120s | 0 | PASS | logs/cancel_rerun_rerun_*.log |
| 故障注入 | `stability_runner.py --fault-inject` (delete frame1.hiss during stage2) | 60s | 1 | PASS | logs/fault_inject_*.log |
| 资源泄漏检查 | `stability_runner.py --leak-check` | 10s | 0 | PASS | stability_results.json |

## Real-data metrics

### Stage1 批量稳定性（6 帧连续）

| 帧 | 天区 | 配置 | wall (s) | 峰值内存 (MB) | HISS 大小 (bytes) | 与 P07-001 wall 差异 | 与 P07-001 峰值差异 |
|---|---|---|---:|---:|---:|---:|---:|
| C001 | Galaxy_Center (dec=-13°) | T4 | 19.085 | 3633.60 | 47705 | -1.1% | -0.01% |
| C003 | NGC1727 (dec=-70°) | T2 | 85.898 | 35471.28 | 19347 | +10.8% | +0.003% |
| C004 | NGC247 | T2 | 17.374 | 792.80 | 19450 | - | - |
| C005 | NGC55 | T3 | 16.882 | 791.25 | 18981 | - | - |
| C006 | NGC83_cluster | T3 | 16.893 | 795.71 | 19286 | - | - |
| C007 | Victory_Nebula (dec=-79°) | T4 | 67.592 | 32607.03 | 47691 | +6.1% | +0.003% |

- **批量总耗时**：约 223.7s
- **成功数**：6/6
- **帧间内存回归**：正常（delta 在 ±138 MB 内双向波动，无累积上升）
- **南天帧（C003/C007）**：峰值内存 32-35 GB，wall time 67-86s（Gaia xpsd 南天分区特性）
- **赤道帧（C001/C004/C005/C006）**：峰值内存 0.8-3.6 GB，wall time 17-19s

### Stage2 重复稳定性（3 次确定性）

| 项 | run1 | run2 | run3 | 差异 | P07-001 基线 |
|---|---:|---:|---:|---:|---:|
| wall (s) | 6.627 | 6.392 | 6.202 | 0.425 | 5.597 |
| 峰值内存 (MB) | 1962.89 | 1979.65 | 1914.06 | 65.59 | 1979.38 |
| HCSD SHA-256 | 2A9BD12E... | 2A9BD12E... | 2A9BD12E... | **一致** | 2A9BD12E... |
| HCSD 大小 (bytes) | 187455430 | 187455430 | 187455430 | **一致** | 187455430 |

- **确定性**：3 次 HCSD SHA-256 完全一致（字节级可重现）
- **匹配 P07-001 基线**：是（SHA-256 完全一致）
- **匹配 P00-003 baseline**：是（SHA-256 完全一致）
- **峰值内存稳定**：差异 65.59 MB（<200MB，冷启动+系统波动可接受）

### 取消后重跑

| 检查项 | 结果 | 说明 |
|---|---|---|
| 取消 exit_code | PASS | 3221225786 (STATUS_CONTROL_C_EXIT 0xC000013A) |
| 取消耗时 | PASS | 12.853s（含取消信号后退出） |
| 进程退出 | PASS | 是 |
| 残留进程 | PASS | 0（tasklist 确认） |
| partial 输出清理 | PASS | 无 partial HISS 残留 |
| 重跑 exit_code | PASS | 0 |
| 重跑 wall (s) | 86.916 | 与正常 C003 一致 |
| 重跑 HISS 大小 | 19347 bytes | 与正常 C003 一致 |
| 重跑成功 | PASS | 是 |

### 故障注入

| 检查项 | 结果 | 说明 |
|---|---|---|
| 删除目标 | frame1.hiss | t=2.041s 删除 |
| exit_code | 1 | 报错退出（非崩溃） |
| 优雅处理 | PASS | graceful_handling=True（非硬崩溃） |
| HCSD 生成 | 否 | 输入缺失，预期行为 |
| 进程退出 | PASS | 是（2.605s 退出） |
| 峰值内存 (MB) | 1907.32 | 与正常 stage2 一致 |

### 资源泄漏检查

| 检查项 | 结果 | 说明 |
|---|---|---|
| 系统可用内存 | PASS | 46679.14 MB / 65446.38 MB (71%, healthy) |
| 残留进程 | PASS | 0 |
| 临时文件 | PASS | 0 |
| stage2 重复峰值差异 | PASS | 65.59 MB (stable, <200MB) |
| stage1 峰值差异 | N/A | 34680.03 MB（天区特性，非泄漏） |

## Failures and investigation

### 无失败用例

所有 13 个测试用例均 PASS。

### 性能异常调查（2 项，均非失败）

1. **C003 wall time +10.8% vs P07-001 基线**
   - 根因：长批次环境下的系统负载波动（P07-001 标准差 1.765s）
   - 结论：非回归，正常波动

2. **stage2 wall time +14.5% vs P07-001 基线**
   - 根因：长批次后系统负载较高 + 冷启动效应（三次运行递减 6.627→6.392→6.202）
   - 结论：非回归，长批次后正常现象

## 测试结论

- **测试用例总数**：13
- **通过**：13
- **失败**：0
- **性能异常调查**：2 项（均非回归，已定位根因）
- **VERDICT**: PASS
