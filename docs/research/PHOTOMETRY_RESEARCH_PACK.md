# 研究包：测光标定（Gaia XP × 系统响应 → 测光星等坐标系）

> ID: RESEARCH-PHOT-001（DOC-404 新建）
> 状态: ACTIVE_INFORMATIVE（研究包只承载一手出处、方法学对照与核验留痕；**不定义公式、常数与门限**）
> 上游: `ASTROCS_DESIGN.md` §2.1（创新点一）、§4.2（Phase1 节点流程：两轮 WCS + 星表引导检测 + apply photometry）、§4.4（测光输出语义）、附录 B
> 合同权威: `docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN）；模块细则: `docs/plugins/algorithms_phase1/06_photometry.md`
> 消费方: SCI-401 / SCI-402（实验单元一与其误差预算）、DOC-403（文档索引门）
> 依据: AGENTS.md §5（三重佐证）、§8（科学查证流程）、任务书 `DOC-404`

---

## 1. 本包用途与边界

- **用途**：为「把单帧图像校准到测光星等坐标系」这条链（Gaia DR3 XP 逆映射定位 → 星点测光 → 光谱×QE×透过率正向合成 → 拟合标定 → 落到像素 → 星等表达）提供**可核验的一手出处**与开源对照，使方法学不建立在对商业软件或二手转述的依赖上。
- **不是**：不定义 AstroCS 的公式、容差与门限（那是 `docs/science/` 与 `docs/algorithms/` 的权威）；不复制任何 GPL 代码；不给出实验数值（数值属 SCI-401）。

## 2. 核验口径与标记

每条引用必须落在下列可复跑核验方式之一；核验记录与原始响应存 `run/DOC-404/evidence/`（见 §10）。

| 标记 | 含义 | 核验方式（本包执行） |
|---|---|---|
| `[DOI]` | DOI 可解析且元数据匹配 | `api.crossref.org/works/<doi>` 返回 title/卷/页/年与所引一致 |
| `[ARXIV]` | arXiv 可解析 | `arxiv.org/abs/<id>` HTTP 200 + arXiv API 返回标题匹配 |
| `[URL]` | 官方文档 URL 可达 | `curl` HTTP 200，正文含所引要点（关键原文摘录见 §10） |
| `[OSS]` | 开源项目可定位到 项目+版本+文件:行 | 指定 tag 的 raw 文件下载后按行定位（`run/DOC-404/evidence/oss_line_anchors.txt`） |
| `[NONE]` | 未找到一手出处 | 明确登记，不编造；转 SCI 任务处理 |

**引用纪律**：GPL 项目（SExtractor / SCAMP / SWarp / Siril / healpy）**只读对照，不复制代码进仓库**；本包只给位置锚与行为描述。

---

## 3. Gaia DR3 XP：连续谱/均值光谱表示、采样与定标

### 3.1 官方文档（一手，ESA/DPAC）

| # | 出处 | 核验 | 要点（与 AstroCS 相关的可核验原文/事实） |
|---|---|---|---|
| G1 | Gaia DR3 文档 §20.12.3 `xp_continuous_mean_spectrum`：https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_continuous_mean_spectrum.html | `[URL]` HTTP 200（54 961 B） | 均值 BP/RP 谱以**基函数连续表示**（basis functions，指向 §5.3.4）；表字段含 `bp_n_parameters`（LSQ 参数个数）、`bp_coefficients`、`bp_coefficient_errors`、`bp_coefficient_correlations`（上三角相关阵，列主序）、`bp_n_relevant_bases`、`bp_chi_squared`；该表不经主 TAP 接口，经 Massive Data / VO Datalink 交付 |
| G2 | Gaia DR3 文档 §20.12.4 `xp_sampled_mean_spectrum`：https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_spectroscopic_tables/ssec_dm_xp_sampled_mean_spectrum.html | `[URL]` HTTP 200（30 402 B） | **采样定标原文**：“All mean spectra are sampled to the same set of absolute wavelength positions, viz. **343 values from 336 to 1020 nm with a step of 2 nm**”；连续谱与采样谱不是一一都有，采样表示可由连续谱经 GaiaXPy 生成 |
| G3 | Gaia DR3 文档 §5.3 BP/RP spectroscopic processing（5.3.1 背景定标 / 5.3.2 拥挤度 / 5.3.3 几何定标 / 5.3.4 内定标 / 5.3.5 外定标 / 5.3.6 DR3 星表内容）：https://gea.esac.esa.int/archive/documentation/GDR3/Data_processing/chap_cu5pho/cu5pho_sec_specProcessing/ | `[URL]` HTTP 200（各节 27–39 kB） | 处理链的官方权威描述：内定标（基函数连续表示）→ 外定标（仪器响应模型 / SED 重建 / 与外部测光对比）；§5.3.6 说明连续谱与采样谱的可得性差异、GaiaXPy 工具入口 |
| G4 | Gaia DR3 光度系统与 passbands：https://www.cosmos.esa.int/web/gaia/dr3-passbands | `[URL]` HTTP 200（85 431 B） | DR3 passband 集 = 已发布的 G、G_BP、G_RP 加 G_RVS；G/BP/RP 曲线同时适用于 EDR3 与 DR3；并给出 DR1 用的 nominal（pre-launch）passband（Jordi et al. 2010）以区分“nominal vs 在轨实测” |
| G5 | GaiaXPy 官方文档：https://gaia-dpci.github.io/GaiaXPy-website/ | `[URL]` HTTP 200 | DPAC 官方工具：连续表示 ↔ 采样谱互转、由输入 SED 模拟 Gaia 谱；对应 G2 中“采样表示可由连续谱得到”的官方说法 |
| G6 | Gaia ESA Archive：https://gea.esac.esa.int/archive/ | `[URL]` HTTP 200 | 数据交付入口（离线星表与 XP 谱文件的来源面） |

### 3.2 论文（XP 处理与定标）

| # | 引用 | 核验 | 定位 |
|---|---|---|---|
| G7 | Gaia Collaboration, Vallenari, A., et al. 2023, *Gaia Data Release 3: Summary of the content and survey properties*, A&A **674**, A1 | `[DOI]` 10.1051/0004-6361/202243940；`[ARXIV]` 2208.00211 | DR3 总览与内容清单（XP 谱、光度、星表规模） |
| G8 | Montegriffo, P., et al. 2023, *Gaia Data Release 3: External calibration of BP/RP low-resolution spectroscopic data*, A&A **674**, A3 | `[DOI]` 10.1051/0004-6361/202243880；`[ARXIV]` 2206.06205 | **XP 外定标主论文**：仪器响应模型、SED 重建、与外部测光对比；XP 谱定标不确定度的权威出处 |
| G9 | De Angeli, F., et al. 2023, *Gaia Data Release 3: Processing and validation of BP/RP low-resolution spectral data*, A&A **674**, A2 | `[DOI]` 10.1051/0004-6361/202243680；`[ARXIV]` 2206.06143 | XP 处理与验证链（含连续表示/采样表示与验证口径） |
| G10 | Carrasco, J. M., et al. 2021, *Internal calibration of Gaia BP/RP low-resolution spectra*, A&A **652**, A86 | `[DOI]` 10.1051/0004-6361/202141249；`[ARXIV]` 2106.01752 | XP **内定标**：基函数连续表示的构造与标定（对应 G3 §5.3.4） |
| G11 | Gaia Collaboration, Brown, A. G. A., et al. 2021, *Gaia Early Data Release 3: Summary of the contents and survey properties*, A&A **649**, A1 | `[DOI]` 10.1051/0004-6361/202039657；`[ARXIV]` 2012.01533 | EDR3 总览（passband 与 XP 的发布背景） |
| G12 | Riello, M., et al. 2021, *Gaia Early Data Release 3: Photometric content and validation*, A&A **649**, A3 | `[DOI]` 10.1051/0004-6361/202039587；`[ARXIV]` 2012.01916 | G/BP/RP 积分光度的内容与验证（与 XP 谱相互独立的定标面） |

> **消歧记录（防误引）**：arXiv **2206.06207** 不是 Montegriffo XP 论文（arXiv API 实测标题为 “Gaia Data Release 3: Mapping the asymmetric disc of the Milky Way”）；XP 外定标论文的正确 arXiv 号是 **2206.06205**（G8）。凡引用 XP 定标必须用 G8 的 DOI/arXiv。

### 3.3 对 AstroCS 的用法与边界（与最高设计 §2.1/§4.2 一致）

- 本项目消费的是 XP 谱的**形状/相对刻度**，用它做**模型通带内的正向合成**（`F_syn = ∫F_λ·T(λ)·Q(λ)·λ dλ`，公式权威在 `docs/science/PHOTOMETRY.md` §2/§5），**不转述、不重推** Gaia 的绝对定标推导（PHOTOMETRY.md §14 已声明“未逐页核验”）；
- **采样口径**：DR3 官方采样网格为 336–1020 nm、步长 2 nm、343 点（G2）。该网格是**固定绝对波长位置**，因此正向合成必须在该网格上做插值/积分并把**网格外（<336 nm、>1020 nm）无数据**显式当作未建模项，不得外推当有效；
- **可得性**：连续表示与采样表示不保证同时存在（G2/G3 §5.3.6），消费侧必须显式记录所用表示与来源版本；
- **通带外推风险**：本帧滤光片通带若显著超出 XP 覆盖或落在其低信噪端，`F_syn` 的颜色项误差上升；该项在误差预算中单列（§7 第 6 条）。

---

## 4. 恒星光谱 × 系统响应（CCD QE × 滤镜透过率 × 光学响应）：合成测光与通带积分

### 4.1 标准方法与权威出处

| # | 引用 | 核验 | 定位 |
|---|---|---|---|
| P1 | Bessell, M. S. 1990, *UBVRI passbands*, PASP **102**, 1181 | `[DOI]` 10.1086/132749 | 通带定义的经典文献：滤光片+探测器（QE）+光学响应的合成透过率，以及能量计数 vs 光子计数口径 |
| P2 | Bessell, M. S., & Murphy, S. 2012, *Spectrophotometric Libraries, Revised Photonic Passbands, and Zero Points for UBVRI, Hipparcos, and Tycho Photometry*, PASP **124**, 140 | `[DOI]` 10.1086/664083 | **photonic passband**（光子计数口径的修正通带）与零点；给出合成测光中 `λ` 权重因子与零点关系的权威处理 |
| P3 | Fukugita, M., et al. 1996, *The Sloan Digital Sky Survey Photometric System*, AJ **111**, 1748 | `[DOI]` 10.1086/117915 | 系统响应（CCD QE × 滤光片 × 光学/大气）合成通带与 AB 零点的完整范例 |
| P4 | Sirianni, M., et al. 2005, *The Photometric Performance and Calibration of the HST Advanced Camera for Surveys*, PASP **117**, 1049 | `[DOI]` 10.1086/444553 | 端到端系统透过率（QE × 滤光片 × 光学）× 光谱 → 合成计数的工程范例与逐项预算写法 |
| P5 | Girardi, L., et al. 2002, *Theoretical isochrones in several photometric systems*, A&A **391**, 195 | `[DOI]` 10.1051/0004-6361:20020612 | 由模型大气光谱做多系统合成测光的标准流程（含响应曲线卷积） |
| P6 | Pickles, A. J. 1998, *A Stellar Spectral Flux Library: 1150–25000 Å*, PASP **110**, 863 | `[DOI]` 10.1086/316197 | 恒星分光通量库（光谱 × 响应积分所需的输入谱标准集） |
| P7 | synphot 官方文档（STScI/astropy 生态）：https://synphot.readthedocs.io/en/latest/ | `[URL]` HTTP 200 | 合成测光的公开实现规范：谱 × 通带积分、计数/能量口径、有效波长 |
| P8 | pysynphot 官方文档（STScI）：https://pysynphot.readthedocs.io/en/latest/ | `[URL]` HTTP 200 | HST 系合成测光的原始公开实现文档（与 P4 配套） |
| P9 | SVO Filter Profile Service：http://svo2.cab.inta-csic.es/theory/fps/ | `[URL]` HTTP 200 | 滤镜/仪器响应曲线的公开权威库（T(λ) 的来源面） |

### 4.2 对 AstroCS 的用法与边界

- 正向合成的**方法学**就是 P1–P9 的标准做法：恒星光谱 ×（QE × 滤镜透过率 × 光学响应）在波长上积分；
- 本项目的**现状差异必须显式声明**：生产取 `Q(λ)≡1`（QE 未建模），因此得到的是“模型通带相对刻度”，颜色项误差单列（见 `docs/plugins/algorithms_phase1/06_photometry.md` §4.1 的 `σ_color`）；
- 通带内积分与光子计数口径的选择由 SCI 合同决定（`F_syn=∫F_λ·T·Q·λ dλ`），本包只提供该口径的标准文献依据（P2 的 photonic passband 讨论）；
- **禁止**把合成通量的绝对归一常数（`1/(hc)` 等）当科学量：常数被标定因子吸收（`docs/science/PHOTOMETRY.md` §6）。

---

## 5. 星等系统、零点、AB vs Vega、通量-星等换算

| # | 引用 | 核验 | 定位 |
|---|---|---|---|
| M1 | Oke, J. B., & Gunn, J. E. 1983, *Secondary standard stars for absolute spectrophotometry*, ApJ **266**, 713 | `[DOI]` 10.1086/160817 | **AB 星等系统**原始文献（`m_AB = −2.5·log10 F_ν − 48.60`，F_ν 以 erg s⁻¹ cm⁻² Hz⁻¹ 计；等价 3631 Jy 零点） |
| M2 | Bessell, M. S. 1979, *UBVRI photometry. II. The Cousins VRI system...*, PASP **91**, 589 | `[DOI]` 10.1086/130542 | Vega 系星等的温度与绝对通量定标 |
| M3 | Bessell & Murphy 2012（同 P2） | `[DOI]` 10.1086/664083 | UBVRI/Hipparcos/Tycho 零点与 photonic passband 的现代复算；零点不确定度写法 |
| M4 | Fukugita et al. 1996（同 P3） | `[DOI]` 10.1086/117915 | AB 零点在成像系统上的落地（SDSS） |
| M5 | Blanton, M. R., & Roweis, S. 2007, *K-Corrections and Filter Transformations in the Ultraviolet, Optical, and Near-Infrared*, AJ **133**, 734 | `[DOI]` 10.1086/510127 | 星等系统间换算/滤光片变换（AB 与 Vega 的互换与 K 修正） |
| M6 | Bohlin, R. C., et al. 2014, *HST CALSPEC Flux Standards: Sirius (and Vega)*, AJ **147**, 127 | `[DOI]` 10.1088/0004-6256/147/6/127 | Vega 绝对通量基准（Vega 系零点的现代一手依据） |
| M7 | Bohlin, R. C., Deustua, S. E., de Rosa, G., et al. 2019, *Hubble Space Telescope Flux Calibration. I. STIS and CALSPEC*, AJ **158**, 211 | `[DOI]` 10.3847/1538-3881/ab480c | HST 通量定标链与 CALSPEC 的误差口径 |
| M8 | Bohlin, R. C., Hubeny, I., & Rauch, T. 2020, *New Grids of Pure-hydrogen White Dwarf NLTE Model Atmospheres and the HST/STIS Flux Calibration*, AJ **160**, 21 | `[DOI]` 10.3847/1538-3881/ab94b4 | CALSPEC 白矮星标准的 NLTE 模型网格（当前绝对通量刻度的一手依据） |
| M9 | CALSPEC 官方页（STScI）：https://www.stsci.edu/hst/instrumentation/reference-data-for-calibration-and-tools/astronomical-catalogs/calspec | `[URL]` HTTP 200（366 kB） | 绝对通量标准星光谱的官方交付面 |
| M10 | Tokunaga, A. T., & Vacca, W. D. 2005, *The Mauna Kea Observatories Near-Infrared Filter Set. III. Isophotal Wavelengths and Absolute Calibration*, PASP **117**, 421 | `[DOI]` 10.1086/429382 | 等相波长与绝对定标（Vega→AB 换算的另一标准处理） |
| M11 | Cohen, M., et al. 2003, *Spectral Irradiance Calibration in the Infrared. XIV. The Absolute Calibration of 2MASS*, AJ **126**, 1090 | `[DOI]` 10.1086/376474 | 合成测光零点的系统性写法（网络/通带卷积与绝对刻度） |
| M12 | Oke, J. B. 1990, *Faint spectrophotometric standard stars*, AJ **99**, 1621 | `[DOI]` 10.1086/115444 | 分光光度标准星网络（零点锚定的观测面） |

**换算关系（标准式，供文档引用；本项目只用相对/星等表达）**：

```text
m        = −2.5·log10(F) + ZP                       # 通量→星等（ZP 依赖系统与通带）
m_AB     = −2.5·log10(F_ν/[erg s⁻¹ cm⁻² Hz⁻¹]) − 48.60   # Oke & Gunn 1983（M1）
F_ν = 3631 Jy ⇒ m_AB = 0                            # AB 零点定义
```

- **对 AstroCS 的约束**：本项目的标定因子把绝对归一吸收，产物只以**星等/相对星等**表达（最高设计 §2.1/§4.4），因此 M1–M12 的绝对零点只作**参考系语义**与误差预算来源，**不得**用于反解仪器参数（见 §8 与 `docs/science/PHOTOMETRY.md` §1/§10）。

---

## 6. 星表引导检测与 WCS 精化的开源对照

**权威范式（本项目）**：两轮 WCS——第一轮全图盲检测粗解，仅供星表投影；第二轮用星表引导检测得到的高纯度星表精解，精解才是权威 WCS；检测定义域 = 用本帧 WCS 把 Gaia 星表逆投影到像素域，只对星表位置做质心/PSF 拟合（`ASTROCS_DESIGN.md` §4.2；`docs/plugins/algorithms_phase1/03_star_detection.md` §4）。

| # | 项目（许可证） | 版本/tag | 入口 文件:行（`[OSS]`） | 对照什么 |
|---|---|---|---|---|
| O1 | **SCAMP**（GPL-3.0） | v2.15.0 | `src/photsolve.c:117`（`photsolve_fgroups`：全局**相对光度**解算入口）；`src/astrsolve.c:117`（`astrsolve_fgroups`：天体测量解算入口）；`src/fitswcs.c`（WCS 结构/投影）；官方页 https://www.astromatic.net/software/scamp/ `[URL]` 200 | 星表引导的**相对零点 + 天体测量**联合解算的工程结构；AstroCS 只对照“用星表做相对定标”的结构，**不引其为天光面/UPM 依据**（SCAMP 核心无像素背景归一） |
| O2 | **astrometry.net**（BSD-3-Clause 系） | 0.98 | `solver/tweak2.c:195`（`tweak2()`：以星表参考做 WCS 精化）；`solver/tweak.c:47`（`tweak_just_do_it()`：tweak 流程入口）；官方文档 https://astrometry.net/doc/ `[URL]` 200 | **盲解 + 星表精化**的完整开源对照（本项目第一轮粗解/第二轮精解的定位与它同构） |
| O3 | **SExtractor**（GPL-3.0） | 2.28.2 | `src/analyse.c:64`（`analyse()` 主测光流程）；`src/analyse.c:568`（`computeaperflux` 调用点）；`src/back.c:51`（`makeback()` 背景网格）；`src/back.c:669`（`backguess()` 背景插值）；`src/fitswcs.c:1375`（`wcs_to_raw()` 天球→像素）；官方页 https://www.astromatic.net/software/sextractor/ `[URL]` 200 | 分块背景网格 + 检测 + 孔径测光 + FLUXERR 的工程实现；AstroCS 只把孔径测光当**诊断/交叉验证**，生产口径是 PSF 拟合域（`docs/plugins/algorithms_phase1/06_photometry.md` §1/§4） |
| O4 | **SEP**（LGPL-3.0） | v1.4.1 | `src/extract.c:206`（`sep_extract()` 检测入口）；`src/aperture.c:190`（`sep_sum_circle` 宏实例化）；`src/aperture.c:263`（`sep_sum_circann` 宏实例化）；`src/aperture.c:604`（`sep_flux_radius()`）；论文 Barbary, K. 2016, JOSS 1, 58 `[DOI]` 10.21105/joss.00058 | SExtractor 算法的库化实现（背景/检测/孔径与误差传播的独立可对拍实现） |
| O5 | **photutils**（BSD-3-Clause） | 3.0.0 | `photutils/detection/daofinder.py:26`（`DAOStarFinder`）；`photutils/aperture/photometry.py:30`（`aperture_photometry()`）；`photutils/psf/photometry.py:217`（`PSFPhotometry`）；`photutils/background/background_2d.py:33`（`Background2D`） | 星检测、孔径/PSF 测光、二维背景的参考实现与误差传播；SCI-401 的数值对拍对象 |
| O6 | **astropy**（BSD-3-Clause） | v8.0.1 | `astropy/wcs/wcs.py:359`（`class WCS`）；`astropy/wcs/wcs.py:1669`（`all_pix2world()`：天球↔像素互转） | WCS 投影/逆投影的独立实现（星表逆映射定位的验证基准） |
| O7 | **SWarp**（GPL-3.0） | 2.41.5 | `src/coadd.c:292`（`coadd_fields()`：逐像素组合主入口）；`src/back.c:413`（`backstat()`）；`src/back.c:642`（`backguess()`）；官方页 https://www.astromatic.net/software/swarp/ `[URL]` 200 | 逐像素加权组合与背景统计的开源对照（Phase2 侧；Phase1 只对照背景/重采样行为） |
| O8 | **Siril**（GPL-3.0） | 1.4.4 | `src/stacking/median_and_mean.c:1091-1094`（帧权重 `1/(pscale²·bgnoise²)` 与其归一化）；仓库 https://gitlab.com/free-astro/siril `[URL]` 200 | 逆方差型帧权重 + 稳健背景噪声的工程实现；与 AstroCS「入库 SNR、Phase2 现场算权重」的区别见 `docs/research/SNR_WEIGHT_RESEARCH_PACK.md` §8 |
| O9 | **DAOPHOT / ALLSTAR**（历史实现，无源码对照） | — | 论文 Stetson 1987, PASP **99**, 191 `[DOI]` 10.1086/131977；Irwin 1985, MNRAS **214**, 575 `[DOI]` 10.1093/mnras/214.4.575；Anderson & King 2000, PASP **112**, 1360 `[DOI]` 10.1086/316632 | **星表引导/拥挤场 PSF 测光**的方法学源头：PSF 拟合测光、增长曲线、逐源不确定度；AstroCS 的“只对星表位置拟合、失败即丢弃”范式与其同族 |
| O10 | SCAMP 论文（会议集，无 DOI/arXiv） | — | Bertin, E. 2006, *Automatic Astrometric and Photometric Calibration with SCAMP*, ASP Conf. Ser. **351**, 112（ADASS XV；bibcode 2006ASPC..351..112B） | 星表引导的**天体测量+相对光度联合解算**的方法学论文。`[NONE]`：未在 arXiv 检索到、Crossref 无 DOI（ADS 对脚本访问返回 405），故只作**会议集级定位**；一手可核验面 = O1 的官方页与 v2.15.0 源码锚 |

---

## 7. 测光标定误差预算的构成文献

**用途**：为 `docs/science/PHOTOMETRY.md` §16.5 与 `docs/plugins/algorithms_phase1/06_photometry.md` §4.1 的**逐项预算**提供一手出处。**本包不给数值**；数值推导与冻结属 SCI-401。

| 预算项 | 一手出处 | 核验 | 该出处支撑什么 |
|---|---|---|---|
| ① 光子噪声（Poisson，含源+天光）与读出/量化 | Mortara & Fowler 1981, SPIE **290**, 28 `[DOI]` 10.1117/12.965833；Merline & Howell 1995, Exp. Astron. **6**, 163 `[DOI]` 10.1007/bf00421131 | `[DOI]`×2 | CCD 方程/噪声模型（信号、天光、读出、量化的方差分解） |
| ② PSF 拟合不确定度（含模型失配、采样不足、拥挤） | Stetson 1987（O9）；Irwin 1985（O9）；Anderson & King 2000（O9）；Naylor 1998, MNRAS **296**, 339 `[DOI]` 10.1046/j.1365-8711.1998.01314.x | `[DOI]` | 逐源 PSF 测光的方差来源与经验误差标定；成像最优提取的方差公式 |
| ③ 最优提取/加权下界（理论下限） | Horne 1986, PASP **98**, 609 `[DOI]` 10.1086/131801；Zackay & Ofek 2017, ApJ **836**, 187 `[DOI]` 10.3847/1538-4357/836/2/187 | `[DOI]` | `PᵀC⁻¹P` 最优提取与多帧点源信息（`Var(F)=1/W`）——预算的理论下限锚 |
| ④ 平场/大尺度响应残余 | Stubbs & Tonry 2006, ApJ **646**, 1436 `[DOI]` 10.1086/505138；Regnault et al. 2009, A&A **506**, 999 `[DOI]` 10.1051/0004-6361/200912446；Padmanabhan et al. 2008, ApJ **674**, 1217 `[DOI]` 10.1086/524677 | `[DOI]`×3 | 端到端 1% 测光的系统项分解；平场/大尺度乘性残差的量级与检验方法 |
| ⑤ 天光/背景估计残余 | Bertin & Arnouts 1996, A&AS **117**, 393 `[DOI]` 10.1051/aas:1996164；Starck & Murtagh 1998, PASP **110**, 193 `[DOI]` 10.1086/316124；Maples et al. 2018, ApJS **238**, 2 `[DOI]` 10.3847/1538-4365/aad23d | `[DOI]`×3 | 背景网格/插值残差、稳健噪声（MRS）与离群剔除对局部背景的偏差 |
| ⑥ 颜色项/通带失配（QE 未建模、XP 谱误差） | Bessell 1990（P1）；Bessell & Murphy 2012（P2）；Fukugita 1996（P3）；Sirianni 2005（P4）；Montegriffo 2023（G8）；De Angeli 2023（G9） | `[DOI]`×6 | 通带卷积对光谱形状的敏感度；XP 外定标的谱误差与通带外推限制 |
| ⑦ 星等定标误差（零点/参考网络/绝对刻度） | Bessell & Murphy 2012（P2）；Burke et al. 2017, AJ **155**, 41 `[DOI]` 10.3847/1538-3881/aa9f22；Schlafly et al. 2012, ApJ **756**, 158 `[DOI]` 10.1088/0004-637X/756/2/158；Bohlin et al. 2014/2019/2020（M6–M8） | `[DOI]`×6 | 零点/巡天定标链的误差预算写法与绝对通量刻度不确定度 |
| ⑧ 大气消光与差分消光（本项目未建模项） | Schlafly & Finkbeiner 2011, ApJ **737**, 103 `[DOI]` 10.1088/0004-637X/737/2/103 | `[DOI]` | 消光/红化的标准处理与参考；本项目只把“未建模的差分消光”列入预算与报告字段 |
| ⑨ 稳健统计与离群（预算的统计处理） | Rousseeuw & Croux 1993, JASA **88**, 1273 `[DOI]` 10.1080/01621459.1993.10476408；Beaton & Tukey 1974, Technometrics **16**, 147 `[DOI]` 10.1080/00401706.1974.10489171；Maples et al. 2018（见⑤） | `[DOI]`×3 | MAD/MedDev 尺度、Tukey biweight、稳健离群剔除（PHOTOMETRY.md §5 的稳健层） |
| ⑩ 非线性最小二乘/拟合收敛（PSF 拟合实现） | Marquardt 1963, SIAM J. Appl. Math. **11**, 431 `[DOI]` 10.1137/0111030；Akima 1970, J. ACM **17**, 589 `[DOI]` 10.1145/321607.321609（谱插值） | `[DOI]`×2 | PSF/零点拟合算法与光谱插值的标准实现依据 |

---

## 8. 对 AstroCS 的直接约束（与最高设计一致的要点）

1. **只对星点测光**，星点位置由 Gaia 星表逆映射获得；盲检测只服务第一轮粗解（`ASTROCS_DESIGN.md` §2.1/§4.2）。
2. **正向合成**用 XP 谱 × 系统响应在模型通带内积分；`Q(λ)≡1`、通带外无数据等未建模项必须显式声明，不得静默当作已建模（P4 的端到端预算写法、G2 的采样边界）。
3. **标定因子绝对值无物理意义**：`k_photo`/`scale` 吸收增益/口径/曝光/透过率等不可得量；唯一判据是**尺度无关的测光一致性**（星等残差散度），见 `docs/science/PHOTOMETRY.md` §1/§10 与 `ASTROCS_DESIGN.md` §4.4。
4. **禁止物理闭合反推**（`k = g·h·c·1e9/(A·t)` 一类）：FITS 头拿不到 g、t、A、光学透过率与大气项，方程欠定；反推等于编造未测量量（`docs/science/PHOTOMETRY.md` §10）。
5. **误差预算必须逐项带出处**（§7 十项），未测项按“不加”处理（上限偏严、fail-closed），并在报告中显式列出（`docs/plugins/algorithms_phase1/06_photometry.md` §4.1）。
6. **跨帧一致性不是门**：不同夜/不同透明度的帧标定系数不同是正常的（`ASTROCS_DESIGN.md` §2.1；`PHOT-GATE-DROP-001`）。

---

## 9. 未找到一手出处的条目（`[NONE]`，转 SCI 任务）

| 条目 | 状态 | 处置 |
|---|---|---|
| SCAMP 会议论文（O10）的 DOI/arXiv | `[NONE]`：arXiv 标题检索无结果、Crossref 无 DOI、ADS 脚本访问 405 | 只作会议集级定位（ASP Conf. Ser. 351, 112；bibcode 2006ASPC..351..112B）；方法学对照改用 O1 的 v2.15.0 源码锚与官方页 |
| 本项目生产拟合器 `Q(λ)≡1` 的具体 QE 曲线 | `[NONE]`：仓内无仪器 QE/光学曲线（`06_photometry.md` §4.1 已登记） | 属 SCI-401 数据面缺口，不由本包补；预算中按未建模项处理 |
| Gaia XP 谱在 336 nm 以下/1020 nm 以上的行为 | 官方文档只给采样范围（G2） | 已按“网格外无数据、不外推”写入 §3.3/§8；若 SCI-401 需要外推必须另立实验与出处 |

---

## 10. 核验记录与复跑命令

**证据目录**：`run/DOC-404/evidence/`（Crossref JSON 逐条落盘、官方文档 HTML、开源源码与行锚日志、arXiv API 响应）。

```bash
# DOI 元数据（逐条）：run/DOC-404/verify_refs.py doi <DOI>...
python3 run/DOC-404/verify_refs.py doi 10.1051/0004-6361/202243880 10.1086/160817 10.1086/664083
# 书目检索（无 DOI 候选）：run/DOC-404/verify_refs.py q "<bibliographic query>"
python3 run/DOC-404/verify_refs.py q "Naylor 1998 optimal extraction imaging photometry MNRAS 296 339"
# 开源 项目+版本+文件:行：run/DOC-404/verify_oss.py / verify_oss2.py / verify_oss3.py
python3 run/DOC-404/verify_oss.py     # 输出 run/DOC-404/evidence/oss_line_anchors.txt
# 官方文档 URL 与关键原文：见 run/DOC-404/evidence/*.html（curl -sL -A ... -w '%{http_code}'）
# 汇总表：run/DOC-404/evidence/verified_refs_table.txt（52 条 Crossref 落盘条目）
```

**关键原文摘录（采样定标，G2）**：

> “All mean spectra are sampled to the same set of absolute wavelength positions, viz. 343 values from 336 to 1020 nm with a step of 2 nm.”
> —— Gaia DR3 Documentation release 1.3, §20.12.4 `xp_sampled_mean_spectrum`（`run/DOC-404/evidence/gaia-xp-sampled.html`）

**核验计数（复跑器实测，2026-09-20）**：`python3 run/DOC-404/check_pack_refs.py` → **`doi=45 ok=45 fail=0 | arxiv=10 ok=10 fail=0 | url=24 ok=24 non200=0`（rc=0，两份研究包去重合计；含本包与 SNR 包）**；开源锚由 `python3 run/DOC-404/check_pack_oss_anchors.py` **逐条断言「锚行/锚区间逐字命中预期符号」→ `anchors=36 ok=36 bad=0`**（证据 `run/DOC-404/evidence/pack_oss_anchors_exact.txt`、`run/DOC-404/evidence/oss_line_anchors.txt`）；`[NONE]` 3 条（§9）。全部 DOI 经 Crossref API 解析且卷/页/年与作者匹配，全部 arXiv 经 `arxiv.org/abs` HTTP 200 + arXiv API 标题匹配。
