# 参考文献与开源项目（reverse_verify 逆向验收工作区）

> 上游：ASTROCS_DESIGN.md §12.3（实验单元的佐证来源要求）、ENGINEERING_SPEC.md §8（文档集与双向索引）
> 来源：`reverse_verify/references/bibliography.md`（2026-09-21 ROOT-CONSOLIDATION 迁入本目录；
> 同批迁入机器可读版 `reverse_verify_bibliography.bib`）。原 `reverse_verify/references/README.md`
> 的**记录规则**并入本节，规则原文保留如下。

## 0 记录规则（原 `reverse_verify/references/README.md`，逐字保留）

**规则**：每条必须给**可核对标识** + **借鉴点** + **不借鉴点** + **场景差异**。
**禁止编造引用。** 无法核对来源的条目一律不收录。

```markdown
### [编号] 作者 (年份). 标题. 出处. DOI/arXiv/URL
- **借鉴点**：…
- **不借鉴点**：…
- **场景差异**：…
- **核对状态**：已核对 / 待核对（注明核对方式）
```

（`reverse_verify_bibliography.bib` 为机器可读版本。）

---

## 1 图像叠加 / 最优组合 / drizzle 与重采样方差

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 1.1 | Fruchter & Hook 2002, Drizzle, PASP 114, 144. DOI 10.1086/338393 `[CR]` | drop-and-share 足迹与 pixfrac；drizzle 累加『输入值加权和』与『权重和』(weight image)——这正是承载逆方差累加的自然位置（输出权重 Σw、输出方差 Σw²σ²）；drizzle 对输入像素值**线性** ⇒ `Var(Ax)=AΣAᵀ` 可直接用 | 输出像素间**不相关**的隐含假设。输入足迹一重叠输出就相关；**禁止**把 drizzle 后的 variance 面当对角协方差（`PΣPᵀ` 的对角线不是全部）；也不把 pixfrac→1 当 PSF 模型替代 | HST 欠采样、PSF 稳定；本项目输出 HEALPix leaf、PSF 随 seeing 变 |
| 1.2 | Zackay & Ofek 2017, ApJ 836, 187. DOI 10.3847/1538-4357/836/2/187; arXiv:1512.06872 `[CR, arXiv]` | 点源最优 coadd **不是**逆方差加权平均，而是 **PSF 匹配后的滤波**组合 ⇒ 本文『SNR 应在 PSF 齐化后定义』的依据；`ΣwᵢIᵢ/Σwᵢ` 只对**面亮度型**信号最优 | 全文高斯噪声假设与『PSF 已知精确』假设；本项目泊松主导、PSF 是拟合的，最优性只在线性化高 S/N 极限成立 | 本项目 UPM 同时拟合 `g_k` 与空间场，不是单纯图像组合 |
| 1.3 | Zackay & Ofek 2017, ApJ 836, 188. DOI 10.3847/1538-4357/836/2/188; arXiv:1512.06879 `[CR, arXiv]` | proper coaddition：Fourier 域**等方差**加权的最大 S/N 测量，输出像素**不相关**且保留全部空间频率信息；作为本文叠加器的**目标态** | 标题中的 **background-dominated noise limit**（天光亮、逐像素噪声仍依赖信号时失效；校准阶段的读出/平场噪声更不成立）；也不假设全马赛克噪声功率谱平稳 | 本项目噪声面由稀疏控制点重建，天然非平稳 |
| 1.4 | Zackay, Ofek & Gal-Yam 2016 (ZOGY), ApJ 830, 27. DOI 10.3847/0004-637X/830/1/27; arXiv:1601.02655 `[CR, arXiv]` | 把 **(i) 乘性通量标度 (ii) PSF 匹配核 (iii) 加性背景项** 三者**分离**并同处一个闭式最优统计量——正是 UPM 的加性/乘性分解 | 两图（参考 vs 科学）框架；本项目对 **K 帧同时**拟合 `g_k` 与天光场，不逐对；也不取背景主导极限与『每对单一标量标度』 | UPM 参数维度远高于 ZOGY（每帧 `g_k` + 每控制点一个场值） |
| 1.5 | Hu & Wang 2024, AJ 167, 231. DOI 10.3847/1538-3881/ad36cb `[CR]` | 2024 年真实欠采样数据上『PSF 匹配 coadd + 显式方差传播』的完整实例，含匹配核引入的噪声相关性处置 ⇒ 可引为当代可辩护的方法选择 | 空间望远镜特性（PSF 稳定、背景低且平稳、无大气变化） | 本项目地基、seeing 时变、天光强且结构化 |
| 1.6 | Makovoz & Marleau 2005 (MOPEX), PASP 117, 1113. DOI 10.1086/432977 `[CR]` | tile **重叠区**对两个候选值插值并**传播插值结果的不确定度** ⇒ 本文『稀疏控制点→插值校正场』最接近的公开先例，也是『插值误差进方差预算』的可引基线 | MOPEX 的逐 tile（分片）插值；本项目插值的是**全局**控制点场，插值误差在全马赛克上空间相关 | 本项目控制点场由 UPM 拟合，不只是插值 |
| 1.7 | DrizzlePac Handbook v3.0 (2025, STScI), https://hst-docs.stsci.edu/drizzpac `[page]` — 文档 | 重采样中 variance/weight 面传播的**操作级**配方与术语（`final_wht`/`final_ivm` 的角色、drizzle 核参数） | HST 特有约定（逐曝光 IVM、畸变/PAM、按曝光时间加权）；也不取它把相关噪声当脚注的做法——本项目相关噪声是一等交付项 | 本项目输出 HEALPix leaf，不是 `_drz` 平面 |
| 1.8 | astropy `reproject` 文档, https://reproject.readthedocs.io/en/stable/ `[page]` — 文档（**未核到 JOSS DOI，勿引**） | `reproject_and_coadd(..., input_weights=..., combine_function=...)` 的实际签名；`input_weights` + `combine_function='mean'` 作**朴素**逆方差加权平均；`match_background` 参数本身即承认『加性背景必须与乘性权重分开处理』 | **本簇最重要的负面发现**：`reproject_and_coadd` **只返回合并数组与 footprint，不返回传播后的方差/协方差面**，`combine_function` 菜单无方差感知选项；`reproject_interp/adaptive/exact` 默认同样不传播 ⇒ **不能**引它替我们做误差传播，必须自己实现 `PΣPᵀ` | 本项目需在 HEALPix 上传播并保留相关核 |
| 1.9 | Aitken 1935, Proc. R. Soc. Edinb. 55, 42. DOI 10.1017/S0370164600014346 `[CR]` | 广义最小二乘/BLUE 原始推导：已知误差协方差下最小方差无偏线性估计由**逆协方差**加权给出 ⇒ `w=1/σ²` 的严格祖先，也是我们真正需要的一般式 `w=C⁻¹1` 的出处 | 协方差**已知**的假设；本项目 `C` 由数据估计（逐像素方差模型 + 拟合的 UPM），权重本身是随机变量，最优性只在渐近意义成立 | 本项目误差相关（重采样后），必须用一般式 |
| 1.10 | Plackett 1949, Biometrika 36, 458. DOI 10.1093/biomet/36.3-4.458 `[CR]` | 最小二乘/最优组合史的简短同行评审记述，供审稿人追问逆方差加权出处时引用 | 不得作为最优性的**证明**（它是历史注记） | 无 |

