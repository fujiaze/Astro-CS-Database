# 当前任务：P04-002 JSONL 事件与稳定错误码

读取 `tasks/P04-002.md` 并执行。扩展 JSONL 事件类型 (stage_started/stage_completed/warning)，定义 `ASTROCS_*` 错误码与 HTTP 状态的映射，确保字段和错误码可由未来 GUI 稳定消费。

## 上一任务完成情况

- P04-001 CLI request 与 effective config: DONE (VERDICT: PASS)
  - 证据: evidence/P04-001/
  - 关键变更: 新增 cli_request_schema.json + effective_config_schema.json 契约; cli_command.cpp 新增 SHA-256 实现 + JSON 合并工具 + inspect/capabilities/cmd_request 子命令 (+763 行)
  - 234/234 测试通过 (189 C++ 集成测试 + 45 Python schema 验证)
  - 兼容性: 0 退化 (Part 1-5 既有测试全通过, P03-003 退出码未变)
  - 残留建议已转移至 P04-002 (嵌套合并评估) / P04-004 (timeouts 语义) / P05-002 (真实数据端到端验证)

## P04-002 依赖

- P04-001 (DONE)

## 执行步骤

1. 扩展 JSONL 事件类型: stage_started / stage_completed / warning (当前已实现 accepted/completed/failed)
2. 定义 ASTROCS_* 错误码与 HTTP 状态映射 (供 GUI 消费)
3. 评估嵌套对象深度合并需求 (若 GUI 需要, 实现 json_merge 递归合并)
4. 更新 capabilities 输出以反映新事件类型
5. 编写测试验证新事件类型与错误码

完成独立复核后, 更新状态并进入依赖满足的下一任务。
