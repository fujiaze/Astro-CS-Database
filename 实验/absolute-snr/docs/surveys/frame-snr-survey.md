# 帧级 SNR（frame-level SNR）文献与开源实现调研记录

> 工作项：**FRAME-SNR-CANON**（RELEASE-02）。代码：`实验/absolute-snr/code/reverse_verify/frame_snr/`；
> 定案：`实验/absolute-snr/docs/frame-snr-canon.md`；中间产物：`run/reverse_verify/frame_snr/`。
>
> **记录规则**（`../README.md §5`）：每条给 **①可核对标识 ②SNR 定义 ③是否扣背景/天光如何进入
> ④借鉴点 ⑤不借鉴点 ⑥场景差异 ⑦证据（逐字）⑧核对状态**。
> **禁止编造引用**：未抓到一手页面的一律标「待核对」并写清尝试路径。
>
> **本轮核对方式**：`curl`/`web_fetch` 抓一手页面与官方源码（raw.githubusercontent.com、
> readthedocs、arXiv/Crossref API、SVN export）；本机 `photutils 3.0.0` / `sep 1.4.1` / `astropy 8.0.1`
> 实跑（装在 `/dev/shm/astrocs_fsnr/frame_snr_canon/pylibs`，**非系统环境**）。
> 本轮**没有**编译运行 IRAF/PSFEx/SExtractor 二进制（如实登记）。

---

## 0 必须先纠正的前提（比编一个更值钱）

| 任务书/常见说法 | 一手核对结果 |
|---|---|
| SExtractor 有 `FLUX_GAUSS` 参数 | **不存在**。SExtractor master（`configure.ac` 版本 **2.29.0**）`src/param.h` 完整参数表内 `FLUX_GAUSS` **零命中**；只有 `FLUX_WIN`（"Gaussian-weighted flux"）。`doc/src/*.rst` 亦零命中。**如实登记用户点名有误。** |
| SExtractor `SNR_WIN` 在手册里有定义 | 参数**存在**（`src/param.h:134` `{"SNR_WIN", "Gaussian-weighted SNR", ...}`、`src/winpos.c:289`），但**官方 rst 手册里没有它**（`Param.rst` grep `SNR` = 0）—— 手册与实现不同步，引用须引源码。 |
| Naylor 1998 的 DOI 是 `10.1046/j.1365-8711.1998.01407.x` | **错**。该 DOI 经 Crossref 核对是 *"Deep hard X-ray source counts from a fluctuation analysis of ASCA SIS images"*（MNRAS 297, 41）。Naylor 的正确 DOI = **`10.1046/j.1365-8711.1998.01314.x`**，MNRAS **296, 339–346**（Crossref 标题逐字 *"An optimal extraction algorithm for imaging photometry"*）。 |
| Irwin 1985 的 DOI 是 `10.1093/mnras/214.3.575` | **错**（Crossref 无此记录）。正确 DOI = **`10.1093/mnras/214.4.575`**，MNRAS 214, **575–604**。 |
| psphot 论文 = Waters et al. 2020 ApJS 251, 4 | **对调了**。psphot 核心论文 = **Magnier et al. 2020, ApJS 251, 5**, arXiv:1612.05244, DOI 10.3847/1538-4365/abb82c；Waters et al. 2020 ApJS 251, 4（DOI ...abb82b）是 detrend/warp/stack 论文，全文 `psphot` 0 次。 |
| DES DR2 论文 = arXiv:2101.02242 | **错**。DES DR2 = **arXiv:2101.05765**, ApJS 255, 20, DOI 10.3847/1538-4365/ac00b3。 |
| DrizzlePac Handbook 有 "weight image is proportional to the inverse variance" | **未找到**（Confluence CQL 全文检索 `totalSize: 0`）。实际表述是 `W = 1/(Var × scale^4)`。 |
| `github.com/panstarrs/ipp`（psphot 源码） | **404**。真实源码在 IPP Trac/SVN：`https://svn.panstarrs.ifa.hawaii.edu/trac/ipp/export/HEAD/trunk/...`（浏览页 403，`export` 直取 200）。 |

---

## 1 总结论（先给结论，再逐条）

1. **七个巡天/望远镜体系里，没有任何一个在星表或图像产品中定义「帧级 SNR 标量」。**
   它们只给**逐源** `flux`/`fluxErr`（SDSS、DES、LSST、psphot、HSC、JWST）或**逐像素** weight/variance（HST-DrizzlePac、JWST 的 ERR）。
2. **唯一给出整帧 SNR 解析公式的是 LSST（SMTN-002）**，但它只用于**深度/极限星等（m5）**计算，
   不是星表字段、不是图像产品字段。
3. 七家在「天光如何进入」上有一个**近乎一致的共识**：
   > **信号项扣掉天光均值；天光散粒噪声进误差分母；源自身泊松是否进分母各家取舍不同**
   > （SDSS **不进**、SExtractor/HSC/JWST/LSST **进**、HST-IVM **不进**）。
4. **「会被天光抬高的假信噪比」在一手来源里有明确证据**：PixInsight 官方文档自述其
   "standard SNR"（式[20]）的**分子是图像方差**，任何加性图像内容（官方举例：飞机尾迹）会
   "introduce a strong bias in the variance used as the numerator"。
5. **ACSD 的帧级 SNR 不能声称"沿用"上述任何一家**：可引的是**结构**（分子已扣背景、分母含天光散粒）
   与 **LSST SMTN-002 的帧级公式形状**；实现落点必须自己写并自己验证。

---

## 2 巡天管线

### [A1] Jones, R. L. (2016/2026). Calculating LSST limiting magnitudes and SNR (SMTN-002). Rubin Observatory / LSST Change-Controlled Document. DOI 10.71929/rubin/3408482. https://smtn-002.lsst.io/

- **SNR 定义**（**唯一一手帧级公式**）：

