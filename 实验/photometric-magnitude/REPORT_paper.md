# 测光星等坐标系（创新点一）· 论文式精读报告

> **报告对象**：`实验/photometric-magnitude`（SCI-A / 任务 SCI-401，控制包 RELEASE-04）
> **正本科学口径**：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN；§16.5「星数依赖与降级语义」）
> **机器可读结果**：`实验/photometric-magnitude/results/step1..step8*.json`、`gates.json`、`GATES.md`（固定种子 20260921）
> **仓库版本**：`VERSION` = 0.11.0-alpha.2；HEAD = `d8495a65b696759bae209e509c6ef2e6a372245a`（main）
> **报告性质**：精读/复核报告。只读代码与结果，未改动任何被引文件；正文每个关键数字均可在附录 A 回溯到 `results/` 的具体字段（或明确标注为正式文档条款）。

---

## 摘要

本单元验证创新点一的核心猜想：**能否把仪器计数（ADU）逐帧标定到锚在 Gaia XP 绝对分光刻度上的「测光星等坐标系」，并在不引入任何物理闭合式的前提下判定这次标定是否可信。** 方法是「星表引导 PSF 测光 → Gaia XP 光谱 × 通带 × QE 积分合成期望通量 → 逐星 `r = log10(F_instr/F_syn)` 的 Tukey-IRLS 稳健零点 `location` → 星等域残差散度 `σ_obs` 落入由本帧误差预算推出的双边界 `[σ_floor, σ_ceiling]`」。

结论分三层。**（i）猜想成立**：纯解析 Oracle 中估计器与真值一致到机器精度（`rtol = 0.0`、`k_rel_err = 3.33e-15`）；三个物理前向仿真帧（含真实 HST M16 星云结构）与一个真实 testdata 帧全部 PASS，`σ_obs` 分别为 0.045344 / 0.057457 / 0.051718 / 0.026520 mag，观测/预算比 1.009 / 0.727 / 0.390 / —。**（ii）成立条件被明确划定**：判据对任何可解析帧都适用、星数不构成拒绝条件；低样本量区间（`n ≲ 22`）下界 `1 − 3·1.166/√n` 趋零甚至变负，过裁剪样本反而 PASS（N5 实测 `n = 5` 时 `σ_floor = −0.004911`，判据失效），故判据**只有上界是硬约束**；判据对散粒噪声敏感（Poisson 天光抬升 2.91×，4× 判红）、对确定性加性图样不敏感（1.08×），因此**不能认证天光扣除质量**。**（iii）工程路径清晰**：星表引导检测相对盲检把匹配率从 0.2254/0.1972 提到 0.9859，真实 4096² 帧上盲检 13163 个检出仅 1.49% 对应星表星、逐源拟合需 30.3× 的拟合次数；但前提是二轮 WCS **平移精化**收敛到 ≲2 px（HST HLSP drz 头部 WCS 与 Gaia 有 ~1.6″ 系统偏移，testdata 真实帧仅 0.0068″）。

诚实边界：本实验证明的是「标定系数无绝对窗口且不可反解仪器参数」（退化族 `A·t/g` 相同 ⇒ 统计不可分；零点平移不变量机器精度级成立），**没有**证明更强的「物理单位在全帧被消除」；窄带（等效宽度 29–39 nm）XP 绝对刻度与 HST PHOTFLAM 的中位差为 −0.147/−0.263/−0.080 mag，**宽带未测**。

---

## 1 引言

### 1.1 问题定位

AstroCS 三个创新点中，创新点一回答的是「**把一帧图像标定到什么尺度上，这个尺度才既物理诚实又可用于跨帧比较**」（`ASTROCS_DESIGN.md` §2.1、§4.2、§4.4）。项目的数据面前提是硬的：**FITS 头拿不到增益、曝光、有效口径、光学/大气透过率**。于是可观测链只能写成乘积形态

```text
I_cal = (g · t · A_eff · η · …) · ∫ F_λ(λ) · T(λ) · Q(λ) · λ dλ  +  噪声
```

其中括号内各项与模型通带的归一常数在数学上**只有乘积可辨识**。因此本项目的产物必须是**星等/相对星等**，标定因子（`k_photo` / `scale`）的绝对值**无物理意义**——它把未知量全部吸收（`docs/science/PHOTOMETRY.md` §1、§16.2）。

### 1.2 待答的三个问题

本报告围绕负责人阅读关心的三件事组织：

1. **猜想是否成立**：单帧、尺度无关的测光一致性判据，能否对一次标定给出可信的 PASS/判红？
2. **在什么条件下成立**：适用域边界在哪，域外会发生什么，哪些结论被明确判为「判不了」？
3. **工程上怎么做**：检测/定位/施加/降级这条链在真实帧上要满足哪些可操作条件？

### 1.3 前置约束（不可协商）

- **禁止物理闭合反推**：不得用 `k = g·h·c·1e9/(A·t)` 之类的闭合式由标定系数反解仪器参数，也不得为 `k_photo`/`scale` 设绝对数值窗口（`PHOTOMETRY.md` §10、§16.3；变更 claim 语义见 §1）。
- **禁止跨帧一致性门**：门只有一个 = 单帧标定是否可信；「帧间一致性」是语义目标与报告字段，不是门禁（`PHOT-GATE-DROP-001`；`PHOTOMETRY.md` §1/§10）。
- **判据必须非退化**：真值无效应时必须归零或判红，恒真门没有证据资格（`AGENTS.md` §5）。

---

## 2 方法

### 2.1 观测链与待估量（推导主线）

合成期望通量（冻结约定，`spectrum_integrator`）：

```text
F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ × 10^(−0.4·G_Gaia)
        S：Gaia DR3 XP 谱（343 点 @2 nm，336–1020 nm）；Akima 子样条插值（区间外填 0）；
        复合 Simpson 1/3 求积（末尾奇数区间 3/8，n==1 退梯形）
```

逐星残差与稳健零点（`PHOTOMETRY.md` §5，实现 `star_matcher.cpp`）：

