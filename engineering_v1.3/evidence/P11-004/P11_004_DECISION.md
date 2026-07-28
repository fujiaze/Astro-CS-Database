# P11-004 Decision

| 字段 | 值 |
|------|-----|
| Decision ID | ADR-P11-004-GATE-V2-OUTCOME |
| 日期 | 2026-07-28 |
| Outcome | **WCS_PRODUCTION_FIX_REQUIRED** |
| 修复性质 | HEADER_REGENERATION_NO_CODE_CHANGE |
| 任务 ID | P11-004 |
| 决策依据 | AUTONOMOUS_ENTRY.md §2 + 23_P11_004_REVIEW_DECISION.md + 26_P11_RECOVERY_RUNBOOK.md |

## 1. 权威星对 Schema

- **数据来源**：`IPVSolver.get_last_inliers()` 导出的 `SolveInlierCache`（C++ 端）
- **字段**：`pair_id, gaia_ra, gaia_dec, det_x_px, det_y_px, internal_pred_x_px, internal_pred_y_px, external_pred_x_px, external_pred_y_px, internal_residual_*, external_residual_*, abs_delta_pred_*`
- **Schema 版本**：`wcs_authoritative_pairs.schema.json` v1.0
- **导出路径**：`reports/gate_v2_final/<frame_id>/matched_pairs_authoritative.jsonl`

## 2. 测试帧清单

- **代表帧总数**：16（T4×5 + T2×5 + T3×6）
- **初始验证**：10/16 PASS，6/16 FAIL（详见 `reports/gate_v2_final/batch_summary.json`）
- **修复后验证**：6/6 修复帧全部 PASS（详见 `reports/gate_v2_post_repair/batch_summary.json`）
- **失败帧清单**（修复前）：
  - T2_RED_LDN43, T2_GREEN_LDN43, T2_BLUE_LDN43, T2_HA_LDN43
  - T2_OIII_NGC1727
  - T3_LUM_NGC55

## 3. Solver Inlier 计数一致性

- **A 层（求解器内部）**：n_inliers ∈ [30, 45]，RMS ∈ [0.052, 0.288] px
- **A 层 p68** ∈ [0.059, 0.237] px
- **A/B 一致性**：所有 16 帧 `n_after_rejection == n_authoritative_pairs`（诊断工具未剔除任何 inlier）
- **结论**：求解器权威星对在 B 层独立回投中保持完整，未引入匹配丢失

## 4. 外部 vs 内部预测统计（delta_pred_dist）

| 统计 | 值（px） |
|------|----------|
| 全局 median | ~1700（与图像尺寸 ~4096 一致，符合"外部 WCS 投影 vs 内部 TRANS 投影使用不同坐标原点"的预期） |
| 通过帧 delta_pred_dist_median | 1240–2101（非异常，仅证明两层预测来自不同变换链） |
| 失败帧 delta_pred_dist_median | 1270–2026（与通过帧同量级，证明失败非求解器内部错误） |

> delta_pred 用于检测"内部预测与外部 WCS 预测是否来自同一变换"。本测试中两者均经独立坐标变换（solver U 系 vs astropy 0-based pixel），距离大是预期的（坐标原点不同），不能直接作为 gate。

## 5. 外部 vs Detector 残差统计（B 层硬 Gate）

### 5.1 修复前（16 帧，原 FITS header）

| 状态 | 帧数 | layer_b_p68 中位数 | layer_b_p90 中位数 | layer_b_p99 中位数 |
|------|------|-------------------|-------------------|-------------------|
| PASS | 10 | 0.076–0.220 | 0.092–0.325 | 0.107–0.476 |
| FAIL | 6 | 5.96–7.27 | 9.37–13.41 | 12.31–17.95 |

### 5.2 修复后（6 帧，重新生成 header）

| frame_id | has_sip | sip_order | layer_b_p68_px | layer_b_p90_px | layer_b_p99_px | gate |
|----------|---------|-----------|---------------|----------------|----------------|------|
| T2_RED_LDN43 | true | 3 | 0.160 | 0.224 | 0.336 | ✅ |
| T2_GREEN_LDN43 | true | 3 | 0.149 | 0.185 | 0.248 | ✅ |
| T2_BLUE_LDN43 | true | 3 | 0.228 | 0.390 | 0.828 | ✅ |
| T2_HA_LDN43 | true | 3 | 0.133 | 0.183 | 0.290 | ✅ |
| T2_OIII_NGC1727 | true | 3 | 0.141 | 0.221 | 0.292 | ✅ |
| T3_LUM_NGC55 | true | 3 | 0.138 | 0.241 | 0.392 | ✅ |

### 5.3 Gate 门限

| 条件 | 门限 | 说明 |
|------|------|------|
| layer_b_external_p68 ≤ 0.75 px | 残差 1σ | 反映系统偏差水平 |
| layer_b_external_p90 ≤ 1.5 px | 90 分位残差 | 反映大部分星点精度 |
| layer_b_external_p99 ≤ 3.0 px | 99 分位残差 | 反映极端离群值 |
| n_authoritative_pairs ≥ 5 | 最小样本数 | 保证统计有效性 |

## 6. Blind Catalog Diagnostic Summary

- **决策**：按 AUTONOMOUS_ENTRY.md §2 第 4 条，`--authoritative-pairs` 模式禁止启用 C 层 blind kd-tree rematching
- **理由**：blind 重匹配会产生暗星误配、饱和质心偏差和非唯一配对，残差与 IPV 内部 RMS 不可比（详见 `ISSUES_DEFERRED.md`）
- **二级健康检查**：仅在 P11-005 全量回归时对失败帧单独启用

