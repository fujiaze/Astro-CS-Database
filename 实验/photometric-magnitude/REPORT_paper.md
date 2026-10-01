# 测光星等坐标系：以 Gaia XP 绝对分光刻度锚定的通量积分拟合

**单元**：`实验/photometric-magnitude`（SCI-A，任务 SCI-401；科学链第 1 点，最高设计 §2）
**性质**：定稿论文（实验重做轮融合重写）。整合三路独立重做证据（路线1/2/3，seed=20260926）与本单元历史正本实验（step1–step9，seed=20260921）；分歧裁决一律以 `实验/裁决台账.md`（D-xx）与 `docs/DISPUTES.md`（A-P1-xx）为准，UNRESOLVED 项不进正文（见 §7 诚实边界）。
**数字溯源约定**：每个关键数字标注 `[文献]`（refs.md 台账条目）、`[实验:code 文件名]` 或 `[推导]`（docs/derivation_robust_weights.md）。
**权威正本**：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001）；机器可读结果 `results/`（历史轮）与 `results/redo/`（重做轮，注明来源路线）。

---

## 摘要

将仪器计数（ADU）逐帧标定到一个可跨帧比较的星等坐标系上，是测光流水线的根基问题；当 FITS 头不携带增益、曝光与透过率信息时，标定因子的绝对值在数学上不可辨识，只有其与参考通量的比值携带科学内容。本文建立并验证一套以 Gaia DR3 XP 绝对分光采样谱锚定的测光星等坐标系：参考合成通量取 `F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ`（W·m⁻²·nm，336–1020 nm @2 nm 官方网格）[7,8]，逐星残差 `r_i = log10(F_instr,i/F_syn,i)` 经固定尺度 Tukey biweight IRLS（c = 4.685）稳健聚合成逐帧零点。常数体系三腿闭合：c = 4.685 使 biweight 位置估计量达到正态 95% 渐近效率（解析积分 ARE = 0.9499974，反解 c* = 4.6850649，与 statsmodels 4.685065 六位一致，一手锚 Kafadar 1983）[实验:code/redo/route2/exp1_robust_constants.py][5,11]；MAD→σ 常数 0.6744897501960817 = Φ⁻¹(3/4) 为解析恒等式（二分复算相对差 1.6×10⁻¹⁶）[推导:D3][实验:route2/exp1]；零点平移不变量逐位成立，标定系数因此被证明不含绝对数值窗口 [推导:D2][实验:route3/exp_S04]。误差预算判据的包络因子 1.166 = √1.361 系 MAD 尺度估计量标准化方差（1.361，Rousseeuw & Croux 1993 Table 2）的平方根 [10][推导:D5]（解释标签按分歧台账 A-P1-01 订正）。独立重做轮进一步把主系统差源量化：星等窗选择使合成零点系统平移最高 0.032 mag（比统计误差大一个量级以上）[实验:route1/exp2]；预过滤窗在 IRLS 就位后对零点精度的边际保护 ≈ 0，其真实作用面是控制进入拟合的样本族 [实验:route1/exp2]。三类数据（纯解析 Oracle、含真实 HST M16 星云结构的物理前向仿真、testdata 真实帧）的结论**并不一致**（订正 P1-B01，2026-10）：仿真与解析两类在预算完整、判据非退化下自洽且可复现（判据在真值无效应时归零、在乘性空间残差注入下单调上升并判红）；**testdata 真实帧腿在把 σ_flat 换成独立于被测样本的预算项后判 ABOVE_CEILING（σ_obs = 0.026520 mag > σ_ceiling = 0.020561 mag）**，本单元因此按审查标准 §6「三类结论一致才判定创新点成立」记**不成立（待修）**。该判红可归到「未消系统项超预算」这一层（与 L4 49 帧「PASS 1/49」同一层），但**该门对成因失明**：它不区分平场 / PSF / 颜色 / 探测器线性度中的任何一项，故**具体归因与「不是判据不可用」都超出本门能给出的证据**（见 §7 第 13、18 条）。
本单元产出的逐帧零点在链条上是 P2 绝对 SNR 的**上游输入**，这是**接口/架构关系**；本单元**未**做把本单元逐 dex 精度传播为 P2 误差底座的量化实验，故不主张「直接构成下游误差底座」这一因果量级断言（无量化传播证据，按 AGENTS §4 不以「架构上相邻」充当科学证据，G08-04 整改 E8）。引用本文读数时须同时带出摘要与 §7 第 13、18 条的限定：testdata 真实帧腿判 **ABOVE_CEILING**（σ_obs = 0.026520 mag > σ_ceiling = 0.020561 mag）、本单元按规范 03 §3「三类结论一致才判定创新点成立」记**不成立（待修）**、且该门对成因失明。

---

## 1 引言

### 1.1 问题定位

测光标定的经典做法依赖仪器元数据（增益、曝光时间、有效口径、光学与大气透过率）将计数换算为物理通量。在无元数据可依赖的观测链上，可观测量的结构是

```
I_cal = (g · t · A_eff · η · …) · ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ + 噪声
```

其中括号内各因子与通带归一常数只有乘积可辨识 [推导]（`docs/science/PHOTOMETRY.md` §1、§16.2）。因此标定因子 `k_photo` 的绝对值无物理意义——它吸收全部未知仪器常数——而可证伪的科学内容转移到两个尺度无关的量上：**逐帧零点** `k_photo` 相对参考刻度的位置，与**帧内测光一致性** `sigma_residual`。

### 1.2 前置约束

- **禁止物理闭合反推**：不得由 `k_photo` 反解增益/口径/曝光，不得为其设绝对数值窗口（`PHOTOMETRY.md` §10、§16.3）。
- **禁止跨帧一致性门**：门只有一个 = 单帧标定是否可信；帧间一致性是报告字段而非门禁（`PHOT-GATE-DROP-001`）。
- **判据非退化**：真值无效应时度量必须归零或判红；恒真门没有证据资格。

### 1.3 相关工作定位

