# 科学与数学问题：集中待推导记录

本记录保存文档整理中遇到、需要后续统一推导或核验的事项。**所有条目均为 UNRESOLVED**；除明确引用的现行恒等式外，本轮不提出替代模型，不选择新门限，不宣布疑点已证伪或已修复。这里的“已知”指已读到的定义或源码事实，历史报告读数按其原始范围解释。

现行定义仍由[统一科学模型](../../../docs/science/unified/UNIFIED_SCIENCE_MODEL.md)、专题 science 和工程合同拥有。实现已偏离确定合同的事项在文末另列，不混作等待科学自由设计的任务。

## 如何接续

每个问题保留问题位置、尚缺的论证、影响和所需证据。后续集中研究时，先固定被估计量、单位、条件与有效域，再查一手文献、比较参考实现并设计独立实验。外部书目和方法引用本轮没有逐项联网复核，不能把转录文献表当作已经读过原文。

既有[数值精度推导](../../normalize-precision-p5/NUMERICAL_PRECISION_DERIVATION.md)已经给出部分条件界；待办是确认假设与实际模型一致、取得可用误差量，不能重复把已有条件证明称为未证明。

## 光度、响应与观测量

### Q01 逐帧响应和最终混合信号究竟估计什么

位置：[光度分册](../../../docs/science/photometry/PHOTOMETRY.md)“原生通带绝对标定”及[UPM](../../../docs/science/sky/UPM.md)的观测模型。

已知：不同 R_k 下未知天体 SED 一般不能由一个标量无偏变换到 R_0，现行设计允许用户组合。待推导：排异集合、背景修正和空间权重变化时，有效混合响应随位置变化的完整表达，及何时仍能解释为一个固定参考响应的已成像信号。

影响：混合产品物理解释、恒星/星云颜色差、接缝评价、FITS 元数据。需要：受控连续谱与窄线谱世界，对响应、PSF、样本选择分别作对照。不能靠单个滤镜名或恒星零点残差解决。

### Q02 合成响应积分的数值归一和 SI 单位

位置：[统一模型](../../../docs/science/unified/UNIFIED_SCIENCE_MODEL.md)的 q 积分、[光度分册](../../../docs/science/photometry/PHOTOMETRY.md)的 Q 积分及 `frame_photometry_fit` 输入约定。

已知：响应是否已含 λ/(hc)、是否按带宽归一、是否含曝光/口径，会改变单位与系数的解释。待统一核验：每个实际积分 API 的输入谱密度、波长单位、QE 合成、输出量及 a_k/k_photo 的方向；常因子可被拟合吸收不表示物理单位正确。

影响：F_ref、Poisson、BUNIT 与跨设备尺度。需要：同一具名 SED/响应的独立积分、SI 换算与非平凡比例对照，区分积分光子率和带平均谱密度。

### Q03 空间 alpha、窗外通量和总通量参考

位置：[噪声分册](../../../docs/science/noise_snr/NOISE_SNR.md)“探测器、校准与光度标度”，[数据合同](../../../docs/engineering/contracts/NORMALIZE_DATA.md)“参考定义”。

已知：完整响应域上的 a_ref=Σalpha_i P_i，P'_i=alpha_i P_i/a_ref；有限窗不重新定义总通量。待核验：空间 alpha 在有限控制窗外不可求值时的可辨识性、截断偏差及是否需要改为明确窗内参考。现行合同“sum”表述需与科学分册的完整响应域一起解释。

影响：源 Poisson、模板归一、参考通量与 SNR。需要：空间梯度/曲率、椭圆 PSF、不同窗口和边缘的独立积分；本轮不选新的归一政策。

### Q04 测光参数、像元观测与共同误差

位置：[光度分册](../../../docs/science/photometry/PHOTOMETRY.md)的 C_cross、[校准](../../../docs/science/calibration/CALIBRATION.md)共享母版项。

待推导：同数据估计零点/空间场后，像元与参数的交叉协方差怎样在现有对角/低秩/局部载体中表达；跨帧共用 Gaia 谱和母版时哪些误差会相关。不能把所有未知项当独立 PSD 附加白噪。

影响：完整精度、upper_bound 和叠加效率解释。需要：带共享参数真值的重复仿真，与独立校准数据拟合情形对照。

## 校准、检测与 PSF

### Q05 平场、像元面积和弥散/点源响应

位置：[校准](../../../docs/science/calibration/CALIBRATION.md)“母版身份”及[Drizzle](../../../docs/science/drizzle/DRIZZLE.md)的 Ω 归一。

已知：flat 可同时包含 QE、照明与像元面积；球面映射又使用真实 Ω。待推导：不同 flat 生产约定下的前向模型、面积因素是否已经包含、点源与弥散源何时共享同一校正。不能用“flat 已归一”推断这些分量已分离。

