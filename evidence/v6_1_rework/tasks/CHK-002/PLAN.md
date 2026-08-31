# CHK-002 PLAN — 建立非空静态图运行图检查器

## 需求 (04_TASK_SPECIFICATIONS.md CHK-002)
命令显式接收 `--ir`、`--module-index`、`--trace`。任一文件缺失或 trace 零节点直接 FAIL。
逐项比较 node ID、module ID/version、edge、port、DATA schema、unit、coordinate、artifact ID/hash、
producer/consumer、resource class、provider、workers、status、start/end。输出完整 diff。
negative fixtures 覆盖少节点、换 artifact、换单位、隐藏节点、空 trace。

## 现状证据
- 旧 tools/check_pipeline_trace.py（F-027）空 trace 也 PASS、人工补 P3 output 节点——正是要废弃的语义。
- 控制包 schemas/pipeline_ir.schema.json 定义 v2 IR（nodes/inputs/outputs/resources）。

## 影响文件
- tools/quality/check_pipeline_graph.py（新）
- evidence/v6_1_rework/tasks/CHK-002/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（检查器）。

## 风险
- IR/trace 格式在 RT-004/RT-009 才冻结；本检查器按控制包 schema 与通用 trace 结构编写，字段缺失时给出 diff 而非崩溃。

## 验收命令
1. `python3 tools/quality/check_pipeline_graph.py --selftest` → SELFTEST_PASS all fixtures（ok/fewer_nodes/swapped_artifact/changed_resource_class/hidden_node/empty_trace/module_version_mismatch）
2. 缺文件 → PIPELINE_GRAPH_FAIL exit 1
3. 空 trace → FAIL
