# F-INSTR 逐条记录 —— 星点通量口径（文献 / 开源科学软件 / 本仓权威）

> 工作项 **F-INSTR-SURVEY**（RELEASE-02 裁决 A6）。配套定案：`reverse_verify/docs/f-instr-canon.md`。
> 规则（`reverse_verify/README.md §5`）：每条给**可核对标识** + 四要素。**禁止编造引用。**

## 核对方式与标记

| 标记 | 含义 |
|---|---|
| `[CR]` | Crossref REST / DOI content negotiation（`Accept: application/vnd.citationstyles.csl+json`），HTTP 200 |
| `[OpenAlex]` | OpenAlex API，HTTP 200 |
| `[arXiv]` | arXiv abs 页，HTTP 200 |
| `[ADS-scan]` | ADS 扫描页 / PDF 实页（`articles.adsabs.harvard.edu`），HTTP 200 + 正文 |
| `[ASCL]` | Astrophysics Source Code Library 记录页 |
| `[page]` | 官方文档实页抓取，HTTP 200 |
| `[repo]` | **本仓自核**：直接读源码/文档，给 `file:line`（最强证据） |
| `[MIS]` | **候选引用被证伪**（题名/出处/主题不符）—— 如实登记，不得沿用 |
| `[UNVERIFIED]` | 本轮未核到（**不得进入论文**） |

**工具限制（诚实登记）**：本会话 `web_search` 工具对多数查询返回无关结果；
ADS 记录页（`ui.adsabs.harvard.edu`）对脚本返回 405/JS 挑战；OUP(MNRAS) Cloudflare-403；IOPscience 验证码；`aanda.org` 403。
因此外部条目**以 DOI 注册元数据（Crossref CSL JSON）+ ADS 扫描页 + 官方文档实页**为准，逐条注明。

## 本轮核对发现的引用订正清单（**重要**）

| 候选引用（任务书/常识流传） | 核对结果 | 正确写法 |
|---|---|---|
| 「Labbe et al. 2003 关于 SNR 最优孔径」 | **主题错**。AJ 125, 1107 是 HDF-S ISAAC 近红外成像/光度红移论文 | 该主题应引 **Howell 1989 `[F-01]`** 与 **Naylor 1998 `[F-02]`**；Crossref/OpenAlex 检索**不存在** 2003 年 Labbé 一作的最优孔径论文 |
| Stetson 1992, "Progress in CCD Photometry", ASP Conf. Ser. 25, 291 | **题名/年份/出处均错** | Stetson **1993**, "**Further** Progress in CCD Photometry", **IAU Colloq. 136**, 291, DOI 10.1017/S0252921100007685 `[F-09]` |
| The Tractor, ApJ 821, 11; arXiv:1506.00837 | **标识全错**（ApJ 821,11 是 Lohfink et al. 的 Fairall 9 论文；该 arXiv 号是 hep-ph 的 leptogenesis） | **ASCL 软件记录 `ascl:1604.008`** `[F-11]` |
| Sirianni et al. 2005, DOI 10.1086/496934 | **DOI 错**（该 DOI 是 *Clinical Infectious Diseases* 的书评） | DOI **10.1086/444553** `[F-24]` |
| De Angeli et al. 2023, "Processing of BP/RP spectra" | **副标题错** | "**Processing and validation of** BP/RP low-resolution spectral data" `[F-21]` |
| Montegriffo et al. 2023, "The spectrophotometric catalogue: XP spectra" | **副标题错** | "**External calibration of** BP/RP low-resolution spectroscopic data" `[F-22]` |
| Bertin 2011 PSFEx, arXiv:1109.xxxxx | **arXiv 号不存在** | ASP Conf. Ser. 442, 435，**无 DOI、无 arXiv** `[F-10]` |
| Anderson & King 2006, PASP 118, 560 | **未核到**（PASP 118,560 是 Branch et al. 的超新星光谱论文） | 用 **Anderson & King 2000, PASP 112, 1360, DOI 10.1086/316632** 与 **2003, PASP 115, 113, DOI 10.1086/345491** `[F-12]`；"ACS ISR 2006-01" 标 `[UNVERIFIED]` |
| 「Stetson 1987 §ADDSTAR」 | **未核到**（1987 全文抓不到） | ADDSTAR 见 **DAOPHOT II 手册（Stetson 1998）§XX** `[F-25]`；**不得**写 "Stetson 1987 §ADDSTAR" |
| SDSS `psfMag` "不做孔径改正" | **错**（本轮由 SDSS DR17 官方文档证伪） | `psfMag` **做**局部改正 + **到 7.4″、随 seeing 变**的改正 `[F-13]` |
| Gaia 论文给出合成测光积分式（含 λ 因子） | **未核到** | 显式积分式在可抓取的一手页面中**没有**；λ 因子应引 **Bessell & Murphy 2012 `[F-23]`** + GaiaXPy 的"必须声明 photonic/energy"要求 `[F-36]` |

---

# A 孔径测光 / 最优孔径

### [F-01] Howell, S. B. (1989). Two-dimensional aperture photometry — Signal-to-noise ratio of point-source observations and optimal data-extraction techniques. PASP 101, 616. DOI 10.1086/132477 `[CR]`
- **通量定义**：孔径和 \(F_{\rm ap}(r)=\sum_{d_i<r}(d_i - b)\)；给出孔径内 SNR 的 CCD 方程
  \({\rm SNR} = F_{\rm ap}/\sqrt{F_{\rm ap}/g + n_{\rm pix}\sigma_{\rm sky}^2(1+n_{\rm pix}/n_{\rm sky})}\)，并求**最优孔径半径**（SNR 极大点）。
- **孔径无关性 / 视宁度鲁棒性**：**否**。最优半径依赖 seeing 与 S/N；seeing 变大最优半径变大。
- **借鉴点**：**SNR 最优孔径**的经典出处；CCD 方程形式（本仓 `snr_science.cpp:118` 已实现同式，注释即引 Howell 1989）。
- **不借鉴点**：**不把"最优孔径"当作通量口径** —— 最优孔径只优化 SNR，不消除孔径亏损，也不是总通量。
- **场景差异**：本项目要在**帧间**比通量（\(k_{\rm photo}\)），最优孔径随 seeing 漂移会直接变成假帧间差。
- **核对状态**：**已核对**（`[CR]` DOI 解析 + Crossref REST 双路，题名/卷/页/年一致）。

### [F-02] Naylor, T. (1998). An optimal extraction algorithm for imaging photometry. MNRAS 296, 339–346. DOI 10.1046/j.1365-8711.1998.01314.x `[CR]`
- **通量定义**：把 Horne 1986 的最优提取推广到**成像**：以归一化 PSF \(p\) 与逐像素方差 \(\sigma_i^2\) 加权，
  \(F = \frac{\sum_i p_i (d_i-b)/\sigma_i^2}{\sum_i p_i^2/\sigma_i^2}\)，\({\rm Var}(F)=1/\sum_i p_i^2/\sigma_i^2\)。
- **孔径无关性 / 视宁度鲁棒性**：**是（条件性）**。式子里**没有孔径半径**；seeing 只改变 \(p\)，只要 \(p\) 跟着更新，\(F\) 不变。
- **借鉴点**：**D2 口径的直接依据**；也是"PSF 加权积分 = 最小方差无偏总通量估计"的可引出处。
- **不借鉴点**：它假设 \(p\) 与 \(\sigma_i^2\) **已知**；本项目 \(p\) 是逐帧拟合的，\(\sigma_i^2\) 的增益项在帧头不可得。
- **场景差异**：本项目是**欠采样 + 时变 seeing**；Naylor 的数值算例为较好采样。
- **核对状态**：**已核对**（`[CR]`；OUP 落地页 403，故只用 DOI 注册元数据）。