影响：空间光度梯度与重复面积校正。需要：可控像元面积畸变、均匀天空与单位通量点源的联合注入。

### Q06 母版估计与修复后的相关误差

位置：[校准](../../../docs/science/calibration/CALIBRATION.md)、[CCD 缺陷](../../../docs/science/calibration/CCD_DEFECT.md)。

已知：固定线性算子下 C'=JCJᵀ，K=1 共用 bias 的代数抵消已有定义。待推导：中位数/剪裁母版、随机 flat 归一、数据依赖坏点掩膜/插值系数的有限样本误差，及对角 κ 膨胀在哪些下游量上有界。

影响：修复值不能当新独立观测、共享母版误差不会按帧数自然消失。需要：固定掩膜与数据驱动掩膜分开的重复仿真；不重新质疑已确定的两种暗场算术。

### Q07 检测阈值、平滑噪声与召回域

位置：[检测科学分册](../../../docs/science/detection/STAR_DETECTION.md)及[检测算法记录](../../../docs/science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md)。

已知：阈值使用未平滑噪声时，不是相同数值的平滑显著性；行差分尺度与裁剪 RMS 不可互换。算法记录含大量“实测”召回表，而专题附录明确部分读数缺复现载体。

待核验：表格对应真实代码、种子、核边界、子像元相位与分母；阈值扫描后择优和多重比较怎样影响置信声明。Clopper–Pearson 的固定试验条件不能自动用于选出来的阈值。影响：IPV 候选、科学源完整性和测试预期。需要：分 PSF/相位/背景的独立留出与纯噪声对照，历史表不直接转为生产保证。

### Q08 中心采样拟合与像元积分模板的一致性

位置：[PSF](../../../docs/science/psf/PSF.md)连续轮廓与像元积分，[模块 PSF](../../../docs/detail/normalize/modules.md#star-psf)。

已知：检测 Gaussian、拟合 Moffat4 与 canonical 单位总通量模板有不同参数/估计器身份。待推导：从中心采样拟合得到的 A、宽度和椭率送入积分模型时，欠采样与相位造成的系统偏差；空间凸组合的支持域和留出误差。

影响：测光通量、F_ref、噪声模板和信息量。需要：连续积分真值生成的星场，比较不同拟合前向模型，避免同模型自拟合自证。

### Q09 像元求积自检能给什么误差保证

位置：[PSF](../../../docs/science/psf/PSF.md)“空间像元积分响应”和既有[数值推导](../../normalize-precision-p5/NUMERICAL_PRECISION_DERIVATION.md)。

已知：有限节点可同时漏掉避开节点的窄核，两阶差趋零而真积分趋 1；该反例已有推导。尚需：限定真实宽度、相位、椭率、窗口的误差控制方法，取得传播到信息量所需真实 δP 界。影响：diagnostics 的 estimated_error 不能当严格证书。需要：独立自适应求积及边界收敛，不能只增加阶数后比较差值。

## 条件信息与稀疏重建

### Q10 QR 截断、通量残差与相对精度

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)“Drizzle 的条件信息传递”，`lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp` 的 `PivotedQr` 和 `complete_spherical_reference`。

已读源码仍有 `i_f = t_norm2 - proj`；条件估计采用 QR 主元范数比的平方。现行 science 已说明近共线相减风险，以及主元门限与谱阈值不同。

待推导/标定：分解误差、秩间隙与实际残差算子误差的联系，科学可辨识性、数值秩和相对精度分别怎样判定。已有 d、γ 条件界保留；尚未取得生产有效的 d、γ。需要：近退化解析族、独立高精度 SVD 及单位缩放对照。不能用一个 rel_tol 同时解释三个问题。

### Q11 半正定协方差与精确约束

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)“半正定协方差”。

已知：当模板不在 range(C)，零噪声方向可含精确参数约束，直接 C⁺ 会丢信息。待明确：真实模型中何时可能出现这种结构，以及目标公共求解接口是否支持一般约束或只声明受限能力。

影响：unavailable、无限信息和退化状态的解释。需要：范围内模型与精确约束模型的独立小矩阵实例，显著负谱不按 PSD 秩亏吞掉。本轮不设计新的求解器。

### Q12 条件 GLS 与“绝对深度”的关系

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)冻结 C_ref 与阈值深度段落。

已知：在 F_ref 处冻结协方差的 GLS 方差不是任意亮度的无条件 CRB。待推导：源 Poisson 显著、背景同估和模板失配时，SNR(F)=q 的阈值解、检测概率和测光覆盖的区别。

影响：不能把单点参考 SNR 改名为完整绝对 SNR 或 5σ 深度。需要：同估计器重复采样、阈值解与检出曲线的独立比较。

### Q13 参考源精度到实际场景面亮度精度

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)“精度桥梁”，[统一模型](../../../docs/science/unified/UNIFIED_SCIENCE_MODEL.md)。

