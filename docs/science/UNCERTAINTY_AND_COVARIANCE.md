# Uncertainty & Covariance

> 上游：ASTROCS_DESIGN.md §5.3（SNR 重建与逆方差叠加）、§3.1（数据对象）

## 目的

给出产品方差/ivar 与相邻像素相关性的权威说明。

## 逐像素方差

- 输入方差：噪声模型 A = `NoiseWeightModelV1`（空背景稳健方差，唯一生产模型，SCI-NOISE-001..015）；
- Drizzle 传播：var_p = Σ v_j w_jp² / D_p²（SCI-DRZ-014）；实现 = 分子 `acc.sumVarNum += varianceValue · w²`、分母 `D_p = Σ a_jp`（`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1632-1634`）。
  与 `lib/algorithms/noise_snr` 的 `snr_noise_scale_law`（`x′=α·x → Var′=α²·Var, ivar′=ivar/α²`，SCI-NOISE-002；实现 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:919`、声明 `snr_estimator.h:256`）同源互引——Drizzle 归一化权重求和即该缩放律的加权形式；
  **量纲**：`v_j` 与 `var_p` 同标度平方（`ADU²`；产品面为面亮度时 `ADU²/sr²`），`w_jp` 无量纲、`D_p = Σ_j a_jp` 为覆盖球面面积（`sr`）且**与 `v_j` 无关** ⇒ 缩放律在标度类别的任何一档上都成立（`DATA_SEMANTICS` §4a；`docs/standards/NUMERIC_STANDARD.md`「量纲与标度」）。
- 产品：HiPS variance + ivar（1/variance）。

## 协方差（重要边界）

同一源像素贡献多个输出像素 ⇒ Drizzle 后相邻输出**非严格独立**：

```text
Cov(S_p, S_q) = Σ_j c_jp c_jq v_j
```

不保存完整 covariance matrix；Monte Carlo 量化（SNR-012）：
nside=512 合成帧相邻像素 mean|ρ|≈0.19、max|ρ|≈0.57。

## 对使用的约束

- pixel variance ≠ aperture variance（**aperture variance 未建模**：本管线仅提供逐像素方差/ivar，未对 aperture 求和建模协方差；aperture 误差须显式加入 `Cov` 项，量化见 SNR-012——nside=512 mean|ρ|≈0.19、max|ρ|≈0.57）；
- 下游科学（如光度测量）如需 aperture 误差须显式考虑 pixfrac/resampling
  协方差；ivar 权重默认只用于逐像素最优组合。

## UPM control estimator 方差（ALG-UPM-CONTROL-IVAR-001）

(本节公式隶属 Phase2 UPM/ALG-UPM-CONTROL-IVAR-001，不属于 `lib/algorithms/noise_snr` 的 `NoiseWeightModelV1`；后者仅提供 σ_bg，经 sampler 阶段乘 k_corr 缩放，且 k_corr = N_retained/N_eff ≥ 1，见 SCI-UPM §5/§6)

UPM 的 control estimator 是 background-clean patch **median**，其方差
不是单 leaf 像素方差：

```text
control_variance = k_corr × (π/2) × sigma_bg² / N_retained
control_ivar     = 1 / control_variance
```

- 独立 Gaussian 基线 Var(median) ≈ πσ²/(2N)（实证 ratio 0.997）；**适用域（必须与 `sigma_bg` 的前提一致）**：样本独立同分布且 patch 内**无未分辨空间结构**。`sigma_bg` 来自 8×8 patch 的稳健尺度，其前提是背景在 patch 尺度局部平稳（`docs/science/SCIENCE_SCOPE.md` §假设的 `γ = dlog(patch 方差)/dlog(patch 中位信号) ≈ 1` 判据）。前提被违反时 `sigma_bg` 量的是空间结构而非随机分量，`control_variance` 与 `control_ivar` **一并失真** ⇒ 该 patch 的噪声场必须显式降级并登记 `degraded_reason`，不得按随机噪声消费；
  **量纲**：`sigma_bg²` 单位 `ADU²`，`N_retained` 无量纲计数，`k_corr` 无量纲 ⇒ `control_variance` 单位 `ADU²`、`control_ivar` 单位 `ADU⁻²`；
- k_corr 表征 Drizzle 输出协方差导致的 N_eff<N_retained：MC
  （pixfrac=0.8，2000 实现）k_corr=1.3883，N_eff≈181/251；冻结 1.4；
- N_retained 用 clipping 后保留样本（patch vs truth 验证）；
- 生产 UPM 权重 = quality × control_reliability × control_ivar（SCI-UPM-WEIGHT-001；
  `control_reliability` **实现为配置常量 1.0，不是按覆盖度算出的几何量**，缺陷登记
  SC-005），禁止再用单像素 ivar/support/SNR 乘因子。
- k_corr 定义域 1 ≤ k_corr：k_corr<1 ⇔ N_eff>N_retained（正相关样本的有效样本量不可能
  大于样本数），`p2_upm_control_variance` 与 `p2_upm_ma_build` 显式拒（rc=1 / rc=7）。

## Phase2 马赛克合成方差（DATA-P2-VAR-001）

马赛克加权积分（SCI-INT §5，w_i=逐样本 ivar，权重是阶段二按该天球像素对应帧集合现场算出的派生量，无 fallback）的
方差传播是 SCI-DRZ-014 一般式 `var_p = Σ_j v_j w_jp²/D_p²` 在积分权重下的
直接特例（w_i=1/v_i）：

```text
ivar_mosaic(p) = W(p) = Σ_i ivar_i(p)     # = wsum（SCI-INT §5 冻结量）
variance_mosaic(p) = 1 / W(p)
一般式（权重非纯逆方差时适用）: variance = Σ_i w_i²·v_i / W²
```

- 上游协方差（§上）不进入逐像素 variance：马赛克 variance 仍是逐像素随机
  方差，aperture 使用边界同上。
- UPM control_variance（ALG-UPM-CONTROL-IVAR-001）只进 w_UPM，与马赛克
  variance 产品严格分离。
- invalid（输出面）：无有效样本（n_used=0）→ variance/ivar=NaN（signal=NaN
  同态，writer 通道 DATA_SEMANTICS §12.4）；输入 ivar 非有限 → hard fail。
- unavailable（fail-closed）：权重非逐样本 ivar 或 ivar 帧缺失 fallback 发生 →
  不写 variance/ivar 产品 + manifest uncertainty_available=false（禁伪值）。
- 产品/manifest 表达与验证门：DATA_SEMANTICS §30.1/§30.5（DATA-P2-VAR-001）。

## Phase3 重采样方差传播（DATA-P3-UNC-001）

反向映射重采样（SCI-P3 §5 冻结采样核）把输入逐像素方差 u 传播到输出平面；
这是方差二次型在重采样权重下的直接应用（同 SCI-DRZ-014 二次形式、SCI-NOISE-002
缩放律同源）：

```text
# 主式（一般式，允许多个输入像素相关）
bilinear:  var_out(i) = [ R C_in Rᵀ ]_ii
           R = 本输出像素对各输入 leaf 的重采样权重行向量（ALG-P3-003 G4 冻结权重 c_k，
               Σ_k c_k = 1）；C_in = 输入逐像素协方差阵
