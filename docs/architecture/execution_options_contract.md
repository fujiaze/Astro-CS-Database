# CON-002 全局 worker 预算合同 (ExecutionOptions)

> 唯一执行预算对象。并行/IO/GPU 路由/确定性/内存预算以此为唯一来源；嵌套模块
> 只能从该预算借用，不得各自创建等规模线程池。异步队列必须有界，取消/错误
> 传播与关闭顺序必须明确。

## 定义

`lib/phase2/include/astro/phase2/execution_options.h`:

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `cpu_workers` | int | 0 (=auto) | CPU worker 数；0 => `max(1, hardware_concurrency)` |
| `io_workers` | int | 0 (=auto) | IO worker 数；0 => `cpu_workers/2`（至少 1） |
| `gpu_route` | string | `"auto"` | `"cpu"|"auto"|"cuda"` |
| `deterministic` | bool | true | 固定 seed/顺序/归并 => 可复现结果 |
| `memory_budget_bytes` | uint64 | 0 | 内存预算字节；0 => 由 `memory_limit_mb` 决定 |

默认值：`cpu_workers = max(1, hardware_concurrency)`；`io_workers = max(1, cpu/2)`；
`gpu_route = "auto"`；`deterministic = true`；`memory_budget_bytes = 0`。

## 配置

`stage2.json` 顶层可选 `execution` 块：

```json
{
  "execution": {
    "cpu_workers": 4,
    "io_workers": 2,
    "gpu_route": "cpu",
    "deterministic": true,
    "memory_budget_bytes": 0
  }
}
```

约束：`cpu_workers/io_workers` 属于 [0,1024]（0=auto）；`gpu_route` 属于 {cpu,auto,cuda}。
违反即 `p2_stage2_parse_config` 返回 false（带错误信息）。schema 见
`工程控制/schemas/stage2.schema.json`（`properties.execution`）。

## CLI 覆盖

`astrocs-stage2 <stage2.json> [--cpu-workers N] [--io-workers N] [--gpu-route cpu|auto|cuda] [--deterministic 0|1]`
CLI 值覆盖配置块的同类字段。

## 使用约定

- 模块仅通过 `ExecutionOptions` 读取已分配预算；`effective_cpu_workers(exec)` /
  `effective_io_workers(exec)` 返回生效值。
- 嵌套模块不得新建等规模线程池（如再 `omp_set_num_threads(hc)`）——必须复用该预算。
- 异步队列容量由 `memory_budget_bytes` 推导（见 CON-008 异步 I/O 合同）。

## 测试

`lib/phase2/tests/execution_options_test.cpp`（目标 `phase2_execution_options`）：
默认=hardware_concurrency、配置覆盖、缺省默认、非法值拒绝、effective 计数器。