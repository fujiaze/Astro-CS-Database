# 插件文档：observability（可观测性）

## 1. 职责与边界

- **职责**：结构化日志、事件流（JSONL）、运行图、资源监控与诊断，贯穿 CLI 到科学模块。
- **不是**：不改变科学结果；不做 I/O 提交（aio）；不因观测开销影响性能预算。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §6.3（JSONL 事件）、§8（资源记录）、§9（run 产物）
- `contracts/schemas/events.schema.json`

## 3. 输入/输出数据合同

- **输出**：JSONL 事件流（schema_version/event_id/run_id/kind：progress/resource/artifact/backend/final）、run-trace.jsonl、resource-timeseries.csv、resource-summary.json、日志。
- 参考：`contracts/schemas/events.schema.json`、`run_*.schema.json`。

## 4. 算法与公式要点

- 事件类型统一 schema，跨模块一致；
- stdout 无日志污染（CLI 合同）：日志走文件/JSONL；
- 运行图（run-graph）是实际观测（trace），`plan` 是预期，**禁止把计划值伪装成实际值**；
- 资源监控字段见 runtime 文档；与退出码联动（exit 10 资源门禁）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `log_level` | `info` | —— | trace/debug/info/warn/error |
| `event_dir` | `run/<run_id>/` | —— | 事件输出目录 |
| `sampling` | —— | —— | 资源采样间隔 |

## 6. 接口/ABI

- 公共日志/事件 API（C ABI 版本化），所有模块调用。

## 7. 错误与边界

- 事件写失败 → 降级为文件日志并标记，不静默吞；
- 凭据/密钥不得出现在日志/事件（脱敏）。

## 8. 测试与 Oracle

- 事件 schema 校验；
- 事件与 run 产物一致性（trace 反映实际）；
- 取消/失败路径事件完整；
- 脱敏测试（无凭据泄漏）。
