# 公共API与分块契约

## 1. 最小API语义

```cpp
enum class RouteMode {
    AutoMixed,
    CpuOnly,
    GpuOnly
};

enum class PartitionKind {
    IndependentOutputTiles,
    PrivatePartialThenMerge
};

enum class ResidencyPolicy {
    HostOnly,          // 不建立设备驻留
    PreferDevice,      // 路由有收益时允许上传并复用
    KeepDevice,        // 本次结果继续留在设备
    MaterializeHost    // 本次结束必须得到主存结果
};

struct KernelInvocation {
    OperationId operation;
    WorkDomain domain;
    PartitionKind partition;
    BufferSet buffers;
    NumericPolicy numeric;
    ResidencyPolicy residency = ResidencyPolicy::PreferDevice;
    RouteMode mode = RouteMode::AutoMixed;
};
```

实际命名可沿用现有代码，但语义必须一致。

## 2. 接入边界

未来算法接入时只需提供：

- 可拆分工作域；
- CPU实现；
- GPU launcher；
- 每item/每tile输入、输出、workspace和partial估算；
- buffer访问角色与驻留策略；
- 输出冲突与merge策略；
- 正确性基线。

ACR不能自动把任意串行C++函数变成GPU kernel。简单任务继续OpenMP，不接入ACR。

## 3. 设备buffer契约

`BufferBinding`至少必须能表达：

- stable key与generation；
- host pointer、bytes和访问模式；
- 可选device allocation/view；
- 当前host/device哪一份有效；
- 是否允许跨token/跨Operation保留；
- 是否要求本次结束物化到host。

禁止仅在状态表中标记resident，而launcher仍使用host临时vector逐块上传。

## 4. PrivatePartialThenMerge契约

调用者不得猜测最大token数量。必须二选一：

1. ACR依据工作池槽位数分配、清零和释放partial scratch；或
2. ACR在claim前公开精确`required_partial_slots/bytes`，不足时拒绝执行。

每次attempt开始前清零该token的partial；失败重试不得叠加旧结果。最终merge由ACR或注册的merge callback明确执行并计时。

## 5. 覆盖与完成语义

每个工作块必须具有稳定ID和attempt：

- `PENDING → CLAIMED → DONE/FAILED`；
- 每块恰好完成一次；
- stale token不能提交旧attempt；
- 设备失败只回收未完成块；
- ExecutionReport只统计真实completion。

## 6. 调试强制模式

`CpuOnly`和`GpuOnly`只用于correctness、Benchmark、故障隔离和对照。正常路径使用`AutoMixed`，但允许根据收益自然退化为单设备。