```
SNR = C / sqrt( C/g + (B/g + sigma_instr^2) * n_eff )
n_eff = 2.266 * (FWHM_eff / pixelScale)^2
sigma_instr^2 = (readNoise^2 + darkCurrent*expTime) * n_exp
```

  符号逐字："where C = total source counts, B = sky background counts **per pixel**, sigma_instr is the
  instrumental noise per pixel (all in ADU) and g = gain."
  实现落点：`lsst/pipe_tasks`（commit `0e56ae0`）`python/lsst/pipe/tasks/computeExposureSummaryStats.py:1262-1319`
  （`:1275` 公式原文，`:1312` `background = (skyBg/gain + sigma_inst**2) * neff`）。
- **是否扣背景 / 天光如何进入**：`B` 是**每像素天光计数**，只出现在**分母**（`(B/g)·n_eff` = 天光泊松按有效像素数放大）。
  分子 `C` 是**源**计数（不含天光）。⇒ **天光只进分母、只进方差、绝不进分子。**
- **借鉴点**：① 帧级 SNR 的**分子=源、分母=源泊松+（天光泊松+仪器噪声）×n_eff** 的结构，
  正是 ACSD 定案式的同构形式（把 `n_eff` 换成 PSF 的 `A_NEA = 1/ΣP_i²`）；
  ② `n_eff` 与 PSF 面积挂钩（ACSD 用 `A_NEA`）；
  ③ 明确区分「帧级深度量」与「逐源测光量」——ACSD 的 `frame_snr` 应当照此定位。
- **不借鉴点**：① 它用于**巡天规划/深度**（输入是曝光级元数据），不是逐帧产品字段；
  ② `n_eff = 2.266(FWHM/pixelScale)²` 是 LSST 的解析近似，ACSD 的 PSF 是离散归一化 Moffat4，
  应直接算 `A_NEA = 1/ΣP_i²` 而不是套这个系数；③ 它假设 gain 已知（ACSD 的 FITS 头拿不到 gain，见 §5）。
- **场景差异**：LSST 是巡天曝光级元数据；ACSD 是单帧 CCD/CMOS 标准化，必须**从帧本身**估噪声。
- **证据（逐字）**：见上引；`https://smtn-002.lsst.io/` HTTP 200，页面标注 DOI 10.71929/rubin/3408482。
- **核对状态**：**已核对**（SMTN-002 页面 + `pipe_tasks` 源码 `grep -n` 行号 + commit 钉版本）。

### [A2] LSST DM `meas_base`（commit `a44f29b`）— 逐源误差语义

- **SNR 定义**：**不定义 SNR。** `meas_base` 全仓库 `grep -rn -i "\bsnr\b"` = **0 命中**（唯一
  "signal-to-noise" 在 `python/lsst/meas/base/tests.py:451` 的测试辅助 docstring）。
  星表只有 `instFlux` / `instFluxErr`（`lib/include/lsst/meas/base/FluxUtilities.h:42-43`：
  `instFlux` "Measured instFlux in DN"；`instFluxErr` "Standard deviation of instFlux in DN"）。
  - `base_CircularApertureFlux`（`src/ApertureFlux.cc:165-171, 206-218`）：
    `instFlux = Σ image·w`，**`instFluxErr = sqrt(Σ variance·w²)`**。
  - `base_SdssShape`（`src/SdssShape.cc:607-626`）：取**质心像素处的 variance 图值**作 `bkgd_var`
    （源码自注 `// XXX Overestimate as it includes object`），Fisher 矩阵求逆 → `instFluxErr = sqrt(cov(0,0))`。
  - `base_LocalBackground`（`src/LocalBackground.cc:105-108`）：annulus `MEANCLIP` 作背景、`STDEVCLIP` 作其误差。
- **是否扣背景 / 天光如何进入**：`instFlux` **已扣背景**（Rubin DP0.2 官方逐字
  "PVIs are stored with the background already subtracted."）。天光散粒噪声**在方差图里**：
  `ip_isr/python/lsst/ip/isr/isrFunctions.py:849-855` `var = image/gain + (readNoise/gain)**2`，
  `image` 含天光。**关键实现事实**：扣背景**不移除**方差 ——
  `afw/include/lsst/afw/image/MaskedImage.h:823-826` 的 `operator-=` 只改 image 面
  （`{ *_image -= rhs; return *this; }`）。⇒ **均值减掉、散粒噪声留在分母**。
- **借鉴点**：① 「扣背景不改方差」的语义有实现级保证，比文档更硬；
  ② 逐源误差与帧级深度误差**分离**，避免"帧级 SNR"被误当测光量。
- **不借鉴点**：① `SdssShape` 用被源污染的质心像素方差（源码自承高估）；
  ② `LocalBackground` 的 fluxErr 是环内**散度**而非均值标准误，语义易误读；
  ③ `Σvar·w²` 假定像素独立，不表达相关噪声。
- **场景差异**：LSST 的 calexp 已由上游 ISR 建好方差图；ACSD 必须自建。
- **核对状态**：**已核对**（`git clone --depth 1` 本地源码 + `grep -n` 行号 + 全仓库 grep 计数）。

### [A3] SDSS photoop / frames — Lupton et al. photo 论文草稿 + DR17 官方文档

- **可核对标识**：草稿 PDF http://www.astro.Princeton.EDU/~rhl/photo-lite.pdf （SDSS DR17 Imaging Pipeline 页
  逐字链接为 "the draft photometric reduction paper"）；https://www.sdss4.org/dr17/algorithms/sky/ ；
  https://data.sdss.org/datamodel/files/PHOTO_REDUX/RERUN/RUN/objcs/CAMCOL/fpFieldStat.html
- **SNR 定义**：**不定义帧级 SNR。** 逐源 PSF 通量的 MLE 及其方差：
  Eq(25) `f_MLE = (Σ_i P_i O_i/σ_i²)/(Σ_i P_i²/σ_i²)`；实际用 Eq(26) `f_MLE' = (Σ_i P_i O_i)/(Σ_i P_i²)`；
  Gaussian PSF 下 `Var(f_PSF) = 4πα²n²`（前提：忽略源自身噪声）。
