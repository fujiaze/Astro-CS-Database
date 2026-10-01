# 实验域 · 结构性恒真门登记（永不能判红的判据）

本文件是**全实验域**（`实验/*` 五个单元）的恒真门唯一登记表，覆盖
absolute-snr、additive-sky-seamless、dense-snr-reconstruct、photometric-magnitude、
healpix-polar、m42-realdata、engineering-evidence、shared。
单元内的旧登记表 `dense-snr-reconstruct/docs/TAUTOLOGY_REGISTER.md` 已改为指向本文件的存根。

## 0 处置原则（唯一）

依据 `run/GOVERN-08/工作包-GOVERN-08原件/standards/08_编译与CI重做规范.md` §4：
「判据可信：正确实现绿、注入对应缺陷红，正负例随附；**恒真、空断言、不读真实对象的判据无效**。」

⇒ **恒真门本身不是罪，把它当证据用才是。** 四类处置：

| 类 | 含义 | 处置 |
|---|---|---|
| **A** | 已诚实隔离：自带标注、已移出门计数、不承担证据位 | **保持不动**。只需检查是否仍被报告引作判别力证据 |
| **B** | 被计入 `gates`/`pass` 汇总或被报告当判别力证据，实则永不判红 | ①接真实参照量改造成真判据，或 ②移出判定、改列诊断项并**同步解除全部证据引用**。**不得保留「恒真但算作证据」的中间态** |
| **C** | 部分恒真合取：合取中某支恒真，整条失去部分判别力 | 拆出恒真那一支单独标注，或换掉该合取项 |
| **D** | 不读真实对象：比较的是本地桩/重实现，不是生产实现 | 见 §4「生产实现绑定」 |

**判别恒真 vs 恒红的双向体检**：除「能红吗」外还须查**恒红**——两个量逐位相同时 `判 <=tol`
的门永远红，而**恒红门会把真实缺陷永久藏在红灯里**。两类同样无效。

**三条硬纪律**：不得为了让门变绿而放宽阈值、加 epsilon、引入恒真路径或吞异常；
判红就如实报红；只改表述不改数据，真实读数一律保留（移出判定不等于删读数）。

## 1 三型分类

| 型 | 特征 | 本域计数（估） |
|---|---|---|
| **1 代数恒等式型** | 恒等式两边数学恒等，含用**逆函数**构造的「外部参照」（拿 `f(f(x))` 或 `1/v` 当参照） | ≈54 |
| **2 结构对称型** | 左右两支由同一次计算赋值（一个数与自己比）、或同一条代码路径产出两个待比较量 | ≈41 |
| **3 往返自证型** | 期望值由产生被测值的**同一来源**重算 | ≈2（本域实扫远多于 2，见下） |

⚠ 原始估算的「仅 2 条往返自证型」与实扫不符：实扫在 `shared`、`dense-snr`、
`m42-realdata`、`engineering-evidence` 四面各找到多条往返自证（如
`shared/synthetic/m16_scene.py:715` V1、`dense-snr/sim/exp_sim01:310-312`、
`dense-snr/route3/exp01:106-110` H1d、`m42-realdata/code/c1_photometry.py:267`、
`engineering-evidence/.../qa_oracle.py:119-132` 等）。以实扫为准。

## 2 单元登记

### 2.1 dense-snr-reconstruct（29 条）

原登记表 24 条经复核：21 条完全正确、3 条需改型别，另有 5 条漏报。

**已修/已标注（类 A）**：`exp04_idw_parameters.py:106` `S2_p_inf_nearest_limit` 经复核
是**双实现交叉核对**（`idw()` 用 `exp(p·log(d/dmin))`+`argpartition`，
参照 `near`（`:89-94`）是独立手写 `argmin` 路径，两者不共享代码），实测逐位一致
**支持**它有判别力，归类偏严，建议移出「永不判红」主表。
`route2/exp_P4R2_06_criterion_arms.py:48/:52` 中 `src_true` 与 `S_src_hat` 是同一次
moffat4 调用的逐字相同值，`:49` 的 `frame` 算后从未使用 ⇒ 该文件整体另属 **类 D**。

