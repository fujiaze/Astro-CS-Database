# refs.md · P1 通量积分拟合——文献腿核验记录（路线1）

**身份**：独立审计三路独立研究之路线1执行者。本件是 report.md 的文献核验台账，逐条一手取证。
**取证方式**：web_fetch（Crossref API、KU Leuven 官方镜像 PDF、PMC）、arxiv_search、arXiv PDF 全文；
扫描版 PDF 经 OCR（pdftoppm + tesseract）提取原句。所有引句为原文照抄，未做改写。
**核验日期**：2026-09-26（本会话）。

---

## R1 Beaton & Tukey 1974（Tukey biweight 出处）

- **结论**：CONFIRMED（期号有出入，见下）。
- **书目**：Albert E. Beaton, John W. Tukey (1974), "The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data", *Technometrics* **16**, 147–185. DOI: [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)
- **核验途径**：Crossref API `https://api.crossref.org/works/10.1080/00401706.1974.10489171`
  → 返回 authors=[Albert E. Beaton, John W. Tukey]、volume=16、page=147-185、published 1974-05。
- **出入记录**：Crossref 与 OpenAlex 均记 **issue=2**；仓内文档若写 16(1) 则期号与出版商元数据不符（卷/页/DOI 无误）。T&F 页面 Cloudflare 403，无法进一步旁证期号。
- **与本仓关系**：PHOTOMETRY.md §14/§14a 书目；02 C11 称该书目本轮 UNPROVEN——本路补证为 CONFIRMED。

## R2 Rousseeuw & Croux 1993（MAD 效率 37% / 标准化方差 1.361）

- **结论**：CONFIRMED（书目与两个数值均确认）。
- **书目**：Peter J. Rousseeuw, Christophe Croux (1993), "Alternatives to the Median Absolute Deviation", *JASA* **88**(424), 1273–1283. DOI: [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)
- **核验途径**：Crossref API（书目）＋ KU Leuven 官方镜像 PDF
  <https://wis.kuleuven.be/stat/robust/papers/publications-1993/rousseeuwcroux-alternativestomedianad-jasa-1993.pdf>（扫描版 OCR）。
- **关键原句**（照抄）：
  - 摘要："…the fact that MAD_n is aimed at symmetric distributions and its low (37%) Gaussian efficiency."
  - 引言："…whereas the location median's asymptotic efficiency is still 64%, the MAD is only 37% efficient."
  - 正文："This yields an efficiency of 58.23%, which is a marked improvement relative to the MAD whose efficiency at Gaussian distributions is 36.74%."
  - Table 2 "Standardized Variance of MAD_n … at Gaussian Data"，n=∞ 行 MAD 列 = **1.361**（→ AV(MAD/Φ⁻¹(3/4)) ≈ 1.36 σ²/n）。

## R3 Kafadar 1983（c=4.685 ↔ 正态 95% 渐近效率的一手出处）

- **结论**：CONFIRMED。
- **书目**：Karen Kafadar (1983), "The Efficiency of the Biweight as a Robust Estimator of Location", *J. Res. Natl. Bur. Stand.* **88**(2), 105–116. DOI: [10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)
- **核验途径**：<https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/>（全文可取回）。
- **关键原句**（照抄，含 4.685）：
  "Asymptotically, c = 4.685 yields 95% asymptotic efficiency at the Gaussian [i.e., n T converges in distribution to N(0,1.0526)]; within two sampling errors, the results in tables 1, 2, and 3 are consistent with this value."
- **注**：PHOTOMETRY.md §14 引 PMC6768164 即本文；本路确认该文献真实给出 4.685 与 95% 效率的显式联系。

## R4 Croux & Rousseeuw 1992（旁证 MAD 37% 效率）

- **结论**：CONFIRMED。
- **书目**：C. Croux, P. J. Rousseeuw (1992), "Time-efficient algorithms for two highly robust estimators of scale", *Computational Statistics* Vol.1, 411–428.
- **核验途径**：<https://wis.kuleuven.be/stat/robust/papers/publications-1992/crouxrousseeuw-timeeffalgosnqn-compstat-1992.pdf>（OCR）。
- **关键原句**（照抄）："its low (37%) gaussian efficiency."

## R5 Gaia XP 合成测光正本两篇（F_syn 口径 / ±2% 外定标精度）

