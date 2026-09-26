# P3 守恒映射算子 · 路线 3 参考文献（refs.md）

核验方式约定：**一手核验** = 通过 arXiv 全文 / Crossref DOI / 出版社官网读到原文相应句段并逐句摘录；**存在性核验** = 确认文献存在与题名/出处，但未读到待引句段；**未证实** = 双路检索（Crossref bibliographic + arXiv）查无此文。检索工具：arxiv_search、journal_lookup、web_search、web_fetch / scholar_fetch。

## A. 一手核验（本路线依赖的每一处锚句都已读到原文）

1. **Górski, Hivon, Banday, Wandelt, Hansen, Reinecke, Bartelmann 2005**, “HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere”, ApJ 622, 759. arXiv:astro-ph/0409513（https://arxiv.org/abs/astro-ph/0409513）。
   - §5：原句 “Npix = 12N²side pixels of the same area Ωpix = π/(3N²side)” —— R3-01 锚。
   - 式(23)：θpix ≡ √Ωpix —— hp_res 定义锚（√(π/3)/N rad = 211076.28514206142″/N）。
   - §5.3（边界构造）：原句 “cos θ = a + b×φ in the equatorial zone, and cos θ = a + b/φ² in the polar caps”（式 19–22）—— 真边曲线族与 R3-06/R3-07 的口径判定锚。**注意：该节明示极冠边在 (φ,z) 平面是 φ 的二次型，非直线。**
2. **Fruchter & Hook 2002**, “Coaddition of HST images: the ‘drizzle’ method”, PASP 114, 144. arXiv:astro-ph/9808087（https://arxiv.org/abs/astro-ph/9808087）。
   - §2：累加式 (2)–(5) 与原句 “a factor of s² is introduced to conserve surface intensity”（即 s²=Σ_p a_jp/A_drop 口径，对常量场逐位复现 S_p=B0）—— R3-03/R3-04 锚。
   - **锚订正**：`docs/science/DRIZZLE.md:40-52` 把该内容归 “§7.2 式(7)” —— §7.2 实为噪声相关节，锚错；真锚在 §2 式(2)–(5)。
3. **Van Oosterom & Strackee 1983**, “The Solid Angle of a Plane Triangle”, IEEE Trans. Biomed. Eng. BME-30(2), 125–126. DOI 10.1109/TBME.1983.325207（Crossref 核验；Ω=2·atan2(det, 1+a·b+b·c+c·a)）—— R3-01/03/06 面积原语锚。
4. **Calabretta & Roukema 2007**, “Mapping on the HEALPix projection”, MNRAS 381, 865–872. DOI 10.1111/j.1365-2966.2007.12297.x（Crossref 核验；单作者预印本 arXiv:astro-ph/0412607, 2004）—— 环序/边界几何的独立推导参照（R3-02/06）。
5. **Snyder 1987**, “Map Projections Used by the U.S. Geological Survey”, USGS Professional Paper 1395（https://pubs.usgs.gov/pp/1395/report.pdf）。
   - 存在性核验＋章级 pinpoint：gnomonic 投影章自 p.164；因 PDF 文本抽取 200k 截断，未读到公式级句段。本报告的 gnomonic 展开式（(1+ρ²)^{3/2}、dev∝h²）为**本路线独立推导并实验测定**（exp06），不依赖该书句段。

## B. 未证实 / 错误引证（幻觉锚清单，停止转引）

