# 底层开发阶段、任务与验收

## Phase 0：Commit F状态冻结与证据修复

- 将 Commit F 能力降级为“资源采样与CPU尾段分块基础”；
- 修复path guard并记录命令、退出码和允许路径；
- 用目标HEAD的干净worktree一次生成Evidence；
- 修复coverage和actual backend报告。

验收：path guard PASS；单一HEAD；推荐设备与实际设备分离；失败块不标DONE。

## Phase 1：数据模型和调用链

- HardwareProfile/TaskDescriptor保留；
- 接通 `API → CostEstimator → Dispatcher → Backend`；
- CostEstimate.per_device真正参与每次claim；
- 无画像明确CPU fallback。

验收：不再只把推荐设备写入报告；集成测试可证明不同画像改变真实执行路径。

## Phase 2：共享工作池与动态guided

- PENDING/CLAIMED/DONE/FAILED状态机；
- CPU/单GPU/多GPU动态claim；
- 基于剩余工作和画像的动态块大小；
- 故障只回收未完成块；
- 实际执行报告。

验收：每块恰好一次；禁止固定70/30切段冒充guided。

## Phase 3：真实资源控制

- CPU/GPU真实采样或明确估算；
- 提交门控、队列深度、batch和worker让步动作；
- RuntimeConfig注入MemoryBudget；
- 所有内存动作实际执行。

验收：50/80/95/100持续负载报告；人工采样值单测不替代端到端验证。

## Phase 4：CPU HardwareProfile完善

- baseline/SSE/AVX/AVX2/AVX-512真实变体；
- FP32/FP64算术、STREAM、reduction、线程/NUMA；
- Google Benchmark或ADR批准等价框架；
- 留出验证。

## Phase 5：真实GPU与画像

- 使用官方支持工具链；
- 至少一个GPU backend构建运行；
- 显存、H2D/D2H、launch、算术、reduction、卷积、atomic、branch；
- CPU-only构建无GPU SDK依赖。

验收：真实设备日志；工具链不可用只能SKIPPED且阻断最终合并。

## Phase 6：真实Mixed与故障

- CPU和GPU同时从共享池领取；
- device-resident与迁移成本；
- GPU预忙/CPU预忙；
- OOM、设备丢失和取消；
- profile只读。

## Phase 7：可靠性

- 经典实验；
- ASan/UBSan实际开启；
- 适用路径TSan；
- 持续运行、泄漏、取消和并发；
- 成熟FFT/BLAS/scan adapter。

## Phase 8：统一交付和main合并

- 同一干净HEAD的Control、Source、Evidence、Merge Report；
- 最新main回归；
- `--no-ff`合并；
- 合并后dormant和普通启动无副作用。

## 共同要求

- 不修改任何现有算法；
- 不推倒有效代码；
- 不用报告字符串代替真实执行；
- 无真实硬件不宣称通过；
- 所有外部命令设超时；
- 失败阻止合并，但仍交付完整失败证据。
