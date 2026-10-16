# 工程设计

工程设计与 science 同级，说明科学模型如何通过组件、类型、接口和资源实现。先读[系统架构](architecture/ARCHITECTURE.md)，再按下表进入负责主题的合同；实际模块接口见[细节索引](../detail/00_INDEX.md)。

| 主题 | 入口 |
|---|---|
| 阶段、数据流与生产模块 | [数据流](architecture/DATA_FLOW.md)、[Normalize 架构](architecture/NORMALIZE_PIPELINE.md)、[模块地图](architecture/MODULE_MAP.md) |
| 对象与生命周期 | [统一对象](UNIFIED_OBJECTS.md)、[Normalize 数据](contracts/NORMALIZE_DATA.md)、[Runtime](contracts/RUNTIME.md)、[命名块](contracts/PIPELINE_BLOCK.md)、[所有权](contracts/OWNERSHIP_LIFETIME.md) |
| 调度与资源 | [ARS 架构](architecture/PROCESS_RUNTIME_ARS.md)、[可编码详细设计](architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md)、[实施路线](architecture/PROCESS_RUNTIME_IMPLEMENTATION.md)、[调度](contracts/SCHEDULER.md)、[性能模型](resources/PERFORMANCE_MODEL.md)、[并发](standards/CONCURRENCY.md)、[CPU 后端](resources/cpu/BACKEND.md) |
| 配置和外部接口 | [ARS-02 冻结 ABI](api/abi/PROCESS_ABI_V2.md)、[配置](contracts/CONFIG.md)、[CLI](contracts/CLI_PROTOCOL.md)、[API](api/PUBLIC_API.md)、[C ABI](api/abi/ABI.md) |
| 产品、I/O 与来源 | [产品交换](data/PHASE_PRODUCT_EXCHANGE.md)、[HiPS](contracts/HIPS_IO.md)、[FITS](contracts/FITS_IO.md)、[发布](contracts/ATOMIC_PUBLISH.md)、[provenance](data/PROVENANCE.md) |
| 日志与诊断 | [日志错误](contracts/LOG_AND_ERROR.md)、[开发模式](architecture/DEV_MODE.md)、[探针](standards/DIAGNOSTIC_PROBES.md) |
| 构建、GUI 与交付 | [构建图](build/BUILD_GRAPH.md)、[GUI](gui/GUI_DESIGN.md)、[发行](build/RELEASE.md) |
| 测试、维护和实现限制 | [测试](testing/TEST.md)、[文档维护](governance/DOCUMENT_GOVERNANCE.md)、[实施差异](architecture/SCIENCE_IMPLEMENTATION_DELTA.md) |

合同说明目标要求；源码、实际运行和独立科学支持分别陈述。schema 管机器形状，不能改变物理定义；索引和检查提供导航与诊断。
