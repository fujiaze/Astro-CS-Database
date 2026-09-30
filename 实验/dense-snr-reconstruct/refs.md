# refs.md — P4 重建稠密 SNR · 文献核验台账（实验单元）

**收录原则**：只收一手 VERIFIED 条目（三路审计任一路实际打开原始来源并核验通过，或本单元复核裁定）；格式 = DOI/arXiv + 核验方式 + 用途。标注级条目（锚形式 UNRESOLVED）单独列出并注明"不承担数值判据"。台账裁决后的出处订正以本表为准。

## VERIFIED（一手核验通过）

| # | 条目 | DOI / arXiv | 核验方式 | 用途 |
|---|---|---|---|---|
| 1 | Shepard 1968, Proc. 1968 23rd ACM Nat. Conf. 517–524 | DOI 10.1145/800186.810616 | Crossref 解析 + ACM DL 条目页（路线1 逐字段；路线3 ACM 页/Brandeis 交叉）。**订正**：旧稿 "SIAM J. Numer. Anal. 5, 372, DOI 10.1137/0705029" 错误（台账 A-P4-05） | IDW 幂次倒数加权形式的一手出处；p 为应用参数、原文未锁定普适值（D-05 文献腿） |
| 2 | Franke 1982, Math. Comp. 38, 181–200 | DOI 10.2307/2007474（亦 10.1090/s0025-5718-1982-0637296-4） | Crossref 逐条（路线1） | 散乱数据插值算子横向比较；样条类 vs 反距离权类选型文献腿 |
| 3 | Lu & Wong 2008, Comput. Geosci. 34, 1044–1055 | DOI 10.1016/j.cageo.2007.07.010 | Crossref（路线1） | IDW 幂次敏感性、按数据密度适配（D-05 方向佐证） |
| 4 | Keys 1981, IEEE TASSP ASSP-29(6), 1153–1160 | DOI 10.1109/TASSP.1981.1163711 | IEEE Xplore 文号 1163711 页 + Crossref 逐字段（题名 "Cubic convolution interpolation for digital image processing"、vol 29、issue 6、page 1153–1160）+ 多源一致（路线3）。**订正**：通行 DOI 10.1109/29.90969 解析 404（台账 A-P4-05）。**误差阶口径降级（本单元复核）**：IEEE 全文付费墙不可取，本表原记的 "O(h³)/O(h⁴) 误差阶" 未能由原文核实（该文为 4 抽头三次卷积核，其通行截断误差阶为 O(h²)）；故本条只作**收敛阶方法学参照**，不承载任何误差阶数值——本单元样条 −3.97/−4.18 与双线性 −1.93/−1.98 的阶数一律由 exp03 实测与样条理论承担 | 双三次卷积核（插值算子形式出处）；收敛阶文献腿（仅方法学） |
| 5 | Zackay & Ofek 2017 I/II, ApJ 836, 187/188 | DOI 10.3847/1538-4357/836/2/187 与 /188；arXiv:1512.06872 / 1512.06879 | Crossref 逐条 + arXiv abs 摘要逐字（路线1；路线2 abs 页） | 最优叠加权方向佐证（"appropriate weights…maximize the SNR"摘要逐字）；II 最优性限定背景噪声受限——本链含源项方差面的逆方差最优性由 Cauchy–Schwarz 一般成立（理论腿自足） |
| 6 | Horne 1986, PASP 98, 609 | DOI 10.1086/131801 | Crossref 逐条（路线1）；ADS/IOP 页（路线3）。注意 DOI 10.1086/131901 是 Vrba 1986，易混 | 逐源方差加权最优提取（σ_F 口径方法学锚） |
| 7 | Naylor 1998, MNRAS 296, 339–346 | DOI 10.1046/j.1365-8711.1998.01314.x | Crossref 逐字段（路线1）；**本单元复核裁定（2026-09-26，api.crossref.org）**：路线3 所记 01333.x 实为 Cropper "Polarimetry of QQ Vul"（MNRAS 295, 353），判误订正 | 成像测光最优提取（通量型 SNR 成像版） |
| 8 | Górski et al. 2005, ApJ 622, 759 | DOI 10.1086/427976；arXiv:astro-ph/0409513 | arXiv abs + IOP 文章页（路线2/3） | 球面域几何语境（P4 稠密场在 P3 映射后的球面域上定义） |
| 9 | Akinshin 2022, *Finite-sample bias-correction factors for the median absolute deviation based on the Harrell–Davis quantile estimator and its trimmed modification* | arXiv:2207.12005 | arXiv abs 页题名逐字（路线2；本单元复核订正旧稿转述题名） | MAD 有限样本偏差 O(1/n) 语境（不改登记渐近常数） |
| 10 | Astier & Antilogus, *The shape of the Photon Transfer Curve of CCD sensors* | arXiv:1905.08677 | PDF 首页题名逐字（路线2；本单元复核订正原转述题名 "of area array CCDs"） | PTC 散粒斜率 1/乘性斜率 2 的领域语境（数值判据以路线2/3 实验闭环） |
| 11 | Rousseeuw & Croux 1993, JASA 88(424), 1273–1283 | DOI 10.1080/01621459.1993.10476408 | Crossref 元数据（路线2）；**本单元复核**：OpenAlex 记 oa_status=closed，Table 2 / “37% efficient” 原句本轮未能独立复核 | MAD 效率 ARE=0.3675 语境（k_eff 理论 1.1664；数值以 exp08 MC 闭环） |
| 12 | Ma & Shang 2014, *The nonlinear photon transfer curve of CCDs and its effects on photometry* | arXiv:1407.8280 | PDF 首页题名逐字（路线2；本单元复核补题名） | 同 10（PTC 语境） |
| 13 | Trujillo, Aguerri, Cepa & Gutiérrez 2001, MNRAS 328, 977–985 | DOI 10.1046/j.1365-8711.2001.04937.x；arXiv:astro-ph/0109067 | Crossref 逐字段 + arXiv PDF；正文 Eq.(1) 逐字："PSF(r) = (β−1)/(πα²)[1+(r/α)²]^{−β}, with the full width at half maximum, FWHM = 2α√(2^{1/β}−1)"，其后仅注 "where PSF(FWHM/2) = (1/2)PSF(0) and the total flux is normalized to 1" | Moffat 轮廓的 α↔FWHM 关系（FWHM = 2α√(2^{1/β}−1)）一手出处；P4-B01 σ 约定的文献腿——该文只定义 α，未定义 σ |

