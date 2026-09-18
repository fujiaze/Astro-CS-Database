# per-doc: docs/science/PSF.md（SCI-PSF-001）

## 发现

| 编号 | 行 | 主张 | 独立证据 | 判定 | 三分类 |
|---|---|---|---|---|---|
| R2-H-09 | 23,81,88,119,128 | robust_residual_sigma=residual_scale/0.7316727929211932 | 复算 E2：精确 0.73167309528061342；相对差 +4.13e-7；同族 STAR_PSF_ALGORITHMS.md:57 又写 0.731673σ | WRONG（微小） | DESIGN-OK-DOC-WRONG |
| R2-H-10 | 20,51 | Moffat4 FWHM=1.230310·σ | 复算 E3：精确 2√2·√(2^{1/4}−1)=1.2303076525901024；相对差 1.91e-6 | AMBIGUOUS（舍入） | DESIGN-OK-DOC-WRONG |
| — | 52 | flux=2πA·sx·sy/3 | 复算 E3：numeric 4.7123889748 vs 公式 4.7123889804（rel 1.2e-9） | CORRECT | — |
| R2-H-21 | 42 | Moffat 1969 A&A 3,455 | ADS 反爬（405），未逐页核验 | INSUFFICIENT-EVIDENCE | DESIGN-OK-EVIDENCE-MISSING |

## 处置建议

- 常数改全精度（0.7316730952806139、1.2303076525901）或标 "≈"；需与实现常数（noise_model.cpp:37、snr_science.cpp:39、dpsf_psf.cpp:25）及 tools/docs_machine_consistency.py 协同。
- 补 Moffat 1969 的 ADS/A&A 页级证据。