已知：T=V_B/V_F 有单位，I_F=SNR²/F_ref² 只描述点源通量。待推导：只保存局部参考窗口时，哪些实际场景噪声/PSF/几何参数仍可重建任意位置 V_B；背景主导近似的可验证误差域是什么。

影响：Mosaic 统计权重与跨 PSF 比较。需要：相同参考 SNR、不同 T 的反例及真实场景 Poisson 对照；定义恒等式不能当成恢复算法，本轮不切换生产权重。

### Q14 稀疏插值、支持与不确定性

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)“稀疏控制、重建与相关结构”。

待推导：log V_F、log SNR、噪声尺度参数等不同重建目标的偏差、正值约束、支持空洞与外推误差；控制点误差相关时的预测不确定性。exp(E log V) 不等于 E V，样条平滑和裁剪均不能自动保证无偏。

影响：密集权重、边缘过冲、tile 连续与叠加效率。需要：空间块留出、跨帧真值、样条/IDW/常量基线对照；不按历史自然样条默认宣称已最优。

### Q15 支持面积、patch 分区与帧摘要

位置：[Normalize 数据合同](../../../docs/engineering/contracts/NORMALIZE_DATA.md)的面积加权 log SNR 摘要，[实施差异](../../../docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)的支持项。

已知：N=ΣwΩ、D=Σa、覆盖并集、叶面积和模型支持是不同量。待推导：确定性 patch 网格映到球面后的不重复面积、孔洞、畸变和局部窗截断如何形成摘要权；当前捕获邻域的 Σmin(D,Ω) 不自动等于该量。

影响：帧摘要、有效面积和跨帧深度比较。需要：可解析足迹分区、重叠/重复计面积负例；本轮保留既定摘要定义，不以实现可得量替换。

## 球面算子、背景与导出

### Q16 真实曲边、drop 定义和候选完备性

位置：[Drizzle](../../../docs/science/drizzle/DRIZZLE.md)、[几何算法](../../../docs/science/algorithms/export/DRIZZLE_GEOMETRY.md)。

待推导：WCS/SIP 像元边界与 pixfrac 收缩在探测器/球面的具体定义，曲边求积、chart 交界和候选搜索的完整误差预算。Σa=A 可在同一错误几何上成立，不能替代逐叶真值。

影响：常值保持、积分通量、相邻协方差和局部 R 的有效性。需要：独立曲边 oracle、极区/面缝/大尺度比与候选遗漏负例；不把 A=pixfrac²Ω 作为无条件恒等式。

### Q17 已归一信号、点源孔径与 HiPS 父层

位置：[Drizzle](../../../docs/science/drizzle/DRIZZLE.md)“正确的守恒对象”和“多分辨率”。

已知：ΣF_p 守恒不证明 ΣS_p Ω_leaf 等于原输入总量。待推导：部分支持、不同 Ω/A、层级平均和点源孔径下的具体积分定义与误差；Ω_valid 在最终只保留 signal 时能够提供哪些可恢复信息。

影响：层级科学语义、点源总通量与导出粗层。需要：完整/部分覆盖、不同 pixfrac 与解析点源的分段算子对照。

### Q18 背景规范的共同退化空间

位置：[UPM](../../../docs/science/sky/UPM.md)的 H_k、A_k 和连通分量规范。

待推导：有效重叠图连通之外，具体公共面/逐帧样条基的零空间维数、弱约束模式和可辨识秩；不同 H_k 时共同规范有哪些函数。图连通不保证全部高阶背景参数可辨识，正则只选择解。

影响：断连/弱重叠、真实低频、差模延拓。需要：明确设计矩阵的小型解析构型和已知低频天空注入；规范选择不能解释成真实天空零点。

### Q19 同数据背景拟合、稳健控制与场景方差

位置：[噪声](../../../docs/science/noise_snr/NOISE_SNR.md)“合成、同数据拟合与后选择”，[UPM](../../../docs/science/sky/UPM.md)“背景控制点”。

已知：z=y−Aβ̂ 的方差包含交叉项，β̂=Ly 且 L 固定时有残差制造者形式。待推导：稳健中位数、剪裁、相关 Drizzle patch 和数据依赖权重的实际控制方差及校正后任意位置方差；k_corr 默认值不是普适物理常数。

影响：背景模型权重、corrected variance 与合成精度。需要：无污染/结构污染、固定拟合/稳健拟合的分开 Monte Carlo；不能把点源 SNR 用作背景误差。

### Q20 排异路由、近零背景与后选择

