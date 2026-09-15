# 权重来源机器判据门（median SNR / support / coverage / FWHM / residual 不得冒充权重）

文档 ID：`SCI-P2-001-WEIGHT-GATE`
实现：`run/v6/sci-p2/oracle/weight_provenance_gate.py`（独立合同门，只读 JSON，不调用生产实现）
驱动器：`run/v6/sci-p2/oracle/run_gate_and_mutations.py`
上位：冻结宪章 §4.1（量不混名）/§6.3；`PROJECT_SPEC` §5/§7/§8；`DESIGN-P2-001` §6.1/§6.2/§6.3/§9；
     `ASTROCS-SCIENCE-MODEL-001` §3/§10/§11；`SCI-PSFW-001` §3/§4/§5/§8；`RULINGS.md` #3/#4/#5。

## 1. 输入记录（`astrocs.phase2-weight-provenance/v1`）

```json
{
  "record_schema": "astrocs.phase2-weight-provenance/v1",
  "mode": "point_information | surface_gls | psfsw_robust | equal | pixel_ivar",
  "science_objective": "...",
  "weight": { "kind": "...", "sources": ["..."], "units": "...", "group_normalized": false },
  "primitives": { ... },                       // 科学原始量（PSF/光度响应/协方差/四分量/共同星集）
  "covariance": { "propagation": "C_out = R C_in R^T",
                  "combination_coefficients": [ ... ],
                  "variance_from": "combination_coefficients",
                  "input_covariance": "provided" },
  "effective_psf": { "definition": "impulse_response_of_combination",
                     "model": "gaussian_sigma_2.0", "values": [...], "normalization": "peak" },
  "diagnostics": { "median_source_snr": 25.0, "support": 0.8, "coverage": 0.9,
                   "fwhm": 4.71, "psf_residual": 0.05 }
}
```

用法：`python3 weight_provenance_gate.py <record.json>` → rc 0 合法 / rc 1 违规 / rc 2 用法错误。

## 2. 判据（R0–R8）

| 规则 | 内容 | 上位锚 |
|---|---|---|
| **R0** | `record_schema` 必须匹配；否则 REJECT | 可审计性 |
| **R1** | `mode` ∈ {point_information, surface_gls, psfsw_robust, equal, pixel_ivar}；未知模式（尤其 `psf_snr_power`）REJECT | `00_READ_FIRST.md`；`SCI-PSFW-001` §4 |
| **R2** | `weight.kind` 必须与 mode 匹配 | 宪章 §4.1 |
| **R3** | **诊断量不得出现在 `weight.sources`**：别名集 {median_source_snr, median_snr, source_snr_median, med_source_snr, support, support_area, coverage, coverage_area, fwhm, psf_fwhm, median_fwhm, source_fwhm, residual, psf_residual, psf_fit_residual, fit_residual} 任一命中即 REJECT；`sources` 全为诊断量亦 REJECT | `PROJECT_SPEC` §4/§7；`UNIFIED` §3/§11；`SCI-PSFW-001` §8 |
| **R4** | 每模式必需原始量必须**同时**出现在 `sources` 与 `primitives`：point_information ⊇ {psf, photometric_response, noise_covariance/ivar}；surface_gls ⊇ {design_matrix 或 normal_matrix, noise_covariance/ivar}；psfsw_robust ⊇ {psf_signal, signal_concentration, robust_noise, robust_background, common_star_set}；pixel_ivar ⊇ {pixel_ivar} | `DESIGN-P2-001` §6.1/§6.2/§6.3 |
| **R5** | 单位/归一语义：psfsw_robust 必须 `units=dimensionless_relative` 且 `group_normalized=true`，且 {ivar, inverse_variance, variance, fisher_information, 1/flux^2, flux^-2} 一律 REJECT；`common_star_set.n_common ≥ 3` 且 `selection_function` 非空；四分量必须分别落产品；point_information 单位须含 `flux`；surface_gls 单位须显式 | `SCI-PSFW-001` §3/§4/§6；`RULINGS.md` #4/#5 |
| **R6** | covariance 必填；`propagation` 非空；`combination_coefficients` 非空数值；`variance_from ∈ {combination_coefficients, linear_combination_coefficients, actual_combination_coefficients}`；若为 {weight(s), psfsw_robust_weight, psfsw, median_source_snr, median_snr, support, coverage, fwhm, psf_residual, residual, effective_psf, source_snr} 之一 → REJECT；psfsw_robust 另须 `input_covariance` | `SCI-PSFW-001` §5；`DESIGN-P3-001` §4；`RULINGS.md` #5 |
| **R7** | `effective_psf` 必填，须有 `definition` 且（非空 `values` 或非空 `model`）；只给 `fwhm` 单标量 → REJECT | `DESIGN-P2-001` §6.2/§6.3/§9；`SCI-PSFW-001` §8 |
| **R8** | `diagnostics` 只能是对象（诊断角色），不得作为权重来源 | `UNIFIED` §3 |

