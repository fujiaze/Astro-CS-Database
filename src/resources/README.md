# resources

现有CpuResources提供CPU池和原预算/租约；本轮不改实现。调用子域、高水位归因、可复用容量arena、内部诊断与CPU/GPU执行承接是待实施目标，见API的E01/E04/E05及EXECUTION；后续先修唯一contracts.hpp再接实现/消费者，不能把未声明目标方法当现有接口。

固定基建骨架见[SYSTEM](../../docs/engineering/rebuild/SYSTEM.md)，目标ProcessHost负责registry/生命周期/统一CPU执行，模块不各建线程池；本轮未改服务实现。