## 2 CCD 噪声模型与 CCD 方程

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 2.1 | Janesick 2007, Photon Transfer: DN → λ, SPIE Press. DOI 10.1117/3.725073 `[CR]` — 书 | photon transfer curve 测**转换增益** `g`(e⁻/DN)、读出噪声 `σ_R`，区分时间噪声与固定图案噪声(DSNU/PRNU) ⇒ 方差模型 `gain`/`readnoise` 输入的经验支柱；也是『用 e⁻ 还是 DN 表述噪声项』的正确写法来源 | 单一、稳定、标量增益的假设（本项目多探测器/多放大器拼接，增益逐放大器变且可漂移）；也不取理想化『线性、无 blooming、无 brighter-fatter』传感器模型 | 本项目元数据中**没有** GAIN/RDNOISE 键（实测 117 张卡全无），只能走经验基线（正文 §2.1 决策 D1） |
| 2.2 | Janesick 2001, Scientific Charge-Coupled Devices, SPIE Press. DOI 10.1117/3.374903 `[CR]` — 书 | 校准帧方差模型必须枚举的噪声项完整清单及物理来源：散粒、读出、暗电流、电荷转移损失、平场/PRNU 误差及其随信号电平的标度 ⇒ 方差方程『完整性』的参照分类学 | 这些项在空间与时间上平稳的假设；本项目把其中若干项当作**空间变化的场**（这正是 UPM 的意义） | 本项目不做探测器级表征，只从数据估计 |
| 2.3 | Howell 2006, Handbook of CCD Astronomy 2nd ed., CUP. DOI 10.1017/cbo9780511807909 `[CR]` — 书 | **CCD 方程**：分子是源积分电子数，分母是源散粒、**天光**散粒（孔径内）、暗流、逐像素读出、平场/闪烁项的平方和 ⇒ 本文帧级 SNR 在简单情形应退化到的形式；也是『扣掉天光均值、保留其方差』的经典权威表述 | **孔径**框架：本项目 Phase-1 是 PSF 拟合测光，`n_pix σ_sky²` 必须换成 PSF 加权 `Σᵢwᵢ²σ_sky,i²`；『平场/闪烁』项变成我们的逐像素响应不确定度 | 本项目把响应不确定度当作可拟合的场（UPM），不是外部定标产物 |
| 2.4 | Mortara & Fowler 1981, Proc. SPIE 290, 28. DOI 10.1117/12.965833 `[CR]` | 天文 CCD 性能表征（增益、读出噪声、噪声预算实测）的早期可引文献，用于论证方差模型各项的历史来源 | 1981 年器件物理（无 brighter-fatter、无电荷扩散建模） | 现代 CMOS/深耗尽器件 |
| 2.5 | EMVA 1288 标准, https://www.emva.org/standards-technology/emva-1288/ `[page]`；Jähne 2010, Optik & Photonik 5, 53, DOI 10.1002/opph.201190082 `[CR]`；Borek 2023, Electronic Imaging 35, 347-1, DOI 10.2352/ei.2023.35.6.iss-347 `[CR]` — 标准+文档 | 转换增益、时间暗噪声、SNR、线性度/PRNU/DSNU 表征的**标准化可复现定义**；声称增益/读出噪声可跨相机比较时应引标准而非只引论文 | 机器视觉工作区间（常温 CMOS、短曝光、无低温暗流、无天文天光）。EMVA 1288 **不覆盖**天文量级暗流散粒噪声、不覆盖天光项、不覆盖亚电子噪声；**不得**把它的 SNR 定义当作我们的 SNR 定义（我们必须含天光） | 本项目没有实验室积分球数据 |
| 2.6 | `ccdproc.create_deviation` 文档, https://ccdproc.readthedocs.io/en/latest/api/ccdproc.create_deviation.html `[page]` — 文档 | **校准帧**逐像素方差模型的参考实现：`create_deviation(ccd_data, gain, readnoise, ...)` 返回标准差面，『Gain is used in this function only to scale the data in constructing the deviation』⇒ `σ = sqrt(data/gain + readnoise²)` 的形状，正是正文式 (2.1)(2.2) 所需 | 它**省略**平场/响应误差项与暗流项；这是**最小**模型，本项目必须补上响应不确定度项，否则方差恰好在平场大尺度残差处偏乐观 | 本项目响应误差由 UPM 拟合，且需要其协方差 |
| 2.7 | Astropy `CCDData`/`NDData` 不确定度类, https://docs.astropy.org/en/stable/nddata/ccddata.html `[page]`；软件引用 Astropy Collaboration 2022, ApJ 935, 167, DOI 10.3847/1538-4357/ac7c74 `[CR]` | **数据模型**：`CCDData` 支持 `StdDevUncertainty`/`VarianceUncertainty`/`InverseVariance` 三态且『error propagation is also supported』⇒ 建议**以逆方差为主产品**（叠加器直接消费、可加），**以方差为交换格式** | 不确定度**逐像素对角**的隐含假设：Astropy 的传播是逐元素的，无法表示 drizzle/reproject 的相关噪声；也不认为其算术传播对**非线性**运算（取对数、除以估计的平场）足够——那些必须自己线性化 | 本项目需要相关核与协方差二次型 |
| 2.8 | Bernstein et al. 2017 (DECam), PASP 129, 114502. DOI 10.1088/1538-3873/aa858e `[CR]` | 为宽视场成像仪建立**显式仪器响应模型**并由此导出逐像素方差（含增益、读出噪声、平场/响应改正**及其不确定度**）的现代范例 ⇒ 『我们的逐像素方差是校准帧在响应模型下的方差』的正确先例；也示范把平场当**被测量的、有不确定度的**量 | DECam 专用探测器参数化，以及『仪器已良好表征、有专用定标数据（dome flat、star flat）』的假设；本项目必须只从帧本身出发 | 本项目无 dome flat 级专用数据；Bernstein 用专门定标产品的地方我们用 UPM 拟合的响应 |

## 3 巡天管线：coadd 中的方差/权重处理

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 3.1 | Morganson et al. 2018 (DES), PASP 130, 074501. DOI 10.1088/1538-3873/aab4ef; arXiv:1801.03177 `[CR, arXiv]` | 端到端**架构**：单帧 detrending → 天体测量解 → coaddition，**每个图像产品携带显式权重/方差面**，并有成文策略说明像素被 mask/clip 时权重怎么办 ⇒ 『方差面作为一等产品』是主流巡天标准实践 | 巡天尺度假设：均匀深度 tiling、每像素巨量曝光、以及『每帧光度响应在 coadd **之前**已由独立定标管线解决』；本项目 UPM **联合**求解响应与 coadd | 本项目帧数少（49 帧）、覆盖不规则 |
| 3.2 | Sevilla-Noarbe et al. 2021 (DES Y3), ApJS 254, 24. DOI 10.3847/1538-4365/abeb66 `[CR]` | 科学级巡天中 **coadd 权重图的具体定义**，以及『权重图在 coadd 图/检测图/测量图之间含义不同』的讨论 ⇒ 论证本文规则：**拟合 UPM 用的权重与叠加用的权重不必是同一个对象，但必须分别成文** | 权重图在所有尺度上都是忠实逆方差的假设——DES 同样必须对相关噪声与『coadd 权重不是 coadd 的逆方差』作修正或加注 | 本项目权重来自稀疏 SNR 重建 + 残差制造者方差 |
| 3.3 | Bosch et al. 2018 (HSC), PASJ 70, S5. DOI 10.1093/pasj/psx080; arXiv:1705.06766 `[CR, arXiv]` | HSC/LSST 血统的管线设计：摘要明示 HSC 管线『builds on the prototype pipeline being developed by the LSST Data Management system』，并同时含『low-level detrending and image characterizations』与 coadd ⇒ 本文『两阶段镜像同一架构』的最佳**血统**引用 | LSST-DM 数据模型假设（butler repo、per-visit/per-tract/per-patch 分区、`SkyMap` 几何）；本项目 HEALPix 控制点 UPM 是**不同**的空间分解，应明确说明而非假装 LSST 兼容 | 本项目以 HEALPix tile + 8×8 控制点网格为空间分解 |
| 3.4 | Waters et al. 2020 (Pan-STARRS), ApJS 251, 4. DOI 10.3847/1538-4365/abb82b; arXiv:1612.05245 `[CR, arXiv]` — **与本文 Phase 2 最接近的公开类比** | 正文（实页核对）：噪声图**不**施加到科学图，而是『used to construct the weight image that contains the pixel-by-pixel variance for the chip stage image』，初始权重图由科学图 + cell gain 构造；detrending 各节配 Detrend Merge Option 表逐步骤规定迭代次数/裁剪阈值/合并方法。借鉴 (i) **科学图与权重/方差图显式分离** (ii) 方差模型由数据+增益构造 (iii) 逐步骤成文裁剪/合并策略；另借鉴其 warping+stacking（『先重采样后加权叠加』在规模上最接近的公开记述） | per-chip/per-cell 增益归一化到『约 1.0 e⁻』与 PS1 专用 detrend 清单；更重要的是**不**假设『一张全局权重图构造好后经 warping 仍正确』——PS1 的 stacking 继承了我们必须处理的相关噪声警告 | PS1 每像素曝光数远超本项目 |
| 3.5 | Magnier et al. 2020 (Pan-STARRS 系统), ApJS 251, 3. DOI 10.3847/1538-4365/abb829 `[CR]` | 3.4 的系统级上下文：detrending/warping/stacking 如何编排、方差/权重产品在数据流中的位置 ⇒ 论证以 manifest + hash 作阶段隔离是公认模式 | PS1 专用数据流与数据库约定 | 本项目三命令（normalize/mosaic/export）以磁盘产品交换 |
| 3.6 | York et al. 2000 (SDSS), AJ 120, 1579. DOI 10.1086/301513 `[CR]` | 现代『先校准帧、再组合、携带逐像素不确定度』架构的历史起点 | drift-scan 专用几何与 2000 年代定标方式（已被 ubercal 取代） | 本项目是凝视成像马赛克 |
| 3.7 | Stoughton et al. 2002 (SDSS EDR), AJ 123, 485. DOI 10.1086/324741 `[CR]` | SDSS **`ivar` / 权重面约定**与成像产品 mask/flag 语义在此详细成文 ⇒ 『发布逆方差面 + mask 并定义其交互』的可引先例（`ivar` 面是 2.7 中 `InverseVariance` 表示的直接祖先） | SDSS flag 位定义；以及『`ivar` 面完整描述噪声』的假设（它是对角的） | 本项目另有相关噪声核 |
| 3.8 | Padmanabhan et al. 2008 (ubercal), ApJ 674, 1217. DOI 10.1086/524677; arXiv:astro-ph/0703454 `[CR, arXiv]` | 摘要（实页核对）：算法『simultaneously solves for the calibration parameters and relative stellar fluxes using overlapping observations』，『decouples the problem of relative calibrations from that of absolute calibrations』，并关注『the spatial structure of the calibration errors, allowing one to isolate particular error modes』⇒ UPM 的 `g_k` + 光滑空间改正场的直接祖先；论证『从重叠帧拟合相对响应、绝对零点单独处理』；『隔离特定空间误差模式』正是 HEALPix 控制点的用途 | **ubercal 假设加性天光背景已被测光阶段去除，它只拟合乘性模型（外加少量加性项），没有逐帧加性天光场**——这是与 UPM 的关键结构差异，必须明说；也不取『同一批星有大量重叠观测』区间，也不取『逐曝光标量（或低阶多项式）响应』（本项目空间场是逐控制点的） | 本项目帧重叠可能只是部分的 |
| 3.9 | Ivezić et al. 2019 (LSST), ApJ 873, 111. DOI 10.3847/1538-4357/ab042c `[CR]` | LSST/Rubin 数据产品（含 coadd 及其方差/mask 面、cell-based coadd 概念）的参考描述 ⇒ 『发布 coadd 同时发布其方差』是现代巡天基线预期 | 参考设计级细节——这是设计研究论文不是算法论文，**不得**据其引任何具体加权公式 | 本项目规模小得多 |
| 3.10 | LSST/Rubin Science Pipelines 文档 `[page]` — 文档 | 当前维护的实现，最好的『实际怎么做』参考：`lsst.drp.tasks.assemble_coadd.AssembleCoaddTask`（现代替代旧的 `SafeClipAssembleCoadd`/`makeCoaddTempExp`；**旧 `lsst.pipe.tasks.*` URL 已 404，勿引旧类名**）；**`CompareWarpAssembleCoaddTask` 是本簇对本项目 UPM 最相关的一篇**——实页核对到『we compute the coadd as an clipped mean (i.e., we clip outliers)』，有 `weightList`、`templateCoadd` 作『model of the static sky』、`CLIPPED` mask 面标记『pixels in the individual warps suspected to contain』瞬变/偏离信号；compare-warp 对静态天光模型拟合并标记逐 warp 偏差——**与 UPM『逐帧加性+乘性改正 vs 公共天光模型』结构上同一个问题**，借鉴 **clipped-mean-with-model** 模式与逐 warp 偏差诊断；`AssembleCellCoaddTask` 与 `lsst.cell_coadds` 给出分块 coadd + 逐 cell provenance；LSST DPDD（LSE-163, https://ls.st/LSE-163）是数据产品的**权威规范** | tract/patch `SkyMap` 几何、Butler 数据访问层；以及『每个 coadd 输入都是同一管线产出的 PSF 匹配/DCR 改正 warp』的假设 | 本项目用 HEALPix tile + 8×8 控制点，不是 tract/patch |
| 3.11 | Marmo & Bertin 2008, MissFITS and WeightWatcher, ASP Conf. Ser. 394, 619 `[page: 书目细节读自 SExtractor 参考表]` — 会议论文；**未核到 DOI** | WeightWatcher 是专门**操作权重图**的工具（创建、合并、在 variance/weight/flag 表示间转换）⇒ 『权重图簿记是被认可、有工具支持的问题』，并作本文 manifest 必须声明『哪个面是 variance、哪个是逆方差、哪个是 mask』的设计先例 | 其格式级约定不能替代我们自己的 schema | 本项目用 JSON/FITS 双写面 |

