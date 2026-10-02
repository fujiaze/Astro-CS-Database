# 实验域 · 结构性恒真门登记（永不能判红的判据）

本文件是**全实验域**（`实验/*` 八个单元）的恒真门登记表，覆盖
absolute-snr、additive-sky-seamless、dense-snr-reconstruct、photometric-magnitude、
healpix-polar、m42-realdata、engineering-evidence、shared。
单元内的旧登记表 `dense-snr-reconstruct/docs/TAUTOLOGY_REGISTER.md` 已改为指向本文件的存根。

> **登记深度不齐（如实登记，不得当作全域等价覆盖）**：本文件对八个单元的覆盖深度**不一致**。
> §2 有**逐条证据面**的只有七个：`dense-snr-reconstruct`（§2.1）、`photometric-magnitude`（§2.2）、
> `m42-realdata`（§2.3）、`shared`（§2.4）、`healpix-polar`（§2.5）、`engineering-evidence`（§2.6）、
> `additive-sky-seamless`（§2.7 + §2.8）。
> **`absolute-snr` 没有 §2 小节**：它在本文件中只出现在 §0.5（计数口径）、§3（生产实现绑定）、
> §4（归档脱钩）与 §2.2 的一句旁注，**共 12 处命中，无一条逐条登记**。
> ⇒ **本文件当前不是、也不得被引用为「全实验域逐条完备的唯一登记表」**；
> `absolute-snr` 的恒真门逐条分类**尚未进行**，登记为未决项（见 §0.5.6）。

## 0 处置原则（唯一）

依据 `run/GOVERN-08/工作包-GOVERN-08原件/standards/08_编译与CI重做规范.md` §4：
「判据可信：正确实现绿、注入对应缺陷红，正负例随附；**恒真、空断言、不读真实对象的判据无效**。」

⇒ **恒真门本身不是罪，把它当证据用才是。** 四类处置：

| 类 | 含义 | 处置 |
|---|---|---|
| **A** | 已诚实隔离：自带标注、已移出门计数、不承担证据位 | **保持不动**。只需检查是否仍被报告引作判别力证据 |
| **B** | 被计入 `gates`/`pass` 汇总或被报告当判别力证据，实则永不判红 | ①接真实参照量改造成真判据，或 ②移出判定、改列诊断项并**同步解除全部证据引用**。**不得保留「恒真但算作证据」的中间态** |
| **C** | 部分恒真合取：合取中某支恒真，整条失去部分判别力 | 拆出恒真那一支单独标注，或换掉该合取项 |
| **D** | 不读真实对象：比较的是本地桩/重实现，不是生产实现 | 见 §3「生产实现绑定」 |

**判别恒真 vs 恒红的双向体检**：除「能红吗」外还须查**恒红**——两个量逐位相同时 `判 <=tol`
的门永远红，而**恒红门会把真实缺陷永久藏在红灯里**。两类同样无效。

**三条硬纪律**：不得为了让门变绿而放宽阈值、加 epsilon、引入恒真路径或吞异常；
判红就如实报红；只改表述不改数据，真实读数一律保留（移出判定不等于删读数）。

## 0.5 计数口径（整改量的分母在这里，先读本节再看任何计数）

前一轮留下的「absolute-snr 108 条」与「可核 36 条」相差 3 倍，是因为两者**不是同一个口径**。
本节把口径钉死，之后本表所有数字都必须注明用的是哪一个。

### 0.5.1 三个口径的定义

| 口径 | 定义 | 去重键 | 用途 |
|---|---|---|---|
| **I 门实例**<br>`gate-instance` | 每一次「产生一个布尔量并把它放进某个门位」的**源码出现点**计 1。同一逻辑门写在 `for`/多个臂/多张 JSON 里，各计一次 | 无（就是出现次数） | 衡量**代码面积**，**不可用作整改量** |
| **II 去重门**<br>`deduplicated-gate` | 同一**逻辑门**只计 1 | `(文件, 归一化门名)` | 衡量**有多少扇独立的门** |
| **III 整改分母**<br>`remediation-denominator` | 口径 II 中**承担证据位**（进 `gates`/`pass`/`all_pass`/`n_pass`/`verdict` 汇总，或被报告当判别力证据）**且结构上不可能给出真实判决**者 | 同上，再按类筛选 | ✅ **整改量的唯一分母** |

**为什么分母必须是 III 而不是 I**：口径 I 把「一条门被 24 个分箱循环各写一次」
与「24 条互相独立的门」算成同一个数；又把已诚实隔离的 **A 类**（自带标注、已移出计数）
与纯诊断量同样进 I。**用 I 当分母会把整改量高估约 3 倍**，前车实测的
`absolute-snr 108` 与 `36` 的差正是这个量级。

### 0.5.2 实测数字（AST 扫描，只读，不改任何文件）

扫描规则（可复现）：`ast` 遍历 `实验/**/*.py`，取
①`dict` 字面量的字符串键、②`x["k"]=` 与 `x.k=` 的赋值，
其值为 `Compare` / `BoolOp` / `Not` / `IfExp` / 布尔字面量 / `bool|all|any|isclose|isfinite|allclose|all_pass(…)` 调用者，
记为一个门位。两种键名规则都报，避免口径随规则漂移：

| 单元 | 窄键名·实例 | 窄键名·去重 | 宽键名·实例 | 宽键名·去重 | 逐条证据面 |
|---|---|---|---|---|---|
| absolute-snr | **108** | 64 | 368 | 206 | ❌ **无**（§0.5.6） |
| additive-sky-seamless | 38 | 32 | 121 | 84 | ⚠ 部分（§2.7/§2.8，未完成） |
| dense-snr-reconstruct | 86 | 47 | 166 | 101 | ✅ §2.1（29 条） |
| engineering-evidence | 0 | 0 | 3 | 1 | ✅ §2.6（19 个 .py） |
| healpix-polar | 16 | 16 | 50 | 36 | ✅ §2.5（33 个 .py） |
| m42-realdata | 0 | 0 | 2 | 0 | ✅ §2.3 |
| photometric-magnitude | 43 | 21 | 94 | 54 | ✅ §2.2（P1） |
| shared | 42 | 11 | 164 | 93 | ✅ §2.4（9 个 .py） |
| **合计** | **333** | **191** | **968** | **575** | — |

⚠ **本表四列全部是口径 I / II（代码面积），没有一列是整改分母。**
整改分母（口径 III）**逐单元**给出，且**只有逐条证据面存在的单元才可对外引用**；
`absolute-snr` 的口径 III（36）在本文件内无证据面，见 §0.5.6。
⚠ `absolute-snr` 的 108 / 368 是**全表最大**（窄、宽两口径下均居首），
但**最大实例数 ≠ 最大整改量**：整改量要按口径 III 逐单元数，不可跨单元相加后再套用。

- **窄键名规则** = 键名匹配 `pass|ok|all_pass|verdict|*_pass|*_ok|*_gate|*_holds|*_zero|…`
  且不含 `level|value|dev|rel|ratio|resid|err|bias|max|min|mean|…`（排除纯诊断量）。
  该规则下 **`absolute-snr` 实例数 = 108，与前一轮派单引用的「108 条」逐位相同** ——
  证实那个数字是**口径 I**，不是整改量。
