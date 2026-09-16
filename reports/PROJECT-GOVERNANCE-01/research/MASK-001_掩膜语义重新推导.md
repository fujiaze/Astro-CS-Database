# MASK-001 掩膜语义重新推导（科学实验定论）

> 任务来源：负责人直令「这类显然有问题的，派 agent 重新推导，做科学实验，SCI 文档不一定全是对的」
> 依据：`ENGINEERING_SPEC.md §3`（科学正确性优先：独立证据证明文档错误时订正文档是**义务**）、
> `工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md`（四条判定规则 + claim 登记）
> 复现基线：仓库 HEAD 工作树（Linux amd64，python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 / g++ 14.2.0）
> 纪律：**零 git 写、零受控文件修改**（脚本与日志均在 `run/PROJECT-GOVERNANCE-01/MASK-001/`，未触碰 `问题扫描/**`、未改 SCI、未改实现）
> 被测面：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp` **生产源零改动**直连编译

---

## 摘要（一页）

| # | 问题 | 唯一结论 | 置信度 |
|---|---|---|---|
| 1 | `rmax=60 px` 的推导是否成立 | **不成立**。全仓不存在任何推导：`10` 与 `6` 是 `noise_model.cpp:376-377` 的代码字面量，被 `NOISE_ESTIMATION.md:134` 抄成「实现锚」，再被 `config/defaults.json:69-89` 标为 `authority_status:"sourced"` 且 `source_ref` **回指该 ALG 行**——循环引用，无第一性依据。从物理目的（空背景方差无偏）导出的正确量级是 **r ≈ 3–6×FWHM**（FWHM=3 px ⇒ 10–18 px），比 60 px 小 **3–6 倍** | **高** |
| 2 | 「掩膜半径与星亮度解耦」不变量是否正确 | **作为物理陈述错误**（EXP-B2 实测：同一 r=10 px 下，最亮星 F=10³→10⁶ ADU 使 σ_bg 偏差从 +0.02% 升到 **+2.29%**；无偏所需半径随 F 单调增大）。它只是「模块 ABI 只收坐标」的实现限制，**不是**物理结论；且该限制在生产调用点**不成立**（`orchestrator.cpp:4720-4730` 已在遍历的 `psf` 行里同时持有 `row[2]=flux`、`row[5]=fwhm`、`row[6]=amplitude`）。正确口径：`r_i = r_local(F_i, FWHM_i, k=0.1σ_bg)`，帧内上界 = 天空预算允许的最大值，硬上界 60 px | **高** |
| 3 | 掩膜覆盖过大时的正确行为 | **自适应半径（逐星 PSF/亮度 + 天空预算收缩），唯一推荐**。EXP-C3 权场效率损失（含失权帧）：P0 现行 **13.28%**、P2 降级打标 **13.28%**、P1 显式失败 **37.96%**、P3 仅按预算收缩 0.24%、**P4 逐星自适应 0.013%**；平场 worst abs(σ 偏差)：P0 6.9%、P2 8.4%、P1 2.6%、P3 4.8%、**P4 1.1%**。P5（小掩膜+统计剥离）权场很好但欠掩膜偏差 +2.5% | **高** |
| 4 | 是否需要改 SCI | **需要**，改动落在 4 处：`NOISE_MODEL.md` §4/:37、§5/:46、§6/:70、§7/:76、§8/:85、§10/:110、§11（补源污染 oracle）、§14（补默认值推导）；`NOISE_ESTIMATION.md` §13.1/:134、§13.2/:155-158；`config/defaults.json` 两条 `noise.*`；实现 3 处（掩膜构造 + ABI + 调用点）。**SCI 的 60 px 公式本身没错，错在「无据数字 + 把它写成物理不变量」**；「SCI 正确、实现错」不成立——实现与 SCI 文本逐字一致 | **高** |

**一句话**：`rmax = max(1,r0)·max(1,scale)` 这个**形式**可保留，但 60 px 必须从「唯一的默认掩膜半径」降为**硬上界**；实际半径必须由「源亮度 + PSF 尺度 + 天空预算」三者决定。现行默认在 256² 帧 50 星时使整帧 `rc=1`、`ivar≡0`（Phase2 全帧失权），在 1K² 800 星时 33% 帧失权、成功的帧 σ_bg 偏差达 −4.6%，在方差有梯度的帧上即使成功也把空间场退化为常量场（梯度恢复比 1.000 vs 真值 1.5）。

---

## 1 权威原文（逐条 文件:行 + 逐字原文）

### 1.1 最高约束：科学正确性优先

**A. `ENGINEERING_SPEC.md:23-31`（§3 科学代码红线）** —— 逐字：

```text
## 3. 科学代码红线（最高优先级）

- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差 **不可随意修改**；
- **科学正确性优先（负责人指令）**：docs/science/** 与 docs/algorithms/** **必须科学正确**。当**独立证据**
  （外部标准/文献/可复现实验）证明文档与标准或事实不符时，**订正文档是义务，不是例外**；流程 = **变更 claim**
  （记录证据、影响面、版本递增）+ 一致性回归。**禁止**以「文档已冻结」为由保留已知错误...
- 反向同样成立：当文档**已被证明正确**而实现不符时，改实现；
```

**B. `工程控制/PROJECT-GOVERNANCE-01/SCIENCE_CORRECTNESS.md:11-17`（四条判定规则）** —— 逐字：

```text
| 文档与**外部标准/文献**不符（如 FITS WCS Paper II、IVOA 规范） | **改文档**（+ 改实现，若实现也随之错） | 执行行，凭标准原文 + 实验 |
| 文档与**可复现实验**不符（如蒙特卡洛证明某默认值统计上不成立） | **改文档**（或改实现，取决于哪边被证明错） | 执行行，凭实验（脚本 + 输出 + 样本量） |
| 文档**自相矛盾**（同文两处互斥） | 判定哪一处与标准/实验一致，改另一处 | 执行行，凭证据 |
```

即：本类裁决权在执行行，凭**外部标准原文 + 可复现实验**。本报告按此交付。

### 1.2 被诉科学条款（`docs/science/NOISE_MODEL.md`，状态 FROZEN T104 2026-08-23）

| 位置 | 逐字原文（改前） |
|---|---|
| §4 `:37` | `- 维度 h>0,w>0，data 非空且含有限值；min_samples（patch 样本数阈）默认 64；rmax 为固定值，不按星亮度/振幅缩放（API 仅 star_x/y 无 amplitude，见 §6）。` |
| §5 `:46` | `patch grid 8×8；星点掩膜 fixed conservative rmax = max(1,r0)·max(1,scale)（统一半径，不按亮度缩放）` |
| §5 `:52` | `全局兜底: 合格 patch variance 的稳健中位数 vmed` |
| §6 `:70` | `- 掩膜半径与星亮度解耦（API 无 amplitude 输入，统一 rmax；若需 PSF-aware adaptive mask 须先扩展 API 并重冻结）；` |
| §7 `:76` | `- **掩膜解耦不变量**：rmax 与输入振幅无关，亮星与暗星掩膜半径相同（fixed conservative 已冻结）。` |
| §7 `:77` | `- **空 support 不传播**：无合格 patch 时 ivar=0, r=1 拒绝加权，不产生伪有效权重。` |
| §8 `:85` | `| 无合格 patch | degenerate=1, 若 sky 样本<min_samples/2 或 robust_sigma 非有限/≤0 ⇒ ivar=0,r=1 拒；否则 degenerate=1 全局常量场 has_spatial_field=0, r=0 fallback | noise_model.cpp:235-260 |` |
| §10 `:110` | `- 将掩膜改为按振幅/星亮度自适应而不扩展 API 并重冻结；` |
| §11 `:117` | `- **Gaussian 合成**：N(0,σ²) 空背景合成帧（σ=5 ADU），经验 σ_bg 在 5% 内复现（SNR-004）。` |
| §14 `:141` | `4. 默认值 min_samples=64 的导出依据（claim SC-002）：8×8 patch（P=64）下以本文件 §11 冻结的 5% oracle 为判据，min_samples=5 时单 patch 偏差 −19.2%... min_samples=64 为 −1.25%/−1.7%、通过率 92.8%...` |

> **§14:141 的存在很关键**：SCI 自己已经确立了「默认值必须有可复跑 MC 推导」的先例（SC-002 对 `min_samples`）。
> 掩膜半径 `rmax` 是同一族默认值，**却没有任何对应条款**——这是本任务的形式化缺口。

### 1.3 被诉算法条款（`docs/algorithms/NOISE_ESTIMATION.md`）

| 位置 | 逐字原文（改前） |
|---|---|
| §13.1 `:134` | `| ALG-NOISE-001 | snr_noise_model_v1_default_config | noise_model.cpp:371-384（默认 8×8/r0=10/scale=6/clip 5.0/min 64/rounds 2/spatial 1/floor 1e-12） |` |
| §13.2 `:155-158` | `- 掩膜统一半径 rmax = max(1, source_mask_radius_px)·max(1, mask_radius_scale) = 默认 10·6 = **60 px**（noise_model.cpp:169-172），对所有星统一，不按振幅/星等缩放（API 无 amplitude 输入）；amps 向量残留未用（:166,171 (void)amps）。` |

**关键事实**：ALG §13.1/:134 **只是把代码默认值列出来**，全节无一句数值推导；`:155` 的「60 px」是**对代码的复述**，不是独立结论。

### 1.4 默认值登记（`config/defaults.json:69-89`）

> **行号口径**：本节行号为本轮进场时（2026-09-17 02:2x）的文件快照。工作树中存在**并发会话**对
> `config/defaults.json` 其他条目的改动（CFG-002：新增 `registry_ref` 5 行 + 2 处无关条目），使 `noise.*` 段整体下移 5 行；
> **`noise.source_mask_radius_px` / `noise.mask_radius_scale` 两条本身未被任何会话改动**
> （本轮末复核：现值仍为 10 / 6，`source_ref` 仍指 `docs/algorithms/NOISE_ESTIMATION.md:134`）。

```json
{ "key": "noise.source_mask_radius_px", "value": 10,
  "constraint": ">= 1；参与 rmax = max(1, source_mask_radius_px) * max(1, mask_radius_scale)",
  "authority_status": "sourced",
  "source": "docs/algorithms/NOISE_ESTIMATION.md:134（默认 r0=10；SCI 只冻结 rmax 公式不给数值）",
  "source_ref": {"path": "docs/algorithms/NOISE_ESTIMATION.md", "line": 134},
  "note": "数值默认 10 仅 ALG §13.1 列出；语义公式见 docs/science/NOISE_MODEL.md:46。同文件 :155。" }
{ "key": "noise.mask_radius_scale", "value": 6,
  "source_ref": {"path": "docs/algorithms/NOISE_ESTIMATION.md", "line": 134} }
```

**循环链实证**：`config/defaults.json` →(source_ref)→ `NOISE_ESTIMATION.md:134` →(内容)→ `noise_model.cpp:376-377` →(被 defaults.json 读取)→ 回到起点。
`authority_status:"sourced"` 在这条链上**没有任何科学来源**。ALG §13.3a 第 7 行（`NOISE_ESTIMATION.md:195`）亦自认：
「`source_mask_radius_px`(10) / `mask_radius_scale`(6) **保留 ALG:134**——SCI §5 只冻结 `rmax=max(1,r0)·max(1,scale)` 公式（:46）不给数值」。

### 1.5 R-5 附带发现（本轮任务起点）

`reports/PROJECT-GOVERNANCE-01/research/R-5_噪声SNR与统计口径.md:836-838` 逐字：

```text
### 5.14 附带发现（需登记，不属本轮任何原议题）：rmax=60 px 默认掩膜导致整帧退化

EXP-5 实测：256×256 帧、50 颗星 ⇒ rc=1（ivar_bg_global=0，Phase2 权重全失），且**与 min_samples 取值无关**。
默认掩膜半径 rmax = max(1,10)·max(1,6) = 60 px（noise_model.cpp:144-147、ALG §13.2:154）在 256² 帧上单星即覆盖 17% 面积。
**建议**：单独立条...本轮**不代为裁决修法**：其权威落点在 docs/science/NOISE_MODEL.md §6:67 的
"掩膜半径与星亮度解耦"冻结条款，改动需负责人批准。
```

本轮（MASK-001）即负责人对该条的派单：**代为裁决**。
---

## 2 外部依据（同类天文管线的标准做法，逐字原文 + URL）

### 2.1 SExtractor（Bertin & Arnouts 1996；SExtractor 2.24.2 官方文档）

出处：*Modeling the background*，<https://sextractor.readthedocs.io/en/latest/Background.html>（2026-09-17 抓取）

> "To compute the background map, SExtractor makes a first pass through the pixel data, estimating the local background in each mesh of a rectangular grid that covers the whole frame. **The background estimator is a combination of κ σ clipping and mode estimation, similar to Stetson's DAOPHOT program.** Briefly, **the local background histogram is clipped iteratively until convergence at ±3σ around its median.**"

> "**The choice of the mesh size BACK_SIZE is very important. If it is too small, the background estimation is affected by the presence of objects and random noise.** Most importantly, part of the flux of the most extended objects can be absorbed into the background map. If the mesh size is too large, it cannot reproduce the small scale variations of the background. Therefore a good compromise must be found by the user. **Typically, for reasonably sampled images, a width of 32 to 512 pixels works well.**"

> "**While being a bit noisier, the clipped 'mode' gives a more robust estimate than the clipped mean in crowded regions.**"（图 3 说明：`32×32` 像素 mesh 被随机高斯轮廓污染）

**对本任务的约束**：SExtractor **没有**任何「固定像素半径的几何源掩膜」。它用 (a) 与帧尺寸/结构尺度挂钩的 mesh 尺寸（32–512 px，**不是** PSF 的固定倍数）、(b) κσ 迭代裁剪（**3σ**），靠稳健统计剥离源污染。**「统一半径 + 不随源变化」在主流管线中没有先例。**

### 2.2 photutils `Background2D`（astropy 官方包，v3.0.0）

出处：<https://photutils.readthedocs.io/en/stable/api/photutils.background.Background2D.html>（2026-09-17 抓取）

签名与默认值（逐字）：

> `Background2D(data, box_size, *, mask=None, coverage_mask=None, fill_value=0.0, **exclude_percentile=10.0**, filter_size=(3, 3), filter_threshold=None, **sigma_clip=<default: SigmaClip(sigma=3.0, maxiters=10)>**, bkg_estimator=None, bkg_rms_estimator=None, interpolator=None)`

> `mask` **"Masked data are excluded from the background and background RMS calculations. mask is intended to mask sources or bad pixels, but a background and background RMS value will be calculated for them based on interpolation of the low-resolution background and background RMS maps."**

> `exclude_percentile` — **"The percentage of masked pixels allowed in a box for it to be included in the low-resolution map. If a box has more than exclude_percentile percent of its pixels masked then it will be excluded from the low-resolution map. ... Note that completely masked boxes are always excluded. In general, exclude_percentile should be kept as low as possible to ensure there are a sufficient number of unmasked pixels in each box for reasonable statistical estimates. The default is 10.0."**

**对本任务的约束（三条硬约束）**：
1. **每个估计箱（box/mesh）的掩膜占比有显式上限**（默认 **10%**），超过即**排除该箱**——不存在「箱内 94% 被掩膜仍当合格控制点」的做法；
2. 箱内被掩膜的像素**不参与统计**，其背景由低分辨率图**插值**补——即「排除 + 插值」是标准降级路径，不是「全帧退化」；
3. 迭代裁剪默认 **3σ / 10 轮**（AstroCS 为 5σ / 2 轮），对欠掩膜的残余源翼更积极。

### 2.3 LSST / Rubin Observatory `SubtractBackgroundTask`

出处：官方源码 `lsst.meas_algorithms/python/lsst/meas/algorithms/subtractBackground.py`（main 分支；配置页 <https://pipelines.lsst.io/py-api/lsst.meas.algorithms.SubtractBackgroundConfig.html>）

逐字原文：

> `binSize` = pexConfig.RangeField( doc="**how large a region of the sky should be used for each background point**", dtype=int, **default=128**, min=1, )
> `ignoredPixelMask` = pexConfig.ListField( doc="Names of mask planes to ignore while estimating the background", dtype=str, **default=["BAD", "EDGE", "DETECTED", "DETECTED_NEGATIVE", "NO_DATA", ]**, )
> `statisticsProperty` = pexConfig.ChoiceField( doc="type of statistic to use for grid points", dtype=str, **default="MEANCLIP"**, ... )
> 单位证据：`nx = math.ceil(maskedImage.getWidth() / self.binSizeX)` ⇒ **binSize 单位是像素**，不是角秒。
> `undersampleStyle` — "**behaviour if there are too few points in grid for requested interpolation style (str, default 'REDUCE_INTERP_ORDER')**"

**对本任务的约束（两条，均已源码核实）**：
1. **源掩膜来自 `DETECTED`/`DETECTED_NEGATIVE` 掩膜面（探测足迹）**，不是固定像素半径：源的等照度足迹随亮度增大 ⇒ **掩膜尺寸与亮度正相关**。「半径与星亮度解耦」与 LSST 做法相反；
2. 当**网格点不足**时标准行为是**显式降级**（`REDUCE_INTERP_ORDER`：降低插值阶数）。
   **纪律性更正**：源码核实 LSST **没有**「单 bin 掩膜占比超阈即丢弃该 bin」的判据；唯一硬失败是整幅全掩膜
   （`if (maskedImage.mask.getArray() & badMask).all(): raise TooManyMaskedPixelsError("All pixels masked. Cannot estimate background.")`）。本节不引用未取得的原文。

### 2.4 SDSS / DECam CP / Legacy Surveys（同级管线的第三、四、五份独立证据）

**SDSS DR17 `photo`（Sky Measurements）**，<https://www.sdss4.org/dr17/algorithms/sky/> 逐字：

> "In the PHOTO pipeline, sky is estimated on **a rectangular grid of 128 pixels** (roughly 50 arcsec) by taking the median of **a set of 256 by 256 pixels centered on each grid point**. The version of photo used in DR7 and before simply interpolated bilinearly between these grid points ... this tended to erroneously include light from extended regions around bright galaxies."
> "... it identifies **BRIGHT (> 51 sigma) sources**. These sources are run through the deblender ... Models are determined for each object, and these are then subtracted away ..."
> "But for **saturated stars (brighter than about r = 14), the outer wings are fit to a power-law, and this wing is then subtracted.**"

**DECam Community Pipeline**（NOIRLab 官方 CP 文档 PL201_3）逐字：

> "using **source masks and medians** to eliminate source light"
> "**Saturation** is essentially the point where the accumulated charge in the CCDs can no longer be properly calibration[sic] ... adds a **saturation bit (bit 2)** in the data quality maps for those pixels"
> "The **footprint of each detection, which includes a small amount of boundary growth**, is then added to the data quality maps and weight map (as zero weight)"
（未取得该文档的背景 mesh 像素尺寸原文——已明确登记，不引用具体数值。）

**Legacy Surveys DR10（DECam 巡天，掩膜半径–星等的显式公式）** —— 这是「半径与亮度解耦」最直接的**反例**：

> MASKBITS BRIGHT (bit 1): "touches a pixel within **half of the locus of a radius-magnitude relation**. Set for Tycho sources with MAG_VT < 13 and Gaia stars with G < 13."
> MEDIUM (bit 11): "... **The MEDIUM radius is twice the BRIGHT radius at the same magnitude.**"
> 官方源码 `legacypipe/py/legacypipe/reference.py` L352-357：
> `def mask_radius_for_mag(mag): ... return 1630./3600. * 1.396**(-mag)`

⇒ **一条在役生产管线把掩膜半径写成星等的指数函数**（越亮半径越大）；且天空估计的做法是
"by **detecting and masking sources**, then computing medians in **sliding 512-pixel boxes**"（<https://www.legacysurvey.org/dr10/description/>）。

**photutils 的标准掩膜构造流程**（User Guide, Masking Sources）逐字：

> "One method to create a source mask is to use a segmentation image ... we use `detect_threshold` ... to get a rough estimate of the threshold at the **2-sigma background noise level** ... then we use the `make_source_mask()` method with a **circular dilation footprint** to create the source mask"（示例 `circular_footprint(radius=10)`）

⇒ 掩膜 = **探测分割 + 膨胀**（尺度与 PSF/源尺寸对应），阈值锚在**背景噪声的 σ 上**——与 §3.3 采用的判据同源。

### 2.5 外部依据小结（可裁决命题）

| 命题 | SExtractor 2.25 | photutils 3.0 | LSST meas_algorithms | SDSS photo / DECam CP / Legacy | 裁决 |
|---|---|---|---|---|---|
| P-A：掩膜尺度应由 PSF/源足迹/亮度决定，而非固定像素数 | mesh 尺寸与结构尺度挂钩（BACK_SIZE 默认 64 px，推荐 32–512 px） | 掩膜 = 分割 + 膨胀；box 须**大于源的典型尺寸** | 掩膜 = `DETECTED` 足迹（随亮度） | SDSS：亮源建模型扣除 + 饱和星扣 wing；Legacy：**`r(mag)=1630″/3600·1.396^(−mag)`** | **外部五条一致否定「固定 60 px 统一半径、与亮度解耦」** |
| P-B：对「可用样本不足」必须有显式阈值/降级 | mesh 太小 ⇒ 官方明示「affected by the presence of objects」 | `exclude_percentile=10%` 排箱 + 插值 | 网格点不足 ⇒ `REDUCE_INTERP_ORDER`；全掩膜 ⇒ 显式异常 | DECam：DQ 位 + 零权重掩膜图 | **外部一致要求显式化**；AstroCS 现行既无占比上限、也无降级标签（只有 rc=0/1 两态） |
| P-C：估计箱内可接受的掩膜占比 | 无占比参数（靠 3σ 迭代裁剪） | **10%（默认，超限即弃箱）** | 无占比阈值（源码核实），仅全掩膜报错 | SDSS：256×256 中值 + 显式 mask | AstroCS 现行「箱内剩 64 px 即合格」在 256² patch 上=6%，在 4K² patch 上=**0.02%**，比 photutils 默认宽松至多 **500 倍** |
| P-D：迭代裁剪强度 | **±3σ** 收敛 | **3σ / 10 轮**（默认） | `MEANCLIP`（裁剪均值） | SDSS：中值 + 模型扣除 | AstroCS 的 **5σ / 2 轮**是外部最宽松的一档 |

---

## 3 实现事实（代码逐行 + 独立复现实验）

### 3.1 掩膜构造的逐行事实

`lib/algorithms/noise_snr/cpp/src/noise_model.cpp:159-189`（生产源，逐字）：

```cpp
std::vector<uint8_t> mask;
if (source_mask) {
    mask.assign((std::size_t)h * (std::size_t)w, 0);
    for (std::size_t i = 0; i < mask.size(); ++i) mask[i] = source_mask[i] != 0.0f;
} else if (star_x && star_y && n_stars > 0) {
    mask.assign((std::size_t)h * (std::size_t)w, 0);
    std::vector<double> amps;  amps.reserve((std::size_t)n_stars);
    // 无振幅信息时统一基础半径 (调用方只传坐标)
    const double r0 = std::max(1.0, c.source_mask_radius_px);
    const double rmax = r0 * std::max(1.0, c.mask_radius_scale);
    (void)amps;
    const double r2 = rmax * rmax;
    for (int i = 0; i < n_stars; ++i) { ...圆盘光栅化... }
}
```

`noise_model.cpp:371-384`：`memset(cfg,0,...)` 后仅设 7 个字段，`source_mask_radius_px=10.0`、`mask_radius_scale=6.0`
⇒ `rmax = 10×6 = 60 px`。`saturation_level` 保持 **0（= 饱和过滤关闭）**。

**三个直接后果（代码事实，无需实验）**：

1. **掩膜半径与 PSF 完全无关**：`r0/scale` 是常数，代码里没有任何 FWHM/σ_psf 输入；
2. **掩膜半径与亮度完全无关**：`amps` 被显式 `(void)` 弃用；`source_mask` 优先于星表（DISP-NOISE-006：两通道互斥）；
3. **饱和过滤默认关闭**：`saturation_level=0`，编排层（`orchestrator.cpp:4731-4734`）只从 header 覆盖 `GAIN/READNOI`，从不设置饱和电平。

**面板实测（EXP-A 的直接证据）**：同一帧、同一星表，仅把 PSF 的 FWHM 从 2 px 改到 5 px（2.5 倍），
`rc / mask_frac / n_qual / sigma_bg` **逐位相同**：

```text
 256x256 sparse  5 星 FWHM=2.0  rc=0 mask_frac=0.4650 nq=40 sig=5.0517
 256x256 sparse  5 星 FWHM=3.0  rc=0 mask_frac=0.4650 nq=40 sig=5.0517   <- 逐位相同
 256x256 sparse  5 星 FWHM=5.0  rc=0 mask_frac=0.4650 nq=40 sig=5.0517   <- 逐位相同
```

**机制**：三次运行的像素数据**并不相同**（PSF 从 2 px 展宽到 5 px），但 60 px 掩膜在三种 FWHM 下都把星的**全部**通量（含 Moffat 重翼）罩住，因此未掩膜像素集合与数值逐位相同 ⇒ 输出逐位相同。
**结论**：**掩膜半径的选取完全不消费 PSF 尺度信息**——半径够不够只由「星密度 × 帧面积」决定，与源的空间尺度无关。这正是 §5 结论 1/2 的实现证据，也说明 60 px 在 FWHM≤5 px 时**超配到掩盖了 PSF 的全部差异**。

### 3.2 独立复现 R-5 §5.14（EXP-A）

生产源零改动编译（`driver_mask.cpp` 直连 `noise_model.cpp`+`snr_science.cpp`），
合成帧 = `N(1000, 5²) ADU` 空背景 + Moffat(β=2.5) 星 + 幂律亮度 `dN/dF ∝ F⁻²`（F ∈ [200, 10⁵] ADU），8×8 patch，`min_samples=64`。

| 帧 | 星密度(星/10⁶px) | 星数 | 掩膜覆盖率 | n_qual | rc | 零权重像素 |
|---|---|---|---|---|---|---|
| 256² | sparse 76 | 5 | 0.432–0.465 | 40–43 | 0 | 0% |
| 256² | medium 305 | 20 | 0.904–0.923 | **9–12** | 0 | 0% |
| 256² | dense 763 | 50 | **1.0000** | **0** | **1** | **100%** |
| 1K² | sparse 76 | 80 | 0.572–0.602 | 61–64 | 0 | 0% |
| 1K² | medium 305 | 320 | 0.938–0.945 | 33–40 | 0 | 0% |
| 1K² | dense 763 | 800 | 0.9957–**1.0000** | 7 / 0 | 0 / **1** | 0% / **100%** |
| 4K² | sparse 76 | 1275 | 0.571 | 64 | 0 | 0% |
| 4K² | medium 305 | 5117 | 0.965–0.967 | 63–64 | 0 | 0% |
| 4K² | dense 763 | 12801 | 0.9994–0.9997 | 13–14 | 0 | 0% |

**42 组中 9 组 `rc=1`（整帧 100% 零权重）**；且**每一组的 FWHM∈{2,3,5} 输出逐位相同**（§3.1）。
R-5 §5.14 的「256² 帧 50 星 ⇒ rc=1」在本轮**独立复现成功**（不同星表/不同 PSF/不同随机种子）。

### 3.3 掩膜半径的无偏性实验（EXP-B）：半径应当由什么决定

**判据设定（a priori，非调参）**：掩膜存在的唯一物理目的是让天空样本「无源」；
要求残余源污染对 σ_bg 的偏差 ≤ 0.5%（SCI §11 冻结 5% oracle 的 1/10 余量）。
局域判据：掩膜到「源面亮度 = k·σ_bg」处（取 k=0.1 ⇒ 残余方差污染 < 1%·σ_bg²）：

```text
Gaussian: r_local = σ_p·sqrt(2·ln(F/(2πσ_p²·k·σ_bg)))
Moffat β: r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),  α = FWHM/(2√(2^(1/β)−1))
```

**结果（1024²，320 星，幂律亮度，Moffat β=2.5，12 seeds）**：

| r [px] | r/FWHM (FWHM=3) | 掩膜覆盖 | n_qual | σ 偏差均值 | 偏差 std | RMSE |
|---|---|---|---|---|---|---|
| 2 | 0.67 | 0.004 | 64 | +2.18% | 0.17% | 2.19% |
| 6 | 2.0 | 0.034 | 64 | +1.37% | 0.17% | 1.38% |
| **10** | **3.3** | 0.092 | 64 | **+0.13%** | 0.10% | **0.17%** |
| 15 | 5.0 | 0.192 | 64 | −0.07% | 0.12% | 0.14% |
| 20 | 6.7 | 0.314 | 64 | −0.07% | 0.12% | 0.14% |
| 30 | 10.0 | 0.565 | 64 | −0.10% | 0.18% | 0.21% |
| 40 | 13.3 | 0.768 | 62.8 | −0.09% | 0.21% | 0.23% |
| **60** | **20.0** | **0.960** | **35.0** | −0.28% | **0.71%** | **0.76%** |

**读数**：r=10 px（3.3×FWHM）已把偏差压到 +0.13%；把半径放大 6 倍到 60 px，
**偏差没有进一步改善**（−0.28%，仍在噪声内），但**估计量散度放大 7 倍**（0.10%→0.71%）、
合格 patch 从 64 掉到 35 ⇒ **RMSE(60 px)/RMSE(10 px) = 4.59**。60 px 是纯损失，无收益。

**PSF 依赖（同一 r 下改 FWHM，偏差单调恶化）**：

| r=10 px | FWHM=2 | FWHM=3 | FWHM=5 |
|---|---|---|---|
| σ 偏差 | −0.11% | +0.13% | **+1.44%** |
| 解析 r_local(k=0.1σ) | 13.8 px | 17.6 px | 23.8 px |

⇒ **同一个 60 px 对不同 PSF 意味着完全不同的污染水平**；半径必须随 PSF 尺度缩放。

**亮度依赖（同一 r 下改最亮星通量，FWHM=3 px，8 seeds）**：

| 最亮星 F [ADU] | r_local(k=0.1σ) | r=2 px | r=4 px | r=6 px | r=10 px | r=15 px |
|---|---|---|---|---|---|---|
| 10³ | 6.6 px | +0.39% | +0.05% | +0.01% | +0.02% | −0.02% |
| 10⁴ | 10.9 px | +1.08% | +0.66% | +0.12% | +0.00% | −0.00% |
| 10⁵ | 17.6 px | +2.13% | +1.86% | +1.30% | +0.17% | −0.00% |
| **10⁶** | **28.1 px** | +4.16% | +3.96% | +3.50% | **+2.29%** | +0.52% |

⇒ **无偏所需半径随源亮度单调增大**（对数律）。「掩膜半径与星亮度解耦」作为物理陈述在此**被直接证伪**；
作为「取帧内最大半径的保守近似」才是它唯一站得住的意思，而该近似在 256²/1K² 上代价是整帧失权（§3.5）。

**对照（负例）**：无星纯高斯帧在 r=0/8/60 px 下输出**逐位相同**（偏差 −0.03%，nq=64）
⇒ 现行 SCI §11 oracle 只用纯高斯帧，**在原理上无法发现任何掩膜半径错误**（见 §4）。

**小帧 regime（256²，20 星，FWHM=3，20 seeds）**：

| r [px] | 2 | 6 | 10 | 15 | 20 | 30 | 40 | 60 |
|---|---|---|---|---|---|---|---|---|
| 偏差均值 | +1.71% | +1.07% | +0.17% | +0.10% | +0.05% | +0.03% | −0.37% | +0.04% |
| 偏差 std | 0.66% | 0.60% | 0.49% | 0.74% | 0.56% | 1.04% | 0.95% | **4.14%** |
| n_qual | 64 | 64 | 64 | 64 | 61.8 | 47.6 | 30.4 | **8.2** |

⇒ 在**尚未失败**的 256²/20 星工况，60 px 已经把 σ_bg 的散度推到 **4.14%**（r=20 px 时 0.56%，**7.4 倍**），
仅剩 8.2 个合格 patch 支撑空间场——**权重场精度被静默劣化**，而 `rc` 仍为 0。

### 3.4 固定 60 px 的可用域（EXP-D，λ = N_s·π·rmax²/A）

λ = 随机像素被掩膜覆盖的期望重数（无量纲）；ρ = 掩膜面积/patch 面积 = 64π·rmax²/A。

| 帧 A | patch 边长 | ρ | n_qual≥8 全域成立 | 零失败域 | 「β 硬化」现象 |
|---|---|---|---|---|---|
| 256² = 65 536 | 32 px | **11.05** | λ ≤ **1.5**（N_s ≤ **8**） | λ ≤ 5.0（N_s ≤ 28） | λ≥3 时偏差散乱至 +11%（nq 1–6） |
| 1K² = 1 048 576 | 128 px | 0.69 | λ ≤ **5.0**（N_s ≤ **463**） | λ ≤ 8.0（N_s ≤ 741） | λ≥8 时 nq≤6，偏差 ±1.5–2.8% |
| 4K² = 16 777 216 | 512 px | 0.043 | λ ≤ **8.0**（N_s ≤ 11 867） | λ > 10（N_s ≥ 14 834 时 nq=5.7） | λ≥10 时 nq≤5，空间场退化 |

**定量域结论**：
- **保守通用域**：`N_s ≤ A/7540`（λ≤1.5）保证三种帧尺寸下 nq≥8。对 4K² ⇒ 2 225 星；1K² ⇒ 139 星；256² ⇒ **8.7 星**。
- 这说明固定 60 px 的可用域**窄到不现实**：一张 1K² 帧只要有 **139 颗** PSF 星，空间方差场就已不可靠；
  256² 帧只要 **9 颗**星就已越界。天文帧的探测星数通常为 10²–10⁴，**现行默认在工作区间之外**。
- 换用 §5 推荐半径（FWHM=3 px 的幂律亮度场，逐星 r 中位数 ≈ 9.5 px）后，同域扩张 **(60/9.5)² ≈ 40 倍**：
  4K² 可到 ≈ 8.9×10⁴ 星、1K² ≈ 5.6×10³ 星、256² ≈ 3.5×10² 星（EXP-C 中 4K²/12 801 星、掩膜 23.2%、nq=64 已验证）。

### 3.5 下游影响：rc=1 与「成功但退化」分别意味着什么

**(a) `rc=1` ⇒ 全帧零权重（代码路径已核实）**：`noise_model_impl` 在 `return 1` 前只置 `degenerate=1`，
`variance_bg_global/ivar_bg_global` 保持 `memset` 的 0；`fill_impl` 走「无空间场」分支，
把 `variance=0, ivar=0` 广播全帧（`noise_model.cpp:451-458`）。编排层对 `nret∈{0,1}` **同等接受**并写入
`variance/ivar` 块（`orchestrator.cpp:4750-4758, 4819-4824`，日志写 `DEGENERATE_GLOBAL`）。
⇒ **整帧 `ivar≡0`，Phase2 该帧权重全失**，且不产生任何 `NOISE_MODEL_STATUS=FAILED` 以外的失败信号。

**(b)「成功但退化」**：当 n_qual 很小但 >0 时，估计量仍输出 `rc=0` 与一个看似正常的 σ_bg，
但 (i) 散度爆炸（§3.3）、(ii) `n_qual<4` 时 `has_spatial_field=0` ⇒ **空间方差场静默退化为常量场**。
在带方差梯度的真实帧上，这直接把权重场变错（EXP-C3）：

```text
1024² medium(320 星) 现行 P0: 梯度恢复比 1.497 (真值 1.5)   eff_loss 0.05%
1024² dense (800 星) 现行 P0: 梯度恢复比 1.000 (真值 1.5)   eff_loss 1.35%   <- 常量场
256²  dense ( 50 星) 现行 P0: 75% 帧全失权, 其余 25% 梯度比 1.000
```

**(c) 附带缺陷（与掩膜同域，需一并登记）**：
1. `saturation_level` 默认 0 ⇒ 饱和像素不被剔除；掩膜收缩到 PSF 尺度后，亮星饱和核的残留风险上升；
2. 兜底阈值不对称：patch 级要求 64 样本，而**全帧**兜底只要求 `min_samples/2 = 32` 像素
   （`noise_model.cpp:250`）——即 32 个像素可以为整帧 4K² 定权重；
3. `R-5` 已登记的 `DISP-NOISE-006`（`source_mask` 与星表通道互斥）在自适应半径方案下会放大：
   两个通道必须用同一套半径语义。

---

## 4 门禁自审（为什么现有门禁全部是绿的）

本轮**实际执行**了现役门禁（生产源零改动、只读）：

| 门禁 | 命令 | 结果 | 对本缺陷的判别力 |
|---|---|---|---|
| NumPy 独立 Oracle（含 default_config 逐字段断言） | `python3 lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py` | **PASS 92/92**（EXIT=0，`logs/gate_numpy_oracle.log`） | **零**；且第 325-326 行**断言** `source_mask_radius_px=10 / mask_radius_scale=6 (rmax=60px)` —— 门禁把缺陷值**冻结成合同** |
| p1noise CTest 套件（units/properties/**oracle**/negative/scale_law/fill） | 直连生产源编译 `p1noise_tests_main.cpp + p1noise_tests_core.cpp` | **P1NOISE TESTS PASS (group=all)**（EXIT=0，`logs/gate_p1noise_core.log`） | **负向**：oracle 组的 `o2_mask_channel_parity`（"FIX-NOISE-D 掩膜解耦 — 亮星 (1e4) 与暗星 (10) 同坐标: 掩膜 rmax 与振幅无关 → 两帧 star 通道模型 bitwise 一致"）**把错误不变量机器化**；修好掩膜语义会让该用例变红 |
| SCI 合同 lint | `python3 tools/science_contract_lint.py docs/science/NOISE_MODEL.md` | **SCIENCE_CONTRACT_LINT_PASS** 15 sections（EXIT=0） | **零**：只校验 15 节结构 + claim ID + 锚点，无数值/语义判据 |
| `ci/checks.json` | 全表 grep | 与掩膜覆盖率/半径相关的检查项 **0 条** | **零** |
| SCI §11 Gaussian oracle | EXP-B4 直接复测 | 无星纯高斯帧在 r=0/8/60 px 下输出**逐位相同** | **结构性零**：oracle 帧**不含任何源**，掩膜在原理上不影响其结果 |

