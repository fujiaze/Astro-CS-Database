# CON-002 全局 worker 预算合同 (ExecutionOptions)

> 上游：ASTROCS_DESIGN.md §8（软件架构）

> 唯一执行预算对象。并行/IO/确定性/内存预算以此为唯一来源；嵌套模块
> 只能从该预算借用，不得各自创建等规模线程池。异步队列必须有界，取消/错误
> 传播与关闭顺序必须明确。

> ⚠ **预算唯一来源**：worker 数**不得**由「硬件并发数」默认值决定——**只能**来自 benchmark
> 生成的机器 profile 与全局预算对象（最高设计 §8）；**禁止**任何绕过 profile 的默认值。
> 合法来源 = benchmark 生成的机器 profile（安装目录）+ 全局预算对象
> （可用 CPU = 亲和性 ∩ cgroup ∩ Job Object 的交集）。
> **禁止**任何 GPU 路由开关与第二个可执行入口（最高设计 §6.2/§8）；
> 配置 schema 与 CLI **不含** `gpu_route`，也不提供兼容别名。
> `ExecutionOptions` 只承载调用方从预算对象借到的值。

## 定义

`lib/algorithms/coverage/include/astro/phase2/execution_options.h`:

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `cpu_workers` | int | **无默认**（由 benchmark profile / 全局预算对象注入） | CPU worker 数；`0` = auto ⇒ 由 profile 与预算对象决定（**禁止** `hardware_concurrency` 兜底） |
| `io_workers` | int | **无默认**（同上） | IO worker 数；`0` = auto ⇒ 由 profile 与预算对象决定（**禁止** `cpu_workers/2` 兜底） |
| `deterministic` | bool | true | 固定 seed/顺序/归并 => 可复现结果 |
| `memory_budget_bytes` | uint64 | 0 | 内存预算字节；0 => 由 `memory_limit_mb` 决定 |

**worker 数唯一来源**：benchmark 生成的机器 profile（安装目录）+ 全局预算对象
（可用 CPU = 亲和性 ∩ cgroup ∩ Job Object 的交集，最高设计 §8）。
`ExecutionOptions` **不是** worker 数的第二来源，只承载调用方从预算对象借到的值。

## 配置

`stage2.json` 顶层可选 `execution` 块（**不含任何 GPU 路由键**）：

```json
{
  "execution": {
    "cpu_workers": 4,
    "io_workers": 2,
    "deterministic": true,
    "memory_budget_bytes": 0
  }
}
```

约束：`cpu_workers/io_workers` 属于 [0,1024]（`0` = auto ⇒ 由 profile 与预算对象决定）。
违反即 `p2_stage2_parse_config` 返回 false（带错误信息）。schema 见
`contracts/schemas/phase_config_*.schema.json` 的 `config` 段（生产配置 schema）。

## CLI 覆盖

**唯一 CLI 入口 = `astrocs`**（`normalize` / `mosaic` / `export` 三个子命令，最高设计 §6.2）。
`mosaic` 子命令接受同名字段的 CLI 覆盖（**键名以命令行实际认的键为准**）：
`--cpu-workers N` / `--io-workers N` / `--deterministic 0|1`。
**不存在** `--gpu-route`。

## 使用约定

- 模块仅通过 `ExecutionOptions` 读取已分配预算；`effective_cpu_workers(exec)` /
  `effective_io_workers(exec)` 返回生效值。
- 嵌套模块不得新建等规模线程池（如再 `omp_set_num_threads(hc)`）——必须复用该预算。
- 异步队列容量由 `memory_budget_bytes` 推导（见 CON-008 异步 I/O 合同）。
- **禁止**任何 GPU 路由开关与第二个可执行入口（最高设计 §8）。
- **代码侧现状（登记）**：`execution_options.h` 仍含 `gpu_route`（:18/:43）与
  `hardware_concurrency` 默认（:16/:24/:38）；文档与合同侧不承认这些键。

## 测试

`lib/algorithms/coverage/tests/execution_options_test.cpp`（目标 `phase2_execution_options`）：
配置覆盖、缺省由 profile/预算注入、非法值拒绝、effective 计数器。