**核心负向判据（任务正文要求的"不得单独冒充"）**：
一份记录的权重若仅由 / 可归约为 {median_source_snr, support, coverage, fwhm, psf_residual} 表达，则 R3 必命中并 rc=1；
即使这些量以"科学来源"名义混入（如 `[psf, support, coverage, photometric_response, noise_covariance]`），R3 同样 rc=1。

## 3. 正向控制与负向 mutation（实测）

正向控制 5/5 被 ACCEPT（rc=0）：`PC1_point_information`、`PC2_surface_gls`、`PC3_psfsw_robust`、`PC4_equal`、`PC5_pixel_ivar`。
证明门不是"一律拒绝"的空门。

17 项负向 mutation 全部 REJECT（rc=1），注入点与期望规则：

| mutation | 注入 | 命中规则 |
|---|---|---|
| M01 | mode → psf_snr_power（未冻结模式进生产） | R1 |
| M02 | weight.sources=[median_source_snr]，primitives 只剩 median SNR | R3/R4 |
| M03 | psfsw_robust 权重 units → ivar | R5 |
| M04 | covariance.variance_from → psfsw_robust_weight | R6 |
| M05 | surface_gls weight.sources=[support, coverage] | R3 |
| M06 | point_information weight.sources=[fwhm] | R3 |
| M07 | psfsw_robust 删除 common_star_set（无共同星集门） | R4/R5 |
| M08 | effective_psf → {"fwhm": 4.71}（只给 FWHM） | R7 |
| M09 | surface_gls 删除 normal_matrix/design_matrix | R4 |
| M10 | point_information 删除 noise_covariance | R4 |
| M11 | psfsw_robust 删除 robust_background 分量 | R5 |
| M12 | covariance.combination_coefficients=[] | R6 |
| M13 | covariance.variance_from → median_source_snr | R6 |
| M14 | weight.sources 混入 support/coverage | R3 |
| M15 | effective_psf definition=""/values=[]/model="" | R7 |
| M16 | weight.sources 混入 psf_fit_residual 别名 | R3 |
| M17 | psfsw_robust group_normalized=false | R5 |

## 4. 结构性反例（诊断量在科学上也不充分）

Oracle C8.5 给出可复算反例：两个归一化 PSF（Gaussian σ=2.0 与 0.85·G(1.9)+0.15·G(6.0)）在
median source SNR / support / coverage / FWHM / residual 五个诊断量上相对差 < 5%，
但 `W_info` 相差 > 10%（实测 dmax=1.357%、W 相对差=12.357%）。因任意 `f(诊断)` 在两人诊断相等时给出相同输出，
其预测误差下界 ≥ |W_a − W_b|/2 > 0（Oracle C8.6）。这把"诊断不可冒充权重"从条款要求变成可执行判据。