位置：[排异](../../../docs/science/integration/REJECTION.md)的电平比例带、[Mosaic 模块](../../../docs/detail/mosaic/modules.md#reject)。

待推导：按标称样本量固定路由后，不同局部有效样本数、噪声、背景电平和 PSF 的误拒/漏拒/偏差。比例带围绕近零背景收缩已在 science 说明，尚需按生产 profile 与容错分支核实统计后果。

影响：真实瞬变、星核、负信号、低频保真与输出方差。需要：固定资格和数据选择分别比较，保留真实时变源的限制；不能把 1/Σw 当后选择后的通用方差。

### Q21 Export 层级启发式、球面核和 sky crop 包络

位置：[导出](../../../docs/science/projection/HIPS_TO_FITS.md)、[重采样](../../../docs/science/resample/RESAMPLE.md)、[Export 模块](../../../docs/detail/export/modules.md#wcs)。

待推导：等面积平均尺度选层对局部最坏方向采样的误差，四象限插值相对目标面积平均的差别，有限边界采样能否包络连续 sky crop。TAN 奇点取决于距切点角距，不能以绝对 Dec 的实现限制替代理论。

影响：有效分辨率、孔径光度、裁剪遗漏和极区。需要：独立 WCS、解析边界极值、非对称裁剪及一次/两次重采样对照。

### Q22 HiPS 标准条款与项目能力的边界

位置：[HiPS 格式](../../../docs/science/formats/HIPS_TILE_FORMAT.md)关于格式封闭词表、tile_width、压缩与多扩展的表述。

待查一手原文：哪些是标准 MUST/SHOULD、哪些是 ACSD 仅支持 FITS/512 的实现选择；“客户端无支持”“非二次幂在规范内合法”等断言需逐条核验。本轮不根据记忆改写标准条款。

影响：外部兼容、归档与 reader 的拒绝范围。需要：标准具体条款和至少一个独立 HiPS 生产者/读者；标准格式与 .hips.zst 私有传输容器分开解释。

### Q23 文献和参考实现支撑范围

位置：各科学分册及算法补充页的参考文献表、[核验文献记录](../../shared/references/CORE_REFERENCES_VERIFIED.md)。

待核验：书目信息、公式位置、实际采用部分、源码版本与许可证。ROOT README 与算法页对部分项目的许可描述不同；本轮不保留 README 作为第二份许可表。书目存在、读过方法与已用实现分别记录。

影响：方法归因、创新性、可复核与许可。需要：逐条读一手论文/标准和实际版本源码；本轮没有完成这项外部审核。

已读测光文献注记还将“甚弱分光标准星网络”关联到题名为 K-corrections and filter transformations 的 [16]，将“大气消光”关联到讨论 reddening/SFD 的 [41]，并含“产物只以星等表达”“未测项不加、上限偏严”的模型断言。前两项需核对原文是否支持具体量，后两项不能覆盖当前线性产品与条件上界定义。来源原文已[完整保留](PHOTOMETRY_REFERENCE_NOTES_SOURCE.md)，正式阅读页保留 44 项书目并改为核验任务，不宣布外部引用已获证实。

## 资源测量与证据解释

### Q24 性能采样代理、利用率分母与证据身份

原文：[来源档案](OBSERVABILITY_SOURCE.txt)中 observability 的“活跃计算线程数的 p50”、PERFORMANCE_MODEL 的“无就绪积压同样计违规”与“无租约哨兵不得回填”；[监控来源](RESOURCE_RECORDS_SOURCE.txt)中的“原始 CSV 不可手工合成”、`lock_wait_ns_est` 和 `io_wait_rate_est`。

分歧：当前 TraceSnapshotObserver 的 active_workers 是活动节点数量，granted_workers 是整个快照内正租约最大值；缺页/非自愿上下文切换不是块 I/O 等待/锁等待的直接测量。资源正文对队列条件和容量回落互相冲突，JSON 和各执行器尚需逐项对应。公开固定盐的哈希链和只读权限也不能证明数据来源真实性。

影响：CPU 利用率、排队归因、跨并发效率及性能证据可信度。所需证据：给出每个被测量的操作定义与量纲、独立线程/队列 oracle、同一运行的实际租约时间序列、多 run 混入与旧快照负例、缺页但无块等待及抢占但无锁等待的对照，另明确完整性与来源认证的不同证据链。本轮只改解释、保留原文，不调整阈值、采样算法或科学数值。

### Q25 HEALPix 离散往返与边界容差

原文：[HEALPix 来源档案](SPATIAL_NOTES_SOURCE.txt)中 Postconditions 的“round-trip误差≤1e−12度”以及 oracle 的“≤1.2×hp_res+1e−9”，并声明极区角点另列退化面。

分歧：任意方向→离散叶→叶中心、叶索引→中心→叶索引和连续坐标互逆并非同一个检验；原文没有清楚限定1e−12对应哪一对象。共享头中的百万点无mismatch属于历史有限集合声明，指定测试文件本轮未取得，不能代替完整误差/完备性证据。

影响：位置误差、候选归属、tile/leaf比较与所谓坐标精度上界。需要：明确方向集合、各阶/边界归属政策、独立参考和量化误差与浮点误差分离；本轮不推导新的通用界，不修改坐标内核。

### Q26 ISA数值误差的比较尺度与独立参考

原文：[CPU来源档案](CPU_SOURCE.txt)中AVX2_PROVIDER的“baseline相对科学参考在容差内，故avx2相对baseline在容差内⇔相对科学参考在容差内”、`|a−b|/max(1,|b|)≤2e−4`及“每元素数ULP”等断言。

分歧：带1量级地板的指标不是全部值域的纯相对误差，两个独立比较的同阈值结论也不能直接互相等价。FMA改变舍入，强相消、次正规、非有限、编译器优化与下游归一可能改变误差解释，共享公式源不是独立真值。

影响：后端等价、基线验证传递、性能选路前的数值责任。需要：固定单位/量级/有效域、直接variant对高精度reference、相消与极值反例、ULP/绝对/相对误差分离和传播到科学估计量的预算。本轮保留2e−4既有配置，不重推界、不改kernel或选路。

### Q27 通用容差、归约误差与 ULP 适用条件

原文：[测试来源档案](TESTING_SOURCE.txt) TEST 第4节称“任何绝对容差…atol ≥ 1 ulp(scale)”“低于1 ulp的绝对容差不可满足”“超出该域…只放宽不收紧”，并按“被比较量的dtype”选u，将 C≤4 与 γ_n 作为通用归约预算。原表还用未定义δ的表达式解释单位舍入。

分歧：精确相同结果的比较与一般近似的误差保证不同；绝对/相对容差的组合、参考量级和相消影响判断。输出、计算、累加dtype未必相同，γ_n需要操作与舍入前提，不能只凭输出类型和项数证明所有算法。与Q26的ISA误差相关，但本项覆盖全仓比较器。

影响：错误放行、误拒、跨线程/ISA数值等价及科学精度声明。需要：原始数值分析文献的具体定理、舍入/溢出/下溢域、真实比较器、准确相等/近零/相消/混合精度反例与估计量误差预算。本轮保留原数值和完整原文，不重推界、不改阈值或测试代码。

### Q28 验证矩阵中的目标量、最优性与接缝阈值

原文：[测试来源档案](TESTING_SOURCE.txt) VALIDATION_EVIDENCE 4.2–4.4把 point_information、surface_gls、psfsw_robust列为生产模式，并称 equal/pixel_ivar“禁止科学最优声明”；11.6给 max abs(rel_step)≤1e−2、采样半距2px及“方差比对电平阶跃原理性失明”。

分歧：旧模式列表与现行统一科学链不同，验证文档不拥有生产模型选择权；最优性取决于目标量、无偏约束、响应/PSF、协方差与支持域，不能仅由模式名字允许或禁止。相对台阶在局部背景接近零、负值或真实结构梯度下的定义域和阈值来源未充分说明，统计量的失明主张也需明确模型。

影响：科学基线比较、背景保真与接缝判据、退役模型被验证规范重新启用。需要：同一估计量下的模型/条件、独立构造与真值、各指标的正负例、阈值推导和经验标定样本。关联Q13/Q18/Q19，本轮只统一文档职责，不恢复旧模式、不改科学算法和门限。

### Q29 覆盖分数、包围盒与保守候选

原文：[存储来源档案](STORAGE_SOURCE.txt) detail第6节定义frac为“覆盖的面积比例，按有效域包围盒计，量化u8”；第5.2节称其使阶段二“跳过近乎空的块”，并称块级索引使“可用性不损失”。

分歧：真实有效域面积、包围盒面积、tile存在性与量化值是不同量。细小但非空支持可在取整后成为零，包围盒也可能含孔洞；仅据比例或阈值剪枝是否保守尚无明确证明。

影响：候选帧完备性、边界微小覆盖、掩膜孔洞与最终通量/覆盖。需要：面积定义、量化/舍入规则、最小非零编码、候选条件以及细条/孤立像素/曲边独立反例；本轮不推导新量化界、不改索引或剪枝。

## 已确定设计下的实现与文档问题

这些事项不需要再选物理模型，后续按确定合同实施或核实，不能混入 Q 条目作为设计授权。

| 标识 | 已读事实或文档冲突 | 影响与后续动作 |
|---|---|---|
| E01 | `parse_reference_config` 对 sed_response 写 not wired，explicit_linear_flux 有可执行解析 | 参考 SED 积分仍需实现，不能据接口字段存在称已完成 |
| E02 | `P1NodeModule::execute` 分派真实八节点，descriptor 与 p1_*.json/FITS 工作载体并存 | typed Runtime 仓、revision、消费者 lease 与实际取数逐边迁移 |
| E03 | `FrameScienceDocument` 有 input_occurrence，生产 fdoc 用 `p1_frame_key(lp)`，差异地图记录 basename 身份风险 | 重复输入、同名帧按 occurrence 合同修复，不重新设计身份 |
| E04 | sidecar 默认 ADU/sr，差异地图记录叶阶 k−9、application_id 与 applied 分支问题 | 按实际单位、叶阶和应用身份同步 writer/reader；本轮未运行反例 |
| E05 | 因子模型 verifier 存在，差异地图记录通用 reader 未调用及基础单位校验缺口 | serializer 可写不代表 reader 完整，按合同接入独立读取 |
| E06 | 来源 API 清单的“负 flux 即拒绝”与最高设计冲突，混有五态集成、WCS 依赖科学源、IPV 未编入根构建及相互矛盾的并发说明 | API 全文已读并归档，正式页已重写为通用合同与真实头/模块定位；不修改实现，也未宣布全函数已验证 |
| E07 | `p1_op_drizzle` 在 `hp_drizzle_run_phase1_hips` 后调用 `aio_pipeline_frame_destroy(frame)`，后续 exposure fallback 分支仍调用 `aio_frame_kv_get(frame,...)` | 静态发现释放后访问路径；是否命中取决于原始 EXPTIME 分支，需单独缺 EXPTIME 构型验证并修实现；本轮只登记 |
| E08 | 格式页仍以 signal/support/variance/ivar 全集泛称科学子产品，阶段二只允许最终 signal | 文件能力与各阶段发布政策分开，不能从格式库能力推断阶段二发布层 |
| E09 | 配置 source_ref/anchor 中有旧章节、行号、内容哈希和旧登记页路径 | 路径迁移只修定位，不能使 quote/hash 自动变为有效证据；需逐项同步，不能改写历史观测 |
| E10 | 检测等科学附录和算法页混入未绑定可复跑载体的“实测”数字 | 过程按来源保留，正文不提升为现行科学事实；本单元保存抽离记录 |
| E11 | 原始实验有已失效 Markdown 目标和旧任务提示 | 历史原文保留，清单记录迁移和来源身份，不能让任务提示重新指挥当前生产 |
| E12 | 算法补充资料仍含未全文核对的大段实现清单；API 来源清单全文已读，但全函数实现核对未完成 | 文档阅读、接口重写和实际函数验证分别记录，不用自动扫描代替完成声明 |
| E13 | 部分 extern "C" 库头含 C++ 的 cstdint/std::uint32_t；C 链接符号不自动表示头文件可由纯 C 编译 | 区分内部 C++ 可调用头与跨库 C ABI 目标，未来逐头核验实际 C/C++ 编译能力；本轮不改头布局 |
| E14 | runtime.h 注释称 ctx 由 Runtime 创建持有，runtime_client::run_pipeline 实际在栈上创建 RunContext 再借给 run | 工程正文已按真实所有权重写；异步消费者不得超出调用方寿命，源码注释同步留待实现维护 |
| E15 | 源码注释、API 文本匹配测试及 anchor/source_ref 仍可能引用退役正文、旧行号或旧章节 | 结构链接检查不覆盖这些程序化文本依赖；后续分别判断是否有效语义测试或需退役的文档冻结检查，不以变动条目数冒称 CI 全绿 |
| E16 | 日志合同要求 `log_artifacts[]`，当前 CLI 生产目录未找到对应写出；Core Error 也不含完整目标 error_report 的所有字段 | 日志生成器、类型存在与完整运行层接线分别报告；正式正文已改为合同目标和真实入口 |
| E17 | TraceSnapshotObserver 复制构造时事件，observe 未按 run_id 过滤，active_workers 为节点数，租约取快照内最大值 | 调用者必须先限定单运行；实时订阅与真实忙碌线程尚需实现和验证，不据字段名认定已完成 |
| E18 | 资源监控旧文 salt 为 acsd-monitor-timeseries-v1，实际 monitor.py 为 acsd-monitor-v1；公开哈希链只作一致性检查 | 已按实际序列化解释，格式和代码不改；不能把可重新生成的链当来源认证 |
| E19 | 核心 contracts.h 仍重复声明 core::ExitCode，exit_codes.h 也有 acsd::ExitCode | 规范声明源与实际重复类型分别说明；本轮不改 ABI 或枚举 |
| E20 | RELEASE/BUILD_NODES 将 build_source_digest 相同当“同一二进制”；实际摘要仅覆盖 gen_build_stamp.py 声明源集，未覆盖编译器/选项/外部链接库等全部输入 | 正式文档区分 configure HEAD、构建 HEAD、声明源摘要和实际二进制哈希；不修改指纹代码 |
| E21 | 旧构建闭包表和阻断叙述已过时：gen_build_graph_doc.py 的 DOC_REL 现为正确路径，NONPROD 的 coverage 工具登记仍与根图不符；文本解析也不能代替实际配置闭包 | 旧表完整归档，正式页提供真实目标/调用定位与工具限制，未运行生成器覆盖正文或编译产品 |
| E22 | make_*_release.py 使用旧响应曲线目录、版本回落字面量和固定生成时间；安装规则已改 config/response_curves，Windows 脚本只生成 CLI zip | 交付目标与已有材料生成器分别说明；本轮未打包、升级版本或发布 |
| E23 | ARTIFACTS 旧表把 DATA-IMG-WEIGHT-001 同时用于[0,1]质量因子与积分逆方差，并称点源 SNR²/F_ref² 可直接作信号权；还把未知 variance=0、最终 P2/P3 辅助图像写成现行合同 | 已按统一工程对象和最高设计解释 legacy 哨兵、同估计量精度及单 signal 发布；保留全部旧 ID/原文，不改 schema 或权重代码；统计重推导见 Q13/Q19 |
| E24 | production_store.py::publish 实际依次 rename 内容、COMPLETE manifest、hash sidecar、provenance；中断可能留下 COMPLETE 而缺后续文件，且部分 open 绕过 StoreIO | 原文“任一失败无 COMPLETE”“同一原子区”“spy证明全部读写”过强；已区分恢复完整条件、最终验证读和真实生产接线，未修代码或运行故障注入 |
| E25 | provenance.parse_version 只取数值段，忽略预发布；revision.module 取模块 ID，revision.abi 从文档形态派生；strategy 不进摘要 | 正文说明实际绑定和摘要范围，不能声称完整 SemVer/C ABI/算法身份均已核验；不改载体 |
| E26 | ATOMIC 旧文称 overwrite先删旧产品，实际 HipsOutputStore 已先暂存再让位旧副本并尝试恢复；C++ PublishResult 也有已可见但持久化未确认状态 | 按当前 publish_directory 与 atomic_publish.h 说明真实次序/三态，旧过强或过时结论归档；尚未验证进程崩溃、竞态及跨平台持久化 |
| E27 | Gaia 查询缓存用 double 的 `==`，旧文称逐位比较；client数据集键含类型/文件数，不能独自认证完整内容身份 | 已按实际缓存比较和绑定范围解释；不更改缓存键和查询算法 |
| E28 | HEALPIX_MAPPING 列 tests/test_healpix_oracle.cpp 为实存可运行，本轮当前目录找不到该文件；共享头仍留百万点历史声明 | 正文保留历史声明身份，测试/输入/日志需回实验取证，未把不存在的文件称可复跑 |
| E29 | PSF 旧算法页称DPSFFitResult12字段，实际头13字段；DPSF_FIT与DPSF_PSF_STATUS同值异义，旧文部分表混用 | 三种九列布局、JSON对象和两套状态域在模块正文分别列明，完整旧原文归档；本轮未改变结构体或算法 |
| E30 | CACHE对仅本地Gaia又要求外部请求每组一次/两级落盘；CODE要求文档所有轮次永久保留，COMMENT却要求迁历史；目录指南还称已跟踪的build文档未入库 | 正文按本地缓存、机器兼容身份、正式说明与历史证据分工重写，目录导航用真实链接；没有改缓存或构建实现 |
| E31 | SCHEDULER称探针行与CLI JSONL同格式兼容，但两schema必填字段及kind枚举不同 | 正文区分独立probe与显式stdout适配，保留机器schema不改 |
| E32 | BoundedAsyncQueue非空error时pop立即nullopt，不排空；cancel空reason时has_error仍false；容量函数即使预算不足也至少1 | 已说明真实close/cancel/所有权与内存边界，不改队列或声明测试已过；需覆盖空原因和中断构型 |
| E33 | FITS_IO低层“不施加缩放”被泛称全AIO行为，生产aio_fits.cpp的native/CFITSIO分支却返回已解码物理值；纯C流式与内部C++ writer也混称统一接口 | 正文区分三层入口、缩放责任、输出部分写和状态域，避免重复BSCALE/BZERO；未改变读取代码 |
| E34 | HIPS_IO把归档读取写成已有保证，AIO旧细节同时登记archive写读/索引/storage未接线；接口枚举还被当阶段发布集合 | 正文区分目标、实际裸读与各层能力，单signal产品政策保持；不凭文档宣称归档生产支持 |
| E35 | cmd_verify未重算cpu_profile摘要，也未运行完整run_manifest schema；writer基础字段两项profile为null，started/finished都在收尾取now，且只flush/rename不能证明完整fsync | 正文按真实写出/验证顺序与范围说明；读取完整函数后记录，不改清单或版本策略 |
| E36 | CLI detail将运行--json写成单JSON、-force写成越过全部检查、日志双文件/清单接线和CPU资源退出10写成已实现 | 依据command_tree/subcommand/parser/commands改写真实JSONL、结构硬检查与磁盘错误，取消不再笼统删除已发布对象；不改CLI代码 |
| E37 | capability_detect.c实际位置含src；cap_classify的AVX-512分支检查0xE0但独立于0x6分支，文档整组策略/完整OS前提和硬件一般定义混写 | 正文给真实入口与分支范围；合成不一致XCR0证据、硬件规范前提及检测/执行安全须后续核验，本轮不改能力位或探测算法 |
| E38 | ISA_VARIANTS两族已拆门面/计算TU，AVX2_PROVIDER仍称整TU高级旗标；BACKEND/SECURE_LOADER混写加载安全强保证和尚未实现的Windows路径 | 统一两族ABI/清单/桥、门面隔离与Windows unsupported现状；hash到dlopen竞态/依赖解析尚需独立验证 |
| E39 | 旧MANIFEST称validate_cpu_profile无生产调用，但当前commands.cpp实际调用后还独立进入memory/backend消费；benchmark可回落用户目录且stdout是路径+verdict | 正文按新调用说明，不据前置校验失败日志推断后续绝不消费；profile与run_manifest身份记录仍待完整核验 |

### 远端合并后的核对范围

| 标识 | 已读事实或文档冲突 | 影响与后续动作 |
|---|---|---|
| E40 | GUI 文档把规划 gui/ 工程与完整分组/事件界面写成实存，浏览器却仍用旧 Alpha 无 GUI 限制；现有独立 Qt 浏览工程不等于完整产品 GUI | 正文区分目标、组件与集成证据，不新建 GUI 或修改产品范围 |
| E41 | GUI 草案使用 fraction/cpu_util/mem_rss/log kind，与现行协议字段不符；中断清零还混淆未知与实测零 | 依正式协议解析，最后观测带陈旧标识；取消成功取实际退出和收尾证据，不凭按钮状态 |
| E42 | STFEngine::auto_stretch 按大于默认零阈值过滤并 sort；旧正文宣称 finite && support>0、nth_element 与 MAD 链 | 已说明静态实现及合法非正值/正无穷风险，仍需调用链与显示构型验证；本轮未改 STF |
| E43 | GUI 把 SET-TEMP 当实测 CCD-TEMP、TELESCOP 当相机，并将原始校准帧路径写成 master_* | 分别保留元数据含义和 master 身份，别名需设备/单位依据，不用名称匹配证明校准成立 |
| E44 | 测试规范强制缺失不能用NaN，与最高设计Export无覆盖NaN相冲突；同输入整文件逐字节也未排除时间戳/run_id | 比较产品定义的有效状态与科学payload，完整性和规范摘要分开；不改NaN编码或哈希代码 |
| E45 | 验证正文混入旧qa-design、门表、只准NumPy/stdlib与同时依赖Astropy、强制另一代理签字、文档禁止机器检查等矛盾流程 | 过程原文归档，现行正文按上位设计解释独立验证、证据范围和研发不设审批；未执行旧任务包或声称历史测试现已通过 |
| E46 | 存储合同一处要求normalize逐批输出coverage.index.json，另一处指定其仅由mosaic发布；storage_form/coverage_index在parser白名单与dead_config_keys中分别识别/登记未接线 | 正文保留字段、身份与目标，明确生产者/消费时序冲突待统一；不能据CLI认识声称归档已写出 |
| E47 | 当前publish_finish对PUNCH_NO_RELEASE删暂存并失败，旧detail却称跳过优化后发布；PUNCH_IO_ERROR包含post_hash_failed等验证失败但writer警告继续 | 正文说明真实分支，优化未收益、无法校验与数据不一致分别核验；本轮不改打洞或失败策略 |
| E48 | aio_hips_set_geometry只检查非空指针、存JSON字符串，manifest直接拼写；原文由可选键推导加性兼容和几何合法过强 | setter存在与输入/同帧/WCS合法分别验证，固定36系数只是历史读数；不改geometry格式 |

本轮已合并远端 `5b33e0417`，保留 Normalize Round 3 的空间 PSF、空间 alpha、typed 边与实验记录。上述 E01–E15 来自合并前静态抽查，不能在新实现下无条件当作仍然存在的缺陷，也不能因提交说明宣称已解决；逐项重读新调用与消费者后更新。原始 191 篇清单继续固定基线，新增代码/记录另外登记。

## 本轮证据边界

本轮阅读正本、模块登记、公共类型和若干真实调用段，没有编译或执行科学管线，也没有重新获取全部真实输入。Q 条目是后续研究问题；E 条目中明确给出的源码片段是静态事实，其余实现缺口引用现有差异地图，尚需逐项当前运行核实。不得把这里的“记录完成”称为科学重推导或功能修复完成。