```text
r_i   = log10(F_instr,i / F_syn,i)                    # dex
S     = MAD(r)/0.6744897501960817,  c = 4.685,  tol = 1e-6,  max_iter = 50
u_i   = (r_i − location)/(c·S);  w_i = (1−u_i²)² (|u|<1) else 0
location = Σ w_i·r_i / Σ w_i;   k_photo = 10^(−location)
σ_residual = MAD(r_inliers)/0.6744897501960817;   σ_obs = 2.5·σ_residual
```

### 2.2 双边界判据（本单元的核心判据）

```text
n             = 本帧匹配星数（Gaia 匹配 ∧ 有效域 ∧ IRLS inlier）
σ_floor       = (1 − 3·1.166/√n)·σ_fit(白; 本帧匹配星通量分布)          [物理下限]
σ_ceiling     = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)                  [预算上限]
PASS  ⟺  σ_floor ≤ σ_obs ≤ σ_ceiling
```

预算项各计一次：光子噪声、PSF 拟合不确定度（精确 Fisher，含**自由常数背景**简并项）、平场残余 `σ_flat`、天光扣除残余 `σ_skyres`、颜色项 `σ_color`、参考侧 `σ_gaia`、量化 `σ_q`。`1.166 = SD(MAD)/MAD`（正态渐近常数），`3×` 为 3σ 抽样允差（唯一约定性选择）。判据形态逐字来源 `docs/plugins/algorithms_phase1/06_photometry.md` §4.1。

### 2.3 物理单位消除与「禁止反解」的论证主线

1. **可辨识性**：观测只给出乘性标定面 `k_photo·m(x,y)`；拆出 `(g, t, A_eff, η)` 需要数据中不存在的外部先验 ⇒ 欠定；
2. **星等免疫**：乘性因子在 `log10` 下变加性零点，零点平移不变量保证 `σ_residual` 与判据不变（`PHOTOMETRY.md` §7）；
3. **不可证伪的窗口**：任何绝对窗口都是未知仪器参数的函数，无证据资格 ⇒ §10 列为不可接受变化；
4. **唯一有意义的判据**是尺度无关的测光一致性（§2.2）。

### 2.4 apply photometry 与降级语义

```text
I_photo = k_photo · m(x,y) · I_cal,   m(x,y) = 10^(+0.4·poly(x,y)) = 1/m_gain(x,y)
```

注意 `m` 是**归一化修正场**（乘性改正），不是仪器增益本身——该方向错误曾在审稿 R9 中被指出并修复（`results/REVIEW.md`）。未启用时产品必须写显式 `degraded_reason` 且 **fail-closed**；星少到 §4 冻结门（`|r_consistent| ≥ 3`）不成立时拟合不产出标度，同样写显式 `degraded_reason` 并 fail-closed。

---

## 3 实验设计

### 3.1 三类实验数据互证（`ASTROCS_DESIGN.md` §12.2）

| 类别 | 数据 | 角色 | 生成 |
|---|---|---|---|
| ① 物理前向仿真 | HST M16 F657N HLSP drz 真实观测模板，24×24 分块平均 → ~0.95″/px；注入位置/SED 取自真实 Gaia DR3 XP；逐像素 Poisson（源+天光+暗流）→ 高斯读出 → 增益 → 饱和 → 量化 | 真实结构 + 已知真值 | `step2_hst_sim.py`（帧 A 透明 1.0 / B 0.62 / C 扩展星场 500 星） |
| ② 纯解析代数合成 | 600 星、SED 取真实 XP、`m(x,y)` 解析已知、无噪声组 + 多噪声组 | Oracle / 收敛性 / 负例 | `step1_analytic.py` |
| ③ testdata 真实帧 | FLI M42 M1 T2 Red 300 s（4096²，`BZERO=32768`） | 底参照（无真值） | `step8_real_frame.py` |

外部绝对刻度对拍：SVO HST/WFC3_UVIS2 F657N/F673N/F502N 总系统透过率 + 头部 `PHOTFLAM/PHOTPLAM`（`step3`，唯一需网络的一步）。

### 3.2 判据与流程

固定种子 `20260921`（所有 RNG 由 `rng(tag)` 经 SHA256 派生，跨机器可复现）；`step1..step9` 串行，`step9_collect.py` 汇总为 `GATES.md/gates.json`。验收项 G1–G8 对应 `ACCEPTANCE_SPEC.md` §2.1 的六项（测光正确性 / 物理单位消除 / 帧间独立 / 星表引导检测 / apply photometry / 非退化负例），另加 G1b（预算逐项）与 G7（三类数据互证）、G8（PHOTFLAM 对拍）。

### 3.3 负例设计（「能红能绿」）

| 负例 | 类型 | 注入/构造 |
|---|---|---|
| N0 | 退化输入 | 真值无效应（`F_instr = inject_scale·F_syn` 精确）⇒ 必须 `BELOW_FLOOR` |
| N1 | 注入-响应 | 乘性平场残差幅度 0→0.16（0.01/0.02/0.04/0.08/0.16） |
| N2 | 注入-响应 | **Poisson 物理口径**天光抬升 0.5/1/2/4×（另设算术梯度对照，明确不作为通过依据） |
| N3 | 注入-响应 | 预算漏掉颜色项（真实 XP 谱 × 两条 QE 曲线）幅度扫描 |
| N4 | 错误门禁反例 | 用 step5 **实测** `k_A/k_B` 检验「跨帧 k 一致性门」 |
| N5 | 退化输入 | 对帧 A **实测**通量按 `\|r−median(r)\| < tol` 过裁剪 |

---

## 4 结果

### 4.1 表 1 · 纯解析 Oracle 与不变量（`step1_analytic.json`）

