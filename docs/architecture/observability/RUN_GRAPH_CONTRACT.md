# AstroCS 运行图渲染工具合同（LOG-003）

> 文档 ID：`ARCH-LOG-RUNGRAPH-003`（归属 `docs/architecture/observability/`，owner SA-LOG-08）
> 状态：ACTIVE_NORMATIVE（LOG-003 冻结）
> 机器可读事实源：`tools/graph/render_run_graph.py`（渲染工具 + 机器验证）、
> `runtime/pipeline/trace_replay.py`（RT-006 权威 replay 聚合）、
> `runtime/pipeline/typed_dag.py`（RT-001 typed DAG 编译器，只读消费）。
> 本文档是视图；字段定义与验收以工具/标准为权威。

## 1. 目的与边界

AstroCS 需要一个**运行图渲染工具**：从 plan/trace 生成 DOT/SVG/JSON 运行图，
标出真实入口、数据边、并行轴、workers、provider、耗时、资源、DLL hash、
artifact hash。旧手绘图/静态架构示意图不再作为规范来源——每次生成都从当前
提交可复现，且机器可验证与 trace 一致。

**验收（tasks/03_RUNTIME_DATA_IO_TASKS.md LOG-003 + 标准 14 §5 + 23 政策）**：
- 图与 trace 调用计数一致：图节点 `call_count` == replay `call_count` ==
  原始 `module_call` 事件计数（`--verify` exit 0 = GRAPH_CONSISTENT）；
- 图与 trace 的 DLL hash、artifact hash 一致（从 trace 事件真实字段取；
  观测缺失留空，**禁止 config 值冒充**）；
- 每次生成写 generator 版本、source SHA、输入文件 hash；DOT/JSON 是可审计
  事实，SVG 是派生展示物（policy §2.5）；
- Doxygen/Graphviz 仅生成文档，不改变产品执行 → `scientific_change` 恒 false。

**边界**：本任务不改科学公式/运行调度；只读消费 `runtime/pipeline` 产物
（不 import 修改）；不实现真实 Graphviz 布局（主机无已登记 dot 时不假装
调用成功——DOT 文本 + 最小合法 SVG 直出，见 §4）。

## 2. 两类图分开（policy §1）

| 图 | 来源 | 表示 | 冒充禁止 |
|---|---|---|---|
| 静态/声明图 | typed plan（`astrocs.typed-dag/v1` / `astrocs.plan-graph/v1`） | resource_class、数据边、operation | 不表示实际调用/耗时/hash |
| 真实运行图 | RT-006 trace JSONL（`astrocs.trace-event/v1`） | 真实入口/调用计数/workers/provider/耗时/DLL/artifact hash | 静态计划不得冒充运行事实 |

`tools/graph/render_run_graph.py render --trace <jsonl> [--plan <plan.json>]`
把二者合成一张运行图：节点=真实执行入口（trace 观测），计划声明属性
（`resource_class`/`operation`）标注 `source=plan`，计划声明但未运行的节点
标 `PLAN_ONLY`（不冒充观测）。

## 3. 固定生成链与可审计头

生成链（policy §2）：trace JSONL +（可选）plan → `graph-runtime.json` +
`graph-runtime.dot`（+ `--svg` 时 `graph-runtime.svg`）。每张图含：

- `generator.tool/version`（`tools/graph/render_run_graph.py` v1.0.0）；
- `source.main_sha`（当前提交 SHA，`--sha` 显式传入，禁止默认猜测）；
- `source.inputs.*.sha256`（trace/plan 输入文件 hash）；
- `metrics`（node_count / module_call_total / scheduler_concurrency_max /
  worker_lease_max / worker_task_total——真实观测）。

DOT 头注释同步上述字段；SVG `<desc>` 同步 metrics + main_sha。**SVG 是派生
展示物，DOT/JSON 才是可审计事实**（审计以 JSON/DOT 为准）。

## 4. 零第三方依赖（Graphviz 不可用时）

本机/控制节点**无 graphviz/dot 二进制、python graphviz 包未装**。DOT 是纯
文本规范形态；本工具用纯 Python 标准库生成 DOT + JSON（审计事实），`--svg`
时直出**最小合法 SVG**（拓扑分层布局，`xml.etree` 可解析）。

**禁止假装调用 dot 成功**：不调用任何外部二进制（代码中无 subprocess）；
无 dot 不报错也不把缺 SVG 当 PASS 阻碍——DOT/JSON 已生成即满足工具职责
（policy §4：工具不可用不能把缺图标记 PASS；只要 DOT/JSON 已生成，其他不
依赖 SVG 的任务继续）。未来若控制节点登记固定版本 Graphviz，可把 DOT 交给
该 dot 渲染 SVG，替换本工具的直出展示物（审计事实不变）。

## 5. 运行图 JSON 合同（astrocs.graph-json/v1）

```
{
  "schema": "astrocs.graph-json/v1",
  "graph_kind": "runtime",
  "generator": {"tool": ".../render_run_graph.py", "version": "1.0.0"},
  "source": {"main_sha": "<40hex>", "inputs": {"trace": {"path","sha256"},
              "plan": {"sha256"}}},
  "run_ids": ["..."],
  "parsed_lines": N, "skipped_lines": M,
  "plan": {"schema","pipeline_id","phase"} | null,
  "plan_error": str|null,
  "outputs": {name: "artifact:..."},
  "metrics": {"node_count","module_call_total","artifact_publish_total",
              "scheduler_concurrency_max","worker_lease_max",
              "worker_task_total"},
  "nodes": [{
    "id", "kind":"node", "module_id", "module_version", "entry",
    "operation", "call_count", "status", "provider",
    "workers", "granted_workers", "wall_ms", "cpu_ms",
    "dll_name", "dll_sha256", "artifacts":[{id,sha256,size}],
    "artifact_publish_count", "resource_class", "resource_class_source",
    "parallel_declared", "first_seen_ts", "last_seen_ts",
    "events": {module_call,node_start,node_end,artifact_publish,
               checkpoint,error,worker_task}
  }],
  "edges": [{"from","to","artifact","artifact_sha256",
             "edge_source":"plan"|"trace",
             "producer_status","consumer_status","data_schema_id","unit"}]
}
```

