# CCD与CMOS缺陷、修复及不确定度

本分册从[Normalize科学链](../normalize/NORMALIZE_SCIENCE.md)与[校准](CALIBRATION.md)定义缺陷分类、列检测、修复和误差传播。正式对象与能力状态见[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，生产实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 缺陷的物理分类

坏列、暗电流异常、读出拖尾、宇宙线和卫星线可能具有相似形态，却需要不同模型。分类同时使用探测器坐标、母版、读出方向、源依赖、曝光及跨帧复现，不能仅由连通域大小决定。

| 现象 | 主要来源 | 识别信息 | 处理边界 |
|---|---|---|---|
| 热像元、坏点 | 暗电流、响应或读出异常 | 母版、温度与曝光、固定坐标 | 缺陷掩膜及可解释修复 |
| 坏列 | 列相关偏置、响应或读出结构 | 跨列统计、母版与探测器固定位置 | 列模型、掩膜及局部修复 |
| CTE拖尾 | 电荷转移中的陷阱俘获与延迟释放 | 读出方向、信号/背景与转移距离 | 电荷转移模型，不能当纯坏列 |
| 宇宙线 | 粒子造成的电荷沉积 | 单帧尖锐形态、重复观测 | 掩膜或经过验证的局部修复 |
| 卫星线及散射光 | 曝光中进入的外部光 | 延展形态、多帧差异 | 排除受污染样本，避免无依据插值 |

仪器数据质量位是一种分类实例。ACS将部分坏列列为偏置结构，热/温像元按其仪器暗电流阈值分类[1]；这些阈值不是所有CCD/CMOS的常数。CTE效应依附于实际电荷源，随仪器、背景与信号状态变化[2]。宇宙线可利用拉普拉斯边缘与细结构区分[3]；事件不固定复现通常提供额外判别力，但不保证任何两帧都不在同一位置受污染。

## 2. 列统计检测

在声明的信号面上，定义列统计量及跨列差分：

\[
 c_x=\operatorname{median}_y I_{yx},\qquad
 d_x=c_x-c_{x-1},\qquad
 \sigma_d=\frac{\operatorname{MAD}(d)}{\Phi^{-1}(0.75)}.
\]

按|d_x−median(d)|与配置的稳健尺度倍数比较，得到跳变候选；反号边缘配对、段宽和邻域检查形成列段。高斯一致性系数为1.482602218505602；相关样本、结构背景或重尾会改变阈值的实际误报率。

列中位数会受拥挤场、弥散结构、渐变响应和掩膜影响。单个连通域可同时包含坏列与天体，阈值碎片也可能断开，因此尺寸不能单独给出物理分类。母版与科学面可以使用不同尺度阈值，但须记录输入标度、有效行和标定依据。对正仿射标度变换，统一变换统计量和尺度可保持此类阈值集合；这不等于不同物理源面一定判出相同列。

检测阈值、邻域列数和段宽是项目配置。示例配置为科学面5.0、母版面10.5个稳健尺度，每侧邻域k=3、最大段宽2k−1=5列、母版候选宽度占比0.001。2k−1定义的是算法窗口政策，不能由几何自行推出所有坏列都不超过该宽度。宽候选应记录为超出模型能力，不以整类丢弃后假称没有缺陷。

## 3. 修复算子与完整协方差

固定掩膜和插值系数时，修复是确定性线性映射y=Jx，因此

\[
 C_y=JC_xJ^T,\qquad
 \operatorname{Cov}(y_i,x_l)=\sum_jJ_{ij}C_{jl}.
\]

某修复值ŷ=Σ_jw_jx_j的方差为wᵀCw；输入独立时才简化为Σ_jw_j²V_j。两独立同方差邻元平均的方差确为V/2，但它与这两个邻元相关。将修复值及邻元当作三次独立测量会重复计信息；保留完整J和C时无需通过人为方差膨胀恢复正确性。确定性修复不增加独立观测信息。

如果只能消费对角近似，可配置

\[
 V_{\rm diag,repair}=\kappa\sum_jw_j^2V_j.
\]

κ=2仅是在两个等方差独立邻元、各权1/2时，让记账对角值回到V的一种政策。它不消除修复与邻元间的协方差，也不是对所有结构、权重、相关输入或下游测量都保守的物理信息下界。单侧复制、宽缺陷、负系数插值和已相关邻域需独立分析；未知相关性标approximate/unmodelled，不宣称一个κ已恢复完整误差。

误差传播与对缺失位置真值的预测误差不同。令真实无噪声信号为s，则修复偏差为(Js−s)_i；即使Σw=1，也只保证常量信号无偏。对任意天体结构无偏还需相应插值再现条件或正确的条件期望模型。陡峭星核、窄结构和宽掩膜可有显著模型偏差。

## 4. 掩膜填补与科学解释

掩膜填补的形式噪声可随平滑尺度减小，却不表示缺失光子被重新观测。边缘外推方法给出特定理想平均模型的形式尺度σ_org/√(2d+1)[4]，不能把它作为所有修复位置的真值误差或独立信息量。DES的伪影处理区分可局部插值的窄缺陷与需掩膜的成像伪影[5]；SDSS明确标识插值中心和插值占比过大时不可信的测光误差，并记录坏列插值的相关噪声[6,7]。

生产对象保留坏点极性（1=坏）、原始缺陷类别、修复状态、系数或模型引用以及缺项。修复不等于恢复好像元身份；饱和与电荷溢出不能仅因数值被填上就成为可用定标星。缺陷掩膜与跨帧排异的接受集合分别传递，详见[排异](../integration/REJECTION.md)。

## 5. 独立核验

验证分别比较缺陷识别、信号预测和信息传播：母版固定列与天体结构的混合；常量、线性梯度和星核穿过坏列；相关与独立邻元；宽缺陷及贴边单侧复制。检查J的实际系数、JCJᵀ、修复与邻元的协方差，以及使用全部样本后的最终估计方差。κ的改变只检验配置政策，不证明完整统计模型。

无缺陷时信号算子应为恒等；有缺陷时与实际插值规则一致。错误掩膜、遗漏交叉协方差、重复把修复视为独立观测等负例需能被实验区分。已有缺陷探针保存在[背景单元缺陷脚本](../../../实验/additive-sky-seamless/code/g08_defect_probe.py)；其输入域和结果不能自动推广到所有缺陷。

## 6. 参考文献

[1] STScI. ACS Data Handbook: [暗电流、热像元与宇宙线](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-3-dark-current-hot-pixels-and-cosmic-rays)。

[2] STScI. WFC3 Data Handbook: [The Nature of CTE Losses](https://hst-docs.stsci.edu/wfc3dhb/chapter-6-wfc3-uvis-charge-transfer-efficiency-cte/6-3-the-nature-of-cte-losses)。

[3] van Dokkum, P. G. Cosmic-Ray Rejection by Laplacian Edge Detection. PASP, 113, 1420–1427. [论文](https://arxiv.org/abs/astro-ph/0108003)。

[4] van Dokkum, P., Pasha, I. A Robust and Simple Method for Filling in Masked Data in Astronomical Images. PASP, 136, 034503. [论文](https://doi.org/10.1088/1538-3873/ad2866)。

[5] Desai, S. et al. Detection and Removal of Artifacts in Astronomical Images. Astronomy and Computing, 16, 67–78. [论文](https://arxiv.org/html/1601.07182v2)。

[6] SDSS. [Image Processing Flags](https://www.sdss4.org/dr17/algorithms/flags_detail/)。

[7] SDSS. [Imaging Data Caveats](https://www.sdss4.org/dr17/imaging/caveats/)。

过程记录与复算示例见[记录](../../../实验/documentation-refactor/docs/CCD_DEFECT_OBSERVATIONS.md)；正文定义不以未配套复现载体的历史读数作为验证依据。