Tukey biweight 位置估计与其 IRLS 解法源自 Beaton & Tukey (1974) [2] 与 Holland & Welsch (1977) 的权重常数表 [4]；c = 4.685 与正态 95% 渐近效率的显式联系由 Kafadar (1983) 给出 [5]（台账 D-06 判该锚有效）。MAD 尺度估计量的 37% 高斯效率与标准化方差 1.361 由 Rousseeuw & Croux (1993) 建立 [10]。Gaia DR3 XP 外定标采样谱的物理刻度、通带定义与 ±2% 外定标精度见官方文档与 Montegriffo et al. (2023) [6,7,8]。反方差加权估计的经典框架为 Aitken (1935) [1]——本项目统一采用反方差口径；本单元 IRLS 使用的是稳健权（§2.2），反方差口径经量纲变换 `ivar′ = ivar/α²` 进入下游（§3 链条位置）。

---

## 2 方法

### 2.1 参考合成通量

```
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ        # 单位 W·m⁻²·nm；不含 10^(−0.4·G)
F_λ(λ_i) = byte_i·flux_mul + flux_min   # 绝对谱辐照度 W·m⁻²·nm⁻¹（XPSD 量化解码）
```

通带 = 滤镜透过率 T 与探测器 QE Q 的组合 [7 正文原句]；λ 网格为官方 343 点 @2 nm（336–1020 nm）[8 §20.12.4]；插值 Akima 子样条（区间外填 0）[13]，求积复合 Simpson 1/3（末尾奇数区间 3/8，`n==1` 退梯形）。**退化分支（订正 P1-m02）**：`n_int == 3`（4 点）时前段 1/3 的区间数 `n_13 = n_int − 3 = 0`，**1/3 部分必须为 0**——历史实现在该分支把 `y[0]` 计入两次，常数被积函数得 3.6667 vs 解析真值 3.0（+22.2%）。生产端（`lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp`）与实验参考实现（`code/scia_common.py::simpson_integrate`）已按变更 claim `PHOT-SIMPSON-N3-001` 同步订正，`n_int ∈ {3}` 的失效域已由 `p1phot` O3 组的闭式期望（常数 3.0、三次式 6.75、n_int=5 组合分支 12.5）与故障注入名 `o3_simpson_n3_reference` 锁定。官方 343 点 XPSD 网格 `n_int = 342` 为偶，不触发该分支。历史上本单元曾把 `×10^(−0.4·magG)` 写成"冻结约定"；该写法不成立——官方定义式带 λ、不带任何星等因子 [8 式 5.41]，逐星乘 `10^(−0.4·G_i)` 会给 `r_i` 注入 +0.4·G_i dex 的加性项，单标量零点吸收不掉 [实验:docs/fsyn_convention.md 负例，dlog10(ratio)/dG = 0.400]。生产实现与订正后公式逐位一致（两真实帧零点复算差 9.5×10⁻¹³ / 4.3×10⁻¹¹）[实验:docs/fsyn_convention.md §2.3]。完整判定见 `docs/fsyn_convention.md`（历史订正记录，保留不改，本节在其上引用）。**工程配套（负责人已批）**：插值/求积设置配置化，运行日志输出不落盘。

### 2.2 逐星残差与稳健零点

```
r_i   = log10(F_instr,i / F_syn,i)                    # dex
S     = MAD(r)/0.6744897501960817,  c = 4.685,  tol = 1e-6,  max_iter = 50
u_i   = (r_i − location)/(c·S);  w_i = (1−u_i²)²·[|u_i|<1]
location = Σ w_i·r_i / Σ w_i;   k_photo = 10^(−location)
sigma_residual = MAD(r_inliers)/0.6744897501960817;   sigma_obs = 2.5·sigma_residual
```

预筛（**订正 P1-M03**：量纲显式、与实现一致 [推导:D1]）：`|delta_i − median(delta)| ≤ 3.0 mag`，其中 `delta_i := −2.5·log10 F_instr,i − G_i`（G 已在该定义内减去一次）。实现 = `code/scia_calib.py:123-126`（阈值 3.0 mag）。历史写法 `|r − median(r)| ≤ 3.0`（r 为 dex）与之同句注「= 1.2 dex」自相矛盾，**已订正**。
**该窗与 `|r_i − median(r)| ≤ 1.2 dex` 不等价**（**订正 P1-M03-b**，G08-04 G2 独立复核推翻前一轮的等价判词）：代数上确有 `delta_i = −2.5·r_i − C_i`、`C_i ≡ G_i + 2.5·log10 F_syn,i`，但 `C_i` 是**逐星量、不是常数**——生产 `F_syn,i` 是逐星 SED 积分 `I_i = ∫F_λ,i·T(λ)·Q(λ)·λ dλ`（`code/scia_common.py:205-233`），随每颗星自己的 XP 谱形与色变化（`10^(−0.4·m)` 只是**额外相乘**的星等指派重标度，不属参考通量定义；本单元正本 `docs/fsyn_convention.md` §1 判「**不含 `10^(−0.4·G)`，`G` 既不进入 `F_syn`**」）。两窗的中心与展宽因此不同源，**不等价**。`3.0/2.5 = 1.2` 只是**阈值换算**（`3.0 mag` 若以 dex 书写则数值为 1.2），**不是**两窗的等价性陈述；等价性成立的前提是 `F_syn,i` 只依赖 `G_i`（`C_i` 退化为星无关常数），该前提不成立。

**权重语义分立**（按负责人已批的反方差口径条款）：IRLS 的 w_i 是稳健权，吸收错配与污染、服务无偏性，不是反方差权；反方差加权（Aitken 1935 [1]）在本单元体现为施加链的量纲变换 `x′ = α·x ⇒ Var′ = α²·Var ⇒ ivar′ = ivar/α²`（α = k_photo·m(x,y)），交下游 P5 消费。两族权重不得混写 [推导:D7]。

**求解器设置的豁免依据**：tol/max_iter 是数值收敛档而非物理量——tol 自 1e-2 收紧至 1e-12，中位 location 变化仅 ~2×10⁻⁴ dex，50 步上限最多 17 步即达 [实验:code/redo/route2/exp1_robust_constants.py]；登记为"求解器设置 + 不变性实验为凭"（豁免清单见实验报告）。

### 2.3 双边界判据

