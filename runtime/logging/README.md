# AstroCS 结构化日志（LOG-001）

本目录冻结 AstroCS 统一结构化日志合同（LOG-001，owner SA-LOG-08）。

## 内容

| 文件 | 说明 |
|---|---|
| `log_event_v1.schema.json` | 机器 JSONL 事件行合同（JSON Schema draft-07）：run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic + seq/ts/error/schema |
| `log_event.py` | 参考实现（合同最小可执行语义）：字段校验、JSONL 序列化、多线程 seq 分配器、中文摘要、敏感路径脱敏、单行大小截断。**不是生产 logger** |
| `../monitoring/` | LOG-002 起的生产资源监控伴生器 |
| `../../docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md` | 权威合同文档（字段表、事件语义、脱敏、验收、LOG-001/LOG-002 边界） |

## 边界（合同冻结范围）

- LOG-001 冻结：字段语义、schema（JSON Schema + 参考实现）、小型验证。产物是**合同 + 检查器**，
  不实现生产 logger 也不接监控。
- LOG-002 冻结：生产 Runtime/模块写入本 schema 的 JSONL；monitor 自动采集重任务资源。
- 模块不得自建文本 logger 冒充结构化输出；所有生产事件最终走统一合同行。
- 不修改科学公式；本目录不包含任何科学/算法源码。

## 快速验证

```bash
python3 -m py_compile runtime/logging/log_event.py tools/monitoring/check_log_contract.py
python3 -m unittest discover -s tests/monitoring -v
python3 tools/monitoring/check_log_contract.py --selfcheck
```