**补登（登记表漏报）**：

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 25 | `sim/exp_sim01_m16_forward_snr_truth.py:388` | `NC3["all_pass"] = True` | 2 | **B + fail-open** | **硬编码字面量 True**；`:378` 的 `"gates": {}` 是空字典，`:377` 有 `informative_only` 标注（部分诚实），但仍经 `:477` 进 `gates` 列表、`:478` 计入 `verdict` |
| 26 | `route2/exp_P4R2_06_criterion_arms.py:100` | `contrast_arm_with_source.verdict` | 1 | **B** | 判据 `src_metric_core > 0.05`，实测 37.4（`:47 A_peak=1416.6` 是反解凑出来的）⇒ 阈值比读数小 3 个量级，对任何正确实现恒真 |
| 27 | `fix/fix01_metric_E_and_gates.py:227-228` | `rel_zero == 0.0`（`c2_ok` 第一合取项） | 1 | **B（部分）** | 零源场 ⇒ `src_hat0 ≡ 0` ⇒ IEEE-754 加 +0 精确 ⇒ `rel_zero` 精确 0。**这条 `C2_zero_source_per_arm_estimator` 门正是 `REPORT_experiment.md:21`/`REPORT_paper.md:264` 用来替代结构恒真门的「重建版判据」，替代品自身也有一半恒真** |
| 28 | `route3/exp04_idw_parameters.py:143` | `NC_flat_zeroing.pass` | 1 | **B** | 常量真值 ⇒ `w ≡ 1/4`、`var_w = var_opt` ⇒ `E ≡ 0`、`rd ≡ 0`；经 `:145-147 all_pass` 计入。原登记表误放「部分恒真」节，**低估了自身严重性** |
| 29 | `route1/exp_p4_04_brightness_forward.py:803` | `reference_is_rewrite_proof` | 2 | **B** | `:605` 把 `:558` 的字面量与它自己比 ⇒ 恒 True。注释 `:600-602` 宣称「参照量重新指向可写文件时该门立刻判红」**是错的**（不动 `:558` 就不会红）。真能判红的 `ref_path_overlaps_out`（`:603`）被算出却未接门 |
| — | `route1/exp_p4_04_brightness_forward.py:674→682→799` | `gain_injection_is_detected_by_this_suite` | — | **fail-open** | `inv_devs = inv_devs or [float("inf")]` ⇒ 无可比读数时塞 `inf` ⇒ `max_inv > tol` ⇒ 门报告「增益注入**已被检出****。同一文件 `:657-660` 注释宣称已消除这个失败模式，实为把它挪到了「无数据」分支 |

**归档脱节**：`results/route1/exp_p4_04_brightness_forward.json → gates` 只有 **7** 门，
现行代码 `:795-803` 有 **14** 门 ⇒ **8 条门没有任何实测记录**。

