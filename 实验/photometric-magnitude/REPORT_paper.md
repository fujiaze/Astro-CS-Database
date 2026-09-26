# 测光星等坐标系：以 Gaia XP 绝对分光刻度锚定的通量积分拟合

**单元**：`实验/photometric-magnitude`（SCI-A，任务 SCI-401；科学链第 1 点，最高设计 §2）
**性质**：定稿论文（实验重做轮融合重写）。整合三路独立重做证据（路线1/2/3，seed=20260926）与本单元历史正本实验（step1–step9，seed=20260921）；分歧裁决一律以 `独立审计/实验重做/总编对账/分歧台账.md` 为准，UNRESOLVED 项不进正文（见 §7 诚实边界）。
**数字溯源约定**：每个关键数字标注 `[文献]`（refs.md 台账条目）、`[实验:code 文件名]` 或 `[推导]`（docs/derivation_robust_weights.md）。
**权威正本**：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001）；机器可读结果 `results/`（历史轮）与 `results/redo/`（重做轮，注明来源路线）。

---

## 摘要

将仪器计数（ADU）逐帧标定到一个可跨帧比较的星等坐标系上，是测光流水线的根基问题；当 FITS 头不携带增益、曝光与透过率信息时，标定因子的绝对值在数学上不可辨识，只有其与参考通量的比值携带科学内容。本文建立并验证一套以 Gaia DR3 XP 绝对分光采样谱锚定的测光星等坐标系：参考合成通量取 `F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ`（W·m⁻²·nm，336–1020 nm @2 nm 官方网格）[文献:V7/V9]，逐星残差 `r_i = log10(F_instr,i/F_syn,i)` 经固定尺度 Tukey biweight IRLS（c = 4.685）稳健聚合成逐帧零点。常数体系三腿闭合：c = 4.685 使 biweight 位置估计量达到正态 95% 渐近效率（解析积分 ARE = 0.9499974，反解 c* = 4.6850649，与 statsmodels 4.685065 六位一致，一手锚 Kafadar 1983）[实验:code/redo/route2/exp1_robust_constants.py][文献:V1/V2]；MAD→σ 常数 0.6744897501960817 = Φ⁻¹(3/4) 为解析恒等式（二分复算相对差 1.6×10⁻¹⁶）[推导:D3][实验:route2/exp1]；零点平移不变量逐位成立，标定系数因此被证明不含绝对数值窗口 [推导:D2][实验:route3/exp_S04]。误差预算判据的包络因子 1.166 = √1.361 系 MAD 尺度估计量标准化方差（1.361，Rousseeuw & Croux 1993 Table 2）的平方根 [文献:V5][推导:D5]（解释标签按分歧台账 A-P1-01 订正）。独立重做轮进一步把主系统差源量化：星等窗选择使合成零点系统平移最高 0.032 mag（比统计误差大一个量级以上）[实验:route1/exp2]；预过滤窗在 IRLS 就位后对零点精度的边际保护 ≈ 0，其真实作用面是控制进入拟合的样本族 [实验:route1/exp2]。三类数据（纯解析 Oracle、含真实 HST M16 星云结构的物理前向仿真、testdata 真实帧）独立给出同一结论：单帧双边界判据在预算完整时可判定，判据在真值无效应时归零，在乘性空间残差注入下单调上升并判红。本单元输出的每 dex 精度直接构成下游跨帧绝对 SNR 链（P2）的误差底座。

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

Tukey biweight 位置估计与其 IRLS 解法源自 Beaton & Tukey (1974) [文献:V3] 与 Holland & Welsch (1977) 的权重常数表 [文献:V4]；c = 4.685 与正态 95% 渐近效率的显式联系由 Kafadar (1983) 给出 [文献:V1]（台账 D-06 判该锚有效）。MAD 尺度估计量的 37% 高斯效率与标准化方差 1.361 由 Rousseeuw & Croux (1993) 建立 [文献:V5]。Gaia DR3 XP 外定标采样谱的物理刻度、通带定义与 ±2% 外定标精度见官方文档与 Montegriffo et al. (2023) [文献:V7/V8/V9]。反方差加权估计的经典框架为 Aitken (1935) [文献:V11]——本项目统一采用反方差口径；本单元 IRLS 使用的是稳健权（§2.2），反方差口径经量纲变换 `ivar′ = ivar/α²` 进入下游（§3 链条位置）。

---

## 2 方法

### 2.1 参考合成通量

