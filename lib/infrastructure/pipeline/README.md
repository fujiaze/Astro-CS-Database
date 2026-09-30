# pipeline

本目录是阶段内命名块内存管线的实现与合同：块的读写、生命周期与 typed DAG 编排在这里落地。

## 职责边界

- 放：编排实现（orchestrator/）、模块加载（module_loader/）、typed DAG 合同与工具、模块端口登记、轨迹回放与样例。
- 不放：三阶段调度器（lib/infrastructure/scheduler/）、科学算法实现（lib/algorithms/）、机器门（eng/ci/）。
- 模块状态不由本目录的登记文件声明，由 eng/tools/quality/check_module_map.py 按最高设计的状态词表现场计算。

## 内容

- orchestrator/ —— 编排器实现与配置（cpp/ 源码与测试、configs/ 阶段 schema 与面板配置）。
- module_loader/ —— 模块注册与安全加载（module_registry.c/.h、secure_loader.c/.h）。
- typed_dag.py 与 typed_dag.schema.json —— typed DAG 校验工具与 schema。
- typed_dag_contract.h —— 阶段内内存块管线合同头。
- module_ports.registry.json —— 模块端口登记表。
- trace_replay.py —— 运行轨迹回放工具。
- fixtures/ —— 管线样例输入（phase2_typed_dag.json）。
- PENDING.md —— 本目录内容与目标位置的迁移登记文件。

## 上游

上游：docs/ACSD_DESIGN.md §8.2（阶段内命名块内存管线与块生命周期）、§8.4（infrastructure/pipeline 条目）。