# AstroCS 测试与验收 (V19)

## 快速回归

```powershell
# 模块级
lib\snr_estimator\cpp\test\noise_model_science_test.exe          # 32 项
lib\healpix_db\healpix_drizzle\tests\variance_propagation_test.exe  # 8 项 (MC 4000, ~33s)
lib\phase2\build\phase2_synthetic_gate.exe                       # 74 项 (~43s)
lib\astro_image_io\tests\pipeline_frame_contract_test.exe
```

## 完整验收 (V19 范围)

- Round0 契约冻结
- Round1 实施
- Round2 全仓代码质量 (shipping 清单/warning/static coverage)
- Round3 科学矩阵 (SNR blocker + CAL/PSF/AST/PHOT/DRZ/UPM/REJ/INT/E2E)
- Round4 Drizzle 操作/资源审查
- Round5 ≥15 红队假设
- Round6 clean-tree 终验 + docs 一致性

## 判定原则

1. 已有更严格 frozen oracle → 不放宽
2. 解析恒等式 → 数值误差门
3. MC → bias + confidence interval
4. 每个 PASS 必须含 input seed/config、truth、metric、threshold 来源、
   actual、artifact hash

## V20 不执行 (如实)

BASS 大数据、真实 16-exposure、GC/t4 regression、真实 2×2/3×3 topology。