```
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ        # 单位 W·m⁻²·nm；不含 10^(−0.4·G)
F_λ(λ_i) = byte_i·flux_mul + flux_min   # 绝对谱辐照度 W·m⁻²·nm⁻¹（XPSD 量化解码）
```

通带 = 滤镜透过率 T 与探测器 QE Q 的组合 [文献:V7 正文原句]；λ 网格为官方 343 点 @2 nm（336–1020 nm）[文献:V9 §20.12.4]；插值 Akima 子样条（区间外填 0），求积复合 Simpson 1/3（末尾奇数区间 3/8，`n==1` 退梯形）。历史上本单元曾把 `×10^(−0.4·magG)` 写成"冻结约定"；该写法不成立——官方定义式带 λ、不带任何星等因子 [文献:V9 式 5.41]，逐星乘 `10^(−0.4·G_i)` 会给 `r_i` 注入 +0.4·G_i dex 的加性项，单标量零点吸收不掉 [实验:RESOLUTION_fsyn_formula.md 负例，dlog10(ratio)/dG = 0.400]。生产实现与订正后公式逐位一致（两真实帧零点复算差 9.5×10⁻¹³ / 4.3×10⁻¹¹）[实验:RESOLUTION_fsyn_formula.md §2.3]。完整判定见 `RESOLUTION_fsyn_formula.md`（历史订正记录，保留不改，本节在其上引用）。**工程配套（负责人已批）**：插值/求积设置配置化，运行日志输出不落盘。

### 2.2 逐星残差与稳健零点

```
r_i   = log10(F_instr,i / F_syn,i)                    # dex
S     = MAD(r)/0.6744897501960817,  c = 4.685,  tol = 1e-6,  max_iter = 50
u_i   = (r_i − location)/(c·S);  w_i = (1−u_i²)²·[|u_i|<1]
location = Σ w_i·r_i / Σ w_i;   k_photo = 10^(−location)
sigma_residual = MAD(r_inliers)/0.6744897501960817;   sigma_obs = 2.5·sigma_residual
```

预筛 `|r − median(r)| ≤ 3.0`（dex 域 1.2 dex，与 3.0 mag 窗严格等价 [推导:D1]）。

**权重语义分立**（按负责人已批的反方差口径条款）：IRLS 的 w_i 是稳健权，吸收错配与污染、服务无偏性，不是反方差权；反方差加权（Aitken 1935 [文献:V11]）在本单元体现为施加链的量纲变换 `x′ = α·x ⇒ Var′ = α²·Var ⇒ ivar′ = ivar/α²`（α = k_photo·m(x,y)），交下游 P5 消费。两族权重不得混写 [推导:D7]。

**求解器设置的豁免依据**：tol/max_iter 是数值收敛档而非物理量——tol 自 1e-2 收紧至 1e-12，中位 location 变化仅 ~2×10⁻⁴ dex，50 步上限最多 17 步即达 [实验:code/redo/route2/exp1_robust_constants.py]；登记为"求解器设置 + 不变性实验为凭"（豁免清单见实验报告）。

### 2.3 双边界判据

```
n             = 本帧匹配星数（Gaia 匹配 ∧ 有效域 ∧ IRLS inlier）
sigma_floor   = (1 − 3·1.166/√n)·sigma_fit(白; 本帧匹配星通量分布)
sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)
PASS ⟺ sigma_floor ≤ sigma_obs ≤ sigma_ceiling
```

预算项各计一次：光子噪声、PSF 拟合不确定度（精确 Fisher，含自由背景简并项）、平场残余、天光扣除残余、颜色项、参考侧、量化；仿真帧上逐项由本帧推导，真实帧上 σ_color/σ_gaia 不可自算、如实标 `null`（上界不完整）。**因子标签订正**：1.166 = √1.361，其中 1.361 是 MAD 尺度估计量在高斯数据下的渐近标准化方差 [文献:V5 Table 2]——即 1.166 是 σ̂ 的相对标准差因子，`3·1.166/√n` 为"σ 估计量自身抽样涨落"的 3σ 包络（**按分歧台账 A-P1-01 订正**；历史正本"1.166 = SD(MAD)/MAD"的标签不准确）。判据形态逐字来源 `docs/plugins/algorithms_phase1/06_photometry.md` §4.1。

### 2.4 施加与降级

`I_photo = k_photo·m(x,y)·I_cal`；未启用时写显式 `degraded_reason` 且 fail-closed。门①（n<3）触发 ⇒ NO_DATA、scale=1.0/σ=0/fit_used=0，下游按未标定分支处理。

---

## 3 【链条位置】

