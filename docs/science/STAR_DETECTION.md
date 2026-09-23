# SCI-P1-STAR-001 — Phase1 星点检测（P1-STAR 冻结层）

> 上游：ASTROCS_DESIGN.md §2.1（测光星等坐标系）、§4.2（Phase1 节点流程）

> 状态: FROZEN
> 性质: 本页是 Phase1 星点检测的 SCI 冻结层落点，声明共享 SCI（PSF / PHOTOMETRY /
> ASTROMETRY，`docs/science/` 既有文档）的语义映射与 matrix 指向，格式沿用
> `STAR_PSF_ALGORITHMS` §11.5 先例。
> ALG 权威: ALG-STARDET-001（docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11）。

## 1 语义要求（semantic anchors，公式锚见 ALG-STARDET-001 §2/§11.1）

- subpixel centroid: 亚像素质心为连续估计（一阶导零交叉 / 二阶导零交叉 /
  椭圆高斯 GSL-LM 中心），不引入 0.5px 网格量化损失；合成场验收容差
  |Δc|≤0.3 px（SNR≥20）见 ALG-STARDET-001 §11.4 F1；SNR 定义（SNR_peak）
  见 `docs/algorithms/GATES_AND_TOLERANCES.md`（G-P1-CENTROID-SCI 行）。
- completeness / false positive (synthetic fields): 完备性与虚警由合成星场
  验收。**召回域必须按 PSF 宽度分档写成
  `SNR_peak ≥ κ·(1 + σ_smooth²/σ_psf²)`**，其中 `σ_smooth = 2.0 px` 是检测所用
  高斯平滑核宽度（`sdet_api.cpp:1772-1774` 常量实参），`σ_psf` 为星点高斯宽度、
  `SNR_peak = A_fit/σ_bg`（定义见 `GATES_AND_TOLERANCES.md` §2）。
  本仓实测（生产 `sdet_detect_ex_f64`，峰值对齐像素中心，24 次/档，
  单星场）：`κ = 6.24 ± 0.60`（由 σ_psf ∈ {1.0, 1.27, 1.5, 2.0, 2.5, 3.0} px
  六档 50% 过渡点反解）；逐档 99% 召回阈 `SNR_peak` = 31.3 / 21.2 / 17.6 /
  14.6 / 9.0 / 8.6。**据此，`SNR_peak ≥ 10` 单独不构成召回域**：σ_psf = 1.0 px
  （FWHM 2.35 px）时 `SNR_peak = 10` 的召回为 **0%**。
  证据：`run/SCI-FIX-STARPSF-01/results/e2b_summary.txt`、`e2_recall_summary.txt`。
  **适用域**：该式对 `σ_psf ≥ 0.8 px`（FWHM ≥ 1.9 px）成立；`σ_psf < 0.8 px` 时
  离散采样使过渡区显著展宽（实测 50% 点 `SNR_peak` = 90，式给 36.3），该式
  **不适用**，须逐档实测。虚警以纯噪声场（无注入星）计数，单位 1/千像素；
  实测 0.000/千像素（8 帧 256×256，`results/e2_noise_summary.txt`）——该 0 值
  是判据下界，判据的鉴别力由负例注入（§4 状态码负例 + 本页 §3 F2）承担。
  检测是经验性图像处理流程，不宣称解析保证。
- 全局检测阈值（`detection.threshold_sigma`）：
  `threshold = median(img) + 5.0·bgnoise`，**单位 ADU**；`bgnoise` 为
  **未平滑原图**的行差分背景噪声 RMS，单位 ADU（`sdet_api.cpp:1783`，
  估计器见 ALG-STARDET-001 §2）。**量纲声明（必须随阈值同读）**：该阈作用在
  `σ_smooth = 2.0` 的平滑图上（`sdet_api.cpp:1856-1857` 判 `smooth > threshold`），
  故它在**平滑图噪声单位**下的取值是 `5.0/‖k‖₂ = 5.0·2σ_smooth·√π = 35.45 σ_smooth`
  （连续 2D 高斯核 `‖k‖₂ = 1/(2σ_smooth√π) = 0.14105`）。
  **`threshold_sigma` 是「未平滑原图噪声」的倍数，不是阈值实际作用图像上的显著性**；
  两者相差 `1/‖k‖₂ = 7.09×`。凡引用「5σ 语义」的判据**必须**声明所用 σ 属于哪幅图。
  **适用域**：`bgnoise` 的行差分估计以「相邻像素噪声独立」为前提（见 §6 与
  ALG-STARDET-001 §2）；重采样/相关噪声输入下该前提不成立。
