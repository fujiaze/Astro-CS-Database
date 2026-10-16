# 统一观测模型的实现语义

本页说明公共物理量怎样被模块和产品使用。观测模型与量纲以统一科学模型为准[1]，条件参考精度以噪声科学正本为准[2]，工程对象与序列化由统一对象页和 Normalize 数据合同规定[3,4]。本页不另定参考通带、参考电平或生产算法。

## 1. 同一模型内区分不同估计量

局部冻结几何、响应与条件噪声后，可写线性观测 `d_k=A_k x+n_k`。A 包含对应阶段的光度响应、PSF、像元积分、WCS 和重采样；C 描述所声明条件下的随机协方差。未知 SED 的通带失配、拟合标度共同项和背景规范不能凭一个白噪声方差数组消除。[1]

点源局部模型是 `y=F h+Z beta+n`，h 必须与观测像元的单位、有效 PSF、积分和变换一致；背景是否已知或共同估计属于 estimator identity。[2]

| 对象 | 实际含义 | 消费边界 |
|---|---|---|
| signal | 指定响应/标度下的线性估计；平面积分量或球面面亮度 | 可以为零或负值；无数据用具名无效状态或 NaN |
| variance / ivar | 同一个 signal 估计量的条件随机方差/倒数 | 只对该估计目标使用；未知不当真实零方差 |
| source_snr | 真实源的测得通量/其误差 | 随源亮度变化的诊断，不等于固定参考空间深度 |
| reference_snr | 固定具名参考下的 F_ref/sqrt(V_F) | 与本帧单位、响应、模板和条件配对 |
| frame reference summary | 有效控制点支持面积加权 log reference_snr 的中位数后取 exp | 降维摘要，不替代局部 controls；兼容 ACSD_FRAME_SNR 不自动改认此身份 |
| point_information I_F | 1/V_F=reference_snr²/F_ref² | 点源通量精度，不能直接作为球面面亮度权重 |
| surface precision w_B | 1/V_B=I_F/T，T=V_B/V_F | 要求同源、同位置、同条件的 transfer；T 有量纲 |
| background-control variance | 指定 patch 均值、中位数或其它背景统计量的方差 | 背景拟合自身使用，不从点源 SNR 借方差 |
| depth_m5 | 给定响应与噪声模型下满足 SNR(F)=5 的参考深度 | 需要明确 SED/星等系统和有效域；不由任意 proxy 直接推出 |
| support / coverage | 明确的有效域、累计面积、覆盖或支持分数 | 各面积定义分名，不作为统计精度 |
| validity / rejection | 输入/模型可用性与异常选择 | 分别表达数据有效性和排异，不冒充信号或方差 |
| provenance | 输入、模型、标度、版本、能力与缺项来源 | 使科学量可独立解释 |

PSF 通量可以是具名 analytic_total、aperture_with_correction 或诊断量；估计器和截断约定必须声明。某一 Moffat 闭式不能替所有模型冻结全链唯一通量公式。FWHM 也不能代替模板 h 和背景 nuisance 下的条件信息。

## 2. 参考与标度配对

canonical 参考来自具名 SED/response 或用户显式正有限积分通量，连同 reference ID、单位和归一状态保存。参考星等只有在具名通带/系统/零点可执行时才能转换；不存在不依赖响应定义的通用默认星等或组内 ADU 公共锚。[4]

共同的是参考光谱和光度约定；不同帧的 F_ref,k 可以不同。确定性标度 alpha 使 F 乘 alpha、随机方差乘 alpha²，SNR 无量纲不变；若要比较 I_F，必须在同一积分单位下配对分母。原生 gain 的单位与 applied 数值单位分开，空间 alpha 按整个积分模板处理。

参考缺失时不从检出星通量中位数发明 canonical reference。合法 signal 可以发布，precision 具名 unavailable。precision 中缺项的 approximate、受前提约束的 upper_bound 和数据损坏是不同状态。

## 3. 稀疏控制点与阶段二重建

阶段一的 canonical 真源是 frame_science.json 及其可求值模型引用。控制点位于确定性有效 patch 网格，无须那里恰有真实恒星。每点绑定 ICRS、实际叶阶/索引、支持、F_ref、V_F、SNR、模板、噪声、背景条件和状态。[4]

`sparse_snr_layer` 的规整像素网格、Catalogue 浏览点和兼容 frame_snr 是不同读面，不能靠名称自动互换。自然双三次样条、值域裁剪、IDW 或帧常量是消费算法选择，不由输入 schema 的默认 description 冻结科学结论；空间支持、孔洞、外推与留出误差须明确。重建 I_F 后仍需 T 和相应实际观测条件，不能因 SNR 场有限就声明面亮度权重严格最优。[2]

当前已有球面 producer 与 sidecar 调用，Mosaic 的规整层读取尚非 canonical reader。静态实现范围集中在差异地图[5]；本页不将目标模型冒称实际全链。

## 4. 产品与生命周期

| 阶段 | 发布对象 | 内部或辅助对象 |
|---|---|---|
| Normalize | 逐帧 signal HiPS、唯一 frame_science.json 与必要模型/来源 | 可有有来源的 support/variance/ivar；工作缓存和开发快照分名 |
| Mosaic | 一个 signal 科学图像 HiPS，加标准元数据/层级和运行摘要 | 精度、支持、背景拟合、排异与权重用于计算，不是最终图像层 |
| Export | 普通二维 WCS signal FITS | 不从单信号恢复丢失的 covariance、SNR 或绝对方差 |

HiPS 多分辨率层级是同一信号的角尺度表示，不是额外科学通道或新观测。有限零与负值保留，无覆盖不填零。[1]

大面默认 FP32，WCS、控制点、参考通量和标定元数据 FP64；显式位深按配置与合同处理。Runtime 管理命名块与 typed 模型寿命，节点借用 const 输入、发布新对象，最后消费者完成后回收；必要 spill 走有身份的 AIO 缓存。开发 snapshot 只观测输出，不作为数据源。

科学配置、机器画像与冻结运行清单分别表达模型选择、机器执行参数与本次输入/输出身份。硬件调优不能改科学语义，能力状态不能被当作流程审批条件。[3]

## 参考文献

[1] [统一科学模型](../../science/unified/UNIFIED_SCIENCE_MODEL.md)。

[2] [噪声、参考 SNR 与精度重建](../../science/noise_snr/NOISE_SNR.md)。

[3] [统一数据对象与序列化登记](../../engineering/UNIFIED_OBJECTS.md)。

[4] [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

[5] [科学设计与生产源码差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
