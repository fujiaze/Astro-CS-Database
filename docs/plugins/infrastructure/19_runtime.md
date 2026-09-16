# 插件文档：runtime（调度与资源）

## 1. 职责与边界

- **职责**：typed DAG 的执行（注册、依赖、调度）、统一线程预算、资源监控、取消与 checkpoint。
- **不是**：不定义科学公式；不产生科学值；模块注册/加载与 ABI 校验由调度器承担（本模块负责执行与资源）。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §7.2（架构）、§8（CPU 后端与资源）
- `docs/design/*_DETAILED_DESIGN.md`（节点与先后关系）

## 3. 输入/输出数据合同

- **输入**：run-plan（节点图、模块 ID、配置、输出路径）、cpu_profile。
- **输出**：run-graph、run-trace（JSONL）、资源时间序列、artifact-manifest、run-summary。
- 参考：`contracts/schemas/run_*.schema.json`。

## 4. 算法与公式要点

- typed DAG：节点 = 模块/entrypoint/operation；科学依赖不可改变（最高设计 §3/4/5）；
- 一个进程只有一个资源调度器与线程预算源；模块不得硬编码 workers、不得私建长期线程池；
- 分块/并行只改变执行，不改变归约次序或科学结果；
- 取消：协作取消 → checkpoint → 干净退出；
- 资源监控：进程/线程 CPU、RSS/PSS、内存增长、读写字节、I/O wait、work units、队列深度、worker 均衡、进度、墙钟。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `workers` | 由 profile | —— | 线程预算（来自 cpu_profile，不得硬编码） |
| `block` | 由 profile | —— | 分块大小 |
| `checkpoint` | true | —— | 是否支持 checkpoint |
| `memory_limit` | —— | MB | 内存预算（可选） |

## 6. 接口/ABI

- entrypoint：run-plan → 执行 → run 产物；
- 模块通过注册表加载（版本化 C ABI 校验），不隐藏整阶段 Session。

## 7. 错误与边界

- ABI/签名/CPU 特征不匹配 → exit 5；
- 执行失败 → exit 6；
- 资源利用率门禁失败 → exit 10；
- 取消/超时 → exit 9。

## 8. 测试与 Oracle

- DAG 依赖顺序测试（科学依赖不可变）；
- 1 worker vs N worker 数值一致；
- 取消/checkpoint 恢复无半成品；
- 资源监控记录完整性；
- 利用率门禁测试（能红能绿）。
