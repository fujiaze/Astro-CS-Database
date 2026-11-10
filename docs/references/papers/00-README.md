# 一手文献卡索引（docs/references/papers/）

> 本目录是 [A1 文献任务](../../workplans/v2-science-engineering/science/02-LITERATURE-AND-PROVENANCE.md) 的**人读卡片**，不是脚本生成表，也不是“已审阅全部原文”的声明。
> 建立日期：2026-11-10。核对人：`lit-researcher`（独立文献 Agent，不参与代码实现）。
> 写作规则见 [references/README.md](../README.md)：**没读到具体页/节/式，就不许写页码和式号**；读的是预印本就不能冒充出版版排版。

## 状态标记（工作报告用语，不是程序枚举）

| 标记 | 含义 |
|---|---|
| `PRIMARY_READ_AND_MAPPED` | 已读到原文相应章节/式号，并已把“原文公式 → ACSD 目标量”的推导要点写清 |
| `PRIMARY_READ_PARTIAL` | 读到了部分章节，但卡片所需的某些结论尚未在原文中定位 |
| `PRIMARY_LOCATED_NOT_YET_READ` | 只确认了永久标识/可获取入口，尚未读到正文（付费墙、许可受限、或本轮未读） |
| `DERIVATION_TO_REVIEW` | ACSD 自研推导，需独立复核（本卡片只给要点，不代替正本） |

**版本纪律**：arXiv 接受版与 A&A/MNRAS/ApJ 出版版的**页码不同**，式号一般相同但未逐条对照。本目录凡标“arXiv 版”的条目，卡片中给出的节/式号只对 arXiv 版负责；出版版页码另注。凡未读到出版版，不写出版版式号。

## 卡片清单

| 卡片 | 主题 | 覆盖算法 | 读取状态 | 主要对应 Process |
|---|---|---|---|---|
| [01](01-fits-wcs-paper-i-ii.md) | FITS WCS Paper I/II | 像素原点、线性变换 PC/CD、TAN、LONPOLE/LATPOLE、球面旋转 | `PRIMARY_READ_AND_MAPPED`（arXiv 版；A&A 卷页已由 NASA FITS 页核对） | `coordinates.wcs`、`projection.export`、`sources.astrometry` |
| [02](02-sip-shupe2005.md) | SIP 约定 | A/B、AP/BP、CRPIX 单位、CD 作用顺序 | `PRIMARY_READ_AND_MAPPED`（ASP Conf. Ser. 347 出版版 PDF） | `coordinates.wcs` |
| [03](03-fruchter-hook-drizzle.md) | Fruchter–Hook Drizzle | drop/pixfrac/scale、线性面积分配、相关噪声 R | `PRIMARY_READ_AND_MAPPED`（arXiv v2；PASP 起页已核对） | `geometry.drizzle`、`geometry.overlap` |
| [04](04-hpx-healpix.md) | HPX 投影与 HEALPix 像素化 | 等面积 chart、|sinθ|=2/3 分区、NESTED 位交错、真叶边界 | `PRIMARY_READ_AND_MAPPED`（2004 预印本 + ApJ 2005）；**2007 MNRAS 版未读** | `geometry.overlap`、`coordinates.wcs`、`projection.export` |
| [05](05-gaia-xp-synthetic-photometry.md) | Gaia XP 外定标与合成光度 | 仪器模型积分方程、55 基、通带积分、R_f 判据 | `PRIMARY_READ_AND_MAPPED`（两篇 arXiv 版） | `response.integrate`、`photometry.*`、`sources.measure` |
| [06](06-montage-background-reprojection.md) | Montage 算法页 | 球面多边形交叠、area 权重 coadd、overlap-difference 背景 | `PRIMARY_READ_AND_MAPPED`（IRSA 算法页；工具论文未读） | `sky.controls`、`sky.fit`、`coadd.integrate` |
| [07](07-zackay-ofek-proper-coadd-context.md) | Zackay–Ofek proper coadd（**仅对照**） | 背景主导下 proper image 的最优性 | `PRIMARY_READ_AND_MAPPED`（2015 预印本 v1；ApJ 版式号未核对） | 不对应任何已实现算法；对照 `coadd.weights`/`coadd.integrate` |
| [08](08-numerics-directed-rounding-linalg.md) | 有向舍入与数值线性代数 | MPFR 舍入语义、IEEE 754 映射、QR/SVD 最小二乘与秩 | `PRIMARY_READ_AND_MAPPED`（MPFR 4.2.2 §4.4、LAPACK 例程文档）；**IEEE 754-2019 正文与 GVL/Higham 未读** | `numerics`（`interval`、`linalg`） |
| [09](09-estimation-blue-gls-nnls-qp.md) | 估计与约束优化 | BLUE/GLS、NNLS/QP、KKT、active-set | `PRIMARY_READ_PARTIAL`（Boyd 已读；**Aitken 1935 与 Lawson–Hanson 未读**） | `coadd.weights`、`sky.fit`、`numerics` |
| [10](10-ccd-poisson-read-noise.md) | CCD Poisson/读出噪声 | 散粒+读出的方差模型、gain 换算、plug-in 偏差 | `PRIMARY_LOCATED_NOT_YET_READ`（EMVA 1288 许可受限、Janesick 为书）；推导为 `DERIVATION_TO_REVIEW` | `observation.calibrate` |

## 与既有 ACSD 文档的关系

- 本目录只提供**来源、页/式映射、适用域与红线**；科学正本仍是 `docs/science/foundation/*`，工程合同仍是 `docs/engineering/rebuild/*`。
- 发现的不一致（版本归属、链接指向的论文与所标年份不符、把预印本式号当出版版等）逐条写在各卡“与现有文档的差异”节，并已在 2026-11-10 汇报给 lead；**本 Agent 未修改任何科学正本或源码**。
- 本目录不镜像任何 PDF：`docs/references/papers/` 只存 Markdown 卡片。SIP 一例的出版版 PDF 可从 CaltechAUTHORS 公开下载，卡片只给链接。