```
n             = 本帧匹配星数（Gaia 匹配 ∧ 有效域 ∧ IRLS inlier）
sigma_floor   = (1 − 3·1.166/√n)·sigma_fit(白; 本帧匹配星通量分布)
sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)
PASS ⟺ sigma_floor ≤ sigma_obs ≤ sigma_ceiling
```

预算项各计一次：光子噪声、PSF 拟合不确定度（精确 Fisher，含自由背景简并项）、平场残余、天光扣除残余、颜色项、参考侧、量化；仿真帧上逐项由本帧推导，真实帧上 σ_color/σ_gaia 不可自算、如实标 `null`（上界不完整）。**仿真帧的 `σ_gaia = 0.002 mag` 未被激活（订正 P1-m04）**：注入与模型共用同一 `mag_eff`，参考侧扰动在 `r_i` 中精确相消（复算相对差 0）⇒ 该项在仿真上不构成可检验的误差源，其计入使上界**偏松**；不影响本单元任何判红的结论（偏松方向仍判红）。
**σ_flat 的独立性与取值口径（订正 P1-B01）**：σ_flat 是**平场/大尺度响应残差**的物理预算项，**不是**被测样本在多项式拟合**之后**的残差散度。历史实现用 `calibrate()['delta_after_m']` 充当 σ_flat——那是 `σ_obs` 自身的函数（自指），使上界随被测统计量一起膨胀；把它换成独立项后同一真实帧由 PASS 翻为 ABOVE_CEILING。现约定：①仿真帧取**真值**（注入的已知平场残差）折算；②真实帧取**仓内约定常数** `σ_flat,hf = 0.0007 mag`（活出处为 `code/scia_calib.py:31` 的字面量；**一手出处至今未取得**，见 §7 第 16 条——它被归给的 `docs/detail/registry/astrocs.phase1.photometry.md`「数值落地口径」（内的「测光一致性判据（单帧、尺度无关、双边界）」 与 `docs/science/PHOTOMETRY.md` §16.4 的对应行在当前树均不存在，故本单元不得称其为权威常数）；③`delta_after_m` 只作**诊断字段**输出，**不得**再进入 `σ_ceiling`。
**判据作用域与状态词（订正 P1-M07）**：`rho_lo = 1 − 3·1.166/√n ≤ 0` ⟺ `n ≤ 12.236` 时下包络在数学上不存在，作用域降级为 `upper_only`、状态词返回 `LOWER_BOUND_UNDEFINED`（不记 PASS）；**禁止**用 `max(rho_lo, 0)` 夹逼（那会让下界恒不触发，属恒真门）。**因子标签订正**：1.166 = √1.361，其中 1.361 是 MAD 尺度估计量在高斯数据下的渐近标准化方差 [10 Table 2]——即 1.166 是 σ̂ 的相对标准差因子，`3·1.166/√n` 为"σ 估计量自身抽样涨落"的 3σ 包络（**按分歧台账 A-P1-01 订正**；历史正本"1.166 = SD(MAD)/MAD"的标签不准确）。判据形态逐字来源 `docs/detail/registry/astrocs.phase1.photometry.md`「数值落地口径」（内的「测光一致性判据（单帧、尺度无关、双边界）」。

### 2.4 施加与降级

`I_photo = k_photo·m(x,y)·I_cal`；未启用时写显式 `degraded_reason` 且 fail-closed。门①（n<3）触发 ⇒ NO_DATA、scale=1.0/σ=0/fit_used=0，下游按未标定分支处理。

---

## 3 【链条位置】

**上游输入（谁给什么）**：

| 输入 | 口径 | 来源 |
|---|---|---|
| `F_instr` [ADU] | PSF 域解析通量；5×5 盒和与 N-25 口径禁止 | 前级检测/测光 |
| `F_syn` [W·m⁻²·nm] | ∫F_λ·T·Q·λ dλ；XP 336–1020 nm @2 nm；域限制**两级、不得合并成一句**（订正 P1-M02）[9]：①**采样表示** `xp_sampled_mean_spectrum`（本单元 `*.xpsd` 解码所需那一支）的额外子集以 **G = 15 mag** 为界；②**连续表示**到 **G < 17.65**。本单元 M16 锥 208 源中 G<15 仅 58 源、G ≥ 17.65 有 6 源 ⇒ 样本跨越采样表示的子集界，绝对刻度改由实测锚定（`PHOTOMETRY.md` §16.5 第 7 条：median 偏差 −0.0037 mag、MAD 0.0033，n=11272） | `spectrum_integrator` |
| `G_Gaia` | 仅进 delta/ZP 诊断，不入 F_syn | Gaia DR3 |
| 质量位 | 饱和预排除 | 探测级 |

**P1 产出（下游消费）**：

| 产出 | 定义 | 消费方 |
|---|---|---|
| `k_photo = 10^(−location)` | [F_syn 单位]/ADU；绝对值无物理意义 | P3/P4 施加链 `I_photo = k_photo·m·I_cal` |
| `sigma_residual` [dex] / `sigma_mag` = 2.5·sigma_residual [mag] | 单帧测光一致性 | P2 零点统计误差 = 1.253·sigma_residual/√N [推导:D6] |
| `ZP_syn`、`zero_point_scatter_mag` | 绝对合成星等零点 | P2 `m_5 = ZP − 2.5·log10(5·σ_F)` |
| `ivar′ = ivar/α²`（α = k_photo·m） | 反方差口径的量纲变换 | P5 SNR 加权 |

