# CON-002 全局 worker 预算合同 (ExecutionOptions)

> 唯一执行预算对象。并行/IO/确定性/内存预算以此为唯一来源；嵌套模块
> 只能从该预算借用，不得各自创建等规模线程池。异步队列必须有界，取消/错误
> 传播与关闭顺序必须明确。

> ⚠ **DOC-202 订正（R10 / R11，2026-09-20）**：
> ① **删除 worker 默认值**（原 `cpu_workers = max(1, hardware_concurrency)`、
>    `io_workers = max(1, cpu/2)`）——依据最高设计 §8「worker 数**不得**由「硬件并发数」默认值
>    决定：**只能**来自 benchmark 生成的机器 profile 与全局预算对象；**禁止**任何绕过 profile 的默认值」；
> ② **删除 GPU 路由键** `gpu_route`（依据最高设计 §8「**禁止**任何 GPU 路由开关」）与
>    **第二个可执行入口** `astrocs-stage2`（依据最高设计 §6.2 唯一命令树 + §8「**禁止**…第二个可执行入口」）。
> 被删面**不保留第二实现**；原文留痕见本文件末「作废留痕」。

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
`contracts/schemas/phase_config_*.schema.json` 的 `config` 段（生产配置 schema；旧
`工程控制/schemas/stage2.schema.json` 已随旧控制包删除）。

## CLI 覆盖

**唯一 CLI 入口 = `astrocs`**（`normalize` / `mosaic` / `export` 三个子命令，最高设计 §6.2）。
`astrocs-stage2` **不是**入口，已作废（最高设计 §7.1/§11「旧可执行程序不是入口」）。
`mosaic` 子命令接受同名字段的 CLI 覆盖（**键名以命令行实际认的键为准**）：
`--cpu-workers N` / `--io-workers N` / `--deterministic 0|1`。
**不存在** `--gpu-route`。

## 使用约定

- 模块仅通过 `ExecutionOptions` 读取已分配预算；`effective_cpu_workers(exec)` /
  `effective_io_workers(exec)` 返回生效值。
- 嵌套模块不得新建等规模线程池（如再 `omp_set_num_threads(hc)`）——必须复用该预算。
- 异步队列容量由 `memory_budget_bytes` 推导（见 CON-008 异步 I/O 合同）。
- **禁止**任何 GPU 路由开关与第二个可执行入口（最高设计 §8）。

## 测试

`lib/algorithms/coverage/tests/execution_options_test.cpp`（目标 `phase2_execution_options`）：
配置覆盖、缺省由 profile/预算注入、非法值拒绝、effective 计数器。
**原「默认 = hardware_concurrency」断言已按 R10 删除**——该断言本身即违规（最高设计 §8）。

## 作废留痕（仅追溯，不得再被引用）

- **R10 原文**：`cpu_workers` 默认 `0 (=auto)`，`0 => max(1, hardware_concurrency)`；
  `io_workers` 默认 `0 (=auto)`，`0 => cpu_workers/2`（至少 1）；
  「默认值：`cpu_workers = max(1, hardware_concurrency)`；`io_workers = max(1, cpu/2)`」。
  作废依据：最高设计 §8（worker 数只能来自 benchmark profile 与全局预算对象）。
- **R11 原文**：`gpu_route` | string | `"auto"` | `"cpu"|"auto"|"cuda"`；
  约束「`gpu_route` 属于 {cpu,auto,cuda}」；CLI
  `astrocs-stage2 <stage2.json> [--cpu-workers N] [--io-workers N] [--gpu-route cpu|auto|cuda] [--deterministic 0|1]`。
  作废依据：最高设计 §8（禁止任何 GPU 路由开关与第二个可执行入口）+ §6.2（唯一命令树）。
- **代码侧待改（不在本任务文件域，登记为交接）**：`execution_options.h` 仍含
  `gpu_route`（:18/:43）与 `hardware_concurrency` 默认（:16/:24/:38）——归
  FIX-201/FIX-208 系列代码任务；**文档侧已按最高设计删键删入口**。