| 检验 | 实测 | 结论 |
|---|---|---|
| Oracle 零点（`m≡1`） | `location = 16.336461198922965`，真值同值；`rtol = 0.0`；`k_rel_err = 3.33e-15`；`n_inliers = 592` | 估计器与真值一致到**机器精度** |
| 冻结积分约定 vs 独立稠密线性+梯形 Oracle | `oversample` 4→256：中位相对差 1.64e-4→2.04e-4，最大 5.07e-4（≈2.2e-4 mag） | 约定差异 ≪ 0.01 mag 预算 |
| 空间增益恢复（deg2） | 无 `m` 模型 `σ_obs = 0.03720` → 拟合后 **0.000365** mag；`Δlocation = −0.001697` dex | 低阶空间增益可被逐帧吸收 |
| 20% 离群稳健性 | 离群率 13.71%，`\|Δlocation\| = 0.000924` dex（< 0.1） | Tukey IRLS 稳健 |
| `S = 0` 退化 | 取 median 不迭代，`location = 0.6989700043`（= 期望） | 退化分支正确 |
| 噪声标定（天光 ×0.25/1/4/16） | `σ_obs` = 0.013352 / 0.013603 / 0.019149 / 0.034587，全 PASS | 度量随噪声单调，判据不恒真 |

**解读**：前五行说明实现与冻结公式一致、且退化/离群分支按设计行为；最后一行是判据非退化的基础——度量随物理噪声单调上升，而不是一条恒真的门。

### 4.2 表 2 · 三帧双边界判据与逐项预算（`step5_calibration_gate.json`）

| 帧 | 说明 | `n`(选中/预算) | `σ_obs` [mag] | `σ_floor` | `σ_ceiling` | 判定 | obs/pred | `k` 相对误差（有效真值） |
|---|---|---|---|---|---|---|---|---|
| A | 透明 1.0 | 48 / 48 | **0.045344** | 0.009659 | 0.056830 | PASS | **1.009** | **2.09%** |
| B | 透明 0.62 | 54 / 54 | **0.057457** | 0.012517 | 0.103015 | PASS | 0.727 | **1.85%** |
| C | 扩展星场 500 星 | 105 / **96** | **0.051718** | 0.014566 | 0.159899 | PASS | 0.390 | **3.95%** |

帧 A 逐项预算（全部由本帧推导）：`σ_pix = 18.40 e-`、结构因子 1.000、`σ_psfsys`（帧内小孔径 r=4 px）= 0.02500 mag（真值口径 0.025633）、`σ_color = 0.005736`、`σ_flat = 0.019732`、`σ_gaia = 0.002`（注入假设）、`σ_skyres = 0.001469`、`σ_q = 0.000436`。

**帧间独立**：`k_B/k_A = 1.6098633`，透明度比倒数 `1/0.62 = 1.61290` ⇒ 比值/期望 = **0.99812**；残差分布 KS 检验 `D = 0.12269`、`p = 0.78688`（`n_A=48, n_B=54`）⇒ 分布一致；`cross_frame_gate_present = false`。

**解读**：三帧全部落在双边界内，且帧 A 的 obs/pred = 1.009 说明预算闭合；帧 C 的 0.390 偏离 1 达 2.6×，根因是拥挤场 `σ_psfsys` 帧内估计偏高（0.106 vs 真值 0.0394，2.7×）使上界偏松——方向保守，但说明**拥挤场的上界判别力下降**。表中 `k` 相对误差为**「有效真值」口径**（扣掉模型通带的公共零点后为 2.09%/1.85%/3.95%）；未扣公共零点的原始相对差为 **46.2%/46.3%/45.2%**（`step5 → frames[].k_rel_err`）——该公共偏移由模型通带未建模项与 `location` 一并吸收，正属 §16.2「只有乘积可辨识」的结论范围，不构成标定失败。

### 4.3 表 3 · 非退化负例响应矩阵（`step7_negatives.json`，5/6 通过）

| # | 构造 | 实测 | 判定 |
|---|---|---|---|
| N0 | 真值无效应 | `σ_obs = 0.0` < `σ_floor = 0.008188` | **BELOW_FLOOR（判红）** ✔ |
| N1 | 乘性平场残差 0→0.16 | 0.045344→0.047185→0.047976→**0.076175**→**0.098790**→**0.159590**（**3.52×**，amp≥0.04 判红） | ✔ |
| N2 | Poisson 天光 ×1/2/4 | 0.025199→0.048113→**0.073354**（**2.91×**，4× 判红） | ✔ |
| N2-对照 | **算术相加**确定性梯度 0→0.4 ADU/px | 0.045344→0.048968（**1.08×**），始终 PASS | 能力边界（不作通过依据） |
| N3 | 预算漏颜色项 | 真实颜色项仅 0.005736 mag ⇒ 不改变判定；判红阈值 **0.04 mag**（真实值的 **6.97×**） | ✔（机制成立，真实量级不成立） |
| N4 | 跨帧 k 门 | `k_ratio_measured = 1.6098633` vs 期望 1.6129032（偏差 0.188%）⇒ 该门会稳定误杀正确帧 | ✔ |
| N5 | 过裁剪真实样本 | `n` 48→45→22→5；`σ_obs` 0.045344→0.038031→0.019762→0.003562，`σ_floor` 0.009638→0.008586→0.002926→**−0.004911**，**全 PASS** | **✘ 未通过**（下界失效） |

**解读**：注入-响应型 3/3 全通过（N1 乘性、N2 散粒、N3 漏项机制），说明判据在域内「能红能绿」；但 N2-对照与 N5 划出两条能力边界——对**确定性加性图样**不敏感、对**过裁剪样本**无判别力（下界在 `n ≲ 22` 时趋零/变负）。

### 4.4 表 4 · 星表引导检测 vs 全图盲检（`step4_guided_vs_blind.json` + `step8_real_frame.json`）

