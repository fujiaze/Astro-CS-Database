# 科学与数学问题：集中待推导记录

本记录保存文档整理中遇到、需要后续统一推导或核验的事项。条目按当前证据区分待推导、历史反例和已修复实现；Q10的旧相减缺陷已修，Q03/Q35的位置参数语义已按Issue6明确授权统一，剩余误差/有效域仍待核。除这些明确订正与现行恒等式外，本轮不提出替代模型或新门限。这里的“已知”指已读到的定义或源码事实，历史报告读数按其原始范围解释。

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

现行统一：F_native是原生解析总通量；正式控制点F_ref(u)=α_*(u)F_native为位置参考幅度，h_ref=(α/α_*)P，不要求完整积分1。实际applied积分总通量为F_native Σ_all αP，两者一般不同。完整域单位归一的另一参数化仍保留，确定性c=a_all/α_*时V_total=c²V_ref、配对SNR不变。有限窗sum仅诊断，旧窗内重归一问题不重新引入。

剩余待核：完整域α不可求值时的实际总量/尾部误差、连续位置与最近像元回退误差、空间场估计协方差及交叉项。需要空间梯度/曲率、不对称PSF、不同窗口/边缘独立积分；位置参数可计算不代表实际全域总量可计算，本轮不改算法。

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

历史反例：旧`i_f=t_norm2−proj`在近共线时相消；当前217e56d99所含实现已两轮显式投影求e、i_f=Σe²，故此缺陷标**已修复**，不再指控当前分支。条件估计仍为QR主元比平方；接收条件i_f>rel_tol·‖t‖²的范数比阈为√rel_tol，与QR列范数门限不同。

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

### Q30 留出星χ²与条件精度验证的解释

原文：远端[第三轮报告](../../normalize-precision-p5/ROUND3_FIX_REPORT.md)第6.3节由χ²/dof中位0.54–0.89声称“模型+噪声自洽”“参考人群…经过真实留出数据验证”，并由faint端0.54给出“噪声模型偏保守约35%”；第8节用空间留一法L1均值0.135对比圆模型逐点相对差中位0.470，称改善约3.5倍。

分歧：χ²的分布/自由度需实际残差、拟合重用、相关、选择与背景模型；样本中位统计不能无条件代替方差标定。两种模板指标使用不同分母、汇总和留出集合，不能仅凭数值相除证明同口径提升。报告有真实程序和记录，但这些解释仍需独立核对，不能据正文归档认作已证伪或已完成验证。

影响：PSF与条件噪声的真实支持、参考SNR保守性和跨模型比较。需要：冻结留出星集、参数是否训练/重拟合、残差与权重、自由度/相关校正、统计抽样分布，以及同样本同指标的对照。本轮不重跑或调整噪声与PSF模型。

### Q31 UPM控制方差、归一化与弱锚误差保证

原文：[Mosaic归约来源](MOSAIC_REDUCERS_SOURCE.txt) UPM_SOLVER称k_corr<1“物理不可达”、k_corr=1须拒，弱零锚0.001给“正则化偏移<~0.1%”，并将各控制格归一为control_reliability后的份额称求解权重；误差排序直接写FP64≪1e−12。当前p2_upm_control_variance确实实施对应k_corr拒绝规则。

分歧：有效样本量与相关系数政策需限定统计量、协方差符号/结构和有限样本域，不能把正相关假设推广到所有背景控制。弱锚偏差依设计矩阵、数据尺度/条件和被估量，不能只由正则系数保证固定百分比。格内归一改变绝对权重和跨格目标，需明确其统计解释。

影响：控制逆方差可信度、相对背景解、断连/单帧区和真实低频保真。需要：实际控制统计量/相关模型、有限样本标定、独立解析与MC、归一前后目标和正则偏差界。关联Q18/Q19/Q27，本轮不改k_corr条件、锚或求解器。

### Q32 检测算子的核、边界与去混叠等价

