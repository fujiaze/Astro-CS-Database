# 当前ACR分支纠正任务

本文件针对现有 `feature/astrocompute-runtime`，必须增量修正。

## P0：Commit F事实纠正

- 提交说明降级为“资源采样与CPU尾段分块基础”；
- `actual_primary_backend` 改为真实完成统计；
- coverage从backend completion导入；
- 修复path guard；
- Evidence改为单一干净HEAD。

## P0：接通真实执行链

```text
API → TaskDescriptor → ProfileStore → CostEstimator → Shared Pending Pool → Backend
```

- `CostEstimate.per_device`参与每次claim；
- 禁止CostEstimate只影响chunk和report字符串；
- 推荐设备与实际设备分开记录；
- 无画像才明确CPU fallback。

## P1：动态工作池

- PENDING/CLAIMED/DONE/FAILED；
- CPU和每张GPU独立worker；
- 动态guided chunk；
- 设备忙闲、队列和驻留；
- 故障回收未完成块；
- exact-once coverage。

## P1：资源控制

- 真实CPU/GPU采样或明确估算；
- submit gate、queue depth、batch和worker让步；
- 所有CPU线程可参与；
- MemoryBudget配置注入；
- StopNewSubmit/ReleaseCache/LowMemoryPath/FallbackOtherDevice/Fail全接通。

## P2：画像和真实GPU

- CPU ISA、FP32/FP64、STREAM、reduction；
- 支持工具链下真实GPU；
- BabelStream、传输、launch、卷积、atomic和branch；
- 真实CPU+GPU Mixed。

## P3：可靠性和交付

- ASan/UBSan实际开启；
- 完整原始日志；
- 单HEAD Evidence；
- 算法目录path guard；
- main合并前后回归。