## 4 背景与噪声估计

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 4.1 | Bertin & Arnouts 1996, SExtractor, A&AS 117, 393. DOI 10.1051/aas:1996164 `[CR]` | 两项（均在当前 SExtractor 文档实页核对）：(1) **背景算法**（.../Background.html）『SExtractor makes a first pass through the pixel data, estimating the local background in each mesh of a rectangular grid that covers the whole frame. The background estimator is a combination of κσ clipping and mode estimation, similar to Stetson's DAOPHOT program.』——正是 UPM 天光场『稀疏网格 + 稳健统计 + 插值』配方的前身。(2) **测光误差方程**（.../Photom.html，**式 (36)**，逐字核对）`FLUXERR = sqrt( Σ_{i∈A} ( σ_i² + p_i / g_i ) )`，『σ_i, p_i, g_i respectively the standard deviation of noise (in ADU) estimated from the local background, p_i the measurement image pixel value subtracted from the background, and g_i the effective detector gain』——**孔径测光 SNR 的规范可引用陈述**，直接回答口径问题：天光**均值被扣除**、**方差留在分母** | 网格背景作为**最终产品**：其 mesh 是检测辅助，插值（中值滤波+样条）不是统计最优重建且**不附不确定度**；UPM 必须给天光场附方差，SExtractor 不提供。也不把孔径误差方程原样用于 PSF 测光（见 2.3），也不把 `FLUXERR` 当总误差——同页自带警告『this error estimate provides a lower limit of the true uncertainty, as it only takes into account photon and detector noise.』**必须一并引用** | 本项目背景场要进 UPM 拟合并有 `C_θ` |
| 4.2 | Barbary 2016, SEP, JOSS 1, 58. DOI 10.21105/joss.00058 `[CR]` | SExtractor 算法的同行评审**库**实现（背景网格、提取、同一通量误差模型）；SEP 还把背景 RMS 图作为一等数组暴露——正是噪声场需要的 | 同 4.1：背景 RMS 图是稳健**局部散度**估计，不是标定过的方差，也不携带传播 | 本项目噪声场需与 PSF、`a_k` 合成信息权重 |
| 4.3 | Bradley, Sipőcz, Robitaille & Tollerud 2020, astropy/photutils 1.0.0, Zenodo. DOI 10.5281/zenodo.4044744 `[DataCite]` — 软件 | (i) `Background2D` 作带 σ 裁剪的网格背景生产实现——文档明示『pixels that are above or below a specified sigma level from the median are discarded』并使用 median/MAD 或 biweight；(ii) `aperture_photometry(..., error=...)` 与 `aperture_sum_err`——文档说明它是传播后的误差且『σ_tot,i is the input error array』 | **已核关键警告**：文档写明『it is assumed that the error keyword specifies the *total* error — either it includes Poisson noise due to individual sources or such noise is irrelevant.』若只把背景误差交给 photutils，它会**照样返回一个错误的 SNR**。也不把 `Background2D` 的插值当作不确定度感知的天光模型 | 本项目 SNR 必须在稀疏控制点上定义并可重建 |
| 4.4 | Akhlaghi & Ichikawa 2015 (NoiseChisel), ApJS 220, 1. DOI 10.1088/0067-0049/220/1/1; arXiv:1505.01664 `[CR, arXiv]` | **噪声优先、非参数**哲学：摘要（实页核对）『imposes negligible constraints on the properties of the targets』、『employs no regression analysis or fittings』、定义 sub-sky detection threshold、『independently of the sky value』找初始检测、用『the ambient noise as a reference』去除虚假检测 ⇒ 本文『SNR 相对**局部估计的噪声场**定义、而非相对假定天光水平』的最强公开论据 | 检测/分割目标：它是检测算法，我们需要**加权用**的噪声场，其 ambient noise 为检测纯度调优而非逆方差加权；也不取『噪声场空间光滑』的假设而不先在自己的数据上验证 | 本项目噪声场要进 UPM 加权与方差产品 |
| 4.5 | Da Costa 1992, Basic Photometry Techniques, ASP Conf. Ser. 23, 90. ADS 1992ASPC...23...90D `[ADS]` | **MMM（Mode = 3×Median − 2×Mean）**稳健背景估计器与 DAOPHOT 血统的『从环/网格像素的裁剪分布估天光』实践；SExtractor 文档把其背景估计描述为 κσ clipping + mode estimation 且参考表 [2] 即引 Da Costa 1992 ⇒ 这是 MMM 的一手出处 | 交互式、单帧、小视场语境；**不给天光估计的不确定度**，且假设天光局部常数。本项目的天光是**带方差的拟合场** | 本项目天光场有 `control_variance` |
| 4.6 | Stetson 1987 (DAOPHOT), PASP 99, 191. DOI 10.1086/131977 `[CR]` | (i) 稳健天光估计器与迭代 PSF 拟合测光环（找源→建 PSF→减→再找→再拟合）；(ii) 对天光环中**邻源污染**的显式处理——正是 Phase-1 测光在密集场会遇到的失效模式；SExtractor 自认的背景估计祖先 | 1987 年的计算妥协（解析+经验查表 PSF、单遍分组、拟合过程无正规误差传播）；也不取『每源一个标量天光』的假设 | 本项目 `W_info = a²PᵀC⁻¹P` 是矩阵形式 |
| 4.7 | Huber 1964, Ann. Math. Stat. 35, 73. DOI 10.1214/aoms/1177703732 `[CR]` | M 估计族的理论基础，支撑所有裁剪均值/裁剪中位数/biweight 背景估计器；当声称估计器在技术意义上**稳健**（有界影响函数、高崩溃点）而非仅『我们在 3σ 处裁剪』时引用 | i.i.d. 对称位置模型：本项目背景像素**空间相关**（PSF 翼、平场结构），违反独立性，使名义稳健标准误偏乐观 | 本项目噪声场是空间场 |
| 4.8 | Popowicz & Smolka 2015, MNRAS 452, 809. DOI 10.1093/mnras/stv1320 `[CR]` | 对**复杂**（结构化、非平坦）天文背景估计的专门处理，可作 SExtractor 网格法的可引用对照，并作为『背景估计是活跃方法学问题而非已解决预处理』的证据 | 除非做基准比较否则不借鉴其具体估计器；**也不借鉴任何不确定度声明**——本轮**未**从全文核实其是否提供不确定度 `[UNVERIFIED]` | 本项目背景场由 UPM 与其他参数联合拟合 |
| 4.9 | SWarp（Bertin）软件与手册, https://www.astromatic.net/software/swarp/ ；手册 https://raw.githubusercontent.com/astromatic/swarp/legacy_doc/prevdoc/swarp.pdf ；源码 https://github.com/astromatic/swarp `[page]` — 文档/软件 | 带权重与加性背景改正地重采样并合并 FITS 图的规范实用实现。借鉴其**参数分解**作设计检查表：重采样核、加权模式、加性背景扣除（`SUBTRACT_BACK`/`BACK_SIZE`/`BACK_FILTERSIZE`）、乘性通量标度模式——『加性背景与乘性标度分开两个旋钮』**正是 UPM 的结构** | SWarp 的加权模式作为方差模型：其输出是合并图 + 权重图，**不是传播后的协方差**；AstrOmatic 项目自身也警告权重图误用（2009-06-02 news item）；也不把其背景扣除当作不确定度感知的 | 本项目 UPM 同时给出参数协方差 |
| 4.10 | Astropy `sigma_clipped_stats`/`SigmaClip` 文档, https://docs.astropy.org/en/stable/api/astropy.stats.sigma_clipped_stats.html `[page]` — 文档 | σ 裁剪迭代的精确可复现定义（`sigma`、`maxiters`、可选 `cenfunc`/`stdfunc`），使方法节能明确写出参数而不是『我们做了 σ 裁剪』 | 对受源污染的天光场使用默认 `cenfunc=mean, stdfunc=std`——应按 photutils 建议用 median/MAD 或 biweight（4.3）；也不把裁剪标准差当作**均值**的方差（它是逐像素散度） | 本项目天光场在 patch 尺度上做同样的裁剪 |
| 4.11 | Astropy Collaboration 2022, ApJ 935, 167. DOI 10.3847/1538-4357/ac7c74 `[CR]` | Astropy 本体的总括引用，凡引 astropy 系算法（2.7、4.10）或使用 astropy 时必需 | 无方法学内容 | 无 |

