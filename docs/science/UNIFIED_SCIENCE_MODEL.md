# AstroCS 统一科学定义与跨阶段合同

> 上游：ASTROCS_DESIGN.md §2（核心科学方法）、§3.1（数据对象）

文档 ID：ASTROCS-SCIENCE-MODEL-001  
状态：TARGET_NORMATIVE  
适用：Phase1/2/3 全部 SCI、ALG、DATA、API 和产品 schema。

## 1. 科学目标分层

AstroCS 不把“最好图像”当单一目标。至少区分：

1. 表面亮度/扩展源的无偏重建与方差；
2. 点源检测功率最大化；
3. 点源通量估计方差最小化；
4. 天体测量精度；
5. 视觉质量/筛帧质量。

目标不同，最优统计量可能不同。所有产品和权重必须声明 science_objective。

## 2. 统一线性观测模型

d_k = A_k x + n_k, Cov(n_k)=C_k。A_k 包含光度响应、PSF、像素响应、WCS 和重采样；C_k 包含随机噪声及可表示的相关项。任何简化必须说明从该模型删去了什么，并以误差门证明适用。

## 3. 权威术语

| 名称 | 定义 | 可否作权重 |
|---|---|---|
| signal | 声明单位和像素语义的科学估计量 | 否 |
| variance / ivar | 同一 signal 估计量的方差及倒数 | 对该估计目标可以 |
| source SNR | F_hat/sigma_F，依赖源亮度 | 不直接作帧权重 |
| depth m5 | 固定参考 PSF/孔径下 5σ 深度 | 摘要，不直接作权重 |
| point-source information | a² Pᵀ C⁻¹ P = 1/Var(F_hat) | 点源目标的严格权重 |
| support | 有效输入/面积贡献 | 否 |
| coverage | 几何/数据有效域 | 否 |
| validity | 坏点/缺失/越界等状态 | 门，不是权重 |
| rejection | 污染推断结果 | 门/概率，不是 coverage |

禁止继续使用无前缀的模糊 weight 或 snr 字段。

## 4. 点源最优统计

模型 d_k = a_k F P_k + n_k 下：

~~~text
Q_k = a_k P_kᵀ C_k⁻¹ d_k
W_k = a_k² P_kᵀ C_k⁻¹ P_k
F_hat_k = Q_k/W_k
Var(F_hat_k) = 1/W_k
~~~

独立帧：Q=ΣQ_k、W=ΣW_k、F_hat=Q/W、Var=1/W。固定参考通量时 SNR²=F_ref²W。相关帧必须使用联合 C，不能简单求和。

## 4.1 叠加权重的来源

权重是 Phase2 集成时按天球像素对应的输入帧集合**现场计算的派生量**；Phase1 与 Phase3 不产生、不消费权重。PSF 拟合质量代理（FWHM、残差尺度等）**只作诊断**，不计入科学叠加权重。
单一权重口径：`weight.default_mode = psf_information_weight`（token `point_information`），不存在其它权重模式。

## 5. 扩展源最优统计

~~~text
x_hat = (Aᵀ C⁻¹ A)⁻¹ Aᵀ C⁻¹ d
Cov(x_hat) = (Aᵀ C⁻¹ A)⁻¹
~~~

只有在同一输出量、线性算子退化为同点采样且噪声独立时，才简化为逐像素 ivar average。该简化不是点源 PSF-aware 最优的替代。

## 6. 校准与系统误差

光度尺度、背景、WCS、PSF 和 master calibration 参数都有不确定度。其影响分为：

- 独立随机项：进入 variance/covariance；
- 共享系统项：进入低秩 covariance/provenance；
- 模型偏差：进入 validity/quality 和系统误差预算，不得伪装随机 ivar。

## 7. 重采样与 Drizzle

重采样是线性算子 R：C_out=R C_in Rᵀ。输出只存对角 variance 时，必须另存 correlation kernel/scale 或可重建算子摘要。Drizzle 的 signal 单位、源/目标像素面积、pixfrac 和归一必须统一；常量面亮度和总积分通量 Oracle 同时成立。

