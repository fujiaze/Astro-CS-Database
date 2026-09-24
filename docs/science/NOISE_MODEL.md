# Noise / Variance / Ivar / SNR Science (SCI-NOISE)

> 上游：ASTROCS_DESIGN.md §2.2（创新点二：跨帧绝对信噪比）、§4.2（Phase1 节点流程）

> ID: SCI-NOISE-001  范围: SCI-NOISE-001..015  状态: FROZEN（冻结定义）  上游: SCI-SCOPE-001  下游 ALG: ALG-NOISE-001..  模块: snr_estimator (NoiseWeightModelV1)

## 1 目的与非目标

- **目的**：估计校准后空背景随机分量的逐像素方差 `variance` 及其倒数 `ivar=1/variance`，作为 Phase2 逐像素科学权重入 `var(x,y)=a+b·x+c·y` 空间场 + 全局兜底，用于 UPM 控制光度拟合与加权积分。
- **非目标**：不估计测光零点残差散度（SCI-PHOT `sigma_residual` QA）；不输出 PSF 拟合质量 `q_psf`（SCI-PSF）；不生产完整协方差矩阵（Drizzle 后相邻像素相关见 `UNCERTAINTY_AND_COVARIANCE.md`）。
- **两个方差面（正向约束）**：本链承载**两个用途不同、量纲相同**的方差面，**不得互相替代**：
  1. **背景方差面**（本文件 §5 的 `variance`）：**空背景随机分量**，供天光建模、诊断与误差报告；
  2. **加权方差面**（§5c）：**该像素的总方差**，含源光子散粒项，供叠加与拟合的最优加权。
  `variance_floor`、三态（可用/不可用/损坏）、产品 dtype 成对不变量对**两个面**同等生效。
- **增益与读出噪声必须进入加权方差面（正向约束）**：`gain`（e⁻/ADU）与 `read_noise_e` 是加权方差面
  的**输入量**，其来源、缺省行为与不可得时的显式降级声明按 §5c；**禁止**把"配置里有这个键、
  但数值路径不读它"当作可用状态。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `x` | 像素值（校准后 ADU/e⁻ 空背景） | 输入 |
| `variance` | 随机分量方差 `σ_bg²` 或平面预测 `a+b·x+c·y` | `NoiseWeightModelV1.var` |
| `ivar` | `1/variance` (ADU⁻²) | `variance_bg_global` 倒数 / `fill` |
| `σ_bg` | `1.482602218505602·MAD(|x−median|)` | `noise_model.cpp:robust_sigma` |
| `MAD` | `median(|x−median(x)|)` | 同上 |
| `rmax` | 掩膜半径**硬上界** `max(1,r0)·max(1,scale)`（默认 60 px）；实际逐星半径 `r_i` 见 §5a | 掩膜 |
| `a,b,c` | 最小二乘平面 `var(x,y)=a+b·x+c·y` | `snr_noise_model_v1` |
| `variance_floor` | **可用方差**的下界（配置默认 `1e-12`，单位 ADU²；`max(var,floor)`）。**只作用于平面预测 > 0 的像素**；预测 ≤ 0 的像素方差不可用，产品面写 `variance=0 ∧ ivar=0`（§5/§7/§9）。非有限或 ≤0 一律显式拒绝，不产出模型（§4） | `default_config` |
| `g_model_floor` | 以 `model*` 为 key 的 floor 注册表 | `noise_model.cpp:33,48,55,86,903` |
| `gain, read_noise_e` | 增益 e-/ADU 与读出噪声 e-：**加权方差面**（§5c）的输入量；其解析式 `snr_noise_gain_variance` 与背景方差面交叉验证 | §5c / `snr_noise_gain_variance` |
| `r_inliers` | Tukey 权重>0 的内点集（SCI-PHOT 复用符号，不混） | QA |
| `degenerate, has_spatial_field` | 退化/空间场标志 | `NoiseWeightModelV1` |
| `r_local(F,FWHM,σ_bg)` | 由「掩膜边缘残余面亮度 = k·σ_bg」导出的无偏所需半径（§5a） | 掩膜 |
| `r_i` | 逐星实际掩膜半径 `clip(r_local(F_i,FWHM_i,k·σ_bg), r_min, rmax)` 经天空预算收缩 | 掩膜 |
| `k` | 掩膜边缘残余面亮度系数，默认 `0.1`（⇒ 残余方差污染 < 1%·σ_bg²） | `noise.mask_k_sigma` |
| `r_min` | 逐星半径下界 `max(1.5 px, 0.75·FWHM_i)` | 掩膜 |
| `N_sky` | 未被掩膜且合法（有限、未饱和）的像素数 | 天空预算 |
| `MASK_LEGACY / MASK_DEGRADED` | 掩膜降级诊断标（信息缺失 / 天空预算收缩后仍退化） | `NoiseWeightModelV1.mask_degraded` |

## 3 物理量和单位

- `x, σ_bg, √variance`: ADU（或 e⁻，同输入标度）；`variance`: ADU²；`ivar`: ADU⁻²；`a`: ADU², `b,c`: ADU²/pixel；`gain`: e-/ADU；`read_noise_e`: e-；`signal`: ADU；掩膜半径/坐标: pixel；`floor`: ADU²。

## 3a 坐标 frame

方差估计在**像素域**进行（8×8 patch 网格与平面场 `var(x,y)=a+b·x+c·y` 的 x,y 均为内部 0-based 像素坐标，GLOSSARY `pixel_coordinate`）；无 WCS/天球参与；帧身份沿用 `frame_id`（DATA_SEMANTICS §5），估计结果随帧 payload 唯一。

## 4 输入有效域

- 维度 `h>0,w>0`，`data` 非空且含有限值；`min_samples`（patch 样本数阈）默认 64；`rmax` 是**逐星掩膜半径的硬上界**（默认 60 px），实际半径 `r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)` 由源通量、PSF 尺度与天空预算导出（§5/§5a）；调用方应提供逐星通量与 FWHM（生产调用点 psf 块 `row[2]=flux` / `row[5]=fwhm`，见 §6），未提供时按 §5a 回调规则降级并置 `MASK_LEGACY` 诊断标。**饱和域（SAT-001）**：`x ≥ saturation_level` 的像素为**饱和像素**，**不参与 blank-sky 统计**（输入有效域规则，**无条件生效**；§5 的 5σ 裁剪不是它的替代品——裁剪的崩溃点是样本中位数，饱和核+源翼一旦占 patch 多数即双双失效）。电平来源优先级 = 显式 `cfg.saturation_level>0` > 帧元数据 FITS `SATURATE` > `DATAMAX`。**`0`/负/非有限 = 「未提供电平」（unset），不等于「无饱和」**；未提供时调用方**必须**在帧产品写显式降级声明 `NOISE_SATURATION_FILTER=DISABLED_NO_METADATA`（禁止静默），并由 §11 饱和域 oracle 的负例约束。<!-- 依据 DATA_SEMANTICS §13.1「data 行：饱和像素过滤不统计」、NOISE_ESTIMATION §13.4「饱和电平以上像素排除」；外部标准 LSST `ip_isr.IsrTaskConfig.doSaturation` 默认 `True` 且置 `SAT` 面（lsst/ip_isr isrTask.py L431-442）。 -->

- 平面场仅 `enable_spatial_field==1 && n_control_points>=4` **且控制点几何张成二维**时启用，否则退化为全局常量场（`has_spatial_field=0`）；几何判据 = 中心化控制点点云 Gram 矩阵特征值比 `λlo/λhi ≥ 1/16`（等价点云条件数 `κ=√(λhi/λlo) ≤ 4`，无量纲；绝对阈值 `|det|>1e-24` 已废除，DISP-NOISE-010）。
- `variance_floor` **必须有限且 > 0**（单位 ADU²）：非有限或 ≤0 时 build 与 fill **都**显式拒绝（`SNR_FLOOR_UNBOUND(-10)`），不产出模型、不静默回退到任何常数（`noise_model.cpp:369-371` build 侧、`:818` fill 侧）。`max(var,floor)` 只作用于**可用**方差（§5/§7）。
- `gain<=0` 时 `snr_noise_gain_variance` 返回 0；**加权方差面**（§5c）在 `gain` 不可用时必须显式降级并具名登记，**禁止**按 0 代入。

