# Science Scope

> 上游：ASTROCS_DESIGN.md §1（项目定位）、§2（核心科学方法）

## 目的

定义 Astro Celestial Sphere Database（ACSD） 科学处理范围与权威链入口。

## 科学定义

ACSD 从多帧天文 CCD 图像估计统一的天球辐射场（HiPS signal）及其
不确定性（variance/ivar），并输出标准 IVOA HiPS 产品。

## 处理链

1. 单帧校准（bias/dark/flat/cosmetic）；
2. 星点检测/PSF/astrometry/photometric calibration；
3. 空背景噪声模型（噪声模型 A = `NoiseWeightModelV1`，唯一生产模型 → ivar）；
4. 球面 Drizzle（线性通量守恒重建 + 方差传播）；
5. Phase2：coverage union → 控制采样 → UPM 联合加性校准 → 排异 →
   ivar 加权积分 → HiPS。

## 变量/单位

- **标度类别封闭词表**（唯一权威 = `docs/standards/NUMERIC_STANDARD.md`「量纲与标度」节）：
  `raw_adu` / `calibrated_adu` / `photo_scaled_adu` / `surface_brightness` / `synthetic_flux`。
- 信号链：`raw_adu`（输入亮场）→ `calibrated_adu`（校准后，ADU）→
  **`photo_scaled_adu`（施加逐帧测光标度后，`x′ = α·x`，`α = frame photscal`，逐帧取值）** →
  `surface_brightness`（HiPS 产品，`ADU/sr`）。e⁻ 只在调用方另行给出 gain 换算后才成立（本链不建模 gain）。
  - **可判定条件**（产品处于哪一档由产品自身声明，不靠推断）：Phase1 帧面由
    `p1_phot.json#photometry_applied` 与 `#photscales`（`DATA-P1-PHOTPROV-001`）判定；
    HiPS 产品由 `BUNIT` 与 `provenance` 判定。缺声明 ⇒ 显式拒绝，不得按同标度消费。
  - **α 是逐帧量，不是全仓常数**（实测：M42 生产 33 帧 `α ∈ [1.1387e-17, 6.0083e-17]`，跨 5.28×；
    `run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`）。
- **标度律（强制）**：`x′ = α·x ⇒ Var′ = α²·Var`、`ivar′ = ivar/α²`；`S = F/A_cell ⇒ Var(S) = Var(F)/A_cell²`。
  一手证据（JCGM 100:2008 §5.1.2 式(10)）、合成实验与真实数据推导见 `docs/standards/NUMERIC_STANDARD.md`。
- 位置：RA/Dec 度（J2000）、HEALPix NESTED、tile+local xy；
- 光度：dex log10 比值、mag；variance：**面亮度域 `ADU^2/sr^2`，像素域 `ADU^2`**（量纲随承载面，见上）。
- 帧级 SNR / `source_snr` / `sparse_snr_layer`：无量纲比值（同一线性标度下 α 相消，故与标度类别无关）。

## 假设

- 每帧为同一 target 的多次曝光（dither/不同滤镜需正确分组）；
- 背景为局部平稳随机场（patch 尺度）；源星点稀疏可掩膜。
  - **适用域**：本条是 `NoiseWeightModelV1` 的 8×8 patch 稳健尺度估计（`docs/science/NOISE_MODEL.md` §5）
    的**唯一**适用域声明。前提成立时随机方差随信号线性增长（泊松口径）。
  - **失效判据（可执行、非退化）**：以 patch 尺度上的对数斜率
    `γ = dlog(patch 方差)/dlog(patch 中位信号)` 为判据——纯随机分量应给出 `γ ≈ 1`；
    `γ` 显著偏离 1 ⇒ 该 patch 估计器量的是**空间结构**而非随机分量，噪声场必须显式降级
    （退回已规定的全局常量场并登记 `degraded_reason`），**不得**按随机噪声消费。
  - **实测（本仓独立复算）**：M42 真实帧 8×8 patch MAD 稳健方差对 patch 中位信号的对数斜率
    `γ = 1.983`（corr 0.593，262 144 个 patch）⇒ **该真实域不满足 `γ ≈ 1`**，前提在本帧上不成立。
    证据：`run/SCI-FIX-SEMANTICS-01/evidence/m42_structure_contamination.json`。
    **诚实边界**：作为参照的「跨帧配对差」口径在同一批文件上给出 `γ = 2.766`（corr 0.362），
    因两帧的标度/天光不同（中位 4.674e-15 vs 2.879e-15）而**本身含确定性标度失配项**，
    故**污染幅度未被本次独立复算定量确认**；本节只冻结判据形式与「前提可被违反」这一事实。

## 有效域

见各科学文档；总体：深空成像，16-bit/32-bit FITS，标准 CCD/CMOS。

## 不保证

- 不保证完整 covariance matrix 产品（相邻像素相关已文档化）；
- 不保证光谱/运动学产品（非本管线范围）。

## 失效条件

- 无合格控制点/无重叠 → NO_DATA / UNDERDETERMINED 显式状态；
- 输入损坏 → INPUT_CORRUPT 显式错误（禁止猜测）；
- **标度不可判定**（无法从产品自身声明判定其标度类别）→ 显式拒绝（`rc=2`），
  不得按同标度消费（`docs/standards/NUMERIC_STANDARD.md`）；
- **局部平稳前提被违反**（patch 尺度结构污染，见 §假设的 `γ` 判据）→ 噪声场显式降级并登记，
  不得静默按随机噪声消费。

## 系统/随机误差

