# refs.md · 文献核验台账（P1 通量积分拟合 / 实验/photometric-magnitude）

**口径**：只收一手 **VERIFIED** 条目（核验途径 + 关键原句或书目级证据可回溯）。本台账在实验重做三路 refs 的收口结果（见本文件各条目的核验途径列）基础上成立；分歧裁决一律以 `实验/裁决台账.md`（D-xx）与 `docs/DISPUTES.md`（A-P1-xx）为准。
**格式**：DOI/arXiv ＋ 核验方式 ＋ 用途。

---

## V1 Kafadar 1983（c = 4.685 ⇔ 正态 95% 渐近效率的一手出处）

- **书目**：Karen Kafadar (1983), "The Efficiency of the Biweight as a Robust Estimator of Location", *J. Res. Natl. Bur. Stand.* 88(2), 105–116. DOI: [10.6028/jres.088.006](https://doi.org/10.6028/jres.088.006)，全文 PMC: [PMC6768164](https://pmc.ncbi.nlm.nih.gov/articles/PMC6768164/)。
- **核验方式**：全文取回（PMC），关键原句照抄："Asymptotically, c = 4.685 yields 95% asymptotic efficiency at the Gaussian…"。**台账 D-06**：父代理直验原文含 c=4.685 原句 ⇒ **锚有效**；路线2 的"PMC 锚内容不符"误判撤回。
- **用途**：`tukey_c = 4.685` 的文献锚（`PHOTOMETRY.md` §14 维持不改，按 D-06）。

## V2 statsmodels `statsmodels/robust/_tables.py`（c = 4.685 的开源逐字锚）

- **核验方式**：开源逐字核验（路径订正 P1-m06：原写 `statistical/robust/` 为 404；正确文件为
  [`statsmodels/robust/_tables.py`](https://raw.githubusercontent.com/statsmodels/statsmodels/main/statsmodels/robust/_tables.py)，
  L16–L27，L24 逐字 `0.95: (4.685065, 0.119414),`），值 4.685065 与本实验解析解 c* = 4.6850649 六位一致
  [实验:code/redo/route2/exp1_robust_constants.py]。
- **用途**：S1 三腿闭合的独立第三方数值锚。

## V3 Beaton & Tukey 1974（Tukey biweight 出处）

- **书目**：A. E. Beaton, J. W. Tukey (1974), "The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data", *Technometrics* 16, 147–185. DOI: [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)。
- **核验方式**：Crossref API 书目级（authors/volume/page/DOI 全符）；Crossref 与 OpenAlex 均记 issue=2（原文未取回，期号以出版商元数据为准）。
- **用途**：biweight 权函数 w=(1−u²)² 的历史出处。
- **引用等级（订正 P1-m06，审查 §1）**：原文**不可得**（T&F 403、JSTOR 反爬、`oa_status=closed`，无任何 OA 全文；可得摘要不含权重函数）⇒ 该条**只能作二手归属**，本单元**不得**声称「w=(1−u²)² 出自该文」已核。

## V4 Holland & Welsch 1977（IRLS 权重常数表）

- **书目**：P. W. Holland, R. E. Welsch (1977), "Robust regression using iteratively reweighted least-squares", *Communications in Statistics – Theory and Methods* **6(9)**, 813–827. DOI: [10.1080/03610927708827533](https://doi.org/10.1080/03610927708827533)。
- **核验方式**：Crossref / OpenAlex 书目级（期号订正 P1-m06：两库一致为 **6(9)**，本单元原写 6(8)；
  卷 6、页 813–827、1977 一致）。
- **用途**：IRLS 权函数与权重常数表的经典出处（与 V3 配对引用）。

## V5 Rousseeuw & Croux 1993（MAD 效率 37% / 标准化方差 1.361）

- **书目**：P. J. Rousseeuw, C. Croux (1993), "Alternatives to the Median Absolute Deviation", *JASA* 88(424), 1273–1283. DOI: [10.1080/01621459.1993.10476408](https://doi.org/10.1080/01621459.1993.10476408)。
- **核验方式**：Crossref 书目 ＋ KU Leuven 官方镜像 PDF 全文 OCR；关键原句照抄："the MAD is only 37% efficient"；Table 2 n=∞ 行 MAD 标准化方差 = **1.361**。**本单元复核（对抗审查 R02）**：Crossref 逐字段可解析，OpenAlex 记 `oa_status=closed`，期刊页与台账所称的 KU Leuven 镜像本轮均不可达 ⇒ **该两条原句本轮未能独立复核**，登记为「未能核实（原因：付费墙 + 无 OA 全文）」；1.166 的数值判据由本单元解析闭式与 exp08 类 MC 实验承担，不依赖该表页。
- **用途**：判据因子 1.166 = √1.361 的文献锚（**按台账 A-P1-01 订正解释标签**：1.361 是 MAD 尺度估计量的**标准化方差**，1.166 = √1.361 是 σ̂ 的相对标准差因子）。

## V6 Gaia DR3 总览（源计数与密度归一）

- **书目**：Gaia Collaboration et al. (2023), *A&A* 674, A1. arXiv: [2208.00211](https://arxiv.org/abs/2208.00211)。
- **核验方式**：arXiv 摘要页 ＋ 1.8e9 源计数原句（路线1）；**arXiv 号自纠**：审查引文 2205.11321 被证伪（实为计量论文），正确号 2208.00211。
- **用途**：FOV/阶梯实验的天空密度归一（**820/deg²，G<16 全天平均——该数值本单元复核未能在 arXiv:2208.00211 全文中核到**：arXiv 预印本 23 页正文与 Crossref 书目均无该密度数值，A&A 发表版全文本轮未取 ⇒ 登记为「未能核实」，不得作为该文献的已核数值引用；正文亦未以该文献支撑 820/deg² 这一数字）。

## V7 Montegriffo et al. 2023a — Gaia XP 合成测光（F_syn 口径）

- **书目**：Gaia Collaboration, P. Montegriffo et al. (2023), "The Galaxy in your preferred colours. Synthetic photometry from Gaia low-resolution spectra", *A&A* 674, A33. arXiv: [2206.06215](https://arxiv.org/abs/2206.06215)，DOI: [10.1051/0004-6361/202243709](https://doi.org/10.1051/0004-6361/202243709)。
- **核验方式**：Crossref 书目 ＋ 摘要/正文原句照抄："Synthetic photometry directly tied to a flux in physical units…"；"passbands… combination of… filter… the sensitivity curve of a photon-counting detector"。
- **用途**：`F_syn = ∫F_λ·T·Q·λ dλ` 的物理刻度与通带定义佐证。

## V8 Montegriffo et al. 2023b — XP 外定标（±2% 精度）

- **书目**：P. Montegriffo et al. (2023), "**Gaia Data Release 3:** External calibration of BP/RP low-resolution **spectroscopic data**", *A&A* 674, A3. arXiv: [2206.06205](https://arxiv.org/abs/2206.06205)，DOI: [10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)。
- **核验方式**：Crossref/arXiv 书目（题名与 DOI 按 P1-m06 订正：原题名漏 "Gaia Data Release 3:" 与 "spectroscopic data"，未给 DOI）
  ＋ §8.1 原句（XP 外定标 ±2%, λ≳400 nm；Fig. 27 作图重标度原句）。
- **用途**：绝对刻度系统差量级；佐证 `10^(−0.4·G)` 不入 F_syn（`docs/fsyn_convention.md`）。

## V9 ESA Gaia DR3 官方文档（XP 采样谱与零点定义）

- **核验方式**：官方文档一手原句：§20.12.4 `xp_sampled_mean_spectrum` flux 字段 "Externally-calibrated combined BP and RP flux"，343 点 @2 nm，336–1020 nm（官方表述逐字："All mean spectra are sampled to the same set of absolute wavelength positions, viz. 343 values from 336 to 1020 nm with a step of 2 nm"）；§5.4.1 式 (5.41) ⟨f_λ⟩ = ∫f_λ S λ dλ / ∫S λ dλ；§5.4.1 绝对刻度 "Thus 1 % is thought to be the current state-of-the art uncertainty on the ‘absolute’ calibration scales."。**节号订正（本单元复核）**：1 % 那句位于 **§5.4.1 正文、Zero points 小节之前**，不在 §5.4.2（§5.4.2 是 Validation）。
- **一手原句（本单元随文件入库）**：零点不可用于合成测光的原句在
  [§5.4.1 *Zero points*](https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html)
  **Table 5.4 之后的 Note** 段，原文照抄：

  > "Note however that as seen in Section 5.4.1 Gaia fluxes are published as *photo-electrons s⁻¹*
  > and are not normalised by the telescope pupil area, so **the given zero points are intended only
  > to be applied to Gaia fluxes and are not suitable for synthetic photometry computations**."

  - **链接订正（本单元复核）**：上条 Note 原引路径 `Data_analysis/chap_cu5pho/sec_cu5pho_calibr/ssec_cu5pho_photCal.html` 现返回 **404**；现行有效路径为 `Data_processing/chap_cu5pho/cu5pho_sec_photProc/cu5pho_ssec_photCal.html`（本轮实测 HTTP 200，抽取正文 28 270 字符，Note 原句逐字命中）。
  - 该句是 **Note**（不是正文条文），且限定语是"给定零点"（`the given zero points`）而非"全部 XP 通量"；
    本单元据此只声明"官方未给商用合成测光的零点"，**不得**引伸成"XP 通量不可用于合成测光"。
  - §20.12.4 表页永久链接（现行有效）：[ESA Gaia DR3 文档 §20.12.4](https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html)。

## V10 Gaia DR3 XP 可用域（两级域；出处 = Montegriffo 2023 附录 B）

- **出处订正（P1-M02）**：该域声明的原始出处是 **Montegriffo et al. 2023（A&A 674, A3）附录 B**，不是 ESA 文档；
  原文（附录 B）："For a **subset** … only sources brighter than **G = 15 mag** … **also** provided in the **sampled**
  representation"；**连续表示（continuous / XP continuous mean spectrum）发布到 G < 17.65**（219 197 643 源）。
- **两级域（必须分别写）**：① 采样表示（`xp_sampled_mean_spectrum`，本单元 `*.xpsd` 解码所需的那一支）的额外子集以 **G = 15 mag** 为界；
  ② **连续表示**到 **G < 17.65**。两者不是同一个域，引用时不得合并成一句 "XP 可用域 G<15"。
- **核验方式**：一手来源 [arXiv:2206.06205](https://arxiv.org/abs/2206.06205) 附录 B（原句照抄）；
  官方表页 [ESA Gaia DR3 文档 §20.12.4](https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html)、
  [§20.12.3](https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_continuous_mean_spectrum.html)；
  `has_xp_sampled` / `has_xp_continuous` 标志位见 `gaia_source` 表说明。
- **本单元样本域（如实登记）**：M16 锥 208 源中 G<15 仅 58 源、15 ≤ G < 17.65 有 144 源、**G ≥ 17.65 有 6 源**（`magG_max = 20.322`）
  ⇒ 6/208 源落在**连续表示域之外**、且整个样本跨越采样表示的子集界。本单元在 G ≤ 18 上的绝对刻度由实测锚定
  （`docs/science/PHOTOMETRY.md` §16.5 第 7 条：median 偏差 −0.0037 mag、MAD 0.0033，n=11272），不是由"G<15 域内"推断。
- **用途**：F_syn 参考侧的适用域声明（两级域）；越界源的登记面。

## V11 Aitken 1935（反方差加权口径）

- **书目**：A. C. Aitken (1935), "On Least Squares and Linear Combination of Observations", *Proceedings of the Royal Society of Edinburgh* 55, 42–48. DOI: [10.1017/S0370164600014346](https://doi.org/10.1017/S0370164600014346)。
- **核验方式**：Crossref API 书目级（本轮补核：卷 55、页 42–48；Crossref issued 记 1936，惯例引 1935，如实记录）。
- **引用等级（订正 P1-m06，审查 §1）**：原文（1935 年 Proc. R. Soc. Edinb.）**无 OA 全文**，仅书目级可核 ⇒ 作**二手归属**，不得作公式的一手锚。
- **用途**：项目反方差（inverse-variance）加权口径的统一定位锚（**负责人已批**：P1/P2 涉及权重处引用）；本单元 IRLS 权为稳健权（非反方差权），反方差口径通过 `ivar′ = ivar/α²` 的量纲变换进入下游（P5）。

## V12 GaiaXPy 2.1.4（开源实现锚）

- **核验方式**：官方开源实现逐字核验：`src/gaiaxpy/spectrum/sampled_spectrum.py:114` 纯线性组合，无星等因子；`calibrate()` vs 官方 `XP_SAMPLED` 产品比值中位 1.000000。
- **用途**：F_syn 绝对口径的实现侧旁证（`docs/fsyn_convention.md`）。

## V13 Akima 1970（F_syn 谱插值基元）

- **书目**：H. Akima (1970), "A New Method of Interpolation and Smooth Curve Fitting Based on Local Procedures", *J. ACM* **17(4)**, 589–602. DOI: [10.1145/321607.321609](https://doi.org/10.1145/321607.321609)。
- **核验方式**：Crossref 书目级（DOI 10.1145/321607.321609：题名、年、卷 17、**页域 589–602**、期 4 全符——**页域订正 P1-m06**：本单元与 `docs/science/PHOTOMETRY.md` §14a 条目 10 历史只给首页 589）。
- **用途**：`spectrum_integrator.cpp` 的 Akima 子样条（`F_syn = ∫F_λ·T·Q·λ dλ` 的插值基元）。

---

## 引文订正记录（原 `run/SCI-401/lit/verified_refs.md`，按 P1-m10 随单元入库）

`run/` 目录为 gitignore 且会被轮次回收，原一手记录 `run/SCI-401/lit/verified_refs.md` 已不存在 ⇒ 其结论随本台账入库：

1. Montegriffo et al. 2023 的 XP 外定标论文是 *A&A* **674, A3**（不是 A33；A33 是合成测光那篇，见 V7）；DOI [10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)。
2. [10.1051/0004-6361/202039587](https://doi.org/10.1051/0004-6361/202039587) = **Riello et al. 2021, A&A 649, A3**（Gaia EDR3 测光），题名与卷页以该 DOI 落地页为准。
3. Bohlin, Hubeny & Rauch 2020 = *AJ* **160, 21**（HST 通量标准），DOI [10.3847/1538-3881/ab94b4](https://doi.org/10.3847/1538-3881/ab94b4)。

另有本轮新增的一手抓取记录：`docs/science/PHOTOMETRY.md` §14a 条目 5 的 "not suitable for synthetic photometry computations" 句的一手全文、落点与限定语分析，**已按 V9 条目随本文件入库**（P1-m11；原始抓取落 `run/FINAL-07/logs/p1-m11-photcal-quote.txt`，该路径不入库，故正文自带原句）。

---

## 不列 VERIFIED（待补脚注）

- **Croux & Rousseeuw 1992/1993**（有限样本 MAD 偏差表值）：MC 复算佐证偏差 ≤0.93%，原文未取得——**不列 VERIFIED**，作"待补"脚注（简报口径）；相关数值以推导腿 ＋ 本单元 MC 实验（[实验:code/redo/route3/exp_S11_zp_sample_floor.py]）自足承载。
- **Huber & Ronchetti 2009 §6.5**：书目级；页码 UNRESOLVED——降级引用或改引 Kafadar（V1），本单元正文改引 V1。
- **Lindegren 2021 亮端数字**：仍开放（不阻成稿，见诚实边界）。
