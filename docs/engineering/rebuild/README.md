# ACSD v2：唯一设计入口

状态：设计交付，等待负责人正式审核；无科学算法实现或生产验收声明。分支 `rebuild/acsd-v2` 从 `c5fee09fedb97ccbe39cb47ea0985e0d8a339db2` 建立，不兼容旧接口、对象、产品或构建系统。

ACSD 在 Windows/Linux amd64 CPU 上提供独立的 Normalize、Mosaic、Export。统一天空为 ICRS、HEALPix NESTED、order≤29；Normalize 输出逐帧面亮度和可恢复模型，Mosaic 输出一个 signal HiPS，Export 输出一个有 WCS 的二维 signal FITS。不同通带可按显式混合目标组合；未知噪声不阻止合法信号，也不成为零方差。Windows GUI 独立构建，仅消费 CLI 配置、事件和产品。

不实现 GPU、ARM、其他投影的严格输入、跨阶 Mosaic、一般非线性去马赛克、任意低计数似然、SACoadd、旧 Runtime/ARS/ABI/插件。低计数数据可保留信号及条件误差；Gaussian 光度拟合不适用时返回 unavailable，不能自动换未定义似然。输入缺 WCS 时必须实际盲解。科学算法是待实现目标，本分支只包含规范、声明、schema、示例及文档校验配置。

## 规范所有权与阅读顺序

| 次序 | 正本 | 唯一责任 |
|---|---|---|
| 1 | [观测与坐标](../../science/foundation/OBSERVATION_COORDINATES.md)、[Gaia 响应](../../science/foundation/GAIA_RESPONSE.md)、[曲边 Drizzle](../../science/foundation/SPHERICAL_DRIZZLE.md)、[统计与合成](../../science/foundation/STATISTICS_SKY_COADD.md)、[独立科学证据](../../science/foundation/INDEPENDENT_ORACLES.md) | 数学定义、适用域、必要推导、物理假设与历史证据 |
| 2 | [SYSTEM](SYSTEM.md) | 算法步骤、源码目录、无环依赖、三命令数据流、依赖和构建 |
| 3 | [OBJECTS](OBJECTS.md) | 对象不变量、索引、单位、模型图、持久化和恢复；字段及类型以[声明合同](../../../include/acsd/contracts.hpp)为唯一正本 |
| 4 | [API](API.md) | 每项操作的前后条件、返回/错误、协议、CLI/配置和 GUI；函数签名只由声明合同拥有 |
| 5 | [EXECUTION](EXECUTION.md) | 精度、误差分账、细化、任务、内存、I/O、线程和停止 |
| 6 | [VERIFICATION](VERIFICATION.md) | 独立真值、必要验收、性能方法与审核门槛 |

字段表是语义说明，不另起一套类型。五份科学基础中的示例 struct 是数学角色示意，由声明合同统一为实际工程字段；不得将不同分册示例逐字实现为互不相容的对象。工程正本不能修改科学公式。若发现科学矛盾，记录反例、修订科学正本并同步合同。

每种通用抽象均有具体消费者：Storage 给分块数组持租约及计费；Resources 为所有算法共享预算；LinearAction 给 R/G/sky/Q 复用及转置；CovarianceAction 给共享根的一次传播；provider 为同一算法接内存和磁盘；GeometryRevision 给精度反馈原子替换依赖闭包。这些不构成运行节点系统或动态插件。

开发入口见[根 README](../../../README.md)，规则见[AGENTS](../../../AGENTS.md)。[清理说明](../../audit/CLEANUP.md)和[逐文件清单](../../audit/removed-files.csv)记录旧树删除；[独立审核](../../audit/REVIEW.md)记录发现和处理。旧系统仅在 Git 历史及固定提交外部证据中存在，不能作为实现或新测试依赖。

审核接受后按 VERIFICATION 的实施次序重新实现；本轮不开始全面编码。设计可审查、声明可编译与算法可用是三个不同状态。