### [F-03] Horne, K. (1986). An optimal extraction algorithm for CCD spectroscopy. PASP 98, 609. DOI 10.1086/131801 `[CR]`
- **通量定义**：光谱域最优提取 \(F = \sum_i p_i(d_i-b)/\sigma_i^2 \big/ \sum_i p_i^2/\sigma_i^2\)（\(p\) 为归一化空间轮廓）。
- **孔径无关性 / 视宁度鲁棒性**：**是（条件性）**，同上（对轮廓形状的归一而非求和范围敏感）。
- **借鉴点**：最优提取的**原始出处**；**本仓已在用**（`snr_science.cpp:110-118` 注释原文引 Horne 1986，`snr_estimator.h:245` 同）。
- **不借鉴点**：光谱场景（一维轮廓、天空沿波长变化）；不能照搬其天空模型。
- **场景差异**：本项目二维成像；但加权公式同形。
- **核对状态**：**已核对**（`[CR]`）。

### [F-04] Labbé, I. et al. (2003). Ultradeep Near-Infrared ISAAC Observations of the Hubble Deep Field South: Observations, Reduction, Multicolor Catalog, and Photometric Redshifts. AJ 125, 1107–1123. DOI 10.1086/346140 `[CR]` `[MIS]`
- **通量定义**：**该文不是关于最优孔径的论文**。它是 HDF-S 的 ISAAC 近红外成像/多色星表/光度红移论文。
- **孔径无关性 / 视宁度鲁棒性**：不适用。
- **借鉴点**：**无（不得作为"SNR 最优孔径"的引用）**。
- **不借鉴点**：**整个引用不成立**。
- **场景差异**：不适用。
- **核对状态**：**已核对为证伪**（`[CR]` DOI 10.1086/346140 解析为上述题名；另用 Crossref 书目检索 + OpenAlex 检索
  **未发现任何 2003 年 Labbé 一作的"最优孔径测光"论文**）。
  ⇒ **登记**：任务书里「Labbe et al. 2003 关于 SNR 最优孔径」这条**引用有误**；
  该主题的正确引用是 **Howell 1989 [F-01]** 与 **Naylor 1998 [F-02]**。

### [F-05] Bertin, E. & Arnouts, S. (1996). SExtractor: Software for source extraction. A&AS 117, 393–404. DOI 10.1051/aas:1996164 `[CR]`
- **通量定义**（**官方文档逐字原文，已核对**，SExtractor 2.24.2 docs）：
  `FLUX_APER`：> "FLUX_APER estimates the flux from the measurement image above the background inside a **circular aperture**. The diameter of the aperture in pixels is defined by the PHOTOM_APERTURES configuration parameter."
  `FLUX_AUTO`：> "FLUX_AUTO provides an estimate of the 'total flux' by integrating pixel values within an **adaptively scaled aperture**." ；> "FLUX_AUTO is the sum of pixel values … inside the **Kron ellipse**" ；> "**≥ 90% of the flux is expected to lie inside a circular aperture of radius k r_Kron with k = 2**"
  `MAG_ISOCOR`：> "Corrected isophotal magnitudes are now **deprecated**; they remain in SExtractor v2.x for compatibility with SExtractor v1." ；> "If one makes the assumption that the intensity profiles of faint objects … are roughly **Gaussian** …, then the fraction η = F_iso/F_tot …" ；> "MAG_ISOCOR = MAG_ISO + 2.5 log10 η."
- **孔径无关性 / 视宁度鲁棒性**：**三者都不是孔径无关的总通量**。
  `FLUX_ISOCOR` **部分**（改正依赖高斯假设 ⇒ 真实 PSF 翼更重时会偏），且**官方已标 deprecated**；
  `FLUX_AUTO` 半径自适应 ⇒ 比固定孔径稳，但**官方明确只含 ~90% 光**，仍随阈值/S/N 漂移；`FLUX_APER` **否**（用户指定直径）。
- **借鉴点**：**"等照度通量必须显式改正才是总通量"** 的权威表述 —— 这正是本项目不能用 `sdet` 等照度通量做标定的直接论据；
  也是"一族口径并存、必须写明用哪一个"的工程先例。
- **不借鉴点**：`FLUX_ISOCOR` 的**高斯**假设；本仓 `docs/science/PSF.md` 用的是 Moffat β=4（翼更重），
  若照搬高斯改正会系统性低估总通量（本工作项 exp5-C4 实测：高斯拟合偏 **+0.141 mag**）。
- **场景差异**：SExtractor 面向星系/多目标巡天星表；本项目是**单星点定标**，只要总通量。
- **核对状态**：**已核对**（`[CR]` 标识 + 官方文档 2.24.2 逐字原文 `[page]`）。

### [F-06] Kron, R. G. (1980). Photometry of a complete sample of faint galaxies. ApJS 43, 305. DOI 10.1086/190669 `[CR]`
- **通量定义**：Kron 半径 \(r_K = \sum r_i I_i / \sum I_i\)，通量取 \(r \lesssim k r_K\)（\(k\approx2.5\)）内的积分 —— 自适应孔径。
- **孔径无关性 / 视宁度鲁棒性**：**部分**。半径跟着轮廓走；但仍随 S/N、阈值、seeing 漂移。
  本工作项实测（`exp3`）：\(M_{\rm seeing}=0.091\) mag（比固定孔径好，比 PSF 总通量差 20×）。
- **借鉴点**："自适应半径"的思想；作为**对照口径**纳入实验（`est_kron`）。
- **不借鉴点**：Kron 是为**星系**（扩展源、Sérsic 轮廓）设计的；星点用 Kron 会引入额外噪声（实测低 S/N 散度 0.142 mag）。
- **场景差异**：星点 vs 星系。
- **核对状态**：**已核对**（`[CR]`）。

### [F-07] Moffat, A. F. J. (1969). A theoretical investigation of focal stellar images in the photographic emulsion and application to photographic photometry. A&A 3, 455. （无 DOI，前 DOI 时代；ADS bibcode 1969A&A.....3.....455M）`[ADS-scan]` `[OpenAlex]`
- **通量定义**：Moffat 轮廓 \(I(r) = A/(1+r^2/\alpha^2)^\beta\)；**总通量** \(= \pi\alpha^2 A/(\beta-1)\)（整平面解析积分）。
- **孔径无关性 / 视宁度鲁棒性**：轮廓**形状**随 seeing 变（\(\alpha\) 变），**归一** \(A\) 是总通量的标度 ⇒ 用总通量口径时与 seeing 无关。
- **借鉴点**：本项目 PSF 族的出处（`docs/science/PSF.md` 用 β=4）；解析总通量公式的来源。
- **不借鉴点**：1969 年**照相底片**的实测轮廓；现代 CCD/CMOS 的 PSF 翼指数常为 2.5–3.5 而非 4（见 exp5-C3 的 +0.025 mag 残差）。
- **场景差异**：底片 vs 硅探测器；但函数族沿用至今。
- **核对状态**：**已核对**（`[ADS-scan]` 扫描页 `1969A&A.....3..455M Page 455` HTTP 200；OpenAlex 题名/卷/页一致；无 DOI 属正常）。

---

# B PSF 测光（总通量口径）

### [F-08] Stetson, P. B. (1987). DAOPHOT: A computer program for crowded-field stellar photometry. PASP 99, 191–222. DOI 10.1086/131977 `[CR]`
- **通量定义**：**PSF 拟合总通量**。用解析 PSF（可选 + 数值残差查找表）与星心、背景**同时**最小二乘；
  输出的 `psfMag` 对应**模型的总归一**（不是任何孔径内的和）。DAOPHOT 另给 `PHOTOMETRY`（孔径）任务做对照。
- **孔径无关性 / 视宁度鲁棒性**：**是**。总通量 = 模型归一，定义里无半径；seeing 变只改形状参数。
- **借鉴点**：**D1 口径的原始出处**；"PSF 通量与孔径通量是两套口径、必须显式区分"的工程先例；
  "解析 PSF + 残差修正"的拟合结构（本仓 `dpsf` 的 Moffat4 拟合同构）。