原文：[检测来源档案](DETECTION_SOURCE.txt)逐算子页把1.4281与文献转述1.4251同时列作待核，声称Triggs边界矩阵而当前sdet_yvv_pass用首/末常量种子；同页O11称论文原页已核验，存疑清单却称多路未取得。另有“间隔<2σ不可分”“argmax重分配与原文同构”“低宽档非单调是另一物理区制”及饱和标记推论。

分歧：实际离散IIR与连续高斯核不同，边界算法和系数改变需以真实核/响应比较；分裂的可辨识性受通量比、相位、噪声和背景影响，argmax规则不自动等于参考实现。有限样本起伏、拟合振幅与像素峰值也不直接证明物理区制或探测器饱和。引文自相矛盾不能由选择一段当已核实处理。

影响：阈值显著性、近邻召回、边缘与饱和源、原文归因。需要：原始论文/作者版本源码、边界和冲激响应独立对照、宽度/相位/不等通量双星及虚警构型；统计召回与条件见Q07。当前系数、阈值和分裂规则保持，不切模型或重新标定。

### Q33 稳健拟合的尺度、中心与误差主张

原文：[天测/光度来源档案](ASTROMETRY_PHOTOMETRY_SOURCE.txt)PHOTOMETRIC_FIT称固定MAD尺度Tukey“继承50%崩溃点”、更新尺度即降到11.9%，并称残差中心偏移δ时MAD一阶偏差δ/σ；sigma_cal_rel与光谱积分误差有较强泛化。PLATESOLVE由1.345·median(abs(r))直接援引Huber 95%效率。

分歧：红降权目标的根/初值选择、尺度估计、污染模型与有限样本决定鲁棒性，不能只凭固定/更新尺度给全算法保证。中心偏移的响应依分布对称/光滑性与样本选择，线性阶数需推导。尺度统计、参数标准误、单源误差和系统误差不能互代，未归一的绝对残差中位数也不自动等于标准差。

影响：光度精度、失配/污染下标度、天测稳健化和下游噪声。需要：原始M/MM理论和实际目标/停机/根选择、中心偏移解析与MC、有限样本/离群矩阵及独立单位积分误差。关联Q02/Q04/Q27，本轮不换鲁棒函数、尺度更新或阈值。

### Q34 图像与参考星表按亮度同位次配对的有效域

原文：[天测来源档案](ASTROMETRY_PHOTOMETRY_SOURCE.txt)PLATESOLVE第4a.5称“位次是两侧样本唯一…同域判据”，要求W只取图像U在完整检测序对应的相同星表亮度位次；当前ipv_select.cpp的select_image_stars/位次窗口代码确实执行这一政策。

分歧：通带/颜色、探测完备性、饱和、遮蔽、混叠、变星与不同视场支持会改变亮度排序；相同序位未必为同一恒星，跨序位几何匹配也未必非法。无饱和时退化为相同策略只说明两代码分支等价，不证明物理对应完整。

影响：Gaia候选召回、IPV成功率及参数/空星表错误归因。需要：同一真星身份下的不同响应与漏检/饱和仿真、完整匹配候选/实际求解、query/capped状态和真实帧分母。该项待统一推导，不改变选星/查询实现，不据缺WCS头否定解算能力。

### Q35 完整响应积分总通量与位置标度参考的差别

原文：科学NOISE_SNR第2节及NORMALIZE_DATA第5节用完整响应域a_ref=Σ_all alpha_jP_j、P'=alphaP/a_ref、F'=a_refF配对。新远端[ROUND4](../../normalize-precision-p5/ROUND4_FLUX_CONVENTION.md)及当前代码改为alpha_star=alpha(u)、h=(alpha/alpha_star)P、F_ref=alpha_star F_native，窗口sum只作诊断；报告请求正式文档改为此约定。

统一结论：Issue6明确授权采用位置参考参数，正式NOISE_SNR/NORMALIZE_DATA/Normalize接口已写F_native、F_ref=α_*F_native与F_applied,total三者分名，并保留另一总积分参数化及V/SNR换算。ROUND4及部分源码仍把F_ref叫applied总通量，这些措辞属于历史/代码注释偏差，不能反过来覆盖正式定义。旧报告数值不因改名而重新验算或宣告无条件成立。

