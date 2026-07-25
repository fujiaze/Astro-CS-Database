# TEST_REPORT: P00-006 审计复核可重复性验证

## 测试目标
验证 audit_reconciliation.json 的合并脚本可重复运行，结果一致，且 163 项全覆盖。

## 测试环境
- **仓库**: f:\Astro dev\Astro CS Normalization Database
- **Commit**: 39e049b
- **分支**: main

## 测试 1: 合并脚本可重复性
- **命令**: `python merge_audit.py`
- **退出码**: 0
- **stdout**: `OK: 163 items unified` / `OPEN=112 CLOSED=50 STALE=0 UNVERIFIED=0 REJECTED=1`
- **重复运行**: 结果一致

## 测试 2: 条目总数验证
| 优先级 | 预期 | 实际 | 结果 |
|---|---|---|---|
| P0P1 | 50 | 50 | PASS |
| P2 | 54 | 54 | PASS |
| P3 | 59 | 59 | PASS |
| 合计 | 163 | 163 | PASS |

## 测试 3: 状态分布验证
| 状态 | 数量 | 占比 | 结果 |
|---|---|---|---|
| OPEN | 112 | 68.7% | PASS |
| CLOSED | 50 | 30.7% | PASS |
| REJECTED | 1 | 0.6% | PASS |
| STALE/UNVERIFIED | 0 | 0% | PASS |

## 测试 4: 抽查 CLOSED 项证据
- B3-C-03 (RANSAC 尺度预检查): ipv_ransac.cpp:398 s_min/s_max ±10% — MATCH
- B3-C-04 (动态阈值): ipv_robust_refine.cpp:600-701 1.4826×MAD + Tukey — MATCH
- B1-H-3 (XISF 错误处理): 已补全 fclose+aio_log — MATCH

## 测试 5: 抽查 OPEN 项证据
- B1-C-1 (PipelineStage 5 阶段): aio_pipeline.h 仍为 5 阶段枚举 — OPEN 确认
- B2-C-1 (cal_stats 未写入): ac_api.cpp 无 cal_stats 写入路径 — OPEN 确认

## 测试 6: 模块覆盖完整性
9 个模块（B1-B9）全部覆盖，与审计总报告一致。

## 结论
- 163 项全部标记状态
- 合并脚本可重复
- 抽查 CLOSED/OPEN 项证据均 MATCH
- **VERDICT: PASS**
