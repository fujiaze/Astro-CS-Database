> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# PSFSW 门目录与负向 mutation 册

- 文档 ID：`ALG-P2-PSFSW-001-GATES`
- 机器可读同源：`psfsw_spec.json` → `gates` / `validation_gates` / `fail_closed` / `forbidden_*`
- 参考实现：`run/v6/alg-p2-psfsw/tools/psfsw_gate.py`（合同门，rc 0/1/2）+ `tools/psfsw_oracle.py`（独立数值 Oracle）

## 1. 门清单 PSFSW-G01..G25

| 门 | 判据 | 失败 rc | 锚 |
|---|---|---|---|
| PSFSW-G01 | `mode == psfsw_robust` 且在生产模式列表内 | 1 | `FZ-MODE-PRODUCTION` |
| PSFSW-G02 | `weight_kind == relative_dimensionless` 且 `weight_units == "1"`；含 `flux^-2`/`ivar` 词即拒 | 1 | `FZ-UNIT-PSFSW`/`FZ-FIELD-PSFSW-UNIT` |
| PSFSW-G03 | `group_normalized` 为真、`normalization.scope=="group"`、`median_target==1.0` | 1 | `FZ-FIELD-PSFSW-UNIT` |
| PSFSW-G04 | 四分量 `measurement_id` 互异（防塌陷为三） | 1 | `FZ-FIELD-PSFSW-4COMP` |
| PSFSW-G05 | 产物任何层不得含禁止键 `{ivar,variance,var,sigma,sigma2,inverse_variance,fisher,information,w_info,w_psf}` | 1 | `FZ-GATE-PSFSW-COV` |
| PSFSW-G06 | `covariance.method=="propagated_from_composite_coefficients"`、`variance_from_weight==false`、`uses_relative_weight_as_ivar==false`、`combination_coefficients` 非空 | 1 | `FZ-GATE-PSFSW-COV`/`SC-ADJ-P203.3` |
| PSFSW-G07 | `effective_psf_id` 非空且 effective PSF 描述完整（非仅 FWHM） | 1 | `FZ-GATE-PSFSW-EPSF` |
| PSFSW-G08 | `common_star_set_id` + `selection_function_id` 存在 | 1 | `SC-ADJ-P203.6` |
| PSFSW-G09 | `independence_proof ∈ {external_reference_catalog, reference_stack_single_threshold}` | 1 | `SC-ADJ-P203.6` |
| PSFSW-G10 | `n_common ≥ 3`，否则 `valid=false`+`insufficient_valid_stars` | 1 | `SCI-P2-001` R5 |
| PSFSW-G11 | 深度稳定性：`max_rel_dev ≤ 0.05`、`abs(rho_s) ≤ 0.8`、`K ≥ 5`、跨度 ≥ 1 mag | 1 | `SCI-PSFW-001` §7.3 |
| PSFSW-G12 | `valid=false` → `weight_value` 为 `null`、`reason` 在白名单；无 median SNR 回退 | 1 | `FZ-GATE-PSFSW-FAILCLOSED` |
| PSFSW-G13 | `valid=true` → 不得带失败 `reason` | 1 | 可审计性 |
| PSFSW-G14 | 版本字段（`composite_version`/指数/`C_norm`/估计器/截断/归一）非空且指数合法 | 1 | `SCI-PSFW-001` §3 |
| PSFSW-G15 | 组内 `median(W_psfsw)==1`（1e-9）且全部 `W_psfsw>0` | 1 | `FZ-FORMULA-PSFSW-COMPOSITE` |
| PSFSW-G16 | `C_norm` 尺度不变：`W_psfsw` 在 `C_norm` 缩放前后逐位一致 | 1 | 组内中值归一 |
| PSFSW-G17 | 四分量摘要 `p05≤p50≤p95`、`valid_area_fraction∈[0,1]` | 1 | `FZ-FIELD-PSFSW-4COMP` |
| PSFSW-G18 | 分量 `(p95−p05)/p50 ≤ 0.30`、趋势 ≤ 0.10（LOW 档 0.20）否则拆 tile/unavailable | 1 | `FZ-DEGRADE-SCALAR` |
| PSFSW-G19 | 标量功率损失 ≤ 0.05、通量偏差 ≤ 0.01 | 1 | `FZ-DEGRADE-SCALAR` |
| PSFSW-G20 | 生产模式列表不含 `psf_snr_power` | 1 | `C-004.1`/`FZ-MODE-DEFERRED` |
| PSFSW-G21 | 生产面不含 legacy 整数模式 `0/1/2`；`0` 尤其禁止 | 1 | `SC-ADJ-S1`/`FZ-FIELD-WEIGHTMODE` |
| PSFSW-G22 | 诊断别名不得出现在 `weight.sources`/`weight_value`/`covariance.variance_from` | 1 | `FZ-GATE-MEDIAN-SNR`/`FZ-GATE-SUPPORT-COVERAGE` |
| PSFSW-G23 | `component_flux_unit` 已声明且组内一致，`Conc` 单位为 `flux/px²` | 1 | 单位可审计 |
| PSFSW-G24 | `calibration_sample_id != acceptance_sample_id` | 1 | `SCI-PSFW-001` §3 |
| PSFSW-G25 | 基线声明只能是 `better_than`/`non_inferior_to` 且带 bootstrap CI | 1 | `SCI-PSFW-001` §4 表/§8 |

## 2. fail-closed 原因白名单（不得增删）