**上游输入（谁给什么）**：

| 输入 | 口径 | 来源 |
|---|---|---|
| `F_instr` [ADU] | PSF 域解析通量；5×5 盒和与 N-25 口径禁止 | 前级检测/测光 |
| `F_syn` [W·m⁻²·nm] | ∫F_λ·T·Q·λ dλ；XP 336–1020 nm @2 nm；**G<15 官方域限制** [文献:V10] | `spectrum_integrator` |
| `G_Gaia` | 仅进 delta/ZP 诊断，不入 F_syn | Gaia DR3 |
| 质量位 | 饱和预排除 | 探测级 |

**P1 产出（下游消费）**：

| 产出 | 定义 | 消费方 |
|---|---|---|
| `k_photo = 10^(−location)` | [F_syn 单位]/ADU；绝对值无物理意义 | P3/P4 施加链 `I_photo = k_photo·m·I_cal` |
| `sigma_residual` [dex] / `sigma_mag` = 2.5·sigma_residual [mag] | 单帧测光一致性 | P2 零点统计误差 = 1.253·sigma_residual/√N [推导:D6] |
| `ZP_syn`、`zero_point_scatter_mag` | 绝对合成星等零点 | P2 `m_5 = ZP − 2.5·log10(5·σ_F)` |
| `ivar′ = ivar/α²`（α = k_photo·m） | 反方差口径的量纲变换 | P5 SNR 加权 |

**精度约定**：①r_i（dex）与 delta_i（mag）不共用容差 [推导:D1]；②`sigma_residual = 0 ∧ fit_ok = true` 的帧不可估计——完美控制点须按非完美处理，P4 不得以其为真值锚 [实验:route3/exp_S05]；③门①（n<3）⇒ NO_DATA 分支，且 n=3 时 sigma_mag 系统性下偏（MAD 有限样本偏差 1.49×，渐近式 1.2533/√n 在 n=3 高估 7.4%）[实验:route3/exp_S11]——P2 的低星数帧零点不确定度被低估；④ZP_syn 三星下限的抽样误差是 n≈2338 满载的 25.7 倍 [实验:route3/exp_S11]。**跨单元归属说明**：`k_corr` 冻结常数 1.4 改两因子几何查表（台账 D-08，负责人已批）由 P3/P5 单元承载，本单元不涉。

---

## 4 数据

三类数据相互佐证（最高设计 §12.2），复现代码与 seed 见 §6：

1. **纯解析代数合成**（Oracle）：600 星，SED 取真实 Gaia XP 谱；无噪声组 + 多噪声组；`m(x,y)` 解析已知。检验实现正确性、积分约定、IRLS 稳健性与负例。[实验:code/step1_analytic.py，seed 20260921]
2. **物理前向仿真**（真实信号模板）：HST M16 F657N HLSP drz 真实观测结构 24×24 分块平均（~0.95″/px），注入星取真实 XP SED；逐像素 Poisson（源+天光+暗流）→读出→增益→饱和→量化。检验真实结构下判据行为。[实验:code/step2_hst_sim.py]
3. **testdata 真实帧**：FLI M42 M1 T2 Red 300 s（4096²，uint16+BZERO）。检验帧内参数、WCS 二轮精化与引导检测的可用性；无真值，作底参照。[实验:code/step8_real_frame.py]

外部交叉核对：SVO HST/WFC3_UVIS2 通带曲线 + 头部 PHOTFLAM/PHOTPLAM；GaiaXPy 官方实现 [文献:V12]。

---

## 5 结果

### 5.1 常数体系三腿闭合（重做轮核心产出）

| 常数 | 读数 | 三腿 |
|---|---|---|
| `c = 4.685` | ARE = 0.9499974；反解 c* = 4.6850649 ≡ statsmodels 4.685065（六位） | [文献:V1/V2][实验:route2/exp1, route3/exp_S01][推导:D4] |
| `0.6744897501960817` | = Φ⁻¹(3/4)，二分复算相对差 1.6×10⁻¹⁶ | [推导:D3][实验:route2/exp1, route3/exp_S02] |
| `1.482602218505602` | 倒数恒等式；**发现** `frame_photometry_fit.cpp:292` 用 4 位截断 1.4826（相对差 1.496×10⁻⁶），违反 `NOISE_ESTIMATION.md:213` 冻结条款 | [推导:D3][实验:route2/exp1]（订正项 A-P1-08） |
| `1.166` | = √1.361（MAD 标准化方差，R&C 1993 Table 2）；路线1 推导腿闭合 √1.361 = 1.1666；MC 渐近 1.170（+0.3% 登记） | [文献:V5][实验:route1/exp1][推导:D5]（标签订正 A-P1-01） |
| 平移不变量 | F_instr 整体乘常数 ⇒ location 平移精确、内点集逐元素相同 | [推导:D2][实验:route3/exp_S04, route2/exp2 负例] |