## 5 PSF 测光

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 5.1 | Stetson 1987 — 见 4.6（交叉引用，不重复） | 迭代 PSF 测光的规范引用 | 同 4.6 | 同 4.6 |
| 5.2 | Bertin 2011, Automated Morphometry with SExtractor and PSFEx, ASP Conf. Ser. 442, 435. ADS 2011ASPC..442..435B `[page: aspbooks 页 + ADS scan]` — 会议论文；**未核到 DOI** | PSFEx 是**用多项式基对 PSF 作场级分解**的参考实现——从星像切片拟合**空间变化**的 PSF。借鉴『PSF 模型是一个**带自身基的场**，不是单张图』 | PSFEx 内部的 `SAMPLE` 基（默认 Sérsic 类基）与『场内有足够密度的合适恒星』假设；本项目每控制点可能星数不足，需要 PSF 模型在场内**共享强度**。也不借鉴其不确定度处理——PSFEx **不产出 PSF 模型的协方差** | 本项目 PSF 信息进入 `W_info`，需要其误差 |
| 5.3 | Lang, Hogg & Mykytyn 2016 (The Tractor), 文档 https://thetractor.org/ ；源码 https://github.com/dstndstn/tractor `[page]` — 软件 | **前向建模/概率化**：直接对像素拟合天空的生成模型（星系与恒星轮廓之和卷积 PSF），带似然，而不是从减影图测通量 ⇒ 『测光模型（进而 UPM）应当是带似然的前向模型』的可引用依据 | 逐目标优化开销；『噪声高斯且逐像素基本不相关』假设；若打算做联合拟合，也不取其『先检测后拟合』两段式 | 本项目 UPM 参数维度含逐帧加性场与乘性响应 |
| 5.4 | Melchior et al. 2018 (scarlet), Astronomy and Computing 24, 129. DOI 10.1016/j.ascom.2018.07.001; arXiv:1802.10157 `[CR, arXiv]` | 摘要（实页核对）：scarlet『describes the observed scene as a mixture of components with compact spatial support and uniform spectra over their support』，并『derive[s] the treatment of correlated noise and convolutions with band-dependent point spread functions, rendering our approach applicable to coadded images observed under variable seeing conditions』⇒ **相关噪声处理**是可引用的、在 coadd 相关噪声下正确测量的公开示范，正是本项目 drizzle/reproject 造成的处境 | 多波段源分离目标与『紧支撑上光谱一致』假设；UPM 的天光场既不紧支撑也不是源 | 本项目噪声相关来自 HEALPix 重采样 |
| 5.5 | Anderson & King 2000, PASP 112, 1360. DOI 10.1086/316632 `[CR]` | **有效 PSF（ePSF）**方法——在比探测器采样更细的网格上通过迭代配准叠加星像构建 PSF 模型，再用于亚像素精确拟合；photutils 实现的 ePSF 概念的规范出处 | HST/WFPC2 特性（极稳定、欠采样、无大气 seeing）；也不取『有大量明亮孤立且分布良好的恒星』假设 | 本项目星密度与分布不均（实测 32 px patch 中 91.5% 少于 3 颗星） |
| 5.6 | Dolphin 2000, PASP 112, 1383. DOI 10.1086/316630 `[CR]` | **完整** PSF 拟合测光管线的可引用示范，含拟合 PSF 通量到标定星等的换算、从 PSF 模型的**孔径改正**、以及 CTE 处理；尤其借鉴孔径改正（PSF 通量 → 无穷孔径通量）——本项目需要它，且它是常见的静默偏差来源 | HST 专用 CTE 与 WFPC2 几何畸变处理 | 本项目孔径改正必须跨帧一致（公共 `F_ref`） |
| 5.7 | `photutils` PSF 测光文档, https://photutils.readthedocs.io/en/stable/user_guide/psf.html `[page]` — 文档 | 实际会用的 API：`PSFPhotometry`、`IterativePSFPhotometry`（文档：『an iterative version of PSFPhotometry where new sources are detected in the [residuals]』，『when used with the DAOStarFinder, is essentially an implementation of』DAOPHOT 式迭代测光）、`GriddedPSFModel`（矩形网格上的空间变化 PSF），以及 Building an effective Point Spread Function (ePSF) 的成文流程 | 与 4.3 同一陷阱——PSF 拟合器消费我们给的逐像素误差数组，**不会**自己构造它；`IterativePSFPhotometry` 的『在残差里检测』在密集场可能注入虚假源 | 本项目需要每控制点的局部 `W_info` |
| 5.8 | Moffat 1969, A&A 3, 455. ADS 1969A&A.....3..455M `[ADS: scan PDF 200]` | **Moffat 轮廓**作为带 `β` 参数控制翼强的解析 PSF 模型；借鉴它作为 PSF 芯+翼的**参数化**并引用其为该轮廓的出处 | 把该轮廓当作真实地基 PSF 的描述：对大气湍流**无物理基础**（是对照相乳胶的经验拟合），既不能复现远翼也不能复现 seeing 的时间变化；也不取『`β` 在场内或整夜恒定』假设 | 本项目 `snr_science.cpp` 固定用 Moffat4（β=4）解析轮廓且**不做 PSF 拟合**（见正文 §2.5 差距 G7） |

## 6 噪声/背景场的稀疏重建

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 6.1 | Górski et al. 2005 (HEALPix), ApJ 622, 759. DOI 10.1086/427976 `[CR]` | (i) **等面积、层级、等纬度**像素化 ⇒ HEALPix 控制点网格是球面**均匀**采样（与赤纬无关），这正是 UPM 控制点是 HEALPix 像素而非 RA/Dec 网格的原因（后者在两极过采样）；(ii) `N_side` 与像素角尺度的显式关系，使控制点间距可写成角分并与场相关长度比较；(iii) 邻居结构给出插值模板 | CMB 分析框架（球谐变换、功率谱、`anafast`/`map2alm`）——我们不做天光场的谐分析；也不取『场在谐意义下带限』假设 | 本项目控制点是每 512² tile 内 8×8 的规则网格（不是按 nside 选的稀疏点集） |
| 6.2 | Zonca et al. 2019 (healpy), JOSS 4, 1298. DOI 10.21105/joss.01298 `[CR]`；源码 https://github.com/healpy/healpy `[page]` | 我们实际使用的 HEALPix 实现的可引用软件引用，含插值函数（HEALPix 网格上的双线性/样条插值）——即控制点场的稀疏→稠密重建算子 | 默认插值作为**统计最优**重建：healpy 的插值是几何的，不知道控制点值的不确定度，也不返回插值误差方差——那部分必须自己提供（见 6.3/6.4） | 本项目重建还必须给出 `Var(SNR)` 用于加权 |
| 6.3 | Eilers & Marx 1996 (P-splines), Statistical Science 11, 89. DOI 10.1214/ss/1038425655 `[CR]` | **P-spline** = B 样条基 + 系数上的**离散差分惩罚**。UPM 光滑空间场的正确工具：(i) 把**基分辨率**与**拟合光滑度**解耦，可用细控制点网格而不过拟合；(ii) 有效自由度由单个光滑参数控制；(iii) 它是**线性**光滑器 ⇒ 估计量协方差有闭式，正是把场的不确定度传播进马赛克方差所需 | 一维表述：本项目场在球面上，朴素 (RA, Dec) 张量积有两极问题与非均匀面积权重——要么用 HEALPix 网格作定义域（6.1），要么显式声明坐标畸变；也不取高斯似然/最小二乘惩罚框架而不检查场的噪声是否同方差（本项目不是，惩罚必须施加在**白化**空间） | 本项目场定义在 HEALPix tile 的 8×8 控制点上 |
| 6.4 | Rasmussen & Williams 2006 (GPML), MIT Press. DOI 10.7551/mitpress/3206.001.0001 `[CR]`；全文 http://www.gaussianprocess.org/gpml | GP 回归作为『稀疏控制点 + 插值』的**有原则**版本：(i) 预测均值是控制点值的核加权组合——插值权重由**假定的协方差结构**决定而非几何选定；(ii) **预测协方差**形如 `K(X*,X*) − K(X*,X)[K(X,X)+σ_n²I]⁻¹K(X,X*)`，**正是本文论断 1 的残差制造者结构 `PΣPᵀ`（`P = I − H`）**——本簇最重要的一条借鉴：GP 给出插值场方差的**闭式可引用表达式**，正是它把稀疏控制点变成可辩护的方差图；(iii) 用边际似然拟合核超参数（光滑度/长度尺度）——这是我们**选择**控制点间距而不是猜的方法 | `O(N³)` 精确推断（控制点数小本身可接受，但全马赛克的稠密预测协方差不应物化——只需其对角，以及为叠加权重需要其逆的作用）；也不取平稳性假设：天光背景与响应误差在马赛克上**不**平稳，单一全局核是必须声明并检验的近似 | 本项目场的相关长度实测约 48 px（EXP-2），且逐帧不同 |
| 6.5 | Foreman-Mackey et al. 2017 (celerite), AJ 154, 220. DOI 10.3847/1538-3881/aa9332 `[CR]` | **特定结构**的核（复指数之和）使 GP 协方差稀疏/带状，把推断从 `O(N³)` 降到 `O(N)` ⇒ 作为**可扩展性论证**：若控制点协方差由紧支撑核或有理谱密度构造，稀疏重建对控制点数保持线性成本，这才使细控制点网格可负担 | **一维时间序列**设定：celerite 的核是一维的，本项目的场是球面二维。借鉴的是**思想**（结构化核 ⇒ 稀疏推断）不是实现；**不得**把 celerite 引作二维空间 GP 方法 | 本项目在 HEALPix tile 网格上 |
| 6.6 | Candès, Romberg & Tao 2006, IEEE Trans. Inf. Theory 52, 489. DOI 10.1109/TIT.2005.862083 `[CR]` | 压缩感知基础结论：在某个基下**稀疏**的信号，只要采样与该稀疏基足够**不相干**，就可用远低于 Nyquist 率的样本数**精确**重建 ⇒ 作为『稀疏控制点集**可能**足以重建天光/噪声场』的理论依据——**但仅当这些场确实在所选基下稀疏** | **本簇最重要的不借鉴**：『精确重建』保证要求 (i) 已知基下**精确**稀疏 (ii) 采样不相干 (iii) 无噪或有界噪声测量。本项目的场既不精确稀疏、也非无噪采样，且采样（规则 HEALPix 网格）与任何光滑基**高度相干**——与理论所需的随机采样恰好相反 ⇒ **不得对规则网格声称压缩感知保证** | 本项目控制点是规则网格，不是随机投影 |
| 6.7 | Candès & Tao 2006, IEEE Trans. Inf. Theory 52, 5406. DOI 10.1109/TIT.2006.885507 `[CR]` | RIP / 近最优恢复结果，使压缩感知保证对噪声与近似稀疏稳健；与 6.6 并列引用以陈述稀疏重建论证的**边界** | 同 6.6，随机投影假设 | 同 6.6 |
| 6.8 | Candès & Wakin 2008, IEEE Signal Processing Magazine 25, 21. DOI 10.1109/MSP.2007.914731 `[CR]` | 压缩感知条件的教程级陈述，便于方法节紧凑陈述假设而不重新推导 | 它是教程而非一手结果；定理请引 6.6/6.7 | 无 |
| 6.9 | Marinucci et al. 2008 (needlets), MNRAS 383, 539. DOI 10.1111/j.1365-2966.2007.12550.x `[CR]` | **needlets**——在实空间与谐空间同时局域的球面小波构造 ⇒ GP/P-spline 重建的替代：需要天光/噪声场的**多尺度**表示（既有大尺度梯度又有小尺度结构）时用它；也是球面小波在天文学中的可引用先例 | CMB 专用统计机械（角功率谱估计、原初非高斯检验）与统计各向同性假设；本项目的场明显各向异性（马赛克有边界、焦面有几何） | 本项目场定义在 HEALPix tile 网格上，不是全天 |
| 6.10 | Shannon 1949, Proc. IRE 37, 10. DOI 10.1109/JRPROC.1949.232969 `[CR]` | 采样定理——**带限场需要多密采样才能精确重建**的精确陈述。这是方法节回答『稀疏 vs 稠密』最干净的写法：控制点间距必须细于天光/噪声场中最小尺度的一半；若满足则（原则上）重建精确、插值误差为零；若不满足则欠采样，必须计入混叠功率 | 带限、无噪、无限域假设：本项目的场不是带限（天光背景在所有尺度上都有功率，马赛克有边界），样本有噪，定义域有限 ⇒ 该定理给的是**设计判据与误差界**，不是精确性保证 | 本项目实测相关长度约 48 px，而 UPM 控制点间距 64 px、Phase-1 patch 网格 512 px |