非有限输入按**样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数**处理：不合格样本从信号、分母与方差三项中一并剔除并重新归一，仅零合格样本的输出像素取 NaN（NaN 是无效的唯一表示），且必须暴露被剔除样本计数；**禁止静默剔除**（`ASTROCS_DESIGN.md` §5.5；正本见 `docs/science/DRIZZLE.md` 与 `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a）。

## 8. 空间模型与标量压缩

PSF、背景、variance、photometric response 和 point information 原则上是空间量。压成帧级标量必须同时满足：

- 空间残差/趋势低于 SCI 冻结阈值；
- 对最终 flux bias、variance 和 detection power 的损失低于阈值；
- 输出标量值、分位数、采样覆盖、模型误差和适用域。

不满足时保存 map/model/control points。不能以节省 18–77 MB 为由先删科学信息再证明。

## 9. Phase 间最小合同

Phase1→Phase2：signal + units、variance/correlation、PSF、photometric response、WCS、validity、point information、depth、provenance。  
Phase2→Phase3：surface-brightness 和/或 point-source 产品族、variance/correlation、effective PSF、coverage/validity/rejection、UPM、provenance。  
任意 HiPS→Phase3：所选输出模式所需层齐全，否则 fail-closed/unavailable。

## 10. 科学验收

- 独立解析、高精度和 Monte Carlo Oracle；
- 注入源恢复 bias、variance、coverage；
- 同一图像改变星表亮度分布只改变 source-SNR 摘要，不改变 information；
- 独立帧满足预测 SNR_combined²=ΣSNR_k²；
- 协方差存在时能识别简单求和的过度乐观；
- 点源与扩展源目标分别相对基线证明无损或改善；
- 每个近似有 mutation 能使门变红。

## 11. 口径禁止项

- 禁止把 median(source SNR) 作为 Phase2 科学权重；
- 禁止使用未绑定固定参考通量/信息模型的 support×snr² 作为权重；
- 禁止把 PSF 拟合质量或复合质量权重当作 1/Var(F_hat)，也禁止把它混名为 Fisher information/ivar；
- 禁止宣称像素 ivar average 对任意 PSF 点源目标都最优；
- 禁止宣称 variance 足以描述所有 Drizzle 相关噪声（相关核/可重建算子摘要必须同存）；
- 禁止宣称一帧内 SNR 无条件常数。

## 12 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现，不改动 §2–§11 任何定义。

- **线性观测模型 d=A x+n、Cov(n)=C**：教科书级（如 Kay, S. M. 1993, Fundamentals of Statistical Signal Processing: Estimation Theory, Prentice Hall；Rodgers, C. D. 2000, Inverse Methods for Atmospheric Sounding, World Scientific）。
- **点源最优统计 Q/W、Fisher information**：Horne 1986, PASP 98, 609；Naylor 1998, MNRAS 296, 339；Zackay & Ofek 2017, ApJ 836, 187。
- **广义最小二乘 x̂=(AᵀC⁻¹A)⁻¹AᵀC⁻¹d、Cov=(AᵀC⁻¹A)⁻¹**：Aitken, A. C. 1935, Proc. Roy. Soc. Edinburgh 55, 42（GLS 原始出处）；教科书级。
- **C_out=R C_in Rᵀ**：Fruchter & Hook 2002, PASP 114, 144；Zackay & Ofek 2017 II, ApJ 836, 188。
- **5σ 深度 m5**：Tonry et al. 2012, ApJ 750, 99；Ivezić et al. 2019, ApJ 873, 111。
- **跨文档口径**：本文件 §3 表的 frame_snr（未加权原始信噪比）与 `docs/science/CONTROL_WEIGHT_SNR.md` §2a（相对质量权重，非科学信噪比）是不同对象，不得互相替代；天光面的现行口径为 `docs/science/PHASE2_UPM.md` 的**纯加性**（Phase2 只做加性校正，乘性空间残留由 Phase1 低阶空间增益处理）。

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

