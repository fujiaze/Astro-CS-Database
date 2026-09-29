# Astro Celestial Sphere Database（ACSD） 运行图渲染工具合同

> 上游：docs/ASTROCS_DESIGN.md §7.2（配置、事件与退出码）、§7.3（错误传播与运行日志）、§8.4（顶层结构）、§10（I/O 与原子产品）

## 1. 目的与边界

机器可读事实源：`eng/tools/graph/render_run_graph.py`（渲染工具 + 机器验证，字段定义与
验收的唯一权威）、`lib/infrastructure/pipeline/trace_replay.py`（权威 replay 聚合）、
`lib/infrastructure/pipeline/typed_dag.py`（typed DAG 编译器，只读消费）。

ACSD 需要一个**运行图渲染工具**：从 plan/trace 生成 DOT/SVG/JSON 运行图，
标出真实入口、数据边、并行轴、workers、provider、耗时、资源、DLL hash、
artifact hash。运行图的规范形态是每次生成都从当前提交可复现、且机器可验证与 trace 一致。

**验收（依据=`docs/ASTROCS_DESIGN.md` §7.2/§7.3 + 本文件）**：
- 图与 trace 调用计数一致：图节点 `call_count` == replay `call_count` ==
  原始 `module_call` 事件计数（`--verify` exit 0 = GRAPH_CONSISTENT）；
- 图与 trace 的 DLL hash、artifact hash 一致（从 trace 事件真实字段取；
  观测缺失留空，字段值只来自 trace 事件）；
- 每次生成写 generator 版本、source SHA、输入文件 hash；DOT/JSON 是可审计
  事实，SVG 是派生展示物；
- Doxygen/Graphviz 仅生成文档、不改变产品执行与科学结果。

**边界**：本工具只读消费 `lib/infrastructure/pipeline` 产物（不 import 修改），
不参与运行调度与科学计算；渲染链不依赖外部布局二进制（DOT 文本 + 最小合法 SVG 直出，见 §4）。

## 2. 两类图分开

| 图 | 来源 | 表示 | 取值来源 |
|---|---|---|---|
| 静态/声明图 | typed plan（`astrocs.typed-dag/v1` / `astrocs.plan-graph/v1`） | resource_class、数据边、operation | 不表示实际调用/耗时/hash |
| 真实运行图 | trace JSONL（`astrocs.trace-event/v1`） | 真实入口/调用计数/workers/provider/耗时/DLL/artifact hash | 取值只来自 trace 事件 |

`eng/tools/graph/render_run_graph.py render --trace <jsonl> [--plan <plan.json>]`
把二者合成一张运行图：节点=真实执行入口（trace 观测），计划声明属性
（`resource_class`/`operation`）标注 `source=plan`，计划声明但未运行的节点
标 `PLAN_ONLY`（不冒充观测）。

## 3. 固定生成链与可审计头

生成链：trace JSONL +（可选）plan → `graph-runtime.json` +
`graph-runtime.dot`（+ `--svg` 时 `graph-runtime.svg`）。每张图含：

- `generator.tool/version`（`eng/tools/graph/render_run_graph.py`，版本取值只来自工具自身）；
- `source.main_sha`（当前提交 SHA，`--sha` 显式传入，取值只来自显式传参）；
- `source.inputs.*.sha256`（trace/plan 输入文件 hash）；
- `metrics`（node_count / module_call_total / scheduler_concurrency_max /
  worker_lease_max / worker_task_total——真实观测）。

DOT 头注释同步上述字段；SVG `<desc>` 同步 metrics + main_sha。**SVG 是派生
展示物，DOT/JSON 才是可审计事实**（审计以 JSON/DOT 为准）。

## 4. 零第三方依赖

渲染链**零外部二进制**，只用 Python 标准库生成 DOT/JSON：DOT 是纯文本规范形态，
本工具以标准库生成 DOT + JSON（审计事实），`--svg` 时直出**最小合法 SVG**
（拓扑分层布局，`xml.etree` 可解析）。

**调用面 = 零外部二进制**（代码中无 subprocess）；只要 DOT/JSON 已生成即满足工具职责，
缺 SVG 不阻碍不依赖 SVG 的消费方。SVG 是派生展示物，可替换渲染后端而不变审计事实。

## 5. 运行图 JSON 合同（astrocs.graph-json/v1）

