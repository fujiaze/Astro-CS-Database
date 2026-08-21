# Science Scope

## 目的

定义 AstroCS 科学处理范围与权威链入口。

## 科学定义

AstroCS 从多帧天文 CCD 图像估计统一的天球辐射场（HiPS signal）及其
不确定性（variance/ivar），并输出标准 IVOA HiPS 产品。

## 处理链

1. 单帧校准（bias/dark/flat/cosmetic）；
2. 星点检测/PSF/astrometry/photometric calibration；
3. 空背景噪声模型（NoiseWeightModelV1 → ivar）；
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

- 不保证完整 covariance matrix 产品（V19 文档化相邻像素相关）；
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

## ID

SCI-SCOPE-001（本文件范围/假设/失效域入口）；SCI-CAL-* / SCI-AST-* / SCI-PHOT-* / SCI-PSF-* / SCI-NOISE-* /
SCI-DRZ-* / SCI-UPM-* / SCI-REJ-* / SCI-INT-*。 # B5-06 同步在 TRACEABILITY.csv 增 SCI-SCOPE-001 行（doc=SCIENCE_SCOPE.md, status=VERIFIED）但本任务不改csv，仅文档。