- **宽键名规则** = 任意布尔值门位（不筛键名）。它把每个布尔标志位都算进来 ⇒ **968 实例**，
  明显过宽。**报告口径时必须同时给出键名规则，否则数字不可比。**

### 0.5.3 「3 倍差」的精确分解（**单单元：absolute-snr**）

> ⚠ **本节全部数字的作用域 = `absolute-snr` 一个单元，不是全实验域。**
> 本文件**没有**任何逐条证据面支撑这 36 条（§2 无 `absolute-snr` 小节，见 §0.5.6），
> 故 **36 不得被引用为「全表/全域整改分母」**。任何「全表 N 条恒真门」的表述在本文件内
> **无据**——本表逐条可核的只有 §2 七个单元各自表头标出的条数。

| 步骤 | 口径 | 数值 | 作用域 | 相对上一行 |
|---|---|---|---|---|
| 起点 | I 门实例 | **108** | absolute-snr 单元 | — |
| 去重 | II 去重门 | **64** | absolute-snr 单元 | ÷1.69 |
| 剔 A/D/诊断量，只留 B∪C∪恒红 | **III 整改分母** | **36** | absolute-snr 单元 | ÷1.78 |
| 合计 | | | | **÷3.00** |

⇒ **「108 与 36 差 3 倍」已完全解释**：1.69 倍来自重复计数（口径 I→II），
1.78 倍来自把已诚实隔离的 A 类与非门量算进分母（口径 II→III）。**两者都不是缺陷，是口径差。**

⚠ **同名不同量的碰撞警告**：§3 表中 `absolute-snr/code/audit/` 一行的 **36** 是该目录的
**.py 文件数**（实测 `find … -name '*.py' | wc -l` = **36**），与本节的**门整改分母 36**
是**两个毫无关系的量，数值巧合相等**。引用「36」时必须写明是哪一个，否则必然误引。

### 0.5.4 本表的计数约定（此后强制）

- 本表 §2 各单元的 A/B/C/D 计数**一律是口径 III**（整改分母），并在该单元表头注明。
- **任何对外引用本表数字的地方，必须写明「口径 III / 整改分母」**；
  只写「共 N 处恒真门」而不写口径的，视为未完成登记。
- **任何对外引用本表数字的地方，必须同时写明作用域（哪个单元/子树）**；
  本文件无逐条证据面的单元（当前为 `absolute-snr`）的数字只能写成
  「某单元的未核实计数」，**不得**升格为全域分母。
- 口径 I 只用于描述代码面积，**不得出现在整改量、工作量估算或完成度声明里**。

### 0.5.5 本节已知局限（如实登记）

1. 窄规则**会漏掉以编号命名的门**（`H11`、`A1`、`CHK-ANA-06` 等），例如
   `absolute-snr/code/audit/` 在窄规则下去重门仅 7、在宽规则下 27，
   而前车按派单口径称该目录含 **64** 条 ⇒ **口径 II 的 191 是下界，不是精确值**。
2. 前一轮的「全域 246 条」在本节两个规则下都**复现不出**（窄 333 / 宽 968）。
   该数字的原始定义已不可考，**本表不再引用它**，改用上表两个可复现规则。
3. 口径 III 的 36（**作用域 = `absolute-snr` 一个单元**；「B 类绝对 SNR 部分」）
   是**语义分类**结果，不由 AST 自动判定；AST 只负责口径 I/II。
   分类依据是 `file:line` + 操作数同源性论证 + 实测读数。
   ⚠ **该 36 在本文件内没有逐条证据面**（§2 无 `absolute-snr` 小节），
   属**未核实计数**，不得当作全实验域分母——详见 §0.5.6。
4. `absolute-snr` 的 **108 / 368 确实是全表最大门实例数**（窄 108、宽 368，两口径下均居首；
   实测合计 窄 333 / 宽 968）。**但这两个数是口径 I（代码面积）**，按 §0.5.4 末条
   不得用作整改量；由它派生的 36 又是**单单元 + 无逐条证据面**（见 §0.5.6）。
   ⇒ **「108／368 是全表最大」成立；「36 是全表整改分母」不成立。**

### 0.5.6 `absolute-snr` 逐条证据面缺失（未决项，如实登记）

**事实**：本文件 §2 下的 `###` 小节共 **11** 个 —— §2.1 `dense-snr-reconstruct`、
§2.2 `photometric-magnitude`、§2.3 `m42-realdata`、§2.4 `shared`、§2.5 `healpix-polar`、
§2.6 `engineering-evidence`、§2.7，以及 §2.8 下的 §2.8.1–§2.8.4
（§2.8 本身是 `##` 级标题，不是 `###`）。
**其中没有 `absolute-snr`。** 全文 `absolute-snr` 共 12 处命中，分布为：
§0.5 计数节 6 处、§2.2 旁注 1 处、§3 生产实现绑定 3 处、§4 归档脱钩 1 处、
文首作用域声明 1 处 —— **无一条是逐条（`file:line` + 门名 + 机制）的登记行**。

**后果**：§0.5.3 的 36 与 §0.5.5 第 3 条的「B 类绝对 SNR 部分」在本文件内
**只有数字、没有证据面**，属**未核实计数**。

**处置（已执行）**：把 36 **降级为单单元（`absolute-snr`）分母**并全程标注作用域；
**不**补造证据面（本轮不审计该单元代码），**不**为了让分母好看而调整任何数字。
**待办**：`absolute-snr` 的逐条 A/B/C/D 分类（需覆盖 `code/audit/` 36 个 `.py`）
尚未进行；在补齐前，**全实验域整改分母在本文件内不存在**，任何全域数字都必须注明来源单元。

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

**归档脱节**：`【复现：按本单元复现清单重跑后读 exp_p4_04_brightness_forward.json；该读数需重跑取得，当前不可离线核验】 → gates` 只有 **7** 门，
现行代码 `:795-803` 有 **14** 门 ⇒ **8 条门没有任何实测记录**。

### 2.2 photometric-magnitude（P1）

