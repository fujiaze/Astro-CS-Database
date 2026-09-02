# P13-001 — EVIDENCE_INDEX

| 字段 | 值 |
| --- | --- |
| 任务 ID | P13-001 |
| 阶段 | P13 |
| Gate | G12 |
| 执行日期 | 2026-07-29 |
| Verdict | PASS |

## 1. 证据清单

| # | 类型 | 路径 | 说明 |
| --- | --- | --- | --- |
| 1 | TASK_REPORT | `evidence/P13-001/TASK_REPORT.md` | 任务报告（目标/设计/实现/验证/通过条件） |
| 2 | TEST_REPORT | `evidence/P13-001/TEST_REPORT.md` | 测试报告（10 用例 795 断言） |
| 3 | REVIEW_REPORT | `evidence/P13-001/REVIEW_REPORT.md` | 独立复核报告 |
| 4 | EVIDENCE_INDEX | `evidence/P13-001/EVIDENCE_INDEX.md` | 本文件 |
| 5 | Runner 脚本 | `evidence/P13-001/scripts/stage1_batch_runner.py` | Stage1 批处理 runner（710 帧） |
| 6 | 测试脚本 | `evidence/P13-001/scripts/test_stage1_batch_runner.py` | 自动测试（10 用例） |
| 7 | 测试日志 | `evidence/P13-001/raw_logs/test_run_20260729_120344.log` | 测试运行日志 |

## 2. 任务定义参考

- 任务文件：`tasks/P13-001.md`
- 参考 Spec：`docs/17_TEST_ARCHITECTURE_AND_FULL_REGRESSION.md`
- 数据集基线：`evidence/P11-005/DATASETS.md`（710 帧 = 7 数据集）

## 3. 交付物验证矩阵

| 交付项 | 任务要求 | 实际交付 | 验证 |
| --- | --- | --- | --- |
| runner | 批处理与恢复入口 | `stage1_batch_runner.py`（6 子命令） | ✓ |
| usage | 用法说明 | TASK_REPORT 第 2.3 节 + 脚本 `--help` epilog | ✓ |
| 自动测试 | 测试 runner 正确性 | `test_stage1_batch_runner.py`（10 用例 795 断言） | ✓ |
| 超时保护 | 有超时 | `subprocess.run(timeout=...)`，默认 600s 可配置 | ✓ |
| hash 缓存 | 绑定 commit/config/input hash | 7 元 hash（commit+orch+config+filters+qe+fits+gaia） | ✓ |
| 断点 | 断点恢复 | `batch_state.json` 持久化 + `run` 默认恢复 | ✓ |
| 分类报告 | 分类报告 | CSV + JSON（by status/device/dataset/filter + failure_classification） | ✓ |

## 4. 测试结果摘要

| 测试 | 断言数 | PASS | FAIL |
| --- | --- | --- | --- |
| test_scan_testdata | 14 | 14 | 0 |
| test_canonical_filter_from_filename | 14 | 14 | 0 |
| test_compute_hash_key_stable | 4 | 4 | 0 |
| test_cache_save_load | 5 | 5 | 0 |
| test_state_save_load | 4 | 4 | 0 |
| test_breakpoint_resume_skips_pass | 6 | 6 | 0 |
| test_fresh_clears_state_and_cache | 6 | 6 | 0 |
| test_failure_classification | 10 | 10 | 0 |
| test_filter_frames | 724 | 724 | 0 |
| test_smoke_run_1_frame | 14 | 14 | 0 |
| **合计** | **795** | **795** | **0** |

## 5. 冒烟测试关键指标

| 指标 | 值 | 阈值 | 通过 |
| --- | --- | --- | --- |
| status | PASS | PASS | ✓ |
| exit_code | 0 | 0 | ✓ |
| elapsed_s | 29.2 | < 600 | ✓ |
| fit_used | 1670 | >= 20 | ✓ |
| scale_factor | 0.002836 | > 0 | ✓ |
| sigma_residual | 0.1816 | > 0 且有限 | ✓ |
| has_snr | 1 | 1 | ✓ |
| snr_n_points | 1984 | > 0 | ✓ |

## 6. testdata 扫描覆盖

| 数据集 | 设备 | 帧数 | DATASETS.md 基线 | 一致 |
| --- | --- | --- | --- | --- |
| Victory_Nebula_T4_Flying_Dutchman | T4 | 228 | 228 | ✓ |
| Galaxy_Center_T4 | T4 | 157 | 157 | ✓ |
| NGC55_T3_flying_dutchman | T3 | 79 | 79 | ✓ |
| NGC247_T2_flying_dutchman | T2 | 68 | 68 | ✓ |
| NGC1727_T2_flying_dutchman | T2 | 64 | 64 | ✓ |
| NGC83_cluster_T3_Flying_Dutchman | T3 | 72 | 72 | ✓ |
| LDN43_T2_flying_dutchman | T2 | 42 | 42 | ✓ |
| **合计** | | **710** | **710** | ✓ |

## 7. VERDICT

```
VERDICT: PASS
```
