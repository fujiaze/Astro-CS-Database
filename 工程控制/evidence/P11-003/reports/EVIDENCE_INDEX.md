# P11-003 证据清单

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-003 |
| 生成日期 | 2026-07-28 |
| 证据目录 | `engineering_v1.2/evidence/P11-003/` |

## 1. 证据文件清单

### 1.1 汇总文件

| # | 文件路径 | 用途 | 备注 |
|---|----------|------|------|
| 1 | `reports/p11_003_summary.json` | 16 帧汇总数据 + 聚合统计 + 跨帧模式 + 根因分析 | 本任务核心产出（3 帧重跑数据已回填） |
| 2 | `reports/group_a_summary.json` | group_a (4帧) 汇总 | T2_HA_LDN43, T2_OIII_NGC1727, T3_RED_NGC55, T3_GREEN_NGC55 |
| 3 | `reports/group_b_summary.json` | group_b (4帧) 汇总 | T3_BLUE/HA/OIII/LUM_NGC55 |
| 4 | `reports/rerun_corrupted_summary.json` | T2_RED/GREEN/BLUE_LDN43 三帧重跑汇总 | 损坏帧重跑恢复后的完整诊断数据 |
| 5 | `REPRESENTATIVE_FRAMES_ARCHIVE.json` | 16 帧设备档案 + 执行计划 + 状态 | 含设备/滤镜/目标元数据 |

### 1.2 单帧证据（每帧 4 个文件 × 16 帧 = 64 个文件）

每帧目录 `reports/<frame_id>/` 下包含：

| 文件 | 用途 | 格式 |
|------|------|------|
| `closure_report.json` | 闭环诊断报告（WCS + 残差统计 + gate 检查） | JSON |
| `matched_pairs.json` | 匹配星对明细（每对星的坐标 + 残差） | JSON |
| `residual_plot.png` | 残差分布图 | PNG |
| `quadrant_plot.png` | 象限分布图 | PNG |

16 帧清单：

| # | frame_id | closure_report | matched_pairs | residual_plot | quadrant_plot |
|---|----------|----------------|---------------|---------------|---------------|
| 1 | T4_RED_Galaxy_Center | ✅ 完整 | ✅ | ✅ | ✅ |
| 2 | T4_GREEN_Galaxy_Center | ✅ 完整 | ✅ | ✅ | ✅ |
| 3 | T4_BLUE_Galaxy_Center | ✅ 完整 | ✅ | ✅ | ✅ |
| 4 | T4_HA_Galaxy_Center | ✅ 完整 | ✅ | ✅ | ✅ |
| 5 | T4_OIII_Galaxy_Center | ✅ 完整 | ✅ | ✅ | ✅ |
| 6 | T2_RED_LDN43 | ✅ 已恢复 | ✅ 已恢复 | ✅ | ✅ |
| 7 | T2_GREEN_LDN43 | ✅ 已恢复 | ✅ 已恢复 | ✅ | ✅ |
| 8 | T2_BLUE_LDN43 | ✅ 已恢复 | ✅ 已恢复 | ✅ | ✅ |
| 9 | T2_HA_LDN43 | ✅ 完整 | ✅ | ✅ | ✅ |
| 10 | T2_OIII_NGC1727 | ✅ 完整 | ✅ | ✅ | ✅ |
| 11 | T3_RED_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |
| 12 | T3_GREEN_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |
| 13 | T3_BLUE_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |
| 14 | T3_HA_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |
| 15 | T3_OIII_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |
| 16 | T3_LUM_NGC55 | ✅ 完整 | ✅ | ✅ | ✅ |

> **重跑恢复历史**：T2_RED/GREEN/BLUE_LDN43 三帧的 closure_report.json 和 matched_pairs.json 此前因文件系统级损坏（全零字节）导致数据缺失，PNG 图表文件始终有效。该三帧已通过重跑恢复完整数据（closure_report.json + matched_pairs.json 均已恢复），汇总数据已回填至 p11_003_summary.json 及相关报告。重跑汇总见 `reports/rerun_corrupted_summary.json`。

### 1.3 报告文件

| # | 文件路径 | 用途 |
|---|----------|------|
| 1 | `reports/TASK_REPORT.md` | 任务目标、执行流程、16 帧结果表、耗时统计 |
| 2 | `reports/TEST_REPORT.md` | 测试方法、门限规则、通过率分析、失败帧分析 |
| 3 | `reports/EVIDENCE_INDEX.md` | 证据清单（本文件） |
| 4 | `reports/REVIEW_REPORT.md` | 复核结论、根因分析、残留风险、后续建议 |

## 2. 文件统计

| 类别 | 数量 | 备注 |
|------|------|------|
| 汇总 JSON | 5 | p11_003_summary + group_a + group_b + rerun_corrupted + archive |
| 单帧 closure_report.json | 16 | 全部有效（3 个曾损坏已重跑恢复） |
| 单帧 matched_pairs.json | 16 | 全部有效（3 个曾损坏已重跑恢复） |
| 单帧 residual_plot.png | 16 | 全部有效 |
| 单帧 quadrant_plot.png | 16 | 全部有效 |
| 报告 Markdown | 4 | TASK/TEST/EVIDENCE/REVIEW |
| **合计** | **73** | |

## 3. 数据完整性说明

| 帧范围 | 数据状态 | 数据来源 |
|--------|----------|----------|
| T4×5 (Galaxy_Center) | ✅ 完整 | closure_report.json (P11-002 v1.0 格式) |
| T2_HA_LDN43 + T2_OIII_NGC1727 | ✅ 完整 | group_a_summary.json (P11-003 v1.0 格式) |
| T3×6 (NGC55) | ✅ 完整 | group_a/b_summary.json (P11-003 v1.0 格式) |
| T2_RED/GREEN/BLUE_LDN43 | ✅ 完整（已恢复） | closure_report.json (重跑恢复) + rerun_corrupted_summary.json |

> **重跑恢复说明**：T2_RED/GREEN/BLUE_LDN43 三帧的 closure_report.json 此前因文件系统级损坏（全零字节）导致详细诊断字段缺失，已通过重跑恢复完整数据。恢复后所有 16 帧均具有完整字段（n_matched, dist_median/p90/p99, x/y 方向统计, quadrant_counts, closure_err, elapsed_sec 等），聚合统计和跨帧模式分析均基于完整 16 帧数据重新计算。
