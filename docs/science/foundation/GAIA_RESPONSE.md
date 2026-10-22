> 本分支将本篇作为既定科学输入，保留以下历史审核措辞和有限证据身份。工程状态只由[新设计入口](../../engineering/rebuild/README.md)定义；历史中的“不创建分支/生产未修改/复算命令”描述其原始研究时点，不约束本分支。历史实验源码和证据已从新树移除，本篇相关路径仅指[固定基线研究档案](https://github.com/fujiaze/Astro-CS-Database/tree/c5fee09fedb97ccbe39cb47ea0985e0d8a339db2/%E5%AE%9E%E9%AA%8C/science-foundation)，不是新实现/测试依赖。实际公共字段由新合同唯一拥有。

# Gaia XP、经验标度与响应

**候选科学规范，待外部独立审核。** 输入观测、像元和单位由[观测模型](OBSERVATION_COORDINATES.md)定义；本文不依赖旧 ACSD 的光度结构或历史 XPMODEL 数值习惯。全部结果附 response、reference SED 和误差来源身份。

## 1. 合成积分及可辨识量

Gaia 恒星谱 \(f_{s,\lambda}\) 采用 W m⁻² nm⁻¹、波长 nm，已知 photon-detection response \(R_k=T_k Q_k E_k\) 无量纲，定义
\[
q_{ks}=\int f_{s,\lambda}(\lambda)R_k(\lambda)
{\lambda_{nm}10^{-9}\over hc}\,d\lambda_{nm}\quad[\mathrm{s^{-1}m^{-2}}].\tag{1}
\]
若响应自身已包含 photon conversion，则不再乘 λ/(hc)。GaiaXPy 的通带零点/standardisation 不自动等价本式的原生 T×QE；对照时确认 photon/energy、归一和谱密度单位。沿用某旧 `∫f T QE λ_nm dλ_nm` 数值时必须命名其非 SI 模型单位并显式换常数，不能将其原数字称为 photon flux。

原生测光数据是按像元积分 PSF/明确孔径测得的恒星总 ADU，带 aperture correction、背景及有限窗定义。模型
\[
C_{ks}=a_k \exp(b(u_s)^T\theta_k)q_{ks}+\epsilon_{ks}.\tag{2}
\]
灰色系数 a 包含曝光、有效集光面积、灰色透过率和 gain 的不可分乘积。对任何 c，`A_eff→c A_eff,g→c g` 保持观测相同，故 Gaia 标度**不能分离口径和电子增益**。gain 未知可估经验信号尺度；无重复观测或物理噪声元数据时不能生成完整 shot-noise。

固定本帧响应、用标量 a 校准后，像元 `x=C/a` 在式(1)的模型标度中；Drizzle 后为该模型标度/sr。空间标度 α(u) 需在 PSF 各像元实际应用，不能将位置中心 α_* 与整颗星积分 `Σα_j P_j` 混同。

## 2. 响应来源与窄带积分

`Response` 至少含有序 `(λ_nm,T,QE)` 或具名解析曲线、photon/energy 身份、归一、实测/假设状态、有效谱域、观测条件及共享误差身份。窄带默认可用
\(T=1_{[c-\Delta/2,c+\Delta/2]}\)，输入 center_nm、fwhm_nm，明确 `top_hat_approx`。这是工程形状假设；OD5 只说明带外抑制，不证明主带平顶、峰值或快速光锥中的中心波长。实测曲线覆盖该假设。未知 QE 只能用具名理想假设或退回相对标度。

数值积分不在粗 XP 网格上开关滤镜。按所有滤镜/QE 折点、top-hat 两端和谱表示分段；对 Gaia 外定标基函数 \(b_l(\lambda)\) 先算
\[
v_{kl}=\int b_l(\lambda)R_k(\lambda){\lambda10^{-9}\over hc}d\lambda,
\quad q_{ks}=v_k^T c_s,\quad
\operatorname{Cov}(q_{ks},q_{ls})=v_k^T\Sigma_{c_s}v_l.\tag{3}
\]
采用带误差估计的 Gauss–Kronrod/分段高阶求积；端点不跨阶跃。基函数积分误差 \(\delta v\) 给 `|δq|≤Σ|c_l||δv_l|`。固定预算先细分积分，不能靠增加插值点声称恢复未知谱线。每次缓存 O(Md)，每星 O(d)，d 为实际谱系数数；跨通带同星 covariance 不能独立复制。

若仅有 sampled/量化谱，明确其样点协方差是否可得；同一低分辨谱插值产生的新点不是独立数据。response 在 XP 支持域外的积分缺失须报告；只有已知非负谱上界时才可将其转有限尾积分界，不能只因“带内有正值”称完整。

Gaia synthetic photometry 原文 Appendix B 定义 \(R_f=\mathrm{FWHM}_{band}/\mathrm{FWHM}_{LSF}\)，一般建议 \(R_f\gtrsim1.4\)。因此 3–7 nm 的求积可以极精确，但未知高频谱结构不能由低分辨 XP 唯一恢复。该建议并非所有源的硬阈值：平滑连续谱可能很好，未分辨强线可能严重失败。

**必须分账**：数值求积误差、XP 随机系数/外定标误差、XP 有限分辨率造成的模型偏差、T/QE/大气/入射角形状误差、星匹配/PSF/孔径误差。确定未知偏差不强制包装成均值为零的随机项。

## 3. 可编码的拟合算法

首选在 flux 域作联合潜变量拟合。令每颗星真实谱系数 c_s（或仅需的通带潜通量 q_s）为 nuisance，Gaia 测量满足 `c_Gaia=c+η`，counts 模型为 \(m_s(\theta,c_s)=a\exp(b_s^T\theta) v^T c_s\)。对 `[counts,c_Gaia]` 的固定联合 Gaussian covariance 作白化最小二乘，跨帧共用同一星的 latent 与误差身份；独立代码 `fit_shared_gaia` 实现其潜通量形式。消去 latent 而改用参数依赖的残差 covariance 时，须明确 profile 或 marginal likelihood；不能仅更新权重就将其称为同一个完整 likelihood，marginal Gaussian 还含 logdet 项。低 SNR flux 保留符号；其强 Poisson/read 非Gaussian域改用原始计数似然/参数化bootstrap，不把本轮高SNR Gaussian实验外推为低SNR已验证。

```text
integrate_response(Response, SpectralBasis, budget) -> {v, numerical_error}
fit_photometry(StarObservations, Responses, basis, gauge, NoiseModel)
    选择非饱和、可测总flux、位置/谱覆盖合格星；保留选择身份
    θ 初始化为 scalar；按星分块组装 counts 与 Gaia 的联合协方差
    联合计算 counts/Gaia residual 与 θ/latent Jacobian
    白化QR/SVD求Gauss–Newton步，线搜索；大规模按source块消元latent
    需要抗污染时Huber IRLS；同source_id作聚类bootstrap
    用真实加权J判秩/缩放条件数；不足时返回不可辨识方向
    用留出星验证颜色/空间残差，输出α(u)、Jacobian及支持域
```

高 SNR 时可用 `r=ln C−ln q` 的线性模型 `[1,b(u),color]`，其 delta-method 方差含 `Var(C)/C²+Var(q)/q²−2Cov(C,q)/(Cq)`；该近似的适用 SNR、正值选择和截尾风险须声明。空间基以训练区域归一化，约束其加权均值为零以与 scalar a 分开；星色与位置共线时联合色项/空间梯度不可辨识。固定权重且模型正确时 GLS covariance 来自实际输入；残差 MAD 不是零点参数标准误。

颜色项默认用于诊断响应模型，或在有谱先验的恒星上作预测；不能用星色替代每个星云像元的 SED。拟合空间项需实际位置支持与留出预测；超出支持可退回 scalar 或带外推误差，不能仅因设计满秩就声称全帧准确。相同 Gaia 星跨帧复用建立跨帧参数 covariance，不以 frame_id 重置误差。

```cpp
struct StarFlux { uint64_t gaia_source_id; PixelCenter0 position;
  double integrated_flux, variance; FluxDefinition definition; NoiseState noise; };
struct SpectralPrediction { double q; SparseFactors shared_spectral_error;
  double integration_abs_error; Domain spectral_support; };
struct PhotoSolution { Vector theta; CovarianceAction covariance;
  Basis spatial_basis; Domain support; Response response; ReferenceSED reference;
  RankReport rank; FitInfluence influence; };
// α 的单位 L/ADU；Jθ 的每列单位随θ定义，保留训练像元依赖。
apply_scale(PhotoSolution, NativePatch) -> CalibratedPatch;
```

星位、PSF积分域及光度应用共享[观测合同](OBSERVATION_COORDINATES.md)的 `PixelCenter0/WcsModel/DetectorFootprint`，不在Gaia或PSF接口再补原点偏移。输出积分 x 的单位与 `SignalEstimate` 一致，数值求积/拟合舍入界、XP随机误差和响应模型偏差分别保留；有限确定偏差不自动作为独立方差。后续面积区间按[几何规范 §5](SPHERICAL_DRIZZLE.md)传播到同一R、B与Cov，若x也有数值区间，signed numerator按完整端点积取界，或用 `ΣR_U δx` 增加确定信号误差界。该数值项不覆盖未知谱线和真实response失配。

n 星、p 参数，固定小 p 的一次稠密局部拟合 O(np²+p³)，共享星的谱 covariance 用按 source 分块算子；大型多帧求解用稀疏 QR/LSQR。不能生成全图 N² covariance。`α=exp(−h^Tθ)` 时 `∂x'/∂θ=−x' h^T`；与同图拟合共用数据的交叉项交给[统一统计](STATISTICS_SKY_COADD.md)，一次归属。

## 4. 参考 SED、跨通带及不可辨识证明

选择固定外部参考谱 f_*，令 \(c_k=q_0(f_*)/q_k(f_*)\)。乘 c_k 仅使该参考谱在响应0下相等，不表示任意谱都相等。若存在 h 使对所有训练星 `∫f_s h=0`，响应 `R` 和 `R+h` 给所有星同一测量，却对新的线谱有不同测量；有限恒星集合无法恢复无限维响应细节。

对任意谱都满足 \(q_0(f)=c q_k(f)\) 当且仅当两个积分核在有效域几乎处处成比例。否则某窄支撑谱就是反例。用户可合成 Hα 与 Red，但其目标为给定规则的混合观测：固定权重/选择下
\[
K_{eff}(p,\omega,\lambda)=\sum_k t_{kp} c_k K_k(p,\omega,\lambda).\tag{4}
\]
只有空间核相同或可分离时才单独写 `R_eff(λ,p)=Σt_k c_k R_k(λ)`。负 BLUE 权可产生带符号的**线性响应**，不能称物理非负滤镜；需要非负通带解释时解约束 `t≥0,Σt=1` 的方差最小化 QP，明确次优目标。数据依赖权重时式(4)是本次选择的条件描述，不是全场固定通带。

相对 sky 只在共同 response 和空间观测可比较的组内识别；组间共同基线无外锚不可测。用同一用户选择规则仍可产生一个正式 signal，同时保存位置相关响应/权重及未知身份，不把混合伪装为纯 Hα。

## 5. 已运行证据与真实验证

`statistics_independent.py` 的解析多项式谱积分对照中，3/7 nm 相对数值误差约 `1.11e−16/2.22e−16`。独立构造 σ=0.05 nm 真线、σ=4 nm 低分辨表示后，3/7 nm 带内线贡献分别低估约 `71.2%/38.9%`；这些是可证伪“数值积分精确⇒物理窄带正确”的合成反例，**不是实测 Gaia LSF**。连续谱 3→7 标量约 2.336 会将中央线高估约133.6%。

独立脚本另实现 shared Gaia GLS，给已知比例/空间项、共享谱误差和 rank 负例；原始输出、随机 seed、coverage 区间及解释见[独立证据](INDEPENDENT_ORACLES.md)。

**DERIVED**：单位、不可辨识证明、谱积分及标度 Jacobian。**INDEPENDENTLY_TESTED**：上述合成谱、共享源联合拟合和数值积分。**ASSUMPTION**：T×QE 及低维可估响应足以代表真实系统。**REQUIRES_REAL_DATA**：Gaia 留出星按颜色/位置分层残差、真实 Hα/Red M42 线比、快速光锥滤镜蓝移、PSF 孔径及大气条件；不能用星拟合 RMS 代替星云验证。

## 6. 原始参考

1. [Gaia Collaboration 2023，A&A 674 A33，§2、Appendix B](https://arxiv.org/html/2206.06215v2)，DOI [10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)：合成测光、低分辨适用域与谱 covariance；采用其科学表示和 GaiaXPy 作外部参照。
2. [Montegriffo et al. 2023，外定标，A&A 674 A3](https://www.aanda.org/articles/aa/pdf/2023/06/aa43880-22.pdf)，DOI 10.1051/0004-6361/202243880：XP 外定标不是无限光谱分辨率。
3. [GaiaXPy 官方实现](https://github.com/gaia-dpci/GaiaXPy)：对已知有效通带复核谱合成，不无条件复用其 standardisation 为任意 T×QE。
4. [Padmanabhan et al. 2008，ubercal](https://doi.org/10.1086/524677)：采用全局相对标度、共享星与 gauge 的成熟方法；不据此辨认未知 gain。
5. [Bessell & Murphy 2012](https://doi.org/10.1086/664083)：photon/energy passband 身份和零点定义。
