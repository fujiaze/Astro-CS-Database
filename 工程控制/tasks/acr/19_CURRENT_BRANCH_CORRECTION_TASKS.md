# 当前ACR分支纠正任务

本文件针对已存在的 `feature/astrocompute-runtime`。必须增量修正，禁止推倒重来。

## P0：删除错误路由模型

1. 搜索 `RouteProfile`、`RouteEntryView`、`preferred_backend`、`routes.json`；
2. 删除以业务 kernel 为键的固定后端推荐；
3. 删除任何CPU/GPU share/weight schema；
4. 保留仅用于诊断的 OperationId，不把它当固定路由表主键；
5. 提供旧画像迁移/拒绝加载行为，禁止静默解释旧格式。

## P0：建立新数据模型

- `DeviceProfile/HardwareProfile`；
- `TaskTraits/TaskDescriptor`；
- `ProfileStore`；
- `CostEstimator`；
- profile schema与版本检查；
- 留出误差和置信度。

## P0：接通调用链

此前若 `parallel_for(OperationId /*id*/, ...)` 忽略ID/traits并直达CPU，必须改为：

```text
API → TaskDescriptor → ProfileStore → CostEstimator → Dispatcher → Backend
```

无画像时才进入明确CPU fallback。

## P1：扩展CPU画像

- 真实baseline/SSE/AVX/AVX2/AVX-512；
- FP32/FP64算术；
- STREAM内存；
- reduction；
- 线程/NUMA；
- 持续负载降频；
- Google Benchmark或ADR批准等价框架。

## P1：真实GPU

- 解决现有CUDA/工具链兼容问题或通过ADR选择可维护工具链；
- 至少一个真实GPU backend；
- BabelStream、H2D/D2H、launch、reduction、卷积、atomic、branch；
- CPU-only构建不依赖GPU SDK。

## P2：CostEstimator和动态调度

- 能力族映射；
- queue/launch/transfer/compute/merge成本；
- 候选块和低置信度惩罚；
- 共享未开始工作池；
- CPU/多GPU领取；
- guided尾部收缩；
- coverage与故障回收；
- profile运行时只读。

## P2：真实资源控制

- 真实CPU/GPU利用率或可审计估算；
- 50/80/95/100目标持续负载；
- 所有CPU worker参与；
- RAM/VRAM限制；
- 不能只向控制器输入人工数值。

## P3：测试与Evidence

- Mixed真实CPU+GPU，无GPU SKIPPED；
- ASan/UBSan实际开启；
- 成熟FFT/BLAS/scan adapter；
- 同一干净HEAD一次生成全部Evidence；
- 算法目录path guard；
- main合并前后回归。
