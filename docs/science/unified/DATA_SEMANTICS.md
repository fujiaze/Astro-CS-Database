# ACSD 数据语义｜三阶段产品、单位、坐标与无效值

本分册从[最高设计](../../ACSD_DESIGN.md)与[统一科学模型](UNIFIED_SCIENCE_MODEL.md)定义三阶段产品、单位、坐标和无效值。字段与持久化结构由[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)承载。

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
- ACSD 的 local NESTED→FITS row-major 映射约定：local=(x,y) Morton 位交错，x 在偶位、y 在奇位，tile 512 时 row=511−x、col=y、offset=row×512+col。该布局须与独立HiPS生产者核对；NESTED位交错本身不能单独确定FITS矩阵方向。
- 所有阶段读写使用同一个公共转换接口，独立验证采用外部WCS/HiPS生产者；实现位置见[科学实现差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

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
| T_k(p) | 同位置、同条件的V_B,k(p)/V_F,k | ([x]/sr)²/[F]²；不能无条件设为 1 |
| w_B,k(p) | 1/V_B,k(p) | 1/([x]/sr)² |
| V_ctrl,k | 背景控制估计量的方差 | ([x]/sr)² |
| support/internal coverage | 采样支持的几何或有效集合描述 | 分数或具名几何量；不是最终另一个图像子产品 |

“ADU/sr”仅在标定后图像数值仍属于 ADU 积分量时准确；若已通过 Gaia XP 模型尺度把计数转换为其他单位，不能保留虚假的 ADU/sr 标签。颜色/目标通带未精确定义时，使用明确的相对模型单位并给出解释限制，不伪称 SI 绝对光谱辐照度。FITS BUNIT 的写法遵守 FITS 大小写和单位语法，不能假定 adu 与 ADU 在正式标准中自动等价。

**线性基单位词表（#9 冻结）**：面亮度链的线性基单位由**实际施加状态**给出，只有两个成员——

| 线性基 token | 含义 | 何时使用 |
|---|---|---|
| `ADU` | 原生校准计数积分量 | 未施加带单位标度的帧（`native_calibrated`） |
| `XPMODEL` | 已施加 Gaia XP 合成通带标度的**模型通带通量**单位：由该帧实际装载的 response（滤镜曲线 + QE + XP 绝对谱归一约定）定义，是相对模型单位，**显式非 SI** | 逐帧实际施加了 `k_photo` 的帧（`relative_scaled`） |

面亮度 canonical 串由线性基单位唯一导出：signal `L/sr`、其方差 `L^2/sr^2`、其逆方差 `sr^2/L^2`（`L` = `ADU` 或 `XPMODEL`）。该词表与导出关系由冻结单位表唯一实现（`lib/algorithms/resample/p3_rsmp_units.cpp`）给出，其它模块不得自建第二套解析/书写词表。`XPMODEL` 的归一约定随 `frame_science.json` 的 `photometry.response_id`/`application_id` 与 `reference.response_id` 落盘；`XPMODEL` 数值只在同一 response 身份内可比。

**产品单位身份的唯一性（导出边界）**：任何消费者（当前为阶段三 Export）都必须从输入 signal 声明的 `BUNIT`（辅以 `ACSD_SIGNAL_UNIT`、`ACSD_PIXEL_SEMANTICS`、`ACSD_PIXEL_AREA_POWER`）**唯一确定**其面亮度单位身份，并经冻结单位表归类校验；对同一逻辑键的重复声明、大小写别名、两个声明面互相冲突、缺失、空串、词汇表外、跨基单位混合或与像元语义矛盾的声明，一律显式失败，**不得**补写任何确定量纲（尤其不得在无证据时写 `ADU/sr`），也不得把失败读取转成正常无覆盖/普通 NaN。通过校验后，产品面写冻结单位表对该线性基单位导出的 canonical 面亮度串（`ADU/sr` 或 `XPMODEL/sr`）；输入原始声明随产物保留供审计。标度（PHOTSCAL/PHOTAPPL 等）与量纲类别的关系分两支：**无量纲**的相对标度只改零点、不改单位串，但必须登记 scale_state/normalization/reference ID；**带单位**的标度（如 `k_photo` 的 `U_out/ADU`）必须让 signal、方差、ivar、F_ref 与模型因子的单位同步切换到 `U_out`，不得继续标 `ADU/sr`。未闭合的标度/响应身份按 #9/#79 处理，不在导出边界自行发明。

星等只是派生对数值，不可对负的天空扣除信号直接取 log 而静默生成有限结果。

## 4. 三种逆方差严格分名

1. reference flux information：I_F=1/V_F=SNR_ref²/F_ref²，单位 1/[F]²。
2. surface-brightness precision：w_B=1/V_B=I_F/T，单位 sr²/[x]²。
3. background control precision：w_ctrl=1/V_ctrl，依对应背景估计量定义。

只有被估计量、标度、协方差条件和传递T匹配时才能比较随机精度；通带不同的用户组合还须保留空间混合响应的解释。几何 drop 分配权 w_geom=a/A_drop 是无量纲，不属于前述逆方差。所有代码/日志/API 中可缩写的“weight”应结合来源、单位和目标量解释，避免第二种同名口径被误当成第一种。

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

Normalize 每帧唯一正式精度载体为 frame_science.json，包含 Drizzle 后的球面 control records、逐点 F_ref/V_F/SNR、response/reference、局部 PSF/协方差、支持域与缺项状态，完整合同见[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。Catalogue HiPS 只可作为浏览投影；像素网格 sparse_snr_layer 是兼容视图，不能因 schema 可加载就视为球面数据完备。

控制点的 C_ref 依 reference-source 条件计算；actual-scene 面亮度方差另行求值。T 有单位和独立 availability；没有 T 不能假设为 1。无精度的合法信号仍可发布，但下游必须报告精度能力。

## 9. 阶段三平面导出合同

阶段三从兼容HiPS的信号通道做WCS重采样，典型输入为阶段二的单signal产品；不要求 variance/ivar/snr 帧数据层。WCS 头与 HiPS 来源、滤镜/光度系统解释一致；重采样在高分辨图像上可能改变相关噪声和有效 PSF，但最终单信号图不能凭空恢复它们。对科学评估从独立模拟/保留的内部算子取得真值，不把后期目标格式升级成数据不存在的置信度声明。

## 10. 研究与实现对照

实现状态见[科学实现差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。外部兼容性依据FITS[1]与HiPS[2]，用独立读者检验坐标、单位和文件方向；本项目writer与reader互相一致不足以证明兼容性。

[1] FITS Working Group. [FITS标准](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)。

[2] IVOA. [HiPS推荐标准](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)。


## 附：#8/#9/#13 的同估计量科学对接（2026-10-17，公式裁决；真实 E2E NOT_RUN）

**#8 点源 Fisher 与面亮度 variance 的区别。** 在冻结参考点源的局部观测模型 `d=Fh+Zβ+ε`、`Cov(ε)=C_ref` 下，若 `C_ref` SPD 且 `Z` 满列秩，则对 nuisance profile 的有效信息量为

\[
I_F=h^TC_{ref}^{-1}h-h^TC_{ref}^{-1}Z(Z^TC_{ref}^{-1}Z)^{-1}Z^TC_{ref}^{-1}h,\qquad V_F=I_F^{-1}.
\]

病态或秩亏时须在可估计子空间求解，不能逐项使用未验证的逆；`F_ref/√V_F` 是点源 reference SNR。Drizzle/Mosaic 的实际被估计量为球面面亮度 `B`（单位 `L/sr`）。对给定线性重建/取样算子 `a`，`\widehat B=a^Td`，`V_B=a^TC_{actual}a`（需包括同一背景/相关项及有效域），最优权重 `w_B=1/V_B`，不是 `I_F`。仅在相同帧/天区/噪声条件、同一 reference 定义、且同时求得 `T=V_B/V_F`（单位 `sr^{-2}`）时，`w_B=I_F/T`；转换的条件必须保留，`T_ref` 不自动是 actual-scene 的 `T`。

严格反例：两幅对 B 的无偏估计 `V_{B,A}=V_{B,B}=1`，故 oracle 最优权为 (1,1)、合成方差 1/2。点源模板分别 `h_A=(1,0)`、`h_B=(1/2,1/2)`，独立像素噪声均 1，则 `I_{F,A}=1,I_{F,B}=1/2`。直接以点源信息加权，实际 B 组合方差为 `(1+1/4)/(3/2)^2=5/9`，比 oracle 高 11.1%。这是**证明错误一般化**的解析反例，不代表已在真实星场测得 11.1%。应以独立向量/矩阵 oracle 和实际同一 Mosaic 被估计量、点源/背景 Poisson、correlation、rejection 后有效集合复算。

**#9 一次尺度与 BUNIT。** 真实逐帧应用 `s'_j=α_j s_j` 时，`Var(s'_j)=α_j^2 Var(s_j)`，协方差 `C'=A C A^T`，其中 `A=diag(α)`。若 `α` 是带单位 `U_{out}/U_{in}` 的校准转换，信号单位必须同步从 `U_in` 改为 `U_out`，球面 BUNIT 为 `U_out/sr`、方差 `U_out^2/sr^2`；若 `α` 无量纲，只是相对标度，数值虽变而仍处原物理单位族，但必须声明 scale_state/normalization/reference ID，不得声称绝对 Gaia 物理标定。两种模型不可仅因“数学上都是乘数”便合并为永远 ADU/sr。检测真实 `photoapplied`、`frame_photscal`、响应物理换算、写出 HiPS FITS BUNIT、sidecar 与 Export 的单位词汇同一性；参考 source_flux 口径与 applied signal 要一次匹配，禁止双重施加。

**#13 球面真源消费的必要且非充分条件。** Stage1 `frame_science/v2` 的 controls 保存 sphere sky/leaf、F_ref/V_F/conditional status/support/model refs；Stage2 reader 实际完成 hash、单位、R/h/C/Z、HEALPix 验证并通过独立空间重建算子插值到所需目标叶，才可称“消费 P1 canonical”。当前 Stage2 读取 `ACSD_SPARSE_SNR_LAYER` 是旧像素层，不是球面 GLS。即使 reader 接通，`I_F` 也只能描述点源，若不能按 #8 得到 actual-scene 的 `V_B`，不得用于生产 `inverse_variance` 面亮度权重；已有同一被估计量的 corrected variance/ivar 可独立保留。验证跨 12 face、极区、空洞、orders 9/11/13、支持域越界与加权 covariance 传播，分别报告读取成功与实际参与权重的科学条件。
