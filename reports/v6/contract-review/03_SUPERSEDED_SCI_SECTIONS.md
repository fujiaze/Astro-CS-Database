> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

> 回应 AR-032 / SO-06：列出与本包冻结冲突的**非 v6** `docs/science/*.md` 段落，**只登记清单与建议措辞，不得改写这些文件**。正式 amendment 须负责人签字后由 DOC-CONVERGE-001(W12) 统一收口。

| 编号 | 文件 | 段落 | 行 | 冲突事实 | 冻结取代条款 | 签字 |
|---|---|---|---|---|---|---|
| SUP-01 | docs/science/DRIZZLE.md | §3 物理量和单位 | 27 | `v,variance`: ADU²；`ivar`: ADU⁻² —— 把输入逐像素方差与输出面亮度方差同名写作 variance | FZ-UNIT-VAR-IN, FZ-UNIT-VAR-SB, FZ-UNIT-IVAR-SB, FZ-BUNIT-SEMANTICS | SO-01 |
| SUP-02 | docs/science/DRIZZLE.md | §5 权重与归一 / §7 不变量 | 38-51,79-84 | `w_jp=a_jp/A_drop`（A_drop=pixfrac²·A_pixel）；`S_p=B0` 仅在 pixfrac=1 成立；面亮度归一因子 A_drop vs A_pixel 分叉 | FZ-FORMULA-DRIZZLE-SB, FZ-FORMULA-DRIZZLE-VAR, FZ-COND-FLUX-CONSERV | SO-02 |
| SUP-03 | docs/science/DRIZZLE.md | §11 验收门 | 117 | `流量守恒：常数场 C 的 S_p=C 全像素 max_abs==0`（把每像素常量 ADU 与常量面亮度混同） | FZ-GATE-CONST-SB | SO-03 |
| SUP-04 | docs/science/DRIZZLE.md | §1 目的与非目标 / §9a 专属问题 | 8,146 | `协方差产品为非目标（仅方差传播，协方差文档化）`；`不存完整协方差矩阵` | FZ-GATE-PARENT-VAR, FZ-FORMULA-COV-PROP, FZ-PROV-SHARED-SYSTEMATIC | SO-04 |
| SUP-05 | docs/science/CONTROL_WEIGHT_SNR.md | § 像素级 SNR 权重（weight_mode=2） | 66,71 | `weight_mode=2`；`weights[s] = support[s] × snr_v²`（support×SNR² 作像素级权重） | FZ-MODE-PRODUCTION, FZ-FIELD-WEIGHTMODE, FZ-GATE-SUPPORT-COVERAGE, FZ-GATE-MEDIAN-SNR | SO-06 |
| SUP-06 | docs/science/ACR_EQUIVALENCE.md | §4 GPU 合同 | 33 | `weight_mode∈{auto,ivar,equal,support_x_snr2}` | FZ-FIELD-WEIGHTMODE, FZ-MODE-PRODUCTION | SO-06 |
| SUP-07 | docs/science/INTEGRATION.md | §1 目的与非目标 | 8 | `不存完整协方差`（作为 Integration 非目标） | FZ-FORMULA-COV-PROP, FZ-GATE-PARENT-VAR | SO-04/SO-06 |
| SUP-08 | docs/science/CALIBRATION.md | §1 目的与非目标 / §9a 专属问题 | 5,91,95 | `read noise：校准层不建模、不传播`；`variance 传播：本层不传播母版方差至 cal` | FZ-PROV-SHARED-SYSTEMATIC, FZ-FORMULA-COV-PROP | SO-06 |
| SUP-09 | docs/science/PHASE3_HIPS_TO_FITS.md | §1 范围与非目标 / §3 输出投影 | 20,100 | 非目标含 `variance/weight/ivar 输入产品`、`flux-per-pixel 输入模式`、`SIN/CAR 等非 TAN 投影` | FZ-P3-MODES, FZ-P3-FAILCLOSED, FZ-UNIT-IVAR-SB | SO-06 |
| SUP-10 | docs/science/UNCERTAINTY_AND_COVARIANCE.md | §V19R3 control estimator 方差 | 21,30-48 | `k_corr=1.3883`（保守 1.4）冻结值 + 口径不完整；`V19 不保存完整 covariance matrix` | FZ-PROV-KCORR, FZ-GATE-PARENT-VAR | SO-06/SO-07 |