### 2.2 photometric-magnitude（P1）

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 1 | `step9_collect.py:161` | `g2_c` | 1 | **B** | `sigma_residual_delta ≡ 0.0` **精确**：`calibrate:136` `r = log10(f_instr/f_syn)` ⇒ `r₁ ≡ r₀ + log10(shift)` 逐元素；`mad_sigma` 对平移严格不变、inlier 掩码逐元素相同。**已知隔离点 `step9_collect.py:148-163` 只隔离了 (a)(b)，(c) 漏网仍进判定** ⇒ G2 现只剩 `g2_d` 一个真子句 |
| 2 | `step4_guided_vs_blind.py:63` | `false_alarms=0` | 2 | **B** | **字面量**；消费方 `step9_collect.py:247` `g4_fa` ⇒ 永真。报告引用 `results/GATES.md:11`、`README.md:264` |
| 3 | `step5_calibration_gate.py:225` | `cross_frame_gate_present=False` | 2 | **B** | **字面量**；消费方 `:189` `g3_nogate` ⇒ 永真。报告引用 `results/GATES.md:10`、`README.md:251` |
| 4 | `step6_apply_and_units.py:61-62` | `I_photo_indep` vs `I_photo` | 2 | **C** | 注释称「不复用 `apply_photometry` 的实现」，但 `scia_pipeline.py:78-80` 的实现就是 `k_photo*m_map*img` ⇒ `rel ≡ 0.0` 精确。消费方 `:288` `g5_recompute` |
| 5 | `step9_collect.py:293` | `g5_fc` | — | **D** | 验证的是 `step6:85-90` 的 **4 行本地桩** `downstream()` 自己抛的 `RuntimeError`，`True` 分支从未执行 |
| 6 | `step9_collect.py:108` | `g1b_fold` | 1 | **C** | `sigma_flat_independent = PHOTON_MAG*s/sqrt(N_eff)`，右端就是 `s`，`N_eff = 1/ΣP² > 1` ⇒ 比值恒 < 1，**对任何归一化 PSF 恒真** |
| 7 | `step9_collect.py:109` | `g1b_quad` | 1 | **C** | `sigma_ceiling = ρ_hi·sqrt(…+σ_sys²…)`，`σ_sys` 是被 sqrt 内的真子集且 `ρ_hi>1` ⇒ 恒真 |
| 8 | `step7_negatives.py:104-105` | `N0` | 1 | **B** | `F_true = f_syn*inject_scale` ⇒ 残差逐元素全等 ⇒ `sigma_obs == 0.0`；`b0` 是**硬编码常量**提供非零 floor ⇒ 永真。有 `provides_injection_response=False` 标注但仍计入 `n_pass` |
| 9 | `step7_negatives.py:303-304` | `N5` | 2 | **B + D** | 门拿 `gate_verdict` 的定义核对 `gate_verdict` 自己（`ρ_lo<=0` 时必然返回 `LOWER_BOUND_UNDEFINED`） |
| 10 | `step7_negatives.py:247` | `N3` | 3 | **B + D** | 残差场是**合成**的，未走 `measure()→guided_photometry→fit_psf→sample_selection` 任何一步；σ_obs 与「漏掉的那项」由同一个 `amp_col` 标量驱动 |
| 11 | `step1_analytic.py:98/:100/:106-110/:141-144` | `rtol`/`k_rel_err`/N0/`oracle_S0_degenerate` | 1 | **B** | `analytic_frame(..., noise=False)` 直接返回 `k*mi*f_syn` ⇒ 三处「Oracle 恢复」全是代数恒等式。**`results/GATES.md:14` 把「Oracle 相对误差 3.33e-15」当作 G7 三类互证之一** |
| 12 | `step9_collect.py:232` | `g4_rob` | — | **恒红** | `fit_psf:384-385` 把位置硬约束在 `x0±2.0`，而 `far` 取偏移 ≥3 px 的行 ⇒ 匹配必然失败 ⇒ **对任何保留 ±2 px 搜索框的实现恒红**。**代码已诚实处理**（`:207-225`、`:276-279` 写明「量的是搜索框不是物理鲁棒性」并如实判红），但 **`results/GATES.md:11` 与 `results/gates.json` 仍记 G4 = PASS ⇒ 归档过期** |

**结构性发现（类 D）**：`code/redo/` 全部 27 个文件**不 import 本单元任何生产模块**
（`grep "scia_common|scia_calib|scia_pipeline|scia_sim"` 零命中）。**同一个 IRLS 在仓内有 6 份
互不相同的拷贝**。最明显危害：`route3/exp_S05_gates_inlier.py:50-64` 的 `H5a` 用本地克隆
论证「**生产门② 对 n≥3 不可达**」——用本地克隆的证据去断言生产门的性质。
主链（`step1`–`step9`）同样全部是本地 numpy 复刻（`scia_sim.py:9`、`scia_common.py:634` 自述）。
⇒ **P1 单元的判据整体上在验证自己的复刻。**

**route3 的 6 条硬编码字面量 `pass: True`**：`exp_S05:94-99`（三字段全字面量 True）、
`exp_S09:73-77`（`zp_16 - zp_16` 自减 + `"pass": True`）、`exp_S10:70-75`（`"value": 0.0` +
`"pass": True`）、`exp_S12:64-69`（**字面量 `"negative_zero": True` 直接写进 `verdict`**）、
`exp_S11:68-75`、`exp_S03:81-87`（`"delta_location": 0.0` 字面量）。全部进 route3 `verdict` 汇总。
**隔离做得最干净的一处（可作模板）**：`exp_S09:72` `H9b.pass` 已登记 + 已移出汇总 +
`docs/DISPUTES.md:104-106` 写明「恒真门无证据资格」。

**PM 未引用 absolute-snr 的门作证据**：全仓 grep 确认，PM 引用 P2 的只有接口值传递
与一处**前置条件指针**。这一点无需处置。