**锚体系核验**（重做轮对审查"幻觉锚"指控的独立复核）：路线3 逐锚抽验 15/15 真实 [实验:route3/exp_S00]；路线2 复核 24 处科学量锚，唯一判"内容不符"者为 PMC6768164——经台账 D-06 直验原文（含 c=4.685 原句）判**锚有效**，误判撤回。三处定位偏差（PHOTOMETRY.md:126/:400 等）按台账 A-P1-04/09 改记"行号漂移"而非内容伪造：正本在 :13/:15–17。Gaia DR3 的 arXiv 号自纠为 2208.00211（原引 2205.11321 证伪）[文献:V6]。

### 5.2 Oracle 与三类数据上的判据行为（历史正本轮，seed 20260921）

- Oracle（`m≡1`）：估计器与真值一致到机器精度（rtol = 0.0，k 相对误差 3.33×10⁻¹⁵）[实验:code/step1_analytic.py]。
- 三帧物理前向仿真 + 一帧真实帧全部 PASS：σ_obs = 0.045344 / 0.057457 / 0.051718 / 0.026520 mag，观测/预算比 1.009 / 0.727 / 0.390 / — [实验:code/step2_hst_sim.py, step5_calibration_gate.py]。
- 负例（判据非退化）：真值无效应 ⇒ sigma_obs = 0 且判红；乘性空间残差注入 ⇒ sigma_obs 单调上升并判红；散粒噪声敏感（Poisson 天光抬升 2.91×，4× 判红）而确定性加性图样不敏感（1.08×）⇒ 判据**不能认证天光扣除质量** [实验:code/step7_negatives.py]。
- 低样本失效模式：`n ≲ 22` 时下界 `1 − 3·1.166/√n` 趋零或变负，过裁剪样本反而 PASS（n = 5 实测 σ_floor = −0.004911）⇒ **判据只有上界是硬约束** [实验:code/step5_calibration_gate.py][推导:D5]。

### 5.3 系统差源量化（重做轮）

- **星等窗 → ZP_syn 系统平移**：窗 {6–16, 6–12, 12–16, 10–15} 实测平移 0 / +0.0153 / **−0.0318** / −0.0144 mag [实验:route1/exp2]——比零点统计不确定度（~10⁻³ mag）大 30 倍，与 XP 外定标 ±2% ≈ 0.022 mag [文献:V8] 同量级。两个消费面的星族口径分裂是可量化的系统差源，必须统一或如实标注。
- **预过滤窗的真实作用面**：在 IRLS/Tukey 就位后，`mag_tolerance = 3.0` 对 ZP 偏差的边际保护 ≈ 0（实测 −7.3×10⁻⁶~0 dex）；其真实作用是控制进入拟合的样本族（保护尺度初值 S 不被错配污染）[实验:route1/exp2]。文献腿登记为项目冻结值（正本明言不引文献背书）。
- **FOV 三常数的危害量化**：缓冲 1.2 与钳位界 1.0/10.0 的优先语义冲突实测确认（0.3″/px 全幅被上抬 ×3.02，10″/px 广角被压低 ×0.69）；错配对经预过滤 + IRLS 后 location 系统偏移 −0.146 dex 且 `|r_consistent| ≥ 3` 门可被满足 ⇒ 奇协方差阵下拟合照常产出错误标度 [实验:route1/exp3]。按台账 A-P1-12，`05 A-4b` 须写明缓冲与钳位的优先语义。
- **1.0 dex 粗筛界非恒真**：干净/污染场通过率 0.055/1.110 对照，能红 [实验:route3/exp_S12]；空间增益阶数（order ≤ 2 + 六降级门槛）确认为科学量而非结构性选择 [实验:route1/exp4 独立复核]。
- **匹配半径**：2.0 px 最优档正确率 ≥ 0.996，歧义率 ≈ r² 理论 [实验:route1/exp2, route3/exp_S10]；全档敏感性表（0.5–4.0 px）支撑其为实现选择。

### 5.4 端到端贯穿用例