## 5 连续定义

```text
patch grid 8×8；星点掩膜**逐星半径** r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)，再按天空预算收缩（§5a）
σ_bg = 1.482602218505602 · median(|x − median(x)|)   # MAD→σ，Gaussian 假设
稳健裁剪: cosmic/hot 5σ 阈，≤2 轮
控制点: 合格 patch（样本数 ≥ min_samples）的 patch variance
空间场: 最小二乘平面 var(x,y) = a + b·x + c·y；**预测 ≤ 0 的像素 = 该处方差不可用
         ⇒ variance=0 ∧ ivar=0（§4a 显式不可用，**不得**由 clamp 产生、不得写 NaN）**；
         仅当预测为正且低于 variance_floor 时按 floor 夹逼（floor 是**数值保护**，见 §9）
几何退化: 控制点近共线 (λlo/λhi < 1/16) ⇒ has_spatial_field=0，fill 走全局常量场
全局兜底: 合格 patch variance 的稳健中位数 vmed
variance_bg_global = max(vmed, variance_floor)      # 合格 patch 支: vmed 已是方差 [ADU²]
variance_bg_global = max(sig², variance_floor)      # 无合格 patch 的全帧退化支: sig 是 σ [ADU]
g_model_floor: 以 model 指针为 key 注册 floor，snr_noise_model_v1_free 时按指针擦除，无全局共享
ivar = 1 / max(variance, floor)   # 仅对**可用**方差（预测 > 0）：fill 阶段 max(a+b·x+c·y, floor)
                                  # 预测 ≤ 0 ⇒ variance=0 ∧ ivar=0（不可用态，§7/§9）
                                  # control 点亦 max(patch_var, floor)（控制点方差恒 ≥ 0，属可用档）
```

```text
Gain/Readnoise 诊断模型 (仅 diagnostic, NOT FOR PRODUCTION):
  var_ADU = max(signal,0)/gain + (read_noise_e / gain)²   # signal: ADU, gain: e-/ADU（冻结换算）, rn: e-
  **加权方差面**（§5c）的源项与常数项来源；与背景方差面（§5，`source==0` empirical）交叉验证 (noise_model_science_test.cpp:238-272)，两面的基线口径不得互相替代
```

与 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:1-938` 一致（`fill_impl` :776-866；`snr_noise_model_v1_fill` :868-883）。

### 5a 掩膜半径的物理导出（MASK-002）

掩膜的唯一目的是让天空样本「无源」。「掩膜半径与星亮度解耦」**作为物理陈述是错的**：同一 `r=10 px` 下把最亮星通量从 `10³` 提到 `10⁶` ADU（FWHM=3 px，Moffat β=2.5，8 seeds），`σ_bg` 偏差从 `+0.02%` 升到 `+2.29%`，无偏所需半径 `6.6 → 28.1 px`（对数律）。正确口径 = 逐星半径由「掩膜边缘残余面亮度 ≤ k·σ_bg」导出（取 `k=0.1` ⇒ 残余方差污染 < 1%·σ_bg² ⇒ `σ_bg` 偏差 ≤ 0.5%，即本文件 §11 冻结 5% oracle 的 1/10 余量）：

```text
Gaussian : r_local = σ_p·sqrt(2·ln(F/(2π σ_p²·k·σ_bg)))
Moffat β : r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),   α = FWHM/(2·sqrt(2^(1/β)−1))

r_i = clip( r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax )
  k     = 0.1                        # noise.mask_k_sigma
  r_min = max(1.5 px, 0.75·FWHM_i)   # noise.mask_r_min_px / noise.mask_fwhm_floor_scale（至少覆盖 PSF 核心）
  rmax  = max(1,r0)·max(1,scale) = 60 px   # **硬上界**（保大 PSF 重翼），不是操作默认半径
天空预算收缩: 取最大 s∈(0,1] 使 n_qualified(s) ≥ 8 且 N_sky(s) ≥ 9216；无可行 s ⇒ rc=1（§7 空 support 不传播不变）
  预算推导（a priori）: 单 patch 相对误差 c ≈ 1.152/√N，中位数效率 1.25 ⇒ SE(σ̂)/σ ≈ 1.44/√N_sky；取 SE ≤ 1.5% ⇒ N_sky ≥ 9216
  ⚠ 单位（SCI-B D2 订正）：1.44/√N 是 σ̂ 的**相对**标准误 SE(σ̂)/σ（无量纲），**不是 dex**；
     若要以 dex 表述同一预算，须除以 ln10：SE_dex ≈ 1.44/ln10/√N ≈ 0.625/√N（两者差 ln10 = 2.3026 倍，禁止混用）。
回调（信息缺失，按序生效）:
  F_i 缺失        ⇒ r_i = 4·FWHM_i（实测 F ≤ 10⁵ ADU 时残余偏差 ≤ 0.70%），置 MASK_LEGACY
  FWHM_i 亦缺失   ⇒ 退回统一 rmax，置 MASK_LEGACY
  σ_bg 两遍法     ⇒ 第一遍 4·FWHM 估 σ_bg，第二遍回代（σ_bg 偏差 1% ⇒ r_local 偏差 ≈ 0.2%，可忽略）