```
{
  "schema": "astrocs.graph-json/v1",
  "graph_kind": "runtime",
  "generator": {"tool": ".../render_run_graph.py", "version": "<version>"},
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
- 数据边优先取 plan 边（typed-dag IR 经 `lib/infrastructure/pipeline/typed_dag.py` 只读推导数据边；
  plan-graph v1 直接取 edges），标注 plan 边引用的 artifact 若被
  `artifact_publish` 观测到则填真实 hash，未发布留空；无 plan 时退化为一组
  trace producer 边（`edge_source="trace"`，consumer 留空）；
- 无 `node_id` 的事件不产生空节点（与 replay 的 "" 聚合节点剔除一致）。

## 6. 机器验证（验收：图与 trace 计数/hash 一致）

`eng/tools/graph/render_run_graph.py verify --verify <graph.json> --trace <jsonl>`：

1. 图节点集合 == trace replay 节点集合（剔除 replay 空 node_id 聚合）；
2. 每节点 `call_count` == replay `call_count` == 原始 `module_call` 计数；
   `entry`/`module_id`/`status`/`provider` 与 replay 观测一致；
3. `dll_name`/`dll_sha256` == trace `module_call` 事件携带值（缺失双方同空）；
4. 节点 `artifacts[{id,sha256,size}]` == trace `artifact_publish` 事件值；
5. 边 `artifact_sha256` == 观测值（未发布 → 必须为空）。

一致 → exit 0 `GRAPH_CONSISTENT`；任一不一致 → exit 1
`GRAPH_INCONSISTENT` + 不一致清单。**篡改图（改 call_count/hash/status）
必然 FAIL**（负测覆盖）。

## 7. 真实入口与字段语义（与 trace 聚合对齐）

TraceEvent（`lib/include/astrocs/core/contracts.h`）JSONL 字段：type/run_id/
node_id/module_id/module_version/dll_name/dll_sha256/build_id/entry/
call_count/workers/granted_workers/provider/kernel_id/status/error/
artifact_id/artifact_sha256/artifact_size/cpu_ms/wall_ms/seq。聚合语义与
`lib/infrastructure/pipeline/trace_replay.py` 对齐（同合法类型集、同 call_count 计数、
同 provider 最后观测胜出）；replay 未聚合的 dll/artifact/workers/cpu 观测
由本工具从事件直接收集。字段值只来自 trace 事件：worker/provider/duration/
hash 等观测字段一律只来自 trace 事件。

## 8. 规范来源

运行图的规范来源 = `eng/tools/graph/render_run_graph.py` 从**当前提交**可复现生成的
`graph-runtime.{json,dot}`（含 generator/source/输入 hash 头）：

- 可审计事实 = 重跑生成的 `graph-runtime.{json,dot}`；`graph-runtime.svg` 是派生展示物；
- 静态架构示意图（`docs/engineering/DATA_FLOW.md` 等 ASCII 流程、`docs/engineering/ARCHITECTURE.md`
  的 mermaid 图）是**信息性视图**，不承载运行事实；
- 运行图语义的变更 = 改本工具 + 改本合同 + 重跑验证；证据面只用重跑生成的图作证。

## 9. 验收

| # | 验收点 | 证据 |
|---|---|---|
| G1 | plan+trace → DOT/SVG/JSON：真实入口/数据边/并行轴/workers/provider/耗时/资源/DLL hash/artifact hash | `test_run_graph_render.py` CLI render |
| G2 | 图与 trace 调用计数一致（replay 双实现） | `verify` exit 0 GRAPH_CONSISTENT |
| G3 | DLL/artifact hash 与 trace 事件真实字段一致；观测缺失留空不冒充 | dll/artifact 一致性测试 |
| G4 | JSON 中间表示结构 + generator/source/输入 hash | graph-json/v1 schema 断言 |
| G5 | SVG 最小且合法；DOT 含全部节点/边；零外部二进制仍产出 DOT/JSON | selfcheck + CLI render |
| G6 | 无 plan producer 边来自 trace；PLAN_ONLY 不冒充观测 | trace-only/plan-only 测试 |
| G7 | 负测：篡改 call_count/hash/status → verify FAIL | 篡改负测 |
| G8 | 资源监控合同、结构化日志合同与 trace 聚合的回归不破坏 | 回归测试 |

## 10. 参考

- 依据：`docs/ASTROCS_DESIGN.md` §7.2（配置、事件与退出码）、§7.3（错误传播与运行日志）
- trace 事件与聚合：`lib/include/astrocs/core/contracts.h` TraceEvent、
  `lib/infrastructure/pipeline/trace_replay.py`
- typed DAG 编译：`lib/infrastructure/pipeline/typed_dag.py`、
  `lib/infrastructure/pipeline/typed_dag.schema.json`
- 资源监控伴随器：`docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md`
- 结构化日志：`docs/engineering/observability/STRUCTURED_LOGGING_CONTRACT.md`