- **是否扣背景 / 天光如何进入**：**天光均值被扣**（`O_i` 逐字 "the observed intensities, with the sky
  background subtracted"）；**天光散粒噪声进分母**（`n²` 逐字 "the per-pixel variance of empty parts of
  the frame (a combination of read noise, dark current, and **photon noise from the sky**)"）；
  **源自身泊松不进**（论文以"可忽略"为前提）。frames 做**两级**天光扣除（整帧 global → 局部重估）。
- **借鉴点**：① 用**空白天区方差**而不是被源污染的逐像素方差 —— 论文给了明确理由（否则会引入
  **亮度相关的系统偏差**）；② `n²` 的分项清单（读出+暗流+天光光子）可直接作为方差分项模板。
- **不借鉴点**：① 忽略源自身泊松；② 该文是**草稿**（正文含 `(XXX ...)` 占位），引用须声明；
  ③ 常数 `n²` 不表达天光空间结构。
- **场景差异**：SDSS 是 TDI 漂移扫描 + 专用测光望远镜；ACSD 单帧、无专用定标硬件。
- **核对状态**：**已核对**（PDF 下载 + pypdf 抽文定位 §5.1/§10.3.1；sdss4.org 与 data.sdss.org HTTP 200）。
  **待核对**：CAS `PhotoObj.psfFluxErr` 列文档（`skyserver.sdss.org` 本环境不可达：TLS unexpected eof / 502）。

### [A4] DES DM（DR1 arXiv:1801.03181；DR2 arXiv:2101.05765）+ DES 官方 SExtractor fork

- **SNR 定义**：**DES 不定义、也不发布 SNR 标量**，用 SExtractor 的 `FLUXERR`/`MAGERR`；
  用户经 Pogson 微分反推 `δm = −(2.5/ln10)(δF/F)`（DR1 §4.4.2 逐字）。
- **是否扣背景 / 天光如何进入**：DES 官方 fork 给出 SExtractor 权威式
  `FLUXERR = sqrt(Σ_{i∈A}(σ_i² + p_i/g_i))`，逐字 "σ_i ... the standard deviation of noise (in ADU)
  **estimated from the local background**, p_i the measurement image pixel value **subtracted from
  the background**"。⇒ 均值已扣、天光散粒经 `σ_i²` 进分母、源泊松经 `p_i/g_i` 进分母且用**扣背景后**的 `p_i`。
  官方自承 "this error estimate provides a **lower limit** of the true uncertainty"。
  实测配置：`BACKPHOTO_TYPE GLOBAL`、`WEIGHT_TYPE MAP_WEIGHT`、`WEIGHT_GAIN Y`。
- **借鉴点**：① 「分子扣背景 / 分母用背景 RMS 承载天光」的分离写法；
  ② **误差是下界的显式声明**（不夸大）—— 表述纪律值得抄。
- **不借鉴点**：误差完全外包给 SExtractor 的局部背景 RMS，无逐像素解析方差；不含相关噪声/拥挤/定标误差。
- **场景差异**：DES 是 coadd 星表级；ACSD normalize 是单帧。
- **核对状态**：**已核对**（官方 fork readthedocs HTTP 200；DR1/DR2 PDF 抽文；GitHub 配置文件 raw 200 + 行号）。

### [A5] Pan-STARRS IPP / psphot（SVN r43094）+ Magnier et al. 2020, ApJS 251, 5, arXiv:1612.05244

- **SNR 定义**：字段叫 **`SN`**（不是 `SNR`），且**有三条口径不同的定义**：
  1. CFF/cmf 星表：`SN = KronFlux/KronFluxErr`（`pmSourceIO_CFF.c:361`，落盘 `:518`
     `psMetadataAddF32(row, PS_LIST_TAIL, "SN", 0, "kron flux signal to noise", SN)`）。
  2. 内部矩：`SN = Sum/sqrt(Var)`（`pmSourceMoments.c:263-264`），头文件自承
     `float SN; ///< approx signal-to-noise`（`pmMoments.h:52`）。
     **实现细节**：`pmSourceMoments.c:215` 是 `wDiff *= weight;`（**只乘一次、未平方**），
     故 `Var = Σ w σ²` 而非严格 `Σ w² σ²`。
  3. PSF 拟合：`SN = |I0/dI0|` → `psfMagErr = 1/SN`（`pmSourcePhotometry.c:141-154`）。
- **是否扣背景 / 天光如何进入**：**调用 psphot 前天光已被上游扣掉**（Magnier et al. 2020 Table 1 脚注 1
  逐字 "Background subtraction is performed by ppSub before calling psphot"）；psphot 内部 `sky ≡ 0.0`
  （`pmSourceMoments.c:143,337`）。**局部天光照测但被显式忽略**（`psphotSourceStats.c:411-413`
  逐字 "// the local sky is now ignored; kept here for reference only"）。
- **借鉴点**：① 「测量与使用分离」的纪律：测天光、落盘、但明确不参与 SNR；
  ② 天光扣除前置到上游使 `sky ≡ 0`，接口干净可审计。
- **不借鉴点**：① `Var = Σ w σ²`（权重不平方）**数学上不是加权和的方差**，会系统性高估 SNR
  （严格应为 `Σ w_i²σ_i²`，LSST/JWST 都是平方形式）；
  ② **同名不同义**（CFF 的 `SN` 是 Kron、RAW 的 `SN` 是矩）；
  ③ 论文公式含 `s_i`（局部天光）但实现 `sky ≡ 0`，**文档/实现有落差**。
- **场景差异**：psphot 面向 PS1 3π 多类产品，天光由上游统一扣；ACSD 必须自己建模天光。
- **核对状态**：**已核对**（SVN `export/HEAD` 原文 HTTP 200 + `nl -ba` 行号；论文标题/DOI 经 arXiv API + Crossref
  `10.3847/1538-4365/abb82c` 双核）。**部分核对**：方差图是否含天光泊松仅由
  `psphotAddNoise.c:71-73` 注释 `weight = flux/gain + rn^2/g^2` 间接支持。

