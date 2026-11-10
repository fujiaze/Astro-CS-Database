# ACSD V2｜一手算法与科学参考文献索引（人工维护、逐论文核读）

> 本文件是**起始索引，不是“全部原论文已阅读全文”声明**。2026-11-10先建立可永久访问的原始出版者/档案入口，后续按 [算法文献任务](../workplans/v2-science-engineering/science/02-LITERATURE-AND-PROVENANCE.md) 逐篇写明页码、公式、适用条件、ACSD差异和对应Process。**绝不以自动文档验证代替论文核读**。PDF的公开仓库存档需要明确开放许可/再分发权；否则保留 DOI、可合法获取的原始链接和个人研究笔记，不擅自镜像。

## 首批核对索引

| 主题 | 原始发表作品或一手规范入口 | ACSD应追溯的适用域（待逐式核查） |
|---|---|---|
| FITS WCS | [FITS WCS Publications](https://fits.gsfc.nasa.gov/fits_wcs.html)；Greisen & Calabretta / Calabretta & Greisen 系列标准文献 | 像素原点、天球坐标、TAN、CD/PC、头与WCS |
| SIP | [Shupe et al. SIP Convention, Caltech archive](https://authors.library.caltech.edu/records/fqnyw-qdw79) | 正向/逆向多项式、CRPIX单位及限制 |
| Drizzle | [Fruchter & Hook (2002), DOI:10.1086/338393](https://doi.org/10.1086/338393) | Variable-Pixel线性重构；不承担ACSD真实曲边严格误差证明 |
| HPX/HEALPix投影 | [Calabretta & Roukema (2007), DOI:10.1111/j.1365-2966.2007.12297.x](https://doi.org/10.1111/j.1365-2966.2007.12297.x) | HPX等面积坐标变换、面缝、极点 |
| HEALPix本体 | [Górski et al. (2005), DOI:10.1086/427976](https://doi.org/10.1086/427976) | HEALPix像素化和NESTED规则 |
| Gaia XP外定标 | [Montegriffo et al. (2023), DOI:10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880) | XP谱表示、外定标、跨源共享条件 |
| Gaia合成光度 | [Gaia DR3 synthetic photometry paper, DOI:10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709) | 通带/零点和真实合成光度假设；需核对精确作者、章节和数据版本 |
| Montage | [Montage算法说明](https://irsa.ipac.caltech.edu/Montage/docs/algorithms.html) | 重叠差分sky；ACSD统一gauge/同数据C为独立推导 |
| Proper coadd对照 | [Zackay & Ofek (2017), DOI:10.3847/1538-4357/836/2/188](https://doi.org/10.3847/1538-4357/836/2/188) | **对照资料**，仅在其背景主导、稳定源等假设内讨论最优性；不是本期已实现算法 |
| 数值有向算术 | [MPFR官方手册](https://www.mpfr.org/mpfr-current/mpfr.html) | directed rounding原语定义；各区间域/极值包围仍由ACSD推导 |
| 统计/估计/矩阵 | 由相关Process实际选择的GLS/PSD/QR/SVD/BLUE/QP原始论文或权威标准补录 | 必须按实际算法变体定位原式，不靠无出处的“经典结论”一笔带过 |

## 原文/算法的人工研究卡规则

每条最终卡必须写：(1)规范书目信息+永久索引与PDF合法性；(2)准确章节/页/公式/数学变量；(3)原文成立的物理/统计/数值域；(4)本项目从该公式到当前估计量的**独立逐步推导**；(5)当前Process及源码路径；(6)是否修改了原法与创新来源；(7)独立反例与科研证据；(8)真实数据风险；(9)原文读取者和审查者。若未获得原文，必须标“仅定位，不可视作已审”。

## 项目原创科学依据

详见 [R1–R5研究审计任务](../workplans/v2-science-engineering/science/03-R1-R5-RESEARCH-EXPERIMENTS.md) 与 [历史原始研究包](https://github.com/fujiaze/Astro-CS-Database/tree/adc7f33d58359ac3d65014f239d7f53fe92e4912/%E5%AE%9E%E9%AA%8C/science-foundation)。历史`round2_manifest.json`、成功和失败反例为原始证据。此处不能把R1–R5均宣称为新原创论文；真正新增内容须与原始来源逐式对照再裁决。
