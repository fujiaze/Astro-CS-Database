# Photometry Science (SCI-PHOT)

> 上游：ASTROCS_DESIGN.md §2.1（创新点一：测光星等坐标系）、§4.4（输出合同）

> ID: SCI-PHOT-001  状态: FROZEN (T103 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-PHOT-001..  模块: photometric_calib (flux_calibrator)

## 1 目的与非目标

- **目的**：将仪器流量 `F_instr` 校准到**锚在 Gaia XP 绝对分光刻度（CALSPEC 溯源）的模型通带**光度尺度；模型通带当前为 `T(λ)·Q(λ)·λ`，**不含光学系统透过率与大气消光**（记为未建模项）。在模型通带内的结果可称「绝对通量（Gaia XP 刻度）」；跨通带/换系统/波段外通量不在本合同范围。估计零点 `location`、尺度因子 `scale` 及残差 QA `sigma_residual / sigma_mag`。
- **非目标**：不处理带通外颜色项高阶效应（仅 QA 暴露残差分布）；不估计逐像素噪声方差（SCI-NOISE 边界）；不做大气消光时变建模。
- **测光标定语义（claim `FIX-SCI-SNR-CANON-001`；不可协商）**：标定目标是**真实测光坐标系（星等）**，手段 = 星点光通量积分 + Gaia + CCD QE + 滤镜透过率曲线；**消除物理单位，只使用星等**。标定因子（本文件 `scale`、Phase1 标定面 `k_photo`）的**绝对值无物理意义**——它把增益/口径/曝光等**未知量全部吸收**；**禁止**用任何物理闭合式（如 `k = g·h·c·1e9/(A·t)`）反推仪器参数或论证其合理性（设计前提 = **FITS 头拿不到这些量**）。**有意义的判据只有一条（尺度无关）**：**测光一致性**——施加后星点**星等**与 Gaia 的残差（散度/MAD）必须小。
**「帧间一致性」（各帧 `k`/`scale` 落在同一测光体系）是语义目标与报告字段，不是门禁判据**
（负责人 §9.49 定案 2「这玩意应该是帧间独立的，为啥要组间对比」；变更 claim `PHOT-GATE-DROP-001`）。
门只有一个 = 单帧标定是否可信，与其它帧无关；跨帧 `k` 不同是正常的。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `F_instr` | 仪器通量 (ADU；e⁻ 需 gain，当前不可得) | 输入 |
| `F_syn` | 合成通量 `∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（**不含** `10^(−0.4·G)`；口径与量纲见 §2a） | 输入（单位：W·m⁻²·nm） |
| `G` | Gaia G 星等（`gaia_source.phot_g_mean_mag`；XPSD 记录内 `mag_raw×0.001−1.5`） | 星等一致性 `delta`、`ZP_syn` 诊断；**不进入 `F_syn`** |
| `r_i` | `log10(F_instr/F_syn)` dex | 定标核心 |
| `delta_i` | `−2.5·log10(F_instr)−G_Gaia` mag | 星等一致性 |
| `location` | IRLS/Tukey 稳健位置（dex） | `star_matcher.cpp:478-525` |
| `scale` | `10^{−location}` 校正因子 `I_cal=I·scale` | 输出 |
| `S` | `MAD(r)/0.6744897501960817` 初值尺度 (dex) | `star_matcher.cpp:21-27` |
| `c` | Tukey 形状参数 `4.685` | 同上 |
| `sigma_residual` | `MAD(r_inliers)/0.6744897501960817` dex | QA |
| `sigma_mag` | `2.5·sigma_residual` mag | QA |
| `mag_tolerance` | 星等一致性阈 `3.0 mag` | `pc_api.cpp:139,398` |
| `psf_status,qf` | 饱和/质量标志 | `snr_estimator` |

## 2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）

> 变更 claim `PHOT-FSYN-CANON-001`（任务 SCI-PHOT-FORMULA-01）。本节把**参考通量的合成口径**写成显式正向约束并逐条给出证据；**不改** §5 的 IRLS/Tukey 定义、§7 不变量、§10 禁改清单与任何阈值/容差。

### 2a.1 定义式

```text
F_syn = ∫ F_λ(λ) · T(λ) · Q(λ) · λ dλ            # W·m⁻²·nm
```

**必须是什么**

- `F_λ(λ)` 是参考星的**绝对**谱辐照度，单位 **W·m⁻²·nm⁻¹**；来源 = Gaia DR3 XP 采样均值谱（官方字段 `flux`，声明单位 `Flux[W m-2 nm-1]`、自述 "Externally-calibrated combined BP and RP flux"）。
  - 本仓生产消费的是该官方产品的**第三方再编码容器**（PixInsight XPSD，见 §14 第 11 条）：官方以 float32 直接给出 `flux`，容器以 **uint8 + 逐星 float32 量化参数** 存储，解码式 `F_λ = byte·flux_mul + flux_min` 是**本仓合同约定**（`gaia_client.c` / `DATA_SEMANTICS.md` §14.2），其**绝对刻度由本轮真实数据实证锚定**（§2a.6），不依赖对容器内部约定的信任。
  - 官方产品本身**可以含负值**（外部定标谱在低信噪波段出现负通量，不做裁剪）；因此"解码后出现负值"不是容器缺陷，非正 `F_syn` 由调用方有效域判据拒绝（§4/§8）。
- `T(λ)` 是滤镜透过率，无量纲，值域 `[0,1]`；来源 = 本帧配置声明的滤镜型号在 `filters.json` 中的曲线。
- `Q(λ)` 是探测器量子效率，无量纲，值域 `[0,1]`；**是通带的组成部分**（见 §2a.4），不是可选装饰。
- `λ` 与 `dλ` 单位 **nm**；`λ` 因子即光子计数约定（CCD 计量光子数，单位波长间隔的光子数 ∝ `F_λ·λ/(hc)`；`1/(hc)` 是与星无关的常数，被零点吸收，故不显式写出）。
- **`F_syn` 中不出现 `G`**：Gaia G 星等既不作乘性因子、也不作归一化因子进入参考通量。

**不成立的条件（退化到「不能定标」）**

| 条件 | 后果 |
|---|---|
| 通带与光谱网格**完全不重叠**（`T(λ)Q(λ)≡0` 于整个网格） | `F_syn ≡ 0`；`F_syn>0` 的有效域判据拒绝**全部**参考星 ⇒ 定标无输入，必须报拟合失败（`NO_DATA`/`zero_point_valid=false`），**不得**给出零点或星等 |
| 解码后 `F_λ` 出现负值且积分 `F_syn≤0` | 该星被有效域判据剔除（§4）；`F_syn` 本身不做非负钳位，非正值由调用方拒绝 |
| 通带显著超出 XP 覆盖（330–1050 nm） | 网格外无数据，`T·Q` 置 0 而**不外推**；颜色项误差上升，结论不成立（§2a.5）。官方口径同此：只有**完全落在** 330–1050 nm 内的通带可复现 |
| 参考星亮度超出 XP 谱的发布范围 | 全部均值谱限 `G < 17.65`、采样表示限 `G < 15`（§2a.5）；超出者无参考谱 ⇒ 不进入定标 |

### 2a.2 量纲逐项推导

| 因子 | 量纲 | 说明 |
|---|---|---|
| `F_λ` | `W·m⁻²·nm⁻¹` | 绝对谱辐照度（XP 采样均值谱官方单位） |
| `T(λ)` | `1` | 无量纲透过率 |
| `Q(λ)` | `1` | 无量纲量子效率 |
| `λ` | `nm` | 光子计数权重 |
| `dλ` | `nm` | 积分元 |
| **`F_syn`** | **`W·m⁻²·nm⁻¹ · nm · nm = W·m⁻²·nm`** | 与 §2 符号表、`DATA_SEMANTICS.md` §14.3 一致 |

- 物理含义：`F_syn = hc · N_γ`，其中 `N_γ` 为通带内**光子计数率**（photons·s⁻¹·m⁻²），`hc` 以 `J·nm` 计（`hc = 1.986445857e-16 J·nm`）。因此 `F_syn` 与光子计数率**只差一个与星无关的常数**，这正是 §1 所述「常数由零点吸收」的量纲依据。
- `F_instr` 单位 ADU，与 `F_syn` **量纲不同**；`r_i = log10(F_instr/F_syn)` 是有量纲比值的对数，`location` 单位 `dex(ADU/[F_syn 单位])`，`scale = 10^{−location}` 单位 `[F_syn 单位]/ADU`（§3）。

### 2a.3 与官方定义的对应关系

Gaia DR3 官方文档 §5.4.1「External Calibration → Zero points」给出合成通量的官方定义（式 5.41，VEGAMAG 平均能量）：

```text
⟨f_λ⟩ = ∫ f_λ(λ) · S(λ) · λ dλ / ∫ S(λ) · λ dλ        # 官方 (5.41)
```

本合同的 `F_syn` 是**同一被积函数**（`S(λ)` ↔ 本合同的 `T(λ)·Q(λ)`）、**去掉归一化分母**的写法：`F_syn = ⟨f_λ⟩ · ∫T(λ)Q(λ)λdλ`。

- 分母 `∫T(λ)Q(λ)λdλ` **与星无关**（同一帧内 `T、Q` 相同）⇒ 它是一个**逐帧常数**，在 `r_i` 中表现为 `location` 的平移、在 `ZP_syn` 中表现为常数偏移，**不改变 `sigma_residual` 与任何散度判据**。
- 因此「绝对归一化不可辨识」与「通带形状必须正确」是两件事：前者由 `location` 吸收，**后者不可**（见 §2a.5）。
- 官方同节明确：Gaia 星表发布的积分通量以 photo-electrons s⁻¹ 计，其零点「are not suitable for synthetic photometry computations」；做合成测光必须按官方 (5.43) 用**同一通带**重算零点。

### 2a.4 `Q(λ)` 的规范地位

- `Q(λ)` **是通带的组成部分，不是可选项**。官方对 passband 的定义（Gaia Collaboration, Montegriffo et al. 2023, A&A 674, A33, §1）："actual TCs, which in the following we also refer to as passbands, are defined by the combination of the TC of an optical filter …, the sensitivity curve of a photon-counting detector (typically a CCD for observations in the optical spectral range), and the TC of the optical elements …, plus a contribution from the terrestrial atmosphere"。
- 本合同的模型通带 `T(λ)·Q(λ)·λ` 因此**必须**含 `Q(λ)`；未配置 `Q` 时按 `Q(λ)≡1` 处理，其物理含义是「假设探测器为**理想量子效率平坦**器件」，**不是**「QE 已被折进 T(λ)」。
- 缺失 `Q` 的量级（真实 M42 视场，26211 颗 XPSD 星，G∈[6,16]，n=10348）：`Q≡1` 与计入 KAF-16803 QE 的合成星等差**中位 +0.811 mag**（被零点吸收）、**跨星散度 0.0082 mag**（不被吸收，进入 `sigma_residual`）。证据：`run/SCI-PHOT-FORMULA-01/evidence/c2_negative_controls.json → N4_QE_missing`。
- 未配置或解析失败时**必须**显式告警，使「没配 QE」与「QE 已计入」在下游可区分（`frame_photometry_fit.cpp` QE 分支）。

### 2a.5 通带失配的误差量级（为什么通带形状是地基）

- 官方与文献一致：XP 合成测光的**绝对刻度上限 ≈ 1%**，而**现实精度由通带失配支配**。官方文档 §5.4.1："Thus 1 % is thought to be the current state-of-the art uncertainty on the 'absolute' calibration scales."；A33 摘要："Existing top-quality photometry can be reproduced within a few per cent over a wide range of magnitudes and colour, for wide and medium bands, and with up to millimag accuracy when synthetic photometry is standardised with respect to these external sources."
- **XP 刻度本身的精度边界（文献）**：Montegriffo et al. 2023, A&A 674, A3 §8.1："for wavelengths higher than λ ≃ 400 nm the accuracy of the calibration is mostly enclosed in the ±2% level"；同节 "systematic errors in the absolute flux scale (which can be present at the 1% level)"；§8.1.1 给出颜色项（λ ≲ 400 nm 越蓝越大）、BP 560–600 nm 亮端 "up to −2% at the bright end"（G ≲ 11）、RP 950 nm "∼3% at G ≃ 4"；§3 定标源对 CALSPEC 的 "flux accuracy of about 1%"。⇒ 参考通量的**绝对**分量有 ~1–2% 的刻度不确定性；该分量在 `r_i` 中是**与星无关的常数**（被 `location` 吸收），**不**进入 `sigma_residual`；进入散度的是**通带形状**项。**注意区分**：XP 的**连续表示**是内部系统（单位 e⁻·s⁻¹，无物理刻度），本仓消费的**采样表示**才是外部定标（W·m⁻²·nm⁻¹）——两者不是同一产品。
- **本仓真实数据定量**（M42，同一批 26211 颗 XPSD 星、同一积分约定，只换通带曲线；证据 `run/SCI-PHOT-FORMULA-01/evidence/c2_negative_controls.json → N5_band_mismatch`）：

  | 通带 | 点数 | 波长范围 | 光子加权有效波长 λ_eff |
  |---|---|---:|---:|
  | `Baader R`（配置声明，正确） | 73 | 572–716 nm | 643.4 nm |
  | `Antlia V Pro Series B`（误取） | 53 | 420–524 nm | 470.3 nm |

  两者合成星等差 `Δm`（G∈[6,16]、解码谱处处为正，n=8406）：**中位 −0.978 mag、跨星散度（1.4826·MAD）0.449 mag**。这个散度**不被任何单标量零点吸收**，直接进入 `sigma_residual`。
- **判据参照**：本仓 49 帧 M42 真实数据的逐帧 `2.5·sigma_residual_dex` 中位由（误取通带）**0.42720 mag** 降到（正确通带）**0.04505 mag**，比值 **9.5×**；而本表独立算出的通带失配散度 **0.449 mag** 与之同量级 ⇒ 两者互为独立佐证（`实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md` §3；本轮 `d1_zp_sigma_rederive.json`）。
- **量级的适用域（不得混用）**：文献中的「通带失配」指**同名通带的曲线差异/微小偏移**，其定量量级为 **0.05–0.1 mag**（Bessell 1990：`"nonlinear deviations of up to 0.1 mag"`、`"systematic differences up to 0.05 mag"`）、**2–5%**（Stubbs & Tonry 2006 引 Saha et al. 2005：`"systematic discrepancies at the 2-5% level. They attribute these discrepancies to passband differences"`）、**~5 mmag**（Burke et al. 2017：`"less than 5 mmag"`，其通带形状精度 `"better than 0.1%"`）、**7 mmag**（Souverin et al. 2024, StarDICE III：中心波长 0.2 nm / 宽带通量 7 mmag）。本节的 **0.449 mag** 来自**取到另一支滤镜**（λ_eff 差 173 nm），**远在该文献区间之外**；因此该数字的用途是「证明通带身份错误不可接受」，**不得**被引作「同名通带失配」的典型量级。

### 2a.6 判定证据汇总（四类）

| 类别 | 证据 | 位置 |
|---|---|---|
| 官方定义 | Gaia DR3 文档 §20.12.4（`flux` 字段单位 `Flux[W m-2 nm-1]`、"Externally-calibrated"、采样网格 "343 values from 336 to 1020 nm with a step of 2 nm"）；§5.4.1 式 (5.41)（含 `λ` 的合成通量定义）与 "1 %" 绝对刻度上限 | 见 §14 第 4–6 条 |
| 文献 | Gaia Collaboration, Montegriffo et al. 2023, A&A 674, A33（合成测光；passband 定义含探测器 QE）；Montegriffo et al. 2023, A&A 674, A3（XP 外定标）；Bessell & Murphy 2012, PASP 124, 140（photonic passband） | §14 |
| 合成实验 | 直接链接生产 `spectrum_integrator.cpp` 的解析探针：常数谱 × 常数 `T` × 常数 `Q` 与闭式解**逐位一致**（rel = 0 / 1.4e−16）；误差分解给出 2 nm 网格离散误差 0.66–1.34%、XPSD uint8 量化误差 0.025%；通带全在网格外 ⇒ `F_syn≡0`；`flux_mul≤0`/非有限 ⇒ 显式 0 | `run/SCI-PHOT-FORMULA-01/evidence/c1_analytic_check.json`、`c1_probe_raw.tsv` |
| 真实数据 | 26211 颗真实 XPSD 星：用官方 G 通带（Riello et al. 2021）+ GaiaXPy `Gaia_DR3_Vega` 零点 −26.4899 复算合成星等，`median(m_syn−G) = −0.0037 mag`、MAD 0.0033 mag（G∈[6,18]、解码谱处处为正，n=11272）；乘 `10^(−0.4G)` 的变体偏移 **+16.34 mag** | `run/SCI-PHOT-FORMULA-01/evidence/a1_xpsd_absolute_check.json`、`a2_bandpass_and_absolute.json` |
| 真实数据（生产链） | 生产落盘 `ZP_syn` 由 §2a.1 公式**逐位复现**：T2/M1 落盘 `−15.126346726632235` vs 复算 `−15.126346726631280`（Δ=9.5e−13，n=2338）；T3/M1 落盘 `−15.123241368129857` vs 复算 `−15.123241368086541`（n=2309） | `run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json` |
| 官方工具旁证（独立子代理核验，本任务未复跑） | GaiaXPy 2.1.4 `calibrate()` 对 `XP_CONTINUOUS` 系数产出的绝对采样谱 vs 官方 `XP_SAMPLED` 产品 `flux`：比值中位 `1.000000`、最大相对差 `1.01e−4`（float32 存储精度）；源码 `src/gaiaxpy/spectrum/sampled_spectrum.py:114` 为纯线性组合 `coefficients @ design_matrix`，**不含任何星等因子** | 见本任务回执「独立查证子代理」节 |


## 3 物理量和单位

- `F_instr`: ADU（e⁻ 需 gain，当前不可得）；`F_syn`: **W·m⁻²·nm**（`F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`，`F_λ` 单位 W·m⁻²·nm⁻¹、`λ` 与 `dλ` 单位 nm；逐项量纲见 §2a.2）；二者**不同量纲**，其比值的对数即 `location`（见 DATA_SEMANTICS §14.3）；`r, location, S, sigma_residual`: dex（`r_i = log10(F_instr/F_syn)` 是**有量纲比值**的对数，`location` 单位 dex(ADU/[F_syn 单位])，`scale = 10^{−location}` 单位 [F_syn 单位]/ADU）；`delta, sigma_mag`: mag；`sigma_cal_rel`: 相对误差（`sigma_cal_rel = ln10·sigma_residual`）；`qf` 无量纲标志。

## 3a 坐标 frame

光度定标**不做空间坐标变换**：参考星表为 Gaia DR3（ICRS/J2000，与 WCS 输出同系）；交叉匹配沿用 WCS 求解后的天球坐标（SCI-WCS），本层只工作在 flux/星等域；帧身份沿用 `frame_id`（DATA_SEMANTICS §5）。

## 4 输入有效域

- 每颗星 `F_instr>0, F_syn>0` 有限值；饱和星（`psf_status!=0` 或 `qf & (SATURATED|HAS_SATURATED)!=0`）不进入匹配与定标，计 `rejected_quality`（`star_matcher.cpp:35-40`）。
- 参考星数 `|r_consistent|>=3` 才进 IRLS，否则 `NO_DATA`；`|r_inliers|>=2` 才估计 `sigma_residual`，否则 `sigma_residual=0`（`552-559`）。
- `S>0` 时迭代 `max_iter=50, tol=1e-6`；`S==0` 跳过 IRLS 取 `median(r)`（`478-525`）。

## 5 连续定义

```text
r_i = log10(F_instr,i / F_syn,i)                         # dex

# 星等一致性预过滤 (进入 IRLS 前)
delta_i = −2.5·log10(F_instr,i) − G_Gaia,i
median_delta = median(delta)
预拒绝 i  若  |delta_i − median_delta| > mag_tolerance    # mag_tolerance=3.0

# IRLS + Tukey biweight (对 r_consistent)
S = MAD(r_consistent)/0.6744897501960817,  location_0 = median(r_consistent)
迭代直到 |loc_new−loc_old|<1e-6 或 50 步:
  u_i = (r_i − location)/(c·S),  c=4.685
  w_i = (1−u_i²)²   (|u|<1),  0 否则
  location = Σ w_i·r_i / Σ w_i
若 S==0 ⇒ location = median(r_consistent), robust_iterations=0

scale = 10^{−location}          # I_cal = I·scale
sigma_residual = MAD(r_inliers)/0.6744897501960817   # r_inliers={i|w_i>0}
sigma_mag = 2.5·sigma_residual
outlier_rate = 1 − |r_inliers|/|r_consistent|
```

与 `lib/algorithms/photometry/cpp/src/star_matcher.cpp:21-27,435-525,552-559`（IRLS/Tukey/scale）及 `lib/algorithms/photometry/cpp/src/pc_api.cpp:139,398`（`mag_tolerance=3.0` 实际传入点；`star_matcher.cpp:241-248` 仅为 `psf_valid` 诊断）一致。

## 6 假设

- 参考端 `F_λ` 是 Gaia DR3 XP 采样均值谱的**外部定标**（绝对）谱辐照度，单位 W·m⁻²·nm⁻¹；本合同据此在**模型通带 `T(λ)·Q(λ)·λ` 内**做正向合成（§2a）。
- **绝对归一化不可辨识，通带形状必须正确**：`F_syn` 与官方「平均通量」只差一个与星无关的分母（§2a.3），该常数被 `location` 吸收，故本合同**不宣称产物处于绝对通量刻度**（产物以星等/相对星等表达）；但通带形状（含 `Q(λ)`）**不被吸收**，其失配直接进入 `sigma_residual`（§2a.5）。
- **模型通带不含光学系统透过率与大气消光**（显式未建模项，跨帧会成为帧间系统差）；`Q(λ)` 缺失时按 `Q≡1`（显式未建模项，§2a.4）；大气/仪器零点在观测尺度稳定；饱和判据可靠（`psf_status==0` 且无 `SATURATED` 标志）。
- **适用域**：
  - 通带必须被 XP 覆盖（330–1050 nm）**完全包含**；官方口径同此（Montegriffo et al. 2023, A&A 674, A3 §8.4："any photometric system whose passbands are fully enclosed in the 330-1050 nm wavelength range covered by Gaia BP and RP spectra"）。本仓消费的采样网格为 336–1020 nm、步长 2 nm、343 点；网格外 `T·Q` 置 0 而**不外推**。
  - 参考星必须在 XP 谱的发布范围内：全部均值谱 `G < 17.65`；**采样表示**（本仓消费的那一支）官方只对 `G < 15` 给出（Montegriffo et al. 2023 附录 B："including only sources brighter than G = 15 mag"）。本仓 XPSD 解码的一致性实测在 `G ∈ [6, 18]` 内为 `median −0.0037 mag`；更暗端逐星量化参数使解码谱出现负值、散度迅速发散（`a2_bandpass_and_absolute.json → a_absolute_check_by_magbin` 的 `[18,22]` 档 median `−2.75 mag`）⇒ 该区间**不在适用域内**。

## 7 独立不变量

- **零点平移不变量**：`F_instr` 全体同乘因子 `k` 时 `location` 增 `log10 k`，`scale` 相应除 `k`，`sigma_residual` 不变。
- **尺度单调性**：`F_instr/F_syn` 比值越大 `location` 越大，非饱和样本集下单调。
- **鲁棒性**：注入 20% 离群 `r` 时 IRLS `location` 变化 `<0.1 dex`（Tukey 权重截断）。
- **S=0 退化**：全体 `r` 相等时 `S=0` ⇒ `location=median(r)`，不迭代。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| 无星/星数不足 | 返回 `NO_DATA` | `star_matcher.cpp:478` 前校验 |
| `S==0` (MAD=0) | 跳过 IRLS，`location=median(r)` | `star_matcher.cpp:552` |
| 饱和/质量异常 | 不参与定标，计 `rejected_quality` | `psf_status!=0 \|\| qf&SATURATED` |
| 预过滤全拒 | `r_consistent` 空 ⇒ `NO_DATA` | `mag_tolerance=3.0` 判别 |
| 非有限 `F` | 显式拒绝，不进入 `r` | 参数校验 |
| 通带与光谱网格完全不重叠（`T·Q≡0`） | `F_syn≡0` ⇒ 有效域拒绝全部参考星 ⇒ 拟合失败（`NO_DATA`/`zero_point_valid=false`），**不产出零点** | §2a.1；`run/SCI-PHOT-FORMULA-01/evidence/c1_analytic_check.json`（`F_band_outside_grid`）、`c2_negative_controls.json`（`N3`：26211 星中 `F_syn>0` 者 **0**） |
| `Q` 曲线落在光谱网格之外 | 同「通带不重叠」：`Q` 重采样后处处为 0 ⇒ `F_syn≡0` | `c1_analytic_check.json`（`C2_QE_outside_grid`） |
| 解码 `F_λ` 含负值使 `F_syn≤0` | 该星被 `F_syn>0` 判据剔除（`F_syn` 不做非负钳位） | `c1_analytic_check.json`（`G_negative_decode`：返回 −4.6375e−10，未钳位） |
| `flux_mul≤0` 或量化参数非有限 | `F_syn` 显式返回 `0`（参数非法） | `spectrum_integrator.cpp:435-438`；`c1_analytic_check.json`（`G2`/`G3`） |

## 9 精度策略

- FP64 对数空间；flux 比值不经 `ivar` 加权（ivar 不用于此层）；IRLS 收敛阈 `1e-6` dex，迭代上限 50。

## 9a 专属问题回答（SCI-002 指定问题逐项）

- **WCS frame/pixel convention**：不在本层处理；交叉匹配依赖 SCI-WCS 输出的 ICRS/J2000 坐标（§3a）。
- **PSF 参数**：星点通量来自 PSF 拟合域（PSF.md），饱和判据 `psf_status/SATURATED` 决定剔除（§4）。
- **aperture/flux/background**：`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）；无孔径背景扣除项——背景已在 PSF/测光上游处理；`F>0` 有限值为有效域，非有限显式拒绝（§8）。
- **photometric scale 与不确定度**：`scale=10^{−location}`（IRLS/Tukey 稳健位置）；不确定度 `sigma_residual`（dex）→`sigma_mag`（mag）→`sigma_cal_rel`（相对）；`q_psf`/`sigma_residual` 禁止混为逐像素 ivar（§10）。

## 10 不可接受变化

- 改变 `c=4.685`/`tol=1e-6`/`max_iter=50`/`mag_tolerance=3.0` 阈值而无 SCI 变更；
- 将 `q_psf` 或 `sigma_residual` 混为逐像素 ivar；
- 忽略饱和/质量标志使饱和星进入定标；
- 用物理闭合式反推仪器参数（增益/口径/曝光）或据此论证 `k_photo`/`scale` 的合理性；
- 为 `k_photo`/`scale` 设**绝对窗口**（绝对值依赖未知仪器参数，不存在有意义的绝对窗口；验收只用**测光一致性**；「帧间一致性」是语义目标/报告字段，**不是门禁**，见变更 claim `PHOT-GATE-DROP-001`）。

## 11 验证 Oracle

- **合成注入**：已知 `scale` 的 `F_instr=k·F_syn` 注入场，估计 `location≈log10 k`（`rtol 1e-4`）。
- **鲁棒门**：注入 20% 离群点，`location` 偏差 `<0.1 dex` 且离群权重为 0。
- **S=0 门**：常数 `r` 场直接取 median 通路，不迭代。
- **Python 参考**：NumPy `median/MAD/Tukey` 对同 `r` 复算 `location/scale`（`rtol 1e-9`）。

## 12 关联 ALG ID

- `ALG-PHOT-001` IRLS-Tukey 零点估计
- `ALG-PHOT-002` 星等一致性匹配与 QA (`sigma_residual/sigma_mag/outlier_rate`)

## 13 追溯与测试

- 权威文件: `docs/science/PHOTOMETRY.md` (SCI-PHOT-001)
- 实现: `lib/algorithms/photometry/cpp/src/star_matcher.cpp` (`location/S/scale, mag_tolerance, IRLS`), `lib/algorithms/photometry/cpp/src/pc_api.cpp` (`pc_calibrate_simple`)
- 公开 API: `lib/algorithms/photometry/cpp/include/photometric_calib.h` (`pc_calibrate_simple, pc_calibrate_simple_with_gaia`)
- 测试: `TST-PHOT-001` 合成零点、`TST-PHOT-INV-001` 鲁棒性、`TST-PHOT-FAIL-001` 饱和拒（新增/映射见 `docs/TRACEABILITY.csv`）

## 14 Primary literature（引用定位声明）

1. Tukey biweight `c=4.685`（95% 高斯渐近效率）：Mosteller & Tukey 1977, *Data Analysis and Regression*；定位经 [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/) 实证核对（"c=4.685 yields 95% asymptotic efficiency at the Gaussian"）。
2. MAD→σ 换算 `1/0.6744897501960817 = 1.482602218505602`：标准正态 MAD 分位恒等式（`Φ⁻¹(3/4) = 0.6744897501960817`，double 逐位等于 `1/1.482602218505602`；与 `docs/science/NOISE_MODEL.md` §14 同值），教科书级恒等式，Project-defined 采纳。4 位截断写法 `0.6745` 与全精度值相对差 **+1.5196e-05**，只允许出现在「≈」语境并标注该偏差，不得再作权威值（W4-A6 处置 V12-N-03）。
3. Gaia DR3 合成通量参考：Gaia Collaboration 星表发布文献——bibcode 级定位（未逐页核验），本合同仅消费星表数值，不转述其定标推导。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 公式与常数。

- **Tukey biweight w=(1−u²)²、c=4.685（95% 高斯渐近效率）**：Beaton, A. E. & Tukey, J. W. 1974, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）；Mosteller & Tukey 1977；Huber & Ronchetti 2009, Robust Statistics, 2nd ed., Wiley（ISBN 978-0-470-12990-6）。
- **MAD→σ 换算 0.6744897501960817 = Φ⁻¹(3/4)**：标准正态分位恒等式；稳健性讨论见 Rousseeuw & Croux 1993, JASA 88, 1273（DOI 10.1080/01621459.1993.10476408）。
- **最优提取（PᵀC⁻¹P 结构）**：Horne, K. 1986, PASP 98, 609（DOI 10.1086/131801）；Naylor, T. 1998, MNRAS 296, 339。**边界**：二者为已知 profile/方差下的最优提取；AstroCS 本层做的是 Gaia 交叉定标的零点/尺度，不是逐源最优提取（§1 非目标），引用只作统计结构对照。
- **误差口径 FLUXERR/MAGERR 与孔径改正**：Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（SExtractor；DOI 10.1051/aas:1996164）；photutils（BSD-3-Clause）aperture_photometry 的误差传播。**差异**：AstroCS 的 sigma_residual 是**逐星定标散度**（dex），不是 SExtractor 的单源通量误差；两者语义不同，禁止互换（NOISE_MODEL §9a）。
- **Gaia XP 绝对分光刻度与 CALSPEC 溯源**：Gaia Collaboration et al. 2023, A&A 674, A1（Gaia DR3）；Gaia Collaboration et al. 2021, A&A 649, A1（EDR3）；Bohlin, R. C., Hubeny, I. & Rauch, T. 2020, AJ 160, 21（DOI 10.3847/1538-3881/ab94b4）；Bohlin et al. 2014, AJ 147, 127（DOI 10.1088/0004-6256/147/6/127）；Bessell, M. & Murphy, S. 2012, PASP 124, 140（DOI 10.1086/664083）。
- **4. XP 采样均值谱的官方定义与单位（§2a.1/§2a.3 的一手依据）**：ESA Gaia DR3 官方文档 §20.12.4 `xp_sampled_mean_spectrum`，https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html 。**原文**："This is the BP/RP externally calibrated sampled mean spectrum. All mean spectra are sampled to the same set of absolute wavelength positions, viz. 343 values from 336 to 1020 nm with a step of 2 nm."；字段表 **"flux : mean BP + RP combined spectrum flux (float[] array, Flux[W m-2 nm-1]) Externally-calibrated combined BP and RP flux."**（2026-09-23 抓取核验，HTTP 200）。
- **5. 合成通量的官方定义式与绝对刻度上限（§2a.1/§2a.5 的一手依据）**：ESA Gaia DR3 官方文档 §5.4.1 *Photometric processing → Calibration → External Calibration → Zero points*，https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html 。**原文（式 5.41）**："in VEGAMAG system the mean energy per wavelength units ⟨f_λ⟩ is calculated as: ⟨f_λ⟩ = ∫ f_λ(λ) S(λ) λ dλ / ∫ S(λ) λ dλ"；**原文（绝对刻度）**："Thus 1 % is thought to be the current state-of-the art uncertainty on the 'absolute' calibration scales."；**原文（零点适用面）**：Gaia 星表通量以 photo-electrons s⁻¹ 发布，其零点 "are not suitable for synthetic photometry computations"（2026-09-23 抓取核验，HTTP 200）。
- **6. XP 合成测光与 passband 定义（§2a.4/§2a.5 的一手依据）**：Gaia Collaboration, Montegriffo, P., Bellazzini, M., De Angeli, F., et al. 2023, A&A 674, A33（DOI 10.1051/0004-6361/202243709；arXiv:2206.06215）。**原文（摘要）**："Synthetic photometry directly tied to a flux in physical units can be obtained from these spectra for any passband fully enclosed in this wavelength range."；"Existing top-quality photometry can be reproduced within a few per cent over a wide range of magnitudes and colour, for wide and medium bands, and with up to millimag accuracy when synthetic photometry is standardised with respect to these external sources."；**原文（passband 含探测器）**："actual TCs, which in the following we also refer to as passbands, are defined by the combination of the TC of an optical filter …, the sensitivity curve of a photon-counting detector (typically a CCD for observations in the optical spectral range), and the TC of the optical elements …, plus a contribution from the terrestrial atmosphere"。
- **7. XP 外定标（仪器响应模型与定标精度）**：Montegriffo, P., De Angeli, F., Andrae, R., Riello, M., et al. 2023, A&A 674, A3（DOI 10.1051/0004-6361/202243880；arXiv:2206.06205）；De Angeli, F., et al. 2023, A&A 674, A2（DOI 10.1051/0004-6361/202243680；arXiv:2206.06143）；Carrasco, J. M., et al. 2021, A&A 652, A86（DOI 10.1051/0004-6361/202141249，XP 内定标）。
- **8. Gaia G/BP/RP 通带与零点（§2a.6 真实数据判据用到的 G 通带与零点）**：Riello, M., De Angeli, F., Evans, D. W., et al. 2021, A&A 649, A3（DOI 10.1051/0004-6361/202039587；arXiv:2012.01916）；通带曲线文件 = 本仓 `lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/GaiaEDR3_passband.dat`（列 `wl, G, G_err, BP, BP_err, RP, RP_err`，带外哨兵值 99.99）；Vega 零点 `ZP_VEG(G)=25.6874±0.0028`（官方 §5.4.1 Table 5.4），本合同复算用 GaiaXPy `Gaia_DR3_Vega` 的 W·m⁻²·nm⁻¹ 制零点 `−26.4899`。
- **9. 光子计数通带（`λ` 因子的文献依据）**：Bessell, M. S. 1990, PASP 102, 1181（DOI 10.1086/132749，UBVRI passbands；能量计数 vs 光子计数口径）；Bessell, M. S. & Murphy, S. 2012, PASP 124, 140（DOI 10.1086/664083，photonic passband 与零点）；Fukugita, M., et al. 1996, AJ 111, 1748（DOI 10.1086/117915）；Sirianni, M., et al. 2005, PASP 117, 1049（DOI 10.1086/444553，端到端系统透过率 × 光谱的工程范例）。
- **10. F_syn 数值积分（Akima + 复合 Simpson）**：Akima, H. 1970, J. ACM 17, 589（DOI 10.1145/321607.321609）；复合 Simpson 1/3 公式（教科书级）。**实现忠实性与误差分解**：`run/SCI-PHOT-FORMULA-01/evidence/c1_analytic_check.json`（生产 == 同输入网格复合 Simpson，rel = 0；2 nm 网格离散误差 0.66–1.34%；XPSD uint8 量化误差 0.025%）。
- **11. XPSD 容器格式（`flux_min`/`flux_mul` 量化解码）**：
  - 容器来源（可核验一手）：PixInsight 官方文档 *Spectrophotometry-based Color Calibration*（SPCC）§3："With the release of version 1.8.8-6 of PixInsight in October 2020, we introduced XPSD (eXtensible Point Source Database), a new database format we have designed and developed for fast and efficient access to massive astrometric and photometric star catalogs."；同节："mean spectra from 336 to 1020 nm sampled discretely at 2 nm steps (343 spectrum values) for each star"。PCL 官方 API 文档 `pcl::GaiaSearchData::normalizeSpectrum`："When enabled, search operations provide sampled spectrum data normalized to the [0,1] range for each star. When normalization is disabled, spectrum data is provided in either the original power units of spectral irradiance (W*m^-2*nm^-1), or in spectral photon flux units (ph*s^-1*m^-2*nm^-1) … Spectrum normalization is disabled by default." ⇒ **8-bit 归一化存储 + 逐星还原参数**是 PixInsight XPSD 的既有设计，还原到 W·m⁻²·nm⁻¹。
  - **字段名的地位**：`flux_min`/`flux_mul` 这两个**字段名**是本仓合同命名（`docs/contracts/DATA_SEMANTICS.md` §14.2；`gaia_client.c:449-455`），**未在 PixInsight 公开文档中核到**（PixInsight 明言 "An in-depth, formal description of the XPSD format is beyond the scope of this document"）。因此本文件**不**把该命名当作 Gaia 官方语义；解码式的**正确性由真实数据的绝对刻度实证锚定**（§2a.6），不依赖对容器内部命名的信任。
  - **与官方产品的关系**：官方 `xp_sampled_mean_spectrum` 以 float32 直接给出 `flux`（W·m⁻²·nm⁻¹），**不含** uint8 量化参数；本仓消费的是 XPSD 再编码容器（`gaia/GaiaDR3SP/gdr3sp-1.0.0-*.xpsd`，20 片，魔数 `XPSD0100`，记录布局 `32B EncodedStarData | float[2] 量化参数 | uint8 flux[343]`）。容器**不是** ESA 官方交付格式。仓库内的 XPSD **测试夹具**为自造（`eng/tests/unit/gaia_xpsd_fixture_gen.c`）；本节引用的实测数据是 `gaia/GaiaDR3SP/` 下的真实文件。
  - **使用约束**：量化参数**逐星不同**（实测跨 6 个数量级），因此 `byte` 数组**不能**当作与星无关的相对谱形使用。

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

## 15 Acceptance

- §11 Oracle 全过：合成注入 `location≈log10 k`（rtol 1e-4）、20% 离群鲁棒门 `<0.1 dex`、S=0 退化门、NumPy 复算 rtol 1e-9；
- §7 四不变量门全过（零点平移/尺度单调/鲁棒性/S=0）；
- `eng/tools/science_contract_lint.py` PASS；
- 解析不变量→SYN-002 转换：已知 flux/background 星场、photometric scale 恢复、离群注入用例登记 SYN-002。

---

## 16 方法链与方法学（DOC-404 增补；不改 §5 公式与常数）

> **上游**：`ASTROCS_DESIGN.md` §2.1（创新点一：测光校准到测光星等坐标系）、§4.2（Phase1 节点流程：星表引导检测 → PSF → WCS 解算 → photometry（**含同一步内的归一化施加**））、§4.4（测光输出语义）；研究包：`docs/research/PHOTOMETRY_RESEARCH_PACK.md`（一手出处与开源对照）。
> **边界**：本节只写方法链与论证，**不引入新公式、常数或门限**；§5 的 IRLS/Tukey 定义、§7 不变量、§10 不可接受变化全部不变；误差预算的**数值与实验**属 SCI-401（推导落点见 §16.4）。

### 16.1 方法链（逐步对应最高设计 §4.2 的节点顺序）

| 步 | 做什么 | 节点（§4.2） | 权威/细则 |
|---|---|---|---|
| ① **Gaia XP 逆映射定位** | 用本帧 WCS（近似指向来自 `wcs.init_source`）把 Gaia DR3 星表（ICRS/J2000，自行/视差传播到观测历元）**逆投影到像素域**，只在星表位置做质心/PSF 拟合；拟合失败直接丢弃（不计虚警、不报错）；上限按亮度取 top 2–5 万，极限星等按焦距/画幅/曝光派生估计 | `platesolve`（星表匹配 + 稳健迭代精化）+ `star_detection` | `docs/plugins/algorithms_phase1/03_star_detection.md` §4、`docs/plugins/algorithms_phase1/05_platesolve.md`；研究包 §6（astrometry.net 盲解+精化、SCAMP 星表解算、astropy WCS 逆投影） |
| ② **星点测光** | 在星表位置做 **PSF 拟合域**测光（全链唯一 `flux` 口径 `flux = 2πA·sx·sy/3`；孔径测光只作显式诊断）；`F_hat = Q/W`、`Var(F_hat)=1/W`；饱和/质量异常不入定标 | `psf` → `photometry` | `docs/plugins/algorithms_phase1/06_photometry.md` §4、`docs/science/PSF_SIGNAL_WEIGHT.md` §2；研究包 §6（DAOPHOT/Anderson & King 的星表引导 PSF 测光族、photutils/SEP 的独立对照） |
| ③ **光谱 × QE × 透过率积分（正向合成期望测光量）** | 用 Gaia DR3 XP 星点光谱 × 系统响应在**模型通带**内积分得 `F_syn`（定义式与量纲见本文件 §2a.1/§2a.2；与官方式 5.41 的对应见 §2a.3）；XP 采样网格 = 336–1020 nm、步长 2 nm、343 点，谱插值按 §14a；`Q(λ)≡1` 与网格外无数据是**显式未建模项**，不外推 | `photometry`（参考侧） | 研究包 §3（Gaia DR3 官方文档 §20.12.3/§20.12.4、Montegriffo 2023、De Angeli 2023）、§4（合成测光标准方法：Bessell 1990、Bessell & Murphy 2012、Sirianni 2005、synphot/pysynphot） |
| ④ **拟合 `k_photo` 与低阶空间增益 `m(x,y)`** | 逐星 `r_i = log10(F_instr/F_syn)` → 星等一致性预过滤 → IRLS/Tukey 稳健位置（§5）；同时用星点残差在帧内估计**低阶乘性空间增益** `m(x,y)`（平场/光学大尺度响应的低阶残余），与 `k_photo` 一并作为标定面 | `photometry` | 本文件 §5；`ASTROCS_DESIGN.md` §4.2（测光归一化落到像素，photometry 一步完成）；研究包 §7 ④（平场/大尺度残余的预算出处） |
| ⑤ **应用到像素** | `I_photo = k_photo·m(x,y)·I_cal` 施加到**整帧像素**（不只星点）；其后所有节点与 drizzle 消费归一化后的像素；该步不可用时产品显式记录 `degraded_reason` 并 **fail-closed**。**施加是 `photometry` 节点内的步骤，不是独立节点**（合并成一步省一次中间产物落盘 = 省一次写 + 一次读的 IO 往返） | `photometry`（同一步内的施加步骤） | `ASTROCS_DESIGN.md` §4.2（含 fail-closed 条款） |
| ⑥ **星等坐标系表达** | 产物通量以**星等/相对星等**表达；零点锚在**同一模型通带**的 Gaia XP 合成刻度上；标定系数绝对值无物理意义 | `noise_snr` 及其后 | `ASTROCS_DESIGN.md` §2.1/§4.4；本文件 §1/§6 |

- **一次检测、一次通量积分、三处复用**：检测、PSF、测光与 SNR 共用同一份星点绑定行与同一通量口径（`ASTROCS_DESIGN.md` §4.2），本节不另立口径。
- **每帧独立标定**：不同夜/不同透明度的帧 `k_photo` 不同是正常的、正确的；「帧间一致性」是语义目标与报告字段，**不是门禁**（`PHOT-GATE-DROP-001`；本文件 §1/§10）。

### 16.2 物理单位消除的论证（为什么产物只能是星等）

- 可观测链是 `I_cal = (g·t·A_eff·η·… ) · ∫F_λ T Q λ dλ + 噪声` 形态的**乘积**：增益 `g`（e⁻/ADU）、曝光 `t`、有效口径 `A_eff`、光学/大气透过率 `η` 等量在本项目数据面上**不可得**（设计前提：FITS 头拿不到这些量），且它们与模型通带归一常数在数学上**只有乘积可辨识**；
- 因此从「仪器计数 + Gaia XP 合成通量」这一组观测里，可辨识量只有**乘性标定面** `k_photo·m(x,y)`；任何对 `(g, t, A_eff, η)` 的拆分都需要引入数据中不存在的外部先验，属**不可辨识**（degenerate）问题；
- 星等/相对星等表达对这种退化**天然免疫**：乘性因子在 `log10` 下变成加性零点，零点平移不变量（§7）保证残差散度 `sigma_residual` 与判据不变；
- 故 §3 明确 `F_instr`（ADU）与 `F_syn`（模型通带积分辐照度）**量纲不同**、其比值的对数即 `location`；§6 明确不宣称绝对通量刻度。这与最高设计 §2.1「消除物理单位」与 §4.4「通量以星等/相对星等表达」一致。

### 16.3 禁止物理闭合反推的理由（§10 条款的论证）

1. **欠定**：单个乘性观测无法同时定出 `g、t、A_eff、η、消光`（未知数多于独立方程），反推必须假定未测量的先验；
2. **不可证伪**：若为 `k_photo`/`scale` 设绝对数值窗口，该窗口是未知仪器参数的函数，任何取值都能被某组未知参数“解释”，因此**没有证据资格**（AGENTS.md §5「判据必须非退化」）；
3. **如实性**：把反推值写入产品等于报告**未测量量**（AGENTS.md §6 禁令；`ASTROCS_DESIGN.md` §4.4）；
4. **唯一有意义的判据是尺度无关的测光一致性**：施加标定后星点**星等**对 Gaia 的残差散度（§5 的 `sigma_residual/sigma_mag`）；门只有一个 = 单帧标定是否可信，与其它帧无关（§1/§10）。
   ⇒ 因此 §10 把「用物理闭合式反推仪器参数」与「为 `k_photo`/`scale` 设绝对窗口」列为**不可接受变化**；本节的论证即该条款的依据，不新增任何阈值。

### 16.4 误差预算的构成与出处（数值属 SCI-401）

判据形态（**已由误差预算推导**，本节不复述数值）与逐项实测见 `docs/plugins/algorithms_phase1/06_photometry.md` §4.1（推导与复算脚本落点 `run/RELEASE-02/parallel/06.md`）。预算项与一手出处：

| 预算项 | 一手出处（研究包 §7） | 仓内状态 |
|---|---|---|
| 光子噪声（源+天光）与读出/量化 | Mortara & Fowler 1981；Merline & Howell 1995 | 由 variance/ivar 传播 |
| PSF 拟合不确定度 | Stetson 1987；Irwin 1985；Anderson & King 2000；Naylor 1998 | `σ_fit`（逐帧自算） |
| 最优提取/信息下界 | Horne 1986；Zackay & Ofek 2017（COAAD I） | `σ_floor` 的物理下限锚 |
| 平场/大尺度响应残余 | Stubbs & Tonry 2006；Regnault et al. 2009；Padmanabhan et al. 2008 | `σ_flat,hf`；大尺度残差**判不了**（已登记） |
| 天光/背景估计残余 | Bertin & Arnouts 1996；Starck & Murtagh 1998；Maples et al. 2018 | `σ_skyres` |
| 颜色项/通带失配（通带曲线错、QE 未建模、XP 谱误差） | Bessell 1990；Bessell & Murphy 2012；Fukugita 1996；Sirianni 2005；Montegriffo 2023（A33）；De Angeli 2023 | `σ_color`：**已定量**——通带取错（Baader R → Antlia V Pro Series B）注入跨星散度 **0.449 mag**（n=8406）；`Q≡1` 注入 **0.0082 mag**（n=10348）。证据 `run/SCI-PHOT-FORMULA-01/evidence/c2_negative_controls.json` |
| 星等定标误差（零点/参考网络） | Bessell & Murphy 2012；Burke et al. 2017；Schlafly et al. 2012；Bohlin et al. 2014/2019/2020 | `σ_Gaia`：XP 绝对刻度上限 **1%**（官方 §5.4.1）；本仓复算的 XP 解码一致性 **~0.004 mag**（median）+ 0.0033 mag（MAD，n=11272） |
| 大气/差分消光（**未建模**） | Schlafly & Finkbeiner 2011 | 无仓内曲线 ⇒ 未测项按「不加」处理（上限偏严、fail-closed） |
| 稳健统计与离群处理 | Rousseeuw & Croux 1993；Beaton & Tukey 1974；Maples et al. 2018 | §5 的 MAD/Tukey 层 |
| 拟合算法与谱插值 | Marquardt 1963；Akima 1970（§14a） | PSF/零点拟合与 `F_syn` 数值积分 |

- 未测项**不得**用估计值填充；预算上限按未测项不加处理，因此**偏严**（fail-closed），与 `06_photometry.md` §4.1 一致。

### 16.5 星数依赖与降级语义（SCI-A 实验定案）

> 依据：实验单元 `实验/photometric-magnitude`（报告 `README.md`、结果 `results/step1..step8*.json`、复跑 `python3 实验/photometric-magnitude/code/step*.py`）。本节只登记**判据的样本量依赖与降级语义**，不改 §5 公式、§7 不变量与 §10 禁改清单。
> C6（参考通量口径）的一手记录另见 `实验/photometric-magnitude/RESOLUTION_fsyn_formula.md`。

1. **星数不构成拒绝条件**：不存在星点少到无法测光的图；任何可解析帧都完成测光定标并出产品，产品按该帧自身实测的 `σ_obs` 与误差预算如实标注精度，不设固定星数门槛、也不套用他帧的精度口径。§5 的双边界判据（`σ_obs` 上下界）按本帧自身星数自算；
2. **星少到拟合不成立时报拟合失败（不是门槛拦截）**：§4 的冻结门「`|r_consistent| ≥ 3` 才进 IRLS」是**求解前提**——不成立时拟合**本就不产出标度**，走 NO_DATA 拟合失败路径（`fit_ok=false` + `degraded_reason` + `error` 上报，产品不得声明已施加测光）。该前提只回答「本次拟合有没有产出标度」，不作「星数够不够」的准入判据；
3. **N5 低样本量边界（判据能力边界，非拒绝门槛）**：`σ_floor = (1 − 3·1.166/√n)·σ_fit(白)` 在 `n ≲ 12` 时为负、在 `n ≲ 22` 时已趋零 ⇒ 对「把样本裁剪到只剩同质星」**没有判别力**（实测 tol=0.002、n=5 时 σ_obs=0.00356 仍 PASS，`results/step7_negatives.json → N5`）。该现象**只出现在低样本量区间**，实拍帧的典型星数区间（实测 n = 105 / 157 等）不构成缺陷；在低样本量区间使用该判据时，下界取 `max(rho_lo, 0)·σ_fit`；
4. **`σ_psfsys` 的孔径口径（C2 订正）**：用「PSF 域通量 vs 独立孔径通量」的中位绝对偏差估计 `σ_psfsys` 时，**必须用小孔径（≈2×FWHM）+ 低背景星子样本**。大孔径（r=10 px）把星云结构算进「方法系统误差」，实测高估约 **10×**（帧 A 0.2574 vs 真值 0.0256 mag；帧 C 1.3775 vs 0.0394 mag）；小孔径（r=4 px）在稀疏场准确（0.0250 / 0.0401 vs 0.0256 / 0.0344），在拥挤场仍上偏约 **2.7×**（保守方向，判据偏松不偏紧）。证据 `results/step5_calibration_gate.json → items_measured`；
5. **判据的敏感域（C3 能力边界）**：`σ_obs` 双边界判据对**散粒噪声**敏感、对**确定性加性图样**不敏感——算术相加 `gx·(x−W/2)`（无散粒）使 σ_obs 仅 1.08× 且始终 PASS，而把天光经 Poisson 前向重画则 2.91×（4× 时判红）。⇒ 该判据能认证的是「天光**噪声**是否被正确预算」，**不能**认证天光**扣除**质量；后者须另设残差检查；
6. **WCS 二轮精化是**平移**精化**：HST HLSP drz 头部 WCS 与 Gaia DR3 有 ~1.6″ 系统偏移（F657N 1.622″、F673N 1.629″，`results/step3_forward_vs_photflam.json → wcs_refinement`）；testdata 真实帧残余仅 0.0068″ ⇒ 二轮精化的必要性取决于上游 WCS 质量，不是流程固定开销；设计须能覆盖 ~2″ 量级平移；
7. **参考通量的合成口径与适用域（C6，任务 SCI-PHOT-FORMULA-01）**：`F_syn` 的唯一权威写法是 §2a.1（绝对谱辐照度 × 通带 × 光子计数权重，**不含** `10^(−0.4·G)`）；XPSD uint8 解码的绝对刻度由真实数据实证锚定（26211 星，`median(m_syn−G)=−0.0037 mag`、MAD 0.0033 mag，G∈[6,18]），适用域 **G ≲ 18**；通带（含 `Q`）与 XP 覆盖必须完全包含，否则 `F_syn≡0` 而**不产出零点**。生产落盘的 `ZP_syn` 由该公式**逐位复现**（T2/M1：落盘 −15.126346726632235 vs 复算 −15.126346726631280，Δ=9.5e−13），证实生产实现与本节口径一致。
8. **绝对刻度诚实边界（C5）**：XP 合成通量在**窄带**（等效宽度 29–39 nm）上与 HST PHOTFLAM 的中位差为 −0.147 / −0.263 / −0.080 mag（F657N/F673N/F502N，色项斜率 0.464/0.765/0.565）；**宽带（Baader R，~144 nm）的同类误差未直接测量**，不得据此断言宽带误差同量级。

### 16.6 指针

- 一手出处与开源对照（项目+版本+文件:行）：`docs/research/PHOTOMETRY_RESEARCH_PACK.md`；
- 模块算法与配置：`docs/plugins/algorithms_phase1/06_photometry.md`、`docs/plugins/algorithms_phase1/07_noise_snr.md`；
- PSF 信息权重与最优性声明：`docs/science/PSF_SIGNAL_WEIGHT.md`；
- 方差与协方差传播：`docs/science/UNCERTAINTY_AND_COVARIANCE.md`。
