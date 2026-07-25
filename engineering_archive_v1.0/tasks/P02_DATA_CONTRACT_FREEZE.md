# P02 数据契约冻结

## 目标

冻结 PipelineFrame、HISS、HCSD 和测试数据管理规则。

## 必做

- ADR 决定 PipelineFrame 唯一所有者；
- 机器可读块 schema；
- HISS/HCSD 正式格式 spec；
- 格式版本与兼容矩阵；
- 大小端、packing、offset、校验和；
- 数据损坏测试；
- 数据集 registry、manifest、Dataset Card；
- 格式迁移策略。

## Gate

任何算法任务不得在未冻结的块/格式上继续扩展字段。
