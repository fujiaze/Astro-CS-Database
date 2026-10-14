# ACSD 文档总入口

ACSD 是 CCD/CMOS 天文图像标准化与球面成像系统：Normalize 逐帧产生已校准 HiPS/必要科学模型，Mosaic 将用户自行选择的不同设备/通带数据合成最终**仅一个 signal 科学图像通道**的 HiPS，Export 重投影为普通二维 WCS FITS。

**先读 [ACSD 最高设计](ACSD_DESIGN.md)**，再按实际责任选择。当前优先收敛 Normalize：

- **[阶段一完整科学链](science/normalize/NORMALIZE_SCIENCE.md)**：校准→IPV 自解天测→Gaia guided science sources/PSF→XP 测光→canonical reference SNR→Drizzle。
- **[阶段一工程与代码](engineering/architecture/NORMALIZE_PIPELINE.md)**：真实调度函数、Typed 端口、producer/consumer 与代码模块归属。
- **[阶段一文档冲突裁决](engineering/governance/NORMALIZE_CONVERGENCE.md)**：哪些旧表述不再有效、哪些代码已经有、哪些科学实验其实没运行。
- [全项目统一科学模型](science/unified/UNIFIED_SCIENCE_MODEL.md)、[数据语义](science/unified/DATA_SEMANTICS.md) 和 [科学阅读入口](science/README.md)。
- [开发与发布诊断](engineering/architecture/DEV_MODE.md)、[探针规范](engineering/standards/DIAGNOSTIC_PROBES.md)。

**不可混淆**：原始 FITS 缺 WCS 不是 IPV 无法求解；当前生产已有 IPV。仅扫描原始 FITS 头不能得出阶段一解析失败结论。同样，HiPS frame_snr 与自身 JSON 一致不意味着 canonical sparse SNR 已经产生。

旧 docs/detail 和 docs/science/algorithms 的实现级描述、实验历史继续可查，但与以上正本冲突时不能重建另一套 Normalize 科学流程。项目文档机器索引在 [DOCUMENT_INDEX.yaml](DOCUMENT_INDEX.yaml)。用户数据的大型 FITS 不上传 Git；开发不设置形式主义流程门禁，科学可信性由实际估计量与独立证据而不是重复 PASS 标志决定。

阶段一完整实施依据还包括 [数据合同与产品 schema](engineering/contracts/NORMALIZE_DATA.md) 和 [C++ 接口及迁移规格](detail/normalize/contracts.md)。设计接口已确定，执行者按它们实施；代码和独立科学验证仍须分别报告。