### 2.3 m42-realdata

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 1 | `code/c2_absolute_snr.py:396-403` | `C2-G3c-weights-exploit-noise-variation` | 2 | **恒红 + B（本单已处置）** | `:367-368` 在分箱循环内取**全数组**中位数 `np.median(W)` ⇒ 24 箱 `n_unique=1`；`weight_efficiency` 对 w 0 次齐次 ⇒ 两臂**逐位相同** ⇒ `ratio_e ≡ 1.0`，门**永不判绿**。已标 `degenerate` 移出计数；报告与 `docs/CRITERIA.md` 的**主结果引用已撤回** |
| 2 | `code/c2_absolute_snr.py:427` | `C2-G3b-optimal-degenerate` | 1 | **B（本单已处置）** | `E(1/v, v) ≡ 0`（Cauchy–Schwarz 取等），实测 −2.22e-16。desc 自认「恒真、无证据资格」却仍计 `n_pass` ⇒ 已标 `degenerate` |
| 3 | `code/c1_photometry.py:405` | `C1-G3b-invariance-numeric` | 1 | **B + D** | `mad1` 与 `mad0` 数学同一量；`r` 是纯合成 `rng3.normal`，从不触 `star_matcher.cpp` |
| 4 | `code/c1_photometry.py:416/:356` | `C1-G3-invariance-degenerate`、`C1-DIAG-undecidable-{t2,t3}` | 字面量 | **B** | `ok` 参数**硬编码 `True`**；内容是诚实登记（不可判定），但 `level="honest-boundary"` 不影响 `Gates.summary()` 的 `n_pass` 计数 |
| 5 | `code/c1_photometry.py:267` | `C1-SPREAD-RECOMPUTE` | 3 | **B** | 读的 `k_photo` 与落盘的 `photscale_spread_dex` 出自**同一份** `p1_phot.json` ⇒ 只验 JSON 序列化；desc 写「**独立**复算」不成立 |
| 6 | `code/c3_seam_additive.py:67` | `C3-SELFTEST-seam-copy` | 3 | **B** | `SC.seam_steps` 是 `sci_c_common.seam_steps` 的逐字副本（`seam_criterion.py:18` 自述）⇒ 只能抓副本漂移 |
| 7 | `code/c3_seam_additive.py:302` | `C3-G1b-seam-block-consistency` | — | **fail-open** | `max(..., default=0.0)` 作用在**先按 `consistent` 筛出的子集**上 ⇒ 无一条边界一致（**正是真接缝被检出时的情形**）⇒ `max_cons = 0.0` ⇒ 空过判绿。**真出问题时门自动变绿** |

**类 D 的例外**：本单元读**真实落盘产品**（HiPS FITS + `p2_*.bin` + manifest），
只是不读 C++ 源码 ⇒ `c4_leaf_allocation.py`（14/14 全绿 + 5 条真负例）是本面最可信的文件。