- **结论**：两篇均 CONFIRMED。
- **A33**：Gaia Collaboration, P. Montegriffo 等 (2023), "Gaia Data Release 3: The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra", *A&A* **674**, A33. [arXiv:2206.06215](https://arxiv.org/abs/2206.06215)，DOI [10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)（Crossref 核验 vol=674, page=A33）。
  - 摘要原句（照抄）："Synthetic photometry directly tied to a flux in physical units can be obtained from these spectra for any passband fully enclosed in this wavelength range."
  - 正文原句（照抄，通带 = 滤镜＋探测器敏感曲线组合）："Today, actual TCs, which in the following we also refer to as passbands, are defined by the combination of the TC of an optical filter —which is designed to select the desired spectral window—, the sensitivity curve of a photon-counting detector (typically a CCD…), and the TC of the optical elements…"
- **A3**：P. Montegriffo 等 (2023), "Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data", *A&A* **674**, A3. [arXiv:2206.06205](https://arxiv.org/abs/2206.06205)，DOI [10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)。
  - 正文原句（照抄，±2% 外定标精度域）："for wavelengths higher than λ ~ 400 nm the accuracy of the calibration is mostly enclosed in the ±2% level marked by the two horizontal dashed blue lines, with some sources showing systematic offsets."
- **取证备注**：aanda.org 被 DataDome 拦截，A&A 证据改从 arXiv PDF 取得。

## R6 Gaia DR3 源计数（S6 阶梯密度标定）

- **结论**：计数数字 CONFIRMED；**一处 arXiv 号修正**。
- **书目**：Gaia Collaboration, A. Vallenari, A. G. A. Brown 等 (2023), "Gaia Data Release 3: Summary of the content and survey properties", *A&A* **674**, A1. **arXiv:2208.00211**，DOI [10.1051/0004-6361/202243940](https://doi.org/10.1051/0004-6361/202243940)。
- **原句**（照抄）："Gaia EDR3 provided celestial positions and the apparent brightness in G for 1.8 billion sources. For 1.5 billion of those sources, parallaxes, proper motions, and the (G_BP − G_RP) colour were also published."
- **修正记录**：核验委托初稿写 arXiv:2205.11321——**REFUTED**（该号实为一篇数字多用表计量论文，physics.ins-det）；正确号为 **2208.00211**。此出入如实登记。

## R7 幻觉锚核验（文件:行，本会话直读实测）

| # | 05 声称的锚 | 实测结果 | 判定 |
|---|---|---|---|
| H1 | `PHOTOMETRY.md:126`＝"载体始终是线性面亮度；星等只在派生/展示时换算" | :126 实为通带失配散度 0.449 mag 的佐证行；该语义现位于 **:227** 与 **:355（§16.1 ⑥）** | **STALE（陈旧锚）**：内容真实存在但行号漂移，按 :126 查阅会判"锚内容不符" |
| H2 | `PHOTOMETRY.md §16.5/:400`＝帧间独立/星数不作准入 | §16.5 起于 **:394**；**:400** 恰为"星少到拟合不成立时报拟合失败（不是门槛拦截）"条 | **成立** |
| H3 | `filter_curve_json.h:356/:444/:207` | :356=`map_filter_name` 声明行 ✓；:444=`load_curve` 声明行 ✓；:207=`check_curve_identity` 声明行 ✓ | **成立**（三处全部精确命中） |
| H4 | `star_matcher.cpp:493-501`（预过滤）、`:555`（步数 50）、`:580`（收敛 1e-6） | :493 预过滤循环体起 ✓；:555=`for (iter…_IRLS_MAX_ITER)` ✓；:580=`if (diff < _IRLS_CONVERGE)` ✓（`break` 在 :582） | **成立** |
| H5 | `frame_photometry_fit.cpp:166-174`（FOV 钳位） | :172-173 确为**条件**钳位（`fov<=0 \|\| fov>=30` 才钳 [1,10]），与 05 A-4b"无条件钳位"主张不符 | **成立（实现与 05 规格不一致确认）**；另 :167 印证 cd_det=0 ⇒ pixel_scale=0 ⇒ 走钳位分支（01/B12 危害实锚） |
| H6 | `pc_api.cpp` 注释宣称上界 10000 | **:290** 注释"直到星数在 2000-10000 范围或达 16.0 上限"；实码 :297-301 仅 `n_gaia >= 2000 \|\| i == 4` 停 | **成立（注释宣称未实现行为）** |

## R8 UNRESOLVED 清单（文献腿查不到即记，不硬凑）

1. FOV 缓冲 1.2 / 钳位界 1.0/10.0 / 异常窗 30.0：**无任何一手文献规定**（`GAIA_QUERY.md` 的 1.2 是赤道带 bbox 裕量，另一对象，不充当锚——与审查①判定一致）。定性：项目约定值，文献腿 UNRESOLVED；推导腿（几何式）与实验腿（本路 exp3）已补。
2. 自适应阶梯 {12,13,14,15,16} / 早停 2000 / 循环上限 5：**无文献规定**。Gaia DR3 源计数（R6）只供密度标定，不规定阶梯取值。定性：工程选择，实验腿（exp3）给出样本量-精度-成本三维标定。
3. `mag_min=6.0 / mag_max=16.0`：无文献锚；且 01/C4 指出拟合端与 ZP_syn 端两消费面口径分裂（本路 exp2 量化了分裂后果）。
4. `match_radius_px=2.0`：无文献锚（匹配半径属实现选择；本路给出理论误配率式＋实验标定）。
5. `max\|log10 m\| ≤ 1.0 dex`：无文献锚，正本注释自认"约定值"（01/C3）；本路实验量化其对登记失效模式（0.1 dex）恒不 binding、对 ≥1.1 dex 场有判别力。
6. `mag_tolerance = 3.0`：正本（PHOTOMETRY.md）明言该窗宽**不引用文献背书**（项目冻结值）；文献腿按定义 UNRESOLVED，理论腿（尺度不变性＋15.85× 通量比）与实验腿已补。
7. `max_stars = 5000`：无文献锚；σ/√N 边际收益曲线（exp3）支持其为成本-精度折中的工程选择。
8. `m_cut` 初值常数 6.0/1.5/2.0：无正本登记（01/C10）；本路实验证明初值不进科学值（exp5），修法按 C10 漏登处理。
