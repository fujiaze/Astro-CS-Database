# 资源感知编排器规范

## 目标

多帧异步推进，但不同时无条件开始；有资源才准入。CPU可长期接近高利用率，滚动负载超过90%停止投喂；内存安全优先。

## 组件

- ResourceMonitor：CPU、RSS、Commit、I/O、活跃阶段；
- FrameCostEstimator：按图像尺寸、T1–T4、星点、Gaia数量、Nside估算阶段峰值；
- MemoryBudgetManager：预约、释放、安全余量和误差系数；
- AdmissionController：CPU回滞、内存门限、阶段兼容矩阵；
- StageScheduler：允许低峰阶段与高峰阶段错峰并行；
- SpillManager：只spill已序列化、可恢复块。

## 准入

`reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget` 才允许进入下一高峰阶段。

## 压力处理

停止准入 → 等待释放 → 清理可重建缓存 → 暂停 → 显式spill → 恢复 → 最后才允许OS swap。不得丢弃未持久化科学数据。