nearest :  var_out = u_in                # 单权重 1 的特例；R C_in Rᵀ → u_in
标量（对角）特例: C_in = diag(u_k) ⇒ var_out = Σ_k c_k² · u_k
ivar_out = 1 / var_out   (var_out 有限且 >0)；var_out=0→0、NaN→NaN 同态
```

- **为何主式必须带协方差项**：本文件协方差节已给出
  `Cov(S_p,S_q)=Σ_j c_jp c_jq v_j`，且自报 mean|ρ|≈0.19、max|ρ|≈0.57
  （SNR-012，nside=512）。只写 `Σ c_k²u_k` 等于假设 C_in 对角，**系统性低估**
  输出方差：偏差因子 ≈ 1+0.75ρ（两像素近邻近似），ρ=0.19 ⇒ 方差低估 36.3%、
  σ 低估 20.2%（MC 复算 0.39415 vs 理论 0.39250）。
  与 ALG-P3-001 §3（`C_y = R C_x Rᵀ`）同式；`Σc_k²` 标量式**只在 C_in 对角时**
  成立，禁止当通用式（标量式只是 C_in 对角时的特例）。
- **Σc_k² ≠ 1 是正确物理**：bilinear 平均降低独立像素方差但引入相邻相关
  （§上协方差机制），禁止误用 Σc_k=1 归一 variance（常数信号场不变量
  SCI-P3 §7 只对 signal 成立，对 variance 不成立）。
- **实现口径（正向约束）**：`propagate_covariance(op, c_in)` 按一般式 `C_y = R C_x Rᵀ` 计算，
  **不假定 `C_in` 对角**（`lib/algorithms/resample/p3_rsmp_covariance.cpp:22-45`）；输出只取对角线作为
  `variance` 产品（`p3_rsmp_propagation.cpp:101,108`）。**完整 `C_x` 是生产输入路径**，对角 `C_in` 只作
  阴性对照（`lib/phase3_session/p3_v6_export.h:129-132`）。
  ⇒ 本文件主式与实现同式；产品面仍只发布对角 `variance`，**使用该 variance 做孔径/测量误差时**
  必须显式加入协方差项（本文件「对使用的约束」同款边界）。
- 输入选择：输入 HiPS 含 variance/ 子产品则 u=variance；否则含 ivar/ 则
  u=1/ivar；两者皆无 → uncertainty unavailable（输出无 VARIANCE/IVAR HDU +
  manifest uncertainty_available=false，DATA_SEMANTICS §30.4 unavailable 模式）；
  负/Inf = 产品损坏显式错误；NaN 传播（C=1）；**ivar==0 像素 = 零权重 ⇒
  u 无效（NaN 传播态），不是硬错误，也不得导出 1/0→Inf**（DATA_SEMANTICS §30.4-1/-3）。
  - **哨兵语义的跨阶段对照（必须成对读）**：Phase1 产品面把 `ivar=0` 定义为**不可用（拒绝加权）**并
    强制成对 `variance=0 ∧ ivar=0`（`docs/science/NOISE_MODEL.md` §7「空 support 不传播」「产品 dtype 成对不变量」）；
    Phase3 输入面把 `ivar=0` 读作**零权重 ⇒ 输出 NaN 传播态**。两者是同一哨兵值在**不同承载面**上的
    语义，转换点在 P3 输入选择：`variance=0 ∧ ivar=0` 的输入像素其输出 `variance/ivar=NaN`，
    既不硬错也不得反解出 `+Inf`。
- invalid（输出面）：无覆盖（C=0）→ variance/ivar=NaN（signal=NaN 同态）；
  覆盖不一致（leaf signal 有限而 u 缺失）→ 输出 NaN + provenance 计数。
- 产品/FITS 表达（EXTNAME=VARIANCE/IVAR、BUNIT 派生）与验证门：
  DATA_SEMANTICS §30.4/§30.5（DATA-P3-UNC-001）；SCI-P3 §9a-10 的
  variance/ivar 拒绝语义的**权威 = 本节 + DATA_SEMANTICS §30.4**。

## 数值精度

FP64；MC 表征 seed 固定可复现。

## ID

SCI-NOISE-011/012；ALG-DRZ-VAR-*；ALG-UPM-CONTROL-IVAR-001；
DATA-UPM-CONTROL-UNC-001；DATA-P2-VAR-001；DATA-P3-UNC-001
（DATA-UNC-001，DATA_SEMANTICS §30）。

## 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动本文件任何公式与容差。

- **线性方差二次型 C_out = R C_in Rᵀ**：线性误差传播（教科书级）；数值稳定性与归约误差见 Higham 2002, Accuracy and Stability of Numerical Algorithms, 2nd ed., SIAM（ISBN 0-89871-521-0）。
- **Drizzle 后相邻像素相关与方差低估**：Fruchter & Hook 2002, PASP 114, 144（DOI 10.1086/338393，§5 相关噪声）；DrizzlePac Handbook（STScI）；Zackay & Ofek 2017, ApJ 836, 188（相关噪声下的信息保持组合）。**差异（正向约束）**：传播链内部按完整二次型计算 `C_y = R C_x Rᵀ`（§Phase3），但**产品面只发布其对角线** `variance`，完整协方差矩阵不作为产品交付（§协方差节）。⇒ 用产品 `variance` 做孔径/测量误差时，必须显式加入协方差项；`Σc_k²u_k` 标量式只在 `C_in` 对角时成立。
- **var(median) = πσ²/(2N)**：**只对高斯样本成立**（正向约束）。一般式为 `Var(median) = 1/(4N·f(m)²)`，
  高斯密度 `f(m) = 1/(σ√(2π))` 代入即得 `πσ²/(2N)`；**换分布必须换 `f(m)`**——均匀分布为 `6σ²/(πN)`（比值 6/π），
  Laplace（尺度 b）为 `b²/N`（比值 1/π）。出处：Cramér, H. 1946, *Mathematical Methods of Statistics*,
  Princeton UP, **§28.5「The quantiles」（pp. 367–369）**（逐字：*"the median z of a sample of n from this
  distribution is asymptotically normal (m, σ√(π/(2n)))"*）。**适用域**：样本 i.i.d.、分布连续、密度在中位数邻域
  连续可微（`f(m)>0`）、大样本渐近。**本仓判据**：`PHASE2_SAMPLER.md` 的 MC 对非高斯族给出判红结果
  （均匀分布实测比值 1.9013 @N=1025 vs 解析 6/π；Laplace 实测 0.3345 vs 解析 1/π）⇒ 把本式用于非高斯样本即判红。
  **未独立验证**：Kendall & Stuart Vol.1 与 Hoaglin et al. 1983 的章节号与逐字原文（付费墙）。
- **像素 ivar 与孔径方差不等价**：aperture 方差须显式加 Cov 项；相关噪声处理见 Zackay & Ofek 2017 II。

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