### 2.4 shared（9 个 .py）

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 1 | `synthetic/noise_model.py:468/:470` | `T3`/`T4` | 1 | **B + D** | 测试自造 `ctrl = ref + delta`，`median|(a+c)−median(a+c)| ≡ median|a−median(a)|` 位精确；`T4` 是 `T3` 的闭式改写。**两者都没调** `NM.expose(mode=MODE_ADDITIVE)` |
| 2 | `synthetic/noise_model.py:496` | `T7` | 1 | **B + D** | 指数由 `dark_double_temp_c=6.0` 写死 ⇒ 比值恒 2 |
| 3 | `synthetic/noise_model.py:435` | `T1` | — | **D** | `x.var()/x.mean()` 只测 numpy 采样器，`expose()` 全程未调 |
| 4 | `synthetic/noise_model.py:482` | `T5` | — | **B** | 声称检出 `1/12` 量化项，但 B=30 时该项仅占预测方差 0.34%；删掉后 `rel_dev = −0.00340` 仍在 0.05 容差内 **15 倍余量 ⇒ 不红** |
| 5 | `synthetic/m16_scene.py:715` | `V1_ab_st_consistent` | 2 | **B + D** | `zp_ab` 与 `zp_ab_from_st` 同出一次调用、输入是写死字面量 ⇒ 差是与输入无关的常数 5.176e-05 |
| 6 | `synthetic/m16_scene.py:793-794` | `V3_mask_propagates` | 1 | **B + D（空断言）** | 测试自己执行了生产同一行 `np.where(valid, fr.adu, 0.0)`，再断言 `0 == 0` |
| 7 | `synthetic/m16_scene.py:810`、`m16_sampling.py:1307` | `V5`、`V2` | 1 | **B + D** | 「纯加性负例」模板复制两次：`(a+c)−(b+c) == (a−b)` |
| 8 | `synthetic/noise_selftest.py:658/:776` | `B1`/`B4` | 1 | **B + D** | `hi = lo + off` ⇒ `Var(hi1−hi2) ≡ Var(lo1−lo2)`；docstring 自认「逐位配对 ⇒ 精确 0」 |
| 9 | `synthetic/noise_selftest.py:275` | `A1` | 2 | **B + D** | 与 `:259` 是**同一 `var_resid`** 上的逐字相同表达式 ⇒ 同一读数**重复计票** |
| 10 | `synthetic/noise_selftest.py:947` | `C3.level_formula` | 2 | **B** | `det.saturation_adu` 属性体就是 `full_well_e/g + bias_adu`，与右边重抄同一表达式 |
| 11 | `synthetic/noise_selftest.py:534` | `additive_negative_control_ratio` | 空断言 | **B** | `(v1 + (mm−1)²·0.0)/v1` —— 乘 0 的项 ⇒ 字面就是 `v1/v1`，**只可能等于 1.0**，不是负例 |
| 12 | `synthetic/m16_mask.py:498-505` | `verdict_*` + `all_pass` | — | **D** | `selftest()` 从不调 `build_frame_mask`，把 SPIKE 判据本地重打一遍再断言 |
| 13 | `synthetic/m16_scene.py:463-466` | `max_abs_rel_dev: 0.0` | — | **fail-open** | `if not sel.any(): return {... "max_abs_rel_dev": 0.0}` ⇒ 无满足像素时报**完美残差** ⇒ 生产断言通过 ⇒ **缺数据 = 绿**（对比 `render_m16_frame:550-556` 本身是 fail-closed 的 `raise AssertionError`，被这个空选择旁路） |
| 14 | `data/synthetic/generate.py:88-90` + `synthetic/render.py:581-590,630` | — | — | **fail-open** | `n_ok` 算出后从不参与 rc；`render.py` 捕获 `FileNotFoundError` → `continue`，而 `main()` **恒返回 0** ⇒ **全部帧 UNAVAILABLE 仍 exit 0** |

**本面唯一真判别门**：`synthetic/noise_selftest.py:893`（`C2`）—— 数据来自同 seed 下两次
**生产**调用 `NM.expose(quantize=True/False)`，对照字面量 `QUANT_VAR=1/12`；
删 `np.round` ⇒ 红；`round` 挪到电子域 ⇒ `1/27` 红；换 `floor` ⇒ `−0.5` 红。
**弱点**：docstring `:867-868` 的注入对照只写在文字里未执行。

