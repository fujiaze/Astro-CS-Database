# AstroCS 结构化日志合同（LOG-001）

> 文档 ID：`ARCH-LOG-STRUCTURED-001`（归属 `docs/architecture/observability/`，owner SA-LOG-08）
> 状态：ACTIVE_NORMATIVE（LOG-001 冻结）
> 机器可读事实源：`runtime/logging/log_event_v1.schema.json`（JSON Schema v1）、
> `runtime/logging/log_event.py`（参考实现）、`tools/monitoring/check_log_contract.py`（检查器）。
> 本文档是视图；字段定义与验收以 schema/检查器为权威（16 标准：JSON/JSONL 输出为真相）。

## 1. 目的与边界

AstroCS 需要一个跨 run/任务/节点/模块/线程的统一结构化日志接口：

- 人可读（中文摘要）与机器可读（JSONL）双输出，来自同一事件；
- 多线程并发事件有确定顺序键（sequence）；
- 错误事件携带可定位的结构化载荷（source/symbol/status）；
- 日志行可机器校验（schema 检查）且有大小上限；
- 日志/诊断不得泄露敏感路径（绝对用户路径/凭据）或夹带 raw testdata。

**边界（本任务冻结范围）**：LOG-001 只冻结合同 + schema + 小型验证，**不实现生产 logger、
不接监控**。LOG-002 把生产 Runtime/模块/资源监控接入本合同的 JSONL 输出。

## 2. 事件模型

每个生产事件产生两路输出，内容一致：

| 输出 | 形态 | 消费方 |
|---|---|---|
| 中文可读摘要 | 单行文本（前缀时间/seq/level/event + 归属 + 诊断） | 操作员/控制台/报告 |
| 机器 JSONL | 单行 JSON 对象 + `\n`，符合 `astrocs.log.event.v1` | 工具/监控/回放/运行图 |

摘要与 JSONL 必须同源生成；禁止两个通道各写一套内容。

### 2.1 JSONL 行结构

单行 = 一个 JSON 对象（**无嵌套对象数组、无跨行**）。顶层字段全部扁平，
`error` 是唯一允许的嵌套对象（仅 error 事件）。

### 2.2 必需字段

| 字段 | 类型 | 语义 | 约束 |
|---|---|---|---|
| `schema` | string | 合同标识 | `const: astrocs.log.event.v1` |
| `seq` | int ≥1 | 进程内单调全局事件序号 | 严格递增、首事件=1、无空洞 |
| `ts` | string | UTC ISO8601 | `YYYY-MM-DDTHH:MM:SSZ`（秒精度，固定 Z） |
| `run` | string | run 标识 | 安全字符 `[A-Za-z0-9._-]{1,128}` |
| `task` | string | 高层任务标识 | 同上；无则 `""` |
| `node` | string | DAG 节点 id | 同上；无则 `""` |
| `module` | string | 唯一 module id | 同上；无则 `""` |
| `phase` | string | 阶段归属 | `phase1/phase2/phase3/runtime/monitoring/cli/""` |
| `commit` | string | 产生事件的 commit | 40 位小写 hex；真实运行现场值 |
| `host` | string | 主机逻辑标识 | 安全字符；不得含用户/凭据 |
| `level` | string | 级别 | `debug/info/warn/error` |
| `event` | string | 事件种类 | `start/progress/end/warn/error/metric/checkpoint/cancel/trace` |
| `units` | string | 数值量纲 | `[A-Za-z0-9/%._-]{0,32}`；无则 `""` |
| `elapsed` | number ≥0 | 自 run 开始经过时间 | 单位 = `units`；无则 0 |
| `diagnostic` | string | 中文诊断/摘要 | ≤1024 字符；可空串但字段必在 |

可选字段：`progress`（[0,1]，progress 事件）、`value`（数值载荷，配 units）、
`error`（仅 error 事件）。

### 2.3 error 载荷（错误事件必填）

| 子字段 | 语义 | 约束示例 |
|---|---|---|
| `source` | 错误来源（模块 id 或仓库内相对路径） | 禁止绝对用户路径（脱敏）；`modules/phase1/noise/impl.cpp` |
| `symbol` | 出错符号 | `astrocs::noise::estimate_sigma` |
| `status` | 稳定错误码 | `IO_READ_FAILED`、`CANCELLED`、`RESOURCE_BUDGET` |

