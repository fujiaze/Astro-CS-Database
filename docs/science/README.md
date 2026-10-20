# 科学模型与方法

**#111 科学地基重建候选**：重新开发请从 [观测与坐标](foundation/OBSERVATION_COORDINATES.md) 开始，依次阅读 [Gaia/响应](foundation/GAIA_RESPONSE.md)、[真实球面 Drizzle](foundation/SPHERICAL_DRIZZLE.md)、[统计/sky/coadd](foundation/STATISTICS_SKY_COADD.md) 和 [独立 Oracle 与证据](foundation/INDEPENDENT_ORACLES.md)。这五篇候选规范等待外部独立审核，不表示旧生产实现已通过；下方旧分册保留参考身份，不以其历史接口约束新核心。

科学阅读从[统一观测模型](unified/UNIFIED_SCIENCE_MODEL.md)、[量与单位](unified/DATA_SEMANTICS.md)及[适用域](unified/SCIENCE_SCOPE.md)开始。专题分册拥有各自估计量和推导，算法补充资料解释数值方法，真实实现与证据由[实施差异](../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)对照。

| 科学链 | 阅读顺序 |
|---|---|
| 单帧标准化 | [Normalize](normalize/NORMALIZE_SCIENCE.md) → [校准](calibration/CALIBRATION.md)与[缺陷](calibration/CCD_DEFECT.md) → [天测](detection/ASTROMETRY.md) → [科学源](detection/STAR_DETECTION.md)和[PSF](psf/PSF.md) → [测光](photometry/PHOTOMETRY.md) → [条件噪声](noise_snr/NOISE_SNR.md) → [Drizzle](drizzle/DRIZZLE.md) |
| 球面马赛克 | [精度与重建](noise_snr/NOISE_SNR.md) → [背景差模](sky/UPM.md) → [排异](integration/REJECTION.md) → [集成](integration/INTEGRATION.md) |
| 平面导出 | [HiPS 格式](formats/HIPS_TILE_FORMAT.md) → [目标 WCS](projection/HIPS_TO_FITS.md) → [数值采样](resample/RESAMPLE.md) |

IPV 在求解中盲检的几何候选与 WCS 后 Gaia 引导的科学源有不同职责；参考通量精度、场景面亮度方差和背景控制方差分别定义。阶段一保留必要模型，阶段二最终只发布 signal HiPS，阶段三输出普通信号 FITS。

[算法资料](algorithms/README.md)中仍有实现说明与历史诊断，阅读时按上层正本判断适用范围；本轮没有将未逐篇核对的资料宣称为已重写。[待推导问题](../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)集中保存科学与数学疑点，历史实验保持证据身份。
