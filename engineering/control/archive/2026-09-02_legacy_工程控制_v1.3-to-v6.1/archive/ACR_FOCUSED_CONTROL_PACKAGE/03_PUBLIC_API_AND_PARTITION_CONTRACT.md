# 公共API与分块契约

## 1. 建议最小API

```cpp
enum class RouteMode {
    AutoMixed,   // 正常生产模式
    CpuOnly,     // 调试、对照和回退
    GpuOnly      // 调试、对照和资格测试
};

enum class PartitionKind {
    IndependentOutputTiles,
    PrivatePartialThenMerge
};

struct WorkDomain {
    std::uint64_t item_count;
    std::uint32_t preferred_tile_width;
    std::uint32_t preferred_tile_height;
};

struct KernelInvocation {
    OperationId operation;
    WorkDomain domain;
    PartitionKind partition;
    BufferSet buffers;
    NumericPolicy numeric;
    RouteMode mode = RouteMode::AutoMixed;
};

ExecutionReport submit(const KernelInvocation& invocation);
```

实际命名可以沿用现有代码，但语义必须一致。

## 2. 接入边界

未来算法接入时必须提供：

- 可拆分工作域；
- CPU实现；
- GPU launcher；
- 每item/每tile输入、输出和workspace估算；
- 输出冲突与合并策略；
- 正确性基线。

ACR不能自动把任意串行C++函数分析成GPU kernel。

## 3. 目标OperationId

当前底层合成测试至少使用：

- `synthetic.dense_pixel_accumulate.fp32`
- `synthetic.dense_pixel_accumulate.fp64acc`
- `synthetic.pixel_reduce.fp64acc`
- `synthetic.drizzle_like_scatter.fp64acc`
- `synthetic.resident_chain`

未来接入真实算法后，使用真实OperationId和同一注册机制替换对应合成Profile。

## 4. 覆盖与完成语义

每个工作块必须具有稳定ID和attempt：

- `PENDING → CLAIMED → DONE/FAILED`；
- 每块恰好完成一次；
- stale token不能提交旧attempt；
- 设备失败只回收未完成块；
- ExecutionReport只统计真实completion，不使用预测值伪装actual。

## 5. 调试强制模式

`CpuOnly`和`GpuOnly`只用于：

- correctness对照；
- Benchmark；
- 故障隔离；
- 无Profile或设备不可用时的明确回退。

正常路径使用`AutoMixed`，但允许其根据边际收益自然只启用一种设备。