**精度约定**：①r_i（dex）与 delta_i（mag）不共用容差 [推导:D1]；②`sigma_residual = 0 ∧ fit_ok = true` 的帧不可估计——完美控制点须按非完美处理，P4 不得以其为真值锚 [实验:route3/exp_S05]；③门①（n<3）⇒ NO_DATA 分支，且 n=3 时 sigma_mag 系统性下偏（MAD 有限样本偏差 1.49×；**订正 P1-M04**：渐近式 1.2533/√n 在 n=3 处对 SE **高估 8.03%**——精确 SE = 0.6698291607404144σ vs 渐近 0.7235930923753581σ，`0.7235930923753581/0.6698291607404145 − 1 = 0.08028`；历史写的 7.4% 是 `1 − 0.9256987` 的误读）[实验:route3/exp_S11]——P2 的低星数帧零点不确定度被低估；④ZP_syn 三星下限的抽样误差是 n≈2338 满载的 25.7 倍 [实验:route3/exp_S11]。**σ_ZP 公式与 σ_star 的统一（订正 P1-M05）**：两个消费面共用一式 `σ_ZP(N) = 1.2533·σ_star/√N`，`σ_star` 随输入域带单位（dex→dex、mag→mag）。用于早停阈论证的 σ_ZP(N) 曲线（`code/redo/route3/exp_S08_ladder.py → H8b`）取 `σ_star = 0.05 mag ≡ 0.02 dex`（route3 H8b 自洽）；按该式 **σ_ZP(5000) = 8.86×10⁻⁴ mag**。历史读数 **6.36×10⁻⁴ mag** 出自 route1 的变体 `0.045 mag/√N`（缺 1.2533 中位数因子、且 σ_star 取 0.045 而非 0.05）⇒ 相对低估 **1.39×**，**已订正**。统一后 N=5000 的 σ_ZP 仍 ≪ 逐星散度 0.05 mag（≈56×）与 A6-GATE 门宽（~0.012–0.042 mag），故 `max_stars = 5000` 作为「σ_ZP(N) 曲线支撑的工程上限」的论证**方向不变**（口径一致性门的红/绿双态见 `exp_S08 → H8d`：统一式 PASS、旧变体 RED）。

**极限星等派生估计：本单元未验证（订正 P1-m12）**：上游设计（`docs/ASTROCS_DESIGN.md` §4.2、`docs/detail/registry/astrocs.phase1.star-detection.md`「数值落地口径与实现落点」、`docs/science/PHOTOMETRY.md` §16.4①）写「极限星等按焦距、画幅、曝光时间派生估计（宁多勿少）」。该句是**设计意图**：本单元**没有**任何实验锚定该派生链，且本单元复跑记录的实锚是生产侧的**固定星等阶梯 {12,13,14,15,16}**（`pc_api.cpp:291/297`、`:708/714`、`:978/984`），其注释宣称的「上界 10000」**未实现**，拥挤场 `n_gaia` 可远超该值（[实验:code/redo/route1/exp3，H6]）。⇒ 引用该句时必须写成「设计意图、本单元未验证」，不得当作已实现的机制。

**跨单元归属说明**：`k_corr` 冻结常数 1.4 改两因子几何查表（台账 D-08，负责人已批）由 P3/P5 单元承载，本单元不涉。

---

## 4 数据

三类数据相互佐证（最高设计 §12.2），复现代码与 seed 见 §6：

1. **纯解析代数合成**（Oracle）：600 星，SED 取真实 Gaia XP 谱；无噪声组 + 多噪声组；`m(x,y)` 解析已知。检验实现正确性、积分约定、IRLS 稳健性与负例。[实验:code/step1_analytic.py，seed 20260921]
2. **物理前向仿真**（真实信号模板）：HST M16 F657N HLSP drz 真实观测结构 24×24 分块平均（~0.95″/px），注入星取真实 XP SED；逐像素 Poisson（源+天光+暗流）→读出→增益→饱和→量化。检验真实结构下判据行为。[实验:code/step2_hst_sim.py]
3. **testdata 真实帧**：FLI M42 M1 T2 Red 300 s（4096²，uint16+BZERO）。检验帧内参数、WCS 二轮精化与引导检测的可用性；无真值，作底参照。[实验:code/step8_real_frame.py]

外部交叉核对：SVO HST/WFC3_UVIS2 通带曲线 + 头部 PHOTFLAM/PHOTPLAM；GaiaXPy 官方实现 [12]。

---

## 5 结果

### 5.1 常数体系三腿闭合（重做轮核心产出）

| 常数 | 读数 | 三腿 |
|---|---|---|
| `c = 4.685` | ARE = 0.9499974；反解 c* = 4.6850649 ≡ statsmodels 4.685065（六位） | [5,11][实验:route2/exp1][推导:D4]（订正 P1-m05：数值锚只归 route2；route3 `exp_S01` 自身数值根 4.6853179 仅 4 位小数一致，不作数值锚） |
| `0.6744897501960817` | = Φ⁻¹(3/4)，二分复算相对差 1.6×10⁻¹⁶ | [推导:D3][实验:route2/exp1, route3/exp_S02] |
| `1.482602218505602` | 倒数恒等式；**发现** `frame_photometry_fit.cpp:292` 曾用 4 位截断 1.4826（相对差 1.496×10⁻⁶），违反 `NOISE_ESTIMATION.md:213` 冻结条款；**已闭环 P1-m01**：HEAD 现为全精度字面量 `1.482602218505602`（修于 `8f15c9fc`，`git log -S 1.482602218505602` 可追） | [推导:D3][实验:route2/exp1]（订正项 A-P1-08 **已闭环**） |
| `1.166` | = √1.361（MAD 标准化方差，R&C 1993 Table 2）；路线1 推导腿闭合 √1.361 = 1.1666；MC 渐近 1.170（+0.3% 登记） | [10][实验:route1/exp1][推导:D5]（标签订正 A-P1-01） |
| 平移不变量 | F_instr 整体乘常数 ⇒ location 平移精确、内点集逐元素相同 | [推导:D2][实验:route3/exp_S04, route2/exp2 负例] |

**锚体系核验**（重做轮对审查"幻觉锚"指控的独立复核）：路线3 逐锚抽验 15/15 真实 [实验:route3/exp_S00]；路线2 复核 24 处科学量锚，唯一判"内容不符"者为 PMC6768164——经台账 D-06 直验原文（含 c=4.685 原句）判**锚有效**，误判撤回。三处定位偏差（PHOTOMETRY.md:126/:400 等）按台账 A-P1-04/09 改记"行号漂移"而非内容伪造：正本在 :13/:15–17。Gaia DR3 的 arXiv 号自纠为 2208.00211（原引 2205.11321 证伪）[3]。

### 5.2 Oracle 与三类数据上的判据行为（历史正本轮，seed 20260921）

