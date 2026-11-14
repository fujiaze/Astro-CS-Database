# ACSD V2｜工程重构唯一入口

**唯一目标：把 main 已经能够执行的天文数据处理系统，重组成科学含义明确、模块可维护、资源有界、真正使用内存流水线的 V2。** 不开发新科学算法，不建立研究路线，不把旧工程替换成只有名字正确的空 Process。旧 API、旧巨大适配器及跨节点临时文件可以淘汰；已验证的计算代码、排异算法、并行与缓存优化优先保留。

## 当前有效文档（只维护这组）

1. [ARCHITECTURE.md](ARCHITECTURE.md)：代码边界、唯一所有权、从 main 移入 V2 的方式。
2. [PIPELINE.md](PIPELINE.md)：**真实内存流水线**，逐块产生/消费，关键屏障、异常/背压、跨命令持久化例外。
3. [MODULES.md](MODULES.md)：每一个生产算法的旧源码与 V2 接收者，逐项迁移和必须保持的行为。
4. [PERFORMANCE.md](PERFORMANCE.md)：原主分支具体优化及对等负载下的实际性能分析。
5. [TASKS.md](TASKS.md)：一条连续执行的工程任务，源码顺序、并发分工、现有已知阻断。
6. [REVIEW.md](REVIEW.md)：**根据文档和真实代码推理**、查看数据流、分析真实产品的审查办法；不设计自动门禁、人工造绿的夹具或覆盖率评分。
7. **[完整科学文档总索引](../../science/README.md)**：main完整科学模型、校准、天测、PSF、Gaia、响应、测光、噪声、真实球面Drizzle、Mosaic/Sky、传统Rejection/Integration、HiPS/WCS/FITS数值与算法实现，以及V2五篇完整科学基础；[生产科学简要范围](../../science/SCIENCE.md)仅是概要，**不能代替这些分册**。
8. **[API](API.md)、[OBJECTS](OBJECTS.md)、[EXECUTION](EXECUTION.md)**：当前公开协议、产品与对象身份、资源/线程/块生命周期的详尽技术合同。实际C++头文件优先级最高。

[AGENTS.md](../../../AGENTS.md) 是执行者的简短工作纪律。**可编译接口始终以 include/acsd/ 的现有真实头文件和生产调用为准**；文档不能虚构一个尚未实现的 API。需要改头文件时，先明确消费者及资源生命周期，随同实际代码修改同步维护这组文档。

## 功能范围与明确排除

保留 main 三阶段用户行为：**Normalize、Mosaic、Export**；校准与DQ、Gaia/IPV/WCS、源与 PSF/测光、自动 NSIDE、原生像元→HEALPix Drizzle、球面多面板 Coverage、真实 Sky 背景匹配、**传统逐位置异常值排除与非负权加权平均**、HiPS 与 FITS 正式输出。

BLUE、非负 QP、新的相关噪声最优化、全新乘性 flat 屏求解器、联合超分辨率、额外随机过程和“为22个Process各发明一套算法”**不属于这次重构任务**。V2 现有高级数学代码不需要为了重构全部立即删除，但不可以占据生产默认路径、阻断传统叠加或强迫新增模型；先核明有没有实际生产消费者，再决定删去/隔离的最小范围。不保留双生产实现。

科学收敛：修正已证实会产生错误图像/错误统计/单位/坐标的缺陷，明确物理模型未知的范围，不以“更科学”为理由提出无止境新算法。数学推理和实际原始数据是裁决依据。

## 关于现有代码状态

main 生产入口为 `lib/infrastructure/cli/commands.cpp` → `lib/infrastructure/scheduler/src/module_adapters.cpp` → `lib/algorithms/`。V2 已有 `src/runtime/{pipe,memory,storage}.cpp` 等基础设施，但现时 `src/workflows/frames_provider.cpp::read_leaf_block` 忽略请求范围、`src/workflows/mosaic.cpp` 全域物化、`src/coadd/` 沿新统计模型重写旧传统叠加。因此**管道的存在不等于真实数据已经在内存中流动**。已恢复的自动 NSIDE / order18 ROI 只是有用的局部成果，不是工程改造完成。

**科学与工程必须同时完整迁移**：保留数学推导、生产算法、接口对象、资源/生命周期及真实代码责任。工作流要实际打通内存管道；不得以删科学细节或保留旧文件通信的方式求简单。 旧历史文件从当前文档工作树删除；Git版本本身的历史无法通过删除工作树文件抹去，但不会再成为规范入口。不得修改 main。
