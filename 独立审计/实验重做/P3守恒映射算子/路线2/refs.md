# refs.md — 路线 2 文献核验记录

全部文献经 arXiv 全文 / crossref API / 出版商页面一手核验（web_fetch 实取）。
核验日期：2026-09-27。凡未取得一手来源者明确标注，绝不编造。

## 1. Górski, Hivon, Banday, Wandelt, Hansen, Reinecke & Bartelmann 2005

- **HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of
  Data Distributed on the Sphere**；ApJ 622, 759；
  DOI: 10.1086/427976；arXiv:astro-ph/0409513。
- 核验途径：arXiv 全文（HTML/PDF）实取。
- 关键原句（本报告引用处）：
  - §5 开篇: "A HEALPix map has Npix = 12 N_side² pixels of the same area
    Ωpix = π/(3N_side²)" —— F-01 A_leaf 的一手锚。
  - §5.3 首句: "Pixel boundaries are non-geodesic" —— 审查认为不可核的 pinpoint，
    本路在全文核得（报告 §4-②）。
  - §5.3 式 (19)–(22)：极冠/赤道带边界参数化；式 (23)/(24)：θpix ≡ √Ωpix。
  - §4 式 (1)：ring 边界 cos θ⋆ = (Nθ−1)/Nθ（极冠环边界族 z = 1−j²/(3N²) 的来源）。

## 2. Fruchter & Hook 2002

- **Drizzle: A Method for the Linear Reconstruction of Undersampled Images**；
  PASP 114, 144；DOI: 10.1086/338393；arXiv:astro-ph/9808087 (v2)。
- 核验途径：arXiv v2 全文实取。
- 关键原句/式（F-04 权重归一 pinpoint，报告 §4-①）：
  - §2 式 (2)/(3) 迭代式，含原句 "where a factor of s² is introduced to conserve
    surface intensity"（s = pixfrac；面亮度保持 → 权重以 drop 面积归一）。
  - §2 式 (4)/(5)：W = Σ a·w（输入图像 j 对输出像素 p 的贡献权重和定义）。
- 用途：w_jp = a_jp/A_drop 的文献依据；1/pf² 偏差的机制来源（错用 A_pixel 分母）。

## 3. Van Oosterom & Strackee 1983

- **The Solid Angle of a Plane Triangle**；IEEE Trans. Biomed. Eng. BME-30(2), 125–126
  （原文封面页码跨 125–127）；DOI: 10.1109/TBME.1983.325207。
- 核验途径：crossref API（web_fetch https://api.crossref.org/works/10.1109/TBME.1983.325207）。
- 公式：Ω = 2·atan2( det[a,b,c], 1 + a·b + b·c + c·a )。
- 生产/test 侧同式佐证：lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_geom.hpp
  geom_solid_angle_tri 使用 2·atan2(|num|, den)（fabs 变体；本路 p3lib 同式）。
  exp04 互校最坏 2.50e-12（全天穷举 N≤64）。

## 4. Sutherland & Hodgman 1974

- **Reentrant Polygon Clipping**；Comm. ACM 17(1), 32–42；DOI: 10.1145/360767.360802。
- 核验途径：crossref API。
- 用途：平面 S-H 逐边裁剪是 1974 原文的**平面**算法；球面逐边大圆裁剪是本仓推广
  （05 规格与 DRIZZLE_GEOMETRY.md 均如此陈述）。本路 p3lib 的球面 S-H
  （clip_halfspace）与平面 S-H（clip_rect_uv）分别实现并用于 exp02/exp03/exp10。

## 5. Calabretta & Greisen 2002

- **Representations of world coordinates in FITS**；A&A 395, 1077–1122；
  DOI: 10.1051/0004-6361:20021327；arXiv:astro-ph/0207413 (Paper II)。
- 核验途径：arXiv 全文实取。
- 关键式：§5.1.3 TAN（gnomonic）式 (54)：R_θ = (180°/π)·cot θ；
  式 (55) 逆变换 θ = tan⁻¹((180°/π)·R_θ)；Fig. 8。
- 用途：exp03/exp05/exp10 的 TAN 投影正逆（p3lib.tan_project/tan_deproject 的规范依据）。

## 6. Calabretta 2004 / Calabretta & Roukema 2007

- Calabretta 2004：**Mapping on the HEALPix grid** 单作者预印本，arXiv:astro-ph/0412607
  （与审查记录"单作者预印本"一致）。
- 期刊版：Calabretta & Roukema 2007, **Mapping on the HEALPix grid**，
  MNRAS 381, 865–872，DOI: 10.1111/j.1365-2966.2007.12297.x。
- 核验途径：crossref API 核 DOI（注意：10.1111/j.1365-2966.2007.12289.x 是 Ross 2007
  2dF 论文，**不是**本篇——初稿曾误写，已修正并在此记录）。
- 用途：HPX 投影支结构（赤道带/极冠分支、z=(2/3)(u+v−1) 族）的独立参照，
  用于核对 02 §1.1 chart 与生产 xyf2ang_replica 的分支一致性。

## 7. l'Huilier 1787（书目级，UNRESOLVED）

- L'Huilier, S. (1787). "Mémoire sur le minimum de cordon des triangles tracées
  sur la surface d'une sphère". Nova acta regiae societatis scientiarum upsaliensis,
  2: 37–44.
- 核验状态：**未取得一手数字化文本/DOI**（18 世纪期刊无稳定数字化标识）。
  引用保持书目级；球面角盈公式（l'Huilier 定理）以其现代教科书表述使用
  （tan²(Ω/4) = tan(s/2)·tan((s−a)/2)·tan((s−b)/2)·tan((s−c)/2)），
  数值正确性由 exp04 与 VOS 的全天互校（2.5e-12）与八分体解析锚（π/2 逐位）兜底。

## 8. 仓库内部只读参照（非文献，定位记录）

- 独立审计/08_修复包/④面积交叠与分配/02_已确立的算法与验证程序.md
  （理论腿权威：§1.1 chart/恒等式、§1.2 矢高表、§1.3 极叶闭式、
  §2 L199–216 守恒构造、§3.1 l'Huilier+VOS、§3.5 预算、量化表）。
- docs/algorithms/DRIZZLE_GEOMETRY.md（w_jp 归一、sumNorm/D_p 分母、pf=1 逐位、δ 公式）。
- lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp
  （xyf2ang_replica L711–774：生产 chart 分支结构；
  planar_polygon_area_n L1007–1031：正交切平面支实现）。
- lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_geom.hpp
  L141–160（geom_solid_angle_tri 的 |det| 形式；gnomonic 平面交叠原语的误差注释
  —— 报告 §5 建议的对象）。

## 9. 核验方法学备注

- arXiv 检索对学术性查询偶返回空 sources；改用 crossref REST API
  （web_fetch https://api.crossref.org/works/<DOI>）核 DOI 元数据成功。
- 所有"原句"引文均为全文实取后逐字摘录，非转述；未取得全文的一手来源（仅 §7）
  明确标记书目级。