- **不借鉴点**：拥挤场多星同时拟合的复杂度；本项目单星定标，不需要 GROUP/多星分解。
- **场景差异**：DAOPHOT 面向拥挤星团；本项目面向稀疏定标星，但**欠采样**更严重。
- **核对状态**：**已核对**（`[CR]` DOI 10.1086/131977 解析；题名大小写归一差异，无实质差异）。
  **注意**：其**全文未能抓取**（ADS 全文扫描被 bot 拦、IOPscience 验证码）⇒ 本条只核到**文章级**，未逐页核验公式号。
  **操作定义（已核对，IRAF 2.18 官方任务文档逐字原文）**：
  `iraf.readthedocs.io/.../daophot/allstar.html`：
  > "ALLSTAR computes x and y centers, sky values, and magnitudes for the stars in photfile by **fitting the PSF psfimage to groups of stars** in the IRAF image image."
  `.../daophot/daopars.html`：
  > "psfrad = 11.0 (scale units) The radius of the circle in scale units **within which the PSF model is defined**."
  > "fitrad = 3.0 (scale units) … Only pixels **within the fitting radius** of the center of a star will contribute to the fits computed by the PEAK, NSTAR and ALLSTAR tasks."
  ⇒ 拟合只用 `fitrad` 内的像素，而模型定义在 `psfrad` 上 ⇒ **输出是"被缩放的 PSF 模型的总通量"，不是任何孔径内的和**。
  **NOT-FOUND**：IRAF 页面**未**明说 PSF 模型归一到 PSF 星的孔径测光（该步为**推断，MEDIUM**）；DAOPHOT II 手册本体（`iraf.net` 403、多个 PDF 镜像 404）本轮取不到。

### [F-09] Stetson, P. B. (1993). Further Progress in CCD Photometry. IAU Colloquium 136, 291–303. DOI 10.1017/S0252921100007685 `[CR]` `[OpenAlex]`
- **通量定义**：DAOPHOT II / ALLSTAR 的 PSF 拟合测光（同 F-08 的总通量口径）。
- **孔径无关性 / 视宁度鲁棒性**：**是**（同 F-08）。
- **借鉴点**：ALLSTAR 的逐星 PSF 拟合实现范式。
- **不借鉴点**：同 F-08。
- **场景差异**：同 F-08。
- **核对状态**：**已核对，但候选引用元数据有误**（如实登记）：
  正确题名是 **"Further Progress in CCD Photometry"**（不是 "Progress in CCD Photometry"），
  年份 **1993**（不是 1992），出处 **IAU Colloquium 136（CUP）**，DOI 10.1017/S0252921100007685；
  **不是** ASP Conf. Ser. 25 —— ASP CS 25 是 *Astronomical Data Analysis Software and Systems I*（ADASS I, 1992）。
  页码 291 相符。

### [F-10] Bertin, E. (2011). Automated Morphometry with SExtractor and PSFEx. ASP Conf. Ser. 442 (ADASS XX), 435. **无 DOI；无 arXiv 版本** `[ADS-scan]` `[page]`
- **通量定义**：PSFEx 从图像自身**提取并建模**空间变化的 PSF（Moffat/Gaussian 等基函数），供 SExtractor 做 PSF 拟合测光。
- **孔径无关性 / 视宁度鲁棒性**：**是**（PSF 拟合总通量），前提是 PSF 模型在视场内可插值。
- **借鉴点**：**"PSF 形状必须从本帧自身测定、且允许空间变化"** 的权威先例 —— 与本项目"逐帧定 PSF"的必然选择一致。
- **不借鉴点**：PSFEx 需要**足够多的亮星**做多项式拟合；本项目单帧可用亮星数有限，且 M42 场拥挤 ⇒ 本工作项实测经验 PSF 被污染（FWHM 5.97 px vs 帧头 2.16 px）。
- **场景差异**：巡天宽场 vs 单帧定标。
- **核对状态**：**已核对**（`[ADS-scan]` PDF 实页 435 抓取 + 文本抽取；ASP 卷目录核对）。
  **订正**：候选里推测的 `arXiv:1109.xxxxx` **不存在**（arXiv API 检索无结果）⇒ **不得写 arXiv 号**。

### [F-11] Lang, D., Hogg, D. W. & Mykytyn, D. (2016). The Tractor: Probabilistic astronomical source detection and measurement. **ASCL 软件记录 ascl:1604.008**。 `[ASCL]` `[MIS]`
- **通量定义**：对每个源做**前向建模**（PSF/星系轮廓 + 背景），通量 = 模型归一（与 D1 同族）。
- **孔径无关性 / 视宁度鲁棒性**：**是**（模型归一）。
- **借鉴点**：前向建模 + 逐源似然的现代实现；"通量是模型参数而非像素和"的清晰表述。
- **不借鉴点**：贝叶斯后验采样/优化的复杂度；本项目不需要星系轮廓。
- **场景差异**：面向星系与多波段联合建模。
- **核对状态**：**候选引用被证伪，已订正**：
  正确标识是 **ASCL 软件记录 `ascl:1604.008`**（ASCL 页 HTTP 200，作者 Lang, Hogg, Mykytyn, 2016），**不是期刊论文**。
  候选给的 **"ApJ 821, 11" 是另一篇论文**（Lohfink et al. 2016, DOI 10.3847/0004-637X/821/1/11, "The Rhythm of Fairall 9"）；
  候选给的 **arXiv:1506.00837 亦无关**（Dev, hep-ph, TeV leptogenesis）。
  ⇒ 引用时**只能**写 ASCL 记录，不得写 ApJ/arXiv。

### [F-12] Anderson, J. & King, I. R. (2000). Toward High-Precision Astrometry with WFPC2. I. Deriving an Accurate Point-Spread Function. PASP 112, 1360. DOI 10.1086/316632 ；(2003) PASP 115, 113. DOI 10.1086/345491 `[CR]`
- **通量定义**：以**有效 PSF（ePSF）**——在欠采样条件下由星像**逐像素重建**的经验 PSF——做拟合测光/天测。
- **孔径无关性 / 视宁度鲁棒性**：**是**（拟合归一）；ePSF 天生处理**欠采样**（本项目 9 μm/1917.6 mm ⇒ 约 0.97″/px，明显欠采样）。
- **借鉴点**：**欠采样下的 PSF 建模**是 HST 路线最可借鉴的部分；本项目 PSF 采样率与 HST/ACS 同量级。
- **不借鉴点**：需要**大量**星像与迭代对齐；单帧定标星数不足以重建 ePSF。
- **场景差异**：空间望远镜 PSF 稳定；本项目地基、seeing 时变。
- **核对状态**：**部分核对 / 候选引用证伪**：
  任务书候选「Anderson & King 2006, PASP 118, 560」**未核到** —— PASP 118, 560 实为 Branch et al. 2006（超新星光谱，DOI 10.1086/502778）；
  ADASS XV（ASP CS 351）完整目录中无此文；2005 HST Calibration Workshop 论文集中只有 Anderson **单作者**的 "Empirical PSFs and Distortion in the WFC Camera"。
  ⇒ 上列 **2000/2003 两篇为已核对（`[CR]`）的替代引用**；广为流传的 "Anderson & King 2006, ACS ISR 2006-01" **本轮未核到权威副本，标 `[UNVERIFIED]`，不得引用**。

---

# C 主流巡天 / 管线怎么做

> 本节"标识"由 `[CR]`/`[page]` 核对；"操作定义"部分见各条的核对状态。

