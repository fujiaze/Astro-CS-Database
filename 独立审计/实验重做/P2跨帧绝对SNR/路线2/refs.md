# refs.md — P2 跨帧绝对SNR · 路线 2 · 文献核验记录

核验日期：2026-09-26。核验手段：Crossref API（DOI 逐条解析）、arXiv 官方 abs 页、IVOA 官方文档 PDF（流级解压全文检索）、维基百科（仅作二手定位，不作为最终锚）。所有「核验状态」栏均记录实际抓到的字段。

## 已核验的一手文献（直接支撑本路实验）

| # | 文献 | 关键字段（实测抓取值） | 支撑条目 | 核验状态 |
|---|---|---|---|---|
| R1 | Pogson, N. (1856). "Magnitudes of Thirty-six of the Minor Planets for the First Day of each Month of [the year 1857]". **MNRAS 17, 12–15**. DOI `10.1093/mnras/17.1.12` | Crossref 实测：题名逐字、容器 MNRAS、卷 17、页 12–15、出版 1856-11-14 | P-CST-01（2.5 = 100^(1/5) 的对数斜率，星等制定义常数） | ✅ 一手（Crossref DOI 解析 + Wikipedia Apparent magnitude 二手一致） |
| R2 | Rousseeuw, P. J. & Croux, C. (1993). "Alternatives to the Median Absolute Deviation". **JASA 88(424), 1273–1283**. DOI `10.1080/01621459.1993.10476408` | Crossref 实测：题名、JASA、88、1273–1283、作者 Rousseeuw/Croux；KU Leuven 官方 PDF 亦在 | P-CST-03（MAD 渐近与替代量、效率比较） | ✅ 一手 |
| R3 | Huber, P. J. (1981). *Robust Statistics*, Wiley.（专著，位置参考：MAD 渐近方差与中位数位置 SE 渐近式 1/(4f(0)²)） | 专著无 DOI；以 R2 与本路闭式推导互证 | P-CST-03 / P-CST-07 | ✅ 标准专著（公认，无需网页核验） |
| R4 | Moffat, A. F. J. (1969). "A Theoretical Investigation of Focal Stellar Images in the Photographic Emulsion 1. The Image Empirical Function". **A&A 3, 455**. ADS bibcode `1969A&A.....3..455M` | 实测：Wikipedia Moffat distribution 参考栏逐字题名 + ADS bibcode 链接（ADS 页面反爬，改由二手页 + bibcode 双确认） | P-CST-05（Moffat 轮廓出处；β=4 的 FWHM↔σ 闭式） | ✅ 一手（二手页转引 + bibcode） |
| R5 | Fruchter, A. S. & Hook, R. N. (2002). "Drizzle: A Method for the Linear Reconstruction of Undersampled Images". **PASP 114, 144–152**. DOI `10.1086/338393`；arXiv:astro-ph/9808087 | Crossref 实测：PASP、114、144–152、题名逐字；arXiv abs 页实测题名/作者/日期 | P-CST-10 / P-CST-22（重采样后相关噪声与方差传播的现象学出处） | ✅ 一手 |
| R6 | IVOA (2017). *HiPS – Hierarchical Progressive Survey*, Version 1.0, Rec 19 May 2017. F. Fernique et al.（ed. P. Fernique）. DOI `10.5479/ADS/bib/2017ivoa.spec.0519F` | 官方 REC 页实测（题名/版本/日期/作者/DOI）；官方 PDF 下载后流级解压，实测正文属性表含 `hips_tile_width = 512` | P-CST-16（Δ = 512/8 = 64 px 的 tile 侧出处） | ✅ 一手（REC 文档原文命中 512） |
| R7 | Riello, M., De Angeli, F., Evans, D. W., et al. (2020/2021). "Gaia Early Data Release 3: Photometric content and validation". **A&A 649**（A3）. arXiv:2012.01916 | arXiv abs 页实测：题名逐字、作者前 19 名、提交 2020-12-03 | P-CST-19（Gaia G 星等制与零点口径的参照系；不直接给 6.0 的锚，见 report §P-CST-19） | ✅ 一手（arXiv；期刊卷期号以 arXiv 记录为准） |
| R8 | Bertin, E. & Arnouts, S. (1996). "SExtractor: Software for source extraction". **A&AS 117, 393–404**. DOI `10.1051/aas:1996164` | Crossref 实测：题名、A&AS、117、393–404 | P-CST-15（背景稳健估计与源掩膜实践出处；NOISE_MODEL §14a 已引） | ✅ 一手 |
| R9 | Stigler, S. M. (1977). "Do Robust Estimators Work with Real Data?". **Annals of Statistics 5(6), 1055–1098**. DOI `10.1214/aos/1176343997` | Crossref 实测：题名、Ann. Statist.、卷 5 | P-CST-06（截尾均值类估计子的经典实证与渐近依据） | ✅ 一手 |
| R10 | Shepard, D. (1968). "A two-dimensional interpolation function for irregularly-spaced data". *Proc. 23rd ACM National Conference*, 517–524. | Wikipedia Inverse distance weighting 条目以 "Shepard's method" 命名确认方法归属；ACM DL 反爬未直接命中 → 以方法命名页 + 多处二手引用登记 | P-CST-24（IDW 出处） | ⚠️ 二手核验（方法归属无疑义，页码以通行引用为准） |
| R11 | Goldberg, D. (1991). "What Every Computer Scientist Should Know About Floating-Point Arithmetic". **ACM Computing Surveys 23(1), 5–48**. | 公认综述；IEEE 754 double eps = 2⁻⁵² 由本路实验 `np.finfo(float64).eps` 直接实测（2.220446049250313e-16） | P-CST-13 | ✅ 标准综述 + 实测双锚 |
| R12 | Janesick, J. (2001). *Scientific Charge-Coupled Devices*, SPIE PM83（Ch.2）；Howell, S. (2006). *Handbook of CCD Astronomy*, 2nd ed.（Ch.4） | 专著；Poisson+read noise 组合模型的通行出处（NOISE_MODEL §14 已引） | P-CST-11 / P-CST-15 | ✅ 标准专著 |

