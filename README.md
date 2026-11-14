# ACSD｜天文观测标准化、球面拼接与FITS输出

ACSD 将CCD/CMOS原始曝光通过 **Normalize → Mosaic → Export** 转换为统一天球坐标和科学量纲的HiPS与FITS。当前`rebuild/acsd-v2`的任务是将`main`已有科学算法及其性能优化迁入更可维护的模块化架构，并把真实大数据通过有界内存流水线处理；**不是重新研发算法，也不是仅生成接口/夹具通过报告**。

## 文档入口

- **[完整科学文档](docs/science/README.md)**：恢复main实际使用的完整数学与算法分册，以及V2现有五篇基础推导；包括校准、Gaia/IPV/WCS、PSF、光度、球面Drizzle、噪声、Sky、**传统排异和加权平均**、HiPS/FITS。
- **[工程架构总入口](docs/engineering/rebuild/README.md)**：模块、数据流、技术协议和工程任务的唯一导航。
- [模块与main源码迁移](docs/engineering/rebuild/MODULES.md)、[真正内存流水线](docs/engineering/rebuild/PIPELINE.md)、[性能](docs/engineering/rebuild/PERFORMANCE.md)。
- [API](docs/engineering/rebuild/API.md)、[对象/持久化](docs/engineering/rebuild/OBJECTS.md)、[执行/资源](docs/engineering/rebuild/EXECUTION.md)。
- [工程任务](docs/engineering/rebuild/TASKS.md)、[真实代码与科学推理审查](docs/engineering/rebuild/REVIEW.md)、[Agent协作规则](AGENTS.md)。

## 实际状态与原则

`main`算法是迁移基础。V2已经有Typed模型、Host/Task/Pipe/Storage和部分Normalize、Mosaic、Export源码，自动NSIDE和order18 ROI也已有局部真实数据证据；**但已有Pipe类不代表实际科学像元已经逐块流动，Mosaic和全幅正式交付还没有完成**。必须按真实生产代码、公式、内存所有权和实际FITS判断状态，不用历史轮次的PASS/FAIL或文档自动检查代替工程成果。

独立命令之间可以使用必要正式科学产品持久化；进程内高数据量模块交接应使用真正有Lease、背压和预算的Block/Pipe，不再用层层临时文件或整幅重复数组作默认通信。保留原main有根据的科学模型与成熟数值算法，修复已证实的错误，继承Drizzle/HiPS/Sky/Export的缓存与流式性能；不开发新BLUE/QP、新乘性flat求解器或联合超分算法。

原始main、Gaia、原始观测数据只读。生产代码和当前科学规格的差异，由实际调用与科学依据推理收敛；当前分支的文档和代码必须同步更新。未执行的全幅、跨面板或全部196帧处理不得宣称已通过。