```

实测（12 seeds；1024²/320 星；Moffat β=2.5；FWHM=3 px）：`r=10 px` ⇒ 偏差 `+0.13%`、RMSE `0.17%`、`n_qualified=64`；`r=60 px` ⇒ 偏差 `−0.28%`、RMSE `0.76%`（**4.6×**）、`n_qualified=35` ⇒ 60 px 在该帧是纯损失。可用域（λ = N_s·π·rmax²/A）：固定 60 px 要 `N_s ≤ A/7540`（4K² 2 225 星、1K² **139** 星、256² **8.7** 星），远窄于天文帧实际星密度 ⇒ 60 px 只能作**硬上界**。策略对照（54 帧/策略 = {256²,1K²,4K²}×{稀疏,中等,密集}×6 seeds，真值 σ_bg=5 ADU）：worst |σ 偏差| 现行统一 60 px **6.93%** / 显式失败 2.63% / 降级打标 8.44% / 仅按预算收缩 4.77% / **本节逐星自适应 1.11%**；平均零权重像素占比 14.8% → **0**；权场效率损失（含失权帧）13.28% → **0.013%**；方差梯度帧恢复比 **1.489–1.505**（真值 1.5）vs 现行常量场 1.000。来源 `reports/PROJECT-GOVERNANCE-01/research/MASK-001_掩膜语义重新推导.md` §3.3/§3.4/§5.3。

### 5b 噪声分类学与完整方差项（物理溯源）

本节给出探测器与天空端噪声项的**完整清单**、方差形式、帧内空间尺度与信号依赖，作为 §5 的物理依据。
它同时界定**本合同的 `variance` 面覆盖到哪一项为止**——§5 的 `variance` 面按本合同口径承载
**空背景随机分量**，不承载源项。

**首要分类轴是时间**：每一项都必须是**固定图案**（time-invariant，帧间相关）或**时变**（time-variant，逐帧独立）。
下表的"帧间相关性"列是该轴，不得省略。

| 项 | 物理来源 | 方差（ADU²） | 帧内空间尺度 | 帧间相关性 | 随信号 | 在背景方差面（§5） |
|---|---|---|---|---|---|---|
| 源光子散粒 | 源光子到达的 Poisson 统计 | `F·P(x,y)/g` | 逐像素（PSF 尺度） | 逐帧独立 | ∝ S | **不含**；加权方差面**必须含**（§5c） |
| 天光光子散粒 | 夜天光（气辉/黄道光/月光散射/光污染）+ 光学散射 | `S_sky(x,y)/g` | **方差图**大尺度；**实现**逐像素（ρ₁ ≈ 0） | **逐帧变**（月相/太阳角/气辉）；**帧内梯度方向亦可变** | ∝ S_sky | 是（经验 MAD 稳健方差） |
| 暗电流散粒 | 硅晶格热激发载流子 | `I_d(T)·t/g` | **方差图**近常数；**实现**逐像素 | 逐帧变（温度、曝光） | 常数 | 隐含（被经验总 rms 吸收） |
| 读出噪声 | 输出放大器复位/源跟随器热噪声 + 1/f；多放大器 ⇒ 分区 | `(RN/g)²`（1/f 分量**无解析方差式**，只可定性） | **方差图**常数（多通道为分区常数） | 1/f 分量**帧间相关**；白噪分量逐帧独立 | 常数 | 已含（σ_sky 声明 `EMPIRICAL_TOTAL_RMS`）；背景方差面禁融合解析式（§10），加权方差面按 §5c 显式承载 |
| 量化噪声 | ADC 均匀量化 | 见下方"文献口径"（**原文未核验**） | **方差图**常数 | 逐帧独立 | 常数 | 隐含（被经验总 rms 吸收） |
| 偏置/本底残差 | master bias 平均后残余、overscan 校正残余 | `σ_bias,res²` | 常数 + **确定性**低频结构（ρ₁ = 0.9995） | **帧间相关**（同一母版） | 常数 | 隐含（被经验总 rms 吸收） |
| 暗电流 FPN（DSNU） | 暗流非均匀 | `(D·D_N)²`，`D_N` = 0.1…0.4 | 固定图案 | **帧间相关**（固定图案） | ∝ 暗流 | 不含 |
| 乘性响应（PRNU/平场残差） | 像素间 QE 差异、尘埃影、照明不均、master flat 自身噪声与拟合残差 | `(ε·S(x,y))²` | PRNU 逐像素白 + 尘埃影/照明低频 | **帧间相关**（固定图案；重装光路后可变） | **∝ S²** | 不含 |
| 列固定图案（stripes） | 薄芯片干涉条纹、时钟馈通 | 按空间频率 | 高频周期（列向） | **帧间相关** | ∝ S | 不含 |
| 高光通量方差亏损 | 近饱和处有效方差低于低通量斜率的外推；相邻像素相关 | 亏损最多 **25%** | — | — | 非线性 | 不含 |
| 重采样相关 | 几何重采样 + drizzle 权重核 | `var_out = Σ c_j² v_j`（对角） | 核尺度（相关长度 1–2 px） | 由几何决定 | — | 方差传播已建模；**协方差非对角不落盘**（`docs/science/UNCERTAINTY_AND_COVARIANCE.md`） |

**不属方差项**：宇宙线、热像素、饱和、响应非线性是**污染与确定性截断**，按 §4 饱和域与 §5 的 5σ 裁剪处置；
**禁止**把它们登记为噪声项（热像素**不是**暗流散粒）。

**指纹判据**：`∝S` 是散粒（源 + 天光），`∝S²` 是乘性，`常数` 是读出/量化/偏置/暗流
⇒ **log-log 斜率 1 / 2 / 0** 把三类分开。该判据必须配**非退化控制**：真值无效应时斜率必须退化到预期值。
**适用域**：`var ∝ mean` 在**高通量端不普适**——近饱和处可亏损最多 25%，且相邻像素相关。

**「慢变」必须按谓词使用（三个谓词判决不同，不得混用）**：
- **方差图平滑**（`σ²(x,y)` 可当常数）—— **成立**。各噪声项对方差图的相对散布：读噪/量化/DSNU ≈ 0.000，
  天光/暗流散粒 ≈ 0.046，PRNU ≈ 0.092。
- **噪声实现空间平滑** —— **对全部随机项不成立**。空间 lag-1 自相关 ρ₁ ≈ 0：读噪 −0.0014、量化 0.0029、
  天光 −0.0029、暗流 −0.0029、DSNU 0.0008、PRNU −0.0039；仅确定性偏置结构 ρ₁ = 0.9995。
- **帧内常数或平面模型能移除噪声** —— **对全部随机项不成立**（移除比例 ≤ 1e−5）。

**解析律（把"慢变"变成可算量）**：散粒项 `relspread(σ²图) = relspread(电平)`，PRNU 为其 **2 倍**。
⇒ **方差图的平滑度是它所跟随的`电平场`的性质，不是噪声自身的性质。**

**⇒ 空间结构必须由 §5 的 patch + 平面场 `var = a + b·x + c·y` 承载**（该平面对 4.6% 倾斜有 18.87σ 功效）。
**禁止**以"慢变"为由把方差降为逐帧一个标量：天光梯度、DSNU、PRNU 的空间结构**都与源无关且可测**。

**可辨识性边界（解析结论，不是样本量问题）**：
- 模型 `μ = t·(S + D + B)` 且 `V = f(μ)`（`f` 为均值的确定性函数）⇒ `∂μ/∂θ_i = ∂V/∂θ_i = t` 对
  `θ_i ∈ {S, D, B}` **全部相同** ⇒ Fisher 信息 `I = c·ones(3,3)`，**恒为秩 1**。
- ⇒ **多帧、已知增益、已知读噪方差、改变曝光时长（即使 3000 倍杠杆）都不能打破该简并**。
- 零空间 `span{(1,−1,0), (1,1,−2)}`：沿其移动（真值暗流偏离 ±666%）时 `χ²` 变化**恒为 0**。
- **唯一**能把秩提到 2 的是**成分相对比例的改变**：加暗帧（`S = 0`）。第 2 个不同曝光的暗帧
  **不再提升秩**（`D` 与 `B` 仍以 `D + B` 的组合出现）。
- ⇒ 源的分离靠**支撑先验**（暗帧、平场、可信 `GAIN`），不靠任何统计量。
- 单帧**不可**分离"天光"与"源"（同波段、同为位置函数）；**多帧差分也不能**（两者一起对消、只留噪声）。
  mesh 背景是天光的低频代理，是**近似**不是分离。
- 模型 `V = σ0² + S/g`（`σ0²` = 全部常数项之和）是 **2 未知量、1 观测量**；有亮源杠杆臂时可辨识 `1/g` 与 `σ0²`（斜率与截距）。
- 需要绝对 `g` ⇒ 加平场对（PTC）或可信 `GAIN`，且 **PTC 前必须先减 bias**（否则斜率与截距都偏）。
- 需要 PRNU ⇒ 加平场（或大 `S` 处的 `∝S²` 项）。
- `Δ` 是**当前数据标度**上的 1 LSB：数据若已线性缩放到浮点（如母版 `ADU/65535`），量化方差必须随标度重算。

**文献口径**：暗流倍温律 `I_d(T) = I_d(T_ref)·2^((T−T_ref)/T_d)`——**`T_d` 是待测参数、无默认值**；
文献实测范围为 **6–8 °C 翻倍**，**不得**写死为 5–7 °C。
DSNU 与暗流散粒的区分是斜率：`∝D`（斜率 1/2）vs `∝D²`（斜率 1）。
**未核验声明**：量化项的具体形式（`1/12 DN²` / `(1/12)^{1/2}` / `(g²−1)/12 e⁻²`）与
暗电流 FPN 的 `(D·D_N)²` 引式**尚未取得原文**（EMVA 1288 正文需注册、Janesick 2007 付费墙）
⇒ **在核验前不得以"一手文献写…"的口吻断言**。1/f 噪声只有定性描述，**无可引用的解析方差式**。

**禁止**：把 `variance` 面（空背景口径）当作逐像素物理总方差场；用 `signal²` 反算或校验产品方差
（`docs/contracts/DATA_SEMANTICS.md` §31.1 数值层）；把上表"不含/隐含"的项声称已显式建模；
把"慢变"当作"可降为逐帧一个标量"的理由。

**负例（判据判别力）**：真值**平坦**的输入下 `variance` 面**不得**出现可测空间梯度（实测 0.91σ 不判红）；
真值**有 4.6% 倾斜**的输入下**必须**判红（实测 18.87σ）——该判据实测**非恒真**。
有源输入下稀疏 SNR 控制点**必须**出现随位置的可测变化（否则稀疏层退化，
见 `docs/science/CONTROL_WEIGHT_SNR.md` §2a/§4）。

### 5c 加权方差面（含源项）

本节定义**加权方差面**——供叠加与拟合的最优加权使用的**总方差**。它与 §5 的**背景方差面**是两个对象：
量纲相同（ADU²）、用途不同、**不得互相替代**。

**参考分解与归因**：像素测量值的参考分解为

~~~text
I = O + N_e/g + N_read,      N_e = N_src + N_sky + N_dark
~~~

`I` 像素测量值（ADU）、`O` 偏置（ADU）、`N_e` 光电子总数、`N_src` 目标天体光电子、
`N_sky` 天空背景光电子、`N_dark` 暗电流电子、`N_read` 读出噪声（ADU）。

**增益约定（正向约束）**：`g` 是**本仓冻结**的增益，单位 **e⁻/ADU**（换算 `ADU = N_e/g`）。
文献里同一物理关系常写成 `I = O + g'·N_e + N_read` 的**倒数形式**，那里 `g'` 的单位是 ADU/e⁻
且 `g' = 1/g`。**本仓一律用 e⁻/ADU**；引用文献公式时必须先确认其约定并换算，
**禁止**把两种约定混用（混用会让源项差 `g²` 倍）。单帧只提供均值与方差
两个可观测量，待分离量多于两个 ⇒ **禁止**声称从单帧"分解"出信号与噪声；本链只做**归因**
（把可观测量按物理来源指派到各项）。秩与零空间的代数依据见 §5b。