### [A6] HSC / HSCPipeline（Bosch et al. 2018, arXiv:1705.06766）

- **SNR 定义**：**不定义任何帧级 SNR 标量。** 只有：
  ① 逐像素检测显著性 Eq(10) `ν = (1/(σ√A))Σ z φ`，`A = Σ φ²`；
  ② 逐源 matched-filter Eq(28) `α_MF = (Σ φ_i z_i)/(Σ φ_i²)`、Eq(30) `σ_MF² = (Σ φ_i²σ_i²)/(Σ φ_i²)²`。
  论文**从未写出 `α_MF/σ_MF` 这个比值**。CModel 与 SdssShape **不定义误差**（全文 `fluxErr` 0 次）。
- **是否扣背景 / 天光如何进入**：分子已扣背景（calexp 逐字 "the detrended, **background-subtracted** image"）；
  分母 Eq(32) `σ_i² = b + α(φ_i + ε_i)`，逐字 "where **b is the level of the background (before it is
  subtracted)**"。⇒ 均值已减、散粒噪声进分母；matched filter **不做 1/σ_i² 加权**（论文给了物理理由）。
- **借鉴点**：① 把「背景均值」与「背景噪声」分得很干净（方差用**减除前**的背景水平构造）；
  ② 对省略项（质心误差）显式声明。
- **不借鉴点**：① 没有帧级 SNR 标量，**不能声称沿用 HSC 定义**；
  ② CModel/SdssShape 无误差定义，不能作模型测光 fluxErr 的文献依据。
- **场景差异**：HSC 分 coadd 与 visit 两层；ACSD normalize 是单帧。
- **核对状态**：**已核对**（ar5iv 全文逐字复核 Eq(28)/Eq(30)/Eq(32)/calexp 四处）。
  **待核对**：PASJ 卷页 "70, S5"（arXiv Journal-ref 为空；OUP 403 Cloudflare）。

### [A7] JWST 管线 `source_catalog`（jwst 3.1.0.dev97+g07ac8cc5e）

- **SNR 定义**：**该步骤不定义、也不输出任何 SNR 量**（`source_catalog.py` 全文 `snr` **0 命中**）。
  唯一的 `snr_threshold`（默认 3.0）是**检测阈值参数**：`threshold_img = snr_threshold * bkg.background_rms`。
- **是否扣背景 / 天光如何进入**：误差来自 drizzle 后的 `ERR` 扩展 —— 官方逐字
  "Photometric errors are calculated from the resampled total-error array contained in the `ERR`
  (`model.err`) array. Note that this total-error array includes source Poisson noise."
  孔径误差 = `sqrt(Σ w_i²σ_i²)`（photutils `aperture/core.py:917-925`）。
  局部背景用 annulus sigma-clipped median 扣除，其**标准误 `sqrt(π/(2N))·std` 单独成列 `aper_bkg_flux_err`，
  不并入 flux_err**。⇒ 天光散粒进分母、均值不进分子。
- **借鉴点**：① 「误差数组 = 总误差、含源泊松、与 data 同形同单位」写成**显式输入契约**；
  ② 局部背景的**不确定度单独成列**，不偷偷混入 flux_err（可追溯性好）。
- **不借鉴点**：① 无 SNR 输出；② flux_err **不含**局部背景扣除的不确定度；
  ③ 误差正确性完全依赖上游 ERR，本步不做任何误差自检 —— ACSD 没有上游，不能照搬。
- **场景差异**：输入是已定标、已减背景、已 drizzle 的 i2d；ACSD 处理原始单帧。
- **核对状态**：**已核对**（`main.rst:44-50` 与列清单逐字复核；`source_catalog.py` 全文 `snr` 0 命中为字符串计数结论）。
  **待核对**：master 分支 commit SHA（GitHub API 返回不含 sha），以文件行数+md5+文档版本号作锚点。

### [A8] HST / DrizzlePac（Handbook §3.3/§5.2；drizzlepac main；Fruchter & Hook 2002）

- **SNR 定义**：**不定义帧级 SNR 标量。** 传播的是逐像素 weight：
  `W' = a·w + W`，`I' = (a·i·w + I·W)/W'`，**`W = 1/(Var × scale⁴)`**。
  噪声相关比 `R = r/(1−1/(3r))`（r≥1）、`r/(1−r/3)`（r≤1）。
  Fruchter & Hook 2002 全文 **`S/N` 出现 0 次**。
- **是否扣背景 / 天光如何进入**：天光散粒噪声进 weight 分母 ——
  IVM 实现 `drizzlepac/imageObject.py:784` `ivm = (flat)**2/(darkimg+(skyimg*flat)+RN**2)`；
  `getskyimg` docstring 逐字 "The value of the sky is what would actually be subtracted from the
  exposure by the skysub step."；天光**均值**在 drizzle 过程中被减掉
  （"Sky subtraction is not applied to the flt.fits images. Instead, the sky value is subtracted
  from the chip, on-the-fly, during the process of drizzling"）。
  反证天光值确实进 IVM：`sky.py:352-355` 警告 IVM 不能配 `'match'` 天光（否则 "derived weights
  will be incorrect"）。
- **借鉴点**：① `W = 1/(Var·scale⁴)` 显式写出重采样尺度对权重的四次方影响；
  ② 把权重口径做成**显式枚举**（EXP/ERR/IVM）并写清各自适用噪声域；
  ③ 官方坦承 `ERR` 权重是 minimum variance 但**非 unbiased**（"using this weight produces a small
  bias (usually no more than one to two percent)"）—— 对任何帧级 SNR 定义都是硬约束。
- **不借鉴点**：① `R` 闭式解只在特定 dither 假设下成立；② 输出只有 SCI/WHT/CTX，**没有 ERR 扩展**；
  ③ IVM 的天光/暗流/读出都是**整帧标量**（`np.ones(shape)*标量`），无法表达天光梯度。