### 2.5 healpix-polar（33 个 .py）

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 1 | `code/audit/route2/exp01_leaf_area.py:69-70` | `abs(rel_dev_from_pi_over_3)<1e-4` | 2 | **B** | 被积量 `A` 来自 `p3lib.py:82-84` `jacobian_chart`，**该函数直接 `np.full_like(u, π/3)`，完全忽略 f,U,V**；基准是同一字面量 ⇒ 判据对 chart 实现**零判别力** |
| 2 | `route2/exp01_leaf_area.py:71/:72` | `metric_true==0.0`、`metric_wrong_candidate>0.2` | 1 | **B** | 度量的期望值就写在同文件 docstring 第 9 行；不是缺陷注入 |
| 3 | `route2/exp02_polar_pixel.py:76`、`route2/exp04_area_operators.py:72` | `full_sky_chord_closure`、`sum_all_leaves_over_4pi_minus_1` | 1 | **B** | VOS 立体角是球面 2-上同调（可加）⇒ 任何测地球面剖分的**有向**扇形和恒等于 4π，与单叶面积误差无关 |
| 4 | `route2/exp04_area_operators.py:86` | `lhuilier_with_normalized_vertex_rel_dev` | 3 | **B** | `:79` **先把被缩放的顶点重新归一化**再交给不归一化的 `lhuilier_triangle` ⇒ 输入与参照逐位相同 ⇒ 恒 0。docstring 仍称「free self-check works」——**负控在测量前被自身抵消** |
| 5 | `route2/exp03_flux_conservation.py:153-155` | `S_over_B0_*` | 1 | **B** | `x_j = B0·A_pixel`、`sumFlux += x_j·w`、`N_p += w·A_pixel` ⇒ `A_pixel` 精确约掉，对**任意** `w` 恒为 0；`predicted_1_over_pf2` 也出自同一 `PF` |
| 6 | `route2/exp10_chain_usecase.py:239/:240` | `idw.max_abs_dev_at_control_points`、`negative_control` | 2 / 1 | **B** | 查询点与控制点重合 ⇒ `d2<1e-30` 分支**把被测数组原样返回**；常量场的归一化加权平均恒等于该常量 |
| 7 | `route3/exp03_weight_conservation.py:283` | `injection_red` | 1 | **B** | 注入字面 `0.9003*a0` 后立刻算 `|0.9003−1|`；**与 `a0`、几何、生产全部无关**。实测 0.09970 而同文件记录的 `expected` 是 0.0996837（差 1.6e-5 > 1e-6）⇒ 若真去测 `2√2/π−1` 本门**必红** |
| 8 | `route3/exp03_weight_conservation.py:307/:456` | `mean_estimator_zero`、`pf2_deficit_visible` | 1 / 2 | **B** | `ΣB0·a/Σa = B0`；`A_drop/A_pix ≡ PF²`（同一函数仅半宽差 PF 倍） |
| 9 | `sim/exp_sim01…py:754/:755/:752` | `M3_*` | 2 / 3 / 2 | **B** | 向量化转写共用同一 `P.tangent_frame`；`sphere_to_chart` 是 `p3lib.chart_to_vec` 的**精确解析逆**再送回去 ⇒ 往返恒等，且 `:277` 只扫 `|z|<2/3`，**极冠分支根本没测** |
| 10 | `sim/exp_sim01…py:799` | `NC-B_equals_pf2_minus_1` | 1 | **B + 语义倒置** | `got` 不经裁剪/权重，`a_drop_chart/a_pix_chart ≡ PF²` ⇒ 实测 −0.35999 而预测 −0.36000；同一 dict 的 `judgement` 写着「**报警（红）**」却计入 `verdict="PASS"` |
| 11 | `route1/e4_flux_conservation.py:147` | `variance_law.ratio_min/max` | 2 | **B** | `ratio = (alpha²*Var[m])/(alpha²*Var[m])` —— **同一数组自除**；`acc2`（`:143`）算出后**从未被使用**（死代码伪装成第二次运行） |
| 12 | `route1/e5_projection_budgets.py:116` | `rel_agreement`（标为 **NEGATIVE CONTROL**） | 1 + 3 | **B** | `ρ=arctan(r)` 正是 `r=tan(ρ)` 的**逆函数**，逐元素 `dA_pl/dA_sp ≡ sec³ρ` ⇒ 机器精度必然一致。**用逆函数构造的「外部参照」，不是负控** |
| 13 | `route3/exp08_scale_constant.py:56/:57` | `identity_bitwise`、`rel_diff_about_minus_2e4` | 1 | **B** | 同一闭式的两种运算序（靠「N 全是 2 的幂」的浮点巧合）；门阈值 `1.97e-4` **就是同一商的手抄值**，两个操作数都是本地字面量 |
| 14 | `route3/exp07_quantization.py:38-39` | `bound_exact` | 1 | **B** | `\|255S−q\| ≤ 0.5/q` 是 `lround` 定义的同义改写；对生产量化语义零判别力 |
| 15 | `code/audit/kcorr/run_extra.py:57` | `gate_pass` | — | **恒红** | `\|k_corr − 1\| < 0.06` 对 N∈{5,25,289}，`k_corr` 有 O(1/N) 有限 N 偏置 ⇒ N=5 时 k=1.7673，**永不满足**。姊妹文件 `run_scan.py:73` 已改用正确口径，`read_tables.py:24` 更把整行**硬编码为 "PASS"** ⇒ 报告里的 PASS 与仓内任何 JSON 都不一致 |
| 16 | `code/audit/sim/exp_sim01…py:508→514` | `outside` 计数 | — | **fail-open** | `outside` 被下一行 `outside = 0` **覆盖** ⇒ `drops_cross_face_excluded` 恒报 0（实测 0/25600），而 `:743-744` 注脚宣称「跨面 drop 被 fail-closed 剔除」 |
| 17 | `code/audit/sim/exp_sim01…py:801-807` | `NC-C_rho_zero_zero_effect` | — | **fail-open** | 只有 `judgement` 字面串，**无 `gates` 键** ⇒ 从不参与 PASS/FAIL，尽管 docstring 把它列为四类判别力之一 |

**route1 无机器裁决聚合**：`grep "gates|all_pass|n_pass"` 在 `route1/` **零命中**
⇒ 结构上不会污染门计数，但也意味着 route1 **不提供任何可机器校验的证据**。