### [F-13] SDSS：Stoughton, C. et al. (2002). Sloan Digital Sky Survey: Early Data Release. AJ 123, 485–548. DOI 10.1086/324741 `[CR]` ；Lupton, R. et al. (2001). The SDSS Imaging Pipelines. ASP Conf. Ser. 238 (ADASS X), 269. arXiv:astro-ph/0101420 `[arXiv]` `[ADS-scan]` ；Alam, S. et al. (2015). The Eleventh and Twelfth Data Releases of the SDSS. ApJS 219, 12. DOI 10.1088/0067-0049/219/1/12 `[CR]` `[arXiv]`
- **通量定义**（**官方文档逐字原文，已核对**，`https://www.sdss4.org/dr17/algorithms/magnitudes/`，DR17）：
  > "For isolated stars, which are well-described by the point spread function (PSF), **the optimal measure of the total flux is determined by fitting a PSF model to the object**."
  > "we do this by sync-shifting the image of a star so that it is exactly centered on a pixel, and then fitting a Gaussian model of the PSF to it"
  > "the difference between the two is then **a local aperture correction**, which gives a corrected PSF magnitude"
  > "Finally, we use bright stars to determine **a further aperture correction to a radius of 7.4'' as a function of seeing**, and apply this to each frame based on its seeing."
  > "The resulting magnitude is stored in the quantity `psfMag`."
  `fiberMag`（原文）："Fiber magnitudes reflect the flux contained within the aperture of a spectroscopic fiber in each band. In the case of fiberMag we assume an aperture appropriate to the SDSS spectrograph (**3'' in diameter**)."
  单位（原文）："In SDSS-III/IV, we express all fluxes in terms of **nanomaggies**…"；`m = [22.5 mag] - 2.5 log10 f`；"a nanomaggy is approximately 3.631×10-6 Jy"。
  帧像素单位（原文，`/dr17/imaging/images/`）："The pixel values in the output images are given in **counts**, not calibrated quantities."；
  转换因子（`/dr17/algorithms/fluxcal/`）："**NMGYPERCOUNT**"。
  **`aa` 单位**：**NOT-FOUND**（DR17 magnitudes/fluxcal/images 三页均未定义该记号）。
- **孔径无关性 / 视宁度鲁棒性**：`psfMag` **是**（模型归一）；`fiberMag` 的孔径改正是**显式**的、依赖 PSF 增长曲线。
- **借鉴点**：**"标定用 PSF 总通量、孔径改正只在需要时显式施加"** 的权威工程范式 —— 与 `docs/science/PHOTOMETRY.md:95` 的冻结口径完全一致。
- **不借鉴点**：SDSS 有**专用定标数据**（PT 星、dome flat、star flat）与多年重复观测；本项目只有单帧 + Gaia。
- **场景差异**：SDSS 2.5 m 专用巡天望远镜、1.3″ seeing 量级、`psfMag` 有完整的孔径改正场（`apCorr` 类产品）。
- **核对状态**：**标识已核对**（三篇均 `[CR]`/`[arXiv]`/`[ADS-scan]` HTTP 200）。
  **订正**：`arXiv:astro-ph/0101420` 的 abs 页 "Journal reference" 字段写作 `ASP Conf.Ser. 10 (2001) 269` —— **该字符串是错的**（应为 **238**），**不得照抄 arXiv 的 journal-ref**。
  **操作定义（psfMag/fiberMag/孔径改正）的逐页原文**：见 §C 补充核对（官方 DR 文档）。

### [F-14] Morganson, E. et al. (2018). The Dark Energy Survey Image Processing Pipeline. PASP 130, 074501. DOI 10.1088/1538-3873/aab4ef `[CR]` `[arXiv:1801.03177]`
- **通量定义**（**部分核对**）：**已核到**（VizieR ReadMe，DES **DR1** 目录 II/357）：
  > "gmagPSF … Weighted average g-band (AB) magnitude, **of PSF fit single epoch detections**"
  > "gFluxPSF … Weighted averaged g-band flux, of PSF fit single epoch detections in **ADU units**"
  同目录另有 SExtractor 式 `AUTO` 量（如 "iFlux uncertainty (FLUXERR AUTO I)"）。
  ⇒ `MAG_PSF` 是 **PSF 拟合星等**（非孔径星等）；**是否施加孔径改正、定标用哪个通量：NOT-FOUND**。
- **孔径无关性 / 视宁度鲁棒性**：**测量**是 PSF 拟合（孔径无关）；**改正状态待核对**。
- **借鉴点**：`MAG_PSF` 作为标定通量的先例（"PSF fit single epoch detections"）。
- **不借鉴点**：DECam 有专门的定标产品与星 flat；本项目的平场/响应误差只能从帧本身拟合。
- **场景差异**：宽场拼接焦平面、每 CCD 独立 PSF。
- **核对状态**：**标识已核对**（`[CR]`+`[arXiv]`）；**操作定义部分核对**。
  **NOT-FOUND 登记**：DES DM 官方文档（`des.ncsa.illinois.edu/releases/dr2/dr2-docs`）是**客户端 Polymer 应用**，正文抓不到（只返回 `<des-public-main></des-public-main>` JS 壳）；
  Morganson+2018 等经 ar5iv 抓取时**在 PSF 测光/列定义章节前被截断**（fetch 上限 100k 字符）。
  ⇒ **不得**声称"DES 对 MAG_PSF 施加了孔径改正"——**待核对**。

### [F-15] LSST / Rubin DM：`PsfFlux` 与 `apCorr`。官方文档 `https://pipelines.lsst.io/api/lsst.meas.base.PsfFluxAlgorithm.html`、`.../ApplyApCorrTask.html`、`.../modules/lsst.meas.base/index.html`（版本 v30_0_11 / Current 2026-08-06）`[page]` ；谱系论文 Bosch, J. et al. (2018). The Hyper Suprime-Cam software pipeline. PASJ 70, S5. DOI 10.1093/pasj/psx080 `[CR]` `[arXiv:1705.06766]`
- **通量定义**（**源码逐字原文，已核对**，`lsst/meas_base@main`）：
  `include/lsst/meas/base/PsfFlux.h`：
  > "A measurement algorithm that estimates instFlux using **a linear least-squares fit with the Psf model**"
  > "we do a least-squares fit of the Psf model (evaluated at a given position) to the data. **For point sources, this provides the optimal instFlux measurement in the limit where the Psf model is correct.**"
  `python/lsst/meas/base/plugins.py`（登记为**需要孔径改正**）：
  > `wrapSimpleAlgorithm(PsfFluxAlgorithm, …, shouldApCorr=True, hasLogName=True)`
  `python/lsst/meas/algorithms/measureApCorr.py`：`refFluxName` doc = "Field name prefix for the flux other measurements should be aperture corrected to match"，默认 `"slot_CalibFlux"`。
  `python/lsst/meas/base/baseMeasurement.py`：`calibFlux = Field(… default="base_CircularApertureFlux_12_0" …, doc="the name of the instFlux measurement algorithm used for calibration")`。
  `src/ApertureFlux.cc`：`static std::array defaultRadii = {{3.0, 4.5, 6.0, 9.0, 12.0, 17.0, 25.0, 35.0, 50.0, 70.0}};` 注释 `// defaults here stolen from HSC pipeline defaults`。
- **孔径无关性 / 视宁度鲁棒性**：**测量本身是**（纯模型最小二乘拟合，无半径）；**但存下来/定标用的通量不是**——
  `shouldApCorr=True` ⇒ 由 `ApCorrMap` 改正到参考通量槽 `slot_CalibFlux`（默认 `base_CircularApertureFlux_12_0`，**圆孔径**）。即"孔径统一"而非"孔径无关"。
- **借鉴点**：**"PSF 通量 + 显式孔径改正场"是现代管线的标准组合**；`apCorr` 用增长曲线**逐帧重建**这一做法，
  与本项目"逐帧 PSF 决定增长曲线"同构。文档原文（`ApplyApCorrTask`）：
  > "class lsst.meas.base.ApplyApCorrTask … Bases: Task — Apply aperture corrections. … run(catalog, apCorrMap): Apply aperture corrections to a catalog of sources."