**加权方差的构成（正向约束）**：

~~~text
sigma_w^2(x,y) = sigma_bg^2(x,y) + S_src(x,y)/g
~~~

- `sigma_bg^2(x,y)` = §5 的**背景方差面**（空背景随机分量：天光散粒、暗流散粒、读出、量化、偏置残差）；
- `S_src(x,y)` = 该像素上**源贡献的信号电平**（ADU 标度，**不含天光**）；`S_src/g` 的单位是 ADU²
  （Poisson 方差在 ADU 标度下的形式，与 §5b 的 `F·P(x,y)/g` 同式）。**这是本面与背景方差面的唯一区别**；
- `S_src` 由**星点测光**给出的源通量与 PSF 轮廓导出，**禁止**由"从信号里减去噪声"得到。

**逐项口径**：§5b 表中标"在背景方差面：不含"的项里，**源光子散粒在本面必须含**；
DSNU、PRNU、列固定图案、高光通量方差亏损、重采样相关**在本面同样不含**（系统性/乘性项，
不属本层的随机加权方差）。

**量纲与标度**：`sigma_w^2` 与 `variance` 同为 ADU²；`g` 必须与 `sigma_w^2` 处于**同一标度**
（帧像素被线性缩放时 `g` 与 `sigma_w^2` 一并按同一因子换算，见 §3 与 §7 的产品 dtype 成对不变量）。

**增益与读出噪声的来源（正向约束）**：`g` 与 `read_noise_e` 的取值来源按以下优先级**显式声明**，
不得依赖单一来源：

1. **本帧自估（首选，不依赖任何帧头关键字）**：对帧内像素做 `V = sigma_0^2 + S/g` 的
   斜率–截距回归。该式是 **2 未知量、1 观测量**，故**必须**有**亮源杠杆臂**（跨越足够宽的
   `S` 量程）才能同时辨识 `1/g` 与 `sigma_0^2`；杠杆臂不足时回归**不可辨识**，
   必须显式判为不可得，**禁止**把弱杠杆臂的拟合值当有效标定。回归前**必须**先扣除偏置，
   否则斜率与截距同时有偏（§5b）。
2. **帧头关键字**：存在 `GAIN`/`RDNOISE` 一类关键字时作为**独立交叉校验**与可选覆盖，
   不因为是"帧头里的数"就免检。
3. **显式配置覆盖**：由调用方给出并随帧产品登记来源与出处。

**任一来源都不可得时本面必须显式降级并具名登记**，按 §5b 的适用域退化为背景受限口径；
**禁止**静默按 0 代入（`g=0` 使源项无定义），**禁止**把"配置里有这个键"当作可用状态。
本面的降级状态随帧产品落 provenance，与背景方差面的三态（可用/不可用/损坏）**正交**登记。

**数据面事实（正向约束）**：本项目正式数据面**不保证**存在增益/读出噪声关键字
（实测：M42 真实帧 124 个头部关键字中 `GAIN`/`GAINONE`/`RDNOISE`/`EGAIN` 全部缺失）。
⇒ 本面**不得**把"从帧头读 `GAIN`"设计成必经路径；自估路径（第 1 条）是本链在正式数据面上的
**主路径**，其可辨识条件由实验给出。

**文献锚（正向约束）**：本节的每一项与 `g` 的获取方式必须逐项标注**一手文献**出处与**适用域**
（教材/标准给 ISBN 或 DOI + 章节页码；经典论文给 DOI 或 arXiv 号）。未建立文献锚的项
**不得**进入生产数值路径。

**禁止**：
- 把背景方差面当作加权方差面——那会系统**低估**源受限像素的噪声、把权重抬高；
- 用 `signal^2` 反算或校验本面（`docs/contracts/DATA_SEMANTICS.md` §31.1 数值层）；
- 在未登记 `g` 来源与降级状态的帧上，声称本面为"绝对"口径。

### 5d 平面场的自适应约束（无标定常数）

背景方差面的空间场（§5 的 `var(x,y) = a + b·x + c·y`）必须满足以下**正向约束**。
其共同点是：**判据的零假设值由理论给定、拟合的可行域由数据给定，均不引入可调标定常数。**

- **控制点有效性（自校准）**：patch 的 MAD-σ 必须量的是**噪声**而不是**空间结构**。判据用自校准统计量
  `R = σ_MAD / σ_white-equiv`，其中 `σ_white-equiv` 是**白噪声等价估计**（由相邻像素一阶差分的稳健 σ 按 `1/√2` 折算，见 §5b 的噪声分类学）。
  **纯噪声的 `R` 理论值为 1**——这是恒等式，不是标定值。
  `R` 显著大于 1 ⇒ 该 patch 的「方差」被空间结构污染（如延展星云），**不得**进入平面拟合。
  「显著」必须由**样本自身散布**决定（例如以同帧内 `R` 的稳健下分位为基准、按其稳健散布定倍数），
  **禁止**写死形如 `R > 3` 的常数阈值。不变量：**纯噪声输入 ⇒ 不剔除任何 patch**。
- **拟合可行域（结构非负）**：拟合必须使平面在**控制点凸包内恒 ≥ 0**。允许的实现形态是**稳健拟合**
  （降低被污染控制点的杠杆）与**非负约束**（如对 `var` 的正参数化 / 非负最小二乘）的组合；
  **禁止**用「平面不可用时退回某个常数场」充当修复——那只是把一个标定值换成另一个标定值。
