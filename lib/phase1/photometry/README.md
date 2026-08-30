# phase1/photometry — Photometer (L2 模块 README)

- 合同: `P1-004` / SCI-PHOT-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/photometry/photometer.h`
- Source: `lib/phase1/photometry/photometer.cpp`
- Test: `tests/unit/p1_wcs_phot_test.cpp` (已知通量解析恢复 20%; 失败显式)

## 职责
Aperture 光栅积分 (背景扣除) + Poisson 误差 + SNR。
失败显式 valid=false + reason (不留貌似有效的空 catalog)。
边界中心/无 sky 像素/空 aperture → 显式失败。
