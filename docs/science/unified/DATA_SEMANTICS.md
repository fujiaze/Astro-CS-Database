# ACSD 数据语义｜三阶段产品、单位、坐标与无效值

> 科学正本；上游 docs/ACSD_DESIGN.md §1–§6；数学模型见 docs/science/unified/UNIFIED_SCIENCE_MODEL.md。重写日期 2026-10-14。旧版关于“全部产品都写 ADU/sr”“阶段三一定传播最终逆方差”“mosaic 必须输出 support/variance/ivar”的表述不再代表目标设计。

## 1. 一条科学量传递链

三种文件/数据层级必须区分：

| 阶段 | 图像产品 | 用于计算的其它对象 | 目的 |
|---|---|---|---|
| normalize | 逐帧 signal HiPS；可用必要辅助子产品 | Gaia XP 光度/通带响应、F_ref,k、frame_snr、稀疏 SNR 控制点、PSF、源/球面噪声与支持域 | 供阶段二完整科学计算 |
| mosaic | **一套单通道 signal HiPS** | 只在计算过程/运行摘要保留的稠密 SNR/方差、背景拟合参数、有效集合、权重、异常排除统计 | 输出可供后续处理的球面图像 |
| export | **一幅普通二维信号 FITS** | 目标 WCS、导出采样核、必要元数据 | 通用后期处理 |

HiPS 的 Norder/Dir/Npix 目录、NESTED index、HiPS 层级 FITS tile、MOC（如采用）、properties、BUNIT 和运行 manifest 是**坐标/文件/来源结构**，不是增加 signal 之外的科学图像通道。

## 2. 坐标的唯一约定

- 平面数组图像 i 对应 FITS 第一轴列（从 0 开始），j 对应第二轴行；FITS 头部像元号习惯从 1 开始，因此整数中心坐标加 1。
- 外部 RA/Dec 使用 ICRS 度；球面计算用弧度和单位方向向量。RA 范围 [0°,360°)，禁止在回绕处直接对经度作直线插值；极区的经度无单独意义。
- HEALPix 使用 NESTED，nside=2^K，ipix 的绝对编号与 tile 内存像元序分开处理。
- 当前 ACSD 确定的 local NESTED→FITS row-major 映射：local=(x,y) Morton 位交错，x 在偶位、y 在奇位，tile 512 时 row=511−x、col=y、offset=row×512+col。此布局来自项目参考 tile 的独立对拍/约定，不应误写成 IVOA HiPS 标准自身指定的唯一矩阵行列公式。
- 该映射由 lib/algorithms/shared/healpix/healpix_core.cpp 中共用函数对外提供，所有阶段读写应使用同一实现，而独立验证采用外部 WCS/HiPS producer。

球面 leaf 立体角 Ω_leaf=4π/(12nside²)。HEALPix 叶边界是分段曲线，不能以叶的等面积性推出任意“四角大圆弧面积”恰是其真实面积。

## 3. 区分观测计数、线性通量和面亮度

| 符号 | 科学量 | 单位约定 |
|---|---|---|
| x_j | 源像元实际在线性标度下记录的积分量 | 原始 ADU、e− 或指定模型通带积分量 |
| Ω_j, A_drop,j, a_jp | 未收缩源面积、drop 球面面积、球面交叠面积 | sr |
| B_j=x_j/Ω_j | 源像元平均面亮度 | [x]/sr |
| S_p | 球面信号面亮度 | [x]/sr（以当前校准后的 [x] 定义） |
| V_B(p) | S_p 对同一个球面面亮度被估计量的方差 | ([x]/sr)² |
| C_B(p,q) | 对两个像元信号的协方差 | ([x]/sr)² |
| F_ref,k, σ_F,k | 第 k 帧规定参考光度尺度下点源总通量及其条件标准差 | [F] |
| SNR_ref,k | F_ref,k/σ_F,k | 1 |
| V_F,k | σ_F,k² | [F]² |
| T_k(p) | V_B,k(p)/V_F,k | ([x]/sr)²/[F]²；不能无条件设为 1 |
| w_B,k(p) | 1/V_B,k(p) | 1/([x]/sr)² |
| V_ctrl,k | 背景控制估计量的方差 | ([x]/sr)² |
| support/internal coverage | 采样支持的几何或有效集合描述 | 分数或具名几何量；不是最终另一个图像子产品 |

“ADU/sr”仅在标定后图像数值仍属于 ADU 积分量时准确；若已通过 Gaia XP 模型尺度把计数转换为其他单位，不能保留虚假的 ADU/sr 标签。颜色/目标通带未精确定义时，使用明确的相对模型单位并给出解释限制，不伪称 SI 绝对光谱辐照度。FITS BUNIT 的写法遵守 FITS 大小写和单位语法，不能假定 adu 与 ADU 在正式标准中自动等价。

星等只是派生对数值，不可对负的天空扣除信号直接取 log 而静默生成有限结果。

## 4. 三种逆方差严格分名

