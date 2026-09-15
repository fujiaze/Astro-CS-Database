# 06 — PSFSW 四分量 / 复合 / 归一 / fail-closed schema

上位锚：`FZ-FIELD-PSFSW-4COMP`、`FZ-FIELD-PSFSW-UNIT`、`FZ-FORMULA-PSFSW-COMPOSITE`、`FZ-GATE-PSFSW-FAILCLOSED`、`FZ-GATE-PSFSW-COV`、`FZ-GATE-PSFSW-EPSF`；
PSF_SIGNAL_WEIGHT §3/§5/§6/§8；PSFW_FREEZE §4.2/§4.3/§5.1/§9；ADJ-P2-03；`RULINGS.md` #3/#4/#5。
机器：`contracts/proposals/v6/data/astrocs.v6.psfsw.v1.schema.json`（`psfsw.v1`）；正例 `examples/psfsw.example.json`。

> 定位：psfsw_robust 是**正式可选 conventional integration 权重**，与 Q/W、surface GLS 并列，**不是 QA-only**；
> 但它无量纲、组内相对，**不得**写成 ivar/Fisher information/W_info。

## 1. 四分量（分别落产品，measurement_id 互异）

```text
Wt_k       = C_norm * S_k^alpha * Conc_k^beta / (N_k^gamma * B_k^delta)   (alpha,beta,gamma,delta >= 0, 版本化)
W_psfsw,k  = Wt_k / median_j(Wt_j)                                      (组内 median = 1, 无量纲相对)
alpha_k(p) = W_psfsw,k * v_k(p) / Sum_j W_psfsw,j * v_j(p)             (v = validity 门)
```

| 分量 | 规范名 | 量纲 | 定义 | 备注 |
|---|---|---|---|---|
| signal | `psfsw.signal` | 与帧 signal/flux 同单位 | `S_k = Sum_{s in S_k} f_hat_{k,s}` | PSF 拟合总 flux 之和；**不是**帧总信号、不是孔径和 |
| concentration | `psfsw.concentration` | signal/像素 | `Conc_k = mean_{s in S_k} f_hat_{k,s} / A_NEA,k`（或文档等价 mean PSF flux 固定约定） | **禁止**用 FWHM 或拟合残差代替 |
| noise | `psfsw.noise` | 与 signal 同单位 | `N_k` 稳健噪声（MAD/Sn 类，须声明估计器与版本） | **不是**像素 σ、不是 m5 |
| background | `psfsw.background` | 与 signal 同单位 | `B_k` 稳健均值背景（须 > 0） | B ≤ 0 且变换未定义 → unavailable |

每分量：`measurement_id` 互异 + `estimator{id,version}` + `spatial_summary{p05,p50,p95,valid_area_fraction}`；
四分量塌陷为三、缺分量、p05>p50>p95、`valid_area_fraction ∉ [0,1]` → REJECT（`FZ-FIELD-PSFSW-4COMP`）。

## 2. 组内归一与无量纲（`FZ-FIELD-PSFSW-UNIT`）

| 字段 | 冻结值 | 越界处置 |
|---|---|---|
| `weight.kind` | `psfsw_robust_weight`（别名 `relative_dimensionless`） | 写成 ivar/fisher/information → REJECT |
| `weight.units` | `1` | 含 `flux^-2`/`ivar`/`1/flux^2` → REJECT |
| `weight.group_normalized` | `true` | false → REJECT |
| `weight.normalization.scope` | `group` | global 且无 selection function → REJECT |
| `weight.normalization.median_target` | `1.0` | 组内 median ≠ 1 → REJECT |
| `weight.normalization.constants_version` | 非空版本串 | 空 → REJECT |

`C_norm`、指数 `(alpha,beta,gamma,delta)`、截断、稳健估计器、归一常数**版本字符串必须落产品**；
标定该版本的样本**不得**与最终验收样本相同（PSF_SIGNAL_WEIGHT §3）。

## 3. 共同星集与 selection function（`FZ-GATE-PSFSW-FAILCLOSED`）

| 字段 | 要求 |
|---|---|
| `common_star_set_id` / `selection_function_id` | 必填非空 |
| `members_hash` | 必填（可审计，RULINGS #4） |
| `construction` | `external_catalog` 或 `reference_stack_threshold`；**禁止**逐帧检测阈值取交 |
| `n_common` | ≥ 3 |
| `exclusion_flags` | 显式列举 `saturated/blended/trailed/moving/psf_mismatch/edge_truncated` |

unavailable 原因白名单（**唯一合法集合**）：
`no_common_star_set`、`background_nonpositive_undefined_transform`、`insufficient_valid_stars`、`selection_bias_gate_failed`、`spatial_nonuniformity_gate_failed`。
`validity.valid=false` 时 `weight.weight_value` **必须为 null**；**禁止**回退成 median source SNR。

## 4. covariance 与 effective PSF 边界

- `covariance_ref` 指向的 covariance 记录必须 `method=propagated_from_composite_coefficients`、`variance_from_weight=false`、`uses_relative_weight_as_ivar=false`；
- `effective_psf_ref` 非空；缺 → REJECT（`FZ-GATE-PSFSW-EPSF`）；
- 禁止 `Var = 1/W_psfsw`、`Var = Sum_k W_psfsw,k`、把任何诊断量当方差面。

## 5. psfsw 产物禁止键（任何层，`FZ-GATE-PSFSW-COV` + PSFW_FREEZE §9.3）

`ivar`、`variance`、`var`、`sigma`、`sigma2`、`inverse_variance`、`fisher`、`fisher_information`、`information`、`w_info`、`w_psf`。
扩展守卫（PSFW_FREEZE §9.3 建议，本提案登记为 `x-astrocs-extended-guard-keys`）：`snr`、`snr2`、`support`、`coverage`。

> 门的作用域是 **psfsw 产物子树**：同一 Phase2 马赛克包可以并存独立的 `point_information` 记录，
> 但 psfsw 记录内不得出现上述键。

## 6. 基线比较（强制）

`baseline_comparison` 必须相对 `equal`/`exposure`/`pixel_ivar`/`W_info` 报告 detection power、FWHM、通量偏差、面亮度偏差；
只能声明「在指定验收数据上优于指定基线」，**不得**声明 Fisher 最优。不得只以「看起来更好」通过（PSF_SIGNAL_WEIGHT §7.5）。

## 7. fail-closed 与负向门

- 四分量塌陷/缺分量/measurement_id 重复 → REJECT；
- units=ivar 或 flux^-2 → REJECT；
- group_normalized=false / scope=global / median≠1 → REJECT；
- `variance_from=psfsw_robust_weight` 或 `Var=1/W_psfsw` → REJECT；
- 缺共同星集/selection function/n_common<3/逐帧检测取交 → unavailable；
- invalid 时写回 median SNR → REJECT；
- 缺 effective PSF → REJECT；
- 常数/指数/估计器未版本化 → REJECT。
