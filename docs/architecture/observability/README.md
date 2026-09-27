# observability

本目录存放可观测性子域的三份合同：结构化日志、运行图与资源监控。

## 职责边界

- 放：日志 schema、运行图渲染、资源监控伴随器的合同正文。
- 不放：日志与错误系统的整体设计（在 docs/design/LOG_AND_ERROR_SYSTEM.md）；日志字段合同（在 docs/contracts/LOG_AND_ERROR_CONTRACT.md）；观测工作细节（在 docs/plugins/infrastructure/21_observability.md）。

## 内容

- `STRUCTURED_LOGGING_CONTRACT.md` —— 结构化日志合同（日志 schema 正本）。
- `RUN_GRAPH_CONTRACT.md` —— 运行图渲染工具合同。
- `RESOURCE_MONITORING_CONTRACT.md` —— 资源监控伴随器合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.1（顶层结构）、§9（CPU 后端与资源）。
