# EVIDENCE INDEX — QA-V19R7-A2-01 (science 域)

> 任务: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-01  
> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` §4.1  
> 审计员: resident:science | 日期: 2026-08-22 | 基线: V19R6R2-W1 HEAD 2767874  
> 模式: 只读不改 (H+E)

## 产出文件

| 文件 | 说明 | 状态 |
|------|------|------|
| `reports/v19r7_quality/audit_findings_science.md` | science 域审计分表 (SCIENCE_SCOPE/CALIBRATION/ASTROMETRY/PHOTOMETRY/PSF vs lib/calibration, plate_solve/ipv, dynamic_psf, photometric_calib, gaia_xpsd_client) | ✅ 已生成 |
| 本文件 `evidence/QA-V19R7-A2-01/EVIDENCE_INDEX.md` | 证据索引 | ✅ |

## 判定

- **覆盖**: 5 份 science 文档 (281 行) + 7 个实现目录 60+ 文件逐节对照完成。
- **分级结果**: **P0: 2, P1: 4, P2: 6**，合计 12 项。
- **P0 清单**:
  - SC-01 SCI-CAL-001 flat_norm=0 静默钳位 0.1 vs 文档“显式拒绝” (`lib/calibration/src/calibrator.cpp:90,120,164`)
  - SC-02 SCI-PHOT-001 `sigma_mag` IRLS+Tukey (c=4.685) vs 文档 median (`lib/photometric_calib/cpp/src/star_matcher.cpp:21,478-525`)
- **A Gate 依赖**: `reports/v19r7_quality/machine_consistency_before.json` (broken=0) 已就绪；本表人工复核完成，无“待 machine_consistency 复核”遗留。
- **下步**: B1-02 (CAL 失效域), B1-04 (PHOTOM 清洗公式), B1-03 (AST/PSF 单位与模型), B1-01/B5-06 (SCOPE 追溯补行)。

## 输入清单 (只读)

- `docs/science/SCIENCE_SCOPE.md`, `CALIBRATION.md`, `ASTROMETRY.md`, `PHOTOMETRY.md`, `PSF.md`
- `lib/calibration/**`, `lib/plate_solve/cpp/ipv/**`, `lib/dynamic_psf/**`, `lib/photometric_calib/**`, `lib/gaia_xpsd_client/**`
- `docs/TRACEABILITY.csv` SCI-* 行, `reports/v19r7_quality/machine_consistency_before.json`

## 方法

逐节对照 (科学定义/变量/单位/假设/有效域/不保证/失效条件/数值精度/ID) + 符号表 + 行号锚定 + TRACEABILITY 闭环，按 P0/P1/P2 分级。

## 关联 Checklist

`工程控制/checklists/QA_V19R7_QUALITY.md` A2-01 science 域审计 — 本证据满足 H+E 要求。
