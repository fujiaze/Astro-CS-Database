# P11-004 证据清单

| 字段 | 值 |
|------|-----|
| 任务 ID | P11-004 |
| 生成日期 | 2026-07-28 |
| 证据目录 | `engineering_v1.3/evidence/P11-004/` |

## 1. 决策与报告文件

| # | 文件路径 | 用途 |
|---|----------|------|
| 1 | `P11_004_DECISION.md` | 最终决策：WCS_PRODUCTION_FIX_REQUIRED（HEADER_REGENERATION_NO_CODE_CHANGE） |
| 2 | `TASK_REPORT.md` | 任务报告（目标、流程、16 帧结果表、修复前后对比） |
| 3 | `TEST_REPORT.md` | 测试报告（双层闭环方法、门限、通过率、失败分析） |
| 4 | `EVIDENCE_INDEX.md` | 证据清单（本文件） |
| 5 | `REVIEW_REPORT.md` | 复核报告（结论、根因、残留风险、后续建议） |
| 6 | `ISSUES_DEFERRED.md` | 遗留问题（P11-002 kd-tree 不可比问题，本次已绕过） |

## 2. 批量汇总文件

| # | 文件路径 | 用途 | 关键字段 |
|---|----------|------|----------|
| 1 | `reports/gate_v2_final/batch_summary.json` | 初始 16 帧批量结果 | n_passed=10, n_failed=6, pass_rate=0.625 |
| 2 | `reports/gate_v2_final/repair_summary.json` | 6 失败帧修复记录 | n_success=6, overall_ok=true, before/after 对比 |
| 3 | `reports/gate_v2_post_repair/batch_summary.json` | 修复后 6 帧验证结果 | n_passed=6, n_failed=0, pass_rate=1.0 |
| 4 | `reports/diagnosis_summary.json` | 早期 8 帧诊断汇总 | 历史数据，已被 gate_v2_final 取代 |
| 5 | `reports/wcs_construction_comparison.json` | WCS 构建等价性证明 | 8/8 帧 EQUIVALENT，投影差异 1e-10 px |

## 3. 单帧证据（16 帧 × 2 文件 = 32 个文件）

### 3.1 初始 Gate v2 验证（gate_v2_final/）

每帧目录 `reports/gate_v2_final/<frame_id>/` 下：

| 文件 | 用途 | 格式 |
|------|------|------|
| `closure_report.json` | 闭环诊断报告（A/B 层残差 + gate 检查） | JSON |
| `matched_pairs_authoritative.jsonl` | 权威星对明细（每对的 det/gaia/internal_pred/external_pred/residual） | JSONL |

16 帧清单：

| # | frame_id | closure_report | matched_pairs | gate (修复前) |
|---|----------|----------------|---------------|----------------|
| 1 | T4_RED_Galaxy_Center | ✅ | ✅ | ✅ PASS |
| 2 | T4_GREEN_Galaxy_Center | ✅ | ✅ | ✅ PASS |
| 3 | T4_BLUE_Galaxy_Center | ✅ | ✅ | ✅ PASS |
| 4 | T4_HA_Galaxy_Center | ✅ | ✅ | ✅ PASS |
| 5 | T4_OIII_Galaxy_Center | ✅ | ✅ | ✅ PASS |
| 6 | T2_RED_LDN43 | ✅ | ✅ | ❌ FAIL (has_sip=false) |
| 7 | T2_GREEN_LDN43 | ✅ | ✅ | ❌ FAIL (has_sip=false) |
| 8 | T2_BLUE_LDN43 | ✅ | ✅ | ❌ FAIL (has_sip=false) |
| 9 | T2_HA_LDN43 | ✅ | ✅ | ❌ FAIL (has_sip=false) |
| 10 | T2_OIII_NGC1727 | ✅ | ✅ | ❌ FAIL (has_sip=false) |
| 11 | T3_RED_NGC55 | ✅ | ✅ | ✅ PASS |
| 12 | T3_GREEN_NGC55 | ✅ | ✅ | ✅ PASS |
| 13 | T3_BLUE_NGC55 | ✅ | ✅ | ✅ PASS |
| 14 | T3_HA_NGC55 | ✅ | ✅ | ✅ PASS |
| 15 | T3_OIII_NGC55 | ✅ | ✅ | ✅ PASS |
| 16 | T3_LUM_NGC55 | ✅ | ✅ | ❌ FAIL (has_sip=false) |

### 3.2 修复后验证（gate_v2_post_repair/）

仅 6 个修复帧，每帧目录 `reports/gate_v2_post_repair/<frame_id>/` 下：

| # | frame_id | closure_report | matched_pairs | gate (修复后) |
|---|----------|----------------|---------------|----------------|
| 1 | T2_RED_LDN43 | ✅ | ✅ | ✅ PASS |
| 2 | T2_GREEN_LDN43 | ✅ | ✅ | ✅ PASS |
| 3 | T2_BLUE_LDN43 | ✅ | ✅ | ✅ PASS |
| 4 | T2_HA_LDN43 | ✅ | ✅ | ✅ PASS |
| 5 | T2_OIII_NGC1727 | ✅ | ✅ | ✅ PASS |
| 6 | T3_LUM_NGC55 | ✅ | ✅ | ✅ PASS |

### 3.3 早期诊断（保留作历史对照）

| # | 路径 | 用途 | 状态 |
|---|------|------|------|
| 1 | `reports/T2_RED_LDN43_siril_v2/` | Siril v2 升级诊断（kd-tree，FAIL） | 历史，已被 authoritative_pairs 取代 |
| 2 | `reports/T2_RED_LDN43_nobright/` | 全星等 kd-tree 诊断（FAIL） | 历史，已被 authoritative_pairs 取代 |

