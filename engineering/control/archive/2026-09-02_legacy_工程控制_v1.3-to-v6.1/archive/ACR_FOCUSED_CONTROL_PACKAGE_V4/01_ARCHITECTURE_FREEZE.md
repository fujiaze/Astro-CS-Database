# ACR架构冻结

## 1. 产品边界

ACR只负责将一个**已显式声明可拆分**的重负载Operation，在CPU和GPU之间进行动态混合分块、数据驻留和内存预算管理。

ACR不负责：

- 自动分析任意串行C++代码依赖；
- 自动把host lambda编译成CUDA kernel；
- 接管所有OpenMP任务；
- 解析、元数据、文件扫描、低负载稀疏查询；
- 在线学习或精确控制设备占用率。

## 2. 冻结调用链

```text
Business Adapter / Standalone Example
        ↓
KernelInvocation + PartitionContract + BufferBinding
        ↓
OperationProfile（只读离线结果）
        ↓
MixedRoutePlanner
        ↓
RAM / staging / per-GPU VRAM feasibility
        ↓
Shared Pending Work Pool
   ├─ CPU Executor：多worker、小块
   └─ CUDA Executor：每GPU一个executor，内部1～3个stream、大块/多in-flight token
        ↓
completion + ownership + transfer + memory report
        ↓
必要的host物化或结果合并
```

## 3. 冻结公共语义

公共命名可沿用当前源码，但必须能表达：

```cpp
enum class RouteMode { AutoMixed, CpuOnly, GpuOnly };
enum class PartitionKind { IndependentOutputTiles, PrivatePartialThenMerge };
enum class ResidencyPolicy { HostOnly, PreferDevice, KeepDevice, MaterializeHost };
```

业务调用只提交一次Operation，不指定CPU/GPU比例、不管理CUDA stream、不直接分配设备份额。

测试内部可存在`ForcedMixed`开关，但不得成为业务公共模式，也不得用于生产性能结论。

## 4. 两种分块契约

### IndependentOutputTiles

每个token独占一个输出范围，适用于加权积分和多数逐像素输出算法。

- CPU和GPU可读取同一逻辑输入；
- 双方写入互不重叠输出范围；
- 不需要原子操作和partial merge；
- GPU拥有的范围最终物化到host，CPU范围无需复制。

### PrivatePartialThenMerge

每个token或设备写私有partial，适用于Drizzle scatter/accumulate等存在输出冲突的算法。

- ACR管理或精确公开partial槽位；
- 每次attempt前清零；
- retry不得重复累计；
- merge显式执行并计时。

## 5. GPU通道模型

每张GPU只注册一个`CudaExecutor`。其内部维护有限的in-flight槽位：

```text
slot 0 = stream + event + staging/device views
slot 1 = stream + event + staging/device views
slot 2 = 可选
```

冻结规则：

- 默认候选通道数为1和2；full测试可加入3；
- 根据加权积分实测选择该Operation的推荐通道数；
- 如果单kernel已经吃满GPU，允许最终选择1；
- stream共享同一GPU队列、显存预算和成本模型；
- 禁止把多个stream报告为多个设备；
- `CudaExecutor`必须暴露可用in-flight槽位，而不是同步提交后立即阻塞全局worker。

## 6. CPU模型

- ACR CPU executor使用现有线程池/oneTBB worker；
- CPU launcher只处理一个`[begin,end)`范围；
- launcher内部不得再次启动完整OpenMP并行区；
- 旧OpenMP路径保留为业务回退和性能基线；
- OpenMP和ACR CPU应复用同一逐像素核心函数，避免算法语义分叉。

## 7. 架构冻结条件

完成本包后，下列内容冻结，业务接入不得重新设计：

- KernelRegistry / KernelInvocation；
- 两种PartitionContract；
- SharedWorkPool与WorkToken/attempt；
- CPU多worker + 每GPU单executor/内部多stream；
- OperationProfile只读离线标定；
- makespan动态claim；
- ResidencyManager与Buffer generation；
- RAM/staging/VRAM容量预算；
- ExecutionReport与Evidence字段。