路线2 用例（2000 星，seed 20260926）：上游给 XP 型谱与 G ⇒ P1 产出 `ZP_syn = 28.9358 mag`、`k_photo = 1.00156` [F_syn]/ADU、`sigma_residual = 0.0493 dex` ⇒ 下游 `sigma_kappa,stat = 1.253·sigma_residual/√N = 0.00138 dex`、`m_5 = 27.19 mag` [实验:route2/exp8]。路线1 用例（800 星）：scale 真值 3.7×10⁶ 恢复为 3.7053×10⁶（相对误差 1.44×10⁻³，与 σ/√800 量级一致）[实验:route1/exp2]。

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
4. **低样本域**：`n ≲ 22` 下界失效；n=3 帧零点不确定度被低估（1.49× 偏差）——下游消费约定见 §3。
5. **循环性**：仿真中引导匹配的定位输入即注入真值；非循环证据来自真实帧。
6. **项目约定（不注文献出处）**：FOV 三常数（缓冲 1.2/钳位 1.0/10.0）、自适应阶梯 {12..16}/2000/5、mag_min 亮端、1.0 dex 界、匹配半径 2.0 px、max_stars=5000（σ_ZP(N) 曲线支撑的工程上限）均为项目约定，敏感性与危害量化已给出；`quality_factor` 0.1/0.5 的豁免（负责人已批，文档写明"项目约定，不注文献出处"）由 P5 单元承载。
7. **条款一致性缺陷（已登记待修）**：`frame_photometry_fit.cpp:292` 的 1.4826 四位截断（A-P1-08）；`mag_max_arr` 长度与 i<5 循环上限耦合使第 6 档改动静默失效（A-P1-08）；B13 接线整改（门语义换轨，路线1 S15）。均为文档/实现整改项，不改判据方向。
8. **文献边界**：Croux & Rousseeuw 1992/1993 有限样本表值原文未取得（MC 佐证 ≤0.93%），不列 VERIFIED，作"待补"脚注；Huber & Ronchetti 2009 §6.5 页码未核，正文改引 Kafadar 1983；Lindegren 2021 亮端数字仍开放（不阻成稿）。

---

## 参考文献

1. A. C. Aitken (1935). On Least Squares and Linear Combination of Observations. *Proc. R. Soc. Edinb.* 55, 42–48. [DOI:10.1017/S0370164600014346](https://doi.org/10.1017/S0370164600014346) [文献:V11]
2. A. E. Beaton, J. W. Tukey (1974). The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data. *Technometrics* 16, 147–185. [DOI:10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171) [文献:V3]
3. Gaia Collaboration et al. (2023). Gaia Data Release 3: Summary of the content and survey properties. *A&A* 674, A1. [arXiv:2208.00211](https://arxiv.org/abs/2208.00211) [文献:V6]
4. P. W. Holland, R. E. Welsch (1977). Robust regression using iteratively reweighted least-squares. *Comm. Statist. – Theory Methods* 6(8), 813–827. [DOI:10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533) [文献:V4]
5. K. Kafadar (1983). The Efficiency of the Biweight as a Robust Estimator of Location. *J. Res. Natl. Bur. Stand.* 88(2), 105–116. [DOI:10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)（全文 [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/)；锚有效性按台账 D-06）[文献:V1]
6. P. Montegriffo et al. (2023). Gaia DR3: External calibration of BP/RP low-resolution spectra. *A&A* 674, A3. [arXiv:2206.06205](https://arxiv.org/abs/2206.06205) [文献:V8]
7. Gaia Collaboration, P. Montegriffo et al. (2023). Gaia DR3: Synthetic photometry from Gaia low-resolution spectra. *A&A* 674, A33. [arXiv:2206.06215](https://arxiv.org/abs/2206.06215) [文献:V7]
8. ESA Gaia DR3 官方文档 §5.4.1（零点定义式 5.41）、§20.12.4（`xp_sampled_mean_spectrum`）[文献:V9]
9. Gaia DR3 XP 官方发布（XP 可用域 G<15）[文献:V10]
10. P. J. Rousseeuw, C. Croux (1993). Alternatives to the Median Absolute Deviation. *JASA* 88(424), 1273–1283. [DOI:10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408) [文献:V5]
11. statsmodels `robust/_tables.py`（开源逐字锚，4.685065）[文献:V2]
12. GaiaXPy 2.1.4（官方开源实现，`sampled_spectrum.py:114`）[文献:V12]

*待补脚注*：Croux & Rousseeuw (1992, 1993) 有限样本表值——原文未取得，MC 佐证 ≤0.93%，不列 VERIFIED（见 §7-8）。
