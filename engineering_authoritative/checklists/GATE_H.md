# Gate H Checklist

- [x] 动态预测 (H-001: FrameCostEstimator, 7阶段成本模型, B-002校准误差<3.4%)
- [x] 内存预约 (H-002: MemoryBudgetManager, 准入公式 reserved+peak+unc+os_margin+worst_next<=budget)
- [x] CPU回滞 (H-002: CPUBackpressure, 90%停止投喂/70%线性回滞)
- [x] 高峰错峰 (H-003: PeakShifter, 优先级队列+防饥饿)
- [x] spill恢复 (H-003: SpillManager, 原子写入+SHA256校验+manifest持久化)
- [x] 安全余量 (H-002: os_margin, 默认2GB可配置)
- [x] 压力测试无OOM (H-004: 4场景8GB/2GB/1.5GB/1GB全部3/3帧完成, 0 OOM)