- **不借鉴点**：LSST 的 `apCorr` 是**空间变化的多项式/插值场**，需要全帧大量亮星；本项目单帧可用亮星数不足。
- **场景差异**：LSST 有专用定标曝光与全局 PSF 模型（`FinalPsf`）。
- **核对状态**：**已核对**（`pipelines.lsst.io` 多页 HTTP 200 + `lsst/meas_base`、`lsst/meas_algorithms` 源码原文；Bosch 2018 `[CR]`+`[arXiv]`）。
  **NOT-FOUND 登记**：LSST 文档里**没有**任何一句字面写"corrected to a uniform/infinite aperture"（该措辞未核到）；
  上面那条链是**源码级证据**，不是文档原话。另 `ApertureFlux.h` 写 radii 单位是 "in pixels"，而 LSST/HSC 惯例常为 arcsec ⇒ `12_0` 的单位**存疑**。
  **注意**：**不存在**一篇"定义 PsfFlux/apCorr"的同行评审 LSST DM 论文 ⇒ 引用时**必须引官方文档 URL + 版本**，不得伪引论文。

### [F-16] Pan-STARRS1：Waters, C. Z. et al. (2020). Pan-STARRS Pixel Processing: Detrending, Warping, Stacking. ApJS 251, 4. DOI 10.3847/1538-4365/abb82b `[CR]` ；Magnier, E. A. et al. (2020). Pan-STARRS Photometric and Astrometric Calibration. ApJS 251, 6. DOI 10.3847/1538-4365/abb82a `[CR]`
- **通量定义**（**逐字原文，已核对**，Magnier et al., "Pan-STARRS Pixel Analysis: Source Detection and Characterization", arXiv:1612.05244 实页）：
  > "Aperture corrections — Measure the **curve-of-growth**, spatial aperture variations, and background-error corrections."
  Table 1 行：`"Linear PSF Fits Y Y Y Y IV.7 All"`、`"Aperture Corrections Y Y Y N IV.9 All"`
  > "DAOPhot : Pixel-map PSF model with analytical component. pro: well-tested, high-quality photometry."
  ⇒ PSF 通量来自 **PSF 模型拟合**，另有**强制的孔径改正阶段**（curve-of-growth）。
- **孔径无关性 / 视宁度鲁棒性**：**是**（+ 显式 apcorr）。
- **借鉴点**：**"PSF 通量 + apcorr"在大规模巡天中的可扩展实现**；`psphot` 对**饱和/边缘**的显式处理。
- **不借鉴点**：PS1 有 3π 多次覆盖与专用定标场；本项目单帧。
- **场景差异**：1.8 m、宽场、GPC1。
- **核对状态**：**标识已核对**（`[CR]` 两篇 HTTP 200）；**操作定义部分核对（MEDIUM）**。
  **待核对登记**：上述原文取自 **arXiv:1612.05244**；该 arXiv 号对应的 ApJS 卷/文章号本轮**未独立核对**。
  而 `[CR]` 核到 **ApJS 251, 6 = "Pan-STARRS Photometric and Astrometric Calibration"（Magnier et al., DOI 10.3847/1538-4365/abb82a）**，
  与 "Pan-STARRS Pixel Analysis…" **题名不同** ⇒ 两者关系**待核对**，引用时请分别注明来源。
  另：fetch 在 §IV.9 正文前被截断，且 `github.com/ippm/ipp`（psphot 源码）**公开不存在（404）**。

### [F-17] HSC：Bosch, J. et al. (2018). PASJ 70, S5. DOI 10.1093/pasj/psx080 `[CR]` ；Aihara, H. et al. (2018). The Hyper Suprime-Cam SSP Survey: Overview and survey design. PASJ 70, S4. DOI 10.1093/pasj/psx066 `[CR]`
- **通量定义**（**逐字原文，已核对**，Bosch et al. 2018, arXiv:1705.06766，CCD 处理 step 13）：
  > "We estimate aperture corrections (Section 4.9.2) for each photometry algorithm by modeling the spatial variation of the ratio of each algorithm's flux to **the flux we use for photometric calibration, by default a 4'' diameter circular aperture**."
  > "The aperture corrections are determined using **the same sample of stars used to construct the PSF model**, and then applied to the measurements for all sources."
  （step 18）> "…and apply the aperture corrections measured in step 13."
  ⇒ **测光标定的参考通量本身就是 4″ 直径圆孔径通量**；`PsfFlux`/`CModelFlux` 是**被改正到该参考孔径**的模型通量。
- **孔径无关性 / 视宁度鲁棒性**：**是**（PSF 通量 + apcorr）。
- **借鉴点**：**同一份代码（LSST 谱系）在真实巡天上的验证**；`CModelFlux` 的星系/星点分流策略。
- **不借鉴点**：HSC 的 `CModel` 对星点会退化到 PSF；本项目全是星点，直接用 `PsfFlux` 即可。
- **场景差异**：8.2 m、0.17″/px（**良好采样**），与本品 0.97″/px（**欠采样**）相反 —— 这是本项目必须自己测 PSF 而不能照搬其参数的原因。
- **核对状态**：**标识已核对**（`[CR]`；注意是 article number S5/S4 而非页码）；**操作定义已核对（HIGH，step 13/18 原文）**。
  **NOT-FOUND**：`CModelFlux`/`PsfFlux` 的**逐条定义句**未取到（ar5iv 截断）；HSC schema/列文档是 JS 应用，抓不到文本。
  ⇒ "CModelFlux 也被改正到 4″" 属**推断（MEDIUM）**，不是原话。

---

# D Gaia XP 与合成测光（与 \(F_{\rm syn}\) 做比值的特殊要求）

### [F-18] Riello, M. et al. (2021). Gaia Early Data Release 3: Photometric content and validation. A&A 649, A3. DOI 10.1051/0004-6361/202039587 `[CR]` `[arXiv:2012.01916]`
- **通量定义**（**逐字原文，已核对**，Gaia DR3 文档 / Riello et al. 2021）：
  > "The key input used by the photometric and low-resolution spectra processing system PhotPipe for the measurement of G-band fluxes are the results of the **Image Parameter Determination (IPD)** process performed by the Intermediate Data Update (IDU) system."
  > "The modelling of the window contents is a complex process involving many calibrations, from the electronic bias through to the **point-spread function (PSF, for 2D windows) or line-spread function (LSF, for 1D windows)**."
  ⇒ Gaia \(G\) 是 **IPD 窗口/PSF(LSF) 模型拟合**的结果，**既不是孔径通量，也不是简单的 PSF 模型总通量**。
- **孔径无关性 / 视宁度鲁棒性**：Gaia 是**空间**观测（无大气 seeing），其 PSF 稳定；该文给出的通带定义是**合成测光**的基准。
- **借鉴点**：**\(G\) 通带定义 + 零点约定**的权威出处；\(F_{\rm syn}\) 与 \(G\) 的关系需要它。
- **不借鉴点**：其图像域处理（窗口门控、扫描）与本项目完全不同；**不得**把 Gaia \(G\) 当作"孔径测光"。
- **场景差异**：空间扫描 vs 地基凝视。
- **核对状态**：**已核对**（`[CR]` + `[arXiv]` + Gaia DR3 文档逐字原文 `[page]`）。
  **NOT-FOUND**：Gaia 官方文档 / GaiaXPy 文档里**没有**印出合成测光的显式积分式（含不含 λ 因子）⇒ **不得**为 λ 因子引 Gaia 论文，见 `[F-23]` 与 `[F-36]`。