### 4.1 三个结构性缺口（本轮认定）

1. **缺「源污染 oracle」**：`ENGINEERING_SPEC.md §5.1` 要求「确定性合成数据生成器 + 独立 Oracle + 科学不变量」。
   现状是 §11 只有「纯高斯帧 σ 复现 5%」一条，**没有任何用例注入星场并检验 σ_bg 是否无偏**。
   `FIX-NOISE-D` 虽然注入了星（amp=1e4/10），但 (i) 只做两帧 bitwise 对比、(ii) 60 px 掩膜把两颗星的 PSF 全部罩住，
   **不检验 σ 恢复**。⇒ 一个把半径设成 1 px 或 1000 px 的实现，能通过全部 92 项 oracle + 全部 ctest。
2. **缺「箱内掩膜占比」上限**：外部标准（photutils 默认 10%）都有该阈值；本实现只要箱内剩 64 px 即合格
   ⇒ 4K² patch（512×512）等价于允许 **99.98% 被掩膜**（§3.1 实测 0.9997 覆盖率下仍报 nq=13、rc=0）。
3. **缺「降级标签」**：现行只有 `rc∈{0,1}` + `degenerate∈{0,1}`；"成功但只剩 7 个 patch + 常量场"与
   "成功且有 64 个 patch + 平面场"在 `photo_stats` 里只靠 `NOISE_N_PATCHES`/`NOISE_SPATIAL_FIELD` 两个裸数字区分，
   没有 `MASK_DEGRADED` 之类可被消费方 fail-closed 的显式标志。

