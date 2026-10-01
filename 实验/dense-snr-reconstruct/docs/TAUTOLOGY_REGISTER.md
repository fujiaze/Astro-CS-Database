# dense-snr-reconstruct · 结构性恒真门登记（永不能判红的判据）

本文件登记本单元内**按代数构造即不可能判红**的门。列入本表**不等于**删除：这些门仍出现在
多份报告的判别力叙述里，删除属跨篇改动，须由前台裁决后统一执行。在裁决之前，它们**不得**
被引用为「判据能红」的证据。

## 判定依据

度量 `E_eff(w, v) = Σ(w²v)/Σ(w)² · Σ(1/v) − 1` 有三条恒等式，三条都在 /tmp 内用随机数据
数值验证过（仓内未改动）：

| 恒等式 | 断言 | 实测上界 |
|---|---|---|
| `E_eff(1/v, v) ≡ 0` | 对任意正数组 `v` 成立 | 2000 例随机 `v`，`max|E| = 4.44e-16`（机器 eps） |
| `E_eff(c·w, v) = E_eff(w, v)` | `E_eff` 对 `w` 按构造齐次 | 3000 例，`worst rel_dev = 1.8e-15`，门容差 1e-8 |
| `E_eff ≥ 0` | Cauchy–Schwarz，`v > 0` 恒成立 | 50000 例对抗抽样，`min = +5.8e-7`，从未为负 |

⇒ 形如 `E(1/v_true)`、`E(c·w, v)`、`E_dense ≥ E_frame`（`E_frame ≡ 0`）的门在数学上不可能判红。

## 判别力实测（10 个注入缺陷）

MAD ×1.35 / ×0.02、nodes +40 / ×7 / 符号翻转、spline ×1000 / +1e9 / 常数填充：

- 本表所列恒真门：**10/10 全绿**（对注入结构性免疫）
- 同文件中真正有判别力的门：`G2_dense_beats_frame` **7/10 判红**，`G3_flat_spline_no_undershoot`
  在符号翻转下判红

这证明问题不在实现，而在**门的代数构造**。

## 清单（22 处）

### `code/calibers/`（7）

| # | file:line | 门 | 型 |
|---|---|---|---|
| 1 | `exp_P4CAL_01_three_calibers.py:160` | `G1_oracle_dense_zero`（操作数建于 `:141` `E_eff(1.0/vf, vf)`） | 代数恒等式 |
| 2 | `exp_P4CAL_01_three_calibers.py:191-192` | `G3_flat_no_fake_advantage`（两个合取项分别强制恒等式 (1) 与 Cauchy–Schwarz (4)） | 代数恒等式 |
| 3 | `exp_P4CAL_02_three_calibers_guarded.py:158` | `G1_oracle_dense_zero`（结构臂，操作数 `:139`） | 代数恒等式 |
| 4 | `exp_P4CAL_02_three_calibers_guarded.py:165` | `G1_oracle_dense_zero`（平坦臂） | 代数恒等式 |
| 5 | `exp_P4CAL_02_three_calibers_guarded.py:173` | `G1_oracle_dense_zero`（紧支撑臂） | 代数恒等式 |
| 6 | `exp_P4CAL_02_three_calibers_guarded.py:166` | `G3_flat_frame_zero`（`w_frame` 为 `np.full` 常量 `:124` × 平坦真值 ⇒ 精确 0） | 代数恒等式 |
| 7 | `exp_P4CAL_02_three_calibers_guarded.py:167` | `G3_flat_dense_no_fake_advantage`（右端 ≡ 0，左端由 Cauchy–Schwarz ≥ 0） | 代数恒等式 |

以上 7 处均对 `spline2d` / `cell_nodes` / `patch_mad_var` / `blocky_px` 的任何注入免疫——
这些符号在门两侧都不出现。

### `code/sim/`（7）

| # | file:line | 门 | 型 |
|---|---|---|---|
| 8 | `exp_sim01_m16_forward_snr_truth.py:303` | `G1_oracle_dense_zero`（操作数 `:222`） | 代数恒等式 |
| 9 | `exp_sim01_m16_forward_snr_truth.py:317` | `G1_oracle_dense_zero_outside` | 代数恒等式 |
| 10 | `exp_sim01_m16_forward_snr_truth.py:310-312` | `R2_metric_scale_invariant`（比 `:221` 的 `E_eff_dense` 与 `:225` 的 `E_eff(3.17·w_dense, v_true)`——就是齐次恒等式本身；`:308-309` 的注释已自承） | 往返自证 |
| 11 | `exp_sim01_m16_forward_snr_truth.py:337` | **`NC-A1_all_calibers_zero`** —— 最严重，见下 | 代数恒等式 + 结构对称 |
| 12 | `exp_sim01_m16_forward_snr_truth.py:338` | `NC-A1_oracle_zero` | 代数恒等式 |
| 13 | `exp_sim01_m16_forward_snr_truth.py:350` | `NC-A2_oracle_zero` | 代数恒等式 |
| 14 | `exp_sim01_m16_forward_snr_truth.py:351` | `NC-A2_frame_zero`（`v2` 常量 `:345` × `w_frame` 常量） | 代数恒等式 |

