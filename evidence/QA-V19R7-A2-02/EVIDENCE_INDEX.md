# EVIDENCE INDEX — QA-V19R7-A2-02 (noise 域)

> 任务: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-02  
> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` §4.1  
> 审计员: resident:science | 日期: 2026-08-22 | 基线: V19R6R2-W1 HEAD 2767874  
> 模式: 只读不改 (H+E)

## 产出文件

| 文件 | 说明 | 状态 |
|------|------|------|
| `reports/v19r7_quality/audit_findings_noise.md` | noise 域审计分表 (NOISE_MODEL + UNCERTAINTY_AND_COVARIANCE vs lib/snr_estimator + k_corr/α²v/ivar 追溯) | ✅ 已生成 |
| 本文件 `evidence/QA-V19R7-A2-02/EVIDENCE_INDEX.md` | 证据索引 | ✅ |

## 判定

- **覆盖**: 2 份 noise 文档 + `lib/snr_estimator` 全量 + `lib/phase2/src/sampler.cpp` k_corr 域 + `SCI-NOISE-001..015` 追溯，逐项对照完成。
- **分级结果**: **P0: 1, P1: 4, P2: 5**，合计 10 项。
- **P0 清单**:
  - NO-01 SCI-NOISE-005 gain/readnoise 域缺失：文档未声明 `snr_noise_gain_variance` 仅作 SNR-005 诊断交叉验证，不得注入生产 `NoiseWeightModelV1` ivar (`lib/snr_estimator/cpp/src/noise_model.cpp:457-464` / `lib/snr_estimator/cpp/include/snr_estimator.h:98-141`)
- **A Gate 依赖**: `reports/v19r7_quality/machine_consistency_before.json` (broken=0) 已就绪；本表人工复核完成。
- **重点核查**:
  - 三层模型符号一致 (仅 gain/floor 待补)
  - `k_corr=1.4` 与 `kcorr_lookup` 查表一致 (`lib/phase2/src/sampler.cpp:38-69`)
  - `α²v` / `ivar` / 协方差未建模声明正确 (P2 互引缺失除外)
- **下步**: B1-05 (NO-01/05-07/09-10), B1-06 (NO-02/08), B5-06/A1-02 (NO-03/04/05)。

## 输入清单 (只读)

- `docs/science/NOISE_MODEL.md`, `docs/science/UNCERTAINTY_AND_COVARIANCE.md`
- `lib/snr_estimator/**`, `lib/phase2/src/sampler.cpp` (k_corr), `docs/TRACEABILITY.csv:23-37`
- `reports/v19r7_quality/machine_consistency_before.json`, `reports/v19r7_quality/standards_violations.json`

## 方法

逐节对照 + 符号表 + 行号锚定 + TRACEABILITY 闭环，重点核查三层模型/k_corr/α²v/ivar/协方差，按 P0/P1/P2 分级。

## 关联 Checklist

`工程控制/checklists/QA_V19R7_QUALITY.md` A2-02 noise 域审计 — 本证据满足 H+E 要求。