## 7 光度定标、相对响应与加性/乘性分离

| 编号 | 可核对标识 | 借鉴点 | 不借鉴点 | 与 ACSD 的差异 |
|---|---|---|---|---|
| 7.1 | Padmanabhan et al. 2008 — 见 3.8（交叉引用） | 相对/绝对解耦（仅乘性部分） | 无逐帧加性天光场 | 同 3.8 |
| 7.2 | Schlafly et al. 2012 (PS1), ApJ 756, 158. DOI 10.1088/0004-637X/756/2/158 `[CR]` | **实时、前向的平场光度解**——从巡天自身的重叠观测**同时**拟合仪器响应（乘性平场）与光度零点，即从数据导出**相对**定标 ⇒ UPM 的 `g_k` 拟合最接近的方法学类比；『乘性响应可仅从重叠帧恢复』的正确引用 | 巡天期间**静态**平场的假设（PS1 对每滤光片在 1.5 年内解一个平场）；本项目 `g_k` 是**逐帧**的，假设更弱但也不能在大量观测上平均；也不取其加性天光处理（与 ubercal 一样推定已在别处处理） | 本项目 `g_k` 目前在生产中恒为 1（未接线） |
| 7.3 | Burke et al. 2018 (DES forward global calibration), AJ 155, 41. DOI 10.3847/1538-3881/aa9f22; arXiv:1706.01542 `[CR, arXiv]` — **UPM 加性+乘性结构最重要的引用** | DES forward global calibration 建立观测通量的**前向模型**，其中仪器响应是**乘性**的、天光/大气贡献是**加性**的，并在全巡天范围内**同时**求解。借鉴 (i) 在**同一个全局最小二乘问题**里显式分离**加性 + 乘性**——正是 UPM 的 `g_k` + 天光/改正场；(ii) 『前向』框架（建模探测器**将会**记录什么，而不是先改正图像再定标）；(iii) 把大气当作加性+乘性项处理 | DES 专用参数化（有限的大气与仪器参数集，配合巡天级 cadence 与专门定标星表）；UPM 的加性场是**逐帧**且在 HEALPix 控制点上空间分辨的，维度高得多；也不取『通带已良好表征、大气消光有良好先验』假设 | 本项目帧数少、无专门定标星表 |
| 7.4 | Tonry et al. 2012 (PS1 测光系统), ApJ 750, 99. DOI 10.1088/0004-637X/750/2/99 `[CR]` | 从仪器响应函数定义巡天测光系统，含把滤光片/探测器/大气**乘积**当作定义通带的东西 ⇒ 定义 `g_k` 归一化**到什么**时引用；也是『乘性响应是通带加权量，不是标量』的引用 | PS1 具体滤光片集，以及『响应由实测滤光片/探测器曲线完全表征』的假设——本项目 `g_k` 是经验的 | 本项目无实测响应曲线 |
| 7.5 | Stubbs & Tonry 2006, ApJ 646, 1436. DOI 10.1086/505138 `[CR]` | **端到端前向定标**哲学：要达到声称的测光精度必须建模整条光学+探测器链，且主导不确定度是**系统性**的（平场、滤光片响应、探测器 QE）而非光子统计 ⇒ 『逐像素方差模型必须含**响应不确定度项**』的正确引用，也是『SNR 预算存在任何曝光时间都无法消除的系统地板』的框架 | 完整仪器化定标计划（专用光电二极管监测、NIST 溯源标准、专用定标望远镜）的假设；本项目没有这些，所以系统项**从数据估计**，因而本身也有不确定度 | 本项目无专用定标硬件 |
| 7.6 | Gaia Collaboration (Montegriffo et al.) 2023, A&A 674, A33. DOI 10.1051/0004-6361/202243709; arXiv:2206.06215 `[CR, arXiv]` | **Gaia 合成测光**的引用——把 Gaia 通量定标的 BP/RP 分光光度转换为任意用户定义通带下的合成星等的机械 ⇒ 把仪器 `g_k` 用 Gaia 星作参考系到物理通量尺度的依据；也是『用户通带合成测光如何定义、其不确定度为何』的可引用陈述 | 合成通带与我们的仪器通带完全一致的假设：任何失配都是直接传播进 `g_k` 的系统项；必须声明我们把合成测光当作**相对**参考，绝对系靠帧间重叠（7.2/7.3）。也不取 DR3 定标声称的不确定度作为我们的不确定度 | 本项目 `F_syn` 不含光学系统透过率与大气消光（`docs/science/PHOTOMETRY.md:7`） |
| 7.7 | Riello et al. 2021, A&A 649, A3. DOI 10.1051/0004-6361/202039587 `[CR]` | Gaia 测光内容（G/BP/RP）及其定标的权威描述与验证，含空间与颜色相关定标项的处理 ⇒ 论证所用 Gaia 参考星**质量**与引用定标不确定度地板时使用 | Gaia 具体扫描律与 CCD 级定标模型 | 本项目只用星表数值 |
| 7.8 | Evans et al. 2018, A&A 616, A4. DOI 10.1051/0004-6361/201832756 `[CR]` | 若使用 DR2 而非 EDR3/DR3 作参考星表时的版本专用引用。**引用实际使用的版本** | 不得在同一句里混用 DR2 与 EDR3/DR3 的定标陈述——定标不同 | 本项目当前用 Gaia DR3 |
| 7.9 | Gaia Collaboration (Drimmel et al.) 2023, A&A 674, A37. DOI 10.1051/0004-6361/202243797 `[CR]` | DR3 内容总括论文，与 7.6/7.7 并列引用（凡说『我们用了 Gaia DR3』时） | 无方法学内容 | 无 |
| 7.10 | PhotometricMosaic（PixInsight 模块）— **`[UNVERIFIED]`；不得作为一手来源引用** | **核对结果（诚实登记）**：预期文档 URL `https://www.woodlandsobservatory.com/PhotometricMosaic/PhotometricMosaic.htm` 今日 **HTTP 404**，Internet Archive CDX 对该路径与域名**无快照**。模块确实存在（PixInsight 论坛多个用户主题已实页核对），但**本轮未能定位权威稳定文档来源，因此不编造**。**本仓内已有其源码**：`run/RELEASE-02/pmosaic/PhotometricMosaic/`（v4.0.2，31 文件 18,081 行），**非开源**——`lib/LeastSquareFit.js:3-11` 明示『This program is free for personal use only. You may not redistribute or modify it.』⇒ **不得复制进 ACSD**。从本地源码读到的算法（可陈述『我们读了源码』，但引用须引同行评审等价物）：乘性 = 星等通量回归（`StarLib.js:888-906`；n<6 过原点 `Σxy/Σx²`，n≥6 调 PJSR `LinearFunction`）**完全不加权**，外加固定条数『垂距最大者逐一剔除』（`StarLib.js:914-933`）**无 σ 裁剪**；加性 = 重叠区分箱**中位数**之差（`SampleGrid.js:322-323`），对 `z = tgt·m − ref` 拟合 PJSR `SurfaceSpline`，权重是**箱内样本计数**（`SampleGrid.js:527-529, 573`）**不是** `1/σ²`。全树**无** variance/ivar/uncertainty 作为计算量的出现，无参数协方差，输出只有信号无方差面，仅 HISTORY 记 5 位有效数字的 `m`（`FitsHeader.js:302-311`）**不带不确定度**。概念层借鉴：加性梯度/背景与乘性标度的**显式分离**（与 UPM 同构） | 其估计器（无权重、无 σ 裁剪、计数权重）、其**无不确定度传播**、以及其许可条款 | PMM 面向业余/Pro-Am 马赛克；本项目要求参数协方差与产品级方差面。**替代引用**：Burke et al. 2018（7.3）作加性+乘性同时拟合的一手来源；SWarp（4.9）作『加性/乘性两个独立旋钮』；Jacob et al. 2010（7.11）作马赛克差分背景改正 |
| 7.11 | Jacob et al. 2010 (Montage), arXiv:1005.4454 `[arXiv]` | 摘要（实页核对）：Montage 构造的马赛克『preserve the astrometry (position) and photometry (intensity) of the sources in the input images』，即把跨 tile 的光度一致性当作一等要求，并**把差分背景改正（加性）与通量守恒（乘性）分开实现**；与 SWarp 并列作为第二个把加性/乘性分离显式化的规范马赛克工具包 | Montage 的通量守恒模式当作**光度定标**——它只把 tile 拉平，不导出带不确定度的响应模型；也不把其背景匹配当作不确定度感知的 | 本项目响应模型带 `C_θ` |