### [F-19] Jordi, C. et al. (2010). Gaia broad band photometry. A&A 523, A48. DOI 10.1051/0004-6361/201015441 `[CR]`
- **通量定义**：Gaia 名义通带 \(G, G_{\rm BP}, G_{\rm RP}\) 的**响应曲线定义**（含望远镜+光学+探测器 QE）。
- **孔径无关性 / 视宁度鲁棒性**：不适用（通带定义）。
- **借鉴点**：**通带必须由"滤光片 × QE"合成**这一约定的出处 —— 与本仓 \(F_{\rm syn}=\int F_\lambda T Q \lambda\,d\lambda\) 的形式一致。
- **不借鉴点**：名义通带是**发射前**模型；实际在轨响应有差异（Riello 2021 才给在轨标定）。
- **场景差异**：本项目用自己的滤光片 + 自己相机的 QE（`filters_json`/`qe_json`）。
- **核对状态**：**已核对**（`[CR]`）。

### [F-20] Carrasco, J. M. et al. (2021). Internal calibration of Gaia BP/RP low-resolution spectra. A&A 652, A86. DOI 10.1051/0004-6361/202141249 `[CR]`
- **通量定义**：BP/RP 光谱的**内标定**（把原始像素→统一的光谱响应），是 XP 光谱可用于合成测光的前提。
- **孔径无关性 / 视宁度鲁棒性**：不适用（光谱域）。
- **借鉴点**：**XP 光谱不是"直接用"的，必须先经过内标定** —— 引用 XP 合成通量时必须引它。
- **不借鉴点**：Gaia 专用的定标流程。
- **场景差异**：空间扫描光谱仪。
- **核对状态**：**已核对**（`[CR]`）。

### [F-21] De Angeli, F. et al. (2023). Gaia Data Release 3: **Processing and validation of BP/RP low-resolution spectral data**. A&A 674, A2. DOI 10.1051/0004-6361/202243680 `[CR]` `[page]`
- **通量定义**：DR3 BP/RP 光谱的**处理与验证**（含波长定标、单位、质量标志）。
- **孔径无关性 / 视宁度鲁棒性**：不适用。
- **借鉴点**：XP 光谱**单位与质量标志**的权威出处（本项目消费 Gaia 星表数值时必须引它）。
- **不借鉴点**：Gaia 专用。
- **场景差异**：不适用。
- **核对状态**：**已核对，副标题需订正**：正确副标题是 **"Processing and validation of BP/RP low-resolution spectral data"**，
  **不是** "Processing of BP/RP spectra"。（作者/卷/文章号 A2 正确；DOI 10.1051/0004-6361/202243680。）

### [F-22] Montegriffo, P. et al. (2023). Gaia Data Release 3: **External calibration of BP/RP low-resolution spectroscopic data**. A&A 674, A3. DOI 10.1051/0004-6361/202243880 `[CR]` `[page]`
- **通量定义**：XP 光谱的**外标定**（把内标定后的光谱锚到绝对分光刻度 / CALSPEC 溯源），并发布 XP 光谱星表。
- **孔径无关性 / 视宁度鲁棒性**：不适用。
- **借鉴点**：**XP 光谱的绝对刻度来源**；"合成通量的绝对归一由外标定决定"的出处。
- **不借鉴点**：Gaia 专用。
- **场景差异**：不适用。
- **核对状态**：**已核对，副标题需订正**：正确副标题是 **"External calibration of BP/RP low-resolution spectroscopic data"**，
  **不是** "The spectrophotometric catalogue: XP spectra"。（作者/卷/文章号 A3 正确；DOI 10.1051/0004-6361/202243880。）

### [F-23] Bessell, M. & Murphy, S. (2012). Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI, Hipparcos, and Tycho Photometry. PASP 124, 140–157. DOI 10.1086/664083 `[CR]`
- **通量定义**：区分**能量型通带**与**光子型（photonic）通带**，给出 **pivot wavelength** 与零点约定。
- **孔径无关性 / 视宁度鲁棒性**：不适用（通带/零点约定）。
- **借鉴点**：**\(F_{\rm syn}\) 的被积函数必须含 \(\lambda\) 因子（光子计数）** 这一约定的权威出处；
  也是"不同约定会给出不同合成星等"的定量警示。
- **不借鉴点**：UBVRI/Hipparcos/Tycho 通带本身；本项目用自己的 Red 滤光片。
- **场景差异**：滤光片不同，但**约定**必须遵守。
- **核对状态**：**已核对**（`[CR]`）。

### [F-24] Sirianni, M. et al. (2005). The Photometric Performance and Calibration of the Hubble Space Telescope Advanced Camera for Surveys. PASP 117, 1049–1112. **DOI 10.1086/444553** `[CR]`
- **通量定义**：HST/ACS 的**测光定标**：孔径测光 + **孔径改正（encircled energy / growth curve）**、CTE 改正、AB 零点；
  明确区分**能量积分**与**光子计数**两种合成通量约定。
- **孔径无关性 / 视宁度鲁棒性**：ACS 的 PSF 稳定（空间），其**孔径改正表**是"用增长曲线把孔径通量改正到无穷孔径"的经典实例。
- **借鉴点**：**"孔径通量 + 显式增长曲线改正 = 总通量"** 的 HST 侧权威先例（与 F-13/F-15 的 `apCorr` 同构）；
  能量/光子约定的写法。
- **不借鉴点**：ACS 有**专用的 EE 表**（由星表/PSF 模型生成）；本项目必须**逐帧从本帧 PSF 生成**增长曲线。
- **场景差异**：空间（无 seeing）vs 地基（seeing 时变）—— 正是本项目不能用固定 EE 表的原因。
- **核对状态**：**已核对，候选 DOI 有误并已订正**：
  候选给的 **DOI 10.1086/496934 不是这篇**（它解析为 *Clinical Infectious Diseases* 41(9), 1369, 书评 "The Year in Infection, Volume 2"）；
  正确 DOI 是 **10.1086/444553**（PASP 117, 1049–1112, 2005）。

### [F-35] photutils 3.0.0 —— PSF 测光与增长曲线（官方文档）`https://photutils.readthedocs.io/en/stable/user_guide/psf.html`、`.../user_guide/curves_of_growth.html`、`.../reference/psf_api.html` `[page]`
- **通量定义**（**逐字原文**）：
  > "The two main PSF-photometry classes are **PSFPhotometry** and **IterativePSFPhotometry**."
  > "It must have parameters called x_0, y_0, and **flux, specifying the central position and total integrated flux**."
  > "The initial flux values for the fit are derived from measuring the flux in a circular aperture with radius aperture_radius."（**只是初值**）
  增长曲线：> "photutils.profiles provides tools to calculate radial profiles … and **curves of growth** (cumulative flux within concentric circular, square, or elliptical apertures)."
- **孔径无关性 / 视宁度鲁棒性**：**是**。拟合参数 `flux` 就是**总积分通量**（"total integrated flux"）⇒ 定义里没有孔径半径。
- **借鉴点**：**D1/D2 的现代参考实现语义**："PSF 测光返回的就是总通量"；增长曲线由 `CurveOfGrowth` 单独提供（与 `[F-32]` 的 `f_in(r)` 同构）。
- **不借鉴点**：**photutils 3.0 没有专用孔径改正 helper**（`psf_api.html` 无 `aperture_correction`；`psf.html`/`aperture.html`/`curves_of_growth.html` 正文中 "aperture correction" 不出现）⇒ 孔径改正要自己用 `CurveOfGrowth` 求比值。
  另：**`BasicPSFPhotometry` 已不在 3.0 API**（旧 API 已移除）。
- **场景差异**：通用库，不绑定任何巡天；本工作项**未安装 photutils**（`ModuleNotFoundError`）⇒ **未做数值交叉核对**（诚实登记）。
- **核对状态**：**已核对**（文档实页 HTTP 200，版本 3.0.0）。**"任何地方都没有 helper" 这一更强命题**只核到 MEDIUM（基于已查页面与 API 页）。

