# ACSD V2｜唯一设计、源码迁移与执行入口

**工程目标**：将 `main` 已有正式科学处理功能迁入一个更可靠的 V2——22 个独立科学 Process、typed数据、唯一Host/Task/Pipe/Storage、严格数值/共享噪声、流式高性能和可重开产品。**不是从零另写功能缩水的图像算法。** 允许破坏旧 ABI/内部JSON/历史产品格式兼容性，**必须继承可证明正确的旧算法内核、旧性能优化及用户可达功能**；旧模型被证伪时用V2科学正本修正，不能机械逐位复刻错误。

> **任务与验收状态**：本次源码审查/文档迁移方案并非生产代码PASS。main对照基线 `c5fee09fedb97ccbe39cb47ea0985e0d8a339db2`，初始V2基线 `1e2f8f8d29ec88c2703a49adea90ecfab801eb14`；实际实施总是先读取分支最新HEAD。旧M42 S1三帧order13的PASS只在特定低阶内部数值/发布域成立，不能用于高分辨率保真或不同面板接缝的成品认证。历史审查事实保留，不因文档编辑改变状态。

## 阅读顺序与唯一维护责任

| 最先阅读 | 唯一负责的内容 |
|---|---|
| **[MIGRATION](MIGRATION.md)** | main实际源码资产、每模块迁移施工包、性能与测试任务、执行顺序；**首先阅读** |
| [五篇科学 Foundation](../../science/foundation/OBSERVATION_COORDINATES.md) | 估计量、真球面几何、响应、随机量/误差、独立真值，不因旧源码变更数学 |
| [SYSTEM](SYSTEM.md) | 一套Runtime骨架、注册、生命周期、Host/Pool/Task/Pipe/Storage服务和构建 |
| [API](API.md)、[OBJECTS](OBJECTS.md) | 唯一typed协议、单位、索引、身份、产品对象和不确定度能力 |
| [EXECUTION](EXECUTION.md) | 所有Process共用的资源/线程/内存/IO/数值与取消规则 |
| [processes/README](processes/README.md) | 22个Process/57 operation的算法步骤、数据前后条件、失败和科学反例 |
| [WORKFLOWS](standards/WORKFLOWS.md) | 三命令真实数据边、**全帧自动分辨率预规划**、sky全局屏障及重算 |
| [COMPUTE_MODULES](standards/COMPUTE_MODULES.md) | 唯一公共数值模块与可组合块函数；不在各Process重造缓存/线程池 |
| [VERIFICATION](VERIFICATION.md)及[迁移实测矩阵](migration/07-VERIFICATION.md) | 科学证明与真实生产验收；旧测试/旧报告不能自行认证新V2 |
| [迁移总执行任务](migration/08-EXECUTION.md) | 逐模块源码任务、性能、Agent工作组织、阶段切换 |

`include/acsd/{model.hpp,runtime/*.hpp,modules/*.hpp}` 是唯一可编译类型/签名正本，`contracts.hpp` 仅聚合。`FOUNDATION_API` 是行为规范，不另声明同名可编译API；共享算法/数学接口只维护一份。未来GPU可沿同一Host/KernelExecutor扩展，当前只验证Windows/Linux amd64 CPU。

## 具体迁移与资料归档

- **旧源码复用**：main `lib/algorithms/`中的已生产可达算法优先抽取改接V2；`module_adapters.cpp`的巨大旧胶水和重复Runtime不复制。旧autoNSIDE/本机Gaia/IPV/PSF/photometry、touched-tile Drizzle、Sky Schur、Rejection多核、SharedTileCache和Export流式机制不得无证缩减。详见[资产与文档处置](migration/00-BASELINE-ASSETS.md)。
- **算法科学更正**：旧曲边近似、Fisher→scene、暗/平场乘性差、响应Q缺失、小N排异与Export可用面积核等按Foundation/Process独立证明；不拿main历史程序输出当绝对科学真值。
- **数据/工程重构**：旧内部工作文件依赖→V2 Ref/Block/Pipe和Storage，支持按hash缓存重用、原子发布、跨进程重开、同一预算与确定性归约。V2阶段报告不因exit0、FITS能打开、22个API登记和测试脚本绿灯而替用户宣布科学产品合格。
- **文档去重**：旧主分支 `docs/ACSD_DESIGN.md`、`docs/science/algorithms`、`docs/detail` 和 `实验` 是有版本的来源与实验；只把真实结论/独立反例引用到本分支。Foundation唯一数学正本，Process唯一科学步骤，Migration仅拥有**旧代码迁移/性能/测试工作单**，不写平行新标准。

## 当前实施边界

优先完成 [#126](https://github.com/fujiaze/Astro-CS-Database/issues/126) 的原功能迁入、真正autoNSIDE、高分辨率星点与至少两个不同面板Mosaic；独立验证通过且性能不过度退化后再执行 [#125](https://github.com/fujiaze/Astro-CS-Database/issues/125) 的全部196帧 Hα50/R49/G49/B48、T2/T3、M1–M6共同WCS四通道产品。详细验收门见[07](migration/07-VERIFICATION.md)，运行次序与授权见[08](migration/08-EXECUTION.md)。

历史实施状态以[IMPLEMENTATION-STATUS](../../audit/IMPLEMENTATION-STATUS.md)及该报告记录的受检SHA/日期为边界；它可能早于最新M42实验，不能作为当前生产全量PASS。文档流程修订本身不改变代码状态。协作以[AGENTS](../../../AGENTS.md)为准；不创建CI/自动文档验收/覆盖率门禁以代替数学与独立反例。