1. reference flux information：I_F=1/V_F=SNR_ref²/F_ref²，单位 1/[F]²。
2. surface-brightness precision：w_B=1/V_B=I_F/T，单位 sr²/[x]²。
3. background control precision：w_ctrl=1/V_ctrl，依对应背景估计量定义。

只有被估计量、通带、标度和传递 T 均保持一致时才能在同一个权重聚合算子里直接比较。几何 drop 分配权 w_geom=a/A_drop 是无量纲，不属于前述逆方差。所有代码/日志/API 中可缩写的“weight”应结合来源、单位和目标量解释，避免第二种同名口径被误当成第一种。

## 5. 无效像元与支持域不是单一布尔概念

至少在计算与日志内部区分：
- 输入没有覆盖：no_data；
- 有覆盖但值或模型无效：invalid_signal / missing_precision；
- 有采样但精度点在重建支持域外：outside_support；
- 数据存在但被排异拒绝：rejected；
- 数据可用且输出信号为 0：valid_zero；
- 存在数学上可定义但未得到外部物理绝对锚的相对背景分量：relative_only。

阶段二最终只有浮点 signal FITS/HiPS 通道，像元可用性通过有限值 vs NaN 区分；负信号合法，有限零表示真实数值 0。详细原因保留在运行诊断/汇总而非另加第二幅有效性图。HiPS tiles 无覆盖时可以不存在或具有 NaN，但读侧必须与目录/有效区域定义一致。

阶段一中间的 variance/ivar 可各自采用合法有限/NaN/0 的状态语义；不允许把“该帧无科学精度估计”默认为“真实零方差”，也不允许仅对矩阵加近零对角正则就把“信息无限”当作观测事实。

## 6. 光度参考、跨设备响应与 systematics

每个 frame 的参考通量 F_ref,k 必须与 frame_snr 采用完全配对的光度单位、参考星等/SED 和响应模型。即使所有帧名义上为 R，滤镜曲线、QE、光学系统、大气可能不同。用户允许任意合并：接口需要记录当前参考响应或混合规则，不以设备同名作为输入过滤。

若使用星表颜色项校正恒星，输出信号作为扩展源的物理意义受未知逐位置 SED 限制。颜色残差、PSF 与天空低频差是不同类别，不可都塞入随机方差或背景加性场。跨帧共同系统误差不按独立帧数自然衰减。

光度变换参数不应在 normalize 与 mosaic 各自被当作新绝对未知乘性场无依据地二次估计。

## 7. Drizzle/HiPS 写盘与层级

- Drizzle 源积分像元 x_j 用 w_geom=a/A_drop 分到 F_p=Σw_geom x_j；面亮度分母 N_p=Σw_geom Ω_j；signal=S_p=F_p/N_p。
- 与上式同一线性算子的随机协方差为 C_out=R C_in Rᵀ；输出相关性不能凭更小的单像元 RMS 被忽略。
- 支持量 D_p=Σa_jp 是可重复累计的 drop 面积，不能未经论证等同覆盖并集；支持 fraction=min(1,D_p/Ω_leaf) 也不能用来恢复超覆盖次数或严格通量。
- 最终 HiPS 允许 Norder 多层图像分辨率；“最终单层 signal”是**单科学通道**而非所有 HiPS 只存一个角分辨率。父阶由孩子的适当面积/有效覆盖重建，不当作新增独立观测。

## 8. 稀疏 SNR 控制点的生产对象

Normalize 每帧唯一正式精度载体为 frame_science.json，包含 Drizzle 后的球面 control records、逐点 F_ref/V_F/SNR、response/reference、局部 PSF/协方差、支持域与缺项状态，完整合同见 engineering/contracts/NORMALIZE_DATA.md。Catalogue HiPS 只可作为浏览投影；像素网格 sparse_snr_layer 是兼容视图，不能因 schema 可加载就视为球面数据完备。

控制点的 C_ref 依 reference-source 条件计算；actual-scene 面亮度方差另行求值。T 有单位和独立 availability；没有 T 不能假设为 1。无精度的合法信号仍可发布，但下游必须报告精度能力。

## 9. 阶段三平面导出合同

阶段三从阶段二**唯一信号通道**做 WCS 重采样，不要求 variance/ivar/snr 帧数据层。WCS 头与 HiPS 来源、滤镜/光度系统解释一致；重采样在高分辨图像上可能改变相关噪声和有效 PSF，但最终单信号图不能凭空恢复它们。对科学评估从独立模拟/保留的内部算子取得真值，不把后期目标格式升级成数据不存在的置信度声明。

## 10. 研究与实现对照

现有详证见：实验/photometric-magnitude、实验/absolute-snr、实验/healpix-polar、实验/dense-snr-reconstruct、实验/additive-sky-seamless；实际生产缺口与原型代码见 docs/engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md。涉及 BUNIT、HiPS 文件顺序、观测时标和角坐标时按 IVOA HiPS / FITS / HEALPix 一手标准做独立验证，不将“生产 writer 与 reader 一致”当作外部兼容性的充分证据。