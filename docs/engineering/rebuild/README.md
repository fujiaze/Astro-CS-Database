# ACSD v2：唯一设计入口

状态：科学设计复核记录见[审核状态](../../audit/final-review.md#科学审核状态)；远端已开始实现，本次仅补工程文档，不把已有实现等同生产验收。本次资源/探针/GPU目标及复核见[工程补全](../../audit/engineering-completeness.md)，尚未接入的接口/配置以API迁移清单明确。分支 `rebuild/acsd-v2` 从 `c5fee09fedb97ccbe39cb47ea0985e0d8a339db2` 建立，不兼容旧接口、对象、产品或构建系统。

ACSD 在 Windows/Linux amd64 CPU 上提供独立的 Normalize、Mosaic、Export。统一天空为 ICRS、HEALPix NESTED、order≤29；Normalize 输出逐帧面亮度和可恢复模型，Mosaic 输出一个 signal HiPS，Export 输出一个有 WCS 的二维 signal FITS。不同通带可按显式混合目标组合；未知噪声不阻止合法信号，也不成为零方差。Windows GUI 独立构建，仅消费 CLI 配置、事件和产品。

首期不实现GPU/ARM后端，但已冻结CPU/GPU批量执行、内存位置、显存预算、事件寿命及回退边界，见EXECUTION；不实现其他投影的严格输入、跨阶 Mosaic、一般非线性去马赛克、任意低计数似然、SACoadd、旧 Runtime/ARS/ABI/插件。低计数数据可保留信号及条件误差；Gaussian 光度拟合不适用时返回 unavailable，不能自动换未定义似然。输入缺 WCS 时必须实际盲解。本分支已含部分实现；新增工程目标仍需按声明→schema→实现→验收迁移，不能因文档描述就声称功能已经存在。

## 规范所有权与阅读顺序

| 次序 | 正本 | 唯一责任 |
|---|---|---|
| 1 | [观测与坐标](../../science/foundation/OBSERVATION_COORDINATES.md)、[Gaia 响应](../../science/foundation/GAIA_RESPONSE.md)、[曲边 Drizzle](../../science/foundation/SPHERICAL_DRIZZLE.md)、[统计与合成](../../science/foundation/STATISTICS_SKY_COADD.md)、[独立科学证据](../../science/foundation/INDEPENDENT_ORACLES.md) | 数学定义、适用域、必要推导、物理假设与历史证据 |
| 2 | [SYSTEM](SYSTEM.md) | 算法步骤、源码目录、无环依赖、三命令数据流、依赖和构建 |
| 3 | [OBJECTS](OBJECTS.md) | 对象不变量、索引、单位、模型图、持久化和恢复；字段及类型以[声明合同](../../../include/acsd/contracts.hpp)为唯一正本 |
| 4 | [API](API.md) | 每项操作的前后条件、返回/错误、协议、CLI/配置和 GUI；函数签名只由声明合同拥有 |
| 5 | [EXECUTION](EXECUTION.md) | 精度、误差分账、细化、任务、内存、I/O、线程和停止 |
| 6 | [VERIFICATION](VERIFICATION.md) | 独立真值、必要验收、性能方法与审核门槛 |

字段表是语义说明，不另起一套类型。五份科学基础不再另行声明struct或公共签名，数学对象语义由声明合同统一为实际工程字段。工程正本不能修改科学公式。若发现科学矛盾，记录反例、修订科学正本并同步合同。

每种目标抽象均有具体消费者：Storage的流式builder/规范封存服务大数组；Resources子域服务共享预算与独立归因；Diagnostics以独立配额观察现有结果；KernelExecutor给规则批次共用CPU及未来设备后端；新增角色均列入API待实施迁移清单；LinearAction 给 R/G/sky/Q 复用及转置；CovarianceAction 给共享根的一次传播；provider 为同一算法接内存和磁盘；GeometryRevision 给精度反馈原子替换依赖闭包。这些不构成运行节点系统或动态插件。

## 开发必需合同的覆盖

| 主题 | 实施正本与验收 |
|---|---|
| 标准API、CLI/GUI/独立driver、错误与状态 | API；现有contracts.hpp及schema；新增要求见迁移清单；VERIFICATION C01—X01 |
| 大数组构建、所有权、规范块/hash、发布/恢复 | OBJECTS+API；EXECUTION资源转交；VERIFICATION B01/H01/P01 |
| 并发归因、背压、取消、cache、内存/磁盘预算 | EXECUTION；VERIFICATION R01/A01 |
| 内部构建、性能probe、中间快照、调试选择器 | SYSTEM构建、API协议、OBJECTS载荷、EXECUTION隔离；VERIFICATION D01 |
| CPU优化、未来GPU驻留/传输/事件与回退 | EXECUTION；VERIFICATION K01及性能章；首期CPU，GPU未实跑 |
| 版本/依赖/平台/发布与编码约束 | SYSTEM；VERIFICATION交付检查 |

这些是当前有效合同；历史审核/修订叙述不增加另一套字段、接口或执行选择。旧v1提供需求覆盖对照，不作为新实现依赖。本轮补齐范围和实际验证见[工程完整性记录](../../audit/engineering-completeness.md)。

开发入口见[根 README](../../../README.md)，规则见[AGENTS](../../../AGENTS.md)。[清理说明](../../audit/CLEANUP.md)和[逐文件清单](../../audit/removed-files.csv)记录旧树删除；[独立审核](../../audit/REVIEW.md)记录发现和处理。旧系统仅在 Git 历史及固定提交外部证据中存在，不能作为实现或新测试依赖。

按VERIFICATION与迁移清单继续现有重构；本轮只改文档，不修改科学实现、声明、schema或构建。设计可审查、声明可编译与算法可用是三个不同状态。