剩余待推导：α_*连续/近像元回退及完整域求值条件、O(abs(m'')/8)是否漏梯度/二维项、估计α的参数不确定性及跨域transfer的配对；需要独立空间场/PSF模拟与可求值模型。旧“本轮不选约定”的授权状态已被本次用户指令更新；仍不修改Agent正在处理的生产源码。

### Q36 坏列方差记账、检测退化与物理分类

原文：[Cosmetic来源](COSMETIC_SOURCE.txt)F6.4及当前column_variance_inflate用(w_L²+w_R²)·var_in[修复位置]·κ，列作修复方差；检测章节把MAD>0作为成立前提，以尺寸排除天体并将并集来源标签称置信度。旧表还对任意NaN注入给出确定全false预期、将偶数双中位约定视作误差。

分歧：两锚点异方差或相关时，需要各锚方差和协方差，修复位置旧方差不能代替它们；中位数修复的非线性、检测/选择和真值预测误差另分析。非零MAD不保证缺陷分类成立，NaN破坏选择比较后也不能由单一构型推定全域确定结果。两中位均值属于明示定义，不自动构成偏差。

影响：修复信号的科学资格、局部噪声与下游参考精度、坏点/修复计数。需要：实际J/非线性估计器、异方差/相关锚与MC、母版/天体结构真值、退化与有限样本条件。关联Q06，本轮不改κ、掩膜、插值核或方差代码。

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
| E49 | acsd_diagnose.py把exit_code=2文本归DLL错误，仍建议旧rejection_cli、私有环境开关及旧产品回退；参数实际接受任意存在目录 | 排障页按当前错误模型说明并标工具启发式局限，不执行/修改诊断脚本 |
| E50 | 共享库旧文把SHA实现等同frame_id全链唯一规范；当前p2_frame_id另组所选properties/原生float/support/可选SNR并截断，未自动覆盖frame_science全部模型 | 区分哈希原语、tree_hash、内容帧身份和occurrence；跨平台字节、时间字段及载荷覆盖需另核验，不改身份算法 |
| E51 | PrecisionContext是普通字段兼容单例，旧文称全链跨DLL唯一及无锁安全；Sha256重复final实际返回全零字符串，final后update静默返回 | 正文给实际启动约定和误用行为，不将单例代替显式对象精度/ABI合同；本轮未改同步或SHA |
| E52 | 远端已补空间PSF、pixel_alpha_at/逐窗alpha、显式通量残差、因子基础单位检查、逐点求积诊断与frame_science排序边，旧架构/差异地图仍称这些全部未接线 | 已重读实际新增源/改动与调用并同步对应文档；文件carrier仍非完整Runtime typed仓，方法名仍不证明谱条件数界，本轮未重跑远端实验 |
| E53 | spatial_psf接口无支持孔洞/无效区域约束；stage_frame_precision对凸包外/构建失败回退帧常模型，而科学正文无支持位置要求unavailable；barycentric容许微负lambda，响应求和跳过非正项而未重归一 | 正文区分实存空间能力与完整支持合同，记录需统一的状态/边界；不据凸组合口号证明每个浮点边界归一，不在本轮改回退或几何算法 |
| E54 | PHASE2_INTEGRATION仍称89行/83行、五态、旧stage2/ACR生产且内部并发不安全；当前integrate.cpp/h有NON_FINITE_OUTPUT=5，实际CLI消费者在p2_op_integrate | 算法正文只保留数值方法，真实API、六态、计数和消费者补入模块说明；不改变reducer或ABI |
| E55 | UPM_SOLVER仍列已退役p2_upm_normalized_weights及M+C为生产，当前adapter拟合/施加使用p2_sky_plane_build_joint/eval_delta_block | 现行职责与保留库内核分开；原长文和历史权重/容差来源保留，不改公共面算法 |
| E56 | reducer的n_finite在weight检查前增加，与头注释不符；多种非OK分支signal仍为memset零而由调用者转换；support也未检查≤1 | 模块页逐项解释真实状态/计数，不据库默认零或n_used判断成功；未修实现或运行边界用例 |
| E57 | 检测旧文SDetParams9项/仅2项消费已过时，当前13项且fitRadius用于额外盲检形状窗；f64入口的flux/mag/extras仍float | 正文按当前头和sdet_apply_psf_shape_gate解释字段/精度，不改布局或将f64泛称全部FP64 |
| E58 | sdet头称预测中心index+0.5，科学正本整数为中心；当前guided adapter直接传pred并把gx/gy赋StarSource、lround取峰，需沿预测/PSF全链核对单一转换 | 记录坐标风险与静态调用，不声称已运行半像元反例；不改坐标或模型 |
| E59 | guided库允许零源成功，p1_op_star_psf_impl却直接DATA失败；输出flux实际为Gaussian振幅，旧文多处混称积分flux/十数组同精度与Moffat拟合 | 区分库/adapter/目标能力、真实列类型与估计器，下游测光/PSF绑定另核验；不以盲检回退替代科学源 |
| E60 | 光度长文一处称IRLS触顶必须scale=1/fit_used=0，另一处及当前cleanAndScale却无converged标志、仍发布最后location/scale；S=0也不必意味着所有r相等 | 正文分开实际停机/输出与目标能力，零散度/未知精度配状态；未修迭代或改变返回策略 |
| E61 | 天测长文将非生产fit_sip Huber与extract_wcs_sip/robust_refine拼作同流程，称Gaia网络I/O主导、求解全单线程，与本地星表和实际投票并行不符 | 科学数值方法与模块接口分层，保留源码路径和历史表，不把命名/旧日志当当前生产性能证据 |
| E62 | 新远端增加alpha_star模板/Poisson配对与平面载荷、PSF in-circle尺度归一/dropped_points、applied application_id及真实非恒定alpha记录，另有同样本同指标空间PSF留出 | 保留全部代码/实验，更新对应静态状态；历史E条目按所查提交解释，不以旧“未接线/未运行”概括新实现，亦不宣称本轮独立复验 |
| E63 | 当前application_id组装用std::to_string(frame_photscal)，典型实跑k≈1.5e−17，定点小数串会失去此标量有效位；response_id仅库键，应用摘要含路径但无曲线字节hash | ID非空不等于实际标度/响应完整绑定，记录精度/碰撞风险与多帧身份来源，未改哈希或执行反例 |
| E64 | ROUND9建议参考F_ref=a_k*m(u)*F_syn并称applied，正式规格却还需实际像元施加alpha；代码只细化sed_response缺项原因，积分核复用和完整参考链不相同 | 正式谱积分/拟合/施加责任保持，报告建议待统一，未把缺项原因当新接口授权或恢复旧执行任务 |
| E65 | Cosmetic旧文同页先写计数串行又写omp reduction，称所有IDW O(N)、零计数一律“检测关闭”；当前correct_frame计数为普通for，方向搜索含坏点游程 | 正文说明真实复杂度/计数分工；开启无缺陷与关闭分别记状态，不用零计数猜测 |
| E66 | ac_correct_frame_f64/columns_f64仍double→float→double；内核int宽高乘法和异常/别名边界旧问题须按真实调用核验，输出px_inflated实际计列数 | 模块补充精度、所有权、返回值与记账单位；未改接口/计数或执行错误构型 |
| E67 | Issue6重新核对检测：LM轴比五参重拟合发生在MAD计算后，MAD可能未配最终参数；nls_lm的cost_old=cost−actred与旧代价注释不符，gtol使用步前g；O11对负/零thr指数数学域无全面校验 | 正式文档给准确实现/影响，不将这些保护称普适等价；保留生产代码，交实现方用独立负例核验。接口还无预测索引回传、提前失败不总清出参、vector异常无完整屏障，均已说明 |
| E68 | Issue6天测现源码：select_image_stars只把NaN排后，短候选可选入；总返回N>=每文件cap不证明实际截断；三角a<1e−6不排所有共线、b/a>0.9含近等边；iter_trans_inner的is_ok初为true；trans.sig=q68.3(d²)却作initial_rms_arcsec传精化；重投影仍用valid_inliers而非refit.inliers | 逐项在正式算法/接口说明准确行为；参数/统计、秩与样本需实现方独立负例核验，保留代码；to_c_result的n_detected/n_catalog固定0及best_inliers复制n_pairs已说明，旧SIP奇异仅warn记录按当前明确失败更新 |
| E69 | Issue6光度当前run_with_gaia_impl<T>试乘始终static_cast<float>，T=double仍舍入；records部分分支未填f_instr/gaia_mag，质量reason7可被invalid回填覆盖；谱stride取DB但缓存仍按入参网格；C层width*height用int且无全域/scale有限检查；裸查询缓冲异常清理需核 | 正式detail记录7完整C签名、所有C++装配字段与分支；frame已NaN预置记录、fit_ok已区分NO_DATA、scale正有限已核，不再称这三项缺陷未修；frame丢试乘图，最终apply另行完成，不能把局部float舍入外推全产品；旧Photometer全图扫描和Simpson三段重复端点均已修复。未改生产代码或运行独立反例 |