- Oracle（`m≡1`）：估计器与真值一致到机器精度（rtol = 0.0，k 相对误差 3.33×10⁻¹⁵）[实验:code/step1_analytic.py]。
- **仿真腿**：三帧物理前向仿真全 PASS，σ_obs = 0.045344 / 0.057457 / 0.051718 mag，观测/预测总项比 1.154 / 1.008 / 0.417（`results/step5_calibration_gate.json → obs_over_predicted`） [实验:code/step2_hst_sim.py, step5_calibration_gate.py]。这三帧的 σ_flat 取**注入真值**（独立于被测样本），且帧 A/B 用裸通量中位统计（**不**依赖 `m(x,y)`，见 `step5_calibration_gate.py` 作者约定），故该 PASS 不受 P1-B01 自指缺陷影响。
- **真实帧腿（订正 P1-B01，判红）**：testdata M42 帧 n=157、inlier=151，σ_obs = **0.026520** mag，σ_floor = 0.006008 mag。
  历史实现把 `calibrate()['delta_after_m']`（= 被测样本多项式拟合**后**的残差散度，σ_obs 自身的函数）当作 σ_flat = 0.019561 mag，
  占上界方差 47.6%，得 σ_ceiling = 0.028366 ⇒ PASS（自指上界，见 §7 订正项 5）。
  改用独立项后取仓内约定常数 σ_flat,hf = **0.0007 mag** ⇒ σ_ceiling = **0.020561** mag；
  σ_obs = 0.026520 **> σ_ceiling ⇒ ABOVE_CEILING（判红）**。翻转临界 σ_flat = **0.013057** mag（= 该约定值的 **18.7 倍**）
  [实验:code/step8_real_frame.py, results/step8_real_frame.json]。
  取仿真口径（σ_flat = 0.000881 mag，同帧族真值折算）时 σ_ceiling 略大（0.0205733 vs 0.0205608 mag），判定同为红 ⇒ 结论对该口径选择不敏感。
- 负例（判据非退化）：真值无效应 ⇒ sigma_obs = 0 且判红；乘性空间残差注入 ⇒ sigma_obs 单调上升并判红；散粒噪声敏感（Poisson 天光抬升 2.91×，4× 判红）而确定性加性图样不敏感（1.08×）⇒ 判据**不能认证天光扣除质量** [实验:code/step7_negatives.py]。
- 低样本失效模式（**订正 P1-M07**）：`rho_lo = 1 − 3·1.166/√n ≤ 0` ⟺ `n ≤ 12.236` 时下包络**不存在**（n = 5 实测 σ_floor = −0.004911）。历史按 `max(rho_lo, 0)·σ_fit` 夹逼 ⇒ 下界恒 0 而 σ_obs ≥ 0 恒真 ⇒ **过裁剪样本反而 PASS，属恒真门**。现改显式最小样本规则：作用域降级 `upper_only`、状态词 `LOWER_BOUND_UNDEFINED`（**不记 PASS**）⇒ 对「把样本裁到只剩同质星」的判别力来自上界与状态词 [实验:code/step7_negatives.py → N5, code/scia_common.py][推导:D5]。
- **三类数据不一致 ⇒ 创新点成立性判定（按审查标准 §6）**：仿真腿与解析腿绿、真实帧腿红 ⇒ 记 **不成立（待修）**。真实帧判红可归到「未消系统项超预算」这一层（与 L4「PASS 1/49」同一层），但**具体成因不可归因**——双边界门对亮度依赖与位置依赖残差给出门不可分辨的读数（§7 第 18 条）。

### 5.3 系统差源量化（重做轮）

- **星等窗 → ZP_syn 系统平移**：窗 {6–16, 6–12, 12–16, 10–15} 实测平移 0 / +0.0153 / **−0.0318** / −0.0144 mag [实验:route1/exp2]——比零点统计不确定度（~10⁻³ mag）大 30 倍，与 XP 外定标 ±2% ≈ 0.022 mag [6] 同量级。两个消费面的星族口径分裂是可量化的系统差源，必须统一或如实标注。
- **预过滤窗的真实作用面**：在 IRLS/Tukey 就位后，`mag_tolerance = 3.0` 对 ZP 偏差的边际保护 ≈ 0（实测 −7.3×10⁻⁶~0 dex）；其真实作用是控制进入拟合的样本族（保护尺度初值 S 不被错配污染）[实验:route1/exp2]。文献腿登记为项目冻结值（正本明言不引文献背书）。
- **FOV 三常数的危害量化**：缓冲 1.2 与钳位界 1.0/10.0 的优先语义冲突实测确认（0.3″/px 全幅被上抬 ×3.02，10″/px 广角被压低 ×0.69）；错配对经预过滤 + IRLS 后 location 系统偏移 −0.146 dex 且 `|r_consistent| ≥ 3` 门可被满足 ⇒ 奇协方差阵下拟合照常产出错误标度 [实验:route1/exp3]。按台账 A-P1-12，`05 A-4b` 须写明缓冲与钳位的优先语义。
- **1.0 dex 粗筛界非恒真**：干净/污染场通过率 0.055/1.110 对照，能红 [实验:route3/exp_S12]；空间增益阶数（order ≤ 2 + 六降级门槛）确认为科学量而非结构性选择 [实验:route1/exp4 独立复核]。
- **匹配半径**：2.0 px 最优档正确率 ≥ 0.996，歧义率 ≈ r² 理论 [实验:route1/exp2, route3/exp_S10]；全档敏感性表（0.5–4.0 px）支撑其为实现选择。

### 5.4 端到端贯穿用例

路线2 用例（2000 星，seed 20260926）：上游给 XP 型谱与 G ⇒ P1 产出 `ZP_syn = 28.9358 mag`、`k_photo = 1.00156` [F_syn]/ADU、`sigma_residual = 0.0493 dex` ⇒ 下游 `sigma_kappa,stat = 1.2533·sigma_residual/√N = 0.00138 dex`（**订正 P1-M05**：与早停阈论证共用一式 `σ_ZP(N) = 1.2533·σ_star/√N`，单位随输入域；本帧 σ_star = 0.0493 dex ⇒ 0.00138 dex）、`m_5 = 27.19 mag` [实验:route2/exp8]。路线1 用例（800 星）：scale 真值 3.7×10⁶ 恢复为 3.7053×10⁶（相对误差 1.44×10⁻³，与 σ/√800 量级一致）[实验:route1/exp2]。

