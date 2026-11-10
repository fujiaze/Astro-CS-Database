# A1｜全算法文献来源与原始论文索引/合法存档

## 原则

**每个生产科学算法、关键数值原语、标准格式和原创扩展必须有来源与技术谱系**。不是每个 `for` 循环都需论文：对数据结构/调度策略可援引设计依据、成熟软件实现、标准与推理；对数学/物理算法必须引用能核对的原始或最早可获得的一手文献。综述只能作为发现来源，不能替代已经能检索到的原论文。引用一篇论文不意味着 ACSD 的模型假设与该论文相同；必须明确哪里直接采用、哪里变体、哪里是独立推导或验证。

## 逐算法文献卡的必需内容（直接写入每篇 Process 的“算法来源与推导”节）

- 论文/标准规范化题名、作者、年份、期刊/会议、DOI或永久标识、稳定原文URL、合法可访问PDF URL、许可/再分发情况、获取日期、如已归档则 PDF SHA256；不得使用临时带token的下载URL作永久出处。
- **准确页、节、公式号、算法号**，先说明作者处理的观测量/假设、变量单位、约束与正确性结论；再推导 ACSD 中本操作所采用的公式，显示转换过程及为什么使用合法。对于“参考的一般方法”，要明确并非逐式复制。
- 代码拥有者、函数路径/具体数学分支、输出科学量和误差/停止条件；不要求每个调用点写同一论文表，公共原语可链接到唯一 NUMERICS 来源段。
- 对原创/组合/改进：最接近先前技术、与论文不同之处、可辨识性或误差新结论、创新主张是否实际独立验证、尚不能声称优于前人的场景；实验入口和失败样本。
- 文献状态标记 `PRIMARY_READ_AND_MAPPED / PRIMARY_LOCATED_NOT_YET_READ / DERIVATION_TO_REVIEW / ORIGINAL_EXPERIMENT_PENDING`；属于工作报告用语，不是软件新枚举。
- 对原始文献没有给出 ACSD 所需性质的地方，以**本项目完整推导**补全，不能写“应该正确”或“参考某算法”。

## 已核查的一手索引种子（需逐篇读原文核到具体页/式）

| ID | 原始来源/永久地址 | 在 ACSD 的地位 | 待细化 |
|---|---|---|---|
| SRC-WCS | [NASA FITS WCS 原始规范文献索引](https://science.data.nasa.gov/fits/documentation/wcs)；Greisen & Calabretta / Calabretta & Greisen (2002) | Pixel/FITS 坐标与天球投影标准；不自动证明自写逆求解器 | Paper I/II 逐式页码、轴和极区 |
| SRC-SIP | Shupe et al. (2005), [The SIP Convention for Representing Distortion in FITS Image Headers](https://authors.library.caltech.edu/records/fqnyw-qdw79) | SIP 多项式和 FITS 约定 | 原文 A/B、AP/BP 与完整 WCS |
| SRC-DRIZZLE | Fruchter & Hook (2002), [DOI 10.1086/338393](https://doi.org/10.1086/338393) | Variable-Pixel/Drizzle 线性分配思想；**不是**ACSD 曲边真叶严格包含证明 | 式号、原法假设、变体推导 |
| SRC-HPX | Calabretta & Roukema (2007), [DOI 10.1111/j.1365-2966.2007.12297.x](https://doi.org/10.1111/j.1365-2966.2007.12297.x) | HPX 等面积图及坐标变换 | 对照 12 face chart 和 apex |
| SRC-HEALPIX | Górski et al. (2005), [DOI 10.1086/427976](https://doi.org/10.1086/427976) | HEALPix 分层/索引科学依据 | NESTED 编码、邻域/精确叶 |
| SRC-GAIA-XP | Montegriffo et al. (2023), [DOI 10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)；Gaia Collaboration (2023), [DOI 10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709) | XP 外定标、合成光度及其适用性；不代表 3nm 谱线已知 | XP 光谱/响应、线宽限制、协方差 |
| SRC-MONTAGE | [Montage 原始算法说明](https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html)；[Montage 工具论文入口](https://arxiv.org/abs/1005.4454) | 重叠差背景思想对照；ACSD 全局 GLS/gauge 为自身推导 | 区分原法背景假设及改动 |
| SRC-COADD-CONTEXT | Zackay & Ofek (2017), [DOI 10.3847/1538-4357/836/2/188](https://doi.org/10.3847/1538-4357/836/2/188) | 点源/背景主导 optimal coadd 的**对照文献**；不可偷换为 ACSD 传统面亮度 BLUE 的原论文或已实现 SACoadd | 同/异目标最优性的条件 |

这些仅是**可核实的首批来源**，不代表所有算法已经完成文献审计。其余如 Poisson/CCD 噪声、GLS/Gauss-Markov、BLUE、PSD/KKT、QR/SVD、区间/MPFR、分段求积/光学 PSF、稀疏统计插值、线性算子、存储事务与异步调度，应分别确定真正使用的算法版本与有权威的一手原文/标准，不能以不准确 DOI 填空。

## 论文存档与权限

在 `docs/references/` 建**一个**人读的按主题索引（不是脚本生成表）；论文链接应直达 DOI/作者档案/机构仓库及可读 PDF，若原文是 OA 或许可明确允许仓库再分发，可按论文标识存入 `references/papers/` 并保许可说明、原 URL 和 SHA256；**若权限不明、订阅受限或仅供个人下载，不把 PDF 提交公开仓库，只保存永久地址/ADS/arXiv/机构索引和本项目原创的公式定位笔记**。禁止用付费墙绕过或篡改版权说明。已入库 PDF 不能替代网页原始来源标识。

## 逐模块/原语覆盖安排

- `observation.calibrate/regroup`：CCD/CMOS 校准、Poisson/read、抽样算子与线性误差传播；
- `coordinates.wcs/catalogue.access/sources.astrometry`：FITS WCS/SIP、Gaia 星表/历元、天体测量/模式匹配/真实候选判别；
- `sources.psf/sources.measure`：像元积分的 Gaussian/Moffat/Airy、PRF/尾界、GLS/PSD/参数估计/可辨识性；
- `response.integrate/photometry.fit/photometry.apply`：Gaia XP、光谱响应积分、联合潜变量标度、空间应用；
- `geometry.overlap/geometry.drizzle`：Fruchter–Hook 与 HPX/HEALPix、一手微内核证据、包含算法；
- `operators.action/uncertainty.covariance/information.reference/information.scene`：线性算子/随机根/Fisher/实际场景/稀疏支持/可认证插值；
- `sky.controls/sky.fit`：Montage 与独立的连通图、gauge、GLS/profile、同数据相关；
- `coadd.select/weights/integrate`：小 N 选择的统计前提、BLUE/PSD/nonnegative QP 和不同响应解释；
- `projection.export`：FITS WCS、面积/支持积分及目标场重采样；
- `src/numerics`：按实际原语分类追溯，不把“IEEE 754”说成已证明数学超越函数包含。

## 审核交付

独立科学审查者每次随机抽取至少一项核心外来算法和一项原创扩展：**脱离代码，从原文推到本 Process 目标公式**，写清相同和不同之处；未读原文的算法不能取得 `ALGORITHM_SPECIFIED` 状态。抽样只是核对手段，不免除对所有算法的逐项文献卡。用户可直接打开 Process→引用→论文→页/公式→本项目变体→证据。