| 指标 | 帧 A（仿真） | 帧 B（仿真） | 真实 M42（4096²） |
|---|---|---|---|
| 引导匹配率（真值 1 px 内） | **0.98592**（70/71） | **0.98592** | 434 候选，全部拟合成功 |
| 盲检匹配率（同口径） | 0.22535 | 0.19718 | 13163 检出，**1.489%** 对应星表星 |
| 匹配率提升 | **+0.76056** | **+0.78873** | 引导 45.16% 被盲检独立确认 |
| 虚警 | 盲检 3 / 引导 **0** | 盲检 5 / 引导 **0** | — |
| 每匹配星成本 | 引导 0.006581 s vs 盲检 0.008096 s | 引导 0.003643 s vs 盲检 0.009309 s | 引导 0.004773 s/星 |
| 粗 WCS 残余 0/1/2 px | 0.98592（不变） | 0.98592（不变） | — |
| 粗 WCS 残余 **3 px** | **0.01408**（崩溃） | **0.01408** | — |

**真实帧算力对照**：盲检 13163 个检出，若逐源做 PSF 拟合需 `13163/434 = 30.3×` 的拟合次数。

**解读**：引导检测在匹配率与每匹配星算力上双向占优，且在真实密集帧上把「拟合次数」压缩到 1/30；但 3 px 粗 WCS 残余即崩溃（`fit_psf` 位置搜索边界为 ±2 px）⇒ **二轮 WCS 平移精化是硬前置，不是可选优化**。

### 4.5 表 5 · `σ_psfsys` 的孔径口径对照（`step5_calibration_gate.json → items_measured`）

| 帧 | r=10 px 大孔径估计 | r=4 px 小孔径估计 | 真值口径 | 大孔径偏差 |
|---|---|---|---|---|
| A（HST 模板，透明 1.0） | 0.257421 mag | **0.024998** mag | 0.025633 mag | **~10×** 高估 |
| B（透明 0.62） | 0.255662 mag | **0.040083** mag | 0.034351 mag | **~7.4×** 高估 |
| C（拥挤扩展星场） | 1.377506 mag | **0.106078** mag | 0.039417 mag | **~35×** 高估（小孔径仍上偏 **2.7×**） |

**解读**：用「PSF 域通量 vs 独立孔径通量」的中位绝对偏差估 `σ_psfsys` 时，大孔径把星云结构算进「方法系统误差」；口径必须取**小孔径（≈2×FWHM）+ 低背景星子样本**，且拥挤场残余 2.7× 上偏（保守方向：判据偏松不偏紧）须如实登记。

### 4.6 表 6 · XP 绝对刻度对拍与 WCS 二轮精化（`step3_forward_vs_photflam.json`）

| 滤镜 | `n` | 中位 `Δmag` | bootstrap σ | MAD | 色项斜率 | 干净子样（G<17 ∧ 低背景） |
|---|---|---|---|---|---|---|
| F657N（窄带） | 42 | **−0.14718** | 0.11307 | 0.49114 | 0.46382 | n=12，−0.013721 / MAD 0.254622 |
| F673N（窄带） | 29 | **−0.26301** | 0.12967 | 0.64544 | 0.76528 | n=12，−0.224429 / MAD 0.104200 |
| F502N（窄带） | 75 | **−0.08011** | 0.03226 | 0.21329 | 0.56457 | n=18，−0.006782 / MAD 0.049218 |

同星跨滤镜（`n=19`）残差之差：中位 **0.117104** mag、MAD 0.325773 mag。透过率曲线独立校验：枢轴波长与头部 `PHOTPLAM` 相对差 3.73e-6 / 7.38e-6；由 `PHOTFLAM` 反推有效面积 44425.5 / 45322.9 cm² vs 几何面积 38453.1 cm²（比值 1.1553 / 1.1787）。

**WCS 二轮精化**：HST HLSP drz 头部 WCS 与 Gaia DR3 的系统偏移 = **1.62177″**（F657N，精化后 83/88 在 4 px 内）/ **1.62903″**（F673N，80/88）；testdata 真实帧残余仅 **0.0067800″**（827/827）。

**解读**：窄带上 XP 合成通量的绝对刻度不确定度是 **0.05–0.65 mag 量级**（取决于星云污染与星等范围），且随星色有 0.46–0.77 mag/mag 的斜率——这是「绝对通量」口径必须保持谨慎的直接证据；而 1.6″ vs 0.0068″ 的对照说明二轮精化的必要性**取决于上游 WCS 质量**，设计须能覆盖 ~2″ 量级平移。

### 4.7 判据汇总

`results/GATES.md` / `gates.json`：**8/9 项 PASS**，其中 **G6 = PARTIAL**（因 N5 未通过）。G1/G1b/G2/G3/G4/G5/G7/G8 全 PASS。真实帧单帧判据：`n = 157`、inlier 151、`σ_obs = 0.026520 ∈ [0.006008, 0.032456]` → PASS；但 `σ_color`/`σ_gaia` 在真实帧上不可自算、如实标 `null` ⇒ **上界不完整**。

---

## 5 讨论

### 5.1 猜想是否成立

**H1（主假说）在实验适用域内成立**：单帧自算误差预算的双边界判据可对一次标定给出 PASS/判红——三个物理前向仿真帧 + 一个真实帧全部 PASS，且注入-响应负例 3/3 能判红、真值无效应能判红（N0）。**H2（引导检测）成立**：匹配率 0.9859 vs 0.2254/0.1972，真实帧拟合次数比 30.3×。

但必须同时说清**它没有证明什么**：本实验证明的是「标定系数无绝对窗口、不可反解仪器参数、零点平移不变量成立」，即在**星等坐标系内自洽**；**没有**证明「物理单位在全帧被消除」这一更强命题——要证明后者需要 `(A, t, g)` 独立可观测，而实验恰恰证明它们不可分（退化族 `A·t/g` 相同 ⇒ 前两组统计不可分；`(A,t)` 为 1 参数退化，加 PTC 可得 `g` 但 `(A,t)` 仍不可分 ⇒ **欠定 2 参数族**）。

### 5.2 在什么条件下成立（适用域）