### 本单元其余（8）

| # | file:line | 门 | 型 |
|---|---|---|---|
| 15 | `route1/exp_p4_01_weight_optimality.py:88` | `identity_machine_precision`（`lhs = snr²/Fref²`、`rhs = 1/sig²`，而 `snr = Fref/sig` ⇒ Fref 抵消） | 代数恒等式 |
| 16 | `route2/exp_P4R2_01_weight_identity_gamma.py:93` | `part_A_identity.verdict`（同恒等式，两处 `:40-43` / `:47-48`） | 代数恒等式 |
| 17 | `route2/exp_P4R2_06_criterion_arms.py:87` | `arm_no_source.verdict`（`:58-60` `rel_diff_arms = max|σ_slow2 + 0/G − σ_slow2|`，即 `0.0 == 0.0`） | 结构对称 |
| 18 | `route2/exp_P4R2_07_luminance_chain.py:126` | `assert np.all(ctrl_snr0 == 0.0)`（0 × (1+ε) ≡ 0） | 代数恒等式 |
| 19 | `route2/exp_P4R2_08_mad_sigma_budget.py:109` | `negative_control_constant_data.verdict`（`mad_const` 是常量数组的 MAD） | 代数恒等式 |
| 20 | `route3/exp01_weight_identity_gamma_optimality.py:37` | `H1a_identity.pass` | 代数恒等式 |
| 21 | `route3/exp01_weight_identity_gamma_optimality.py:80` | `H1c_negative_control.pass`（等 σ ⇒ 任何常量权重向量都给出方差 4/n，展度 ≡ 0） | 代数恒等式 |
| 22 | `route3/exp01_weight_identity_gamma_optimality.py:106-110` | `H1d_fref_propagation.pass`（「期望」比值由产生被测值的同一个常量 0.4 与同一个 `dm` 重算） | 往返自证 |
| 23 | `route3/exp01_weight_identity_gamma_optimality.py:120` | `downstream_interface.pass`（`1/sig²` vs `(f/sig)²/f²`） | 代数恒等式 |
| 24 | `route3/exp04_idw_parameters.py:106` | `S2_p_inf_nearest_limit`（参照 `near`（`:89-94` 手写 argmin）就是被测 IDW 的 p→∞ 极限；实测 `max|idw(1000) − nearest| = 0.000e+00`，p=30 给 2.22e-16） | 代数恒等式 |

> 计数口径：按**门定义处**计 24 个实例（其中 `S2_p_inf_nearest_limit` 的恒等式合取项另计）。
> 第二轮复核给出的是「7 处」（仅 `calibers/`），实测 `calibers/` + `sim/` 单独就有 **14 处**。

## 部分恒真（**不计入**上表，但需知悉）

这些门的合取里同时含真判据，整体仍能判红，只是其中一支是恒等式：

- `fix/fix01_metric_E_and_gates.py:87,:90` —— 同为齐次恒等式，但同一条 `a_ok` 合取内另有真子句
- `route3/exp02_metric_E_properties.py:84,:90` —— 第一合取恒等式，第二合取 `E_flat_wrong > 0.01` 为真判据
- `route3/exp06_white_noise_and_purity.py:113,:147` —— `bitwise_equal` / `snr_zero` 为 IEEE-754 加零精确性
- `route3/exp03:246`、`route3/exp04:143` —— 任何常量重建都给 E ≡ 0，只能抓「注入虚假结构」的缺陷
- `route1/exp_p4_03:168` `kappa_theory_holds` —— 比的是构造点云时用的同一个 `ax_ratio`（`:120`），但留了 5% 有限样本带

## 最严重的一条

`sim/exp_sim01_m16_forward_snr_truth.py:337` `NC-A1_all_calibers_zero`：

该臂**从不调用** `calibers()` / `patch_mad_var` / `cell_nodes` / `spline2d`。
`:331-332` 算出**一个**数 `z = E_eff(1.0/v_flat, v_flat)`，`:334` 把**同一个变量 `z`**
赋给 `E_eff_frame` / `E_eff_cell` / `E_eff_dense` / `E_eff_shuffled` 四个字段。
所谓「四口径对照」是**一个数与自己比**，估计器在该门下不可达。

## 处置方向（待前台裁决）

两条路，择一：

1. **移出判决、登记为诊断项** —— 数值保留在 `results/` 与报告里，但不再计入 `gates`、
   不再被引作判别力证据；
2. **改接真实参照量** —— 对 `calibers()` / `spline2d()` / `patch_mad_var()` 的**输出**做
   独立重算，而不是让门去比 `E_eff` 的恒等式。

两条路都**不得**靠放宽阈值、加 epsilon 或删读数来"解决"。