## 建议措辞（逐条，仅为提案，未改写）

### SUP-01 — docs/science/DRIZZLE.md §3 物理量和单位（行 27）

- 冲突：`v,variance`: ADU²；`ivar`: ADU⁻² —— 把输入逐像素方差与输出面亮度方差同名写作 variance
- 取代条款：FZ-UNIT-VAR-IN, FZ-UNIT-VAR-SB, FZ-UNIT-IVAR-SB, FZ-BUNIT-SEMANTICS
- 建议措辞：把该行拆为 `pixel_variance_in`（输入 v_j，ADU^2）与 `sb_variance_out`（输出 variance_p，ADU^2/px^4）两名单列，`ivar` 单位改为 px^4/ADU^2；由负责人签字后由 DOC-CONVERGE-001 统一改写。
- 签字：SO-01

### SUP-02 — docs/science/DRIZZLE.md §5 权重与归一 / §7 不变量（行 38-51,79-84）

- 冲突：`w_jp=a_jp/A_drop`（A_drop=pixfrac²·A_pixel）；`S_p=B0` 仅在 pixfrac=1 成立；面亮度归一因子 A_drop vs A_pixel 分叉
- 取代条款：FZ-FORMULA-DRIZZLE-SB, FZ-FORMULA-DRIZZLE-VAR, FZ-COND-FLUX-CONSERV
- 建议措辞：标注 §5/§7 为 legacy 归一：V6 目标态为面亮度保持 S_p=Σ_j B_j a_jp/Σ_j a_jp（等价 w_SB_jp=a_jp/A_pixel,j=pixfrac²·w_legacy），通量守恒为条件不变量并写 provenance.flux_conservation_factor=pixfrac²。
- 签字：SO-02

### SUP-03 — docs/science/DRIZZLE.md §11 验收门（行 117）

- 冲突：`流量守恒：常数场 C 的 S_p=C 全像素 max_abs==0`（把每像素常量 ADU 与常量面亮度混同）
- 取代条款：FZ-GATE-CONST-SB
- 建议措辞：改为：常量场 Oracle 真值按面亮度 B0 构造 x_j=B0·A_pixel,j，门为 S_p=B0（|S_p/B0−1|<1e-3）对全部 pixfrac∈(0,1]；保留负向门（常量 ADU 构造/S_p=F_p 必红）。
- 签字：SO-03

### SUP-04 — docs/science/DRIZZLE.md §1 目的与非目标 / §9a 专属问题（行 8,146）

- 冲突：`协方差产品为非目标（仅方差传播，协方差文档化）`；`不存完整协方差矩阵`
- 取代条款：FZ-GATE-PARENT-VAR, FZ-FORMULA-COV-PROP, FZ-PROV-SHARED-SYSTEMATIC
- 建议措辞：改为：协方差/相关核为强制输出面；只存对角 variance 时必须另存 correlation kernel rho_ij 或可重建算子摘要 + 近似误差，并声明为下界；不得声明精确。
- 签字：SO-04

### SUP-05 — docs/science/CONTROL_WEIGHT_SNR.md § 像素级 SNR 权重（weight_mode=2）（行 66,71）

- 冲突：`weight_mode=2`；`weights[s] = support[s] × snr_v²`（support×SNR² 作像素级权重）
- 取代条款：FZ-MODE-PRODUCTION, FZ-FIELD-WEIGHTMODE, FZ-GATE-SUPPORT-COVERAGE, FZ-GATE-MEDIAN-SNR
- 建议措辞：标注为 ARCHIVED：support×snr² 权重形态被 UNIFIED §11 撤销；support/coverage 只作门，local_snr/frame_snr 只作相对质量诊断，不得作科学权重来源；生产权重枚举改为显式三模式。
- 签字：SO-06

### SUP-06 — docs/science/ACR_EQUIVALENCE.md §4 GPU 合同（行 33）