### 4.2 门禁自审结论

本缺陷不是「谁漏跑了测试」，而是**判据集不完备 + 两条门禁把缺陷反向冻结**。
按 `ENGINEERING_SPEC.md §8`（「每项检查有正例与负例；豁免必须显式登记且只减不增」），
落地修复时**必须同批替换** `o2_mask_channel_parity` 与 numpy oracle 的默认值断言（见 §5.5 变更清单 V-4/V-5），
否则新实现会被旧门禁判红。这不是「为让测试变绿而调参」——是**判据从「半径与亮度无关」改为「半径随亮度/PSF 单调 + 无偏性达标」**，
且新门禁自带**负例**（EXP-B2 的固定小半径必须被检出为有偏）。

---

## 5 唯一结论 + 置信度

### 5.1 结论 1：`rmax=60 px` 的推导不成立；正确的定量域如下

**(a) 推导链不成立**（§1.3/§1.4 已给逐字证据）：
`config/defaults.json` ↔ `NOISE_ESTIMATION.md:134` ↔ `noise_model.cpp:376-377` 构成**闭环自引**，
链条上没有任何第一性依据；SCI §5:46 只冻结了**函数形式** `rmax=max(1,r0)·max(1,scale)`，从未给数值。

**(b) 从 SCI 冻结的物理目的重新推导**（空背景方差的**无偏**估计）：
掩膜的唯一目的是让天空样本无源。要求残余源污染对 σ_bg 的偏差 ≤ 0.5%（SCI §11 冻结 5% oracle 的 1/10 余量，a priori），
取「掩膜到源面亮度 = k·σ_bg」判据（k=0.1 ⇒ 残余方差污染 < 1%·σ_bg²）：

