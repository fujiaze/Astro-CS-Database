# phase1/stars — StarDetector (L2 模块 README)

- 合同: `P1-003` / SCI-PSF-001 / SCI-PHOT-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/stars/star_detector.h`
- Source: `lib/phase1/stars/star_detector.cpp`
- Test: `tests/unit/p1_stars_test.cpp` (7 组: 孤立/重叠/饱和/边缘/纯噪声/tie-breaker/catalog)

## 职责
局部峰检测 + 质心/二阶矩 (FWHM/ellipticity) + sigma-clip 背景估计。
去重: flux 降序 + tie breaker (更左优先)。质量位: 1=饱和 2=边缘 4=重叠。
Catalog: 坐标(px)/flux(ADU)/FWHM(px)/SNR/质量位/id。

## 合同要点 (不抄完整公式)
- 5σ 检测阈值; 纯噪声无显著误报。
- 公式与单位见 SCI-PSF-001 / SCI-PHOT-001。