| E70 | Issue6公共API完整2325行复读发现多处同页矛盾：取消/并发总表与逐族冲突、Noise无锁注册表风险已被mutex修复、旧Drizzle缺自动/异步/capture形参、PSF compact原行映射曾错误要求只取n_valid前缀 | 已恢复Normalize18校准/缺陷、7 PSF、27 Noise和14 Drizzle完整声明/布局及语义，修正有锁/取消/并发/原行映射；Runtime全公开字段/方法及15 session签名入正式正文。Mosaic/Export逐参数与完整模块算法仍待后批，不以完整读过API等同所有接口已对照完；未改生产代码或运行ABI/并发实验 |

| E71 | Coverage build只保存输入/union指针后memset，未保存或校验容量，第二遍重扫K后直写；tile_ipix读取错误跳过；max_leaf_order实际hips_order/tile层级，像元叶阶+9；旧复杂度误用去重K且漏所有frame_tiles驻留 | science/detail已写真实两遍/快照与容量风险、TlogT/O(T+N)、外包络不等真实重叠、六接口/五结构；保留代码不修。旧filter全等政策按现代码删除，no-input的rc1/status0与无完整C异常屏障仍记录；下游order+9有效域、tile错误完整性需实现方独立负例核验 |

| E72 | 局部p2_session原文complete与当前partial矛盾；旧每失败必free不实（guard在coverage fill成功后）；model仅run局部且无RAII；run未validate/无完整异常屏障；rc0空model走失败仍map_rc(0)；重复run不清ran/error/stages；inspect只写span.data/count不写head | 正式detail已保存全部接口/字段/配置和真实分支，修正旧持有模型/生产七节点factory声明。首次取消inspect可created、旧成功后失败可保留partial、persist缺path静默跳过仍存在；host nested回调/ABI、异常泄漏、容量快照及标度依赖停止阈值需实现方独立核验。生产源码未改，未运行；原“ADU·sr⁻¹上绝对阈值必不可达”改为有条件舍入风险，不把历史单次max_iter当普遍结论 |

本轮已合并远端 `5b33e0417`，保留 Normalize Round 3 的空间 PSF、空间 alpha、typed 边与实验记录。已重读新增空间PSF完整实现、相关改动与生产调用，确认E52所列路径并同步架构/实施差异及接口说明；第三轮报告全文已读，其数值本轮未重跑。E01–E15来自合并前抽查，E01的sed_response未接线、E03的basename身份、E04的signal叶阶、E07释放后EXPTIME读取本轮相应代码段仍可见；其余不自动当作仍存或已解决。E05中因子基础单位检查已增加，但通用reader是否全面调用仍须分开核对。原始191篇清单继续固定基线，新增代码/记录另外登记。

## 本轮证据边界

本轮阅读正本、模块登记、公共类型和若干真实调用段，没有编译或执行科学管线，也没有重新获取全部真实输入。Q 条目是后续研究问题；E 条目中明确给出的源码片段是静态事实，其余实现缺口引用现有差异地图，尚需逐项当前运行核实。不得把这里的“记录完成”称为科学重推导或功能修复完成。