```text
Gaussian : r_local = σ_p·sqrt(2·ln(F/(2πσ_p²·k·σ_bg)))
Moffat β : r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1),  α = FWHM/(2√(2^(1/β)−1))
```

⇒ **r 不是常数，而是 (F, FWHM, σ_bg) 的函数**，随 F 对数增长、随 FWHM 线性增长。
FWHM=3 px、F=10⁵ ADU、β=2.5 时 r_local ≈ 17.6 px（5.9×FWHM）；F=10⁴ 时 10.9 px。
EXP-B 实测：**r=10 px 已把偏差压到 +0.13%**（RMSE 0.17%），而 r=60 px 的 RMSE 是 **0.76%（4.6 倍差）**。

**(c) 60 px 在什么条件下才合理**（三条同时成立）：

| 条件 | 定量界限（本轮实测/推导） |
|---|---|
| 帧面积足够吸收覆盖率 | `N_s ≤ A/7540`（λ = N_s π rmax²/A ≤ 1.5）⇒ 4K² 帧 ≤ 2 225 星；1K² ≤ **139** 星；256² ≤ **8.7** 星 |
| PSF 足够大（60 px 才不浪费） | 60 px = r_local(F=10⁵, k=0.1) 对应的 FWHM ≈ 10 px（Moffat β=2.5）⇒ **FWHM ≥ 10 px 时 60 px 才「刚好够」**；FWHM=3 px 的主流采样下超配 **3–6 倍** |
| 亮度分布不越界 | F ≤ 4.5×10⁶ ADU（FWHM=10 px 时）；F=10⁶ ADU 且 FWHM=3 px 时 60 px 才勉强够（r_local=28 px < 60） |

**结论**：60 px 作为**硬上界**（cap）成立；作为**唯一默认值**不成立。可用域 `N_s ≤ A/7540` 远窄于天文帧的实际星密度（1K² 139 颗即越界）。
**置信度：高**（证据：循环引用链逐行核实 + EXP-B 12~20 seeds MC + EXP-D 三帧尺寸 × 12 个 λ 扫描）。

### 5.2 结论 2：「掩膜半径与星亮度解耦」不变量作为物理陈述**错误**

**(a) 实验证伪**（EXP-B2，1024²，320 星，FWHM=3 px，8 seeds，Moffat β=2.5）：

| 最亮星 F | 10³ | 10⁴ | 10⁵ | 10⁶ |
|---|---|---|---|---|
| r=6 px 偏差 | +0.01% | +0.12% | +1.30% | **+3.50%** |
| r=10 px 偏差 | +0.02% | +0.00% | +0.17% | **+2.29%** |
| 无偏所需 r（解析） | 6.6 px | 10.9 px | 17.6 px | **28.1 px** |

同半径下，把最亮星从 10³ 提到 10⁶ ADU（3 个数量级），偏差从 0.02% 涨到 2.29% ⇒ **半径必须随亮度增长**。

**(b) 外部标准一致反对**（§2）：
Legacy Surveys 生产代码 `mask_radius_for_mag(mag) = 1630/3600 · 1.396^(−mag)`（半径是星等的指数函数）；
LSST 用 `DETECTED` 探测足迹；SExtractor/photutils 用分割+膨胀；SDSS 对饱和星扣 power-law wing。
**没有一条主流管线采用「与亮度解耦的固定半径」。**

**(c) 正确口径**（可直接写入 SCI）：

```text
r_i = clip( r_local(F_i, FWHM_i, k·σ_bg),  r_min,  R_CAP )
    r_local: 见 5.1(b)；k = 0.1（掩膜边缘残余面亮度 ≤ 0.1σ_bg）
    r_min  = max(1.5 px, 0.75·FWHM_i)     # 至少覆盖 PSF 核心
    R_CAP  = 60 px                         # 保留现行 60 px 作为硬上界（覆盖大 PSF 的重翼）
    σ_bg   ：两遍法——第一遍用 r=4·FWHM 估 σ_bg，第二遍回代。
             敏感度：σ_bg 偏差 1% ⇒ r_local 偏差 ≈ 0.2%（可忽略）
回调（信息缺失时）：
    F_i 不可用      ⇒ r_i = 4·FWHM_i（EXP-B 实测 F≤10⁵ 时残余偏差 ≤ 0.70%）
    FWHM_i 亦不可用 ⇒ 退回现行统一 rmax=max(1,r0)·max(1,scale) 并打 MASK_LEGACY 诊断标
```

**(d) 关于「API 无 amplitude」这一理由的事实核查**：
模块 C ABI 确实只收 `star_x/star_y`（`snr_estimator.h:161-166`）——但这**不是物理约束**：
生产调用点 `orchestrator.cpp:4720-4730` 正在遍历 `psf` 块的 9 列行，其中 `row[2]=flux`、`row[5]=fwhm`、`row[6]=amplitude`
**与 `row[3]/row[4]`（正在读取的 cx/cy）同在一块内存里**（字段语义锚：`orchestrator.cpp:4558-4562`、
`noise_model.cpp:340-347` 的 `snr_psf_fit_quality` 行映射）。⇒ 扩展 ABI 是**机械改动**，不是新的数据依赖。
SCI §6:70 的「API 无 amplitude」把**接口现状**写成了**物理假设**，这是本条的文档级错误。

**置信度：高**（实验 + 五条外部标准 + 调用点源码核实）。

### 5.3 结论 3：掩膜覆盖过大时的正确行为 = 自适应半径（逐星 + 天空预算）

**候选**（EXP-C/EXP-C3，生产源零改动；真值 σ_bg=5 ADU；成功帧与失权帧分别计数）：

- **P0_prod**：现行实现（r=60 固定；nq≥1 用 patch 中位数；nq==0 用全帧未掩膜像素、阈值 32）
- **P1_fail_closed**：r=60 固定；nq<8 ⇒ 拒绝加权（显式诊断失败）
- **P2_degrade**：r=60 固定；nq<8 ⇒ 全帧未掩膜像素（≥64）估计 + 打 degraded 标
- **P3_shrink**：均匀半径从 60 收缩到满足天空预算的最大值（无 PSF/亮度信息）
- **P4_psf_flux**：逐星 `r_i = r_local(F_i, FWHM_i, k=0.1σ_bg)` 再乘性收缩到预算（★推荐）
- **P5_clip**：小掩膜 r=2·FWHM + 3σ×5 轮统计剥离（以统计剥离替代几何大掩膜）

**天空预算的 a priori 推导**（非调参）：单 patch 相对误差 `c ≈ 1.152/√N`（R-5 EXP-1: N=64 时 14.4%），
中位数效率 1.25 ⇒ `SE(σ̂)/σ ≈ 1.44/√N_sky`；取 SE ≤ 1.5% ⇒ **N_sky ≥ 9216**；并要求 **nq ≥ 8**（中位数稳健 + 空间场可辨识）。

**结果 A：平坦背景（{256²,1K²,4K²} × {稀疏,中等,密集} × 6 seeds = 54 帧/策略）**

