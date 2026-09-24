# CCD/CMOS 线性缺陷（坏列 / 坏行 / 拖尾列）检测与修复：一手文献证据

> 编制说明：每条给出**编者真的打开读过**的原文位置（节号 / 页码 / 式号）+ 英文短引。
> 标注约定：**[已核验]** = 直接打开原文并摘录；**[二手转引]** = 仅从另一份已读文献的参考文献表得到书目；**[未核验]** = 见末节说明。
> 工作缓存：`run/lit/cache/`；抓取脚本：`run/lit/{batch,litfetch,h2t,search}.py`。

---

## 0. 一句话结论（供报告直接引用）

坏列、CTE 拖尾、宇宙线/卫星线在三类一手文档里有**互不重叠的判据**：
坏列由**跨帧稳定的偏置结构**定义（ACS DQ 128 "Bias structure (e.g., bad columns)"）；坏列插值后"**方差被低估**"有**显式解析式**（van Dokkum & Pasha 2024 §6：`σ_mask = σ_org (2d+1)^(−0.5)`，离掩膜边缘越远越小）与**官方文字**（SDSS `BAD_COUNTS_ERROR`："it is probably underestimated"；Caveats："noticeably correlated noise"），并有**显式方差赋值算法**（Desai et al. 2016 §4.2：PSF 核内高斯误差传播 + 以该 σ² 抽样复原）；
CTE 拖尾是**沿平行读出方向、只出现在源/热像素上游、长度随行号与背景变化**的电荷尾（ACS DHB §4.6.1、WFC3 DHB §6.3）；
卫星线/宇宙线是**跨帧不复现**的外源事件（Kruk et al. 2023 用 2002–2021 档案统计，逐帧率 2.7%）。

---

## 1. 坏列检测的统计方法

### 1.1 仪器手册的一手定义与阈值（最硬的证据）

- [STScI 2025/2026, ACS Data Handbook (ACS DHB), 在线版 §4.3.2](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-3-dark-current-hot-pixels-and-cosmic-rays) §4.3.2 —— **"热/温像素"的定量阈值（这是坏点/坏列谱系里唯一给出数值判据的一手手册条款）**：
  "A pixel above 0.14 e¯/pixel/second is considered a 'hot' pixel. A pixel below the hot pixel range but above 0.06 e¯/pixel/second is considered a 'warm' pixel."
  同节 Table 4.5 给出该阈值随任务历史的**变更记录**（发射–2004-10-08 用静态热像素表；2004-10-08 至 2007-01 温 0.02–0.08 / 热 ≥0.08；SM4–2015-01-15 温 0.04–0.08 / 热 ≥0.08；2015-01-15 至今 温 0.06–0.14 / 热 ≥0.14 e⁻/s），并注明 "there have been several changes to the definition of warm and hot pixels throughout the lifetime of ACS… The definition of 'warm' and 'hot' pixel is somewhat arbitrary"。
  同节 Table 4.4：WFC(−81 °C) 新增**稳定温像素 0.16 %/年、稳定热像素 0.10 %/年**；退火每次治愈约 1–4 % 热像素（HRC 约 14 %）。
  → **注意**：手册给出的是**暗电流绝对阈值**，不是"列中值偏离 k·σ"。这是本报告能找到的最接近"定量判据"的官方条款。

