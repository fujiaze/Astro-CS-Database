> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 1. weight_mode 三面互斥（`FZ-MODE-*` / `FZ-FIELD-WEIGHTMODE`）

| mode | 状态 | 权重对象 | 单位 | 权威式 | covariance 来源 | effective PSF | 组内归一 | 禁止声明 |
|---|---|---|---|---|---|---|---|---|
| point_information | PRODUCTION_FROZEN | W_info | ADU^-2 | `Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var=1/W` | combination_coefficients | True | False | support/coverage/median_source_snr/fwhm/residual as weight; pixel ivar equivalence for arbitrary PSF |
| surface_gls | PRODUCTION_FROZEN | A^T C^-1 A | 1/(surface_brightness^2) | `x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1` | combination_coefficients | True | False | pixel ivar unconditional optimality |
| psfsw_robust | PRODUCTION_FROZEN | psfsw_robust_weight | 1 | `Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j)` | combination_coefficients | True | True | ivar; fisher_information; variance_from_weight; 1/W_psfsw |
| equal | DOCUMENTED_BASELINE | unit_weight | 1 | `I_out=mean_k d_k` | combination_coefficients | True | False | scientific optimality |
| pixel_ivar | DOCUMENTED_BASELINE | pixel_ivar | 1/BUNIT^2 | `I_out=Sum_k w_k d_k/Sum_k w_k, w_k=1/v_k` | combination_coefficients | True | False | point-source optimality for arbitrary PSF |
| psf_snr_power | DEFERRED_NOT_PRODUCTION | ratio_of_powers | 1 | `DEFERRED (未冻结)` | combination_coefficients | True | True | production; fisher_optimality |

- 生产 = ['point_information', 'surface_gls', 'psfsw_robust']；文档基线 = ['equal', 'pixel_ivar']；延迟 = ['psf_snr_power']。
- legacy 整数 {0=support×snr², 1=equal, 2=ivar} 一律被取代，`legacy_integer_allowed=false`；0 不得进任何科学权重面。
- 生产枚举禁止值：['psf_snr_power', 'auto', 'support_x_snr2', 0]。

## 2. 禁止作为权重来源的诊断别名（`FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE`）

`median_source_snr, median_snr, source_snr_median, med_source_snr, support, support_area, coverage, coverage_area, fwhm, psf_fwhm, median_fwhm, source_fwhm, residual, psf_residual, psf_fit_residual, fit_residual, psfsw_robust_weight, psfsw`

任一 token 出现在 `weight.sources` / `weight_value` / `covariance.variance_from` 即 REJECT。
注意：`psfsw_robust_weight` 与 `psfsw` 也在禁止来源集内——psfsw 权重不得被当作 ivar/W_info/方差来源。

## 3. psfsw 产物禁止键（任何层命中即 REJECT，`FZ-GATE-PSFSW-COV`）

`ivar, inverse_variance, variance, var, sigma, sigma2, fisher, fisher_information, information, w_info, w_psf`

扩展守卫（登记）：`snr, snr2, support, coverage`

## 4. 双词表映射建议（归 W6 SCHEMA-INTEGRATE-001 归一，不发明第三套）

| 语义 | SCI-PSFW 词表 | SCI-P2 词表 |
|---|---|---|
| 权重对象种类 | `weight_kind="relative_dimensionless"` | `weight.kind="psfsw_robust_weight"` |
| 权重单位串 | `weight_units="1"` | `weight.units="dimensionless_relative"` |
| 归一域 | `normalization.scope="group"` | `group_normalized=true` |
| 组内 median 目标 | `normalization.median_target=1.0` | `group_normalized=true (median=1)` |

## 5. 各模式声明的科学原始量（禁止诊断量）

| mode | declared weight sources |
|---|---|
| point_information | psf, photometric_response, noise_covariance |
| surface_gls | design_matrix, noise_covariance, upm_scale |
| psfsw_robust | psfsw.signal, psfsw.concentration, psfsw.noise, psfsw.background |
| equal | unit_weight |
| pixel_ivar | pixel_ivar |