### 2.6 engineering-evidence（19 个 .py）

`v6/qa-design/oracle/qa_oracle.py` 是门密集面。**A 类（声明层诚实）**：docstring `:4-5`
明写「不 import、不 link、不执行任何生产实现或生产测试二进制」，`qa_matrix.json` 每门带
`must_not:["astrocs","lib/","cli/"]`。**但** `validate_spec.py:112-113` 的 `V-ORACLE-MUSTNOT`
只检查 `must_not` 非空，**从不校验其内容**，也无机制验证 oracle 真的没读 `lib/` ⇒ 声明无强制。

**B 类（结构上零 mutation 通路）**：

| # | file:line | 门 | 型 | 说明 |
|---|---|---|---|---|
| 1 | `qa_oracle.py:250-256` | `CHK-ANA-11F` / `G-ANA-11` | 1 | `m = abs(xj/xj − 1) + abs((0.8²·xj)/(0.8²·xj) − 1)`，两边字面相同 ⇒ `measured ≡ 0.0`。**函数体从不引用参数 `B`** ⇒ 无任何 mutation 通路（实测 38 条 mutation 全跑，该 check 一次不红） |
| 2 | `qa_oracle.py:475-480` | `CHK-BASE-05` / `G-BASE-05` | 1 | `prod`/`defr` 是两个**不相交字面量列表**；`B` 从不被读 |
| 3 | `qa_oracle.py:145-156` | `CHK-ANA-06` | 2 | `dims` 与 `exp` 是**同一份字面量表写两遍**后逐键互比；声称校验单位表，实际不读任何单位表 |
| 4 | `qa_oracle.py:119-132/:134-143/:158-165/:175-196` | `CHK-ANA-04/05/07A/08` | 1 / 2 | 四条绿路径分别是同一表达式自比或代数消去，实测全 `0.0` |
| 5 | `qa_oracle.py:167-173/:429-442/:444-451/:454-459,470-473,483-519` | `CHK-ANA-07B`、`CHK-INJ-06/07`、`CHK-BASE-01/04`、`CHK-RD-01/02/03/05/06` | 字面量 | 共 10 条只比对**同函数内构造的字面量列表**。`CHK-RD-03`（`:496-501`）把数据集名列表赋给 `idx` 后**从不使用**；`CHK-RD-05`（`:505-513`）把 12 个字面量查存在自己刚建的 12 字面量列表里 |

**部分恒真合取（C）**：`CHK-ANA-09`（`:198-208`）、`CHK-ANA-10`（`:210-229`）、
`CHK-MC-06`（`:347-359`）各有一支恒等，另一支为真判据。

**不可运行的脚本**：`release-01/VIS-001/l4/profile2.py` 与 `profile_and_crop.py` 都
`from fits_probe import read_fits`，而 **`fits_probe` 在全仓不存在**（`find` 无结果）。

### 2.7 additive-sky-seamless（70 个 .py）

见 §5 移交说明：核心 `c1`–`c7` 已完成分类，`audit_rework/`、`reverse_verify/`、
顶层 `aux` 三簇的完整逐行表在本单窗口内未完成，如实登记。

已确认的硬发现：

- **恒红/恒真并存于 `audit_rework/route1/c6_identifiability_dof.py:90`**：
  `kappa_identical = bool(k_c == k_f)`，而 `assess` 的对角均衡 `D⁻¹HD⁻¹` 对全局标量缩放
  **精确不变** ⇒ `k_c ≡ k_f` 数学恒等；但代码用**逐位** `==` 比较，实测 `reldiff` 1.1e-15
  ⇒ **逐位为 False ⇒ 恒红**。归档 JSON 实记 `"kappa_identifiable": false`。两条门都该删。
- `audit_rework/route3/exp03_median_variance.py:117` `"zero_effect_metric_zero": 0.0 == 0.0`
  字面量自比；`:100` `"identity_holds"` 拿公式值比**该公式的逐字誊抄**（实测逐位相同 ⇒ 恒绿）
  ⇒ **结构上无法发现生产 `sci_c_common.py` 用舍入 `1.4826` 与之差 2.22e-6 的真实分歧**。