- [STScI 2025/2026, ACS DHB, 在线版 §3.4 "doDQI – Bad Pixel Determination" 与 Table 3.4](https://hst-docs.stsci.edu/acsdhb/chapter-3-acs-calibration-pipeline/3-4-calacs-processing-steps) §3.4 + Table 3.4 —— **坏列的官方 DQ 位定义与掩膜机制**：
  "The function doDQI initializes the Data Quality (DQ) array by combining it with a table of known permanent bad pixels for the detector, stored in the bad pixel reference table (named by the image header keyword BPIXTAB)."
  Table 3.4 逐位定义（PDF 版 p.87；在线版 Table 3.4）：
  `4 = Bad detector pixel or vignetted pixel`、`16 = Hot pixel (dark current > 0.14 e¯/sec)`、`32 = Pixels with unstable dark current; includes random telegraph signal noise and fading hot pixels`、`64 = Warm pixel (dark current: 0.06–0.14 e¯/sec)`、**`128 = Bias structure (e.g., bad columns)`**、`1024 = Sink pixel or pixel affected by sink pixel charge traps`、`4096 = Cosmic ray rejected by AstroDrizzle`、`8192 = Cosmic ray rejected by acsrej`、**`16384 = Reserved (satellite trail masks, etc.)`**。
  → 关键点：**坏列在 ACS 里是"偏置结构"缺陷（bias structure），来自 bias 参考帧的稳定结构**；手册**未给出**判定坏列的 k·σ 数值。

- [STScI 2025/2026, ACS DHB §4.5.6 "Satellite Trails"](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-5-image-anomalies) §4.5.6 —— 外源事件率与官方识别工具：
  "As of 2022, detectable satellites trails were estimated to cross the ACS WFC FOV at a rate of ~0.6 per hour, such that ~10% of full-frame ACS/WFC images will suffer contamination."
  官方两个算法：`acstools.detsat`（边缘检测 + Hough，ACS ISR 2016-01）与 `acstools.findsat_mrt`（Median Radon Transform，ACS ISR 2022-08），"Both programs can be used to flag identified trails in the input image with DQ flag 16384."

- [STScI 2026, WFC3 Data Handbook §6.3 "The Nature Of CTE Losses"](https://hst-docs.stsci.edu/wfc3dhb/chapter-6-wfc3-uvis-charge-transfer-efficiency-cte/6-3-the-nature-of-cte-losses) §6.3 —— 拖尾的**方向性与定量量级**（用于与坏列区分，见 §3）：
  "CTE takes charge away from downstream pixels and deposits it into upstream pixels. Visually, the effect results in 'trails' of charge that extend out from sources in the direction opposite the readout amplifier."
  定量："a charge packet that contains just one electron and is located far from the readout amplifier will encounter 40 traps on its ~2000-pixel journey to the serial register"；"Packets with 1000 electrons on zero background will lose about 300 electrons, so over 70% will survive"；"Packets with 10,000 electrons on an image with no background will lose only about 6% of their electrons."

### 1.2 期刊/会议：缺陷表征与坏列判据

- [Bosch et al. 2018, PASJ 70, S5 (HSC 软件管线), arXiv:1705.06766](https://arxiv.org/abs/1705.06766) §4.5 "Bad Pixel Interpolation"（PDF p.18）—— **把坏列明确当作"噪声无穷大"的像素来处理，这是 HSC 对坏列统计处置的一手表述**：
  "In order to apply this to interpolation, we set the noise to be infinitely larger in the bad columns than the good. Considering the case of infinite signal-to-noise ratio and restricting ourselves to only 5 terms (centered on the bad column)… the resulting weights for a one-dimensional interpolation with α = 1 are {−0.274, 0.774, 0.000, 0.774, −0.274}; the bad pixel has of course a weight of 0.000."
  同节：修复后 "Image mask bits are set to indicate both that a pixel was interpolated and the reason why (e.g. cosmic ray, sensor defect, saturation, etc.)"；并给出**可用性判据**："Generally only objects whose centers were interpolated (flags pixel interpolated center) should be considered to have unreliable measurements."
  同文 Table 2（PDF p.6）mask plane 定义：`BAD` = "Object overlaps a sensor defect."，`INTERPOLATED` = "Object overlaps a pixel that was set by interpolating its neighbors"。
  §4.4（PDF p.18）宇宙线判据：对 NS/EW/NW-SE/NE-SW 四对邻居做多条件检验，阈值 `c2 = 0.6`，宇宙线事件最小电荷 **150 e⁻**。

- [SDSS DR17, Algorithms: Image masks](https://www.sdss4.org/dr17/algorithms/masks/) §"BLEEDING and BRIGHT_STAR masks" —— 巡天级（非像素级）掩膜的来源与类型："The masks are based on the original masks contained in the fpM files generated by the frames pipeline."；类型 0=BLEEDING、1=BRIGHT_STAR、2=TRAIL、3=HOLE、4=SEEING。
- [Aihara et al. 2018, PASJ 70, S4, DOI 10.1093/pasj/psx081, arXiv:1702.08449](https://arxiv.org/abs/1702.08449)（PDF p.12、p.19）—— HSC 单帧处理的方差/掩膜同构生成，并提到 warping 引入 "covariances between the pixels"；坏点常因 "a cosmic ray or bad pixel column"。
- [Kruk et al. 2023, Nature Astronomy 7, 262–268, DOI 10.1038/s41550-023-01903-3](https://doi.org/10.1038/s41550-023-01903-3) Abstract + Main —— **HST 档案级卫星线统计（外源事件率）**：
  "We find that a fraction of 2.7% of the individual exposures with a typical exposure time of 11 minutes are crossed by satellites and that the fraction of satellite trails in the images increases with time."
  形态判据："In contrast to asteroid trails that appear as short, curved trails in the images due to the parallax effect caused by the motion of the spacecraft around the Earth, satellite trails traverse the entire field of view (FoV) of the HST observations quickly and, in most cases, appear as straight lines."

- [Tyson et al. 2020, AJ 160, 226, arXiv:2006.12417](https://arxiv.org/abs/2006.12417) —— Rubin/LSST 的卫星拖尾掩膜与亮度缓解；PDF 已下载但**本报告未逐段摘录**（条目级）。

### 1.3 教材与手册：IRAF 的"没有自动判据"本身是结论

- [Massey 1997, "A User's Guide to CCD Reductions with IRAF"（ccduser3, 15 Feb 1997），Wayback 存档 PostScript](http://web.archive.org/web/20060921164016id_/http://iraf.noao.edu/iraf/ftp/ftp/docs/ccduser3.ps.Z) §3.6「Constructing a bad pixel mask」（p.15）与 `ccdproc` 参数表（Fig. 6，p.13–14）—— **IRAF 手册级文档不提供任何自动坏列判据**：
  坏列由用户自建文件传入，参数串为 `(fixpix=no) Fix bad CCD lines and columns?` 与 `(fixfile=) File describing the bad lines and columns`；§3.6 把坏点定义为 "pixels—usually partial columns—that are nonlinear"，做法是用长/短平场序列（"several thousand e/pixel" 与 "100 e/pixel"）**人工**构造掩膜。
  同处出现的 σ 值是**过扫拟合的排异因子**、与坏列无关：`(low_rej=3.) Low sigma rejection factor`、`(high_re=3.) High sigma rejection factor`。
  ⚠️ **核验方式说明（引用时请保留）**：iraf.net 本站对该文档返回 **HTTP 403（Cloudflare 挑战）**，本条用的是 Wayback 存档的 PostScript（`.ps.Z`，2.4 MB，已 gunzip 为 8.0 MB）；**PostScript 正文文本层无法用 `pdftotext` 抽取**，因此上列参数串是从 PS 源码的**字面字符串**中提取并核对（`grep -a -o '(fixfile=\|fixpix\|low_rej=\|high_re=\|ccdproc' `），**未逐句读 §3.6 正文散文**。
- **结论（写入报告可直接引用）**：**"列中值偏离 k·σ 判坏列"这一判据，在可打开的手册级一手来源中不存在** —— IRAF 靠用户掩膜 + 固定 fixfile、HST 靠参考表（BPIXTAB）+ 暗电流绝对阈、THELI 主张事前标定 + 零权（见 §1.4）。真正成体系使用 k·σ / 多级 σ 的是**伪影（宇宙线）掩膜**，不是坏列判据（见 §1.4 Desai et al. 2016）。
- Howell, *Handbook of CCD Astronomy* (2nd ed., CUP 2006)：仅经 Crossref 取到**章节级 DOI**（例：[10.1017/cbo9780511807909.006](https://doi.org/10.1017/cbo9780511807909.006)，ISBN 9780521852159），Cambridge Core 目录/正文**未打开** ⇒ **不能给章节号**；标「未核验，仅 DOI 级」。
- Berry & Burnell, *The Handbook of Astronomical Image Processing* (Willmann-Bell 2005)：Wayback availability API 返回 429，未取得目录或公开样章 ⇒ **[未核验正文]**。
- IRAF 现行文档站（`iraf.net/irafhelp.php?val=ccdmask|badcols|cosmicrays|ccdproc`）：全站 **HTTP 403**；`stsdas.stsci.edu` gethelp 404 ⇒ `ccdmask`/`badcols` 类任务是否存在、其判据为何，**[未核验]**。

### 1.4 其他仪器手册与管线的坏列口径（阈值均为绝对量或上游标定）

- [WFC3 Instrument Handbook §5.4 "WFC3 CCD Characteristics and Performance"](https://hst-docs.stsci.edu/spaces/WFC3IHB/pages/148522498/5.4+WFC3+CCD+Characteristics+and+Performance) —— **逐条列出坏列位置（这是"坏列清单"级的一手证据）**：
  "The bad detector pixel population, generally located along columns, is relatively constant. Bad detector pixels are shown for the upper chip (chip 1) and lower chip (chip 2) in Figure 5.10 and Figure 5.11"；Figure 5.10 图注："Bad detector pixels (DQI = 4) on the upper chip (chip 1) are in **columns 1104, 2512, 2542, 2543, 2869, 3646, and 3667** in the flt image."；Figure 5.11 图注："…on the lower chip (chip 2) are in **columns 223, 242, 243, 751, 1417, 1469, 2655, 2696, 2707, and 3915** in the flt image."
  阈值只对**热像素**给出且为绝对量："the hot pixel threshold (defined as **54 e-/hr**)"；2020 年 6 月把 post-flash 由 12 e⁻ 提到 20 e⁻ 后 "a significant percentage of stable pixels being misflagged as unstable"，故 **2021 年 WFC3 团队把"热像素阈值"改成一个由公式决定的值**（引 WFC3 ISR 2020-08 / 2021-03 / 2021-06；ISR 本体未取到）。
- [WFC3 Data Handbook §3.2.3 "Data Quality Array Initialization"](https://hst-docs.stsci.edu/wfc3dhb/chapter-3-wfc3-data-calibration/3-2-uvis-data-calibration-steps) —— **BPIXTAB 只讲怎么用、不讲怎么生成**："This step initializes the data quality array by reading a table of known bad pixels for the detector, as stored in the Bad Pixel reference table, BPIXTAB."；DQ 4 = "bad detector pixel or beyond aperture"；"DQICORR combines the DQ flags from preprocessing, BPIXTAB, SNKCFILE, and saturation tests into a single result… combined using a bit-wise logical OR operation"。
- [STIS Data Handbook §2.5 "Error and Data Quality Array"](https://hst-docs.stsci.edu/spaces/STISDHB/pages/148155882/2.5+Error+and+Data+Quality+Array) —— 同族 flag 语义（**含"坏行"的唯一官方措辞**）：DQ = 4 "Bad detector pixel (e.g., **bad column or row**, mixed science and bias for overscan, or beyond aperture)."；DQ = 512 "Bad pixel in reference file."
- [Desai et al. 2016, Astronomy and Computing 16, 1（DES 单帧/叠加管线）, arXiv:1601.07182](https://arxiv.org/abs/1601.07182) §3.2（PDF p.5）—— **坏列识别被明确归为上游且定性**："bad columns are marked already prior to the masking pipeline, where the bad pixel map is created using **outliers in dome flats and bias corrects** to identify dead or hot columns"；被标像素 "the weight map values for pixels that have been flagged are **set to zero**"。
  同文 §3.2（PDF p.7）的 **σ 阈值属于伪影（宇宙线）掩膜、不是坏列判据**：第一遍 "classified as a candidate artifact if it has a value >5, corresponding to a **5σ**"；第二遍对已知伪影邻域 "deviations greater than **2.5σ**"；第三遍再扩 "deviations greater than **1.5σ**"。
- [Erben et al. 2005（THELI / GaBoDS IV）, AN 326, 432, arXiv:astro-ph/0501144](https://arxiv.org/abs/astro-ph/0501144) §4.6（arXiv PDF p.23）—— 主张事前标定 + 零权，且同样**不给列级判据**：
  "many image pixels carry non-Gaussian noise properties (such as **bad columns**, vignetted image regions, cosmics)"；"To perform a weighted mean co-addition all bad image pixels have to be known beforehand and **assigned a zero weight** in the co-addition process"；坏点 "are most effectively identified in the **masDARK** frames"（同文 §2，PDF p.7 定义 masDARK）。

---

## 2. 插值修复的偏差分析（方差被低估 / 相关样本数减少）

**核心命题的现状（已闭环）**：
- ✅ **有显式解析式，且方向正是"被低估"**：van Dokkum & Pasha (2024) §6（arXiv PDF p.12）给出填补像素的形式逐像素噪声 `σ_mask = σ_org (2d + 1)^(−0.5)` —— 离掩膜边缘越远噪声越小 ⇒ 按邻元赋值必然**低估**该处方差。见 §2.0。
- ✅ **有显式的方差赋值算法**：Desai et al. (2016) §4.2（PDF p.9）—— 用 PSF 核内未掩膜像元做**高斯误差传播**得到 `σ^−2(x,y)`，**再以该 σ² 为方差抽一个高斯随机数**作为填补值，使填补像素的逐像素起伏与周围未污染像素相当；并为每个插值像素置 BPM 位。见 §2.0。
- ✅ **"误差被低估"有官方文字**：SDSS DR17 Flags Detail 的 `BAD_COUNTS_ERROR`："you should not believe the PSF flux error; **it is probably underestimated**"；"**noticeably correlated noise**"见 Caveats。见 §2.2。
- ⚠️ 仍**没有**任何来源给出"坏列插值后**相关系数 ρ** 或**有效独立样本数 N_eff**"的定量表达式；也没有把上述 σ 公式与`k·σ`坏列检测串成完整误差预算的一手文献。

### 2.0 两条决定性证据（本轮新增，直接回答"方差该赋何值"）

1. [van Dokkum & Pasha 2024, PASP 136, 034503, DOI 10.1088/1538-3873/ad2866（"maskfill"）, arXiv:2312.03064](https://arxiv.org/abs/2312.03064) **§6 Conclusions**（arXiv PDF p.12；原文所在页眉为期刊版 p.12）—— **唯一给出解析式的一手来源**，逐字：
   "Even though the algorithm is well-defined it is difficult to assign an uncertainty to the filled-in values. **The formal per-pixel noise decreases with d, the distance to the edge of the mask, according to σ_mask = σ_org (2d + 1)^(−0.5)**,"（σ_org = "the per-pixel noise outside of the mask"）。
   → 该式说明：**以邻元噪声传播给填补像素赋值时，形式噪声随"到掩膜边缘的距离 d"衰减**，即越是掩膜内部、形式噪声越小 —— 这正是"插值把噪声降到邻列水平以下"的定量表述；作者自陈 "it is difficult to assign an uncertainty to the filled-in values"。
   同文 §6 另给算法自身的适用边界（引用时勿省略）："if the circular mask had been created to cover a defect, the reconstruction would have entirely missed the star at that location. Fortunately, **most detector defects and cosmic rays are on scales of a few pixels, where the code is reliable** and its behavior is predictable."
2. [Desai et al. 2016, Astronomy and Computing 16, 1, arXiv:1601.07182](https://arxiv.org/abs/1601.07182) **§4.2 "Interpolation over Non-imaged Artifacts"**（PDF p.9）—— **唯一给出完整"方差赋值 + 抽样复原"处置的一手来源**，逐字：
   "we calculate the associated inverse variance weight σ^−2(x, y) of the interpolated pixel **using Gaussian error propagation over the unweighted pixels within the PSF kernel**. Finally, we sample the expected observed value Ĩ of the pixel as **a Gaussian random deviate with the measured mean I and variance σ² so that the pixel to pixel variations of the interpolated pixels are comparable to those of the surrounding uncontaminated pixels**. For each interpolated pixel a bit is set in the BPM image."
   同文 §4.1（PDF p.9）给出适用域与代价："a non-imaged artifact– that is, an artifact like a cosmic ray or **a bad column** that has not been imaged through the telescope and camera optics– can be effectively replaced through interpolation with **only a modest increase in noise** over the case where the artifact was not present. This applies to all non-imaged artifacts that have widths that are small compared to the FWHM of the PSF."
   同文 §4.1 还给出**外源事件不可插值的判据**（与 §5.2 第 4 类呼应）："Imaged artifacts like satellite trails have sizes that are as large as or larger than the PSF, and therefore it is **not possible to interpolate over them without adopting a strong prior** on the underlying light distribution… the safest approach is to mask all these affected pixels, **set their weights to zero** and not allow them to contribute to the coadd."
   → **对命题的意义**：Desai et al. 的做法把"插值像素的方差"显式提高为**按误差传播算出的 σ² 并主动注入随机涨落**，而不是抄邻列方差；这从反面确认了"直接抄邻列方差会低估该处方差"。
3. 负面/对照结果（同为已读）：[LSST `afw` `Interpolate.h`（main）](https://raw.githubusercontent.com/lsst/afw/main/include/lsst/afw/math/Interpolate.h) —— 该通用插值器只有 `CONSTANT`/`LINEAR`/样条 Style 与 `makeInterpolate` 族，**全文 0 处 `sigma`/`variance`** ⇒ LSST 的 `afw` 插值层**不承担**插值像素方差赋值（赋值发生在 `ip_isr` 的 variance plane 层，见 §2.3）；该头文件只声明插值风格与工厂函数，未见任何方差/σ 接口（本次仅核验了该头文件本身，未核验 `afw` 其余实现）。

### 2.1 仪器/管线把坏列像素当作"噪声不可用"

1. **把坏像素的噪声设为无穷大，而不是抄邻列方差**
   [Bosch et al. 2018, PASJ 70, S5, arXiv:1705.06766](https://arxiv.org/abs/1705.06766) §4.5（PDF p.18）：
   "In order to apply this to interpolation, we set the noise to be infinitely larger in the bad columns than the good."
   → 该文对坏列像素给出的插值权重是 `{−0.274, 0.774, 0.000, 0.774, −0.274}`（坏列自身权重 0.000），并明确 "it is clear that the estimator is unbiased if Σ_j D_pj = 1"（式 4 下方）。这是"插值估计量无偏"与"该像素噪声信息不可用"两条论断的一手出处。

2. **插值像素被单独打标，且只有"中心被插值"的天体才被判定为不可靠**
   同上 §4.5：mask 位区分"被插值"与"插值原因"；"Generally only objects whose centers were interpolated (flags pixel interpolated center) should be considered to have unreliable measurements."
   [Aihara et al. 2018, PASJ 70, S4, DOI 10.1093/pasj/psx081, arXiv:1702.08449](https://arxiv.org/abs/1702.08449)（PDF p.9、p.12）："Variance and mask images are generated from a science image and are processed as with the science image."；"The saturated and interpolated flags come in two variants; any … and center … The latter can be used in most cases because the interpolation outside of the central region should be reasonable"。

3. **坏列与宇宙线在掩膜上会互相叠加成复杂形状（对修复算法本身的约束）**
   [van Dokkum & Pasha 2024, PASP 136, 034503, arXiv:2312.03064](https://arxiv.org/abs/2312.03064) §2.2（PDF p.3）：
   "As an example, bad columns typically intersect many cosmic rays, producing complex shapes."
   同文 §1（PDF p.1）：天文图像中 "missing or unwanted information, such as bad pixels, bad columns, cosmic rays, masked objects, or residuals from imperfect model subtraction"；其 `maskfill` 用 3×3 中值滤波迭代外推 + 最后对掩膜区内做 3×3 boxcar 平滑（§2.2）。

### 2.2 SDSS 的一手表述：误差被低估 + 相关噪声（本轮新增，最关键）

- [SDSS DR17, Algorithms: Understanding the Image Processing Flags – Details](https://www.sdss4.org/dr17/algorithms/flags_detail/) §"Other Pixel-level Problems" —— 逐字：
  "If the photometric pipeline recognizes a pixel as bad (due to a bad column, a cosmic ray, or a bleed trail), it is interpolated over. If this is true for any pixel within the object, it is flagged INTERP."
  "if the interpolation is over a cosmic ray or a single bad column, for example, the photometry should be essentially perfect."（**单列插值被官方认为基本无损**）
  "INTERP_CENTER means that the interpolated pixel(s) in question fell within 3 pixels of the center of the object. This is a warning that perhaps the photometry of this object may be affected."
  "PSF_FLUX_INTERP is set. This means that more than 20% of the PSF flux is interpolated over in the band in question … In practice, most objects with this flag set still appear to have perfectly good PSF photometry, but the number of outliers (say, in a color-color plot) is definitely larger than usual."
  **"You should be especially suspicious if BAD_COUNTS_ERROR is set in a given band, which says that the interpolation over bad pixels is so significant that you should not believe the PSF flux error; it is probably underestimated."**
- [SDSS DR17, Algorithms: Bitmasks](https://www.sdss4.org/dr17/algorithms/bitmasks/) §OBJECT1 flags / §PhotoObj flags —— 位定义：`INTERP`=bit17 "The object contains interpolated pixels (e.g. cosmic rays or bad columns)."；`BAD_COUNTS_ERROR`=bit8 "An object containing interpolated pixels had too few good pixels to form a reliable estimate of its error"；`INTERP_CENTER`=bit12 "An object's center is very close to at least one interpolated pixel."；`PSF_FLUX_INTERP`=bit15 "The fraction of light actually detected (as opposed to guessed at by the interpolator) was less than some number (currently 80%) of the total."
- [SDSS DR17, Imaging Caveats](https://www.sdss4.org/dr17/imaging/caveats/) §"Bad CCD columns" —— **坏列导致空间相关噪声的官方原文**：
  "Some chips have bad CCD columns which get interpolated over by the photometric pipeline, leading to noticeably correlated noise. The bad columns for each run are currently available in fpM*.fits."
- [SDSS DR17, Tutorials: Flags（Clean Photometry）](https://www.sdss4.org/dr17/tutorials/flags/) 与 [Algorithms: Recommendation on the Use of Photometric Processing Flags](https://www.sdss4.org/dr17/algorithms/photo_flags_recommend/) —— 官方"干净测光"定义显式剔除插值相关位：`not EDGE, NOPROFILE, PEAKCENTER, NOTCHECKED, PSF_FLUX_INTERP, SATURATED, or BAD_COUNTS_ERROR`、`not INTERP_CENTER or not COSMIC_RAY`；"Removing Objects with Interpolation Problems" 节检 `PSF_FLUX_INTERP`、`BAD_COUNTS_ERROR` 与 `INTERP_CENTER ∧ CR`。
- [York et al. 2000, AJ 120, 1579, DOI 10.1086/301513, arXiv:astro-ph/0006396](https://arxiv.org/abs/astro-ph/0006396) §4（p.10）："it corrects the data for data defects (interpolation over bad columns and bleed trails, finding and interpolating over 'cosmic rays', etc)"。
- [Aihara et al. 2011, ApJS 193, 29, DOI 10.1088/0067-0049/193/2/29, arXiv:1101.1559](https://arxiv.org/abs/1101.1559) §3.2（p.8）："DR8 includes 'corrected frames', FITS files of each frame which have been bias subtracted and flat-fielded, with bad columns and cosmic rays interpolated over."
- [Blanton et al. 2011, AJ 142, 31, DOI 10.1088/0004-6256/142/1/31, arXiv:1105.1960](https://arxiv.org/abs/1105.1960) §3（p.6）："The images have had defects such as bad columns and cosmic rays identified and interpolated over by the photo pipeline."；其自有流程 "We interpolate over saturated pixels and cosmic rays using simple linear interpolation in the x direction."

→ **可引用的因果链（三层，均有原文）**：坏列 → 被插值（INTERP）→ ①插值像素与邻列**相关噪声**（SDSS Caveats）；②插值占比过大时**PSF 通量误差被低估、不可信**（SDSS BAD_COUNTS_ERROR）；③中心被插值的天体测量不可靠（SDSS INTERP_CENTER / HSC center 位 / PS1 suspect 掩膜）。

### 2.3 反证：Rubin/LSST 的实现把插值后的像元值代回方差式（代码级证据）

- [Rubin/LSST Science Pipelines `lsst.ip.isr` 源码（GitHub `lsst/ip_isr`, main, `python/lsst/ip/isr/isrFunctions.py`）](https://raw.githubusercontent.com/lsst/ip_isr/main/python/lsst/ip/isr/isrFunctions.py) —— 代码级证据（**非论文，属实现证据**）：
  `interpolateDefectList` / `interpolateFromMask` 调 `measAlg.interpolateOverDefects` 并置 `INTRP` 位；`updateVariance()` 的 docstring 为 "Set the variance plane based on the image plane"，实现为 `var = image/gain + (readNoise/gain)²` —— 即**插值像素的 variance 由插值后的像元值推出，而非由插值算子的传递函数推出**；`setBadRegions()` 中 `badPixels = (mask & BAD) > 0 AND (mask & INTRP) == 0`（INTRP 像素不参与坏区统计填充）。另有 `maskSatCoreColumns()`（"Mask full columns that have 20 percent of the height of the footprint"）与 `maskITLDip()`（列掩膜，超 `itlDipMaxColsPerImage` 回滚）。
  → 这解释了为什么"方差被低估"的显式公式在文献里找不到：**主流管线的做法是"标记 INTRP + 让方差面按插值后的值计算"，把可信度判断交给下游 flag**，而不是给插值像素单独赋一个解析方差。
  ⚠️ **证据强度分级（引用时务必保留）**：Rubin 侧"插值像素方差不可信"**只有代码证据**（`updateVariance()` 的实现路径 + `setBadRegions()` 把 INTRP 排除）；官方文档（DPDD LSE-163 §2.2）**只定义 variance plane 与 mask plane 存在**，**从未写成"不可信"的句子**。与之相对，**"误差被低估"的明确文字表述只在 SDSS 侧存在**（`BAD_COUNTS_ERROR`："it is probably underestimated"）。不要把 Rubin 的代码行为转述为 Rubin 的官方论断。
- [Rubin Observatory Data Products Definition Document, LSE-163（rev. 2023-07-10），DOI 10.71929/rubin/2587118](https://docushare.lsst.org/docushare/dsweb/Get/LSE-163) §2.2 "Image Characterization Data"（p.15）：
  "Each processed image, including the coadds, will record information on pixel variance (the 'variance plane'), as well as per-pixel masks (the 'mask plane'). These will allow the users to determine the validity and usefullness of each pixel in estimating the flux density recorded in that area of the sky."
  p.19 脚注 20 把 "**bad pixel/column interpolation**" 与 bias/dark 相减、flat fielding 并列为处理步骤；p.24 列出 "by artifacts (bad columns, diffraction spikes, etc.)"。
- [Rubin/LSST Science Pipelines, Getting started tutorial part 3](https://pipelines.lsst.io/getting-started/display.html) §"Interpreting displayed mask colors" —— calexp 掩膜位官方清单：`BAD, CR, CROSSTALK, DETECTED, DETECTED_NEGATIVE, EDGE, INTRP, NOT_DEBLENDED, NO_DATA, SAT, SUSPECT, UNMASKEDNAN`；前文 "Exposures … contain not only the image data, but also a variance image for uncertainty propagation, a bit mask image plane, and key-value metadata."

### 2.4 巡天对"掩膜比例"的量化门槛（可替代 mag 偏差的定量口径）

- [Chambers et al. 2016, arXiv:1612.05560](https://arxiv.org/abs/1612.05560) **§2.5 "GPC1 – the Gigapixel Camera #1"**（PDF p.7，节标题已 grep 确认）Table 3 "Pixel Mask fractions" —— **逐类像素掩膜占比**：Good Pixels 76 %；No Pixel/gap 10.1 %；**Detector flaws 10.7 %**；**Poor Charge Transfer Efficiency 2.2 %**；Other defect flags 1 %。正文："the dead cells, pixel gaps and masking of defective pixels account for an overall loss of 20% of the focal plane in any one exposure. There is an additional dynamic masking of around 2-3% per exposure, which mostly covers the 'burn-trails'. Therefore the overall fill factor of the camera is 76 ± 1% per exposure"。
- [Magnier et al. 2020, ApJS 251, 3, arXiv:1612.05244](https://arxiv.org/abs/1612.05244) §4.5（PDF p.12）—— **掩膜像素对测光的定量门槛（PSF_QF）**：`PSF_QF` = "the number of unmasked pixels is summed, weighted by the normalized PSF model"，取值 0（全掩膜）–1（全未掩膜）；"For a generous cut... PSF_QF > 0.85 is used in some contexts; in other cases, we require PSF_QF > 0.95 to ensure a high-quality measurement."；并定义四类 suspect 掩膜（SPIKE/CORE/BURNTOOL/插值-卷积污染），"If the normalized PSF-weighted fraction of pixels masked due to any of these four conditions exceeds 25%, then one of the following bits is raised…"。§（p.5）："The library functions used by psphot understand two types of masked pixels: 'bad' and 'suspect'. Bad pixels are those which should not be used in any operations, while suspect pixels are those for which the reported signal may be contaminated or biased, but may be usable in some contexts."
- [Waters et al. 2020, ApJS 251, 4, arXiv:1612.05245](https://arxiv.org/abs/1612.05245) §（PDF p.9）："The remaining mask category accounts for known bad columns, cells that do not calibrate well, and vignetting."；mask 位 `DETECTOR` 0x0001 / `CONV.BAD` 0x2000 / `CONV.POOR` 0x4000（"The pixel is bad after convolution with a bad pixel."）。
- [DES DR1（Abbott et al. 2018, ApJS 239, 18, DOI 10.3847/1538-4365/aae9f0, arXiv:1801.03181）](https://arxiv.org/abs/1801.03181) §3.2 Multi-Epoch (Coadd) Processing（p.7–8）—— 坏列进入 weight/mask 的方式：
  "a pair of weight planes are formed, that set the as-yet unaltered single-epoch weights to zero to remove defects tracked in the MSK plane. Both weight planes are formed so that we can separately track **spatially persistent defects** (e.g., saturated stars and bleed trails) and **temporary defects** (e.g., interpolated bad columns, cosmic-rays, satellite trails)."
  "A mask plane (MSK) is formed that carries a value of 0 for good pixels and 1 for pixels where no good data exist (due to lack of image coverage or persistent defects)."
  "The weight plane is not altered to account for flagged defects; this allows the user to customize the severity of the defects to be [masked]"（p.7 栏间，原句被分栏截断）。
  → **DES 明确把"插值坏列"归类为 temporary defect**，与 persistent defect 分开跟踪，这是"坏列修复后信息等级下降"的巡天级一手口径。

1. **把坏像素的噪声设为无穷大，而不是抄邻列方差**
   [Bosch et al. 2018, PASJ 70, S5, arXiv:1705.06766](https://arxiv.org/abs/1705.06766) §4.5（PDF p.18）：
   "In order to apply this to interpolation, we set the noise to be infinitely larger in the bad columns than the good."
   → 该文对坏列像素给出的插值权重是 `{−0.274, 0.774, 0.000, 0.774, −0.274}`（坏列自身权重 0.000），并明确 "it is clear that the estimator is unbiased if Σ_j D_pj = 1"（式 4 下方）。这是"插值估计量无偏"与"该像素噪声信息不可用"两条论断的一手出处。

2. **插值像素被单独打标，且只有"中心被插值"的天体才被判定为不可靠**
   同上 §4.5：mask 位区分"被插值"与"插值原因"；"Generally only objects whose centers were interpolated (flags pixel interpolated center) should be considered to have unreliable measurements."
   → 这是"修复会污染测量可靠性"的一手判据，但**是定性分级，不是方差赋值**。

3. **坏列与宇宙线在掩膜上会互相叠加成复杂形状（对修复算法本身的约束）**
   [van Dokkum & Pasha 2024, PASP 136, 034503, arXiv:2312.03064](https://arxiv.org/abs/2312.03064) §2.2（PDF p.3）：
   "As an example, bad columns typically intersect many cosmic rays, producing complex shapes."
   同文 §1（PDF p.1）：天文图像中 "missing or unwanted information, such as bad pixels, bad columns, cosmic rays, masked objects, or residuals from imperfect model subtraction"；其 `maskfill` 用 3×3 中值滤波迭代外推 + 最后对掩膜区内做 3×3 boxcar 平滑（§2.2）。

- SDSS 的 `INTERP` 位（"pixel's value has been interpolated"）：[SDSS DR7 read_mask 文档](https://classic.sdss.org/dr7/products/images/read_mask.html) 明确 `S_MASK_INTERP = 0, /* pixel's value has been interpolated */`，且 mask 可按名字 `CR` 或 `INTERP` 取 HDU。→ **官方承认插值像素需单独标记**；但该页**未讨论方差如何赋值**。

- **LSST/Rubin variance plane、DrizzlePac Handbook、Fruchter & Hook 关于权重**：DrizzlePac Handbook v3 PDF 已下载（`hst-docs.stsci.edu/drizzpac/files/148006999/206803956/1/1760017278941/The_DrizzlePac_Handbook_Version3.pdf`），但该 PDF **无文本层**（`pdftotext` 抽出 0 字符），无法逐句摘录；LSST pipelines 文档的 mask/variance 页面在本次检索中未定位到稳定 URL。**[未核验]**，见末节。
  已核验书目：[Fruchter & Hook 2002, PASP 114, 144–152, DOI 10.1086/338393, arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)（摘要原文："can weight input images according to the statistical significance of each pixel… the noise characteristics of output images are discussed"）。

---

## 3. CTE 拖尾 vs 坏列：形态与成因的区分判据

- [STScI 2025/2026, ACS DHB §4.6.1 "The Issue"](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-6-wfc-ccd-detector-charge-transfer-efficiency-cte) §4.6.1（PDF p.169）—— **拖尾的形态判据（方向 + 长度 + 附着对象）**：
  "This is seen directly in the 'charge trails' that can extend to over 50 pixels in length upstream (i.e., away from the parallel transfer direction) of hot pixels, cosmic rays and bright stars (see Figure 4.28 for an example)."
  同节给出辐射前转移效率量级："typically 0.999996 of each charge packet was transferred successfully from one pixel into the next"；并明确成因："the flux of energetic particles such as relativistic protons and electrons damages the silicon lattice of the CCD detectors. This creates both 'hot' pixels and charge traps."
  同书 Figure 4.28 图注："A section (800 × 800) of an ACS frame of 47 Tucanae. Note the presence of parallel CTE trails extending from the stars indicating the effect of CTE on the detector."

- [STScI 2026, WFC3 DHB §6.3](https://hst-docs.stsci.edu/wfc3dhb/chapter-6-wfc3-uvis-charge-transfer-efficiency-cte/6-3-the-nature-of-cte-losses) §6.3 —— 拖尾**方向 = 平行转移方向反向**，随行号累积：
  "CTE takes charge away from downstream pixels and deposits it into upstream pixels… 'trails' of charge that extend out from sources in the direction opposite the readout amplifier."
  Figure 6.1 图注给出坐标约定："Parallel shifting is downward toward the readout amplifier."

- [Anderson & Bedin 2010, PASP 122, 1035, DOI 10.1086/656399, arXiv:1007.3987](https://arxiv.org/abs/1007.3987) §1（PDF p.3）与 Fig. 1（PDF p.4）—— **拖尾的成因（晶格空位陷阱）与对科学的两类影响**：
  "when energetic particles impact CCD detectors, they can displace silicon atoms and create vacancies (defects) in the silicon lattice. During the read-out process, these defects can temporarily trap electrons, causing some of a pixel's charge to arrive late at the read-out register, and thus to be associated with a different pixel."
  "First, it shifts charge from the core of a source to a trail that extends well outside of a normal photometric aperture, thus reducing the brightness measured in aperture photometry and PSF-fitting… Second, imperfect CTE blurs out the profiles of objects, shifting their centroids and increasing their FWHMs."
  Fig. 1 图注（**区分 CTE 拖尾与读出电子学伪影的一手依据**）："The vertical trails extending upward from the stars are indicative of imperfect CTE; **the horizontal streaks are an artifact of the post-repair readout electronics** and are currently under study (Grogin et al. 2010)."
  §2（PDF p.7–8）：模型 "reproduces the observed trails out to 70 pixels"；用暗帧中的 warm pixel（WP）与宇宙线做标定靶（"The WPs in dark frames serve as delta functions that allow us to calibrate the size and extent of CTE trails for pixels of various flux in images that have essentially no background"）。
  → **与坏列的关键区别**：CTE 拖尾**依附于一个真实电荷源**（星、热像素、宇宙线），长度可达数十像素且**随行号/背景/流量变化**；坏列是**无源、跨帧固定**的偏置结构（ACS DQ 128）。

- [Massey et al. 2010, MNRAS 401, 371 (arXiv:0909.0507)](https://arxiv.org/abs/0909.0507)（PDF p.7）—— 陷阱密度的**时间累积率**：
  "This is fit by a constant accumulation of (4.34 ± 0.13) × 10⁻⁴ traps per pixel per day, and an initial… density on launch of ρ0q = 0.037 ± 0.001 traps per pixel"；像素级改正耗时 "25 minutes per 4096 × 4096 ACS/WFC image on a single 2 GHz processor"。

- [Snyder & Roodman 2020, Proc. SPIE 11454 (arXiv:2001.03223)](https://arxiv.org/abs/2001.03223) §1（PDF p.1）—— **deferred charge（延迟电荷）的一手定义与 LSST 规格**：
  "The incomplete transfer of charge results in a spurious trail of signal (most noticeable in bright sources) that can affect precision measurements of source position and shape."
  "the charge transfer inefficiency (CTI), defined as the ratio of electrons not transferred between two neighboring pixels, to the total electrons before the transfer, and is measured for both parallel and serial pixel transfers."
  "The LSST specification for serial CTI and parallel CTI is less than 5 × 10⁻⁶ and less than…"；测量方法为 EPER（extended pixel edge response）。

- [Townsley et al. 2000, ApJ 534, 424 (arXiv:astro-ph/0004048)](https://arxiv.org/abs/astro-ph/0004048)（PDF p.1–3）—— 拖尾模型的另一支（Chandra ACIS）：摘要/正文用 "charge trailing" 描述电荷再分配（原文片段："(charge trailing), shielding within an event (charge in the leading pixels of the 3×3 event island…"）；引入 "CTI deviation map"；"central broadening with row number is primarily the…"。
  → 与坏列的区别判据同 ACS/WFC3：**沿读出方向、随行号增长的拖尾**。

- [Ryon & Grogin 2026, ACS/WFC 串行 CTE (arXiv:2602.02844)](https://arxiv.org/abs/2602.02844)（PDF p.5、p.17）—— **串行（X 方向）CTE** 的拖尾与"re-trailing"：
  "trailing, resulting in image R. The difference image, D = R − I, is subtracted from the…"；"may be released into the second pixel, i.e., 're-trailing'"；结论："serial-CTE-corrected dark frames show little to no residual serial CTE trailing from hot…"。
  → 对应 ACS DHB §4.6.3 所述 "In 2024, a serial (X-direction) CTE correction model was added to the existing parallel (Y-direction) CTE correction model in CALACS. Serial CTE losses are a minor effect compared to parallel."

- 经典专著 Janesick, *Scientific Charge-Coupled Devices* (SPIE Press, 2001) Chapter 8：**[二手转引]** —— 出处为 Anderson & Bedin 2010 §1 原文 "Janesick (2001, Chapter 8) summarizes the recent laboratory-based…"（PDF p.3）；本报告未打开该书正文。

---

## 4. 坏列修复对测光的影响（定量结果）

- [Bosch et al. 2018, PASJ 70, S5, arXiv:1705.06766](https://arxiv.org/abs/1705.06766) §4.5（PDF p.18）—— **唯一找到的"可用性分级"判据**：
  "Generally only objects whose centers were interpolated (flags pixel interpolated center) should be considered to have unreliable measurements."
  → 这是"坏列/坏点插值会破坏测光可靠性"的官方定性判据；**该文未给出 mag 级偏差数字**。
  同文 §4.4（PDF p.18）给出宇宙线掩膜的事件级阈值 150 e⁻（可与缺陷掩膜口径对照）。

- [Anderson & Bedin 2010, PASP 122, 1035, arXiv:1007.3987](https://arxiv.org/abs/1007.3987) §1（PDF p.3）—— **CTE 拖尾对测光的定量机理（孔径测光与 PSF 拟合都被低估）**：
  "it shifts charge from the core of a source to a trail that extends well outside of a normal photometric aperture, thus reducing the brightness measured in aperture photometry and PSF-fitting. This has a significant impact on photometry, both for point sources and for extended sources."
  摘要："when we apply the image-restoration process to science images with a variety of stars on a variety of background levels, it restores flux, position, and shape."
  → 该文给出的是**修复有效性**（flux/position/shape 被恢复），**未给出修复后残余的 mag 偏差数字**。

- [ACS DHB §4.6.2](https://hst-docs.stsci.edu/acsdhb/chapter-4-acs-data-processing-considerations/4-6-wfc-ccd-detector-charge-transfer-efficiency-cte) §4.6.2（PDF p.171）—— **背景依赖的定量损失**（用于判断测光偏差来源，而非坏列本身）：
  "Observations with very low background (< 30 e¯ for ACS) will suffer large losses for very faint sources… ACS ISR 2022-04 shows that when the background is 40 e⁻ or greater, even the faintest stars retain >50% of their electrons."
  §4.6.3：ACS 的像素级 CTE 改正基于 Anderson & Bedin (2010) 方法，迭代前向建模；2018 年重参数化（ACS ISR 2018-04）；2024 年加入串行（X 方向）项（ACS ISR 2024-07）。

- [Legacy Survey DR10 bitmasks 文档](https://www.legacysurvey.org/dr10/bitmasks/) —— 巡天层面把坏列并入 bad-pixel 掩膜的官方定义：
  `BADPIX` 位描述为 "bad columns, hot pixels, etc."；并定义 `ANYMASK_X`（源触及任一同波段图像中的坏像素）与 `ALLMASK_X`（源触及全部图像中的坏像素）两种口径。
  → 这是"坏列掩膜进入测光样本定义"的一手文档依据；**该页未给出 mag 偏差数字**。

- [Flaugher et al. 2015, AJ 150, 150, DOI 10.1088/0004-6256/150/5/150, arXiv:1504.02900](https://arxiv.org/abs/1504.02900) §4.1 / "CCD selection"（PDF p.30–31、p.34）—— **DECam 的坏像素率定量规格与实测（本报告找到的唯一"率"级定量）**：
  "While the cosmetic requirements for DECam CCDs were that no individual CCD shall has more than **2.5% bad pixels**, an additional criterion was applied to the average of the focal plane. The whole focal plane was required to have no more than **0.5% bad pixels**."
  "The selection criteria were, in order, an especially high full well (FW > 180,000 e⁻), high QE, and lastly a low fraction of defective pixels (**< 0.4% bad pixels**)."
  "The worst CCD had **0.389% bad pixels**. Over the 62 CCDs on the focal plane just **0.049% are considered bad pixels**, more than 10× better than the requirement."
  → 注意口径：DECam 用的是"坏像素（bad pixels）"而非"坏列数"，**未给坏列条数**。
- [Chambers et al. 2016（PS1）](https://arxiv.org/abs/1612.05560) §2.5 Table 3 —— 逐类掩膜占比（Detector flaws 10.7 %、Poor CTE 2.2 %、Overall fill factor 76 ± 1 %，详见 §2.4）。
- [Magnier et al. 2020（PS1 psphot）](https://arxiv.org/abs/1612.05244) §4.5 —— 以 `PSF_QF > 0.85 / > 0.95` 作为掩膜像素占比的可用性门槛（详见 §2.4）。
- [SDSS DR17 Flags Detail](https://www.sdss4.org/dr17/algorithms/flags_detail/) —— 官方对偏差的**定性层级**：单条坏列插值 "the photometry should be essentially perfect"；插值 > 20 % PSF 流量 → `PSF_FLUX_INTERP`，"the number of outliers … is definitely larger than usual"；插值严重 → `BAD_COUNTS_ERROR`，"you should not believe the PSF flux error; it is probably underestimated"。
- **未找到**任何给出"坏列修复导致的 **mag 级**偏差数字"的一手来源（详见末节）。已找到的最强定量口径是上表的**率与掩膜占比**（DECam 0.049 %–0.389 % 坏像素、PS1 fill factor 76 ± 1 %、PS1 PSF_QF 门槛 0.85/0.95、SDSS 20 % PSF 流量门槛）。

---

## 5. 术语一手定义与四类现象区分

### 5.1 术语表（定义与形态，逐条给出处）

| 术语 | 一手定义（原文） | 出处 |
|---|---|---|
| **bad column**（坏列） | `128 = Bias structure (e.g., bad columns)` —— 归类为**偏置结构**缺陷，来自 BPIXTAB 参考表的**永久**坏像素 | ACS DHB Table 3.4 / §3.4 doDQI |
| **bad / dead pixel**（坏点） | `4 = Bad detector pixel or vignetted pixel` | 同上 |
| **hot pixel**（热像素） | "A pixel above 0.14 e¯/pixel/second is considered a 'hot' pixel"（随历史变更，见 Table 4.5） | ACS DHB §4.3.2 |
| **warm pixel**（温像素） | "above 0.06 e¯/pixel/second is considered a 'warm' pixel"（0.06–0.14 e⁻/s） | ACS DHB §4.3.2 |
| **unstable dark current pixel** | `32 = Pixels with unstable dark current; includes random telegraph signal noise and fading hot pixels` | ACS DHB Table 3.4 |
| **sink pixel / charge trap** | `1024 = Sink pixel or pixel affected by sink pixel charge traps` | ACS DHB Table 3.4 |
| **charge trap（辐射致陷阱）** | "these defects can temporarily trap electrons, causing some of a pixel's charge to arrive late at the read-out register" | Anderson & Bedin 2010 §1 |
| **CTE / CTI** | "the charge transfer inefficiency (CTI), defined as the ratio of electrons not transferred between two neighboring pixels, to the total electrons before the transfer" | Snyder & Roodman 2020 §1 |
| **deferred charge（延迟电荷）** | "The incomplete transfer of charge results in a spurious trail of signal (most noticeable in bright sources)" | Snyder & Roodman 2020 §1 |
| **charge trailing（电荷拖尾）** | 模型中的电荷再分配项；Chandra ACIS 语境 | Townsley et al. 2000 |
| **linear artifacts（线性伪影，含坏列）** | "In addition to satellite trails, other common artifacts can be linear in nature (e.g., some cosmic rays, certain types of scattered light, **charge bleeding**, and diffraction spikes), all of which must be detected and masked." | ACS ISR 2022-08 §1（PDF p.2） |
| **interpolated pixel** | `S_MASK_INTERP = 0, /* pixel's value has been interpolated */` | SDSS DR7 read_mask 文档 |

> 术语辨析提示：**"linear defect" 在本次检索的天文一手文献中未出现为固定术语**；工程语境中的"线性缺陷"在天文侧对应的是**分散的多个具体类别**（bad column / hot column / sink pixel / charge bleeding / CTE trail），需要按上表逐条落到具体 DQ 位或手册条款，而不是当作单一术语使用。

### 5.2 四类现象的区分依据

1. **传感器本征缺陷（晶格损伤、暗电流增强、陷阱）**
   成因一手表述：Anderson & Bedin 2010 §1（"energetic particles… displace silicon atoms and create vacancies (defects) in the silicon lattice"）；ACS DHB §4.3.2（"When pixels are damaged by radiation or other causes, they can suffer enhanced dark current"）。
   形态：单像素或小簇（hot/warm/sink）；**列级**表现为整列偏置结构（DQ 128）。
2. **读出电路缺陷（列并行 ADC / 输出寄存器 / 读出电子学）**
   一手依据：Anderson & Bedin 2010 Fig. 1 图注把 ACS/WFC 上的**水平条纹**明确归因于 "an artifact of the **post-repair readout electronics**"（与 CTE 的竖直拖尾并列对比）。
   另有 ACS DHB §4.5.3 的 cross-talk（`~10⁻⁴–10⁻⁵` of source）与 §4.5.2 的 glint（"a thin ray of light extending diagonally across the WFC FOV"，ACS ISR 2024-05）属于读出/光路伪影，但手册**未把任何一类明确称为"列并行 ADC 缺陷"**。
3. **CTE 拖尾**
   判据三条（均有原文）：① 方向 = 平行转移方向的反向、指向远离读出放大器（WFC3 DHB §6.3）；② 依附于真实电荷源（星/热像素/宇宙线）且长度可达 50–70 像素（ACS DHB §4.6.1；Anderson & Bedin 2010）；③ 强度随行号、背景与流量变化（WFC3 DHB §6.3 陷阱计数；ACS DHB §4.6.2 背景依赖）。
4. **宇宙线 / 卫星线（外源事件，非缺陷）**
   判据：**跨帧不复现 + 形态与方向不固定 + 可贯穿整个视场**。
   - Kruk et al. 2023（Nature Astron. 7, 262）：卫星线 "traverse the entire field of view (FoV)… and, in most cases, appear as straight lines"，逐帧发生率 2.7%（2002–2021 档案）。
   - ACS ISR 2016-01（Borncamp & Lim）§（PDF p.4、p.7）：用**角度众数一致性**排除宇宙线/衍射星芒 —— "cosmic rays and diffraction spikes may be fitted with their own line segment and we do not want them to be confused and included within the satellite"；并承认主要假阳性来自 "long, interspersed cosmic rays traveling at the same angle while crossing the corners of the chip"。
   - ACS ISR 2022-08（Stark et al.）§1（PDF p.2）把 **charge bleeding**（电荷溢出，与坏列/饱和相关）与宇宙线、散射光、衍射星芒一起列为"线性伪影"，用 Median Radon Transform 做整体线性特征检测；MRT 对**整条路径**敏感而对局部亮源不敏感。
   - LA-Cosmic（van Dokkum 2001, PASP 113, 1420, arXiv:astro-ph/0108003）§2–§3.2：以 Laplacian 边缘检测 + 细结构比 `L⁺/F` 区分宇宙线与欠采样点源 —— "The critically sampled star has L⁺/F = 0.7, and L⁺/F = 1.8 for the undersampled star. The cosmic-ray has L⁺/F = 21, and is easily distinguished from undersampled point sources."；默认 `flim = 2`，WFPC2 数据需 `flim ≈ 5`。
   → **单帧长直线不能只靠形态判定**：必须叠加"跨帧是否复现"（缺陷固定复现；卫星线/宇宙线不复现）与"是否贯穿全视场、方向是否符合卫星过境几何"两条判据（Kruk 2023 + ACS ISR 2016-01 的角度众数判据）。

---

## 6. 未核验 / 查不到（诚实登记）

| 条目 | 状态 | 查了什么 / 为何没拿到 |
|---|---|---|
| **Howell, *Handbook of CCD Astronomy* (2nd ed., CUP 2006) 的坏列/坏像素章节号与正文** | 未核验 | 未获得可打开的正文或完整目录页；不给出章节号以免编造 |
| **Berry & Burnell, *The Handbook of Astronomical Image Processing* (2005) 的坏列章节** | 未核验 | 同上；Willmann-Bell 站点在本次会话中不可达 |
| ~~IRAF 的坏列判据~~ | ✅ **已解决（有保留）** | iraf.net 本站 `irafhelp.php` 全 403，但 Wayback 存档的 **Massey 1997 ccduser3 PostScript** 可读（见 §1.3）：**IRAF 无自动 k·σ 坏列判据，靠用户 fixfile**。保留项：`ccdmask`/`badcols` 类任务是否存在仍未核验（stsdas.stsci.edu gethelp 404）；PS 文件**未逐句读散文**，仅核验字面参数字符串 |
| **HST 仪器手册是否给出坏列判定阈值** | ✅ **已解决（否定）** | WFC3 IHB §5.4 与 WFC3 DHB §3.2.3、STIS DHB §2.5 全部已读：只有**坏列清单/位置**与 DQ 位语义、以及热像素的**绝对暗电流阈**（54 e⁻/hr = 13.5 e⁻/pixel；2021 起改为公式，见 WFC3 ISR 2020-08/2021-03/2021-06，ISR 本体未取到）。**没有任何 k·σ 列判据** |
| **"列中值偏离 k·σ 判为坏列"的期刊/手册定量判据（k 取多少、为什么）** | **查不到** | 检索路径：ACS DHB §3.4/§4.3/§4.5 全文、WFC3 DHB §5.5/§6、Legacy Survey DR10、SDSS DR7/DR18、HSC 管线论文（arXiv:1705.06766）全文 grep `bad column`/`sigma`/`threshold`。**结论：这些一手文档只给出"暗电流绝对阈值"（0.06/0.14 e⁻/s）与"DQ 位分类"，未给出坏列的 k·σ 判据。** 该判据若存在，更可能在 IRAF/ccdproc 类软件文档或未公开的仪器组内部 ISR 中 |
| **DrizzlePac Handbook v3 中关于坏像素权重/方差传播的原文** | 未核验 | PDF 已下载（5.3 MB）但**无文本层**，`pdftotext` 抽出 0 字符；未做 OCR |
| **LSST/Rubin pipelines 文档的 mask plane 与 variance plane 定义页** | 未核验 | 尝试 `pipelines.lsst.io/modules/lsst.afw.image/masks.html` 与 `.../index.html` 均 404 或未命中目标内容；未定位到稳定 URL |
| ~~"插值坏像素后方差应赋何值"的显式公式~~ | ✅ **已解决** | 见 §2.0：van Dokkum & Pasha 2024 §6 给 `σ_mask = σ_org (2d+1)^(−0.5)`；Desai et al. 2016 §4.2 给"PSF 核内高斯误差传播 + 按 σ² 抽样复原"。**仍未解决的是：相关系数 ρ 或有效独立样本数 N_eff 的定量表达式** |
| **坏列/缺陷修复导致的 mag 级测光偏差数字** | **查不到** | 检索路径：HSC 管线（定性"unreliable measurements"）、ACS DHB §4.6（CTE 损失定量，非坏列）、Legacy Survey DR10（掩膜口径）、SDSS DR17 全套 flag 文档（最强表述仅到"error … probably underestimated"）、PS1 `psphot`（PSF_QF 门槛，非 mag）、DES DR1/DR2（掩膜与权重口径）、arXiv `abs:"bad columns" AND abs:"photometry"`（0 命中）、`abs:"bad columns" AND abs:"Dark Energy Camera"`（0 命中）。**没有任何一手来源给出 mmag 或 % 级的坏列修复测光偏差** |
| **逐条"坏列条数"定量描述（"N bad columns out of 4096"）** | **查不到** | 已读并 grep：DECam/Flaugher 2015（只给坏**像素**率 0.049 %–0.389 %，无坏列条数）、DES DR1 §3.2 / DES DR2（DR2 全文 grep `bad column/bad pixel/defect/interpolat/mask plane` **0 命中**）、PS1 Chambers 2016 Table 3（只给掩膜面积占比）、PS1 Magnier 2020 / Waters 2020、HSC Bosch 2018 / Aihara 2018、SDSS DR17 文档群。**没有任何一手来源给出"每片 CCD 多少条坏列"** |
| **IOP / ADS 的出版商页面复核** | 受限 | `iopscience.iop.org` 与 ADS 均被 captcha 拦截；上列期刊卷页/DOI 通过 **arXiv abs 页的 "Journal reference / Related DOI" 字段**核对，未在出版商站点复核 |
| **HSC pipedoc（hsc.mtk.nao.ac.jp/pipedoc）与 SDSS DR18 imaging 子页** | 未取到 | 前者返回 404；后者子页 404，SDSS 部分改用 SDSS-IV DR17 文档站（`sdss4.org/dr17`） |
| **Janesick 2001 *Scientific Charge-Coupled Devices* Chapter 8 正文** | 二手转引 | 仅由 Anderson & Bedin 2010 §1 引用得到（"Janesick (2001, Chapter 8) summarizes the recent laboratory-based…"）；未打开该书 |
| **Kruk et al. 2022, A&A 661, A85** | 二手转引 + 已发现书目错误 | ACS ISR 2022-08 参考文献表写作 "Kruk, S., García Martín, P., Popescu, M., et al. 2022, , 661, A85"；**Crossref 反查该卷页实为 "Hubble Asteroid Hunter"（DOI 10.1051/0004-6361/202142998），并非卫星线论文**。Kruk 等真正的卫星线论文是 [Nature Astronomy 7, 262–268 (2023), DOI 10.1038/s41550-023-01903-3](https://doi.org/10.1038/s41550-023-01903-3)（已核验） |
| **WFC3 ISR 2021-09（陷阱模型原始 ISR）与 ACS ISR 2011-01/2012-03/2018-04/2022-04/2024-07 的 PDF 原文** | 未核验 | STScI ISR 索引页仅列出近两年 PDF；旧 ISR 的直链模式（`isrYYMM.pdf`）在旧路径下 404。其结论已在 WFC3 DHB §6.3/§6.4 与 ACS DHB §4.6 中**转述并已摘录** |