| # | file:line | 门 | 型 | 类 | 说明 |
|---|---|---|---|---|---|
| 1 | `step9_collect.py:161` | `g2_c` | 1 | **B** | `sigma_residual_delta ≡ 0.0` **精确**：`calibrate:136` `r = log10(f_instr/f_syn)` ⇒ `r₁ ≡ r₀ + log10(shift)` 逐元素；`mad_sigma` 对平移严格不变、inlier 掩码逐元素相同。**已知隔离点 `step9_collect.py:148-163` 只隔离了 (a)(b)，(c) 漏网仍进判定** ⇒ G2 现只剩 `g2_d` 一个真子句 |
| 2 | `step4_guided_vs_blind.py:63` | `false_alarms=0` | 2 | **B** | **字面量**；消费方 `step9_collect.py:247` `g4_fa` ⇒ 永真。报告引用 `photometric-magnitude/docs/GATES.md:11`、`README.md:264` |
| 3 | `step5_calibration_gate.py:225` | `cross_frame_gate_present=False` | 2 | **B** | **字面量**；消费方 `:189` `g3_nogate` ⇒ 永真。报告引用 `photometric-magnitude/docs/GATES.md:10`、`README.md:251` |
| 4 | `step6_apply_and_units.py:61-62` | `I_photo_indep` vs `I_photo` | 2 | **C** | 注释称「不复用 `apply_photometry` 的实现」，但 `scia_pipeline.py:78-80` 的实现就是 `k_photo*m_map*img` ⇒ `rel ≡ 0.0` 精确。消费方 `:288` `g5_recompute` |
| 5 | `step9_collect.py:293` | `g5_fc` | — | **D** | 验证的是 `step6:85-90` 的 **4 行本地桩** `downstream()` 自己抛的 `RuntimeError`，`True` 分支从未执行 |
| 6 | `step9_collect.py:108` | `g1b_fold` | 1 | **C** | `sigma_flat_independent = PHOTON_MAG*s/sqrt(N_eff)`，右端就是 `s`，`N_eff = 1/ΣP² > 1` ⇒ 比值恒 < 1，**对任何归一化 PSF 恒真** |
| 7 | `step9_collect.py:109` | `g1b_quad` | 1 | **C** | `sigma_ceiling = ρ_hi·sqrt(…+σ_sys²…)`，`σ_sys` 是被 sqrt 内的真子集且 `ρ_hi>1` ⇒ 恒真 |
| 8 | `step7_negatives.py:104-105` | `N0` | 1 | **B** | `F_true = f_syn*inject_scale` ⇒ 残差逐元素全等 ⇒ `sigma_obs == 0.0`；`b0` 是**硬编码常量**提供非零 floor ⇒ 永真。有 `provides_injection_response=False` 标注但仍计入 `n_pass` |
| 9 | `step7_negatives.py:303-304` | `N5` | 2 | **B + D** | 门拿 `gate_verdict` 的定义核对 `gate_verdict` 自己（`ρ_lo<=0` 时必然返回 `LOWER_BOUND_UNDEFINED`） |
| 10 | `step7_negatives.py:247` | `N3` | 3 | **B + D** | 残差场是**合成**的，未走 `measure()→guided_photometry→fit_psf→sample_selection` 任何一步；σ_obs 与「漏掉的那项」由同一个 `amp_col` 标量驱动 |
| 11 | `step1_analytic.py:98/:100/:106-110/:141-144` | `rtol`/`k_rel_err`/N0/`oracle_S0_degenerate` | 1 | **B** | `analytic_frame(..., noise=False)` 直接返回 `k*mi*f_syn` ⇒ 三处「Oracle 恢复」全是代数恒等式。**`photometric-magnitude/docs/GATES.md:14` 把「Oracle 相对误差 3.33e-15」当作 G7 三类互证之一** |
| 12 | `step9_collect.py:232` | `g4_rob` | — | **恒红** | `fit_psf:384-385` 把位置硬约束在 `x0±2.0`，而 `far` 取偏移 ≥3 px 的行 ⇒ 匹配必然失败 ⇒ **对任何保留 ±2 px 搜索框的实现恒红**。**代码已诚实处理**（`:207-225`、`:276-279` 写明「量的是搜索框不是物理鲁棒性」并如实判红），但 **`photometric-magnitude/docs/GATES.md:11` 与 `【复现：按本单元复现清单重跑后读 gates.json；该读数需重跑取得，当前不可离线核验】` 仍记 G4 = PASS ⇒ 归档过期** |

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


**扫描末端补登（B14–B20，只登记未改代码）**

| # | file:line | 门 | 型 | 类 | 说明 | 报告引用 |
|---|---|---|---|---|---|---|
| B14★ | `redo/route3/exp_S00_anchor_verification.py:86-89` | 「幻觉锚」排除条件**按作者自己写的 `note` 字符串内容**判定（`"条件钳位" not in note and "锚漂移" not in note`）⇒ 15 个锚里 **3 个由手写注预置豁免**；配合 `:73` `"anchor_real": True` 是字面量起点（只有 `:78-80` 的 `except OSError` 才置 False，而那个方向是正确的 fail-closed）⇒ **幻觉检查本身有 3 处由作者手写注释放行，结论被预置** | 1/2 | **B** + D | **危害较高**，建议前台优先。修法：改显式白名单并公开 | `REPORT_route3.md:189`「15/15 锚真实存在」、`:189-194`「幻觉仅限 2 处文档行号漂移」 |
| B15 | `redo/route3/exp_S08_ladder.py:62` | `np.max(queries_ladder) <= 5`，而 `:49` `np.argmax` 作用在 5 元素 `LADDER` 上 ⇒ 元素恒 ∈ [1,5] ⇒ 由循环边界保证。**`:61` 的 criterion 字段自己写了「（结构性）」⇒ 有标注但仍在 `:145` 的 `verdict` 里**（「声明了但仍留在汇总」比「未声明」更危险） | 2 | **B** | 建议移出 `verdict` | `REPORT_route3.md:139` 带 ✔ |
| B16 | `redo/route3/exp_S08_ladder.py:76` | 两项都由**同一条闭式** `1.2533·σ_res/√N` 生成 ⇒ 比值 ≡ `√(2000/200)=√10≈3.1623`，**与 `1.2533` 与 `σ_res` 的取值无关** ⇒ 门实际只测「两个硬编码 N 之间的比值落在 (2.5,4)」 | 1 | **B** | — | `REPORT_experiment.md:117`、`REPORT_paper.md:108` |
| B17 | `redo/route3/exp_S07_fov_constants.py:57` | `1.2 * 1.0 > 10 * wcs_err_deg`：左边是字面量、右边 `wcs_err_deg` 配 `:56` 自陈的写死常数 5.6e-4 ⇒ **无数据、无随机、无代码路径**。同文件 `:70` 的 4 个构型是**作者手挑且全部落在 `[1.2,10]` 内** ⇒ 恒真 | 1/2 | **B** | — | `REPORT_route3.md:131` 带 ✔ |
| B18 | `redo/route2/exp3_adaptive_ladder.py:79` | `:69` 模拟器自己的哨兵 `or i==4` 强制在 i=4 返回 ⇒ 判据只能取 0。`:40` docstring 自陈「**Emulate** `pc_api.cpp:977-1008`」而该 C++ **从未被打开** ⇒ 本地桩冒充生产 | 2 | **B + D** | — | `REPORT_route2.md:132`「G=17 档可达性 = 0（**复现** …）」 |
| B19 | `redo/route2/exp5_match_radius.py:92` | `false_match_count_metric: 0` 是字面量；ρ→0 的孤立场**从未被仿真**（`:30-35` 的 `recovery_prob`/`false_match_prob` 是本文件自写闭式，生产 KD-tree 匹配器未调用） | 2 | **B + D** | — | `REPORT_route2.md:153` 带 ✅ |
| B20 | `redo/route3/exp_S04_mag_tolerance.py:82-83/:94` | `:112` 汇总的 3 项中 **2 项永真**：`H4a` 两个合取项**都是**代数恒等式（`scale=10^-location` ⇒ `scale(k)/scale(1)=1/k` 精确；残差同量平移 ⇒ MAD 严格不变）；`H4b_clean` 的「无效应」由**注入幅度 0.015 dex ≪ 最窄窗宽 0.5 mag** 保证，是算术后果不是检验结果。全文件 `grep discriminating_power` **零命中** | 1 | **B** | 同文件 `:109` `H4b_contaminated` 是唯一真判据，**勿动** | 无独立引用；**P1-m09 自查未覆盖 S04** |