- **凸包外才是不可用**：仅当像素落在控制点凸包**外**（真正的边缘外推）且预测 ≤ 0 时，才按 §5/§9④
  编码为不可用态（`variance=0 ∧ ivar=0`）。凸包**内**出现大面积预测 ≤ 0 属**拟合缺陷**，
  必须按 §8 登记并 fail-closed，**不得**静默出片。
- **可观测量（必须写入 provenance）**：控制点方差的动态范围、平面系数、凸包内预测 ≤ 0 的像素占比、
  被剔除 patch 数与 `R` 的分布。**缺任一项即视为该帧的方差面不可审计。**

## 6 假设

- 空背景在 patch 尺度局部平稳；源星点可被**逐星半径掩膜**（§5a）与 5σ 裁剪分离；
- 掩膜半径**随源通量与 PSF 尺度变化**（§5a）：`r_i = r_local(F_i, FWHM_i, k=0.1·σ_bg)` 经天空预算收缩。模块 C ABI 只收坐标属**接口现状**，不是物理约束——生产调用点（orchestrator psf 块 9 列行：`row[2]=flux`、`row[5]=fwhm`、`row[6]=amplitude`，字段语义锚 `orchestrator.cpp:4558-4562`）本已持有该信息，ABI 已扩展为可选逐星数组（`star_flux`/`star_fwhm`）；缺省时按 §5a 回调规则降级并置 `MASK_LEGACY`；
- 增益/读出噪声的解析式仅在 `signal≈μ` 的 Poisson+读出噪声假设下有意义：它**不替代**背景方差面（§5）的经验方差，**只**作为加权方差面（§5c）的源项与常数项来源。

## 7 独立不变量

- **常量场不变量**：常数输入 `x=C` 时 `σ_bg=0` ⇒ `has_spatial_field=0, degenerate` 全局常量场，不产生伪梯度。
- **掩膜无偏性不变量**：默认掩膜下残余源污染对 `σ_bg` 的偏差 **≤ 2%**（§11 源污染 oracle 验收阈，12 seeds；`k=0.1σ_bg` 是设计取值，EXP-C 54 帧实测 worst 1.11%，MASK-002 门 512²/3 seeds 实测 worst 0.15%）；逐星半径 `r_i` 对 `F_i` 与 `FWHM_i` **单调不减**（同 `(F_i,FWHM_i,σ_bg)` 逐位可复现）。「半径与亮度解耦」**不是**不变量。
- **天空预算不变量**：任何掩膜方案必须留下 `n_qualified ≥ 8` 且 `N_sky ≥ 9216`；不满足 ⇒ 按 §5a 收缩半径；收缩到 `r_min` 仍不满足 ⇒ `ivar=0, r=1` 拒绝加权（「空 support 不传播」保持）。
- **空 support 不传播**：无合格 patch 时 `ivar=0, r=1` 拒绝加权，不产生伪有效权重；**对外产品面即 `variance=0 ∧ ivar=0`（禁写 NaN：NaN 保留给产品损坏，见 DATA_SEMANTICS §4a F-UNC-001）。**
- **Floor 夹逼不变量**：`variance_floor` 单位为 **ADU²**、配置默认 `1e-12` 属冻结项（变更须走工程变更）。clamp **只作用于可用方差**：任意**可用** `variance` 经 `max(..., floor)` 后 `variance ≥ floor` 且 `ivar = 1/variance` 有限。**不可用一律 `ivar=0`（不得由 clamp 产生）**。每帧生效 floor 及其来源（配置默认/注册表 key）必须随帧产品登记。
- **产品 dtype 成对不变量**：产品面 `(variance, ivar)` 必须落在两态之一——**可用** `variance>0 ∧ isfinite(variance) ∧ ivar=1/variance`（输出 dtype 表示精度内）；**不可用** `variance=0 ∧ ivar=0`。生效 floor 在与产品相同的 dtype 中不可表示时（如按 α² 换算后 `1e-46` 在 float32 下溢为 0，而 `1/floor` 上溢为 `+inf`），该像素取不可用态——**禁止**发布 `(0, +inf)` 这类自相矛盾的对。消费侧按数组标度消费时，必须把 floor 一并按 α² 换算（§3 量纲；生产消费点 `module_adapters.cpp` 的 `nm_data_scale`）。
- **量纲一致**：`variance` [ADU²] → `ivar` [ADU⁻²] 倒数关系精确，`gain` 模型量纲 `max(signal,0)/gain` [ADU²] 无量纲混。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 掩膜覆盖过大（收缩后仍 `n_qualified < 8` 或 `N_sky` 不足） | 先按 §5a 天空预算收缩逐星半径；收缩到 `r_min` 仍不可行 ⇒ `degenerate=1, ivar=0, r=1` 拒（不产生伪权重）；收缩生效但 `n_qualified < 8` ⇒ 置 `MASK_DEGRADED` | EXP-A/EXP-C/EXP-D（MASK-001 §3.2/§3.4/§5.3） |
| 无合格 patch（收缩后仍无） | `degenerate=1`；若**全部未掩膜** sky 样本 < `max(min_samples, 9216)` ⇒ `ivar=0,r=1` 拒（**收紧**：现行 `min_samples/2=32` 像素可为整帧定权重，现按 `SE(σ̂)/σ ≈ 1.144/√N_sky ≤ 1.5%` 要求 `N_sky ≥ 9216`，与 §5a 同一预算常数，）；否则 `degenerate=1` 全局常量场 `has_spatial_field=0, r=0` fallback 并置 `MASK_DEGRADED` 诊断标 | `noise_model.cpp:530-585` |
| 全帧 NaN/饱和 | 饱和像素按 §4「饱和域」剔除；**全帧无任何合法 sky 样本**（NaN+饱和全剔，或样本 < §5a 预算）⇒ `degenerate=1, ivar_bg_global=0, r=1`；**电平未提供（unset）时本行的饱和支不可达**——这正是 §4 强制显式降级声明的理由 | noise_model.cpp:122-126,530-585（valid_pixel/collect_patch_sky）；SAT-001 |
| `variance_floor` 非有限或 ≤0 | build 与 fill **都**显式拒绝：`SNR_FLOOR_UNBOUND(-10)`，不产出模型、不静默回退常数 | `noise_model.cpp:369-371,818,889` |
| `gain<=0` | `snr_noise_gain_variance` 返回 0 | `noise_model.cpp:928-936`（判据 :931） |
| `star_x/y` 非有限 | 掩膜跳过该星，不污染统计 | 参数校验 |
| `MAD=0` | `σ_bg=0` ⇒ 退化路径（见上） | `robust_sigma` |
| **平面在控制点凸包内大面积预测 ≤ 0**（拟合缺陷，非边缘外推） | 该帧方差面**不可审计**：必须登记「凸包内预测 ≤ 0 占比」「平面系数」「控制点方差动态范围」「被剔除 patch 数」并 **fail-closed**，不得静默出片。产品编码仍按 §5/§9④ 逐像素取不可用态 | §5d |
| **控制点方差动态范围极端**（结构污染使 `R` 远大于 1） | 污染 patch 必须按 §5d 的自校准判据剔除后再拟合；剔除后仍极端 ⇒ 同上行 | §5d |

## 9 精度策略