- 冲突：`weight_mode∈{auto,ivar,equal,support_x_snr2}`
- 取代条款：FZ-FIELD-WEIGHTMODE, FZ-MODE-PRODUCTION
- 建议措辞：标注为 ARCHIVED/HISTORICAL：ACR 等价门不得反向定义生产权重枚举；生产权重枚举以 FZ-MODE-PRODUCTION 为唯一来源，auto/support_x_snr2/legacy 整数不得出现在产品 schema 合法取值域。
- 签字：SO-06

### SUP-07 — docs/science/INTEGRATION.md §1 目的与非目标（行 8）

- 冲突：`不存完整协方差`（作为 Integration 非目标）
- 取代条款：FZ-FORMULA-COV-PROP, FZ-GATE-PARENT-VAR
- 建议措辞：改为：不存完整巨型矩阵仍成立，但必须输出对角 variance + 相关核/可重建算子摘要 + 近似误差门；协方差/相关核为强制输出面。
- 签字：SO-04/SO-06

### SUP-08 — docs/science/CALIBRATION.md §1 目的与非目标 / §9a 专属问题（行 5,91,95）

- 冲突：`read noise：校准层不建模、不传播`；`variance 传播：本层不传播母版方差至 cal`
- 取代条款：FZ-PROV-SHARED-SYSTEMATIC, FZ-FORMULA-COV-PROP
- 建议措辞：按 DESIGN-P1 §4.2 与 ADJ-OBS-01 改为：校准层传播 J C_in J^T 逐像素方差（read noise/photon/quantization/dark），共享 master 进 covariance 面（低秩/相关核/master ID+强度参数），模型偏差进 validity/quality；不存完整巨型矩阵。
- 签字：SO-06

### SUP-09 — docs/science/PHASE3_HIPS_TO_FITS.md §1 范围与非目标 / §3 输出投影（行 20,100）

- 冲突：非目标含 `variance/weight/ivar 输入产品`、`flux-per-pixel 输入模式`、`SIN/CAR 等非 TAN 投影`
- 取代条款：FZ-P3-MODES, FZ-P3-FAILCLOSED, FZ-UNIT-IVAR-SB
- 建议措辞：按 DESIGN-P3 §1/§3 与 ADJ-P3-01 改为：V6 目标态支持 variance（含相关核）输入、逐像素通量/面亮度双语义与 TAN/SIN/CAR/AIT 四投影；相位/采样核由版本化 kernel registry 注册并带独立 Oracle；须负责人签字后改写。
- 签字：SO-06

### SUP-10 — docs/science/UNCERTAINTY_AND_COVARIANCE.md §V19R3 control estimator 方差（行 21,30-48）

- 冲突：`k_corr=1.3883`（保守 1.4）冻结值 + 口径不完整；`V19 不保存完整 covariance matrix`
- 取代条款：FZ-PROV-KCORR, FZ-GATE-PARENT-VAR
- 建议措辞：部分取代（annotation）：保留按尺度查找表与 1.4 域内值，但必须补冻结定义 k_corr=Var(median)/[π σ_bg²/(2 N_retained)]、显式适用域（几何/pixfrac/patch/估计器/球面）与固定种子 MC 标定脚本；未复跑前禁止跨域外推。『不保存完整矩阵』本身不违规（对角+相关核合法）。
- 签字：SO-06/SO-07

## 覆盖声明

本清单覆盖 AR-032 点名的 `DRIZZLE.md` / `CONTROL_WEIGHT_SNR.md` / `ACR_EQUIVALENCE.md` / `INTEGRATION.md`，并补登 `CALIBRATION.md`（OI-03）、`PHASE3_HIPS_TO_FITS.md`（F3-05/F3-06 口径）与 `UNCERTAINTY_AND_COVARIANCE.md`（F-OBS-05 k_corr 口径，部分取代/需注记）。其余非 v6 `docs/science/*.md`（ASTROMETRY/NOISE_MODEL/PHASE2_UPM/PHOTOMETRY/PSF/REJECTION/SCIENCE_SCOPE/STAR_DETECTION/UNIFIED_SCIENCE_MODEL/PSF_SIGNAL_WEIGHT）本轮未登记冲突；UNIFIED 与 PSF_SIGNAL_WEIGHT 为上位权威（非取代对象）。