| 条件 | 边界 | 证据 |
|---|---|---|
| 星数 | **不构成拒绝条件**（无适用域门槛）：判据按本帧自身星数自算；星少到拟合不成立时走 NO_DATA 拟合失败路径 | `PHOTOMETRY.md` §16.5 第 1–2 条；本实验实测：帧 C `n=105` PASS、真实帧 `n=157` PASS |
| 下界有效性 | `n ≲ 22` 时 `σ_floor` 趋零/变负 ⇒ **只有上界是硬约束** | `step7 → N5`（`n=5` 时 `σ_floor = −0.004911`） |
| 判据敏感域 | 对散粒噪声敏感（2.91×）、对确定性加性图样不敏感（1.08×）⇒ **不能认证天光扣除质量** | `step7 → N2` 与 `N2.arithmetic_gradient_control` |
| 空间响应形态 | `m(x,y)` 为低阶（deg≤2）乘性；高阶/小尺度平场未覆盖 | `README.md` §6.2 |
| 样本条件 | 星等窗 13–20、`G` 波段参考侧、`FWHM ≈ 2` px 采样 | `README.md` §6.2 |
| 预算完整性 | 仿真帧预算逐项由本帧（含真值）推导；**真实帧仅 `σ_pix`/结构因子/`σ_psfsys`/`σ_flat` 可自算**，`σ_color`/`σ_gaia` 缺项 ⇒ 上界不完整 | `step8 → single_frame_gate.unavailable_items` |
| 帧间语义 | 各帧独立标定；`k` 不同是正确物理；**不得设跨帧门** | `step5 → frame_independence`；`step7 → N4` |

### 5.3 工程上怎么做（可操作结论）

1. **两轮 WCS 是流程前置**：第一轮盲检测只给粗 WCS 供投影；第二轮必须做**平移精化**并收敛到 ≲2 px（`fit_psf` 位置搜索边界 ±2 px），否则引导匹配率从 0.9859 崩到 0.0141；设计须覆盖 ~2″ 量级平移（HST HLSP drz 实测 1.62″/1.63″）。
2. **检测走星表引导**：把 Gaia DR3 逆投影到像素域，只在星表位置做 PSF 拟合；拟合失败直接丢弃、不计虚警（实测虚警 0）。
3. **通量口径唯一**：全链只用 PSF 拟合域通量 `flux = 2πA·s_x·s_y/3`；孔径测光只作显式诊断（`σ_psfsys` 的估计量必须用小孔径 + 低背景子样本）。
4. **施加到整帧像素**：`I_photo = k_photo·m(x,y)·I_cal`；`m` 是归一化修正场（`10^{+0.4·poly}`），不是增益；改正后残差必须小于不改正（帧 A：0.019732 vs 0.045344，改善 = true）。
5. **降级 fail-closed**：未产出标度的路径（含 §4 冻结门不成立）与未启用路径必须写 `degraded_reason` 并拒绝下游消费；**不得**把 NO_DATA 的占位 `k=1.0` 伪装成「已应用」。
6. **判据用法**：报出 `σ_obs/σ_ceiling`、`σ_obs/σ_floor` 与逐项分解；**只有上界可作硬约束**；天光扣除质量必须另设残差检查（本判据不覆盖）。

### 5.4 诚实边界（本报告认定的清单）

1. **强命题未建立**：「物理单位在全帧被消除」不予支持；只证明了标定系数无绝对窗口、不可反解、零点平移不变量成立。
2. **星数只进判据、不作准入门槛**：本实验的判据样本量为帧 C（选中 105）与真实帧（157）；帧 C 的判据内样本 `budget.n = 96`（9 颗星未进入白噪声子样）⇒ 判据内 `n` 与帧的匹配星数不是同一统计量，报出时必须写清用的是哪一个。
3. **下界在低样本量区间失效**（N5）：`n ≲ 22` 时下界无判别力；在该区间使用判据时，下界取 `max(ρ_lo, 0)·σ_fit`。
4. **上界在拥挤场偏松**：帧 C 的 `σ_psfsys` 帧内估计 0.106 vs 真值 0.0394（2.7×），obs/pred 仅 0.390。
5. **不认证天光扣除**：对确定性加性图样 1.08× 不敏感。
6. **宽带未测**：窄带（29–39 nm）结论不可外推到宽带（Baader R，~144 nm）。
7. **真实帧无真值**：`σ_color`/`σ_gaia` 标 `null`，真实帧判据强度低于仿真帧。
8. **引导匹配率有循环性**：仿真帧的定位输入就是注入真值位置（共用同一 WCS 正/反变换），0.9859 度量的是流水线完备度，不是独立定位精度；非循环检验来自真实帧（827/827）。「必须 ≲2 px」由 `fit_psf` ±2 px 边界决定，属实现边界而非独立物理发现。
9. **`σ_psfsys` 小孔径口径是作者约定**（有真值三方对照），生产文档原用 r=10 px。
10. **`apply` 的「独立复算 rel = 0.0」不是独立证据**（同一表达式两份拷贝）；`downstream` 是桩函数，「下游消费归一化像素」未被真正验证（`REVIEW.md` R12，轮次 2 未在必查清单内修复）。
11. **有效面积/几何面积 1.155/1.179 无独立标定**，只作旁证，不作定量结论。
12. **网络边界**：`step0_fetch_refs.sh` 是唯一需网络的一步，缓存不入库。
13. **对照实现独立性**：盲检为自实现 numpy 对照口径，不是生产 `sdet_api`；photutils/sep 只做源码语义核对、未在本机安装运行。
14. **墙钟时间不可复现**：`README.md` §4.7 的 2.093 s / 1.751 s 与当前 `step8_real_frame.json` 的 2.0717 s / 3.1875 s 不一致（同一行的比值类数字稳定）；本报告以 JSON 为准。
15. **星数门槛不落地（已裁决）**：测光不设星数门槛，`lib/` 内不存在按星数拒绝的准入判据；生产代码只保留 SCI-PHOT-001 §4 的**求解前提** `kMinFitStars = 3`（不成立即 NO_DATA，走拟合失败路径）。

### 5.5 与正式科学文档口径的关系与冲突