- `call_count`/`entry`/`module_id`/`status`/`provider`/`workers`/
  `granted_workers`/`wall_ms`/`cpu_ms`/`dll_*`/`artifacts[*]` 全部来自 trace
  事件（`aggregate_trace`，语义与 `trace_replay.py` 对齐），**配置/计划值
  不落这些字段**；
- `resource_class`/`operation`/`parallel_declared` 是计划声明属性，恒带
  `resource_class_source="plan"`；
- 数据边优先取 plan 边（typed-dag IR 经 RT-001 编译器只读推导数据边；
  plan-graph v1 直接取 edges），标注 plan 边引用的 artifact 若被
  `artifact_publish` 观测到则填真实 hash，未发布留空；无 plan 时退化为一组
  trace producer 边（`edge_source="trace"`，consumer 留空）；
- 无 `node_id` 的事件不产生空节点（与 replay 的 "" 聚合节点剔除一致）。

## 6. 机器验证（验收：图与 trace 计数/hash 一致）

`tools/graph/render_run_graph.py verify --verify <graph.json> --trace <jsonl>`：

1. 图节点集合 == trace replay 节点集合（剔除 replay 空 node_id 聚合）；
2. 每节点 `call_count` == replay `call_count` == 原始 `module_call` 计数；
   `entry`/`module_id`/`status`/`provider` 与 replay 观测一致；
3. `dll_name`/`dll_sha256` == trace `module_call` 事件携带值（缺失双方同空）；
4. 节点 `artifacts[{id,sha256,size}]` == trace `artifact_publish` 事件值；
5. 边 `artifact_sha256` == 观测值（未发布 → 必须为空）。

一致 → exit 0 `GRAPH_CONSISTENT`；任一不一致 → exit 1
`GRAPH_INCONSISTENT` + 不一致清单。**篡改图（改 call_count/hash/status）
必然 FAIL**（负测覆盖）。

## 7. 真实入口与字段语义（与 RT-006 对齐）

TraceEvent（`include/astrocs/core/contracts.h`）JSONL 字段：type/run_id/
node_id/module_id/module_version/dll_name/dll_sha256/build_id/entry/
call_count/workers/granted_workers/provider/kernel_id/status/error/
artifact_id/artifact_sha256/artifact_size/cpu_ms/wall_ms/seq。聚合语义与
`runtime/pipeline/trace_replay.py` 对齐（同合法类型集、同 call_count 计数、
同 provider 最后观测胜出）；replay 未聚合的 dll/artifact/workers/cpu 观测
由本工具从事件直接收集。**禁止 config 冒充**：worker/provider/duration/
hash 等观测字段一律只来自 trace 事件。

## 8. 旧手绘图不再作为规范来源

本工具交付前，仓库无产自工具的运行图规范来源；`evidence/**` 下历史
`*.dot`（如 `v6_1_rework` PROD_REACHABILITY）是**旧审计期手写/一次性产物**
（归档性质，非当前规范来源）。LOG-003 之后：

- 运行图规范来源 = `tools/graph/render_run_graph.py` 从**当前提交**可复现
  生成的 `graph-runtime.{json,dot}`（含 generator/source/输入 hash 头）；
- 静态架构示意图（`docs/architecture/DATA_FLOW.md` 等 ASCII 流程、ARCH-001
  mermaid、历史 evidence DOT）是**信息性视图**，不作运行事实规范来源；
- 文档维护：变更运行图语义必须改本工具 + 本合同 + 重跑验证，**不得手改
  生成的图作证**（标准 14 §5）。

## 9. 验收（LOG-003）

| # | 验收点 | 证据 |
|---|---|---|
| G1 | plan+trace → DOT/SVG/JSON：真实入口/数据边/并行轴/workers/provider/耗时/资源/DLL hash/artifact hash | `test_run_graph_render.py` CLI render |
| G2 | 图与 trace 调用计数一致（replay 双实现） | `verify` exit 0 GRAPH_CONSISTENT |
| G3 | DLL/artifact hash 与 trace 事件真实字段一致；观测缺失留空不冒充 | dll/artifact 一致性测试 |
| G4 | JSON 中间表示结构 + generator/source/输入 hash | graph-json/v1 schema 断言 |
| G5 | SVG 最小合法；DOT 含全部节点/边；无 dot 二进制仍 PASS | selfcheck + CLI render |
| G6 | 无 plan producer 边来自 trace；PLAN_ONLY 不冒充观测 | trace-only/plan-only 测试 |
| G7 | 负测：篡改 call_count/hash/status → verify FAIL | 篡改负测 |
| G8 | LOG-002 / RT-006 / LOG-001 回归不破坏 | 回归测试 |

## 10. 参考

- 任务规格：`tasks/03_RUNTIME_DATA_IO_TASKS.md` LOG-003
- RT-006：`include/astrocs/core/contracts.h` TraceEvent、
  `runtime/pipeline/trace_replay.py`
- RT-001：`runtime/pipeline/typed_dag.py`、
  `runtime/pipeline/typed_dag.schema.json`
- LOG-002：`docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md`
- 控制包标准：`14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md` §5、
  `23_GRAPH_AND_DOC_TOOL_POLICY.md`
