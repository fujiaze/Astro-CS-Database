# 五篇科学论文的核验参考文献与适用边界

本文件为五篇 `REPORT_paper.md` 的共享**文献核验登记**。仅经学术出版社/机构/作者官方网站查证书目信息的条目记为 **BIB_VERIFIED**；该状态只表示作者、标题、期刊/年份、DOI 可以核验，**不表示 ACSD 的具体公式、常数、实现或实验阈值已由该文献证明**。对“本文已完整精读推导”不给未经逐页核验的保证。规范化 BibTeX 见同目录 `CORE_REFERENCES_VERIFIED.bib`。

| 编号 | 文献及永久入口 | 与 ACSD 的直接关系 | 不能引用它证明的事项 | 核验状态 |
|---|---|---|---|---|
| R01 | Gaia Collaboration / Montegriffo et al. 2023, *Gaia DR3: The Galaxy in your preferred colours: Synthetic photometry from Gaia low-resolution spectra*, A&A 674 A33, [DOI](https://doi.org/10.1051/0004-6361/202243709) | P1 合成测光、Gaia XP 波长与绝对通量刻度的一手基础 | ACSD 自研积分器无差错、任意相机通带皆有效 | BIB_VERIFIED |
| R02 | Xiao et al. 2025, *Calibration of Complementary Metal-oxide-semiconductor Sensor-based Photometry to a Few-millimagnitude Precision: The Case of the Mini-SiTian Array*, ApJL 982 L27, [DOI](https://doi.org/10.3847/2041-8213/adbd3c) | P1 的 Gaia XP CMOS 校准直接先例；需做算法要素对照 | ACSD 已达到毫米星等，或两者完整实现同构 | BIB_VERIFIED |
| R03 | Padmanabhan et al. 2008, *An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data*, ApJ 674, 1217–1233, [DOI](https://doi.org/10.1086/524677) | P1/P5 重叠观测的相对与绝对零点、全局光度简并 | ACSD P5 公共加性天光物理信号唯一可恢复 | BIB_VERIFIED |
| R04 | Newberry 1991, *Signal-to-noise considerations for sky-subtracted CCD data*, PASP 103, 122–130, [DOI](https://doi.org/10.1086/132801) | P2 天光扣除后的噪声传播和独立随机源归因 | 重采样相关噪声可忽略、真实帧绝对覆盖率准确 | BIB_VERIFIED |
| R05 | Zackay & Ofek 2017, *How to COADD Images. II. A Coaddition Image that is Optimal for Any Purpose in the Background-dominated Noise Limit*, ApJ 836, 188, [DOI](https://doi.org/10.3847/1538-4357/836/2/188) | P2/P4 叠加权重、噪声相关性与目标估计量的关系 | ACSD 每像素场和点源 PSF 通量使用同一标量权重即最优 | BIB_VERIFIED |
| R06 | Fruchter & Hook 2002, *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*, PASP 114, 144–152, [DOI](https://doi.org/10.1086/338393) | P3 drop、交叠面积、通量保持与相关噪声 | ACSD HEALPix 极区候选与面边界误差恒为 0 | BIB_VERIFIED |
| R07 | Górski et al. 2005, *HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere*, ApJ 622, 759–771, [DOI](https://doi.org/10.1086/427976) | P3 球面等面积层级像素化背景 | ACSD chart 内所有几何裁剪实现严格正确 | BIB_VERIFIED |
| R08 | Wahba 1990, *Spline Models for Observational Data*, SIAM CBMS 59, [DOI](https://doi.org/10.1137/1.9781611970128) | P4 样条平滑、正则化、偏差-方差权衡 | 自然三次样条天然无过冲或无需外推门 | BIB_VERIFIED |
| R09 | Hyman 1983, *Accurate Monotonicity Preserving Cubic Interpolation*, SIAM J. Sci. Stat. Comput. 4, 645–654, [DOI](https://doi.org/10.1137/0904045) | P4 形状保持插值是一套有专门约束的算法，不可与自然样条混同 | Hyman 的形状保持结果天然适用于任意二维自然样条 | BIB_VERIFIED |
| R10 | Huber 1964, *Robust Estimation of a Location Parameter*, Annals of Mathematical Statistics 35, 73–101, [DOI](https://doi.org/10.1214/aoms/1177703732) | P1/P5 稳健位置拟合的理论先例 | 具体代码收敛、`delta=1.345` 对所有误差分布均 95% 有效 | BIB_VERIFIED |
| R12 | PixInsight Team, *Messier 31 H-alpha Processing Notes: LocalNormalization*, [官方案例](https://www.pixinsight.com/examples/M31-Ha/) | P5 的局部背景与全局尺度适配消融、参考图像影响 | ACSD/PMM 的源代码数学细节与真实天空公共模可辨识性 | OFFICIAL_DOC_VERIFIED |\n| R11 | John Murphy, *AstroProcessing: NormalizeScaleGradient and PhotometricMosaic*, [作者官网](https://www.astroprocessing.com/) | P5 天文马赛克增益/背景/渐变处理的可对照软件工作流 | 源码求解方程、二维增益可辨识性、PMM 与 ACSD 数学同构 | AUTHOR_SITE_VERIFIED |

专题对照与新颖性限定见 [PRIOR_ART_CLAIM_MATRIX.md](PRIOR_ART_CLAIM_MATRIX.md)，其中 NSG 的一手算法行为采用作者官方 [NSG 说明](https://www.astroprocessing.com/nsg.html)：差分恒星测光求 scale，surface spline 建梯度模型，先乘性标度、再相对背景校正；参考图的背景梯度仍残留。此类软件说明不是数学期刊论文。\n\n## 用于五篇论文的引文归属

- P1 直接引 R01/R02/R03，统计论证引 R10；须把“小论文贡献”限定为 ACSD 特定单位语义、稳健错误预算和真实帧适用域，不能泛称发明 Gaia XP 合成测光。
- P2 引 R04/R05；必须解释 `sigma_F` 的统计被估量、自由背景、协方差、条件假设与真实覆盖率分立。
- P3 引 R06/R07；将“滴落代数权重和为 1”与“实现下跨面/跨叶的误差”明确分开。
- P4 引 R08/R09/R05；对样条过冲、域外预测和定权转换逐条给可复现对照。
- P5 引 R03/R10/R11；加性/乘性可辨识、重叠图连通分量、低频结构保真和真实缝检测必须独立做反例。

## 核验层次与引文质量门

- `BIB_VERIFIED`：出版信息可追溯；不等于已精读全文。
- `MATH_CHECKED`：另需带页/式号与独立推导或可复算脚本，须明确逐页核对。
- `IMPLEMENTATION_CHECKED`：仓库生产源码、SHA、调用链、可执行负例可追溯。
- `EXPERIMENT_CHECKED`：输入索引/哈希、运行命令、环境、随机种子、代码 SHA、原始观测摘要与置信区间可复现。
- `RELEASE_VERIFIED`：构建、CI、完整测试和真实帧判据收敛，不得由其他等级推断。

链接只能证明该文献确实存在；对具体公式和常数不得“靠 DOI 兜底”。新搜集的文献先比对原仓 `reverse_verify_bibliography.bib`，不要删除历史实验的合法引文编号。