### [F-36] GaiaXPy 2.1.4 —— XP 合成测光的单位与曲线类型要求（官方文档）`https://gaiaxpy.readthedocs.io/en/latest/description.html`、`.../usage.html` `[page]`
- **通量定义 / 单位**（**逐字原文**）：
  > "The external or absolute system is … where the spectrum is defined in units of **W nm-1 m-2** on a scale of absolute wavelengths."
  > "internally calibrated spectra have units of **electrons per second per pixel sample**."
  > "The synthetic fluxes are given in units of **W nm-1 m-2 for photometric systems on VEGAMAG** and **W Hz-1 m-2 for systems in AB**."
- **孔径无关性 / 视宁度鲁棒性**：不适用（光谱域）。
- **借鉴点**：**"必须声明滤光片曲线是光子型还是能量型"** 的权威操作要求：
  > "it must be clearly specified if the transmission curves are **photonic curves or energy curves** (see, e.g., Bessell & Murphy 2012)."
  ⇒ 这正是 λ 因子进入 \(F_{\rm syn}\) 的位置；与本仓 `spectrum_integrator.cpp:266-274`（被积函数含 λ）一致。
- **不借鉴点**：GaiaXPy 是 XP 专用工具链；本项目自实现积分。
- **场景差异**：本项目消费的是**本仓 Gaia 客户端缓存的光谱数值**，不是在线 XP 产品。
- **核对状态**：**已核对**（文档实页 HTTP 200，版本 2.1.4）。
  **NOT-FOUND 登记**：**显式积分式**未在 GaiaXPy 文档或 Gaia DR3 文档 §5.5.1 中印出；
  GaiaXPy 指向的 "Gaia Collaboration, Montegriffo et al., *Synthetic photometry from Gaia low-resolution spectra*`（arXiv:2206.06215）**本轮未取到正文** ⇒ `[UNVERIFIED]`，**不得引用其公式**。

---

# E 合成注入 / 人工星测试（方法论）

### [F-25] Stetson, P. B. (1998). DAOPHOT II User's Manual（Dominion Astrophysical Observatory / HIA），§XX **ADDSTAR**。`http://www.star.bris.ac.uk/~mbt/daophot/mud9.ps` `[page]`
- **通量定义**：ADDSTAR 把**合成星**按用户给定（或随机）的**位置与星等**加进图像，随后由 FIND/PHOTOMETRY 等常规流程找回，
  用"输入 vs 输出"比较来估计**星探测效率**与**测光精度**。
- **孔径无关性 / 视宁度鲁棒性**：不适用（方法论）。
- **借鉴点**：**"注入已知真值 → 走完整管线 → 比较回收值"** 这一人工星测试范式的原始出处；
  也是本工作项 exp0–exp5 的方法论祖先。
- **不借鉴点**：ADDSTAR 手册**未规定噪声必须如何重抽**（它把星像**加**到已有像素上）；这正是负责人指出的
  「只是用加法增加做实验，那没有区别」的问题 —— **本项目必须显式重抽 Poisson 散粒与读出噪声**。
- **场景差异**：1980–90 年代 IRAF 环境；本项目用真实 L4 帧 + 电子域物理正向渲染。
- **核对状态**：**手册实页已核对**（`mud9.ps` HTTP 200，476,620 字节 PostScript，文本抽取到 §XX ADDSTAR 原文，
  逐字引文："This routine is used to add synthetic stars … the star-finding efficiency and the photometric accuracy can be estimated by comparing the output data for these stars to what was put in."）。
  **`[UNVERIFIED]`**：候选的「Stetson 1987 §ADDSTAR」**未核到**（1987 全文抓取被拦）⇒ **不得**写 "Stetson 1987 §ADDSTAR"。

### [F-26] Suchyta, E. et al. (2016). No galaxy left behind: accurate measurements with the faintest objects in the Dark Energy Survey. MNRAS 457, 786–808. DOI 10.1093/mnras/stv2953 `[CR]`
- **通量定义**：**Balrog**——把合成源注入**真实 DES 图像**，再走完整 DM 管线，用以刻画测量偏差随星等/大小的演化。
- **孔径无关性 / 视宁度鲁棒性**：不适用（方法论）。
- **借鉴点**：**"真实图像作底 + 合成源注入 + 走生产管线"** 的现代同行评审先例，正是本工作项的方法论。
- **不借鉴点**：Balrog 注入的是**星系**、关心剪切/光度红移偏差；本工作项注入**星点**、关心通量口径。
- **场景差异**：DES 宽场；本项目单帧定标。
- **核对状态**：**已核对**（`[CR]` DOI 解析，题名/卷/页一致）。

### [F-27] Korytov, D. et al. (2021). The LSST DESC DC2 Simulated Sky Survey. ApJS 253, 31. DOI 10.3847/1538-4365/abd62c `[CR]`
- **通量定义**：全链路图像模拟（含 **Poisson 散粒**、读出噪声、宇宙线、亮星溢出等），产出带真值的模拟巡天。
- **孔径无关性 / 视耸度鲁棒性**：不适用。
- **借鉴点**：**"模拟必须包含物理噪声过程"** 的现代权威先例（与本工作项 §3.1 的噪声模型同构）。
- **不借鉴点**：DC2 是**全合成**（非真实帧作底）；本工作项按负责人令**优先真实帧作底**。
- **场景差异**：LSST 参数；本项目用自己的真实帧统计定标噪声。
- **核对状态**：**已核对**（`[CR]`）。

### [F-28] Dolphin, A. E. (2000). WFPC2 Stellar Photometry with HSTphot. PASP 112, 1383–1396. DOI 10.1086/316630 `[CR]`
- **通量定义**：HSTphot 的 PSF 拟合测光；**人工星测试**用于标定完备度与测光偏差，并显式建模噪声。
- **孔径无关性 / 视宁度鲁棒性**：PSF 拟合总通量（是）；空间 PSF 稳定。
- **借鉴点**：**"人工星测试 + 显式噪声模型"** 在 HST 数据上的经典实现。
- **不借鉴点**：WFPC2 专用（CTE、几何畸变）。
- **场景差异**：空间 vs 地基。
- **核对状态**：**已核对**（`[CR]`）。

### [F-29] Anderson, J., Sarajedini, A., Bedin, L. R. et al. (2008). The ACS Survey of Globular Clusters. V. Generating a Comprehensive Star Catalog for Each Cluster. AJ 135, 2055–2073. DOI 10.1088/0004-6256/135/6/2055 `[CR]`
- **通量定义**：ePSF 拟合测光 + **大规模人工星测试**（含完备度与偏差）。
- **孔径无关性 / 视宁度鲁棒性**：**是**（ePSF 归一）。
- **借鉴点**：人工星测试的规模与判据设计；**欠采样**下 ePSF 的构建。
- **不借鉴点**：球状星团拥挤场；本项目稀疏场。
- **场景差异**：HST/ACS vs 地基 CMOS。
- **核对状态**：**已核对**（`[CR]`）。

### [F-29b] 关于"Poisson 重抽"的诚实登记
- **未找到**任何一篇同行评审论文的**主题**就是"对注入源做 Poisson 重抽"。
  该做法存在于上列论文的**图像模拟 / 源注入**小节（F-26/F-27/F-28/F-29）以及 DAOPHOT II 手册（F-25）之中。
- ⇒ 报告里**不得**伪引一篇"Poisson resampling 论文"；应引 **F-25（人工星测试范式）+ F-27（含 Poisson 的全链路模拟）**，
  并**自己写出噪声模型**（本工作项 `docs/f-instr-canon.md §3.1`）。

---

# F 本仓权威与实现（`[repo]` 自核，最强证据）

