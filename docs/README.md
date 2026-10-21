# ACSD 文档

先读[最高设计](ACSD_DESIGN.md)，理解三命令、产品与软件边界，再按问题进入科学、工程和模块说明。[索引](DOCUMENT_INDEX.yaml)提供路径和用途，不定义额外设计或验证状态。

| 阅读目标 | 入口 |
|---|---|
| 全新核心设计 | [五份科学地基](science/foundation/OBSERVATION_COORDINATES.md) → [架构规范、对象与标准 API](engineering/rebuild/README.md) |
| 观测模型、单位与适用域 | [统一模型](science/unified/UNIFIED_SCIENCE_MODEL.md)、[数据语义](science/unified/DATA_SEMANTICS.md) |
| 从原始帧到单帧球面产品 | [Normalize 科学链](science/normalize/NORMALIZE_SCIENCE.md) → [生产架构](engineering/architecture/NORMALIZE_PIPELINE.md) → [数据合同](engineering/contracts/NORMALIZE_DATA.md) → [接口规格](detail/normalize/contracts.md) |
| 多帧背景、精度与合成 | [噪声与参考精度](science/noise_snr/NOISE_SNR.md)、[背景](science/sky/UPM.md)、[Mosaic 管线](detail/mosaic/pipeline.md) |
| 球面到平面导出 | [导出模型](science/projection/HIPS_TO_FITS.md)、[重采样](science/resample/RESAMPLE.md)、[Export 管线](detail/export/pipeline.md) |
| 开发基础设施 | [系统架构](engineering/architecture/ARCHITECTURE.md)、[工程入口](engineering/README.md)、[模块索引](detail/00_INDEX.md) |
| 实际能力与证据缺口 | [实施差异](engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)、[未决问题](engineering/governance/UNRESOLVED.md) |

science 拥有估计量与推导，engineering 拥有类型和工程行为，detail 说明真实接口与调用；研究过程在[实验目录](../实验/README.md)。本轮[处置记录](../实验/documentation-refactor/REPORT.md)列覆盖范围，[待推导问题](../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)保留尚需统一论证的科学与数学事项。