## 8 四条论断的来源核查

### 论断 1：`Var(corrected) = P Σ Pᵀ`，`P = I − H`

**结论：形式与记号是本文自己的推导；底层数学是标准的且可引用，但没有任何单一文献对我们的管线陈述过这个等式。不得作为『借来的等式』引用。**

- **受支持的部分**：一般仿射传播 `Var(Ax) = A Var(x) Aᵀ`（非线性映射用一阶/Taylor 版）是经典误差传播结果，规范可引用来源 **Ku 1966, J. Res. NBS 70C, 263. DOI 10.6028/jres.070c.025 `[CR]`**。**残差制造者结构** `M = I − H`（`H` 为 hat/投影矩阵）是标准线性模型理论；其**预测协方差**类比出现在 GP 回归（**Rasmussen & Williams 2006 `[CR]`**，Ch. 2，等式号 `[UNVERIFIED]`），代数上就是同一个残差制造者对象。加权最小二乘的投影/hat 矩阵视角在 **Hogg, Bovy & Lang 2010, arXiv:1008.4686 `[arXiv]`** 有教学式讨论；**该文据本轮所知未使用符号 `H`，也不陈述 `P = I − H`——不得把该记号归给它。**
- **不受支持、必须声明为本文自己的**：把 `P = I − H` 具体认定为**我们**的改正算子（bias/dark/flat、天光扣除、UPM 场扣除）。该认定是建模选择，仅当改正 (i) 对数据线性或已线性化、且 (ii) 其参数被视为**固定/已知**时成立。若改正**由同一批数据估计**（我们的 UPM 正是），则 `PΣPᵀ` **低估**方差，除非把估计的改正与数据之间的交叉协方差并入 `Σ`。**必须显式声明；审稿人会找它。**

### 论断 2：`C_θ = (JᵀWJ)⁻¹` 与预测方差 `J_out C_θ J_outᵀ`

**结论：作为标准结果受支持，且有同样受支持的重要限制。第二个表达式是 delta 方法对第一个的应用。**

- **受支持来源**：**GSL 参考手册**，Nonlinear Least-Squares Fitting 一节下的 Covariance matrix of best fit parameters 小节，https://www.gnu.org/software/gsl/doc/html/nls.html `[page: 已核该节标题存在于当前手册]`（以残差 Jacobian 定义最佳拟合参数协方差，并在 Weighted Nonlinear Least-Squares 小节说明加权/无权区别）；**Hogg, Bovy & Lang 2010, arXiv:1008.4686 `[arXiv]`**（含成立条件）；**Andrae, Schulze-Hartung & Melchior 2010, arXiv:1012.3754 `[arXiv]`**（摘要已核『The number of degrees of freedom can only be estimated for linear models. Concerning nonlinear models, the number of degrees of freedom is unknown』——**不得用 reduced χ² 重标定 `C_θ`** 的可引用警告，而 UPM 是非线性模型）。
- **限制**：`C_θ = (JᵀWJ)⁻¹` 要求 `W = C_d⁻¹` 且 `C_d` **已知到只差一个尺度**；若只有**相对**方差，`C_θ` 的绝对尺度不由该式单独确定——这正是人们去用 reduced-χ² 重标定的时刻，而 Andrae et al. 证明这对非线性模型无效。该式给的是**估计量**协方差，假定模型正确、噪声高斯，是**局部线性化**结果：在最优点用 Jacobian，因此当模型约束差或参数退化时**低估**方差；`J_out C_θ J_outᵀ` 继承同一限制。预测方差式是 **delta 方法**（Ku 1966 的应用），**忽略预测的固有散度**——若『预测』是含噪测量而非无噪函数值，必须再加逐像素噪声项。**须说明是哪一种。**

### 论断 3：逆方差加权 `w = 1/σ²` 是最小方差无偏线性组合

**结论：在 Gauss–Markov 假设下受支持；相关误差下的推广 `w = C⁻¹1` 也受支持。但若误差相关、或 `σ_i` 本身是估计量，该论断按字面为假——而这两点都适用于我们。**

- **受支持来源**：**Aitken 1935, Proc. R. Soc. Edinb. 55, 42. DOI 10.1017/S0370164600014346 `[CR]`**（广义最小二乘原始结果；对角情形 `w_i=1/σ_i²` 是无相关误差的特例）；**Plackett 1949, Biometrika 36, 458 `[CR]`**；**Rasmussen & Williams 2006 `[CR]`**（贝叶斯/GP 陈述）；教科书重述 **Ivezić, Connolly, VanderPlas & Gray 2014, Princeton UP. DOI 10.23943/princeton/9780691151687.001.0001 `[CR]`**。
- **限制**：定理是 **Gauss–Markov** 陈述，要求误差不相关、零均值、方差**已知**（差一个公共尺度）；本项目的逐像素方差由拟合的噪声模型**估计**，权重是随机变量 ⇒ 估计量仍无偏，但只在**渐近**意义下最小方差，低 S/N 时实际有偏。若误差**相关**（任何重采样、drizzle、控制点场插值之后都是），则 `w = 1/σ_i²` **不**最优，正确权重是 `w = C⁻¹1` ⇒ 这是『马赛克权重必须由**完整传播的协方差** `PΣPᵀ` 构造、而非仅用对角方差』的强可引用论据。『无偏』是针对**加权均值**；若改用**加权和**（例如以面亮度意义保通量），估计量不同，最优性陈述不迁移——**须明确我们做的是哪一种组合**。

### 论断 4：孔径测光 SNR `= flux / sqrt(flux/gain + n_pix σ_bkg² + ...)`

**结论：受支持——而且本轮在一手来源中找到了逐字的规范陈述。这是四条论断中最强的一条。**

- **规范陈述（逐字核对）**：**Bertin & Arnouts 1996, A&AS 117, 393. DOI 10.1051/aas:1996164 `[CR]`**，见当前 SExtractor 手册 https://sextractor.readthedocs.io/en/latest/Photom.html `[page]`，**式 (36)**：`FLUXERR = sqrt( Σ_{i∈A} ( σ_i² + p_i / g_i ) )`，『where A is the set of pixels defining the photometric aperture, and σ_i, p_i, g_i respectively the standard deviation of noise (in ADU) estimated from the local background, p_i the measurement image pixel value subtracted from the background, and g_i the effective detector gain in e⁻/ADU at pixel i.』**同页自带必须一并引用的警告**：『this error estimate provides a lower limit of the true uncertainty, as it only takes into account photon and detector noise.』
- **支持性陈述**：**Howell 2006, CUP. DOI 10.1017/cbo9780511807909 `[CR]`**（CCD 方程；**等式号 `[UNVERIFIED]`，按 Ch. 4 引用**）；**photutils 孔径测光文档**（`aperture_sum_err` 是孔径上求积的传播误差，『σ_tot,i is the input error array』，且警告 error 必须是**总**误差）；**Mortara & Fowler 1981, DOI 10.1117/12.965833 `[CR]`**。
- **口径问题（扣 vs 当噪声）由该方程解决，且是受支持的、非本文推导**：天光的**均值被扣除**（`p_i` 是『减去背景后的像素值』），而天光的**方差留在分母**（`σ_i²` 是『从局部背景估计的噪声标准差』）。扣天光去掉的是**偏移**不是**噪声**，而且它**增加**了天光估计自身的不确定度 ⇒ **扣均值、留方差**（并在可能时增大方差）。
- **必须由我们自己补上**：`n_pix σ_bkg²` 对**孔径**（硬边求和）成立；对**我们的 PSF 测光**要变成拟合足迹上带 PSF 权重的 `Σ_i w_i²σ_i²`——这是适配不是借来的结果。规范方程**没有**天光估计自身的不确定度项、没有平场/响应误差项、没有相关噪声项（SExtractor 自己这么说）；须补上并引 **Stubbs & Tonry 2006, DOI 10.1086/505138 `[CR]`** 作系统响应项、**Bernstein et al. 2017, DOI 10.1088/1538-3873/aa858e `[CR]`** 作响应模型处理。

---

## 9 本轮明确**未**收录的条目（核对失败）

- SWarp / Tractor / PSFEx 的 **ASCL ID**（`ascl.net` 在本环境不可达，`[UNVERIFIED]`）——自行核对前不得引用；
- astropy `reproject` 的任何 JOSS/Zenodo DOI（未核到）；
- Merline & Howell 1995（从未核实）；
- 任何 GitHub 仓库的**具体版本 tag**（只核实了 `/releases` 页面存在——请自行 pin 一个 tag 并记录）；
- PhotometricMosaic 的权威文档 URL（见 7.10）；
- 任何『未读全文』论文的**等式号**。

## 10 本次核对产生的三条最高价值修正