### 5.5 引导检测的工程结论（H2）

星表引导检测把匹配率从盲检的 0.2254/0.1972 提升到 0.9859；真实 4096² 帧上盲检 13163 个检出仅 1.49% 对应星表星、逐源拟合需 30.3× 拟合次数。前提是二轮 WCS 平移精化收敛到 ≲2 px（HST HLSP drz 头部与 Gaia 存在 ~1.6″ 系统偏移，testdata 真实帧仅 0.0068″）[实验:code/step4_guided_vs_blind.py]。诚实边界：仿真中引导匹配的定位输入即注入真值（结构性循环），非循环证据来自真实帧。

---

## 6 复现

**固定 seed**：历史正本轮 `20260921`（`scia_common.rng(tag)` SHA256 派生）；重做三路统一 `20260926`（各脚本写死，路线1 派生 SEED+1…）。

```bash
cd <repo root>
bash 实验/photometric-magnitude/code/run_all.sh          # 历史正本轮 step0–step9（quick 跳过 step6）
bash 实验/photometric-magnitude/code/redo/run_all.sh     # 重做三路 route1–route3（结果基准快照 results/redo/）
```

单脚本均为纯 Python + numpy，不 import 仓库任何 Python；重做轮每脚本 CPU ≪ 5 min。基准读数快照：`results/step1..step8*.json`、`results/redo/route{1,2,3}/`（注明来源路线的汇总在 `results/redo_summary.json`）。

---

## 7 诚实边界

