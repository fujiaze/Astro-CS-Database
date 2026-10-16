# HEALPix 球面 Drizzle、HiPS 层级与相关噪声

本分册依据[最高设计](../../ACSD_DESIGN.md)与[统一科学模型](../unified/UNIFIED_SCIENCE_MODEL.md)，定义球面交叠、信号归一及相关噪声。条件参考精度见[噪声分册](../noise_snr/NOISE_SNR.md)，实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 系统坐标与图像量

输入平面 WCS 是含 SIP 等适用畸变的像元足迹映射；像素中心与边界不是同一个对象。内部天球方向采用 ICRS 单位向量/弧度，外部常见接口为 RA/Dec 度；RA 穿越 0°、极区、极赤道转折和 HEALPix 基面边界需在球面做几何处理，不能以普通经度差值直接插值。

HEALPix的等面积与曲边由[1]定义。ACSD使用 NESTED、nside=2^K，所有叶为等立体角
\[
 \Omega_{\rm leaf}=\frac{4\pi}{12\,{\rm nside}^2}.
\]
这保证**叶面积相等**，并不保证叶的形状是大圆边构成的球面四边形。HEALPix 真边界为其等面积 chart 内常坐标线映射至球面后的曲线。尤其在极区，连接边界端点的大圆弦一般不能视为精确边界。

输入源像元 j 的积分计数/线性通量 x_j，在选择的统一标度上对应真实像元立体角 Ω_j 和像元平均面亮度 B_j=x_j/Ω_j。若 x_j 的单位已从 ADU 映射为合成模型通带通量，则 BUNIT 应体现该单位；不得硬把所有产品统一命名 ADU/sr。

## 2. 理想 Drizzle：一份面积定义贯穿全部公式

Drizzle通过缩小足迹控制额外卷积和噪声相关[2]。输入像元 j 的 drop 是原像元在约定的球面收缩映射下得到的 D_j（0<pixfrac≤1）。定义：
- A_j=area(D_j)：**实际**收缩 drop 球面面积；
- Ω_j=area(未收缩源像元足迹)：真实源面积；
- H_p：目标 HEALPix 叶的球面区域；
- a_jp=area(D_j∩H_p)：以同一几何定义求得的非负交叠面积；
- w_jp=a_jp/A_j：源信号分配权（无量纲）；
- N_p=Σ_j w_jpΩ_j：面亮度归一分母（sr）；
- D_p=Σ_j a_jp：drop 面积重复命中的累计面积（sr）。

理想全域覆盖/裁剪完整时有
\[
 \sum_p a_{jp}=A_j,\qquad \sum_p w_{jp}=1.
\]
由此得
\[
 F_p=\sum_jw_{jp}x_j,\qquad
 S_p=\frac{F_p}{N_p}
 =\frac{\sum_j a_{jp}(\Omega_j/A_j) B_j}{\sum_j a_{jp}(\Omega_j/A_j)},
 \qquad c_{pj}=\frac{w_{jp}}{N_p}.
\]

### 2.1 正确的守恒对象

Σ_p F_p=Σ_j x_j 是**drop 分配的累加量守恒**（仅对完整参与的输入/目标分区，边缘裁剪要按实际足迹计）；常值场 B_j=B0 使 S_p=B0（D_p>0），是**面亮度保持**。这两个性质不是同一个。

输出信号有覆盖归一，不能一般地写成 Σ_p S_pΩ_leaf=Σ_j x_j：目标每个 leaf 可能接受多帧、多个重叠 drop，分母 N_p 与 Ω_leaf 不同。**总通量守恒**如用于某个孤立目标的测光，需要另写该目标的具体孔径积分、背景及采样模型，不得用 F_p 的恒等式冒充任意输出孔径保持。

### 2.2 pixfrac 与双重面积

一般不允许用 A_j=pixfrac²Ω_j 作为精确球面恒等式：球面收缩方式、WCS 曲率和几何近似会引入误差。必须实际求两种面积；当 pixfrac=1 且 drop 几何就是未收缩足迹时，两者应一致。尤其不允许权重分子按 drop 面积归一，面亮度分母却改用 D_p 而没有补偿 N_p，避免出现 B0/pixfrac² 偏差。

这里定义的 c_pj 作用于 x_j，单位 1/sr；作用于 B_j 的等价无量纲系数是 w_jpΩ_j/N_p。不可混读，特别在不同源像元角尺度时。

## 3. 球面几何实现与独立对照

生产可选两类清晰可辨的数值路线：