## 标注级（锚形式 UNRESOLVED，不承担数值判据）

| 条目 | 锚 | 缺口 | 处置 |
|---|---|---|---|
| Aitken 1935, Proc. R. Soc. Edinburgh 55, 42–48 | **DOI 10.1017/S0370164600014346（Crossref 逐字段可解析：作者 Aitken A. C.、题名 "IV.—On Least Squares and Linear Combination of Observations"、vol 55、page 42–48、issued 1936）** | 无。**DOI 订正（本单元复核）**：原行所记 “doi.org/10.1017/S0080456800012684 返回 Not Found” 实测成立，但该串**不是**本文的 DOI；本单元与 P1/P2 台账同用的 10.1017/S0370164600014346 解析正常，Crossref 记 issued 1936（卷 55 跨 1935–36，通行引 1935） | 逆方差定权（GLS）理论腿；纯理论引用 |
| Moffat 1969, A&A 3, 455 | ADS bibcode 1969A&A.....3..455M（路线2/3） | **未能独立核验**：无 DOI（老卷）；Crossref 按题名查无该卷记录；ADS 在线页要求人机校验（P4-m11） | PSF 轮廓族出处（仅"smooth 幂律核"陈述）；Moffat4 的 α↔FWHM 关系改挂 Trujillo 2001 逐字 Eq.(1)，σ 约定为项目定义（PSF.md §16），均不依赖本条目 |
| de Boor 2001, *A Practical Guide to Splines*, Revised ed., Springer | 书目级（路线1/3） | 版次页未在线核验 | 节点复现/C² 性质由实验独立证实（≤3.6e-15、收敛阶 −3.97/−4.18），不依赖本引用 |
| Huber 1981 / Rousseeuw & Croux 原文全文 | 定性引用（路线3） | 付费墙未打开 | 仅 mesh 中值滤波定性支撑；实验不依赖其数值 |

## 核验方法

- Crossref：GET api.crossref.org/works/{DOI}，题名/作者/年份/卷页逐字段比对（本单元对 Naylor 两候选 DOI 复核即用此法）。
- arXiv：arXiv API / abs 页抓取，题名/作者/摘要关键主张比对。
- ADS：bibcode 条目页；IEEE：Xplore 文号页；ACM：DL 条目页。
- 文献腿职责边界（三路共约定）：文献承担"算子形式/方法学有出处"；数值判据一律以固定 seed 实验闭环，文献不替实验背书数值。
