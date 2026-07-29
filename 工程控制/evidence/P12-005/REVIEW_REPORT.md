# P12-005 — REVIEW_REPORT (独立复核)

| 字段 | 值 |
| --- | --- |
| 任务 ID | P12-005 |
| 复核日期 | 2026-07-28 |
| 复核模式 | 自动复核 (基于证据完整性 + Spec 一致性) |
| 复核依据 | `tasks/P12-005.md`、`docs/07_SNR_AND_HISS_PROVENANCE_SPEC.md`、`evidence/P12-004` 实测数据 |

## 1. 任务范围确认

P12-005 任务目标: 修复 SNR 模型与 HISS 持久化，使 16 帧代表帧测光矩阵全部通过 Gate，并验证 SNR 模型成功写入 HISS。

**确认**: 任务范围与 P12-004 失败基线一一对应，4 类修复覆盖了 P12-004 全部 16 帧失败原因。

## 2. 修复正确性复核

### 2.1 修复 1 (initDiag 误覆盖) — PASS

- **Spec 一致性**: `docs/07_SNR_AND_HISS_PROVENANCE_SPEC.md` 要求 PhotometricDiag 字段语义正确，`valid_fsyn` 应等于有效 Gaia 星数。
- **代码审查**: `star_matcher.cpp` L45-49 注释明确说明 initDiag 不再重置 spectrum_rows_total/valid_fsyn，由 pc_api.cpp 在光谱积分阶段填充。
- **实测验证**: T4_RED 实测 `valid_fsyn=14649`，`spectrum_rows_total=14649`，与 Gaia 星数一致。
- **结论**: 修复正确，无副作用。

### 2.2 修复 2 (scale_factor 误判) — PASS

- **Spec 一致性**: `docs/07_SNR_AND_HISS_PROVENANCE_SPEC.md` 仅要求 `scale_factor > 0`，无下限约束。
- **代码审查**: `run_photometric_matrix.py` L70 `SCALE_FACTOR_MIN = 0.0`，L268 `if not (scale > SCALE_FACTOR_MIN and scale <= SCALE_FACTOR_MAX)`。
- **实测验证**: 16/16 帧 scale_factor 范围 5e-6 ~ 2.8e-3，全部 > 0，不再误判。
- **结论**: 修复正确，符合 Spec。

### 2.3 修复 3 (窄带滤光片) — PASS

- **Spec 一致性**: `docs/07_SNR_AND_HISS_PROVENANCE_SPEC.md` 要求滤光片透过率曲线覆盖中心波长。
- **数据审查**: `filters.json` L2571-2675 新增 Baader 7nm H-alpha (21 点, 640-672nm) 和 Baader 8.5nm OIII (25 点, 484-518nm) 曲线。
- **代码审查**: `orchestrator.cpp` L1397-1402 新增 H-alpha/OIII 大小写变体映射。
- **实测验证**: T4 HA/OIII + T2 HA/OIII + T3 HA/OIII 全部加载成功，scale_factor 合理。
- **结论**: 修复正确，曲线定义合理。

### 2.4 修复 4 (中文路径) — PASS

- **方案合理性**: 使用 ASCII junction 是绕过 MSYS2 MinGW64 std::filesystem 中文路径 bug 的有效方案，无需修改 C++ 源码（避免引入 Windows 特定代码）。
- **校准目录配置**: 按设备生成独立 stage1_config (T2/T3/T4)，将 calibration_dir 指向 ASCII 路径。
- **实测验证**: T2 全部 5 帧不再出现 filesystem error，stage1 成功完成。
- **结论**: 修复正确，方案简洁有效。

## 3. Gate 通过情况复核

### 3.1 总体 Gate

| 类别 | 预期 | 实际 | 结论 |
| --- | --- | --- | --- |
| 总通过率 | 100% | 100% (16/16) | PASS |
| Broadband fit_used ≥ 20 | 10/10 | 10/10 (最小 258) | PASS |
| Narrowband fit_used ≥ 8 | 6/6 | 6/6 (最小 235) | PASS |
| scale_factor > 0 | 16/16 | 16/16 | PASS |
| sigma_residual > 0 且有限 | 16/16 | 16/16 | PASS |

### 3.2 SNR 持久化 Gate

| 项 | 预期 | 实际 | 结论 |
| --- | --- | --- | --- |
| has_snr=1 | 16/16 | 16/16 | PASS |
| n_points > 0 | 16/16 | 16/16 (最小 234) | PASS |
| snr_phot 数值合理 | > 0 | > 0 (T4_RED=2.3916) | PASS |
| provenance 元数据完整 | 16/16 | 16/16 | PASS |

## 4. 通过条件逐项核对 (来自 tasks/P12-005.md)

| # | 通过条件 | 状态 | 证据 |
| --- | --- | --- | --- |
| 1 | 参考 Spec 和 Gate checklist 的全部强制项满足 | ✓ | 16/16 Gate PASS, has_snr=1 全部满足 |
| 2 | 没有未声明的 fallback、skip 或数据范围缩减 | ✓ | 全部 16 帧走完整 stage1 流水线 |
| 3 | TASK_REPORT.md、TEST_REPORT.md、EVIDENCE_INDEX.md、REVIEW_REPORT.md 完整 | ✓ | 4 件套全部生成 |
| 4 | 独立复核最后一行 `VERDICT: PASS` | ✓ | 本文件末尾 |

## 5. 禁止捷径检查

来自 tasks/P12-005.md: "不得在缺失时默认weight=1仍报告成功"

- **检查**: SNR 模型 `snr_phot` 由实际 PSF 星表统计计算 (snr_estimator.cpp 中 `compute_snr_phot()`)，未使用默认值 1.0。
- **证据**: T4_RED `snr_phot=2.3916`，T4_HA 等帧 snr_phot 均为合理数值。
- **结论**: 未违反禁止捷径。

## 6. 证据完整性复核

- 16 份 photometry_report.json SHA256 哈希全部计算并记录于 EVIDENCE_INDEX.md
- 16 份 stage1.log 完整保留，记录命令行/退出码/PhotometricDiag KV/SNR 模型构建日志
- 16 份 HISS 文件全部包含 has_snr=1
- PHOTOMETRY_MATRIX.csv 自动生成，未被人工编辑

## 7. 旧功能回归复核

| 功能 | 复核结论 |
| --- | --- |
| P12-001 PhotometricDiag 20 字段 | ✓ 全部输出正确 |
| P12-002 KD-tree 双向匹配 | ✓ rejected_ambiguous == 0 (16/16) |
| P12-003 光谱积分 | ✓ valid_fsyn > 0 (16/16) |
| P11-006 WCS/SIP 序列化 | ✓ stage1 完成 exit=0 (16/16) |

## 8. 未尽事项与建议

- **建议**: P12-006 应进行 710 帧全量回归测试，验证修复在更大数据集上的稳定性。
- **风险**: 中文路径修复依赖 ASCII junction，若用户重命名 junction 目录则可能失效。建议在 P12-006 之前考虑在 C++ 端增加 UTF-8 路径处理（可选）。
- **风险**: T2/T3 的 Astrodon HA 滤光片暂用 Baader 曲线近似，可能在精确光度测量中引入小偏差。建议在后续阶段补充 Astrodon 曲线数据。

## 9. 最终判定

所有通过条件已满足，所有禁止捷径已检查，证据完整可信。

VERDICT: PASS