## 4. 脚本文件

| # | 文件路径 | 用途 | 版本 |
|---|----------|------|------|
| 1 | `scripts/wcs_closure_diagnostic_v3.py` | v3 双层闭环诊断工具（authoritative_pairs 模式） | v3.4 (CRPIX 1-based 修复) |
| 2 | `scripts/repair_failed_frames.py` | 修复 6 失败帧，重新生成 FITS header | v3.5 |
| 3 | `scripts/generate_batch_config.py` | 生成 16 帧批量配置 | v1.0 |
| 4 | `scripts/compare_wcs_construction.py` | WCS 构建等价性验证（to_astropy_wcs vs WCS(header)） | v1.0 |
| 5 | `scripts/verify_crpix_offset.py` | CRPIX 0.5 偏移验证（用户否决，未运行） | v1.0 |
| 6 | `scripts/run_diagnosis.ps1` | 批量诊断 PowerShell 脚本 | v1.0 |
| 7 | `scripts/batch_frames.json` | 16 帧批量配置（初始验证） | v1.0 |
| 8 | `scripts/batch_frames_repaired.json` | 6 帧修复后批量配置 | v1.1 |
| 9 | `scripts/backup_failed_frames.json` | 6 失败帧备份配置 | v1.0 |

## 5. 原始日志

| # | 文件路径 | 用途 |
|---|----------|------|
| 1 | `raw_logs/batch_gate_v2.log` | 初始 16 帧 Gate v2 批量日志 |
| 2 | `raw_logs/batch_gate_v2_post_repair.log` | 修复后 6 帧 Gate v2 批量日志 |
| 3 | `raw_logs/repair_failed_frames.log` | 修复 6 失败帧详细日志 |
| 4 | `raw_logs/backup_failed_frames.log` | 备份 6 失败帧原 header 日志 |
| 5 | `raw_logs/compare_wcs_construction_20260728_123150.log` | WCS 构建等价性对比日志 |
| 6 | `raw_logs/single_diag_v4.log` | 单帧诊断 v4 日志 |
| 7 | `raw_logs/single_diag_v5.log` | 单帧诊断 v5 日志 |
| 8 | `raw_logs/run_diagnosis.log` | 早期批量诊断日志 |
| 9 | `raw_logs/batch_gate_v2_v2.log` | Gate v2 v2 批量日志 |
| 10 | `raw_logs/batch_gate_v2_v3.log` | Gate v2 v3 批量日志 |
| 11 | `raw_logs/batch_gate_v2_v4.log` | Gate v2 v4 批量日志 |
| 12 | `raw_logs/T2_RED_LDN43_siril_v2.log` | T2_RED_LDN43 Siril v2 诊断日志 |
| 13 | `raw_logs/T2_RED_LDN43_nobright.log` | T2_RED_LDN43 全星等诊断日志 |

## 6. 备份文件

| # | 路径 | 用途 |
|---|------|------|
| 1 | `backups/` (子目录) | 6 失败帧原 FITS header 备份（用于回滚） |

## 7. 文件统计

| 类别 | 数量 | 备注 |
|------|------|------|
| 决策与报告 Markdown | 6 | DECISION + TASK/TEST/EVIDENCE/REVIEW + ISSUES_DEFERRED |
| 批量汇总 JSON | 5 | gate_v2_final + repair_summary + gate_v2_post_repair + diagnosis_summary + wcs_construction_comparison |
| 单帧 closure_report.json | 16 + 6 = 22 | 初始 16 + 修复后 6 |
| 单帧 matched_pairs_authoritative.jsonl | 16 + 6 = 22 | 初始 16 + 修复后 6 |
| 早期诊断 closure_report/matched_pairs | 2 × 2 = 4 | T2_RED_LDN43_siril_v2 + T2_RED_LDN43_nobright |
| 脚本 | 9 | wcs_closure_diagnostic_v3 + repair_failed_frames + 等 |
| 原始日志 | 13 | 批量 + 单帧 + 修复 + 对比 |
| 备份 | 6 个 FITS | 6 失败帧原 header |
| **合计** | **~83** | |

## 8. 关键证据完整性

| 证据 | 完整性 | 说明 |
|------|--------|------|
| 16 帧 closure_report.json | ✅ 完整 | 全部有效（含 6 修复后重新验证） |
| 16 帧 matched_pairs_authoritative.jsonl | ✅ 完整 | 全部有效 |
| 6 帧修复前后对比 | ✅ 完整 | repair_summary.json 含 before/after 字段 |
| WCS 构建等价性证明 | ✅ 完整 | 8/8 帧 EQUIVALENT |
| 原始 header 备份 | ✅ 完整 | 6 个 FITS 备份文件可回滚 |
| Siril 风格剔除日志 | ✅ 完整 | n_after_rejection == n_authoritative_pairs（未剔除） |

## 9. 数据来源说明

- **A 层 inlier**：C++ `IPVSolver.get_last_inliers()` 直接导出，未经 Python 修改
- **B 层 WCS**：`astropy.wcs.WCS(header)` 从 FITS Header 独立构建，未调用 PlateSolve transform
- **Gaia 星表**：`gaia_client_cone_search_for_solver` C API 查询，与 PlateSolve 内部使用同一接口
- **坐标转换**：solver U → astropy 0-based pixel（v3.4 CRPIX 1-based 修复后）
- **迭代剔除**：Siril 1.4.3 风格（35 分位 sigma, 10σ 软剔除, 5 次迭代）