1. **`reproject_and_coadd` 不传播方差**（API 页已核）——不能引它『替我们做了误差传播』；那部分代码是我们自己的。
2. **孔径 SNR 的规范方程是 SExtractor 式 (36)，且自带『这是下界』的警告**（逐字读到）——这正好是论证我们额外系统项的依据。
3. **加性+乘性同时拟合的正确一手来源是 Burke et al. (2018)（DES forward global calibration），不是 ubercal**——ubercal 只有乘性；而常被引用的 PhotometricMosaic 文档是死链且无存档快照。


---

# 附：P1-SPATIAL-GAIN（Phase1 低阶空间乘法增益）参考文献

> 工作项：`实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/`（报告 `docs/p1-spatial-gain.md`）。
> 规则同上（`../README.md §5`）。核对方式：arXiv Atom API（标题/作者/摘要逐字）、本地源码逐字核对。
> 与上文 SNR 条目的交叉核对：**上文 §10.3 指出 ubercal 只含乘性、加性+乘性同拟合的一手来源是 Burke et al. (2018)**；
> 本工作项独立复核后**同意**该判断，并在 R1 的『借鉴点』中只主张乘性/空间结构与相对-绝对解耦，不主张加性。

### [P1SG-R1] Padmanabhan, N. et al. (2008). An Improved Photometric Calibration of the Sloan Digital Sky Survey Imaging Data. ApJ 674, 1217. arXiv:astro-ph/0703454v2；DOI 10.1086/524677
- **可核对标识**：arXiv:astro-ph/0703454v2（arXiv API 按标题检索命中，标题/第一作者 N. Padmanabhan/摘要逐字一致）；DOI 10.1086/524677（公开检索命中 IOP 文章页）。
- **借鉴点**：
  1. **同时解算标定参数与相对星流量**（摘要逐字："simultaneously solves for the calibration parameters and relative stellar fluxes using overlapping observations"）—— 把"逐帧空间乘法增益"写成重叠观测上的联合最小二乘，与我们的**真实数据代理测量**（`src/real_gain.py` 的差分联合拟合）同构；
  2. **相对标定与绝对标定解耦**（摘要逐字："decouples the problem of 'relative' calibrations, from that of 'absolute' calibrations"）—— 直接对应我们独立识别出的可辨识性结构：帧间差只能定出**相对**空间增益，绝对锚点必须由外部目录（Gaia F_syn）提供；
  3. **显式关注标定误差的空间结构**（摘要逐字："pay special attention to the spatial structure of the calibration errors, allowing one to isolate particular error modes"）—— 支持把 m(x,y) 显式建成低阶空间曲面而不是单一标量。
- **不借鉴点**：
  1. 只有**乘性**项（零点和 + 平场），**没有**加性天光/梯度项（与上文 SNR §10.3 的独立核对一致）；我们不做"用背景吸收梯度"这一步；
  2. 全巡天规模的重叠网络（8500 sq.deg.）与迭代稀疏求解器不引入：ACSD 有 Gaia 绝对锚点，每帧可独立拟合；
  3. 其 ~1%(griz)/~2%(u) 相对精度依赖巡天重叠冗余与大气模型，不能搬到 12 板块 / 49 帧的小样本。
- **场景差异**：SDSS 是巡天（同一天区多次多夜多相机列覆盖，目标全巡天统一）；ACSD 是单帧→马赛克（每帧独立对 Gaia 定标，目标帧间/板块间乘性一致）。空间乘法增益的物理来源也不同：SDSS 以相机列平场+大气为主，我们以平场大尺度残差（Q3 实测为线性梯度为主）为主。
- **核对状态**：**已核对**（arXiv API 返回的标题/作者/摘要与引用一致；DOI 经公开检索命中）。

### [P1SG-R2] Burke, D. L. et al. (2018). Forward Global Photometric Calibration of the Dark Energy Survey. AJ 155, 41. arXiv:1706.01542v1
- **可核对标识**：arXiv:1706.01542v1（arXiv API 按标题检索命中，标题/第一作者 D. Burke/摘要逐字一致）。
- **借鉴点**：
  1. **前向标定**：把仪器响应建成**显式参数化模型**并前向施加，而不是事后经验改正 —— 与"把 m(x,y) 显式建成低阶多项式并乘到像素上"同构；
  2. 摘要逐字要求 "estimate the **spatial- and time-dependence** of the passbands of individual survey exposures" —— 把**逐曝光（逐帧）**的空间依赖当一等公民，支持"逐帧一个 m_k(p)"的建模；
  3. 目标 "stable in time and uniform over the celestial sky to one percent or better" 可作判据量级的参照。
- **不借鉴点**：
  1. FGCM 需要**辅助仪器数据 + 大气模型**（摘要逐字："combines data taken with auxiliary instrumentation at the observatory with data from the broad-band survey imaging itself and models of the instrument and atmosphere"）；ACSD 没有这些，只能从星点 + Gaia 目录反演；
  2. 它估计的是 **passband（随波长）** 的空间-时间依赖，我们估计**灰度乘法增益**（不含颜色项）；
  3. 其大气/仪器模型复杂度不引入（冻结链不接受外部大气模型）。
- **场景差异**：DES 是巡天、grizY 五波段、宇宙学目标、要求 1% 天区均匀性；ACSD 是 M42 等单目标多夜多望远镜、单波段 Red、以马赛克接缝一致为目标（Q1/Q3 实测接缝 1–3%）。
- **核对状态**：**已核对**（arXiv API 返回的标题/作者/摘要与引用一致）。

### [P1SG-R3] Murphy, J. (2019). PhotometricMosaic v1.0（PixInsight 脚本）
- **可核对标识**：本地源码副本 `run/RELEASE-02/pmosaic/PhotometricMosaic/`。`PhotometricMosaic.js` 头部逐字：`// Version 1.0 (c) John Murphy 20th-Oct-2019`，以及 `// Download it from the official website:` / `// https://astroprocessing.com/`。实现文件清单（`lib/Gradient.js`、`lib/SampleGrid.js`、`lib/StarDetector.jsh` 等）已核对。
  **注意（与上文 §7.10/§9 一致）**：官方文档 URL 的可达性**本工作项未验证**（上文报告为死链且无存档快照）；本条目只引**源码副本**这一可核对标识。
- **借鉴点**：
  1. **乘性项与加性项严格分离**：乘性 scale 由**星点测光**定，加性差值曲面只承担梯度 —— 这是"空间乘法必须用星点估、不能从背景拟合"的工程先例；
  2. 加性梯度用**曲面样条**（surface spline）而非低阶多项式，说明加性项可以很灵活；
  3. `DEFAULT_STAR_FLUX_TOLERANCE 1.5` / `DEFAULT_OUTLIER_PERCENT 2` 给出离群剔除量级的工程参照。
- **不借鉴点**：
  1. PMM 是**两图平面马赛克**，乘性 scale 是**整图一个标量**（无空间自由度）+ 加性曲面；我们要的是**逐帧空间乘法曲面 m_k(x,y)**；
  2. 其加性样条在平面像素坐标上做，ACSD 的 Phase2 加性面在球面上做（`lib/algorithms/coverage/src/sky_plane.cpp`），坐标模型不可照搬；
  3. 曲面样条自由度很高，与"低阶、去掉主要残差、不管高阶"的要求相反。
- **场景差异**：PMM 是两张已配准平面图求相对增益+相对梯度，一次性、交互式；ACSD 是 49 帧 / 12 板块 / 2 台望远镜、单帧独立对 Gaia 定标 + 球面重建，需要逐帧可复现、可 provenance、fail-closed 的自动化管线。
- **核对状态**：**已核对**（本地源码头部版本/作者/URL 逐字核对；`lib/` 文件清单已核对）；官方文档 URL 可达性**未核**（已在上文登记）。

### 本工作项明确**未**收录（核对失败）
- Tukey biweight 的 `c = 4.685` 常数出处：未能从可核对一手文献确认，**不收录**。该常数本轮**原样继承**主线冻结规范（`docs/science/PHOTOMETRY.md` SCI-PHOT-001 / `star_matcher.cpp` 的 `_TUKEY_C`），**未重新推导、未改动**。
- Pan-STARRS1 测光定标（Schlafly et al.）：未能通过 arXiv 检索核对到正确 arXiv 号，**不收录**。
- Beaton & Tukey (1974) Technometrics 16, 147：DOI 重定向到出版商域，本环境无法跟随跨域跳转完成逐字核对，**不收录**。
---

## 帧级 SNR 定案（FRAME-SNR-CANON）新增条目

> 工作项 **FRAME-SNR-CANON**（RELEASE-02）。完整调研记录见 `frame-snr-survey.md`；
> 定案见 `../docs/frame-snr-canon.md`。**本轮所有条目均为一手抓取核对**；
> 未抓到的一律标「待核对」并写清尝试路径。

### [FSNR-01] Jones, R. L. (2016/2026). Calculating LSST limiting magnitudes and SNR (SMTN-002)
- **可核对标识**：https://smtn-002.lsst.io/ （HTTP 200，页面标注 DOI 10.71929/rubin/3408482，By: R. Lynne Jones）；
  实现落点 `lsst/pipe_tasks` commit `0e56ae0` 的 `python/lsst/pipe/tasks/computeExposureSummaryStats.py:1262-1319`。
- **借鉴点**：**唯一一手帧级 SNR 解析式** `SNR = C/sqrt(C/g + (B/g + σ_instr²)·n_eff)`，`n_eff = 2.266(FWHM/pixelScale)²`；
  天光 `B` 只进分母。ACSD 定案式 (2.4)(2.7) 与之同构（`n_eff → A_NEA = 1/ΣP_i²`）。
- **不借鉴点**：仅用于深度/极限星等 m5，非产品字段；`n_eff` 的 2.266 系数是 LSST 解析近似；
  它假设 gain 已知（ACSD 的 FITS 头拿不到）。
- **核对状态**：**已核对**（页面 + 源码 `grep -n` 行号 + commit 钉版本）。

