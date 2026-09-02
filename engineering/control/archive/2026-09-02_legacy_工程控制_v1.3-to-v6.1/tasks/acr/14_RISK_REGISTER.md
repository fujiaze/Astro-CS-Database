# 风险登记

## R1 通用画像无法描述所有算法
缓解：TaskClass/TaskTraits提供访存、halo、稀疏、原子和分支事实；未来热点必要时增加新的通用能力族，而不是手工设备比例。

## R2 单一总分误导路由
缓解：保存分精度、尺寸、驻留和模式的曲线；禁止用GEMM/AXPY总分代表全部任务。

## R3 模型预测误差导致设备闲置
缓解：工作保持共享池和guided尾部收缩；设备完成即继续领取，但不在线改写画像。

## R4 数据迁移抵消GPU收益
缓解：单独测H2D/D2H、pinned和驻留；成本模型必须计入传输和启动；小任务可留CPU。

## R5 ISA最高不等于最快
缓解：各ISA真实Benchmark；cpu_features只门禁；持续负载记录AVX降频。

## R6 第三方工具链差异
缓解：公共API隔离、backend feature gate、CPU-only、依赖锁和PoC；不同时引入多个重叠runtime。

## R7 根CMake/ABI冲突
缓解：独立target、最小diff、复用仓库已有依赖、合并前完整链接和安装测试。

## R8 普通运行副作用
缓解：lazy initialization；ACR未调用不探测设备、不创建线程、不警告。

## R9 95%被实现成任务比例或少线程
缓解：资源控制文档和测试明确分离；所有线程可参与；禁止share配置。

## R10 动态分块重叠或遗漏
缓解：全局work ID、coverage bitmap、原子领取或受控队列、故障回收状态机。

## R11 Benchmark不真实
缓解：分离kernel/transfer/end-to-end；预热、重复、持续负载、原始数据；无真实GPU不宣称通过。

## R12 Evidence混入不同HEAD
缓解：生成前锁定commit；生成后检查所有报告commit一致；任何修复后全部重生成。

## R13 支线越界
缓解：path guard、算法零diff、原子提交和合并门禁。