| # | 事项 | 判定 |
|---|---|---|
| 1 | §16.5「星数不构成拒绝条件」 vs 生产代码的 §4 求解前提 `kMinFitStars = 3`（`frame_photometry_fit.h`；`module_adapters.cpp` 的 `kMinFitStars` 引用） | **一致**：前者是不设准入门槛，后者是「本次拟合有没有产出标度」的求解前提；不成立即 NO_DATA 拟合失败，不是门槛拦截 |
| 2 | `06_photometry.md` §4.1「判据尚未在代码中生效」 vs §16.5 的降级语义 | **不冲突**：前者是模块落地状态，后者是判据的样本量依赖与降级语义；但须注意**实验 PASS ≠ 生产已设门** |
| 3 | `06_photometry.md` §4.1 记录 L4 真实数据 PASS 1/49、未消系统项 0.0442 mag vs 本实验 C5 的窄带 0.05–0.65 mag | **对象不同**：C5 量的是 XP 参考侧**窄带绝对刻度**，不能直接解释 0.0442 mag；只能作为候选来源之一 |
| 4 | `σ_psfsys` 孔径口径（生产 r=10 px vs 本实验 r=4 px） | **已由 §16.5 第 4 条订正**为小孔径 + 低背景子样本 ⇒ 与本报告一致 |
| 5 | 帧 C「n=105」与判据内 `budget.n=96` | **两个统计量**（帧匹配星数 vs 判据内白噪声子样），报出时须写明用的是哪一个 |
| 6 | `README.md` §4.7 墙钟时间 vs `step8_real_frame.json` | **数字不同步**（非科学结论冲突），以 JSON 为准 |

---

## 6 结论

1. **猜想成立（域内）**：单帧、尺度无关的双边界判据可判定测光标定是否可信；三个物理前向仿真帧（含真实 HST 星云结构）与一个真实 testdata 帧全部 PASS，Oracle 一致到机器精度（`rtol = 0.0`、`k_rel_err = 3.33e-15`）。
2. **成立条件明确**：判据对任何可解析帧都适用、不设星数门槛；低样本量区间（`n ≲ 22`）下界失效（N5），**只有上界是硬约束**；判据只认证天光**噪声**预算，不认证天光**扣除**质量。
3. **物理诚实性成立**：退化族 `A·t/g` 相同 ⇒ 统计不可分，零点平移不变量机器精度级成立 ⇒ 禁止由 `k_photo` 反解 `(A, t, g)`（欠定 2 参数族）、禁止绝对窗口。
4. **工程路径可执行**：星表引导 + 二轮平移精化（≲2 px）把匹配率从 0.2254/0.1972 提到 0.9859，真实帧拟合次数压缩 30.3×；`apply photometry` 落像素并以 `degraded_reason` fail-closed。
5. **精度口径须保守**：`σ_psfsys` 必须用小孔径（大孔径高估约 10×）；窄带 XP 绝对刻度中位差 −0.08 ~ −0.26 mag、逐星 MAD 0.21–0.65 mag，**宽带未测**。
6. **遗留待办**：① 判据下界在低样本量区间的形式修正；② 天光扣除残差检查；③ 帧匹配星数与判据内样本口径的报出口径统一；④ `README` 与归档 JSON 的数字同步（墙钟时间）。

---

## 7 参考文献

### 7.1 项目内（版本与提交可核验）