- **场景差异**：HST 有几何畸变 + 亚像素 dither；ACSD 若不做亚像素重采样则 `scale→1`、`W = 1/Var`。
- **核对状态**：**已核对**（Confluence REST `body.view` 正文 + `imageObject.py:774-791` 逐字复核 + PDF 抽文互校）。
  **待核对**：STIS Data Handbook §2.5 的 ERR 数值定义。

---

## 3 测光工具

### [B1] SExtractor（Bertin & Arnouts 1996, A&AS 117, 393；DOI 10.1051/aas:1996164）— master = 2.29.0

- **SNR 定义**：
  - 孔径：`FLUXERR = sqrt( Σ_{i∈A} ( σ_i² + p_i/g_i ) )`（`doc/src/Photom.rst`，`:label: fluxerr`；旧手册编号式(36)）。
  - 窗口：**`SNR_WIN = FLUX_WIN / FLUXERR_WIN`**（`src/winpos.c:289`；`src/param.h:134`
    `{"SNR_WIN", "Gaussian-weighted SNR", ...}`），其中 `FLUX_WIN = Σ_i w_i I_i`（高斯窗），
    `FLUXERR_WIN = sqrt(esum)`。
  - **`FLUX_GAUSS` 不存在**（`param.h` 参数表零命中）。
- **是否扣背景 / 天光如何进入**：`p_i` 逐字是 "the measurement image pixel value **subtracted from the
  background**"；`σ_i` 是 "the standard deviation of noise (in ADU) **estimated from the local
  background**"。窗口路径的 `pix = *stript` 来自 SExtractor 的**已减背景**内部图像。
  ⇒ **分子已扣背景（含 `SNR_WIN`）；天光散粒进分母；天光均值不进分子。**
  官方警告："this error estimate provides a **lower limit** of the true uncertainty, as it only takes
  into account photon and detector noise."
- **借鉴点**：① 上式是孔径误差的规范式；② 「分子扣背景」的写法；
  ③ `SNR_WIN` 说明**窗口口径本身不会**被天光抬高（只要窗口和扣了背景）—— 反例只能是"未扣背景的窗口口径"。
- **不借鉴点**：① 误差不含天光估计自身的不确定度（`n_sky` 项缺失）；② 不含相关噪声、拥挤、定标误差；
  ③ 手册与实现不同步（`SNR_WIN` 只在源码里）。
- **场景差异**：SExtractor 是逐源星表工具，无帧级概念。
- **证据（逐字）**：`doc/src/Photom.rst`（raw.githubusercontent.com 抓取）；`src/winpos.c:230-300`；`src/param.h:44-141`。
- **核对状态**：**已核对**（官方源码 raw + 官方 rst 逐字；**未**编译运行 SExtractor 二进制，
  本环境无 `sex`/`extract` 可执行文件 —— 如实登记为「源码级核对，非二进制对拍」）。

### [B2] DAOPHOT / IRAF `phot`（Stetson 1987, PASP 99, 191, DOI 10.1086/131977）

- **SNR 定义**：

```
flux = sum - area*msky
err  = sqrt( flux/epadu + area*stdev**2 + area**2 * stdev**2 / nsky )
SNR  = flux / err
```

- **是否扣背景 / 天光如何进入**：① 天光均值进**信号分子**（`flux = sum − area·msky`），不作噪声项；
  ② 天光散粒噪声经 `area·stdev²` 进误差（`stdev` 从天空像素实测，已含散粒+读出）；
  ③ **`nsky` 自身不确定度有独立项** `area²·stdev²/nsky`。
- **借鉴点**：**`nsky` 项必须进帧级 SNR**，否则亮天空下系统性高估 SNR —— 本调研最该抄的一条。
- **不借鉴点**：`epadu`/`readnoise`/`stdev`/`sigma` 全靠用户手填（`datapars.sigma` 默认 0.0）；
  天光是单一标量 `msky`，无空间变化。
- **场景差异**：单星逐目标天空环；帧级需把"每源一环"变成"帧级天光模型 + 每源局部残差"。
- **证据（逐字）**：IRAF 官方帮助页 `https://iraf.readthedocs.io/en/latest/tasks/noao/digiphot/daophot/phot.html`
  行 515–525；源码 `https://raw.githubusercontent.com/iraf-community/iraf/main/noao/digiphot/apphot/phot/apcomags.x`
  行 28–55（`err1 = areas[i]*sigma**2` / `err2 = mags[i]/padu` / `err3 = sigma**2*areas[i]**2/nsky`）。
- **核对状态**：**已核对**（IRAF 官方帮助页 3 处 + 独立镜像 1 处 + IRAF 官方源码 3 个文件，文档与源码一致）。
  **待核对**：**Stetson 1987 原文正文**（OpenAlex 记录 `oa_status: closed`、`any_repository_has_fulltext: false`；
  ADS 405 WAF、scixplorer 202 JS 挑战、IOPscience 反爬、Massey PDF 本工具链不解析）——
  **不冒充原文逐字引用**，只引 IRAF 实现级公式。

### [B3] DAOPHOT / IRAF `allstar`（PSF 拟合分支）

- **SNR 定义**：`merr` 来自非线性最小二乘的参数协方差矩阵；逐像素方差有显式公式：

```
error = sqrt(term1 + term2 + term3 + term4)
term1 = (readnoise/epadu)**2 ; term2 = I/epadu
term3 = (.01*flaterr*I)**2   ; term4 = (.01*proferr*M/p1/p2)**2
```

- **是否扣背景 / 天光如何进入**：天光散粒**进** `term2 = I/epadu`，其中 `I` 是**含天光**的整像素强度
  （与孔径分支相反）；天光均值作为 `I` 的一部分进分子，但轮廓插值项用 `M = I − sky` 显式减掉。
- **借鉴点**：「读出噪声² + 总强度/gain + 平场误差² + 轮廓插值误差²」的四项结构，
  以及用**协方差对角元**而非 `sqrt(Σσ²)` 给误差。
