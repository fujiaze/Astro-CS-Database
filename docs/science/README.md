# ACSD V2 科学文档｜完整数学与现有算法

这里是**从 main 科学体系迁入 V2 的实际科学知识正本**，不是上一轮只有 2.7 KB 的概述。涵盖观测模型、单位、坐标、PSF、响应、光度、噪声、球面几何、传统Mosaic/排异/加权平均及FITS输出的公式、条件、边界和数值实现。

## 科学层级与工程责任

- **基础定义**：[观测与坐标](foundation/OBSERVATION_COORDINATES.md)、[Gaia与响应](foundation/GAIA_RESPONSE.md)、[球面Drizzle](foundation/SPHERICAL_DRIZZLE.md)、[统计/相对Sky/传统合成](foundation/STATISTICS_SKY_COADD.md)、[独立物理/数学依据](foundation/INDEPENDENT_ORACLES.md)。V2已有的详尽推导恢复在此，**不要求开发其全部研究性解算器**。
- **生产科学分册**：main已具备的功能定义，以真实生产调用的物理估计量和现存代码为依据；包含相应单位/公式/边界及科学性说明。
- **算法实现分册**：main已实现的数值计算步骤与工程优化；可作为V2直接迁移计算核的依据，但其旧文件格式/接口/历史审核轮次与冻结测试清单**不再具有工程权威**。

科学文档不是自动门禁，也不代表每种列举的算法已经被V2接入。**旧版正确算法优先移植，已证实的物理/数学错误必须修正；不借重构提出新研究方法。** 真实生产实现与分块数据流唯一见[模块迁移](../engineering/rebuild/MODULES.md)、[内存流水线](../engineering/rebuild/PIPELINE.md)、[科学/代码推理审查](../engineering/rebuild/REVIEW.md)。

## 领域科学文档

### 统一观测、适用范围与单位

- [UNIFIED_SCIENCE_MODEL](unified/UNIFIED_SCIENCE_MODEL.md)
- [DATA_SEMANTICS](unified/DATA_SEMANTICS.md)
- [SCIENCE_SCOPE](unified/SCIENCE_SCOPE.md)

### 校准与原像元质量

- [CALIBRATION](calibration/CALIBRATION.md)
- [CCD_DEFECT](calibration/CCD_DEFECT.md)
- [CALIBRATION_ALGORITHMS](algorithms/normalize/CALIBRATION_ALGORITHMS.md)
- [COSMETIC_ALGORITHMS](algorithms/normalize/COSMETIC_ALGORITHMS.md)

### Gaia、天测、检测、PSF

- [ASTROMETRY](detection/ASTROMETRY.md)
- [STAR_DETECTION](detection/STAR_DETECTION.md)
- [PSF](psf/PSF.md)
- [PLATESOLVE](algorithms/normalize/PLATESOLVE.md)
- [STAR_DETECTION_ALGORITHMS](algorithms/normalize/STAR_DETECTION_ALGORITHMS.md)
- [STAR_PSF_ALGORITHMS](algorithms/normalize/STAR_PSF_ALGORITHMS.md)

### 光度与响应

- [PHOTOMETRY](photometry/PHOTOMETRY.md)
- [PHOTOMETRIC_FIT](algorithms/normalize/PHOTOMETRIC_FIT.md)
- [GAIA_QUERY](algorithms/common/GAIA_QUERY.md)

### 噪声与Drizzle

- [NOISE_SNR](noise_snr/NOISE_SNR.md)
- [DRIZZLE](drizzle/DRIZZLE.md)
- [DRIZZLE_GEOMETRY](algorithms/export/DRIZZLE_GEOMETRY.md)
- [HEALPIX_MAPPING](algorithms/export/HEALPIX_MAPPING.md)

### Mosaic、真实天光、传统排异与加权叠加

- [UPM](sky/UPM.md)
- [REJECTION](integration/REJECTION.md)
- [INTEGRATION](integration/INTEGRATION.md)
- [PHASE2_COVERAGE](algorithms/mosaic/PHASE2_COVERAGE.md)
- [PHASE2_SAMPLER](algorithms/mosaic/PHASE2_SAMPLER.md)
- [PHASE2_REJECTION](algorithms/mosaic/PHASE2_REJECTION.md)
- [PHASE2_INTEGRATION](algorithms/mosaic/PHASE2_INTEGRATION.md)

### HiPS、重采样与FITS

- [HIPS_TILE_FORMAT](formats/HIPS_TILE_FORMAT.md)
- [HIPS_TO_FITS](projection/HIPS_TO_FITS.md)
- [RESAMPLE](resample/RESAMPLE.md)
- [PHASE3_PROJ_IMPL](algorithms/export/PHASE3_PROJ_IMPL.md)
- [PHASE3_RESAMPLE](algorithms/export/PHASE3_RESAMPLE.md)
- [PHASE3_RSMP_IMPL](algorithms/export/PHASE3_RSMP_IMPL.md)
- [PHASE3_FITS_IMPL](algorithms/export/PHASE3_FITS_IMPL.md)

## 当前实施约束

Mosaic依 main 的 Coverage→Sampler→公共Sky→传统Rejection→非负权积分；实现 `Σwx/Σw` 与合法support/invalid状态。BLUE、QP、SACoadd、联合乘性flat及新迭代恢复不是本轮必需，也不应成为阻断。自动 NSIDE 必须按真实WCS/SIP且全帧一致；所谓高阶必须保留原始图像真实频率，Export不能把粗叶放大叫重建。内存管道负责块传递而非对科学公式另造副本。

**文档迁移说明**：以下分册保留科学公式与main算法步骤；若正文还出现旧路径、被淘汰的历史报告名或测试冻结标识，它们不是新V2入口，实施必须逐源码替换为本目录链接/正式接口。该文档迁移不会自动修复生产代码或凭历史数字宣称验收。
