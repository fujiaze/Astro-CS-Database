# 目标架构

## 1. 最小调用链

```text
Future integration / synthetic target kernel
        ↓
KernelInvocation + PartitionContract
        ↓
OperationProfile（只读）
        ↓
MixedRoutePlanner
        ↓
RAM/VRAM可行性检查
        ↓
Shared Pending Work Pool
   ├─ CPU executor：领取CPU块
   └─ GPU executor：领取GPU批次
        ↓
completion、coverage、驻留状态和结果合并
```

无合格Profile时安全回退到CPU多线程；不得伪造GPU路由。

## 2. 必需组件

### KernelRegistry / KernelInvocation

同一`OperationId`注册CPU实现和GPU launcher。普通host lambda只能作为CPU实现，不能被当作CUDA kernel。

### PartitionContract

算法明确如何安全拆分。当前只需要两类：

- `IndependentOutputTiles`：每个块拥有独立输出区域，优先用于积分；
- `PrivatePartialThenMerge`：每个设备或块写私有部分结果，最终注册合并器，供Drizzle类累计使用。

禁止多个设备无协议地并发写同一输出。

### OperationProfile

每个目标Operation保存简单的CPU/GPU实测参数，不建立庞大通用画像。

### MixedRoutePlanner

根据剩余工作、CPU/GPU队列、数据驻留、传输成本和内存预算确定：

- 当前哪些设备有边际收益；
- CPU与GPU各自的下一块大小；
- 是否停止某个慢设备继续claim；
- 尾段何时缩块。

### ResidencyManager

跟踪Host/Device/Both及脏状态，使输入只在必要时上传，输出只在需要时下载。

## 3. 不要求重写的现有能力

若现有代码中的TaskDescriptor、HardwareProfile、CostEstimator等结构已稳定，可以保留并作为内部实现；但当前生产路径和验收只要求本包定义的最小字段。不得继续为未接入的通用任务扩充模型。

## 4. Dormant要求

未调用ACR时：

- 不创建线程；
- 不枚举GPU；
- 不加载Profile；
- 不输出未标定警告；
- CPU-only构建不强依赖CUDA。