- **不借鉴点**：`flaterr=0.75%`/`proferr=5.0%` 是**用户给的百分数常数**（拍脑袋），照抄等于把常数写进科学定义。
- **场景差异**：星团级拥挤场、按 group 联合拟合。
- **核对状态**：**已核对**（`daopars.html:323-334` + `allstar/dpalphot.x:571-613` 逐字一致）。

### [B4] PSFEx（Bertin 2011, ASP Conf. Ser. 442, 435；文档 3.18.2）

- **SNR 定义**：**PSFEx 不定义自己的 SNR，也不输出任何误差面/误差列。** 它**消费**输入星表的
  `SNR_WIN` 作样本筛选；输出星表的 `snr` 列是从输入星表**原样拷贝**（`src/sample.c:567-572, 794`）。
  唯一自有的逐像素方差式是拟合权重（方程 3）`σ_i² = σ_b² + p_i/g + (α·p_i)²`，不对外输出。
- **是否扣背景 / 天光如何进入**：`σ_b²` 是**局部背景像素方差**（从星表头 `SEXBKDEV` 读入），
  天光散粒进 `σ_b²`；源散粒进 `p_i/g`，`p_i` 逐字 "recorded **above the background**"；天光均值不进。
- **借鉴点**：三分结构（背景方差 + 源散粒 + 相对系统项）干净；背景方差**从帧本身估出**而非用户手填。
- **不借鉴点**：**完全不解决"输出误差"问题**，不能当 SNR 语义权威；
  噪声/增益来源外包给星表头关键字，缺关键字直接 `EXIT_FAILURE`。
- **场景差异**：离线 PSF 建模工具，工作对象是星表 VIGNET 小图。
- **核对状态**：**已核对**（readthedocs 三页 + GitHub 源码 `catout.h`/`sample.c` 交叉印证）。

### [B5] photutils 3.0.0（本机实跑）

- **SNR 定义**：`aperture_sum_err = sqrt(Σ_{i∈A} w_i² σ_tot,i²)`；`ApertureStats.sum_err = sqrt(Σ_{i∈A} σ_tot,i²)`。
  **不提供 SNR 列**，需自己写 `sum/sum_err`。
- **是否扣背景 / 天光如何进入**：`aperture_photometry` **不做任何背景扣除**，文档逐字要求 `data`
  "should be background-subtracted"；天光进误差的**唯一**途径是调用方自己构造的 `error` 数组；
  **没有 `nsky` 概念**，天空环自身不确定度**默认不传播**。
  `calc_total_error` 给出 `σ_tot = sqrt(σ_bkg² + I/g_eff)`。
- **借鉴点**：`error` 语义定义得极干净（per-pixel 1σ 总误差，含源泊松），把噪声建模责任显式交给调用方。
- **不借鉴点**：无内建天空环不确定度传播 —— 直接用会漏项。
- **本机实测**（`run/reverse_verify/frame_snr/external_crosscheck.json`）：
  - `aperture_sum_err`（center 法）= `sqrt(Σ_{i∈A}σ_i²)` **逐位一致（rel_diff = 0.0）**，
    且像素集合用**严格** `r_i < r`（109 px，而非 `≤` 的 113 px）；
  - 加常数天光 `C=1234.5` 后扣背景的 `aperture_sum` 相对变化 **3.1e-15**；
    未扣背景则精确增加 `C·n_pix`（134560.5 = 1234.5×109，预测一致）；
  - 天光 σ 增大 ⇒ `aperture_sum_err` 严格增大（75.54 → 124.57 → 337.07，B = 10/100/1000 e-/pix）；
  - 同口径解析式与 photutils 的 SNR **rel_diff = 0.0**（差异全部来自口径约定：πr² vs 整数像素、`n_pix/n_sky` 项）。
- **核对状态**：**已核对**（官方文档 + 本机 3.0.0 源码 + 本机实跑，三者一致）。

### [B6] `sep` 1.4.1（本机实跑）

- **SNR 定义**：`sumerr = sqrt(Σ_pixel var_i + (sum/gain 若 gain>0))`；**不返回 SNR**。
- **是否扣背景 / 天光如何进入**：`bkgann` 时背景 = 环内未掩膜像素**平均值**，扣除量 `bkg_mean*area`；
  天光均值进信号（被减掉）；**天空环自身不确定度进 `sumerr`**
  （`sep.pyx:1072-1073` `bkgfluxerr = bkgfluxerr/bkgarea*area; fluxerr1 = sqrt(fluxerr1² + bkgfluxerr²)`）
  —— **这是 sep 比 photutils 强的地方**。
  **但**：Poisson 项 `sumvar[j] += sum[j]/im->gain`（`src/aperture.c:564-571`）用的是**未扣天光的孔径原始总和**，
  而 `bkgann` 分支只改了 flux、没重算 Poisson 项 ⇒ **天光散粒被重复计入两次**（真实语义缺陷）。
- **借鉴点**：「背景环的不确定度必须加到孔径误差上」这条 sep 做对了，实现只有两行。
- **不借鉴点**：Poisson 项用含天光原始总和（重复计入）；不返回 SNR；默认 `subpix=5` 是近似面积。
- **本机实测**：`sep.sum_circle(..., err=<per-pixel σ>, bkgann=(10,16))` → flux 1488.27 / fluxerr 216.87 / SNR 6.86
  （与 canon 的 8.42 不同口径：sep 用 `subpix=5` 近似面积且 Poisson 项含天光）。
- **核对状态**：**已核对**（官方 readthedocs + GitHub 官方源码 `sep.pyx`/`src/aperture.c` + 本机实跑）。
  **注意**：`sep.Background(data, 64, 64)` 在 1.4.1 会报错 —— 第 2 位是 `mask`，必须用 `bw=`/`bh=` 关键字。

---

## 4 点源 SNR 理论

### [C1] Horne, K. (1986). An optimal extraction algorithm for CCD spectroscopy. PASP 98, 609. DOI 10.1086/131801