#### 可机械执行的整改（7 文件 / 9 门位，只加标记 + 移出 `verdict`，不动实现）

| 文件 | 门位 | 现状 |
|---|---|---|
| `exp_S03_irls_convergence.py:81-87` | `negative_zero_check` | `:84` `"delta_location": 0.0` 字面量 + `:85` `pass = it == 0`，而本地 `irls:24-25` 对 `s<=0` 直接 return ⇒ 恒真 |
| `exp_S05_gates_inlier.py:94-99` | `negative_gate1` | `:96-97` 三字段**全是字面量 `True`** + `:98` `"pass": True`；声称验「n=2 ⇒ NO_DATA」但 n=2 根本没进任何函数 |
| `exp_S09_mag_minmax.py:73-77` | `negative_zero_check` | `:74` `"same_family_delta": zp_16 - zp_16` **自己减自己** + `:76` 字面量 `True` |
| `exp_S10_match_radius.py:70-75` | `negative_zero_check` | `:73` `"value": 0.0` + `:74` `"pass": True` 均字面量 |
| `exp_S12_dex_bound.py:64-69` | `negative_zero_check` | **字面量 `"negative_zero": True` 直接写进 `:72` 的 `verdict`** |
| `exp_S11_zp_sample_floor.py:68-75` | `negative_zero_check` | 隔离标记**有效**，但 `:77` 的 `verdict` 仍写字面量 `True` |
| `exp_S04_mag_tolerance.py:82-94` | `H4a` / `H4b_clean` | 见 B20 |

**样板（勿误改）**：`exp_S09_mag_minmax.py:72` `H9b.pass` —— 登记 + 已移出 `:83` 的 `verdict`
两件都做了，`docs/DISPUTES.md:104-106` 明写「恒真门无证据资格」⇒ **全仓最标准的一处隔离**。

#### 最高杠杆的一处：撤回声明没有落到结果行

P1 单元**已经写了**撤回声明，但**没有一条落到 route2/route3 报告的结果行上**：

| 撤回声明位置 | 声明内容 | 仍带 ✔ 的结果行 |
|---|---|---|
| `REPORT_experiment.md:79,:122`；`REPORT_paper.md:201` | `exp_S06` H6a/H6b + `exp_S02`/`exp_S11` 的 `negative_zero_check`「无判别力、不得当证据」 | `REPORT_route3.md:84`、`:122`、`:166` |
| `docs/DISPUTES.md:104-106` | S09 `H9b`「恒真」、`negative_zero_check` 是字面量 | `REPORT_route3.md:148` |
| 主链 `step9_collect.py:148-163` | G2 的 (a)(b) 降级为 `same_source_self_checks` | `photometric-magnitude/docs/GATES.md:9` 仍是隔离**之前**的旧证据串 |

⇒ **只改这 3 张表即可消除本单元一半的「声称有证据实则恒真」，不动任何实现。**
本单窗口内未做完，登记为待办。

#### 结构性缺口

`photometric-magnitude/docs/GATES.md` 的 G1–G9 **完全不含 `route1`/`route2`/`route3`/`reverse_verify` 的任何一条门**
（只有 `step5/step7/step8` 链）⇒ **33 个 `.py` 的门没有任何机器可执行的总账**。
而 `REPORT_route3.md:231` 写「13 个实验脚本全部运行通过（**verdict 全绿**）」，
那个 `verdict` 里含至少 10 条永真或字面量行。
另：`reverse_verify/p1_spatial_gain/` 有 3 处可复现性断裂——`real_gain.py:43` 的仓根定位器
找 `docs/ASTROCS_DESIGN.md` 而仓内是 `docs/ACSD_DESIGN.md`（8 次循环全落空）；
`real_ridge.py:157`/`real_pixel_check.py:171`/`analyze_real.py:112` 三处写文件前缺 `os.makedirs`；
`cpp/p1sg_oracle.cpp` 是本单元**唯一带真退出码的真门**（阈值写在看结果之前，含 3 个负例），
但 `grep p1sg_oracle` 在所有 `.py` 中**零命中** ⇒ **没有任何 Python 驱动它**。

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

> **移交说明（原 §2.7 的「见 §5 移交说明：核心 `c1`–`c7` 已完成分类」已删除）**：
> 该指针**悬空**——本文件 §5 是「维护规则」，既无移交说明，也**不含任何 `c1`–`c7` 分类**。
> 实测：`c1`–`c7` 在本文件内**零命中**，其 A/B/C/D 分类**从未在本文件内产出过**。
> ⇒ 该「已完成分类」的声称**无据，已撤回**。现状如实登记：
> - **`c1`–`c7`（历史正本腿）逐条 A/B/C/D 分类：未完成**（本文件内无）。
>   这些腿的**读数效力**另有正本讨论（不属本表职责）：
>   `实验/additive-sky-seamless/REPORT_experiment.md:25,168`（称其为「历史正本」、
>   C3/C7 定性方向不变、C5 定量优势不可复现、原始运行版本不可判定）、
>   `REPORT_paper.md:9,110,124`（凡标 `[实验:c1–c7*.py]` 的量值均为历史读数）。
>   **注意：那是「读数效力」的限定，不是恒真门的 A/B/C/D 分类**，不得当作已完成的分类引用。
> - **`audit_rework/`、`reverse_verify/`、顶层 `aux` 三簇完整逐行表：未完成**（见本节下方硬发现，
>   其中 `audit_rework/` 的 6 条 `open_red` 已在 §6.3 落地可见化）。
> - `aux` 簇**不存在**（实测见 §2.8：`reverse_verify/` 顶层只有 4 个子目录，无散落 `.py`/`.sh`）。
> ⇒ **本单元当前没有完整的逐条证据面**，其「70 个 .py」只是规模描述，不是整改分母。

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
| `absolute-snr/【复现：实验/absolute-snr/code/exp05/run_all.sh 生成的 exp05_e5_gates.json】` | `meta.n_gates=28 / all_pass=true` 描述的**门集合已不存在**；7 条纯恒等门已移入 `identity_checks`，另有 6 条真门不在存档里 |
| `photometric-magnitude/photometric-magnitude/docs/GATES.md` 与 `gates.json` | 落后于现行 `step9_collect.py`（G4 已改判红、G2 证据串已改），仍记 G4 = PASS |
| `dense-snr-reconstruct/【复现：按本单元复现清单重跑后读 exp_p4_04_brightness_forward.json；该读数需重跑取得，当前不可离线核验】` | 只有 7 门，现行代码 14 门 ⇒ **8 条门无任何实测记录** |
| `healpix-polar` `read_tables.py:24` | 整行硬编码 `"PASS"`，与落盘 JSON 冲突 |

⇒ **「代码改好了、归档没重跑」在本仓是系统性的**，三次都导致报告仍引用已被代码自己否定的证据。
`results/` 由前台统一重跑；本单只登记，不覆写。

## 5 维护规则

