# P3 守恒映射算子 · 路线1 文献核验记录

核验日期：2025-09-26/27（会话内）。核验方式：arXiv API（arxiv_search）、一手 PDF/abs 页抓取（scholar_fetch / web_fetch）、Semantic Scholar API。原则：只记本人本轮亲眼核验过的内容；查不到的记 UNRESOLVED，不编造。

---

## R1. Górski et al. 2005（HEALPix 等面积像元）——已核验，一手

- arXiv: [astro-ph/0409513](https://arxiv.org/abs/astro-ph/0409513)（v1，提交 2004-09-21）；发表版 ApJ 622:759–771，DOI [10.1086/427976](https://doi.org/10.1086/427976)。
- 作者：K. M. Górski, E. Hivon, B. D. Wandelt, A. J. Banday, F. K. Hansen, M. Reinecke, M. Bartelmann（早前笔记 "Bartelman" 为笔误，以 arXiv 页为准）。
- 关键 pinpoint（自全文抓取核对）：
  - §5 原句（紧邻 §5.1 "Pixel Positions" 之前）：**"A HEALPix map has Npix = 12 N_side^2 pixels of the same area Ω_pix = π/(3 N_side^2)"** —— S1 的文献腿：A_leaf = π/(3N²) sr 为一手明文。
  - §4 式(1)：cos θ* = (N_θ − 1)/N_θ = 2/3（等面积纬度定位）；§4：Ω_pix = 4π/(N_θ N_φ)。
- 用途：S1（leaf 面积）、S2（nside 尺度常数的文献背景）、E1 的 chart 几何出处。

## R2. Fruchter & Hook 2002（Drizzle）——已核验，一手，含 §7.2 pinpoint

- arXiv: [astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)（v2，提交 1998-08-10）；发表版 PASP 114, 144。
- 关键 pinpoint：
  - §2：pixfrac = "ratio of the linear size of the drop to the input pixel"；s = "ratio of the linear size of an output pixel to an input pixel"。
  - 式(2)(3)：含 "a factor of s² is introduced to conserve surface intensity"。
  - 式(4)(5)：W = Σ a·w；I = Σ d·a·w·s²/W。
  - §7 "NOISE IN DRIZZLED IMAGES"、§7.2 "The Calculation"、§8（全文锚点 @22445 / @24635 / @29810）。
  - **§7.2 式(7) 后紧跟原句：** "where axy is the fractional area overlap of the drop of input data pixel dxy with the output pixel o" —— 权重 = drop 与输出像元的**面积交叠比**，一手 pinpoint 已取得（补上审查员"pinpoint 未证"的缺口）。
- 用途：S4（w_jp = a_jp/A_drop 口径的文献腿）、E4。

## R3. Calabretta & Greisen 2002（WCS Ⅱ，TAN 投影）——已核验，一手

- arXiv: [astro-ph/0207413](https://arxiv.org/abs/astro-ph/0207413)（v1，提交 2002-07-19）；发表版 A&A 395, 1077。
- 关键 pinpoint（§5.1.3 "TAN: gnomonic"）：
  - 式(54)：R_θ = (180°/π)·cot θ；式(55)：逆变换。
  - 原句："Since the projection is from the center of the sphere, all great circles are projected as straight lines"（大圆 → 直线，故四角点弦四边形在 TAN 下是精确边界表示）。
- 用途：S5（gnomonic 面积预算的投影出处）、E5。

## R4. Van Oosterom & Strackee 1983（球面三角形 solid angle）——已核验，元数据一手

- "The Solid Angle of a Plane Triangle"，IEEE Trans. Biomed. Eng., 1983，DOI [10.1109/TBME.1983.325207](https://doi.org/10.1109/TBME.1983.325207)，PMID 6832789，作者 A. van Oosterom, J. Strackee。
- 核验途径：Semantic Scholar API（api.semanticscholar.org/graph/v1/paper/DOI:10.1109/TBME.1983.325207）；IEEE 摘要页 closed，公式本体（Ω = 2·atan2(|a·(b×c)|, 1+a·b+b·c+c·a)）以开放二手实现与本文 E6 独立数值互校佐证。
- 用途：S3/S8（真面积真值源与分歧探测器）、E1/E2/E5/E6 的面积真值函数。

## R5. l'Huilier 定理——二手层已核验，原始 pinpoint UNRESOLVED

- MathWorld "Spherical Excess"（[mathworld.wolfram.com/SphericalExcess.html](https://mathworld.wolfram.com/SphericalExcess.html)）原句："The equation for the spherical excess in terms of the side lengths a, b, and c is known as l'Huilier's theorem"。另载 Hariot（1603）更早的立体角=球面过剩观察。
- 原始文献 S. L'Huilier, *Nova Acta Upsaliensis*（1787）条目 pinpoint：UNRESOLVED（gutenberg Todhunter 抓取失败，仅得重定向页）。
- 引用纪律注意：en.wikipedia "L'Huilier's theorem" 是另一条欧氏平面几何定理（1809），**不可引用**。
- 用途：S8、E6。

## R6. Calabretta 2004 "Mapping on the HEALPix grid"——存在性已核验

- arXiv: [astro-ph/0412607](https://arxiv.org/abs/astro-ph/0412607)（v1，提交 2004-12-23，单人预印本，未见正式发表）。存在性与作者已核；仅作背景引用，不承载数值结论。

## R7. Snyder 1987（USGS PP1395, Map Projections: A Working Manual）——书目已核验，章节 pinpoint UNRESOLVED

- 书目：[pubs.usgs.gov/publication/pp1395](https://pubs.usgs.gov/publication/pp1395)；gnomonic 章约在 PDF p.164。
- 全文 PDF 抓取在 200k 字符处截断，未及 gnomonic 章的 sec²c / sec³c scale-factor 原句 ⇒ **pinpoint UNRESOLVED**（诚实边界）。
- 处置：S5 的文献腿由 R3（C&G 2002 式(54)）+ 本报告自含推导承担；Snyder 只列背景。

---

## 核验失败与绕行记录（过程性，供审计）

1. arxiv_search 全题名查询 HEALPix 返回无关结果 → 改字段组合查询命中。
2. web_search 多次空返回 → 改 web_fetch 直取已知 URL（Wikipedia、MathWorld、Semantic Scholar API）。
3. web_fetch doi.org → ieeexplore 跨域重定向不被自动跟随 → Semantic Scholar API 替代成功。
4. scholar_fetch USGS PP1395 全文 PDF 截断（LEN=200000, trunc=true）→ R7 pinpoint 记 UNRESOLVED。
5. gutenberg.org Todhunter《Spherical Astronomy》抓取仅返回重定向页（len=659）→ 放弃，R5 落二手层。