- **覆盖率层把「测不了」判成「真无台阶」，且已实测漏绿**：`seam_gate_coverage.py:172`
  `if c["cov"] <= c["cov_null_hi"]: coverage_zone="none"`。实测 `frac=0.3` 一行：
  边上有真 4% 台阶、`rel_step=-0.00917` 低于门 0.01、生产门 **PASS**、覆盖率层不但没报
  「测不了」，反而归入 `none`＝「真无台阶」并**原样保留 PASS** ⇒ **主动出具了错误的肯定结论**，
  直接违反 `README.md:111`。9 条门全 OK 却没抓到，因为 `COV5` 只扫**无噪**臂，从不走带噪路径。
- `seam_gate_coverage.py` 的 `COV1_mirror_faithful` 实测逐位 0.0（恒绿）却计入 9 门汇总并被
  `README.md:485` 引用；`COV9_amplitude_independence` 阈值 `thr=1.5 ADU` 远低于网格最小台阶
  `6 ADU` ⇒ 五档读数全同 ⇒ 极差恒 0。
- `COV3/5/6/7/9` 全部写成 `all(... for r in ... if <filter>)`，**过滤器选到 0 行时 `all([])` 返回 True**。
- `sky_plane_zero_negative.py:311-314`：探针缺失时 N2a/N2b/N2c **一条门都不加入**，
  其余全绿 ⇒ `:373` 的退出码判定因 `n` 不含被跳过的门而走 `return 0`；
  docstring `:38-39` 承诺的退出码 2 **从未实现**。
- **更正**：`g08_defect_probe.py` 不是纯正面样板。10 个缺陷**没有一个能触及 `lib/**`**
  （注入全在探针自己的施加函数 `:105,:113-114`）；`N2c` 对「δ 加均匀常数偏置」
  **结构性必绿且逐位相同**（order-2 `polyfit` 基线吸收常数平移，而 `step` 平移不变），
  且 `:162,166` 用硬编码名字把这个盲区摘掉；`DEF5` 的 N2b 支是 D0 的数**逐位复用**。
  ⇒ 应降为「**判决函数**判别力证据（不覆盖 `lib/**`）」，不得引作生产判别力。

## 3 生产实现绑定（第 3 项的结论）

| 子树 | `.py` 数 | 真读 `lib/**` 的 |
|---|---|---|
| `absolute-snr/code/audit/` | **36** | **1**（`route3/exp11_frozen_operator_transfer.py`） |
| `healpix-polar` / `m42-realdata` / `engineering-evidence` / `shared` | **67** | **0** |
| `photometric-magnitude/code/redo/` | 27 | **0**（主链亦然） |
| `dense-snr-reconstruct` | 28 | **0** |
| `absolute-snr` 其余（`exp01`–`exp06`、`reverse_verify/`、`b7_recon_driver.cpp` 等） | — | 部分**真读**（`reverse_verify/frame_snr/` 直调 `snr_science.cpp`） |

⇒ 按规范 08 §4「不读真实对象的判据无效」，上表左侧绝大多数单元的绿灯
**不构成 ACSD 生产实现的判别力证据**。
`absolute-snr/code/audit/` 已在本单裁定为**本地重实现自检**（见该目录 `README.md` 顶部）。

## 4 归档脱钩（系统性）

| 位置 | 症状 |
|---|---|
| `absolute-snr/results/exp05_e5_gates.json` | `meta.n_gates=28 / all_pass=true` 描述的**门集合已不存在**；7 条纯恒等门已移入 `identity_checks`，另有 6 条真门不在存档里 |
| `photometric-magnitude/results/GATES.md` 与 `gates.json` | 落后于现行 `step9_collect.py`（G4 已改判红、G2 证据串已改），仍记 G4 = PASS |
| `dense-snr-reconstruct/results/route1/exp_p4_04_brightness_forward.json` | 只有 7 门，现行代码 14 门 ⇒ **8 条门无任何实测记录** |
| `healpix-polar` `read_tables.py:24` | 整行硬编码 `"PASS"`，与落盘 JSON 冲突 |

⇒ **「代码改好了、归档没重跑」在本仓是系统性的**，三次都导致报告仍引用已被代码自己否定的证据。
`results/` 由前台统一重跑；本单只登记，不覆写。

## 5 维护规则

- 新增恒真门时**必须**同时：①标 `degenerate`/`level="degenerate-control"` 并移出门计数；
  ②在本表登记 `file:line`、型别、类；③检查报告是否仍引它作判别力证据。
- 登记表的计数口径：**按门定义处计**。恒红门与恒真门同等无效，均须登记。
- 本表不替代各单元的 `REPORT_*.md`；报告引用的判据必须在报告里注明其证据等级。