### [F-30] AstroCS 冻结合同：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN，2026-08-23）`[repo]`
- **通量定义**（原文 `:14-17`）：
  > `F_instr` | 仪器通量 (ADU；e⁻ 需 gain，当前不可得) | 输入
  > `F_syn` | 合成通量 = `∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（Gaia 星表模型）
  > `r_i` | `log10(F_instr/F_syn)` dex
  > `delta_i` | `−2.5·log10(F_instr)−G_Gaia` mag
- **口径指定**（原文 `:95`，§9a）：
  > **PSF 参数**：星点通量来自 PSF 拟合域（PSF.md），饱和判据 `psf_status/SATURATED` 决定剔除（§4）。
  （`:96`）> **aperture/flux/background**：`F_instr` 为仪器通量（ADU·px 或 e⁻ 同尺度）；**无孔径背景扣除项**——背景已在 PSF/测光上游处理
- **孔径无关性 / 视宁度鲁棒性**：合同**要求**用 PSF 拟合域 ⇒ 规范层面已是孔径无关口径。
- **借鉴点**：**这是本工作项定案的规范依据**；\(F_{\rm syn}\) 的被积函数已含 \(\lambda\) 因子（光子计数），**符合** F-23 的约定。
- **不借鉴点**：`:69` 明示**模型通带不含光学系统透过率与大气消光**（未建模项）⇒ 跨帧（airmass 不同）会成为帧间系统差，必须靠帧间一致性判据卡住。
- **场景差异**：不适用（本仓自身合同）。
- **核对状态**：**已核对**（直接读 `docs/science/PHOTOMETRY.md:14-17,37,69,85,95,96,100,107`）。

### [F-31] AstroCS 冻结合同：`docs/science/PSF.md`（SCI-PSF-001）`[repo]`
- **通量定义**（原文 `:21`、`:52`、`:87`）：
  > `flux` | 解析通量 `2πA·sxsy/3` (β=4) | `dpsf_psf.cpp:368`
  > `flux = 2πA·sxsy/3`   (整平面延伸假设)
  > **aperture/flux/background**：解析通量 `flux=2πA·sxsy/3`，单位 **ADU**（`I,B` 为 ADU/pixel，对探测器平面二维积分后为 ADU；β=4 整平面延伸假设，Project-defined 推导）；背景 `B` 模型内联合拟合，**无独立孔径 annulus**。
- **孔径无关性 / 视宁度鲁棒性**：**天生孔径无关**（整平面解析积分，无半径参数）。
- **借鉴点**：**D1 口径的公式与单位约定**；"背景在模型内联合拟合、不设 annulus" 已冻结。
- **不借鉴点**：β=4 是 **Project-defined**（`:118` 明示"不引用外部公式号"）；本工作项 exp5 实测 β=4 对 β=3.5 真值的残差 **+0.025 mag**（常数项，可被 \(k_{\rm photo}\) 吸收）。
- **场景差异**：不适用。
- **核对状态**：**已核对**（直接读 `docs/science/PSF.md:21,52,64,87,98,106,118,126`）。

### [F-32] 本仓**已存在**的 PSF 总通量与解析增长曲线 `[repo]`
- **通量定义**：
  - `lib/algorithms/psf/src/dpsf_psf.cpp:428` 原文：
    > `// Moffat4 (beta=4) 解析积分: flux = 2 * pi * A * sx * sy / (beta - 1) = 2 * pi * A * sx * sy / 3`
  - `lib/algorithms/psf/include/dynamic_psf.h:29` 结构体字段 `double flux;`；`:182` 冻结 9 列布局 `(status,B,flux,cx,cy,fwhm,A,mad,eccentricity)`。
  - `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:118` 原文：
    > `//   f_in(r) = 1 - (1 + r^2/(2 sigma^2))^-3      (Moffat4 beta=4 解析 enclosed fraction)`
  - `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:110-118` 已实现 **Horne 1986 最优提取**：
    > `Var(F) = 1 / sum_i (P_i^2/sigma_i^2) ; SNR_F = F / sqrt(Var(F))`
  - `lib/algorithms/noise_snr/cpp/include/snr_estimator.h:279`：`aperture_correction = 1/f_in(r)`（**孔径改正已在库内**）。
- **孔径无关性 / 视宁度鲁棒性**：`flux` 是整平面解析积分 ⇒ 孔径无关；`f_in(r)` 是 seeing 相关的增长曲线 ⇒ 可用于**显式**孔径改正。
- **借鉴点**：**D1 与 D2 所需的一切都已在本仓内实现** ⇒ A6 的改动面很小（见定案 §5）。
- **不借鉴点**：这些量目前**只服务 SNR 模块**，未进入测光标定路径。
- **场景差异**：不适用。
- **核对状态**：**已核对**（逐行读源码）。

### [F-33] 本仓**当前生产**的 \(F_{\rm instr}\) 实际口径（**病灶**）`[repo]`
- **通量定义**：
  - **测光标定路径**：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:139-151` ——
    `m00 = Σ_{5×5} (I − background)` 且 `if (v <= 0) continue;`（**5×5 固定盒 + 正性截断**），`s.flux = m00`；
    `:143` 边缘仅置 `quality |= 2`；`:167` `peak > 50000.0` 置饱和位。
  - **等照度口径**：`lib/algorithms/star_detection/src/sdet_detector.cpp:281-294` ——
    `flux = Σ_{CC}(I − bkg)` 且 `if (val > 0.0f) flux += val;`（连通域 = 阈上区域）。**该路径只被 `wcs-platesolve` 使用**（`module_adapters.cpp:2016` 注释）。
  - **孔径口径**：`lib/algorithms/photometry/wrapper_phase1/photometer.h:26` `Photometer(aperture_radius_px = 4.0, sky_annulus_inner = 6.0, sky_annulus_outer = 10.0)`；输出 `p1_flux.json`。
  - **喂给拟合的量**：`lib/infrastructure/scheduler/src/module_adapters.cpp:3242` `pfl.push_back(srcs[s].value("flux", 0.0));` —— 即上面 `m00`；
    经 `:3288` `freq.psf_flux = pfl.data();` → `star_matcher.cpp:318` `m.f_instr = psf_flux[j];` → `:589` `double scale = std::pow(10.0, -location);`。
- **孔径无关性 / 视宁度鲁棒性**：**否**。本工作项实测：\(M_{\rm seeing} = 1.353\) mag（seeing 1.5→5.0 px），
  seeing 2.0→4.0 px 的**假增益 0.516×**；且该量**随孔径剧烈变化**（0.493/0.672/0.808/0.925/0.985）。
- **借鉴点**：无（这是被定案替换的对象）。
- **不借鉴点**：**全部**（固定盒和、正性截断、等照度阈值、未改正小孔径）。
- **场景差异**：不适用。
- **核对状态**：**已核对**（逐行读源码；`p1_sources.json`/`p1_psf.json` 的写出位置亦核对）。
  ⇒ **与 [F-30] `:95` 的冻结口径冲突**：合同要求 PSF 拟合域，实现给的是检测器 5×5 盒和。

### [F-34] 本仓 \(F_{\rm syn}\) 积分与匹配链 `[repo]`
- **通量定义**：`lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:266-274` 原文：
  > `// 被积函数: S(λ)·T(λ)·Q(λ)·λ (无 QE 时 Q=1.0)`
  > `integrand[i] = s_grid[i] * t_grid[i] * q * grid[i];`
  网格步长 1.0 nm（`:247`），Akima 插值（`:259-264`），Simpson 积分（`:274`）。
- **孔径无关性 / 视宁度鲁棒性**：不适用（光谱域）。
- **借鉴点**：**已含光子计数 \(\lambda\) 因子，符合 [F-23] 的约定** ⇒ **无需改动**。
- **不借鉴点 / 风险**：`:230` 在无 QE 曲线时只 `LOG_INFO`（`compute_f_syn: QE curve not provided, F_syn without Q(λ)`）
  ⇒ 颜色项缺失会引入**星色相关**的系统差，建议升级为显式降级标志。
- **场景差异**：不适用。
- **核对状态**：**已核对**（逐行读源码）。
