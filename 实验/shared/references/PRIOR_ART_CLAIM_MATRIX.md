# ACSD 五篇论文的前沿工作对照与贡献边界

> 使用方法：每篇 `REPORT_paper.md` 的引言/讨论需依据本矩阵逐条陈述“基础理论已知、实现形式不同、本文可验证贡献、尚不能主张的结论”。本矩阵不是全面的全球新颖性检索报告，只核查下述具名代表性原始材料。各学术 DOI 的书目信息见 [CORE_REFERENCES_VERIFIED.md](CORE_REFERENCES_VERIFIED.md)；网站级材料不应伪装为同行评审论文。

## P1：合成测光与逐帧零点

| 一手材料 | 已有工作 | 与 ACSD 的相同点 | 潜在差异及要做的实证 |
|---|---|---|---|
| Gaia Collaboration / Montegriffo 2023 [R01] | Gaia XP 外定标谱、任意通带合成测光 | 参考谱乘通带响应积分；需要正确光子/能量单位 | ACSD 使用自定义探测器 QE/滤镜组合与无元数据图像链，须用 GaiaXPy 或独立积分比对光谱通量和颜色残差 |
| Xiao et al. 2025 [R02] | 基于 Gaia XP 的 CMOS 观测光度校准及毫米星等量级的精度实证 | Gaia XP 作外部合成光度参考、逐帧标准星标定 | 本项目没有得到同等真实精度证据；比较输入条件、颜色校正、空间场、重复次数和性能，不把“CMOS XP 定标首次提出”写作贡献 |
| Padmanabhan et al. 2008 [R03] | 重叠巡天多帧相对定标与绝对锚分离 | 共同标度的 gauge/零点简并意识 | 若能证明不依赖仪器元数据的统一投影数据接口、误差域和生产可复算，有可能是系统工程而非全新测光估计器贡献 |

**可投稿的题目方向：**“Gaia-XP-anchored photometric standardization with explicit calibration identifiability and real-frame error budget”。**不可发表的当前话术：**“发现新的 Gaia XP 合成测光技术；达到几毫米星等”。必须复核 XP 光谱有效域（光谱模型、波长和颜色系统差）与 QE、filter 的物理量纲。

## P2：参考 SNR 与随机/相关噪声

| 一手材料 | 已有工作 | ACSD 必须新增的可验证内容 |
|---|---|---|
| Horne 1986 [R13] | 经典轮廓加权最优 CCD 光谱抽取，给出真实仪器下高 SNR 提取先例 | 该论文不是 ACSD 图像点源 PSF 的严格一一等价实现，须讨论目标与背景/协方差差别 |
| Newberry 1991 [R04] | 扣天空后 CCD 噪声内部来源与误差传播 | 固定 `F_ref` 的观测目标、背景同时拟合时解析/经验 coverage、增益 ADU/e- 的审计 |
| Zackay & Ofek 2017 [R05] | 背景主导下有明确定义的最优联合图像、相关噪声问题 | 点源信息、扩展源面亮度、重采样协方差与跨帧权重的可辨识条件，不能只称“所有像素 SNR² 最优” |
| Fruchter & Hook 2002 [R06] | Drizzle 造成输出相关噪声的基本机制 | P2+P3 跨单元 `C_out=R C_in Rᵀ` 生产路径与经验覆盖率联合检验 |

**可投稿方向：**“Transferability and calibration of conditional point-source SNR across photometrically normalized spherical image products”。**首要阻断：**真实 117 颗配对星的经验散度报告超过解析预测，需要先分离固定系统偏差与随机噪声。现有 Python 代数等价测试不证明生产传递链已经正确。

## P3：drop-to-HEALPix 面积分配

| 一手材料 | 已有工作 | ACSD 可能有科学贡献的部分 |
|---|---|---|
| Fruchter & Hook 2002 [R06] | 可变像元面积重采样与亮度/通量保持 | drop 与 HEALPix face 图元之间的映射细节、边界退化、实测计算复杂度与跨叶误差 |
| Górski et al. 2005 [R07] | HEALPix 等面积层级球面像元索引 | 不能将 HEALPix 本身当新像素化方案；可证明 chart 交叠求解器与球面弧边 oracle 的误差界 |

数学分配权重之和为 1 是已有 Drizzle 的基本条件，单独验证这一代数不变式不足成论文创新。P3 是否成为方法小论文，取决于能否独立推导/穷举并修复 face 中心、顶角、极区和跨面位置的数值误差（包括源面与目标面边界几何近似）。**必须避免：**全局积分量抵消局部 0.3% 异常后仍宣称“精确守恒”。