```text
no_common_star_set | background_nonpositive_undefined_transform | insufficient_valid_stars
| selection_bias_gate_failed | spatial_nonuniformity_gate_failed
```
（`FZ-GATE-PSFSW-FAILCLOSED`；`PSFSW-G12`。触发映射见主规格 §8.2。）

## 3. 负向 mutation 册（注入即红，rc=1）

| # | 注入 | 期望命中门 |
|---|---|---|
| m01 | `weight_units` 改成 `flux^-2` | G02 |
| m02 | `weight_kind` 改成 `ivar` | G02 |
| m03 | `concentration` 直接复制 `signal`（四分量塌陷） | G04 |
| m04 | 产物注入 `ivar` 键 | G05 |
| m05 | `covariance.method` 改成 `variance_from_weight`，`variance_from_weight=true` | G06 |
| m06 | `normalization.scope` 改成 `global` | G03 |
| m07 | `composite_version`/指数/`C_norm` 置空 | G14 |
| m08 | 删除共同星集与 selection function | G08 |
| m09 | `valid=false` 但 `weight_value` 仍写值（median SNR 回退） | G12 |
| m10 | `valid=true` 同时带 `reason` | G13 |
| m11 | 删除 effective PSF（`effective_psf_id=""`） | G07 |
| m12 | `n_common=2` | G10 |
| m13 | 深度稳定性 `max_rel_dev=0.20` | G11 |
| m14 | `independence_proof=per_frame_threshold_intersection` | G09 |
| m15 | 分量非均匀 `(p95−p05)/p50=0.45` | G18 |
| m16 | 生产模式列表加入 `psf_snr_power` | G20 |
| m17 | `weight_mode=0`（legacy support×snr²） | G21 |
| m18 | `covariance.variance_from="psfsw_robust_weight"` | G06 |
| m19 | `group_normalized=false` | G03 |
| m20 | `concentration` 用 `fwhm` 代替（`component_source="fwhm"`） | G04/G22 |
| m21 | 组内中值归一被去掉（`W_psfsw=Wt`，去掉 median 归一） | G15 |
| m22 | `C_norm` 缩放后 `W_psfsw` 改变（伪实现） | G16 |
| m23 | `calibration_sample_id == acceptance_sample_id` | G24 |
| m24 | 基线声明 `fisher_optimal` | G25 |
| m25 | 逐帧检测交集构造共同星集（样本派生） | G09 + Oracle V8 负向控制 |

### 3.1 机器 mutation ID 映射（与 `run/v6/alg-p2-psfsw/tools/run_gate_and_mutations.py` 逐字一致）

```text
m01_units_inverse_flux          m02_weight_kind_ivar            m03_concentration_collapse
m04_inject_ivar_key             m05_covariance_from_weight      m06_normalization_global
m07_version_empty               m08_no_common_star_set          m09_invalid_with_weight_value
m10_valid_with_reason           m11_no_effective_psf            m12_n_common_below_min
m13_depth_fail                  m14_independence_per_frame      m15_nonuniformity_wide
m16_psf_snr_power_in_production m17_legacy_weight_mode_0        m18_variance_from_weight_token
m19_group_not_normalized        m20_concentration_is_fwhm       m21_normalization_removed
m22_cnorm_breaks_invariance     m23_calibration_eq_acceptance   m24_fisher_optimal_claim
m25_sample_derived_stars        m26_unjustified_fail_closed
```

## 4. 正向控制

至少 5 条合法 psfsw 记录必须 ACCEPT（rc=0）：`PC1_nominal`、`PC2_low_n_tier`（LOW 档合法）、`PC3_preferred_n`、`PC4_high_C_norm_invariant`（`C_norm=1e9`，W 不变）、`PC5_unavailable_insufficient_stars`（合法 fail-closed 产物：`valid=false`+`weight_value=null`+白名单原因且由记录自身诊断自洽）。本门作用域为 `psfsw` 记录；`equal`/`pixel_ivar` 的模式合法门归 `SCI-P2-001` 权重来源门。缺正向控制 = 空门，不得 PASS。

## 5. 独立 Oracle 验证门 V1..V13

| ID | 内容 | rc |
|---|---|---|
| V1 | spec json 结构完整（阈值/门/白名单/映射齐备） | 0 |
| V2 | 复合式正性与四分量单调方向（冻结指数） | 0 |
| V3 | 组内 median=1 + `C_norm` 不变性 | 0 |
| V4 | conventional coadd 系数和=1 且复现常量场 | 0 |
| V5 | `C_out=R C_in Rᵀ` vs Monte Carlo（rel<3%） | 0 |
| V6 | 非法方差代理（`1/ΣW`、`ΣW`）与真实系数方差可区分 | 0 |
| V7 | effective PSF 算子 = 解析 conventional `P_eff`；`FWHM(P_eff) != median(FWHM_k)` | 0 |
| V8 | 参考来源共同星集过深度门；逐帧检测变体不过（负向控制） | 0 |
| V9 | `n_common` 分档（2 fail / 3 low / 10 standard） | 0 |
| V10 | 空间非均匀门（常量过 / 40% 散度红） | 0 |
| V11 | 门 + mutation（正向 ACCEPT + ≥24 负向 REJECT rc=1） | 0 |
| V12 | tracked 文档 claim 审计（注入删除 → rc=1） | 0 |
| V13 | 越界写审计（tracked 仅 write_scope；run 仅本任务 run 目录） | 0 |

`V11`/`V12` 的“注入即红”是**先证伪门、再声明门有效**（不是同实现自证）；`V8` 的负向控制直接复现 W1 复核 Oracle K8 的深度偏差方向。