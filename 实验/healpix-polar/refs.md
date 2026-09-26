# refs.md — 实验单元 healpix-polar 文献核验台账

**收口口径**：只收一手 VERIFIED 条目（三路独立审计与 k_corr 补实验中至少一路全文实取核验，
关键锚句经两路以上独立一致或父代理直验，见 分歧台账 D-03）。核验记录原文见
`独立审计/实验重做/P3守恒映射算子/{路线1,路线2,路线3,补实验-k_corr}/refs.md`（只读参照）。
格式：DOI/arXiv + 核验方式 + 用途。UNRESOLVED 项不进正文引用，只在文末登记。

---

## VERIFIED（可进论文引用）

### V1. Górski et al. 2005（HEALPix 等面积离散化）

- **标识**：ApJ 622, 759；DOI [10.1086/427976](https://doi.org/10.1086/427976)；arXiv:[astro-ph/0409513](https://arxiv.org/abs/astro-ph/0409513)。
- **核验方式**：arXiv 全文实取（路线1/2/3 三路独立抓取，关键句逐字摘录一致）。
- **核验到的锚句**：
  - §5："A HEALPix map has Npix = 12 N_side² pixels of the same area Ωpix = π/(3N_side²)"（A_leaf 文献腿）。
  - §5.3 首句："Pixel boundaries are non-geodesic"；式(19)–(22)："cos θ = a + b×φ in the equatorial zone, and cos θ = a + b/φ² in the polar caps"（极冠边在 (φ,z) 平面为 φ 的二次型——矢高口径判定的依据）。
  - 式(23)：θpix ≡ √Ωpix（hp_res 定义锚）。
- **用途**：A_leaf=π/(3N²) 文献腿；hp_res=211076.28514206142″/N；极冠矢高与自适应细分的边界曲线族口径。

### V2. Fruchter & Hook 2002（Drizzle）

- **标识**：PASP 114, 144；DOI [10.1086/338393](https://doi.org/10.1086/338393)；arXiv:[astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)（v2，即 PASP 发表版预印本）。
- **题名**（arXiv 摘要页原文，k_corr 补实验逐字核验）："Drizzle: A Method for the Linear Reconstruction of Undersampled Images"。（路线3 report 所记 "Coaddition of HST images: the 'drizzle' method" 与摘要页不符，以摘要页为准——旧稿题名错误一事按路线3 判定维持，正确题名按本条。）
- **核验方式**：arXiv v2 全文实取（路线1/2/3 + 补实验-k_corr 四路独立；**出处节号按分歧台账 D-03 终裁：父代理直验原文**）。
- **核验到的锚句**：
  - **权重公式＝§2 式(2)–(5)**（D-03 终裁）：式(2)/(3) 迭代含原句 "where a factor of s² is introduced to conserve surface intensity"；式(4)/(5) 权重和 W = Σ a·w。路线1 曾标"§7.2 式(7) 后"，系取错节，**撤换**。
  - **方差/相关＝§7 式(6)–(10)**（与权重分列引用）：§7.1 "Drizzle frequently divides the power from a given input pixel between several output pixels. As a result, the noise in adjacent pixels will be correlated."；§7.2 式(6)(7) 单输出像素方差（"where axy is the fractional area overlap of the drop of input data pixel dxy with the output pixel o"）；式(8)–(10) R=σc/σp 定义与闭式，唯一数值例 R=1.662（p=0.6, s=0.5）。
  - 子串级检索：全文不含 1.3883 / 1.4 / k_corr / covariance（k_corr 补实验，D-08）。
- **用途**：w_jp = a_jp/A_drop 的 s² 表面亮度守恒（§2）；k_corr 的必要性论证只可引其 §7.1 相关论断，k_corr 数值本身不是 F&H 内容（D-08）。

### V3. Van Oosterom & Strackee 1983（球面三角形立体角）

- **标识**：IEEE Trans. Biomed. Eng. BME-30(2), 125–126；DOI [10.1109/TBME.1983.325207](https://doi.org/10.1109/TBME.1983.325207)。
- **核验方式**：Crossref API（路线2/3）+ Semantic Scholar API（路线1）；公式 Ω = 2·atan2(det[a,b,c], 1+a·b+b·c+c·a) 由三路独立数值互校兜底（全天穷举互差 ≤3.3e-12）。
- **用途**：全部球面面积真值原语（VOS）；oracle 与生产共用式。

### V4. Sutherland & Hodgman 1974（多边形裁剪）

- **标识**：Comm. ACM 17(1), 32–42；DOI [10.1145/360767.360802](https://doi.org/10.1145/360767.360802)。
- **核验方式**：Crossref API（路线2/3）。
- **用途**：逐边裁剪原语；球面逐边大圆裁剪是本仓对平面 S-H 的推广（规格与实现一致陈述）。

### V5. Calabretta & Greisen 2002（WCS Ⅱ，TAN）

- **标识**：A&A 395, 1077；DOI [10.1051/0004-6361:20021327](https://doi.org/10.1051/0004-6361:20021327)；arXiv:[astro-ph/0207413](https://arxiv.org/abs/astro-ph/0207413)。
- **核验方式**：arXiv 全文实取（路线1/2）。
- **核验到的锚句/式**：§5.1.3 式(54) R_θ = (180°/π)·cot θ、式(55) 逆变换；"Since the projection is from the center of the sphere, all great circles are projected as straight lines"。
- **用途**：drop 足迹的 TAN 正逆投影；gnomonic 面积预算的投影出处（Snyder 的公式级 pinpoint 未取得前由本条承担）。

### V6. Calabretta & Roukema 2007（Mapping on the HEALPix projection）

- **标识**：MNRAS 381, 865–872；DOI [10.1111/j.1365-2966.2007.12297.x](https://doi.org/10.1111/j.1365-2966.2007.12297.x)；单作者预印本 arXiv:[astro-ph/0412607](https://arxiv.org/abs/astro-ph/0412607)（2004）。
- **核验方式**：Crossref API 核 DOI（路线2/3；注意 10.1111/j.1365-2966.2007.12289.x 是 Ross 2007 2dF 论文，初稿曾误写、已记录订正）。
- **用途**：HPX 支结构（赤道带/极冠分支）的独立参照；chart 分支一致性的旁证。

### V7. Fernique et al. 2015（MOC 1.0，IVOA Recommendation）

- **标识**：IVOA Recommendation 1.0；arXiv:[1505.02937](https://arxiv.org/abs/1505.02937)。
- **核验方式**：arXiv 页级（EXP-07-POLAR §5 引用；"叶侧 chart 精确边界 + drop 侧自适应细化"的同构性对照）。
- **用途**：chart 原生路径设计定位（EXP-07 正本历史内容）。

---

## 标注级 / 不列 VERIFIED（不得作为数值依据引用）

| 条目 | 状态 | 处置 |
|---|---|---|
| l'Huilier 1787, Nova Acta Upsaliensis 2, 37–44 | 书目级；原始 pinpoint UNRESOLVED（三路一致，台账 §4.2 开放项） | 公式以其现代教科书表述使用，数值由 VOS 全天互校兜底；引用保持书目级，不编造页码 |
| Snyder 1987, USGS PP 1395 | 存在性核验（gnomonic 章自 p.164）；公式级 pinpoint UNRESOLVED（PDF 抓取 200k 截断） | gnomonic 展开式以本单元独立推导＋实验（[实验:e5_projection_budgets.py]/[实验:exp06_projection_budget.py]）承担，Snyder 只列背景 |
| Kahan 2000, Miscalculating Area and Angles of a Needle-like Triangle | 定位级（EXP-07 §5 背景引用，未一手核验公式段落） | 仅作针状三角形数值问题的背景指引，不承载任何数值 |
| "Turner et al. 2006, A&A 458, 343" | **未证实**（Crossref + arXiv 双路检索无命中，路线3） | 停止转引 |
| "Calabretta & Greisen 1995, A&AS 114, 343" | **错误引证**（作者对/出处对不上，路线3） | 以 V5/V6 替代 |
| Huber & Ronchetti 2009 等权重文献 | 归 P1/P2 单元台账 | 本单元不引用 |

---

## 核验方法学备注

- 全部"原句"引文为全文实取后逐字摘录，非转述；未取得全文者明确标书目级。
- 路线1/2/3 相互独立执行核验（无通信），关键锚句三路摘录一致后采信；出处节号冲突（F&H §2 vs §7.2）按分歧台账 D-03 由父代理直验原文终裁。
- 18–19 世纪原始文献（l'Huilier）无稳定数字化标识时保持书目级，不以二手转述充当 pinpoint。