系统性：flat 残差、PSF 色差、测光零点漂移（QA 元数据化）；
随机性由**两个用途不同的方差面**承载，二者量纲相同、**不得互相替代**（`docs/science/NOISE_MODEL.md` §5/§5c）：
**背景方差面** = 空背景随机分量，由 `NoiseWeightModelV1` 以 **patch 稳健尺度（MAD→σ）** 估计并落成 `variance/ivar`，
其唯一基线是**经验空背景方差**（`source==0`，`noise_model.cpp:618`）；
**加权方差面** = 该像素的**总方差**，含**源光子散粒项**，供叠加与拟合的最优加权，其源项与常数项来自
`gain`/\`read_noise_e\` 与星点测光给出的源电平（`docs/science/NOISE_MODEL.md` §5c）。
「光子泊松 + 读出噪声」的参数式 `var_ADU = max(signal,0)/gain + (read_noise_e/gain)²`
**不得**作为背景方差面的来源，**必须**作为加权方差面的来源。
**适用域**：背景方差面 = 空背景散粒 + 读出的**经验合计**；系统项与 Drizzle 后协方差不在任一面内
（`docs/science/UNCERTAINTY_AND_COVARIANCE.md`）。

## 数值精度

精度按数据形态归属：**稠密数据**（与像素数同阶的面——图像面、球面累加器、方差/权重/覆盖面、HiPS tile 面）以 **FP32** 承载发布面；
**稀疏/有限数据**（帧级 SNR、WCS 解、星表匹配、测光定标）全程 **FP64**。
  - **计算与承载分离**：本层内部归约/累积用 FP64，FP32 是**落盘/发布 dtype**（块类型 `AIO_BLOCK_FLOAT32`/`AIO_BLOCK_FLOAT64` 由精度模式选择，`module_adapters.cpp:6355-6377`）。
  - **量纲后果（必须显式处理）**：任何**绝对**常数一旦随标度换算就会改变可表示性。方差地板 `1e-12`（单位 `ADU²`）按 `α²`（`α` 为逐帧测光标度，`α ∈ [1.1387e-17, 6.0083e-17]`）换算后为 `~1e-46`，在 FP32 中精确下溢为 `0`，而 `1/floor` 上溢为 `+inf`；
    该像素**必须**取不可用态 `(variance=0 ∧ ivar=0)`，禁止发布 `(0, +inf)`（`docs/science/NOISE_MODEL.md` §7「产品 dtype 成对不变量」、§9 ②③）。
  - **显式等价路径门**：UPM 稀疏模型与稠密缓存对同一输入的块输出按 `EXPECT_NEAR(..., 1e-12)` 判等（`lib/algorithms/coverage/tests/synthetic_gate.cpp:2849`，用例 `Phase2Upm.SparseEqualsDense`）；该门只覆盖 UPM 物化路径，不覆盖 FP32/FP64 承载面之间的差异。

## 参考文献

Fruchter & Hook (2002)；Zackay & Ofek (2017)；IVOA HiPS 规范。

## 参考文献（含参考代码库与许可证）

- Fruchter, A. S. & Hook, R. N. 2002, PASP 114, 144（DOI 10.1086/338393）：Drizzle 线性重建。
- Zackay, B. & Ofek, E. O. 2017, ApJ 836, 187/188：多图像点源最优检测/测光与 proper coadd。
- Horne, K. 1986, PASP 98, 609；Naylor, T. 1998, MNRAS 296, 339：最优提取与成像最优 PSF 光度。
- Newberry, M. V. 1991, PASP 103, 122；Janesick, J. R. 2001, SPIE PM83：CCD 噪声/gain。
- Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（SExtractor）：检测/背景/误差口径。
- Greisen & Calabretta 2002, A&A 395, 1061；Calabretta & Greisen 2002, A&A 395, 1077：FITS WCS Paper I/II。
- Górski, K. M. et al. 2005, ApJ 622, 759；Fernique, P. et al. 2015, A&A 578, A114：HEALPix/HiPS。
- IVOA HiPS 1.0（https://www.ivoa.net/documents/HiPS/）与 IVOA MOC 1.0（https://www.ivoa.net/documents/MOC/）：HiPS/MOC 互操作。
- Padmanabhan, N. et al. 2008, ApJ 674, 1217；Bertin, E. 2006, ASPC 351, 112（SCAMP）：相对光度联合定标。
- Rosner, B. 1983, Technometrics 25, 165；Maples et al. 2018, ApJS 238, 2：ESD/RCR 排异。
- 参考代码库（含许可证）：Astropy（BSD-3-Clause）、photutils（BSD-3-Clause）、astropy-healpix（BSD-3-Clause）、DrizzlePac（BSD-3-Clause）、ccdproc（BSD-3-Clause）、reproject（BSD-3-Clause）；SExtractor/PSFEx/SWarp/SCAMP（GPL-3.0）、healpy（GPL-2.0）、Siril（GPL-3.0）、LSST ip_isr（GPL-3.0）——GPL 代码只作行为对照，不复制进本仓；WCSLIB（LGPL-3.0）、CFITSIO（宽松许可）。完整清单与核验状态见 docs/references/SCIENTIFIC_REFERENCES.md §M。


## ID

SCI-SCOPE-001（本文件范围/假设/失效域入口）；SCI-CAL-* / SCI-AST-* / SCI-PHOT-* / SCI-PSF-* / SCI-NOISE-* /
SCI-DRZ-* / SCI-UPM-* / SCI-REJ-* / SCI-INT-*。
