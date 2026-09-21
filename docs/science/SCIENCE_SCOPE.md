# Science Scope

> 上游：ASTROCS_DESIGN.md §1（项目定位）、§2（核心科学方法）

## 目的

定义 AstroCS 科学处理范围与权威链入口。

## 科学定义

AstroCS 从多帧天文 CCD 图像估计统一的天球辐射场（HiPS signal）及其
不确定性（variance/ivar），并输出标准 IVOA HiPS 产品。

## 处理链

1. 单帧校准（bias/dark/flat/cosmetic）；
2. 星点检测/PSF/astrometry/photometric calibration；
3. 空背景噪声模型（噪声模型 A = `NoiseWeightModelV1`，唯一生产模型 → ivar）；
4. 球面 Drizzle（线性通量守恒重建 + 方差传播）；
5. Phase2：coverage union → 控制采样 → UPM 联合加性校准 → 排异 →
   ivar 加权积分 → HiPS。

## 变量/单位

- 信号：ADU（校准前）/ e⁻ 或归一化 ADU（校准后）；
- 位置：RA/Dec 度（J2000）、HEALPix NESTED、tile+local xy；
- 光度：dex log10 比值、mag；variance：信号单位²。

## 假设

- 每帧为同一 target 的多次曝光（dither/不同滤镜需正确分组）；
- 背景为局部平稳随机场（patch 尺度）；源星点稀疏可掩膜。

## 有效域

见各科学文档；总体：深空成像，16-bit/32-bit FITS，标准 CCD/CMOS。

## 不保证

- 不保证完整 covariance matrix 产品（相邻像素相关已文档化）；
- 不保证光谱/运动学产品（非本管线范围）。

## 失效条件

- 无合格控制点/无重叠 → NO_DATA / UNDERDETERMINED 显式状态；
- 输入损坏 → INPUT_CORRUPT 显式错误（禁止猜测）。

## 系统/随机误差

系统性：flat 残差、PSF 色差、测光零点漂移（QA 元数据化）；
随机性：光子泊松 + 读出噪声（NoiseWeightModelV1）。

## 数值精度

默认 FP64 科学计算；FP32 仅显式等价路径（SparseEqualsDense 1e-12 门）。

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
