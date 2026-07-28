# P12-001 证据索引 — 增加Photometric分阶段诊断

## 任务信息
- **任务ID**: P12-001
- **阶段**: P12 (Photometric 分阶段诊断)
- **Gate**: G12
- **执行日期**: 2026-07-28
- **子任务C范围**: 同步 Python ctypes 封装 + 运行测试 + 生成证据文件

## 证据清单

### 报告文件
| 文件 | 说明 |
|------|------|
| `TASK_REPORT.md` | 任务报告: 执行摘要 + 修改文件 + 通过条件 |
| `TEST_REPORT.md` | 测试报告: 单元测试 + 契约测试 + CLI 验证 |
| `EVIDENCE_INDEX.md` | 本文件: 证据索引 |

### 规格文件
| 文件 | 说明 |
|------|------|
| `spec.md` | P12-001 规格: 目标 + 范围 + PhotometricDiag 字段 + Gate + 不做项 |

### 脚本
| 文件 | 说明 |
|------|------|
| `scripts/test_contract.py` | 契约测试: photometry_report.json vs schema (5 项验证) |

### 原始日志
| 文件 | 说明 |
|------|------|
| `raw_logs/test_photometric_calib_p12_001.log` | 单元测试日志 (5 项, 2/5 PASS) |
| `raw_logs/test_contract.log` | 契约测试日志 (5 项, 5/5 PASS) |
| `raw_logs/stage1_cli_output.log` | Orchestrator stage1 CLI 输出 (含 quality_metric 事件) |
| `raw_logs/photometry_report.json` | photometry_report.json 副本 (含 17 个诊断字段) |
| `raw_logs/git_log_oneline.txt` | git log --oneline -5 |
| `raw_logs/git_status.txt` | git status --short |

## 证据完整性
- [x] TASK_REPORT.md 完整
- [x] TEST_REPORT.md 完整
- [x] EVIDENCE_INDEX.md 完整
- [x] spec.md 完整
- [x] 单元测试日志落盘
- [x] 契约测试日志落盘
- [x] CLI 输出日志落盘
- [x] photometry_report.json 副本落盘
- [x] git 状态记录

## 关键结果
- **单元测试**: 2/5 PASS (3 FAIL 因预存 KD-tree bug, P12-002 范围)
- **契约测试**: 5/5 PASS
- **CLI 验证**: photometry_report.json + quality_metric 事件 + 8 阶段日志埋点 全部通过
- **回归**: 无 (P12-001 未修改算法核心逻辑, KD-tree bug 为预存问题)