### [FSNR-02] Bosch, J. et al. (2018). The Hyper Suprime-Cam Software Pipeline. PASJ 70, S5. arXiv:1705.06766
- **可核对标识**：https://arxiv.org/abs/1705.06766 ；全文 https://ar5iv.labs.arxiv.org/html/1705.06766 。
- **借鉴点**：Eq(32) `σ_i² = b + α(φ_i + ε_i)`，逐字 "where **b is the level of the background (before it is subtracted)**" ——
  「背景均值」与「背景噪声」分离的干净范式；matched filter Eq(28)(30)。
- **不借鉴点**：**HSC 没有任何帧级 SNR 标量**（论文从未写出 `α_MF/σ_MF`）；CModel/SdssShape 不定义误差。
- **场景差异**：HSC 分 coadd 与 visit 两层；ACSD normalize 是单帧。
- **核对状态**：**已核对**（ar5iv 全文逐字复核 Eq(28)/Eq(30)/Eq(32)/calexp 四处）。**待核对**：PASJ 卷页（OUP 403 Cloudflare）。

### [FSNR-03] Magnier, E. A. et al. (2020). Pan-STARRS Pixel Analysis: Source Detection and Characterization. ApJS 251, 5. DOI 10.3847/1538-4365/abb82c. arXiv:1612.05244
- **可核对标识**：源码 SVN `https://svn.panstarrs.ifa.hawaii.edu/trac/ipp/export/HEAD/trunk/` + `psModules/src/objects/pmSourceIO_CFF.c` 等；
  版本钉扎 `export/43094` 与 `export/HEAD` 的 `psphotSourceStats.c` md5 一致。
- **借鉴点**：局部天光**照测但不参与 SNR**（`psphotSourceStats.c:411-413` 逐字 "// the local sky is now ignored; kept here for reference only"）。
- **不借鉴点**：`Var = Σ w σ²`（权重**未平方**，`pmSourceMoments.c:215`）数学上不是加权和的方差；
  同名 `SN` 有三条不同口径（Kron/矩/PSF）；论文公式含 `s_i` 但实现 `sky ≡ 0`。
- **⚠️ 纠正**：psphot 核心论文是 **Magnier et al. 2020 ApJS 251, 5**；
  Waters et al. 2020 ApJS 251, 4（DOI ...abb82b）是 detrend/warp/stack 论文，全文 `psphot` 0 次。
  `github.com/panstarrs/ipp` **不存在**（GitHub API 404）。
- **核对状态**：**已核对**（SVN 原文 + `nl -ba` 行号 + arXiv API/Crossref 双核）。**部分核对**：方差图是否含天光泊松仅注释间接支持。

### [FSNR-04] Abbott, T. M. C. et al. (2021). The Dark Energy Survey Data Release 2. ApJS 255, 20. DOI 10.3847/1538-4365/ac00b3. arXiv:2101.05765
- **可核对标识**：https://sextractor.readthedocs.io/en/des_dr1/Photom.html （DES 官方 SExtractor fork，HTTP 200）；
  配置 https://raw.githubusercontent.com/DarkEnergySurvey/multiepoch/master/etc/20160629_sex.config 。
- **借鉴点**：`FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))`，`p_i` 逐字 "**subtracted from the background**"，
  `σ_i` 逐字 "estimated from the local background" —— 「分子扣背景 / 分母承载天光」的权威范式；
  官方自承误差是 "a **lower limit** of the true uncertainty"（表述纪律）。
- **不借鉴点**：误差完全外包给局部背景 RMS；`BACKPHOTO_TYPE GLOBAL` 在强梯度下不够。
- **⚠️ 纠正**：DES DR2 论文是 **arXiv:2101.05765**（任务书给的 2101.02242 是 Brucalassi et al. 的 Ariel 光谱论文）。
- **核对状态**：**已核对**（官方 fork 页 + DR1/DR2 PDF 抽文 + 配置文件行号）。

### [FSNR-05] Bertin, E. & Arnouts, S. (1996). SExtractor: Software for source extraction. A&AS 117, 393. DOI 10.1051/aas:1996164
- **可核对标识**：master 分支（`configure.ac` 版本 **2.29.0**）`src/param.h`、`src/winpos.c`、`doc/src/Photom.rst`。
- **借鉴点**：孔径误差规范式 `FLUXERR = sqrt(Σ(σ_i² + p_i/g_i))`；
  **`SNR_WIN = FLUX_WIN/FLUXERR_WIN`**（`src/winpos.c:289`，`src/param.h:134`）证明「窗口口径本身不会被天光抬高」。
- **不借鉴点**：误差不含天光估计自身不确定度；手册与实现不同步（`SNR_WIN` 只在源码里）。
- **⚠️ 纠正**：`FLUX_GAUSS` **不存在**（`param.h` 参数表零命中）。
- **核对状态**：**已核对**（源码级）。**未做**：与 SExtractor 二进制对拍（本环境无可执行文件）。

### [FSNR-06] Stetson, P. B. (1987). DAOPHOT: A computer program for crowded-field stellar photometry. PASP 99, 191. DOI 10.1086/131977
- **可核对标识**：IRAF 官方帮助页 `https://iraf.readthedocs.io/en/latest/tasks/noao/digiphot/daophot/phot.html` 行 515–525；
  源码 `noao/digiphot/apphot/phot/apcomags.x` 行 28–55。
- **借鉴点**：`err = sqrt(flux/epadu + area·stdev² + area²·stdev²/nsky)` ——
  **`nsky` 项（天光估计自身的不确定度）必须进误差**，否则亮天空下系统性高估 SNR。
- **不借鉴点**：`epadu`/`readnoise`/`stdev` 全靠用户手填；天光是单一标量无空间变化。
- **核对状态**：**书目已核对**（Crossref DOI 10.1086/131977）；**实现级公式已核对**（IRAF 官方帮助页 + 官方源码一致）；
  **原文正文 待核对**（OpenAlex `oa_status: closed`；ADS 405 WAF、IOPscience 反爬）——
  **不冒充原文逐字引用**。

### [FSNR-07] Horne, K. (1986). An optimal extraction algorithm for CCD spectroscopy. PASP 98, 609. DOI 10.1086/131801
- **可核对标识**：Crossref `https://api.crossref.org/works/10.1086/131801`。
- **借鉴点**：`σ_F^-2 = Σ_i P_i²/σ_i²`、`SNR_F = F/σ_F` —— **本定案 (2.2)(2.3) 的规范出处**；
  `A_NEA = 1/ΣP_i²` 是其在 `σ_i = σ` 常数下的直接推论。
- **核对状态**：**书目已核对**（Crossref 逐字：PASP 98, 609, 1986-06）；**正文等式 待核对**（PASP 闭源）。

### [FSNR-08] Naylor, T. (1998). An optimal extraction algorithm for imaging photometry. MNRAS 296, 339–346. DOI **10.1046/j.1365-8711.1998.01314.x**
- **可核对标识**：Crossref 检索命中（标题/卷/页一致）。
- **借鉴点**：Horne 1986 在二维成像情形下的推广；"最优提取 ≠ 固定孔径求和"。
- **⚠️ 纠正**：任务书给的 DOI `10.1046/j.1365-8711.1998.01407.x` 是**另一篇论文**
  （Crossref 逐字 *"Deep hard X-ray source counts from a fluctuation analysis of ASCA SIS images"*, MNRAS 297, 41）。
- **核对状态**：**书目已核对**；**正文 待核对**。

### [FSNR-09] Irwin, M. J. (1985). Automatic analysis of crowded fields. MNRAS 214, 575–604. DOI **10.1093/mnras/214.4.575**
- **⚠️ 纠正**：任务书给的 `10.1093/mnras/214.3.575` 在 Crossref **无记录**；正确是 `.4.575`。
- **核对状态**：**书目已核对（Crossref）**；**正文与误差式 待核对**。

### [FSNR-10] PixInsight. New Image Weighting Algorithms（官方方法学文档）
- **可核对标识**：https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html （`web_fetch` HTTP 200；`curl` 被 406 拒绝）。
- **借鉴点（反面教材）**：官方自述 standard SNR（式[20]）"**corresponds to the ratio of powers standard
  formulation of signal-to-noise ratio**"，且其 Figure 9 说明逐字承认
  "a big airplane trail that introduces a **strong bias in the variance used as the numerator of the SNR
  equation** (see Equation [20])" ⇒ **加性图像内容（天光/梯度/尾迹）会抬高该定义的分子**。
  这是「假信噪比」的一手官方证据。
- **不借鉴点**：功率比型不可做 `w = SNR²/F_ref²` 换算；`c3`/`c4` 随版本漂移。
- **注意**：**PSFSNR（式[18]）的分子已扣局部背景**，故它本身不被天光均值抬高（与 standard SNR 不同）。
  页面公式为**图片**，本轮**未逐字提取符号**。
- **核对状态**：**已核对**（官方页面正文逐字；公式符号未提取）。

### [FSNR-11] 本工作项明确**未**收录 / 未做
- **`A_NEA = 1/ΣP_i²` 的一手出处**：**未定位** ⇒ 引用时必须写成"由 Horne 1986 推出"，**不得**安给某篇"提出 A_NEA 的论文"。
- **Labbé et al. 2003**：任务书点名为"最优孔径与 SNR"来源，本轮**未能核对到任何相关内容** ⇒ **不收录**，不编造。
- **Stetson 1987 / Naylor 1998 / Irwin 1985 正文**：闭源/不可达 ⇒ 只引实现级公式与书目。
- **SExtractor 二进制对拍**：本环境无 `sex`/`extract` ⇒ **未做**（只做源码级核对）。
- **HST/哈勃数据**：本工作区**无**（全仓 `find -iname '*hst*' / '*hubble*'` 命中 0）⇒ 真实数据实验改用真实实拍 M42 帧。
- **DrizzlePac Handbook "proportional to the inverse variance"**：Confluence CQL 全文检索 `totalSize: 0` ⇒ **该说法不存在**，正确表述是 `W = 1/(Var × scale⁴)`。