- **SNR 定义**（最优提取）：`σ_F^-2 = Σ_i P_i²/σ_i²`，`SNR_F = F/σ_F`。
- **是否扣背景 / 天光如何进入**：天光**均值**在提取前已从数据里扣除（提取的对象是扣背景后的谱）；
  天光**散粒**经 `σ_i²` 进分母。⇒ 均值不进分子、散粒进分母。
- **借鉴点**：**ACSD 逐源/帧级 SNR 的规范式就是这一条**；`1/ΣP_i² = A_NEA` 是它的白噪声特例。
- **不借鉴点**：光谱（一维、按波长 bin）设定；`P_i` 已知且不随波长变的假设；不考虑相关噪声。
- **场景差异**：ACSD 是二维成像、`P` 用解析 Moffat4 近似。
- **证据（逐字）**：Crossref `https://api.crossref.org/works/10.1086/131801` 逐字
  `"title":["An optimal extraction algorithm for CCD spectroscopy"],"container-title":["Publications of the Astronomical Society of the Pacific"],"volume":"98","page":"609","published-print":{"date-parts":[[1986,6]]}`。
  **正文等式未逐字抓取**（PASP 闭源）—— 公式形式引自 ACSD 前轮 `bibliography.md` 与本仓 `snr_science.cpp:4` 的实现注释，
  **本条目只对书目记录做一手核对**。
- **核对状态**：**书目已核对（Crossref）**；**正文等式号 待核对**。

### [C2] Naylor, T. (1998). An optimal extraction algorithm for imaging photometry. MNRAS 296, 339–346. DOI **10.1046/j.1365-8711.1998.01314.x**

- **SNR 定义**：成像版最优提取（把 Horne 的一维最优提取推广到二维成像，给出最优孔径/加权口径）。
- **是否扣背景 / 天光如何进入**：同 Horne —— 均值扣、散粒进分母。
- **借鉴点**：**"最优孔径"在成像情形下的规范引用**；论证"最优提取 ≠ 固定孔径求和"。
- **不借鉴点**：未逐字核对正文；具体等式 `[UNVERIFIED]`。
- **场景差异**：地面成像、PSF 由星点估计。
- **证据（逐字）**：Crossref 检索命中
  `10.1046/j.1365-8711.1998.01314.x | ['An optimal extraction algorithm for imaging photometry'] | ['Monthly Notices of the Royal Astronomical Society'] | 296 | 339-346`。
  **⚠️ 任务书给的 DOI `...01407.x` 是另一篇论文**（Crossref 逐字：*"Deep hard X-ray source counts from a
  fluctuation analysis of ASCA SIS images"*, MNRAS 297, 41）—— 必须用 `...01314.x`。
- **核对状态**：**书目已核对（Crossref 检索命中，标题/卷/页一致）**；**正文 待核对**。

### [C3] Irwin, M. J. (1985). Automatic analysis of crowded fields. MNRAS 214, 575–604. DOI **10.1093/mnras/214.4.575**

- **SNR 定义**：拥挤场自动分析（DAOPHOT 血统的测光误差式）。
- **是否扣背景 / 天光如何进入**：待核对（正文未取得）。
- **借鉴点**：拥挤场测光误差与星点检测的规范引用；常被引作"等效噪声面积/最优加权"的先驱。
- **不借鉴点**：**未能定位 `A_NEA = 1/ΣP_i²` 的一手出处**（见 C5）。
- **场景差异**：照相底片/早期 CCD 拥挤场。
- **证据（逐字）**：Crossref 检索命中 `10.1093/mnras/214.4.575 | ['Automatic analysis of crowded fields'] | MNRAS | 214 | 575-604`。
  **⚠️ 任务书给的 `10.1093/mnras/214.3.575` 在 Crossref 无记录** —— 正确是 `.4.575`。
- **核对状态**：**书目已核对（Crossref）**；**正文与误差式 待核对**。

### [C4] Zackay, B. & Ofek, E. O. (2017). How to COAAD Images. I./II. ApJ 836, 187/188. arXiv:1512.06872 / 1512.06879

- **SNR 定义**：点源最优组合**不是**逆方差加权平均，而是 **PSF 匹配滤波**后的组合；
  `ΣwᵢIᵢ/Σwᵢ` 只对**面亮度型**信号最优。
- **是否扣背景 / 天光如何进入**：全文工作在 background-dominated noise limit（天光主导），
  天光进噪声功率谱；均值不进信号。
- **借鉴点**：**"帧级 SNR 是点源（PSF）SNR，不是面亮度 SNR；两者不可混用"** 的一手论据；
  也是"逆方差换算 `w = SNR²/F_ref²` 只在 PSF 齐化后严格成立"的论据。
- **不借鉴点**：高斯噪声假设、PSF 精确已知假设；background-dominated 极限。
- **场景差异**：ACSD 的 UPM 同时拟合 `g_k` 与空间场，不是单纯图像组合。
- **核对状态**：**沿用本仓 `bibliography.md §1.2/§1.3` 的 `[CR, arXiv]` 核对结果**（前轮已核 DOI 10.3847/1538-4357/836/2/187 与 arXiv:1512.06872）；**本轮未重复抓取**（arXiv API 本轮返回解析失败，如实登记）。

### [C5] 等效噪声面积 `A_NEA = 1/Σ_i P_i²` — **未定位到一手出处**

- **结论（诚实登记）**：本轮**未能**把 `A_NEA = 1/ΣP_i²` 定位到某一篇一手文献的逐字定义。
  它在数学上就是 Horne 1986 最优提取在白噪声（`σ_i = σ` 常数）下的直接推论：
  `σ_F^-2 = ΣP_i²/σ² ⇒ σ_F = σ/√(ΣP_i²) = σ·√A_NEA`，
  并且 LSST SMTN-002 的 `n_eff = 2.266(FWHM/pixelScale)²` 是同一量的解析近似（**不同符号、不同推导**）。