| 策略 | 成功帧率 | worst abs(σ 偏差) | 平均零权重占比 |
|---|---|---|---|
| P0_prod（现行） | 0.85 | **6.93%** | **0.148** |
| P1_fail_closed | 0.72 | 2.63% | 0.278 |
| P2_degrade | 0.85 | **8.44%** | 0.148 |
| P3_shrink | 1.00 | 4.77% | 0.000 |
| **P4_psf_flux** | **1.00** | **1.11%** | **0.000** |
| P5_clip | 1.00 | 2.79% | 0.000 |

典型格点：256² 密集（50 星）P0/P1/P2 **全部 100% 失权**，P3/P4/P5 全部成功；
1K² 密集（800 星）P0/P1/P2 有 33% 帧失权、成功帧 σ 偏差 −4.6%，P4 偏差 −0.00%。

**结果 B：方差梯度帧（grad=0.5，真值 var 比 1.5）——权重场误差**

| 策略 | 平均效率损失（含失权帧） | 最大效率损失 | 梯度恢复比（成功帧） |
|---|---|---|---|
| P0_prod（现行） | **13.28%** | 100% | 1.000（退化常量场） |
| P1_fail_closed | **37.96%** | 100% | — |
| P2_degrade | **13.28%** | 100% | 1.000（退化常量场） |
| P3_shrink | 0.244% | 1.29% | 1.36–1.51 |
| **P4_psf_flux** | **0.013%** | **0.057%** | **1.489–1.505** |
| P5_clip | 0.020% | 0.18% | 1.43–1.50 |

（效率损失 = `1 − Var_opt/Var_est`，即错误权场导致的加权均值方差相对膨胀；这是「权重场误差」的唯一标量口径。）

**唯一推荐：P4（逐星 PSF/亮度自适应半径 + 天空预算收缩 + 显式降级标志）。**
理由（全部由数字支撑）：
1. **无偏**：worst |σ 偏差| 1.11%（P0 6.93%、P2 8.44%）；
2. **不丢帧**：成功帧率 1.00、零权重占比 0（P0/P2 14.8%、P1 27.8%）；
3. **权场最准**：平均效率损失 0.013%（P0/P2 13.28%，P1 37.96%）；
4. **保住空间场**：梯度恢复比 1.489–1.505（真值 1.5），而 P0/P2 退化为 1.000；
5. **保留下限语义**：预算不可行时仍 `rc=1`，SCI §7「空 support 不传播」不变。

**P3 是「最小改动版」次优**（不改 ABI、只加预算收缩）：成功率 1.00、效率损失 0.244%，
但 worst |σ 偏差| 4.77%（4K² 密集帧上半径仍取到 ~57 px，只解决「整帧失权」，没解决「精度劣化」）。
**P5 是「零 ABI 改动版」第三**：权场极好（0.020%）但欠掩膜偏差 **+2.5%**（亮星 F=10⁵、FWHM=3 px 时 r=6 px 不够），
已逼近 SCI §11 的 5% 门，不建议单独采用。
**P1 显式失败被否**：它把「估计不出来」变成「整帧没有权重」，效率损失 37.96%（最差）；
§3.5(b) 已证「成功但退化」比失败更危险——**正确做法是让估计重新变得可用（自适应），而不是扩大失败面**。
**P2 降级打标被否**：与 P0 数值完全相同（现行实现在 nq==0 时**已经**是 P2），且全局常量场丢掉空间梯度。

**置信度：高**（54 帧 × 6 策略 × 两类真值场；失败/成功分别计数，未做选择性统计）。

### 5.4 结论 4：是否需要改 SCI —— **需要**；「SCI 正确、实现错」不成立

**判定表**（逐条款）：

| 条款 | 判定 | 谁错 |
|---|---|---|
| SCI §5:46 的**形式** `rmax=max(1,r0)·max(1,scale)` | 形式可保留（它只是"统一半径"的一种参数化），但必须补「r0/scale 不是自由常数，而是 (F,FWHM,σ_bg) 的导出量」且「它是**上界**」 | SCI **不完整** |
| 数值 `r0=10` / `scale=6` | SCI 从未给出该数值；ALG:134/defaults.json 给出的数值**无推导**，且 source_ref 自指 | **默认值层错** |
| §6:70「掩膜半径与星亮度解耦」 | 作为**物理假设**错误（EXP-B2 直接证伪 + 五条外部标准反例） | **SCI 错** |
| §7:76「掩膜解耦不变量」 | 同上，且已被实现门禁 `o2_mask_channel_parity` 机器化 | **SCI 错** |
| §11 验证 Oracle | **结构不完备**：唯一相关 oracle 用无源纯高斯帧，对掩膜半径零判别力（EXP-B4 实测 r=0/8/60 输出逐位相同） | **SCI 错（不完备）** |
| §8:85 退化表 | 未登记「掩膜覆盖过大导致整帧退化」这一**最常见**退化路径；把「sky 样本 < min_samples/2」写成唯一阈值；未写全帧兜底用的是**全部未掩膜像素** | **SCI 不完整** |
| §10:110 禁令 | 「禁止按振幅/亮度自适应」与 SCI 自身的物理目的冲突；应改写为条件句（未扩展 API 并重冻结时禁止） | **SCI 表述需改** |
| 实现 `noise_model.cpp` | 与 SCI 文本**逐字一致**（§3.1 已逐行核对），不是"实现违背文档" | **实现需随文档改** |

⇒ **SCI 与实现同错，且错在 SCI 端（无据默认值 + 错误不变量 + 不完备 oracle），实现只是忠实执行。**

### 5.5 变更清单（改前原文 → 建议改后原文 → 依据）—— **交前台派单落地，本 Agent 未改任何 SCI/实现**

> 每条对应 `SCIENCE_CORRECTNESS.md` 登记表所需的一条 claim（改了什么 / 依据证据 / 影响面 / 版本递增）。
> 建议同一 claim 下落地，claim 号建议 `SC-006`（MASK-001）。

**V-1 `docs/science/NOISE_MODEL.md §4:37`**

- 改前：
  > `- 维度 h>0,w>0，data 非空且含有限值；min_samples（patch 样本数阈）默认 64；rmax 为固定值，不按星亮度/振幅缩放（API 仅 star_x/y 无 amplitude，见 §6）。`
- 建议改后：
  > `- 维度 h>0,w>0，data 非空且含有限值；min_samples（patch 样本数阈）默认 64；
  > rmax 为**逐星半径的硬上界**（默认 60 px），实际半径 r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, rmax)
  > 由源亮度、PSF 尺度与天空预算导出（§5、§14）；调用方应提供逐星通量与 FWHM（生产 psf 块 row[2]/row[5]），
  > 未提供时按 §5 的回调规则降级并打 MASK_LEGACY 诊断标。`
- 依据：EXP-B/EXP-B2/EXP-D（§3.3/§3.4）；外部标准 §2（Legacy/LSST/SExtractor/photutils/SDSS）。

**V-2 `docs/science/NOISE_MODEL.md §5:46`**

- 改前：
  > `patch grid 8×8；星点掩膜 fixed conservative rmax = max(1,r0)·max(1,scale)（统一半径，不按亮度缩放）`
- 建议改后：
  > `patch grid 8×8；星点掩膜**逐星半径** r_i = clip(r_local(F_i, FWHM_i, k·σ_bg), r_min, R_CAP)：
  >   r_local = α·sqrt((F(β−1)/(π α² k σ_bg))^(1/β) − 1)（Moffat β，α = FWHM/(2√(2^(1/β)−1))）；
  >             Gaussian 极限 r_local = σ_p·sqrt(2·ln(F/(2πσ_p² k σ_bg)))
  >   k = 0.1（掩膜边缘残余面亮度 ≤ 0.1σ_bg ⇒ 残余方差污染 < 1%·σ_bg²，⇒ σ_bg 偏差 ≤ 0.5%）
  >   r_min = max(1.5 px, 0.75·FWHM_i)；R_CAP = 60 px（= max(1,r0)·max(1,scale)，**上界语义**，非默认值）
  >   天空预算收缩：取最大 s∈(0,1] 使 nq(s) ≥ 8 且 N_sky(s) ≥ 9216；s=0 ⇒ rc=1（保留 §7 空 support 不传播）`
- 依据：EXP-B（r=10 px 偏差 +0.13% vs r=60 px RMSE 0.76%）、EXP-B2（亮度依赖）、EXP-D（可用域）、EXP-C（预算阈值由 SE≤1.5% 导出并实测通过）。

**V-3 `docs/science/NOISE_MODEL.md §6:70`**

- 改前：
  > `- 掩膜半径与星亮度解耦（API 无 amplitude 输入，统一 rmax；若需 PSF-aware adaptive mask 须先扩展 API 并重冻结）；`
- 建议改后：
  > `- 掩膜半径**随源亮度与 PSF 尺度变化**：r_i = r_local(F_i, FWHM_i, k=0.1σ_bg)（§5）。
  >   模块 C ABI 现仅收坐标属**接口现状**，非物理约束：生产调用点（orchestrator psf 块 row[2]=flux / row[5]=fwhm）
  >   已持有该信息；ABI 扩展为可选参数数组，缺省时按 §5 回调规则降级；`
- 依据：EXP-B2（同半径下 F 10³→10⁶ 偏差 0.02%→2.29%）；外部标准 §2.4（Legacy `mask_radius_for_mag`）；调用点源码核实（§5.2(d)）。

**V-4 `docs/science/NOISE_MODEL.md §7:76`**

- 改前：
  > `- **掩膜解耦不变量**：rmax 与输入振幅无关，亮星与暗星掩膜半径相同（fixed conservative 已冻结）。`