- 新增恒真门时**必须**同时：①标 `degenerate`/`level="degenerate-control"` 并移出门计数；
  ②在本表登记 `file:line`、型别、类；③检查报告是否仍引它作判别力证据。
- 登记表的计数口径：**按门定义处计**。恒红门与恒真门同等无效，均须登记。
- 本表不替代各单元的 `REPORT_*.md`；报告引用的判据必须在报告里注明其证据等级。

## 2.8 `reverse_verify/data_matrix/` 与 `m16_scene/`（口径 III，本批次补完）

> 本节补上前一轮 §2.7 声明「逐行穷举表未完成」的缺口。
> **口径**：§0.5 的**口径 III（整改分母）**；类标为 B/C/D/fail-open，`open_red` 即真实主张门当前为红。

**规模**：`data_matrix/` 4 个 `.py`（1050 行，3 个 runner + `dtmlib.py`）、
`m16_scene/` 3 个 `.py`（623 行）。`aux` 簇**不存在**——
`reverse_verify/` 顶层只有 4 个子目录，无散落 `.py`/`.sh`。

### 2.8.1 `data_matrix/`（**19 条**，表头原写「12 条」已订正；19 = 实际逐行登记行数）

| # | file:line | 门 | 型 | 类 | 机制 | 计入 | 报告引用 |
|---|---|---|---|---|---|---|---|
| 1 | `exp1_sky_poisson_snr.py:154→168` | `C1_sky_snr_monotone_decreasing` | — | **D（真门）** | 8 档实测 SNR 中位 84.7…14.6 单调；可红 | 是 | 无 |
| 2 | `exp1:157→170` | `C2_ratio_matches_poisson_prediction` | 3 | **B + D** | 期望 σ 取自**生成了被测帧的同一个** `noise_model.py`（`rng.poisson`）⇒ 自洽但错误的噪声模型照样通过。实测 rel_dev 0.0061 / 阈 0.05 | 是 | 无 |
| 3 | `exp1:185` | `C3_low_snr_regime_reached` | — | **D（真门）** | 实测 0.78 < 15 | 是 | 无 |
| 4 | `exp1:196-212→217` | `N1_additive_only_metric_vanishes` | 1+2 | **B（逐位恒等）** | `:202-207` 把**同一个** `delta_adu` 加到配对的两帧上 ⇒ 差分逐位相消、`b_hat = median(annulus)` 再相消一次。存档 `max|SNR_ctrl/SNR_ref − 1| = 0.000e+00` | 是 | 无 |
| 5 | `exp1:236`/`:245→253` 支 1 | `N2_…` 配对半 | 1+2 | **B + C** | 同 seed、同 `frame_index`；`MODE_MEAN_ONLY` 下 `n_e = lam_e.copy()`（**不消耗 RNG**）、`sweep_sky.json` 的 `cosmic_ray: null` ⇒ `rng.normal` 逐位相同 ⇒ 两臂只差一个被 `b_hat` 消掉的确定性基座。存档 `paired_ratios = [1.0]×6` | 是 | 无 |
| 6 | `exp1:246→253` 支 2 | `N2_…` 非配对半 | — | 真门（**唯一负载**） | 实测 0.0396 / 阈 0.05 ⇒ **只剩 21 % 余量**；且同时改 seed **与** `frame_index`，不是受控对照 | 是 | 无 |
| 7 | `exp2_mosaic_shape_difference.py:233-236` | `G2_common_mode_negative_vanishes` | 2 | **B（逐位恒等）** | `common_mode_overlap.json` 的 6 帧**不带任何逐帧参数**，`frame_weights()`（`:60-65`）纯由确定性量构成 ⇒ 6 张权重图逐位相同 ⇒ `std(delta,axis=0)` **精确 0.0**、`R` **精确 1.0**。任何 seed、任何噪声下都不可能非零 | 是 | 无 |
| 8 | `exp2:237-240` | `G3_contrast` | — | **B + fail-open（无条件绿）** | `ratio = (d_real/d_com) if d_com > 0 else float("inf")`；G7 的恒等式强制 `d_com ≡ 0.0` ⇒ **`else inf` 恒触发** ⇒ `inf > 10.0` **恒真**。零信息量 | 是 | 无 |
| 9 | `exp2:229-232` | `G1_material_penalty_real_different_pointing` | — | **D + 臂与文档不符** | `mosaic_diff_pointing_realbase.json` 六帧 `pointing: null` ⇒ 完全共指、`glob_index: 0` ⇒ `render.py:299 p = hits[0]` 取**同一块**真实板块；docstring `:20` 却称「6 个**不同真实板块**…不同指向」 | 是 | 无 |
| 10 | `exp2:241-247` | `G4_synthetic_different_pointing_detectable` | — | 真门、fail-closed | 错误路径 `{"error":…}` ⇒ `.get(…,0)` = 0 ⇒ 红 | 是 | 无 |
| 11 | `exp2:248-252` | `G5_measurable_from_frames` | — | 真门 | 独立测量的块 σ | 是 | 无 |
| 12 | `exp2:253-257` vs `:258-261` | `G6_conditions_only_change_not_material` | — | 真门 + **元数据自相矛盾** | G6 门住 `dark[...]["R_SP0_median"] < 1.05`，同一个数又被存进 `reference_dark_samefield` 并注「参考值，**非判据**」 | 是 | 无 |
| 13 | `exp3_variance_closure.py:163-165→169` | `E3_additive_arm_no_effect` | 1 | **B（纯代数）** | `((f1+δ) − (f2+δ))` vs `(f1 − f2)`：同一个标量加在减法的两个操作数上 | 是 | 无 |
| 14 | `exp3:154-156` | —（死代码） | — | 死代码 | `for b in base["blocks"]: dv.append(0.0)`；**`dv` 从不被读**。读起来像逐块方差变化计算，实则追加字面量 0.0 | 否 | 无 |
| 15 | `exp3:173-186` | `E4_closure_test_has_power` | — | **B（弱）+ 文档/臂不符** | 拿 mean_only 臂的读噪声底方差去比一个乘了 `B7/B0 = 250` 的预测（`:176`），且 `src_e` 写死 0.0（`:178`）⇒ 结构上远大于 0.5。docstring `:19` 写「纯加性臂」，代码跑的是 `MODE_MEAN_ONLY` | 是 | 无 |
| 16 | `exp3:67-68→114`/`:138` | `E1_physical_arm_variance_closure`、`E2_spatially_structured_sky_closure` | 3 | **B + D** | `var_pred_pix` 的 `lam_e` 直接取自**渲染器自己的真值面** ⇒ 期望值由产生被测值的同一来源重算；只能验证渲染器实现了它自己声明的模型 | 是 | 无 |
| 17 | `exp3:203-206` | `E5_production_estimator_bias_quantified` | — | **B（反向门）** | `abs(prod_bias) > 0.5×0.1`：**估计器正确时判红**。docstring `:22` 还称偏差是单向的（「偏高」），实测项却是 `abs()` | 是 | 无 |
| 18 | `dtmlib.py:283-284` | `missing_real_base_inputs()` | — | **A + fail-open** | `pats = [q for q in pats if any(ch in q for ch in "*?[")]`：**不含通配符**的 `real_base.path` 被滤掉、永不检查 ⇒ 守卫报 `ok: True`，失败改为在 `render.real_base_surface` 里抛 `FileNotFoundError` | 喂 SKIP | 无 |
| 19 | `dtmlib.py:277-280` | 损坏 recipe | — | **A + fail-open**（已文档化） | `except Exception: continue` | 否 | 无 |