## 核验中发现的引用纠错

1. **DOI `10.1086/341773` 不是 Fruchter & Hook**（Crossref 实测解析为一篇 2002 年儿科病毒学论文）。Fruchter & Hook 2002 的正确 DOI 是 **`10.1086/338393`**。任何引用旧 DOI 的仓内文档应订正。
2. 审查-1（审查-05-②-科学性-1.md）建议的 "Peacock (1984) Comparing Distributions, Ap.J. 283, 387" 与 "Huber 1981 p.128 Eq.(33)" 本路未采信为 P-CST-03/04 的文献锚：P-CST-04 是纯数学常数（本路闭式推导 + 双精度逐位复现即足），P-CST-03 的规范锚是 R2/R3。此为对审查员**订正建议**的选择性不同意，不影响其"文献腿缺失"的判定本身。

## 明确未查到（UNRESOLVED，绝不编造）

| 项 | 查询内容 | 结果 |
|---|---|---|
| U1 | `m_ref = 6.0`（Gaia G）作为跨帧参考星等档的一手文献锚 | 未找到任何一手来源把 6.0 mag 规定为参考星等；"第六星等≈肉眼极限"为通俗说法，无规范文献地位 → 判定为**单位制锚点**（见 report §P-CST-19） |
| U2 | `ζ = 1.152`（单 patch 相对标准误常数）的标定文献或实验登记 | 渐近推导给 1.16639；本路 N=64 实测 1.15425±0.00183、N=9216 实测 1.16175±0.00581 → 1.152 最可能是 **patch 尺度（N=64）MC 标定值**，但 05/NOISE_MODEL 均未注册标定条件 |
| U3 | P-CST-11 两个基准点（+14.5009% / +38.2524%）的 (RN, g, σ_sky) 物理参数 | 反解得 x=(RN/g)/σ_sky,total = 0.557715 / 0.954658，曲线族验证通过，但原始参数未在任何文档注册 |
| U4 | P-CST-23 数值系列（+4e-6 / +2.4e-4 / +1.33%）的 (β, 窗语义) | 语义已复原为 S2（通量精确+截断权重）+Moffat β≈2.5+窗帽 256（3 点同号同单调，2 点同量级），但未逐位复原 → 半 UNRESOLVED |
| U5 | P-CST-10 `k_corr` 标定值 1.3883 的原始实验网格 | 无归档；本路以 (n_eff=3, ρ≈0.194) 等相关玩具族复原出 1.3883，但属构造性复原非原始数据 |

## 审查件清单（本路输入）

- `独立审计/证据/审查-05-②-科学性-1.md`（25 条三腿核查 + 幻觉锚指控）
- `独立审计/证据/审查-05-②-科学性-3.md`（177 条三腿矩阵 + 订正句 C1–C7）
- 注：同目录无 ②-科学性-2 件（有 ①-科学性-2/-SUMMARY，与本模块无关）。
- 权威正本：`docs/science/NOISE_MODEL.md`（§2 符号、§4 输入域、§5/5a/5c/5d、§9a、§11、§14）；`独立审计/08_修复包/②跨帧绝对信噪比/05_正向规格.md`；`独立审计/08_修复包/②跨帧绝对信噪比/02_已确立的算法与验证程序.md`（§3 有锚常数表、§5 V-1~V-14）。