`level=error` 而缺 `error` 对象 = schema 违例；非 error 事件携带 error = 违例。

## 3. 多线程事件顺序（sequence）

- `seq` 由**唯一进程内分配器**在事件提交临界区分配（参考实现 `SeqAllocator`）；
- 进入临界区顺序 = seq 顺序 = 事件接受顺序；**seq 严格递增、无空洞**；
- 消费方（监控/运行图/审计）以 `seq` 为顺序键重放，不依赖墙钟或线程交错猜测；
- 墙钟 `ts` 仅用于展示与跨进程比对，不作同进程顺序判定的依据；
- 生产实现必须保证 emit 在临界区内完成或 seq 与写序一致（LOG-002 强制）。

验收样例：两个线程并发各写 N 条 → 合并后 `seq == 1..2N` 且无重复/空洞。

## 4. 中文摘要

摘要模板（参考实现 `LogEvent.summary()`）：

```
[<ts>] seq=<seq> <level> <event> (<phase>/<module>/<node>)：<diagnostic>
错误事件追加：<status> @ <symbol> (<source>)；计时事件追加：（elapsed=<elapsed><units>）
```

## 5. 敏感路径脱敏（强制）

写入 `diagnostic`/`message` 等自由文本前必须执行脱敏（参考实现 `redact()`）：

| 模式 | 替换 |
|---|---|
| 绝对路径 `/home/<user>/...`、`/Users/<user>/...`、`/tmp/...` | `<redacted>` |
| Windows 盘符路径 `C:\...`、UNC `\\...` | `<redacted>` |
| `scheme://...` URL（含用户信息） | `<redacted>` |
| `password=/token=/secret=/api_key=` 及 `Authorization:` 值 | `<redacted>` |

结构化字段（`run/node/module/host/...`）本身只接受安全字符 schema，
从源头杜绝路径/凭据注入。仓库内相对路径（`source`）合法，不脱敏。

## 6. 大小上限与 schema 检查

- 单行（含 `\n`）上限 **4096 字节**（`MAX_LINE_BYTES`）；超限按 UTF-8 边界截断 + 省略号，不切坏多字节字符；
- 日志文件总量上限与轮转策略由 LOG-002/运行配置定义（本任务冻结单行上限）；
- 机器检查器 `tools/monitoring/check_log_contract.py` 提供：schema 校验（缺字段被拒）、
  seq 单调性、error 载荷、级别/事件枚举、脱敏样例、单行大小；输出机器 JSON 判定。

## 7. 与既有 Core 日志的关系

`include/astrocs/core/logging.h`（CORE-008 Logger/MetricsAggregator）是既有运行时组件
（owner SA-RT-05 路径）；LOG-001 **不修改它**。本合同是其事件语义的冻结外部化：
既有字段 `ts/component/event/message/seq/node_id/run_id/progress/wall_us` 的
等价语义映射到合同字段表 2.2（`component→module/phase` 归属、`message→diagnostic` 等）。
LOG-002 在 Runtime 集成时以本合同为单一事实源做适配，双写/映射细节由 LOG-002 冻结。

## 8. 验收（LOG-001）

| # | 验收点 | 证据 |
|---|---|---|
| A1 | 日志字段全：run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic | schema required + 测试 |
| A2 | 中文可读摘要 + 机器 JSONL 双输出 | 参考实现 + 测试（summary/to_jsonl 同源） |
| A3 | 敏感路径脱敏 | 脱敏规则测试样例 |
| A4 | 多线程事件顺序有 sequence | 并发 seq 测试（1..2N 无空洞） |
| A5 | 错误包含 source/symbol/status | error 载荷 schema + 负测 |
| A6 | 日志 schema 检查和大小上限 | 检查器 PASS/FAIL 样例 + 截断测试 |
| A7 | 无 raw testdata | 本任务不产生/不引用 raw testdata；全局 forbidden globs 不含本路径数据 |

## 9. 参考

- 任务规格：`tasks/03_RUNTIME_DATA_IO_TASKS.md` LOG-001
- 控制包标准：`14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md` §4/§5、`13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md` §5
- 机器事实源：`runtime/logging/log_event_v1.schema.json`、`tools/monitoring/check_log_contract.py`