- saturation / blend / edge: 饱和判定=3×3 邻域双条件
  （meanhigh−bg ≥ 0.7·dynrange 且 pixel0−minhigh ≤ 0.1·dynrange）；饱和平台
  中心=edge-walking 几何中心；饱和与正常星重叠（d²<4.0）丢正常星保饱和星；距边界 <2px 允许丢弃。
  **`dynrange` 的定义域与量纲（ADU）**：`dynrange = min(max(img), 65535) − median(img)`
  （`sdet_api.cpp:1834-1837`，`norm = 65535.0f` 为字面量）。**适用域 = 以 uint16
  整数 ADU 读出、满阱=65535 ADU 的帧**；对已定标的 float 帧（本项目 Phase1 生产
  输入），该式给出的是「帧自身 max 与 65535 的较小者」，**不等于探测器饱和电平**。
  实测（M42_M1_T2 Red 20251212@012404 校准帧，4096²，float32，数据域
  [−16975.6, 80789.7] ADU）：`norm=65535`、`dynrange=65339.0 ADU`、
  被标 `saturated` 的检出星 94/2474，其中 53 颗的 3×3 峰值 ≤ 65535 ADU。
  真饱和平台电平独立测量（7×7 窗内 ≥6 像元落在峰值 0.5% 内者，n=36）：
  `min/median/max = 62150 / 64987 / 80790 ADU`，与 65535 之比 0.948–1.233，
  **44.4% 高于 65535** ⇒ 校准后平台电平是空间变化量，**不是常数 65535**。
  证据：`run/SCI-FIX-STARPSF-01/results/e3_real_summary.txt`、`e3c_plateau.txt`。
  ⇒ `saturated` 列在 float 定标帧上**必须**按「`A_fit > min(max(img),65535) − median(img)`」
  这一 Project-defined 判据消费，**不得**读作探测器饱和真值。
- deterministic ordering: 输出按 mag 升序全序确定（NaN 恒排末尾），
  dedup/sort/maxStars 截断串行，输出与线程数 bitwise 无关
  （ALG-STARDET-001 §5）。

## 2 与共享 SCI 的关系

- SCI-PSF-001（PSF）：检测输出（cx/cy/FWHM/振幅/背景）为 PSF 拟合与
  plate solve 的输入；**检测侧母函数 = 椭圆高斯**（本页 §1/§3，
  ALG-STARDET-001 §2），**PSF 侧 = 椭圆 Moffat4**（SCI-PSF-001 §5）；
  两模型**宽度列不可跨块比较**：同 sx 下 `FWHM_gauss/FWHM_moffat =
  2.354820/1.230310 = 1.9140×`（DISP-STAR-007）。
- SCI-PHOT-001（PHOTOMETRY）：正常星 mag=−2.5·log10(Σ_box(pixel−B_fit))
  为粗测光（检测侧自估），最终测光归 PHOTOMETRY 域；饱和星 mag 量纲差异
  已登记 DISP-STAR-004（ALG-STARDET-001 §11.3）。
- SCI-AST-001（ASTROMETRY）：像素中心=索引+0.5 约定
  （ALG-STARDET-001 §2 残差坐标），star_det 权威块消费方（plate solve
  fallback）按此约定解析坐标（DATA_SEMANTICS §17）。

## 3 基线选择与验收语义

- 基线算法=peaker 七步候选 + 椭圆高斯 GSL trust-region LM 拟合
  （7 参数，`fwhm=2.3548·sx`；高斯拟合 Moffat4 真星质心无偏 median 0.0047 px，
  但 FWHM 报值/真值=1.086、解析流量比=0.902；生产路径
  `sdet_detect_impl`，`sdet_api.cpp:1599-2353`）。
- 现状缺陷不隐瞒（DISP-STAR-001..005 显式登记，ALG-STARDET-001 §11.3）；
  本层不声称缺陷已修复。
- 本页与 ALG-STARDET-001/DATA-P1-STAR/API-STAR-001 组成 Phase1 星点检测冻结层；
  禁止编排层词汇（descriptor astrocs.phase1.star-psf）反向改写本层。

## 4 测试设计锚

TEST-STAR-DESIGN-001（ALG-STARDET-001 §11.4）为冻结测试设计：
F1 合成场统计（质心/FWHM/召回/虚警）、F2 饱和/混合/边缘专项、F3 确定性
（线程数 bitwise 一致+全序断言）、F4 FP64 独立 oracle 与 FP32 量化容差
（FP32 项只覆盖 FP32→uint16 量化通道，非端到端位置门；端到端绝对位置门=
G-P1-CENTROID-1）、F5 状态码负例、F6 回归锚。可执行 TEST-P1-STAR-001 按本设计
落地，容差冻结不得放宽。