- 建议改后：
  > `- **掩膜无偏性不变量**：默认掩膜下残余源污染对 σ_bg 的偏差 ≤ 0.5%（由 k=0.1σ_bg 与 §11 源污染 oracle 保证）；
  >   半径对 F 与 FWHM 单调不减；
  > - **天空预算不变量**：任何掩膜方案必须留下 nq ≥ 8 个合格 patch 且 N_sky ≥ 9216 像素；
  >   不满足 ⇒ 按 §5 收缩半径；收缩到 r_min 仍不满足 ⇒ rc=1 拒绝加权（§7 空 support 不传播保持）。`
- 依据：EXP-B/EXP-B2（无偏性）、EXP-C（预算阈值实测通过）、EXP-D（预算不可行的边界）。

**V-5 `docs/science/NOISE_MODEL.md §8:85`（退化表行）**

- 改前：
  > `| 无合格 patch | degenerate=1, 若 sky 样本<min_samples/2或robust_sigma非有限/≤0 ⇒ ivar=0,r=1 拒；否则 degenerate=1 全局常量场 has_spatial_field=0, r=0 fallback | noise_model.cpp:235-260 |`
- 建议改后：
  > `| 掩膜覆盖过大（nq<8 或 N_sky→0） | 先按 §5 天空预算收缩半径；仍不可行 ⇒ degenerate=1, ivar=0, r=1 拒（**不产生伪权重**） | EXP-A/EXP-D 复跑 |
  > | 无合格 patch（收缩后仍无） | degenerate=1；若**全部未掩膜** sky 样本 < min_samples ⇒ ivar=0,r=1 拒；否则 degenerate=1 全局常量场 has_spatial_field=0, r=0 fallback，并置 MASK_DEGRADED 诊断标 |`
- 依据：EXP-A（256²/50 星 rc=1 实测）、EXP-C3（常量场兜底使梯度恢复比 1.000）；与 photutils `exclude_percentile` 的显式降级精神一致（§2.2）。

**V-6 `docs/science/NOISE_MODEL.md §10:110`**

- 改前：
  > `- 将掩膜改为按振幅/星亮度自适应而不扩展 API 并重冻结；`
- 建议改后：
  > `- 在**未扩展 API 提供逐星通量/FWHM 且未重冻结**的前提下，擅自把半径改为按亮度自适应；
  > - 取消/绕过天空预算判据（nq≥8、N_sky≥9216）而直接产出权重场；`
- 依据：V-3/V-4；ENGINEERING_SPEC §3（科学语义订正与架构重构不得混提）。

**V-7 `docs/science/NOISE_MODEL.md §11`（新增 oracle，补结构性缺口）**

- 改前（该节只有 5 条，无源污染判据）：
  > `- **Gaussian 合成**：N(0,σ²) 空背景合成帧（σ=5 ADU），经验 σ_bg 在 5% 内复现（SNR-004）。`
- 建议**新增一条**（不改原条）：
  > `- **源污染 oracle（MASK-001）**：合成帧 = N(0,5²) 空背景 + N_s 颗 Moffat(β=2.5) 星
  >   （幂律亮度 dN/dF ∝ F⁻²，F∈[2×10², 10⁵] ADU，FWHM=3 px，星位随机）；
  >   默认掩膜下 |σ̂_bg/σ_bg − 1| ≤ 2%（12 seeds 均值）且 n_qualified ≥ 8、N_sky ≥ 9216；
  >   负例（门禁必须能红）：把半径固定为 60 px 且 256²/50 星 ⇒ 必须 rc=1（整帧退化）；
  >   把半径固定为 2 px 且 F_max=10⁶ ADU ⇒ 必须检出 |σ 偏差| > 2%。
  >   复跑：run/PROJECT-GOVERNANCE-01/MASK-001/expB_radius_bias.py、expC_policies.py、expD_domain.py。`
- 依据：EXP-B/EXP-B2（正例与负例的实际数值）、EXP-A（失败面）、EXP-D（可用域）。

**V-8 `docs/science/NOISE_MODEL.md §14`（新增默认值导出依据，仿 §14:141 对 min_samples 的先例）**

- 建议新增第 5 条：
  > `5. 掩膜半径默认值的导出依据（claim SC-006 / MASK-001）：以 §11 源污染 oracle 为判据，
  >   R_CAP=60 px、k=0.1、r_min=max(1.5 px, 0.75·FWHM)、N_sky≥9216、nq≥8；
  >   k=0.1 下 r_local(F=10⁵, FWHM=3, β=2.5) = 17.6 px，实测 r=10 px 偏差 +0.13%（RMSE 0.17%）；
  >   而统一 60 px 在 1024²/320 星上 RMSE 0.76%（4.6×），在 256²/50 星上整帧退化（rc=1）。
  >   来源 reports/PROJECT-GOVERNANCE-01/research/MASK-001_掩膜语义重新推导.md §3.3/§3.4。`
- 依据：EXP-B/EXP-D；与 §14:141（min_samples 的 MC 推导）同构。

**V-9 `docs/algorithms/NOISE_ESTIMATION.md §13.1:134、§13.2:155-158`**

- 改前：
  > `| ALG-NOISE-001 | snr_noise_model_v1_default_config | noise_model.cpp:371-384（默认 8×8/r0=10/scale=6/...） |`
  > `- 掩膜统一半径 rmax = max(1, source_mask_radius_px)·max(1, mask_radius_scale) = 默认 10·6 = 60 px ...`
- 建议改后：改为「实现锚 + 判据」两条，并**删除"默认 60 px"的表述**：
  > `- 掩膜半径 = 逐星 r_local(F_i,FWHM_i,k·σ_bg) 经天空预算收缩，硬上界 R_CAP = max(1,r0)·max(1,scale)
  >   = 默认 10·6 = 60 px（**上界**，见 SCI §5/§14）；`
- 依据：§1.4 循环引用链；§5.1(c) 的可用域。

**V-10 `config/defaults.json`（noise.* 两条）**

- 改前（§1.4 已给逐字）：`noise.source_mask_radius_px = 10`、`noise.mask_radius_scale = 6`，
  `authority_status:"sourced"` + `source_ref: NOISE_ESTIMATION.md:134`
- 建议改后：
  - 两条的 `source_ref` 改指 `docs/science/NOISE_MODEL.md §5` 的**实际陈述行**（消除循环引用），
    `note` 写明「上界语义，非操作默认值」；
  - **新增** `noise.mask_k_sigma = 0.1`、`noise.mask_r_min_px = 1.5`、`noise.mask_fwhm_floor_scale = 0.75`、
    `noise.mask_budget_min_patches = 8`、`noise.mask_budget_min_sky = 9216`，
    `source_ref` 全部指向 `docs/science/NOISE_MODEL.md` 的 §5/§14 行。
- 依据：SCIENCE_CORRECTNESS 判定规则（科学默认值必须引用 `docs/science/**` 的实际陈述）；ALG §13.3a 第 7 行同款要求。

**V-11 实现（3 处）**

1. `lib/algorithms/noise_snr/cpp/src/noise_model.cpp:159-189`：掩膜构造改为
   (a) 逐星半径数组（新 ABI 参数；NULL ⇒ §5 回调规则）、(b) 天空预算收缩（二分 s）、(c) 预算不可行 ⇒ `return 1`；
2. `lib/algorithms/noise_snr/cpp/include/snr_estimator.h` 两个入口
   （`snr_noise_model_v1` / `_v1_f64`）新增 **可选** 参数 `const double* star_flux, const double* star_fwhm`
   （NULL 兼容旧调用；同时新增 `mask_degraded` / `mask_radius_p50` / `mask_frac` 诊断字段）；
3. `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:4720-4749`：把已读到的
   `row[2]`(flux)、`row[5]`(fwhm) 一并传入；`NOISE_MODEL_STATUS` 增加 `DEGRADED_MASK` 取值。
- 依据：§5.2(d)（信息已在同结构体）；EXP-C（P4 的实测优势）；ENGINEERING_SPEC §3（科学改动与架构改动分开提交）。

**V-12 门禁（必须与实现同批，否则新实现被判红）**

1. `lib/algorithms/noise_snr/tests/p1noise/p1noise_tests_core.cpp:336-359` 的 `o2_mask_channel_parity`
   （断言"亮星与暗星掩膜 bitwise 一致"）**替换**为 `o2_mask_radius_monotone`
   （半径对 F、对 FWHM 单调不减；同 (F,FWHM) 时逐位可复现）+ `o3_source_bias_oracle`（§11 新 oracle）；
2. `lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py:325-326` 的默认值断言
   由「r0=10/scale=6 (rmax=60px)」改为「R_CAP=60px 为上界 + k/r_min/预算五参数」；
3. `ci/checks.json` 注册 V-7 的新 oracle 用例（正例 + 负例各一）。
- 依据：ENGINEERING_SPEC §8（每项检查有正例与负例）；SCIENCE_CORRECTNESS（变更 claim 必填"影响面"）。

### 5.6 落地顺序建议（前台派单）

1. **SC-006 claim 登记** + V-1..V-10 文档/默认值改动（科学语义改动，单独一提交）；
2. V-12 门禁替换 + 新 oracle（可与 1 同批，因为旧门禁断言的就是被替换的条款）；
3. V-11 实现改动（架构/ABI 改动，**单独一提交**，禁与 1/2 混提——ENGINEERING_SPEC §3）；
4. 复跑 `ctest -R p1noise_` + `noise_model_numpy_oracle.py` + 新 oracle；
   P4 策略须在 256²/50 星、1K²/800 星、4K²/12 801 星三档全部 rc=0 且 |偏差| ≤ 2%。

---

## 6 证据清单（命令、退出码、产物）

### 6.1 环境与基线

```console
$ cd "/workspace/Astro CS Database" && python3 -c "import sys,numpy,scipy;print(sys.version.split()[0],numpy.__version__,scipy.__version__)"; g++ --version | head -1; nproc
3.13.5 2.2.4 1.15.3
g++ (Debian 14.2.0-19) 14.2.0
16
```

