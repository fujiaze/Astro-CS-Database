# 全局 worker 预算合同（ExecutionOptions）

> 上游：ASTROCS_DESIGN.md §8（软件架构）、§9（CPU 后端与资源）

**唯一执行预算对象**。并行 / IO / 确定性 / 内存预算以此为唯一来源；嵌套模块只能从该预算借用，线程池规模同源于该预算。异步队列必须有界，取消 / 错误传播与关闭顺序必须明确。

**预算唯一来源**：worker 数**只**来自 benchmark 生成的机器 profile 与全局预算对象（最高设计 §9）；默认值一律取自 profile。合法来源 = benchmark 生成的机器 profile（安装目录）+ 全局预算对象（可用 CPU = 亲和性 ∩ cgroup ∩ Job Object 的交集）。GPU 路由开关与第二个可执行入口**不在合同面内**（最高设计 §1.4）；配置 schema 与 CLI **不含** `gpu_route`，也不提供兼容别名。`ExecutionOptions` 只承载调用方从预算对象借到的值。

## 定义

`lib/algorithms/coverage/include/astro/phase2/execution_options.h`：

| 字段 | 类型 | 默认 | 说明 |
|---|---|---|---|
| `cpu_workers` | int | **无默认**（由 benchmark profile / 全局预算对象注入） | CPU worker 数；`0` = auto ⇒ 由 profile 与预算对象决定，兜底面排除 `hardware_concurrency` |
| `io_workers` | int | **无默认**（同上） | IO worker 数；`0` = auto ⇒ 由 profile 与预算对象决定，兜底面排除 `cpu_workers/2` |
| `deterministic` | bool | true | 固定 seed / 顺序 / 归并 ⇒ 可复现结果 |
| `memory_budget_bytes` | uint64 | 0 | 内存预算字节；0 ⇒ 由 `memory_limit_mb` 决定 |

**worker 数唯一来源**：benchmark 生成的机器 profile（安装目录）+ 全局预算对象（可用 CPU = 亲和性 ∩ cgroup ∩ Job Object 的交集，最高设计 §9）。`ExecutionOptions` **不是** worker 数的第二来源，只承载调用方从预算对象借到的值。

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

约束：`cpu_workers`/`io_workers` 属于 [0,1024]（`0` = auto ⇒ 由 profile 与预算对象决定）。违反即 `p2_stage2_parse_config` 返回 false（带错误信息）。约束的机器校验在 `lib/algorithms/coverage/src/stage2_common.cpp`；phase_config schema 不承载 `execution` 键（硬件字段禁令，见 `eng/contracts/schemas/phase_config_mosaic.schema.json` 的 description）。

## CLI 面

**唯一产品 CLI 入口 = `acsd`**（`normalize` / `mosaic` / `export` 三个子命令，最高设计 §7.1），其命令面**不接受** worker 数或确定性旗标——预算由机器 profile 与全局预算对象给出，不由命令行覆盖（命令树正本 = `lib/infrastructure/cli/command_tree.h`，接口面 = `docs/engineering/CLI_PROTOCOL_V1.md`）。

`--cpu-workers` / `--io-workers` / `--deterministic` 属**工具面 `astrocs-stage2` 的旗标**（`lib/algorithms/coverage/tools/stage2.cpp`），供分阶段调试与基准使用，不属产品 CLI 面，也不进入发布安装面。合同面**不存在** `--gpu-route`。

## 使用约定

- 模块仅通过 `ExecutionOptions` 读取已分配预算；`effective_cpu_workers(exec)` / `effective_io_workers(exec)` 返回生效值。
- 嵌套模块复用该预算；`omp_set_num_threads(hc)` 属新建等规模线程池，越界即判红。
- 异步队列容量由 `memory_budget_bytes` 推导（见 `ASYNC_IO_CONTRACT.md`）。
- GPU 路由开关与第二个可执行入口**不在合同面内**（最高设计 §1.4）。实现面与该条款的偏差登记于 `docs/KNOWN_LIMITATIONS.md`。

## 测试

`lib/algorithms/coverage/tests/execution_options_test.cpp`（目标 `phase2_execution_options`）：配置覆盖、缺省由 profile / 预算注入、非法值拒绝、effective 计数器。