1. **已证与未证的边界**：本单元证明的是"标定系数无绝对窗口且不可反解仪器参数"（退化族 `A·t/g` 同统计 ⇒ 不可分；平移不变量机器精度级成立），**没有**证明更强的"物理单位在全帧被消除"。
2. **绝对刻度核对范围**：窄带（等效宽度 29–39 nm）XP 绝对刻度与 HST PHOTFLAM 中位差 −0.147/−0.263/−0.080 mag [实验:code/step3_forward_vs_photflam.py]；**宽带未测**。
3. **判据能力边界**：对散粒噪声敏感、对确定性加性图样不敏感 ⇒ 不能认证天光扣除质量；真实帧上 σ_color/σ_gaia 不可自算 ⇒ 上界不完整。
4. **低样本域**：`rho_lo ≤ 0` ⟺ `n ≤ 12.236` 下包络不存在（历史用 `max(rho_lo,0)` 夹逼等于把下界做成恒真门，已按 P1-M07 撤回）；n=3 帧零点不确定度被低估（MAD 有限样本偏差 1.49×，渐近式高估 SE 8.03%）——下游消费约定见 §3。
5. **σ_flat 自指（P1-B01，已订正）**：历史把 `delta_after_m`（被测样本拟合后残差）当预算项，使上界随被测统计量膨胀；**已订正**为独立项（仿真取注入真值、真实帧取仓内约定常数 `σ_flat,hf = 0.0007 mag`，其一手出处未取得，见第 16 条），`delta_after_m` 降级为诊断字段。该订正使真实帧判定由 PASS 翻为 **ABOVE_CEILING**——本单元**不再声称**真实帧残差落在本帧预算内。**残余不确定度（如实）**：0.0007 mag 是 L4 高空间频项的**折算**口径，其向本单元逐星预算的换算链**未独立核证**；即便取仿真口径（0.000881 mag）判定同样为红，故结论对该换算不敏感。**归因边界**：该判红只能归到「未消系统项超预算」这一层，门对具体成因失明（见第 18 条）。
6. **循环性**：仿真中引导匹配的定位输入即注入真值；非循环证据来自真实帧。
7. **项目约定（不注文献出处）**：FOV 三常数（缓冲 1.2/钳位 1.0/10.0）、自适应阶梯 {12..16}/2000/5、mag_min 亮端、1.0 dex 界、匹配半径 2.0 px、max_stars=5000（σ_ZP(N) 曲线支撑的工程上限，统一式下 σ_ZP(5000) = 8.86×10⁻⁴ mag，见 §3）均为项目约定，敏感性与危害量化已给出；**注意**：本单元**未**验证「极限星等按焦距/画幅/曝光派生估计」这一上游设计意图（实锚是固定阶梯 {12..16}，注释宣称的上界 10000 未实现，见 §3）；`quality_factor` 0.1/0.5 的豁免（负责人已批，文档写明"项目约定，不注文献出处"）由 P5 单元承载。
8. **条款一致性缺陷**：`frame_photometry_fit.cpp:292` 的 1.4826 四位截断（A-P1-08）**已闭环**（HEAD 为全精度字面量）；`mag_max_arr` 长度与 i<5 循环上限耦合使第 6 档改动静默失效（A-P1-08）**仍开**；B13 接线整改（门语义换轨，路线1 S15）**仍开**。均为文档/实现整改项，不改判据方向。
9. **文献边界**：Beaton & Tukey 1974（V3，原文无 OA 全文）与 Aitken 1935（V11）只能作**二手归属**，不得声称已核原文；Croux & Rousseeuw 1992/1993 有限样本表值原文未取得（MC 佐证 ≤0.93%），不列 VERIFIED，作"待补"脚注；Huber & Ronchetti 2009 §6.5 页码未核，正文改引 Kafadar 1983；Lindegren 2021 亮端数字仍开放（不阻成稿）。**已订正**：V4 期号 6(8)→**6(9)**、V8 题名+DOI、V10 适用域改成**两级域**（原文出处 Montegriffo 2023 附录 B）、V2 路径 `robust/_tables.py`、V9 官方文档永久链接（`Data_analysis/…` 路径已 404，改为 `Data_processing/…`）与 1% 句的节号（§5.4.1，非 §5.4.2）、V5 Rousseeuw & Croux 1993 全文与 Table 2 值本轮未能独立复核。
10. **σ_flat 口径换算链**：见第 5、16 条——0.0007 mag 的一手出处未取得，其向逐星预算的换算也未独立核证；判定对该换算不敏感（两种独立口径同判红）。
11. **Red-team 审查标准 §7「恒真门无证据资格」的自查（P1-M07 家族）**：本单元已按该条撤回/降级 4 处退化判据——①低样本下界 `max(rho_lo,0)·σ_fit`（现 `LOWER_BOUND_UNDEFINED`，不记 PASS）；②`exp_S06` 的 `H6a/H6b`（对 F_syn 量纲与星等因子的**自洽**检查，非可证伪门）；③`exp_S02/exp_S11` 的 `negative_zero_check`（常数序列 ⇒ 度量恒 0，**无判别力**）；④N5 的历史 PASS 语义。②③的现有 JSON 字段保留作实现守卫，但**不得**再作为证据引用，已在对应报告文中标注。
12. **本轮订正的证据边界（如实）**：真实帧 σ_obs = 0.026520 mag 与 σ_psfsys 的**独立复算**由审查员完成（`run/FINAL-07/审核包/科研审查/evidence-P1/`）；订正者复用的是同一代码路径的**重跑读数**，两者相对差 ≤1.7×10⁻⁷。σ_floor 依赖「白噪星通量分布」的 σ_fit 口径，其逐星换算同样未独立核证。
13. **探测器线性适用域未测（乘性假设的前提缺失）**：`I_photo = k_photo·m(x,y)·I_cal` 的乘性假设要求探测器工作在线性区，这是把 `r_i = log10(F_instr/F_syn)` 的对数变成加性零点的**唯一前提**。本单元的代码里只有**饱和硬门**（`saturation_adu = 65535`，`scia_sim.py:75`、`step5_calibration_gate.py:63`、`step7_negatives.py:48`、`step8_real_frame.py:112`），**不拦「未饱和但已进入非线性区」的像元**，也不检查任何电子数上限；全单元源码没有「线性化 / 非线性 / 线性区」的任何量。**「线性化残差」不在 `σ_ceiling` 的任何一项里**：亮度依赖的乘性残差（电荷转移低效、放大器辉光、前置放大器压缩）在远未饱和的电子数下就使 `F_instr ∝ 真实通量` 失配，且失配随亮度单调变化，实测可把 `sigma_obs` 从 0.00220 抬到 0.01216（见第 18 条）。⇒ **进入非线性区的电子数阈值未测**，在该阈值落地之前，本单元**不宣称**真实帧判红的成因已被排除，也不宣称 `sigma_ceiling` 已含探测器线性度项。
14. **σ_gaia 的仿真取值自参照，且比正本口径乐观 5.4 倍**：仿真帧取 `σ_gaia = 0.002 mag`（`step5_calibration_gate.py:111`、`step7_negatives.py:69` 的硬编码），恰等于仿真注入的参考侧误差 `ref_err_mag = 0.002`（`scia_sim.py:93`）⇒ **预算项 = 被注入真值**，自洽但自参照。Gaia DR3 XP 绝对刻度上限 1% ⇒ `−2.5·log10(1.01) = 0.0109 mag`，是 0.002 的 **5.4 倍**；把它换成 0.0109 时帧 A 的 `σ_ceiling` 由 0.048473 升到 0.049646（+2.4%，判定不翻转）。⇒ 仿真腿三帧 PASS 是在「参考侧系统差 0.185%」这一**比正本乐观 5.4 倍**的条件下条件性成立的。真实帧 `σ_gaia = null` 不可自算，上界不完整。
15. **σ_psfsys 的孔径口径是本实验约定而非生产口径**：实验用 `r = 4 px`（`step8_real_frame.py:146` 的 `aperture_flux(..., r_ap=4.0, r_in=6.0, r_out=10.0)`），生产定义是 `r = 10 px`。同帧实测 `sigma_psfsys_ap10_inframe = 0.2574` vs `r=4` 的 `0.0250`，**10.3 倍**；真实帧**只**跑 r=4。属口径选择而非缺陷，但在读 `σ_ceiling` 时须按 r=4 口径读。
16. **σ_flat = 0.0007 mag 的一手出处至今未取得（它单独决定真实帧判红方向）**：该值由 `scia_calib.py:31` 的 `SIGMA_FLAT_HF_CANONICAL = 0.0007` 活着，被本单元多份文档归给 `docs/detail/registry/astrocs.phase1.photometry.md`「数值落地口径」（内的「测光一致性判据（单帧、尺度无关、双边界）」，并称「落在 `docs/science/PHOTOMETRY.md` §16.4 的同一行」。**该路径与该行在当前树均不存在**（`docs/science/PHOTOMETRY.md` §16.4 只有 `σ_flat,hf` 字样、无 0.0007）。它单独决定真实帧判红方向（`σ_ceiling = 0.020561` 由它定，翻转临界值 0.013057 = 其 18.7 倍）⇒ 按 AGENTS §4「`我觉得`/`惯例如此`不算证据」，该值在本单元内**只能作仓内约定常数引用，不得称权威常数**，且其向逐星预算的折算链亦未独立核证（第 5 条）。
17. **双边界判据在低样本域与尺度估计上双重偏松**：`n ≤ 12.236` 时 `rho_lo ≤ 0`，作用域降级为 `upper_only`（下界**不存在**，只报 `LOWER_BOUND_UNDEFINED`）；同时 `sigma_obs` 由 MAD 型尺度估计量给出，该估计在 n=3 处的**期望值只有 0.67σ**（缺损因子 1.49），在被测统计量本身系统性偏低的方向上再偏一次。⇒ 双重偏松：只剩上界，而上界一侧的被测量偏低。降级条件**只按 n 判**，对「样本量够但残差场被单一时变结构支配」的情形不充分（见第 18 条）。
18. **双边界门对未建模乘性残差「可见但成因失明」，且下包络在自称有效的样本域内可被推翻**：`sigma_obs_mag` 是 `m(x,y)` **之前**的统计量（`scia_calib.py` 中 `out.update(..., sigma_obs_mag = 2.5*sigma_residual)` 位于 `if m_degree > 0` 分支之前），`m(x,y)` 只改诊断字段 `delta_after_m`。实测（`code/redteam/rt_gate_attribution.py`，n = 48 ≫ 12.236，星等 12–20，逐星测光噪声取本单元 `noise_sigma_mag` 口径，`sigma_floor = 0.01731`、`sigma_ceiling = 0.03097`）：

    | 注入的乘性残差 | 幅度 `a` | `sigma_obs` [mag] | `m_degree=0` 与 `=2` 逐位相同？ | 判定 |
    |---|---:|---:|:--:|---|
    | 无（基线） | 0 | 0.00220 | 是 | `BELOW_FLOOR` |
    | 亮度依赖 `a·(F/F_max)` | 0.02 | 0.00874 | 是 | `BELOW_FLOOR` |
    | 亮度依赖 | 0.20 | 0.01826 | 是 | **`PASS`** |
    | 位置依赖 `a·(x−x̄)/σ_x` | 0.02 | 0.02884 | 是 | **`PASS`** |
    | 位置依赖 | 0.03 | 0.04057 | 是 | **`ABOVE_CEILING`** |
    | 位置依赖 | 0.05 | 0.06892 | 是 | `ABOVE_CEILING` |

    三条读数：(i) **可见**——未建模的乘性残差把 `sigma_obs` 单调抬高，门看得见；(ii) **成因失明**——位置依赖残差下 `m(x,y)` 把**同一批星残差散度**由 `sigma_obs = 0.06892` 压到 `delta_after_m = 0.00819`（吸收 99%），而 `sigma_obs` 本身在 `m_degree = 0` 与 `m_degree = 2` 下**逐位相同**——`m(x,y)` 吸收了多少完全不进门的读数，门给不出任何可用于区分「平场残余 / PSF / 颜色 / 探测器非线性」的量；(iii) **下包络可被推翻**——`LOWER_BOUND_UNDEFINED` 的降级条件只按 n 判，n = 48（远超 12.236、下包络自称有效）时**仅 2% 的位置依赖残差**即把判定由 `BELOW_FLOOR` 翻成 `PASS`、3% 翻成 `ABOVE_CEILING`。⇒ **真实帧判红（`sigma_obs = 0.026520 > σ_ceiling = 0.020561`）只能归到「未消系统项超预算」这一层，具体归因到任何一项都超出该门能给出的证据**；第 5 条与摘要中的判红叙述按此口径读。

