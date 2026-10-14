# ACSD 工程正本

工程文档负责实际软件架构、数据/ABI、调度与产品 I/O，科学公式的定义在 docs/science，用户目标在 docs/ACSD_DESIGN.md。**当前 Normalize 工程唯一横向设计**为 [architecture/NORMALIZE_PIPELINE.md](architecture/NORMALIZE_PIPELINE.md)，其科学上游为 [science/normalize/NORMALIZE_SCIENCE.md](../science/normalize/NORMALIZE_SCIENCE.md)。历史文档之间的矛盾和事实归因见 [governance/NORMALIZE_CONVERGENCE.md](governance/NORMALIZE_CONVERGENCE.md)。

工程目录：architecture 包含组件责任、单阶段 typed DAG、资源与数据流；contracts/API 规定真实稳定接口；standards 规定数值、并发、快照、写盘；resources/observability 规定 CPU、I/O 与统计开销；build/发行及 CLI 入口；governance 保存文档权威与历史冲突；testing 保存独立反例和运行证据的表示方式。

开发详细探针与发布构建通过 [DEV_MODE](architecture/DEV_MODE.md) 的唯一 ACSD_DEV_MODE 编译开关隔离，数据和性能探针按 [DIAGNOSTIC_PROBES](standards/DIAGNOSTIC_PROBES.md) 实施。**纯调试快照绝不能成为同阶段科学节点的输入来源**，需要科学暂存时使用与编译开关无关的正式 cache/spill 契约。

先确认真实命令和 module_adapter 调度的生产函数，再把代码转为 typed 数据对象；不可仅凭旧 registry 的“合同已冻结”声称有完整实现。三阶段实际产品设计不因代码重构更改。
