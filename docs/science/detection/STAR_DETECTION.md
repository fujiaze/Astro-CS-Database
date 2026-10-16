# 星点候选与Gaia引导科学源表

本分册依据[Normalize科学链](../normalize/NORMALIZE_SCIENCE.md)，定义科学源表的坐标、测量与质量状态，并说明盲检诊断的阈值和适用域。天测由[ASTROMETRY](ASTROMETRY.md)定义，空间积分模型由[PSF分册](../psf/PSF.md)定义；正式对象和实现状态分别见[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)与[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 解算候选与科学测量

IPV内部在校准像元上盲检solver_detections，用于星表匹配及WCS求解，不依赖已经解出的WCS。WCS完成后，Gaia位置经本帧完整WCS与SIP投到像元域，产生独立science_sources，服务于PSF、测光、掩膜和噪声模型。

科学源表保留source/Gaia ID、预测及实测位置、通量估计器、单位、方差能力、饱和、混叠、边缘和拟合失败原因。引导位置必须包含畸变；仅用线性CD不能保证在边角仍落到星像。无可用Gaia源或拟合失败使相应科学模型unavailable，不伪装成成功，也不因此否定已经具备合法WCS和signal的帧。

完整WCS引导测量与全图阈值盲检的检出概率不同。后者可作为具名诊断；本页阈值与召回分析只适用于所声明的盲检算法，不作为IPV内部必须调用同一阈值的约束。

## 2. 像元中心与测量模型

内部坐标(x,y)=(column,row)是0基连续坐标，整数处为像元中心，首像元边界[-0.5,0.5]。FITS WCS使用1基中心，边界处加1或减1[1]。若某内部求解器使用下标+0.5的边界坐标，应由单一adapter转换到本合同，不能把两种坐标同时称作0基中心。

检测候选可用椭圆高斯拟合位置、振幅和粗通量；它不保证Moffat真实星核的总通量无偏。高斯FWHM/σ=2√(2ln2)=2.354820045；Moffat4在本项目σ=α/√2口径下系数为1.230307652590102。两参数相同数值不代表相同轮廓、FWHM或积分响应，最终测光须绑定其实际估计器。

连续质心可用矩、局部模型或非线性最小二乘[2,3]，不能再量化到半像元网格。定位误差随通量、背景、PSF、子像元相位、邻源与模型失配变化；例如0.3 pixel和峰值/背景尺度≥20可作为特定合成构型的项目目标，不能宣称任意真实源均满足。

## 3. 背景噪声与平滑阈值

行差分d_yx=I_yx−I_y,x−1在局部平稳噪声下满足

\[
 \operatorname{Var}(d)=2\sigma_n^2(1-\rho_{\rm adj}).
\]

逐行稳健差分尺度的中位数再除√2，仅在相邻相关为零、结构和裁剪影响可忽略时估计单像元σ_n。正相关导致低估，负相关导致高估。另一种全帧裁剪RMS还会响应天空结构与源污染，不应与行差分统计量互换。

若平滑图S=KI、固定背景b，项目盲检阈值可定义S>b+κσ_raw。κ=5.0指未平滑原图噪声的倍数。固定线性核下C_sm=KC_rawKᵀ；独立同方差、远离边界时σ_sm=σ_raw√(ΣK²)，等效倍数为κ/√(ΣK²)。阈值单位不变，显著性必须按实际作用图的协方差解释。

递归高斯滤波可按Young–van Vliet方法实现[4]，有限边界与实际递推系数决定有效核。以σ_s=2.0的项目递推为例，q=1.1532634818423229、B=0.227285251090804、b₁=1.2327675485381264、b₂=−0.5533139546361017、b₃=0.09326115500717135，直流增益约1；保留的核范数读数为Σk_1D²≈0.133174181553，分离二维核ΣK²≈0.017735362632，√(ΣK²)≈0.133174181553。因此κ=5在该条件下约为37.5448个平滑噪声尺度。该数不自动适用于边界、相关输入或另一组系数。

连续二维高斯近似给√(ΣK²)≈1/(2σ_s√π)=0.1410474，等效35.4491，与实际递推核不同。只凭σ_s名称不能宣称核完全一致。实际核、边界和冲激响应的算法对照见[检测细节](../../detail/normalize/modules/star_detection.md)，其中工程行为不能反向改变本页坐标和统计定义。

## 4. 召回与虚警

召回按已知真值的匹配规则计数，报告注入数、成功数、错误匹配、峰值或积分SNR定义、PSF、子像元相位、背景、拥挤度及筛选。纯噪声场的虚警按面积计数。阈值型提取的公开方法可与SExtractor对照[5]，不能借其名称替代本算法的标定。

固定条件、独立同分布的n次试验全部成功时，单侧95% Clopper–Pearson下界[6]为0.05¹ᐟⁿ。要求该下界≥0.99需n≥299；n=64、100、298、1000时分别约0.95427、0.97049、0.989997、0.997009。双侧95%区间的下端使用0.025，不能与此单侧结果混用。

盲检召回随PSF、相位和信噪比变化，可在声明范围分档测量；不能假设不同宽度的概率曲线形状必相同。50%过渡点与99%召回阈不同；无经复核阈值表的构型不报告已达到目标召回率。扫描后择取最优阈值还涉及多重比较及验证集，不能把训练集零失败当普适保证。包含过渡带、纯噪声和注入模型失配，才能评价判别力。

## 5. 饱和、混叠与边缘

探测器饱和应优先依据原生饱和电平、满阱/ADC限制、读出元数据与掩膜；校准和空间光度标度后该电平可随位置变化。16位存储上限65535不保证是物理满阱或饱和电平，也不能无条件用于浮点产品。

项目平顶诊断可写为3×3邻域均值减背景≥0.7dynrange，且peak减邻域最小值≤0.1dynrange。若dynrange=min(max(I),65535)−median(I)，结果只表示这一数值政策下的平顶候选，不能自动称探测器饱和真值；无正动态范围时模型不适用。检测段饱和岛与拟合段逐峰判断是不同算法，不混用其标志。

混叠、饱和或贴边不一定妨碍几何候选，但可能使测光、PSF或参考精度失效；按消费者声明质量。边缘处理、去重、排序和星数截断使用确定规则，平行度变化按所声明容差比较，不由排序确定性直接推出全部浮点结果逐位相同。

## 6. 数据交接与参考文献

science_sources和PSF共享源绑定；photometry消费正确通量估计及单位，noise使用同一模板和条件噪声。峰值/背景比、拟合质量q_psf与实际source_snr分名；frame_snr_ref及canonical controls也不由候选SNR中位数替代。

[1] Greisen, E. W., Calabretta, M. R. Representations of world coordinates in FITS. A&A, 395, 1061–1075. [论文](https://arxiv.org/abs/astro-ph/0207407)。

[2] Levenberg, K. A Method for the Solution of Certain Non-Linear Problems in Least Squares. QAM, 2, 164–168. [论文](https://doi.org/10.1090/qam/10666)。

[3] Marquardt, D. W. An Algorithm for Least-Squares Estimation of Nonlinear Parameters. JSIAM, 11, 431–441. [论文](https://doi.org/10.1137/0111030)。

[4] Young, I. T., van Vliet, L. J. Recursive Implementation of the Gaussian Filter. Signal Processing, 44, 139–151. [论文](https://doi.org/10.1016/0165-1684(95)00020-E)。

[5] Bertin, E., Arnouts, S. SExtractor: Software for Source Extraction. A&AS, 117, 393–404. [论文](https://doi.org/10.1051/aas:1996164)。

[6] Clopper, C. J., Pearson, E. S. The Use of Confidence or Fiducial Limits Illustrated in the Case of the Binomial. Biometrika, 26, 404–413. [论文](https://doi.org/10.1093/biomet/26.4.404)。

过程记录与复算示例见[记录](../../../实验/documentation-refactor/docs/DETECTION_OBSERVATIONS.md)；正文定义不以未配套复现载体的历史读数作为验证依据。