### 2.8.2 `m16_scene/`（**13 条**，表头原写「11 条」已订正；13 = 实际逐行登记行数）

| # | file:line | 门 | 型 | 类 | 机制 | 计入 | 报告引用 |
|---|---|---|---|---|---|---|---|
| 1 | `exp_a6_seeing_aperture.py:228-231` | `P1_psf_domain_seesing_independent` | — | 真门（D） | 实测 0.00297 / 0.00943 | 是 | `m16_scene/README.md:11` |
| 2 | `:232-235` | `P2_box5_seesing_dependent` | — | 真门（D） | 实测 0.3449 / 阈 0.30（15 % 余量）。但对生产缺陷 `star_detector.cpp:151` 的论证走的是 `FL.est_box5`（`:10-11`、`:38`）——**本地重实现**，产品从不运行 | 是 | `README.md:11` |
| 3 | `:260-263` | `P3a_negative_control_identical` | 2+3 | **B（逐位恒等）** | 同一 scene dict 用**同一 seed 渲染两次**（`:251-252`），`est_psf_optimal` 无 RNG，`dmag(x,x) = −2.5·log10(1) = −0.0`。这是渲染器确定性测试；docstring `:50` 归给它的「证明臂间差异来自 PSF 核而非噪声重抽」**是 P3b 的命题** | 是 | **`README.md:11`「负例逐位归零」** |
| 4 | `:272-275` | `P3b_negative_control_noise_only` | — | **C** | 合取第 2 支 `mad_noise < box_span`：拿 ~1e-2 mag 的 MAD 比 ~0.3 mag 的 `box_span`（P2 已独立强制 ≥0.30）⇒ 约松 30 倍，现实不可能红 | 是 | `README.md:11` |
| 5 | `exp_variance_closure.py:129`/`:161` | `C3_pass` | 2 | **B + fail-open** | `np.all(a1[~v1] == 0.0)`，而 `:80` 已经做过 `a1 = np.where(v1, f1.adu, 0.0)` ⇒ **断言 `np.where` 把自己的 0 归零了**；无无效像素时 `else True` 空过 | 是 | **`README.md:12`「C3 掩膜传播」计入 9/9 PASS** |
| 6 | 同上 `:32`（docstring） | C3 声称 | 过度声称 | — | 「无效像素必须为 0，**且与 HDU MASK 一致**」——后半段**全文件未实现**（从不读 mask HDU） | 是 | `README.md:12` |
| 7 | `:159` | `C1_pass` | 3 | **B + D** | 期望值取自渲染器自己的真值面 | 是 | `README.md:12` |
| 8 | `:160` | `C2_pass` | — | 真门但**容差自指** | `tol = 3√2·sig_v`，`sig_v = v_on·sqrt(8/n_pairs)` ⇒ 方差被放大 10 倍，容差同步放大 10 倍仍能过。诚实的归一化是同文件 `:136` 的 `C2_z_score` | 是 | `README.md:12` |
| 9 | `:158` | `C4_pass` | — | 真门 | bulk [p1,p99] 区间 \|rel\| ≤ 2 %，依据充分 | 是 | `README.md:12` |
| 10 | `:65-67` | —（死桩） | — | 死代码 | `def flat_blocks(...): """…""" return None` — 有 docstring 的桩，从不被调用 | 否 | 无 |
| 11 | `exp_realbase_consistency.py:99-103` | `C1_max_abs_rel_dev_implied_base_rate` | 1 | **B（自陈恒等式却仍计分）** | `implied = truth_e/(t·m) − sky − dark/m` 是渲染器自己链条 `truth_e = t(src+sky)m + tD` 的**精确逆**，docstring `:26` 自陈「故是恒等式」，却坐在 `rec["pass"]` 里并打印 PASS/FAIL。**反证**：`README.md:13` 记它在修复前真红过（`C1 = 0.99990 = 1 − 1/t`）⇒ 它是 P9 ×t 缺陷的**真回归守卫**，不是捏造的门 | 是 | **`README.md:13`** |
| 12 | `:117-118` | `C2_sigma_ratio` | — | 真门、fail-closed（**不是 D**） | `isfinite(sig_ratio) and 0.5 <= ratio <= 2.0`；NaN ⇒ 红。**真读真实 HST FITS**（`:72` `from astropy.io import fits`） | 是 | `README.md:13` |
| 13 | `:114`/`:119` | `C3_pearson_r` | — | 真门但**不完整** | docstring `:31` 把「剔除合成饱和像素后再算一次」写进判据，但 `r_unsat` 算了（`:115`）、报了（`:127`），**从未进入任何 pass 表达式** | 是 | `README.md:13` |

### 2.8.3 簇级类 D 结论与两个新的「无产物被当有产物」

`grep -nE 'subprocess|astropy|fits\.open|open\(|scia_|sci_c_common|lib/|upm_probe|sky_probe|acsd'`
在两簇内只有 **4 处命中**：三个 `open(…,"w")` 写盘 + 一个 `from astropy.io import fits`。
⇒ **无一条门读 `lib/**`、无一条 shell 到 `acsd` 二进制、无一条触 `sky_probe`/`upm_probe`/`sci_c_common`**
⇒ **整簇为类 D**（已在 `README.md:420-421` 用文字披露，但仍以 `level=data` 留在 `all_pass` 内）。

**两条比恒真门更严重的新发现**：

1. **`exp2` 与 `exp3` 从未产出过任何判决。** 两个脚本的固化 JSON 都写着
   `"status": "SKIP"`, `"all_pass": null` ⇒ **G1–G6 与 E1–E5 共 11 条门的阈值在本仓一次都没被求值过**。
   （缺输入守卫本身是诚实的：`dtmlib.py:289-299` 写 `status: SKIP, all_pass: null`、返回 rc=2，
   由 `exp2:213-215`／`exp3:96-98` 消费，**没有假通过**。）
2. **`m16_scene/README.md:12-13` 的「9/9 PASS」有两行引的是根本不存在的结果文件**：
   `closure/variance_closure.json` 与 `M16FIX/regression.json` 都不存在，且
   `REPORT_experiment.md:164` 记 `m16_scene.py:550-552` 的 `AssertionError` 让三个脚本全部跑不动。
   ⇒ **「没跑过」被写成「全过」**，与 `c10_anchor_forensics.py` 的「找不到被记成找到」同型。

### 2.8.4 两簇小结（口径 III）

| 口径 | `data_matrix` | `m16_scene` | 合计 |
|---|---|---|---|
| B（含 B+D、B+fail-open） | 10 | 4 | **14** |
| C（部分恒真合取） | 1（N2，另计 B） | 2（P3b、C2_pass，另计 B） | **3** |
| A（已诚实隔离） | 1（rc=2 跳过守卫） | 0 | **1** |
| D（整簇） | 全部 10 条实验门 + 输入守卫 | 7 | **17** |
| fail-open | 3 | 2 | **5** |
| 恒红 | **0** | **0** | **0** |
| 真门（无判别力问题） | 6 | 6 | **12** |