## 5 物理量和单位（units）

- 像素坐标: 质心 c=(cx,cy) 单位 px（像素，0-based 索引+0.5 中心约定，
  见 §2 SCI-AST-001）；容差 |Δc|≤0.3 px 同量纲比较。
- FWHM/孔径尺寸: 单位 px；σ 派生量 σ=FWHM/(2√(2ln2)) 同为 px。
- 亮度/流量: 原始读出量单位 ADU（模拟数字单元）；粗测光
  mag=−2.5·log10(Σ_box(pixel−B_fit)) 中 pixel 与 B_fit 均为 ADU，
  mag 无量纲（星等）；dynrange 单位 ADU（定义域见 §1 saturation 条）。
- 背景噪声: `bgnoise` 单位 **ADU**（未平滑原图行差分 RMS，`sdet_api.cpp:1783`）；
  检测阈值 `threshold` 单位 **ADU**；`threshold_sigma` 无量纲，
  其 σ 基准 = 未平滑原图噪声（不是平滑图噪声，§1）。
- **两套 `sigma_bg` 不可互换**：盲检测阈值用 `bgnoise`（行差分族）；
  `p1_sources.json` 的 `noise_sigma`（= `SNR_det` 的分母）由 wrapper 侧
  `StarDetector::estimate_background` 的 2 轮 median±3σ 裁剪后 RMS 给出
  （`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:30-70`；
  审计 `run/CLEAN-401/third_sigma/README.md`）。同帧实测：13.8148 vs 20.7384 ADU，
  比值 1.5012（`run/SCI-FIX-STARPSF-01/results/e4_sigma_summary.txt`）。
  凡写「σ_bg」的判据**必须**点名用哪一套。
- 角度量: PA/位置角单位 deg；虚警密度单位 1/千像素（0.1/千像素，§1）；
  SNR 无量纲（σ 为背景噪声 ADU RMS，须点名估计器，见上条）。
- 时间量: 无本域时间物理量（检测为单帧快照流程，无曝光时间归一化项）。

单位约定与 docs/science/PHOTOMETRY.md（ADU/mag）、ASTROMETRY.md（px/deg）
一致；本节为 checker（check_science_units.py）声明本页物理量与单位完备性，
不改任何既有语义。

> 本域门与容差的量测域/统计量/SNR 定义/阈值来源见 `docs/algorithms/GATES_AND_TOLERANCES.md`（F-2 冻结门表；门不得引用表外阈值）。

## 6 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现；§1–§5 语义与 ALG-STARDET-001 公式锚不变。

- **阈值检测与去混叠**：Bertin, E. & Arnouts, S. 1996, A&AS 117, 393（SExtractor；DOI 10.1051/aas:1996164）；源码 SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）detect.c/scan.c。**差异**：AstroCS 用 median+5σ 全局阈 + peaker 局部极大 + 去重，非 SExtractor 的阈值网格/去混叠，引用仅作方法学对照。
- **质心估计（一阶矩/导数零交叉）**：Stetson, P. B. 1987, PASP 99, 191（DAOPHOT；DOI 10.1086/131977）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）centroid_sources 的 1D Gaussian / quadratic / com 估计器。
- **椭圆高斯 LM 拟合**：Levenberg 1944, Quart. Appl. Math. 2, 164；Marquardt 1963, SIAM J. Appl. Math. 11, 431；Moré 1978, Lecture Notes in Math. 630, 105；实现对照 GSL gsl_multifit_nlinear（GPL-3.0，https://www.gnu.org/software/gsl/）。
- **IIR 递归高斯平滑**：Young, I. T. & van Vliet, L. J. 1995, Signal Processing 44, 139。**核验状态**：文章级。
- **SNR_peak 与门**：docs/algorithms/GATES_AND_TOLERANCES.md §2（本域唯一 SNR 定义）与 §3 门表。
- **饱和/边缘处理**：无直接文献，Project-defined（ALG-STARDET-001 §2）；对照见 Stetson 1987 与 IRAF/DAOPHOT（IRAF/NOAO 许可，非 OSI）。
- **与 PSF 侧的模型差**：检测侧椭圆高斯 FWHM=2.3548·σ 与 PSF 侧 Moffat4 FWHM=1.230310·σ 相差 1.9140×（DISP-STAR-007），两列不可跨块比较；Moffat 出处见 Moffat 1969, A&A 3, 455 与 docs/science/PSF.md §14。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