**纪律声明**：本轮**零 git 写**（未执行任何 git 写命令）；**未修改任何受控文件**
（`docs/**`、`lib/**`、`config/**`、`tests/**`、`ci/**`、`问题扫描/**` 均未写）；
全部新增文件只落 `run/PROJECT-GOVERNANCE-01/MASK-001/`（本报告落在 `reports/PROJECT-GOVERNANCE-01/research/`）。

### 6.2 脚本与日志（全部在 `run/PROJECT-GOVERNANCE-01/MASK-001/`）

| ID | 脚本 | 命令（timeout 保护） | 退出码 | 日志 |
|---|---|---|---|---|
| EXP-0 | `exp0_replica_validation.py` | `timeout 600 python3 -u exp0_replica_validation.py` | 0 | `logs/exp0_replica_validation.log`（rtol 1e-9 / fill f32 max 5e-8，7 例全过） |
| EXP-A | `expA_failure_surface.py` | `timeout 3000 python3 -u expA_failure_surface.py` | 0 | `logs/expA_failure_surface.log/.json`（42 组，9 组 rc=1） |
| EXP-B | `expB_radius_bias.py` | `timeout 3000 python3 -u expB_radius_bias.py` | 0 | `logs/expB_radius_bias.log/.json` |
| EXP-C | `expC_policies.py` | `timeout 3600 python3 -u expC_policies.py` | 0 | `logs/expC_policies.log`、`logs/expC_policies_flat.json`、`logs/expC2_policies_gradient.json` |
| EXP-C3 | `expC3_gradient_weight.py` | `timeout 1800 python3 -u expC3_gradient_weight.py` | 0 | `logs/expC3_gradient_weight.log/.json` |
| EXP-D | `expD_domain.py` | `timeout 3600 python3 -u expD_domain.py` | 0 | `logs/expD_domain.log/.json` |
| 门禁-1 | 冻结 NumPy oracle | `timeout 600 python3 -u lib/algorithms/noise_snr/tests/p1noise/noise_model_numpy_oracle.py` | 0 | `logs/gate_numpy_oracle.log`（**PASS 92/92**） |
| 门禁-2 | p1noise CTest 套件 | `g++ -std=c++17 -O2 -I… p1noise_tests_main.cpp p1noise_tests_core.cpp noise_model.cpp snr_science.cpp -pthread` → 运行 | 0 | `logs/gate_p1noise_core.log`（**P1NOISE TESTS PASS (group=all)**） |
| 门禁-3 | SCI 合同 lint | `timeout 600 python3 tools/science_contract_lint.py docs/science/NOISE_MODEL.md` | 0 | `logs/gate_science_contract_lint.log`（**PASS 15 sections**） |
| 派生量 | `logs/derived_summary.log` | 由上述 JSON 汇总（RMSE 比、λ 边界、策略全域统计） | 0 | 同左 |

**公共构件**：

- `driver_mask.cpp` —— 生产源零改动直连驱动（cfg 半径/裁剪/rounds 可注，导出 fill 的 ivar 场）；
- `masklib.py` —— 合成帧（Gaussian/Moffat PSF、幂律亮度、方差梯度）、生产驱动封装、
  **NumPy 独立复算**（EXP-0 验证 rtol 1e-9）、广义距离场 `scaled_field`（逐星半径的精确掩膜与逐 patch 存活计数）、指标函数；
- 编译：`g++ -std=c++17 -O2 -I lib/algorithms/noise_snr/cpp/include driver_mask.cpp lib/algorithms/noise_snr/cpp/src/noise_model.cpp lib/algorithms/noise_snr/cpp/src/snr_science.cpp -pthread -o driver_mask`（退出码 0）。

### 6.3 关键原始输出（节选，完整见日志）

```text
# EXP-A（生产默认 rmax=60 px）
 256x256  dense(50 星)  rc=1  mask_frac=1.0000  nq=0   zero_w=1.0000     <- 独立复现 R-5 §5.14
1024x1024 dense(800 星) rc=0/1 各半  mask_frac=0.9957/1.0000  nq=7/0
4096x4096 dense(12801 星) rc=0  mask_frac=0.9994~0.9997  nq=13~14      <- 存活但空间场退化

# EXP-B（1024², 320 星, FWHM=3 px）
 r=10 px: bias=+0.00131 std=0.00103 rmse=0.00167 nq=64.0 mask=0.0916
 r=60 px: bias=-0.00279 std=0.00712 rmse=0.00765 nq=35.0 mask=0.9604
 RMSE(60)/RMSE(10) = 4.59

# EXP-C（平坦, 54 帧/策略）
P0_prod        ok=0.85 worst|bias|=0.0693 zero_frac_mean=0.148
P1_fail_closed ok=0.72 worst|bias|=0.0263 zero_frac_mean=0.278
P2_degrade     ok=0.85 worst|bias|=0.0844 zero_frac_mean=0.148
P3_shrink      ok=1.00 worst|bias|=0.0477 zero_frac_mean=0.000
P4_psf_flux    ok=1.00 worst|bias|=0.0111 zero_frac_mean=0.000
P5_clip        ok=1.00 worst|bias|=0.0279 zero_frac_mean=0.000

# EXP-C3（方差梯度 grad=0.5, 真值 var 比 1.5）
P0_prod        eff_loss_mean=0.13278 max=1.00000 grad_ratio(成功帧)=1.000
P1_fail_closed eff_loss_mean=0.37964 max=1.00000
P2_degrade     eff_loss_mean=0.13278 max=1.00000 grad_ratio=1.000
P3_shrink      eff_loss_mean=0.00244 max=0.01289 grad_ratio=1.36~1.51
P4_psf_flux    eff_loss_mean=0.00013 max=0.00057 grad_ratio=1.489~1.505
P5_clip        eff_loss_mean=0.00020 max=0.00181 grad_ratio=1.43~1.50

# EXP-D（λ = N_s·π·rmax²/A）
A=  65536 px²: nq≥8 到 λ≤1.5 (N_s≤8)      ; 零失败到 λ≤5.0 (N_s≤28)
A=1048576 px²: nq≥8 到 λ≤5.0 (N_s≤463)    ; 零失败到 λ≤8.0 (N_s≤741)
A=16777216 px²: nq≥8 到 λ≤8.0 (N_s≤11867) ; 零失败到 λ>10 (N_s≥14834 时 nq=5.7)
```

### 6.4 外部依据抓取记录（2026-09-17）

| 来源 | URL | 用途 |
|---|---|---|
| SExtractor「Modeling the background」 | <https://sextractor.readthedocs.io/en/latest/Background.html> | BACK_SIZE 32–512 px 推荐、±3σ 迭代裁剪、mesh 过小致源流量被吸收 |
| SExtractor 2.25.0 `config/default.sex` | <https://github.com/astromatic/sextractor/blob/2.25.0/config/default.sex> | 默认 BACK_SIZE=64 / BACK_FILTERSIZE=3 |
| photutils 3.0.0 `Background2D` | <https://photutils.readthedocs.io/en/stable/api/photutils.background.Background2D.html> | `exclude_percentile=10%` 排箱 + 插值；`SigmaClip(3σ,10)` |
| photutils User Guide「Background Estimation / Masking」 | <https://photutils.readthedocs.io/en/stable/user_guide/background.html> | box_size 须大于源典型尺寸；掩膜 = 分割 + 膨胀（2σ 阈值） |
| LSST `subtractBackground.py` | <https://github.com/lsst/meas_algorithms/blob/main/python/lsst/meas/algorithms/subtractBackground.py> | binSize=128 px、ignoredPixelMask 含 DETECTED、全掩膜异常、undersampleStyle |
| LSST `SubtractBackgroundConfig` 文档 | <https://pipelines.lsst.io/py-api/lsst.meas.algorithms.SubtractBackgroundConfig.html> | undersampleStyle=REDUCE_INTERP_ORDER |
| SDSS DR17 Sky Measurements | <https://www.sdss4.org/dr17/algorithms/sky/> | 128 px 网格 + 256×256 中值；BRIGHT 源建模扣除；饱和星扣 wing |
| DECam CP 官方文档（存档快照） | <https://web.archive.org/web/20220819004446/https://legacy.noirlab.edu/noao/staff/fvaldes/CPDocPrelim/PL201_3.html>（原站直连失败） | source masks + medians；饱和/DQ 位；探测足迹 + boundary growth |
| Legacy Surveys DR10 MASKBITS + legacypipe 源码 | <https://www.legacysurvey.org/dr10/bitmasks/>、<https://github.com/legacysurvey/legacypipe/blob/DR10.0.12/py/legacypipe/reference.py#L352-L357> | **`mask_radius_for_mag(mag)=1630/3600·1.396^(−mag)`**：掩膜半径 = 星等指数函数 |
| Legacy Surveys DR10 Sky Level | <https://www.legacysurvey.org/dr10/description/> | 「detecting and masking sources, then computing medians in sliding 512-pixel boxes」 |

**未取得逐字原文（明确登记，未引用数值）**：Valdes et al. 2014 (ASP Conf. 485, 379) PDF 取回失败；
DECam CP 背景 mesh 的具体像素尺寸（官方文档无该数值）；
LSST「单 bin 掩膜占比超阈即弃 bin」（源码核实**不存在**该判据，已据此更正 §2.3）。

---

## 附：本轮未做与不做的事

- **未改 SCI/ALG/默认值/实现/测试/CI**（全部改动以 §5.5 变更清单形式交前台派单）；
- **未碰 `问题扫描/**`**；
- **未为让任何测试变绿而调参**：所有阈值（k=0.1、N_sky≥9216、nq≥8、R_CAP=60）均由
  「σ_bg 偏差 ≤ 0.5% / SE ≤ 1.5% / 空间场可辨识」三个 a priori 判据导出，随后由 EXP-B/C/D 的独立数值验证；
  把 k 调大会同时抬高 P4 的 mask_frac 并降低其精度优势，实验表已给出该权衡的完整曲线。