⇒ 两簇的失效方向是**恒绿**，不是恒红；本域此前的恒红集中在 `audit_rework/`
（**见 §6.3 的 6 条 `open_red` 表**，其中第 1–3 条为恒红：c6 `kappa_identical`、
c7 `fake_ivar_domination.exclusive`、e8 `kappa_changes`）。
⚠ 本行原写「见 §2.7 与 **§7.3**」——**本文件不存在 §7**（章节止于 §6.3），该指针悬空已删除。
⚠ 口径提示：上表是**类计数**（同一行可同时计入 B 与 C/D/fail-open），不等于表头条数；
§2.8.1/§2.8.2 表头的逐行登记数（19 / 13）与本表类计数互相自洽，原表头 12 / 11 有误已订正。


## 6 本批次（第二批）的代码侧处置

> 口径与总量见交付件 `run/GOVERN-08/审核包-R2/G08-05-整改-恒真门批次2.md`。
> 本节只登记**已落到代码的**处置；纯登记未改代码的条目不在此列。

### 6.1 `additive-sky-seamless/code/seam_gate_coverage.py`：「测不了」被写成「真无台阶」

**危害等级：全域最高**——这是**主动出具错误的肯定结论**，不是恒真门。

| 项 | 内容 |
|---|---|
| 原位置 | `:172` `if c["cov"] <= c["cov_null_hi"]: coverage_zone = "none"`，注释「真无台阶：中位读数 0 是有效结论」 |
| 实测漏绿 | 真实 4 % 台阶（12 ADU）、`frac = 0.30`、σ_pix=20 ⇒ `rel_step = −0.0091718`（低于门阈 0.01）⇒ 生产 `PASS`；覆盖率层 `cov = 0.0075 ≤ cov_null_hi` ⇒ `zone = none` ⇒ **原样保留 PASS** |
| 根因（定量） | `cov` 是逐样本判据 ⇒ 最小可检相对幅度是 `thr/bg`。无噪时 `thr = 0.5·gate·bg = 1.5 ADU` ⇒ 检出限 0.005 = **0.5×门阈**（比门灵敏 2 倍）；带噪时 `thr ≈ 3·MAD(ctrl) ≈ 66–70 ADU` ⇒ 检出限 **≈0.22 = 门阈的 21.6–23.0 倍**。**比门瞎 22 倍的诊断量给出的是「看不见」，代码却写成「没有」** |
| 处置 | `coverage()` 新增 `detection_floor_rel = thr/bg` 与 `blind_to_gate`；`edge_with_coverage()` 增第四区 `unresolved`；新增判决值 `UNRESOLVED`（证据不足，**不计通过也不谎称有台阶**）；`n == 0` 退化分支由 `cov=0.0` 改为 fail-closed 的 `blind_to_gate=True` |
| 顺带纠正（反方向） | 原实现在 `frac = 1.0` 带噪臂上把生产的 `FAIL` 覆盖成 `FAIL_COVERAGE`（真实台阶被改记成「覆盖率不足」）。现定优先级 `FAIL > FAIL_COVERAGE > UNRESOLVED > PASS`：**生产已测到台阶时一律记 `FAIL`** |
| 注入验证 | 正确实现 **10 门全 PASS、退出码 0**；注入 A（撤掉检出限前置、退回三区逻辑）⇒ **COV5/COV8/COV10 三门红、退出码 1**，且 `frac=0.30` 判决由 `UNRESOLVED` **退回 `PASS`** |

同文件内一并处置的恒真/空过门：

| 门 | 判定 | 处置 |
|---|---|---|
| `COV1_mirror_faithful` | **B（结构对称）**：`edge_samples` 与生产 `edge_metric` 走**同一条 `bilinear` 采样路径**、同选边规则、同 `median` ⇒ 实测差**逐位 0.0** | **移出门计数**，改入 `selfchecks`；读数保留 |
| `COV3/5/6/7/9` | **B（空过）**：原为 `all(... for r in ... if <filter>)`，过滤器选到 0 行时 `all([])` 返回 **True** | 改为「先断言分母非空」 |
| `COV5` | 只扫**无噪**臂、从不走带噪路径 ⇒ 9 门全 OK 却没抓到 | 两臂都扫 |
| `COV9_amplitude_independence` | **B（夹具的解析后果）**：无噪时 `thr` 恒为兜底项 ⇒ `cov ≡ f`，极差实测 0.0 | **路线①**：加带噪二维网格后该命题**实测不成立**（极差 0.045–0.887）。未放宽阈值（0.01 未动），把无噪那条移入自检、带噪实测写成方向相反的可判红断言 |

### 6.2 `photometric-magnitude/code/redo/route3/`：六个文件的 `discriminating_power` 标记

按 `exp_S09:72` `H9b`（全仓最干净样板）给六个文件补标记并**移出 `verdict`**，读数一律保留：

| 文件 | 移出门位 | 判定 |
|---|---|---|
| `exp_S03_irls_convergence.py` | `negative_zero_check` | 常值场 ⇒ Δ 恒 0 是构造使然；且 `delta_location` 原是**写死的字面量 0.0**，不是算出来的 |
| `exp_S04_mag_tolerance.py` | `H4b_clean` | 零效应守卫：清洁场下窗宽本就不截断，不能证明窗宽无关性（真负载是 `H4b_contaminated`） |
| `exp_S05_gates_inlier.py` | `negative_gate1` | **三个字段与 `pass` 全是硬编码字面量 `True`**，没有任何计算 |
| `exp_S09_mag_minmax.py` | `negative_zero_check` | `zp_16 − zp_16` 自减 |
| `exp_S10_match_radius.py` | `negative_zero_check` | `value` 写死 0.0、`pass` 写死 True |
| `exp_S12_dex_bound.py` | `negative_zero_check` | `m ≡ 1` 是构造；`verdict` 里原本是**字面量 `True`** 直接写入 |

⇒ 六处 `verdict` 现在只含真实门（实测 H3a/H3b/v8_replay、H4a/H4b_contaminated、
H5a/H5b/H5c、H9a、H10a/H10b、H12a/H12b），并在 JSON 里新增 `registered_not_counted` 字段。

### 6.3 `additive-sky-seamless/code/audit_rework/`：门汇总器（修「已判红的门被静默吞掉」）

**根因**：本子树 38 个脚本**无一 `sys.exit(1)`、无一 `exit(1)`**（`grep` 零命中），
而 `run_all.sh` 用 `set -eu` 并在 `run()` 里向上传子壳退出码
⇒ **红灯在物理上无法传播到退出码**：任何判红都不会让 `run_all.sh` 失败，
违反规范 08 §5「红灯不以 waiver 覆盖，SKIP 不计通过」。

**为什么不能「见 false 就红」**：固化正本里有 **101 个 `false`**、落在 **20** 个 `(文件, 叶名)`
组合上，四类语义完全不同——`demonstration`（负例演示，为假**正是**被演示的内容）、
`diagnostic`（诊断读数）、`table_cell`（网格逐格分类标记）、`open_red`（真实主张门当前为红）。
一刀切会产生大量假警报，而**假警报会让汇总器恒红**——恒红与恒真同等无效。