1. **“Turner et al. 2006, A&A 458, 343”** —— Crossref bibliographic 与 arXiv 双路检索均查不到与该卷页匹配的、与 drizzle/重投影相关的文献。**未证实，停止转引**（UNRESOLVED 登记在 report.md 自报节）。
2. **“Calabretta & Greisen 1995, A&AS 114, 343”** —— 作者对与出处对不上：Greisen & Calabretta 的 WCS 论文是 2002, A&A 395, 1061；球面 mappings 是 Calabretta & Roukema 2007（见 A-4）；1995 年 A&AS 114 卷亦无此文。旧引证错误，以 A-4 为准。
3. **F&H 题名错误**（旧稿）：正确题名为 “Coaddition of HST images: the ‘drizzle’ method”（2002 发表于 PASP；预印本题名版本差异见 arXiv 页）。
4. **VOS 题名错误**（旧稿）：正确题名 “The Solid Angle of a Plane Triangle”（A-3）。
5. **Górski §5.3 “节号不可核”（审查员暂记）** —— 已过时：A-1 第 3 条一手闭合（本路线实验同时证明该节对实现口径判定的实质影响：02 line-58 的 (φ,z) 直线全称命题在极冠为假）。

## C. 仓内锚核验记录（只读抽验，行号以本轮核验时为准）

| 锚 | 位置 | 判定 | 实验证据 |
|---|---|---|---|
| PI/(3nside²) | spherical_overlap.cpp:1239–1242 | 正确 | exp01（A_leaf 互证 ≤3.3e−12） |
| HP_CIRCUMRADIUS_FACTOR 1.25 注释 | spherical_overlap.cpp:42 | 数值成立裕量 20.9%；“1.14 经验值” 未再现 | exp04（全域 1.0415） |
| “偏差<4e−8” 注释 | spherical_overlap.cpp:1002 | **错**（真值 ≈4.16e−7 rad @1e−3） | exp06 part_b |
| max_angular_radius_for_tan=1e−3 | spherical_overlap.cpp:1078–1091 | 阈值处支路偏差 ~θ² 量级成立 | exp06 part_b |
| wcs_epsilon=max(scale·1e−12, 1e−11) | spherical_overlap.cpp:942 | 结构性守护阈值，写豁免理由 | exp03 间接覆盖 |
| “机器精度”＋缺 floor 表述 | spherical_overlap.h:221–230 | 表述与实现不符 | exp07（q=floor(255S+0.5)） |
| 1e−6 阈值 | spherical_overlap.cpp:574 | 阈值存在；矢高 6.39e−2≫1e−6·hp_res | exp05 |
| weight=overlap/drop | drizzle_engine.cpp:1617 | w_jp=A_drop 归一口径正确 | exp03（守恒 8.2e−15） |
| /A_pixel 契约 | ASTROCS_DESIGN.md:218（行号自 209 漂移） | **错**（应 A_drop） | exp03 负例（Σ_p w′=pf²） |
| 211034.6 | hp_drizzle_api.h:114 | **禁抄值**（相对差 −1.97e−4，nside 决策窗翻转） | exp08 |
| orchestrator.cpp:180–181,198 / module_adapters.cpp:7603 | 链路接线 | 与链条位置节一致 | exp03 控制点用例 |
| DRIZZLE.md:40–52 | docs/science | w_jp 段有效；无 π/(3N²) 锚；F&H 节号错 | exp01/02/03 |
| 02 line-58 “(φ,z) 直线” | 08_修复包/④/02 | **全称命题在极冠为假** | exp05（两口径差 3.4 倍） |
| 02 §1.3 表 / §1.4 角点规则 | 08_修复包/④/02:77–129 | 数值逐位复现；构造逐位复现 | exp01/02/04 |

## D. 检索式与路径备注

- F&H 全文：scholar_fetch(arXiv PDF) 后定位 “s² is introduced” 与式 (2)–(5) 所在节；§7.2 标题确认为噪声相关。
- Górski 全文：scholar_fetch(arXiv PDF)，§5 搜 “Ωpix”、§5.3 搜 “equatorial zone”。
- VOS：journal_lookup + Crossref DOI 解析页核对题名与作者。
- C&R：journal_lookup（MNRAS 381, 865）+ arXiv:astro-ph/0412607 页面（作者栏单作者，2004；正式版 2007 两作者）。
- Turner 2006：web_search(Crossref bibliographic) + arxiv_search(au:Turner, drizzle/重投影关键词) 双路无命中。