## P4：空间插值的统计估计目标

| 一手材料 | 已有工作 | ACSD 的验证责任 |
|---|---|---|
| Wahba 1990 [R08] | 平滑样条、正则/偏差-方差与参数选择 | 稀疏 `V_ref(x)` 在天球的物理量定义、控制点密度失效和最终权重偏差 |
| Fritsch & Carlson 1980 [R14] | 通过单调性条件构造形状保持三次插值 | ACSD 需检验二维重建中的正值/外推/欠约束，而不能将自然样条冒充形状保持 |
| Hyman 1983 [R09] | 单调性/形状保持约束三次插值 | 不可把“自然样条”和“单调三次插值”混成一种天然无过冲的算法；显式测试超越节点范围与边界行为 |
| Zackay & Ofek 2017 [R05] | 统计被估量与最优合并的关系 | `F_ref` 固定参考信噪比和逐像素场景 SNR 是两个对象；给出反例表明混用的权重偏差 |

**可投稿方向：**“Calibrated sparse-to-dense reference-noise reconstruction with support-aware interpolation for spherical mosaics”；评审可能质疑“仅把已有插值搬到另一坐标系”，需要给出跨场型数据、理论风险界/异常构型和生产效率上的实证差异。

## P5：加乘性对齐、公共模与马赛克接缝

| 资料 | 一手已确认事实 | ACSD 的对比轴与不能外推的东西 |
|---|---|---|
| Makovoz, Khan & Moshir 2005 [R15] | 已经提出在重叠图上最小化图像配对背景差并联合解常量偏置，这比仅比较影像处理应用更接近 P5 数学基线 | ACSD 联合连续背景场、不同连通分量 gauge 和扩展源保真需要额外可重复比较，不能以“首次联合背景匹配”主张创新 |
| Padmanabhan 2008 [R03] | 多个重叠观测可构建联合相对标定，但绝对锚与相对自由度不同 | ACSD 每连通分量的 additive/log-gain gauge；缺重叠不可能只凭重叠内部求出绝对零点 |
| John Murphy, [NSG 官方说明](https://www.astroprocessing.com/nsg.html) [R11] | **差分恒星光度**测相对亮度尺度；**surface spline** 拟合相对梯度；先尺度归一再渐变去除；目标匹配选定参考帧的梯度，参考图自身梯度仍留在结果 | ACSD 也应将恒星乘性和天空加性观测分开；不能声称作者官方支持“恢复绝对物理天空共模” |
| John Murphy, [PMM/NSG 软件官网](https://www.astroprocessing.com/) [R11] | 作者提供两款 PixInsight app 和参考文档，并明确列出 PMM 镶嵌流程 | PMM 拼缝思路可作软件对比，但仅官网入口不足以推断具体求解方程、分段融合公式或二维 gain 可辨识证明 |
| [PixInsight 官方 LocalNormalization 实例](https://www.pixinsight.com/examples/M31-Ha/) [R12] | 同一数据集比较过仅加性校正与包含全局尺度适配；大尺度噪声受参考图像影响，波段和参考选择会左右结构可靠性 | ACSD 对公共参考面、扩展星云结构与权重必须做独立保真检验，不能“缝指标变小”就称“真实背景已恢复” |

**P5 与 NSG 的方法差异要试验而不能口头宣布：** NSG 官方描述的是匹配**指定参考帧**的尺度和相对背景，ACSD 最高设计尝试的是**多帧联合公共面+每帧差模**，有不同 gauge 和连通图约束。对比时固定同一帧集、匹配恒星、WCS、PSF、训练/留出星及星云掩膜，报告 Star photometry、Seam normal-gradient profile、Diffuse flux 和 Power spectrum；不能把商业 PMM 的使用结果直接作为 ACSD 实现正确的证明。

## 统一裁决

五点之中，**基础工具/概念均有先例**，但“全部科学问题不新颖”与“ACSD 完全无可发表价值”并非逻辑等价；同样，已有数学工具的组合也不自动构成新论文。每篇最少拿出一个由独立 oracle 支撑、相对现有方法有量化价值的特定贡献，否则论文题目和摘要应以“工程验证研究”而非“原创算法论文”表述。

**书目核验≠全文精读。** 投稿前应由 Agent/审稿者逐页确认引用到具体公式/页码的语义并补原文上下文，尤其 P1 XP 光子响应单位、P2 GLS 噪声、P3 叶边界构造、P4 样条外推、P5 PMM 的实际可公开算法边界。未经确认的材料宁可写“作者官方说明/工程参考”，不可编造论文或公式号。