- FP64 全链路；MAD 常数**唯一权威写法** `1.482602218505602`（= 1/Φ⁻¹(3/4) 的 double 字面量；11 位简写 `1.4826022185` 只能出现在"约等于"语境，与之绝对差 **5.602e-12**、相对差 **3.779e-12**）；`q_psf` 的 `0.7316727929211932`（10–90% trimmed mean \|residual\| →σ）与上述 MAD 常数不可互换（前者 trimmed mean，后者 MAD）。
- `variance_floor` 保证**可用方差**的 `ivar` 有限。要求逐条：
  ① `variance_floor` 的配置默认值与单位（ADU²）是冻结项，**生效值**及其来源必须随帧产品登记（§7 Floor 夹逼不变量）；
  ② 生效 floor 在**与产品相同的 dtype** 中必须可表示。绝对常数地板无法同时服务 ADU 与 α² 两个标度（float32 最小次正规 ≈ `1.4e-45`：`1e-12` 经 α²=1e-34 换算后为 `1e-46`，在 float32 下精确下溢为 0）⇒ **换算由消费侧承担**：以非 ADU 标度消费方差数组的调用方必须把 floor 一并按 α² 换算；
  ③ 若换算后 floor 在该 dtype 中仍不可表示，或 `variance`/`ivar` 在该 dtype 中下溢为 0 / 上溢为非有限，则该像素取**不可用态**（§7 产品 dtype 成对不变量），**禁止**发布 `(0, +inf)`；
  ④ **平面预测 ≤ 0 ⇒ 该像素方差不可用（`variance=0 ∧ ivar=0`），不得 clamp 成 floor**（`noise_model.cpp:830-864`；判据 `p1noise_negative` 的 `n7_plane_pred_unavailable`、`n7b_dtype_underflow_pair`）。
- 5σ 裁剪 ≤2 轮，避免过度剔除。

## 9a 专属问题回答（SCI-003 指定问题逐项）

- **signal/noise/blank sky**：`x`=校准后空背景像素值（ADU 同标度）；noise=空背景随机分量；blank sky 样本域=星点**逐星半径掩膜**（§5a，硬上界 `rmax`）+ 5σ≤2 轮裁剪后的合格 patch（§5）。
- **sigma_cal_rel / 零点标准误（消费侧口径，SCI-PHOT 引用）**：`sigma_cal_rel = ln10·sigma_residual` 是**逐星定标散度**（dex → 相对），**不是**零点（median 位置）的不确定度；零点统计标准误为 `sigma_location_se_dex ≈ 1.253·sigma_residual/√N_eff`（`1.253=√(π/2)`，median 的位置标准误），`sigma_location_se_mag = 2.5·sigma_location_se_dex`（实现 `snr_phot_cal_quality`；代码已按此实现，本行仅补文档口径，）。
- **σ_sky 入参口径与 c_est 单位（SCI-B 定案，防双计）**：① 逐像素噪声组合 `σ_i² = σ_sky,i² + (RN/g)² + F·P_i/g`，读噪只出现一次；`sigma_sky_adu` 的语义必须显式声明为 `shot_noise_only`（天光+暗流散粒）或 `empirical_total_rms`（经验总 rms，含读噪）——前者才叠加 `(RN/g)²`，后者不得叠加；声明与实际来源不一致 ⇒ fail-closed。实测双计使 σ_F 高估 +12.8%~+34.0%（`实验/absolute-snr/results/b2_noise_terms.json`），插件侧口径落点 `docs/plugins/algorithms_phase1/07_noise_snr.md` §4.2a。② 本文件 §5a 的 `1.44/√N` 是**相对**标准误（无量纲），dex 口径为 `1.44/ln10/√N ≈ 0.625/√N`；把它当 dex 阈值用会高估噪声项 2.3026 倍（EXP-205 的 τ_A/τ_B 即此误，见 `实验/absolute-snr/results/DOC_CORRECTIONS.md` D2）。
- **SNR**：本合同不产出 SNR 图。消费侧可以构成**逐像素探测显著性** `signal/√variance`，但该量**不含源泊松项**（`variance` 仅空背景），不是源的通量信噪比；源通量 SNR 必须另行定义（逐源 `σ_F`，见 CONTROL_WEIGHT_SNR.md §1）。本层唯一产出为 `variance/ivar`（GLOSSARY `variance/ivar`）。
- **variance/ivar**：`variance`=ADU²（平面场或全局兜底），**可用**像素 `ivar=1/max(variance,floor)` 精确倒数（§7 量纲不变量）。两态穷尽：**平面预测 ≤ 0 或产品 dtype 不可表示 ⇒ `variance=0 ∧ ivar=0`（不可用态）**；`variance_floor` 非有限或 ≤0 ⇒ build/fill 显式拒绝 `SNR_FLOOR_UNBOUND(-10)`（§4/§8）；非有限输入在参数域拒绝（§8）。
- **Poisson+read noise**：`var_ADU=max(signal,0)/gain+(read_noise_e/gain)²` 是**加权方差面**（§5c）的源项与常数项来源；**背景方差面**（§5）的唯一基线为 empirical MAD（`source==0`），两者不得互相替代。
- **权重归一与适用域**：`ivar` 作为 Phase2 逐像素科学权重直接入加权（归一在消费侧 `Σw/Σ`），适用域=空背景随机分量；不含源泊松项/系统项/协方差（Drizzle 后相关见 UNCERTAINTY_AND_COVARIANCE.md）；`ivar=0` 显式表示不可用，禁止伪装（§7 空 support 不传播）。

## 10 不可接受变化

- 把 `snr_noise_gain_variance` 一类解析式融合进**背景方差面**（§5）：该面的唯一基线是 empirical MAD（`source==0`），即使帧头有 gain 也不融合；加权方差面（§5c）按该节口径**必须**含源项与常数项；
- 在**未按 §5a 扩展 ABI 提供逐星通量/FWHM 且未重冻结**的前提下，擅自把半径改为按亮度自适应（**正确做法是 §5a**——禁止的是无依据的自适应，不是自适应本身）；
- 取消或绕过天空预算判据（`n_qualified ≥ 8`、`N_sky ≥ 9216`）而直接产出权重场；
- 改变 `variance_floor` 默认值 `1e-12` 或 `g_model_floor` 的指针 key 隔离语义；
- 将 `q_psf`/`photometric scatter` 混为逐像素 `variance`；
- 改变 `8×8` patch 网格或 `5σ ≤2 轮` 裁剪策略而无 SCI 变更。

## 11 验证 Oracle

