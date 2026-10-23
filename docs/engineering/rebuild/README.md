# v2设计入口：固定基建与独立process

当前处于内部重构开发阶段，采用一套当前标准，允许破坏式修改；不要求v1或早期v2接口/产品/配置兼容。v1只作为“文档完整度”参照，不移植其Runtime或实现层次。Git历史和科学原始证据继续保留。

本轮只修改文档。远端已有部分科学实现，但标准ProcessHost/registry与下述新增服务尚未按新骨架接线；文档完成不表示运行验收通过。GPU只设计扩展，当前CPU，不实现GPU后端。

## 阅读顺序与唯一所有权

| 文件 | 唯一维护内容 |
|---|---|
| [SYSTEM](SYSTEM.md) | 一份固定基建骨架：注册/typed边界/生命周期/调度/服务/构建装配 |
| [processes目录](processes/README.md) | 每个科学process的独立输入输出、算法、停止、资源模型、并行/探针与独立反例 |
| [API](API.md) | 所有process共用的调用/错误/配置/事件与诊断协议；签名只由公开声明拥有 |
| [OBJECTS](OBJECTS.md) | 公共类型语义、单位/index、不可变所有权、当前格式封存/恢复；无旧产品兼容 |
| [EXECUTION](EXECUTION.md) | 共用内存/线程/I/O/数值格式/误差和GPU扩展标准 |
| [VERIFICATION](VERIFICATION.md) | 共用验收和证据标准、基建/模块独立验证矩阵 |
| [FOUNDATION_API](standards/FOUNDATION_API.md)、[SCHEDULER](standards/SCHEDULER.md) | 定稿字段/函数/所有权、重规划/region/清理与首期调度实施算法 |
| [DEVELOPMENT](standards/DEVELOPMENT.md)、[完整校准实施例](standards/CALIBRATION_WALKTHROUGH.md) | 可分配的M0—M6任务、22模块资源工作单元表、完整typed接入和固定验收输入 |
| [COMPUTE_MODULES](standards/COMPUTE_MODULES.md) | 可复用计算积木、typed函数/Task组合、最小依赖包和接入标准 |
| [WORKFLOWS](standards/WORKFLOWS.md) | Normalize/Mosaic/Export的固定数据边、屏障与反馈组合 |
| [NUMERICS](standards/NUMERICS.md)、[PROBES](standards/PROBES.md) | 唯一共享数值原语合同；初始探针目录与生成规则 |

保留六份根工程文件供现有工具识别；SYSTEM是唯一骨架，算法详细步骤已拆到process，不再放在SYSTEM集中维护。子目录标准属于上述相应正本，不新增一套框架或公共签名。公开字段/签名仍唯一由[contracts.hpp](../../../include/acsd/contracts.hpp)拥有；本轮新基建目标接口已在FOUNDATION_API定稿，后续统一落到源码；不把未实施接口伪称现有能力。

## 科学基础

[观测与坐标](../../science/foundation/OBSERVATION_COORDINATES.md)、[Gaia响应](../../science/foundation/GAIA_RESPONSE.md)、[曲边Drizzle](../../science/foundation/SPHERICAL_DRIZZLE.md)、[统计与合成](../../science/foundation/STATISTICS_SKY_COADD.md)、[独立真值](../../science/foundation/INDEPENDENT_ORACLES.md)拥有数学定义、适用域与推导。process拥有算法实施细节，不能改变这些科学假设；科学矛盾先给反例并修正科学依据。本轮未修改五份基础文档。

ACSD首期Windows/Linux amd64 CPU，科学天空ICRS/HEALPix NESTED、order≤29。Normalize输出逐帧面亮度及模型，Mosaic输出唯一signal HiPS，Export输出WCS二维signal FITS。unknown噪声不填零；跨通带组合需要具名科学目标。GUI只消费配置/事件/产品协议。

## 如何逐模块维护

按[模板](processes/TEMPLATE.md)修改或新增process文档，在其模块登记一个typed descriptor并实现CPU路径，补独立反例；构建自动汇总registry，Host无需加算法switch。算法改变通常只改本模块及明确受影响消费者；公共资源或数据合同改变才联动共用标准。不是每个scalar helper都需要process节点。

实施次序：VirtualComputePool/Task/Pipe及可复用模块强类型边界→Host/registry→公共资源/Storage/CPU执行与默认观测→逐模块CPU接入→固定工作流/CLI/driver统一装配→实际验收。未来GPU沿既定KernelExecutor/ArrayBacking/Completion合同扩展，不要求当前实现CUDA/ROCm、保留旧API或双版本产品reader。

本次更正见[process框架记录](../../audit/process-framework.md)，可直接开发的补全内容与实际检查边界见[实施规格复核](../../audit/developer-specification.md)；先前审核报告保留历史范围，不能用来认证本次新设计。[根README](../../../README.md)提供现有检查入口；[AGENTS](../../../AGENTS.md)规定协作与提交边界。