1. AstroCS 仓库，`VERSION = 0.11.0-alpha.2`，HEAD `d8495a65b696759bae209e509c6ef2e6a372245a`（main）。
2. `docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN，T103 冻结 2026-08-23）；§16.5 由 DOC-502（commit `b85c79dd`）登记；§16.2/§16.3 为单位消除与禁止反解论证。
3. `docs/plugins/algorithms_phase1/06_photometry.md` §4.1（判据形态与逐项预算；记录 L4 49 帧 PASS 1/49）。
4. `ACCEPTANCE_SPEC.md` §2.1（SCI-A 六项验收判据）；`ASTROCS_DESIGN.md` §2.1/§4.2/§4.4/§12.2/§12.3。
5. 实验单元：`实验/photometric-magnitude/README.md`、`results/GATES.md`、`results/gates.json`、`results/step1..step8*.json`、`results/REVIEW.md`（两轮独立审稿）、`results/DOC_CORRECTIONS.md`、`results/REVERSE_VERIFY_CANON.md`。
6. 实验代码：`实验/photometric-magnitude/code/*.py`（固定种子 20260921）；`code/README.md` 记录运行环境 Python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1。

### 7.2 一手文献（逐条标注核验状态）

- Montegriffo et al. 2023, A&A **674, A3**, DOI [10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)（Gaia DR3 XP 连续谱）——**文章级**：卷/页/DOI 经检索命中，定标推导未逐页核验。
- Riello et al. 2021, A&A **649, A3**, DOI [10.1051/0004-6361/202039587](https://doi.org/10.1051/0004-6361/202039587)（Gaia EDR3 测光）——**文章级**（A&A 649, A3 经检索核验）。
- Gaia Collaboration et al. 2021, A&A **649, A1**, DOI [10.1051/0004-6361/202039657](https://doi.org/10.1051/0004-6361/202039657), arXiv:2012.01533（EDR3 总览）——**文章级**。
- Bohlin, Hubeny & Rauch 2020, AJ **160, 21**, DOI [10.3847/1538-3881/ab94b4](https://doi.org/10.3847/1538-3881/ab94b4), arXiv:2005.10945（HST 通量标准）——**文章级**（DOI 与卷页经检索核验）。
- Horne, K. 1986, PASP **98, 609**, DOI [10.1086/131801](https://doi.org/10.1086/131801)（最优提取 Fisher 结构）——**文章级**。
- Beaton & Tukey 1974, Technometrics **16, 147**, DOI [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)；Mosteller & Tukey 1977, *Data Analysis and Regression*；Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.（ISBN 978-0-470-12990-6）——Tukey biweight `c = 4.685`；**书籍条目未逐页核验**。
- Rousseeuw & Croux 1993, JASA **88, 1273**, DOI [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)（MAD 稳健性）——**文章级**。
- Bertin & Arnouts 1996, A&AS **117, 393**, DOI [10.1051/aas:1996164](https://doi.org/10.1051/aas:1996164)（SExtractor）——**文章级**。
- Akima, H. 1970, J. ACM **17, 589**, DOI [10.1145/321607.321609](https://doi.org/10.1145/321607.321609)（谱插值）——**文章级**。
- STScI WFC3 Data Handbook §9.1（`PHOTFLAM/PHOTPLAM/PHOTBW` 定义）——**在线手册，文章级/未逐页核验**。
- 预算出处表中出现但本报告**未独立核验卷页**者：Stetson 1987；Irwin 1985；Anderson & King 2000；Naylor 1998；Zackay & Ofek 2017；Schlafly & Finkbeiner 2011；Stubbs & Tonry 2006（见 `PHOTOMETRY.md` §16.4）——**标注：未逐页核验**。

### 7.3 开源实现与数据服务（项目 + 版本 + 文件:行；注明运行状态）

- astropy：本机运行 **7.0.1**；`astropy/stats/funcs.py` `mad_std` 行号属源码阅读版本 **8.0.1**（两者语义相同，行号未在 7.0.1 上核对）。
- photutils **3.0.0** `detection/daofinder.py:26,210`（`xycoords` 跳过源查找的语义证据）、`psf/photometry.py:217`、`aperture/photometry.py:30`——**未在本机安装运行**。
- sep **1.4.1** `sep.pyx:387`——**未在本机安装运行**。
- SVO Filter Profile Service：HST/WFC3_UVIS2 F657N/F673N/F502N 总系统透过率（SHA256 前 12 位 `2af43d2dec10` / `fdb18eb39936` / `587ef650b066`）。
- 外部数据：HST HLSP M16 F657N/F673N drz（含 `PHOTFLAM` 头部）；`testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts`。

---

## 附录 A · 数字 → 证据文件:字段 索引表

> 路径均相对仓库根；`results/` 指 `实验/photometric-magnitude/results/`。标注「正式文档」的条目不在 `results/` 内，属定案条款而非实测数字。

| # | 关键数字 | 证据文件 → 字段 |
|---|---|---|
| A1 | Oracle `location = 16.336461198922965`，`rtol = 0.0`，`k_rel_err = 3.33e-15` | `results/step1_analytic.json → oracle_zero_point_m1.{location,location_true,rtol,k_rel_err,n_inliers}` |
| A2 | 20% 离群 `\|Δlocation\| = 0.000924` dex（< 0.1） | `results/step1_analytic.json → oracle_robustness_20pct.dloc / outlier_rate` |
| A3 | `S=0` 退化 `location = 0.6989700043360189` | `results/step1_analytic.json → oracle_S0_degenerate.{S,location,location_expected}` |
| A4 | 积分约定差异中位 1.64e-4→2.04e-4（最大 5.07e-4） | `results/step1_analytic.json → integral_convention_vs_dense_linear.rows[].median_rel/max_rel` |
| A5 | 空间增益拟合：0.03720 → 0.000365 mag | `results/step1_analytic.json → spatial_gain_recovery.{sigma_obs_no_m_model,sigma_obs_with_m_model}` |
| A6 | 天光 ×0.25/1/4/16 ⇒ σ_obs 0.013352/0.013603/0.019149/0.034587 | `results/step1_analytic.json → noise_scaling.rows[].{sky_mult,sigma_obs_mag,verdict}` |
| A7 | 三帧 `σ_obs` = 0.045344 / 0.057457 / 0.051718 | `results/step5_calibration_gate.json → frames[].sigma_obs_mag` |
| A8 | 三帧 `σ_floor`/`σ_ceiling` = 0.009659/0.056830、0.012517/0.103015、0.014566/0.159899；判定全 PASS | `results/step5_calibration_gate.json → frames[].budget.{sigma_floor,sigma_ceiling} / verdict` |
| A9 | obs/pred = 1.0092 / 0.7274 / 0.3902；`k` 相对误差（有效真值）2.09%/1.85%/3.95% | `results/step5_calibration_gate.json → frames[].{obs_over_predicted,k_rel_err_effective}` |
| A10 | 帧 A 逐项预算：σ_pix 18.4024 e-、σ_psfsys 0.024998、σ_color 0.005736、σ_flat 0.019732、σ_gaia 0.002 | `results/step5_calibration_gate.json → frames[A].budget.* 与 items_measured.*` |
| A11 | `k_B/k_A = 1.6098633`，比值/期望 0.99812；KS `D = 0.12269, p = 0.78688` | `results/step5_calibration_gate.json → frame_independence.{k_ratio,k_ratio_over_expected,residual_distribution_ks}` |
| A12 | 引导匹配率 0.98592 vs 盲检 0.22535/0.19718；提升 0.76056/0.78873 | `results/step4_guided_vs_blind.json → frames[].{guided.match_rate,blind.match_rate,match_rate_gain}` |
| A13 | 粗 WCS 3 px ⇒ 匹配率 0.01408 | `results/step4_guided_vs_blind.json → frames[A].coarse_wcs_robustness[3].match_rate` |
| A14 | 真实帧：盲检 13163（1.489% 对应星表星）、引导 434（45.16% 被确认）、30.3× | `results/step8_real_frame.json → guided_vs_blind_real.{blind_detections,blind_precision_vs_guided,guided_candidates,guided_completeness_vs_blind}` |
| A15 | 真实帧单帧判据 `n=157`、inlier 151、σ_obs 0.026520 ∈ [0.006008, 0.032456] PASS；σ_color/σ_gaia = null | `results/step8_real_frame.json → single_frame_gate.{n,n_inliers,sigma_obs_mag,sigma_floor,sigma_ceiling,verdict,items,unavailable_items}` |
| A16 | 真实帧 WCS 残余 0.0067800″（827/827） | `results/step8_real_frame.json → wcs_refinement.{shift_arcsec,n_matched_within_tol,n_in_frame}` |
| A17 | HLSP drz WCS 偏移 1.62177″/1.62903″（83/88、80/88） | `results/step3_forward_vs_photflam.json → wcs_refinement.{F657N,F673N}.{shift_arcsec,n_matched_within_tol,n_in_frame}` |
| A18 | σ_psfsys 三口径：r=10 px 0.257421/0.255662/1.377506；r=4 px 0.024998/0.040083/0.106078；真值 0.025633/0.034351/0.039417 | `results/step5_calibration_gate.json → frames[].items_measured.{sigma_psfsys_ap10_inframe,sigma_psfsys_inframe,sigma_psfsys_truth_mag}` |
| A19 | N1 乘性平场 0.045344→0.159590（3.52×），amp≥0.04 判红 | `results/step7_negatives.json → negatives[N1].{rows,response_ratio,pass_}` |
| A20 | N2 Poisson 天光 0.025199→0.048113→0.073354（2.91×），4× 判红；算术梯度对照 1.08× | `results/step7_negatives.json → negatives[N2].{rows,response_ratio,arithmetic_gradient_control}` |
| A21 | N3 判红阈值 0.04 mag = 真实颜色项的 6.97× | `results/step7_negatives.json → negatives[N3].{real_color_sigma_mag,discriminating_amplitude_mag,ratio_discriminating_over_real}` |
| A22 | N4 `k_ratio_measured = 1.6098633` vs 期望 1.6129032（偏差 0.188%） | `results/step7_negatives.json → negatives[N4].{k_ratio_measured,k_ratio_expected,relative_deviation_from_expected}` |
| A23 | N5 过裁剪：n 48→45→22→5，σ_obs 0.045344→0.003562，σ_floor → **−0.004911**，全 PASS | `results/step7_negatives.json → negatives[N5].{rows,pass_,finding}` |
| A24 | 退化族 `A·t/g = 225.056` 恒定；中位通量 4831.2/4865.1/4840.2 ADU；σ_obs 0.018911/0.018119/0.013438 | `results/step6_apply_and_units.json → unit_elimination.degenerate_family[]` |
| A25 | 零点平移不变量：shift 7.3 ⇒ Δlocation 0.8633228601（期望同）、k 比 0.1369863014、Δσ_residual = 0 | `results/step6_apply_and_units.json → unit_elimination.zero_point_shift_invariance` |
| A26 | apply：drizzle 尺度比 2.060596e-17（期望 2.060581e-17）；m 改正后残差 0.019732 vs 不改正 0.045344；rel = 0.0 | `results/step6_apply_and_units.json → apply.{downstream_consumption.drizzle_scale_ratio_median,apply_photometry.independent_recompute_max_rel_diff,magnitude_only.resid_mag_mad_sigma,resid_mag_mad_sigma_no_m}` |
| A27 | XP vs PHOTFLAM：F657N −0.14718±0.11307（n=42，MAD 0.49114，斜率 0.46382）；F673N −0.26301±0.12967（n=29，0.64544，0.76528）；F502N −0.08011±0.03226（n=75，0.21329，0.56457） | `results/step3_forward_vs_photflam.json → per_filter.{F657N,F673N,F502N}.{median_dmag,median_bootstrap_sigma,mad_sigma_dmag,color_slope_mag_per_mag,n}` |
| A28 | 干净子样：−0.013721/0.254622（n=12）、−0.224429/0.104200（n=12）、−0.006782/0.049218（n=18）；同星跨滤镜 n=19 中位 0.117104 / MAD 0.325773 | `results/step3_forward_vs_photflam.json → per_filter[].clean_sample 与 cross_filter.{n,median_diff,mad_diff}` |
| A29 | 枢轴波长相对差 3.73e-6/7.38e-6；反推有效面积 44425.5/45322.9 vs 几何 38453.1 cm²（1.1553/1.1787） | `results/step3_forward_vs_photflam.json → throughput_curves.validation[].{pivot_rel_diff,area_from_photflam_cm2,geometric_area_cm2,area_ratio}` |
| A30 | 判据汇总 8/9 PASS（G6 PARTIAL） | `results/gates.json → {n_gates,n_pass}`；`results/GATES.md` 表 |
| A31 | 星数不构成拒绝条件；星少到拟合不成立走 NO_DATA 拟合失败 | **正式文档**：`docs/science/PHOTOMETRY.md` §16.5 第 1–2 条（非 `results/` 实测） |
| A32 | 判据形态（σ_floor/σ_ceiling/ρ 系数） | **正式文档**：`docs/plugins/algorithms_phase1/06_photometry.md` §4.1；`results/step5_calibration_gate.json → criterion` |
| A33 | 生产代码 §4 求解前提 `kMinFitStars = 3`（不成立即 NO_DATA，非星数准入门槛） | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h`；`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `kMinFitStars` 引用 |
| A34 | 固定种子与复现入口 | `results/*.json → seed = 20260921`；`实验/photometric-magnitude/code/run_all.sh` |

## 附录 B · 复现命令

```bash
cd <repo root>
bash 实验/photometric-magnitude/code/run_all.sh          # 全量（约 10–20 min）
bash 实验/photometric-magnitude/code/run_all.sh quick    # 跳过 step6
# 单步
bash 实验/photometric-magnitude/code/step0_fetch_refs.sh            # 唯一需网络
python3 实验/photometric-magnitude/code/step1_analytic.py           # Oracle / 不变量 / 负例
python3 实验/photometric-magnitude/code/step2_hst_sim.py            # HST 前向仿真帧 A/B/C
python3 实验/photometric-magnitude/code/step3_forward_vs_photflam.py
python3 实验/photometric-magnitude/code/step4_guided_vs_blind.py
python3 实验/photometric-magnitude/code/step5_calibration_gate.py
python3 实验/photometric-magnitude/code/step6_apply_and_units.py
python3 实验/photometric-magnitude/code/step7_negatives.py
python3 实验/photometric-magnitude/code/step8_real_frame.py
python3 实验/photometric-magnitude/code/step9_collect.py            # → results/GATES.md
```

---

*本报告为只读精读产物：未运行任何 git 写操作，未修改 `code/`、`results/`、生产代码或正式科学文档；唯一新建文件即本文件。*