**路线 A：HEALPix 等面积 chart 求交叠。** 用可逆的面内坐标映射把真实曲边 leaf 转为 chart 的坐标方块，将源 drop 边界参数化并映射进 chart；以该 chart 的真实面积 Jacobian 计算交集。分段函数必须处理 12 个面、极/赤道分界、经度回绕和退化点。坐标连续性、面内方向、边界裁剪和 Jacobian 必须独立复核；“chart 是方形”不等于任意近似投影都自动精确守恒。

**路线 B：球面多边形数值求交叠。** 以顶点连成的大圆边近似 drop/leaf 的曲边，作球面半空间裁剪；必须明确这是**顶点弦近似**，不是精确 HEALPix 叶。细小多边形的面积/近共线交点须采用数值稳定公式；误差由 WCS、曲边近似、浮点与候选集共同决定。

两条路线可采用一条作为生产、一条加高阶曲边细分为独立 oracle，但不能让被测代码的同一角点集合和同一面积公式承担所有“独立”参照。必要时用球面高精度多边形/自适应积分、参数域等面积积分进行交叉验证。

### 3.1 局部几何误差预算

每个源 drop 至少评估：
\[
 \epsilon^{\rm partition}_j=\frac{\sum_p a_{jp}}{A_j}-1;\quad
 \epsilon^{\rm curve}_j=\frac{A_j^{\rm approx}-A_j^{\rm truth}}{A_j^{\rm truth}};
\]
另外检查源/目标 footprint、面缝走向、负交叠面积、候选遗漏、浮点下溢和近零 drop。第一个量接近 0 **不能排除**源/目标几何同时采用错误曲边模型。

几何数值研究的构型、分割残差和失效域见[球面实验报告](../../../实验/healpix-polar/REPORT_experiment.md)及[曲边推导](../../../实验/healpix-polar/docs/DERIVATIONS-P3.md)。某个近似几何模型的分割残差不是与真实曲边的误差上界。

### 3.2 独立坐标测试

至少覆盖：
- 12 个 base face、东西方向、南北半球、极区、赤道带、|z|=2/3 转折；
- NESTED ang2pix/pix2ang 及连续小数坐标；RA≈0/360，Dec≈±90；
- 单 leaf、四叶交点、tile 边界、face 交点和跨 face 的多像元 drop；
- 小到近数值精度限和大到跨多个 leaf 的 drop；
- 真实 WCS 变形、不同源/目标尺度比、pixfrac 从低于 1 到 1；
- 不同线程归约顺序和 float32/float64；
- 与至少一个独立 HEALPix 实现对拍索引，以及独立 HiPS tile 生成器对拍图像手性。

目标是用**物理量与误差曲线**解释几何，而不是让固定宽容差决定结果可信度。

## 4. 方差与空间协方差严格传播

Drizzle 是随几何冻结后对**输入线性信号**的线性算子；矩阵 R_{pj}=c_pj。输入 C_in 是源像元计数/线性信号的随机协方差。于是
\[
 C_{\rm leaf}=R C_{\rm in}R^T.
\]
对独立输入 v_j：
\[
 V_p=\sum_j c_{pj}^{2}v_j
 =\frac{\sum_jv_jw_{jp}^{2}}{N_p^2},\quad
 {\rm Cov}(S_p,S_q)=\sum_jc_{pj}c_{qj}v_j.
\]

平方和、双重归一、单位匹配不可随 pixfrac 调整而分叉。标度 α 的确定性转换使信号乘 α，方差乘 α²；估计 α 的共同不确定性另算。

对于线性孔径统计量 L=a^TS，真方差
\[
 {\rm Var}(L)=a^TC_{\rm leaf}a
 = (R^Ta)^TC_{\rm in}(R^Ta).
\]
对邻近像元取平均时，丢掉非对角元会高估有效独立样本数；输出图像视觉变平滑并不代表点源检测意义的信噪比同步增加。带负孔径权重（例如减背景）时，对角近似的误差方向不保证为低估。

不要求为最终阶段二 HiPS 存全矩阵。阶段一/二内部可用局部相关核、回溯重采样算子、等效 NEA/PSF 参数与经独立标定的近似来计算所需测量统计量；近似必须注明条件及量级，不应只用一个通用 k_corr 覆盖所有统计对象。

## 5. 覆盖、有效性与输出单信号

D_p 是累计 drop 交叠面积，允许大于 Ω_leaf（多帧、多 drop 重叠），不能当作已去重覆盖并集。发布支撑比例可以为 min(1,D_p/Ω_leaf)，但它是**描述型观测支撑**，不是像元精度，也不能无条件代回通量积分。

阶段一可保留单帧 signal/support/variance/ivar 等为阶段二的输入辅助产品；阶段二只发布 signal 图像 HiPS。即使没有最终 support 图像层，内部仍需区分：
- 真正的零亮度（有限值 0）；
- 完全无数据（NaN 或具名无效状态）；
- 有图像数据但精度模型未知；
- 多帧有效重叠与单帧覆盖；
- 已被异常样本选择排除。