- **Gaussian 合成**：`N(0,σ²)` 空背景合成帧（`σ=5 ADU`），经验 `σ_bg` 在 `5%` 内复现（`SNR-004`）。
- **Poisson 交叉**：`μ/gain + rn²/gain²` 的 `var_th` 与经验 `variance_bg_global` 在 5% 内一致（`SNR-005, 238-272`），用于**背景方差面**的独立交叉验证，不改变该面的基线口径。
- **平面场恢复**：注入线性梯度 `var(x,y)=a+b·x+c·y` 场，拟合 `a,b,c` 在 10% 内复现（`SNR-006`）。**逐像素真值口径**：`max|v̂/v_true−1|` 的**解析真值必须显式含 patch 内天光梯度项** `((dμ/dx)²+(dμ/dy)²)·P²/12`（`P` = patch 边长），否则对强梯度帧**过严**——E12 实测**漏项时判 +49.9%（假红）**、含项（本实验解析项 = 26.04 ADU²）后 **+3.56%/+3.67%/+4.05%**（≤10% 门）；等价可接受口径 = 「**同几何独立参考**」。系数恢复门（`|b̂/b−1|`、`|ĉ/c−1|` ≤10%）不受影响。
- **不变性门**：常量场、空 patch 拒绝、`floor` 夹逼、量纲 `ivar=1/var` 四门（`TST-NOISE-INV-*`）。**平面不可用态门（能红能绿）**：① 正例——预测 ≤ 0 的像素 `variance==0 ∧ ivar==0`，预测 > 0 的像素与独立 LS oracle 逐位一致；② 负例——把不可用像素 clamp 成 floor（`variance==floor ∧ ivar==1/floor`）必须判红，且生效 floor 在输出 dtype 中下溢时必须判红 `(0, +inf)` 这类对（`p1noise_negative` 的 n7/n7b；故障注入 `p1noise_selfcheck` 验证判据非恒真）。
- **源污染 oracle（MASK-001）**：合成帧 = `N(0,5²)` 空背景 + `N_s` 颗 Moffat(β=2.5) 星（幂律亮度 `dN/dF ∝ F⁻²`，`F∈[2×10²,10⁵]` ADU，FWHM=3 px，星位随机）；默认掩膜下 `|σ̂_bg/σ_bg − 1| ≤ 2%`（12 seeds）且 `n_qualified ≥ 8`、`N_sky ≥ 9216`。**负例（门必须能红）**：① 半径固定 60 px 且 256²/50 星 ⇒ 必须 `rc=1`（整帧退化）；② 半径固定 2 px 且 `F_max=10⁶` ADU ⇒ 必须检出 `|σ 偏差| > 2%`。③ 半径与 `F_i`/`FWHM_i` 不单调 ⇒ 必须红（`o2_mask_radius_monotone`）。生产门见 ALG §13.4 TEST-NOISE-DESIGN-001 的 FIX-NOISE-D/H。
- **饱和域 oracle（SAT-001）**：校准后饱和平台不是常数——平台电平 `SAT` 经平场除法带响应残差 `ε`（相对 1% ⇒ 平台散布 `0.01·SAT`，对 sky 仅 `0.01·μ`），故「平台像素」在值域上是**有噪声的整段总体**，不能指望 5σ 裁剪兜底。判据：**正例**（电平已提供）⇒ 污染控制点数不增加、权场中位数方差与平台 patch `ctrl_variance` **严格下降**、帧平均 `ivar` ≥ 3× 未过滤臂；**负例（门必须能红）** ① 电平未提供（`saturation_level=0`）且帧含亮星饱和核（512²，平台半径 44 px，掩膜 6 px，`F=5×10¹³ ADU`）⇒ 平台 patch `ctrl_variance` `6.87×10⁸ → 1.74×10⁸ ADU²`（提供电平后仍受源翼限制，故只判严格下降）、帧平均 `ivar` 比 `0.218`（丢失 4.6× 权重）、`sigma_bg_global` **两臂均 5.08 ADU（差 0%）**——缺陷只在逐像素权重场可见，这正是它的静默性；**域与限制**：现行实现以**校准后**值判饱和，平台经平场响应残差 `ε` 抹平后 `x ≥ 电平` 只能移除平台总体的一部分，全平台 patch 的干净恢复必须靠 §5a 掩膜半径（MASK-002），本过滤**不是**掩膜的替代品；② 帧无 `SATURATE`/`DATAMAX` 时必须存在显式降级声明且**不得**被读成「无饱和」。生产门 `ctest -R p1noise_saturation`（含 `p1noise_saturation_wiring`）。
- **Python 参考**：NumPy 对同 `data` 的 `median/MAD/5σ裁剪/平面最小二乘` 复算 `variance/ivar`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-NOISE-001` `snr_noise_model_v1` 空背景方差估计（8×8 patch + MAD + 平面场）
- `ALG-NOISE-002` `snr_noise_model_v1_fill` 平面场填充：可用像素 `max(预测,floor)` + `ivar=1/var`；预测 ≤ 0 或产品 dtype 不可表示 ⇒ `0/0`
- `ALG-NOISE-003` `snr_noise_gain_variance`：**加权方差面**（§5c）的源项/常数项解析式，兼作背景方差面的交叉验证（SNR-005）

## 13 追溯与测试

- 权威文件: `docs/science/NOISE_MODEL.md` (SCI-NOISE-001..015)
- 实现: `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` (`snr_noise_model_v1, _f64, _fill, _free, snr_noise_gain_variance, g_model_floor`), `lib/algorithms/noise_snr/cpp/include/snr_estimator.h`
- 公开 API: `snr_noise_model_v1, snr_noise_model_v1_f64, snr_noise_model_v1_fill, snr_noise_model_v1_free, snr_noise_gain_variance, snr_noise_model_v1_default_config`
- 测试: `TST-NOISE-001..015` (`noise_model_science_test.cpp`), `TST-NOISE-INV-*` 四门、不变量，`TST-NOISE-FAIL-*` 空 patch/NaN 拒绝（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Newberry, M. V. 1991, PASP, 103, 122（DOI 10.1086/132801，SCI-001 已核验原文存在性）：该文的贡献是 **processing noise F** —— 含 bias、dark count、preflashing 与像元灵敏度差异（"field flattening"）的校正；§3.2 逐字「the greatest noise contribution, results from flat-field division」。本合同引它支持 §5b 的**乘性/平场项是主要噪声贡献**。本合同 §5 诊断公式为 Project-defined，不引用其具体公式号。**禁止**把它列为"Poisson+读出噪声分解"的来源。
2. MAD→σ 换算 `1.482602218505602 = 1/Φ⁻¹(3/4)`：标准正态 MAD 分位恒等式（`Φ⁻¹(3/4) = 0.6744897501960817`，double 逐位等于 `1/1.482602218505602`），教科书级，Project-defined 采纳。SCI-PHOT 侧的 4 位写法 `0.6745` 与全精度值相对差 **+1.5196e-05**（等价地 `1/0.6745` 相对差 **−1.5196e-05**），属该侧容许截断，**不得与本节冻结值互换**（V12-N-03，）。
3. Tukey biweight 内点权重（`r_inliers` 复用）：SCI-PHOT §14/PMS 文献链，本层仅消费 QA 集合不重复估计。
5. 掩膜半径默认值的导出依据（MASK-002）：以 §11 源污染 oracle 为判据，`k=0.1`、`r_min=max(1.5 px, 0.75·FWHM)`、硬上界 `rmax=max(1,r0)·max(1,scale)=60 px`、`N_sky ≥ 9216`、`n_qualified ≥ 8`。`k=0.1` 下 `r_local(F=10⁵ ADU, FWHM=3 px, β=2.5) = 17.6 px`（Gaussian 极限同阶）；实测 `r=10 px` 偏差 +0.13%（RMSE 0.17%），统一 60 px 在 1024²/320 星上 RMSE 0.76%（**4.6×**）、在 256²/50 星上整帧退化（rc=1）。**导出链**：`10`/`6`/`60 px` 由 `noise_model.cpp:703-704` → `NOISE_ESTIMATION.md:143` → `eng/packaging/config/defaults.json` 的 `source_ref` 三段登记，推导依据 = 本文件 §11 源污染 oracle。
4. 默认值 `min_samples=64` 的导出依据：8×8 patch（P=64）下以本文件 §11 冻结的 5% oracle 为判据，`min_samples=5` 时单 patch 偏差 −19.2%、全局 `sigma_bg_global` 偏差 −25.1%、5% 门通过率 **0.6%**；`min_samples=64` 为 −1.25%/−1.7%、通过率 **92.8%**（纯高斯蒙特卡洛；常规无掩膜帧上 5 与 64 逐位同输出，差异只在 patch 残余样本 5~63 的掩膜 regime）。判据 = 本文件 §11 冻结的 5% oracle。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5/§5a 公式与常数。

- **MAD→σ 常数 1.482602218505602 = 1/Φ⁻¹(3/4)**：标准正态分位恒等式；稳健性/有限样本校正见 Rousseeuw & Croux 1993, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。
- **稳健尺度与 σ-clipping**：Hoaglin, Mosteller & Tukey (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。
- **背景网格 + 稳健 σ 估计**：Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（**§2** 背景网格与稳健估计：k-σ 裁剪、`mode = 2.5·median − 1.5·mean`、中值滤波、双线性插值、32–128 像元网格；DOI 10.1051/aas:1996164。§3 讲的是**检测**（峰值/阈值、Lutz 单遍连通域、template frame 卷积），不是背景）；源码 SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）back.c/makeback。**差异**：SExtractor 用 mode/median 与迭代 σ，AstroCS 用 8×8 patch 的 MAD + 最小二乘平面场，二者**不等价**（网格尺寸、尺度估计器、场基不同），引用仅作方法学对照。**适用域（不得外推）**：该文**不讨论权重图、逆方差加权或逐像元方差通道**，不得引它支持本项目 `variance/ivar` 面。
- **多尺度稳健噪声（MRS/N*）**：Starck, J.-L. & Murtagh, F. 1998, “Automatic Noise Estimation from the Multiresolution Support”, PASP 110, 193（DOI 10.1086/316124，starlet 小波；PixInsight ImageWeighting §2.4 的 MRS 出处）；Starck, J.-L. & Murtagh, F. 2006, Astronomical Image and Data Analysis, 2nd ed., Springer（ISBN 978-3-540-33023-3）Ch.2–3；Starck, Donoho & Candès 2003, A&A 398, 785（DOI 10.1051/0004-6361:20021569）。**核验状态**：文章级；AstroCS 现状**未采用**小波 MRS/N*，该条只作选型对照。
- **Poisson+read noise 诊断式**：Janesick 2001 SPIE PM83 Ch.2；Howell 2006 Handbook of CCD Astronomy Ch.4。（Newberry 1991 的贡献是 processing noise 与平场项，**不属**本条，见 §14 item 1。）
- **饱和过滤**：LSST ip_isr（GPL-3.0）IsrTaskConfig.doSaturation 与 SAT 面；FITS SATURATE/DATAMAX 关键字（FITS Standard）。
- **掩膜半径的解析导出**：Gaussian/Moffat 轮廓尾翼积分属 Project-defined 推导（§5a）；Moffat 轮廓出处见 Moffat 1969, A&A 3, 455；PSF 尺度与 FWHM 换算见 docs/science/PSF.md §5。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

### 14a.1 PixInsight N* 常数与本项目口径

- PixInsight 官方 N* 稳健噪声（.pidoc 式[14][15]）：`N*_MAD=2.48308·MAD(R*)`、`N*_Sn=2.03636·S_n(R*)`（`S_n` 为 Rousseeuw & Croux 1993 尺度估计；常数用 10000 幅 4096² 高斯白噪声 bootstrap 标定）。**AstroCS 未采用这两个常数**：本项目用标准正态 MAD 一致化 `1.482602218505602`（§9），与 PI 的 2.48308 定义域不同，**不得互换**。
- PCL 2.10.4 `PSFSignalEstimator.h` 的 `NStar()` 默认取 `NStar_Sn`（2.03636），而 `Estimates::NStar` 文档注释写 2.05435——PI 自身存在版本/注释冲突（登记 UNRESOLVED U3）；本分片在 4×10⁶ 高斯样本上复核标准 Sn 的 σ 一致化为 1.1926，未能复现 2.03636。
- 逐像素 ivar 的开源对照（锚点均在固定 commit 上逐字核对，行号随版本漂移）：
  - **SWarp**（GPL-3.0，commit `2f7e8b6` = `2.41.5-35-g2f7e8b6`，`src/coadd.c:1279-1311`；tag `2.41.5` 上为 :1282-1314）：权重是**逐像素通道**（`WEIGHT_TYPE ∈ {BACKGROUND, MAP_RMS, MAP_VARIANCE, MAP_WEIGHT}`，后三者要求与科学图同尺寸的权重图；不存在帧级标量 σ 权重模式）。内存缓冲装**方差**：`outwpix = 1/Σ(1/var_k)`；默认输出权重场为 `WEIGHT_FIELD`，写盘时经 `var_to_weight()`（`src/weight.c:334-354`）转成 **ivar = Σ(1/var_k)**（`src/coadd.c:808`）。逐帧标量 `sigfac` 只是乘在整张逐像素图上的归一化因子。
  - **SExtractor**（GPL-3.0，commit `90296de`，`src/analyse.c:200-203,304-310`）：`sigtv` 是方差累加器。**两条互斥路径**——无权图（`gainflag==0`）在通量层加 `F_tot/gain`，即 `Σ σ_bkg² + F_tot/gain`；有权图且 `WEIGHT_GAIN=Y`（默认）在逐像元层加 `F_pix/gain·var_pix/backnoise2`，严格式为 `Σ var_pix·(1 + F_pix/(gain·backnoise2))`。本文件 §5 的 `Σ(σ_bkg²+F_pix/gain)` 是两者的公共近似，只在方差图被标定到 `var ≈ backnoise2` 时逐字成立；`DETECT_TYPE=PHOTO` 路径（`var2 = pix²·var`）不适用。
  - **SEP**（LGPL-3.0，commit `93b3ac5`，`src/aperture.c:516-570`）：孔径方差 `σ²_sum = Σ var_pix·w + Σ/gain`，`w` 是**面积分数**（过采样分支 `w = (1/subpix)²`、整像元分支 `w = 1`），**只乘一次**、无平方；掩膜改正是线性缩放。仅当 `subpix=1`（`w ∈ {0,1}`）时 `w² = w`。
  - **photutils**（BSD-3-Clause，tag `3.0.0`/`2.3.0`，`photutils/utils/errors.py:91-92`；2.2.0/2.1.0 → :92-93、1.5.0–2.0.2 → :88-89）：`σ_tot² = σ_bkg² + I/g_eff` 逐字成立，实现为 `calc_total_error` 返回 `sqrt(bkg_error² + data/g_eff)`。**粒度**：这是**逐像元**误差，**不含**孔径求和与面积权重项，不得直接当孔径方差用。
  与本文件 §5 诊断式同构，可作对拍基线；引用时**必须带版本**，且不得据「同构」推断各实现的适用域相同。
- 背景/权重重标定先例：SWarp `RESCALE_WEIGHTS`（GPL-3.0，commit `2f7e8b6`，`src/back.c:361-389`）把 `sigfac` 置为**逐背景网格** `σ_mesh / sqrt(W_mesh)` 的**中位数**（`field->sigma` 为科学图背景网格实测 σ，`wfield->back` 为权重图自身背景网格值；前导非正值剔除，全坏则 `sigfac=1.0`）。**适用域**：只对 `VAR_FIELD`/`WEIGHT_FIELD` 两类输入生效（`MAP_RMS`/`BACKGROUND` 不进入）；`sigfac` 是**逐帧标量**，作用是该帧方差图整体乘 `sigfac²` 并把 `var_thresh` 同乘 `sigfac²`。可作为本项目权重/方差标定门的对照。

## 15 Acceptance

- §11 Oracle 全过：Gaussian 5% 复现、Poisson 诊断 5% 交叉（仅诊断）、平面场 10% 恢复、四不变量门、Python 参考 rtol 1e-9、**源污染 oracle（含星帧，正例 + 三条负例；）**、**饱和域 oracle（正例 + 两条负例；）**；
- §11 平面不可用态门全过（`ctest -R p1noise_negative`；含 `n7_plane_pred_unavailable` 与 `n7b_dtype_underflow_pair`）：预测 ≤ 0 的像素 `variance==0 ∧ ivar==0`、预测 > 0 的像素与独立 LS oracle 逐位一致、产品面不存在 `(0, +inf)` 这类对；两档各自非空（判据非恒真），并由 `ctest -R p1noise_selfcheck` 的故障注入证明该判据能红；
- §8 全部退化路径显式（无合格 patch/NaN/floor/gain≤0）；饱和过滤状态在帧产品里**显式可读**（`NOISE_SATURATION_FILTER`，）；
- `eng/tools/science_contract_lint.py` PASS（15 节 + ID + 锚点）；
- 解析不变量→SYN-003 转换：Gaussian/Poisson/常量/blank sky/outlier/small-N 用例、estimator bias 与 ivar 边界（零/负/NaN→ivar=0）登记 SYN-003。