- **借鉴点**：**可以用，但引用方式必须是"由 Horne 1986 直接推出"，不得安给某一篇"提出 A_NEA 的论文"。**
- **判定方法（待办）**：查 Howell 2006 *Handbook of CCD Astronomy* 2nd ed. 的 "effective noise area" 索引项，
  与 Naylor 1998 正文（本轮均未取得）。**在定位到之前，本仓一律写成"定义 + 由 Horne 1986 推出"。**
- **核对状态**：**待核对（未定位一手出处）**。

### [C6] Labbé et al. 2003 — **任务书点名的这篇与主题不符 / 未核对**

- **结论（诚实登记）**：任务书写"如 Irwin 1985、Naylor 1998、Labbe et al. 2003 关于最优孔径与 SNR"。
  本轮**未能**核对到 Labbé et al. 2003 中与"最优孔径/SNR"相关的任何内容；**未收录**，不编造。
- **判定方法（待办）**：在 ADS/Crossref 按 `Labbé 2003 AJ` 检索候选（AJ 125, 1107 等），
  逐篇核对其摘要是否含 aperture/optimal/SNR；命中后再按本格式补录。**在核对前不得引用。**
- **核对状态**：**待核对（未收录）**。

---

## 5 关键对照：哪些定义会被天光抬高（逐条）

### [D1] 未扣背景的通量型 `(F + n_pix·B)/σ`

- **结构**：分子含天光基座 `n_pix·B`，分母只随 `√B` 增长。
- **后果**：`B→∞` 时 `SNR → n_pix·B/√(n_pix·B) = √(n_pix·B) → ∞`。**SNR 随天光上升而上升。**
- **一手依据**：SExtractor 与 DAOPHOT 都**显式**把背景从分子扣掉（`p_i` "subtracted from the background"；
  `flux = sum − area·msky`）—— 反证"不扣"是错的。
- **本仓数值证据**：`run/reverse_verify/frame_snr/redlines_physical.json` 的 `P10` 红例 A
  （B = 0/10/100/1000/10000 e-/pix ⇒ 23.81/31.49/95.69/333.7/1031，**严格上升 ⇒ 否决**）。

### [D2] 功率比型（分子是**方差/和的平方**）—— PixInsight "standard SNR" 式[20]

- **结构**：`SNR² = σ²/σ_n²`（全局尺度估计），**分子是图像方差**。
- **一手依据（逐字，PixInsight 官方《New Image Weighting Algorithms》2.7 节）**：
  > "The SNR estimator is defined as [20] where is a scale estimate calculated for the image and is,
  > as before, an estimate of the standard deviation of the noise. **This corresponds to the ratio of
  > powers standard formulation of signal-to-noise ratio.**"
  以及同页 Figure 9 说明：
  > "The frame at index 13 ... has a big airplane trail that introduces a **strong bias in the variance
  > used as the numerator of the SNR equation** (see Equation [20])."
  ⇒ **官方自己承认：任何加性图像内容（尾迹、天光、梯度）会抬高式[20] 的分子。**
- **PixInsight PSFSNR（式[18]）**：官方逐字 "PSF SNR is a realization of the ratio of powers paradigm"；
  其分子是**各星 PSF 通量之和的平方**（`(Σf)²`），而 `f` 来自"FWTM 孔径内像素**减局部背景**求和"
  ⇒ **PSFSNR 的分子已扣背景**，故它**不会**被天光均值抬高（天光只进 `σ_n`）。
  **注意**：页面公式是**图片**，本轮**未能逐字提取符号**（`c3`/`c4` 常数见本仓
  `docs/science/PSF_SIGNAL_WEIGHT.md:44` 的既有记录，引用须带版本）。
- **本仓数值证据**：`P10` 红例 B（同 D1 数值，**严格上升 ⇒ 否决**）。

### [D3] `SNR_WIN` 类窗口口径在强背景下的行为

- **结论**：**SExtractor 的 `SNR_WIN` 本身不会被天光抬高** —— 因为它的窗口和取自**已减背景**的图像，
  且 `FLUXERR_WIN` 的分母含天光散粒（`σ_i²`）。
- **会被抬高的变体**：**未扣局部背景**的窗口和 `Σ_i w_i I_i`（含天光基座）。
- **本仓数值证据**：`P10` 红例 C（B = 0/10/100/1000/10000 ⇒ 41.53/42.44/81.67/235.4/711.9，**上升 ⇒ 否决**）。
- **借鉴点**：判定一个口径是否安全，只需问一句 —— **分子里有没有天光均值？** 有 ⇒ 必被抬高。

### [D4] 本仓的红线判据（可复跑）

`run/reverse_verify/frame_snr/redlines_physical.json` 的 `P10` 用**同一个** `monotone_criterion()`
同时判绿例（canon）与红例：绿例严格下降（28.37→2.591），三个红例严格上升 ⇒ **否决**。
**能红能绿**由"同一函数 + 同一物理数据"保证。

---

## 6 本轮明确**未**收录 / 未做（诚实登记）

1. **Stetson 1987 原文正文与误差式** —— 待核对（闭源，见 B2）。
2. **Naylor 1998 / Irwin 1985 正文与等式号** —— 待核对（仅核了书目）。
3. **Labbé et al. 2003** —— 未核对、未收录（见 C6）。
4. **`A_NEA = 1/ΣP_i²` 的一手出处** —— 未定位（见 C5）。
5. **与 SExtractor 二进制对拍** —— **未做**：本环境无 `sex`/`extract` 可执行文件；
   只做了**源码级**核对（`src/winpos.c:289`、`doc/src/Photom.rst`）。
6. **编译运行 IRAF / PSFEx** —— 未做（文档 + 源码阅读）。
7. **HST 数据** —— 本工作区**无哈勃数据**（全仓 `find -iname '*hst*' / '*hubble*'` 命中 0）；
   真实数据作底改用真实实拍 M42 帧（见 `frame-snr-canon.md` §3 P13）。
8. **PixInsight 式[18]/[20] 的逐字符号** —— 页面公式为图片，未提取（见 D2）。
9. **PixInsight 常数版本漂移** —— 沿用本仓既有记录，本轮未重核。