这些区别在写最终信号图时按不丢失零亮度语义的无效表示发布，运行摘要可存全局诊断，不应制造第二、第三个科学图像层。

## 6. HiPS NESTED 局部索引与 FITS 文件顺序

Tile宽度固定为 512（2^9）时，某 tile 的 leaf 分辨率 order_leaf=order_tile+9。NESTED local 序由 (x,y) 的位交错构成，ACSD的共享坐标约定：x 占偶数位，y 占奇数位；FITS row-major 像元映射是
\[
 \mathrm{index}_{\rm FITS}=(511-x)\cdot512+y.
\]
此映射是**ACSD采用的矩阵方向约定**，须与HiPS标准及独立生产者核对[3]；它并非仅由 NESTED 编码和 FITS 标准分别推出的唯一 HiPS 图像排列。独立手性检查应覆盖多个波段和基面；生产读写器的相互往返只证明内部一致性。

写端/读端/export 端须共享函数而不是复制位操作公式。HEALPix 位置关系与 tile 像素矩阵排布为不同对象。需验证父子变换、跨面相邻、WCS 生成与 FITS 显示方向。

## 7. HiPS 多分辨率：物理语义和层级重建

每个 order 的 tile 是同一信号场的不同角尺度表示，不是新增独立观测。父像元若由子像元表示形成，则选择并固定目标：
\[
 S_P=\frac{\sum_{p\in{\rm child}(P)}\Omega_{p,\rm valid}S_p}
 {\sum_{p\in{\rm child}(P)}\Omega_{p,\rm valid}} .
\]
其中 Ω_valid 是该层真实受支持区域的有效面积/近似量（与下采样政策一致），不是不同帧叠加的逆方差权重。若完全覆盖且每个四分子叶等面积，上式退化为四子均值；若部分覆盖，要避免把 NaN 作 0 混合。**高层分辨率的可观测值与由细层派生值未必与直接对原始帧重新 Drizzle 完全相同**，必须以数值误差解释差异。

避免在层级生成时反复以独立插值核进行多轮任意平滑。评估阶间扩展源总通量、点源孔径光度、PSF/MTF 变化、噪声功率谱与真实支持域；HiPS 层级变换不应凭视觉上噪声更细腻而声称信息增益。

## 8. 阶段三从 HiPS 导出平面

阶段三读取兼容的HiPS信号产品（典型输入为阶段二马赛克），按用户设定 WCS 生成二维 FITS，用于后期处理；默认不以原始 CCD/CMOS 帧绕开 HiPS 重建。输出 WCS 由目标投影准确计算，验证投影手性、像元中心、角尺度、奇点和 RA wrap。

分开两种用途：
- **通用后期**：按明确方法重采样已有球面信号；nearest、局部线性和按区域交叠的面积重采样可作候选，参数及实际生效方法记录在元数据。
- **数值测量与误差研究**：使用已知真值世界或保留的内部算子信息研究二次重采样后的星点通量、有效 PSF 和相关噪声。最终只有单层信号，**不得无来源生成绝对方差/ivar/point-information**。

不能将阶段三的 bilinear 重采样造成的噪声峰度/空间频谱变化称作 Drizzle 自身的误差；Drizzle 与 export 必须在对照实验中单独打开/关闭与报告。

## 9. 面积平均的适用条件

一般情形的信号是 S_p=Σ a_jp(Ω_j/A_j)B_j / Σ a_jp(Ω_j/A_j)。只有参与该像元的 Ω_j/A_j 为共同常数时才简化为 Σa_jpB_j/Σa_jp；例如相同局部面积收缩比例的理想情形。既然真实球面上 A_j=pixfrac²Ω_j 只是近似，不能同时无条件写面积平均等式。常值场保持和分配通量守恒不依赖这一简化。科学接口必须传实际 Ω、A、a 或同一 R，不由消费者重建近似比例。

## 10. 参考文献

[1] Górski, K. M. et al. HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere. ApJ, 622, 759–771. [论文](https://arxiv.org/abs/astro-ph/0409513)；[官方几何说明](https://healpix.sourceforge.io/html/intro_Geometric_Algebraic_Propert.htm)。

[2] Fruchter, A. S., Hook, R. N. Drizzle: A Method for the Linear Reconstruction of Undersampled Images. PASP, 114, 144–152. [论文](https://arxiv.org/html/astro-ph/9808087v2)。

[3] IVOA. HiPS: Hierarchical Progressive Surveys. [推荐标准](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)。
