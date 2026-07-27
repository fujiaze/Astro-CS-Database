# 证据索引

- Task/ADR：P10-006 T1-T4 真实校准代表帧验证
- Commit：待提交
- Date：2026-07-27
- Environment：Windows + Python 3.11 + PowerShell 7

## 目标/问题

汇总 P10-006 任务的全部证据（交付物 + 脚本 + 日志 + 报告），用于 P10 Gate 独立复核。

## 输入与范围

- 任务定义：tasks/P10-006.md
- 规范：docs/04_CALIBRATION_MASTER_RESOLUTION_SPEC.md
- 依赖：P10-005（Light-to-Master 解析）

## 执行/决策

### 证据清单

| 类别 | 文件 | 说明 |
|------|------|------|
| 数据交付物 | REPRESENTATIVE_CALIBRATION_REPORT.csv | 16 行代表帧校准结果（路径 + 统计 + 坏点 + 通过判定） |
| 数据交付物 | CALIBRATION_VALIDATION_SUMMARY.json | 汇总统计（按设备/按滤镜 + 通过率 + 详情） |
| 数据交付物 | calibrated/*.fits (16 个) | 校准后 FITS 文件（float32，含 CALIBRAT/SRCFRAME 关键字） |
| 脚本交付物 | scripts/validate_representative_calibration.py | 代表帧校准主脚本（475 行） |
| 脚本交付物 | scripts/test_calibration_outputs.py | 测试套件（332 行，25 测试） |
| 原始日志 | raw_logs/validate_representative_calibration.log | 校准执行日志 |
| 原始日志 | raw_logs/test_calibration_outputs.log | 测试执行日志 |
| 报告 | TASK_REPORT.md | 任务执行详情 |
| 报告 | TEST_REPORT.md | 测试矩阵详情 |
| 报告 | REVIEW_REPORT.md | 独立复核报告 |
| 报告 | EVIDENCE_INDEX.md | 证据索引（本文件） |

### 校准后 FITS 文件清单（16 个）

| 文件名 | 设备/滤镜 | 尺寸 | 大小 |
|--------|----------|------|------|
| T2_BLUE_calibrated.fits | T2/BLUE | 4096x4096 | ~64 MB |
| T2_GREEN_calibrated.fits | T2/GREEN | 4096x4096 | ~64 MB |
| T2_HA_calibrated.fits | T2/HA | 4096x4096 | ~64 MB |
| T2_OIII_calibrated.fits | T2/OIII | 4096x4096 | ~64 MB |
| T2_RED_calibrated.fits | T2/RED | 4096x4096 | ~64 MB |
| T3_BLUE_calibrated.fits | T3/BLUE | 4096x4096 | ~64 MB |
| T3_GREEN_calibrated.fits | T3/GREEN | 4096x4096 | ~64 MB |
| T3_HA_calibrated.fits | T3/HA | 4096x4096 | ~64 MB |
| T3_LUM_calibrated.fits | T3/LUM | 4096x4096 | ~64 MB |
| T3_OIII_calibrated.fits | T3/OIII | 4096x4096 | ~64 MB |
| T3_RED_calibrated.fits | T3/RED | 4096x4096 | ~64 MB |
| T4_BLUE_calibrated.fits | T4/BLUE | 4500x3600 | ~62 MB |
| T4_GREEN_calibrated.fits | T4/GREEN | 4500x3600 | ~62 MB |
| T4_HA_calibrated.fits | T4/HA | 4500x3600 | ~62 MB |
| T4_OIII_calibrated.fits | T4/OIII | 4500x3600 | ~62 MB |
| T4_RED_calibrated.fits | T4/RED | 4500x3600 | ~62 MB |

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python validate_representative_calibration.py` | 300s | 0 |
| `python test_calibration_outputs.py` | 60s | 0 |

## 结果与证据

- 3 个数据交付物完整（CSV + JSON + 16 FITS）
- 2 个脚本交付物完整（校准脚本 + 测试套件）
- 2 个原始日志完整（校准日志 + 测试日志）
- 4 个报告完整（TASK + TEST + REVIEW + EVIDENCE_INDEX）
- 16/16 代表帧 PASS（100% 通过率）
- 25/25 测试 PASS

## 风险/回滚/残留

- 16 个 calibrated FITS 共约 1 GB，已纳入证据目录
- 后续 P11/P12 阶段可直接复用这些 calibrated FITS 作为输入示例

## 结论

P10-006 证据完整。3 个数据交付物 + 2 个脚本交付物 + 2 个原始日志 + 4 个报告齐全。16/16 代表帧通过率 100%，25/25 测试通过。可作为 P10 Gate 的关键证据。