**处置**：
- 新增 `rollup.py`（tracked）+ `GATE_DISCLOSURE.json`（32 条逐点披露，tracked）；
- 键为 `<结果文件名>|<归一点路径>`（数组下标归一为 `[]`）——
  工作副本是平铺的（`c3_seam_gate.json`）、固化正本带路由前缀（`route1/c3_seam_gate.json`），
  按相对路径会让同一条门算两次；
- **fail-closed**：任何 `false` 未被披露 ⇒ 记 `UNDECLARED` ⇒ **判红**；
- `open_red` **不豁免**，照样让汇总器红；
- 与 `【复现：按本单元复现清单重跑后读 _gate_rollup.json；该读数需重跑取得，当前不可离线核验】`（另一汇总器的产物）做**交叉核对**：
  任何一方标红而本表未列 `open_red` ⇒ 判红；
- 两个来源都扫（工作副本 + 固化正本），只扫其一都会静默漏判。

**实测**：真实状态下 32 个 `false` 全部已披露、0 未披露、**6 条 `open_red`** ⇒ **退出码 1**。
注入验证（在 `/tmp` 副本把 6 条 `open_red` 翻绿并同步更新披露）⇒ **退出码 0**
⇒ 汇总器**双向可判**，不是恒红门。

**6 条 `open_red`（不豁免，保持红）**：

| # | 位置 | 性质 |
|---|---|---|
| 1 | `route1/c6_identifiability_dof.json` `global_weight_invariance.kappa_identical` | **结构性恒红**：对角均衡 `D⁻¹HD⁻¹` 对全局标量缩放精确不变 ⇒ `k_c ≡ k_f` 数学恒等，但代码用**逐位 `==`** ⇒ 红不红由浮点最后一位决定（实测 reldiff 1.10e-15） |
| 2 | `route1/c7_share_vs_abs_weight.json` `fake_ivar_domination.exclusive` | **恒红门被当作正面证据引用**：`share > 1.0 − 1e-20` 在 binary64 下**坍回精确 1.0** ⇒ 实为 `share > 1.0`，对任意输入无解；而文件里真实的 `share` 恰好就是 1.0 |
| 3 | `route2/e8_identifiability_rank_rtol.json` `column_equilibration_invariance.kappa_changes` | **`inf − inf = NaN` 吞红**：`nan > 1.0` 为 False ⇒ 门静默报「kappa 未变」，同段 note 却称「kappa 变 12 个量级」——自相矛盾 |
| 4 | `route3/q10_weight_arms.json` `ordering_matches_doc_16.1.3` | **门谓词强于正本 ⇒ 恒红，但正本结论本身成立**（订正见下） |
| 5-6 | `supp_507_relstep/eb_relstep_calibration.json` `part2a_spec_recipe_x10.spec_red` / `.mean_gt_02` | **05 规格自己的注入配方（单节点系数 ×10）不使规格变红**：`frac_gt_01=0.0357`、`mean_rel=0.0388`、`max_rel=0.901`，两判据皆 false ⇒ 缺陷注入未被检出 |

⚠ **本次只让这六条红灯变得可见，没有改任何阈值、没有把它们改绿。**
按规范 08 §5，红色不以 waiver 覆盖；按派单「不得为了让门变绿而放宽阈值」的纪律，
在门本身的缺陷修好之前，红就是正确状态。

#### 第 4 条订正：「§16.1.3」是**伪引**，且原判定的因果方向**反了**

**（a）节号不存在（伪引，已删逐字引用）**
`docs/science/PHASE2_UPM.md` 的第 16 章只有 **§16 / §16.1 / §16.2 / §16.3**
（实测 `grep -n '^#\{1,4\} 16'` → `371:## 16`、`377:### 16.1`、`389:### 16.2`、`407:### 16.3`），
**没有 §16.1.3**；全文 `grep 16\.1\.3` **零命中**。
「§16.1.3」只作为**门名字符串** `ordering_matches_doc_16.1.3` 存在于
`code/audit_rework/route3/exp10_weight_arms.py:118,152`、
`【复现：按本单元复现清单重跑后读 q10_weight_arms.json；该读数需重跑取得，当前不可离线核验】:66`、
`code/audit_rework/GATE_DISCLOSURE.json:64` 与
`docs/engineering/UNRESOLVED_REGISTER.md:2793,2797`。
该节号**自 `exp10_weight_arms.py:9` 的注释起就是错的**，且
`【复现：按本单元复现清单重跑后读 audit_rework_summary.json；该读数需重跑取得，当前不可离线核验】:90` 与 `REPORT_paper.md:100` **已自承「非正本 16.1.3」**
（原文作「非正本 16.1.3 的 13×」）。⇒ 本表此前仍把它当**文档**引用，是本表的失真。
（另：本表从未出现 `0.0245/0.0651/0.3203` 这三个数字，实测零命中；
 该组数字不出现在本文件，故不构成本表的伪引内容。）

**（b）正本实际说了什么（准确表述）**
`docs/science/PHASE2_UPM.md:384-385`，**§16.1 第 3 条**：
> **权重**：`control_ivar` 臂的伪影漏入与噪声 RMS 均为三臂最小
> （`uniform` 与 `SNR²` 两臂更大），与独立结论一致。

它只声称 **ivar 最小、uniform 与 SNR² 都更大**，即 `ivar < uniform` 且 `ivar < snr2`；
**它没有声称 `uniform` 与 `snr2` 的相对次序。**

**（c）实测：正本结论成立，门红的原因是门谓词过强（因果方向与原判定相反）**
`exp10_weight_arms.py:118-120` 的谓词是三元链
`ivar < uniform < snr2`。实测 `pollution_leak_bias`（`q10_weight_arms.json`）：

| 臂 | `pollution_leak_bias` | 正本要求 | 实测 |
|---|---|---|---|
| `ivar` | **2.6099** | 三臂最小 | ✅ 最小 |
| `snr2` | **3.0872** | > ivar | ✅ 成立 |
| `uniform` | **6.4997** | > ivar | ✅ 成立 |

⇒ **`ivar` 确为三臂最小，正本 §16.1 第 3 条的结论被实测证实。**
门为 `false` 的**唯一**原因是实测 `uniform(6.4997) > snr2(3.0872)`，不满足门额外要求的
`uniform < snr2` —— **这一条正本从未主张**。
⇒ 原判定「与文档声明的排序不符」**不成立**：不符的是**门自己的谓词**，不是正本；
本条**不是「文档错还是实现错」的待裁决分歧**，而是**门谓词写强了**。
⚠ 由此**不得**推出「该门可以改绿」——按纪律本轮**未改任何谓词**，红仍保留；
正确处置是**把谓词收窄到正本实际声称的 `ivar < uniform 且 ivar < snr2`**，
该改动属 `additive-sky-seamless` 代码面，**不在本单写入面**，登记为待办。
⚠ 同时撤回本表原措辞「报告仍当通过项引用」中的**通过**暗示：该门自身实测为 `false`，
`REPORT_paper.md:100` 实为在「非正本 16.1.3」的限定语下引用读数，
**是否构成「把红门当绿证据引用」须由该单元车道复核，本单不下结论**。
