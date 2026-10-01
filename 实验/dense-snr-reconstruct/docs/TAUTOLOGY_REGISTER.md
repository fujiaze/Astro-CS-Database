# dense-snr-reconstruct · 恒真门登记（存根）

本单元的恒真门登记已并入**全实验域**唯一登记表：

→ **`实验/TAUTOLOGY_REGISTER.md` §2.1（dense-snr-reconstruct）**

原文件曾只登记本单元，且存在三处计数不一致（标题「24 处」、正文小计「22 处」、表编到 24）
与若干漏报。合并版补登 5 条（`sim:388` 硬编码 `all_pass=True`、`exp_P4R2_06:100`、
`fix01:227-228`、`exp04:143`、`exp_p4_04:803`）与 1 处 fail-open（`exp_p4_04:674`），
并订正了 `exp04_idw_parameters.py:106` 的归类偏严问题。

本单元的判别力实测结论仍然成立，摘要如下（详见登记表）：

- `E_eff(1/v, v) ≡ 0`、`E_eff(c·w, v) = E_eff(w, v)`、`E_eff ≥ 0` 三条恒等式成立，
  凡形如 `E(1/v_true)`、`E(c·w, v)`、`E_dense ≥ E_frame` 的门在数学上不可能判红；
- 10 个注入缺陷（MAD ×1.35/×0.02、nodes +40/×7/符号翻转、spline ×1000/+1e9/常数填充）
  对本表所列恒真门 **10/10 全绿**，而同文件中真正有判别力的门
  `G2_dense_beats_frame` **7/10 判红** ⇒ 问题不在实现，在门的代数构造；
- 最严重一条仍是 `sim/exp_sim01_m16_forward_snr_truth.py:337` `NC-A1_all_calibers_zero`：
  该臂从不调用 `calibers()`/`patch_mad_var`/`cell_nodes`/`spline2d`，
  `:331-332` 算出**一个**数 `z`，`:334` 把**同一个 `z`** 赋给四个字段 ⇒ 「四口径对照」是一个数与自己比。

处置原则见 `实验/TAUTOLOGY_REGISTER.md` §0；四条硬纪律见 §0 末。
**不得保留「恒真但算作证据」的中间态。**