## 7. Production Files Changed

### 7.1 修改的 FITS 文件（6 个）

修复脚本：`scripts/repair_failed_frames.py`（v3.5）
修复方式：调用 `solve_and_write_wcs(overwrite=True)` 重新求解并写入 header

| # | 文件 | 修改前 | 修改后 |
|---|------|--------|--------|
| 1 | `testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |
| 2 | `testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |
| 3 | `testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |
| 4 | `testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |
| 5 | `testdata/NGC1727_T2_flying_dutchman/lights/NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |
| 6 | `testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts` | CRPIX=(2048.0, 2048.0), 无 SIP | CRPIX=(2048.5, 2048.5), SIP order=3 |

### 7.2 未修改的代码文件

按 AUTONOMOUS_ENTRY.md §2 约束，本次修复未触及任何 C++ 代码：

- `lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp` — 未修改（当前版本已正确生成 SIP）
- `lib/plate_solve/cpp/ipv/src/ipv_sip.cpp` — 未修改
- `lib/plate_solve/python/solve_and_write_wcs.py` — 未修改

### 7.3 诊断工具升级（仅 evidence 目录，不算生产代码）

| 文件 | 变更 | 用途 |
|------|------|------|
| `scripts/wcs_closure_diagnostic_v3.py` | 新增 v3.0（authoritative_pairs 模式） | A/B 双层闭环验证 |
| `scripts/wcs_closure_diagnostic_v3.py` (v3.3) | 修复 `u_to_astropy_pixel` Y 轴方向 | solver U 系与 astropy 0-based pixel 系正确转换 |
| `scripts/wcs_closure_diagnostic_v3.py` (v3.4) | 修复 CRPIX 1-based/0-based 转换 | `wcs.wcs.crpix` 返回 FITS 1-based 值，需 -1 转换为 0-based |
| `scripts/repair_failed_frames.py` | 新增 v3.5 | 重新求解 6 个失败帧并写入正确 header |

## 8. Reasoning

### 8.1 触发条件

按 AUTONOMOUS_ENTRY.md §2 第 2 条："权威星对闭环失败且出现一致的符号、旋转、尺度或位置误差 → 才允许在 WCS 生产端最小修复"。

6 帧失败呈现**一致的尺度+位置误差**：
- 一致的 SIP 缺失（has_sip=false, sip_order=0）
- 一致的 B 层残差异常（p68 ≈ 6–7 px，p99 ≈ 12–18 px）
- 一致的 CRPIX 偏差（2048.0 而非 2048.5）

满足触发条件。

### 8.2 根因定位

通过 `compare_wcs_construction.py` 与 `verify_crpix_offset.py` 实验确认：

1. **当前 C++ 代码（ipv_wcs.cpp:287-288,348）已正确实现 SIP 输出和 CRPIX+0.5**，证据：对 6 帧重新求解后全部生成 has_sip=true, sip_order=3, CRPIX=(2048.5, 2048.5)
2. **6 帧的失败 FITS header 是历史遗留**：在 SIP 序列化功能完整实现前生成，未写入 A_ORDER/B_ORDER/AP_ORDER/BP_ORDER 系数
3. **B 层 astropy WCS 在缺失 SIP 时退化为 1 阶 TAN 投影**：无法拟合 3 阶光学畸变，导致残差异常

### 8.3 修复策略

采用 **HEADER_REGENERATION_NO_CODE_CHANGE** 策略：
- 用当前代码（已含 SIP 输出）对 6 个失败帧重新求解
- 写入新的 FITS header（含 A/B/AP/BP 系数，CRPIX 1-based）
- 不修改任何 C++ 源码、不修改 solve_and_write_wcs.py

### 8.4 验证结果

修复后 6/6 帧通过 B 层硬 Gate：
- p68 中位数 0.13–0.23 px（远低于 0.75 px 门限）
- p90 中位数 0.18–0.39 px（远低于 1.5 px 门限）
- p99 中位数 0.25–0.83 px（远低于 3.0 px 门限）

## 9. Risks and Follow-up

### 9.1 风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 修复覆盖了原 FITS header | 历史 header 已备份至 `engineering_v1.3/evidence/P11-004/backups/` | 备份完整可回滚 |
| 其他 testdata 帧可能也存在 header 历史遗留问题 | P11-005 全量回归需考虑 | P11-005 执行时统一使用当前代码重新求解 |
| ipv_wcs.cpp 内部仍有 CRPIX 实现冲突（line 165 vs 287） | 不影响当前修复，但需在 P11-006 评估 | 记录至 `ISSUES_DEFERRED.md` 待 P11-006 处理 |

### 9.2 后续任务

按 AUTONOMOUS_ENTRY.md §2 第 5 条："无论哪一分支 → 进入 P11-005 的 710 全量回归"：

- **P11-005**：PlateSolve 710 全量回归测试，对所有成功帧或分层抽样执行权威星对闭环
- **P11-006**：更新 WCS/SIP 契约和 provenance，处理 `ipv_wcs.cpp` CRPIX 实现冲突

## 10. 最终结论

P11-004 以 `WCS_PRODUCTION_FIX_REQUIRED` 完成。修复性质为数据修复（重新生成 6 个 FITS header），未修改任何生产代码。所有 16 帧代表帧现已通过权威星对双层闭环硬 Gate，可进入 P11-005 全量回归测试。