---

## 参考文献

正文方括号数字对应本节条目。本节与文献台账 `refs.md` 的 V 编号对应：V11→1、V3→2、V6→3、V4→4、V1→5、V8→6、V7→7、V9→8、V10→9、V5→10、V2→11、V12→12、V13→13。

1. A. C. Aitken (1935). On Least Squares and Linear Combination of Observations. *Proc. R. Soc. Edinb.* 55, 42–48. [DOI:10.1017/S0370164600014346](https://doi.org/10.1017/S0370164600014346)
2. A. E. Beaton, J. W. Tukey (1974). The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data. *Technometrics* 16, 147–185. [DOI:10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)
3. Gaia Collaboration et al. (2023). Gaia Data Release 3: Summary of the content and survey properties. *A&A* 674, A1. [arXiv:2208.00211](https://arxiv.org/abs/2208.00211)
4. P. W. Holland, R. E. Welsch (1977). Robust regression using iteratively reweighted least-squares. *Comm. Statist. – Theory Methods* **6(9)**, 813–827. [DOI:10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533)（**期号订正 P1-m06**：Crossref/OpenAlex 两库一致为 6(9)，原写 6(8)）
5. K. Kafadar (1983). The Efficiency of the Biweight as a Robust Estimator of Location. *J. Res. Natl. Bur. Stand.* 88(2), 105–116. [DOI:10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)（全文 [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/)；锚有效性按台账 D-06）
6. P. Montegriffo et al. (2023). **Gaia Data Release 3:** External calibration of BP/RP low-resolution **spectroscopic data**. *A&A* 674, A3. [arXiv:2206.06205](https://arxiv.org/abs/2206.06205)、[DOI:10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)（**题名与 DOI 订正 P1-m06**）
7. Gaia Collaboration, P. Montegriffo, M. Bellazzini, F. De Angeli, R. Andrae, et al. (2023). Gaia Data Release 3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra. *A&A* 674, A33. [arXiv:2206.06215](https://arxiv.org/abs/2206.06215)，[DOI:10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)
8. ESA Gaia DR3 官方文档，§5.4.1（零点定义式 5.41 与绝对刻度 1%）、§20.12.4（`xp_sampled_mean_spectrum`，343 点 @2 nm，336–1020 nm）。[§5.4.1 *Zero points*](https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html)、[§20.12.4](https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html)
9. Gaia DR3 XP **可用域（两级）**：采样表示 `xp_sampled_mean_spectrum` 的额外子集以 **G = 15 mag** 为界、**连续表示到 G < 17.65**（连续表示 219 197 643 源）。原始出处 = P. Montegriffo et al. (2023), *A&A* 674, A3, **附录 B**，非 ESA 文档；[arXiv:2206.06205](https://arxiv.org/abs/2206.06205) 附录 B 原句 "For a subset of sources with BP and RP spectral data including only sources brighter than G = 15 mag … BP and RP spectra are also provided in the sampled representation"、"BP and RP mean spectra are published for 219,197,643 sources. This list includes mostly sources with G-band magnitude brighter than 17.65 mag"、[DOI:10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)
10. P. J. Rousseeuw, C. Croux (1993). Alternatives to the Median Absolute Deviation. *JASA* 88(424), 1273–1283. [DOI:10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)
11. statsmodels `robust/_tables.py`（开源逐字锚，4.685065）
12. GaiaXPy 2.1.4（官方开源实现，`sampled_spectrum.py:114`）
13. H. Akima (1970). A New Method of Interpolation and Smooth Curve Fitting Based on Local Procedures. *J. ACM* **17(4)**, 589–602. [DOI:10.1145/321607.321609](https://doi.org/10.1145/321607.321609)（**页域订正 P1-m06**：历史题录只给首页 589）

*待补脚注*：Croux & Rousseeuw (1992, 1993) 有限样本表值——原文未取得，MC 佐证 ≤0.93%，不列 VERIFIED（见 §7-9）。
